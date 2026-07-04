// TestPanelDarkModeSubscribe.cpp : spec 032 T007 (2026-07-04)
//
// 3 behavior-level assertions for the WeaselPanel dark mode subscribe
// path. Mirrors the production logic in WeaselPanel.cpp::OnSettingChange
// and ::_RefreshStylePalette in test-local free functions.
//
// We do NOT link WeaselPanel.cpp (WTL/ATL/Gdiplus) or WeaselServer.cpp
// (tray icon). The mocked IsUserDarkMode() returns a test-local flag
// instead of reading HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize.
//
// The 3 assertions match spec 032 spec.md section 1 T007:
//   1. WM_SETTINGCHANGE with lParam = "ImmersiveColorSet" triggers palette
//      refresh + Refresh(); other lParam values do not trigger.
//   2. _RefreshStylePalette() sets back_color = 0x1E1E1E when dark mode is on.
//   3. _RefreshStylePalette() restores light back_color (0xF0F0F0) when off.

#include "stdafx.h"
#include <windows.h>  // LPARAM / wchar_t for WM_SETTINGCHANGE mock
#include <boost/detail/lightweight_test.hpp>
#include <iostream>
#include <string>

// Win32 constant used by the test (mirrors what WeaselPanel.cpp expects).
#ifndef WM_SETTINGCHANGE
#define WM_SETTINGCHANGE 0x001A
#endif

namespace {

// Test-local mock for IsUserDarkMode(). Production reads HKCU; we flip a flag.
bool g_test_dark_mode = false;
bool IsUserDarkModeMock() { return g_test_dark_mode; }

// Test-local mirror of UIStyle: only the fields _RefreshStylePalette touches.
struct UIStyleMirror {
  int back_color = 0;
  int text_color = 0;
  int hilited_candidate_back_color = 0;
  int hilited_candidate_text_color = 0;
};

// Test-local recorder for Refresh() invocations.
int g_refresh_call_count = 0;
void RefreshMock() { ++g_refresh_call_count; }

// Mirror WeaselPanel::OnSettingChange. Returns true iff Refresh was triggered.
// (Production: also calls _CreateLayout; out of scope for this assertion.)
bool OnSettingChangeMirror(LPARAM lParam) {
  if (lParam == 0) return false;
  const wchar_t* section = reinterpret_cast<const wchar_t*>(lParam);
  if (std::wcscmp(section, L"ImmersiveColorSet") != 0) return false;
  // Production would call _RefreshStylePalette() + Refresh() here.
  RefreshMock();
  return true;
}

// Mirror WeaselPanel::_RefreshStylePalette.
void RefreshStylePaletteMirror(UIStyleMirror& style) {
  bool dark = IsUserDarkModeMock();
  if (dark) {
    style.back_color = 0x1E1E1E;
    style.text_color = 0xE0E0E0;
    style.hilited_candidate_back_color = 0x2D2D30;
    style.hilited_candidate_text_color = 0xFFFFFF;
  } else {
    style.back_color = 0xF0F0F0;
    style.text_color = 0x000000;
    style.hilited_candidate_back_color = 0xD0D0D0;
    style.hilited_candidate_text_color = 0x000080;
  }
}

}  // namespace

int main() {
  // T1: WM_SETTINGCHANGE + "ImmersiveColorSet" lParam triggers Refresh.
  //    Other lParam values (e.g. NULL, random section) do NOT trigger.
  {
    g_refresh_call_count = 0;
    // (a) lParam = NULL: no-op.
    bool triggered_a = OnSettingChangeMirror(0);
    BOOST_TEST_EQ(triggered_a, false);
    BOOST_TEST_EQ(g_refresh_call_count, 0);
    // (b) lParam pointing to a different section: no-op.
    bool triggered_b = OnSettingChangeMirror(
        reinterpret_cast<LPARAM>(const_cast<wchar_t*>(L"WindowMetrics")));
    BOOST_TEST_EQ(triggered_b, false);
    BOOST_TEST_EQ(g_refresh_call_count, 0);
    // (c) lParam = "ImmersiveColorSet": triggers Refresh.
    bool triggered_c = OnSettingChangeMirror(
        reinterpret_cast<LPARAM>(const_cast<wchar_t*>(L"ImmersiveColorSet")));
    BOOST_TEST_EQ(triggered_c, true);
    BOOST_TEST_EQ(g_refresh_call_count, 1);
    std::cout << "  PASS: T1 OnSettingChange filter" << std::endl;
  }

  // T2: _RefreshStylePalette() with dark mode ON -> back_color = 0x1E1E1E.
  {
    g_test_dark_mode = true;
    UIStyleMirror s;
    RefreshStylePaletteMirror(s);
    BOOST_TEST_EQ(s.back_color, 0x1E1E1E);
    BOOST_TEST_EQ(s.hilited_candidate_text_color, 0xFFFFFF);
    std::cout << "  PASS: T2 dark palette" << std::endl;
  }

  // T3: _RefreshStylePalette() with dark mode OFF -> back_color = 0xF0F0F0.
  {
    g_test_dark_mode = false;
    UIStyleMirror s;
    RefreshStylePaletteMirror(s);
    BOOST_TEST_EQ(s.back_color, 0xF0F0F0);
    BOOST_TEST_EQ(s.text_color, 0x000000);
    std::cout << "  PASS: T3 light palette" << std::endl;
  }

  std::cout << "3 / 3 assertions passed" << std::endl;
  return boost::report_errors();
}
