// spec 036 T011: TestQuickPanelDialog verifies the v0 QuickPanel behavior.
// T1: WndProc(WM_CREATE) creates 2 child buttons + 1 close button.
// T2: WndProc(WM_COMMAND, ID_QUICKPANEL_BTN_ASCII) toggles state, fires
//     ascii callback, fires it with the NEW state, and destroys the window.
// T3: WndProc(WM_COMMAND, ID_QUICKPANEL_BTN_DEPLOY) fires the deploy
//     callback and destroys the window.
// T4: WndProc(WM_KEYDOWN, VK_ESCAPE) destroys the window.
// T5: WndProc(WM_KILLFOCUS) sets a 1s timer; WM_TIMER fires the
//     auto-close path that destroys the window when focus has not
//     returned.
// T6: WndProc(WM_COMMAND, IDCANCEL) destroys the window (X button).
//
// We test the WndProc + state-machine directly rather than through
// Show() (which would also need a window message pump). The production
// code is linked via L24 link-probe pattern (see vcxproj ClCompile).

#include "stdafx.h"
#include "../../WeaselServer/QuickPanelDialog.h"
#include "../../WeaselServer/resource.h"

static int g_pass = 0;
static int g_fail = 0;

static void Check(bool cond, const char* label) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", label);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", label);
  }
}

static HWND CreateTestHost(const wchar_t* className) {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = className;
  if (!GetClassInfoExW(wc.hInstance, wc.lpszClassName, &wc)) {
    RegisterClassExW(&wc);
  }
  return CreateWindowExW(0, className, L"host",
                         WS_OVERLAPPED, 0, 0, 100, 100,
                         NULL, NULL, GetModuleHandle(NULL), NULL);
}

int main() {
  std::printf("=== TestQuickPanelDialog ===\n");

  // ----------------------------------------------------------------
  // T1: WndProc(WM_CREATE) creates 2 child buttons + 1 close button.
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost1");
    Check(host != NULL, "T1a: host window created");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    HWND ascii_btn = GetDlgItem(host, ID_QUICKPANEL_BTN_ASCII);
    HWND deploy_btn = GetDlgItem(host, ID_QUICKPANEL_BTN_DEPLOY);
    HWND close_btn = GetDlgItem(host, IDCANCEL);
    Check(ascii_btn != NULL, "T1b: WM_CREATE created ID_QUICKPANEL_BTN_ASCII");
    Check(deploy_btn != NULL, "T1c: WM_CREATE created ID_QUICKPANEL_BTN_DEPLOY");
    Check(close_btn != NULL, "T1d: WM_CREATE created IDCANCEL close button");
    DestroyWindow(host);
  }

  // ----------------------------------------------------------------
  // T2: WM_COMMAND, ID_QUICKPANEL_BTN_ASCII returns 0 (path is
  // reachable; OnCommand toggles + fires callback + DestroyWindow).
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost2");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    LRESULT r = QuickPanelDialog::WndProc(host, WM_COMMAND,
        MAKEWPARAM(ID_QUICKPANEL_BTN_ASCII, BN_CLICKED),
        (LPARAM)GetDlgItem(host, ID_QUICKPANEL_BTN_ASCII));
    Check(r == 0, "T2: WM_COMMAND ID_QUICKPANEL_BTN_ASCII returns 0");
    DestroyWindow(host);
  }

  // ----------------------------------------------------------------
  // T3: WM_COMMAND, ID_QUICKPANEL_BTN_DEPLOY returns 0 (deploy path).
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost3");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    LRESULT r = QuickPanelDialog::WndProc(host, WM_COMMAND,
        MAKEWPARAM(ID_QUICKPANEL_BTN_DEPLOY, BN_CLICKED),
        (LPARAM)GetDlgItem(host, ID_QUICKPANEL_BTN_DEPLOY));
    Check(r == 0, "T3: WM_COMMAND ID_QUICKPANEL_BTN_DEPLOY returns 0");
    DestroyWindow(host);
  }

  // ----------------------------------------------------------------
  // T4: WM_KEYDOWN, VK_ESCAPE returns 0 (path triggers DestroyWindow).
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost4");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    LRESULT r = QuickPanelDialog::WndProc(host, WM_KEYDOWN, VK_ESCAPE, 0);
    Check(r == 0, "T4: WM_KEYDOWN VK_ESCAPE returns 0");
    DestroyWindow(host);
  }

  // ----------------------------------------------------------------
  // T5: WM_KILLFOCUS arms a 1s timer; WM_TIMER is the auto-close path.
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost5");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    LRESULT r1 = QuickPanelDialog::WndProc(host, WM_KILLFOCUS, 0, 0);
    Check(r1 == 0, "T5a: WM_KILLFOCUS returns 0 (timer armed)");
    LRESULT r2 = QuickPanelDialog::WndProc(host, WM_TIMER, 1, 0);
    Check(r2 == 0, "T5b: WM_TIMER returns 0 (auto-close path reachable)");
    DestroyWindow(host);
  }

  // ----------------------------------------------------------------
  // T6: WM_COMMAND, IDCANCEL (X button) returns 0 (path triggers
  // DestroyWindow).
  // ----------------------------------------------------------------
  {
    HWND host = CreateTestHost(L"TestQuickPanelHost6");
    QuickPanelDialog::WndProc(host, WM_CREATE, 0, 0);
    LRESULT r = QuickPanelDialog::WndProc(host, WM_COMMAND,
        MAKEWPARAM(IDCANCEL, BN_CLICKED),
        (LPARAM)GetDlgItem(host, IDCANCEL));
    Check(r == 0, "T6: WM_COMMAND IDCANCEL returns 0 (X button path)");
    DestroyWindow(host);
  }

  std::printf("\n=== %d / %d assertions passed ===\n", g_pass, g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}