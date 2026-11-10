// 验证码发信通道。
//
// 首版通道决策见 docs/research/fluxing-cloud-2026-11/email-verify-channel.md：
//   - 生产：Cloudflare Email Service 的 Workers `send_email` 绑定（需 Paid 计划）。
//   - 本地 / 测试：MAIL_MODE="outbox"，只写 mail_outbox 表，不发信。
// 生产路径绝不存在「读验证码」的入口：mail_outbox 只在 outbox 模式被写，且生产不读它。

import type { Env } from './env';
import { fail } from './errors';
import { nowSeconds } from './crypto';

export const MAIL_SUBJECT = '火流猩输入法 · 邮箱验证码';

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

export async function sendVerificationMail(env: Env, to: string, code: string): Promise<void> {
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