# spec 049 计划 - QuickPanelDialog v4 macOS 风格（v0.19.0.0 设计意图）

> **本 plan 只描述"v0.19 实施时"的技术方案**，不在本 session 实施。

## 1. 技术上下文

- **当前实现（v0.18.29.0）**：
  - `WeaselServer/QuickPanelDialog.{h,cpp}` 8 入口
  - 渲染：spec 037 FluxingComponents（Button/Toggle/Panel/Label + D2DRenderer）
  - 大小：300×220
  - 触发：Alt+, + 左键托盘
  - 自动关闭：1s 失焦

- **目标实现（v0.19.0.0）**：
  - 6 入口（schema / dict / phrase / full-half / keyboard / login）
  - 渲染：GDI+ + ColorMatrix 半透明（弃 D2D）
  - 大小：300×36 行（每个按钮一行）— 总高度 ~250
  - 触发：同 v0.18.29.0
  - 自动关闭：3s 失焦（v4 设计调整）

## 2. 技术方案

### 2.1 渲染层（GDI+ 替代 D2D）

```cpp
// QuickPanelDialog.cpp 主体
class QuickPanelDialog {
  static Gdiplus::Image* s_logo;  // resource/fluxing-logo.png
  static Gdiplus::Image* s_iconSchema, s_iconDict, s_iconPhrase,
                          s_iconFullHalf, s_iconKeyboard, s_iconLogin;
  static Gdiplus::ColorMatrix s_vibrancy;  // 4% alpha
  static Gdiplus::SolidBrush* s_brushHilited;  // 0x0A84FF @ 12% alpha
  static Gdiplus::Pen* s_penBorder;  // 14px rounded rect outline
};
```

### 2.2 6 个 icon SVG path 列表（v0.19 实施时确定）

| 按钮 | SF Symbol 候选 | 含义 |
|---|---|---|
| schema | `list.bullet.rectangle` | 方案列表 |
| dict | `book.closed` | 词典 |
| phrase | `text.bubble` | 短语 |
| full-half | `moon.circle` 或 `circle.righthalf.filled` | 全/半角 |
| keyboard | `keyboard` | 软键盘 |
| login | `person.crop.circle` | 登录（v2.1+ cloud sync） |

### 2.3 工具复用（DeployerUiHelper.h）

```cpp
// QuickPanelDialog.cpp 用到
namespace dui = deployer_ui;
dui::EnableDarkTitleBar(s_hwnd);
dui::GetDpiForWindow(s_hwnd);
dui::Scale(20, dpi);  // 20 logical px = scaled to dpi
HFONT hFont = dui::CreateUiFont(13, dpi);  // SF Pro / Microsoft YaHei
dui::BringDialogToFront(s_hwnd);
dui::EnableResizableFrame(s_hwnd);
```

## 3. 任务粒度

- 不在本 spec 拆任务（design-only）
- 真正实施时新建 spec 049-impl

## 4. 依赖

- spec 037/041 FluxingComponents（已 ship）
- spec 033 FluxingDarkModeBridge（已 ship）
- `FluxingConfigEditor/DeployerUiHelper.h` helper（已就位）
- `resource/fluxing-logo.png` logo（已就位）

## 5. 触发条件（实施前要确认）

见 spec.md §8。