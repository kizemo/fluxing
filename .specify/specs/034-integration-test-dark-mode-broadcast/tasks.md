# 034 - Tasks - TestDarkModeBroadcast (integration test for F11 cross-cut broadcast)

> Atomic task list. Each T### is 1-3 files and <4h. Run in order
> T001 -> T008. T001-T003 are independent of T004-T005 (vcxproj +
> test source can be authored in any order; T006 requires both to be
> done).

## Phase 1 - test source + boilerplate (T001-T002)

- [ ] T001 [P1] Create `test\TestDarkModeBroadcast\TestDarkModeBroadcast.cpp`
      with 6+ behavior-level assertions (per plan.md section 2.5):
      T1: SendMessage(WM_SETTINGCHANGE, "ImmersiveColorSet") reaches
          the test WndProc and triggers bridge->Refresh() (verify by
          subscriber firing + the WndProc filter calling bridge.Refresh()).
      T2: After the SendMessage, bridge.IsDarkMode() is consistent
          with the HKCU AppsUseLightTheme value (0 = dark, non-0 =
          light).
      T3: Two subscribers, both fire in registration order, on a real
          state change. Verify by call_count + call_order vector.
      T4: CurrentPalette() bytes byte-equal the spec 033 production
          constants for both dark and light palettes.
      T5: HKCU write + SendMessage(WM_SETTINGCHANGE) + bridge
          refresh chain produces a state change that propagates to
          all subscribers.
      T6: Re-running the chain without a registry change does NOT
          re-fire subscribers (idempotence).
      The WndProc filter is a test-local stub (L25 mock pattern) that
      mirrors WeaselPanel::OnSettingChange filter logic (3 lines of
      wcscmp). Real WeaselPanel linking is deferred (see plan.md
      section 2.1 Scope B).
      UTF-8, no BOM, CRLF. Verify: byte-level, no C0/C1 overlong,
      no 0x3F runs (the file should not have any `?` substitution).
      Use Windows.h + minimal WTL/ATL includes for HWND_MESSAGE
      CreateWindowEx. The L24 link-probe entry is for FluxingDark
      ModeBridge.cpp ONLY (not WeaselPanel.cpp).

- [ ] T002 [P1] Create boilerplate files in
      `test\TestDarkModeBroadcast\`:
      - `stdafx.h` (include guard + Windows.h + `fluxing\Fluxing
        DarkModeBridge.h`; nothing more; do NOT include any
        WeaselUI header).
      - `stdafx.cpp` (empty TU, just `#include "stdafx.h"`).
      - `targetver.h` (`#define _WIN32_WINNT _WIN32_WINNT_WIN10` etc.,
        matching the spec 033 test boilerplate).
      UTF-8, no BOM, CRLF. Verify: byte-level clean.

## Phase 2 - vcxproj (T003)

- [ ] T003 [P1] Create `test\TestDarkModeBroadcast\TestDarkModeBroadcast.vcxproj`
      as a copy of `test\TestDarkModeBridge\TestDarkModeBridge.vcxproj`
      with the following changes:
      - ProjectGuid: NEW GUID generated via `[guid]::NewGuid()` in
        PowerShell (L23). Use `{B220A318-9188-459D-ABC0-C571C21E5829}`
        (captured 2026-07-04, format uppercase).
      - RootNamespace: `TestDarkModeBroadcast`.
      - File includes:
        `<ClCompile Include="TestDarkModeBroadcast.cpp" />`
        `<ClCompile Include="..\..\RimeWithWeasel\FluxingDarkModeBridge.cpp" />`
        `<ClCompile Include="stdafx.cpp" />`
        (L24 link-probe pattern: link the production FluxingDark
        ModeBridge.cpp directly; do NOT link WeaselPanel.cpp - the
        WndProc filter is a test-local L25 mock; do NOT mirror any
        production content into the test).
      - AdditionalIncludeDirectories: keep `$(SolutionDir)\RimeWithWeasel`
        (already in the spec 033 template). Do NOT add WeaselUI; the
        test does not include any WeaselUI headers.
      Verify: byte-level clean; both Debug AND Release OutDir have
      the L31 fix (`$(SolutionDir)\$(Configuration)\`); both
      AdditionalIncludeDirectories include `$(SolutionDir)\RimeWithWeasel`
      (L36 sibling-fix coverage).

## Phase 3 - sln + scripts registration (T004-T005)

- [ ] T004 [P1] Add `TestDarkModeBroadcast` to `weasel.sln`:
      - Insert the new Project block AFTER the TestDarkModeBridge
        block (line 47), BEFORE the TestTrayRestoreIgnored block
        (line 49). Use the ProjectGuid from T003.
      - Insert 4 ProjectConfigurationPlatforms lines AFTER line 203
        (TestDarkModeBridge Release|Win32.Build.0), BEFORE line 204
        (TestTrayRestoreIgnored.Debug|Win32.ActiveCfg).
      Verify: byte-level diff against the weasel.sln shows exactly
      5 new lines (1 Project block + EndProject + 4 config lines)
      and the rest of the file is unchanged.

- [ ] T005 [P1] Add `TestDarkModeBroadcast` to the test-infra scripts:
      - `scripts\test-infra\run-test-suite.bat`: add to the build
        loop and the run loop (12 -> 13 entries).
      - `scripts\test-infra\verify-test-binaries-fresh.bat`: add to
        the stale detector list (12 -> 13 entries).
      Verify: byte-level diff shows exactly 2 new entries (one per
      script).

## Phase 4 - verify (T006-T007)

- [ ] T006 [P1] Build + run all tests:
      - `cmd /c scripts\test-infra\run-test-suite.bat` -> RC 0,
        `=== ALL TESTS PASSED ===`, 13/13 test projects.
      - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`
        -> RC 0, 13 FRESH.
      - `Release\TestDarkModeBroadcast.exe` reports 6+ assertions PASS.
      - Other 12 tests still pass (no regression).
      Capture stdout + stderr to `C:\TEMP\test-034-*.log`.

- [ ] T007 [P1] Smoke test on the new 0.18.23.0 installer:
      - Build via `xbuild.bat weasel installer` (xmake path).
      - Run AGENTS.md sec 2.5 recipe with L41 corrections (D: drive
        path, unquoted /D=).
      - All 8 invariants pass.
      - L42 verification (NO-OP for spec 034 because no production
        code is added; weasel.dll should still contain 0x001E1E1E
        palette bytes from spec 033; verify count > 0 to confirm
        no regression in the linked binary).
      - Cleanup via uninstall.exe /S.

## Phase 5 - commit + release (T008)

- [ ] T008 [P1] Commit + tag + push:
      - Bump env.bat + weasel.props to 0.18.23.0 (local-only, not
        committed per A3).
      - Add CHANGELOG.md entry for 0.18.23.0 (prepended; byte-level
        splice at the right position).
      - Commit 1: `test(fluxing): spec 034 - TestDarkModeBroadcast
        (F11 cross-cut integration test, unlocks spec 022)`.
      - Commit 2: `chore(release): 0.18.23.0 - spec 034 TestDarkMode
        Broadcast + spec 022 unblocked`.
      - Tag `v0.18.23.0` lightweight, push to kizemo/Fluxing.

## Anti-patterns to avoid (per plan.md section 6)

- [ ] AP-034-A: do not link WeaselPanel.cpp into the test (WTL/ATL/
      Gdiplus dependency cost is too high; L25 mock pattern is the
      boundary approach).
- [ ] AP-034-B: do not skip the lParam filter in the WndProc.
- [ ] AP-034-C: do not use SetDarkForTest as the primary path.
      Use HKCU.
- [ ] AP-034-D: do not omit HKCU restoration. Restore at exit.
- [ ] AP-034-E: do not omit window destruction. DestroyWindow +
      UnregisterClass at exit.
- [ ] AP-034-F: do not use a mirrored palette struct. Compare to spec
      033 production constants directly.
- [ ] AP-034-G: do not add the new test to the weasel.sln without a
      unique GUID (L23).
- [ ] AP-034-H: do not skip the L31 OutDir fix in the new vcxproj.
- [ ] AP-034-I: do not `git add .`. Stage by path per A10.
- [ ] AP-034-J: do not commit env.bat / weasel.props per A3.
