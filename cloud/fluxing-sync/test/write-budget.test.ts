// 写预算护栏单测（ADR 0076 D2-3）。
//
// 分三层覆盖：
//   1. 纯函数：evaluateWriteBudget 的三个分支（未超限 / 恰好等于阈值 / 超过阈值）、
//      planWriteRows 的 1 行 vs 2 行口径、utcDayStartSeconds 的 UTC 日边界、
//      writeBudgetRowsPerDay 的读取与回落；
//   2. 既有行为对照：小批量推送仍 200，D1 行数按既有语义变化
//      （新条目 +1 行；已有条目重推是「DELETE + INSERT」⇒ 行数不变、序号前进）；
//   3. 集成：超限时 429 write_budget_exceeded + 中文文案 + **零写入**
//      （D1 零新增行、R2 零对象）。
//
// 注意：isolatedStorage 默认开启 ⇒ 多步流程必须写在同一个 it() 里。

import { SELF, env } from 'cloudflare:test';
import { describe, expect, it } from 'vitest';
import {
  WRITE_BUDGET_EXCEEDED_CODE,
  WRITE_BUDGET_EXCEEDED_MESSAGE,
  countTodayWrittenRows,
  evaluateWriteBudget,
  planWriteRows,
  utcDayStartSeconds,
} from '../src/sync';
import { DEFAULT_WRITE_BUDGET_ROWS_PER_DAY, writeBudgetRowsPerDay } from '../src/env';

const BASE = 'https://fluxing-sync.test';

/** 12 字节 nonce（对齐 FluxingCloudEnvelope.h 的 kNonceBytes） */
const NONCE = b64(new Uint8Array(12).fill(7));
/** 48 字节密文（≥ 16 字节认证标签即可通过校验） */
const CIPHER = b64(new Uint8Array(48).fill(9));

function b64(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary);
}

interface ApiResult {
  status: number;
  body: any;
}

async function api(
  method: string,
  path: string,
  options: { body?: unknown; token?: string } = {},
): Promise<ApiResult> {
  const headers: Record<string, string> = {};
  if (options.body !== undefined) headers['content-type'] = 'application/json';
  if (options.token !== undefined) headers.authorization = `Bearer ${options.token}`;

  const response = await SELF.fetch(`${BASE}${path}`, {
    method,
    headers,
    body: options.body === undefined ? undefined : JSON.stringify(options.body),
  });
  const text = await response.text();
  return { status: response.status, body: text.length === 0 ? null : JSON.parse(text) };
}

/** 从 outbox 里取最近一封验证码邮件中的 6 位码（测试专用通道，生产不存在）。 */
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

function item(itemKey: string, overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    item_key: itemKey,
    type: 'config',
    hlc: { ms: 1_700_000_000_000, counter: 0, device: 'device-a' },
    nonce: NONCE,
    cipher: CIPHER,
    ...overrides,
  };
}

/** 注册 → 取码 → 验证 → 登录，返回后续用例要用的句柄。 */
async function signUpAndLogin(email: string, device: string) {
  const authKey = `auth-key-${email}`;

  const registered = await api('POST', '/v1/auth/register', {
    body: { email, auth_key: authKey, device },
  });
  expect(registered.status).toBe(200);

  const code = await latestCode(email);
  const verified = await api('POST', '/v1/auth/verify', { body: { email, code } });
  expect(verified.status).toBe(200);

  const loggedIn = await api('POST', '/v1/auth/login', {
    body: { email, auth_key: authKey, device },
  });
  expect(loggedIn.status).toBe(200);

  return { authKey, code, register: registered.body, login: loggedIn.body };
}

/** 该用户在 sync_items 里的行数。 */
async function syncItemRows(userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

/** 该用户在 R2 桶里的对象数（键前缀 u/<user_id>/）。 */
async function syncR2Objects(userId: string): Promise<number> {
  const listed = await env.BUCKET.list({ prefix: `u/${userId}/` });
  return listed.objects.length;
}

/** 临时改写 worker 的普通变量绑定（测试用）：写不进去时返回 false。 */
function trySetWriteBudget(raw: string): boolean {
  try {
    (env as unknown as Record<string, unknown>).WRITE_BUDGET_ROWS_PER_DAY = raw;
    return (env as unknown as Record<string, unknown>).WRITE_BUDGET_ROWS_PER_DAY === raw;
  } catch {
    return false;
  }
}

// ---------------------------------------------------------------- 纯函数

describe('写预算判定（纯函数）', () => {
  it('未超限放行，返回 within_budget', () => {
    const result = evaluateWriteBudget({ usedRows: 10, plannedRows: 5, limitRows: 100 });
    expect(result.allowed).toBe(true);
    expect(result.reason).toBe('within_budget');
    expect(result.usedRows).toBe(10);
    expect(result.plannedRows).toBe(5);
    expect(result.limitRows).toBe(100);
  });

  it('恰好等于阈值放行（判定式是 > 而非 >=）', () => {
    const result = evaluateWriteBudget({ usedRows: 90, plannedRows: 10, limitRows: 100 });
    expect(result.allowed).toBe(true);
    expect(result.reason).toBe('within_budget');
  });

  it('超过阈值拒绝，返回 budget_exceeded', () => {
    const result = evaluateWriteBudget({ usedRows: 90, plannedRows: 11, limitRows: 100 });
    expect(result.allowed).toBe(false);
    expect(result.reason).toBe('budget_exceeded');
  });

  it('planned 口径：新条目 1 行、已存在行 2 行、批内重复的第 2 次起 2 行', () => {
    // 两个全新条目 ⇒ 各 1 行
    expect(planWriteRows(['config/a', 'config/b'], new Set())).toBe(2);
    // 两条都已存在 ⇒ 各 2 行（DELETE + INSERT）
    expect(planWriteRows(['config/a', 'config/b'], new Set(['config/a', 'config/b']))).toBe(4);
    // 批内重复：第 1 次 1 行（行还不存在），第 2 次 2 行（行已存在）⇒ 3 行
    expect(planWriteRows(['config/a', 'config/a'], new Set())).toBe(3);
    // 混合：a 已存在、b 新 ⇒ 2 + 1
    expect(planWriteRows(['config/a', 'config/b'], new Set(['config/a']))).toBe(3);
  });

  it('UTC 日边界：今日 00:00 UTC 对齐到整日', () => {
    expect(utcDayStartSeconds(1_700_000_000_000 % 86_400 + 86_400 * 20_000)).toBe(
      86_400 * 20_000,
    );
    // 恰好落在 UTC 00:00:00 的那一秒本身就是边界
    expect(utcDayStartSeconds(86_400 * 20_000)).toBe(86_400 * 20_000);
  });

  it('上限读取：环境变量可覆盖，缺失或非法时回落到默认常量 80,000', () => {
    expect(DEFAULT_WRITE_BUDGET_ROWS_PER_DAY).toBe(80_000);
    // 绑定缺失 ⇒ 回落默认值
    expect(writeBudgetRowsPerDay({} as never)).toBe(DEFAULT_WRITE_BUDGET_ROWS_PER_DAY);
    // 非法值（不是数字 / 越界）⇒ 回落默认值
    expect(writeBudgetRowsPerDay({ WRITE_BUDGET_ROWS_PER_DAY: 'abc' } as never)).toBe(
      DEFAULT_WRITE_BUDGET_ROWS_PER_DAY,
    );
    expect(writeBudgetRowsPerDay({ WRITE_BUDGET_ROWS_PER_DAY: '0' } as never)).toBe(
      DEFAULT_WRITE_BUDGET_ROWS_PER_DAY,
    );
    // 合法值 ⇒ 原样采用
    expect(writeBudgetRowsPerDay({ WRITE_BUDGET_ROWS_PER_DAY: '1234' } as never)).toBe(1234);
  });
});

// ---------------------------------------------------------------- 既有行为对照

describe('小批量推送不受护栏影响', () => {
  it('正常推送仍 200；新条目 +1 行，重推覆盖不增行', async () => {
    const { register, login } = await signUpAndLogin('budget-ok@example.com', 'device-budget-ok');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    const pushed = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main'), item('phrases/main', { type: 'phrases' })] },
    });
    expect(pushed.status).toBe(200);
    expect(pushed.body.accepted).toEqual(['config/main', 'phrases/main']);
    expect(pushed.body.skipped).toEqual([]);
    const firstSeq = pushed.body.server_seq as number;
    expect(firstSeq).toBeGreaterThan(0);

    // 两个新条目 ⇒ D1 两行、R2 两个对象（既有语义不变）
    expect(await syncItemRows(userId)).toBe(2);
    expect(await syncR2Objects(userId)).toBe(2);

    // 重推（HLC 更新）⇒ 仍是「覆盖一行」：行数不变、server_seq 前进
    const repushed = await api('POST', '/v1/sync/push', {
      token,
      body: {
        items: [
          item('config/main', {
            cipher: b64(new Uint8Array(50).fill(11)),
            hlc: { ms: 1_700_000_000_001, counter: 0, device: 'device-a' },
          }),
        ],
      },
    });
    expect(repushed.status).toBe(200);
    expect(repushed.body.accepted).toEqual(['config/main']);
    expect(repushed.body.server_seq).toBeGreaterThan(firstSeq);
    expect(await syncItemRows(userId)).toBe(2);
    expect(await syncR2Objects(userId)).toBe(2);

    // 今日已写行数（纯读口径）= 当前仍存在的行数（同一行当日反复写只计 1）
    expect(await countTodayWrittenRows(env as never, userId, Math.floor(Date.now() / 1000))).toBe(
      2,
    );
  });
});

// ---------------------------------------------------------------- 集成：超限 ⇒ 零写入

describe('超限时整批拒绝且零写入', () => {
  it('429 write_budget_exceeded + 中文文案 + D1 零新增行 + R2 零对象', async () => {
    const { register, login } = await signUpAndLogin('budget-over@example.com', 'device-budget-over');
    const token = login.access_token as string;
    const userId = register.account_id as string;

    // 注入小阈值：1 行预算 ⇒ 推 2 条（planned = 2）必然超限。
    const original = (env as unknown as Record<string, unknown>).WRITE_BUDGET_ROWS_PER_DAY;
    const patched = trySetWriteBudget('1');
    expect(patched).toBe(true);

    try {
      const rejected = await api('POST', '/v1/sync/push', {
        token,
        body: { items: [item('config/main'), item('config/other')] },
      });
      expect(rejected.status).toBe(429);
      expect(rejected.body.error.code).toBe(WRITE_BUDGET_EXCEEDED_CODE);
      expect(rejected.body.error.code).toBe('write_budget_exceeded');
      expect(rejected.body.error.message).toBe(WRITE_BUDGET_EXCEEDED_MESSAGE);
      expect(rejected.body.error.message).toBe(
        '今日云端写入已达上限，请明天再试或减少本机词典规模。',
      );

      // 零写入：D1 一行没多、R2 一个对象没建
      expect(await syncItemRows(userId)).toBe(0);
      expect(await syncR2Objects(userId)).toBe(0);

      // 拉取也拿不到任何条目
      const pulled = await api('GET', '/v1/sync/pull?since=0', { token });
      expect(pulled.status).toBe(200);
      expect(pulled.body.items).toEqual([]);
    } finally {
      (env as unknown as Record<string, unknown>).WRITE_BUDGET_ROWS_PER_DAY = original;
    }

    // 预算恢复后同一条推送可正常落库（证明 429 是预算驱动的，不是状态损坏）
    const allowed = await api('POST', '/v1/sync/push', {
      token,
      body: { items: [item('config/main'), item('config/other')] },
    });
    expect(allowed.status).toBe(200);
    expect(await syncItemRows(userId)).toBe(2);
    expect(await syncR2Objects(userId)).toBe(2);
  });
});
