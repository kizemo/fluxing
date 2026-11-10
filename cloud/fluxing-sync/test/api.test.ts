// 接口集成测试：跑在 workerd 里（vitest-pool-workers），D1 / R2 / crypto.subtle 都是真实实现。
//
// 覆盖 plan §6.1 的 9 条端点 + GET /v1/health，重点压不变量而不是罗列字段：
//   1. 服务端只见密文：push 的 cipher 不透明，pull 逐字节原样回传。
//   2. 未验证邮箱只能读、不能写（T101 结论）。
//   3. server_seq 单调前进：同一 item_key 重推后序号必须变大。
//   4. 令牌轮换 + 撤销设备即时失效。
//   5. 清空云端数据不删账号。
//
// 注意：isolatedStorage 默认开启 ⇒ 每个 it() 拿到独立的存储，多步流程必须写在同一个 it() 里。

import { SELF, env } from 'cloudflare:test';
import { describe, expect, it } from 'vitest';

const BASE = 'https://fluxing-sync.test';

/** 12 字节 nonce（对齐 FluxingCloudEnvelope.h 的 kNonceBytes） */
const NONCE = b64(new Uint8Array(12).fill(7));
/** 48 字节密文（≥ 16 字节认证标签即可通过校验） */
const CIPHER = b64(new Uint8Array(48).fill(9));

function b64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

interface ApiResult {
  status: number;
  // 响应体形状由端点决定（对象 / 数组），测试里直接取字段
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

/**
 * 把该邮箱已发的验证码记录往前挪，绕开 60 秒重发闸（测试专用夹具）。
 * 不这么做的话「注册 → 立刻重发」必然撞 429，测不到正常重发那条路径。
 */
async function backdateVerifications(email: string, seconds: number): Promise<void> {
  await env.DB.prepare(
    'UPDATE email_verifications SET created_at = created_at - ? WHERE email = ?',
  )
    .bind(seconds, email)
    .run();
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

// ---------------------------------------------------------------- 巡活

describe('GET /v1/health', () => {
  it('返回服务标识与当前环境', async () => {
    const result = await api('GET', '/v1/health');
    expect(result.status).toBe(200);
    expect(result.body).toMatchObject({
      ok: true,
      service: 'fluxing-sync',
      environment: 'test',
    });
    expect(typeof result.body.time).toBe('number');
  });

  it('方法不符返回 405', async () => {
    const result = await api('POST', '/v1/health');
    expect(result.status).toBe(405);
    expect(result.body.error.code).toBe('method_not_allowed');
  });
});

// ---------------------------------------------------------------- 认证

describe('认证', () => {
  it('未注册邮箱登录返回 401，且不透露账号是否存在', async () => {
    const result = await api('POST', '/v1/auth/login', {
      body: { email: 'nobody@example.com', auth_key: 'whatever', device: 'device-x' },
    });
    expect(result.status).toBe(401);
    expect(result.body.error.code).toBe('invalid_credentials');
    expect(result.body.error.message).not.toContain('不存在');
  });

  it('注册返回 salt / kdf，且标记需要邮箱验证', async () => {
    const email = 'signup@example.com';
    const result = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: 'auth-key-signup', device: 'device-signup' },
    });
    expect(result.status).toBe(200);
    expect(result.body.need_email_verify).toBe(true);
    expect(typeof result.body.account_id).toBe('string');
    expect(typeof result.body.salt).toBe('string');
    // kdf 参数由服务端下发，客户端不得自行假设
    expect(result.body.kdf).toEqual({ algo: 'PBKDF2-SHA256', iters: 600000 });
  });

  it('同邮箱重复注册被 60 秒重发节流挡住（429 而非 409）', async () => {
    const email = 'duplicate@example.com';
    const body = { email, auth_key: 'auth-key-duplicate', device: 'device-dup' };
    const first = await api('POST', '/v1/auth/register', { body });
    expect(first.status).toBe(200);

    // 顺序上先查重发窗口、再查 users ⇒ 60 秒内重复注册拿到的是 429
    const second = await api('POST', '/v1/auth/register', { body });
    expect(second.status).toBe(429);
    expect(second.body.error.code).toBe('verify_too_frequent');
  });

  it('验证码错误返回 400，且账号仍未验证', async () => {
    const email = 'wrongcode@example.com';
    const authKey = 'auth-key-wrongcode';
    await api('POST', '/v1/auth/register', {
      body: { email, auth_key: authKey, device: 'device-wrongcode' },
    });

    const code = await latestCode(email);
    const wrong = code === '000000' ? '111111' : '000000';
    const mismatch = await api('POST', '/v1/auth/verify', { body: { email, code: wrong } });
    expect(mismatch.status).toBe(400);
    expect(mismatch.body.error.code).toBe('verify_code_mismatch');

    const login = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: authKey, device: 'device-wrongcode' },
    });
    expect(login.status).toBe(200);
    expect(login.body.email_verified).toBe(false);
  });

  it('未验证邮箱：可登录、可读，但推送被 403 拦住', async () => {
    const email = 'pending@example.com';
    const authKey = 'auth-key-pending';
    const registered = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: authKey, device: 'device-pending' },
    });
    expect(registered.status).toBe(200);

    const login = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: authKey, device: 'device-pending' },
    });
    expect(login.status).toBe(200);
    expect(login.body.email_verified).toBe(false);
    const token = login.body.access_token as string;

    const push = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main')] },
    });
    expect(push.status).toBe(403);
    expect(push.body.error.code).toBe('email_not_verified');

    // 读与设备管理不拦
    const pull = await api('GET', '/v1/sync/pull', { token });
    expect(pull.status).toBe(200);
    expect(pull.body.items).toEqual([]);

    const devices = await api('GET', '/v1/devices', { token });
    expect(devices.status).toBe(200);
    expect(devices.body).toHaveLength(1);
  });

  it('刷新令牌轮换：旧的立即失效，新 access 可用', async () => {
    const { login } = await signUpAndLogin('rotate@example.com', 'device-rotate');

    const refreshed = await api('POST', '/v1/auth/refresh', {
      body: { refresh_token: login.refresh_token },
    });
    expect(refreshed.status).toBe(200);
    expect(refreshed.body.expires_in).toBe(3600);
    expect(refreshed.body.access_token).not.toBe(login.access_token);
    expect(refreshed.body.refresh_token).not.toBe(login.refresh_token);

    const reuse = await api('POST', '/v1/auth/refresh', {
      body: { refresh_token: login.refresh_token },
    });
    expect(reuse.status).toBe(401);
    expect(reuse.body.error.code).toBe('refresh_token_revoked');

    const pullWithNew = await api('GET', '/v1/sync/pull', {
      token: refreshed.body.access_token,
    });
    expect(pullWithNew.status).toBe(200);

    // access token 不能当 refresh 用
    const misuse = await api('POST', '/v1/auth/refresh', {
      body: { refresh_token: refreshed.body.access_token },
    });
    expect(misuse.status).toBe(401);
    expect(misuse.body.error.code).toBe('invalid_refresh_token');
  });
});

// ---------------------------------------------------------------- 验证码重发
//
// 恢复路径：注册后验证码过期（TTL 600 秒）或 5 次错误用尽时，再调 register 会撞
// 409 email_taken ⇒ 用户被锁死。POST /v1/auth/resend 提供重新获取的出口。

describe('验证码重发', () => {
  it('未注册邮箱重发返回 404 email_not_found', async () => {
    const result = await api('POST', '/v1/auth/resend', {
      body: { email: 'ghost@example.com' },
    });
    expect(result.status).toBe(404);
    expect(result.body.error.code).toBe('email_not_found');
    expect(result.body.error.message).toBe('该邮箱尚未注册，请先注册。');
  });

  it('已验证邮箱重发返回 409 email_already_verified', async () => {
    const email = 'verified@example.com';
    await signUpAndLogin(email, 'device-verified');

    const result = await api('POST', '/v1/auth/resend', { body: { email } });
    expect(result.status).toBe(409);
    expect(result.body.error.code).toBe('email_already_verified');
    expect(result.body.error.message).toBe('该邮箱已完成验证，无需重新获取验证码。');
  });

  it('未验证邮箱重发返回 200，且验证码与邮件都落库', async () => {
    const email = 'resend@example.com';
    const registered = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: 'auth-key-resend', device: 'device-resend' },
    });
    expect(registered.status).toBe(200);

    // 注册刚发过一封 ⇒ 60 秒频率闸会拦；把已发记录往前挪，单独验证「正常重发」这条路径
    await backdateVerifications(email, 120);

    const result = await api('POST', '/v1/auth/resend', { body: { email } });
    expect(result.status).toBe(200);
    expect(result.body.ok).toBe(true);

    const verifications = await env.DB.prepare(
      'SELECT COUNT(*) AS n FROM email_verifications WHERE email = ?',
    )
      .bind(email)
      .first<{ n: number }>();
    // 注册一条 + 重发一条
    expect(verifications?.n).toBe(2);

    const mails = await env.DB.prepare(
      'SELECT COUNT(*) AS n FROM mail_outbox WHERE to_addr = ?',
    )
      .bind(email)
      .first<{ n: number }>();
    expect(mails?.n).toBe(2);
  });

  it('60 秒内重复重发被频率闸拦下（429 verify_too_frequent）', async () => {
    const email = 'frequent@example.com';
    const registered = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: 'auth-key-frequent', device: 'device-frequent' },
    });
    expect(registered.status).toBe(200);

    // 注册那一封还在 60 秒窗口内 ⇒ 重发被闸门拦下
    const result = await api('POST', '/v1/auth/resend', { body: { email } });
    expect(result.status).toBe(429);
    expect(result.body.error.code).toBe('verify_too_frequent');
  });

  it('重发后可以用新验证码完成验证（恢复路径闭环）', async () => {
    const email = 'recover@example.com';
    const registered = await api('POST', '/v1/auth/register', {
      body: { email, auth_key: 'auth-key-recover', device: 'device-recover' },
    });
    expect(registered.status).toBe(200);

    await backdateVerifications(email, 120);
    const resent = await api('POST', '/v1/auth/resend', { body: { email } });
    expect(resent.status).toBe(200);

    // 取最新那封邮件里的码（重发后 outbox 里是最新一封）
    const code = await latestCode(email);
    const verified = await api('POST', '/v1/auth/verify', { body: { email, code } });
    expect(verified.status).toBe(200);

    const login = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: 'auth-key-recover', device: 'device-recover' },
    });
    expect(login.status).toBe(200);
    expect(login.body.email_verified).toBe(true);
  });
});

// ---------------------------------------------------------------- 同步

describe('同步', () => {
  it('主链路：推送 → 拉取 → 重推 → 增量拉 → 设备列表', async () => {
    const email = 'happy@example.com';
    const device = 'device-happy';
    const { register, login } = await signUpAndLogin(email, device);
    const token = login.access_token as string;

    expect(login.email_verified).toBe(true);
    expect(login.account_id).toBe(register.account_id);
    expect(login.salt).toBe(register.salt);

    const pushed = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main'), item('phrases/main', { type: 'phrases' })] },
    });
    expect(pushed.status).toBe(200);
    expect(pushed.body.accepted).toEqual(['config/main', 'phrases/main']);
    const firstSeq = pushed.body.server_seq as number;
    expect(firstSeq).toBeGreaterThan(0);

    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.status).toBe(200);
    expect(pulled.body.has_more).toBe(false);
    expect(pulled.body.items).toHaveLength(2);

    const configItem = pulled.body.items.find((row: any) => row.item_key === 'config/main');
    expect(configItem).toMatchObject({
      type: 'config',
      nonce: NONCE,
      cipher: CIPHER,
      hlc: { ms: 1_700_000_000_000, counter: 0, device: 'device-a' },
    });
    expect(configItem.server_seq).toBeGreaterThan(0);

    // 同 item_key 重推（**HLC 更新**）：序号必须前进，否则客户端会以为「没变化」。
    // HLC 不更新的重推会被服务端守卫跳过，见「HLC 守卫」那组用例。
    const newCipher = b64(new Uint8Array(50).fill(11));
    const repushed = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: newCipher,
            hlc: { ms: 1_700_000_000_001, counter: 0, device: 'device-a' },
          }),
        ],
      },
    });
    expect(repushed.status).toBe(200);
    expect(repushed.body.accepted).toEqual(['config/main']);
    expect(repushed.body.skipped).toEqual([]);
    expect(repushed.body.server_seq).toBeGreaterThan(firstSeq);

    // 全量拉：仍然只有 2 条（同一 item_key 覆盖），内容已更新
    const afterRepush = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(afterRepush.body.items).toHaveLength(2);
    const updated = afterRepush.body.items.find((row: any) => row.item_key === 'config/main');
    expect(updated.cipher).toBe(newCipher);

    // 增量拉：只拿到重推后的那一条
    const incremental = await api('GET', `/v1/sync/pull?since=${firstSeq}`, { token });
    expect(incremental.status).toBe(200);
    expect(incremental.body.items).toHaveLength(1);
    expect(incremental.body.items[0].item_key).toBe('config/main');

    const devices = await api('GET', '/v1/devices', { token });
    expect(devices.status).toBe(200);
    expect(devices.body).toHaveLength(1);
    expect(devices.body[0]).toMatchObject({
      device_id: device,
      name: device,
      revoked: false,
      current: true,
    });
  });

  it('分页：limit 生效，cursor 续读不重不漏', async () => {
    const { login } = await signUpAndLogin('paging@example.com', 'device-paging');
    const token = login.access_token as string;
    const keys = ['config/a', 'config/b', 'config/c'];

    const pushed = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: keys.map((key, index) =>
          item(key, { hlc: { ms: 1000 + index, counter: 0, device: 'device-paging' } }),
        ),
      },
    });
    expect(pushed.status).toBe(200);

    const first = await api('GET', '/v1/sync/pull?limit=2', { token });
    expect(first.body.items).toHaveLength(2);
    expect(first.body.has_more).toBe(true);
    expect(typeof first.body.next_cursor).toBe('string');

    const cursor = encodeURIComponent(first.body.next_cursor as string);
    const second = await api('GET', `/v1/sync/pull?limit=2&cursor=${cursor}`, { token });
    expect(second.body.items).toHaveLength(1);
    expect(second.body.has_more).toBe(false);

    const all = [...first.body.items, ...second.body.items].map((row: any) => row.item_key);
    expect(all).toEqual(keys);
  });

  it('推送校验：任何一条非法都整批 400，不写半条', async () => {
    const { login } = await signUpAndLogin('validate@example.com', 'device-validate');
    const token = login.access_token as string;

    const empty = await api('POST', '/v1/sync/push', { token, body: { items: [] } });
    expect(empty.status).toBe(400);
    expect(empty.body.error.code).toBe('bad_items');

    // item_key 含竖线会破坏信封 8 行格式 ⇒ 拒绝
    const pipe = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config|main')] },
    });
    expect(pipe.status).toBe(400);
    expect(pipe.body.error.code).toBe('bad_request');

    const badType = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main', { type: 'exe' })] },
    });
    expect(badType.status).toBe(400);

    const shortNonce = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main', { nonce: b64(new Uint8Array(8).fill(1)) })] },
    });
    expect(shortNonce.status).toBe(400);
    expect(shortNonce.body.error.message).toContain('nonce');

    // 密文至少要装得下 GCM 认证标签
    const shortCipher = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main', { cipher: b64(new Uint8Array(8).fill(1)) })] },
    });
    expect(shortCipher.status).toBe(400);
    expect(shortCipher.body.error.message).toContain('cipher');

    const badCounter = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main', { hlc: { ms: 1, counter: 70000, device: 'd' } })] },
    });
    expect(badCounter.status).toBe(400);
    expect(badCounter.body.error.message).toContain('counter');

    // 扁平别名（hlc_ms / hlc_counter / device）也接受：客户端拿错形态不该整批失败
    const flat = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          {
            item_key: 'config/flat',
            type: 'config',
            hlc_ms: 1,
            hlc_counter: 0,
            device: 'flat-device',
            nonce: NONCE,
            cipher: CIPHER,
          },
        ],
      },
    });
    expect(flat.status).toBe(200);

    // 前面所有 400 都没落库，云端只有这一条
    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.body.items.map((row: any) => row.item_key)).toEqual(['config/flat']);
  });

  it('拉取参数校验：since / limit / cursor 非法一律 400', async () => {
    const { login } = await signUpAndLogin('params@example.com', 'device-params');
    const token = login.access_token as string;

    const since = await api('GET', '/v1/sync/pull?since=abc', { token });
    expect(since.status).toBe(400);
    expect(since.body.error.code).toBe('bad_since');

    const limit = await api('GET', '/v1/sync/pull?limit=0', { token });
    expect(limit.status).toBe(400);
    expect(limit.body.error.code).toBe('bad_limit');

    const cursor = await api('GET', '/v1/sync/pull?cursor=***', { token });
    expect(cursor.status).toBe(400);
    expect(cursor.body.error.code).toBe('bad_cursor');
  });

  it('拉取响应带顶层 server_seq（客户端据此推进增量游标）', async () => {
    const { login } = await signUpAndLogin('seq@example.com', 'device-seq');
    const token = login.access_token as string;

    const pushed = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main')] },
    });
    expect(pushed.status).toBe(200);

    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.status).toBe(200);
    expect(typeof pulled.body.server_seq).toBe('number');
    expect(pulled.body.server_seq).toBeGreaterThan(0);
    expect(pulled.body.server_seq).toBe(pushed.body.server_seq);
    // 每条 item 也带自己的 server_seq
    expect(pulled.body.items[0].server_seq).toBe(pulled.body.server_seq);
  });
});

// ---------------------------------------------------------------- HLC 守卫
//
// 服务端 push 必须比较 HLC：客户端把「合并后的条目」回传时，若不带这道守卫，
// 服务端较新的版本会被客户端手上那份旧版本覆盖 ⇒ 两端来回乒乓、永不收敛。
// 比较规则逐字镜像 include/FluxingCloudEnvelope.h 的 CompareHlc：
// ms → counter → device（device 按字典序 tiebreak）。

describe('HLC 守卫', () => {
  it('推送更旧的 HLC：该条被跳过，服务端版本不被回退', async () => {
    const { login } = await signUpAndLogin('older@example.com', 'device-older');
    const token = login.access_token as string;

    const newerCipher = b64(new Uint8Array(50).fill(21));
    const first = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: newerCipher,
            hlc: { ms: 2_000_000_000_000, counter: 5, device: 'device-b' },
          }),
        ],
      },
    });
    expect(first.status).toBe(200);
    expect(first.body.accepted).toEqual(['config/main']);
    expect(first.body.skipped).toEqual([]);

    // 同名、更旧的 HLC ⇒ 整条跳过（不写 R2 / D1 / 不推进 server_seq）
    const olderCipher = b64(new Uint8Array(50).fill(22));
    const second = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: olderCipher,
            hlc: { ms: 1_999_999_999_999, counter: 9, device: 'device-z' },
          }),
        ],
      },
    });
    expect(second.status).toBe(200);
    expect(second.body.skipped).toContain('config/main');
    expect(second.body.accepted).not.toContain('config/main');
    expect(second.body.accepted).toEqual([]);
    // 跳过不推进 server_seq
    expect(second.body.server_seq).toBe(first.body.server_seq);

    // 拉回来的仍是较新版本（证明没有被回退）
    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.status).toBe(200);
    const row = pulled.body.items.find((entry: any) => entry.item_key === 'config/main');
    expect(row.cipher).toBe(newerCipher);
    expect(row.hlc).toEqual({ ms: 2_000_000_000_000, counter: 5, device: 'device-b' });
  });

  it('推送更新的 HLC：该条被接受，拉取拿到新版本', async () => {
    const { login } = await signUpAndLogin('newer@example.com', 'device-newer');
    const token = login.access_token as string;

    const oldCipher = b64(new Uint8Array(50).fill(31));
    const first = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: oldCipher,
            hlc: { ms: 2_000_000_000_000, counter: 0, device: 'device-a' },
          }),
        ],
      },
    });
    expect(first.status).toBe(200);
    expect(first.body.accepted).toEqual(['config/main']);

    // 同 ms、counter 更大 ⇒ 更新
    const newCipher = b64(new Uint8Array(50).fill(32));
    const second = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: newCipher,
            hlc: { ms: 2_000_000_000_000, counter: 1, device: 'device-a' },
          }),
        ],
      },
    });
    expect(second.status).toBe(200);
    expect(second.body.accepted).toEqual(['config/main']);
    expect(second.body.skipped).toEqual([]);
    expect(second.body.server_seq).toBeGreaterThan(first.body.server_seq);

    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    const row = pulled.body.items.find((entry: any) => entry.item_key === 'config/main');
    expect(row.cipher).toBe(newCipher);
    expect(row.hlc).toEqual({ ms: 2_000_000_000_000, counter: 1, device: 'device-a' });
  });

  it('同 ms 同 counter 时按 device 字典序定胜负（与 CompareHlc 的 tiebreak 一致）', async () => {
    const { login } = await signUpAndLogin('tiebreak@example.com', 'device-tiebreak');
    const token = login.access_token as string;

    const base = { ms: 2_000_000_000_000, counter: 3 };
    const first = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: b64(new Uint8Array(50).fill(41)),
            hlc: { ...base, device: 'device-m' },
          }),
        ],
      },
    });
    expect(first.body.accepted).toEqual(['config/main']);

    // device 字典序更小 ⇒ 旧版本 ⇒ 跳过
    const smaller = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: b64(new Uint8Array(50).fill(42)),
            hlc: { ...base, device: 'device-a' },
          }),
        ],
      },
    });
    expect(smaller.body.skipped).toContain('config/main');
    expect(smaller.body.accepted).toEqual([]);

    // device 字典序更大 ⇒ 更新 ⇒ 接受
    const greater = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: b64(new Uint8Array(50).fill(43)),
            hlc: { ...base, device: 'device-z' },
          }),
        ],
      },
    });
    expect(greater.body.accepted).toEqual(['config/main']);
    expect(greater.body.skipped).toEqual([]);
  });

  it('一批里新旧混排：新版写入、旧版跳过', async () => {
    const { login } = await signUpAndLogin('mixed@example.com', 'device-mixed');
    const token = login.access_token as string;

    const first = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/a', { hlc: { ms: 2_000_000_000_000, counter: 0, device: 'device-a' } }),
          item('config/b', { hlc: { ms: 2_000_000_000_000, counter: 0, device: 'device-a' } }),
        ],
      },
    });
    expect(first.body.accepted).toEqual(['config/a', 'config/b']);

    const second = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/a', { hlc: { ms: 1_000_000_000_000, counter: 0, device: 'device-a' } }),
          item('config/b', { hlc: { ms: 3_000_000_000_000, counter: 0, device: 'device-a' } }),
        ],
      },
    });
    expect(second.body.skipped).toEqual(['config/a']);
    expect(second.body.accepted).toEqual(['config/b']);
  });
});

// ---------------------------------------------------------------- 设备与账号

describe('设备与账号', () => {
  it('撤销设备：列表标记 revoked，该设备的令牌立即失效', async () => {
    const { login } = await signUpAndLogin('revoke@example.com', 'device-revoke');
    const token = login.access_token as string;

    const devices = await api('GET', '/v1/devices', { token });
    const target = devices.body[0];
    expect(target.current).toBe(true);

    // 路径参数同时接受 devices.id 与客户端稳定 device_id，这里用后者验证别名
    const revoked = await api('DELETE', `/v1/devices/${target.device_id}`, { token });
    expect(revoked.status).toBe(200);
    expect(revoked.body).toMatchObject({ ok: true, id: target.id });

    const afterRevoke = await api('GET', '/v1/sync/pull', { token });
    expect(afterRevoke.status).toBe(401);
    expect(afterRevoke.body.error.code).toBe('token_revoked');
  });

  it('撤销不存在的设备返回 404', async () => {
    const { login } = await signUpAndLogin('unknown-device@example.com', 'device-unknown');
    const result = await api(
      'DELETE',
      '/v1/devices/00000000-0000-0000-0000-000000000000',
      { token: login.access_token },
    );
    expect(result.status).toBe(404);
    expect(result.body.error.code).toBe('device_not_found');
  });

  it('清空云端数据：只清同步内容，账号仍可登录', async () => {
    const email = 'clear@example.com';
    const device = 'device-clear';
    const { authKey, login } = await signUpAndLogin(email, device);
    const token = login.access_token as string;

    const pushed = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [item('config/main'), item('userdb/main', { type: 'userdb' })],
      },
    });
    expect(pushed.status).toBe(200);

    const cleared = await api('DELETE', '/v1/account/data', { token });
    expect(cleared.status).toBe(200);
    expect(cleared.body).toMatchObject({ ok: true, deleted_objects: 2, deleted_items: 2 });

    const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
    expect(pulled.body.items).toEqual([]);

    // users 行没被删：原凭证仍能登录，邮箱仍是已验证状态
    const relogin = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: authKey, device },
    });
    expect(relogin.status).toBe(200);
    expect(relogin.body.email_verified).toBe(true);

    // 重复清空保持幂等
    const again = await api('DELETE', '/v1/account/data', { token });
    expect(again.status).toBe(200);
    expect(again.body).toMatchObject({ ok: true, deleted_objects: 0, deleted_items: 0 });
  });
});

// ---------------------------------------------------------------- 统一错误面

describe('统一错误面', () => {
  it('未知路径返回 404 中文原因', async () => {
    const result = await api('GET', '/v1/nope');
    expect(result.status).toBe(404);
    expect(result.body.error.code).toBe('not_found');
    expect(typeof result.body.error.message).toBe('string');
  });

  it('缺少令牌返回 401', async () => {
    const result = await api('GET', '/v1/sync/pull');
    expect(result.status).toBe(401);
    expect(result.body.error.code).toBe('missing_token');
  });

  it('令牌格式不对返回 401', async () => {
    const response = await SELF.fetch(`${BASE}/v1/sync/pull`, {
      headers: { authorization: 'Token abc' },
    });
    expect(response.status).toBe(401);
    const body = (await response.json()) as any;
    expect(body.error.code).toBe('bad_token_format');
  });

  it('请求体不是对象返回 400', async () => {
    const response = await SELF.fetch(`${BASE}/v1/auth/login`, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: '[]',
    });
    expect(response.status).toBe(400);
    const body = (await response.json()) as any;
    expect(body.error.code).toBe('bad_request');
  });

  it('成功响应带 no-store 与 nosniff', async () => {
    const response = await SELF.fetch(`${BASE}/v1/health`);
    expect(response.headers.get('cache-control')).toBe('no-store');
    expect(response.headers.get('x-content-type-options')).toBe('nosniff');
  });
});