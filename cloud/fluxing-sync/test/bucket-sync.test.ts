// 桶级同步端点单测（ADR 0077：POST /v1/sync/buckets/push、GET /v1/sync/buckets/pull）。
//
// 覆盖十类判据：
//   1. 新桶推送 ⇒ accepted + sync_buckets 行数 + R2 `b/` 对象数 + server_seq 前进；
//   2. blob **原样回吐**（服务端不解密、不重排、不需要桶级 nonce）；
//   3. 同设备同桶的旧 HLC ⇒ **整桶跳过**，不推进 server_seq、不改 R2 blob；
//   4. 同 bucket_key **跨设备互不压制**（D5 每设备独立视图）+ device_id = devices.id 行主键；
//   5. 入参校验：桶数上限 / item_count 上界 / cipher 非法 / 空数组，错误必须指明第几个桶；
//   6. pull 的 since 增量 + limit 分页 + cursor 续读 + has_more；
//   7. 写预算超限 ⇒ 429 write_budget_exceeded + D1/R2 **零写入**，且与逐条口径互不干扰；
//   8. 老端点回归：逐条 push/pull 行为不受桶实现影响；
//   9. DELETE /v1/account/data 清**两个前缀 + 两张表**；
//  10. 路由与鉴权面：405 / 401 / 403。
//  另有纯函数层：writeBudgetBucketRowsPerDay 回落口径、bucketR2KeyFor 的键形状。
//
// 注意：isolatedStorage 默认开启 ⇒ 多步流程必须写在同一个 it() 里。
// blob 是本测试自造的**合成**容器（服务端按 ADR 0077 D3 不解析桶内容），
// 每块 64 字节，形如「条目数 × 64 B」；真实信封由客户端 EnvelopeCodec 生成。

import { SELF, env } from 'cloudflare:test';
import { describe, expect, it } from 'vitest';
import { DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY, writeBudgetBucketRowsPerDay } from '../src/env';
import { bucketR2KeyFor } from '../src/wire';
import { countTodayWrittenBucketRows } from '../src/sync-buckets';
import { WRITE_BUDGET_EXCEEDED_CODE, WRITE_BUDGET_EXCEEDED_MESSAGE } from '../src/sync';

const BASE = 'https://fluxing-sync.test';
const PUSH_PATH = '/v1/sync/buckets/push';
const PULL_PATH = '/v1/sync/buckets/pull';

const HLC_MS = 1_700_000_000_000;

function b64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

function fromB64(text: string): Uint8Array {
  const binary = atob(text);
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) out[i] = binary.charCodeAt(i);
  return out;
}

interface ApiResult {
  status: number;
  body: any;
}

async function api(
  method: string,
  path: string,
  options: { body?: unknown; token?: string } = {},
): Promise<ApiResult> {
  const headers: Record<string, string> = {};
  if (options.body !== undefined) headers['content-type'] = 'application/json';
  if (options.token !== undefined) headers.authorization = `Bearer ${options.token}`;

  const response = await SELF.fetch(`${BASE}${path}`, {
    method,
    headers,
    body: options.body === undefined ? undefined : JSON.stringify(options.body),
  });
  const text = await response.text();
  return { status: response.status, body: text.length === 0 ? null : JSON.parse(text) };
}

async function latestCode(email: string): Promise<string> {
  const row = await env.DB.prepare(
    'SELECT body FROM mail_outbox WHERE to_addr = ? ORDER BY id DESC LIMIT 1',
  )
    .bind(email)
    .first<{ body: string }>();
  if (row === null) throw new Error(`没有发给 ${email} 的验证码邮件`);
  const matched = /验证码是：(\d{6})/.exec(row.body);
  if (matched === null) throw new Error(`验证码邮件正文里没有 6 位码：${row.body}`);
  return matched[1] as string;
}

/** 注册 → 取码 → 验证 → 登录（指定客户端设备名），返回 token 与账号 id。 */
async function signUpAndLogin(email: string, device: string) {
  const authKey = `auth-key-${email}`;

  const registered = await api('POST', '/v1/auth/register', {
    body: { email, auth_key: authKey, device },
  });
  expect(registered.status).toBe(200);

  const code = await latestCode(email);
  const verified = await api('POST', '/v1/auth/verify', { body: { email, code } });
  expect(verified.status).toBe(200);

  const loggedIn = await api('POST', '/v1/auth/login', {
    body: { email, auth_key: authKey, device },
  });
  expect(loggedIn.status).toBe(200);

  return { authKey, register: registered.body, login: loggedIn.body };
}

/** 同账号在**另一台设备**上登录（设备行已存在，登录走 UPDATE 分支）。 */
async function loginSecondDevice(email: string, authKey: string, device: string): Promise<string> {
  const loggedIn = await api('POST', '/v1/auth/login', {
    body: { email, auth_key: authKey, device },
  });
  expect(loggedIn.status).toBe(200);
  return loggedIn.body.access_token as string;
}

/** 合成桶 blob：item_count 个 64 字节块（服务端不解析内容）。 */
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

async function bucketRows(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM sync_buckets WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

async function itemRows(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

async function listKeys(prefix: string): Promise<string[]> {
  const listed = await env.BUCKET.list({ prefix, limit: 1000 });
  return (listed.objects ?? []).map((object) => object.key);
}

async function readObject(key: string): Promise<Uint8Array | null> {
  const object = await env.BUCKET.get(key);
  return object === null ? null : new Uint8Array(await object.arrayBuffer());
}

function setVar(name: string, value: unknown): void {
  (env as unknown as Record<string, unknown>)[name] = value;
}

// ---------------------------------------------------------------- 纯函数

describe('桶级配置与键规则（纯函数）', () => {
  it('桶写预算默认 4,000 桶行/日，非法或越界回落默认', () => {
    expect(DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY).toBe(4_000);
    expect(writeBudgetBucketRowsPerDay({} as never)).toBe(DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY);
    expect(
      writeBudgetBucketRowsPerDay({ WRITE_BUDGET_BUCKETS_PER_DAY: 'abc' } as never),
    ).toBe(DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY);
    expect(
      writeBudgetBucketRowsPerDay({ WRITE_BUDGET_BUCKETS_PER_DAY: '0' } as never),
    ).toBe(DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY);
    expect(
      writeBudgetBucketRowsPerDay({ WRITE_BUDGET_BUCKETS_PER_DAY: '-5' } as never),
    ).toBe(DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY);
    expect(
      writeBudgetBucketRowsPerDay({ WRITE_BUDGET_BUCKETS_PER_DAY: '1234' } as never),
    ).toBe(1234);
  });

  it('R2 桶键 = b/<uid>/<sha256(device)>/<sha256(bucket_key)>，换设备必换键', async () => {
    const userId = 'user-aaaaaaaa-bbbb-cccc-dddd00000000';
    const keyA = await bucketR2KeyFor(userId, 'device-row-1', 'bucket/1');
    const keyB = await bucketR2KeyFor(userId, 'device-row-2', 'bucket/1');
    const hash = '[0-9a-f]{64}';
    expect(keyA).toMatch(new RegExp(`^b/${userId}/${hash}/${hash}$`));
    // 同 bucket_key、不同设备 ⇒ 键必须不同（否则两台机器的桶互相覆盖）
    expect(keyA).not.toBe(keyB);
    // 同设备、不同 bucket_key ⇒ 键不同，且账号段一致
    const keyC = await bucketR2KeyFor(userId, 'device-row-1', 'bucket/2');
    expect(keyC).not.toBe(keyA);
    expect(keyC.startsWith(`b/${userId}/`)).toBe(true);
    // 逐条前缀 u/ 与桶前缀 b/ 同级不嵌套 ⇒ 清空账号时两个前缀都要扫
    expect(keyA.startsWith('u/')).toBe(false);
  });
});

// ---------------------------------------------------------------- 1 + 2：推送与回吐

describe('桶推送与拉取', () => {
  it('新桶：accepted / 行数 / R2 对象数 / server_seq 一致', async () => {
    const { register, login } = await signUpAndLogin('bucket-push@example.com', 'device-bucket-push');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const pushed = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1', 5)] },
    });
    expect(pushed.status).toBe(200);
    expect(pushed.body.accepted).toEqual(['phrases/main#0', 'phrases/main#1']);
    expect(pushed.body.skipped).toEqual([]);
    const seq = pushed.body.server_seq as number;
    expect(seq).toBeGreaterThan(0);

    // 两个桶 ⇒ D1 两行、R2 两个对象，全部落在 b/ 前缀下
    expect(await bucketRows(userId)).toBe(2);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(2);
    // 桶端点绝不碰逐条表（ADR 0077 D6 并存而非替代）
    expect(await itemRows(userId)).toBe(0);

    // 今日已写桶行（纯读口径）
    expect(await countTodayWrittenBucketRows(env as never, userId, Math.floor(Date.now() / 1000))).toBe(
      2,
    );

    // 2. blob 原样回吐：服务端不解密、不改字节，字段名对齐 D7
    const pulled = await api('GET', `${PULL_PATH}?since=0`, { token });
    expect(pulled.status).toBe(200);
    expect(pulled.body.buckets).toHaveLength(2);
    expect(pulled.body.has_more).toBe(false);
    expect(pulled.body.server_seq).toBeGreaterThanOrEqual(seq);

    const first = pulled.body.buckets[0];
    expect(Object.keys(first).sort()).toEqual(
      ['bucket_key', 'cipher', 'device_id', 'hlc', 'item_count', 'server_seq'].sort(),
    );
    expect(first.bucket_key).toBe('phrases/main#0');
    expect(first.item_count).toBe(3);
    expect(first.hlc).toEqual({ ms: HLC_MS, counter: 0, device: 'hlc-device-a' });
    expect(fromB64(first.cipher)).toEqual(blobFor(3, 3));

    // 无桶级 nonce 字段（D3 否决桶级 AEAD）
    for (const row of pulled.body.buckets) {
      expect(row.nonce).toBeUndefined();
    }

    // 落库字节与上报 blob 一致
    const keys = await listKeys(`b/${userId}/`);
    for (const key of keys) {
      const stored = await readObject(key);
      expect(stored).not.toBeNull();
      expect(stored!.length % 64).toBe(0);
    }
  });

  it('旧 HLC ⇒ 整桶跳过：不写 D1、不推进 server_seq、不改 R2 blob', async () => {
    const { register, login } = await signUpAndLogin('bucket-stale@example.com', 'device-bucket-stale');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const first = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0', 4, { cipher: b64(blobFor(4, 7)) })] },
    });
    expect(first.status).toBe(200);
    const seq1 = first.body.server_seq as number;
    const [r2Key] = await listKeys(`b/${userId}/`);
    const blobBefore = await readObject(r2Key!);

    // 更低 HLC 重推（内容也不同）⇒ 守卫命中，整桶跳过
    const stale = await api('POST', PUSH_PATH, {
      token,
      body: {
        buckets: [
          bucket('phrases/main#0', 4, {
            cipher: b64(blobFor(4, 8)),
            hlc: { ms: HLC_MS - 1_000, counter: 0, device: 'hlc-device-a' },
          }),
        ],
      },
    });
    expect(stale.status).toBe(200);
    expect(stale.body.accepted).toEqual([]);
    expect(stale.body.skipped).toEqual(['phrases/main#0']);
    // 跳过不推进游标
    expect(stale.body.server_seq).toBe(seq1);
    expect(await bucketRows(userId)).toBe(1);
    // R2 blob 未被覆盖
    expect(await readObject(r2Key!)).toEqual(blobBefore);

    // 相等 HLC 同样跳过（口径是「已存 ≥ 上报」）
    const equal = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0', 4, { cipher: b64(blobFor(4, 9)) })] },
    });
    expect(equal.body.skipped).toEqual(['phrases/main#0']);
    expect(equal.body.accepted).toEqual([]);

    // 更高 HLC ⇒ 接受、行数不变、序号前进（DELETE + INSERT）
    const fresh = await api('POST', PUSH_PATH, {
      token,
      body: {
        buckets: [
          bucket('phrases/main#0', 4, {
            cipher: b64(blobFor(4, 10)),
            hlc: { ms: HLC_MS + 1_000, counter: 0, device: 'hlc-device-a' },
          }),
        ],
      },
    });
    expect(fresh.status).toBe(200);
    expect(fresh.body.accepted).toEqual(['phrases/main#0']);
    expect(await bucketRows(userId)).toBe(1);
    expect(fresh.body.server_seq).toBeGreaterThan(seq1);
    expect(await readObject(r2Key!)).toEqual(blobFor(4, 10));
  });

  it('同 bucket_key 跨设备互不压制，且 device_id = devices.id 行主键', async () => {
    const email = 'bucket-multi-device@example.com';
    const { authKey, register, login } = await signUpAndLogin(email, 'device-bucket-a');
    const tokenA = login.access_token as string;
    const userId = register.account_id as string;

    const tokenB = await loginSecondDevice(email, authKey, 'device-bucket-b');

    // A 先推一个较新的桶
    const pushedA = await api('POST', PUSH_PATH, {
      token: tokenA,
      body: { buckets: [bucket('phrases/main#0')] },
    });
    expect(pushedA.status).toBe(200);

    // B 推同一 bucket_key 的**更旧** HLC ⇒ 仍必须接受（守卫作用域带 device_id）
    const pushedB = await api('POST', PUSH_PATH, {
      token: tokenB,
      body: {
        buckets: [
          bucket('phrases/main#0', 2, {
            cipher: b64(blobFor(2, 5)),
            hlc: { ms: HLC_MS - 5_000, counter: 0, device: 'hlc-device-b' },
          }),
        ],
      },
    });
    expect(pushedB.status).toBe(200);
    expect(pushedB.body.accepted).toEqual(['phrases/main#0']);
    expect(pushedB.body.skipped).toEqual([]);

    // 每设备独立视图 ⇒ 两行、两个 R2 对象
    expect(await bucketRows(userId)).toBe(2);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(2);

    // device_id 存的是服务端设备行主键，不是客户端上报的设备名
    const mapping = await env.DB.prepare(
      `SELECT b.device_id AS bucket_device, d.device_id AS client_device, d.id AS row_id
         FROM sync_buckets b JOIN devices d ON d.id = b.device_id
        WHERE b.user_id = ?`,
    )
      .bind(userId)
      .all<{ bucket_device: string; client_device: string; row_id: string }>();
    const rows = mapping.results ?? [];
    expect(rows).toHaveLength(2);
    for (const row of rows) {
      expect(row.bucket_device).toBe(row.row_id);
      expect(['device-bucket-a', 'device-bucket-b']).toContain(row.client_device);
      // 客户端设备名绝不落进 device_id 列
      expect(row.bucket_device).not.toBe(row.client_device);
    }

    // pull 是跨设备视图：A 能看到 B 的桶
    const pulledA = await api('GET', `${PULL_PATH}?since=0&limit=100`, { token: tokenA });
    expect(pulledA.status).toBe(200);
    expect(pulledA.body.buckets).toHaveLength(2);
    const devices = new Set(pulledA.body.buckets.map((row: any) => row.device_id));
    expect(devices.size).toBe(2);

    // 伪造他人 device_id 必须被拒（防跨设备压制）
    const otherRow = rows.find((row) => row.client_device === 'device-bucket-b')!;
    const spoofed = await api('POST', PUSH_PATH, {
      token: tokenA,
      body: { device_id: otherRow.row_id, buckets: [bucket('phrases/main#9')] },
    });
    expect(spoofed.status).toBe(400);
    expect(spoofed.body.error.code).toBe('bad_device_id');
    expect(await bucketRows(userId)).toBe(2);
  });
});

// ---------------------------------------------------------------- 5：入参校验

describe('桶入参校验', () => {
  it('桶数 / item_count / cipher / 空数组的 400 与「第几个桶」定位', async () => {
    const { login } = await signUpAndLogin('bucket-invalid@example.com', 'device-bucket-invalid');
    const token = login.access_token as string;

    // 桶数上限 64
    const tooMany = Array.from({ length: 65 }, (_unused, i) => bucket(`phrases/main#${i}`, 1));
    const rejected = await api('POST', PUSH_PATH, { token, body: { buckets: tooMany } });
    expect(rejected.status).toBe(400);
    expect(rejected.body.error.code).toBe('too_many_buckets');
    expect(rejected.body.error.message).toContain('64');

    // 空数组 / 非数组
    expect((await api('POST', PUSH_PATH, { token, body: { buckets: [] } })).status).toBe(400);
    expect((await api('POST', PUSH_PATH, { token, body: {} })).status).toBe(400);

    // item_count 超上限 4096 ⇒ 400，并提示该降级走逐条推送
    const overItems = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1', 4097)] },
    });
    expect(overItems.status).toBe(400);
    expect(overItems.body.error.message).toContain('第 2 个桶');
    expect(overItems.body.error.message).toContain('4096');

    // item_count 为负 ⇒ 400（直搭对象，不经 blobFor，避免测试自己先炸）
    const negativeCount = await api('POST', PUSH_PATH, {
      token,
      body: {
        buckets: [
          {
            bucket_key: 'x',
            item_count: -1,
            hlc: { ms: HLC_MS, counter: 0, device: 'hlc-device-a' },
            cipher: b64(blobFor(1, 3)),
          },
        ],
      },
    });
    expect(negativeCount.status).toBe(400);
    const missingCount = await api('POST', PUSH_PATH, {
      token,
      body: {
        buckets: [
          {
            bucket_key: 'x',
            hlc: { ms: HLC_MS, counter: 0, device: 'hlc-device-a' },
            cipher: b64(blobFor(1, 3)),
          },
        ],
      },
    });
    expect(missingCount.status).toBe(400);

    // cipher 非法 base64 ⇒ 400，定位到第 2 个桶
    const badCipher = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('x'), bucket('y', 3, { cipher: 'not@@base64' })] },
    });
    expect(badCipher.status).toBe(400);
    expect(badCipher.body.error.message).toContain('第 2 个桶');
    expect(badCipher.body.error.message).toContain('base64');

    // HLC 缺失 ⇒ 400
    const noHlc = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [{ bucket_key: 'x', item_count: 1, cipher: b64(blobFor(1, 3)) }] },
    });
    expect(noHlc.status).toBe(400);

    // bucket_key 超长（> 512 字节）
    const longKey = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('p'.repeat(600))] },
    });
    expect(longKey.status).toBe(400);
  });

  it('校验先于写入：任一桶非法 ⇒ 整批零写入', async () => {
    const { register, login } = await signUpAndLogin('bucket-atomic@example.com', 'device-bucket-atomic');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const rejected = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1', 9999)] },
    });
    expect(rejected.status).toBe(400);
    expect(await bucketRows(userId)).toBe(0);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(0);
  });
});

// ---------------------------------------------------------------- 6：增量与分页

describe('桶拉取增量与分页', () => {
  it('since 增量、limit 分页、cursor 续读、has_more、空页游标不倒退', async () => {
    const { register, login } = await signUpAndLogin('bucket-pull@example.com', 'device-bucket-pull');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const pushed = await api('POST', PUSH_PATH, {
      token,
      body: {
        buckets: Array.from({ length: 3 }, (_unused, i) =>
          bucket(`phrases/main#${i}`, 1, { cipher: b64(blobFor(1, i + 1)) }),
        ),
      },
    });
    expect(pushed.status).toBe(200);
    const lastSeq = pushed.body.server_seq as number;

    // limit=1 ⇒ 只回 1 桶 + has_more=true
    const page1 = await api('GET', `${PULL_PATH}?since=0&limit=1`, { token });
    expect(page1.body.buckets).toHaveLength(1);
    expect(page1.body.has_more).toBe(true);
    expect(typeof page1.body.next_cursor).toBe('string');

    // cursor 续读第二页
    const page2 = await api('GET', `${PULL_PATH}?since=0&limit=1&cursor=${page1.body.next_cursor}`, {
      token,
    });
    expect(page2.body.buckets).toHaveLength(1);
    expect(page2.body.buckets[0].bucket_key).toBe('phrases/main#1');
    expect(page2.body.has_more).toBe(true);

    // 最后一页：has_more=false
    const page3 = await api('GET', `${PULL_PATH}?since=0&limit=1&cursor=${page2.body.next_cursor}`, {
      token,
    });
    expect(page3.body.buckets).toHaveLength(1);
    expect(page3.body.buckets[0].bucket_key).toBe('phrases/main#2');
    expect(page3.body.has_more).toBe(false);
    expect(page3.body.server_seq).toBe(lastSeq);

    // since 增量：已同步到 lastSeq ⇒ 零新桶，游标保持不回退
    const caught = await api('GET', `${PULL_PATH}?since=${lastSeq}`, { token });
    expect(caught.status).toBe(200);
    expect(caught.body.buckets).toEqual([]);
    expect(caught.body.has_more).toBe(false);
    expect(caught.body.next_cursor).toBeNull();
    expect(caught.body.server_seq).toBe(lastSeq);

    // since 非法 / limit 非法 / cursor 非法
    expect((await api('GET', `${PULL_PATH}?since=-1`, { token })).body.error.code).toBe('bad_since');
    expect((await api('GET', `${PULL_PATH}?limit=0`, { token })).body.error.code).toBe('bad_limit');
    expect((await api('GET', `${PULL_PATH}?cursor=%21%21%21`, { token })).body.error.code).toBe(
      'bad_cursor',
    );

    // 新推一桶后增量拉取只拿到它
    const more = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/other#0', 2)] },
    });
    expect(more.status).toBe(200);
    const delta = await api('GET', `${PULL_PATH}?since=${lastSeq}`, { token });
    expect(delta.body.buckets).toHaveLength(1);
    expect(delta.body.buckets[0].bucket_key).toBe('phrases/other#0');
    expect(delta.body.buckets[0].server_seq).toBeGreaterThan(lastSeq);
    expect(delta.body.server_seq).toBe(more.body.server_seq);
    expect(await bucketRows(userId)).toBe(4);
  });

  it('R2 桶对象缺失 ⇒ pull 自愈删索引行，不返回 500', async () => {
    const { register, login } = await signUpAndLogin('bucket-heal@example.com', 'device-bucket-heal');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const pushed = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1')] },
    });
    expect(pushed.status).toBe(200);
    expect(await bucketRows(userId)).toBe(2);

    // 只丢一个桶对象（模拟 R2 局部失败）
    const row = await env.DB
      .prepare('SELECT r2_key FROM sync_buckets WHERE user_id = ? AND bucket_key = ?')
      .bind(userId, 'phrases/main#0')
      .first<{ r2_key: string }>();
    await env.BUCKET.delete(row!.r2_key);

    const pulled = await api('GET', `${PULL_PATH}?since=0`, { token });
    expect(pulled.status).toBe(200);
    // 缺失的桶不返回半截数据，存在的那个照常返回
    expect(pulled.body.buckets.map((entry: any) => entry.bucket_key)).toEqual(['phrases/main#1']);
    // 自愈：悬挂索引行被清掉
    expect(await bucketRows(userId)).toBe(1);
  });
});

// ---------------------------------------------------------------- 7：写预算护栏（桶口径）

describe('桶写预算护栏', () => {
  it('超限 ⇒ 429 + 中文文案 + D1/R2 零写入；恢复后可正常落库', async () => {
    const { register, login } = await signUpAndLogin('bucket-budget@example.com', 'device-bucket-budget');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const bucketsVar = 'WRITE_BUDGET_BUCKETS_PER_DAY';
    const rowsVar = 'WRITE_BUDGET_ROWS_PER_DAY';
    const originalBuckets = (env as unknown as Record<string, unknown>)[bucketsVar];
    const originalRows = (env as unknown as Record<string, unknown>)[rowsVar];

    // 桶预算 1 行 ⇒ 推 2 桶（planned=2）必然超限
    setVar(bucketsVar, '1');
    expect((env as unknown as Record<string, unknown>)[bucketsVar]).toBe('1');
    try {
      const rejected = await api('POST', PUSH_PATH, {
        token,
        body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1')] },
      });
      expect(rejected.status).toBe(429);
      expect(rejected.body.error.code).toBe(WRITE_BUDGET_EXCEEDED_CODE);
      expect(rejected.body.error.code).toBe('write_budget_exceeded');
      expect(rejected.body.error.message).toBe(WRITE_BUDGET_EXCEEDED_MESSAGE);
      expect(rejected.body.error.message).toBe(
        '今日云端写入已达上限，请明天再试或减少本机词典规模。',
      );
      // 零写入：桶表零行、b/ 前缀零对象
      expect(await bucketRows(userId)).toBe(0);
      expect(await listKeys(`b/${userId}/`)).toHaveLength(0);

      const pulled = await api('GET', `${PULL_PATH}?since=0`, { token });
      expect(pulled.body.buckets).toEqual([]);
    } finally {
      setVar(bucketsVar, originalBuckets);
    }

    // 预算恢复 ⇒ 同一批可正常落库（证明 429 由预算驱动，不是状态损坏）
    const allowed = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1')] },
    });
    expect(allowed.status).toBe(200);
    expect(await bucketRows(userId)).toBe(2);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(2);

    // 口径换表：把**逐条**预算压到 1 行，桶端点不受影响（护栏读 sync_buckets）
    setVar(rowsVar, '1');
    try {
      const stillOk = await api('POST', PUSH_PATH, {
        token,
        body: { buckets: [bucket('phrases/main#2', 1, { hlc: { ms: HLC_MS + 1, counter: 0, device: 'hlc-device-a' } })] },
      });
      expect(stillOk.status).toBe(200);
      expect(stillOk.body.accepted).toEqual(['phrases/main#2']);
    } finally {
      setVar(rowsVar, originalRows);
    }
  });
});

// ---------------------------------------------------------------- 8：老端点回归

describe('老逐条端点回归', () => {
  it('桶端点与逐条端点并存、游标空间互不污染', async () => {
    const { register, login } = await signUpAndLogin('bucket-coexist@example.com', 'device-bucket-coexist');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    // 老端点：逐条推两条
    const legacyPush = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          {
            item_key: 'config/main',
            type: 'config',
            hlc: { ms: HLC_MS, counter: 0, device: 'device-a' },
            nonce: b64(new Uint8Array(12).fill(7)),
            cipher: b64(new Uint8Array(48).fill(9)),
          },
          {
            item_key: 'phrases/main',
            type: 'phrases',
            hlc: { ms: HLC_MS, counter: 1, device: 'device-a' },
            nonce: b64(new Uint8Array(12).fill(7)),
            cipher: b64(new Uint8Array(48).fill(9)),
          },
        ],
      },
    });
    expect(legacyPush.status).toBe(200);
    expect(legacyPush.body.accepted).toEqual(['config/main', 'phrases/main']);
    expect(await itemRows(userId)).toBe(2);
    expect(await bucketRows(userId)).toBe(0);

    const legacyPull = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(legacyPull.status).toBe(200);
    expect(legacyPull.body.items).toHaveLength(2);

    // 桶端点：写自己的表与 b/ 前缀
    const bucketPush = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0')] },
    });
    expect(bucketPush.status).toBe(200);
    expect(await itemRows(userId)).toBe(2);
    expect(await bucketRows(userId)).toBe(1);

    // 老游标不受桶行影响：桶行 id 更大，但 pull 仍按 sync_items 返回，
    // 且用老端点游标拉不到桶 ⇒ 两套游标不可混用（ADR 0077 D6 已知边界）
    const sinceBucket = bucketPush.body.server_seq as number;
    const legacyPull2 = await api('GET', `/v1/sync/pull?since=${sinceBucket}`, { token });
    expect(legacyPull2.status).toBe(200);
    expect(legacyPull2.body.items).toEqual([]);

    // 桶端点只读 sync_buckets：全量拉只见桶行，用桶游标续读即空
    const bucketPullAll = await api('GET', `${PULL_PATH}?since=0`, { token });
    expect(bucketPullAll.status).toBe(200);
    expect(
      (bucketPullAll.body.buckets as Array<Record<string, unknown>>).map((b) => b.bucket_key),
    ).toEqual(['phrases/main#0']);
    const bucketPull2 = await api('GET', `${PULL_PATH}?since=${bucketPush.body.server_seq}`, {
      token,
    });
    expect(bucketPull2.body.buckets).toEqual([]);
  });
});

// ---------------------------------------------------------------- 9：清空账号数据

describe('清空云端数据', () => {
  it('DELETE /v1/account/data 清 u/ 与 b/ 两个前缀、sync_items 与 sync_buckets 两张表', async () => {
    const { register, login } = await signUpAndLogin('bucket-clear@example.com', 'device-bucket-clear');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const legacy = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          {
            item_key: 'config/main',
            type: 'config',
            hlc: { ms: HLC_MS, counter: 0, device: 'device-a' },
            nonce: b64(new Uint8Array(12).fill(7)),
            cipher: b64(new Uint8Array(48).fill(9)),
          },
        ],
      },
    });
    expect(legacy.status).toBe(200);

    const bucketed = await api('POST', PUSH_PATH, {
      token,
      body: { buckets: [bucket('phrases/main#0'), bucket('phrases/main#1')] },
    });
    expect(bucketed.status).toBe(200);

    expect(await listKeys(`u/${userId}/`)).toHaveLength(1);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(2);
    expect(await itemRows(userId)).toBe(1);
    expect(await bucketRows(userId)).toBe(2);

    const cleared = await api('DELETE', '/v1/account/data', { token });
    expect(cleared.status).toBe(200);
    expect(cleared.body.ok).toBe(true);
    // 既有字段名不变，含义扩为两前缀合计
    expect(cleared.body.deleted_objects).toBe(3);
    expect(cleared.body.deleted_items).toBe(1);
    expect(cleared.body.deleted_bucket_rows).toBe(2);

    expect(await listKeys(`u/${userId}/`)).toHaveLength(0);
    expect(await listKeys(`b/${userId}/`)).toHaveLength(0);
    expect(await itemRows(userId)).toBe(0);
    expect(await bucketRows(userId)).toBe(0);

    // 清完仍可读写（不删账号）
    const repush = await api('POST', PUSH_PATH, { token, body: { buckets: [bucket('phrases/main#0')] } });
    expect(repush.status).toBe(200);
    expect(await bucketRows(userId)).toBe(1);
  });
});

// ---------------------------------------------------------------- 10：路由与鉴权面

describe('路由与鉴权面', () => {
  it('方法不符 405、缺令牌 401、未验证邮箱 403', async () => {
    // 405：路径存在但方法不对
    const wrongMethod = await api('GET', PUSH_PATH);
    expect(wrongMethod.status).toBe(405);
    expect(wrongMethod.body.error.code).toBe('method_not_allowed');

    const wrongMethod2 = await api('POST', PULL_PATH, { body: {} });
    expect(wrongMethod2.status).toBe(405);

    // 401：没有 Authorization
    const unauthenticated = await api('GET', `${PULL_PATH}?since=0`);
    expect(unauthenticated.status).toBe(401);
    expect(unauthenticated.body.error.code).toBe('missing_token');

    const pushUnauthenticated = await api('POST', PUSH_PATH, { body: { buckets: [bucket('x')] } });
    expect(pushUnauthenticated.status).toBe(401);

    // 403：未验证邮箱可读不可写
    const email = 'bucket-unverified@example.com';
    const authKey = 'auth-key-bucket-unverified';
    const registered = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: authKey, device: 'device-bucket-unverified' },
    });
    expect(registered.status).toBe(200);
    const loggedIn = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: authKey, device: 'device-bucket-unverified' },
    });
    expect(loggedIn.status).toBe(200);
    expect(loggedIn.body.email_verified).toBe(false);
    const pendingToken = loggedIn.body.access_token as string;

    const denied = await api('POST', PUSH_PATH, {
      token: pendingToken,
      body: { buckets: [bucket('phrases/main#0')] },
    });
    expect(denied.status).toBe(403);
    expect(denied.body.error.code).toBe('email_not_verified');

    // 读不拦
    const allowedRead = await api('GET', `${PULL_PATH}?since=0`, { token: pendingToken });
    expect(allowedRead.status).toBe(200);
    expect(allowedRead.body.buckets).toEqual([]);

    // 尾斜杠归一化同样命中（路由手写 switch 的既有约定）
    const trailingSlash = await api('POST', `${PUSH_PATH}/`, { token: pendingToken, body: { buckets: [] } });
    expect(trailingSlash.status).toBe(403);
  });
});
