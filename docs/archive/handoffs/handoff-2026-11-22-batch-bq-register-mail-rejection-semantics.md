# 批次 BQ 交接文档 · 注册发信失败语义修正 + 失败注册回滚（ADR 0075）

- 日期：2026-11-22
- 批次：BQ（接 BP；本批 = BO §7 遗留的「`/v1/auth/register` 恒 503 `mail_send_failed`」独立修复，用户指令「开做 BQ 注册 503 修复」）
- 分支：`feat/100-cloud-sync`（远程 `kizemo/feat/100-cloud-sync`）
- 性质：**服务端产品修复 + 单测门禁 + 生产部署 + 部分复验 + 文档**（**未改** `librime/`、**未改** IPC 协议与 `include/WeaselIPCData.h`、**未改** `WeaselServer/**` 客户端、**未新增**依赖、**未 bump** 版本号）
- 授权：用户指令已授权 `cloud/fluxing-sync/**` 改动 + `wrangler deploy` 直达生产 `api.aiec.fun`
- **本批未提交**（代码 / 测试 / ADR / 文档改动全部留在工作树，等用户拍板）

---

## 0. 接力结论速览（2026-11-22 22:55）

**2026-11-23 08:41 定时轮补充**：D1 写额度**有余**（两条独立真实写各 1 行成功）⇒ P0 已彻底解除；`bq.%` 残留**零复发**、`sync_items` 40,061 未变 ⇒ 08:15 清理成果保持有效；**§13 更正**：`users` 4 行 = 2 真实账号 + 2 `ak.*` 探针（上一轮「1 条前缀未核对探针行」为误判）；**新发现**写放大第二来源 = 设备 / `refresh_tokens` 无上限增长（22 devices / 90 tokens）⇒ BR 子项① 拆两条线；新增 §8.4（证据）与 §11-11（多行 SQL 坑）。**2026-11-23 09:15 提交回填**：用户已拍板「**现在提交，分 docs 与 fix 两个 commit**」⇒ **BQ 批已入库**（fix 一个 + docs 一个，具体 SHA 见 §14 / `git log -2`）；BR 批按用户拍板「**下轮开工，先出 ADR 0076**」（子项① 双线 + 子项② 零写入化）。**下一会话第一件事**：按 §8.4(4) 与 §9-3 开工 BR 批（ADR 0076），**未获明确指令不得 push / deploy / 改客户端**。
- **本批最重要的产出不是「修好了 503」，而是「把一个假象拆成两个真问题」**：
  1. **BO 的「注册链路整体不可用」口径被推翻** —— 根因是上游对**保留域收件人**（`example.com` = RFC 2606/6761 保留域、null MX）返回**永久拒绝 422**，被旧错误映射伪装成「可重试的 503」；对照实验证明**真实域收件人注册返回 200**（body 含 `account_id` / `salt` / `kdf` / `need_email_verify=true`）⇒ **注册链路对真实邮箱可用**。
  2. **生产 D1 每日行写入额度耗尽（Cloudflare `code 7500`）** —— 这是**独立事故**，影响面**不止注册**（云同步推送要写 `sync_items`），处置需用户拍板（§8）。
- **已达成**：上游失败三类分流（永久拒绝 ⇒ 422 `mail_recipient_rejected`；配置类 ⇒ 500 `server_misconfigured`；429 ⇒ 503 `mail_rate_limited`；409/5xx/网络 ⇒ 503 `mail_send_failed`）、失败注册回滚（三态 `ok`/`degraded`/`failed`）、诊断头 `X-Fluxing-Mail-Provider-Status` / `X-Fluxing-Mail-Rollback` / `X-Fluxing-Mail-Rollback-Error`；门禁 `TSC_EXIT=0` + `CHECK_EXIT=0`（**5 文件 / 86 用例**，基线 4/65）；生产 Version **`96facaba-9277-4c77-b9f9-4df5cb6de41e`**；复验①（保留域 ⇒ 422 + 三头）**生产实测达成**。
- **未达成（三条判据）**：零残留 / 立即重试不撞 429·409 / 真实域端到端 —— **阻塞点全部在 D1 配额**（DELETE 与 UPDATE 均被拒 ⇒ 连降级兜底都失效），**不是本批代码**；配额恢复（UTC 午夜 ≈ 北京次日 08:00）后用 `tmp\bq-verify.ps1` 与 `tmp\bq-e2e-mailtm-v4.ps1` 复验。**2026-11-23 08:15 定时轮复验：UTC 翻页后 D1 额度已恢复 ⇒ BQ 判据 ③④ 全部达成**（回滚 `ok`、探针三表零残留、立即重试回到 422 不再撞 429、15 行探针残留已清零；实测见 §8.3）。**根因未消除（客户端全量推送写放大）⇒ 转 BR 批治理。**
- **下一会话第一件事**：先读 §8 / §8.2 / **§8.3**（**判据③④ 已于 08:15 达成、残留已清零**）；用户**仍未拍板** D1 处置与 BQ 是否 commit ⇒ **不要自行 commit / redeploy**；接着按 §8.3(4) 与 §8.2(4) **立 BR 批**（① 推送写放大治理；② 失败注册「零写入」化 —— 需 ADR 0076 + 用户拍板）。

---

## 1. 当前状态（一段话）

BO 批登记的独立缺陷「`/v1/auth/register` 恒 503 `mail_send_failed` ⇒ 新用户注册链路不可用」在本批被**重新定性并部分修复**。BQ 用**对照实验**（同上游、同时刻、同密钥、同 `from`，只换收件人域）证明：保留域 `example.com` 被上游**永久拒绝**（生产实测上游实际返回 **422**），真实域 `qq.com` **注册返回 200** ⇒ 旧代码把上游 4xx 一律映射成「503 请稍后重试」，让**永久拒绝伪装成临时故障**，且失败注册**仍留下账号行与验证码行**（占号 + 消耗 60 秒重发闸窗口 + 生产留僵尸账号）。本批按 ADR 0075 落地：上游失败改为**三类分流**，4xx 永久拒绝 ⇒ **422 `mail_recipient_rejected`** + 中文文案「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」（客户端只透传 `error.message`、且 **422 不在自动重试集合** ⇒ **客户端零改动即语义正确**）；失败注册**回滚**（DELETE 优先，被 D1 拒则降级 UPDATE 失效化，三态 `ok`/`degraded`/`failed`）；新增三个 ASCII 诊断头（上游 status / 回滚状态 / 回滚失败原因）⇒ **绕开 `wrangler tail` 不可用的死角**，上游原文首次可从外部自证。门禁 `TSC_EXIT=0`、`CHECK_EXIT=0`（**5 文件 / 86 用例**，基线 4/65，既有 65 条逐条保留），已 `wrangler deploy` 直达生产（Version `96facaba-9277-4c77-b9f9-4df5cb6de41e`，迭代史 v1→v4 见 ADR 0075）。生产复验①**达成**：保留域探针 ⇒ **HTTP 422** + `mail_recipient_rejected` + 上述中文文案 + `X-Fluxing-Mail-Provider-Status: 422`（**上游实际 422**）+ `X-Fluxing-Mail-Rollback: failed` + `X-Fluxing-Mail-Rollback-Error`（含 D1 配额原文）。**未达成**：判据③④⑤（零残留 / 立即重试 / 真实域端到端）被**生产 D1 每日行写入配额耗尽**（Cloudflare `code 7500`）阻塞 —— 账号变成「**能建、不能改、不能删**」，影响面**不止注册**（云同步推送写 `sync_items` 同样受影响）。

---

## 2. 必读文档（全部为绝对路径）

| 用途 | 路径 |
|---|---|
| **本批真源（先读这份）** | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md` |
| 进站第 1 份 · 身份/模块/不变量 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` |
| 进站第 2 份 · 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接规则） | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 进站第 3 份 · 法则层（R1–R9 / P1–P8） | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 进站第 4 份 · 部署 + NSIS 配方 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` |
| 进站第 5 份 · 工程技能消费契约 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` |
| **本批决策依据（ADR 0075 · 已 Accepted · 含实测回填）** | `F:\soft\00selfmade\rime_claude\docs\adr\0075-register-mail-rejection-semantics-and-failed-registration-rollback.md` |
| ADR 索引（含 0075 行） | `F:\soft\00selfmade\rime_claude\docs\adr\README.md` |
| 上一批真源（BP · 推送吞吐 / 12002） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bp-push-throughput-server-concurrency.md` |
| 再上一批（BO · 12002 根因定案 · §7/§9-6/§11-6 已由 BQ 修订） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md` |
| 规格 100（本批未改） | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` |
| 服务端真源（发信 / 注册 / 错误映射） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\mail.ts`、`src\auth.ts`、`src\errors.ts`、`src\index.ts` |
| 服务端单测（本批新增 + 追加） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\register-mail-failure.test.ts`（新建）、`test\mail-http.test.ts`（+7 条） |
| 服务端既有单测（未改） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\api.test.ts` |
| 客户端真源（**本批禁改**，仅供理解语义） | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccount.cpp`（`:92-106` `ExtractServerError`）、`CloudTransport.cpp:105-113`（`ShouldRetry`）、`CloudAccountUi.h:41`（`kIdStatus=1006`） |
| D1 schema（users / email_verifications / mail_outbox / password_resets） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\migrations\0001_init.sql`、`0002_password_resets.sql` |
| 部署配置（路由 / D1 / R2 / vars） | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\wrangler.toml` |
| 本批复验脚本（**凭据只在内存 / tmp，禁止入库**） | `F:\soft\00selfmade\rime_claude\tmp\bq-verify.ps1`、`tmp\bq-e2e-mailtm-v4.ps1`、`tmp\bq-v4-deploy-verify.ps1`、`tmp\bq-d1-cleanup.ps1` |

---

## 3. 本批逐项进度

| # | 事项 | 状态 | 证据 |
|---|---|---|---|
| 1 | 根因定案（对照实验 + null MX + 上游错误表） | ✅ | ADR 0075「事实 1–3 / 9」 |
| 2 | ADR 0075（8 条决策 + 实测回填） | ✅ | `docs/adr/0075-…md` |
| 3 | `docs/adr/README.md` 索引补 0075 行 | ✅ | `git diff --stat` = +1 |
| 4 | 服务端改码：`mail.ts` 分类 / `errors.ts` 诊断头 + 422 / `auth.ts` 回滚 + 降级兜底 | ✅ | §5 落点表 |
| 5 | 单测：既有 13 条逐字保留 + 追加 7 条 + 新建 `register-mail-failure.test.ts`（13 条） | ✅ | `TSC_EXIT=0`、`CHECK_EXIT=0`、5 文件 / 86 用例 |
| 6 | 部署生产 `api.aiec.fun` | ✅ | `DEPLOY_EXIT=0`，Version `96facaba-9277-4c77-b9f9-4df5cb6de41e` |
| 7 | 复验①保留域 ⇒ 422 + 三头 | ✅ | §6.1 |
| 8 | 复验③零残留 / ④立即重试 / ⑤真实域端到端 | ❌ **被 D1 配额阻塞** | §6.2 / §8 |
| 9 | BO 交接 §7 / §9-6 / §11-6 口径修订 | ✅ | §4 |
| 10 | 本批交接 + relay prompt | ✅ | 本文件 + `docs\archive\prompts\prompt-next-session-2026-11-22-batch-bq-register-mail-rejection-semantics.md` |
| 11 | 敏感 tmp 文件清理 | ✅ | §11 |
| 12 | D1 探针行清理 | ❌ **待配额恢复** | §13 |
| 13 | commit / push | ⏸ **等用户拍板** | §12 |
| 14 | 客户端构建 / 出包 / 装机 | ➖ **不需要**（客户端零改动） | ADR 0075 决策 6 |

---

## 4. 根因定案（决定性对照实验）

**同上游、同时刻、同密钥、同 `from`、同请求形状，只换收件人域**：

| 收件人域 | 结果 |
|---|---|
| `@example.com` | HTTP **503** `mail_send_failed`（BO 轮实测，耗时 2567 ms） |
| `@qq.com` | HTTP **200**（1585 ms），body 含 `account_id` / `salt` / `kdf{PBKDF2-SHA256,600000}` / `need_email_verify=true` |

⇒ **发信密钥、发件域 `aiec.fun`、`from` 形状、鉴权头形状全部可用**；拒收与**收件人域**相关。
`Resolve-DnsName example.com -Type MX` ⇒ **`./0`**（null MX，RFC 2606/6761 保留域，声明不接受任何邮件）；`qq.com` 有真实 MX ⇒ 被拒的是**根本收不到信的收件域**。

**旧代码的第二个错误**（`src/mail.ts:196-223`）：上游 4xx 一律映射成 **503 `mail_send_failed`「请稍后重试」**（Resend 官方错误表：除 429/409/5xx 外，4xx 语义上都是「重试无用」的永久拒绝）⇒ 永久拒绝被表述成临时故障；客户端又对 5xx 自动重试（第 2 次撞 60 秒闸）⇒ **用户看到的是「验证码发送过于频繁」，真实原因被二次症状覆盖**。

**第三个错误**（`src/auth.ts`）：发信失败时 `users` 行与 `email_verifications` 行**都已提交** ⇒ 占号（重试注册先撞 429 再撞 409）、失败仍消耗 60 秒窗口、生产留僵尸账号。

---

## 5. 代码改动落点（5 文件，改 4 + 新增 1 测试）

| 文件 | 落点 |
|---|---|
| `cloud/fluxing-sync/src/mail.ts` | `:51` `PROVIDER_CONFIG_ERROR_NAMES`（14 个配置类 name）；`:82` `parseUpstreamErrorName`；`:109` `classifyUpstreamFailure`；`:150` 422 中文文案；`:271` `extractVerificationCode`；`:280` `redactSensitive`（导出）；`:290-355` `sendViaHttp`（`:336-342` 分类 + 日志，`:343-351` 仅网络类重试）；`:363` `sendMail` 三模式；`:406` `sendVerificationMail` |
| `cloud/fluxing-sync/src/errors.ts` | `:8` `PROVIDER_STATUS_HEADER`；`:14` `ROLLBACK_HEADER`；`:20` `ROLLBACK_ERROR_HEADER`；`toAsciiHeaderValue`（新增，`[^\x20-\x7e]` → `?`）；`D1_ROW_WRITE_QUOTA_PATTERN`（`/\[code:\s*7500\]/`）；`ApiError(status, code, message, providerStatus?, rollbackStatus?, rollbackError?)`；`:67` `fail.unprocessable(code,message)`（422，**参数序与其它工厂相反，已注释**）；`:88` `toErrorResponse`（写 3 个诊断头；未知异常命中 7500 ⇒ 503 `storage_quota_exceeded`「服务端存储写入额度已用尽，请稍后重试。」） |
| `cloud/fluxing-sync/src/auth.ts` | `:29-31` `CODE_TTL_SECONDS=600` / `RESEND_INTERVAL_SECONDS=60` / `MAX_VERIFY_ATTEMPTS=5`；`:224-237` `assertResendAllowed`；`:249` `createVerificationCode`（造码与发信拆开）；`:273` `RollbackOutcome`（`ok`/`degraded`/`failed`）；`:276` `RollbackReport`；`:282` `scrubForDiagnostics`（`[redacted]`）；`:291` `deleteRow`；`:307` `neutralizeUser`（`UPDATE users SET email='deleted+<userId>@invalid.local'`）；`:324` `neutralizeVerification`（`UPDATE email_verifications SET created_at=0`）；`:349` `rollbackRegister`（ASCII `problems.push` 模板）；`:386` `rollbackVerification`；`:405` `withRollbackStatus(error, report, email)`；`:423` `handleRegister`；`:482` `handleVerify`（**未改**）；`:531` `handleResend` |
| `cloud/fluxing-sync/test/mail-http.test.ts` | 既有 **13 条逐字保留** + 追加 **7 条**（400→422 / 400+API key 文案→422 / 422 missing_required_field→500 / 404→500 / 502→503 / 非 JSON 400→422 / 409→503） |
| `cloud/fluxing-sync/test/register-mail-failure.test.ts`（**新建**） | **13 条**：零残留 / 立即重试不撞 429·409 / 上游 500 零残留 / 重发失败保留账号+删本次验证码行 / 立即再重发不撞 429 / 成功路径不受影响 / 日志脱敏 / 诊断头带·不带 / DELETE 被拒但 UPDATE 成功 ⇒ `degraded`（占号解除、可立刻重试）/ DELETE 与 UPDATE 都被拒 ⇒ `failed` + 诊断头带 `neutralize failed` / 两条 ASCII 守卫断言（`/^[\x20-\x7e]*$/`） |

**`src/index.ts` 未改**（`:19` import `toErrorResponse`、`:68/73/78` 路由、`:143-144` catch ⇒ `toErrorResponse(error)` 已是既有形状）。

**三条关键实现纪律**：
1. **回滚绝不调用 `DB.batch`**（batch 是整体事务，一条被拒 ⇒ 全部连坐；生产实测 batch **一条都没删掉**）⇒ 逐条执行。
2. **回滚降级兜底**：DELETE 被 D1 拒 ⇒ 改 UPDATE 失效化（解除占号 + 解除 60 秒闸）⇒ `degraded`；两条都失败才 `failed`。
3. **诊断头载荷必须 ASCII**（Fetch 规范：非 ASCII 在浏览器侧抛 `TypeError`）⇒ ASCII 模板 + `[redacted]`；**中文只进服务端日志**。

---

## 6. 生产复验实测（Version `96facaba-9277-4c77-b9f9-4df5cb6de41e`）

### 6.1 复验①（保留域 `@example.com` 探针）—— ✅ 达成

- HTTP **422**、错误码 `mail_recipient_rejected`、文案「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」⇒ **判据「不再是 503」达成**；
- `X-Fluxing-Mail-Provider-Status: 422` ⇒ **上游实际返回 422**（此前推测 400，实测修正为 422）；
- `X-Fluxing-Mail-Rollback: failed` + `X-Fluxing-Mail-Rollback-Error: email_verifications delete failed(D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midni`（截断 160，ASCII）⇒ **回滚被 D1 配额挡住，且根因文本首次从外部自证**；
- 诊断头载荷实测为 ASCII（此前中文载荷会触发 Workers 的「header value contains non-ASCII characters」告警 ⇒ 已按 ADR 0075 决策 8 改为 ASCII 模板）。

### 6.2 复验③④⑤（零残留 / 立即重试 / 真实域端到端）—— ❌ 被 D1 配额阻塞

- 生产 D1 的 `UPDATE` 与 `DELETE` **一律被拒**：Cloudflare `code 7500`，原文「Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue.」；Worker 内部回滚与 `wrangler d1 execute --remote` 手工语句**两条路径同样被拒**。
- 同时刻对照：**INSERT 仍可用**（探针 `users` / `email_verifications` 行数持续增长）；**UPDATE 在额度耗尽前仍可用**（23:05 一次 9 行批量 UPDATE 成功，`changes=9` / `rows_written=18`），额度耗尽后连单行 UPDATE 也被拒 ⇒ **账号变成「能建、不能改、不能删」**。
- ⇒ 判据③「零残留」与④「立即重试不撞 429/409」在配额恢复前**无法成立**（降级兜底也依赖 UPDATE，一并被拒）；判据⑤端到端同样受阻（`verify` 要写 `consumed_at` / `email_verified`）。
- **端到端的第二个独立阻塞**：一次性收信服务 `mail.tm` 的 token 端点对本探针地址持续 **401 Invalid credentials**（账号创建 201 成功；加 6 秒开通延迟 + 4 次重试仍 401）⇒ 取不到验证码，**与本服务无关**；`mail.gw` 直连被拒（connection refused）。
- **建议复验路径**（配额恢复后）：`tmp\bq-verify.ps1`（保留域 ⇒ 422 + 三头 + D1 计数）、`tmp\bq-e2e-mailtm-v4.ps1`（真实域端到端；若 `mail.tm` 仍 401，改用真实邮箱人工端到端，**需用户在场**）。

---

## 7. 门禁实测

| 项 | 判据 | 实测 |
|---|---|---|
| `npm --prefix …\cloud\fluxing-sync run typecheck` | 退出码 0 | `TSC_EXIT=0` |
| `npm --prefix …\cloud\fluxing-sync run check` | 退出码 0，用例数 ≥ 65 | `CHECK_EXIT=0`，**5 文件 / 86 用例**（基线 4/65 ⇒ **+1 文件 +21 用例**；既有 65 条逐条保留） |
| `npm --prefix …\cloud\fluxing-sync run deploy` | 退出码 0，路由 `api.aiec.fun` | `DEPLOY_EXIT=0`，Version **`96facaba-9277-4c77-b9f9-4df5cb6de41e`** |

> 部署迭代史（同批中间版本，仅过程记录）：v1 `a6323b2d-890f-4fcd-aba4-e004288d4392` → v2 `c0244ab0-a376-4383-b451-69fc8d0fdbe3`（回滚改逐条删除 + 回滚诊断头）→ v3 `84f82cf4-f47a-403a-97e9-d0a140e2df7e`（D1 配额 503 语义）→ **v4 `96facaba-9277-4c77-b9f9-4df5cb6de41e`**（回滚降级兜底 + 诊断头 ASCII 化）。

**客户端门禁本批不适用**（客户端零改动 ⇒ 无需 `build.bat` / 出包 / 装机 / sha 比对）。

---

## 8. 【P0 · 生产事故】D1 每日行写入额度耗尽（需用户拍板）

- 账号 `016c8e73…` / 库 `e4ea114a…`；Cloudflare 拒绝原文含 `code 7500`「Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue.」
- **影响面不止注册**：云同步推送要写 D1 `sync_items` ⇒ 额度耗尽期间**真实用户的云同步写路径也会失败**（BQ 范围外，但同一根因）。
- **恢复时间**：UTC 午夜重置 ≈ **北京时间次日 08:00**（本轮实测时刻 2026-11-22 22:55 北京 = 14:55 UTC，尚未到重置点）。
- **处置选项（需用户拍板，属付费/等待决策）**：① 等 UTC 午夜自动重置（免费，但期间云同步写路径失败）② 升级 D1 付费计划（立即恢复，涉及付费）③ 其他（例如降低写放大、错峰批量）。
- **在配额恢复前**：不要反复跑 `tmp\bq-d1-cleanup.ps1`（每条写语句都会撞额度上限，纯噪音）；**读语句（SELECT）始终可用** ⇒ 复盘取证可以继续做。

---

## 8.1 【2026-11-23 00:13–00:45 定时轮复核】实测实录（verified 逐条标注）

> 本节由 2026-11-23 00:13 触发的定时轮会话写入；**未重做** BQ 已完成的取证 / 改码 / 部署，只做「P0 状态确认 + 配额实测 + 判据复验 + 主因取证」。

### (1) 用户拍板状态：**仍未拍板**（verified）

上一会话 `mvs_fe7ef1292a314686b69613916826bd3a` 于 **2026-11-22 23:01（北京时间）**收尾，把 P0 三选项（等 UTC 午夜重置 / 升级 D1 付费 / 其他）抛给用户，**其后无用户回复** ⇒ 三个选项均未选定；BQ 全部改动仍留在工作树未提交。

### (2) 配额状态：**仍耗尽**（verified，00:24）

- Worker 内回滚的 DELETE 被拒，诊断头原文（BQ 新增的头，第一次派上用场）：
  `X-Fluxing-Mail-Rollback-Error = email_verifications delete failed(D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midni…`
- 手工 `wrangler d1 execute DB --remote` 的 DELETE 同样被拒；**INSERT 仍可用、SELECT 始终可用** ⇒ 现状仍是「**能建、不能改、不能删**」，§8 的定性不变。
- **两个判别坑（新增，重要）**：
  1. **0 行写入不能用来判配额**：`UPDATE … WHERE` 不命中任何行时返回 `success: true`、`rows_written: 0` ⇒ **假阳性**。必须用 **≥1 行**的真实写入判别（本轮 00:19 有一次 1 行 DELETE 成功、00:24 同类 DELETE 全被拒 ⇒ 写预算在这 5 分钟内耗尽；**具体计费口径 not verified**）。
  2. **`code 7500` 不是配额专属码**：普通 SQL 错误（如 `no such column: uid`）同样返回 `SQLITE_ERROR [code: 7500]` ⇒ 判别必须看**错误原文**（`exceeded D1's free tier daily row write limit`），不能看码。

### (3) BQ 三条判据复验（复用未改动一行的 `tmp\bq-verify.ps1`）

| 判据 | 结果 | 证据 |
|---|---|---|
| ① 保留域 ⇒ 422 + 三诊断头 | ✅ 仍成立 | `R1_STATUS=422 CODE=mail_recipient_rejected`、`X-Fluxing-Mail-Provider-Status=422`、中文文案逐字正确；`X-Fluxing-Mail-Rollback=failed`（回滚被配额挡住） |
| ③ 零残留 | ❌ **不达成** | D1 残留 `users` 1 行 + `email_verifications` 1 行 + `mail_outbox` 0 行 |
| ④ 立即重试不撞 429·409 | ❌ **不达成** | `R2_STATUS=429 CODE=verify_too_frequent` |
| ⑤ 真实域端到端 | ⏸ 未做 | 第二重阻塞未变：`mail.tm` token 端点对本探针地址持续 401 ⇒ 需真实邮箱在场 |

⇒ **③④ 的阻塞点确认仍是 §8 配额，不是本批代码**；配额恢复后用同一脚本重跑即可闭环（脚本未改一行）。

### (4) 本轮新增探针残留（待配额恢复后清理，共 7 行）

`users` 4 行 + `email_verifications` 3 行，邮箱均为 `@example.com`：`bq.v2.1795358301991` / `bq.v2.1795364194408` / `bq.v3.1795358450930` / `bq.v3.1795364272970`（`bq.v2.1795364194408` 的验证码行已在 00:19 被成功删掉）⇒ 清理口径见 §13。

### (5) **P0 主因新证据：打爆额度的是「客户端自身的全量词典推送」，不是探针脚本**

2026-11-23 00:38 远端只读取账（UTC 11-22 = 北京 11-22 16:00 → 11-23 08:00 这一个 UTC 日）：

| 表 | 累计 | 今日(UTC 11-22) |
|---|---|---|
| `sync_items` | 40,061 | **40,060** |
| `refresh_tokens` | 94 | 32 |
| `users` | 15 | 11 |
| `email_verifications` | 19 | 11 |
| `devices` | 25 | 1 |
| `mail_outbox` / `password_resets` | 4 / 2 | 0 / 0 |

- 今日 D1 行写入合计 ≈ **40.1k 行**，其中 **99.9% 是 `sync_items`**。
- 这 40,060 行**全部属真实账号** `25940969@qq.com`（`user_id d42756f4-866c-465f-a9d4-aa6d47464426`）、`type=userdb`；`COUNT(DISTINCT item_key)=40,061` = 总行数 ⇒ **行级无写放大**（每个 item_key 只落一行）。
- 时间分布（小时桶）：UTC **11:40 / 12:40 / 13:40** = 14,621 / 16,224 / 9,215 行 ⇒ 与 BQ 批次运行期间「本机客户端无输入会话 ⇒ 机会性同步把本地词典一次性推上云」的窗口吻合（北京时间 19:40–22:40）。
- BO / BP 探针脚本合计只写了几百行 ⇒ **不是元凶**（verified）。
- ⇒ **推论（inferred）**：本次打爆额度的主因是**产品自身的同步写路径**（一次全量 4 万条词典推送）；文档标称的免费额度 100k 行/日 与实测 ~40k 即耗尽**不一致** ⇒ 账户实际生效额度口径 **not verified**（可能含 D1 内部 KV 双写计费）。

### (6) 复发风险与建议（供拍板用）

- 云端已存有该账号 **40,061** 条 item ⇒ 若客户端只推变更项，明天实际写入量很小 ⇒ **当天不再打满的概率高（inferred）**；但**重建词典 / 第二台设备 / 换机再推一次全量**就会再次打满。
- ⇒ 建议立 **BR 批「同步推送写放大治理」**：客户端只推变更条目（增量）＋ 服务端仅在内容变化时写行 ＋ 必要时保留期 GC ＋ 明确「免费额度下可承载的词典规模」上限。
- ⇒ **本批建议**：**不升级付费**（等待是免费且可逆的；08:00 后大概率自愈），把付费选项留给「BR 批证明免费额度确实不够用」之后再谈。

---

## 8.2 【2026-11-23 07:42–07:52 定时轮复核】额度判定非单调 + 429 成因源码级定案 + 残留现状

### (1) 结论速览（本轮，verified）

- **配额仍耗尽，且判定非单调**：同一分钟内 **1 行 `UPDATE` 成功**（`changes:1 / rows_written:2`）而 **1 / 2 / 14 行的 `DELETE` 全部被拒**（错误原文 `Your account has exceeded D1's free tier daily row write limit … [code: 7500]`，账号 `016c8e73…`）⇒ **「≥1 行写入成功」不能当作「额度有余」的判据**（补 §11-5）。
- **BQ 判据③④ 仍不达成**，且本轮拿到**源码级因果链**（见 (2)）⇒ **不是本批代码缺陷，是 D1 写额度问题**。
- **新增结构性修法候选（BR 批）**：把 `users` / `email_verifications` 的落库**挪到发信成功之后** ⇒ 失败注册**零写入** ⇒ 判据③④ 结构性达成、**不再依赖 DELETE/UPDATE 额度**（见 (4)）。
- **本轮清理尝试全部未生效**：逐行 `DELETE` ×16 + 批量 `DELETE` ×3，**计数一行没少** ⇒ 残留仍在（见 (5)）。

### (2) `429 verify_too_frequent` 成因已源码级定案（verified，`cloud/fluxing-sync/src/auth.ts`）

| 事实 | 位置 |
|---|---|
| 重发闸只看 `email_verifications.created_at`；距上次发码 < `RESEND_INTERVAL_SECONDS`（60 秒）即抛 `verify_too_frequent` | `auth.ts:224-237`（`assertResendAllowed`） |
| 失败注册回滚的三态设计：**DELETE 成功 = `ok`**（真零残留）；**DELETE 被拒但 UPDATE 成功 = `degraded`**（行还在但已失效化）；**两条都失败 = `failed`** | `auth.ts:337-348`（`rollbackRegister` 注释） |
| 降级路径 `neutralizeVerification` 把 `email_verifications.created_at` **归零** ⇒ 专门用来解除 60 秒重发窗口（注释原文：「用户立刻重试不再撞 429 verify_too_frequent」） | `auth.ts:320-335` |
| `neutralizeUser` 把 `users.email` 改成 `deleted+<id>@invalid.local`（失效化而非删除） | `auth.ts:307-318` |

⇒ **因果链**：回滚被配额挡掉（`failed`）→ 验证码行残留且 `created_at` 未归零 → 立刻重试撞 60 秒重发闸 → `429 verify_too_frequent`。
⇒ **判据④ 的 429 是判据③ 残留的下游症状，不是独立缺陷**（§8.1(6) 只记成「另立观测面」，本轮补上源码依据）。
⇒ **推论（inferred）**：只要回滚落到 `degraded`（`UPDATE` 可用、`DELETE` 被拒），**判据④ 就应达成**而判据③ 不达成 ⇒ **额度恢复后若回滚落在 `degraded`，判词要分开写，不能笼统说「③④ 都阻塞」**。

### (3) 额度判定为什么「非单调」（观察 + 工具坑，机制 not verified）

| 时刻（北京 / UTC） | 操作 | 结果 |
|---|---|---|
| 07:42 / 23:42 | 单行 `DELETE`（1 行） | **成功** |
| 07:43 / 23:43 | 批量 `DELETE`（`email_verifications` ~12 行） | **被拒**（7500） |
| 07:45 / 23:45 | 批量 `DELETE`（11 行，`rows_written: 11`） | **成功** |
| 07:46:49 / 23:46:49 | `UPDATE email_verifications SET created_at=created_at WHERE email LIKE 'bq.%'`（1 行） | **成功**（`changes:1 / rows_written:2`） |
| 07:46:49+ / 23:46 | Worker 内回滚（DELETE + 降级 UPDATE） | **两条都被拒** ⇒ `failed` |
| 07:47 / 23:47 | 批量 `DELETE`（2 行 / 14 行） | **被拒**（7500） |
| 07:48–07:50 / 23:48–23:50 | 逐行 `DELETE` ×16 + 单行 `DELETE` ×1 | **全部未生效**（计数不变） |
| **07:52:04** / 23:52:04 | 单行 `DELETE`（`bq.v3.1795391211638@example.com`） | **成功**（`changes:1 / rows_written:1`）⇒ `users` LIKE 'bq.%' 14 → **13** |
| 07:52:1x–07:53 / 23:52 | 逐行 `DELETE` ×30（2 轮 × 15，含 2 行验证码 + 13 行 users） | **全部被拒**（QUOTA_7500） |

⇒ **同一账号 / 同一 D1 / 同一分钟内写操作成、败交替** ⇒ 与「单一按日累计计数器」模型不符（inferred：Cloudflare 侧计数器同步或语句预估存在竞态 / 不同语句类型计费口径不同；**外部不可判定，别再猜机制**）。
⇒ **工程结论（verified）**：① 额度判据必须是「**连续 ≥2 条不同语句都成功**」才敢判有余；② 额度抖动期**不要跑会写库的探针**（每次失败注册都新增 2 行残留）；③ 清理优先**逐行删**（单行 DELETE 的成功记录都在小批量侧）；④ **禁止用 `Remove-Item` / 批量 DELETE 的 exit code 当额度判据**。

### (4) BR 批新候选：失败注册「零写入」化（推荐，尚未实现）

**问题**：当前回滚**依赖 D1 写额度**（DELETE 优先、UPDATE 兜底）⇒ 额度一耗尽，失败注册**必然留残留**（判据③），并连带把立刻重试打成 429（判据④）。**这不是额度问题，是设计对额度有依赖。**

**候选修法**：把「落库」挪到「发信成功之后」——
1. 内存里生成验证码并算 `code_hash`（**不发信、不落库**）；
2. 发信（上游 mail API）；
3. **发信成功才落库**：`INSERT email_verifications`（`code_hash` + `created_at`）；
4. **发信失败 ⇒ 一行都不写** ⇒ 无残留、无需回滚 ⇒ 判据③④ 结构性达成、**与 D1 额度无关**。

**连带范围**：`auth.ts:552-560` 注释的**重发路径**（ADR 0075 决策 3「重发失败只删本次验证码行」）同样**依赖 DELETE 额度** ⇒ 零写入化应**同时覆盖重发路径**（否则额度耗尽时重发也会留残行）。

**代价 / 风险**（需 BR 批 ADR 拍板）：① 「注册」语义从「建号」变成「建号 + 发码成功」⇒ 失败时库里查无此邮箱（**这正是 422 语义想要的**）；② 建号与建码之间的并发竞态窗口扩大 ⇒ 需补 `INSERT` 冲突处理；③ 需重跑 `test/register-mail-failure.test.ts` 全组 + 新增「发信失败零写入」断言。

**与既有 ADR 的关系**：ADR 0075 决策 2 已确立「失败注册回滚」；本候选是它的**替代实现**（把回滚从「事后删」改成「事中不写」）⇒ **需新 ADR（ADR 0076）+ 用户拍板**，本轮**不改码**。

### (5) 残留现状（07:50 实测，**清理未生效**）

| 表 | 计数 | 说明 |
|---|---|---|
| `users` LIKE 'bq.%' | **13** | 本批探针残留；07:52:04 删掉 1 行后额度即尽，其余 30 次逐行 DELETE 全被拒（**§13 的 00:38 数字已过期，以本表为准**） |
| `email_verifications` LIKE 'bq.%' | **2** | 同上 |
| `users` LIKE 'ak.%' | 2 | 更早批次遗留，**待用户定**（§12-5） |
| `email_verifications` LIKE 'ak.%' | 3 | 同上 |
| `users` 合计 | 18 | 含真实账号 |
| `users` WHERE email='25940969@qq.com' | **1** | **真实账号完好，未被误伤** |
| `sync_items` | **40,061** | 真实账号的全量词典，本轮未动 |

⇒ **下一轮第一件事**：UTC 午夜（≈北京 08:00）后立刻 ① 跑 `tmp\bq-verify.ps1` ② 看 `X-Fluxing-Mail-Rollback` 落在 `ok` / `degraded` / `failed` ③ 跑 `tmp\bq-d1-cleanup.ps1`（或逐行删）清 `bq.%` ④ 按 §8.2(2) 的分层结论**分别**给判据③④ 判词。
⇒ **今日（UTC 11-22）剩余可写额度 ≈ 1 行**（07:52:04 成功删掉最后一行后，紧接着 30 条逐行 DELETE 全被拒）⇒ **翻页前不要再试清理、不要再跑注册探针**（INSERT 仍可用 ⇒ 每跑一次失败注册就多 2 行残留）。

### (6) 用户拍板状态：**仍未拍板**（verified，与 §8.1(1) 一致）

`ask_aa0bd45e5d4494a4c8e37642`（D1 是否升级付费 / BQ 是否现在提交）两问**至今未答**。本轮继续按 §8.1(6) 的既定处置走：**不升级付费**（等待免费且可逆）⇒ 若 (4) 的 BR 候选被采纳，**付费议题可整体推迟**，因为失败注册不再需要任何写额度。

---

## 8.3 【2026-11-23 08:15 定时轮复验】配额已恢复 · **BQ 判据 ③④ 全部达成**

### (1) 结论速览（verified）

- **UTC 日翻页（08:00 北京）后 D1 写额度已恢复** ⇒ **回滚由 `failed` 变为 `ok`**。
- **判据③ 零残留：✅ 达成** —— 探针邮箱在 `users` / `email_verifications` / `mail_outbox` 三张表**计数全 0**（`bq-verify.ps1` 自带的三条 D1 计数查询）。
- **判据④ 立即重试：✅ 达成** —— 立刻第二次注册**回到 422 `mail_recipient_rejected`**（`R2_NOT_429=True`、`R2_NOT_409=True`），不再撞 60 秒重发闸。
- **判据① 保留域 422 + 诊断头：✅ 仍达成** —— `X-Fluxing-Mail-Provider-Status: 422`、`X-Fluxing-Mail-Rollback: ok`、中文文案逐字正确。
- **回滚错误头消失**：本轮响应**不再带** `X-Fluxing-Mail-Rollback-Error`（无错误 ⇒ 无该头）⇒ 与「回滚走完且成功」一致。
- **残留已清零**：本轮逐行 DELETE **15/15 全部成功**（2 行 `email_verifications` + 13 行 `users`）⇒ `users` LIKE 'bq.%' = **0**、`email_verifications` LIKE 'bq.%' = **0**。
- **判据⑤ 真实域端到端：⏸ 仍未判定** —— 阻塞点未变（`mail.tm` token 端点对本探针地址 401；`mail.gw` 直连被拒）⇒ 需真实邮箱 + 用户在场。
- ⇒ **§8.2(2) 的预测被验证**：回滚落在 `ok` ⇒ 判据③④ 同时达成（当时若落在 `degraded`，则只会达成 ④）⇒ **BQ 批的代码路径本身没有问题，昨天只是被额度挡住**。

### (2) 复验证据（未改动 `tmp\bq-verify.ps1`，脚本一行未改）

| 项 | 值 |
|---|---|
| 运行时刻 | 2026-11-23 **08:15:40** 北京 / **00:15:40 UTC**（UTC 日已翻页） |
| R1_STATUS / CODE | **422 / `mail_recipient_rejected`** |
| R1_PROVIDER_STATUS | **422** |
| R1_ROLLBACK | **`ok`**（08:15 前为 `failed`） |
| R1_MESSAGE | 验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。（逐字一致） |
| R2_STATUS / CODE | **422 / `mail_recipient_rejected`**（08:15 前为 429 `verify_too_frequent`） |
| R2_ROLLBACK | **`ok`** |
| R2_NOT_429 / R2_NOT_409 | **True / True** |
| D1 users(probe) | **n=0** |
| D1 email_verifications(probe) | **n=0** |
| D1 mail_outbox(probe) | **n=0** |
| 额外探针（`bq.v5.*@example.com`） | 422，`X-Fluxing-Mail-Rollback: ok`，**无** `X-Fluxing-Mail-Rollback-Error` 头 |

### (3) 清理结果（逐行 DELETE，15/15 成功）

| 表 | 清理前 | 清理后 |
|---|---|---|
| `users` LIKE 'bq.%' | 13 | **0** |
| `email_verifications` LIKE 'bq.%' | 2 | **0** |
| `mail_outbox` LIKE 'bq.%' | 0 | 0 |

**未动的行**：`users` LIKE 'ak.%' = **2**、`email_verifications` LIKE 'ak.%' = **3**（更早批次遗留，**待用户定**，见 §12-5）；`users` WHERE email='25940969@qq.com' = **1**（**真实账号完好**）；`sync_items` = **40,061**（真实账号的全量词典，未动）；`devices` = 25；`refresh_tokens` = 98。

### (4) P0 状态变化（**额度已恢复 ≠ 根因已消除**）

- **已恢复**：UTC 翻页后额度自动重置 ⇒ 生产链路恢复（**未升级付费、未 redeploy、未改码**）。
- **根因仍在**：今天打爆额度的是**客户端自身的一次性全量词典推送**（40,061 行，见 §8.1(5)）⇒ 只要客户端再推一次全量（重建词典 / 第二台设备 / 换机），**明天同一时刻会再次打爆**。
- ⇒ **BR 批两个子项都要做**：① **推送写放大治理**（客户端只推增量 + 服务端仅在内容变化时写行）= **防复发**；② **失败注册「零写入」化**（§8.2(4)）= **让失败路径不依赖写额度**（昨天已实证：额度一耗尽，失败注册必然留残留，并把立刻重试打成 429）。
- ⇒ **本轮建议（不变）**：**不升级 D1 付费**；付费议题留到 BR 批证明免费额度确实承载不了词典规模时再谈。

### (5) 用户拍板状态：**仍未拍板**（verified）

`ask_aa0bd45e5d4494a4c8e37642`（D1 是否升级付费 / BQ 是否现在提交）两问**至今未答**。本轮按既定口径走：**不升级付费**、**不自行 commit**。但**判据③④ 已达成 ⇒ BQ 批已达到「可提交」状态**（门禁 `TSC_EXIT=0` + `CHECK_EXIT=0` 5 文件 / 86 用例 + 生产实测），**是否 commit 由用户决定**（§12-2）。

---

## 8.4 【2026-11-23 08:41 定时轮】额度有余 · 零复发 · 残留构成更正

- **① 写额度有余（verified，满足「连续 ≥2 条不同语句成功」纪律）**：08:41 用**零副作用** UPDATE 探针实测 —— `UPDATE users SET email_verified = email_verified WHERE id = (SELECT id FROM users WHERE email LIKE 'ak.%' ORDER BY id LIMIT 1)` ⇒ `success=true` / `rows_written=1`；`UPDATE email_verifications SET attempts = attempts WHERE email LIKE 'ak.%' ORDER BY id LIMIT 1` ⇒ `success=true` / `rows_written=1` ⇒ **两条独立写成功** ⇒ 判定额度有余（既非 0 行假阳性，也非单条侥幸）⇒ **不需等待、不需付费**。
- **② 探针残留零复发（verified）**：`users LIKE 'bq.%'` = 0、`email_verifications LIKE 'bq.%'` = 0、`mail_outbox` 合计 4（仍为更早批次历史行）；`sync_items` = **40,061 未变** ⇒ 客户端本轮**未**再次全量推送 ⇒ 08:15 清理成果保持有效。
- **③ `users` 构成更正（verified）**：逐行核对后确认 4 行 = **2 真实账号 + 2 `ak.*` 探针**，**不存在**「前缀未核对」的探针行（上一轮 §13 表述有误，已更正）；明细见 §13。
- **④ 新发现：写放大的第二个来源（verified）**：真实账号 B 持有 **22 devices / 90 refresh_tokens**，客户端每次登录 / 续期都写 `refresh_tokens` ⇒ 即便不推全量词典，**设备与令牌无上限增长**也会持续吃每日行写入额度 ⇒ BR 子项① 必须同时治 (a) `sync_items` 全量 与 (b) 设备 / 令牌无上限两条线。
- **⑤ 新工具坑（verified）**：`wrangler d1 execute --command` 传多行 SQL ⇒ 报 `Couldn't find a D1 DB with name or binding 'DB'`（**误导性**）⇒ 压成单行即成功 ⇒ 详见 §11-11。
- **⑥ 拍板状态（未变）**：BQ 批是否 commit **仍未获用户答复**（上一轮问卷卡与本轮均无回答；上一轮 `wrap_up` 未生成卡片）⇒ **未 commit、未 deploy、未改码**；BR 批按既定决策**下轮立项**（ADR 0076）。

---

## 9. 未闭环清单（以本节为准）

1. ✅ **P0 · D1 行写入额度耗尽 —— 已于 2026-11-23 08:15 随 UTC 翻页自动恢复**（§8.3(1)(4)）⇒ 生产链路恢复，**未升级付费 / 未 redeploy / 未改码**；**但根因未消除**（客户端一次性全量词典推送 40,061 行）⇒ 复发风险仍在 ⇒ 转为 **BR 批子项 ①「推送写放大治理」**；额度判据的坑（≥1 行写入成功 ≠ 有余）留在 §8.2(3) / §11-5。
2. ✅ **BQ 门禁判据 ③零残留 / ④立即重试 —— 2026-11-23 08:15 达成**（§8.3(1)(2)）：回滚 `ok`、探针三表残留计数全 0、立即重试回到 **422**（`R2_NOT_429=True` / `R2_NOT_409=True`）⇒ **BQ 批服务端门禁判据全部达成**；**只剩 ⑤ 真实域端到端**（⏸ 需真实邮箱在场，`mail.tm` token 401 未变）⇒ **用户已于 2026-11-23 09:15 拍板提交，落地为两个 commit**（见 §14）；**只剩 ⑤ 真实域端到端**（⏸ 需真实邮箱在场，`mail.tm` token 401 未变）。
3. ❗ **BR 批（已决定下轮立项，ADR 0076）**：① **推送写放大治理拆成两条线** —— (a) `sync_items` 全量推送 40,061 行；(b) **设备 / `refresh_tokens` 无上限增长**（真实账号 B 已累积 22 devices / 90 tokens，每次登录续期都写行，§8.4(4)）⇒ **只治 (a) 不够**；② **失败注册「零写入」化** —— 把 `users` / `email_verifications` 落库挪到**发信成功之后** ⇒ 失败注册一行都不写 ⇒ **失败路径与 D1 额度彻底解耦**（重发路径同样要覆盖，见 §8.2(4)）；两子项**同批做**；**本轮未改码**，下轮开工。
4. ❗ **`mail.tm` token 端点 401** —— 端到端取码的第二重阻塞；备选 = 真实邮箱人工端到端（需用户在场）。
5. ❗ **口令重置发信路径同构问题未修**（`password_resets` 表「先落库后发信」，`migrations/0002_password_resets.sql:13-27`）—— 属另一条链路，ADR 0075 决策 7 明确不碰。
6. ❗ **`wrangler tail` 彻底不可用**（代理 `fetch failed` ×3、直连 `connect ETIMEDOUT 202.182.98.125:443`）⇒ 无 Worker 端原始日志；本批改用响应头自证绕开。
7. ❗ **真实用户收件箱送达率未验证**（本轮用可读收件箱做端到端代理验证，不等于 QQ / Gmail 的投递率与进信箱率）。
8. ❗ **上游错误归属的误判风险**：上游 4xx 一律归给收件人侧（5b 分支）；若上游实际是「我方 payload 字段非法」且错误名不在配置类名单，用户会看到「请换一个邮箱」⇒ 缓解靠日志保留上游 `name` + body 截断（ADR 0075 后果段已登记）。
9. ❗ 承接 BP / BN / BM / BL 的未闭环项（R2 孤儿对象、`sync_items` ↔ R2 一致性核对、双机 AC 7 条 / restore 端到端 / 输入延迟两组对照 / G-3·G-5 真跑、18 份 `run-*.log` 是否入库、`tmp\bh-c2d-fmt-check.cpp` 处置、2 个 `ak.probe.*@example.com` 账号）—— 见 §12。

---

## 10. 证据路径索引

| 用途 | 路径 |
|---|---|
| 本批决策 + 实测回填（ADR 0075） | `F:\soft\00selfmade\rime_claude\docs\adr\0075-register-mail-rejection-semantics-and-failed-registration-rollback.md` |
| 本批真源（本文档） | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md` |
| 接力 prompt（本批） | `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-22-batch-bq-register-mail-rejection-semantics.md` |
| 错误映射（改前）落点 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\mail.ts:196-223` |
| 失败注册落库（改前）落点 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts`（`:262` 60 秒闸、`:264-267` 409 占号、`:275-279` 写 users、`:281` 造码+发信） |
| 客户端语义（本批**未改**） | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccount.cpp:92-106`、`CloudTransport.cpp:105-113`、`CloudAccountUi.h:41` |
| 保留域探针复验脚本 | `F:\soft\00selfmade\rime_claude\tmp\bq-verify.ps1` |
| 端到端脚本（最终版 v4） | `F:\soft\00selfmade\rime_claude\tmp\bq-e2e-mailtm-v4.ps1` |
| 部署 + 探针 + D1 计数脚本 | `F:\soft\00selfmade\rime_claude\tmp\bq-v4-deploy-verify.ps1` |
| D1 探针行清理脚本（配额恢复后跑） | `F:\soft\00selfmade\rime_claude\tmp\bq-d1-cleanup.ps1` |
| D1 schema | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\migrations\0001_init.sql`（`:19-30` users、`:75-86` email_verifications、`:88-94` mail_outbox） |
| 上游官方错误表 | `https://resend.com/docs/api-reference/errors`（2026-11-22 取证） |

---

## 11. 环境事实与工具坑（不可从仓库推导）

1. **PS7 的 `Invoke-WebRequest.Content` 是 `byte[]`，不是字符串** ⇒ 直接强转会得到 `123 34 64 ...` 字节码（JSON 解析必挂、POST body 也会错）⇒ 必须 `[Text.Encoding]::UTF8.GetString($raw)` 解码。
2. **PS7 的 `Invoke-RestMethod -SkipHttpErrorCheck` 只回 body，没有 `StatusCode` / `Content` 属性** ⇒ 要状态码 / 响应头必须用 `Invoke-WebRequest -SkipHttpErrorCheck`。
3. **`wrangler tail` 本机彻底不可用**：走代理 `fetch failed`（×3，含 `--use-env-proxy` 变体）、直连 `connect ETIMEDOUT 202.182.98.125:443`；`Start-Process node` 起的子进程会自行退出 ⇒ **不要在这条路上反复试探**，改用响应头自证。
4. **Cloudflare API 与 mail.tm 必须直连**（本机代理 `127.0.0.1:7897` 当前 SSL 握手失败，curl exit 35）⇒ 部署/远端 D1 命令前设 `$env:HTTPS_PROXY=$null; $env:HTTP_PROXY=$null; $env:ALL_PROXY=$null; $env:NO_PROXY='*'`；`mail.tm` 用 `curl.exe --noproxy '*'`。
5. **D1 免费额度按「行写入」计**：额度耗尽后 `DELETE` / `UPDATE` **一律被拒**（`code 7500`），`INSERT` 在耗尽前仍可用，**读始终可用** ⇒ 「能建、不能改、不能删」。
6. **`mail.tm` token 端点对本探针地址持续 401**（账号创建 201 成功、换 token 401；加 6 秒延迟 + 4 次重试仍 401）；`mail.gw` 直连被拒（connection refused）；`mail.tm` `/domains` **间歇性返回空响应体**（同 URL 前两次空、第三次正常）⇒ 取码脚本必须带重试。
7. **探针凭据纪律**：`auth_key` / 账号明文只进 `tmp/` 文件，**禁止入库、禁止写进日志/报告/对话**；回答与报告里只给 `长度 + sha16 前 16 位 + 尾部 4 字符` 作校验位。
8. **`Remove-Item` 被本地硬安全策略拦截**；删文件须用可恢复删除通道（`rm -- "<target>"`）；清代理变量用 `$env:X = $null`。
9. **`npm --prefix … exec -- wrangler …` 会在错误 cwd 运行** ⇒ 必须显式 `--config F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\wrangler.toml`。
10. **`Resolve-DnsName` 混合类型响应**会让自定义格式化脚本抛 Null 方法错误 ⇒ 取 MX 时按 `Type -eq 'MX'` 过滤。
12. **旧坑沿用（不重复）**：`cmd //c` 假绿必须 `cmd /c`；`build.bat` 日志 GBK/UTF-16 混杂须按 BOM 判编码；整文件 clang-format 有既有漂移必须 `--lines=起:止`；`CHANGELOG.md` 是 CRLF-only 而 handoff / ADR / prompt 是 **LF-only**；NSIS `InstallDirRegKey` 指向 32 位视图（**无需清注册表**，清了丢 `RimeUserDir`）；`/D=` 必须是最后一个参数且不加引号；`Start-Process -Wait` 对 NSIS 挂死 ⇒ 用 `Wait-Process -Timeout`；装机后服务 pid 必变属预期；派「只读取证 + 落盘报告」子会话**必须派 `worker`**（`explore`/`verifier` 秒返 `ROLE_MISMATCH`）；派子会话 `effort` **一律不传**。

13. **`wrangler d1 execute --command` 的 SQL 必须单行**（2026-11-23 08:41 实测）：传多行 SQL（PowerShell here-string）⇒ 报 `Couldn't find a D1 DB with name or binding 'DB'`（**误导性报错** —— DB 并未消失，换行被拆成额外位置参数导致 binding 解析失败）⇒ 判「D1 不可用」前先 `$sql -replace "\s+"," "` 压成单行重试（同一段 SQL 压成单行即成功）。

---

## 12. 需要用户本人在场 / 待用户拍板

**待用户拍板**
1. ✅ **P0 · D1 每日行写入额度耗尽 —— 已于 2026-11-23 08:15 随 UTC 翻页自动恢复**（§8.3(1)(4)）：**不升级付费 / 不 redeploy / 不改码即自愈** ⇒ 本项无需再拍板；但**根因未消除**（客户端一次性全量词典推送 40,061 行）⇒ 复发风险转入 **BR 批子项 ①「推送写放大治理」**（§9-1 / §9-3）。
2. ✅ **BQ + BP 批改动已 commit**（2026-11-23 09:15，用户拍板「现在提交，分 docs 与 fix 两个 commit」）—— 服务端 fix 一个 commit（4 源码 + 1 新测试）+ 文档一个 commit（ADR 0075 + README + BO/BP/BQ 交接 + 本交接 + 本 prompt + CHANGELOG 条目）⇒ **见 §14**；**是否 push 到 `kizemo/feat/100-cloud-sync` 仍待用户指令**（本轮未 push）。
3. 18 份 `run-*.log` 是否 `git add -f` 入库（承接 BP/BN/BM/BL）。
4. `tmp\bh-c2d-fmt-check.cpp`（83 767 B，本机策略删不掉）如何处置。
5. R2 残留 **164 个孤儿对象**是否走「按 `sha256(item_key)` 重建 key 逐个 `wrangler r2 object delete`」；另 2 个 `ak.probe.*@example.com` 账号是否删除。

**需两台机器 + 用户在场（承接 BN/BO/BP）**
6. 双机 AC 7 条；restore 端到端；输入延迟两组对照；G-3 / G-5 真跑。
7. **真实域注册端到端**（register → 收码 → verify → login）—— 若 `mail.tm` 仍 401，需真实邮箱人工收码。

**已知产品侧后果（刻意不做）**
8. 1017 是单行 STATIC，多提示叠加可能显示不全（ADR 0072 后果段已登记，刻意不做自动换行）。

---

## 13. 探针与云端数据现状（bq.% 已清零；跨批残留待用户定）

- **本批 D1 探针行：已于 2026-11-23 08:15 全部清零**（verified）：逐行 DELETE **15/15 全部成功**（2 行 `email_verifications` + 13 行 `users`）⇒ 最终快照 `users` LIKE `bq.%` = **0**、`email_verifications` LIKE `bq.%` = **0**、`mail_outbox` 的 bq.% = **0** ⇒ **BQ 批探针零残留**。
- **仍在库的跨批残留（本批未动，待用户拍板）**：更早批次的 `ak.probe.*@example.com` 探针账号 **2 个**（`users` 2 行 + `email_verifications` 3 行，其中 2 行已 `consumed_at`）⇒ **技术上可删，等用户拍板**；BO 批遗留 `bo12002.probe.20261122154809@example.com` + 82 行 `sync_items` + 82 个 R2 对象（清理入口见 BO 交接 §13：`tmp\bo-push-probe.ps1 -Mode Cleanup`）。
- **`users` 合计 4 行的真实构成（2026-11-23 08:41 逐行核对；上一轮 §13 表述「1 条前缀未核对的早期探针行」为误判，已更正）** = **2 个真实账号 + 2 个 `ak.*` 探针**，**不存在**「前缀未核对」的探针行：
  - 真实账号 A：`kiz***@163.com`（len 14，`email_verified=1`，devices 1 / tokens 2 / **sync_items 0** / 验证码 1 行）= 用户本人账号；
  - 真实账号 B：`259***@qq.com`（len 15，`email_verified=1`，**devices 22 / tokens 90 / sync_items 40,061** / 验证码 4 行）= 压测账号；
  - 探针 ×2：`ak.probe.*@example.com`（len 27 / 28，devices 1，tokens 2 / 4，sync_items 0，验证码 2 / 1 行）。
- **真实账号未受影响**：`users` 中 **2 个真实账号完好**（真实邮箱见 tmp 记录，不写入本文档）；`sync_items` = 40,061 行、`devices` = 25、`refresh_tokens` = 98（本批全程未动）。
- **`mail_outbox` 的 4 行历史行**（`created_at ≈ 2026-11-11`，`id` 5–8，`body_len` 85）是**更早批次产生**、**非本轮**，勿误删。
- **本轮已清理的敏感文件**（`tmp/` 已被 gitignore，从不入库）：`tmp\bq-register-probe.txt`（含 `auth_key` 明文）、`tmp\AN-register-qq.txt`、`tmp\bo-push-probe-state.json`（含明文 `auth_key`）、`tmp\bq-mail-tail.out` / `.err`、`tmp\bq-mail-tail2.out` / `.err`。
- **清理手法口径（额度抖动期）**：**逐行 DELETE**，**不要**用批量 DELETE（批量曾在额度抖动期被拒）；额度耗尽时 INSERT 仍可用 ⇒ **每跑一次失败注册探针就新增 2 行残留**，UTC 日翻页前不要跑探针；`tmp\bq-d1-cleanup.ps1` 已内置直连 + 显式 `--config`（本轮实际清理走逐行 DELETE）。
---

## 14. Git 与工作区现状（接手时先看这里）

- 分支 `feat/100-cloud-sync`（跟踪 `kizemo/feat/100-cloud-sync`）；BP 批提交（`cd4e370` / `3846b4c` / `d99ce30` / `3e32cb8`）已推。
- **本批已提交**（2026-11-23 09:15，用户拍板「现在提交，分 docs 与 fix 两个 commit」）—— 两个 commit（具体 SHA 以 `git log -2` 为准，HEAD = 较新者）：
  - **fix commit**（scope `fluxing`）：`cloud/fluxing-sync/src/{auth,errors,mail}.ts` + `test/mail-http.test.ts` + **新增** `test/register-mail-failure.test.ts`（**5 文件 / 927 insertions / 21 deletions**）；
  - **docs commit**（scope `fluxing`）：ADR 0075 + `docs/adr/README.md` + BO/BP 交接与 prompt 修订 + 本交接 + 本 prompt + `CHANGELOG.md` 条目（**10 文件 / 843+ insertions**）。
- **未 push**（用户只授权 commit；推 `kizemo/feat/100-cloud-sync` 待明确指令）。
- 提交前的历史工作树改动（现全部入库）：
  - 源码：`cloud/fluxing-sync/src/auth.ts`、`src/errors.ts`、`src/mail.ts`；`cloud/fluxing-sync/test/mail-http.test.ts`（改）、`test/register-mail-failure.test.ts`（**新**）。
  - 文档：`docs/adr/0075-…md`（**新**）、`docs/adr/README.md`、`docs/archive/handoffs/handoff-2026-11-22-batch-bo-…md`（§7 / §9-6 / §11-6 修订）、`docs/archive/handoffs/handoff-2026-11-22-batch-bp-…md`（上一轮文档漂移修订）、`docs/archive/prompts/prompt-next-session-2026-11-22-batch-bp-…md`、`CLAUDE.md`、`.specify/memory/project-knowledge.md`。
  - 本批新增：`docs/archive/handoffs/handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md`、**本文件**、配套 relay prompt 文件。
- **未跟踪且不得入库**：`tmp/`（已 gitignore）、`reports/fluxing-bo-12002/`、`release/`、`docs/design/quickpanel-v6/design-demos/*.html`、`scripts/verify/*.ps1`、`reports/fluxing-bk-syncstatus/sync-status-*.txt`、`reports/fluxing-bh-ui/probe-bh.ps1.bak-pass8`；`librime` 子模块 dirty（`m`，**不得编辑、不得 `git add librime`**）。
- **commit 前必做**：`git status -sb` + `git diff --stat`（阻止 `weasel.build.props` / `env.bat` / `*.obj` 误提交）；Conventional Commits + 模块前缀。
- 装机现场（未变，客户端本批零改动）：`D:\Program Files\fluxing\weasel\WeaselServer.exe`，服务 pid **285340**（16:16:09 起），`RimeUserDir` 未动、未登出/未登录/未改注册表。
