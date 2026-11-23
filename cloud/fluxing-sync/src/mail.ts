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
//
// 模板口径（批次 AT-2 起有两套，**不得混用**）：
//   · MAIL_SUBJECT  = 注册 / 邮箱验证验证码
//   · RESET_SUBJECT = 重置口令验证码（主题与正文都点明「重置口令」，
//     避免用户把重置码当成注册码去注册流程里用，反之亦然）。

import type { Env } from './env';
import { ApiError, fail } from './errors';
import { nowSeconds } from './crypto';

export const MAIL_SUBJECT = '火流猩输入法 · 邮箱验证码';
export const RESET_SUBJECT = '火流猩输入法 · 重置口令验证码';

/** 一封信的完整内容（主题 + 纯文本 + HTML）。 */
export interface MailMessage {
  to: string;
  subject: string;
  text: string;
  html: string;
}

const DEFAULT_HTTP_ENDPOINT = 'https://api.resend.com/emails';
const DEFAULT_AUTH_HEADER = 'Authorization';
const DEFAULT_AUTH_PREFIX = 'Bearer ';
const HTTP_TIMEOUT_MS = 10_000;
const HTTP_MAX_ATTEMPTS = 2;

/**
 * 上游错误名 ⇒ 判定为「发信通道配置异常」（我方 500 server_misconfigured）的名单。
 *
 * 判别口径：这些错误名说的是**通道/账号/参数配置**有问题（key 没配、key 被停用、
 * 端点路径不对、缺必填字段……），与「收件人被拒」是两回事 ⇒ 重试无用，要人去改配置。
 * 不在这份名单里的 4xx（如 validation_error / recipient_rejected）⇒ 按收件人侧处理。
 */
const PROVIDER_CONFIG_ERROR_NAMES = new Set([
  'missing_api_key',
  'restricted_api_key',
  'suspended_api_key',
  'invalid_permission',
  'not_found',
  'method_not_allowed',
  'missing_required_field',
  'missing_required_parameter',
  'invalid_attachment',
  'invalid_parameter',
  'invalid_idempotency_key',
  'forge_not_enabled',
  'forge_unavailable',
  'return_path_unavailable',
]);

/** 上游失败的分类结果：我方对外的 HTTP 状态码 + 稳定错误码 + 中文原因。 */
export interface UpstreamFailure {
  /** 我方对外 HTTP 状态码 */
  httpStatus: number;
  /** 我方对外错误码 */
  code: string;
  /** 我方对外中文原因 */
  message: string;
}

/**
 * 从上游响应体里解析错误名（Resend 形状：{ statusCode, name, message }）。
 * 解析不出（不是 JSON / 没有 name / name 不是字符串）⇒ 返回空串，调用方按「无 name」处理。
 */
export function parseUpstreamErrorName(bodyText: string): string {
  try {
    const parsed = JSON.parse(bodyText) as unknown;
    if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) return '';
    const name = (parsed as Record<string, unknown>).name;
    return typeof name === 'string' && name.length > 0 ? name : '';
  } catch {
    return '';
  }
}

/**
 * 上游发信失败的分类（**只做分类，不发请求**）。
 *
 * 判定顺序（逐条对应上游语义，顺序不可换）：
 *   1. 401 / 403        ⇒ 500 server_misconfigured（鉴权失败，重试无用）
 *   2. 429              ⇒ 503 mail_rate_limited（上游限流，可稍后重试）
 *   3. 409              ⇒ 503 mail_send_failed（上游语义是「稍后重试」）
 *   4. >= 500 或 < 400  ⇒ 503 mail_send_failed（上游故障 / 异常分支，可重试）
 *   5. 其余 4xx：
 *        上游错误名命中「通道配置类」名单 ⇒ 500 server_misconfigured（要人去改配置）
 *        否则 ⇒ 422 mail_recipient_rejected（收件人被拒 ⇒ **换邮箱**才有意义，别重试）
 *
 * 第 5 条的 422 是本轮新增的关键：把「永久性拒绝」与「可重试的临时故障」分开，
 * 客户端只对 429 / 5xx / 传输失败自动重试（见客户端 CloudTransport.cpp），
 * 422 不在重试集合内 ⇒ 客户端会直接把中文原因显示给用户，不会重试掩盖根因。
 */
export function classifyUpstreamFailure(status: number, bodyText: string): UpstreamFailure {
  if (status === 401 || status === 403) {
    return {
      httpStatus: 500,
      code: 'server_misconfigured',
      message: '发信服务拒绝了请求（鉴权失败），请联系管理员。',
    };
  }
  if (status === 429) {
    return {
      httpStatus: 503,
      code: 'mail_rate_limited',
      message: '验证码邮件发送过于频繁，请稍后重试。',
    };
  }
  if (status === 409) {
    return {
      httpStatus: 503,
      code: 'mail_send_failed',
      message: '验证码邮件发送失败，请稍后重试。',
    };
  }
  if (status >= 500 || status < 400) {
    return {
      httpStatus: 503,
      code: 'mail_send_failed',
      message: '验证码邮件发送失败，请稍后重试。',
    };
  }
  // 其余 4xx：先看上游错误名是不是「通道配置类」问题。
  const upstreamName = parseUpstreamErrorName(bodyText);
  if (upstreamName.length > 0 && PROVIDER_CONFIG_ERROR_NAMES.has(upstreamName)) {
    return {
      httpStatus: 500,
      code: 'server_misconfigured',
      message: '发信通道配置异常，请联系管理员。',
    };
  }
  return {
    httpStatus: 422,
    code: 'mail_recipient_rejected',
    message: '验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。',
  };
}

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

/** 重置口令验证码正文：明确点明是「重置口令」，与注册验证码区分。 */
export function resetBody(code: string): string {
  return [
    '您好，',
    '',
    `您的火流猩输入法重置口令验证码是：${code}`,
    '',
    '验证码 10 分钟内有效，请勿转告他人。',
    '如非本人申请重置口令，请忽略本邮件，您的口令不会被更改。',
    '',
    '—— 火流猩输入法',
  ].join('\n');
}

/** 重置口令验证码 HTML 版（与 resetBody 文案一致）。 */
export function resetHtml(code: string): string {
  return [
    '<div style="font-family:system-ui,-apple-system,sans-serif;font-size:15px;line-height:1.7;color:#222">',
    '<p>您好，</p>',
    `<p>您的火流猩输入法重置口令验证码是：<b style="font-size:20px;letter-spacing:2px">${escapeHtml(code)}</b></p>`,
    '<p>验证码 10 分钟内有效，请勿转告他人。</p>',
    '<p>如非本人申请重置口令，请忽略本邮件，您的口令不会被更改。</p>',
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
 *   · 409 / >=500 / <400  ⇒ 503 mail_send_failed（上游故障，可重试）
 *   · 其余 4xx            ⇒ 500 server_misconfigured（上游错误名属通道配置类）
 *                          或 422 mail_recipient_rejected（收件人被拒，重试无用）
 * 网络类失败会重试一次（HTTP_MAX_ATTEMPTS），业务类错误**不重试**。
 */

/** 从正文里取本次的 6 位验证码（模板固定为「…验证码是：123456」）。 */
function extractVerificationCode(text: string): string {
  const matched = /(\d{6})/.exec(text);
  return matched === null ? '' : matched[1];
}

/**
 * 日志脱敏：抹掉收件人邮箱、验证码明文与 API Key。
 * 上游错误体可能回显请求内容（如「Invalid `to` field」里带地址）⇒ 打印前必须脱敏。
 *
 * 导出是为了能直接单测这条红线（worker 侧的 console 输出没法在测试侧截获）。
 */
export function redactSensitive(text: string, message: MailMessage, apiKey: string): string {
  let out = text;
  const secrets = [message.to, extractVerificationCode(message.text), apiKey];
  for (const secret of secrets) {
    if (secret.length === 0) continue;
    out = out.split(secret).join('[已脱敏]');
  }
  return out;
}

async function sendViaHttp(
  env: Env,
  message: MailMessage,
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
    to: [message.to],
    subject: message.subject,
    text: message.text,
    html: message.html,
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
      // 只记录状态码、上游错误名与截断后的响应体：
      // 不含验证码明文、不含 API Key、不含收件人邮箱（上游错误体可能回显请求内容 ⇒ 打印前脱敏）。
      const upstreamName = parseUpstreamErrorName(text);
      console.error(
        `[fluxing-sync] 第三方发信接口返回异常：status=${response.status} name=${upstreamName} body=${redactSensitive(text, message, apiKey).slice(0, 300)}`,
      );
      // 业务类错误（拿到 HTTP 响应）**不重试**：直接按分类抛出去。
      const classified = classifyUpstreamFailure(response.status, text);
      throw new ApiError(classified.httpStatus, classified.code, classified.message, response.status);
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

/**
 * 按 MAIL_MODE 投递一封信（三条通道共用同一份报文）。
 *   outbox     = 只写 mail_outbox 表（本地 / 测试 / 内测）
 *   http       = 第三方 HTTPS 邮件 API（生产路线）
 *   cloudflare = Workers send_email 绑定
 */
export async function sendMail(env: Env, message: MailMessage, fetchImpl?: FetchLike): Promise<void> {
  const mode = (env.MAIL_MODE ?? 'outbox').toLowerCase();

  if (mode === 'outbox') {
    await env.DB.prepare(
      'INSERT INTO mail_outbox (to_addr, subject, body, created_at) VALUES (?, ?, ?, ?)',
    )
      .bind(message.to, message.subject, message.text, nowSeconds())
      .run();
    return;
  }

  if (mode === 'http') {
    const doFetch: FetchLike =
      fetchImpl ?? ((input: string, init: RequestInit) => fetch(input, init));
    await sendViaHttp(env, message, doFetch);
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
    const raw = buildRawMime(from, message.to, message.subject, message.text);
    await env.EMAIL.send(new EmailMessage(from, message.to, raw));
  } catch (error) {
    console.error('[fluxing-sync] 发送验证码邮件失败：', error);
    throw fail.unavailable('验证码邮件发送失败，请稍后重试。', 'mail_send_failed');
  }
}

/** 注册 / 邮箱验证验证码。 */
export async function sendVerificationMail(
  env: Env,
  to: string,
  code: string,
  fetchImpl?: FetchLike,
): Promise<void> {
  await sendMail(
    env,
    { to, subject: MAIL_SUBJECT, text: verificationBody(code), html: verificationHtml(code) },
    fetchImpl,
  );
}

/** 重置口令验证码：主题与正文都与注册验证码区分开。 */
export async function sendPasswordResetMail(
  env: Env,
  to: string,
  code: string,
  fetchImpl?: FetchLike,
): Promise<void> {
  await sendMail(
    env,
    { to, subject: RESET_SUBJECT, text: resetBody(code), html: resetHtml(code) },
    fetchImpl,
  );
}
