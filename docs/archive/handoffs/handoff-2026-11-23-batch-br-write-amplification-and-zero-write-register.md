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
