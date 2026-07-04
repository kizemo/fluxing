# 036 · 火流猩输入法 v2 · 托盘快速面板 v0（单进程内嵌版）

> 元 spec 006 的 **第一阶段 ship 切片**。scope 限制：仅在 WeaselServer 单进程内注册 `Alt+,` 全局热键 + 处理托盘左键单击 + 弹出一个简单的 win32 弹窗（不做 D2D/不拆独立进程/不做 mac 风控件库）。后续 spec 037/038 在 v0.20 迭代到完整 mac 风面板。

## 0. 上下文

- spec 006 整体设计 16 个 tasks（FluxingComponents / DarkModeBridge / FluxingPanelHost / FluxingIPCClient），是 2-3 周大工程。
- **v0 ship 切片**：仅交付 "Alt+, 唤起 + 托盘左键唤起 + 简单弹窗" 三件事，闭环 v0.18.24.0 release。
- F11 DarkModeBridge 已在 spec 033 ship（`RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}`），spec 036 不重复造轮子。
- 现状：`WeaselServer/SystemTraySDK.cpp::OnTrayNotification` 已处理右键（11 项菜单）和左键双击（默认 item = Settings），**左键单击无处理**。这是 spec 036 的天然接入点。
- 现状：`WeaselIPCServer/WeaselServerImpl.h` 有完整 `CWindowImpl<>` WndProc（`MESSAGE_HANDLER` 宏），可在 `OnCreate` 注册 `Alt+,` 全局热键。

## 1. 产品视角

### 1.1 目标

提供 1 个全局热键 + 1 个托盘交互，把"最常用的 1 个切换"（中/英 `ascii_mode`）做成"任意 app 焦点 0.5 秒可达"，作为 spec 006 mac 风面板的最小可用版本。

### 1.2 用户故事

- US036-A [P1]：任意 app 焦点下按 `Alt+,` → 弹出 "中/英切换" 弹窗，含 1 个 "中/英" toggle button + 1 个 "重部署" 按钮。点击 toggle → 切换 ascii_mode + 弹窗关闭。
- US036-B [P1]：左键单击托盘图标 → 弹同一弹窗。
- US036-C [P1]：ESC 关闭弹窗，失焦 1s 自动关闭。
- US036-D [P2]：弹窗订阅 FluxingDarkModeBridge 主题色（spec 033 基础设施）— v0 推迟到 v0.20.0。

### 1.3 验收

- Given Fluxing v0.18.24.0 已安装，
- When 用户在 notepad 按 `Alt+,`，
- Then 屏幕右下角弹出 win32 弹窗（300x150 像素），含"中/英"和"重部署"两个按钮。
- And 点击"中/英" → ascii_mode 切换，弹窗关闭，托盘图标变 ASCII 态。
- And 按 ESC → 弹窗关闭。
- And 失焦 1s → 弹窗自动关闭。

## 2. Out of scope（v0 不做）

- mac 风渲染栈（D2D/DirectWrite）、FluxingComponents 控件库、Theme.h/.cpp、DarkModeBridge 引入（本 spec 033 已 ship，不重复）。
- FluxingPanelHost 独立进程（YAGNI，单进程足够 v0）。
- 8-12 个常用入口的 mac 风 grid 布局（v0 只 1 个入口 + 1 个 deploy 按钮）。
- 暗色主题自动渐变（spec 033 已 ship 基础设施；v0 弹窗用 static 颜色，订阅留 stub）。
- 跨用户/跨语言配置（hard-code `ascii_mode`）。

## 3. 依赖

- spec 005 `Shift+space` 切中英键位（已在 0.18.5+ ship）。
- spec 033 `FluxingDarkModeBridge`（已在 0.18.22.0+ ship，本 spec 引用但不重写）。
- Windows `RegisterHotKey(HWND hWnd, int id, MOD_ALT, VK_OEM_COMMA)` 全局热键（与 L04 同机制）。
- 现有 `WeaselServer/WeaselServerApp.cpp` + `WeaselServer/SystemTraySDK.cpp` + `WeaselIPCServer/WeaselServerImpl.{h,cpp}`。

## 4. 完成定义（v0.18.24.0 ship）

- [ ] 4.1 `WeaselIPCServer/WeaselServerImpl.h` 加 `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` + 加 `LRESULT OnHotkey(...)` 实现。
- [ ] 4.2 `WeaselIPCServer/WeaselServerImpl.cpp::OnCreate` 加 `RegisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL, MOD_ALT, VK_OEM_COMMA)`。
- [ ] 4.3 `WeaselIPCServer/WeaselServerImpl.cpp::Run` 退出前 `UnregisterHotKey(m_hWnd, ID_HOTKEY_QUICK_PANEL)`。
- [ ] 4.4 新建 `WeaselServer/QuickPanelDialog.{h,cpp}` — 简单 win32 弹窗（300x150, 含 "中/英" toggle + "重部署" + 关闭按钮 + ESC 处理 + 失焦 1s 关闭）。
- [ ] 4.5 `WeaselServer/resource.h` 加 `ID_HOTKEY_QUICK_PANEL` 常量。
- [ ] 4.6 `WeaselServer/WeaselServerApp.cpp::SetupMenuHandlers` 加 `ID_WEASELTRAY_QUICK_PANEL` 菜单 handler → 弹 `QuickPanelDialog`。
- [ ] 4.7 `WeaselServer/WeaselServerApp.cpp::Run` 在 `m_server.Start()` 后注册 `ID_WEASELTRAY_QUICK_PANEL` 行为。
- [ ] 4.8 `WeaselServer/WeaselTrayIcon.cpp::Refresh`（或新增 `WeaselServer/SystemTraySDK.cpp::OnTrayNotification`）增加 `WM_LBUTTONUP` 分支 → 模拟 `ID_WEASELTRAY_QUICK_PANEL` 菜单命令。
- [ ] 4.9 `WeaselServer/WeaselServer.rc` 在 3 个语言的 11 项菜单中加"快速面板"项，位置在 SETTINGS 之后；ID = ID_WEASELTRAY_QUICK_PANEL。
- [ ] 4.10 新建 `test/TestQuickPanelDialog/` 完整测试项目（vcxproj + cpp + stdafx）。
- [ ] 4.11 `weasel.sln` 加 TestQuickPanelDialog 项目 + sln entries。
- [ ] 4.12 `xbuild.bat weasel installer` → 0 errors；`TestQuickPanelDialog` 编译通过。
- [ ] 4.13 跑 TestQuickPanelDialog → 6/6 PASS。
- [ ] 4.14 AGENTS.md §2.5 smoke test 8/8 PASS。
- [ ] 4.15 升 v0.18.24.0（weasel.props + env.bat + CHANGELOG.md），commit，push to kizemo/Fluxing。
- [ ] 4.16 `release/fluxing-0.18.24.0-installer.exe` 生成并 commit。

## 5. 风险（v0 ship 切片特有）

- **R1**: `Alt+,` 与系统 / IDE 已知快捷键冲突（Vim 选 mark、Sourcetree 切 sidebar、某些 IDE 调 settings）。spec 036 风险登记（PRD §7）；v0 不做冲突检测，依赖用户反馈。
- **R2**: `WM_LBUTTONUP` 加进 OnTrayNotification 后会改变既有"双击 Settings"的用户体感（既有行为：双击托盘 = 弹 WeaselDeployer 设置）。spec 036 加新分支：单击 = QuickPanel，双击 = 既有 Settings（保留）。
- **R3**: QuickPanelDialog 用 `MessageBox` 类 win32 弹窗有 DPI 缩放问题（DPI 150/200 时文字会模糊）。v0 接受此限制（已 ship WeaselPanel 也只支持 DPI 100）。
- **R4**: `RegisterHotKey` 在已注册的情况下 RegisterHotKey 会失败（ERROR_HOTKEY_ALREADY_REGISTERED）。v0 直接 `RegisterHotKey` 而不检查；v0.20 加 fallback（log + 提示用户）。

## 6. Anti-patterns (AP-036-A/B/C/D)

- **AP-036-A**: 引入新依赖（D2D / WPF / Qt / Dear ImGui）。spec 004 §5 全局约束禁止；YAGNI。
- **AP-036-B**: 把 QuickPanelDialog 写成 mac 风控件库的前体（提取抽象基类、画虚接口）。YAGNI；v0.20 重构。
- **AP-036-C**: 跳过 TestQuickPanelDialog 直接 ship。R6 证据先于断言。
- **AP-036-D**: 修改 install.nsi（v0 不需要改安装脚本；新版 release 自带新 WeaselServer.exe + new WeaselPanel behavior；install 路径不变）。

## 7. Cross-references

- L04（spec 005 hotkey test） — `RegisterHotKey` 同机制；spec 036 复用 L04 验证模式。
- L24（link-probe pattern） — TestQuickPanelDialog 不直接链接 WeaselTrayIcon.cpp（WTL/ATL/Gdiplus cost），用 L25 mock pattern。
- L25（mock WndProc pattern） — TestQuickPanelDialog 用 test-local WndProc 验证 WM_HOTKEY dispatch。
- L40 — spec 036 不改 install.nsi，避免 L40 PRD/TDD corruption trap。
- L42 — TestQuickPanelDialog 验证 production code（QuickPanelDialog.cpp）确实被 link 到 weasel.dll / WeaselServer.exe，不允许 production code dead-strip。
- spec 033（FluxingDarkModeBridge） — 已 ship；spec 036 引用其 Palette 数据结构，但 v0 不做订阅（v0.20 接入）。
- spec 006 — 完整 mac 风面板设计；spec 036 是 v0 ship 切片，spec 037/038+ 是后续迭代。

## 8. spec 037+ 规划（v0.20+ 路线图，不在本 spec 范围）

- spec 037 — FluxingComponents 基础控件库（Button/Toggle/Panel），mac 风渲染栈。
- spec 038 — FluxingPanelHost 独立进程 + 8-12 入口 grid 布局。
- spec 039 — QuickPanelDialog 暗色主题订阅 FluxingDarkModeBridge + 200ms 渐变。
- spec 040 — QuickPanelDialog 拖动 + 多 DPI 验证清单。
- spec 041 — Alt+, 热键冲突检测（共享数据库）+ 重映射 UI。

## X. 变更日志

- 2026-07-04: spec 036 创建。YAGNI 切片；spec 006 完整设计推迟到 v0.20+。