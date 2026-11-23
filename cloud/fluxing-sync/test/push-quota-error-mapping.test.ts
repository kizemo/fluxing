// 推送写路径的 D1 配额错误映射单测（ADR 0076 D0-2 补强 · 推送侧）。
//
// 背景（2026-11-23 批次 BR 生产实测坐实）：D1 免费额度耗尽时，Worker 内 D1 绑定抛出的
// 错误文本**不带** `[code: 7500]` ⇒ 旧判据（只认 REST API 形态）命中不了 ⇒ 推送这类
// 写路径对外返回笼统 500 internal_error ⇒ 客户端与运维都会误判成代码缺陷，
// 而真实原因只是「UTC 午夜自动重置」的暂时性平台故障。
//
// 本文件锁住的是**推送写路径本身**（不是错误映射函数的单元测试 —— 那是
// test/d1-quota-error-mapping.test.ts 的职责）：
//   1. 真请求路径（SELF.fetch → src/index.ts 顶层 catch → toErrorResponse）在 D1 写阶段
//      （env.DB.batch）被配额错误打断时，对外是 503 storage_quota_exceeded + 中文文案；
//   2. D1 零落库（batch 抛错 ⇒ sync_items 一行未增），拉取也看不到任何条目；
//   3. 无关错误仍然报 500 internal_error（不误伤）；
//   4. 故障是**一次性**的：恢复后同一条推送可正常 200（错误不是粘性的）。
//
// 注意：isolatedStorage 默认开启 ⇒ 多步流程必须写在同一个 it() 里。

import { SELF, env } from 'cloudflare:test';
import { describe, expect, it, vi } from 'vitest';

const BASE = 'https://fluxing-sync.test';

/** 12 字节 nonce（对齐 FluxingCloudEnvelope.h 的 kNonceBytes） */
const NONCE = b64(new Uint8Array(12).fill(7));
/** 48 字节密文（≥ 16 字节认证标签即可通过校验） */
const CIPHER = b64(new Uint8Array(48).fill(9));

/** REST API 形态（wrangler d1 execute / Cloudflare API）：文本尾部带 [code: 7500]。 */
const REST_QUOTA_ERROR =
  "Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details. [code: 7500]";

/** Worker 内 D1 绑定形态：D1_ERROR 前缀 + 同一句话，但**没有** [code: 7500]。 */
const WORKER_QUOTA_ERROR =
  "D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details.";

function b64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
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

/** 从 outbox 里取最近一封验证码邮件中的 6 位码（测试专用通道，生产不存在）。 */
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

function item(itemKey: string, overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    item_key: itemKey,
    type: 'config',
    hlc: { ms: 1_700_000_000_000, counter: 0, device: 'device-a' },
    nonce: NONCE,
    cipher: CIPHER,
    ...overrides,
  };
}

/** 注册 → 取码 → 验证 → 登录，返回后续用例要用的句柄。 */
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

  return { authKey, code, register: registered.body, login: loggedIn.body };
}

/** 该用户在 sync_items 里的行数。 */
async function syncItemRows(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

/**
 * 让 D1 的批量写阶段（推送落库的唯一写入口）抛出指定错误。
 *
 * 只打断 batch ⇒ 鉴权与读路径仍走真实本地 D1 ⇒ 测的是「推送写阶段失败」这条真路径。
 * 返回还原函数（必须在 finally 里调用，否则会污染后续用例）。
 */
function breakD1Batch(error: unknown): () => void {
  const db = env.DB as unknown as { batch: (statements: readonly unknown[]) => Promise<unknown> };
  const spy = vi.spyOn(db, 'batch').mockRejectedValue(error as never);
  return () => spy.mockRestore();
}

describe('推送写路径遇 D1 配额耗尽 ⇒ 503 storage_quota_exceeded', () => {
  it('Worker 内绑定形态（无 [code: 7500]）⇒ 503 + 中文文案 + D1 零落库', async () => {
    const { register, login } = await signUpAndLogin('push-quota-worker@example.com', 'device-pq-w');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const restore = breakD1Batch(new Error(WORKER_QUOTA_ERROR));
    let rejected: ApiResult;
    try {
      rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main')] },
      });
    } finally {
      restore();
    }

    expect(rejected!.status).toBe(503);
    expect(rejected!.body.error.code).toBe('storage_quota_exceeded');
    expect(rejected!.body.error.message).toBe('服务端存储写入额度已用尽，请稍后重试。');

    // D1 零落库：batch 抛错 ⇒ 一行都没写进去
    expect(await syncItemRows(userId)).toBe(0);
    // 拉取也看不到任何条目（索引行不存在 ⇒ 客户端不会拿到半截数据）
    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.status).toBe(200);
    expect(pulled.body.items).toEqual([]);
  });

  it('REST 形态（带 [code: 7500]）⇒ 同样是 503', async () => {
    const { register, login } = await signUpAndLogin('push-quota-rest@example.com', 'device-pq-r');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const restore = breakD1Batch(new Error(REST_QUOTA_ERROR));
    let rejected: ApiResult;
    try {
      rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main')] },
      });
    } finally {
      restore();
    }

    expect(rejected!.status).toBe(503);
    expect(rejected!.body.error.code).toBe('storage_quota_exceeded');
    expect(await syncItemRows(userId)).toBe(0);
  });

  it('非 Error 实例（普通对象）⇒ 同样是 503', async () => {
    const { login } = await signUpAndLogin('push-quota-obj@example.com', 'device-pq-o');
    const token = login.access_token as string;

    const restore = breakD1Batch({ message: WORKER_QUOTA_ERROR });
    let rejected: ApiResult;
    try {
      rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main')] },
      });
    } finally {
      restore();
    }

    expect(rejected!.status).toBe(503);
    expect(rejected!.body.error.code).toBe('storage_quota_exceeded');
  });

  it('无关错误 ⇒ 仍报 500 internal_error（不误伤成配额错误）', async () => {
    const { login } = await signUpAndLogin('push-quota-other@example.com', 'device-pq-x');
    const token = login.access_token as string;

    const restore = breakD1Batch(new Error('boom: unrelated failure'));
    let rejected: ApiResult;
    try {
      rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main')] },
      });
    } finally {
      restore();
    }

    expect(rejected!.status).toBe(500);
    expect(rejected!.body.error.code).toBe('internal_error');
  });

  it('故障不是粘性的：恢复后同一条推送正常 200 并落库', async () => {
    const { register, login } = await signUpAndLogin('push-quota-recover@example.com', 'device-pq-c');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const restore = breakD1Batch(new Error(WORKER_QUOTA_ERROR));
    try {
      const rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main')] },
      });
      expect(rejected.status).toBe(503);
    } finally {
      restore();
    }

    // 配额恢复（UTC 午夜自动重置）⇒ 同一条推送成功
    const allowed = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main')] },
    });
    expect(allowed.status).toBe(200);
    expect(allowed.body.accepted).toEqual(['config/main']);
    expect(await syncItemRows(userId)).toBe(1);
  });
});
