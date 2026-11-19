// MAIL_MODE="http"（第三方 HTTPS 邮件 API，当前 = Resend）分支单测。
//
// 零真实网络：sendVerificationMail 支持注入 fetch 替身（FetchLike）。
// 判别力要点（变异验证打的就是这几处）：
//   · 缺 key / 非 HTTPS 端点 / MAIL_FROM 非法 ⇒ 500 server_misconfigured
//   · 401 / 403 ⇒ 500 server_misconfigured（鉴权失败），且**不重试**
//   · 429 ⇒ 503 mail_rate_limited；其它 4xx / 5xx ⇒ 503 mail_send_failed
//   · 网络类异常重试一次（共 2 次调用）；业务类错误不重试（1 次调用）
//   · http 模式**不写 mail_outbox** ⇒ 生产库不留明文验证码
//   · 端点 / 鉴权头名 / 前缀可配 ⇒ 换服务商不改代码

import { describe, it, expect } from 'vitest';
import { env } from 'cloudflare:test';
import { sendPasswordResetMail, sendVerificationMail, MAIL_SUBJECT, RESET_SUBJECT, type FetchLike } from '../src/mail';
import type { Env } from '../src/env';

interface Call {
  url: string;
  init: RequestInit;
}

function makeEnv(overrides: Record<string, string | undefined> = {}): Env {
  return {
    ...(env as unknown as Env),
    // 显式覆盖 miniflare 从 wrangler.toml [vars] 注入的值：
    // 单测绝不能依赖配置文件当前内容（否则改一次 wrangler.toml 就假红）。
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

/** 返回 200 的替身，并记录每次调用。 */
function okResponder(body = '{"id":"mail_abc123"}'): { calls: Call[]; fetchImpl: FetchLike } {
  const calls: Call[] = [];
  const fetchImpl: FetchLike = async (url, init) => {
    calls.push({ url, init });
    return new Response(body, { status: 200 });
  };
  return { calls, fetchImpl };
}

/** 返回指定状态码的替身，并记录每次调用。 */
function statusResponder(status: number, body = '{"message":"boom"}'): {
  calls: Call[];
  fetchImpl: FetchLike;
} {
  const calls: Call[] = [];
  const fetchImpl: FetchLike = async (url, init) => {
    calls.push({ url, init });
    return new Response(body, { status });
  };
  return { calls, fetchImpl };
}

function headersOf(call: Call): Record<string, string> {
  return call.init.headers as unknown as Record<string, string>;
}

function payloadOf(call: Call): {
  from: string;
  to: string[];
  subject: string;
  text: string;
  html: string;
} {
  return JSON.parse(String(call.init.body)) as {
    from: string;
    to: string[];
    subject: string;
    text: string;
    html: string;
  };
}

describe('MAIL_MODE="http" 第三方 HTTPS 邮件 API', () => {
  it('成功：按 Resend 形状 POST 到端点，带 Bearer 鉴权，且不写 mail_outbox', async () => {
    const { calls, fetchImpl } = okResponder();
    await sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl);

    expect(calls.length).toBe(1);
    expect(calls[0].url).toBe('https://api.resend.com/emails');
    expect(calls[0].init.method).toBe('POST');

    const headers = headersOf(calls[0]);
    expect(headers['Authorization']).toBe('Bearer re_test_key_0123456789');
    expect(headers['content-type']).toBe('application/json');

    const payload = payloadOf(calls[0]);
    expect(payload.from).toBe('no-reply@aiec.fun');
    expect(payload.to).toEqual(['user@example.com']);
    expect(payload.subject).toBe('火流猩输入法 · 邮箱验证码');
    expect(payload.text).toContain('123456');
    expect(payload.html).toContain('123456');

    // http 模式不落明文验证码（批次 AK 教训：生产库不留明文）
    const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM mail_outbox').first<{ n: number }>();
    expect(Number(row?.n ?? 0)).toBe(0);
  });

  it('MAIL_FROM_NAME 存在时 from 拼成「显示名 <地址>」', async () => {
    const { calls, fetchImpl } = okResponder();
    await sendVerificationMail(
      makeEnv({ MAIL_FROM_NAME: '火流猩输入法' }),
      'user@example.com',
      '123456',
      fetchImpl,
    );
    expect(payloadOf(calls[0]).from).toBe('火流猩输入法 <no-reply@aiec.fun>');
  });

  it('端点 / 鉴权头 / 前缀可配（换服务商不改代码）', async () => {
    const { calls, fetchImpl } = okResponder();
    await sendVerificationMail(
      makeEnv({
        MAIL_HTTP_ENDPOINT: 'https://mail.example.net/v1/send',
        MAIL_HTTP_AUTH_HEADER: 'X-Api-Key',
        MAIL_HTTP_AUTH_PREFIX: '',
      }),
      'user@example.com',
      '123456',
      fetchImpl,
    );
    expect(calls[0].url).toBe('https://mail.example.net/v1/send');
    expect(headersOf(calls[0])['X-Api-Key']).toBe('re_test_key_0123456789');
  });

  it('缺 API key ⇒ 500 server_misconfigured，且一个请求都不发', async () => {
    const { calls, fetchImpl } = okResponder();
    await expect(
      sendVerificationMail(makeEnv({ MAIL_HTTP_API_KEY: '' }), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(0);
  });

  it('端点非 HTTPS ⇒ 500 server_misconfigured，且一个请求都不发', async () => {
    const { calls, fetchImpl } = okResponder();
    await expect(
      sendVerificationMail(
        makeEnv({ MAIL_HTTP_ENDPOINT: 'http://api.resend.com/emails' }),
        'user@example.com',
        '123456',
        fetchImpl,
      ),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(0);
  });

  it('MAIL_FROM 非法 ⇒ 500 server_misconfigured，且一个请求都不发', async () => {
    const { calls, fetchImpl } = okResponder();
    await expect(
      sendVerificationMail(makeEnv({ MAIL_FROM: 'not-an-address' }), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(0);
  });

  it('401 ⇒ 500 server_misconfigured，且业务错误不重试（只调 1 次）', async () => {
    const { calls, fetchImpl } = statusResponder(401);
    await expect(
      sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(1);
  });

  it('403 ⇒ 500 server_misconfigured，且只调 1 次', async () => {
    const { calls, fetchImpl } = statusResponder(403);
    await expect(
      sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(1);
  });

  it('429 ⇒ 503 mail_rate_limited，且只调 1 次', async () => {
    const { calls, fetchImpl } = statusResponder(429);
    await expect(
      sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 503, code: 'mail_rate_limited' });
    expect(calls.length).toBe(1);
  });

  it('500 ⇒ 503 mail_send_failed，且只调 1 次', async () => {
    const { calls, fetchImpl } = statusResponder(500);
    await expect(
      sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 503, code: 'mail_send_failed' });
    expect(calls.length).toBe(1);
  });

  it('网络异常 ⇒ 重试一次后 503 mail_send_failed（共 2 次调用）', async () => {
    const calls: Call[] = [];
    const fetchImpl: FetchLike = async (url, init) => {
      calls.push({ url, init });
      throw new TypeError('network down');
    };
    await expect(
      sendVerificationMail(makeEnv(), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 503, code: 'mail_send_failed' });
    expect(calls.length).toBe(2);
  });

  it('重置口令邮件：主题与正文都点明「重置口令」，与注册验证码模板区分开', async () => {
    const { calls, fetchImpl } = okResponder();
    await sendPasswordResetMail(makeEnv(), 'user@example.com', '654321', fetchImpl);

    const payload = payloadOf(calls[0]);
    expect(payload.subject).toBe(RESET_SUBJECT);
    expect(payload.subject).not.toBe(MAIL_SUBJECT);
    expect(payload.text).toContain('重置口令验证码是：654321');
    expect(payload.html).toContain('重置口令验证码');
  });

  it('MAIL_MODE 非法 ⇒ 500 server_misconfigured', async () => {
    const { calls, fetchImpl } = okResponder();
    await expect(
      sendVerificationMail(makeEnv({ MAIL_MODE: 'smtp' }), 'user@example.com', '123456', fetchImpl),
    ).rejects.toMatchObject({ status: 500, code: 'server_misconfigured' });
    expect(calls.length).toBe(0);
  });
});
