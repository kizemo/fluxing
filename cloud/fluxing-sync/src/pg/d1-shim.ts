// D1 → node-postgres 兼容层（路线 B · ADR 0079）。
//
// 目标：让 13 个 handler / wire.ts / crypto.ts / errors.ts **一个字都不改**地跑在自建 PG 上。
// 手段：只替换边界 —— 复刻 `D1Database` 被实际消费的那一小块 API。刻意**不**逐条改写
// 全仓 64 处 `prepare` / 63 处 `bind` / 5 处 `batch`：那样会把一次「换底座」变成一次
// 「重写同步逻辑」，而同步逻辑正是事故史最密的地方（ADR 0076 / 0077 / 0078）。
//
// 被消费的 API 面（2026-11-26 逐文件回读全仓 12 个 .ts / 3,664 行，逐项计数）：
//   prepare(sql)            → PgCompatStatement                64 处
//   stmt.bind(...)          → PgCompatStatement（不可变，D1 语义）   63 处
//   all<T>()                → { results: T[] }                 3 个文件各 2 处
//   first<T>()              → T | null                         24 处，**无一例传列名**
//   run()                   → { meta: { changes } }            只有 account.ts:92 / :97 消费 changes
//   DB.batch(stmts[])       → 事务内全成功或全不生效               5 处（devices 1 / sync 2 / buckets 2）
//   **未使用**：`.raw()` / `.changes` / `.last_row_id` / `.duration`（全仓 0 命中 ⇒ 不实现，
//   实现反而会造出「看着能用其实没测过」的假面）。
//
// 三处不能想当然的语义：
//   1. **占位符**：SQLite 用 `?`，PG 用 `$n`。全仓 301 个 `?`，必须按出现顺序重编号，
//      且必须跳过字符串字面量 / 双引号标识符 / 行注释 / 块注释 —— 否则 `'?'` 这类
//      字面量会把后面所有编号顶歪一位，症状是「参数类型不匹配」这种看不出根因的报错。
//   2. **BIGINT**：node-postgres 默认把 int8 读成**字符串**。`hlc_ms` / `server_seq` /
//      `id` / `COUNT(*)` 在 Workers 上都是 JS number；不装类型解析器就会静默改变
//      线上响应形状（客户端按 number 解析，字符串会一路传到比较运算）。
//   3. **batch 必须是事务**：sync.ts:391 / sync-buckets.ts:292 是交替 DELETE+INSERT，
//      半途失败若留下「删了没插」的中间态，等于把一次失败升级成一次数据丢失。
//      故 batch = 单连接 + BEGIN/COMMIT + 失败 ROLLBACK + 必 release。

import { dynamicImport } from './dynamic-import';

// ---------------------------------------------------------------------------
// node-postgres 的结构类型（自声明：本仓库零依赖、无 @types/node）
// ---------------------------------------------------------------------------

/** pg 查询结果的最小契约（只取本仓库消费的三个字段）。 */
export interface PgResultLike {
  rows: Record<string, unknown>[];
  /** INSERT / UPDATE / DELETE 的影响行数；其它语句为 null。 */
  rowCount: number | null;
}

/** pg 客户端（连接池取出的单连接）。 */
export interface PgClientLike {
  query(text: string, values?: readonly unknown[]): Promise<PgResultLike>;
}

/** 连接池取出的连接，多一个 release。 */
export interface PgClientHandleLike extends PgClientLike {
  release?(): void;
}

/** pg 连接池。 */
export interface PgPoolLike extends PgClientLike {
  connect(): Promise<PgClientHandleLike>;
  end?(): Promise<void>;
  /** 必须挂：空闲连接报错（PG 重启 / 网络抖动）不挂会直接崩进程。 */
  on?(event: string, listener: (error: Error) => void): unknown;
}

/** pg.types —— 只用到 setTypeParser。 */
export interface PgTypesLike {
  setTypeParser(oid: number, parser: (value: string) => unknown): void;
}

/** `import('pg')` 之后的模块形状。 */
export interface PgModuleLike {
  Pool: new (config: Record<string, unknown>) => PgPoolLike;
  types?: PgTypesLike;
}

/** pg 的 int8（BIGINT）OID。 */
const PG_TYPE_OID_BIGINT = 20;

// ---------------------------------------------------------------------------
// 占位符重编号
// ---------------------------------------------------------------------------

/**
 * 把 SQLite 的 `?` 占位符按出现顺序改写成 PG 的 `$1..$n`。
 *
 * 跳过四类区域（其中的 `?` 不是占位符）：
 *   · 单引号字符串字面量，`''` 为转义；
 *   · 双引号标识符，`""` 为转义；
 *   · `--` 行注释（到行尾）；
 *   · 斜杠星号对包起来的块注释（未闭合则吃到结尾 —— SQL 本身已坏，交给 PG 报错）。
 *
 * 不做的事：美元引用（`$$...$$` / `$tag$...$tag$`）。本仓库 SQL 里零出现，
 * 一旦将来出现，`$` 会被原样透传而不是被误当占位符 —— 失败模式是「PG 报语法错」，
 * 不是「静默错位」，可接受。
 *
 * @param sql 原始 SQL
 * @returns 占位符已改写为 `$n` 的 SQL
 */
export function renumberPlaceholders(sql: string): string {
  const parts: string[] = [];
  let ordinal = 0;
  let index = 0;
  const length = sql.length;

  while (index < length) {
    const char = sql.charAt(index);

    if (char === "'" || char === '"') {
      const start = index;
      index += 1;
      while (index < length) {
        const current = sql.charAt(index);
        if (current === char) {
          if (sql.charAt(index + 1) === char) {
            index += 2;
            continue;
          }
          index += 1;
          break;
        }
        index += 1;
      }
      parts.push(sql.slice(start, index));
      continue;
    }

    if (char === '-' && sql.charAt(index + 1) === '-') {
      const start = index;
      while (index < length && sql.charAt(index) !== '\n') {
        index += 1;
      }
      parts.push(sql.slice(start, index));
      continue;
    }

    if (char === '/' && sql.charAt(index + 1) === '*') {
      const start = index;
      const end = sql.indexOf('*/', index + 2);
      index = end < 0 ? length : end + 2;
      parts.push(sql.slice(start, index));
      continue;
    }

    if (char === '?') {
      ordinal += 1;
      parts.push('$' + ordinal);
      index += 1;
      continue;
    }

    parts.push(char);
    index += 1;
  }

  return parts.join('');
}

// ---------------------------------------------------------------------------
// 绑定值规整
// ---------------------------------------------------------------------------

/**
 * 把 JS 值规整成 pg 能安全绑定的形状。
 *
 *   · `undefined` → `null`：pg 对 undefined 会告警并按 null 处理，显式化免得日志噪音；
 *   · `boolean` → `1` / `0`：库里那些 0/1 位是 SMALLINT（**刻意不用 BOOLEAN**，
 *     见 migrations-pg/0001_init.sql 附录 B-3 —— 保 JSON 响应形状不变）；
 *   · `bigint` → 十进制字符串：pg 的绑定编码器不接受 bigint。
 */
function normalizeBindValue(value: unknown): unknown {
  if (value === undefined) return null;
  if (typeof value === 'boolean') return value ? 1 : 0;
  if (typeof value === 'bigint') return value.toString();
  return value;
}

// ---------------------------------------------------------------------------
// 语句
// ---------------------------------------------------------------------------

/** D1 `all()` / `run()` 的返回形状（只保留被消费的字段）。 */
export interface PgCompatResultLike<T> {
  success: true;
  results: T[];
  meta: { changes: number };
}

/** 一条已编译好的 SQL（供 batch 复用，避免二次重编号）。 */
export interface CompiledQuery {
  text: string;
  values: readonly unknown[];
}

/**
 * D1 预编译语句的替身。
 *
 * 不可变：`bind()` 返回**新实例**（与 D1 一致）。这很重要 —— 调用方普遍写成
 * `env.DB.prepare(sql).bind(...)` 一行了事，但 sync.ts 里也存在「同一 SQL 多次 bind
 * 成不同语句再 batch」的写法；可变实现会让那些语句共享最后一次绑定值。
 */
export class PgCompatStatement {
  readonly #database: PgCompatDatabase;
  readonly #sql: string;
  readonly #values: readonly unknown[];
  #compiled: CompiledQuery | null;

  constructor(database: PgCompatDatabase, sql: string, values: readonly unknown[]) {
    this.#database = database;
    this.#sql = sql;
    this.#values = values;
    this.#compiled = null;
  }

  /** 绑定参数；再次调用覆盖上一次（D1 语义），返回新语句。 */
  bind(...values: readonly unknown[]): PgCompatStatement {
    return new PgCompatStatement(this.#database, this.#sql, values.map(normalizeBindValue));
  }

  /** 编译（重编号一次并缓存）。batch 需要它，故为公开方法，但只在内部使用。 */
  toQuery(): CompiledQuery {
    if (this.#compiled === null) {
      this.#compiled = {
        text: renumberPlaceholders(this.#sql),
        values: this.#values,
      };
    }
    return this.#compiled;
  }

  /** 取全部行。 */
  async all<T = Record<string, unknown>>(): Promise<PgCompatResultLike<T>> {
    const result = await this.#database.query(this.toQuery());
    return {
      success: true,
      results: result.rows as T[],
      meta: { changes: result.rowCount ?? 0 },
    };
  }

  /**
   * 取第一行。
   *
   * 全仓 24 处调用**无一例传列名**，但 D1 的这个重载是真实存在的，
   * 补上以免将来第一处 `first<string>('id')` 静默返回整行对象。
   */
  async first<T = Record<string, unknown>>(columnName?: string): Promise<T | null> {
    const result = await this.#database.query(this.toQuery());
    const row = result.rows[0];
    if (row === undefined) return null;
    if (columnName !== undefined) return row[columnName] as T;
    return row as T;
  }

  /** 执行写语句。`changes` 只被 account.ts:92 / :97 消费。 */
  async run(): Promise<PgCompatResultLike<Record<string, unknown>>> {
    const result = await this.#database.query(this.toQuery());
    return {
      success: true,
      results: result.rows,
      meta: { changes: result.rowCount ?? 0 },
    };
  }
}

// ---------------------------------------------------------------------------
// 数据库
// ---------------------------------------------------------------------------

/**
 * `D1Database` 的替身：只有 `prepare` 与 `batch`。
 *
 * 调用方由 node-server.ts 在构造 `Env` 时一次性 `as unknown as D1Database`，
 * 故本类**不**试图满足 D1 的完整类型面 —— 那是假承诺。
 */
export class PgCompatDatabase {
  readonly #pool: PgPoolLike;

  constructor(pool: PgPoolLike) {
    this.#pool = pool;
  }

  /** 预编译（本层只存 SQL 与绑定值，真正的 prepare 由 PG 在 query 时做）。 */
  prepare(sql: string): PgCompatStatement {
    return new PgCompatStatement(this, sql, []);
  }

  /**
   * 批量执行，**事务内全成功或全不生效**。
   *
   * 空数组直接返回，不取连接（clearPrefix 在空库上会传空列表）。
   */
  async batch(statements: readonly PgCompatStatement[]): Promise<PgCompatResultLike<Record<string, unknown>>[]> {
    if (statements.length === 0) return [];

    const client = await this.#pool.connect();
    const results: PgCompatResultLike<Record<string, unknown>>[] = [];
    try {
      await client.query('BEGIN');
      for (const statement of statements) {
        const compiled = statement.toQuery();
        const result = await client.query(compiled.text, compiled.values);
        results.push({
          success: true,
          results: result.rows,
          meta: { changes: result.rowCount ?? 0 },
        });
      }
      await client.query('COMMIT');
      return results;
    } catch (error) {
      try {
        await client.query('ROLLBACK');
      } catch {
        // 连接已坏（网络断 / 后端重启）时 ROLLBACK 本身会失败。
        // 此时 PG 侧事务随连接断开自动回滚，不能让它掩盖真正的原始错误。
      }
      throw error;
    } finally {
      client.release?.();
    }
  }

  /** 单条查询（内部用；batch 走独立连接以保证事务性）。 */
  async query(compiled: CompiledQuery): Promise<PgResultLike> {
    return this.#pool.query(compiled.text, compiled.values);
  }
}

// ---------------------------------------------------------------------------
// 装配
// ---------------------------------------------------------------------------

/**
 * 安装必要的类型解析器。
 *
 * 只装一个：int8 → JS number（并在超出安全整数时原样返回字符串，而不是静默丢精度）。
 * 其它 OID 一律不动 —— 动得越多，与 Workers 上的响应形状差异越大。
 */
export function installTypeParsers(pg: PgModuleLike): void {
  pg.types?.setTypeParser(PG_TYPE_OID_BIGINT, (value: string) => {
    const parsed = Number(value);
    return Number.isSafeInteger(parsed) ? parsed : value;
  });
}

/**
 * 建立兼容层数据库。
 *
 * `pg` 由调用方传入（而非本模块 import），这样单测可以塞假模块，
 * 也保证本文件不出现任何静态 `import 'pg'`（Workers 打包范围会因此失败）。
 */
export function createPgDatabase(pg: PgModuleLike, pool: PgPoolLike): PgCompatDatabase {
  installTypeParsers(pg);
  return new PgCompatDatabase(pool);
}

/**
 * 运行时加载 `pg` 模块。
 *
 * ESM/CJS 互操作两种形态都接受：`import('pg')` 可能给出命名空间（`Pool` 在顶层），
 * 也可能只给 `{ default: module }`（取决于 pg 的导出写法与 Node 版本的 lexer 能力）。
 * 不判定会得到「Pool is not a constructor」这种与根因无关的报错。
 */
export async function loadPgModule(): Promise<PgModuleLike> {
  const namespace = (await dynamicImport('pg')) as PgModuleLike & { default?: PgModuleLike };
  if (typeof namespace.Pool === 'function') return namespace;
  const fallback = namespace.default;
  if (fallback !== undefined && typeof fallback.Pool === 'function') return fallback;
  throw new Error('无法加载 pg 模块：未找到 Pool 构造器。请确认已安装 pg。');
}

/** 按连接串建池。 */
export async function createPgPool(options: {
  connectionString: string;
  max: number;
}): Promise<{ pg: PgModuleLike; pool: PgPoolLike }> {
  const pg = await loadPgModule();
  const pool = new pg.Pool({
    connectionString: options.connectionString,
    max: options.max,
    idleTimeoutMillis: 30_000,
    connectionTimeoutMillis: 10_000,
  });
  // 空闲连接错误必须有人接住：不挂这个监听器，一次 PG 重启就能让整个同步服务进程退出。
  pool.on?.('error', (error: Error) => {
    console.error('[fluxing-sync] 空闲 PG 连接报错：' + error.message);
  });
  return { pg, pool };
}