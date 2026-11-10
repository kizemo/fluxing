// 同步推送 / 拉取（plan §6.1 接口、§6.3 R2 布局）。
//
// 不变量：
//   1. 服务端只见密文：cipher 原样落 R2，不解密、不解析信封 8 行文本、不比较明文。
//   2. 写序 R2 先、D1 后。D1 失败只留「孤儿对象」（下一次同 key 推送直接覆盖），
//      绝不出现「D1 有索引、R2 没内容」的悬挂引用。
//   3. server_seq 取「本次推送涉及条目的最大 sync_items.id」，不用 D1 的
//      `meta.last_row_id`：后者是连接级游标，可能跨表取到客户端尚未见过的行号 ⇒
//      客户端把 since 设过去就永久漏推。
//   4. 单条非法 ⇒ 整批 400（不做部分接受）。客户端拿到的「全有或全无」比半成功好恢复。
//   5. **HLC 守卫**（与客户端 `FluxingCloudEnvelope.h` 的 `CompareHlc` 同一套规则）：
//      同一 (user_id, item_key) 上，服务端已存版本 ≥ 客户端送上来的版本 ⇒ 该条
//      整条跳过（不写 R2、不写 D1、不推进 server_seq）。没有这条守卫，「客户端把
//      合并后的条目回传」会把服务端较新的版本回退成旧版 ⇒ 两端乒乓，永不收敛。

import type { Env } from './env';
import type { AuthContext } from './auth';
import { fail } from './errors';
import {
  fromBase64Strict,
  nowSeconds,
  sha256Hex,
  toBase64,
  toBase64Url,
  utf8,
} from './crypto';
import { MAX_PUSH_ITEMS, compareHlc, r2KeyFor, validatePushItem } from './wire';
import type { Hlc, ValidatedItem } from './wire';

const DEFAULT_PULL_LIMIT = 50;
const MAX_PULL_LIMIT = 100;

interface SyncRow {
  id: number;
  item_key: string;
  type: string;
  hlc_ms: number;
  hlc_counter: number;
  hlc_device: string;
  r2_key: string;
  nonce_b64: string;
  cipher_len: number;
}

// ---------------------------------------------------------------- 入参解析

function parseSince(raw: string | null): number {
  if (raw === null || raw.length === 0) return 0;
  if (!/^\d{1,15}$/.test(raw)) {
    throw fail.badRequest('since 必须是非负整数序号。', 'bad_since');
  }
  const value = Number.parseInt(raw, 10);
  if (!Number.isSafeInteger(value)) throw fail.badRequest('since 数值过大。', 'bad_since');
  return value;
}

function parseLimit(raw: string | null): number {
  if (raw === null || raw.length === 0) return DEFAULT_PULL_LIMIT;
  if (!/^\d{1,4}$/.test(raw)) {
    throw fail.badRequest('limit 必须是正整数。', 'bad_limit');
  }
  const value = Number.parseInt(raw, 10);
  if (value < 1) throw fail.badRequest('limit 必须大于 0。', 'bad_limit');
  return Math.min(value, MAX_PULL_LIMIT);
}

/** cursor 是 base64url(十进制 id)，不透明；解析失败一律 400，不做容错猜测。 */
function parseCursor(raw: string | null): number | null {
  if (raw === null || raw.length === 0) return null;
  const bytes = fromBase64Strict(raw, 64);
  if (bytes === null) throw fail.badRequest('cursor 无效。', 'bad_cursor');
  const text = new TextDecoder().decode(bytes);
  if (!/^\d{1,15}$/.test(text)) throw fail.badRequest('cursor 无效。', 'bad_cursor');
  const value = Number.parseInt(text, 10);
  if (!Number.isSafeInteger(value)) throw fail.badRequest('cursor 无效。', 'bad_cursor');
  return value;
}

function encodeCursor(id: number): string {
  return toBase64Url(utf8(String(id)));
}

// ---------------------------------------------------------------- 推送

export async function handlePush(
  env: Env,
  auth: AuthContext,
  body: Record<string, unknown>,
): Promise<unknown> {
  // T101：未验证邮箱的账号不得写入同步数据（只拦写，不拦读与设备管理）
  if (!auth.emailVerified) {
    throw fail.forbidden('邮箱尚未验证，请先完成邮箱验证再同步数据。', 'email_not_verified');
  }

  const rawItems = body.items;
  if (!Array.isArray(rawItems)) throw fail.badRequest('items 必须是数组。', 'bad_items');
  if (rawItems.length === 0) throw fail.badRequest('items 不能为空。', 'bad_items');
  if (rawItems.length > MAX_PUSH_ITEMS) {
    throw fail.badRequest(
      `单次推送条目数上限为 ${MAX_PUSH_ITEMS}（实收 ${rawItems.length}）。`,
      'too_many_items',
    );
  }

  // 先整体校验，任一条非法就什么都不写
  const items = rawItems.map((raw, index) => validatePushItem(raw, index));

  const now = nowSeconds();

  // 取本批涉及的既有行，作为 HLC 守卫的比较基准
  const uniqueKeys = [...new Set(items.map((item) => item.itemKey))];
  const placeholders = uniqueKeys.map(() => '?').join(', ');
  const existing = await env.DB.prepare(
    `SELECT item_key, hlc_ms, hlc_counter, hlc_device
       FROM sync_items
      WHERE user_id = ? AND item_key IN (${placeholders})`,
  )
    .bind(auth.userId, ...uniqueKeys)
    .all<{
      item_key: string;
      hlc_ms: number;
      hlc_counter: number;
      hlc_device: string;
    }>();

  // 逐条守卫：服务端已存版本 ≥ 客户端送上来的版本 ⇒ 整条跳过（不写 R2 / D1，
  // 也不推进 server_seq）。`current` 随本批已接受的条目一起更新，因此同一批里
  // 重复出现的 item_key 也按「先到先得、后到者与已接受版本比较」处理。
  const current = new Map<string, Hlc>();
  for (const row of existing.results ?? []) {
    current.set(row.item_key, {
      ms: row.hlc_ms,
      counter: row.hlc_counter,
      device: row.hlc_device,
    });
  }
  const accepted: ValidatedItem[] = [];
  const skipped: string[] = [];
  for (const item of items) {
    const stored = current.get(item.itemKey);
    if (stored !== undefined && compareHlc(stored, item.hlc) >= 0) {
      skipped.push(item.itemKey);
      continue;
    }
    current.set(item.itemKey, item.hlc);
    accepted.push(item);
  }

  // 1) 先写 R2（同 key 覆盖，天然幂等）；跳过的条目不写
  const staged: Array<{ itemKey: string; r2Key: string }> = [];
  for (const item of accepted) {
    const keyHash = await sha256Hex(utf8(item.itemKey));
    staged.push({ itemKey: item.itemKey, r2Key: await r2KeyFor(auth.userId, keyHash) });
  }
  for (let i = 0; i < accepted.length; i += 1) {
    const item = accepted[i]!;
    const target = staged[i]!;
    await env.BUCKET.put(target.r2Key, item.cipherBytes, {
      httpMetadata: { contentType: 'application/octet-stream' },
      // 自定义元数据只放纯 ASCII（item_key / hlc_device 可能是中文），
      // nonce 的权威来源是 D1 的 nonce_b64 列，这里只是给运维排查用。
      customMetadata: {
        type: item.type,
        hlc_ms: String(item.hlc.ms),
        hlc_counter: String(item.hlc.counter),
        nonce_b64: item.nonceB64,
      },
    });
  }

  // 2) 再写 D1：DELETE + INSERT，让 UNIQUE(user_id, item_key) 下拿到新的自增 id，
  //    这样同一条目重推也能让 server_seq 前进（否则客户端会以为「没变化」）。
  const statements: D1PreparedStatement[] = [];
  for (let i = 0; i < accepted.length; i += 1) {
    const item = accepted[i]!;
    statements.push(
      env.DB.prepare('DELETE FROM sync_items WHERE user_id = ? AND item_key = ?').bind(
        auth.userId,
        item.itemKey,
      ),
    );
    statements.push(
      env.DB
        .prepare(
          `INSERT INTO sync_items
             (user_id, item_key, type, hlc_ms, hlc_counter, hlc_device, r2_key, nonce_b64, cipher_len, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        )
        .bind(
          auth.userId,
          item.itemKey,
          item.type,
          item.hlc.ms,
          item.hlc.counter,
          item.hlc.device,
          staged[i]!.r2Key,
          item.nonceB64,
          item.cipherBytes.length,
          now,
        ),
    );
  }
  if (statements.length > 0) await env.DB.batch(statements);

  // 3) server_seq = 本次**真正写入**条目的最大 id（跳过不推进）
  const writtenKeys = [...new Set(accepted.map((item) => item.itemKey))];
  let serverSeq = 0;
  if (writtenKeys.length > 0) {
    const writtenPlaceholders = writtenKeys.map(() => '?').join(', ');
    const seqRow = await env.DB
      .prepare(
        `SELECT COALESCE(MAX(id), 0) AS seq FROM sync_items WHERE user_id = ? AND item_key IN (${writtenPlaceholders})`,
      )
      .bind(auth.userId, ...writtenKeys)
      .first<{ seq: number }>();
    serverSeq = seqRow?.seq ?? 0;
  } else {
    // 本批没有任何条目被写入（全部跳过）：游标不能倒退 ⇒ 取该用户当前已有的最大 id
    const seqRow = await env.DB
      .prepare('SELECT COALESCE(MAX(id), 0) AS seq FROM sync_items WHERE user_id = ?')
      .bind(auth.userId)
      .first<{ seq: number }>();
    serverSeq = seqRow?.seq ?? 0;
  }

  return {
    accepted: accepted.map((item) => item.itemKey),
    skipped,
    server_seq: serverSeq,
  };
}

// ---------------------------------------------------------------- 拉取

export async function handlePull(env: Env, auth: AuthContext, url: URL): Promise<unknown> {
  const since = parseSince(url.searchParams.get('since'));
  const limit = parseLimit(url.searchParams.get('limit'));
  const cursor = parseCursor(url.searchParams.get('cursor'));
  // 两个入口给同一个游标：cursor 是上一页的续读位置，since 是全量起点，取较大者防回退
  const after = Math.max(since, cursor ?? 0);

  const listed = await env.DB
    .prepare(
      `SELECT id, item_key, type, hlc_ms, hlc_counter, hlc_device, r2_key, nonce_b64, cipher_len
         FROM sync_items
        WHERE user_id = ? AND id > ?
        ORDER BY id ASC
        LIMIT ?`,
    )
    .bind(auth.userId, after, limit + 1)
    .all<SyncRow>();

  const rows = listed.results ?? [];
  const hasMore = rows.length > limit;
  const page = hasMore ? rows.slice(0, limit) : rows;

  const items: Array<Record<string, unknown>> = [];
  const healed: number[] = [];
  for (const row of page) {
    const object = await env.BUCKET.get(row.r2_key);
    if (object === null) {
      // 自愈：R2 对象缺失（历史孤儿 / 手工清理过桶）。删掉索引行并告警，不抛 500 ——
      // 抛错会让客户端卡在同一个游标上无限重试，比丢一条已被破坏的条目更糟。
      healed.push(row.id);
      continue;
    }
    const cipher = new Uint8Array(await object.arrayBuffer());
    if (cipher.length !== row.cipher_len) {
      console.warn(
        `[fluxing-sync] 密文长度不一致 r2_key=${row.r2_key} 索引=${row.cipher_len} 实际=${cipher.length}`,
      );
    }
    items.push({
      item_key: row.item_key,
      type: row.type,
      hlc: { ms: row.hlc_ms, counter: row.hlc_counter, device: row.hlc_device },
      nonce: row.nonce_b64,
      cipher: toBase64(cipher),
      server_seq: row.id,
    });
  }

  if (healed.length > 0) {
    console.warn(`[fluxing-sync] 清理缺失对象的索引行 ${healed.length} 条：${healed.join(',')}`);
    await env.DB.batch(
      healed.map((id) => env.DB.prepare('DELETE FROM sync_items WHERE id = ?').bind(id)),
    );
  }

  const lastRow = page.length > 0 ? page[page.length - 1]! : null;
  return {
    items,
    next_cursor: lastRow === null ? null : encodeCursor(lastRow.id),
    has_more: hasMore,
    // 顶层 server_seq：本次响应覆盖到的最大序号。
    // 客户端契约（WeaselServer/Cloud/CloudSyncService.h 的 ParsePullResponse）要求
    // pull 响应带顶层 server_seq；字段缺失时客户端把游标写成 0 ⇒ 每次都重复拉同一批。
    // 空页时沿用请求游标位置（after）：本次没有任何新条目，游标不能倒退。
    server_seq: lastRow === null ? after : lastRow.id,
  };
}