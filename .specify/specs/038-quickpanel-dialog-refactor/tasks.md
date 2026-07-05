# 038 · Tasks · QuickPanelDialog 重构

> 每条 1-3 文件, < 4h. spec 037 ship 后下一个 spec.

## Phase 1 · 改造 QuickPanelDialog

- [ ] T001 [P1] 改写 WeaselUI/QuickPanelDialog.h - 用 fluxing::ui::Fluxing* unique_ptr
- [ ] T002 [P1] 改写 WeaselUI/QuickPanelDialog.cpp - CreateControls 实例化 4 个 Fluxing 控件
- [ ] T003 [P1] HandleCreate / HandleCommand 适配 (ascii_toggle 直接调 callback; close button 仍 WM_COMMAND)
- [ ] T004 [P1] Hide() 流程: 先 reset 4 个 unique_ptr (destructor KillTimer) 再 DestroyWindow
- [ ] T005 [P1] L31 fix-coverage audit: WeaselUI.vcxproj + WeaselUI/xmake.lua 已含 4 个 Fluxing 控件 (spec 037 ship), 0 changes

## Phase 2 · TestQuickPanelRefactor

- [ ] T006 [P1] 新建 test/TestQuickPanelRefactor/ 目录
- [ ] T007 [P1] 新建 test/TestQuickPanelRefactor/stdafx.h + .cpp + targetver.h (L47 BOM-free)
- [ ] T008 [P1] 新建 test/TestQuickPanelRefactor/TestQuickPanelRefactorMain.cpp (入口, namespace fluxing_test)
- [ ] T009 [P1] 新建 test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp (5 assertions, L24 link-probe pattern)
- [ ] T010 [P1] 新建 test/TestQuickPanelRefactor/TestQuickPanelRefactor.vcxproj (L31 IntDir 修复 + WeaselUI + WeaselUI\FluxingComponents include path)
- [ ] T011 [P1] weasel.sln 加 TestQuickPanelRefactor 1 project + 4 config entries (L47 byte-level patch)

## Phase 3 · 三路径验证

- [ ] T012 [P1] xbuild.bat weasel installer -> 0 errors, 16 test projects visible
- [ ] T013 [P1] msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32 -> 0 errors
- [ ] T014 [P1] scripts\test-infra\run-test-suite.bat -> 16/16 PASS, ALL TESTS PASSED
  - TestQuickPanelDialog 10/10 (spec 036 回归)
  - TestQuickPanelRefactor 5/5 (spec 038 new)

## Phase 4 · 字节 + 架构验证

- [ ] T015 [P1] L42 byte-verify: 0x001E1E1E 仍在 weasel.dll (spec 033 dark-mode palette 未 dead-strip)
- [ ] T016 [P1] L14 arch-verify: 6 binary (WeaselServer / WeaselDeployer / WeaselSetup / weasel.dll / rime.dll = 0x14C, weaselx64.dll = 0x8664)
- [ ] T017 [P1] L47 BOM audit: WeaselUI/QuickPanelDialog.h+.cpp + TestQuickPanelRefactor/*.h+*.cpp + weasel.sln 全部 BOM=0 (vcxproj/sln) 或 BOM=1 (NSIS) 或 BOM=0 (.bat)
- [ ] T018 [P1] L47 escape audit: 全部新文件 byte-grep 0x5C 0x72 0x5C 0x6E 0 次匹配 (literal \r\n 陷阱)

## Phase 5 · Release v0.18.27+

- [ ] T019 [P1] 升 v0.18.27+ (v0.18.27.0): weasel.props (VERSION_PATCH=27) + env.bat (FLUXING_VERSION=0.18.27, WEASEL_BUILD=0, RELEASE_BUILD=1)
- [ ] T020 [P1] xbuild.bat weasel installer 重新生成 (fluxing-0.18.27.0-installer.exe)
- [ ] T021 [P1] 复制 output/archives/fluxing-0.18.27.0-installer.exe -> release/fluxing-0.18.27.0-installer.exe
- [ ] T022 [P1] scripts/test-infra/run-test-suite.bat: 15 -> 16 test projects (TestQuickPanelRefactor 加入 build + run 列表)
- [ ] T023 [P1] commit + push to kizemo/Fluxing (含 CHANGELOG.md + release/ + production code + test code)
- [ ] T024 [P1] tag v0.18.27.0 + push kizemo

## Phase 6 · Lessons + Spec

- [ ] T025 [P1] L48 lessons-learned 追加 (QuickPanelDialog 集成 spec 037 控件的实际经验)
- [ ] T026 [P1] spec 039 bootstrap (下一 spec: FluxingComponents 动画扩展 spec 037 plan 2.4 200ms slide 已实现, spec 039 可能做 hover/press 状态)

## Anti-patterns to avoid (per spec 037 plan §2.8 + spec 038 spec §3)

- [ ] AP-038-A: do NOT 在 QuickPanelDialog 内部重新实现 dark-mode 切换 (FluxingTheme 已经做)
- [ ] AP-038-B: do NOT 修改 spec 037 4 个控件以适配 QuickPanelDialog
- [ ] AP-038-C: do NOT 修改 install.nsi (L09 BOM trap)
- [ ] AP-038-D: do NOT 重构 FluxingComponents 的 vcxproj 配置 (L31 + L47 已 fix)
- [ ] AP-038-E: do NOT 把 QuickPanelDialog 重写为 dialog resource (保留动态创建 child HWND 模式)
- [ ] AP-038-F: do NOT 在 Hide() 后让 FluxingToggle 的 200ms 动画 timer 继续运行 (unique_ptr reset 前先 KillTimer, T004 已覆盖)