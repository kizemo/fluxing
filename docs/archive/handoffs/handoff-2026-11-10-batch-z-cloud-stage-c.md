# Handoff · 批次 Z · spec 100 阶段 C（服务端 Cloudflare，T211–T215）

> 生成：2026-11-10 · 分支 `feat/100-cloud-sync` · 本批提交 `5495d52`（23 files / +6611）

spec 100 **阶段 C 已全部完成（T211 / T212 / T213 / T214 / T215 五任务 DONE）**：在 `cloud/fluxing-sync/` 落下 Cloudflare Workers + D1 + R2 的 TypeScript 服务端，`plan.md` §6.1 的 **9 条契约端点全部实现**（另加 `GET /v1/health` 供巡活，共 10 条路由），`plan.md` §6.2 的 D1 表按 **7 表 4 索引** 落地（多出的 3 张与 6 条刻意偏差见 §3.3），`plan.md` §6.3 的 R2 对象键 `u/<account_id>/<sha256(item_key)>` 照实现。**测试与门禁全部实测通过（verified）**：`npm run check` 退出码 0，workerd 真实运行 **20/20 用例 PASS**，`npm run typecheck` 通过，并做了**变异验证**证明用例有牙（改掉一个错误码即 1 failed / 19 passed，还原后与原文件逐字节一致并复跑全绿）。**未验证的硬边界**：`wrangler deploy`、自定义域路由、`MAIL_MODE=cloudflare` 真实发信 —— 三者都依赖外部资源（自有域名 A 记录、Workers Paid 计划），当前无一就绪，属**非代码原因**，见 §7。

阶段 C 的红线是 **T103「不得硬编码 `*.workers.dev`」** 与 **零知识（服务端永不见明文口令与 `enc_key`）**。前者用「`workers_dev` / `preview_urls` 双关 + `test/check-endpoint-policy.mjs` 递归扫全目录字面量」落实，门禁**零豁免**、文档自身也不许写默认子域字样；后者用 `verifier = HMAC-SHA256(AUTH_PEPPER, auth_key)` + 「服务端不解析信封、只按字段存取密文」落实。

**下一批可立刻开工**：阶段 D（客户端运行时，T301 起）。它与本批无文件冲突面（本批全在 `cloud/fluxing-sync/`，阶段 D 全在 `WeaselServer/Cloud/`），且**不依赖**上面两个外部资源就能完成纯逻辑与 mock 传输部分（inferred：T301 的 CNG 加解密、T302 的 `ICloudTransport` 抽象、T303 的 Credential Manager 都是本机能力）。真正需要真实端点的只有「部署联调」与「真实发信」两条线。

---

## 1. 必读文档（绝对路径）

| # | 路径 | 读什么 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-10-batch-z-cloud-stage-c.md` | **本文**。批次 Z 状态、契约偏差、工具坑 |
| 2 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\spec.md` | 需求与验收判据（US1–US12） |
| 3 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\plan.md` | **§6.1 接口 / §6.2 D1 表 / §6.3 R2 布局**（本批唯一契约来源）；§3.1 密钥派生（阶段 D 用）；`:181` = `DELETE` 语义 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` | 阶段 C 第 102–139 行（T211–T215）；阶段 D 第 143 行起 |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\next-phase-plan.md` | 执行序与 D1–D5 决策；阶段 E 被 D1 阻塞的推荐解法 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\adr\0039-cloud-account-sync-architecture.md` | 账户 / 设备 / 密钥 / 同步边界 |
| 7 | `F:\soft\00selfmade\rime_claude\include\FluxingCloudEnvelope.h` | 客户端契约基准（commit `e395bda`，1020 行）；阶段 D 的 AAD 与信封格式以它为准 |
| 8 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\README.md` | 端点清单、6 条 D1 偏差表、**工具链事实（本轮实测）**、未验证项 |
| 9 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\migrations\0001_init.sql` | 表结构真相源；文件头注释记 D1–D6 偏差 |
| 10 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\{userdict-merge-semantics,endpoint-reachability,email-verify-channel}.md` | 三份硬约束取证结论，**不要重新实测覆盖** |

---

## 2. 阶段 C 逐任务进度

| 任务 | 状态 | 交付 | 判据达成情况 |
|---|---|---|---|
| **T211** · Worker 骨架与 D1 / R2 初始化 | **DONE 2026-11-10** | `wrangler.toml` / `migrations/0001_init.sql` / `src/index.ts`（+ `env.ts` / `errors.ts` / `wire.ts`） | **判据换载体**：tasks.md 要的是「`wrangler dev` 启动日志 + `/v1/ping` 请求响应」。实做用 **vitest-pool-workers（真 workerd）** 取证，且探活路径定名 **`GET /v1/health`** 而非 `/v1/ping`（verified：`src/index.ts:51`）。理由见 §3.1（A2 / A3） |
| **T212** · 认证接口 | **DONE 2026-11-10** | `src/auth.ts`（483 行）/ `src/mail.ts` / `src/crypto.ts` | 四条端点齐；`verifier` 用 `AUTH_PEPPER`（Workers Secret，缺失即 500 fail closed，**绝不回退内置默认值**）；refresh 轮换只存哈希 ⇒ 用例「旧的立即失效，新 access 可用」PASS（verified） |
| **T213** · 同步推拉接口 | **DONE 2026-11-10** | `src/sync.ts`（241 行）/ `src/wire.ts`（取值域校验） | 推 3 条 → 拉回 3 条、`since` 增量、`cursor` 分页、重推单调前进、单条非法整批 400 —— 4 条用例全 PASS（verified）。**R2 密文抽查为间接证据**：`pull` 的 `cipher` 与推送逐字节相等（`test/api.test.ts:287`）+ pull 内容取自 R2 ⇒ R2 存的就是密文；**未写**一条直接 `env.SYNC_BUCKET.get()` 的断言（not verified） |
| **T214** · 设备与账号端点 | **DONE 2026-11-10** | `src/devices.ts` / `src/account.ts` | 「撤销设备 → 列表 `revoked` + 该设备令牌立即失效」PASS；「清空云端数据 → 只清同步内容，账号仍可登录」PASS（verified）。判据要的「清空后 R2 前缀为空」由同一用例内的 pull 返回 0 条间接证明；**未直接 list R2 前缀**（not verified） |
| **T215** · 服务端集成测试 | **DONE 2026-11-10** | `test/api.test.ts`（529 行 / 20 用例）+ `test/apply-migrations.ts` + `test/check-endpoint-policy.mjs` + `vitest.config.ts` | `20 passed (20)` / `TEST FILES 1 passed`（verified）。**覆盖度与 tasks.md 清单的差值**：tasks.md 点名「越权取他人条目」，本轮以「统一错误面 5 例 + 令牌形状校验」覆盖鉴权面，**没有**写「A 用户拿 B 用户 item」的定向用例（not verified，属阶段 G 加固项） |

阶段 C **无 in-flight、无 pending**。阶段 D 起为 PENDING。

---

## 3. 契约与实现的对齐结论（阶段 D 必须照这个接）

### 3.1 三处对 tasks.md 字面的刻意调整

| 编号 | tasks.md 原字面 | 实做 | 理由 |
|---|---|---|---|
| A1 | T211 建 **4 张表** | **7 表 4 索引** | §6.2 的 4 张表不足以支撑「邮箱验证码」「令牌轮换」「outbox 发信」；多出 `email_verifications` / `mail_outbox`，并把 `refresh_tokens` 一表两用（`kind` = `access` / `refresh`） |
| A2 | T211 判据贴 **`wrangler dev` 日志** | 用 **vitest-pool-workers** 在真 workerd 里跑 | `wrangler dev` 需交互式进程 + 端口 + 手工 curl，产物不可复跑；pool-workers 给的是**同一条运行时 + 可重跑断言**，证据强度更高。代价：**没有** `wrangler dev` 启动日志（not verified） |
| A3 | T211 探活路径 **`/v1/ping`** | **`GET /v1/health`** | `plan.md` §6.1 端点表里没有 `/v1/ping`；自造第二个探活名会让「9 条契约端点」变成 10 条契约面。现约定：**9 条契约端点 + 1 条运维 `/v1/health`**，已在 `README.md` 与 `src/index.ts:3` 写明 |

### 3.2 阶段 D 客户端必须对齐的三条线上格式

1. **`hlc` 规范形态 = 嵌套对象 `{ms, counter, device}`**。输入侧容忍扁平别名（`hlc_ms` / `hlc_counter` / `device`）以兼容旧冒烟脚本，**输出只发嵌套形态**（verified：`test/api.test.ts:388` 用扁平输入、`:270` 断言嵌套输出）。**这条由服务端定义，阶段 D 的 `CloudCrypto` / 信封序列化必须照嵌套写**，否则 AAD 与 `FluxingCloudEnvelope.h` 的 HLC 文本行对不上。
2. **`POST /v1/auth/login` 响应含 `email_verified`（布尔）**——对 §6.1 的 **additive** 扩展。语义：未验证账号**可以登录、可以 pull、可以看设备**，**只有 push 被 403 `email_not_verified`**。阶段 D 的账号页必须消费该字段来区分「需验证邮箱」与「密码错」。
3. **`DELETE /v1/devices/:id` 的 `:id` 同时接受 `devices.id` 与 `devices.device_id`**。阶段 D 传哪个都行，但**传 `device_id` 更稳**（`devices.id` 是库内自增，换库会变）。

### 3.3 对 `plan.md` §6.2 的 6 条 D1 偏差（已写进 `0001_init.sql` 文件头与 README 表）

| 编号 | 偏差 | 理由 |
|---|---|---|
| D1 | `refresh_tokens` 加 `kind` 列（`access` / `refresh`） | access token 也走「不透明随机串 + 只存哈希」⇒ 换「无签名密钥 / 无 JWT 库」+ **撤销设备立即失效** |
| D2 | 新增 `email_verifications` 表 | T101 决定首版必须邮箱验证码；`users` 里只留 `email_verified` 标志 |
| D3 | `sync_items.id` AUTOINCREMENT 充当 `server_seq` | 免自建序列表；`server_seq` 取「本次推送涉及 item_key 集合上的 `MAX(id)`」，**明确弃用 `meta.last_row_id`**（并发下会串号） |
| D4 | `sync_items` 加 `nonce_b64` 列 | GCM nonce 必须随密文一起回来，否则阶段 D 解不开 |
| D5 | 新增 `mail_outbox` 表，**仅 `MAIL_MODE=outbox` 时写** | Workers Free 档不能出站；outbox 让「验证码链路」在无 Paid 计划时可取证 |
| D6 | `devices` 加 `UNIQUE(user_id, device_id)` | 同一设备重复注册应更新而非插新行 |

### 3.4 其余已定死的取舍（阶段 D 照此假设）

- **写序 R2 先、D1 后** —— 避免「有索引无内容」。**pull 遇 R2 对象缺失采取自愈**（删索引行 + `console.warn` + 跳过），不返回 500。
- **push 单条非法即整批 400，不写半条**（verified 用例：「推送校验：任何一条非法都整批 400，不写半条」）。
- **`cipher` 有最小长度校验**（至少装得下 GCM 认证标签，8 字节被 400 拒，`test/api.test.ts:364`）。
- **`DELETE /v1/account/data` 只清 R2 前缀 + `sync_items`**，保留 `users` / `devices` / `refresh_tokens` / `email_verifications`（`plan.md:181` = `{ok}`，spec.md FR-014 双证）。
- **`handleRegister` 里节流先于查重** ⇒ 同邮箱 60 秒内二次注册得 **429 `verify_too_frequent` 而非 409 `email_taken`**。行为已在测试名与 README 如实记录，**不改实现**（节流优先可防枚举）。
- **路由不设 `Allow` 头、不做 CORS**（原生客户端，无浏览器源）；成功响应带 `no-store` + `nosniff`（有用例）。
- **服务端不解析信封 8 行文本**，只按 §6.1 JSON 字段存取；对齐点是**字段名 + 取值域校验**，不是文本解析。
- **不做**：服务端解密、Web 管理后台、口令找回、`WeaselDeployer` 账号入口、托盘图标入口、服务端每日推/拉配额强制。

---

## 4. 下一阶段计划（按此序）

| 序 | 任务 | 为什么在这个位置 |
|---|---|---|
| 1 | **T301 · CNG 加密模块**（`WeaselServer/Cloud/CloudCrypto.*`） | 阶段 D 的地基，且**完全不依赖网络与已部署服务端**：PBKDF2 → HKDF → AES-256-GCM 全在本机。AAD 必须取 `FluxingCloudEnvelope.h` 的 `item_key\|type\|hlc`（§3.2 第 1 条的嵌套形态） |
| 2 | **T302 · WinHTTP 传输模块**（`CloudTransport.*`，`ICloudTransport` 可 mock） | 与 T301 无文件冲突；先把接口抽出，后续 T303/T304 才能在**无真实端点**下用 mock transport 测 |
| 3 | **T303 · 凭据存储**（Credential Manager） | 依赖 T301（要存 `enc_key`）；「读失败 ⇒ 回到未登录而非崩溃」是**必须现在定的语义**，晚做会让 T304 状态机返工 |
| 4 | **T304 · 账户客户端**（状态机 `未登录 → 登录中 → 已登录 → 需重新登录`） | 依赖 T301/T302/T303。此时可用 mock transport 全链路自测，**不需要真实端点** |
| 5 | **T305 · 词库 / 短语导出导入协调** | 依赖 T102（条目化格式）+ T303；要挂既有维护态机制，风险最高，放最后 |

顺序的硬理由：**1–4 全部可在「无域名、无 Paid 计划」下完成并取证**，所以先把它们做完；真正被外部资源卡住的只有 §7 的三项。

**跨批的并行选项**：阶段 E 的 T401（账号页）依赖 T304；`next-phase-plan.md` 记明阶段 E 被 D1 决策阻塞，推荐解法是「快捷面板第 4 槽重新成为『账号』入口」。但**当前快捷面板是 4 槽且已摘掉两个键帽（批次 U）**，所以 T401 开工前要用户确认第 4 槽归属（见 §8）。

---

## 5. 证据路径

| 证据 | 路径 / 命令 | 结果 |
|---|---|---|
| 提交 | `git show --stat 5495d52` | 23 files changed，6611 insertions，无 `node_modules` |
| 端点门禁（T103） | `cd cloud\fluxing-sync; node test/check-endpoint-policy.mjs` | `[T103 红线] 通过：目录内无默认子域字面量，workers_dev / preview_urls 均已关闭` ⇒ `ENDPOINT_EXIT=0`（verified） |
| 集成测试 | `cd cloud\fluxing-sync; npm test` | `Test Files 1 passed (1)` / `Tests 20 passed (20)` ⇒ `VITEST_EXIT=0`（verified） |
| 合并门禁 | `cd cloud\fluxing-sync; npm run check`（= `check:endpoint` + `vitest`） | `CHECK_EXIT=0`（verified） |
| 用例清单 | `npx vitest run --reporter=verbose` → `tmp\cloud-stage-c-vitest-verbose.log` | 20 条用例名（health 2 / 认证 6 / 同步 4 / 设备与账号 3 / 统一错误面 5） |
| 类型检查 | `cd cloud\fluxing-sync; npm run typecheck`（`tsc --noEmit`） | 无输出即通过（verified；**只覆盖 `src/**`**，见 §6 坑 5） |
| **变异验证** | 把 `src/sync.ts:87` 的 `email_not_verified` 改成 `email_not_verified_MUTANT` | `MUTANT_EXIT=1`、`Tests 1 failed \| 19 passed (20)`；写回后 `RESTORED_BYTE_IDENTICAL=True`、`CLEAN_EXIT=0`、`Tests 20 passed (20)`（verified —— 证明用例有牙，不是空转断言） |
| 提交信息原文 | `tmp\cloud-stage-c-commit-msg.txt`（1308 B，UTF-8 无 BOM） | 与 `5495d52` 实际信息一致 |

阶段 B 单测口径（本轮**未重跑**，基线来自批次 Y）：`tmp\build-fluxing-cloud-tests.cmd` → 判据 `TEST_FLUXINGCLOUD_EXIT`，基线 `RESULT: PASS=134 FAIL=0`。本批**未触碰** `include/FluxingCloudEnvelope.h`（verified：`5495d52` 的文件清单里没有它）。

---

## 6. 环境事实与工具坑（从仓库里推不出来的）

1. **`@cloudflare/vitest-pool-workers@0.23.0` 没有 `./config` 子路径导出**。`import { defineWorkersConfig } from '@cloudflare/vitest-pool-workers/config'` 必报 `Missing "./config" specifier`。正确写法（0.23 起）：
   ```ts
   import { cloudflareTest, readD1Migrations } from '@cloudflare/vitest-pool-workers';
   import { defineConfig } from 'vitest/config';
   export default defineConfig({
     plugins: [cloudflareTest(async () => ({ wrangler: { configPath: './wrangler.toml' }, miniflare: { bindings: { /* … */ } } })) ],
     test: { setupFiles: ['./test/apply-migrations.ts'] },
   });
   ```
   依据是包内 `dist/codemods/vitest-v3-to-v4.mjs`（从旧写法到新写法的自动改写脚本）。
2. **`test.poolOptions.workers.singleWorker` 在 0.23 已不存在**，传了会被 zod `$strip` **静默丢弃**——留着就是假配置，已删。
3. **`compatibility_date` 有本机上限**。`2026-11-01` ⇒ workerd 拒绝启动：`newest date supported by this server binary is "2026-08-22"`。现值 **`2026-08-01`**（留余量，不贴上限）。**换机器 / 升级 workerd 后可上调**。
4. **端点门禁会扫到文档自身**。首次跑退出码 1，命中自家 `README.md:28/79/83`（在解释规则时写了默认子域字面量）。取舍：**门禁零豁免**，改文档措辞而非给门禁加白名单（白名单是最容易被下一个人绕过的口子）。⇒ 以后在这个目录里写任何解释性文字，**都不许出现默认子域字样**。
5. **`tsconfig.json` 只 `include` `src/**/*.ts`**：`test/` 不进 `npm run typecheck`。理由是 `cloudflare:test` 模块的类型声明来源不确定，纳进去会造**假红**；测试走 esbuild 转译 + 运行时断言。⇒ 别把 `test/` 加进 include「顺手补全」。
6. **`@cloudflare/vitest-pool-workers has been renamed to @cloudflare/vitest-plugin`** 只是一条**改名警告**，当前版本功能正常。迁移是独立技改，**不要混进业务提交**。
7. **版本硬约束**：`vitest@^4.1.0` + `@cloudflare/workers-types@^5.x`，低版本 `npm install` 直接 `ERESOLVE`。
8. **本机工具链（实测）**：node `v26.13.0`、npm `11.20.0`、registry `https://registry.npmmirror.com`；wrangler `4.159.0`、vitest `4.1.12`、`@cloudflare/vitest-pool-workers` `0.23.0`、`@cloudflare/workerd-windows-64` 已就位 ⇒ **workerd 能在本机启动，不需要 Node 侧 `node:sqlite` 兜底方案**（本批曾把它列为兜底，已证伪）。
9. **`git commit -F -` 不读 stdin**（`-F` 只接受文件名）。长提交信息一律先写文件再 `git commit -F <file>`；本批用的是 `tmp\cloud-stage-c-commit-msg.txt`。
10. **一次 `edit` 调用只能传一组 `old_string`/`new_string`**；JSON 里重复键会被静默丢弃，返回的 `1 block(s)` 不代表 N 处都改了。
11. **`Select-String -Path` 多路径中任一缺失则整条失败**；**PowerShell 单行里混用 `'` 与 `"` 的正则容易报「字符串缺少终止符」** ⇒ 优先用 `grep` 专用工具，别在 `bash` 里拼正则。
12. **安全策略拦「先 `Set-Location` 改工作目录、再对旧路径写/删」** ⇒ 用 `git -C <repo>` 或全路径参数式调用。
13. **既有脏文件（三个，本批刻意未提交）**：`RimeWithWeasel/RimeWithWeasel.cpp`（假脏，115+/3-）、`librime` 子模块（假脏，`m` 态）、`docs/design/quickpanel-v6/design-demos/{a-benchmark,b-craft,c-roulette}.html`（三份未跟踪设计稿）。**下一批提交前照样排除**。
14. **`web_search` 返回 HTTP 402**（套餐余额耗尽），本轮只能 `web_fetch` 抓厂商文档镜像。若下批要查 Cloudflare 文档，直接用 `web_fetch` 抓 `developers.cloudflare.com` 对应页。

---

## 7. verified / inferred / not verified 三分账

**verified（本机实跑）**
- 9 条契约端点 + `/v1/health` 的 10 条路由在真 workerd 里跑通，20/20 用例 PASS，`npm run check` 退出码 0。
- 端点门禁零豁免生效（含扫到文档自身的第一次红）。
- 变异验证证明用例有牙；还原后逐字节一致。
- `tsc --noEmit` 通过（限 `src/**`）。
- 「服务端只见密文」：`pull` 的 `cipher` 与 `push` 逐字节相等，且 pull 内容取自 R2。
- 「未验证邮箱只能读不能写」：`push` 被 403 `email_not_verified`，`pull` / 设备列表不受影响。
- 「撤销设备 → 令牌立即失效」；「清空数据 → 账号仍可登录、同步内容归零」。
- 三个既有脏文件未被卷进提交（`5495d52` 清单 23 个文件全在 `cloud/fluxing-sync/`）。

**inferred（有依据但未实测）**
- 真 D1 / R2 与本地 miniflare 的 D1 / R2 行为一致（在 SQLite 语义、R2 put/get 一致性上）。
- 自定义域路由配好后端点可达（依据 `endpoint-reachability.md` 的 T103 结论；**达成与否取决于域名是否解析**）。
- 阶段 D 把 `hlc` 写成嵌套形态即可与 `FluxingCloudEnvelope.h` 对上（服务端侧已按嵌套实现并有用例，客户端侧未写）。

**not verified（没跑 / 跑不了）**
- `wrangler deploy` —— **未执行**。自有域名（`api.<我方域名>`）无 A 记录，`wrangler.toml` 仍留 `<API_DOMAIN>` 占位。
- 自定义域路由与 TLS —— 未配置。
- `MAIL_MODE=cloudflare` 真实发信 —— **未执行**。Workers Paid 未开通（T101 结论：Free 档不能出站，首版必须 Paid / 3,000 封每月）。邮箱验证链路只在 `MAIL_MODE=outbox` 下取证。
- `wrangler dev` 启动日志 + curl 请求响应 —— 未采集（判据换了载体，见 §3.1 A2）。
- 直接 `env.SYNC_BUCKET.get()` / `list()` 的 R2 内容与前缀断言 —— 未写（现有证据是间接的，见 §2 T213/T214 行）。
- 「A 用户拿 B 用户 item」的定向越权用例 —— 未写（tasks.md T215 点名项，列为阶段 G 加固）。
- 主解决方案全量构建 —— 本批只改 TypeScript，**未跑** `xbuild.bat`。inferred 不受影响（`cloud/` 不在任何 C++ 构建输入里）。
- 阶段 B 单测 —— 本轮未重跑（基线 `PASS=134 FAIL=0` 来自批次 Y；本批未改 `FluxingCloudEnvelope.h`）。

---

## 8. 需要用户（人）在场才能推进的事项

1. **自有域名解析**：给 `api.<我方域名>` 加 A / CNAME 记录，并把 `wrangler.toml` 里的 `<API_DOMAIN>` 换成真域名。没有这一步，`wrangler deploy` 与自定义域路由**一步都走不了**。
2. **Workers Paid 计划**：开通后才能验 `MAIL_MODE=cloudflare` 真实发信（T101 硬约束）。在此之前「邮箱验证码」只能在 outbox 模式下自证。
3. **Cloudflare 账号授权**：`wrangler login`（或 `CLOUDFLARE_API_TOKEN` 环境变量）需要用户本人在浏览器里完成 OAuth —— 自动化无法代做。
4. **阶段 E 第 4 槽归属**：`next-phase-plan.md` 推荐把快捷面板第 4 槽改回「账号」入口，但批次 U 已把它做成 4 槽并摘掉两个键帽（「快捷键设置」「用户登录」）。**这属于改动用户可见界面结构 ⇒ 必须由用户拍板**，不是工程判断能替代的。
5. **`plan.md` / `tasks.md` 的偏差回写**：§3.1 的 A1–A3 与 §3.3 的 D1–D6 目前只落在代码注释与 README。是否要把它们回写进 `plan.md` §6.2（让文档与实现同源）属文档治理决策，**建议下批开工前用一次提交补齐**，但需要用户认可「以实现为准改文档」这个方向。

---

## 9. 与其他并行线的关系

- **与批次 Y（阶段 B）**：零冲突。批次 Y 是 `include/FluxingCloudEnvelope.h` + `test/TestFluxingCloud/`，本批是 `cloud/`，文件不相交。
- **与托盘 / 快捷面板线**：零冲突。批次 W（语言栏项去除）、批次 U（4 槽 + 图标几何）、批次 V（面板位置 + 主题）均未触碰。
- **与 IPC 协议面**：**未触碰** `include/WeaselIPCData.h`（verified：`5495d52` 文件清单）。故不触发「IPC 四边同步」硬规则。阶段 E 的 T402 明确「首版不加部署器账号入口」也正是为了守住这条。