# 014 Plan - Restore Shift_L/R single-key select 2nd/3rd candidate (runtime-verified)

## Summary

This plan delivers a single-file fix to output/data/default.yaml that
**reverses the L19 over-correction** and restores the has_menu:
Shift+Shift_L/R send 2/3 bindings promised by spec 005 v1.1. The fix uses the
spec 012 form (ccept: Shift+Shift_L, not bare Shift_L) so that the binding
matches the TSF Shift_L down event ({keycode=Shift_L, modifier=Shift}) but
**does not match** the TSF release event ({keycode=Shift_L, modifier=Release}).

The change is **2 line inserts** (no deletions) in the existing
key_binder/bindings list, plus a rewrite of TestDefaultHotkeys.cpp to flip
L19 negative assertions to positive, and a **new** tiny C++ runtime test
	est/TestShiftSelectBinding/ that pins down the parse + match invariants.

The test gap that caused L18 and L19 to ship 100% string-passing, 100%
runtime-regressing fixes is **structurally closed** by this spec.

## Technical Context

- **Language / version**: YAML 1.2 (config) + C++ (test, /std:c++17)
- **Build system**: xmake (primary, xbuild.bat weasel installer) for
  installer; MSBuild (msbuild weasel.sln) for the test binaries
- **Dependencies (unchanged)**: librime 1.13.1 (submodule at
  librime/src/rime/), already verified for RimeGetModifierByName case
  sensitivity (L16) and for KeyEvent::operator== strict keycode+modifier
  comparison (librime/src/rime/key_event.h:64)
- **Storage / files touched**:
  - output/data/default.yaml - 2 line inserts + comment update
  - 	est/TestDefaultHotkeys/TestDefaultHotkeys.cpp - flip 2 L19 negatives
    to positives, add 4 new positive assertions
  - 	est/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - runtime
    test for parse + match invariants
  - 	est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -
    MSBuild project (modeled on TestDefaultHotkeys.vcxproj)
  - 	est/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)
  - weasel.sln - add 1 Project() block + 12 ProjectConfigurationPlatforms
    entries for the new test
  - .specify/memory/lessons-learned.md - new L21 entry
  - env.bat + weasel.props - bump FLUXING_VERSION 0.18.7 -> 0.18.8, etc.
  - CHANGELOG.md - new [0.18.8.0-fluxing] section
  - elease/fluxing-0.18.8.0-installer.exe (new) - copy from
    output/archives/ after successful xbuild.bat installer
- **Platform**: Windows 8.1 ~ Windows 11; x86 (per L10/L14, librime Win32-only)
- **Project type**: Windows TSF IME front-end
- **Performance**: 2 new map entries in key_binder; O(log N) lookup cost;
  negligible
- **Constraints**:
  - YAML must stay UTF-8 (no BOM) + CRLF (per L11)
  - C++ test files follow .clang-format and /std:c++17
  - Conventional Commits with luxing scope (per P4 amendment 1)
  - All 3 test projects must pass msbuild weasel.sln /t:TestDefaultHotkeys;TestShiftSelectBinding;TestResponseParser;TestWeaselIPC /p:Configuration=Release /p:Platform=Win32
  - No new third-party dependency (P3)
- **Scale**: 1 commit, ~6 file paths touched

## Constitution Check (against .specify/memory/constitution.md v1.1.0)

| Principle | Pass? | Notes |
|---|---|---|
| I. Intent Before Implementation | yes | spec.md Goal + 6 prioritized user stories; this plan opens with "restore spec 005 v1.1 US1-B, runtime-verified" |
| II. Test-Backed Change | yes | NEW: TestShiftSelectBinding pins down parse + match invariants at runtime. Closes the L18/L19 testing gap. |
| III. Spec-Artifact Discipline | yes | spec/plan/tasks in this directory |
| IV. Structured Clarification | yes | No [NEEDS CLARIFICATION] in spec; scope was clarified in chat 2026-07-02 |
| V. Incremental Delivery | yes | One file YAML insert + test additions + version bump; independently shippable |

| Hard rule | Pass? | Notes |
|---|---|---|
| R1 (intent + acceptance in chat) | yes | This plan + spec state the intent and the GWT acceptance |
| R2 (no tech words in spec.md) | yes | spec.md mentions librime 1.13.1 only as a citation; no TSF/xmake/etc. in spec.md body |
| R3 (priority + US tag in tasks.md) | yes | All 8 tasks carry P1/P2 priority and link back to US1-A..US1-F |
| R4 (Constitution Check in plan.md) | yes | This section |
| R5 (<= 4h, 1-3 files per task) | yes | T001, T002, T003 each touch exactly 1 file; T004 touches 1 new file; T005 touches sln + 3 new files (close) |
| R6 (paste test output) | yes | T003 mandates running all 4 test exes; T008 mandates pasting build output |
| R7 (one source of truth) | yes | spec.md sec 2.2 is the single source of truth for the binding list |
| R8 (specs versioned in git) | yes | The 3 new documents are committed in the same atomic commit as the YAML fix |
| R9 (lookup beats memory) | yes | Source-of-truth citations: librime/src/rime/key_event.h:64, librime/src/rime/gear/key_binder.cc:228-233, librime/src/rime/gear/ascii_composer.cc:73-98, WeaselTSF/KeyEventSink.cpp:31 |

## Source-of-Truth Citations

| Claim | File:line | Verified by |
|---|---|---|
| KeyEvent::operator== strict keycode+modifier | librime/src/rime/key_event.h:64 | direct read in L19 evidence chain |
| KeyBinder::Bind lower_bound ordering | librime/src/rime/gear/key_binder.cc:228-233 | direct read |
| AsciiComposer::load_bindings skips noop | librime/src/rime/gear/ascii_composer.cc:24-45 | direct read |
| AsciiComposer::ToggleAsciiModeWithKey returns false on missing key | librime/src/rime/gear/ascii_composer.cc:193-203 | direct read |
| TSF injects SHIFT_MASK when VK_SHIFT held | WeaselTSF/KeyEventSink.cpp:31 | direct read (L19 evidence) |
| installed default.yaml == source default.yaml | D:\\Program Files\\fluxing\\weasel\\data\\default.yaml == F:\\soft\\00selfmade\\rime\\output\\data\\default.yaml (SHA256 A8586044DD25528B...81CD391) | direct hash compare |
| key_binder cc:271-287 strict find on KeyEvent | librime/src/rime/gear/key_binder.cc:271-287 | direct read |
| Installed rime.dll == source rime.dll (3041792 bytes) | output/Win32/rime.dll vs D:\\Program Files\\fluxing\\weasel\\rime.dll | direct compare |

## Risk Register

| ID | Risk | Mitigation |
|---|---|---|
| R1 | Shift+Shift_L mask equals Shift; TSF mask also Shift; matches | runtime test (T004) pins the parse+match invariant |
| R2 | ascii_composer.cc:75-98 still has short-press Shift toggle; after L19's switch_key.Shift_L/R: noop, load_bindings skips; ToggleAsciiModeWithKey returns false; no toggle | pre-existing behavior, verified by 0.18.7.0 user test (no Shift-alone ascii toggle) |
| R3 | new Shift+Shift_L/R binding collides with Shift+space toggle | different ccept -> different KeyEvent -> different map bucket; no collision |
| R4 | user-side default.custom.yaml overrides key_binder | verified empty in D:\\Program Files\\fluxing\\user1\\fluxing\\default.custom.yaml (0 bytes) |
| R5 | binding list ordering changed | spec 005 plan.md sec 2.2 rule (has_menu before always) preserved; intra-has_menu order between Shift+Shift_L and Control+1 is irrelevant (different KeyEvent) |
| R6 | 2 new map entries -> O(log N) lookup, negligible | measure if needed; 100% ignored unless we exceed 100 bindings |
| R7 | TestShiftSelectBinding links librime headers in user code path - is librime/include on the include path? | Yes, librime/include/rime/key_event.h is in the librime submodule and is the public API; vcxproj must add librime/include to AdditionalIncludeDirectories |
| R8 | WeaselServer restart needed for yaml change to take effect | xbuild.bat installer triggers uninstall+reinstall per L20; restart of WeaselServer is implicit in the reinstall path |

## Deliverable

- 1 commit on the Fluxing branch (with a 2nd commit for the version bump
  if needed; see tasks.md T006)
- Commit message uses ix(fluxing): scope per P4
- No upstream PR (per P8 waiver, brand-fork only)
- Optional lightweight tag 0.18.8.0 (Fluxing-side, not pushed to kizemo
  unless user requests)
