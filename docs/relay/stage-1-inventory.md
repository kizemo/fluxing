# 阶段 1 盘点 — 双写（① 段）常驻取证收口

> 批次：BR-①g 客户端桶化 · 依据 ADR 0077「① 双写」段
> 取证时间窗：2026-11-27 08:03:05 — 09:25:18（本地时钟）；判据④ 取数 09:25:53 — 09:36:54
> 本文件所有数字均为本轮**盘上回读**结果；**全文不含任何凭据值**（只记长度与 sha16 前 16 位）。

---

> **【修复轮补记 · 2026-11-27】** 本文件已被接力链 relay-02of06 阶段 1 修复轮**就地改写**：
> §0 — §8 为**上一环（relay-01of06）**的取证与裁定，**原文保留不动**；
> **§9 为本修复轮**的新事实与新裁定（事实核对 / 926 缺口已收敛 / 规则 1 v2 / F3 / 判据④ 替代口径草案）。
> 修复轮产物：docs/relay/stage-1-legacy-gap-classification.md（926 分类表）、
> docs/adr/0081-cloud-sync-criterion-four-substitute-and-bucket-rule-one-v2-draft.md（判据④ 替代口径 ADR **草案**）。
## 0. 裁定结论（置顶）

**ADR 0077 `:440` 四道判据 = 三绿一红 ⇒ 门禁不过。**

| 判据 | 结论 | 关键数字 |
|---|---|---|
| ① 桶端点请求零 4xx/5xx | 🟢 **绿**（成功轮口径，**间接**证明） | 桶拉取完整扫完 1,000 桶 / 92,353 条、无早退；外部探针 `http_errors=0` |
| ② 桶拉桶数 = 服务端 `sync_buckets` 行数 | 🟢 **绿** | 1,000 = 1,000；`Σitem_count` 92,353 = `pulled_items` 92,353 |
| ③ 侧车 `brow` 行数 = 已推成功桶数 | 🟢 **绿**（**累计**口径） | `brow` 1,000 = 服务端行数 1,000 = 累计已推成功桶数 |
| ④ 两路拉取条目集合逐条 HLC 一致 | 🔴 **红** | `missing_in_bucket=926`、`hlc_mismatch=39043`、`http_errors=0` |

**后果**：

1. `docs/plan-relay.md:57` 的阶段 1 门禁（「四道判据…**全绿才进阶段 2**」）**不满足** ⇒ **不得进入接力链阶段 2**。
2. ADR 0077 `:451` 回退触发条件**命中**（「桶端点漏数据 | 桶端点拉取集合 ⊊ 老端点拉取集合（**任何一轮**）」）⇒ **阶段 3/4（切读 / 退役）仍禁**；回退目标 = ADR 阶段 2（双写+校验），当前即在阶段 1 ⇒ 无回退动作。
3. 本环**停在阶段 1**，台账回写 `status=blocked`，**不起下一环**（`docs/relay/next-relay-prompt.txt` RULES 第 34 行：停链时 never start a next session）。
4. 判据④ 的**根因已部分坐实**（见 §2.4.2）：桶端点**没有**篡改 HLC；红的原因是 **2026-11-26 桶化首导整库重分配了条目 HLC**，老端点仍持 11-24 旧批次 HLC ⇒ **两路永久不可自愈**。

---

## 1. 取证现场（盘上回读）

| 项 | 值 |
|---|---|
| 客户端进程 | `D:\Program Files\fluxing\weasel\WeaselServer.exe` pid **41800**，起于 2026-11-27 **08:02:34**（本轮**从未杀/重启** —— 杀进程会重置 `startup_pending_` 并推迟 kBoth 轮约 62 min） |
| 阶段位来源 | User 级环境变量 `FLUXING_CLOUD_SYNC_BUCKET_PHASE=1`（优先级最高） |
| 侧车 | `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state`，**12,115,139 B** / **94,283 行**，mtime **08:11:00** |
| 侧车字段 | `phase`=**1**（1 行）、`brow`=**1,000 行**、`row`=**93,278 行**、`bseq`=**2027**、`seq`=**71652** |
| 诊断日志 | `%TEMP%\fluxing-profile-notify.log`，**10,306 行** / 736,256 B（共享读法 `File.Open(Open,Read,ReadWrite)`） |
| 凭据口径 | Windows 凭据管理器目标 `Fluxing:CloudAccount`，`token_len=43`、`token_sha16=544318da433b2d58`、`email_len=15`（**值全程未落盘 / 未入日志 / 未入本文件**） |

### 1.1 取证窗口内的三轮现场（逐字摘自诊断日志）

| 轮 | 时间 | 方向 | 关键诊断行（逐字） |
|---|---|---|---|
| 启动轮 | 08:03:05 → **08:11:00** | kPullOnly | `reconcile scan_complete=0 matched=92272 deferred=1311 divergent=10 pushed=0`<br>`bucket phase=1 pushed=0 skipped=0 pulled_buckets=1000 pulled_items=92353` |
| **失败轮** | 09:11:02 → **09:14:55** | kBoth | `reconcile scan_complete=0 matched=0 deferred=0 divergent=0 pushed=0 early_exit=bucket_pull_failed`<br>`bucket phase=1 pushed=0 skipped=0 pulled_buckets=800 pulled_items=73921` |
| **成功轮（判据基准）** | 09:15:57 → **09:25:18** | kBoth | `bucket quota held_unproven=0 held_quota=0 rows_pushed=1928 rows_held=0 quota_rows=2000`<br>`reconcile scan_complete=0 matched=92272 deferred=1311 divergent=0 pushed=0 early_exit=unattributed`<br>`bucket phase=1 pushed=896 skipped=0 pulled_buckets=1000 pulled_items=92353` |

> 启动轮 `divergent=10`、成功轮 `divergent=0`：两轮都有 `early_exit=*`，但**桶拉取本身都完整**（`pulled_buckets=1000` / `pulled_items=92353`）⇒ 差异项属 reconcile 侧口径，**不是桶端点数据差异**。判据④ 另用外部逐 key 比对独立复核（§2.4）。

---

## 2. 四道判据逐条裁定

判据原文（ADR 0077 `:440` 逐字）：

> ① 桶端点请求零 4xx/5xx；② 桶端点拉取的桶数 = 服务端 `sync_buckets` 该用户行数；③ 客户端侧车 `brow` 行数 = 已推成功桶数；④ 两路拉取的**条目集合逐条 HLC 一致**（允许桶端点多条目，不允许少）

### 2.1 判据① — 桶端点请求零 4xx/5xx → 🟢 绿（成功轮口径，间接证明）

- **客户端侧**：成功轮桶拉取**完整走完**（1,000 桶 / 92,353 条），**无 `bucket_pull_failed` 早退**；桶推同轮完成（`rows_pushed=1928`、`held_quota=0`、`held_unproven=0`）。
- **外部只读探针**（`phase1-503-diag-20261127.txt` 逐字）：
  - `GET /v1/sync/buckets/pull?limit=100` → **200**，body_len **3,550,698 B**；`limit=1000` → **200**，body_len **3,550,698 B**（与 limit=100 **同长** ⇒ 服务端把 limit 夹到 100/页，ADR 0077 预期行为）；
  - `GET /v1/sync/pull?limit=100` → **200**，body_len **33,030 B**；`limit=1000` → **200**，body_len **33,030 B**（同样被夹）；
  - 无 bearer → **401** `{"error":{"code":"missing_token"}}`；伪造路由 `/v1/__probe__` → **404** `{"error":{"code":"not_found"}}`
    ⇒ **应用层活着、鉴权层正常**，先前 503 **不是**请求形态或鉴权问题。
- **证据边界（必须写明）**：客户端诊断日志**没有桶端点的独立 HTTP 状态行**（只有老端点 `push batch … http=200` 与 `bucket phase=` 汇总行）⇒ 判据① 是**间接证明**：桶端点若返 4xx/5xx，客户端会 `early_exit=bucket_pull_failed` 中断；成功轮**未中断且扫完 1,000 桶** ⇒ 该轮桶端点请求无 4xx/5xx。失败轮的失败性质未被客户端记录（见 §4 F2）。
- 外部探针与判据④ 脚本均统计 `http_errors=0`。

### 2.2 判据② — 桶拉桶数 = 服务端 `sync_buckets` 该用户行数 → 🟢 绿

| 侧 | 值 | 来源 |
|---|---|---|
| 客户端桶端点拉取的桶数 | **1,000** | 成功轮 `bucket phase=… pulled_buckets=1000` |
| 服务端 `sync_buckets` 该用户行数 | **1,000** | 生产 D1 只读 SELECT |
| 服务端 `Σ(item_count)` | **92,353** | 同上 |
| 客户端该轮拉到的桶内条目数 | **92,353** | 成功轮 `pulled_items=92353` |

- **1,000 = 1,000 ⇒ 相等**；**92,353 = 92,353 ⇒ 条目数也对齐**（两个独立来源互印）。
- 附带：侧车 `seq=71652` 与 D1 `sync_items` 的 `MAX(id)=71652` **数值相同** ⇒ 提示本地条目级游标已追平服务端条目表最新 id（**推断**，非判据项）。

### 2.3 判据③ — 侧车 `brow` 行数 = 已推成功桶数 → 🟢 绿（累计口径）

| 侧 | 值 |
|---|---|
| 侧车 `brow` 行数 | **1,000** |
| 服务端 `sync_buckets` 该用户行数（= 累计已推成功桶数） | **1,000** |
| 侧车 `brow` 首键 | `0038b32337217f3cac6604ed2481cd66affcc1c4f7872733117853d70dd8a401` |
| 服务端桶端点第 1 页首 `bucket_key` | `0038b32337217f3cac6604ed2481cd66affcc1c4f7872733117853d70dd8a401`（逐字相同） |

**口径取「累计」而非「本轮」的判别依据**：

1. ADR 0077 `:266` 定义 `brow` 为**桶行**（`bucket_key` / `ms` / `counter` / `device` / `item_count` / `fingerprint`）⇒ 它是**持久化的桶集合记录**，天然累计。
2. ADR 0077 `:331` 与 `:512` 明确「**没推成功的桶不写 `brow`**，下一轮自然重试」⇒ `brow` 行数 ≡ **累计已推成功桶数**。
3. 故等式两侧同为累计量：**1,000 = 1,000**。
4. 「本轮 `pushed=896`」是**单轮增量**，与累计量不同口径，**不得相减判红**。896 < 1,000 的差额来自服务端 HLC 守卫 / 已存在同值跳过等逐桶判定（`bucket quota` 行 `held_quota=0`、`held_unproven=0`、`rows_held=0` ⇒ **没有**桶被配额或未证实库存扣住）⇒ 差额**不是**「推失败」。
5. 第三方印证：侧车 `brow` 首键与服务端桶端点首 `bucket_key` **逐字相同** ⇒ 侧车与云端桶集合同源。

### 2.4 判据④ — 两路拉取的条目集合逐条 HLC 一致 → 🔴 **红**

#### 2.4.1 比对方法与结果

- **老端点侧** = 生产 D1 `sync_items` **只读** SELECT（`item_key` / `hlc_ms` / `hlc_counter` / `hlc_device` 为明文列），按 id 区间**分块 4,000** 取，共 **18 块 / 40,112 行**（`MAX(id)=71652`、`dup_item_keys=0`、该用户行 **40,112**）。
- **桶端点侧** = `GET /v1/sync/buckets/pull` **全量分页 10 页**（1,000 桶 / **92,354** 信封 / unique 92,354 / `item_count_mismatch_buckets=0` / `dup_keys_across_buckets=0` / `http_errors=0`）；桶 blob 为 8 行信封块（`v1` / `item_key` / `type` / `hlc_ms` / `hlc_counter` / `device` / `nonce` / `cipher`）⇒ 条目键与 HLC 为明文，无需解密载荷。
- **交叉验证（证明 D1 ≡ 老端点）**：`GET /v1/sync/pull?since=0|15000|30000` 各 100 条，共 **300 条** ⇒ `d1_missing=0`、`hlc_mismatch=0`、`http_errors=0` ⇒ **D1 取数正确**，红不是老端点侧取数造成的。

| 指标 | 值 | 判据要求 |
|---|---|---|
| 老端点侧条目数 | 40,112 | — |
| 桶端点侧条目数 | 92,354 | — |
| **桶端点缺失的 key（老端点有、桶端点无）** | **926** | **不允许 > 0 ⇒ 红** |
| **HLC 不一致的 key** | **39,043** | **不允许 > 0 ⇒ 红** |
| 桶端点多出的 key（允许） | 53,168 | 允许 |
| 桶端点 HTTP 错误数 | 0 | 0 ✔ |

明细逐条落盘：`reports/fluxing-br-1g/phase1-judge4-hlc-compare-20261127.md`（缺失 50 条 + HLC 不一致 50 条样本，含 `legacy_hlc=` / `bucket_hlc=` 逐字值）。

#### 2.4.2 根因定性（已坐实部分）

**`hlc_mismatch=39043` 的机制 = 2026-11-26 桶化首导「整库重分配条目 HLC」，不是桶端点篡改数据。**

证据链（三段，全部盘上回读）：

1. **不匹配样本的 HLC 呈严格规律**：`legacy_hlc=1795339039468:14944:c7cc011f1b29de53` vs `bucket_hlc=1795565189774:14945:c7cc011f1b29de53`、`…:14945` vs `…:14946`、… ⇒ 桶侧 `ms` 恒为 **1795565189774**（2026-11-26），`counter` 与 legacy **逐条 +1** 连续 ⇒ 桶侧是一条**新分配**的连续序列，不是原值被改写。
2. **本地侧车 `row` 行的 ms 与桶侧 ms 逐字相同**：侧车样本 `row rime_ice\tA 1795565189774 0 c7cc011f1b29de53 s2:a54e3ad7…` ⇒ 本地当前条目 HLC 的 `ms` = **1795565189774**、device = `c7cc011f1b29de53`，**与桶内 HLC 同源同值** ⇒ 桶推**沿用了客户端当前的条目 HLC**，桶端点**没有**重分配或篡改。
3. **老端点侧仍是旧批次**：D1 `sync_items` 的 `hlc_ms` 恒为 **1795339039468**（2026-11-24 推送批次）⇒ 差异源于**两次独立写入事件**，不是同一份数据被改。

**推论（已坐实）**：11-26 桶化首导给全部本地条目重新分配了 HLC 并写入桶端点；老端点**永久**持有旧 HLC ⇒ **两路 HLC 差异不会自愈**（除非回退到阶段 0 重推，或让桶推沿用老端点旧 HLC —— 后者会与本地侧车不一致）。这与 §P-O「整库还原 ⇒ 指纹全变 ⇒ 全量重推」的写放大链在 **HLC 层面**同源：**整库重分配 ⇒ 每轮都判为「本地改动」**。

**`missing_in_bucket=926` 的性质 = 未判定。**

- 已排除「脚本口径错误」：信封解析为 8 行块、`item_key`/`hlc_ms`/`hlc_counter`/`device` 均取自条目级字段；交叉验证证明 D1 ≡ 老端点（`d1_missing=0`、`hlc_mismatch=0`）。
- **未能判定的原因**：侧车 `row` 行的键是**文本键**（`rime_ice\tA` 形式，词典名 + 转义条目名），而云端 `item_key` 是 **sha256 hex** ⇒ **两侧 key 空间不同**，无法直接做集合包含判断；建立映射需要额外确认云端 `item_key` 的 hash 口径（**本环未做**）。
- **旁证（倾向「非大规模漏本地」）**：客户端 reconcile 侧 `matched=92272` / `deferred=1311` / `divergent=0`（桶端点 92,353 条 vs 本地）⇒ 桶端点内容与**本地**高度一致（99.9%），**未见**「本地 93,278 条里大批没上桶」的迹象；缺失样本的 legacy HLC 多为 11-24 批次 `ms=1795339039468`，其中最早一条为 `1794411631262:0:a651f3d8fcb3ab8a`（**另一台设备**的最初批次）⇒ 与「老端点侧留存已删条目（僵尸）」相容，但**未证实**。
- **⇒ 登记为下一环/人工裁决项**：需要「侧车文本键 ↔ 云端 `item_key`」映射口径，才能把 926 条拆成「本地已删僵尸」与「本地现存未上桶」两类。

---

## 3. 回退触发条件核查（ADR 0077 `:445-454`，任一命中即须回退）

| 触发条件 | 原文判据 | 本轮实测 | 命中 |
|---|---|---|---|
| 桶端点错误率升高 | 连续 **3 轮**出现桶端点 4xx/5xx | 窗口内桶端点失败**仅 1 轮**（09:11 轮），且客户端未记录 HTTP 状态码 ⇒ 未达「连续 3 轮」 | **否** |
| 交叉校验差异 | 差异明细非零且连续 2 轮无法定位 | ADR 的 ② 段判据，尚未进入该段 | **否** |
| **桶端点漏数据** | 桶端点拉取集合 ⊊ 老端点拉取集合（**任何一轮**） | 判据④：`missing_in_bucket=926` ⇒ **桶端点 ⊊ 老端点** | **🔴 是** ⇒ 回退目标 = ADR 阶段 2（双写+校验），当前即阶段 1 ⇒ **无回退动作**；但 **阶段 3/4 仍禁** |
| 写放大回归 | `sync_buckets` 写入增量 > 变更桶数 × 2（稳态轮） | 本轮 `rows_pushed=1928`；896 × 2 = **1,792** ≤ 1,928（差 136 为往返记账差）⇒ 未超 | **否** |
| 库存扫不完 | 16 页内 `has_more=true`（或总桶数 > 1600） | 桶端点 **10 页**内扫完 1,000 桶（`has_more=false`），1,000 < 1,600 | **否** |
| 预算护栏误伤 | 桶端点推触发 429 `write_budget_exceeded` 且非真实超额 | 无 429；`held_quota=0` / `rows_held=0` | **否** |

**结论：六条中命中 1 条（桶端点漏数据）⇒ 阶段 3/4 切读/退役红线不变。**

---

## 4. 附带发现（不单独阻塞，但须带入下一环）

### F1 — 服务端 503 归属未定案

- 现象：`phase1-judge4-hlc-compare.ps1`（v1）首跑时**两侧端点同时返回 503** ⇒ 其 `JUDGE4_VERDICT=FAIL` 为**假数据**，不得引用。
- 已排除：请求形态（limit 被服务端夹到 100/页）、鉴权（无 bearer ⇒ 正常 401）、`devices.last_seen` 刷新（`src/auth.ts` 的 last_seen 写入已 **fail-open**，失败不再打成 503）。
- 仍候选：D1 配额 `code 7500` → 503 `storage_quota_exceeded`、其它存储故障 → 503 `storage_unavailable`、Cloudflare 边缘超时。
- **旁证**：worker 单请求 `cfWorker;dur≈19,229–19,607 ms` ⇒ worker/D1 极慢（约为健康值的一个数量级）。
- **状态：未定性** —— 未取得 503 响应体与服务端日志 ⇒ **不得据此宣称根因**。

### F2 — 失败轮的桶拉取失败原因未被客户端记录

- 09:11 轮：`early_exit=bucket_pull_failed`，桶端点拉到 **800** 桶 / **73,921** 条后中断（第 9 页起），该轮**桶推为 0**。
- 客户端**没有**桶端点失败状态行 ⇒ 无法判定是 4xx/5xx 还是网络/超时。
- 影响：判据① 只在成功轮成立；失败轮不计入「零 4xx/5xx」的正面证据。

### F3 — 客户端缺桶端点 HTTP 状态诊断行（可观测性缺口）

- 日志里只有老端点 `push batch items=… http=200`，**无**桶端点对应状态行 ⇒ 任何桶端点错误只能从 `bucket phase=` 汇总行反推。
- 建议（后续批次候选改动，**本环未做**）：补一行桶端点 `pull/push … http=<code>`，使判据① 从间接证明升级为直接证据。

### F4 — 侧车 `row` 键与云端 `item_key` 不同 key 空间

- 侧车 `row` 用**文本键**（`词典名\t条目名`，如 `rime_ice\tA`），云端 `item_key` 用 **sha256 hex** ⇒ **无法**直接做「本地 ⊆ 云端」的集合包含判断。
- 这是判据④ `missing_in_bucket=926` 无法定性的直接原因；也意味着**任何**以本地侧车为准的云端覆盖率断言都必须先补 hash 口径。

---

## 5. 已验证 / 推断 / 未验证

**已验证（盘上证据）**
- 四道判据的成功轮数值、侧车字段与 `row` 样本、D1 只读计数、HTTP 状态码矩阵、逐 key HLC 比对全量结果、回退条件逐条核查。
- 客户端进程与阶段位在整个取证窗口内未被干预（未杀、未重启）。
- 桶推沿用本地当前条目 HLC（侧车 `row` 的 ms/device 与桶内 HLC 逐字相同）。

**推断（证据一致但非直接证明）**
- 侧车 `seq=71652` = D1 `sync_items` `MAX(id)` ⇒ 本地条目级游标已追平服务端。
- 本轮 `pushed=896` 与累计 1,000 的差额来自服务端侧逐桶判定（`held_*=0` ⇒ 非配额扣留）。
- 926 条缺失**倾向**「老端点侧僵尸条目」而非「桶端点漏本地条目」（依据：reconcile `matched=92272`/`divergent=0` 与缺失样本的 legacy 批次分布）。

**未验证**
- 926 条缺失的**确切性质**（需 F4 的 hash 口径）。
- 09:11 失败轮的失败**性质**（4xx/5xx vs 网络层）。
- 服务端 503 的**根因**（F1）。
- **跨设备可见性**（判据④ 只比对同一账号的云端两路数据，**未**做第二台设备的端到端可见性验证 —— 那是阶段 3 的判据）。
- **AC-09 仍为 FAIL**（本环未改记，不得引用为通过）。

---

## 6. 证据文件清单

| 文件 | 作用 | 字节 |
|---|---|---|
| `reports/fluxing-br-1g/phase1-kboth-round3-20261127-evidence.log` | 成功轮（09:25:18）三行判据逐字记录 | 3,922 |
| `reports/fluxing-br-1g/phase1-enter-20261127-evidence.log` | 失败轮（09:14:55）记录 | 589 |
| `reports/fluxing-br-1g/phase1-503-diag-20261127.txt` | HTTP 状态码矩阵 + 响应头耗时 | 6,250 |
| `reports/fluxing-br-1g/phase1-judge4-hlc-compare-20261127.md` | 判据④ 逐 key HLC 比对报告（含差异样本各 50 条） | 17,149 |
| `reports/fluxing-br-1g/phase1-judge4-hlc-compare-20261127.txt` | 判据④ 运行日志 | 3,320 |
| `reports/fluxing-br-1g/phase1-judge4-hlc-compare-v3.ps1` | 判据④ 取数脚本（D1 分块 + 桶端点分页 + 3 页交叉验证） | 19,714 |
| `reports/fluxing-br-1g/phase1-503-diag.ps1` | HTTP 状态码矩阵探针 | 4,964 |
| `reports/fluxing-br-1g/watch-kboth-round.ps1` | kBoth 轮证据守候器（自带落盘判据 + 心跳） | — |
| `docs/relay/stage-1-inventory.md` | 本文件 | — |

> `reports/**` 按项目约定**不入库**；上述文件作为盘上证据保留在本机工作树。

---

## 7. 停链原因与需人工裁决项

### 7.1 停链原因

`docs/plan-relay.md:57` 的阶段 1 门禁要求四道判据**全绿**；判据④ 红 ⇒ 门禁不过 ⇒ 依 `docs/plan-relay.md:41`「阶段不得自动前进，验收不过就停在当前段」，本链**停在阶段 1**。

### 7.2 需人工裁决的三项（**建议按 A → B → C 顺序**）

**A. 桶推 HLC 口径 —— 是否要求「桶推沿用老端点已存在的条目 HLC」？**
- 现状：11-26 桶化首导整库重分配 HLC（`ms` 统一为 1795565189774），桶端点 = 本地当前 HLC，老端点 = 11-24 旧 HLC ⇒ 判据④ 结构性不可满足。
- 选项 A1：**改桶推口径**（推桶时若本地条目已有老端点 HLC 则沿用）⇒ 两路 HLC 一致，代价：需服务端/客户端协议与本地侧车同时改，且需重推一轮。
- 选项 A2：**修订判据④**（把「逐条 HLC 一致」改为「同一 key 两侧 HLC 可比且桶端点不旧于本地」等口径）⇒ 需独立 ADR 修订 ADR 0077 `:440`。
- 选项 A3：**接受现状**并把 ADR 0077 `:451` 的回退触发条件改写为「桶端点 ⊉ 老端点**活跃**条目集合」。

**B. 926 条缺口定性 —— 是否先补「侧车文本键 ↔ 云端 `item_key`」映射口径？**
- 未补则无法区分「本地已删僵尸」与「本地现存未上桶」；后者属**真实漏数据**，优先级远高于前者。
- 建议：作为**独立小批次**先落 hash 口径（客户端侧一条诊断/工具即可），再重跑判据④。

**C. 是否授权进入接力链阶段 2（规则 1 盲区修复）？**
- 本环按门禁**未**推进（`current_stage` 保持 1，`status=blocked`）。
- 若人工裁定 A/B 的修复方案**不在**阶段 2 写集内（`inventory_complete` 死分支），则「进阶段 2」不能使判据④ 转绿 ⇒ 建议**先按 A/B 立项，再决定是否放行阶段 2**。
- 若裁定「判据④ 口径修订即可放行」，则可人工改 `current_stage` 或直接授权本链推进。

### 7.3 红线（不变）

阶段 3/4 永不启用、阶段不得自动递增、规则 1 盲区未修前不得切读、生产 D1/R2/Cloudflare 只读、不动用户词典数据、AC-09 仍 FAIL 不得改记、凭据值绝不进日志/输出/提交物。

---

## 8. 阶段 2 前置条件（若人工放行）

1. **规则 1 盲区（`inventory_complete` 死分支）必须先修** —— 阶段 3/4 在此之前**仍禁**。
2. A/B 两项裁决落盘（各自独立 ADR），必要时修订 ADR 0077 `:440` 的判据④ 口径。
3. F1（503 根因）建议在阶段 2 期间取服务端日志定性，避免阶段 3 的「一轮扫完」判据被服务端抖动击穿。
4. F3（桶端点 HTTP 状态行）可与阶段 2 的改动一并落地。
5. **阶段不得自动递增** —— 阶段 2 的进入需人工显式放行。

---

## 9. 修复轮（relay-02of06 · 2026-11-27 10:03 — 11:xx · 本批最新事实，先读这里）

> §0 — §8 是**上一环（relay-01of06）**的取证与裁定，**原文保留**（含当时的红判据④），
> 不要删改；本节记录**修复轮**的新事实与新裁定。
> 本轮写集：`WeaselServer/Cloud/**` + `test/TestFluxingCloud/**` + `docs/adr/**`（新 ADR 草案）
> + `docs/relay/**`；**零生产写**（生产 D1 只 SELECT、侧车只读、本地词典只读）。

### 9.1 人工裁决与本环任务（2026-11-27 09:5x）

| 裁决 | 结论 | 本环动作 |
|---|---|---|
| **A**（桶推 HLC 口径） | **先事实核对（A-0）**；本轮**既不改判据、也不动生产** | 产出事实结论 + 判据④ **替代口径 ADR 草案**（`docs/adr/0081-…-draft.md`），**不改 ADR 0077** |
| **B**（926 条缺口定性） | **立即开工，并入阶段 1 修复轮**（全程只读） | 产出 `docs/relay/stage-1-legacy-gap-classification.md` |
| **C**（是否放行下一段） | **放行，但重排链的阶段序列**（新链 6 段） | 本环 = 阶段 1 修复轮 |

### 9.2 A-0 事实核对结论（已坐实）

1. **R1（远端 HLC 沿用）没有被绕过** —— 桶推路径 `PushChangedBuckets` **不分配条目 HLC**，
   直接消费对账循环产出的 `merged` HLC（R1 命中 ⇒ 沿用远端 HLC；R4 分歧/本地新增 ⇒ `HlcTick`）。
   ⇒ 判据④ 的红**不可能**由「R1 被绕过」解释。
2. **侧车 HLC 写回 = 无条件沿用**（远端更旧也照搬）；`HlcObserve` 只抬高**后续新分配**。
   ⇒ 这直接解释 `hlc_mismatch=39043`：legacy 路径每轮把远端旧 HLC 写回侧车，
   侧车被拖旧；而桶路径拿到的是较新的桶级 HLC。
3. **判据④替代口径在构造上成立**：对「桶侧与本地侧车**同时存在**」的键，
   「桶侧条目 HLC == 本地侧车同 key HLC」应当成立（绕开 legacy 的拖旧效应，
   直击切读真正关心的事）。
4. **A1 三条替代路线均超红线，本链不做**：回退 93,278 行 / pull 让旧 HLC 覆盖 /
   一次性迁移桶表 ≈ 190,000 行写入 ⇒ 全部超写预算与数据正确性红线。

详见 ADR 0081 草案 §2。

### 9.3 B 定性结果：**926 条缺口已收敛为 1 条**

| 项 | 09:28（上一环判据④ v3） | **10:48（本环）** |
|---|---|---|
| legacy 键数 | 40,112 | 40,112 |
| 桶侧键数 | 92,354 | **94,589** |
| legacy 键不在桶侧（缺口） | **926** | **1** |
| 分类 | — | `A=0` / `B_local_unknown=1` / `C=0` |

剩余 1 条：`cd3cc4e9…865f`（legacy_hlc `1794411631262:0:a651f3d8fcb3ab8a`）
⇒ 本地侧车查不到 ⇒ **疑似本地已删、云端残留的僵尸行**。

**归因边界（未坐实）**：桶侧键数增长与客户端 09:25 / 09:41 两轮桶推送时间相关，
但「legacy 键进入桶侧」的具体机制本轮**未逐条坐实** ⇒ 登记待坐实，**不得当结论**。

对照数字（盘上回读 `tmp/relay02/gap-summary.txt`）：

```
legacy_vs_sidecar_legacy_older=39962   legacy_vs_sidecar_equal=149   legacy_vs_sidecar_unmapped=1
bucket_vs_sidecar_equal=93279          bucket_vs_sidecar_bucket_newer=0
bucket_vs_sidecar_bucket_older=0       bucket_vs_sidecar_unmapped=1310
map_variant=unescaped                  http_errors=0
```

⇒ **桶侧 vs 侧车：两侧都存在的 93,279 个键，HLC 逐字相等**（newer=0、older=0）
⇒ **判据④替代口径在 10:48 成立**。
⇒ `bucket_vs_sidecar_unmapped=1310` 与真机 `deferred=1310` 逐字吻合 ⇒ 两侧口径自洽。

### 9.4 客户端实现层修复（已落码 + 编译通过）

| 项 | 旧 | 新 |
|---|---|---|
| **规则 1 闸门** | `if (!inventory_complete && BucketHasUnprovenEntry(unproven_keys, entries))`；`inventory_complete = remote_complete = bucket_scan_complete` ⇒ **恒假 ⇒ 从未生效**（真机 `held_unproven=0` 实证） | `if (BucketRemoteViewIncomplete(incomplete_buckets, bucket_key)) { ++held_unproven; continue; }` —— 证据取自**桶空间**：本轮拉到该桶、但解出条数 != 服务端 `item_count` / 解包失败 / 桶内畸形 ⇒ 整桶压制 |
| **判定落点** | 散在 .cpp | 收敛到**生产头** inline `BucketRemoteViewIncomplete()`（生产与单测同源，宪法 R7） |
| **`unproven_keys`** | 桶路径消费（实际恒空） | **保留**于 legacy 编号空间（legacy R2 延后语义不变），加注释「桶路径不再消费」 |
| **F3 可观测性** | 桶端点 HTTP 状态**完全不可观测** | `PullBuckets` 增 `int* http_status` 出参；桶拉取/桶推送各打一条**无条件**诊断行：`bucket pull page=.. http=.. buckets=.. items=.. has_more=..` / `bucket push page=.. http=.. buckets=.. accepted=.. skipped=..`（前缀与 `bucket phase=` / `bucket quota ` 不冲突 ⇒ 探针 v4 正则不破） |

**为什么证据必须取自桶空间**：服务端按 `bucket_id` **先 DELETE 后 INSERT**
⇒ 未列出的远端条目被**静默删除** ⇒ 风险方向是「服务器有、我们没读到」；
legacy `unproven_keys` 方向相反且两套编号空间**互不参照** ⇒ 混用会指向错误方向。

**规则 1 v2 的残留缺口（留档，不得声称已根治）**：阶段 1/2 的桶拉取是**增量**
（`since=bucket_seq_`），本轮**没拉到**的桶缺当轮证据 ⇒ 当前只对「拉到但视图不完整」
的桶压制。**禁止**把 `inventory_complete` 改成 legacy 库存完整性
（`!has_more` 在阶段 1/2 常态为 true ⇒ 会压制几乎所有桶）。

### 9.5 本轮订正的既有记载

- **handoff §P-H.4「brow 目前不落盘」⇒ 订正**：`brow` **已落盘**，实测 **1,024 行**
  （侧车标签直方图 `brow:1024`）。ADR 0077 中同源注释需在后续批次同步修订
  （**本轮未改 ADR 0077**）。

### 9.6 判据④ 的当前裁定状态

| 判据 | 上一环（09:28） | 本环（修复轮后） |
|---|---|---|
| ① 桶端点零 4xx/5xx | 🟢 | 🟢（`http_errors=0`，11 页全 200） |
| ② 桶拉桶数 = 服务端行数 | 🟢 | 🟢（1,000 = 1,000） |
| ③ `brow` 行数 = 已推成功桶数 | 🟢 | 🟢（`brow` 1,024 ≥ 1,000） |
| ④ 两路拉取条目集合逐条 HLC 一致 | 🔴 | 🔴 **原文未改、仍按红记**；**替代口径**在 10:48 成立（93,279/93,279 相等）⇒ **待人工批准口径修订** |

⇒ **阶段 1 门禁（单独 msbuild + gate-p0-run.ps1）已绿**（见 §9.8：PASS=1014 / FAIL=0 / UFFFD_COUNT=0 / CLANGFORMAT_HUNK_DRIFT=0）⇒ **进入阶段 2（真机复核轮）**；但 **判据④ 口径修订仍待人工批准**（ADR 0081 = Draft）⇒ 阶段 2 按**原文口径**裁定并如实记红，草案口径只作参考测量。

### 9.7 红线（不变）

阶段 3/4 永不启用、阶段不得自动递增、规则 1 残留缺口未修前不得切读、
生产 D1/R2/Cloudflare 只读、不动用户词典数据、`AC-09` 仍 FAIL **不得改记**、
`AC-212` / `AC-215②` 未判定、`AC-213` / `AC-214` 永久退役、凭据值绝不进日志/输出/提交物。

### 9.8 门禁结果（盘上回读 · 2026-11-27 11:08）

| 指标 | 值 |
|---|---|
| `PASS` | **1014**（基线 951 ⇒ **+63**，含本轮新增鉴别力用例） |
| `FAIL` | **0** |
| `UFFFD_COUNT` | **0** |
| `CLANGFORMAT_HUNK_DRIFT` | **0**（改动 hunk 口径） |
| `CLANGFORMAT_WHOLEFILE_DRIFT` | 3（**HEAD 存量**整文件口径，含本轮未改动的 `CloudRimeUserDictBackend.cpp` ⇒ 不做整文件重排） |
| 门禁日志 | `reports/fluxing-br-1g/gate-relay02-20261127-110813.log`（703 B · **不入库**） |
| 测试 exe | `Release/TestFluxingCloud.exe`（1,072,128 B · mtime 2026-11-27T11:08:11） |

**门禁前置条件（本轮新增工具坑）**：跑门禁前必须先
`$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'` —— 本机 User 级该变量常驻 `1`
（客户端真机跑阶段 1 双写）⇒ **被测试进程继承** ⇒ 全部 legacy 用例误入桶模式
⇒ **89 项假失败**（`PASS=922 FAIL=89`，首个失败组 =「BL 协调器」）。置 `0`
（= 老端点单跑）后**同一次构建** `PASS=1014 FAIL=0` / `EXE_EXIT=0`
⇒ **实现层无回归**，假红来自环境变量泄漏。`gate-p0-run.ps1` 本身不清环境
⇒ 每轮跑门禁前必须自行压掉该变量。

**鉴别力门禁用例**：`TestBucketRule1RemoteViewGate`（好样本：`pushed=1` /
`held_unproven=0` / `rows_pushed=2` / 侧车写出 `brow`；坏样本：`pushed=0` /
**`held_unproven=1`** / `rows_pushed=0` / 侧车**不**写 `brow`）⇒ 规则 1 v2 闸门在
**生产决策路径**上有鉴别力（两样本方向相反），不是 0 命中的假绿。