# 016 - Behavior-level test framework (Option D from PRD/TDD)

> Scope: scaffold the integration test framework per TDD.md sec 3, with
> TestBindingResolution as the first concrete test. This spec does not
> ship a fully-runnable test; it ships the vcxproj + scaffold + spec
> + plan + tasks, and is the prerequisite for any later test that
> needs to drive librime's actual key_binder / user_dict_update / etc.
>
> Per TDD.md sec 3, the integration test strategy is:
> - TestBindingResolution.cpp - parse spec 005 yaml actual binding
>   is librime-accepted (L18 key gap)
> - TestUserDictUpdate.cpp - spec 008 mock RimeUserDict
> - TestPhrasesRoundTrip.cpp - spec 009 phrases.json
> - TestDarkModeBroadcast.cpp - spec 004 sec 9 WM_SETTINGCHANGE
> - TestYamlRoundTripE2E.cpp - spec 007 round-trip

## 0. Background

- TDD.md sec 1 declares the test pyramid:
  "Integration (mock librime)" is the missing layer.
- L18 (spec 005 v1.1) and L19 (spec 005 v1.1 over-correction) both
  shipped 100% string-passing / 100% runtime-regressing fixes. Root
  cause: the string test in TestDefaultHotkeys + spec 014's
  TestShiftSelectBinding cannot prove that librime's key_binder
  actually matches the binding on a real KeyEvent.
- spec 005 v1.1 US1-B (Shift_L/R select 2nd/3rd candidate) is now
  restored (spec 014 / L21), but the **next** key_binder change will
  regress again unless we have a real runtime test.
- spec 014 / L21 ships a partial fix: TestShiftSelectBinding
  re-parses the yaml and asserts the *string* binding form. It does
  not exercise librime's actual key_binder.

## 1. Product Angle (PRD section)

### 1.1 Goal

Close the L18 / L19 testing gap. A future change to key_binder / ascii_composer
that re-introduces a release-event collision or a missing has_menu binding
must fail CI before merge, not after release.

### 1.2 User Stories

- **US1-A** [P1]: Developer changes a binding in default.yaml.
  Runs the new TestBindingResolution. The test fails if librime's
  key_binder does not match the expected KeyEvent for the binding.
- **US1-B** [P1]: CI runs TestBindingResolution on every push. PR is
  blocked if a binding regresses.
- **US1-C** [P2]: Developer adds a new binding. The new binding is
  automatically picked up by TestBindingResolution (data-driven).

### 1.3 Acceptance (GWT)

- Given librime 1.13+ is built (output\rime.dll + librime\build\lib\Release\rime.lib)
- When the developer runs scripts\run-tests.bat
- Then TestBindingResolution builds + runs and verifies:
  - ccept: Shift+Shift_L (spec 014) matches a real KeyEvent with
    keycode=Shift_L, modifier=kShiftMask (NOT kReleaseMask)
  - ccept: Control+1 matches a real KeyEvent with
    keycode=1 (key_table.h), modifier=kControlMask
  - ccept: Shift+space matches a real KeyEvent with
    keycode=space, modifier=kShiftMask
  - The TSF release event (keycode=Shift_L, modifier=kReleaseMask)
    does NOT match the ccept: Shift+Shift_L binding (the L18
    invariant)
- And the test exits 0
- And the test is part of scripts\run-tests.bat (TDD.md sec 3.3)

## 2. Technical Angle (TDD section)

### 2.1 Files in this spec

| File | Change |
|---|---|
| test/TestBindingResolution/TestBindingResolution.cpp | NEW: scaffold source with planned test cases documented in code |
| test/TestBindingResolution/TestBindingResolution.vcxproj | NEW: vcxproj (modeled on TestWeaselIPC.vcxproj, with librime deps) |
| test/TestBindingResolution/TestBindingResolution.vcxproj.filters | NEW: solution explorer filter |
| scripts/run-tests.bat | Add TestBindingResolution to the test list |
| .specify/specs/016-behavior-level-test-framework/ | spec/plan/tasks three-piece set |
| .specify/memory/lessons-learned.md | New L23 entry: behavior-level test framework pattern |

### 2.2 vcxproj key configuration

The vcxproj is modeled on TestWeaselIPC.vcxproj with these changes:
- AdditionalIncludeDirectories adds $(SolutionDir)\librime\include
- AdditionalLibraryDirectories adds $(SolutionDir)\librime\build\lib\Release
- AdditionalDependencies adds rime.lib (librime C API)
- OutputDir: Release\
- Configuration: Release|Win32 (per AGENTS.md sec 2.3)

### 2.3 Test source scaffold (planned implementation)

The test file will:
1. Load output\data\default.yaml and parse the key_binder.bindings
   list using yaml-cpp (vendored in librime\include\yaml-cpp)
2. For each binding, parse ccept: and send: strings using
   rime::KeyEvent::Parse and rime::KeySequence::Parse
3. Initialize rime::KeyEvent objects with various modifier combinations
   (release, Shift, Control, no-modifier) and verify the key_binder
   matching logic via the C API (rime_get_api()->process_key)
4. Assert:
   - The spec 014 binding (Shift+Shift_L) matches only when
     modifier=kShiftMask (NOT kReleaseMask)
   - The release event (keycode=Shift_L, modifier=kReleaseMask)
     does NOT match the binding (L18 invariant)
   - Control+1 matches when modifier=kControlMask

### 2.4 Risks and mitigations

- **R1**: librime\build\lib\Release\rime.lib may not exist on a clean
  checkout. Mitigation: spec 016 includes a precondition check
  (#if __has_include(<rime_api.h>) or runtime check) that skips
  the test if librime is not built. CI will build librime first.
- **R2**: rime::KeyEvent::Parse modifier case-sensitivity (L16):
  Shift+Shift_L (uppercase) is accepted; shift+shift_l (lowercase)
  is rejected. Test must use uppercase form.
- **R3**: yaml-cpp vendored in librime\include\yaml-cpp is a
  third-party library. Test must not modify it.
- **R4**: Linking rime.lib may pull in dependencies (yaml-cpp, boost,
  opencc, marisa). All vendored.

## 3. Out of scope (deferred to spec 017+)

- Filling in the actual TestBindingResolution assertions (requires
  librime to be built and the rime API to be loaded)
- TestUserDictUpdate, TestPhrasesRoundTrip, TestDarkModeBroadcast,
  TestYamlRoundTripE2E (all in TDD.md sec 3)
- Migrating TestResponseParser test_4 to a real integration test
  (TDD.md sec 8 identifies this gap)
- Re-enabling the AGENTS.md sec 2.5 silent-install smoke test on
  developer machines (requires stopping the running WeaselServer)

## 4. Implementation steps (T001-T005)

- T001 - P1 - create test/TestBindingResolution/TestBindingResolution.cpp
- T002 - P1 - create test/TestBindingResolution/TestBindingResolution.vcxproj
- T003 - P1 - create test/TestBindingResolution/TestBindingResolution.vcxproj.filters
- T004 - P1 - register TestBindingResolution in weasel.sln
- T005 - P1 - add TestBindingResolution to scripts/run-tests.bat
- T006 - P1 - lessons-learned L23 entry
- T007 - P1 - bump env.bat + weasel.props to 0.18.10.0 (test infra only)
- T008 - P1 - rebuild installer + commit + tag v0.18.10.0 + push

## 5. Status as of 2026-07-02

- spec 016 ships the SCAFFOLD (vcxproj + .cpp placeholder + sln
  registration + script hook + spec/plan/tasks)
- The actual assertions in TestBindingResolution.cpp are stubbed
  with comments describing the planned behavior
- The test will be **enabled** when librime is built (a one-time
  build task per AGENTS.md sec 4.4) and the .cpp is filled in
- Until then, the test project builds with a stub main() that
  returns 0 (and prints "TestBindingResolution: SCAFFOLD MODE -
  no assertions yet" so it's clear the test is not a no-op)

## 6. Done

- T001..T008 all completed
- vcxproj + .cpp + .filters + sln registration all in place
- scripts\run-tests.bat builds + runs the scaffold
- 0.18.10.0 installer ships (test-infra-only release, like 0.18.9.0)
- L23 lesson recorded

## 7. References

- TDD.md sec 3 (integration test strategy)
- L18 (key_binder single-key Shift binding collision, original)
- L19 (over-correction, superseded by L21)
- L21 (spec 014 fix; same dual-test pattern)
- L22 (spec 015 system(pause) + if errorlevel 1 anti-patterns)
- spec 005 v1.1 US1-B (Shift_L/R select 2nd/3rd candidate promise)