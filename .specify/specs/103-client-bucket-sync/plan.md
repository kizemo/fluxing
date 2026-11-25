# 103 · 客户端桶化同步 · 实现计划（plan.md）

> **性质**：客户端批次 **BR-①g**。本批**只改客户端**（`WeaselServer/Cloud/**`），服务端（`cloud/fluxing-sync`）**不改**。
> **唯一权威设计来源**：[ADR 0077 桶级同步存储架构](../../../docs/adr/0077-bucket-level-sync-storage-architecture.md)（Status=Accepted，客户端段 D12–D28）
> **需求件**：[spec.md](spec.md)（FR-201…FR-216 / AC-201…AC-212）
> **任务件**：[tasks.md](tasks.md)（T1…T16）
> **上游**：spec [102-bucket-level-sync-storage](../102-bucket-level-sync-storage/spec.md)（服务端先行批次 BR-①f，已部署生产）· [ADR 0076](../../../docs/adr/0076-cloud-write-amplification-governance-and-zero-write-registration.md)

---

## 1. 本批做什么、做到哪一步

### 1.1 一句话

把客户端的「**逐条推拉**」改成「**按桶推拉**」，用**五阶段状态机**驱动灰度，让「**老端点退役**」成为**四段可验证、可回滚**的流程。

### 1.2 为什么客户端是下一批（服务端已就位）

ADR 0077 D11 把实现分成两期：服务端先行（BR-①f，已部署生产并冒烟 9/9 PASS）、客户端随后（BR-①g）。
服务端先行不改变**任何客户端行为** ⇒ 客户端不切桶，**写放大一天都不会减少** ⇒ 客户端切换是本 ADR 生效于生产的**必要条件**。

- 本批交付物 = **可编译、可单测、可回滚的客户端能力**，含**出包装机 + 真机判据**。
- 本批**不改**服务端任何一行 ⇒ 服务端生产版本不受影响，回滚只需改客户端环境变量。

### 1.3 交付判据

判据全部落在 spec.md 的 **AC-201…AC-212**。其中**本批必须实测通过**的是 AC-201…AC-211；
**AC-212（真实传输时长未实测）** 属需真机抽样的观测项，**当前记「未判定」**，**不得**用推算值记 PASS。

---

## Constitution Check (against `.specify/memory/constitution.md` v1.1.0)

依据：[.specify/memory/constitution.md](../../memory/constitution.md) §「核心原则」。五条逐一过闸。

| 原则 | 原文要义 | 本计划如何满足 | 阻断条件 |
|------|----------|----------------|----------|
| **I. Intent Before Implementation（意图先于实现）** | 写码前必须存在已批准的需求工件，意图、范围、验收判据三者明确 | 意图与范围来自 **ADR 0077 客户端段 D12–D28**；需求件 spec.md 已先行落盘，含范围边界（§3）、9 条不变量（§7）、12 条验收判据（§6）；本 plan.md 在 tasks.md 之前存在，任务编号引用本 plan 的章节与文件名 | spec.md 或 ADR 0077 任一缺失或被推翻 ⇒ 不得开工 |
| **II. Test-Backed Change（测试支撑变更）** | 行为变更必须同时更新既有测试或新增测试，判据须明确成功 / 失败条件及如何证明 | 每个新行为均有对应单测（§7）；门禁 = 云端单测工程 `PASS`/`FAIL` 计数 **FAIL = 0** 且用例数只增不减（基线 **PASS=813**）；条目级路径的既有断言**逐条保留** | 任一条既有用例被删或断言被放宽 ⇒ 视同红线，本批停线 |
| **III. Spec-Artifact Discipline（工件与真相源纪律）** | 行为 / API / 架构变更必须先创建或更新规范工件；ADR / 需求 / 计划 / 任务齐备，命名与 ID 约定一致，无悬空引用 | ADR 0077 已存在并编号唯一；本目录三件套按 spec 100 / 101 / 102 既有体例命名；FR / AC / T 编号连续且互相引用无悬空；与 ADR 的任何偏离**就地登记**（spec §8 共 6 条 + ADR「客户端段偏离登记」3 条） | 出现未登记的偏离、或工件间编号互相矛盾 ⇒ 不得进收尾 |
| **IV. Structured Clarification（结构化澄清优先）** | 实现前把未明需求转成穷尽、独立可答的问题，逐项给推荐答案与后果，并写入 spec 的「待核实问题」 | spec.md §9 已把不确定项转成 **Q1…Q4** 编号问题，每条附现状与影响 | `[NEEDS CLARIFICATION]` 未清零前**只允许做不依赖该答案的部分**（§2 表前置 6 / T1）；依赖答案的任务不得开工 |
| **V. Incremental Delivery（增量交付）** | 切成小而可验证的任务，单任务不越界，每阶段都有机器可判判据、文档同步、提交意图 | 16 个任务按依赖分层：纯函数层 → 阶段机 → 线格式 → 状态文件 → 编排接入 → 降级与恢复 → 测试 → 门禁 → 文档；每个任务带验收判据（§3）；**阶段 0 默认不变** ⇒ 每层都可单独回滚 | 单任务涉及 >4 文件或 >4h ⇒ 拆分后再排 |

**附加红线（来自宪法 R1–R9，本批适用项）**：

- **R1 稳定契约**：IPC 协议不变（无 `include/WeaselIPCData.h` 改动）。条目级加密附注**逐字不动**（`include/FluxingCloudEnvelope.h` 不改）。
- **R2 单一真相源**：「技术文档不得把未实施目标写成既成事实」⇒ **阶段 0 是默认值**、未开闸时行为与本批之前逐字等价；**AC-212 当前记「未判定」**，不得写成「传输已验证」。
- **R2 另条**：「文档不得把待核实项写成已确认结论」⇒ 单轮预估传输时长**必须**写「未判定 + 需要什么」（spec Q1）。
- **R3 任务优先级 + 用户故事标签**、**R4 计划必须含 Constitution Check**：本节即第 2 节。
- **R5 任务规模**：单任务 ≤4h、≤3 个生产文件（`tasks.md` 逐任务登记，测试文件另计）。
- **P2/P5 中文**：本批新增的用户可见错误文案全部为简体中文。
- **P4 Conventional Commits + 模块前缀**：本批提交前缀用 `WeaselServer:`（客户端同步面），subject 形如 `WeaselServer: 云同步客户端桶化（阶段状态机 + 侧车 v2）`。

---

## 3. 架构与数据流

### 3.1 客户端同步编排（既有顺序，桶化后加法）

现行 `CloudSyncCoordinator::SyncOnce` 的顺序（`WeaselServer/Cloud/CloudSyncService.cpp`）：

```
1) 导出本地词典
2) 拉远端（按页分批，游标只增不减）
3) 对账并分配 / 复用 HLC（R1 远端 HLC 沿用 / R2 库存未证实不推送 / R4 分歧本地胜出）
4) 合并（MergeEntries：同键 HLC 大者胜）
5) 把合并结果回写本地
6) 推送「本地改动过、且合并后仍是本地版本」的条目（分批）
7) 更新 sidecar（只把「已同步」的条目标记为已同步；没推成功的保留旧行，下一轮继续推）
```

桶化后**不改变这个顺序**，只在 2 / 6 / 7 三处按阶段分流：

```
2) 拉远端
   阶段 0：只走条目级端点（既有路径逐字不动）
   阶段 1/2：条目级 + 桶级（桶级解包后的条目并入同一远端集合）
   阶段 3/4：只走桶级端点
6) 推送
   阶段 0：只走条目级端点
   阶段 1/2：条目级 + 桶级（桶级推「指纹变化」的整桶）
   阶段 3/4：只走桶级端点
7) 更新 sidecar
   阶段 0：条目级行（既有语义）
   阶段 ≥1：条目级行 + 桶行 + 阶段位 + 桶游标
```

**关键约束**：阶段 0/1 期间条目级路径**逐字不动** ⇒ 未开闸时行为与本批之前等价（AC-210）。

### 3.2 桶化推送数据流（阶段 ≥ 1 的桶侧）

```
BuildBucketPushBody（CloudBucketSync.h）
  输入：CloudBucketPushItem[] = { bucket_key(64 hex), item_count, cipher_b64(整桶 blob), hlc(桶级) }
  输出：请求体 JSON（device_id 为空 ⇒ 不带该字段）

服务端：resolvePushDeviceId 以「当前登录设备的行主键」为权威
  ⇒ 客户端的 device_id 与服务端行主键不是同一个东西 ⇒ **客户端一律不上报 device_id**
```

### 3.3 桶化拉取数据流（阶段 ≥ 1 的桶侧）

```
GET /v1/sync/buckets/pull?since=<bseq>&limit=<≤100>&cursor=<base64url(id)>
  响应：{ "buckets": [ { bucket_key, device_id, hlc, cipher, item_count, server_seq } ],
           "next_cursor": "...", "has_more": bool, "server_seq": n }

ParseBucketPullResponse（CloudBucketSync.h，独立实现）
  → 每桶：UnpackBucketBlob（8 行一块切分）→ 逐块 ParseEnvelope → 复用既有解密路径
  → 解不开只计数；userdb 类另做形态校验并单独计数 malformed（同 OpenItemsTolerant 语义）
```

**硬闸**：**不得**用 `ParsePullResponse` 解析桶响应 —— 它对每个条目要求 `nonce` 字段
（`CloudSyncService.cpp:264`），桶对象**无 nonce** ⇒ 会被整批判为非法（AC-207）。

### 3.4 不变量映射

| ADR 0077 不变量 | 本批客户端实现位 |
|-----------------|------------------|
| I1 服务端只见密文 | 客户端把桶包当**不透明 blob** 传输；`cipher_b64` 是整包 base64，服务端不解析 |
| I2 条目级 AAD 逐字不变 | `include/FluxingCloudEnvelope.h` **零改动**；桶包由既有 `SerializeEnvelope` 产物串接 |
| I3 条目级 HLC 保留 | 桶级时序取桶内条目时序**最大值**；条目级时序仍在包内 ⇒ 逐条裁决能力不变 |
| I4 桶级无二次加密 | `PackBucketBlob` 只串接既有密文块，**不引入**桶级随机串/附注 |
| I5 阶段不得自动递增 | `ResolveBucketPhase` 只解析外部值 + 侧车值，**无自增逻辑** |
| I6 两套编号空间互不解释 | 侧车 v2：`seq`（条目级）与 `bseq`（桶级）**各自推进、互不参照**（AC-211） |
| I7 未开闸零行为变化 | 阶段 0 为默认；条目级代码路径**逐字不动**（AC-210） |
| I8 桶内容不变则不推 | 桶指纹用**语义口径**（与条目级侧车同源）⇒ 稳态零写（AC-206） |
| I9 桶级不猜测桶边界 | 桶边界由**客户端**计算；服务端只按 (用户, 设备, 桶标识) 精确 upsert |

---

## 4. 数据结构设计

### 4.1 侧车 v2 文本格式

```
#fluxing-cloud-sync-v2
seq\t<条目级游标>
keyfp\t<口令指纹>
bseq\t<桶级游标>
phase\t<阶段位 0..4>
row\t<item_key>\t<ms>\t<counter>\t<device>\t<payload_hash>
brow\t<bucket_key>\t<ms>\t<counter>\t<device>\t<item_count>\t<fingerprint>
```

- **头行**：`#fluxing-cloud-sync-v2`（v1 是 `#fluxing-cloud-sync-v1`）。
- **保留 v1 字段**：`seq` / `keyfp` / `row` —— 语义**逐字不变**（`row` 的 `payload_hash` 仍是 `s2:` 口径）。
- **新增字段**：`bseq`（桶游标，**独立编号空间、强制从 0 起**）/ `phase`（阶段位）/ `brow`（桶行）。
- **必须能读 v1**：读到 v1 ⇒ **阶段 = 0**、**桶游标 = 0**、**无桶行**（AC-210）。

### 4.2 桶行 `brow` 的字段

| 字段 | 含义 | 来源 |
|---|---|---|
| `bucket_key` | 桶标识（64 位小写十六进制） | `ComputeBucketKeyId` |
| `ms` / `counter` / `device` | 该桶**已推成功**的桶级时序 | `AggregateBucketHlc`（上报前已按 D16 抬升） |
| `item_count` | 该桶条目数 | 分组时统计 |
| `fingerprint` | 该桶内容指纹 | `BucketFingerprint`（**语义口径**） |

**写回时机**：只有**推送成功**的桶才写 `brow`；没推成功的**不写** ⇒ 下一轮继续推（失败续传）。

---

## 5. 逐文件模块改造

> 标注：`←` = 插入位。**本批只允许改 `WeaselServer/Cloud/**` 与 `test/TestFluxingCloud/**`**。

### 5.1 `WeaselServer/Cloud/CloudBucketSync.h` / `.cpp` —— 纯函数层（**已落盘，未编译验证**）

现状（2026-11-25 09:35 核对）：`.h` 已声明桶键派生、分桶、桶包打包/拆包、桶级时序、桶内容指纹、**五阶段枚举 + 阶段解析**、**桶级线格式（推送体构造 + 拉取响应解析）**；
`.cpp` 已实现桶键派生、分桶、打包/拆包、桶级时序、内容指纹共 8 个函数，
**但阶段机与线格式的 4 个函数（阶段解析、生效解析、推送体构造、拉取响应解析）尚无实现** ⇒ 见 `tasks.md` T5 / T6。

新增/既有常量（与服务端 `cloud/fluxing-sync/src/wire.ts` 逐字对齐）：

| 常量 | 取值 | 服务端对应 |
|---|---|---|
| `kBucketShardCount` | 64 | —— （客户端分片参数） |
| `kBucketSubShardCount` | 16 | —— （客户端分片参数） |
| `kBucketCount` | 1024 | —— （64 × 16） |
| `kBucketMaxItems` | 4096 | `MAX_BUCKET_ITEMS` |
| `kBucketPushBatch` | 64 | `MAX_PUSH_BUCKETS` |
| `kBucketMaxBlobBytes` | 4 MiB | `MAX_BUCKET_BLOB_BYTES` |
| `kBucketMaxKeyBytes` | 512 | `MAX_BUCKET_KEY_BYTES` |
| `kBucketKeyHexLength` | 64 | —— （桶键十六进制长度） |
| `kBucketPhaseEnvVar` | `FLUXING_CLOUD_SYNC_BUCKET_PHASE` | —— （唯一的覆盖入口） |

**桶键派生口径**（D14）：`DeriveBucketMacKey`（HKDF 标签 `fluxing-bucketkey-mac-v1`）⇒ `ComputeBucketShard`（加密域摘要字节取一级/二级分片）⇒ `ComputeBucketKeyId`（再做一次 MAC ⇒ 64 位小写十六进制）。

### 5.2 `WeaselServer/Cloud/CloudSyncService.h` / `.cpp` —— 编排层

- **阶段解析与生效**：读 `kBucketPhaseEnvVar` ⇒ `ResolveBucketPhase`（环境变量优先、非法则回退侧车）⇒ 本轮阶段。
- **侧车 v2**：`BuildSidecarText` / `ParseSidecarText` 扩展为 v2（新增 `bseq` / `phase` / `brow`），**必须能读 v1**。
  ⚠ 既有 `BuildSidecarText` / `ParseSidecarText` 是**条目级侧车**的函数（`CloudSyncService.cpp:1668` / `:1696`），
  **老端点仍在用** ⇒ 桶级侧车必须**另起**一套函数（不改动既有条目级函数的语义），避免阶段 0 行为变化。
- **桶化推送**：按 (词典名, 分片) 分组 ⇒ 每桶算指纹 ⇒ 与侧车 `brow` 比对 ⇒ 指纹变化的桶打包推送（分批 ≤ `kBucketPushBatch`）。
- **桶化拉取**：按 `bseq` 拉桶（每页 ≤ 100；单轮页数上限 = **max(16, 词典数 × 11 + 2)**，硬上限 32 页）⇒ 解包 ⇒ 复用既有解密路径。
- **降级**：单桶 `item_count > kBucketMaxItems` 或 blob > `kBucketMaxBlobBytes` ⇒ 该桶走条目级端点推。
- **部分恢复**（切读阶段）：库存扫描完整时，「侧车已同步但远端视图缺失」的条目 ⇒ 所在桶强制重推。

### 5.3 `test/TestFluxingCloud/` —— 单测

新增桶化用例，**既有条目级用例逐条保留**（基线 `PASS=813 FAIL=0`）。

⚠ **本机已知坑**：`weasel.sln` **不含** `TestFluxingCloud` 工程 ⇒ 必须**单独 msbuild** 并显式带 `/p:SolutionDir=`，
否则跑到上一轮旧 exe（`PASS` 恒等于旧基线）。验收脚本见 `scripts/verify/verify-br1g-bucket.ps1`（按红线**不入库**）。

### 5.4 不改动的文件（显式登记）

`include/FluxingCloudEnvelope.h` · `include/WeaselIPCData.h` · `cloud/fluxing-sync/**`（全部）·
`WeaselServer/Cloud/CloudCrypto.{h,cpp}` · `WeaselServer/Cloud/CloudUserDictIo.{h,cpp}` ·
`WeaselServer/Cloud/CloudAccountDialog.cpp` · `librime/**`（子模块）。

### 5.5 本批文件清单（改动允许范围，提交前的 `git diff --stat` 以此为准）

| 文件 | 动作 | 对应任务 |
|------|------|----------|
| `WeaselServer/Cloud/CloudBucketSync.h` | 改（已完成，待核） | T2 / T3 / T4 / T5 / T6 |
| `WeaselServer/Cloud/CloudBucketSync.cpp` | 改（已完成，待核） | T2 / T3 / T4 / T5 / T6 |
| `WeaselServer/Cloud/CloudSyncService.h` | 改 | T7 / T8 / T9 / T10 |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 改 | T7 / T8 / T9 / T10 / T11 / T12 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 改（新增桶化用例） | T13 |
| `WeaselServer/WeaselServer.vcxproj` / `test/TestFluxingCloud/TestFluxingCloud.vcxproj` | 改（登记 `CloudBucketSync.cpp`） | T14 |
| `docs/adr/0077-bucket-level-sync-storage-architecture.md` | 改（已落盘） | T15 |
| `.specify/specs/103-client-bucket-sync/*` | 新增（spec / plan / tasks 已落盘） | T15 |
| `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` | 改（已落盘 §M） | T15 |
| `docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` | 改（已落盘） | T15 |
| `CLAUDE.md` | 改（已落盘） | T15 |

---

## 6. 参数与口径速查（**两个口径不可混用**）

| 项 | 取值 | 来源 |
|---|---|---|
| 一级分片数 | 64 | `kBucketShardCount` |
| 二级分片数 | 16 | `kBucketSubShardCount` |
| 每词典桶数 | **1024** | 64 × 16 |
| 每桶条目数 | ≈ **92** 条（本地全集 94,604 ÷ 1024）；仅按未上云 54,536 条计则为 53 条 | 派生值 |
| 单请求桶数上限 | **64** | `kBucketPushBatch` / `MAX_PUSH_BUCKETS` |
| 单页桶数上限 | **100** | 服务端 `MAX_PULL_LIMIT` |
| 单轮页数上限 | **max(16, 词典数 × 11 + 2)**，**不超过 32 页**（硬上限） | D27 动态计算（与 spec 103 AC-203 **逐字同源**，便于两处对账） |
| 单桶条目数上限 | **4096** | `kBucketMaxItems` / `MAX_BUCKET_ITEMS` |
| 单桶字节上限 | **4 MiB** | `kBucketMaxBlobBytes` / `MAX_BUCKET_BLOB_BYTES` |
| 客户端单次超时窗口 | **30 s** | `WeaselServer/Cloud/CloudTransport.h` |
| **索引行数**（存量/设备） | 54,536 → **1,024**（约 **53 倍**） | AC-204 (a) |
| **D1 实际写行数**（首导） | 54,536 → ≈ **2,048**（约 **27 倍**） | AC-204 (b) |
| 单请求索引写行数上界 | **2 × 本轮桶数** ≤ **128** | AC-204 |

---

## 7. 测试策略（门禁口径）

| 层 | 内容 | 判据 |
|---|---|---|
| 纯函数层 | 桶键派生 / 分桶 / 打包拆包 / 桶级时序 / 指纹 / 阶段解析 | 逐条断言（spec AC-201 / AC-203 / AC-205 / AC-208） |
| 线格式层 | 推送体构造 / 拉取响应解析（含「桶响应喂老解析器必须失败」） | AC-207 |
| 状态文件层 | v2 写出 / v1 读入 / 两套编号空间互不参照 | AC-210 / AC-211 |
| 编排层 | 分批参数 / 稳态零写 / 降级 / 部分恢复 | AC-203 / AC-206 / AC-208（部分恢复**无独立 AC**，见 spec Q4） |
| 端到端层 | 桶推 → 桶拉 → 解包 → 逐条可验 | AC-211 |
| 门禁 | 云端单测工程 `FAIL = 0` 且用例数 ≥ 基线 813；编译 0 错 0 警告；U+FFFD = 0；clang-format 定点 0 漂移 | 宪法 R5 |

---

## 8. 待拍板项（正文中标 `[NEEDS CLARIFICATION]`）

| 编号 | 事项 | 推荐答案 | 后果 |
|---|---|---|---|
| **Q-A** | 单轮预估传输时长未实测（spec Q1） | 在真机轮抽样实测，**在此之前记「未判定」** | 不阻塞实现；阻塞「传输可完成」记为已达成 |
| **Q-B** | 多词典场景下单轮页数会触到 32 页硬上限（spec Q2） | **实测本机常见词典数**，据此确认 32 页硬上限是否够用（硬上限是客户端常量，调整不需服务端改动） | 不阻塞单词典场景；4 词典及以上可能扫不完库存 ⇒ 「库存未证实」闸门压制推送 |
| **Q-C** | 老端点退役是否含服务端下线（spec Q3） | **客户端只做到「不再发」**；服务端下线另立决策记录 + 用户单独授权 | 避免本批越界触碰服务端 |
| **Q-D** | 部分恢复的判定窗口（spec Q4） | 按「本轮扫描完整 + 侧车条目未出现在远端视图」判定 | 扫描不完整时不做判定，避免误判致写放大回归 |

---

## 9. 已知边界（**不属本批**）

1. **每账号口径的写预算护栏 ≠ 整库额度护盾**：多账号累计仍可能打爆全局额度（BR-002 不立项，已拍板维持现状）。
2. **首次导入仍是全量传输**：密文不可压缩 ⇒ 桶化只降低**行写入量**，不降低**出网字节**。
3. **桶自愈粒度变粗**：云端丢桶会连带丢掉该设备该桶全部条目 ⇒ 客户端已做**部分恢复**补偿（D28），但非完整恢复。
4. **老端点读放大随退役才止血**：`last_seen` 刷新等读路径附带写的占比，在阶段 3/4 才下降。
5. **AC-212 未实测**：真实桶包字节数与写入耗时**未抽样** ⇒ 不得宣称「传输已验证」。
