// 「忘记密码 / 重置口令」端点测试（批次 AT-2）。
//
// 覆盖服务端硬要求：
//   1. 目的隔离：注册码与重置码**互不通用**（两个方向都断言）。
//   2. 生命周期：TTL 超时、错误次数上限、消费后不可复用、60 秒重发窗口。
//   3. 落库口径：verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))。
//   4. 任何失败路径都**不得**改动 verifier。
//   5. start 对不存在邮箱的行为（统一成功，防邮箱枚举）。
//
// 隔离口径：重置码落在独立表 password_resets，注册码在 email_verifications，
// 两者物理分表 ⇒ 互不通用不依赖「查询时记得加 purpose 过滤」这种人工约定。

import { SELF, env } from 'cloudflare:test';
import { describe, expect, it } from 'vitest';
import { MAIL_SUBJECT, RESET_SUBJECT } from '../src/mail';

const BASE = 'https://fluxing-sync.test';

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

/** 从 outbox 里取该邮箱最近一封指定主题邮件中的 6 位码（测试专用通道，生产不存在）。 */
async function codeFromMail(email: string, subject: string): Promise<string> {
  const row = await env.DB.prepare(
    'SELECT body FROM mail_outbox WHERE to_addr = ? AND subject = ? ORDER BY id DESC LIMIT 1',
  )
    .bind(email, subject)
    .first<{ body: string }>();
  if (row === null) throw new Error(`没有发给 ${email} 的「${subject}」邮件`);
  const matched = /验证码是：(\d{6})/.exec(row.body);
  if (matched === null) throw new Error(`邮件正文里没有 6 位码：${row.body}`);
  return matched[1] as string;
}

async function registerCode(email: string): Promise<string> {
  return codeFromMail(email, MAIL_SUBJECT);
}

async function resetCode(email: string): Promise<string> {
  return codeFromMail(email, RESET_SUBJECT);
}

/** 服务端落库的 verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))，这里独立复算一遍。 */
async function expectedVerifier(authKey: string): Promise<string> {
  const pepper = env.AUTH_PEPPER;
  if (typeof pepper !== 'string') throw new Error('测试环境缺少 AUTH_PEPPER');
  const encoder = new TextEncoder();
  const key = await crypto.subtle.importKey(
    'raw',
    encoder.encode(pepper),
    { name: 'HMAC', hash: 'SHA-256' },
    false,
    ['sign'],
  );
  const signature = new Uint8Array(
    await crypto.subtle.sign('HMAC', key, encoder.encode(authKey)),
  );
  let binary = '';
  for (const byte of signature) binary += String.fromCharCode(byte);
  return btoa(binary);
}

async function storedVerifier(email: string): Promise<string | null> {
  const row = await env.DB.prepare('SELECT verifier FROM users WHERE email = ?')
    .bind(email)
    .first<{ verifier: string }>();
  return row?.verifier ?? null;
}

async function emailVerifiedFlag(email: string): Promise<number> {
  const row = await env.DB.prepare('SELECT email_verified FROM users WHERE email = ?')
    .bind(email)
    .first<{ email_verified: number }>();
  return row?.email_verified ?? -1;
}

/** 注册一个账号（未验证），返回注册码。 */
async function register(email: string, authKey: string): Promise<void> {
  const result = await api('POST', '/v1/auth/register', {
    body: { email, auth_key: authKey, device: 'device-reset' },
  });
  expect(result.status).toBe(200);
}

/** 注册 → 验证 → 登录，返回登录响应（供令牌失效断言用）。 */
async function signUpAndLogin(email: string, authKey: string) {
  await register(email, authKey);
  const code = await registerCode(email);
  const verified = await api('POST', '/v1/auth/verify', { body: { email, code } });
  expect(verified.status).toBe(200);
  const loggedIn = await api('POST', '/v1/auth/login', {
    body: { email, auth_key: authKey, device: 'device-reset' },
  });
  expect(loggedIn.status).toBe(200);
  return loggedIn.body;
}

// ---------------------------------------------------------------- 发起重置

describe('POST /v1/auth/password-reset/start', () => {
  it('已存在账号：返回 ok 且不回显验证码，重置邮件已发出', async () => {
    const email = 'start@example.com';
    await register(email, 'auth-key-start');

    const result = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(result.status).toBe(200);
    expect(result.body).toEqual({ ok: true, email });

    // 响应体里不得出现任何验证码 / 凭证字段
    expect(Object.keys(result.body).sort()).toEqual(['email', 'ok']);
    expect(JSON.stringify(result.body)).not.toMatch(/\d{6}/);

    // 重置邮件用的是**重置专属**主题与正文
    const mail = await env.DB.prepare(
      'SELECT subject, body FROM mail_outbox WHERE to_addr = ? ORDER BY id DESC LIMIT 1',
    )
      .bind(email)
      .first<{ subject: string; body: string }>();
    expect(mail?.subject).toBe(RESET_SUBJECT);
    expect(mail?.body).toContain('重置口令验证码');
  });

  it('不存在邮箱：同样返回 ok，但不发信、不落库（防邮箱枚举）', async () => {
    const email = 'ghost@example.com';

    const result = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(result.status).toBe(200);
    expect(result.body).toEqual({ ok: true, email });

    const mails = await env.DB.prepare(
      'SELECT COUNT(*) AS n FROM mail_outbox WHERE to_addr = ?',
    )
      .bind(email)
      .first<{ n: number }>();
    expect(Number(mails?.n ?? 0)).toBe(0);

    const rows = await env.DB.prepare(
      'SELECT COUNT(*) AS n FROM password_resets WHERE email = ?',
    )
      .bind(email)
      .first<{ n: number }>();
    expect(Number(rows?.n ?? 0)).toBe(0);

    // 后续 confirm 也拿不到码（没有码可校验）
    const confirm = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: '123456', auth_key: 'auth-key-ghost' },
    });
    expect(confirm.status).toBe(400);
    expect(confirm.body.error.code).toBe('reset_no_code');
  });

  it('60 秒内重复发起被频率闸拦下（429 reset_too_frequent）', async () => {
    const email = 'frequent@example.com';
    await register(email, 'auth-key-frequent');

    const first = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(first.status).toBe(200);

    const second = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(second.status).toBe(429);
    expect(second.body.error.code).toBe('reset_too_frequent');
  });

  it('注册码的 60 秒窗口不影响重置码（两个流程各限各的）', async () => {
    const email = 'independent@example.com';
    await register(email, 'auth-key-independent');

    // 注册码刚发过（还在注册码的 60 秒窗口内），重置码照样能发
    const result = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(result.status).toBe(200);
  });

  it('邮箱格式非法返回 400', async () => {
    const result = await api('POST', '/v1/auth/password-reset/start', {
      body: { email: 'not-an-email' },
    });
    expect(result.status).toBe(400);
    expect(result.body.error.code).toBe('bad_request');
  });
});

// ---------------------------------------------------------------- 目的隔离

describe('目的隔离：注册码与重置码互不通用', () => {
  it('注册码不能当重置码用：verifier 不变', async () => {
    const email = 'cross1@example.com';
    const oldAuthKey = 'auth-key-cross1';
    await register(email, oldAuthKey);
    const regCode = await registerCode(email);

    // 先发起一次重置（于是 password_resets 里有了一行），再用**注册码**去 confirm
    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);

    const before = await storedVerifier(email);
    const result = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: regCode, auth_key: 'auth-key-new-cross1' },
    });
    expect(result.status).toBe(400);
    expect(result.body.error.code).toBe('reset_code_mismatch');
    expect(await storedVerifier(email)).toBe(before);
  });

  it('没发起过重置时，注册码在 confirm 一侧查不到（reset_no_code）', async () => {
    const email = 'cross2@example.com';
    await register(email, 'auth-key-cross2');
    const regCode = await registerCode(email);

    const before = await storedVerifier(email);
    const result = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: regCode, auth_key: 'auth-key-new-cross2' },
    });
    expect(result.status).toBe(400);
    expect(result.body.error.code).toBe('reset_no_code');
    expect(await storedVerifier(email)).toBe(before);
  });

  it('重置码不能当注册验证码用：邮箱验证状态不变', async () => {
    const email = 'cross3@example.com';
    await register(email, 'auth-key-cross3');

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);
    const code = await resetCode(email);

    // 把重置码喂给注册验证端点 ⇒ 必须失败
    const verified = await api('POST', '/v1/auth/verify', { body: { email, code } });
    expect(verified.status).toBe(400);
    expect(verified.body.error.code).toBe('verify_code_mismatch');
    expect(await emailVerifiedFlag(email)).toBe(0);

    // 注册流程本身仍可照常用注册码完成验证（语义未变）
    const regCode = await registerCode(email);
    const ok = await api('POST', '/v1/auth/verify', { body: { email, code: regCode } });
    expect(ok.status).toBe(200);
    expect(await emailVerifiedFlag(email)).toBe(1);
  });
});

// ---------------------------------------------------------------- 确认重置

describe('POST /v1/auth/password-reset/confirm', () => {
  it('成功后 verifier = HMAC(AUTH_PEPPER, 新 auth_key)，新凭证可登录、旧凭证失效', async () => {
    const email = 'confirm@example.com';
    const oldAuthKey = 'auth-key-confirm-old';
    const newAuthKey = 'auth-key-confirm-new';
    const login = await signUpAndLogin(email, oldAuthKey);

    const before = await storedVerifier(email);
    expect(before).toBe(await expectedVerifier(oldAuthKey));

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);
    const code = await resetCode(email);

    const result = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code, auth_key: newAuthKey },
    });
    expect(result.status).toBe(200);
    expect(result.body).toEqual({ ok: true, email });
    expect(Object.keys(result.body).sort()).toEqual(['email', 'ok']);

    // 落库口径：verifier 换成 HMAC(pepper, 新 auth_key)
    expect(await storedVerifier(email)).toBe(await expectedVerifier(newAuthKey));
    expect(await storedVerifier(email)).not.toBe(before);

    // 新凭证能登录，旧凭证不能
    const relogin = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: newAuthKey, device: 'device-reset' },
    });
    expect(relogin.status).toBe(200);
    const oldLogin = await api('POST', '/v1/auth/login', {
      body: { email, auth_key: oldAuthKey, device: 'device-reset' },
    });
    expect(oldLogin.status).toBe(401);
    expect(oldLogin.body.error.code).toBe('invalid_credentials');

    // 口令已变 ⇒ 旧令牌立即失效（客户端必须重新登录）
    const pull = await api('GET', '/v1/sync/pull', { token: login.access_token });
    expect(pull.status).toBe(401);
    expect(pull.body.error.code).toBe('token_revoked');
  });

  it('验证码错误：verifier 不变', async () => {
    const email = 'wrongcode@example.com';
    await signUpAndLogin(email, 'auth-key-wrongcode');

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);
    const code = await resetCode(email);
    const wrong = code === '000000' ? '111111' : '000000';

    const before = await storedVerifier(email);
    const result = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: wrong, auth_key: 'auth-key-wrongcode-new' },
    });
    expect(result.status).toBe(400);
    expect(result.body.error.code).toBe('reset_code_mismatch');
    expect(await storedVerifier(email)).toBe(before);
  });

  it('重置码超时：400 reset_code_expired，verifier 不变', async () => {
    const email = 'expired@example.com';
    await signUpAndLogin(email, 'auth-key-expired');

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);

    // 把过期时间往前挪（只动 expires_at，不动 created_at ⇒ 哈希仍然对得上）
    const expiredAt = Math.floor(Date.now() / 1000) - 1;
    await env.DB.prepare('UPDATE password_resets SET expires_at = ? WHERE email = ?')
      .bind(expiredAt, email)
      .run();

    const code = await resetCode(email);
    const before = await storedVerifier(email);
    const result = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code, auth_key: 'auth-key-expired-new' },
    });
    expect(result.status).toBe(400);
    expect(result.body.error.code).toBe('reset_code_expired');
    expect(await storedVerifier(email)).toBe(before);
  });

  it('错误次数超上限：锁定，verifier 不变', async () => {
    const email = 'locked@example.com';
    await signUpAndLogin(email, 'auth-key-locked');

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);
    const code = await resetCode(email);
    const wrong = code === '000000' ? '111111' : '000000';

    const before = await storedVerifier(email);
    for (let i = 0; i < 5; i += 1) {
      const attempt = await api('POST', '/v1/auth/password-reset/confirm', {
        body: { email, code: wrong, auth_key: 'auth-key-locked-new' },
      });
      expect(attempt.status).toBe(400);
      expect(attempt.body.error.code).toBe('reset_code_mismatch');
    }

    // 第 6 次：已锁定
    const locked = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: wrong, auth_key: 'auth-key-locked-new' },
    });
    expect(locked.status).toBe(400);
    expect(locked.body.error.code).toBe('reset_code_locked');
    expect(await storedVerifier(email)).toBe(before);
  });

  it('重置码消费后不可复用', async () => {
    const email = 'reuse@example.com';
    await signUpAndLogin(email, 'auth-key-reuse');

    const started = await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    expect(started.status).toBe(200);
    const code = await resetCode(email);

    const first = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code, auth_key: 'auth-key-reuse-new' },
    });
    expect(first.status).toBe(200);

    const second = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code, auth_key: 'auth-key-reuse-again' },
    });
    expect(second.status).toBe(400);
    expect(second.body.error.code).toBe('reset_code_used');
    expect(await storedVerifier(email)).toBe(await expectedVerifier('auth-key-reuse-new'));
  });

  it('请求体缺字段 / 方法不符一律 400 / 405', async () => {
    const email = 'shape@example.com';
    await signUpAndLogin(email, 'auth-key-shape');
    await api('POST', '/v1/auth/password-reset/start', { body: { email } });
    const code = await resetCode(email);

    const noCode = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, auth_key: 'auth-key-shape-new' },
    });
    expect(noCode.status).toBe(400);
    expect(noCode.body.error.code).toBe('bad_request');

    const noAuthKey = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code },
    });
    expect(noAuthKey.status).toBe(400);
    expect(noAuthKey.body.error.code).toBe('bad_request');

    const badCode = await api('POST', '/v1/auth/password-reset/confirm', {
      body: { email, code: '12345', auth_key: 'auth-key-shape-new' },
    });
    expect(badCode.status).toBe(400);
    expect(badCode.body.error.code).toBe('bad_request');

    const wrongMethod = await api('GET', '/v1/auth/password-reset/start');
    expect(wrongMethod.status).toBe(405);
    expect(wrongMethod.body.error.code).toBe('method_not_allowed');
  });
});
