// 批次 BR · ADR 0076 D1（失败注册零写入化）+ D3（设备 / 令牌上限）单测。
//
// 本文件是 T1 / T3 的**规范测试文件**，逐条锁住交接要求的 10 条判据：
//   1. 发信失败 ⇒ users / email_verifications **零 INSERT**（假 D1 收到的 INSERT 次数 = 0）。
//   2. 发信成功 ⇒ 落库顺序「发信在前、INSERT 在后」（users → email_verifications）。
//   3. 发信成功但落库失败 ⇒ 503 storage_unavailable + 中文文案 + 回滚三态诊断头。
//   4. 邮箱唯一冲突 ⇒ 409 email_taken。
//   5. 重发失败 ⇒ 零写入（旧验证码行保持不变、仍然有效）。
//   6. 设备达 32 台 ⇒ 429 device_limit_reached + 中文文案。
//   7. 上限下同 device_id 重复登录 ⇒ 允许（走 UPDATE 分支，不被拒、不新增设备行）。
//   8. 已撤销设备被优先回收（回收「已撤销且最久未见」的行，活跃设备一台不少）。
//   9. 有效 refresh 超 64 ⇒ 撤销最旧的有效行。
//  10. 过期 refresh 被 GC（30 天保留窗口 + 单次最多 200 行）。
//
// 零真实网络：发信走注入的 fetch 替身（沿用 mail.ts 既有的 doFetch 注入风格）；
// 需要取验证码明文的用例走 MAIL_MODE='outbox'（只写 mail_outbox）。
// 断言「零写入 / 调用顺序」时用 SQL 记录器（spy env.DB.prepare）——
// mail 的 http 通道**不写 mail_outbox** ⇒ INSERT 计数不被污染。

import { describe, it, expect, vi } from 'vitest';
import { env } from 'cloudflare:test';
import {
  handleLogin,
  handleRefresh,
  handleRegister,
  handleResend,
  handleVerify,
  MAX_VALID_REFRESH_TOKENS,
  REFRESH_TOKEN_GC_MAX_ROWS,
  REFRESH_TOKEN_RETENTION_SECONDS,
} from '../src/auth';
import { MAX_DEVICES_PER_ACCOUNT } from '../src/devices';
import { toErrorResponse } from '../src/errors';
import { hmacSha256, nowSeconds, sha256, toBase64, utf8 } from '../src/crypto';
import type { Env } from '../src/env';

/** 上游拒绝收件人的响应体（Resend 形状：{ statusCode, name, message }）。 */
const RECIPIENT_REJECTED_BODY =
  '{"statusCode":400,"name":"validation_error","message":"Invalid `to` field"}';

interface ErrShape {
  status: number;
  code: string;
  message: string;
}

interface LoginResult {
  access_token: string;
  refresh_token: string;
}

/** env 副本：**绝不**修改共享 env 对象（否则会污染同文件里的其它用例）。 */
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

/** 捕获抛出的错误（用于断言状态码 / 错误码 / 中文文案）。 */
async function catchError(promise: Promise<unknown>): Promise<ErrShape> {
  try {
    await promise;
  } catch (error) {
    return error as ErrShape;
  }
  throw new Error('预期抛出错误，但调用成功了。');
}

function registerBody(email: string): Record<string, unknown> {
  return { email, auth_key: `auth-key-${email}`, device: 'test-device-01' };
}

/**
 * 假 fetch：返回指定状态码与响应体，不打真实网络。
 *
 * · timeline 非空 ⇒ 把 marker 记进时间线（用于断言「发信在前、INSERT 在后」）。
 * · calls  ⇒ 记录被调用的次数（用于证明失败路径真的发起了上游调用）。
 */
function stubFetch(
  status: number,
  body: string,
  timeline?: string[],
  marker = 'MAIL_SENT',
  calls?: number[],
): typeof fetch {
  return async () => {
    calls?.push(1);
    timeline?.push(marker);
    return new Response(body, { status });
  };
}

/** SQL 记录器：记录所有被 prepare 的语句（用来数 INSERT、断言调用顺序）。 */
interface SqlRecorder {
  sqls: string[];
  timeline: string[];
  restore: () => void;
}

function recordSqls(timeline: string[] = []): SqlRecorder {
  const db = env.DB as unknown as { prepare: (sql: string) => unknown };
  const originalPrepare = db.prepare.bind(db);
  const sqls: string[] = [];
  const spy = vi.spyOn(db, 'prepare').mockImplementation((sql: string) => {
    sqls.push(sql);
    timeline.push(`SQL:${sql.trim().slice(0, 48)}`);
    return originalPrepare(sql);
  });
  return { sqls, timeline, restore: () => spy.mockRestore() };
}

/** 只取 INSERT 语句（零写入判据）。 */
function insertsOf(recorder: SqlRecorder): string[] {
  return recorder.sqls.filter((sql) => sql.trimStart().toUpperCase().startsWith('INSERT INTO'));
}

/** 一条「写操作失败」的注入规则：SQL 前缀匹配 + 抛出的错误。 */
interface FailureRule {
  prefix: string;
  error: Error;
}

/**
 * 让匹配 SQL 前缀的语句抛错（窄窗口 / 回滚降级的注入点）。
 * 返回恢复函数（测试收尾必须调用，避免污染同文件里的其它用例）。
 */
function installWriteFailures(rules: FailureRule[]): () => void {
  const db = env.DB as unknown as {
    prepare: (sql: string) => {
      bind: (value: unknown) => { run: () => Promise<unknown> };
    };
  };
  const originalPrepare = db.prepare.bind(db);
  const spy = vi.spyOn(db, 'prepare').mockImplementation((sql: string) => {
    const rule = rules.find((item) => sql.startsWith(item.prefix));
    if (rule === undefined) return originalPrepare(sql);
    const failingStatement = {
      bind: (_value: unknown) => failingStatement,
      run: async () => {
        throw rule.error;
      },
    };
    return failingStatement as unknown as ReturnType<typeof db.prepare>;
  });
  return () => spy.mockRestore();
}

async function countRows(
  table: 'users' | 'email_verifications' | 'mail_outbox',
  email: string,
): Promise<number> {
  const column = table === 'mail_outbox' ? 'to_addr' : 'email';
  const row = await env.DB.prepare(`SELECT COUNT(*) AS n FROM ${table} WHERE ${column} = ?`)
    .bind(email)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/** 从 mail_outbox 取最近一封验证码邮件里的 6 位码（测试专用通道，生产不存在）。 */
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
 * 把该邮箱已发的验证码行往前挪，绕开 60 秒重发闸（测试专用夹具）。
 *
 * 注意：验证码哈希的输入里**含 created_at**（`email-verify-v1|email|code|created_at`）
 * ⇒ 只挪时间会让旧码在校验时必然 mismatch ⇒ 必须同步重算 code_hash。
 */
async function backdateVerifications(email: string, seconds: number, code: string): Promise<void> {
  const pepper = utf8((env as unknown as Env).AUTH_PEPPER ?? '');
  const rows = await env.DB.prepare(
    'SELECT id, created_at FROM email_verifications WHERE email = ?',
  )
    .bind(email)
    .all<{ id: string; created_at: number }>();
  for (const row of rows.results ?? []) {
    const createdAt = row.created_at - seconds;
    const hash = toBase64(
      await hmacSha256(pepper, utf8(`email-verify-v1|${email}|${code}|${createdAt}`)),
    );
    await env.DB.prepare(
      'UPDATE email_verifications SET created_at = ?, code_hash = ? WHERE id = ?',
    )
      .bind(createdAt, hash, row.id)
      .run();
  }
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
  return (await handleLogin(makeEnv(), {
    email,
    auth_key: authKey,
    device: deviceId,
  })) as LoginResult;
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

/** 该 device_id 对应的设备行是否仍在库里。 */
async function deviceRowExists(userId: string, deviceId: string): Promise<boolean> {
  const row = await env.DB.prepare(
    'SELECT id FROM devices WHERE user_id = ? AND device_id = ?',
  )
    .bind(userId, deviceId)
    .first<{ id: string }>();
  return row !== null;
}

/** 令牌哈希：与 auth.ts 的 hashToken 完全一致（base64(sha256(utf8(token)))。 */
async function tokenHash(token: string): Promise<string> {
  return toBase64(await sha256(utf8(token)));
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

/** 铺一批 refresh 行（kind 可选、过期时间可选、创建时间可选）。 */
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

// ---------------------------------------------------------------- D1 · 零写入

describe('D1-2 发信失败 ⇒ 零写入', () => {
  it('判据 1：发信被上游拒绝 ⇒ 422 mail_recipient_rejected + 中文文案，且 INSERT 次数 = 0', async () => {
    const email = 'zero-write@example.com';
    const calls: number[] = [];
    const recorder = recordSqls();
    try {
      const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY, undefined, 'MAIL_SENT', calls);
      const error = await catchError(
        handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
      );
      expect(error.status).toBe(422);
      expect(error.code).toBe('mail_recipient_rejected');
      expect(error.message).toBe(
        '验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。',
      );

      // 真的发起了上游调用（不是被前置校验短路）。
      expect(calls.length).toBe(1);

      // **零写入**：users / email_verifications 一行都没写 ⇒ 假 D1 收到的 INSERT 次数 = 0。
      expect(insertsOf(recorder)).toEqual([]);
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);

      // 零写入 ⇒ 无残留可回滚 ⇒ 回滚诊断头固定 'ok'（头部契约保持稳定）。
      expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
    } finally {
      recorder.restore();
    }
  });

  it('判据 2：发信成功 ⇒ 落库顺序「发信在前、INSERT 在后」（users → email_verifications）', async () => {
    const email = 'order@example.com';
    const timeline: string[] = [];
    const recorder = recordSqls(timeline);
    try {
      const fetchImpl = stubFetch(200, '{"id":"mail-ok"}', timeline);
      const result = (await handleRegister(
        makeEnv({ MAIL_MODE: 'http' }),
        registerBody(email),
        fetchImpl,
      )) as { need_email_verify: boolean };
      expect(result.need_email_verify).toBe(true);

      const mailAt = timeline.indexOf('MAIL_SENT');
      const usersAt = timeline.findIndex((item) => item.startsWith('SQL:INSERT INTO users'));
      const codeAt = timeline.findIndex((item) =>
        item.startsWith('SQL:INSERT INTO email_verifications'),
      );

      expect(mailAt).toBeGreaterThanOrEqual(0);
      expect(usersAt).toBeGreaterThan(mailAt);
      expect(codeAt).toBeGreaterThan(usersAt);
      // 落库顺序固定「先 users 后验证码」（D1-4：并发注册的败者卡在 users 的 UNIQUE(email)）。
      expect(insertsOf(recorder)).toHaveLength(2);
    } finally {
      recorder.restore();
    }
  });

  it('判据 3a：发信成功但 users 落库失败 ⇒ 503 storage_unavailable + 中文文案 + 回滚头 ok', async () => {
    const email = 'narrow-users@example.com';
    const restore = installWriteFailures([
      { prefix: 'INSERT INTO users', error: new Error('模拟 D1 写入失败') },
    ]);
    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      expect(error.status).toBe(503);
      expect(error.code).toBe('storage_unavailable');
      expect(error.message).toBe(
        '验证码已发送，但服务器存储暂时不可用，请稍后重试或重新获取验证码。',
      );
      // users 行没写进去 ⇒ 没有残留可回滚 ⇒ 回滚头 'ok'。
      expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
    } finally {
      restore();
    }
  });

  it('判据 3b：验证码行落库失败 ⇒ 回滚三态 degraded（DELETE 被拒 ⇒ UPDATE 失效化）', async () => {
    const email = 'narrow-degraded@example.com';
    const restore = installWriteFailures([
      { prefix: 'INSERT INTO email_verifications', error: new Error('模拟 D1 写入失败') },
      { prefix: 'DELETE FROM users', error: new Error('模拟 D1 删除失败') },
    ]);
    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      expect(error.status).toBe(503);
      expect(error.code).toBe('storage_unavailable');

      const response = toErrorResponse(error);
      expect(response.headers.get('X-Fluxing-Mail-Rollback')).toBe('degraded');
      const rollbackError = response.headers.get('X-Fluxing-Mail-Rollback-Error') ?? '';
      expect(rollbackError).toContain('delete failed');
      expect(/^[\x20-\x7e]*$/.test(rollbackError)).toBe(true);

      // 降级效果：users 行的 email 被改成占位地址 ⇒ 原邮箱不再占号。
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
    } finally {
      restore();
    }
  });

  it('判据 3c：回滚的 DELETE 与失效化都被拒 ⇒ 回滚三态 failed（诊断头带原因）', async () => {
    const email = 'narrow-failed@example.com';
    const restore = installWriteFailures([
      { prefix: 'INSERT INTO email_verifications', error: new Error('模拟 D1 写入失败') },
      { prefix: 'DELETE FROM users', error: new Error('模拟 D1 删除失败') },
      { prefix: 'UPDATE users SET email', error: new Error('模拟 D1 写入失败') },
    ]);
    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      expect(error.status).toBe(503);
      expect(error.code).toBe('storage_unavailable');

      const response = toErrorResponse(error);
      expect(response.headers.get('X-Fluxing-Mail-Rollback')).toBe('failed');
      expect(response.headers.get('X-Fluxing-Mail-Rollback-Error') ?? '').toContain(
        'neutralize failed',
      );

      // 错误体不含内部细节（只有诊断头带）。
      const body = (await response.json()) as { error: { code: string; message: string } };
      expect(body.error.code).toBe('storage_unavailable');
      expect(body.error.message).not.toContain('D1');
    } finally {
      restore();
    }
  });

  it('判据 4a：邮箱已被占用 ⇒ 409 email_taken，且不写库、不发信', async () => {
    const email = 'taken@example.com';
    await handleRegister(makeEnv(), registerBody(email));
    const firstCode = await latestCode(email);
    // 频率闸口径不变（D1-6）：60 秒窗口内重复注册会先撞 429 verify_too_frequent。
    // 这里把已发验证码行的 created_at 往前挪 ⇒ 窗口已过 ⇒ 才会走到「邮箱已被占用」的查重分支。
    await backdateVerifications(email, 120, firstCode);

    const calls: number[] = [];
    const recorder = recordSqls();
    try {
      const fetchImpl = stubFetch(200, '{"id":"mail-ok"}', undefined, 'MAIL_SENT', calls);
      const error = await catchError(
        handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
      );
      expect(error.status).toBe(409);
      expect(error.code).toBe('email_taken');
      expect(error.message).toBe('该邮箱已注册，请直接登录。');
      // 查重命中 ⇒ 不会走到发信、更不会落库。
      expect(calls.length).toBe(0);
      expect(insertsOf(recorder)).toEqual([]);
      // 账号行没有被顶掉（仍是一条），也没有多出验证码行。
      expect(await countRows('users', email)).toBe(1);
      expect(await countRows('email_verifications', email)).toBe(1);
    } finally {
      recorder.restore();
    }
  });

  it('判据 4b：同邮箱并发注册 ⇒ 败者 409 email_taken，且没有验证码孤儿行', async () => {
    const email = 'race@example.com';
    let arrived = 0;
    let release: () => void = () => {};
    const gate = new Promise<void>((resolve) => {
      release = resolve;
    });
    // 两个请求都在「发信」这一步等着 ⇒ 双方必然都跑完了查重（只读）才一起落库，
    // 于是败者必然卡在 users 的 UNIQUE(email) 上（D1-4）。
    const fetchImpl = async () => {
      arrived += 1;
      if (arrived >= 2) release();
      await gate;
      return new Response('{"id":"mail-ok"}', { status: 200 });
    };

    const settled = await Promise.allSettled([
      handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
      handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
    ]);
    const fulfilled = settled.filter((item) => item.status === 'fulfilled');
    const rejected = settled.filter((item) => item.status === 'rejected');
    expect(fulfilled).toHaveLength(1);
    expect(rejected).toHaveLength(1);

    const loser = (rejected[0] as PromiseRejectedResult).reason as ErrShape;
    expect(loser.status).toBe(409);
    expect(loser.code).toBe('email_taken');
    expect(loser.message).toBe('该邮箱已注册，请直接登录。');

    // 败者卡在 users INSERT ⇒ 没有验证码孤儿行。
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
  });

  it('判据 5：重发失败 ⇒ 零写入，旧验证码行保持不变、仍然有效', async () => {
    const email = 'resend-zero@example.com';
    await handleRegister(makeEnv(), registerBody(email));
    const oldCode = await latestCode(email);
    await backdateVerifications(email, 120, oldCode);

    const calls: number[] = [];
    const recorder = recordSqls();
    try {
      const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY, undefined, 'MAIL_SENT', calls);
      const error = await catchError(
        handleResend(makeEnv({ MAIL_MODE: 'http' }), { email }, fetchImpl),
      );
      expect(error.status).toBe(422);
      expect(error.code).toBe('mail_recipient_rejected');
      expect(calls.length).toBe(1);

      // 零写入：users 行仍在（账号不删）、验证码行数不变（旧行没被删、也没新增）。
      expect(insertsOf(recorder)).toEqual([]);
      expect(await countRows('users', email)).toBe(1);
      expect(await countRows('email_verifications', email)).toBe(1);
      expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');

      // 旧验证码仍然有效（本次重发失败没有作废它）。
      await expect(handleVerify(env as unknown as Env, { email, code: oldCode })).resolves.toMatchObject({
        ok: true,
      });
    } finally {
      recorder.restore();
    }
  });
});

// ---------------------------------------------------------------- D3 · 设备 / 令牌上限

describe('D3-1 设备上限：32 台 / 账号', () => {
  it('上限常量按 ADR 0076 固定为 32 / 64 / 30 天 / 200 行', () => {
    expect(MAX_DEVICES_PER_ACCOUNT).toBe(32);
    expect(MAX_VALID_REFRESH_TOKENS).toBe(64);
    expect(REFRESH_TOKEN_RETENTION_SECONDS).toBe(30 * 24 * 3600);
    expect(REFRESH_TOKEN_GC_MAX_ROWS).toBe(200);
  });

  it('判据 6：设备数已达 32 ⇒ 登录被拒（429 device_limit_reached + 中文文案），且不签发令牌', async () => {
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
    expect(await countRevokedDevices(userId)).toBe(0);
  });

  it('判据 7：上限下同 device_id 重复登录 ⇒ 允许（走 UPDATE 分支，不被拒、不新增设备行）', async () => {
    const email = 'device-known@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    // 31 台活跃 + 1 台已撤销（同一台机器的旧凭证）⇒ 设备行数已达上限 32。
    await seedDevices(userId, 'known', 31, 0);
    await env.DB.prepare(
      'INSERT INTO devices (id, user_id, name, device_id, last_seen, revoked_at, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)',
    )
      .bind('known-revoked-0', userId, 'known-revoked-0', 'known-device', nowSeconds() - 9999, nowSeconds() - 9999, nowSeconds() - 9999)
      .run();
    expect(await countDevices(userId)).toBe(32);

    // 同一 device_id 再登录 ⇒ **不新增设备行** ⇒ 不受上限约束 ⇒ 允许。
    const tokens = await login(email, authKey, 'known-device');
    expect(typeof tokens.access_token).toBe('string');
    expect(typeof tokens.refresh_token).toBe('string');

    // 设备行数不变（没有多出一台），且该设备被恢复为活跃。
    expect(await countDevices(userId)).toBe(32);
    expect(await countActiveDevices(userId)).toBe(32);
    const row = await env.DB.prepare(
      'SELECT revoked_at FROM devices WHERE user_id = ? AND device_id = ?',
    )
      .bind(userId, 'known-device')
      .first<{ revoked_at: number | null }>();
    expect(row?.revoked_at ?? null).toBeNull();
  });

  it('判据 8：已撤销设备被优先回收（回收「已撤销且最久未见」的行，活跃设备一台不少）', async () => {
    const email = 'device-reclaim@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    await seedDevices(userId, 'reclaim', 31, 5);

    const tokens = await login(email, authKey, 'new-device-after-reclaim');
    expect(typeof tokens.access_token).toBe('string');

    // 31 活跃 + 5 已撤销（共 36）⇒ 回收 5 台后仍 < 32 ⇒ 放行并新建 1 台 ⇒ 共 32 台。
    expect(await countDevices(userId)).toBe(32);
    expect(await countActiveDevices(userId)).toBe(32);
    expect(await countRevokedDevices(userId)).toBe(0);

    // 被回收的正是那 5 台已撤销设备（最久未见的优先）；活跃设备一台不少。
    for (let i = 0; i < 5; i += 1) {
      expect(await deviceRowExists(userId, `reclaim-revoked-${i}`)).toBe(false);
    }
    for (let i = 0; i < 31; i += 1) {
      expect(await deviceRowExists(userId, `reclaim-active-${i}`)).toBe(true);
    }
    expect(await deviceRowExists(userId, 'new-device-after-reclaim')).toBe(true);
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
  });
});

describe('D3-4 有效 refresh token 上限：64 个 / 账号', () => {
  it('判据 9：有效 refresh 超 64 ⇒ 撤销最旧的有效行，签发后仍 ≤ 64', async () => {
    const email = 'refresh-cap@example.com';
    const { userId, authKey } = await verifiedAccount(email);

    // 铺 65 个「未过期且未撤销」的 refresh 行（created_at 越早越旧）。
    const now = nowSeconds();
    await seedRefreshTokens(userId, 'seed-device-row', 'seed-token', 65, {
      kind: 'refresh',
      createdAt: now - 100000,
    });
    expect(await countValidRefresh(userId)).toBe(65);

    // 登录 ⇒ 签发前先把有效行压到上限内（撤最旧的）⇒ 签发后仍 ≤ 64。
    const tokens = await login(email, authKey, 'refresh-cap-device');
    expect(typeof tokens.refresh_token).toBe('string');

    // 最旧的两行被撤销；较新的行没被误撤。
    expect(await revokedAtOf(userId, 'seed-token-0')).not.toBeNull();
    expect(await revokedAtOf(userId, 'seed-token-1')).not.toBeNull();
    expect(await revokedAtOf(userId, 'seed-token-2')).toBeNull();
    expect(await countValidRefresh(userId)).toBeLessThanOrEqual(MAX_VALID_REFRESH_TOKENS);
    expect(await countValidRefresh(userId)).toBe(64);
  });

  it('轮换续期同样受上限约束（refresh 行只增不减 ⇒ 必须靠撤销收敛）', async () => {
    const email = 'refresh-rotate@example.com';
    const { userId, authKey } = await verifiedAccount(email);
    const tokens = await login(email, authKey, 'refresh-rotate-device');
    expect(await countValidRefresh(userId)).toBe(1);

    const rotated = (await handleRefresh(makeEnv(), {
      refresh_token: tokens.refresh_token,
    })) as LoginResult;
    expect(typeof rotated.refresh_token).toBe('string');

    // 旧 refresh 被轮换作废、新 refresh 签发 ⇒ 有效行数仍是 1。
    expect(await countValidRefresh(userId)).toBe(1);
    expect(await revokedAtOf(userId, tokens.refresh_token)).not.toBeNull();
  });
});

describe('D3-3 过期行 GC：30 天保留窗口 + 单次最多 200 行', () => {
  it('判据 10：过期超 30 天的行分两次清完（单次 200 行上限），窗口内的行不删', async () => {
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
