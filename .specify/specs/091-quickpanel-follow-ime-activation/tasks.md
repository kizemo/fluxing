# 091 · Tasks — 快捷设置栏显隐跟随输入法激活状态

> 优先级：**P1** = 本轮必做；**P2** = 本轮应做；**P3** = 可延后。
> 用户故事标签：`[US091-A]` 显隐跟随输入法激活；`[US091-B]` 语言栏菜单去除常驻项。

---

## T001 [P1] [US091-A] 协议层：新增 FocusOut 事件种类标志 + 纯判定函数

- **输入**：`include/WeaselIPC.h`
- **输出**：
  - 新增 `enum { WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0 };`
    （紧邻既有 `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED`，注释写明与它的对称关系）
  - 新增纯函数
    `inline bool ShouldHideQuickPanelOnFocusOut(DWORD param, bool always_show)`
  - `weasel::Client::FocusOut()` 改为 `void FocusOut(DWORD client_caps = 0);`
- **约束**：线格式（命令号 / 字段布局）**一字不改**；只复用既有 `wParam` 槽位
- **验收**：`include/WeaselIPC.h` 内无新增消息枚举项；`WEASEL_IPC_LAST_COMMAND` 未变

## T002 [P1] [US091-A] 客户端：`FocusOut` 透传参数

- **输入**：`WeaselIPC/WeaselClientImpl.h` / `WeaselIPC/WeaselClientImpl.cpp`
- **输出**：
  - `ClientImpl::FocusOut(DWORD client_caps = 0)` ⇒
    `_SendMessage(WEASEL_IPC_FOCUS_OUT, client_caps, session_id.load());`
  - `Client::FocusOut(DWORD client_caps)` 转发
- **约束**：默认参数 0 ⇒ 现有全部调用点（`KeyEventSink.cpp:91`、`WeaselTSF.cpp:464`）行为**不变**
- **验收**：`grep -n "FocusOut" WeaselIPC/WeaselClientImpl.cpp` 显示参数已透传

## T003 [P1] [US091-A] 前端：profile deactivated 时带上标志

- **输入**：`WeaselTSF/WeaselTSF.cpp`（`_ProfileNotifyLoop` 的 `kProfileNotifyDeactivated` 分支，`:404-407`）
- **输出**：`m_client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);`
- **约束**：**不改** `OnSetFocus` 那条失焦路径（`KeyEventSink.cpp:91`）—— 它必须继续发 0
- **验收**：源码里「切输入法」与「切窗口」两条路径发出的 `wParam` **可区分**

## T004 [P1] [US091-A] 服务端：按事件种类分流

- **输入**：`RimeWithWeasel/RimeWithWeasel.cpp`（`FocusOut`，`:531-559`）
- **输出**：
  ```
  if (ShouldHideQuickPanelOnFocusOut(param, QuickPanelDialog::AlwaysShowMode()))
    QuickPanelDialog::Hide();
  ```
  并把 `:550-557` 的注释改成「两种语义 + 指向 ADR 0024」
- **约束**：`QuickPanelDialog::Hide()` **本体不动**（ADR 0016 D1）；5 条手势路径不动
- **验收**：常驻态下切输入法 ⇒ 隐藏；常驻态下切窗口 ⇒ 不隐藏

## T005 [P1] [US091-B] 语言栏菜单去除「快速面板常驻」项

- **输入**：`WeaselTSF/WeaselTSF.rc`（UTF-16LE + BOM）
- **输出**：删掉 3 条 `MENUITEM ... ID_QUICKPANEL_ALWAYS_SHOW`
  —— `IDR_MENU_POPUP:72`（英）/ `IDR_MENU_POPUP_HANT:97`（繁）/ `IDR_MENU_POPUP_HANS:122`（简）
- **约束**：
  - **保留** `WeaselServer/WeaselServer.rc` 的 3 处托盘菜单项（取消入口）
  - 文件编码 / BOM / 行尾**保持原样**（UTF-16LE + BOM）
  - 其余菜单项与顺序**一字不动**
- **验收**：改完 `grep -c "ID_QUICKPANEL_ALWAYS_SHOW" WeaselTSF/WeaselTSF.rc` = 0；
  `WeaselServer/WeaselServer.rc` 仍 = 3；资源编译通过

## T006 [P2] 共享头注释同步

- **输入**：`include/resource.h:35-39`
- **输出**：更新注释 —— 说明该编号现在**只有托盘菜单**在用，语言栏弹出菜单已移除该项（spec 091）
- **约束**：`#define ID_QUICKPANEL_ALWAYS_SHOW 40019` 的**取值不动**（必须与 `WeaselServer/resource.h:53` 相等）

## T007 [P1] 回归测试：`test/TestWeaselIPC` 的 `/verify-focusout-flags`

- **输入**：`test/TestWeaselIPC/TestWeaselIPC.cpp`
- **输出**：
  - mock 的 `TestRequestHandler` 新增 `FocusOut(DWORD param, DWORD session_id)` 重写 ⇒
    把 `param` 写进**与 exe 同目录**的 `focusout_param.txt`
  - 新增 `verify_focusout_flags_main()`：8 条断言（见 plan.md「测试策略」T1–T8）
  - `_tmain` 增加 `/verify-focusout-flags` 分支
- **验收**：单独跑该模式 ⇒ 退出码 0 且输出 `[FOCUSOUT] PASS`；
  故意把 T003 的标志去掉 ⇒ 必须变红

## T008 [P2] 套件接线

- **输入**：`scripts/test-infra/run-test-suite.bat`
- **输出**：在 TestWeaselIPC 块内、`/stop` 之前加一行
  `"Release\TestWeaselIPC.exe" /verify-focusout-flags < nul` + `if !errorlevel! NEQ 0 set "FAIL=1"`
- **约束**：必须在 mock 服务器起来之后、`/stop` 之前
- **验收**：完整套件输出出现 `[FOCUSOUT] PASS`

## T009 [P1] 格式化 + 构建 + 完整套件

- **输入**：本轮全部改动文件
- **输出**：
  - `git-clang-format --diff --style=file HEAD`（只格式化改动行；**绝不全仓跑**）
  - 21 个测试工程 `/t:Clean` 后全量重编 + 完整套件 ⇒ 贴输出
- **约束**：跑测试前 `Stop-Process -Name WeaselServer -Force`，跑完恢复服务
- **验收**：套件退出码 0 且无 `error_like` 行

## T010 [P1] 版本号 1.1.5 → 1.1.6 + 打包

- **输入**：`env.bat`（gitignored，版本真源）、`weasel.props`（gitignored）、
  `build.bat:44` / `xbuild.bat:55`（tracked fallback）
- **输出**：版本资源 `1.1.6.0`；`output\archives\fluxing-1.1.6-installer.exe`
- **约束**：`env.bat` 必须 `RELEASE_BUILD=1`；**禁用** `git describe --tags` 取版本号
- **验收**：NSIS 打包 exit 0，产物大小 / SHA256 入 CHANGELOG

## T011 [P2] 文档同步

- **输入**：`CHANGELOG.md`、`docs/adr/0024-*.md`、`include/resource.h` 注释
- **输出**：`[1.1.6]` 段的 `### 主要更新` 两条；ADR 0024（MADR 格式）
- **验收**：`grep -c "## \[1.1.6\]" CHANGELOG.md` = 1

## T012 [P1] 提交 + 推送 + 交付

- **输入**：本轮全部改动
- **输出**：提交（Conventional Commits + 模块前缀）⇒ 推 `kizemo/Fluxing` ⇒ 交付 exe + 验收清单
- **约束**：提交前 `git status -sb` + `git diff --stat` 复核（防 `weasel.props` / `env.bat` / `*.obj` 误提交）
- **验收**：`git rev-list --left-right --count kizemo/Fluxing...HEAD` = `0 0`
