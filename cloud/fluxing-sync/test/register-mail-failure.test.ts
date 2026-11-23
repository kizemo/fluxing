// 注册 / 重发验证码「发信失败 ⇒ 回滚」单测。
//
// 背景（批次 BQ）：第三方发信服务对**收件域**的永久性拒绝（例如 null MX 的 example.com）
// 之前被一律映射成 503 mail_send_failed ⇒ 客户端自动重试 ⇒ 撞上 60 秒重发闸 ⇒
// 真正的根因（收件人被拒）被掩盖成「验证码发送过于频繁」。
// 同时注册流程是「先写库、后发信」且无回滚 ⇒ 发信失败后残留 users 行与 email_verifications 行
// ⇒ 该邮箱被占号（重试撞 409 email_taken）+ 白白吃掉 60 秒重发窗口（重试撞 429 verify_too_frequent）。
//
// 本文件锁三件事：
//   1. 发信失败 ⇒ users 行与 email_verifications 行都清零（零残留）。
//   2. 失败不占号、不消耗 60 秒重发窗口 ⇒ 立刻重试仍是 422，不是 429 / 409。
//   3. 重发失败 ⇒ 保留 users 行（账号已存在），只删本次验证码行。
//
// 零真实网络：发信走注入的 fetch 替身（沿用 sendVerificationMail 既有的 doFetch 注入风格）。

import { describe, it, expect, vi } from 'vitest';
import { env } from 'cloudflare:test';
import { handleRegister, handleResend } from '../src/auth';
import { ApiError, toErrorResponse } from '../src/errors';
import { redactSensitive } from '../src/mail';
import type { Env } from '../src/env';

/** 上游拒绝收件人的响应体（Resend 形状：{ statusCode, name, message }）。 */
const RECIPIENT_REJECTED_BODY =
  '{"statusCode":400,"name":"validation_error","message":"Invalid `to` field"}';

interface ErrShape {
  status: number;
  code: string;
  message: string;
}

/** env 副本：**绝不**修改共享 env 对象（否则会污染同文件里的其它用例）。 */
function makeEnv(overrides: Record<string, string | undefined> = {}): Env {
  return {
    ...(env as unknown as Env),
    MAIL_MODE: 'http',
    MAIL_FROM: 'no-reply@aiec.fun',
    MAIL_FROM_NAME: '',
    MAIL_HTTP_ENDPOINT: 'https://api.resend.com/emails',
    MAIL_HTTP_AUTH_HEADER: 'Authorization',
    MAIL_HTTP_AUTH_PREFIX: 'Bearer ',
    MAIL_HTTP_API_KEY: 're_test_key_0123456789',
    ...overrides,
  } as Env;
}

/** 假 fetch：返回指定状态码与响应体，不打真实网络。 */
function stubFetch(status: number, body: string): typeof fetch {
  return async () => new Response(body, { status });
}

function registerBody(email: string): Record<string, unknown> {
  return { email, auth_key: `auth-key-${email}`, device: 'test-device-01' };
}

async function countRows(table: 'users' | 'email_verifications', email: string): Promise<number> {
  const row = await env.DB.prepare(`SELECT COUNT(*) AS n FROM ${table} WHERE email = ?`)
    .bind(email)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/** 捕获抛出的错误（用于断言「不是 429 / 不是 409」这类否定式判据）。 */
async function catchError(promise: Promise<unknown>): Promise<ErrShape> {
  try {
    await promise;
  } catch (error) {
    return error as ErrShape;
  }
  throw new Error('预期抛出错误，但调用成功了。');
}

/**
 * 把该邮箱已发的验证码行往前挪，绕开 60 秒重发闸（测试专用夹具）。
 * 不这么做的话「注册 → 立刻重发」必然撞 429，测不到「重发失败 ⇒ 回滚」那条路径。
 */
async function backdateVerifications(email: string, seconds: number): Promise<void> {
  await env.DB.prepare('UPDATE email_verifications SET created_at = created_at - ? WHERE email = ?')
    .bind(seconds, email)
    .run();
}

describe('注册 / 重发验证码：发信失败 ⇒ 回滚', () => {
  it('发信被上游拒绝（400）⇒ 抛 422 mail_recipient_rejected，且库里零残留', async () => {
    const email = 'bounce@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    await expect(handleRegister(makeEnv(), registerBody(email), fetchImpl)).rejects.toMatchObject({
      status: 422,
      code: 'mail_recipient_rejected',
      message: '验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。',
    });

    // 零残留：既没有账号行，也没有验证码行。
    expect(await countRows('users', email)).toBe(0);
    expect(await countRows('email_verifications', email)).toBe(0);
  });

  it('发信失败后立刻重试同一邮箱 ⇒ 仍以 422 失败（不撞 429 verify_too_frequent、不撞 409 email_taken）', async () => {
    const email = 'retry@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    await expect(handleRegister(makeEnv(), registerBody(email), fetchImpl)).rejects.toMatchObject({
      status: 422,
      code: 'mail_recipient_rejected',
    });

    const second = await catchError(
      handleRegister(makeEnv(), registerBody(email), fetchImpl),
    );
    expect(second.status).toBe(422);
    expect(second.code).toBe('mail_recipient_rejected');
    // 否定式判据：失败不占号、不消耗 60 秒重发窗口。
    expect(second.status).not.toBe(429);
    expect(second.status).not.toBe(409);
    expect(second.code).not.toBe('verify_too_frequent');
    expect(second.code).not.toBe('email_taken');

    expect(await countRows('users', email)).toBe(0);
    expect(await countRows('email_verifications', email)).toBe(0);
  });

  it('上游 500 ⇒ 抛 503 mail_send_failed，且同样零残留', async () => {
    const email = 'upstream-down@example.com';
    const fetchImpl = stubFetch(500, '{"message":"boom"}');

    await expect(handleRegister(makeEnv(), registerBody(email), fetchImpl)).rejects.toMatchObject({
      status: 503,
      code: 'mail_send_failed',
    });

    expect(await countRows('users', email)).toBe(0);
    expect(await countRows('email_verifications', email)).toBe(0);
  });

  it('重发失败 ⇒ users 行仍在（账号不删）、本次验证码行已删，且立刻再重发不撞 429', async () => {
    const email = 'resend@example.com';

    // 先用默认 outbox 模式注册成功（不传 doFetch ⇒ 只写 mail_outbox，不发真信）。
    const registered = (await handleRegister(
      makeEnv({ MAIL_MODE: 'outbox' }),
      registerBody(email),
    )) as { need_email_verify: boolean };
    expect(registered.need_email_verify).toBe(true);
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);

    // 把已发的验证码行往前挪，绕开 60 秒重发闸（测试夹具）。
    await backdateVerifications(email, 3600);

    // http 模式 + 失败 fetch ⇒ 重发失败。
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);
    await expect(handleResend(makeEnv(), { email }, fetchImpl)).rejects.toMatchObject({
      status: 422,
      code: 'mail_recipient_rejected',
    });

    // users 行仍在；本次验证码行已被删 ⇒ 该邮箱行数回到注册时那 1 条。
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);

    // 立刻再重发：不撞 429（失败不消耗 60 秒窗口），仍以 422 失败。
    const again = await catchError(handleResend(makeEnv(), { email }, fetchImpl));
    expect(again.status).not.toBe(429);
    expect(again.status).toBe(422);
    expect(again.code).toBe('mail_recipient_rejected');
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
  });

  it('日志脱敏：上游回显请求内容时，邮箱 / 验证码明文 / API Key 都被抹掉', () => {
    const email = 'echo@example.com';
    const code = '123456';
    const apiKey = 're_test_key_0123456789';
    const message = {
      to: email,
      subject: '火流猩输入法 · 邮箱验证码',
      text: `您的火流猩输入法账号验证码是：${code}`,
      html: `<b>${code}</b>`,
    };
    // 模拟上游把请求体原样回显成错误响应体。
    const raw = `{"to":["${email}"],"text":"您的验证码是：${code}","html":"<b>${code}</b>","key":"${apiKey}"}`;

    const out = redactSensitive(raw, message, apiKey);

    expect(out).not.toContain(email);
    expect(out).not.toContain(code);
    expect(out).not.toContain(apiKey);
    expect(out).toContain('[已脱敏]');
  });

  it('成功路径不受影响：outbox 模式注册 ⇒ need_email_verify: true，users 1 行、email_verifications 1 行', async () => {
    const email = 'happy@example.com';

    const result = (await handleRegister(
      makeEnv({ MAIL_MODE: 'outbox' }),
      registerBody(email),
    )) as { need_email_verify: boolean; account_id: string };

    expect(result.need_email_verify).toBe(true);
    expect(typeof result.account_id).toBe('string');
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
  });
});

describe('错误响应诊断头：X-Fluxing-Mail-Provider-Status', () => {
  it('拿到了上游 HTTP 响应 ⇒ 响应带该头，值是上游 status 的纯数字串', () => {
    const response = toErrorResponse(
      new ApiError(422, 'mail_recipient_rejected', '收件人被拒，请更换邮箱地址后重试。', 400),
    );
    expect(response.status).toBe(422);
    expect(response.headers.get('X-Fluxing-Mail-Provider-Status')).toBe('400');
  });

  it('没有上游调用（配置缺失 / 预检失败）⇒ 不带该头', () => {
    const response = toErrorResponse(new ApiError(500, 'server_misconfigured', '发信通道未配置。'));
    expect(response.headers.get('X-Fluxing-Mail-Provider-Status')).toBeNull();
  });
});

describe('错误响应诊断头：X-Fluxing-Mail-Rollback', () => {
  it('回滚成功 ⇒ 头为 ok，且库里零残留', async () => {
    const email = 'rollback-ok@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const error = await catchError(handleRegister(makeEnv(), registerBody(email), fetchImpl));
    expect(error.status).toBe(422);

    const response = toErrorResponse(error);
    expect(response.headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
    expect(await countRows('users', email)).toBe(0);
    expect(await countRows('email_verifications', email)).toBe(0);
  });

  it('DELETE 被拒但 UPDATE 失效化成功 ⇒ 头为 degraded：占号解除、可立刻重试，根因仍是 422', async () => {
    const email = 'rollback-degraded@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const db = env.DB as unknown as {
      prepare: (sql: string) => {
        bind: (value: unknown) => { run: () => Promise<unknown> };
      };
    };
    const originalPrepare = db.prepare.bind(db);
    const failingStatement = {
      bind: (_value: unknown) => failingStatement,
      run: async () => {
        throw new Error('模拟 D1 删除失败');
      },
    };
    const spy = vi.spyOn(db, 'prepare').mockImplementation((sql: string) => {
      if (sql.startsWith('DELETE FROM')) return failingStatement as unknown as ReturnType<typeof db.prepare>;
      return originalPrepare(sql);
    });

    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email), fetchImpl));
      // 根因不被覆盖：状态码与错误码仍是上游拒绝的语义。
      expect(error.status).toBe(422);
      expect(error.code).toBe('mail_recipient_rejected');

      const response = toErrorResponse(error);
      expect(response.headers.get('X-Fluxing-Mail-Rollback')).toBe('degraded');
      // 降级原因进诊断头（不进错误体）；头值必须是 ASCII（HTTP 头规范）。
      const rollbackError = response.headers.get('X-Fluxing-Mail-Rollback-Error') ?? '';
      expect(rollbackError).toContain('delete failed');
      expect(/^[\x20-\x7e]*$/.test(rollbackError)).toBe(true);

      // 降级效果一：users 行被改成占位地址 ⇒ 原邮箱不再占号。
      expect(await countRows('users', email)).toBe(0);
      // 降级效果二：验证码行仍在，但 created_at 归零 ⇒ 不吃 60 秒重发窗口。
      const row = await env.DB.prepare('SELECT created_at FROM email_verifications WHERE email = ?')
        .bind(email)
        .first<{ created_at: number }>();
      expect(Number(row?.created_at)).toBe(0);

      // 立刻重试：不撞 409 / 429，仍是 422。
      const again = await catchError(handleRegister(makeEnv(), registerBody(email), fetchImpl));
      expect(again.status).toBe(422);
      expect(again.code).not.toBe('email_taken');
      expect(again.code).not.toBe('verify_too_frequent');
    } finally {
      spy.mockRestore();
    }
  });

  it('DELETE 与 UPDATE 都被拒 ⇒ 头为 failed，根因仍是 422，诊断头带失败原因', async () => {
    const email = 'rollback-fail@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const db = env.DB as unknown as {
      prepare: (sql: string) => {
        bind: (value: unknown) => { run: () => Promise<unknown> };
      };
    };
    const originalPrepare = db.prepare.bind(db);
    const failingStatement = {
      bind: (_value: unknown) => failingStatement,
      run: async () => {
        throw new Error('模拟 D1 写入失败');
      },
    };
    const spy = vi.spyOn(db, 'prepare').mockImplementation((sql: string) => {
      if (sql.startsWith('DELETE FROM')) return failingStatement as unknown as ReturnType<typeof db.prepare>;
      if (sql.startsWith('UPDATE users SET email')) {
        return failingStatement as unknown as ReturnType<typeof db.prepare>;
      }
      if (sql.startsWith('UPDATE email_verifications SET created_at')) {
        return failingStatement as unknown as ReturnType<typeof db.prepare>;
      }
      return originalPrepare(sql);
    });

    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email), fetchImpl));
      // 根因不被覆盖：状态码与错误码仍是上游拒绝的语义。
      expect(error.status).toBe(422);
      expect(error.code).toBe('mail_recipient_rejected');

      const response = toErrorResponse(error);
      expect(response.headers.get('X-Fluxing-Mail-Rollback')).toBe('failed');
      const rollbackError = response.headers.get('X-Fluxing-Mail-Rollback-Error') ?? '';
      expect(rollbackError).toContain('neutralize failed');
      expect(/^[\x20-\x7e]*$/.test(rollbackError)).toBe(true);
      // 错误体不含内部细节（只有诊断头带）。
      const body = (await response.json()) as { error: { code: string; message: string } };
      expect(body.error.code).toBe('mail_recipient_rejected');
      expect(body.error.message).not.toContain('D1');
    } finally {
      spy.mockRestore();
    }
  });

  it('回滚走逐条删除 ⇒ 绝不调用 DB.batch（生产实测 batch 一条都没删掉）', async () => {
    const email = 'no-batch@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const db = env.DB as unknown as {
      prepare: (sql: string) => unknown;
      batch: (statements: unknown[]) => Promise<unknown>;
    };
    const batchSpy = vi.spyOn(db, 'batch');

    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email), fetchImpl));
      expect(error.status).toBe(422);
      expect(batchSpy).not.toHaveBeenCalled();
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
    } finally {
      batchSpy.mockRestore();
    }
  });
});

describe('D1 行写入配额耗尽（Cloudflare code 7500）⇒ 暂时性 503', () => {
  const QUOTA_ERROR =
    "Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details. [code: 7500]";

  it('未知异常命中配额特征 ⇒ 503 storage_quota_exceeded（不是笼统 500）', async () => {
    const response = toErrorResponse(new Error(QUOTA_ERROR));
    expect(response.status).toBe(503);
    const body = (await response.json()) as { error: { code: string; message: string } };
    expect(body.error.code).toBe('storage_quota_exceeded');
    expect(body.error.message).toBe('服务端存储写入额度已用尽，请稍后重试。');
  });

  it('注册写库被配额拒绝 ⇒ 同样 503 storage_quota_exceeded，且库里零残留', async () => {
    const email = 'quota@example.com';
    const db = env.DB as unknown as {
      prepare: (sql: string) => { bind: (value: unknown) => { run: () => Promise<unknown> } };
    };
    const originalPrepare = db.prepare.bind(db);
    const failingStatement = {
      bind: (_value: unknown) => failingStatement,
      run: async () => {
        throw new Error(QUOTA_ERROR);
      },
    };
    const spy = vi.spyOn(db, 'prepare').mockImplementation((sql: string) => {
      if (sql.startsWith('INSERT INTO users')) {
        return failingStatement as unknown as ReturnType<typeof db.prepare>;
      }
      return originalPrepare(sql);
    });

    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      const response = toErrorResponse(error);
      expect(response.status).toBe(503);
      const body = (await response.json()) as { error: { code: string } };
      expect(body.error.code).toBe('storage_quota_exceeded');
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
    } finally {
      spy.mockRestore();
    }
  });
});
