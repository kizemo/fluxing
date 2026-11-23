// D1「每日行写入配额耗尽」错误映射单测（ADR 0076 D0-2 补强）。
//
// 生产实测（2026-11-23，批次 BR 判据 3）：D1 免费额度耗尽时，Worker 内 D1 绑定抛出的错误
// 文本**不带** `[code: 7500]` ⇒ 旧判据（只认 REST API 形态）命中不了 ⇒ 登录 / 续期 / 推送
// 这些写路径对外返回笼统 500 internal_error ⇒ 客户端与运维都会误判成代码缺陷，
// 而真实原因只是「UTC 午夜自动重置」的暂时性平台故障。
//
// 本文件锁住：REST 形态、Worker 绑定形态、非 Error 实例、字符串形态都能映射到
// 503 storage_quota_exceeded；无关错误仍然是笼统 500（不误伤）。

import { describe, it, expect } from 'vitest';
import { isD1RowWriteQuotaError, toErrorResponse } from '../src/errors';

/** REST API 形态（wrangler d1 execute / Cloudflare API）：文本尾部带 [code: 7500]。 */
const REST_QUOTA_ERROR =
  "Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details. [code: 7500]";

/** Worker 内 D1 绑定形态：D1_ERROR 前缀 + 同一句话，但**没有** [code: 7500]。 */
const WORKER_QUOTA_ERROR =
  "D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details.";

async function expectQuotaResponse(response: Response): Promise<void> {
  expect(response.status).toBe(503);
  const body = (await response.json()) as { error: { code: string; message: string } };
  expect(body.error.code).toBe('storage_quota_exceeded');
  expect(body.error.message).toBe('服务端存储写入额度已用尽，请稍后重试。');
}

describe('D1 配额耗尽判定（两种错误文本形态）', () => {
  it('REST 形态（带 [code: 7500]）⇒ 命中', () => {
    expect(isD1RowWriteQuotaError(new Error(REST_QUOTA_ERROR))).toBe(true);
  });

  it('Worker 内绑定形态（D1_ERROR，无 [code: 7500]）⇒ 命中', () => {
    expect(isD1RowWriteQuotaError(new Error(WORKER_QUOTA_ERROR))).toBe(true);
  });

  it('不是 Error 实例的普通对象 ⇒ 命中', () => {
    expect(isD1RowWriteQuotaError({ message: WORKER_QUOTA_ERROR })).toBe(true);
  });

  it('字符串形态 ⇒ 命中', () => {
    expect(isD1RowWriteQuotaError(WORKER_QUOTA_ERROR)).toBe(true);
  });

  it('无关错误 ⇒ 不命中（不误伤）', () => {
    expect(isD1RowWriteQuotaError(new Error('D1_ERROR: no such table: users'))).toBe(false);
    expect(isD1RowWriteQuotaError(new Error('boom'))).toBe(false);
  });
});

describe('D1 配额耗尽 ⇒ 对外 503 storage_quota_exceeded（不是笼统 500）', () => {
  it('REST 形态 ⇒ 503', async () => {
    await expectQuotaResponse(toErrorResponse(new Error(REST_QUOTA_ERROR)));
  });

  it('Worker 内绑定形态 ⇒ 503', async () => {
    await expectQuotaResponse(toErrorResponse(new Error(WORKER_QUOTA_ERROR)));
  });

  it('非 Error 实例（普通对象）⇒ 503', async () => {
    await expectQuotaResponse(toErrorResponse({ message: WORKER_QUOTA_ERROR }));
  });

  it('字符串形态 ⇒ 503', async () => {
    await expectQuotaResponse(toErrorResponse(WORKER_QUOTA_ERROR));
  });

  it('无关错误 ⇒ 仍是 500 internal_error', async () => {
    const response = toErrorResponse(new Error('boom'));
    expect(response.status).toBe(500);
    const body = (await response.json()) as { error: { code: string } };
    expect(body.error.code).toBe('internal_error');
  });
});
