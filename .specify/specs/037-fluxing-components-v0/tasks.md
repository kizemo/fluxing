# 037 · Tasks · FluxingComponents 控件库 v0

> 任务清单。每条 1-3 文件，< 4h。

## Phase 1 · 基础设施

- [ ] T001 [P1] 新建目录 WeaselUI/FluxingComponents/。
- [ ] T002 [P1] 新建 WeaselUI/FluxingComponents/stdafx.h（PCH 头；包含 WeaselUI stdafx.h 公共头）。
- [ ] T003 [P1] 新建 WeaselUI/FluxingComponents/D2DRenderer.h（class FluxingD2DRenderer 单例 API）。
- [ ] T004 [P1] 新建 WeaselUI/FluxingComponents/D2DRenderer.cpp（ID2D1Factory + IDWriteFactory 单例 + HWND render target 缓存）。

## Phase 2 · Theme 适配器

- [ ] T005 [P1] 新建 WeaselUI/FluxingComponents/FluxingTheme.h（class FluxingTheme 单例 + Subscribe/Unsubscribe API）。
- [ ] T006 [P1] 新建 WeaselUI/FluxingComponents/FluxingTheme.cpp（构造时调 FluxingDarkModeBridge::Subscribe + 内部订阅者列表 + 同步通知）。

## Phase 3 · 4 个基础控件

- [ ] T007 [P1] 新建 WeaselUI/FluxingComponents/Button.h（class FluxingButton 静态工厂 + SetOnClick + SetLabel API）。
- [ ] T008 [P1] 新建 WeaselUI/FluxingComponents/Button.cpp（HWND child + D2D 圆角 6px + 3 style + 主题订阅 + 重绘）。
- [ ] T009 [P1] 新建 WeaselUI/FluxingComponents/Toggle.h（class FluxingToggle 静态工厂 + IsOn/SetOn + OnChanged callback + 200ms 动画 API）。
- [ ] T010 [P1] 新建 WeaselUI/FluxingComponents/Toggle.cpp（HWND child + 圆点 + 状态翻转 + 滑动动画 timer）。
- [ ] T011 [P1] 新建 WeaselUI/FluxingComponents/Panel.h + .cpp（class FluxingPanel 静态工厂 + Card/Plain style）。
- [ ] T012 [P1] 新建 WeaselUI/FluxingComponents/Label.h + .cpp（class FluxingLabel 静态工厂 + Small/Medium/Large font size）。

## Phase 4 · vcxproj + xmake 集成

- [ ] T013 [P1] 修改 WeaselUI/WeaselUI.vcxproj 加 7 个新 .cpp 到 ClCompile 项（FluxingTheme / Button / Toggle / Panel / Label / D2DRenderer + stdafx.cpp）。
- [ ] T014 [P1] 修改 WeaselUI/xmake.lua 加 7 个新 .cpp 到 add_files 项（spec 036 既有 xmake path 同步）。
- [ ] T015 [P1] L36 fix-coverage audit: 验证 7 个 .cpp 全部被 vcxproj + xmake 引用（无遗漏）。

## Phase 5 · 测试

- [ ] T016 [P1] 新建 test/TestFluxingComponents/{TestFluxingButton.cpp, TestFluxingToggle.cpp, TestFluxingPanel.cpp, TestFluxingTheme.cpp, stdafx.h, stdafx.cpp, targetver.h}。
- [ ] T017 [P1] 新建 test/TestFluxingComponents/TestFluxingComponents.vcxproj（mirror TestDarkModeBroadcast vcxproj 结构，4 个 ClCompile + 1 link + OutDir 用 L31 修复后的 SolutionDir 反斜杠模式）。
- [ ] T018 [P1] weasel.sln 加 TestFluxingComponents 项目 + 4×3 platform configuration entries。
- [ ] T019 [P1] xbuild.bat weasel installer → 0 errors, 15 test projects visible in build output。
- [ ] T020 [P1] scripts\test-infra\run-test-suite.bat → 15/15 PASS, 115+18=133+ assertions, "=== ALL TESTS PASSED ==="。

## Phase 6 · 回归 + 字节验证

- [ ] T021 [P1] test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe → 35/35 PASS (回归)。
- [ ] T022 [P1] test\TestDarkModeBroadcast\Release\TestDarkModeBroadcast.exe → 14/14 PASS (回归)。
- [ ] T023 [P1] L42 byte-verify: 0x001E1E1E 仍在 weasel.dll（spec 033 dark-mode palette bytes 未被新代码 dead-strip）。
- [ ] T024 [P1] L14 arch-verify: 6 个 binary（WeaselServer / WeaselDeployer / WeaselSetup / weasel.dll / rime.dll = 0x14C, weaselx64.dll = 0x8664, weaselARM64.dll = 0xAA64）。

## Phase 7 · Release v0.18.26.0

- [ ] T025 [P1] 升 v0.18.26.0: weasel.props (VERSION_PATCH=26, PRODUCT_VERSION=0.18.26.0, FILE_VERSION=0.18.26.0) + env.bat (FLUXING_VERSION=0.18.26, WEASEL_BUILD=0, RELEASE_BUILD=1)。
- [ ] T026 [P1] xbuild.bat weasel installer 重新生成 (fluxing-0.18.26.0-installer.exe)。
- [ ] T027 [P1] 复制 output/archives/fluxing-0.18.26.0-installer.exe → 
elease/fluxing-0.18.26.0-installer.exe。
- [ ] T028 [P1] commit + push to kizemo/Fluxing (含 CHANGELOG.md + release/fluxing-0.18.26.0-installer.exe + spec 037 docs + production code + test code)。

## Anti-patterns to avoid (per plan.md §2)

- [ ] AP-037-A: do NOT 引入新依赖 (D2D/WPF/Qt/ImGui/Skia)。复用 WeaselUI 现有 D2D/DirectWrite。
- [ ] AP-037-B: do NOT 把 4 个控件抽抽象基类。YAGNI；v0 各控件独立。
- [ ] AP-037-C: do NOT 跳过 4 个 TestFluxingComponents 直接 ship。
- [ ] AP-037-D: do NOT 在本 spec 重构 QuickPanelDialog。留 spec 038。
- [ ] AP-037-E: do NOT 修改 install.nsi。v0 release 自带新 WeaselServer.exe。
- [ ] AP-037-F: do NOT 写虚派生层次（spec 039+ 才需要）。
