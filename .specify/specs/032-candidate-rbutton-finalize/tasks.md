# 032 · Tasks · 候选字编辑收尾（spec 008 T007 + T008）

## Phase 1 · T007 暗色主题订阅

- [ ] T001 [P1] [R3] `WeaselUI/WeaselPanel.h::BEGIN_MSG_MAP` 加 `MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)`
- [ ] T002 [P1] [R3] `WeaselUI/WeaselPanel.h` 加 `OnSettingChange` + `_RefreshStylePalette` 声明
- [ ] T003 [P1] [R3] `WeaselUI/WeaselPanel.cpp` 加 `OnSettingChange` impl（检查 lParam = "ImmersiveColorSet"，调 _RefreshStylePalette + Refresh）
- [ ] T004 [P1] [R3] `WeaselUI/WeaselPanel.cpp` 加 `_RefreshStylePalette` impl（IsUserDarkMode + 硬编码 2 色 + 触发 _CreateLayout）

## Phase 2 · T008 tray 恢复按钮

- [ ] T005 [P1] `WeaselServer/resource.h` 加 `#define ID_WEASELTRAY_RESTORE_IGNORED 40017`
- [ ] T006 [P1] `WeaselServer/WeaselServer.rc` 3 个语言菜单块各加 `MENUITEM "恢复被屏蔽的候选 (&I)", ID_WEASELTRAY_RESTORE_IGNORED`（插入位置：靠近 DICT_MANAGEMENT）
- [ ] T007 [P1] [R2] `WeaselServer/WeaselServerApp.cpp::SetupMenuHandlers` 加 `m_server.AddMenuHandler(ID_WEASELTRAY_RESTORE_IGNORED, [...] DeleteFileW FindFirstFileW ...)`

## Phase 3 · 集成测试

- [ ] T008 [P1] 新建 `test/TestPanelDarkModeSubscribe/TestPanelDarkModeSubscribe.cpp`（3 真实 assertions: WM_SETTINGCHANGE lParam 判定 / dark palette 切换 / Refresh 调用）
- [ ] T009 [P1] 新建 `test/TestPanelDarkModeSubscribe/TestPanelDarkModeSubscribe.vcxproj`（照 TestCandidateIgnoreFilter.vcxproj 模板）
- [ ] T010 [P1] 新建 `test/TestTrayRestoreIgnored/TestTrayRestoreIgnored.cpp`（3 真实 assertions: 创建 ignore file / DeleteFileW 后文件不存在 / 空目录返回 true）
- [ ] T011 [P1] 新建 `test/TestTrayRestoreIgnored/TestTrayRestoreIgnored.vcxproj`（同样模板）
- [ ] T012 [P1] 编辑 `weasel.sln` 加 2 个 test project 节点
- [ ] T013 [P1] 编辑 `scripts/test-infra/run-test-suite.bat` 加 2 个 test exe 到 test list
- [ ] T014 [P1] 编辑 `scripts/test-infra/verify-test-binaries-fresh.bat` 加 2 个 test name 到 check list
- [ ] T015 [P1] [L36] rg `m_style.bg_color` 看其他地方是否也用此字段（如果 hardcode color 也是一种 partial ship pattern，可能要 spec 006 统一）

## Phase 4 · 验证

- [ ] T016 [P1] `cmd /c xbuild.bat weasel` → 0 errors
- [ ] T017 [P1] `cmd /c scripts\test-infra\run-test-suite.bat` → 11/11 PASS
- [ ] T018 [P1] `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 11/11 FRESH
- [ ] T019 [P1] AGENTS.md sec 5 五步 pre-commit gate
- [ ] T020 [P1] [L37] 验证 WeaselPanel.cpp + WeaselServer/WeaselServer.rc 写入后 `CR == LF`

## Phase 5 · 提交

- [ ] T021 [P1] commit：`feat(fluxing): spec 032 - candidate rbutton finalize (T007+T008 of 008)`
- [ ] T022 [P1] push：`git push kizemo Fluxing`
- [ ] T023 [P1] 不 tag（bookkeeping sub-release，spec 008 完整收尾但不发 installer）