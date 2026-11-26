/**
 * W5 测试底座 · 路线 B（自建 PostgreSQL）—— 与 `test/`（workerd + miniflare）**并存**。
 *
 * 两套底座各守一条路线，互不替代：
 *   · `test/**`      → `npm test`     → workerd 真绑定（D1 + R2）      → Workers 路线回归；
 *   · `test-pg/**`   → `npm run test:pg` → 真 PostgreSQL 18.4 + 本地对象树 → 自建路线回归。
 *
 * 同一份业务代码在两边跑：`src/index.ts` 的 `route()` 与 13 个 handler 一个字都不改，
 * 差异只落在边界 —— `env.DB` 换成 `createPgDatabase()` 的 shim，`env.BUCKET` 换成
 * `createFsBucket()`。这就是 W1–W4 的不变量，本文件是它的**可执行证明**。
 *
 * 为什么 Node 侧能直接 `worker.fetch()`（而不是起 HTTP 服务）：
 *   `src/` 里唯一的 workerd 专属引用是 `mail.ts:398` 的**动态** `import('cloudflare:email')`，
 *   而 `MAIL_MODE = 'outbox'` 会在 `mail.ts:367-382` 提前 return ⇒ 该分支结构上不可达，
 *   且动态 import 不被静态解析。其余依赖全是标准 Web API（`crypto.subtle` / `btoa` /
 *   `TextEncoder` / `Request` / `Response`），Node 26 全部原生提供。
 *
 * 隔离策略：每个测试文件独占一个 PostgreSQL schema（`t_<slug>`），
 * 配合 `vitest.pg.config.ts` 的 `fileParallelism: false` 串行跑 ⇒ 文件之间零共享状态。
 * ⚠ `search_path` 必须写进**连接串**（`options=-c search_path=...`）：
 *   `pg.Pool` 是多连接，`SET search_path` 只在单条连接上生效，会随机踩到别的 schema。
 */

import { mkdir, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { randomBytes } from 'node:crypto';

import { setDynamicImport } from '../src/pg/dynamic-import';

// `new Function` 造出来的 `import()` 在 Vitest 的 ESM 运行器里拿不到 import callback
// （实测 `TypeError: A dynamic import callback was not specified.`），这里换成原生 import。
// `@vite-ignore` 让 Vite 不去静态分析变量 specifier；本文件永不进 Workers 打包图。
setDynamicImport((specifier) => import(/* @vite-ignore */ specifier) as Promise<unknown>);

import worker from '../src/index';
import type { Env } from '../src/env';
import { createPgDatabase, createPgPool, loadPgModule } from '../src/pg/d1-shim';
import { createFsBucket } from '../src/pg/r2-fs';

export const BASE = 'https://fluxing-sync.test';

const MIGRATION = new URL('../migrations-pg/0001_init.sql', import.meta.url);

/**
 * 与 `vitest.config.ts` 的 `TEST_PEPPER` 取同一个值：两套底座的 pepper 必须一致，
 * 否则「同一条口令在两套底座上派生出不同 hash」这种假差异会把真缺陷盖掉。
 * 真实值走 `wrangler secret put AUTH_PEPPER` / ECS 的 systemd EnvironmentFile，绝不进代码库。
 */
const TEST_PEPPER = 'fluxing-test-pepper-0123456789abcdef';

export interface ApiOptions {
  body?: unknown;
  token?: string;
  headers?: Record<string, string>;
}

export interface ApiResult {
  status: number;
  body: any;
}

export interface PgTestContext {
  env: Env;
  /** 直接打 handler，不经网络：`worker.fetch(Request, env)`。 */
  api: (method: string, path: string, options?: ApiOptions) => Promise<ApiResult>;
  /** 关 pool + 删对象树临时目录。 */
  close: () => Promise<void>;
}

/** 路线 B 的测试要求一台真 PostgreSQL；缺连接串时直接给出可读中文错误，不静默跳过。 */
function baseUrl(): string {
  const url = process.env.FLUXING_PG_URL;
  if (!url || url.length === 0) {
    throw new Error(
      '缺少环境变量 FLUXING_PG_URL —— 路线 B 的测试需要一台真 PostgreSQL（缺省本机：postgres://…@127.0.0.1:5432/fluxing_sync_test）。',
    );
  }
  return url;
}

/** schema 名只允许 `[a-z][a-z0-9_]*`，杜绝注入。 */
function quoteIdent(name: string): string {
  if (!/^[a-z][a-z0-9_]{0,62}$/.test(name)) {
    throw new Error(`非法 schema 标识符：${name}（只允许小写字母、数字与下划线，且以字母开头）。`);
  }
  return `"${name}"`;
}

/**
 * 建一个隔离的 PG 测试环境。
 *
 * 步骤：① 独立 admin 连接 drop/create schema → `SET search_path` → 跑 `migrations-pg/0001_init.sql`
 * （DDL 自带 `BEGIN;…COMMIT;`，无 `CREATE SCHEMA` / `SET` / `CREATE EXTENSION`，可整段执行）；
 * ② 业务 pool 用带 `options=-c search_path=` 的连接串连同一个 schema；③ 对象树落系统临时目录。
 */
export async function createPgTestContext(slug: string): Promise<PgTestContext> {
  const schema = `t_${slug}`;
  const pg = await loadPgModule();
  const url = baseUrl();

  const admin = new pg.Client({ connectionString: url });
  await admin.connect();
  try {
    await admin.query(`DROP SCHEMA IF EXISTS ${quoteIdent(schema)} CASCADE`);
    await admin.query(`CREATE SCHEMA ${quoteIdent(schema)}`);
    await admin.query(`SET search_path TO ${quoteIdent(schema)}`);
    await admin.query(await readFile(MIGRATION, 'utf8'));
  } finally {
    await admin.end();
  }

  const scoped = new URL(url);
  scoped.searchParams.set('options', `-c search_path=${schema}`);
  // createPgPool / createFsBucket 都是 async（内部要 await 动态 import 拿 pg / node:fs）。
  const { pool } = await createPgPool({ connectionString: scoped.toString(), max: 4 });

  const root = join(tmpdir(), 'fluxing-test-pg', `${slug}-${randomBytes(4).toString('hex')}`);
  await mkdir(root, { recursive: true });

  const env = {
    DB: createPgDatabase(pg, pool),
    BUCKET: await createFsBucket(root),
    ENVIRONMENT: 'test',
    ACCESS_TOKEN_TTL: '3600',
    REFRESH_TOKEN_TTL: '2592000',
    KDF_ALGO: 'PBKDF2-SHA256',
    KDF_ITERS: '600000',
    MAIL_MODE: 'outbox',
    MAIL_FROM: 'no-reply@test.local',
    MAIL_FROM_NAME: '火流猩输入法（测试）',
    MAIL_HTTP_ENDPOINT: 'https://example.invalid/emails',
    MAIL_HTTP_AUTH_HEADER: 'Authorization',
    MAIL_HTTP_AUTH_PREFIX: 'Bearer ',
    WRITE_BUDGET_ROWS_PER_DAY: '80000',
    WRITE_BUDGET_BUCKETS_PER_DAY: '4000',
    AUTH_PEPPER: TEST_PEPPER,
  } as unknown as Env;

  const api = async (
    method: string,
    path: string,
    options: ApiOptions = {},
  ): Promise<ApiResult> => {
    const headers: Record<string, string> = { ...(options.headers ?? {}) };
    if (options.body !== undefined) headers['content-type'] = 'application/json';
    if (options.token !== undefined) headers.authorization = `Bearer ${options.token}`;

    const request = new Request(`${BASE}${path}`, {
      method,
      headers,
      body: options.body === undefined ? undefined : JSON.stringify(options.body),
    });
    const response = await worker.fetch(request, env);
    const text = await response.text();
    return { status: response.status, body: text.length === 0 ? null : JSON.parse(text) };
  };

  const close = async (): Promise<void> => {
    await pool.end();
    await rm(root, { recursive: true, force: true });
  };

  return { env, api, close };
}

/** 从 `mail_outbox` 里取最近一封验证码邮件的 6 位码（`MAIL_MODE=outbox` 专用通道）。 */
export async function latestCode(env: Env, email: string): Promise<string> {
  const row = await env.DB.prepare(
    'SELECT body FROM mail_outbox WHERE to_addr = ? ORDER BY id DESC LIMIT 1',
  )
    .bind(email)
    .first<{ body: string }>();
  if (row === null) throw new Error(`没有发给 ${email} 的验证码邮件`);
  const matched = /验证码是：(\d{6})/.exec(row.body);
  if (matched === null) throw new Error(`验证码邮件格式不符：${row.body.slice(0, 80)}`);
  return matched[1] as string;
}