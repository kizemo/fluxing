# 复核立项书 · 写预算护栏的计费口径

- 日期：2026-11-27（阶段 2 · 写预算阈值超限负样本自证轮之后）
- 范围：`cloud/fluxing-sync` 的**写预算护栏**（ADR 0076 D2-3 / D8）与**桶端点预算**（ADR 0077 D8）
- 触发：自证轮实测 `19,999 行 / 5 语句 ⇒ Rows written 80,001`，与护栏口径 `1 / 2` 相差 4–5 倍
- 状态：**待拍板**。本轮**零代码改动、零迁移、零部署、零生产写**
- 上游证据：`docs/relay/stage-2-wb-selfproof-20261127.md`；裁定：`docs/relay/stage-2-rulings-20261127.md` §5
- **【2026-11-27 15:47 降级 · 用户裁定「摘掉阶段 3，先修 P0 假空再切源」】** 本文 = **D1 迁移期历史测算**，不是现行待办。**保留有效**：§1 的 `80,001` / 每行 4 计费行 / 新 5 · 重推 9 的实测数字（写放大 9× 的原始标定）。**随 D1 退役作废**：§3 的方案 A / B1 / B2 对比、§4 的 ADR 0082 草案、§5 的 spec 104 判据 AC-1..AC-9、§6 六步部署路径、§7 生产阈值影响 —— 全部不再立项（ADR 0082 与 spec 104 **从未创建**）。接力阶段 3 槽位已改载 **P0 假空修复**，见 `docs/plan-relay.md` §2。

---

## 0. 一页结论

1. **成本模型已从源码坐实**：产品推送真实写形态是 **`DELETE` + `INSERT`**（不是 `UPDATE`），`sync_items` 上有 **3 条二级索引** 且 `id` 是 **`AUTOINCREMENT`**。
   - 新条目 = **5 计费行**（INSERT 4 + `sqlite_sequence` 1）
   - 重推已存在条目 = **9 计费行**（DELETE 4 + INSERT 4 + seq 1）
   - 现护栏按 **1 / 2** 记账 ⇒ **低估 5× / 4.5×**。

2. **`src/env.ts:45-48` 的「落 20,000 = 单账号当日最多 20%」是错的**：真实最坏是 **100%**（20,000 条全新）或 **90%**（10,000 条重推），即一次合规推送即可吃满 D1 免费日额度。

3. **比放大 5× 更严重的是护栏可被无限绕过**：`used = COUNT(*) WHERE updated_at >= 今日00:00UTC`（`src/sync.ts:202-214`）计量的是「今日被写过的**当前行**数」，**不是写入次数 ⇒ 非单调**。同一批已存在条目反复重推时 `used` 恒定、`planned` 恒定 ⇒ `used + planned ≤ limit` **永真**。源码自己在 `src/sync.ts:93-96` 承认「本护栏只会低估」。
   - 算例：稳定工作集 6,666 条，`used+planned = 19,998 ≤ 20,000` **每轮都放行**，每轮真实写 59,994 计费行 ⇒ **两轮即打穿 100,000**。这不需要恶意，正常「反复同步同一批笔记」即可触发。

4. **方案 A（只把 20,000 下调到 5,000）不修结构缺陷**：它把可绕过的工作集从 6,666 降到 1,666、把打穿轮次从 2 轮推到 7 轮，但仍可打穿；且「逻辑行」这个量纲对用户与运维都不可解释。

5. **推荐 B1**：改按 **D1 计费行** 口径 + **`write_usage` 原子账本**（`PRIMARY KEY(user_id, day, scope)`，点读 O(1)，成功写入后 `ON CONFLICT DO UPDATE SET rows = rows + excluded.rows`）+ **索引数门禁**（迁移里 `ON sync_items` / `ON sync_buckets` 的索引计数必须等于 3，否则测试变红，防止未来加索引不改成本常量）。阈值**仍为 20,000**，此时「20,000 = 单账号 ≤20%」**第一次真正成立**。

6. **A 与 B1 不是二选一**：B1 修的是**语义**（该不该计写入次数），阈值只是**刻度**。采纳 B1 后仍可在 12,000 / 20,000 / 25,000 之间单独选。B1 取 20,000 计费行 ⇒ 约 **4,000 新条目/日** 或 **2,222 次重推/日**。

7. **必须同时登记的硬张力**（不因方案选择而消失）：40,061 条**首次全量上云 = 200,305 计费行 > 100,000 日额度** ⇒ **免费 D1 当天装不下一次 4 万条首导**，与护栏阈值无关。现状（4× 低估）同样装不下（160,244）。唯一出口是桶化路径（阶段 3，**当前仍禁**）或 Workers Paid（**已否决**）。

---

## 1. 成本模型：D1 计费行 vs 逻辑行

### 1.1 D1 的计费单位是「行写入」，含索引维护

Cloudflare D1 的 `rows_written` 统计的是**实际写入的表行 + 索引条目**，不是「业务对象个数」。一条 `INSERT` 进带 N 条二级索引的表，计费 ≈ `1 (表行) + N (索引条目)`；`DELETE` 对称；`AUTOINCREMENT` 额外维护 `sqlite_sequence`。

### 1.2 实测对账（坐实 4 行/条 + 1 行/语句）

自证轮种子：**19,999 行**、**5 条 `INSERT` 语句**（每条 4,000 行）。D1 自报 `Rows read: 20019` / **`Rows written: 80001`**。

```
19,999 × 4 = 79,996
79,996 + 5（每条 INSERT 语句维护一次 sqlite_sequence）= 80,001   ← 与 D1 自报逐字相等
```

⇒ **每行 = 1 表行 + 3 索引条目 = 4**；**每条 `INSERT` 语句额外 +1**（`sqlite_sequence`）。

`sync_items` 的索引全集（盘上）：

| 来源 | 对象 |
|---|---|
| `migrations/0001_init.sql:58` | `id INTEGER PRIMARY KEY AUTOINCREMENT`（触发 `sqlite_sequence`） |
| `migrations/0001_init.sql:71` | `CREATE UNIQUE INDEX idx_sync_user_item ON sync_items(user_id, item_key)` |
| `migrations/0001_init.sql:73` | `CREATE INDEX idx_sync_user_id ON sync_items(user_id, id)` |
| `migrations/0003_sync_items_updated_index.sql:22` | `CREATE INDEX idx_sync_user_updated ON sync_items(user_id, updated_at)` |

⇒ **3 条二级索引**，与实测的 4 行/条吻合。

### 1.3 逐操作成本表

| 操作 | DELETE 命中 | INSERT | 计费行 | 证据强度 | 现护栏记账 |
|---|---|---|---|---|---|
| 新条目 | 0 | 1 | **5**（4 + seq 1） | **实测坐实**（80,001） | 1 |
| 重推已存在条目 | 1 | 1 | **9**（DEL 4 + INS 4 + seq 1） | INSERT 侧实测；**DELETE 侧为对称推断**（见 §6 步骤 0′ 标定） | 2 |
| 桶：新桶 | 0 | 1 | **5** | 结构同构（`0004_sync_buckets.sql:31` AUTOINCREMENT + `:47/:50/:52` 三条索引） | 1 |
| 桶：重推 | 1 | 1 | **9** | 同上 | 2 |
| B1 新增：账本 upsert | 0/1 | 1 | **1**（无索引、无 AUTOINCREMENT） | 待落码 | — |

> DELETE 侧若不按 4 计费，则重推成本降为 5；成本常量取**上界 9** 是保守方向（多算不炸库）。§6 步骤 0′ 用约 10 行写的标定测量可把推断变实测，实测低于 9 再把常量下调并在注释附证据。

### 1.4 现护栏的三处偏差

| # | 偏差 | 位置 | 后果 |
|---|---|---|---|
| B-1 | `planned` 低估 5× / 4.5× | `src/sync.ts:172-189` | 声明的 20% 实为 100% |
| B-2 | `used` 非单调（见 §2） | `src/sync.ts:191-214` | 护栏可无限绕过 |
| B-3 | 文档声称与实际不符 | `src/env.ts:45-48`、`src/sync.ts:93-96`、`wrangler.toml:58-61` | 运维据错误数字做容量决策 |

桶端点同构：`src/sync-buckets.ts:228-243` 复用 `planWriteRows`（传 `bucket_key`），`countTodayWrittenBucketRows` 同样 COUNT 派生 ⇒ B-1 / B-2 全部继承。`WRITE_BUDGET_BUCKETS_PER_DAY = "4000"`（`wrangler.toml:71`）最坏 2,000 桶重推 × 9 = 18,000（18%），当前**未越界**，但同样有绕过路径。

---

## 2. 结构缺陷：`used` 非单调（比 5× 更严重）

### 2.1 源码事实

```ts
// src/sync.ts:202-214
const dayStart = utcDayStartSeconds(now);
const row = await env.DB.prepare(
  'SELECT COUNT(*) AS n FROM sync_items WHERE user_id = ? AND updated_at >= ?',
).bind(userId, dayStart).first<{ n: number }>();
```

`DELETE` + `INSERT` 之后该条目**仍是一行**，`updated_at` 被刷新为今天 ⇒ 下一轮 `COUNT(*)` **不变**。而 `planWriteRows` 对「已存在」也只记 2。⇒ `used` 与 `planned` 都是**当前状态量**，不是**流量**。护栏把一个流量约束写成了状态约束。

`src/sync.ts:93-96` 原文自承：

> `used` = …**行数**（纯读 COUNT）。同一行当日被反复写只计 1 ⇒ 真实行写入数 ≥ used ⇒ 本护栏只会**低估**，不会高估。

问题在于「低估」不是小幅偏差，而是**无上界**。

### 2.2 可无限绕过的算例

设稳定工作集 **M = 6,666 条**且全部已存在于库：

```
used    = 6,666                        （今日行数，恒 6,666）
planned = 6,666 × 2 = 13,332
used + planned = 19,998 ≤ 20,000        ⇒ 放行（每轮都放行）
每轮真实写 = 6,666 × 9 = 59,994 计费行
第一轮 59,994（占 60%）→ 第二轮 119,988（> 100,000，中途 code: 7500 打穿全库）
```

护栏看到的 `19,998` **恒定不动**，无论跑多少轮。这正好命中 ADR 0076 的立项初衷——「一个人一次性全量重推打爆全库 D1 免费额度，导致该库内所有账号写操作一起失效」——护栏存在，但它不生效。

### 2.3 不需要恶意，正常使用即可

真实客户端行为（ADR 0076 D2-5a 语义指纹之后已消除「同一条没改也重推」，但**用户真实改动**依旧以 `DELETE+INSERT` 落库）：

- 一台设备每次同步推送前一轮变更的 M 条 → `M` 条已存在 ⇒ `planned = 2M`。
- `3M ≤ 20,000` ⇒ `M ≤ 6,666`。一个 6,666 条以内的日常改动集，**每轮都判「在预算内」**。
- 按 §2.2，两轮打穿。

⇒ 该缺陷的判据不是「能否被攻击」，而是「**正常重推 2 轮即可让全库所有账号写失败**」。

---

## 3. 候选方案

### 3.1 方案 A — 保逻辑行口径，阈值下调到 5,000

- 改动：`wrangler.toml:63` → `"5000"`；`src/env.ts:50` → `5_000`；`vitest.config.ts:37`、`test-pg/harness.ts:162` 同步；注释改写。
- 单轮最坏：5,000 新建 × 5 = **25,000（25%）**；1,666 重推 × 9 = **14,994**。
- 结构：B-2 **完全未修** ⇒ `M ≤ 1,666` 可无限轮，**7 轮**打穿 100,000。
- 代价：≈0.5 人日、零迁移、零客户端影响。
- 收益：把「一次合规推送吃满额度」降为 25%，**缩小了口径误差的危害面，但没有消除可绕过性，也没有让数字变得可解释**。

### 3.2 方案 B1（推荐）— 计费行口径 + 原子账本 + 索引数门禁

**B1-1 计费常量**（`src/sync.ts` 顶部，注释写死来源与实测证据）

```ts
/** sync_items 二级索引数（0001:71、0001:73、0003:22）—— 加索引必须同步改 WRITE_COST_*。 */
export const SYNC_ITEMS_INDEX_COUNT = 3;
/** 新条目：INSERT(1 表行 + 3 索引) + sqlite_sequence 1 = 5（实测 19,999 行 ⇒ 80,001 行）。 */
export const WRITE_COST_ITEM_INSERT = 5;
/** 重推：DELETE(4) + INSERT(4) + seq(1) = 9（DELETE 侧待 §6 步骤 0′ 标定）。 */
export const WRITE_COST_ITEM_REPLACE = 9;
```

桶端点独立常量同值（`WRITE_COST_BUCKET_INSERT/REPLACE`），来源 `0004_sync_buckets.sql:31/:47/:50/:52`。

**B1-2 账本迁移** `migrations/0005_write_usage.sql`

```sql
CREATE TABLE IF NOT EXISTS write_usage (
  user_id TEXT    NOT NULL,
  day     INTEGER NOT NULL,   -- 今日 00:00 UTC 的 epoch 秒（与 utcDayStartSeconds 同源）
  scope   TEXT    NOT NULL,   -- 'items' | 'buckets'
  rows    INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (user_id, day, scope)
);
```

无二级索引、无 AUTOINCREMENT ⇒ 每次 upsert **1 计费行**，点读 **O(1)**（旧的 `COUNT(*)` 是 O(行数)，随用量线性增长）。

**B1-3 判定**：`used = SELECT rows ... WHERE (user_id, day, scope)`（缺行视为 0）；
`planned = Σ WRITE_COST_*`（纯函数 `chargeWriteRows`）；
`used + planned ≤ limit` 才放行 —— 判定位置不变，仍在**任何写动作之前**（`src/sync.ts:327-341`、`src/sync-buckets.ts:227-243`）。

**B1-4 记账**：D1 `batch` 成功返回之后追加一条

```sql
INSERT INTO write_usage (user_id, day, scope, rows) VALUES (?, ?, ?, ?)
  ON CONFLICT(user_id, day, scope) DO UPDATE SET rows = rows + excluded.rows
```

原子累加 ⇒ `used` **单调**，反复重推会持续抬高 `used`，护栏真正生效。

**B1-5 上线日兜底**（账本无行但当日已有旧写入）：`used = max(ledger_rows, countTodayWrittenRows() × WRITE_COST_* )`。纯读、无状态、跨 UTC 日自动失效（`day` 分量切割）。**不采用**「上线日全天拒绝」这类会误伤真实用户的做法。

**B1-6 门禁**（§5 详列）：索引数断言 + 非单调回归用例 + 正/负边界。

### 3.3 方案 B2（本轮不做）— 桶化路径替代逐条推送

`sync_buckets` 已按 `1024 桶/词典` 存储（`idx_sync_buckets_user_device_item` 按设备隔离），行数从 40,061 降到 1,024 量级，天然消解放大。但：
- 阶段 3 切读**当前被硬禁**（`docs/plan-relay.md:142`；ADR 0077:411-412 要求云端历史 40,061 条先完成桶化覆盖）；
- 规则 1（`unproven_keys` 整桶压制）盲区未修（handoff §P-O / §P-P.10：`scan_complete=0`、`deferred=1310` ⇒ 推送闸门结构性常闭）。

⇒ B2 是**正确的终局**，但**不是本轮可交付项**。B1 与 B2 不冲突：B1 落地后，桶端点同样受计费行口径保护，阶段 3 开闸时不需要再改一次语义。

### 3.4 对比

| 维度 | 现状 | A（5,000 逻辑行） | **B1（20,000 计费行）** |
|---|---|---|---|
| 单轮最坏占日额度 | **100% / 90%** | 25% / 15% | **20% / 20%** |
| 与 `env.ts:45-48` 声称一致 | ❌ 差 5× | ❌ 差 1.25× | ✅ **成立** |
| `used` 可被反复重推绕过 | ✅ 可（2 轮打穿） | ✅ 可（7 轮打穿） | ❌ **不可（单调累加）** |
| 量纲可解释 | ❌「逻辑行」无对外含义 | ❌ 同 | ✅「D1 计费行 = 日额度的百分比」 |
| 未来加索引后自动报警 | ❌ 无 | ❌ 无 | ✅ `SYNC_ITEMS_INDEX_COUNT` 断言 |
| 每日可用新条目 | 20,000（但一推即满额） | 5,000 | ≈4,000 |
| 每日可用重推次数 | 无限（漏洞） | 无限（漏洞） | ≈2,222 |
| 迁移 / 代码面 | — | 4 处常量 | 1 迁移 + 2 文件 + 测试 |
| 工作量 | — | ≈0.5 人日 | ≈1.5–2 人日 |
| 回滚成本 | — | 改数 + deploy | `wrangler rollback`（账本表可原地弃用，**无需 DROP**） |

### 3.5 推荐：B1，并明确它不排斥 A 的刻度

- B1 修的是**语义**（计写入次数 vs 计当前行数）；A 只调**刻度**。语义不改，刻度怎么调都留着可绕过路径。
- 采纳 B1 后阈值仍可单独选：
  - `12,000` 计费行 = 12% 额度，≈2,400 新条目/日（最保守，给桶与登录留余量）
  - **`20,000` 计费行 = 20%，≈4,000 新条目/日（推荐，兑现现有注释声称）**
  - `25,000` 计费行 = 25%，≈5,000 新条目/日（若产品侧认为 4,000 太紧）
- 无论选哪个刻度，「一次 4 万条首导」在免费额度下都不可达（§0-7）⇒ 该问题必须由桶化或付费解决，**不要用「把阈值调大」去试图解决**。

---

## 4. ADR 0082 草案（拍板后原样落 `docs/adr/0082-write-budget-billing-row-accounting.md`）

### 4.1 标题与状态

**ADR 0082 — 写预算护栏改按 D1 计费行口径记账**

状态：Draft（待拍板）｜取代：ADR 0076 D2-3 的「行」口径定义｜关联：ADR 0076 D2-3 / D8、ADR 0077 D8

### 4.2 Context

ADR 0076 D2-3 为「一人一次性全量重推打爆全库 D1 免费额度」引入每账号每日写预算护栏，口径为 `sync_items` 的**行数**（`planWriteRows` 记 1/2）。该口径建立在一个未经实测的假设上：一次条目写入 ≈ 一次行写入。

2026-11-27 阶段 2 自证轮在生产 D1 上实测：19,999 行 / 5 语句 ⇒ `Rows written = 80,001`，即 **4 行/条 + 1 行/语句**。原因是产品写形态为 `DELETE` + `INSERT`（`src/sync.ts:376-408`，目的是让 `AUTOINCREMENT` 的 `id` 前进以推进 `server_seq`），而 `sync_items` 有 3 条二级索引。真实成本是 **5（新条目）/ 9（重推）**，护栏记账 1/2 ⇒ 低估 5× / 4.5×。

同一轮复核发现更严重的结构缺陷：`used` 取自 `COUNT(*) ... WHERE updated_at >= 今日00:00UTC`（`src/sync.ts:202-214`），计量「今日仍在库的当前行数」而非「写入次数」。`DELETE+INSERT` 后行数不变 ⇒ `used` 非单调、永不增长 ⇒ 固定工作集的反复重推使 `used + planned` 恒定通过。源码在 `src/sync.ts:93-96` 已自承「只会低估」，但低估幅度**无上界**：6,666 条工作集每轮真实写 59,994 计费行、每轮均判「在预算内」，两轮打穿 100,000。

⇒ ADR 0076 的安全目标（把「打爆全库」降级为「单账号 429」）在当前实现下**未达成**。

### 4.3 Decision

1. 护栏计量单位由「`sync_items` 行数」改为 **D1 计费行**，成本常量 `WRITE_COST_ITEM_INSERT = 5`、`WRITE_COST_ITEM_REPLACE = 9`（桶端点同值），注释写死来源与实测证据。
2. `used` 不再由 `COUNT(*)` 派生，改由**原子账本** `write_usage(user_id, day, scope, rows)` 点读；成功写入后 `ON CONFLICT DO UPDATE SET rows = rows + excluded.rows`。`used` 因此**单调不减**。
3. 预算判定仍在**任何写动作之前**（超限 ⇒ 429 `write_budget_exceeded` + R2/D1 零写入），该性质不变。
4. 阈值 `WRITE_BUDGET_ROWS_PER_DAY` 语义改为「每账号每 UTC 日的计费行上限」，默认值取 **20,000**（= 日额度 20%）；桶端点 `WRITE_BUDGET_BUCKETS_PER_DAY` 语义同改。
5. 新增**索引数门禁**：迁移中 `CREATE INDEX ... ON sync_items` / `ON sync_buckets` 的计数必须等于 `SYNC_ITEMS_INDEX_COUNT` / `SYNC_BUCKETS_INDEX_COUNT`（当前均为 3），不等则测试失败并提示「改索引必须同步改 `WRITE_COST_*` 与 ADR 0082」。
6. 上线日兜底用 `max(账本, COUNT × cost)` 纯读派生，不引入「上线日全拒」这类误伤路径。

### 4.4 Consequences

**正面**
- 「20,000 = 单账号 ≤20% 日额度」第一次成立，`src/env.ts:45-48` 的错误声称被消除。
- 护栏恢复其设计目标：反复重推会持续抬高 `used`，最终稳定返回 429。
- 每次判定的读成本从 `O(今日行数)` 降到 `O(1)`（点读主键）。
- 未来给 `sync_items` / `sync_buckets` 加索引时，若忘记改成本常量，门禁立即变红。

**负面 / 张力（必须接受）**
- 有效吞吐显著下降：每日 ≈4,000 新条目 或 ≈2,222 次重推（旧口径名义 20,000 条 / 10,000 次重推，但那条路径本身会打穿额度）。
- **40,061 条首次全量上云 = 200,305 计费行 > 100,000** ⇒ 免费 D1 当天装不下一次首导。该限制在现状下同样存在（160,244），ADR 0076 从未解决，本 ADR 只是把它显式化。唯一出口为桶化阶段 3（当前禁）或 Workers Paid（已否决，见 handoff §O）。
- 并发请求仍可能各自判定通过而合计超发（两个并发各 4,000 计费行 ⇒ 8,000，仍 ≤ 20,000，安全裕度存在；极端并发下可超）。**残余风险按「并发数 × 单请求上限」登记**，不引入分布式锁；护栏内缩 10% 作为余量。
- 账本行本身占用写入（≈1 计费行/轮/scope），可在 limit 侧预留 64 行/日余量。

**中立**
- `planWriteRows` 保留但**降级为诊断口径**（注释明确「不是计费口径」），新函数 `chargeWriteRows` 承担门禁。避免破坏既有 6 处测试断言。

### 4.5 Alternatives considered

- **A：保逻辑行、阈值降至 5,000** —— 只缩小危害面，不消除可绕过性（1,666 条工作集 7 轮打穿），且量纲仍不可解释。**否决**。
- **B2：桶化替代逐条推送** —— 正确的终局，但阶段 3 被硬禁（ADR 0077:411-412）、规则 1 盲区未修。**不作为本 ADR 的替代，作为后续路径**。
- **改 UPDATE 语义替代 DELETE+INSERT** —— 会破坏 `server_seq` 单调推进（客户端据此判「有无变化」），需重设计同步协议。**超出本 ADR 范围**。
- **把 `sqlite_sequence` 与索引成本忽略（只记 4/8）** —— 与实测 80,001 不符，且加索引后会静默偏移。**否决**。

### 4.6 Validation

§5 六类门禁 + §6 部署路径 + 发布后 24 h 观测 `write_usage.rows` 与 D1 `rows_written_24h` 的比值落在 [0.9, 1.1]。

---

## 5. spec 104 与门禁用例

`spec 104`（`.specify/specs/104-write-budget-billing-rows/spec.md`）提案的验收条目：

| ID | 内容 | 判别力 |
|---|---|---|
| AC-1 | `chargeWriteRows` 对全新键返回 `5 × n`，对已存在键返回 `9 × n`，混合批返回 `Σ` | 纯函数，A 方案下必红（A 不改此函数） |
| AC-2 | `chargeWriteRows` 单测断言「`sync_items` 的索引数 == `SYNC_ITEMS_INDEX_COUNT`」；人为在迁移里加第 4 条索引 ⇒ 测试**必须变红** | 唯一能捕获「未来加索引」的用例 |
| AC-3 | **非单调回归**：模拟 6,666 条已存在键连续两轮推送。第一轮 `allowed=true` 且账本累计 `59,994`；第二轮**必须** `allowed=false` | **旧实现必红**（旧 `used+planned` 恒 19,998） |
| AC-4 | 正边界：`used=0`、4,000 条全新 ⇒ `planned=20,000`、`used+planned=20,000` ⇒ **放行**（`evaluateWriteBudget` 用 `>` 才拒） | 防止「把阈值改小」式假修复 |
| AC-5 | 负边界：4,001 条全新 ⇒ `20,005 > 20,000` ⇒ **429 `write_budget_exceeded`，R2 与 D1 零写入** | 与 AC-4 成对，夹逼 |
| AC-6 | 上线日兜底：账本无行、当日 `COUNT=1,000` ⇒ `used = max(0, 1,000×5) = 5,000` | 防止上线当天把老用户额度重置 |
| AC-7 | 桶端点同构：2,000 桶重推 ⇒ `planned=18,000` 放行；2,001 ⇒ 拒 | 桶路径不得漏改 |
| AC-8 | `used` 单调性：连续 5 轮同批重推，`write_usage.rows` **严格递增**且第 N 轮被拒 | 直接测护栏语义 |
| AC-9（可选） | 生产标定：`DELETE 1 行 + INSERT 1 行` 单语句实测 `Rows written`，回填 `WRITE_COST_ITEM_REPLACE` 注释 | 把 §1.3 的 DELETE 推断变实测 |

既有测试的处置：
- `test/write-budget.test.ts`（三层：纯函数三分支 / `planWriteRows` 1-2 口径 / UTC 边界 / 集成 429 零写入）—— 保留，`planWriteRows` 断言不变，**新增** `chargeWriteRows` 段。
- `test/bucket-sync.test.ts:164-176`（桶阈值回落）、`:601-617`（双口径互不干扰）—— 保留。
- 新增 `test/write-usage-ledger.test.ts`：AC-1/2/3/6/8。
- 新增 `test/write-budget-boundary.test.ts`：AC-4/5/7。

---

## 6. 部署与授权路径

**每一步之间必须停下等授权**（`stage-2-rulings-20261127.md:168`；接力链内环**不得自行执行**生产写）。

| 步骤 | 动作 | 是否生产写 | 授权 |
|---|---|---|---|
| 0 | 盘上复读：4 处阈值 + 3 处索引定义 + `planWriteRows` 现状 | 否 | 不需要 |
| **0′** | **标定测量**：`d1 execute --remote --file=` 一条 `DELETE + INSERT`（约 10 行写）读回 `Rows written`，确认 DELETE 侧是否为 4 | **是**（≈10 行） | **需单独授权** |
| 1 | 落码：`src/sync.ts` 成本常量 + `chargeWriteRows` + 账本读写；`src/sync-buckets.ts` 同构；`migrations/0005_write_usage.sql` | 否（本地） | 只需落码授权 |
| 2 | 本地门禁：`npx vitest run`（AC-1..8），要求全绿；AC-2 用「加一条假索引」自证会红后还原 | 否 | 不需 |
| 3 | 生产 DDL：`npx wrangler d1 migrations apply DB --remote` ⇒ 新建 `write_usage`（1 表、0 索引） | **是**（DDL） | **需单独授权** |
| 4 | 部署：`npx wrangler deploy` ⇒ 替换生产 Version（当前生产 Version 记录在 `wrangler deployments status`） | **是** | **需单独授权** |
| 5 | 生产探针：正边界 4,000 条全新（若当日额度允许）+ 负边界 1 条 ⇒ 复验 429 与零写入 | **是**（≈20,000 计费行） | **需单独授权，且受当日额度约束** |
| 6 | 观测 24 h：`write_usage.rows` 汇总 vs `d1 info.rows_written_24h`，比值落在 [0.9, 1.1] | 只读 | 不需 |

**额度前置检查**（每个生产写步骤前必做）：`npx wrangler d1 info DB --json` 读 `rows_written_24h`，余量 < 计划写入量 ⇒ **不执行**，改期 UTC 次日（北京时间 08:00 后）。当前余量 ≈6,018 行（入口 13,956 → 收尾 93,982）⇒ 步骤 0′ 可跑，步骤 5 必须等重置。

**不改动**：`hkcu\Software\Fluxing` / `hkcu\Software\Fluxing\Weasel` / `InstallDir` / `RimeUserDir` / 用户词典 / 桶阶段变量（`FLUXING_CLOUD_SYNC_BUCKET_PHASE` 保持 0）。

---

## 7. 对生产现值 20,000 的影响与回滚

### 7.1 影响

| 项 | 现状 | B1 落地后 |
|---|---|---|
| `WRITE_BUDGET_ROWS_PER_DAY` | `"20000"`（`wrangler.toml:63`） | **仍是 `"20000"`**（数字不变，**语义变**） |
| 该数字的含义 | 最多 20,000「逻辑行」，真实最坏 100,000 计费行 | 最多 20,000 **计费行** = 日额度 20% |
| 单账号每日可用新条目 | 名义 20,000；一推即满额 | ≈4,000，可持续 |
| 单账号每日可用重推 | 无限（漏洞） | ≈2,222 |
| 真实用户可感知影响 | 无（护栏没生效过） | **首次生效**：大库首导需分多日 |
| 客户端 | 不需改 | 不需改（429 已是既有契约） |
| 诊断面 | 无 | 新增 `charge=` 字段进既有诊断行 |

**关键判断**：B1 不提高任何限额，只让既有声称变成事实。生产上唯一真实的可见变化是「**首次全量上云从一天变成多天**」——而现状下这一天本身就会因 `code: 7500` 失败，所以这不是新增的坏路径，而是把已经坏掉的路径变成可预期的分片。

### 7.2 回滚（三级，均不需 DROP 表）

1. **代码回滚**：`npx wrangler rollback`（或重新 deploy 步骤 4 之前记录的 Version id）。旧代码不读 `write_usage` ⇒ **无影响**。
2. **阈值回滚**：改 `wrangler.toml:63` + deploy。
3. **迁移回滚**：`write_usage` 可**原地弃用**（不读不写即无成本）。若确需移除，`DROP TABLE write_usage` 安全（无外键引用、无其他表依赖）。

⇒ 这是本方案的重要优点：**回滚不需要数据迁移，也不需要恢复任何被覆盖的行**。

### 7.3 与今日残留的关系

今日生产残留（探针账号 `wb.probe.1127150009@aiec.fun` / uid `a1b4e4d4-7ac5-497a-b21f-68f32ba66ab9`、`sync_items` 20,000 行、R2 1 个 32 B 对象）**与本复核无关**：
- UTC 日重置后该账号 `used` 自动归零（现状由 `updated_at` 派生，B1 后由 `day` 分量派生，行为一致）；
- `-Stage cleanup` 今日**禁跑**（`DELETE` 20,000 行按 ≈4× 放大需 ≈80,000 行写，必触发 `code: 7500`），排期 **2026-11-28 08:00（北京时间）之后**。

---

## 8. 本轮未决与登记

| # | 事项 | 状态 |
|---|---|---|
| U-1 | 方案 A / B1 拍板 | **待用户裁定** |
| U-2 | B1 若采纳，刻度选 12,000 / 20,000 / 25,000 计费行 | **待用户裁定** |
| U-3 | `stage-2-rulings-20261127.md` §5.4-2 要求「超限时 `code: 7500` 可复现」，但实测超限是 **429 `write_budget_exceeded`**，`7500` 只在打穿账号级额度时出现 ⇒ 两者不可同时成立 | **口径冲突未订正**（改裁定书需单独授权） |
| U-4 | 40,061 条首导在免费 D1 当天不可达 | 已登记，出口 = 桶化阶段 3 或 Workers Paid（均受阻） |
| U-5 | Workers Paid 升级 | **已否决**（handoff §O），D1 免费 10 万行/日是长期硬约束 |
| U-6 | 桶阶段 3/4 开闸 | **仍禁**（ADR 0077:411-412 硬前置未满足） |
| U-7 | `write_usage` 并发超发（§4.4） | 按「并发数 × 单请求上限」登记，不引入锁 |
| U-8 | 缺陷 ①-1（`bucket pull` 无有限重试）、6 条 `bucket_older` 未归因、503 根因未定 | 沿用，非本轮范围 |
| U-9 | handoff 未追加自证轮与复核轮（链持锁，最新顶节 `## AL`） | 锁释放后补 |
| U-10 | `relay-ledger.ps1:186-202` 的 `all_done` 只看 `stages[]` 不看 `current_stage vs max_stages` | 诊断面歧义，链持锁期间不改 |

---

## 9. 证据清单

**源码（全部盘上复读）**

| 文件:行 | 内容 |
|---|---|
| `cloud/fluxing-sync/src/sync.ts:85-116` | 护栏口径自述；`:93-96` 自承「只会低估」；`:107-116` 桶化后只管逐条端点 |
| `cloud/fluxing-sync/src/sync.ts:152-164` | `evaluateWriteBudget`：`used + planned <= limit` |
| `cloud/fluxing-sync/src/sync.ts:172-189` | `planWriteRows`：1 / 2 口径 |
| `cloud/fluxing-sync/src/sync.ts:191-214` | `utcDayStartSeconds` + `countTodayWrittenRows`（COUNT 派生、非单调） |
| `cloud/fluxing-sync/src/sync.ts:221-232` | `evaluatePushWriteBudget` |
| `cloud/fluxing-sync/src/sync.ts:327-341` | 预算判定位置（任何写之前） |
| `cloud/fluxing-sync/src/sync.ts:376-408` | **DELETE + INSERT** 写形态（`server_seq` 推进动机见 `:376-377`） |
| `cloud/fluxing-sync/src/sync-buckets.ts:227-243` | 桶端点复用 `planWriteRows` + `countTodayWrittenBucketRows` |
| `cloud/fluxing-sync/src/env.ts:45-50` | 「落 20,000 = 单账号当日最多 20%」声称；`DEFAULT_WRITE_BUDGET_ROWS_PER_DAY = 20_000` |
| `cloud/fluxing-sync/src/env.ts:93-117` | 桶默认 4,000、区间 1~10,000,000、回落逻辑 |
| `cloud/fluxing-sync/migrations/0001_init.sql:57-73` | `sync_items` + `AUTOINCREMENT` + 2 条索引 |
| `cloud/fluxing-sync/migrations/0003_sync_items_updated_index.sql:22` | 第 3 条索引 |
| `cloud/fluxing-sync/migrations/0004_sync_buckets.sql:30-52` | `sync_buckets` + `AUTOINCREMENT` + 3 条索引 |
| `cloud/fluxing-sync/wrangler.toml:58-71` | 阈值 `"20000"` / `"4000"` |

**生产实测**

- `reports/relay-stage2-wb/wb-run-20261127-150002.log`（3,239 B，已脱敏）：`Rows read 20019 / Rows written 80001`
- `reports/relay-stage2-wb/wb-evidence-20261127-150002.json`：`FINAL_WB_VERDICT=PASS`、12 项 CHECK
- `reports/relay-stage2-wb/seed-20261127-150002.sql`：19,999 行 / 5 语句

**既有门禁**：`test/write-budget.test.ts`（三层）、`test/bucket-sync.test.ts:164-176` / `:601-617`

**裁定与计划**：`docs/relay/stage-2-rulings-20261127.md` §5.2 `:163`、§5.3 `:168`、§5.4 `:170-174`、§6 `:178-187`；`docs/plan-relay.md:142`（桶阶段红线）

**编号现状**：`docs/adr/` 最新 `0081-*` ⇒ 本草案取 **0082**；`.specify/specs/` 最新 `103-*` ⇒ 本 spec 取 **104**（均**未创建**）

---

## 10. 授权请求

本轮只读，未改任何代码、未迁移、未部署、零生产写。请就两点裁定：

1. **采纳 B1 口径**（计费行 + `write_usage` 原子账本 + 索引数门禁），还是选 A（只把逻辑行阈值降到 5,000）？
2. 若采纳 B1，刻度取 **20,000**（20%，≈4,000 新条目/日，推荐）/ **12,000**（12%，最保守）/ **25,000**（25%，≈5,000 新条目/日）？

裁定后按 §6 逐步执行，每步单独取授权。