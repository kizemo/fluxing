// 认证：注册 / 邮箱验证 / 验证码重发 / 登录 / 续期 / 请求鉴权。
//
// 安全不变量（spec 100 + plan §3.2）：
//   1. 服务端永不见明文口令、永不见 enc_key；只收到客户端派生的 auth_key。
//   2. 落库的是 verifier = HMAC-SHA256(AUTH_PEPPER, auth_key)，不是 auth_key 本身。
//   3. AUTH_PEPPER 缺失 / 过短 ⇒ fail closed（500），绝不回退到内置默认值。
//   4. 令牌一律「不透明随机串 + 只存哈希」；撤销设备即立刻切断其全部令牌。

import type { Env } from './env';
import { fail } from './errors';
import {
  hmacSha256,
  newOpaqueToken,
  nowSeconds,
  randomBytes,
  randomInt,
  sha256,
  timingSafeEqual,
  toBase64,
  utf8,
} from './crypto';
import { sendVerificationMail } from './mail';

const EMAIL_RE = /^[^\s@]{1,64}@[^\s@.]+(?:\.[^\s@.]+)+$/;
const MAX_EMAIL_LENGTH = 254;
const MAX_AUTH_KEY_LENGTH = 1024;
const MAX_DEVICE_BYTES = 256;

const CODE_TTL_SECONDS = 600;
const RESEND_INTERVAL_SECONDS = 60;
const MAX_VERIFY_ATTEMPTS = 5;

const DEFAULT_ACCESS_TTL = 3600;
const DEFAULT_REFRESH_TTL = 30 * 24 * 3600;
const DEFAULT_KDF_ALGO = 'PBKDF2-SHA256';
const DEFAULT_KDF_ITERS = 600000;
/** 设备 last_seen 的写放大抑制窗口（秒）：同一设备 5 分钟内不重复写 */
const LAST_SEEN_REFRESH_SECONDS = 300;

export interface AuthContext {
  userId: string;
  deviceRowId: string;
  emailVerified: boolean;
}

export interface IssuedTokens {
  access_token: string;
  refresh_token: string;
  expires_in: number;
}

export interface KdfParams {
  algo: string;
  iters: number;
}

interface UserRow {
  id: string;
  verifier: string;
  salt: string;
  kdf_algo: string;
  kdf_iters: number;
  email_verified: number;
}

interface DeviceInput {
  id: string;
  name: string;
}

// ---------------------------------------------------------------- 基础工具

function requirePepper(env: Env): Uint8Array {
  const pepper = env.AUTH_PEPPER;
  if (typeof pepper !== 'string' || pepper.length < 16) {
    throw fail.serverMisconfigured(
      '服务端未配置登录 pepper（AUTH_PEPPER）或长度不足 16 个字符，已拒绝处理认证请求。',
    );
  }
  return utf8(pepper);
}

function normalizeEmail(raw: unknown): string {
  if (typeof raw !== 'string') throw fail.badRequest('邮箱（email）必须是字符串。');
  const email = raw.trim().toLowerCase();
  if (email.length === 0) throw fail.badRequest('邮箱（email）不能为空。');
  if (email.length > MAX_EMAIL_LENGTH) throw fail.badRequest('邮箱（email）过长。');
  if (!EMAIL_RE.test(email)) throw fail.badRequest('邮箱（email）格式不正确。');
  return email;
}

function requireAuthKey(raw: unknown): string {
  if (typeof raw !== 'string') throw fail.badRequest('登录凭证（auth_key）必须是字符串。');
  if (raw.length === 0) throw fail.badRequest('登录凭证（auth_key）不能为空。');
  if (raw.length > MAX_AUTH_KEY_LENGTH) throw fail.badRequest('登录凭证（auth_key）过长。');
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u001f\u007f]/.test(raw)) {
    throw fail.badRequest('登录凭证（auth_key）不得包含控制字符。');
  }
  return raw;
}

function requireDeviceString(raw: unknown, field: string): string {
  if (typeof raw !== 'string') throw fail.badRequest(`设备标识（${field}）必须是字符串。`);
  const value = raw.trim();
  if (value.length === 0) throw fail.badRequest(`设备标识（${field}）不能为空。`);
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u001f\u007f]/.test(value)) {
    throw fail.badRequest(`设备标识（${field}）不得包含控制字符。`);
  }
  if (utf8(value).length > MAX_DEVICE_BYTES) {
    throw fail.badRequest(`设备标识（${field}）过长（上限 ${MAX_DEVICE_BYTES} 字节）。`);
  }
  return value;
}

/**
 * 解析 `device` 字段。
 * 规范形态：字符串 = 客户端稳定的设备标识（同时用作展示名）；
 * 也可传 `{ id, name }` 把稳定标识与展示名分开。
 */
function parseDevice(raw: unknown): DeviceInput {
  if (raw === undefined || raw === null) {
    throw fail.badRequest('缺少设备标识（device）。');
  }
  if (typeof raw === 'string') {
    const id = requireDeviceString(raw, 'device');
    return { id, name: id };
  }
  if (typeof raw === 'object' && !Array.isArray(raw)) {
    const obj = raw as Record<string, unknown>;
    const idRaw = obj.id ?? obj.device_id;
    const id = requireDeviceString(idRaw, 'device.id');
    const name = obj.name === undefined ? id : requireDeviceString(obj.name, 'device.name');
    return { id, name };
  }
  throw fail.badRequest('设备标识（device）必须是字符串或 { id, name } 对象。');
}

function intVar(raw: string | undefined, fallback: number, min: number, max: number): number {
  if (raw === undefined) return fallback;
  const parsed = Number.parseInt(raw, 10);
  if (!Number.isFinite(parsed) || parsed < min || parsed > max) return fallback;
  return parsed;
}

function kdfParams(env: Env): KdfParams {
  const algo = typeof env.KDF_ALGO === 'string' && env.KDF_ALGO.length > 0 ? env.KDF_ALGO : DEFAULT_KDF_ALGO;
  const iters = intVar(env.KDF_ITERS, DEFAULT_KDF_ITERS, 100000, 4000000);
  return { algo, iters };
}

async function computeVerifier(pepper: Uint8Array, authKey: string): Promise<string> {
  return toBase64(await hmacSha256(pepper, utf8(authKey)));
}

async function hashToken(token: string): Promise<string> {
  return toBase64(await sha256(utf8(token)));
}

async function codeHash(pepper: Uint8Array, email: string, code: string, createdAt: number): Promise<string> {
  return toBase64(await hmacSha256(pepper, utf8(`email-verify-v1|${email}|${code}|${createdAt}`)));
}

function sameString(a: string, b: string): boolean {
  return timingSafeEqual(utf8(a), utf8(b));
}

// ---------------------------------------------------------------- 设备 / 令牌

async function upsertDevice(env: Env, userId: string, device: DeviceInput): Promise<string> {
  const now = nowSeconds();
  const existing = await env.DB.prepare(
    'SELECT id FROM devices WHERE user_id = ? AND device_id = ?',
  )
    .bind(userId, device.id)
    .first<{ id: string }>();

  if (existing) {
    // 重新登录即恢复该设备（清掉 revoked_at），并刷新展示名与 last_seen
    await env.DB.prepare('UPDATE devices SET name = ?, last_seen = ?, revoked_at = NULL WHERE id = ?')
      .bind(device.name, now, existing.id)
      .run();
    return existing.id;
  }

  const id = crypto.randomUUID();
  await env.DB.prepare(
    'INSERT INTO devices (id, user_id, name, device_id, last_seen, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, NULL, ?)',
  )
    .bind(id, userId, device.name, device.id, now, now)
    .run();
  return id;
}

async function issueToken(
  env: Env,
  userId: string,
  deviceRowId: string,
  kind: 'access' | 'refresh',
  ttlSeconds: number,
): Promise<string> {
  const token = newOpaqueToken();
  const hash = await hashToken(token);
  const now = nowSeconds();
  await env.DB.prepare(
    'INSERT INTO refresh_tokens (id, user_id, device_id, kind, token_hash, expires_at, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, ?, NULL, ?)',
  )
    .bind(crypto.randomUUID(), userId, deviceRowId, kind, hash, now + ttlSeconds, now)
    .run();
  return token;
}

async function issueTokenPair(env: Env, userId: string, deviceRowId: string): Promise<IssuedTokens> {
  const accessTtl = intVar(env.ACCESS_TOKEN_TTL, DEFAULT_ACCESS_TTL, 60, 24 * 3600);
  const refreshTtl = intVar(env.REFRESH_TOKEN_TTL, DEFAULT_REFRESH_TTL, 3600, 365 * 24 * 3600);
  const accessToken = await issueToken(env, userId, deviceRowId, 'access', accessTtl);
  const refreshToken = await issueToken(env, userId, deviceRowId, 'refresh', refreshTtl);
  return { access_token: accessToken, refresh_token: refreshToken, expires_in: accessTtl };
}

// ---------------------------------------------------------------- 验证码

async function assertResendAllowed(env: Env, email: string): Promise<void> {
  const latest = await env.DB.prepare(
    'SELECT created_at FROM email_verifications WHERE email = ? ORDER BY created_at DESC LIMIT 1',
  )
    .bind(email)
    .first<{ created_at: number }>();
  if (latest) {
    const elapsed = nowSeconds() - latest.created_at;
    if (elapsed < RESEND_INTERVAL_SECONDS) {
      const wait = RESEND_INTERVAL_SECONDS - elapsed;
      throw fail.tooFrequent(`验证码发送过于频繁，请 ${wait} 秒后再试。`, 'verify_too_frequent');
    }
  }
}

async function issueVerificationCode(env: Env, email: string): Promise<void> {
  const pepper = requirePepper(env);
  await assertResendAllowed(env, email);
  const now = nowSeconds();
  const code = String(randomInt(1_000_000)).padStart(6, '0');
  const hash = await codeHash(pepper, email, code, now);
  await env.DB.prepare(
    'INSERT INTO email_verifications (id, email, code_hash, attempts, expires_at, consumed_at, created_at) VALUES (?, ?, ?, 0, ?, NULL, ?)',
  )
    .bind(crypto.randomUUID(), email, hash, now + CODE_TTL_SECONDS, now)
    .run();
  await sendVerificationMail(env, email, code);
}

// ---------------------------------------------------------------- 端点实现

export async function handleRegister(env: Env, body: Record<string, unknown>): Promise<unknown> {
  const pepper = requirePepper(env);
  const email = normalizeEmail(body.email);
  const authKey = requireAuthKey(body.auth_key);
  const device = parseDevice(body.device);
  void device; // 设备在验证通过后的首次登录才落库，注册阶段不建

  await assertResendAllowed(env, email);

  const existing = await env.DB.prepare('SELECT id FROM users WHERE email = ?')
    .bind(email)
    .first<{ id: string }>();
  if (existing) throw fail.conflict('该邮箱已注册，请直接登录。', 'email_taken');

  const verifier = await computeVerifier(pepper, authKey);
  const salt = toBase64(randomBytes(16));
  const kdf = kdfParams(env);
  const userId = crypto.randomUUID();
  const now = nowSeconds();

  await env.DB.prepare(
    'INSERT INTO users (id, email, verifier, salt, kdf_algo, kdf_iters, email_verified, created_at) VALUES (?, ?, ?, ?, ?, ?, 0, ?)',
  )
    .bind(userId, email, verifier, salt, kdf.algo, kdf.iters, now)
    .run();

  await issueVerificationCode(env, email);

  return {
    account_id: userId,
    salt,
    kdf: { algo: kdf.algo, iters: kdf.iters },
    need_email_verify: true,
  };
}

export async function handleVerify(env: Env, body: Record<string, unknown>): Promise<unknown> {
  const pepper = requirePepper(env);
  const email = normalizeEmail(body.email);
  const rawCode = body.code;
  if (typeof rawCode !== 'string') throw fail.badRequest('验证码（code）必须是字符串。');
  const code = rawCode.trim();
  if (!/^\d{6}$/.test(code)) throw fail.badRequest('验证码格式不正确（应为 6 位数字）。');

  const now = nowSeconds();
  const row = await env.DB.prepare(
    'SELECT id, code_hash, attempts, expires_at, consumed_at, created_at FROM email_verifications WHERE email = ? ORDER BY created_at DESC LIMIT 1',
  )
    .bind(email)
    .first<{
      id: string;
      code_hash: string;
      attempts: number;
      expires_at: number;
      consumed_at: number | null;
      created_at: number;
    }>();

  if (!row) throw fail.badRequest('没有待验证的验证码，请重新获取。', 'verify_no_code');
  if (row.consumed_at !== null) throw fail.badRequest('验证码已使用，请重新获取。', 'verify_code_used');
  if (now > row.expires_at) throw fail.badRequest('验证码已过期，请重新获取。', 'verify_code_expired');
  if (row.attempts >= MAX_VERIFY_ATTEMPTS) {
    throw fail.badRequest('验证码错误次数过多，请重新获取。', 'verify_code_locked');
  }

  const expected = await codeHash(pepper, email, code, row.created_at);
  if (!sameString(expected, row.code_hash)) {
    await env.DB.prepare('UPDATE email_verifications SET attempts = attempts + 1 WHERE id = ?')
      .bind(row.id)
      .run();
    const left = MAX_VERIFY_ATTEMPTS - (row.attempts + 1);
    throw fail.badRequest(
      left > 0 ? `验证码不正确，还可尝试 ${left} 次。` : '验证码不正确，且已达错误次数上限，请重新获取。',
      'verify_code_mismatch',
    );
  }

  await env.DB.prepare('UPDATE email_verifications SET consumed_at = ? WHERE id = ?')
    .bind(now, row.id)
    .run();
  await env.DB.prepare('UPDATE users SET email_verified = 1 WHERE email = ?').bind(email).run();

  return { ok: true };
}

export async function handleResend(env: Env, body: Record<string, unknown>): Promise<unknown> {
  // 缺 pepper 一律 fail closed（与其他认证端点一致）
  requirePepper(env);
  const email = normalizeEmail(body.email);

  const user = await env.DB.prepare('SELECT id, email_verified FROM users WHERE email = ?')
    .bind(email)
    .first<{ id: string; email_verified: number }>();
  if (!user) throw fail.notFound('该邮箱尚未注册，请先注册。', 'email_not_found');
  if (user.email_verified === 1) {
    throw fail.conflict('该邮箱已完成验证，无需重新获取验证码。', 'email_already_verified');
  }

  // issueVerificationCode 自带 60 秒频率闸（verify_too_frequent）、10 分钟 TTL、
  // 5 次错误上限与 code_hash 落库；MAIL_MODE="outbox" 时只写 mail_outbox 不发信。
  await issueVerificationCode(env, email);

  return { ok: true };
}

export async function handleLogin(env: Env, body: Record<string, unknown>): Promise<unknown> {
  const pepper = requirePepper(env);
  const email = normalizeEmail(body.email);
  const authKey = requireAuthKey(body.auth_key);
  const device = parseDevice(body.device);

  const user = await env.DB.prepare(
    'SELECT id, verifier, salt, kdf_algo, kdf_iters, email_verified FROM users WHERE email = ?',
  )
    .bind(email)
    .first<UserRow>();

  if (!user) {
    // 无论邮箱是否存在都跑一次 HMAC，让两条路径的耗时接近，避免邮箱枚举
    await computeVerifier(pepper, authKey);
    throw fail.unauthorized('邮箱或登录凭证不正确。', 'invalid_credentials');
  }

  const computed = await computeVerifier(pepper, authKey);
  if (!sameString(computed, user.verifier)) {
    throw fail.unauthorized('邮箱或登录凭证不正确。', 'invalid_credentials');
  }

  const deviceRowId = await upsertDevice(env, user.id, device);
  const tokens = await issueTokenPair(env, user.id, deviceRowId);

  return {
    ...tokens,
    salt: user.salt,
    kdf: { algo: user.kdf_algo, iters: user.kdf_iters },
    account_id: user.id,
    // 追加字段（plan §6.1 未列）：客户端据此提示「请先完成邮箱验证」，否则未验证状态下
    // 推送会被 403 拦住而没有可解释的原因。
    email_verified: user.email_verified === 1,
  };
}

export async function handleRefresh(env: Env, body: Record<string, unknown>): Promise<unknown> {
  const raw = body.refresh_token;
  if (typeof raw !== 'string' || raw.length === 0) {
    throw fail.badRequest('缺少刷新令牌（refresh_token）。');
  }

  const hash = await hashToken(raw);
  const row = await env.DB.prepare(
    'SELECT id, user_id, device_id, kind, expires_at, revoked_at FROM refresh_tokens WHERE token_hash = ?',
  )
    .bind(hash)
    .first<{
      id: string;
      user_id: string;
      device_id: string;
      kind: string;
      expires_at: number;
      revoked_at: number | null;
    }>();

  if (!row || row.kind !== 'refresh') {
    throw fail.unauthorized('刷新令牌无效，请重新登录。', 'invalid_refresh_token');
  }
  if (row.revoked_at !== null) {
    throw fail.unauthorized('刷新令牌已失效，请重新登录。', 'refresh_token_revoked');
  }
  if (nowSeconds() > row.expires_at) {
    throw fail.unauthorized('刷新令牌已过期，请重新登录。', 'refresh_token_expired');
  }

  const device = await env.DB.prepare('SELECT revoked_at FROM devices WHERE id = ?')
    .bind(row.device_id)
    .first<{ revoked_at: number | null }>();
  if (!device || device.revoked_at !== null) {
    await env.DB.prepare('UPDATE refresh_tokens SET revoked_at = ? WHERE id = ?')
      .bind(nowSeconds(), row.id)
      .run();
    throw fail.unauthorized('该设备已被移除，请重新登录。', 'device_revoked');
  }

  // 令牌轮换：旧刷新令牌立即作废，避免重放
  await env.DB.prepare('UPDATE refresh_tokens SET revoked_at = ? WHERE id = ?')
    .bind(nowSeconds(), row.id)
    .run();

  return issueTokenPair(env, row.user_id, row.device_id);
}

/** 从 Authorization: Bearer 取令牌并校验；失败抛 401 中文错误。 */
export async function authenticate(env: Env, request: Request): Promise<AuthContext> {
  const header = request.headers.get('authorization');
  if (header === null) {
    throw fail.unauthorized('缺少访问令牌（Authorization: Bearer <token>）。', 'missing_token');
  }
  const matched = /^Bearer\s+(\S+)$/i.exec(header.trim());
  if (!matched) {
    throw fail.unauthorized('访问令牌格式不正确（应为 Bearer <token>）。', 'bad_token_format');
  }
  const token = matched[1] as string;
  const hash = await hashToken(token);

  const row = await env.DB.prepare(
    `SELECT t.user_id AS user_id, t.device_id AS device_id, t.expires_at AS expires_at,
            t.revoked_at AS revoked_at, t.kind AS kind,
            u.email_verified AS email_verified,
            d.revoked_at AS device_revoked_at, d.last_seen AS device_last_seen
       FROM refresh_tokens t
       JOIN users u ON u.id = t.user_id
       LEFT JOIN devices d ON d.id = t.device_id
      WHERE t.token_hash = ?`,
  )
    .bind(hash)
    .first<{
      user_id: string;
      device_id: string;
      expires_at: number;
      revoked_at: number | null;
      kind: string;
      email_verified: number;
      device_revoked_at: number | null;
      device_last_seen: number | null;
    }>();

  const now = nowSeconds();
  if (!row || row.kind !== 'access') {
    throw fail.unauthorized('访问令牌无效。', 'invalid_token');
  }
  if (row.revoked_at !== null) {
    throw fail.unauthorized('访问令牌已失效，请重新登录。', 'token_revoked');
  }
  if (now > row.expires_at) {
    throw fail.unauthorized('访问令牌已过期，请用刷新令牌续期。', 'token_expired');
  }
  if (row.device_revoked_at !== null) {
    throw fail.unauthorized('该设备已被移除，请重新登录。', 'device_revoked');
  }

  if (row.device_last_seen === null || now - row.device_last_seen >= LAST_SEEN_REFRESH_SECONDS) {
    await env.DB.prepare('UPDATE devices SET last_seen = ? WHERE id = ?').bind(now, row.device_id).run();
  }

  return {
    userId: row.user_id,
    deviceRowId: row.device_id,
    emailVerified: row.email_verified === 1,
  };
}