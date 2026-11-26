/**
 * 地基自证（W5）：证明「同一份 handler 能在真 PostgreSQL 上跑」这一件事成立，
 * 而不是证明某个业务端点正确 —— 业务回归由后续搬迁的测试文件负责。
 *
 * 四条断言，每条都指向一个具体的失败模式：
 *   1. `GET /v1/health` 200        → `worker.fetch()` 在 Node 里可调，`route()` 分派正常；
 *   2. 9 张表全部存在               → `migrations-pg/0001_init.sql` 落在**本文件的 schema**
 *                                    （证明 `options=-c search_path=` 真的生效，没有污染 public）；
 *   3. shim 写入-读回往返           → `prepare/bind/first/run` 与 `meta.changes` 在真 PG 上成立；
 *   4. 无令牌访问受保护端点 → 401   → 鉴权链路在 PG 底座上不是「全放行」。
 */

import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { createPgTestContext, type PgTestContext } from './harness';

/** 与 `migrations-pg/0001_init.sql` 的 9 个 CREATE TABLE 一一对应。 */
const EXPECTED_TABLES = [
  'devices',
  'email_verifications',
  'mail_outbox',
  'password_resets',
  'refresh_tokens',
  'schema_migrations',
  'sync_buckets',
  'sync_items',
  'users',
];

let ctx: PgTestContext;

beforeAll(async () => {
  ctx = await createPgTestContext('harness');
});

afterAll(async () => {
  await ctx.close();
});

describe('PG 测试底座地基', () => {
  it('handler 在 Node 里可调用：GET /v1/health 返回 200', async () => {
    const result = await ctx.api('GET', '/v1/health');
    expect(result.status).toBe(200);
    expect(result.body).toBeTypeOf('object');
  });

  it('DDL 落在本文件的私有 schema：9 张表齐全且不落在 public', async () => {
    const rows = await ctx.env.DB.prepare(
      "SELECT tablename FROM pg_tables WHERE schemaname = current_schema() ORDER BY tablename",
    ).all<{ tablename: string }>();

    const names = (rows.results ?? []).map((row) => row.tablename);
    expect(names).toEqual(EXPECTED_TABLES);

    const current = await ctx.env.DB.prepare('SELECT current_schema() AS s').first<{ s: string }>();
    expect(current?.s).toBe('t_harness');
  });

  it('shim 写入-读回往返：meta.changes 与 first() 语义成立', async () => {
    const email = 'ground@test.local';
    const inserted = await ctx.env.DB.prepare(
      'INSERT INTO users (id, email, verifier, salt, kdf_algo, kdf_iters, email_verified, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
    )
      .bind('u_ground', email, 'verifier-placeholder', 'salt-placeholder', 'PBKDF2-SHA256', 600_000, 0, 1_700_000_000)
      .run();
    expect(inserted.meta.changes).toBe(1);

    const row = await ctx.env.DB.prepare('SELECT email, email_verified FROM users WHERE email = ?')
      .bind(email)
      .first<{ email: string; email_verified: number }>();
    expect(row?.email).toBe(email);
    expect(row?.email_verified).toBe(0);

    const updated = await ctx.env.DB.prepare(
      'UPDATE users SET email_verified = ? WHERE email = ?',
    )
      .bind(1, email)
      .run();
    expect(updated.meta.changes).toBe(1);

    const missing = await ctx.env.DB.prepare('SELECT email FROM users WHERE email = ?')
      .bind('nobody@test.local')
      .first();
    expect(missing).toBeNull();
  });

  it('鉴权链路在 PG 底座上不放行：无令牌访问 /v1/sync/pull 返回 401', async () => {
    const result = await ctx.api('GET', '/v1/sync/pull');
    expect(result.status).toBe(401);
  });
});