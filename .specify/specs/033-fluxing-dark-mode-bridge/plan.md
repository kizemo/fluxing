# 033 - Plan - FluxingDarkModeBridge extraction

## 1. Technical context

- **C++17, MSVC v143, /utf-8, /MT** (matches existing RimeWithWeasel
  static library target).
- **RimeWithWeasel/FluxingDarkModeBridge.{h,cpp}** - new files in
  the existing `RimeWithWeasel` static library target (no new vcxproj
  for the production code; the new `test/TestDarkModeBridge` vcxproj
  is a separate test project that links FluxingDarkModeBridge.cpp
  via direct ClCompile entry, per the L24 link-probe pattern).
- **Subsystem**: WINDOWS (no console window).
- **No new dependencies** (no ATL, no WTL, no Boost - the bridge
  only uses Win32 RegOpenKeyExW / RegQueryValueExW + stdlib).
- **No changes to installer.nsi** for this spec (the bridge is a
  product binary, not an installer-level change; L13 / L17 do not
  apply).

## 2. Architecture

### 2.1 Singleton with std::call_once

The singleton is a function-local static initialized via
`std::call_once`. C++17 guarantees thread-safe initialization of
function-local statics, so no explicit mutex is needed at startup.
The subscriber list is guarded by a `std::mutex` because
WM_SETTINGCHANGE can fire on any thread:

```cpp
class FluxingDarkModeBridge {
 public:
  static FluxingDarkModeBridge* Get();
  bool IsDarkMode() const;
  bool Refresh();   // re-reads registry, fires callbacks, returns new state
  uint64_t Subscribe(Callback cb);
  void Unsubscribe(uint64_t handle);
  Palette CurrentPalette() const;
  // Internal: for test injection
  void SetDarkForTest(bool dark);  // undefined in production builds
 private:
  FluxingDarkModeBridge() = default;
  bool ReadAppsUseLightTheme() const;  // HKCU + fallback
  mutable std::mutex mu_;
  std::vector<std::pair<uint64_t, Callback>> subscribers_;
  uint64_t next_handle_ = 1;
  bool current_dark_ = false;
};
```

The `SetDarkForTest` function is a back-door for behavior-level
testing without HKCU registry mocking (L04-style fallback). It is
defined unconditionally but only called from `test/TestDarkModeBridge`.
Production code never calls it; it is documented as test-only.

### 2.2 Registry read (L04-style fallback)

```cpp
bool FluxingDarkModeBridge::ReadAppsUseLightTheme() const {
  HKEY hKey = nullptr;
  LONG rc = RegOpenKeyExW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey);
  if (rc != ERROR_SUCCESS) return true;  // default light
  DWORD value = 1;  // default = light
  DWORD size = sizeof(value);
  rc = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(hKey);
  if (rc != ERROR_SUCCESS) return true;
  return value == 0;  // 0 = dark
}
```

The `KEY_WOW64_64KEY` flag is critical: a 32-bit process on x64
Windows otherwise sees the 32-bit registry view, which may not
have `AppsUseLightTheme` (Windows writes it to the 64-bit view).

### 2.3 Palette (byte-equal to existing WeaselPanel inline values)

```cpp
struct Palette {
  DWORD back;
  DWORD text;
  DWORD hilited_back;
  DWORD hilited_text;
};
static const Palette kPaletteDark    = {0x1E1E1E, 0xE0E0E0, 0x2D2D30, 0xFFFFFF};
static const Palette kPaletteLight   = {0xF0F0F0, 0x000000, 0xD0D0D0, 0x000080};
```

These are the **exact same bytes** as the existing WeaselPanel.cpp
inline values (verified by visual diff during this spec authoring).

### 2.4 WeaselPanel refactor

`WeaselUI/WeaselPanel.cpp` changes:

- Line 1445-1457: `OnSettingChange` becomes
  ```cpp
  LRESULT WeaselPanel::OnSettingChange(UINT uMsg, WPARAM wParam,
                                       LPARAM lParam, BOOL& bHandled) {
    if (lParam != 0) {
      const wchar_t* section = (const wchar_t*)lParam;
      if (wcscmp(section, L"ImmersiveColorSet") == 0) {
        fluxing::FluxingDarkModeBridge::Get()->Refresh();
        Refresh();
      }
    }
    bHandled = false;
    return 0;
  }
  ```

- Line 1460-1480: `_RefreshStylePalette` becomes
  ```cpp
  void WeaselPanel::_RefreshStylePalette() {
    auto palette = fluxing::FluxingDarkModeBridge::Get()->CurrentPalette();
    m_style.back_color = palette.back;
    m_style.text_color = palette.text;
    m_style.hilited_candidate_back_color = palette.hilited_back;
    m_style.hilited_candidate_text_color = palette.hilited_text;
    _CreateLayout();
  }
  ```

- `IsUserDarkMode` is no longer used by WeaselPanel; keep it
  defined for now in case other callers exist (grep before
  deleting).

## 3. Constitution check

| Rule | Status | Notes |
|------|--------|-------|
| I. Intent | OK | spec.md sections 1-4 cover goal + out-of-scope + dependencies + success |
| II. Test | OK | T005-T007: TestDarkModeBridge links production code, 5+ assertions |
| III. Spec-artifact | OK | spec.md + plan.md + tasks.md in `.specify/specs/033-.../` |
| IV. Clarification | OK | No [NEEDS CLARIFICATION] markers |
| V. Incremental | OK | This spec ships the F11 foundation only; spec 034+ adds the UI |
| R1 | OK | spec.md articulates intent + acceptance |
| R2 | OK | spec.md is tech-agnostic; plan.md has the C++ specifics |
| R3 | OK | All T001-T009 marked P1 |
| R4 | OK | This file is the constitution check |
| R5 | OK | Each task <4h, 1-3 files |
| R6 | OK | T008 is the evidence requirement (smoke test) |
| R7 | OK | spec/plan/tasks consistent (cross-checked during authoring) |
| R8 | OK | spec versioned in git via the T009 commit |
| R9 | OK | spec 004 section 9 cited as source; L10 cited for Win32-only |
| P1-P8 | OK | P8 (fluxing: scope) - new code in fluxing namespace, kizemo only |

## 4. Risks

- **R1**: HKCU read from 32-bit process on x64 Windows without
  `KEY_WOW64_64KEY` returns default light. Mitigation: use
  `KEY_WOW64_64KEY` explicitly (the production binary is x86 but
  may run on x64 OS via WOW64).
- **R2**: Thread-safety: WM_SETTINGCHANGE can fire on any thread.
  Mitigation: subscriber list guarded by `std::mutex`; iteration
  on a stable snapshot (copy under lock, iterate without lock).
- **R3**: TestDarkModeBridge linking the production code is new
  pattern (previous tests used mirrors per L24). If it causes
  build break (Boost dep chain, RimeWithWeasel missing symbols),
  fall back to a thin shim. Mitigation: TestDarkModeBridge only
  uses stdlib + Win32; no RimeWithWeasel symbol dependencies
  outside FluxingDarkModeBridge.cpp.
- **R4**: Singleton lifetime: function-local static lifetime is
  process-lifetime, so subscribers added in DLL_PROCESS_ATTACH
  and removed in DLL_PROCESS_DETACH must work across the bridge.
  The spec is for an EXE (FluxingPanelHost in 034+) and the
  WeaselPanel (loaded by TSF). For the EXE, lifetime is fine.
  For WeaselPanel, the panel is destroyed on TSF uninit, so
  subscribers added in panel ctor and removed in panel dtor
  are bracketed correctly.
- **R5**: CharSet: production code is `UNICODE` + `_UNICODE`
  defined globally in xmake.lua. `RegOpenKeyExW` uses wide
  strings; `L"..."` literals are 16-bit. No Ansi/Unicode mixing.

## 5. Cross-references

- spec 004 section 9 (F11 source of truth for palette values)
- spec 006 (parent spec for tray quick settings)
- spec 032 T007 (TestPanelDarkModeSubscribe, the mirror test)
- L04 (librime / Win32 fallback pattern)
- L10 (Win32-only librime constraint)
- L24 (link-probe pattern for behavior-level tests)
- L31, L36 (vcxproj OutDir backslash; fix-coverage audit)
- L40 (PRD/TDD corruption; spec is in English because PRD/TDD
  are broken)
- L41 (AGENTS.md smoke test recipe)
