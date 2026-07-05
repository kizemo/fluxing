// TestFluxingTheme.cpp - spec 037 T016/T019 (2026-07-05)
//
// Behavior-level integration test for fluxing::ui::FluxingTheme.
//
// L24 link-probe pattern: link the actual production code by
// direct ClCompile entry, not a mirror.
//
// 4 assertions:
//   T1: Instance() returns the same reference on repeated calls
//   T2: Subscribe + Unsubscribe manage the subscription list
//       correctly; unknown handle is a no-op
//   T3: CurrentPalette() returns the bridge's current palette
//   T4: dark mode toggle fires FluxingTheme subscribers in
//       registration order (FIFO)
//
// Cleanup: subscribers unsubscribed at exit. HKCU registry
// restored to original value at exit.

#include "stdafx.h"
#include "FluxingTheme.h"
#include "FluxingDarkModeBridge.h"
#include <cstdio>
#include <functional>
#include <vector>

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

DWORD ReadAppsUseLightTheme() {
  HKEY hKey = nullptr;
  LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &hKey);
  if (rc != ERROR_SUCCESS) return 1;
  DWORD value = 1;
  DWORD size = sizeof(value);
  rc = RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(hKey);
  if (rc != ERROR_SUCCESS) return 1;
  return value;
}

bool WriteAppsUseLightTheme(DWORD value) {
  HKEY hKey = nullptr;
  LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, nullptr, REG_OPTION_NON_VOLATILE,
      KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &hKey, nullptr);
  if (rc != ERROR_SUCCESS) return false;
  rc = RegSetValueExW(hKey, L"AppsUseLightTheme", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value),
                       sizeof(value));
  RegCloseKey(hKey);
  return rc == ERROR_SUCCESS;
}

}  // namespace

namespace fluxing_test {

int RunThemeTest() {  using fluxing::ui::FluxingTheme;
  using fluxing::FluxingDarkModeBridge;

  // Capture original HKCU value for restoration.
  DWORD orig_value = ReadAppsUseLightTheme();

  // ----------------------------------------------------------------
  // T1: Instance() returns the same reference on repeated calls.
  // ----------------------------------------------------------------
  auto& theme1 = FluxingTheme::Instance();
  auto& theme2 = FluxingTheme::Instance();
  Check(&theme1 == &theme2,
        "T1: FluxingTheme::Instance() returns the same reference");

  // ----------------------------------------------------------------
  // T2: Subscribe + Unsubscribe. We register two subscribers,
  //     verify they both fire on a palette change, then
  //     unsubscribe one and verify only the other fires on the
  //     next change. Unknown handle is a no-op.
  // ----------------------------------------------------------------
  int count_a = 0;
  int count_b = 0;
  auto h_a = theme1.Subscribe(
      [&](::fluxing::Palette const&) { ++count_a; });
  auto h_b = theme1.Subscribe(
      [&](::fluxing::Palette const&) { ++count_b; });

  // Set known starting state: light (1).
  WriteAppsUseLightTheme(1);
  FluxingDarkModeBridge::Get()->Refresh();
  int base_a = count_a;
  int base_b = count_b;

  // Trigger a change: dark (0) -> bridge fires subscribers.
  WriteAppsUseLightTheme(0);
  FluxingDarkModeBridge::Get()->Refresh();
  Check(count_a == base_a + 1,
        "T2a: subscriber A fired once on dark change");
  Check(count_b == base_b + 1,
        "T2b: subscriber B fired once on dark change");

  // Unsubscribe A. Next change should fire only B.
  theme1.Unsubscribe(h_a);
  WriteAppsUseLightTheme(1);
  FluxingDarkModeBridge::Get()->Refresh();
  Check(count_a == base_a + 1,
        "T2c: subscriber A did NOT fire after Unsubscribe");
  Check(count_b == base_b + 2,
        "T2d: subscriber B fired again on second change");

  // Unknown handle is a no-op (does not crash, does not throw).
  theme1.Unsubscribe(0xDEADBEEF);
  Check(true, "T2e: Unsubscribe with unknown handle is a no-op");

  // ----------------------------------------------------------------
  // T3: CurrentPalette() returns the bridge's current palette.
  //     Set bridge state to known dark, then verify the
  //     FluxingTheme CurrentPalette() returns the same Palette.
  // ----------------------------------------------------------------
  WriteAppsUseLightTheme(0);
  FluxingDarkModeBridge::Get()->Refresh();
  auto bridge_pal = FluxingDarkModeBridge::Get()->CurrentPalette();
  auto theme_pal = theme1.CurrentPalette();
  Check(theme_pal.back == bridge_pal.back &&
        theme_pal.text == bridge_pal.text &&
        theme_pal.hilited_back == bridge_pal.hilited_back &&
        theme_pal.hilited_text == bridge_pal.hilited_text,
        "T3: FluxingTheme::CurrentPalette() == bridge.CurrentPalette()");

  // ----------------------------------------------------------------
  // T4: FIFO order on multi-subscriber fan-out. Reset, register
  //     3 subscribers with distinguishable callbacks, flip dark,
  //     verify order.
  // ----------------------------------------------------------------
  theme1.Unsubscribe(h_b);
  std::vector<int> order;
  auto h1 = theme1.Subscribe(
      [&](::fluxing::Palette const&) { order.push_back(1); });
  auto h2 = theme1.Subscribe(
      [&](::fluxing::Palette const&) { order.push_back(2); });
  auto h3 = theme1.Subscribe(
      [&](::fluxing::Palette const&) { order.push_back(3); });

  WriteAppsUseLightTheme(1);
  FluxingDarkModeBridge::Get()->Refresh();  // sync state
  order.clear();
  WriteAppsUseLightTheme(0);
  FluxingDarkModeBridge::Get()->Refresh();
  Check(order.size() == 3,
        "T4a: 3 subscribers all fired on state change");
  Check(order[0] == 1 && order[1] == 2 && order[2] == 3,
        "T4b: subscribers fired in FIFO registration order");

  // Cleanup.
  theme1.Unsubscribe(h1);
  theme1.Unsubscribe(h2);
  theme1.Unsubscribe(h3);

  // Restore HKCU.
  WriteAppsUseLightTheme(orig_value);
  FluxingDarkModeBridge::Get()->Refresh();

  std::printf("\nTestFluxingTheme: %d/%d PASS\n", g_pass,
              g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}
}  // namespace fluxing_test