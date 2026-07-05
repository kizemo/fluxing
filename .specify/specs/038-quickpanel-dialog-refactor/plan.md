# 038 · Plan · QuickPanelDialog 重构

> spec 037 FluxingComponents v0 的第一个真实消费者。YAGNI 切片：只替换控件,
> 不改交互行为或位置 / 尺寸。

## 1. 技术上下文

- 复用 spec 037 的 4 个生产控件 + FluxingTheme (订阅 dark-mode 切换)
- WeaselUI 主项目已经 link 了 FluxingComponents (spec 037 ship), 所以
  QuickPanelDialog.cpp 只需 `#include "FluxingComponents/Button.h"` 等
- QuickPanelDialog 当前文件: `WeaselUI/QuickPanelDialog.{h,cpp}` (用原生 Win32
  button / static / 自绘背景)

## 2. 架构

### 2.1 QuickPanelDialog 接口不变

```cpp
class QuickPanelDialog {
 public:
  QuickPanelDialog();
  ~QuickPanelDialog();
  void Show(HWND parent);
  void Hide();
  void Toggle();
  // 现有 spec 036 API 100% 保留 (TestQuickPanelDialog 10/10 PASS 不变)
 private:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT HandleCreate();
  LRESULT HandleCommand(WPARAM, LPARAM);
  LRESULT HandleKeyDown(WPARAM);
  LRESULT HandleKillFocus();
  LRESULT HandleTimer();
  LRESULT HandleDestroy();
  void CreateControls(HWND hwnd);
  void DestroyControls();

  HWND hwnd_ = nullptr;
  // spec 037 重构: 用 Fluxing* unique_ptr 替代原生 HWND
  std::unique_ptr<fluxing::ui::FluxingButton> deploy_button_;
  std::unique_ptr<fluxing::ui::FluxingToggle> ascii_toggle_;
  std::unique_ptr<fluxing::ui::FluxingLabel> title_label_;
  std::unique_ptr<fluxing::ui::FluxingPanel> card_panel_;
  // 关闭按钮保留原生 (spec 038 不重写)
  HWND close_button_ = nullptr;
};
```

### 2.2 CreateControls 改造

```cpp
void QuickPanelDialog::CreateControls(HWND hwnd) {
  RECT client;
  GetClientRect(hwnd, &client);
  RECT title_rc = {10, 5, client.right - 10, 25};
  title_label_ = fluxing::ui::FluxingLabel::Create(
      hwnd, title_rc, L"Quick Panel",
      fluxing::ui::FluxingLabel::FontSize::Large);

  RECT card_rc = {5, 30, client.right - 5, client.bottom - 5};
  card_panel_ = fluxing::ui::FluxingPanel::Create(
      hwnd, card_rc, fluxing::ui::FluxingPanel::Style::Card);

  // ASCII toggle in card panel
  RECT toggle_rc = {15, 10, 75, 30};
  ascii_toggle_ = fluxing::ui::FluxingToggle::Create(
      card_panel_->Hwnd(), toggle_rc, /*initial=*/false);
  ascii_toggle_->SetOnChanged([this](bool on) {
    // ... existing ASCII toggle logic
  });

  // Deploy button (Primary style, right side)
  RECT deploy_rc = {client.right - 110, 5, client.right - 15, 35};
  deploy_button_ = fluxing::ui::FluxingButton::Create(
      card_panel_->Hwnd(), deploy_rc, L"Deploy",
      fluxing::ui::FluxingButton::Style::Primary);
  deploy_button_->SetOnClick([this]() {
    // ... existing Deploy trigger logic
  });
}
```

### 2.3 HandleCreate 改造

保留 WM_TIMER + WM_KILLFOCUS auto-close 逻辑不变. WM_COMMAND 仍由原生 close
button (IDCANCEL) 触发; FluxingButton::SetOnClick 直接调用 callback, 不走
WM_COMMAND (per spec 037 L24: FluxingButton::HandleLButtonUp 直接 fire on_click).

### 2.4 dark-mode 切换

无需任何代码 - FluxingTheme::Instance() 已经在 spec 037 ship, 4 个 Fluxing 控件
在 Create() 时自动 Subscribe, dark-mode 切换时自动 InvalidateRect。

## 3. 测试

### 3.1 保留 spec 036 TestQuickPanelDialog

10/10 PASS 不变. 验证 ID_QUICKPANEL_BTN_ASCII / _DEPLOY / IDCANCEL WM_COMMAND
仍然触发 (因为 spec 037 的 FluxingButton::SetOnClick 内部 fire on_click_, 但
我们的 HandleCommand 仍接收 IDCANCEL from native close button, 并且 ascii_toggle
直接触发 on_changed_ callback).

### 3.2 新增 TestQuickPanelRefactor

5 assertions (per spec 037 plan §2.7 link-probe pattern):
- T1: QuickPanelDialog::Show() 成功创建 4 个 Fluxing 控件 (Hwnd() != nullptr)
- T2: ascii_toggle_->IsOn() == false 初始状态
- T3: ascii_toggle_->SetOn(true) 触发 OnChanged callback
- T4: dark-mode 切换 (写 HKCU + bridge refresh) 后 4 个控件自动 InvalidateRect
- T5: deploy_button_->GetStyle() == Primary (visual style)

### 3.3 TestQuickPanelRefactor.vcxproj

mirror TestFluxingComponents.vcxproj (L31 fix + L47 byte-level 修复):
- IntDir 用 `$(SolutionDir)\msbuild\...` (有 `\`)
- IncludeDirectives: WeaselUI + WeaselUI\FluxingComponents + RimeWithWeasel
- Link: 实际生产代码 via direct ClCompile (Button.cpp, Toggle.cpp, Panel.cpp,
  Label.cpp, FluxingTheme.cpp, D2DRenderer.cpp, FluxingDarkModeBridge.cpp,
  QuickPanelDialog.cpp) - NOT mirror

### 3.4 Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec.md §1.1 明确 |
| II. Test | OK | 5 assertions + spec 036 10 assertions 回归 |
| III. Spec-Artifact | OK | spec + plan + tasks |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | YAGNI 切片 |
| R1-R9 | OK | 引用 L24 / L25 / L26 / L31 / L42 / L43 / L46 / L47 |
| P1-P8 | OK | P8 brand-fork scope |

## 4. 风险

- **R1**: FluxingButton 的 child HWND 是 WS_CHILD | WS_VISIBLE, 但 spec 037 创建时
  没有 WS_EX_TRANSPARENT. QuickPanelDialog 现有 click-outside 逻辑可能误捕获
  FluxingButton 内部 click. 修复: 在 WM_NCHITTEST 透传 FluxingButton 的 hwnd_。
- **R2**: spec 036 ASCII toggle 触发后立即 Hide(). spec 037 FluxingToggle 内部
  启动 200ms 动画 timer. 如果 toggle 触发后立即 Hide, 动画 timer 仍在跑, 会
  触发已 destroy 的 HWND 的 WM_TIMER -> crash. 修复: Hide() 时先 ascii_toggle_
  reset() (unique_ptr destructor 调 Destroy + KillTimer)。
- **R3**: FluxingButton SetOnClick 直接 fire callback (不走 WM_COMMAND), 但
  spec 036 TestQuickPanelDialog T2-T3 期望 WM_COMMAND 触发. 解决: spec 036 的
  WM_COMMAND 测试保持原状 (用原生 close button), 4 个 Fluxing 控件不走
  WM_COMMAND. TestQuickPanelRefactor 新增直接调用 callback 的测试.