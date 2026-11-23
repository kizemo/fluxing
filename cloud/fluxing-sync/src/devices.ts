// 设备管理（plan §6.1：GET /v1/devices、DELETE /v1/devices/:id）。
//
// 与 §6.1 的两处补充（均 additive，不改既有字段名）：
//   · 列表项多返回 `id` / `revoked` / `current`。§6.1 的 `[{device_id, name, last_seen, created}]`
//     里没有可用来调 DELETE 的句柄，`id` 正是路径参数；`current` 让账号页能标「本机」。
//   · DELETE 的 `:id` 同时接受 devices.id（服务端 UUID）与 devices.device_id（客户端稳定标识），
//     两者都经过同一 user_id 约束，不会越权。

import type { Env } from './env';
import type { AuthContext } from './auth';
import { fail } from './errors';
import { nowSeconds } from './crypto';

interface DeviceRow {
  id: string;
  device_id: string;
  name: string;
  last_seen: number;
  revoked_at: number | null;
  created_at: number;
}

/**
 * D3-1：每账号设备行数上限（ADR 0076）。
 *
 * 背景：device_id 由**凭据生命周期**决定（客户端只在凭据为空时生成）⇒
 * 换机 / 重建凭据 = 服务端多一台「设备」，而旧实现对 devices 表**没有任何上限**。
 */
export const MAX_DEVICES_PER_ACCOUNT = 32;

/** 统计该账号的设备行数。 */
async function countDevices(env: Env, userId: string): Promise<number> {
  const row = await env.DB.prepare('SELECT COUNT(*) AS n FROM devices WHERE user_id = ?')
    .bind(userId)
    .first<{ n: number }>();
  return Number(row?.n ?? 0);
}

/**
 * 回收「已撤销且最久未见」的设备行（最多 limit 行）。
 *
 * D3-2：回收**只动** devices 行与该设备 refresh_tokens 的 revoked_at；
 * **不动** sync_items、**不动** R2 对象、不动用户的同步数据。
 *
 * 令牌的撤销是必需的：设备行被删后，鉴权里的 LEFT JOIN devices 拿不到设备行 ⇒
 * 该设备已签发的 access token 会因为「设备行不存在」而绕过设备撤销检查 ⇒ 必须先撤销令牌。
 */
async function reclaimRevokedDevices(env: Env, userId: string, limit: number): Promise<number> {
  if (limit <= 0) return 0;
  const revoked = await env.DB.prepare(
    `SELECT id FROM devices
      WHERE user_id = ? AND revoked_at IS NOT NULL
      ORDER BY last_seen ASC, created_at ASC
      LIMIT ?`,
  )
    .bind(userId, limit)
    .all<{ id: string }>();

  const now = nowSeconds();
  let reclaimed = 0;
  for (const row of revoked.results ?? []) {
    await env.DB.prepare(
      'UPDATE refresh_tokens SET revoked_at = COALESCE(revoked_at, ?) WHERE device_id = ?',
    )
      .bind(now, row.id)
      .run();
    await env.DB.prepare('DELETE FROM devices WHERE id = ?').bind(row.id).run();
    reclaimed += 1;
  }
  return reclaimed;
}

/**
 * D3-1：设备上限护栏（登录落设备前调用）。
 *
 * 口径（ADR 0076 D3-1）：
 *   · 统计该账号的 devices 行数；< 上限 ⇒ 直接放行。
 *   · ≥ 上限 ⇒ 先回收「已撤销且最久未见」的设备行（DELETE + 撤销该设备的令牌）。
 *   · 回收后仍 ≥ 上限 ⇒ **拒绝登录**（429 device_limit_reached + 中文文案）。
 *
 * 硬约束：**绝不自动删除仍在使用的设备**（未撤销的设备行一律保留）
 * ⇒ 达到上限后必须由用户在账号页撤销旧设备才能继续登录。
 * 这是 ADR「已知副作用」段明确登记的**有意硬约束**，不是缺陷。
 */
export async function enforceDeviceLimit(env: Env, userId: string): Promise<void> {
  const count = await countDevices(env, userId);
  if (count < MAX_DEVICES_PER_ACCOUNT) return;

  // 回收：删到「装下这一台之后仍不超上限」为止（只删已撤销的行）。
  const target = count - (MAX_DEVICES_PER_ACCOUNT - 1);
  const reclaimed = await reclaimRevokedDevices(env, userId, target);
  if (count - reclaimed < MAX_DEVICES_PER_ACCOUNT) return;

  throw fail.tooFrequent(
    `此账号的设备数已达上限（${MAX_DEVICES_PER_ACCOUNT} 台），请在账号页撤销不用的设备后再登录。`,
    'device_limit_reached',
  );
}

export async function handleDeviceList(env: Env, auth: AuthContext): Promise<unknown> {
  const listed = await env.DB
    .prepare(
      `SELECT id, device_id, name, last_seen, revoked_at, created_at
         FROM devices
        WHERE user_id = ?
        ORDER BY created_at ASC`,
    )
    .bind(auth.userId)
    .all<DeviceRow>();

  return (listed.results ?? []).map((row) => ({
    id: row.id,
    device_id: row.device_id,
    name: row.name,
    last_seen: row.last_seen,
    created: row.created_at,
    revoked: row.revoked_at !== null,
    current: row.id === auth.deviceRowId,
  }));
}

export async function handleDeviceRevoke(
  env: Env,
  auth: AuthContext,
  rawSegment: string,
): Promise<unknown> {
  let target: string;
  try {
    target = decodeURIComponent(rawSegment).trim();
  } catch {
    throw fail.badRequest('设备标识不是合法的 URL 编码。', 'bad_device_id');
  }
  if (target.length === 0) throw fail.badRequest('设备标识不能为空。', 'bad_device_id');

  const row = await env.DB
    .prepare('SELECT id FROM devices WHERE user_id = ? AND (id = ? OR device_id = ?)')
    .bind(auth.userId, target, target)
    .first<{ id: string }>();
  if (!row) throw fail.notFound('没有找到该设备（可能已被移除）。', 'device_not_found');

  const now = nowSeconds();
  // COALESCE 让重复调用保持幂等：已经撤销过就保留首次撤销时间，不刷新时间戳
  await env.DB.batch([
    env.DB
      .prepare('UPDATE devices SET revoked_at = COALESCE(revoked_at, ?) WHERE id = ?')
      .bind(now, row.id),
    // 该设备已发出的 access / refresh 令牌同时作废（FR-019 / AC-14）
    env.DB
      .prepare('UPDATE refresh_tokens SET revoked_at = COALESCE(revoked_at, ?) WHERE device_id = ?')
      .bind(now, row.id),
  ]);

  return { ok: true, id: row.id };
}