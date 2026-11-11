# 验证码发信通道 · Resend 落地指南（$0 路线）

> 决策来源：2026-11-11 用户拍板 —— 走第三方 HTTPS 邮件 API（Resend），**不**开通 Cloudflare Workers Paid（$5/月冲突）。
> 服务端实现：`cloud/fluxing-sync/src/mail.ts` 的 `MAIL_MODE="http"` 分支（通用第三方 HTTPS 邮件 API + Resend 兼容载荷）。
> 本文件是**用户侧操作指南**：只列「你要动手做的事」，我方代码/门禁状态见 §5。

---

## 0. 链路总览

```
客户端「注册」→ POST /v1/auth/register
    → 服务端生成 6 位验证码（存 D1 email_verifications，存 hash 不存明文）
    → 服务端调 POST https://api.resend.com/emails   ← 本次要打通的这一段
    → Resend 投递到你填的邮箱
你收到验证码 → 填码 → POST /v1/auth/verify-email → email_verified=1 → 登录
```

## 1. 需要你做的三件事（总计约 15 分钟）

| # | 事项 | 在哪做 | 预计耗时 |
|---|---|---|---|
| 1 | 注册 Resend 账号 | [resend.com/signup](https://resend.com/signup) | 3 分钟 |
| 2 | 添加发信域名 + 在 Cloudflare 加 3~4 条 DNS 记录 | Resend 后台 + Cloudflare 控制台 | 8 分钟 |
| 3 | 创建 API Key 并注入 Workers Secret | Resend 后台 + 一条命令 | 4 分钟 |

**我方代码已经全部就绪**，你做完上面三步，我立刻切 `MAIL_MODE="http"` 部署并做真实发信验证。

---

## 2. 步骤一：注册 Resend 账号

1. 打开 <https://resend.com/signup>，用你的邮箱注册（建议用 `kizemo@163.com`，待验证账号与日常收信邮箱一致，便于核对）。
2. 收激活邮件并完成验证（未激活无法创建域名与 API Key）。
3. 免费档额度（够用）：**3,000 封/月、100 封/日、1 个发信域名、2 个 API Key**。验证码邮件量级远低于此。

## 3. 步骤二：添加发信域名 + DNS 记录

### 3.1 Resend 后台添加域名

Resend 后台 → **Domains** → **Add Domain**：

- 域名填 **`aiec.fun`**（根域）。这样 `no-reply@aiec.fun` 不用改。
  > Resend 官方更推荐子域（如 `mail.aiec.fun`）以隔离发信信誉。若你想更规范：用 `mail.aiec.fun`，并把 `wrangler.toml` 的 `MAIL_FROM` 改成 `no-reply@mail.aiec.fun`、`MAIL_FROM_NAME` 不变。
- **Region**：选离收件人最近的。国内用户建议 `ap-northeast-1` / `ap-southeast-1`；不确定就选 `us-east-1`（不影响功能）。
- **Return-Path**：留默认（`send.aiec.fun`）。

添加后进入该域名的 **Records** 标签页，会看到 3 条记录（DKIM / SPF / MX），**下一步要照抄它们**。

### 3.2 Cloudflare 控制台加 DNS 记录（必须你手动做）

Cloudflare → 站点 **aiec.fun** → **DNS** → **Add record**，逐条照抄 Resend 给的值：

| 类型 | 名称（Name） | 内容（Value） | 必需 | 说明 |
|---|---|---|---|---|
| TXT | `resend._domainkey` | `p=...`（Resend 给） | ✅ | DKIM 签名 |
| MX | `send` | `feedback-smtp.<region>.amazonses.com` | ✅ | 回信路由；若 Resend 给的是 CNAME 就选 CNAME 类型 |
| TXT | `send` | `v=spf1 include:amazonses.com ~all` | ✅ | SPF 授权 |
| TXT | `_dmarc` | `v=DMARC1; p=none; rua=mailto:dmarc@resend.dev` | 可选 | DMARC，建议加 |

> ### ⚠️ 头号坑：CNAME 记录不要开橙云代理
> Resend 官方原文：*"when shown a CNAME, make sure **not** to use proxying features (e.g., Cloudflare's orange cloud) for that record, as it will prevent verification from completing."*
> ⇒ 如果 Resend 给的是 **CNAME**，在 Cloudflare 里 **Proxy status 必须选 `DNS only`（灰云）**。开了橙云，域名验证**永远不通过**。

> 提示：本机现有的 Cloudflare 凭据**没有 DNS 写权限**（OAuth 只有 zone 读；用户自建 Token 只有 Account 作用域 4 条），所以这一步只能你手动点。若你想让我代劳，需要新建一个带 `Zone > DNS > Edit` 权限的 API Token 给我。

### 3.3 等验证通过

- 一般 **15 分钟内**通过，DNS 全球传播最长 72 小时。
- Resend 后台该域名状态变 **Verified** 即成功；超时可点 "Restart verification"。
- 自查：<https://dns.email/>

## 4. 步骤三：创建 API Key 并注入

### 4.1 Resend 后台创建

Resend 后台 → **API Keys** → **Create API Key**：

- **Name**：`fluxing-sync`
- **Permission**：选 **`Sending access`**（最小权限，只要发信权限，不要 full access）
- **Domain**：选 `aiec.fun`（限定只能从这个域发）

> **API Key 只显示一次**，当场复制保存（形如 `re_xxxxxxxxxxxxxxxx`）。

### 4.2 注入 Workers Secret（两种方式，选一种）

**方式 A（推荐，我把 key 挡在上下文之外）**

你把 key 存成一个纯文本文件，然后执行一条命令：

```powershell
# 1) 存 key（记事本另存为 UTF-8，一行，没有多余空格）
notepad F:\soft\00selfmade\rime_claude\tmp\resend-key.txt
# 内容示例：re_AbCdEf0123456789

# 2) 一键上线（注入 Secret + 切 MAIL_MODE=http + 部署 + 巡活 + 可选发测试信）
pwsh -NoProfile -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\scripts\cloud\resend-go-live.ps1 -TestEmail kizemo@163.com
```

脚本**不回显 key**，只打印步骤与状态码。`tmp\` 已在 `.gitignore` 里，key 不会进仓库。

**方式 B（你自己注入，我来完成剩下的）**

```powershell
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
node node_modules\wrangler\bin\wrangler.js secret put MAIL_HTTP_API_KEY
```
（提示时粘贴 key，回车）执行完告诉我「已注入」，我接着切 `MAIL_MODE="http"`、部署并验证。

---

## 5. 我方已就绪的部分（无需你参与）

| 项 | 状态 |
|---|---|
| `src/mail.ts` `MAIL_MODE="http"` 分支（Resend 兼容载荷 / 10s 超时 / 网络类重试 1 次 / 错误分类） | ✅ 代码完成 |
| `src/env.ts` 新增 5 个 env 字段（`MAIL_HTTP_ENDPOINT` / `MAIL_HTTP_API_KEY` / `MAIL_HTTP_AUTH_HEADER` / `MAIL_HTTP_AUTH_PREFIX` / `MAIL_FROM_NAME`） | ✅ |
| `wrangler.toml` 新增 vars（端点 / 鉴权头 / 前缀 / 发件人显示名） | ✅（`MAIL_MODE` 仍为 `outbox`，等 key 就位再切） |
| `test/mail-http.test.ts`：12 条单测（成功 / 缺 key / 非 HTTPS / 401 / 403 / 429 / 500 / 网络重试 / 不写 outbox / 端点可配 …） | ✅ |
| 门禁：`tsc --noEmit` = 0 · 端点策略扫描 = 0 · `vitest` **42 passed**（基线 30 + 新增 12） | ✅ |
| 变异验证：4 处定向破坏（关重试 / 改 429 映射 / 关 http 分支 / 改限流码）**全部被单测捕获**，基线恢复后 12/12 绿 | ✅ |
| `MAIL_HTTP_API_KEY` 走 Workers Secret（**绝不**进代码库 / D1 / 日志） | ✅ 设计如此 |
| http 模式**不写 `mail_outbox`** ⇒ 生产库不留明文验证码 | ✅ 设计如此 |

## 6. 排错速查

| 现象 | 含义 | 处理 |
|---|---|---|
| 注册返回 `500 server_misconfigured` | Secret 没注入 | 走 §4.2 注入后重试 |
| 注册返回 `503 mail_rate_limited` | 触发服务商限流 | 等一分钟再试；免费档 100 封/日 |
| 注册返回 `503 mail_send_failed` | 上游 5xx 或网络异常 | 看 Worker 日志里的 `status=` 与响应体片段 |
| Resend 后台域名一直 `Pending` | DNS 没生效 / CNAME 开了橙云 | 回 §3.2 查 Proxy status |
| Resend 返回 403 | 域名未验证或 key 无权 | 回 §3.3 / 检查 key 的 Domain 限制 |
| 收到邮件但进垃圾箱 | 新域信誉未起 | 加 DMARC + 后续注册量爬坡；免费额度不影响 |
| 邮件根本没到、也没报错 | 检查发件地址是否与已验证域名一致 | `MAIL_FROM` 必须属于已验证域 |

## 7. 与既有探针的关系（已知需同步修改）

`scripts/cloud/live-e2e-probe.mjs` 的取码步骤原本**从 D1 `mail_outbox` 读明文验证码**。
`MAIL_MODE="http"` 不再写该表 ⇒ 切 http 后该探针的取码来源必须改为**用户邮箱**（或 Resend API 的发信记录），否则会读到空。
本轮部署时一并处理，处理前**不要**用旧探针的取码结论判定线上发信失败。

## 8. 相关文档

- 决策与架构：`docs/adr/0058-*.md`（第三方 HTTPS 邮件 API 通道）
- 外部前置总表：`docs/ops/fluxing-cloud-external-prerequisites.md`
- 通道选型调研：`docs/research/fluxing-cloud-2026-11/email-verify-channel.md`
- 客户端协议契约（换通道不改编解码）：`.specify/specs/100-fluxing-cloud-account-sync/plan.md` §4.4
