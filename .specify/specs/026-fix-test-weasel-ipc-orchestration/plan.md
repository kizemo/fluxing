# 026 - Plan - TestWeaselIPC orchestration fix

## Technical context

- **Files touched**: 1 (scripts\run-tests.bat).
- **No C++ change, no test project change, no installer change.**
- **Build system**: none invoked. Pre-commit gate runs the existing test suite only.
- **No version bump** (gitignored env.bat / weasel.props not touched; release is 0.18.15.0 bookkeeping+fix).

## Step-by-step

### T001 - Verify failure mode (pre-flight)

Run `cmd /c scripts\run-tests.bat` and capture:
- `OUTER_RC` from `%ERRORLEVEL%` after the call.
- Whether `=== ALL TESTS PASSED ===` or `=== TESTS FAILED ===` prints.
- TestWeaselIPC return code when run directly: `Release\TestWeaselIPC.exe & echo %ERRORLEVEL%`.

Expected: `=== TESTS FAILED ===` and TestWeaselIPC direct run returns `-2` (STATUS_INVALID_HANDLE).

### T002 - Write L30 (diagnosis lesson)

Add L30 to `.specify\memory\lessons-learned.md` documenting:
- the silent -2 failure mode of TestWeaselIPC when no server is running
- the PowerShell `cmd /c` false-positive (OUTER_RC=0 vs real %ERRORLEVEL%=1)
- the test-orchestration gap: no script in this repo has ever spawned /start + client + /stop
- AP-L30-A/B/C anti-patterns

### T003 - Implement batch orchestration

Replace the `for %%E in (...TestWeaselIPC...)` line for TestWeaselIPC only.
Approach: extract TestWeaselIPC out of the for loop into a dedicated block:

```batch
rem TestWeaselIPC: integration test needs server spawn + client + shutdown
echo === Setting up TestWeaselIPC server (background) ===
start "" /B "Release\TestWeaselIPC.exe" /start
ping -n 3 127.0.0.1 >nul
echo === Running TestWeaselIPC.exe (client) ===
"Release\TestWeaselIPC.exe" < nul
set "IPC_RC=!errorlevel!"
echo === Shutting down TestWeaselIPC server ===
"Release\TestWeaselIPC.exe" /stop < nul
if !IPC_RC! NEQ 0 set "FAIL=1"
```

Use `ping -n 3 127.0.0.1 >nul` (2s sleep) for cross-availability - per L22 "Windows-isms"
recommendation. Do NOT use `timeout` (not always on PATH). Do NOT use `sleep` (not Windows).

### T004 - Verify orchestration

Run `cmd /c scripts\run-tests.bat` and confirm:
- `=== ALL TESTS PASSED ===` prints
- `OUTER_RC=0`
- All 6 test projects report their expected counts (35 + 13 + 6 + 4 + 1 + 6 = 65 PASS)
  (TestWeaselIPC now reports `1 / 1` since it actually ran the round-trip)

### T005 - Write L31 (fix lesson)

Add L31 to lessons-learned.md documenting:
- the orchestration pattern (start /B + ping wait + client + /stop)
- the /B flag rationale (no new console window)
- the pre-emptively-failed TestWeaselIPC had a built-in /stop (3-arg branch in _tmain)
  so cleanup is guaranteed
- AP-L31-A/B anti-patterns

### T006 - Pre-commit gate

AGENTS.md sec 5 five steps:
- byte health on scripts\run-tests.bat (CRLF preserved, no BOM needed for .bat)
- run-tests.bat OUTER_RC=0, all 65 PASS
- build hygiene (no new .log/.token)
- format (no clang-format locally)
- scope check (single `test(fluxing):` scope per P4)

### T007 - Commit, tag, push

Commit on `Fluxing` with `test(fluxing):` scope. Tag v0.18.15.0 (lightweight).
Push to kizemo/Fluxing and kizemo/v0.18.15.0.

## Constitution check

| Rule | Status | Note |
|---|---|---|
| R1 intent+acceptance | OK | spec.md sections 0+1 |
| R2 spec separate from plan | OK | spec.md is text-only |
| R3 single priority | OK | All P1 (test infra closure) |
| R4 constitution gate | OK | This table |
| R5 task size < 4h, 1-3 files | OK | Single .bat file; 2 lessons; ~1 hour total |
| R6 done = evidence | OK | T004 verification output is the evidence |
| R7 one source of truth | OK | Lessons L30/L31 are in lessons-learned.md only |
| R8 specs versioned in git | OK | spec/plan/tasks committed |
| R9 lookup beats memory | OK | This spec is the result of verifying the actual |
|  |  | TestWeaselIPC return code (-2) rather than trusting the OUTER_RC=0 myth |
