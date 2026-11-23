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
import { writeBudgetRowsPerDay } from './env';
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
import { putAllWithConcurrency } from './r2-write';

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

// ---------------------------------------------------------------- 写预算护栏
//
// ADR 0076 D2-3：每账号每 UTC 日的 D1 行写入预算护栏。
//
// 目的：把「一个人（或一台机器）一次性全量重推」这个写放大源，从
// 「打爆全库 D1 免费额度 ⇒ 该库内**所有**账号的写操作一起失效」
// 降级为「单账号可诊断的 429 write_budget_exceeded」。
//
// **口径（如实说明：这是估算，不是精确计费器）**：
//   · used = 该用户**今日（UTC）**在 sync_items 里当前仍存在、且 updated_at 落在
//     今日 00:00 UTC 之后的**行数**（纯读 COUNT）。同一行当日被反复写只计 1
//     ⇒ 真实行写入数 ≥ used ⇒ 本护栏只会**低估**，不会高估。
//   · planned = 本批**守卫放行**的条目预计产生的行写入数：
//       - 该 item_key 在本批之前已存在于 sync_items ⇒ 2 行（DELETE + INSERT）；
//       - 该 item_key 本批首次出现 ⇒ 1 行（INSERT；配套 DELETE 命中 0 行）；
//       - 同一 item_key 在本批内重复出现 ⇒ 从第 2 次起按 2 行计（此时行已存在）。
//     ⇒ 护栏针对的是「一次性全量 / 批量重推」这一真正的放大源，
//     **不宣称**它能精确计出 Cloudflare 的行写入账单。
//   · limit = 每账号每日 80,000 行（环境变量 WRITE_BUDGET_ROWS_PER_DAY 可覆盖）。
//   · 判定：used + planned > limit ⇒ **整批拒绝**（429），且拒绝发生在
//     **任何写动作之前** ⇒ R2 与 D1 都是**零写入**。

/** 超限时返回的稳定错误码（客户端只读 message；code 供服务端与探针诊断）。 */
export const WRITE_BUDGET_EXCEEDED_CODE = 'write_budget_exceeded';

/** 超限时返回的中文文案（constitution P2/P5：用户可见字符串保持简体中文）。 */
export const WRITE_BUDGET_EXCEEDED_MESSAGE = '今日云端写入已达上限，请明天再试或减少本机词典规模。';

/** 预算判定输入。 */
export interface WriteBudgetInput {
  /** 该用户今日（UTC）已写行数（纯读取得）。 */
  usedRows: number;
  /** 本批预计产生的 D1 行写入数。 */
  plannedRows: number;
  /** 每账号每日上限（行）。 */
  limitRows: number;
}

/** 预算判定结果。 */
export interface WriteBudgetResult {
  /** 是否放行。 */
  allowed: boolean;
  /** 判定原因：'within_budget' = 放行；'budget_exceeded' = 超限（整批拒绝）。 */
  reason: 'within_budget' | 'budget_exceeded';
  usedRows: number;
  plannedRows: number;
  limitRows: number;
}

/**
 * 写预算判定（纯函数，无 IO）。
 *
 * 判定式：`usedRows + plannedRows > limitRows` ⇒ 超限。
 * **恰好等于阈值放行**（`>` 而非 `>=`）：与 ADR 0076 D2-3 的原文
 * 「估算值 + planned > 80,000 行 ⇒ 整批拒绝」逐字一致。
 */
export function evaluateWriteBudget(input: WriteBudgetInput): WriteBudgetResult {
  const usedRows = input.usedRows;
  const plannedRows = input.plannedRows;
  const limitRows = input.limitRows;
  const allowed = usedRows + plannedRows <= limitRows;
  return {
    allowed,
    reason: allowed ? 'within_budget' : 'budget_exceeded',
    usedRows,
    plannedRows,
    limitRows,
  };
}

/**
 * 估算本批将产生的 D1 行写入数（纯函数）。
 *
 * @param acceptedItemKeys 守卫放行的条目键（按写入顺序）
 * @param existingItemKeys 本批之前已存在于 sync_items 的条目键
 */
export function planWriteRows(
  acceptedItemKeys: readonly string[],
  existingItemKeys: ReadonlySet<string>,
): number {
  let planned = 0;
  const seen = new Set<string>();
  for (const key of acceptedItemKeys) {
    if (seen.has(key) || existingItemKeys.has(key)) {
      // 行已存在 ⇒ DELETE 命中 1 行 + INSERT 1 行 = 2 行
      planned += 2;
    } else {
      // 本批首次出现 ⇒ DELETE 命中 0 行 + INSERT 1 行 = 1 行
      planned += 1;
    }
    seen.add(key);
  }
  return planned;
}

/** 今日 00:00 UTC 的 epoch 秒（epoch 本身与 UTC 对齐 ⇒ 直接整除 86400）。 */
export function utcDayStartSeconds(now: number): number {
  return now - (now % 86_400);
}

/**
 * 取该用户今日（UTC）已写行数（**纯读**，不写任何一行）。
 *
 * 口径：sync_items 中 user_id 匹配、且 updated_at >= 今日 00:00 UTC 的行数。
 * 列名以 migrations/0001_init.sql 为准（sync_items.updated_at，单位秒）。
 */
export async function countTodayWrittenRows(
  env: Env,
  userId: string,
  now: number,
): Promise<number> {
  const dayStart = utcDayStartSeconds(now);
  const row = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ? AND updated_at >= ?',
  )
    .bind(userId, dayStart)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

/**
 * 推送前的写预算判定（读 + 判定，不写任何一行）。
 *
 * @param limitRows 每日上限；缺省从环境变量读（读不到则回落默认常量）
 */
export async function evaluatePushWriteBudget(
  env: Env,
  userId: string,
  acceptedItemKeys: readonly string[],
  existingItemKeys: ReadonlySet<string>,
  now: number,
  limitRows: number = writeBudgetRowsPerDay(env),
): Promise<WriteBudgetResult> {
  const plannedRows = planWriteRows(acceptedItemKeys, existingItemKeys);
  const usedRows = await countTodayWrittenRows(env, userId, now);
  return evaluateWriteBudget({ usedRows, plannedRows, limitRows });
}

// ---------------------------------------------------------------- 推送

export async function handlePush(
  env: Env,
  auth: AuthContext,
  body: Record<string, unknown>,
): Promise<unknown> {
  return handlePushWithLimit(env, auth, body, writeBudgetRowsPerDay(env));
}

/**
 * 推送主流程（与 handlePush 同一条路径，只是把每日写预算上限显式传进来）。
 *
 * 单独暴露 limitRows 参数是为了让单测能在不改动 worker 绑定的前提下注入小阈值，
 * 验证「超限 ⇒ 429 + 零写入」这条路径。生产调用方一律走 handlePush。
 */
export async function handlePushWithLimit(
  env: Env,
  auth: AuthContext,
  body: Record<string, unknown>,
  limitRows: number,
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

  // 写预算护栏（ADR 0076 D2-3）：**任何写动作之前**判定 ⇒ 超限时 R2 与 D1 都零写入。
  // 口径见本文件「写预算护栏」小节：used = 今日已写行数（纯读），
  // planned = 守卫放行条目的预计行写入数（新条目 1 行 / 已存在行 2 行）。
  const existingItemKeys = new Set<string>((existing.results ?? []).map((row) => row.item_key));
  const budget = await evaluatePushWriteBudget(
    env,
    auth.userId,
    accepted.map((item) => item.itemKey),
    existingItemKeys,
    now,
    limitRows,
  );
  if (!budget.allowed) {
    throw fail.tooFrequent(WRITE_BUDGET_EXCEEDED_MESSAGE, WRITE_BUDGET_EXCEEDED_CODE);
  }

  // 1) 先写 R2（同 key 覆盖，天然幂等）；跳过的条目不写
  const staged: Array<{ itemKey: string; r2Key: string }> = [];
  for (const item of accepted) {
    const keyHash = await sha256Hex(utf8(item.itemKey));
    staged.push({ itemKey: item.itemKey, r2Key: await r2KeyFor(auth.userId, keyHash) });
  }
  // R2 阶段内部并发化（ADR 0074）：单条 put ≈ 626–706 ms，串行 64 条 = 40 071 ms，
  // 超过客户端 15 s 接收超时 ⇒ 真机报 12002（WinHTTP ERROR_WINHTTP_TIMEOUT）。
  // 并发度取 6，写序仍是「R2 先、D1 后」，条目数上限仍是 64。
  await putAllWithConcurrency(
    env.BUCKET,
    accepted.map((item, i) => ({
      key: staged[i]!.r2Key,
      value: item.cipherBytes,
      options: {
        httpMetadata: { contentType: 'application/octet-stream' },
        // 自定义元数据只放纯 ASCII（item_key / hlc_device 可能是中文），
        // nonce 的权威来源是 D1 的 nonce_b64 列，这里只是给运维排查用。
        customMetadata: {
          type: item.type,
          hlc_ms: String(item.hlc.ms),
          hlc_counter: String(item.hlc.counter),
          nonce_b64: item.nonceB64,
        },
      },
    })),
  );

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