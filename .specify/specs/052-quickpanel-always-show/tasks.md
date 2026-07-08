# spec 052 任务清单 - QuickPanel 长显模式

## P1 (must)

- [ ] T001: QuickPanelDialog.h 加 Mode 枚举 + 新方法声明 (30 min)
- [ ] T002: QuickPanelDialog.cpp 加 SetMode / SetOpacity / EnableAlwaysShowMode / ToggleMode (1.5 h)
- [ ] T003: QuickPanelDialog.cpp 加 WM_MOUSEMOVE / WM_MOUSELEAVE / WM_NCMOUSEMOVE handlers (1 h)
- [ ] T004: resource.h 加 ID_QUICKPANEL_ALWAYS_SHOW (5 min)
- [ ] T005: WeaselServerApp.cpp 修改 Alt+ handler + 加启动时触发 (30 min)
- [ ] T006: xmake build 0 errors 0 warnings (10 min)
- [ ] T007: 跑测试套件 (5 min)
- [ ] T008: 重打 installer v0.18.31.0 (5 min)
- [ ] T009: commit + CHANGELOG (10 min)
- **总计**: 4.25 h

## 验收

- [ ] T-A1: xmake -y 0 errors 0 warnings
- [ ] T-A2: TestQuickPanelDialog 10/10 PASS（不回归）
- [ ] T-A3: installer 包含新行为（字符串搜索 ToggleMode / EnableAlwaysShowMode 命中）
- [ ] T-A4: 重启输入法服务即可生效（无需重启系统）
- [ ] T-A5: 切到火流猩 → 面板自动显示 20%
- [ ] T-A6: 鼠标悬停 → 100%
- [ ] T-A7: Alt+, → 隐藏/显示切换

## 状态

- 2026-07-08: spec 052 创建 (本 session)
- 2026-07-08: 实施 T001-T009 (待做)