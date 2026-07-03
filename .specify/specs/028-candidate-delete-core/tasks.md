# 028 · Tasks · 候选字编辑 stage 1 (核心 API 集成 + test)

> All tasks P1 (unblock TDD 3.1 + ship spec 008 stage 1).
> `[P]` = parallelizable (different files, no dependency).

## Phase 1 - Pre-flight (T001)

- [ ] T001 verify rime_api symbol
  - dumpbin /EXPORTS librime\dist_Win32\rime.lib | grep delete_candidate
  - Expect: `delete_candidate` and `delete_candidate_on_current_page`
    symbols present

## Phase 2 - Production code (T002-T003)

- [ ] T002 [P] add `DeleteCandidateOnCurrentPage` to `include\RimeWithWeasel.h`
  - Insert after `SelectCandidateOnCurrentPage` declaration
  - 3 lines: comment + signature + `;`
- [ ] T003 [P] add `DeleteCandidateOnWeasel.cpp` impl to `RimeWithWeasel\RimeWithWeasel.cpp`
  - Insert after `SelectCandidateOnCurrentPage` impl
  - 10 lines: DLOG + m_disabled check + rime_api call + _UpdateUI

## Phase 3 - Test project (T004-T006)

- [ ] T004 [P] write `test\TestUserDictUpdate\TestUserDictUpdate.cpp`
  - 4 TEST() blocks per spec 028 spec.md §2.2
  - Test-local RimeApi stub (no link to rime.lib)
- [ ] T005 [P] write `test\TestUserDictUpdate\TestUserDictUpdate.vcxproj`
  - L31 OutDir fix applied: `$(SolutionDir)\$(Configuration)\...`
  - Standard test project pattern, Release|Win32 only
- [ ] T006 [P] add new project to `weasel.sln`
  - Generate new GUID
  - Add ActiveCfg AND Build.0 entries (L31 AP-L31-D)
  - Insert in the test projects section

## Phase 4 - Test infra (T007)

- [ ] T007 [P] update `scripts\test-infra\run-test-suite.bat`
  - Add `TestUserDictUpdate` to build loop (line ~57)
  - Add `TestUserDictUpdate` to run loop (line ~73)

## Phase 5 - Lesson (T008)

- [ ] T008 write L35 (rime_api.h has no is_user_dict)
  - Append to `.specify\memory\lessons-learned.md`
  - AP-L35-A: planning around unverified C API fields
  - AP-L35-B: assuming C++ class member means C API exposes it

## Phase 6 - CHANGELOG (T009)

- [ ] T009 add CHANGELOG entry for v0.18.17.0
  - New `## [0.18.17.0-fluxing] - 2026-07-04` section
  - Brief summary: "spec 028: wire up rime_api::delete_candidate +
    TestUserDictUpdate integration test (TDD 3.1 7/7); stage 1 of spec 008"
  - Cross-reference to spec 028 + L35

## Phase 7 - Pre-commit gate + release (T010-T011)

- [ ] T010 AGENTS.md sec 5 five-step gate
  - byte health on new files (CRLF, no BOM, no 0xC0/0xC1)
  - run-test-suite.bat OUTER_RC=0, 7/7 PASS
  - build hygiene (no new .log/.token)
  - format (no clang-format locally)
  - scope check (single `feat(fluxing):` scope per P4)
- [ ] T011 commit + tag v0.18.17.0 + push kizemo

## Done = evidence

- `git log --oneline -1` shows the new commit on `Fluxing`
- `git tag -l v0.18.17.0` shows the new tag
- `git status` shows working tree clean (excluding librime submodule)
- `cmd /c scripts\test-infra\run-test-suite.bat` reports
  `=== ALL TESTS PASSED ===` and `OUTER_RC=0`
- TestUserDictUpdate outputs: 4 PASS, 0 FAIL
- All 7 test projects (the prior 6 + new TestUserDictUpdate) PASS