# 026 - Tasks - TestWeaselIPC orchestration fix

> All tasks P1 (test infra closure - the run-tests gate is the primary CI signal).
> `[P]` = parallelizable (different files, no dependency).

## Phase 1 - Pre-flight (T001)

- [ ] T001 verify the failure mode
  - Run `cmd /c scripts\run-tests.bat` and capture OUTER_RC + last 20 lines
  - Run `Release\TestWeaselIPC.exe & echo %ERRORLEVEL%` directly
  - Confirm: `=== TESTS FAILED ===` and TestWeaselIPC returns `-2`

## Phase 2 - Diagnosis (T002)

- [ ] T002 write L30 (TestWeaselIPC silent -2 + PowerShell false-positive)
  - Append to `.specify\memory\lessons-learned.md`
  - AP-L30-A/B/C anti-patterns

## Phase 3 - Implementation (T003)

- [ ] T003 rewrite TestWeaselIPC section of `scripts\run-tests.bat`
  - Extract TestWeaselIPC out of the for loop
  - `start "" /B "Release\TestWeaselIPC.exe" /start`
  - `ping -n 3 127.0.0.1 >nul` (2s wait)
  - `"Release\TestWeaselIPC.exe" < nul` (client mode)
  - capture errorlevel
  - `"Release\TestWeaselIPC.exe" /stop < nul`
  - apply errorlevel to FAIL

## Phase 4 - Verification (T004)

- [ ] T004 run `cmd /c scripts\run-tests.bat`
  - Expect: `=== ALL TESTS PASSED ===`
  - Expect: `OUTER_RC=0`
  - Expect: TestWeaselIPC outputs server-replies + get-response-data lines (proves real round-trip)

## Phase 5 - Lesson (T005)

- [ ] T005 write L31 (orchestration pattern + /B + ping wait)
  - Append to lessons-learned.md
  - AP-L31-A/B anti-patterns

## Phase 3.5 - Additional root causes (discovered in pre-flight)

- [ ] T003.1 [P] fix TestRequestHandler virtual hiding
  - Add `override` keyword to FindSession / AddSession / RemoveSession / ProcessKeyEvent
  - Change AddSession signature to match base: (LPWSTR buffer, EatLine eat = 0)
  - Change return types to DWORD (matches base class)
- [ ] T003.2 [P] fix TestWeaselIPC.vcxproj OutDir path glue
  - Change all `$(SolutionDir)$(Configuration)` to `$(SolutionDir)\$(Configuration)`
  - Add explicit trailing `\` separator in OutDir / IntDir
- [ ] T003.3 [P] add missing Build.0 line to weasel.sln
  - `{9C1CC4BA-...}.Release|Win32.Build.0 = Release|Win32`
- [ ] T003.4 [P] write L31 (the two root causes lesson)
  - Document virtual hiding trap (always use `override`)
  - Document vcxproj OutDir path glue (always use explicit `\` after $(SolutionDir))
  - AP-L31-A/B/C/D anti-patterns

## Phase 6 - Pre-commit gate + release (T006-T007)

- [ ] T006 AGENTS.md sec 5 five-step gate
  - byte health on scripts\run-tests.bat
  - run-tests.bat OUTER_RC=0
  - build hygiene
  - format
  - scope check (single `test(fluxing):` scope per P4)
- [ ] T007 commit + tag v0.18.15.0 + push kizemo

## Done = evidence

- `git log --oneline -1` shows the new commit on `Fluxing`
- `git tag -l v0.18.15.0` shows the new tag
- `git status` shows working tree clean (excluding librime submodule)
- `cmd /c scripts\run-tests.bat` reports `=== ALL TESTS PASSED ===` and `OUTER_RC=0`
- TestWeaselIPC outputs prove real round-trip (server replies: 1 + get response data: 1 + buffer reads: Greeting=Hello, 小狼毫.)
