# spec 049 任务清单 - QuickPanelDialog v4 macOS 风格

> **Design-only spec**。无实施任务，仅记录设计意图和触发条件。

## P0 (must, design phase)

- [x] T001: 写 spec.md (v3-macos 设计意图 + 6 入口 + 4 设计稿对比)
- [x] T002: 写 plan.md (GDI+ 渲染 + DeployerUiHelper 复用 + 6 icon 候选)
- [x] T003: 写 tasks.md (本文件)
- [x] T004: backup 半成品代码到 output/backup.b-pre-revert/
- [x] T005: 保留 docs/design/ 4 个 HTML 设计稿
- [x] T006: 保留 resource/fluxing-logo.png
- [x] T007: 保留 FluxingConfigEditor/DeployerUiHelper.h

## P1 (实施时, v0.19.0.0)

- [ ] T008: 用 frontend-ui-engineering 技能确定 6 个 icon 的 SVG path
- [ ] T009: 144 DPI 实机截图对比 v3 vs v3-macos，用户评审
- [ ] T010: 写 spec 049-impl/{spec,plan,tasks}.md
- [ ] T011: 重写 QuickPanelDialog.h/.cpp（GDI+ 实现）
- [ ] T012: 6 入口 IPC handler
- [ ] T013: 毛玻璃 + 4% hover + 12% active 验证 100/150/200 DPI
- [ ] T014: 重写 TestQuickPanelDialog（基于 6 入口）
- [ ] T015: ship v0.19.0.0 installer

## P2 (后续)

- [ ] T016: 暗色主题 200ms 渐变（spec 037 动画经验复用）
- [ ] T017: 3s 失焦自动关闭（替代 v0.18.29.0 的 1s）

## 估计

- T001-T007: 已完成（design 阶段 ~1 session）
- T008-T015: v0.19 实施时 ~1-2 周
- T016-T017: 0.5 周

## 依赖

- spec 037/041/033（已 ship）
- 当前 Phase 1（F3/F4/F5）ship 后再启动

## 状态

- 2026-07-07: design 阶段完成（本 session）
- 2026-07-08: 半成品 code revert + 备份（output/backup.b-pre-revert/）
- 触发实施: Phase 1 全部 ship + 用户反馈稳定后