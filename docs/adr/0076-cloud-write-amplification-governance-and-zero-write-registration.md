# ADR 0076: 推送写放大治理与失败注册零写入化

> 批次 BR · 承接 [ADR 0075](0075-register-mail-rejection-semantics-and-failed-registration-rollback.md) · 关联 spec `100-fluxing-cloud-account-sync`

## Status

**Accepted**（2026-11-23：决策已采纳 + 服务端改码已落码入库 + 已部署生产；判据 1 / 2 / 4 达成，**判据 3 未完成**，详见文末「生产复验结果（2026-11-23）」）

**修订（2026-11-24）**：用户已明确授权客户端改动（BR-①c）⇒ **D2-5a** 把 D2-5 的口径与判据补齐为**可实施决策集**。
D2-5 原文的**原则与判据不变**；服务端决策（D2-1 / D2-2 / D2-3 / D2-4 / D3-* / D4-*）**逐字未动**；
本轮改动**仅限 `WeaselServer/Cloud/**`（客户端）**，**不改服务端协议**、**不改 `include/WeaselIPCData.h`**、**不改 `librime/`**
⇒ **无需 `wrangler deploy`**，但必须**出包装机 + 装机实证 + 真机判据复验**。

本 ADR **替代** ADR 0075 决策 3 在「注册 / 重发」路径上的**实现方式**（事后删除 ⇒ 事中不写）；
ADR 0075 的**已发布契约**（错误语义、回滚三态、三个 ASCII 诊断头）全部保留，见 D0-2 / D1-5。

---

## Context

### 0. 本 ADR 要解决的两个问题（批次 BR 范围，交接 §9-3）

1. **推送写放大治理**，拆两条线：
   - (a) `sync_items` **全量推送**（单次 40,061 行）；
   - (b) `devices` / `refresh_tokens` **无上限增长**（真实账号已累积 22 devices / 90 tokens）。
2. **失败注册「零写入」化**：把 `users` / `email_verifications` 落库挪到**发信成功之后**，失败注册**一行都不写**（重发路径同样覆盖）。

### 1. 事实基线

> 标注约定：【实测】= 生产 D1 查询 / HTTP 响应可复算；【读码】= 本仓库源码逐行确认；【推断】= 由前两者推导，未单独复验。

#### 1.1 写放大规模与计费口径

- 【实测】2026-11-22 UTC 日 D1 行写入：`sync_items` 40,060（累计 40,061）、`refresh_tokens` 32（累计 98）、
  `users` 11、`email_verifications` 11、`devices` 1 ⇒ 合计 ≈ 40.1k 行，**99.9% 是 `sync_items`**。
- 【实测】40,061 行全部属真实账号 B、`type=userdb`、`COUNT(DISTINCT item_key)` = 总行数 ⇒ **行级无重复**；
  时间分布 UTC 11:40 / 12:40 / 13:40 = 14,621 / 16,224 / 9,215 ⇒ 与「本机无输入会话 ⇒ 机会性同步一次性推全量词典」吻合。
- 【实测】D1 免费额度按**行写入**计；耗尽时生产返回 `[code: 7500] exceeded D1's free tier daily row write limit`
  ⇒ 一次全量推 ≈ 40% 日额度；**两次即打爆**，且打爆后**该 DB 内所有账号的所有写操作一起失效**
  （BQ 批实测：`UPDATE` / `DELETE` 一律被拒，仅 `INSERT` 尚可用）。
- 【读码】写路径（`cloud/fluxing-sync/src/sync.ts` `handlePush`）：查既有行 → **HLC 守卫**
  （`compareHlc(stored, item.hlc) >= 0` ⇒ 整条跳过：不写 R2、不写 D1、不推进 `server_seq`）→
  **R2 先写**（并发 6，ADR 0074）→ **D1 再写 `DELETE + INSERT`**（每条变更 **2 次行写入**）→
  `server_seq` = 本次真正写入条目的最大 `id`。

#### 1.2 客户端**已经是**增量推送，真正的放大源是「本地状态丢失」

- 【读码】`WeaselServer/Cloud/CloudSyncEngine.cpp:207-210` 注释：推送集合由 **sidecar 差分**算出
  （只有本地改动过的条目才进集合）。
- 【读码】`WeaselServer/Cloud/CloudSyncService.cpp:1003-1019`：sidecar 里 payload 哈希没变 ⇒ 沿用旧 HLC；
  变了**或 sidecar 无该条记录** ⇒ `HlcTick(now)` 分配**新 HLC**（`ms` = 当前墙钟 ⇒ **必然大于**服务端已存 HLC）
  ⇒ 服务端守卫**必然放行** ⇒ 该条被重写。
- 【读码】sidecar 落点 = 用户数据目录（`CloudAccountDialog.cpp:85-89` `ResolveFluxingUserDataPath`），
  与凭据 blob **物理分离**。
- 【推断】⇒ **sidecar 丢失 / 换机 / 重建用户数据目录 ⇒ 整库被当成「本地改动」⇒ 40,061 行全部重写
  （≈ 80,122 行写入）**。这是当前最大、最可复现的放大源；且客户端**同一轮已经拉取并解密了远端副本**（merge 阶段），
  具备判定「内容其实没变」的现成条件（见 D2-5）。

#### 1.3 服务端**无法**在密文域做内容去重（该路线关闭）

- 【读码】条目是客户端端到端加密：AAD = `BuildAad(item_key, type, hlc)`、nonce 12 B、**`hlc` 参与 AAD**
  ⇒ 内容未变但 HLC 变 ⇒ 密文必变；服务端只见密文 ⇒ **无法判定「内容未变」**。
- 【推断】若改传明文内容哈希，服务端即可去重，但会把词条**内容指纹**暴露给服务端 ⇒ 破坏端到端密文语义 ⇒ 拒绝（见 D2-2）。

#### 1.4 设备与令牌无上限增长

- 【读码】`WeaselServer/Cloud/CloudCrypto.cpp:403-425` `GenerateDeviceId` = `BCryptGenRandom` 8 字节 ⇒
  16 位十六进制**随机** id；它由**凭据生命周期**决定（`CloudAccount.cpp:215` / `:365` 仅在凭据为空时生成），
  随凭据 blob 持久化（`CloudCredentialStore.cpp:114`）⇒ **不是「机器」的稳定标识** ⇒ 凭据重建一次 = 服务端多一台「设备」。
- 【实测】真实账号 B：**22 devices / 90 refresh_tokens**（4 个真实账号合计 25 devices / 98 tokens）。
- 【读码】`upsertDevice` 只对**相同 `device_id`** 做 UPDATE；新 `device_id` ⇒ INSERT 新行；**无任何清理 / 上限**。
  `issueTokenPair` 每次签发 access + refresh **两行**；`handleRefresh` 轮换时把旧 refresh 行标 `revoked_at`
  后再签两行 ⇒ **一次续期净增 2 行**；`refresh_tokens` **只增不减**。
- 【读码】`migrations/0001_init.sql`：`devices` 有 `UNIQUE(user_id, device_id)`（只保证同 id 不重复）、
  `refresh_tokens.token_hash UNIQUE`；两者**都没有行数上限 / 保留窗口 / GC 触发点**。

#### 1.5 失败注册「先写后发信」

- 【读码】`cloud/fluxing-sync/src/auth.ts:427-484` `handleRegister`：查重 → **INSERT `users`（:451-455）** →
  `createVerificationCode`（:249-265，**INSERT `email_verifications` :259-263**）→
  `sendVerificationMail`（`mail.ts:408-419`）⇒ 发信失败再靠 `rollbackRegister`（:349）**事后删**。
- 【读码】`handleResend`（:535-575）同构：先 INSERT 验证码行再发信；失败只删本次验证码行、**保留 `users`**。
- 【读码】`createVerificationCode` 只做三件事：生成 6 位码 → pepper 哈希 → INSERT；
  而 `sendVerificationMail(env, email, code, doFetch)` **只依赖 `code` 明文字符串**、不读库
  ⇒ **验证码完全可以先在内存里生成、先发信、成功后再落库**（这是 D1 成立的前提）。
- 【实测】BQ 批已证明「事后回滚」在额度耗尽时**回滚本身也会被拒** ⇒ 生产出现 `X-Fluxing-Mail-Rollback: failed`
  ⇒ 库里留下 `users` + `email_verifications` 两条残留（**占号** + 吃掉 60 秒重发窗口，真正的根因被掩盖）。

---

## Decision

### D0 范围与不变量

- **D0-1** 本批改码**只动服务端** `cloud/fluxing-sync/**`（含 `migrations/` 与 `test/`）+ 文档。
  **不动** IPC 协议与 `include/WeaselIPCData.h`、**不动** `WeaselServer/**` 客户端、**不动** `librime/`、
  不 bump 版本号、不新增第三方依赖
  ⇒ **不触发「IPC 四边同步」红线**（注册 / 验证 / 重发链路本就不走 IPC：`include/WeaselIPCData.h` 云账号相关检索零命中）。
- **D0-2** ADR 0075 已发布契约**全部保留**：上游 4xx ⇒ **422 `mail_recipient_rejected`**；
  401/403 与配置类 ⇒ **500 `server_misconfigured`**；429 ⇒ **503 `mail_rate_limited`**；
  409/5xx/网络 ⇒ **503 `mail_send_failed`**；D1 配额错误 ⇒ **503 `storage_quota_exceeded`**；
  三个 ASCII 诊断头 `X-Fluxing-Mail-Provider-Status` / `X-Fluxing-Mail-Rollback` / `X-Fluxing-Mail-Rollback-Error` 继续存在。
- **D0-3** 本 ADR 只**替代** ADR 0075 决策 3 在注册 / 重发路径上的实现方式（事后删 ⇒ 事中不写）；
  **回滚能力本身不撤销**，退化为「发信已成功但落库失败」这一窄窗口的兜底（D1-5）。
- **D0-4** **客户端零改动**是本批硬约束 ⇒ 所有新错误码只允许通过**既有** `error.message` 中文文案透传
  （客户端 `WeaselServer/Cloud/CloudAccount.cpp:92-106` `ExtractServerError` 只读 `message`），
  **禁止**要求客户端去读 `error.code`。

### D1 失败注册零写入化（子项② · 服务端）

- **D1-1 验证码生成与落库解耦**：拆出纯函数产出 `{ code, codeHash, createdAt, expiresAt }`（**不发信、不落库**）；
  `createVerificationCode` 改为「纯生成」，落库动作独立为 `persistVerificationCode(...)`。
- **D1-2 注册路径新顺序**：查重（只读）→ 生成验证码（内存）→ **发信** → 成功 ⇒ `INSERT users` ⇒ `INSERT email_verifications`；
  失败 ⇒ **一行都不写**，直接返回上游失败语义。
- **D1-3 重发路径新顺序**：查用户（只读）→ 生成验证码（内存）→ **发信** → 成功 ⇒ `INSERT email_verifications`
  （**旧码行保持不变、仍然有效**，与今天「失败只删本次行」的语义一致）；失败 ⇒ 一行都不写。
- **D1-4 并发与唯一性**：`users.email` UNIQUE ⇒ 落库顺序固定为「**先 users 后验证码**」
  ⇒ 同邮箱并发注册时**败者卡在 `users` INSERT**（⇒ 映射为 **409 `email_taken`**），不会留下验证码孤儿行。
  代价：败者可能已经发出一封多余邮件（可接受，不改语义）。
- **D1-5 窄窗口兜底（已知且刻意保留的例外）**：发信成功但落库失败（典型：D1 配额耗尽）
  ⇒ 用户手上有一封**用不了**的验证码邮件 ⇒ 返回 **503 `storage_unavailable`** + 中文文案
  「验证码已发送，但服务器存储暂时不可用，请稍后重试或重新获取验证码。」，并**尽力回滚**已插入的 `users` 行
  （沿用 `rollbackRegister` 的 DELETE 优先 / UPDATE 失效化降级 ⇒ 三态 `ok|degraded|failed` 与 `X-Fluxing-Mail-Rollback` 头照旧）。
  此窗口内**零写入不可保证**。
- **D1-6 频率闸口径不变**：`assertResendAllowed` 仍以 `email_verifications` 已有行为准
  ⇒ 失败注册**不吃掉** 60 秒窗口（与 BQ 批修复后口径一致），且**不消耗任何 D1 额度**。
- **D1-7 口令重置路径本批不碰**（`password_resets` 先落库后发信，与本项同构）：ADR 0075 决策 7 已明确不碰
  ⇒ 本 ADR 只记录平移配方（同一套「纯生成 → 发信 → 落库」模式），列入后续批次候选。

### D2 `sync_items` 写放大治理（子项①(a)）

- **D2-1 基线口径文档化**（作为后续所有容量讨论的基准）：每条词典行 = **1 行 D1** + **1 个 R2 对象**；
  已存在条目的每次变更 = **2 行 D1**（DELETE + INSERT）；首推全量 = N 行。
- **D2-2 关闭「服务端密文域内容去重」路线**（理由见 Context 1.3）⇒ 后续不再讨论「服务端如何识别内容未变」。
- **D2-3 每用户每日行写入预算护栏（新增，服务端）**：在 `handlePush` 落库**之前**，
  先估算本批将产生的 D1 行写入数 `planned`（= 守卫放行条目数 × 每条 1 或 2 行），
  并用**纯读**方式取该用户 UTC 今日已写行数
  （`SELECT COUNT(*) FROM sync_items WHERE user_id = ? AND updated_at >= <今日 00:00 UTC>`）
  ⇒ 估算值 + `planned` > **80,000 行/账号/UTC 日**（环境变量可覆盖）⇒ **整批拒绝**，
  返回 **429 `write_budget_exceeded`** + 中文文案
  「今日云端写入已达上限，请明天再试或减少本机词典规模。」⇒ **零写入**（拒绝发生在落库之前）。
  - **口径**：这是**估算**（同一行当日被反复写只计 1）⇒ 护栏针对「一次性全量 / 批量重推」这一真正的放大源，
    **不宣称**它是精确计费器。
  - **收益**：把「一个人（或一台机器）打爆全库额度 ⇒ 所有账号一起不可用」降级为「单账号可诊断的 429」。
  - 推送到一半被 429 截断是可接受状态：客户端只把**服务端真的收下**的条目标记为已同步（既有逻辑），
    余下条目下轮续传。
- **D2-4 本批不改 `DELETE + INSERT` 写序**：理由 = `sync_items.id`（AUTOINCREMENT）**就是** `server_seq`
  **就是** pull 游标 ⇒ 改成 UPSERT 会让「已拉过的条目更新后不再被拉到」（**正确性回归**）；
  若引入独立 `seq` 列，则需把现有 40,061 行**回填**（一次 ≈ 40k 行写入 = **当日额度直接打爆**）⇒ 本批不做，
  登记为后续批次（需专门低额度窗口 + 客户端兼容验证 + 回滚预案）。稳态下变更条目数量很少，2× 系数是可接受的小项。
- **D2-5 根治项在客户端（需单独授权，本批不动客户端）**：客户端**已经**在同一轮里拉取并解密远端副本（merge 阶段）
  ⇒ 只需在「本地条目在 sidecar 中**无记录**」时，**若远端同 `item_key` 的明文 payload 逐字一致 ⇒ 沿用远端 HLC**
  （不分配新 HLC）⇒ 服务端 HLC 守卫命中 ⇒ **零写入**。
  **判据**：删除 sidecar 后同步一次，D1 `sync_items` 今日写入增量 = **0**（当前行为 ≈ 40,061）。
  该项触及 `WeaselServer/Cloud/**`，而交接 §0 明文「未获明确指令不得改客户端」
  ⇒ **列为 BR-①c 子批次，需用户明确授权后开工**；本 ADR 先把口径与判据写死，避免授权后再讨论。
- **D2-5a（2026-11-24 修订 · BR-①c 客户端根治方案）把 D2-5 补齐为可实施决策集**：用户已授权客户端改动
  ⇒ 本条**取代** D2-5 的「待授权」措辞，并把「远端 HLC 沿用」拆成可落码、可单测、可判据的九个子决策。
  **范围**：仅 `WeaselServer/Cloud/**`（客户端协调器 + 侧车文件）；**不改服务端协议、不改 `include/WeaselIPCData.h`、不改 `librime/`**。
  - **D2-5a-0 事实基线（已取证，作为设计前提）**：服务端 `handlePull` 是
    `SELECT ... WHERE user_id = ? AND id > ? ORDER BY id ASC LIMIT ?`，且 `sync_items` 对 `(user_id, item_key)` 唯一
    ⇒ `since=0` 的分页拉取枚举的是**当前库存**而非版本历史；服务端 HLC 守卫为
    `compareHlc(stored, item.hlc) >= 0 ⇒ skipped`（跳过时**不写 R2、不写 D1、不推进 `server_seq`**）。
    ⇒ **客户端只要能把远端明文 payload 与本地逐字比对，就能自己算出「不需要写」**——这是本方案可行的基石。
  - **D2-5a-1 顺序倒置（协调器主流程改序）**：一轮同步改为
    「导出本地 → **先拉远端** → 分配 / 复用 HLC → 合并 → 回写本地 → 推送 → 更新侧车」。
    现行顺序是「分配 HLC → 拉远端 ⇒ 分配 HLC 时**还看不到远端**」⇒ 改序是 R1 成立的前提。
  - **D2-5a-2（R1 · 远端 HLC 沿用 · 零写入主体）**：本地条目与本轮远端条目同 `item_key` **且明文 payload 逐字节一致** ⇒
    `L.hlc := R.hlc`、**不标记本地改动**、**不进推送集合**、侧车记 `(key, R.hlc, sha256(payload))` ⇒ 该条目**零写入**。
    判别条件是**明文逐字节相等**（不是 HLC 相等、不是密文相等）：远端 payload 解密后与本地比较，
    相等即证明「本地这条就是云端那条的当前版本」⇒ 服务端 HLC 守卫必然命中（`stored == R.hlc`，`compareHlc >= 0`）⇒ 服务端零写入。
  - **D2-5a-3（R2 · 库存未证实不推送）**：闸门条件 = **本轮拉取没有扫完整个云端库存**（`has_more == true`）
    ⇒ 本轮**不得**推送「既无侧车记录、又不在本轮远端集合里」的本地条目（它们**延迟**到后续轮次）。
    ⇒ **侧车文件格式不变**（不引入新版本行、不扩展既有字段取值域 ⇒ 兼容性风险为零）；
    **「库存是否扫完」不落盘**（每轮由服务端当场回答；扫描进度由**既有游标**推进承载）⇒ 本批**零新增状态面**。
    **【2026-11-24 修订 · 口径修正】** 原文写的是「`pull_start_cursor == 0`（本轮从零做全量库存扫描）**且**
    `has_more == true`」，实现与 101 批 spec 改取其**超集**（触发条件只看 `has_more`，解除条件 = 某轮
    `has_more == false`）。**修正理由**：服务端单轮只吐 1,000 条（100 条 × 10 页）⇒ 大账号要**多轮**才扫完库存
    （见 D2-5a-8 ①）；若闸门只在首轮成立，**第 2 轮起**就会把「本轮尚未见到」的本地条目当成新增推送
    ⇒ 服务端 HLC 守卫再次落空 ⇒ 写放大原样复现 ⇒ **D2-5a-7 的判据不成立**。
    ⇒ 在「服务端按 id 游标分页 + 单轮上限」的前提下，**超集口径是逻辑必然**，不是可选修正。
    **副作用如实登记**（见 D2-5a-8 ⑤）：侧车完好但云端增量超过单轮拉取上限时，新增本地词条也会被
    **延后**到扫完那一轮才推送（延迟，不丢数据）。
  - **D2-5a-4（R3 · 侧车重建）**：R1 命中条目 + 远端胜出条目都写入侧车（携带**远端 HLC**）⇒ 扫描期**逐轮重建**侧车
    ⇒ 扫描完成后回到常规增量行为。
  - **D2-5a-5（R4 · 分歧条目 · 本地胜出）**：同 `item_key` 但**明文 payload 不同** ⇒ 视作**本地改动**（分配新墙钟 HLC + 推送），
    与现行行为逐字一致。**已知代价如实登记**：侧车丢失后丧失「按 HLC 定序」的能力，可能覆盖另一台设备的更新版本；
    本轮**不修**（属 D2-5a-8 已知边界）。
  - **D2-5a-6（R5 · 观测口径）**：新增**无条件**诊断行
    `[cloudsync] reconcile scan_complete=<0|1> matched=<n> deferred=<n> divergent=<n> pushed=<n>`
    （本轮失败也照打）⇒ 判据取证**不依赖**界面显示。
  - **D2-5a-7 判据**：删除侧车后同步 ⇒ D1 `rows_written_24h` **增量 = 0** **且** `sync_items` 行数增量 = **0**
    （现行行为 ≈ 40,061 行）；取证须同时确认服务端拉取自愈 `DELETE`（`handlePull` 的 `healed` 分支）**为空**
    （自愈 DELETE 也是写入，会破坏「零写入」判据）。
  - **D2-5a-8 已知边界（不修，如实登记）**：① 单轮 1,000 条上限 ⇒ 大账号需**多轮**扫完库存
    （扫描未完成的轮次**零写入**）；② 分歧条目本地胜出的既有风险**不修**；
    ③ **口令轮换路径（`keyfp` 不匹配 ⇒ 全量重推）本质不可免**（`enc_key` 变 ⇒ `item_key` 的 MAC 身份变 ⇒ 云端旧行解不开）
    ⇒ 不在本项范围；④ 不改服务端 ⇒ **无需 `wrangler deploy`**，但必须**出包装机 + 装机实证 + 真机判据复验**；
    ⑤ **【2026-11-24 新增】新增本地词条的上云被延后**：R2 闸门在「库存未扫完」期间**持续生效**
    （D2-5a-3 修订口径）⇒ 侧车完好但云端增量超过单轮拉取上限时，本地新词条要等**扫完库存的那一轮**
    才推送 ⇒ **延迟，不丢数据**；该延迟是「服务端按 id 游标分页 + 单轮 1,000 条上限」的**结构性后果**，
    本批**不通过放大单轮页数**来消解（会显著拉长单轮耗时并放大 R2 读取量，收益不抵成本）；
    若后续要压缩延迟，属独立议题（BR-①e 候选），须另立 ADR。
  - **D2-5a-9（a-9 · 语义指纹 · 易变字段剔除 · 2026-11-24 修订）**：在 a-8 之上**再追加一个子决策** ⇒ 把
    「本地改动判定」与「回写判定」从**整条 payload 的 sha256** 改成**语义指纹**（剔除 `d=` / `t=` 两个易变字段）。
    **触发事件**：2026-11-24 AC-09 真机判据**第 1 次复跑 = FAIL**（判据未达成），根因已坐实（详见
    `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` 的 §F）。
    - **根因（已坐实，2026-11-24 复跑取证 + 逐行读码）**：
      1. **侧车哈希取的是整条 payload**：当前 payload 哈希 = `Sha256Hex(entry.payload)`
        （`WeaselServer/Cloud/CloudSyncService.cpp:1127` 算当前值、`:1381` 写入侧车行、`:1578` 序列化落盘、
        `:1630` 解析回读；字段定义 `WeaselServer/Cloud/CloudSyncService.h:243`、文件格式注释 `:329` / `:460`）。
      2. **payload 形态** = `<码><空格><TAB>c=<commits> d=<dee> t=<tick>`
        （装配 `WeaselServer/Cloud/CloudUserDictIo.cpp:97-108`；切分 `SplitDictPayload` `:326-337`；
        形态注释 `:343`；`t=` 解析 `:169-174`）。
      3. **回写改写 `d=` / `t=`**：客户端每轮同步都会把合并结果**回写本地词典**（`restore_user_dict` → rime
        `UserDbMerger`，调用点 `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp:157-159`）⇒ merger 重算 `d=`
        （`librime/src/rime/dict/user_db.cc:208` / `:216`，`algo::formula_d(0, their_tick_, v.dee, v.tick)`）
        并把 `t=` 改写成 db 当前 tick（`librime/src/rime/dict/user_db.cc:221`，`o.tick = max_tick_`）
        ⇒ **同一未改词条的 payload 哈希每轮必变**。
      4. **后果（a-8 ② 写放大的真实机理）**：本地改动判定（侧车哈希 vs 当前 payload 哈希，
        `WeaselServer/Cloud/CloudSyncService.cpp:1151` 分歧分支 / `:1164` 沿用分支）**每轮都判「改过」**
        ⇒ 每轮分配**新墙钟 HLC**（`HlcTick(&clock_, now_ms)`，`:1152` / `:1174`）⇒ 服务端 HLC 守卫失效
        ⇒ 既有 `sync_items` 行被反复 UPDATE。
      5. **回写判定是同一个病灶的第二处**（`WeaselServer/Cloud/CloudSyncService.cpp:1194-1205`：
        `merged[i].payload != local_entries[index].payload` ⇒ `changed = true` ⇒ 整库回写）
        ⇒ **漂移源头（回写）与受害面（哈希判定）互为因果** ⇒ 只改其一不断根 ⇒ 必须两端同改。
    - **a-9① 语义指纹（新增口径）**：新增「语义指纹」= payload 的 **code 段 + 值段剔除 `d=` / `t=` token 后的剩余**
      （**保留 `c=` 学习次数**）⇒ 指纹对回写导致的 `d=` / `t=` 漂移**不敏感**、对**真实学习**（`c=` 增长）**敏感**。
      指纹实现仍复用既有切分函数 `SplitDictPayload`（`WeaselServer/Cloud/CloudUserDictIo.cpp:326-337`）
      ⇒ **不新增 payload 形态解析器**（口径不漂移，宪法 R7）。
    - **a-9② 判定口径统一（一处改，处处用）**：**本地改动判定**（`WeaselServer/Cloud/CloudSyncService.cpp:1135` 取语义指纹 /
      `:1151` 基线四态）、**R1 远端一致判定**（`:1163`）、**R4 分歧判定**（基线态 `:1148-1161`）、
      **回写判定**（`:1242-1249`）、**侧车哈希字段**（`:1433` 写行 / `:1451` 旧口径就地重锚 / `:1668` 序列化 /
      `:1696` 解析）**一律改用语义指纹**；侧车哈希字段改存 **`s2:<sha256(语义指纹)>`**
      （前缀标识口径版本 ⇒ 与旧裸 hex **天然可区分**，无需另开版本号）。
      **实施后坐标（2026-11-24 a-9 落码）**；三个 helper = `:1625` `SemanticFingerprint`（复用
      `SplitDictPayload` ⇒ 不新增第二套 payload 解析器）/ `:1657` `SemanticHashHex` / `:1664` `IsSemanticHash`。
    - **a-9③ 旧口径兼容（旧侧车基线不可比）**：旧侧车行（**裸 64 位 hex、无 `s2:` 前缀**）的哈希**与语义指纹不可比**
      ⇒ 本轮**不推送**（避免一次性 4 万条洪峰）+ **抬高本地 HLC**（本地胜出，防止云端旧版回灌覆盖本地）
      + 侧车重建时把该行**升级**为 `s2:` 口径（重新锚定基线）⇒ **之后的真实改动仍能被发现**。
    - **a-9④ 回写止血（漂移源头同时断掉）**：回写判定改语义口径 ⇒ `d=` / `t=` 差异**不再触发整库回写**
      ⇒ rime 回写不再改写本地词典 ⇒ **漂移源头同时断掉**（双管齐下：判定侧不敏感 + 回写侧不产生漂移）。
    - **a-9⑤ 已知边界（如实登记，不修）**：① 云端保留旧的 `d=` / `t=` 值、**不再随本地更新**（语义口径忽略它）；
      ② 升级前发生、且**从未上云**的本地改动**不会自动上传**（推荐迁移 = 升级后**删一次侧车**，
      走完整库存重扫、逐条与云端比对）；③ 本地 54,536 条**从未上云**的条目仍按 R2 闸门（a-3）**延后到库存扫完**
      （约 41 轮）⇒ 属**独立议题 BR-①e**，需用户拍板（是否允许推送新条目 / 是否提高单轮拉取页数 / 是否升级 Workers Paid）；**2026-11-24 用户拍板：BR-①e = 维持现状**（库存扫完前不推新条目 / 不提高单轮拉取页数 / 不做 pacing）⇒ **本议题关闭、不另立 ADR**；Workers Paid 与 BR-② 仍待拍板。
      ④ 修复后单轮写入量由「每轮 3,072+ 行」降为 **0**（AC-09 场景），但**库存扫完后会一次性补推那 54,536 条**
      ⇒ 服务端**每账号每日 80,000 行预算**（D2-3）与 **D1 免费额度**需一并复核
      （BR-② / Workers Paid **待用户拍板**）。
- **D2-5a-9 实施状态（2026-11-24 · BR-①c 第二轮 · 已落码）**：a-9①…a-9⑤ **全部已实施**并通过门禁
  （客户端 `TestFluxingCloud` **PASS=813 FAIL=0**，基线 787 ⇒ +26，既有 787 条逐条保留；构建 0 错误；改动文件 U+FFFD=0；
  clang-format 改动 hunk 内 0 漂移）⇒ 出包 **43,001,594 B** / sha16 `1E955184BB1F099D`；装机 `INSTALL_EXIT=0`；
  **三处 `WeaselServer.exe` sha256 逐字节相等** `A106522FD8F5AC31ABBF392FA906F76ABFD73032A73640E0FE45742057394A94`。
  **但 AC-09 真机判据仍为 FAIL（未复跑）**：修复后真机两轮**在拉取阶段即 503 早退**（生产 D1 免费额度耗尽 `code 7500`；
  且服务端 `cloud/fluxing-sync/src/auth.ts:1043-1045` 的 `devices.last_seen` 刷新是**读路径上的写** ⇒ 额度耗尽时**拉取也 503**）
  ⇒ 未进对账循环 ⇒ `semantic legacy_rows` 诊断行**零出现** ⇒ **修复尚未取得真机证据**（先前「真机零推送 ⇒ 修复生效」的推断**已纠正**）。
  复跑 = 定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（北京时间 **2026-11-25 08:10:46**），**探针升 v4**
  （`round_ran` 须**同见** `reconcile` 行**且** `semantic legacy_rows=` 行；主判据改 `sync_items` **表级**写入增量，
  因 `devices.last_seen` 属读路径写、不得计入写放大 ⇒ `rows_written_24h` 全局计数降为严格变体）。
  **只有 `AC09_VERDICT=PASS` 才可把 a-9 记为「判据达成」。**
- **D2-6 sidecar 落点加固（客户端，低优先，BR-①d 候选）**：sidecar 与凭据 blob 同目录并纳入用户数据备份 / 迁移清单
  ⇒ 降低「重建用户数据目录 ⇒ 全量重推」的触发概率。

### D3 `devices` / `refresh_tokens` 无上限增长（子项①(b) · 服务端）

- **D3-1 设备上限 32 台/账号**：登录落设备时统计该用户 `devices` 行数；≥ 32 ⇒ 先回收**已撤销且最久未见**的设备行
  （DELETE，1 行写入）；仍 ≥ 32 ⇒ **拒绝登录**并返回 **429 `device_limit_reached`** + 中文文案
  「此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。」
  ⇒ **不自动删除任何仍在使用的设备**（不删用户数据）。
- **D3-1a（2026-11-23 改码轮修订）设备上限只约束「新设备」**：改码轮发现 D3-1 的字面口径（≥32 ⇒ 拒绝**任何**登录，含已存在的 `device_id`）会造成**自锁**：账号恰好有 32 台活跃设备时，用户在**任何一台**上都登不上（access token 过期且 refresh 已被 GC 时需服务端人工介入）⇒ 与 D3-1「不删用户数据」的初衷相悖。
  ⇒ **修订**：设备上限只在**新增设备行**的路径上判定（`upsertDevice` 的 INSERT 分支之前调 `enforceDeviceLimit`）；**同一 `device_id` 重新登录走 UPDATE 分支 ⇒ 不新增设备行 ⇒ 不受上限约束、不被拒**（顺带清掉该设备 `revoked_at`、刷新 `last_seen`）。
  ⇒ **表增长上限不变**（`devices` 行数仍 ≤ 32 ⇒ 写放大源仍被封住）；**仍不自动删除任何活跃设备**（D3-1 的硬约束不变）。
  ⇒ **本条取代 Consequences 段「设备达到 32 台上限后必须由用户在账号页撤销旧设备才能继续登录」的表述**：改为「**新设备**登录需先撤销旧设备；**已登记设备重新登录不受阻**」。
  ⇒ 判据成对覆盖：新增用例「上限下同 `device_id` 重复登录 ⇒ 允许（走 UPDATE 分支，不被拒、不新增设备行）」与「新 `device_id` ⇒ 429 `device_limit_reached`」。
- **D3-2 不删除用户同步数据**：设备回收只动 `devices` 行与该设备 `refresh_tokens` 的 `revoked_at`；
  **不动** `sync_items`、**不动** R2 对象（口径与 `cloud/fluxing-sync/src/devices.ts` 撤销语义一致）。
- **D3-3 `refresh_tokens` 保留窗口 + GC 触发点**：登录 / 续期时顺带清理 `expires_at < now - 30 天` 的行
  （每次操作最多删 200 行，保护额度）⇒ 表体积收敛。
  **如实标注**：DELETE 同样计入 D1 行写入 ⇒ **GC 不省额度**；它解决的是「表只增不减」与「长期凭据面」，不是计费。
- **D3-4 有效 refresh token 上限 64/账号**：签发新 refresh 前统计该用户「未过期且未撤销」的 refresh 行；
  超 64 ⇒ 先清过期行；仍超 ⇒ **撤销（标 `revoked_at`）最旧的有效 refresh 行**（1 行写入）再签发
  ⇒ 长期凭据面收敛，且不破坏单台设备的正常续期（上限 64 ≫ 已观测的 22 台设备）。
- **D3-5 不变**：`authenticate` 的 `last_seen` 5 分钟抑制窗口、`devices` 的 `UNIQUE(user_id, device_id)`、
  `/v1/account/data` 的清理范围、`/v1/devices` 的撤销语义 ⇒ **全部逐字不变**。

### D4 门禁与判据

- **D4-1 服务端门禁**：`npm run typecheck` ⇒ **TSC_EXIT=0**；`npm run check` ⇒ **CHECK_EXIT=0**，
  既有 **5 文件 / 86 用例**逐条保留（BQ 基线），本批新增组断言（目标 ≥ 20 条）全部针对 D1 / D2-3 / D3 的新分支；
  改动文件 **U+FFFD = 0**。
- **D4-1a（2026-11-23 改码轮澄清）**：「逐条保留」指**用例数量与断言意图逐条保留**，**不指注入点冻结**。
  D0-3 已把「失败注册的事后删除」实现方式替换为「事中不写」⇒ BQ 批 `test/register-mail-failure.test.ts` 中
  **3 个编码旧回滚注入点**的用例（`degraded` / `failed` / 配额 503）必须把注入点迁到**窄窗口**
  （发信成功但落库失败），**断言值逐字不变**（仍分别断言 `'degraded'` / `'failed'` / 503 `storage_quota_exceeded`）；
  该文件用例数仍为 14，总用例数不减、不删。
  - 注入点归属：`degraded` / `failed` 两例命中 **`email_verifications` 的 INSERT**（`users` 已插入 ⇒ 走真实 `rollbackRegister`）；
    配额例命中 **`INSERT INTO users`**（users 未插入 ⇒ 天然零残留、无需回滚），且必须同时注入一个**能发成功的 `doFetch`**
    （否则会撞上上游 401 ⇒ 500 `server_misconfigured`，到不了落库点）。
  - 非配额类存储故障（⇒ 503 `storage_unavailable` + 「验证码已发送，但服务器存储暂时不可用，请稍后重试或重新获取验证码。」）
    由本批**新增**用例覆盖，不占用既有 3 例。
- **D4-1b（2026-11-23 19:38 轮）推送写路径配额错误映射单测**：新增 `cloud/fluxing-sync/test/push-quota-error-mapping.test.ts`（**5 条**断言）—— 手法 = `SELF.fetch` 走**真请求路径**（`SELF.fetch` → `src/index.ts` 顶层 catch → `toErrorResponse`），只打断 `env.DB.batch`（推送落库的**唯一写入口**）⇒ 鉴权与读路径仍走真实本地 D1 ⇒ 测的确实是「推送写阶段失败」这条真路径；覆盖 = **Worker 内绑定形态**（`D1_ERROR:` 开头、无 `[code: 7500]`）/ **REST 形态**（带 `[code: 7500]`）/ **非 `Error` 实例**（普通对象）三种配额错误 ⇒ 均返回 **503 + code `storage_quota_exceeded`** + 中文「服务端存储写入额度已用尽，请稍后重试。」；**无关错误仍 500 `internal_error`**（不误伤）；D1 **零落库**（batch 抛错 ⇒ `sync_items` 一行未增）且拉取为空；故障**非粘性**（恢复后同一条推送正常 200 并落库 1 行）。
- **D4-1b 门禁数字（2026-11-23 19:38 轮最新）**：`npm --prefix cloud/fluxing-sync run typecheck` ⇒ **0**；`npm --prefix cloud/fluxing-sync run check` ⇒ **0** ⇒ **11 文件 / 143 用例**（上一轮基线 **10 文件 / 138 用例** ⇒ **+1 文件 / +5 用例**，既有 138 条逐条保留）；门禁日志落盘 `tmp/br-push-quota-check.log`（`tmp/` 已 gitignore，**不入库**）。
- **D4-2 生产复验（需 deploy 授权）**：
  1. 保留域探针注册 ⇒ **422** + `mail_recipient_rejected` + 中文文案 + `X-Fluxing-Mail-Provider-Status: 422`
     + **新增判据：`users` / `email_verifications` 行数前后对比 = 0 增量**；
  2. 重发失败（对已注册邮箱）⇒ 同样 **零增量**；
  3. 写预算护栏 ⇒ 用小账号构造超限 ⇒ **429 `write_budget_exceeded`** + 中文文案 + **零写入**；
  4. 设备上限 ⇒ 构造 32 台 ⇒ 登录被拒 + 中文文案。
- **D4-3 文档**：`CHANGELOG.md` 条目 + 批次 BR 交接文档 + relay prompt + `CLAUDE.md` §2 指针。

### D5 本批明确不做

- 不升级 D1 付费（用户已定：等待免费且可逆）；只有当 BR 批证明免费额度确实承载不了词典规模时再谈。
- 不改 IPC 协议 / 客户端 C++ / `librime/` / 版本号 / 第三方依赖。
- 不做 `sync_items` 的 `seq` 列迁移（D2-4）。
- 不做服务端明文内容哈希（D2-2）。
- 不自动删除仍在使用的设备（D3-1）。
- 不动口令重置路径（D1-7）。

---

## Alternatives considered

- **Option A：服务端密文域内容去重（比较密文 / 上传明文哈希）**
  rejected：AAD 含 HLC ⇒ 内容未变但 HLC 变则密文必变，服务端无法判定「内容未变」；
  上传明文哈希会把词条内容指纹暴露给服务端，破坏端到端密文语义 ⇒ D2-2 关闭该路线。
- **Option B：把 `DELETE + INSERT` 换成 `INSERT OR REPLACE`**
  rejected：`id` 推进语义与 pull 游标耦合（`server_seq` = 最大 `id`）⇒ 改写后「已拉过的条目更新后不再被拉到」；
  且 REPLACE 的计费口径（是否仍算 2 行）不确定 ⇒ 与 D2-4 一起延后。
- **Option C：引入独立 `seq` 列并回填**
  rejected：回填 40,061 行 = 当日额度直接打爆 ⇒ 必须专门低额度窗口 + 客户端兼容验证 + 回滚预案 ⇒ 延后批次。
- **Option D：直接升级 D1 付费套餐**
  rejected：用户已定「不升级、等待免费且可逆」⇒ 本 ADR 只在 BR 证明免费额度承载不了词典规模时再谈。
- **Option E：客户端侧「远端 HLC 沿用」（D2-5）**
  **推荐且是根治手段**，但触及 `WeaselServer/Cloud/**`，交接明文「未获明确指令不得改客户端」
  ⇒ 列为 **BR-①c**，需用户明确授权后开工。
- **Option F：口令重置路径同构修复（`password_resets` 先落库后发信）**
  rejected：ADR 0075 决策 7 明确不碰；本批只在 D1-7 记录平移配方 ⇒ 列入后续批次候选。

---

## Consequences

**更容易**

- 失败注册/重发**零行写入** ⇒ 不再产生占号残留、不再吃掉 60 秒重发窗口、不再在额度抖动期加剧阻塞，
  且失败探针不再消耗 D1 额度（可直接用保留域反复探测）。
- 写放大被**显式封顶**：单账号单日超预算 ⇒ 429 + 可诊断文案，不再把全库拖垮。
- `devices` / `refresh_tokens` 从「只增不减」变为「有上限 + 有保留窗口 + 有 GC 触发点」。

**更难**

- 注册/重发的落库顺序**反转** ⇒ 「发信成功但落库失败」成为一个必须显式处理的新状态（D1-5），
  其语义是「用户收到了一封用不了的验证码邮件」，文案与错误码都必须说清楚。
- 写预算是**估算**（同一行当日反复写只计 1）⇒ 护栏不是精确计费器，不能用它推断剩余额度。
- `DELETE + INSERT` 的 2× 系数**保留**（D2-4）⇒ 变更密集的用户仍有 2× 行写入。

**已知副作用 / 残留**

- 同邮箱并发注册：败者会多发一封邮件（D1-4）。
- 推送到一半被 429 截断：余下条目下轮续传（客户端既有语义，无需改动）。
- 设备达到 32 台上限后，**新设备**登录需先由用户在账号页撤销旧设备；**已登记 `device_id` 的重新登录不受该上限阻挠**（走 `UPDATE devices` 分支，设备行数不变）（D3-1 / D3-1a）——这是有意的硬约束，不是缺陷。
- **写预算护栏是「每账号」口径，不是「平台额度护盾」（2026-11-23 19:38 轮登记的已知边界）**：D2-3 的上限 = **每账号每 UTC 日 80,000 行**，而 Cloudflare D1 免费套餐的 10 万行/天是**每个 D1 数据库（全局）**口径 ⇒ 多账号各自不超自己的护栏，**累计仍可打爆全局额度** ⇒ 护栏的真实作用是「**单账号放大源刹车**」，**不是**平台额度护盾；实现本身早已声明该近似（`cloud/fluxing-sync/src/sync.ts` 注释：护栏「**不宣称**它能精确计出 Cloudflare 的行写入账单」；`used` 只统计 `sync_items` 当日写入行数 ⇒ 行数**下界**，不含 `devices` / `refresh_tokens` 等行）。**本批不改实现** ⇒ 候选后续项 **BR-②**（改为全局口径 / 按活跃账号数分摊总额度）**待用户拍板**。详见文末「2026-11-23 19:38 轮补充：写预算护栏的口径边界（BR-② 候选）」。
- **推送写序「R2 先、D1 后」在 D1 失败时留下 R2 孤儿对象（无害，2026-11-23 19:38 轮登记）**：D1 阶段因配额耗尽抛错 ⇒ **R2 已写**（留下孤儿对象）、**D1 零落库**（`sync_items` 一行未增）；**无害原因** = 索引行不存在 ⇒ **拉取看不到任何条目**（客户端不会拿到半截数据）；重试时同 key 覆盖 ⇒ **孤儿自然被替换**。
- BR-①c（客户端远端 HLC 沿用）**未授权 ⇒ 本批不闭合最大放大源**；在它落地前，
  「删 sidecar / 换机」仍可能触发一次全量重写（现由 D2-3 护栏兜住）。
- **生产复验暴露的新缺口（已在本批修掉）**：D1 免费额度耗尽会让**所有 D1 写路径**返回通用
  **500 `internal_error`**（含**真实用户登录** —— 登录要写 `refresh_tokens`）
  ⇒ 登录 / 推送路径原先**未把** D1 配额错误（`code 7500`）映射为可解释的 **503**
  （BQ 批次只在注册回滚路径做了 `storage_quota_exceeded` 映射）⇒ 客户端只能看到通用 500。
  - **根因**：Worker 内 D1 绑定抛出的错误文本**不带** `[code: 7500]`（**只有 REST 通道带**）
    ⇒ 旧判据 `/\[code:\s*7500\]/` 命中不了 ⇒ 登录 / 续期 / 推送等 D1 写路径对外报笼统 **500 `internal_error`**。
  - **修法**：`cloud/fluxing-sync/src/errors.ts` 新增 `errorText()` 与 `isD1RowWriteQuotaError()`
    （同时接受 **REST 形态**、**带 `[code: 7500]` 形态**、**`D1_ERROR:` 绑定形态**、**纯句子形态**，
    并容忍抛出值**不是** `Error` 实例）；`toErrorResponse` 改用该判据；
    `cloud/fluxing-sync/src/auth.ts` **删除本地重复副本**、统一走 `errors.ts` 的共享判据
    （**两处各留一份正是本次漏配的根因**）。
  - **验证**：新增 `test/d1-quota-error-mapping.test.ts`（**10 条**断言），
    `test/register-zero-write.test.ts` 追加 **1 条**（Worker 形态 ⇒ 503 `storage_quota_exceeded`）；
    门禁 `typecheck` **0** / `check` **0** = **10 文件 / 138 用例**（原 9 文件 / 127 用例 ⇒ **+1 文件 / +11 用例**）；
    补部署 **DEPLOY_EXIT=0** ⇒ 新生产 Version **`8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`**；
    生产**活体验证 PASS**（`tmp/br-quota-503-probe.mjs` ⇒ `LOGIN_STATUS=503` +
    `LOGIN_CODE=storage_quota_exceeded` + 中文「服务端存储写入额度已用尽，请稍后重试。」+
    `VERDICT_QUOTA_503=PASS`）⇒ 配额耗尽期真实用户登录现在拿到**可解释、可重试**的 503
    （UTC 午夜自动恢复），不再被误判为代码缺陷。详见文末「补部署：配额错误可解释化（2026-11-23）」。

---

## Verification & Rollback

- **判据**：见 D4-1 / D4-2（服务端门禁 + 生产复验）。
- **回滚**：本批改动集中在服务端 `cloud/fluxing-sync/**` ⇒ 回滚 = `wrangler deploy` 上一版本（Worker 版本可一键回退），
  D1 的落库顺序反转**不涉及 schema 变更** ⇒ 无需数据迁移回滚；
  D2-3 / D3 的护栏均为「超限即拒绝」⇒ 关掉阈值即恢复旧行为。
- **未在本 ADR 闭合**：BR-①c（客户端）、BR-①d（sidecar 加固）、`seq` 列迁移、口令重置路径同构修复、
  D1 免费额度承载不了词典规模的付费决策 ⇒ 见批次 BR 交接文档「未闭环清单」。

---

## 生产复验结果（2026-11-23）

**部署**：`npm --prefix cloud/fluxing-sync run deploy` ⇒ **DEPLOY_EXIT=0**，新生产 Version ID =
**`80385732-3473-4e78-b0ba-5a8e29ca1451`**（BQ 版回滚目标 = `96facaba-9277-4c77-b9f9-4df5cb6de41e`，对应 commit `314d03e`）。

**本批无数据库迁移**：`a6ee188` 只改 `src/{auth,devices,env,sync}.ts` + `test/*` + `vitest.config.ts` + `wrangler.toml`；
`migrations/` 仍只有 `0001_init.sql` / `0002_password_resets.sql` 且工作树干净 ⇒ 部署**无需** `d1 migrations apply`。

**门禁**：`npm run typecheck` ⇒ **TSC_EXIT=0**；`npm run check` ⇒ **CHECK_EXIT=0**（**9 文件 / 127 用例**，
BQ 基线 5 文件 / 86 用例逐条保留 +41）。

### ADR D4-2 四条判据的生产复验结果（如实登记）

1. **判据 1（注册发信失败 ⇒ 零写入）= PASS**：HTTP **422** + `code=mail_recipient_rejected` + 中文文案
   「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」+
   `X-Fluxing-Mail-Provider-Status: 422` + `X-Fluxing-Mail-Rollback: ok`；
   `users` / `email_verifications` **零增量**。
2. **判据 2（重发失败 ⇒ 零写入）= PASS**：HTTP **422** + `X-Fluxing-Mail-Provider-Status: 422` +
   `X-Fluxing-Mail-Rollback: ok`；`email_verifications` / `users` / `sync_items` **零增量**。
3. **判据 3（写预算超限 ⇒ 429 + R2/D1 零写入）= 未完成（被平台配额阻塞）**：已按 ADR 完成
   「阈值 10 → 部署 → 复验 → 恢复 80000 → 再部署」全流程，但复验步骤的**登录前置**即失败
   （`C3_LOGIN_STATUS=500`）⇒ 拿不到 access_token ⇒ **推送判据本身没有跑到**。
   阈值已安全恢复：`WRITE_BUDGET_ROWS_PER_DAY = "80000"`，
   `git status --porcelain -- cloud/fluxing-sync/wrangler.toml` **为空**（与 HEAD **逐字节一致**），
   恢复后的部署 **DEPLOY_EXIT=0**。**本条不得记为 PASS。**
4. **判据 4（设备上限 32 台 ⇒ 429）= 行为达成**：第 32 台之后登录被拒，`code=device_limit_reached` +
   中文文案「此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。」；
   `devices` 行数**恰好 32** = `MAX_DEVICES_PER_ACCOUNT`。

### 复验过程中定位到的 500 根因（不是本批代码缺陷）

- **现象**：**已登记 `device_id` 重新登录 ⇒ HTTP 500 `internal_error`**。
- **定位**：对生产 D1 逐条打 SQL ⇒ Cloudflare 返回 **`code: 7500`**
  「Your account has exceeded D1's free tier daily row write limit」
  （`wrangler d1 info` 显示 `rows_written_24h: 122253`，免费额度 10 万行/天）
  ⇒ **生产 D1 免费额度耗尽**。
- **行为边界**：写 **0 行**的语句（`SELECT`、匹配 0 行的 `DELETE`）照常通过；
  `UPDATE users` 与 `INSERT refresh_tokens` **均被拒**。
- **影响面**：**不限于探针** —— 真实用户登录也要写 `refresh_tokens` ⇒ **同样 500**
  ⇒ 属生产事故级问题；UTC 午夜（北京时间次日 08:00）重置。
- **定性**：**不是** BR 代码缺陷，**也不是** IPC / 协议问题。
- **待改进点（已在本批修掉）**：登录 / 推送等 D1 写路径原先**没有**把 D1 配额错误（`code 7500`）映射成可解释的
  503（BQ 批次只在注册回滚路径做了 `storage_quota_exceeded` 映射）⇒ 客户端只能看到通用 500。
  - **根因**：Worker 内 D1 绑定抛出的错误文本**不带** `[code: 7500]`（**只有 REST 通道带**）
    ⇒ 旧判据 `/\[code:\s*7500\]/` 命中不了 ⇒ 统一报笼统 **500 `internal_error`**。
  - **修法**：`cloud/fluxing-sync/src/errors.ts` 新增 `errorText()` + `isD1RowWriteQuotaError()`
    （**REST 形态 / 带 `[code: 7500]` 形态 / `D1_ERROR:` 绑定形态 / 纯句子形态**四路兼容，
    且容忍抛出值**不是** `Error` 实例）；`toErrorResponse` 改用该判据；
    `cloud/fluxing-sync/src/auth.ts` 删除本地重复副本、统一走共享判据
    （**两处各留一份 = 本次漏配根因**）。
  - **验证结论**：补部署后生产**活体验证 PASS** —— 同一路径由 500 `internal_error` 变为
    **503 `storage_quota_exceeded`** + 中文「服务端存储写入额度已用尽，请稍后重试。」
    ⇒ 配额耗尽期真实用户登录拿到**可解释、可重试**的 503（UTC 午夜自动恢复），不再是「不明 500」。
    详见文末「补部署：配额错误可解释化（2026-11-23）」。

### 探针残留（未清理）

- 复验用探针账号 `br.probe.1795431653344@example.com`
  （`users` 1 行 + `devices` 32 行 + `refresh_tokens` 64 行）与
  `br.c2.1795431653344@example.com`（1 行）仍留在生产 D1；
  **清理同样被 D1 配额阻塞**（删除行也是行写入）⇒ 需等配额重置后按前缀删除。
- 证据文件（均在 `tmp/`，**禁止入库**）：`tmp/br-probe-{setup,c1,c2,c4}.log`、`tmp/br-probe-c3-run.log`、
  `tmp/br-deploy-1.log`、`tmp/br-d1-probe.log`、`tmp/br-d1-baseline-20261123.json`、
  `tmp/br-prod-verify.mjs`、`tmp/br-threshold.ps1`。

### 本批客户端零改动

BR-①c（远端 HLC 沿用，根治写放大的客户端项）用户明确授权**本批不开工**；未改 IPC 协议与
`include/WeaselIPCData.h`、未改 `librime/`、未 bump 版本号（装机版仍 1.3.8.0）。

### 补部署：配额错误可解释化（2026-11-23）

- **根因**：Worker 内 D1 绑定抛出的错误文本**不带** `[code: 7500]`（**只有 REST 通道带**）
  ⇒ 旧判据 `/\[code:\s*7500\]/` 命中不了 ⇒ 登录 / 续期 / 推送等 D1 写路径对外一律报笼统
  **500 `internal_error`**（用户看到的是「不明 500」，无法判断是「稍后重试」还是「代码缺陷」）。
- **修法**：`cloud/fluxing-sync/src/errors.ts` 新增 `errorText()` 与 `isD1RowWriteQuotaError()`
  （同时接受 **REST 形态**、**带 `[code: 7500]` 形态**、**`D1_ERROR:` 绑定形态**、**纯句子形态**，
  并容忍抛出值**不是** `Error` 实例）；`toErrorResponse` 改用该判据；
  `cloud/fluxing-sync/src/auth.ts` **删除本地重复副本**、统一走 `errors.ts` 的共享判据
  （**两处各留一份正是本次漏配的根因**）。
- **单测**：新增 `cloud/fluxing-sync/test/d1-quota-error-mapping.test.ts`（**10 条**断言）；
  `test/register-zero-write.test.ts` 追加 **1 条**（Worker 形态 ⇒ 503 `storage_quota_exceeded`）。
- **门禁**：`npm --prefix cloud/fluxing-sync run typecheck` ⇒ **0**；
  `npm --prefix cloud/fluxing-sync run check` ⇒ **0** ⇒ **10 文件 / 138 用例**
  （原 9 文件 / 127 用例 ⇒ **+1 文件 / +11 用例**）。
- **部署**：`npm --prefix cloud/fluxing-sync run deploy` ⇒ **DEPLOY_EXIT=0** ⇒
  新生产 Version ID = **`8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`**
  （BR 主体版本 = `80385732-3473-4e78-b0ba-5a8e29ca1451`；BR 之前的回滚目标仍是 BQ 版
  `96facaba-9277-4c77-b9f9-4df5cb6de41e`，对应 commit `314d03e`）。
- **生产活体验证 = PASS**：`tmp/br-quota-503-probe.mjs` ⇒ `LOGIN_STATUS=503` +
  `LOGIN_CODE=storage_quota_exceeded` + 中文「服务端存储写入额度已用尽，请稍后重试。」+
  `VERDICT_QUOTA_503=PASS`（证据 `tmp/br-quota-503-probe.log`）；
  **修复前同一路径是 500 `internal_error`**。
- **意义**：配额耗尽期真实用户登录现在拿到**可解释、可重试**的 503（UTC 午夜自动恢复），
  不再被误判为代码缺陷。**D1 免费额度耗尽本身仍是生产事故级问题**（影响面含真实用户登录写路径；
  是否升级 Workers Paid **需用户拍板，未执行**）。
- **仍未闭环**：判据 3（写预算护栏 429）**仍待 D1 配额重置后补跑**；生产探针残留待清理；
  BR-①c（客户端远端 HLC 沿用）**未授权、未开工**。

### 2026-11-23 19:38 轮补充：写预算护栏的口径边界（BR-② 候选）

> 本节是**增量登记**：不改 ADR 的任何决策条文，**`Status` 仍为 `Accepted`**，D2-3 的实现与口径**逐字未动**。
> 前文的 **9 文件 / 127 用例**（部署与判据复验轮）与 **10 文件 / 138 用例**（配额错误可解释化轮）是**各该轮次的历史门禁数字**；
> **本轮最新门禁 = 11 文件 / 143 用例**（见 D4-1b）。

#### 事实 B：护栏是「每账号」口径，D1 免费额度是「每数据库（全局）」口径

- **D2-3 护栏口径** = **每账号每 UTC 日 80,000 行**（`WRITE_BUDGET_ROWS_PER_DAY`，缺失或非法回落默认 80,000）；
  `used` 取该用户 `sync_items` 当日已写行数（`updated_at >= UTC 日首`）。
- **Cloudflare D1 免费套餐口径** = **10 万行 / 天 / 每个 D1 数据库** ⇒ **全局**，**不是每账号**。
- ⇒ **多账号各自不超自己的 80k 护栏，累计仍可打爆全局额度** ⇒ 护栏**不是**「平台额度护盾」，
  它的作用是「**单账号放大源刹车**」（把「一个人打爆全库额度 ⇒ 所有账号一起不可用」降级为
  「单账号可诊断的 429」，见 D2-3「收益」条）。
- 【实测】2026-11-23 生产即属此类：全局 `rows_written_24h = 122254` 触发 `code 7500`
  （**该计数是全局口径，不是单账号口径**）。
- 【读码】实现本身早已声明该近似：`cloud/fluxing-sync/src/sync.ts` 护栏注释写明护栏
  「**不宣称**它能精确计出 Cloudflare 的行写入账单」；`used` 只统计 `sync_items` 当日写入行数
  ⇒ 是行数**下界**，**不含** `devices` / `refresh_tokens` 等行。
- **处置**：**本批不改实现**（D2-3 原文即每账号口径；改口径属**设计变更**，需**独立 ADR + 用户拍板**）
  ⇒ 登记为**已知边界**（已同步写入 Consequences 段）+ 候选后续项 **BR-②**：是否把护栏改为全局口径、
  或按活跃账号数分摊总额度（**两者都需用户拍板，本批不做**）。

#### 事实 C：推送写序「R2 先、D1 后」在 D1 失败时留下 R2 孤儿对象（无害）

- 推送写序 = **R2 先、D1 后**（ADR 0074）；若 D1 阶段因配额耗尽抛错 ⇒ **R2 已写**（留下孤儿对象）、
  **D1 零落库**（`sync_items` 一行未增）。
- **无害原因**：索引行不存在 ⇒ **拉取看不到任何条目**（客户端不会拿到半截数据）；
  重试时同 key 覆盖 ⇒ **孤儿自然被替换**。

#### 本轮门禁（事实 A：推送写路径的配额错误映射已被单测锁住）

- 新增 `cloud/fluxing-sync/test/push-quota-error-mapping.test.ts`（**5 条**断言）：
  `SELF.fetch` 走**真请求路径**（`SELF.fetch` → `src/index.ts` 顶层 catch → `toErrorResponse`），
  只打断 `env.DB.batch`（推送落库的**唯一写入口**）⇒ 鉴权与读路径仍走真实本地 D1
  ⇒ 测的确实是「推送写阶段失败」这条真路径。
- 覆盖：Worker 内绑定形态（`D1_ERROR:` 开头、无 `[code: 7500]`）/ REST 形态（带 `[code: 7500]`）/
  非 `Error` 实例（普通对象）三种配额错误 ⇒ 均 **503 + `storage_quota_exceeded`** + 中文
  「服务端存储写入额度已用尽，请稍后重试。」；无关错误仍 **500 `internal_error`**（不误伤）；
  D1 **零落库**（`sync_items` 一行未增）且拉取为空；故障**非粘性**（恢复后同一条推送正常 200 并落库 1 行）。
- **门禁**：`npm --prefix cloud/fluxing-sync run typecheck` ⇒ **0**；`run check` ⇒ **0** ⇒
  **11 文件 / 143 用例**（上一轮基线 **10 文件 / 138 用例** ⇒ **+1 文件 / +5 用例**，既有 138 条逐条保留）；
  日志落盘 `tmp/br-push-quota-check.log`（`tmp/` 已 gitignore，**不入库**）。

#### 本轮生产状态（判据 3 仍未完成）

- 当前生产 Version = **`8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`**（含 D1 配额错误可解释化修复）；
  BR 主体版本 = `80385732-3473-4e78-b0ba-5a8e29ca1451`；BR 之前的回滚目标仍是 BQ 版
  `96facaba-9277-4c77-b9f9-4df5cb6de41e`。
- 生产仍处于 D1 免费额度耗尽（`code 7500`）：19:36 复查 `rows_written_24h = 122254`（未重置）；
  真实用户登录当前返回**可解释的 503**（修复前是 500）。
- **判据 3（写预算超限 ⇒ 429 + R2/D1 零写入）仍未完成** ⇒ **不得记为 PASS**。
- **判据 3 的自动补跑已排定**：定时任务 ID `self-2fd9cb456b4b1eb65b256941d0500f74`，
  触发时间 = 北京时间 **2026-11-24 08:10**（UTC 午夜 08:00 配额重置之后）；动作 = 阈值降到 10 → deploy →
  跑 `node tmp/br-prod-verify.mjs c3` → 阈值恢复 80000 → 再 deploy → 校验
  `cloud/fluxing-sync/wrangler.toml` 与 HEAD 逐字节一致 → 跑 `node tmp/br-prod-verify.mjs cleanup` 清理探针；
  **判据 3 只有在日志出现 429 `write_budget_exceeded` 且 R2/D1 零写入时才能记 PASS**。
- **探针残留仍未清理**：`br.probe.1795431653344@example.com`（`users` 1 + `devices` 32 + `refresh_tokens` 64）
  与 `br.c2.1795431653344@example.com`（1 行）—— **删除行也是行写入** ⇒ 被 `code 7500` 阻塞，
  必须等重置后按前缀删除并回读零残留。
- **是否升级 Workers Paid 仍未获用户拍板、未执行**（付费动作必须先获明确同意）；
  **客户端 BR-①c（远端 HLC 沿用）仍未获授权、未开工**。
