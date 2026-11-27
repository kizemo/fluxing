# ADR 0081 —— 云同步判据④替代口径与 BR-①g 规则 1 v2（**草案 · 未生效 · 待人工批准**）

> **状态：Draft（草案）**。本文件**不修改** ADR 0077，**不修改**判据④原文，
> **不修改**任何验收判据本身。它只登记：(a) 事实核对（A-0）已坐实的结论；
> (b) 判据④**替代口径**的建议（供人工拍板后才落为正式修订）；
> (c) 规则 1 v2（桶空间真实库存证据）与 F3 诊断行的设计决定 —— 这两项是
> **客户端实现层修复**，已在本轮落码并由门禁用例覆盖。
>
> **正式生效条件**：人工审阅本草案后（或明确授权）才把「判据④替代口径」写进
> ADR 0077 / spec 103 的判据文本；在此之前，判据④仍按原文判。

---

## 1. 背景与问题

### 1.1 判据④为什么红

批次 BR-①g 的阶段 1 取证（`docs/relay/stage-1-inventory.md` §2.4）在 2026-11-27
09:28 判出**判据④红**：**两路拉取的条目集合逐条 HLC 一致** —— 不成立。

同轮实测数字（`tmp/relay02/judge4-v3.ps1`，v3 脚本）：

```
d1_sync_items_total_rows=40112  max_id=71652  dup_item_keys=0
bucket_pages=10  buckets=1000  envelopes=92354
item_count_mismatch_buckets=0   dup_keys_across_buckets=0
legacy_keys=40112  bucket_keys=92354
missing_in_bucket=926  hlc_mismatch=39043  extra_in_bucket=53168
http_errors=0
JUDGE4_VERDICT=FAIL
```

三个症状同时出现：`missing_in_bucket=926`（926 条 legacy 条目在桶侧**找不到**）、
`hlc_mismatch=39043`（同一键在两路的 HLC 差得很远）、`extra_in_bucket=53168`
（桶侧多出 5 万余条 legacy 侧没有的条目）。

### 1.2 人工裁决给本环的任务

上游会话（2026-11-27 09:5x，裁决 A/B/C）给本环（relay-02of06，阶段 1 修复轮）的
任务是：**先做事实核对（A-0），不改判据、不动生产**；把「926 条缺口逐条分类」
（B）并入阶段 1 一并做完（全程只读）；并修复客户端侧两个已坐实的实现缺口
（规则 1 死分支、F3 可观测性）。

---

## 2. A-0 事实核对（已坐实 · 盘上代码 + 真机日志）

### 2.1 R1（远端 HLC 沿用）**没有被绕过**

**结论：R1 在桶推路径上成立，不是判据④红的根因。**

代码事实（`WeaselServer/Cloud/CloudSyncService.cpp`）：

- 桶推路径 `PushChangedBuckets` **不分配条目级 HLC**。它直接消费对账循环产出的
  `merged` 条目的 HLC（`merged` 的 HLC 由 R1 / R4 分支决定）。
- 对账循环里：R1 命中（本地条目与远端条目同键）⇒ 沿用**远端** HLC；
  R4 分歧（远端旧于本地）或本地新增 ⇒ 才走 `HlcTick(&clock_, now_ms)` 分配新墙钟 HLC。
- 因此「桶侧 HLC 与本地侧车同 key HLC 不一致」**不可能**由"R1 被绕过"解释。

### 2.2 侧车 HLC 写回语义 = **无条件沿用**

侧车重建（R3）时写回的 HLC 是**无条件沿用**本轮的远端 HLC（远端更旧也照搬）；
`HlcObserve` 只抬高**后续新分配**的时序，不回改已写回的值。

⇒ 这直接解释了 `hlc_mismatch=39043`：**legacy 路径每轮把远端旧 HLC 写回侧车**，
侧车值被 legacy 路径拖旧；而桶路径拿到的是较新的桶级 HLC。两路读到的自然不同。

### 2.3 判据④替代口径在**构造上成立**

既然桶侧条目 HLC 来自「远端条目 HLC（经 R1 沿用）」，而侧车同 key HLC 来自
「R3 无条件沿用远端 HLC 的写回」，那么：

> **判据④替代口径**：对每个在桶侧与本地侧车**同时存在**的键，
> 「桶侧条目 HLC == 本地侧车同 key HLC」应当成立。

这个口径**绕开了 legacy 路径的拖旧效应**，直接检验「桶路径写入的 HLC 与侧车
记录是否一致」—— 也就是切读（阶段 3）真正关心的那件事。

> **口径边界（必须写进判据文本）**：只对**两侧同时存在**的键生效。
> 单侧存在的键（桶侧多出的 53,168 条、legacy 侧多出的 926 条）**不参与**逐条比对，
> 而是各自单独计数（`extra_in_bucket` / `missing_in_bucket`）。

### 2.4 A1 三条替代路线的代价对照（均超红线，本链不做）

| 路线 | 做法 | 代价 | 判定 |
| --- | --- | --- | --- |
| A1-1 | 回退：把桶侧 94,589 条降回 40,112 条 legacy 口径 | 需重写约 **93,278 行** D1 | **超红线**（击穿 8 万行/账号写预算 + D1 免费额度） |
| A1-2 | 让 pull 侧沿用旧 HLC（远端旧值覆盖本地新值） | 破坏 R1「本地胜出」语义 ⇒ 可能回灌覆盖本地新词 | **超红线**（数据正确性） |
| A1-3 | 一次性把 legacy 表整体迁移成桶表 | 单次 ≈ 94,604 条 ⇒ 约 190,000 行写入 | **超红线**（复现 2026-11-23 生产事故） |

⇒ **本链不做任何一条**；只出「替代口径」建议 + 客户端侧实现层修复。

---

## 3. 决策

### D1 —— 判据④**暂不改原文**；本 ADR 只登记**替代口径草案**

判据④原文（「两路拉取的条目集合逐条 HLC 一致」）**保持不变**，直到人工批准本草案。
批准后的建议措辞见 §4.1。

### D2 —— 规则 1 v2：闸门改用**桶空间的真实库存证据**

**旧口径（恒不成立，已坐实）**：

```cpp
bool inventory_complete = remote_complete;              // remote_complete == bucket_scan_complete
...
if (!inventory_complete && BucketHasUnprovenEntry(unproven_keys, entries)) { ... }
```

调用方**只在 `bucket_scan_complete` 为真时**才进入 `PushChangedBuckets`
⇒ `!inventory_complete` **恒假** ⇒ 规则 1 在阶段 1/2 下**从不生效**。

**真机日志实证**（2026-11-27 09:25:18 与 09:39:49）：

```
bucket quota held_unproven=0 held_quota=0 rows_pushed=1928 rows_held=0 quota_rows=2000
```

`held_unproven=0` 恒为 0 ⇒ 规则 1 **从未压制过任何桶**。

**新口径**：桶空间的真实库存证据 = 「本轮拉到该桶、但**远端视图不完整**」：

- 客户端解出的条数 **!=** 服务端声明的 `item_count`；
- 或 base64 / 解包失败；
- 或桶内出现畸形条目 / 解不开条目。

⇒ 该桶键进 `incomplete_buckets` ⇒ **整桶压制**（`held_unproven++`、不写 `brow`、
下一轮自然续推）。

**为什么证据必须取自桶空间**（风险方向决定）：

- 服务端按 `bucket_id` **先 DELETE 后 INSERT** ⇒ 未列出的远端条目会被**静默删除**
  ⇒ 风险方向是「**服务器有、我们没读到**」。
- legacy 空间的 `unproven_keys` 方向相反（「**我们有、服务器可能没有**」），
  且两套编号空间**互不参照**（legacy 文本键空间 vs 桶 64 位 hex 键空间）
  ⇒ 混用会把闸门指向错误的风险方向。

**落点**：判定收敛到**生产头**单一 inline 函数 `BucketRemoteViewIncomplete()`
（`WeaselServer/Cloud/CloudSyncService.h`），生产与单测**共用同一份实现**
（宪法 R7：不复制第二份判定逻辑）。

### D3 —— `unproven_keys` **保留**于 legacy 编号空间，桶路径不再消费

`unproven_keys` 仍在 legacy 路径的 R2 延后语义与既有诊断口径里使用 ⇒ 保留不动，
但**加注释标明**「桶路径不再消费」⇒ 避免后续维护者误以为桶推受它保护，
也避免误删导致 legacy 路径回归。

### D4 —— F3：桶端点 HTTP 状态**无条件**诊断行

- `PullBuckets` 增加 `int* http_status` 出参（传输层失败 ⇒ `0`；HTTP 错误 ⇒ 服务端状态码）。
- 桶拉取：每页**成功或失败**各打一条
  `bucket pull page=<n> http=<code> buckets=<n> items=<n> has_more=<0|1>`。
- 桶推送：每批打一条
  `bucket push page=<n> http=<code> buckets=<n> accepted=<n> skipped=<n>`。
- 前缀与既有 `bucket phase=` / `bucket quota ` **不冲突** ⇒ AC-09 探针 v4 的
  `bucket phase=` 前缀正则不破。

**为什么必要**：此前桶端点的状态码**完全不可观测** ⇒ 503（额度）与 401（令牌）
在诊断日志里无法区分 —— 这是批次 BR-①f 判据链的取证缺口（§F2）。

### D5 —— **规则 1 v2 的残留缺口（必须留档，不得声称已根治）**

本轮拉到 + 远端视图完整的桶 **不代表**「该桶的全部远端条目都被我们读到」：

- 阶段 1/2 的桶拉取是**增量**（`since=bucket_seq_`），不是全量扫描；
- 本轮**没拉到**的桶（游标未覆盖）缺少当轮证据 ⇒ 当前方案**只**对
  「拉到但视图不完整」的桶压制。

**明确禁止的偷懒做法**：不得把 `inventory_complete` 改成 legacy 库存完整性
（`!has_more` 在阶段 1/2 常态为 true ⇒ 会压制几乎所有桶 ⇒ 桶推彻底停摆）。

**残留缺口的收口路径**（需独立批次 + ADR + 门禁用例，本链不做）：
让「桶的远端视图完整性」成为**可跨轮累积**的事实（例如把每桶已见条目键集合
或桶级校验和落进侧车 `brow` 行），使「未拉到但已被前轮证实完整」的桶也能被判定。

---

## 4. 判据④替代口径（**草案措辞**，待人工批准）

### 4.1 建议措辞

> **判据④（替代口径 · 草案）**：对**桶侧与本地侧车同时存在**的每一个条目键，
> 「桶侧条目 HLC」必须等于「本地侧车同 key 的 HLC」；
> 单侧存在的键**不参与**逐条比对，改为**分别计数**
> （`extra_in_bucket` = 桶侧多出的键数、`missing_in_bucket` = legacy 侧多出的键数）。
> 任一「两侧都存在」的键出现 HLC 不等 ⇒ 判红。

### 4.2 鉴别力用例（**判据必须有鉴别力**）

| 用例 | 构造 | 期望 |
| --- | --- | --- |
| 好样本 | 桶侧条目 HLC == 侧车同 key HLC | **绿**：不等计数 = 0 |
| 坏样本（桶侧旧于侧车） | 桶侧 HLC 比侧车小 1 ms | **红**：不等计数 ≥ 1 |
| 坏样本（桶侧新于侧车） | 桶侧 HLC 比侧车大 1 ms | **红**：不等计数 ≥ 1 |

> **鉴别力自证要求**：好样本与坏样本**各跑一次**；只跑好样本得到绿，**不构成**
> 判据生效的证据（0 命中不能当生效证据）。

### 4.3 本轮替代口径的**对照数字**

2026-11-27 10:48 的只读复跑（`reports/fluxing-br-1g/phase1-legacy-gap-classify.ps1`
第 2 轮，产物 `tmp/relay02/gap-summary.txt`）：

```
legacy_keys=40112        bucket_keys=94589
legacy_vs_sidecar_equal=149
legacy_vs_sidecar_legacy_newer=0
legacy_vs_sidecar_legacy_older=39962
legacy_vs_sidecar_unmapped=1
bucket_vs_sidecar_equal=93279
bucket_vs_sidecar_bucket_newer=0
bucket_vs_sidecar_bucket_older=0
bucket_vs_sidecar_unmapped=1310
```

读法：

- **桶侧 vs 侧车**：`equal=93,279`、`newer=0`、`older=0` ⇒ **两侧都存在的 93,279 个键
  全部逐字相等** ⇒ 替代口径在 2026-11-27 10:48 **成立**。
- `bucket_vs_sidecar_unmapped=1310` = 桶侧有、**本地侧车没有**的键（1,310 条）
  ⇒ 单侧存在 ⇒ 按 §4.1 **不参与**逐条比对，单独计数。
- **legacy 侧 vs 侧车**：`legacy_older=39,962` ⇒ legacy 路径把侧车 HLC **拖旧**
  ⇒ 正是 §2.2 所述的写放大机制，**不是** R1 被绕过。
- `map_variant=unescaped` ⇒ 云端 `item_key` 是**未转义**口径的 HMAC 身份键
  （`legacy_mapped_unescaped=40111` / `legacy_mapped_raw=0`）⇒ HMAC 映射成立。

### 4.4 **926 条缺口在 10:48 已收敛到 1 条**（B 定性结果）

同日 10:48 的同一次只读复跑给出：

```
gap_total=1  catA_local_known=0  catB_local_unknown=1  catC_unmappable=0
```

逐条分类表见 `docs/relay/stage-1-legacy-gap-classification.md`。

**观测事实**：09:28 判据④ 的 `missing_in_bucket=926`（桶侧 92,354 键）到 10:48 变成
`gap_total=1`（桶侧已扩到 94,589 键）；同时 `bucket_vs_sidecar_unmapped=1310` 与真机
日志的 `deferred=1310` 逐字吻合。

**归因边界（未坐实，不得当结论）**：桶侧键数从 92,354 增至 94,589 与客户端自身
在 09:25 / 09:41 两轮的桶推送（`pushed=896`、`pushed=84`）同期发生，**时间上相关**；
但「legacy 键进入桶侧」的**具体机制**（本地词典是否真的持有那 926 条对应条目、
以及推送覆盖顺序）**本轮未逐条坐实**。⇒ 登记为**待坐实**，不得写成结论。

### 4.5 口径选择的连带影响（登记）

替代口径**只**改变判据④的判定方式，**不**放松下列既有约束：

- 阶段 3（切读）仍受「云端历史 legacy 条目须先完成桶化覆盖」的硬前置约束
  （ADR 0077 §D / handoff §O-1.1）—— 本轮**未**放松，阶段 3 **仍禁**。
- 规则 1 v2 是**实现层修复**，不改变「桶级 HLC 必须严格大于已存值」的服务端守卫语义。
- 单侧键的 1,310 / 926 条计数仍必须**逐轮记录**，供阶段 2 复核轮重判。

---

## 5. 影响面

| 对象 | 变化 |
| --- | --- |
| `WeaselServer/Cloud/CloudSyncService.h` | 新增 `FormatBucketPullDiagLine` / `FormatBucketPushDiagLine` / `BucketRemoteViewIncomplete`（生产头 inline，生产与单测同源）；`PullBuckets` 增 `http_status` 出参；`PullBucketsIntoRemote` 增 `incomplete_buckets` 出参；`PushChangedBuckets` 形参 `unproven_keys` → `incomplete_buckets` |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 规则 1 v2 闸门替换；桶拉/桶推诊断行；`unproven_keys` 加「桶路径不再消费」注释 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 新增规则 1 v2 鉴别力门禁用例（好/坏样本各跑一次，走生产决策路径） |
| ADR 0077 / spec 103 判据文本 | **未改**（本 ADR 只是草案） |
| 服务端 `cloud/fluxing-sync/**` | **未改**（本轮零生产写） |
| 生产 D1 / R2 / 用户词典数据 | **未改**（本轮只读） |

---

## 6. 验收判据（本轮）

1. 编译：`test/TestFluxingCloud` 单独 msbuild（显式 `/p:SolutionDir=`）**通过**。
2. 门禁：`reports/fluxing-br-1g/gate-p0-run.ps1` ⇒ `PASS=<n>`（基线 951，本轮 +n）、
   `FAIL=0`、`UFFFD_COUNT=0`、`CLANGFORMAT_HUNK_DRIFT=0`。
3. 鉴别力：规则 1 v2 门禁用例**好样本绿 / 坏样本红**各跑一次，断言落在生产可观测
   输出（`last_bucket_stats()` 的 `pushed` / `held_unproven` + 假传输收到的桶推送请求体）。
4. 零生产写：本轮**未**对生产 D1/R2 执行任何写操作（只 SELECT）。

## 7. 未闭环 / 留档

- 判据④替代口径**未生效**（等人工批准 ⇒ 才写进 ADR 0077 / spec 103）。
- §D5 的规则 1 残留缺口（跨轮累积的桶视图完整性证据）**未修**，需独立批次。
- §4.4 中「legacy 键进入桶侧」的具体机制**未坐实**。
- 阶段 3（切读）**仍禁**；`AC-09` **仍 FAIL（不得改记）**；`AC-212` / `AC-215②` **未判定**；
  `AC-213` / `AC-214` **永久退役**。
