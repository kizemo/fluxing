/**
 * W6 · D1 兼容层的**纯单元**判据（不碰真 PostgreSQL）。
 *
 * 与 `d1-shim-pg.test.ts` 分开：本文件只验 shim 自身的语义（占位符扫描、
 * 绑定参数个数断言、`first()` 的空值归一、`rowCount === null` 归一、batch 的事务包裹），
 * 用假 pool 驱动 ⇒ **不装 PG 也能跑**，是最快的一道闸。
 *
 * 四条判据各自指向一个具体的失败模式：
 *   1. 占位符扫描：字面量 / 注释 / 标识符里的 `?` 不能被当成占位符；
 *   2. 绑定参数个数断言：个数不符必须**显式失败**（而不是把参数错位发到 PG）；
 *   3. `first()` 无行 ⇒ `null`（D1 语义），不是 `undefined`（pg 原生语义）；
 *   4. `rowCount === null` ⇒ `meta.changes === 0`（否则 `if (changes === 0)` 分支静默走错）。
 */

import { describe, expect, it } from 'vitest';

import {
  createPgDatabase,
  renumberPlaceholders,
  scanPlaceholders,
  type PgModuleLike,
  type PgPoolLike,
  type PgResultLike,
} from '../src/pg/d1-shim';

interface FakeCall {
  text: string;
  values?: readonly unknown[];
}

/** 假 pool：记录每次 query，并按需返回固定结果 / 在第 N 次抛错。 */
function makeFakePool(options: {
  rows?: Record<string, unknown>[];
  rowCount?: number | null;
  failAtCall?: number;
} = {}) {
  const calls: FakeCall[] = [];
  const clientCalls: string[] = [];
  let callIndex = 0;

  const result = (): PgResultLike => ({
    rows: options.rows ?? [{ n: 1 }],
    rowCount: options.rowCount ?? (options.rows ?? [{ n: 1 }]).length,
  });

  const client = {
    async query(text: string, values?: readonly unknown[]): Promise<PgResultLike> {
      callIndex += 1;
      clientCalls.push(text);
      if (options.failAtCall !== undefined && callIndex === options.failAtCall) {
        throw new Error('假库故障');
      }
      return result();
    },
    release(): void {},
  };

  const pool: PgPoolLike = {
    async query(text: string, values?: readonly unknown[]): Promise<PgResultLike> {
      calls.push({ text, values });
      return result();
    },
    async connect() {
      return client;
    },
  };

  return { calls, clientCalls, pool };
}

/** 没有 `types` 的假 pg 模块（`installTypeParsers` 只做可选调用）。 */
const FAKE_PG: PgModuleLike = { Pool: function () {} as unknown as PgPoolLike['constructor'] };

function makeDb(pool: PgPoolLike) {
  return createPgDatabase(FAKE_PG, pool);
}

describe('W6 · 占位符扫描', () => {
  it('按出现顺序编号，且跳过字面量 / 注释 / 标识符里的问号', () => {
    expect(renumberPlaceholders('SELECT * FROM t WHERE a = ? AND b = ?')).toBe(
      'SELECT * FROM t WHERE a = $1 AND b = $2',
    );
    expect(renumberPlaceholders("SELECT '?' AS q, ? AS v")).toBe("SELECT '?' AS q, $1 AS v");
    expect(renumberPlaceholders("SELECT 'it''s ?', ?")).toBe("SELECT 'it''s ?', $1");
    expect(renumberPlaceholders('-- a ? b\nSELECT ?')).toBe('-- a ? b\nSELECT $1');
    expect(renumberPlaceholders('/* a ? b */ SELECT ?')).toBe('/* a ? b */ SELECT $1');
    expect(renumberPlaceholders('SELECT "a?b", ? FROM t')).toBe('SELECT "a?b", $1 FROM t');
    expect(renumberPlaceholders('SELECT 1')).toBe('SELECT 1');
  });

  it('计数与改写结果自洽：placeholderCount === 改写后 $n 的个数', () => {
    const sql = "SELECT '?' , ? /* ? */ , ?";
    const scanned = scanPlaceholders(sql);
    expect(scanned.placeholderCount).toBe(2);
    expect(scanned.text.match(/\$\d+/g)?.length).toBe(2);
    expect(scanned.text).toContain("'?'");
  });
});

describe('W6 · 绑定参数个数断言', () => {
  it('占位符数 ≠ 绑定参数数 ⇒ 显式失败，且报中文错', async () => {
    const { pool } = makeFakePool();
    const db = makeDb(pool);
    await expect(db.prepare('SELECT ? AS a, ? AS b').bind(1).first()).rejects.toThrow(/占位符个数/);
    await expect(db.prepare('SELECT ? AS a, ? AS b').bind(1).first()).rejects.toThrow(/绑定参数个数/);
  });

  it('有占位符却不绑定 ⇒ 同样失败（与 D1「参数不足」同义）', async () => {
    const { pool } = makeFakePool();
    const db = makeDb(pool);
    await expect(db.prepare('SELECT ? AS a').first()).rejects.toThrow(/绑定参数个数/);
  });

  it('错误信息只给个数，不含 SQL 正文与绑定值', async () => {
    const { pool } = makeFakePool();
    const db = makeDb(pool);
    const error = await db
      .prepare('SELECT secret_column FROM t WHERE a = ? AND b = ?')
      .bind('topsecret')
      .first()
      .catch((caught: unknown) => caught as Error);
    expect(error).toBeInstanceOf(Error);
    const message = (error as Error).message;
    expect(message).not.toContain('secret_column');
    expect(message).not.toContain('topsecret');
  });

  it('零占位符 + 零绑定 ⇒ 正常执行', async () => {
    const { pool, calls } = makeFakePool();
    const db = makeDb(pool);
    const row = await db.prepare('SELECT 1 AS n').first<{ n: number }>();
    expect(row).not.toBeNull();
    expect(calls[0]?.text).toBe('SELECT 1 AS n');
    expect(calls[0]?.values).toEqual([]);
  });
});

describe('W6 · first() 的空值归一（D1 null vs pg undefined）', () => {
  it('无行 ⇒ 返回 null，且不是 undefined', async () => {
    const { pool } = makeFakePool({ rows: [] });
    const db = makeDb(pool);
    const row = await db.prepare('SELECT n FROM t WHERE n = ?').bind(1).first();
    expect(row).toBeNull();
    expect(row).not.toBeUndefined();
  });

  it('无行 + 传列名 ⇒ 同样返回 null', async () => {
    const { pool } = makeFakePool({ rows: [] });
    const db = makeDb(pool);
    const row = await db.prepare('SELECT n FROM t WHERE n = ?').bind(1).first('n');
    expect(row).toBeNull();
    expect(row).not.toBeUndefined();
  });

  it('有行 ⇒ 返回整行对象', async () => {
    const { pool } = makeFakePool({ rows: [{ n: 42 }] });
    const db = makeDb(pool);
    const row = await db.prepare('SELECT ? AS n').bind(42).first<{ n: number }>();
    expect(row?.n).toBe(42);
  });
});

describe('W6 · meta.changes 归一', () => {
  it('rowCount === null ⇒ changes === 0（不是 null / undefined）', async () => {
    const { pool } = makeFakePool({ rows: [], rowCount: null });
    const db = makeDb(pool);
    const result = await db.prepare('CREATE TABLE w6_tmp (id INTEGER)').run();
    expect(result.meta.changes).toBe(0);
    expect(result.meta.changes).not.toBeNull();
    expect(result.meta.changes).not.toBeUndefined();
  });

  it('rowCount 为 0 ⇒ changes === 0', async () => {
    const { pool } = makeFakePool({ rows: [], rowCount: 0 });
    const db = makeDb(pool);
    const result = await db.prepare('UPDATE t SET a = ? WHERE b = ?').bind('x', 1).run();
    expect(result.meta.changes).toBe(0);
  });
});

describe('W6 · batch 的事务包裹', () => {
  it('空数组 ⇒ 直接返回 [] 且不取连接', async () => {
    const { pool } = makeFakePool();
    let connectCalled = false;
    const spyPool: PgPoolLike = {
      ...pool,
      async connect() {
        connectCalled = true;
        return pool.connect();
      },
    };
    const db = makeDb(spyPool);
    expect(await db.batch([])).toEqual([]);
    expect(connectCalled).toBe(false);
  });

  it('全部成功 ⇒ BEGIN + COMMIT 都在，且每条语句各拿一次结果', async () => {
    const { pool, clientCalls } = makeFakePool();
    const db = makeDb(pool);
    const results = await db.batch([
      db.prepare('SELECT ? AS n').bind(1),
      db.prepare('SELECT ? AS n').bind(2),
    ]);
    expect(results.length).toBe(2);
    expect(clientCalls).toContain('BEGIN');
    expect(clientCalls).toContain('COMMIT');
    expect(clientCalls).not.toContain('ROLLBACK');
  });

  it('中途失败 ⇒ ROLLBACK，且不 COMMIT（事务不能半提交）', async () => {
    const { pool, clientCalls } = makeFakePool({ failAtCall: 2 });
    const db = makeDb(pool);
    await expect(
      db.batch([db.prepare('SELECT ? AS n').bind(1), db.prepare('SELECT ? AS n').bind(2)]),
    ).rejects.toThrow(/假库故障/);
    expect(clientCalls).toContain('BEGIN');
    expect(clientCalls).toContain('ROLLBACK');
    expect(clientCalls).not.toContain('COMMIT');
  });
});
