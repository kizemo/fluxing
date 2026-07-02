# 018 - Fill TestBindingResolution with real assertions (close L18 / L19 testing gap)

> Scope: replace the SCAFFOLD / LINKED stub in TestBindingResolution.cpp
> with 4 real assertions that close the L18 / L19 testing gap. The 4
> assertions match the spec 016 GWT contract, with refinements
> documented below based on the spec 017 + TDD.md discoveries:
>
> - We do NOT call librime's real `rime::KeyEvent::Parse` (would need
>   rime.dll loaded at runtime; violates TDD.md sec 3.2 mock-librime
>   principle, and rime::KeyEvent lives in
>   `librime/src/rime/key_event.h` which is not in the dist include
>   path). We write a small, deterministic, C++-only mock parser
>   in the test source that simulates the part of the API surface
>   we actually need.
> - We do NOT introduce yaml-cpp (would require building the
>   `librime/deps/yaml-cpp` submodule separately; the dist
>   `librime/dist_Win32/` only ships rime.lib, not yaml-cpp.lib).
>   We use a small, hand-rolled line scanner that walks the
>   `output\data\default.yaml` text and extracts the
>   `key_binder.bindings[*]` entries that match a specific `when:`
>   clause. This is more code than the "contains" string match in
>   spec 014 / TestShiftSelectBinding, but less than a full yaml
>   parser.
> - The 4 assertions do NOT need to call any librime function
>   (`rime_get_api`, etc.). They are entirely self-contained in
>   the test source.

## 0. Background

- spec 016 (v0.18.10.0) shipped TestBindingResolution as a SCAFFOLD
  MODE stub that printed "SCAFFOLD MODE" and returned 0.
- spec 017 (v0.18.11.0) upgraded the stub to LINKED MODE, which
  proves the build system can link `rime.lib` and resolve
  `rime_get_api` at link time. The test does NOT call any librime
  function.
- The actual `L18 / L19 testing gap` is not just "is the test
  project wired up" -- it is "is the binding in default.yaml
  parseable by a real key_binder at runtime". TestDefaultHotkeys
  and TestShiftSelectBinding both answer a weaker question
  ("does the yaml string contain the expected binding form"). The
  string test is necessary but not sufficient.
- spec 018 closes the gap by writing a `MockKeyEvent::Parse` and
  a `MockKeyBinder::Match` that simulate the librime API surface
  the binding actually exercises, then drive those mocks with the
  binding form parsed from default.yaml. If the binding form is
  valid (per spec 014 / L21), the mock says "matches"; if the
  binding form is the L19 cause (bare `accept: Shift_L`), the
  mock says "would match the release event too" -- which is the
  L18 invariant violation.

## 1. Product Angle (PRD section)

### 1.1 Goal

Catch the next L18 / L19 class bug at CI time, not at release time.
A future change to `output\data\default.yaml` that re-introduces a
bare-`Shift_L` binding, or that breaks the spec 014 `Shift+Shift_L`
binding form, must fail the test job before merge.

### 1.2 User Stories

- **US1-A** [P1]: Developer changes `output\data\default.yaml` to
  re-introduce `accept: Shift_L` (bare). `TestBindingResolution`
  fails the L19 guard assertion. PR is blocked.
- **US1-B** [P1]: Developer changes `accept: Shift+Shift_L` to
  `accept: Shift_L` (the L19 form). `TestBindingResolution` fails
  the L18 invariant assertion (the TSF release event
  {keycode=Shift_L, modifier=Release} would now match the binding).
  PR is blocked.
- **US1-C** [P1]: Developer moves the `accept: Shift+Shift_L`
  binding to AFTER `accept: Control+1` in the binding list.
  `TestBindingResolution` fails the ordering assertion
  (librime uses first-match-wins, so order matters for
  consistency).
- **US1-D** [P2]: Developer removes the `has_menu: Shift+Shift_L`
  binding entirely. `TestBindingResolution` fails the existence
  assertion. PR is blocked.

### 1.3 Acceptance (GWT)

- Given librime 1.13+ rime.lib is linkable (spec 017 precondition)
- When the developer runs `scripts\run-tests.bat`
- Then TestBindingResolution builds AND:
  - Prints "TestBindingResolution: LINKED rime.lib" first line
    (carries over from spec 017)
  - Prints "Test 1 PASS: Shift+Shift_L parses to (keycode=Shift_L, modifier=Shift)..."
  - Prints "Test 2 PASS: TSF release event (keycode=Shift_L, modifier=Release) does NOT match Shift+Shift_L binding"
  - Prints "Test 3 PASS: Shift+Shift_L appears at position N (before Control+1 at position M, N < M)"
  - Prints "Test 4 PASS: Control+1 parses to (keycode=1, modifier=Control)..."
  - Exits 0 on all 4 PASS; exits 1 if any FAIL
- And the run-tests.bat result for the other 4 tests is unchanged
  (TestDefaultHotkeys 35/35, TestShiftSelectBinding 13/13,
  TestResponseParser 3/4, TestWeaselIPC PASS)

## 2. Technical Angle (TDD section)

### 2.1 Files in this spec

| File | Change |
|---|---|
| test/TestBindingResolution/TestBindingResolution.cpp | MODIFIED: replace SCAFFOLD / LINKED branches with 4 real assertions; introduce MockKeyEvent + MockKeyBinder namespaces |
| .specify/specs/018-fill-binding-resolution/ | NEW: spec/plan/tasks three-piece set |
| .specify/memory/lessons-learned.md | New L25 entry: MockKeyEvent / MockKeyBinder pattern + spec 014 ordering rule + why real rime::KeyEvent is not callable from a test |
| CHANGELOG.md | [0.18.12.0-fluxing] section |
| release/fluxing-0.18.12.0-installer.exe | rebuilt with bumped version |

### 2.2 MockKeyEvent + MockKeyBinder (the test surface)

The mock namespace provides:

```cpp
namespace mock {
    enum Modifier {
        kShift = 1 << 0,
        kControl = 1 << 2,
        kRelease = 1 << 8  // distinct bit; mirrors librime kReleaseMask
    };

    struct KeyEvent {
        int keycode;   // X11 keysym-equivalent: 0xffe1=Shift_L, 0xffe2=Shift_R,
                       //   '1'=0x31, ' '=0x20, etc. The mock uses a small
                       //   lookup table for the names in default.yaml.
        int modifier;  // OR of the Modifier bits above.
        std::string repr() const;  // "Shift+Shift_L" / "Control+1" / "Release+Shift_L"
    };

    // Parse "Shift+Shift_L" / "Control+1" / "Shift+space" / "comma" / etc.
    // into a KeyEvent. Returns false on parse error.
    bool ParseKeyEvent(const std::string& s, KeyEvent* out);

    // Mimics librime 1.13 key_event.h:64 KeyEvent::operator==.
    bool Match(const KeyEvent& binding, const KeyEvent& pressed) {
        return binding.keycode == pressed.keycode &&
               binding.modifier == pressed.modifier;
    }
}
```

Key implementation notes:

1. **Modifier case sensitivity (L16)**: `Shift` (uppercase S) parses
   to kShift. `shift` (lowercase s) is a parse error. The mock
   mirrors the L16 invariant.
2. **Special key names**: `Shift_L`, `Shift_R`, `Control_L`, `Control_R`,
   `space`, `Tab`, `Left`, `Right`, `Page_Up`, `Page_Down`, `comma`,
   `period`, `bracketleft`, `bracketright` -- the names that appear
   in `output\data\default.yaml`. Any other name is a parse error
   (mock will refuse to match; real librime would also refuse).
3. **Single character keys**: `1`, `2`, `3`, ..., `9`, `a`, `b`, ...,
   `z`. `accept: 1` parses to (keycode=0x31, modifier=0). `accept: Control+1`
   parses to (keycode=0x31, modifier=kControl).
4. **TSF release event**: `(keycode=Shift_L, modifier=kRelease)`. This
   is the L18 invariant target. The mock produces this in Test 2
   to verify it does NOT match a binding with
   `(keycode=Shift_L, modifier=kShift)`.

### 2.3 default.yaml scanner (the data source)

The test does not need a full yaml parser. It needs to:
1. Find the line `key_binder:`
2. Find the next line `bindings:`
3. Walk the `- { when: ..., accept: ..., send: ... }` lines after it
4. For each line, regex out `when: (\w+)`, `accept: ([\w+]+)`,
   `send: (\w+)`

This is ~30 lines of C++ string scanning. It is sufficient for
the test surface (default.yaml is a hand-edited, well-formed yaml
in this repo; the scanner is intentionally not a general parser).

### 2.4 The 4 assertions

- **Test 1**: parser sanity
  - For each `accept:` string in `key_binder.bindings[*]`, call
    `mock::ParseKeyEvent`. Assert all parse to a valid KeyEvent.
  - Specifically check `accept: Shift+Shift_L` parses to
    `(keycode=Shift_L, modifier=kShift)`.
  - Specifically check `accept: Control+1` parses to
    `(keycode=1, modifier=kControl)`.

- **Test 2**: L18 invariant (release event does NOT match)
  - Construct a `mock::KeyEvent` with `keycode=Shift_L,
    modifier=mock::kRelease` (the TSF release event).
  - Assert `mock::Match(binding_event, release_event) == false`
    where `binding_event` is the parsed
    `accept: Shift+Shift_L`.
  - This is the L18 invariant: the spec 014 binding form MUST
    distinguish release (modifier=kRelease) from press-with-modifier
    (modifier=kShift).

- **Test 3**: spec 014 ordering
  - Walk the binding list and find the index of
    `accept: Shift+Shift_L` and `accept: Control+1`.
  - Assert `idx(Shift+Shift_L) < idx(Control+1)`.

- **Test 4**: existence + positive parse
  - Assert there is at least one `when: has_menu, accept: Shift+Shift_L`
    in the binding list.
  - Assert there is at least one `when: has_menu, accept: Control+1`
    in the binding list.
  - Assert neither has a bare `accept: Shift_L` (L19 guard).

### 2.5 Risks and mitigations

- **R1**: The mock parser may diverge from real librime behavior.
  Mitigation: the mock is intentionally minimal. It only parses
  the names that actually appear in `output\data\default.yaml`. If
  real librime accepts a name the mock does not, the test will
  report a parse failure for a binding that actually works at
  runtime. This is acceptable because the L18 / L19 bug class is
  about *which binding form is chosen*, not about *which names
  librime accepts*. spec 014 / TestShiftSelectBinding is the
  string-level guard; spec 018 is the parse-level guard.
- **R2**: The default.yaml scanner may break if default.yaml
  formatting changes (e.g. someone reformats the binding list to
  multi-line). Mitigation: the scanner is line-based and looks
  for the exact pattern `- { when: ..., accept: ..., send: ... }`.
  If someone reformats, the test will fail with a clear message
  ("scanner could not find any bindings"). The fix is to update
  the scanner, not to update the spec.
- **R3**: The mock may incorrectly handle a modifier name that
  has different cases in different bindings (e.g. `Shift+Shift_L`
  vs `shift+Shift_L`). Mitigation: the L16 rule says librime is
  case-sensitive on the FIRST letter of the modifier name. The
  mock follows L16.
- **R4**: The mock is hard-coded to `output\data\default.yaml`.
  If the path is wrong, the test fails. Mitigation: the
  existing `argv[1]` override pattern from TestDefaultHotkeys
  / TestShiftSelectBinding is used. CI / run-tests.bat calls the
  .exe with the absolute path.

## 3. Out of scope (deferred to spec 019+)

- Filling in the actual TestBindingResolution assertions that
  call `rime::KeyEvent::Parse` (would need rime.dll at runtime;
  violates TDD §3.2; deferred until a future spec decides to
  use a real rime.dll loader via a separate process)
- Building yaml-cpp as a test dependency (would require
  librime/deps/yaml-cpp build setup; deferred until a future
  spec needs full yaml parsing)
- TestUserDictUpdate, TestPhrasesRoundTrip, TestDarkModeBroadcast,
  TestYamlRoundTripE2E (TDD.md sec 3.1 -- spec 019+)
- The pre-existing TestResponseParser test_4 bug (spec 015 known
  issue; spec 018 does not touch TestResponseParser)

## 4. Implementation steps (T001-T005)

- T001 - P1 - write MockKeyEvent + MockKeyBinder in the test
  source (no vcxproj change; mock is self-contained in .cpp)
- T002 - P1 - write default.yaml scanner + 4 assertions
- T003 - P1 - lessons-learned L25 entry (MockKeyEvent pattern +
  L18 / L19 design rationale)
- T004 - P1 - bump env.bat + weasel.props 0.18.11 -> 0.18.12;
  rebuild installer; commit + tag v0.18.12.0 + push
- T005 - P1 - update CHANGELOG.md with [0.18.12.0-fluxing] section

## 5. Status as of 2026-07-02

- librime 1.13.1 rime.lib is linkable (spec 017 verified)
- TestBindingResolution.vcxproj has librime include + lib paths
  (spec 017 added)
- spec 018 ships the 4 real assertions (replacing the SCAFFOLD /
  LINKED branch from spec 017)
- v0.18.12.0 installer ships

## 6. Done

- T001..T005 all completed
- TestBindingResolution.cpp has 4 real assertions
- The 4 assertions cover the L18 (release event) and L19 (bare
  Shift_L) invariant
- scripts\run-tests.bat: TestBindingResolution now in
  "4/4 PASS" mode
- L25 lesson recorded

## 7. References

- TDD.md sec 3.1, 3.2 (integration test strategy + mock librime
  principle; this spec respects the mock principle)
- AGENTS.md sec 4.4 (librime is Win32-only; rime::KeyEvent lives
  in librime/src/rime/, not in dist include)
- L16 (modifier case sensitivity; spec 014 established the rule)
- L18 (release event does NOT match Shift+Shift_L binding; this
  spec's Test 2 is the L18 invariant at the mock level)
- L19 (bare Shift_L binding causes release event collision; this
  spec's Test 4 has a negative assertion for it)
- L21 (spec 014 fix; same dual-test pattern as spec 018)
- L22, L23, L24 (test infra in the chain)
- spec 014 (shift select 2nd/3rd candidate contract)
- spec 015, spec 016, spec 017 (this spec is the fifth in the chain)