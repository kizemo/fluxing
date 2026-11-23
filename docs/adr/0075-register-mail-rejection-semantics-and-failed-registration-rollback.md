# ADR 0075 — 注册发信失败按「永久拒绝 / 临时故障 / 配置错误」分流，失败注册一律回滚

- **Status**: **Accepted**（2026-11-22；用户指令「开做 BQ 注册 503 修复」，本篇为批次 BQ 的决策依据）
- **Date**: 2026-11-22
- **Deciders**: kizemo（仓库维护者）+ mavis
- **Spec**: `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` —— **不改规格**（本篇只修服务端错误语义与失败回滚，不新增端点、不改协议）
- **Related**: ADR 0058（第三方 HTTPS 发信通道，定义 `MAIL_MODE=http` 与 Resend 形状的 payload）；ADR 0064（服务端口令重置端点）；批次 BO / BP 交接文档
- **Revises**: —（**不推翻任何既有 ADR**；本篇修订的是代码里的错误映射口径，见「事实 4」）
- **Supersedes / superseded by**: —

---

## Context

### 事实 1 — 生产 `/v1/auth/register` 对保留域收件人恒 503 `mail_send_failed`

2026-11-22 22:13（北京时间）实测生产 `https://api.aiec.fun`：

| 请求 | 结果 |
|---|---|
| `POST /v1/auth/register`（探针邮箱 A，`@example.com`） | **HTTP 503** `{"error":{"code":"mail_send_failed","message":"验证码邮件发送失败，请稍后重试。"}}`，耗时 **2567 ms** |
| `POST /v1/auth/resend`（同一探针邮箱 A） | **HTTP 503** 同码 |

（探针凭据只存在于 `F:\soft\00selfmade\rime_claude\tmp\bq-register-probe.txt`，**不入库、不入本篇**。）

### 事实 2 — 同一上游、同一时刻、只换收件人域 ⇒ 真实域收件人 200 成功

同一时刻、同一密钥、同一 `from`（`火流猩输入法 <no-reply@aiec.fun>`）、同一请求形状，只把收件人域从 `example.com` 换成 `qq.com`：

| 收件人域 | 结果 |
|---|---|
| `@example.com` | **HTTP 503** `mail_send_failed`（2567 ms） |
| `@qq.com` | **HTTP 200**（1585 ms），响应体含 `account_id` / `salt` / `kdf{PBKDF2-SHA256,600000}` / `need_email_verify=true` |

⇒ **发信密钥、发件域 `aiec.fun`、`from` 形状、鉴权头形状均可用**；拒收与**收件人域**相关，与账号、密钥、端点无关。

### 事实 3 — `example.com` 是 null MX 保留域，QQ 是真实 MX 域

`Resolve-DnsName example.com -Type MX` ⇒ **`./0`**（preference 0 → 目标 `.`，即 RFC 2606 / RFC 6761 保留域，**声明不接受任何邮件**）；`qq.com` ⇒ 真实 MX 记录。

⇒ 与事实 2 的对照一致：被拒的是**根本收不到信的收件域**。（**推断**：上游对该类收件域做了拒绝；上游原文未直读，见「事实 5」。）

### 事实 4 — 现有错误映射把上游 4xx 一律伪装成可重试的 503

`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\mail.ts:196-223`：拿到上游响应后按 status 分流 ——

| 上游 | 我方 | 行号 |
|---|---|---|
| 401 / 403 | 500 `server_misconfigured` | `:205-207` |
| 429 | 503 `mail_rate_limited` | `:208-210` |
| **其它 4xx / 5xx** | **503 `mail_send_failed`「验证码邮件发送失败，请稍后重试。」** | `:211` |
| 网络异常 / 超时（重试 1 次，共 2 次调用） | 503 `mail_send_failed` | `:212-223` |

⇒ **永久性拒绝被表述为「临时故障，请稍后重试」**，语义错误：用户按提示重试永远不会成功；运维从错误码也看不出真实原因。

### 事实 5 — 上游原文未直读（诚实登记；改用「自证法」取证）

- `wrangler tail` 三次失败（`fetch failed`；含 `node --use-env-proxy` 变体）⇒ 拿不到 `mail.ts:202-204` 打印的上游 status / body 原文；
- Workers Observability 未在 `cloud/fluxing-sync/wrangler.toml` 启用；
- `MAIL_HTTP_API_KEY` 只能以 Cloudflare Secret 形式存在，**不可读回**（`wrangler.toml:56` 注释已写明不入库）。

⇒ 只能由「503 ≠ 401/403/429」反推上游落在「其它 4xx/5xx」或「网络异常」两类之一。**本篇不臆断具体上游状态码**；改由本篇「决策 5」的诊断头 `X-Fluxing-Mail-Provider-Status` 在部署后**由我们自己的响应**把上游 status 直接暴露出来（自证法）。

### 事实 6 — 发信失败仍留下账号行与验证码行（无事务、无回滚）

`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts`：

| 步 | 动作 | 行号 |
|---|---|---|
| 1 | 60 秒重发闸 `assertResendAllowed` | `:262`（实现 `:224-237`） |
| 2 | 查重，命中即 409 `email_taken` | `:264-267` |
| 3 | **写 `users` 行**（`email_verified=0`） | `:275-279` |
| 4 | `issueVerificationCode`：60 秒闸 → 生成 6 位码 → **写 `email_verifications` 行** → **发信** | `:281`（实现 `:239-251`，写行 `:245-249`，发信 `:250`） |

⇒ 发信失败时 **两个行都已提交**，后果有三：

1. **占号**：同一邮箱无法再注册 —— 重试注册先撞 60 秒闸（429 `verify_too_frequent`，`:262`），过闸后撞 409 `email_taken`（`:267`）；
2. **失败仍消耗 60 秒窗口**：闸门依据 `email_verifications` 的最新行（`:224-237`），失败的发信也留下了一行；
3. **生产 D1 留僵尸账号**：本轮两个探针账号即由此产生。

### 事实 7 — 客户端只透传 message；5xx 自动重试会掩盖根因

- `WeaselServer/Cloud/CloudAccount.cpp:92-106`（`ExtractServerError`）**只读 `error.message`**，不读 `error.code`；`:154-158`（`SendJson`）只判 2xx / 非 2xx；
- `WeaselServer/Cloud/CloudTransport.cpp:105-113`（`ShouldRetry`）对 **429** 与 **5xx** 自动重试，`CloudTransport.h:75` 默认 `max_retries = 2` ⇒ **最多 3 次尝试**；
- 注册失败文案落在控件 `kIdStatus = 1006`（`WeaselServer/Cloud/CloudAccountUi.h:41`；写入 `CloudAccountDialog.cpp:1500-1502`，注册作用域文案 `CloudAccountDialog.cpp:626-628`）。

⇒ 服务端把永久拒绝表述成 503 时：客户端自动重试 → 第 2 次请求撞上 60 秒闸 → 用户看到的是「验证码发送过于频繁」，**真实原因被二次症状覆盖**。

### 事实 8 — BO 批「注册链路不可用」口径仅由保留域探针支撑，需修订

`docs/archive/handoffs/handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md` §4 / §7 登记的「`/v1/auth/register` 恒 503 ⇒ 新用户「注册 → 收码 → 验证」链路不可用」结论，**其唯一证据是 `@example.com` 探针**。事实 2 证明真实域收件人返回 200 ⇒ 该口径**过宽**，本篇一并修订。

### 事实 9 — 上游官方错误分类：4xx 全部是永久性拒绝

Resend 官方错误表（`https://resend.com/docs/api-reference/errors`，2026-11-22 取证）：400 `validation_error` / 401 `missing_api_key`·`restricted_api_key` / 403 `validation_error`（发件域未验证、沙箱限制、收件箱额度） / 404 `not_found` / 405 `method_not_allowed` / 409（幂等冲突·资源锁）/ 422 `missing_required_field`·`invalid_parameter` 等 / 429 `rate_limit_exceeded`·`daily_quota_exceeded` / 500 `application_error` / 503 `dns_lookup_failed`·`service_unavailable`。

⇒ **除 429 / 409 / 5xx 外，4xx 语义上都是「重试无用」的永久性拒绝** —— 这正是事实 4 里被抹平的语义。

---

## Decision

### 决策 1 — 上游失败按三类分流（永久拒绝 / 临时故障 / 配置错误）

在 `src/mail.ts` 内新增分类函数，判定**严格按下列顺序**：

| 序 | 条件 | 我方 status | error.code | message |
|---|---|---|---|---|
| 1 | 上游 401 / 403 | 500 | `server_misconfigured` | 发信服务拒绝了请求（鉴权失败），请联系管理员。**（逐字沿用现状）** |
| 2 | 上游 429 | 503 | `mail_rate_limited` | 验证码邮件发送过于频繁，请稍后重试。**（逐字沿用现状）** |
| 3 | 上游 409（幂等冲突 / 资源锁，上游语义即「稍后重试」） | 503 | `mail_send_failed` | 验证码邮件发送失败，请稍后重试。**（逐字沿用现状）** |
| 4 | 上游 ≥ 500，或 < 400（3xx 等异常分支） | 503 | `mail_send_failed` | 验证码邮件发送失败，请稍后重试。**（逐字沿用现状）** |
| 5a | 其余 4xx，且上游错误 `name` ∈ 配置类名单（`missing_api_key` / `restricted_api_key` / `suspended_api_key` / `invalid_permission` / `not_found` / `method_not_allowed` / `missing_required_field` / `missing_required_parameter` / `invalid_attachment` / `invalid_parameter` / `invalid_idempotency_key` / `forge_not_enabled` / `forge_unavailable` / `return_path_unavailable`） | 500 | `server_misconfigured` | 发信通道配置异常，请联系管理员。**（新增文案）** |
| 5b | 其余 4xx（含 `validation_error`、含无法解析的响应体） | **422** | **`mail_recipient_rejected`** | **验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。**（新增文案） |

**重试语义不变**：拿到上游 HTTP 响应 ⇒ **只调 1 次**；只有网络异常 / 超时才重试 1 次（共 2 次调用）。

### 决策 2 — 新增错误码 `mail_recipient_rejected`（HTTP 422）

- `src/errors.ts` 的 `fail` 工厂新增一个可产出 **422 + 自定义 code** 的工厂；错误体形状 `{ error: { code, message } }` **不变**；
- 客户端不读 `error.code`（事实 7）⇒ 界面直接显示新文案，**无需改客户端**；
- 422 **不在**客户端重试集合（只重试 429 / 5xx / 传输失败）⇒ 永久拒绝不再被自动重试掩盖。

### 决策 3 — 失败注册一律回滚（`users` 行 + 本次验证码行）

`src/auth.ts` 把 `issueVerificationCode` 拆成「造码落库」与「发信」两步，由调用方负责发信与回滚：

- `handleRegister`：写 `users` 行 → 造码落库 → **发信**；发信抛错 ⇒ 用 `env.DB.batch([...])`（D1 batch 为事务）删除该 `users` 行（按 `id`）与本次 `email_verifications` 行（按 `id`），打印一行不含邮箱 / 验证码 / 密钥的诊断日志，然后**原样抛出**原异常；
- 效果：① 失败不留账号；② 邮箱不被占用（重试注册不会再撞 409 `email_taken`）；③ **失败不消耗 60 秒重发闸窗口**（闸门依据 `email_verifications`，行已删）⇒ 客户端的自动重试可以立即重试，不再被自己的节流闸挡住。

### 决策 4 — 重发失败只回滚验证码行，保留账号

`handleResend`：造码落库 → 发信；发信抛错 ⇒ 只删本次 `email_verifications` 行，**保留 `users` 行**（账号已存在，重发失败不应把账号删掉），然后原样抛出。

### 决策 5 — 诊断面（把「上游说了什么」变成可观测事实）

1. **响应头 `X-Fluxing-Mail-Provider-Status`**：值为上游返回的 HTTP status（纯数字）；**没有发出上游调用时（配置缺失 / 预检失败）不写该头**；
2. **日志行**：`[fluxing-sync] 第三方发信接口返回异常：status=<n> name=<上游错误名或空> body=<截断 300 字>`；**绝不含验证码明文、API Key、收件人邮箱**（沿用 `mail.ts:201` 的既有纪律）。

⇒ 决策 5 使「事实 5」的死角被打开：**下一次复发时，上游 status 可直接从响应头读到**，不再依赖 `wrangler tail`。

### 决策 6 — 不动客户端、不动 IPC 协议、不动客户端文案

- 注册 / 验证 / 重发链路**不走 IPC**（`include/WeaselIPCData.h` 内云账号相关检索零命中）⇒ **不触发** CLAUDE.md 的「IPC 四边同步」红线；
- 客户端只透传 `error.message`（事实 7）⇒ 服务端语义修好即界面修好；
- 本篇**不改** `WeaselServer/**`、**不改** `include/WeaselIPCData.h`、**不改**控件 ID 与布局表。

### 决策 7 — 不改发信通道架构与相关路径

- **不**改 `MAIL_MODE = "http"`、**不**改发件域 `aiec.fun`、**不**改 `MAIL_HTTP_*` 配置、**不**改 ADR 0058 的架构决策；
- **不**动 `password_resets` 路径（`migrations/0002_password_resets.sql:13-27`）：口令重置的「先落库后发信」同构问题**在本篇范围外**，登记为未覆盖项（见后果段）；
- **不** bump 版本号、**不**新增依赖、**不**改 `librime/`。

### 决策 8 — 回滚降级兜底、诊断头 ASCII 化、D1 配额错误的暂时性语义

生产实测逼出来的三条补充决策（同一批内落地）：

1. **回滚降级兜底**：`DELETE` 可能被 D1 拒（Cloudflare `code 7500`，每日行写入配额耗尽）⇒ 纯 DELETE 回滚在该状态下必然失败。回滚改为「**DELETE 优先，被拒则降级 UPDATE 失效化**」：
   - `users` 行 ⇒ `UPDATE users SET email = 'deleted+<userId>@invalid.local'`：解除「占号」⇒ 重试不再撞 409；
   - `email_verifications` 行 ⇒ `UPDATE email_verifications SET created_at = 0`：解除 60 秒重发窗口 ⇒ 重试不再撞 429；
   - 回滚状态**三态**：`ok`（真零残留）/ `degraded`（行还在但已失效化，效果等价）/ `failed`（两条路都失败）。**状态与失败原因只进诊断头，不进错误体、不改状态码与中文文案**；
   - 实现细节：回滚**逐条执行**，**绝不**用 `DB.batch`（batch 是整体事务，一条被拒则全部连坐 —— 生产实测 batch 一条都没删掉）。
2. **诊断头载荷一律 ASCII**：HTTP 头值必须 ASCII（Fetch 规范：非 ASCII 在浏览器侧直接抛 TypeError，WinHTTP 侧也可能解析异常）⇒ 失败原因用 ASCII 模板（`delete failed(...)` / `neutralize failed(...)`），脱敏标记用 `[redacted]`，中文只进服务端日志。
3. **D1 配额错误 = 暂时性 503**：任何写库失败若命中 `[code: 7500]`（每日行写入配额耗尽，UTC 午夜重置）⇒ 统一映射为 **503 `storage_quota_exceeded`**「服务端存储写入额度已用尽，请稍后重试。」，而不是笼统 500（否则客户端与运维都会误判为代码缺陷）。映射放在 `toErrorResponse` 兜底分支，惠及所有写库失败路径。

### 门禁判据

- `npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run typecheck` ⇒ **退出码 0**；
- `npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run check` ⇒ **退出码 0**，用例数 ≥ 基线 **4 文件 / 65 用例**（既有 65 条**逐条保留**，只允许追加）；
- `npm --prefix F:\soft\00selfmade\rime_claude\cloud\fluxing-sync run deploy` ⇒ **退出码 0**，生产路由 `api.aiec.fun`；
- 复验判据：① 保留域收件人 ⇒ **不再是 503**，而是 422 `mail_recipient_rejected`（或按事实 5 的自证法读出上游 status）；② 真实域收件人 ⇒ register 200 **且真收到验证码邮件** ⇒ verify 200 ⇒ login 200（端到端）；③ 发信失败后 D1 中该邮箱的 `users` / `email_verifications` **零残留**；④ 失败后同一邮箱**立即**重试**不撞 429、不撞 409**。

---

## Consequences

### 正面

- ✅ **永久拒绝不再伪装成「稍后重试」**：用户看到「请更换邮箱地址后重试」，而不是一个永远转圈的 503；
- ✅ **失败注册不再占号**：邮箱可立即重试，客户端的自动重试不再被 60 秒闸反咬；
- ✅ **失败不污染生产 D1**：不再产生 `email_verified=0` 的僵尸账号；
- ✅ **客户端零改动**：注册 / 验证 / 重发链路不走 IPC，本篇不触发四边同步红线；
- ✅ **可诊断性自证**：响应头直接给出上游 status，结束「`wrangler tail` 不通 ⇒ 无法诊断」的死角。

### 负面 / 取舍

- ⚠️ **上游 4xx 一律归给收件人侧（5b 分支）**，若上游实际是「我方 payload 字段非法」而错误名不是配置类名单，用户会看到「请换一个邮箱地址」——**误归因风险**。缓解：日志里保留上游 `name` + body 截断，可事后定位；
- ⚠️ **超时但上游实际已投递**的极端情形（上游收下、我们 10 秒超时）会因回滚而作废那一枚验证码（`mail.ts:41` 超时 10 s，上游实测 1.5–2.6 s ⇒ 概率极低）；用户重试会收到新码；
- ⚠️ **失败后不再消耗 60 秒窗口** ⇒ 连续失败的客户端可以把上游调用打得更密（但不会真的发出邮件，上游按**受理成功**计费，失败调用不计费）；
- ⚠️ 新增一个 HTTP 状态码 **422** 进入对外契约（客户端不读 code ⇒ 无兼容风险；外部若有基于 503 的监控需调整）。

### 未覆盖 / 未闭环

- ❗ **【P0 · 生产事故】生产 D1 写额度已耗尽**：账号 `016c8e73…` / 库 `e4ea114a…` 的 `UPDATE` 与 `DELETE` 一律被 Cloudflare 拒（`code 7500`），INSERT 在耗尽前仍可用 ⇒ 账号「能建、不能改、不能删」。**影响面不止注册**：云同步推送要写 `sync_items` ⇒ 额度耗尽期间**真实用户的云同步写路径也会失败**。**处置需用户拍板**：等 UTC 午夜自动重置（≈北京时间次日 08:00）/ 升级 D1 付费计划 / 其他。
- ❗ **本篇门禁判据 ③「零残留」、④「立即重试不撞 429/409」、⑤「真实域端到端」本轮未达成** —— 阻塞点是上面那条 D1 额度（DELETE 与 UPDATE 都被拒 ⇒ 连降级兜底都失效），不是本篇代码；配额恢复后用 `tmp\bq-verify.ps1` 与 `tmp\bq-e2e-mailtm-v4.ps1` 复验。
- ❗ **端到端另有一个外部阻塞**：`mail.tm` 的 token 端点对本探针地址持续 401（账号创建 201 成功）⇒ 取不到验证码；与本服务无关。
- ❗ **口令重置发信路径同构问题未修**（`password_resets` 表「先落库后发信」）—— 属另一条链路，本篇明确不碰；
- ❗ **上游原文（status + body）仍需部署后才取得**（本篇「决策 5」提供取证通道）；
- ❗ **真实用户的收件箱送达率仍未验证**（本轮用可读收件箱做端到端代理验证，不等于 QQ / Gmail 等真实邮箱的投递率与进信箱率）；
- ❗ **BO 交接文档 §4 / §7 的「注册链路不可用」表述需要就地修订**（事实 8）；
- ❗ 云端**脏条目**、R2 **孤儿对象**、双机项等 BP 在册未闭环项**不在本篇范围**。

---

## 验证口径（按此顺序）

1. `typecheck` ⇒ 退出码 0；
2. `check`（`check:endpoint` + `vitest run`）⇒ 退出码 0，用例数 ≥ 65，记录文件数与用例数；
3. `deploy` ⇒ 退出码 0，记录 Worker Version；
4. **保留域复现**：`POST /v1/auth/register`（`@example.com` 探针）⇒ 期望 **422 `mail_recipient_rejected`**，并从响应头 `X-Fluxing-Mail-Provider-Status` 读出上游 status；
5. **端到端复验**：可读收件箱 ⇒ register 200 → 真收到验证码邮件 → verify 200 → login 200（脚本 `tmp\bq-e2e-mailtm.ps1`，凭据只在内存）；
6. **零残留复验**：发信失败后查生产 D1，该邮箱 `users` / `email_verifications` 均为 0 行；
7. **立即重试复验**：同一保留域邮箱在失败后立刻再注册 ⇒ **不是 429、不是 409**；
8. **清理**：删除本批探针账号行（`email LIKE 'bq.%'`）与 `tmp` 下探针文件。

### 本轮（2026-11-22）实测结果

**门禁**

| 项 | 判据 | 实测 |
| --- | --- | --- |
| typecheck | 退出码 0 | `TSC_EXIT=0` |
| check | 退出码 0，用例数 ≥ 65 | `CHECK_EXIT=0`，**5 文件 / 86 用例**（基线 4 文件 / 65 用例 ⇒ +1 文件 +21 用例；既有 65 条逐条保留） |
| deploy | 退出码 0，路由 `api.aiec.fun` | `DEPLOY_EXIT=0`，生产 Version **`96facaba-9277-4c77-b9f9-4df5cb6de41e`** |

> 部署迭代史（同批中间版本，仅为过程记录）：v1 `a6323b2d-890f-4fcd-aba4-e004288d4392` → v2 `c0244ab0-a376-4383-b451-69fc8d0fdbe3`（回滚改逐条删除 + 回滚诊断头）→ v3 `84f82cf4-f47a-403a-97e9-d0a140e2df7e`（D1 配额 503 语义）→ **v4 `96facaba-9277-4c77-b9f9-4df5cb6de41e`**（回滚降级兜底 + 诊断头 ASCII 化）。

**复验 ①（保留域 `@example.com` 探针，生产实测）—— 达成**

- HTTP **422**、错误码 `mail_recipient_rejected`、文案「验证码邮件被邮件服务拒绝：该邮箱地址无法接收验证码，请更换邮箱地址后重试。」⇒ **判据「不再是 503」达成**；
- `X-Fluxing-Mail-Provider-Status: 422` ⇒ **上游实际返回 422**（「事实 5」的自证法落地：上游是 4xx 永久拒绝成立，但具体值由此前的推测 400 修正为 **422**）；
- `X-Fluxing-Mail-Rollback: failed` + `X-Fluxing-Mail-Rollback-Error: email_verifications delete failed(D1_ERROR: Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) …)` ⇒ **回滚被 D1 每日行写入配额挡住，且根因文本首次从外部自证**（`wrangler tail` 不通的老问题由此绕开）；
- 诊断头载荷实测为 ASCII（此前中文载荷会触发 Workers 的「header value contains non-ASCII characters」告警 ⇒ 已按决策 8 改为 ASCII 模板）。

**复验 ②③④⑤（零残留 / 立即重试 / 真实域端到端）—— 未达成，阻塞点不在本篇代码**

- 生产 D1 的 `UPDATE` 与 `DELETE` **一律被拒**：Cloudflare `code 7500`，原文「Your account has exceeded D1's free tier daily row write limit. Upgrade to a paid plan or wait until tomorrow (midnight UTC) to continue.」；Worker 内部回滚与 `wrangler d1 execute --remote` 手工语句**两条路径同样被拒**。
- 同时刻对照：**INSERT 仍可用**（探针 `users` / `email_verifications` 行数持续增长）、**UPDATE 在额度耗尽前仍可用**（23:05 一次 9 行批量 UPDATE 成功，`changes=9` / `rows_written=18`），额度耗尽后连单行 UPDATE 也被拒 ⇒ **账号变成「能建、不能改、不能删」**。
- ⇒ 判据 ③「零残留」与 ④「立即重试不撞 429/409」在配额恢复前**无法成立**（降级兜底也依赖 UPDATE，一并被拒）；判据 ⑤ 端到端同样受阻（`verify` 要写 `consumed_at` / `email_verified`）。
- **影响面不止注册**：云同步推送要写 D1 `sync_items` ⇒ 额度耗尽期间**真实用户的云同步写路径也会失败**（这已超出本篇范围，属独立 P0 事故，处置见下）。
- **端到端的第二个独立阻塞**：一次性收信服务 `mail.tm` 的 token 端点对本探针地址持续 **401 Invalid credentials**（账号创建 201 成功、换 token 401；加 6 秒开通延迟 + 4 次重试仍 401）⇒ 取不到验证码，与本服务无关。
- **建议复验路径**：配额恢复后跑 `tmp\bq-verify.ps1`（保留域 ⇒ 422 + 三头 + D1 计数）与 `tmp\bq-e2e-mailtm-v4.ps1`（真实域端到端；若 `mail.tm` 仍 401，改用真实邮箱人工端到端，需用户在场）。

---

## See also

- `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bq-register-mail-rejection-semantics.md`
- `F:\soft\00selfmade\rime_claude\docs\adr\0058-third-party-https-mail-api-channel.md`
- `F:\soft\00selfmade\rime_claude\docs\adr/0064-server-password-reset-endpoints.md`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\mail.ts`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\errors.ts`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\index.ts`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\mail-http.test.ts`
- `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\test\register-mail-failure.test.ts`
- `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-22-batch-bo-push-batch-align-server-limit.md`（§4 / §7 的口径由本篇修订）
- Resend 官方错误表：`https://resend.com/docs/api-reference/errors`
