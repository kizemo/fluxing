# 006 · Plan · 托盘快速设置面板

## 1. 技术上下文

- **C++ 17 / WTL / ATL / D2D / DirectWrite**（与现有 WeaselUI 同栈）。
- **修改文件**：新增 `FluxingPanelHost/{main,AppWindow,QuickSettingsWindow}.{h,cpp}` + `FluxingComponents/{Button,List,KeyCap,TextField,Panel,Toggle,Theme}.{h,cpp}` + `RimeWithWeasel/{DarkModeBridge,FluxingIPCClient}.{h,cpp}`。
- 修改 `WeaselServer/WeaselServerApp.cpp` 注册 `Alt+,` 全局热键 + 启动 `FluxingPanelHost` 子进程。
- 修改 `WeaselServer/WeaselServer.rc` 托盘菜单加"快速设置"项。
- 不引入新依赖（P3）；`weasel.sln` + `xmake.lua` 加新 project（P3 vsproj mirror）。

## 2. Architecture

- **进程模型**：`FluxingPanelHost.exe` 独立进程；`WeaselServer` 通过 stdin/stdout 走 WeaselIPC 派生命令。
- **F11 暗色主题**：006 是 ship 起点（spec 004 §9.6），`FluxingComponents/Theme` + `FluxingDarkModeBridge` 由 006 引入；007/008/009 ship 后订阅。
- **mac 风渲染栈**：复用 `WeaselUI/WeaselPanel` 的 D2D/DirectWrite 管线（spec 004 §5 UI 资源约束）。
- **冷启动 ≤ 100ms**：靠子进程预热 + 资源预加载（task 阶段验证 SC-003 16ms 渲染预算）。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确"快速设置门面"意图 |
| II. Test | OK | TestDarkModeBridge 引入（006 ship 起点） |
| III. Spec-Artifact | OK | 3 件套齐全 + design.md |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | 006 独立 ship，007/008/009 后到 |
| R1-R9 | OK | 引用 L04 + spec 004 §9 |
| P1-P8 | OK | P8 brand-fork scope (fluxing:) |

## 4. 风险

- **R1**：冷启动 100ms 预算紧。需 task 阶段 benchmark；若超时，调整为 panel 预热常驻。
- **R2**：mac 风渲染栈与现有 WeaselUI 候选面板 D2D 资源冲突；需共享 ID2D1Factory / IDWriteFactory。
- **R3**：Alt+, 与系统 / IDE 已知快捷键冲突（与 spec 007 冲突检测共享数据库）。