# 037 · Plan · FluxingComponents 控件库 v0

> YAGNI 切片。spec 006 完整设计的 1/N 实施。spec 038 才用本 spec 的控件。

## 1. 技术上下文

- **D2D / DirectWrite**（与现有 WeaselUI 相同栈）：ID2D1Factory / ID2D1HwndRenderTarget / IDWriteTextFormat。
- **修改文件**：WeaselUI/WeaselUI.vcxproj（加 7 个新 .cpp 到 ClCompile）、WeaselUI/xmake.lua（同步加 7 个 .cpp）、weasel.sln（加 TestFluxingComponents 项目）。
- **新建文件**：
  - WeaselUI/FluxingComponents/FluxingTheme.h + .cpp
  - WeaselUI/FluxingComponents/Button.h + .cpp
  - WeaselUI/FluxingComponents/Toggle.h + .cpp
  - WeaselUI/FluxingComponents/Panel.h + .cpp
  - WeaselUI/FluxingComponents/Label.h + .cpp
  - WeaselUI/FluxingComponents/D2DRenderer.h + .cpp
  - WeaselUI/FluxingComponents/stdafx.h（PCH for the FluxingComponents subdir; mirror WeaselUI stdafx.h）
- **测试**：test/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h, TestFluxingComponents.vcxproj}。
- 不引入新依赖（d2d1 / dwrite / windowscodecs 已在 WeaselUI 现有 link 配置中）。
- 不修改 install.nsi。

## 2. 架构

### 2.1 命名空间与目录

`
WeaselUI/
  FluxingComponents/
    stdafx.h
    FluxingTheme.h
    FluxingTheme.cpp
    Button.h
    Button.cpp
    Toggle.h
    Toggle.cpp
    Panel.h
    Panel.cpp
    Label.h
    Label.cpp
    D2DRenderer.h
    D2DRenderer.cpp
`

namespace fluxing::ui，class 前缀 Fluxing。

### 2.2 FluxingTheme 单例

`cpp
namespace fluxing::ui {
  class FluxingTheme {
  public:
    static FluxingTheme& Instance();
    fluxing::Palette CurrentPalette() const;  // 调 FluxingDarkModeBridge
    using Callback = std::function<void(fluxing::Palette const&)>;
    using Handle = size_t;
    Handle Subscribe(Callback cb);
    void Unsubscribe(Handle h);
  private:
    FluxingTheme();  // 构造时调 FluxingDarkModeBridge::Subscribe
    // ... 内部订阅者列表
  };
}
`

- 构造时调 FluxingDarkModeBridge::Get().Subscribe([this](auto pal) { NotifyAll(pal); })。
- NotifyAll 调每个 callback 一次（同步，UI 线程）。
- 内部用 std::vector<std::pair<Handle, Callback>> 存订阅者；Subscribe 返回递增 Handle，Unsubscribe 移除。

### 2.3 FluxingButton

`cpp
namespace fluxing::ui {
  class FluxingButton {
  public:
    enum class Style { Primary, Secondary, Destructive };
    static std::unique_ptr<FluxingButton> Create(
      HWND hwndParent, RECT const& rect, std::wstring const& label, Style style);
    void Destroy();
    void SetOnClick(std::function<void()> cb);
    void SetLabel(std::wstring const& label);
  private:
    // 内部：HWND + ID2D1HwndRenderTarget + ID2D1SolidColorBrush cache
  };
}
`

- 内部创建 child HWND（WS_CHILD | WS_VISIBLE），class name FluxingButtonClass。
- WM_PAINT handler 调 D2D 渲染：圆角矩形 6px（用 ID2D1RoundedRectangleGeometry 或 path）+ 文本（IDWriteTextFormat）。
- 颜色从 FluxingTheme::Instance().CurrentPalette() 拿（primary 用 hilited_back，secondary 用 back，destructive 用 0xD04545）。
- WM_LBUTTONUP 触发 SetOnClick 注册的 callback。
- 订阅 FluxingTheme 主题变化 → InvalidateRect 重绘。

### 2.4 FluxingToggle

`cpp
namespace fluxing::ui {
  class FluxingToggle {
  public:
    static std::unique_ptr<FluxingToggle> Create(
      HWND hwndParent, RECT const& rect, bool initial);
    void Destroy();
    bool IsOn() const;
    void SetOn(bool b);
    using OnChangedCallback = std::function<void(bool)>;
    void SetOnChanged(OnChangedCallback cb);
  private:
    // 滑动动画 200ms (SetTimer + WM_TIMER 推进 progress 0..1)
  };
}
`

- 内部 child HWND，30x16 圆角矩形。
- 圆点用 ID2D1Ellipse 渲染，按 IsOn() 状态计算 x 偏移。
- WM_LBUTTONUP 翻转状态 + 触发 OnChangedCallback + 启动滑动动画。

### 2.5 FluxingPanel / FluxingLabel

`cpp
namespace fluxing::ui {
  class FluxingPanel {
  public:
    enum class Style { Card, Plain };
    static std::unique_ptr<FluxingPanel> Create(
      HWND hwndParent, RECT const& rect, Style style);
    void Destroy();
  };
  class FluxingLabel {
  public:
    enum class FontSize { Small, Medium, Large };  // 13/15/17
    static std::unique_ptr<FluxingLabel> Create(
      HWND hwndParent, RECT const& rect, std::wstring const& text, FontSize size);
    void Destroy();
    void SetText(std::wstring const& text);
  };
}
`

- FluxingPanel 简单背景填充（Card: 圆角 8px + 微阴影 4px 偏移；Plain: 无）。
- FluxingLabel 单行文本，居中/居左可选（v0 仅居中）。

### 2.6 FluxingD2DRenderer

`cpp
namespace fluxing::ui {
  class FluxingD2DRenderer {
  public:
    static FluxingD2DRenderer& Instance();
    ID2D1Factory* D2DFactory() const;
    IDWriteFactory* WriteFactory() const;
    ID2D1HwndRenderTarget* CreateHwndRenderTarget(HWND hwnd);
    void ReleaseHwndRenderTarget(HWND hwnd);
  private:
    FluxingD2DRenderer();
    // ... 单例管理
  };
}
`

- 内部 ID2D1Factory* + IDWriteFactory* 全局单例。
- CreateHwndRenderTarget 给每个 HWND 创建一个 ID2D1HwndRenderTarget，缓存到 map。
- ReleaseHwndRenderTarget 在 HWND 销毁时调用，避免资源泄漏。

### 2.7 测试策略

- **TestFluxingButton** (4 assertions):
  - T1: Create 返回非空 unique_ptr
  - T2: 模拟 WM_LBUTTONUP 触发 OnClick callback
  - T3: SetLabel 更新 HWND 文本（GetWindowText 验证）
  - T4: 主题变化时 InvalidateRect 被调用（mock WndProc 计数）
- **TestFluxingToggle** (4 assertions):
  - T1: Create 初始状态 IsOn() == initial
  - T2: SetOn(true) 改变状态 + 触发 OnChanged
  - T3: WM_LBUTTONUP 翻转状态
  - T4: 200ms 滑动动画完成后状态稳定
- **TestFluxingPanel** (3 assertions):
  - T1: Create 成功
  - T2: Card style 圆角半径 8px（内部状态）
  - T3: Plain style 无圆角
- **TestFluxingTheme** (3+ assertions):
  - T1: Instance() 返回同一引用
  - T2: Subscribe/Unsubscribe 正确管理订阅者
  - T3: CurrentPalette() 调 FluxingDarkModeBridge
  - T4: dark mode 切换触发订阅者 callback

### 2.8 Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确 v0 切片意图 |
| II. Test | OK | 4 个 TestFluxingComponents ≥ 14 assertions + 回归 13 个 test project |
| III. Spec-Artifact | OK | spec + plan + tasks 3 件套 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | YAGNI 切片；spec 038+ 后续迭代 |
| R1-R9 | OK | 引用 L24 / L25 / L26 / L31 / L36 / L40 / L42 / L43 / L44 / L46 / L47 |
| P1-P8 | OK | P8 brand-fork scope (fluxing:) |

## 3. 风险

- **R1**: D2D 渲染器依赖 d2d1.lib + dwrite.lib，vcxproj / xmake 需正确 link。spec 037 复用 WeaselUI 现有 link 配置。
- **R2**: ID2D1HwndRenderTarget 在窗口 resize / DPI 变化时需 Resize()。v0 不实现 resize handler（v0 调用方传入固定 rect）。
- **R3**: FluxingTheme 订阅 FluxingDarkModeBridge 时若 callback 触发重绘，需保证重绘在 UI 线程（v0 同步调用，不跨线程）。
- **R4**: spec 038 重构 QuickPanelDialog 时，控件 API 可能需调整。spec 037 留 FluxingButton::SetOnClick(std::function<void()>) 让重构无侵入。
- **R5**: D2D 资源泄漏（ID2D1SolidColorBrush 每次 Create 需 Release）。v0 用成员变量缓存，析构时 Release。
- **R6**: 7 个新 .cpp 同步到 vcxproj + xmake。L36 fix-coverage audit 必跑（grep 全部 .cpp 引用）。