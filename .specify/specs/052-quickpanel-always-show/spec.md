# 052 - QuickPanel 长显模式 (v0.18.31.0)

> **修改 v0.18.29.0 spec 045 的 QuickPanelDialog 行为**：从"按需弹出"改为"切到火流猩输入法时自动长显"。
> 设计稿：`docs/design/04-quick-settings-v3-macos.html` (v3-macos 风格已 ship)

## 0. 上下文

- v0.18.29.0 QuickPanel 行为（spec 045）：按 Alt+, 或左键托盘图标**弹出**，1 秒失焦后**自动关闭**。单次交互面板。
- **新需求**（用户 2026-07-08）：
  1. 切到火流猩输入法 → QuickPanel **自动长显**（20% 透明度）
  2. 鼠标悬停 → 100% 不透明
  3. Alt+, → 切换"长显/隐藏"两种模式
  4. 隐藏后再用快捷键 → 重新显示（隐式恢复长显模式）
- 安装新版本无需重启/注销，**重启输入法服务即可**（L48 PPL 进程替代方案）。

## 1. 产品视角

### 1.1 目标

让 QuickPanel 从"工具栏式按需弹出"变成"持续可访问的状态指示器"，符合 macOS 输入法指示器 UX。

### 1.2 用户故事

- US052-A [P1]: 切到火流猩输入法 → 屏幕右下角自动出现 5 行 8 入口面板，20% 透明度（淡灰感）
- US052-B [P1]: 鼠标悬停面板 → 立即变 100% 不透明（清晰可读）
- US052-C [P1]: 鼠标离开 0.5 秒后 → 恢复 20% 透明度
- US052-D [P1]: 按 Alt+, → 面板隐藏（不关闭服务，保留状态）
- US052-E [P1]: 隐藏后按 Alt+, 或再切到火流猩输入法 → 面板重新显示（恢复长显模式）
- US052-F [P1]: 点击面板内任何按钮（toggle/button）→ 临时 100% 不透明 2 秒（操作反馈），之后恢复 20%
- US052-G [P2]: 双击面板标题"⚙ Fluxing" → 触发"切换方案"操作（与现有 spec 045 一致）

### 1.3 验收

- Given 用户安装 v0.18.31.0 并**重启输入法服务**（不需重启系统）
- When 用户切到火流猩输入法（或首次激活）
- Then 屏幕右下角出现 5 行 8 入口面板，20% 透明度（alpha=51）
- And 鼠标移上去 → 100% 不透明（alpha=255）
- And 按 Alt+, → 面板隐藏
- And 再按 Alt+, → 面板重新出现（回到 20% 透明度）
- And 不影响现有的按钮交互（toggle 切换、文件打开、部署、退出）

## 2. 范围

### 2.1 改动文件

- `WeaselServer/QuickPanelDialog.h` - 加 Mode 枚举 + 新方法声明
- `WeaselServer/QuickPanelDialog.cpp` - 加 WS_EX_LAYERED + 透明度切换 + 鼠标追踪
- `WeaselServer/WeaselServerApp.cpp` - 加 `ID_QUICKPANEL_ALWAYS_SHOW` handler + 启动时调用
- `WeaselServer/resource.h` - 加 `ID_QUICKPANEL_ALWAYS_SHOW` 命令 ID
- `WeaselIPCServer/WeaselServerImpl.cpp` - Alt+ handler 改用 ToggleMode

### 2.2 不在范围

- 视觉样式大改（v3-macos 视觉规格已经 ship 0.18.29.0，alpha 是新加的）
- 双击标题切方案（US052-G 推迟到 v0.19）
- 自定义 alpha 阈值（20% 写死，UI 不暴露）
- 把长显状态持久化到注册表（重启后默认长显，不记忆用户的隐藏选择）
- 移动端 / 多显示器适配

## 3. 技术方案

### 3.1 状态机

```cpp
enum class QuickPanelMode {
  kHidden,         // 不可见
  kAlwaysShow,     // 长显 20% (alpha=51)
  kFocused,        // 暂时 100% (alpha=255) - 用户正在操作
};
```

### 3.2 窗口属性

- 现有：`WS_EX_TOPMOST | WS_EX_TOOLWINDOW, WS_POPUP | WS_VISIBLE | WS_BORDER`
- 新加：`WS_EX_LAYERED`（必须！否则 SetLayeredWindowAttributes 失败）
- 新加：调 `SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)` 切换透明度
- 注：WS_EX_LAYERED 启用后窗口不再受 WS_VISIBLE 单独控制，需要 SetLayeredWindowAttributes 或 ShowWindow 配合

### 3.3 鼠标追踪

- `WM_MOUSEMOVE` 首次进入 → 调 `TrackMouseEvent(TME_LEAVE)` 注册 WM_MOUSELEAVE 监听
- `WM_MOUSELEAVE` → 0.5 秒后（WM_TIMER 触发）回 20%
- 鼠标 hover 期间直接 100%

### 3.4 Alt+ 行为

- 旧：`Alt+,` → 调 `QuickPanelDialog::Show()`
- 新：`Alt+,` → 调 `QuickPanelDialog::ToggleMode()`
  - 如果当前 `kAlwaysShow/kFocused` → 隐藏（`kHidden`）
  - 如果当前 `kHidden` → 重新显示（`kAlwaysShow` + 100% 2 秒后回 20%）
- 左键托盘图标 = 同 Alt+（菜单 handler 同样调 ToggleMode）

### 3.5 切到火流猩时的触发

- `WeaselServerApp::Run()` 启动时调 `QuickPanelDialog::EnableAlwaysShowMode()`
- 这个方法创建 QuickPanel（如果不存在）+ 设为 kAlwaysShow + 20% alpha
- 后续用户重启输入法服务时同样触发（service restart 会重新调 Run）

## 4. 风险

| 风险 | 缓解 |
|---|---|
| WS_EX_LAYERED 影响现有窗口消息（WM_PAINT 半透明） | 仅画布 + 文本，用 GDI+ / GDI 直接画，不依赖 layered child 控件 |
| 20% 透明下文字看不清 | 选 20%（alpha=51），20% 是 macOS 隐藏状态的常见值；hover 100% 即可读 |
| Tray icon 左键也调 ToggleMode，但用户习惯单击弹一次 | 保持一致：单击也切换；UX 与 macOS 工具栏一致 |
| QuickPanel 创建一次后再 Show() 会重复创建？ | 加 `s_hwnd` 静态缓存：已存在则只调 SetWindowPos + 改 alpha |
| TSF 服务重启后 Reset alpha | 每次 OnCreate 都调一次 SetLayeredWindowAttributes 设置 alpha=51 |

## 5. 依赖

- v0.18.29.0 spec 045（已 ship，作为基线）
- docs/design/04-quick-settings-v3-macos.html（视觉规格）

## 6. 关联

- spec 045（QuickPanel 8 入口 mac 风格，本次修改其行为层）
- spec 049（v4 视觉设计，本 spec 暂用 v3-macos 视觉）
- lessons-learned L50/L51（spec 037 ship 时 D2D fallback，本 spec 用 GDI 不依赖 D2D）