# 004 · Tasks · 火流猩输入法 v2 路线图

> 元 tasks。每条 Txxx = 一份子 spec 的 bootstrap。
> 任务粒度：每条 1-3 文件，< 4h。完成定义 = 子 spec 的 design.md 落盘并过 spec-check。

## Phase 1 · 元 spec 落盘

- [x] T001 [P] 写 `004-fluxing-v2-roadmap/spec.md`
- [x] T002 [P] 写 `004-fluxing-v2-roadmap/plan.md`
- [x] T003 [P] 写 `004-fluxing-v2-roadmap/tasks.md`

## Phase 2 · 子 spec bootstrap

- [ ] T004 [P] [US1] 写 `005-default-hotkeys-rev2/design.md`
- [ ] T005 [P] [US2] 写 `006-tray-quick-settings/design.md`
- [ ] T006 [P] [US3] 写 `007-yaml-config-ui/design.md`
- [ ] T007 [P] [US4] 写 `008-candidate-edit/design.md`
- [ ] T008 [P] [US5] 写 `009-personal-shortcuts/design.md`
- [ ] T009 [P] [US7] 写 `010-cloud-sync/design.md`（仅设计，P2）
- [ ] T010 [P] [US7-bootstrap] 写 `011-fluxing-bootstrapper/design.md`（仅设计，P3）

## Phase 3 · 交叉验证

- [ ] T011 spec-check 全部 8 份 spec（004 + 005-011）一致性 / 覆盖 / 不明确项
- [ ] T012 把发现的不明确项 / 冲突 / 缺口整理成"评审问题清单"提交给用户

## Phase 4 · 评审与提交（不擅自 commit）

- [ ] T013 等用户评审 8 份 spec，汇总修订
- [ ] T014 用户拍板后，一次 commit：`docs(spec): add 004-fluxing-v2-roadmap + 5 份 P1 子 spec design + 2 份 P2/P3 设计`

## Phase 5 · 实施期（评审后另起 spec 的 tasks.md）

- 子 spec 005 → 008 → 006 → 009 → 007 → 暗色横切 → 整合 v2.0.0
- 详见各子 spec 自己的 `tasks.md`（评审通过后生成）

## 关键依赖

- 005 独立，可最先 ship
- 008 独立，但部署顺序排在 005 之后
- 006 独立，但 ship 时机为 FluxingPanelHost.exe + FluxingComponents 库就绪
- 009 依赖 006 组件库（list/button 复用）
- 007 依赖 006 + 009
- 暗色横切（F11）作为最后一步统一收口