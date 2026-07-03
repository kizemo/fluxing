# 029 · Tasks · L31 fix 范围扩展

> All tasks P1 (L31 fix completeness). [P] = parallelizable.

## Phase 1 - File-level fixes (T001-T003)

- [ ] T001 [P] fix TestResponseParser.vcxproj OutDir
  - Pattern B: `$(SolutionDir)msbuild\$(Configuration)\$(Platform)\`
  - Replace with `$(SolutionDir)\$(Configuration)\`
  - Verify CRLF preserved (L07)
- [ ] T002 [P] fix TestBindingResolution.vcxproj OutDir
  - Pattern A: `$(SolutionDir)$(Configuration)\` (no backslash)
  - Replace with `$(SolutionDir)\$(Configuration)\`
  - Verify CRLF preserved
- [ ] T003 [P] fix TestYamlRoundTripE2E.vcxproj OutDir
  - Pattern A (same as T002)
  - Replace with `$(SolutionDir)\$(Configuration)\`
  - Verify CRLF preserved

## Phase 2 - Verification (T004)

- [ ] T004 run `cmd /c scripts\test-infra\run-test-suite.bat`
  - Expect: RC 0, === ALL TESTS PASSED ===, 7/7
  - Expect: build output for fixed projects reads
    `F:\soft\00selfmade\rime\Release\<name>.exe` (no glue)
- [ ] T004b run `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`
  - Expect: RC 0, 7 FRESH

## Phase 3 - Cleanup (T005)

- [ ] T005 delete stale binaries at wrong path
  - `F:\soft\00selfmade\rimemsbuild\Release\Win32\TestResponseParser.exe`
  - `F:\soft\00selfmade\rimemsbuild\Release\TestBindingResolution.exe`
  - `F:\soft\00selfmade\rimemsbuild\Release\TestYamlRoundTripE2E.exe`
  - These directories are gitignored, so deletion is local-only.

## Phase 4 - Lesson (T006)

- [ ] T006 write L36 (L31 fix coverage gap)
  - Append to `.specify\memory\lessons-learned.md`
  - Document the 3-of-4 coverage gap
  - AP-L36-A/B/C anti-patterns

## Phase 5 - CHANGELOG (T007)

- [ ] T007 add CHANGELOG entry for v0.18.18.0
  - New `## [0.18.18.0-fluxing] - 2026-07-04` section
  - Brief summary: "spec 029: L31 fix coverage (3 remaining test vcxproj)"
  - Cross-reference to spec 029 + L36

## Phase 6 - Pre-commit gate + release (T008-T009)

- [ ] T008 AGENTS.md sec 5 five-step gate
  - byte health on 3 modified .vcxproj files
  - run-test-suite.bat OUTER_RC=0, 7/7 PASS
  - build hygiene
  - format
  - scope check (single `test(fluxing):` scope)
- [ ] T009 commit + tag v0.18.18.0 + push kizemo

## Done = evidence

- `git log --oneline -1` shows the new commit on `Fluxing`
- `git tag -l v0.18.18.0` shows the new tag
- `git status` shows working tree clean (excluding librime submodule)
- `cmd /c scripts\test-infra\run-test-suite.bat` reports
  `=== ALL TESTS PASSED ===` and RC 0
- 3 stale binaries at wrong path are deleted