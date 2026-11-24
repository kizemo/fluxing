# tasks · 规格 102 · 桶级同步存储架构（服务端先行批次 BR-①f）

> **本文件只覆盖服务端（批次 BR-①f）。客户端改造属下一批 BR-①g，见 §8「本批不做什么」。**
> 体例沿用 `.specify/specs/101-cloud-sync-remote-hlc-reuse/tasks.md`：硬前置 → 任务索引表（每条带优先级
> `[P1]/[P2]/[P3]` 与用户故事 `[US#]`）→ 逐任务「目标 / 改动点 / 硬闸 / 验收判据 / 禁止 / 状态」→ 收尾。
> **宪法 R5：单任务 ≤ 4 小时、1–3 文件。** 每条任务的「状态」行开工时如实改写。

> **⚠ 2026-11-24 12:29 实盘校正（本文件初稿写作时未知，执行前必读）**
> 工作树**已存在**本批服务端实现（属**另一写入者**，本会话未触碰其代码）：新增
> `cloud/fluxing-sync/src/sync-buckets.ts` / `migrations/0004_sync_buckets.sql` / `test/bucket-sync.test.ts`，
> 改动 `src/account.ts` / `src/env.ts` / `src/index.ts` / `src/wire.ts`（`git diff --stat` = 228 insertions / 13 deletions）。
> ⇒ **T4–T7 的文件落点、函数命名（实盘为 `handleBucketPush` / `handleBucketPull` / `countTodayWrittenBucketRows`）、
> R2 键形态（实盘含设备哈希段）、桶写预算默认值（实盘单测钉 4,000 桶行/日）、索引集合（实盘多一条 `(user_id, updated_at)`）、
> 测试布局（实盘为单文件 12 用例）一律以实盘为准**，差异逐条与推荐处置见 **[plan.md §10.1](plan.md)**。
> 这些任务的对象由「从零新建」改为「**核对并补齐既有实现**」，**禁止**并行新建第二套。
> 实盘实现**尚未经门禁验证**（本会话未跑 `typecheck` / `check`）⇒ T13 仍是硬闸。

---

## 1. 硬前置（开工前逐条确认，任一不成立不得动手）

| # | 前置 | 已确认依据 | 状态 |
|---|---|---|---|
| 1 | ADR 0077 已定稿且决策 D1–D11 与本 spec 一致 | `docs/adr/0077-bucket-level-sync-storage-architecture.md`（Status=Proposed，:6-8） | ✅ |
| 2 | 取证报告已就位 | `reports/fluxing-br-1f/recon-blob-arch.md`（§4 服务端改造清单 S1–S9、§6.3 两大风险） | ✅ |
| 3 | 服务端基线门禁为绿：`typecheck` 0 / `check` 0（**12 文件 / 151 用例**） | `cloud/fluxing-sync/package.json` 的 `typecheck` / `check` 脚本 | ⚠ **本轮未执行**（见 §7）⇒ 开工第一动作 |
| 4 | 基线数字已留档（用于「既有用例逐条保留」比对） | 同上（spec 101 tasks.md 同体例） | ⚠ 开工第一动作 |
| 5 | 老端点与 `sync_items` 表在当前分支未被他人改动 | `src/index.ts:101-111` / `src/sync.ts` 全文 / `migrations/0001_init.sql:57-73` | ✅ |
| 6 | `migrations/` 现有序号为 `0001`/`0002`/`0003` ⇒ 新表必须用 **`0004`** | `Get-ChildItem cloud/fluxing-sync/migrations` | ✅ |
| 7 | `src/wire.ts` 的 `MAX_PUSH_ITEMS` 仍被 `src/sync.ts:253` 消费 ⇒ 常量不得删除 | grep 结果 | ✅ |
| 8 | 老端点 pull 响应**不含** `healed` 键（自愈只删索引行 + 写日志） | `src/sync.ts:460-477` | ✅（本批必须保持同构） |
| 9 | 本机可直接执行 `wrangler`（部署与 D1 只读回读用） | `wrangler` 在 PATH（既有批次已用过） | ⚠ 部署轮确认 |
| 10 | 生产当前 Version id 与回滚目标已记录 | `wrangler deployments list`（开工时取） | ⚠ 部署轮确认 |

---

## 2. 任务索引表

| # | 优先级 | 用户故事 | 任务 | 主要文件 | 估时 |
|---|---|---|---|---|---|
| T1 | P1 | AC-101 | 落 `migrations/0004_sync_buckets.sql` 并本地库应用验证 | `migrations/0004_sync_buckets.sql` | 1 h |
| T2 | P1 | US-A / US-B | `src/wire.ts` 新增桶级常量与 `bucketR2KeyFor()` | `src/wire.ts` | 1.5 h |
| T3 | P1 | US-A | `src/sync.ts` 新增桶级类型 + 请求体校验 + HLC 比较复用 | `src/sync.ts` | 2 h |
| T4 | P1 | US-A / US-D | `handleBucketsPush`：读既有桶行 → HLC 守卫 → 写序「先 R2 后 D1」 | `src/sync.ts` | 3.5 h |
| T5 | P1 | US-B | `handleBucketsPull`：游标分页 + R2 缺失自愈（响应键集与老端点同构） | `src/sync.ts` | 3 h |
| T6 | P1 | US-C | 桶级写预算护栏（新常量 + `evaluateBucketsWriteBudget` + 估算改桶数） | `src/sync.ts` `src/env.ts` | 3 h |
| T7 | P1 | US-A / US-B | `src/index.ts` 注册两个桶级端点路由 | `src/index.ts` | 1 h |
| T8 | P1 | US-A / US-D | 桶级推送单测：新建 / 未变跳过 / **同桶不同设备各自成行** / blob 不透明 / 字节与条数上限 / **写序断言** | `test/buckets-push.test.ts` | 4 h |
| T9 | P1 | US-B | 桶级拉取单测：分页语义 / 不返回条目内容 / **R2 缺失 → 200 + 索引行已删 + 不抛 500** | `test/buckets-pull.test.ts` | 3.5 h |
| T10 | P1 | US-C | 桶级预算单测（`test/write-budget.test.ts` 逐条保留 + 新增） | `test/write-budget.test.ts` | 3 h |
| T11 | P1 | US-C | 桶级配额错误映射单测（429 + 503 双路、零写入） | `test/buckets-quota-error-mapping.test.ts` | 3 h |
| T12 | P1 | US-D | `handleDeleteAccountData` 扩展覆盖 `b/` 前缀 + `sync_buckets` 清理 + 自证 | `src/account.ts` | 3 h |
| T13 | P1 | US-D | 账号数据清除单测（两个前缀、两张表、**自证可失败**） | `test/account-data.test.ts` | 3 h |
| T14 | P1 | US-E | 老端点逐字不动回归断言单测 | `test/legacy-endpoints-unchanged.test.ts` | 2.5 h |
| T15 | P2 | US-F | `wrangler.toml` 新增 `WRITE_BUDGET_BUCKETS_PER_DAY` + 口径注释订正 | `wrangler.toml` | 1 h |
| T16 | P2 | US-F | ADR 0077 Status 转 Accepted + 落盘 D2/D9 口径澄清 | `docs/adr/0077-*.md` | 1.5 h |
| T17 | P2 | US-E | `test/check-endpoint-policy.mjs` 增检新代码无默认 `*.workers.dev` 字面量 | `test/check-endpoint-policy.mjs` | 1 h |
| T18 | P3 | US-G | 本地全链路验证（miniflare + `applyD1Migrations` 自动发现 0004） | `test/apply-migrations.ts`（预期**无需改动**） | 1.5 h |
| T19 | P3 | US-H | 服务端 AC-101…AC-106 逐条打点（只读 D1 + `wrangler tail`） | `reports/fluxing-br-1f/verify-server.md` | 3 h |
| T20 | P3 | US-H | 回填 spec 102 §9 待核实项 + 登记 **BR-①g 前置清单**（侧车 v2 / `seq=0` / 桶对象无 `nonce`） | 本目录三件套 + `docs/adr/0076-*.md` | 2 h |

**合计 20 条任务**；P1 = 14 条，P2 = 3 条，P3 = 3 条。

---

## 3. P1 —— 桶级存储与两个端点（T1…T14）

### T1 · `migrations/0004_sync_buckets.sql` `[P1] [AC-101]`
- **目标**：新增桶级索引表，DDL 与 **ADR 0077 D1 的列集合逐字一致**。
- **改动点**：新建 `cloud/fluxing-sync/migrations/0004_sync_buckets.sql`（ADR 0077 D1 / 取证报告 S1）。
- **硬闸**：① 列名逐字：`id` / `user_id` / **`device_id`** / `bucket_key` / `hlc_ms` / `hlc_counter` /
  **`hlc_device`** / `r2_key` / **`cipher_len`** / `item_count` / `updated_at`（**ADR D1 确实含 `hlc_device` 与 `cipher_len`**）；
  ② `id INTEGER PRIMARY KEY AUTOINCREMENT`；③ `user_id` 外键 `ON DELETE CASCADE`；
  ④ `UNIQUE (user_id, device_id, bucket_key)`；⑤ `INDEX (user_id, id)`（本项目命名 `idx_buckets_user_id`）；
  ⑥ **不得有 `nonce_b64` 列**（桶级无 nonce，ADR 0077 D3）；⑦ 不得删改 `sync_items` 的任何列或索引；
  ⑧ 序号必须是 `0004`；⑨ 编码与换行沿用同目录既有 migration。
- **验收判据**：本地 `wrangler d1 migrations apply DB --local` 退出码 0；
  `SELECT sql FROM sqlite_master WHERE name='sync_buckets'` 含 `UNIQUE (user_id, device_id, bucket_key)`
  与 `idx_buckets_user_id`，且**不含** `nonce_b64`；老表 `sync_items` 的 DDL 文本与升级前**逐字节相同**。
- **禁止**：把序号写成 `0003`（已被 `0003_sync_items_updated_index.sql` 占用）；给 `sync_items` 加 `seq` 列。
- **状态**：待开工。

### T2 · `src/wire.ts` 桶级常量与桶键 `[P1] [US-A] [US-B]`
- **目标**：新增 ADR 0077 D4 的键派生规则与新端点专属上限常量。
- **改动点**：`bucketR2KeyFor(userId, bucketKey)` ⇒ `b/${userId}/${hex(sha256(bucket_key))}`；
  `MAX_PUSH_BUCKETS` / `MAX_BUCKET_CIPHER_BYTES` / `MAX_PUSH_BUCKETS_TOTAL_BYTES`（取值见 plan.md §5.1（常量取值）与 §8 Q-A（预算阈值待拍板））。
- **硬闸**：① **不得改动既有常量的值与语义**（`NONCE_BYTES` / `TAG_BYTES` / `KEY_BYTES` / `HLC_COUNTER_MAX` /
  `MAX_PAYLOAD_BYTES` / `MAX_CIPHER_BYTES` / `ITEM_TYPES` / `MAX_ITEM_KEY_BYTES` / `MAX_DEVICE_BYTES` /
  `MAX_PUSH_ITEMS` / `Hlc` / `compareUtf8Bytes` / `compareHlc` / `assertHeaderString` / `parseHlc` /
  `validatePushItem` / `r2KeyFor`）；② 头字符串与字节上限校验必须复用既有函数，不得另写一套；
  ③ `bucket_key` 校验复用 `MAX_ITEM_KEY_BYTES`（同为十六进制 MAC 文本）。
- **验收判据**：`typecheck` 0；单测可断言 `bucketR2KeyFor` 与 `r2KeyFor` 前缀不同（`b/` vs `u/`）且同输入产出稳定同值；
  `git diff -- src/wire.ts` 中既有导出函数的函数体**零改动**。
- **禁止**：为了让桶级代码复用而修改 `validatePushItem` 的行为；改动 `item_key` 派生规则。
- **状态**：待开工。

### T3 · `src/sync.ts` 桶级类型与请求校验 `[P1] [US-A]`
- **目标**：定义 `BucketsPushRequestBody` / `BucketInput` / `BucketRow`，把「条目列表」换成「桶列表」。
- **改动点**：`src/sync.ts` 新增类型与 `validateBucketInput()`（**仅做合法性检查、不解密、不解析 blob 内容**）；
  复用既有 `parseSince` / `parseCursor` / `encodeCursor`，**不得另写一套游标编解码**。
- **硬闸**：① `cipher` 必须是 base64 文本，服务端**只取字节数**（ADR 0077 D3/D5）；
  ② 上报的 `cipher_len` 必须等于解码后字节数（不符 ⇒ 400 中文）；③ 校验顺序：**先校 schema（400）、再判预算（429）**
  （对齐 ADR 0075 的「先校验再发信再落库」范式）；④ `hlc` 必须走既有 `parseHlc` 语义（`counter` 0…65535、
  `device` 长度 ≤ `MAX_DEVICE_BYTES`，比较必须用 `compareHlc`）；⑤ `item_count` 必须 ≥ 1 且为整数。
- **验收判据**：`typecheck` 0；非法输入（缺字段 / `cipher` 非字符串 / base64 非法 / `item_count=0` /
  `cipher_len` 不符 / `hlc_counter>65535`）逐条返回 400 中文说明，且 D1 **零落库**。
- **禁止**：在服务端解析 blob 内容；引入桶级二次加密（ADR 0077 D3 已否决）。
- **状态**：待开工。

### T4 · `handleBucketsPush` `[P1] [US-A] [US-D]`
- **目标**：实现 ADR 0077 D9 的 push 语义 —— 按桶 upsert、按设备各自成行、blob 不透明。
- **改动点**：`src/sync.ts` 新增 `handleBucketsPush(request, env, ctx, userId)`，流程 =
  鉴权（沿用）→ schema 校验 → 读该用户既有桶行（按 `(user_id, device_id, bucket_key)`）→ HLC 守卫 →
  预算护栏（T6）→ 逐桶 **R2 `put` 先**（key = `bucketR2KeyFor(...)`，值 = blob 原文字节，
  `customMetadata` 记 `item_count` / `cipher_len` / `hlc_ms` / `hlc_counter`）→
  **D1 `DELETE+INSERT` 后**（沿用 ADR 0076 D2-4 与 `src/sync.ts:348-380` 的写法）→
  返回 `{accepted, skipped, server_seq}`（`accepted` / `skipped` 为**桶键**列表）。
- **硬闸**：① **守卫比较键必须是三元组 `(user_id, device_id, bucket_key)`**（同桶不同设备必须各自成行，ADR D2/D5/D9）；
  ② 守卫语义 = `compareHlc(stored, incoming) >= 0 ⇒ skipped`（同值也跳过，对齐 `src/sync.ts:290`）；
  ③ 预算判定必须在**任何写之前**；④ 单请求总预算超限 ⇒ 429 **整请求拒绝、不留部分写入**；
  ⑤ 未验证邮箱 ⇒ 403 `email_not_verified`（沿用 `src/sync.ts:245-248` 分支语义）；
  ⑥ R2 失败 ⇒ 整请求 500 且 D1 零写入，**已写 R2 对象可接受为孤儿**（不变量 I9 / ADR D11 可逆性）；
  ⑦ 响应**不得新增条目级字段**（取证报告 §4.2 草案的 `accepted_entries` / `skipped_entries` 已被否决）。
- **验收判据**：AC-103 的五个场景在 T8 单测逐条通过；活体 `curl` 返回体恰为三个键；
  `server_seq` 在本轮全 `skipped` 时仍返回该用户当前最大桶行 `id`（对齐 `src/sync.ts:382-401`）。
- **禁止**：改 `handlePush` / `handlePushWithLimit` 的任何一行；在服务端计算桶归属或校验 `bucket_key` 的来源。
- **状态**：待开工。

### T5 · `handleBucketsPull` `[P1] [US-B]`
- **目标**：实现 ADR 0077 D9 的 pull 语义 —— 桶游标分页 + blob 原文返回 + R2 缺失自愈。
- **改动点**：`src/sync.ts` 新增 `handleBucketsPull(request, env, ctx, userId)`，流程 =
  `after = Math.max(since, cursor ?? 0)`（对齐 `src/sync.ts:416-417`）→
  `SELECT ... WHERE user_id = ? AND id > ? ORDER BY id LIMIT ?`（参数 `limit + 1`）→
  `hasMore` / `page` 判定逐字对齐 `src/sync.ts:431-432` → 逐桶 R2 `get` 取 blob 原文 →
  R2 缺失 ⇒ 记入 `healed` 数组并**跳过该桶**、事后 `DELETE FROM sync_buckets WHERE id = ?`（`env.DB.batch`）→
  返回 `{buckets, next_cursor, has_more, server_seq}`。
- **硬闸**：① `limit` 语义 = **桶数**（默认 50 / 上限 100，复用 `DEFAULT_PULL_LIMIT` / `MAX_PULL_LIMIT`，不新增常量）；
  ② `next_cursor` / `has_more` / `server_seq` **同名同义**，空页时 `server_seq = after`、**不得倒退**
  （逐字对齐 `src/sync.ts:467-477`）；③ 桶条目内容服务端**不解密、不返回任何条目级字段**；
  ④ R2 缺失**不得抛 500**；⑤ **响应不得新增 `healed` 键** —— 老端点的自愈只写日志 + 删索引行、不进响应
  （`src/sync.ts:460-477`），桶级必须保持同构，否则会触发客户端 `ParsePullResponse` 的多余键判定；
  ⑥ 桶对象字段严格按 ADR D7：`bucket_key` / `device_id` / `hlc{ms,counter,device}` / `cipher` / `item_count` / `server_seq`，
  **不含 `nonce`**。
- **验收判据**：AC-104 三个场景在 T9 单测通过；连续带 `cursor` 的调用可无重叠遍历全部桶行；
  空库存返回 `has_more=false`；`LIMIT` 参数为 `limit+1`；自愈后 `SELECT COUNT(*) FROM sync_buckets WHERE id=?` = 0。
- **禁止**：把 `since` 改成时间戳语义；为「告诉客户端丢了几桶」而在响应里加新键（属协议变更，需另立 ADR）。
- **状态**：待开工。

### T6 · 桶级写预算护栏 `[P1] [US-C]`
- **目标**：ADR 0077 D8 + 取证报告 S8 —— 新表、新口径、新常量；老口径不动。
- **改动点**：① `src/env.ts` 新增 `WRITE_BUDGET_BUCKETS_PER_DAY?` / `DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY` /
  `writeBudgetBucketsPerDay()`（非法值回落默认、不 fail closed，与 `writeBudgetRowsPerDay` 同款，`src/env.ts:55-69`）；
  ② `src/sync.ts` 新增 `countTodayWrittenBuckets()`（`FROM sync_buckets`）与 `evaluateBucketsWriteBudget()`；
  ③ 新增桶数估算（新桶 1 行 / 已存在桶 2 行）；④ **改写护栏小节注释（`src/sync.ts:85-105`）为双口径**。
- **硬闸**：① 老函数 `evaluateWriteBudget` / `planWriteRows` / `countTodayWrittenRows` 与常量
  `DEFAULT_WRITE_BUDGET_ROWS_PER_DAY = 80_000` **值与语义不动**；② 估算必须**纯读**（不得为估算写 D1）；
  ③ 超限返回 `fail.tooFrequent(WRITE_BUDGET_EXCEEDED_CODE, WRITE_BUDGET_EXCEEDED_MESSAGE)` ——
  **HTTP 429 / `code` / 中文文案逐字不变，不得新增第二个错误码**；
  ④ 桶级估算必须并入 `devices` / `refresh_tokens` 的既有附加写口径（沿用老估算函数的附加项）。
- **验收判据**：AC-105 判据 (a)/(b) 在 T10 / T11 单测通过；429 响应体
  `code=write_budget_exceeded`、`message=本次同步写入量超出每日额度，请稍后重试。`；
  老端点预算用例（`test/write-budget.test.ts` 既有条目）**逐条保留**。
- **禁止**：把「每账号口径」注释写成「平台额度护盾」（护栏不是护盾，取证报告 §6.3 R1）；为估算引入新的 D1 写。
- **状态**：待开工。

### T7 · `src/index.ts` 路由注册 `[P1] [US-A] [US-B]`
- **目标**：把两个新路径挂进 `switch`，老 `case` 不动。
- **改动点**：`case '/v1/sync/buckets/push'` / `case '/v1/sync/buckets/pull'`，插在 `src/index.ts:111` 之后、
  `/v1/account/data` 之前；方法校验沿用 `assertMethod`（`:37`），鉴权沿用 `requireAuth`。
- **硬闸**：① **必须走 `normalizePath` 后的精确 `case`**（不得新增正则，防路由绕过）；
  ② `GET /v1/sync/buckets/push` ⇒ 405 中文「该接口只支持 POST 请求。」；
  ③ 未命中路径仍落 `default` ⇒ 404 `not_found`（`:136`）；④ 老 `case` 的行序与实现体零改动。
- **验收判据**：AC-102 —— 老端点与 `sync_items` DDL 的 `git diff` 为空；
  新路径 POST 200 / GET 405 / 未鉴权 401 / 错路径 404 四组实测通过。
- **禁止**：改 `/v1/sync/push` 或 `/v1/sync/pull` 的实现体；把新端点做成老端点的别名。
- **状态**：待开工。

### T8 · 桶级推送单测 `[P1] [US-A] [US-D]`
- **目标**：把「按设备各自成行」「blob 不透明」「写序」三条钉进测试。
- **改动点**：新建 `cloud/fluxing-sync/test/buckets-push.test.ts`（miniflare + `test/apply-migrations.ts`；
  假 R2 用 `src/r2-write.ts:40-42` 的 `R2WriteBucket` 结构类型）。
- **硬闸**：必须覆盖 **AC-103 的全部五个场景**，其中场景 3（同桶不同设备各自成行）与
  场景 6（写序断言：R2 成功 / D1 `batch` 抛错 ⇒ R2 有对象、D1 无行）为**不可省略项**；
  另需覆盖三组边界（字节恰好到限 / 超一字节 / 桶数超 `MAX_PUSH_BUCKETS`）⇒ 413 或 400 中文说明。
- **验收判据**：新文件 ≥ 8 条用例全通过；`check` 0 且用例数较基线**只增不减**。
- **禁止**：为了通过而 mock 掉 D1（写序断言必须打真 miniflare D1）；断言里出现默认 `*.workers.dev` 字面量。
- **状态**：待开工。

### T9 · 桶级拉取单测 `[P1] [US-B]`
- **目标**：钉住分页语义与 R2 缺失自愈。
- **改动点**：新建 `cloud/fluxing-sync/test/buckets-pull.test.ts`。
- **硬闸**：必须含「R2 对象已丢 ⇒ 响应 **200**、该桶被剔除、`sync_buckets` 行已删、**不抛 500**、响应无 `healed` 键」；
  必须含「`limit` 按桶数生效、`limit+1` 探测、末桶不丢、末轮 `has_more=false`」；
  必须含「响应体不含任何条目级字段、桶对象不含 `nonce`」；
  `since` / `cursor` 取大逻辑必须与条目级一致。
- **验收判据**：AC-104 三场景通过；空页时 `server_seq === after`（不倒退、不为 0）。
- **禁止**：断言「响应含 `healed` 计数」（老端点没有该键，加它 = 未申报的协议变更）。
- **状态**：待开工。

### T10 · 桶级预算单测 `[P1] [US-C]`
- **目标**：新表口径生效 + 老口径未变，双证。
- **改动点**：`cloud/fluxing-sync/test/write-budget.test.ts` 在既有用例之后新增桶级用例。
- **硬闸**：既有断言**逐字保留、不得删改**；新增用例必须断言 429 + 中文文案 + `code` 逐字不变；
  必须断言超限响应后 `sync_buckets` **零落库**且 R2 **零 `put`**。
- **验收判据**：`check` 0；既有用例名集合是新集合的子集。
- **禁止**：新增第二个 429 错误码；把阈值调大以「让用例过」。
- **状态**：待开工。

### T11 · 桶级配额错误映射单测 `[P1] [US-C]`
- **目标**：覆盖 429 护栏与真 D1 配额错误 503 两条独立闸门。
- **改动点**：新建 `cloud/fluxing-sync/test/buckets-quota-error-mapping.test.ts`
  （以 `test/push-quota-error-mapping.test.ts` 为骨架，走真 `SELF.fetch` 路径）。
- **硬闸**：只打断 `env.DB.batch`，**不得打断 R2 `put`**（必须复现「R2 已写、D1 零落库」的真实形态）；
  配额错误三种形态（Worker 绑定 `D1_ERROR:` / REST 带 `[code: 7500]` / 非 `Error` 实例）⇒
  **503 `storage_quota_exceeded`** + 中文「服务端存储写入额度已用尽，请稍后重试。」；
  无关错误仍 500 `internal_error`（不误伤）；故障**非粘性**。
- **验收判据**：`check` 0；两类闸门（429 / 503）各至少一条断言；D1 零落库且拉取为空。
- **禁止**：把 503 文案改成英文；新增第二处配额判据实现（必须走 `src/errors.ts` 的共享判据）。
- **状态**：待开工。

### T12 · `handleDeleteAccountData` 扩展 `[P1] [US-D]`
- **目标**：ADR 0077 D6 —— 同一次调用清 `u/` 与 `b/` 两个前缀 + 两张索引表。
- **改动点**：`src/account.ts` —— 前缀循环改为 `['u/'+userId+'/', 'b/'+userId+'/']`，每前缀独立轮次预算；
  新增 `DELETE FROM sync_buckets WHERE user_id = ?`；自证 `list` 覆盖两个前缀；
  `deleted_items` 语义改为「两张索引表删除行数之和」（键名不变）。
- **硬闸**：① 任一 `delete` / `deleteBatch` 失败 ⇒ 仍 500 `clear_incomplete`、**不得继续下一步**；
  ② **自证必须是真判据**：`sync_buckets` 行被删、`b/` 对象被删各一条，**不得只回 `ok:true` 恒真**；
  ③ `LIST_LIMIT=1000` / `DELETE_BATCH=500` / `DELETE_ATTEMPTS=3` / `MAX_ROUNDS=1000` 常量值不动；
  ④ 轮次预算耗尽但未报错 ⇒ `ok:false, deleted_objects:0` 的既有语义保留。
- **验收判据**：AC-106 通过；清除后 `SELECT COUNT(*) FROM sync_items` 与 `FROM sync_buckets` 均为 0、
  两个前缀 `list` 均空；老用户数据清除既有用例逐条保留。
- **禁止**：只清 `u/` 前缀；把桶对象删除交给「未来某个 GC」；漏删索引表（否则桶级 pull 返回空 blob）。
- **状态**：待开工。

### T14 · 老端点逐字不动回归断言 `[P1] [US-E]`
- **目标**：把「本批不改老端点」变成机器可校验的判据（AC-102）。
- **改动点**：新建 `cloud/fluxing-sync/test/legacy-endpoints-unchanged.test.ts`。
- **硬闸**：断言 `handlePush` / `handlePushWithLimit` / `handlePull` 可从 `src/sync.ts` 导入且签名不变、
  经 `/v1/sync/push` 与 `/v1/sync/pull` 可达且行为不变；`MAX_PUSH_ITEMS` 仍被 `src/sync.ts:253` 消费；
  `sync_items` DDL 文本未变；断言两套端点的响应键集差异（条目级多 `nonce`、桶级无）。
- **验收判据**：AC-102 的「`git diff` 为空」判据 + 本文件通过。
- **禁止**：写成永真比较（例如把自己和自己比）；为本批便利而重构老端点内部实现。
- **状态**：待开工。

### T13 · 账号数据清除单测 `[P1] [US-D]`
- **目标**：给 T12 的自证提供**可失败**的判据。
- **改动点**：新建 `cloud/fluxing-sync/test/account-data.test.ts`。
- **硬闸**：必须含一条「若实现漏了 `b/` 前缀则本条**必须失败**」的反例断言（直接检查 `b/` 对象与
  `sync_buckets` 行在清除后确实为 0，而非检查响应键值）；必须含「`u/` 侧既有断言逐条保留」。
- **验收判据**：`check` 0；人为把 T12 的 `b/` 分支注掉后，本文件至少 1 条用例失败（**当场验一次、验完改回**）。
- **禁止**：只断言 `resp.status === 200`；用 `ok:true` 当作「清干净了」的证据。
- **状态**：待开工。

---

## 4. P2 —— 配置与决策回写（T15…T17）

### T15 · `wrangler.toml` `[P2] [US-F]`
- **目标**：新 var 上生产 + 订正口径注释。
- **改动点**：`[vars]` 新增 `WRITE_BUDGET_BUCKETS_PER_DAY = "<取值，见 plan.md §5.1（常量取值）与 §8 Q-A（预算阈值待拍板）>"`；
  把 `wrangler.toml:56-59` 的注释改写为「老常量只管条目级端点 `sync_items`，新常量管 `sync_buckets`」双口径表述。
- **硬闸**：① 既有 var（`ALLOW_REGISTRATION` / `API_BASE_URL` / `WEB_BASE_URL` / `MAIL_*` / `LINK_TTL_HOURS` /
  `RATE_LIMIT_*` / `MAX_DEVICES_PER_ACCOUNT` / `MAX_REFRESH_TOKENS_PER_ACCOUNT` / `REFRESH_RETENTION_DAYS` /
  `WRITE_BUDGET_ROWS_PER_DAY = "80000"`）**值与相对顺序不动**；② 不得写 Secret；
  ③ 不得出现默认 `*.workers.dev` 字面量；④ `routes` / `d1_databases` / `r2_buckets` 段不动。
- **验收判据**：`wrangler deployments status` 显示新 var；老 var 仍为 `"80000"`。
- **禁止**：为「省额度」调小 `WRITE_BUDGET_ROWS_PER_DAY`（属生产参数变更，需单独拍板）。
- **状态**：待开工。

### T16 · ADR 0077 回写 `[P2] [US-F]`
- **目标**：Status `Proposed → Accepted`，并把 D2 与 D9 的「桶数 vs 桶大小」口径澄清落盘。
- **改动点**：`docs/adr/0077-bucket-level-sync-storage-architecture.md` —— Status 更新；新增「批次 BR-①f 落盘说明」：
  D1 的「128 行」与 D9 的「852 行」来自两套不同的分桶参数（D2 固定 64 分片 vs D9 每桶 64 条），
  **服务端索引行数与桶大小无关**，上界式 = `每请求桶数 ≤ MAX_PUSH_BUCKETS(100)`、
  **账号存量桶行上界 = 设备数(32) × 桶数**（ADR D5）；桶数与桶大小取值属客户端参数（ADR D8/D9 ⇒ BR-①g）。
- **硬闸**：不得改写 D1–D11 决策正文的语义；不得删除「备选方案与否决理由」表；
  取证报告 §4.2 草案与 ADR 的偏离处必须写明「以 ADR D3 的 blob 口径为准」。
- **验收判据**：ADR 文件含新小节；`git diff -- docs/adr/0076-*.md` 为**空**（AC-102 ④：`sync_items` 继续承载）。
- **禁止**：把 D2/D9 的矛盾「就地抹平」成单一数字而不说明两者关系（必须写清服务端口径为何与两个示例数字都不同）。
- **状态**：待开工。

### T17 · 端点策略检查扩展 `[P2] [US-E]`
- **目标**：新代码同样受「禁用默认 `*.workers.dev`」门禁保护。
- **改动点**：确认 `cloud/fluxing-sync/test/check-endpoint-policy.mjs` 的扫描范围覆盖
  `src/sync.ts` 新增函数与 `migrations/0004_sync_buckets.sql`；若已按目录全量扫描则只需验证。
- **硬闸**：判据不得放宽；命中字面量必须退出码非 0。
- **验收判据**：脚本对全 `src/` + `migrations/` 运行退出码 0；人为在 `src/sync.ts` 塞一条默认子域字面量 ⇒
  脚本失败（**当场验一次、验完改回**）。
- **禁止**：把检查范围缩到 `src/index.ts` 一个文件。
- **状态**：待开工。

---

## 5. P3 —— 集成与真机验证（T18…T20）

### T18 · 本地全链路 + migration 装载 `[P3] [US-G]`
- **目标**：确认新 migration 被自动应用、单测无需改装载代码。
- **改动点**：跑 `typecheck` + `check`；仅在 `test/apply-migrations.ts` 需要显式列文件时才改。
- **硬闸**：装载走 `applyD1Migrations(env.DB, env.TEST_MIGRATIONS)`（按 `migrations/` 目录发现）⇒ **预期零改动**；
  出现「表不存在」必须先查 migration 命名与序号，再考虑装载代码。
- **验收判据**：本地 miniflare 建表成功；同一桶 push → pull 往返**字节级相等**（AC-111）；
  老端点单测在全新本地库上仍全绿。
- **禁止**：在单测里手工 `CREATE TABLE`（必须走 migration）。
- **状态**：待开工。

### T19 · 服务端 AC 打点 `[P3] [US-H]`
- **目标**：生产部署后用只读判据证明 AC-101…AC-106。
- **改动点**：新建 `reports/fluxing-br-1f/verify-server.md`，逐条记 AC / 判据 / 命令 / 实测数字 / 结论。
- **硬闸**：① D1 只允许 `SELECT` 与 `wrangler d1 info`（**禁写**）；② 写判据用 `wrangler tail` 日志计数，
  不得在生产 D1 建探针行；③ AC-103 场景 3 需**两个设备 token 的桶级推送** ⇒ 无客户端时可用
  `curl` + 预配账号完成，**无法完成则记「未判定」并写明缺什么，不得记 PASS**；
  ④ 只有判据实际达成才可记 PASS（沿用本仓既往红线）。
- **验收判据**：六条 AC 各有明确 PASS / FAIL / UNDETERMINED 与证据行号或命令输出。
- **禁止**：把文档或工具描述当作已验证；把「响应结构合理」当判据。
- **状态**：待开工。

### T20 · 待核实回填 + BR-①g 前置清单 `[P3] [US-H]`
- **目标**：把 spec §9 的 Q1…Q5 结论回填，并把客户端批次的前置条件写全。
- **改动点**：本目录三件套 + `docs/adr/0076-*.md` **仅追加**「已知边界」小节（登记 D2-5a-9 的 `503` 早退轮
  `reconcile` 行全 0 现象属新表预期范围，不在本批修复）；新增「BR-①g 前置清单」段落。
- **硬闸**：BR-①g 前置清单必须含四条：① 侧车 v2 升版与 `seq = 0` 强制重置（ADR 0077 D6，spec AC-109）；
  ② 桶对象**不含 `nonce` 字段**（ADR D7）对客户端 `ParsePullResponse` 必填键判定的影响评估（AC-108）；
  ③ `has_more` / `next_cursor` / 顶层 `server_seq` 在桶级分页的真机验证（AC-104 客户端侧）；
  ④ 桶大小取值 + R2 单对象 `put` 吞吐实测（取证报告 §6.3 R3，spec AC-112）。
- **验收判据**：清单落盘；spec §9 每条有结论与依据。
- **禁止**：在本批实现任何客户端代码；顺手改 `WeaselServer/Cloud/**`。
- **状态**：待开工。

---

## 6. 收尾硬闸（全部任务完成后逐条打勾，缺一不得收批次）

- [ ] `npm --prefix 'F:\soft\00selfmade\rime_claude\cloud\fluxing-sync' run typecheck` ⇒ 退出码 **0**
- [ ] `npm --prefix 'F:\soft\00selfmade\rime_claude\cloud\fluxing-sync' run check` ⇒ 退出码 **0**，
      文件数与用例数**较基线 12 文件 / 151 用例只增不减**，既有 151 条逐条保留
- [ ] `git status -sb` + `git diff --stat`：改动仅落在 plan.md §5.10 文件清单内；
      **未**误入 `weasel.props` / `env.bat` / `*.obj` / `release/` / `reports/**`（未跟踪项按红线不入库）
- [ ] **未**执行 `git add` / `git commit` / `git push` / `wrangler deploy` / `d1 migrations apply --remote`
      （入库与部署需用户授权）
- [ ] `librime` 子模块**未**被 `git add`
- [ ] 新增 `*.ts` / `*.sql` / `*.toml` 中 U+FFFD 替换字符数 = **0**；用户可见字符串全部为简体中文
- [ ] `test/check-endpoint-policy.mjs`（或等价门禁）对新文件运行通过，无默认 `*.workers.dev` 字面量
- [ ] `migrations/0004_sync_buckets.sql` 与 ADR 0077 D1 的**列集合与约束逐字一致**，且不含 `nonce_b64`
- [ ] `git diff -- cloud/fluxing-sync/src/sync.ts` 中 `handlePush` / `handlePushWithLimit` / `handlePull`
      三段函数体**零改动**（AC-102 主判据）
- [ ] `git diff -- docs/adr/0076-*.md` 为**空**（`sync_items` 继续承载）
- [ ] 桶级 pull 响应键集 = `{buckets, next_cursor, has_more, server_seq}`，**不含 `healed`**

## 7. 本轮（spec 创建轮）的执行状态

- **本轮未写任何生产代码、未执行任何门禁、未部署、未 commit。**
  §1 前置 3 / 4 的基线数字（12 文件 / 151 用例）沿用既往批次记录，**开工第一动作必须实测复现**。
- 估时合计约 **46 小时**（T1…T20），P1 约 33 小时 ⇒ 需跨多轮次会话执行；
  每完成一阶段须按 `AGENTS.md` §9 同步更新 handoff 与接力 prompt。

## 8. 本批不做什么（属 BR-①g 客户端批次）

1. 客户端分桶打包 / 拆包、桶级 HLC 分配、`bucket_key` 计算（ADR 0077 D2/D3/D4）。
2. 侧车 v2 升版与 `seq = 0` 强制重置（ADR 0077 D6）。
3. 桶内条目上限 4096 与「超限降级老端点」的客户端实现（ADR 0077 D2）。
4. 桶大小 / 分片数取值调参与 R2 吞吐实测（ADR 0077 D9、取证报告 §6.3 R3）。
5. `healed` 自愈粒度变粗的部分恢复方案（ADR 0077 D10）。
6. 出包装机与真机判据（AC-09 复跑仍由既有定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` 负责，本批不改判据）。
7. `sync_items` 表与老端点的退役（ADR 0077 D11：另立 ADR）。**2026-11-24 用户拍板**：老端点读放大（每次推送 `COUNT` 扫该用户全部行）的止血**随本项退役一并执行** ⇒ BR-①g 必须交付 ADR 0077 D6 的显式退役流程「**双写 → 校验 → 切读 → 退役**」四段 + **回滚预案**；**只做「切读」就下老端点 = 不合格**。①f 内老端点仍**逐字不动**。
8. `devices.last_seen` 读路径写占比上升的治理（取证报告 §6.3 R4）。
9. 写预算护栏改「全局口径 / 按活跃账号分摊」（ADR 0076 已知边界，批次 BR-②）—— **2026-11-24 用户拍板：维持现状 = 每账号口径**；护栏是「单账号放大源刹车」，**不是** Cloudflare 全局额度护盾 ⇒ 本批不改实现，ADR 0077 D8 口径注释保持，BR-② **不立项**（仅作已知边界登记）。
10. 是否升级 Workers Paid —— **2026-11-24 用户拍板：暂不升级** ⇒ D1 免费额度仍是硬约束 ⇒ 主链第 0 步「额度活性探针 + 零写入纪律」必须保留；**BR-①g 开工前须先定「桶化后一次性补推约 54,536 条」的分批策略**，否则会再撞 `code 7500`。
