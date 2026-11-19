# 批次 AT · 账号页 UI 重构 + 注册 / 找回密码入口（交接文档）

- **日期**：2026-11-19
- **分支**：`feat/100-cloud-sync`；HEAD = `3b74783`（其上 `9070f38` ← `d49a0ca` ← `38c78a7` ← `8da2f72`）
- **本批性质**：**代码实现批次**（客户端 UI 重构 + 一处真缺陷修复）；**未改 `librime/`、未改 IPC 协议、未新增第三方依赖**（⇒ 不触发 CLAUDE.md「IPC 四边同步」红线）
- **子批次**：**AT-1 客户端（已完成 · 门禁全绿）** / **AT-2 服务端「忘记密码」端点（未做 · 被用户决策阻塞）**
- **ADR**：`0063`（新增 · **Proposed**）账号页信息架构

---

## 1. 用户原始需求（2026-11-19，逐条对照实现）

| # | 需求原文要点 | 本批状态 |
|---|---|---|
| 1 | 已登录态与未登录态**界面区分**；已登录态只显示「登录账号 / 退出登录 / 立即同步 / 关闭」 | ✅ AT-1：已登录态 8 项（含 ADR 0062 定稿的「最近同步」与「机会性同步」说明两行文字） |
| 2 | 未登录态显示「登录邮箱 / 密码框 / 登录 / 注册 / 忘记密码 / 关闭」；邮箱框可列出历史登录过的邮箱；点登录可选「是否保存账号」；**需要邮箱验证时点登录后弹出独立验证码窗口**（不再内嵌主窗）；登录成功后切到已登录态 | ✅ AT-1：未登录态 12 项；历史邮箱下拉 + 「保存账号」复选框 + 独立验证码窗 |
| 3 | 点「注册」→ 进入注册账号界面 | ✅ AT-1：独立注册窗（邮箱 / 密码 / 注册并发送验证码） |
| 4 | 点「忘记密码」→ 进入找回密码界面（邮箱发验证码 + 修改密码） | ⚠️ **客户端窗口已就位**；**发信可用**（复用现有 `/v1/auth/resend`），**重置端点服务端尚不存在 ⇒ 必然 404**（AT-2） |

---

## 2. 落码清单

### 新增文件（4）
| 文件 | 内容 |
|---|---|
| `WeaselServer/Cloud/CloudAccountUi.h` | **布局与可见性的唯一真相源**：控件 ID 枚举（1001–1017 冻结、1018 起只追加）、5 个角色窗口的布局表（`AccountLoginLayout` 12 项 / `AccountLoggedInLayout` 8 项 / `VerifyLayout` 7 / `RegisterLayout` 8 / `ForgotLayout` 11 / `DeviceLayout` 8）、派生函数（`AccountRoleLayout` / `AccountVisibleLayout` / `AccountVisibleIds` / `AccountVisibleControlIds` / `AccountWidgetClassOf` / `AccountWidgets` / `AccountClientSize` / `DeviceDialogEnabledControlIds`）。**故意不含 `<windows.h>`** ⇒ 生产工程与单测工程都能直接包含 |
| `WeaselServer/Cloud/CloudAccountWidgets.h` | 共用 Win32 helper：`ToWide` / `ToUtf8` / `DefaultUiFont` / `ApplyFont` / `AddControl` / `ReadControlText` / `SecureZeroWide` / `CenterOnOwner` / `EnsureWindowShown` |
| `WeaselServer/Cloud/CloudAccountHistory.{h,cpp}` | 历史邮箱合并 / 序列化 / 解析 / 落盘 / 读回；落点 `<用户数据目录>\fluxing-cloud-accounts.history`（最近在前、大小写不敏感去重、上限 10 条） |
| `docs/adr/0063-account-page-information-architecture-and-state-views.md` | 账号页信息架构 ADR（10 条决策 + 4 条事实 + 7 条已驳回备选方案） |

### 修改文件（10）
- `WeaselServer/Cloud/CloudAccountDialog.{h,cpp}` —— 控制器 + **1 个 WndProc / 5 个角色窗口**；新增 `history()` / `ConsumeVerifyRequest()` / `ShowDevice()`
- `WeaselServer/Cloud/CloudAccount.{h,cpp}` —— `Login(..., bool persist = true)`、`ResetPassword`、**`creds_persisted_` 门控**（不勾「保存账号」时登录仍成功，但不写凭据、不清已有凭据）
- `WeaselServer/WeaselServer.vcxproj` / `test/TestFluxingCloud/TestFluxingCloud.vcxproj` —— 接线新文件（脚本保 BOM + CRLF）
- `WeaselServer/resource.h` —— `ID_WEASELTRAY_CLOUD_DEVICE 40025`
- `WeaselServer/WeaselServer.rc` —— 三语言各加一条菜单「设备与云数据 (`&Y`)」（**UTF-16LE + BOM + CRLF，脚本保真**；`&C` 已被「用户文件夹」占用 ⇒ 选 `&Y`）
- `WeaselServer/WeaselServerApp.cpp` —— 新菜单项 handler（`m_uiThreadId` + `PostMessage` 回 UI 线程 → `account_dialog::ShowDevice()`）
- `test/TestFluxingCloud/TestFluxingCloud.cpp` —— 新增 `TestAccountPageUi()`（11 条）+ `TestAccountHistory()`（7 条）
- `docs/adr/README.md` —— 0063 索引行
- `CHANGELOG.md` —— `[Unreleased]` 段批次 AT-1 条目
- `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` —— 追加「批次 AT 收尾记录」段（AT-1…AT-4）

---

## 3. 关键决策（ADR 0063）

1. **登录态用互斥的两套布局**表达，不用「同屏 + 灰亮」。
2. **一个 WndProc + 5 个角色窗口**（角色值与 `CloudStatusScope` 一一对应），不写 5 套窗口类。
3. **可见性与几何同源于一份真相源**，生产代码与单测共用（constitution R7）。
4. **控件 ID 只追加、不改值**（1001–1017 历史上被外部脚本与探针引用过）。
5. **历史邮箱存用户数据目录纯文本**，与 Credential Manager 解耦（理由：`Logout` 会连带删掉 email ⇒ 历史不能放凭据里）。
6. **「保存账号」默认选中**；不勾选时登录**仍然成功**。
7. **需要邮箱验证时自动弹独立验证码窗**。
8. **设备与云数据搬进独立窗口**，入口 = 托盘 / 快捷设置栏右键菜单新项。
9. AT-1 的「忘记密码」**临时耦合**现有端点（AT-2 拆开）。
10. **不新增 IPC 消息**。

---

## 4. 缺陷取证链（本批修的真缺陷）

**现象**：用户要求的「点登录后弹出独立验证码窗口」在实现里**从未生效**；注册成功后同样弹不出验证码窗 ⇒ 用户被卡在「登录成功，但邮箱尚未验证」。

**取证**（源码级，窄路径 grep）：
- `SubmitLogin()` 成功且 `email_verified == false` ⇒ 置 `impl->verify_request_email`；
- `SubmitRegister()` 成功 ⇒ 同样置 `impl->verify_request_email`；
- `ConsumeVerifyRequest()`（定义在 `CloudAccountDialog.cpp:497`）与 `ShowVerifyWindow()`（定义在 `:1334`）**没有任何调用者** ⇒ 一次性请求置位了却无人消费；
- 服务端**允许**未验证邮箱登录（`handleLogin` 直接 `issueTokenPair` 并附 `email_verified: user.email_verified === 1`，`auth.ts:386-394`）⇒ 客户端的 `need_verify` 分支本该成立。

**修法**：把消费点挂到 UI 刷新入口 `RefreshUi()` —— 后台任务完成后 `NotifyWindows()` 广播 `WM_APP_REFRESH` ⇒ UI 线程消费一次性请求 ⇒ 弹独立验证码窗（取走即清 ⇒ 多窗口同时刷新也只弹一次）。

**连带修正**：注册成功提示文案（「请在验证码框输入」→「请在弹出的验证码窗口输入」）；验证成功后同步 `email_verified` 内存状态 + 账号页状态行 + 清口令框。

---

## 5. 门禁数字

| 门禁 | 结果 |
|---|---|
| `build.bat weasel` | **EXIT=0 · 0 警告 · 0 错误**（`WeaselServer.vcxproj -> output\Win32\WeaselServer.exe`） |
| 云单测 `Release\TestFluxingCloud.exe` | **`RESULT: PASS=492 FAIL=0` / `VERDICT: ALL PASS`**（基线 473 ⇒ **+19**；总数 492 与修断言前一致） |
| clang-format（本批 6 个改动文件） | **0 漂移**（clang-format 18.1.8） |
| clang-format（仓库级 `clang-format.ps1 -i`） | **红在既有历史漂移**（`test/Verifier2_G2_G12/v0_19_0_32_G2_G12.cpp` 等**本批未触碰**的文件）⇒ 与本批无关 |

> ⚠️ 已知既有告警（非本批引入）：`WeaselIPCServer/WeaselServerImpl.cpp(558,20): warning C4101: “ex”: 未引用的局部变量`。

---

## 6. 已验证 / 推断 / 未验证

**已验证（本轮实测）**
- 编译 0 警告 0 错误；云单测 492/0；格式 0 漂移；本批 6 文件字节健康（UTF-8 无 BOM + LF）。
- 服务端 `handleLogin` 允许未验证邮箱登录；`handleVerify` 只返回 `{ok:true}`（不返令牌）⇒ 验证后仍需用户点「登录」拿令牌（既有语义，未改）。
- 服务端**无**任何找回密码端点（auth 路由只有 register / verify / resend / login / refresh）。

**推断（未实测）**
- 验证码窗弹出后的真机交互（发信 → 输码 → 验证成功 → 回账号页登录）**未在真机跑过**。
- 「保存账号」勾 / 不勾两次登录的真机表现未验。

**未验证（需用户在场 / 需第二台设备）**
- 账号页两态的**目视**验收（跨进程抓不到 IME 自绘窗的像素判据）。
- 双机 AC 7 条（AC-03/04/05/08/09/13/14）+ restore 端到端。
- 输入延迟两组对照。
- **本批未装机、未重新出包**（1.3.2 包不含本批代码；`WeaselServer.exe` 是 **PPL** ⇒ 装机前必须注销 / 重启）。

---

## 7. 阻塞项：AT-2 的用户决策（**尚未答复**）

**问题**：重置口令 ⇒ `auth_key` 与 `enc_key` **都变**（二者均由 `HKDF(master, …)` 派生，`master` 依赖口令；`CloudCrypto.h:5-9,47-57`）⇒ **旧 R2 密文解不开**；而服务端**永不见** `enc_key` ⇒ 无法在服务端重加密。

| 方案 | 内容 | 状态 |
|---|---|---|
| **A（推荐）** | 重置即**清空云端**数据 | 属不可逆数据删除 ⇒ **需用户明确同意** |
| B | 只换 `auth_key`、保留 `enc_key` | **违反红线**（口令变了却保留旧加密密钥）⇒ 不建议 |
| C | 两段式：客户端拿新 `enc_key` 重推全量 | 复杂度高，但无数据损失 |

**另有一处服务端必须隔离**：`email_verifications` 表**无 `purpose` 列**且 `codeHash` 不含用途 ⇒ 直接复用会让「注册码可当重置码用」；`handleVerify` 副作用固定为 `SET email_verified = 1` 不可复用 ⇒ AT-2 须按 purpose 隔离（迁移 `0002` 加 `purpose` 列并混入 `codeHash`，或另起新表）。

**AT-2 闭环还需**：部署线上 Worker + D1 迁移 + 用户在信箱收一封真实重置验证码邮件（**不代跑**）。

---

## 8. 证据路径

- 布局真相源：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountUi.h`
- 控制器 / 5 角色窗口：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountDialog.cpp`（`RefreshUi` ≈ :1025 / `WM_CREATE` 角色取法 ≈ :1084 / `kRegister` 分支 ≈ :1178 / `ShowVerifyWindow` ≈ :1334）
- 历史邮箱：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\CloudAccountHistory.cpp`
- 单测：`F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.cpp`（`TestAccountPageUi` / `TestAccountHistory`）
- ADR：`F:\soft\00selfmade\rime_claude\docs\adr\0063-account-page-information-architecture-and-state-views.md`
- 账本：`F:\soft\00selfmade\rime_claude\.specify\specs\100-fluxing-cloud-account-sync\tasks.md`（末尾「批次 AT 收尾记录」）
- 构建 / 单测日志：`F:\soft\00selfmade\rime_claude\tmp\at-build-1.log` / `tmp\at-test-build-1.log` / `tmp\at-test-run-1.log`
- 服务端：`F:\soft\00selfmade\rime_claude\cloud\fluxing-sync\src\auth.ts`（`handleRegister` :255 / `handleVerify` :291 / `handleResend` :340 / `handleLogin` :360）

---

## 9. 本轮工具坑

1. **`edit` 工具一次只传一组 old/new**（多组会静默只留最后一组）⇒ 每处补丁单独一次调用。
2. **`.vcxproj` / `.rc` 必须脚本保 BOM + CRLF**（`edit` 工具的 `\n` 会插出混合行尾）；`WeaselServer.rc` 是 **UTF-16LE + BOM + CRLF**。
3. **正则匹配 CRLF 文件的行尾要用 `\r?$`**（`$` 在 `\r` 前不匹配）。
4. **全仓 grep（无 path）会超时** ⇒ 必须窄路径；`codebase-memory-mcp` 图谱**未索引** `WeaselServer/Cloud/**` 与 `cloud/**` ⇒ 降级 grep/read。
5. **`build.bat weasel` 不含 `TestFluxingCloud` 工程** ⇒ 单测须单独跑 `tmp\build-fluxing-cloud-tests.cmd`（**必须带 `SolutionDir`**，否则 `RimeWithWeasel\FluxingLocalChangeHook.cpp` 找不到头）。
6. **不能单编 `.vcxproj`**（缺方案级 include 路径，`wtl/atlapp.h` 找不到）⇒ 走 `build.bat`。
7. **`git commit -F -` 不吃 stdin**（消息写临时文件）。
8. **`cd` 后写入被安全策略拦** ⇒ 一律绝对路径 + `git -C`。
9. **取证类「具体值」必须当场回读文件**（上一轮凭形态脑补编造过口令 ⇒ 已沉淀进 agent memory）。
10. **`clang-format.ps1` 的 `-i` 是「检查」、`-n` 才是「就地格式化」**（与直觉相反）。
11. **`Copy-Item` 还原保留旧 mtime** ⇒ MSBuild 不重编译 ⇒ 变异验证会假红（还原后必须 touch）。
12. **`.vcxproj` 脚本接线会留 1 处裸 LF**（HEAD 是纯 CRLF）⇒ 提交前必须逐文件做字节健康扫描（裸 LF 计数 + BOM + 行尾形态），不能只看 `git diff`。本批已在提交前抓到并修掉（`WeaselServer.vcxproj`：CRLF 623→624、BARE_LF 1→0、BOM 未变）。
13. **`.rc` 是 UTF-16LE ⇒ 逐字节扫 0x0A 会把 CRLF 误判成裸 LF** ⇒ 校验 `.rc` 必须先 `Encoding.Unicode.GetString` 解码再统计行尾，并核对 BOM(FF FE) 与三语言条目条数。

---

## 10. 下一批建议顺序

1. 用户答复 AT-2 的 A/B/C 决策（**唯一硬阻塞**）⇒ 落 **ADR 0064**（服务端「忘记密码 / 重置口令」端点：purpose 隔离验证码 + 新端点 + 邮件模板）。
2. 实现 AT-2（服务端：`src/index.ts` 加路由 + `src/auth.ts` 加 `handlePasswordReset` + D1 迁移 `0002` + `src/mail.ts` 重置邮件模板 + 客户端 `SubmitResetPassword` 接真端点）⇒ 部署线上 Worker ⇒ 用户在信箱收一封真实重置邮件（**不代跑**）。
3. 出包（版本真相源四处同步：1.3.2 → 1.3.3）⇒ 用户注销 / 重启后装机 ⇒ 跑用户在场 AC（账号页两态目视 + 验证码窗 + 注册 + 找回密码 + 保存账号两次登录）。
4. 跑双机 AC 7 条 + restore 端到端 + 输入延迟两组对照。
5. 视授权 push 已积累的提交（**当前 6 个未 push**，含批次 AT-1 的 `feat(fluxing)` 提交）。

---

## 11. 接力 prompt

见当轮回答正文（按用户 2026-11-08 指示，**接力 prompt 直接写在回答里，不另落文件**）。
