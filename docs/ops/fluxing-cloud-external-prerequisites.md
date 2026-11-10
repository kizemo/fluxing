# Fluxing 云同步 · 需要你亲自处理的几件事

> 适用：spec 100「Fluxing 云同步」服务端已落地（`cloud/fluxing-sync/`，阶段 C 提交 `5495d52`），
> 客户端运行时代码已落地（阶段 D，`WeaselServer/Cloud/`，2026-11-11）。
>
> 本文只列**必须由人（账号持有人 / 域名持有人）操作**的事项。凡是改代码能解决的，都不在这里。

## 0. 状态总览（2026-11-11 更新）

| # | 事项 | 状态 | 谁做 | 阻塞什么 | 花钱吗 |
|---|---|---|---|---|---|
| 1 | Cloudflare 账号 + 授权 `wrangler` | 🔴 **待办（硬阻塞）** | 你（OAuth 登录不可代做） | 一切部署 | 免费 |
| 2 | 创建 D1 库与 R2 桶 + 回填绑定 ID | 🔴 **待办**（授权后我可代跑） | 你授权 → 我代跑 | 服务端启动即报错 | 免费档内 |
| 3 | 自有域名 `aiec.fun` + `api.aiec.fun` 路由 | 🟡 **配置已就绪**，等部署自动生效 | 你只需跑 `wrangler deploy` | 客户端连不上服务端 | 域名本身可能已付费 |
| 4 | 验证码发信通道 | 🟢 **已拍板：$0** | — | 已解除 | $0 |
| 5 | 阶段 E 账号入口放哪 | 🟢 **已拍板：快捷设置栏的右键菜单** | — | 已解除 | 无关 |
| 6 | `plan.md` / `tasks.md` 六处偏差回写 | 🟢 **已完成 2026-11-11** | — | 已解除 | 无关 |

### 已替你做完的部分（2026-11-11）

- **阶段 D 客户端运行时全量落地**（T301–T305 五任务全 DONE）：`WeaselServer/Cloud/` 新建 18 个文件；WeaselServer Win32 构建 **0 错误**产出 exe；阶段 D 单测追加 8 个测试函数、整包 **`PASS=297 FAIL=0`**（基线 134）。详见 `docs/archive/handoffs/handoff-2026-11-11-batch-aa-stage-d-not-started.md`。
- **`wrangler.toml` 已填自有域名**：`routes = [{ pattern = "api.aiec.fun", custom_domain = true }]`、`MAIL_FROM = "no-reply@aiec.fun"`（**`database_id` 仍待回填**，见 §2）。
- **文档回写**：`plan.md` §3.1/§4.4/§5/§6.1/§6.2/§9/§13 与 `tasks.md` 各阶段状态行已按实做回写；新增 **ADR 0054**（客户端密钥派生与 salt 策略）。

---

## 1. Cloudflare 账号与授权（🔴 硬阻塞 · 唯一需要你点浏览器的一步）

### 1.1 你要做的

1. 注册 / 登录 Cloudflare 账号（`aiec.fun` 域名若已托管在 Cloudflare，用**同一个**账号 —— 这很关键，见 §3）。
2. 在本机终端执行：

   ```
   cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
   npx wrangler login
   ```

   浏览器弹出授权页 → 用你的账号点「Allow」。**这一步我无法代做**（OAuth 同意属人工动作，涉及你的账号凭据）。

3. 授权成功后自证：

   ```
   npx wrangler whoami
   ```

   **判据**：输出里能看到你的账号邮箱与 Account ID，且**不出现**「You are not authenticated」。

4. 回我一句「**已授权**」，剩下的 §2 / §3 / §4 我来跑。

### 1.2 不想用 OAuth 的替代方案

Cloudflare Dashboard → My Profile → API Tokens → Create Token，选 "Edit Cloudflare Workers" 模板，再补 D1 / R2 权限。拿到 token 后在**当前终端**设环境变量：

```
$env:CLOUDFLARE_API_TOKEN = '<你的 token>'
```

**不要**把 token 写进仓库任何文件。`.dev.vars` 可以放本地密钥，但要先确认它在 `.gitignore` 里（`cloud/fluxing-sync/` 已有 `.dev.vars.example`，真文件不入库）。

---

## 2. 数据面资源（D1 / R2）—— 授权后我可代跑

### 2.1 资源名（**以 `wrangler.toml` 为准，不是旧文档里的名字**）

| 资源 | 名称 | 绑定名 | 用途 |
|---|---|---|---|
| D1 数据库 | `fluxing-sync` | `DB` | 账号、设备、同步条目索引、验证码 |
| R2 桶 | `fluxing-sync` | `BUCKET` | 密文对象 |

### 2.2 命令（授权后按序执行）

```
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler d1 create fluxing-sync
npx wrangler r2 bucket create fluxing-sync
```

`wrangler d1 create` 会打印 `database_id`（一串 UUID）。**把它填回 `wrangler.toml` 的 `database_id = "..."`**（现在是占位全零 UUID）。

### 2.3 表结构与 pepper

```
npx wrangler d1 migrations apply fluxing-sync --remote
npx wrangler secret put AUTH_PEPPER
```

- 迁移 SQL 在 `cloud/fluxing-sync/migrations/0001_init.sql`（**7 表 4 索引**）。
- `AUTH_PEPPER` 是服务端的 pepper（Workers Secret，**绝不进代码库、不进 D1**）。缺失即 500 fail closed，**没有内置默认值可回退**。请给一段 ≥32 字节的随机串（例：`openssl rand -hex 32`，或 PowerShell `[Convert]::ToHexString((1..32 | % { Get-Random -Maximum 256 }))`）。

### 2.4 免费额度是否够

**够。** D1 Free：读 500 万行/天、写 10 万行/天、存储 5 GB 总额、10 个库；R2 Free：存储 10 GB-月、Class A 100 万次/月、Class B 1000 万次/月、**出站流量免费**。以「一个用户一天同步几十条」的量级，免费档有 3–4 个数量级余量。详见 `docs/research/fluxing-cloud-2026-11/free-tier-verdict.md`。

---

## 3. 自有域名 `aiec.fun` 与 `api.aiec.fun`

### 3.1 已替你做好的（仓库内）

`cloud/fluxing-sync/wrangler.toml` 现在是：

```toml
workers_dev = false
preview_urls = false
routes = [{ pattern = "api.aiec.fun", custom_domain = true }]
```

`custom_domain = true` 的含义：**`wrangler deploy` 会自动为 `api.aiec.fun` 创建 DNS 记录**（前提：`aiec.fun` 已托管在**同一个** Cloudflare 账号），指向部署目标主机名，并开启橙色云代理。**你不需要手工加 DNS 记录**（前提满足时）。

> ⚠️ **不要**打开 `workers_dev` / `preview_urls`。T103 硬约束：境内 DNS 会污染平台默认子域，端点必须挂在自有域名下；`cloud/fluxing-sync/test/check-endpoint-policy.mjs` 是**零豁免**扫描，连文档措辞都算。

### 3.2 部署与验证

```
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler deploy
```

**判据（两条都要跑）**：

```
curl -sS -o NUL -w "%{http_code}" https://api.aiec.fun/v1/health
```

期望 **200**。
- **522 / 523** ⇒ DNS 或 Worker 路由没绑上（多为「`aiec.fun` 不在同一账号」，或还没部署）。
- **404** ⇒ Worker 起了但路由没匹配（检查 `routes` 行是否生效、`wrangler.toml` 有没有被改坏）。

再从**另一台机器**跑同一条命令，验证不是本机缓存骗人。

### 3.3 兜底：`aiec.fun` 不在同一 Cloudflare 账号时

若 `aiec.fun` 托管在**别的** Cloudflare 账号（或不在 Cloudflare），`wrangler deploy` **无法**自动建记录 ⇒ 需手工加：

| 类型 | 名称 | 内容 | 代理 |
|---|---|---|---|
| CNAME | `api` | `wrangler deploy` 输出里给出的**目标主机名** | 已代理（橙色云） |

目标主机名由 `wrangler deploy` 直接打印（形如 `<worker>.<账号子域>.workers.dev`）—— **照抄那一行**即可。
加完再跑 §3.2 的两条判据。

### 3.4 客户端侧已就绪

客户端的 BaseUrl **可注入**：环境变量 `FLUXING_CLOUD_BASE_URL`，缺省 `https://api.aiec.fun`。⇒ 联调时若换端点，不必改代码。

---

## 4. 验证码发信通道：🟢 **已拍板 —— $0**

### 4.1 决策

**不采购 Workers Paid**（账号级最低消费 $5/月），走 $0 路线：

| 阶段 | 通道 | 成本 | 现状 |
|---|---|---|---|
| 开发 / 内测 | `MAIL_MODE="outbox"` —— 验证码只写 `mail_outbox` 表、**不发信** | $0 | ✅ **服务端已实现**（`src/mail.ts`），`wrangler.toml` 现值即此 |
| 上线前 | **第三方 HTTPS 邮件 API**（Workers Free 允许出站 `fetch`） | $0（免费档额度） | 🔶 **待实施**（需加一个 `MAIL_MODE` 分支 + 第三方 API key） |

**为什么不选 Cloudflare Email Service**：官方原文「Arbitrary-recipient sending requires Workers Paid」⇒ 账号级最低消费 $5/月，与 $0 目标冲突。

**代价（已记录）**：第三方通道**打破** T101「同厂商、不接第三方」的自定边界 —— 代价是多一个第三方账号 + 一把 API key 的托管面 + 该服务免费额度随时可能改。已写进 `plan.md` §4.4 的修订记录。

**协议面不变**：服务端只**收**验证码结果、不参与发信 ⇒ 换通道**不改客户端协议**（客户端消费的 login 响应 `email_verified` 语义不变）。未验证账号对 `push` 仍被 403 `email_not_verified`。

### 4.2 上线前要做的（不阻塞当前编码）

1. 选一家邮件 API（Resend / Brevo / SendGrid 等）并注册账号；
2. 在 Worker 里加 `MAIL_MODE="<vendor>"` 分支（转发到第三方 HTTPS 端点），API key 走 Workers Secret；
3. 补一条端到端发信用例（阶段 G）。

---

## 5. 阶段 E 账号入口：🟢 **已拍板 —— 快捷设置栏的右键菜单**

- **不是**面板第 4 槽（快捷面板仍是 4 键帽，几何与状态真相源不动 —— spec 099 边界）。
- **不是「删掉右键菜单」** —— 右键菜单**换地方**了：托盘语言栏项虽在批次 W 下线（ADR 0053），其右键菜单已迁到快捷设置栏品牌图标（ADR 0038），账号入口挂在**同一份菜单路由**上即可。
- 账号页本身**已可被程序调用**：`fluxing::cloud::account_dialog::Show()`（`WeaselServer/Cloud/CloudAccountDialog.h`）。
- 落地属**阶段 E 的 T405**（本批**未做**）。

---

## 6. 文档治理：🟢 **已完成（2026-11-11）**

用户已拍板「按实现回写」。本轮已做：

| 文档 | 改了什么 |
|---|---|
| `plan.md` | §3.1 密钥派生（CNG → 自实现 PBKDF2/HKDF + salt 策略）· §4.4 验证码 $0 路线 · §5 模块表与 §5.2 词库互斥（快照通道）· §6.1 线上契约补充 · §6.2 D1 表（7 表 + D1–D6 偏差表）· §9 变更面 · §13 验证策略 |
| `tasks.md` | 阶段 A–C 偏差回写说明 · 阶段 C 状态块 · 阶段 D 五任务状态与实现偏差 · 阶段 E 的 T401 / T405 |
| `docs/adr/0054-*.md` | 新增：客户端密钥派生与 salt 策略（CNG 的 PBKDF2 默认 PRF=SHA-1、CNG 无 HKDF、服务端从不用 salt 校验） |
| `docs/adr/README.md` | 加 0054 索引行 |

---

## 7. 你不需要操心的事（已处理 / 会处理）

- 客户端密钥派生、加密、信封封装（AES-GCM 复用共享头，5 字段 AAD）—— 纯本地计算，无外部依赖。
- HTTPS 客户端（WinHTTP）· 凭据存储（Credential Manager / DPAPI）—— **不落明文、不落日志、口令用后清零**。
- 账号状态机、设备管理、词库快照导出 / 还原 —— 全部单测覆盖。
- 服务端 9 条契约端点 + `/v1/health`、23 个文件、20/20 用例 —— 阶段 C 已 DONE 并提交 `5495d52`。
- 客户端 18 个新文件、单测 `PASS=297 FAIL=0`、WeaselServer 构建 0 错误 —— 阶段 D 已 DONE。

---

## 8. 未取证清单（诚实标注）

以下事实**没有一手证据**，不要当成已确认：

1. 第三方邮件服务（Resend / Brevo / SendGrid）的免费额度、条款、是否需信用卡 —— 未核价（本机 `web_search` 返回 HTTP 402，只能 `web_fetch` 抓指定 URL）。
2. Cloudflare Email Service 是否按封单独计费、Paid 含的 3,000 封是否跨月累计 —— 只有 T101 的二手结论，未在官方价目页原文核到。
3. Workers Paid 的 $5 是否有更便宜的 Workers 专用订阅 —— 官方原文写的是「minimum charge of $5 USD per month for an account」，未见更低档。
4. D1 免费档「5 GB total storage」是按账号还是按库 —— 官方原文措辞为总额，我按账号总额理解。
5. 本地区（中国大陆）到 Cloudflare 边缘的**实际延迟与可用性** —— T103 只做了端点可达性设计，**没有真实链路测量**。这会影响真机体验，上线前需实测。
6. **PBKDF2 60 万次在本机的实际耗时** —— 单测用 1000/2000 次 + 常量断言，登录路径走 600000 次但**未单独计时**。首次登录延迟需真机测量后再定 UI 反馈策略。
