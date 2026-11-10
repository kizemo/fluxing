// 清空云端数据（plan §6.1：DELETE /v1/account/data → {ok}）。
//
// 语义（plan.md:181 + spec.md FR-014 双证）：
//   · 只清「同步内容」，**不删账号**：users / devices / refresh_tokens / email_verifications
//     全部保留，清空后仍可继续登录与同步。
//   · 「重置账号并清空云端」（spec 3.3 / R7）是另一条未纳入首版的流程。
//
// 不变量：
//   1. 不吞失败：R2 批量删重试后仍失败 ⇒ 500 clear_incomplete，让客户端可原样重试；
//      半清干净比报错更糟——客户端会以为云端已空。
//   2. 成功前自证：再列一次前缀，非空即视为失败。
//   3. 每轮都从「前缀起点」重新 list，不沿用上一轮的 cursor：批量删之后旧游标可能跳过对象。

import type { Env } from './env';
import type { AuthContext } from './auth';
import { fail } from './errors';

/** 单次 list 上限（R2 允许的最大值） */
const LIST_LIMIT = 1000;
/** 单次 delete 的条数上限，避免超大请求体 */
const DELETE_BATCH = 500;
/** 单批删除的重试次数 */
const DELETE_ATTEMPTS = 3;
/** 轮次上限：1000 × 1000 = 100 万对象，超过就说明前缀清不掉 */
const MAX_ROUNDS = 1000;

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function deleteBatch(env: Env, keys: string[]): Promise<void> {
  let lastError: unknown;
  for (let attempt = 1; attempt <= DELETE_ATTEMPTS; attempt += 1) {
    try {
      await env.BUCKET.delete(keys);
      return;
    } catch (error) {
      lastError = error;
      if (attempt < DELETE_ATTEMPTS) await sleep(attempt * 50);
    }
  }
  console.error('[fluxing-sync] 删除 R2 对象失败：', lastError);
  throw fail.internal('云端数据未能完全清除，请稍后重试。', 'clear_incomplete');
}

export async function handleDeleteAccountData(env: Env, auth: AuthContext): Promise<unknown> {
  const prefix = `u/${auth.userId}/`;
  let deletedObjects = 0;

  for (let round = 0; round < MAX_ROUNDS; round += 1) {
    const listed = await env.BUCKET.list({ prefix, limit: LIST_LIMIT });
    const keys = (listed.objects ?? []).map((object) => object.key);
    if (keys.length === 0) break;

    for (let offset = 0; offset < keys.length; offset += DELETE_BATCH) {
      const slice = keys.slice(offset, offset + DELETE_BATCH);
      await deleteBatch(env, slice);
      deletedObjects += slice.length;
    }

    if (round === MAX_ROUNDS - 1) {
      throw fail.internal('云端对象数量异常，未能完成清除，请稍后重试。', 'clear_incomplete');
    }
  }

  // D1 索引：sync_items 才是「云端有什么」的权威来源
  const removed = await env.DB.prepare('DELETE FROM sync_items WHERE user_id = ?')
    .bind(auth.userId)
    .run();
  const deletedItems = removed.meta?.changes ?? 0;

  // 自证：前缀必须为空，否则不报成功
  const probe = await env.BUCKET.list({ prefix, limit: 1 });
  if ((probe.objects ?? []).length > 0) {
    throw fail.internal('云端数据未完全清除，请稍后重试。', 'clear_incomplete');
  }

  return { ok: true, deleted_objects: deletedObjects, deleted_items: deletedItems };
}