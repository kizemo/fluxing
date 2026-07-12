// QuickPanelDialog v3-rev3 — PURE GDI + per-pixel alpha (spec 074 v0.19.0.10)
//
// 设计: docs/design/quickpanel-v3/index.html
// 历史雷区(plan.md 强制):
//   ❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)
//   ❌ 绝对不 CreateStreamOnHGlobal + Release() (L67 根因)
//   ❌ 绝对不用 D2D ID2D1HwndRenderTarget (L74 黑 panel,DComp 未 promote)
//   ❌ 绝对不用 LWA_ALPHA uniform (L77 — 不是 liquid glass,只是 86% 全白)
//   ❌ 绝对不在 WS_EX_LAYERED 后 BitBlt 到 window DC (双路径会闪烁)
//   → 用 UpdateLayeredWindow + 32-bit DIB 一次性提交
//
// 本实现纯 Win32 GDI + per-pixel alpha:
//   - LoadImageW 直接从 PNG 文件创建 HBITMAP(无 GDI+ 路径)
//   - 全部 painting 用 HDC + SelectObject(无 D2D、无 GDI+)
//   - 5 个图标用 MoveTo/LineTo/Ellipse/Rectangle 画
//   - 32-bit DIBSection (BGRA) 存放 layered surface
//   - PaintOpaqueContent() 画 opaque 内容到 s_hdcMem (logo + icons + 圆角边)
//   - ApplyAlphaGradient() 改 pBits alpha 通道:圆角内按 y 渐变 140→82,圆角外 0
//   - WS_EX_LAYERED + UpdateLayeredWindow(ULW_ALPHA) 提交到 screen
//
#include "stdafx.h"
#include <functional>
#include "QuickPanelDialog.h"

// L81-fix: PNG logo 加载改用 WIC 不用 GDI+ Bitmap(IStream*) (L67-L69 雷区) 也不用
// LoadImageW(IMAGE_BITMAP) (L81 — IMAGE_BITMAP 不含 PNG,静默失败)。
// 需要 <wincodec.h> + windowscodecs.lib (在 xmake.lua 已加)。
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

static const wchar_t kWindowClassName[] = L"FluxingQuickPanel_v3";

namespace {
// 圆角掩码测试:4 corner cells 用 circle equation,其余 (W-r)..(H-r) 范围内的 inside=true。
// per-pixel 内调用 ~54K 次 (360x68,1x DPI),避免 CreateRoundRectRgn / PtInRegion 开销。
inline bool IsInsideRoundedRect(int x, int y, int W, int H, int r) {
  if (x < 0 || x >= W || y < 0 || y >= H) return false;
  if (x < r && y < r) {
    int dx = r - x, dy = r - y;
    return (dx * dx + dy * dy) <= (r * r);
  }
  if (x >= W - r && y < r) {
    int dx = x - (W - r - 1), dy = r - y;
    return (dx * dx + dy * dy) <= (r * r);
  }
  if (x < r && y >= H - r) {
    int dx = r - x, dy = y - (H - r - 1);
    return (dx * dx + dy * dy) <= (r * r);
  }
  if (x >= W - r && y >= H - r) {
    int dx = x - (W - r - 1), dy = y - (H - r - 1);
    return (dx * dx + dy * dy) <= (r * r);
  }
  return true;
}
}  // anonymous namespace

// 颜色常量(在文件作用域,本地作用域,kIcoDimC 等)
// L82-fix: panel bg 从纯白 RGB(255,255,255) 改成 RGB(245,245,250) 浅玻璃冷色。
// 原因(L82 user feedback):在浅色桌面 wallpaper 上,纯白 BG + 白边框 = 完全隐形。
// 浅玻璃色 仍保留 macOS Liquid Glass 视觉感,但**任何背景下**都能看出 panel 形状。
constexpr COLORREF kIcoDimC   = RGB(50, 50, 60);     // 深灰 — 改深 10 step,作为 panel 边框色
                                              //   (之前 RGB(60,60,67),user 报告 panel 边界 transparent)
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
// L81 + L82: LoadLogoWIC 用 WIC 解码 PNG(不用 LoadImageW/IMAGE_BITMAP,后者不支持 PNG)。
// 流程:CoCreateInstance(IWICImagingFactory) → CreateDecoderFromFilename →
// GetFrame(0) → FormatConverter(32bppBGRA) → CopyPixels 到 32-bit DIBSection。
//
// L82: 文件名改 `fluxing-logo_small.png` (用户实际 logo,20x20 PNG icon)。
// 之前 v0.19.0.10/0.11 用 `fluxing-logo.png` (700x700 大 logo,中心裁切后
// 渲染出来是橘红色块,不是 design 上的小 icon)。
HRESULT QuickPanelDialog::LoadLogoWIC(HWND /*hwnd*/, HBITMAP& hBmpOut) {
  hBmpOut = NULL;
  wchar_t exeDir[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exeDir, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
  if (lastSlash) *lastSlash = L'\0';
  wchar_t logoPath[MAX_PATH];
  _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo_small.png", exeDir);

  // 确保 thread 上 COM 已初始化(WIC STA)。失败也不致命 → 仅显示空 logo
  HRESULT hrInit = EnsureComInit();
  if (FAILED(hrInit)) return hrInit;

  Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&wic));
  if (FAILED(hr)) return hr;

  Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
  hr = wic->CreateDecoderFromFilename(logoPath, nullptr, GENERIC_READ,
                                      WICDecodeMetadataCacheOnLoad, &dec);
  if (FAILED(hr)) return hr;  // 文件可能不存在 — 用户没装 logo,或路径错

  Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
  hr = dec->GetFrame(0, &frame);
  if (FAILED(hr)) return hr;

  Microsoft::WRL::ComPtr<IWICFormatConverter> fmtConv;
  hr = wic->CreateFormatConverter(&fmtConv);
  if (FAILED(hr)) return hr;

  hr = fmtConv->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA,
                            WICBitmapDitherTypeNone, nullptr, 0.0,
                            WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) return hr;

  UINT w = 0, h = 0;
  hr = fmtConv->GetSize(&w, &h);
  if (FAILED(hr) || w == 0 || h == 0) return hr;

  // 创建 32-bit BGRA DIB Section (top-down,负 height) 接收 CopyPixels 输出。
  // 与 CreateOffscreenDC 的 s_hBmpMem 用同样的 masks,保证 BitBlt/AlphaBlend 时 alpha
  // 通道结构一致。
  BITMAPV5HEADER bi = {};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = w;
  bi.bV5Height = -(LONG)h;  // top-down
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  void* bits = nullptr;
  HBITMAP hbmp = CreateDIBSection(nullptr, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                                   &bits, nullptr, 0);
  if (!hbmp || !bits) {
    if (hbmp) DeleteObject(hbmp);
    return E_OUTOFMEMORY;
  }

  UINT stride = w * 4;
  UINT total = stride * h;
  hr = fmtConv->CopyPixels(nullptr, stride, total, (BYTE*)bits);
  if (FAILED(hr)) {
    DeleteObject(hbmp);
    return hr;
  }

  hBmpOut = hbmp;
  return S_OK;
}

// L81:COM 一次性初始化。CoInitializeEx 返回 S_OK 表示初始化成功,
// S_FALSE 表示**已**初始化过(无需重复)。我们 idempotent 调用一次。
HRESULT QuickPanelDialog::EnsureComInit() {
  static bool s_initialized = false;
  if (s_initialized) return S_OK;
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (SUCCEEDED(hr)) {
    s_initialized = true;
    return S_OK;
  }
  // RPC_E_CHANGED_MODE:thread 已经被初始化成不同的模式 — 通常由 WinSparkle 等
  // CoInitializeEx(COINIT_MULTITHREADED) 在前。WIC 在 STA/MTA 都 ok,容忍。
  if (hr == RPC_E_CHANGED_MODE) {
    s_initialized = true;  // 不要再重试
    return S_FALSE;
  }
  return hr;
}

HRESULT QuickPanelDialog::CreateOffscreenDC(int w, int h) {
  DestroyOffscreenDC();
  s_panelW_phys = w;
  s_panelH_phys = h;
  HDC hdcScreen = GetDC(NULL);
  s_hdcMem = CreateCompatibleDC(hdcScreen);
  if (!s_hdcMem) { ReleaseDC(NULL, hdcScreen); return E_FAIL; }

  // L78-fix: 改用 32-bit DIB section(带 alpha channel)
  // 之前 CreateCompatibleBitmap 是 24-bit DDB,GradientFill 的 alpha 被丢掉 → 全黑
  // L81-fix: 用 top-down DIB (负 height) 让 y=0 ↔ top, y=H-1 ↔ bottom 直观一致,
  // 否则 ApplyAlphaGradient 的 gradient 计算和实际像素位置是反的。
  BITMAPV5HEADER bi = {};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = w;
  bi.bV5Height = -h;  // top-down (负值)
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask   = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask  = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;
  void* pBits = nullptr;
  s_hBmpMem = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &pBits, NULL, 0);
  if (!s_hBmpMem) { DeleteDC(s_hdcMem); s_hdcMem = NULL; ReleaseDC(NULL, hdcScreen); return E_FAIL; }
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
    // L81-fix: 切到其他 IME (en-US 等) 时 WeaselServer 进程会失焦 → WM_ACTIVATEAPP 触发。
    // 自动 Hide() 让 panel 跟着前台 app 切走,不残留屏幕上。
    case WM_ACTIVATEAPP: {
      if (w == FALSE) {  // app 被 deactivate (前台切走)
        Hide();
      }
      return 0;
    }
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

// PaintOpaqueContent: v0.19.0.10 新抽出。把原 OnPaint 的"画 opaque 内容到 s_hdcMem"
// 部分抽出来 — roundRgn WHITE bg + 1px border + top highlight + logo + 5 icons。
// 不再 BitBlt 到 window DC (WS_EX_LAYERED 后双路径会闪烁,见 file header 雷区)。
void QuickPanelDialog::PaintOpaqueContent(HDC hdc) {
  // 1a. 圆角 panel 背景 — L82-fix: 从 WHITE_BRUSH 改成 s_hBrushPanelBg
  // (s_hBrushPanelBg = CreateSolidBrush(kBgTop = RGB(245,245,250))。
  // 在浅色 wallpaper 上不再是"panel 与背景融为一体"。)
  HRGN panelRgn = CreateRoundRectRgn(0, 0, kPanelW, kPanelH, kPanelRadius, kPanelRadius);
  FillRgn(hdc, panelRgn, s_hBrushPanelBg);
  DeleteObject(panelRgn);

  // 1b. 1px 边框 — L82-fix: 从 WHITE_BRUSH(浅色桌面隐形)改成 kIcoDimC
  // (RGB(50,50,60) 深灰,任何背景下可见)。Border RGB 不在 ApplyAlphaGradient 的
  // "panel bg" 范围 (r>=240 检查不通过),所以 border 保持 opaque alpha=255,
  // 给 panel 一个**稳定的轮廓**,per-pixel alpha 仅作用于 bg fill。
  HRGN borderRgn = CreateRoundRectRgn(0, 0, kPanelW, kPanelH, kPanelRadius, kPanelRadius);
  FrameRgn(hdc, borderRgn, s_hBrushIconDim, 1, 1);
  DeleteObject(borderRgn);

  // 1c. 顶 1px 高光 (设计: top highlight 用白 0.85 alpha)
  RECT topHL = {kPanelRadius, 0, kPanelW - kPanelRadius, 1};
  FillRect(hdc, &topHL, s_hBrushHighlight);

  // 2. 画 logo (Fluxing 红色猿猴)
  // L81-fix: 用 AlphaBlend + AC_SRC_ALPHA 替代 BitBlt(SRCCOPY)。
  // 原因:v0.19.0.10 用 BitBlt(SRCCOPY) 把 WIC 加载的 32-bit BGRA logo 画到 32-bit BGRA panel,
  // GDI 在两个 32-bit DIB 之间 BitBlt(SRCCOPY) 行为:
  //   - dest 是 BI_BITFIELDS DIB
  //   - src 是 BI_BITFIELDS DIB
  //   - SRCCOPY 在这种情况下**会剥离 alpha** — dest alpha 全部被写成 255
  //     (GDI 不区分 source alpha channel,直接当 RGB 复制)
  // 加上 source 的 transparent pixels (alpha=0) 会变成 RGB only 的黑色,
  // 整张 logo 在 panel 上看起来是黑底 + 红 logo。
  // AlphaBlend(AC_SRC_ALPHA) 是 GDI 唯一能保留 per-pixel alpha 的合成操作。
  //
  // L81-fix2: 大 PNG (700x700) 中心裁切。
  // L82-fix: 小 PNG (20x20 用户实际 logo `fluxing-logo_small.png`) 用全画布
  // + AlphaBlend 拉伸 (20→56)。GDI AlphaBlend 自带线性拉伸,这是 WIC + AlphaBlend
  // 路径相对 StretchBlt(SRCCOPY 剥 alpha) 的优势。
  if (s_hBmpLogo) {
    HDC hdcMemLogo = CreateCompatibleDC(hdc);
    if (hdcMemLogo) {
      HGDIOBJ prev = SelectObject(hdcMemLogo, s_hBmpLogo);
      BITMAP bm = {};
      GetObject(s_hBmpLogo, sizeof(bm), &bm);
      int logoW = bm.bmWidth;
      int logoH = bm.bmHeight;
      BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

      // L82: 自适应 source rect
      // - 大 logo (>= 2x brand area):中心裁切 (避免空白边缘)
      // - 小 logo (< 2x brand area):全画布 + AlphaBlend 拉伸到 kBrandSize
      int srcL, srcT, srcW, srcH;
      if (logoW >= kBrandSize * 2 && logoH >= kBrandSize * 2) {
        // 大 logo:中心裁切
        srcL = (logoW - kBrandSize) / 2;
        srcT = (logoH - kBrandSize) / 2;
        srcW = kBrandSize;
        srcH = kBrandSize;
      } else {
        // 小 logo:全画布拉伸
        srcL = 0;
        srcT = 0;
        srcW = logoW;
        srcH = logoH;
      }

      AlphaBlend(hdc,
                 kPanelPadding, kPanelPadding,
                 kBrandSize, kBrandSize,
                 hdcMemLogo,
                 srcL, srcT, srcW, srcH,
                 bf);
      SelectObject(hdcMemLogo, prev);
      DeleteDC(hdcMemLogo);
    }
  }

  // 3. 画 5 个按钮
  // L79-fix: hover 只改 icon stroke 颜色(不画 bg 填充,符合 v3-rev3 设计)
  // active: bg 橙渐变 + icon 白
  int buttonStartX = kPanelPadding + kBrandSize + 4;
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (kBtnSize + 2);
    int y0 = kPanelPadding;

    bool isActive = (i == s_activeIdx);
    bool isHover = (i == s_hoveredIdx);

    HBRUSH bgBrush = NULL;
    if (isActive) bgBrush = s_hBrushActive;  // active: 橙

    if (bgBrush) {
      HRGN rgn = CreateRoundRectRgn(x0, y0, x0 + kBtnSize, y0 + kBtnSize,
                                     kBtnRadius, kBtnRadius);
      FillRgn(hdc, rgn, bgBrush);
      DeleteObject(rgn);
    }

    HPEN iconPen;
    if (isActive) iconPen = (HPEN)GetStockObject(WHITE_PEN);
    else if (isHover) iconPen = s_hPenIconAccent;
    else iconPen = s_hPenIconDim;
    HPEN oldPen = (HPEN)SelectObject(hdc, iconPen);

    int iconX = x0 + (kBtnSize - kIcoSize) / 2;
    int iconY = y0 + (kBtnSize - kIcoSize) / 2;
    switch (i) {
      case 0: DrawIconSchema(hdc, iconX, iconY); break;
      case 1: DrawIconPhrase(hdc, iconX, iconY); break;
      case 2: DrawIconSymbols(hdc, iconX, iconY); break;
      case 3: DrawIconSettings(hdc, iconX, iconY); break;
      case 4: DrawIconAccount(hdc, iconX, iconY); break;
    }
    SelectObject(hdc, oldPen);
  }
}

// ApplyAlphaGradient: v0.19.0.10 新增 per-pixel alpha pipeline。
// 把 32-bit DIB 的 alpha 通道从默认 alpha=255 (GDI 写入默认) 改成 liquid glass 形状:
// - 圆角外 (corner & outside) alpha = 0 → 桌面可见
// - 圆角内,RGB == 纯白 (panel bg / border / top highlight): 按 y 渐变
//   top alpha=kAlphaPanelTop (140 = 0x88),bot alpha=kAlphaPanelBot (82 = 0x52)
//   Linear interpolate by row ratio
// - 圆角内,RGB 含色 (icons / logo / active orange bg): 保留 alpha=255 (opaque 图形)
//
// 为什么按 RGB 区分?GDI Brush 没有 alpha 通道,在 32-bit BI_BITFIELDS DIB 上画出来一律
// alpha=255。区分 bg vs icon 的廉价办法是看 RGB:panel bg 是 WHITE_BRUSH,纯白;
// icons 用 kIcoDimC(60,60,67)/kAccentC(255,95,49)/kAccent2C(155,81,224),非纯白;
// logo 是 fluxing 品牌色 PNG,非纯白; 边框 1px white — 仅 border 这一处会被设成 alpha=gradient
// (轻量损失,1px 的 opaque vs 半透明差异肉眼几乎不可察)。
void QuickPanelDialog::ApplyAlphaGradient() {
  if (!s_hBmpMem || s_panelW_phys <= 0 || s_panelH_phys <= 0) return;
  BITMAP bm = {};
  if (!GetObject(s_hBmpMem, sizeof(bm), &bm) || !bm.bmBits) return;

  DWORD* p = static_cast<DWORD*>(bm.bmBits);
  const int W = s_panelW_phys;
  const int H = s_panelH_phys;
  const int r = kPanelRadius;
  const int Hminus1 = (H > 1) ? (H - 1) : 1;

  for (int y = 0; y < H; y++) {
    // linear gradient: alpha = top at y=0 → bottom at y=H-1
    // 用定点数 (16.16) 避免浮点 perf 抖动,但小数足够小用整数足够精确
    int aPanel = kAlphaPanelTop +
                 ((int)(kAlphaPanelBot - kAlphaPanelTop) * y / Hminus1);

    for (int x = 0; x < W; x++) {
      DWORD* px = &p[(size_t)y * W + x];
      if (!IsInsideRoundedRect(x, y, W, H, r)) {
        *px = 0;  // alpha=0 (BGRA all zero) — fully transparent (桌面可见)
        continue;
      }
      // 圆角内:RGB 是 panel bg 浅玻璃色 → 渐变 alpha (panel bg)
      // L82-fix: 之前判 `r >= 250 && g >= 250 && b >= 250` 配纯白 panel bg。
      // 现在 panel bg = kBgTop = RGB(245,245,250) 浅玻璃冷色,放宽阈值到
      // `r >= 240 && g >= 240 && b >= 240`,确保 panel bg (245,245,250) 命中
      // gradient alpha,top highlight (255,255,255) 也命中。
      //
      // L82-fix2: **else 分支必须强制设 alpha=255**。原因:GDI 在 32-bit BI_BITFIELDS
      // DIB 上,MoveTo/LineTo (pen) 和 FrameRgn (border brush) 写入 RGB 但
      // **alpha = 0**(它们不管理 alpha 通道)。只有 FillRgn/FillRect 写 alpha=255。
      // 如果我们 leave-as-is,icon lines + border 都是 alpha=0 → 完全透明,
      // 用户看不到。这是 v0.19.0.11 user 反馈的"border transparent"的根因。
      // 修复:else 分支强制设 alpha=255 保留 RGB,让所有非-bg 像素(border, icons,
      // logo, active bg) 保持完全 opaque,配合 gradient panel bg 形成
      // **深色 border 在浅色 bg 上明显可见** 的视觉差。
      BYTE r8 = (*px >> 16) & 0xFF;
      BYTE g8 = (*px >> 8)  & 0xFF;
      BYTE b8 =  *px        & 0xFF;
      if (r8 >= 240 && g8 >= 240 && b8 >= 240) {
        // panel bg / top highlight:gradient alpha + 保留 RGB
        *px = ((DWORD)aPanel << 24) | (r8 << 16) | (g8 << 8) | b8;
      } else {
        // border / icons / logo / active bg:opaque alpha=255 + 保留 RGB
        *px = 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
      }
    }
  }
}

// RepaintLayered: v0.19.0.10 新 layered surface 提交。
// - PaintOpaqueContent → ApplyAlphaGradient → UpdateLayeredWindow
// - UpdateLayeredWindow 从 s_hdcMem 读 32-bit BGRA → 提交整个 panel 到 screen,
//   屏幕合成器按 per-pixel alpha 决定哪些像素透桌面。
//
// WS_EX_LAYERED 后 WM_PAINT 路径不再有效 (BeginPaint/EndPaint 拿到 DC 但 layered
// window 不在那画) — 改成走 UpdateLayeredWindow 一次性提交。Screen 上看是
// 透明 + 半透明白底图标,符合 Liquid Glass 设计意图。
void QuickPanelDialog::RepaintLayered(HWND hwnd) {
  if (!hwnd || !s_hdcMem || !s_hBmpMem ||
      s_panelW_phys <= 0 || s_panelH_phys <= 0) return;

  // L81-fix: 必须在 PaintOpaqueContent 之前 memset s_hBmpMem pBits = 0!
  // 原因:s_hBmpMem 是 32-bit BGRA DIB Section,pBits 是 raw buffer。GDI 写入时
  // 默认 alpha=255 + RGB,Pixel 没被画到的位置仍然保留**上一次的 RGB+alpha**。
  // 这导致:
  //   - 上一次 active=true 的橙色 button0 在下次 active=false 时仍然显示橙色
  //     (PaintOpaqueContent 不画它,但残留在 pBits 里)
  //   - hover 状态变化时残影同样存在
  //   - icon 在某个像素位置偶然被画过一次,下次 GetClientRect 重设也不会清理
  //
  // memset 0 = BGRA(0,0,0,0) = fully transparent black。后续 PaintOpaqueContent
  // 只画必要内容;ApplyAlphaGradient 把圆角内白色像素改 alpha=gradient,
  // 圆角外保留 alpha=0(已经是 0 from memset)。
  BITMAP bm = {};
  if (GetObject(s_hBmpMem, sizeof(bm), &bm) && bm.bmBits) {
    SecureZeroMemory(bm.bmBits, bm.bmHeight * bm.bmWidthBytes);
  }

  PaintOpaqueContent(s_hdcMem);
  ApplyAlphaGradient();

  // v0.19.0.10 diag-dump: 把 s_hBmpMem 的 raw 32-bit BGRA 写到文件便于
  // 验证 ApplyAlphaGradient 真的把 alpha 写到 pBits 了 (PrintWindow 在
  // WS_EX_LAYERED 路径上可能把 alpha composite 掉,看不真切)。
  // 当 FLUXING_QP_DIAG_DUMP=1 才写文件。
  if (GetEnvironmentVariableW(L"FLUXING_QP_DIAG_DUMP", nullptr, 0) != 0) {
    // v0.19.0.11 L81 diag: dump TWO files for full verification
    // (a) qp-dump.bmp = s_hBmpMem (panel after RepaintLayered)
    // (b) qp-logo.bmp = s_hBmpLogo (raw logo bitmap from LoadLogoWIC)
    BITMAP bm = {};
    if (GetObject(s_hBmpMem, sizeof(bm), &bm) && bm.bmBits) {
      HANDLE f = CreateFileW(L"F:\\soft\\00selfmade\\rime_claude\\qp-dump.bmp",
                             GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
      if (f != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        BITMAPFILEHEADER bfh = {};
        bfh.bfType = 0x4D42;
        DWORD dibSize = 40 + 12;
        bfh.bfSize = 14 + dibSize + (DWORD)bm.bmWidthBytes * bm.bmHeight;
        bfh.bfOffBits = 14 + dibSize;
        BITMAPINFOHEADER bi = {};
        bi.biSize = 40;
        bi.biWidth = bm.bmWidth;
        bi.biHeight = -bm.bmHeight;  // top-down
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = 3;  // BI_BITFIELDS
        bi.biSizeImage = (DWORD)bm.bmWidthBytes * bm.bmHeight;
        WriteFile(f, &bfh, sizeof(bfh), &written, nullptr);
        WriteFile(f, &bi, sizeof(bi), &written, nullptr);
        DWORD masks[3] = {0x00FF0000, 0x0000FF00, 0x000000FF};
        WriteFile(f, masks, sizeof(masks), &written, nullptr);
        WriteFile(f, bm.bmBits, bi.biSizeImage, &written, nullptr);
        CloseHandle(f);
      }
    }

    // (b) logo bitmap dump to verify WIC actually loaded PNG
    if (s_hBmpLogo) {
      BITMAP bmL = {};
      if (GetObject(s_hBmpLogo, sizeof(bmL), &bmL) && bmL.bmBits) {
        HANDLE fL = CreateFileW(L"F:\\soft\\00selfmade\\rime_claude\\qp-logo.bmp",
                                GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
        if (fL != INVALID_HANDLE_VALUE) {
          DWORD written = 0;
          BITMAPFILEHEADER bfhL = {};
          bfhL.bfType = 0x4D42;
          DWORD dibSizeL = 40 + 12;
          bfhL.bfSize = 14 + dibSizeL + (DWORD)bmL.bmWidthBytes * bmL.bmHeight;
          bfhL.bfOffBits = 14 + dibSizeL;
          BITMAPINFOHEADER biL = {};
          biL.biSize = 40;
          biL.biWidth = bmL.bmWidth;
          biL.biHeight = -bmL.bmHeight;
          biL.biPlanes = 1;
          biL.biBitCount = 32;
          biL.biCompression = 3;
          biL.biSizeImage = (DWORD)bmL.bmWidthBytes * bmL.bmHeight;
          WriteFile(fL, &bfhL, sizeof(bfhL), &written, nullptr);
          WriteFile(fL, &biL, sizeof(biL), &written, nullptr);
          DWORD masksL[3] = {0x00FF0000, 0x0000FF00, 0x000000FF};
          WriteFile(fL, masksL, sizeof(masksL), &written, nullptr);
          WriteFile(fL, bmL.bmBits, biL.biSizeImage, &written, nullptr);
          CloseHandle(fL);
        }
      }
    }
  }

  // window position: client → screen, 给 UpdateLayeredWindow 的 destination origin
  POINT ptPos = {0, 0};
  RECT rcClient;
  GetClientRect(hwnd, &rcClient);
  MapWindowPoints(hwnd, NULL, (POINT*)&rcClient, 2);
  ptPos.x = rcClient.left;
  ptPos.y = rcClient.top;

  POINT ptSrc = {0, 0};
  SIZE sizeWnd = {s_panelW_phys, s_panelH_phys};
  // AC_SRC_OVER + AC_SRC_ALPHA: source per-pixel alpha 通道生效
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

  if (!UpdateLayeredWindow(hwnd, NULL, &ptPos, &sizeWnd, s_hdcMem,
                           &ptSrc, 0, &blend, ULW_ALPHA)) {
    // UpdateLayeredWindow 失败 — 调试钩子 (L74 教训)
    DWORD err = GetLastError();
    (void)err;
  }
}

LRESULT QuickPanelDialog::OnPaint(HWND hwnd) {
  // v0.19.0.10 layered path:BeginPaint 必须配对 EndPaint,但 layered window 不在
  // window DC 上画 — 走 RepaintLayered 提交 UpdateLayeredWindow 路径。
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  (void)hdc;
  EndPaint(hwnd, &ps);
  RepaintLayered(hwnd);
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
    // L81-fix: reuse 路径也强制置顶(防止其他窗口在 hide→show 间隙盖上面板)
    SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // v0.19.0.10: layered path — InvalidateRect 无意义,直接重画提交
    RepaintLayered(s_hwnd);
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
  // v0.19.0.10: 不再 SetLayeredWindowAttributes(LWA_ALPHA) — 那种 uniform 半透明
  // 不是 liquid glass (面板整体 86% 不透明)。 per-pixel alpha 走 RepaintLayered →
  // ApplyAlphaGradient → UpdateLayeredWindow(ULW_ALPHA)。
  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  // L81-fix: WS_EX_TOPMOST + WS_EX_LAYERED 路径 z-order 不稳,经常被其他窗口盖住。
  // 显式 SetWindowPos(HWND_TOPMOST) 强制置顶,与 WS_EX_TOPMOST 等价但更可靠。
  // SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE:不改变 geometry 也不抢焦。
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  RepaintLayered(s_hwnd);  // 第一次提交 layered surface (带渐变 alpha)
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
