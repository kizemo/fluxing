# ADR 0079 —— 路线 B 的 PG 方言适配层：边界替换、占位符单点断言与三条硬闸门

- 状态：**Accepted**（路线 B · D1/R2 → 自建 Node + PostgreSQL）
- 批次：**W6 必改项落码批**（由 BR 批次 §V-11 第 ② 项派生；本批只做 W6，不碰 W7 远端、不碰 BR-①h 的 C++ 改动）
- 触发：`docs/research/fluxing-cloud-2026-11/port-to-pg-recon.md` §9.4 移植判定 1
  （「必改项只有占位符形态」）与 §9.5 的四条未坐实项
- 上游指针：`docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`
  §U / §V / §W；`docs/research/fluxing-cloud-2026-11/port-to-pg-recon.md` §4–§9；
  `docs/adr/0076-cloud-write-amplification-governance-and-zero-write-registration.md`；
  `docs/adr/0077-bucket-level-sync-storage-architecture.md`；
  `docs/adr/0078-userdict-integrity-guards-against-empty-snapshot-restore.md`

> **本 ADR 的存在理由**：`cloud/fluxing-sync/src/pg/d1-shim.ts` / `r2-fs.ts` / `dynamic-import.ts`
> 三个文件的头注释都已引用「路线 B · ADR 0079」，而 `docs/adr/` 目录当时最高只到 0078
> ⇒ 属**悬空引用**。本批补齐本文件，把已落码的决策固化成可审计的书面结论。

---

## 1. 背景与问题

路线 B 要求**同一份服务端业务代码**能跑在两个底座上：

| 底座 | 现状 / 目标 | 状态 |
| --- | --- | --- |
| Cloudflare Workers + D1 + R2 | 现状（生产在跑） | 已上线 |
| Node + PostgreSQL + 本地对象存储 | 目标（自建） | W1–W5 已落码，**未切源** |

服务端 17 个 `.ts` / 5,019 行里，D1 调用面是：

| API | 次数 | 备注 |
| --- | ---: | --- |
| `.prepare(` | 65 | 其中 58 个第一参是字符串字面量 SQL |
| `.bind(` | 65 | 与 `prepare` 1:1 |
| `.first(` | 23 | **无一例传列名** |
| `.run(` | 27 | 只有 `account.ts:92` / `:97` 消费 `changes` |
| `.all(` | 7 | `{ results }` 形态 |
| `DB.batch(` | 6 | sync 2 / buckets 2 / devices 1 / 其他 |
| `env.DB` | 70 | 全部经 Env 注入 |
| `env.BUCKET` | 9 | `r2-fs` 侧 |

三条路可选：

1. **逐条改写成 PG 方言**（`?` → `$n`、逐条重写 SQL）—— 会把一次「换底座」变成一次
   「重写同步逻辑」，而同步逻辑正是事故史最密的地方（ADR 0076 / 0077 / 0078）；
2. **在业务文件里插方言分支** —— 同样把底座差异散进业务代码，每加一个端点就有新分叉点；
3. **边界替换**：只实现一层与 D1 调用面**同形**的兼容层，业务代码**一个字都不改**。

选 3。

---

## 2. 决策

### D1 · 边界替换而非逐条改写

兼容层放在 `cloud/fluxing-sync/src/pg/`（`d1-shim.ts` / `r2-fs.ts` / `dynamic-import.ts`），
**只实现被实际消费的那一小块 D1 API**。未被消费的 API（`.raw()` / `.changes` /
`.last_row_id` / `.duration` / `.exec()`）**刻意不实现** —— 实现一个「看着能用其实没测过」的
假面，比不实现更危险。`.exec()` 的 3 处命中经逐点核实**全是 `RegExp.prototype.exec`**，
不是 D1 的 `stmt.exec()`。

> 「不实现」的前提是**逐点核实过零命中**，不是「估计没人用」。

### D2 · 占位符单点改写 + 唯一断言收口点

SQLite 用 `?`，PG 用 `$n`。改写发生在**一个**收口点：`PgCompatStatement.toQuery()`。

- 扫描器只有**一套实现**：`scanPlaceholders(sql)` 同时产出改写文本与占位符个数；
  `renumberPlaceholders(sql)` 是它的薄封装（保留旧调用面）。
  **禁止出现第二套扫描器** —— 两套实现必然对不上，对不上时谁对谁错没法判定。
- 扫描器跳过四类区域（其中的 `?` 不是占位符）：单引号字符串字面量（`''` 为转义）、
  双引号标识符、行注释、块注释。
- **断言**：编译时若「占位符个数 ≠ 绑定参数个数」⇒ 抛中文错误。

### D3 · 断言取**严格模式**（个数不等即失败）

与 D1 自身的语义同义（D1 对参数个数不符的语句报
`Not enough parameter values were provided`）。PG 侧原始报错是
「bind message supplies N parameters, but prepared statement requires M」——
**与根因隔着两层**；而 `prepare(sql)` 后**不** bind（SQL 里有占位符）在 D1 上同样是失败，
严格模式一并覆盖。

失败信息**只给个数**，不给 SQL 正文、更不给绑定值：

> `SQL 占位符个数（N）与绑定参数个数（M）不一致：请检查该语句 bind() 的参数个数。`

⇒ 结构上不可能把用户数据写进日志（凭据红线：AGENTS.md §4.6.1）。

### D4 · `first()` 的「无行」归一为 `null`（不是 `undefined`）

`pg` 在无行时返回 `undefined`，D1 返回 `null`。shim 归一为 `null`。
全仓逐点核实：`.first()` 的结果**没有任何一处**与 `undefined` 比较
（`sync.ts:307` / `sync-buckets.ts:208` 的 `stored` 来自 `Map.get`；
`sync-buckets.ts:112` 的 `claimed` 来自 JSON body）⇒ 该差异对本仓代码不可观测，
归一是安全的。

### D5 · `batch()` = 单连接真事务

D1 的 `batch` 保证「全部成功或全部不生效」。`pg.Pool.query` 每次可能取不同连接 ⇒
在池上直接发 `BEGIN/COMMIT` **是错的**。实现固定为：

```
const c = await pool.connect();
try { BEGIN; …各语句…; COMMIT; } catch { ROLLBACK; throw; } finally { c.release(); }
```

`sync.ts:391` / `sync-buckets.ts:292` 是交替 DELETE+INSERT：半途失败若留下
「删了没插」的中间态，等于把一次失败升级成一次数据丢失 ⇒ 事务性是**数据完整性**要求，
不是性能优化。

### D6 · int8 归一为 JS number

node-postgres 默认把 int8 读成**字符串**。`hlc_ms` / `server_seq` / `id` / `COUNT(*)`
在 Workers 上都是 JS number；不归一就会静默改变线上响应形状（客户端按 number 解析，
字符串会一路传到比较运算）。**不装类型解析器**（`pg.types.setTypeParser` 全局影响面太大），
只在 shim 的读取路径上归一。

### D7 · R5 写预算语义**本批不改**（写明理由与触发条件）

`WRITE_BUDGET_ROWS_PER_DAY=80,000` / `WRITE_BUDGET_BUCKETS_PER_DAY=4,000` /
`LAST_SEEN_REFRESH_SECONDS=3600` / `REFRESH_TOKEN_GC_MAX_ROWS=200`
全部是**为 D1 免费额度服务的**（recon R5）。PG 上额度约束消失 ⇒ 这些值应回归正常语义
（例如 `last_seen` 回到 300 s），但：

- **本批不改**。理由：① 切源尚未发生（§V-6 八条阻塞未解），提前改这些值会让
  「D1 生产行为」与「PG 预发行为」**同时**变，事故后无法二分归因；
  ② 写预算护栏语义变更属于**服务端行为语义变更**，必须自带 ADR 与门禁用例；
  ③ 桶化路线下每账号口径的护栏已由客户端侧阶段状态机分担（ADR 0077）。
- **触发条件**：W7 切源前的同批里改，且必须满足 —— ① 客户端桶化阶段状态机已冻结；
  ② 生产 D1 只读回读的 `rows_written_24h` 基线已记录；③ 改前改后各跑一次
  `sync_items` / `sync_buckets` 行数差分。

### D8 · 三条硬闸门**一条都不解除**（recon §6）

1. **假空闸门**：`kDefaultBaseUrl` 改指新域名之前，必须已有 P0 假空的失败复现用例 +
   修复后的通过用例（ADR 0078 / BR-①h）。
2. **数据核对闸门**：`users` / `devices` / `refresh_tokens` / `sync_items` / `sync_buckets`
   逐表行数与 D1 侧只读回读一致；R2 对象数与 DB 内 `r2_key` 去重计数一致。
3. **回滚闸门**：切源后保留「改回 `api.aiec.fun` 一个字符串」的回滚路径；
   不得在同一批里删除旧端点代码。

**本 ADR 只改兼容层，不触及任何一条闸门。**

### D9 · 桶化代码长期冻结在 phase 0（recon K6）

客户端桶化（ADR 0077）代码**保留不删**，默认长期冻结在 phase 0；
**阶段 3 / 4 在 B 路线下永不启用**。本 ADR 不改变该状态。

### D10 · 外键不关、配额错误码不删（recon R1 / R4）

- **R1**：D1/SQLite 默认不强制外键，PG 强制。**不关 FK**；用测试把删除顺序固定下来
  （禁止靠「PG 不报错」蒙过去）。
- **R4**：`errors.ts` 的 `isD1RowWriteQuotaError` 在 PG 上永远为 false ⇒
  **保留代码不删**（无害），但 `storage_quota_exceeded` 这条 503 分支在 PG 侧是死路。

---

## 3. 占位符清点（本批实测，盘上回读）

审计脚本：`reports/fluxing-br-1g/w6/audit-bind-count.mjs`（`reports/**` 一律不入库）。
口径纪律：**占位符计数只用 shim 自己的实现**（esbuild 打包 `src/pg/d1-shim.ts` 后 import），
脚本内不重写第二套扫描器。

| 项 | 实测值 | 含义 |
| --- | ---: | --- |
| `PREPARE_SITES` | 65 | `.prepare(` 站点总数 |
| 覆盖度交叉验证 | 65 == 65 | `.prepare` 标记数 == `.prepare(` 调用数 ⇒ **无拼写变体漏站** |
| `DYNAMIC_SITES` | 7 | 第一参非常量（动态 / 透传 / 文档注释假阳性） |
| `TOTAL_PLACEHOLDERS` | **135** | 58 个字符串字面量站点的占位符总数（扫描器口径） |
| `NAIVE_TOTAL` | 135 | 同上站点朴素 `?` 计数 |
| `LITERAL_DELTA_SITES` | **0** | 朴素口径与扫描器口径差额 = 0 ⇒ **没有「字面量里的 `?`」陷阱** |
| `MISMATCH_SITES` | **0** | 占位符数 ≠ 绑定数的站点数 = 0 |
| `NOBIND_SITES` | **0** | 有占位符却没 bind 的站点数 = 0 |

7 个动态站点逐点归类：

| 站点 | 归类 | 占位符 / 绑定 |
| --- | --- | --- |
| `sync.ts:279` | 运行时展开 `uniqueKeys.map(() => '?')` | N+1 / N+1 |
| `sync.ts:399` | 同上 | N+1 / N+1 |
| `sync-buckets.ts:180` | 同上（多 `device_id`） | N+2 / N+2 |
| `sync-buckets.ts:300` | 同上 | N+2 / N+2 |
| `auth.ts:418` | **变量透传** `prepare(sql).bind(id)`，SQL 来自调用方 `auth.ts:487` / `:500` | 1 / 1（各） |
| `errors.ts:36` | **文档注释假阳性**（`.prepare(` 出现在注释里） | 非真站点 |
| `d1-shim.ts:215` | **文档注释假阳性**（同上） | 非真站点 |

⇒ **全仓真站点 = 63**（65 − 2 注释假阳性），每个站点的「占位符数 == 绑定数」均成立。

### 3.1 订正：早期清点的「145」是过计数

recon §9.3 / `d1-shim.ts` 旧头注释记的「`prepare` 第一参内共 145 个 `?`」
**不得作为迁移工作量依据**：那 145 = 135（静态真占位符）+ 6（4 个动态站点模板内联的 `?`）
+ **4（4 处 `map(() => '?')` 构造行本身也被数了进去）**。
构造行不是占位符，是「生成占位符的源码」。
同理，整文件朴素计数 386 个含三元运算符与可选参数，**不可用作迁移依据**。

⇒ 本 ADR 与 `d1-shim.ts` 头注释均已订正为「可静态计数的占位符 135 个」，
并显式记下动态站点与变量透传站点的归属。

---

## 4. 断言可启用的前置条件与本批门禁

**前置条件**：全仓 65 个 `prepare` 站点逐点算过、0 失配、0 无绑定（§3）。
未满足此前提**不得**打开断言 —— 断言一旦在某个业务站点上不成立，
该端点在自建 PG 上**必然报错**，且报错信息（PG 原生措辞）看不出根因。

本批门禁（全部盘上回读）：

| 门禁 | 命令 | 期望 |
| --- | --- | --- |
| 类型 | `tsc --noEmit -p tsconfig.json` | 0 错 |
| node 打包 | esbuild `src/node-main.ts` `--external:pg` | 成功 + 字节/sha256 记录 |
| 运行期自测 | esbuild `pg-port-selftest.ts` → node | `SELFTEST_PASS` 由 31 升至 **37** / FAIL=0 |
| **Workers 打包不变量** | `wrangler deploy --dry-run` | `index.js` **91,912 B 逐字未变** + 九条零污染全 False |
| workerd 套件 | `npm test` | 13 files / 164 tests |
| 真 PG 套件 | `npm run test:pg` | 3 files / **24 tests**（基线 1 file / 4 tests） |
| 负路径 | 占位符 / 绑定数不符 | 退出码 1 + 中文错 |

新增用例（`cloud/fluxing-sync/test-pg/`，`tsconfig` 的 `include` 只含 `src/**/*.ts`
⇒ 这些文件**不进 tsc 门禁**，由 vitest 以 esbuild 转译运行）：

- `d1-shim-unit.test.ts` —— 14 条纯单元判据（不需 PG）：扫描器七例、计数自洽、
  绑定数不符抛中文错、有占位符未绑定也失败、错误信息**不含** `secret_column` /
  `topsecret`、`first()` 无行 ⇒ `null`（含传列名重载）、`rowCount===null` ⇒ `changes===0`、
  `batch([])` 不取连接、batch 成功有 BEGIN+COMMIT 无 ROLLBACK、batch 中途失败有 ROLLBACK 无 COMMIT。
- `d1-shim-pg.test.ts` —— 6 条真 PG 判据：`batch()` **真原子**（第 2 条违反主键 ⇒
  第 1 条 INSERT 也不落库）、batch 成功两条落库、真 PG `first()` 无行 ⇒ `null` 且非 `undefined`、
  DDL（`rowCount===null`）⇒ `changes===0`、UPDATE 影响 0 行 ⇒ `changes===0`、
  真库上占位符 / 绑定数不符先失败。

---

## 5. 后果与边界

**正面**：

- 业务代码零改动 ⇒ 底座差异被关在一个文件里；同步逻辑（事故史最密处）一行未动。
- 断言把「两端口径不同」从**静默事故**降级成**一次显式失败**。
- `batch()` 真原子 ⇒ 半途失败不会留下「删了没插」的中间态。

**代价 / 边界**：

- 兼容层是**新增的抽象层**：将来若要支持第三个底座，抽象层要重估
  （当前形态假设「D1 语义」是稳定的）。
- `int8` 归一在读取路径上做，**只覆盖 shim 自己的查询**；
  任何绕过 shim 直连 PG 的新代码都会拿回字符串。
- 断言只管**个数**，不管**类型**：`?` 绑成 text 还是 bigint 仍由 PG 判定。
- 本 ADR **不解除任何一条硬闸门**，也**不构成切源许可**。
  W7 切源仍被 §V-6 的八条阻塞（Cloudflare 301 / 证书 / ECS 零部署 / 根盘 / `5432` 收口 /
  PG 大版本 / ICP 备案 / 干净目录 `npm ci`）卡住。

---

## 6. 证据路径（均不入库）

| 证据 | 路径 |
| --- | --- |
| 占位符双口径审计 | `reports/fluxing-br-1g/w6/audit-bind-count.mjs` |
| 审计输出 | `reports/fluxing-br-1g/w6/audit-bind-count.out.txt` / `w6/bind-count.json` |
| 本批门禁编排 | `reports/fluxing-br-1g/w6/gate-w6.ps1` |
| 门禁产物 | `reports/fluxing-br-1g/w6/gate-w6.out.txt` / `w6/gate-esbuild.mjs` |
| 运行期自测源（已改） | `reports/fluxing-br-1g/pg-port-selftest.ts` |
| W6 只读清点产物 | `reports/fluxing-br-1g/w6/audit.mjs` → `summary.json` / `api-hits.csv` / `dialect-hits.csv` |

入库文件（本 ADR 所在批）：`cloud/fluxing-sync/src/pg/d1-shim.ts`、
`cloud/fluxing-sync/test-pg/d1-shim-unit.test.ts`、
`cloud/fluxing-sync/test-pg/d1-shim-pg.test.ts`、`docs/adr/0079-*.md`。

---

## 7. 未证实项（不得当结论）

1. **ECS 侧可部署性未证实** —— 本批全部证据只覆盖**本机** PG 18.4；
   ECS 的 PG 大版本（`psql --version`）未回读，`npm ci` 在干净目录的可复现性未实测。
2. **生产 D1 的当前行数与 `rows_written_24h` 未回读**（本批零写操作）—— 迁移前必须重新只读回读。
3. **`isUniqueConstraintError` 的 PG 23505 映射未在生产量级实测**（8 处 `INSERT INTO` 全为
   UUID 主键新建，唯一键冲突属错误路径）。
4. **动态站点的运行时占位符数未逐轮实测** —— N+1 / N+2 同构是**静态读码 + 手核**结论；
   真实 N 取决于该批条目数。断言在运行时会兜住，但**尚未在生产量级触发过**。
5. **R5 写预算语义改法未定** —— 见 D7，触发条件未满足。
6. **P0 假空未修** ⇒ 硬闸门 1 未解除；本 ADR 不提供任何绕过。
