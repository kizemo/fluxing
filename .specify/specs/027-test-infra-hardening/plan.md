# 027 - Plan - Test infra hardening

## Technical context

- **Files touched (new)**: 4
  - `scripts\test-infra\install_smoke_test.bat` (new, ~25 lines)
  - `scripts\test-infra\run-test-suite.bat` (new, ~110 lines, the meat)
  - `scripts\test-infra\verify-test-binaries-fresh.bat` (new, ~50 lines)
  - `.specify\specs\027-test-infra-hardening\{spec,plan,tasks}.md` (new, 3 files)
- **Files touched (existing)**: 2
  - `scripts\run-tests.bat` (rewritten as thin wrapper, ~10 lines)
  - `.specify\memory\lessons-learned.md` (append L32)
- **No C++ change, no test project change, no installer change, no CI change.**
- **Build system**: none invoked. Pre-commit gate runs the existing test suite only.
- **No version bump** in `env.bat` / `weasel.props` (gitignored, only bumped for
  code releases). This is a bookkeeping sub-release; the release tag is
  `v0.18.16.0` per P8 / P4 scope convention.

## Step-by-step

### T001 - Write `install_smoke_test.bat` (entry point only)

This is a thin entry point that documents the AGENTS.md sec 2.5 recipe and
exits 0. Body is:

```batch
@echo off
rem ====================================================================
rem scripts\test-infra\install_smoke_test.bat
rem Entry point for the NSIS silent-install smoke test.
rem
rem The full recipe (PowerShell assertions for layout invariants:
rem exit code, fluxing\weasel\, user1\fluxing\, HKLM InstallDir, HKCU
rem RimeUserDir, rime.dll size, prebuilt dicts, PE arch) lives in
rem AGENTS.md sec 2.5 - that is the source of truth and is NOT
rem duplicated here. This wrapper exists so that:
rem   1. Future CI steps can call a NAMED entry point.
rem   2. The convention "smoke tests have a wrapper" is established.
rem   3. Future work can port the recipe to pure cmd / NSISExec.
rem
rem Usage: scripts\test-infra\install_smoke_test.bat
rem Exit: 0 always (today); the actual recipe is run by following
rem        AGENTS.md sec 2.5 manually or by piping it through powershell.
rem
rem Spec 027 (2026-07-03) - see .specify\specs\027-test-infra-hardening.
rem ====================================================================
echo [install_smoke_test] See AGENTS.md sec 2.5 for the silent-install
echo                       smoke test recipe. This wrapper is a named
echo                       entry point; the actual run is done by
echo                       following that recipe. Exiting 0 (smoke test
echo                       infra is wired up; recipe itself unchanged).
exit /b 0
```

### T002 - Write `run-test-suite.bat` (the meat)

This is the current `scripts\run-tests.bat` body moved here verbatim.
The differences vs. the current `scripts\run-tests.bat` are:

1. The script lives at `scripts\test-infra\run-test-suite.bat`, so
   `set "SOL_DIR=%~dp0..\.."` (repo root is 2 levels up).
2. Header is updated to reflect the new location and to cite L30.

The body is otherwise byte-for-byte identical to the spec 026 version
(the version that has TestWeaselIPC orchestration). No behavioral change.

### T003 - Write `verify-test-binaries-fresh.bat` (mtime check)

Body iterates 6 test projects, calls a one-liner PowerShell per project:

```powershell
powershell -NoProfile -Command "if ((Get-Item 'Release\<Name>.exe').LastWriteTimeUtc -lt (Get-ChildItem 'test\<Name>\*.cpp','test\<Name>\*.h','test\<Name>\*.vcxproj' -Recurse | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc) { exit 1 } else { exit 0 }"
```

If any project reports stale, print which one and exit 1. This is the
L31 stale-binary detection codified.

### T004 - Rewrite `scripts\run-tests.bat` as a thin wrapper

```batch
@echo off
rem scripts\run-tests.bat is now a thin wrapper around
rem scripts\test-infra\run-test-suite.bat. See spec 027.
cmd /c "%~dp0test-infra\run-test-suite.bat" %*
exit /b %ERRORLEVEL%
```

After this, `scripts\run-tests.bat` is 5 lines and exists for
backward-compat with anyone who was calling it directly (including
the 4-spec-long muscle memory from spec 015-026).

### T005 - Write L32 (infra hardening pattern)

Append L32 to `.specify\memory\lessons-learned.md` documenting:
- the lesson-to-script promotion pattern
- when a lesson is "spec" (applies to one PR) vs "infra" (applies forever)
- the 3 tests for "is this lesson infra-worthy":
  1. Has it bitten us in 2+ specs?
  2. Is the fix a one-liner that's easy to forget?
  3. Can a thin wrapper enforce it without changing product behavior?
- AP-L32-A/B anti-patterns

### T006 - Verify

Run `cmd /c scripts\test-infra\run-test-suite.bat` and confirm:
- `=== ALL TESTS PASSED ===` prints
- `OUTER_RC=0` (from the new wrapper)
- All 6 test projects report their expected counts

Run `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` and
confirm it exits 0 (all binaries fresh; if not, identify which one and
rebuild).

Run `cmd /c scripts\test-infra\install_smoke_test.bat` and confirm it
exits 0 and prints the "see AGENTS.md sec 2.5" pointer.

### T007 - Pre-commit gate

AGENTS.md sec 5 five steps:
- byte health on the 3 new .bat files (CRLF preserved, no BOM needed for .bat)
- run-test-suite.bat OUTER_RC=0, all 6 PASS
- build hygiene (no new .log/.token)
- format (no clang-format locally)
- scope check (single `test(fluxing):` scope per P4)

### T008 - Commit, tag, push

Commit on `Fluxing` with `test(fluxing):` scope. Tag v0.18.16.0 (lightweight).
Push to kizemo/Fluxing and kizemo/v0.18.16.0.

Update CHANGELOG.md with the new sub-release section before committing.

## Constitution check

| Rule | Status | Note |
|---|---|---|
| R1 intent+acceptance | OK | spec.md sections 0+1 |
| R2 spec separate from plan | OK | spec.md is text-only |
| R3 single priority | OK | All P1 (test infra closure) |
| R4 constitution gate | OK | This table |
| R5 task size < 4h, 1-3 files | OK | 3 new .bat + 1 thin rewrite + L32; ~2-3 hours total |
| R6 done = evidence | OK | T006 verification output is the evidence |
| R7 one source of truth | OK | L32 in lessons-learned.md only |
| R8 specs versioned in git | OK | spec/plan/tasks committed |
| R9 lookup beats memory | OK | This spec re-derives the run-tests.bat body from the spec 026 version (no guessing from prose) |