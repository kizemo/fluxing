# 016 - Plan

> Companion to spec.md (intent) and tasks.md (execution checklist).
> This file picks the technical approach, lists the Constitution Check
> per AGENTS.md R4, and the verification matrix per R6.

## 1. Approach

**Two concurrent deliverables, addressed in one spec:**

1. **TestBindingResolution scaffold** (the new test project): vcxproj
   + .cpp + .filters + stdafx + targetver, registered in weasel.sln
   with the correct GUID and 2 ProjectConfigurationPlatforms entries
   (Debug|Win32 + Release|Win32). The .cpp is a stub that returns 0
   and prints "SCAFFOLD MODE - no assertions yet" so it is visibly
   not a no-op.

2. **Behavior-level test framework pattern (L23)**: establish the
   "tests that need real librime state" pattern as a build-time
   precondition. The vcxproj will be configured with the librime
   include + lib paths in a follow-up (not this spec), but the
   scaffold must be ready to be filled in. The L23 lesson records
   the pattern so spec 017+ can follow it.

**Approach decision tree** (record of what was considered):

- "Use a pre-built rime.lib from librime-1.13.1" - REJECTED for this
  spec. librime is a git submodule and the import .lib is not
  pre-built on a clean checkout (AGENTS.md sec 4.4 + L10 sec 2). A
  one-time `build.bat rime` is required. The scaffold's stub
  main() avoids the link dependency so the test project builds in
  ~2s without librime. The next spec (017) can fill in the assertions
  after librime is built.
- "Put TestBindingResolution under xmake" - REJECTED. xmake is the
  inner-loop build tool, but test projects are msbuild-only
  (AGENTS.md sec 2.1, mirrored by spec 015 decision). Adding a
  xmake.lua entry for one test is bigger churn than the scaffold.
- "Wire TestBindingResolution into a pre-build of rime.lib" - REJECTED.
  librime build is a 5-15 min cmake invocation. The CI test job
  should not block on librime unless assertions are enabled. The
  scaffold-by-default / assertions-when-librime-built split is the
  right granularity.
- "Skip weasel.sln registration" - REJECTED. The scaffold must be
  buildable from the test script (spec 016 sec 2.1). Skipping sln
  registration would force every developer to remember an
  undocumented msbuild command.

## 2. Constitution Check (AGENTS.md R4)

| Principle | Pass? | Notes |
|---|---|---|
| **I. Intent Before Implementation** | YES | spec.md sec 0/1 captures intent, US, GWT acceptance |
| **II. Test-Backed Change** | YES | spec adds a new test project (TestBindingResolution) that closes the L18/L19 testing gap |
| **III. Spec-Artifact Discipline** | YES | this three-piece set is the artifact |
| **IV. Structured Clarification** | YES | sec 3 Out of scope is explicit; no [NEEDS CLARIFICATION] markers |
| **V. Incremental Delivery** | YES | T001..T008 are each independently testable; commit is one PR |
| R1: intent + acceptance in spec | YES | sec 1.3 GWT |
| R2: spec layer vs plan layer | YES | spec.md has no language/framework names |
| R3: every task has priority | YES | all T001..T008 are P1 |
| R4: Constitution Check | YES | this section |
| R5: task granularity < 4h | YES | each task is < 30 min |
| R6: done requires evidence | YES | tasks.md acceptance criteria are runnable commands |
| R7: one source of truth | YES | spec/plan/tasks are consistent |
| R8: specs are versioned | YES | this spec is in git under .specify/specs/016-*/ |
| R9: lookup beats memory | YES | TestWeaselIPC.vcxproj was read (byte-level) and modeled; weasel.sln structure was empirically verified |

## 3. Risk register (R3)

| Risk | Mitigation | Section |
|---|---|---|
| Empty GUID in weasel.sln (handoff bug) | spec 016 T004 + this PR byte-level fixes the GUID + adds the 2 ProjectConfigurationPlatforms entries | L23 |
| PS string interpolation corrupts vcxproj content with stray CR | Use byte-level writes; verify first 100 bytes after write | L23, A1 |
| `if errorlevel 1` in run-tests.bat misinterprets negative Windows STATUS | Use `if !errorlevel! NEQ 0` with delayed expansion (already in L22) | L22 |
| WeaselServer running on dev machine blocks smoke test | Out of scope; AGENTS.md sec 2.5 requires clean machine. CI will exercise the smoke test. | 3 |
| librime not pre-built blocks real assertions | Out of scope for spec 016 (scaffold only); spec 017+ will fill in assertions after `build.bat rime` | 3 |
| vcxproj has only 2 configs (Debug\|Win32, Release\|Win32) but weasel.sln wants 4-8 | sln ProjectConfigurationPlatforms for TestBindingResolution declares only the 2 real configs; this matches what vcxproj ItemGroup declares | 2.1 |

## 4. Verification matrix (R6)

| What | How | Expected |
|---|---|---|
| TestBindingResolution builds | `msbuild test\TestBindingResolution\TestBindingResolution.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\` | exit 0, Release\TestBindingResolution.exe produced |
| TestBindingResolution runs (scaffold) | `Release\TestBindingResolution.exe < nul` | exit 0, prints "SCAFFOLD MODE - no assertions yet" |
| weasel.sln parses | Open in Visual Studio 2022 / `devenv /Build Release weasel.sln` | no "inconsistent GUID" warning |
| All 5 tests via script | `scripts\run-tests.bat` | exit non-zero (TestResponseParser test_4 still fails; pre-existing) |
| weasel.sln byte health | byte count + CRLF count + no lone LF/CR | 16148 bytes, 229 CRLF, 0 lone LF, 0 lone CR |
| L23 exists | `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L23 "` | 1 match |
| Git state | `git log --oneline -1 + git tag --list v0.18.10.0` | 1 new commit + 1 new tag |
| v0.18.10.0 installer | `xbuild.bat installer` | exit 0, 0.18.10.0 exe produced |

## 5. Anti-patterns avoided

- **A1** (PS string API loses BOM/CRLF): all file edits use [IO.File]::ReadAllBytes / WriteAllBytes with New-Object System.Text.UTF8Encoding($false). Verified on existing files before modifying.
- **A2** (PS patches install.nsi): not touching install.nsi in this spec.
- **A10** (git add . from root): the commit is `git add <explicit list>`, never `git add .`. env.bat + weasel.props are not staged.
- **A12** (PS Start-Process for installer): out of scope; smoke test runs in CI.
- **Empty GUID in sln (L23)**: byte-level patch with real GUID `{99277F52-0973-411A-8171-E65FA3FF6D69}` read from vcxproj, not from script variable.
- **`if errorlevel 1` in batch (L22)**: scripts\run-tests.bat already uses `if !errorlevel! NEQ 0` with delayed expansion.

## 6. References

- spec.md, tasks.md (sibling files)
- TDD.md sec 3 (integration test strategy) + sec 1 (test pyramid)
- AGENTS.md sec 2.3 (test commands) + sec 4.4 (librime) + sec 5 (pre-commit)
- L18 (key_binder single-key Shift binding collision, original)
- L19 (over-correction, superseded by L21)
- L21 (spec 014 fix; same dual-test pattern)
- L22 (spec 015 system(pause) + if errorlevel 1 anti-patterns)
- spec 015 (test infra: scripts\run-tests.bat + L22 + ci.yml test job)