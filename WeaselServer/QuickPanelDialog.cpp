// spec 036 + spec 038 + spec 045 v0.18.29.0: QuickPanelDialog
// implementation. See QuickPanelDialog.h for design notes.
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

// spec 045 v0.18.29.0: 8-12 entry layout per spec 006 design.md sec 1.2.
// 5 rows: title / 3 toggles / schema / 2 folder btns / deploy+quit.
// QP_WIDTH grew from 300 -> 480, QP_HEIGHT grew from 150 -> 220.
#define QP_TIMER_AUTOCLOSE 1
#define QP_AUTOCLOSE_DELAY_MS 1000
#define QP_WIDTH 480
#define QP_HEIGHT 220
#define QP_BUTTON_H 28
#define QP_TOGGLE_W 50
#define QP_TOGGLE_H 20
#define QP_ROW_GAP 4

// Window class name. Registered once on first Show() call.
static const wchar_t kClassName[] = L"FluxingQuickPanel_v1";

// Static state (private members; class methods can access them
// directly, lambdas must use the public accessors).
HWND QuickPanelDialog::s_hwnd = NULL;
std::function<void(bool)> QuickPanelDialog::s_onAsciiToggle;
std::function<void(bool)> QuickPanelDialog::s_onSimpToggle;
std::function<void(bool)> QuickPanelDialog::s_onFullwidthToggle;
std::function<void(const std::wstring&)> QuickPanelDialog::s_onSelectSchema;
std::function<void()> QuickPanelDialog::s_onOpenUserFolder;
std::function<void()> QuickPanelDialog::s_onOpenProgramFolder;
std::function<void()> QuickPanelDialog::s_onDeploy;
std::function<void()> QuickPanelDialog::s_onQuit;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_deploy_button_;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_quit_button_;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_schema_button_;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_user_folder_button_;
std::unique_ptr<fluxing::ui::FluxingButton>
    QuickPanelDialog::s_program_folder_button_;
std::unique_ptr<fluxing::ui::FluxingToggle>
    QuickPanelDialog::s_ascii_toggle_;
std::unique_ptr<fluxing::ui::FluxingToggle>
    QuickPanelDialog::s_simp_toggle_;
std::unique_ptr<fluxing::ui::FluxingToggle>
    QuickPanelDialog::s_fullwidth_toggle_;
std::unique_ptr<fluxing::ui::FluxingLabel>
    QuickPanelDialog::s_title_label_;
std::unique_ptr<fluxing::ui::FluxingLabel>
    QuickPanelDialog::s_schema_label_;
std::unique_ptr<fluxing::ui::FluxingPanel>
    QuickPanelDialog::s_card_panel_;

// Cached initial state (drives the toggle knob position).
static bool s_currentAscii = false;
static bool s_currentSimp = false;
static bool s_currentFullwidth = false;

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

// Compute the panel position: above the system tray (work area
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

// AP-038-F / AP-045-B: Tear down all FluxingComponents unique_ptrs.
// Destructors run deterministically: Destroy() -> KillTimer (toggle
// animation) + Unsubscribe (theme) + ReleaseHwndRenderTarget (D2D)
// + DestroyWindow (child HWND). After this returns, the FluxingToggle
// 200ms slide timers are guaranteed dead.
void DestroyFluxingControls() {
  QuickPanelDialog::DeployButton().reset();
  QuickPanelDialog::QuitButton().reset();
  QuickPanelDialog::SchemaButton().reset();
  QuickPanelDialog::UserFolderButton().reset();
  QuickPanelDialog::ProgramFolderButton().reset();
  QuickPanelDialog::AsciiToggle().reset();
  QuickPanelDialog::SimpToggle().reset();
  QuickPanelDialog::FullwidthToggle().reset();
  QuickPanelDialog::TitleLabel().reset();
  QuickPanelDialog::SchemaLabel().reset();
  QuickPanelDialog::CardPanel().reset();
}

// Helper: build a 3-tuple sub-rect inside the card panel. Each row
// in the 5-row 8-entry layout uses one of these.
struct RowRect {
  RECT a;
  RECT b;
  RECT c;
};

RowRect MakeRow(LONG top, LONG card_w) {
  LONG pad = 12;
  LONG usable = card_w - 2 * pad;
  LONG third = usable / 3;
  RowRect r;
  r.a = {pad, top, pad + third - 4, top + QP_BUTTON_H};
  r.b = {pad + third, top, pad + 2 * third - 4, top + QP_BUTTON_H};
  r.c = {pad + 2 * third, top, pad + usable, top + QP_BUTTON_H};
  return r;
}

// Helper: close the panel after firing a callback (most buttons
// auto-close after click; the toggles do their own DestroyWindow in
// the SetOnChanged lambda so the slide animation can finish first).
template <typename Fn>
void FireAndClose(Fn&& fn) {
  fn();
  HWND h = QuickPanelDialog::ActiveHwnd();
  if (h && IsWindow(h)) {
    DestroyWindow(h);
  }
}

// T002 + spec 045 v0.18.29.0: instantiate the spec 037 Fluxing
// controls under the dialog HWND. The 5-row layout is:
//   Row 1 (top 0):   "Fluxing" title label + native close X
//   Row 2 (top 36):  3 toggles ascii/simp/full + small labels above
//   Row 3 (top 92):  current schema label + 切换 button
//   Row 4 (top 132): 用户文件夹 / 程序文件夹 buttons
//   Row 5 (top 168): 部署 / 退出 buttons
// spec 037 R3: FluxingButton/Toggle fire on_click / on_changed
// directly from WM_LBUTTONUP - they do NOT route through WM_COMMAND.
// The native close (IDCANCEL) button continues to fire WM_COMMAND.
void CreateFluxingControls(HWND hwnd, const std::wstring& schema,
                           const std::vector<std::wstring>& schemas) {
  using namespace fluxing::ui;

  RECT client;
  GetClientRect(hwnd, &client);

  // Row 1: title label "Fluxing" + native close X. Title left side,
  // close X is added in OnCreate (uses client rect).
  RECT title_rc = {16, 8, client.right - 32, 30};
  QuickPanelDialog::TitleLabel() = FluxingLabel::Create(
      hwnd, title_rc, L"\xE2\x9A\x99 Fluxing",  // U+2699 gear icon
      FluxingLabel::FontSize::Large);

  // Card panel: rounded background container covering rows 2-5.
  RECT card_rc = {8, 36, client.right - 8, client.bottom - 8};
  QuickPanelDialog::CardPanel() = FluxingPanel::Create(
      hwnd, card_rc, FluxingPanel::Style::Card);

  HWND card = QuickPanelDialog::CardPanel()->Hwnd();
  RECT card_client;
  GetClientRect(card, &card_client);
  LONG card_w = card_client.right;

  // Helper: inside card-local coords, build a row of 3 cells.
  auto row3 = [&](LONG top) {
    LONG pad = 12;
    LONG usable = card_w - 2 * pad;
    LONG third = usable / 3;
    RowRect r;
    LONG cell_h = QP_BUTTON_H;
    r.a = {pad, top, pad + third - 4, top + cell_h};
    r.b = {pad + third, top, pad + 2 * third - 4, top + cell_h};
    r.c = {pad + 2 * third, top, pad + usable, top + cell_h};
    return r;
  };

  // Row 2: 3 toggles. Each toggle is 50x20; centered in its third.
  RowRect r2 = row3(8);
  LONG toggle_top = r2.a.top + (QP_BUTTON_H - QP_TOGGLE_H) / 2;
  RECT ascii_rc = {r2.a.left, toggle_top,
                   r2.a.left + QP_TOGGLE_W, toggle_top + QP_TOGGLE_H};
  QuickPanelDialog::AsciiToggle() =
      FluxingToggle::Create(card, ascii_rc, s_currentAscii);
  QuickPanelDialog::AsciiToggle()->SetOnChanged([](bool on) {
    s_currentAscii = on;
    auto cb = QuickPanelDialog::OnAsciiToggle();
    FireAndClose([cb, on]() { if (cb) cb(on); });
  });

  RECT simp_rc = {r2.b.left, toggle_top,
                  r2.b.left + QP_TOGGLE_W, toggle_top + QP_TOGGLE_H};
  QuickPanelDialog::SimpToggle() =
      FluxingToggle::Create(card, simp_rc, s_currentSimp);
  QuickPanelDialog::SimpToggle()->SetOnChanged([](bool on) {
    s_currentSimp = on;
    auto cb = QuickPanelDialog::OnSimpToggle();
    FireAndClose([cb, on]() { if (cb) cb(on); });
  });

  RECT fw_rc = {r2.c.left, toggle_top,
                r2.c.left + QP_TOGGLE_W, toggle_top + QP_TOGGLE_H};
  QuickPanelDialog::FullwidthToggle() =
      FluxingToggle::Create(card, fw_rc, s_currentFullwidth);
  QuickPanelDialog::FullwidthToggle()->SetOnChanged([](bool on) {
    s_currentFullwidth = on;
    auto cb = QuickPanelDialog::OnFullwidthToggle();
    FireAndClose([cb, on]() { if (cb) cb(on); });
  });

  // Row 2 labels above the toggles: 中/英 / 简/繁 / 全/半角.
  // Implemented as static native STATIC controls (no Fluxing label
  // needed for tiny captions; we keep the code compact).
  RECT lbl_a = {r2.a.left, 0, r2.a.left + QP_TOGGLE_W, 6};
  RECT lbl_b = {r2.b.left, 0, r2.b.left + QP_TOGGLE_W, 6};
  RECT lbl_c = {r2.c.left, 0, r2.c.left + QP_TOGGLE_W, 6};
  (void)lbl_a; (void)lbl_b; (void)lbl_c;  // see D2D captions below
  // Inline the captions as D2D-rendered FluxingLabel::Small below
  // the toggles using CreateWindowExW + SS_CENTER.
  LONG cap_top = toggle_top + QP_TOGGLE_H + 2;
  CreateWindowExW(0, L"STATIC", L"\xE4\xB8\xAD/\xE8\x8B\xB1",
                  WS_CHILD | WS_VISIBLE | SS_CENTER,
                  r2.a.left, cap_top, QP_TOGGLE_W, 14,
                  card, NULL, GetModuleHandle(NULL), NULL);
  CreateWindowExW(0, L"STATIC", L"\xE7\xAE\x80/\xE7\xB9\x81",
                  WS_CHILD | WS_VISIBLE | SS_CENTER,
                  r2.b.left, cap_top, QP_TOGGLE_W, 14,
                  card, NULL, GetModuleHandle(NULL), NULL);
  CreateWindowExW(0, L"STATIC", L"\xE5\x85\xA8/\xE5\x8D\x8A\xE8\xA7\x92",
                  WS_CHILD | WS_VISIBLE | SS_CENTER,
                  r2.c.left, cap_top, QP_TOGGLE_W + 14, 14,
                  card, NULL, GetModuleHandle(NULL), NULL);

  // Row 3: schema label + 切换 button. Schema label takes 2/3 width.
  LONG row3_top = 56;
  RECT schema_lbl_rc = {12, row3_top, card_w - 100, row3_top + QP_BUTTON_H};
  std::wstring schema_text =
      L"\xE5\xBD\x93\xE5\x89\x8D\xE6\x96\xB9\xE6\xA1\x88: " +
      (schema.empty() ? std::wstring(L"(none)") : schema);
  QuickPanelDialog::SchemaLabel() = FluxingLabel::Create(
      card, schema_lbl_rc, schema_text, FluxingLabel::FontSize::Medium);
  RECT schema_btn_rc = {card_w - 96, row3_top, card_w - 12,
                        row3_top + QP_BUTTON_H};
  QuickPanelDialog::SchemaButton() = FluxingButton::Create(
      card, schema_btn_rc, L"\xE5\x88\x87\xE6\x8D\xA2",
      FluxingButton::Style::Secondary);
  QuickPanelDialog::SchemaButton()->SetOnClick([]() {
    // spec 045 v0.18.29.0 placeholder: if we have a "previous"
    // schema id cached, cycle to next; otherwise pick first. This
    // will be replaced by a real popup list in spec 046.
    std::function<void(const std::wstring&)> cb =
        QuickPanelDialog::OnSelectSchema();
    if (cb) {
      cb(L"");  // empty -> signal "advance to next" (handler interprets)
    }
    FireAndClose([](){});
  });

  // Row 4: 用户文件夹 / 程序文件夹. Two buttons full-width split.
  LONG row4_top = 96;
  LONG half = (card_w - 24) / 2;
  RECT user_btn_rc = {12, row4_top, 12 + half, row4_top + QP_BUTTON_H};
  RECT prog_btn_rc = {12 + half + 8, row4_top, card_w - 12,
                      row4_top + QP_BUTTON_H};
  QuickPanelDialog::UserFolderButton() = FluxingButton::Create(
      card, user_btn_rc, L"\xF0\x9F\x93\x81 \xE7\x94\xA8\xE6\x88\xB7\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9",
      FluxingButton::Style::Secondary);
  QuickPanelDialog::UserFolderButton()->SetOnClick([]() {
    auto cb = QuickPanelDialog::OnOpenUserFolder();
    FireAndClose([cb]() { if (cb) cb(); });
  });
  QuickPanelDialog::ProgramFolderButton() = FluxingButton::Create(
      card, prog_btn_rc, L"\xF0\x9F\x93\x82 \xE7\xA8\x8B\xE5\xBA\x8F\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB9",
      FluxingButton::Style::Secondary);
  QuickPanelDialog::ProgramFolderButton()->SetOnClick([]() {
    auto cb = QuickPanelDialog::OnOpenProgramFolder();
    FireAndClose([cb]() { if (cb) cb(); });
  });

  // Row 5: 部署 (Primary, left half) / 退出 (Destructive, right half).
  LONG row5_top = 132;
  RECT deploy_btn_rc = {12, row5_top, 12 + half, row5_top + QP_BUTTON_H};
  RECT quit_btn_rc = {12 + half + 8, row5_top, card_w - 12,
                      row5_top + QP_BUTTON_H};
  QuickPanelDialog::DeployButton() = FluxingButton::Create(
      card, deploy_btn_rc, L"\xF0\x9F\x9A\x80 \xE9\x83\xA8\xE7\xBD\xB2",
      FluxingButton::Style::Primary);
  QuickPanelDialog::DeployButton()->SetOnClick([]() {
    auto cb = QuickPanelDialog::OnDeploy();
    FireAndClose([cb]() { if (cb) cb(); });
  });
  QuickPanelDialog::QuitButton() = FluxingButton::Create(
      card, quit_btn_rc, L"\xE2\x8F\xBB \xE9\x80\x80\xE5\x87\xBA",
      FluxingButton::Style::Destructive);
  QuickPanelDialog::QuitButton()->SetOnClick([]() {
    auto cb = QuickPanelDialog::OnQuit();
    FireAndClose([cb]() { if (cb) cb(); });
  });
  (void)schemas;  // reserved for spec 046 popup list
}

}  // namespace

void QuickPanelDialog::Show(bool currentAscii,
                             bool currentSimp,
                             bool currentFullwidth,
                             const std::wstring& currentSchema,
                             const std::vector<std::wstring>& availableSchemas,
                             std::function<void(bool)> onAsciiToggle,
                             std::function<void(bool)> onSimpToggle,
                             std::function<void(bool)> onFullwidthToggle,
                             std::function<void(const std::wstring&)> onSelectSchema,
                             std::function<void()> onOpenUserFolder,
                             std::function<void()> onOpenProgramFolder,
                             std::function<void()> onDeploy,
                             std::function<void()> onQuit) {
  // AP-036-H: if a panel is already visible, destroy it first so we do
  // not leak a window or have two panels competing for input.
  if (s_hwnd && IsWindow(s_hwnd)) {
    DestroyWindow(s_hwnd);
    s_hwnd = NULL;
  }
  // Tear down any leftover Fluxing controls from a prior Show().
  DestroyFluxingControls();

  s_onAsciiToggle = onAsciiToggle;
  s_onSimpToggle = onSimpToggle;
  s_onFullwidthToggle = onFullwidthToggle;
  s_onSelectSchema = onSelectSchema;
  s_onOpenUserFolder = onOpenUserFolder;
  s_onOpenProgramFolder = onOpenProgramFolder;
  s_onDeploy = onDeploy;
  s_onQuit = onQuit;
  s_currentAscii = currentAscii;
  s_currentSimp = currentSimp;
  s_currentFullwidth = currentFullwidth;

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
  CreateFluxingControls(s_hwnd, currentSchema, availableSchemas);
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
  s_onSimpToggle = nullptr;
  s_onFullwidthToggle = nullptr;
  s_onSelectSchema = nullptr;
  s_onOpenUserFolder = nullptr;
  s_onOpenProgramFolder = nullptr;
  s_onDeploy = nullptr;
  s_onQuit = nullptr;
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
  // spec 045: layout is built inside Show() via CreateFluxingControls.
  // Here we only add the native close X (the spec 036 IDCANCEL path
  // that TestQuickPanelDialog T1+T6 verify still works).
  RECT client_for_x;
  GetClientRect(hwnd, &client_for_x);
  CreateWindowExW(0, L"BUTTON", L"X",
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  client_for_x.right - 28, 6, 22, 22,
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
  s_onSimpToggle = nullptr;
  s_onFullwidthToggle = nullptr;
  s_onSelectSchema = nullptr;
  s_onOpenUserFolder = nullptr;
  s_onOpenProgramFolder = nullptr;
  s_onDeploy = nullptr;
  s_onQuit = nullptr;
  return 0;
}

LRESULT QuickPanelDialog::OnCommand(HWND, WPARAM wParam, LPARAM) {
  WORD id = LOWORD(wParam);
  // Only the native close (X) button routes through WM_COMMAND in
  // the spec 038+045 layout. All other buttons/toggles fire their
  // callbacks directly from their own WndProcs via
  // SetOnClick / SetOnChanged.
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