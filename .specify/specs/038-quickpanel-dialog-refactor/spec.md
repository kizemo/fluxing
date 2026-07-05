# 038 · Spec · QuickPanelDialog 重构使用 FluxingComponents

> spec 037 FluxingComponents v0 控件库的下一个消费者。spec 036 Tray QuickPanel v0
> 的 QuickPanelDialog (位于 WeaselUI/QuickPanelDialog.{h,cpp}) 当前用原生 Win32
> button + static + 自绘; spec 038 重构为 spec 037 的 4 个控件 (Button/Toggle/
> Panel/Label), 获得 dark-mode 自动适配 + 圆角 6px + Toggle 200ms 动画 + 一致
> 主题切换。

## 1. Intent

### 1.1 用户价值

QuickPanelDialog 是用户按 Alt+, (spec 036) 弹出的快速面板, 包含 ASCII 模式切换
+ RIME Deploy 触发 + Esc 关闭三个交互入口。当前用原生 Win32 button, dark-mode
切换时不刷新背景, 浅色/深色用户都能看到反差强烈的白底控件, 破坏 dark-mode 体验。

使用 spec 037 的 FluxingButton + FluxingToggle + FluxingPanel + FluxingLabel 重写
后:

- dark-mode toggle 时 4 个控件自动重新着色 (subscribed FluxingTheme)
- 主面板背景变成 FluxingPanel::Card (圆角 8px) 风格
- ASCII 切换从 button 变成 FluxingToggle (圆角矩形 + 圆点 + 200ms slide 动画)
- RIME Deploy 按钮使用 FluxingButton::Primary (filled hilited_back 配色)
- 标题 "Quick Panel" 使用 FluxingLabel::Large (17pt)

### 1.2 验收条件

1. QuickPanelDialog 视觉与 spec 036 截图一致 (3 个交互元素 + 标题 + 关闭按钮)
2. 切 dark/light mode 时 4 个控件自动刷新 (无需手动 WM_PAINT)
3. ASCII toggle 切换有 200ms 圆点 slide 动画
4. 现有 spec 036 TestQuickPanelDialog 10/10 PASS 不变
5. 新增 TestQuickPanelRefactor 覆盖 spec 037 控件在 QuickPanelDialog 上下文的集成

## 2. 范围

### 2.1 改动文件

- `WeaselUI/QuickPanelDialog.h` (rewrite - 使用 fluxing::ui::Fluxing* 类型)
- `WeaselUI/QuickPanelDialog.cpp` (rewrite - 内部实例化 4 个 Fluxing 控件)
- `test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp` (新 - 集成测试)
- `test/TestQuickPanelRefactor/TestQuickPanelRefactor.vcxproj` (新 - L31 fixed)
- `test/TestQuickPanelRefactor/stdafx.h, stdafx.cpp, targetver.h` (新)
- `test/TestQuickPanelRefactor/TestQuickPanelRefactorMain.cpp` (新 - 入口)
- `weasel.sln` (1 project + 4 config entries)
- `scripts/test-infra/run-test-suite.bat` (15 -> 16 test projects)
- `CHANGELOG.md` (新版本条目)

### 2.2 不在范围

- 调整 QuickPanelDialog 的功能 (Alt+, 触发, ASCII toggle, Deploy trigger, Esc
  关闭) - spec 036 已 ship, spec 038 不改行为
- 添加新的交互元素
- 改变 QuickPanelDialog 的位置 / 尺寸 / Z-order

## 3. Anti-patterns (per spec 037 AP-037-D / AP-037-F)

- **AP-038-A**: do NOT 在 QuickPanelDialog 内部重新实现 dark-mode 切换
  (FluxingTheme 已经在做这事)
- **AP-038-B**: do NOT 修改 spec 037 的 4 个控件 (Button/Toggle/Panel/Label)
  以适配 QuickPanelDialog
- **AP-038-C**: do NOT 在 spec 038 修改 install.nsi (L09 BOM trap)
- **AP-038-D**: do NOT 在 spec 038 重构 FluxingComponents 的 vcxproj 配置
  (spec 037 已经 ship, L31 + L47 已经 fix)
- **AP-038-E**: do NOT 把 QuickPanelDialog 重写为 dialog resource (rc)
  - 当前代码用纯 C++ 动态创建 child HWND, 保留这个模式

## 4. Constitution Check (per spec 037 plan §2.8)

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | §1.1 明确用户价值 + 验收条件 |
| II. Test | OK | 复用 spec 036 TestQuickPanelDialog + 新增 TestQuickPanelRefactor |
| III. Spec-Artifact | OK | spec + plan + tasks 3 件套 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | YAGNI 切片, 仅替换控件不增加功能 |
| R1-R9 | OK | 引用 L24 / L25 / L26 / L31 / L42 / L43 / L46 / L47 |
| P1-P8 | OK | P8 brand-fork scope (fluxing:) |

## 5. 风险

- **R1**: FluxingButton/Toggle/Panel/Label 内部都使用自己的 WndProc class name
  (FluxingButtonClass 等), 跟 QuickPanelDialog 现有 WndProc 不冲突
- **R2**: 现有 TestQuickPanelDialog 用 ID_QUICKPANEL_BTN_ASCII 等 command IDs -
  spec 038 保留这些 IDs (作为 FluxingButton 的子 HWND, WM_COMMAND 仍触发)
- **R3**: QuickPanelDialog 之前用 `IsWindow()` 检查 ascii_button_ - spec 038 改为
  `FluxingButton::Hwnd() != nullptr`