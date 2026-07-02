# 017 - Plan

> Companion to spec.md (intent) and tasks.md (execution checklist).
> This file picks the technical approach, lists the Constitution Check
> per AGENTS.md R4, and the verification matrix per R6.

## 1. Approach

**One real deliverable + one documentation deliverable:**

1. **Wire librime include + lib paths into TestBindingResolution.vcxproj**
   so that adding a real `rime.lib` link is a one-line change in spec 018+
   (only need to add `rime.lib` to AdditionalDependencies, the include
   + lib paths are already there).
2. **Add a link-probe branch in TestBindingResolution.cpp** that uses
   `__has_include(<rime_api.h>)` to switch between "SCAFFOLD MODE"
   (no rime_api.h) and "LINKED rime.lib" (rime_api.h found AND
   symbol resolved at link time). This makes the link state visible
   at run time without requiring rime.dll to be loaded.

**Approach decision tree** (record of what was considered):

- "Add `rime.lib` to AdditionalDependencies unconditionally" -
  REJECTED. spec 016 §2.4 R1 explicitly designs for the case where
  rime.lib is missing. Unconditional link would break the "scaffold
  builds on clean checkout without librime" property that spec 016
  promised. The `__has_include` guard is the right granularity.
- "Add a runtime `access()` check instead of `__has_include`" -
  REJECTED. `__has_include` is C++17, faster, and catches the
  missing-header case at compile time. The runtime case (lib
  present but symbol unresolved) is still caught by LNK2001 from
  the extern pointer declaration.
- "Rebuild rime.lib fresh" - REJECTED. The existing
  `librime/dist_Win32/lib/rime.lib` (built 2026-07-01, 293,342
  bytes) is linkable (verified by smoke test). Rebuilding would
  take 5-15 minutes (cmake + msbuild) and produce the same bytes
  (input is identical: librime 1.13.1 commit 1c233581, no source
  changes). The verification matrix confirms the link works.
- "Use a real rime_get_api() call" - REJECTED. Per TDD.md sec 3.2
  integration tests are MOCK librime, not real rime.dll loading.
  The link-probe declares the symbol (forces link) but does NOT
  call it (would need rime.dll at runtime). spec 018+ will mock
  the key_binder; spec 017 only proves the link path works.
- "Move include/ to TestBindingResolution" - REJECTED. The existing
  `include/` at repo root is the librime C API header location per
  AGENTS.md sec 4.4 (`copy rime_*.h include\`). Moving it would
  change the weasel.props include path for all projects.

## 2. Constitution Check (AGENTS.md R4)

| Principle | Pass? | Notes |
|---|---|---|
| **I. Intent Before Implementation** | YES | spec.md sec 0/1 captures intent, US, GWT acceptance |
| **II. Test-Backed Change** | YES | spec 017 IS a test-infra change; verification matrix has a real link-probe test |
| **III. Spec-Artifact Discipline** | YES | this three-piece set is the artifact |
| **IV. Structured Clarification** | YES | sec 3 Out of scope is explicit; no [NEEDS CLARIFICATION] markers |
| **V. Incremental Delivery** | YES | T001..T005 are each independently testable; commit is one PR |
| R1: intent + acceptance in spec | YES | sec 1.3 GWT |
| R2: spec layer vs plan layer | YES | spec.md has no language/framework names |
| R3: every task has priority | YES | all T001..T005 are P1 |
| R4: Constitution Check | YES | this section |
| R5: task granularity < 4h | YES | each task is < 30 min |
| R6: done requires evidence | YES | tasks.md acceptance criteria are runnable commands |
| R7: one source of truth | YES | spec/plan/tasks are consistent; TDD.md sec 3.2 mock-librime principle respected |
| R8: specs are versioned | YES | this spec is in git under .specify/specs/017-*/ |
| R9: lookup beats memory | YES | librime dist path + rime_api.h location verified by byte-level read; smoke test link ran |

## 3. Risk register (R3)

| Risk | Mitigation | Section |
|---|---|---|
| `__has_include(<rime_api.h>)` returns true on a fresh checkout without librime built (header missing) | If header is missing, `__has_include` returns 0, code falls back to SCAFFOLD MODE; LNK is unaffected (no extern declaration in SCAFFOLD branch) | 2.4 R1 |
| Link-probe declares `rime_get_api` symbol; if rime.lib missing, LNK2001 | The whole point of the link-probe is to surface this. If it surfaces, the dev runs `build.bat rime` to fix it. Documented in run-tests.bat header. | 2.4 R2 |
| Adding librime\include to AdditionalIncludeDirectories may shadow vendored yaml-cpp / glog headers used by rime.dll | The test project does not use yaml-cpp / glog. The include path is additive. If spec 018+ adds yaml-cpp parsing, it can use the absolute path to librime\include\yaml-cpp. | 2.4 R4 |
| env.bat / weasel.props 0.18.10 -> 0.18.11 bump accidentally staged in commit | AGENTS.md sec 3.3 + L22: env.bat + weasel.props are gitignored. `git status -s` after bump must show `??` not ` M`. Verified manually. | T005 |
| librime submodule pointer dirty from previous builds | spec 017 does NOT commit any librime changes (AGENTS.md sec 4.4: no librime edits). The dirty pointer is a pre-existing condition, not in spec scope. | 3 |

## 4. Verification matrix (R6)

| What | How | Expected |
|---|---|---|
| rime.lib link smoke | `cl /nologo /EHsc /I include _t.cpp /link /LIBPATH:librime\dist_Win32\lib rime.lib /OUT:_t.exe` (already passed 2026-07-02) | exit 0, _t.exe ~89 KB |
| TestBindingResolution vcxproj builds | `msbuild test\TestBindingResolution\TestBindingResolution.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\` | exit 0, Release\TestBindingResolution.exe produced |
| TestBindingResolution runs (linked) | `Release\TestBindingResolution.exe < nul` | exit 0, prints "LINKED rime.lib" first line |
| run-tests.bat (all 5) | `scripts\run-tests.bat` | exit 0 for TestBindingResolution line; non-zero overall due to TestResponseParser test_4 (pre-existing) |
| weasel.sln byte health | unchanged from spec 016 (16148 B, 229 CRLF) | matches spec 016 |
| TestBindingResolution.vcxproj byte health | byte count grows by ~250 bytes (2 include path changes, 2 lib path changes × 2 configs) | exact delta tracked |
| TestBindingResolution.cpp byte health | byte count grows by ~400 bytes (link-probe branch) | exact delta tracked |
| L24 exists | `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L24 "` | 1 match |
| Git state | `git log --oneline -1 + git tag --list v0.18.11.0` | 1 new commit + 1 new tag |
| v0.18.11.0 installer | `xbuild.bat installer` | exit 0, ~42 MB exe produced |

## 5. Anti-patterns avoided

- **A1** (PS string API loses BOM/CRLF): all file edits use [IO.File]::ReadAllBytes / WriteAllBytes with `New-Object System.Text.UTF8Encoding($false)`. Verified on existing files before modifying.
- **A2** (PS patches install.nsi): not touching install.nsi in this spec.
- **A10** (git add . from root): the commit is `git add <explicit list>`, never `git add .`. env.bat + weasel.props are not staged.
- **A12** (PS Start-Process for installer): out of scope; smoke test runs in CI.
- **Empty GUID in sln (L23)**: spec 017 does not touch weasel.sln.
- **`if errorlevel 1` in batch (L22)**: scripts\run-tests.bat already uses `if !errorlevel! NEQ 0` with delayed expansion; no change needed.
- **system("pause") in test code (L22)**: spec 017 does not add system("pause"); in fact, the link-probe branch's std::cout is explicitly flushed via std::endl.

## 6. References

- spec.md, tasks.md (sibling files)
- TDD.md sec 3.1, 3.2 (integration test strategy + mock librime principle)
- AGENTS.md sec 4.4 (librime is Win32-only, build.bat rime flow) + sec 3.3 (version bump)
- L18, L19, L21, L22, L23 (lessons in the chain)
- spec 014, spec 015, spec 016 (this spec is the third in the chain)