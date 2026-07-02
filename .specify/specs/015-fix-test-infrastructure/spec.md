# 015 - Fix test infrastructure (TestResponseParser + TestWeaselIPC + system(pause) anti-pattern)

> Scope: make all 4 test projects (TestDefaultHotkeys, TestShiftSelectBinding,
> TestResponseParser, TestWeaselIPC) buildable from a single documented
> command and runnable in non-interactive environments with reliable exit codes.
> TDD.md sec 6 R-008 (CI test job) and sec 2.1 (test projects ship but
> Release exe does not exist).
>
> This spec is the infrastructure prerequisite for spec 016 (Option D -
> behavior-level test framework). It does not add new tests; it makes
> existing tests actually work.

## 0. Background

- TDD.md sec 2.1 (2026-07-01 inventory): TestResponseParser + TestWeaselIPC
  have vcxproj + are registered in weasel.sln, but Release exe does not
  exist (vcxproj never built). TestDefaultHotkeys was fixed in commit
  6014587 (spec 014 / 0.18.7.0) and 10b72e2 (CI test job scope) ships.
- AGENTS.md sec 2.3 documents the 3 test projects but the command
  msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
  fails before reaching the test projects because of pre-existing RC errors
  in WeaselTSF.rc / WeaselServer.rc / WeaselDeployer.rc / WeaselSetup.rc
  (STRZ macro + version WORDs issue, out of scope for this spec).
- Therefore the working pattern is: build each test project's vcxproj
  **standalone** with an explicit /p:SolutionDir= so that
  $(SolutionDir)\\include resolves correctly. This pattern was
  discovered on 2026-07-02 while building spec 015.
- TestResponseParser.cpp and TestWeaselIPC.cpp both contain
  system("pause") as the last line before return. This is a
  Windows-console interactive UX anti-pattern: it works when
  you double-click the exe (waits for keypress), but **crashes with
  0xC0000005 ACCESS_VIOLATION** when stdin is closed (CI, scripts,
  PowerShell with redirected stdin). TestDefaultHotkeys + TestShiftSelectBinding
  do NOT have this anti-pattern and work cleanly in non-interactive mode.

## 1. Product Angle (PRD section)

### 1.1 Goal

Make the existing 4 test projects reliable in CI and in developer inner
loops. No new product features; this is a quality-of-life fix for
testing the product exists.

### 1.2 User Stories

- **US1-A** [P1]: Developer wants to verify a hotkey change does not
  regress the default-key set. Runs one documented command, gets
  35/35 PASS, 13/13 PASS, N/N PASS, M/M PASS within 60 seconds.
- **US1-B** [P1]: CI on github runs the same documented command, gets
  green check on test job, blocks merge on red.
- **US1-C** [P2]: Developer wants to add a new test case to TestResponseParser,
  re-runs the documented command, sees their new assertion included.

### 1.3 Acceptance (GWT)

- Given a clean checkout of Fluxing at v0.18.9.0+ (or current HEAD)
- When the developer runs scripts\run-tests.bat
- Then within 60 seconds, all 4 test exes are built and run
- And the script prints either === ALL TESTS PASSED === or
  === TESTS FAILED === plus which test(s) failed
- And the script exit code matches (0 = pass, non-zero = fail)

- Same command run in GitHub Actions windows-2022 image: same 4
  test exes built and run, same PASS/FAILED output, same exit code

## 2. Technical Angle (TDD section)

### 2.1 Files changed

| File | Change |
|---|---|
| test/TestResponseParser/TestResponseParser.cpp | Remove system(pause) line |
| test/TestWeaselIPC/TestWeaselIPC.cpp | Remove system(pause) line |
| scripts/run-tests.bat | NEW: single-command wrapper that builds + runs all 4 tests |
| .github/workflows/ci.yml | Update test job (TDD.md sec 6.2) to call scripts/run-tests.bat |
| .specify/memory/lessons-learned.md | New L22 entry: system(pause) + !errorlevel! NEQ 0 |
| .specify/specs/015-fix-test-infrastructure/ | spec/plan/tasks three-piece set |
| CHANGELOG.md | New [0.18.9.0-fluxing] section |
| release/fluxing-0.18.9.0-installer.exe | NSIS repack (binaries unchanged from 0.18.8.0) |
| env.bat | 0.18.8 -> 0.18.9 (gitignored) |
| weasel.props | 0.18.8 -> 0.18.9 (gitignored) |

### 2.2 Build & test command (the documented one)

The actual script content is in scripts/run-tests.bat (created in T003).
Pattern: for each of the 4 test projects, call
msbuild test\<Name>\<Name>.vcxproj /t:Build /p:Configuration=Release
/p:Platform=Win32 /p:SolutionDir=<repo-root>\ (explicit SolutionDir is
the workaround for the standalone-build SolutionDir-resolution issue).
Then run each Release\<Name>.exe and check exit code.

### 2.3 Risks and mitigations

- **R1**: scripts/run-tests.bat hard-codes Boost + VS paths. Same
  constraint as env.bat (which is gitignored). Mitigation: script is
  documented as Windows dev machine only; matches AGENTS.md sec 2.1
  prerequisite environment. CI gets the same env via setup-actions.
- **R2**: TestResponseParser test_4 may have a genuine test failure
  (not just the system(pause) crash). **CONFIRMED 2026-07-02**: test_4
  BOOST_ASSERT(2 == c.candies.size()) fails because WeaselIPC's
  ContextUpdater does not implement ctx.cand.0 / ctx.cand.1 array-style
  deserialization. This is a **pre-existing WeaselIPC bug** that was
  previously hidden by system(pause) (the test never reached the
  assertion). Mitigation: spec 015 ships the infrastructure; the
  WeaselIPC fix is filed as separate work item (spec 016+ scope).
- **R3**: Building WeaselIPC standalone triggers a full rebuild of all
  WeaselIPC source files. Mitigation: incremental builds work after
  the first run. 30s first build, 5s incremental.

## 3. Out of scope

- Fixing the pre-existing RC errors in WeaselTSF.rc / WeaselServer.rc /
  WeaselDeployer.rc / WeaselSetup.rc (STRZ2 macro). Separate spec
  (016 or later). These do not block test infrastructure because the
  test projects are built standalone.
- Adding new test cases (Option D - spec 016).
- Modifying the .rc file macros (e.g., defining STRZ2).
- Refactoring WeaselIPC to use $(SolutionDir)\\WeaselIPC in include
  path for build standalone use. This is technically correct but
  masks the real issue (SolutionDir resolution). The script approach
  is the documented workaround.
- Fixing the WeaselIPC ContextUpdater cand.0 / cand.1 deserialization bug
  (revealed by TestResponseParser test_4). Separate spec (016+ scope).

## 4. Implementation steps (T001-T006)

- T001 - P1 - byte-level remove system(pause) from TestResponseParser.cpp
- T002 - P1 - byte-level remove system(pause) from TestWeaselIPC.cpp
- T003 - P1 - create scripts/run-tests.bat
- T004 - P1 - update .github/workflows/ci.yml test job to call scripts/run-tests.bat
- T005 - P1 - lessons-learned L22 entry: system(pause) + !errorlevel! anti-patterns
- T006 - P1 - bump version, rebuild installer, smoke test, commit, tag, push

## 5. Status as of 2026-07-02

- All 4 test projects BUILD successfully via scripts/run-tests.bat
- TestDefaultHotkeys: 35/35 PASS
- TestShiftSelectBinding: 13/13 PASS (spec 014 contract)
- TestWeaselIPC: PASS (output shows successful WeaselServer roundtrip)
- **TestResponseParser: test_4 FAILS** - BOOST_ASSERT(2 == c.candies.size()) in test_4 aborts
  - This is a **pre-existing WeaselIPC bug** surfaced by spec 015's test infrastructure
  - The test expects ctx.cand.0 / ctx.cand.1 array-style deserialization
    to produce 2 candidates, but WeaselIPC's ContextUpdater does not implement it
  - **Out of scope for spec 015** (test infrastructure, not WeaselIPC feature work)
  - Will be filed as separate spec 016+ work item
- spec 015 infrastructure (script + system(pause) removal) SHIPS regardless:
  - script correctly reports === TESTS FAILED === + exit code 1 when TestResponseParser aborts
  - CI gating will block merge on the test_4 failure (which is the correct behavior)

## 6. Done

- All 4 test projects BUILD via scripts/run-tests.bat
- 3/4 test projects RUN successfully (TestResponseParser reveals pre-existing WeaselIPC bug)
- ci.yml test job updated (T004) to call the same script
- 0.18.9.0 installer ships
- L22 lesson recorded (T005)

## 7. References

- TDD.md sec 2.1 (test project inventory) + sec 6.2 (recommended ci.yml test job)
- AGENTS.md sec 2.3 (test commands)
- L21 (spec 014 L19 over-correction): same pattern of two independent
  tests + a lesson entry
- L22 (spec 015 system(pause) + !errorlevel! NEQ 0 anti-patterns, this spec)
- L11 (BOM double rule): system(pause) crash is a related class of
  Windows-isms in code that should be cross-platform