# 030 · Tasks · 候选字右键（stage 2: UI 接线）

## Phase 1 · IPC plumbing（修 spec 028 漏的父类虚函数 + 加新命令）

- [ ] T001 [P1] [R1] `include/WeaselIPC.h`：加 `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE` enum（插在 `HIGHLIGHT_CANDIDATE` 和 `CHANGE_PAGE` 之间）
- [ ] T002 [P1] [R1] `include/WeaselIPC.h`：`RequestHandler` 加 `virtual void DeleteCandidateOnCurrentPage(size_t index, DWORD session_id) {}`
- [ ] T003 [P1] [R1] `include/WeaselIPC.h`：`Client` 加 `bool DeleteCandidateOnCurrentPage(size_t index);` 公有方法
- [ ] T004 [P1] [R1] `include/RimeWithWeasel.h:53`：`RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 加 `override` 关键字
- [ ] T005 [P1] [R1] `WeaselIPC/WeaselClientImpl.cpp`：加 `ClientImpl::DeleteCandidateOnCurrentPage` + `Client::DeleteCandidateOnCurrentPage`（pass-through）
- [ ] T006 [P1] [R1] `WeaselIPCServer/WeaselServerImpl.h`：加 `OnDeleteCandidateOnCurrentPage` 声明
- [ ] T007 [P1] [R1] `WeaselIPCServer/WeaselServerImpl.cpp`：加 impl + `HandlePipeMessage` 注册 `PIPE_MSG_HANDLE`

## Phase 2 · WeaselUI 接线

- [ ] T008 [P1] [R1] `WeaselUI/WeaselPanel.h`：加 `m_deleteCallback` 成员 + `SetDeleteCandidateCallback` setter + `OnRButtonDown` 声明
- [ ] T009 [P1] [R1] `WeaselUI/WeaselPanel.h::BEGIN_MSG_MAP` 加 `MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)`
- [ ] T010 [P1] [R1] `WeaselUI/WeaselPanel.cpp`：加 `OnRButtonDown` 实现（命中判定 + 100ms 防抖 + callback 派发）

## Phase 3 · WeaselTSF 接线

- [ ] T011 [P1] [R1] `WeaselTSF/CandidateList.cpp::OnKeyDown` 在 `SetUICallBack` 旁多设 `_ui->SetDeleteCandidateCallback([this](size_t index) { _tsf->HandleDeleteCandidate(index); })`
- [ ] T012 [P1] [R1] `WeaselTSF/WeaselTSF.h` 加 `HandleDeleteCandidate` + `_DeleteCandidateOnCurrentPage` 声明
- [ ] T013 [P1] [R1] `WeaselTSF/WeaselTSF.cpp` 加 `_DeleteCandidateOnCurrentPage` impl（调 `m_client.DeleteCandidateOnCurrentPage(index)`，不模拟 VK_SELECT）

## Phase 4 · 集成测试

- [ ] T014 [P1] [R2] 新建 `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp`（4 真实 assertions：hit, no-hit, client-dispatch, debounce）
- [ ] T015 [P1] [R2] 新建 `test/TestCandidateRButtonDown/TestCandidateRButtonDown.vcxproj`（照 `TestUserDictUpdate.vcxproj` 模板，L31 修复 `$(SolutionDir)\$(Configuration)\`）
- [ ] T016 [P1] [R2] 编辑 `weasel.sln` 加 `TestCandidateRButtonDown` project 节点（跟 spec 028 T005 同一格式）
- [ ] T017 [P1] [R2] 编辑 `scripts/test-infra/run-test-suite.bat` 加 `TestCandidateRButtonDown.exe` 到 test list

## Phase 5 · 验证

- [ ] T018 [P1] [R4] `cmd /c xbuild.bat weasel` → 0 errors
- [ ] T019 [P1] [R2] `cmd /c scripts\test-infra\run-test-suite.bat` → 8/8 PASS
- [ ] T020 [P1] [R4] `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 5/5 binaries fresh（spec 029 L31 回归）
- [ ] T021 [P1] AGENTS.md sec 5 五步 pre-commit gate（byte health / clang-format / scope 等）
- [ ] T022 [P1] [R3] `rg "override" include/RimeWithWeasel.h` 验证 `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 实际是 override（不是 shadow）

## Phase 6 · 提交

- [ ] T023 [P1] commit：`feat(fluxing): spec 030 - candidate right-click UI wiring (stage 2 of 008)`
- [ ] T024 [P1] push：`git push kizemo Fluxing`
- [ ] T025 [P1] 不 tag（bookkeeping sub-release；下次发 installer 时再 tag v0.18.19.0 或 v0.19.0.0）