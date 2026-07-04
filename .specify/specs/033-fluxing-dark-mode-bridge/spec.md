# 033 - FluxingDarkModeBridge (stage 1 of spec 006 / F11 cross-cut)

> Part of spec 006 (tray quick settings panel). F11 is the cross-cutting
> dark-mode concern per spec 004 section 9. spec 033 ships the foundation
> (`FluxingDarkModeBridge` + `TestDarkModeBridge`) as a stand-alone
> increment. Stages 2-4 (FluxingComponents, FluxingPanelHost, Quick
> Settings window) follow in spec 034+.

## 0. Context

spec 004 section 9 requires that v2 panels (candidate list, tray quick
settings, frequent phrases list, settings UI) all respond to Windows
dark-mode toggle within 200ms. Today the dark-mode detection is **inline
in `WeaselUI/WeaselPanel.cpp::OnSettingChange` (line 1445)** and the
palette is hardcoded in `_RefreshStylePalette` (line 1460). This is the
P1 ship state for the candidate panel, but two problems remain:

1. **Tightly coupled**: any other panel (tray menu, config UI, phrases
   list) would need to duplicate the `WM_SETTINGCHANGE` filter logic
   and the hardcoded palette. This is what spec 004 section 9.6 calls
   the "横切" problem.
2. **Untested**: the existing logic in WeaselPanel cannot be
   behavior-level tested without linking the entire WTL/ATL/Gdiplus
   stack. `TestPanelDarkModeSubscribe` (spec 032) tests a **mirror**
   of the production code, not the production code itself. If the
   production code drifts from the test mirror, tests pass but the
   product breaks.

## 1. Goal

Extract the dark-mode detection + palette + subscriber notification into
a **standalone module** (`RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}`)
that:

- Can be **linked into any panel** (WeaselPanel, FluxingPanelHost, etc.)
  without dragging in WTL/ATL.
- Can be **behavior-level tested** without mocking the WTL stack.
- Exposes a **subscriber pattern** (one bridge, N panels) so future
  panels subscribe to palette changes without re-implementing the
  `WM_SETTINGCHANGE` filter.
- Preserves **byte-for-byte the same palette values** as the current
  WeaselPanel inline implementation (no behavior change in this spec).

## 2. Out of scope (deferred to spec 034+)

- FluxingComponents (Button, List, KeyCap, TextField, Panel, Toggle)
- FluxingPanelHost process (subprocess model, tray quick settings)
- FluxingIPCClient (WeaselServer <-> FluxingPanelHost IPC)
- `Alt+,` global hotkey
- 200ms gradient animation
- New schema-driven palette (current ship palette stays hardcoded)
- Spec 007 (yaml config UI), spec 009 (frequent phrases) - both depend
  on this foundation but ship in later specs

## 3. Dependencies

- WeaselPanel.cpp::OnSettingChange + _RefreshStylePalette (existing)
- spec 032 T007 - TestPanelDarkModeSubscribe (existing test mirror)
- spec 004 section 9 F11 cross-cut spec (defines palette values)
- L40 (PRD/TDD corruption) - documents why this spec's documents
  are in English, not Chinese
- L41 (AGENTS.md smoke test recipe update) - smoke test follows the
  corrected recipe (D: drive path, unquoted /D=)

## 4. Success criteria

### 4.1 Functional (existing behavior preserved)

- The hardcoded dark palette in WeaselPanel.cpp
  (0x1E1E1E / 0xE0E0E0 / 0x2D2D30 / 0xFFFFFF) and light palette
  (0xF0F0F0 / 0x000000 / 0xD0D0D0 / 0x000080) are byte-equal
  after the refactor.
- WM_SETTINGCHANGE with lParam = "ImmersiveColorSet" still triggers
  the palette refresh.
- WM_SETTINGCHANGE with any other lParam value is still a no-op.
- HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize
  AppsUseLightTheme = 0 still selects dark palette.

### 4.2 Structural (new behavior)

- `FluxingDarkModeBridge::IsDarkMode()` reads HKCU
  `AppsUseLightTheme` (DWORD) and returns true if 0, false otherwise.
  Defaults to false (light) on any error.
- `FluxingDarkModeBridge::Subscribe(callback)` returns an opaque
  subscription handle. The callback fires with the new `bool is_dark`
  argument on every palette change.
- `FluxingDarkModeBridge::Unsubscribe(handle)` removes the subscription.
- `FluxingDarkModeBridge::Refresh()` is the public entry point for
  the WM_SETTINGCHANGE handler in any panel. It re-reads
  `IsDarkMode()`, iterates subscribers, fires callbacks, and returns
  the new state.
- `FluxingDarkModeBridge::CurrentPalette()` returns a `Palette` struct
  with the 4 hardcoded color values for the current state.
- WeaselPanel.cpp::OnSettingChange becomes a 3-line wrapper that calls
  `FluxingDarkModeBridge::Get()->Refresh()`.

### 4.3 Test coverage

`TestDarkModeBridge` (new) is a behavior-level test that links
**the actual production code** (not a mirror). It:

- Tests `IsDarkMode()` with a mocked HKCU registry read.
- Tests `Subscribe()` + `Unsubscribe()` ordering (later subscribe
  is called first when iterating, LIFO; or per design choice).
- Tests `Refresh()` fires all subscribers exactly once per call.
- Tests `CurrentPalette()` returns the same bytes as the old inline
  WeaselPanel palette for both dark and light states.

The test project is built via the standard msbuild pattern (per L31
fix) and added to `weasel.sln` + `scripts/test-infra/run-test-suite.bat`.

### 4.4 Smoke test

A release of this spec must pass the AGENTS.md section 2.5 silent-
install smoke test (using the L41-corrected recipe: D: drive path,
unquoted /D=).

## 5. Tasks (incremental)

- [ ] T001 [P1] Create `RimeWithWeasel/FluxingDarkModeBridge.h`:
      namespace `fluxing`, struct `Palette { DWORD back, text,
      hilited_back, hilited_text; }`, class `FluxingDarkModeBridge`
      with `Get()` (singleton), `IsDarkMode()`, `Refresh()`,
      `Subscribe(callback)`, `Unsubscribe(handle)`,
      `CurrentPalette()`. Include guards. UTF-8, no BOM, CRLF.

- [ ] T002 [P1] Create `RimeWithWeasel/FluxingDarkModeBridge.cpp`:
      implement the above. Read HKCU via `RegOpenKeyExW` /
      `RegQueryValueExW` (with the L04-style fallback chain:
      try `AppsUseLightTheme` DWORD first, fall back to light).
      Subscriber list stored as `std::vector<std::pair<Handle,
      Callback>>`. Handle is a monotonically increasing `uint64_t`
      for O(1) Unsubscribe. CRLF, no BOM.

- [ ] T003 [P1] Add `FluxingDarkModeBridge.cpp` to RimeWithWeasel
      xmake target. Edit `RimeWithWeasel/xmake.lua` to add the new
      file to `add_files`. (No new project; this is a new .cpp in
      an existing static library target.)

- [ ] T004 [P1] Refactor `WeaselUI/WeaselPanel.cpp`:
      - Replace `_RefreshStylePalette` body with a call to
        `fluxing::FluxingDarkModeBridge::Get()->CurrentPalette()`
        to get the palette, then assign to `m_style`.
      - Replace `OnSettingChange` body with a 3-line wrapper:
        filter lParam, call `Refresh()`, set `bHandled`.
      - Delete the hardcoded palette constants from WeaselPanel.cpp
        (they now live in FluxingDarkModeBridge.cpp).
      - Update `WeaselUI/WeaselPanel.h` if necessary to add an
        `#include "FluxingDarkModeBridge.h"`.
      - Verify the existing TestPanelDarkModeSubscribe test still
        passes (it tests a mirror, not the production code, so it
        will pass unchanged).

- [ ] T005 [P1] Create `test/TestDarkModeBridge/` with:
      - `TestDarkModeBridge.cpp` (5+ behavior-level assertions)
      - `TestDarkModeBridge.vcxproj` (using the L31-fix template:
        `OutDir=$(SolutionDir)\$(Configuration)\` with backslash)
      - `stdafx.h`, `stdafx.cpp`, `targetver.h` (boilerplate)
      - The test must link the **actual production code** by
        adding `RimeWithWeasel/FluxingDarkModeBridge.cpp` to the
        vcxproj's ClCompile list (per L24 link-probe pattern).

- [ ] T006 [P1] Register the test project:
      - Add `TestDarkModeBridge` Project block to `weasel.sln`
        with the standard 6-platform ProjectConfigurationPlatforms
        (Debug|Win32, Release|Win32 per existing pattern; x86
        only because librime is Win32-only per L10).
      - Add `TestDarkModeBridge` to
        `scripts/test-infra/run-test-suite.bat` build + run loops.
      - Generate a new GUID for the project (use a fresh UUID
        that doesn't conflict with existing ones).

- [ ] T007 [P1] Build + run tests:
      - `cmd /c scripts\test-infra\run-test-suite.bat` must report
        `=== ALL TESTS PASSED ===` with 12/12 test projects
        (was 11/11; adds TestDarkModeBridge).
      - `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat`
        must report `=== ALL TEST BINARIES FRESH ===` with 12
        FRESH entries.
      - TestDarkModeBridge must report at least 5/5 PASS.

- [ ] T008 [P1] AGENTS.md section 2.5 smoke test on a fresh
      `release\fluxing-0.18.20.0-installer.exe` (or whatever the
      next version is): all 8 invariants pass using the L41
      recipe (D:\FluxingTest\0.18.20\pf, unquoted /D=).

- [ ] T009 [P1] Commit + tag:
      - Bump env.bat + weasel.props to 0.18.20.0 (local-only).
      - CHANGELOG.md entry for 0.18.20.0 (transliterated per spec
        032 template, ASCII-only per L01).
      - `feat(fluxing): spec 033 - FluxingDarkModeBridge (F11 cross
        cut foundation)`.
      - `chore(release): 0.18.20.0 - spec 033 + L40/L41 stable`.
      - Tag v0.18.20.0 lightweight, push to kizemo/Fluxing.

## 6. Anti-patterns to avoid (per L36 audit pattern)

- AP-033-A: copy-paste the WeaselPanel inline palette constants
  into the test; the test must link the **actual** FluxingDarkMode
  Bridge.cpp, not a mirror (L24 / L26).
- AP-033-B: use `RegOpenKeyExW` without `KEY_WOW64_64KEY`; on
  x64 Windows the 32-bit installer process sees a 32-bit registry
  view which may not have `AppsUseLightTheme`. (Check how
  WeaselPanel's existing code reads it.)
- AP-033-C: subscribe inside a callback - reentrancy bug; iteration
  of the subscriber list must be on a stable snapshot.
- AP-033-D: use `static` for the singleton; thread-safety matters
  because WM_SETTINGCHANGE can fire on any thread. Use a function-
  local `static` initialized via `std::call_once` (C++17).
- AP-033-E: omit the `OutDir` backslash fix in the new vcxproj;
  L31 was a multi-spec recurring bug, so audit at T005.

## 7. Constitution check

- I. Intent: spec/plan/tasks all present (this file + 033-plan.md
  + 033-tasks.md).
- II. Test: T005-T007 ensure test-backed change (production code
  linked into test, not mirrored).
- III. Spec-artifact: 3 files in `.specify/specs/033-.../`.
- IV. Clarification: no [NEEDS CLARIFICATION] markers.
- V. Incremental: this spec is a stand-alone shippable increment;
  stages 2-4 are explicit follow-on specs (034+).
- R1-R9: all 9 hard rules addressed (R1 = this file; R2 = spec
  has no language/framework names beyond C++; R3 = single P1
  priority; R4 = constitution check below; R5 = 9 tasks all
  <4h, 1-3 files each; R6 = smoke test evidence required
  pre-commit; R7 = spec/plan/tasks consistent; R8 = spec
  versioned in git; R9 = spec 004 / spec 032 cited as sources).
- P1-P8: P8 (brand-fork scope) - new files in `fluxing` namespace,
  push to kizemo only, never to origin.

## 8. Cross-references

- spec 004 section 9 (F11 dark mode cross-cut)
- spec 006 (the larger tray-quick-settings parent spec)
- spec 032 T007 (TestPanelDarkModeSubscribe, the existing mirror
  test that this spec's TestDarkModeBridge will eventually
  supplement / replace)
- L01, L02, L37, L40 (Chinese / byte-level discipline)
- L04, L24 (HKCU / librime link-probe patterns)
- L10 (librime is Win32-only; x64 builds will not be exercised)
- L31, L36 (vcxproj OutDir backslash, fix-coverage audit)
- L41 (AGENTS.md smoke test recipe)

## 9. Source-of-truth for the v0.18.20.0 version

- L40 documents that `.specify/PRD.md` and `.specify/TDD.md` are
  corrupted (literal '?' characters) and **unrecoverable from git
  history**. The authoritative project intent is in spec 004 +
  constitution + sub-specs 005-032 + AGENTS.md. spec 033 cites
  spec 004 section 9 as the F11 source of truth.
- spec 033-tasks.md has the atomic step list.
- spec 033-plan.md has the technical approach (this file is the
  spec, not the plan; the plan is a separate file).
