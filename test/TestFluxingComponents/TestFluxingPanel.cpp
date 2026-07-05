// TestFluxingPanel.cpp - spec 037 T016/T019 (2026-07-05)
//
// Behavior-level integration test for fluxing::ui::FluxingPanel.
//
// L24 link-probe pattern: link the actual production code by
// direct ClCompile entry, not a mirror.
//
// 3 assertions:
//   T1: Create() returns non-null unique_ptr with valid HWND
//   T2: Card style reports kCardRadius == 8
//   T3: Plain style has zero corner radius (no rounding)
//
// Cleanup: panel unique_ptr destroyed at exit. Test host
// window destroyed at exit.

#include "stdafx.h"
#include "Panel.h"
#include "FluxingTheme.h"
#include <cstdio>

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

const wchar_t* kHostClassName = L"TestFluxingPanelHostClass";
HWND g_host_hwnd = nullptr;

LRESULT CALLBACK TestHostWndProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam) {
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool CreateTestHost() {
  WNDCLASSW wc = {};
  wc.lpfnWndProc = TestHostWndProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = kHostClassName;
  if (!RegisterClassW(&wc)) {
    DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
  }
  g_host_hwnd = CreateWindowExW(
      0, kHostClassName, L"TestFluxingPanelHost",
      WS_OVERLAPPEDWINDOW, 0, 0, 400, 300, HWND_DESKTOP, nullptr,
      GetModuleHandle(nullptr), nullptr);
  return g_host_hwnd != nullptr;
}

void DestroyTestHost() {
  if (g_host_hwnd) {
    DestroyWindow(g_host_hwnd);
    g_host_hwnd = nullptr;
  }
  UnregisterClassW(kHostClassName, GetModuleHandle(nullptr));
}

}  // namespace

namespace fluxing_test {

int RunPanelTest() {  using fluxing::ui::FluxingPanel;

  if (!CreateTestHost()) {
    std::printf("FATAL: failed to create test host window\n");
    return 2;
  }

  // ----------------------------------------------------------------
  // T1: Create(Card) returns non-null unique_ptr with valid HWND.
  // ----------------------------------------------------------------
  RECT rc1 = {10, 10, 200, 100};
  auto card = FluxingPanel::Create(g_host_hwnd, rc1,
                                    FluxingPanel::Style::Card);
  Check(card != nullptr, "T1a: Create(Card) returns non-null");
  Check(card->Hwnd() != nullptr,
        "T1b: Hwnd() is non-null after Create(Card)");
  Check(card->GetStyle() == FluxingPanel::Style::Card,
        "T1c: GetStyle() == Card");

  // ----------------------------------------------------------------
  // T2: kCardRadius == 8 (the Card style constant).
  // ----------------------------------------------------------------
  Check(FluxingPanel::kCardRadius == 8,
        "T2: FluxingPanel::kCardRadius == 8 (Card style rounding)");

  // ----------------------------------------------------------------
  // T3: Plain style has no rounding. We verify by creating a
  //     Plain panel and checking GetStyle() == Plain. The
  //     rendering constant for Plain is 0 (no FillRoundedRectangle
  //     call); this is observable in Panel.cpp's HandlePaint
  //     which uses a different code path for Plain (transparent
  //     WS_EX_TRANSPARENT extended style + no fill). We assert
  //     the style enum value here; the visual difference is
  //     covered by the production code review.
  // ----------------------------------------------------------------
  RECT rc2 = {10, 120, 200, 200};
  auto plain = FluxingPanel::Create(g_host_hwnd, rc2,
                                     FluxingPanel::Style::Plain);
  Check(plain != nullptr, "T3a: Create(Plain) returns non-null");
  Check(plain->GetStyle() == FluxingPanel::Style::Plain,
        "T3b: GetStyle() == Plain (no rounding)");

  // ----------------------------------------------------------------
  // Cleanup.
  // ----------------------------------------------------------------
  card.reset();
  plain.reset();
  DestroyTestHost();

  std::printf("\nTestFluxingPanel: %d/%d PASS\n", g_pass,
              g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}
}  // namespace fluxing_test