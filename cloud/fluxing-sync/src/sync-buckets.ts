// 桶级同步推送 / 拉取（ADR 0077 D1–D10；端点 POST /v1/sync/buckets/push、GET /v1/sync/buckets/pull）。
//
// 与 src/sync.ts 的关系：**并行**而非替代。老端点逐字不动（ADR 0077 D6），本模块只服务
// 新路径 ⇒ 灰度靠 URL 分流（客户端与服务端之间没有 version / capability 字段，
// 取证报告 §3.5 ⇒ 路径是唯一的分流手段）。
//
// 不变量（逐条继承 sync.ts 的五条，并按桶粒度改写）：
//   1. **服务端只见密文**：blob 原样落 R2。blob 是「N 个既有条目级 8 行信封文本块串接」
//      （ADR 0077 D3），服务端**不解析**它 —— 不数条目、不看 item_key、不比明文。
//      D3 同时否决了桶级二次 AEAD ⇒ 本模块没有桶级 nonce / 桶级 AAD 概念。
//   2. **写序 R2 先、D1 后**：D1 失败只留孤儿对象（下次同 (设备, 桶) 推送直接覆盖），
//      绝不出现「D1 有索引、R2 没内容」的悬挂引用。
//   3. **server_seq 取「本次真正写入的桶行的最大 sync_buckets.id」**，不用 D1 的
//      `meta.last_row_id`（连接级游标，可能跨表取到客户端尚未见过的行号）。
//      ⚠ 该 id 空间与 sync_items.id **不同**（ADR 0077 D6 新增约束）⇒ 客户端侧车必须
//      按协议版本分区并强制重置 seq = 0，禁止跨版本复用游标。
//   4. **单个非法 ⇒ 整批 400**（不做部分接受），与老端点同口径。
//   5. **HLC 守卫在「同一设备 × 同一桶」上生效**（ADR 0077 D4 + D5）：服务端已存桶级
//      HLC ≥ 客户端送上来的桶级 HLC ⇒ **整桶跳过**（不写 R2、不写 D1、不推进 server_seq）。
//      守卫作用域必须带 device_id，否则会跨设备误跳过（A 的新桶会永久压制 B 的推送）。
//      条目级 HLC 仍在 blob 内，由客户端 RemoteWins 逐条裁决 ⇒ 服务端无需理解。
//   6. **写预算护栏口径换表**（ADR 0077 D8）：used 从 sync_items COUNT 换成 sync_buckets
//      COUNT，planned 的估算对象从条目数换成桶数。判定式、错误码、中文文案**复用**老端点
//      那一套（WRITE_BUDGET_EXCEEDED_CODE / MESSAGE）⇒ 不新增错误面，客户端无需新分支。

import type { Env } from './env';
import { writeBudgetBucketRowsPerDay } from './env';
import type { AuthContext } from './auth';
import { fail } from './errors';
import { nowSeconds, toBase64, toBase64Url, utf8, fromBase64Strict } from './crypto';
import { MAX_PUSH_BUCKETS, bucketR2KeyForVersioned, compareHlc, validatePushBucket } from './wire';
import type { Hlc, ValidatedBucket } from './wire';
import { deleteAllQuietly, putAllWithConcurrency } from './r2-write';
import {
  WRITE_BUDGET_EXCEEDED_CODE,
  WRITE_BUDGET_EXCEEDED_MESSAGE,
  evaluateWriteBudget,
  planWriteRows,
  utcDayStartSeconds,
} from './sync';

const DEFAULT_PULL_LIMIT = 50;
const MAX_PULL_LIMIT = 100;

interface SyncBucketRow {
  id: number;
  bucket_key: string;
  device_id: string;
  hlc_ms: number;
  hlc_counter: number;
  hlc_device: string;
  r2_key: string;
  cipher_len: number;
  item_count: number;
}

// ---------------------------------------------------------------- 入参解析
//
// 三个 query 参数的解析与错误文案**逐字沿用** sync.ts 的同名私有函数（文案里不含
// 「条目」字样，对桶同样成立）；错误码也沿用 bad_since / bad_limit / bad_cursor，
// 不给客户端增加新的分支。唯一语义差别：limit 的单位从「条目数」变成「桶数」（D7）。

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

/** cursor 是 base64url(十进制桶行 id)，不透明；解析失败一律 400，不做容错猜测。 */
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

// ---------------------------------------------------------------- 设备归属
//
// ADR 0077 D1 只写了 device_id 这一列、D7 的示例值是 "<hex>"，**没有规定它的来源**。
// 本批定为「服务端权威」：行的 device_id 一律取 authenticate() 出来的 devices.id 行主键，
// 与 refresh_tokens.device_id 的既有先例逐字同构（0001_init.sql:46-47 原句
// 「指向 devices.id（行主键），不是客户端上报的 device_id」）。
//
// 客户端若在请求体里自带 device_id，则**只允许与自己的登录设备相等** ⇒ 防跨设备覆盖：
// 桶级守卫的作用域是 (user_id, device_id, bucket_key)，若能伪造 device_id，
// 一台机器就能把另一台机器的桶行整桶压制住（永不收敛）。

function resolvePushDeviceId(auth: AuthContext, body: Record<string, unknown>): string {
  const claimed = body.device_id;
  if (claimed === undefined || claimed === null) return auth.deviceRowId;
  if (typeof claimed !== 'string' || claimed.length === 0) {
    throw fail.badRequest('device_id 必须是非空字符串。', 'bad_device_id');
  }
  if (claimed !== auth.deviceRowId) {
    throw fail.badRequest(
      'device_id 与当前登录设备不一致，不能写入其他设备的桶数据。',
      'bad_device_id',
    );
  }
  return auth.deviceRowId;
}

// ---------------------------------------------------------------- 写预算护栏（桶口径）

/**
 * 取该用户今日（UTC）在 **sync_buckets** 已写的行数（**纯读**，不写任何一行）。
 *
 * 与 sync.ts 的 countTodayWrittenRows 同构，只是换表（ADR 0077 D8）：
 * 口径同样是「当日仍存在且 updated_at 落在今日 00:00 UTC 之后的行数」⇒ 只低估不高估。
 * 列名以 migrations/0004_sync_buckets.sql 为准（updated_at 单位秒）。
 */
export async function countTodayWrittenBucketRows(
  env: Env,
  userId: string,
  now: number,
): Promise<number> {
  const dayStart = utcDayStartSeconds(now);
  const row = await env.DB.prepare(
    'SELECT COUNT(*) AS n FROM sync_buckets WHERE user_id = ? AND updated_at >= ?',
  )
    .bind(userId, dayStart)
    .first<{ n: number }>();
  return row?.n ?? 0;
}

// ---------------------------------------------------------------- 推送

export async function handleBucketPush(
  env: Env,
  auth: AuthContext,
  body: Record<string, unknown>,
): Promise<unknown> {
  // 未验证邮箱的账号不得写入同步数据（与老端点同一道闸，只拦写不拦读）
  if (!auth.emailVerified) {
    throw fail.forbidden('邮箱尚未验证，请先完成邮箱验证再同步数据。', 'email_not_verified');
  }

  const deviceId = resolvePushDeviceId(auth, body);

  const rawBuckets = body.buckets;
  if (!Array.isArray(rawBuckets)) throw fail.badRequest('buckets 必须是数组。', 'bad_buckets');
  if (rawBuckets.length === 0) throw fail.badRequest('buckets 不能为空。', 'bad_buckets');
  if (rawBuckets.length > MAX_PUSH_BUCKETS) {
    throw fail.badRequest(
      `单次推送桶数上限为 ${MAX_PUSH_BUCKETS}（实收 ${rawBuckets.length}）。`,
      'too_many_buckets',
    );
  }

  // 先整体校验，任一个非法就什么都不写
  const buckets = rawBuckets.map((raw, index) => validatePushBucket(raw, index));

  const now = nowSeconds();

  // 取本批涉及的既有行（**必须带 device_id**：守卫只在同设备同桶上生效），作为 HLC 比较基准
  const uniqueKeys = [...new Set(buckets.map((bucket) => bucket.bucketKey))];
  const placeholders = uniqueKeys.map(() => '?').join(', ');
  const existing = await env.DB.prepare(
    `SELECT bucket_key, hlc_ms, hlc_counter, hlc_device, r2_key
       FROM sync_buckets
      WHERE user_id = ? AND device_id = ? AND bucket_key IN (${placeholders})`,
  )
    .bind(auth.userId, deviceId, ...uniqueKeys)
    .all<{
      bucket_key: string;
      hlc_ms: number;
      hlc_counter: number;
      hlc_device: string;
      r2_key: string;
    }>();

  // 变更前的对象键（ADR 0080 语义 3 的第二个条件）。必须从**预读**原始行取：下面的
  // batch 是 DELETE + INSERT，跑完旧 r2_key 即被抹掉。与守卫基准 `current` 分开存，
  // 否则本批 accepted 会把旧键覆盖成新键 ⇒ 删掉刚写的对象。作用域 = 同一设备同一桶。
  const previousR2Keys = new Map<string, string>();
  for (const row of existing.results ?? []) {
    if (typeof row.r2_key === 'string' && row.r2_key.length > 0) {
      previousR2Keys.set(row.bucket_key, row.r2_key);
    }
  }

  // 逐桶守卫：服务端已存版本 ≥ 客户端送上来的版本 ⇒ 整桶跳过。
  // `current` 随本批已接受的桶一起更新，因此同一批里重复出现的 bucket_key
  // 也按「先到先得、后到者与已接受版本比较」处理（与老端点逐字同构）。
  const current = new Map<string, Hlc>();
  for (const row of existing.results ?? []) {
    current.set(row.bucket_key, {
      ms: row.hlc_ms,
      counter: row.hlc_counter,
      device: row.hlc_device,
    });
  }
  const accepted: ValidatedBucket[] = [];
  const skipped: string[] = [];
  for (const bucket of buckets) {
    const stored = current.get(bucket.bucketKey);
    if (stored !== undefined && compareHlc(stored, bucket.hlc) >= 0) {
      skipped.push(bucket.bucketKey);
      continue;
    }
    current.set(bucket.bucketKey, bucket.hlc);
    accepted.push(bucket);
  }

  // 写预算护栏：**任何写动作之前**判定 ⇒ 超限时 R2 与 D1 都零写入。
  // planned 的估算复用 sync.ts 的 planWriteRows（口径与桶完全同构：新行 1 行、
  // 已存在行 DELETE+INSERT = 2 行），只是传的是 bucket_key 与「本设备」的既有键集合。
  const existingBucketKeys = new Set<string>((existing.results ?? []).map((row) => row.bucket_key));
  const planned = planWriteRows(
    accepted.map((bucket) => bucket.bucketKey),
    existingBucketKeys,
  );
  const used = await countTodayWrittenBucketRows(env, auth.userId, now);
  const budget = evaluateWriteBudget({
    usedRows: used,
    plannedRows: planned,
    limitRows: writeBudgetBucketRowsPerDay(env),
  });
  if (!budget.allowed) {
    throw fail.tooFrequent(WRITE_BUDGET_EXCEEDED_MESSAGE, WRITE_BUDGET_EXCEEDED_CODE);
  }

  // 1) 先写 R2（版本化键，ADR 0080 语义 2：新键先落，旧对象等 D1 提交成功再删）；
  //    跳过的桶不写。混合态合法：存量桶行仍指非版本化键，读路径只用行内 r2_key ⇒
  //    老桶不重推也照常可读，下次重推自然收敛到版本化键。
  //    R2 阶段内部并发度沿用 ADR 0074 的 6（D9 的 7.5 s / 64 put 实测据此成立）。
  const staged: Array<{ r2Key: string }> = [];
  for (const bucket of accepted) {
    staged.push({
      r2Key: await bucketR2KeyForVersioned(auth.userId, deviceId, bucket.bucketKey, bucket.hlc),
    });
  }
  await putAllWithConcurrency(
    env.BUCKET,
    accepted.map((bucket, i) => ({
      key: staged[i]!.r2Key,
      value: bucket.blobBytes,
      options: {
        httpMetadata: { contentType: 'application/octet-stream' },
        // 自定义元数据只放纯 ASCII 可控值（bucket_key / hlc_device 可能是中文），
        // 且不作为任何判据来源 —— blob 的权威在 R2 对象本身，元数据只是运维排查用。
        customMetadata: {
          kind: 'bucket',
          item_count: String(bucket.itemCount),
          blob_len: String(bucket.blobBytes.length),
          hlc_ms: String(bucket.hlc.ms),
          hlc_counter: String(bucket.hlc.counter),
        },
      },
    })),
  );

  // 2) 再写 D1：DELETE + INSERT（ADR 0076 D2-4 原样继承），让 UNIQUE 三元键下拿到新的
  //    自增 id ⇒ 同一桶重推也能让 server_seq 前进，否则客户端会以为「没变化」。
  const statements: D1PreparedStatement[] = [];
  for (let i = 0; i < accepted.length; i += 1) {
    const bucket = accepted[i]!;
    statements.push(
      env.DB
        .prepare('DELETE FROM sync_buckets WHERE user_id = ? AND device_id = ? AND bucket_key = ?')
        .bind(auth.userId, deviceId, bucket.bucketKey),
    );
    statements.push(
      env.DB
        .prepare(
          `INSERT INTO sync_buckets
             (user_id, device_id, bucket_key, hlc_ms, hlc_counter, hlc_device,
              r2_key, cipher_len, item_count, updated_at)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        )
        .bind(
          auth.userId,
          deviceId,
          bucket.bucketKey,
          bucket.hlc.ms,
          bucket.hlc.counter,
          bucket.hlc.device,
          staged[i]!.r2Key,
          bucket.blobBytes.length,
          bucket.itemCount,
          now,
        ),
    );
  }
  if (statements.length > 0) await env.DB.batch(statements);

  // 2.5) 旧版本桶对象回收（ADR 0080 语义 3）：**只在 batch 返回之后**。
  //      条件：batch 已成功 / 旧 r2_key 存在 / 旧 ≠ 新。删除不抛错（吞掉 + 逐键孤儿日志），
  //      否则已提交的一批会被判失败 ⇒ 客户端重推 ⇒ 写放大。
  if (statements.length > 0) {
    const retired: string[] = [];
    const seen = new Set<string>();
    for (let i = 0; i < accepted.length; i += 1) {
      const oldKey = previousR2Keys.get(accepted[i]!.bucketKey);
      if (oldKey === undefined || oldKey === staged[i]!.r2Key) continue;
      if (seen.has(oldKey)) continue;
      seen.add(oldKey);
      retired.push(oldKey);
    }
    await deleteAllQuietly(env.BUCKET, retired);
  }

  // 3) server_seq = 本次**真正写入**桶行的最大 id（跳过不推进）
  const writtenKeys = [...new Set(accepted.map((bucket) => bucket.bucketKey))];
  let serverSeq = 0;
  if (writtenKeys.length > 0) {
    const writtenPlaceholders = writtenKeys.map(() => '?').join(', ');
    const seqRow = await env.DB
      .prepare(
        `SELECT COALESCE(MAX(id), 0) AS seq FROM sync_buckets
          WHERE user_id = ? AND device_id = ? AND bucket_key IN (${writtenPlaceholders})`,
      )
      .bind(auth.userId, deviceId, ...writtenKeys)
      .first<{ seq: number }>();
    serverSeq = seqRow?.seq ?? 0;
  } else {
    // 本批没有任何桶被写入（全部跳过）：游标不能倒退 ⇒ 取该账号当前已有的最大桶行 id。
    // 这里**不带** device_id：pull 是跨设备视图（D5），游标空间是账号级。
    const seqRow = await env.DB
      .prepare('SELECT COALESCE(MAX(id), 0) AS seq FROM sync_buckets WHERE user_id = ?')
      .bind(auth.userId)
      .first<{ seq: number }>();
    serverSeq = seqRow?.seq ?? 0;
  }

  return {
    accepted: accepted.map((bucket) => bucket.bucketKey),
    skipped,
    server_seq: serverSeq,
  };
}

// ---------------------------------------------------------------- 拉取

/**
 * GET /v1/sync/buckets/pull（ADR 0077 D7 契约）。
 *
 * 顶层三字段 next_cursor / has_more / server_seq **同名同义保留**（客户端
 * ParsePullResponse 依赖），条目数组字段名从 items 换成 buckets。
 *
 * **不带 device 过滤**：本端点返回该账号**所有设备**的桶视图（D5），
 * 由客户端按 blob 内的条目级 HLC 合并 ⇒ 无 lost update。
 */
export async function handleBucketPull(
  env: Env,
  auth: AuthContext,
  url: URL,
): Promise<unknown> {
  const since = parseSince(url.searchParams.get('since'));
  const limit = parseLimit(url.searchParams.get('limit'));
  const cursor = parseCursor(url.searchParams.get('cursor'));
  // cursor 是上一页续读位置，since 是全量起点，取较大者防回退
  const after = Math.max(since, cursor ?? 0);

  const listed = await env.DB
    .prepare(
      `SELECT id, bucket_key, device_id, hlc_ms, hlc_counter, hlc_device,
              r2_key, cipher_len, item_count
         FROM sync_buckets
        WHERE user_id = ? AND id > ?
        ORDER BY id ASC
        LIMIT ?`,
    )
    .bind(auth.userId, after, limit + 1)
    .all<SyncBucketRow>();

  const rows = listed.results ?? [];
  const hasMore = rows.length > limit;
  const page = hasMore ? rows.slice(0, limit) : rows;

  const buckets: Array<Record<string, unknown>> = [];
  const healed: number[] = [];
  for (const row of page) {
    const object = await env.BUCKET.get(row.r2_key);
    if (object === null) {
      // 自愈：R2 桶对象缺失。删掉桶行并告警，不抛 500 —— 抛错会让客户端卡在同一个
      // 游标上无限重试。⚠ 桶化后这里是「丢一整桶条目」而非「丢一条」（ADR 0077 D10
      // 已知边界，本批接受，登记为 BR-①g 候选）。
      healed.push(row.id);
      continue;
    }
    const blob = new Uint8Array(await object.arrayBuffer());
    if (blob.length !== row.cipher_len) {
      console.warn(
        `[fluxing-sync] 桶 blob 长度不一致 r2_key=${row.r2_key} 索引=${row.cipher_len} 实际=${blob.length}`,
      );
    }
    buckets.push({
      bucket_key: row.bucket_key,
      device_id: row.device_id,
      hlc: { ms: row.hlc_ms, counter: row.hlc_counter, device: row.hlc_device },
      // 原样回吐，服务端不解密、不重排、不改写（E2E 不变量 + D3「零新增格式」）
      cipher: toBase64(blob),
      item_count: row.item_count,
      server_seq: row.id,
    });
  }

  if (healed.length > 0) {
    console.warn(
      `[fluxing-sync] 清理缺失桶对象的索引行 ${healed.length} 条：${healed.join(',')}`,
    );
    await env.DB.batch(
      healed.map((id) => env.DB.prepare('DELETE FROM sync_buckets WHERE id = ?').bind(id)),
    );
  }

  const lastRow = page.length > 0 ? page[page.length - 1]! : null;
  return {
    buckets,
    next_cursor: lastRow === null ? null : encodeCursor(lastRow.id),
    has_more: hasMore,
    // 空页时沿用请求游标位置（after）：本次没有任何新桶，游标不能倒退。
    server_seq: lastRow === null ? after : lastRow.id,
  };
}
