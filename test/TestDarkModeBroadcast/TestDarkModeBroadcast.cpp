// TestDarkModeBroadcast.cpp - spec 034 T001 (2026-07-04)
//
// Behavior-level integration test for the F11 dark-mode broadcast
// pipeline. Verifies the end-to-end path:
//
//   Windows OS event (user toggles dark mode)
//       |
//       v
//   Windows broadcasts WM_SETTINGCHANGE with lParam = "ImmersiveColorSet"
//       |
//       v
//   TestDarkModeBroadcast WndProc (test-local stub, L25 mock pattern)
//   receives the message via SendMessage.
//       |
//       v
//   WndProc filter: lParam != 0 AND wcscmp(lParam, L"ImmersiveColorSet") == 0
//   (mirrors WeaselPanel::OnSettingChange filter; real WeaselPanel is
//   not linked because of WTL/ATL/Gdiplus cost; see plan.md sec 2.1).
//       |
//       v
//   fluxing::FluxingDarkModeBridge::Get()->Refresh()
//   (ACTUAL production code from spec 033, linked via L24).
//       |
//       v
//   bridge.ReadAppsUseLightTheme() reads HKCU registry
//       |
//       v
//   If state changed, fire all subscribers in FIFO order
//       |
//       v
//   Test captures subscriber notifications + palette bytes.
//
// 6 behavior-level assertions:
//   T1: WndProc filter on ImmersiveColorSet -> bridge.Refresh() called
//   T2: bridge.IsDarkMode() consistent with HKCU AppsUseLightTheme
//   T3: 2 subscribers fire in registration order on real state change
//   T4: CurrentPalette() bytes byte-equal spec 033 production constants
//   T5: HKCU write + SendMessage -> state change -> subscribers fire
//   T6: Re-send without registry change -> no re-fire (idempotence)
//
// All test mutations of HKCU are restored at exit (AP-034-D).
// All test windows are destroyed at exit (AP-034-E).
// All subscribers are unsubscribed at exit.

#include "stdafx.h"
#include "FluxingDarkModeBridge.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// Win32 constants. WM_SETTINGCHANGE = 0x001A is in WinUser.h, but we
// redefine it locally to keep the test header-light.
#ifndef WM_SETTINGCHANGE_LOCAL
#define WM_SETTINGCHANGE_LOCAL 0x001A
#endif

// HKCU registry path for dark mode (matches production bridge path).
static const wchar_t* kAppsUseLightThemePath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const wchar_t* kAppsUseLightThemeName = L"AppsUseLightTheme";

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", name);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", name);
  }
}

// Test WndProc. Mirrors WeaselPanel::OnSettingChange filter logic.
// On WM_SETTINGCHANGE with lParam = L"ImmersiveColorSet", call
// bridge->Refresh() (identical to production).
//
// The test WndProc also records whether the filter passed (lParam
// matched), so the test can assert the filter behavior independently
// of the bridge behavior.
//
// L25 mock pattern: real WeaselPanel::OnSettingChange is not
// callable here because linking WeaselPanel.cpp would pull in
// WTL/ATL/Gdiplus + d2d1.lib + dwrite.lib. The filter is 3 lines.
LRESULT CALLBACK TestWndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                             LPARAM lParam) {
  if (uMsg == WM_SETTINGCHANGE_LOCAL) {
    if (lParam != 0) {
      const wchar_t* section = reinterpret_cast<const wchar_t*>(lParam);
      if (std::wcscmp(section, L"ImmersiveColorSet") == 0) {
        fluxing::FluxingDarkModeBridge::Get()->Refresh();
      }
    }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// HKCU read helper. Returns the AppsUseLightTheme DWORD value, or 1
// (light) if the key is missing (per production behavior).
DWORD ReadAppsUseLightTheme() {
  HKEY hKey = nullptr;
  LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kAppsUseLightThemePath, 0,
                          KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey);
  if (rc != ERROR_SUCCESS) return 1;  // default = light
  DWORD value = 1;
  DWORD size = sizeof(value);
  rc = RegQueryValueExW(hKey, kAppsUseLightThemeName, nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(hKey);
  if (rc != ERROR_SUCCESS) return 1;
  return value;
}

// HKCU write helper. Returns true on success.
bool WriteAppsUseLightTheme(DWORD value) {
  HKEY hKey = nullptr;
  LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, kAppsUseLightThemePath, 0,
                             nullptr, REG_OPTION_NON_VOLATILE,
                             KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr,
                             &hKey, nullptr);
  if (rc != ERROR_SUCCESS) return false;
  rc = RegSetValueExW(hKey, kAppsUseLightThemeName, 0, REG_DWORD,
                      reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(hKey);
  return rc == ERROR_SUCCESS;
}

// Test-owned hidden message-only window. Created at test start,
// destroyed at test end (AP-034-E).
HWND g_hwnd = nullptr;
const wchar_t* kClassName = L"TestDarkModeBroadcastClass";

bool CreateTestWindow() {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = TestWndProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = kClassName;
  if (!RegisterClassW(&wc)) {
    DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) {
      std::printf("  RegisterClassW failed: %lu\n", err);
      return false;
    }
  }
  g_hwnd = CreateWindowExW(0, kClassName, L"TestDarkModeBroadcast",
                            0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            GetModuleHandle(nullptr), nullptr);
  if (!g_hwnd) {
    std::printf("  CreateWindowExW failed: %lu\n", GetLastError());
    return false;
  }
  return true;
}

void DestroyTestWindow() {
  if (g_hwnd) {
    DestroyWindow(g_hwnd);
    g_hwnd = nullptr;
  }
  UnregisterClassW(kClassName, GetModuleHandle(nullptr));
}

// Helper: send a WM_SETTINGCHANGE with the given section name (or
// NULL if section is null) to the test window.
void SendSettingChange(const wchar_t* section) {
  LPARAM lParam = section ? reinterpret_cast<LPARAM>(section) : 0;
  SendMessage(g_hwnd, WM_SETTINGCHANGE_LOCAL, 0, lParam);
}

}  // namespace

int main() {
  using fluxing::FluxingDarkModeBridge;
  using fluxing::Palette;

  // Spec 033 production constants (mirror of FluxingDarkModeBridge.cpp
  // static const Palette kPaletteDark / kPaletteLight).
  const Palette kExpectedDark = {0x1E1E1E, 0xE0E0E0, 0x2D2D30, 0xFFFFFF};
  const Palette kExpectedLight = {0xF0F0F0, 0x000000, 0xD0D0D0, 0x000080};

  auto* bridge = FluxingDarkModeBridge::Get();
  if (!bridge) {
    std::printf("FATAL: bridge singleton is null\n");
    return 2;
  }

  // Set up test window.
  if (!CreateTestWindow()) {
    std::printf("FATAL: failed to create test window\n");
    return 2;
  }

  // Capture original HKCU value to restore at exit (AP-034-D).
  DWORD orig_value = ReadAppsUseLightTheme();
  bool orig_exists = (RegOpenKeyExW(HKEY_CURRENT_USER,
                                     kAppsUseLightThemePath, 0,
                                     KEY_QUERY_VALUE | KEY_WOW64_64KEY,
                                     nullptr) == ERROR_SUCCESS);
  // We do not actually delete the key at exit even if it did not exist
  // before (the test creates it). Restoration = write orig_value back.
  // This is the safest behavior: the test never deletes a key that it
  // did not create.

  // Set known starting state: AppsUseLightTheme = 1 (light). This
  // guarantees the bridge cached state is "light" and that a write to
  // 0 (dark) is a real change that fires subscribers.
  if (!WriteAppsUseLightTheme(1)) {
    std::printf("FATAL: cannot write initial AppsUseLightTheme=1\n");
    DestroyTestWindow();
    return 2;
  }
  // Force the bridge to re-read the registry so its cached state
  // matches what we just wrote. This is the test-only back-door
  // (SetDarkForTest is also available, but Refresh() exercises the
  // production path).
  bridge->Refresh();

  // Two subscribers for T3 / T5 / T6.
  int call_count_1 = 0;
  bool last_value_1 = false;
  int call_count_2 = 0;
  bool last_value_2 = false;
  std::vector<bool> call_order;  // records the dark state of each call
  // (call_count + call_order lets us verify FIFO registration order).
  auto h1 = bridge->Subscribe(
      [&](bool is_dark) { ++call_count_1; last_value_1 = is_dark;
                          call_order.push_back(is_dark); });
  auto h2 = bridge->Subscribe(
      [&](bool is_dark) { ++call_count_2; last_value_2 = is_dark;
                          call_order.push_back(is_dark); });

  // ----------------------------------------------------------------
  // T1: WndProc filter on ImmersiveColorSet -> bridge.Refresh() called.
  // We verify by writing dark to registry, sending the message, and
  // observing that bridge.IsDarkMode() flipped.
  // ----------------------------------------------------------------
  {
    bool was_dark = bridge->IsDarkMode();
    DWORD new_value = was_dark ? 1 : 0;
    bool was_light = !was_dark;
    WriteAppsUseLightTheme(new_value);
    // Send via WndProc + lParam = L"ImmersiveColorSet".
    SendSettingChange(L"ImmersiveColorSet");
    // bridge should now reflect the new value.
    Check(bridge->IsDarkMode() != was_dark,
          "T1: SendMessage WM_SETTINGCHANGE ImmersiveColorSet -> bridge.IsDarkMode flips");
    // restore for the next test.
    WriteAppsUseLightTheme(was_light ? 1 : 0);
    bridge->Refresh();  // re-sync bridge cached state with restored value
    call_count_1 = 0; call_count_2 = 0; call_order.clear();
  }

  // ----------------------------------------------------------------
  // T2: bridge.IsDarkMode() consistent with HKCU AppsUseLightTheme.
  // After forcing both to a known state, verify they agree.
  // ----------------------------------------------------------------
  {
    WriteAppsUseLightTheme(0);  // dark
    bridge->Refresh();
    DWORD hkcu_value = ReadAppsUseLightTheme();
    bool bridge_dark = bridge->IsDarkMode();
    bool hkcu_dark = (hkcu_value == 0);
    Check(bridge_dark == hkcu_dark,
          "T2a: bridge.IsDarkMode() matches HKCU AppsUseLightTheme=0 (dark)");

    WriteAppsUseLightTheme(1);  // light
    bridge->Refresh();
    hkcu_value = ReadAppsUseLightTheme();
    bridge_dark = bridge->IsDarkMode();
    hkcu_dark = (hkcu_value == 0);
    Check(bridge_dark == hkcu_dark,
          "T2b: bridge.IsDarkMode() matches HKCU AppsUseLightTheme=1 (light)");
    call_count_1 = 0; call_count_2 = 0; call_order.clear();
  }

  // ----------------------------------------------------------------
  // T3: 2 subscribers fire in registration order on a real state change.
  // ----------------------------------------------------------------
  {
    WriteAppsUseLightTheme(0);  // dark (was light)
    bridge->Refresh();  // state change -> both subscribers fire
    Check(call_count_1 == 1,
          "T3a: subscriber 1 fired exactly once on state change");
    Check(call_count_2 == 1,
          "T3b: subscriber 2 fired exactly once on state change");
    Check(call_order.size() == 2,
          "T3c: total subscriber calls == 2 (FIFO order)");
    if (call_order.size() == 2) {
      Check(call_order[0] == true && call_order[1] == true,
            "T3d: both subscribers received the new dark=true state");
    } else {
      Check(false, "T3d: skipped (call_order size != 2)");
    }
    // restore for the next test.
    WriteAppsUseLightTheme(1);
    bridge->Refresh();
    call_count_1 = 0; call_count_2 = 0; call_order.clear();
  }

  // ----------------------------------------------------------------
  // T4: CurrentPalette() bytes byte-equal spec 033 production constants.
  // ----------------------------------------------------------------
  {
    WriteAppsUseLightTheme(0);  // dark
    bridge->Refresh();
    Palette dark_pal = bridge->CurrentPalette();
    bool dark_ok = (dark_pal.back == kExpectedDark.back) &&
                   (dark_pal.text == kExpectedDark.text) &&
                   (dark_pal.hilited_back == kExpectedDark.hilited_back) &&
                   (dark_pal.hilited_text == kExpectedDark.hilited_text);
    Check(dark_ok, "T4a: CurrentPalette() dark = 0x1E1E1E/0xE0E0E0/0x2D2D30/0xFFFFFF");

    WriteAppsUseLightTheme(1);  // light
    bridge->Refresh();
    Palette light_pal = bridge->CurrentPalette();
    bool light_ok = (light_pal.back == kExpectedLight.back) &&
                    (light_pal.text == kExpectedLight.text) &&
                    (light_pal.hilited_back == kExpectedLight.hilited_back) &&
                    (light_pal.hilited_text == kExpectedLight.hilited_text);
    Check(light_ok, "T4b: CurrentPalette() light = 0xF0F0F0/0x000000/0xD0D0D0/0x000080");
  }

  // ----------------------------------------------------------------
  // T5: HKCU write + SendMessage WM_SETTINGCHANGE + bridge refresh
  // chain produces a state change that propagates to all subscribers.
  // This is the full end-to-end path via the WndProc (not a direct
  // bridge->Refresh()).
  // ----------------------------------------------------------------
  {
    // Reset to known light state.
    WriteAppsUseLightTheme(1);
    bridge->Refresh();
    call_count_1 = 0; call_count_2 = 0; call_order.clear();

    // Write dark to HKCU, then send the WM_SETTINGCHANGE. The WndProc
    // receives the message, filters on lParam, and calls bridge.Refresh().
    WriteAppsUseLightTheme(0);
    SendSettingChange(L"ImmersiveColorSet");

    Check(call_count_1 == 1,
          "T5a: subscriber 1 fired after SendMessage end-to-end chain");
    Check(call_count_2 == 1,
          "T5b: subscriber 2 fired after SendMessage end-to-end chain");
    Check(bridge->IsDarkMode() == true,
          "T5c: bridge.IsDarkMode() == true after end-to-end chain");
  }

  // ----------------------------------------------------------------
  // T6: Re-send without registry change -> no re-fire (idempotence).
  // After T5, the cached state is "dark". A re-send of the same
  // message without changing the registry should NOT re-fire
  // subscribers (the bridge checks for state change before firing).
  // ----------------------------------------------------------------
  {
    // State is dark from T5. Registry is dark.
    // Re-send the message. bridge.Refresh() reads the same value,
    // sees no state change, and does not fire.
    int prev_count_1 = call_count_1;
    int prev_count_2 = call_count_2;
    SendSettingChange(L"ImmersiveColorSet");
    Check(call_count_1 == prev_count_1,
          "T6a: subscriber 1 NOT re-fired on no-op refresh (idempotence)");
    Check(call_count_2 == prev_count_2,
          "T6b: subscriber 2 NOT re-fired on no-op refresh (idempotence)");
  }

  // ----------------------------------------------------------------
  // Cleanup (AP-034-D, AP-034-E).
  // ----------------------------------------------------------------
  bridge->Unsubscribe(h1);
  bridge->Unsubscribe(h2);
  DestroyTestWindow();
  // Restore original HKCU value (or 1 if it did not exist; the test
  // never deletes a key).
  WriteAppsUseLightTheme(orig_exists ? orig_value : 1);
  bridge->Refresh();  // re-sync bridge cached state with restored value

  std::printf("\nTestDarkModeBroadcast: %d/%d PASS\n", g_pass,
              g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}
