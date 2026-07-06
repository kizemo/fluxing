# 041 - FluxingComponents 多 DPI 验证（v0.18.28+）

> 元 spec 006 的**DPI 验证切片**。spec 037 R2 / spec 038 plan §3 明确推迟
> "DPI 100/150/200 三档验证" 到 v0.18.28+。spec 039 R039-C 也指出 "Dropdown
> 在高 DPI 下定位可能错位。v1 仅在 96 DPI 验证，高 DPI 推迟 spec 041"。

## 0. 上下文

- v0.18.27.2 ship 后用户报告：在 144 DPI（1.5x 缩放）显示器上，QuickPanelDialog
  通过 Alt+, 调出后视觉错乱（黑顶条、文字偏右、看不到按钮）。v0.18.27.3/0.18.27.4
  hotfix 多次尝试修复均失败（L52 lessons-learned）。
- spec 037 R2 明确说 "v0 不实现 resize handler" — 即 v0.18.27.x control library
  不考虑 DPI awareness。
- spec 041 必须实现完整的 D2D PerMonitor V1 DPI 处理路径：
  - WM_DPICHANGED handler on each control
  - ID2D1HwndRenderTarget backing store 物理像素
  - D2D 绘制坐标用 physical client rect
- 现状：WeaselServer/WeaselServer.cpp:36 调用 SetProcessDpiAwareness(
  PROCESS_PER_MONITOR_DPI_AWARE)。所有 child HWND 由 Windows DPI virtualize
  到 physical surface（logical / scale）。

## 1. 产品视角

### 1.1 目标

- 修复 v0.18.27.2 visual bug：在 144 DPI 显示器上，QuickPanelDialog 视觉正确
  （标题"Quick Panel"完整显示，CardPanel 与 dialog 背景区分可见，Toggle 椭圆形
  位置正确，Deploy 按钮文字完整可见）。
- 控件库 FluxingComponents（Label/Panel/Button/Toggle）在 100/125/150/175/200
  DPI 下视觉一致。
- QuickPanelDialog 在 DPI 切换时（用户跨显示器）正确 resize + 重新渲染。
- 不引入新依赖、不破坏 spec 037/038 既有测试。

### 1.2 用户故事

- US041-A [P1]: FluxingComponents 4 控件在 144 DPI 显示正确（标题、卡片、按钮、Toggle 视觉无误）
- US041-B [P1]: FluxingD2DRenderer::CreateHwndRenderTarget 用 physical pixel size 创建 backing store
- US041-C [P1]: 4 控件添加 WM_DPICHANGED handler 调 ID2D1HwndRenderTarget::Resize(&new_size)
- US041-D [P2]: TestFluxingComponents 添加 DPI 100/150/200 测试用例（每个 DPI ≥ 2 assertions）
- US041-E [P2]: TestQuickPanelDialog DPI 150 baseline 截图回归测试
- US041-F [P3]: QuickPanelDialog 在 DPI 切换时（move between monitors）正确 resize + repaint

### 1.3 验收

- Given Fluxing v0.18.28.0 安装在 144 DPI 显示器，
- When 用户按 Alt+, 调出 QuickPanelDialog，
- Then 标题"Quick Panel"完整显示（不溢出 child HWND），
- And CardPanel 与 dialog 背景有视觉区分（不同颜色或可见 border），
- And Toggle 椭圆形位置正确（轨道居中，圆点 on/off 位置明显），
- And Deploy 按钮文字"Deploy"完整可见。
- And TestFluxingComponents 在 DPI 100/150/200 各 ≥ 2 assertions PASS。
- And 现有 spec 036 TestQuickPanelDialog 10/10 PASS 不变。
- And spec 038 TestQuickPanelRefactor 16/16 PASS 不变。

## 2. 范围

### 2.1 改动文件

- `WeaselUI/FluxingComponents/D2DRenderer.h` - 添加 `GetPhysicalClientRect` helper
- `WeaselUI/FluxingComponents/D2DRenderer.cpp` - CreateHwndRenderTarget 用 physical size
- `WeaselUI/FluxingComponents/Label.cpp` - HandlePaint 用 physical rect + WM_DPICHANGED
- `WeaselUI/FluxingComponents/Panel.cpp` - 同上
- `WeaselUI/FluxingComponents/Button.cpp` - 同上
- `WeaselUI/FluxingComponents/Toggle.cpp` - 同上
- `WeaselUI/FluxingComponents/targetver.h` - 升 _WIN32_WINNT 到 WIN10（GetDpiForWindow）
- `test/TestFluxingComponents/TestFluxingTheme.cpp` - 加 DPI 100/150/200 test
- `test/TestFluxingComponents/TestFluxingButton.cpp` - 同上
- `test/TestFluxingComponents/TestFluxingPanel.cpp` - 同上
- `test/TestFluxingComponents/TestFluxingToggle.cpp` - 同上
- `test/TestFluxingComponents/TestFluxingLabel.cpp` - 同上
- `test/TestFluxingComponents/TestFluxingMain.cpp` - DPI test 主入口
- `weasel.sln` - 加新 DPI test config entries（如果需要）
- `CHANGELOG.md` - v0.18.28.0 entry
- `.specify/memory/lessons-learned.md` - 追加 L52 DPI lessons
- `release/fluxing-0.18.28.0-installer.exe` - 新 ship 二进制

### 2.2 不在范围

- 不改 WeaselServer/QuickPanelDialog.cpp 的尺寸（spec 038 anti-pattern 仍生效）；
  QP 物理 200×100 在 144 DPI 仍小，但控件库修复后视觉应大幅改善。
- 不引入 GDI+/Skia/WPF 等新 D2D 替代栈。
- 不实现 spec 040+ 的 hover/press 200ms 渐变（spec 039 范围）。
- 不实现拖动支持（spec 042+ 范围）。

## 3. Anti-patterns

- **AP-041-A**: 修改 spec 037 控件 API 签名（保持 FluxingLabel::Create 等 4 个
  static factory 方法签名不变）
- **AP-041-B**: 修改 QuickPanelDialog.cpp（QP 自身代码超出 spec 041 范围）
- **AP-041-C**: 引入 DPI 检测的 singleton 全局状态（用 HWND per-window DPI 局部处理）
- **AP-041-D**: 在 spec 041 修改 install.nsi（L09 BOM trap 仍生效）
- **AP-041-E**: 把 D2DRenderer::CreateHwndRenderTarget 改成全局变量（保持
  per-HWND cache）

## 4. Constitution Check (per spec 037 plan §2.8)

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | §1.1 明确用户价值 + 验收条件 |
| II. Test | OK | 复用 spec 037 TestFluxingComponents + 加 DPI test |
| III. Spec-Artifact | OK | spec + plan + tasks 3 件套 |
| IV. Clarification | OK | 无 [NEEDS CLARIFICATION] |
| V. Incremental | OK | YAGNI 切片，仅 DPI 修复 |
| R1-R9 | OK | 引用 L24/L31/L46/L47 + 新增 L52 |
| P1-P8 | OK | P8 brand-fork scope (fluxing:) |

## 5. 风险

- **R1**: D2D HwndRenderTarget 在 PerMonitor V1 DPI aware 进程里 backing store
  size 必须用 physical pixel。spec 037 v0 用了 logical size，导致 144 DPI 上视觉错乱。
  修复路径：GetClientRect → MulDiv by 96/dpi → 物理像素 → CreateHwndRenderTarget。
  风险：与 v0.18.27.0/1/2 ship 行为有 breaking change，所有 spec 037/038 测试需重跑。
- **R2**: D2D TextFormat 字号 17pt = 22.67 DIP（DIP 是 device-independent pixel，
  1 DIP = 1/96 inch），与 backing store physical pixel 1:1 映射在 96 DPI 时正确；
  在 144 DPI 时 backing store 物理小但 D2D 仍按 DIP 渲染，文字 glyph 物理像素
  反而变小（缩放比例 1/1.5）。需确认 17pt 文字视觉大小一致（需要 backing store
  物理 size + DIP → physical 转换正确）。
- **R3**: WeaselServer SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) 已
  ship；不改此调用（spec 041 范围外）。
- **R4**: QuickPanelDialog 物理 200×100 在 144 DPI 下太小装不下控件；
  spec 041 不改 QP 自身（AP-041-B），接受控件修复后视觉改善但不完美的现实。
  完整 QP 重设计留给 spec 044+。
- **R5**: WM_DPICHANGED 在 D2D 控件中的实现必须先 ReleaseHwndRenderTarget
  再调 CreateHwndRenderTarget，否则 D2DERR_RECREATE_TARGET race condition。
  L48 已经记录 FluxingD2DRenderer atexit crash，需避免相同 pattern。

## 6. 完成定义 (v0.18.28.0 ship)

- [ ] 6.1 spec 041 spec.md/plan.md/tasks.md 3 文件齐全
- [ ] 6.2 D2DRenderer 添加 GetPhysicalClientRect(HWND, RECT*) helper
- [ ] 6.3 CreateHwndRenderTarget 用 physical pixel size 创建 backing store
- [ ] 6.4 4 控件 HandlePaint 用 GetPhysicalClientRect 取 D2D 绘制坐标
- [ ] 6.5 4 控件添加 WM_DPICHANGED handler 调 Resize(&new_size)
- [ ] 6.6 WeaselUI/targetver.h 升 _WIN32_WINNT 到 WIN10（GetDpiForWindow 可用）
- [ ] 6.7 TestFluxingComponents 加 DPI 100/150/200 测试用例（每个 DPI ≥ 2 assertions）
- [ ] 6.8 weasel.sln 加新 test config entries（如果 DPI test 是独立项目）
- [ ] 6.9 xbuild.bat weasel installer → 0 errors
- [ ] 6.10 scripts/test-infra/run-test-suite.bat → 全部 PASS
- [ ] 6.11 跑 AGENTS.md §2.5 smoke test 验证 installer layout
- [ ] 6.12 在 144 DPI 显示器上截图验证 QP 视觉正确（标题完整、CardPanel 可见、
  Toggle 椭圆位置对、Deploy 按钮文字完整）
- [ ] 6.13 升 v0.18.28.0: weasel.props + env.bat
- [ ] 6.14 commit + push + 追加 L52 lessons-learned
- [ ] 6.15 release/fluxing-0.18.28.0-installer.exe ship

## 7. Cross-references

- L43 (per-target /LTCG:OFF) - 测试项目避免被 dead-strip
- L46 (msbuild path parity) - 三路径验证（xbuild + msbuild + test suite）
- L47 (BOM cascade) - 不改 install.nsi
- L48 (FluxingD2DRenderer atexit crash) - ReleaseHwndRenderTarget 必须在
  WM_DPICHANGED 中正确调用，避免 atexit race
- L52 (per-window DPI handling pattern) - 新 lessons

## 8. 路线图

- spec 039 (v0.18.27+) - hover/press 200ms 渐变 + 4 新控件 (Slider/Dropdown/
  Checkbox/Radio)
- spec 040 (v0.18.27+) - Grid 布局 8-12 入口
- spec 041 (v0.18.28+) - **本 spec DPI 验证**
- spec 042 (v0.18.28+) - 拖动支持
- spec 043 (v0.18.29+) - FluxingPanelHost 独立进程

## X. 变更日志

- 2026-07-06: spec 041 创建。L52 调试总结：spec 037 R2 推迟 DPI 到 v0.18.28+，
  spec 041 必须实现完整 PerMonitor V1 D2D DPI 处理。