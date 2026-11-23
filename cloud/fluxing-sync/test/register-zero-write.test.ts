// 注册 / 重发验证码「零写入」单测（ADR 0076 D1）。
//
// 背景（批次 BQ 生产实测）：旧顺序是「先写库、后发信」，发信失败再靠 DELETE 回滚。
// 但 D1 每日行写入额度耗尽时**回滚本身也被拒** ⇒ 库里留下 users + email_verifications
// 两条残留（占号 + 吃掉 60 秒重发窗口，真正的根因「收件人被拒」被掩盖成「验证码过于频繁」）。
//
// ADR 0076 D1 把顺序反转成「纯生成 → 发信 → 落库」⇒ **发信失败一行都不写**。
//
// 本文件锁六件事：
//   1. 发信失败 ⇒ users / email_verifications 零行（零写入）。
//   2. 发信成功 ⇒ 两表各有且仅有 1 行，且验证码可用。
//   3. 同邮箱并发注册 ⇒ 败者 409 email_taken，且**没有验证码孤儿行**。
//   4. 窄窗口（发信成功但落库失败）⇒ 503 storage_unavailable + 中文文案 + 回滚三态。
//   5. D1 配额耗尽 ⇒ 503 storage_quota_exceeded（D0-2 优先于 D1-5）。
//   6. 失败注册不吃 60 秒窗口 ⇒ 可立即重试。
//
// 零真实网络：发信走注入的 fetch 替身（沿用 sendVerificationMail 既有的 doFetch 注入风格）；
// 默认走 outbox 模式（只写 mail_outbox，验证码从表里取）。

import { describe, it, expect, vi } from 'vitest';
import { env } from 'cloudflare:test';
import { handleRegister, handleResend, handleVerify } from '../src/auth';
import { toErrorResponse } from '../src/errors';
import { hmacSha256, toBase64, utf8 } from '../src/crypto';
import type { Env } from '../src/env';

/** 上游拒绝收件人的响应体（Resend 形状：{ statusCode, name, message }）。 */
const RECIPIENT_REJECTED_BODY =
  '{"statusCode":400,"name":"validation_error","message":"Invalid `to` field"}';

/** D1 行写入配额耗尽（Cloudflare code 7500）。 */
const QUOTA_ERROR =
  "Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details. [code: 7500]";

interface ErrShape {
  status: number;
  code: string;
  message: string;
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

/** 假 fetch：返回指定状态码与响应体，不打真实网络。 */
function stubFetch(status: number, body: string): typeof fetch {
  return async () => new Response(body, { status });
}

function registerBody(email: string): Record<string, unknown> {
  return { email, auth_key: `auth-key-${email}`, device: 'test-device-01' };
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

/** 捕获抛出的错误（用于断言「不是 429 / 不是 409」这类否定式判据）。 */
async function catchError(promise: Promise<unknown>): Promise<ErrShape> {
  try {
    await promise;
  } catch (error) {
    return error as ErrShape;
  }
  throw new Error('预期抛出错误，但调用成功了。');
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

describe('D1 零写入：发信失败 ⇒ users / email_verifications 一行都不写', () => {
  it('发信被上游拒绝（400）⇒ 422 mail_recipient_rejected，且两表零行', async () => {
    const email = 'zero-write@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const error = await catchError(
      handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
    );
    expect(error.status).toBe(422);
    expect(error.code).toBe('mail_recipient_rejected');
    expect(error.message).toBe(
      '验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。',
    );

    // **零写入**：既没有账号行，也没有验证码行。
    expect(await countRows('users', email)).toBe(0);
    expect(await countRows('email_verifications', email)).toBe(0);
    // 零写入 ⇒ 无残留可回滚 ⇒ 回滚诊断头固定 'ok'（头部契约保持稳定）。
    expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
  });

  it('发信成功 ⇒ users / email_verifications 各有且仅有 1 行，且验证码可用', async () => {
    const email = 'success@example.com';

    const result = (await handleRegister(makeEnv(), registerBody(email))) as {
      need_email_verify: boolean;
      account_id: string;
    };
    expect(result.need_email_verify).toBe(true);
    expect(typeof result.account_id).toBe('string');
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
    expect(await countRows('mail_outbox', email)).toBe(1);

    // 落库顺序「先 users 后验证码」⇒ 落库后的验证码能正常完成验证。
    const code = await latestCode(email);
    await expect(handleVerify(env as unknown as Env, { email, code })).resolves.toMatchObject({
      ok: true,
    });
  });

  it('同邮箱并发注册 ⇒ 败者 409 email_taken，且没有验证码孤儿行', async () => {
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

    // 败者卡在 users INSERT ⇒ 没有验证码孤儿行。
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
  });

  it('失败注册不吃 60 秒窗口 ⇒ 立刻重试即可注册成功（不撞 429 / 409）', async () => {
    const email = 'retry-now@example.com';
    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);

    const failed = await catchError(
      handleRegister(makeEnv({ MAIL_MODE: 'http' }), registerBody(email), fetchImpl),
    );
    expect(failed.status).toBe(422);

    // 立刻重试（换成能发信的通道）⇒ 不撞 429 verify_too_frequent、不撞 409 email_taken。
    const retried = (await handleRegister(makeEnv(), registerBody(email))) as {
      need_email_verify: boolean;
    };
    expect(retried.need_email_verify).toBe(true);
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
  });

  it('重发失败 ⇒ 零写入：旧验证码行保持不变、仍然有效', async () => {
    const email = 'resend-zero@example.com';
    await handleRegister(makeEnv(), registerBody(email));
    const oldCode = await latestCode(email);
    await backdateVerifications(email, 120, oldCode);

    const fetchImpl = stubFetch(400, RECIPIENT_REJECTED_BODY);
    const error = await catchError(
      handleResend(makeEnv({ MAIL_MODE: 'http' }), { email }, fetchImpl),
    );
    expect(error.status).toBe(422);
    expect(error.code).toBe('mail_recipient_rejected');

    // 零写入：users 行仍在（账号不删）、验证码行数不变（旧行没被删、也没新增）。
    expect(await countRows('users', email)).toBe(1);
    expect(await countRows('email_verifications', email)).toBe(1);
    expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');

    // 旧验证码仍然有效（本次重发失败没有作废它）。
    await expect(handleVerify(env as unknown as Env, { email, code: oldCode })).resolves.toMatchObject({
      ok: true,
    });
  });
});

describe('D1-5 窄窗口：发信已成功、落库失败', () => {
  it('users 落库失败 ⇒ 503 storage_unavailable + 中文文案，且零残留', async () => {
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

      // 邮件已经发出去（outbox 留痕）⇒ 用户手上的验证码用不了（这正是窄窗口的含义）。
      expect(await countRows('mail_outbox', email)).toBe(1);
      // users 行没写进去 ⇒ 没有残留可回滚。
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
      expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
    } finally {
      restore();
    }
  });

  it('验证码行落库失败 ⇒ 503 storage_unavailable，且已插入的 users 行被回滚掉', async () => {
    const email = 'narrow-code@example.com';
    const restore = installWriteFailures([
      { prefix: 'INSERT INTO email_verifications', error: new Error('模拟 D1 写入失败') },
    ]);
    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      expect(error.status).toBe(503);
      expect(error.code).toBe('storage_unavailable');
      // users 行已写入 ⇒ 回滚把它删掉 ⇒ 零残留（不占号）。
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
      expect(toErrorResponse(error).headers.get('X-Fluxing-Mail-Rollback')).toBe('ok');
    } finally {
      restore();
    }
  });

  it('回滚时 DELETE 被拒 ⇒ 回滚三态 degraded（占号解除）', async () => {
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
    } finally {
      restore();
    }
  });

  it('回滚的 DELETE 与失效化都被拒 ⇒ 回滚三态 failed（诊断头带原因）', async () => {
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
      const rollbackError = response.headers.get('X-Fluxing-Mail-Rollback-Error') ?? '';
      expect(rollbackError).toContain('neutralize failed');
      expect(/^[\x20-\x7e]*$/.test(rollbackError)).toBe(true);

      // 错误体不含内部细节（只有诊断头带）。
      const body = (await response.json()) as { error: { code: string; message: string } };
      expect(body.error.code).toBe('storage_unavailable');
      expect(body.error.message).not.toContain('D1');
    } finally {
      restore();
    }
  });

  it('D1 配额耗尽（code 7500）⇒ 503 storage_quota_exceeded（D0-2 优先于 D1-5）', async () => {
    const email = 'narrow-quota@example.com';
    const restore = installWriteFailures([
      { prefix: 'INSERT INTO users', error: new Error(QUOTA_ERROR) },
    ]);
    try {
      const error = await catchError(handleRegister(makeEnv(), registerBody(email)));
      expect(error.status).toBe(503);
      expect(error.code).toBe('storage_quota_exceeded');
      expect(error.message).toBe('服务端存储写入额度已用尽，请稍后重试。');
      expect(await countRows('users', email)).toBe(0);
      expect(await countRows('email_verifications', email)).toBe(0);
    } finally {
      restore();
    }
  });
});
