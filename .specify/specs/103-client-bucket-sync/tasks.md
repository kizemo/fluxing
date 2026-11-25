# tasks · 规格 103 · 客户端桶化同步（批次 BR-①g）

> **本文件只覆盖客户端（批次 BR-①g）**；服务端（批次 BR-①f）已完成并部署生产，其任务清单见
> [spec 102 tasks.md](../102-bucket-level-sync-storage/tasks.md)。
> 体例沿用 spec 101 / 102 的 `tasks.md`：硬前置 → 任务索引表（每条带优先级 `[P1]/[P2]` 与用户故事 `[US#]`）
> → 逐任务「目标 / 改动点 / 硬闸 / 验收判据 / 禁止 / 状态」→ 收尾硬闸 → 执行状态 → 与 spec 102 §8 的对应关系。
> **宪法 R5：单任务 ≤ 4 小时、≤ 3 个生产文件**（测试文件另计）。每条任务的「状态」行开工时如实改写。

> **⚠ 2026-11-25 实盘校正（执行前必读）**
> 工作树**已存在**部分客户端桶化实现（属**另一写入者**，本批文档会话未触碰其代码）：
> 新增 `WeaselServer/Cloud/CloudBucketSync.h`（217 行）/ `CloudBucketSync.cpp`（216 行）。
> - `.h` 已声明：桶键派生 / 分桶 / 打包拆包 / 桶级 HLC / 桶内容指纹 / **五阶段枚举 + 阶段解析** / **桶级线格式**；
> - `.cpp` **只实现**其中 8 个纯函数（`DeriveBucketMacKey` / `ComputeBucketShard` / `ComputeBucketKeyId` /
>   `PackBucketBlob` / `UnpackBucketBlob` / `AggregateBucketHlc` / `BucketFingerprint` / `MakeHlcStrictlyGreater`），
>   **阶段解析、生效解析、推送体构造、拉取响应解析 4 个函数在 `.cpp` 中无实现**（grep 无命中）；
> - 该文件**未入版本控制、未进 `WeaselServer.vcxproj`**（`Select-String 'CloudBucketSync'` 无命中）⇒ **未编译验证**。
> ⇒ T2/T3/T4 的性质由「从零新建」改为「**核对并补齐既有实现**」，**禁止**并行新建第二套；
>   T5/T6/T7/T8/T9/T10/T11/T12 仍为**从零新建**。

---

## 1. 硬前置（开工前逐条确认，任一不成立不得动手）

| # | 前置 | 已确认依据 | 状态 |
|---|---|---|---|
| 1 | ADR 0077 客户端段（D12–D28）已定稿 | `docs/adr/0077-bucket-level-sync-storage-architecture.md`（Status=Accepted；客户端段 D12–D28；「转 Accepted 不代表客户端功能已验证」） | ✅ |
| 2 | 服务端桶级端点已上线生产且冒烟通过 | 交接文档 §K STEP 4/5（生产 Version `4f163326-f2a5-4c69-be56-24cb800e6915`，桶端点冒烟 9/9 PASS） | ✅ |
| 3 | 客户端单测基线为绿：`PASS=813 FAIL=0`（BR-①c 第二轮） | `test/TestFluxingCloud/TestFluxingCloud.cpp` 既有基线 | ⚠ **开工第一动作：实测复现并留档** |
| 4 | 单独 msbuild 命令已验证可用 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe` + `/p:SolutionDir=F:\soft\00selfmade\rime_claude\` | ⚠ **本机 `dotnet` 不可用** |
| 5 | `weasel.sln` **不含** `TestFluxingCloud` 工程 ⇒ 必须单独 msbuild | 既有批次反复踩坑：不带 `/p:SolutionDir=` 会跑到上一轮旧 exe，`PASS` 恒等于旧基线（**假绿**） | ✅ |
| 6 | `CloudBucketSync.cpp` 需登记进**两个** vcxproj（`WeaselServer.vcxproj` + 单测工程），新 `.cpp` 需 `PrecompiledHeader=NotUsing` | 实盘 `WeaselServer.vcxproj` 中 `Select-String 'CloudBucketSync'` 无命中 | ⚠ T14 |
| 7 | 服务端契约常量已逐字核对 | `MAX_PUSH_BUCKETS=64`（`src/wire.ts:243`）/ `MAX_PULL_LIMIT=100`、`DEFAULT_PULL_LIMIT=50`（`src/sync-buckets.ts:43`）/ `MAX_BUCKET_ITEMS=4096`（`src/wire.ts:246`）/ `MAX_BUCKET_BLOB_BYTES=4 MiB`（`src/wire.ts:256`）/ `MAX_BUCKET_KEY_BYTES=512`（`src/wire.ts:233`） | ✅ |
| 8 | 客户端既有分页 / 分批常量已逐字核对 | `kPullPageLimit=100`（`CloudSyncService.cpp:877`）/ `kMaxPullPages=10`（`:879`）/ `kServerMaxPushItems=64`（`CloudSyncService.h:59`）/ `kPushBatchSize=32`（`:65`） | ✅ |
| 9 | 条目级解析器**不可**用于桶响应 | `ParsePullResponse`（`CloudSyncService.cpp:193`）对每个条目强制取 `nonce`（`:264`，缺失即报错）⇒ 桶对象**无 `nonce`** ⇒ 必失败（AC-207 / AC-108） | ✅ |
| 10 | 语义指纹口径与条目级侧车同源 | `SemanticHashHex`（`CloudSyncService.cpp:1657` / `.h:254`）/ 侧车 `payload_hash` 存 `s2:` 口径（BR-①c 第二轮） | ✅ |
| 11 | 侧车 v1 现状已核对（升版不得改其语义） | `BuildSidecarText`（`.cpp:1668`，写 `#fluxing-cloud-sync-v1` + `seq` + `keyfp` + `row`）/ `ParseSidecarText`（`.cpp:1696`）/ `ResetCloudSyncSidecarFile`（`.cpp:1760`） | ✅ |
| 12 | 侧车实际路径与诊断日志路径已核对 | 侧车 `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state`（§K-3）；诊断日志 `%TEMP%\fluxing-profile-notify.log` | ✅ |

---

## 2. 任务索引表

| # | 优先级 | 用户故事 | 任务 | 主要文件 | 估时 |
|---|---|---|---|---|---|
| T1 | P1 | US-G | 硬前置核对与基线留档（单独 msbuild 通路 + `PASS=813 FAIL=0` 复现 + 既有符号逐字核对） | 无（只读核对） | 1.5 h |
| T2 | P1 | US-A | 桶键派生与分桶（加密域 HMAC + 独立 HKDF 标签 + 64 × 16 = 1024 桶） | `CloudBucketSync.h/.cpp` | 3 h |
| T3 | P1 | US-A / US-B | 桶包打包 / 拆包（既有 8 行信封块串接，**不新增**桶级 nonce / AAD） | `CloudBucketSync.h/.cpp` | 3 h |
| T4 | P1 | US-A | 桶级 HLC（桶内取最大 + 严格大于已存）与桶内容指纹（语义口径） | `CloudBucketSync.h/.cpp` | 2.5 h |
| T5 | P1 | US-D | 五阶段状态机（枚举 / 解析 / 生效口径：环境变量优先 → 侧车 → 默认 0） | `CloudBucketSync.h/.cpp` | 3 h |
| T6 | P1 | US-B / US-G | 桶级线格式：推送体构造 + **独立**拉取响应解析 | `CloudBucketSync.h/.cpp` | 3.5 h |
| T7 | P1 | US-E / US-F | 侧车 v2 读写（**另起新函数**、必须能读 v1、`bseq` 独立从 0 起） | `CloudSyncService.h/.cpp` | 3 h |
| T8 | P1 | US-B | 桶级拉取编排接入（`bseq` 分页 + 动态页数上限 + 解包复用既有解密路径） | `CloudSyncService.h/.cpp` | 3 h |
| T9 | P1 | US-A / US-C | 桶级推送编排接入（分组 → 指纹比对 → 分批 ≤ 64 桶 → 只推变化桶） | `CloudSyncService.h/.cpp` | 4 h |
| T10 | P1 | US-D / US-F | 阶段分流接线（阶段 0 条目级路径**逐字不动**；阶段 ≥1 才分流） | `CloudSyncService.h/.cpp` | 3 h |
| T11 | P1 | US-A | 超限降级（单桶 `item_count > 4096` 或 blob > 4 MiB ⇒ 该桶走条目级端点） | `CloudSyncService.cpp` | 2 h |
| T12 | P1 | US-B | 切读阶段的部分恢复（远端视图缺失 ⇒ 所在桶强制重推） | `CloudSyncService.cpp` | 2.5 h |
| T13 | P1 | US-G | 桶化单测（分桶确定性 / 密文逐字节相等 / 阶段 0 等价 / 桶响应喂老解析器必失败 …） | `test/TestFluxingCloud/TestFluxingCloud.cpp` | 4 h |
| T14 | P1 | US-G | 构建登记 + 门禁（两个 vcxproj 登记 `CloudBucketSync.cpp` + 单独 msbuild + `PASS/FAIL` + U+FFFD + clang-format） | 两个 `.vcxproj` + 验证脚本 | 2.5 h |
| T15 | P2 | US-E | 文档（ADR 0077 客户端段 + spec 103 三件套 + handoff §M + 接力 prompt + CLAUDE.md 指针） | `docs/**` + `.specify/**` | 2 h |
| T16 | P2 | US-H | 出包装机与真机判据（阶段切换探针 + 桶端点真机轮 + AC-212 回填） | 打包流程 + 探针脚本（不入库） | 4 h |

**合计 16 条任务**；P1 = 14 条，P2 = 2 条；估时合计约 **46.5 小时**（P1 ≈ 40.5 小时）⇒ 需跨多轮次会话执行；
每完成一阶段须按 `AGENTS.md` §9 同步更新 handoff 与接力 prompt。

---

## 3. P1 —— 纯函数层（T2…T6）

### T1 · 硬前置核对与基线留档 `[P1] [US-G]`
- **目标**：把「开工前必须先复现基线」变成纪律，避免在旧基线上写新断言。
- **改动点**：**只读核对**，不产出产品代码；核对结果写进 handoff §M 的证据路径。
- **硬闸**：① 单独 msbuild 必须显式带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`，否则跑上一轮旧 exe；
  ② 基线数字必须是**本轮实测**（`PASS` / `FAIL` 计数 + 用例总数），不得抄录既往批次；
  ③ `CloudBucketSync.h/.cpp` 的既有符号名与常量值必须与本文档**逐字一致**；
  ④ `WeaselServer.vcxproj` 中 `CloudBucketSync` 无登记 ⇒ 记为待办（归 T14）。
- **验收判据**：产出「基线 `PASS=` / `FAIL=` / 用例总数 + msbuild 命令全文 + 既有符号清单」三行可粘贴记录；
  用例总数与 `PASS=813 FAIL=0` 不符时**以实测为准**并登记差异。
- **禁止**：把「编译过」当「门禁绿」；改任何产品文件。
- **状态**：进行中（基线与符号核对已部分完成，见 handoff §M.7 证据路径；基线复现由代码子会话执行）。

### T2 · 桶键派生与分桶 `[P1] [US-A]`
- **目标**：条目 → 桶的**确定性**归属（ADR 0077 D14 / D15 / D23）。
- **改动点**：`CloudBucketSync.h/.cpp` 的 `DeriveBucketMacKey` / `ComputeBucketShard` / `ComputeBucketKeyId`
  与常量 `kBucketShardCount=64` / `kBucketSubShardCount=16` / `kBucketCount=1024` / `kBucketKeyHexLength=64`。
- **硬闸**：① **明文 `sha256(item_key)` 派生一律不得使用**（可被字典穷举反推「哪个词条落在哪个桶」）；
  ② 桶键用**独立 HKDF 标签** `fluxing-bucketkey-mac-v1`（**不得**复用 `fluxing-itemkey-mac-v1`）；
  ③ 桶键输出 **64 位小写十六进制**；桶键输入带词典名长度前缀消歧；
  ④ 一级 64 × 二级 16 = **1024 桶 / 词典**（`64` 是**分片数**，**不是**每桶条目数上限）；
  ⑤ 同一条目在任何机器、任何轮次落同一桶（不得引入设备 ID / 时间 / 随机源）。
- **验收判据**：分桶函数对固定输入两次调用**逐字节相等**；单条目变更后受影响桶数 = **1**（AC-201）；
  桶键不含任何明文词条片段。
- **禁止**：为「省一次 MAC」把两级分片合成一次；把桶键退化成明文摘要。
- **状态**：进行中（`.cpp` 已落盘，含桶键派生与分桶的 8 个纯函数之一，**未编译验证**；口径以本条为准逐字核对）。

### T3 · 桶包打包 / 拆包 `[P1] [US-A] [US-B]`
- **目标**：桶包 = N 个**既有条目级密文块直接串接**（ADR 0077 D19）。
- **改动点**：`CloudBucketSync.h/.cpp` 的 `PackBucketBlob` / `UnpackBucketBlob`。
- **硬闸**：① **不得**引入桶级随机串 / 桶级附注 / 桶级二次加密（ADR 0077 D3 已否决）；
  ② 条目级信封 8 行文本**逐字不变**；③ 拆包必须**逐块**还原（不得按行切分猜边界）；
  ④ 桶键 / 条目键的长度上限校验复用既有常量（`kBucketMaxKeyBytes=512`）。
- **验收判据**：同一批条目分别走条目级推送与桶推送后，**密文块逐字节相等**（逐条比对，不是只比条数）（AC-202）。
- **禁止**：新增桶级 AAD；改动 `include/FluxingCloudEnvelope.h`（**零改动**）。
- **状态**：进行中（`.cpp` 已落盘，**未编译验证**）。

### T4 · 桶级 HLC 与桶内容指纹 `[P1] [US-A]`
- **目标**：桶级时序可越过服务端守卫；桶内容「没变」可判定（ADR 0077 D16 / D17）。
- **改动点**：`CloudBucketSync.h/.cpp` 的 `AggregateBucketHlc` / `MakeHlcStrictlyGreater` / `BucketFingerprint`。
- **硬闸**：① 桶级 HLC = 桶内条目 HLC **最大值**；② 上报值必须**严格大于**侧车已存桶 HLC
  （否则真实变更被服务端守卫整桶 `skipped`）；③ 桶指纹必须是**语义口径**（`s2:` / `SemanticHashHex` 同源），
  **不得**用密文哈希或整条 payload 哈希（否则 rime 每轮回写重算 `d=`/`t=` ⇒ 每轮重推）；④ 不得新增第二套指纹算法。
- **验收判据**：同桶同内容再推 ⇒ 服务端**整桶跳过**（零写入）；桶内容变化但条目 HLC 未变大 ⇒ 上报值 **>** 已存值（AC-205）。
- **禁止**：把桶指纹做成密文哈希；让桶 HLC 小于或等于桶内任一条目 HLC。
- **状态**：进行中（`.cpp` 已落盘，**未编译验证**）。

### T5 · 五阶段状态机 `[P1] [US-D]`
- **目标**：灰度可推进、可回退、可审计（ADR 0077 D12 / 四段退役路线）。
- **改动点**：`CloudBucketSync.h/.cpp` 的 `enum class CloudBucketPhase { kLegacy=0, kDualWrite=1, kVerify=2,
  kSwitchRead=3, kRetired=4 }`、`kBucketPhaseEnvVar = "FLUXING_CLOUD_SYNC_BUCKET_PHASE"`、
  `BucketPhaseValue` / `ParseBucketPhaseValue` / `ResolveBucketPhase`。
- **硬闸**：① 生效口径 = **环境变量优先 → 侧车 `phase` → 默认 0**；非法值回退侧车，**不得**抛异常中断同步；
  ② 环境变量生效时**写回侧车**（使 UI 与下一次不带变量的启动保持一致）；③ **阶段不得自动递增**
  （每次跃迁必须有人工判据）；④ **回滚 = 设环境变量为 0 跑一轮**（不改代码、不重装、不删数据）；
  ⑤ 阶段值必须可打印为**简体中文**可读文本（用户可见）。
- **验收判据**：设环境变量为 0 ⇒ 该轮桶端点请求数 = **0**、条目级路径行为与本批之前**逐字等价**（AC-208 / AC-210）。
- **禁止**：在客户端内部「跑完一轮就自动进下一阶段」；用非 0 值作为默认值。
- **状态**：**待开工**（`ParseBucketPhaseValue` / `ResolveBucketPhase` 在 `.cpp` 中**无实现**）。

### T6 · 桶级线格式：推送体构造 + 独立拉取响应解析 `[P1] [US-B] [US-G]`
- **目标**：桶级请求 / 响应按桶契约成型，**不污染**条目级协议（ADR 0077 D26 / AC-108）。
- **改动点**：`CloudBucketSync.h/.cpp` 的 `BuildBucketPushBody` / `ParseBucketPullResponse` /
  `CloudBucketPushItem` / `CloudBucketPullItem`。
- **硬闸**：① 桶响应字段集合 = {桶标识, 设备标识, 桶级时序, 桶密文, 条目数, 服务端序号}，**无随机串（nonce）字段**；
  ② **禁止**复用 `ParsePullResponse`（它对每个条目强制取 `nonce`，桶对象无此字段 ⇒ 必失败）；
  ③ 条目级 `ParsePullResponse` 与其字段判定**逐字不动**；④ 推送体**不带** `device_id`
  （服务端 `resolvePushDeviceId` 以当前登录设备行主键为权威）；⑤ 非法字段一律给**简体中文**错误说明。
- **验收判据**：把桶响应喂给条目级解析器**必须失败**（证明确有独立实现）+ 桶响应可被新解析器正确解析（AC-207）。
- **禁止**：给老解析器加「兼容桶」分支；给桶响应加 `nonce` 占位。
- **状态**：**待开工**（`BuildBucketPushBody` / `ParseBucketPullResponse` 在 `.cpp` 中**无实现**）。

---

## 4. P1 —— 状态文件与编排层（T7…T12）

### T7 · 侧车 v2 读写 `[P1] [US-E] [US-F]`
- **目标**：侧车承载两套编号空间 + 阶段位 + 桶行（ADR 0077 D13 / D25）。
- **改动点**：`CloudSyncService.h/.cpp` —— **另起** v2 侧车构建 / 解析函数；
  `ResetCloudSyncSidecarFile`（`.cpp:1760`）同步把 `phase` / `bseq` 归零。
- **硬闸**：① 头行 `#fluxing-cloud-sync-v2`；保留 v1 的 `seq` / `keyfp` / `row`（语义**逐字不变**）；
  新增 `bseq` / `phase` / `brow`（`bucket_key` / `ms` / `counter` / `device` / `item_count` / `fingerprint`）；
  ② **必须能读 v1**（读到 v1 ⇒ 阶段 = 0、`bseq` = 0、无 `brow`）；③ `bseq` 是**独立编号空间、强制从 0 起**，
  **不得默认沿用** `seq`；④ **`seq` 不归零**（偏离 ADR 0077 D6 —— 四段渐进退役期间老端点仍活跃，重置会致老端点全量重拉重写）；
  ⑤ v1 的 `BuildSidecarText` / `ParseSidecarText` **公开 API 与语义不得破坏**（阶段 0 逐字等价）；
  ⑥ 只有**推送成功**的桶才写 `brow`（失败续传）。
- **验收判据**：读 v1 ⇒ 阶段 = 0、`bseq` = 0、无桶行；写 v2 后再读回字段一致；`bseq` 与 `seq` **互不参照**（AC-210 / AC-211）。
- **禁止**：直接改既有 `BuildSidecarText` / `ParseSidecarText` 的条目级语义；把 `bseq` 与 `seq` 互相赋值。
- **状态**：**待开工**。

### T8 · 桶级拉取编排接入 `[P1] [US-B]`
- **目标**：按桶拉取，解包后并入既有远端集合（ADR 0077 D20 / D27）。
- **改动点**：`CloudSyncService.h/.cpp` 的桶级拉取路径（`bseq` 游标 + 分页）。
- **硬闸**：① 单页桶数 ≤ **100**（服务端 `MAX_PULL_LIMIT`）；② **单轮页数上限 = max(16, 词典数 × 11 + 2)**，
  且**不超过 32 页**（硬上限）—— 与 spec 103 AC-203 **逐字同源**；③ 解包后**复用同一解密路径**
  （`OpenItemsTolerant` 语义：`.cpp:1547` / `.h:218`），**不新写第二套解密实现**；
  ④ `userdb` 类另做形态校验并单独计数 malformed（与条目级同语义）；⑤ 条目级分页常量
  （`kPullPageLimit=100` / `kMaxPullPages=10`）**不动**。
- **验收判据**：单词典时单轮页数 = **16**；多词典时按公式算出页数 **>** 16；词典数 ≥ 3 时触到 **32** 页硬上限；
  拉到的条目并入后与条目级拉取**等价**（AC-203 / AC-211）。
- **禁止**：改条目级分页常量；把桶响应交给 `ParsePullResponse`。
- **状态**：**待开工**。

### T9 · 桶级推送编排接入 `[P1] [US-A] [US-C]`
- **目标**：只推**内容变化**的桶，写入量与词条数解耦（ADR 0077 D18 / D20）。
- **改动点**：`CloudSyncService.h/.cpp` 的桶级推送路径。
- **硬闸**：① 桶内容 = **合并后本地视图的全集**按 (词典名, 分片) 分组后的**整桶**
  （服务端是 `DELETE+INSERT` 覆盖语义 ⇒ 只推变更条目会丢同桶其它条目）；② **只推指纹变化的桶**
  （指纹 = 语义口径）⇒ 稳态零写；③ 单请求 ≤ **64 桶**（服务端 `MAX_PUSH_BUCKETS`）⇒ 1024 桶首导 = 16 个请求；
  ④ 分批失败**不得**使整轮失败，且失败桶**不写 `brow`**（下一轮续推）；⑤ 推送体不带 `device_id`。
- **验收判据**：连续两轮无变更 ⇒ 桶端点推送请求数 = **0**（AC-206）；单请求桶数 ≤ 64；
  写入量两口径分别断言（索引行数 ≈ **1,024**；D1 实际写行数 ≤ **2 × 桶数** = **2,048**）（AC-204）。
- **禁止**：只推「本轮改动」的条目进桶；把桶推成功当桶内容已正确（必须比对指纹）。
- **状态**：**待开工**。

### T10 · 阶段分流接线 `[P1] [US-D] [US-F]`
- **目标**：阶段 0 默认不变；阶段 ≥1 才走桶路径（ADR 0077 D12 / 四段退役路线）。
- **改动点**：`CloudSyncService.h/.cpp` 的 `CloudSyncCoordinator::SyncOnce` 编排分流。
- **硬闸**：① **阶段 0 是默认值**，未开闸时端点调用序列与状态文件语义与本批之前**逐字等价**（AC-210）；
  ② 阶段 1 = 双写（条目级 + 桶级）、阶段 2 = 双写 + 交叉校验、阶段 3 = **切读**（老端点完全不发）、
  阶段 4 = 老端点退役；③ **阶段不得自动递增**；④ 诊断行必须**无条件**打印
  （失败轮也要打），形如 `bucket phase=.. pushed=.. skipped=.. pulled_buckets=.. pulled_items=..`；
  ⑤ 诊断行**不得**含明文词条内容、词典名、令牌（宪法 P2 / P5）。
- **验收判据**：阶段 0 ⇒ 桶端点请求数 = **0** 且条目级既有断言**逐条保留**；阶段 1 ⇒ 两类端点都发；
  阶段 3 ⇒ 条目级端点请求数 = **0**（AC-208 / AC-210）。
- **禁止**：改条目级既有代码路径的**行为**（只能加旁路分流）；把阶段值写死为非 0。
- **状态**：**待开工**。

### T11 · 超限降级 `[P1] [US-A]`
- **目标**：极端桶不拖垮整轮（ADR 0077 D21）。
- **改动点**：`CloudSyncService.cpp` 的降级分支。
- **硬闸**：① 单桶 `item_count > 4096`（`kBucketMaxItems`）或 blob > **4 MiB**（`kBucketMaxBlobBytes`）
  ⇒ **该桶**改走条目级端点推；② 降级**只影响该桶**，其余桶照常推；③ 降级须打**诊断行**计数。
- **验收判据**：构造超限桶 ⇒ 该桶走条目级端点、其余桶走桶端点、整轮**不失败**（FR-214）。
- **禁止**：因为一桶超限就放弃整轮；静默丢弃该桶（**必须**走条目级端点推）。
- **状态**：**待开工**。

### T12 · 切读阶段的部分恢复 `[P1] [US-B]`
- **目标**：桶自愈粒度变粗后的补偿（ADR 0077 D28 / D10 客户端部分）。
- **改动点**：`CloudSyncService.cpp` 的切读阶段比对逻辑。
- **硬闸**：① 判定前提 = **本轮库存扫描完整**（扫描不完整 ⇒ **不做判定**，避免误判致写放大回归）；
  ② 「侧车已同步但远端视图缺失」的条目 ⇒ **所在桶强制重推**（即使桶指纹未变）；
  ③ 强制重推**不得**突破单请求桶数上限；④ 判定结果须体现在诊断行。
- **验收判据**：构造「远端缺 1 桶」⇒ 该桶被强制重推；扫描不完整时**不**触发强制重推（AC-212 之外的 D28 判据）。
- **禁止**：把「远端缺桶」直接当成「本地数据丢失」而清本地；无条件全量重推。
- **状态**：**待开工**。

---

## 5. P1 —— 测试与门禁（T13、T14）

### T13 · 桶化单测 `[P1] [US-G]`
- **目标**：把客户端桶化的每条行为钉进用例（spec 103 AC-201…AC-211）。
- **改动点**：`test/TestFluxingCloud/TestFluxingCloud.cpp`（**新增**用例，既有条目级用例**逐条保留**）。
- **硬闸**：必须覆盖 —— ① 分桶确定性（两次调用逐字节相等）；② 改一条词条只重写 **1** 个桶；
  ③ 条目级推送与桶推送的**密文块逐字节相等**；④ 分批参数（≤64 桶/请求、≤100 桶/页、
  单轮页数 = `max(16, 词典数 × 11 + 2)` 且 ≤ 32）；⑤ 桶 HLC 严格大于已存值；
  ⑥ 稳态零写（连续两轮无变更 ⇒ 桶推送请求数 = 0）；⑦ **桶响应喂条目级解析器必须失败**；
  ⑧ 读 v1 侧车 ⇒ 阶段 0 / `bseq` 0 / 无桶行；⑨ 环境变量为 0 ⇒ 桶端点请求数 = 0；
  ⑩ 桶推 → 桶拉 → 解包 → 逐条可验且条目集合相等。
- **验收判据**：`PASS` 计数 ≥ 基线、`FAIL` = **0**；**既有用例名集合是新集合的子集**。
- **禁止**：删改既有条目级用例；断言里出现默认 `*.workers.dev` 字面量。
- **状态**：**待开工**。

### T14 · 构建登记与门禁 `[P1] [US-G]`
- **目标**：让桶化代码**真正进入构建**并跑出门禁数字。
- **改动点**：`WeaselServer.vcxproj` 与单测工程的 `.vcxproj` 登记 `CloudBucketSync.cpp`（新 `.cpp` 需
  `PrecompiledHeader=NotUsing`）；验收脚本 `scripts/verify/verify-br1g-bucket.ps1`（按红线**不入库**）。
- **硬闸**：① 单独 msbuild **必须**显式带 `/p:SolutionDir=`（否则跑旧 exe ⇒ `PASS` 恒等于旧基线 = **假绿**）；
  ② 门禁输出必须含 `BUILD_EXIT` / `PASS` / `FAIL` / `UFFFD_COUNT` / `VCPROJ_REGISTRATION` / `CLANGFORMAT_DRIFT`；
  ③ U+FFFD 计数 = **0**；④ clang-format 定点 **0 漂移**；⑤ 编译 **0 错 0 警告**；
  ⑥ `git status -sb` + `git diff --stat` 确认改动只落在本批文件清单内（未误入 `weasel.props` / `env.bat` /
  `*.obj` / `release/` / `reports/**`）。
- **验收判据**：两个 vcxproj 均登记 `CloudBucketSync.cpp`；单独 msbuild 退出码 0；用例数较基线**只增不减**且 `FAIL` = 0。
- **禁止**：把 `weasel.sln` 的构建结果当门禁结果（该 sln **不含** `TestFluxingCloud`）；提交构建产物。
- **状态**：**待开工**。

---

## 6. P2 —— 文档与真机（T15、T16）

### T15 · 文档 `[P2] [US-E]`
- **目标**：让本批的每条决定与偏离可追溯，且**不把未实测写成已达成**（宪法 R2）。
- **改动点**：`docs/adr/0077-bucket-level-sync-storage-architecture.md`（客户端段 D12–D28 + 四段退役路线 +
  回滚预案 + 补推分批策略 + 偏离登记）/ `.specify/specs/103-client-bucket-sync/{spec,plan,tasks}.md` /
  `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（§M）/
  `docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` /
  `CLAUDE.md` BR 指针。
- **硬闸**：① **D1–D11 原文逐字不动**（只在必要处补引用）；② ADR 0077 转 `Accepted` **不代表**客户端功能已验证
  （须在 Status 处写明）；③ **AC-212 记「未判定 + 需要什么」**，不得用推算值记 PASS；
  ④ handoff §M 与接力 prompt **不得**写任何本批「已达成 / PASS」结论，未产生实测数字处写 `TODO(主会话回填): <字段名>`；
  ⑤ 接力 prompt **只给全路径指针**，每个路径必须 `Test-Path` 可解析；⑥ `CLAUDE.md` 只写「本批进行中 + 决策要点 + 指针」；
  ⑦ 所有新文件 U+FFFD = **0**、用户可见字符串为简体中文、换行沿用原文件风格。
- **验收判据**：五组文件齐备；路径指针 `Test-Path` **bad = 0**；U+FFFD = 0；ADR Status = `Accepted`。
- **禁止**：在文档里写「客户端已实施 / 已验证」；改写 D1–D11 原文。
- **状态**：进行中（ADR 0077 客户端段、spec 103 三件套、handoff §M、接力 prompt、`CLAUDE.md` 指针已落盘；
  **实测数字待主会话回填**）。

### T16 · 出包装机与真机判据 `[P2] [US-H]`
- **目标**：真机证明桶化端到端可用，并**回填 AC-212**。
- **改动点**：打包流程（`RELEASE_BUILD=1`）+ 装机 + 真机轮次；探针脚本
  `scripts/verify/probe-br1g-bucket-phase.ps1`（设 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` → 重启
  `WeaselServer` → 读 `%TEMP%\fluxing-profile-notify.log`），按红线**不入库**。
- **硬闸**：① release 构建必须 `RELEASE_BUILD=1` in `env.bat`，**禁用** `git describe --tags` 取版本号；
  ② 出包后核对 `WeaselServer.exe` 的 sha256 **构建 = 包内 = 装机后**三者逐字节相等；
  ③ 真机轮必须产出**真实桶 blob** 的 `elapsed_ms` 序列 ⇒ 才能回填 AC-112 的「单轮预估传输时长」
  （**不得**用合成 blob 压测代替）；④ 阶段切换只经**环境变量**，不改代码、不重装；
  ⑤ 探针脚本里的 `Log` / `PROBE_OUTPUT` / `RECONCILE_LINES` **不得**用 `Write-Output`（会污染函数返回流，
  见 handoff §K-4）；⑥ 出包 / 装机 / 真机判据**需用户在场或明确授权**。
- **验收判据**：装机退出码 0；真机轮可产出桶推送请求计数与 `elapsed_ms`；AC-112 的四项数值据实回填
  （未测项写「未判定 + 需要什么」）。
- **禁止**：把「装上了」当判据通过；用推算值回填 AC-212。
- **状态**：**待开工**。

---

## 7. 收尾硬闸（全部任务完成后逐条打勾，缺一不得收批次）

- [ ] 单独 msbuild（带 `/p:SolutionDir=`）退出码 **0**；`PASS` ≥ 基线且 `FAIL` = **0**，既有条目级用例**逐条保留**
- [ ] 两个 `.vcxproj` 均已登记 `CloudBucketSync.cpp`
- [ ] U+FFFD 计数 = **0**；新增用户可见字符串全部为简体中文
- [ ] clang-format 定点 **0 漂移**
- [ ] `git status -sb` + `git diff --stat`：改动仅落在 plan.md §5.5 文件清单内；**未**误入 `weasel.props` /
      `env.bat` / `*.obj` / `release/` / `reports/**` / `librime`（子模块）
- [ ] **未**执行 `git add` / `git commit` / `git push`（入库需用户授权）
- [ ] `git diff -- include/FluxingCloudEnvelope.h` 为**空**（条目级附注逐字不变）
- [ ] `git diff -- cloud/fluxing-sync/**` 为**空**（服务端本批不改）
- [ ] ADR 0077 的 D1–D11 原文**逐字未改**
- [ ] 接力 prompt 内所有路径 `Test-Path` = **True**（bad = 0）
- [ ] handoff §M 无「已达成 / PASS」类结论；未实测处均为 `TODO(主会话回填): <字段名>`
- [ ] AC-212 的「单轮预估传输时长」已据实回填（未测则写「未判定 + 需要什么」）

## 8. 本批（spec 103 创建轮）的执行状态

- **本轮未写任何产品代码**：仅落盘文档工件（ADR 0077 客户端段 / spec 103 三件套 / handoff §M / 接力 prompt /
  `CLAUDE.md` 指针）；**未执行**门禁、**未**出包、**未**装机、**未**部署、**未**入库推送。
- 客户端桶化实现由**另一写入者**推进（`CloudBucketSync.{h,cpp}` 已落盘、未编译、未入 git）；
  本批任务表中客户端实施项状态为 `进行中` / `待开工`，**均未标 `done`**。
- 估时合计约 **46.5 小时**（T1…T16），P1 约 40.5 小时 ⇒ 需跨多轮次会话执行；
  每完成一阶段须按 `AGENTS.md` §9 同步更新 handoff 与接力 prompt。

## 9. 与 spec 102 §8「本批不做什么（属 BR-①g 客户端批次）」的对应关系

| # | spec 102 §8 条目 | 本批落点 | 状态 |
|---|---|---|---|
| 1 | 客户端分桶打包 / 拆包、桶级 HLC 分配、`bucket_key` 计算 | **T2 / T3 / T4** | 进行中（`.cpp` 已落盘，未编译验证） |
| 2 | 侧车 v2 升版与 `seq = 0` 强制重置 | **T7**（**偏离**：强制重置落在新增 `bseq`，老 `seq` 不归零，见 ADR 0077 D25） | 待开工 |
| 3 | 桶内条目上限 4096 与「超限降级老端点」 | **T11** | 待开工 |
| 4 | 桶大小 / 分片数取值调参与 R2 吞吐实测 | 取值 = **T2**（64 × 16 = 1024 桶）；吞吐实测 = **T16**（真机轮回填 AC-212） | 取值进行中 / 实测待开工 |
| 5 | `healed` 自愈粒度变粗的部分恢复方案 | **T12** | 待开工 |
| 6 | 出包装机与真机判据 | **T16**（AC-09 复跑仍由既有定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` 负责，本批不改判据） | 待开工 |
| 7 | `sync_items` 表与老端点的退役（**四段 + 回滚预案，只做切读就下老端点 = 不合格**） | **T5 / T10** 交付阶段状态机与回滚路径；四段路线 = 双写 → 校验 → 切读 → 退役（ADR 0077「四段退役路线 + 回滚预案」）；**服务端端点下线另立决策记录并需用户单独授权**（spec Q3） | 待开工 |
| 8 | `devices.last_seen` 读路径写占比上升的治理 | **不属本批**（登记为已知边界；阶段 3/4 才下降，见 plan.md §9 第 4 条） | 不做 |
| 9 | 写预算护栏改「全局口径 / 按活跃账号分摊」 | **不改实现**（已拍板维持每账号口径；BR-② 不立项，仅作已知边界登记） | 不做 |
| 10 | 是否升级 Workers Paid | **不升级**（已拍板暂不升级）；**开工前须先定补推分批策略** —— 已定：轮内分批（≤ 64 桶/请求）+ 失败续传，**不需要分日 pacing**（ADR 0077 D20 / D22） | 已定 |

> **与服务端批次的关系**：服务端（BR-①f）已部署生产；本批（BR-①g）**只改客户端**，`cloud/fluxing-sync/**` 零改动。
> **AC-09 仍为 FAIL**（既有判据未改记 PASS），本批不触碰其判据。
