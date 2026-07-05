# 039 · Tasks · FluxingComponents 动画扩展 + 4 新控件

> 任务清单。每条 1-3 文件, < 4h。Phase 1-2 = hover/press 状态机扩展; Phase 3-4 = 4 新控件; Phase 5-7 = 测试 + release。

## Phase 1 · hover/press 状态机基础

- [ ] T001 [P1] 修改 WeaselUI/FluxingComponents/FluxingTheme.h 加 6 个新颜色常量 (hilited_back / pressed_back / hover_track / pressed_track / hilited_border / hilited_text), 在 CurrentPalette() 暗/亮分支分别填值。
- [ ] T002 [P1] 修改 WeaselUI/FluxingComponents/FluxingTheme.cpp 加 InterpolateColor(from, to, progress) helper (lerp RGB)。
- [ ] T003 [P1] 新建 WeaselUI/FluxingComponents/HoverState.h + .cpp (enum State { Normal, Hover, Pressed } + StartTransition(target_state, duration_ms) + Tick(delta_ms) + GetProgress())。

## Phase 2 · 4 老控件 hover/press 扩展

- [ ] T004 [P1] 修改 WeaselUI/FluxingComponents/Button.cpp 加 hover/press 状态机 + WM_MOUSEMOVE / WM_MOUSELEAVE / WM_LBUTTONDOWN / WM_LBUTTONUP 处理 + HandlePaint 根据 progress 计算背景色。
- [ ] T005 [P1] 修改 WeaselUI/FluxingComponents/Toggle.cpp 加 hover/press 状态机 + 滑轨颜色根据 progress 计算。
- [ ] T006 [P1] 修改 WeaselUI/FluxingComponents/Panel.cpp 加 hover 状态 (Card style 边框色根据 progress 计算)。
- [ ] T007 [P1] 修改 WeaselUI/FluxingComponents/Label.cpp 加 hover 状态 (Large size 文字色根据 progress 计算)。

## Phase 3 · 4 新控件实现

- [ ] T008 [P1] 新建 WeaselUI/FluxingComponents/Slider.h + .cpp (200x24 + 圆角 12px + 圆头 20px + SetValue 0-100 + OnChanged callback + WM_LBUTTONDOWN/MOVE/UP 拖动 + SetCapture/ReleaseCapture)。
- [ ] T009 [P1] 新建 WeaselUI/FluxingComponents/Dropdown.h + .cpp (200x30 + placeholder 文字 + items vector + OnSelected callback + 子菜单 HWND popup + WS_POPUP)。
- [ ] T010 [P1] 新建 WeaselUI/FluxingComponents/Checkbox.h + .cpp (200x20 + 方框 + checked/unchecked + SetChecked + IsChecked + OnChanged callback)。
- [ ] T011 [P1] 新建 WeaselUI/FluxingComponents/Radio.h + .cpp (200x20 + 圆圈 + checked/unchecked + SetChecked + IsChecked + OnChanged callback)。

## Phase 4 · vcxproj + xmake 集成

- [ ] T012 [P1] 修改 WeaselUI/WeaselUI.vcxproj 加 4 新 .cpp + 1 HoverState.cpp 到 ClCompile 项。
- [ ] T013 [P1] 修改 WeaselUI/xmake.lua 加 4 新 .cpp + 1 HoverState.cpp 到 add_files 项。
- [ ] T014 [P1] L36 fix-coverage audit: 验证 5 新 .cpp + 5 改 .cpp 全部被 vcxproj + xmake 引用 (无遗漏)。

## Phase 5 · 测试

- [ ] T015 [P1] 新建 test/TestFluxingHover/{TestFluxingHover.cpp, stdafx.h, stdafx.cpp, targetver.h, TestFluxingHover.vcxproj} (8 assertions: Button/Toggle/Panel/Label × 2 状态)。
- [ ] T016 [P1] 新建 test/TestFluxingComponentsV2/{TestFluxingSlider.cpp, TestFluxingDropdown.cpp, TestFluxingCheckbox.cpp, TestFluxingRadio.cpp, stdafx.h, stdafx.cpp, targetver.h, TestFluxingComponentsV2.vcxproj} (16 assertions: 4 新控件 × 4 assertions)。
- [ ] T017 [P1] weasel.sln 加 TestFluxingHover + TestFluxingComponentsV2 项目 + 各 4×3 platform configuration entries (L48-#1: 2 entries per config = 2 lines)。
- [ ] T018 [P1] scripts\test-infra\run-test-suite.bat 加 TestFluxingHover + TestFluxingComponentsV2 到 build + run 列表 (17 个测试项目从 15 个)。
- [ ] T019 [P1] xbuild.bat weasel installer -> 0 errors, 17 test projects visible in build output。
- [ ] T020 [P1] scripts\test-infra\run-test-suite.bat -> 17/17 PASS, 200+ assertions, "=== ALL TESTS PASSED ==="。

## Phase 6 · 回归 + 字节验证

- [ ] T021 [P1] test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe -> 35/35 PASS (回归)。
- [ ] T022 [P1] test\TestQuickPanelRefactor\Release\TestQuickPanelRefactor.exe -> 8/8 PASS (回归, spec 038 不破)。
- [ ] T023 [P1] test\TestFluxingComponents\Release\TestFluxingComponents.exe -> 35+ assertions PASS (回归, spec 037 不破)。
- [ ] T024 [P1] L42 byte-verify: 0x001E1E1E + 0x002D2D30 + 0x001F1F22 + 0x00353539 全部在 weasel.dll。
- [ ] T025 [P1] L14 arch-verify: 6 binary 全部 arch 一致 (x86=0x14C, x64=0x8664, ARM64=0xAA64)。
- [ ] T026 [P1] L47 byte-verify: 4 新 .h+.cpp + 2 新 vcxproj 全部 byte-healthy (C0=0 C1=0 BOM=False LF 行尾, sln CRLF)。
- [ ] T027 [P1] L48 防御性测试退出模式: TestFluxingHover + TestFluxingComponentsV2 main() 用 ExitProcess(rc) 跳过 atexit static destructors (FluxingD2DRenderer singleton 析构 crash 防护)。

## Phase 7 · Release v0.18.28.0

- [ ] T028 [P1] 升 v0.18.28.0: weasel.props (VERSION_PATCH=28, PRODUCT_VERSION=0.18.28.0, FILE_VERSION=0.18.28.0) + env.bat (FLUXING_VERSION=0.18.28, WEASEL_BUILD=0, RELEASE_BUILD=1)。
- [ ] T029 [P1] xbuild.bat weasel installer 重新生成 (fluxing-0.18.28.0-installer.exe)。
- [ ] T030 [P1] Copy installer: output\archives\fluxing-0.18.28.0-installer.exe -> release\fluxing-0.18.28.0-installer.exe。
- [ ] T031 [P1] Smoke test (AGENTS.md sec 2.5): silent install + 8 invariants PASS + L14 arch-verify 6 binary 一致。
- [ ] T032 [P1] CHANGELOG.md 加 v0.18.28.0-fluxing entry, 列出 5 P1 控件动画 + 4 新控件 + 2 新测试项目 + L48 follow-up。
- [ ] T033 [P1] 5-file commit: spec 039 production code + 2 test projects + CHANGELOG.md + release/fluxing-0.18.28.0-installer.exe (weasel.props + env.bat gitignored)。
- [ ] T034 [P1] git push kizemo Fluxing。
- [ ] T035 [P1] git tag v0.18.28.0 + git push kizemo v0.18.28.0。
- [ ] T036 [P1] L49 lessons-learned 追加 (spec 039 期间新发现, 如有)。
- [ ] T037 [P1] spec 040 bootstrap (FluxingPanelHost grid 布局)。
- [ ] T038 [P1] PRD + TDD 更新 v0.18.28.0 ship 状态 + spec 039 状态 + L48/49 累计。


- [ ] T039 [P1] L46 三路径 hard gate 重跑: xbuild.bat weasel installer (exit 0) + msbuild weasel.sln (0 errors) + run-test-suite.bat (17/17 PASS)。
- [ ] T040 [P1] AGENTS.md sec 5 Pre-Commit Checklist: 5 步全 PASS, 输出粘到 chat response (R6 evidence before assertion)。
