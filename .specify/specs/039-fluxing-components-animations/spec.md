# 039 · 火流猩输入法 v2 · FluxingComponents 动画扩展 (hover/press 状态 + Slider/Dropdown/Checkbox/Radio 新控件)

> 元 spec 006 的**第三阶段 ship 切片**。spec 037 ship 了 4 个基础控件 + D2DRenderer + FluxingTheme (无 hover/press 状态), spec 038 ship 了 QuickPanelDialog 重构使用这些控件。本 spec 是 spec 037 plan 2.4 提到的 "200ms 动画扩展" 加上 4 个新控件 (Slider/Dropdown/Checkbox/Radio) 的 v1 ship 切片。

## 0. 上下文

- spec 037 (0.18.26.0) ship 了 FluxingButton / FluxingToggle / FluxingPanel / FluxingLabel 4 个基础控件 + D2DRenderer + FluxingTheme。但 4 个控件都只有 normal 状态, 没有 hover/press 视觉反馈。
- spec 037 plan 2.4 提到 "200ms slide animation" 是 v0.18.26 范围内最低动画, 完整 hover/press 状态机推迟到本 spec 039。
- spec 038 (0.18.27.0) ship 了 QuickPanelDialog 重构使用这些控件, 但 UI 静态。spec 039 让 UI 活起来。
- spec 006 完整设计包含 Slider / Dropdown / Checkbox / Radio 4 个新控件, 这是 mac 风 UI 的标准控件集。spec 039 v1 切片只 ship 这 4 个控件 + hover/press 状态, 推迟 grid 布局 (spec 040+) 和多 DPI 验证 (spec 041+)。

## 1. 产品视角

### 1.1 目标

让 spec 037 的 4 个基础控件加上 hover/press 视觉状态 (200ms 渐变过渡), 同时 ship Slider / Dropdown / Checkbox / Radio 4 个 mac 风新控件。QuickPanelDialog 在后续 spec 040 重构使用新控件做 8-12 入口 grid 布局。

### 1.2 用户故事

- US039-A [P1]: FluxingButton 鼠标 hover 时, 背景色 200ms 渐变到 hilited_back 颜色 (spec 033 dark bg 0x2D2D30)。鼠标离开 200ms 渐变回 normal。
- US039-B [P1]: FluxingButton 鼠标按下 (WM_LBUTTONDOWN) 时, 背景色立即切到 pressed_back 颜色 (0x1F1F22), 松开后 200ms 渐变回 hover 或 normal。
- US039-C [P1]: FluxingToggle 鼠标 hover 时, 滑轨颜色 200ms 渐变到 hover 颜色 (0x353539)。按下时立即切到 pressed 颜色 (0x404045)。
- US039-D [P1]: FluxingPanel (Card style) 鼠标 hover 时, 边框色 200ms 渐变到 hilited_border (0x3F3F45)。
- US039-E [P1]: FluxingLabel (Large size) 鼠标 hover 时, 文字色 200ms 渐变到 hilited_text (0xE8E8EA)。
- US039-F [P1]: 新 FluxingSlider - 横向 200x24 滑块, 圆角 12px, 圆头 20px 直径, 拖动 SetValue(int 0-100) 触发 OnChanged(int) 回调。风格匹配 FluxingButton (Primary/Secondary)。
- US039-G [P1]: 新 FluxingDropdown - 200x30 下拉按钮, 默认显示 placeholder 文字, 点击弹出菜单 (HWND 子窗口), 选中项触发 OnSelected(int index)。
- US039-H [P1]: 新 FluxingCheckbox - 200x20 方框, 默认 unchecked (空方框), 点击切换 checked (打勾), 触发 OnChanged(bool checked)。
- US039-I [P1]: 新 FluxingRadio - 200x20 圆圈, 默认 unchecked (空圆圈), 点击切换 checked (实心圆点), 触发 OnChanged(bool checked)。

### 1.3 验收

- Given Fluxing v0.18.27.0 已安装, QuickPanelDialog 弹出,
- When 鼠标 hover ASCII toggle button,
- Then 200ms 内背景色渐变到 hilited_back (观察可视变化, 也可 byte-verify D2D render target 调用)。
- And 4 个新控件 (Slider/Dropdown/Checkbox/Radio) 在 TestFluxingComponents v2 项目中各 ≥ 4 assertions PASS。
- And 4 个老控件 hover/press 状态在 TestFluxingHover 项目中 ≥ 8 assertions PASS (每个老控件 2 个 assertion: hover on, hover off)。

## 2. Out of scope (v1 不做)

- FluxingPanelHost 独立进程 (spec 040+)。
- Grid 布局 (8-12 入口, spec 040+ 才用 Slider/Dropdown)。
- 多 DPI 验证 (DPI 100/150/200, spec 041+)。
- 拖动支持 (spec 042+)。
- Dropdown 嵌套子菜单 (YAGNI, 单层足够 v1)。

## 3. 依赖

- spec 037 FluxingComponents 4 控件 + D2DRenderer + FluxingTheme (0.18.26.0 ship ✓)。
- spec 038 QuickPanelDialog 重构使用 Fluxing 控件 (0.18.27.0 ship ✓)。
- spec 033 FluxingDarkModeBridge (0.18.22.0 ship ✓)。
- spec 036 QuickPanelDialog v0 (0.18.24.0 ship ✓, 为 spec 038 重构前提供基础)。

## 4. 风险

- R039-A: D2D render target 在 hover 状态快速切换时可能掉帧。缓解: 使用 200ms timer 而非 D2D 原生 interpolation。
- R039-B: Slider 拖动时 OnChanged 回调频率过高可能影响性能。缓解: OnChanged 在 WM_MOUSEMOVE 中节流到 16ms (60 FPS)。
- R039-C: Dropdown 子菜单在多 DPI 下定位可能错位。v1 仅在 96 DPI (100%) 验证, 多 DPI 推迟 spec 041。
- R039-D: Radio button group 互斥需要外部管理, 控件本身只管单选状态。缓解: TestFluxingComponents v2 提供 RadioGroup helper。

## 5. 推迟 (deferred) 原因

- Grid 布局: 需要 FluxingPanelHost 重构 (spec 040+), YAGNI v1 不做。
- 多 DPI: 需要在 DPI 100/150/200 系统切换验证, v1 仅 96 DPI baseline。
- 拖动: 需要 DWM 拖动 API + 窗口 z-order 管理, 与 hover 状态机交互复杂, v2+ 切片。
- 子菜单: Dropdown 嵌套子菜单需要 popup window stacking, YAGNI v1。
