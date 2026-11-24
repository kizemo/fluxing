# 102 · 桶级同步存储架构 · 实现计划（plan.md）

> **性质**：服务端先行批次 **BR-①f**。本批**只改服务端**（`cloud/fluxing-sync`），客户端改造（BR-①g）**不在本批范围**。
> **唯一权威设计来源**：[ADR 0077 桶级同步存储架构](../../../docs/adr/0077-bucket-level-sync-storage-architecture.md)（Status=Proposed）
> **需求件**：[spec.md](spec.md)（FR-101…FR-117 / AC-101…AC-112）
> **任务件**：[tasks.md](tasks.md)（T1…T20）
> **上游**：spec [100-fluxing-cloud-account-sync](../100-fluxing-cloud-account-sync/spec.md) · spec [101-cloud-sync-remote-hlc-reuse](../101-cloud-sync-remote-hlc-reuse/spec.md) · [ADR 0076](../../../docs/adr/0076-push-write-amplification-and-zero-write-register.md)

---

## 1. 本批做什么、做到哪一步

### 1.1 一句话

把「每条用户词一个云对象 + 一行索引」改成「**每（设备 × 桶）一个云对象 + 一行索引**」，用新表 `sync_buckets` 与新端点 `/v1/sync/buckets/push` / `/v1/sync/buckets/pull` 落地；**老表 `sync_items` 与老端点 `/v1/sync/push` / `/v1/sync/pull` 逐字不动**，两套并存；老客户端照常工作。

### 1.2 为什么服务端先行

ADR 0077 的**可逆性声明**（§可逆性）与**分期**（D11）都要求服务端先上线、保持向后兼容，客户端随后切换。顺序反过来会让现网客户端在同步时直接失败。因此：

- 本批交付物 = **可部署、可单测证明的服务端能力**，不含任何客户端调用方。
- 本批**不要求**客户端存在 ⇒ 新端点上线后短期无人调用属正常，**不得**据此判定失败。
- 客户端不切换 ⇒ **本批不 bump 版本号、不出包、不装机**。版本号只在 BR-①g 客户端实际切换到新路径时 bump（spec NFR-105 / AC-112）。

### 1.3 交付判据

判据全部落在 spec.md 的 **AC-101…AC-112**，每条可机器判定。其中**本批必须实测通过**的是 AC-101…AC-110 与 AC-112；**AC-111（现网调用量与 D1 日写入行数的下降比例）**属上线后的观测项，本批无生产流量可测，登记为**待用户授权部署后复验**。

---

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

依据：[.specify/memory/constitution.md](../../memory/constitution.md) §「核心原则」（第 51–92 行）。五条逐一过闸。

| 原则 | 原文要义 | 本计划如何满足 | 阻断条件 |
|------|----------|----------------|----------|
| **I. Intent Before Implementation（意图先于实现）** | 写码前必须存在已批准的需求工件，意图、范围、验收判据三者明确 | 意图与范围来自 **ADR 0077**（D1–D11 + 备选方案否决），需求件 spec.md 已先行落盘，含范围边界（§3）、7 条不变量（§7）、12 条验收判据（§6）；本 plan.md 在 tasks.md 之前存在，任务编号引用本 plan 的章节与文件名 | spec.md 或 ADR 0077 任一缺失或被推翻 ⇒ 不得开工 |
| **II. Test-Backed Change（测试支撑变更）** | 行为变更必须同时更新既有测试或新增测试，判据须明确成功 / 失败条件及如何证明 | 每个新行为均有对应单测（§7）；门禁 = `npm run typecheck` 返回 0 **且** `npm run check` 返回 0，用例数只增不减（基线 12 文件 / 151 用例）；老端点回归**复用既有 151 条用例逐条保留**，不删、不改断言、不 `.skip` | 任一条既有用例被删或断言被放宽 ⇒ 视同红线，本批停线 |
| **III. Spec-Artifact Discipline（工件与真相源纪律）** | 行为 / API / 架构变更必须先创建或更新规范工件；ADR / 需求 / 计划 / 任务齐备，命名与 ID 约定一致，无悬空引用 | ADR 0077 已存在并编号唯一；本目录三件套按 spec 100 / 101 既有体例命名（spec / plan / tasks）；FR / AC / T 编号连续且互相引用无悬空；与 ADR 的任何偏离**就地登记**（§10）并回填 ADR（T20） | 出现未登记的偏离、或工件间编号互相矛盾 ⇒ 不得进收尾 |
| **IV. Structured Clarification（结构化澄清优先）** | 实现前把未明需求转成穷尽、独立可答的问题，逐项给推荐答案与后果，并写入 spec 的「待核实问题」 | spec.md §9 已把不确定项转成 **Q1…Q5** 编号问题，每条附推荐答案与后果；其中 **Q1**（ADR 0077 内部 D2 与 D9 的分桶口径互斥）标 `[NEEDS CLARIFICATION]` | `[NEEDS CLARIFICATION]` 未清零前**只允许做不依赖该答案的部分**（§2 表前置 9 / T1、T2）；依赖答案的任务不得开工 |
| **V. Incremental Delivery（增量交付）** | 切成小而可验证的任务，单任务不越界，每阶段都有机器可判判据、文档同步、提交意图 | 20 个任务按依赖分层：建表 → 常量 → 护栏 → push → pull → 装配 → 账户清理 → 测试 → 门禁 → 文档；每个任务带验收判据（§3）；**本批本身即一个可独立回滚的 thin slice**（老路径零改动） | 单任务涉及 >4 文件或 >4h ⇒ 拆分后再排 |

**附加红线（来自宪法 R1–R9，本批适用项）**：

- **R1 稳定契约**：IPC 协议不变（本批纯服务端）。但 `wire.ts` 常量属跨端契约，故 **既有常量取值逐字不动**（§5.1）。
- **R2 单一真相源**：「技术文档不得把未实施目标写成既成事实」⇒ `docs/adr/0077` 第 8 行已明写「服务端实现尚未完成」。**本计划的任何产出都不得写成「桶级架构已上线」**；生产未部署前，ADR 0077 状态仍为 `Proposed`。
- **R2 另条**：「文档不得把待核实项写成已确认结论」⇒ 取证报告 `reports/fluxing-br-1f/recon-blob-arch.md` 的草案形态（桶级 AEAD）已被 ADR 0077 **D3 否决**（`docs/adr/0077:97`）。**以 ADR 为准**，§10 登记该偏离。
- **R3 任务优先级 + 用户故事标签**、**R4 计划必须含 Constitution Check**：本节即第 2 节。
- **R5 任务规模**：单任务 ≤4h、≤3 个生产文件（§3 逐任务登记，测试文件另计）。
- **P2/P5 中文**：本批新增的用户可见错误文案沿用既有中文；新 code（`bucket_too_large` / `too_many_buckets`）的 message 亦为简体中文。
- **P4 Conventional Commits + 模块前缀**：本批提交前缀用 `fluxing:`（云端同步属 fluxing 品牌分支的服务端面），subject 形如 `fluxing: sync 桶级存储端点（服务端先行 BR-①f）`。

---

## 3. 架构与数据流

### 3.1 装配链（既有，本批只做加法）

```
fetch(request, env, ctx)                                  src/index.ts
  └ normalizePath(pathname)                                       :43
     switch (normalized)                                                 :51
       case "/v1/sync/push"  → requireAuth → handlePush       :101-105  ← 不动
       case "/v1/sync/pull"   → requireAuth → handlePull      :107-111  ← 不动
       case "/v1/sync/buckets/push" → requireAuth → handleBucketsPush   ← 新增（插在 :111 之后）
       case "/v1/sync/buckets/pull"  → requireAuth → handleBucketsPull   ← 新增
       case "/v1/account/data" → requireAuth → handleDeleteAccountData   :119-123 ← 扩展
```

`/v1/sync/buckets/*` 无路径参数 ⇒ **不新增正则**，直接用 `normalizePath` 后的精确 `case` 匹配。路由顺序上把 `/v1/sync/*` 全部放在 `/v1/account/data` 之前。

### 3.2 push 数据流（新）

```
handleBucketsPush(request, env, ctx, userId)                    src/sync.ts
 1. 邮箱未验证 → 403 email_not_verified               照抄老 handlePushWithLimit :245-248
 2. readJsonObject → body.device_id 校验 → buckets 非数组 / 空 / 条数 > MAX_PUSH_BUCKETS → 400
 3. 逐桶 validateBucketItem（长度上限 + 非法即 400，不静默跳过） → 重复 bucket_key 去重
 4. 对去重后的桶键集合 SELECT 既有行 WHERE user_id=? AND device_id=? AND bucket_key IN (…)
 5. 双层守卫：compareHlc(stored, incoming) >= 0 → skip；否则 → upsert
 6. 写预算估算（纯读）：planBucketsWriteRows(upsert 桶数) → 超限 → 429 write_budget_exceeded（在任何写之前）
 7. putAllWithConcurrency → R2 put b/<user>/<hash(bucket_key)>      （失败 → 502 object_write_failed，D1 零写）
 8. D1 batch：对 upsert 桶 DELETE 旧行 + INSERT 新行（同事务，全有全无）
 9. 返回 { accepted, skipped, server_seq }
```

### 3.3 pull 数据流（新）

```
handleBucketsPull(request, env, ctx, userId)
 1. since / cursor → after = Math.max(since, cursor ?? 0)          照抄老 handlePull :416-417
 2. SELECT id, device_id, bucket_key, hlc_*, cipher_len, item_count
    FROM sync_buckets WHERE user_id=? AND server_seq > after ORDER BY server_seq LIMIT ?
    （limit+1 探针判 has_more；limit 夹在 [1, MAX_PULL_LIMIT]）     照抄 :424-432
 3. has_more 时 page = slice(0, limit)
 4. 并行 env.SYNC_OBJECTS.get(bucketR2KeyFor(userId, bucket_key))   不传 Range
 5. 对象缺失 → 收集该行的 (device_id, bucket_key)，事后 batch DELETE（自愈）
 6. 对象存在 → blob = 响应体文本 → buckets.push({ device_id, bucket_key, hlc, cipher: blob, item_count, server_seq })
    （blob 为空串也如实返回 + cipher_len === 0；不伪造条目、不丢桶）
 7. next_cursor = page 末行 server_seq（无行则回请求游标）
 8. 顶层 server_seq = SELECT COALESCE(MAX(id), 0)                   照抄 :471-477 语义
 9. 返回 { buckets, next_cursor, has_more, server_seq }
```

**自愈判据不新增响应键**（§4.6 论证）：老 `handlePull` 的自愈**只写 `console.warn` + `DELETE FROM sync_items`，响应体里没有 `healed`**（`src/sync.ts:438-465`、`:467-477`）。桶级 pull 与之同构，测试判据改看索引行被删 + 下一轮不再返回该桶。

### 3.4 不变量映射

| ADR 0077 不变量 | 本批服务端实现位 |
|-----------------|------------------|
| I3 服务端只见密文、无法判断词库内容 | blob 是 `string`，服务端只做文本透传，**不解析、不计数、不重组**条目；桶内条目顺序不影响密文（桶键 = OMAC 权威）⇒ 服务端无从对拍 |
| I5 不删库不删表不删对象（本批） | migration 只 `CREATE TABLE` + `CREATE INDEX`，无任何 `DROP` / `ALTER` |
| I6 老端点与老表逐字不动 | `handlePushWithLimit` / `handlePull` / `sync_items` DDL 零改动；既有常量取值零改动 |
| I7 新路径失败不阻塞老路径 | 新代码全在新表 / 新端点内；`/v1/account/data` 是唯一共享改动点，其新步骤**失败即 500 `clear_incomplete`**，语义与老段一致 |
| I8 服务端不因桶内单条目非法而拒整桶 | 服务端**根本不看条目** ⇒ 结构性满足，无需实现解析分支 |
| I9 客户端提交整桶、服务端不猜测桶边界 | 服务端只按 `(user_id, device_id, bucket_key)` 精确 upsert，不做任何分桶计算 |

---

## 4. 数据库设计

### 4.1 migration 文件

`cloud/fluxing-sync/migrations/0004_sync_buckets.sql`（**0003 已被 `0003_sync_items_updated_index.sql` 占用**，新表取 0004）。

### 4.2 DDL

```sql
-- 0004_sync_buckets.sql
-- 桶级同步存储（ADR 0077 D1）。本表只增不改，sync_items 与老端点逐字保留（I6）。
CREATE TABLE IF NOT EXISTS sync_buckets (
  id INTEGER PRIMARY KEY AUTOINCREMENT,   -- server_seq 编号空间，全局单调
  user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  device_id TEXT NOT NULL,                -- 桶最后写入设备（D5 / D7）
  bucket_key TEXT NOT NULL,               -- OMAC 密文标识，服务端不解释
  hlc_ms INTEGER NOT NULL,
  hlc_counter INTEGER NOT NULL,
  hlc_device TEXT NOT NULL,               -- 桶级 HLC 三元组的 device 段（守卫比较用）
  r2_key TEXT NOT NULL,                   -- b/<user>/<hash(bucket_key)>
  cipher_len INTEGER NOT NULL,            -- blob 的 UTF-8 字节数（D1 列名是 cipher_len，不是 blob_len）
  item_count INTEGER NOT NULL,            -- 桶内条目数（D10 自愈判据用）
  updated_at INTEGER NOT NULL
);

-- 唯一约束必须含 device_id（D5：每设备独立视图）
CREATE UNIQUE INDEX IF NOT EXISTS uniq_buckets_user_device_bucket
  ON sync_buckets (user_id, device_id, bucket_key);

-- pull 侧游标扫描用（user_id + id 序，对应老表 idx_sync_user_id 的同构作用）
CREATE INDEX IF NOT EXISTS idx_buckets_user_id ON sync_buckets (user_id, id);
```

索引名沿用本项目既有风格（老表为 `idx_sync_user_item` / `idx_sync_user_id`，非 ADR 示例里的 `idx_sync_user_id` 直译）。

要点：

- 唯一约束 **必须含 `device_id`** ⇒ `(user_id, device_id, bucket_key)`，与 ADR 0077 D1 一致。取证报告草案写的是 `(user_id, bucket_key)`，属偏离，登记于 §10。含 `device_id` 的后果：不同设备各自覆盖自己的桶行 ⇒ 同一逻辑桶在云端只保留**最后写入设备**的视图（D5 明写「不做逐设备物化存储」，设备数上限 32 ⇒ 单用户索引行硬上界 `32 × 桶数`）。
- **无 `nonce_b64` 列**：blob 是「N 个既有信封文本块的拼接」，每条目自带 nonce（D3）⇒ 索引表不存桶级 nonce。取证报告草案的 `nonce` 列作废。
- `cipher_len` / `item_count` 供拉取侧做「空桶 vs 对象缺失」区分与自愈判据（D10）：`item_count = 0` 是**合法的空桶**，与对象缺失（自愈删行）是两回事。
- `id INTEGER PRIMARY KEY AUTOINCREMENT` ⇒ `server_seq` 语义（全局单调、DELETE+INSERT 会换新号）与老表完全一致；客户端须容忍此跳号（AC-109）。

### 4.3 编号空间的后果（ADR 0077 D6 / spec FR-115）

`sync_items.id` 与 `sync_buckets.id` 是**两个独立的 AUTOINCREMENT 序列**。桶级 pull 返回的 `server_seq` 来自后者 ⇒ 客户端老侧车里的游标数值**不可跨路径复用**，BR-①g 必须把侧车升版（v2）并显式携带路径标识，升级首机 `seq = 0` 全量重扫。本批只交付新表 / 新端点，客户端不切换 ⇒ 无现网副作用。

---

## 5. 逐文件模块改造

> 标注：`←` = 插入位（行号取证于 2026-11-24 实盘）。

### 5.1 `src/wire.ts` —— 新增桶级常量与校验（不改既有）

新增：

| 常量 | 建议取值 | 依据 |
|------|----------|------|
| `MAX_PUSH_BUCKETS` | `100` | 与 `MAX_PULL_LIMIT = 100` 对齐，单请求 ≤ 一轮拉取页数；子请求数 ≤ 100 × 并发 6，不超 Durable Object 的 1,000 子请求上限。护栏超限降级值取 `48` |
| `MAX_BUCKET_CIPHER_BYTES` | `2 * 1024 * 1024`（2 MiB） | 单条目 `MAX_CIPHER_BYTES`（1 MiB）× 4096 条会远超 Worker 内存；桶内条目上限 4096（ADR D2）在真实词库下极少达到，2 MiB 给足余量且与老端点 1 MiB 量级同阶 |
| `MAX_PUSH_BUCKETS_TOTAL_BYTES` | `32 * 1024 * 1024`（32 MiB） | 整请求体量闸，防单请求打爆 Worker 内存；超限 → 413 |
| `MAX_BUCKET_KEY_BYTES` | `512` | 与 `MAX_ITEM_KEY_BYTES = 512` 同阶（bucket_key 实为 base64url OMAC，32 字节；512 为宽容上限） |
| `MAX_ITEM_COUNT` | `4096` | ADR 0077 D2「桶内条目上限 4096」 |

既有常量 **`MAX_PUSH_ITEMS = 64` / `MAX_PAYLOAD_BYTES` / `MAX_CIPHER_BYTES` / `MAX_ITEM_KEY_BYTES` / `MAX_DEVICE_BYTES` / `DEFAULT_PULL_LIMIT` / `MAX_PULL_LIMIT` 取值逐字不动**（FR-103 / AC-104）。

新增函数：

- `bucketR2KeyFor(userId: string, bucketKey: string): string` ⇒ `` `b/${userId}/${hashHex}` ``，`hashHex = sha256Hex(bucketKey)`。**与既有 `r2KeyFor()` 的 `u/` 前缀并存，构成两个键前缀**（§6.4 / AC-108）。
- `validateBucketItem(item): BucketItem \| null`：按 §5.4 的异常口径返回，不引入第二套校验风格。

### 5.2 `src/env.ts` —— 新增桶级预算变量

```ts
WRITE_BUDGET_BUCKETS_PER_DAY?: string;              // Env 可选字段
DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY = <见下>;      // 默认值
writeBudgetBucketsPerDay(env): number;              // 非法值回落默认 + console.warn（不 fail closed）
```

区间校验沿用既有 `1 ~ 10_000_000` 的写法与「非法回落默认」策略（`src/env.ts:45-46` / `:55-69`）。

**建议默认值 = `8_000`**（登记为 §8 Q-A 待拍板）。推导：

- 一次全库重建 = 852 桶 × 2 行 = **1,704** 行索引写入 ⇒ 8,000 ≈ **4.7 次全库重建 / 账号 / 日**，日常增量一轮只动几个桶，远超够用。
- 与老护栏「80,000 行」同量级护栏住最坏情形：桶数口径 852 × 32 设备 × 2 = 54,528 行 ⇒ 8,000 反而**更严**，把「32 设备全量重建」这类洪峰压回老护栏量级。
- `wrangler.toml` 的 `[vars]` 同步写入该值并补口径注释。

### 5.3 `src/sync.ts` —— 新增桶级三个函数（老函数零改动）

新增小节注释（沿用 `:85-105` 的写法），并**必须逐条保留**「每账号口径 ≠ 平台全局额度」的声明（`:93-96`）。

1. `countTodayWrittenBuckets(env, userId, startMs)`
   ```sql
   SELECT COUNT(*) AS n FROM sync_buckets WHERE user_id = ? AND updated_at >= ?
   ```
2. `planBucketsWriteRows(bucketCount)` ⇒ `bucketCount * 2`（DELETE 旧行 + INSERT 新行）。
3. `evaluateBucketsWriteBudget(env, userId, plannedBuckets)`：`used` 取**两表之和**（`sync_items` + `sync_buckets`，ADR 0077 D8），`limit` 取 `writeBudgetBucketsPerDay()`；`used + planned > limit ⇒ exceeded`。**纯读，无任何写语句。**
4. `handleBucketsPush(request, env, ctx, userId)`：§3.2 九步。
5. `handleBucketsPull(request, env, ctx, userId)`：§3.3 九步。

复用既有件：`fail` 工厂、`jsonResponse`、`readJsonObject`、`compareHlc`、`assertHeaderString`、`putAllWithConcurrency`、`R2WriteBucket`、`AuthedRequest.env.SYNC_OBJECTS`、UTC 日界的计算（与老 `evaluateWriteBudget` 同一算法，不重写第二套）。

**429 响应体逐字不变**：`fail.tooFrequent(WRITE_BUDGET_EXCEEDED_CODE, WRITE_BUDGET_EXCEEDED_MESSAGE)`（`:108` / `:111`），**不新增第二个 429 码**。

### 5.4 输入校验的异常口径（FR-116 / spec §7 不变量）

| 情形 | 响应 |
|------|------|
| `buckets` 非数组 / 空数组 / 条数 > `MAX_PUSH_BUCKETS` | 400，中文 message |
| 单桶 `bucket_key` 非法（非字符串 / 空 / 超 `MAX_BUCKET_KEY_BYTES`） | 400 |
| 单桶 `cipher` 非法（非字符串 / 超 `MAX_BUCKET_CIPHER_BYTES`） | 400 |
| 单桶 `item_count` 非法（非整数 / 负 / 非安全整数 / > `MAX_ITEM_COUNT`） | 400 |
| `cipher` UTF-8 字节数 ≠ `cipher_len`（若请求带该字段） | 400 |
| 整请求字节数 > `MAX_PUSH_BUCKETS_TOTAL_BYTES` | 413 `fail.tooLarge(...)` |
| **`item_count = 0` 的空桶** | **合法，不拒** |
| **`item_count` 与 blob 内实际信封块数不符** | **不校验、不拒**（服务端不解析 blob ⇒ 无从数起，I8）；该字段只作索引 / 诊断 / 自愈判据用（ADR D10） |
| 同请求内重复 `bucket_key` | 去重取**首条**，其余忽略 |
| blob **非完整信封文本块拼接** | **不拒**（结构非法条目属客户端责任，I8） |
| 邮箱未验证 | 403 `email_not_verified` |
| 未授权 / 缺 `X-Device-Id` | 401 / 400，由 `requireAuth` 与 `assertHeaderString` 提供 |

### 5.5 `src/index.ts` —— 路由插入两条 case

插在 `/v1/sync/pull`（`:107-111`）之后、`/v1/account/data`（`:119-123`）之前，各 4 行，照抄老端点的 `assertMethod` + `requireAuth` + `return handle(...)` 形态。不新增 `DEVICE_PATH_RE` 之类的正则。

### 5.6 `src/account.ts` —— `handleDeleteAccountData` 扩展第二段清理

现状：单段循环删 `u/${userId}/` 前缀对象 + `DELETE FROM sync_items`，返回 `{ ok, deleted_objects, deleted_items }`（`:46-78`）。

改造：在既有循环之后追加**第二段**，删 `b/${userId}/` 前缀对象 + `DELETE FROM sync_buckets WHERE id IN (…LIMIT 500)`，同样受 `MAX_ROUNDS` 约束、同样「任一批失败 ⇒ 500 `clear_incomplete`」。

返回值键名**保持不变**：`deleted_objects` = 两前缀对象总数；`deleted_items` = **两索引表删除行数之和**（语义变宽、键名不变，向后兼容）。

### 5.7 `wrangler.toml` —— `[vars]` 新增一行

```toml
WRITE_BUDGET_BUCKETS_PER_DAY = "8000"
```

并在其上方补口径注释（说明桶级预算与条目级预算分表计数、每账号口径）。**其余内容逐字不动**（routes / d1 / r2 / 其余 vars）。

### 5.8 `test/apply-migrations.ts` —— 预期**零改动**

装载走 `@.cloudflare/vitest-direct-harness` 的 `applyD1Migrations(env.DB, env.TEST_MIGRATIONS)`，**按 `migrations/` 目录自动发现全部 `.sql`**，且 harness 自己负责迁移目录路径（`test/apply-migrations.ts:22-25` 的既有注释已说明这一点）。⇒ 新增 0004 文件即被自动装载。

**该预期必须由 AC-110 实测证明**：若 harness 实际只装载到某个固定编号，则本批唯一允许的替代改法是补一行装载路径，仍不改 `migrations/` 之外的任何东西（AC-110 的兜底分支）。

### 5.9 不改动的文件（显式登记）

`src/auth.ts` · `src/crypto.ts` · `src/r2-write.ts` · `src/dlq.ts` · `src/mail.ts` · `src/errors.ts` · `src/rate-limit.ts` · `src/kv.ts` · `migrations/0001_init.sql` · `migrations/0002_*.sql` · `migrations/0003_*.sql` · `test/` 既有 12 个 `.test.ts` · `WeaselServer/**`（全部）· `output/**`。

### 5.10 本批文件清单（改动允许范围，T19 提交前的 `git diff --stat` 以此为准）

| 文件 | 动作 | 对应任务 |
|------|------|----------|
| `cloud/fluxing-sync/migrations/0004_sync_buckets.sql` | 新增 | T1 |
| `cloud/fluxing-sync/src/wire.ts` | 改（新增常量 + `bucketR2KeyFor` + `validateBucketItem`） | T2 |
| `cloud/fluxing-sync/src/env.ts` | 改（新增 `WRITE_BUDGET_BUCKETS_PER_DAY` 读取） | T3 |
| `cloud/fluxing-sync/wrangler.toml` | 改（`[vars]` 新增一行 + 口径注释） | T3 |
| `cloud/fluxing-sync/src/sync.ts` | 改（新增桶级 5 个函数，老函数不动） | T4 / T5 / T6 |
| `cloud/fluxing-sync/src/index.ts` | 改（新增两条 `case`） | T7 |
| `cloud/fluxing-sync/src/account.ts` | 改（`handleDeleteAccountData` 追加第二段） | T8 |
| `cloud/fluxing-sync/test/buckets-push.test.ts` | 新增 | T9 |
| `cloud/fluxing-sync/test/buckets-pull.test.ts` | 新增 | T10 |
| `cloud/fluxing-sync/test/buckets-write-budget.test.ts` | 新增 | T11 |
| `cloud/fluxing-sync/test/buckets-account-data-delete.test.ts` | 新增 | T12 |
| `cloud/fluxing-sync/test/apply-migrations.ts` | **预期零改动**（AC-110 证明；不成立才补装载路径） | T13 |
| `docs/adr/0077-bucket-level-sync-storage-architecture.md` | 改（状态回填 + 偏离附录） | T17 |
| `CLAUDE.md` / `AGENTS.md` | 改（批次指针，按 §9 收尾双产物规则） | T18 |
| `.specify/specs/102-bucket-level-sync-storage/{spec,plan,tasks}.md` | 本三件套 | — |

**禁止出现在 diff 中**：`migrations/0001~0003`、`src/auth.ts`、`src/crypto.ts`、`src/errors.ts`、`src/r2-write.ts`、`src/dlq.ts`、`src/mail.ts`、`src/rate-limit.ts`、`src/kv.ts`、既有 12 个 `test/*.test.ts`、`WeaselServer/**`、`output/**`、`librime`（submodule，红线）。

---

## 6. 接口契约

### 6.1 `POST /v1/sync/buckets/push`

请求（Bearer + `X-Device-Id` 头）：

```json
{
  "device_id": "device-id-字符串",
  "buckets": [
    { "bucket_key": "<base64url OMAC>", "hlc": {"ms": 1700000000000, "counter": 0, "device": "dev-1"},
      "cipher": "<N 个条目信封文本块直接串接的 UTF-8 文本，base64 或原文二选一，见下>",
      "item_count": 850, "cipher_len": 118400 }
  ]
}
```

**`cipher` 的编码**：客户端的 blob 本身就是 8 行制信封的 UTF-8 **文本**（`FluxingCloudEnvelope.h:SerializeEnvelope`），不含 NUL、不含二进制 ⇒ **建议直接传文本**，不做 base64。

- 好处：服务端无需 base64 解码、`cipher_len` 与 R2 对象字节数天然一致、blob 可原样 `put` 到 R2 供桶级 pull 原样回传。
- 代价：请求体膨胀约 4/3 ⇒ `MAX_PUSH_BUCKETS_TOTAL_BYTES` 按膨胀后的字节数取值。
- 该选择登记为 §8 **Q-B 待拍板**；若拍成 base64，服务端必须解码后再 put，且 `cipher_len` 要定义为**解码后**字节数，pull 响应同样需明确编码。**两种选择都必须全链一致，且都属跨端契约** ⇒ 必须在 BR-①g 客户端开工前定死。

响应 200：

```json
{ "accepted": ["<bucket_key>", …], "skipped": ["<bucket_key>", …], "server_seq": 12345 }
```

- `accepted` / `skipped` **是桶键数组**（ADR D3 / 备选方案「让 push 响应按桶回报」的采纳形态）。
- **不含** `accepted_entries` / `skipped_entries`（I8：服务端不数条目，无从产生条目级反馈）。
- 全部 `skipped` 时 `server_seq` 返回当前 `MAX(id)`，与老 push 同构（`src/sync.ts:382-401`）。

错误：400 / 401 / 403 `email_not_verified` / 413 / 429 `write_budget_exceeded` / 502 `object_write_failed`（D1 零写）/ D1 配额 503 `storage_quota_exceeded`（既有顶层 `toErrorResponse` 兜住，本批不改 `src/errors.ts`）。

### 6.2 `POST /v1/sync/buckets/pull`

请求：`{ "since"?: number, "cursor"?: number, "limit"?: number }`（`limit` 单位 = **桶数**，复用 `DEFAULT_PULL_LIMIT = 50` / `MAX_PULL_LIMIT = 100`，**不新增常量**）。

响应 200：

```json
{
  "buckets": [
    { "device_id": "dev-1", "bucket_key": "…", "hlc": {"ms":…,"counter":…,"device":"…"},
      "cipher": "<blob 原文>", "item_count": 850, "server_seq": 101 }
  ],
  "next_cursor": 101,
  "has_more": false,
  "server_seq": 101
}
```

- 键集**固定为四个**：`{ buckets, next_cursor, has_more, server_seq }`。
- **不新增 `healed`**（老 pull 也没有）；**不回 `cipher_len`**（可由 `cipher` 自行测得，且老 pull 亦不回 `cipher_len`）。
- 顶层 `server_seq` 语义 = 该用户 `sync_buckets` 的 `MAX(id)`；**空页时回请求游标 `after`**（与老 pull 的 `:471-477` 同构）。
- 桶级 R2 对象缺失时**跳过该桶**（`item_count > 0` ⇒ 删索引行自愈）；`item_count = 0` 的空桶**照常返回、`cipher` 为空串**。

### 6.3 老端点

`/v1/sync/push` / `/v1/sync/pull` 的**请求键、响应键、取值语义、错误码逐字不变**，继续读写 `sync_items`，与新表 / 新端点互不影响。

### 6.4 R2 键空间

| 路径 | 键前缀 | 键 |
|------|--------|----|
| 条目级（老） | `u/` | `u/${userId}/${sha256(item_key)}` |
| 桶级（新） | `b/` | `b/${userId}/${sha256(bucket_key)}` |

两前缀**并存**；账户注销必须清两个前缀（AC-108）。

---

## 7. 测试策略

新增测试文件（建议 4 个，按主题分文件，避免单文件过大）：

| 文件 | 覆盖 | 对应 AC |
|------|------|---------|
| `test/buckets-push.test.ts` | 首轮全写 / 未变 skipped / 更旧拒绝 / 同毫秒 counter 决胜 / 重复桶键去重 / 空桶合法 / 超限 400 / 非数组 400 | AC-103 / AC-104 / AC-105 / AC-106 / AC-117 |
| `test/buckets-pull.test.ts` | 桶数 = 索引行数；`item_count` 与 blob 文本块数一致；`item_count>0` 且对象缺失 ⇒ 行被删 + 响应无该桶；`item_count=0` 空桶照常返回空 blob；分页 `has_more` / `next_cursor`；响应键集恰为四个 | AC-106 / AC-107 / AC-117 |
| `test/buckets-write-budget.test.ts` | `sync_buckets` 口径估算；超限 429 `write_budget_exceeded` + 两表行数零增长 + R2 零写；`updated_at` 只计当日；与老 `write-budget.test.ts` 互不影响 | AC-104 / AC-105 |
| `test/buckets-account-data-delete.test.ts` | 桶级 push 落库后 `DELETE /v1/account/data` ⇒ `sync_buckets` 行数零增长（含 `ON DELETE CASCADE` 路径）、`b/` 与 `u/` 两前缀均 0 个对象、响应键名不变 | AC-108 |

既有 **12 文件 / 151 用例逐条保留**，尤其 `write-budget`（7 用例，含「老护栏默认 80,000 行」断言）必须原样通过 ⇒ 这是 AC-104「既有常量取值不动」的机器证明。

辅助：`d1-quota-error-mapping` / `push-quota-error-mapping` 的 D1 配额判据**不得因新表而失效**（`toErrorResponse` 未改，预期无需改动）。

测试基建复用：`test/apply-migrations.ts` 的 `seedUser` / `mintToken`（自动装载 0004 由 AC-110 证明）、`SELF.fetch` 走真请求路径、只打断 `env.DB.batch` / `env.SYNC_OBJECTS` 造故障。

---

## 8. 待拍板项（不阻塞开工，但必须在客户端开工前定死）

| 编号 | 事项 | 推荐答案 | 后果 |
|------|------|----------|------|
| **Q-A** | `DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY` 取值 | **8,000** | 偏严 ⇒ 极端多设备全量重建会被 429 挡住（可解释、可重试）；偏松 ⇒ 回到「护栏挡不住全局额度」的老问题（spec §7 已登记「每账号 ≠ 全局」边界）。ADR 0077 未给数值，属本批新增决策 |
| **Q-B** | `cipher` 传文本还是 base64 | **文本原样** | 影响跨端契约、`cipher_len` 定义、请求体膨胀比；与 Q-B 联动的还有「pull 响应是否同样回文本」。必须与 BR-①g 一起定，本批单侧定下后写入 ADR 0077 附录 |
| **Q-C** | `MAX_PUSH_BUCKETS = 100` 是否够用 | **100** | 首导 852 桶 ⇒ 客户端需分 9 个请求；与 D11 的「首导 ≤ 30 s」判据可能冲突 ⇒ 由客户端限流 + 分批承担，服务端不再放大该值（放大 = 单请求内存风险） |

以上三项写入 spec.md §9（Q1…Q5 之外，ADR 0077 内部 D2 与 D9 的分桶口径互斥仍为 **Q1 `[NEEDS CLARIFICATION]`**：D2 说固定 64 分片 / 每桶约 852 条 / 共 64 桶 / 首导 128 行，D9 说每桶 64 条 / 共 852 桶 / 首导 852 行；而 D9 的「单桶 put ≈ 7.5 s」是按**每条目一次 put** 计算的，与 D3「一个桶 = 一个对象 = 一次 put」矛盾 ⇒ **需用户拍板，不得自行选一个**）。

**服务端实现与桶大小无关**（不解释 bucket_key、不猜分桶）⇒ Q1 不阻塞本批，但阻塞 BR-①g 的客户端分桶与首导耗时判据。

---

## 9. 风险与回滚

| 风险 | 说明 | 缓解 |
|------|------|------|
| Worker 内存 | 一轮全量 = 852 桶 × 数 MB ⇒ 单请求不可承载 | `MAX_PUSH_BUCKETS` / `MAX_BUCKET_CIPHER_BYTES` / `MAX_PUSH_BUCKETS_TOTAL_BYTES` 三道闸，超限 400 / 413 而非崩溃 |
| **护栏口径** | 桶级预算仍是**每账号**口径，挡不住 D1 的**全局** 10 万行/日 | 如实登记（spec §7），不做「平台额度护盾」的承诺；Q-A 取值偏严；是否改全局口径属 BR-② |
| **首导仍是一次性传输** | 新表不改变首导要推 852 个桶的事实（D9） | 客户端分批 + 跨日；D11 已给非 E2E 迁移通道备选；本批不实现迁移 |
| **healed 粒度变粗** | 自愈信号从「具体条目」变「桶 + `item_count`」（D10） | 空桶以 `item_count = 0` 表示，自愈删行只针对 `item_count > 0`；诊断日志登记被删桶 |
| **读路径附带写占比上升** | 首次全量拉取后校验 blob 时，桶级自愈可能一次删掉约 852 行 | 接受该风险（ADR 已记）；桶数远小于条目数（64 或 852 ≪ 54,622），整体写放大远小于老路径 |
| Durable Object 子请求上限 | 1,000 子请求 / 单次 Cloudflare 调用 | 单请求 ≤ 100 桶 ⇒ put/get 并发 6、子请求 ≤ 100；pull 同理 |
| 索引表膨胀 | `sync_buckets` 行数上界 = `设备数 32 × 桶数` | 远小于 `sync_items` 的 40,061 行；无新风险 |
| 两个 `server_seq` 编号空间 | 游标不可跨路径复用 | AC-109 单测钉住；BR-①g 侧车升 v2 + `seq = 0` |
| `migrations` 自动装载假设 | `applyD1Migrations` 是否真按目录自动发现 | AC-110 作为**开工第一道硬闸**；不成立时改走 §5.8 的兜底分支 |
| 部署打爆现网额度 | D1 免费额度 11-24 轮 `rows_written_24h = 69,428 / 100,000` | **本批不部署、不 apply 迁移**，须用户授权并先读 `rows_written_24h`（spec NFR-106） |

**回滚**：本批全部改动落在新表 / 新函数 / 两条新 `case` / 一段新 `vars` / 新测试文件，唯一侵入既有代码的点是 `handleDeleteAccountData` 追加第二段。回滚 = revert 该 commit ⇒ 老表、老端点、老客户端行为**完全不变**；`sync_buckets` 表与桶级对象**不删**，留作回滚保险（ADR 0077 可逆性 D11）。

---

## 10. 与 ADR 0077 / 取证报告的偏离登记

| # | 偏离 | 依据 | 处置 |
|---|------|------|------|
| 1 | 取证报告 `reports/fluxing-br-1f/recon-blob-arch.md` 的**桶级 AEAD** 草案（桶 nonce + 桶 AAD + 长度前缀分帧）与 ADR 0077 **D3**（`docs/adr/0077:92-97`，`:97` 明写「否决备选方案中的桶级 AEAD（长度前缀分帧 + 桶 nonce）」）冲突 | 宪法 R2：以权威决策件为准 | **按 D3 实现**：blob = N 个既有信封文本块直接串接、服务端不解析、无桶级 nonce。报告草案只作背景 |
| 2 | 报告草案的索引表列 `nonce` | ADR 0077 D1 的列集是 `hlc_device` / `cipher_len`，无 `nonce` | 按 D1 建表；`cipher_len`（不是 `blob_len`） |
| 3 | 报告草案唯一约束 `(user_id, bucket_key)` | ADR 0077 D1 是 `(user_id, device_id, bucket_key)` | 按 D1；后果 = 每设备独立视图、云端保留最后写入设备（D5） |
| 4 | 报告草案 push 请求带 `entries[]`（条目数组） | ADR 0077 D3「客户端提交整桶、服务端不猜边界」 | push 改为整包 `cipher` + `item_count` + `cipher_len`，无 `entries[]` |
| 5 | 报告草案 pull 响应带桶级 `nonce` | ADR 0077 D7 的响应示例无 `nonce`，有 `device_id` / `item_count` | 按 D7 |
| 6 | 报告草案 push 响应 `accepted_entries` / `skipped_entries` | ADR 0077 备选方案表采纳「按桶回报」；I8 服务端不数条目 | 只回 `accepted` / `skipped`（桶键）+ `server_seq` |
| 7 | 报告草案 `migration: 0003` | `migrations/0003_sync_items_updated_index.sql` 已存在 | 新表用 **0004** |
| 8 | 报告草案的 R2 键 `sync/${userId}/${bucket_key}` | 既有 `r2KeyFor()` 形态是 `u/${userId}/${sha256(key)}`，且注销清理靠前缀列举 | 改为 `b/${userId}/${sha256(bucket_key)}`，并把 `u/` 列为保留前缀（否则注销会漏） |
| 9 | ADR 0077 **内部** D2（64 分片）与 D9（每桶 64 条）分桶口径互斥；D9 的 7.5 s 按每条目一次 put 计算，与 D3 矛盾 | ADR 自相矛盾 | 标 **Q1 `[NEEDS CLARIFICATION]`** 交用户拍板；服务端不受影响，客户端开工前必须定 |
| 10 | 老 pull 响应**无 `healed`**（实盘 `src/sync.ts:438-465` / `:467-477`），与「自愈可回读」的直觉不符 | 实盘代码 | 桶级 pull 同样不回 `healed`；判据改看索引行删除 + 日志；已在 §3.3 / §6.2 / AC-106 落实 |
| 11 | ADR 0077 未给 `DEFAULT_WRITE_BUDGET_BUCKETS_PER_DAY` 数值、未定 `cipher` 编码 | 决策件留白 | 本批按 §8 Q-A / Q-B 给出推荐值并拍板后回填 ADR |

### 10.1 实盘已有实现与本计划的差异（2026-11-24 12:19–12:29，另一会话产出，本会话未改其代码）

`git status -sb` 显示工作树已存在本批服务端实现：新增 `cloud/fluxing-sync/src/sync-buckets.ts`（17,443 B）· `cloud/fluxing-sync/migrations/0004_sync_buckets.sql`（3,703 B）· `cloud/fluxing-sync/test/bucket-sync.test.ts`（33,488 B）；改动 `src/account.ts`（+46）· `src/env.ts`（+49）· `src/index.ts`（+16）· `src/wire.ts`（+130）⇒ **合计 228 insertions / 13 deletions**。这些文件属**另一写入者**，本三件套不触碰。差异与推荐处置：

| # | 议题 | 实盘（已存在） | 本计划（先前措辞） | 推荐处置 |
|---|------|----------------|--------------------|----------|
| 12 | 模块落点 | 新文件 `src/sync-buckets.ts` | 追加进 `src/sync.ts`（§5.3 / T4-T6） | **采纳实盘**。新文件隔离度更高、老 `sync.ts` 保持零 diff（对 AC-102 / T14 逐字不动判据更有利）⇒ 本计划 §5.3 / §5.10 / T4-T6 以实盘为准 |
| 13 | 函数命名 | `handleBucketPush` / `handleBucketPull`（单数 Bucket）、`countTodayWrittenBucketRows` | `handleBucketsPush` / `handleBucketsPull` / `countTodayWrittenBuckets` / `planBucketsWriteRows` / `evaluateBucketsWriteBudget` | **采纳实盘命名**，本计划与 tasks.md 中的函数名按实盘订正，不反向改代码 |
| 14 | **桶写预算默认值** | 单测钉住 **4,000 桶行/账号/日** | §8 Q-A 建议 **8,000** | **建议采纳实盘 4,000**：一次全库重建（852 桶 × 2 = 1,704 行）可容纳 2 次、比老护栏 80,000 行严得多，更能兜住「每账号口径挡不住全局 10 万行额度」的已知边界（§9）；若拍 8,000 ⇒ `env.ts` 默认值、`wrangler.toml`、`bucket-sync.test.ts:163` 三处必须同步改 ⇒ 属**需用户拍板**，登记为 Q-A 的实际答案 |
| 15 | R2 桶键 | `b/<uid>/<sha256(device)>/<sha256(bucket_key)>`（含设备哈希） | `b/${userId}/${sha256(bucket_key)}`（§6.4，无设备段） | **采纳实盘**。键内含设备段才是 D5「每设备独立视图」的物理保证（否则不同设备争用同一对象 ⇒ lost update）⇒ §6.4 作废，改按本行 |
| 16 | `device_id` 语义 | 存 `devices.id`（行主键），与 `refresh_tokens.device_id` 口径一致，明确区分于 `hlc_device` | 未区分，笼统写「桶最后写入设备」 | **采纳实盘**：服务端权威比较键三元组里的 `device_id` 是**服务端行归属**，而 pull 响应 JSON 的 `device_id` 是 D7 要求的**每设备独立视图标识**（ADR 原文「pull 响应回 `device_id` 告知客户端每个桶来自哪个设备」）。⇒ **需用户拍板**：响应回 `devices.id`（实盘倾向，客户端须能解析为分桶键）还是回 HLC device 串（ADR D7 字面）；本批文档不得把任一写法写成 ADR 结论 |
| 17 | `ON DELETE CASCADE` | **不带**（`user_id TEXT NOT NULL REFERENCES users(id)`），靠 `account.ts` 显式两段删除 | 带 `ON DELETE CASCADE`（§4.2） | **采纳实盘**：不依赖 D1 的级联语义（`foreign_keys` 是否开启在 D1 侧不确定），显式删除 + 自证 list 计数更可控；本计划 §4.2 的 DDL 按实盘订正 |
| 18 | 索引集合 | 三条：`idx_sync_buckets_user_device_item`（UNIQUE 三元）/ `idx_sync_buckets_user_id(user_id, id)` / `idx_sync_buckets_user_updated(user_id, updated_at)` | 只写了两条，**漏 `(user_id, updated_at)`** | **采纳实盘**：第三条是 D8 写预算 `COUNT ... WHERE user_id=? AND updated_at>=?` 的必需索引，与 `0003_sync_items_updated_index.sql` 同构 ⇒ 本计划 §4.2 缺它属**缺陷**，已在此登记 |
| 19 | 索引命名风格 | `idx_sync_buckets_*` | `uniq_buckets_*` / `idx_buckets_*` | **采纳实盘**（与既有 `idx_sync_*` 前缀一致） |
| 20 | 测试布局 | 单文件 `bucket-sync.test.ts`（12 个 `it`，含老端点并存回归、405/401/403 路由面） | 计划新增 4 个测试文件（§7） | **采纳实盘**，§7 的多文件划分作废；覆盖面判据以实盘的 12 个用例是否覆盖 AC-103…AC-108 为准（**由 T13 实测 `npm run check` 判定，不凭本计划断言其已覆盖**） |
| 21 | `cipher` 编码 | 由 `validateBucketPush` 按 UTF-8 字节校验（`bucket-sync.test.ts:392` 测 `cipher` 非法 ⇒ 400） | §8 Q-B 待拍板（文本 vs base64） | 状态不变：**仍以 §8 Q-B 待拍板处理**，实盘倾向文本口径，但未定稿前不得写成结论 |

**门禁状态**：实盘实现**尚未由本会话验证** —— 未跑 `npm run typecheck` / `npm run check`，故不得声称其可用或已过闸。T13（门禁）与 T1–T12 的任务对象需按本表订正为「**核对并补齐既有实现**」而非「从零新建」。

---

## 11. 落地顺序（与 tasks.md 对应）

1. **前置复测**（0.5 h）：`npm run typecheck` / `npm run check` 必须复现 0 / 0 与 12 文件 / 151 用例基线（**本轮未跑，不沿用口头基线**）。
2. **T1 建表 + 装载证明**（AC-110 硬闸）⇒ 单独跑通迁移装载，失败即停线，不改代码绕它。
3. **T2 常量**（`wire.ts`）→ **T3 预算变量**（`env.ts` + `wrangler.toml`）。
4. **T4 护栏三函数**（`sync.ts`，纯读）→ **T5 push** → **T6 pull**。
5. **T7 路由装配**（`index.ts`）。
6. **T8 账户清理扩展**（`account.ts`，唯一侵入既有函数处）。
7. **T9–T12 四个测试文件** ⇒ **T13 门禁**（typecheck 0 + check 0 + 用例数只增不减）。
8. **T14 老端点逐字节回归证明**（`git diff` 判据，非口头声明）。
9. **T15–T18 文档**：ADR 0077 状态回填 + 偏离附录、spec 100/101 交叉引用、CLAUDE.md / AGENTS.md 批次指针、handoff + relay prompt。
10. **T19 commit**（Conventional Commits + `fluxing:` 前缀，**不 push、不 deploy、不 apply 迁移**）。
11. **T20 部署与生产复验闸门**（须用户授权）。

逐任务的目标 / 改动点 / 验收判据 / 禁止事项见 [tasks.md](tasks.md)。
