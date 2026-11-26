/**
 * W-5.5 · `batch()` 在**五个业务调用点**上的真原子性逐点判据（真 PostgreSQL）。
 *
 * 背景（handoff §W-5.5 / ADR 0079）：`src/pg/d1-shim.ts` 的 `batch()` = 单连接 +
 * BEGIN/COMMIT + 失败 ROLLBACK。真库层面「第二条失败 ⇒ 第一条也不落库」已由
 * `d1-shim-pg.test.ts` 验过一次，但那是**shim 通用形状**；本文件验的是
 * **五个业务调用点各自的语句序列**在真中途失败下是否真的「全不生效」。
 *
 * 五个调用点（逐点枚举见 `d1-api-surface-audit.test.ts`）：
 *   1. `src/devices.ts:143`        设备撤销：UPDATE devices + UPDATE refresh_tokens
 *   2. `src/sync.ts:391`           条目推送：DELETE + INSERT 交替（每条目 2 条）
 *   3. `src/sync.ts:473`           条目拉取自愈：多条 DELETE FROM sync_items
 *   4. `src/sync-buckets.ts:292`   桶推送：DELETE + INSERT 交替
 *   5. `src/sync-buckets.ts:394`   桶拉取自愈：多条 DELETE FROM sync_buckets
 *
 * 手法：在**真库**上装一个 BEFORE 触发器，让批次里的**第 2 条**（或中间某条）必然抛错
 * —— 这才是真正的「中途失败」；再断言**第一条也没落库**（不留「删了没插」的中间态）。
 * 每个用例都带**阳性对照**：撤掉触发器后同一操作必须成功 ⇒ 证明失败确实是触发器注入的。
 *
 * ⚠ 已知边界（**不在本文件的断言范围内**，但必须登记）：
 *   R2 写入发生在 D1 batch **之前**且**不在事务内**（`sync.ts:340` / `sync-buckets.ts:240`），
 *   所以批次失败时 R2 对象已被覆盖、而 D1 行仍是旧版 ⇒ 密文与 HLC 可能短暂不一致。
 *   本文件只断言 **D1 侧**的原子性（batch 的语义边界），R2 一致性另立批次。
 *
 * 隔离：独占 schema `t_atomic`，随 context 关闭一起 drop。
 */

import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { createPgTestContext, latestCode, type PgTestContext } from './harness';

const HLC_MS = 1_700_000_000_000;
/** 12 字节 nonce（对齐 FluxingCloudEnvelope.h 的 kNonceBytes）。 */
const NONCE = b64(new Uint8Array(12).fill(7));
/** 48 字节密文（≥ 16 字节认证标签即可通过校验）。 */
const CIPHER = b64(new Uint8Array(48).fill(9));

let ctx: PgTestContext;

beforeAll(async () => {
  ctx = await createPgTestContext('atomic');
});

afterAll(async () => {
  await ctx.close();
});

function b64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

interface ApiOutcome {
  status: number;
  body: any;
  /** 请求是否以异常形式抛出（而不是返回 4xx / 5xx 响应）。 */
  thrown: boolean;
}

/** 包一层 try/catch：批次失败既可能表现为 500 响应，也可能直接抛异常，两种都算「失败」。 */
async function callApi(
  method: string,
  path: string,
  options: { body?: unknown; token?: string } = {},
): Promise<ApiOutcome> {
  try {
    const result = await ctx.api(method, path, options);
    return { status: result.status, body: result.body, thrown: false };
  } catch (error) {
    return {
      status: 0,
      body: { thrown: error instanceof Error ? error.message : String(error) },
      thrown: true,
    };
  }
}

/** 注册 → 取码 → 验证 → 登录，返回 access token。 */
async function signUpAndLogin(email: string, device: string): Promise<string> {
  const authKey = `auth-key-${email}`;

  const registered = await ctx.api('POST', '/v1/auth/register', {
    body: { email, auth_key: authKey, device },
  });
  expect(registered.status).toBe(200);

  const code = await latestCode(ctx.env, email);
  const verified = await ctx.api('POST', '/v1/auth/verify', { body: { email, code } });
  expect(verified.status).toBe(200);

  const loggedIn = await ctx.api('POST', '/v1/auth/login', {
    body: { email, auth_key: authKey, device },
  });
  expect(loggedIn.status).toBe(200);
  return loggedIn.body.access_token as string;
}

function item(itemKey: string, overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    item_key: itemKey,
    type: 'config',
    hlc: { ms: HLC_MS, counter: 0, device: 'device-a' },
    nonce: NONCE,
    cipher: CIPHER,
    ...overrides,
  };
}

/** 合成桶 blob：item_count 个 64 字节块（服务端按 ADR 0077 D3 不解析桶内容）。 */
function blobFor(itemCount: number, fill: number): Uint8Array {
  const out = new Uint8Array(64 * itemCount);
  out.fill(fill);
  return out;
}

function bucket(
  bucketKey: string,
  itemCount = 3,
  overrides: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    bucket_key: bucketKey,
    hlc: { ms: HLC_MS, counter: 0, device: 'hlc-device-a' },
    item_count: itemCount,
    cipher: b64(blobFor(itemCount, 3)),
    ...overrides,
  };
}

interface TriggerSpec {
  /** 触发器名（同时决定函数名 `<name>_fn`）。 */
  name: string;
  table: string;
  event: 'INSERT' | 'UPDATE' | 'DELETE';
  /** 命中条件所比较的列。 */
  column: string;
  /** 命中条件所比较的值。 */
  value: string;
}

/**
 * 装一个「命中即抛错」的 BEFORE 行触发器。
 *
 * 为什么用触发器而不是语法错 / 唯一约束冲突：
 *   · 唯一约束冲突只能让**冲突那一条**失败，且必须先有一条已存在的冲突行；
 *   · 触发器可以精确地让**批次里的第 2 条（或中间某条）**失败 ⇒ 这才是「中途失败」；
 *   · 触发器活在测试 schema 里，随 schema 一起 drop ⇒ 不污染生产库。
 */
async function installFailingTrigger(spec: TriggerSpec): Promise<void> {
  const fn = `${spec.name}_fn`;
  const row = spec.event === 'DELETE' ? 'OLD' : 'NEW';
  await ctx.env.DB.prepare(
    `CREATE OR REPLACE FUNCTION ${fn}() RETURNS trigger AS $$
BEGIN
  IF ${row}.${spec.column} = '${spec.value}' THEN
    RAISE EXCEPTION 'w6 注入的中途失败';
  END IF;
  RETURN ${row};
END;
$$ LANGUAGE plpgsql`,
  ).run();
  await ctx.env.DB.prepare(`DROP TRIGGER IF EXISTS ${spec.name} ON ${spec.table}`).run();
  await ctx.env.DB.prepare(
    `CREATE TRIGGER ${spec.name} BEFORE ${spec.event} ON ${spec.table} FOR EACH ROW EXECUTE FUNCTION ${fn}()`,
  ).run();
}

async function dropTrigger(name: string, table: string): Promise<void> {
  await ctx.env.DB.prepare(`DROP TRIGGER IF EXISTS ${name} ON ${table}`).run();
}

describe('W-5.5 · 调用点 1/5：src/devices.ts:143 设备撤销', () => {
  it('第二条（UPDATE refresh_tokens）失败 ⇒ 第一条（UPDATE devices）也不得落库', async () => {
    const device = 'device-revoke-w6';
    const token = await signUpAndLogin('w6-atomic-revoke@example.com', device);

    const deviceRow = await ctx.env.DB.prepare('SELECT id FROM devices WHERE device_id = ?')
      .bind(device)
      .first<{ id: string }>();
    expect(deviceRow).not.toBeNull();
    const deviceRowId = deviceRow!.id;

    const tokenRows = await ctx.env.DB.prepare(
      'SELECT COUNT(*) AS n FROM refresh_tokens WHERE device_id = ?',
    )
      .bind(deviceRowId)
      .first<{ n: number }>();
    expect(Number(tokenRows?.n)).toBeGreaterThan(0);

    await installFailingTrigger({
      name: 'w6_trg_revoke',
      table: 'refresh_tokens',
      event: 'UPDATE',
      column: 'device_id',
      value: deviceRowId,
    });
    try {
      const failed = await callApi('DELETE', `/v1/devices/${device}`, { token });
      expect(failed.status).not.toBe(200);

      // 关键断言：devices 那条 UPDATE 也必须没落库（不能只回滚一半）。
      const after = await ctx.env.DB.prepare('SELECT revoked_at FROM devices WHERE id = ?')
        .bind(deviceRowId)
        .first<{ revoked_at: number | null }>();
      expect(after?.revoked_at ?? null).toBeNull();
    } finally {
      await dropTrigger('w6_trg_revoke', 'refresh_tokens');
    }

    // 阳性对照：撤掉触发器后同一操作必须成功 ⇒ 证明上面的失败确实是注入的。
    const ok = await callApi('DELETE', `/v1/devices/${device}`, { token });
    expect(ok.status).toBe(200);
    const afterOk = await ctx.env.DB.prepare('SELECT revoked_at FROM devices WHERE id = ?')
      .bind(deviceRowId)
      .first<{ revoked_at: number | null }>();
    expect(afterOk?.revoked_at ?? null).not.toBeNull();
  });
});

describe('W-5.5 · 调用点 2/5：src/sync.ts:391 条目推送（DELETE + INSERT 交替）', () => {
  it('第二条 INSERT 失败 ⇒ 之前的 DELETE + INSERT 全部回滚（不留「删了没插」）', async () => {
    const token = await signUpAndLogin('w6-atomic-push@example.com', 'device-push-w6');

    const first = await ctx.api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/A')] },
    });
    expect(first.status).toBe(200);

    const before = await ctx.env.DB.prepare(
      'SELECT id, hlc_ms FROM sync_items WHERE item_key = ?',
    )
      .bind('config/A')
      .first<{ id: number; hlc_ms: number }>();
    expect(before).not.toBeNull();

    await installFailingTrigger({
      name: 'w6_trg_push_b',
      table: 'sync_items',
      event: 'INSERT',
      column: 'item_key',
      value: 'config/B',
    });
    try {
      const failed = await callApi('POST', '/v1/sync/push', {
        token,
        body: {
          items: [
            item('config/A', { hlc: { ms: HLC_MS + 1, counter: 0, device: 'device-push-w6' } }),
            item('config/B'),
          ],
        },
      });
      expect(failed.status).not.toBe(200);

      // A 的 DELETE + INSERT 必须一起回滚 ⇒ 行还在、id 与 hlc 都是旧值。
      const after = await ctx.env.DB.prepare(
        'SELECT id, hlc_ms FROM sync_items WHERE item_key = ?',
      )
        .bind('config/A')
        .first<{ id: number; hlc_ms: number }>();
      expect(after).not.toBeNull();
      expect(String(after!.id)).toBe(String(before!.id));
      expect(String(after!.hlc_ms)).toBe(String(before!.hlc_ms));

      // B 完全没落库。
      const b = await ctx.env.DB.prepare('SELECT id FROM sync_items WHERE item_key = ?')
        .bind('config/B')
        .first<{ id: number }>();
      expect(b).toBeNull();
    } finally {
      await dropTrigger('w6_trg_push_b', 'sync_items');
    }

    // 阳性对照。
    const ok = await callApi('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/A', { hlc: { ms: HLC_MS + 1, counter: 0, device: 'device-push-w6' } }),
          item('config/B'),
        ],
      },
    });
    expect(ok.status).toBe(200);
    const rows = await ctx.env.DB.prepare(
      'SELECT item_key FROM sync_items WHERE item_key IN (?, ?)',
    )
      .bind('config/A', 'config/B')
      .all<{ item_key: string }>();
    expect(rows.results.length).toBe(2);
  });
});

describe('W-5.5 · 调用点 3/5：src/sync.ts:473 条目拉取自愈（多条 DELETE）', () => {
  it('中间一条 DELETE 失败 ⇒ 整批零删除（孤儿索引行一条都不能少删）', async () => {
    const token = await signUpAndLogin('w6-atomic-heal@example.com', 'device-heal-w6');

    const pushed = await ctx.api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/heal1'), item('config/heal2')] },
    });
    expect(pushed.status).toBe(200);

    // 制造孤儿：把两条条目的 R2 对象删掉（自愈路径的触发条件）。
    for (const key of ['config/heal1', 'config/heal2']) {
      const row = await ctx.env.DB.prepare('SELECT r2_key FROM sync_items WHERE item_key = ?')
        .bind(key)
        .first<{ r2_key: string }>();
      expect(row).not.toBeNull();
      await ctx.env.BUCKET.delete(row!.r2_key);
    }

    await installFailingTrigger({
      name: 'w6_trg_heal2',
      table: 'sync_items',
      event: 'DELETE',
      column: 'item_key',
      value: 'config/heal2',
    });
    try {
      const failed = await callApi('GET', '/v1/sync/pull?since=0', { token });
      expect(failed.status).not.toBe(200);

      // 整批零删除：两条孤儿索引行都还在。
      const rows = await ctx.env.DB.prepare(
        'SELECT item_key FROM sync_items WHERE item_key IN (?, ?)',
      )
        .bind('config/heal1', 'config/heal2')
        .all<{ item_key: string }>();
      expect(rows.results.length).toBe(2);
    } finally {
      await dropTrigger('w6_trg_heal2', 'sync_items');
    }

    // 阳性对照：撤掉触发器后自愈必须真正删掉两条。
    const ok = await callApi('GET', '/v1/sync/pull?since=0', { token });
    expect(ok.status).toBe(200);
    const rows = await ctx.env.DB.prepare(
      'SELECT item_key FROM sync_items WHERE item_key IN (?, ?)',
    )
      .bind('config/heal1', 'config/heal2')
      .all<{ item_key: string }>();
    expect(rows.results.length).toBe(0);
  });
});

describe('W-5.5 · 调用点 4/5：src/sync-buckets.ts:292 桶推送（DELETE + INSERT 交替）', () => {
  it('第二条 INSERT 失败 ⇒ 之前的 DELETE + INSERT 全部回滚', async () => {
    const token = await signUpAndLogin('w6-atomic-bpush@example.com', 'device-bpush-w6');

    const first = await ctx.api('POST', '/v1/sync/buckets/push', {
      token,
      body: { buckets: [bucket('bk/A')] },
    });
    expect(first.status).toBe(200);

    const before = await ctx.env.DB.prepare(
      'SELECT id, hlc_ms FROM sync_buckets WHERE bucket_key = ?',
    )
      .bind('bk/A')
      .first<{ id: number; hlc_ms: number }>();
    expect(before).not.toBeNull();

    await installFailingTrigger({
      name: 'w6_trg_bpush_b',
      table: 'sync_buckets',
      event: 'INSERT',
      column: 'bucket_key',
      value: 'bk/B',
    });
    try {
      const failed = await callApi('POST', '/v1/sync/buckets/push', {
        token,
        body: {
          buckets: [
            bucket('bk/A', 3, { hlc: { ms: HLC_MS + 1, counter: 0, device: 'hlc-device-a' } }),
            bucket('bk/B'),
          ],
        },
      });
      expect(failed.status).not.toBe(200);

      const after = await ctx.env.DB.prepare(
        'SELECT id, hlc_ms FROM sync_buckets WHERE bucket_key = ?',
      )
        .bind('bk/A')
        .first<{ id: number; hlc_ms: number }>();
      expect(after).not.toBeNull();
      expect(String(after!.id)).toBe(String(before!.id));
      expect(String(after!.hlc_ms)).toBe(String(before!.hlc_ms));

      const b = await ctx.env.DB.prepare('SELECT id FROM sync_buckets WHERE bucket_key = ?')
        .bind('bk/B')
        .first<{ id: number }>();
      expect(b).toBeNull();
    } finally {
      await dropTrigger('w6_trg_bpush_b', 'sync_buckets');
    }

    // 阳性对照。
    const ok = await callApi('POST', '/v1/sync/buckets/push', {
      token,
      body: {
        buckets: [
          bucket('bk/A', 3, { hlc: { ms: HLC_MS + 1, counter: 0, device: 'hlc-device-a' } }),
          bucket('bk/B'),
        ],
      },
    });
    expect(ok.status).toBe(200);
    const rows = await ctx.env.DB.prepare(
      'SELECT bucket_key FROM sync_buckets WHERE bucket_key IN (?, ?)',
    )
      .bind('bk/A', 'bk/B')
      .all<{ bucket_key: string }>();
    expect(rows.results.length).toBe(2);
  });
});

describe('W-5.5 · 调用点 5/5：src/sync-buckets.ts:394 桶拉取自愈（多条 DELETE）', () => {
  it('中间一条 DELETE 失败 ⇒ 整批零删除', async () => {
    const token = await signUpAndLogin('w6-atomic-bheal@example.com', 'device-bheal-w6');

    const pushed = await ctx.api('POST', '/v1/sync/buckets/push', {
      token,
      body: { buckets: [bucket('bk/H1'), bucket('bk/H2')] },
    });
    expect(pushed.status).toBe(200);

    for (const key of ['bk/H1', 'bk/H2']) {
      const row = await ctx.env.DB.prepare('SELECT r2_key FROM sync_buckets WHERE bucket_key = ?')
        .bind(key)
        .first<{ r2_key: string }>();
      expect(row).not.toBeNull();
      await ctx.env.BUCKET.delete(row!.r2_key);
    }

    await installFailingTrigger({
      name: 'w6_trg_bheal2',
      table: 'sync_buckets',
      event: 'DELETE',
      column: 'bucket_key',
      value: 'bk/H2',
    });
    try {
      const failed = await callApi('GET', '/v1/sync/buckets/pull?since=0', { token });
      expect(failed.status).not.toBe(200);

      const rows = await ctx.env.DB.prepare(
        'SELECT bucket_key FROM sync_buckets WHERE bucket_key IN (?, ?)',
      )
        .bind('bk/H1', 'bk/H2')
        .all<{ bucket_key: string }>();
      expect(rows.results.length).toBe(2);
    } finally {
      await dropTrigger('w6_trg_bheal2', 'sync_buckets');
    }

    // 阳性对照。
    const ok = await callApi('GET', '/v1/sync/buckets/pull?since=0', { token });
    expect(ok.status).toBe(200);
    const rows = await ctx.env.DB.prepare(
      'SELECT bucket_key FROM sync_buckets WHERE bucket_key IN (?, ?)',
    )
      .bind('bk/H1', 'bk/H2')
      .all<{ bucket_key: string }>();
    expect(rows.results.length).toBe(0);
  });
});
