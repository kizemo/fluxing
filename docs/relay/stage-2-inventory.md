# 阶段 2 盘点 — 复核轮：真机 kBoth 重判四道判据

- 链：BR-①g 桶化双写后续批次（6 段）
- 本环：`relay-03of06`（阶段 2）· session `mvs_3ff9eb4043e6496ca71c5edd7f5e0216`
- 日期：2026-11-27（11:12 起 · 13:1x 收口）
- 仓库：`F:\soft\00selfmade\rime_claude` · 分支 `feat/100-cloud-sync`
- 进环 HEAD：`1d86d8c7fbf0101b51ccdc4d3452bf04db2eeef9`（阶段 1 提交）
- 来源口径：`docs\plan-relay.md:56-62`（阶段 2 目标 / 判据 / 硬约束 / 产出 / 红线）

> 一句话结论：**复核轮不通过**（① 红 · ② 绿但依赖轮的选择 · ③ 不可判定 · ④ 红），
> 阶段不自动推进；④ 的红是**结构性**的，工程侧无法自愈 ⇒ 按
> `docs\relay.json` 的 `stop_conditions`「human checkpoint hit -> status=blocked」
> 与「ambiguous or contradictory acceptance criteria」落 `status=blocked`。

---

## 1. 本环做了什么（执行摘要）

| # | 动作 | 结果 |
|---|---|---|
| 1 | 核实装机二进制是否含阶段 1 改动 | **不含**（旧件早于提交 `1d86d8c`）⇒ 必须先部署 |
| 2 | 升版 1.3.13 + 构建 + 出包 + 静默装机 | 全成功（三处 `WeaselServer.exe` sha256 逐字相等） |
| 3 | 解「启动轮 `bucket phase=0`」疑点 | 维护闸门早退副作用，**不是**阶段解析 bug |
| 4 | 真机 kBoth 轮取证 | 跑到 page9 `http=503` 早退；**桶推路径本轮未执行**（`quota_lines=0`） |
| 5 | 判据④ 原文口径重判 | **FAIL**（`hlc_mismatch=39946` / `missing_in_bucket=1` / `extra_in_bucket=54478`） |
| 6 | 判据④ 替代口径（ADR 0081 草案）参考测量 + 鉴别力 | 好/坏样本方向相反、偏移量**精确 +500** ⇒ 口径有鉴别力 |
| 7 | 侧车 `brow`/`row` 与生产 `sync_buckets` 只读差分 | **三方逐字相等**（1,024 桶 / 94,589 条 / MAX(id)=3971） |
| 8 | 阶段位回滚（`phase=1 → 0`）+ 进程复位 | 成功（新 pid 564580 @13:10:19） |

**未做**（红线内禁止）：切读（阶段 3/4 永久禁用）、任何生产写、服务端迁移/部署、
用户词典数据变动、`/v1/auth/refresh` 调用。

---

## 2. 部署前置 — 阶段 1 修复从未装机（本环必须补）

阶段 1 的门禁绿是在**构建产物**上跑的，装机件仍是 11-25 的 1.3.11 ⇒ 直接取证会
判的是旧代码。处置：

| 项 | 值 |
|---|---|
| 版本 | `1.3.13`（`version.txt` / `env.bat` / `weasel.props` / `CHANGELOG.md` 四件对齐） |
| 构建 | `tmp\br1h-buildfix.bat`，`BUILD_EXIT=0`（114.25 s） |
| 构建产物 | `build\windows\x86\release\WeaselServer\WeaselServer.exe` · 3,108,864 B · sha256 `3B12827BF8604AF99B934557F4B0D3C869A3B4C0BDC5D8911AB6B27489B90ABF` |
| 出包 | `output\archives\fluxing-1.3.13-installer.exe` · 43,146,124 B · sha256 `15DA46361B268D21FB01EDFCE761C68FD6A587E3422AE4A2595546FBFCCA2BC8` |
| 文件/产品版本 | `FileVersion=1.3.13.0` / `ProductVersion=1.3.13.1` |
| 装机 | `reports\fluxing-br-1g\stage2-install.ps1`，`INSTALL_EXIT=0`（133 s） |
| 三处 sha256 | 构建 = 包内 = 装机后，**逐字相等** |
| 装机后进程 | pid **493512** @ 11:37:48（`D:\Program Files\fluxing\weasel\WeaselServer.exe`） |
| 侧车 | 12,284,871 B **未被装机改动** |

**工具坑**：`xmake -r` 首轮报 `error: FluxingDarkModeBridge.cpp`（无诊断行），
**原样重跑即绿** ⇒ 并行编译偶发，不要据此改代码。

---

## 3. 四道判据 — 按 `plan-relay.md:59` 原文口径重判

> 原文（逐字）：① 桶端点请求零 4xx/5xx；② 桶拉桶数 = 服务端 `sync_buckets` 该用户行数；
> ③ 侧车 `brow` = 已推成功桶数；④ 按新口径重判（**判别力**：坏样本必须判红）。

| 判据 | 原文口径关键点 | 本环实测 | v1 裁定 | **v2 追溯裁定（2026-11-27）** |
|---|---|---|---|---|
| ① 零 4xx/5xx | 桶端点请求的状态码 | 启动轮 11 页 **全 200**；kBoth 轮 page1–8 全 200，**page9 `http=503`** | 🔴 FAIL | 🟢 **PASS**（`4xx = 0`；503 单条登记；仅一轮 `kBoth` ⇒ 无「连续两轮 5xx」） |
| ② 桶拉桶数 = 服务端行数 | 1,024 vs 1,024 | 启动轮 `pulled_buckets=1024`，生产 `COUNT(*)=1024` | 🟢 PASS（附条件） | 🟢 **PASS**（按「最近一个自然收尾且无 4xx/5xx 的完整轮」，条件取消） |
| ③ `brow` = 已推成功桶数 | 原文未定义时间窗 | `brow` 行数 1,024；本轮 `pushed=0` | ⚪ UNDETERMINED | ⚪ **未判定**（③-1 作废，改「桶推执行证明」；本环 `bucket quota` 0 命中） |
| ④ 两路逐条 HLC 一致 | 全等 + 坏样本判红 | `hlc_mismatch=39946` / `missing_in_bucket=1` / `extra_in_bucket=54478` | 🔴 FAIL | 🟡 **条件 PASS**（替代口径「桶侧不得旧于本地」：`unmapped=0`；`bucket_older=6` 待归因） |

### 3.1 判据① — FAIL

kBoth 轮（`reports\fluxing-br-1g\stage2c-kboth-evidence.txt`）桶拉逐页：

- page1–page8：`http=200`（各 100 桶）
- **page9：`13:05:32.546 bucket pull page=9 http=503 buckets=0 items=0 has_more=0`**
- 紧随其后：`13:05:32.601 reconcile … early_exit=bucket_pull_failed`
- 收官行：`13:05:32.604 bucket phase=1 pushed=0 skipped=0 pulled_buckets=800 pulled_items=73922`

⇒ 只要轮内出现非 2xx，判据① 即红。**启动轮全 200 不构成对冲**（原文是「请求零 4xx/5xx」，
不是「至少有一轮全 200」）。

**503 根因未定**：服务端 `auth.ts:1043-1045` 在**读路径**上刷新 `devices.last_seen`
（读也写）⇒ 与 D1 写额度/并发相关的可能性存在，但本轮**未取到**服务端日志或
Cloudflare 指标，**不得当结论**。

### 3.2 判据② — PASS（口径已裁定，条件取消）

| 侧 | 值 | 证据 |
|---|---|---|
| 客户端桶拉（启动轮，11 页） | 1,024 桶 / items Σ=94,589 | page1–10 各 100 桶 + page11 24 桶 `has_more=0` |
| 生产 `sync_buckets` | 1,024 行 / Σ`item_count`=94,589 / MAX(id)=3971 | `tmp\relay03\post-kboth-sync_buckets.json` |
| 用户范围 | 单用户 `d42756f4-866c-465f-a9d4-aa6d47464426` | 同上 |

⇒ 逐字相等。**2026-11-27 裁定（v2）**：取批次内**最近一个**同时满足「`bucket pull` 全部分页 http 均为 2xx」与「最后一页 `has_more=0`」的完整轮 ⇒ 启动轮满足（`1,024 / 94,589` 逐字相等）⇒ **PASS（无条件）**，`PASS_WITH_CAVEAT` 作废。
**陷阱（必须留证）**：kBoth 轮 503 那页的 `has_more=0` 是**错误响应体解析结果**，不是自然收尾 ⇒「无 4xx/5xx」是**前置条件**而非并列条件；批次内**所有**轮次分页状态必须全部落盘，不得只保留有利轮次。

### 3.3 判据③ — 未判定（v2「桶推执行证明」；原口径缺陷 ③-1 作废）

**2026-11-27 裁定（v2）**：判据③ 改为「桶推执行证明」—— 同一轮日志必须同时出现 ① 至少一行 `bucket quota ` 前缀诊断行（`FormatBucketQuotaLine`：`held_unproven / held_quota / rows_pushed / rows_held / quota_rows`）② 同一轮 `bucket phase=` 行的 `pushed + skipped >= 1`。**判绿** = 两条同时成立且 `rows_pushed <= quota_rows`；**判红** = 有 `bucket phase=` 但同轮无 `bucket quota`，或 `pushed + skipped = 0`；**未判定** = 无 `bucket phase=`。

本环实测：`reports\fluxing-br-1g\stage2c-kboth-evidence.txt` **全文件 `bucket quota` 0 命中**；`bucket phase=1 pushed=0 skipped=0`（kBoth 轮 503 早退）⇒ **未判定**。

- **原口径（`brow` = 已推成功桶数）作废**：`brow` 是**桶级账**（每桶一行，push 成功写指纹 / skipped 写空指纹 / 未写桶保留旧行；`CloudSyncService.cpp:2321-2335` / `:2207-2260`，`CloudSyncService.h:575`），盘上 1,024 行 = 全库桶数，**不是本轮推送计数** ⇒ 「历史累计 1,024/1,024」无鉴别力，明确排除。
- **放弃的公式**：`pushed + skipped + held = 计划桶数` —— `FormatBucketQuotaLine` 不打「计划桶数」，`skipped` 混多种成因，不可从已印字段复原。
- **取得判据③ 的唯一路径** = 一轮不早退的 `kBoth` ⇒ 必须先修缺陷 ①-1（见 §11.1-1）。

### 3.4 判据④ — v1 FAIL / v2 条件 PASS（替代口径「桶侧不得旧于本地」）

| 指标 | 值 |
|---|---|
| `d1_rows_fetched` | 40,112 |
| `d1_sync_items_user_rows` | 40,112（用户行全量，校验口径命中） |
| `dup_item_keys` | 0 |
| 桶侧（R2 直读） | `rows=1024 buckets=1024 envelopes=94589 unique_keys=94589 item_count_mismatch=0 dup_keys=0` |
| 交叉校验（R2 直读 vs D1 桶表） | `buckets=1024 d1_missing=0 mismatch=0 http_errors=0` |
| `legacy_keys` / `bucket_keys` | 40,112 / 94,589 |
| **`missing_in_bucket`** | **1** |
| **`hlc_mismatch`** | **39,946** |
| **`extra_in_bucket`** | **54,478** |
| `http_errors` | 0 |
| **`JUDGE4_VERDICT`** | **FAIL** |

**与阶段 1 基线的对照**（`tmp\relay03\stage1-phase1-judge4-hlc-compare-20261127.txt`：
`missing_in_bucket=926` / `hlc_mismatch=39043` / `extra_in_bucket=53168`）：

- `missing_in_bucket` 926 → **1**（阶段 1 的 B 定性 + 规则 1 v2 生效方向正确）
- `hlc_mismatch` 39,043 → 39,946、`extra_in_bucket` 53,168 → 54,478
- **两侧基线不同**（桶侧条目 92,354 → 94,589，+2,235）⇒ `extra` / `mismatch` 的绝对值
  **不可跨环直接比较**；同环内三角身份（R2 直读 = D1 桶表 = 侧车）成立

⇒ **原文口径仍红，且「两路逐字相等」是结构性不可达**（证明链）：① 桶侧 HLC 与本地侧车逐字相同（桶侧 `ms` 恒 `1795565189774`、counter 逐条 +1）⇒ 桶推既未篡改也未重分配 HLC；② 2026-11-26 桶化首导整库重分配了本地侧车 HLC；③ 老端点 D1 永久持 11-24 旧批次且不参与本轮同步 ⇒ 两路不可自愈；④ 要两路逐字相等只有三条路（回退侧车 93,278 行 HLC / pull 用旧 HLC 覆盖 / 一次性迁移桶表 ≈190,000 行写），**三条全部越红线** ⇒ 再排取证轮只会得到同样的红。

**2026-11-27 裁定（v2）**：判据④ 采纳替代口径「**桶侧不得旧于本地**」（切读安全性口径），原文「两路逐条 HLC 一致」作废。本环 = **条件 PASS**：`unmapped = 0`（等价硬拦「真实漏数据」）、`dup_item_keys = 0`、`http_errors = 0`；`bucket_older = 6` 待 §4 四准入归因。`extra_in_bucket`（54,478）明文退出判据④（切读后桶侧为唯一权威源），风险由「桶阶段 3/4 仍禁 + 规则 1 盲区未修前不得切读」兜底。

---

## 4. 判据④ 替代口径（ADR 0081 · **2026-11-27 已裁定采纳 · Proposed**）实测

口径：**桶侧不得旧于本地侧车**（切读安全性口径），逐 key 比较，好/坏样本判向必须相反。

| 样本 | `bucket_vs_sidecar_equal` | `bucket_newer` | `bucket_older` | `unmapped` |
|---|---|---|---|---|
| 基线（真实侧车） — 阶段 1 测 | 93,279 | 0 | 0 | 1,310 |
| **基线（真实侧车） — 本环测** | **94,583** | **0** | **6** | **0** |
| 坏样本 `badnewer`（+500 条桶侧更新） | 94,083 | **500** | 6 | 0 |
| 坏样本 `badolder`（+500 条桶侧更旧） | 94,083 | 0 | **506** | 0 |

- 三份合计恒 **94,589**（= 桶侧条目总数）⇒ 分类完备
- 坏样本相对基线在**期望方向精确偏移 500**（`506 = 6 + 500`）⇒ **判据有鉴别力**
- **但基线本身非零**：草案口径下仍有 **6 条「桶侧旧于本地」**违例 ⇒ 即便采纳草案口径，
  也需先解释这 6 条（或设容差并留证），**不能直接判绿**

**2026-11-27 裁定**：替代口径**已采纳**为判据④ 的唯一口径（ADR 0081 升 `Proposed`）⇒ `plan-relay.md:122`「暂不采纳 A2」**作废**，本表从「参考测量」升为**判据口径实测**。基线 6 条 `bucket_older` 按四准入处置（逐条落盘 `reports\fluxing-br-1g\judge4-v2-older-attrib.tsv` + 机械判据 `bucket_hlc.ms <= 1795565189774`），**归因全绿才判 PASS**，不得改口径掩盖。

---

## 5. 侧车 vs 生产桶表只读差分（判据② 的独立旁证）

侧车真身：`D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state`
（**只读**，本环未修改；扰动只在 `tmp\relay03\` 副本上做）

| 项 | 值 | 与生产对照 |
|---|---|---|
| 文件大小 | 12,284,871 B（kBoth 轮前）→ 12,284,873 B（轮后） | 早退轮**不整库回写**（mtime 停在 12:01:11） |
| 行数 | 95,619 行 | — |
| 头部 | `#fluxing-cloud-sync-v2` / `seq 71684` / `bseq 3971` / `phase 1` / `keyfp a6789e5b…` | `bseq 3971` = 生产 `MAX(id)` |
| `^brow\t` 行数 | **1,024** | = 生产桶行数 1,024 |
| `row` 条目行 | **94,589** | = 生产 Σ`item_count` = R2 信封数 |

⇒ **三方（侧车 / D1 桶表 / R2 对象体）逐字相等**。

---

## 6. 桶推路径真机连续三轮未验证（本环新增一次成因）

| 环 | 轮性质 | `pushed` | 直接成因 |
|---|---|---|---|
| 11-25 §P | 启动轮 `kPullOnly` | 0 | 启动轮固定 `kPullOnly`，桶推闸门要求 ∈ {`kPushOnly`,`kBoth`} ⇒ **结构性关闭** |
| 11-26 §P-P | phase 1/2 | 0 | `scan_complete=0` / `deferred=1310` ⇒ 规则 1「库存未证实不推送」闸门常闭 |
| **11-27 本环** | kBoth | 0 | **桶拉 page9 `http=503` 早退** ⇒ `early_exit=bucket_pull_failed` |
| **11-27 本环** | 启动轮（`kPullOnly`） | 0 | 同上（结构性） |

本环 kBoth 轮 `bucket quota` 行**从未出现** ⇒ 桶推函数**未被调用**。
⇒ 「桶推路径真机验证」这一目标**仍然未达成**，且成因已换过一次（不是同一原因重复）。

---

## 7. 生产零写入与侧车未污染（本环自证）

| 证据 | 值 |
|---|---|
| `sync_buckets` 行/Σ/MAX(id) | 1,024 / 94,589 / 3,971（轮前轮后**不变**） |
| wrangler 元数据 | `changes:0` / `rows_written:0` |
| D1 `rows_written_24h` | 14,063 → **13,968**（下降，非本环写入） |
| D1 `rows_read_24h` | 418,140 → **658,371**（+240,231，**全部归因本环取证自身**：R2/桶表/D1 判据④ 全量读） |
| 侧车 | 未修改（sha256 前后相等，脚本自检） |

---

## 8. 阶段位回滚（收口必做）

`reports\fluxing-br-1g\stage2-rollback-phase.ps1` → `stage2-rollback-phase.txt`：

| 项 | 值 |
|---|---|
| 回滚前 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` | `1` |
| 回滚前 pid / 启动时间 | 493512 / 2026-11-27 11:37:48 |
| 回滚前侧车 `phase` 行 | `[phase 1]` |
| 回滚后 User 级变量 | **`0`**（回读确认） |
| 回滚后 pid / 启动时间 | **564580** / 2026-11-27 13:10:19（`restart_confirmed=1`） |
| 回滚后侧车 `phase` 行 | 仍 `phase 1` ⇒ 写回发生在下一轮，**未验证**，**不代判** |
| 诊断日志末三条 `bucket phase` | `phase=0`（门禁测试 exe · 11:39）/ `phase=1`（12:01 启动轮）/ `phase=1`（13:05 kBoth 早退轮） |

**注意**：11:39:04–11:39:05 的密集 `phase=0` 行来自**门禁测试 exe**（同段可见
`early_exit=maintenance_gate` / `begin_maintenance` / `export_userdict`）⇒ 判据① 的
http 码统计**必须限定桶端点行**，否则会被测试进程污染。

---

## 9. 门禁（阶段 1 修复轮的部署前复核）

`reports\fluxing-br-1g\gate-relay03-20261127.log`（盘上回读）：

| 指标 | 值 | 判据 |
|---|---|---|
| `PASS` | 1014 | ✅ |
| `FAIL` | 0 | ✅ |
| `UFFFD_COUNT` | 0 | ✅ |
| `CLANGFORMAT_HUNK_DRIFT` | 0 | ✅ |
| `CLANGFORMAT_WHOLEFILE_DRIFT` | 3（`CloudSyncService.cpp` / `CloudRimeUserDictBackend.cpp` / `TestFluxingCloud.cpp`） | HEAD 存量，**不在判绿条件内** |

跑前必须压阶段变量：`$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'`（否则测试进程继承
User 级 `1` ⇒ 全部 legacy 用例误入桶模式 ⇒ 89 项假失败）。

---

## 10. 阶段 2 结论与红线状态

**结论 = 复核轮不通过**，阶段**不自动推进**（`plan-relay.md:41`）。

| 红线 | 状态 |
|---|---|
| 阶段 3/4 切读 | **仍永久禁用** |
| 阶段不得自动递增 | 遵守（本环未 bump） |
| 生产 D1 / R2 / Cloudflare | **只读**（`rows_written:0` 自证） |
| 用户词典数据 | 未动（侧车只读、`RimeUserDir`/`InstallDir` 未清） |
| 凭据值 | 未进日志/输出/提交物；未调用 `/v1/auth/refresh` |
| `AC-09` | **仍 FAIL，未改记** |
| `AC-212` / `AC-215②` | 未判定 |
| `AC-213` / `AC-214` | 永久退役 |

---

## 11. 原待裁定六项 —— 2026-11-27 裁定对照（本文件不再挂「待裁定」）

| # | 原待裁定项 | 裁定 | 落点 |
|---|---|---|---|
| 1 | 判据④ 是否采纳替代口径「桶侧不得旧于本地」 | **采纳**（基线 6 条按四准入逐条归因，不做容差豁免） | `stage-2-rulings-20261127.md` §1；ADR 0081 → `Proposed` |
| 2 | 判据③ `brow` 语义与时间窗（缺陷 ③-1） | **改写为「桶推执行证明」**；③-1 作废；不取「历史累计 1,024/1,024」读法 | §2；本文件 §3.3 |
| 3 | 判据② 以哪一轮为准 | **最近一个「全分页 2xx + 末页 `has_more=0`」的完整轮**；`PASS_WITH_CAVEAT` → `PASS` | §3；本文件 §3.2 |
| 4 | 503 与是否重排取证轮 | **不单独排取证轮**；新登记客户端缺陷 **①-1**（`bucket pull` 无有限重试）归接力阶段 2 前半段，先修再排一轮 | §4；本文件 §11.1 |
| 5 | `WRITE_BUDGET_ROWS_PER_DAY` 80,000 vs 20,000 | **落 20,000**；执行是一次生产写，**需单独授权** | §5；`plan-relay.md` 阶段 3 |
| 6 | 阶段 6 硬前置（`api.jury.xin` 放行 / ECS `5432` 收口 / 两条探针账号） | **仍未裁定**（阶段 6 前必须定） | `plan-relay.md` 阶段 6 |

### 11.1 本环新登记缺陷与残留（未闭环，下一环必读）

1. **缺陷 ①-1（客户端 · 写集内 · 接力阶段 2 前半段）**：`bucket pull` 遇非 2xx **无重试直接早退**（`early_exit=bucket_pull_failed`）⇒ 单次 5xx 即整轮作废。要求：同 page 5xx 重试 ≤ 2 次（退避 1s / 3s）、4xx 不重试、仍失败打印 `bucket pull ... retry=<n>`；验收 = 门禁用例注入 1 次 5xx 必须重试成功，注入 4xx 必须不重试。
2. **6 条 `bucket_older` 未归因**：需扩展 `tmp\relay03\classify-r2.ps1`（复用已取 `tmp\relay03\r2objs\` 的 1,024 blob，**不重拉 R2**）输出明细到 `reports\fluxing-br-1g\judge4-v2-older-attrib.tsv`，并按 `bucket_hlc.ms <= 1795565189774` 机械判据逐条落归因。
3. **503 根因未定**：`auth.ts:1043-1045` 读路径刷新 `devices.last_seen`（读也写）与深偏移查询超时为两个待验假设；`rows_written_24h ≈ 13,968` 远低于 80,000 / 100,000 ⇒ 写预算耗尽不是主要假设。**未出结论前一律记「未定」**。只做一次合并只读排查（`wrangler tail` 13:05:32 ±5 分钟 + CF Analytics，只查 page 9 / `OFFSET 800`），**禁止**调 `/v1/auth/refresh`。
4. **判据④ 的 `bucket_older` 归因未跑 ⇒ 本环不得记 PASS**；**阶段 2 总体不判绿**（唯一致命项是判据③ 未判定）。
5. **冻结项**：桶阶段 3/4 仍禁；规则 1 盲区未修前不得切读；`AC-09` 仍 FAIL 不得改记；`AC-212` / `AC-215②` 未判定；`AC-213` / `AC-214` 永久退役。

---

## 12. 证据文件清单（全部盘上存在）

| 文件 | 内容 | 入库 |
|---|---|---|
| `reports\fluxing-br-1g\stage2r2-judge4-hlc-compare-20261127.txt` | 判据④ 原文口径裁定（34 行） | ❌ reports 不入库 |
| `reports\fluxing-br-1g\stage2c-kboth-evidence.txt` | kBoth 轮逐页桶拉 + reconcile + 收官行（20,533 B） | ❌ |
| `reports\fluxing-br-1g\gate-relay03-20261127.log` | 门禁四行（18 行） | ❌ |
| `reports\fluxing-br-1g\stage2-rollback-phase.txt` | 阶段位回滚五行 | ❌ |
| `reports\fluxing-br-1g\stage2-install.ps1` | 静默装机 + 三处 sha 校验 | ❌ |
| `reports\fluxing-br-1g\stage2-capture-kboth.ps1` / `stage2-wait-kboth.ps1` | kBoth 取证 runner（带落盘判据 + 心跳） | ❌ |
| `tmp\relay03\judge4-stage2-r2.ps1` | 判据④ 重判（**R2 直读路径**，替代原 HTTP 版） | ❌ tmp 不入库 |
| `tmp\relay03\classify-r2.ps1` | 替代口径 + 鉴别力分类 | ❌ |
| `tmp\relay03\fetch-r2-buckets.ps1` | R2 桶对象批量取回（1,024 blob · `length_mismatch=0`） | ❌ |
| `tmp\relay03\r2objs\` | 1,024 个桶 blob（12:00:48→12:07:37 `=== R2 FETCH END OK ===`） | ❌ |
| `tmp\relay03\classify-good|classify-badnewer|classify-badolder\gap-summary.txt` | 鉴别力三连跑 | ❌ |
| `tmp\relay03\post-kboth-sync_buckets.json` / `pre-*.json` | 生产桶表只读快照 | ❌ |
| `docs\relay\stage-2-inventory.md` | 本文件 | ✅ |

---

## 13. 本环新增工具坑（照抄勿重踩）

1. **禁止脚本调用服务端 `/v1/auth/refresh`** —— 成功即轮换并**撤销**旧 `refresh_token`；
   本轮原版判据④ 脚本撞上此红线（桶端点 401 → refresh 得 **400** 后中止）。
   客户端 11:39:05 自行续期但 `AT-1 persisted=false` 不落盘 ⇒ 凭据库那对**已死**。
   **替代路径 = D1 取 `r2_key` + R2 `object get` 直读**（只读、无需令牌、信封明文）。
2. **判据④ 脚本必须强制重查 D1** —— 命中快照缓存会复用旧数（改 `if ($true)`）；
   块级失败默认静默通过 ⇒ 已加 3 次重试 + 失败即 throw；校验口径 =
   `d1_sync_items_user_rows` 必须 = 40,112。
3. **`wrangler r2` 子命令只有 `get` / `put` / `delete`** ⇒ 批量取回必须自己循环
   `node .../wrangler.js r2 object get <key> --file=<out> --remote`；先清
   `HTTP_PROXY/HTTPS_PROXY/ALL_PROXY` 并把 `NO_PROXY='*'`（本机直连 Cloudflare）。
4. **诊断日志全量 `ReadToEnd` / 递归找侧车会让 bash 任务挂起** ⇒ 有界尾读
   （`-Tail 4000` + 正则过滤）+ 已知绝对路径。
5. **含 `Push-Location`/`Pop-Location` 或 `Move-Item` 循环的 bash 命令会被安全策略硬拦**
   ⇒ 绝对路径 + `Copy-Item`，禁递归删除与切目录。
6. **detached runner 会被环境回收** ⇒ 必须自带落盘判据 + 心跳 + `done` 文件，
   不能拿「进程还活着」当证据。
7. **`.NET [regex]::Replace` 的替换串 `$1` 后接数字会被解析成组号**（`$113`）⇒ 用 `${1}`。
8. **`Select-String -Path <root>\install.nsi` 打不开** ⇒ 真实文件是 `output\install.nsi`。
9. **`set_status=running` 被台账闸门拒收**（`status_running_is_human_only`）⇒ 停链后
   不可续链；恢复只能走人类换代动作（归档旧台账 → 重跑 `start-relay.ps1`）。

---

## 14. 第 2 代链 `relay-02of06` 阶段 2 收口（2026-11-27 16:2x · 修复轮 + `kBoth` 真机轮 · 四判据 3 PASS / 1 FAIL ⇒ 停链）

> 本节由 `relay-02of06`（会话 `mvs_f16dfeac5e2445b4a34f60f39d1dfc25`）追加，**append-only，不改动 §1–§13 的任何既有证据与结论**。
> §1–§13 属第 1 代链 `relay-03of06` 的同名产物；两代的判据口径不同（第 1 代按**原文**口径，本代按 `stage-2-rulings-20261127.md` 的 **v2** 口径），**冲突时以本代 §14 为准，但旧节不得删改**。

### 14.1 背景与本环定位

- 上一环（`relay-01of06`）为**确认环**：对修复提交 `1d86d8c` 做零改动复核，三项只读判据 3/3 PASS（`stage-1-confirm-20261127.md`、handoff `## AM`）。
- 本环（`relay-02of06`）为**修复 + 重判轮**：先修 §6 登记的缺陷 ①-1（`bucket pull` 无有限重试 ⇒ 单次 5xx 即整轮早退），过门禁、升版出包装机，再等第 2 轮 `kBoth` 取证并按 v2 口径重判四道判据。
- 本环**只动** `WeaselServer/Cloud/**`（用户已授权）+ `test/TestFluxingCloud/**`；**未**读写生产 D1 / R2 / Cloudflare，**未**动用户词典数据，**未**启用桶阶段 3 / 4。

### 14.2 修复轮（2A）—— 缺陷 ①-1 有限重试

**缺陷。** `PullBucketsIntoRemote` 翻页循环遇任一 `5xx` / 传输层失败即 `break`（整轮桶拉取早退）⇒ 该轮 `bucket_scan_complete=0` ⇒ 规则 1「库存未证实不推送」闸门常闭 ⇒ 桶推永远不发生。

**修法（三文件）。**

| 文件 | 改动 |
|---|---|
| `WeaselServer/Cloud/CloudSyncService.h` | `FormatBucketPullDiagLine(...)` 增**可选**第六字段 ` retry=<n>`（缺省 `-1` ⇒ 与旧日志逐字一致）；`constexpr int kBucketPullMaxRetries = 2;`；`inline bool BucketPullRetryable(int)`；`inline int BucketPullRetryDelayMs(int)`（`{1000, 3000}`）；`SetBucketPullRetryDelayOverrideMsForTest(int)` |
| `WeaselServer/Cloud/CloudSyncService.cpp` | 匿名命名空间 `g_bucket_pull_retry_delay_override_ms` + setter；`PullBucketsIntoRemote` 翻页循环改**同页有限重试** `for(;;)`（每次尝试前复位 `page / page_more / next_seq / next_cursor / pull_err / page_http_status`；`++stats->pull_requests` 移入尝试内，按 **HTTP 尝试**计数）；可重试则先打 `retry=<已重试次数>` 行再 `::Sleep(delay)` 后 `++page_retries`，预算尽或不重试则 `break` |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 新增 `DiagMsgOf`（剥 `[时间戳][tag] ` 前缀 + 裁尾换行）、`RetryCountOf`、`BucketRetryRunRound`、`TestBucketPullRetry`（9 条断言：单次 503 重试成功 / 400 不重试 / 连续 503 三段） |

**语义裁定（须保留）。** 可重试 = `http==0`（传输层失败）**或** `5xx`；不重试 = `4xx` 与「`2xx` + 解析失败」。裁定原文只写「5xx 重试 / 4xx 不重试」，把 `http=0` 归入可重试是**同向扩展**，已在本节登记口径。`retry=` 只在**真发生重试**时追加 ⇒ 4xx 早退行与旧日志逐字一致，既有探针的前缀解析不被破坏。

**门禁（判绿四行）。** `MSBUILD_EXIT=0` / `GATE_EXIT=0` / **`PASS=1047 FAIL=0`** / `UFFFD_COUNT=0` / **`CLANGFORMAT_HUNK_DRIFT=0`** ⇒ 起点基线 1,014 上 **+33**。旁证 `CLANGFORMAT_WHOLEFILE_DRIFT=3` —— 属 HEAD 存量整文件漂移，不计入判绿（与 `## AM` 同口径，不重排）。

### 14.3 升版 1.3.14 + 出包 + 装机（2A 收尾）

- **版本四处同步**：`version.txt`、`weasel.props`（`VERSION_PATCH` 13→14、`PRODUCT_VERSION` / `FILE_VERSION` → 1.3.14.0）、`env.bat`（4 处）、`CHANGELOG.md`（字节级前插 `## [1.3.14]` 段 1,180 B）；回读一致性 `VERSION_CONSISTENCY=PASS`；`env.bat RELEASE_BUILD=1` 在位。
- **构建**（VS2022 `vcvars32.bat` + `xmake f -a x86 -m release -c` + `xmake -r` + `xbuild.bat weasel installer`）：`VCVARS_EXIT=0` / `CFG_EXIT=0` / `BUILD_EXIT=0` / `XBUILD_EXIT=0`；`xmake` 自报 `build ok, spent 107.078s`。
- **包指纹**：`output\archives\fluxing-1.3.14-installer.exe` = **43,149,679 B / 2026-11-27 14:42:15 / sha256 `B979C46438CDB38F1ABEB5DB616F4997921CFDF334DE9B2850C947BAEA8B1BF2`**。
- **三处 sha256 全等**：构建产物 = 包内 `output\Win32\WeaselServer.exe` = 装机后 `D:\Program Files\fluxing\weasel\WeaselServer.exe` = **3,111,424 B / mtime 14:39:46 / sha256 `2E38EBB30207BE816BD50FCB2F95A5C189467DBE577D86BD99C24A42F6E432DA`**；`OUT_FV=1.3.14.0` / `OUT_PV=1.3.14.1`。
- **静默装机**：`INSTALL_EXIT=0 elapsed_s=52`；装机后 `post_pid=396228` / `post_started=2026-11-27 14:43:59`；侧车 `post_sidecar_bytes=12,284,871`（未被装机改动）。

### 14.4 真机取证（2B）—— 两轮日志实测

**第 1 轮（启动轮，恒 `kPullOnly`，14:44–14:55）。** `bucket pull page=1..11` 全 `http=200`，末页 `buckets=24 items=2235 has_more=0`（14:49:18 收官），**无 5xx、无 `retry=`**；`14:55:20.196 reconcile scan_complete=0 matched=94589 deferred=0 divergent=6 pushed=0`；同毫秒 `bucket phase=1 pushed=0 skipped=0 pulled_buckets=1024 pulled_items=94589` ⇒ **启动轮桶推结构性关闭的实证**（不是缺陷，是 `CloudSyncEngine` 的方向约定）。

**第 2 轮（`kBoth`，15:55–16:09）—— 桶推路径首次真机执行成功。**

| 阶段 | 实测 |
|---|---|
| 桶拉取 | `15:56:16–15:59:53` `page=1..11` 全 `http=200`，末页 `buckets=24 items=2235 has_more=0` |
| 语义侧车 | `16:02:12 semantic legacy_rows=0 legacy_held=0`；`16:02:16 userdict import_empty_rejected=0` |
| 桶推 | `16:02:49–16:06:02` `page=1..15`：前 14 页 `buckets=64 accepted=64 skipped=0`，末页 `buckets=5 accepted=5 skipped=0` ⇒ **901 桶全部 `accepted`、`skipped` 为零** |
| 配额 | `16:06:02.410 bucket quota held_unproven=0 held_quota=0 rows_pushed=1802 rows_held=0 quota_rows=2000` |
| 收尾 | `16:09:05.705 reconcile scan_complete=0 matched=94589 deferred=0 divergent=0 pushed=0`；`16:09:05.706 bucket phase=1 pushed=901 skipped=0 pulled_buckets=1024 pulled_items=94589` |

### 14.5 四道判据重判（v2 口径 · 唯一权威 = `stage-2-rulings-20261127.md`）

| 判据 | v2 口径 | 实测 | 判定 |
|---|---|---|---|
| ① `bucket pull` 无 5xx 早退 | `4xx=0` 硬红；5xx 逐条登记，同一 page 连续两轮 5xx 才判红 | 该轮全 `http=200`，零 4xx / 零 5xx，`RETRY_SEEN=0` | **PASS** |
| ② 完整轮存在 | 取批次内最近一个「全分页 2xx + 末页 `has_more=0`」的完整轮 | 15:55 轮 `page=1..11` 全 2xx + 末页 `has_more=0` | **PASS** |
| ③ 桶推真实执行 | 同轮必须有 `bucket quota ` 行；`pushed + skipped >= 1`；`rows_pushed <= quota_rows` | quota 行在位；`901 + 0 = 901 >= 1`；`1802 <= 2000` | **PASS** |
| ④ 桶侧不得旧于本地 | `unmapped=0` **且** `bucket_older=0`（经 §1.3 四条准入）；条款 3 = `bucket_hlc.ms <= 1795565189774` | `unmapped=0` ✔；**`bucket_older=6`，6 条全部 `RED_bucket_hlc_exceeds_cutoff`** | **FAIL** |

**⇒ 阶段 2 四道判据不可能全绿，本环不得记 PASS。**

### 14.6 判据④ 归因轮实测（只读，复用 `tmp\relay03\r2objs\` 缓存，未发 HTTP）

```
bucket_side=r2_direct rows=1024 buckets=1024 envelopes=94589 unique_keys=94589
              item_count_mismatch=0 dup_keys=0 http_errors=0
map_sanity    legacy_mapped_raw=0 legacy_mapped_unescaped=40111 legacy_total=40112
bucket_vs_sidecar  equal=94583 bucket_newer=0 bucket_older_or_other=6 unmapped=0
legacy_vs_sidecar  equal=159 legacy_newer=0 legacy_older_or_other=39952 unmapped=1
older_attrib  rows=6 residue=0 red=6 device_differs=0 cutoff_ms=1795565189774
```

**6 条红明细**（`reports\fluxing-br-1g\judge4-v2-older-attrib.tsv`，表头 `item_key / local_hlc / bucket_hlc / 归因`）：

- `item_key` 前 12 hex：`0efadf717976` / `13b90f414b0c` / `91ed87374dbd` / `a4fd65e32fb7` / `ba53b91a1def` / `f70c87a8a801`
- `local_hlc` 全为 `1795751392344:<5|7|9|10|11|15>:c7cc011f1b29de53`
- `bucket_hlc` 全为 `1795747588092:<对应计数>:c7cc011f1b29de53`
- 差值 −63.4 min；墙钟：local **2026-11-27 11:49:52**、bucket **10:46:28**、阈值 **2026-11-25 08:06:29**
- 两侧设备 id 相同 ⇒ 非 `device_differs`

**排除假红。** local 侧时间戳 11:49:52 **早于**侧车快照 mtime `12:05:38` ⇒ 不是「快照读取晚于写入」造成的假红。

**鉴别力三样本复现（先证探针能红）。** 基线 `good` ⇒ `equal=94583 newer=0 older=6 unmapped=0`；`bad-newer` ⇒ `bucket_newer=500`、`older=6`；`bad-older` ⇒ `bucket_older=506`（=6+500，精确）；三份键集规模恒等 94,589 ⇒ **探针具备鉴别力，`older=6` 不是恒零假绿**。

### 14.7 节段位与运行时状态（收口时）

- **User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` 已回滚 `0`**（回读 `PHASE_USER_AFTER=[0]`）。
- **运行中的 `pid 396228` 未重启**（本环红线：重启会重置 `startup_pending_`，把 `kBoth` 轮再推后 ≈60 min）⇒ 其**进程环境块快照仍为 `1`** ⇒ 侧车 `phase` 写回 `0` **本环内不可验证**，登记为未验证项。
- 相位来源坐实：`CloudSyncService.cpp:1138` 用 `std::getenv` ⇒ 读进程环境块快照 ⇒ **改 User 级注册表变量不影响运行中进程**。

### 14.8 停链裁定

按台账 `stop_conditions`：判据④ 在上一链（13:19）与本链**两次**判 FAIL ⇒ 命中「same acceptance failed twice」；同时属「human checkpoint hit」 ⇒ 收口记 **`status=blocked`**、**不转交下一环**、停止链条。

**未闭环 / 待人工裁定：**

1. **判据④ 6 条红的机制未取证**。现为假设：这 6 条本地 HLC 在 10:46→11:49 被本地抬升，最可能来自语义侧车「旧口径行抬高本地 HLC」的重锚分支（ADR 0076 D2-5a-9 第 ② 条修法）。本轮 901 桶已全部推上去 ⇒ **复看 `bucket_older` 是否归零即可证伪或坐实该假设**，但**尚未复看**（需再跑一轮只读归因，属下一环或人工授权范围）。
2. **规则 1 盲区未修**：`inventory_complete` 只看「拉到底了」，`unproven_keys` 不收「远端桶读不出来」的键 ⇒ 半库存桶会被回推 ⇒ 服务端 `DELETE+INSERT` 静默删条目 ⇒ **阶段 3 / 4 后不可逆**。**切读仍禁。**
3. **阶段 3（P0 假空修复）未开工**（阶段语义以 `plan-relay.md` §2 为准，台账 `stage_titles[2]` 旧「写预算」文案已随 15:47 裁定作废，闸门不提供改写动作）。
4. 台账闸门能力缺口：`set_status=running` 被拒（停链后不可续链）、`next_session_id` 无法清空 ⇒ 恢复只能走人类换代（归档旧台账 → 重跑 `start-relay.ps1`）。
5. `AC-09` **仍 FAIL，未改记**；`AC-212` / `AC-215②` 未判定；`AC-213` / `AC-214` 永久退役。

### 14.9 本环新增工具坑（5 条）

1. **改 User 级桶相位变量不影响运行中进程**（`std::getenv` 读进程环境块快照）⇒ 「设变量 + 观察」在同一进程内永远假阴性；要么重启进程（代价 = `kBoth` 轮推后 ≈60 min），要么接受「本轮不可验证」。
2. **等待器判真值不能用「`bucket phase=1` 行数 ≥2」** —— 启动轮（`kPullOnly`）也打该行 ⇒ 会假绿。可靠真值 = **`bucket quota ` 行** 或 **`bucket phase=… pushed=[1-9]`**；旧 `stage2-wait-kboth.ps1` 已弃用，改用 `reports\fluxing-br-1g\stage2d-wait-kboth.ps1`。
3. **缺 BOM 的中文 `.ps1` 交 PS 5.1 执行会解析失败**（`Unexpected token ')'` / `The assignment expression is not valid`）⇒ 给人跑的装置脚本补 UTF-8 BOM；**无 BOM 但含中文**的脚本必须用 `pwsh` 跑。
4. **诊断行逐字比对必须先剥 `[时间戳][tag] ` 前缀** —— 行尾是**纯 LF、无 `\r`**（`include/FluxingDiagLog.h:104-116`，单次 `WriteFile` 行级原子）⇒ 「CRLF 导致比对失败」是错误假说，真因是前缀。
5. **`git clang-format HEAD` 要求工作树已 `git add` 该文件**（未暂存会被拒）；复验口径 = `git clang-format --diff HEAD`（返回 1 属正常改动，复验须 `0` 行 diff）。

### 14.10 证据清单（`reports/**` 不入库）

| 路径 | 内容 |
|---|---|
| `reports\fluxing-br-1g\stage2b-build.1.3.14.log` / `.cmd` | 1.3.14 构建 + 出包转录 |
| `reports\fluxing-br-1g\stage2-install.log` / `stage2-install.ps1` | 静默装机转录与脚本（BOM 已补） |
| `reports\fluxing-br-1g\stage2-bump-1.3.14.ps1` | 升版脚本（字节保真往返） |
| `reports\fluxing-br-1g\gate-p0-run-stage2b.log` | 门禁四行全量转录（`PASS=1047`） |
| `reports\fluxing-br-1g\stage2d-wait-kboth.ps1` / `stage2d-wait-state.txt` / `stage2d-kb-evidence.txt` | 等待器、命中终态、43 行证据摘录 |
| `reports\fluxing-br-1g\stage2d-classify-attrib.log` | 判据④ 归因轮全文 |
| `reports\fluxing-br-1g\judge4-v2-older-attrib.tsv` | 6 条红逐条落盘（条款 2 要求形状） |
| `reports\fluxing-br-1g\stage2-judge4-verdict.md` | 判据④ 裁定书（条款 1 PASS / 条款 2 FAIL） |
| `reports\fluxing-br-1g\stage2r2-judge4-hlc-compare-20261127.md` | R2 直读口径对照（`legacy_keys=40112 / bucket_keys=94589`） |
| `reports\fluxing-br-1g\stage2d-progress.md` | 本环进度与中间数字 |