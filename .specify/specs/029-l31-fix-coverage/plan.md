# 029 · Plan · L31 fix 范围扩展

## Technical context

- **Files touched (existing)**: 3
  - `test\TestResponseParser\TestResponseParser.vcxproj` (OutDir fix)
  - `test\TestBindingResolution\TestBindingResolution.vcxproj` (OutDir fix)
  - `test\TestYamlRoundTripE2E\TestYamlRoundTripE2E.vcxproj` (OutDir fix)
- **Files touched (new)**: 4
  - `.specify\specs\029-l31-fix-coverage\{spec,plan,tasks}.md` (3 files)
  - `.specify\memory\lessons-learned.md` (append L36)
- **No C++ change, no test source change, no test infra change, no installer.**

## Step-by-step

### T001 - byte-level OutDir replace for TestResponseParser.vcxproj

Read bytes, find `<OutDir>$(SolutionDir)msbuild\$(Configuration)\$(Platform)\</OutDir>`,
replace with `<OutDir>$(SolutionDir)\$(Configuration)\</OutDir>`.

Note: this changes the OutDir from `msbuild\Release\Win32\` (deeply
nested under the product project's intermediate output) to
`Release\` (the top-level expected location, matching TestWeaselIPC
and TestUserDictUpdate).

This will cause a one-time migration: the .exe currently lives at
`F:\soft\00selfmade\rimemsbuild\Release\Win32\TestResponseParser.exe`
and will move to `F:\soft\00selfmade\rime\Release\TestResponseParser.exe`
on the next build. The old path is the L31 bug; the new path is
correct.

Verify: rebuild `TestResponseParser` standalone and confirm the new
path. Then delete the old `.exe` at the wrong path (it is now stale
garbage) to prevent accidental runs of the wrong binary.

### T002 - byte-level OutDir replace for TestBindingResolution.vcxproj

Read bytes, find `<OutDir>$(SolutionDir)$(Configuration)\</OutDir>` (the
pattern A: no backslash between `$(SolutionDir)` and `$(Configuration)`),
replace with `<OutDir>$(SolutionDir)\$(Configuration)\</OutDir>`.

One-time migration: the .exe currently lives at
`F:\soft\00selfmade\rimemsbuild\Release\TestBindingResolution.exe`
(where `rime` + `R` are glued). Will move to
`F:\soft\00selfmade\rime\Release\TestBindingResolution.exe` on next
build. Delete old.

### T003 - byte-level OutDir replace for TestYamlRoundTripE2E.vcxproj

Same as T002 (pattern A).

One-time migration: same shape. Delete old.

### T004 - Verify

Run `cmd /c scripts\test-infra\run-test-suite.bat` (via Start-Process
to avoid L33/L34 path issues) and confirm:
- All 7 test projects PASS
- Build output reads `<name>.vcxproj -> F:\soft\00selfmade\rime\Release\<name>.exe`
  (correct path, no glue) for the 3 fixed projects
- `=== ALL TESTS PASSED ===` and RC 0

Run `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`:
- All 7 FRESH, RC 0.

Optional: touch a .cpp source and re-run verify-test-binaries-fresh
to confirm the L31 detection (now that binaries are at the right
path) is no longer a false positive (it is the real check we built
in spec 027).

### T005 - Cleanup

After T004 passes, delete the 3 old wrong-path .exe files
(`rimemsbuild\Release\Win32\TestResponseParser.exe`,
`rimemsbuild\Release\TestBindingResolution.exe`,
`rimemsbuild\Release\TestYamlRoundTripE2E.exe`). These are
stale-binary hazards (L31 again) and should not linger.

If they live in `msbuild\Release\Win32\`, that directory is
gitignored, so deletion is local-only (no git operations needed).

### T006 - Write L36 lesson

Append L36 to `.specify\memory\lessons-learned.md` documenting:
- The 3-of-4 coverage gap pattern: a "fix" that lands in 1 file is
  only fully correct if all sibling files share the same fix.
- The audit checklist: after any L## fix, grep the repo for sibling
  files with the same broken pattern.
- AP-L36-A: "Fixed it in the test exe I was touching" - the 3 others
  still have the bug; they just happen to still build because the
  wrong path is also a valid path on disk.
- AP-L36-B: Trusting the L## lesson to be "applied repo-wide" when
  in fact it was applied to one file. Always re-grep after writing
  the lesson.
- AP-L36-C: Leaving stale binaries at the wrong path "because they
  still build and run". They will be picked up by future ad-hoc
  invocations and confuse the next agent.

### T007 - Pre-commit gate

AGENTS.md sec 5 five steps:
- byte health on 3 modified .vcxproj files (CRLF preserved, no
  0xC0/0xC1, BOM unchanged)
- run-test-suite.bat OUTER_RC=0, 7/7 PASS
- build hygiene (no new .log/.token)
- format (no clang-format locally)
- scope check (single `test(fluxing):` scope per P4 - this is
  test infra cleanup, not new behavior)

### T008 - CHANGELOG + commit + tag + push

Update CHANGELOG.md with `## [0.18.18.0-fluxing] - 2026-07-04` section.
Commit on `Fluxing` with `test(fluxing):` scope. Tag v0.18.18.0
(lightweight). Push to kizemo/Fluxing and kizemo/v0.18.18.0.

## Constitution check

| Rule | Status | Note |
|---|---|---|
| R1 intent+acceptance | OK | spec.md sections 0+1 |
| R2 spec separate from plan | OK | spec.md is text-only |
| R3 single priority | OK | All P1 (L31 fix completeness) |
| R4 constitution gate | OK | This table |
| R5 task size < 4h, 1-3 files | OK | 3 modified + 1 lesson; ~1 hour total |
| R6 done = evidence | OK | T004 verification output is the evidence |
| R7 one source of truth | OK | L36 in lessons-learned.md only |
| R8 specs versioned in git | OK | spec/plan/tasks committed |
| R9 lookup beats memory | OK | This spec grepped the repo for the broken pattern, not assumed it |