# 037 · 火流猩输入法 v2 · FluxingComponents 控件库 v0（mac 风基础控件）

> 元 spec 006 的**第二阶段 ship 切片**。scope 限制：建立 FluxingComponents 基础控件库（Button / Toggle / Panel / Label）的头文件 + D2D 渲染基础设施，**仅供 spec 038 QuickPanelDialog 重构使用**。后续 spec 039 接入暗色主题、spec 040 多 DPI 验证、spec 041 拖动 + 热键冲突检测。

## 0. 上下文

- spec 006 整体设计 16 个 tasks（含 FluxingComponents / DarkModeBridge / FluxingPanelHost / FluxingIPCClient），是 2-3 周大工程。
- spec 036 已 ship 0.18.24.0：QuickPanelDialog v0（win32 简单弹窗 + ASCII toggle + Deploy 按钮），不依赖本 spec。
- spec 033 已 ship 0.18.22.0：FluxingDarkModeBridge（Palette 单例 + 订阅者通知 + L42 byte-verify 0x001E1E1E 在 weasel.dll）。
- **v0 ship 切片**（本 spec 037）：仅交付 4 个基础控件 + 1 个 D2D 渲染器 + 1 个 Theme 适配器，闭环 v0.18.26.0 release。
- 现状：WeaselServer/QuickPanelDialog.cpp 是 win32 CreateWindowEx + 2 button + ESC + 失焦 1s 关闭（spec 036 v0），**没有** mac 风外观。
- 现状：WeaselUI/WeaselPanel.cpp 是 D2D/DirectWrite 渲染（已有 ID2D1RenderTarget + ID2D1SolidColorBrush 基础），可通过新 FluxingComponents 命名空间复用。

## 1. 产品视角

### 1.1 目标

为后续 mac 风 UI 提供可复用的基础控件库（4 类）+ D2D 渲染抽象层 + 主题适配器。QuickPanelDialog 在 spec 038 重构为 FluxingButton + FluxingToggle + FluxingPanel 组合；本 spec 只 ship 控件库本身，不重构 QuickPanelDialog。

### 1.2 用户故事

- US037-A [P1]：在 WeaselServer/WeaselServerImpl.h 中引入 <FluxingComponents/Button.h>，编译通过。
- US037-B [P1]：调用 FluxingButton::Create(hwndParent, rect, L"中/英", FluxingButton::Style::Primary) 返回一个可绘制的控件 HWND/句柄。
- US037-C [P1]：调用 FluxingPanel::Create(hwndParent, rect, FluxingPanel::Style::Card) 返回一个圆角矩形容器。
- US037-D [P1]：FluxingTheme 适配 FluxingDarkModeBridge 的 CurrentPalette()，亮/暗色变化时通过订阅者自动重绘。
- US037-E [P2]：spec 038 重构 QuickPanelDialog 使用 4 个新控件，UI 看起来像 macOS 弹窗。v0.18.27+ 推迟到本 spec 后续。

### 1.3 验收

- Given Fluxing v0.18.26.0 已安装，
- When 开发者引入 <FluxingComponents/Button.h> 创建按钮，
- Then 按钮以 D2D 渲染（mac 风圆角 6px + 微高光），订阅 FluxingDarkModeBridge 主题色。
- And 切换系统暗色 → 按钮颜色 200ms 内渐变（spec 033 bridge 已在）。
- And 4 个 test project 全 PASS（TestFluxingButton / TestFluxingToggle / TestFluxingPanel / TestFluxingTheme 各 ≥ 4 assertions）。

## 2. Out of scope（v0 不做）

- mac 风 Slider / Dropdown / Checkbox / Radio（spec 039+）。
- FluxingPanelHost 独立进程（YAGNI，单进程足够 v0；spec 040+ 才需要）。
- 完整 8-12 入口 grid 布局（spec 038 才用）。
- DPI 100/150/200 三档验证（spec 041 推迟到 v0.18.28+）。
- 动画过渡（仅 200ms 颜色渐变由 FluxingDarkModeBridge 提供；按钮 hover/press 不做 v0）。
- i18n（按钮 label 字符串仍由调用方传入；v0 不内置 i18n）。

## 3. 依赖

- spec 005 Shift+space 切中英键位（已在 0.18.5+ ship）。
- spec 033 FluxingDarkModeBridge（已在 0.18.22.0+ ship；本 spec 引用其 CurrentPalette() 拿 4 色 + Subscribe() 接主题变化）。
- spec 036 QuickPanelDialog v0（已 ship 0.18.24.0；spec 038 才重构到本 spec 的控件）。
- 现有 WeaselUI/WeaselPanel.cpp D2D/DirectWrite 基础设施（ID2D1Factory / ID2D1HwndRenderTarget / IDWriteTextFormat 复用）。
- Win32 HWND 模型（不引入 XAML / WPF / Qt / Dear ImGui）。
- AGENTS.md §2.5 L14 arch consistency（控件库只能 x86 / weaselx64.dll x64；不引入 ARM64 特殊路径）。

## 4. 完成定义（v0.18.26.0 ship）

- [ ] 4.1 新建目录 WeaselUI/FluxingComponents/（与 WeaselPanel.cpp 同级）。
- [ ] 4.2 新建 WeaselUI/FluxingComponents/FluxingTheme.h（namespace fluxing::ui，class FluxingTheme 单例 + Subscribe(Callback) / CurrentPalette()）。
- [ ] 4.3 新建 WeaselUI/FluxingComponents/FluxingTheme.cpp（实现订阅 FluxingDarkModeBridge + 内部缓存 Palette + 触发重绘回调）。
- [ ] 4.4 新建 WeaselUI/FluxingComponents/Button.h（class FluxingButton 静态工厂 Create(hwnd, rect, label, style) + void Destroy()）。
- [ ] 4.5 新建 WeaselUI/FluxingComponents/Button.cpp（D2D 圆角矩形 6px + 文本 + 3 style: Primary/Secondary/Destructive，订阅 FluxingTheme）。
- [ ] 4.6 新建 WeaselUI/FluxingComponents/Toggle.h + .cpp（class FluxingToggle，状态 bool + 滑动动画 200ms）。
- [ ] 4.7 新建 WeaselUI/FluxingComponents/Panel.h + .cpp（class FluxingPanel，圆角矩形 8px 容器 + 内边距 12px）。
- [ ] 4.8 新建 WeaselUI/FluxingComponents/Label.h + .cpp（class FluxingLabel，单行文本 + 字号 13/15/17 三档）。
- [ ] 4.9 新建 WeaselUI/FluxingComponents/D2DRenderer.h（class FluxingD2DRenderer 抽象工厂 + 单例）。
- [ ] 4.10 新建 WeaselUI/FluxingComponents/D2DRenderer.cpp（包装 ID2D1Factory + IDWriteFactory + ID2D1HwndRenderTarget 创建/复用）。
- [ ] 4.11 修改 WeaselUI/WeaselUI.vcxproj 加 7 个新 .cpp 到 ClCompile 项（FluxingTheme / Button / Toggle / Panel / Label / D2DRenderer + stdafx）。
- [ ] 4.12 修改 WeaselUI/xmake.lua 加 7 个新 .cpp 到 add_files 项（spec 036 已 ship xmake path，spec 037 同步）。
- [ ] 4.13 新建 test/TestFluxingComponents/ 测试项目（4 个独立 .cpp + vcxproj + stdafx）。
- [ ] 4.14 weasel.sln 加 TestFluxingComponents 项目 + 4×3 platform configuration entries。
- [ ] 4.15 xbuild.bat weasel installer → 0 errors, 14+1 test projects visible in build output。
- [ ] 4.16 scripts\test-infra\run-test-suite.bat → 14+1=15/15 PASS, 115+18=133+ assertions, "=== ALL TESTS PASSED ==="。
- [ ] 4.17 跑 AGENTS.md §2.5 smoke test 13+1/13+1 PASS（含 L14 arch consistency 6 files + L42 byte-verify 0x001E1E1E 仍 in weasel.dll）。
- [ ] 4.18 升 v0.18.26.0: weasel.props (VERSION_PATCH=26, PRODUCT_VERSION=0.18.26.0, FILE_VERSION=0.18.26.0) + env.bat (FLUXING_VERSION=0.18.26, WEASEL_BUILD=0, RELEASE_BUILD=1)。
- [ ] 4.19 commit + push to kizemo/Fluxing (含 CHANGELOG.md + release/fluxing-0.18.26.0-installer.exe + spec 037 docs + production code + test code)。

## 5. 风险（v0 ship 切片特有）

- **R1**: D2D 渲染器依赖 d2d1.lib + dwrite.lib，vcxproj / xmake 需正确加 link 标志。spec 037 复用 WeaselUI 现有 link 配置（已含 d2d1/dwrite），风险低。
- **R2**: ID2D1HwndRenderTarget 在窗口 resize / DPI 变化时需 Resize()。v0 不实现 resize handler（v0 调用方传入固定 rect）。
- **R3**: FluxingTheme 订阅 FluxingDarkModeBridge 时若 callback 触发重绘，需保证重绘在 UI 线程（v0 同步调用，不跨线程）。
- **R4**: spec 038 重构 QuickPanelDialog 时，控件 API 可能需调整。spec 037 留 FluxingButton::SetOnClick(std::function<void()>) 让重构无侵入。
- **R5**: D2D 资源泄漏（ID2D1SolidColorBrush 每次 Create 需 Release）。v0 用成员变量缓存，析构时 Release。

## 6. Anti-patterns (AP-037-A/B/C/D/E)

- **AP-037-A**: 引入新依赖（D2D / WPF / Qt / Dear ImGui / Skia）。spec 037 复用 WeaselUI 现有 D2D/DirectWrite，**不引入**新依赖。
- **AP-037-B**: 把 FluxingComponents 写成 mac 风控件库的前体（提取抽象基类、画虚接口、虚派生层次）。v0 4 个控件**各自独立**不抽基类，YAGNI。
- **AP-037-C**: 跳过 4 个 TestFluxingComponents 直接 ship。R6 证据先于断言。
- **AP-037-D**: 在本 spec 重构 QuickPanelDialog。v0 037 不动 QuickPanelDialog，留给 spec 038。
- **AP-037-E**: 修改 install.nsi。v0 release 自带新 WeaselServer.exe + 控件库，新版本号即可。

## 7. Cross-references

- L04（spec 005 hotkey test）— RegisterHotKey 同机制；本 spec 不涉及热键，仅引用 QuickPanelDialog 既有。
- L24（link-probe pattern）— TestFluxingComponents 验证生产代码（FluxingButton.cpp 等）确实被 link 到 weasel.dll，不允许 production code dead-strip。
- L25（mock WndProc pattern）— TestFluxingButton / TestFluxingPanel 用 test-local WndProc 验证 WM_PAINT dispatch。
- L26（mirror drift）— 测试必须 link 实际生产代码（不 mirror），L26 的反面教材。
- L31（vcxproj OutDir path-glue）— TestFluxingComponents.vcxproj 用 $(SolutionDir)\msbuild\... 模式（spec 037 0.18.25.0 已修 3 个旧 vcxproj）。
- L36（fix-coverage audit）— vcxproj 修改后 grep 全部 test 目录，确保无遗漏。
- L40 — spec 037 不改 install.nsi，避免 L40 PRD/TDD corruption trap。
- L42 — TestFluxingComponents byte-verify 控件订阅 FluxingDarkModeBridge 实际生效（用 byte-level search 验证 callback 指针非空）。
- L43 — spec 037 修改 WeaselUI.vcxproj 时保留 per-target /LTCG:OFF 修复。
- L44 — 中文 commit message 走 byte-level write，不经 PS 解析。
- L46 — 三路径验证（xbuild + msbuild + test suite）全部 0 errors / "ALL TESTS PASSED" 才算 ship。
- L47（待 L31 修复后追加）— VsDevCmd 触发 MSB6001 PATH/Path 冲突 + vcxproj $(SolutionDir)msbuild 缺 \ 引发 MSB3491。
- spec 006 — 完整 mac 风面板设计；spec 036 / 037 / 038+ 是分阶段 ship 切片。
- spec 033（FluxingDarkModeBridge）— 已 ship；本 spec 引用其 API。
- spec 036（QuickPanelDialog v0）— 已 ship；本 spec 不重构它，留 spec 038。
- spec 038（QuickPanelDialog 重构）— 计划中的后续 spec，使用本 spec 4 个控件重构 QuickPanelDialog。

## 8. spec 038+ 规划（v0.18.27+ 路线图，不在本 spec 范围）

- spec 038 — QuickPanelDialog 重构（用 FluxingButton + FluxingToggle + FluxingPanel + FluxingLabel 替换 win32 button）。
- spec 039 — FluxingTheme 接入 QuickPanelDialog + 200ms 渐变。
- spec 040 — 多 DPI 验证清单（DPI 100/150/200）+ 弹窗 resize handler。
- spec 041 — FluxingComponents 拖动支持（FluxingPanel::StartDrag）+ Alt+, 热键冲突检测。
- spec 042 — FluxingPanelHost 独立进程 + 8-12 入口 grid 布局。

## X. 变更日志

- 2026-07-05: spec 037 创建。YAGNI 切片；spec 006 完整设计分阶段 ship，spec 037 是控件库 v0，spec 038+ 是后续迭代。