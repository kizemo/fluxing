# 015 - Plan

> Companion to spec.md (intent) and tasks.md (execution checklist).
> This file picks the technical approach, lists the Constitution Check
> per AGENTS.md R4, and the verification matrix per R6.

## 1. Approach

**Three concurrent issues, all addressed in one spec:**

1. **Test infrastructure works only when invoked from solution build**:
   $(SolutionDir) resolves to the .vcxproj's own directory when built
   standalone, breaking $(SolutionDir)\include and the 8 of 8
   WeaselIPC.vcxproj AdditionalIncludeDirectories entries. Fix: invoke
   msbuild with explicit /p:SolutionDir=<repo-root>\ to override the
   default. Documented as a single .bat wrapper to avoid every
   developer having to remember the override.

2. **system("pause") anti-pattern in 2 test files**: this is a
   "works on my machine" pattern that crashes with 0xC0000005 when
   stdin is closed. Fix: remove the line. Cost: developer can no
   longer double-click the .exe to see the test output. Acceptable:
   the .exes are not the user-facing product; they are CI artifacts.

3. **CI test job is incomplete**: per TDD.md sec 6.2, the test job
   should run all 4 test projects; commit 10b72e2 scoped it to
   TestDefaultHotkeys only. Fix: call scripts\run-tests.bat from
   the test job so CI and developer-inner-loop share the same entry
   point.

**Approach decision tree** (record of what was considered):

- "Patch WeaselIPC.vcxproj to add $(SolutionDir)\WeaselIPC" - REJECTED.
  This works around the issue but masks the real problem
  ($(SolutionDir) is undefined in standalone vcxproj build). The
  workaround is local to one project; the underlying issue would
  reappear if anyone else ever tries to build WeaselIPC standalone.
  The script-with-explicit-SolutionDir pattern is the right fix
  at the right level (the script knows it's running standalone).
- "Move WeaselIPC source headers to include/" - REJECTED. The current
  architecture is deliberate: include/ is for public WeaselIPC
  headers consumed by WeaselServer / WeaselDeployer / WeaselSetup,
  WeaselIPC/ is for internal implementation. Don't churn the
  architecture for a test infrastructure issue.
- "Use xmake to build the test projects" - REJECTED. xmake is the
  inner-loop build tool, but test projects are msbuild-only
  (AGENTS.md sec 2.1). Adding xmake.lua for the 4 test projects is
  a bigger churn than the script fix.
- "Just remove system(pause) and call it done" - PARTIALLY REJECTED.
  Removing system(pause) is necessary but not sufficient. Without
  the script, every developer has to remember the msbuild-with-
  explicit-SolutionDir incantation.

## 2. Constitution Check (AGENTS.md R4)

| Principle | Pass? | Notes |
|---|---|---|
| **I. Intent Before Implementation** | YES | spec.md sec 0/1 captures intent, US, GWT acceptance |
| **II. Test-Backed Change** | YES | spec adds + improves tests; verification is the new test command |
| **III. Spec-Artifact Discipline** | YES | this three-piece set is the artifact |
| **IV. Structured Clarification** | YES | sec 3 Out of scope is explicit; no [NEEDS CLARIFICATION] markers |
| **V. Incremental Delivery** | YES | T001..T006 are each independently testable; commit is one PR |
| R1: intent + acceptance in spec | YES | sec 1.3 GWT |
| R2: spec layer vs plan layer | YES | spec.md has no language/framework names |
| R3: every task has priority | YES | all T001..T006 are P1 |
| R4: Constitution Check | YES | this section |
| R5: task granularity < 4h | YES | each task is < 30 min |
| R6: done requires evidence | YES | tasks.md acceptance criteria are runnable commands |
| R7: one source of truth | YES | spec/plan/tasks are consistent |
| R8: specs are versioned | YES | this spec is in git under .specify/specs/015-*/ |
| R9: lookup beats memory | YES | WeaselIPC source files were Get-ChildItem-enumerated; SolutionDir resolution was empirically verified by running msbuild /t:Rebuild with /v:diagnostic |

## 3. Risk register (R3)

| Risk | Mitigation | Section |
|---|---|---|
| Hard-coded paths in scripts\run-tests.bat break on non-matching dev machine | Documented as "Windows dev machine only"; matches env.bat constraint | 2.3 R1 |
| Removing system(pause) breaks developer's habit of double-clicking .exe | Documented in L22; the .exe is a CI artifact, not user-facing | L22 |
| SolutionDir override might not work for all msbuild versions | Tested on msbuild 17.x (VS 2022 BuildTools); fallback is "build through solution" which is the AGENTS.md sec 2.3 documented command | 2.3 R3 |
| Pre-existing RC errors block solution-level build | Out of scope (sec 3); test projects build standalone so RC errors don't apply | 3 |

## 4. Verification matrix (R6)

| What | How | Expected |
|---|---|---|
| TestResponseParser builds | msbuild test\TestResponseParser\TestResponseParser.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\ | exit 0, Release\TestResponseParser.exe produced |
| TestResponseParser runs | Release\TestResponseParser.exe < nul | exit 0 (no ACCESS_VIOLATION) |
| TestWeaselIPC builds | same pattern | exit 0, exe produced |
| TestWeaselIPC runs | Release\TestWeaselIPC.exe < nul | exit 0 |
| All 4 tests via script | scripts\run-tests.bat | exit 0, "35/35 PASS" + "13/13 PASS" printed |
| CI workflow valid | Get-Content .github\workflows\ci.yml (no tabs in indent) | parses as YAML |
| Installer smoke test | AGENTS.md sec 2.5 recipe | 8 invariants PASS |
| Git state | git log --oneline -1 + git tag --list v0.18.9.0 | 1 new commit + 1 new tag |
| L22 exists | Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L22 " | 1 match |

## 5. Anti-patterns avoided

- **A1** (PS string API loses BOM/CRLF): all file edits use [IO.File]::ReadAllBytes / WriteAllBytes with New-Object System.Text.UTF8Encoding \False. Verified on existing files before modifying.
- **A2** (PS patches install.nsi): not touching install.nsi in this spec.
- **A10** (git add . from root): the commit is git add <explicit list>, never git add .. env.bat + weasel.props are not staged.
- **A12** (PS Start-Process for installer): smoke test uses cmd /c, not Start-Process.
- **system(pause) anti-pattern (L22)**: spec removes the line from both files.

## 6. References

- spec.md, tasks.md (sibling files)
- TDD.md sec 2.1 (test inventory) + sec 6.2 (recommended ci.yml test job)
- AGENTS.md sec 2.3 (test commands) + sec 5 (pre-commit checklist)
- L21 (spec 014 lesson, same dual-test pattern)