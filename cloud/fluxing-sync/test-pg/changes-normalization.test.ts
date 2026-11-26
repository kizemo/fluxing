/**
 * W-5.3 · `rowCount === null` 归一化在**真 PostgreSQL** 上的三条路径判据。
 *
 * 背景（handoff §W-5.3）：`src/pg/d1-shim.ts` 把 pg 的 `result.rowCount ?? 0` 归一成
 * D1 的 `meta.changes`（数字）。PG 对 DDL 返回 `rowCount === null`，若不归一，
 * 消费方 `if (changes === 0)` 会静默走错分支。本文件把三条路径逐条钉住：
 *
 *   1. DDL（`CREATE TABLE`）        ⇒ PG rowCount 为 null ⇒ changes 必须是 **0**（不是 null / undefined）；
 *   2. UPDATE 影响 0 行              ⇒ PG rowCount 为 0    ⇒ changes 必须是 **0**；
 *   3. 正常写入（INSERT / UPDATE / DELETE 各命中 1 行）⇒ changes 必须是 **1**。
 *
 * 三条路径都额外断言 `typeof changes === 'number'` —— 这是「恒为数字」的字面含义。
 *
 * 与 `d1-shim-unit.test.ts`（假 pool，只验归一逻辑）、
 * `d1-shim-pg.test.ts`（真库，但只覆盖 DDL / UPDATE-0 / batch 原子）的关系：
 * 本文件是**逐路径表驱动**的补充，把「正常写入 ⇒ 1」这条也拉进真库口径。
 *
 * 隔离：独占 schema `t_norm`，随 context 关闭一起 drop。
 */

import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { createPgTestContext, type PgTestContext } from './harness';

let ctx: PgTestContext;

beforeAll(async () => {
  ctx = await createPgTestContext('norm');
  await ctx.env.DB.prepare('CREATE TABLE w6_norm_tmp (id TEXT PRIMARY KEY, v TEXT NOT NULL)').run();
});

afterAll(async () => {
  await ctx.close();
});

/** 三条路径的表驱动判据：SQL + 绑定值 + 期望的 changes。 */
const CASES: Array<{ name: string; sql: string; bind: unknown[]; expect: number }> = [
  {
    name: 'DDL（CREATE TABLE）⇒ PG rowCount 为 null ⇒ changes 归一为 0',
    sql: 'CREATE TABLE w6_norm_ddl_tmp (id INTEGER)',
    bind: [],
    expect: 0,
  },
  {
    name: 'UPDATE 影响 0 行 ⇒ changes 为 0',
    sql: 'UPDATE w6_norm_tmp SET v = ? WHERE id = ?',
    bind: ['x', '不存在的键'],
    expect: 0,
  },
  {
    name: 'DELETE 影响 0 行 ⇒ changes 为 0',
    sql: 'DELETE FROM w6_norm_tmp WHERE id = ?',
    bind: ['不存在的键'],
    expect: 0,
  },
  {
    name: 'INSERT 正常写入 1 行 ⇒ changes 为 1',
    sql: 'INSERT INTO w6_norm_tmp (id, v) VALUES (?, ?)',
    bind: ['w6-a', '1'],
    expect: 1,
  },
  {
    name: 'UPDATE 命中 1 行 ⇒ changes 为 1',
    sql: 'UPDATE w6_norm_tmp SET v = ? WHERE id = ?',
    bind: ['2', 'w6-a'],
    expect: 1,
  },
  {
    name: 'DELETE 命中 1 行 ⇒ changes 为 1',
    sql: 'DELETE FROM w6_norm_tmp WHERE id = ?',
    bind: ['w6-a'],
    expect: 1,
  },
];

describe('W-5.3 · meta.changes 在真库三条路径上恒为数字', () => {
  for (const testCase of CASES) {
    it(testCase.name, async () => {
      const result = await ctx.env.DB.prepare(testCase.sql).bind(...testCase.bind).run();
      expect(result.meta.changes).toBe(testCase.expect);
      expect(typeof result.meta.changes).toBe('number');
      expect(result.meta.changes).not.toBeNull();
      expect(result.meta.changes).not.toBeUndefined();
    });
  }

  it('批量 batch 里的每条语句也各自拿到数字型 changes', async () => {
    const results = await ctx.env.DB.batch([
      ctx.env.DB.prepare('INSERT INTO w6_norm_tmp (id, v) VALUES (?, ?)').bind('w6-b', '1'),
      ctx.env.DB.prepare('UPDATE w6_norm_tmp SET v = ? WHERE id = ?').bind('2', 'w6-b'),
      ctx.env.DB.prepare('DELETE FROM w6_norm_tmp WHERE id = ?').bind('不存在的键'),
    ]);
    expect(results.length).toBe(3);
    expect(results[0]?.meta.changes).toBe(1);
    expect(results[1]?.meta.changes).toBe(1);
    expect(results[2]?.meta.changes).toBe(0);
    for (const result of results) {
      expect(typeof result.meta.changes).toBe('number');
    }
  });

  it('业务代码真正消费 changes 的两条路径：DELETE 命中 1 行 ⇒ 计数为 1，命中 0 行 ⇒ 计数为 0', async () => {
    // 对应 src/account.ts:92 / :97 的消费口径：`removed.meta?.changes ?? 0`。
    await ctx.env.DB.prepare('INSERT INTO w6_norm_tmp (id, v) VALUES (?, ?)').bind('w6-c', '1').run();

    const removed = await ctx.env.DB.prepare('DELETE FROM w6_norm_tmp WHERE id = ?')
      .bind('w6-c')
      .run();
    expect(removed.meta?.changes ?? 0).toBe(1);

    const removedAgain = await ctx.env.DB.prepare('DELETE FROM w6_norm_tmp WHERE id = ?')
      .bind('w6-c')
      .run();
    expect(removedAgain.meta?.changes ?? 0).toBe(0);
  });
});
