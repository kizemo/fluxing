# 027 - Tasks - Test infra hardening

> All tasks P1 (test infra closure - the run-tests gate is the primary CI signal).
> `[P]` = parallelizable (different files, no dependency).

## Phase 1 - Pre-flight (T001)

- [ ] T001 verify current state
  - Run `cmd /c scripts\run-tests.bat` and capture OUTER_RC + last 20 lines
  - Run `Release\TestWeaselIPC.exe & echo %ERRORLEVEL%` directly
  - Confirm: 6/6 PASS, OUTER_RC=0
  - This is the baseline we are NOT regressing

## Phase 2 - Implementation (T002-T004)

- [ ] T002 write `scripts\test-infra\install_smoke_test.bat`
  - Thin entry point, body is the "see AGENTS.md sec 2.5" pointer
  - Header documents why this is a wrapper not a duplicate
  - Exits 0
- [ ] T003 [P] write `scripts\test-infra\run-test-suite.bat`
  - Body is `scripts\run-tests.bat` body verbatim
  - Only diff: `SOL_DIR=%~dp0..\..` (2 levels up)
  - Header cites L30
  - Exits with real OUTER_RC (not PowerShell false-positive)
- [ ] T004 [P] write `scripts\test-infra\verify-test-binaries-fresh.bat`
  - Iterates 6 test projects
  - PowerShell one-liner per project compares mtimes
  - Exits 0 if all fresh, 1 if any stale
  - Header cites L31
- [ ] T005 [P] rewrite `scripts\run-tests.bat` as thin wrapper
  - 5 lines, just calls `scripts\test-infra\run-test-suite.bat %*`
  - Exits with %ERRORLEVEL% (the wrapper propagated it)

## Phase 3 - Verification (T006)

- [ ] T006 run all 3 new wrappers
  - `cmd /c scripts\test-infra\install_smoke_test.bat` exits 0, prints pointer
  - `cmd /c scripts\test-infra\run-test-suite.bat` exits 0, 6/6 PASS
  - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` exits 0, all fresh
  - `cmd /c scripts\run-tests.bat` still works (backward-compat)

## Phase 4 - Lesson (T007)

- [ ] T007 write L32 (infra hardening pattern)
  - Append to `.specify\memory\lessons-learned.md`
  - 3 tests for "is this lesson infra-worthy"
  - AP-L32-A/B anti-patterns

## Phase 5 - CHANGELOG (T008)

- [ ] T008 add CHANGELOG entry for v0.18.16.0
  - New `## [0.18.16.0-fluxing] - 2026-07-03` section
  - Brief summary: "test infra hardening: 3 new wrappers in scripts\test-infra\"
  - Cross-reference to spec 027

## Phase 6 - Pre-commit gate + release (T009-T010)

- [ ] T009 AGENTS.md sec 5 five-step gate
  - byte health on 3 new .bat files (CRLF, no BOM)
  - run-test-suite.bat OUTER_RC=0
  - build hygiene (no new .log/.token)
  - format (no clang-format locally)
  - scope check (single `test(fluxing):` scope per P4)
- [ ] T010 commit + tag v0.18.16.0 + push kizemo

## Done = evidence

- `git log --oneline -1` shows the new commit on `Fluxing`
- `git tag -l v0.18.16.0` shows the new tag
- `git status` shows working tree clean (excluding librime submodule)
- `cmd /c scripts\test-infra\run-test-suite.bat` reports `=== ALL TESTS PASSED ===` and `OUTER_RC=0`
- `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` reports 0 stale binaries
- `cmd /c scripts\test-infra\install_smoke_test.bat` reports 0 and prints the pointer