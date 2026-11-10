# Fluxing 云同步 · 需要你亲自处理的几件事

> 适用：spec 100「Fluxing 云同步」服务端已落地（`cloud/fluxing-sync/`，阶段 C 提交 `5495d52`），
> 客户端运行时代码已落地（阶段 D，`WeaselServer/Cloud/`，2026-11-11）。
>
> 本文只列**必须由人（账号持有人 / 域名持有人）操作**的事项。凡是改代码能解决的，都不在这里。

## 0. 状态总览（2026-11-11 更新）

| # | 事项 | 状态 | 谁做 | 阻塞什么 | 花钱吗 |
|---|---|---|---|---|---|
| 0 | **启用 R2**（账号级开关，后台点一次） | 🟢 **已完成**（用户 2026-11-10 21:57 确认「R2 已启用」；实测 `r2 bucket create` 不再返回 `code 10042`） | 你已点 | 已解除 | 免费额度内 |
| 1 | Cloudflare 账号 + 授权 `wrangler` | 🟢 **已完成**（OAuth Token，凭据写入 2026-11-10 21:24；权限实测含 `d1 write` / `workers_scripts write` / `workers_routes write` / `zone read`） | — | 已解除 | 免费 |
| 2 | 创建 D1 库 + 回填 `database_id` | 🟢 **已完成**（`e4ea114a-29b4-4abd-87e6-4e6cb20d86e4`，已回填 `wrangler.toml`） | — | 已解除 | 免费档内 |
| 3 | D1 迁移 `0001_init.sql` | 🟢 **已完成**（远程执行 13 条命令，`sqlite_master` 计 9 张表） | — | 已解除 | 免费档内 |
| 4 | 创建 R2 桶 `fluxing-sync` | 🟢 **已完成 2026-11-11**（`r2 bucket create fluxing-sync` → `✅ Created bucket`，`CREATE_EXIT=0`） | — | 已解除 | 免费档内 |
| 5 | 部署 Worker + 自动建 `api.aiec.fun` DNS | 🟢 **已完成 2026-11-11**（`wrangler deploy` 成功，Version ID `c22a483d-0460-4d17-b46b-ac3347c2c138`；`api.aiec.fun (custom domain)` 自动建好；`/v1/health` = **200**） | — | 已解除 | 免费 |
| 6 | 验证码发信通道 | 🟢 **已拍板：$0** | — | 已解除 | $0 |
| 7 | 阶段 E 账号入口放哪 | 🟢 **已拍板：快捷设置栏的右键菜单** | — | 已解除 | 无关 |
| 8 | `plan.md` / `tasks.md` 六处偏差回写 | 🟢 **已完成 2026-11-11** | — | 已解除 | 无关 |
| 9 | 写入 Workers Secret `AUTH_PEPPER` | 🟢 **已完成 2026-11-11**（`secret put` 成功，`secret list` 见 `AUTH_PEPPER` / `secret_text`） | — | 已解除 | 免费 |

> **令牌 vs OAuth（2026-11-10 实测结论）**：OAuth 授权**已经可用**，令牌不是当前的阻塞点。
> 建 API Token 的真正价值是「**不依赖 30 天有效的 refresh_token**、可供 CI 使用」⇒ 可选但推荐，见 §1.3。

### 已替你做完的部分（2026-11-11）

- **阶段 D 客户端运行时全量落地**（T301–T305 五任务全 DONE）：`WeaselServer/Cloud/` 新建 18 个文件；WeaselServer Win32 构建 **0 错误**产出 exe；阶段 D 单测追加 8 个测试函数、整包 **`PASS=297 FAIL=0`**（基线 134）。详见 `docs/archive/handoffs/handoff-2026-11-11-batch-aa-stage-d-not-started.md`。
- **`wrangler.toml` 已填自有域名**：`routes = [{ pattern = "api.aiec.fun", custom_domain = true }]`、`MAIL_FROM = "no-reply@aiec.fun"`、`database_id = e4ea114a-29b4-4abd-87e6-4e6cb20d86e4`（已回填，见 §2.2）。
- **文档回写**：`plan.md` §3.1/§4.4/§5/§6.1/§6.2/§9/§13 与 `tasks.md` 各阶段状态行已按实做回写；新增 **ADR 0054**（客户端密钥派生与 salt 策略）。

---

## 1. Cloudflare 账号与授权（🟢 已完成 · 2026-11-10 21:24）

### 1.1 当前状态（已实测，勿重做）

OAuth 授权**已经成功**，凭据落在 `C:\Users\Duanyi\AppData\Roaming\xdg.config\.wrangler\config\default.toml`
（`oauth_token` / `refresh_token` / `expiration_time` / `scopes` 四个键，写入时刻 **2026-11-10 21:24:37**）。

```
👋 You are logged in with an OAuth Token, associated with the email kizemo@163.com.
┌──────────────────────────┬──────────────────────────────────┐
│ Account Name             │ Account ID                       │
├──────────────────────────┼──────────────────────────────────┤
│ Kizemo@163.com's Account │ 016c8e736b685af88860034ea6eb1daa │
└──────────────────────────┴──────────────────────────────────┘
```

实测权限范围（`whoami` 输出原文）：`user (read)` · `account (read)` · `zone (read)` · `d1 (write)` ·
`workers (write)` · `workers_scripts (write)` · `workers_routes (write)` · `workers_kv (write)` ·
`workers_tail (read)` · `pages (write)` · `ssl_certs (write)` · `offline_access` 等。

⇒ **§2 / §3 的 D1 与部署命令已可代跑**（D1 库已建、迁移已应用，见 §2）。
⇒ **令牌（API Token）不是当前阻塞点**；它解决的是「refresh_token 30 天到期后要重新授权」与「CI 无交互授权」两件事。

### 1.2 为什么 handoff 里写的是「未授权」（已修正）

批次 AD handoff 记录的 `whoami` = 「You are not authenticated」是**授权完成前**的快照。
用户于 21:24 自行完成 `wrangler login` 并点 Allow ⇒ 以本节为准。

### 1.3 建 API Token（可选但推荐 · 简体中文后台逐步指引）

> Cloudflare 控制台支持简体中文界面。下文每一步都给了**中文文案 + 英文原文**两套标签；
> 若你界面上的措辞与此处不同（文案随版本微调），**以英文原文为准**。
> 直达链接：<https://dash.cloudflare.com/profile/api-tokens>

**第 0 步 · 可选：切成简体中文界面**
控制台左下角头像/账户名 → 「语言 / Language」→ 简体中文。

**第 1 步 · 打开「API 令牌」页**
左下角头像 → 「我的个人资料」(My Profile) → 左侧「API 令牌」(API Tokens)。
（也可以直接开 <https://dash.cloudflare.com/profile/api-tokens>）

**第 2 步 · 点「创建令牌」(Create Token)**
页面顶部右侧蓝色按钮。点开后在「创建令牌」页看到两条路：
- 「使用模板」(Use Template) —— 现成模板；
- 「创建自定义令牌」(Create Custom Token) —— 自己配权限。

**第 3 步 · 选「创建自定义令牌」(Create Custom Token)**

> 为什么不直接用模板：官方模板「编辑 Cloudflare Workers」(Edit Cloudflare Workers) **不含 D1 权限**，
> 而我们必须跑 `wrangler d1 create` / `d1 migrations apply --remote`。
> 走模板的话，得先点「使用模板」再在下一步补 D1 —— 步骤反而更多。
> ⚠️ **不要**选「编辑 Cloudflare 管理员」(Edit Cloudflare Administrator) 之类的全局管理员模板。

**第 4 步 · 填「令牌名称」(Token name)**
建议 `fluxing-sync-deploy`。（名称以后可改，**令牌值不行**。）

**第 5 步 · 配置「权限」(Permissions) —— 关键是「按作用域分条」**

> ⚠️ **2026-11-11 实测澄清（照着旧版本这步会卡住）**
>
> 新版建令牌界面把权限拆成**按作用域分开的 policy 条**，第一屏只有两个入口：
> - 「从头开始，使用**帐户**策略」(Start with an **account** policy)
> - 「从头开始，使用**区域**策略」(Start with a **zone** policy)
>
> **某条策略的权限下拉只列该作用域下的权限组**：
> - 帐户策略 → 只列**帐户 (Account)** 级权限组（D1、Workers Scripts、Workers R2 Storage、Account Settings…）
> - 区域策略 → 只列**区域 (Zone)** 级权限组（DNS、Workers Routes、Zone Settings、Zone WAF…）
>
> 所以在帐户策略的权限下拉里**搜不到** `DNS` 与 `Workers 路由` —— 这不是界面缺项，
> 而是它们属于区域作用域。同一个概念词在两个作用域下**英文动词写法还不一致**（据官方权限表）：
>
> | 作用域 | 英文权限名写法 | 例 |
> |---|---|---|
> | 帐户 (Account) | **Edit** | `D1 Edit`、`Workers Scripts Edit`、`Workers R2 Storage Edit` |
> | 区域 (Zone) | **Write** | `DNS Write`、`Workers Routes Write`、`Zone Write` |
>
> 中文界面里两者都可能译作「编辑 / 写入」，**以英文原文的 Edit / Write 为准**。
> 另：帐户策略里能搜到的 `DNS 防火墙`(DNS Firewall) 是**另一个**权限组，不是 DNS 记录权限。

**5.1 第一条策略：帐户策略（本项目必须，4 行）**

点「添加权限」(Add permission) 逐行加：

| 作用域 | 权限组 | 级别（英文原文） | 为什么需要 |
|---|---|---|---|
| 帐户 (Account) | D1 | 编辑 (**Edit**) | `wrangler d1 create` / `migrations apply --remote` |
| 帐户 (Account) | Workers R2 存储 (Workers R2 Storage) | 编辑 (**Edit**) | `wrangler r2 bucket create` |
| 帐户 (Account) | Workers 脚本 (Workers Scripts) | 编辑 (**Edit**) | `wrangler deploy` / `secret put` |
| 帐户 (Account) | 帐户设置 (Account Settings) | 读取 (**Read**) | 令牌模式下让 wrangler 能列出账号 |

**5.2 第二条策略：区域策略（本项目**不需要**，可整条跳过）**

历史上这里列过 `DNS Write` + `Workers Routes Write` 两行，**现已确认非必需**：
`wrangler.toml` 用的是
`routes = [{ pattern = "api.aiec.fun", custom_domain = true }]`，
且 zone `aiec.fun` 与 Worker **同账号** ⇒ DNS 记录与路由绑定由 Workers 平台在 `deploy` 时
**自动创建**。实证：2026-11-11 `wrangler deploy` 输出 `api.aiec.fun (custom domain)`，
全程**未手工建任何 CNAME**，部署后 `https://api.aiec.fun/v1/health` 直接 200。

> 若将来确实需要区域级权限（例如换用 `routes` 显式指定而非 `custom_domain`，
> 或让令牌代管别的子域），做法是：在令牌编辑页点「添加策略 / Add policy」→
> 选「区域」→ 「包含 / Include」→ 「特定区域 / Specific zone」→ `aiec.fun`
> → 该策略的权限下拉里才会出现 `DNS Write` / `Workers Routes Write`。

**第 6 步 · 配置策略的作用域（Resources / Include）**
- 帐户策略 → 「包含 / Include」→ 「特定帐户 / Specific account」→ 选
  `Kizemo@163.com's Account`（Account ID `016c8e736b685af88860034ea6eb1daa`）；
  - 界面默认给的就是「所有帐户 / All accounts」，**能收窄就收窄**，但只有一个账号时两者等价。
- 区域策略（若建了）→ 「包含 / Include」→ 「特定区域 / Specific zone」→ `aiec.fun`。

**第 7 步 · 「客户端 IP 地址过滤」(Client IP Address Filtering)** —— **留空**。本机公网 IP 会变，填了就等于自锁。

**第 8 步 · 「期限」(TTL)** —— 选最长（例如 1 年）。到期后需重建，命令不变。

**第 9 步 · 「继续以…摘要」(Continue to summary) → 「创建令牌」(Create Token)**。

**第 10 步 · 立刻复制令牌值**
弹窗明确提示令牌值**只显示这一次**，关闭后无法再查看（只能删了重建）。

**第 11 步 · 存进项目（推荐路径，已被 .gitignore 覆盖）**

```
cloud\fluxing-sync\.env
```
内容一行：
```
CLOUDFLARE_API_TOKEN=<粘贴你的令牌>
```

安全性已实测：该目录 `.gitignore` 第 7 行忽略 `.env`（`git check-ignore -v` 命中
`cloud/fluxing-sync/.gitignore:7:.env`）⇒ 不会入库。
⚠️ 别把令牌写进 `wrangler.toml`、`.dev.vars.example`、任何文档或提交信息。

**第 12 步 · 自证**
```
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler whoami
```
看到账号邮箱与 Account ID 即成功（`.env` 里的令牌优先于已存在的 OAuth 凭据）。

**第 12.1 步 · 实测验证记录（2026-11-11 22:30 · 🟢 全绿）**

用户按本节建了「帐户策略 + 4 条权限」的自定义令牌，实测**四条能力全部用令牌身份跑通**：

| 要验的权限 | 命令 | 结果 |
|---|---|---|
| 令牌本身 + `Account Settings Read` | `wrangler whoami` | ✅ 列出 Account ID `016c8e736b685af88860034ea6eb1daa` |
| `D1 Edit` | `wrangler d1 list` / `d1 execute --remote` | ✅ 实查表名 users / devices / refresh_tokens / sync_items |
| `Workers R2 Storage Edit` | `wrangler r2 bucket list` | ✅ 见 `fluxing-sync` |
| `Workers Scripts Edit`（读） | `wrangler secret list` | ✅ 见 `AUTH_PEPPER` |
| `Workers Scripts Edit`（**写**） | `wrangler deploy` | ✅ 退出码 0，Version `bf0305d3-…`，`api.aiec.fun (custom domain)` |

- **身份确认**：`whoami` 输出 `You are logged in with an User API Token` +
  `The API Token is read from the CLOUDFLARE_API_TOKEN environment variable`
  ⇒ 确证走的是令牌，**不是 OAuth 回落**，测试有效。
- 出现 `Unable to retrieve email for this user. Are you missing the User->User Details->Read permission?`
  **属正常**：我们没勾 `User Details Read`，不影响任何部署能力。
- **`DNS Write` / `Workers Routes Write` 不需要**：`deploy` 后 `api.aiec.fun` 绑定成功、
  线上 `/v1/health` 返回 200，全程未手工建 DNS 记录。
- ⚠️ 别拿 `d1 list` 的 `num_tables` 当「迁移没生效」的证据 —— 本次该字段显示 `0`，
  但 `d1 execute --remote` 实查 `sqlite_master` 拿到全部业务表。以 `sqlite_master` 为准。

> **机制实证（A/B，2026-11-10）**：wrangler **4.159.0 会自动读取当前工作目录的 `.env`** 里的
> `CLOUDFLARE_API_TOKEN`。对照组放了一个假令牌 → 报
> `Invalid format for Authorization header [code: 6111]`；撤掉 `.env` → 回落到 OAuth 凭据并成功。
> ⇒ **不需要** `setx`、也不需要把令牌塞进系统环境变量。

**第 13 步（可选）** 想让令牌在你自己新开的终端里也生效：
```
setx CLOUDFLARE_API_TOKEN "<你的令牌>"
```
（只写用户级环境变量；已开着的终端不会自动继承。）

### 1.4 启用 R2（🟢 **已完成 2026-11-11** · 以下为历史留痕）

> **当前状态**：你于 2026-11-10 21:57 在后台点完「启用 R2」，我于 2026-11-11 实测复核 ——
> `r2 bucket create fluxing-sync` 返回 `✅ Created bucket 'fluxing-sync' with default storage class of Standard.`，
> **不再出现 `code 10042`** ⇒ 该项已解除，本节仅作留档，不需要你再做任何操作。

原阻塞现象（2026-11-10 21:31 实测）：`wrangler r2 bucket create` → `code 10042`
「Please enable R2 through the Cloudflare Dashboard」。
⇒ 这是**账号级开关**，**API Token 绕不过去**，只能人去后台点一次：

1. 控制台左侧栏 → 「R2 对象存储」(R2 Object Storage)；
2. 首次进入会显示未启用 → 点「启用 R2 / 购买」(Enable R2 / Purchase)；
3. Cloudflare 会要求**绑定支付方式**（信用卡）—— 免费额度内不扣费，它只是超额计费的担保。
   ⚠️ 这一步与 spec 100 的「$0 路线」不冲突（**不选 Workers Paid**，不产生月度最低消费），
   但确实需要一张卡；不想绑卡的话，唯一替代是把密文对象改存 D1 BLOB ⇒ **需要改服务端代码**，属另一个决策。
4. 启用后回一句「**R2 已启用**」，我立刻代跑：
   `wrangler r2 bucket create fluxing-sync` → `secret put AUTH_PEPPER` → `deploy` → `curl` 判据。
   ⇒ 🟢 **2026-11-11 已全部代跑完毕**（见 §2 / §3.2）。

---

## 2. 数据面资源（D1 / R2）—— 🟢 **全部完成（2026-11-11）**

### 2.1 资源名（**以 `wrangler.toml` 为准，不是旧文档里的名字**）

| 资源 | 名称 | 绑定名 | 用途 | 现状 |
|---|---|---|---|---|
| D1 数据库 | `fluxing-sync` | `DB` | 账号、设备、同步条目索引、验证码 | ✅ 已建：`e4ea114a-29b4-4abd-87e6-4e6cb20d86e4` |
| R2 桶 | `fluxing-sync` | `BUCKET` | 密文对象 | ✅ 已建（2026-11-11，`Standard` 存储类） |

### 2.2 命令（✅ 2026-11-11 已全部执行，无需重跑）

```
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler r2 bucket create fluxing-sync     # ✅ Created bucket 'fluxing-sync'
```

`wrangler d1 create` 已执行（2026-11-10 13:24:35Z），产出 `database_id = e4ea114a-29b4-4abd-87e6-4e6cb20d86e4`，
**已回填进 `wrangler.toml`**（原为占位全零 UUID）。

### 2.3 表结构与 pepper（✅ 均已落地）

```
npx wrangler d1 migrations apply fluxing-sync --remote   # ✅ 已执行（13 条命令，sqlite_master 计 9 张表）
npx wrangler secret put AUTH_PEPPER                      # ✅ 已执行（2026-11-11，需 Worker 存在 ⇒ 排在 deploy 之后）
```

- 迁移 SQL 在 `cloud/fluxing-sync/migrations/0001_init.sql`（**7 表 4 索引**）。
- `AUTH_PEPPER` 是服务端的 pepper（Workers Secret，**绝不进代码库、不进 D1**）。缺失即 500 fail closed，**没有内置默认值可回退**。
- 本机已备好 32 字节随机值，存于 **gitignored** 的 `tmp/AD-auth-pepper.txt`（64 位十六进制，实测 `isHex64=True`）⇒ 部署时由我读入并 `secret put`，**不入库**。
- **已写入远程 Workers Secret**（2026-11-11）：`secret put` 回 `✨ Success! Uploaded secret AUTH_PEPPER`；`secret list` 回 `{"name":"AUTH_PEPPER","type":"secret_text"}`。
- **两条功能判别（不只信元数据自证）**：
  1. **只读路径** —— `POST /v1/auth/login` 打一个不存在的用户，回 **401** `{"error":{"code":"invalid_credentials",…}}`。
     若 pepper 未加载，此处必然是 **500 `server_misconfigured`**（`src/auth.ts:73` `requirePepper()`，pepper < 16 字符即 fail closed）⇒ 反证 pepper 真实进入运行时。
  2. **写入路径** —— `POST /v1/auth/register` 一个冒烟用户，回 **200** 且带 `account_id` / `salt` / `kdf:{algo:PBKDF2-SHA256,iters:600000}` / `need_email_verify:true`
     ⇒ 凭据落库、KDF 参数、发信入队三条链路都通。
- **D1 落库复核**：`sqlite_master` 9 张表（`_cf_KV` / `d1_migrations` / `devices` / `email_verifications` / `mail_outbox` / `refresh_tokens` / `sqlite_sequence` / `sync_items` / `users`）；
  冒烟后计数 `users=1 devices=0 verifications=1 outbox=1` ⇒ 验证码进了 `mail_outbox`（`MAIL_MODE="outbox"` 的预期行为）。
- **冒烟数据已清空**：按 `smoke-%@aiec.fun` 删 `email_verifications` / `mail_outbox`（按 `to_addr`）/ `devices` / `refresh_tokens` / `sync_items` / `users` 共 6 条，
  复核 `users=0 verifications=0 outbox=0` ⇒ 生产库回到干净待测状态。

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

> ✅ **前提已实测坐实（2026-11-10）**：用 OAuth 凭据查 `GET /zones?name=aiec.fun` ⇒
> `success=true`、`zones=1`、`status=active`、`zone id = f686751b049e4270d1a4a1bcb4849852`，
> 且与账号 `016c8e736b685af88860034ea6eb1daa` 同属一个 Cloudflare 账号
> ⇒ **§3.3 的手工 CNAME 兜底不需要走**，`wrangler deploy` 会自动建 DNS。
> （仍需注意：OAuth 令牌的 scope 里有 `zone (read)` 但**没有** DNS 写权限；
> 真到部署那一步若报 DNS 权限不足，就走 §1.3 的 API Token —— 那里带 `Zone > DNS > Edit`。）

> ⚠️ **不要**打开 `workers_dev` / `preview_urls`。T103 硬约束：境内 DNS 会污染平台默认子域，端点必须挂在自有域名下；`cloud/fluxing-sync/test/check-endpoint-policy.mjs` 是**零豁免**扫描，连文档措辞都算。

### 3.2 部署与验证（🟢 **已完成 2026-11-11**）

```
cd F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler deploy
```

**实测留痕（2026-11-11）**：

```
Total Upload: 42.65 KiB / gzip: 11.45 KiB
Uploaded fluxing-sync (3.65 sec)
Deployed fluxing-sync triggers (2.62 sec)
  api.aiec.fun (custom domain)
Current Version ID: c22a483d-0460-4d17-b46b-ac3347c2c138
DEPLOY_EXIT=0
```

绑定全部解析成功：`env.DB (fluxing-sync)` · `env.BUCKET (fluxing-sync)` · `ENVIRONMENT=dev` ·
`ACCESS_TOKEN_TTL=3600` · `REFRESH_TOKEN_TTL=2592000` · `KDF_ALGO=PBKDF2-SHA256` · `KDF_ITERS=600000` ·
`MAIL_MODE=outbox` · `MAIL_FROM=no-reply@aiec.fun`。

**判据（两条都要跑）**：

```
curl -sS -o NUL -w "%{http_code}" https://api.aiec.fun/v1/health
```

期望 **200**。**实测结果**：

```
GET https://api.aiec.fun/v1/health ⇒ HTTP 200
{"ok":true,"service":"fluxing-sync","environment":"dev","time":1794319132}
```

> ⇒ **线上判据已 verified**。`custom_domain = true` 确实自动建了 `api.aiec.fun` 的 DNS 记录，
> §3.3 的手工 CNAME 兜底**不需要走**（此前担心的「OAuth 无 DNS 写权限」未成为阻塞 —— 自定义域由 Workers 侧接管）。

- **522 / 523** ⇒ DNS 或 Worker 路由没绑上（多为「`aiec.fun` 不在同一账号」，或还没部署）。
- **404** ⇒ Worker 起了但路由没匹配（检查 `routes` 行是否生效、`wrangler.toml` 有没有被改坏）。

再从**另一台机器**跑同一条命令，验证不是本机缓存骗人。（**未做** —— 需第二台设备，见 §8。）

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
7. **`api.aiec.fun` 从第二台机器 / 另一网络的可达性** —— 只在本机 PowerShell 7 上打到 200，**未做异地验证**（§3.2 要求的「另一台机器」判据未执行）。这关系到境内多端同步的实际可用性。
8. **R2 密文对象的真实读写** —— 桶已建、绑定已解析，但**尚未跑过一次真实的 `PUT`/`GET`**（客户端还没联调）。「绑定存在」不等于「对象能写」。
