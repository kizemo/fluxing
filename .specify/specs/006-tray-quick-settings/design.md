# 006 · 火流猩输入法 v2 · 托盘快速设置面板

> 范围：把"点击托盘图标"或"按 `Alt+,`"映射到一个 mac 风的快速设置面板；旧托盘 7 级菜单保留作为兼容回退入口（隐藏在"更多 →"二级菜单里）。

## 0. 上下文

- `WeaselServer/WeaselServerApp.cpp` 现有 `ID_WEASELTRAY_*` 一组命令，全部调 `execute/explore/open/check_update`。
- 当前托盘菜单有 11 项：Settings / Dict Manager / Sync / User Folder / Program Folder / Log Folder / Help / Forum / CheckUpdate / Deploy / Quit。
- 候选面板用高质量视觉渲染在 `WeaselUI/WeaselPanel`；新建的 mac 风窗口**复用其渲染栈**（不引入新依赖）。

## 1. 产品视角

### 1.1 触发方式

| 触发 | 行为 |
|---|---|
| **左键单击托盘图标** | 弹出快速设置面板（默认行为） |
| **右键托盘图标** | 保留旧菜单（"退出"等"次要"项） |
| **`Alt+,`** 全局热键 | 弹出快速设置面板（即使输入法未激活） |
| 面板已开时再次触发 | 关闭面板（toggle） |
| **ESC** | 关闭面板 |
| 面板失焦 | 1s 后自动关闭（防止遮挡） |

### 1.2 面板内容

快速设置面板（**8-12 个最常用入口**）：

```
┌─────────────────────────────────────────────────┐
│  ◐  Fluxing                            ⚙ 更多  │
├─────────────────────────────────────────────────┤
│  ⌨  中/英           简/繁           全/半角       │
│  ▣   ────●           ──●──           ──●──         │
├─────────────────────────────────────────────────┤
│  🅰  当前方案：朙月拼音                          │
│  拼   English                       Pinyin       │
├─────────────────────────────────────────────────┤
│  📖  同步状态：● 已同步 · 1 小时前   ↻ 立即同步  │
├─────────────────────────────────────────────────┤
│  📁  用户文件夹       📂  程序文件夹              │
│  ⚙  偏好设置         ⌨  快捷键                   │
│  📜  部署             ⏻  退出                     │
└─────────────────────────────────────────────────┘
```

每项行为：
- 中/英 / 简繁 / 全半角 = 切换 RIME 选项（`SetOption`）
- 当前方案 = 弹出方案选择列表（弹二级面板）
- 同步状态 = 显示云同步状态（P1 显示"未配置"占位；P2 spec 010 接入）
- 用户文件夹 / 程序文件夹 = 调 `explore` 命令打开
- 偏好设置 = 打开 spec 007 配置 UI（待实施）
- 快捷键 = 打开 spec 007 快捷键页
- 部署 = 调 `WeaselDeployer.exe /deploy`
- 退出 = 调 `WeaselServer.exe /q`
- ⚙ 更多 = 展开旧托盘菜单的所有项（兼容回退）

### 1.3 用户故事

- **US2-A** [P1]：用户在任何 app 焦点下按 `Alt+,` → 弹出 mac 风快速设置面板。
- **US2-B** [P1]：点击面板上的"中/英"切换 → 输入法立即切到英文，再点切回中文。
- **US2-C** [P1]：面板启动时间 ≤ 100ms（preloaded）。
- **US2-D** [P1]：面板支持鼠标拖动、ESC 关闭、失焦 1s 自动关闭。

### 1.4 验收

- Given Fluxing v2.0.0 安装并运行
- When 用户在 notepad 按 `Alt+,`
- Then 在屏幕右下角（系统托盘上方）弹出 mac 风面板，自绘渲染，启动 ≤ 100ms
- And 面板顶部显示三个开关：中/英、简/繁、全/半角；当前态显示亮色高亮
- And 点击"中/英" → 切换成功，托盘图标变 ASCII 态

## 2. 技术视角

### 2.1 新增模块

| 模块 | 路径 | 角色 |
|---|---|---|
| `FluxingPanelHost` | `FluxingPanelHost/main.cpp`, `AppWindow.h/.cpp` | 子 exe 进程；接受命令行参数决定显示哪个面板 |
| `FluxingComponents` | `FluxingComponents/{Button,List,KeyCap,TextField,Panel,Toggle}.{h,cpp}` | mac 风基础控件库 |
| `FluxingComponents/Theme` | `FluxingComponents/Theme.{h,cpp}` | 主题（亮/暗/自定义色板）；复用 weasel.yaml `style.color_scheme` |
| `FluxingPanelHost/QuickSettings` | `QuickSettingsWindow.h/.cpp` | 快速设置面板 UI 实现 |
| `FluxingIPCClient` | `RimeWithWeasel/FluxingIPCClient.{h,cpp}` | 复用 WeaselIPC 的 client，与 WeaselServer 通信（已有 Reuse） |
| `FluxingDarkModeBridge` | `FluxingPanelHost/DarkModeBridge.{h,cpp}` | 接收 `WM_SETTINGCHANGE`（来自 WeaselServer 转发的广播） |

### 2.2 进程模型

- **WeaselServer 启动时** = `CreateProcess("FluxingPanelHost.exe", "FluxingPanelHost.exe --preload", ...)`，但**不显示窗口**（隐藏态 + 资源预热）。
- **触发时** = WeaselServer 通过进程间 `WM_COPYDATA` 发送"show QuickSettings" → FluxingPanelHost 切到显示态。
- **关闭时** = FluxingPanelHost 保留进程（不退出），窗口销毁，资源保留 → 下次显示 ≤ 100ms。
- **WeaselServer 退出时** = 关闭 FluxingPanelHost。

> 这个模型参考了现有 `WeaselServer` 内嵌 UI（`m_ui = new weasel::UI`），但把 UI 拆到独立进程以隔离崩溃面 + 便于 mac 风/暗色独立迭代。

### 2.3 全局热键

- `RegisterHotKey(HWND_BROADCAST, ID_HOTKEY_QUICK_SETTINGS, MOD_ALT, VK_OEM_COMMA)`。
- `ID_HOTKEY_QUICK_SETTINGS` = WeaselServer 主窗口收到 → 通过 `WM_COPYDATA` 转发给 FluxingPanelHost。
- 已占用冲突检测：若 `Alt+,` 被其他 app 注册，spec 007 UI 提示用户"已占用"，并提供"重映射"按钮（默认改 `Alt+;`）。

### 2.4 数据流

```
用户按 Alt+,  或 左键托盘
  → WeaselServer 收到事件
  → FluxingIPCClient 发 "show QuickSettings" 给 FluxingPanelHost
  → FluxingPanelHost 创建/显示 QuickSettingsWindow
  → QuickSettingsWindow::Initialize 读 weasel.yaml + 当前 status
  → 自绘渲染
  → 用户点击"中/英"
  → QuickSettingsWindow 发 IPC "SetOption ascii_mode" 给 WeaselServer
  → WeaselServer 调 rime_api->set_option
  → 托盘图标 + 候选面板状态同步
```

### 2.5 验证步骤

1. **TDD**：
   - `TestQuickSettingsWindow.cpp` — mock IPC 客户端，断言：
     - 收到 "show QuickSettings" 命令后窗口显示。
     - 点击"中/英"按钮后发 "SetOption ascii_mode"。
     - 主题色板切换无残留资源泄漏。
2. **手动验证**：
   - 装 Fluxing v2.0.0，托盘图标左键 → 面板弹出（≤ 100ms）。
   - 按 `Alt+,` → 面板弹出。
   - 切到 dark mode → 面板 200ms 渐变切色。
   - DPI 100 / 150 / 200 各验证一次。
3. **回归**：
   - 旧托盘右键菜单仍然可用。
   - 旧 `WeaselServer /q` 退出仍然工作。

## X. 暗色主题集成（F11 横切）

本 spec 涉及的所有 mac 风窗口都必须在 v2.0.0 整合时支持暗色主题。具体集成点：

- **主题源**：`%LocalAppData%\Fluxing\weasel.yaml` 的 `style.color_scheme`；新增 `theme.light` / `theme.dark` 双套色板。
- **触发**：监听 Windows `WM_SETTINGCHANGE` (lParam = `SPI_SETDESKWALLPAPER` 等) 主题变更 → 走 `FluxingDarkModeBridge` 广播给所有 mac 风窗口。
- **过渡**：色板切换 200ms 渐变（使用 `ID2D1SolidColorBrush` 的 `ColorF` 插值）。
- **存储**：颜色缓存按主题名索引（`LightColors` / `DarkColors`），切换时换指针；不重新分配资源。
- **DPI**：暗色切换不触发 `dpiScaleLayout` 重新计算。
- **候选面板**：与本 spec 的 mac 风窗口同步（共享色板缓存）。
- **测试**：`TestDarkModeBridge.cpp` mock `WM_SETTINGCHANGE`，断言所有订阅窗口收到回调 + 色板指针更新。

涉及文件：
- `FluxingComponents/Theme.{h,cpp}`（spec 006 引入，**所有 spec 共用**）
- `FluxingPanelHost/DarkModeBridge.{h,cpp}`（spec 006 引入）
- `RimeWithWeasel/WeaselUtility.{h,cpp}` 加主题切换广播
- `WeaselUI/WeaselPanel.cpp` 接收主题切换

依赖：spec 006（Theme/DarkModeBridge 必先 ship），spec 008/009/007 在 006 之后 ship。

## 3. Out of scope

- 不实现 P2 云同步状态显示（spec 010 接入）。
- 不实现 mac 风切换开关（默认 mac 风；spec 007 加切换）。
- 不做"自定义面板布局"（v2 固定布局）。

## 4. 完成定义

- [ ] T001 `FluxingPanelHost/main.cpp` + `AppWindow.h/.cpp` 骨架
- [ ] T002 `FluxingComponents/Button,List,KeyCap,TextField,Panel,Toggle.h/.cpp`
- [ ] T003 `FluxingComponents/Theme.h/.cpp` + 亮/暗色板
- [ ] T004 `FluxingPanelHost/QuickSettingsWindow.h/.cpp` 主面板
- [ ] T005 `FluxingIPCClient.h/.cpp` IPC 通信
- [ ] T006 `FluxingPanelHost/DarkModeBridge.h/.cpp` + `WeaselServerApp` 加广播
- [ ] T007 `WeaselServer/WeaselServerApp.cpp` 启动 PanelHost + 注册 `Alt+,` 热键
- [ ] T008 `WeaselServer/WeaselServer.rc` 托盘菜单（左键 = show QuickSettings）
- [ ] T009 `weasel.sln` + `xmake.lua` 加新项目（FluxingPanelHost）
- [ ] T010 `test/TestQuickSettingsWindow.cpp` 单测通过
- [ ] T011 手动验证 3 平台 3 DPI
- [ ] T012 commit：`feat(fluxing): spec 006 tray quick settings panel`
- [ ] T013 release `fluxing-0.20.0.0-installer.exe`