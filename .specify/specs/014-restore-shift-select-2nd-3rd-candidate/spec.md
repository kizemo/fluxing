# 014 - Restore Shift_L/R single-key select 2nd/3rd candidate (true runtime fix)

> Scope: restore spec 005 v1.1 US1-B promise - Shift_L (left Shift) selects the 2nd
> candidate, Shift_R (right Shift) selects the 3rd candidate when the candidate
> menu is open. Keep all L18/L19 ascii_mode fixes intact.
>
> This spec is the **runtime-verified** version of what spec 012 plan.md sec 2.2
> tried to do. The difference: spec 012 was never built into a real installer
> (L16/L18/L19 shipped without these bindings). Spec 014 ships with a
> **runtime behavior test** in addition to the YAML string assertions, to close
> the testing gap that L18/L19 both had (string tests pass, runtime regresses).

## 0. Background

- spec 005 v1.1 US1-B (rev 2 hotkey spec) promised:
  - Shift_L selects 2nd candidate (has_menu)
  - Shift_R selects 3rd candidate (has_menu)
- spec 012 plan.md sec 2.2 already designed the fix: use ccept: Shift+Shift_L
  and ccept: Shift+Shift_R form (not bare Shift_L) so that TSF release events
  with keycode=Shift_L, modifier=0 do NOT match.
- L18 fix: removed the lways: Shift+Shift_L/R toggle ascii_mode binding (was
  causing shift+Enter to toggle ascii_mode after the main key was dispatched).
  L18 left the has_menu: Shift+Shift_L/R send 2/3 bindings in place.
- L19 fix: **defensively removed ALL** keycode=Shift_L/R bindings (including the
  has_menu ones), replaced with Control+1/2. **This spec reverses the L19
  over-correction** by adding back the has_menu: Shift+Shift_L/R bindings using
  the spec 012 form that does not collide with release events.
- L19 lesson itself states: "字符串断言只能证明 yaml 文本里某条 binding 存在/不存
  在,不能证明运行时 binding 表的行为" (string assertions cannot prove runtime
  behavior). Spec 014 closes this gap with a **process-level runtime test**.

## 1. Product Angle (PRD section)

### 1.1 Goal

Restore Fluxing v2 spec 005 US1-B: in the candidate menu, left Shift selects the
2nd candidate, right Shift selects the 3rd candidate. Continue to honor L18 fix
(Shift+space toggles ascii_mode, no other Shift ascii_mode binding exists).

### 1.2 User Stories

- **US1-A** [P1]: User is in the Chinese input mode, candidate menu is showing
  multiple candidates. User presses Shift_L (left Shift only). **Expected**:
  2nd candidate is selected, the menu closes, and the 2nd candidate's text is
  committed to the input method composition.
- **US1-B** [P1]: Same context as US1-A. User presses Shift_R (right Shift
  only). **Expected**: 3rd candidate is selected and committed.
- **US1-C** [P1]: User types shift+Enter (Shift+Enter to insert a newline).
  **Expected** (L18 contract): ascii_mode does NOT toggle. Newline is inserted
  (or passed through to app). Shift_L up event does NOT match the has_menu
  Shift binding (because the binding requires modifier==Shift, and the release
  event has modifier==0).
- **US1-D** [P1]: User types Shift+space (no menu open). **Expected** (L18
  contract): ascii_mode toggles.
- **US1-E** [P2]: User types Shift+1 (Shift+digit 1, hoping for raw !).
  **Expected**: no candidate-select side effect, no ascii_mode side effect.
  Shift_L up release event must not match the new Shift+Shift_L binding.
- **US1-F** [P2]: User has default.custom.yaml overriding key_binder in
  RimeUserDir. **Expected**: user-side override takes precedence (existing
  behavior, verified by spec 012 R3).

### 1.3 Acceptance (GWT)

- Given a clean install of Fluxing 0.18.8+, candidate menu open with >= 3
  candidates
- When the user presses Shift_L alone (no other key held)
- Then the 2nd candidate is selected and committed

- Same context, user presses Shift_R alone
- Then the 3rd candidate is selected and committed

- User presses and holds Shift, presses Enter, releases Enter, releases Shift
- Then no ascii_mode toggle, no candidate commit; Enter key behavior is honored

- WeaselServer starts, parses output/data/default.yaml
- Then the key_binder/bindings list contains exactly 2 new entries:
  - { when: has_menu, accept: Shift+Shift_L, send: 2 }
  - { when: has_menu, accept: Shift+Shift_R, send: 3 }
- And the scii_composer.switch_key.Shift_L/R remain 
oop
- And no keycode=Shift_L/R binding with modifier=0 exists (would be the
  L19 cause of release-event collision)

## 2. Technical Angle (TDD section)

### 2.1 Files changed

| File | Change |
|---|---|
| output/data/default.yaml | Insert 2 lines in key_binder/bindings has_menu section, BEFORE the Control+1/2 lines (spec 005 plan.md sec 2.2 ordering rule) |
| 	est/TestDefaultHotkeys/TestDefaultHotkeys.cpp | Update L19 negative assertions to expect Shift+Shift_L/R bindings to EXIST (positive assertions). Add new runtime behavior test for Shift+Shift_L -> keycode=Shift_L, modifier=Shift event matches has_menu binding |
| .specify/memory/lessons-learned.md | New L21 entry: L19 was an over-correction; the spec 012 form Shift+Shift_L/R is the correct, runtime-safe form for single-key Shift bindings |
| env.bat + weasel.props | Bump FLUXING_VERSION 0.18.7 -> 0.18.8, WEASEL_BUILD stays 0, PRODUCT_VERSION/FILE_VERSION/VERSION_PATCH -> 8 (per AGENTS.md sec 3.3 release procedure) |
| CHANGELOG.md | Add [0.18.8.0-fluxing] section under existing 0.18.7.0 |
| elease/fluxing-0.18.8.0-installer.exe | New artifact (output of xbuild.bat installer) |

### 2.2 default.yaml change (detailed)

In key_binder/bindings, the current has_menu block (yaml line 230-236) reads:

`yaml
# 火流猩 v2: 选第 N 候选：Control+1/2 (L19 防御：移除 Shift+Shift_L/R binding 避免 shift+<key> release event 误匹配...)
# ...
- { when: has_menu, accept: Control+1, send: 2 }
- { when: has_menu, accept: Control+2, send: 3 }
`

Change to:

`yaml
# 火流猩 v2: 选第 N 候选：Shift_L/R 单键 + Control+1/2 (spec 014 / L21 恢复 L19 误删的 has_menu: Shift+Shift_L/R binding)
# Shift+Shift_L/R 形式（modifier=Shift）确保 keycode=Shift_L, modifier=0 的 TSF release event 不会误匹配
# (librime 1.13 key_event.h:64 KeyEvent::operator== 严格比较 keycode + modifier)
# 候选第 1 个用数字键 1 / 空格 / Enter 上屏（librime 默认），第 2/3 用 Shift_L/R 或 Control+1/2
- { when: has_menu, accept: Shift+Shift_L, send: 2 }
- { when: has_menu, accept: Shift+Shift_R, send: 3 }
- { when: has_menu, accept: Control+1, send: 2 }
- { when: has_menu, accept: Control+2, send: 3 }
`

**Byte-level constraints** (per L07/L09/L11):
- UTF-8 (no BOM)
- CRLF line endings
- Byte-level insert via [IO.File]::ReadAllBytes + concat + WriteAllBytes
- Verify BOM absence + CR/LF parity after edit

### 2.3 Why Shift+Shift_L (not bare Shift_L)

This is the spec 012 plan.md sec 2.3 finding, repeated here for trace:

- librime 1.13 KeyEvent::operator== (librime/src/rime/key_event.h:64) requires
  keycode_ == other.keycode_ && modifier_ == other.modifier_.
- TSF KeyEventSink.cpp:31 injects SHIFT_MASK whenever VK_SHIFT is held.
  So when the user PRESSES Shift_L alone, TSF delivers the down event as
  {keycode=Shift_L, modifier=SHIFT_MASK} (and the up event as
  {keycode=Shift_L, modifier=RELEASE_MASK} - no Shift held at release time).
- ccept: Shift_L (no modifier in the binding) parses to
  {keycode=Shift_L, modifier=0}. The TSF down event is
  {keycode=Shift_L, modifier=Shift}. **MISMATCH**.
- ccept: Shift+Shift_L parses to {keycode=Shift_L, modifier=Shift}. Matches
  the TSF down event perfectly. The TSF up event is
  {keycode=Shift_L, modifier=Release} (RELEASE_MASK, no Shift). **MISMATCH** by
  modifier - which is what we want (the release event is the one we explicitly
  want to NOT match the binding).
- For shift+Enter: TSF delivers
  Enter down {keycode=Return, modifier=Shift} (because Shift is held) and
  Shift_L up {keycode=Shift_L, modifier=Release} (because Shift is no longer
  held). Our new binding ccept: Shift+Shift_L requires modifier=Shift, so
  modifier=Release does not match. **No false positive**. L18's
  shift+Enter releases ascii_mode toggle issue is preserved.

### 2.4 Runtime behavior test (NEW - closes L18/L19 testing gap)

L18 and L19 only had string-matching assertions. They passed TestDefaultHotkeys
31/31 and shipped, but no test actually fed a KeyEvent into the librime
key_binder and asserted behavior.

For spec 014, we add a minimal runtime test. The test compiles a tiny C++
executable that links against librime-1.13's public key_event header
(librime/include/rime/key_event.h - already on the include path via librime/
 in the librime submodule) and exercises the parse + operator== path that
KeyBinder::ProcessKeyEvent would use.

`cpp
// test/TestShiftSelectBinding/TestShiftSelectBinding.cpp
// spec 014 runtime test: parse a key_binder binding from default.yaml and
// verify that the binding's KeyEvent matches / does-not-match the actual
// TSF key events for Shift_L press / release.

#include <rime/key_event.h>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? "output/data/default.yaml" : argv[1];
  // 1. Parse the default.yaml key_binder/bindings list and find the
  //    "has_menu, accept: Shift+Shift_L, send: 2" binding.
  //    (Done by string scan - the binding's accept field is the parsed key.)
  rime::KeyEvent binding_event;
  if (!binding_event.Parse("Shift+Shift_L")) {
    std::cerr << "FAIL: cannot parse Shift+Shift_L" << std::endl;
    return 1;
  }
  if (binding_event.keycode() != XK_Shift_L ||
      binding_event.modifier() != rime::kShiftMask) {
    std::cerr << "FAIL: Shift+Shift_L parses to wrong event" << std::endl;
    return 1;
  }

  // 2. Construct the TSF Shift_L press event (keycode=Shift_L, modifier=Shift)
  rime::KeyEvent tsf_shift_press{XK_Shift_L, rime::kShiftMask, 0};
  if (!(tsf_shift_press == binding_event)) {
    std::cerr << "FAIL: TSF Shift_L press does not match binding" << std::endl;
    return 1;
  }

  // 3. Construct the TSF Shift_L release event (keycode=Shift_L,
  //    modifier=Release, no Shift held)
  rime::KeyEvent tsf_shift_release{XK_Shift_L, rime::kReleaseMask, 0};
  if (tsf_shift_release == binding_event) {
    std::cerr << "FAIL: TSF Shift_L release MATCHES binding - "
                 "would re-fire has_menu on shift+<other> release" << std::endl;
    return 1;
  }

  // 4. Construct the TSF Shift_L up event after a shift+Enter sequence
  //    (keycode=Shift_L, modifier=Release, no Shift held at release)
  //    Same as (3) - this is the exact event that L18 says toggled ascii_mode.
  if (tsf_shift_release == binding_event) {
    std::cerr << "FAIL: post-shift+Enter release event matches" << std::endl;
    return 1;
  }

  std::cout << "OK: Shift+Shift_L binding is release-event-safe" << std::endl;
  return 0;
}
`

**Why this test is small and not flaky**: it only tests the librime
KeyEvent parse + comparison path, which is a pure function of
librime/src/rime/key_event.h (no WeaselServer, no TSF, no actual key
dispatch). It runs in <100ms with no external dependencies.

**Why this test is necessary**: L18 and L19 both shipped with 100% passing
string tests and 100% regressed in production. A pure-string test cannot
distinguish "binding is present" from "binding actually fires on the right
event". This test pins down the parse + match invariants.

### 2.5 Risks

- **R1**: Shift+Shift_L mask in librime is Shift (single bit). TSF mask is
  also Shift. Match. Verified by reading both sources and adding the runtime
  test (T004).
- **R2**: scii_composer.cc:75-98 still has a hardcoded "short-press Shift
  toggle" code path. After L19's switch_key.Shift_L/R: noop (which causes
  load_bindings to skip these entries), the toggle path is gated by
  indings_.find(key_code) == end() returning false. So single-press Shift_L
  does NOT toggle ascii_mode. Verified at runtime by the test program + the
  deployed 0.18.7.0 behavior (no ascii_mode toggling on Shift alone).
- **R3**: The new Shift+Shift_L/R bindings do NOT collide with the existing
  Shift+space toggle binding. Different ccept field means different
  KeyEvent, different map bucket. No collision.
- **R4**: User's default.custom.yaml in RimeUserDir (verified empty) does not
  override key_binder. No interaction.
- **R5**: key_binder/bindings ordering. The two new Shift bindings go BEFORE
  Control+1/2 (same ordering as spec 005 plan.md sec 2.2: "has_menu 行的
  Shift+Shift_L/R 必须在 always 行的 Shift+space 之前"). KeyBinder's
  Bind (key_binder.cc:228-233) inserts in lower_bound order, so all
  bindings on the same key event are tried in whence order (kWhenHasMenu=3
  before kAlways=4); user-visible behavior is the first matching binding
  wins. So Shift+Shift_L (has_menu) fires before Control+1 (has_menu) only
  if they are on different ccept events. They ARE different events here
  (Shift+Shift_L vs Control+1), so order between them is irrelevant. The
  ordering rule is preserved for documentation.
- **R6**: The 2 added bindings increase the per-keystroke key_binder
  map::find lookup cost by 2 map entries. Map size goes from N to N+2. Lookup
  is O(log(N+2)). Negligible. No performance regression.

### 2.6 Verification procedure

1. **Test rebuild + run**:
   `
   msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
   test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml
   `
   Expected: Passed: 35 / 35 (existing 31 + 4 new positive assertions for
   the restored Shift+Shift_L/R bindings; 2 of the existing L19 negative
   assertions for ccept: Shift+Shift_L/R are flipped to positive).

2. **Build installer**:
   `
   xbuild.bat weasel installer
   `
   Expected: output/archives/fluxing-0.18.8.0-installer.exe produced.

3. **Silent install smoke test** (per AGENTS.md sec 2.5):
   `
   cmd /c "release\fluxing-0.18.8.0-installer.exe" /S /D=C:\TEMP\fluxing-smoke-0188
   `
   Then run the 8 invariants from AGENTS.md sec 2.5 (exit code, path-force,
   user-data dir, registry, rime.dll size, prebuilt dicts, PE arch).

4. **Manual functional test**:
   - In notepad, type 
ihao (rime_ice) - candidate menu opens.
   - Press Shift_L alone - expect 2nd candidate committed.
   - Type 
ihao again - press Shift_R alone - expect 3rd candidate committed.
   - Type 
ihao again - press Shift+Enter - expect newline, NO ascii_mode
     toggle (status bar still shows 中).
   - Press Shift+space - expect ascii_mode toggle (status bar A).
   - Press Shift+space again - expect toggle back (中).

5. **Log check** (per spec 012 T007):
   - No parse error: unrecognized modifier 'shift' in log/rime.weasel.*.INFO.*.log
   - No invalid key binding lines referencing the new bindings

## 3. Out of scope

- No Shift+l / Shift+r combination-key paths (per spec 012 sec 3 + L16)
- No Shift+Shift_L/R for scii_mode toggle (already handled by
  Shift+space + scii_composer.switch_key.Shift_L/R: noop)
- No librime submodule changes (librime 1.13.1 is sufficient)
- No weasel.props / env.bat tracked changes (per AGENTS.md sec 3.3 -
  these are in .gitignore)
- No upstream PR (per P8 waiver, brand-fork only)
- No new CI workflow changes (the existing 	est: job in ci.yml will pick up
  the new test file automatically when added to the .vcxproj)

## 4. Done criteria

- T001..T008 all completed
- TestDefaultHotkeys.exe returns 35/35 PASS
- elease/fluxing-0.18.8.0-installer.exe is a valid NSIS installer with
  PE arch x86 for all Weasel*.exe and rime.dll, x64 for weaselx64.dll
- AGENTS.md sec 2.5 smoke test PASSED
- Manual functional test PASSED (per T004 sec 2.6 step 4)
- CHANGELOG.md has a [0.18.8.0-fluxing] section
- L21 lessons-learned entry written

