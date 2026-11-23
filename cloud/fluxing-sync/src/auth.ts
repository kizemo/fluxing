// 认证：注册 / 邮箱验证 / 验证码重发 / 登录 / 续期 / 请求鉴权。
//
// 安全不变量（spec 100 + plan §3.2）：
//   1. 服务端永不见明文口令、永不见 enc_key；只收到客户端派生的 auth_key。
//   2. 落库的是 verifier = HMAC-SHA256(AUTH_PEPPER, auth_key)，不是 auth_key 本身。
//   3. AUTH_PEPPER 缺失 / 过短 ⇒ fail closed（500），绝不回退到内置默认值。
//   4. 令牌一律「不透明随机串 + 只存哈希」；撤销设备即立刻切断其全部令牌。

import type { Env } from './env';
import { ApiError, fail, isD1RowWriteQuotaError } from './errors';
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
import { sendPasswordResetMail, sendVerificationMail } from './mail';
import { enforceDeviceLimit } from './devices';

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

// ---------------------------------------------------------------- ADR 0076 D3 上限
//
// D3-3：refresh_tokens 保留窗口（30 天）+ 单次 GC 上限（200 行）。
// D3-4：每账号「未过期且未撤销」的 refresh token 上限（64 个）。
//
// **如实标注（ADR 0076 D3-3 原文）**：DELETE 同样计入 D1 行写入 ⇒ **GC 不省额度**。
// 它解决的是「表只增不减」与「长期凭据面」，不是计费。

/** D3-3：refresh token 过期后的保留窗口（秒）：超过该窗口的行在登录 / 续期时被清理。 */
export const REFRESH_TOKEN_RETENTION_SECONDS = 30 * 24 * 3600;

/** D3-3：单次 GC 最多删除的行数（保护 D1 额度）。 */
export const REFRESH_TOKEN_GC_MAX_ROWS = 200;

/** D3-4：每账号「未过期且未撤销」的 refresh token 行数上限。 */
export const MAX_VALID_REFRESH_TOKENS = 64;

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
    // **已知设备**（同一 device_id）重新登录 ⇒ 走 UPDATE 分支 ⇒ **不新增设备行**
    // ⇒ 设备行数不变 ⇒ **不受设备上限约束**（ADR 0076 D3-1 的上限只约束「新设备」）。
    // 重新登录即恢复该设备（清掉 revoked_at），并刷新展示名与 last_seen。
    await env.DB.prepare('UPDATE devices SET name = ?, last_seen = ?, revoked_at = NULL WHERE id = ?')
      .bind(device.name, now, existing.id)
      .run();
    return existing.id;
  }

  // D3-1：**新** device_id 才受设备行数上限（32 台/账号）约束。
  // 超限时先回收「已撤销且最久未见」的设备行；回收后仍超限 ⇒ 拒绝登录
  // （429 device_limit_reached）。口径与实现见 devices.ts 的 enforceDeviceLimit。
  await enforceDeviceLimit(env, userId);

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
  // D3-4：签发新 refresh 前先把该账号的有效 refresh 行压到上限内。
  await enforceRefreshTokenLimit(env, userId);
  const accessToken = await issueToken(env, userId, deviceRowId, 'access', accessTtl);
  const refreshToken = await issueToken(env, userId, deviceRowId, 'refresh', refreshTtl);
  return { access_token: accessToken, refresh_token: refreshToken, expires_in: accessTtl };
}

/**
 * D3-4：统计该账号「未过期且未撤销」的 refresh 行数（access 行不计入本上限）。
 *
 * 口径：只算 kind='refresh' ⇒ 上限针对的是「可续期的长期凭据面」
 * （ADR 0076 D3-4：上限 64 ≫ 已观测的 22 台设备 ⇒ 不破坏单台设备的正常续期）。
 */
async function countValidRefreshTokens(env: Env, userId: string, now: number): Promise<number> {
  const row = await env.DB.prepare(
    `SELECT COUNT(*) AS n FROM refresh_tokens
      WHERE user_id = ? AND kind = 'refresh' AND revoked_at IS NULL AND expires_at > ?`,
  )
    .bind(userId, now)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/**
 * D3-4：有效 refresh token 上限护栏。
 *
 * 超限 ⇒ **撤销（标 revoked_at）最旧的有效 refresh 行**（不删行：保留审计痕迹），
 * 撤销到「签发之后仍 ≤ 上限」为止 ⇒ 长期凭据面收敛。
 * 撤销的是**别的设备**的 refresh 行时，那台设备需要重新登录一次
 * （上限 64 ≫ 正常设备数 ⇒ 正常用户碰不到这条路径）。
 */
async function enforceRefreshTokenLimit(env: Env, userId: string): Promise<void> {
  const now = nowSeconds();
  let valid = await countValidRefreshTokens(env, userId, now);
  // 目标：本次签发 1 个新 refresh 之后，有效行仍 ≤ 上限。
  while (valid > MAX_VALID_REFRESH_TOKENS - 1) {
    const oldest = await env.DB.prepare(
      `SELECT id FROM refresh_tokens
        WHERE user_id = ? AND kind = 'refresh' AND revoked_at IS NULL AND expires_at > ?
        ORDER BY created_at ASC, id ASC
        LIMIT 1`,
    )
      .bind(userId, now)
      .first<{ id: string }>();
    if (!oldest) break;
    await env.DB.prepare('UPDATE refresh_tokens SET revoked_at = ? WHERE id = ?')
      .bind(now, oldest.id)
      .run();
    valid -= 1;
  }
}

/**
 * D3-3：清理该账号「过期超过保留窗口（30 天）」的 refresh token 行。
 *
 * 触发点：登录 / 续期（各顺带一次）。每次最多删 REFRESH_TOKEN_GC_MAX_ROWS 行（保护额度）。
 * **如实标注**：DELETE 同样计入 D1 行写入 ⇒ **GC 不省额度**；
 * 它解决的是「表只增不减」与「长期凭据面」，不是计费。
 */
async function gcExpiredRefreshTokens(env: Env, userId: string): Promise<void> {
  const cutoff = nowSeconds() - REFRESH_TOKEN_RETENTION_SECONDS;
  await env.DB.prepare(
    `DELETE FROM refresh_tokens
      WHERE id IN (
        SELECT id FROM refresh_tokens
         WHERE user_id = ? AND expires_at < ?
         LIMIT ?
      )`,
  )
    .bind(userId, cutoff, REFRESH_TOKEN_GC_MAX_ROWS)
    .run();
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

/**
 * 验证码的**纯生成**（ADR 0076 D1-1）：只在内存里产出验证码与哈希，
 * **不发信、不落库**。
 *
 * 拆成「生成 / 落库」两步是 D1-2 的前提：落库挪到**发信成功之后** ⇒
 * 发信失败时**一行都不写**（不再占号、不再吃掉 60 秒重发窗口）。
 *
 * `code` 是明文 6 位码，**仅用于发信**：绝不落库、绝不进日志。
 */
export interface GeneratedVerification {
  /** 该验证码行的主键（随机 UUID）；纯生成阶段不触碰数据库。 */
  id: string;
  /** 明文 6 位码（仅用于发信）。 */
  code: string;
  /** pepper 哈希（落库用）。 */
  codeHash: string;
  /** 创建时间（秒）。 */
  createdAt: number;
  /** 过期时间（秒）。 */
  expiresAt: number;
}

async function generateVerificationCode(env: Env, email: string): Promise<GeneratedVerification> {
  const pepper = requirePepper(env);
  const now = nowSeconds();
  const code = String(randomInt(1_000_000)).padStart(6, '0');
  const hash = await codeHash(pepper, email, code, now);
  return {
    id: crypto.randomUUID(),
    code,
    codeHash: hash,
    createdAt: now,
    expiresAt: now + CODE_TTL_SECONDS,
  };
}

/**
 * 落库：把已生成的验证码写进 email_verifications。
 *
 * **只在发信成功之后调用**（ADR 0076 D1-2 / D1-3）⇒ 发信失败 ⇒ 零写入。
 */
async function persistVerificationCode(
  env: Env,
  email: string,
  generated: GeneratedVerification,
): Promise<void> {
  await env.DB.prepare(
    'INSERT INTO email_verifications (id, email, code_hash, attempts, expires_at, consumed_at, created_at) VALUES (?, ?, ?, 0, ?, NULL, ?)',
  )
    .bind(generated.id, email, generated.codeHash, generated.expiresAt, generated.createdAt)
    .run();
}

/**
 * 回滚结果：
 *   'ok'       = 残留已清干净（DELETE 成功）
 *   'degraded' = DELETE 被拒，但已用 UPDATE 把行「失效化」，效果等价于删除
 *   'failed'   = 彻底失败（库里可能还有可用残留）
 */
type RollbackOutcome = 'ok' | 'degraded' | 'failed';

/** 回滚报告：结果 + 失败原因（原始文本，由调用方脱敏后使用）。 */
interface RollbackReport {
  outcome: RollbackOutcome;
  error?: string;
}

/** 诊断文本脱敏：抹掉收件人邮箱与 6 位数字（验证码明文）。标记用 ASCII（要进 HTTP 头）。 */
function scrubForDiagnostics(text: string, email: string): string {
  let out = text.replace(/\d{6}/g, '[redacted]');
  if (email.length > 0) out = out.split(email).join('[redacted]');
  return out;
}

/**
 * 删一行。成功返回 undefined；失败返回失败原因（调用方据此降级 / 汇总）。
 */
async function deleteRow(env: Env, sql: string, id: string): Promise<string | undefined> {
  try {
    await env.DB.prepare(sql).bind(id).run();
    return undefined;
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    console.error('[fluxing-sync] 回滚删除失败：', sql, detail);
    return detail;
  }
}

/**
 * 降级兜底（users 行）：DELETE 被拒时改用 UPDATE 把 email 改成唯一的占位地址。
 * 效果 = 解除「占号」⇒ 用户用原邮箱重试不再撞 409 email_taken。
 * 该账号本来就没验证过（email_verified=0），改 email 不影响任何真实数据。
 */
async function neutralizeUser(env: Env, userId: string): Promise<string | undefined> {
  try {
    await env.DB.prepare('UPDATE users SET email = ? WHERE id = ?')
      .bind(`deleted+${userId}@invalid.local`, userId)
      .run();
    return undefined;
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    console.error('[fluxing-sync] 回滚降级（users 失效化）失败：', detail);
    return detail;
  }
}

/**
 * 降级兜底（email_verifications 行）：DELETE 被拒时把 created_at 归零。
 * 效果 = 解除 60 秒重发窗口（闸只看 created_at）⇒ 用户立刻重试不再撞 429 verify_too_frequent。
 */
async function neutralizeVerification(env: Env, verificationId: string): Promise<string | undefined> {
  try {
    await env.DB.prepare('UPDATE email_verifications SET created_at = 0 WHERE id = ?')
      .bind(verificationId)
      .run();
    return undefined;
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error);
    console.error('[fluxing-sync] 回滚降级（验证码行失效化）失败：', detail);
    return detail;
  }
}

/**
 * 回滚「发信已成功、落库失败」这一窄窗口留下的残留（ADR 0076 D1-5）。
 *
 * 2026-11-22 生产实测（BQ 批次）：失败注册后 users 行与验证码行都残留在库里 ⇒ 用户立刻重试
 * 撞 429 verify_too_frequent。当时 D1 侧 DELETE 被拒（手工 DELETE 报 Cloudflare code 7500
 * 「exceeded D1's free tier daily row write limit」）⇒ 纯 DELETE 回滚必然失败。
 * 因此实现为「DELETE 优先，被拒则降级 UPDATE 失效化」：
 *   · DELETE 成功 ⇒ 'ok'（真正零残留）
 *   · DELETE 被拒但 UPDATE 成功 ⇒ 'degraded'（行还在，但已失效：不占号、不吃重发窗口）
 *   · 两条都失败 ⇒ 'failed'（如实上报，诊断头里带 D1 原始错误）
 * 删除顺序按「子表在前、父表在后」（将来若给 email_verifications 加外键也不会撞约束）。
 *
 * 注意（ADR 0076 D1-2）：**发信失败**这条路已经不需要回滚了（零写入 ⇒ 没有残留）；
 * 本函数只在「邮件已发出、落库失败」的窄窗口里被调用。
 */
async function rollbackRegister(
  env: Env,
  userId: string,
  verificationId: string | undefined,
): Promise<RollbackReport> {
  const problems: string[] = [];
  let degraded = 0;
  let failed = 0;

  if (verificationId !== undefined) {
    const delErr = await deleteRow(env, 'DELETE FROM email_verifications WHERE id = ?', verificationId);
    if (delErr !== undefined) {
      const updErr = await neutralizeVerification(env, verificationId);
      if (updErr === undefined) {
        degraded += 1;
        problems.push(`email_verifications delete failed(${delErr}); neutralized`);
      } else {
        failed += 1;
        problems.push(`email_verifications delete failed(${delErr}); neutralize failed(${updErr})`);
      }
    }
  }

  const delUserErr = await deleteRow(env, 'DELETE FROM users WHERE id = ?', userId);
  if (delUserErr !== undefined) {
    const updErr = await neutralizeUser(env, userId);
    if (updErr === undefined) {
      degraded += 1;
      problems.push(`users delete failed(${delUserErr}); neutralized`);
    } else {
      failed += 1;
      problems.push(`users delete failed(${delUserErr}); neutralize failed(${updErr})`);
    }
  }

  if (failed > 0) return { outcome: 'failed', error: problems.join(' ; ') };
  if (degraded > 0) return { outcome: 'degraded', error: problems.join(' ; ') };
  return { outcome: 'ok' };
}

/**
 * 给错误附上回滚结果：**不改动**状态码 / 错误码 / 中文文案，
 * 只多带诊断字段，让「残留是否清干净」在响应头（X-Fluxing-Mail-Rollback）里可见。
 */
function withRollbackStatus(error: unknown, report: RollbackReport, email: string): unknown {
  if (error instanceof ApiError) {
    const detail =
      report.error === undefined ? undefined : scrubForDiagnostics(report.error, email);
    return new ApiError(
      error.status,
      error.code,
      error.message,
      error.providerStatus,
      report.outcome,
      detail,
    );
  }
  return error;
}

// ---------------------------------------------------------------- 存储故障分类
//
// ADR 0076 D0-2 / D1-4 / D1-5：
//   · D1 配额耗尽（Cloudflare code 7500）⇒ 503 storage_quota_exceeded（更具体、可运维定位）
//   · 其它存储故障 ⇒ 503 storage_unavailable（窄窗口：邮件已发出去，但验证码没落库）
//   · UNIQUE 约束冲突（users.email）⇒ 并发注册的败者 ⇒ 409 email_taken

/**
 * 是否 D1 行写入配额耗尽（⇒ 503 storage_quota_exceeded）。
 *
 * 与 errors.ts **共用同一份判据**（isD1RowWriteQuotaError）——
 * 2026-11-23 批次 BR 生产实测坐实：Worker 内 D1 绑定抛出的文本**不带** `[code: 7500]`，
 * 本文件原先那份「只认 REST 形态」的副本命中不了 ⇒ 登录等写路径对外报笼统 500。
 * 两处各留一份判据正是那次漏配的根因 ⇒ 判据只留一份。
 */
function isD1QuotaError(error: unknown): boolean {
  return isD1RowWriteQuotaError(error);
}

/** 是否 UNIQUE 约束冲突（users.email 撞车 ⇒ 同邮箱并发注册的败者）。 */
function isUniqueConstraintError(error: unknown): boolean {
  const detail = error instanceof Error ? error.message : String(error);
  return /unique constraint failed/i.test(detail);
}

/**
 * 窄窗口（发信已成功、落库失败）的对外错误（ADR 0076 D1-5）。
 *
 * 错误码分两档（**D0-2 优先于 D1-5**）：
 *   · D1 配额耗尽（code 7500）⇒ 503 storage_quota_exceeded（保留 ADR 0075 已发布的语义）
 *   · 其它存储故障 ⇒ 503 storage_unavailable + 中文文案
 *     「验证码已发送，但服务器存储暂时不可用，请稍后重试或重新获取验证码。」
 *
 * 两种都是 503、都如实说明「邮件已经发出去了」⇒ 用户手上的验证码可能用不了。
 */
function storageFailureError(error: unknown): ApiError {
  if (isD1QuotaError(error)) {
    return fail.unavailable('服务端存储写入额度已用尽，请稍后重试。', 'storage_quota_exceeded');
  }
  return fail.unavailable(
    '验证码已发送，但服务器存储暂时不可用，请稍后重试或重新获取验证码。',
    'storage_unavailable',
  );
}

// ---------------------------------------------------------------- 端点实现

export async function handleRegister(
  env: Env,
  body: Record<string, unknown>,
  doFetch?: typeof fetch,
): Promise<unknown> {
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

  // ADR 0076 D1-1 / D1-2：验证码在内存里**纯生成** ⇒ 先发信、发信成功才落库。
  const generated = await generateVerificationCode(env, email);

  try {
    await sendVerificationMail(env, email, generated.code, doFetch);
  } catch (error) {
    // **零写入**：发信失败 ⇒ users / email_verifications 一行都不写 ⇒
    // 既不占号、也不吃掉 60 秒重发窗口 ⇒ 用户可以立刻重试（根因不会被掩盖）。
    // 无残留 ⇒ 回滚诊断头固定 'ok'（头部契约保持稳定，兼容既有断言）。
    console.error('[fluxing-sync] 注册发信失败：本次注册零写入（未落库，无残留）。');
    throw withRollbackStatus(error, { outcome: 'ok' }, email);
  }

  // 落库顺序固定「先 users 后 email_verifications」（D1-4）：
  // 同邮箱并发注册时败者卡在 users 的 UNIQUE(email) ⇒ 映射成 409 email_taken，
  // 且不会留下验证码孤儿行。
  try {
    await env.DB.prepare(
      'INSERT INTO users (id, email, verifier, salt, kdf_algo, kdf_iters, email_verified, created_at) VALUES (?, ?, ?, ?, ?, ?, 0, ?)',
    )
      .bind(userId, email, verifier, salt, kdf.algo, kdf.iters, now)
      .run();
  } catch (error) {
    if (isUniqueConstraintError(error)) {
      // 并发注册：另一请求已占用该邮箱 ⇒ 409 email_taken。
      // 本次请求一行都没写 ⇒ 无残留 ⇒ 回滚诊断头 'ok'。
      console.error('[fluxing-sync] 注册并发落库冲突：该邮箱已被另一请求占用。');
      throw withRollbackStatus(
        fail.conflict('该邮箱已注册，请直接登录。', 'email_taken'),
        { outcome: 'ok' },
        email,
      );
    }
    // D1-5 窄窗口：邮件已发出去，但 users 行没写进去 ⇒ 本次仍然零残留（无行可回滚）。
    console.error('[fluxing-sync] 注册落库失败（窄窗口：邮件已发送）。');
    throw withRollbackStatus(storageFailureError(error), { outcome: 'ok' }, email);
  }

  try {
    await persistVerificationCode(env, email, generated);
  } catch (error) {
    // D1-5 窄窗口：users 行已写入、验证码行没写进去 ⇒ 尽力回滚 users 行
    //（DELETE 优先、被拒则 UPDATE 失效化降级 ⇒ 三态 ok|degraded|failed）。
    console.error(
      '[fluxing-sync] 注册验证码落库失败（窄窗口：邮件已发送），已回滚本次注册残留。',
    );
    const report = await rollbackRegister(env, userId, generated.id);
    throw withRollbackStatus(storageFailureError(error), report, email);
  }

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

export async function handleResend(
  env: Env,
  body: Record<string, unknown>,
  doFetch?: typeof fetch,
): Promise<unknown> {
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

  // 频率闸口径不变（只看 email_verifications 的既有行 ⇒ D1-6）。
  await assertResendAllowed(env, email);

  // ADR 0076 D1-1 / D1-3：验证码在内存里纯生成 ⇒ 先发信、成功才落库。
  const generated = await generateVerificationCode(env, email);

  try {
    await sendVerificationMail(env, email, generated.code, doFetch);
  } catch (error) {
    // **零写入**：发信失败 ⇒ 本次一行都不写；
    // **旧验证码行保持不变、仍然有效**（与今天「失败只删本次行」的语义一致）。
    // 无残留 ⇒ 回滚诊断头固定 'ok'。
    console.error('[fluxing-sync] 重发发信失败：本次重发零写入（旧验证码行仍有效）。');
    throw withRollbackStatus(error, { outcome: 'ok' }, email);
  }

  try {
    await persistVerificationCode(env, email, generated);
  } catch (error) {
    // D1-5 窄窗口：邮件已发出去但新码没落库 ⇒ 用户手上的新码用不了；
    // 旧码行仍在、仍有效 ⇒ 没有残留可回滚（回滚状态 'ok'）。
    console.error('[fluxing-sync] 重发验证码落库失败（窄窗口：邮件已发送）。');
    throw withRollbackStatus(storageFailureError(error), { outcome: 'ok' }, email);
  }

  return { ok: true };
}

// ---------------------------------------------------------------- 重置口令
//
// 「忘记密码 / 重置口令」的两段式流程：
//   POST /v1/auth/password-reset/start   { email }                    → { ok, email }
//   POST /v1/auth/password-reset/confirm { email, code, auth_key }    → { ok, email }
//
// 安全不变量（与注册流程同一套红线）：
//   1. 服务端永不见明文口令、永不见 enc_key；confirm 收的是客户端派生好的新 auth_key。
//   2. 落库仍是 verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))。
//   3. 重置码落在**独立表** password_resets（见 migrations/0002_password_resets.sql），
//      与注册码的 email_verifications 物理分表 ⇒ 两类码互不通用（硬要求）。
//      因此 handleVerify 一行都不用改，注册流程语义逐字不变。
//   4. 限流口径与注册码一致：60 秒重发窗口 / 10 分钟 TTL / 5 次错误上限，
//      但窗口只看 password_resets ⇒ 与注册码的窗口互不影响。

/** 重置码哈希：前缀串与注册码（"email-verify-v1|..."）不同 ⇒ 跨用途绝不可能通用。 */
async function resetCodeHash(
  pepper: Uint8Array,
  email: string,
  code: string,
  createdAt: number,
): Promise<string> {
  return toBase64(await hmacSha256(pepper, utf8(`password-reset-v1|${email}|${code}|${createdAt}`)));
}

/** 重置码的重发节流：只看 password_resets，注册码不参与。 */
async function assertResetAllowed(env: Env, email: string): Promise<void> {
  const latest = await env.DB.prepare(
    'SELECT created_at FROM password_resets WHERE email = ? ORDER BY created_at DESC LIMIT 1',
  )
    .bind(email)
    .first<{ created_at: number }>();
  if (latest) {
    const elapsed = nowSeconds() - latest.created_at;
    if (elapsed < RESEND_INTERVAL_SECONDS) {
      const wait = RESEND_INTERVAL_SECONDS - elapsed;
      throw fail.tooFrequent(`验证码发送过于频繁，请 ${wait} 秒后再试。`, 'reset_too_frequent');
    }
  }
}

async function issueResetCode(env: Env, email: string): Promise<void> {
  const pepper = requirePepper(env);
  await assertResetAllowed(env, email);
  const now = nowSeconds();
  const code = String(randomInt(1_000_000)).padStart(6, '0');
  const hash = await resetCodeHash(pepper, email, code, now);
  await env.DB.prepare(
    'INSERT INTO password_resets (id, email, code_hash, attempts, expires_at, consumed_at, created_at) VALUES (?, ?, ?, 0, ?, NULL, ?)',
  )
    .bind(crypto.randomUUID(), email, hash, now + CODE_TTL_SECONDS, now)
    .run();
  await sendPasswordResetMail(env, email, code);
}

/**
 * 发起重置口令：查账号 → 发重置码。
 *
 * 不存在的邮箱**统一返回成功**（与 handleLogin 的防枚举口径一致）：
 * 不发信、不落库，响应与真实账号完全同形 ⇒ 接口不泄露「哪些邮箱已注册」。
 * 代价是用户会一直等一封不会到的信；这是为换取不泄露账号存在性而刻意接受的。
 */
export async function handlePasswordResetStart(
  env: Env,
  body: Record<string, unknown>,
): Promise<unknown> {
  requirePepper(env);
  const email = normalizeEmail(body.email);

  const user = await env.DB.prepare('SELECT id FROM users WHERE email = ?')
    .bind(email)
    .first<{ id: string }>();
  if (!user) return { ok: true, email };

  await issueResetCode(env, email);

  return { ok: true, email };
}

/**
 * 确认重置口令：校验 purpose='reset' 的验证码（独立表 ⇒ 注册码进不来），
 * 成功后把 users.verifier 换成 HMAC(AUTH_PEPPER, 新 auth_key)。
 */
export async function handlePasswordResetConfirm(
  env: Env,
  body: Record<string, unknown>,
): Promise<unknown> {
  const pepper = requirePepper(env);
  const email = normalizeEmail(body.email);
  const rawCode = body.code;
  if (typeof rawCode !== 'string') throw fail.badRequest('验证码（code）必须是字符串。');
  const code = rawCode.trim();
  if (!/^\d{6}$/.test(code)) throw fail.badRequest('验证码格式不正确（应为 6 位数字）。');
  const authKey = requireAuthKey(body.auth_key);

  const now = nowSeconds();
  const row = await env.DB.prepare(
    'SELECT id, code_hash, attempts, expires_at, consumed_at, created_at FROM password_resets WHERE email = ? ORDER BY created_at DESC LIMIT 1',
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

  if (!row) throw fail.badRequest('没有待用的重置验证码，请重新获取。', 'reset_no_code');
  if (row.consumed_at !== null) throw fail.badRequest('重置验证码已使用，请重新获取。', 'reset_code_used');
  if (now > row.expires_at) throw fail.badRequest('重置验证码已过期，请重新获取。', 'reset_code_expired');
  if (row.attempts >= MAX_VERIFY_ATTEMPTS) {
    throw fail.badRequest('重置验证码错误次数过多，请重新获取。', 'reset_code_locked');
  }

  const expected = await resetCodeHash(pepper, email, code, row.created_at);
  if (!sameString(expected, row.code_hash)) {
    await env.DB.prepare('UPDATE password_resets SET attempts = attempts + 1 WHERE id = ?')
      .bind(row.id)
      .run();
    const left = MAX_VERIFY_ATTEMPTS - (row.attempts + 1);
    throw fail.badRequest(
      left > 0 ? `验证码不正确，还可尝试 ${left} 次。` : '验证码不正确，且已达错误次数上限，请重新获取。',
      'reset_code_mismatch',
    );
  }

  // 账号是否存在放在**验证码校验之后**判断：先查账号会让错误码泄露「该邮箱已注册」。
  const user = await env.DB.prepare('SELECT id FROM users WHERE email = ?')
    .bind(email)
    .first<{ id: string }>();
  if (!user) throw fail.badRequest('该邮箱尚未注册，无法重置口令。', 'reset_no_account');

  // 先消费验证码再改口令：改口令失败时代码已作废 ⇒ 宁可让用户重新申请，也不留可重放的码。
  await env.DB.prepare('UPDATE password_resets SET consumed_at = ? WHERE id = ?')
    .bind(now, row.id)
    .run();
  const verifier = await computeVerifier(pepper, authKey);
  await env.DB.prepare('UPDATE users SET verifier = ? WHERE email = ?').bind(verifier, email).run();
  // 口令已变 ⇒ 该账号全部令牌（access / refresh）立即失效，客户端必须重新登录。
  await env.DB.prepare(
    'UPDATE refresh_tokens SET revoked_at = ? WHERE user_id = ? AND revoked_at IS NULL',
  )
    .bind(now, user.id)
    .run();

  return { ok: true, email };
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

  // D3-3：登录时顺带 GC 该账号过期超过保留窗口的 refresh token 行。
  await gcExpiredRefreshTokens(env, user.id);

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

  // D3-3：续期时顺带 GC 该账号过期超过保留窗口的 refresh token 行。
  await gcExpiredRefreshTokens(env, row.user_id);

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