# 批次 BR · 推送写放大治理 + 失败注册零写入化（ADR 0076）· 交接文档

- **生成时间**：2026-11-23
- **分支**：`feat/100-cloud-sync`（远程 `kizemo/feat/100-cloud-sync`）
- **上游**：`origin` 只读
- **本批真源**：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md` §9-3
- **本批决策源**：`F:\soft\00selfmade\rime_claude\docs\adr\0076-cloud-write-amplification-governance-and-zero-write-registration.md`（Status = **Accepted**，2026-11-23 转正）

## §0 结论速览

- **本批性质** = **ADR 0076 已落盘并转 Accepted + 取证完成 + 服务端 T1-T4 改码与单测已入库 + 已部署生产（Version `80385732-3473-4e78-b0ba-5a8e29ca1451`）+ 判据 1 / 2 PASS、判据 4 行为达成 + D1 配额错误映射已修（补部署 Version `8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`，生产活体验证 PASS）**（**已 push / 未改客户端 / 未 bump 版本号**；**判据 3 未完成**，被生产 D1 免费额度耗尽阻塞）。
- **改码轮范围**：只改 `cloud/fluxing-sync/**`（服务端），**客户端零改动** ⇒ 不触发 IPC 四边同步红线；ADR 0075 已发布的错误语义与三个 ASCII 诊断头**逐字保留**。
- **范围（交接 §9-3，两子项同批做）**：① 推送写放大治理，拆两条线（(a) `sync_items` 全量推送；(b) `devices` / `refresh_tokens` 无上限增长）；② 失败注册「零写入」化（`users` / `email_verifications` 落库挪到发信成功之后，重发路径同样覆盖）。
- **核心事实（推翻了本轮开工时的初始假设）**：客户端**已经是增量推送**（`WeaselServer/Cloud/CloudSyncEngine.cpp:207-210` 注释：推送集合由 sidecar 差分算出）⇒ 真正的放大源不是「每次都推全量」，而是 **sidecar 丢失 / 换机 ⇒ 全部条目重分配新 HLC（ms = 墙钟，必然大于服务端旧 HLC）⇒ 服务端 HLC 守卫失效 ⇒ 40,061 行全部重写（≈ 80,122 行写入）**。
- **服务端无法在密文域去重**：AAD 含 `hlc` + nonce 随机 ⇒ 内容未变也判定不了；上传明文内容哈希会泄漏词条指纹、破坏端到端密文语义 ⇒ **该路线在 ADR 0076 D2-2 明确关闭**，不再反复讨论。
- **最大放大源的根治手段在客户端**（ADR D2-5：拉取到的远端副本与本地明文逐字一致时沿用远端 HLC ⇒ 守卫命中 ⇒ 零写入）⇒ 触及 `WeaselServer/Cloud/**`，而交接 §0 明文「未获明确指令不得改客户端」⇒ **列为 BR-①c，需用户明确授权**。

## §1 已完成（本轮）

1. **进站阅读**（按 `CLAUDE.md` 顺序）：`CONTEXT.md`、`AGENTS.md`、`.specify/memory/constitution.md`、`.specify/memory/project-knowledge.md`（§11 + 附录 A）、`docs/agents/domain.md`、BQ 交接全文、ADR 0075、`.claude/rules/codebase-graph.md`。
2. **图谱降级**：`codebase-memory-mcp` 的 `paths` 只索引 C++ 侧（`Weasel**/*.cpp|.h` 等），本次 `search_graph` 命中全为 librime/WTL 噪声；`cloud/fluxing-sync/src/**`（TypeScript）本就超出 scope ⇒ 改用 Read/Grep 取证。
3. **服务端取证（逐行读）**：`src/sync.ts`、`src/auth.ts`（870 行）、`src/wire.ts`、`src/devices.ts`、`src/account.ts`、`src/mail.ts`（关键段）、`migrations/0001_init.sql`。
4. **客户端取证（只读，禁改）**：`CloudCrypto.cpp`（`GenerateDeviceId`）、`CloudSyncService.cpp` HLC 分配段、`CloudAccountDialog.cpp` sidecar 落点、`CloudSyncEngine.cpp` 差分推送注释、`CloudCredentialStore.cpp`。
5. **ADR 0076 落盘**：`docs/adr/0076-cloud-write-amplification-governance-and-zero-write-registration.md`（20,187 B）。
6. **ADR 索引登记**：`docs/adr/README.md` 的 ADR index 表补上 **0076** 行（Status = Proposed）⇒ 该行**留在工作树未入库**（本批红线：未获明确指令不得 commit）⇒ 随下一次获授权的 commit 一并入库。

7. **T1 零写入化改码**（`cloud/fluxing-sync/src/auth.ts`，diff 322 行）：拆出纯函数 `generateVerificationCode`（只产 `{code, codeHash, createdAt, expiresAt}`，不发信不落库）+ `persistVerificationCode`；`handleRegister` 新顺序 = 闸门 → 查重（只读）→ 算 verifier / salt / kdf → 纯生成 → **发信** → `INSERT users` → `persistVerificationCode`；**发信失败 ⇒ 零写入**（回滚头保持 `'ok'`，因为没有残留可回滚）；`INSERT users` UNIQUE 冲突 ⇒ 409 `email_taken` + 零残留；验证码行落库失败 ⇒ 真 `rollbackRegister` 三态；`handleResend` 同构（失败零写入、旧验证码行仍在且仍有效）。新增 `D1_ROW_WRITE_QUOTA_PATTERN` / `isD1QuotaError` / `isUniqueConstraintError` / `storageFailureError`；**不动** ADR 0075 的上游错误分类、三个诊断头常量、`handleVerify` / `handlePasswordReset*` / `authenticate`。
8. **T2 写预算护栏改码**（`src/sync.ts` +170、`src/env.ts` +41、`wrangler.toml` +5、`vitest.config.ts` +4）：新增 `evaluateWriteBudget` / `planWriteRows` / `utcDayStartSeconds` / `countTodayWrittenRows` / `evaluatePushWriteBudget`；护栏插在 **HLC 守卫之后、R2 staging / put 之前** ⇒ 拒绝时 **R2 与 D1 双零写入**；判定式 `used + planned > limit`（等于放行）；`used` = `COUNT(*)` `updated_at >= UTC 日首`（`sync_items` 的时间列是 `updated_at`）；阈值走 `WRITE_BUDGET_ROWS_PER_DAY`，默认 **80,000** 行 / 账号 / UTC 日，缺失或非法**回落默认（不 fail closed）**。
9. **T3 设备与令牌上限改码**（`src/devices.ts` +77，`src/auth.ts` 内）：`MAX_DEVICES_PER_ACCOUNT = 32`、`countDevices`、`reclaimRevokedDevices`（**只删 `revoked_at IS NOT NULL`**，按 `last_seen ASC, created_at ASC`，删设备行前先撤其 `refresh_tokens`，避免「撤销设备但令牌仍可用」）、`enforceDeviceLimit`（≥32 先回收已撤销设备，仍 ≥32 ⇒ 429 `device_limit_reached` + 中文文案「此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。」）；`MAX_VALID_REFRESH_TOKENS = 64`（`enforceRefreshTokenLimit` 按需撤销最旧有效行，签发后仍 ≤64）、`REFRESH_TOKEN_RETENTION_SECONDS`（30 天）、`REFRESH_TOKEN_GC_MAX_ROWS`（单次 200 行，登录 / 续期触发）。**绝不自动删活跃设备**；`handleDeviceList` / `handleDeviceRevoke` 逐字未动。
10. **T4 单测**：新增 `test/write-budget.test.ts`、`test/register-zero-write.test.ts`、`test/device-token-limits.test.ts`、`test/zero-write-register-and-limits.test.ts`；既有 `test/register-mail-failure.test.ts` 按 **ADR D4-1a** 把 3 个编码旧回滚注入点的用例（`degraded` / `failed` / 配额 503）**注入点迁到窄窗口**（发信成功但落库失败）⇒ **该文件用例数仍 14、断言意图逐条保留**（`'degraded'` / `'failed'` / 503 `storage_quota_exceeded` 的断言值不变），非配额类存储故障由**新增**用例覆盖。
11. **父会话独立复核**（不采信子会话自报）：逐行读 `handleRegister` / `handleResend` / `enforceDeviceLimit` / `enforceRefreshTokenLimit` / 护栏插入点的 diff，确认「先发信后落库」「拒绝时零写入」「只删已撤销设备且先撤令牌」三条关键性质；另亲跑全量门禁（结果见 §3）。
12. **文档登记**：ADR 0076 补 **D4-1a** 段（澄清「逐条保留」的含义与 3 个用例的注入点归属）；`docs/adr/README.md` index 补 0076 行；本交接文档补改码轮记录。

## §2 下一步（服务端改码已完成；剩余动作需用户授权）

- **T1（服务端 · 子项②）零写入化**：✅ **已落码**（见 §1 第 7 项）。
- **T2（服务端 · 子项①(a)）写预算护栏**：✅ **已落码**（见 §1 第 8 项）。**不做** `DELETE+INSERT` 改写（D2-4）、**不做** `seq` 列迁移。
- **T3（服务端 · 子项①(b)）设备/令牌上限**：✅ **已落码**（见 §1 第 9 项）。**如实标注**：GC 不省额度（行删除同样计写入）。
- **T4（单测）**：✅ **新增 4 个测试文件** + **既有 `test/register-mail-failure.test.ts` 按 D4-1a 迁注入点**（见 §1 第 10 项）；门禁数字见 §3。
- **T5（父会话全量门禁复跑）**：`npm run typecheck` ⇒ TSC_EXIT=0；`npm run check` ⇒ CHECK_EXIT=0（BQ 基线 5 文件 / 86 用例逐条保留 + 本批新增组）；改动文件 U+FFFD = 0。**19:38 轮最新门禁 = `typecheck` 0 / `check` 0 = 11 文件 / 143 用例**（基线 10 文件 / 138 用例 ⇒ **+1 文件 / +5 用例**，既有 138 条逐条保留）；本轮新增 `cloud/fluxing-sync/test/push-quota-error-mapping.test.ts`（**5 条**断言）。
- **T6（部署 + 生产复验，需授权）**：✅ **已完成**（部署与复验均已执行；**但判据 3 未完成**）。
  - **部署**：`npm --prefix cloud/fluxing-sync run deploy` ⇒ **DEPLOY_EXIT=0**，新生产 Version ID = **`80385732-3473-4e78-b0ba-5a8e29ca1451`**（BQ 版回滚目标 = `96facaba-9277-4c77-b9f9-4df5cb6de41e`，对应 `314d03e`）；**本批无数据库迁移**（`migrations/` 未动 ⇒ 无需 `d1 migrations apply`）。
  - **判据 1 = PASS**（注册发信失败 ⇒ 零写入）：422 + `mail_recipient_rejected` + 中文文案「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」+ `X-Fluxing-Mail-Provider-Status: 422` + `X-Fluxing-Mail-Rollback: ok` + `users` / `email_verifications` **零增量**。
  - **判据 2 = PASS**（重发失败 ⇒ 零写入）：422 + provider-status 422 + rollback ok + `email_verifications` / `users` / `sync_items` **零增量**。
  - **判据 4 = 行为达成**（设备上限 32 台 ⇒ 429）：第 32 台后登录被拒 + `device_limit_reached` + 中文文案「此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。」+ `devices` 行数**恰好 32**。
  - **判据 3 = 未完成**（写预算超限 ⇒ 429 + R2/D1 零写入）：已按 ADR 完成「阈值 10 → 部署 → 复验 → 恢复 80000 → 再部署」全流程，但复验的**登录前置**即失败（`C3_LOGIN_STATUS=500`）⇒ 拿不到 access_token ⇒ **推送判据未跑到** ⇒ **不得记为 PASS**；阈值已恢复 `WRITE_BUDGET_ROWS_PER_DAY = "80000"`，`wrangler.toml` 与 HEAD **逐字节一致**，恢复后部署 DEPLOY_EXIT=0。
  - **500 根因** = **生产 D1 免费额度耗尽**（Cloudflare `code 7500`）⇒ **不是本批代码缺陷**；**影响面含真实用户登录**（详见 §3 与 ADR 0076「生产复验结果（2026-11-23）」）。
  - **D1 配额错误映射已在本批修掉**（补部署）：**根因** = Worker 内 D1 绑定抛出的错误文本**不带** `[code: 7500]`（**只有 REST 通道带**）⇒ 旧判据 `/\[code:\s*7500\]/` 命中不了 ⇒ 登录 / 续期 / 推送等 D1 写路径对外报笼统 **500 `internal_error`**；**修法** = `cloud/fluxing-sync/src/errors.ts` 新增 `errorText()` 与 `isD1RowWriteQuotaError()`（**REST 形态 / 带 `[code: 7500]` 形态 / `D1_ERROR:` 绑定形态 / 纯句子形态**四路兼容，并容忍抛出值**不是** `Error` 实例），`toErrorResponse` 改用该判据，`cloud/fluxing-sync/src/auth.ts` **删除本地重复副本**、统一走 `errors.ts` 的共享判据（**两处各留一份正是本次漏配的根因**）；**单测** = 新增 `test/d1-quota-error-mapping.test.ts`（**10 条**断言）+ `test/register-zero-write.test.ts` 追加 **1 条**（Worker 形态 ⇒ 503 `storage_quota_exceeded`）；**门禁** = `typecheck` **0** / `check` **0** = **10 文件 / 138 用例**（原 9 文件 / 127 用例 ⇒ **+1 文件 / +11 用例**）；**部署** = DEPLOY_EXIT=0 ⇒ 新生产 Version **`8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`**；**生产活体验证 PASS** = `tmp/br-quota-503-probe.mjs` ⇒ `LOGIN_STATUS=503` + `LOGIN_CODE=storage_quota_exceeded` + 中文「服务端存储写入额度已用尽，请稍后重试。」+ `VERDICT_QUOTA_503=PASS`（证据 `tmp/br-quota-503-probe.log`；修复前同一路径是 **500 `internal_error`**）⇒ 配额耗尽期真实用户登录现在拿到**可解释、可重试**的 503（UTC 午夜自动恢复），不再被误判为代码缺陷。
- **T7（入库 + push）**：✅ **入库已完成 / push 已完成**。
  - **入库 = 5 个 commit**：`a6ee188`（服务端 T1-T4 改码，11 文件）、`ac23bfa`（ADR 0076）、`d628e53`（ADR 0075 + BQ handoff）、`314d03e`（BQ 修复）、`89f90fd`（回填 BR 实施进度 + `CHANGELOG.md`）。
  - **push 已完成**：`git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync` ⇒ **PUSH_EXIT=0** ⇒ `3e32cb8..89f90fd feat/100-cloud-sync -> feat/100-cloud-sync` ⇒ **前 5 个 commit 已全部进入远程**（远程已含 `3e32cb8..89f90fd` 区间全部 5 笔）。
  - **本轮新增两笔提交**（服务端 D1 配额映射修复 + 本次文档回写）由收尾时**快进续推**（命令同上）；**显式按路径 `git add`，禁用 `git add .`**。
- **T8（需用户授权才可开工）**：BR-①c 客户端「远端 HLC 沿用」（判据：删 sidecar 后同步一次，`sync_items` 今日写入增量 = 0）；BR-①d sidecar 落点加固（低优先）。
- **T9（判据 3 自动补跑 · 已排定）**：定时任务 ID `self-2fd9cb456b4b1eb65b256941d0500f74`，触发时间 = 北京时间 **2026-11-24 08:10**（UTC 午夜 08:00 配额重置之后）；动作清单 = 阈值降到 10 → deploy → 跑 `node tmp/br-prod-verify.mjs c3` → 阈值恢复 80000 → 再 deploy → 校验 `cloud/fluxing-sync/wrangler.toml` 与 HEAD 逐字节一致 → 跑 `node tmp/br-prod-verify.mjs cleanup` 清理探针；**判据 3 只有在日志出现 429 `write_budget_exceeded` 且 R2/D1 零写入时才能记 PASS**（当前**不得**记为 PASS）。
- **T10（探针 cleanup · 待配额重置后执行）**：`br.probe.1795431653344@example.com`（`users` 1 + `devices` 32 + `refresh_tokens` 64）与 `br.c2.1795431653344@example.com`（1 行）仍在生产 D1 ⇒ **删除行也是行写入** ⇒ 被 `code 7500` 阻塞 ⇒ 必须等重置后按前缀删除并**回读零残留**。
- **BR-②（候选 · 待用户拍板 · 本批不做）**：写预算护栏是**每账号**口径（80,000 行 / 账号 / UTC 日），而 D1 免费额度是**每数据库（全局）**口径 ⇒ 护栏**不是**平台额度护盾，只是「单账号放大源刹车」⇒ 候选方向 = 改为全局口径 / 按活跃账号数分摊总额度（**两者都需用户拍板**）。
- **Workers Paid（待用户拍板 · 未执行）**：D1 免费额度耗尽本身仍是生产事故级问题（影响面含真实用户登录写路径）⇒ 是否升级 Workers Paid **必须先获明确同意**，本轮**未执行**。
- **BR-①c（待授权 · 未开工）**：客户端「远端 HLC 沿用」仍是根治最大放大源的手段（见 T8）⇒ **未获授权不动手**。

## §3 已验证 / 推断 / 未验证

**已验证（实测或逐行读码可复算）**

- 【实测】2026-11-22 UTC 日 D1 行写入：`sync_items` 40,060（累计 40,061）、`refresh_tokens` 32（累计 98）、`users` 11、`email_verifications` 11、`devices` 1；40,061 行全属真实账号 B、`type=userdb`、`COUNT(DISTINCT item_key)` = 总行数 ⇒ 行级无写放大。
- 【实测】D1 免费额度按行写入计，耗尽时返回 `[code: 7500] exceeded D1's free tier daily row write limit`；打爆后 `UPDATE`/`DELETE` 一律被拒、仅 `INSERT` 可用（BQ 批）。
- 【实测】真实账号 B = 22 devices / 90 refresh_tokens（4 个真实账号合计 25 devices / 98 tokens）。
- 【读码】`handleRegister` 顺序 = 查重 → INSERT `users`（`auth.ts:451-455`）→ INSERT `email_verifications`（`auth.ts:259-263`）→ 发信（`mail.ts:408-419` 只依赖 `code` 明文，不读库）⇒ 验证码可先内存生成、先发信、后落库。
- 【读码】`handleResend`（`auth.ts:535-575`）同构；失败只删本次验证码行、保留 `users`。
- 【读码】`handlePush` 写序 = 查既有行 → HLC 守卫（`compareHlc(stored, item.hlc) >= 0` ⇒ 整条跳过）→ R2 先写 → D1 `DELETE + INSERT`（每条变更 2 次行写入）→ `server_seq` = 最大 `id`。
- 【读码】客户端 `CloudSyncService.cpp:1003-1019`：sidecar 命中且 payload 哈希未变 ⇒ 沿用旧 HLC；否则 `HlcTick(now)` 分配新 HLC（墙钟 ⇒ 必大于服务端旧值）。
- 【读码】`GenerateDeviceId`（`CloudCrypto.cpp:403-425`）= `BCryptGenRandom` 8 字节随机；仅在凭据为空时生成，随凭据 blob 持久化 ⇒ device_id 绑定「凭据生命周期」而非「机器」。
- 【读码】`upsertDevice` 仅同 `device_id` 走 UPDATE；`issueTokenPair` 每次 2 行；`handleRefresh` 轮换净增 2 行（旧行只标 `revoked_at`）；`devices` / `refresh_tokens` 无上限 / 无 GC。
- 【实测】**推送写路径的 D1 配额错误映射已被单测锁住**：新增 `cloud/fluxing-sync/test/push-quota-error-mapping.test.ts`（**5 条**断言）—— 手法 = `SELF.fetch` 走**真请求路径**（`SELF.fetch` → `src/index.ts` 顶层 catch → `toErrorResponse`），只打断 `env.DB.batch`（推送落库的**唯一写入口**）⇒ 鉴权与读路径仍走真实本地 D1 ⇒ 测的确实是「推送写阶段失败」这条真路径；覆盖 = **Worker 内绑定形态**（`D1_ERROR:` 开头、无 `[code: 7500]`）/ **REST 形态**（带 `[code: 7500]`）/ **非 `Error` 实例**（普通对象）三种配额错误 ⇒ 均 **503 + `storage_quota_exceeded`** + 中文「服务端存储写入额度已用尽，请稍后重试。」；**无关错误仍 500 `internal_error`**（不误伤）；D1 **零落库**（batch 抛错 ⇒ `sync_items` 一行未增）且拉取为空；故障**非粘性**（恢复后同一条推送正常 200 并落库 1 行）；**门禁** = `typecheck` **0** / `check` **0** = **11 文件 / 143 用例**（基线 10 文件 / 138 用例 ⇒ **+1 文件 / +5 用例**，既有 138 条逐条保留）；日志 `tmp/br-push-quota-check.log`（`tmp/` 已 gitignore，**不入库**）。

**推断（由实测 + 读码推导，未单独复验）**

- sidecar 丢失 / 换机 / 重建用户数据目录 ⇒ 40,061 行全部重写（≈ 80,122 行写入）—— 推导链完整，但**未在真机制造 sidecar 丢失**验证过。
- 写预算按「今日已写行数」估算 ⇒ 同一行当日反复写只计 1（护栏不是精确计费器）。
- 【推断】写预算护栏是**每账号**口径（80,000 行 / 账号 / UTC 日），而 D1 免费额度是**每数据库（全局）**口径 ⇒ **多账号各自不超护栏、累计仍可打爆全局额度** ⇒ 护栏**不是**平台额度护盾，只是「单账号放大源刹车」（推导链 = D2-3 口径 + Cloudflare 套餐口径；**未在生产构造多账号并发超限验证**）。

**未验证 / 未闭环**

- BR-①c 客户端「远端 HLC 沿用」**未实现、未授权** ⇒ 最大放大源在本批之后**仍未根治**（现由 T2 护栏兜住）。
- ADR D1-5 窄窗口（发信成功但落库失败）**未在生产复验**（需构造 D1 配额耗尽 + 发信成功）。
- ADR D2-3 写预算护栏的**拒绝路径仍未在生产复验**（判据 3 未跑到，见本节「部署与生产复验轮补充」）。
- ADR D3 设备上限**已在生产复验（行为达成）**；**refresh token 上限 64 未单独复验**。
- 口令重置路径（`password_resets` 先落库后发信）**同构缺陷仍在**（ADR 0075 决策 7 明确不碰；ADR 0076 D1-7 只记录平移配方）。
- `sync_items.seq` 列迁移**未做**（D2-4）。
- D1 免费额度能否承载词典规模**未结论** ⇒ 付费议题仍未决（用户已定：不升级，等免费且可逆）。
- 承接 BQ §12 未闭环项（18 份 `run-*.log` 是否入库、`tmp\bh-c2d-fmt-check.cpp` 处置、R2 残留 164 孤儿对象是否逐个删除、2 个 `ak.probe.*@example.com` 账号是否删除）本轮**未处理**。
- **写预算护栏的口径边界（2026-11-23 19:38 轮登记的已知边界）**：D2-3 护栏上限是**每账号每 UTC 日 80,000 行**；Cloudflare D1 免费套餐的 10 万行/天是**每个 D1 数据库（全局）**额度 ⇒ 多账号各自不超自己的护栏、**累计仍可打爆全局额度** ⇒ 护栏**不是**「平台额度护盾」，而是「**单账号放大源刹车**」。2026-11-23 生产实测即属此类（全局 `rows_written_24h = 122254` 触发 `code 7500`，**该计数是全局口径**）。实现本身早已声明该近似（`src/sync.ts` 注释：护栏「**不宣称**它能精确计出 Cloudflare 的行写入账单」；`used` 只统计 `sync_items` 当日写入行数 ⇒ 行数**下界**，不含 `devices` / `refresh_tokens` 等行）。⇒ **本批不改实现**（改口径属设计变更，需独立 ADR + 用户拍板）⇒ 登记候选后续项 **BR-②**（改为全局口径 / 按活跃账号数分摊总额度；**待用户拍板**）。
- **推送写序「R2 先、D1 后」在 D1 失败时留下 R2 孤儿对象（无害，2026-11-23 19:38 轮登记）**：D1 阶段因配额耗尽抛错 ⇒ **R2 已写**（留下孤儿对象）、**D1 零落库**（`sync_items` 一行未增）⇒ 无害原因 = 索引行不存在 ⇒ **拉取看不到任何条目**（客户端不会拿到半截数据）；重试时同 key 覆盖 ⇒ **孤儿自然被替换**。
- **判据 3 的自动补跑已排定（尚未执行）**：定时任务 ID `self-2fd9cb456b4b1eb65b256941d0500f74`，北京时间 **2026-11-24 08:10** 触发（UTC 午夜 08:00 配额重置之后）⇒ 动作 = 阈值降到 10 → deploy → `node tmp/br-prod-verify.mjs c3` → 阈值恢复 80000 → 再 deploy → 校验 `cloud/fluxing-sync/wrangler.toml` 与 HEAD 逐字节一致 → `node tmp/br-prod-verify.mjs cleanup` 清理探针；**判据 3 只有在日志出现 429 `write_budget_exceeded` 且 R2/D1 零写入时才能记 PASS**。
- **19:38 轮生产状态（现状）**：当前生产 Version = `8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`（含 D1 配额错误可解释化修复；BR 主体版本 `80385732-3473-4e78-b0ba-5a8e29ca1451`；BR 之前的回滚目标仍是 BQ 版 `96facaba-9277-4c77-b9f9-4df5cb6de41e`）；生产仍处于 D1 免费额度耗尽（`code 7500`），19:36 复查 `rows_written_24h = 122254`（未重置）；真实用户登录当前返回**可解释的 503**（修复前是 500）；**判据 3 仍未完成 ⇒ 不得记为 PASS**。

### 改码轮补充（2026-11-23 · 服务端 T1–T4）

**已验证（父会话亲跑，不采信子会话自报）**

- 【实测】**服务端全量门禁**：`npm run typecheck` ⇒ **TSC_EXIT=0**；`npm run check` ⇒ `check:endpoint` 通过（`[T103 红线] 通过`）+ vitest **9 文件 / 127 用例全过 ⇒ CHECK_EXIT=0**（BQ 基线 5 文件 / 86 用例逐条保留 + 本批新增组）。
- 【实测】既有 `test/register-mail-failure.test.ts` 按 ADR D4-1a 把 3 个编码旧回滚注入点的用例迁到**窄窗口**后 **14/14 全绿**（72 条 `expect`）；文件 BOM=False / CRLF=0 / U+FFFD=0。
- 【读码】改码轮后的**设备上限新口径**：`upsertDevice` 里 `enforceDeviceLimit` 只在 **INSERT 分支之前**调用 ⇒ **只约束新设备**；同 `device_id` 重新登录走 UPDATE 分支（清 `revoked_at`、刷 `last_seen`）⇒ 不新增设备行、不被拒 ⇒ 消除「32 台活跃 ⇒ 全设备登不上」的自锁风险，**设备行数上限仍是 32**。⇒ **ADR 0076 已补 D3-1a 修订条款**并同步更正 Consequences 段对应表述。
- 【实测】`src/env.ts` 原缺文件末尾换行 ⇒ 本轮补齐（2,883 B → 2,884 B，末字节 0x0A）。

**未验证 / 未闭环（改码轮）**

- **生产复验（ADR D4-2 四条判据）**: **改码轮时点仍未做** —— 需 `wrangler deploy` 授权：保留域注册 422 且 `users`/`email_verifications` 零增量 / 重发失败零增量 / 写预算 429 且零写入 / 设备上限拒绝并给中文文案。**该项已在后续「部署与生产复验轮补充」中执行**（判据 1 / 2 PASS、判据 4 行为达成、**判据 3 未完成**）。
- **写预算护栏的真实阈值路径仍未在生产复验**（判据 3 被平台配额阻塞，见下）；单测用可写 var 注入小阈值走完整 HTTP 路径；`used` 是行数**下界**，不是精确计费器。
- **新增测试文件功能重叠**：`test/register-zero-write.test.ts`（10 例）、`test/device-token-limits.test.ts`（7 例）、`test/zero-write-register-and-limits.test.ts`（16 例）覆盖同一 D1+D3 范围；`test/write-budget.test.ts`（8 例）覆盖 D2-3 ⇒ 是否合并待用户定（本轮**未合并**，四个文件全部保留且全绿）。
- **用例计数一律以父会话亲跑的 vitest JSON 报告为准**（逐文件实测）：基线 `api` 30 / `mail-http` 20 / `password-reset` 14 / `r2-write` 8 / `register-mail-failure` 14 = **86 逐条保留**；新增 `device-token-limits` 7 / `register-zero-write` 10 / `write-budget` 8 / `zero-write-register-and-limits` 16 = **+41**；**全量 127**。⇒ 本文档与子会话自报中出现的「17 例」「11 例」等计数**均已按实测订正**，不再保留。
- **并发改动来源未查清**：改码轮期间 `src/auth.ts` 的 `upsertDevice` 与新增 `test/zero-write-register-and-limits.test.ts` 由**非本会话派出的执行体**落盘（子会话自报「非我所改」）⇒ 本会话已按「不采信自报」逐行复核其语义并亲跑全量门禁 ⇒ **commit 前必须逐文件 `git diff` 复核，避免把来源不明的改动一起入库**。

### 部署与生产复验轮补充（2026-11-23 · T6 / T7）

**已验证（生产实测）**

- 【实测】**生产部署**：`npm --prefix cloud/fluxing-sync run deploy` ⇒ **DEPLOY_EXIT=0**，新生产 Version ID = **`80385732-3473-4e78-b0ba-5a8e29ca1451`**（BQ 版回滚目标 = `96facaba-9277-4c77-b9f9-4df5cb6de41e`，对应 `314d03e`）；**本批无数据库迁移**（`a6ee188` 只改 `src/{auth,devices,env,sync}.ts` + `test/*` + `vitest.config.ts` + `wrangler.toml`；`migrations/` 仍只有 `0001_init.sql` / `0002_password_resets.sql` 且工作树干净 ⇒ 无需 `d1 migrations apply`）。
- 【实测】**判据 1 PASS**（注册发信失败 ⇒ 零写入）：HTTP **422** + `code=mail_recipient_rejected` + 中文文案「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」+ `X-Fluxing-Mail-Provider-Status: 422` + `X-Fluxing-Mail-Rollback: ok`；`users` / `email_verifications` **零增量**。
- 【实测】**判据 2 PASS**（重发失败 ⇒ 零写入）：HTTP **422** + provider-status 422 + rollback ok；`email_verifications` / `users` / `sync_items` **零增量**。
- 【实测】**判据 4 行为达成**（设备上限 32 台 ⇒ 429）：第 32 台之后登录被拒，`code=device_limit_reached` + 中文文案「此账号的设备数已达上限（32 台），请在账号页撤销不用的设备后再登录。」；`devices` 行数**恰好 32** = `MAX_DEVICES_PER_ACCOUNT`。
- 【实测】**500 的根因 = 生产 D1 免费额度耗尽，不是本批代码缺陷**：已登记 `device_id` 重新登录 ⇒ HTTP 500 `internal_error`；对生产 D1 逐条打 SQL ⇒ Cloudflare 返回 **`code: 7500`**「Your account has exceeded D1's free tier daily row write limit」（`wrangler d1 info` 显示 `rows_written_24h: 122253`，免费额度 10 万行/天）⇒ 写 **0 行**的语句（`SELECT`、匹配 0 行的 `DELETE`）照常通过，`UPDATE users` 与 `INSERT refresh_tokens` **均被拒** ⇒ **不是** BR 代码缺陷、**也不是** IPC / 协议问题；**影响面含真实用户登录**（登录要写 `refresh_tokens` ⇒ 同样 500）⇒ 生产事故级问题；UTC 午夜（北京时间次日 08:00）重置。
- 【实测】**阈值恢复后状态一致**：已按 ADR 完成「阈值 10 → 部署 → 复验 → 恢复 80000 → 再部署」全流程；阈值已安全恢复为 `WRITE_BUDGET_ROWS_PER_DAY = "80000"`，`git status --porcelain -- cloud/fluxing-sync/wrangler.toml` **为空**（与 HEAD **逐字节一致**），恢复后的部署 **DEPLOY_EXIT=0**。
- 【实测】**入库已完成**：5 个 commit = `a6ee188` / `ac23bfa` / `d628e53` / `314d03e` / `89f90fd`（区间 `3e32cb8..89f90fd`）。
- 【实测】**push 已完成**：`git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync` ⇒ **PUSH_EXIT=0** ⇒ `3e32cb8..89f90fd feat/100-cloud-sync -> feat/100-cloud-sync` ⇒ **前 5 个 commit 已全部进入远程**。
- 【实测】**D1 配额错误映射已修 + 补部署 + 生产活体验证 PASS**：**根因** = Worker 内 D1 绑定抛出的错误文本**不带** `[code: 7500]`（**只有 REST 通道带**）⇒ 旧判据 `/\[code:\s*7500\]/` 命中不了 ⇒ D1 写路径对外报笼统 **500 `internal_error`**；**修法** = `cloud/fluxing-sync/src/errors.ts` 新增 `errorText()` + `isD1RowWriteQuotaError()`（四路形态兼容 + 容忍抛出值非 `Error` 实例），`toErrorResponse` 改用该判据，`cloud/fluxing-sync/src/auth.ts` 删除本地重复副本统一走共享判据（**两处各留一份 = 漏配根因**）；**单测** = 新增 `test/d1-quota-error-mapping.test.ts`（10 条）+ `test/register-zero-write.test.ts` 追加 1 条；**门禁** = `typecheck` 0 / `check` 0 = **10 文件 / 138 用例**（原 9 文件 / 127 用例 ⇒ +1 文件 / +11 用例）；**部署** = DEPLOY_EXIT=0 ⇒ 新生产 Version **`8e0c3648-cd1e-4baa-b1ab-caa3bb0cc61a`**；**活体验证** = `tmp/br-quota-503-probe.mjs` ⇒ `LOGIN_STATUS=503` + `LOGIN_CODE=storage_quota_exceeded` + 中文「服务端存储写入额度已用尽，请稍后重试。」+ `VERDICT_QUOTA_503=PASS`（证据 `tmp/br-quota-503-probe.log`）⇒ 修复前同一路径是 **500 `internal_error`**。（本条记的是「配额错误可解释化轮」的历史门禁 **10 文件 / 138 用例**；**19:38 轮最新门禁 = 11 文件 / 143 用例**。）

**未验证 / 未闭环（部署与生产复验轮）**

- **判据 3（写预算超限 ⇒ 429 `write_budget_exceeded` + R2/D1 零写入）未跑到**：复验步骤的**登录前置**即失败（`C3_LOGIN_STATUS=500`）⇒ 拿不到 access_token ⇒ 推送判据本身没有跑到 ⇒ **不得记为 PASS**；需等 D1 配额重置后按「阈值 10 → 部署 → `node tmp/br-prod-verify.mjs c3` → 恢复 80000 → 再部署 → 校验 `wrangler.toml` 与 HEAD 一致」补跑。
- **探针残留未清理**：`br.probe.1795431653344@example.com`（`users` 1 行 + `devices` 32 行 + `refresh_tokens` 64 行）与 `br.c2.1795431653344@example.com`（1 行）仍留在生产 D1；**清理同样被 D1 配额阻塞**（删除行也是行写入）⇒ 需等配额重置后按前缀删除。
- **生产 push 已完成**：前 5 个 commit **已 push**（`3e32cb8..89f90fd`，PUSH_EXIT=0）⇒ 不再是待办；本轮新增的两笔提交（服务端配额映射修复 + 文档回写）由收尾**快进续推**。
- **D1 配额错误映射（`code 7500` ⇒ 可解释 503）已在本批修掉**：旧判据只认带 `[code: 7500]` 的 **REST 形态**，而 **Worker 内 D1 绑定抛出的错误文本不带该标记** ⇒ 登录 / 续期 / 推送等写路径报笼统 500 ⇒ 现由 `cloud/fluxing-sync/src/errors.ts` 的共享判据（`errorText()` + `isD1RowWriteQuotaError()`）覆盖，**生产活体验证 PASS**（503 `storage_quota_exceeded` + 中文「服务端存储写入额度已用尽，请稍后重试。」）。
- **D1 免费额度耗尽本身仍是生产事故级问题**：影响面含**真实用户登录写路径**（登录要写 `refresh_tokens`）⇒ UTC 午夜（北京时间次日 08:00）自动重置；**是否升级 Workers Paid 需用户拍板，本轮未执行**。
- BR-①c 客户端「远端 HLC 沿用」**未实现、未授权**（用户明确**本批不开工**）⇒ 最大放大源仍未根治（现由 T2 护栏兜住）。
- ADR D1-5 窄窗口（发信成功但落库失败）、D2-4 `seq` 列迁移、口令重置路径同构修复（ADR 0075 决策 7 明确不碰）**仍未做**。

## §4 证据路径

- ADR：`F:\soft\00selfmade\rime_claude\docs\adr\0076-cloud-write-amplification-governance-and-zero-write-registration.md`
- 服务端：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\{sync,auth,wire,devices,account,mail}.ts`、`migrations\0001_init.sql`、`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\push-quota-error-mapping.test.ts`
- 客户端（只读取证）：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\{CloudCrypto,CloudSyncService,CloudSyncEngine,CloudAccount,CloudAccountDialog,CloudCredentialStore}.{h,cpp}`
- 门禁：`npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run typecheck` / `run check`
- BQ 复验脚本（凭据只在 tmp，禁止入库）：`F:\soft\00selfmade\rime_claude\tmp\{bq-verify.ps1,bq-e2e-mailtm-v4.ps1,bq-v4-deploy-verify.ps1,bq-d1-cleanup.ps1}`
- BR 部署与生产复验证据（**全部在 `tmp/`，禁止入库**）：`F:\soft\00selfmade\rime_claude\tmp\br-probe-{setup,c1,c2,c4}.log`、`tmp\br-probe-c3-run.log`、`tmp\br-deploy-1.log`、`tmp\br-d1-probe.log`、`tmp\br-d1-baseline-20261123.json`、`tmp\br-prod-verify.mjs`、`tmp\br-threshold.ps1`、`tmp\br-push-quota-check.log`

## §5 本轮工具坑

- `Set-Location` **不改 .NET 进程 CWD** ⇒ `[IO.File]::ReadAllLines('src\auth.ts')` 会报「找不到路径」⇒ 一律用绝对路径。
- CJK 文件写入必须字节级：`-join "`n"` 拼行 + `[IO.File]::WriteAllText(path, text, UTF8Encoding($false))` ⇒ 避免 PS 默认编码污染；`CHANGELOG.md` 是 CRLF-only，handoff / ADR / prompt 是 **LF-only**，`CLAUDE.md` 插入段按文件自身行尾拼。
- PowerShell 单引号字符串里 **反引号是字面量** ⇒ markdown 的反引号只能在单引号串里写；双引号串会吃掉反引号。
- **bash 里长篇中文内联脚本容易被本地安全网关误判为删除类命令而整体拦截** ⇒ 正文不要出现删除命令字面量；长文落盘改用专用写文件工具，bash 只做小补丁。
- `wrangler d1 execute --command` 的 SQL 必须压成单行；Cloudflare API 与 mail.tm 必须直连（清代理变量）；`wrangler tail` 本机彻底不可用 ⇒ 诊断一律靠响应头自证。
- 文件删除用本机受信任的可恢复删除器（路径见 `CLAUDE.md` 工具坑），不要用 `Remove-Item`（被本地硬安全策略拦截）。

### 部署与生产复验轮新增（2026-11-23）

- **`wrangler tail` 在本机不可用**：直连 `157.240.12.35:443` ETIMEDOUT ⇒ 生产 500 **拿不到服务端堆栈** ⇒ 只能靠「对生产 D1 逐条打 SQL」反推根因。
- **判定「D1 配额耗尽」的定位手法**：用「**对生产 D1 逐条打 SQL** + `wrangler d1 info` 读 `rows_written_24h`」定位（写 0 行的 `SELECT` 照常通过，而 `UPDATE` / `INSERT` 被拒 ⇒ 据此反推是配额而非代码缺陷）—— 之所以只能走这条路，是因为本机 `wrangler tail` **直连 ETIMEDOUT 不可用**，拿不到服务端错误原文与堆栈。
- PS7 `Invoke-WebRequest.Content` **可能是 `byte[]`** ⇒ 须 `[Text.Encoding]::UTF8.GetString($raw)` 解码，否则 JSON 解析必挂。
- `wrangler d1 execute --command` 的 SQL **必须单行**（多行会被当成非法语句）。
- Cloudflare API **必须直连**：清 `HTTP_PROXY` / `HTTPS_PROXY` / `ALL_PROXY` **及小写变体**，并设 `NO_PROXY=*`。
- D1 免费额度按「**行写入**」计 ⇒ 写 0 行不消耗；**删除行也是行写入** ⇒ 额度耗尽时连清理探针都做不了。
- `cmd //c "build.bat …"` 会**假绿**（被 MSYS 吞参数）⇒ 必须 `cmd /c`。
- 本地单测用**假后端**，**跑不出真实 D1 平台语义** ⇒ 本次生产 500 在本地单测同场景是 **200** ⇒ D1 配额类问题**必须**在生产验证。
- 后台脚本**必须用绝对路径** ⇒ `Set-Location` **不改 .NET 进程 CWD**（同上文旧坑，本轮再次踩到）。
- 长篇中文落盘用**字节级** `[IO.File]::WriteAllText(..., UTF8Encoding($false))` ⇒ bash 内联中文长脚本易被安全网关拦截。

## §6 Git 与提交现状

- 分支：`feat/100-cloud-sync`；**前 5 笔 commit 已 push**（远程 `3e32cb8..89f90fd`）⇒ `git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync` ⇒ **PUSH_EXIT=0** ⇒ `3e32cb8..89f90fd feat/100-cloud-sync -> feat/100-cloud-sync`。
- **本批入库的 5 个 commit**（**已 push**）：
  - `a6ee188` `fix(fluxing): 服务端写放大治理与失败注册零写入化（批次 BR · ADR 0076 · T1-T4）`（11 文件）；
  - `ac23bfa` `docs(fluxing): ADR 0076 推送写放大治理与失败注册零写入化（批次 BR）`；
  - `d628e53` `docs(fluxing): ADR 0075 + BQ batch handoff for register mail rejection semantics`；
  - `314d03e` `fix(fluxing): register mail rejection semantics + failed registration rollback`；
  - `89f90fd` `docs(fluxing): 回填批次 BR 服务端 T1-T4 实施进度与 CHANGELOG 条目`。
- **本轮新增两笔提交**（服务端 D1 配额错误映射修复 + 本次文档回写）由**收尾时快进续推**（命令同上）⇒ 当前提交序列见 `git log --oneline -3`（**不写死 hash**，避免自指漂移）。
- **工作树**：除 `librime`（dirty submodule，**不得 `git add librime`**）与下列未跟踪杂项外**干净** ⇒ **已跟踪改动全部入库**。
- **禁入库杂项**（未跟踪，勿 `git add`）：`release/`、`reports/**`、`scripts/verify/*.ps1`、`docs/design/quickpanel-v6/design-demos/*.html`、`tmp/**`、`*.log`、`env.bat`、`weasel.props`。
- **已部署 / 已 push / 未改客户端 / 未 bump 版本号**（装机版仍 1.3.8.0）。
- **入库红线**：显式按路径 `git add <path>`，**禁用 `git add .`**；commit 前必须 `git status -sb` + `git diff --stat`；Conventional Commits + 模块前缀（服务端批只能 `fluxing` / `docs` / `ci`）。

### 接手核对轮补充（2026-11-23 20:25 · 新会话接手 · **未改产品代码**）

**本轮只做「状态核对 + 定时任务指令订正 + 文档回写」**；取证 / 改码 / 部署 / 入库 / push **一律未重做**。

- 【实测】**分支已完全同步**：本地 `HEAD` = `cedd19a` = 远程跟踪 `kizemo/feat/100-cloud-sync` ⇒ `git status -sb` 显示 `## feat/100-cloud-sync...kizemo/feat/100-cloud-sync` 且 **无 ahead/behind 标记** ⇒ **上一轮遗留的「2 个 commit 未推」已不存在**（cron 指令里那一步据此删去）。
- 【实测】**明早补跑链路在位**：定时任务 `self-2fd9cb456b4b1eb65b256941d0500f74` = `scheduleType=once` / `runAtMs=1795479046336`（= 北京时间 **2026-11-24 08:10:46**）/ `enabled=true` / `status=active` / **运行历史为空 ⇒ 从未执行**；绑定会话 `mvs_a819329ea0b042f48e756cd8ef0fc84f`（**未归档** / idle）⇒ 补跑结果落在该会话。
- 【实测】**本轮 `cron_update` 已改写任务指令**：删去已过时的「补推 2 个 commit」步骤；新增「第 4 步 · 收尾双产物」（要求补跑轮结束后就地改写本 handoff + `CLAUDE.md` BR 指针 + relay prompt）。
- 【实测】**补跑脚本逐行核对在位**：`tmp/br-c3-run.ps1` 第 93-96 行输出四行 `FINAL_C3_VERDICT` / `FINAL_THRESHOLD_RESTORED` / `FINAL_WRANGLER_TOML_CLEAN` / `FINAL_CLEANUP_VERDICT`；第 86-90 行 STEP7 调 `node tmp/br-prod-verify.mjs cleanup` 并抓 `VERDICT_CLEANUP`；`tmp/br-prod-verify.mjs` 支持 `setup|c1|c2|c3|c4|cleanup|status|all` 全部 stage ⇒ **明早只需跑这一条脚本**（`pwsh -NoProfile -File tmp\br-c3-run.ps1`）。
- 【实测】**接力 prompt 已存在且为最新**：`docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`。
- 【实测】**本会话已设一次性回检**：`self-390d58e2e3374a4d410517e7402a8902` ⇒ 北京时间 **2026-11-24 08:20 前后**读 `tmp/br-c3-run.log` 四行 + `git status -sb` ⇒ 只有 `FINAL_C3_VERDICT=PASS` 才可记判据 3 PASS。
- 【如实】**本轮门禁未重跑**（未改任何产品代码）⇒ 沿用 19:38 轮结论（`typecheck` 0 / `check` 0 = **11 文件 / 143 用例**）。
- 【实测】**工作树现状**（本轮开始时）：`M CLAUDE.md`（上一轮未入库的 BR 指针回写）+ `m librime`（dirty submodule，**禁止 `git add librime`**）+ 未跟踪杂项（`release/` / `reports/**` / `scripts/verify/*.ps1` / `docs/design/quickpanel-v6/design-demos/*.html`）⇒ 按红线不入库。
- **待用户拍板三项（本轮未动）**：Workers Paid 是否升级 / **BR-①c（客户端远端 HLC 沿用）是否授权** / **BR-②（护栏是否改全局口径或按活跃账号数分摊总额度）**。

---

### 判据 3 补跑轮（2026-11-24 08:12 · 无人值守自动补跑 · **判据 3 = PASS**）

**本轮性质** = 生产 D1 免费额度 UTC 午夜（北京时间 08:00）重置后，**一键脚本无人值守补跑**（`tmp/br-c3-run.ps1`）⇒ 服务端写预算护栏判据 3 首次取得判定结果；**未改任何产品代码**。

- 【脚本内实际输出（08:11:08 → 08:12:18）】：阈值 80000 → **10**（`WRITE_BUDGET_ROWS_PER_DAY = "10"`）→ `STEP2_DEPLOY_EXIT=0` → `node tmp/br-prod-verify.mjs c3` → **`C3_VERDICT=PASS`**（`STEP3_C3_EXIT=0`）→ 恢复 **80000**（`GIT_CLEAN=TRUE`，`THRESHOLD_RESTORE=script_ok`）→ `STEP5_DEPLOY_EXIT=0` → `WRANGLER_TOML_CLEAN=True` → `STEP7 cleanup` → `STEP7_CLEANUP_EXIT=0` → `C3_SCRIPT_EXIT=0`。
- 【判定四行（只认这四行，退出码不作数）】：**`FINAL_C3_VERDICT=PASS`** / **`FINAL_THRESHOLD_RESTORED=True`** / **`FINAL_WRANGLER_TOML_CLEAN=True`** / **`FINAL_CLEANUP_VERDICT=PASS`**。
  - ⇒ **判据 3「写预算超限 ⇒ 429 `write_budget_exceeded` 且 R2/D1 零写入」= PASS**（探针日志同时命中 HTTP 429 + `write_budget_exceeded`，且 D1 `sync_items` 行数零增长）。
  - ⇒ **阈值已安全恢复 80000**（生产**不再**停在 10 行预算 ⇒ 真实用户推送不会被 429）。
  - ⇒ **`cloud/fluxing-sync/wrangler.toml` 与 git HEAD 逐字节一致**（`GIT_CLEAN=TRUE`）。
  - ⇒ **探针残留已清零**：【实测】`SELECT email FROM users WHERE email LIKE 'br.%'` 返回 **0 行**（`br.probe.*` / `br.c2.*` 已删）⇒ 前一轮登记的「生产探针残留未清理」**已闭环**。
- 【实测 · 生产 D1 额度回读】（直连、清代理）：`rows_written_24h = 69428`（额度**确已重置**且未再耗尽，< 100,000 免费额度）/ `write_queries_24h = 27704` / `rows_read_24h = 5216989`。
- 【实测 · 生产部署时刻】恢复部署（阈值 80000）于 **2026-11-24T00:11:50.899Z**（= 北京时间 **08:11:50**）⇒ 生产运行的即**阈值 80000 的恢复版**。
- 【本轮未动】= 未改产品代码 / 未 bump 版本号 / 未手改 `wrangler.toml`（脚本自身恢复并校验）/ 未重做既有取证 ⇒ 门禁沿用前轮结论（`typecheck` 0 / `check` 0 = 11 文件 / 143 用例；客户端 PASS=787 FAIL=0）。
- 【本轮附带发现】昨天为 BR-①c 建的 09:00 AC-09 复跑定时任务 `80a9267f-eff9-4091-924c-9cc1d8f3a0bd` 已变为 `status=deleted` / `enabled=false` ⇒ **09:00 不会自动触发** ⇒ 已改为在本会话直接复跑（AC-09 必须排在判据 3 收尾之后 —— 判据 3 的 cleanup 删行同样计入 `rows_written_24h`，两轮重叠会污染 AC-09 的零增量基线）。

### BR-①c 轮（2026-11-23 21:50 · 客户端「远端 HLC 沿用」根治 · ADR 0076 D2-5a）

**本轮性质** = **用户已授权改客户端代码** ⇒ 客户端红线解除，动 `WeaselServer/Cloud/**`；交付链 = ADR 修订 → spec/plan/tasks 三件套 → 客户端实现 → 单测 → 门禁 → 出包装机 → 装机实证 → 真机判据（**本轮 = 未判定，见下**）→ 收尾双产物 → 入库 + push。
**未动** = 服务端（判据 1/2/4 成果不返工）、`librime/`、IPC 协议与 `include/WeaselIPCData.h`、`output/install.nsi`（⇒ 不触发 G-a/G-b/G-c 与 Sandbox harness）、公开版本号（仍 1.3.8.0）。

#### §A 本轮已达成（已验证）

- 【已入库 + 已 push】ADR 0076 修订稿（D2-5a a-0…a-8）= commit **`d4b5108`**（`14635a5..d4b5108`，PUSH_EXIT=0）。
- 【已落盘，**同 commit 入库**】ADR 二次修订：a-3 改**超集口径**（闸门触发 = `has_more == true`，解除 = 某轮 `has_more == false`）+ 副作用登记；a-8 新增 ⑤「新增本地词条上云被延后」。
- 【已落盘】spec/plan/tasks 三件套 = `.specify/specs/101-cloud-sync-remote-hlc-reuse/{spec.md,plan.md,tasks.md}`。
- 【已落盘 + 已复核】客户端实现（现役路径 = `WeaselServer/Cloud/CloudSyncService.cpp` 的 `CloudSyncCoordinator::SyncOnce`；`CloudSyncEngine.cpp` **未改**）：
  - `CloudSyncService.h`：`CloudReconcileStats` :378、`FormatReconcileDiagLine` :391、`last_reconcile_stats()` :436、`SyncRound` :445、`last_stats_` :465。
  - `CloudSyncService.cpp`：`SyncOnce` :888 调 `SyncRound` :904；**无条件**诊断行 :900；HLC 分配移到**拉远端之后**；**R1**（明文 payload 逐字节相等 ⇒ 沿用远端 HLC）:1135；**R2** 闸门 `inventory_unproven = has_more` :1107、deferred 判定 :1177、推送过滤 :1249；**R4** :1146；**R3** 侧车重建 :1360。
- 【已跑 · 父会话亲跑全量门禁】`msbuild weasel.sln Release|Win32` BUILD_EXIT=0（23,590 ms）；`TestDefaultHotkeys` / `TestWeaselIPC` / `TestFluxingCloud` 全 EXIT=0 ⇒ **PASS=787 FAIL=0**（基线 708 ⇒ **+79**，零删除，新增用例组 `test/TestFluxingCloud/TestFluxingCloud.cpp` :4504 起）；U+FFFD=0（7 文件）；clang-format 18.1.8：`CloudSyncService.cpp/.h` **漂移 0**，`TestFluxingCloud.cpp` 261（= HEAD 自带历史债，新增块零新漂移）。日志 `tmp/br1c-parent-gate.log`。
  - **既有遗留（本批不修）**：`release\TestResponseParser.exe` MISSING ⇒ `weasel.sln` Release|Win32 **未构建该工程**。
- 【已落盘】实现报告 `reports/fluxing-br-1c/impl-report.md`（8,649 B：改动坐标 / 门禁表 / 已知边界 6 条 / 未闭环项 / 设计歧义点）。
- 【已落盘】`CHANGELOG.md` `## [1.3.8]` 段顶部新增用户可见条目（简体中文，宪法 P2/P5）。
- 【已出包 · 真出包】新安装包 `output\archives\fluxing-1.3.8-installer.exe` = **42,981,713 B / sha16 `017722F3FD0EC512` / 2026-11-23 21:39:38**（副本 `tmp\fluxing-1.3.8-br1c-installer.exe`；BP 旧包备份 `tmp\fluxing-1.3.8-installer.bp-20261122.bak.exe` = 42,984,891 B / sha16 `15B5DB6732B33E8C`）。
  - **载荷三处 sha256 逐字节相等**（构建产物 `output\Win32\WeaselServer.exe` = 包内 = 装机后 `D:\Program Files\fluxing\weasel\WeaselServer.exe`）= `5201E31F2AD18CAADA8514C7A482A8A3961759C968E80A968566EBE5D375176D` ⇒ `SHA_MATCH_BUILD_VS_INSTALLED = True`。
  - 新 shim `weaselx64.1.3.8.1.0.dll`（1,988,608 B / 21:37:58）⇒ **全新名，未覆盖任何在用映射**（规避 L107）。
  - **版本闸门未破**：`weasel.props` 的 `PRODUCT_VERSION`/`FILE_VERSION` 仍 **1.3.8.0** = `version.txt`(1.3.8) + ".0"；本批只本地把 `env.bat` 的 `WEASEL_BUILD` 0→1（`env.bat` 未跟踪、**不入库**，备份 `tmp/env.bat.br1c.bak`）⇒ 正式发布（1.3.9 + `version.txt` + 版本线 + git tag）属**独立发布决策**，本批不做。
- 【已装机实证】`cmd /c "<installer> /S /D=D:\Program Files\fluxing"` ⇒ **INSTALL_EXIT=0**（无需 `/FORCE`；`guard_install_path.nsh` 的 `SetErrorLevel 2` 只在**装到非注册表 InstallDir** 时触发）；WeaselServer 进程已换新（pid 285340 @11-22 16:16:09 → **pid 987684 @2026-11-23 21:40:53**）。

#### §B 真机判据 AC-09 —— **本轮 = 未判定**（不是 PASS）

- 【实测】2026-11-23 21:47 轮：探针移走侧车（`fluxing-cloud-sync.state` → `.ac09bak`，5,008,953 B）→ 触发同步 ⇒ **同步实际失败**：
  - `SYNC_STATUS_AFTER=最近同步：2026-11-23 21:47:51 · 失败（服务端存储写入额度已用尽，请稍后重试。）`；
  - 诊断日志三轮（21:41:29 / 21:42:43 / 21:47:51）均为 `reconcile scan_complete=0 matched=0 deferred=0 divergent=0 pushed=0` ⇒ **远端条目一条都没拉到**（`deferred=0` ⇒ 无条目进入比对）⇒ 拉取/鉴权在配额耗尽下直接失败。
  - D1 `rows_written_24h` 122254 → 122254（增量 0）、`sync_items` 40061 → 40061（增量 0）。
- 【**假绿根因（已定位并修）**】探针 v1 的 `$syncOk` 只判「BK UI 探针跑完 + 读到 1017 文本」，**未判同步成败** ⇒ 失败态被记成 `AC09_VERDICT=PASS`。
  - **该零增量无判别力**：旧行为（sidecar 丢失 ⇒ 重分配 HLC ⇒ 整库重推）在配额耗尽下同样被拒 ⇒ 同样 delta=0 ⇒ **正反两种实现都给出相同数字**。
- 【已修】探针 **v2**（`reports/fluxing-br-1c/probe-ac09.ps1`，纯 ASCII）：
  - 判定依据从「UI 状态文本」改为**客户端诊断日志的无条件 reconcile 行**（每轮必打 ⇒ 不依赖 1017 的 10 秒观察窗口）；
  - 三态：`PASS`（轮次确实跑了 且 D1 增量 = 0 且 `push batch` 行数 = 0）/ `FAIL`（轮次跑了但写入非 0 ⇒ 真回归）/ `UNDETERMINED`（轮次没跑起来 ⇒ 外部阻塞）；
  - 非 PASS ⇒ **自动回滚侧车**（避免用户侧车缺失）；PASS ⇒ 保留客户端重建的新侧车，`.ac09bak` 留档。
- 【已执行】21:50:08 用 `-RestoreSidecar` 把侧车**回滚到原位**（5,008,953 B / mtime 2026-11-23 10:39:09）⇒ 用户状态已复原，明早复跑从完整状态起步。
- 【已排定】明早 **09:00** 一次性自动复跑（北京时间 2026-11-24），命令 `pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\reports\fluxing-br-1c\probe-ac09.ps1`，日志 `reports/fluxing-br-1c/ac09-run.log`，判定 token `AC09_VERDICT=` + `AC09_SUMMARY`。
  - **复跑前提**：生产 D1 免费额度在**北京时间 2026-11-24 08:00**（UTC 午夜）重置 ⇒ 08:00 前复跑必然 `UNDETERMINED`；判据 3 的补跑（08:10:46）会写 D1 ⇒ 排在它之后以免污染增量基线。

#### §C 本轮查清的关键事实（判定强度限制，**必须知道**）

- 【实测 · 源码】客户端单轮拉取上限 = `kPullPageLimit = 100`（`CloudSyncService.cpp:877`）× `kMaxPullPages = 10`（:879）⇒ **单轮最多 1,000 条**；游标 `since = last_server_seq_`（:1035）**持久化在侧车里**。
- 【实测 · 数据】云端 `sync_items` = **40,061 条** ⇒ 删 sidecar 后 `has_more` **必然为 true** ⇒ R2 闸门（`inventory_unproven = has_more`，:1107）**必然生效** ⇒ 第一轮零写入。
  - ⇒ **AC-09 一轮判据证明的是「闸门生效（不整库重推）」，不是「HLC 沿用逐条生效」**：后者需等库存扫完（`scan_complete = 1`，仅当 `has_more == false`，:1111-1113）。
  - ⇒ **收敛代价（新增边界）**：删 sidecar 后需约 **41 轮**（40061 ÷ 1000）才能扫完库存 ⇒ **新增本地词条上云被延后到那之后**（不丢数据，但延迟大）⇒ 登记为 **BR-①e 候选**（可考虑「扫完前允许推送新条目」或「提高单轮页数」，两者各有权衡，**须独立 ADR + 用户拍板**）。
- 【推断 · 已登记为间接证据】ADR a-7 要求的「服务端 `handlePull` 的 `healed` 自愈 DELETE 为空」**服务端未暴露计数接口** ⇒ 只能由「`sync_items` COUNT 增量 = 0」**间接**证明（healed 会 DELETE 远端畸形条目 ⇒ COUNT 会减少；COUNT 不变 ⇒ 未发生 healed DELETE）。
- 【实测】诊断日志 `%TEMP%\fluxing-profile-notify.log`（行形 `[时间戳][cloudsync] …`，`include/FluxingDiagLog.h`）**2 MB 封顶后停止追加** ⇒ 当前 593,363 B（未满）⇒ 判据依赖它 ⇒ **复跑前必须先查 `bytes`，超限则先清日志**。
- 【实测】用户数据目录 = HKCU `SOFTWARE\Fluxing\Weasel` :: `RimeUserDir` = `D:\Program Files\fluxing\user1\fluxing`；侧车 = `<该目录>\fluxing-cloud-sync.state`。

#### §D 本轮工具坑（新增，勿重试）

- **`xbuild.bat` 需 VS 开发环境**（:3-6 `if not defined include` 守卫 ⇒ `exit /b` **无错误码** ⇒ 从普通 PowerShell 调**必假绿**）：首次出包因此产出了**与 BP 旧包逐字节相同**的包（sha16 `15B5DB6732B33E8C`，根本没重打）。修法 = 落盘 `tmp/br1c-build-installer.cmd`（纯 ASCII：`VsDevCmd.bat` → x64 TSF → x86 sln → NSIS），并把 `env.bat` 的 `WEASEL_BUILD` 0→1。
  - **判据提示**：`cmd //c "build.bat …"` 会假绿（被 MSYS 吞参数）⇒ 必须 `cmd /c`。
- **判据设计铁律（本轮新增）**：**「失败态」也能给出全部为零的数字** ⇒ 涉及「写入增量 = 0」的判据，**必须同时证明动作真的发生了**（本轮用「诊断日志出现新的无条件 reconcile 行」代替 UI 状态文本）。
- **UI 观察窗口不可靠**：BK 探针 `POLL_SECONDS=10`；服务端同步失败时 1017 文本会变（「失败（…）」）⇒ **只看「文本是否变化」会把失败当成功**。

#### §E 本轮未闭环 / 下一步

1. **判据 3（服务端写预算）**：定时任务 `self-2fd9cb456b4b1eb65b256941d0500f74`（北京时间 **2026-11-24 08:10:46**）自动补跑；回检 `self-390d58e2e3374a4d410517e7402a8902`（08:20 前后读 `tmp/br-c3-run.log` 四行）⇒ **只有 `FINAL_C3_VERDICT=PASS` 才可记 PASS**。
2. **AC-09 客户端判据**：明早 **09:00** 自动复跑（定时任务 ID `80a9267f-eff9-4091-924c-9cc1d8f3a0bd`，`runAtMs=1795482000336`，`scheduleType=once`，`status=active`；探针 v2 已就位）⇒ **本轮记「未判定」，不得记 PASS**；复跑后若 `AC09_VERDICT=PASS` ⇒ 需补记「第二层判据」：库存扫完那一轮仍零写入（需再跑约 41 轮，见 §C）。
3. **生产探针残留**：`br.probe.1795431653344@example.com` / `br.c2.1795431653344@example.com` ⇒ 删除行也计写入 ⇒ 等额度重置后按前缀清理并回读零残留。
4. **待用户拍板（未动）**：Workers Paid 是否升级（维持不升级）/ **BR-②（护栏是否改全局口径或按活跃账号数分摊）** / **BR-①e（库存扫完前是否允许推送新条目 / 是否提高单轮页数）**。
5. **入库存**：ADR 二次修订、客户端实现 + 单测、spec 三件套、`CHANGELOG.md` 条目、本 handoff 追加段、relay prompt 改写 ⇒ **显式按路径 `git add`**，**禁 `git add .`**，`librime` 禁 add。

---

### §F · AC-09 真机判据复跑轮（第 1 次）—— 判据未达成（FAIL）

**本轮性质** = 客户端判据 **AC-09** 的**第 1 次真机复跑**（BR-①c 轮收尾复验）⇒ **判据未达成 = FAIL**；
**根因已坐实** ⇒ 修订决策落 ADR 0076 **D2-5a-9（a-9 语义指纹）** ⇒ **代码修复待实施**。
**本轮未改任何产品代码**（仅文档回写 + 探针脚本修正）⇒ 门禁未重跑，沿用 BR-①c 轮结论
（客户端 `PASS=787 FAIL=0`；服务端 `typecheck` 0 / `check` 0 = 11 文件 / 143 用例）。

#### §F-1 判据结果

- 【实测】**AC-09 真机判据第 1 次复跑 = FAIL（判据未达成）**。
- 【实测 · 工具坑】**探针 v2 的正则缺陷**：探针 `reports/fluxing-br-1c/probe-ac09.ps1` 的 v2 版本用 `^\[` 匹配诊断行，
  但真实诊断行**行首没有 `[`** ⇒ **`^\[` 永不匹配** ⇒ 机器判定 `AC09_VERDICT=UNDETERMINED`。
  - ⇒ **UNDETERMINED 不覆盖硬数据** ⇒ 据硬数据登记 **FAIL**（**不是**「未判定」，**更不是** PASS）。
  - 【已修】探针已修到 **v3**（正则改 `^\[?`）。
- 【实测 · 生产 D1 只读回读】PRE `rows_written_24h = 69428`、`sync_items` COUNT = **40061**
  ⇒ POST `rows_written_24h = 72420`（**+2,992**）、`sync_items` COUNT = **40061**（**+0**）
  ⇒ **全是既有行 UPDATE、无新增行** ⇒ 判据「写入增量 = 0」**不成立**。
- 【实测 · 诊断日志】`%TEMP%\fluxing-profile-notify.log`（**行首无 `[`**）：
  - `2026-11-24 08:14:19.339][cloudsync] reconcile scan_complete=0 matched=0 deferred=54536 divergent=67 pushed=3424`
  - `2026-11-24 08:29:53.568][cloudsync] reconcile scan_complete=0 matched=0 deferred=54536 divergent=67 pushed=3072`
- 【实测】`2026-11-24 08:27:58.632][cloudsync] push batch items=32 elapsed_ms=2812 http=503 err=服务端存储写入额度已用尽，请稍后重试。`
  ⇒ **外部阻塞** = 生产 D1 免费额度耗尽（UTC 日重置 = 北京时间 **2026-11-25 08:00**）⇒ **干净一轮须等重置后复跑**。
- 【已排定】复跑定时任务 **`4d3e51f8-fdda-41bb-bbc1-12e844914dc5`**（`scheduleType=once` /
  `runAtMs=1795565446336` = 北京时间 **2026-11-25 08:10:46**）。

#### §F-2 根因（已坐实）

- 【实测 · 源码】**侧车 `payload_hash` 取的是整条 payload 的 sha256**：当前哈希 = `Sha256Hex(entry.payload)`
  （`WeaselServer/Cloud/CloudSyncService.cpp:1127` 算当前值、`:1381` 写入侧车行、`:1578` 序列化、`:1630` 解析回读）。
- 【实测 · 源码】**payload 形态** = `<码><空格><TAB>c=<commits> d=<dee> t=<tick>`
  （`WeaselServer/Cloud/CloudUserDictIo.cpp:97-108` 装配、`:326-337` `SplitDictPayload` 切分、`:343` 形态注释）。
- 【实测 · 源码】**客户端每轮同步都会把合并结果回写本地词典**（`restore_user_dict` → rime `UserDbMerger`，
  调用点 `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp:157-159`）⇒ merger 重算 `d=`
  （`librime/src/rime/dict/user_db.cc:208` / `:216`，`formula_d(0, their_tick_, v.dee, v.tick)`）
  并把 `t=` 改写成 db 当前 tick（`librime/src/rime/dict/user_db.cc:221`，`o.tick = max_tick_`）
  ⇒ **同一未改词条的 payload 哈希每轮必变**。
- 【实测】**后果**：本地改动判定（侧车哈希 vs 当前 payload 哈希，`WeaselServer/Cloud/CloudSyncService.cpp:1151` /
  `:1164`）**每轮都判「改过」** ⇒ 每轮分配**新墙钟 HLC**（`HlcTick(&clock_, now_ms)`，`:1152` / `:1174`）
  ⇒ 服务端 HLC 守卫失效 ⇒ 既有 `sync_items` 行被反复 UPDATE ⇒ **写放大**（ADR 0076 D2-5a-8 ② 的**真实机理**）。
- 【实测】**「每轮同一前缀 3,072 条」是截断假象**：本轮 **94,604** 条本地条目中，**54,536** 条 deferred
  （无侧车记录 + 库存未证实）、**67** 条 divergent、**40,001** 条被判本地改动 ⇒ 待推集合 = **40,001** 条，
  按本地（词典序）排列 ⇒ 推送被服务端 503 额度**截断在第 3,072 条** ⇒ 看起来像「同一批键反复 churn」
  （实测 **OVERLAP = 3072 = 全含**）⇒ **侧车已知条目本轮 40,068 条全部被判本地改动**。
- 【实测 · 旁证】侧车两版比对（`fluxing-cloud-sync.state` 08:29:53 vs `fluxing-cloud-sync.state.failed-ac09` 08:14:19）
  ⇒ **HASH_DIFF = 3072 / HLC_DIFF = 3072**；全量哈希对齐（对当前导出快照）⇒ `.state` **fullMatch = 3072** /
  **noMatch = 36,988**（其余条目自 11-23 起未刷新哈希）。
- 【实测 · 已排除的解释】**导出快照不是陈旧文件**：全盘普查 `*.userdb.txt` 只有 1 个
  （`…\sync\<uid>\rime_ice.userdb.txt`，mtime **2026-11-24 08:14:23**，**94,610** 行 = 4 头 + 94,604 数据行），
  `FindSnapshotFile` 的候选顺序里**第 1 个不存在、第 2 个存在** ⇒ 导出读到的就是**最新快照**。
- 【实测 · 快照】94,604 行中 `t=` 只有两个取值（**27610 × 94,588**、**27024 × 16**）⇒ tick 被回写归一；
  `d=` 存在 **1.12192e-66** 量级的连续衰减值 ⇒ 与 `formula_d` 反复缩放一致。

#### §F-3 修复设计（a-9，已定案；**代码由父会话实施**）

决策全文落 ADR 0076 **D2-5a-9**（`docs/adr/0076-cloud-write-amplification-governance-and-zero-write-registration.md`）：

- **a-9① 语义指纹**：payload 的 **code 段 + 值段剔除 `d=` / `t=` token 后的剩余**（**保留 `c=` 学习次数**）
  ⇒ 对回写导致的 `d=` / `t=` 漂移**不敏感**、对**真实学习**（`c=` 增长）**敏感**。
- **a-9② 判定口径统一**：本地改动判定 / R1 远端一致判定 / R4 分歧判定 / **回写判定** / 侧车哈希字段
  **一律改用语义指纹**；侧车哈希字段改存 **`s2:<sha256(语义指纹)>`**。
- **a-9③ 旧口径兼容**：旧侧车行（**裸 64 位 hex、无 `s2:` 前缀**）**基线不可比** ⇒ 本轮**不推送**
  （避免一次性 4 万条洪峰）+ **抬高本地 HLC**（本地胜出，防云端旧版回灌覆盖本地）+ 侧车重建时把该行
  **升级**为 `s2:` 口径（重新锚定基线）⇒ 之后的真实改动仍能被发现。
- **a-9④ 回写止血**：回写判定改语义口径 ⇒ `d=` / `t=` 差异**不再触发整库回写** ⇒ rime 回写不再改写本地词典
  ⇒ **漂移源头同时断掉**（双管齐下）。
- **a-9⑤ 已知边界**：① 云端保留旧的 `d=` / `t=` 值、不再随本地更新（语义口径忽略它）；
  ② 升级前发生、且从未上云的本地改动**不会自动上传**（推荐迁移 = 升级后删一次侧车，走完整库存重扫、逐条与云端比对）；
  ③ 本地 **54,536** 条从未上云的条目仍按 R2 闸门延后到库存扫完（约 41 轮）⇒ 属**独立议题 BR-①e**，
  需用户拍板（是否允许推送新条目 / 是否提高单轮拉取页数 / 是否升级 Workers Paid）；
  ④ 修复后单轮写入量由「每轮 3,072+ 行」降为 **0**（AC-09 场景），但**库存扫完后会一次性补推那 54,536 条**
  ⇒ 服务端每账号每日 **80,000** 行预算与 **D1 免费额度**需一并复核（BR-② / Workers Paid **待用户拍板**）。

#### §F-4 已验证 / 推断 / 未验证

**已验证（实测或逐行读码可复算）**

- 【实测】AC-09 第 1 次复跑 = **FAIL**；探针 v2 因 `^\[` 永不匹配而给出 `AC09_VERDICT=UNDETERMINED`（已修 v3：`^\[?`）。
- 【实测】生产 D1 只读回读：PRE `rows_written_24h=69428` / `sync_items` COUNT=40061 ⇒ POST `rows_written_24h=72420`
  （**+2,992**）/ `sync_items` COUNT=40061（**+0**）⇒ 全是既有行 UPDATE、无新增行。
- 【实测】诊断日志三行（08:14:19.339 / 08:29:53.568 / 08:27:58.632）逐字见 §F-1 ⇒ 本轮 94,604 条中
  deferred=54536、divergent=67、待推 40,001 条、被 503 截断在第 3,072 条。
- 【实测】侧车两版比对 HASH_DIFF=3072 / HLC_DIFF=3072；全量对齐 `.state` fullMatch=3072 / noMatch=36,988。
- 【实测】全盘普查 `*.userdb.txt` **只有 1 个**（mtime 2026-11-24 08:14:23，94,610 行）⇒ 导出快照**不是陈旧文件**。
- 【实测】快照 `t=` 只有两个取值（27610 × 94,588、27024 × 16）；`d=` 存在 1.12192e-66 量级连续衰减值。
- 【读码】侧车哈希 = 整条 payload 的 sha256（`CloudSyncService.cpp:1127` / `:1381` / `:1578` / `:1630`）；
  回写经 `restore_user_dict` → `UserDbMerger`（`CloudRimeUserDictBackend.cpp:157-159`），
  merger 重算 `d=`（`librime/src/rime/dict/user_db.cc:208` / `:216`）并改写 `t=`（`:221`）。

**推断（由实测 + 读码推导，未单独复验）**

- 侧车已知条目 40,068 条「本轮全部被判本地改动」= 由「待推集合 40,001 条 + deferred 54,536 + divergent 67」
  与 94,604 总数**反推**得到 ⇒ **未逐条枚举验证**。
- 修复后「单轮写入量降为 0」= 由语义指纹口径 + 回写止血推导 ⇒ **未实施、未实测**。
- 「库存扫完后一次性补推 54,536 条」= 由 deferred 条目数与 R2 闸门口径推导 ⇒ **未实测**。

**未验证 / 未闭环**

- **a-9 代码修复未实施** ⇒ AC-09 仍为 FAIL ⇒ **不得记 PASS**。
- **干净一轮的 AC-09 复跑未跑**：本轮被生产 D1 免费额度耗尽阻塞（503 `服务端存储写入额度已用尽，请稍后重试。`）
  ⇒ 重置 = 北京时间 **2026-11-25 08:00**；已排定定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`
  （北京时间 **2026-11-25 08:10:46**）复跑。
- **「HLC 逐条沿用」的第二层判据仍未跑**：需库存扫完（`scan_complete = 1`，约 41 轮）那一轮再验。
- **服务端 `handlePull` 的 `healed` 自愈 DELETE 仍只能间接证明**（服务端未暴露计数接口，见 §C）。
- **BR-①e / BR-② / Workers Paid** 三项**仍待用户拍板**，本轮未动。

#### §F-5 未闭环 / 下一步

1. **实施 a-9**（代码 + 单测 + 门禁 + 出包装机 + 装机实证）：决策见 ADR 0076 D2-5a-9；
   任务清单见 `.specify/specs/101-cloud-sync-remote-hlc-reuse/tasks.md` 末尾「第二轮修复任务（a-9 语义指纹）」。
2. **复跑 AC-09**：定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（北京时间 2026-11-25 08:10:46），
   探针 v3（`reports/fluxing-br-1c/probe-ac09.ps1`，正则 `^\[?`）⇒ **只有 `AC09_VERDICT=PASS` 才可记 PASS**。
3. **库存扫完后的第二层判据**：约 41 轮后 `scan_complete = 1` 那一轮仍须零写入（见 §C / §F-4）。
4. **待用户拍板（未动）**：Workers Paid 是否升级 / **BR-②**（护栏口径）/ **BR-①e**（新条目上云 / 单轮页数）。
5. **入库存**：ADR 0076 D2-5a-9 修订 + 本 handoff §F + relay prompt 就地改写 + `CLAUDE.md` BR 指针追加
   ⇒ **显式按路径 `git add`**，**禁 `git add .`**，`librime` 禁 add。

---

### §G · a-9 实施轮（2026-11-24）—— 客户端根治修复已落地并装机；**AC-09 真机判据仍未复跑**

**本轮性质** = 客户端 **ADR 0076 D2-5a-9（a-9 语义指纹）** 的**实施 + 单测 + 门禁 + 出包装机 + 装机实证 + 文档回写**。
**本轮改了产品代码**（`WeaselServer/Cloud/CloudSyncService.{h,cpp}` + `test/TestFluxingCloud/TestFluxingCloud.cpp`）⇒ 与 §F 轮（纯取证、零产品代码）性质不同。
**本轮未提交、未 push**（见 §G-7 第 2 项）。

#### §G-1 实施落点（a-9①…a-9⑤ 全部落码）

- `WeaselServer/Cloud/CloudSyncService.h`：新增 3 个公开声明 + 口径注释（`SemanticFingerprint` / `SemanticHashHex` / `IsSemanticHash`）。
- `WeaselServer/Cloud/CloudSyncService.cpp`：
  - 三个 helper 落在 `}  // namespace`（:1622）之后、`BuildSidecarText`（:1668）之前 —— `SemanticFingerprint` **:1625**、`SemanticHashHex` **:1657**、`IsSemanticHash` **:1664**；
  - `SemanticFingerprint` **复用既有导出函数 `SplitDictPayload`**（`fluxing::cloud` 同命名空间、非限定调用可直接解析；`CloudUserDictIo.h` 已 include；实现 `CloudUserDictIo.cpp:326-337` 首个 TAB 切分）⇒ **行为不变**（无 TAB 原样退化；剔除 `d=`/`t=` token、**保留 `c=`**）⇒ **不新增第二套 payload 形态解析器**（宪法 R7 · 单一真相源）；
  - 对账主循环 :1116 起改写：新增 `legacy_baseline` 向量（:1124）、`legacy_rows` / `legacy_held` 计数（:1129-1130）、基线四态 `kNone/kMatch/kDiff/kLegacy`（:1148-1157）、语义指纹取用（:1135-1136）、R1 语义一致即命中（:1163）、旧口径抬高本地 HLC 并计入 `legacy_held`（:1195 / :1212）、**循环后无条件打印 `semantic legacy_rows=<n> legacy_held=<n>`**（:1228-1230）；
  - 推送集合过滤新增 `if (legacy_baseline[i]) continue;`（:1302）；
  - 回写判定（a-9④）:1242-1249 改用 `SemanticFingerprint` 比较 ⇒ `d=` / `t=` 差异**不再触发整库回写**（漂移源头同时断掉）；
  - R3 侧车重建：:1433 改存 `SemanticHashHex`（`s2:<sha256>`）；旧口径行在 :1438-1452 **就地重锚**为 `s2:` + 当前条目 HLC。
- `test/TestFluxingCloud/TestFluxingCloud.cpp`：R1 旧断言 `payload_hash.size()==64` → `67` + `s2:` 前缀；新增 `TestCloudSemanticFingerprint()`（:5088-5217，3 组共 **26** 条新断言 = 指纹性质 8 / **核心回归**（`d=`/`t=` 漂移 ⇒ 零推送零回写）/ 旧口径迁移（压住不推 + 就地重锚））；`main()` 在 `TestCloudRemoteHlcReuse();` 之后调用（:5303-5305）。
- `CloudUserDictIo.{h,cpp}` / `CloudRimeUserDictBackend.cpp` **未改**（复用既有导出函数即可）。

#### §G-2 门禁（父会话亲跑，全绿）

- `weasel.sln` Release|Win32：`SLN_BUILD_EXIT=0`。
- `TestFluxingCloud.vcxproj` **单独**构建（带 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`）：`TESTPROJ_BUILD_EXIT=0`。
- `TestDefaultHotkeys` EXIT=0 / `TestWeaselIPC` EXIT=0 / `TestFluxingCloud` EXIT=0 ⇒ **`RESULT: PASS=813 FAIL=0` / `VERDICT: ALL PASS`**（基线 787 ⇒ **+26**，既有 787 条逐条保留）。
- 改动文件 **U+FFFD = 0**；clang-format 定点：`.h` 漂移 **ERR=0**；测试文件 ERR=261 / UNIQUE=217（其余为既有遗留漂移）但**本轮改动 hunk 内 IN_MINE=0**。
- **⚠ 门禁坑（务必记）**：`weasel.sln` **不含** `TestFluxingCloud` 工程 ⇒ 只跑 sln 会拿**上一轮旧 exe**（PASS 恒 787）⇒ 必须单独 msbuild 该 vcxproj 且**显式传 `SolutionDir`**（否则 `$(SolutionDir)` 未定义 ⇒ `FluxingLocalChangeHook.h` C1083）。
- 门禁首跑 `TestWeaselIPC EXIT=-3`，单独复跑 EXIT=0 ⇒ **偶发，非回归**。

#### §G-3 出包装机 + 装机实证

- 出包：`tmp/br1c-build-installer.cmd` ⇒ `ALL_STEPS_OK` / `NSIS_RC=0` / EXIT=0。
- 新包 `output\archives\fluxing-1.3.8-installer.exe` = **43,001,594 B** / sha256 `1E955184BB1F099D40E6B088ED8D0299398A1B2118DD48D8ECDD6B5FFEBEF569` / sha16 `1E955184BB1F099D` / mtime **2026-11-24 09:25:04**（与上轮 42,981,713 B / `017722F3FD0EC512` 不同 ⇒ **代码确实变更**）。
- 装机 `INSTALL_EXIT=0`。
- **三处 sha256 逐字节相等** = **`A106522FD8F5AC31ABBF392FA906F76ABFD73032A73640E0FE45742057394A94`**（2,810,880 B）：构建 `output\Win32\WeaselServer.exe` = 包内 `tmp\pkg-a9-20261124092826\WeaselServer.exe` = 装机后 `D:\Program Files\fluxing\weasel\WeaselServer.exe`。
- **载荷落在 `<InstallDir>\weasel\`**（不是安装根目录）；旧备份 `WeaselServer.exe.bak*` / `engine-nested-fluxing-bak-20261103` 未动。
- 新客户端已在跑：**PID 609072**，启动 **09:28:17**，`startup_allowed allowed=1 reason=console_session_match`。

#### §G-4 ⚠ 纠正前一结论：真机两轮**不是「零推送」**，而是**拉取阶段 503 早退**

- 【实测】真机诊断日志 `%TEMP%\fluxing-profile-notify.log`（629,000 B，行首**无** `[`）在 **09:28:56.452 / 09:30:02.423 / 09:34:37.107** 共 3 行 `reconcile scan_complete=0 matched=0 deferred=0 divergent=0 pushed=0`，**其间没有任何 `semantic legacy_rows=` 行**。
- 【读码】`reconcile` 行由 `CloudSyncService.cpp:900`（`SyncOnce` 内）**无条件**打印 ⇒ `SyncRound` 早退**也会**有一行全 0；`semantic legacy_rows` 行在 :1228（**对账循环之后**）⇒ **只有真正跑完对账才有** ⇒ 三轮**根本没进对账循环**。
- 【实测 · 失败原因】BK 探针读 1017：`最近同步：2026-11-24 09:30:02 · 失败（服务端存储写入额度已用尽，请稍后重试。）`（09:34:37 同文案）⇒ **生产 D1 免费额度耗尽（`code 7500`）阻塞拉取**。
- 【读码 · 为什么「读」也会 503】`cloud/fluxing-sync/src/auth.ts:1043-1045`：`device_last_seen` 距今 ≥ `LAST_SEEN_REFRESH_SECONDS`（300 s）时执行 `UPDATE devices SET last_seen` ⇒ **认证路径带写** ⇒ 额度耗尽时**拉取（读语义）也 503**。
- 【实测 · D1 读数（09:35）】`rows_written_24h = 79,941`（08:20 时 72,420，**仍在涨**）、`write_queries_24h = 31,872`、`rows_read_24h = 9,360,874`、`read_queries_24h = 2,514`、`database_size = 19,255,296`、`uuid = e4ea114a-29b4-4abd-87e6-4e6cb20d86e4`。
- 【实测 · 用户词典快照】`…\sync\<uid>\rime_ice.userdb.txt` = 4,794,397 B / mtime 2026-11-24 09:29:57。
- ⇒ **先前记的「真机零推送零分歧 ⇒ 修复生效」结论错误，本节予以纠正**；**AC-09 仍记 FAIL**。

#### §G-5 真机侧车现状 + 探针 v3 假绿缺陷（本轮新发现）

- 【实测】真机侧车 `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state` = **40,063 行**（**LEGACY64 = 40,061**、**S2 = 0**、首行 `#fluxing-cloud-sync-v1`）、mtime **08:29:53** ⇒ **真机侧车仍是旧口径**（裸 64 hex、无 `s2:`）⇒ 新客户端**从未成功跑完一轮** ⇒ **a-9③ 旧口径迁移路径尚未被真机证据覆盖**。
- 【实测】另两份副本仍在：`fluxing-cloud-sync.state.failed-ac09`（08:14:19，同尺寸）；`.ac09bak` 不存在。
- 【读码】`last_server_seq_` 随侧车持久化（`LoadSidecar` :810 置 0 / `ParseSidecarText` :821-822 / `SaveSidecar` :834 写出 / :1459-1460 落盘）⇒ **删侧车 ⇒ 拉取游标归零 ⇒ 从 seq 0 全量重扫**（单轮上限 `kPullPageLimit=100` × `kMaxPullPages=10` = 1,000 条 ⇒ 约 41 轮）。
- **⚠ 探针 v3 假绿缺陷**：`round_ran` 只看 `reconcile` 行 ⇒ 早退轮也会被误判 PASS ⇒ **探针已升 v4**（`reports/fluxing-br-1c/probe-ac09.ps1`，**不入库**）：
  1. `round_ran` 必须**同时**见到 `reconcile` 行**且** `semantic legacy_rows=` 行（后者只在跑完对账后打印 ⇒ 硬证据）；
  2. 主判据改 **`sync_items` 表级写入计数**（`updated_at` 秒级，`>= pre 时刻` 计数；时间单位自动探测）⇒ **不把 `devices.last_seen` 那 1 行读路径写算进写放大**；`rows_written_24h` 全局计数改列为**严格变体** `AC09_VERDICT_STRICT`（服务端 DELETE+INSERT 写序 ⇒ 重写 1 条计 2 行写入）；
  3. 记录 `legacy_rows` / `legacy_held` 实测值 + 侧车 `s2:` / 旧口径行数普查（迁移前后）；
  4. 保留 `AC09_VERDICT=` / `AC09_SUMMARY=` 两行输出契约（cron 与人工读法不变）；新增 `STAGE_VERDICT[Legacy|Deleted]` 分段结论。

#### §G-6 已验证 / 推断 / 未验证

**已验证（实测或逐行读码可复算）**

- 门禁数字（§G-2）、出包与三处 sha（§G-3）。
- 真机 3 行 `reconcile` 全 0 且**无** `semantic` 行（§G-4）⇒ 两轮**未进对账循环**（早退）⇒ **「修复已在真机生效」未被证实**。
- BK 探针读 1017 = **失败（服务端存储写入额度已用尽）**（§G-4）。
- 真机侧车 40,061 条**旧口径**、`s2:` = 0（§G-5）。
- D1 读数（§G-4）与 `auth.ts` 读路径写（§G-4）。

**推断（读码 + 实测推导，未单独复验）**

- a-9 修复合规 ⇒ AC-09 场景单轮写入降为 0 ⇒ **未实测**（真机未跑完对账）。
- 旧口径迁移后侧车将以 `s2:` 口径重锚 ⇒ **未实测**。
- 「库存扫完后一次性补推 54,536 条」⇒ **未实测**。

**未验证 / 未闭环**

- **AC-09 真机判据仍为 FAIL（未复跑）** ⇒ 重置 = 北京时间 **2026-11-25 08:00**；定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（**2026-11-25 08:10:46**）用**探针 v4** 复跑。
- **a-9③ 旧口径迁移未被真机覆盖**；**第二层判据**（`scan_complete = 1` 那轮零写入，约 41 轮后）未跑。
- `devices.last_seen` 读路径写对 D1 全局额度的长期影响未量化。
- **BR-①e / BR-② / Workers Paid** 三项仍待用户拍板，本轮未动。

#### §G-7 下一步（顺序固定）

1. **复跑 AC-09**（定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`，北京时间 **2026-11-25 08:10:46**）⇒ 探针 **v4** ⇒ **只有 `AC09_VERDICT=PASS` 才可把 AC-09 改记 PASS**；机器给 `UNDETERMINED` / `FAIL` 时**不得**覆盖硬数据，也**不得**据此记 PASS。
2. **入库存**：显式按路径 `git add`（`WeaselServer/Cloud/CloudSyncService.cpp`、`.h`、`test/TestFluxingCloud/TestFluxingCloud.cpp`、`CHANGELOG.md`、ADR 0076、本 handoff、relay prompt、`CLAUDE.md`、spec 101 tasks）⇒ **禁 `git add .`**、`librime` **禁 add**；Conventional Commits + 模块前缀（客户端 `WeaselServer` / 文档 `docs`）⇒ `git commit -F <file>` ⇒ push（直连曾 `Recv failure: Connection was reset` ⇒ 失败则走默认代理）。
3. **第二层判据**：库存扫完（`scan_complete = 1`）那一轮仍须零写入。
4. **待用户拍板**：Workers Paid 是否升级 / BR-② / BR-①e。

