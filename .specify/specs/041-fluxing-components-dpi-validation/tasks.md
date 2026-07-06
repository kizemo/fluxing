# spec 041 任务清单 - FluxingComponents 多 DPI 验证（v0.18.28+）

## P1 (must)

- [ ] T001: 升 WeaselUI/FluxingComponents/targetver.h 到 _WIN32_WINNT_WIN10 (GetDpiForWindow)
- [ ] T002: D2DRenderer.h 添加 `static void GetPhysicalClientRect(HWND, RECT*)` 声明
- [ ] T003: D2DRenderer.cpp 实现 GetPhysicalClientRect (MulDiv by 96/dpi)
- [ ] T004: D2DRenderer.cpp CreateHwndRenderTarget 用 physical pixel size 创建 backing store
- [ ] T005: Label.cpp HandlePaint GetClientRect → GetPhysicalClientRect
- [ ] T006: Label.cpp 添加 WM_DPICHANGED handler (Release rt + InvalidateRect)
- [ ] T007: Panel.cpp HandlePaint GetClientRect → GetPhysicalClientRect
- [ ] T008: Panel.cpp 添加 WM_DPICHANGED handler
- [ ] T009: Button.cpp HandlePaint GetClientRect → GetPhysicalClientRect
- [ ] T010: Button.cpp 添加 WM_DPICHANGED handler
- [ ] T011: Toggle.cpp HandlePaint GetClientRect → GetPhysicalClientRect
- [ ] T012: Toggle.cpp 添加 WM_DPICHANGED handler
- [ ] T013: 升级 TestFluxingComponents/test/Test* 项目的 targetver.h 到 WIN10
- [ ] T014: TestFluxingButton.cpp 加 DPI 100/150/200 测试用例 (3 assertions)
- [ ] T015: TestFluxingPanel.cpp 加 DPI 100/150/200 测试用例
- [ ] T016: TestFluxingToggle.cpp 加 DPI 100/150/200 测试用例
- [ ] T017: TestFluxingLabel.cpp 加 DPI 100/150/200 测试用例
- [ ] T018: TestFluxingTheme.cpp 加 DPI 100/150/200 测试用例
- [ ] T019: xbuild.bat weasel installer → 0 errors
- [ ] T020: scripts/test-infra/run-test-suite.bat → 全部 PASS (15+1=16+ tests)
- [ ] T021: 升 v0.18.28.0 version (weasel.props + env.bat)
- [ ] T022: 跑 AGENTS.md §2.5 smoke test 验证 installer layout
- [ ] T023: 在 144 DPI 显示器上截图验证 QP 视觉正确
- [ ] T024: commit + push to kizemo/Fluxing
- [ ] T025: 追加 L52 lessons-learned (DPI handling 完整 pattern)
- [ ] T026: ship release/fluxing-0.18.28.0-installer.exe

## P2 (should)

- [ ] T027: TestQuickPanelDialog 加 DPI 150 baseline 截图回归测试
- [ ] T028: TestQuickPanelRefactor 加 DPI 150 集成测试

## P3 (nice)

- [ ] T029: QP dialog DPI 切换 move-between-monitors 测试

## 估计

- T001-T013 (production code DPI 修复): 4-6 小时
- T014-T018 (DPI 测试用例): 2-3 小时
- T019-T022 (build + smoke): 1 小时
- T023 (visual validation): 0.5 小时
- T024-T026 (release ship): 0.5 小时
- **总计**: 8-11 小时

## 依赖

- spec 037 (v0.18.26.0) ship - 4 控件 + D2DRenderer
- spec 038 (v0.18.27.0) ship - QuickPanelDialog 重构
- L48 FluxingD2DRenderer atexit crash lesson
- L52 调试总结 (本 spec 范围新增)