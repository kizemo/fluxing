# 014 Tasks - Restore Shift_L/R single-key select 2nd/3rd candidate (runtime-verified)

> Single release on Fluxing. Each task <= 1 file (or tightly-coupled group),
> <= 30 min. Acceptance: T003 + T005 test binaries return 0; T007 smoke test
> PASS; T008 manual functional test PASS.

## T001 - P1 - US1-A/B - insert 2 Shift+Shift_L/R bindings in default.yaml

- **File**: output/data/default.yaml
- **Action**: in the key_binder/bindings list, **insert** 2 lines BEFORE the
  existing Control+1/2 lines, and update the L19 comment to reflect the
  spec 014 restore:

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

- **Byte-level**:
  - UTF-8 (no BOM, per L11)
  - CRLF line endings per L11
  - Use byte-level replace via [IO.File]::ReadAllBytes + concat +
    WriteAllBytes. The L19 comment (line 230-234) is the unique anchor
    for the replace; do not do a global text search-and-replace.
  - **Pre-verify uniqueness**: Select-String confirms the L19 comment is
    present exactly once before doing the replace.

- **Acceptance**:
  - Test-Bom output\data\default.yaml returns "no-BOM" (first 3 bytes are
    not 0xEF 0xBB 0xBF)
  - CR count == LF count (full CRLF parity)
  - File size delta is +N bytes (insertion only, no deletion); expected
    ~600 bytes including comment update
  - git diff --stat output/data/default.yaml shows the change in the
    key_binder/bindings section only
  - Select-String -Path output\data\default.yaml -Pattern
    "accept: Shift\+Shift_L, send: 2" returns 1 match
  - Select-String -Path output\data\default.yaml -Pattern
    "accept: Shift\+Shift_R, send: 3" returns 1 match
  - Select-String -Path output\data\default.yaml -Pattern
    "accept: Shift\+Shift_L\"" returns 0 matches (L19 negative preserved
    for the no-modifier form, which is the bare-Shift_L form)
  - Line 76 Shift_L: noop and line 77 Shift_R: noop unchanged

## T002 - P1 - US1-A/B - flip L19 negative assertions to positive + add new positives

- **File**: test/TestDefaultHotkeys/TestDefaultHotkeys.cpp
- **Action**:
  - Change L19 negative assertion line 81:
    - FROM: check("L19: accept: Shift+Shift_L 任意 binding 已不存在", !Contains(content, "accept: Shift+Shift_L"));
    - TO: check("L14: has_menu: accept: Shift+Shift_L, send: 2 已恢复 (spec 014 / L21)", Contains(content, "accept: Shift+Shift_L, send: 2"));
  - Change L19 negative assertion line 82:
    - FROM: check("L19: accept: Shift+Shift_R 任意 binding 已不存在", !Contains(content, "accept: Shift+Shift_R"));
    - TO: check("L14: has_menu: accept: Shift+Shift_R, send: 3 已恢复 (spec 014 / L21)", Contains(content, "accept: Shift+Shift_R, send: 3"));
  - Change L19 negative assertion line 99-100:
    - FROM: check("L19 负断言: has_menu Shift+Shift_L 已移除 (L19)", content.find("accept: Shift+Shift_L") == std::string::npos);
    - TO: check("L14: has_menu Shift+Shift_L binding 已恢复 (spec 014)", content.find("accept: Shift+Shift_L") != std::string::npos);
  - Change L19 negative assertion line 101-102:
    - FROM: check("L19 负断言: has_menu Shift+Shift_R 已移除 (L19)", content.find("accept: Shift+Shift_R") == std::string::npos);
    - TO: check("L14: has_menu Shift+Shift_R binding 已恢复 (spec 014)", content.find("accept: Shift+Shift_R") != std::string::npos);
  - Add 4 NEW positive assertions (after line 65, before line 68):
    `cpp
    // F1 选第 2/3 候选：Shift_L/R 单键 (spec 014 / L21 恢复)
    check("Shift_L 单键选 2 候选 (has_menu) - spec 014 恢复",
          Contains(content, "accept: Shift+Shift_L, send: 2"));
    check("Shift_R 单键选 3 候选 (has_menu) - spec 014 恢复",
          Contains(content, "accept: Shift+Shift_R, send: 3"));
    // F1 ascii_composer switch_key 保持 noop (防止与 has_menu binding 双触发)
    check("ascii_composer.Shift_L: noop 保持 (spec 014)",
          Contains(content, "Shift_L: noop"));
    check("ascii_composer.Shift_R: noop 保持 (spec 014)",
          Contains(content, "Shift_R: noop"));
    `
  - Keep the L19 negative assertions for the **bare** Shift_L/R form (which
    would be the L19 worry): lines 95-106 are still valid because we are not
    adding accept: Shift_L (no modifier). The negative assertion
    content.find("toggle: ascii_mode, accept: Shift_L") == std::string::npos
    remains true.

- **Byte-level**: UTF-8 (no BOM), CRLF. byte-level edit.

- **Acceptance**:
  - git diff --stat test/TestDefaultHotkeys/TestDefaultHotkeys.cpp shows
    the 4 + 4 line changes
  - TestDefaultHotkeys.cpp file size delta is +N bytes
  - cl /EHsc /std:c++17 /I include test\TestDefaultHotkeys\TestDefaultHotkeys.cpp
    compiles clean

## T003 - P1 - US1-A/B - rebuild + run all test projects

- **Action**:
  `
  msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
  test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe output\data\default.yaml
  test\TestShiftSelectBinding\Release\TestShiftSelectBinding.exe
  test\TestResponseParser\Release\TestResponseParser.exe
  test\TestWeaselIPC\Release\TestWeaselIPC.exe
  `
- **Expected**:
  - TestDefaultHotkeys.exe -> Passed: 35 / 35 (existing 31 + 4 new positive
    assertions; 2 of the L19 negatives were flipped, 2 new positives added,
    net change +4)
  - TestShiftSelectBinding.exe -> prints "OK: Shift+Shift_L binding is
    release-event-safe" and exits 0
  - TestResponseParser.exe and TestWeaselIPC.exe -> exit 0 (no regression)
- **Acceptance**:
  - All 4 test exes return 0
  - In final assistant message, paste the last 10 lines of each test exe's
    output (R6: evidence)

## T004 - P1 - US1-A/B - create TestShiftSelectBinding (NEW project, runtime test)

- **Files** (3 new files + 1 weasel.sln edit):
  - test/TestShiftSelectBinding/TestShiftSelectBinding.cpp (new) - the
    runtime test code from spec 014 plan.md sec 2.4
  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj (new) -
    based on test/TestDefaultHotkeys/TestDefaultHotkeys.vcxproj, with
    AdditionalIncludeDirectories += librime/include
  - test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj.filters (new)
  - weasel.sln - add 1 Project() block + 12
    ProjectConfigurationPlatforms entries (model on TestDefaultHotkeys)
  - .gitignore - no change (existing patterns cover test/**/Release/)

- **Action**:
  - The cpp file content is in spec.md sec 2.4. Key points:
    - #include <rime/key_event.h> (librime public header)
    - Construct rime::KeyEvent from {keycode, modifier, release_mask}
    - Use rime::KeyEvent::Parse("Shift+Shift_L") and check
      keycode() == XK_Shift_L && modifier() == kShiftMask
    - Construct TSF-style press and release events
    - Assert tsf_press == binding_event and !(tsf_release == binding_event)
    - Exit 0 on success, 1 on any FAIL

- **Acceptance**:
  - All 4 test exes compile and run in T003
  - TestShiftSelectBinding.exe prints "OK: Shift+Shift_L binding is
    release-event-safe"
  - weasel.sln file size delta is +N bytes (around 1300)

## T005 - P1 - US1-D - bump version to 0.18.8.0 in env.bat + weasel.props

- **Files** (2 files, both .gitignored per AGENTS.md sec 3.3 - this is
  expected):
  - env.bat - change FLUXING_VERSION=0.18.7 -> 0.18.8,
    VERSION_PATCH=7 -> 8, PRODUCT_VERSION=0.18.7.0 -> 0.18.8.0,
    FILE_VERSION=0.18.7.0 -> 0.18.8.0, header comment 0.18.7.0 -> 0.18.8.0
  - weasel.props - same VERSION_PATCH/PRODUCT_VERSION/FILE_VERSION bump
- **Byte-level**: UTF-8 (no BOM for env.bat; BOM for weasel.props as it's
  xml). CRLF. byte-level edit.
- **Acceptance**:
  - git status shows env.bat and weasel.props as untracked (NOT
    modified) per AGENTS.md sec 3.3 ("weasel.props and env.bat are in
    .gitignore - the version bump files do NOT get committed")
  - After xbuild.bat installer, the resulting installer name is
    output/archives/fluxing-0.18.8.0-installer.exe

## T006 - P1 - xbuild.bat installer

- **Action**:
  `
  xbuild.bat weasel installer
  `
- **Expected**:
  - xbuild.bat exit code 0
  - output/archives/fluxing-0.18.8.0-installer.exe produced (~42 MB)
- **Acceptance**:
  - In final assistant message, paste the last 30 lines of xbuild.bat
    output (R6: evidence)
  - release/fluxing-0.18.8.0-installer.exe is the new file

## T007 - P1 - US1-D - silent install smoke test (AGENTS.md sec 2.5)

- **Action**: copy release/fluxing-0.18.8.0-installer.exe to a temp
  location, run the 8 invariants from AGENTS.md sec 2.5:
  1. exit code == 0
  2. fluxing suffix was forced (engine binaries under fluxing\weasel)
  3. user-data dir is co-located under fluxing\user1\fluxing\
  4. HKLM InstallDir is the fluxing root
  5. HKCU RimeUserDir is under user1\fluxing\
  6. rime.dll is present and 2-5 MB
  7. prebuilt dicts present
  8. PE arch consistency (Weasel*.exe x86, weaselx64.dll x64)
- **Acceptance**:
  - All 8 invariants PASS
  - In final assistant message, paste the smoke test report
- **Use cmd /c wrapper per L20**:
  `
  cmd /c "\"release\fluxing-0.18.8.0-installer.exe\" /S /D=C:\TEMP\fluxing-smoke-0188"
  `

## T008 - P1 - manual functional test (4 sub-tests per spec 014 sec 1.3)

- **Action**: after T007 smoke test, in notepad:
  1. Type 
ihao - candidate menu opens
  2. Press Shift_L alone - expect 2nd candidate committed
  3. Type 
ihao again - press Shift_R alone - expect 3rd candidate
     committed
  4. Type 
ihao again - press Shift+Enter - expect newline, NO ascii_mode
     toggle (status bar still shows 中)
  5. Press Shift+space - expect ascii_mode toggle (status bar A)
  6. Press Shift+space again - expect toggle back (中)
- **Acceptance**:
  - All 4 sub-tests PASS
  - In final assistant message, paste the test transcript

## T009 - P1 - lessons-learned L21 entry

- **File**: .specify/memory/lessons-learned.md
- **Action**: append new L21 entry documenting:
  - L19 was an over-correction (removed useful has_menu Shift+Shift_L/R
    bindings to defend against a different bug)
  - The spec 012 form accept: Shift+Shift_L (not bare Shift_L) is the
    runtime-safe way to bind single-key Shift
  - String-matching tests alone cannot verify key_binder behavior;
    runtime test on KeyEvent::Parse + operator== is necessary
  - User-visible impact: "按 Shift_L 在候选窗选 2nd 候选" was silently
    broken across 0.18.6.0 + 0.18.7.0
- **Byte-level**: UTF-8 (no BOM), CRLF. byte-level edit.
- **Acceptance**:
  - Select-String -Path .specify\memory\lessons-learned.md -Pattern
    "^##\s+L21 " returns 1 match

## T010 - P1 - commit + tag + push + release

- **Action**:
  `
  git add output/data/default.yaml test/TestDefaultHotkeys/TestDefaultHotkeys.cpp
  git add test/TestShiftSelectBinding/ weasel.sln
  git add .specify/memory/lessons-learned.md
  git add .specify/specs/014-restore-shift-select-2nd-3rd-candidate/
  git add CHANGELOG.md
  git add release/fluxing-0.18.8.0-installer.exe
  git commit -m "fix(fluxing): spec 014 - restore Shift_L/R select 2nd/3rd candidate (L21)"
  git tag v0.18.8.0
  # DO NOT push unless user explicitly requests (AGENTS.md sec 3.4:
  # Fluxing brand-fork has no CI release)
  `
- **Acceptance**:
  - git log --oneline -1 shows the new commit
  - git status is clean after commit
  - git tag shows v0.18.8.0
  - In final assistant message, paste commit hash

## 9. Cross-spec consistency audit (R7)

- spec 005 v1.1 design.md sec 2.2 example still uses accept: shift+l
  (lowercase). This is a pre-existing inconsistency from spec 012 sec 9
  audit. Not blocking for this commit.
- L19 entry in lessons-learned.md will be updated to note it was an
  over-correction; not a separate task, included in T009.
- spec 012 sec 3 Out of scope still applies (no Shift+l/r combination-key
  paths). Not changed by this commit.

## 10. Done

- T001..T010 all completed
- TestDefaultHotkeys 35/35 PASS, TestShiftSelectBinding PASS
- 0.18.8.0 installer built + smoke test PASS + manual functional test PASS
- L21 written, spec 014 committed, v0.18.8.0 tagged
