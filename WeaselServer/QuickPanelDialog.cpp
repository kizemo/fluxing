// spec 049 + spec 052: QuickPanelDialog v4 macOS-style toolbar
// + always-show mode (20% alpha=51, hover 255, Alt+, toggle)
// Horizontal bar: brand logo + 6 icon buttons + separators
// GDI+ rendering, no D2D dependency.
// Buttons: 1=方案 2=词典 3=短语 4=全半角 5=符号 6=登录(占位)
#include "stdafx.h"
#include <memory>
#include <GdiPlus.h>
#include "QuickPanelDialog.h"
#include "resource.h"
#pragma comment(lib, "gdiplus.lib")

using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;
using Gdiplus::GraphicsPath;
using Gdiplus::ImageAttributes;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::TextRenderingHintAntiAlias;
using Gdiplus::UnitPixel;

#define QP_WIDTH         292
#define QP_HEIGHT         38
#define QP_TIMER_FADE      1
#define QP_FADE_STEP      34   // (255-179)/3 ≈ 25, fewer timer ticks
#define QP_ALPHA_DEFAULT 179   // 70% opaque (30% transparent) per spec 052 user feedback
#define QP_ALPHA_HOVER   255   // fully opaque

static const wchar_t kClassName[] = L"FluxingQuickPanel_v4";

// ─── Static state ─────────────────────────────────────────────────────

HWND                    QuickPanelDialog::s_hwnd         = NULL;
QuickPanelDialog::Mode  QuickPanelDialog::s_mode         = QuickPanelDialog::Mode::kHidden;
bool                    QuickPanelDialog::s_fullwidth    = false;
int                     QuickPanelDialog::s_alpha        = QP_ALPHA_DEFAULT;
int                     QuickPanelDialog::s_targetAlpha  = QP_ALPHA_DEFAULT;
bool                    QuickPanelDialog::s_mouseTracked = false;

static HANDLE s_hFadeTimer = NULL;

QuickPanelDialog::OnClick  QuickPanelDialog::s_onSchema;
QuickPanelDialog::OnClick  QuickPanelDialog::s_onUserFolder;
QuickPanelDialog::OnClick  QuickPanelDialog::s_onPhrases;
QuickPanelDialog::OnToggle QuickPanelDialog::s_onFullwidth;
QuickPanelDialog::OnClick  QuickPanelDialog::s_onSymbols;
QuickPanelDialog::OnClick  QuickPanelDialog::s_onLogin;

static std::unique_ptr<Image> s_logo;

// ─── Logo resource loading ────────────────────────────────────────────

namespace {

void LoadLogo() {
  if (s_logo) return;
  HRSRC hrsrc = FindResourceW(NULL, MAKEINTRESOURCEW(IDR_FLUXING_LOGO), RT_RCDATA);
  if (!hrsrc) return;
  HGLOBAL hglob = LoadResource(NULL, hrsrc);
  if (!hglob) return;
  const void* data = LockResource(hglob);
  DWORD size = SizeofResource(NULL, hrsrc);
  if (!data || size == 0) return;
  IStream* stream = NULL;
  CreateStreamOnHGlobal(NULL, TRUE, &stream);
  if (!stream) return;
  ULONG written = 0;
  stream->Write(data, size, &written);
  LARGE_INTEGER zero = {0};
  stream->Seek(zero, STREAM_SEEK_SET, NULL);
  s_logo.reset(new Bitmap(stream));
  stream->Release();
}

BOOL RegisterClassOnce(HINSTANCE hInst) {
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = QuickPanelDialog::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = kClassName;
  if (GetClassInfoExW(hInst, kClassName, &wc)) return TRUE;
  return RegisterClassExW(&wc) != 0;
}

POINT ComputeOrigin() {
  RECT work;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  POINT p;
  p.x = work.right - QP_WIDTH - 12;
  p.y = work.bottom - QP_HEIGHT - 12;
  return p;
}

// ─── Layout ────────────────────────────────────────────────────────────

RECT LogoRect()  { RECT r = {6, 5, 32, 31}; return r; }
RECT Sep1Rect()  { RECT r = {38, 9, 40, 27}; return r; }
RECT Btn1Rect()  { RECT r = {46, 4, 78, 32}; return r; }
RECT Sep2Rect()  { RECT r = {84, 9, 86, 27}; return r; }
RECT Btn2Rect()  { RECT r = {92, 4, 124, 32}; return r; }
RECT Sep3Rect()  { RECT r = {130, 9, 132, 27}; return r; }
RECT Btn3Rect()  { RECT r = {138, 4, 170, 32}; return r; }
RECT Sep4Rect()  { RECT r = {176, 9, 178, 27}; return r; }
RECT Btn4Rect()  { RECT r = {184, 4, 216, 32}; return r; }
RECT Sep5Rect()  { RECT r = {222, 9, 224, 27}; return r; }
RECT Btn5Rect()  { RECT r = {230, 4, 262, 32}; return r; }
RECT Sep6Rect()  { RECT r = {268, 9, 270, 27}; return r; }
RECT Btn6Rect()  { RECT r = {276, 4, 286, 32}; return r; }

BOOL InRect(int x, int y, RECT r) {
  return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

int HitTest(int x, int y) {
  if (InRect(x, y, Btn1Rect())) return 1;
  if (InRect(x, y, Btn2Rect())) return 2;
  if (InRect(x, y, Btn3Rect())) return 3;
  if (InRect(x, y, Btn4Rect())) return 4;
  if (InRect(x, y, Btn5Rect())) return 5;
  if (InRect(x, y, Btn6Rect())) return 6;
  if (InRect(x, y, LogoRect())) return -1;
  return 0;
}

void InvalidatePanel(HWND hwnd) {
  RECT rc = {0, 0, QP_WIDTH, QP_HEIGHT};
  InvalidateRect(hwnd, &rc, FALSE);
}

// ─── Fade animation ────────────────────────────────────────────────────

void StopFadeTimer() {
  if (s_hFadeTimer) {
    DeleteTimerQueueTimer(NULL, s_hFadeTimer, INVALID_HANDLE_VALUE);
    s_hFadeTimer = NULL;
  }
}

VOID CALLBACK FadeTimerProc(PVOID, BOOLEAN) {
  HWND hwnd = QuickPanelDialog::s_hwnd;
  if (!hwnd || !IsWindow(hwnd)) { StopFadeTimer(); return; }
  int diff = QuickPanelDialog::s_targetAlpha - QuickPanelDialog::s_alpha;
  if (diff == 0) { StopFadeTimer(); return; }
  int step = diff > 0 ? QP_FADE_STEP : -QP_FADE_STEP;
  QuickPanelDialog::s_alpha += step;
  if ((step > 0 && QuickPanelDialog::s_alpha > QuickPanelDialog::s_targetAlpha) ||
      (step < 0 && QuickPanelDialog::s_alpha < QuickPanelDialog::s_targetAlpha)) {
    QuickPanelDialog::s_alpha = QuickPanelDialog::s_targetAlpha;
  }
  SetLayeredWindowAttributes(hwnd, 0, (BYTE)QuickPanelDialog::s_alpha, LWA_ALPHA);
}

void StartFadeTo(HWND hwnd, int target) {
  QuickPanelDialog::s_targetAlpha = target;
  if (QuickPanelDialog::s_alpha == target) { StopFadeTimer(); return; }
  StopFadeTimer();
  CreateTimerQueueTimer(&s_hFadeTimer, NULL, FadeTimerProc, NULL,
                        50, 50, WT_EXECUTEDEFAULT);
}

// ─── Button fire ───────────────────────────────────────────────────────

void FireButton(int id) {
  // spec 052: in always-show mode, buttons fire but the panel stays visible
  // (no auto-close). Callbacks are fired, panel remains in always-show mode.
  switch (id) {
    case 1: if (QuickPanelDialog::s_onSchema)     QuickPanelDialog::s_onSchema();     break;
    case 2: if (QuickPanelDialog::s_onUserFolder)  QuickPanelDialog::s_onUserFolder();  break;
    case 3: if (QuickPanelDialog::s_onPhrases)    QuickPanelDialog::s_onPhrases();    break;
    case 4: if (QuickPanelDialog::s_onFullwidth)  QuickPanelDialog::s_onFullwidth(!QuickPanelDialog::s_fullwidth); break;
    case 5: if (QuickPanelDialog::s_onSymbols)    QuickPanelDialog::s_onSymbols();    break;
    case 6: if (QuickPanelDialog::s_onLogin)       QuickPanelDialog::s_onLogin();       break;
  }
}

// ─── GDI+ Drawing ──────────────────────────────────────────────────────

void DrawRoundRect(Graphics* g, const Gdiplus::RectF& rc, float r,
                   const Gdiplus::Color& fill, const Gdiplus::Color& border,
                   float borderW = 0.5f) {
  GraphicsPath path;
  path.AddLine(rc.X + r, rc.Y, rc.GetRight() - r, rc.Y);
  path.AddArc(rc.GetRight() - 2*r, rc.Y, 2*r, 2*r, 270, 90);
  path.AddLine(rc.GetRight(), rc.Y + r, rc.GetRight(), rc.GetBottom() - r);
  path.AddArc(rc.GetRight() - 2*r, rc.GetBottom() - 2*r, 2*r, 2*r, 0, 90);
  path.AddLine(rc.GetRight() - r, rc.GetBottom(), rc.X + r, rc.GetBottom());
  path.AddArc(rc.X, rc.GetBottom() - 2*r, 2*r, 2*r, 90, 90);
  path.AddLine(rc.X, rc.GetBottom() - r, rc.X, rc.Y + r);
  path.AddArc(rc.X, rc.Y, 2*r, 2*r, 180, 90);
  path.CloseFigure();
  if (fill.GetAlpha() > 0) {
    SolidBrush br(fill);
    g->FillPath(&br, &path);
  }
  if (border.GetAlpha() > 0 && borderW > 0) {
    Gdiplus::Pen pen(border, borderW);
    g->DrawPath(&pen, &path);
  }
}

void DrawIcon(Graphics* g, int btnId, bool isPressed, int cx, int cy) {
  Gdiplus::Color fill(0x1d, 0x1d, 0x1f);
  if (isPressed) fill.SetValue(0xFF0a84ff);
  Gdiplus::Pen pen(fill, 1.5f);
  Gdiplus::SolidBrush br(fill);
  int s = 10;
  // Btn 1: 方案 → 3 horizontal lines
  if (btnId == 1) {
    g->DrawLine(&pen, (INT)(cx-s), (INT)(cy-s+2), (INT)(cx+s), (INT)(cy-s+2));
    g->DrawLine(&pen, (INT)(cx-s), (INT)cy, (INT)(cx+s), (INT)cy);
    g->DrawLine(&pen, (INT)(cx-s), (INT)(cy+s-2), (INT)(cx+s), (INT)(cy+s-2));
  }
  // Btn 2: 词典 → document with lines
  else if (btnId == 2) {
    Gdiplus::RectF doc(cx-s*0.6f, cy-s*0.7f, s*1.2f, s*1.4f);
    g->DrawRectangle(&pen, doc);
    g->DrawLine(&pen, (INT)(cx-s*0.4f), (INT)(cy-s*0.3f), (INT)(cx+s*0.4f), (INT)(cy-s*0.3f));
    g->DrawLine(&pen, (INT)(cx-s*0.4f), (INT)cy, (INT)(cx+s*0.2f), (INT)cy);
    g->DrawLine(&pen, (INT)(cx-s*0.4f), (INT)(cy+s*0.3f), (INT)(cx+s*0.1f), (INT)(cy+s*0.3f));
  }
  // Btn 3: 短语 → pencil shape
  else if (btnId == 3) {
    g->DrawLine(&pen, (INT)(cx-s), (INT)(cy+s), (INT)cx, (INT)(cy+s));
    g->DrawLine(&pen, (INT)cx, (INT)(cy+s), (INT)(cx+s), (INT)cy);
    g->DrawLine(&pen, (INT)(cx+s), (INT)cy, (INT)(cx+s), (INT)(cy-s));
  }
  // Btn 4: 全半角 → large circle (full) or small filled dot (half)
  else if (btnId == 4) {
    if (QuickPanelDialog::IsFullwidth()) {
      g->DrawEllipse(&pen, cx-s, cy-s, s*2, s*2);
    } else {
      g->FillEllipse(&br, cx-2, cy-2, 4, 4);
    }
  }
  // Btn 5: 符号 → keyboard grid
  else if (btnId == 5) {
    float k = 3.5f, gap = 1.5f;
    for (int row = -1; row <= 1; ++row) {
      for (int col = -1; col <= 1; ++col) {
        g->FillRectangle(&br, cx + col*(k+gap) - k/2,
                              cy + row*(k+gap) - k/2, k, k);
      }
    }
  }
  // Btn 6: 登录 → person silhouette (dimmed / disabled look)
  else if (btnId == 6) {
    g->FillEllipse(&br, cx-3, (INT)(cy-s*0.6f), 6, 6);
    Gdiplus::RectF body(cx-s*0.7f, cy+s*0.1f, s*1.4f, s*0.8f);
    g->FillEllipse(&br, body);
  }
}

void DoPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT crc; GetClientRect(hwnd, &crc);
  HDC memDC = CreateCompatibleDC(hdc);
  HBITMAP memBM = CreateCompatibleBitmap(hdc, crc.right, crc.bottom);
  SelectObject(memDC, memBM);

  Graphics g(memDC);
  g.SetSmoothingMode(SmoothingModeAntiAlias);
  g.SetTextRenderingHint(TextRenderingHintAntiAlias);

  // spec 055 bugfix: Background was SolidBrush alpha=0xF0 (opaque pale
  // grey) — user reported the panel looked like a "dark ugly border".
  // The dark border came from DrawRoundRect's second alpha=0x14 color
  // (8% black). Real design intent (spec 049 v3-macos): translucent
  // background, no visible border.
  //
  // Fix:
  //   1. SolidBrush background uses very light alpha=0xE8 (91%) so the
  //      desktop subtly shows through, matching macOS Big Sur+ chrome.
  //   2. Drop the explicit border call from DrawRoundRect (pass
  //      alpha=0 border).
  //   3. Soft 1px hairline below the bar (subtle, not the dark frame).
  SolidBrush bg(Gdiplus::Color(0xE8, 0xF6, 0xF6));
  g.FillRectangle(&bg, 0, 0, crc.right, crc.bottom);

  // Outer rounded rect (bar container) — NO dark border.
  Gdiplus::RectF bar_rc(1.0f, 1.0f, (float)crc.right - 2, (float)crc.bottom - 2);
  DrawRoundRect(&g, bar_rc, 14.0f,
                Gdiplus::Color(0xE8, 0xF6, 0xF6),
                Gdiplus::Color(0x00, 0x00, 0x00, 0x00));   // transparent border

  // Soft hairline shadow below the bar (1px line, very light).
  {
    Gdiplus::Pen hp(Gdiplus::Color(0x18, 0x00, 0x00, 0x00), 1.0f);
    g.DrawLine(&hp, (INT)14, (INT)((float)crc.bottom - 0.5f),
                   (INT)(crc.right - 14), (INT)((float)crc.bottom - 0.5f));
  }

  // Logo: gradient rounded rect + logo image
  RECT lr = LogoRect();
  Gdiplus::RectF logo_rc((float)lr.left, (float)lr.top,
                          (float)(lr.right - lr.left), (float)(lr.bottom - lr.top));
  DrawRoundRect(&g, logo_rc, 8.0f,
                Gdiplus::Color(0xE6, 0x5E, 0xFF),
                Gdiplus::Color(0x30, 0xFF, 0xFF, 0xFF), 0.5f);
  if (s_logo) {
    int imgW = s_logo->GetWidth();
    int imgH = s_logo->GetHeight();
    int drawW = 18, drawH = 18;
    int ox = lr.left + (lr.right - lr.left - drawW) / 2;
    int oy = lr.top  + (lr.bottom - lr.top - drawH) / 2;
    // Draw logo as-is (no color tint - will appear in original colors on gradient)
    g.DrawImage(s_logo.get(), (int)(lr.left + (lr.right - lr.left - drawW) / 2),
                (int)(lr.top + (lr.bottom - lr.top - drawH) / 2),
                drawW, drawH);
  }

  // Separators (spec 055 bugfix: was 0x14,0x14,0x14 - very dark
  // vertical hairlines that contributed to the "dark ugly border"
  // user feedback. Use much lighter alpha=0x10 (6%) instead.)
  auto drawSep = [&](RECT sr) {
    Gdiplus::Pen p(Gdiplus::Color(0x10, 0x80, 0x80, 0x80), 1.0f);
    g.DrawLine(&p, sr.left, sr.top, sr.left, sr.bottom);
  };
  drawSep(Sep1Rect()); drawSep(Sep2Rect()); drawSep(Sep3Rect());
  drawSep(Sep4Rect()); drawSep(Sep5Rect()); drawSep(Sep6Rect());

  // Buttons
  for (int i = 1; i <= 6; ++i) {
    RECT br = (i==1 ? Btn1Rect():i==2?Btn2Rect():i==3?Btn3Rect():i==4?Btn4Rect():i==5?Btn5Rect():Btn6Rect());
    int cx = (br.left + br.right) / 2;
    int cy = (br.top + br.bottom) / 2;
    DrawIcon(&g, i, false, cx, cy);
  }

  BitBlt(hdc, 0, 0, crc.right, crc.bottom, memDC, 0, 0, SRCCOPY);
  DeleteObject(memBM);
  DeleteDC(memDC);
  EndPaint(hwnd, &ps);
}

} // anonymous namespace

// ─── Public API ────────────────────────────────────────────────────────

void QuickPanelDialog::Show(bool currentFullwidth,
                             OnClick onSchema,
                             OnClick onUserFolder,
                             OnClick onPhrases,
                             OnToggle onFullwidth,
                             OnClick onSymbols,
                             OnClick onLogin) {
  // spec 052: Show() is called for Alt+, / tray click.
  // If the panel is already visible in always-show mode, just refresh
  // callbacks and keep showing (do NOT toggle).
  if (s_hwnd && IsWindow(s_hwnd)) {
    // Panel already visible - update callbacks only, don''t hide
    s_onSchema    = onSchema;
    s_onUserFolder = onUserFolder;
    s_onPhrases   = onPhrases;
    s_onFullwidth = onFullwidth;
    s_onSymbols   = onSymbols;
    s_onLogin     = onLogin;
    return;
  }

  // Clean up any stale state
  Hide();

  s_onSchema    = onSchema;
  s_onUserFolder = onUserFolder;
  s_onPhrases   = onPhrases;
  s_onFullwidth = onFullwidth;
  s_onSymbols   = onSymbols;
  s_onLogin     = onLogin;
  s_fullwidth   = currentFullwidth;
  s_mode        = Mode::kAlwaysShow;

  LoadLogo();

  HINSTANCE hInst = GetModuleHandle(NULL);
  if (!RegisterClassOnce(hInst)) return;

  POINT origin = ComputeOrigin();
  s_hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      kClassName, L"Fluxing",
      WS_POPUP,
      origin.x, origin.y, QP_WIDTH, QP_HEIGHT,
      NULL, NULL, hInst, NULL);
  if (!s_hwnd) return;

  // Initial opacity: always-show 20% (alpha=51)
  s_alpha = QP_ALPHA_DEFAULT;
  s_targetAlpha = QP_ALPHA_DEFAULT;
  SetLayeredWindowAttributes(s_hwnd, 0, (BYTE)QP_ALPHA_DEFAULT, LWA_ALPHA);

  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  InvalidatePanel(s_hwnd);
}

void QuickPanelDialog::Hide() {
  StopFadeTimer();
  if (s_hwnd && IsWindow(s_hwnd)) DestroyWindow(s_hwnd);
  s_hwnd = NULL;
  s_mode = Mode::kHidden;
  s_onSchema    = nullptr;
  s_onUserFolder = nullptr;
  s_onPhrases   = nullptr;
  s_onFullwidth = nullptr;
  s_onSymbols   = nullptr;
  s_onLogin     = nullptr;
}

void QuickPanelDialog::ToggleMode() {
  if (s_mode == Mode::kHidden) {
    // Re-show in always-show mode
    // Use the stored fullwidth state and empty callbacks
    // (the actual callbacks will be set by the next Show/EnableAlwaysShowMode call)
    EnableAlwaysShowMode();
  } else {
    // Hide the panel
    Hide();
  }
}

void QuickPanelDialog::EnableAlwaysShowMode() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    // Already visible, just ensure it''s in always-show mode
    s_mode = Mode::kAlwaysShow;
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    StartFadeTo(s_hwnd, QP_ALPHA_DEFAULT);
    return;
  }

  // Create a minimal panel in always-show mode with empty callbacks.
  // The actual callbacks will be set by the next explicit Show() call.
  s_fullwidth = false;  // default state
  s_mode = Mode::kAlwaysShow;

  LoadLogo();

  HINSTANCE hInst = GetModuleHandle(NULL);
  if (!RegisterClassOnce(hInst)) return;

  POINT origin = ComputeOrigin();
  s_hwnd = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      kClassName, L"Fluxing",
      WS_POPUP,
      origin.x, origin.y, QP_WIDTH, QP_HEIGHT,
      NULL, NULL, hInst, NULL);
  if (!s_hwnd) return;

  s_alpha = QP_ALPHA_DEFAULT;
  s_targetAlpha = QP_ALPHA_DEFAULT;
  SetLayeredWindowAttributes(s_hwnd, 0, (BYTE)QP_ALPHA_DEFAULT, LWA_ALPHA);

  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  InvalidatePanel(s_hwnd);
}

// ─── WndProc ───────────────────────────────────────────────────────────

LRESULT CALLBACK QuickPanelDialog::WndProc(HWND hwnd, UINT msg,
                                            WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_CREATE:    return OnCreate(hwnd);
    case WM_DESTROY:   return OnDestroy(hwnd);
    case WM_PAINT:     return OnPaint(hwnd);
    case WM_LBUTTONUP: { POINT p = {LOWORD(l), HIWORD(l)}; OnLButtonUp(hwnd, p.x, p.y); return 0; }
    case WM_MOUSEMOVE: OnMouseMove(hwnd); return 0;
    case WM_MOUSELEAVE: OnMouseLeave(hwnd); return 0;
    case WM_TIMER:     return OnTimer(hwnd, w);
    case WM_KEYDOWN:
      if (w == VK_ESCAPE) {
        // spec 052: ESC in always-show mode hides the panel
        QuickPanelDialog::Hide();
        return 0;
      }
      break;
  }
  return DefWindowProc(hwnd, msg, w, l);
}

LRESULT QuickPanelDialog::OnCreate(HWND) { return 0; }

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  KillTimer(hwnd, QP_TIMER_FADE);
  StopFadeTimer();
  s_mouseTracked = false;
  if (s_hwnd == hwnd) s_hwnd = NULL;
  s_logo.reset();
  return 0;
}

LRESULT QuickPanelDialog::OnPaint(HWND hwnd) {
  DoPaint(hwnd);
  return 0;
}

LRESULT QuickPanelDialog::OnLButtonUp(HWND hwnd, int x, int y) {
  int id = HitTest(x, y);
  if (id > 0) FireButton(id);
  return 0;
}

void QuickPanelDialog::OnMouseMove(HWND hwnd) {
  if (!s_mouseTracked) {
    TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
    TrackMouseEvent(&tme);
    s_mouseTracked = true;
  }
  StartFadeTo(hwnd, QP_ALPHA_HOVER);
}

void QuickPanelDialog::OnMouseLeave(HWND hwnd) {
  s_mouseTracked = false;
  // spec 052: in always-show mode, fade back to 20% (no auto-hide)
  StartFadeTo(hwnd, QP_ALPHA_DEFAULT);
}

LRESULT QuickPanelDialog::OnTimer(HWND hwnd, WPARAM w) {
  // spec 052: removed QP_TIMER_AUTOHIDE; panel stays visible in always-show mode
  return 0;
}