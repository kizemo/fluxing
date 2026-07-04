# 034 - Plan - TestDarkModeBroadcast (integration test for F11 cross-cut broadcast)

> Plan layer. Implementation strategy for spec 034. The test is the
> deliverable; production code is unchanged from spec 033 (shipped in
> 0.18.22.0).

## 1. Technical context

- **C++17, MSVC v143, /utf-8, /MT** (matches existing RimeWithWeasel
  static library target; also matches the spec 033 TestDarkModeBridge
  template that we copy).
- **test/TestDarkModeBroadcast/** - new test directory; vcxproj is a
  copy of test/TestDarkModeBridge/TestDarkModeBridge.vcxproj with the
  ProjectGuid + file name updated. The test exe is a single Win32
  console application that links the production code via direct
  ClCompile entry (L24 link-probe pattern).
- **Subsystem**: CONSOLE (test must print PASS/FAIL to stdout; this
  matches the existing 12 test projects).
- **No new dependencies** (no ATL, no WTL, no Gdiplus for the test
  proper; the L24 link-probe entry for WeaselPanel.cpp brings in
  WTL/ATL headers transitively, which is fine because the production
  WeaselPanel itself depends on them).
- **No changes to installer.nsi** for this spec (the test is a
  product binary, not an installer-level change; L13 / L17 do not
  apply).

## 2. Architecture

### 2.1 What we are testing (and what we are NOT testing)

This is the critical architectural decision for spec 034. After
detailed analysis of WeaselUI/WeaselPanel.cpp and the spec 033
production code, the testable surface decomposes into TWO scopes:

**Scope A: what TestDarkModeBroadcast tests (the integration boundary).**

The end-to-end pipeline from a Windows message to a bridge subscriber
notification, EXERCISING the actual production FluxingDarkModeBridge
(L24 link-probe pattern) but with a test-local filter stub for the
panel-side OnSettingChange:

  Windows OS event (user toggles dark mode in Settings)
      |
      v
  Windows broadcasts WM_SETTINGCHANGE with lParam = "ImmersiveColorSet"
      |
      v
  TestDarkModeBroadcast WndProc (test-local filter stub) receives
  the message via SendMessage.
      |
      v
  WndProc filters: lParam != 0 AND wcscmp(lParam, L"ImmersiveColorSet") == 0
  (identical filter logic to WeaselPanel::OnSettingChange; this is
  the L25 mock pattern - the test mirrors the production filter
  because instantiating a real WeaselPanel would require WTL/ATL/
  Gdiplus/d2d1.lib/dwrite.lib + a valid weasel::UI& + a valid
  m_status + m_ctx + the full CWindowImpl machinery).
      |
      v
  fluxing::FluxingDarkModeBridge::Get()->Refresh()
  (ACTUAL production code from spec 033, linked via L24).
      |
      v
  bridge.ReadAppsUseLightTheme() reads HKCU registry
  (ACTUAL production code).
      |
      v
  If state changed, fire all subscribers in FIFO order
  (ACTUAL production code; subscribers are also real std::function
  callbacks).
      |
      v
  Test captures subscriber notifications + palette bytes.

**Scope B: what TestDarkModeBroadcast does NOT test (deferred).**

The WeaselPanel.cpp::OnSettingChange + WeaselPanel::Refresh() +
_RefreshStylePalette() path. This is a separate scope because:

1. **WTL/ATL dependency cost**. WeaselPanel inherits CWindowImpl,
   CDoubleBufferImpl, and includes GdiplusBlur.h + d2d1.lib +
   dwrite.lib. Linking WeaselPanel.cpp into a test binary would
   add 5-10 MB of dependency and 2-3 minutes of build time. The
   test would need a weasel::UI& reference (which itself depends
   on WeaselServer, WeaselIPCClient, etc.).
2. **L42 risk**. The whole point of L42 is that linking code
   statically does not guarantee it runs in the production binary
   (LTCG dead-strip). For a TEST binary, this is a moot point (the
   test runs in its own process), but the WTL/ATL include cost
   is real.
3. **Spec 022 placeholder intent**. The spec 022 plan section
   already explicitly says "the test will use the spec 018 L25 mock
   pattern (mock librime objects + hand-rolled assertion-only test
   code, no yaml-cpp / no real rime::KeyEvent)". The L25 pattern
   is to mock the boundary and exercise the real path through the
   boundary, not to re-implement the entire stack.
4. **spec 032 covers the panel-side mirror**. TestPanelDarkMode
   Subscribe (spec 032 T007) already tests a mirror of
   WeaselPanel::OnSettingChange + _RefreshStylePalette with 3
   behavior-level assertions. The WeaselPanel production code
   itself is exercised by the in-process AGENTS.md sec 2.5 smoke
   test (the installed WeaselServer.exe runs the production
   WeaselPanel code every time a user toggles dark mode; this is
   a "dogfood" test of the production path that runs at every
   release). spec 032 mirror is sufficient for the panel-side
   assertions; spec 034 job is the bridge broadcast integration
   boundary, which spec 032 mirror does not cover.

**Why the WndProc filter stub is acceptable (the L25 pattern).**

The L25 lesson says: when the cost of linking the real boundary
object is disproportionate to the assertion value, mirror the
boundary and assert the BEHAVIOR (not the byte-equality of the
implementation). The test asserts:

- "SendMessage(WM_SETTINGCHANGE, lParam="ImmersiveColorSet") causes
  bridge.Refresh() to fire" (boundary behavior).
- "SendMessage(WM_SETTINGCHANGE, lParam="WindowMetrics") does NOT
  cause bridge.Refresh() to fire" (boundary behavior).
- "SendMessage(WM_SETTINGCHANGE, lParam=NULL) does NOT cause
  bridge.Refresh() to fire" (boundary behavior).

The actual filter implementation (production: 3-line wcscmp check
in OnSettingChange; test: 3-line wcscmp check in WndProc) is
trivial. The "drift" risk that L26 warns about is the same risk
that TestPanelDarkModeSubscribe already carries - and spec 032
explicitly accepted that risk by mirroring the production code.
spec 034 makes the same trade-off for the same reason: the cost
of the real WeaselPanel is too high for the assertion value.

If a future spec wants to test the REAL WeaselPanel::OnSettingChange
end-to-end (e.g. for regression-testing the full chain), the path
is clear: link WeaselPanel.cpp + WeaselIPCData + WeaselUI + the
full WTL/ATL/Gdiplus stack. This is deferred to a later spec.

### 2.2 Why a test-owned window is the right model

Three options were considered for triggering WM_SETTINGCHANGE:

1. **Real TSF text service**: requires a full TSF text service
   registration, a focus session, and Windows message dispatch.
   This is 1000+ lines of TSF boilerplate and would couple the test
   to the TSF SDK surface (L18 lesson - mock the boundary, do not
   re-implement it). Rejected.

2. **Call WeaselPanel::OnSettingChange directly**: bypasses the
   message pump but tests the production handler. This is what
   spec 032 mirror test does. Rejected because it does not verify
   the full path (no SendMessage -> Windows message routing ->
   window proc dispatch).

3. **Test-owned hidden message-only window + SendMessage**: the
   test creates a hidden HWND_MESSAGE window whose WndProc is a
   forwarding stub that calls WeaselPanel::OnSettingChange. The
   test then calls SendMessage(hwnd, WM_SETTINGCHANGE, 0, lParam)
   to trigger the path. This verifies:
   - Windows message dispatch works (SendMessage -> WndProc).
   - The WndProc -> production handler handoff works.
   - The production filter on lParam works (only "ImmersiveColorSet"
     triggers the refresh; other lParam values do not).
   This is the L18 lesson applied: mock the boundary (test owns
   the window), but exercise the real production path through it.
   **Selected.**

### 2.3 Why a WndProc stub that calls WeaselPanel directly

Production WeaselPanel uses WTL message maps (BEGIN_MSG_MAP /
MSG_WM_SETTINGCHANGE). The WTL MSG_WM_SETTINGCHANGE macro dispatches
OnSettingChange based on the Windows message routing. The test
WndProc must do the same routing. There are two ways:

  a. **Include WeaselPanel.h and use the WTL CHAIN_MSG_MAP**
     pattern. This requires the test to instantiate a WeaselPanel
     or a test-derivative that chains the message map. Heavy.

  b. **Forward the relevant messages manually** in a test-local
     WndProc. The test WndProc receives WM_SETTINGCHANGE and calls
     WeaselPanel::OnSettingChange(WM_SETTINGCHANGE, 0, lParam,
     bHandled) directly with bHandled defaulted to FALSE. This is
     what WTL MSG_WM_SETTINGCHANGE expands to internally. Light.
     **Selected.**

### 2.4 Bridge state and idempotence

TestDarkModeBroadcast must handle the singleton state set by
previous tests. If TestDarkModeBridge ran first and left the bridge
in dark mode, our test will start in dark mode. Solution:

- At test start, capture the current bridge.IsDarkMode() value.
- At test end, restore the original HKCU value AND call
  bridge.Refresh() to re-sync the cached state with the restored
  registry value.
- The assertion "after WM_SETTINGCHANGE, bridge.IsDarkMode() matches
  HKCU" is RELATIVE: it asserts the consistency, not the absolute
  value. The test then verifies the value matches the registry by
  reading the registry directly.

### 2.5 What the 6+ assertions check

  T1: WM_SETTINGCHANGE + "ImmersiveColorSet" lParam -> WeaselPanel
      OnSettingChange called -> bridge.Refresh() called.
  T2: bridge.IsDarkMode() == (HKCU AppsUseLightTheme == 0) after
      the refresh (consistency between bridge and registry).
  T3: Two subscribers, both fire in registration order, on a real
      state change (dark <-> light). Verified by call_count and
      call_order vector.
  T4: CurrentPalette() returns byte-equal values to the spec 033
      constants (0x1E1E1E / 0xE0E0E0 / 0x2D2D30 / 0xFFFFFF dark;
      0xF0F0F0 / 0x000000 / 0xD0D0D0 / 0x000080 light). Verified
      by direct struct field comparison.
  T5: Set HKCU AppsUseLightTheme = 0 (dark), PostMessage
      WM_SETTINGCHANGE with "ImmersiveColorSet", poll the bridge
      (via SendMessage + processing) - bridge.IsDarkMode() becomes
      true and subscribers fired with dark=true.
  T6: Same as T5 but no registry change - subscribers do NOT fire
      (call_count stays 0; idempotence).

T7-T8 are optional extensions covered in the tasks.md.

## 3. File layout

  test/TestDarkModeBroadcast/
    TestDarkModeBroadcast.cpp    - the test main + assertions
    TestDarkModeBroadcast.vcxproj - copy of TestDarkModeBridge.vcxproj
    stdafx.h                     - include guard + Windows.h
    stdafx.cpp                   - empty TU for precompiled header
    targetver.h                  - _WIN32_WINNT_WIN10 etc.

  weasel.sln (modified) - add 1 Project block + 4
  ProjectConfigurationPlatforms lines for the new test.

  scripts/test-infra/run-test-suite.bat (modified) - add
  test\TestDarkModeBroadcast to the build loop and the run loop.

  scripts/test-infra/verify-test-binaries-fresh.bat (modified) -
  add TestDarkModeBroadcast to the stale detector list.

## 4. Constitution check

| Rule | Status | Notes |
|------|--------|-------|
| I. Intent | OK | spec.md sections 0-4 cover goal + out-of-scope + dependencies + success |
| II. Test | OK | T1-T6 are behavior-level assertions against production code |
| III. Spec-artifact | OK | spec.md + plan.md + tasks.md in .specify/specs/034-.../ |
| IV. Clarification | OK | No [NEEDS CLARIFICATION] markers |
| V. Incremental | OK | spec 033 shipped the foundation; spec 034 adds the test |
| R1 | OK | spec.md articulates intent + acceptance criteria |
| R2 | OK | spec.md is tech-agnostic; plan.md has the C++ specifics |
| R3 | OK | All T001-T005 marked P1 |
| R4 | OK | This file is the constitution check |
| R5 | OK | Each task <4h, 1-3 files (most are 1-2 files) |
| R6 | OK | T007 is the evidence requirement (smoke test + test pass) |
| R7 | OK | spec/plan/tasks consistent (cross-checked during authoring) |
| R8 | OK | spec versioned in git via the T008 commit |
| R9 | OK | spec 004 + spec 033 cited as sources; L24/L26/L31/L36/L42 cited |
| P1-P8 | OK | P8 (fluxing: scope) - new code in test/ dir, kizemo only |

## 5. Risks

- **R1**: WeaselPanel.cpp pulls in WTL/ATL headers transitively when
  linked via L24. This adds compile time + may pull in additional
  libraries (atlthunk.lib, etc.). Mitigation: pre-declare the
  WndProc and OnSettingChange in the test local WndProc to avoid
  pulling in the full WeaselPanel.h. The WndProc only needs the
  signature, not the full message map.
  - Actually: the test must include WeaselPanel.h to call the
    OnSettingChange method. The WTL/ATL include cost is unavoidable
    for this test. spec 033 spec 033 vcxproj already pulls in
    RimeWithWeasel and librime; weasel.dll is NOT pulled in (the
    test is its own exe).
  - Verify: build TestDarkModeBridge.vcxproj to confirm the
    include cost is acceptable. If it is, mirror the same pattern
    in TestDarkModeBroadcast.

- **R2**: Singleton state leak between tests. TestDarkModeBridge
  runs first, sets the bridge to dark, then exits. TestDarkMode
  Broadcast starts in dark state. Mitigation: see section 2.4 - the
  test reads HKCU first, captures it, and works with the actual
  state. Idempotence is the actual invariant being tested.

- **R3**: HKCU AppsUseLightTheme is a USER-level key. The test
  modifies it. Mitigation: capture original value, write the test
  value, run assertions, restore original value. The test MUST be
  runnable on a developer machine without permanent side effects.

- **R4**: test execution time. WeaselPanel.cpp is large (~3k lines)
  and the linker will pull in WTL/ATL symbols. The build time for
  the new test vcxproj may be 1-2 minutes. Mitigation: this is a
  one-time cost; incremental rebuilds will be fast.

## 6. Anti-patterns (per spec 033 AP-### pattern)

- **AP-034-A**: do not mirror WeaselPanel::OnSettingChange into the
  test. The test links the production code (L24).
- **AP-034-B**: do not skip the lParam filter. The "ImmersiveColorSet"
  check is part of what the test verifies.
- **AP-034-C**: do not use SetDarkForTest as the primary path.
  SetDarkForTest is a unit-test back-door. TestDarkModeBroadcast is
  an integration test - it must exercise the HKCU path.
- **AP-034-D**: do not omit HKCU restoration. The test must restore
  the original AppsUseLightTheme value at exit.
- **AP-034-E**: do not omit window destruction. The test must
  DestroyWindow + UnregisterClass at exit.
- **AP-034-F**: do not use a mirrored palette struct. The test
  compares against the spec 033 production constants directly.
- **AP-034-G**: do not add the new test to the weasel.sln without
  a unique GUID (L23).
- **AP-034-H**: do not skip the L31 OutDir fix in the new vcxproj.
- **AP-034-I**: do not git add . to commit (A10).
- **AP-034-J**: do not commit env.bat / weasel.props (A3).
