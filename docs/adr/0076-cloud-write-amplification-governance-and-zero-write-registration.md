# ADR 0076: 推送写放大治理与失败注册零写入化

> 批次 BR · 承接 [ADR 0075](0075-register-mail-rejection-semantics-and-failed-registration-rollback.md) · 关联 spec `100-fluxing-cloud-account-sync`

## Status

**Proposed**（待实现 + 门禁 + 生产复验通过后转 Accepted）

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
- 设备达到 32 台上限后**必须**由用户在账号页撤销旧设备才能继续登录（D3-1）——这是有意的硬约束，不是缺陷。
- BR-①c（客户端远端 HLC 沿用）**未授权 ⇒ 本批不闭合最大放大源**；在它落地前，
  「删 sidecar / 换机」仍可能触发一次全量重写（现由 D2-3 护栏兜住）。

---

## Verification & Rollback

- **判据**：见 D4-1 / D4-2（服务端门禁 + 生产复验）。
- **回滚**：本批改动集中在服务端 `cloud/fluxing-sync/**` ⇒ 回滚 = `wrangler deploy` 上一版本（Worker 版本可一键回退），
  D1 的落库顺序反转**不涉及 schema 变更** ⇒ 无需数据迁移回滚；
  D2-3 / D3 的护栏均为「超限即拒绝」⇒ 关掉阈值即恢复旧行为。
- **未在本 ADR 闭合**：BR-①c（客户端）、BR-①d（sidecar 加固）、`seq` 列迁移、口令重置路径同构修复、
  D1 免费额度承载不了词典规模的付费决策 ⇒ 见批次 BR 交接文档「未闭环清单」。
