# fluxing-sync · 火流猩输入法云同步服务端

spec 100 阶段 C（T211–T215）的产物：Cloudflare Workers + D1 + R2 的零知识同步后端。

- **接口契约**：`.specify/specs/100-fluxing-cloud-account-sync/plan.md` §6.1（端点）/ §6.2（D1 表）/ §6.3（R2 布局）
- **线格式基准**：`include/FluxingCloudEnvelope.h`（commit `e395bda`）的 8 行文本格式 —— `v1 / item_key / type / hlc_ms / hlc_counter / device / nonce / cipher`
- **架构决策**：`docs/adr/0039-cloud-account-sync-architecture.md`
- **取证结论**：`docs/research/fluxing-cloud-2026-11/`（合并语义 / 端点可达性 / 发信通道）

服务端**只见密文**：`cipher` 原样落 R2，不解密、不解析信封文本、不比较明文。它拿到的唯一
敏感信息是 `AUTH_PEPPER`——用来把客户端派生的 `auth_key` 折成 verifier，永不回传。

## 目录

| 路径 | 作用 |
|---|---|
| `src/index.ts` | 路由（9 条契约端点 + `GET /v1/health`）与统一错误面 |
| `src/auth.ts` | 注册 / 邮箱验证 / 登录 / 续期 / Bearer 鉴权 |
| `src/sync.ts` | 推送与增量拉取，`server_seq` 语义 |
| `src/devices.ts` | 设备列表与撤销 |
| `src/account.ts` | 清空云端数据（不删账号） |
| `src/wire.ts` | 信封字段校验；常量与头文件逐字对齐 |
| `src/crypto.ts` | SHA-256 / HMAC / 随机数 / base64 工具 |
| `src/mail.ts` | 验证码发信（`outbox` 与 `cloudflare` 两种模式） |
| `src/errors.ts` | `ApiError` + `{ error: { code, message } }` 统一响应 |
| `migrations/0001_init.sql` | D1 初始结构（含对 §6.2 的 6 条偏差说明） |
| `test/api.test.ts` | workerd 内跑的端到端接口测试 |
| `test/check-endpoint-policy.mjs` | 端点形态门禁（禁止 Cloudflare 默认子域，见 T103） |

## 本地开发

前置：Node ≥ 20、npm ≥ 10。首次：

```powershell
cd cloud\fluxing-sync
npm install
Copy-Item .dev.vars.example .dev.vars   # 然后把 AUTH_PEPPER 换成自己的随机串
npm run migrate:local                  # 建本地 D1（.wrangler/state）
npm run dev                            # http://127.0.0.1:8787
```

发信默认 `MAIL_MODE=outbox`，验证码只写进本地 D1 的 `mail_outbox` 表，不发真信。取码：

```powershell
npx wrangler d1 execute fluxing-sync --local --command "SELECT to_addr, body FROM mail_outbox ORDER BY id DESC LIMIT 1"
```

## 测试与门禁

```powershell
npm test           # vitest-pool-workers：测试跑在真实 workerd 里，D1/R2/crypto 均为真实实现
npm run typecheck  # tsc --noEmit（只覆盖 src/**，测试目录靠运行时断言兜底）
npm run check      # check:endpoint + test，提交前跑这个
```

要点：`isolatedStorage` 默认开启，每个 `it()` 拿到独立存储 ⇒ **多步流程必须写在同一个 `it()` 里**。

工具链事实（2026-11-10 实测，踩过就别再踩）：

- 装在 vitest 4 分支上的 `@cloudflare/vitest-pool-workers@0.23` **没有 `./config` 子路径导出**，
  `defineWorkersConfig` 已被移除；正确写法是在 `defineConfig({ plugins: [cloudflareTest({...})] })` 里传插件，
  且 `singleWorker` 选项随之取消（`dist/pool/index.d.mts` 的 zod schema 里已无该键，传了会被静默 strip）。
- `compatibility_date` **不得晚于本地 workerd 二进制支持的最新日期**（本机实测上限 `2026-08-22`），
  否则 workerd 起不来，报 `This Worker requires compatibility date ..., but the newest date supported by this server binary is ...`。
  现值 `2026-08-01`，留了余量。
- 启动时会有 `@cloudflare/vitest-pool-workers has been renamed to @cloudflare/vitest-plugin` 警告 —— 只是改名提示，
  当前版本功能正常；迁移是独立的技改，不要混进业务改动里做。
- 该包**只在 vitest 4 上工作**：`vitest@^4.1.0` 与 `@cloudflare/workers-types@^5.x` 是硬约束，
  低版本会在 `npm install` 阶段 `ERESOLVE`。

## 部署

```powershell
# 1) 建资源（一次性）
npx wrangler d1 create fluxing-sync
npx wrangler r2 bucket create fluxing-sync

# 2) 把 d1 create 输出的 database_id 填进 wrangler.toml 的 [[d1_databases]]
#    同时把 <API_DOMAIN> 换成自有域名（见下「端点形态」）

# 3) 配秘密（不进代码库、不进 wrangler.toml）
npx wrangler secret put AUTH_PEPPER

# 4) 建表 + 发布
npm run migrate:remote
npm run deploy
```

### 端点形态（T103 硬约束）

实测结论见 `docs/research/fluxing-cloud-2026-11/endpoint-reachability.md`：**境内 DNS 污染导致
Cloudflare 默认子域（`workers_dev` 那个域）不可用**，因此：

- `wrangler.toml` 固定 `workers_dev = false` 与 `preview_urls = false`；
- 对外只走 `routes` 里的自有域名（形态必须是 `https://api.<我方域名>`）；
- `npm run check:endpoint` 会递归扫本目录，命中默认子域字样即失败（本文档也不例外）。

> 当前阻塞：自有域名尚未注册（`fluxing.*` 无 A 记录），`wrangler.toml` 里是 `<API_DOMAIN>` 占位，
> 因此本模块只完成了「可构建 + 可测试」，真实部署未执行。
>
> 已实测（2026-11-10，本机）：`npm run check` 退出码 0 —— 端点门禁通过 + `test/api.test.ts` **20/20 PASS**（workerd 真实运行）；
> `npm run typecheck` 通过。**未验证**：`wrangler deploy`、自定义域路由、`MAIL_MODE=cloudflare` 真实发信。

### 发信通道

`MAIL_MODE=cloudflare` 走 Workers `send_email` 绑定（`wrangler.toml` 里 `[[send_email]]` 目前注释掉）。
结论见 `docs/research/fluxing-cloud-2026-11/email-verify-channel.md`：**Workers Free 档不能出站发送**，
首版必须 Paid 计划（3,000 封/月）。未开通前验证码链路只能在 `outbox` 模式取证。

## 与 plan §6.2 的刻意偏差

| 编号 | 偏差 | 理由 |
|---|---|---|
| D1 | `refresh_tokens` 增加 `kind`（`access`/`refresh`） | access token 也「不透明随机串 + 只存哈希」，撤销设备 / 清空即失效；省掉 JWT 与签名密钥 |
| D2 | 新增 `email_verifications` 表 | §6.2 未覆盖验证码的过期 / 重发节流 / 错误次数上限 |
| D3 | `sync_items.id` 用 `AUTOINCREMENT` 充当 `server_seq` | 保证序号不复用，客户端的 `since` 增量拉才不漏推 |
| D4 | `sync_items` 增加 `nonce_b64` | nonce 属信封头，放 D1 让 pull 有权威来源（R2 元数据是尽力而为） |
| D5 | 新增 `mail_outbox` 表 | 只服务 `MAIL_MODE=outbox`；生产不写不读，故不存在「取验证码后门」 |
| D6 | `devices` 加 `UNIQUE(user_id, device_id)` | 同一台机器重复登录只留一行 |

另有两处 additive 字段（不改既有字段名，客户端可忽略）：

- `POST /v1/auth/login` 多返回 `email_verified`：否则未验证状态下推送被 403 拦住而没有可解释来源。
- `GET /v1/devices` 列表项多返回 `id` / `revoked` / `current`：§6.1 原字段里没有可传给
  `DELETE /v1/devices/:id` 的句柄；`DELETE` 的路径参数同时接受 `devices.id` 与 `device_id`。

## 安全不变量

1. 服务端永不见明文口令与 `enc_key`；落库的是 `verifier = base64(HMAC-SHA256(AUTH_PEPPER, auth_key))`。
2. `AUTH_PEPPER` 缺失或短于 16 字符 ⇒ 一律 500 fail closed，绝不回退内置默认值。
3. 令牌只存 `base64(SHA-256(token))`；撤销设备即把该设备全部令牌置 `revoked_at`。
4. 未验证邮箱只拦写（push 403），不拦读与设备管理。
5. 写序 R2 先、D1 后：失败只留「孤儿对象」，绝不出现「D1 有索引、R2 没内容」。
6. push 单条非法即整批 400（不做部分接受）；`server_seq` 取本次涉及条目的 `MAX(id)`，
   **不用** `meta.last_row_id`（连接级游标会越过客户端未见过的行号 ⇒ 永久漏推）。
7. pull 遇 R2 对象缺失时自愈（删索引行 + 告警 + 跳过），不返回 500——否则客户端会卡在同一游标无限重试。

## 已知限制（首版不做）

- 服务端每日推 100 / 拉 50 的配额**未强制**（现由客户端调度自律）。
- 无 Web 管理后台、无口令找回、无「重置账号」流程（`DELETE /v1/account/data` 只清数据）。
- 无 CORS：客户端是原生进程，不存在浏览器源。
- `POST /v1/auth/register` 在 60 秒内重复提交同一邮箱返回 **429**（重发节流先于查重），不是 409。