# 008 · Tasks · 候选字编辑

## Phase 1 · 核心实现

- [ ] T001 [P1] [US4-A/B] 新建 `RimeWithWeasel/CandidateEdit.{h,cpp}`（删除 + 屏蔽 + 防抖 100ms）
- [ ] T002 [P1] [US4-C] 修改 `RimeWithWeasel/RequestHandler.h` + `RimeWithWeaselHandler.cpp` 加 `RequestDeleteCandidate`
- [ ] T003 [P1] [US4-A/B] 修改 `WeaselUI/WeaselPanel.{h,cpp}` 加 `OnRButtonDown(UINT, CPoint)`

## Phase 2 · fallback 验证

- [ ] T004 [P1] git submodule update librime；验证 `rime_candidate_t::is_user_dict` 字段（spec 008 §2.2）；不存在则用启发式
- [ ] T005 [P1] 验证 RIME 引擎 1.13 `customization` 钩子是否存在；不存在则在 `rime_ice.schema.yaml` 加 speller/algebra patch fallback
- [ ] T006 [P1] 客户端过滤 user_ignore.txt 实现（fallback 方案 B）

## Phase 3 · 集成

- [ ] T007 [P1] 暗色主题订阅（spec 004 §9.4 集成点表）
- [ ] T008 [P1] 托盘面板"恢复"按钮接入（spec 006）

## Phase 4 · 验证

- [ ] T009 [P1] 新建 `test/TestCandidateEdit.cpp`（mock 候选 + mock user_dict_update + 防抖）
- [ ] T010 [P1] `xbuild.bat weasel installer` → 0 errors
- [ ] T011 [P1] 手动验证清单 3 平台 3 DPI
- [ ] T012 [P1] 回归清单（左键上屏 + 右键唤菜单已不再唤出）

## Phase 5 · 提交 & Release

- [ ] T013 [P1] commit：`feat(fluxing): spec 008 candidate right-click delete/ignore`
- [ ] T014 [P1] release `fluxing-0.19.0.0-installer.exe` 推 `kizemo`