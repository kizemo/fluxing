# 007 · Tasks · yaml 可视化设置 UI

## Phase 1 · 基础

- [ ] T001 [P1] [US3-A] 新建 `FluxingConfigEditor/YamlRoundTrip.{h,cpp}`（保留注释 + key 顺序）
- [ ] T002 [P1] 新建 `FluxingConfigEditor/ConflictChecker.{h,cpp}` + 静态 db（json）
- [ ] T003 [P1] 新建 `FluxingConfigEditor/ConfigEditorApp.{h,cpp}`（主窗口 + 左侧导航）

## Phase 2 · 各页

- [ ] T004 [P1] [US3-A] 新建 `FluxingConfigEditor/HotkeyPage.{h,cpp}`（列表 + 按键录制器）
- [ ] T005 [P1] [US3-B] 新建 `FluxingConfigEditor/StylePage.{h,cpp}`（配色 + 透明度 + 字体 + 暗色模式）
- [ ] T006 [P1] [US3-C] 新建 `FluxingConfigEditor/SchemaPage.{h,cpp}`（方案启用 / 停用 / 拖拽 / 配置）
- [ ] T007 [P1] [US3-D] 新建 `FluxingConfigEditor/UserDictPage.{h,cpp}`（user.db 搜索 / 编辑 / 删除 / 批量导入）

## Phase 3 · 集成

- [ ] T008 [P1] 更新 `weasel.sln` + `xmake.lua` 加 `FluxingConfigEditor` project
- [ ] T009 [P1] 接入 `FluxingPanelHost` 二级窗口（共享 006 进程）

## Phase 4 · 验证

- [ ] T010 [P1] 新建 `test/TestYamlRoundTrip.cpp` + `TestConflictChecker.cpp`
- [ ] T011 [P1] `xbuild.bat weasel installer` → 0 errors
- [ ] T012 [P1] 手动验证 3 平台 3 DPI

## Phase 5 · 提交 & Release

- [ ] T013 [P1] commit：`feat(fluxing): spec 007 yaml config UI`
- [ ] T014 [P1] release `fluxing-0.22.0.0-installer.exe` 推 `kizemo`