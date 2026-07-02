# 015 - Tasks

## T001 - P1 - byte-level remove system("pause") from TestResponseParser.cpp

- **File**: test/TestResponseParser/TestResponseParser.cpp
- **Action**: remove the line   system("pause"); (8 spaces + system call)
  located between 	est_4(); and return boost::report_errors(); in
  _tmain. Keep the blank line and the return line.
- **Byte-level**: UTF-8 (no BOM, matches existing file), CRLF. Use
  [IO.File]::ReadAllBytes and write back the modified bytes; do NOT
  use PowerShell string APIs (L01 / A1).
- **Acceptance**:
  - Select-String -Path test\TestResponseParser\TestResponseParser.cpp -Pattern "system.*pause" returns 0 matches
  - The Release\TestResponseParser.exe build succeeds (was: build OK
    before this change, still OK after)
  - The test runs to completion in non-interactive mode (< nul).
    Test 1, 2, 3 produce 0 BOOST_TEST failures; test 4 fails as
    documented in spec 015 sec 5 (pre-existing WeaselIPC bug).

## T002 - P1 - byte-level remove system("pause") from TestWeaselIPC.cpp

- **File**: test/TestWeaselIPC/TestWeaselIPC.cpp
- **Action**: remove the line   system("pause"); (no leading whitespace
  per the file's style) located before return 0; in main. Keep
  the blank line and the return 0; line.
- **Byte-level**: UTF-8 (no BOM, matches existing file), CRLF. byte-level edit.
- **Acceptance**:
  - Select-String -Path test\TestWeaselIPC\TestWeaselIPC.cpp -Pattern "system.*pause" returns 0 matches
  - Release\TestWeaselIPC.exe exits with code 0 in PowerShell
    (was: output fine but blocked on pause; with pause removed, clean exit)

## T003 - P1 - create scripts/run-tests.bat

- **File**: scripts/run-tests.bat (NEW)
- **Action**: write the documented build+test wrapper. The script:
  1. Sets BOOST_ROOT and VS dev env (call vcvars32.bat via 8.3 short path)
  2. For each of the 4 test projects, calls
     msbuild <project>.vcxproj /t:Build /p:Configuration=Release
     /p:Platform=Win32 /p:SolutionDir=<absolute-path> (no trailing
     backslash) and checks exit code
  3. For each of the 4 built exes, runs Release\<Name>.exe < nul and
     checks exit code using if !errorlevel! NEQ 0 set FAIL=1 (delayed
     expansion, see L22)
  4. Prints either === ALL TESTS PASSED === or === TESTS FAILED ===
  5. Returns the FAIL value to the outer scope via the
     endlocal & set "OUTER_RC=..." & exit /b pattern
- **Byte-level**: ASCII + CRLF (Windows .bat convention). No BOM.
- **Acceptance**:
  - scripts\run-tests.bat exits non-zero on the current HEAD (because
    TestResponseParser test_4 fails); exits 0 if TestResponseParser test_4
    is fixed
  - The script prints the per-test build + run sections in order
  - Total runtime < 60 seconds on a clean tree
  - Manual check: errorlevel check is !errorlevel! NEQ 0 (delayed
    expansion, NOT if errorlevel 1 which misinterprets -1073741819
    as success; see L22)

## T004 - P1 - update .github/workflows/ci.yml test job

- **File**: .github/workflows/ci.yml
- **Action**: replace the current test job (commit 10b72e2) body with
  a call to scripts\run-tests.bat. The current job only runs
  TestDefaultHotkeys per commit 10b72e2 ("scope test job to TestDefaultHotkeys
  only; ignore release/*.exp+.lib"). Spec 015 expands to all 4 tests.
- **Byte-level**: YAML is UTF-8 (no BOM). Preserve existing line endings
  detected by the file's first line.
- **Acceptance**:
  - The test job in ci.yml calls scripts\\run-tests.bat (or pwsh equivalent)
  - The test job's "needs:" includes the existing build job
  - Manual inspection: workflow is syntactically valid YAML
    (no unbalanced quotes, no tab characters in indentation)

## T005 - P1 - lessons-learned L22 entry

- **File**: .specify/memory/lessons-learned.md
- **Action**: append a new L22 section documenting two anti-patterns:
  1. system("pause") in test code (Windows-console interactive UX
     pattern) crashes with 0xC0000005 in non-interactive environments
     (CI, scripts, redirected stdin). The pattern is a "works on my
     machine" anti-pattern: developer double-clicks the .exe in Explorer,
     it works; CI runs the same .exe with stdin closed, it crashes.
     The right pattern: tests should return 0; directly, NOT call
     system("pause") or any "wait for keypress" function.
  2. if errorlevel 1 in batch scripts misinterprets negative Windows
     STATUS codes (e.g. 0xC0000005 = -1073741819) as "no error" because
     -1073741819 < 1 numerically. The right pattern is
     if !errorlevel! NEQ 0 with delayed expansion, which compares
     the value as a signed integer correctly.
  3. The two are related: the spec 015 fix removed system(pause) to
     expose the real test failure (TestResponseParser test_4), but the
     negative errorlevel then masqueraded as a pass until the script
     adopted the !errorlevel! NEQ 0 pattern.
- **Byte-level**: UTF-8 (no BOM, matches existing file), CRLF. byte-level edit.
- **Acceptance**:
  - Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L22 " returns 1 match
  - File total length grew by ~2000 bytes

## T006 - P1 - bump version + rebuild + commit + tag + push

- **Files** (3 files, env.bat + weasel.props are gitignored per
  AGENTS.md sec 3.3):
  - env.bat: 0.18.8 -> 0.18.9 (FLUXING_VERSION / VERSION_PATCH /
    PRODUCT_VERSION / FILE_VERSION + header comment)
  - weasel.props: 0.18.8 -> 0.18.9 (VERSION_PATCH / PRODUCT_VERSION /
    FILE_VERSION)
  - .gitignore confirms env.bat + weasel.props are untracked after the
    bump (NOT modified)
- **Action**:
  1. Bump env.bat + weasel.props
  2. Run xbuild.bat weasel installer to produce
     output/archives/fluxing-0.18.9.0-installer.exe
  3. Run AGENTS.md sec 2.5 silent-install smoke test on the new
     installer
  4. git add test/TestResponseParser/TestResponseParser.cpp
     test/TestWeaselIPC/TestWeaselIPC.cpp
     scripts/run-tests.bat .github/workflows/ci.yml
     .specify/memory/lessons-learned.md
     .specify/specs/015-fix-test-infrastructure/
     CHANGELOG.md
     release/fluxing-0.18.9.0-installer.exe
  5. git commit -m "fix(test): spec 015 - fix test infrastructure (system pause removal + run-tests.bat + L22)"
  6. git tag v0.18.9.0
  7. git push kizemo Fluxing + git push kizemo v0.18.9.0
- **Acceptance**:
  - xbuild.bat installer exit code 0
  - release/fluxing-0.18.9.0-installer.exe exists, ~42 MB
  - Smoke test: 8 invariants PASS (AGENTS.md sec 2.5)
  - git log --oneline -1 shows the new commit
  - git tag --list v0.18.9.0 returns 1 match
  - git push output shows the v0.18.9.0 tag on kizemo