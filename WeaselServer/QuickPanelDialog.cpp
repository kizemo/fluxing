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
using Gdiplus::REAL;
using Gdiplus::LineCap;
using Gdiplus::LineJoin;
using Gdiplus::LineCapRound;
using Gdiplus::LineCapSquare;
using Gdiplus::LineJoinRound;
using Gdiplus::RectF;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientModeHorizontal;

#define QP_WIDTH         292
#define QP_HEIGHT         38
#define QP_TIMER_FADE      1
#define QP_FADE_STEP      34   // (255-179)/3 ≈ 25, fewer timer ticks
// spec 052 §1 + US052-A: "切到火流猩输入法 → 屏幕右下角自动出现 ...
// 20% 透明度（淡灰感）". 20% of 255 = 51. codex 0.18.30.0 wrote 179
// (70%) which violated the spec; spec 056 bugfix restores 51.
#define QP_ALPHA_DEFAULT  51   // 20% opaque (80% transparent) per spec 052
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
// spec 056 bugfix: layout rewritten to match design (04-quick-settings-v3-macos.html):
//   bar       14px radius
//   brand     26x26 logo with 8px radius and gradient (blue→purple)
//   button    32x28 with 7px radius, hover 4% black overlay
//   separator 1px wide, 18px tall, rgba(0,0,0,0.08)
// 6 icons: schema / dict / phrase / full-half / symbols / login
// (was using GDI+ DrawLine placeholders — spec 056 replaces with
//  GDIplus::GraphicsPath SVG-like paths per design)

RECT BrandRect()  { RECT r = {6, 6, 32, 32}; return r; }     // 26x26 brand
RECT Sep1Rect()   { RECT r = {36, 11, 37, 27}; return r; }
RECT Btn1Rect()   { RECT r = {40, 6, 72, 34}; return r; }    // 32x28 buttons
RECT Sep2Rect()   { RECT r = {76, 11, 77, 27}; return r; }
RECT Btn2Rect()   { RECT r = {80, 6, 112, 34}; return r; }
RECT Sep3Rect()   { RECT r = {116, 11, 117, 27}; return r; }
RECT Btn3Rect()   { RECT r = {120, 6, 152, 34}; return r; }
RECT Sep4Rect()   { RECT r = {156, 11, 157, 27}; return r; }
RECT Btn4Rect()   { RECT r = {160, 6, 192, 34}; return r; }
RECT Sep5Rect()   { RECT r = {196, 11, 197, 27}; return r; }
RECT Btn5Rect()   { RECT r = {200, 6, 232, 34}; return r; }
RECT Sep6Rect()   { RECT r = {236, 11, 237, 27}; return r; }
RECT Btn6Rect()   { RECT r = {240, 6, 272, 34}; return r; }

// Now total width = 278, fits QP_WIDTH=292 with 14px right padding.
// Update QP_WIDTH to match new layout:
#undef QP_WIDTH
#define QP_WIDTH  278
// Height: design is 38 (6 padding top + 26 brand + 6 padding bottom)
#undef QP_HEIGHT
#define QP_HEIGHT 38

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
  if (InRect(x, y, BrandRect())) return -1;
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

// spec 056 bugfix: DrawIcon rewritten with GDI+ GraphicsPath
// (SVG-style vector paths). Each icon is a 16x16 unit path centered
// at (cx, cy), matching SF Symbols stroke style (1.5 stroke width,
// round cap/join). Old code drew 3-5 lines as placeholders — that
// was just a "temp placeholder", not real design implementation.

// Build a SF-Symbols-style icon path. viewBox 0 0 20 20, drawn at
// (cx-8, cy-8) with width=16. Returns GraphicsPath sized 16x16.
static std::unique_ptr<GraphicsPath> MakeIconPath(int btnId) {
  auto path = std::make_unique<GraphicsPath>();
  const float s = 16.0f;
  // Map design viewBox 0..20 to 0..16 (offset drawn at cx-8)
  const float k = s / 20.0f;
  switch (btnId) {
    case 1: { // schema: 3 rounded rectangles (list.bullet.rectangle)
      for (int i = 0; i < 3; ++i) {
        float y = 3.0f + i * 5.25f;
        path->AddRectangle(RectF((REAL)3*k, (REAL)y*k, (REAL)14*k, (REAL)3.5f*k));
      }
      break;
    }
    case 2: { // dict: document with 3 lines (book.closed)
      path->AddRectangle(RectF((REAL)3*k, (REAL)4.5f*k, (REAL)14*k, (REAL)11*k));
      // inner horizontal lines
      path->AddLine((REAL)6*k, (REAL)7*k, (REAL)14*k, (REAL)7*k);
      path->AddLine((REAL)6*k, (REAL)10*k, (REAL)14*k, (REAL)10*k);
      path->AddLine((REAL)6*k, (REAL)13*k, (REAL)11*k, (REAL)13*k);
      break;
    }
    case 3: { // phrase: pencil shape
      path->AddLine((REAL)13.5f*k, (REAL)3.5f*k, (REAL)16.5f*k, (REAL)6.5f*k);
      path->AddLine((REAL)16.5f*k, (REAL)6.5f*k, (REAL)7*k, (REAL)16*k);
      path->AddLine((REAL)7*k, (REAL)16*k, (REAL)4*k, (REAL)16*k);
      path->AddLine((REAL)4*k, (REAL)16*k, (REAL)4*k, (REAL)13*k);
      path->AddLine((REAL)4*k, (REAL)13*k, (REAL)13.5f*k, (REAL)3.5f*k);
      path->CloseFigure();
      break;
    }
    case 4: { // full-half: filled dot (current is full; alternate via s_fullwidth)
      if (QuickPanelDialog::IsFullwidth()) {
        // large circle outline
        path->AddEllipse(4*k, 4*k, 12*k, 12*k);
      } else {
        // small filled dot
        path->AddEllipse(8*k, 8*k, 4*k, 4*k);
      }
      break;
    }
    case 5: { // symbols: keyboard grid (3x3 dots + bottom bar)
      for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 5; ++c) {
          float cx = (3 + c*3.5f)*k;
          float cy = (5 + r*2.5f)*k;
          path->AddEllipse(cx - 0.5f*k, cy - 0.5f*k, 1*k, 1*k);
        }
      }
      path->AddLine(5*k, 14*k, 15*k, 14*k);
      break;
    }
    case 6: { // login: person silhouette (head + body)
      // head circle
      path->AddEllipse(RectF((REAL)7*k, (REAL)3.5f*k, (REAL)6*k, (REAL)6*k));
      // body rectangle
      path->AddRectangle(RectF((REAL)3*k, (REAL)11*k, (REAL)14*k, (REAL)6*k));
      break;
    }
  }
  return path;
}

void DrawIcon(Graphics* g, int btnId, bool isPressed, bool isHover, int cx, int cy) {
  // Color: design uses #1d1d1f (28,28,31) text color
  Gdiplus::Color fgColor(0xFF, 0x1d, 0x1d, 0x1f);
  if (isPressed) fgColor = Gdiplus::Color(0xFF, 0x0a, 0x84, 0xff);  // accent
  // dim login (btn 6) - design uses 35% opacity
  if (btnId == 6) fgColor = Gdiplus::Color(0x59, 0x60, 0x60, 0x67);

  // Translate path to (cx-8, cy-8)
  Gdiplus::Matrix m;
  m.Translate((REAL)(cx - 8), (REAL)(cy - 8));
  auto path = MakeIconPath(btnId);
  if (!path) return;
  path->Transform(&m);

  if (isPressed) {
    // accent background (rgba(10,132,255,0.12))
    SolidBrush accent(Gdiplus::Color(0x1f, 0x0a, 0x84, 0xff));
    g->FillRectangle(&accent, cx - 16, cy - 14, 32, 28);
  } else if (isHover) {
    // hover background (rgba(0,0,0,0.04))
    SolidBrush hover(Gdiplus::Color(0x0a, 0x00, 0x00, 0x00));
    g->FillRectangle(&hover, cx - 16, cy - 14, 32, 28);
  }

  if (btnId == 6) {
    // Login is disabled (design aria-disabled). Fill with dim color.
    SolidBrush dim(fgColor);
    g->FillPath(&dim, path.get());
  } else {
    // Stroke only (SF Symbols outline style)
    Gdiplus::Pen pen(fgColor, 1.5f);
    pen.SetLineCap((Gdiplus::LineCap)Gdiplus::LineCapRound,
                   (Gdiplus::LineCap)Gdiplus::LineCapRound,
                   (Gdiplus::DashCap)Gdiplus::LineCapRound);
    pen.SetLineJoin((Gdiplus::LineJoin)Gdiplus::LineJoinRound);
    g->DrawPath(&pen, path.get());
  }
}

// spec 056 bugfix: DoPaint rewritten to match design (04-quick-settings-v3-macos.html):
//   background: rgba(246,246,246,0.72) translucent (we use alpha 184 = 0.72*255)
//   border:     1px solid rgba(0,0,0,0.08)
//   shadow:     0 8px 24px rgba(0,0,0,0.10)
//   radius:     14px outer (bar), 8px brand, 7px buttons
//   brand gradient: linear-gradient(135deg, #0a84ff 0%, #5e5ce6 100%)

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

  // 1. Background fill (translucent pale grey, design surface)
  SolidBrush bg(Gdiplus::Color(0xB8, 0xF6, 0xF6, 0xF6));
  g.FillRectangle(&bg, 0, 0, crc.right, crc.bottom);

  // 2. Outer bar (14px radius, design color)
  Gdiplus::RectF bar_rc(0.5f, 0.5f, (float)crc.right - 1, (float)crc.bottom - 1);
  DrawRoundRect(&g, bar_rc, 14.0f,
                Gdiplus::Color(0xB8, 0xF6, 0xF6, 0xF6),
                Gdiplus::Color(0x14, 0x00, 0x00, 0x00));   // rgba(0,0,0,0.08) border

  // 3. Brand (26x26 logo with gradient blue→purple)
  RECT br = BrandRect();
  Gdiplus::RectF brand_rc((float)br.left, (float)br.top,
                         (float)(br.right - br.left), (float)(br.bottom - br.top));
  // Gradient fill: blue (#0a84ff) to purple (#5e5ce6)
  Gdiplus::LinearGradientBrush gradient(
      brand_rc,
      Gdiplus::Color(0xFF, 0x0a, 0x84, 0xff),    // top-left blue
      Gdiplus::Color(0xFF, 0x5e, 0x5c, 0xe6),    // bottom-right purple
      Gdiplus::LinearGradientModeHorizontal);
  GraphicsPath brand_path;
  brand_path.AddLine(brand_rc.X + 8, brand_rc.Y, brand_rc.GetRight() - 8, brand_rc.Y);
  brand_path.AddArc((REAL)brand_rc.GetRight() - 16, (REAL)brand_rc.Y, (REAL)16, (REAL)16, (REAL)270, (REAL)90);
  brand_path.AddLine(brand_rc.GetRight(), brand_rc.Y + 8, brand_rc.GetRight(), brand_rc.GetBottom() - 8);
  brand_path.AddArc((REAL)brand_rc.GetRight() - 16, (REAL)brand_rc.GetBottom() - 16, (REAL)16, (REAL)16, (REAL)0, (REAL)90);
  brand_path.AddLine(brand_rc.GetRight() - 8, brand_rc.GetBottom(), brand_rc.X + 8, brand_rc.GetBottom());
  brand_path.AddArc((REAL)brand_rc.X, (REAL)brand_rc.GetBottom() - 16, (REAL)16, (REAL)16, (REAL)90, (REAL)90);
  brand_path.AddLine(brand_rc.X, brand_rc.Y + 8, brand_rc.X, brand_rc.Y + 8);
  brand_path.AddArc((REAL)brand_rc.X, (REAL)brand_rc.Y, (REAL)16, (REAL)16, (REAL)180, (REAL)90);
  brand_path.CloseFigure();
  g.FillPath(&gradient, &brand_path);
  if (s_logo) {
    // spec 061: logo now fills the entire brand area to eliminate
    // the "blue border" effect (blue gradient bleeding through
    // the logo's transparent margin). The 700x700 logo scales down
    // to drawW x drawH = 26x26, matching the brand rect exactly.
    int drawW = 26, drawH = 26;
    int ox = br.left;
    int oy = br.top;
    g.DrawImage(s_logo.get(), ox, oy, drawW, drawH);
  }

  // 4. Separators (1px, very light, design rgba(0,0,0,0.08))
  auto drawSep = [&](RECT sr) {
    Gdiplus::Pen p(Gdiplus::Color(0x14, 0x00, 0x00, 0x00), 1.0f);
    g.DrawLine(&p, sr.left, sr.top, sr.left, sr.bottom);
  };
  drawSep(Sep1Rect()); drawSep(Sep2Rect()); drawSep(Sep3Rect());
  drawSep(Sep4Rect()); drawSep(Sep5Rect()); drawSep(Sep6Rect());

  // 5. Icons (6 SF-Symbols-style paths via GDI+ GraphicsPath)
  // Track hover state: button highlighted if mouse is inside its rect.
  // For spec 056, we approximate hover via mouse position from last WM_MOUSEMOVE.
  POINT mousePt;
  GetCursorPos(&mousePt);
  ScreenToClient(hwnd, &mousePt);
  for (int i = 1; i <= 6; ++i) {
    RECT btnr = (i==1 ? Btn1Rect():i==2?Btn2Rect():i==3?Btn3Rect():i==4?Btn4Rect():i==5?Btn5Rect():Btn6Rect());
    int cx = (btnr.left + btnr.right) / 2;
    int cy = (btnr.top + btnr.bottom) / 2;
    bool isHover = InRect(mousePt.x, mousePt.y, btnr);
    // isPressed: tracked via LButtonDown state. For simplicity we
    // approximate via button #4 state (fullwidth toggle).
    bool isPressed = (i == 4 && QuickPanelDialog::IsFullwidth());
    DrawIcon(&g, i, isPressed, isHover, cx, cy);
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
    // Panel already visible - update callbacks only, don't hide
    s_onSchema    = onSchema;
    s_onUserFolder = onUserFolder;
    s_onPhrases   = onPhrases;
    s_onFullwidth = onFullwidth;
    s_onSymbols   = onSymbols;
    s_onLogin     = onLogin;
    // spec 061: the buttons in the panel were created during the
    // initial Show() call - they each hold a lambda that captures
    // `s_onSchema` etc by REFERENCE. Setting the new lambda here
    // is enough; the next click will fire the new callback. No
    // recreate-window needed (which would cause a visual flicker).
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
    // Re-show in always-show mode. spec 061: re-show using the LAST
    // stored callbacks. WeaselServerApp.cpp::SetupMenuHandlers calls
    // Show() right before triggering ToggleMode, which stored the
    // 6 callbacks in s_onSchema..s_onLogin. If ToggleMode is called
    // from somewhere else (e.g. Alt+, handler) the stored callbacks
    // may still be the ones from the most recent explicit Show().
    EnableAlwaysShowMode(s_onSchema, s_onUserFolder, s_onPhrases,
                        s_onFullwidth, s_onSymbols, s_onLogin);
  } else {
    // Hide the panel
    Hide();
  }
}

void QuickPanelDialog::EnableAlwaysShowMode(
    OnClick onSchema,
    OnClick onUserFolder,
    OnClick onPhrases,
    OnToggle onFullwidth,
    OnClick onSymbols,
    OnClick onLogin) {
  if (s_hwnd && IsWindow(s_hwnd)) {
    // Already visible - just ensure it's in always-show mode and
    // refresh the callbacks (the user may have changed them via a
    // subsequent Show() call).
    s_mode = Mode::kAlwaysShow;
    s_onSchema    = onSchema;
    s_onUserFolder = onUserFolder;
    s_onPhrases   = onPhrases;
    s_onFullwidth = onFullwidth;
    s_onSymbols   = onSymbols;
    s_onLogin     = onLogin;
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    StartFadeTo(s_hwnd, QP_ALPHA_DEFAULT);
    return;
  }

  // Create a minimal panel in always-show mode. spec 061: the
  // callbacks are now passed in (not null) so button clicks work
  // immediately after panel creation.
  s_fullwidth   = false;  // default state (can be updated by next Show call)
  s_mode        = Mode::kAlwaysShow;
  s_onSchema    = onSchema;
  s_onUserFolder = onUserFolder;
  s_onPhrases   = onPhrases;
  s_onFullwidth = onFullwidth;
  s_onSymbols   = onSymbols;
  s_onLogin     = onLogin;

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