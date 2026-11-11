# ADR 0058 · 验证码发信通道改走第三方 HTTPS 邮件 API（Resend 兼容）

- **状态**：Accepted（代码已实现并过门禁；**生产切换待 API Key 就位**）
- **日期**：2026-11-11
- **决策人**：用户（2026-11-11 明确拍板「引导我使用 resend 方案」，拒绝 Workers Paid）
- **关联**：spec 100 阶段 E / T605、批次 AL；[落地指南](../../docs/ops/fluxing-cloud-resend-setup.md)

## 背景

spec 100 的账号链路必须能把 6 位验证码**真实投递**到用户邮箱，否则「注册 → 验证 → 登录」只到 D1 表为止，用户永远拿不到码。既有两条通道都不成立：

| 方案 | 阻塞点 |
|---|---|
| Cloudflare `send_email` 绑定 | 官方要求 **Workers Paid**，账号级最低消费 **$5/月**；与用户 $0 路线冲突 |
| `MAIL_MODE="outbox"`（当前生产值） | 只写 D1 `mail_outbox`，**根本不发信**；且生产库存下**明文验证码** |

## 决策

服务端新增第三种发信模式 **`MAIL_MODE="http"`**：走**第三方 HTTPS 邮件 API**，首个服务商 **Resend**。

实现要点（`cloud/fluxing-sync/src/mail.ts`）：

- **通用而非 Resend 专用**：端点 / 鉴权头名 / 鉴权前缀全部走 env（`MAIL_HTTP_ENDPOINT` / `MAIL_HTTP_AUTH_HEADER` / `MAIL_HTTP_AUTH_PREFIX`），载荷采用 Resend 形状 `{from, to[], subject, text, html}` ⇒ **将来换服务商只改配置不改代码**。
- **密钥绝不落盘**：`MAIL_HTTP_API_KEY` 只走 Workers Secret（`wrangler secret put`），不进代码库 / D1 / 日志。
- **失败必须可读**：错误一律「中文原因 + 稳定错误码」，**禁止静默降级**（批次 AK 的教训：静默降级让用户只看到「网络请求失败」，服务端查不到原因）。
  - 缺 key / 端点非 HTTPS / `MAIL_FROM` 非法 / 上游 401·403 ⇒ `500 server_misconfigured`
  - 上游 429 ⇒ `503 mail_rate_limited`
  - 其它 4xx / 5xx / 网络异常 ⇒ `503 mail_send_failed`
- **重试策略**：网络类失败重试 **1 次**（共 2 次尝试）；业务类错误**不重试**（重试无意义且放大限流）。单次超时 10 s（`AbortController`）。
- **不写 `mail_outbox`**：http 模式**不落明文验证码**到生产库。
- **端点与发件地址均要求 HTTPS / 合法地址**，否则 fail closed。

## 备选与取舍

| 备选 | 为何不选 |
|---|---|
| Cloudflare `send_email`（需 Paid） | $5/月与用户 $0 路线冲突（用户 2026-11-11 明确拒绝） |
| 自建 SMTP | Worker 出站 25 端口受限；且要自维护 IP 信誉 / 退信 |
| `mail_outbox` + 人工取码 | 不能算「真实投递」；且明文验证码长期留库是合规负债 |
| 保留 `outbox` 作生产默认 | 用户永远收不到码，注册链路等于半残 |

## 后果

- ✅ 生产发信不再依赖 Workers 付费计划，**$0 路线成立**。
- ✅ 生产库不再存明文验证码。
- ⚠️ **引入第三方依赖**：Resend 额度（免费档 3,000 封/月、100 封/日、1 域名、2 key）成为新的外部约束；服务商故障即注册链路故障（已用 `503` + 中文原因暴露，不静默）。
- ⚠️ **DNS 需人工介入**：Resend 的 DKIM / SPF / MX 记录必须加到 Cloudflare，而本机现有凭据**无 DNS 写权限** ⇒ 只能由用户在控制台手动添加。
- ⚠️ **`mail_outbox` 取码路径下线**：`scripts/cloud/live-e2e-probe.mjs` 原从 `mail_outbox` 取明文码，切 http 后该步骤失效，取码来源须改为**用户邮箱**。

## 验证证据（verified，2026-11-11）

- 服务端门禁：`tsc --noEmit` = 0 · 端点策略扫描 = 0 · `vitest` **42 passed**（基线 30 + 新增 12）。
- 变异验证（证明用例有牙）：4 处定向破坏 —— 关重试 / 改 429 映射 / 关 http 分支 / 改限流码 —— **全部被单测捕获**（EXIT=1），还原后全量仍 **42 passed**。
- **未验证（not verified）**：真实邮件投递（需 API Key + 域名验证 + 用户在 Cloudflare 加 DNS 记录，本轮**未执行**）；生产 `MAIL_MODE` 仍为 `outbox`（⇒ 线上行为与部署前完全一致）；新代码**已部署**，Version ID `2e6db488-da38-4a3a-8ec7-7f302b167035`，`GET /v1/health` = 200。
