# 036 · Plan · 托盘快速面板 v0（单进程内嵌版）

> YAGNI 切片。spec 006 完整设计的 1/N 实施。

## 1. 技术上下文

- **Win32 / WTL / ATL**（与现有 WeaselServer 相同栈）。
- **修改文件**：`WeaselIPCServer/WeaselServerImpl.{h,cpp}`（加 WM_HOTKEY + RegisterHotKey）、`WeaselServer/WeaselServerApp.cpp`（加菜单 handler）、`WeaselServer/WeaselServer.rc`（加菜单项）、`WeaselServer/SystemTraySDK.cpp`（加 WM_LBUTTONUP 分支）、`WeaselServer/WeaselTrayIcon.{h,cpp}`（暴露 WM_LBUTTONUP 回调）。
- **新建文件**：`WeaselServer/QuickPanelDialog.{h,cpp}`（简单 win32 弹窗）、`test/TestQuickPanelDialog/{TestQuickPanelDialog.cpp,stdafx.cpp,stdafx.h,targetver.h,TestQuickPanelDialog.vcxproj}`。
- **修改 vcxproj**：`WeaselServer/WeaselServer.vcxproj`（加 QuickPanelDialog.cpp 到 ClCompile）、`weasel.sln`（加 TestQuickPanelDialog 项目）。
- 不引入新依赖；不修改 install.nsi。

## 2. 架构

### 2.1 进程模型

不拆独立进程（YAGNI）。QuickPanelDialog 是 WeaselServer 进程内的 win32 HWND 弹窗，HWND_MESSAGE-only（不显示在任务栏），位置屏幕右下角系统托盘上方。

### 2.2 触发路径

```
    用户按 Alt+, 或 左键托盘
      → WeaselServerImpl 收到 WM_HOTKEY / ID_WEASELTRAY_QUICK_PANEL
      → m_pRequestHandler->ShowQuickPanel()  (RimeWithWeaselHandler 加新方法)
      → QuickPanelDialog::Show(weasel::Status&)
      → 弹窗显示 ascii_mode toggle + 重新部署按钮
      → 用户点击 toggle → QuickPanelDialog 回调 m_pRequestHandler->SetOption("ascii_mode", ...)
      → WeaselServer 调 rime_api->set_option
      → 托盘图标 + 候选面板状态同步 (tray_icon.Refresh() + m_ui.Update())
    ```

### 2.3 Alt+, 注册位置

`WeaselServerImpl::OnCreate` 中注册。原因：ServerImpl 是唯一在 `m_server.Run()` 消息循环中持续运行的可控 HWND，且 OnCreate 只触发 1 次。`OnDestroy`/`OnEndSystemSession` 中 UnregisterHotKey 清理。

### 2.4 左键单击托盘处理

现状：OnTrayNotification 处理右键 (WM_RBUTTONUP) 和双击 (WM_LBUTTONDBLCLK)，左键单击 (WM_LBUTTONUP) 未处理。spec 036 在 OnTrayNotification 增加 WM_LBUTTONUP 分支：

```cpp
else if (LOWORD(lParam) == WM_LBUTTONUP) {
  // spec 036: left click = show QuickPanel (R2 keeps double-click = settings)
  if (m_pTarget) { ::PostMessage(m_pTarget, WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL, 0); }
}
```

### 2.5 QuickPanelDialog 行为

- 300x150 win32 HWND_MESSAGE-only 弹窗（实际是 WS_POPUP | WS_VISIBLE）
- 2 个按钮：
  - "中/英" toggle button（label 反映当前 ascii_mode，ID 1001）
  - "重部署" button（ID 1002）
- 1 个关闭按钮（X）
- ESC 键 → DestroyWindow
- 失焦 1s → DestroyWindow（用 `SetTimer(hwnd, 1, 1000, NULL)` + `WM_KILLFOCUS` 启动计时）
- 弹窗位置：屏幕右下角（系统托盘上方 20px gap）

### 2.6 验证步骤

1. **TDD** — `TestQuickPanelDialog` 6/6 PASS：
   - T1: `RegisterHotKey` 成功 + `GetRegisteredHotKey` 验证
   - T2: `WM_HOTKEY` dispatch 触发回调（mock WndProc）
   - T3: 弹窗 CreateWindow 成功 + ASCII toggle SetOption 回调
   - T4: ESC → DestroyWindow
   - T5: 失焦 1s → DestroyWindow（mock KillFocus + 推进消息）
   - T6: 重部署按钮 → 启动 WeaselDeployer.exe 进程（mock Process32First）
2. **手动验证**：
   - 装 v0.18.24.0，托盘图标左键 → 弹窗弹出（≤ 100ms）。
   - 按 `Alt+,` → 弹窗弹出。
   - 点击"中/英" → 切中英，托盘图标变 ASCII 态。
   - 按 ESC / 失焦 1s → 弹窗关闭。
3. **回归**：
   - 旧托盘右键菜单 11 项仍可用。
   - 旧左键双击 → WeaselDeployer Settings 仍工作。
   - TestDefaultHotkeys 31/31 PASS。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确 v0 切片意图 |
| II. Test | OK | TestQuickPanelDialog 6/6 + 回归 TestDefaultHotkeys 31/31 |
| III. Spec-Artifact | OK | spec + plan + tasks 3 件套（精简版） |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | YAGNI 切片；spec 037/038+ 后续迭代 |
| R1-R9 | OK | 引用 L04 / L24 / L25 / L40 / L42 |
| P1-P8 | OK | P8 brand-fork scope (fluxing:) |

## 4. 风险

- **R1**: `Alt+,` 冲突（Vim/Sourcetree/IDE）。spec 036 风险登记；v0 不做冲突检测（YAGNI）。
- **R2**: 既有左键双击 = Settings 行为保留（spec 036 加新分支，不改既有）。
- **R3**: 弹窗 DPI 缩放模糊（v0 接受，DPI 100 主用场景）。
- **R4**: `RegisterHotKey` 失败处理（v0 失败不显示 UI；v0.20 加 log）。