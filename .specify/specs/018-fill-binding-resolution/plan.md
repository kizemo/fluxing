# 018 - Plan

> Companion to spec.md (intent) and tasks.md (execution checklist).
> This file picks the technical approach, lists the Constitution Check
> per AGENTS.md R4, and the verification matrix per R6.

## 1. Approach

**One self-contained change: replace the SCAFFOLD / LINKED stub in
TestBindingResolution.cpp with a 4-assertion mock-driven test.**

The 4 assertions are:
1. Parser sanity: every `accept:` form in
   `key_binder.bindings[*]` parses to a valid `mock::KeyEvent`.
2. L18 invariant: a TSF release event (keycode=Shift_L,
   modifier=Release) does NOT match the parsed
   `accept: Shift+Shift_L` binding.
3. spec 014 ordering: the `Shift+Shift_L` binding appears before
   the `Control+1` binding in the list.
4. Existence + L19 guard: the `Shift+Shift_L` binding exists in
   `when: has_menu`, AND there is NO bare `accept: Shift_L`
   binding in `key_binder`.

**Approach decision tree** (record of what was considered):

- "Call rime::KeyEvent::Parse directly" - REJECTED for 3 reasons:
  (1) `rime::KeyEvent` lives in `librime/src/rime/key_event.h`,
  which is NOT in `librime/dist_Win32/include/` -- the dist only
  ships `rime_api.h` (C API), not the C++ API headers.
  (2) `key_table.h` includes `<X11/keysym.h>` which is Linux-only.
  (3) Calling `Parse` would need `rime.dll` loaded at runtime,
  which violates TDD.md sec 3.2 "integration tests are MOCK
  librime, not real rime.dll loading".
  All three reasons independently rule out the real API. The
  mock is the only viable path.

- "Build yaml-cpp from librime/deps/yaml-cpp" - REJECTED. The
  deps submodules need their own build (cmake; ~5 min). The
  current `librime/dist_Win32/lib/` only ships `rime.lib`, not
  `yaml-cpp.lib`. Adding the build step is bigger churn than the
  test itself. The hand-rolled line scanner is ~30 lines and is
  sufficient for the well-formed yaml in this repo.

- "Use a regex library" - REJECTED. The test is a single .cpp;
  we do not want to add `<regex>` to the includes (it would force
  `LanguageStandard=stdcpp17` is already set, so this would work,
  but adding a regex when hand-coded string scanning is 30 lines
  is overkill). We use `std::string::find` + manual parsing.

- "Add 4 separate test executables" - REJECTED. The 4 assertions
  are 4 invariants of the SAME binding; they share the mock
  state. Splitting them would duplicate the mock setup. One exe
  with 4 PASS/FAIL lines is the right granularity (matches the
  spec 014 / TestShiftSelectBinding pattern).

- "Make the test fail on L18 / L19 violations" - this is the
  whole point of the test. The 4 assertions are designed to
  catch these violations (Test 2 is L18, Test 4 has an L19
  negative assertion).

## 2. Constitution Check (AGENTS.md R4)

| Principle | Pass? | Notes |
|---|---|---|
| **I. Intent Before Implementation** | YES | spec.md sec 0/1 captures intent, US, GWT acceptance |
| **II. Test-Backed Change** | YES | spec 018 IS a real test; it adds 4 assertions that catch L18 / L19 class bugs |
| **III. Spec-Artifact Discipline** | YES | this three-piece set is the artifact |
| **IV. Structured Clarification** | YES | sec 3 Out of scope is explicit; sec 2.5 R1-R4 cover design risks |
| **V. Incremental Delivery** | YES | T001..T005 are each independently testable; commit is one PR |
| R1: intent + acceptance in spec | YES | sec 1.3 GWT |
| R2: spec layer vs plan layer | YES | spec.md has no language/framework names (except mocking approach documented in sec 2.2) |
| R3: every task has priority | YES | all T001..T005 are P1 |
| R4: Constitution Check | YES | this section |
| R5: task granularity < 4h | YES | each task is < 1 hour |
| R6: done requires evidence | YES | tasks.md acceptance criteria are runnable commands |
| R7: one source of truth | YES | spec/plan/tasks are consistent; TDD.md sec 3.2 mock-librime principle respected |
| R8: specs are versioned | YES | this spec is in git under .specify/specs/018-*/ |
| R9: lookup beats memory | YES | default.yaml structure verified by byte-level read; rime::KeyEvent location verified by ls librime/src/rime/ |

## 3. Risk register (R3)

| Risk | Mitigation | Section |
|---|---|---|
| MockKeyEvent::Parse diverges from real librime | Mock is intentionally minimal; only handles names in default.yaml; spec 014 / TestShiftSelectBinding is the string-level guard for the binding form | 2.5 R1 |
| default.yaml scanner breaks on reformat | Scanner is line-based; `- { when: ..., accept: ..., send: ... }` is the exact pattern; failure mode is "no bindings found" + clear error message | 2.5 R2 |
| Modifier case-sensitivity bug (L16) | Mock follows L16: `Shift` (uppercase) parses, `shift` (lowercase) is error; Test 1 explicitly exercises this | 2.5 R3 |
| Test path resolution (default.yaml location) | Use `argv[1]` override (matches TestDefaultHotkeys / TestShiftSelectBinding); run-tests.bat passes absolute path | 2.5 R4 |
| Mock parser adds noise to .cpp | mock namespace is isolated; namespace block is ~80 lines; .cpp byte count delta is expected ~2-3 KB | 2.1 |

## 4. Verification matrix (R6)

| What | How | Expected |
|---|---|---|
| TestBindingResolution builds | `msbuild test\TestBindingResolution\TestBindingResolution.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir=<repo>\` | exit 0, Release\TestBindingResolution.exe produced |
| TestBindingResolution runs | `Release\TestBindingResolution.exe` | exit 0, prints 4 PASS lines |
| All 5 tests via script | `scripts\run-tests.bat` | exit 0 for TestBindingResolution; non-zero overall due to TestResponseParser test_4 (pre-existing) |
| Test 1 (parser sanity) | manual: run .exe, count "Test 1 PASS" lines | 1 PASS, 0 FAIL |
| Test 2 (L18 invariant) | manual: run .exe, count "Test 2 PASS" lines | 1 PASS, 0 FAIL |
| Test 3 (spec 014 ordering) | manual: run .exe, count "Test 3 PASS" lines | 1 PASS, 0 FAIL |
| Test 4 (existence + L19 guard) | manual: run .exe, count "Test 4 PASS" lines | 1 PASS, 0 FAIL |
| TestBindingResolution.cpp byte health | byte count grows by ~2-3 KB (mock + scanner + 4 asserts) | exact delta tracked |
| L25 exists | `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L25 "` | 1 match |
| Git state | `git log --oneline -1 + git tag --list v0.18.12.0` | 1 new commit + 1 new tag |
| v0.18.12.0 installer | `xbuild.bat installer` | exit 0, ~42 MB exe produced |

## 5. Anti-patterns avoided

- **A1** (PS string API loses BOM/CRLF): all file edits use [IO.File]::ReadAllBytes / WriteAllBytes with `New-Object System.Text.UTF8Encoding($false)`. Verified on existing files before modifying.
- **A2** (PS patches install.nsi): not touching install.nsi in this spec.
- **A10** (git add . from root): the commit is `git add <explicit list>`, never `git add .`. env.bat + weasel.props are not staged.
- **A12** (PS Start-Process for installer): out of scope; smoke test runs in CI.
- **Empty GUID in sln (L23)**: spec 018 does not touch weasel.sln.
- **`if errorlevel 1` in batch (L22)**: scripts\run-tests.bat already uses `if !errorlevel! NEQ 0` with delayed expansion; no change needed.
- **system("pause") in test code (L22)**: spec 018 does not add system("pause"); test output flushes via std::endl.
- **call librime symbols from test (TDD §3.2)**: spec 018 uses mock namespace only; no rime.dll loaded at runtime.
- **RIME_VERSION in test (AP-L24-A)**: spec 018 does not reference RIME_VERSION; the spec 017 sizeof(RimeApi) sanity check is still present.

## 6. References

- spec.md, tasks.md (sibling files)
- TDD.md sec 3.1, 3.2 (integration test strategy + mock librime principle)
- AGENTS.md sec 4.4 (librime is Win32-only, rime::KeyEvent location) + sec 3.3 (version bump)
- L16 (modifier case sensitivity)
- L18 (release event does NOT match Shift+Shift_L binding)
- L19 (bare Shift_L binding causes release event collision)
- L21, L22, L23, L24 (lessons in the chain)
- spec 014, spec 015, spec 016, spec 017 (this spec is the fifth in the chain)