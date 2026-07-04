# 034 - TestDarkModeBroadcast (integration test for F11 cross-cut broadcast)

> Scope: TDD.md sec 3.1 integration test for spec 004 F11 (dark-mode
> cross-cut). Verifies the end-to-end pipeline that propagates a
> Windows dark-mode toggle event to all subscribers of the
> FluxingDarkModeBridge.

## 0. Context

spec 004 section 9 F11 requires that all v2 panels (candidate list,
tray quick settings, frequent phrases list, settings UI) respond to
the Windows dark-mode toggle within 200ms. The implementation was
decomposed across two specs:

- **spec 032** (panel side): refactored WeaselPanel.cpp to subscribe
  via the OnSettingChange WM_SETTINGCHANGE handler + _RefreshStylePalette
  (now uses FluxingDarkModeBridge::Get()->CurrentPalette()).
- **spec 033** (production foundation): extracted the dark-mode
  detection + palette + subscriber notification into a standalone
  module, RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}, with its own
  test (TestDarkModeBridge, 18 behavior-level assertions, L24
  link-probe pattern).

**The remaining gap**: spec 033 TestDarkModeBridge tests the bridge
in isolation. It uses a test-only back-door (SetDarkForTest) to inject
the dark state without touching the registry, and exercises Refresh()
in the same process. It does NOT verify:

1. The full Windows message path: WM_SETTINGCHANGE dispatched via
   PostMessage -> lParam = "ImmersiveColorSet" filter ->
   bridge.Refresh().
2. Multiple subscribers all firing in one refresh (the subscription
   pattern that spec 004 F11 relies on for multi-panel consistency).
3. The byte-equal palette values from the bridge match the values
   expected by the (refactored) WeaselPanel._RefreshStylePalette
   consumer.
4. The HKCU registry path: writing HKCU AppsUseLightTheme = 0 and
   then calling bridge.Refresh() really does flip the state and
   notify subscribers (this is the OS event path that production
   uses, distinct from the SetDarkForTest back-door).
5. Idempotence: re-running Refresh() with the same registry value
   does NOT re-fire subscribers (no event spam, no flicker).

Spec 022 (created in spec 019 batch as a placeholder) was BLOCKED
on spec 004 production code. That block is now lifted by spec 033.
This spec (034) is the implementation that unblocks spec 022.

## 1. Goal

Add a new behavior-level test, test/TestDarkModeBroadcast/, that
exercises the bridge broadcast integration boundary end-to-end with
6+ assertions. The test links the ACTUAL production code:

- RimeWithWeasel/FluxingDarkModeBridge.cpp (L24 link-probe pattern
  via direct ClCompile entry in the vcxproj). This gives the test
  the real bridge, the real std::mutex-guarded subscriber list, the
  real HKCU reader, and the real callback dispatch.
- A test-owned WndProc that filters WM_SETTINGCHANGE lParam values
  (the L25 mock pattern: the filter logic is trivial - 3 lines -
  and instantiating the real WeaselPanel would require WTL/ATL/
  Gdiplus/d2d1.lib/dwrite.lib, a valid weasel::UI& reference, and
  a valid m_status + m_ctx state). The test asserts the BEHAVIOR
  of the filter (ImmersiveColorSet triggers refresh; other lParam
  values do not; NULL does not) and exercises the REAL path through
  the bridge from that point forward.

The test must:

- Link the actual FluxingDarkModeBridge.cpp via direct ClCompile
  entry (L24 link-probe pattern, fixing the L26 mirror-drift class
  of bugs at the bridge layer).
- Use a test-owned hidden window (CreateWindowEx with HWND_MESSAGE
  parent) for the WM_SETTINGCHANGE message pump.
- Write + restore the HKCU AppsUseLightTheme value via the Win32
  RegSetValueExW API.
- Register 2-3 subscribers (lambdas) to the bridge, then trigger
  WM_SETTINGCHANGE via SendMessage to the test window, then assert
  that:
  - All subscribers fired.
  - The palette bytes match the spec 033 production constants
    (0x1E1E1E / 0xE0E0E0 / 0x2D2D30 / 0xFFFFFF dark;
     0xF0F0F0 / 0x000000 / 0xD0D0D0 / 0x000080 light).
  - The cached IsDarkMode() reflects the new state.
  - Re-triggering WM_SETTINGCHANGE without a registry change does
    NOT re-fire subscribers (idempotence).

For the broader test surface (WeaselPanel::OnSettingChange + Refresh
+ _RefreshStylePalette with the real WTL/ATL stack), see spec 032
TestPanelDarkModeSubscribe (3 mirror assertions) and AGENTS.md
sec 2.5 smoke test (real WeaselServer runs the real WeaselPanel on
every dark-mode toggle in production - this is a continuous
dogfood test of the panel-side production path).

## 2. Out of scope (deferred)

- Cross-process broadcast (WeaselServer -> WeaselSetup tray): the
  production WM_SETTINGCHANGE path is in-process; tray menu rebuild
  is handled by WeaselSetup own OnSettingChange + Refresh.
- The 200ms gradient animation: that is a UX-side spec, not a
  bridge-side contract. The bridge fires callbacks immediately on
  Refresh(); the consumer (WeaselPanel) is responsible for any
  animation timing. (See spec 006 / F11 for the animation spec.)
- The FluxingPanelHost subprocess model: the tray quick settings
  panel in spec 006 will use a separate process and IPC, which is
  a different broadcast model. Spec 034 covers only the in-process
  broadcast that the current production binary uses.
- Schema-driven palette: spec 033 hardcoded the palette values.
  The "themed palette from rime_ice.schema" is a separate spec.
- Real TSF integration: TestDarkModeBroadcast uses a test-owned
  hidden window (CreateWindowEx with HWND_MESSAGE) for the message
  pump. It does NOT spin up a real TSF text service.

## 3. Dependencies

- RimeWithWeasel/FluxingDarkModeBridge.{h,cpp} (spec 033 production
  code, shipped in 0.18.22.0).
- WeaselUI/WeaselPanel.cpp (spec 032 refactor; production code
  already calls bridge.Refresh() in OnSettingChange and
  bridge.CurrentPalette() in _RefreshStylePalette).
- test/TestDarkModeBridge/ (spec 033 unit test - kept as-is; spec
  034 is a different test with a different focus).
- L24 (link-probe pattern) - the cure for the L26 mirror-drift bug.
- L26 (test mirror drifts from production over time) - the
  anti-pattern this spec explicitly avoids.
- L31 (vcxproj OutDir trailing backslash) - applied to the new
  TestDarkModeBroadcast vcxproj.
- L36 (sibling-fix coverage audit) - the vcxproj template is
  copy-pasted from TestDarkModeBridge, so we apply the same fix to
  both Debug AND Release OutDir pairs.
- L42 (false-positive test pass when code is dead-stripped from
  production) - N/A for spec 034 because this spec adds NO new
  production code; the test only links existing production code
  into a new test binary.
- L43 (/LTCG:OFF per-target cure) - N/A for the test binary; the
  test is a static exe (ConfigurationType=Application), not a
  shared library, and weasel.dll is not built by this test.

## 4. Success criteria

### 4.1 Functional

- TestDarkModeBroadcast reports 6+ behavior-level assertions PASS
  (T1-T6 below, T7-T8 are optional but recommended).
- All assertions run against the actual production code
  (WeaselPanel.cpp + FluxingDarkModeBridge.cpp) via the L24
  link-probe pattern.
- The test cleans up after itself: restores the original
  HKCU AppsUseLightTheme value, destroys the test window,
  unsubscribes all subscribers.

### 4.2 Test list (T1-T8)

- T1: WM_SETTINGCHANGE with lParam = "ImmersiveColorSet" reaches
  WeaselPanel::OnSettingChange and triggers bridge.Refresh() +
  WeaselPanel::Refresh().
- T2: After OnSettingChange, bridge.IsDarkMode() returns the value
  matching the current HKCU AppsUseLightTheme (0 = dark, non-0 =
  light).
- T3: Multiple subscribers all fire in registration order when
  the state changes (verify by counting calls + recording the
  order).
- T4: Palette bytes from bridge.CurrentPalette() byte-equal the
  pre-033 WeaselPanel inline values (both dark and light).
- T5: HKCU write + WM_SETTINGCHANGE + bridge.Refresh() chain
  produces a state change that propagates to all subscribers.
- T6: Re-running the chain without a registry change does NOT
  re-fire subscribers (idempotence - no event spam).
- T7 (optional): bridge.Unsubscribe(handle) stops the subscriber
  from receiving subsequent refresh notifications.
- T8 (optional): subscriber that calls Unsubscribe from inside its
  callback does not corrupt the iteration (reentrancy safety).

### 4.3 Verification

- cmd /c scripts\test-infra\run-test-suite.bat -> RC 0,
  === ALL TESTS PASSED ===, 13/13 test projects.
- cmd /c scripts\test-infra\verify-test-binaries-fresh.bat ->
  RC 0, 13 FRESH.
- The new TestDarkModeBroadcast.exe lands in
  F:\soft\00selfmade\rime\Release\TestDarkModeBroadcast.exe
  (L31 fix: OutDir with trailing backslash).
- AGENTS.md sec 2.5 smoke test against the new 0.18.23.0
  installer -> all 8 invariants pass (no behavior regression
  in the shipped binary; spec 034 only adds a test, not new
  product code, so the smoke test should be identical to 0.18.22.0).

## 5. References

- spec 004 (F11 cross-cut spec, sec 9)
- spec 019 (created spec 022 placeholder, original commit 8123d59)
- spec 022 (placeholder now unblocked; tracked-but-not-modified)
- spec 032 (TestPanelDarkModeSubscribe, the panel-side mirror test)
- spec 033 (FluxingDarkModeBridge production code +
  TestDarkModeBridge unit test, shipped in 0.18.22.0)
- L24 (link-probe pattern - the test links production code
  directly)
- L26 (mirror drift - the bug class this spec avoids)
- L31 (vcxproj OutDir trailing backslash)
- L36 (sibling-fix coverage)
- L42 (false-positive test pass - not applicable to this spec)
