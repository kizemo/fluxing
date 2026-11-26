# fluxing-sync → 自建 PostgreSQL 移植 recon（路线 B）

- 日期：2026-11-26
- 性质：**只读 recon**。未改任何产品代码、未跑门禁、未动生产 D1/R2、未做腾讯云远端变更。
- 依据：用户 2026-11-26 拍板「**B：现在就切 jury.xin**」；台账 `docs/archive/handoffs/handoff-2026-11-23-batch-br-….md` `## Q` / `§Q-9`。
- 扫描脚本产物：`reports/fluxing-br-1g/pg-port-inventory.txt`（32,368 B，不入库）。
- 配套草案：`cloud/fluxing-sync/migrations-pg/0001_init.sql`（PG DDL，**未验证、未接入运行时**）。

---

## 0 一句话结论

**这不是「131 处 Cloudflare 调用逐条改写」，而是「两个 shim + 一个进程入口」。**

理由只有三条盘上事实，缺一条结论都不成立：

1. **SQL 方言命中 = 0** —— 全部语句是标准 `SELECT / INSERT / UPDATE / DELETE` + `?` 占位符，
   没有任何 `INSERT OR REPLACE` / `ON CONFLICT` / `strftime` / `json_extract` / `PRAGMA` /
   `AUTOINCREMENT`（在 `.ts` 内）等 SQLite 专有构造。
   ⇒ `?` → `$n` 可以机械改写，SQL 文本**不需要重写**。
2. **存储面只有两个对象** —— `env.DB`（69 处）/ `env.BUCKET`（8 处）；所有 handler 签名都是
   `(env: Env, …) => Promise<Response>`，没有 `c.executionCtx` / `waitUntil` / `DurableObject` /
   `KV` / `caches` / `scheduled` 依赖。
   ⇒ 用 D1-兼容 shim 顶替 `env.DB`、R2-兼容 shim 顶替 `env.BUCKET` 后，**处理器代码一行不用改**。
3. **R2 已经是结构类型** —— `src/r2-write.ts` 自己声明 `R2WriteBucket { put(key, value, options) }`，
   故意不用 `R2Bucket`，注释原文「这样单测可以传假桶」。
   ⇒ 换成文件系统或本地 S3 只需满足同一签名。

真正贵的不是源码，是**测试底座**：13 个 `*.test.ts` 共 5,270 行跑在
`@cloudflare/vitest-pool-workers`（miniflare 的 D1/R2 绑定）上，这套地基在 PG 上整体作废，
必须换（详见 §3 W5）。

---

## 1 盘上事实

### 1.1 源码调用面（`cloud/fluxing-sync/src`，12 个 .ts / 3,664 行）

| 文件 | 行数 | prepare | bind | all | first | run | batch | env.DB | env.BUCKET |
|---|---|---|---|---|---|---|---|---|---|
| auth.ts | 1,069 | 36 | 36 | 0 | 15 | 21 | 0 | 36 | 0 |
| sync.ts | 489 | 8 | 8 | 2 | 3 | 0 | 2 | 10 | 2 |
| mail.ts | 433 | 1 | 1 | 0 | 0 | 1 | 0 | 1 | 0 |
| sync-buckets.ts | 407 | 8 | 8 | 2 | 3 | 0 | 2 | 10 | 2 |
| wire.ts | 337 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| errors.ts | 204 | 1 | 0 | 0 | 0 | 1 | 0 | 1 | 0 |
| index.ts | 165 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| devices.ts | 154 | 8 | 8 | 2 | 2 | 2 | 1 | 9 | 0 |
| env.ts | 118 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| crypto.ts | 113 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| account.ts | 105 | 2 | 2 | 0 | 0 | 2 | 0 | 2 | 3 |
| r2-write.ts | 70 | 0 | 0 | 1 | 0 | 0 | 0 | 0 | 1 |
| **合计** | **3,664** | **64** | **63** | **7** | **23** | **27** | **5** | **69** | **8** |

- 占位符 `?` 共 **301** 个 ⇒ shim 需按语句内的出现顺序重编号为 `$1..$n`。
- 早前口径「131 处」= `prepare 64 + bind 63 + D1Database 1 + R2Bucket 3` 的并集，**数字对得上**；
  但它不是「131 处需要人工改写的地方」——`prepare`/`bind` 是同一个表达式链，改写发生在 shim 一侧。

### 1.2 SQLite 方言

`(?i)(insert or replace|insert or ignore|on conflict|strftime|julianday|json_extract|json_group|autoincrement|pragma|last_insert_rowid|changes()|collate nocase|glob|unixepoch|datetime()`
⇒ **命中 0 行**（12 个源文件全文）。

唯一带 SQLite 痕迹的是 `migrations/*.sql` 里的 `INTEGER PRIMARY KEY AUTOINCREMENT` 与
`CREATE INDEX … ON …`，属于 schema 层，按 §4 翻译即可。

### 1.3 入口与绑定

- `src/index.ts:155-163`：`export default { async fetch(request, env) { … } } satisfies ExportedHandler<Env>`
  —— **唯一** Cloudflare 专有入口。
- `src/index.ts:53`：`async function route(request: Request, env: Env): Promise<Response>`，
  路由全靠手写 `if` + `url.pathname` 匹配，**无 Hono / itty-router**。
- `src/env.ts:3-6`：`Env` 接口只有 `DB: D1Database` / `BUCKET: R2Bucket` 两个对象绑定，
  其余全是字符串变量 + 一个可选的 `EMAIL` 发信对象。
- 依赖：运行时**零第三方依赖**（`package.json` 无 `dependencies`），仅 devDeps =
  `@cloudflare/vitest-pool-workers` / `@cloudflare/workers-types` / `typescript` / `vitest` / `wrangler`。
  ⇒ 移植不需要拆任何 npm 依赖树。

### 1.4 迁移

| 文件 | 字节 | 内容 |
|---|---|---|
| 0001_init.sql | 4,067 | users / devices / refresh_tokens / sync_items / email_verifications / mail_outbox |
| 0002_password_resets.sql | 1,684 | password_resets（与 0001 的验证码表刻意分表） |
| 0003_sync_items_updated_index.sql | 1,508 | `idx_sync_user_updated(user_id, updated_at)` |
| 0004_sync_buckets.sql | 3,703 | sync_buckets + 3 个索引 |

合计 **10,962 B**，已全部按 §4 规则翻译进 `migrations-pg/0001_init.sql`（合并为单文件，附映射表）。

### 1.5 测试底座

`test/` 共 16 个文件 / **5,377 行**，其中：

- **13 个 `*.test.ts` / 5,270 行**，跑在 `vitest-pool-workers`（`wrangler` 提供的 miniflare 绑定）；
- `apply-migrations.ts`（6 行）—— 直接调 miniflare 的 D1 migration API；
- `check-endpoint-policy.mjs`（88 行）—— 纯文本检查，**与存储无关，可原样保留**；
- `env.d.ts`（13 行）—— `@cloudflare/workers-types` 的类型引用。

测试对 Cloudflare 的耦合度（`miniflare|D1Database|R2Bucket|env.DB|cloudflare:` 命中数）：
`zero-write-register-and-limits.test.ts` 21 / `device-token-limits.test.ts` 16 /
`password-reset.test.ts` 8 / `register-mail-failure.test.ts` 8 / `bucket-sync.test.ts` 6 /
`register-zero-write.test.ts` 6 / `api.test.ts` 5 / `push-quota-error-mapping.test.ts` 5 /
`mail-http.test.ts` 3 / `write-budget.test.ts` 3 / `apply-migrations.ts` 2 / `env.d.ts` 2 /
其余 4 个为 0（`d1-quota-error-mapping` / `last-seen-fail-open` / `r2-write` / `check-endpoint-policy`）。

---

## 2 移植形状

```
                    ┌──────────────────────────────┐
   HTTP (nginx) ───▶│ src/node-server.ts  (新, ~120 行) │
                    │  node:http → Request/Response │
                    └──────────────┬───────────────┘
                                   │ route(request, env)   ← 原样不动
                    ┌──────────────▼───────────────┐
                    │ src/index.ts  route()        │  ← 64 处 prepare/bind 全不动
                    │  13 个 handler (env, …)      │
                    └──────┬───────────────┬───────┘
                           │ env.DB        │ env.BUCKET
                    ┌──────▼──────┐ ┌──────▼───────┐
                    │ d1-shim.ts  │ │ r2-fs.ts     │
                    │  (新, ~300) │ │  (新, ~80)   │
                    └──────┬──────┘ └──────┬───────┘
                           │               │
                        pg.Pool       本机目录（或 S3）
```

**不变量**：`route()` 与 13 个 handler 的源码、`wire.ts` 的信封/校验、`crypto.ts` 的派生与比较、
`errors.ts` 的错误码映射，全部逐字保留。移植只发生在边界。

---

## 3 工作面清单（W1–W7）

| # | 工作面 | 产物 | 估算 | 风险 |
|---|---|---|---|---|
| W1 | **D1 shim** | `src/pg/d1-shim.ts`：`prepare(sql)` → `{ bind(...), all(), first(), run(), batch() }`，`?`→`$n`，复刻 `meta.changes` / `results` / `first` 返回 null 的语义 | 250–350 行 | **batch 原子性**：D1 `batch` 是事务；`pg.Pool` 上必须 `connect()` 单连接 + `BEGIN/COMMIT`（见 §4-R3） |
| W2 | **R2 shim** | `src/pg/r2-fs.ts`：`put / get / delete`，key 直接映射为相对路径（需做路径穿越防护） | 60–100 行 | key 含 `/` 与十六进制串，需校验不含 `..`；`get` 返回体的 `.arrayBuffer()` 形状要对齐 |
| W3 | **进程入口** | `src/node-server.ts`：`node:http` 收流 → 构造标准 `Request` → 调 `route()` → 回写 `Response`；`env` 从 `process.env` 装配 | 100–150 行 | 请求体大小上限（`MAX_PAYLOAD_BYTES` 8 MiB）与流式读取 |
| W4 | **PG schema** | `migrations-pg/0001_init.sql`（本轮已出草案） | — | 见 §4 五条语义差异 |
| W5 | **测试底座** | 二选一：① `@electric-sql/pglite`（进程内 PG，最贴近真 PG）② 对一台一次性 PG 跑集成。**13 个测试文件 / 5,270 行需要逐文件迁底座** | **本路线最大单项成本** | miniflare 的 `env.DB` 语义与 pglite 不完全等价（尤其 `meta.changes`） |
| W6 | **部署** | nginx vhost + certbot 证书 + systemd unit + `pg` 实例 + 备份 | 半天级 | ECS 根盘 **89% 已用 / 仅剩 14 G**；`5432` 当前**公网监听**须先收口；PG 大版本未证实 |
| W7 | **切源** | `WeaselServer/Cloud/CloudAccountDialog.cpp:60-61` 的 `kDefaultBaseUrl` 改指新域名；重新出包 + 装机 | 小时级 | **硬闸门：P0 假空必须先修**（§6） |

### 3.1 已经可以确定不需要做的事

- 不需要改客户端密钥派生 / 不需要重新加密任何数据 —— 服务端是**不透明密文存储**
  （`wrangler.toml [vars]` 原文：`KDF_ALGO` / `KDF_ITERS`「服务端只回显给客户端，
  绝不用它们派生任何东西」）。
- 不需要重写 64 处 `prepare` / 63 处 `bind`。
- 不需要拆依赖树（运行时零依赖）。
- 不需要改协议 / 改信封 / 改错误码（`wire.ts` 与 `errors.ts` 原样保留）。

---

## 4 PG DDL 翻译规则与五条语义差异

### 翻译规则

| SQLite / D1 | PostgreSQL | 理由 |
|---|---|---|
| `INTEGER PRIMARY KEY AUTOINCREMENT` | `BIGINT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY` | 要求单调不复用；导入生产数据后必须 `setval` 到 max(id)，否则主键冲突 |
| `INTEGER`（时间戳 / 计数） | `BIGINT` | 避免 INT4 上限；unix 秒到 2038 会溢出 |
| `INTEGER`（0/1 布尔位） | `SMALLINT` + `DEFAULT 0` | **不用 `BOOLEAN`**：JSON 里必须还是 `0/1`，改布尔会改变线上响应形状 |
| `TEXT` | `TEXT` | 无变化 |
| `REFERENCES x(id)` | 保留 | 见 R1 |
| `CREATE UNIQUE INDEX` / `CREATE INDEX` | 同名同列 | 便于逐条对照审计 |

### 差异（必须在 W1/W5 阶段验证，不能假设等价）

- **R1 · 外键是否强制**：D1/SQLite 默认**不强制**外键（`PRAGMA foreign_keys` 默认 off），PG **强制**。
  ⇒ 任何「先删父行、后删子行」的既有代码路径在 PG 上会直接报错。
  受影响点：`account.ts`（2 处 `DELETE FROM`）、`devices.ts`（1 处）。
  **处置**：不关 FK；用测试把删除顺序固定下来（禁止靠「PG 不报错」蒙过去）。
- **R2 · `batch()` 原子性**：D1 `batch` 保证全部成功或全部不生效。`pg.Pool.query` 每次可能取不同连接 ⇒
  在池上直接发 `BEGIN/COMMIT` **是错的**。必须 `const c = await pool.connect()` … `finally { c.release() }`。
- **R3 · `run()` 的 `meta.changes`**：D1 给 `meta.changes`；`pg` 给 `rowCount`。shim 必须映射，
  并且 `rowCount === null` 的情况要归一化为 0（否则 `if (res.meta.changes === 0)` 类分支会静默走错）。
- **R4 · 配额错误不再存在**：`errors.ts` 的 `isD1RowWriteQuotaError`（code 7500 三组正则）在 PG 上
  永远为 false。**保留代码不删**（无害），但 `storage_quota_exceeded` 这条 503 分支在 PG 侧成为死路。
- **R5 · 写预算护栏失去原意**：`WRITE_BUDGET_ROWS_PER_DAY=80,000` / `WRITE_BUDGET_BUCKETS_PER_DAY=4,000`、
  `LAST_SEEN_REFRESH_SECONDS=3600`、`REFRESH_TOKEN_GC_MAX_ROWS=200` 全部是**为 D1 免费额度服务的**。
  PG 上额度约束消失 ⇒ 这些值应回归正常语义（例如 `last_seen` 回到 300 s），
  **但必须在一个 ADR 里显式记录「为什么改」**，不能顺手改掉。

---

## 5 风险登记

| # | 风险 | 影响 | 处置 |
|---|---|---|---|
| K1 | **P0 假空未修就切源**（本地词典被读成空 ⇒ 整库还原 ⇒ 指纹全变 ⇒ 全量重推） | D1 上有 429 零写入当意外刹车；**PG 上没有等价刹车** ⇒ 可逆事故变不可逆删数据 | 硬闸门：假空修复 + 门禁用例之后再切 `kDefaultBaseUrl` |
| K2 | 生产 D1 数据导出与会话一致性 | 40,069 行 `sync_items` + 1,000 行 `sync_buckets` + R2 对象；导出期间仍在写入 | 导出方案要能给出「导出的 id 上界」；R2 对象数与 DB 行数双向核对 |
| K3 | 测试底座替换 | 5,270 行测试的可用性 | W5 单独一批，不进 W1–W4 的提交 |
| K4 | ECS 容量与端口暴露 | 根盘 89% 用满（剩 14 G）、`5432` 公网监听、`sales-bi-update.service` failed | 部署前收口：`5432` 只监听 `127.0.0.1`、清理磁盘、PG 大版本核实 |
| K5 | 域名 / 备案 / 证书 | 未证实 | 倾向用 `api.jury.xin` 子域（继承 `jury.xin` 已签发的 LE 与备案主体）；**待回读证实** |
| K6 | 桶化代码何去何从 | 客户端 1.3.11 已装机、默认 phase 0 | **保留不删**，长期冻结在 phase 0；阶段 3/4 在 B 路线下永不启用（需 ADR 记录） |

---

## 6 硬闸门（挂到部署路径上，不靠人记得）

1. **假空闸门**：`kDefaultBaseUrl` 改指新域名之前，必须已有 P0 假空的失败复现用例 + 修复后的通过用例。
2. **数据核对闸门**：`users` / `devices` / `refresh_tokens` / `sync_items` / `sync_buckets` 逐表行数与
   D1 侧只读回读一致；R2 对象数与 DB 内 `r2_key` 去重计数一致。
3. **回滚闸门**：切源后保留「改回 `api.aiec.fun` 一个字符串」的回滚路径；不得在同一批里删除旧端点代码。

---

## 7 未证实项（不得当结论）

1. `jury.xin` / `jury.fun` 的 ICP 备案主体与 `api.jury.xin` 可用性 —— 未回读证实。
2. ECS 上 PG 的**大版本**与是否已装（`5432` 监听 ≠ 已被本服务使用）。
3. 生产 D1 的**当前**行数与 `rows_written_24h`（本轮零写操作，未回读）—— 迁移前必须重新只读回读。
4. R2 是否还有 `get` 调用路径（`env.BUCKET` 8 处中已确认 `put` 6 波并发与 `account.ts` 的
   `delete`；`get` 是否存在未逐处核实）—— W2 开工前必须逐处读过。
5. `pglite` 能否完整复刻 D1 的 `batch` / `meta.changes` 语义 —— 未验证。

---

## 8 W0 依赖裁定：`pg` 登记为运行时依赖（2026-11-26 已执行并回读）

### 8.1 裁定

**`pg` 必须登记为 `dependencies`，精确版本 `8.23.1`（不加 `^`）。**
不是 `devDependencies`（Node 运行时真的 `import('pg')`），
不是 `optionalDependencies`（缺了它服务必须拒绝启动，不是降级运行）。

盘上落点：

- `cloud/fluxing-sync/package.json` → `"dependencies": { "pg": "8.23.1" }`
- `cloud/fluxing-sync/package-lock.json` → `"node_modules/pg"` 条目存在；root `""` 包块同步记录 `"pg": "8.23.1"`；锁文件 131,488 B → **136,991 B**
- `package.json` 的 `description` 同步订正为「双底座：Cloudflare Workers + D1 + R2 / 自建 Node + PostgreSQL + 本地对象存储」（原描述只写 Workers 路线，与路线 B 现状矛盾）

### 8.2 为什么之前是 `--no-save`，以及为什么必须改

W1–W4 当时的纪律是「运行时零 `dependencies`」，目的是让 `wrangler deploy` 不把 `pg` 吸进 Workers 包。
该目的**由另外两层机制保证，与依赖清单无关**：

1. `src/pg/dynamic-import.ts` 用 `new Function('return import(...)')` 隐藏 specifier ⇒ 静态分析看不到 `pg`；
2. 打包门禁用 `--external:pg` ⇒ 即使可见也不进包体。

而 `--no-save` 的实际后果是：**ECS 上 `npm ci` 装不到 `pg`，服务起不来**（缺 `pg` 时 `loadPgModule()` 抛错）。
`--no-save` 拦不住任何东西，只拦住了自己部署。

### 8.3 登记后门禁复跑（全部盘上回读，2026-11-26）

| 门禁 | 命令 | 结果 |
| --- | --- | --- |
| 类型 | `node node_modules\typescript\lib\tsc.js --noEmit -p tsconfig.json` | `TSC_EXIT=0` / `TSC_LINES=0` |
| node 打包 | esbuild `src/node-main.ts` `--external:pg` | `ESBUILD_EXIT=0` / `BUNDLE_BYTES=108734` / `BUNDLE_SHA256=1C2B0BB6CCEA4C56C8B56C481D398EDCFFDE753C507A6F603B5AFE0D892B9696`（与 §R 登记**逐字相等**） |
| 运行期自测 | esbuild `pg-port-selftest.ts` → `node` | `SELFTEST_PASS=31 FAIL=0` |
| **Workers 打包不变量** | `wrangler deploy --dry-run --outdir` | `WRANGLER_EXIT=0` / `index.js` **91,912 B** / gzip 20.53 KiB |

### 8.4 Workers 包零污染断言（本轮新增判据）

对 `wrangler deploy --dry-run` 产物 `index.js`（90,963 chars）逐串断言：

```
node_modules/pg = False    pg-protocol = False    pg-types = False    pg-pool = False
from"pg"        = False    require("pg") = False  new Function = False
```

`new Function = False` 是本轮信息量最大的一条：它证明 **`src/pg/**` 整棵子树都不在 Workers 入口
（`src/index.ts`）的依赖图里**，而不只是 `pg` 这一个包被 external 掉。所以「把 `pg` 加进
`dependencies`」对 Workers 部署体积、冷启动、bundle 解析都零影响 —— 不是「影响很小」，是结构上不可达。

### 8.5 已知的小差异（不掩盖）

自测 bundle 字节数 19,578 → **19,533**（−45 B）。唯一变量是 `package.json` 依赖清单变化导致的
esbuild external 元信息变化，自测项数未变（31/31 PASS）。node bundle 与 Workers bundle 的判据字节数
**没有**变化。

### 8.6 边界

- 本裁定只解决「ECS 上能不能装上 `pg`」，**不证明 ECS 侧可部署**（§7 未证实项 1–3 仍在）。
- `pg` 钉死 `8.23.1` 是为了与已跑通的 §S / §T 证据同源；升级需重跑 §S 的 DDL 与 §T 的端到端链。
- `package.json` 的 `scripts` 在本节写就时未改动；**W5 随后新增了 `test:pg`**（`vitest run --config vitest.pg.config.ts`，见 handoff `§U-3`），本节其余结论不受影响。**`npm ci` 的可复现性仍未在干净目录实测**（登记为待验证）。
## 9 W6 · PG 方言清点（2026-11-26 · 只读清点 · 未改产品代码 / 未跑门禁 / 未出包）

### 9.1 盘上回读基线

- 服务端 TypeScript：**17 个文件 / 5,019 行**（`[System.IO.File]::ReadAllLines().Count` 口径）。
  - `pg/` 三件 = `d1-shim.ts` 384 + `r2-fs.ts` 430 + `dynamic-import.ts` 50 = 864 行；
    两个 Node 入口 = `node-server.ts` 465 + `node-main.ts` 22 = 487 行。
  - 业务文件 12 个 = 5,019 − 864 − 487 = **3,668 行**（§0 曾记 3,664，差 4 行属统计口径差，以本轮为准）。
- 清点脚本：`reports/fluxing-br-1g/w6/audit.mjs`（`reports/**` 一律不入库）。
  产物：`summary.json` / `api-hits.csv` / `dialect-hits.csv`（表头 + **0 行**）/ `prepare-sql.jsonl`。

### 9.2 D1 API 调用面（泛型感知口径）

第一次统计用 `indexOf('.first(')`，漏掉了 `.first<T>(` 这类带类型参数的写法（报 0，真值 23）。
改为正则 `\.(first|all|run|batch|prepare)\s*(<[^>]*>)?\s*\(` 重算：

| API | 次数 | 备注 |
| --- | ---: | --- |
| `.prepare(` | 65 | 其中 63 个第一参可解析为字符串字面量 SQL |
| `.bind(` | 65 | 与 `prepare` 基本 1:1 |
| `.first(` | 23 | PG 侧返回 `row` 或 `undefined`，需与 D1 的 `null` 对齐 |
| `.run(` | 27 | `meta.changes` ← `rowCount`（shim 已实现） |
| `.all(` | 7 | `{ results }` 形态 |
| `.batch(` | 6 | R2 方案：单连接 + `BEGIN`/`COMMIT` |
| `env.DB` | 70 | 全部经 Env 注入 |
| `env.BUCKET` | 9 | `r2-fs` 侧 |

> 另有 `.exec(` 3 处命中，未逐点确认是否 D1 用法 ⇒ **不计入**本表。

### 9.3 SQL 语句形态（本轮最重要发现）

从 65 个 `.prepare(` 站点提取第一参、归一化空白后：

| 动词 | 条数 |
| --- | ---: |
| `SELECT ` | 30 |
| `UPDATE ` | 17 |
| `DELETE FROM` | 8 |
| `INSERT INTO` | 8 |
| 合计 | **63** |

**SQLite 专有方言零命中**（`cloud/fluxing-sync/src` 全量 ripgrep 复核同样为零）：

`ON CONFLICT` / `INSERT OR REPLACE` / `INSERT OR IGNORE` / `RETURNING` / `strftime` / `datetime(` /
`julianday` / `unixepoch` / `json_extract` / `json_each` / `json_object` / `PRAGMA` / `AUTOINCREMENT` /
`WITHOUT ROWID` / `IFNULL` / `IIF(` / `COLLATE NOCASE` / `GROUP_CONCAT` / `last_insert_rowid` / `changes()`

全部使用**位置占位符 `?`**：`prepare` 第一参内**静态可计数**的共 **135** 个（扫描器口径与朴素口径差额 = 0 ⇒ 没有「字面量里的 `?`」陷阱）。
另有 **4 个动态构造站点**在运行时展开 `map(() => '?')`（`src/sync-buckets.ts:180` / `:300` = N+2、`src/sync.ts:279` / `:399` = N+1，占位符数与绑定数同构）+ **1 个变量透传站点**（`src/auth.ts:418`，两个调用方 `src/auth.ts:487` / `:500` 各 1 个 `?` + 1 绑定）⇒ 这 5 个站点必须**逐点手核**。
（**旧记的 145 是过计数** = 135 + 6（4 个动态站点运行时展开的 N+1 / N+2）+ 4（`map(() => '?')` **构造行本身**的字面量 —— 与前 6 个是同一批站点的重复计入）；整文件朴素计数 386 个 `?` 含三元运算与可选参数，**同样不可用作迁移依据**。）

### 9.4 移植判定

1. **必改项只有占位符形态**：`pg` 不接受匿名 `?`，**135 处静态占位符 + 5 个动态/透传站点**需改成 `$1..$n`。两条路：逐处改 SQL，或在 `d1-shim.prepare()` 内做位置改写。后者只动一个收口点、零业务风险 ⇒ **已按此落码**（见 §9.6 / ADR 0079）。
2. **零方言语法** ⇒ 不需要「SQL 重写轮」，不存在逐条重写 `ON CONFLICT` / 时间函数的工作。
3. **`batch()` 6 处**仍是最大语义差异（R2 已定：单连接 + `BEGIN`/`COMMIT`），**未实测**。
4. **`INSERT` 一律无 upsert**：8 处 `INSERT INTO` 全部按 UUID 主键新建行；`sync_items` / `sync_buckets`
   的覆盖写作走 `DELETE` + `INSERT`（见 `account.ts:89` / `account.ts:94`）⇒ 唯一键冲突属**错误路径**
   而非正常路径，`isUniqueConstraintError` 的 PG 23505 映射必须实测（见 §7 未证实项）。

### 9.5 本节未坐实

- `.first(` 23 个站点的「无行」返回值两端是否等价（D1 `null` vs `pg` 的 `undefined`）。
- `.exec(` 3 处归属未确认。
- `batch()` 6 处失败是否真原子（R2 只是方案）。
- `rowCount === null` 的归一化是否覆盖全部 27 个 `run()` 站点（shim 有归一化，未逐点证实）。

### 9.6 W6 必改项已落码（2026-11-26）

**§9.5 四项的当前状态**：

| §9.5 原条目 | 当前状态 |
| --- | --- |
| `.first(` 23 处「无行」返回值两端是否等价 | **shim 侧已实测**（真 PG：`first()` 无行 ⇒ `null`，不是 `undefined`；无行 + 传列名同归一）；**D1 侧未跑对照** ⇒ 未证实 |
| `.exec(` 3 处归属未确认 | **`src/pg/d1-shim.ts` 头注释记为「全是 `RegExp.prototype.exec`」**；本批**未新增**逐点核实证据 ⇒ 与本节旧口径不一致，以 ADR 0079 为准，**不算闭合** |
| `batch()` 6 处失败是否真原子 | **shim 侧已实测**（真 PG：第二条违反唯一约束 ⇒ 整批 `ROLLBACK`、第一条不落库）；**6 个业务调用点未逐点验证** ⇒ 未证实 |
| `rowCount === null` 归一化是否覆盖全部 27 个 `run()` 站点 | **仍未逐点证实**（shim 有归一化，本批只测了 DDL 与 UPDATE 两条路径） |

**落码要点**（commit `5a65a6994832b4202486009613310c140d224848`，4 文件 +663 / −19）：

- `scanPlaceholders()` = **唯一**扫描实现（跳过字符串字面量 / 双引号标识符 / 行注释 / 块注释），`renumberPlaceholders()` 是薄封装；审计脚本复用同一实现（esbuild 打包后 import）⇒ **不存在第二套 SQL 解析器**。
- 断言落在 `PgCompatStatement.toQuery()`（唯一收口点，一次覆盖 all / first / run / batch）：**严格模式**，占位符个数 ≠ 绑定参数个数即抛；错误信息**只给两个个数**，不含 SQL 正文、不含绑定值。
- 静态审计（`reports/fluxing-br-1g/w6/audit-bind-count.mjs`，不入库）：`PREPARE_SITES=65 / DYNAMIC_SITES=7 / TOTAL_PLACEHOLDERS=135 / NAIVE_TOTAL=135 / EXPR_NAIVE_TOTAL=141 / LITERAL_DELTA_SITES=0 / MISMATCH_SITES=0 / NOBIND_SITES=0 / AUDIT_EXIT=0`。
- 65 处 `.prepare(` 里 **2 处是文档注释假阳性**（`src/errors.ts:36` / `src/pg/d1-shim.ts:215`）⇒ 真站点 **63**；两个假阳性站点贡献 0 个占位符，135 不受影响。
- ADR 0079 = `docs/adr/0079-pg-dialect-shim-boundary-replacement-and-placeholder-assertion.md`（282 行 / 15,692 B），D1–D10 决策：**R5 写预算本批不改**（理由 + 触发条件）、三条硬闸门一条都不解除、桶化 phase 0 冻结、外键不关 / 配额错误码不删。

**门禁结果**（`reports/fluxing-br-1g/w6/gate-w6.ps1` → `gate-w6.txt`）：`GATE_VERDICT=PASS` / `TSC_LINES=0` / node 包 110,414 B（sha256 `668CB107…`，基线 109,104 B ⇒ +1,310 B）/ `SELFTEST_PASS=37 FAIL=0`（基线 31）/ Workers 包 91,912 B **逐字未变** + 九条零污染全 False / workerd 13 files · 164 tests / test-pg **3 files · 24 tests**（基线 1 文件 4 用例）/ 负路径 `NEG_EXIT=1` + 中文错 + **PG 文件确实失败且纯单元文件确实仍通过**。

**push 已完成（走直连）**：远端 `refs/heads/feat/100-cloud-sync` = `8645b2778125f26c66e586feeeb704485274b26d` = `git rev-parse HEAD`（逐字相等，ahead 0 / behind 0，快进推送 `442e0ba..8645b27`）。
**仍未闭环**：**代理路线仍断**（`Recv failure: Connection was aborted`）⇒ 后续远端操作须显式 `-c http.proxy= -c https.proxy=` · `test-pg/**` 不在 `tsconfig.json` 的 `include`（新增测试无类型闸门）· §9.5 表内三条未证实项照旧。详见 handoff `## W`。