# 批次 BR · 推送写放大治理 + 失败注册零写入化（ADR 0076）· 交接文档

- **生成时间**：2026-11-23
- **分支**：`feat/100-cloud-sync`（远程 `kizemo/feat/100-cloud-sync`）
- **上游**：`origin` 只读
- **本批真源**：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md` §9-3
- **本批决策源**：`F:\soft\00selfmade\rime_claude\docs\adr\0076-cloud-write-amplification-governance-and-zero-write-registration.md`（Status = **Proposed**）

## §0 结论速览

- **本批性质** = **ADR 0076 已落盘 + 取证完成**；**改码未开工**（未 push / 未 deploy / 未改客户端 / 未 bump 版本号）。
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

## §2 下一步（严格按 ADR 顺序，先服务端、客户端另行授权）

- **T1（服务端 · 子项②）零写入化**：按 ADR D1 落地 —— 拆纯生成函数（`{ code, codeHash, createdAt, expiresAt }`，不发信不落库）→ 注册顺序改为「查重 → 内存生成 → 发信 → INSERT users → INSERT email_verifications」→ 重发同理（成功才 INSERT，失败一行不写）。窄窗口「发信成功但落库失败」⇒ 503 `storage_unavailable` + 中文文案 + 沿用 `rollbackRegister` 三态兜底。**不动** ADR 0075 已发布的错误语义与三个 ASCII 头。
- **T2（服务端 · 子项①(a)）写预算护栏**：ADR D2-3 —— 落库前用纯读估算本账号 UTC 今日已写行数 + 本批 `planned`，超 80,000 行 ⇒ 整批拒绝 429 `write_budget_exceeded` + 中文文案 ⇒ 零写入。阈值走环境变量。**不做** `DELETE+INSERT` 改写（D2-4）、**不做** `seq` 列迁移。
- **T3（服务端 · 子项①(b)）设备/令牌上限**：ADR D3 —— 设备上限 32 台（先回收已撤销且最久未见，仍超则拒绝登录 429 `device_limit_reached` + 中文文案，**不自动删活跃设备**）；refresh 有效上限 64（超限撤销最旧有效行）；过期行 30 天保留窗口 + 登录/续期顺带 GC（每次最多 200 行）。**如实标注**：GC 不省额度（行删除同样计写入）。
- **T4（单测）**：服务端 `npm run check` 既有 **5 文件 / 86 用例**（BQ 基线）逐条保留；本批新增组断言（目标 ≥ 20 条）覆盖 D1 零写入 / D2-3 超限拒绝 / D3 上限与 GC。`npm run typecheck` 必须 0；改动文件 U+FFFD = 0。
- **T5（部署 + 生产复验，需授权）**：`wrangler deploy` 后按 ADR D4-2 四条判据复验（保留域注册 422 且 `users`/`email_verifications` 零增量 / 重发失败零增量 / 写预算 429 且零写入 / 设备上限拒绝并给中文文案）。
- **T6（需用户授权才可开工）**：BR-①c 客户端「远端 HLC 沿用」（判据：删 sidecar 后同步一次，`sync_items` 今日写入增量 = 0）；BR-①d sidecar 落点加固（低优先）。

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

**推断（由实测 + 读码推导，未单独复验）**

- sidecar 丢失 / 换机 / 重建用户数据目录 ⇒ 40,061 行全部重写（≈ 80,122 行写入）—— 推导链完整，但**未在真机制造 sidecar 丢失**验证过。
- 写预算按「今日已写行数」估算 ⇒ 同一行当日反复写只计 1（护栏不是精确计费器）。

**未验证 / 未闭环**

- BR-①c 客户端「远端 HLC 沿用」**未实现、未授权** ⇒ 最大放大源在本批之后**仍未根治**（现由 T2 护栏兜住）。
- ADR D1-5 窄窗口（发信成功但落库失败）**未在生产复验**（需构造 D1 配额耗尽 + 发信成功）。
- ADR D2-3 写预算护栏的**拒绝路径**未在生产复验。
- ADR D3 设备上限 / refresh 上限**未在生产复验**（需构造 32 台设备 / 64 个有效 refresh）。
- 口令重置路径（`password_resets` 先落库后发信）**同构缺陷仍在**（ADR 0075 决策 7 明确不碰；ADR 0076 D1-7 只记录平移配方）。
- `sync_items.seq` 列迁移**未做**（D2-4）。
- D1 免费额度能否承载词典规模**未结论** ⇒ 付费议题仍未决（用户已定：不升级，等免费且可逆）。
- 承接 BQ §12 未闭环项（18 份 `run-*.log` 是否入库、`tmp\bh-c2d-fmt-check.cpp` 处置、R2 残留 164 孤儿对象是否逐个删除、2 个 `ak.probe.*@example.com` 账号是否删除）本轮**未处理**。

## §4 证据路径

- ADR：`F:\soft\00selfmade\rime_claude\docs\adr\0076-cloud-write-amplification-governance-and-zero-write-registration.md`
- 服务端：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\{sync,auth,wire,devices,account,mail}.ts`、`migrations\0001_init.sql`
- 客户端（只读取证）：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\{CloudCrypto,CloudSyncService,CloudSyncEngine,CloudAccount,CloudAccountDialog,CloudCredentialStore}.{h,cpp}`
- 门禁：`npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run typecheck` / `run check`
- BQ 复验脚本（凭据只在 tmp，禁止入库）：`F:\soft\00selfmade\rime_claude\tmp\{bq-verify.ps1,bq-e2e-mailtm-v4.ps1,bq-v4-deploy-verify.ps1,bq-d1-cleanup.ps1}`

## §5 本轮工具坑

- `Set-Location` **不改 .NET 进程 CWD** ⇒ `[IO.File]::ReadAllLines('src\auth.ts')` 会报「找不到路径」⇒ 一律用绝对路径。
- CJK 文件写入必须字节级：`-join "`n"` 拼行 + `[IO.File]::WriteAllText(path, text, UTF8Encoding($false))` ⇒ 避免 PS 默认编码污染；`CHANGELOG.md` 是 CRLF-only，handoff / ADR / prompt 是 **LF-only**，`CLAUDE.md` 插入段按文件自身行尾拼。
- PowerShell 单引号字符串里 **反引号是字面量** ⇒ markdown 的反引号只能在单引号串里写；双引号串会吃掉反引号。
- **bash 里长篇中文内联脚本容易被本地安全网关误判为删除类命令而整体拦截** ⇒ 正文不要出现删除命令字面量；长文落盘改用专用写文件工具，bash 只做小补丁。
- `wrangler d1 execute --command` 的 SQL 必须压成单行；Cloudflare API 与 mail.tm 必须直连（清代理变量）；`wrangler tail` 本机彻底不可用 ⇒ 诊断一律靠响应头自证。
- 文件删除用本机受信任的可恢复删除器（路径见 `CLAUDE.md` 工具坑），不要用 `Remove-Item`（被本地硬安全策略拦截）。

## §6 Git 与提交现状

- 开工时：`## feat/100-cloud-sync...kizemo/feat/100-cloud-sync [ahead 2]`（BQ 的 `314d03e` fix + `d628e53` docs **已入库未 push**）。
- 工作树：仅未跟踪杂项（`release/`、`reports/**`、`scripts/verify/*.ps1`、`docs/design/quickpanel-v6/design-demos/*.html`）+ `librime` dirty（`m`，**不得 `git add librime`**）。
- 本轮提交：`docs(fluxing): ADR 0076 推送写放大治理与失败注册零写入化`（单 scope；**显式按路径 git add，禁用 `git add .`**）。
- **未 push / 未 deploy / 未改客户端**（交接 §0/§9-3 要求明确指令）。
