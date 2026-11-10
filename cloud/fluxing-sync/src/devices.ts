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