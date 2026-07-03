# 029 · L31 fix 范围扩展 (close the 3-of-4 test vcxproj L31 coverage gap)

> **Scope**: extend the L31 vcxproj OutDir/IntDir fix from TestWeaselIPC
> (1 project, spec 026) to the remaining 3 test projects:
> TestResponseParser, TestBindingResolution, TestYamlRoundTripE2E.
> This is bookkeeping cleanup, not new behavior. After this spec,
> every test vcxproj uses `$(SolutionDir)\$(Configuration)\...` with
> the explicit backslash after `$(SolutionDir)`.

## 0. Why now (intent before implementation)

While writing spec 028 (the new TestUserDictUpdate test project), the
L31 path-glue fix was applied to `TestUserDictUpdate.vcxproj` as a
matter of course (we had just learned the lesson in spec 026). After
the spec 028 run-test-suite.bat output, the L31 bug was observed
again in the build output of the OTHER test projects:

```
TestResponseParser.vcxproj -> F:\soft\00selfmade\rimemsbuild\Release\Win32\WeaselIPC.lib
  (OutDir = $(SolutionDir)msbuild\... with no separator)
TestBindingResolution.vcxproj -> F:\soft\00selfmade\rimemsbuild\Release\...
  (OutDir = $(SolutionDir)$(Configuration)\ with no separator)
TestYamlRoundTripE2E.vcxproj -> F:\soft\00selfmade\rimemsbuild\Release\...
  (same)
```

The test exes still build and run (because the broken OutDir happens
to point to a path that exists), but:
- TestResponseParser.exe is written into the same directory as the
  RimeWithWeasel product project 's intermediate `msbuild\` output.
  This is a **collision risk** for product builds.
- The verify-test-binaries-fresh.bat (spec 027) compares mtimes
  against `Release\<Name>.exe` (the top-level Release dir, which
  spec 026 established). If a test exe is built to the WRONG
  location, the FRESH check at the right location sees a stale
  binary and reports a false positive.
- The bug violates the L31 contract: **every** vcxproj in this
  solution should use `$(SolutionDir)\$(Configuration)\` with the
  explicit backslash. We fixed it for 1 of 4 test projects in spec
  026; this spec fixes the remaining 3.

## 1. Acceptance criteria

- `test\TestResponseParser\TestResponseParser.vcxproj`:
  - `OutDir` = `$(SolutionDir)\$(Configuration)\` (was `$(SolutionDir)msbuild\$(Configuration)\$(Platform)\`)
  - `IntDir` = `$(SolutionDir)\msbuild\$(Configuration)\$(Platform)\$(ProjectName)\` (unchanged for this project; the IntDir is fine)
- `test\TestBindingResolution\TestBindingResolution.vcxproj`:
  - `OutDir` = `$(SolutionDir)\$(Configuration)\` (was `$(SolutionDir)$(Configuration)\` - missing leading backslash)
- `test\TestYamlRoundTripE2E\TestYamlRoundTripE2E.vcxproj`:
  - `OutDir` = `$(SolutionDir)\$(Configuration)\` (same fix as TestBindingResolution)
- `cmd /c scripts\test-infra\run-test-suite.bat` exits 0 with **7/7** test
  projects. The build output for the 3 fixed projects now reads
  `<name>.vcxproj -> F:\soft\00selfmade\rime\Release\<name>.exe`
  (correct location, with explicit backslash).
- `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` exits 0
  with all 7 FRESH. (No change in PASS/FAIL count, but the script
  now actually verifies the right binaries because they are at the
  right path.)
- L36 added to `.specify\memory\lessons-learned.md` documenting the
  3-of-4 coverage gap and the pattern of "fix at the L## level, not
  the symptom level".
- AGENTS.md sec 5 five-step pre-commit gate passes.
- No installer change. No product code change. No `env.bat` /
  `weasel.props` bump (bookkeeping sub-release).

## 2. Approach (chosen: byte-level OutDir/IntDir replace)

For each of the 3 .vcxproj files:

1. Read the file as bytes (L07: avoid PowerShell string APIs that
   mangle encoding).
2. Find the `<OutDir>...</OutDir>` value.
3. Replace it with the L31-correct value.
4. Write back as bytes; verify CRLF preserved (L07), BOM unchanged,
   no 0xC0/0xC1.

The replace is mechanical: every broken OutDir is one of two known
patterns:
- Pattern A: `$(SolutionDir)$(Configuration)\` (no backslash between
  SolutionDir and Configuration) - TestBindingResolution, TestYamlRoundTripE2E
- Pattern B: `$(SolutionDir)msbuild\$(Configuration)\$(Platform)\` (no
  backslash between SolutionDir and msbuild) - TestResponseParser

Both become `$(SolutionDir)\$(Configuration)\` (L31 fix from spec 026).

IntDir is also L31-fixed where it has the same broken pattern. For
TestResponseParser the IntDir is already correct
(`$(SolutionDir)\msbuild\...` with the leading backslash) - no change
needed.

## 3. Non-goals

- No test exe source change.
- No run-test-suite.bat change.
- No verify-test-binaries-fresh.bat change.
- No CI workflow change.
- No installer rebuild.

## 4. References

- L31 (vcxproj OutDir path-glue - the original spec 026 fix).
- spec 026 (test-weasel-ipc-orchestration - where the L31 fix landed
  for TestWeaselIPC.vcxproj).
- spec 027 (test-infra-hardening - the verify-test-binaries-fresh.bat
  that depends on the right path).
- spec 028 (candidate-delete-core - the spec that added
  TestUserDictUpdate.vcxproj with the L31 fix as a matter of course,
  exposing the 3-of-4 coverage gap).
- AGENTS.md sec 4.1 (vcxproj L31 trap - "use explicit `\` after
  `$(SolutionDir)`").