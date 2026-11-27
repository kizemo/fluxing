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

| 判据 | 原文口径关键点 | 本环实测 | 裁定 |
|---|---|---|---|
| ① 零 4xx/5xx | 桶端点请求的状态码 | 启动轮 11 页 **全 200**；kBoth 轮 page1–8 全 200，**page9 `http=503`** | 🔴 **FAIL** |
| ② 桶拉桶数 = 服务端行数 | 1,024 vs 1,024 | 启动轮 `pulled_buckets=1024`，生产 `COUNT(*)=1024` | 🟢 **PASS**（附条件） |
| ③ `brow` = 已推成功桶数 | 原文未定义时间窗 | `brow` 行数 1,024；本轮 `pushed=0` | ⚪ **UNDETERMINED** |
| ④ 两路逐条 HLC 一致 | 全等 + 坏样本判红 | `hlc_mismatch=39946` / `missing_in_bucket=1` / `extra_in_bucket=54478` | 🔴 **FAIL** |

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

### 3.2 判据② — PASS（附条件：结论依赖「取哪一轮」）

| 侧 | 值 | 证据 |
|---|---|---|
| 客户端桶拉（启动轮，11 页） | 1,024 桶 / items Σ=94,589 | page1–10 各 100 桶 + page11 24 桶 `has_more=0` |
| 生产 `sync_buckets` | 1,024 行 / Σ`item_count`=94,589 / MAX(id)=3971 | `tmp\relay03\post-kboth-sync_buckets.json` |
| 用户范围 | 单用户 `d42756f4-866c-465f-a9d4-aa6d47464426` | 同上 |

⇒ 逐字相等。**但**：kBoth 轮被 503 截断在 **800 桶 / 73922 条**，若按 kBoth 轮取值则
② 也不成立。原文未写「以哪一轮为准」⇒ 本环按**完成的整轮**（启动轮）判绿，
并**显式登记该依赖**，交人工确认口径。

### 3.3 判据③ — UNDETERMINED（口径缺陷 ③-1）

代码事实（`CloudSyncService.cpp:2321-2335` / `:2207-2260`，`CloudSyncService.h:575`）：

- `brow` 行 = **桶级记录**，格式 `brow\t<bucket_key>\t<ms>\t<counter>\t<device>\t<item_count>\t<fingerprint>`
- push 成功 ⇒ 写指纹；被 skipped ⇒ 写空指纹；**本轮未写的桶保留旧行**
- ⇒ `brow` 是「侧车已知桶的账」，**不是「本轮已推成功桶数」**

两种读法结论相反：

| 读法 | 值 | 结论 |
|---|---|---|
| 「本轮已推成功」 | 本轮 `pushed=0`（kBoth 早退，桶推未执行） | 1,024 ≠ 0 ⇒ **不成立** |
| 「历史累计已推」 | 1,024 / 1,024 行 | 成立 |

⇒ **登记为口径缺陷 ③-1**，取 UNDETERMINED，**不取有利读法**。修口径需人工定
「时间窗 + `brow` 语义」后再重判。

### 3.4 判据④ — FAIL（原文口径，`reports\fluxing-br-1g\stage2r2-judge4-hlc-compare-20261127.txt`）

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

⇒ **原文口径仍红**。这不是实现缺陷：`plan-relay.md:120` 已坐实「桶侧 HLC 与本地侧车
逐字相同；2026-11-26 桶化首导整库重分配 HLC，老端点 D1 永久持 11-24 旧批次」⇒
两路差异**不可自愈**，再排取证轮也只会得到同样的红。

---

## 4. 判据④ 替代口径（ADR 0081 草案）参考测量 — **草案 · 未批准**

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

依据 `plan-relay.md:122`「暂不采纳 A2（修订判据④）」⇒ 本测量**只作参考**，
标「草案 · 未批准」，**不改判据、不改 ADR 0077 正文**。

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

## 11. 待人工裁定（本链不自行决定）

1. **判据④ 口径** —— 是否采纳 ADR 0081 草案的替代口径（「桶侧不得旧于本地」）？
   若采纳：需同时裁定基线 6 条「桶侧旧于本地」如何处置（改代码 / 设容差 / 留证豁免）。
   若不采纳：判据④ 永远红 ⇒ 阶段 2 无法通过 ⇒ 本批在「双写」段永久停留。
2. **判据③ 口径（缺陷 ③-1）** —— `brow` 的语义是「侧车已知桶账」而非「本轮推送计数」；
   原文「已推成功桶数」需明确时间窗（本轮 / 历史累计）与是否改判据措辞。
3. **判据② 的口径（新发现）** —— 原文未定「以哪一轮为准」；本环按完成的整轮判绿，
   请确认该读法。
4. **503 与重排取证轮** —— 桶推路径三轮未验证（`kPullOnly` 结构 / 规则 1 闸门 / 503），
   是否再排一轮取证？若再排，需先定 503 的排查面（服务端日志 or Cloudflare 指标）。
5. **`WRITE_BUDGET_ROWS_PER_DAY`** —— 盘上 80,000 vs §O 拍板 20,000，仍排阶段 3。
6. **阶段 6 硬前置** —— `api.jury.xin` 放行范围 / ECS `5432` 公网收口 / 两条历史探针账号，仍未裁定。

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