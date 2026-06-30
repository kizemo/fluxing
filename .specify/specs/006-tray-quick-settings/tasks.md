# 006 · Tasks · 托盘快速设置面板

> 任务清单。每条 1-3 文件，< 4h。

## Phase 1 · 基础组件

- [ ] T001 [P1] [US2-A/C] 新建 `FluxingComponents/{Button,List,KeyCap,TextField,Panel,Toggle}.{h,cpp}`（mac 风基础控件）
- [ ] T002 [P1] [US2-C] 新建 `FluxingComponents/Theme.{h,cpp}`（F11 暗色/亮色色板；spec 004 §9.2）
- [ ] T003 [P1] 新建 `RimeWithWeasel/DarkModeBridge.{h,cpp}`（订阅者注册 + WM_SETTINGCHANGE 监听 + 200ms 渐变）
- [ ] T004 [P1] 新建 `RimeWithWeasel/FluxingIPCClient.{h,cpp}`（复用 WeaselIPC 命名管道客户端）

## Phase 2 · 面板 UI

- [ ] T005 [P1] [US2-A/B] 新建 `FluxingPanelHost/{main,AppWindow,QuickSettingsWindow}.{h,cpp}`（面板主窗 + 8-12 入口布局）
- [ ] T006 [P1] [US2-D] 面板支持鼠标拖动、ESC 关闭、失焦 1s 自动关闭
- [ ] T007 [P1] 修改 `WeaselServer/WeaselServerApp.cpp` 注册 `Alt+,` 全局热键 + 启动 FluxingPanelHost 子进程
- [ ] T008 [P1] 修改 `WeaselServer/WeaselServer.rc` 托盘菜单加"快速设置"项（兼容旧 11 项保留在"更多 →"）

## Phase 3 · 集成

- [ ] T009 [P1] 更新 `weasel.sln` + `xmake.lua` 加新 project（FluxingPanelHost / FluxingComponents）
- [ ] T010 [P1] [US2-C] benchmark 冷启动 ≤ 100ms（SC-003 验证）
- [ ] T011 [P1] 新建 `test/TestDarkModeBridge.cpp` mock WM_SETTINGCHANGE 验证订阅者通知

## Phase 4 · 验证

- [ ] T012 [P1] `xbuild.bat weasel installer` → 0 errors
- [ ] T013 [P1] 手动验证清单 3 平台 3 DPI（Win10/11 × DPI 100/150/200）
- [ ] T014 [P1] AGENTS.md §2.5 静默安装 smoke test PASS

## Phase 5 · 提交 & Release

- [ ] T015 [P1] commit：`feat(fluxing): spec 006 tray quick settings`
- [ ] T016 [P1] release `fluxing-0.20.0.0-installer.exe` 推 `kizemo`