/**
 * W6 · D1 兼容层在**真 PostgreSQL** 上的判据（路线 B · 本机 PG 18.4）。
 *
 * 纯单元判据在 `d1-shim-unit.test.ts`；本文件只验**只有真库才能验**的四件事：
 *   1. `batch()` 真原子：第二条语句失败 ⇒ **第一条也必须回滚**（假 pool 只能验「发了 ROLLBACK」，
 *      验不了「数据真的没落库」—— 这一条是 D1 `batch` 语义的命门）；
 *   2. `batch()` 成功路径：两条都落库，且 `meta.changes` 各为 1；
 *   3. 真 PG 上 `first()` 无行 ⇒ `null`（不是 pg 原生的 `undefined`）；
 *   4. 真 PG 上 `rowCount === null`（DDL）⇒ `meta.changes === 0`；UPDATE 影响 0 行也 ⇒ 0。
 *
 * 隔离：每个测试文件独占一个 PG schema（`t_shim`），`vitest.pg.config.ts` 串行跑。
 * 自建表 `w6_batch_tmp` 随 schema 一起被 drop ⇒ 不用手工清理。
 */

import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { createPgTestContext, type PgTestContext } from './harness';

let ctx: PgTestContext;

beforeAll(async () => {
  ctx = await createPgTestContext('shim');
  await ctx.env.DB.prepare('CREATE TABLE w6_batch_tmp (id TEXT PRIMARY KEY, v TEXT NOT NULL)').run();
});

afterAll(async () => {
  await ctx.close();
});

describe('W6 · batch 真原子', () => {
  it('第二条违反唯一约束 ⇒ 整批回滚，第一条也不落库', async () => {
    await ctx.env.DB.prepare('DELETE FROM w6_batch_tmp').run();

    const statements = [
      ctx.env.DB.prepare('INSERT INTO w6_batch_tmp (id, v) VALUES (?, ?)').bind('dup', '1'),
      // 同主键 ⇒ PG 报 23505，让整批在中途失败。
      ctx.env.DB.prepare('INSERT INTO w6_batch_tmp (id, v) VALUES (?, ?)').bind('dup', '2'),
    ];

    await expect(ctx.env.DB.batch(statements)).rejects.toThrow();

    const rows = await ctx.env.DB.prepare('SELECT id FROM w6_batch_tmp WHERE id = ?')
      .bind('dup')
      .all<{ id: string }>();
    // 关键断言：第一条也必须没落库 —— 这才是「全成功或全不生效」。
    expect(rows.results.length).toBe(0);
  });

  it('全部成功 ⇒ 两条都落库，且 meta.changes 各为 1', async () => {
    await ctx.env.DB.prepare('DELETE FROM w6_batch_tmp').run();

    const results = await ctx.env.DB.batch([
      ctx.env.DB.prepare('INSERT INTO w6_batch_tmp (id, v) VALUES (?, ?)').bind('ok1', 'a'),
      ctx.env.DB.prepare('INSERT INTO w6_batch_tmp (id, v) VALUES (?, ?)').bind('ok2', 'b'),
    ]);

    expect(results.length).toBe(2);
    expect(results[0]?.meta.changes).toBe(1);
    expect(results[1]?.meta.changes).toBe(1);

    const rows = await ctx.env.DB.prepare('SELECT id FROM w6_batch_tmp').all<{ id: string }>();
    expect(rows.results.length).toBe(2);
  });
});

describe('W6 · 真库上的空值与 changes 归一', () => {
  it('first() 无行 ⇒ null（不是 undefined）', async () => {
    const row = await ctx.env.DB.prepare('SELECT id FROM w6_batch_tmp WHERE id = ?')
      .bind('不存在的键')
      .first();
    expect(row).toBeNull();
    expect(row).not.toBeUndefined();
  });

  it('DDL（rowCount === null）⇒ meta.changes === 0', async () => {
    const result = await ctx.env.DB.prepare('CREATE TABLE w6_ddl_tmp (id INTEGER)').run();
    expect(result.meta.changes).toBe(0);
    expect(result.meta.changes).not.toBeNull();
  });

  it('UPDATE 影响 0 行 ⇒ meta.changes === 0', async () => {
    const result = await ctx.env.DB.prepare('UPDATE w6_batch_tmp SET v = ? WHERE id = ?')
      .bind('x', '不存在的键')
      .run();
    expect(result.meta.changes).toBe(0);
  });

  it('占位符数 ≠ 绑定参数数 ⇒ 真库上也先失败，不把参数错位发出去', async () => {
    await expect(
      ctx.env.DB.prepare('SELECT ? AS a, ? AS b').bind(1).first(),
    ).rejects.toThrow(/占位符个数/);
  });
});
