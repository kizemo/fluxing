// 设备上限 / refresh token 上限 / 过期行 GC 单测（ADR 0076 D3）。
//
// 背景：device_id 由**凭据生命周期**决定（客户端只在凭据为空时生成）⇒
// 换机 / 重建凭据 = 服务端多一台「设备」，而旧实现对 devices 表**没有任何上限**；
// refresh_tokens 表同理，只增不减 ⇒ 长期凭据面无界。
//
// ADR 0076 D3 给三条护栏：
//   D3-1 每账号设备上限 32 台；超限先回收「已撤销且最久未见」的设备行，仍超限则拒绝登录。
//   D3-4 每账号「未过期且未撤销」的 refresh token 上限 64 个；签发前撤销最旧的。
//   D3-3 refresh token 过期后保留 30 天；登录 / 续期时顺带 GC，单次最多删 200 行。
//
// 本文件锁六件事：
//   1. 设备数达 32 ⇒ 登录被拒（429 device_limit_reached + 中文文案），且不签发任何令牌。
//   2. 有已撤销设备可回收 ⇒ 回收后登录成功，设备总数收敛到 32。
//   3. 回收后仍超限 ⇒ 仍拒绝，且**活跃设备一台不少**（绝不自动删在用设备）。
//   4. 设备回收**不动** sync_items（用户的同步数据不受影响）。
//   5. 有效 refresh 超过 64 ⇒ 撤销最旧的有效行，签发后仍 ≤ 64。
//   6. 过期行 GC：30 天保留窗口 + 单次最多 200 行 ⇒ 分两次才清干净，窗口内行不删。
//
// 零真实网络：发信走 MAIL_MODE='outbox'（只写 mail_outbox，验证码从表里取）。

import { describe, it, expect } from 'vitest';
import { env } from 'cloudflare:test';
import {
  handleLogin,
  handleRefresh,
  handleRegister,
  handleVerify,
  MAX_VALID_REFRESH_TOKENS,
  REFRESH_TOKEN_GC_MAX_ROWS,
  REFRESH_TOKEN_RETENTION_SECONDS,
} from '../src/auth';
import { MAX_DEVICES_PER_ACCOUNT } from '../src/devices';
import { nowSeconds, sha256, toBase64, utf8 } from '../src/crypto';
import type { Env } from '../src/env';

interface ErrShape {
  status: number;
  code: string;
  message: string;
}

interface LoginResult {
  access_token: string;
  refresh_token: string;
}

/** env 副本：**绝不**修改共享 env 对象。 */
function makeEnv(overrides: Record<string, string | undefined> = {}): Env {
  return {
    ...(env as unknown as Env),
    MAIL_MODE: 'outbox',
    MAIL_FROM: 'no-reply@aiec.fun',
    MAIL_FROM_NAME: '',
    MAIL_HTTP_ENDPOINT: 'https://api.resend.com/emails',
    MAIL_HTTP_AUTH_HEADER: 'Authorization',
    MAIL_HTTP_AUTH_PREFIX: 'Bearer ',
    MAIL_HTTP_API_KEY: 're_test_key_0123456789',
    ...overrides,
  } as Env;
}

/** 捕获抛出的错误。 */
async function catchError(promise: Promise<unknown>): Promise<ErrShape> {
  try {
    await promise;
  } catch (error) {
    return error as ErrShape;
  }
  throw new Error('预期抛出错误，但调用成功了。');
}

/** 从 mail_outbox 取最近一封验证码邮件里的 6 位码。 */
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

/** 令牌哈希：与 auth.ts 的 hashToken 完全一致（base64(sha256(utf8(token)))。 */
async function tokenHash(token: string): Promise<string> {
  return toBase64(await sha256(utf8(token)));
}

/** 注册 + 邮箱验证 ⇒ 拿到可登录的账号（**不**建设备行：设备只在登录时创建）。 */
async function verifiedAccount(
  email: string,
): Promise<{ userId: string; authKey: string }> {
  const authKey = `auth-key-${email}`;
  const result = (await handleRegister(makeEnv(), {
    email,
    auth_key: authKey,
    device: 'seed-device-01',
  })) as { account_id: string };
  const code = await latestCode(email);
  await handleVerify(env as unknown as Env, { email, code });
  return { userId: result.account_id, authKey };
}

async function login(
  email: string,
  authKey: string,
  deviceId: string,
): Promise<LoginResult> {
  const result = (await handleLogin(makeEnv(), {
    email,
    auth_key: authKey,
    device: deviceId,
  })) as LoginResult;
  return result;
}

/**
 * 铺设备行：
 *   · active  = 未撤销（revoked_at IS NULL）的设备；last_seen 递增，越早越「旧」。
 *   · revoked = 已撤销（revoked_at 非空）的设备；last_seen 明显更早 ⇒ 回收时**优先**被选中。
 * id 统一带 prefix ⇒ 跨用例不撞主键。
 */
async function seedDevices(
  userId: string,
  prefix: string,
  active: number,
  revoked: number,
): Promise<void> {
  const now = nowSeconds();
  for (let i = 0; i < active; i += 1) {
    await env.DB.prepare(
      'INSERT INTO devices (id, user_id, name, device_id, last_seen, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, NULL, ?)',
    )
      .bind(`${prefix}-active-${i}`, userId, `${prefix}-active-${i}`, `${prefix}-active-${i}`, now - i, now - i)
      .run();
  }
  for (let i = 0; i < revoked; i += 1) {
    await env.DB.prepare(
      'INSERT INTO devices (id, user_id, name, device_id, last_seen, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)',
    )
      .bind(
        `${prefix}-revoked-${i}`,
        userId,
        `${prefix}-revoked-${i}`,
        `${prefix}-revoked-${i}`,
        now - 10000 - i,
        now - 5000,
        now - 10000 - i,
      )
      .run();
  }
}

async function countDevices(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM devices WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

async function countActiveDevices(userId: string): Promise<number> {
  const row = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM devices WHERE user_id = ? AND revoked_at IS NULL',
  )
    .bind(userId)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

async function countRevokedDevices(userId: string): Promise<number> {
  const row = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM devices WHERE user_id = ? AND revoked_at IS NOT NULL',
  )
    .bind(userId)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/** 该账号「未过期且未撤销」的 refresh 行数（D3-4 的口径）。 */
async function countValidRefresh(userId: string): Promise<number> {
  const row = await env.DB.prepare(
    `SELECT COUNT(*) AS n FROM refresh_tokens
      WHERE user_id = ? AND kind = 'refresh' AND revoked_at IS NULL AND expires_at > ?`,
  )
    .bind(userId, nowSeconds())
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

async function countTokens(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM refresh_tokens WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/** expires_at 早于 cutoff 的行数（GC 口径）。 */
async function countExpiredBefore(userId: string, cutoff: number): Promise<number> {
  const row = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM refresh_tokens WHERE user_id = ? AND expires_at < ?',
  )
    .bind(userId, cutoff)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

async function revokedAtOf(userId: string, token: string): Promise<number | null> {
  const row = await env.DB.prepare(
    'SELECT revoked_at FROM refresh_tokens WHERE user_id = ? AND token_hash = ?',
  )
    .bind(userId, await tokenHash(token))
    .first<{ revoked_at: number | null }>();
  return row?.revoked_at ?? null;
}

/** 该 token 对应的行是否仍在库里（区分「行没了」与「行在但未撤销」）。 */
async function tokenRowExists(userId: string, token: string): Promise<boolean> {
  const row = await env.DB.prepare(
    'SELECT id FROM refresh_tokens WHERE user_id = ? AND token_hash = ?',
  )
    .bind(userId, await tokenHash(token))
    .first<{ id: string }>();
  return row !== null;
}

/** 铺一批 refresh 行（kind 可选、过期时间可选）。 */
async function seedRefreshTokens(
  userId: string,
  deviceRowId: string,
  prefix: string,
  count: number,
  options: { kind?: 'access' | 'refresh'; expiresAt?: number; createdAt?: number } = {},
): Promise<void> {
  const kind = options.kind ?? 'refresh';
  const now = nowSeconds();
  const expiresAt = options.expiresAt ?? now + 30 * 24 * 3600;
  const createdAt = options.createdAt ?? now;
  for (let i = 0; i < count; i += 1) {
    const token = `${prefix}-${i}`;
    await env.DB.prepare(
      'INSERT INTO refresh_tokens (id, user_id, device_id, kind, token_hash, expires_at, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, ?, NULL, ?)',
    )
      .bind(
        `${prefix}-row-${i}`,
        userId,
        deviceRowId,
        kind,
        await tokenHash(token),
        expiresAt,
        createdAt,
      )
      .run();
  }
}

describe('D3-1 设备上限：32 台 / 账号', () => {
  it('上限常量按 ADR 0076 固定为 32 / 64 / 30 天 / 200 行', () => {
    expect(MAX_DEVICES_PER_ACCOUNT).toBe(32);
    expect(MAX_VALID_REFRESH_TOKENS).toBe(64);
    expect(REFRESH_TOKEN_RETENTION_SECONDS).toBe(30 * 24 * 3600);
    expect(REFRESH_TOKEN_GC_MAX_ROWS).toBe(200);
  });

  it('设备数已达 32 ⇒ 登录被拒（429 device_limit_reached + 中文文案），且不签发令牌', async () => {
    const email = 'device-cap@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    await seedDevices(userId, 'cap', 32, 0);

    const error = await catchError(login(email, authKey, 'brand-new-device'));
    expect(error.status).toBe(429);
    expect(error.code).toBe('device_limit_reached');
    expect(error.message).toBe(
      '此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。',
    );

    // 拒绝时**既不删设备也不新建设备**，更不签发任何令牌。
    expect(await countDevices(userId)).toBe(32);
    expect(await countActiveDevices(userId)).toBe(32);
    expect(await countTokens(userId)).toBe(0);
  });

  it('已撤销设备可回收 ⇒ 回收后登录成功，设备总数收敛到 32', async () => {
    const email = 'device-reclaim@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    await seedDevices(userId, 'reclaim', 31, 5);

    const tokens = await login(email, authKey, 'new-device-after-reclaim');
    expect(typeof tokens.access_token).toBe('string');
    expect(typeof tokens.refresh_token).toBe('string');

    // 31 活跃 + 5 已撤销（共 36）⇒ 回收 5 台后仍 < 32 ⇒ 放行并新建 1 台 ⇒ 共 32 台。
    expect(await countDevices(userId)).toBe(32);
    expect(await countActiveDevices(userId)).toBe(32);
    expect(await countRevokedDevices(userId)).toBe(0);
  });

  it('回收后仍超限 ⇒ 仍拒绝登录，且活跃设备一台不少（绝不自动删在用设备）', async () => {
    const email = 'device-still-over@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    await seedDevices(userId, 'over', 35, 3);

    const error = await catchError(login(email, authKey, 'another-device'));
    expect(error.status).toBe(429);
    expect(error.code).toBe('device_limit_reached');

    // 35 活跃 + 3 已撤销（共 38）⇒ 回收 3 台后仍 35 ≥ 32 ⇒ 拒绝；活跃的 35 台一台没少。
    expect(await countActiveDevices(userId)).toBe(35);
    expect(await countDevices(userId)).toBe(35);
    expect(await countTokens(userId)).toBe(0);
  });

  it('设备回收只动 devices 行 ⇒ sync_items 一行不少', async () => {
    const email = 'device-sync-intact@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    await seedDevices(userId, 'syncintact', 31, 5);
    await env.DB.prepare(
      'INSERT INTO sync_items (user_id, item_key, type, hlc_ms, hlc_counter, hlc_device, r2_key, nonce_b64, cipher_len, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
    )
      .bind(userId, 'userdb/词条', 'userdb', 1700000000000, 0, 'hlc-device', 'r2-key', 'bm9uY2U=', 12, 1700000000000)
      .run();

    await login(email, authKey, 'device-that-triggers-reclaim');

    // 回收的是「已撤销设备」⇒ 用户的同步数据完全不受影响。
    const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ?')
      .bind(userId)
      .first<{ n: number }>();
    expect(Number(row?.n ?? 0)).toBe(1);
    expect(await countDevices(userId)).toBe(32);
  });
});

describe('D3-4 有效 refresh token 上限：64 个 / 账号', () => {
  it('有效 refresh 超 64 ⇒ 撤销最旧的有效行后仍能签发，且总数 ≤ 64', async () => {
    const email = 'refresh-cap@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    const tokens = await login(email, authKey, 'refresh-cap-device');

    const deviceRow = await env.DB.prepare(
      'SELECT id FROM devices WHERE user_id = ? AND device_id = ?',
    )
      .bind(userId, 'refresh-cap-device')
      .first<{ id: string }>();
    const deviceRowId = deviceRow?.id ?? '';

    // 登录已签发 1 个有效 refresh；再铺 64 个更早的有效 refresh ⇒ 共 65 个（超上限）。
    const now = nowSeconds();
    await seedRefreshTokens(userId, deviceRowId, 'seed-token', 64, {
      kind: 'refresh',
      createdAt: now - 1000,
    });
    expect(await countValidRefresh(userId)).toBe(65);

    // 续期：旧 refresh 被轮换作废 ⇒ 有效数回落到 64 ⇒ 签发前再撤 1 个最旧的 ⇒ 签发后仍 ≤ 64。
    const refreshed = (await handleRefresh(makeEnv(), {
      refresh_token: tokens.refresh_token,
    })) as LoginResult;
    expect(typeof refreshed.refresh_token).toBe('string');

    // 最旧的有效行（seed-token-0）被撤销。
    expect(await revokedAtOf(userId, 'seed-token-0')).not.toBeNull();
    // 最新的有效行仍在（没被误撤）。
    expect(await revokedAtOf(userId, 'seed-token-63')).toBeNull();
    expect(await countValidRefresh(userId)).toBeLessThanOrEqual(MAX_VALID_REFRESH_TOKENS);
    expect(await countValidRefresh(userId)).toBe(64);
  });
});

describe('D3-3 过期行 GC：30 天保留窗口 + 单次最多 200 行', () => {
  it('过期超 30 天的行分两次清完（单次 200 行上限），窗口内的行不删', async () => {
    const email = 'gc-window@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    const now = nowSeconds();
    const cutoff = now - REFRESH_TOKEN_RETENTION_SECONDS;

    // 250 行「过期超过 30 天」（陈旧）+ 1 行「过期 10 天」（仍在保留窗口内）。
    await seedRefreshTokens(userId, 'gc-device', 'stale-token', 250, {
      kind: 'access',
      expiresAt: cutoff - 24 * 3600,
      createdAt: cutoff - 24 * 3600,
    });
    await seedRefreshTokens(userId, 'gc-device', 'inside-window-token', 1, {
      kind: 'access',
      expiresAt: now - 10 * 24 * 3600,
      createdAt: now - 10 * 24 * 3600,
    });
    // 计数口径 = expires_at 早于保留窗口截止点（窗口内那行**不**计入）。
    expect(await countExpiredBefore(userId, cutoff)).toBe(250);

    // 第一次登录 ⇒ 顺带 GC：单次最多删 200 行 ⇒ 250 行陈旧数据还剩 50 行。
    await login(email, authKey, 'gc-device-1');
    expect(await countExpiredBefore(userId, cutoff)).toBe(50);
    // 窗口内（过期 10 天）的行**不删**（行仍在、也未撤销）。
    expect(await tokenRowExists(userId, 'inside-window-token-0')).toBe(true);
    expect(await revokedAtOf(userId, 'inside-window-token-0')).toBeNull();

    // 第二次登录 ⇒ 把剩下的 50 行陈旧数据清掉；窗口内的行依然保留。
    await login(email, authKey, 'gc-device-1');
    expect(await countExpiredBefore(userId, cutoff)).toBe(0);
    expect(await tokenRowExists(userId, 'inside-window-token-0')).toBe(true);
  });
});
