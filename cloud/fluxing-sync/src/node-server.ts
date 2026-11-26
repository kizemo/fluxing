/**
 * 自建 Node 运行时（路线 B：腾讯云 ECS + PostgreSQL + 本地对象树）。
 *
 * 本文件把 `node:http` 的请求适配成 Workers `Request`，把响应写回 `ServerResponse`，
 * 然后把请求整体交给 `index.ts` 里那个**一个字都没改**的 `fetch` 处理器。
 *
 * 三条边界（越界就等于重写业务代码，明确不做）：
 *   1. `route()` / 13 个 handler / `wire.ts` / `crypto.ts` / `errors.ts` 逐字保留；
 *   2. 本文件与 `node-main.ts` **不进 Workers 打包图** —— `index.ts` 不 import 它们，
 *      wrangler 静态追踪不到；但 `tsc --noEmit` 的 `include` 覆盖 `src/` 下所有 `.ts`，
 *      所以这里必须做到**零 Node 类型依赖**（不写 `import 'node:http'`，不写
 *      `declare const process`，只用自声明的结构类型 + `dynamicImport`）。
 *   3. 启动方式靠 esbuild 打一个单文件 mjs 再 `node` 跑（`scripts/` 未纳入本文件职责）——
 *      直接 `node --experimental-strip-types src/node-main.ts` 走不通，原因是既有业务
 *      代码全是**无扩展名**相对导入（`./env`、`./errors` …），Node ESM 解析器不认；
 *      改那些导入会同时打破 Workers 侧约定，代价大于收益。
 *
 * 刻意不做的事：
 *   · 不接管压缩 / 静态资源 / TLS —— 那些交给前置 nginx；
 *   · 不实现 keep-alive 调优、限流、超时 —— 先让语义对齐，性能与防护另开批次；
 *   · 不打印任何请求头 —— `Authorization` 绝不进日志。
 */

import worker from './index';
import type { Env } from './env';
import { ApiError, toErrorResponse } from './errors';
import { dynamicImport } from './pg/dynamic-import';
import { createPgDatabase, createPgPool } from './pg/d1-shim';
import { createFsBucket } from './pg/r2-fs';

// ---------------------------------------------------------------------------
// 结构类型（不依赖 @types/node，也不与 @cloudflare/workers-types 冲突）
// ---------------------------------------------------------------------------

/** `http.IncomingMessage` 的最小面。 */
export interface IncomingMessageLike {
  method?: string;
  url?: string;
  headers: Record<string, string | string[] | undefined>;
  on(event: 'data', listener: (chunk: Uint8Array) => void): unknown;
  on(event: 'end', listener: () => void): unknown;
  on(event: 'error', listener: (error: Error) => void): unknown;
}

/** `http.ServerResponse` 的最小面。 */
export interface ServerResponseLike {
  statusCode: number;
  setHeader(name: string, value: string): void;
  end(payload?: Uint8Array): void;
}

/** `http.Server` 的最小面。 */
export interface HttpServerLike {
  listen(port: number, host: string, callback: () => void): unknown;
  close(callback?: () => void): unknown;
  on(event: 'error', listener: (error: Error) => void): unknown;
  address?(): { port?: number } | string | null;
  /** 存在但**不调用**：优雅关闭时强杀连接会截断在途同步写。 */
  closeAllConnections?(): void;
}

/** `node:http` 模块。 */
export interface HttpModuleLike {
  createServer(listener: (request: IncomingMessageLike, response: ServerResponseLike) => void): HttpServerLike;
}

/** `globalThis.process` 的最小面（绝不 `declare const process`）。 */
interface ProcessLike {
  env?: Record<string, string | undefined>;
  on?(event: string, listener: () => void): unknown;
  exit?(code?: number): void;
}

// ---------------------------------------------------------------------------
// 常量
// ---------------------------------------------------------------------------

/** 默认监听地址：只绑回环，公网入口一律走 nginx 反代。 */
const DEFAULT_BIND_HOST = '127.0.0.1';
const DEFAULT_BIND_PORT = 8787;
const DEFAULT_PG_POOL_MAX = 10;
/** 64 MiB：远大于任何真实同步载荷（单条词典条目上限是 KB 级）。 */
const DEFAULT_MAX_BODY_BYTES = 64 * 1024 * 1024;

/**
 * 转发给业务层的请求头黑名单。
 *
 * `content-length` 必须剔除：body 由本文件重新构造，长度以实际缓冲字节数为准，
 * 保留了旧值就会让 undici 判「长度不匹配」直接报错。
 */
const DROP_REQUEST_HEADERS: ReadonlySet<string> = new Set([
  'connection',
  'keep-alive',
  'proxy-authenticate',
  'proxy-authorization',
  'te',
  'trailer',
  'transfer-encoding',
  'upgrade',
  'host',
  'content-length',
]);

/** 允许从进程环境透传给业务层的配置键（白名单，不整块转发）。 */
const ENV_PASSTHROUGH_KEYS = [
  'AUTH_PEPPER',
  'ENVIRONMENT',
  'ACCESS_TOKEN_TTL',
  'REFRESH_TOKEN_TTL',
  'KDF_ALGO',
  'KDF_ITERS',
  'MAIL_MODE',
  'MAIL_FROM',
  'MAIL_FROM_NAME',
  'MAIL_HTTP_ENDPOINT',
  'MAIL_HTTP_API_KEY',
  'MAIL_HTTP_AUTH_HEADER',
  'MAIL_HTTP_AUTH_PREFIX',
  'EMAIL',
  'WRITE_BUDGET_ROWS_PER_DAY',
  'WRITE_BUDGET_BUCKETS_PER_DAY',
] as const;

/** Host 头白名单字符：挡住把 `\r\n` 或路径塞进 authority 的构造尝试。 */
const HOST_PATTERN = /^[A-Za-z0-9.\-:[\]]+$/;

// ---------------------------------------------------------------------------
// 配置
// ---------------------------------------------------------------------------

/** 运行时配置。 */
export interface NodeRuntimeConfig {
  /** PG 连接串（`postgres://…`）。 */
  pgUrl: string;
  /** 对象树根目录（绝对路径），对应 R2 桶。 */
  objectRoot: string;
  bindHost: string;
  bindPort: number;
  maxBodyBytes: number;
  pgPoolMax: number;
  /** 原始进程环境，用于组装业务层 `Env`。 */
  envVars: Record<string, string | undefined>;
}

/**
 * 从环境变量读配置。
 *
 * 缺必需项**直接抛**：带着半份配置启动，失败会推迟到第一条同步请求，
 * 那时候的现象（500）看起来像业务 bug，排查成本高一个数量级。
 */
export function readRuntimeConfig(source: Record<string, string | undefined>): NodeRuntimeConfig {
  return {
    pgUrl: requireString(source, 'FLUXING_PG_URL'),
    objectRoot: requireString(source, 'FLUXING_OBJECT_ROOT'),
    bindHost: optionalString(source, 'FLUXING_BIND_HOST') ?? DEFAULT_BIND_HOST,
    bindPort: readInteger(source, 'FLUXING_BIND_PORT', DEFAULT_BIND_PORT, 1, 65_535),
    maxBodyBytes: readInteger(source, 'FLUXING_MAX_BODY_BYTES', DEFAULT_MAX_BODY_BYTES, 1_024, 1_073_741_824),
    pgPoolMax: readInteger(source, 'FLUXING_PG_POOL_MAX', DEFAULT_PG_POOL_MAX, 1, 1_000),
    envVars: source,
  };
}

function requireString(source: Record<string, string | undefined>, key: string): string {
  const value = optionalString(source, key);
  if (value === undefined) {
    throw new Error('缺少必需的环境变量 ' + key + '，拒绝启动。');
  }
  return value;
}

function optionalString(source: Record<string, string | undefined>, key: string): string | undefined {
  const value = source[key];
  if (typeof value !== 'string') return undefined;
  const trimmed = value.trim();
  return trimmed.length === 0 ? undefined : trimmed;
}

function readInteger(
  source: Record<string, string | undefined>,
  key: string,
  fallback: number,
  min: number,
  max: number,
): number {
  const raw = optionalString(source, key);
  if (raw === undefined) return fallback;
  const parsed = Number(raw);
  if (!Number.isInteger(parsed) || parsed < min || parsed > max) {
    throw new Error('环境变量 ' + key + ' 的值 ' + raw + ' 不合法，合法区间是 ' + min + '..' + max + '。');
  }
  return parsed;
}

/**
 * 组装业务层 `Env`。
 *
 * 只做一次 `as unknown as Env`：`Env` 的 `DB` / `BUCKET` 是 Workers 的全局接口类型，
 * Node 侧的两个兼容对象在结构上满足消费点，但不可能在类型层面「变成」`D1Database`。
 * 断言集中在这一行，其余位置一律走真实类型。
 */
export function buildEnv(source: Record<string, string | undefined>, db: unknown, bucket: unknown): Env {
  const picked: Record<string, string> = {};
  for (const key of ENV_PASSTHROUGH_KEYS) {
    const value = optionalString(source, key);
    if (value !== undefined) picked[key] = value;
  }
  if (picked['ENVIRONMENT'] === undefined) picked['ENVIRONMENT'] = 'self-hosted';
  return { ...picked, DB: db, BUCKET: bucket } as unknown as Env;
}

// ---------------------------------------------------------------------------
// 请求适配
// ---------------------------------------------------------------------------

/** 把 `node:http` 请求变成业务层能直接吃的 `Request`。 */
export function toWorkerRequest(request: IncomingMessageLike, body: Uint8Array | null, method: string): Request {
  const init: RequestInit = { method, headers: toHeaders(request.headers) };
  if (body !== null && body.byteLength > 0 && method !== 'GET' && method !== 'HEAD') {
    init.body = body;
  }
  return new Request(buildUrl(request), init);
}

/** 组装绝对 URL。业务层只拿它做路由，不读 origin。 */
function buildUrl(request: IncomingMessageLike): string {
  const raw = request.url ?? '/';
  const path = raw.startsWith('/') ? raw : '/' + raw;
  return 'http://' + readHost(request.headers) + path;
}

function readHost(headers: Record<string, string | string[] | undefined>): string {
  const raw = headers['host'];
  const value = Array.isArray(raw) ? raw[0] : raw;
  if (typeof value !== 'string' || value.length === 0 || !HOST_PATTERN.test(value)) {
    return 'localhost';
  }
  return value;
}

function toHeaders(headers: Record<string, string | string[] | undefined>): Headers {
  const result = new Headers();
  for (const [name, value] of Object.entries(headers)) {
    if (value === undefined) continue;
    const lower = name.toLowerCase();
    if (DROP_REQUEST_HEADERS.has(lower)) continue;
    if (Array.isArray(value)) {
      if (value.length === 0) continue;
      // Cookie 是唯一不能用 ", " 合并的请求头，合并后服务端解析会串味。
      result.set(lower, value.join(lower === 'cookie' ? '; ' : ', '));
      continue;
    }
    result.set(lower, value);
  }
  return result;
}

/**
 * 全量缓冲请求体。
 *
 * 超限后**继续 drain 到 end** 再拒绝：提前 destroy 会让客户端看到一个没有响应体的
 * 连接重置，而不是可读的 413。
 *
 * 返回 `null` 表示超限。
 */
function readBody(request: IncomingMessageLike, limit: number): Promise<Uint8Array | null> {
  return new Promise<Uint8Array | null>((resolve, reject) => {
    const chunks: Uint8Array[] = [];
    let total = 0;
    let overflow = false;
    request.on('data', (chunk: Uint8Array) => {
      if (overflow) return;
      total += chunk.byteLength;
      if (total > limit) {
        overflow = true;
        chunks.length = 0;
        return;
      }
      chunks.push(chunk);
    });
    request.on('end', () => {
      resolve(overflow ? null : concatChunks(chunks, total));
    });
    request.on('error', (error: Error) => {
      reject(error);
    });
  });
}

function concatChunks(chunks: Uint8Array[], total: number): Uint8Array {
  const merged = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    merged.set(chunk, offset);
    offset += chunk.byteLength;
  }
  return merged;
}

// ---------------------------------------------------------------------------
// 请求处理
// ---------------------------------------------------------------------------

/**
 * 造 `http.createServer` 的监听器。
 *
 * 响应路径的两点硬要求：
 *   1. 先 `arrayBuffer()` 全量取完再 `setHeader` —— 业务层返回的都是内存里的小响应；
 *   2. `content-length` 交给 Node 自己按实际字节数算，绝不手工沿用它。
 */
export function createRequestListener(
  config: NodeRuntimeConfig,
  env: Env,
): (request: IncomingMessageLike, response: ServerResponseLike) => void {
  return (request: IncomingMessageLike, response: ServerResponseLike): void => {
    const startedAt = Date.now();
    const method = (request.method ?? 'GET').toUpperCase();
    const routePath = stripQuery(request.url ?? '/');
    void (async () => {
      try {
        const body = await readBody(request, config.maxBodyBytes);
        if (body === null) {
          throw new ApiError(413, 'payload_too_large', '请求体超过 ' + config.maxBodyBytes + ' 字节上限。');
        }
        const apiResponse = await worker.fetch(toWorkerRequest(request, body, method), env);
        await writeResponse(response, apiResponse);
        logAccess(method, routePath, apiResponse.status, Date.now() - startedAt);
      } catch (error) {
        const failure = toErrorResponse(error);
        await writeResponse(response, failure);
        logAccess(method, routePath, failure.status, Date.now() - startedAt);
        if (!(error instanceof ApiError)) {
          console.error('[fluxing-sync] 请求处理失败：' + describeError(error));
        }
      }
    })();
  };
}

async function writeResponse(response: ServerResponseLike, apiResponse: Response): Promise<void> {
  const payload = new Uint8Array(await apiResponse.arrayBuffer());
  apiResponse.headers.forEach((value: string, name: string) => {
    response.setHeader(name, value);
  });
  response.statusCode = apiResponse.status;
  response.end(payload);
}

function stripQuery(url: string): string {
  const cut = url.search(/[?#]/);
  return cut < 0 ? url : url.slice(0, cut);
}

function describeError(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

/** 只打方法 / 路径 / 状态 / 耗时 —— 绝不打印任何请求头。 */
function logAccess(method: string, routePath: string, status: number, elapsedMs: number): void {
  console.log('[fluxing-sync] ' + method + ' ' + routePath + ' ' + status + ' ' + elapsedMs + 'ms');
}

// ---------------------------------------------------------------------------
// 启动 / 关闭
// ---------------------------------------------------------------------------

/** 已启动的服务。 */
export interface StartedServer {
  /** 监听地址（含实际端口，`:0` 时是分配到的端口）。 */
  readonly origin: string;
  readonly port: number;
  /** 幂等关闭：停监听 → 关连接池。 */
  close(): Promise<void>;
}

/** 起服务。建池失败 / 端口占用都在这里同步暴露，不推迟到首个请求。 */
export async function startServer(config: NodeRuntimeConfig): Promise<StartedServer> {
  const { pg, pool } = await createPgPool({ connectionString: config.pgUrl, max: config.pgPoolMax });
  const db = createPgDatabase(pg, pool);
  const bucket = await createFsBucket(config.objectRoot);
  const env = buildEnv(config.envVars, db, bucket);

  const http = (await dynamicImport('node:http')) as HttpModuleLike;
  const server = http.createServer(createRequestListener(config, env));
  await listen(server, config.bindPort, config.bindHost);

  const port = resolvePort(server, config.bindPort);
  console.log(
    '[fluxing-sync] 自建运行时已监听 http://' +
      config.bindHost +
      ':' +
      String(port) +
      '（对象树根 ' +
      bucket.root +
      '，PG 池上限 ' +
      String(config.pgPoolMax) +
      '）',
  );

  let closing: Promise<void> | null = null;
  return {
    origin: 'http://' + config.bindHost + ':' + String(port),
    port,
    close(): Promise<void> {
      if (closing === null) {
        closing = (async () => {
          await closeServer(server);
          await pool.end?.();
        })();
      }
      return closing;
    },
  };
}

/**
 * 从进程环境起服务，并挂 SIGINT / SIGTERM。
 *
 * 返回实例是为了让测试能拿到实际端口并主动关闭；生产入口见 `node-main.ts`。
 */
export async function startFromEnvironment(): Promise<StartedServer> {
  const proc = readProcess();
  const started = await startServer(readRuntimeConfig(proc?.env ?? {}));
  for (const signal of ['SIGINT', 'SIGTERM']) {
    proc?.on?.(signal, () => {
      void started.close().then(() => {
        proc?.exit?.(0);
      });
    });
  }
  return started;
}

function readProcess(): ProcessLike | undefined {
  return (globalThis as unknown as { process?: ProcessLike }).process;
}

function listen(server: HttpServerLike, port: number, host: string): Promise<void> {
  return new Promise<void>((resolve, reject) => {
    // 这个监听器**保留不摘**：绑端口失败要 reject，运行期 server 级错误也要有人接住。
    // resolve 之后再 reject 是空操作，不会造成误判。
    server.on('error', (error: Error) => {
      reject(error);
    });
    server.listen(port, host, () => {
      resolve();
    });
  });
}

function closeServer(server: HttpServerLike): Promise<void> {
  return new Promise<void>((resolve) => {
    // Node 19+ 的 close() 会顺带关掉空闲 keep-alive 连接，所以在途请求能自然收尾。
    server.close(() => {
      resolve();
    });
  });
}

function resolvePort(server: HttpServerLike, fallback: number): number {
  const address = server.address?.();
  if (address !== undefined && address !== null && typeof address === 'object') {
    if (typeof address.port === 'number') return address.port;
  }
  return fallback;
}