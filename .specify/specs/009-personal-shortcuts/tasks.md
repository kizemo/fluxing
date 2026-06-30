# 009 · Tasks · 个人快捷短语

## Phase 1 · 基础

- [ ] T001 [P1] 新建 `FluxingPersonalShortcuts/PhrasesStore.{h,cpp}`（json 读写 + 读写锁 + use_count 持久化）
- [ ] T002 [P1] 新建 `FluxingPersonalShortcuts/PinyinHint.{h,cpp}`（基于 RIME 引擎 API）
- [ ] T003 [P1] 新建 `FluxingPersonalShortcuts/PhrasesWindow.{h,cpp}`（列表 UI，spec 006 组件复用）

## Phase 2 · 编辑

- [ ] T004 [P1] 新建 `FluxingPersonalShortcuts/PhraseEditWindow.{h,cpp}`（编辑 / 新增弹窗）
- [ ] T005 [P1] 修改 `WeaselServer/WeaselServerApp.cpp` 注册 `Alt+K` 全局热键

## Phase 3 · 集成

- [ ] T006 [P1] 更新 `weasel.sln` + `xmake.lua` 加 `FluxingPersonalShortcuts` project
- [ ] T007 [P1] 接入 `FluxingPanelHost` 二级窗口
- [ ] T008 [P1] 暗色主题订阅（spec 004 §9.4 集成点表）

## Phase 4 · 验证

- [ ] T009 [P1] 新建 `test/TestPhrasesStore.cpp` + `TestPinyinHint.cpp`
- [ ] T010 [P1] `xbuild.bat weasel installer` → 0 errors
- [ ] T011 [P1] 手动验证 3 平台 3 DPI

## Phase 5 · 提交 & Release

- [ ] T012 [P1] commit：`feat(fluxing): spec 009 personal shortcuts (phrases)`
- [ ] T013 [P1] release `fluxing-0.21.0.0-installer.exe` 推 `kizemo`