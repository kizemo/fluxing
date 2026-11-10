# Handoff · 批次 AB · spec 100 阶段 D「客户端运行时」完成（T301–T305 全 DONE）

> 生成：2026-11-11 · 分支 `feat/100-cloud-sync`
> **文件名沿用批次 AA**（`handoff-2026-11-11-batch-aa-stage-d-not-started.md`）：按 AGENTS.md §9「同日同 spec ⇒ 就地改写既有文件、不另建重复文件」处理，**内容已整体替换为批次 AB**。
> 上一批：[批次 Z · 阶段 C（服务端）](handoff-2026-11-10-batch-z-cloud-stage-c.md) —— 提交 `5495d52` / `ab4de1a`

---

## 0. 本批一句话

**spec 100 阶段 D（T301–T305）五任务全部 DONE 并落盘**：新建 `WeaselServer/Cloud/`（**18 个文件**），T301 密钥派生 / T302 传输抽象 / T303 凭据存放 / T304 同步编排 / T305 账号页与词库快照通道全部实现；单测 **PASS=297 FAIL=0**（基线 134），`build.bat weasel` **0 错误**；文档侧按用户授权**回写** `plan.md` 9 处 + `tasks.md` 5 处、新建 **ADR 0054**、重写 `docs/ops/fluxing-cloud-external-prerequisites.md`、回填 `cloud/fluxing-sync/wrangler.toml` 的自定义域。

**尚缺一环**：Cloudflare 侧的人工动作（OAuth 授权 / D1 / R2 / secret / deploy / DNS）**全部未执行**，服务端仍未上线 ⇒ 阶段 D 的一切**线上**判据都是 not verified。

---

## 1. 必读文档（绝对路径）

| # | 路径 | 读什么 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-11-batch-aa-stage-d-not-started.md` | **本文**。批次 AB 全部记账 |
| 2 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md` | 阶段 D 第 **147 行起**（T301–T305，逐任务带「状态 / 实现偏差 / Done 判据」）；阶段 E 见其 T401 / T405 |
| 3 | `F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\plan.md` | §3.1 密钥派生 · §4.4 邮箱验证码（$0 路线）· §5 模块表 · §6.1 线上契约 · §6.2 D1 表（7 表 + D1–D6）· §9 变更面 · §13 验证策略 |
| 4 | `F:\soft\00selfmade\rime_claude\docs\adr\0054-client-key-derivation-and-salt-strategy.md` | **本批新建**。客户端密钥派生与 salt 策略的决策依据（自实现 PBKDF2/HKDF + 按邮箱确定性派生 salt） |
| 5 | `F:\soft\00selfmade\rime_claude\docs\ops\fluxing-cloud-external-prerequisites.md` | **本批整篇重写**。3 项待办 + 3 项已解除，每项含「谁做 · 命令 · 判据」；Cloudflare 人工动作的操作手册就在这里 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\free-tier-verdict.md` | 免费分档结论（Workers Paid = 账号级最低消费 $5/月；任意收件人发信需 Paid；Free 档存数据 + 跑端点可 $0） |
| 7 | `F:\soft\00selfmade\rime_claude\include\FluxingCloudEnvelope.h` | 契约基准（阶段 B 提交 `e395bda`）。**AAD 以它为准、不要改它**：`BuildAad` `:506` 5 字段；`AesGcmSeal` `:609` / `AesGcmOpen` `:676`；`MergeEntries` `:993` |
| 8 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\` | **本批新增目录**，18 个文件（见 §2.1） |
| 9 | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\` | 单测落点（追加 8 个测试函数 ⇒ 整包 297 用例） |
| 10 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-10-batch-z-cloud-stage-c.md` | 上批技术事实：§3.1 A1–A3 偏差 · §3.2 三条线上格式 · §3.3 D1–D6 偏差 · §7 verified/not verified 三分账 |
| 11 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\wrangler.toml` | 服务端配置（**`database_id` 仍是占位全零 UUID**、自定义域 `api.aiec.fun` 已回填） |

---

## 2. 阶段 D 逐任务进度（**全部 DONE 2026-11-11**）

### 2.1 新增文件清单（`WeaselServer/Cloud/`，**18 个**）

`CloudJson.{h,cpp}` · `CloudCrypto.{h,cpp}` · `CloudTransport.{h,cpp}` · `CloudCredentialStore.{h,cpp}` · `CloudAccount.{h,cpp}` · `CloudSyncService.{h,cpp}` · `CloudUserDictIo.{h,cpp}` · `CloudAccountDialog.{h,cpp}` = 8 个模块 × 2 = **16**，外加 `CloudRimeUserDictBackend.cpp` + `CloudMaintenanceHost.h` = **18**。

### 2.2 T301 · 密钥派生与信封加密 —— DONE

- **PBKDF2-HMAC-SHA256 60 万次** + **HKDF-SHA256** 分离 `auth_key` / `enc_key`（info = `fluxing-auth-v1` / `fluxing-enc-v1`），**全部自实现**（仓库零第三方依赖，宪法 P3）。
- **AES-256-GCM 仍复用 CNG**：`include/FluxingCloudEnvelope.h` 的 `crypto::AesGcmSeal/AesGcmOpen` + 5 字段 `BuildAad`（生产与单测同源，R7）。
- **salt 策略 = 按邮箱确定性派生**：`salt = SHA256("fluxing-enc-v1|" + lower(email))`；服务端**从不用 salt 校验**，客户端登录前拿不到服务端随机 salt ⇒ 只能确定性派生。决策见 **ADR 0054**。
- **HKDF 形态**：固定 32B 全零盐 + 单块 32B 输出（`PRK = HMAC(zeros32, ikm)`；`OKM = HMAC(PRK, info||0x01)`）；无适用 RFC 5869 官方向量 ⇒ 用公式复算 + .NET 独立实现双证。
- 口令缓冲用后清零；派生与登录路径**不写日志**。

### 2.3 T302 · 传输抽象 —— DONE

- `ICloudTransport` + `WinHttpTransport`（**WinHTTP，禁 libcurl**），**`BaseUrl` 可注入**（无域名也能单测）。
- 本 SDK 的 `WinHttpOpenRequest` 是 **7 参**形态（传满 7 参）。
- **禁自动认证但不关证书校验**：`WINHTTP_OPTION_DISABLE_FEATURE` + `WINHTTP_DISABLE_AUTHENTICATION`。
- `workers_dev = false` / `preview_urls = false` 保持；`cloud/fluxing-sync/test/check-endpoint-policy.mjs` **零豁免**扫描（只扫 `cloud/fluxing-sync/`，不扫文档）。

### 2.4 T303 · 凭据存放 —— DONE

- Windows Credential Manager（`advapi32` + `crypt32`），**不落明文文件**。
- **不在 TSF 回调里调用**（宪法 P2：不得在 `ITfTextInputProcessor::*` 回调内做同步 IO / 注册表 / 阻塞等待）。
- 凭据读取失败 ⇒ **视为未登录**（不崩）。

### 2.5 T304 · 同步编排 —— DONE

- `CloudAccount`（账户状态机 + 设备管理）+ `CloudSyncService`（推拉编排 + 条目合并）。
- 冲突合并走 `MergeEntries`（复用 `FluxingCloudEnvelope.h`，**未另写一份合并实现** —— 已 grep 取证测试文件里没有第二份）。
- **HLC 输出只发嵌套 `{ms,counter,device}`**（批次 Z §3.2 第 1 条）。

### 2.6 T305 · 账号页与词库快照通道 —— DONE

- **T102 快照通道**：词库导出 / 还原用 **`backup_user_dict` / `restore_user_dict`**（**不用** `export_user_dict` / `import_user_dict`），互斥经 `RimeWithWeaselHandler` 的 `StartMaintenance` / `EndMaintenance`（经 `CloudMaintenanceHost.h` 注入），**只在后台线程**执行。
- 行格式 `<code>\t<phrase>\t<c=.. d=.. t=..>`；云端 `item_key = "<词典名>\t<词条>"`。
- `CloudAccountDialog`：普通 Win32 **modeless** 窗口；`fluxing::cloud::account_dialog::Show()` 返回 `HWND`（`CloudAccountDialog.h:88`）。
- login 响应消费 `email_verified`；`DELETE /v1/devices/:id` 传 `device_id`。
- **已知局限**：墓碑（tombstone）**不跨端传播**。

### 2.7 工程接线（同一批 DONE）

- `WeaselServer/xmake.lua`：`add_files("./Cloud/*.cpp")`（原 glob **不递归**）+ `add_links` 补 `bcrypt` / `winhttp` / `advapi32` / `crypt32`。
- `WeaselServer/WeaselServer.vcxproj`：9 个 `.cpp` 逐文件 `PrecompiledHeader=NotUsing` + 8 个头 + 4 个库 + 全部 8 个 ItemDefinitionGroup 补 `/utf-8`。
- `test/TestFluxingCloud/TestFluxingCloud.vcxproj`：编入 7 个生产实现。
- `WeaselServer/WeaselServerApp.cpp`：`RegisterCloudMaintenanceHost(m_handler.get())`（**必须全局作用域** —— 定义在 `fluxing::cloud` 内会 LNK2001）。
- **`WeaselServer/stdafx.h` 未改**（它没有 `NOMINMAX`，所以逐文件 `NotUsing` + 文件头自带 `#define NOMINMAX` 是既定红线）。

---

## 3. 文档回写清单（本批，按用户「回写」授权）

| 文件 | 改了什么 |
|---|---|
| `plan.md` | **9 处**：① 头部新增「修订记录」表（2026-11-11 + 授权依据）② §3.1 密钥派生改写（CNG → 自实现 PBKDF2/HKDF + salt 策略 + Argon2id 否决理由修正）③ §4.4 邮箱验证码改 **$0 路线**（`MAIL_MODE="outbox"` 开发/内测 + 上线前第三方 HTTPS 邮件 API；**明确不选** Cloudflare Email Service、不采购 Workers Paid）④ §5 模块表（新增 `CloudJson` / `CloudSyncService`；`CloudCrypto` 标自实现；`CloudUserDictIo` 改快照通道；`CloudAccountDialog` 改普通 Win32 modeless；`CloudSyncEngine` 标阶段 F 未落地）⑤ §5.2 词库互斥改写（backup/restore + 维护态 + 墓碑不传播局限）⑥ §6.1 线上契约补充（login 含 `email_verified`；`/v1/health` 是第 10 条运维路由；`/v1/ping` 不存在；DELETE 传 `device_id`）⑦ §6.2 D1 表改 **7 表 4 索引** + D1–D6 偏差表 ⑧ §9 变更面按实做更新 ⑨ §13 服务端验证策略改 vitest-pool-workers |
| `tasks.md` | **5 处**：① §0 文档治理（A1–A3 + D1–D6 + 阶段 D 偏差已回写）② 阶段 C 状态块（7 表/4 索引、vitest-pool-workers、`GET /v1/health`）③ **阶段 D 整块重写**（T301–T305 各加「状态 DONE 2026-11-11」+ 实现偏差 + Done 判据达成/未达成）④ T401 改「界面层 DONE / 入口未接 / 普通 Win32 窗口」⑤ T405 改「账号入口接线 PENDING，位置 = 快捷设置栏右键菜单」 |
| `docs/adr/0054-client-key-derivation-and-salt-strategy.md` | **新建**（~5.9 KB）。记录「自实现 PBKDF2/HKDF + 按邮箱确定性派生 salt + 服务端从不用 salt 校验」的理由 / 决策 / 后果 / 验证 |
| `docs/adr/README.md` | 加 0054 索引行（Accepted） |
| `CHANGELOG.md` | `## [Unreleased]` 下插入「阶段 D 客户端运行时落地（尚无用户可见入口）」条目 |
| `docs/ops/fluxing-cloud-external-prerequisites.md` | **整篇重写**（12,039 B）：状态总览表（3 待办 + 3 已解除）+ 7 条 Cloudflare 命令与判据 + DNS 自动/手工 + 验证码 $0 决策 + 阶段 E 入口决策 + 文档回写清单 + 未取证清单 |
| `cloud/fluxing-sync/wrangler.toml` | **3 处**：① 启用 `routes = [{ pattern = "api.aiec.fun", custom_domain = true }]` ② `MAIL_FROM = "no-reply@aiec.fun"` ③ `send_email` 绑定注释改为「$0 路线不启用」 |
| `CLAUDE.md` | §2 交接指针更新为批次 AB |

### 3.1 用户已拍板的三件事（本批已落盘，勿再翻案）

1. **验证码通道 = $0**（`MAIL_MODE="outbox"` 开发/内测 + 上线前第三方 HTTPS 邮件 API；不采购 Workers Paid；接受打破 T101「同厂商」边界的代价）。
2. **阶段 E 账号入口 = 快捷设置栏的右键菜单**（右键菜单**不是被删除**，是换地方；与品牌图标右键 / 托盘右键共用 `IDR_MENU_POPUP`；不改面板几何与状态真相源；不新增 IPC 消息）。
3. **`plan.md` / `tasks.md` 的 6 处偏差按实现回写**。

---

## 4. 门禁结果（**verified**）

| 门禁 | 命令 | 结果 |
|---|---|---|
| 阶段 D 单测 | `tmp\build-fluxing-cloud-tests.cmd` | `TEST_EXIT=0`；`RESULT: PASS=297 FAIL=0`；`VERDICT: ALL PASS`（基线 134）→ 日志 `tmp\stageD-build.log` |
| WeaselServer 构建 | `build.bat weasel`（**必须在仓库根目录作 cwd**） | 0 错误，产出 `output/Win32/WeaselServer.exe` → 日志 `tmp\stageD-weaselserver-build.log` |
| clang-format | `clang-format --dry-run -Werror`（18 个文件） | **CLANG_EXIT=0**（此前 CLANG_EXIT=1，已 `clang-format -i` 修掉；因目录整体未跟踪，无 diff 噪声） |

> 说明：格式化后**已重跑**上面两条门禁，结果以上表为准。

---

## 5. 下一阶段计划与理由

### 5.1 阶段 E「账号入口接线」（**T405**，代码侧已具备被调能力）

- 账号页 API 已就绪：`fluxing::cloud::account_dialog::Show()` 返回 `HWND`。
- 待做：把「账号」菜单项接进**快捷设置栏右键菜单**（复用 `IDR_MENU_POPUP`）。
- 理由：批次 U 摘掉「用户登录」键帽、批次 W 摘掉语言栏项 ⇒ 目前**没有任何入口**能进账号页；而右键菜单同时被品牌图标右键与托盘右键复用 ⇒ 一处接线三处可见，成本最低、且不需要恢复被摘掉的键帽（用户已明确「右键菜单不是删除，是换地方」）。

### 5.2 阶段 F「同步引擎」（`CloudSyncEngine` 未落地）

- `plan.md` §5 已标注阶段 F 未实现；当前 `CloudSyncService` 只到「推拉 + 合并」层。

### 5.3 验证码 $0 路径落地（阶段 G）

- `MAIL_MODE` 需新增第三方 vendor 分支（当前只有 `outbox` / `cloudflare` 两个模式，**没有 `console`** —— 旧 ops 文档写错，本批已改正）。
- **额度与条款尚未核价**（本机 `web_search` 恒返 HTTP 402，只能 `web_fetch` 官方页）。

### 5.4 Cloudflare 人工动作（**阻塞线上判据**，见 §6）

---

## 6. 需用户在场 / 人工的事

### 6.1 Cloudflare OAuth 授权（**唯一的硬阻塞**）

```batch
cd /d F:\soft\00selfmade\rime_claude\cloud\fluxing-sync
npx wrangler login
npx wrangler whoami
```

**判据**：`whoami` 能看到邮箱 + Account ID，且**不出现** `You are not authenticated`。

> 授权完成后**由我代跑**其余全部命令（d1 create / r2 bucket create / migrations apply --remote / secret put / deploy）。

### 6.2 D1 库 + R2 桶（用户已批「跑吧」）

```batch
npx wrangler d1 create fluxing-sync
npx wrangler r2 bucket create fluxing-sync
```

- `d1 create` 打印的 `database_id` **需回填** `wrangler.toml`（当前是占位全零 UUID）。
- **桶名是 `fluxing-sync`**（旧 ops 文档写的 `fluxing-sync-blobs` 是错的，已改）。

### 6.3 迁移 + secret + 部署

```batch
npx wrangler d1 migrations apply fluxing-sync --remote
npx wrangler secret put AUTH_PEPPER
npx wrangler deploy
```

- `AUTH_PEPPER` ≥ **32 字节随机串**，**绝不进仓库**。
- 部署判据：`curl -sS -o NUL -w "%{http_code}" https://api.aiec.fun/v1/health` = **200**。

### 6.4 DNS（`aiec.fun` → `api.aiec.fun`）

- 仓库侧已配好 `routes = [{ pattern = "api.aiec.fun", custom_domain = true }]`。
- 若 `aiec.fun` 与 Cloudflare 账号**同账号** ⇒ `wrangler deploy` 自动建记录。
- 若**跨账号** ⇒ 需手工 **CNAME** `api` → `wrangler deploy` 输出的目标主机名（**橙色云代理**开启）。
- 细节与兜底见 `docs/ops/fluxing-cloud-external-prerequisites.md` §3.3。

### 6.5 真机联调（需用户在场）

- 账号页未登录 / 已登录截图、真实发信、真实请求日志 ⇒ 依赖服务端部署完成，**目前全部 not verified**。

---

## 7. 证据路径

| 证据 | 路径 |
|---|---|
| 单测日志 | `F:\soft\00selfmade\rime_claude\tmp\stageD-build.log`（`PASS=297 FAIL=0` / `VERDICT: ALL PASS`） |
| 构建日志 | `F:\soft\00selfmade\rime_claude\tmp\stageD-weaselserver-build.log`（0 错误） |
| 阶段 D 源码 | `F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\`（18 文件） |
| 单测源码 | `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp`（+ `TestFluxingCloud.vcxproj`） |
| 接线 | `F:\soft\00selfmade\rime_claude\WeaselServer\xmake.lua`、`WeaselServer\WeaselServer.vcxproj`、`WeaselServer\WeaselServerApp.cpp` |
| ADR | `F:\soft\00selfmade\rime_claude\docs\adr\0054-client-key-derivation-and-salt-strategy.md` |
| 运维手册 | `F:\soft\00selfmade\rime_claude\docs\ops\fluxing-cloud-external-prerequisites.md` |
| 服务端配置 | `F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\wrangler.toml` |
| 免费结论 | `F:\soft\00selfmade\rime_claude\docs\research\fluxing-cloud-2026-11\free-tier-verdict.md` |

---

## 8. 本批工具坑（新增，供后续避开）

1. **`Cloud/` 实为 18 个文件，不是 10 个** —— 「10」是**模块数**（8 个模块 + 2 个单文件），文档里一度写成「10 个文件」，本批已全部改正为 18。
2. **`rime_get_api()` 不是 `RimeGetApi`** —— 真名带下划线。
3. **`MAIL_MODE` 只有 `outbox` / `cloudflare`**，**没有 `console`** —— 旧 ops 文档写错，本批已改正。
4. **`WinHttpOpenRequest` 在本 SDK 是 7 参** —— 少传会编译失败。
5. **`RegisterCloudMaintenanceHost` 必须定义在全局作用域**（命名空间内 Impl + 全局包装），否则 LNK2001。
6. **不要改 `WeaselServer/stdafx.h`** —— 它没有 `NOMINMAX`，逐文件 `NotUsing` 是既定红线。
7. **不要用 CNG 的 `BCryptDeriveKeyPBKDF2`** —— 它**默认 PRF = SHA-1**，与 WebCrypto 的 SHA256 不对齐。
8. **不要用 `export_user_dict` / `import_user_dict`** —— T102 红线，必须走 `backup_user_dict` / `restore_user_dict`。
9. **不要在 `CloudAccountDialog.cpp` 对邮箱做 `ToUtf8` 再赋 `std::wstring`**。
10. **未加 `mutable` 的 lambda 不可调 `SecureZeroString(&captured)`**。
11. **clang-format 是提交前必过关口**（AGENTS.md §5 第 4 步 / `.claude/rules/commit-checklist.md` 第 1 步 / 宪法 P1）—— 本批 `Cloud/` 18 个文件初始 `CLANG_EXIT=1`，`clang-format -i` 后归零；**改完必须重跑单测与构建两条门禁**才能声称双绿。
12. **不要在 `cloud/fluxing-sync/` 内写默认子域字面量**（端点门禁零豁免扫描）。
13. 既有的坑仍然成立：`git commit -F -` **不吃 stdin**（须写临时文件再 `-F <file>`）；一次 `edit` 传多组 `old_string`/`new_string` 会**静默丢键**（只生效最后一组）；`git add .` 禁用（要显式按路径 add）。

---

## 9. 三分账（verified / inferred / not verified）

### 9.1 verified（已实测）

- `WeaselServer/Cloud/` 18 个文件齐全；`fluxing::cloud::account_dialog::Show()` 返回 `HWND`。
- 单测 `PASS=297 FAIL=0`；`build.bat weasel` 0 错误。
- clang-format 18.1.8 对 18 个文件 `--dry-run -Werror` 干净（`CLANG_EXIT=0`）。
- 服务端 `MAIL_MODE` 只有 `outbox` / `cloudflare` 两模式。
- `include/FluxingCloudEnvelope.h` 的 5 字段 AAD 与 `MergeEntries` 形态；测试文件里**无第二份**合并 / HLC 实现。
- Cloudflare 官方原文两条：**Workers Paid = 账号级最低消费 $5/月**；**任意收件人发信需 Workers Paid**。

### 9.2 inferred（有依据的推断，非实测）

- 「Free 档存数据 + 跑端点可 $0」成立（基于官方额度原文推演）。
- 真正的天花板是 **D1 写 10 万行/天**（≈5,000 日活 × 每人每天 20 条）—— 内测与个人使用远在免费档内。
- **PBKDF2 60 万次的实际耗时未单独计时**（单测用 1000/2000 次 + 常量断言）⇒ 登录首帧延迟需真机测。

### 9.3 not verified（未验证 / 依赖外部）

- `wrangler login` **未执行**；`whoami` 未跑。
- D1 库 / R2 桶 **未创建**；`wrangler.toml` 的 `database_id` **仍是占位全零 UUID**。
- `AUTH_PEPPER` **未设置**；`wrangler deploy` **未执行**；`api.aiec.fun` **未部署未验证**。
- 真实发信（`MAIL_MODE=cloudflare` 绑定）**未验证**。
- 账号页真机联调 / 截图 / 请求日志 **未验证**（依赖服务端上线）。
- 第三方邮件 API 分支的**额度与条款未核价**。
- 阶段 E 入口（T405 菜单项接线）**未做**。
