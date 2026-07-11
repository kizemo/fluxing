// QuickPanelDialog v3-rev3 — PURE GDI implementation (spec 074 L75)
//
// 设计: docs/design/quickpanel-v3/index.html
// 历史雷区(plan.md 强制):
//   ❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)
//   ❌ 绝对不 CreateStreamOnHGlobal + Release() (L67 根因)
//   ❌ 绝对不用 D2D ID2D1HwndRenderTarget (L74 黑 panel,DComp 未 promote)
//
// 本实现纯 Win32 GDI:
//   - LoadImageW 直接从 PNG 文件创建 HBITMAP(无 GDI+ 路径)
//   - 全部 painting 用 HDC + SelectObject(无 D2D、无 GDI+)
//   - 5 个图标用 MoveTo/LineTo/Ellipse/Rectangle 画
//   - WS_EX_LAYERED + SetLayeredWindowAttributes(LWA_ALPHA, 220)
//     86% uniform translucency(per-pixel alpha 留给 v0.19.0.8+ UpdateLayeredWindow)
//
#include "stdafx.h"
#include <functional>
#include "QuickPanelDialog.h"

static const wchar_t kWindowClassName[] = L"FluxingQuickPanel_v3";

// 颜色常量(在文件作用域,本地作用域,kIcoDimC 等)
constexpr COLORREF kIcoDimC   = RGB(60, 60, 67);     // 灰
constexpr COLORREF kAccentC  = RGB(255, 95, 49);    // 品牌橙
constexpr COLORREF kAccent2C = RGB(155, 81, 224);  // 品牌紫
constexpr COLORREF kWhiteC   = RGB(255, 255, 255); // 白色(active icon)

void QuickPanelDialog::DrawIconSchema(HDC hdc, int x, int y) {
  // 双向切换箭头(viewBox 24x24,scale 到 30x30)
  // 上箭头 →: (4,9)→(17,9) + 箭头头
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC));
  MoveToEx(hdc, x + 5, y + 11, nullptr);
  LineTo(hdc, x + 23, y + 11);
  MoveToEx(hdc, x + 19, y + 8, nullptr);
  LineTo(hdc, x + 23, y + 11);
  LineTo(hdc, x + 19, y + 14);
  // 下箭头 ←: (20,15)→(7,15) + 箭头头
  MoveToEx(hdc, x + 25, y + 19, nullptr);
  LineTo(hdc, x + 7, y + 19);
  MoveToEx(hdc, x + 11, y + 16, nullptr);
  LineTo(hdc, x + 7, y + 19);
  LineTo(hdc, x + 11, y + 22);
  DeleteObject(SelectObject(hdc, pen));
}

void QuickPanelDialog::DrawIconPhrase(HDC hdc, int x, int y) {
  // 对话气泡:圆角矩形 + 尾巴
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC));
  HBRUSH brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
  // 主体:圆角矩形
  RoundRect(hdc, x + 5, y + 5, x + 25, y + 20, 6, 6);
  // 尾巴:斜线
  MoveToEx(hdc, x + 10, y + 20, nullptr);
  LineTo(hdc, x + 8, y + 25);
  LineTo(hdc, x + 13, y + 20);
  // 三条短横线代表文本
  MoveToEx(hdc, x + 9, y + 10, nullptr);
  LineTo(hdc, x + 21, y + 10);
  MoveToEx(hdc, x + 9, y + 14, nullptr);
  LineTo(hdc, x + 18, y + 14);
  DeleteObject(SelectObject(hdc, pen));
  DeleteObject(SelectObject(hdc, brush));
}

void QuickPanelDialog::DrawIconSymbols(HDC hdc, int x, int y) {
  // 键盘(viewBox 24x24)
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC));
  // 外壳
  RoundRect(hdc, x + 3, y + 6, x + 27, y + 24, 2, 2);
  // 顶行按键分隔(4个)
  MoveToEx(hdc, x + 8, y + 6, nullptr); LineTo(hdc, x + 8, y + 12);
  MoveToEx(hdc, x + 13, y + 6, nullptr); LineTo(hdc, x + 13, y + 12);
  MoveToEx(hdc, x + 18, y + 6, nullptr); LineTo(hdc, x + 18, y + 12);
  MoveToEx(hdc, x + 22, y + 6, nullptr); LineTo(hdc, x + 22, y + 12);
  // 中行分隔
  MoveToEx(hdc, x + 3, y + 12, nullptr); LineTo(hdc, x + 27, y + 12);
  MoveToEx(hdc, x + 3, y + 16, nullptr); LineTo(hdc, x + 27, y + 16);
  // 中行内部
  MoveToEx(hdc, x + 11, y + 16, nullptr); LineTo(hdc, x + 11, y + 20);
  MoveToEx(hdc, x + 18, y + 16, nullptr); LineTo(hdc, x + 18, y + 20);
  // 空格(底部加粗)
  HPEN penBold = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 3, kIcoDimC));
  MoveToEx(hdc, x + 8, y + 22, nullptr); LineTo(hdc, x + 22, y + 22);
  DeleteObject(SelectObject(hdc, penBold));
  DeleteObject(SelectObject(hdc, pen));
}

void QuickPanelDialog::DrawIconSettings(HDC hdc, int x, int y) {
  // 齿轮(中心圆 + 8 辐射线)
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC));
  // 中心圆
  Ellipse(hdc, x + 9, y + 9, x + 21, y + 21);
  // 8 辐射线
  for (int i = 0; i < 8; i++) {
    double angle = i * 3.14159265 / 4.0;
    double dx = cos(angle), dy = sin(angle);
    MoveToEx(hdc, int(x + 15 + dx * 7), int(y + 15 + dy * 7), nullptr);
    LineTo(hdc, int(x + 15 + dx * 11), int(y + 15 + dy * 11));
  }
  DeleteObject(SelectObject(hdc, pen));
}

void QuickPanelDialog::DrawIconAccount(HDC hdc, int x, int y) {
  // 头像(头 + 肩)
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, kIcoDimC));
  // 头
  Ellipse(hdc, x + 9, y + 5, x + 21, y + 17);
  // 肩(开口向下的弧) - Arc 需要 8 个 int
  Arc(hdc, x + 5, y + 16, x + 25, y + 28, x + 5, y + 28, x + 25, y + 28);
  DeleteObject(SelectObject(hdc, pen));
}


// ===== 静态成员定义 =====
HWND     QuickPanelDialog::s_hwnd         = NULL;
QuickPanelDialog::Mode QuickPanelDialog::s_mode = QuickPanelDialog::Mode::kHidden;
bool     QuickPanelDialog::s_fullwidth    = false;
bool     QuickPanelDialog::s_mouseTracked = false;
int      QuickPanelDialog::s_alpha        = 255;
int      QuickPanelDialog::s_targetAlpha  = 255;
QuickPanelDialog::OnClick QuickPanelDialog::s_onSchema;
QuickPanelDialog::OnClick QuickPanelDialog::s_onUserFolder;
QuickPanelDialog::OnClick QuickPanelDialog::s_onPhrases;
QuickPanelDialog::OnToggle QuickPanelDialog::s_onFullwidth;
QuickPanelDialog::OnClick QuickPanelDialog::s_onSymbols;
QuickPanelDialog::OnClick QuickPanelDialog::s_onLogin;

int QuickPanelDialog::s_hoveredIdx = -1;
int QuickPanelDialog::s_activeIdx  = -1;

HBITMAP  QuickPanelDialog::s_hBmpLogo     = NULL;
HBRUSH   QuickPanelDialog::s_hBrushPanelBg   = NULL;
HBRUSH   QuickPanelDialog::s_hBrushIconDim   = NULL;
HBRUSH   QuickPanelDialog::s_hBrushIconAccent= NULL;
HBRUSH   QuickPanelDialog::s_hBrushActive    = NULL;
HBRUSH   QuickPanelDialog::s_hBrushHighlight = NULL;
HPEN     QuickPanelDialog::s_hPenIconDim     = NULL;
HPEN     QuickPanelDialog::s_hPenIconAccent  = NULL;
HPEN     QuickPanelDialog::s_hPenHighlight   = NULL;
HDC      QuickPanelDialog::s_hdcMem       = NULL;
HBITMAP  QuickPanelDialog::s_hBmpMem      = NULL;
int      QuickPanelDialog::s_panelW_phys  = 0;
int      QuickPanelDialog::s_panelH_phys  = 0;

// ===== 内部 =====
HRESULT QuickPanelDialog::LoadLogoWIC(HWND hwnd, HBITMAP& hBmpOut) {
  hBmpOut = NULL;
  // L75 避雷:不用 GDI+ Bitmap(IStream*),不用 CreateStreamOnHGlobal。
  // 直接 LoadImageW 从 PNG 文件创建 HBITMAP(纯 Win32 GDI,无 IStream 生命周期)。
  wchar_t exeDir[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exeDir, MAX_PATH);  // hwnd 是 HWND 不是 HMODULE,必须传 NULL
  wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
  if (lastSlash) *lastSlash = L'\0';
  wchar_t logoPath[MAX_PATH];
  _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo.png", exeDir);

  // LR_LOADFROMFILE 加载文件,LR_CREATEDIBSECTION 返回 DIB section(可 AlphaBlend)
  hBmpOut = (HBITMAP)LoadImageW(
      NULL, logoPath, IMAGE_BITMAP,
      0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
  return hBmpOut ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

HRESULT QuickPanelDialog::CreateOffscreenDC(int w, int h) {
  DestroyOffscreenDC();
  s_panelW_phys = w;
  s_panelH_phys = h;
  HDC hdcScreen = GetDC(NULL);
  s_hdcMem = CreateCompatibleDC(hdcScreen);
  if (!s_hdcMem) { ReleaseDC(NULL, hdcScreen); return E_FAIL; }
  s_hBmpMem = CreateCompatibleBitmap(hdcScreen, w, h);
  if (!s_hBmpMem) { ReleaseDC(NULL, hdcScreen); return E_FAIL; }
  SelectObject(s_hdcMem, s_hBmpMem);
  ReleaseDC(NULL, hdcScreen);
  return S_OK;
}

void QuickPanelDialog::DestroyOffscreenDC() {
  if (s_hdcMem) { DeleteDC(s_hdcMem); s_hdcMem = NULL; }
  if (s_hBmpMem) { DeleteObject(s_hBmpMem); s_hBmpMem = NULL; }
}

int QuickPanelDialog::HitTest(int x, int y) {
  if (s_panelW_phys == 0) return -1;
  if (x < 0 || y < 0 || x >= s_panelW_phys || y >= s_panelH_phys) return -1;
  int padding = kPanelPadding;
  int brandX = padding;
  int brandY = padding;
  if (x >= brandX && x < brandX + kBrandSize && y >= brandY && y < brandY + kBrandSize) return -2;  // brand
  int buttonStartX = padding + kBrandSize + 4;
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (kBtnSize + 2);
    if (x >= x0 && x < x0 + kBtnSize && y >= padding && y < padding + kBtnSize) return i;
  }
  return -1;
}

// ===== Window procedure =====
LRESULT CALLBACK QuickPanelDialog::WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_CREATE:    return OnCreate(hwnd);
    case WM_DESTROY:   return OnDestroy(hwnd);
    case WM_PAINT:     return OnPaint(hwnd);
    case WM_ERASEBKGND: return 1;          // GDI 双缓冲,不让 Windows 清背景
    case WM_LBUTTONDOWN: {
      POINT p = {LOWORD(l), HIWORD(l)};
      s_activeIdx = HitTest(p.x, p.y);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    case WM_LBUTTONUP: {
      POINT p = {LOWORD(l), HIWORD(l)};
      int hit = HitTest(p.x, p.y);
      if (hit >= 0 && hit == s_activeIdx) {
        // T005: 5 按钮 no-op (v0.19.0.7 ship 切片)
      }
      s_activeIdx = -1;
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    case WM_MOUSEMOVE: {
      POINT p = {LOWORD(l), HIWORD(l)};
      int hit = HitTest(p.x, p.y);
      if (hit != s_hoveredIdx) {
        s_hoveredIdx = hit;
        InvalidateRect(hwnd, NULL, FALSE);
      }
      if (!s_mouseTracked) {
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        s_mouseTracked = true;
      }
      return 0;
    }
    case WM_MOUSELEAVE: {
      s_mouseTracked = false;
      if (s_hoveredIdx != -1) {
        s_hoveredIdx = -1;
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    case WM_KILLFOCUS: return OnKillFocus(hwnd);
    case WM_KEYDOWN:   return OnKeyDown(hwnd, w);
    case WM_TIMER:      return OnTimer(hwnd, w);
    default: break;
  }
  return DefWindowProc(hwnd, msg, w, l);
}

LRESULT QuickPanelDialog::OnCreate(HWND hwnd) {
  s_hwnd = hwnd;
  RECT rc;
  GetClientRect(hwnd, &rc);
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) return 0;

  // 创建资源
  LoadLogoWIC(hwnd, s_hBmpLogo);
  s_hBrushPanelBg    = CreateSolidBrush(kBgTop);
  s_hBrushIconDim    = CreateSolidBrush(kIcoDimC);
  s_hBrushIconAccent = CreateSolidBrush(kAccentC);
  s_hBrushActive     = CreateSolidBrush(kAccentC);
  s_hBrushHighlight  = CreateSolidBrush(kHighlight);
  s_hPenIconDim      = CreatePen(PS_SOLID, 2, kIcoDimC);
  s_hPenIconAccent   = CreatePen(PS_SOLID, 2, kAccentC);
  s_hPenHighlight    = CreatePen(PS_SOLID, 1, kHighlight);

  // off-screen DC
  CreateOffscreenDC(w, h);

  // 渐变背景:GDI GradientFill (GDI 原生,不是 GDI+)
  // (v0.19.0.7 用 LWA_ALPHA 86% uniform 简化,gradient 留 v0.19.0.8+)

  return 0;
}

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  s_hoveredIdx = -1;
  s_activeIdx = -1;
  DestroyOffscreenDC();
  if (s_hBmpLogo) { DeleteObject(s_hBmpLogo); s_hBmpLogo = NULL; }
  if (s_hBrushPanelBg)   { DeleteObject(s_hBrushPanelBg);   s_hBrushPanelBg   = NULL; }
  if (s_hBrushIconDim)   { DeleteObject(s_hBrushIconDim);   s_hBrushIconDim   = NULL; }
  if (s_hBrushIconAccent){ DeleteObject(s_hBrushIconAccent);s_hBrushIconAccent= NULL; }
  if (s_hBrushActive)    { DeleteObject(s_hBrushActive);    s_hBrushActive    = NULL; }
  if (s_hBrushHighlight) { DeleteObject(s_hBrushHighlight); s_hBrushHighlight = NULL; }
  if (s_hPenIconDim)     { DeleteObject(s_hPenIconDim);     s_hPenIconDim     = NULL; }
  if (s_hPenIconAccent)  { DeleteObject(s_hPenIconAccent);  s_hPenIconAccent  = NULL; }
  if (s_hPenHighlight)   { DeleteObject(s_hPenHighlight);   s_hPenHighlight   = NULL; }
  if (s_hwnd == hwnd) s_hwnd = NULL;
  return 0;
}

LRESULT QuickPanelDialog::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  if (!hdc || !s_hdcMem) { EndPaint(hwnd, &ps); return 0; }

  // 1. 画到 off-screen DC (避免闪烁)
  // 背景渐变:简单的 GDI GradientFill (垂直)
  TRIVERTEX vert[2] = {
    {0, 0, kBgTop & 0xFFFFFF, 0xFF00},  // top
    {0, kPanelH, kBgBot & 0xFFFFFF, 0xFF00}  // bottom (full alpha, 86% LWA will dim)
  };
  GRADIENT_RECT gRect = {0, 1};
  GradientFill(s_hdcMem, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

  // 圆角面板效果(用 RoundRect 画边框)
  RECT panelRect = {0, 0, kPanelW, kPanelH};
  // 不画边框(去 WS_EX_LAYERED 后,D2D 边框不画了)

  // 2. 画 logo (Fluxing 红色猿猴)
  if (s_hBmpLogo) {
    HDC hdcMemLogo = CreateCompatibleDC(s_hdcMem);
    SelectObject(hdcMemLogo, s_hBmpLogo);
    BitBlt(s_hdcMem, kPanelPadding, kPanelPadding, kBrandSize, kBrandSize,
           hdcMemLogo, 0, 0, SRCCOPY);
    DeleteDC(hdcMemLogo);
  }

  // 3. 画 5 个按钮
  int buttonStartX = kPanelPadding + kBrandSize + 4;
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (kBtnSize + 2);
    int y0 = kPanelPadding;

    // 选 brush:  active 态用品牌色,hover 态用品牌色,默认用 dim
    bool isActive = (i == s_activeIdx);
    bool isHover = (i == s_hoveredIdx);
    HBRUSH bgBrush = NULL;
    if (isActive) bgBrush = s_hBrushActive;
    else if (isHover) bgBrush = s_hBrushIconAccent;

    if (bgBrush) {
      HRGN rgn = CreateRoundRectRgn(x0, y0, x0 + kBtnSize, y0 + kBtnSize, kBtnRadius, kBtnRadius);
      FillRgn(s_hdcMem, rgn, bgBrush);
      DeleteObject(rgn);
    }

    // 画 icon
    int iconX = x0 + (kBtnSize - kIcoSize) / 2;
    int iconY = y0 + (kBtnSize - kIcoSize) / 2;
    HPEN oldPen = (HPEN)SelectObject(s_hdcMem, isActive ? GetStockObject(WHITE_PEN) : (isHover ? s_hPenIconAccent : s_hPenIconDim));
    switch (i) {
      case 0: DrawIconSchema(s_hdcMem, iconX, iconY); break;
      case 1: DrawIconPhrase(s_hdcMem, iconX, iconY); break;
      case 2: DrawIconSymbols(s_hdcMem, iconX, iconY); break;
      case 3: DrawIconSettings(s_hdcMem, iconX, iconY); break;
      case 4: DrawIconAccount(s_hdcMem, iconX, iconY); break;
    }
    SelectObject(s_hdcMem, oldPen);
  }

  // 4. Blit 到屏幕
  BitBlt(hdc, 0, 0, kPanelW, kPanelH, s_hdcMem, 0, 0, SRCCOPY);
  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT QuickPanelDialog::OnLButtonUp(HWND, int, int) { return 0; }
LRESULT QuickPanelDialog::OnLButtonDown(HWND, int, int) { return 0; }
void    QuickPanelDialog::OnMouseMove(HWND) {}
void    QuickPanelDialog::OnMouseLeave(HWND) {}

LRESULT QuickPanelDialog::OnTimer(HWND hwnd, WPARAM w) {
  if (w == 1) {
    KillTimer(hwnd, 1);
    Hide();
  }
  return 0;
}

LRESULT QuickPanelDialog::OnKillFocus(HWND hwnd) {
  SetTimer(hwnd, 1, 1000, NULL);
  return 0;
}

LRESULT QuickPanelDialog::OnKeyDown(HWND hwnd, WPARAM key) {
  if (key == VK_ESCAPE) {
    Hide();
    return 0;
  }
  return DefWindowProc(hwnd, key, 0, 0);
}

// ===== Public API =====
void QuickPanelDialog::Show(bool currentFullwidth,
                             OnClick onSchema, OnClick onUserFolder,
                             OnClick onPhrases, OnToggle onFullwidth,
                             OnClick onSymbols, OnClick onLogin) {
  (void)currentFullwidth; (void)onSchema; (void)onUserFolder;
  (void)onPhrases; (void)onFullwidth; (void)onSymbols; (void)onLogin;
  if (s_hwnd) {
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(s_hwnd, NULL, FALSE);
    return;
  }
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kWindowClassName;
  RegisterClassExW(&wc);

  RECT workArea;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
  int x = workArea.right - kPanelW - 12;
  int y = workArea.bottom - kPanelH - 12;

  s_hwnd = CreateWindowExW(
      WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST,
      kWindowClassName, L"Fluxing QuickPanel",
      WS_POPUP,
      x, y, kPanelW, kPanelH,
      NULL, NULL, GetModuleHandle(NULL), NULL);
  if (!s_hwnd) return;
  // L75: 86% uniform translucency(per-pixel 留 v0.19.0.8+ UpdateLayeredWindow)
  SetLayeredWindowAttributes(s_hwnd, 0, kAlphaPanel, LWA_ALPHA);
  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  InvalidateRect(s_hwnd, NULL, FALSE);
}

void QuickPanelDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, 1);
    ShowWindow(s_hwnd, SW_HIDE);
  }
  s_hoveredIdx = -1;
  s_activeIdx = -1;
}

void QuickPanelDialog::ToggleMode() {
  if (s_hwnd && IsWindow(s_hwnd) && IsWindowVisible(s_hwnd)) {
    Hide();
  } else {
    Show(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  }
}

void QuickPanelDialog::EnableAlwaysShowMode(
    OnClick onSchema, OnClick onUserFolder, OnClick onPhrases,
    OnToggle onFullwidth, OnClick onSymbols, OnClick onLogin) {
  s_onSchema     = onSchema;
  s_onUserFolder = onUserFolder;
  s_onPhrases    = onPhrases;
  s_onFullwidth  = onFullwidth;
  s_onSymbols    = onSymbols;
  s_onLogin      = onLogin;
  Show(false, onSchema, onUserFolder, onPhrases, onFullwidth, onSymbols, onLogin);
}
