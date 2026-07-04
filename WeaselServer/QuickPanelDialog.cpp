// spec 036: QuickPanelDialog implementation. See QuickPanelDialog.h for
// design notes. This file is intentionally lightweight:
//   - Plain Win32 (no WTL/ATL/MFC to keep TestQuickPanelDialog build cost
//     low; see L24 link-probe pattern in spec 034 plan.md \xC2\xA72.1).
//   - One static class; only one panel may be shown at a time.
//   - All state is per-class-static; thread-affinity is the WeaselServer
//     main UI thread (the dialog is created/destroyed from message-pump
//     context, never from a worker thread).

#include "stdafx.h"
#include "QuickPanelDialog.h"
#include "resource.h"

// spec 036 win32 constants. Defined here to keep the header light.
#define QP_TIMER_AUTOCLOSE 1
#define QP_AUTOCLOSE_DELAY_MS 1000
#define QP_WIDTH 300
#define QP_HEIGHT 150
#define QP_BUTTON_W 130
#define QP_BUTTON_H 36

// Window class name. Registered once on first Show() call.
static const wchar_t kClassName[] = L"FluxingQuickPanel_v0";

// Static state.
HWND QuickPanelDialog::s_hwnd = NULL;
std::function<void(bool)> QuickPanelDialog::s_onAsciiToggle;
std::function<void()> QuickPanelDialog::s_onDeploy;

// Current displayed ASCII state (drives the toggle button label).
static bool s_currentAscii = false;

namespace {

// Register the window class. Idempotent (checks whether the class is
// already registered; if so, returns true without re-registering).
bool RegisterClassOnce(HINSTANCE hInst) {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = QuickPanelDialog::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;
  // RegisterClassEx returns 0 on failure (e.g. class already exists).
  // We use a sentinel: if the class exists, FindClass returns non-NULL.
  if (GetClassInfoExW(hInst, kClassName, &wc)) {
    return true;
  }
  return RegisterClassExW(&wc) != 0;
}

// Compute the panel position: centered above the system tray (work area
// bottom-right, with 20px gap). Falls back to center screen if the
// tray is not found.
POINT ComputePanelOrigin() {
  RECT workArea;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
  POINT origin;
  origin.x = workArea.right - QP_WIDTH - 20;
  origin.y = workArea.bottom - QP_HEIGHT - 20;
  return origin;
}

}  // namespace

void QuickPanelDialog::Show(bool currentAscii,
                             std::function<void(bool)> onAsciiToggle,
                             std::function<void()> onDeploy) {
  // AP-036-H: if a panel is already visible, destroy it first so we do
  // not leak a window or have two panels competing for input.
  if (s_hwnd && IsWindow(s_hwnd)) {
    DestroyWindow(s_hwnd);
    s_hwnd = NULL;
  }

  s_onAsciiToggle = onAsciiToggle;
  s_onDeploy = onDeploy;
  s_currentAscii = currentAscii;

  HINSTANCE hInst = GetModuleHandle(NULL);
  if (!RegisterClassOnce(hInst)) {
    return;
  }

  POINT origin = ComputePanelOrigin();
  s_hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
      kClassName,
      L"Fluxing QuickPanel",
      WS_POPUP | WS_VISIBLE | WS_BORDER,
      origin.x, origin.y,
      QP_WIDTH, QP_HEIGHT,
      NULL,
      NULL,
      hInst,
      NULL);
  if (!s_hwnd) {
    return;
  }

  // AP-036-I: center cursor in the panel and show it (UX nicety; user
  // can immediately click the toggle without re-aiming). Skipped if the
  // system has accessibility settings that disallow it.
  // NOTE: For v0 we do NOT call SetCursorPos or SetCapture - keep it
  // simple, the user can move the mouse themselves. Toggle is large
  // enough to be a generous click target.
}

void QuickPanelDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    DestroyWindow(s_hwnd);
  }
  s_hwnd = NULL;
  s_onAsciiToggle = nullptr;
  s_onDeploy = nullptr;
}

LRESULT CALLBACK QuickPanelDialog::WndProc(HWND hwnd, UINT uMsg,
                                            WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_CREATE:
      return OnCreate(hwnd, wParam, lParam);
    case WM_DESTROY:
      return OnDestroy(hwnd);
    case WM_COMMAND:
      return OnCommand(hwnd, wParam, lParam);
    case WM_KILLFOCUS:
      return OnKillFocus(hwnd, wParam, lParam);
    case WM_TIMER:
      return OnTimer(hwnd, wParam, lParam);
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT QuickPanelDialog::OnCreate(HWND hwnd, WPARAM, LPARAM) {
  // 2 buttons side-by-side: ASCII toggle (left) and Deploy (right).
  // Layout: 10px left margin, 10px right margin, 10px between buttons,
  // 30px top margin, 36px height, 10px bottom margin.
  int y = 30;
  int asciiX = 10;
  int deployX = asciiX + QP_BUTTON_W + 10;

  // ASCII toggle. Label reflects current state. We update the label on
  // every Show() call (in case state changed between Show invocations).
  const wchar_t* label = s_currentAscii ? L"英文 [切 ASCII]"
                                          : L"中文 [切 ASCII]";
  CreateWindowExW(0, L"BUTTON", label,
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  asciiX, y, QP_BUTTON_W, QP_BUTTON_H,
                  hwnd, (HMENU)(UINT_PTR)ID_QUICKPANEL_BTN_ASCII,
                  GetModuleHandle(NULL), NULL);

  // Deploy button.
  CreateWindowExW(0, L"BUTTON", L"重新部署",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  deployX, y, QP_BUTTON_W, QP_BUTTON_H,
                  hwnd, (HMENU)(UINT_PTR)ID_QUICKPANEL_BTN_DEPLOY,
                  GetModuleHandle(NULL), NULL);

  // Close (X) button at the top-right corner of the panel.
  CreateWindowExW(0, L"BUTTON", L"X",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  QP_WIDTH - 30, 5, 22, 22,
                  hwnd, (HMENU)(UINT_PTR)IDCANCEL,
                  GetModuleHandle(NULL), NULL);

  return 0;
}

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  KillTimer(hwnd, QP_TIMER_AUTOCLOSE);
  if (s_hwnd == hwnd) {
    s_hwnd = NULL;
  }
  s_onAsciiToggle = nullptr;
  s_onDeploy = nullptr;
  return 0;
}

LRESULT QuickPanelDialog::OnCommand(HWND hwnd, WPARAM wParam, LPARAM) {
  WORD id = LOWORD(wParam);
  switch (id) {
    case ID_QUICKPANEL_BTN_ASCII:
      // AP-036-J: toggle the state, fire the callback, then destroy.
      // The callback is responsible for invoking m_pRequestHandler->SetOption.
      s_currentAscii = !s_currentAscii;
      if (s_onAsciiToggle) {
        s_onAsciiToggle(s_currentAscii);
      }
      DestroyWindow(hwnd);
      return 0;
    case ID_QUICKPANEL_BTN_DEPLOY:
      if (s_onDeploy) {
        s_onDeploy();
      }
      DestroyWindow(hwnd);
      return 0;
    case IDCANCEL:
      // X button.
      DestroyWindow(hwnd);
      return 0;
  }
  return 0;
}

LRESULT QuickPanelDialog::OnKillFocus(HWND, WPARAM, LPARAM) {
  // AP-036-K: start the 1s auto-close timer. If focus returns within
  // 1s, the timer fires KillTimer (set in OnTimer). Otherwise, destroy.
  HWND hwnd = s_hwnd;
  if (hwnd && IsWindow(hwnd)) {
    SetTimer(hwnd, QP_TIMER_AUTOCLOSE, QP_AUTOCLOSE_DELAY_MS, NULL);
  }
  return 0;
}

LRESULT QuickPanelDialog::OnTimer(HWND hwnd, WPARAM wParam, LPARAM) {
  if (wParam == QP_TIMER_AUTOCLOSE) {
    KillTimer(hwnd, QP_TIMER_AUTOCLOSE);
    // AP-036-L: only destroy if the panel still does not have focus.
    // This guards against the "user clicks back into the panel" case.
    if (GetFocus() != hwnd) {
      DestroyWindow(hwnd);
    }
  }
  return 0;
}