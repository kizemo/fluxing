// 清空云端数据（plan §6.1：DELETE /v1/account/data → {ok}）。
//
// 语义（plan.md:181 + spec.md FR-014 双证）：
//   · 只清「同步内容」，**不删账号**：users / devices / refresh_tokens / email_verifications
//     全部保留，清空后仍可继续登录与同步。
//   · 「重置账号并清空云端」（spec 3.3 / R7）是另一条未纳入首版的流程。
//
// ADR 0077 之后这里要清**两套并存**的同步内容：
//   · 逐条路径：R2 前缀 `u/<user_id>/`、D1 表 `sync_items`；
//   · 桶级路径：R2 前缀 `b/<user_id>/`、D1 表 `sync_buckets`。
//   两个前缀同桶同级 ⇒ 一次 list 一个前缀，清完逐条**不会**漏掉桶对象。
//   注意：`sync_buckets` 由 migrations/0004 建表，**部署前必须先应用该迁移**，否则本接口报错。
//
// 不变量：
//   1. 不吞失败：R2 批量删重试后仍失败 ⇒ 500 clear_incomplete，让客户端可原样重试；
//      半清干净比报错更糟——客户端会以为云端已空。
//   2. 成功前自证：每个前缀都再列一次，非空即视为失败。
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

/** 清空一个 R2 前缀下的全部对象，返回删除的对象数。 */
async function clearPrefix(env: Env, prefix: string): Promise<number> {
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

  // 自证：前缀必须为空，否则不报成功
  const probe = await env.BUCKET.list({ prefix, limit: 1 });
  if ((probe.objects ?? []).length > 0) {
    throw fail.internal('云端数据未完全清除，请稍后重试。', 'clear_incomplete');
  }

  return deletedObjects;
}

export async function handleDeleteAccountData(env: Env, auth: AuthContext): Promise<unknown> {
  // 逐条前缀 + 桶前缀（ADR 0077）：两个前缀都要清，且各自自证。
  // deleted_objects 的字段名保持不变，含义扩为「两个前缀删除的对象总数」。
  let deletedObjects = 0;
  deletedObjects += await clearPrefix(env, `u/${auth.userId}/`);
  deletedObjects += await clearPrefix(env, `b/${auth.userId}/`);

  // D1 索引：sync_items / sync_buckets 才是「云端有什么」的权威来源
  const removed = await env.DB.prepare('DELETE FROM sync_items WHERE user_id = ?')
    .bind(auth.userId)
    .run();
  const deletedItems = removed.meta?.changes ?? 0;

  const removedBuckets = await env.DB.prepare('DELETE FROM sync_buckets WHERE user_id = ?')
    .bind(auth.userId)
    .run();
  const deletedBucketRows = removedBuckets.meta?.changes ?? 0;

  return {
    ok: true,
    deleted_objects: deletedObjects,
    deleted_items: deletedItems,
    deleted_bucket_rows: deletedBucketRows,
  };
}