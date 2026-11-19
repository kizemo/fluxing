# ADR 0063 — 账号页信息架构：按登录态互斥的两套视图 + 四个独立子窗口

- **Status**: Proposed（批次 AT-1 已落码；编译门禁与真机验收未完成 ⇒ 待用户验收后转 Accepted）
- **Date**: 2026-11-19
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（阶段 D · T305 / 阶段 F · T505 / 阶段 G）
- **Revises**: —（新增；不推翻 ADR 0062 的「机会性同步」口径与文案）
- **Lessons**: `.specify/memory/lessons-learned.md` L222–L225
- **Supersedes / superseded by**: —（新增）

---

## 背景

用户 2026-11-19 提出账号页重构需求（原文见 spec 100 tasks「批次 AT」段）：

1. **已登录态**与**未登录态**必须区分。已登录态只显示：登录账号、退出登录、立即同步、关闭。
2. **未登录态**显示：登录邮箱框、密码框、登录、注册、忘记密码、关闭。
3. 邮箱框可列出**历史登录过的邮箱**。
4. 点「登录」可选择**是否保存账号**。
5. 首次登录某账号需要邮箱验证时，点登录后**弹出独立窗口**收验证码（不再内嵌主窗口）。
6. 点「注册」进入注册账号界面；点「忘记密码」进入找回密码界面（邮箱发验证码 + 修改密码）。
7. 登录成功后主窗口切到已登录态。

### 事实 1 — 改造前的账号页是「一屏全堆 + 灰亮」

`WeaselServer/Cloud/CloudAccountDialog.cpp`（批次 AS 之前）：

- `WM_CREATE` **一次性创建 21 个控件**并全部常驻；
- 登录 / 退出 / 同步 / 设备 / 清空云端数据 / 验证码 全部**同屏**；
- 切换登录态只靠 `EnableWindow` 灰亮，**从不隐藏控件、从不显示当前登录邮箱**；
- 验证码输入框 `kIdVerifyCode`(1012) **内嵌主窗口第 3 行**。

⇒ 用户看到的是「一屏永远不变的表单」，无法从界面判断自己处于登录态还是未登录态。

### 事实 2 — 仓库里不存在任何「历史邮箱 / 记住账号 / 偏好存储」机制

全仓检索（批次 AT 取证）：历史邮箱、记住账号、偏好存储三类设施**全部为零**。账号页 UI 也**零自动化测试**。

### 事实 3 — 凭据存储是单条覆盖写，且 `Logout` 会连带删掉 email

`WeaselServer/Cloud/CloudCredentialStore.cpp`：target = `Fluxing:CloudAccount`，magic `FLCRED1`，
存 email / account_id / device_id / access_token / refresh_token / auth_key_b64 / enc_key_b64 /
server_salt_b64 / email_verified / expires_in，**不存口令**。`Logout` 走 `store_->Delete`，
**连 email 一起删**。

⇒ **历史邮箱不能存在凭据存储里**，否则「退出登录」会把用户的历史一起清掉。

### 事实 4 — 服务端没有「忘记密码」端点

`cloud/fluxing-sync/src/index.ts:51-113` 的 auth 路由只有 register / verify / resend / login / refresh；
grep「忘记密码 / reset / password-reset」在 `cloud/**` **零命中**。
历史决策见 spec 100 `spec.md:53`、`tasks.md:373`、ADR 0039、handoff D11：「首版不做找回密码」，
理由是「改密牵动全量重加密」。

---

## 决策

### 决策 1 — 登录态用**互斥的两套布局**表达，不用「同屏 + 灰亮」

- 未登录态可见控件集 = `AccountLoginLayout()`（12 项：邮箱标签 / 邮箱框 / 历史标签 / 历史下拉 /
  密码标签 / 密码框 / 保存账号 / 登录 / 注册 / 忘记密码 / 状态行 / 关闭）。
- 已登录态可见控件集 = `AccountLoggedInLayout()`（8 项：当前账号标题 / 当前账号邮箱 / 退出登录 /
  立即同步 / 同步状态 / 机会性同步说明 / 状态行 / 关闭）。
- 两态**互斥**（仅共用状态行与关闭按钮）。

**已登录态保留两行文字 + 一行说明**，是用户「只显示 4 项」表述的**按控件理解**：用户点名的四项是
「登录账号 / 退出登录 / 立即同步 / 关闭」；`kIdSyncStatus`（最近同步结果）与 `kIdOpportunityNote`
（机会性同步说明）是 ADR 0062 已定稿的用户可见口径，**不因本次重构被删**。

### 决策 2 — 一个 WndProc + 5 个**角色窗口**，不写 5 套独立窗口类

账号页拆成 5 个窗口（角色值与 `CloudStatusScope` 一一对应）：

| 角色 | 窗口类名 | 客户区 | 职责 |
|---|---|---|---|
| `kAccount` | `FluxingCloudAccountDialog_v1` | 480×250 | 登录 / 退出 / 立即同步（登录态切换） |
| `kVerify` | `FluxingCloudVerifyDialog_v1` | 400×214 | 邮箱验证码 |
| `kRegister` | `FluxingCloudRegisterDialog_v1` | 400×236 | 注册账号 |
| `kForgot` | `FluxingCloudForgotDialog_v1` | 400×270 | 找回口令 |
| `kDevice` | `FluxingCloudDeviceDialog_v1` | 480×346 | 设备列表 / 撤销设备 / 清空云端数据 |

- 角色值经 `CREATESTRUCT.lpCreateParams` 传入，落 `GWLP_USERDATA`；
- **同一个 `WndProc`** 按角色分支；可见性与几何用 `ShowWindow` + `SetWindowPos` 切换，**不重建控件**；
- 控制器用**窗口注册表**（`std::map<HWND,int>`）而不是「一个主窗口句柄」⇒ 后台任务完成时
  `WM_APP_REFRESH` **广播**给所有窗口，各窗口按**自己的作用域**取状态文案。

**理由**：控制器是**进程级单例**且被 5 个窗口共享。若状态文案只有一份，主窗口的「同步成功」会
覆盖验证码窗口的「验证码已发送」⇒ 必须分作用域（`status_text[5]`）。

### 决策 3 — 可见性与几何**同源于一份真相源**，生产代码与单测共用

新建 `WeaselServer/Cloud/CloudAccountUi.h`：

- 控件 ID 表（`AccountControlId`）、窗口角色（`AccountWindowRole`）、控件类别（`AccountWidgetClass`）；
- 6 张布局表（未登录 / 已登录 / 验证码 / 注册 / 找回 / 设备）；
- 派生函数 `AccountRoleLayout` / `AccountVisibleLayout` / `AccountVisibleIds` /
  `AccountVisibleControlIds` / `AccountWidgetClassOf` / `AccountWidgets` / `AccountClientSize`。

⇒ 「可见性」与「几何」不可能漂移：可见集**就是**布局表本身，不是另抄一份 ID 清单
（constitution R7「生产与单测共用一套实现」）。

**硬约束**：本头**不包含 `<windows.h>`**（避免 min/max 宏与 `<algorithm>` 打架）⇒ 任何工程都能包含它。
两个工程（`WeaselServer.vcxproj` / `TestFluxingCloud.vcxproj`）的**全部**配置都开了 `/utf-8`
⇒ 本头允许中文注释。

### 决策 4 — 控件 ID **只追加、不改值**

1001–1017 是 T305 / T505 时代定下的，**历史上被外部脚本与探针引用过** ⇒ 值冻结。
1018 起是批次 AT-1 新增（历史下拉 / 保存账号 / 当前账号 / 忘记密码 / 机会性同步说明 /
新密码 / 重置按钮 / 各静态标签 …）。

### 决策 5 — 历史邮箱存**用户数据目录下的纯文本文件**，与凭据存储解耦

- 落点：`<用户数据目录>\fluxing-cloud-accounts.history`（真相源 = `ResolveFluxingUserDataPath`）；
- 格式：每行一个邮箱，UTF-8 无 BOM，最多 **10** 条，**最近在前**，去重（大小写不敏感）；
- 读写在**工作线程**做（UI 线程不碰磁盘），路径解析真源 = `include/FluxingUserPaths.h`（header-only）；
- 与凭据存储解耦的理由见**事实 3**。

### 决策 6 — 「保存账号」复选框默认选中；不勾选时登录**仍然成功**

`CloudAccount` 增加 `creds_persisted_` 门控：

| 动作 | 勾选（默认） | 不勾选 |
|---|---|---|
| 登录 | 写凭据库 | **不写**，且**不动**已有凭据（仅本次会话内存生效） |
| `Logout` | 删凭据 | 不删 |
| `Refresh` | 落盘 | 不落盘 |
| `RestoreFromStore` | 置 `persisted = true` | — |

⇒ 「不保存账号」不等于「不让登录」。这是用户明确要求的语义。

### 决策 7 — 需要邮箱验证时，登录成功后**自动弹验证码窗口**

流程：点「登录」→ 后台线程跑 PBKDF2 + `/v1/auth/login` → 成功但 `email_verified == false`
⇒ 置一次性请求 `verify_request_email` + **best-effort 自动发一次验证码**
⇒ UI 线程消费请求 ⇒ 弹出**独立**验证码窗口（不再内嵌主窗）。

**落码时发现的缺口（批次 AT-1 已修）**：首版实现里 `ConsumeVerifyRequest()` 与
`ShowVerifyWindow()` **没有任何调用者** ⇒ 一次性请求置位了却没人消费 ⇒ 验证码窗口
**永远弹不出来**（登录未验证路径与注册路径同时失效，用户被卡在「登录成功但邮箱
尚未验证」）。修法 = 把消费点挂到 UI 刷新入口 `RefreshUi()`：后台任务完成后
`NotifyWindows()` 广播 `WM_APP_REFRESH` ⇒ UI 线程消费一次性请求 ⇒ 弹独立验证码窗
（取走即清 ⇒ 多个窗口同时刷新也只弹一次）。

### 决策 8 — 设备与云数据搬进独立窗口，入口是托盘 / 快捷设置栏右键菜单新项

- 新增 `ID_WEASELTRAY_CLOUD_DEVICE 40025`（`WeaselServer/resource.h`）；
- `WeaselServer.rc` 的 **CHS / CHT / ENU 三块**各加一条 `MENUITEM`（该文件是
  **UTF-16LE + BOM + CRLF**，编辑必须脚本保真）；
- 助记符 `&Y`（`&C` 已被「用户文件夹」占用，`&Y` 是「与 / 云」的拼音首字母，三种语言均空闲）；
- 未登录时该窗口**照样能打开**（菜单项常在），但会改云端数据的按钮**置灰**
  （`DeviceDialogEnabledControlIds(logged_in)`）；
- 菜单资源 `IDR_MENU_POPUP` 被托盘右键与快捷设置栏右键**共用**（ADR 0038）⇒ 一个菜单项覆盖两个入口。

### 决策 9 — 批次 AT-1 的「忘记密码」**临时耦合**现有端点

- 「发送验证码」调用**现有** `POST /v1/auth/resend`（今天就能真发信，Resend `MAIL_MODE=http`）；
- 「重置密码」调用 `POST /v1/auth/password-reset`（**AT-2 前必然 404**）⇒ 客户端把服务端返回的
  失败**原样转成中文状态行**，不静默。

**该耦合必须在批次 AT-2 拆开**：现有 `email_verifications` 表**无 purpose 列**且 `codeHash`
不混入用途 ⇒ 直接复用会导致「注册码可当重置码用」。AT-2 须按 purpose 隔离
（迁移 `0002` 加 `purpose` 列并混入 `codeHash`，或另起新表）。

### 决策 10 — 不新增 IPC 消息

本批次只改客户端 UI，不碰 `include/WeaselIPCData.h` ⇒ **不触发四边同步红线**
（server / TSF / deployer 三端 + ADR + `test/TestWeaselIPC/*`）。

---

## 后果

### 正面

- 登录态**一眼可辨**（两套互斥布局，不是灰亮堆叠）；
- 验证码 / 注册 / 找回 / 设备四个流程**各自独立成窗**⇒ 主窗不再被长表单撑高（480×490 → 480×250）；
- 状态文案分作用域 ⇒ 多窗并行时不会互相覆盖；
- 可见性与几何同源 ⇒ 单测可直接断言「登录态下不该出现邮箱框」这类**用户可感**的规则；
- 历史邮箱不再随「退出登录」消失。

### 负面 / 代价

- 账号页从 1 个窗口变 5 个 ⇒ 窗口生命周期、置前、父子关系（子窗居中于主窗）都要管；
- 新增 4 个文件 + 2 个新头（`CloudAccountUi.h` / `CloudAccountWidgets.h` / `CloudAccountHistory.{h,cpp}`）；
- 「忘记密码」在 AT-2 之前**只有发信能用**，重置一定失败（404 ⇒ 中文提示）。

### 风险

- 窗口注册表（`std::map<HWND,int>`）若在窗口销毁后未反注册 ⇒ 悬垂 HWND ⇒ 必须在 `WM_DESTROY` 注销；
- 账号页 UI **仍无自动化测试**（跨进程抓不到自绘与独立窗），只能靠**布局表单测** + 用户目视；
- `WeaselServer.exe` 是 **PPL** ⇒ 装机前必须注销 / 重启，旧映像仍在跑。

---

## 备选方案（已驳回）

| # | 方案 | 驳回理由 |
|---|---|---|
| 1 | 保留单窗口，靠 `EnableWindow` 灰亮区分登录态 | 用户明确要求「区分」，灰亮堆叠无法表达状态；且「登录账号」当前邮箱根本没地方显示 |
| 2 | 验证码继续内嵌主窗第 3 行 | 用户明确要求「弹出独立窗口」；且主窗会被验证码行撑高 |
| 3 | 历史邮箱存进 `FLCRED1` 凭据 blob | `Logout` 的 `Delete` 会连带清 email ⇒ 历史会丢（**事实 3**） |
| 4 | 历史邮箱存注册表 | 注册表写入属同步 I/O 且难以限制条数 / 顺序；纯文本文件可读可备份、便于排查 |
| 5 | 写 5 套独立窗口类（每类一份 WndProc） | 5 份重复的窗口样板；角色化单 WndProc 更少代码、更少漂移 |
| 6 | 「保存账号」不勾选时直接拒绝登录 | 用户要求的是「选择是否保存账号」，不是「是否允许登录」 |
| 7 | 「忘记密码」等 AT-2 一起做，AT-1 不出这个按钮 | 用户 4 条需求同时提出；AT-1 先出「发信可用、重置如实报错」的版本，用户能先看到界面 |

---

## 与其它 ADR 的关系

- **ADR 0062**（维护准入闸门 / 机会性同步）：本 ADR **保留**其已定稿的用户可见文案
  （「你正在输入时同步会自动延后，停止输入后自动完成。」）⇒ `kIdOpportunityNote` 不得删改；
- **ADR 0038**（菜单资源共用）：托盘与快捷设置栏右键共用 `IDR_MENU_POPUP` ⇒ 新菜单项只加一处；
- **ADR 0057**（item_key TAB 契约）：本 ADR 不涉及；
- **待写**：**ADR 0064** = 服务端「忘记密码 / 重置口令」端点（批次 AT-2），含 purpose 隔离、
  撤销全部 refresh_tokens、以及「重置口令 ⇒ `enc_key` 变 ⇒ 旧 R2 密文解不开」的处置口径。
