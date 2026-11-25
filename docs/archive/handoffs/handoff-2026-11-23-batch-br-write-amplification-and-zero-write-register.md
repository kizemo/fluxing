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
2. **入库存** ⇒ **2026-11-24 09:51 已完成**（明细见 §G-8）：`2b83cd6`（客户端）+ `feb02ad`（文档）已推送，远程 `kizemo/feat/100-cloud-sync` 与本地同步（`rev-list --left-right --count` = 0/0）。
3. **第二层判据**：库存扫完（`scan_complete = 1`）那一轮仍须零写入。
4. **待用户拍板**：Workers Paid 是否升级 / BR-②（**BR-①e 已于 2026-11-24 拍板 = 维持现状**，见 §G-9）。

#### §G-8 入库与推送记录（2026-11-24 09:51）

- 提交前核对：`git status -sb` + `git diff --stat`（tracked 修改 9 个文件；`librime` 仍为 dirty submodule ⇒ **未 add**）；改动文件 **U+FFFD = 0**（9/9）；换行风格保持原样（`CHANGELOG.md` CRLF、其余 LF）。
- **提交 1** `2b83cd6` `fix(WeaselServer): 云同步写放大根治 —— 语义指纹取代整条 payload 哈希` ⇒ 3 文件 / +301 / −24（`CloudSyncService.cpp` +156/−24、`CloudSyncService.h` +30、`TestFluxingCloud.cpp` +139/−6）。
- **提交 2** `feb02ad` `docs(fluxing): BR-①c 第二轮文档同步 —— 语义指纹实施状态与判据复跑口径` ⇒ 6 文件 / +334 / −3（`tasks.md` +59/−1、`CHANGELOG.md` +7、`CLAUDE.md` +3/−1、ADR 0076 +57、本 handoff +209、relay prompt 改写）。
- **推送**：默认链路（走代理）**失败** —— `fatal: unable to access 'https://github.com/kizemo/fluxing.git/': TLS connect error: error:0A000126:SSL routines::unexpected eof while reading`（EXIT=128）⇒ 改走**直连** `git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync` ⇒ **PUSH_EXIT=0**（`65c9669..feb02ad feat/100-cloud-sync -> feat/100-cloud-sync`）。⇒ **本轮链路与上一轮相反：代理断、直连通**（两条路都要试）。
- 收尾状态：`git status -sb` ⇒ 分支与远程同步（0/0）；工作树只剩 `m librime`（dirty submodule）+ 既有 untracked（`release/`、`reports/**`、`scripts/verify/*.ps1`、`docs/design/quickpanel-v6/design-demos/*.html`）⇒ **按红线一律不入库**。
- **定时任务已同步**：`4d3e51f8-fdda-41bb-bbc1-12e844914dc5` 的 prompt 已就地改写（新增：新客户端已装机 + 提交哈希、⚠ 上一轮「真机零推送」判读纠正、⚠ 读路径也会 503 的机理、探针 v4 双 Stage 与 `AC09_VERDICT_STRICT` 口径），复核 `enabled=true` / `status=active` / `runAtMs=1795565446336`（北京时间 **2026-11-25 08:10:46**）。
- **§G-7 第 2 步（入库存）到此关闭**；剩余仅 ① AC-09 复跑 ② 第二层判据 ③ 待用户拍板两项（BR-①e 已拍板，见 §G-9）。

#### §G-9 用户拍板记录（2026-11-24 11:08）

- **BR-①e = 维持现状**（用户明确选择「维持现状，等库存扫完」）⇒ **本议题关闭、不另立 ADR**：
  - 库存扫完（`scan_complete = 1`，约 41 轮）前**不推送新条目**（R2「库存未证实不推送」闸门**保持收紧**）；
  - **不提高**单轮拉取页数（维持 `kPullPageLimit = 100` × `kMaxPullPages = 10` = 1,000 条/轮）；
  - 54,536 条从未上云条目**不做 pacing**（本批**不实现**推送 cap / 限速）⇒ 待库存扫完那轮自然发生一次性补推；
  - 代价（已知并接受）：**真实本地改动的上云被延后约 41 轮**（不丢数据，只延迟）。
- **仍待用户拍板两项**（本轮未动）：Workers Paid 是否升级（维持不升级）/ **BR-②**（写预算护栏是否改全局口径或按活跃账号数分摊）。
- ⚠ **BR-①e 关闭后的连带风险（未闭环）**：库存扫完那轮一次性补推 54,536 条 ⇒ 服务端每账号每日写预算 **80,000 行** 与 D1 全局免费额度 **10 万行/日** 两个上限**在同一时刻逼近** ⇒ 可能再次触发 D1 额度耗尽 ⇒ 连带「登录 / 拉取也 503」（`auth.ts` 的 `devices.last_seen` 属读路径写）。**当前无缓解手段**：本批不做 pacing、Workers Paid 未升级 ⇒ 该窗口内额度告急只能等 UTC 日重置。⇒ **建议在库存扫完之前（即 AC-09 第二层判据那一轮之前）复核 BR-② 的口径**，否则补推窗口可能撞额度。

#### §G-10 Cloudflare 免费额度可行性评估（2026-11-24 11:50）

**官方额度（Cloudflare 官方定价页，2026-11-24 核实）**

| 项目 | Workers Free | Workers Paid（$5/月） |
|---|---|---|
| 写入行数 | **100,000 行/日**（账号级全局池，超限硬失败 `code 7500`，不计费不降级） | 首 **5,000 万行/月** 含 + $1.00/百万行 |
| 读取行数 | 500 万行/日 | 首 250 亿行/月 含 + $0.001/百万行 |
| 存储 | 5 GB（账号合计） | 首 5 GB 含 + $0.75/GB-月 |

**结论：不是「不可行」，但「免费」撑不住规模化首导。**

1. **烧掉额度的那次不是「同步一次词库」这个动作本身，而是写放大 bug** —— 根因 = 侧车哈希取整条 payload sha256 + rime 回写重算 `d=`/`t=` ⇒ 每轮把 4 万条全量重写（实测 +2,992 行/轮，`sync_items` COUNT 不变）。已由 ADR 0076 **D2-5a-9 语义指纹**根治 ⇒ **修复后稳态每轮写入趋近 0**（AC-09 待 2026-11-25 08:10 复跑实证）。
2. **但免费额度在本架构下仍然脆**，四条硬约束：
   - **一次性首导 54,536 条** ≈ 5.5 万行 = 免费日额度 **55%**；若 `item_key UNIQUE` 的索引行也计写入（官方：索引写额外计 1 行）⇒ 约 11 万行 = **110%** ⇒ **单用户一次首导即可打爆全球池**。
   - **额度是账号级全局池 + 硬失败** ⇒ 任一用户突发 ⇒ **其他所有用户登录/拉取一起 503**（`auth.ts:1043-1045` 的 `devices.last_seen` 属**读路径写** ⇒ 额度耗尽时连读语义一起被拒）。
   - **`devices.last_seen` 写地板** = `LAST_SEEN_REFRESH_SECONDS = 300` ⇒ 每设备 288 行/日 ⇒ 10 万行 ≈ 347 设备/日；且该写在读路径 ⇒ 写失败直接阻断拉取。
   - **读侧已超文档额度**：实测 `rows_read_24h = 9,360,874` > 免费 500 万行/日 ⇒ 主因 `sync_items` 4 万行表**无 `updated_at` 索引** ⇒ 写预算护栏 / 拉取自愈检查**全表扫描**（≈234 次全扫 ≈ 9.3M 行）⇒ 读侧随时可能硬失败。
3. **分层判定**：
   - 自用 / 少量用户（≤10）+ 已建侧车不再首导 + 修复后 ⇒ **免费可行**（稳态 ≈ 0 写，只剩 `last_seen` 地板）。
   - 公开注册 / 上百用户 / 每人数万词条 / 频繁首导重导 ⇒ **免费不可行**（不是「优化一下」，是「按行计费」与「词条级增量」模型不匹配）。
4. **三条路径（待用户拍板）**：
   - **A 架构解**：D1 从「逐条索引」降为「用户级元数据」（每用户每次同步 1 行：blob key + hlc + size），词条包落 R2（免费 10 GB）⇒ 写入量 O(用户数) 而非 O(词条数) ⇒ 免费额度可撑上百用户。代价：改 push/pull 协议 + 客户端侧车改块级指纹 ≈ 一个新批次。
   - **B 付费解**：升级 Workers Paid（$5/月 ≈ ¥36/月）⇒ 写入 5,000 万行/月（含）⇒ 首导 5.5 万行 = 月额度 **0.11%**；并彻底消除「额度耗尽 ⇒ 登录/拉取 503」这类生产事故级风险。**当前性价比最高的一步。**
   - **C 过渡解**：维持现状 + 分日 pacing（5.5 万条摊到 3 天）⇒ 免费额度勉强够，但期间挤占其他用户额度，且 `last_seen` 地板与读侧超额仍在。
5. **无论选哪条都建议先做的两个止血项**（各几行改动、零成本、可逆）：
   - 给 `sync_items.updated_at` 建索引（消掉 4 万行全表扫描 ⇒ 读侧从 9.36M/天 降到几百/天）；
   - `devices.last_seen` 刷新间隔 300 s → 1 h（288 → 24 行/设备/天），或改为「与其它写同批提交」，把它从**读路径**上摘掉。
6. **官方来源**：Cloudflare Workers/D1 定价页（https://developers.cloudflare.com/workers/platform/pricing ）—— 免费 10 万行写入/日、付费 5,000 万行/月含 + $1.00/百万行；「索引写额外计 1 行写入」。

---

### §H · BR-①f 服务端桶化轮（2026-11-24 12:40）—— 服务端已实施 + 门禁 + 入库推送；**迁移与部署被 D1 免费「读」额度阻塞 ⇒ 生产尚无桶级端点，BR-①g 客户端无法联调**

**本轮性质** = ADR 0077「路径 A 架构解」的**服务端落地**（spec 102 三件套 + `migrations/0004_sync_buckets.sql` + `src/sync-buckets.ts` + wire / env / index / account + 单测）⇒ **改了服务端产品代码**（`cloud/fluxing-sync/**`），**未改任何客户端代码**、**未 apply 迁移**、**未部署**。
**用户拍板（2026-11-24）** = 「先零成本止血，再做 A 服务端改架构（D1 降级 + 词条包落 R2）」⇒ 止血项与 ADR 0077 本体由前轮完成（`2c46013` / `506d32b`，均已推送）。

#### §H-1 本轮入库范围（commit `a7c61e0`，`git show --numstat` 实测）

| 文件 | 增 / 删 | 备注 |
|---|---|---|
| `.specify/specs/102-bucket-level-sync-storage/spec.md` | +157 / −0 | 18,125 B |
| `.specify/specs/102-bucket-level-sync-storage/plan.md` | +471 / −0 | **41,148 B** |
| `.specify/specs/102-bucket-level-sync-storage/tasks.md` | +367 / −0 | **32,216 B** |
| `cloud/fluxing-sync/migrations/0004_sync_buckets.sql` | +52 / −0 | 建表 + **三条**索引 |
| `cloud/fluxing-sync/src/sync-buckets.ts` | +407 / −0 | `handleBucketPush` / `handleBucketPull` |
| `cloud/fluxing-sync/src/wire.ts` | +127 / −3 | 桶入参契约 |
| `cloud/fluxing-sync/src/env.ts` | +49 / −0 | 桶级写预算配置 |
| `cloud/fluxing-sync/src/account.ts` | +36 / −10 | 注销账号连带清桶 |
| `cloud/fluxing-sync/src/index.ts` | +16 / −0 | 两条新路由 |
| `cloud/fluxing-sync/test/bucket-sync.test.ts` | +818 / −0 | **13 用例 / 172 断言** |
| **合计** | **+2,500 / −13** | 10 files changed |

- Commit **`a7c61e0`**（full `a7c61e0aedae81257bc32f6edbe10c4a25035969`）`feat(fluxing): 云同步桶级端点 —— sync_buckets 设备×桶双维索引 + R2 词条包（ADR 0077）`。
- ⚠ **数字校正（以本表为准，覆盖开工指令的口径）**：`plan.md` 实测 **41,148 B**（指令写 36,344）、`tasks.md` **32,216 B**（指令写 31,088）、新单测 **13 用例**（指令写 14 ⇒ vitest 总数 164 = 基线 151 + 13 可反证）、逐文件增删行以 numstat 为准（`wire.ts` +127/−3 而非 +93、`env.ts` +49 而非 +11、`index.ts` +16 而非 +14、`account.ts` +36/−10 而非 +19）。

#### §H-2 迁移 0004 实测内容（`cloud/fluxing-sync/migrations/0004_sync_buckets.sql`）

- **L30 `CREATE TABLE sync_buckets (...)`** —— **没有 `IF NOT EXISTS`**（与开工指令描述不同 ⇒ 重复 apply 会报错，重跑前须先查 `d1_migrations` 记录）。
- 列序逐字对齐 ADR 0077 D1（`docs/adr/0077-...md:64-67`）：`id INTEGER PRIMARY KEY AUTOINCREMENT` / `user_id` / `device_id` / `bucket_key` / `hlc_ms` / `hlc_counter` / `hlc_device` / `r2_key` / `cipher_len` / `item_count` / `updated_at`。
- **三条**索引（开工指令只说两条）：L47 `CREATE UNIQUE INDEX idx_sync_buckets_user_device_item ON sync_buckets(user_id, device_id, bucket_key)`（I3 落点）、L50 `idx_sync_buckets_user_id (user_id, id)`（增量拉）、L52 `idx_sync_buckets_user_updated (user_id, updated_at)`（写预算 COUNT）。
- 文件头注释 **B1–B6** 把「与 `sync_items` 的六处刻意差异」写进迁移本体：B1 多 `device_id` 且存 **`devices.id` 行主键**（口径引 `0001_init.sql:46-47`）、B2 **无 `nonce_b64` 列**（D3 不做桶级二次加密）、B3 多 `item_count`、B4 `cipher_len` 语义改为「桶 blob 字节数」、**B5 桶侧 `id` 是独立编号空间 ⇒ `server_seq` 禁止跨协议版本复用游标 ⇒ 客户端侧车必须按版本分区**、B6 额外 `(user_id, updated_at)` 索引。

#### §H-3 实现落点（关键 file:line）

- **推送** `src/sync-buckets.ts:150-322`：邮箱未验证先 403（:156-158）→ `resolvePushDeviceId`（:160）→ 桶数组上界（:163-170）→「**先整体校验，任一个非法就什么都不写**」（:172-173）→ 取同设备同桶既有行作 HLC 基准（:177-191）→ 逐桶守卫：服务端已存 ≥ 客户端 ⇒ **整桶 `skipped`**（:204-214）→ **写预算判定（:216-232，在任何写动作之前）** → R2 写（:234-258，并发度沿用 ADR 0074 的 6）→ D1 `DELETE + INSERT`（:260-292）→ `server_seq =` 本次**真正写入**桶行的最大 id（:294-315；全部跳过时取账号级 `MAX(id)` 防游标倒退）。
- **拉取** `src/sync-buckets.ts:335-407`：`after = max(since, cursor)`（:344）→ `WHERE user_id = ? AND id > ? ORDER BY id ASC LIMIT limit+1`（:346-356）→ `has_more` 口径同逐条端点（:359-360）→ **单行 R2 对象缺失 = 跳过 + 批量删该行 + `console.warn` 自愈**（:366-372、:390-397）⇒ **不返 500**（抛错会让客户端卡在同一游标无限重试）→ 顶层三字段同名同义（:400-406）。
- **`device_id` 服务端权威** `src/sync-buckets.ts:110-123`（设计说明与理由 :100-108）：一律取 access token 反查出的 `auth.deviceRowId`；客户端自带值**只允许相等**，否则 400 `bad_device_id`（:113-121）⇒ 防「伪造 device_id 让 A 设备整桶压制 B 设备行（永不收敛）」。ADR 0077 D7 的响应示例（`docs/adr/0077-...md:131`）只写 `"device_id": "<hex>"`、**未规定来源** ⇒ 本批定为服务端权威（与 `refresh_tokens.device_id` 先例同构）。
- **路由** `src/index.ts:117` `POST /v1/sync/buckets/push`、`:123` `GET /v1/sync/buckets/pull`；老 `/v1/sync/push|pull` 逐字未动（§H-6 的「老逐条端点回归」describe 守着）。
- **配置** `src/env.ts:39` `WRITE_BUDGET_BUCKETS_PER_DAY?`、`:92` 默认 **4_000**（合法区间 :95-96 = 1..10,000,000，越界回落默认 :107/:115）。⚠ `cloud/fluxing-sync/wrangler.toml` **未声明**该变量（只有 `WRITE_BUDGET_ROWS_PER_DAY = "80000"`，toml L60）⇒ 生产当前取代码默认 **4,000 桶行/账号/UTC 日**。
- **注销账号** `src/account.ts:86` 清 R2 前缀 `b/<user_id>/`、`:94` `DELETE FROM sync_buckets WHERE user_id = ?`、`:103` 响应新增 `deleted_bucket_rows`；文件头 :10-12 已写明「**部署前必须先应用 0004 迁移，否则本接口报错**」。

#### §H-4 不变量 I1–I5（逐行复核通过）

- **blob 格式** = 既有条目级信封文本块**直接串接**（与 `/v1/user-dict/export` 文件体同构）⇒ **条目级 AAD 逐字不变**、**不做桶级二次 AEAD**（ADR 0077 D3；迁移头 B2 同记「无 nonce_b64」）。
- **I1 服务端零明文**：pull 原样回吐 `cipher`（:383-384），服务端不解析 blob（`item_count` 只是客户端申报值 + 运维指标）。
- **I2 `device_id` 服务端权威**：`sync-buckets.ts:110-123`（见 §H-3）。
- **I3 `(user_id, device_id, bucket_key)` 三元键 UPDATE 整行替换**：DELETE（:267-269）+ INSERT（:270-290），R2 键含设备维度（`bucketR2KeyFor(auth.userId, deviceId, bucket.bucketKey)`，:238）。
- **I4 写预算判定在任何写动作之前**：:216-232 先于 :234 的 R2 put 与 :292 的 D1 batch ⇒ 超限 **429 + D1/R2 零写入**，错误面复用 `WRITE_BUDGET_EXCEEDED_CODE` / `_MESSAGE`（:231）⇒ **不新增错误码，客户端无需新分支**。
- **I5 pull 只返 `id > max(since, cursor)`**、按 id 升序、`has_more` 同口径、单行 R2 缺失自愈不返 500（:366-372）。

#### §H-5 `MAX_BUCKET_ITEMS` 的**双重身份**（登记清楚，别当缺陷）

- 「桶内**实际**条目数 ≤ 4096」**无法从密文域校验** ⇒ 服务端只见密文、解不开；硬判实际条数就得解密 ⇒ **与 I1 矛盾** ⇒ 因此它是 **wire 契约上限 + 客户端侧护栏**。
- 服务端**可执行**的两条：① 客户端**申报**的 `item_count` 上界 400 硬拦（`src/wire.ts:291-299`，文案「…超出上限 4096（实收 N），该桶应降级走逐条推送。」）；② blob **字节**上限 `MAX_BUCKET_BLOB_BYTES = 4 * 1024 * 1024`（`src/wire.ts:256`）经 `fromBase64Strict(raw.cipher, MAX_BUCKET_BLOB_BYTES)`（**`src/wire.ts:301`** ⇒ 注意在 **wire.ts**，不是 sync-buckets.ts）执行，非法 base64 或超限在批量校验阶段 400 并回显上限（:302-306）。
- 常量位置：`MAX_BUCKET_ITEMS = 4096`（`src/wire.ts:246`）、`MAX_PUSH_BUCKETS = 64`（:243，含义 = 单次 64 个 R2 put，与逐条端点 `MAX_PUSH_ITEMS = 64`（:33）**同值不同义**）。
- base64 解码**逐字复用既有 `fromBase64Strict`** ⇒ 未新增第二套编解码器（宪法 R7 单一真相源）。

#### §H-6 门禁（父会话独立复跑，**未采信子代理自报**）

- `npm --prefix cloud\fluxing-sync run typecheck` ⇒ **`TYPECHECK_EXIT=0`**。
- `npm --prefix cloud\fluxing-sync run check`（含 `check:endpoint`）⇒ **`CHECK_EXIT=0`**：**Test Files 13 passed (13) / Tests 164 passed (164)**，Start **2026-11-24 12:33:48**，Duration **13.05s**；`check:endpoint` 的 T103 红线通过（目录内无默认子域字面量，workers_dev / preview_urls 均关闭）。
- 基线 12 文件 / 151 用例 ⇒ **+1 文件 / +13 用例**，既有 151 条无一走红。
- 日志 **`reports/fluxing-br-1f/gate-1.log`**（1,450 B，**不入库**）。

#### §H-7 ⚠ 部署未完成 —— 新发现的**第二个生产级阻塞：免费额度连「读」也挡**

- `npx wrangler d1 migrations apply DB --remote` ⇒ **`MIGRATE_EXIT=1`**，错误原文：
  > `Your account has exceeded D1's free tier daily row read limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue. See https://developers.cloudflare.com/d1/platform/limits/ for more details. [code: 7500]`

  （API 路径 `/accounts/016c8e736b685af88860034ea6eb1daa/d1/database/e4ea114a-29b4-4abd-87e6-4e6cb20d86e4/query` 失败；wrangler 4.159.0；日志 **`reports/fluxing-br-1f/deploy-1.log`**，797 B，不入库）
- **推翻本轮开工时的判断**：此前认为「migration 是 DDL、只花几行 ⇒ 当天可 apply」⇒ **错**。wrangler 的 `migrations apply` 内部走 D1 **`/query` 通道** ⇒ 被**读额度**直接拒（免费 **5,000,000 行/日**）。
- 【实测 · 额度】12:26 `rows_read_24h = 9,083,789` → 12:38 `9,083,795`；**本轮收尾（12:52）`wrangler d1 info DB --json` 复测 = `rows_read_24h = 9,083,801`（仍 > 5M ⇒ 读额度未恢复）、`rows_written_24h = 33,043`**（12:38 曾为 72,420 ⇒ 写计数随 24 小时滚动窗口已回落 ⇒ **当前瓶颈是读、不是写**）、`database_size = 19,255,296`。⇒ **「等 UTC 午夜写额度重置」不足以保证迁移能跑**；前置判据要改看 **`rows_read_24h < 5,000,000`**。
- 【实测 · 生产现状】`npx wrangler deployments status` ⇒ 当前生效 Version = **`5f3732e8-f1d2-4829-a8dd-1c5b97a90039`**（Created 2026-11-24T04:04:12.850Z = 止血版，**无桶级端点**）。
- 【实测 · 迁移】`migrations/0003`（`idx_sync_user_updated`）与 `0004`（`sync_buckets`）**均未 apply 到生产** ⇒ 读侧止血的收益也还没到账 ⇒ **客户端桶化（BR-①g）现在无法联调**。

#### §H-8 硬前置：`migrations apply` 必须**先于** `deploy`（顺序不可颠倒）

`src/account.ts:94` 的注销账号处理**无条件** `DELETE FROM sync_buckets` ⇒ 若 Worker 先上线而表不存在，**「注销账号数据」对所有用户直接报错**（生产回归）。⇒ 顺序固定：① 确认 `rows_read_24h < 5,000,000` → ② `npx wrangler d1 migrations apply DB --remote`（期望 `MIGRATE_EXIT=0`）→ ③ 复测迁移 EXPLAIN → ④ `npm --prefix cloud\fluxing-sync run deploy` → ⑤ 生产冒烟。**任何情况下不得跳过 ② 直接 ④。**

#### §H-9 已验证 / 推断 / 未验证

**已验证（本轮亲测，可复算）**

- §H-1…§H-6 全部数字与行号（`git show --numstat` / Select-String / 逐行读码）；commit `a7c61e0` 已推送（`PUSH_EXIT=0`、`506d32b..a7c61e0`、`rev-list --count` = 0）。
- 生产 Version `5f3732e8-f1d2-4829-a8dd-1c5b97a90039`（`wrangler deployments status` EXIT=0）与 D1 读数（`wrangler d1 info DB --json` EXIT=0）。
- 迁移 apply 失败与错误原文（`deploy-1.log`）。

**推断（未实测）**

- 桶化后「单轮 D1 写入量与词条数解耦」⇒ 生产从未跑过桶端点，**零实证**（ADR 0077 D9 的估算）。
- 4,000 桶行/日的护栏是否够用 ⇒ 未实测（且 `wrangler.toml` 未显式声明该变量）。

**未验证 / 未闭环**

- 迁移未 apply、Worker 未部署 ⇒ **桶端点在生产上不存在**；生产冒烟未做。
- **AC-09 仍记 FAIL**（客户端语义指纹版，本轮未复跑）⇒ 定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（`scheduleType=once`、`runAtMs=1795565446336` = 北京时间 **2026-11-25 08:10:46**）用探针 **v4** 复跑，**只有 `AC09_VERDICT=PASS` 才可改记 PASS**。
- healed 自愈「丢一整桶条目」的粒度风险（ADR 0077 D10）本批**不解决**，登记为 BR-①g 已知边界。
- ADR 0077 的 **Status 仍为 Proposed**（`docs/adr/0077-...md:8`）、「生产复验结果」小节仍空（:230）⇒ 本批红线不动 `docs/adr/`，待服务端生产可用后另轮回写。
- **BR-①g 客户端桶化未开工**。

#### §H-10 下一轮（顺序固定）

1. **AC-09 定时复跑**（任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`，北京时间 **2026-11-25 08:10:46**）⇒ 探针 **v4** `reports/fluxing-br-1c/probe-ac09.ps1` ⇒ **只有日志出现 `AC09_VERDICT=PASS` 才可把 AC-09 改记 PASS**；`UNDETERMINED` / `FAIL` 不得覆盖硬数据。
2. **先看 `rows_read_24h` 是否 < 5,000,000**，再 `npx wrangler d1 migrations apply DB --remote`（0003 + 0004，期望 **`MIGRATE_EXIT=0`**）。
3. **复测迁移 EXPLAIN**：期望写预算护栏与增量拉取从全表扫描改走 **`idx_sync_user_updated`** / 桶侧 `idx_sync_buckets_user_id`、`idx_sync_buckets_user_updated`。
4. **之后**才 `npm --prefix cloud\fluxing-sync run deploy`（新 Version id 回写 §H-7）。
5. **生产冒烟**：新账号一对设备 → 走桶端点 push 1 桶 → pull → 断言 `skipped` / `has_more` / 游标推进。
6. 再开工 **BR-①g 客户端桶化**：分桶打包 / 拆包、桶级 HLC 分配、**侧车 v2 升版 + 强制 `seq = 0`**（§H-2 的 B5 独立编号空间约束）、第二道条目数护栏（服务端不可判 ⇒ 客户端必判）、出包装机 + 真机判据。
7. **待用户拍板**：Workers Paid 是否升级 / BR-②（护栏改全局口径或按活跃账号数分摊）/ **新增一项：D1 读额度已耗尽 ⇒ 老逐条端点的读放大是否也要止血**。

#### §H-11 本轮工具坑（新增，勿重试）

- `git show HEAD:<path> | Out-String` **不能**用来判文件尾部字节（管道会规范化行尾）⇒ 用 `[IO.File]::ReadAllBytes` 或先落文件再测。
- 判「缺尾部换行要不要修」= 先看包内有无 lint/format 门禁：`cloud/fluxing-sync/package.json` scripts 只有 `test` / `check` / `check:endpoint` / `typecheck` ⇒ **无格式门禁**，且未改动的 `src/sync.ts` 同样缺尾换行（仓库既有状态）⇒ **不制造无谓 diff**。
- 前台 bash 里 `Start-Sleep` 超过 60 s 会被自动转后台并触发额度闸门 ⇒ **等待必须与有用检查合并成一条命令**。
- **wrangler `migrations apply` 在 D1 读额度耗尽时同样被拒**（`[code: 7500]`）⇒ 部署前置判据要先看 `rows_read_24h < 5,000,000`，**不能只盯 `rows_written_24h`**。
- `wrangler deployments list` 从**最旧**开始输出 ⇒ 查「当前生效 Version」要用 `wrangler deployments status`（本次据此核到 `5f3732e8`）。
- **（下一轮勘查轮补记）同一命令的输出顺序坑同样适用于 `d1 migrations list`** —— 迁移按**版本号升序**输出，判断「哪几条未 apply」要看**末尾**而不是开头。
- **本轮未改任何产品代码**（仅文档回写 + 勘查报告落盘）⇒ **门禁沿用 §H-6**（`typecheck` EXIT=0 / `check` EXIT=0 = 13 文件 / 164 用例）。

---

#### §I · 本轮（2026-11-24 13:30–15:10 · 腾讯云现机只读勘查 + `E:\storedata-upload` 盘查 + 服务器成本评估（含归因纠正））

**本轮性质** = ① 只读盘查 `E:\storedata-upload` 是否存有腾讯云连接资料；② 经用户授权后用现成 SSH 通道对腾讯云现机 `124.223.115.201` 做**只读勘查**；③ 回答「Fluxing 同步服务部署到腾讯云要多少钱」，并**纠正 §G-10 里被我写错的一条归因**。**本轮未改任何产品代码、未 apply 迁移、未部署、未在生产写入任何东西。**

##### §I-1 · `E:\storedata-upload` 盘查结论（严格只读，仓库文件零改动）

| 项 | 实测 |
|---|---|
| 本机 SSH 配置真相源 | `C:\Users\Duanyi\.ssh\config`（**仓库之外**，非项目文件）：`Host 124.223.115.201` / `User ubuntu` / `IdentityFile E:\Users\Duanyi\.ssh\id_ed25519` / `IdentitiesOnly yes` / `StrictHostKeyChecking accept-new`；`known_hosts` 已含该 IP（3 条：ed25519/rsa/ecdsa） |
| 项目内的云凭据 | `.env`（**untracked**）仅 PostgreSQL 连接五项：`PG_HOST_PROD=124.223.115.201` / `PG_PORT_PROD=5432` / `PG_USER_PROD=kizemo` / `PG_PASSWORD_PROD`（len=19、sha16 `11786780520D44E5`）/ `PG_DB_PROD=pgsql` |
| 腾讯云控制台凭据 | **全仓零命中**（无 `SecretId` / `SecretKey` / `AKID`、无 `.pem` / `.ppk`）⇒ 只能走 SSH，**无法用 API 买/建/改云资源** |
| 已有脱敏机制（本项目未做） | `scripts/scan_plaintext_secrets.py` + `scripts/_precommit_secret_scan.sh` + `tests/test_scan_plaintext_secrets.py`，覆盖 `AKID` / `SecretId\|SecretKey` / `sshpass -p` / `-----BEGIN [A-Z ]*PRIVATE KEY-----` ⇒ **GitHub 上的 `SKILL.md`（tracked）不含明文密钥** |
| 真缺陷 A（应修） | **`.env` 与 `~/.ssh/` 都不在 git 仓库里** ⇒ 重装系统 / 换机即丢口令与 SSH 通道；项目自己的 `docs/handoffs/handoff-traffic-source-restructure-2026-11-03.md:100` 原话「**`.env` 不在 git 里，不能当作长期真相源**」 |
| 真缺陷 B（应修） | `docs/env.example.md:5,8`（**tracked**）含两条**生产网明文口令**（均 len=16，sha16 `02428A51348B0F65` / `9605102F924C526D`），且**与 `.env` 里那条口令的哈希都对不上**（第三个值）⇒ 建议改为 1Password 引用并清洗历史 |

##### §I-2 · 腾讯云现机勘查结论（VM-0-3-ubuntu，只读）

**一句话结论 = 可以承载，但不建议现在切。**

**已具备**：nginx 1.18.0（已跑 WordPress `jury.xin`，443 + certbot 自动续期 `certbot.timer` active）；Docker 29.1.3（跑 6 容器）；Node v22.23.2；**Cloudflare Tunnel 正在该机运行**（`cloudflared.service` enabled + `Restart=always`，`/etc/cloudflared/config.yml` 已有 ingress 与 `*.aiec.fun → http://localhost:8080` 一条 ⇒ 印证 §G-10 的「隧道绑在腾讯云这台机上」）；`sudo -n` 免密可用；psql 客户端 14.24；Python3 + 一堆 ETL 库；certbot / git / docker 齐；**磁盘剩 14 G**、内存剩 4.6 G、4 vCPU、负载有余量。

**真实缺口（不是钱，是工程量）**：① **无 Postgres server**（`postgresql.service` not-found、无 `postgresql.conf`、无监听 5432 的服务进程）⇒ 需自建并维护 PG + 备份 + 迁移 + 连接池；② **R2 无现成替代**（该机只有本地磁盘与 MySQL）⇒ 密文包存储层要重写；③ **Worker 代码强绑 D1 + R2**（`env.DB.batch` / `env.BUCKET.put/get`）⇒ 自建 = **重写存储层**，不是「换台机」。

**新增反证（勘查发现的风险）**：① 磁盘 `89% 已用（仅剩 14 G）` ⇒ 装 R2 替代品 + 数据库前**必须先清理**（`/var` 57 G 是大头）；② 该机已在跑 BI 生产栈（sales-bi-core / Grafana / Prometheus / Loki / 宝塔 / WordPress），**Fluxing 同步与 BI 同机 = 故障与负载耦合**；③ `/etc/cloudflared/config.yml` **权限 0666（全局可写）** ⇒ 该机任意本地用户可改 Tunnel 路由，**安全缺陷**（与本项目无关，但同机部署会被牵连）；④ `sales-bi-update.service` **failed** ⇒ 该机已有带病服务；⑤ 5432 / 3306 / 9100 **公网监听** ⇒ 腾讯云安全组未证实（本机无控制台凭据）；⑥ 境内域名 `*.aiec.fun` 的 ICP 备案主体归属未证实 ⇒ **切自建前必须先查备案**。

**本轮未取到（4 项，如实登记，不得据此立项开工）**：PostgreSQL server 版本与 `postgresql.conf` 位置；实例带宽 / 计费方式 / 机型代号；`*.aiec.fun` 的 ICP 备案状态；该机出站 443 实际可用性。

**成本回答口径（本轮据此立论）**：免费档（轻量 2C2G4M 首年 **¥40–48**）可跑通，但「**自建 = 用固定月费换回全部运维责任**」。生产建议 2 核 4 G / 50 G SSD / 6–8 M：**¥288–576/年**；带独立 PG：**¥636–912/年**；若用腾讯云自有域名做**境内 HTTPS 对外服务，ICP 备案 + 等保是硬前置**。完整估算表见 `docs/research/fluxing-cloud-2026-11/tencent-cloud-ecs-recon.md`。

**建议路线（待用户拍板）**：**先零成本止血（已在 §G-10/§H 落地）→ 做 A 桶化 → 客户端 BR-①g → 协议冻结后再评估「是否把已冻结的服务搬到自建」**。「复用现机自建」列为候选 **BR-⑤**，排在 BR-①g / BR-①e 之后。

##### §I-3 · 已验证 / 推断 / 未验证（本轮新增）

| 状态 | 内容 |
|---|---|
| **已验证** | SSH 免密通道可用（`BatchMode=yes` 两轮均返回真实输出，含 `hostnamectl` / `df` / `docker ps` / `ss -lntp` / `systemctl`）；`E:\storedata-upload` 盘查 5 项表实（§I-1）；远端 nginx 与 cloudflared 配置内容（grep 原文取证） |
| **推断** | 「同机部署可行」= 从 nginx 已跑 WordPress + Docker 已在跑推断；「磁盘需先清理」= 从 `89% 已用` 推断 |
| **未验证** | §I-2 列的 4 项缺证；自建后的真实延迟与稳定性；腾讯云安全组端口暴露面；该机出站 443 |
| **已否定** | 上一轮「复用现机几乎零边际成本 ⇒ 自建划算」的说法 ⇒ 真实成本是**存储层重写**，不是「换台机」 |

##### §I-4 · `GITHUB_TOKEN` 进日志的根因、清理结果与红线落点（2026-11-24 追加）

**用户决定**：**暂不轮换**，改为「**清理日志 + 在项目文档设红线**」。

**根因（已坐实）**：令牌不是被 `git push -v` 打出来的，而是被**我自己写的诊断脚本整块 dump 环境变量**带出来的 —— 取证脚本里的 `=== GIT ENV ===` 段 `Get-ChildItem env:` 把全部环境变量（含密钥）写进输出，该输出又被写进会话历史。旁证 = 该段同时 dump 出的腾讯云 PostgreSQL 口令，在 `E:\storedata-upload\docs\memory\feedback_no_plaintext_secrets.md` 里正是被明令禁止的行为（「**NEVER dump env vars**」），该会话当年踩过同一个坑并立了自己的规矩，本项目这边却没有对应红线。

**清理结果（实测）**：

| 落点 | 结果 |
|---|---|
| 会话快照（`snapshots\...\g13\...\weasel\CLAUDE.md` 第 90 行 `github_token=...`，被压缩快照逐代复制） | **已掩码**（`[REDACTED-GITHUB-TOKEN]`，size 561595→561578、`hits=0 marks=1`，UTF-8 无 BOM / LF 不变、**不留 `.bak`**） |
| 运行时 `v2\sessions` 全树 jsonl | **5,082 个文件 / 18 处命中已就地掩码**（`write_locked=0 read_failed=0`）⇒ **形态零残留** |
| 运行时 `v2\sessions` 全树 `reports\tool-outputs\*.txt` | **补掩 4 处**（`POST_MASK_OCCURRENCES=4`）⇒ `rg -c` 整树复核 `VERIFY_EXIT=1`（**零命中**） |
| 项目 `reports/` 与 `tmp/` | 扫描后**零明文命中**（`tmp/*` 命中的是变量名字面量，不是令牌）⇒ **项目产物无需清洗** |
| PSReadLine 历史（`ConsoleHost_history.txt` 1,415 B / 18 行） | **干净** |
| `%APPDATA%\GitHub CLI\hosts.yml` / `~\.config\gh\hosts.yml` / `cmdkey` / `git credential fill` / `logs\mavis.log` / `log\mavis.log` | 全部 MISSING 或空 ⇒ **无需处理** |

**残留待用户授权（本轮不动）**：**`GITHUB_TOKEN` 仍作为环境变量常驻**（实测 **Process 与 User 两个作用域都有值**，len=40、`ghp_` 经典 PAT；Machine 空）⇒ 建议删除 User 级变量（属用户环境变更，非项目文件 ⇒ 需授权）；`gh auth status` 已实测「**The token in GITHUB_TOKEN is invalid**」⇒ 它连 gh 都用不上，留着只有泄露风险。**未轮换**（遵用户决定）。

**红线落点（已写进项目文档）**：`AGENTS.md` **§4.6.1 红线：凭据值绝不允许进日志 / 输出 / 提交物**（紧接既有 §4.6 Secrets and tokens 就地扩写，避免另开新章撞号；禁止 dump 环境变量、禁止 verbose 推送、禁止凭据拼命令行与 URL、禁止令牌常驻环境变量）+ `CLAUDE.md` **§2 一条极短指针**。

##### §I-5 · 本轮工具坑

- **`Get-ChildItem env:` 是泄露放大器**（本轮根因）⇒ 诊断脚本只打印**需要的键**，或打印前统一掩码。
- **单字母函数名撞 PowerShell 内置别名**：定义 `function H(...)` 求 sha16，被解析为 `Get-History` ⇒ 报错时**把环境变量值回显进异常文本**，等于二次泄露。脚本内**禁用单字母函数名**。
- **PowerShell 全量扫运行时 sessions 树（11,505 文件 / 3.36 GB）必然超时**：`Select-String` 300 s 超时；后台 `rg` 扫 30+ 分钟无终态且**输出文件被 rg 独占打不开**。⇒ 改**定向读**已知落点，或用 `rg -c` 只取计数、或按日期子树分批。
- **sessions 目录层级是 `v2\sessions\2026\11\23\…`（年\月\日），不是 `2026-11-23`** ⇒ 递归匹配要按真实层级写。
- **远端勘查**：`docker` 必须 `sudo -n`；管道给 `bash -s` 的脚本**首行不能带 BOM**；`api.ipify.org` 在该机 **Connection refused**（出站可用性未判定，不得据此断定断网）；`169.254.169.254` metadata 被挡（curl 28）。
- 上一轮自列的「`package.json` 绑定名缺陷」**已被实测推翻**：`wrangler.toml` 与 `package.json` 的绑定名均为 `BUCKET` ⇒ **不改**。


---

### §I · 本轮（2026-11-24 13:30–15:10）· `E:\storedata-upload` 盘查 + 腾讯云 ECS 只读勘查 + `GITHUB_TOKEN` 泄露面清理与凭据红线

**本轮性质** = **未改任何服务端/客户端产品代码**；门禁沿用 §H-6（`typecheck` 0 / `check` 0 = 13 文件 / 164 用例）。两件事：① 用户否定「新购机 / aiec.fun 所在机」框架，要求盘查 `E:\storedata-upload` 是否存腾讯云连接资料（**严格只读、可复制**）并用现成 SSH 通道上机做**只读勘查**；② 用户拍板「`GITHUB_TOKEN` 曾被打进日志 —— **暂不轮换**，改为**清理日志 + 在项目文档设红线**」。

#### §I-1 `E:\storedata-upload` 盘查结论（全程只读，一个文件都没改）

| 判据 | 实测 |
|---|---|
| 机器地址 | `.env:3`（**untracked**）`PG_HOST_PROD=124.223.115.201` / `PG_PORT_PROD=5432` / `PG_USER_PROD=kizemo` / `PG_DB_PROD=pgsql` |
| SSH 通道 | **不在项目里**，在仓库之外：`~/.ssh/config` 条目 `Host 124.223.115.201` + `IdentityFile E:\Users\Duanyi\.ssh\id_ed25519` + `IdentitiesOnly yes` + `User ubuntu` + `StrictHostKeyChecking accept-new`（免密已实测可用）；`known_hosts` 命中该 IP **3 条**、`aiec.fun` **0 条** ⇒ 佐证两者是**两台不同机器** |
| 该机角色 | ubuntu + **宝塔** + PostgreSQL；同机跑 sales-bi / Evidence BI（`/opt/sales-bi`）；库 `taobao` / `jd` / `pdd` / `erp`；口令真相源 = **1Password** |
| 不存在 | `.env.deploy`、`maintenance/` 目录、`scripts/ssh_retry.sh`；全仓 grep `sshpass` / `ssh -` / `IdentityFile` **零命中**；**无腾讯云控制台 `SecretId`/`SecretKey`/`AKID`、无 `.pem`/`.ppk`** ⇒ 只能走 SSH |
| 风险登记 | `docs/env.example.md`（**tracked**）含 `PG_PASSWORD_CLOUD` / `PG_PASSWORD_LOCAL` 明文值（且与 `.env` 两值哈希都不同 = 第三个值）⇒ 与本轮红线直接冲突，已作为「同类问题在他仓也存在」的实证；`docs/superpowers/plans/2026-10-11-pdd-loader-and-subconfigs.md:17` 记 `.env` 曾丢失且 hook `guard-secret-files.js` 阻止重建 ⇒ **`.env` 属易失副本**，别当真相源 |

#### §I-2 腾讯云 ECS `124.223.115.201` 只读勘查（两轮 SSH 实测，远端零写入）

**完整报告（含全部实测数字）**：`docs/research/fluxing-cloud-2026-11/tencent-cloud-ecs-recon.md`

- 机器 = `VM-0-3-ubuntu` / 4 vCPU / 7.5 GiB RAM（可用 4.6 GiB）+ 2 GiB swap / **`/` 118G 已用 100G = 89%（仅剩 14G）** / 免密 sudo 可用。
- 已有基建（利好）：**nginx 1.18 + certbot（`jury.xin` LE 证书在续期）+ systemd + Docker（6 容器 sales-bi-core / Grafana / Prometheus / Alertmanager / Loki / Promtail）+ 已在跑的 cloudflared tunnel**。
- 反证（风险，逐条见报告 §三/§六）：磁盘 89%、**5432 + 3306 + 9100 监听公网**（安全组未证实）、`/etc/cloudflared/config.yml` **0666 全局可写**、`sales-bi-update.service` **failed**、该机已承载 WordPress `jury.xin` 与整套 BI。
- **未取到的 4 项**（不得据此立项）：PG server 版本（本地只有 psql client 14.24）、实例规格带宽/计费/机型代号（需控制台凭据）、**ICP 备案状态**、公网出站 443 实际可用性（`api.ipify.org` 被拒但 cloudflared 在线 ⇒ 判「未判定」）。
- **结论 = 可以承载，但不建议现在切**：自建的真实成本不是「换台机器」，而是**存储层重写**（`src/sync.ts` / `src/sync-buckets.ts` / `src/wire.ts` / `src/account.ts` / `src/auth.ts` / `migrations/*.sql` 全部直接打在 D1 + R2 接口上，libsql/Turso/PG 需另写适配层）+ 协议未冻结需新旧端点并行 + 境内机 TLS 握手实测 200 ms、境内 CDN/证书/合规运维成本 ⇒ 列为 **BR-⑤**（桶化上线并验证后的独立评估批次）。
- 顺带澄清：桶化方案**已消掉「条数 × 每条约 2 行」的写放大**（40,061 条词条密文进 R2，D1 只留设备 × 桶一行），所以「读放大 / 逐条索引」问题 A 与 B 一样能解决；**B 路线（Workers Paid $5/月）仍是当前最省事且零代码改动的止血选项**，只是它不解决读路径且是账号级全局池。

#### §I-3 `GITHUB_TOKEN` 泄露面：根因坐实 + 清理结果（本轮核心）

**根因（实测）**：① 早期一轮的**自写诊断脚本**用 `Get-ChildItem env:` / `${!v}` **整块 dump 环境变量** ⇒ 令牌值落进会话快照；② **`GITHUB_TOKEN` 同时存在于 Process 与 User 两个环境变量作用域**（len=40、`ghp_` 开头经典 PAT）⇒ 用户级持久值被所有子进程继承，任何 dump 都会带出；③ 另一次**取证脚本用单字母函数名 `H` 求哈希，撞内置别名 `H=Get-History`** ⇒ 报错回显了密钥全值（PowerShell 变量插值先展开，整串密钥进了异常文本）。**未出现**：`.git/config` remote url 内嵌凭据、PSReadLine 历史、`%APPDATA%\gh\hosts.yml`、运行时 `logs/`、`log/` 目录 ⇒ 这些**都不需要清理**；`gh auth status` 现报 **`The token in GITHUB_TOKEN is invalid.`** ⇒ 该 PAT 已失效。

**清理口径（据实测确立）**：按**令牌形态正则**定位与替换，**不需要知道原值** ⇒ 原值不再出现在任何输出里；**不留 `.bak`**。

| 范围 | 结果 |
|---|---|
| 运行时 sessions 全树（5,082 个 jsonl，11,505 文件 / 3,362.5 MB） | **掩掉 6 个文件 / 18 处**（`write_locked=0` `read_failed=0`）+ 补掩 `reports/tool-outputs/` 下 **4 处**（分属 10-16 / 10-24 两个**其他会话目录**，形态同 `ghp_` ⇒ 一并脱敏，未读未复述其值）⇒ 全树 `rg` 复核**零命中** |
| 已掩样本 | 快照 `g000000000013` 第 90 行：`PRE size=561595 hits=1` → `POST size=561578 hits=0 marks=1`，UTF-8 无 BOM、LF 不变 |
| 他树复核 | `v2`（sessions 之外）/ `maintenance` / `memory` / `plugins` / `logs` / `log` / `%LOCALAPPDATA%\Temp` 顶层 ⇒ **零命中** |
| **项目 `F:\soft\00selfmade\rime_claude`** | tracked 仅 `docs/Fluxing-code-map/06-customization-points.md:126` 提到 `secrets.GITHUB_TOKEN`（GitHub Actions **合法文档提及，不是泄露**）；untracked 命中全是**变量名字面量**（`tmp/l170-head-check.txt`、`tmp/step2-backup/lessons-learned.md`、`tmp/step3-backup/AGENTS.md*`）⇒ **项目产物零明文令牌值，未改动任何项目文件** |

**未闭环 / 需用户授权**：**删除 User 级 `GITHUB_TOKEN` 环境变量**属「用户环境变更」，本轮**没有动**；`gh` 已判其 invalid ⇒ 若其它工具仍依赖它会受影响，故报「建议删除 + 等授权」。轮换仍按用户拍板**暂不做**。

#### §I-4 红线落点（已入库）

- **`AGENTS.md` §4.6.1「红线：凭据值绝不允许进日志 / 输出 / 提交物」**（§4.6 原已存在 ⇒ **就地扩写子节，不新开编号、不撞号**；文件 40,667 B / 753 行 / **LF / 无 BOM**）：四条禁止动作 = ① 禁 dump 环境变量（只输出键名 + 长度 + sha16 前 16 位）② 禁把凭据拼进命令行 / URL ③ 禁在含凭据时开 `git push -v` / `curl -v` / `wrangler --debug` ④ 禁把令牌常驻 User 级环境变量；另登记**两条事故实证**与**处置口径**（形态正则定位不需原值、清理不留 `.bak`、运行时日志不在 git 仓库 ⇒ 清理不产生 commit、合法提及变量名的文档不算泄露）。
- **`CLAUDE.md` §2** 加一条极短强制项指向该红线（注入预算仅 32,768 B ⇒ **不复述细节**）。

#### §I-5 本轮工具坑（新增，勿重试）

- PowerShell 里**函数名 / 变量名一律取长名**（`Get-Sha16`）：单字母 `H` 撞内置别名 `Get-History` ⇒ **异常文本回显密钥全值**。
- `git show HEAD:<path>` / `Get-Content` 经**管道会规范化行尾** ⇒ 判 CRLF/LF 与尾部字节必须用 `[IO.File]::ReadAllText` / `ReadAllBytes`。
- 全量扫描 3.3 GB 会话树：`Select-String` 前台 **300 s 超时**、后台 `rg` 全量 **30+ 分钟无终态**且输出文件被独占（`ReadAllLines` 报 being used by another process）⇒ 改为**一次后台调用内「掩码 + 就地 `rg -c` 复核」合并完成**，且**等待必须与有用检查合并成一条命令**。
- 运行时会话目录层级是 `v2\sessions\2026\11\23\…`（**不是** `2026-11-23`）；`maintenance\task-state\LATEST.md` 属**另一项目**（Alpha File Manager），其「最近一条用户指令」字段不代表本批。
- 远端勘查：宝塔 `vhost/nginx` 与 `/etc/caddy` **都不存在**（nginx 为 apt 包 + `sites-enabled` 托管）；`169.254.169.254` 元数据被挡（curl 28）；`docker` 需 `sudo -n`；管道给 `bash -s` 首行**带 BOM** ⇒ `command not found`；here-string 里 `awk 'NR==2{print $3"/"$2}'` 转义踩坑 ⇒ 改用 `grep`。

#### §I-6 · 本轮 commit 的推送状态（未落，网络侧）

- 本批文档 commit（含 §I 与红线、勘查报告、接力 prompt 订正）**已入库、已于 2026-11-24 15:11 补推成功**（见 §J-1；本条「未推送」结论**已过时**，以 §J-1 为准）。
- **失败形态（共 5 次尝试）**：代理 + 默认 SSL 后端 ⇒ `TLS connect error: error:0A000126:SSL routines::unexpected eof while reading`；代理 + `-c http.sslBackend=schannel` ⇒ `schannel: failed to receive handshake`；两者均 `PUSH_EXIT=128`。
- **判据（一次取齐，别反复试）**：`curl.exe --noproxy '*' -o NUL -s -w "%{http_code}" --max-time 12 https://github.com/` 与 `curl.exe -x http://127.0.0.1:7897 ...` **同时返回 `000`** ⇒ 不是「代理坏了走直连」那条既有经验（那种是直连 200 / 代理握手失败），而是**两条路径同时不通** = 本机出口网络故障。
- **纪律**：按既有记忆**带退避重试**（20–40 s）即可，**不得**为此改全局 git config、**不得**重启用户的 Clash Verge（`verge-mihomo`）；网络恢复后一条命令补推：
  `git -C F:/soft/00selfmade/rime_claude -c http.proxy=http://127.0.0.1:7897 -c https.proxy=http://127.0.0.1:7897 push kizemo feat/100-cloud-sync`
- **不做的事**：本轮**不留**未终态的后台重试任务（后台任务完成会自动唤醒父会话烧额度）；把「补推」写进接力 prompt 首件事，由下一轮或用户手动执行。

---

### §J · 本轮（2026-11-24 15:11–15:30 · BR-①f 阶段收口轮）—— 补推已落 + D8 口径订正入码 + 生产链自动轮扩写

**本轮性质** = ① 补推本批末笔 docs commit（§I-6 的「未推送」结论作废）；② 按 ADR 0077 D8 把**写预算护栏的双口径**落到配置与代码注释（**仅注释/配置，无功能变更**）；③ 把生产链定时任务从「3 步」扩成「7 步全链路」（AC-09 → 迁移 → EXPLAIN → deploy → 冒烟 → BR-①g 开工 → 收尾双产物）；④ 收口双产物 + CLAUDE.md 指针更新。**本轮未 apply 迁移、未部署、未在生产写入任何东西、未改客户端。**

#### §J-1 补推结果（已验证）

| 项 | 实测 |
|---|---|
| 补推前 | `HEAD = ed6dd1d`，远程 tip `8a4f959`，`rev-list --count kizemo/feat/100-cloud-sync..HEAD` = **1** |
| 执行 | `git -C F:/soft/00selfmade/rime_claude -c http.proxy=http://127.0.0.1:7897 -c https.proxy=http://127.0.0.1:7897 push kizemo feat/100-cloud-sync` |
| 结果 | **`8a4f959..ed6dd1d`，PUSH1_EXIT=0**；之后 `ahead = 0` |
| **与 §I-6 相反** | §I-6 记「代理与直连两条路径同时不通 = 出口网络故障」；**本轮 15:11 那次代理链路一次即通**，直连未触发 ⇒ 出口并非持续故障，而是**间歇性抖动**（15:30 再次双路 `000`，见 §J-1b） |

#### §J-1b 本轮 commit `0b08972` 的推送状态（**已推送成功**，ahead = 0）

- 本轮收口 commit `0b08972`（`chore(fluxing): ADR 0077 D8 护栏双口径订正 + BR-①f 阶段收口轮`）**已入库、已推送远程**（`ed6dd1d..0b08972`，PUSH_EXIT=0，之后 `ahead = 0`）。
- **中间的实测判据（15:30–15:36）**：`curl` 直连 `DIRECT=000` / 代理 `PROXY=000` **同时失败**；代理进程**仍在监听**（`PROXY_LISTEN=PID 67596`）⇒ 是**出口/握手层故障**，不是「代理进程挂了」；直连 `git push` 报 `Connection timed out after 300000 milliseconds`（EXIT=128）；代理路径报 `TLS connect error: error:0A000126:SSL routines::unexpected eof while reading`（EXIT=128）⇒ **与 §I-6 记录的形态逐字一致**。
- **恢复方式**：`curl --max-time 10` 判据 + **22 s 退避**重试，**第 1 次退避即恢复**（`ATTEMPT=1 PROXY=200 DIRECT=000` ⇒ 代理通、直连仍不通）⇒ 随后代理路径 `git push` 一次成功。
- **与 §J-1 的关系**：15:11 推 `ed6dd1d` 成功、15:30 推 `0b08972` 失败、15:38 退避后成功 ⇒ **出口网络是间歇性抖动**，**不能拿上一轮「通」的结论当现状**，每轮都要实测判据。
- **纪律（未违反）**：全程**未改全局 git config**、**未重启用户代理进程**（Clash Verge / `verge-mihomo`）、**未试第二条 SSL 后端**（§I-6 记录 `schannel` 亦失败）；只做退避重试。
- **本批两条 commit 均已在远程 `kizemo/feat/100-cloud-sync`**：`ed6dd1d`（§I 文档 + 红线 + 勘查报告）与 `0b08972`（D8 双口径 + §J 收口）。


#### §J-2 ADR 0077 D8 双口径订正（本轮唯一的产品侧改动，**注释 + 配置，无行为变更**）

- **`cloud/fluxing-sync/wrangler.toml`**：新增 `WRITE_BUDGET_BUCKETS_PER_DAY = "4000"`（桶行口径上限，**与 `src/env.ts` 代码默认值 `4_000` 一致** ⇒ 运行行为等价）+ 订正 `WRITE_BUDGET_ROWS_PER_DAY` 的注释（「80,000 行 ≈ 允许一次全量重推」**在桶化路径上已不成立**：桶化路径产生的是「设备数 × 桶数」量级桶行，上界 32 × 64 = 2,048 行/账号）。
- **`cloud/fluxing-sync/src/sync.ts`** 写预算护栏小节：追加「⚠ 口径订正（ADR 0077 D8 · 桶化后本小节只管逐条端点）」段，指明桶端点走 `sync_buckets` 行 / `WRITE_BUDGET_BUCKETS_PER_DAY` / 实现见 `src/sync-buckets.ts` 与 `src/env.ts`，并重申两道护栏都是**每账号刹车、非 Cloudflare 全局护盾**。
- **门禁（父会话亲跑，非采信自报）**：`typecheck` **EXIT=0** / `check` **EXIT=0** = **13 文件 / 164 用例**（基线 §H-6 同值）。
- **为何无需新增测试文件**：`test/bucket-sync.test.ts` 的「老逐条端点回归」(:648) 走**真实 `/v1/sync/push` + `/v1/sync/pull` 行为断言**（非字节 diff）⇒ 改注释安全；T13（`DELETE /v1/account/data` 清 `u/`+`b/` 两前缀与两表，:716）与 T14 等价项已由该文件合并覆盖。

#### §J-3 生产链自动轮扩写（定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`，北京时间 2026-11-25 08:10:46）

**订正前的 3 个缺口 → 已补齐**：

| 缺口 | 订正后 |
|---|---|
| 迁移命令用了 `npm run migrate:remote`（内部用**库名** `fluxing-sync`） | 改为**已验证的绑定名 `DB` 形式** + 显式 `--config`；并写明「不要用 `migrate:remote`」 |
| 只含迁移 0003，**缺 0004 / deploy / 生产冒烟** | 扩为 7 步：0 额度核查 → 1 AC-09 → 2 迁移(0003+0004) → 3 EXPLAIN → 4 deploy → 5 冒烟 → 6 BR-①g 开工 → 7 收尾双产物+入库+推送 |
| 缺额度前置闸门 | 第 0 步强制 `rows_read_24h < 5,000,000` 才可进第 2 步，否则**如实登记并停** |

**新增的硬性约束（写进任务 prompt）**：① 第 2 步前先 `migrations list` 看**末尾**哪些未 apply（0004 **无 `IF NOT EXISTS`** ⇒ 已 applied 就不许重跑 apply）；② 第 4 步 deploy **必须在**第 2 步 migrate 之后（`src/account.ts:94` 无条件 `DELETE FROM sync_buckets` ⇒ 0004 未 apply 就部署 ⇒ 「注销账号数据」对所有用户报错 = 生产回归）；③ 0003 迁移前读计划为 `idx_sync_user_id`（护栏 COUNT 每次推送扫该用户全部行），迁移后必须变 `idx_sync_user_updated`，否则**如实登记 FAIL**；④ 凭据红线（只输出键名 + 长度 + sha16 前 16 位）；⑤ 推送先直连后代理、**禁改全局 git config、禁重启用户代理**。

**第 6 步的边界（本轮明确写死）**：BR-①g **只做「开工」**（设计落点 + 任务清单细化 + 客户端侧第一批**可门禁验证**的改动），**本轮不要求出包装机与真机判据**（留给下一轮有主会话在场时）；客户端门禁必须单独 msbuild `TestFluxingCloud` vcxproj 且显式传 `/p:SolutionDir=`（`weasel.sln` **不含**该工程 ⇒ 否则跑旧 exe、PASS 恒 787）。

#### §J-4 生产阻塞现状（已验证，2026-11-24 15:11–15:19 实测）

- `wrangler d1 info DB --json` ⇒ `rows_read_24h = 9,083,744`（**> 免费 5,000,000 行/日**）、`rows_written_24h = 33,043`、`read_queries_24h = 1,539`、`write_queries_24h = 13,110`、`database_size = 19,255,296`。
- `wrangler d1 migrations list DB --remote` ⇒ **FAILED `[code: 7500]`**「exceeded D1's free tier daily row **read** limit ⇒ wait until tomorrow (midnight UTC)」⇒ **连 `migrations list` 都被读额度挡**。
- 读计数 12:52 的 9,083,801 → 15:11 的 9,083,744 **基本静止** ⇒ a-9 修复后客户端已不再刷读，但旧读量仍留在当日 UTC 桶内 ⇒ **2026-11-25 08:00（北京）重置**。
- 当前生效生产 Version = **`5f3732e8-f1d2-4829-a8dd-1c5b97a90039`**（Created 2026-11-24T04:04:12.850Z，止血版，**无桶级端点**）；迁移 0003/0004 **均未进生产**。

#### §J-5 已验证 / 推断 / 未验证（本轮新增）

| 状态 | 内容 |
|---|---|
| **已验证** | 补推 EXIT=0（ahead 0）；`typecheck` 0 / `check` 0 = 13 文件 164 用例；D1 额度四项读数；`migrations list` 被 `code 7500` 拒；当前生产 Version id；cron 任务 prompt 已扩写（7 步）且 `enabled=true`/`status=active`/`runAtMs` 未变；接力 prompt 已就地改写（2,580 B）；CLAUDE.md §2 已加 BR-①f 指针 |
| **推断** | 「D1 读计数已静止 ⇒ a-9 后客户端不再刷读」（读计数差 57 行是控制面自身查询，非客户端批量）；「2026-11-25 08:00 重置后可跑完整链」（基于 UTC 日重置机制 + 任务排在重置之后 10 分钟） |
| **未验证** | AC-09 复跑结果（**仍记 FAIL**，待 08:10:46 探针 v4）；迁移 0003+0004 的实际 apply；EXPLAIN 前后对比；新生产 Version id；桶端点生产冒烟；BR-①g 客户端任何产出；`WRITE_BUDGET_BUCKETS_PER_DAY` 在生产上的实际取值（**须在 deploy 后回读**） |

#### §J-6 本轮新增工具坑（§H-11 已记的不重复）

- `package.json` 的 `migrate:remote` = `wrangler d1 migrations apply fluxing-sync --remote`（**库名**）；已验证可解析的是**绑定名 `DB`** ⇒ 定时任务/脚本里**一律用 `DB` 形式 + 显式 `--config`**，别用 npm script。
- handoff 文档本身是 **LF / 无 BOM**（实测 120,207 B / 793 行 / `CRLF=0`）⇒ 追加小节时保持 LF，不要引入 CRLF。
- 用 `edit` 改中文长行时**必须逐字复述原文**（含全角标点）⇒ 手抄极易出错（本轮 `用户` 被误写成 `_user` 导致匹配失败）；改前先 `read` 回该行原文。
- **PowerShell 里的 `Remove-Job` 会被本地安全网关当成删除类命令整体拦掉**（连带整条复合命令被拒，即使它只是清理作业）⇒ **不要在 bash 里写 `Remove-Job` / `Remove-Item` 之类**；需要并行探测时改用顺序执行（两个 `curl` 顺序跑 ≤ 25 s），或用 `Start-ThreadJob` 但**不清理**（作业随进程退出自动消失）。
- **出口网络故障是间歇性的**（本轮 15:11 通、15:30 断）⇒ **每轮都必须实测 `curl` 双路径判据**，不能拿上一轮「通」的结论当现状；直连 `git push` 在出口不通时会**挂满 300 s**（`Connection timed out after 300000 milliseconds`）⇒ 探测用 `curl --max-time 10` 先行，别直接拿 git 探活。

#### §J-7 定时任务缺陷修复轮（2026-11-24 16:16，北京）

**触发**：用户 2026-11-24 指示「你新建了两个明早的定时任务，请修复bug」。复核本会话新建的两个明早一次性任务（主链 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` + 回检 `self-74142b51b3ab20d8a7a62216dabe0054`）⇒ **定位 5 个缺陷**（主链 4 个 + 回检 1 个，其中 3 个为跨任务缺陷）⇒ 本节记录逐条定位、修法与最终任务态。

| # | 缺陷 | 证据 | 修法 | 状态 |
|---|---|---|---|---|
| 1 | **回检时机与主链时长错配**：旧回检 08:21:05 触发，而主链含探针两阶段（默认 `-Stage All`，每阶段 `WaitRoundSec=300` + `SettleSec=45`）+ 迁移 + EXPLAIN + deploy + 冒烟 + BR-①g + 收尾 ⇒ 必然撞在半截日志上 | 实测 `%TEMP%\fluxing-profile-notify.log` 单轮同步 1–15 分钟；主链 runAtMs=1795565446336（08:10:46）、旧回检 runAtMs=1795566065738（08:21:05），间隔仅 10 分 19 秒 | 回检**改到 2026-11-25 10:30:00**（runAtMs `1795573800336`），且判据改为读哨兵而非抢读半截日志 | 已修 |
| 2 | **回检的「日志新鲜度闸门」形同虚设**：`Tee-Object -FilePath` 管道启动即创建/截断文件 ⇒ mtime 数秒内即 > 08:10:46；且旧轮 `reports/fluxing-br-1c/ac09-run.log`（2026-11-24 08:18）**含 `=== AC-09 v4 END ===`** ⇒ 会被误读为本轮结果 | 实测旧轮日志含收尾标记；mtime 闸门对 `Tee-Object` 失效 | 主链第 1 步改写**带时间戳新日志** `reports/fluxing-br-1f/ac09-run-<yyyyMMdd-HHmmss>.log`，并把路径写进哨兵 `AC09_LOG=`；回检**只认哨兵里的路径** + **完整性双标记**（`AC09_VERDICT=` **与** `=== AC-09 v4 END ===` 缺一即 UNDETERMINED）⇒ **禁 mtime 判新鲜度** | 已修 |
| 3 | **主链 prompt 自相矛盾**：正文写「本轮六步」但实际列第 0–7 步（8 段）；收尾双产物排链尾 ⇒ 任一步失败则全部实测数字丢失 | 旧 prompt 正文与步骤列表不一致 | 改为「第 0–7 步（共 8 段）」+ 三条贯穿硬规矩（A 每段结束立刻追加一行 `reports/fluxing-br-1f/chain-progress.log` 纯 ASCII 进度；B 任何一段失败 / BLOCKED 也必须先写 handoff §K 再停；C 凭据红线） | 已修 |
| 4 | **任务名与内容不符**：旧名「BR-1c 真机判据 AC-09 第二次复跑」/「Task follow-up (dabe0054)」⇒ 从名字看不出跑什么 | 任务名与实际链内容无关 | 主链改名「Fluxing BR-①f 生产链自动轮（第0-7步：AC-09复跑→迁移0003/0004→EXPLAIN→deploy→桶端点冒烟→BR-①g开工→收尾入库）2026-11-25 08:10」；回检改名「Fluxing BR-①f 生产链回检（哨兵判定 AC-09 + 迁移/部署/冒烟结果）2026-11-25 10:30」 | 已修 |
| 5 | **冒烟缺鉴权路径 + BR-①g 缺前置闸门**：旧 prompt 只提只读助手 `tmp/AK-d1.mjs`（无鉴权、不能 push / pull），也未说明 `device_id` 服务端权威 ⇒ 冒烟步骤实际不可执行；且 BR-①g 无「上游未通过就不许改客户端代码」的闸门 | `tmp/AK-d1.mjs` 仅只读；`src/sync-buckets.ts:110-123` `resolvePushDeviceId` 以服务端 `auth.deviceRowId` 为权威，客户端自带 `device_id` 不等即 400 `bad_device_id` | 第 5 步补齐完整路径（`tmp/br-prod-verify.mjs setup` → `POST /v1/auth/login`（`auth_key`）→ **两台设备两个 `device.id`** → push / pull 断言 → `cleanup` 后额外删 `sync_buckets` 探针行）；缺鉴权路径 ⇒ 记 BLOCKED「冒烟未执行」，**禁止**把失败当通过；第 6 步加前置闸门（第 1–5 步全 PASS 才开工，否则 `STEP=6 STATUS=SKIP` 且不改客户端代码；改码前先跑门禁基线，红了回滚） | 已修 |

**附带修正**：主链第 0 步的额度闸门口径由 `rows_read_24h < 5,000,000` 改为**「一条极小读命令能否成功」**（`SELECT COUNT(*) AS n FROM d1_migrations`）—— 理由：`rows_read_24h` 是**滚动 24 小时**计数，而 Cloudflare 额度按 **UTC 日**重置 ⇒ 08:10 时它可能仍显示昨日高峰值（> 500 万）而额度其实已重置 ⇒ 用它当硬闸门会白白停摆一整天（**已验证**：2026-11-24 15:11 实测 `rows_read_24h = 9,083,744` 但读计数已静止）。

**最终任务态（已验证，`cron_read` 回读）**：

| 任务 | ID | 触发 | 形态 |
|---|---|---|---|
| 主链（8 段） | `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` | 2026-11-25 08:10:46（runAtMs 1795565446336） | once / enabled / active / session=new / model `minimax/MiniMax-M3.1-Flash` / project `F:\soft\00selfmade\rime_claude` |
| 回检（哨兵判定） | `312fd16e-d687-4973-b636-f1044ad9cdd0` | 2026-11-25 10:30:00（runAtMs 1795573800336） | once / enabled / active / session=new / 同 model 与 project |
| ~~旧回检~~ | ~~`self-74142b51b3ab20d8a7a62216dabe0054`~~ | ~~2026-11-25 08:21:05~~ | **已删除**（`cron_delete` ⇒ `success: true`） |

**回检任务判据（要点）**：① 只读哨兵 `reports/fluxing-br-1f/chain-progress.log`，**禁 mtime 判新鲜度**；② AC-09 需**完整性双标记**齐备才可登记 PASS，缺一即 UNDETERMINED；③ 迁移 / 部署 / 冒烟按哨兵 `STEP=3/4/5` 的 STATUS 登记，缺失记 `MISSING`；④ **不改任何产品代码**、不重复主链工作；⑤ 未完成时**最多自排一次 +90 分钟补排**（补排轮不再续排）；⑥ 凭据红线照旧（只输出键名 + 长度 + sha16 前 16 位）。

**交接小节编号约定（本轮新定，防撞号）**：**主链写 §K**（生产链实测），**回检写 §L**（回检结论）⇒ 两个定时任务不会写进同一节。

**本轮新增工具坑（§J-6 不重复）**：
- **`cron_update` 没有 `at` / `runAtMs` 字段**（只有 `schedule` / `timezone` / `prompt` / `session` / `enabled` 等）⇒ **改一次性任务的触发时间只能 `cron_delete` + `cron_create(at=…)`**。
- **`cron_create` 的一次性任务不接受 `enabled` 字段** ⇒ 报 `CRON_INVALID_INPUT: enabled is available for recurring creation. Use cron_update to pause a one-time task.`（一次性任务默认即 enabled；要暂停得事后 `cron_update`）。
- **`cron_create` 在 `session.mode=sessionId` 时必须省略 `agent_name`**（否则报 `validation: agent_name must be omitted when session.mode=sessionId`）；`mode=new` 时则**必须**给 `agent_name`（本次给 `mavis`）。
- 一次性任务 `at` 传 **Unix 毫秒**（`1795573800336` = 2026-11-25 10:30:00 北京）⇒ 与 `cron_read` 回读的 `runAtMs` 一致，可用于对账。

**§J-7 三态标注**：

| 状态 | 内容 |
|---|---|
| **已验证** | 5 个缺陷逐条定位并已修（主链 `cron_update` 回读确认新名 + 8 段链 + 三条硬规矩 + 闸门口径订正 + 鉴权路径 + 前置闸门）；回检新任务建成（`cron_create` ⇒ `ok: true`，`runAtMs=1795573800336`、`enabled=true`、`status=active`）；旧回检已删（`cron_delete` ⇒ `success: true`）；`cron_read` 列表确认本项目仅剩这 2 个 enabled 任务，另 2 个（`b76b2dad…` filemanager / `f31f43cf…` practilog）仍 paused **未动**；本轮**未改任何产品代码**（工作树仅 `m librime` + 既有 untracked） |
| **推断** | 回检 10:30 足以覆盖主链 8 段（按主链各段实测耗时估算）；补排机制在主链整体阻塞（额度未重置）时能给出第二次观察窗口 |
| **未验证** | 两个任务的**实际运行结果**（尚未到触发时间）；主链第 1 步 AC-09 复跑结果（**仍记 FAIL**）；迁移 0003/0004 的实际 apply；EXPLAIN 前后对比；新生产 Version；桶端点生产冒烟；BR-①g 客户端产出 |

#### §J-8 用户拍板落地轮（2026-11-24 16:23，北京）

**拍板来源**：问卷卡 `ask_1f8ccef18141ef216578c60b`（4 项已答）；**卡 `ask_ff50e357895edb4fca4cac06`（⑤ 腾讯云安全 + ⑥ 是否合并两个定时任务）尚未作答 ⇒ 该两项本轮未动、未代答**。

| # | 议题 | 用户拍板 | 落地动作与后果 |
|---|---|---|---|
| ① | User 级环境变量 `GITHUB_TOKEN`（PAT 已 invalid，明文常驻）是否授权删除 | **暂不删除** | 本轮**不动** User 级环境变量；维持「形态正则扫描」作为唯一兜底（既有扫描留档 `reports/fluxing-br-1f/token-sweep-2026-11-24.txt`）⇒ 后续批次继续沿用「凭据值绝不进日志 / 输出 / 提交物」红线（AGENTS.md §4.6.1） |
| ② | Cloudflare Workers Paid 是否升级 | **暂不升级** | D1 免费额度仍是硬约束 ⇒ ① 主链第 0 步「额度活性探针 + 零写入纪律」**必须保留**；② 迁移 0004 建表写入与桶化后的补推洪峰都要压在单日额度内 ⇒ **新增前置**：桶化后一次性补推约 54,536 条的洪峰需在 **BR-①g 开工前定分批策略**，否则会再撞 `code 7500`（登记为 BR-①g 前置项，**未验证**） |
| ③ | BR-② 写预算护栏是否改全局口径 | **维持现状（每账号口径）** | **本批不改实现**；ADR 0077 D8 的口径注释（逐条量 `sync_items` 行 80,000/账号/UTC 日、桶量 `sync_buckets` 行 4,000/账号/UTC 日，皆**每账号刹车、非 Cloudflare 全局护盾**）保持；BR-② 作为**已知边界**登记，不立项 |
| ④ | 老逐条端点读放大何时止血 | **随 BR-①g 退役** | ①f 内老端点**逐字不动**（ADR 0077 D6 不变）；**BR-①g 必须交付 ADR 0077 D6 的显式退役流程「双写 → 校验 → 切读 → 退役」四段 + 回滚预案**（已写进 spec 102 §8 第 7 项）；**只做「切读」就下老端点 = 不合格** |

**本轮实际改动（仅文档，无产品代码）**：`.specify/specs/102-bucket-level-sync-storage/tasks.md` §8 第 7 / 9 / 10 项按上述拍板就地订正；本节落盘；`CLAUDE.md` §2 指针补记拍板结果。

**本轮刻意未改主链定时任务 prompt**（`4d3e51f8`）：该 prompt 约 4,000 字中文，`cron_update` 只能整段替换 ⇒ 逐字重述风险高（§J-6 已记同类坑）。改用两条**既有读取路径**把拍板传给明早主链 —— ① 主链开头要求读本 handoff 的 §H-10 与 **§J**（§J-8 即在 §J 内）；② 主链第 6 步要求读 **spec 102 `tasks.md` 的客户端侧条目**，而 §8「本批不做什么（属 BR-①g 客户端批次）」正是客户端侧清单，第 7 / 9 / 10 项已写入本次拍板 ⇒ 决策**双路径可达**，无需重写 prompt。

**未做 / 未验证**：
- ⑤ 腾讯云两项安全问题（`cloudflared/config.yml` 权限 0666、5432/3306/9100 公网监听）—— **等用户拍板后处理**；本轮未连腾讯云机、未改任何远端权限；
- ⑥ 两个定时任务是否合并 —— **等用户拍板**；当前仍为「主链 08:10:46 + 回检 10:30」两个独立任务（推荐保持分离，理由见 §J-7）；
- 本轮**未跑门禁**（无产品代码改动，沿用 §J-2 的 `typecheck` 0 / `check` 0 = 13 文件 / 164 用例）；**未部署、未迁移、未改生产**。

#### §K BR-①f 生产链自动轮实测（2026-11-25 08:11–08:47，北京）

**本轮性质** = **生产链第 0–7 步自动执行 + 生产变更（迁移 + 部署）+ 收尾双产物**（**本轮未改任何产品代码**；服务端桶化代码早在 `a7c61e0` 入库）。**执行者** = 一次性定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（runAtMs 1795565446336 = 北京时间 2026-11-25 08:10:46）。

**生产链八段结果总表**（哨兵 `reports/fluxing-br-1f/chain-progress.log`）：

| 段 | STATUS | 关键实测数字 |
|---|---|---|
| STEP 0 额度活性探针 | DONE | `SELECT COUNT(*) FROM d1_migrations` ⇒ n=2、rows_read=2、rows_written=0、PROBE_EXIT=0、**无 `[code: 7500]`** ⇒ 额度已按 UTC 日重置；d1 info 当时 rows_read_24h=531 / rows_written_24h=12 / num_tables=8 / database_size=19,255,296 |
| STEP 1 AC-09 复跑 | DONE（**判定 FAIL**） | `AC09_VERDICT=FAIL`、`AC09_SUMMARY stages=2 overall=FAIL strict=FAIL`；Legacy 段 table_write_delta=8 / count_delta=0 / rows24h_delta=40 / push=1；Deleted 段 table_write_delta=0 / count_delta=0 / rows24h_delta=0 / push=1；客户端 PID 57640、`WeaselServer.exe` sha256 `A106522FD8F5AC31ABBF392FA906F76ABFD73032A73640E0FE45742057394A94`（= a-9 语义指纹版期望值） |
| STEP 2 迁移 0003+0004 | DONE | `wrangler d1 migrations apply DB --remote` ⇒ **MIGRATE_EXIT=0**，一次应用 0003+0004；`num_tables` 8→**9**；database_size 19,255,296→**21,295,104** |
| STEP 3 EXPLAIN 复测 | DONE | `sync_items` 护栏 COUNT ⇒ `SEARCH sync_items USING COVERING INDEX idx_sync_user_updated (user_id=? AND updated_at>?)`；桶侧护栏 COUNT ⇒ `idx_sync_buckets_user_updated`；桶侧拉取真实形态（`user_id=? AND id>? ORDER BY id ASC`）⇒ `idx_sync_buckets_user_id (user_id=? AND id>?)` ⇒ **三条读计划全部命中新索引** |
| STEP 4 部署 Worker | DONE | `npm run deploy` ⇒ **DEPLOY_EXIT=0**；**新生产 Version `4f163326-f2a5-4c69-be56-24cb800e6915`**（Created 2026-11-25T00:28:02.846Z）；绑定回读 `WRITE_BUDGET_ROWS_PER_DAY=80000` / `WRITE_BUDGET_BUCKETS_PER_DAY=4000` ⇒ ADR 0077 D8 双口径在生产生效 |
| STEP 5 生产桶端点冒烟 | DONE | **9/9 全 PASS**：login A/B=200；push1200 ⇒ accepted 含目标 key / skipped=[] / server_seq=1；同 HLC 再推 ⇒ accepted=[] 且 skipped 含目标 key（**HLC 守卫生效**）；pull A=200 / buckets 含目标 / has_more=false / next_cursor 存在 / server_seq=1 / **cipher 原样往返=true** / item_count=1；**pull B 可见第一台推的桶（跨设备可见）**；清理 ⇒ 探针 `sync_buckets` 行 changes=1 → 残留 **0**、`br.%` users 残留 **0**、`VERDICT_CLEANUP=PASS`（`{"users":0,"ev":0,"devices":0,"rt":0,"sync":0}`） |
| STEP 6 BR-①g 客户端桶化 | **SKIP** | 闸门「第 1–5 步全部 DONE 且关键判定 PASS」不满足（STEP 1 关键判定 AC-09 = FAIL）⇒ 记 `STEP=6 STATUS=SKIP BLOCKED_BY=STEP1 REASON=AC09_VERDICT_FAIL NO_CLIENT_CODE_CHANGED` ⇒ **未改任何客户端代码**（分桶打包 / 侧车 v2 / seq 归零均未开工） |
| STEP 7 收尾双产物 | DONE | 本节 §K + 接力 prompt 就地改写 + `CLAUDE.md` §2 指针订正 + 文档入库推送 |

**§K-1 AC-09 = FAIL 的性质拆分（本轮最重要的判定，必须区分两段）**：

| 段 | 含义 | 硬数据 | 判读 |
|---|---|---|---|
| **Legacy 段**（侧车完好） | 真实本地改动 8 条的**正常增量推送** | table_write_delta=**8**、count_delta=**0**、rows24h_delta=**40**、push=1；诊断行 `08:15:22 push batch items=8 elapsed_ms=2469 http=200`；`08:17:14 reconcile scan_complete=0 matched=67 deferred=54536 divergent=0 pushed=8`；`08:18:49 semantic legacy_rows=0 legacy_held=0` | **属真实改动的正常写入**，量级比修复前「每轮重写 3,072–40,001 行」低两个数量级 ⇒ a-9 语义指纹修复在生产生效（`legacy_rows=0` ⇒ 旧口径行已全部锚定为 `s2:`，无遗留旧口径待推） |
| **Deleted 段**（删侧车） | 侧车丢失场景 | table_write_delta=**0**、count_delta=**0**、rows24h_delta=**0**、push=1 | **a-7 字面判据成立**（生产 D1 零写入）⇒ 侧车丢失不再触发全量重推 |
| **整体** | — | `AC09_VERDICT=FAIL` | 探针 PASS 判据（`probe-ac09.ps1:318`）= `table_write_delta==0 且 count_delta==0 且 pushCount==0` 三者**同时**成立 ⇒ 只要客户端发出过一条 `push batch` 日志行即判 FAIL ⇒ **整体判定 FAIL 的唯一触发项是 `push=1`（Legacy 段有真实改动要推）**，不是写放大复发 |

⇒ **AC-09 不得改记 PASS**（只有 `AC09_VERDICT=PASS` 才可改记）。**已验证**：客户端诊断行 `08:19:37 reconcile matched=67 deferred=94537 divergent=0 pushed=0`、`semantic legacy_rows=0 legacy_held=0`；**未验证**：Deleted 段 push=1 为何出现（探针 per-stage 明细缺失，见 §K-4）。

**§K-2 写放大核查（`rows_written_24h` 由 52 涨到 40,187 —— 已澄清，不是事故）**：
- **已验证硬证据**：`sync_items` COUNT 仍 = **40,061**（未增）、`MAX(updated_at)` = **1795565719**（= 北京时间 08:15:19）⇒ **08:15:19 之后 `sync_items` 零写入**。
- **归因（推断）**：`rows_written_24h` 的增量来自迁移 0003 的 `CREATE INDEX` 扫描 40,061 行（Cloudflare 计数延迟上报），**不是**写放大回流。
- **处置**：不按事故处理；但**桶化后一次性补推约 54,536 条的洪峰**仍须先定分批策略（§J-8 拍板 ② 的前置，未闭环）。

**§K-3 侧车与客户端现状（下一轮必须先看）**：
- 现役侧车 `D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-sync.state` = **5,129,103 B** / mtime 08:17:14（Legacy 段重建后的 `s2:` 口径）；
- `.failed-ac09` = **8,714 B** / mtime 08:19:37（Deleted 段客户端自建的 67 条小侧车）；
- 探针 FAIL 路径执行了 `SIDECAR_RESTORED_BECAUSE_NOT_PASSED` ⇒ **恢复的是 Deleted 段之前的 5 MB 侧车**；
- **已验证**：客户端 PID 57640 存活（WS 49.7 MB）；诊断日志末次写入 08:19:37（此后无 cloudsync 行）。

**§K-4 探针侧缺陷（✅ 已定案并修复 · 2026-11-25 收尾轮）**：
- **症状**：本轮 AC-09 日志**只有全局 PRE 行 + 最终 verdict 行**（9 行），per-stage 明细行（PRE/POST、`DELTA_*`、`STAGE_SUMMARY`、`RECONCILE_LINES`）**全部缺失**；`GetReconcileStat` / `GetSemanticStat` 解析返回 `legacy_rows=-1 / legacy_held=-1 / scan_complete=`（空）。
- **根因（差分复现坐实，非推断）**：**`Log` 用 `Write-Output` 写日志 ⇒ 日志文本进入函数的「返回流」** ⇒ ① `WaitForRound` 的返回值 = `POLL` 日志行 + hashtable 组成的**数组**（`$r.GetType()` = `Object[]`）⇒ `$r.Reconcile[-1]` 取到的是**数组**而非字符串 ⇒ 正则 `-match` 对数组不命中 ⇒ 统计恒为 `-1` / 空；② `RunStage` 的**全部 per-stage 明细**被调用方 `$results += , (RunStage ...)` 一并吞进结果数组 ⇒ **永远不进 stdout / 日志文件**（顶层 `Log` 不在函数内，所以顶层 6 行 + 尾部 3 行正常）。
- **差分复现证据（已验证）**：同一组真实诊断行（`scan_complete=0 matched=67 deferred=54536 divergent=0 pushed=8` / `legacy_rows=0 legacy_held=0`）喂两版实现 —— **DIRTY 版（`Write-Output` 记账）**：`$r` 类型 `Object[]`、`Reconcile[-1]` 是数组、`legacy_rows=-1 legacy_held=-1`；**CLEAN 版**：`$r` 类型 `Hashtable`、`legacy_rows=0 legacy_held=0`、`scan_complete=0 matched=67 deferred=54536 divergent=0 pushed=8` ⇒ **证明根因在返回流污染，与正则写法无关**（原判断「正则表面可匹配 ⇒ 根因未定位」已被推翻）。
- **修法（6 处定点修改，探针升 v5 语义）**：`Log` 由 `Write-Output` 改 **`Write-Host`**（写宿主 stdout，**不进函数返回流** ⇒ 不污染返回值）；`PROBE_OUTPUT` / `RECONCILE_LINES` 两处 `Write-Output` 同改 `Write-Host`；`GetReconcileStat` / `GetSemanticStat` 各加**数组解包守卫**（`if ($line -is [array]) { $line = @($line)[-1] }` + `$null` 早退）⇒ 双保险。
- **修后验证（已验证）**：PowerShell 语法解析 **PARSE_ERRORS=0**；单测四态全对 —— 标量行 / 数组包裹行 解析结果**一致且非 -1**，`$null` 输入返回 `-1` 哨兵；文件 **LF=413 / CRLF=0 / 无 BOM**（行尾与 BOM 约定未破坏），`Write-Output` 残留 **0** 处、`Write-Host` 4 处。
- **注意（已验证）**：`Write-Host` 文本经 `pwsh -File … 2>&1 | Tee-Object` **仍会落盘**（本机实测 `HOST_LINE` 进入 tee 文件）⇒ **不影响「日志落盘」这一原始诉求**，只切断返回流污染。
- **入库状态**：`reports/fluxing-br-1c/probe-ac09.ps1` 属 `reports/**` ⇒ 按红线**不入库**（本机工作副本已改好，下一轮直接用）；若换机器 / 从干净检出重跑，需按下表**重放这 6 处改动**。
- **重放清单（跨机复现用）**：① `Log`：`Write-Output` → `Write-Host`；② `Write-Output $probeOut.TrimEnd()` → `Write-Host …`；③ `@($r.Reconcile) | ForEach-Object { Write-Output $_ }` → `Write-Host $_`；④ `@($r.Semantic) | …` 同改；⑤ `GetReconcileStat` 头部加数组解包 + `$null` 早退；⑥ `GetSemanticStat` 头部同加。
- **遗留**：探针**本轮未在生产上复跑**（修复属于收尾轮的额外动作，未获授权再动真机侧车 / 生产）⇒ 修复**未在真实轮次里端到端验证过 per-stage 明细是否落盘**（仅单测 + 语法 + tee 落盘实测）⇒ 下一轮复跑时**必须先看日志行数**：预期从 9 行涨到数十行且含 `STAGE_SUMMARY[Legacy]` / `STAGE_SUMMARY[Deleted]`。

**§K-5 待观察风险（未验证）**：探针 FAIL 路径恢复的 5 MB 侧车**不含**客户端 08:19:37 自建侧车时抬高的本地 HLC 水位 ⇒ 下一轮客户端推送可能被服务端 HLC 守卫跳过（云端胜）⇒ 存在「静默不推送」风险 ⇒ 需在下一轮客户端活动后复查 `pushed=` 是否恢复非零。

**§K-6 本轮新增工具坑（§J-6 不重复）**：
- **`wrangler d1 execute` 偶发 `fetch failed` 网络抖动** ⇒ 关键读数（残留行数）必须**重跑确认**，不能拿失败输出当 0（本轮已对 `sync_buckets` 残留重跑确认 = 0）。
- **迁移只 apply 一次**：0004 建表**无 `IF NOT EXISTS`**，重复 apply 会报 duplicate table。
- **`wrangler d1 migrations list` 按版本号升序输出**，判「未 apply」看**末尾**；**`wrangler deployments list` 从最旧开始**，查生效 Version 要用 `deployments status`。
- **`npm run migrate:remote` 不可用**（用库名 `fluxing-sync` 而非绑定名 `DB`）⇒ 一律 `wrangler ... DB --remote --config`。
- **PowerShell 自定义函数名带连字符并立即带参调用**（`function Add-Line([string]$s){...}` 后 `Add-Line ("...")`）被解析成类型转换 ⇒ 报「“(” 后应为表达式」，整段脚本未执行 ⇒ 改用 `Add-Content -LiteralPath $log -Value "..." -Encoding utf8` + `$ts = Get-Date -Format 'yyyy-MM-ddTHH:mm:sszzz'`。
- **同一条命令里既 `Set-Location` 又带删除类目标** ⇒ 被本地安全网关整体拦截（`write/delete targets cannot be verified safely after a command working directory change`）⇒ 删除 / 清理须**单独成一条命令**且不改工作目录。

**§K-7 三态标注**：

| 状态 | 内容 |
|---|---|
| **已验证** | STEP 0 额度活性（无 `code: 7500`）/ STEP 2 迁移 0003+0004 MIGRATE_EXIT=0 且 num_tables 8→9 / STEP 3 三条 EXPLAIN 全命中新索引 / STEP 4 DEPLOY_EXIT=0 + 新生产 Version `4f163326-f2a5-4c69-be56-24cb800e6915` + D8 双绑定回读 / STEP 5 桶端点冒烟 9/9 PASS + HLC 守卫生效 + cipher 原样往返 + 跨设备可见 + 清理零残留 / STEP 1 AC-09 复跑 **FAIL**（两段硬数据）/ `sync_items` COUNT 40,061 未变 + MAX(updated_at) 冻结 08:15:19 / 客户端 exe sha256 与 a-9 版期望值逐字相等 / **探针根因差分复现坐实**（DIRTY/CLEAN 两版对照）**+ 修复后语法 0 错 + 单测四态全对 + tee 落盘实测** / **本轮未改任何产品代码**（工作树仅 `m librime` + 既有 untracked；探针属 `reports/**` 不入库） |
| **推断** | `rows_written_24h` 增量来自 CREATE INDEX 扫描的延迟上报（非写放大回流）/ AC-09 整体 FAIL 的唯一触发项是 `push=1` |
| **未验证** | 探针修复**未在真实轮次端到端复跑**（下一轮复跑时看日志行数是否从 9 涨到数十行 + 是否出现 `STAGE_SUMMARY[Legacy/Deleted]`）/ Deleted 段 `push=1` 的来源 / 侧车 HLC 水位丢失是否导致下一轮「静默不推送」 / 客户端 54,536 条从未上云条目的补推洪峰与分批策略 / BR-①g 客户端桶化的实际产出 / ⑤ 腾讯云两项安全问题与 ⑥ 两任务合并（§J-8 仍待用户拍板） |

**§K-8 待用户裁定（✅ 探针项已闭合；剩 1 项影响下一轮是否开工）**：
1. **BR-①g 闸门（唯一未决项）**：是否接受「Deleted 段零写入（a-7 字面判据成立）+ Legacy 段 8 行属正常增量」作为开闸依据，让下一轮启动客户端桶化（**推荐**），或维持严格判据（AC-09 整体 PASS）并要求用户空闲时复跑 AC-09。
2. ~~**探针缺陷修复归属**~~ → **已闭合（2026-11-25 收尾轮自行修复，见 §K-4）**：根因差分复现坐实 + 6 处定点修改 + 语法/单测/tee 三重验证通过 ⇒ **下一轮复跑前探针已是可用状态**，不再需要用户拍板「谁去修」；但**探针文件属 `reports/**` 不入库** ⇒ 换机需按 §K-4「重放清单」重放 6 处改动。

**§K-9 收尾轮自行动作说明（透明化）**：本轮（STEP 7 收尾）在用户未拍板的情况下**自行修了探针**（`reports/fluxing-br-1c/probe-ac09.ps1`）⇒ 依据：根因已由差分复现坐实、修改范围仅限测试脚本、**不触碰任何产品代码与生产**、可逆（探针不入库、无生产副作用）⇒ 属「明显 bug 直接按推荐方案修」范畴，**不构成对 BR-①g 闸门的放宽**（客户端代码一字未动）。

**§K-10 ADR 0077 状态说明**：本轮**未改任何 ADR**（`docs/adr/0077-bucket-level-sync-storage-architecture.md` Status 仍为 Proposed）——服务端桶化已在生产生效并通过冒烟，但 ADR 0077 的 D9 / D10 / D11 涉及客户端侧分桶打包与侧车 v2，**BR-①g 未开工** ⇒ 是否转 Accepted 待 BR-①g 完成后按 ADR 自身的验收条件判定（**推断**，ADR 内验收条款未在本轮逐条复核）。

---

## M · 批次 BR-①g（客户端桶化）· 文档与设计轮（2026-11-25）

**本节性质** = **客户端桶化（BR-①g）的文档与设计轮**：落盘 ADR 0077 客户端段（D12–D28 + 四段退役路线 + 回滚预案 + 补推分批策略）、spec 103 三件套、handoff §M、接力 prompt、`CLAUDE.md` BR 指针。

**§M 段落成稿时（2026-11-25 上午）客户端尚未入库**；截至本轮收口：客户端实现提交 `f7a62f6`、文档提交 `fdc71b9`，**均已推送**（远端 `kizemo/feat/100-cloud-sync` HEAD = `fdc71b9`）；门禁**已跑**（`MSBUILD_EXIT=0` / `PASS=899 FAIL=0` / `UFFFD_COUNT=0` / `VCPROJ_REGISTRATION=OK`）；**服务端仍零改动**（`cloud/**` 零改动、**未部署**）；出包 / 装机 / 真机轮结论见 §M.7。
**客户端实现已入库并通过编译与单测**（`f7a62f6`：新增 `WeaselServer/Cloud/CloudBucketSync.{h,cpp}` + 改 `CloudSyncService.{h,cpp}` + 两个 vcxproj 登记 + 7 组新单测；门禁 `PASS=899 FAIL=0`，基线 813 ⇒ **+86**）⇒ **但真机轮尚未跑** ⇒ 本节仍**不含**任何真机轮「已验证 / PASS」类结论；凡真机轮未产生实测数字处一律留 `TODO(主会话回填): <字段名>` 显式占位。
**§L 由另一回检任务占用**，本节不触碰。
**§K-10 记的「ADR 0077 Status 仍为 Proposed」是当时状态**；本轮已将 ADR 0077 Status 转 `Accepted`（服务端部分已部署生产）——**转 Accepted 不代表客户端功能已验证**（见 §M.6 第 1 行）。

### M.1 本批目标与范围（10 项清单 · 逐条对应 spec 102 `tasks.md` §8）

| # | spec 102 §8 条目 | 本批落点 | 本轮状态 |
|---|---|---|---|
| 1 | 客户端分桶打包 / 拆包、桶级 HLC 分配、`bucket_key` 计算 | spec 103 `tasks.md` **T2 / T3 / T4** | 进行中（`.cpp` 已落盘 8 个纯函数，**未编译验证**） |
| 2 | 侧车 v2 升版与 `seq = 0` 强制重置 | **T7**（**偏离**：强制重置落在新增 `bseq`，老 `seq` 不归零） | 待开工 |
| 3 | 桶内条目上限 4096 与「超限降级老端点」 | **T11** | 待开工 |
| 4 | 桶大小 / 分片数取值调参与 R2 吞吐实测 | 取值 = **T2**（64 × 16 = 1024 桶）；吞吐实测 = **T16**（真机轮回填 AC-212） | 取值进行中 / 实测待开工 |
| 5 | `healed` 自愈粒度变粗的部分恢复方案 | **T12** | 待开工 |
| 6 | 出包装机与真机判据 | **T16**（AC-09 判据仍由既有定时任务 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` 负责，本批**不改**判据） | 待开工 |
| 7 | `sync_items` 表与老端点的退役（**四段 + 回滚预案；只做「切读」就下老端点 = 不合格**） | **T5 / T10** 交付阶段状态机与回滚路径；四段路线 = 双写 → 校验 → 切读 → 退役（ADR 0077「四段退役路线 + 回滚预案」）；**服务端端点下线另立决策记录 + 需用户单独授权**（spec 103 Q3） | 待开工 |
| 8 | `devices.last_seen` 读路径写占比上升的治理 | **不属本批**（登记为已知边界；阶段 3/4 才下降） | 不做 |
| 9 | 写预算护栏改「全局口径 / 按活跃账号分摊」 | **不改实现**（已拍板维持每账号口径；BR-② 不立项，仅作已知边界登记） | 不做 |
| 10 | 是否升级 Workers Paid | **不升级**（已拍板暂不升级）；**开工前须先定补推分批策略** —— 已定：轮内分批（≤ 64 桶/请求）+ 失败续传，**不需要分日 pacing**（ADR 0077 D20 / D22） | 已定 |

**本批只改客户端**：`cloud/fluxing-sync/**` 零改动；服务端桶级端点已在生产（见 §K STEP 4 / STEP 5）。
**本批不改既有判据**：AC-09 仍为 **FAIL**，**不改记 PASS**（§M.5）。

### M.2 已定设计决策（13 条）

1. **阶段状态机 0–4**：0 = 老端点单跑（**默认**）｜1 = 双写｜2 = 双写 + 交叉校验｜3 = 切读（老端点完全不发）｜4 = 老端点退役。
2. **生效口径**：环境变量 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` **优先** → 侧车 `phase` → 默认 0；环境变量生效时**写回侧车** ⇒ **回滚 = 设环境变量为 0 跑一轮**（不改代码、不重装、不删数据）。
3. **阶段不得自动递增**：每次跃迁必须有人工判据。
4. **桶键派生**：用**加密域** HMAC 摘要字节（**弃用**明文 `sha256(item_key)`，可被字典穷举反推）；独立 HKDF 标签 `fluxing-bucketkey-mac-v1`（**不**与 `fluxing-itemkey-mac-v1` 复用）；桶键 = 64 位小写 hex；桶键输入带词典名长度前缀消歧。
5. **分桶粒度**：一级 64 × 二级 16 = **1024 桶 / 词典**（`64` 是**分片数**，**不是**每桶条目数上限）⇒ 每桶 ≈ **92** 条（本地全集 94,604 ÷ 1024；未上云口径 53 条），远低于服务端 `MAX_BUCKET_ITEMS = 4096`。
6. **桶级 HLC** = 桶内条目 HLC **最大值**，且必须**严格大于**侧车已存桶 HLC（`MakeHlcStrictlyGreater` 抬升）⇒ 否则真实变更被服务端守卫整桶 `skipped`。
7. **桶内容指纹用语义口径**（`s2:` / `SemanticHashHex` 与条目级侧车同源）⇒ **不用**密文哈希 / 整条 payload 哈希（否则 rime 每轮回写重算 `d=`/`t=` ⇒ 每轮重推，写放大复发）。
8. **桶内容 = 合并后本地视图的全集**按 (词典名, 分片) 分组后的**整桶**（服务端是 `DELETE+INSERT` 覆盖语义 ⇒ 只推变更条目会丢同桶其它条目）；**只推指纹变化的桶** ⇒ 稳态零写；没推成功的桶**不写 `brow`** ⇒ 失败续传。
9. **桶 blob 零新增密码学**：N 个既有 8 行信封文本块**直接串接**，不新增桶级 nonce / 桶级 AAD ⇒ 条目级可验证性逐字不变；拉取解包后**复用同一解密路径**（`OpenItemsTolerant` 语义，不新写第二套解密实现）。
10. **桶级 pull 解析器独立实现**：桶对象字段 = {桶标识, 设备标识, 桶级时序, 桶密文, 条目数, 服务端序号}，**无 nonce** ⇒ 不可复用 `ParsePullResponse`（它在 `CloudSyncService.cpp:264` 对每个条目强制取 `nonce`）⇒ 喂老解析器**必失败**（AC-207 / AC-108）。条目级解析器**逐字不动**。
11. **分批参数**：推 ≤ **64 桶 / 请求**（服务端 `MAX_PUSH_BUCKETS`）⇒ 1024 桶首导 = 16 个请求；拉 ≤ **100 桶 / 页**；**单轮页数上限 = max(16, 词典数 × 11 + 2)**，且**不超过 32 页**（硬上限）⇒ 单词典 16 页可一轮扫完 1024 桶；词典数 ≥ 3 时触到 32 页封顶 ⇒ 4 词典及以上可能扫不完库存（待实测，见 §M.5）。
12. **超限降级**：单桶 `item_count > 4096` 或 blob > **4 MiB** ⇒ **该桶**改走条目级端点推，其余桶不受影响，整轮不失败。
13. **侧车 v2**：头行 `#fluxing-cloud-sync-v2`；保留 v1 的 `seq` / `keyfp` / `row`（语义逐字不变）；新增 `bseq`（**独立编号空间、强制从 0 起**）/ `phase` / `brow`（`bucket_key` / `ms` / `counter` / `device` / `item_count` / `fingerprint`）；**必须能读 v1**（v1 ⇒ 阶段 = 0、`bseq` = 0、无 `brow`）。

### M.3 偏离 ADR 0077 原文之处及理由（6 条）

| # | ADR 0077 原文 | 本批口径 | 理由 |
|---|---|---|---|
| 1 | **D2**：分片用**明文**摘要（`sha256(item_key)[0] >> 2`） | 改用**加密域** HMAC 摘要字节 + **独立** HKDF 标签 | 明文摘要可被**字典穷举**反推「哪个词条落在哪个桶」⇒ 桶归属本身成侧信道 |
| 2 | **D2**：固定 **64** 个一级分片 | 64 × 16 = **1024 桶 / 词典** | 64 桶 ⇒ 每桶约 852 条 ⇒ R2 单对象 put 时长超客户端 30 s 超时窗（D9 实测换算）；1024 桶 ⇒ 每桶约 92 条，且 D1 首导仍只 1,024 行（远低于 10 万额度） |
| 3 | **D6**：侧车升版并**强制重置 `seq = 0`** | 保留 `seq` 给老端点 + **新增独立 `bseq` 从 0 起** | 四段渐进退役期间**老端点仍活跃**；重置 `seq` 会致老端点全量重拉重写 ⇒ 写放大回归。D6 原意（两编号空间互不解释）已达成 |
| 4 | **D9**：默认桶大小取 **64 条**（⇒ 852 桶） | 「64」是**一级分片数**，**不是**每桶条目数上限 ⇒ 1024 桶 / 词典 | D2 与 D9 混用了「分片数」与「每桶条目数上限」两个层级（**回答 spec 102 §9 Q1**）；D9 的「64 条 ≈ 7.5 s」保留为**推算档位**，不作本粒度实测值 |
| 5 | **D9**：单轮拉取覆盖 **64 桶** / 固定 16 页 | 单页 ≤ 100 桶；**单轮页数上限 = max(16, 词典数 × 11 + 2)**，硬上限 32 页 | 固定页数在多词典时**扫不完库存** ⇒ 客户端「库存未证实」闸门**永久压制推送**（新词条永远上不了云） |
| 6 | **D9 表格**的「64 条/桶 ≈ 7.5 s」 | 本粒度（≈ 92 条/桶）单轮时长**未实测** ⇒ **记「未判定」** | D9 的 7.5 s 是**另一粒度**的换算值，不可直接引用；AC-212 需真机轮实测（§M.5） |

### M.4 写入量两个口径（**不可混用**）

| 口径 | 桶化前 | 桶化后（设计值） | 改善 |
|---|---|---|---|
| **D1 索引行数**（写预算护栏计的行） | 54,536 行（`sync_items`） | **1,024 行**（`sync_buckets`，1 设备 × 1024 桶） | ≈ **53 倍** |
| **D1 实际写行数**（首导，R2 `put` ⇒ `DELETE+INSERT`） | ≈ 54,536 行 | ≈ **2,048 行**（1,024 桶 × 2） | ≈ **27 倍** |
| **单请求**索引写行数上界 | — | ≤ **2 × 本轮桶数** = **128**（64 桶/请求） | — |

- 两口径**分别**断言（AC-204），**不得**只测其一；「实际写行数」含 `DELETE+INSERT` 两条写入，故不等于索引行数。
- 「53 倍 / 27 倍」是**设计值**（按 1024 桶与 2× 上界推算），**不是实测**；真机轮的实测写增量 = `**+22 行**（阶段 1 实测：`rows_written_24h` 40,259 ⇒ 40,281；**桶推一次都没发**，这 +22 不是桶写）⇒ 桶写入量仍是**未判定**（见 §M.8）`。
- 首导对象数为**本地全集 94,604 条**（其中未上云 54,536 条）⇒ 每桶 ≈ 92 条（未上云口径 53 条）。

### M.5 未验证 · 未闭环清单

| # | 项 | 现状 / 缺什么 |
|---|---|---|
| 1 | **客户端桶化实现** | **已入库并编译通过**：实现提交 `f7a62f6`（已推送）；`CloudBucketSync.{h,cpp}` 新增 + `CloudSyncService.{h,cpp}` 改造 + 两个 vcxproj 登记 + 7 组新单测（`PASS=899 FAIL=0`）；本节客户端条目**已达「实现已落码 + 单测已绿」**，但**真机轮未验证** |
| 2 | **门禁数字** | **已跑**（2026-11-25 独立复跑 `scripts/verify/verify-br1g-bucket.ps1`）⇒ `MSBUILD_EXIT=0`（TestFluxingCloud **独立 msbuild** + 显式 `/p:SolutionDir=`，`weasel.sln` 不含该工程） / `PASS=899 FAIL=0`（基线 813 ⇒ **+86**，只增不减） / `UFFFD_COUNT=0` / `VCPROJ_REGISTRATION=OK` / clang-format **改动 hunk 漂移 0**（整文件口径 `TestFluxingCloud.cpp` 261 = **既有基线**，非本批引入） |
| 3 | **出包 / 装机** | 本轮**未出包、未装机** ⇒ `1.3.9 包 **43,023,508 B**（`output/archives/fluxing-1.3.9-installer.exe`）· `BUILD_EXIT=0` · `INSTALL_EXIT=0`（验收 C1–C4 全 PASS）· 三处 `WeaselServer.exe` sha256 **全等** = `BBBDC47C646E83A8A681718255E3F2B90C768F77472E09064310995332F17930`（2,894,848 B）· `verify-installed.ps1` ⇒ `ok=True problems=0`（**须用 pwsh 7**，5.1 会把 UTF-8 无 BOM 按 ANSI 读 ⇒ CJK 解析错）` |
| 4 | **真机轮** | 本轮**未跑真机轮** ⇒ AC-201…AC-211 均**未在真机验证** ⇒ `**部分达成**：阶段 0 基线 + **阶段 1 双写**已在真机跑出诊断行（`bucket phase=1 pushed=0 skipped=0 pulled_buckets=0 pulled_items=0`、`LEGACY_PUSH_LINES=0`、`VERDICT=OK`）；**阶段 2 未观测到**（阶段机在等下一轮时被终止）；**阶段 3/4 未执行（主动拦截）**；**桶推一次都没发**（生产 `sync_buckets` 恒 0）⇒ AC-201…AC-211 **多数仍未在真机验证**（根因与处置见 §M.8）` |
| 5 | **AC-112 单轮预估传输时长** | **未判定**（需要什么：真机轮的 `elapsed_ms` 序列 + 生产 D1 写增量读数 + 真实桶 blob 字节数抽样）；**不得**用 D9 的 7.5 s 推算值或合成 blob 压测代替 |
| 6 | **AC-09** | 仍为 **FAIL**（`AC09_VERDICT=FAIL`）⇒ **不得改记 PASS**；本批不改其判据 |
| 7 | **服务端存量** | `sync_items` 仍 **40,061 行**未迁桶 ⇒ 老端点仍在承载（桶化只影响**新写**路径） |
| 8 | **多词典硬上限** | 词典数 ≥ 3 时单轮页数触到 **32 页**封顶 ⇒ 4 词典及以上可能扫不完库存 ⇒ 需**实测本机常见词典数**（硬上限是客户端常量，调整不需服务端改动） |
| 9 | **R2 孤儿对象** | 生产 R2 **无生命周期规则** ⇒ 只删 D1 索引行会留孤儿对象；本机 `wrangler` 无 `r2 object list` ⇒ **无法证实也无法证伪**（未闭环） |
| 10 | **Deleted 段 `push=1` 来源** | 未定位（探针 per-stage 明细缺失，见 §K-4） |
| 11 | **侧车 HLC 水位** | 探针 FAIL 路径恢复的 5 MB 侧车**不含**客户端自建侧车时抬高的本地 HLC 水位 ⇒ 存在「静默不推送」风险，**未验证** |
| 12 | **探针修复** | `reports/fluxing-br-1c/probe-ac09.ps1` 的 6 处修复**未在真实轮次端到端复跑**（下一轮复跑须先看日志行数是否从 9 涨到数十行） |
| 13 | **客户端侧补推洪峰** | 已升级为**阶段 3/4 的硬前置**：阶段 3 关掉逐条端点后，桶路径会一次性放行约 **94,604 条**（≈ 190,000 行写入）⇒ 击穿 8 万行/账号预算与 D1 免费额度 ⇒ **已在阶段 3 开跑前主动拦截**（生产 `sync_buckets` 恒 0）；两条补齐规则见 §M.8.4 |
| 14 | **R2 闸门未进桶路径** | `PushChangedBuckets`（`CloudSyncService.cpp:1954-1968`）对 `merged` **全部**条目分桶，**未**继承 ADR 0076 D2-5a-2 的「库存未证实不推送」⇒ 54,536 条 `deferred` 会被打包上云 ⇒ **已定位、未修**（§M.8.3） |
| 15 | **每轮桶推配额闸门** | 缺失 ⇒ 无分批上传能力；**好消息**：桶行「本轮没推的保留旧指纹」（`CloudSyncService.cpp:2168-2185`）⇒ 加配额闸门即可天然续推，**不需要另造水位**（§M.8.4） |

### M.6 每条标注：已验证 · 推断 · 未验证

| 项 | 状态 | 依据 |
|---|---|---|
| ADR 0077 Status 转 `Accepted`（**服务端部分**已部署生产） | **已验证** | §K STEP 4（`DEPLOY_EXIT=0`，生产 Version `4f163326-f2a5-4c69-be56-24cb800e6915`）+ STEP 5（桶端点冒烟 9/9 PASS） |
| **「转 Accepted 不代表客户端功能已验证」** | **已验证**（文档事实） | ADR 0077 Status 处已明写；客户端代码未编译、未入 git、未进 `.vcxproj` |
| 服务端契约常量（`MAX_PUSH_BUCKETS=64` / `MAX_PULL_LIMIT=100` / `MAX_BUCKET_ITEMS=4096` / `MAX_BUCKET_BLOB_BYTES=4 MiB` / `MAX_BUCKET_KEY_BYTES=512`） | **已验证** | `cloud/fluxing-sync/src/wire.ts:233/243/246/256`、`src/sync-buckets.ts:43` |
| 客户端既有分页 / 分批常量（`kPullPageLimit=100` / `kMaxPullPages=10` / `kServerMaxPushItems=64` / `kPushBatchSize=32`） | **已验证** | `CloudSyncService.cpp:877/879`、`CloudSyncService.h:59/65` |
| 桶响应**无 `nonce`** ⇒ 不可复用 `ParsePullResponse` | **已验证** | `CloudSyncService.cpp:193`（`ParsePullResponse`）/ `:264`（逐条目强制取 `nonce`）；ADR 0077 D26 桶对象字段集 |
| 侧车 v1 现状（`BuildSidecarText` / `ParseSidecarText` / `ResetCloudSyncSidecarFile`） | **已验证** | `CloudSyncService.cpp:1668` / `:1696` / `:1760` |
| `CloudBucketSync.{h,cpp}` 已落盘、`.cpp` 缺 4 个实现、未进 `.vcxproj` | **已验证** | grep 结果（`ParseBucketPhaseValue` / `ResolveBucketPhase` / `BuildBucketPushBody` / `ParseBucketPullResponse` 无命中）；`Select-String 'CloudBucketSync' WeaselServer.vcxproj` 无输出 |
| **每桶 ≈ 92 条**（本地全集 94,604 ÷ 1024；未上云口径 53 条） | **已验证**（口径为派生值） | 94,604 = 本地词典条目行数（§F 快照实测 4 头 + 94,604 数据行）；54,536 = reconcile 的 `deferred` 计数 |
| 「1024 桶 ⇒ 写入量改善 ≈ 53 倍 / 27 倍」 | **推断**（设计值，非实测） | 由 1,024 索引行与 2 × 桶数的 `DELETE+INSERT` 上界推算；真机实测 = M.5 第 13 项 |
| 「多词典时固定页数会扫不完库存 ⇒ 永久压制推送」 | **推断**（机理成立，未实测） | 「库存未证实」闸门语义（ADR 0076 D2-5a R2）；4 词典及以上的实际词典数**未实测** |
| 单轮预估传输时长 | **未判定** | AC-212；见 M.5 第 5 项 |
| 客户端桶化代码可编译 / 单测绿 / 稳态零写 / 桶响应独立解析 | **未验证** | 代码子会话仍在实施；本节不含任何该类结论 |
| AC-09 | **未验证**（判据仍 **FAIL**） | §K STEP 1（`AC09_VERDICT=FAIL`） |
| 图谱降级 | **已验证**（降级理由成立） | `codebase-memory-mcp` 对 `WeaselServer/Cloud/**` 的 `search_graph` / `search_code` / `query_graph` 均**不命中** ⇒ 该目录**未入图谱索引** ⇒ 按 `.claude/rules/codebase-graph.md` 降级到 Grep / Read 取证 |

### M.7 证据路径（均为仓库根相对路径，已逐个 `Test-Path` 通过）

**设计与规格**
- `docs/adr/0077-bucket-level-sync-storage-architecture.md`（客户端段 D12–D28、四段退役路线 + 回滚预案、补推分批策略、偏离登记、AC-112 四项数值）
- `docs/adr/0076-cloud-write-amplification-governance-and-zero-write-registration.md`（D2-5a-9 语义指纹；写放大根因）
- `.specify/specs/103-client-bucket-sync/spec.md`（FR-201…FR-216 / AC-201…AC-212 / Q1…Q4）
- `.specify/specs/103-client-bucket-sync/plan.md`（Constitution Check、数据流、侧车 v2 格式、逐文件改造、参数速查）
- `.specify/specs/103-client-bucket-sync/tasks.md`（T1…T16，含与 spec 102 §8 的 10 项对应）
- `.specify/specs/102-bucket-level-sync-storage/tasks.md`（§8「本批不做什么」10 项原文）

**客户端代码（实现由另一写入者推进，本轮未改）**
- `WeaselServer/Cloud/CloudBucketSync.h`（已声明常量 / 五阶段枚举 / 线格式；含**未实现**声明）
- `WeaselServer/Cloud/CloudBucketSync.cpp`（已实现 8 个纯函数；缺 4 个实现）
- `WeaselServer/Cloud/CloudSyncService.h`（`kServerMaxPushItems` / `kPushBatchSize` / `OpenItemsTolerant` / `SemanticHashHex`）
- `WeaselServer/Cloud/CloudSyncService.cpp`（`ParsePullResponse` / 分页常量 / 侧车 v1 三函数）
- `WeaselServer/Cloud/CloudUserDictIo.cpp`（payload 装配与切分）
- `WeaselServer/Cloud/CloudTransport.h`（客户端单次超时窗口 30 s）
- `include/FluxingCloudEnvelope.h`（条目级附注，本批**零改动**）

**服务端（已部署生产，本批零改动）**
- `cloud/fluxing-sync/src/sync-buckets.ts`（桶级端点 + 分页常量）
- `cloud/fluxing-sync/src/wire.ts`（桶级契约常量）
- `cloud/fluxing-sync/src/account.ts`（账号数据清除：双前缀 + 自证）

**取证与探针（`reports/**`、`scripts/verify/**` 按红线**不入库**）**
- `reports/fluxing-br-1g/interim-state.md`（本轮中间状态与取证台账）
- `reports/fluxing-br-1f/recon-blob-arch.md`（服务端改造清单与风险）
- `reports/fluxing-br-1c/probe-ac09.ps1`（AC-09 探针，含 §K-4 的 6 处修复）
- `scripts/verify/verify-br1g-bucket.ps1`（客户端门禁一键脚本）
- `scripts/verify/probe-br1g-bucket-phase.ps1`（真机阶段切换探针）

**本批产物**
- `docs/archive/handoffs/handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（本文件 §M）
- `docs/archive/prompts/prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（接力 prompt，就地改写）
- `CLAUDE.md`（BR 指针追加 BR-①g 轮次要点）



---

## M.7 主会话收口轮（2026-11-25 上午 · 客户端桶化落地 + 门禁 + 出包装机）

> 本节由**主会话**回填，全部数字标注 **已验证 / 推断 / 未验证**。
> §M 主体（§M.1–§M.6）由文档写入者成稿，本节只追加「落地后的事实」，**不回改 §M.1–§M.6 的历史陈述**。

### M.7.1 本轮实际改动与入库（已验证）

| 项 | 值 |
|---|---|
| 客户端实现提交 | **`f7a62f6`** `feat(WeaselServer): 客户端桶化同步 —— 分桶打包/桶级 HLC/侧车 v2/阶段机`（7 文件 **+2,138 / −48**） |
| 文档提交 | **`fdc71b9`** `docs(fluxing): BR-①g 客户端桶化 —— ADR 0077 客户端段 D12-D28 + spec 103 三件套 + 交接 §M`（7 文件 **+1,187 / −6**） |
| 推送结果 | 直连 `PUSH_EXIT_DIRECT=128`（`Recv failure: Connection was reset`）⇒ 回退代理 `PUSH_EXIT_PROXY=0` ⇒ 远端 `kizemo/feat/100-cloud-sync` HEAD = **`fdc71b9`** |
| 服务端 | **零改动**（`cloud/**` 未动）、**未部署**（生产 Version 仍 `4f163326-f2a5-4c69-be56-24cb800e6915`） |
| 客户端改动文件 | 新增 `WeaselServer/Cloud/CloudBucketSync.{h,cpp}`；改 `WeaselServer/Cloud/CloudSyncService.{h,cpp}`、`WeaselServer/WeaselServer.vcxproj`、`test/TestFluxingCloud/TestFluxingCloud.{cpp,vcxproj}` |

新增单测 7 组（`TestFluxingCloud.cpp` `:5246/:5296/:5356/:5404/:5440/:5515/:5597`，`main()` 接入 `:5712-5718`）。

### M.7.2 门禁实测数字（已验证 · 主会话独立复跑）

复跑命令：`pwsh -File F:\soft\00selfmade\rime_claude\scripts\verify\verify-br1g-bucket.ps1`
（日志落盘：`reports/fluxing-br-1g/verify-main.log`）

| 指标 | 值 | 备注 |
|---|---|---|
| `MSBUILD_EXIT` | **0** | `TestFluxingCloud` **必须单独 msbuild** 且显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`（`weasel.sln` 不含该工程 ⇒ 否则跑旧 exe、PASS 恒 813 **假绿**） |
| `EXE_EXIT` | **0** | 新 exe 真跑，未复用上一轮 |
| 单测 | **`PASS=899 FAIL=0`**（`VERDICT: ALL PASS`） | 基线 **813** ⇒ **+86**，只增不减；另有回环回归 `PASS=11 FAIL=0` |
| `UFFFD_COUNT` | **0** | 5 个改动文件均无替换字符 |
| `VCPROJ_REGISTRATION` | **OK** | 两个 vcxproj 均登记 `CloudBucketSync.cpp` + `.h` |
| clang-format | **改动 hunk 漂移 0** | `git clang-format --diff HEAD~2` ⇒ `clang-format did not modify any files`；4 个客户端文件**整文件 0 替换**；`TestFluxingCloud.cpp` 整文件 **261 替换 = 既有基线**（不在本批改动 hunk 内 ⇒ 非本批引入） |

### M.7.3 门禁脚本自身口径缺陷（本轮发现并已修 · 已验证）

`scripts/verify/verify-br1g-bucket.ps1` 原第 124–126 行：
`$formatted = & $cf.Source --style=file $f` ⇒ 返回 **`string[]`**，却与 `[IO.File]::ReadAllText($f)`（`string`）做 `-ne` 比较 ⇒
**每个文件都被判为漂移** ⇒ `CLANGFORMAT_DRIFT=5`（= 文件数）是**假红**，与「子会话自报 0 漂移」并不矛盾。

已修为：`$formatted` 先 `-join [char]10`、原文先剔除 `CR` 再比较，并增打两个分口径：
`CLANGFORMAT_WHOLEFILE_DRIFT=<n>`（整文件，含既有基线）与 `CLANGFORMAT_HUNK_DRIFT=<n>`（**改动 hunk 口径 = 行动口径**）。
**未入库**（`scripts/verify/*.ps1` 按红线不入库）⇒ **换机须按 §K-4 惯例重放该脚本修正**。

### M.7.4 版本升版口径事故与修正（已验证 · 本轮真实踩坑）

`install.nsi:68` 的 `OutFile "archives\fluxing-${FLUXING_VERSION}-installer.exe"` ⇒ **归档名只吃 `FLUXING_VERSION`**。
而版本钉分布在 **`env.bat` 四处**（`FLUXING_VERSION` / `WEASEL_BUILD` / `PRODUCT_VERSION` / `FILE_VERSION`）+ `VERSION_MAJOR/MINOR/PATCH` + 仓库根 `version.txt`。

**首轮只改了 `VERSION_PATCH`（8→9）与 `PRODUCT_VERSION`（1.3.8.0→1.3.9.0）** ⇒ 构建日志实测
`PRODUCT_VERSION=1.3.9.0` 而 **`FLUXING_VERSION=1.3.8` / `FILE_VERSION=1.3.8.0`** ⇒
① 产物名仍是 `fluxing-1.3.8-installer.exe`；② Windows 版本资源仍 1.3.8 ⇒ **装机走不到「严格更高版本」升级路径**；
③ **覆盖了上一轮（BR-①c）的 1.3.8 归档**（43,001,594 B → 43,012,822 B，BR-①c 那个包已不可复现）。

**修正**：`env.bat` 的 `FLUXING_VERSION` ⇒ `1.3.9`、`FILE_VERSION` ⇒ `1.3.9.0`（连同已改的 `VERSION_PATCH=9` / `PRODUCT_VERSION=1.3.9.0` 与 `version.txt=1.3.9`）⇒ **重打 1.3.9**。
⇒ **给下一轮的硬规则：版本升版必须改 `env.bat` 四个钉 + `version.txt` 五处，只改 `VERSION_PATCH` 是半升版。**

### M.7.5 出包 / 装机（已验证）

| 项 | 值 |
|---|---|
| 构建 | `build.bat weasel installer`（`RELEASE_BUILD=1` 已在 `env.bat`）⇒ 首轮 `BUILD_EXIT=0`（**半升版产物，已作废**）；1.3.9 重打中 |
| 归档 | `output/archives/fluxing-1.3.9-installer.exe` = **43,023,508 B**（`build.bat weasel installer`，`BUILD_EXIT=0`；日志 `reports/fluxing-br-1g/build-package-1.3.9.log`） |
| 装机 | **`INSTALL_EXIT=0`**（`scripts/install/silent-install.ps1 -InstallRoot "D:\Program Files\fluxing"` ⇒ `=== SILENT INSTALL OK - ALL HARD CHECKS PASSED ====`，验收 C1–C4 全 PASS；已装 `DisplayVersion=1.3.9`；`RimeUserDir` 已保存/恢复 = `D:\Program Files\fluxing\user1\fluxing`） |
| 三处 sha256 相等（构建=包内=装机后） | **全等** = `BBBDC47C646E83A8A681718255E3F2B90C768F77472E09064310995332F17930`（2,894,848 B）：构建 `output/Win32/WeaselServer.exe` = 包内（`output/7z.exe` 解包出的 `$TEMP\fluxing-staged\WeaselServer.exe`）= 装机后 `D:\Program Files\fluxing\weasel\WeaselServer.exe`；整目录另跑 `verify-installed.ps1` ⇒ `ok=True problems=0 detail=安装状态与构建产物一致；DisplayVersion=1.3.9` |

### M.7.6 真机轮（**未判定** —— 本轮未跑）

阶段机 0→1→2→3 **尚未在真实轮次跑过**。机制已就位：`scripts/verify/probe-br1g-bucket-phase.ps1 -Phase <n>`
（写 User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` ⇒ 重启 `WeaselServer` ⇒ 等一轮同步 ⇒ 读 `%TEMP%\fluxing-profile-notify.log` 的
`bucket phase=.. pushed=.. skipped=.. pulled_buckets=.. pulled_items=..` 诊断行）。
**回滚口径**：把该环境变量设为 `0` 跑一轮即回到老端点（客户端会把 `phase` 写回侧车）。

**未判据与「需要什么」**：
- AC-201…AC-211 ⇒ **未在真机验证** ⇒ **未判定**（真机轮未跑；判据清单与「需要什么」见 §M.7.6）；
- 真机轮 D1 写增量行数（AC-204「实际写行数」口径） ⇒ **未判定**（真机轮未跑；需要什么见 §M.7.6）；
- AC-112 单轮预估传输时长 ⇒ **仍未判定**（需要真机 `elapsed_ms` 序列 + 生产 D1 写增量读数 + 真实桶 blob 字节数抽样）；
- **53 倍 / 27 倍仍是设计值，不是实测**。

### M.7.7 本轮未闭环清单（逐条标注）

| # | 项 | 状态 |
|---|---|---|
| 1 | 真机阶段机 0→1→2→3 未跑 | **未判定**（探针就位，见 §M.7.6） |
| 2 | AC-112 单轮预估传输时长 | **未判定**（需真机回填） |
| 3 | 「53 倍 / 27 倍」写入量降幅 | **推断（设计值）**，非实测 |
| 4 | 生产桶表写入量回落 | **未验证**（桶表仍 0 行） |
| 5 | `R2` 生命周期规则缺失 ⇒ 孤儿对象不可证 | **未闭环**（`wrangler.toml` 无 `[[r2_buckets.rules]]`；清理必须走 `POST /v1/account/data`） |
| 6 | `device_id` 客户端一律不上报 | **推断**（服务端 body 省略 ⇒ 回落 `auth.deviceRowId`） |
| 7 | 换口令 ⇒ 旧桶成孤儿 | **已知边界**（既解不开也删不掉） |
| 8 | `brow` 丢失 ⇒ 不知服务端已存桶 HLC | **已知边界**（可能反复被整桶守卫 skipped） |
| 9 | 阶段 3/4 每轮约 11 个拉取请求 | **推断**（按轮计配额余量待真机确认） |
| 10 | BR-①f `AC-09` | 仍为 **FAIL** ⇒ **不得改记 PASS**（本批不改其判据） |
| 11 | 侧车 HLC 水位 / 探针修复端到端复跑 | 沿用 BR-①f 未闭环项 |
| 12 | 1.3.8 归档被覆盖（BR-①c 包不可复现） | **已发生**（见 §M.7.4） |

### M.7.8 本轮工具坑（已验证）

1. **`git clang-format` 只看改动行** ⇒ 整文件漂移 261 是既有基线，不要误判为本批引入。
2. **`CLANGFORMAT_DRIFT` 原为假红**（数组 vs 字符串比较）⇒ 已修（§M.7.3）。
3. **版本升版要改五处**（`env.bat` 四钉 + `version.txt`），只改 `VERSION_PATCH` ⇒ 半升版（§M.7.4）。
4. `install.nsi` 归档名**不含 `WEASEL_BUILD`** ⇒ 同 `FLUXING_VERSION` 会**覆盖**既有归档。
5. `wrangler d1 execute` 偶发 `fetch failed` ⇒ 关键读数必须重跑确认，不能把失败当 0。
6. 前台 bash 超过 60s 会被自动转成后台任务 ⇒ 长构建要用 `run_in_background` + `task_output` 等待式轮询。
7. 安全网关：同一命令既 `Set-Location` 又带写/删目标 ⇒ 整体拦截；全程用绝对路径、不切目录。

---

## L · 回检（2026-11-25 10:30）

**本节性质** = 定时回检任务 `312fd16e-d687-4973-b636-f1044ad9cdd0`（2026-11-25 10:30 触发）对主链 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`（2026-11-25 08:10:46 触发）的**只读判定 + 文档回写**。
本轮**未改任何产品代码**（`cloud/fluxing-sync/**` / `WeaselServer/**` 零改动）、**未重复主链工作**、**未自行启动主链**。

### L.1 CHAIN_STATUS = COMPLETED（步骤层全终态）/ AC-09 = FAIL（判据层未达成）

真相源 = 哨兵 `reports/fluxing-br-1f/chain-progress.log`，**逐行按内容解析**（未用 mtime 判新鲜度 —— `Tee-Object` 启动即截断文件，mtime 不可信）。

| 哨兵行 | STATUS | 关键字段 | 本次回检判定 |
|---|---|---|---|
| `STEP=0` | DONE | `PROBE=d1_migrations_rows=2 CODE=7500=no ROWS_READ_24H=531 ROWS_WRITTEN_24H=12` | 额度活性通过（**无** `code: 7500`） |
| `STEP=1` | DONE | `AC09_VERDICT=FAIL AC09_LOG=reports\fluxing-br-1f\ac09-run-20261125-081355.log LEGACY=twd=8,cd=0,push=1 DELETED=twd=0,cd=0,push=1 ROUND_RAN=both` | **AC-09 = FAIL**（见 L.2） |
| `STEP=2` | DONE | `MIGRATE_EXIT=0 APPLIED=0003,0004` | 迁移 0003+0004 成功 |
| `STEP=3` | DONE | `EXPLAIN_ITEMS=idx_sync_user_updated EXPLAIN_BUCKETS_USER=idx_sync_buckets_user_id EXPLAIN_BUCKETS_UPDATED=idx_sync_buckets_user_updated` | EXPLAIN **三条全命中新索引** |
| `STEP=4` | DONE | `DEPLOY_EXIT=0` | Worker 部署成功（生产 Version 见 §K） |
| `STEP=5` | DONE | `VERDICT_SMOKE=PASS SMOKE_EXIT=0 LOG=tmp\br-bucket-smoke-20261125.log` | 桶端点生产冒烟 PASS |
| `STEP=5_CLEANUP` | DONE | `VERDICT_CLEANUP=PASS RESIDUAL_USERS=0 RESIDUAL_BUCKETS=0` | 探针残留清零 |
| `STEP=6` | SKIP | `BLOCKED_BY=STEP1 REASON=AC09_VERDICT_FAIL NO_CLIENT_CODE_CHANGED` | 闸门未开（**设计内 SKIP**，非崩溃） |
| `STEP=7` | DONE | `HANDOFF_SECTION=K RELAY_PROMPT=… CLAUDE_MD_UPDATED=yes NO_PRODUCT_CODE_CHANGED=yes` | 收尾双产物已出 |
| `STEP=7B` | DONE | `PROBE_FIX_DOC_WRITEBACK COMMIT=0f36675 PARSE_ERRORS=0 UNITTEST=4CASE PASS` | 探针缺陷闭合（§K-4） |
| `STEP=BR1G` ×3 | START / RUN / **DONE** | `COMMIT_CODE=f7a62f6 COMMIT_DOCS=fdc71b9 GATE_PASS=899 GATE_FAIL=0 VERSION=1.3.9 PACK_BUILD=RUNNING REALMACHINE=NOT_RUN` | **BR-①g 客户端轮**（另一写入者，非本主链 8 段） |

**编号口径差异（必须记，避免下一轮误判）**：哨兵实际编号 = **迁移 `STEP=2` / EXPLAIN `STEP=3` / 部署 `STEP=4` / 冒烟 `STEP=5`**；
而本回检任务正文写的是「STEP=3 迁移 / STEP=4 EXPLAIN / STEP=5 冒烟」⇒ **回检正文错位一位**（主链 prompt 的步号与哨兵一致，错的是回检正文的语义标注）。
本节按**语义**登记（迁移 / EXPLAIN / 冒烟三项均存在且为 DONE），并同时保留哨兵原编号 ⇒ **无 MISSING**。
（两种口径交叉验证：按字面 `STEP=3/4/5` 取 = EXPLAIN / 部署 / 冒烟，**同样三项全 DONE** ⇒ 结论一致。）

### L.2 AC-09 判定 = FAIL（完整性双标记齐备，值原样登记）

- 哨兵 `AC09_LOG=` 指向 `reports\fluxing-br-1f\ac09-run-20261125-081355.log` —— **本轮新文件**（带时间戳），**不是** 2026-11-24 旧轮 `reports/fluxing-br-1c/ac09-run.log`（该旧轮虽含收尾标记，但不在 `AC09_LOG=` 指向路径上 ⇒ 未读、未引用）。
- **双标记核验**：① `AC09_VERDICT=` ✅ 存在（`[08:20:14] AC09_VERDICT=FAIL`）；② `=== AC-09 v4 END ===` ✅ 存在（`[08:20:14]`）⇒ **两标记齐备** ⇒ 判定值可采信（非 UNDETERMINED）。
- **值 = `FAIL`**（`AC09_SUMMARY stages=2 overall=FAIL strict=FAIL`）：Legacy 段 `table_write_delta=8, count_delta=0, rows24h_delta=40, push=1`；Deleted 段 `table_write_delta=0, count_delta=0, rows24h_delta=0, push=1`；两段 `round_ran=True`。
- **AC-09 保持 FAIL，不改记 PASS**（只有 `AC09_VERDICT=PASS` 才可改记）。

### L.3 只读复核（未写生产）

- `SELECT COUNT(*) AS buckets FROM sync_buckets` ⇒ **0 行**；返回 `success=true` / `changes=0` / `rows_written=0` ⇒ ① 桶表**存在且可查**（迁移 0004 已生效）② 生产 D1 额度**未耗尽**（无 `code: 7500`）③ 当前**无桶数据**（客户端桶化真机轮未跑 ⇒ 与 §M.7.7 第 4 项一致）。
- 本轮**未执行任何写操作**：未 `DELETE` / 未 `INSERT` / 未 `deploy` / 未改 `wrangler.toml`。

### L.4 三态标注

| 结论 | 状态 | 依据 |
|---|---|---|
| 主链 8 段（+7B +5_CLEANUP）**全部到达终态**，无 `BLOCKED` / `MISSING` / `NOT_STARTED` 段 | **已验证** | 哨兵逐行解析（L.1） |
| 迁移 0003+0004 成功 | **已验证** | `STEP=2 MIGRATE_EXIT=0 APPLIED=0003,0004` |
| EXPLAIN 三条命中新索引 | **已验证** | `STEP=3` |
| Worker 部署成功 | **已验证** | `STEP=4 DEPLOY_EXIT=0` |
| 桶端点生产冒烟 PASS + 清理零残留 | **已验证** | `STEP=5` / `STEP=5_CLEANUP` |
| AC-09 = FAIL（双标记齐备、原样登记） | **已验证** | L.2 |
| 生产桶表存在且当前 0 行、额度未耗尽 | **已验证** | L.3 只读复核 |
| 「整体 FAIL 的唯一触发项是 `push=1`、Deleted 段字面判据成立」 | **推断**（沿用 §K-1 的判读；本轮只读未重新拆解两段语义） | §K-1 + 本轮 `AC09_SUMMARY` |
| BR-①g 真机阶段机 0→1→2→3 后 AC-09 能否转 PASS | **未验证**（真机轮未跑） | 哨兵 `STEP=BR1G … REALMACHINE=NOT_RUN`；§M.7.6 |
| 补排轮能否改变 AC-09 结论 | **推断（大概率不能）** | 补排读的是同一哨兵 + 同一 AC09 日志；只有主链/真机轮重跑才会产生新证据 |
| §M.7.7 未闭环 11 项 | **未闭环**（沿用，本轮未新增闭环项） | §M.7.7 |

### L.5 结论一句话

**CHAIN_STATUS = COMPLETED**（步骤层 11 行全终态、无 MISSING）；**AC-09 = FAIL**（双标记齐备、原样登记、**不改记 PASS**）；**迁移 0003+0004 = DONE / EXPLAIN = DONE / 桶端点冒烟 = DONE（PASS）/ 部署 = DONE** ⇒ **服务端桶化生产链已闭合**，客户端判据 AC-09 仍未达成，且 BR-①g 真机轮未跑。

### L.6 补排（+90 分钟）

本次 = **第一次**回检（此前无 §L），且哨兵内存在 `AC09_VERDICT=FAIL` ⇒ 按回检任务第 5 步**自排一次补排回检**：一次性任务，时间 = 本轮 + 90 min = **2026-11-25 12:01:56 +08:00**（`runAtMs=1795579316073`），任务名加「（补排）」，prompt 与本任务相同。
**补排只是观察窗口**：它读的还是同一哨兵 + 同一 AC09 日志 ⇒ **除非 BR-①g 真机轮或主链重跑向哨兵追加新行，否则补排结论必然与本节相同**。补排轮**不再续排**。

### L.7 补排轮复核（2026-11-25 12:02 · 观察窗口关闭）

**本节性质** = 补排轮 cron `33ebe723-f6f4-4c4a-9f21-1fa5c8cb7986`（`runAtMs=1795579316073` = 2026-11-25 12:01:56 +08:00，由 §L.6 自排）对主链 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5` 的**只读判定 + 文档回写**。
本轮**未改任何产品代码**（`cloud/fluxing-sync/**` / `WeaselServer/**` 零改动）、**未重复主链工作**、**未自行启动主链**。

#### L.7.1 相对 10:30 首轮的唯一新增证据（已验证）

哨兵 `reports/fluxing-br-1f/chain-progress.log` 现有 **14 行**（10:30 首轮读到 11 行）⇒ **新增 1 行**：

```
2026-11-25T10:37:33+08:00 STEP=BR1G REALMACHINE STATUS=DONE NOTE=PHASE0_BASE=OK PHASE1=OK(phase=1,pushed=0,skipped=0,pulled=0) PHASE2=NOT_OBSERVED PHASE3=INTERCEPTED_BEFORE_RUN PHASE4=SKIP ROOTCAUSE=bucket_scan_complete_gate(CloudSyncService.cpp:1608-1611) LEGACY_PULL_PER_ROUND=1000 CLOUD_ITEMS=40061 ROUNDS_TO_SCAN=41 PROD_SYNC_BUCKETS=0 PROD_ROWS_WRITTEN_24H=40281 BASE=40259 DELTA=22(非桶写) ROLLBACK=phase0@10:36:38 USER_ENV=REMOVED HAZARD=phase3_flood_94604_items_190000_rows NEXT=R2_GATE_INTO_BUCKET_PATH+PER_ROUND_QUOTA
```

该行 = §M.8 记录的 BR-①g 真机轮（10:24–10:35）收官行 ⇒ **与 §M.8 一致，不改变 §L 的任何判定**（AC-09 仍 FAIL、真机轮仍记「未判定」、生产 `sync_buckets` 仍 0）。

#### L.7.2 AC-09 复核 = FAIL（已验证 · 双标记复验齐备）

- 哨兵 `AC09_LOG=reports\fluxing-br-1f\ac09-run-20261125-081355.log` 未变；本轮**再次逐字复验**：`AC09_VERDICT=` 计数 = 1（`[08:20:14] AC09_VERDICT=FAIL`）、`=== AC-09 v4 END ===` 计数 = 1 ⇒ **双标记齐备** ⇒ 判定值可采信（非 UNDETERMINED）。
- 值 = **`FAIL`**（原样登记，**不改记 PASS**）。2026-11-24 旧轮 `reports/fluxing-br-1c/ac09-run.log` 仍**未读、未引用**。

#### L.7.3 迁移 / EXPLAIN / 部署 / 冒烟 判定（已验证 · 两种口径交叉验证）

| 语义项 | 哨兵实际行 | STATUS | 关键字段 | 本轮判定 |
|---|---|---|---|---|
| 迁移 0003+0004 | `STEP=2` | DONE | `MIGRATE_EXIT=0 APPLIED=0003,0004` | **DONE** |
| EXPLAIN 复测 | `STEP=3` | DONE | 三条命中 `idx_sync_user_updated` / `idx_sync_buckets_user_id` / `idx_sync_buckets_user_updated` | **DONE** |
| 部署 Worker | `STEP=4` | DONE | `DEPLOY_EXIT=0` | **DONE** |
| 桶端点生产冒烟 | `STEP=5` | DONE | `VERDICT_SMOKE=PASS SMOKE_EXIT=0` | **DONE / PASS** |

按回检正文**字面**步号取（`STEP=3` / `STEP=4` / `STEP=5`）= EXPLAIN / 部署 / 冒烟 ⇒ **同样三项全 DONE** ⇒ **无 MISSING**（与 §L.1 的口径错位说明一致）。

#### L.7.4 只读复核（未写生产 · 已验证）

- `SELECT COUNT(*) AS buckets FROM sync_buckets` ⇒ **0 行**；返回 `success=true` / `changes=0` / `rows_written=0` / `rows_read=0` ⇒ ① 桶表**存在且可查**（迁移 0004 已生效）② 生产 D1 额度**未耗尽**（无 `code: 7500`）③ 当前**无桶数据**（与 §M.8 真机轮「生产 `sync_buckets` 恒 0」一致）。
- 本轮**未执行任何写操作**：未 `DELETE` / 未 `INSERT` / 未 `deploy` / 未改 `wrangler.toml`。

#### L.7.5 三态标注

| 结论 | 状态 | 依据 |
|---|---|---|
| 主链 8 段（+7B +5_CLEANUP）仍全终态，无 `BLOCKED` / `MISSING` / `NOT_STARTED` | **已验证** | 哨兵 14 行逐行解析（L.7.1） |
| AC-09 = FAIL（双标记齐备、原样登记、未改记 PASS） | **已验证** | L.7.2 |
| 迁移 0003+0004 / EXPLAIN / 部署 / 桶端点冒烟 = 全 DONE | **已验证** | L.7.3 |
| 生产桶表存在且当前 0 行、额度未耗尽、本轮零写操作 | **已验证** | L.7.4 只读复核 |
| BR-①g 真机轮已收官且与 §M.8 一致（哨兵新增第 14 行） | **已验证** | L.7.1 |
| 「整体 FAIL 的唯一触发项是 `push=1`、Deleted 段字面判据成立」 | **推断**（沿用 §K-1；本轮只读，未重新拆解两段语义） | §K-1 + 本轮 `AC09_SUMMARY` |
| BR-①g 真机阶段机 0→1→2→3 后 AC-09 能否转 PASS | **未验证**（真机轮已跑，但阶段 2 未观测、阶段 3 被拦截 ⇒ 无新的对账证据） | §M.8 + 哨兵第 14 行 |
| §M.7.7 未闭环 11 项 + §M.8.4 两条补齐规则（R2 闸门进桶路径 + 每轮桶推配额） | **未闭环**（本轮未新增闭环项） | §M.7.7 / §M.8.4 |

#### L.7.6 结论一句话

**CHAIN_STATUS = COMPLETED**（哨兵 14 行全终态、无 MISSING）；**AC-09 = FAIL**（双标记齐备、原样登记、**不改记 PASS**）；**迁移 0003+0004 = DONE / EXPLAIN = DONE / 部署 = DONE / 桶端点冒烟 = DONE（PASS）** ⇒ 与 §L 首轮结论**完全一致**：服务端桶化生产链仍闭合，客户端判据 AC-09 仍未达成，生产 `sync_buckets` 仍 0 行。

#### L.7.7 补排收口（不再自排第三次）

按回检任务第 5 步「**若本次已经是补排轮 ⇒ 不再补排，直接收口**」⇒ 本轮**不再自排第三次**。
观察窗口关闭依据（已验证）：主链 STEP 1 与 AC-09 自 08:10 起**无新证据**；哨兵唯一增量（第 14 行）属 BR-①g 真机轮，不改变本判定。
⇒ 后续 AC-09 转 PASS 的唯一路径 = **BR-①g 真机轮按 §M.8.4 两条补齐规则（R2「库存未证实不推送」进桶路径 + 每轮桶推行数配额）重跑并跑完对账循环**（或主链重跑 STEP 1）⇒ 需**有人值守的真机轮**，不是第三个定时回检能解决的。
附带事实（已验证）：主链 `4d3e51f8-fdda-41bb-bbc1-12e844914dc5`、首轮回检 `312fd16e-d687-4973-b636-f1044ad9cdd0`、本补排 `33ebe723-f6f4-4c4a-9f21-1fa5c8cb7986` 三个一次性任务均已 `enabled=false` / `status=paused` ⇒ **无残留定时任务会再触发**。

#### L.7.8 本轮工具坑（已验证）

1. 哨兵 mtime 仍不可信（`Tee-Object` 启动即截断文件）⇒ 继续**只按内容**判定新鲜度。
2. `wrangler d1 execute` 只读命令须用**绝对路径** + `--config <绝对路径>/wrangler.toml`；返回体里 `rows_written=0` / `changes=0` 可直接当「未写生产」的证据。
3. 本机首个 bash 调用会先做 **shell discovery**（PowerShell）：`cd /f/...` 形式会被解析成 `F:\f\...` ⇒ 一律用 `Set-Location 'F:\...'` 或全程绝对路径。

---

## §M.8 真机轮根因定位与「阶段 3 生产风险」拦截（2026-11-25 10:24–10:35 · 补记）

> 本节是 §M.7.6「真机轮」的**续写**：把「阶段 1 `pushed=0`」从「现象」追到「代码行」，并在阶段 3 开跑前**主动拦截**了一次会打爆生产 D1 免费额度的推送。

### M.8.1 现象（已验证）

- 阶段 1 诊断行（`reports/fluxing-br-1g/phase1-probe.log` · `phases-summary.log`）：
  `2026-11-25 10:27:41.769][cloudsync] bucket phase=1 pushed=0 skipped=0 pulled_buckets=0 pulled_items=0`
  同期 `reconcile scan_complete=0 matched=67 deferred=54536 divergent=0 pushed=0`，`LEGACY_PUSH_LINES=0`，`VERDICT=OK`。
- 生产回读（只读）：`sync_buckets = 0`、`sync_items = 40,061`、`users = 4`；
  `wrangler d1 info` ⇒ `rows_written_24h = 40,281`（基线 40,259 ⇒ **+22**，非桶推产生：桶推一次都没发）。
- 阶段 2（10:27:42 起）**未观测到**诊断行：阶段机在等下一轮同步时被我主动终止（见 M.8.4）。

### M.8.2 根因：桶推被「库存扫描完整」闸门挡住（已验证 · 代码行级）

- 闸门在 `WeaselServer/Cloud/CloudSyncService.cpp:1608-1611`：

  ```cpp
  if (bucket_mode &&
      (direction == SyncDirection::kPushOnly ||
       direction == SyncDirection::kBoth) &&
      bucket_scan_complete) {           // ← 库存扫描未完成 ⇒ 本轮不推桶
  ```

  注释（同文件 1604-1607）写明理由：**桶内容 = 合并视图的整桶**，视图不完整时推上去会覆盖掉没看到的远端条目 ⇒ 这是「整桶覆盖闸门」，**不是** bug。
- 为什么 `bucket_scan_complete` 恒为假（已验证）：库存扫描的完整性来自**逐条端点**的分页拉取，
  上限 `kMaxPullPages = 10` × `kPullPageLimit = 100` = **每轮最多 1,000 条**，
  而云端存量 `sync_items = 40,061` ⇒ **需要约 41 轮**才能扫完（与 BR-①e 登记的「41 轮」同源）。
  实测 `scan_complete=0` ⇒ 闸门常闭 ⇒ 阶段 1/2 **一轮桶都不推** ⇒ `pushed=0 skipped=0`（**不是**推了空桶，是**根本没发请求**）。
- 为什么不是「桶内容为空」：`PushChangedBuckets` 里 `changed = !known`（首次无桶行 ⇒ 必推），
  而 `merged` 有 94,604 条 ⇒ 若闸门放开，首轮应推 **≈1,024 桶**（`kBucketShardCount=64` × `kBucketSubShardCount=16`）。

### M.8.3 拦截：阶段 3 会一次性放行约 94,604 条 ⇒ 已主动掐断（已验证 · 风险评估 + 处置）

**风险机理（推断，已由代码行坐实）**：

1. 阶段 3/4 = `legacy_endpoints = false`（`CloudSyncService.cpp:1132-1135`）⇒ 逐条端点**完全不发**；
   ⇒ 库存完整性再也无法由逐条扫描满足 ⇒ 闸门在阶段 3 变成「无扫描可依赖」。
2. `PushChangedBuckets`（`CloudSyncService.cpp:1954-1968`）对 **`merged` 的全部条目**分桶，
   **没有**继承 ADR 0076 D2-5a-2 的 **R2「库存未证实不推送」** 闸门 ⇒
   54,536 条 `deferred`（从未上云）条目会**混进桶里被一次性上传**。
3. 服务端是 `R2 put ⇒ D1 DELETE+INSERT`（覆盖语义）⇒ 每条条目 ≈2 行写入 ⇒
   单轮写入量 ≈ **94,604 × 2 + 1,024 索引 ≈ 190,000 行**（远超单账号 80,000 行/日预算与 D1 免费额度 100,000 行/日）
   ⇒ 服务端会返回 `429 write_budget_exceeded`，但**预算耗尽前的那批写入已经落库**
   ⇒ **复现 BR 批次 2026-11-23 的生产事故**（D1 免费额度耗尽 ⇒ 登录写 `refresh_tokens` 被拒 ⇒ 真实用户登录 500）。

**处置（已验证）**：

- 10:31:4x 主动终止阶段机后台任务（`bg_f4f62f35`），**在阶段 3 开跑之前**；
  ⇒ 本轮真机**从未进入阶段 3**，生产 `sync_buckets` 保持 **0 行**、`rows_written_24h` 仅 +22（与桶推无关）。
- 机器已复位到 **phase=0**：User 级 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` 置 0 → 重启 `WeaselServer` →
  跑一轮让**侧车 `phase` 写回 0** ⇒ 再删除 User 级变量（回滚闭环证据：`reports/fluxing-br-1g/rollback-phase0.log`）。

### M.8.4 因此，BR-①g 第 ⑩ 项（「桶化后一次性补推约 54,536 条」的分批策略）**不是可选项，而是阶段 3/4 的硬前置**

**已验证的可落点（好消息）**：桶推**天然可续推**——`CloudSyncService.cpp:2168-2185` 里
「本轮没推的桶保留旧指纹行」「推成功但服务端没认的桶不写行」⇒ 下一轮自动重试；
⇒ 加一个「每轮最多推 N 桶 / M 行」的配额闸门即可把上传摊到多轮，无需另造水位。

**必须补的两条规则（下一批落地）**：

1. **R2 闸门进桶路径**：桶内容只能包含「库存已证实」的条目（云端已有 / 本轮拉到的），
   **不得**把 54,536 条 `deferred` 条目打包上云；
   否则桶化等于「把用户历史上敲过的每个词都上传」，与 ADR 0076 D2-5a-2 的原意相反。
2. **每轮配额闸门**：单轮桶推的行数上限（建议 **≤ 8,000 行/轮**，对应 80,000 行/日预算的 10%，
   留足登录写 `refresh_tokens` 与 `users` 的余量），超出的桶留给下一轮。

**未闭环（明确记为「未判定」，不得记 PASS）**：

- 桶推路径在真机上**从未真正发出过一次请求**（`sync_buckets` 恒 0）⇒
  桶端点 push body / HLC 守卫 / R2 往返 / 桶级拉取解包（客户端解析器）在生产上**均未验证**；
- 阶段 3/4 真机判据（切读、老端点退役、回滚）**未执行**；
- 阶段 2（双写 + 交叉校验）诊断行**未观测到**（阶段机在等待下一轮时被终止）。

### M.8.5 下一批的最小可执行路径（供接力会话直接开工）

1. 客户端加「R2 闸门进桶路径」+「每轮桶推行数配额」两条规则（`CloudSyncService.cpp:1954-2065` 一带），
   配套 `test/TestFluxingCloud/TestFluxingCloud.cpp` 用例 ⇒ 门禁复跑（基线 `PASS=899 FAIL=0`）。
2. 升版 + 出包 + 装机 + 三处 sha256 对账（照 §M.7.5 的口径）。
3. 真机：**只跑阶段 1/2**（此时逐条端点仍在 ⇒ 库存扫描仍在推进 ⇒ 桶推闸门仍闭 ⇒ 生产安全），
   连续跑够 41 轮让库存扫描完成（每轮约 3 分钟 ⇒ 约 2 小时）⇒ 观察桶推是否按配额逐轮放行。
4. 库存扫描完成后才允许阶段 3；阶段 3 首轮必须观测到 `pushed>0` 且 `sync_buckets` 行数按配额增长、
   `rows_written_24h` 增量 **≤ 8,000**；再跑阶段 0 验证回滚。
5. 服务端零改动的前提不变（桶端点已在生产，本批仍不需要 deploy）。

## O · 用户拍板落地轮（2026-11-25 · 闸门 / 平台额度 / 护栏口径 / jury.xin 自建）

- **拍板来源**：用户 2026-11-25 消息（**不是**问卷卡）四项：① BR-①g 闸门「根据你的推荐来操作」② 平台付费档「不升级」③ 写入护栏口径「按你的推荐操作」④ 新增议题「是否需要把云同步从 Cloudflare 转移到我们自己的服务器（jury.xin）」。另 **⑤ 腾讯云两项安全配置** 与 **⑥ 两个定时任务是否合并** —— 用户只复述了议题、**未给答案** ⇒ 仍待拍板，本节只给推荐，**未执行、未代答**（沿用 §J-8「本轮未动、未代答」先例）。
- **本节编号取 §O**：文中 1117 行已存在 `§N AC-113b` 旧标题（其他批次遗留），为避免撞号顺延一位。

### O-1 四项决策

**O-1.1 BR-①g 闸门 = 有条件开闸**（采纳 §K-8 项 1 的既有推荐，与用户「按你的推荐」一致）
- 开闸依据（已验证）：Deleted 段 `table_write_delta=0` / `count_delta=0` / `rows24h_delta=0` ⇒ a-7 字面判据成立；Legacy 段 `twd=8` 属真实改动（`push=1`）的正常增量。
- **AC-09 仍登记 FAIL**（整体 FAIL 的唯一触发项是两段的 `push=1`，属推断）⇒ 改记 PASS 的权限只属复跑日志出现 `AC09_VERDICT=PASS`（§K-8 项 3），本节不构成放宽。
- **闸门放行范围 = 阶段 0→1→2 的实现 + 真机复跑**，**不含阶段 3（切读）**。阶段 1→2 的放行证据 = 桶推路径真机**至少发出一次请求**且生产 `sync_buckets > 0` 行（当前实测 = 0 行，§M.8）。
- **阶段 3 新增硬前置（本轮新发现 · 推断/未验证）**：切读前必须完成「历史库存的桶化覆盖」。`sync_items` 现有 40,061 条若未在桶端点建档，切读后新装/重装设备只从桶端点拉取 ⇒ **历史词条不可见 = 用户可见的数据丢失**。阶段 4（老逐条端点退役）同以此为前提；§J-8 拍板 ④ 的「双写 → 校验 → 切读 → 退役」四段**不得跳段**，只做「切读」就下老端点 = 不合格。
- 实现前置 = §M.8.4 两条规则（规则 1 库存扫描完整性闸门进桶路径 + 规则 2 每轮桶推行数配额 8,000），**顺序不得颠倒**。

**O-1.2 平台付费档 = 不升级 Workers Paid** ⇒ Cloudflare D1 免费「10 万行/日（每库全局口径）」成为长期硬约束。后果与要求（三条，全部落档）：
- 桶化是该约束下唯一的结构性解法。§J-2 稳态降幅「索引行 ≈53× / D1 实际写行 ≈27×」是**设计值（推断）**，未实测，不得写成实测。
- 补推洪峰必须轮内分批 ≤64 桶/请求（`kBucketPushBatch=64`）+ 每轮行配额（O-1.1 规则 2），不得单轮倾倒 1,024 桶。
- 护栏必须能在额度耗尽时**只拦同步写、不拦登录/续期**。2026-11-23 事故实证：额度耗尽时 `cloud/fluxing-sync/src/auth.ts:1043-1045` 的 `devices.last_seen` 刷新（读路径上的写）+ `refresh_tokens` 插入把真实用户登录变成 500/503。

**O-1.3 写入护栏口径 = 维持「每账号」+ 下调阈值 + 立项最小版全局熔断（BR-②′）**
- 维持每账号（`WRITE_BUDGET_ROWS_PER_DAY` / `WRITE_BUDGET_BUCKETS_PER_DAY` 的 `WHERE user_id = ?` 语义）：既有实现与单测不推翻、改造成本为零；桶化后每账号日写量本身已小。
- **不推荐**把护栏整体改成「整库总量」口径：需要跨账号计数、与既有 per-account 语义并存，属改造，且**替代不了**真实目标「额度耗尽不得拖垮登录」。
- 推荐补两件（下一轮，动生产需用户单独授权）：
  - ① `cloud/fluxing-sync/wrangler.toml` 的 `WRITE_BUDGET_ROWS_PER_DAY` 由 **80,000 下调到 20,000**。理由：80,000 = 全局 10 万额度的 80%，单账号即可拖垮全局；桶化后全量补推峰值 ≈2,048 行/用户/轮，20,000 仍留 ≈10× 余量。`WRITE_BUDGET_BUCKETS_PER_DAY=4,000` 保留（全量补推 1,024 桶 < 4,000）。
  - ② 立项 **BR-②′（最小版平台级同步写熔断）**：在同步写路径（逐条 push + 桶 push）上加**全局**日写入熔断，超阈直接拒绝且**只影响同步**，登录/续期/邮箱验证路径不受影响。需一份新 ADR 定死近似边界：`rows_written_24h` 只在 Cloudflare REST / `wrangler d1 info` 可见、**Worker 内读不到** ⇒ 熔断量只能用 Worker 自建表的写入计数估算（推断），沿用既有纪律「**不宣称**能精确计出 Cloudflare 账单行数」（`src/sync.ts` 注释 + ADR 0076 D2-3）。
- 与 §J-8 拍板 ③ 的关系：该轮「BR-② 不立项」的前提是「Workers Paid 暂不升级但桶化后看情况」；本轮用户明确**不升级** ⇒ 前提收紧，故只立**最小版熔断**（不动既有 per-account 实现），不做 §J-8 当时否决的「全局口径改造 / 按活跃账号分摊」。

**O-1.4 jury.xin 自建 = 现在不切；登记为 BR-⑤，待云同步协议冻结后另行立项**
- 依据 `docs/research/fluxing-cloud-2026-11/tencent-cloud-ecs-recon.md` §一（2026-11-24 两轮 SSH 只读实测）：**可以承载，但不建议现在切**。本节采纳该结论。
- 实测硬约束（已验证）：根盘 **89% 已用 / 剩 14 G**（`/var` 57 G）；该机负载重且**带病**（`sales-bi-update.service` = failed）；`/etc/cloudflared/config.yml` **0666 全局可写**；**5432 / 3306 / 9100 公网监听**（安全组未证实）；**ICP 备案未证实**（国内机对外提供 80/443 的合规硬前置，未证实 = 未通过，可能一票否决）；实例规格/带宽未证实（元数据探测被挡、仓库内无任何腾讯云 API 凭据）。
- 真实成本（与「换台机」无关）：`cloud/fluxing-sync/**` 与 D1 + R2 强耦合（`id` = `server_seq` = pull 游标）⇒ 自建 = **一次存储层重写**，并要把「E2E 加密 + HLC 守卫 + 桶键 HMAC（`fluxing-bucketkey-mac-v1`）」这套已验证语义在 PostgreSQL 上**重证一遍**。
- 利好（如实登记，不据此改判）：nginx 1.18 + certbot（`jury.xin` LE 证书自动续期）+ systemd + Docker + Prometheus/Grafana/Loki 全在跑，加一个 vhost/容器的边际工作量接近零；境内可达性有现成落点。
- **本轮新论证（推断）**：桶化完成是「未来可迁移性」的**前置**而非竞争方案 —— 桶化后库存从 40,061 行降为 ≈1,024 个 R2 对象 + 1,024 行索引，搬迁数据量与停机风险大幅下降；现在切 = 带着写放大问题搬迁，且迁移本身的密文搬迁会撞 D1 额度（删除行同样计写入）。
- BR-⑤ 开工前置四项（沿用报告 §五.3）：清盘并承诺磁盘余量 / 补证 PG server 版本与 `postgresql.conf` 位置 / 备案与实例规格带宽由用户在控制台确认 / 修 cloudflared 权限并查 `sales-bi-update.service` failed 成因。
- 本节**只落决策**：未做迁移实施、未连腾讯云机、未改任何远端配置与权限。

### O-2 仍待拍板两项（含推荐，未执行）

- **⑤ 腾讯云两项安全配置**：(a) `/etc/cloudflared/config.yml` 权限 0666 ⇒ 建议改 **0600**（同目录已有 3 份 bak，变更前先备份；属远端变更，需单独授权）；(b) 5432/3306/9100 公网监听 ⇒ 建议**先到腾讯云控制台核实安全组**，再把 DB 端口收口到内网 / SSH 隧道或改 bind 地址，9100（node_exporter）不应公网可达。⚠️ 该机同时承载 WordPress `jury.xin` 与 sales-bi 生产栈，任何变更都有连带风险 ⇒ 建议**独立运维轮**处理，不与 BR 批次混跑。
- **⑥ 两个定时任务是否合并**（主链 `4d3e51f8` 08:10:46 + 回检）：推荐**保持分离**，理由见 §J-7（主链是带哨兵进度行的长链、回检是只读复核；合并后任一段失败会连带丢判定）。

### O-3 本轮边界与状态回读

- 本轮 = **决策落档轮**：只写三份文档（本 handoff §O、`CLAUDE.md` §2 BR 指针、同批 relay prompt 就地改写）。
- **入库情况**：三份文档已按用户指令 commit 入库 = **`f8de78f`**（`docs(BR): 用户拍板落地轮 …`，`3 files changed, 58 insertions(+), 1 deletion(-)`；暂存前逐条核对仅 `CLAUDE.md` + handoff + prompt 三个文件，`librime` dirty submodule 与 `reports/**`、`scripts/verify/**`、`release/` 一律未 add）；**对 `kizemo/feat/100-cloud-sync` 仍 ahead 1、未 push**。
- **未改任何产品代码 / 未跑门禁 / 未出包 / 未装机 / 未动生产**。状态原样：`AC-09 = FAIL`；客户端机器 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` 已删 = **phase 0**；生产 Version `4f163326-f2a5-4c69-be56-24cb800e6915`；生产 `sync_buckets` 仍 **0 行**、`sync_items` 仍 **40,061 行**；门禁数字沿用 §M.7.3 的 `PASS=899 FAIL=0`（未重跑）；1.3.9 包与装机 exe sha256 不变。

### O-4 下一轮（BR-①g 实现轮）任务序与前置

1. 实现 §M.8.4 规则 1 + 规则 2（`WeaselServer/Cloud/CloudSyncService.cpp`、`WeaselServer/Cloud/CloudBucketSync.h`、`WeaselServer/Cloud/CloudBucketSync.cpp`；单测进 `test/TestFluxingCloud/`，基线 **899** 不得回归）。
2. 门禁：照 §M.7.3 **单独** msbuild `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.vcxproj` 且显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`（`weasel.sln` 不含该工程，否则跑旧 exe、PASS 恒 899）+ U+FFFD=0 + clang-format 改动 hunk 零漂移；随后升版出包装机 + 构建/包内/装机三处 exe sha256 对账。
3. 真机**只跑阶段 1/2**：连续 **≥41 轮**让库存扫描完成（每轮约 3 分钟 ≈ 2 小时）⇒ 观察桶推按配额逐轮放行 ⇒ 复跑 `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1c\probe-ac09.ps1` 判 AC-09（只有 `AC09_VERDICT=PASS` 才可改记）。
4. 库存扫描完成后才允许阶段 3；阶段 3 首轮必须观测到 `pushed>0` 且 `sync_buckets` 行数按配额增长、`rows_written_24h` 增量 **≤ 8,000**；之后跑阶段 0 验证回滚。
5. 服务端零改动前提不变（例外 = O-1.3 ① 的阈值下调，需用户单独授权 deploy）。
- **needs the user（需要用户机器/授权）**：① 真机连续 41 轮同步窗口（约 2 小时，期间输入法同步占用网络，且不得锁屏 —— 真机取帧类判据须先查 `LogonUI`）；② `wrangler.toml` 阈值改动 + 生产 deploy 授权；③ BR-②′ 的 ADR 与实现授权；④ ⑤ 两项腾讯云远端变更授权（权限 + 安全组核实）；⑤ ⑥ 合并与否的答复；⑥ 是否给 BR-⑤ 立项排期。
- **本轮工具坑**：① 不带 `path` 收窄的全仓库 `grep` 会超时（本轮 `jury\.xin` 首次全仓检索超时），改为限定 `docs/` 后一次命中；② 往本 handoff 追加章节前必须**先列 `^## ` 标题**再定号（1117 行有遗留 `§N` 标题，按 §K→§L→§M→§N 直觉续编会撞号）；③ 本批 relay prompt 是**单行文件**（2,892 字符一行），就地改写前须整行读取，不能用首屏截断内容拼 `old_string`；④ handoff / CLAUDE.md / prompt 三个文件均为 **LF、无 BOM**，追加时不得引入 CRLF。

## P · 批次 BR-①g 补齐规则落地轮 + 装机修正轮（2026-11-25 14:30–16:20 · §M.8.4 规则 1/2 进桶路径 + 门禁 924 + 出包 1.3.10 + 装机修正 + §M.8.1 根因订正）

> **本批性质** = 按 §O-4 工序，把 §M.8.4 的两条补齐规则真正下沉到**桶推路径**，跑门禁、出包、装机；并**订正 §M.8.1 登记的「阶段 1 推 0 桶」根因**（原归因**误判**，本轮以源码 + 真机日志交叉坐实，见 §P-F）。
> **本轮未做** = 真机阶段 1/2 **未跑完**（根因订正后需重跑；runner 已备，见 §P-F-4）；探针 v5 重编（留下一轮）；**未推进阶段 3 切读**（§O-1.1 硬前置未满足）。
> **AC-09 仍登记 FAIL**（本轮**无** `AC09_VERDICT=PASS` 日志 ⇒ 按 §O-1.1「只有复跑日志出现该标记才可改记」，**不改记**）。
> **服务端零改动 / 未 deploy**；客户端唯一授权范围 = `WeaselServer/Cloud/**`（用户 2026-11-23 已授权，见 §M 指针）。

### P-A. 已达成（本轮实测）

1. **规则 1「库存未证实不推送」进桶路径** — `WeaselServer/Cloud/CloudSyncService.cpp`
   - 老端点拉取阶段收集未证实条目键 → `std::set<std::string> unproven_keys`（cpp:1328 声明；cpp:1424-1430 在 `known == nullptr && inventory_unproven` 分支登记，其中 `inventory_unproven = has_more`，cpp:1316）；cpp:1616-1618 传入桶推。
   - `PushChangedBuckets` 新增入参 `const std::set<std::string>& unproven_keys`；**整桶压制**（桶内含任一未证实条目即整桶不推）—— 桶级端点语义是**整桶 DELETE+INSERT**，只剔除部分条目会让被剔除者永久消失。
   - 闸门 cpp:2060-2067；计数写 `CloudBucketRoundStats::held_unproven`（`.h`:541，**不进** `FormatBucketDiagLine`）。
2. **规则 2「每轮桶推行数配额」** — 同文件 + `CloudSyncService.h`
   - `.h`:589 `constexpr size_t kBucketPushRowsPerRound = 2000;`；cpp:1959-1960 起算 `quota_rows`；cpp:2086-2111 按**整桶**截断（`SelectBucketsWithinQuota`），超出部分计 `held_quota` 顺延下轮（被压桶不写 `brow` ⇒ 指纹保持旧值 ⇒ 下一轮自动续推）。
   - 新增无条件诊断行 `FormatBucketQuotaLine`（cpp:2253-2257，插在 `bucket_rows_ = new_brow;` 之前）。
   - **既有 `FormatBucketDiagLine` 五字段逐字未改**（`.h`:566-574；生产侧与单测侧双重逐字锁定 ⇒ AC-09 探针 v4 的前缀正则不破）。
3. **单测** — `test/TestFluxingCloud/TestFluxingCloud.cpp`
   - 新增 `TestBucketInventoryGateAndQuota()`，覆盖：未证实条目整桶压制、`held_unproven` 计数、配额按整桶截断、顺延下轮、阶段 0/1 行为不变。
4. **门禁（本轮实跑，非推断）**
   - `msbuild WeaselServer\WeaselServer.vcxproj /p:SolutionDir=… /p:Configuration=Release` ⇒ **`WS_EXIT=0`**、`: error ` 命中 **0**（`reports/fluxing-br-1g/gate-1.3.10-weaselserver.log`）。
     - **坑**：历史 6 连败的 `PushChangedBuckets` 签名不一致，本轮**只改 `.h` 声明参数序**（把 `unproven_keys` 移到两个 `bool` **之前**，与 `.cpp` 定义/调用点一致）即编过。教训 = **签名类改动必须以 msbuild 实跑为准，`read` 回读不足以证明生效**。
   - 单独 `msbuild test\TestFluxingCloud\TestFluxingCloud.vcxproj` ⇒ **`GATE_BUILD_EXIT=0`**（`gate-1.3.10-build2.log`；`weasel.sln` **不含**该工程 ⇒ 必须单独 msbuild 且显式带 `/p:SolutionDir=`，否则跑的是上一轮旧 exe、PASS 恒 899）。
   - 跑测 ⇒ **`RESULT: PASS=924 FAIL=0`**（`gate-1.3.10-run2.log`；§M.7 基线 899 ⇒ **+25 断言、零回归**）。
   - U+FFFD = 0（三文件）+ clang-format 改动 hunk 漂移 **0**。
5. **发布口径升版 1.3.10**（`env.bat` 四钉 + `version.txt`）
   - `env.bat`：`FLUXING_VERSION=1.3.10` / `VERSION_PATCH=10` / `PRODUCT_VERSION=1.3.10.0` / `FILE_VERSION=1.3.10.0`（`WEASEL_BUILD=1`、`RELEASE_BUILD=1` 保留）。
   - `version.txt`：`1.3.10`（CRLF）。
   - **升版五处口径**（AGENTS.md §4.2 + 项目记忆）：只改 `VERSION_PATCH` = 半升版；`env.bat` 属 **local-only / gitignored** ⇒ **不入库**，入库的版本载体只有 `version.txt`。
6. **出包 1.3.10 = 成功**（`build.bat weasel installer`）
   - 包：`output/archives/fluxing-1.3.10-installer.exe` **43,022,044 B @ 11-25 15:59:11**（同目录 1.3.9 = 43,023,508 B @ 10:15:45，命名口径一致）。
   - 构建日志 `reports/fluxing-br-1g/build-package-1.3.10b.log`（45,625 B / 594 行）`: error ` 命中 **0**；NSIS 正常收尾 `Output: …fluxing-1.3.10-installer.exe` + `Total size: 43022044 / 155943381 bytes (27.5%)` + `Compressed data: 42902232`；仅 **8 条与 1.3.9 轮逐条相同的已知无害告警**（7010：weaselARM/ARM64/ARM64X.dll ×2 组、`data\*.gram`、`data\lua\*.txt`）。
   - **出包判据口径（须固化）**：日志出现 NSIS `Total size` **且** `Output: …installer.exe` **且** `output/archives/` 下该 exe 真实存在并回读 size/mtime；**只看 `BUILD_EXIT=0` 回显不算证据**。
7. **三处 `WeaselServer.exe` sha256 对账 + 装机态复核（本轮新测）**
   - 构建侧 = `output/Win32/WeaselServer.exe` **2,898,944 B @ 11-25 15:56:50**（与 `gate-1.3.10-weaselserver.log` 的 15:56:51 同一轮编译 ⇒ **进包的就是门禁验过的那个 exe**）
     sha256 `D7DFAC785CB1C2D262716D37DD21C19BDC6C02BD4A688E4CDB8446F519E8EB79`
   - 包内侧 = `7z e` 从包抽取（`SubType = NSIS-3 Unicode` / `Everything is Ok` / `Size: 2898944`）
     sha256 `D7DFAC785CB1C2D262716D37DD21C19BDC6C02BD4A688E4CDB8446F519E8EB79` ⇒ **与构建侧逐字节相等**
   - 装机侧 = `D:\Program Files\fluxing\weasel\WeaselServer.exe`
     **2,898,944 B** / **2026-11-25 15:56:50** / sha256 **`D7DFAC785CB1C2D262716D37DD21C19BDC6C02BD4A688E4CDB8446F519E8EB79`**
     ⇒ 三处对账 **全等（构建 = 包内 = 装机，逐字节相等）**；装机后进程 **pid=197728，StartTime = 2026-11-25 16:14:42**，可执行路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe` ⇒ **运行中的进程加载的是 1.3.10 新镜像**（安装器自带 pre-install `taskkill`，见 `output/install.nsi` 168-180 ⇒ 不违反 PPL「进程换新」约束）。
   - **⚠ `INSTALL_EXIT` 硬数字缺**：16:14:05 那次 `reinstall.ps1` 的 stdout **未落盘**（`reports/fluxing-br-1g/reinstall.ps1` 存在但无对应输出文件）⇒ **不写退出码**；改用**状态证据**坐实「装机已生效」：卸载项 `DisplayVersion = 1.3.10` + 目标 exe 的 size/mtime/sha256 与构建侧逐字节相等 + 进程于 16:14:42 重新拉起。**补齐办法** = 重跑 `reinstall.ps1` 并把 stdout 落盘（本轮不重跑，避免二次中断输入法）。
   - 装机前 INSTDIR 基线回读 = 2,894,848 B @ 11-25 10:14:12 sha16 `BBBDC47C646E83A8`（= §M.7 的 1.3.9 轮三处全等值）⇒ 本轮确为 1.3.9 → 1.3.10 覆盖升级。
   - **装机不再手杀 `WeaselServer`**：实读 `output/install.nsi` 168-180 确认安装器自带 pre-install `taskkill` 3 次重试 + 2s 延迟。

### P-B. 已坐实的实现层不一致 / 残留风险（**本轮未改代码**，留下一轮）

1. **`.h` 注释与常量口径不符** — 注释写「取 **4,096** 的理由」（`CloudSyncService.h`:582-588），但 `kBucketPushRowsPerRound` 实为 **`= 2000`**（`:589`）⇒ **注释须改写为 2,000 口径**（订正会改源码 ⇒ 已出包不重出，登记为「包 = 注释修正前源码」，注释不影响二进制）。
2. **规则 1 的判定口径（两处缺陷，合并登记）**
   - `bool inventory_complete = remote_complete;`（cpp:1958）之后**从未再赋值**；`legacy_endpoint_active` 只用于降级条目路径 ⇒ 注释声称的「老端点旁证参与判定」与实现不完全一致。
   - **阶段 1/2 下规则 1 实际完全不生效**：`remote_complete` 实参 = `bucket_scan_complete`（cpp:1617 传参、cpp:1958 赋值），而 `bucket_scan_complete = !bucket_more`（cpp:1289）来自**桶端点**拉取 ⇒ 生产 `sync_buckets` = **0 行** ⇒ 桶端点一轮即扫完 ⇒ `bucket_scan_complete = true` ⇒ `inventory_complete = true` ⇒ cpp:2064 的 `!inventory_complete && …` **恒为假** ⇒ **规则 1 在阶段 1 不会压住任何桶**，R2「库存未证实不推送」在桶路径**形同虚设**。
   - **后果（阶段 1）**：`merged` 含**全部本地条目**（cpp:1445-1446 `MergeLocalAndRemote`）⇒ 54,536 条 deferred（未证实）条目会**随桶包上云**。**本轮风险评估 = 低**：桶端点当前为空（`sync_buckets` 0 行）⇒ 无 DELETE 覆盖既有远端桶 ⇒ 不丢数据；行数上 ≈1,024 桶 ⇒ ≈2,048 D1 写行（远低于 80,000 护栏）。但**与 R2 字面语义冲突**，且一旦桶端点已有存量，规则 1 失效就会变成 §P-B-4 的丢数据风险。
3. **配额估算偏保守，补推轮数被低估** — 每桶固定 `EstimateBucketWriteRows(1, false)` = **2 行/桶** ⇒ 2,000 行/轮 ≈ **1,000 桶/轮**；单词典 1,024 桶 ⇒ 需 **2 轮**（非 §M.8「一轮扫完」）。§M.8.5 的「约 41 轮」估算应按**桶轮**重算（桶轮每轮 ≈1,000 桶 ⇒ 全量覆盖 ≈2 轮；41 轮是**逐条端点**库存扫描的轮数，与桶覆盖不是一回事）。
4. **残留丢数据风险（规则 1 未覆盖的边界）** — `inventory_complete=false` 时，只含已证实条目的桶仍可能装着**本轮未列出**的远端条目 ⇒ 该桶整桶 DELETE+INSERT 仍可丢数据。需要「远端桶内容 = 已证实集合」的更强前提，或桶级 merge 语义。
5. **每轮桶推行数配额的权威值三处互斥（新增，来自 ADR/spec 复核）** — 交接 §O-1.1 / §M.8.4 写 **8,000**；代码注释写 **4,096**；实现常量 **2,000**；而 ADR 0077 / ADR 0076 / spec 103 **五份文档完全没有「每轮桶推行数配额」这个概念**（ADR 0077:327-332 明确只列两层保护 = 轮内 ≤64 桶/请求 + 失败续传）⇒ 需拍板**唯一值**并回写 ADR/spec，否则阶段 3 判据（「`rows_written_24h` 增量 ≤ 8,000」）与实现**无法对账**。
6. **阶段 3 的「40,061 条桶化覆盖」硬前置只存在于交接文档**（handoff:1479）⇒ ADR 0077 / spec 103 **无此决策** ⇒ 决策层缺口，须在阶段 3 开闸前补进 ADR。

### P-C. 本轮工具坑（新增，须带走）

1. **`Wait-Process -Id $bpid` 在另一次 bash 调用里 `$bpid` 恒空**（每次 bash 调用都是全新 pwsh 进程，变量不跨调用）⇒ 等待被**静默跳过**，导致在构建**中途**读日志，得到 6 个 `: error ` 幻影命中、`INSTALLER_PRESENT=False`、`已用时间 00:00:14.52` 等**幻影读数**，一度误判「出包失败」。**修法** = 守候与读取写在**同一脚本/同一调用**内，或把 PID 落盘再读。
2. **中文 MSVC/NSIS 构建日志必须用 GBK(936) 字节级解码读**：`Select-String -Encoding utf8` / `ReadAllLines` 默认 UTF-8 会产生幻影命中。**可用配方**（一次取齐）：
   ```powershell
   $enc=[Text.Encoding]::GetEncoding(936)
   $lines=$enc.GetString([IO.File]::ReadAllBytes("$root\reports\fluxing-br-1g\build-package-1.3.10b.log")) -split "`r?`n"
   for($n=0;$n -lt $lines.Count;$n++){ if($lines[$n] -imatch 'error'){ '{0}: {1}' -f ($n+1),$lines[$n].Trim() } }
   ($lines -imatch 'Output:|installer\.exe|archives')
   @(Get-Process msbuild,cl,link,makensis -EA SilentlyContinue).Count   # 0 = 构建已终态
   ```
   完整错误原文在 `build.bat` 用 `/fl1 /fl2` 写的**根目录 `msbuild1.log` / `msbuild2.log`**，不在控制台日志里。
3. **`output\WeaselServer.exe` 不存在**（build.bat 不往该处复制）⇒ 构建侧哈希要从 `output\Win32\WeaselServer.exe` 取；`build\windows\*` / `tmp\*pkg*` / `hotfix_v0.67_patch_AB` / `output\backup.*` 下另有 30+ 个**历史陈旧 exe**，枚举时必须排除 `reports\`、`pkg*` 并看 mtime，否则会取到旧包 exe。
4. **并发嫌疑已判明排除**：16:00:50 实测 `msbuild,cl,link,makensis` 存活数 **0**；`git stash list` 为空；「14:58 装机 / 7,219,200 B / `INSTALL_EXIT=0`」的旧观测**作废**（属幻影读数）。
5. **`.specify/specs/103-client-bucket-sync/tasks.md` 不可信**（与 §M 实测矛盾：基线仍写 813、实现仍写「未入版本控制」、无「R2 闸门进桶路径 / 每轮桶推行数配额」两条规则、AC 编号混用 spec 102 的 AC-108/AC-112）⇒ 判进度只认 handoff §M.8 / §O / §P + 磁盘实况。
6. 旧坑仍有效：bash 里 `cd` 后写文件被硬安全策略拒绝（一律绝对路径 + `git -C`）；正则以 `\\` 开头被误判 UNC；单字母函数名被内置别名遮蔽（`h`=`Get-History`）；`Remove-Item` 删日志被整条拒；`verify-installed.ps1` 必须在 **pwsh 7** 下跑。
7. **`reports/` 里的补丁脚本 ≠ 已落码的改动（新增）** — `reports/fluxing-br-1g/patch-cpp-rules.ps1` 是一份**从未应用**的补丁脚本（意图去掉 `bucket_scan_complete` 一票否决、只拦「拉取失败 / remote_entries 不可信」两类真风险）；**本轮源码 cpp:1611-1614 仍保留 `bucket_scan_complete` 条件** ⇒ **不要被 `reports/**` 里的 `.ps1` 误导成「已改」**；任何改动状态只认 `WeaselServer/**` 的源码与 `git diff`。
8. **`AC-213` / `AC-214` 是悬空引用（新增）** — 全仓 `spec 103` 只到 **AC-212**（`.specify/specs/103-client-bucket-sync/spec.md`:119，`git show fdc71b9` 为该文件唯一提交）；客户端实现入库笔 `f7a62f6` **未触及** spec 103；唯��提及处是 `reports/fluxing-br-1g/section-p.md:72` 指称的 spec 版本 ⇒ **那份版本从未落盘** ⇒ 本轮**未改** spec 103。若阶段 1/2 真机判据要编号载体，须**新写** AC（不得引用 213/214）。
9. **ADR 内部同一张表三名一物（新增）** — 「每桶条目数 / 桶数 / 单轮预估传输时长 / 超时窗口」在 ADR 0077:282/:395/:451/:460 记作 **AC-112**，在 spec 103:119 记作 **AC-212**，而 ADR 0077:460 末句却写「见 spec 103 的 **AC-203**」（AC-203 实际是分批参数）⇒ **以 `SPEC103:119`（AC-212）为准**，ADR 编号待订正。

### P-D. Git 现状（本轮收口）

- 分支 `feat/100-cloud-sync`；**本轮入库前** HEAD = `a38f0b9`，对 `kizemo/feat/100-cloud-sync` **ahead 3**（`git status -sb` 实测）。**入库后**代码笔 = `ce55fa2`、版本笔 = `d36b82b`（实测 `git log --oneline -3`）；文档笔 sha 在提交后才生成 ⇒ **不在本节写死**（避免自引用）。
- 本轮共 **3 笔提交**（单一 scope，Conventional Commits）：
  1. `feat(WeaselServer): …` ⇒ `WeaselServer/Cloud/CloudSyncService.h`、`WeaselServer/Cloud/CloudSyncService.cpp`、`test/TestFluxingCloud/TestFluxingCloud.cpp`
  2. `chore(fluxing): 升版 1.3.10` ⇒ `version.txt`
  3. `docs(fluxing): …` ⇒ 本 handoff §P + 接力 prompt + `CLAUDE.md` 的 §P 指针条目
  ⇒ **入库后 ahead = 6**（**订正草稿算术**：草稿写「ahead = 5」是错的 —— 3 + 2 代码/版本笔 + 1 文档笔 = **6**；实测：入库前 ahead 3 ⇒ 代码/版本两笔后 **ahead 5** ⇒ 文档笔后 **ahead 6**）。文档笔 sha 在提交后才生成 ⇒ **不在本节写死**（见 `git log --oneline -3`）。
- **订正 §P 草稿 D 段的两处错误（以本节为准）**：
  - **`env.bat` 不入库**（AGENTS.md §4.2：local-only + gitignored；A3/A10）—— §P 草稿误列，已删除。版本载体入库的只有 `version.txt`。
  - **`.specify/specs/103-client-bucket-sync/spec.md` 本轮未改**（工作树干净）—— §P 草稿写的「（AC-213/214）」是**悬空引用**（见 P-C-8），已删除该条。
- **禁止入库**：` m librime`（dirty submodule 红线）、`release/`、`reports/**`、`scripts/verify/*.ps1`、根目录 `msbuild1.log` / `msbuild2.log`。
- commit 前已跑 `git status -sb` + `git diff --stat`（**严禁 `git add .`** ⇒ 一律按路径显式 `git add`）。

### P-E. 未闭环 / 下一步（§Q 候选）

1. **真机阶段 1/2 复跑**（需用户在场：约 1.5–2.5 h 连续窗口、**不得锁屏**；装机本身会短暂中断输入法）—— **本轮未跑完**：§M.8.1 的根因归因已订正（P-F-1），须按 **P-F-4 的 runner** 重跑；判据 = 诊断行 `bucket phase=1 pushed>0` **且**生产 `sync_buckets > 0`。**阶段不得自动递增**。
2. **探针 v5 重编**（本轮按指令未做）—— 须同时看 `reconcile` 与 `bucket phase=` 行，并对**桶路径**三态判定 PASS/FAIL/UNDETERMINED；沿用 v4 的「Write-Host 而非 Write-Output」修复（见 §J-8）。
3. **P-B-1 注释订正** + **P-B-2 规则 1 判定口径**（含阶段 1 完全不生效的处置） + **P-B-3 配额口径按桶轮重算** + **P-B-4 更强前提设计** + **P-B-5 配额权威值拍板** + **P-B-6 阶段 3 硬前置补进 ADR**。
4. **AC-09 仍 FAIL** —— 唯一转 PASS 路径 = 复跑日志出现 `AC09_VERDICT=PASS`（规则 1/2 已落地，**待真机轮**）。
5. 生产 `sync_buckets` 实测恒 **0 行** ⇒ 桶推路径真机仍未验证；**不得据此改记 AC-09**。
6. 待用户单独授权：改生产 `cloud/fluxing-sync/wrangler.toml` 阈值 80,000→**20,000** + deploy；BR-②′ 全局同步写熔断（硬要求**只拦同步写、不拦登录续期**）；**阶段 3 切读开闸**（§O-1.1 硬前置：云端历史 40,061 条须先完成桶化覆盖 + 分批方案，且「双写→校验→切读→退役」四段**不得跳段**）；BR-⑤ jury.xin 自建排期；⑤ 腾讯云 cloudflared 0666→0600 与 5432/3306/9100 公网监听收口；⑥ 两个定时任务是否合并（推荐保持分离，§J-7）。
7. 已拍板勿再问（2026-11-25，commit `f8de78f`）：不升级 Workers Paid ⇒ **D1 免费 10 万行/日（每库全局口径）是长期硬约束**；护栏维持每账号口径但阈值降到 20,000；BR-①g 闸门**有条件开**（仅放行两条规则 + 真机阶段 1/2 复跑）。

### P-F. 本轮新发现（★ §M.8.1 根因订正 · 真机 runner 要点）

**F-1 · §M.8.1 登记的根因「桶推被『库存扫描完整』闸门挡住」= 误判，本轮订正。**
- **原登记**（handoff §M.8.1 + CLAUDE.md BR-①g 真机轮条目）：`bucket phase=1 pushed=0 skipped=0 pulled_buckets=0 pulled_items=0` 的根因 = `CloudSyncService.cpp:1608-1611` 的「库存扫描完整」闸门，因老端点每轮仅拉 1,000 条 vs 云端 40,061 条 ⇒ 需约 41 轮 ⇒ 闸门常闭 ⇒ 一轮桶都没推。
- **订正（源码 + 真机日志交叉坐实）**：闸门条件里的 `bucket_scan_complete` 来自**桶端点**拉取（cpp:1273-1289，`= !bucket_more`），**不是**逐条端点的 40,061 库存扫描；逐条端点的扫描完整性另记在 `reconcile scan_complete=…`（cpp:1320-1322，本轮实测 `scan_complete=0` ⇒ 那一路确实没扫完，但**它不喂桶推闸门**）。生产 `sync_buckets` = **0 行** ⇒ 桶端点一轮即扫完 ⇒ `bucket_scan_complete = true` ⇒ **闸门本应打开**。
- **真实根因 = 同步轮的方向**：`CloudSyncEngine.cpp:205-213`「**启动轮只拉不推**」——引擎**启动后的第一轮**方向固定为 `SyncDirection::kPullOnly`（`startup_pending_` 消费后置 false），此后每轮才都是 `kBoth`。而桶推闸门 `CloudSyncService.cpp:1611-1613` 要求 `direction == kPushOnly || direction == kBoth` ⇒ **kPullOnly 轮里桶推结构性关闭** ⇒ 诊断行必然是 `pushed=0 skipped=0`（`skipped` 同样是 0 ⇒ 桶推**函数根本没被调用**，不是「推了但被跳过」）。
- **旁证（三条同时成立）**：① 真机日志 `phase1-probe.log:12` 的桶行与 `:15` 的 reconcile 行**同一毫秒**（`10:27:41.769`）⇒ 同一轮；② `pulled_buckets=0` ⇒ 桶端点确实被调用过（若桶模式未生效则连桶行都不会有 phase=1）；③ `reconcile matched=67` ⇒ 逐条端点**拉到了数据** ⇒ 该轮方向 ∈ {kPullOnly, kBoth}，而桶行 `skipped=0` 排除了「进了桶推但被守卫跳过」。
- **对既往结论的影响**：§M.8.1/§O-4 的「连续 ≥41 轮让库存扫描完成 ⇒ 桶推闸门放行」这条工序**前提错误**（闸门不由逐条库存完整性决定）⇒ **该 41 轮口径作废**，改为 P-F-4 的「等第 2 轮（kBoth）」。§M.8.1 里「阶段 3 会一次性放行约 94,604 条 ⇒ ≈190,000 行写入 ⇒ 击穿 8 万行预算」的风险**仍然成立**（阶段 3 切读 + 逐条端点退役 = 一次性全量上云），**该拦截不得撤销**。

**F-2 · 桶推闸门的真实开关在「轮的方向」，而非「库存完整性」。**
- 阶段 1/2 的首轮（kPullOnly）**永远**推 0 桶 ⇒ **任何只看首轮的探针都会给出 `pushed=0` 的假阴性** ⇒ 现有 `scripts/verify/probe-br1g-bucket-phase.ps1` 的判据 `if ($newText -match 'bucket phase=') { break }`（第 71 行）**必然只抓到首轮** ⇒ 这是 §M.8.1 假阴性的第二个成因（探针侧），与 F-1（引擎侧）叠加。
- 连带后果：§M.8.1 的 `LEGACY_PUSH_LINES=0` **不能**读作「legacy 零写入的证据」——该轮是 kPullOnly，legacy 推送路径（cpp:1502-1608）**根本没进**。legacy 零写入要等 **kBoth 轮**再看 `reconcile pushed=`。

**F-3 · kBoth 轮的写入量预判（真机前的安全评估，本轮推断）。**
- legacy 侧：cpp:1509-1510 跳过 `deferred[i]`、cpp:1514-1515 跳过 `legacy_baseline[i]` ⇒ 54,536 条 deferred + 旧口径基线条**都不进 legacy 推送集合** ⇒ **预期 legacy 零写入**（不会复现 2026-11-23 的 40k 行洪峰）。
- 桶侧：P-B-2 已坐实规则 1 在阶段 1 **不生效** ⇒ `merged`（cpp:1445-1446，含全部本地条目）分桶后 ≈1,024 桶全部「指纹变化」⇒ 受规则 2 配额（2,000 行/轮 ÷ 2 行/桶 ≈ 1,000 桶/轮）截断 ⇒ **首轮推 ≈1,000 桶（≈2,000 D1 写行 + ≈1,024 索引行），剩余 ≈24 桶顺延下一轮** ⇒ 两轮完成全量桶化覆盖。行数远低于 80,000/账号/日护栏与 D1 免费 10 万行/日全局额度 ⇒ **阶段 1/2 首轮不存在额度击穿风险**（推断，待真机实测 `rows_written_24h` 增量确认）。
- **阶段 3 仍然禁止**：切读后逐条端点退役 ⇒ 一次性全量上云 ⇒ ≈190,000 行 ⇒ 击穿护栏 + 复现 2026-11-23 生产事故（§M.8.1 拦截，**继续有效**）。

**F-4 · 真机 runner 的硬要求（下一轮照此写/改）。**
- **必须等「第 2 轮」**：第 1 轮 = 启动轮（kPullOnly，桶推结构性关闭）；第 2 轮起 = kBoth（桶推闸门开）。
- **轮次节奏**（`CloudSyncEngine.h`:105-118 默认值）：启动轮 `startup_delay_ms = 0`（**立即**）；本地改动防抖 **5 分钟**；推送全局最小间隔 **15 分钟**；**定时轮 60 分钟** ⇒ 若期间无本地改动，第 2 轮最坏在 **60 分钟后**；`daily_push_limit = 100` / `daily_pull_limit = 50`（每日推/拉次数上限）。
- **可加速路径**：在窗口内**新增/修改任一词条**（`NotifyLocalChange`）⇒ `dirty_since` 起算 ⇒ 防抖 5 分钟后触发 kBoth 轮（`min_push_interval` 以 `last_push_ms` 兜底，首次为 0 ⇒ 不额外压制）⇒ **约 5–6 分钟即可拿到 kBoth 轮**。这需要用户在场敲几个词（**不得锁屏**）。
- **判定口径**：kBoth 轮应同时出现 `bucket phase=1 pushed>0`（桶推路径首次真机验证）与 `reconcile … pushed=0`（legacy 零写入 ⇒ AC-09 的前置条件之一）；`bucket quota=…` 行给出 `held_unproven` / `held_quota` / `rows_pushed` / `rows_held` / `quota_rows` 五个新计数。
- **禁止**：本轮与下一轮都**不得**把阶段推到 **3/4**（`FLUXING_CLOUD_SYNC_BUCKET_PHASE` 只能取 1/2；回滚 = 设 0 跑一轮）；`scripts/verify/run-br1g-phases.ps1` **含 phase 3 循环，不可直接使用** ⇒ 下一轮须另写只跑 1/2 的 runner（放 `reports/fluxing-br-1g/`，**不入库**），沿用其「User 级变量 + 父进程 `$env:` 同时设置」的修正。

**F-5 · 本轮 §P 的取证纪律复盘（供下一轮直接用）。**
- 装机态复核**只读、可复现**：注册表三处（`HKLM\…\Fluxing\Weasel` 的 `InstallDir` = `D:\Program Files\fluxing`；`HKCU\Software\Fluxing\Weasel` 的 `RimeUserDir` = `D:\Program Files\fluxing\user1\fluxing`；卸载项 `DisplayVersion` = `1.3.10`）+ 文件（`D:\Program Files\fluxing\weasel\WeaselServer.exe`）+ 进程（`Win32_Process` 取 `ExecutablePath` + `Get-Process` 取 `StartTime`）⇒ **单路径成立**（`D:\Program Files\weasel` 不存在，`user1\fluxing\rime_ice.userdb` 在位）。
- **三处哈希取法**：构建侧 `output\Win32\WeaselServer.exe`（**不是** `output\WeaselServer.exe`，后者不存在）；包内侧 `output\7z.exe e … -o<tmp> WeaselServer.exe`（绝对路径，**不要 `Set-Location`**）；装机侧 `Get-FileHash -LiteralPath`。
