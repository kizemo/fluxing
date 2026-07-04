# 036 · Tasks · 托盘快速面板 v0（单进程内嵌版）

> 任务清单。每条 1-3 文件，< 4h。

## Phase 1 · 热键 + WndProc

- [x] T001 [P1] `WeaselServer/resource.h` 加 `ID_HOTKEY_QUICK_PANEL` + `ID_WEASELTRAY_QUICK_PANEL` + `ID_QUICKPANEL_BTN_ASCII` + `ID_QUICKPANEL_BTN_DEPLOY` 常量。
- [x] T002 [P1] `WeaselIPCServer/WeaselServerImpl.h` 加 `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` + `LRESULT OnHotkey(...)` 声明。
- [x] T003 [P1] `WeaselIPCServer/WeaselServerImpl.cpp` 加 `OnHotkey` 实现 + `OnCreate` 加 `RegisterHotKey` + `OnDestroy`/`OnEndSystemSession` 加 `UnregisterHotKey`。

## Phase 2 · QuickPanelDialog 实现

- [x] T004 [P1] 新建 `WeaselServer/QuickPanelDialog.h` (API: `class QuickPanelDialog { static void Show(HINSTANCE, HWND parent, weasel::Status const&, std::function<void(bool ascii)>, std::function<void()> onDeploy); }`)
- [x] T005 [P1] 新建 `WeaselServer/QuickPanelDialog.cpp` (win32 CreateWindowEx WS_POPUP + 2 buttons + ESC + 失焦 1s 关闭 + 屏幕右下角定位)。
- [x] T006 [P1] `WeaselServer/WeaselServerApp.cpp::SetupMenuHandlers` 加 `ID_WEASELTRAY_QUICK_PANEL` handler → `QuickPanelDialog::Show(...)`。
- [x] T007 [P1] `WeaselServer/WeaselServerApp.h` 加 forward declare `class QuickPanelDialog;` + 引入 `<functional>`。

## Phase 3 · 托盘左键单击 + 菜单项

- [x] T008 [P1] `WeaselServer/SystemTraySDK.cpp::OnTrayNotification` 加 `WM_LBUTTONUP` 分支 → `::PostMessage(m_pTarget, WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL, 0)`。
- [x] T009 [P1] `WeaselServer/WeaselServer.rc` 在 3 个语言菜单 (SimpChinese/TradChinese/English) 的 SETTINGS 之后插入 "快速面板 (&K)" / "快速面板 (&K)" / "QuickPanel (&K)" 菜单项 (ID_WEASELTRAY_QUICK_PANEL)。使用 (&K) 避免 (&Q) 与退出 (Quit) 冲突。
- [x] T010 [P1] `WeaselServer/WeaselServer.vcxproj` 加 `QuickPanelDialog.cpp` 到 ClCompile 项。

## Phase 4 · 测试

- [x] T011 [P1] 新建 `test/TestQuickPanelDialog/{TestQuickPanelDialog.cpp, stdafx.cpp, stdafx.h, targetver.h}`。
- [x] T012 [P1] 新建 `test/TestQuickPanelDialog/TestQuickPanelDialog.vcxproj` (mirror TestDarkModeBroadcast vcxproj 结构)。
- [x] T013 [P1] `weasel.sln` 加 `TestQuickPanelDialog` 项目 + 4×3 platform configuration entries。
- [x] T014 [P1] `xbuild.bat weasel installer` → 0 errors, 14 test projects visible in build output。
- [x] T015 [P1] `test\TestQuickPanelDialog\Release\TestQuickPanelDialog.exe` → 10/10 PASS (T1a-T1d + T2-T6)。
- [x] T016 [P1] `test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe` → 35/35 PASS (回归)。
- [x] T017 [P1] `test\TestDarkModeBroadcast\Release\TestDarkModeBroadcast.exe` → 14/14 PASS (回归)。

## Phase 5 · Release v0.18.24.0

- [x] T018 [P1] 升 v0.18.24.0: `weasel.props` (VERSION_PATCH=24, PRODUCT_VERSION=0.18.24.0, FILE_VERSION=0.18.24.0) + `env.bat` (FLUXING_VERSION=0.18.24, WEASEL_BUILD=0, RELEASE_BUILD=1)。
- [x] T019 [P1] 跑 AGENTS.md §2.5 smoke test 13/13 PASS (含 L14 arch consistency 6 files)。
- [x] T020 [P1] `xbuild.bat weasel installer` 重新生成 (fluxing-0.18.24.0-installer.exe, 42,651,950 bytes)（带新 version）。
- [x] T021 [P1] 复制 `output/archives/fluxing-0.18.24.0-installer.exe` → `release/fluxing-0.18.24.0-installer.exe`。
- [x] T022 [P1] commit (501a2ce) → push to kizemo/Fluxing (3dcd047..501a2ce) (含 weasel.props? 不, gitignored; 含 CHANGELOG.md + release/fluxing-0.18.24.0-installer.exe + spec 036 docs + production code) → push to kizemo/Fluxing。

## Anti-patterns to avoid (per plan.md §2)

- [ ] AP-036-A: do NOT 引入新依赖 (D2D/WPF/Qt/ImGui)。YAGNI。
- [ ] AP-036-B: do NOT 把 QuickPanelDialog 写成 mac 风控件库前体。v0.20 重构。
- [ ] AP-036-C: do NOT 跳过 TestQuickPanelDialog 直接 ship。
- [ ] AP-036-D: do NOT 修改 install.nsi。v0 release 自带新 WeaselServer.exe。
- [ ] AP-036-E: do NOT 改既有"双击托盘 = Settings"行为。spec 036 加新分支。