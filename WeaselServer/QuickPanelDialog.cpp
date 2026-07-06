// spec 036 + spec 038: QuickPanelDialog implementation. See
// QuickPanelDialog.h for design notes.
//
// L47 include order (Windows SDK 10.0.26100.0): the FluxingComponents
// .h files reference D2D/DirectWrite types (ID2D1Factory, IDWriteFactory)
// and ComPtr<>, so the PCH must include <unknwn.h> + <d2d1.h> +
// <dwrite.h> + <wrl/client.h> in that order. WeaselServer stdafx.h
// (WTL/ATL) does not include these, so we include them here after
// stdafx.h. Order matters per L47-#4: d2d1.h transitively includes
// dcommon.h which requires IUnknown from unknwn.h.

#include "stdafx.h"

// L47-#4: include order is load-bearing. WeaselServer/stdafx.h does
// NOT include d2d1/dwrite (it pulls in WTL/ATL instead), so we add
// them here in the documented safe order for Windows SDK 10.0.26100.0.
#include <unknwn.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include "QuickPanelDialog.h"
#include "resource.h"

#include "FluxingComponents/Button.h"
#include "FluxingComponents/Toggle.h"
#include "FluxingComponents/Label.h"
#include "FluxingComponents/Panel.h"

// spec 036 win32 constants. Defined here to keep the header light.
#define QP_TIMER_AUTOCLOSE 1
#define QP_AUTOCLOSE_DELAY_MS 1000
#define QP_WIDTH 300
#define QP_HEIGHT 150
#define QP_BUTTON_W 130
#define QP_BUTTON_H 36

// Window class name. Registered once on first Show() call.
static const wchar_t kClassName[] = L"FluxingQuickPanel_v0";

// Static state (private members; class methods can access them
// directly, lambdas must use the public accessors).
HWND QuickPanelDialog::s_hwnd = NULL;
std::function<void(bool)> QuickPanelDialog::s_onAsciiToggle;
std::function<void()> QuickPanelDialog::s_onDeploy;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_deploy_button_;
std::unique_ptr<fluxing::ui::FluxingToggle>
    QuickPanelDialog::s_ascii_toggle_;
std::unique_ptr<fluxing::ui::FluxingLabel>
    QuickPanelDialog::s_title_label_;
std::unique_ptr<fluxing::ui::FluxingPanel>
    QuickPanelDialog::s_card_panel_;

// Current displayed ASCII state (drives the toggle knob position).
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

// AP-038-F: Tear down all 4 FluxingComponents unique_ptrs. The
// destructors run deterministically: Destroy() -> KillTimer (toggle
// animation) + Unsubscribe (theme) + ReleaseHwndRenderTarget (D2D)
// + DestroyWindow (child HWND). After this returns, the FluxingToggle
// 200ms slide timer is guaranteed dead.
void DestroyFluxingControls() {
  QuickPanelDialog::DeployButton().reset();
  QuickPanelDialog::AsciiToggle().reset();
  QuickPanelDialog::TitleLabel().reset();
  QuickPanelDialog::CardPanel().reset();
}

// T002: Instantiate the 4 spec 037 Fluxing controls under the
// dialog HWND. The card panel is the visual container (8px rounded
// fill); the title label sits above it; the ASCII toggle and Deploy
// button are children of the card panel.
//
// spec 037 R3: FluxingButton/Toggle fire on_click / on_changed
// directly from WM_LBUTTONUP - they do NOT route through WM_COMMAND.
// This is the entire point of using the spec 037 controls - the
// ASCII toggle callback fires inside the toggle's WndProc, the
// Deploy callback fires inside the button's WndProc, and we never
// see WM_COMMAND for them. The native close (IDCANCEL) button
// continues to fire WM_COMMAND.
void CreateFluxingControls(HWND hwnd) {
  using namespace fluxing::ui;

  RECT client;
  GetClientRect(hwnd, &client);

  // L50 + AP-050 layout fix: enforce minimum height for TitleLabel (17pt Large
  // needs ~28px; original 20px truncated the text glyphs and showed only the
  // D2D rt background). Also align CardPanel top to leave room, and
  // stop CardPanel right edge from overflowing client.right (which
  // would silently clip under WS_BORDER).
  // Title label: "Quick Panel" at the top of the dialog (17pt Large).
  // L50: rect.top=4, rect.bottom=30 -> height=26 -> enough for 17pt text
  // and 4px padding both sides.
  RECT title_rc = {10, 4, client.right - 32, 30};
  QuickPanelDialog::TitleLabel() = FluxingLabel::Create(
      hwnd, title_rc, L"Quick Panel", FluxingLabel::FontSize::Large);

  // Card panel: the rounded background container under the title.
  // L50: card_rc.top=32 (was 30) gives 2px breathing room under title.
  // L50: card_rc.right=client.right-2 (was -5) ensures the CardPanel
  // stays inside client area even with WS_BORDER insets (typically 1-2px).
  RECT card_rc = {5, 32, client.right - 2, client.bottom - 5};
  QuickPanelDialog::CardPanel() = FluxingPanel::Create(
      hwnd, card_rc, FluxingPanel::Style::Card);

  // ASCII toggle (FluxingToggle): inside the card panel, left side.
  // Initial position reflects the current ASCII state (Chinese on
  // entry = off; ASCII = on).
  // L50: use card-local coords (parent=card), width 50, height 20.
  HWND card = QuickPanelDialog::CardPanel()->Hwnd();
  RECT card_client;
  GetClientRect(card, &card_client);
  LONG card_local_width = card_client.right;
  RECT toggle_rc = {15, 12, 65, 32};
  QuickPanelDialog::AsciiToggle() = FluxingToggle::Create(
      card, toggle_rc, /*initial=*/s_currentAscii);
  QuickPanelDialog::AsciiToggle()->SetOnChanged(
      [](bool on) {
        s_currentAscii = on;
        if (QuickPanelDialog::OnAsciiToggle()) {
          QuickPanelDialog::OnAsciiToggle()(on);
        }
        HWND h = QuickPanelDialog::ActiveHwnd();
        if (h && IsWindow(h)) {
          DestroyWindow(h);
        }
      });

  // Deploy button (FluxingButton::Primary): inside the card panel,
  // right side.
  // L50: use card-local coords. client.right in card context = card width.
  // width=100 (was 95), height=24 (was 30), top=10 (was 5) gives 12px
  // vertical breathing room inside the 20px-tall card padding.
  RECT deploy_rc = {static_cast<LONG>(card_local_width - 115), 10, static_cast<LONG>(card_local_width - 15), 34};
  QuickPanelDialog::DeployButton() = FluxingButton::Create(
      card, deploy_rc, L"Deploy", FluxingButton::Style::Primary);
  QuickPanelDialog::DeployButton()->SetOnClick([]() {
    if (QuickPanelDialog::OnDeploy()) {
      QuickPanelDialog::OnDeploy()();
    }
    HWND h = QuickPanelDialog::ActiveHwnd();
    if (h && IsWindow(h)) {
      DestroyWindow(h);
    }
  });
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
  // Tear down any leftover Fluxing controls from a prior Show().
  DestroyFluxingControls();

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
}

void QuickPanelDialog::Hide() {
  // AP-038-F: Tear down Fluxing controls BEFORE DestroyWindow so
  // the FluxingToggle 200ms animation timer is killed while its
  // HWND is still valid (its KillTimer needs a live hwnd_).
  DestroyFluxingControls();
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
  // T002: instantiate the 4 spec 037 Fluxing components.
  CreateFluxingControls(hwnd);

  // spec 036 AP-036-M: native close (X) button preserved at the
  // top-right corner. TestQuickPanelDialog T1 + T6 verify that
  // this HWND exists and that WM_COMMAND IDCANCEL fires DestroyWindow.
  // L50 fix: native close X button now uses client.right (not QP_WIDTH) for
  // x position, because WS_BORDER reduces client area by ~2px each side.
  // old code: QP_WIDTH - 30 = 270 -> with WS_BORDER the button straddled
  // the client area border and overlapped TitleLabel.
  // new code: client.right - 25 = ~267 (cleanly inside client area), top=4
  // aligns with title_rc.top=4.
  RECT client_for_x;
  GetClientRect(hwnd, &client_for_x);
  CreateWindowExW(0, L"BUTTON", L"X",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  client_for_x.right - 25, 4, 20, 20,
                  hwnd, (HMENU)(UINT_PTR)IDCANCEL,
                  GetModuleHandle(NULL), NULL);

  return 0;
}

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  KillTimer(hwnd, QP_TIMER_AUTOCLOSE);
  // AP-038-F: clean up Fluxing components here too. WM_DESTROY
  // fires after the user clicks any button (callback already
  // triggered DestroyWindow). The destructors run KillTimer +
  // Unsubscribe + ReleaseHwndRenderTarget + DestroyWindow in the
  // documented order.
  DestroyFluxingControls();
  if (s_hwnd == hwnd) {
    s_hwnd = NULL;
  }
  s_onAsciiToggle = nullptr;
  s_onDeploy = nullptr;
  return 0;
}

LRESULT QuickPanelDialog::OnCommand(HWND, WPARAM wParam, LPARAM) {
  WORD id = LOWORD(wParam);
  // Only the native close (X) button routes through WM_COMMAND in
  // the spec 038 layout. The ASCII toggle and Deploy button fire
  // their callbacks directly from their own WndProcs via
  // SetOnChanged / SetOnClick.
  switch (id) {
    case IDCANCEL:
      // X button -> DestroyWindow; OnDestroy tears down controls.
      if (s_hwnd && IsWindow(s_hwnd)) {
        DestroyWindow(s_hwnd);
      }
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
    if (GetFocus() != hwnd) {
      DestroyWindow(hwnd);
    }
  }
  return 0;
}
