# 018 - Tasks

## T001 - P1 - write MockKeyEvent + MockKeyBinder in TestBindingResolution.cpp

- **File**: test/TestBindingResolution/TestBindingResolution.cpp
- **Action**: byte-level replace the current SCAFFOLD / LINKED main()
  with a new main() that:
  1. Declares a `mock` namespace with `KeyEvent`, `Modifier`, `ParseKeyEvent`, `Match`
  2. Uses a hand-coded key name lookup table (Shift_L, Shift_R, Control_L,
     Control_R, space, Tab, Left, Right, Page_Up, Page_Down, comma,
     period, bracketleft, bracketright, 0-9, a-z)
  3. Parses `accept: Shift+Shift_L` style strings with case-sensitive
     first-letter modifier (L16)
  4. Implements `Match` exactly as `binding.keycode == pressed.keycode &&
     binding.modifier == pressed.modifier` (mirrors librime 1.13
     key_event.h:64 KeyEvent::operator==)
- **Mock keycode table** (small hardcoded map):
  - "Shift_L" -> 0xffe1
  - "Shift_R" -> 0xffe2
  - "Control_L" -> 0xffe3
  - "Control_R" -> 0xffe4
  - "space" -> 0x20
  - "Tab" -> 0xff09
  - "Left" -> 0xff51
  - "Right" -> 0xff53
  - "Page_Up" -> 0xff55
  - "Page_Down" -> 0xff56
  - "comma" -> 0x2c
  - "period" -> 0x2e
  - "bracketleft" -> 0x5b
  - "bracketright" -> 0x5d
  - "0"-"9" -> 0x30-0x39
  - "a"-"z" -> 0x61-0x7a
  - "grave" -> 0x60 (for Control+grave / Control+Shift+grave)
  - "exclam" -> 0x21 (for Control+Shift+exclam in commented bindings)
  - "at" -> 0x40 (similar)
  - "dollar" -> 0x24 (for Control+Shift+dollar commented)
  - "1"-"9" -> 0x31-0x39
- **Modifier table** (case-sensitive first letter per L16):
  - "Shift" -> mock::kShift
  - "Control" -> mock::kControl
  - "Alt" -> mock::kAlt (kMod1)
  - "Super" -> mock::kSuper (kMod4)
  - "Release" -> mock::kRelease (kReleaseMask mirror)
  - lowercase variants are PARSE ERRORS
- **Acceptance**:
  - `Release\TestBindingResolution.exe` builds (msbuild)
  - First stdout line is "TestBindingResolution: LINKED rime.lib" (carries over from spec 017)
  - The 4 PASS / FAIL assertions follow
  - Exit code 0 if all 4 PASS; 1 if any FAIL

## T002 - P1 - default.yaml scanner + 4 assertions

- **File**: test/TestBindingResolution/TestBindingResolution.cpp
- **Action**: in main(), after the link-probe from spec 017:
  1. Read `output\data\default.yaml` (via argv[1] override pattern)
  2. Find the `key_binder:` line, then `bindings:` line after it
  3. For each subsequent `- { when: ..., accept: ..., send: ... }` line,
     regex out the 3 fields via std::string::find
  4. Build a `std::vector<Binding>` where `Binding = {when, accept_parsed, send}`
  5. Run the 4 assertions on this vector
- **Test 1 (parser sanity)**:
  - For each binding in the vector, the parsed `accept_parsed` is valid (keycode != 0)
  - Print "Test 1 PASS: parsed N bindings" or "Test 1 FAIL: ..."
- **Test 2 (L18 invariant)**:
  - Find the binding with `accept: Shift+Shift_L`
  - Construct a `mock::KeyEvent` with keycode=0xffe1 (Shift_L), modifier=mock::kRelease
  - Assert `mock::Match(binding_event, release_event) == false`
  - Print "Test 2 PASS: TSF release event (Shift_L, Release) does NOT match Shift+Shift_L binding"
- **Test 3 (spec 014 ordering)**:
  - Find indices: idx_shift = index of `accept: Shift+Shift_L`,
    idx_ctrl = index of `accept: Control+1`
  - Assert `idx_shift < idx_ctrl`
  - Print "Test 3 PASS: Shift+Shift_L at position N, Control+1 at position M (N < M)"
- **Test 4 (existence + L19 guard)**:
  - Assert there exists a binding with `when: has_menu, accept: Shift+Shift_L`
  - Assert there exists a binding with `when: has_menu, accept: Control+1`
  - Assert there is NO binding with `accept: Shift_L` (bare, not `Shift+Shift_L`)
  - Print "Test 4 PASS: has_menu Shift+Shift_L exists; no bare accept: Shift_L (L19 guard)"
- **Byte-level**: replace the SCAFFOLD / LINKED main() body. byte
  count grows by ~3-4 KB. The spec 017 `__has_include` guard
  stays; the SCAFFOLD branch now also has the 4 assertions
  (in SCAFFOLD mode, the 4 assertions would fail loudly because
  default.yaml may not be loadable; the test would exit 1 with
  a "default.yaml not found" error -- this is correct behavior).
- **Acceptance**:
  - `Release\TestBindingResolution.exe` builds
  - First line: "TestBindingResolution: LINKED rime.lib (rime_get_api resolved at link time, sizeof(RimeApi)=396)"
  - Then 4 "Test N PASS" lines
  - Exit code 0

## T003 - P1 - lessons-learned L25 entry

- **File**: .specify/memory/lessons-learned.md
- **Action**: append a new L25 section.
- **Key points**:
  1. rime::KeyEvent (C++ API) is in `librime/src/rime/key_event.h`,
     NOT in `librime/dist_Win32/include/`. The dist only ships
     `rime_api.h` (C API). For behavior-level tests, the C++
     API is not directly accessible.
  2. `key_table.h` (which defines the modifier constants) includes
     `<X11/keysym.h>` -- Linux-only. Cannot be included from
     a Windows test source.
  3. The MOCK approach (TDD §3.2) is the only viable path: write
     a small C++ namespace in the test source that simulates the
     keycode + modifier + match logic, without linking or loading
     rime.dll. The mock is intentionally minimal (only the names
     actually in default.yaml).
  4. The mock's `Match` function (keycode + modifier ==) is the
     exact behavior librime 1.13 key_event.h:64 uses. This is the
     L18 invariant in code form.
  5. The 4 assertions catch the next L18 / L19 class bug. They
     are not a substitute for spec 014 / TestShiftSelectBinding
     (string-level) -- they are a complementary guard at the
     parse level.
- **Acceptance**:
  - `Select-String -Path .specify\memory\lessons-learned.md -Pattern "^##\s+L25 "` returns 1 match
  - File total length grew by ~2500 bytes

## T004 - P1 - bump version 0.18.11 -> 0.18.12 + CHANGELOG + L25

- **Files** (tracked):
  - test/TestBindingResolution/TestBindingResolution.cpp (MODIFIED)
  - .specify/memory/lessons-learned.md (MODIFIED)
  - .specify/specs/018-fill-binding-resolution/* (3 files, NEW)
  - CHANGELOG.md (MODIFIED -- 0.18.12.0 section)
  - release/fluxing-0.18.12.0-installer.exe (NEW, ~42 MB)
- **Files** (gitignored, NOT in commit):
  - env.bat: 0.18.11 -> 0.18.12
  - weasel.props: 0.18.11 -> 0.18.12

## T005 - P1 - rebuild installer + commit + tag + push

- **Action**:
  1. Bump env.bat + weasel.props
  2. `xbuild.bat installer` to produce 0.18.12.0 installer
  3. Move installer to `release/fluxing-0.18.12.0-installer.exe`
  4. `git add` only the explicit tracked paths above
  5. `git commit -m "test(fluxing): spec 018 - fill TestBindingResolution with 4 real assertions (L18 / L19 invariant)"`
  6. `git tag v0.18.12.0`
  7. `git push kizemo Fluxing + git push kizemo v0.18.12.0`
- **Acceptance**:
  - xbuild.bat installer exit code 0
  - release/fluxing-0.18.12.0-installer.exe exists, ~42 MB
  - git log --oneline -1 shows the new commit
  - git tag --list v0.18.12.0 returns 1 match
  - git push output shows the v0.18.12.0 tag on kizemo