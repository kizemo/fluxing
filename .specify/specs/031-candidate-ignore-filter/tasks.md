# 031 · Tasks · 候选字屏蔽（stage 3 of spec 008 — 客户端过滤）

## Phase 1 · WeaselPanel 接线

- [ ] T001 [P1] [R1] `WeaselUI/WeaselPanel.h` 加 `m_ignoreList` + `m_ignoreFilePath` 私有成员 + `_IgnoreCurrentCandidate` / `_FilterIgnoredCandidates` 私有方法声明 + `LoadIgnoreList` 公开方法声明
- [ ] T002 [P1] [R1] `WeaselUI/WeaselPanel.cpp` 加 `LoadIgnoreList` impl（byte-level + BOM 判别 + CRLF/LF/CR split）
- [ ] T003 [P1] [R1] `WeaselUI/WeaselPanel.cpp` 加 `_FilterIgnoredCandidates` impl（反向 erase）
- [ ] T004 [P1] [R1] `WeaselUI/WeaselPanel.cpp` 加 `_IgnoreCurrentCandidate` impl（CreateFileW + WriteFile byte-level）
- [ ] T005 [P1] [R1] `WeaselUI/WeaselPanel.cpp::OnRButtonDown` 扩展（text.length >= 2 -> delete；text.length < 2 -> ignore；按 text 防抖 100ms）
- [ ] T006 [P1] [R1] `WeaselUI/WeaselPanel.cpp::OnCreate` 末尾调 `LoadIgnoreList(m_ctx.schema_id)`（如果 schema_id 已知）
- [ ] T007 [P1] [R1] `WeaselUI/WeaselPanel.cpp::Update` 末尾调 `_FilterIgnoredCandidates()`

## Phase 2 · 集成测试

- [ ] T008 [P1] [R2] 新建 `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.cpp`（5 真实 assertions: UTF-8 BOM CRLF, blank skip, UTF-16 LE BOM, filter removed, filter not-removed）
- [ ] T009 [P1] [R2] 新建 `test/TestCandidateIgnoreFilter/TestCandidateIgnoreFilter.vcxproj`（照 `TestCandidateRButtonDown.vcxproj` 模板，L31 修复 `$(SolutionDir)\$(Configuration)\`）
- [ ] T010 [P1] [R2] 编辑 `weasel.sln` 加 `TestCandidateIgnoreFilter` project 节点（GUID: C5C9D3E1-7B2F-4F6A-9D8E-3C1B5A7E9F42）
- [ ] T011 [P1] [R2] 编辑 `scripts/test-infra/run-test-suite.bat` 加 `TestCandidateIgnoreFilter.exe` 到 test list
- [ ] T012 [P1] [R2] 编辑 `scripts/test-infra/verify-test-binaries-fresh.bat` 加 `TestCandidateIgnoreFilter` 到 check list
- [ ] T013 [P1] [L36] rg 整个 repo 找其他 text file reader（`std::ifstream` + `<<` 模式），验证它们也用 byte-level + BOM 判别（spec 031 L36 coverage audit）

## Phase 3 · 验证

- [ ] T014 [P1] [R4] `cmd /c xbuild.bat weasel` → 0 errors
- [ ] T015 [P1] [R2] `cmd /c scripts\test-infra\run-test-suite.bat` → 9/9 PASS
- [ ] T016 [P1] [R4] `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 9/9 binaries fresh
- [ ] T017 [P1] AGENTS.md sec 5 五步 pre-commit gate
- [ ] T018 [P1] [L37] 验证 WeaselPanel.cpp 写入后 `CR == LF`（避免 spec 030 重蹈覆辙）

## Phase 4 · 提交

- [ ] T019 [P1] commit：`feat(fluxing): spec 031 - candidate ignore filter (stage 3 of 008)`
- [ ] T020 [P1] push：`git push kizemo Fluxing`
- [ ] T021 [P1] 不 tag（bookkeeping sub-release）