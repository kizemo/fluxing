// 验证码发信通道。
//
// 首版通道决策见 docs/research/fluxing-cloud-2026-11/email-verify-channel.md：
//   - 生产：Cloudflare Email Service 的 Workers `send_email` 绑定（需 Paid 计划）。
//   - 本地 / 测试：MAIL_MODE="outbox"，只写 mail_outbox 表，不发信。
// 生产路径绝不存在「读验证码」的入口：mail_outbox 只在 outbox 模式被写，且生产不读它。
//
// 2026-11-11 用户拍板走 $0 路线 ⇒ 新增 MAIL_MODE="http"：第三方 HTTPS 邮件 API
// （当前 = Resend；Workers Free 允许出站 fetch）。契约：
//   · 端点：MAIL_HTTP_ENDPOINT（默认 https://api.resend.com/emails）
//   · 鉴权：MAIL_HTTP_API_KEY 走 Workers Secret（绝不进代码库 / D1 / 日志）
//   · 请求体：{ from, to:[...], subject, text, html }（Resend 形状）
//   · 成功判定：HTTP 2xx；失败一律「中文原因 + 稳定错误码」，**不静默吞掉**
//     （批次 AK 的教训：静默降级会让用户收到「网络请求失败」却查不到原因）。
//
// 关键性质：http 模式**不写 mail_outbox** ⇒ 生产库里不留明文验证码。

import type { Env } from './env';
import { ApiError, fail } from './errors';
import { nowSeconds } from './crypto';

export const MAIL_SUBJECT = '火流猩输入法 · 邮箱验证码';

const DEFAULT_HTTP_ENDPOINT = 'https://api.resend.com/emails';
const DEFAULT_AUTH_HEADER = 'Authorization';
const DEFAULT_AUTH_PREFIX = 'Bearer ';
const HTTP_TIMEOUT_MS = 10_000;
const HTTP_MAX_ATTEMPTS = 2;

/** 可注入的 fetch 形态：生产用全局 fetch，单测注入替身（不打真实网络）。 */
export type FetchLike = (input: string, init: RequestInit) => Promise<Response>;

export function verificationBody(code: string): string {
  return [
    '您好，',
    '',
    `您的火流猩输入法账号验证码是：${code}`,
    '',
    '验证码 10 分钟内有效，请勿转告他人。',
    '本次操作由您本人发起；如非本人操作，请忽略本邮件。',
    '',
    '—— 火流猩输入法',
  ].join('\n');
}

function escapeHtml(text: string): string {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

export function verificationHtml(code: string): string {
  return [
    '<div style="font-family:system-ui,-apple-system,sans-serif;font-size:15px;line-height:1.7;color:#222">',
    '<p>您好，</p>',
    `<p>您的火流猩输入法账号验证码是：<b style="font-size:20px;letter-spacing:2px">${escapeHtml(code)}</b></p>`,
    '<p>验证码 10 分钟内有效，请勿转告他人。</p>',
    '<p>本次操作由您本人发起；如非本人操作，请忽略本邮件。</p>',
    '<p>—— 火流猩输入法</p>',
    '</div>',
  ].join('');
}

function rfc5322Date(now: Date): string {
  // 只用 UTC 形式，避免依赖宿主时区
  return now.toUTCString().replace('GMT', '+0000');
}

function buildRawMime(from: string, to: string, subject: string, body: string): string {
  // 中文主题与正文一律 base64 编码，避免 MIME 头部编码踩坑
  const encoder = new TextEncoder();
  const b64 = (s: string): string => {
    const bytes = encoder.encode(s);
    let bin = '';
    for (const byte of bytes) bin += String.fromCharCode(byte);
    return btoa(bin);
  };
  const messageId = `${crypto.randomUUID()}@${from.split('@')[1] ?? 'localhost'}`;
  return [
    `From: ${from}`,
    `To: ${to}`,
    `Subject: =?UTF-8?B?${b64(subject)}?=`,
    `Date: ${rfc5322Date(new Date())}`,
    `Message-ID: <${messageId}>`,
    'MIME-Version: 1.0',
    'Content-Type: text/plain; charset=UTF-8',
    'Content-Transfer-Encoding: base64',
    '',
    b64(body),
    '',
  ].join('\r\n');
}

/** 发件地址：MAIL_FROM 必填且必须含 '@'；MAIL_FROM_NAME 可选（拼成「名 <地址>」）。 */
function buildFromAddress(env: Env): string {
  const address = (env.MAIL_FROM ?? '').trim();
  if (address.length === 0 || !address.includes('@')) {
    throw fail.serverMisconfigured('发信地址未正确配置（MAIL_FROM）。');
  }
  const name = (env.MAIL_FROM_NAME ?? '').trim();
  return name.length > 0 ? `${name} <${address}>` : address;
}

/**
 * 第三方 HTTPS 邮件 API 发信（当前 = Resend）。
 *
 * 失败分类（全部中文 + 稳定错误码）：
 *   · 缺 key / 端点非 HTTPS ⇒ 500 server_misconfigured（配置问题，重试无用）
 *   · 401 / 403           ⇒ 500 server_misconfigured（鉴权失败）
 *   · 429                 ⇒ 503 mail_rate_limited（可稍后重试）
 *   · 其它 4xx / 5xx / 网络异常 ⇒ 503 mail_send_failed
 * 网络类失败会重试一次（HTTP_MAX_ATTEMPTS），业务类错误**不重试**。
 */
async function sendViaHttp(
  env: Env,
  to: string,
  code: string,
  fetchImpl: FetchLike,
): Promise<void> {
  const endpoint = (env.MAIL_HTTP_ENDPOINT ?? DEFAULT_HTTP_ENDPOINT).trim();
  const apiKey = (env.MAIL_HTTP_API_KEY ?? '').trim();
  if (apiKey.length === 0) {
    throw fail.serverMisconfigured('发信通道未配置（缺少 MAIL_HTTP_API_KEY）。');
  }
  if (!endpoint.startsWith('https://')) {
    throw fail.serverMisconfigured('发信端点必须使用 HTTPS（MAIL_HTTP_ENDPOINT）。');
  }

  const authHeader = (env.MAIL_HTTP_AUTH_HEADER ?? DEFAULT_AUTH_HEADER).trim() || DEFAULT_AUTH_HEADER;
  const authPrefix = env.MAIL_HTTP_AUTH_PREFIX ?? DEFAULT_AUTH_PREFIX;
  const from = buildFromAddress(env);
  const payload = {
    from,
    to: [to],
    subject: MAIL_SUBJECT,
    text: verificationBody(code),
    html: verificationHtml(code),
  };
  const body = JSON.stringify(payload);

  for (let attempt = 1; attempt <= HTTP_MAX_ATTEMPTS; attempt += 1) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), HTTP_TIMEOUT_MS);
    try {
      const response = await fetchImpl(endpoint, {
        method: 'POST',
        headers: {
          'content-type': 'application/json',
          [authHeader]: `${authPrefix}${apiKey}`,
        },
        body,
        signal: controller.signal,
      });
      const text = await response.text();
      if (response.ok) {
        console.log(`[fluxing-sync] 验证码邮件已交给第三方发信接口（attempt=${attempt}/${HTTP_MAX_ATTEMPTS}）。`);
        return;
      }
      // 只记录状态码与截断后的响应体：不含验证码、不含 API key。
      console.error(
        `[fluxing-sync] 第三方发信接口返回异常：status=${response.status} body=${text.slice(0, 300)}`,
      );
      if (response.status === 401 || response.status === 403) {
        throw fail.serverMisconfigured('发信服务拒绝了请求（鉴权失败），请联系管理员。');
      }
      if (response.status === 429) {
        throw fail.unavailable('验证码邮件发送过于频繁，请稍后重试。', 'mail_rate_limited');
      }
      throw fail.unavailable('验证码邮件发送失败，请稍后重试。', 'mail_send_failed');
    } catch (error) {
      if (error instanceof ApiError) throw error;
      console.error(
        `[fluxing-sync] 调用第三方发信接口失败（attempt=${attempt}/${HTTP_MAX_ATTEMPTS}）：`,
        error instanceof Error ? error.message : error,
      );
    } finally {
      clearTimeout(timer);
    }
  }

  throw fail.unavailable('验证码邮件发送失败，请稍后重试。', 'mail_send_failed');
}

export async function sendVerificationMail(
  env: Env,
  to: string,
  code: string,
  fetchImpl?: FetchLike,
): Promise<void> {
  const body = verificationBody(code);
  const mode = (env.MAIL_MODE ?? 'outbox').toLowerCase();

  if (mode === 'outbox') {
    await env.DB.prepare(
      'INSERT INTO mail_outbox (to_addr, subject, body, created_at) VALUES (?, ?, ?, ?)',
    )
      .bind(to, MAIL_SUBJECT, body, nowSeconds())
      .run();
    return;
  }

  if (mode === 'http') {
    const doFetch: FetchLike =
      fetchImpl ?? ((input: string, init: RequestInit) => fetch(input, init));
    await sendViaHttp(env, to, code, doFetch);
    return;
  }

  if (mode !== 'cloudflare') {
    throw fail.serverMisconfigured(`发信模式配置非法（MAIL_MODE=${env.MAIL_MODE ?? '未设置'}）。`);
  }

  const from = env.MAIL_FROM;
  if (!from || !from.includes('@') || from.includes('<')) {
    throw fail.serverMisconfigured('发信地址未正确配置（MAIL_FROM）。');
  }
  if (!env.EMAIL) {
    throw fail.unavailable('发信通道未绑定（send_email）。', 'mail_unavailable');
  }

  try {
    // Workers 内置模块；只在真正发信时加载，本地 / 测试不会走到这里
    const { EmailMessage } = await import('cloudflare:email');
    const raw = buildRawMime(from, to, MAIL_SUBJECT, body);
    await env.EMAIL.send(new EmailMessage(from, to, raw));
  } catch (error) {
    console.error('[fluxing-sync] 发送验证码邮件失败：', error);
    throw fail.unavailable('验证码邮件发送失败，请稍后重试。', 'mail_send_failed');
  }
}
