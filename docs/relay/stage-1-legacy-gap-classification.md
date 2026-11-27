# 阶段 1 · BR-①g「926 条缺口」逐条分类表（B 定性 · 只读）

> 批次：BR-①g 客户端桶化 · 接力链 relay-02of06 阶段 1 修复轮
> 取数时间：2026-11-27 **10:43:46 — 10:48:12**（本地时钟）
> 取数脚本：`reports/fluxing-br-1g/phase1-legacy-gap-classify.ps1`（第 2 轮，只读）
> 产物：`tmp/relay02/gap-classified.tsv`、`tmp/relay02/gap-summary.txt`（**均不入库**）
> **零生产写**：生产 D1 只 `SELECT`；侧车只读；本地词典只读。
> **全文不含任何凭据值**（只记长度与 sha16 前 16 位）。

---

## 0. 结论速览（置顶）

**09:28 判据④ 取到的 926 条缺口，到 10:48 已收敛为 1 条。**

| 项 | 09:28（判据④ v3） | 10:48（本轮 B 定性） |
|---|---|---|
| legacy 键数 | 40,112 | 40,112 |
| 桶侧键数 | 92,354 | **94,589** |
| legacy 键**不在桶侧**（= 缺口） | **926** | **1** |
| 桶侧键**不在 legacy 侧**（extra） | 53,168 | 54,477（派生） |

**分类结果**（10:48）：

| 类别 | 含义 | 条数 |
|---|---|---|
| `A_local_known` | 云端 legacy 键能映射到**本地侧车**的文本键 ⇒ 本地已知 | **0** |
| `B_local_unknown` | 64 位小写 hex 身份键、**本地侧车查不到** ⇒ 本地确实没有（疑似本地已删的僵尸行） | **1** |
| `C_unmappable` | 非 hex（墓碑 / 明文词典键）⇒ 与身份键空间不同，**不可判定** | **0** |

**剩余 1 条明细**（`tmp/relay02/gap-classified.tsv` 全量，仅 1 行）：

| item_key | legacy_hlc | category | text_key | sidecar_hlc |
|---|---|---|---|---|
| `cd3cc4e9afb595db902b8eefcdb52e2c5edcd470a4075514c34b7428eada865f` | `1794411631262:0:a651f3d8fcb3ab8a` | `B_local_unknown` | （空） | （空） |

---

## 1. 分类口径（判别规则）

对**每一条云端 legacy `item_key`**（D1 `sync_items` 表）判定：

1. **命中侧车映射** ⇒ `A_local_known`
   - 映射构造：侧车 93,279 条 `row` 行的**文本键**（形如 `dict\tphrase`）⇒ 经
     `HMAC-SHA256(HKDF-SHA256(enc_key, salt=zeros32, info="fluxing-itemkey-mac-v1"), 文本键)`
     得 64 位小写 hex ⇒ 与云端 `item_key` 同空间。
   - **转义变体**：脚本同时构造 raw 与 unescaped 两个变体，取映射命中率高者。
     本轮实测 `legacy_mapped_raw=0` / `legacy_mapped_unescaped=40111`
     ⇒ 选中 **unescaped**（`map_variant=unescaped`）⇒ 云端 `item_key` 是**未转义**口径。
2. **未命中 且 `item_key` 是 64 位小写 hex** ⇒ `B_local_unknown`
   - 理由：hex 形态说明它**就是**身份键空间里的键，而本地侧车（本地已知集合的代理）
     里没有 ⇒ 本地确实没有这条 ⇒ 疑似**本地已删但云端残留**的僵尸行。
3. **未命中 且 非 hex** ⇒ `C_unmappable`
   - 墓碑（删除条目）的 `item_key` 是**词典明文**，与 64 位 hex 身份键**不同属同一空间**
     ⇒ 无法用 HMAC 映射判定 ⇒ **不可判定**，不得硬塞进 A 或 B。

> **本地已知集合的代理**：以侧车 93,279 条 `row` 行为准（侧车 = 本地已同步条目的
> 权威本地记录）。侧车路径：
> `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state`
> （12,062,935 B / 94,308 行）。标签直方图：`row=93279, brow=1024, seq=1, bseq=1, phase=1, keyfp=1`。

---

## 2. 全量对照数字（盘上回读 · `tmp/relay02/gap-summary.txt`）

```
legacy_keys=40112
bucket_keys=94589
gap_total=1
catA_local_known=0
catB_local_unknown=1
catC_unmappable=0
legacy_vs_sidecar_equal=149
legacy_vs_sidecar_legacy_newer=0
legacy_vs_sidecar_legacy_older=39962
legacy_vs_sidecar_unmapped=1
bucket_vs_sidecar_equal=93279
bucket_vs_sidecar_bucket_newer=0
bucket_vs_sidecar_bucket_older=0
bucket_vs_sidecar_unmapped=1310
map_variant=unescaped
VERDICT=OK
```

**逐项读法**：

| 对照 | 数字 | 含义 |
|---|---|---|
| `legacy_vs_sidecar_legacy_older` | **39,962** | legacy 路径的 HLC 比侧车**旧** ⇒ legacy 路径每轮把旧 HLC 写回侧车（写放大机制，**不是** R1 被绕过） |
| `legacy_vs_sidecar_equal` | 149 | legacy 与侧车 HLC 相同 |
| `legacy_vs_sidecar_unmapped` | 1 | legacy 键在侧车里查不到 ⇒ 即 §0 那 1 条 `B_local_unknown` |
| `bucket_vs_sidecar_equal` | **93,279** | 桶侧与侧车**同时存在**的 93,279 个键，HLC **逐字相等** ⇒ 判据④替代口径成立（见 ADR 0081 §4.3） |
| `bucket_vs_sidecar_bucket_newer/older` | **0 / 0** | 桶侧 HLC **既不新于也不旧于**侧车 ⇒ 无一侧漂移 |
| `bucket_vs_sidecar_unmapped` | 1,310 | 桶侧有、**本地侧车没有**的键（1,310 条）⇒ 单侧存在，不参与逐条比对 |

**交叉印证**：`bucket_vs_sidecar_unmapped=1310` 与真机客户端日志的 `deferred=1310`
（`reconcile scan_complete=0 matched=92349 deferred=1310 divergent=16`）**逐字吻合**
⇒ 两侧口径自洽。

**HTTP 错误**：`http_errors=0`（桶拉取 11 页全部 200）。

---

## 3. 归因边界（**未坐实，不得当结论**）

- **已坐实**：桶侧键数从 09:28 的 92,354 增至 10:48 的 94,589；缺口从 926 收敛到 1。
- **时间上相关**：同期客户端跑过两轮桶推送（09:25 `pushed=896 skipped=0`、
  09:41 `pushed=84 skipped=880`）。
- **未坐实**：「legacy 键进入桶侧」的**具体机制**（本地词典是否真的持有那 926 条
  对应条目、推送覆盖顺序如何）本轮**未逐条坐实** ⇒ 登记为**待坐实**。
- **不做推断**：不得据此宣称「判据④ 的红是假红」或「已自愈」。判据④ **原文未改**，
  替代口径仍是**草案**（ADR 0081 §4.1，待人工批准）。

---

## 4. 复跑方式

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File `
  'F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\phase1-legacy-gap-classify.ps1' `
  *>&1 | Tee-Object -FilePath 'F:\soft\00selfmade\rime_claude\tmp\relay02\gap-classify-console2.log'
```

**脚本安全约束（本轮确立，勿回退）**：

- **绝不**在脚本里调用服务端 `/v1/auth/refresh` —— 成功即轮换并**撤销**旧
  `refresh_token` ⇒ 运行中客户端会永久失去续期能力。改为：401 时**只重读本机凭据库**
  重试（上限 3 次）。
- 生产 D1 只 `SELECT`；侧车只读；本地词典只读。

---

## 5. 本轮修订的既有记载（**订正**）

| 记载 | 原文 | 订正 |
|---|---|---|
| handoff §P-H.4 | 「桶指纹（brow）目前**不落盘**」 | **订正**：`brow` **已落盘**，实测 1,024 行（标签直方图 `brow:1024`） |
| ADR 0077 相关注释 | 「brow 目前不落盘 ⇒ 现网实测写入量级远高于配额的设计意图」 | 同上，需在后续批次同步修订（**本轮未改 ADR 0077**） |
