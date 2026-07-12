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

void QuickPanelDialog::DrawIconSchema(HDC hdc, int x, int y, COLORREF penColor) {
  // 双向切换箭头(viewBox 24x24,scale 到 30x30)
  // 上箭头 →: (4,9)→(17,9) + 箭头头
  // L86-fix: pen color 从 caller 传入(default 灰 / hover 橙 / active 白)
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
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

void QuickPanelDialog::DrawIconPhrase(HDC hdc, int x, int y, COLORREF penColor) {
  // 对话气泡:圆角矩形 + 尾巴
  // L86-fix: pen color 从 caller 传入
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
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

void QuickPanelDialog::DrawIconSymbols(HDC hdc, int x, int y, COLORREF penColor) {
  // 键盘(viewBox 24x24)
  // L86-fix: pen color 从 caller 传入
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
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

void QuickPanelDialog::DrawIconSettings(HDC hdc, int x, int y, COLORREF penColor) {
  // 齿轮(中心圆 + 8 辐射线)
  // L86-fix: pen color 从 caller 传入
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
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

void QuickPanelDialog::DrawIconAccount(HDC hdc, int x, int y, COLORREF penColor) {
  // 头像(头 + 肩)
  // L86-fix: pen color 从 caller 传入
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
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
bool     QuickPanelDialog::s_dragging     = false;   // L87-fix 手动 drag 状态
POINT    QuickPanelDialog::s_dragStartCursor = {0, 0}; // L87-fix drag 开始时 cursor
RECT     QuickPanelDialog::s_dragStartWindow = {0, 0, 0, 0}; // L87-fix drag 开始时 window pos
int      QuickPanelDialog::s_outsideMs     = 0;      // L89-fix: 鼠标在 panel 外累计 ms
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
HBRUSH   QuickPanelDialog::s_hBrushShadow    = NULL;
HPEN     QuickPanelDialog::s_hPenIconDim     = NULL;
HPEN     QuickPanelDialog::s_hPenIconAccent  = NULL;
HPEN     QuickPanelDialog::s_hPenHighlight   = NULL;
HDC      QuickPanelDialog::s_hdcMem       = NULL;
HBITMAP  QuickPanelDialog::s_hBmpMem      = NULL;
int      QuickPanelDialog::s_panelW_phys  = 0;
int      QuickPanelDialog::s_panelH_phys  = 0;
float    QuickPanelDialog::s_dpr_x        = 1.0f;
float    QuickPanelDialog::s_dpr_y        = 1.0f;
int      QuickPanelDialog::s_panelPadding_phys = 0;
int      QuickPanelDialog::s_btnSize_phys  = 0;
int      QuickPanelDialog::s_icoSize_phys  = 0;
int      QuickPanelDialog::s_btnRadius_phys = 0;
int      QuickPanelDialog::s_brandSize_phys = 0;
int      QuickPanelDialog::s_panelRadius_phys = 0;
int      QuickPanelDialog::s_btnGap_phys   = 0;

// ===== 内部 =====
// L81 + L82: LoadLogoWIC 用 WIC 解码 PNG(不用 LoadImageW/IMAGE_BITMAP,后者不支持 PNG)。
// 流程:CoCreateInstance(IWICImagingFactory) → CreateDecoderFromFilename →
// GetFrame(0) → FormatConverter(32bppBGRA) → CopyPixels 到 32-bit DIBSection。
//
// L82: 文件名改 `fluxing-logo_small.png` (用户实际 logo,20x20 PNG icon)。
// 之前 v0.19.0.10/0.11 用 `fluxing-logo.png` (700x700 大 logo,中心裁切后
// 渲染出来是橘红色块,不是 design 上的小 icon)。
//
// L83-fix: 加 IWICBitmapScaler 在 WIC 阶段把 PNG 预缩放到 s_brandSize_phys (DPI-aware
// 物理像素)。原因:GDI AlphaBlend **不支持拉伸**(MS docs 显式说 "AlphaBlend does not
// support stretching or compressing"),L82 误以为会拉伸。预缩放到 dest 大小后
// AlphaBlend 1:1 OK。
HRESULT QuickPanelDialog::LoadLogoWIC(HWND /*hwnd*/, HBITMAP& hBmpOut) {
  hBmpOut = NULL;
  wchar_t exeDir[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exeDir, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
  if (lastSlash) *lastSlash = L'\0';
  wchar_t logoPath[MAX_PATH];
  // L84-fix: try fluxing-logo_small.png first (20x20 PNG icon, actual user logo),
  // fall back to fluxing-logo.png (700x700 big logo) if small not found.
  // Some user installs may be missing the small file (older v0.19.0.x installs
  // didn't have it), and the small was added in v0.19.0.13 — fallback ensures
  // logo is always shown after upgrade.
  _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo_small.png", exeDir);
  if (GetFileAttributesW(logoPath) == INVALID_FILE_ATTRIBUTES) {
    _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo.png", exeDir);
  }

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

  // L83: IWICBitmapScaler 预缩放到 brand area 物理大小。保证 GDI AlphaBlend 1:1,
  // 不需要 AlphaBlend 拉伸(它不支持)。
  // 用 s_brandSize_phys(DPI 缩放后的物理像素),不是 logical kBrandSize。
  Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
  hr = wic->CreateBitmapScaler(&scaler);
  if (FAILED(hr)) return hr;
  UINT targetSize = (UINT)s_brandSize_phys;
  if (targetSize < 1) targetSize = 1;  // 保护 0
  hr = scaler->Initialize(frame.Get(), targetSize, targetSize,
                          WICBitmapInterpolationModeHighQualityCubic);
  if (FAILED(hr)) return hr;

  Microsoft::WRL::ComPtr<IWICFormatConverter> fmtConv;
  hr = wic->CreateFormatConverter(&fmtConv);
  if (FAILED(hr)) return hr;

  hr = fmtConv->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA,
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
  // L83-fix: WM_MOUSEMOVE lParam 在 PerMonitor DPI Aware 进程下是 **physical pixels**
  // (Windows 自动缩放从 logical → physical 投递)。HitTest 必须用 physical 像素常量
  // (s_*_phys 按 dpr 缩放后的)。之前用 logical kPanelPadding 等,在 sub-100% DPI 显示器
  // 上 mouse 物理 222 → logical 148 → 按 360x68 layout 算 hit = btn2,但 user 实际在物理
  // btn1 (x=148/240=61% 横向位置)。Hover 完全错位。改用 physical 常量后,physical 222
  // → btn1 (btn1 在 physical x=83..120)。
  int padding = s_panelPadding_phys;
  int brandX = padding;
  int brandY = padding;
  if (x >= brandX && x < brandX + s_brandSize_phys && y >= brandY && y < brandY + s_brandSize_phys) return -2;  // brand
  int buttonStartX = padding + s_brandSize_phys +
                     max(1, (int)(4 * s_dpr_x + 0.5f));
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (s_btnSize_phys + s_btnGap_phys);
    if (x >= x0 && x < x0 + s_btnSize_phys && y >= padding && y < padding + s_btnSize_phys) return i;
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
    // L87-fix: panel 拖动支持 — 改用 **手动 drag** 替代 v0.19.0.16 L86 的 WM_NCHITTEST
    // + HTCAPTION。L86 的 HTCAPTION 在 WS_POPUP + WS_EX_LAYERED 窗口下 Windows
    // DefWindowProc 没有处理 system drag(L86 报告"无法拖动"是 user 反馈)。
    // 手动 drag 方法:
    //   WM_LBUTTONDOWN:HitTest 不在 button 内,SetCapture + 记录原 cursor pos + 原 window pos
    //   WM_MOUSEMOVE:如果 captured,计算 delta + SetWindowPos 移动 window
    //   WM_LBUTTONUP:ReleaseCapture 结束 drag
    // 这样 button 区域正常 click,空白区域可以拖动。
    case WM_NCHITTEST: {
      POINT p = {LOWORD(l), HIWORD(l)};
      ScreenToClient(hwnd, &p);
      int hit = HitTest(p.x, p.y);
      if (hit == -1) return HTCAPTION;  // 空白/品牌区 — 拖动
      return HTCLIENT;  // 按钮区 — 正常 click/hover
    }
    case WM_LBUTTONDOWN: {
      POINT p = {LOWORD(l), HIWORD(l)};
      int hit = HitTest(p.x, p.y);
      // L88-fix: drag **任何位置**都启动(button + brand + 空白)。之前 v0.19.0.17
      // L87 只在 hit==-1 时启动,user 长按 brand area 不响应。修复:无论 hit 是什么,
      // SetCapture + record,这样整个 panel 任何地方长按都拖动。button click 仍然
      // 通过 s_activeIdx 处理(LButtonUp 时检查 s_activeIdx 是否释放前还在同一按钮)。
      SetCapture(hwnd);
      s_dragging = TRUE;
      GetCursorPos(&s_dragStartCursor);
      RECT rc;
      GetWindowRect(hwnd, &rc);
      s_dragStartWindow = rc;
      if (hit >= 0) {
        s_activeIdx = hit;  // button click 仍然 work
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (s_dragging) {
        POINT cur;
        GetCursorPos(&cur);
        int dx = cur.x - s_dragStartCursor.x;
        int dy = cur.y - s_dragStartCursor.y;
        SetWindowPos(hwnd, NULL,
                     s_dragStartWindow.left + dx,
                     s_dragStartWindow.top + dy,
                     0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      } else {
        // L87-fix 替代 v0.19.0.14 L84 polling timer 的 WM_MOUSEMOVE 路径
        // (L86 仍保留 polling timer,这里双保险)
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
      }
      return 0;
    }
    case WM_LBUTTONUP: {
      if (s_dragging) {
        s_dragging = FALSE;
        ReleaseCapture();
        // drag 结束后清 hover
        s_hoveredIdx = -1;
        InvalidateRect(hwnd, NULL, FALSE);
      } else {
        POINT p = {LOWORD(l), HIWORD(l)};
        int hit = HitTest(p.x, p.y);
        if (hit >= 0 && hit == s_activeIdx) {
          // 5 按钮 no-op (spec 070 T007)
        }
        s_activeIdx = -1;
        InvalidateRect(hwnd, NULL, FALSE);
      }
      return 0;
    }
    // L81-fix: 切到其他 IME (en-US 等) 时 WeaselServer 进程会失焦 → WM_ACTIVATEAPP 触发。
    // 自动 Hide() 让 panel 跟着前台 app 切走,不残留屏幕上。
    case WM_ACTIVATEAPP: {
      if (w == FALSE) {  // app 被 deactivate (前台切走)
        Hide();
      }
      return 0;
    }
    // L84-fix: 用 polling timer (100ms) 替代仅靠 WM_MOUSEMOVE 更新 s_hoveredIdx。
    // 原因:WS_EX_LAYERED + WS_EX_NOACTIVATE panel 下 WM_MOUSEMOVE 投递不可靠
    // (L83 sandbox 验证 hovered 实际仍 -1 即使我们 SendMessage WM_MOUSEMOVE)。
    // Polling GetCursorPos + ScreenToClient 自己查鼠标位置,绕过 WM 投递。
    // L84-fix: 用 polling timer (id 2, 100ms) 替代仅靠 WM_MOUSEMOVE 更新 s_hoveredIdx。
    // 原因:WS_EX_LAYERED + WS_EX_NOACTIVATE panel 下 WM_MOUSEMOVE 投递不可靠
    // (L83 sandbox 验证 hovered 实际仍 -1 即使我们 SendMessage WM_MOUSEMOVE)。
    // Polling GetCursorPos + ScreenToClient 自己查鼠标位置,绕过 WM 投递。
    case WM_TIMER: {
      if (w == 2) {  // hover polling timer (id 2, 100ms) — 同时负责 auto-hide
        POINT p;
        if (GetCursorPos(&p) && ScreenToClient(hwnd, &p)) {
          int hit = HitTest(p.x, p.y);
          if (hit != s_hoveredIdx) {
            s_hoveredIdx = hit;
            InvalidateRect(hwnd, NULL, FALSE);
          }
          // L89-fix: auto-hide 当鼠标在 panel 外超过 1.5 秒。之前 v0.19.0.18 panel
          // 显示后没有 auto-hide 时机(WS_EX_NOACTIVATE 收不到 OnKillFocus),挡 user
          // 输入区 → "无法输入中文"。修复:polling timer 检查 hit==-1 (panel 外)
          // 时累加 s_outsideMs,达到 1500ms 自动 Hide。
          if (hit == -1 && !s_dragging) {
            s_outsideMs += 100;
            if (s_outsideMs >= 1500) {
              s_outsideMs = 0;
              Hide();
            }
          } else {
            s_outsideMs = 0;
          }
        }
        return 0;
      }
      return OnTimer(hwnd, w);  // 其它 timer (id 1) 走原 OnKillFocus 关闭逻辑
    }
    // L87-fix: WM_LBUTTONDOWN/UP/MOUSEMOVE 合并到上面新的 manual-drag 实现
    // (case 411/427/455 已处理 button click + manual drag),所以这里删除老的
    // duplicates (case 503/509/519),避免 C2196 "case 重复" 错误。
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

  // L83-fix: DPI 缩放。计算 dpr 并把所有设计常量 (logical 360x68) 缩放到
  // 物理 surface 大小。sandbox 显示器 sub-100% DPI (Windows auto-scale 窗口到 240x45
  // physical),不缩放的话 drawing 用 logical 360 出界 → icons 2-4 看不见 / HitTest 错位。
  s_panelW_phys = w;
  s_panelH_phys = h;
  s_dpr_x = (float)w / (float)kPanelW;
  s_dpr_y = (float)h / (float)kPanelH;
  // DPI 缩放 round-down (>=1) 避免 0 出现 — 保护 drawing 不会 sub-pixel 全部丢失
  auto scale_x = [&](int logical) {
    int v = (int)(logical * s_dpr_x + 0.5f);
    return v < 1 ? 1 : v;
  };
  auto scale_y = [&](int logical) {
    int v = (int)(logical * s_dpr_y + 0.5f);
    return v < 1 ? 1 : v;
  };
  s_panelPadding_phys  = scale_x(kPanelPadding);
  s_btnSize_phys       = scale_x(kBtnSize);
  s_icoSize_phys       = scale_y(kIcoSize);
  s_btnRadius_phys     = scale_x(kBtnRadius);
  s_brandSize_phys     = scale_x(kBrandSize);
  s_panelRadius_phys   = scale_x(kPanelRadius);
  s_btnGap_phys        = scale_x(kBtnGap);

  // 创建资源
  LoadLogoWIC(hwnd, s_hBmpLogo);
  s_hBrushPanelBg    = CreateSolidBrush(kBgTop);
  s_hBrushIconDim    = CreateSolidBrush(kIcoDimC);
  s_hBrushIconAccent = CreateSolidBrush(kAccentC);
  s_hBrushActive     = CreateSolidBrush(kAccentC);
  s_hBrushHighlight  = CreateSolidBrush(kHighlight);
  s_hBrushShadow     = CreateSolidBrush(RGB(200, 215, 235));  // 底部阴影 (L85:浅蓝,避免黑线)
  s_hPenIconDim      = CreatePen(PS_SOLID, 2, kIcoDimC);
  s_hPenIconAccent   = CreatePen(PS_SOLID, 2, kAccentC);
  s_hPenHighlight    = CreatePen(PS_SOLID, 1, kHighlight);

  // off-screen DC
  CreateOffscreenDC(w, h);

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
  if (s_hBrushShadow)    { DeleteObject(s_hBrushShadow);    s_hBrushShadow    = NULL; }
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
  // L83-fix: 全部用 physical 像素。surface 物理大小 = s_panelW_phys × s_panelH_phys,
  // layout 物理常量已按 dpr 缩放。
  const int W = s_panelW_phys;
  const int H = s_panelH_phys;
  const int pad = s_panelPadding_phys;
  const int radius = s_panelRadius_phys;

  // 1a. 圆角 panel 背景 — 浅玻璃冷色,L82-fix:任何背景下都可见
  HRGN panelRgn = CreateRoundRectRgn(0, 0, W, H, radius, radius);
  FillRgn(hdc, panelRgn, s_hBrushPanelBg);
  DeleteObject(panelRgn);

  // 1b. 1px 深灰 border — L82-fix:border RGB(50,50,60) opaque 永远可见
  // L83-fix: FrameRgn 在 sub-100% DPI scale (e.g. dpr=0.667) 下 1px brush = 0.667 physical
  // = sub-pixel → GDI 不绘制。改用 **RoundRect() with pen + NULL_BRUSH** 画 outline。
  // RoundRect() 在所有 DPI 下都会画出 outline(因为它是 line primitive,不是 brush
  // fill)。注意 RoundRect 边界是 (x1, y1) 到 (x2, y2) inclusive,不填内部。
  // border 宽度至少 1 physical pixel:对于 dpr=0.667,取 2 logical (1.33 physical);
  // 对于 dpr=1.0,取 1 logical (=1 physical);对于 dpr>=1.5,取 max(1, 1)=1 logical。
  int borderW = max(1, (int)(1.0f * s_dpr_x + 0.5f));
  HPEN borderPen = CreatePen(PS_SOLID, borderW, kIcoDimC);
  if (borderPen) {
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, 0, 0, W - 1, H - 1, radius, radius);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
  }

  // 1c. 顶部 2px 高光 (L83 mac 风格:顶部反射,白色)
  // 物理缩放:min 1px (低 DPI 不能画 0px)
  int hlHeight = max(1, (int)(2 * s_dpr_y + 0.5f));
  RECT topHL = {radius, 2, W - radius, 2 + hlHeight};
  FillRect(hdc, &topHL, s_hBrushHighlight);

  // 1d. 底部 1px 阴影 (L83 mac 风格:底部阴影线)
  int shHeight = max(1, (int)(1 * s_dpr_y + 0.5f));
  RECT botShadow = {radius, H - 4 - shHeight, W - radius, H - 4};
  FillRect(hdc, &botShadow, s_hBrushShadow);

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
  // 2. 画 logo (Fluxing 红色猿猴)
  // L83-fix: 用 IWICBitmapScaler 在 WIC 阶段把 PNG 预缩放到 brand area 大小
  // (s_brandSize_phys),AlphaBlend 1:1 不需要拉伸。**GDI AlphaBlend 不支持拉伸**
  // (MS docs 显式说),所以必须预缩放。L82 误以为 AlphaBlend 会拉伸。
  if (s_hBmpLogo) {
    HDC hdcMemLogo = CreateCompatibleDC(hdc);
    if (hdcMemLogo) {
      HGDIOBJ prev = SelectObject(hdcMemLogo, s_hBmpLogo);
      BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
      // AlphaBlend src/dst 都是 s_brandSize_phys → 1:1 复制
      AlphaBlend(hdc,
                 pad, pad, s_brandSize_phys, s_brandSize_phys,
                 hdcMemLogo,
                 0, 0, s_brandSize_phys, s_brandSize_phys,
                 bf);
      SelectObject(hdcMemLogo, prev);
      DeleteDC(hdcMemLogo);
    }
  }

  // 3. 画 5 个按钮 — L83-fix: 用 s_*_phys 常量
  // L79-fix: hover 只改 icon stroke 颜色(不画 bg 填充,符合 v3-rev3 设计)
  // L87-fix: brand→btns 间距 4 logical (dpr=1 → 4 物理,dpr=0.7 → 2.8→2 物理)。
  // 给个 max(2, ...) 保证 ≥ 2 物理像素(在 dpr=0.7 时也能看到间距)。
  int buttonStartX = pad + s_brandSize_phys + max(2, (int)(4 * s_dpr_x + 0.5f));
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (s_btnSize_phys + s_btnGap_phys);
    int y0 = pad;

    bool isActive = (i == s_activeIdx);
    bool isHover  = (i == s_hoveredIdx);

    // L86-fix: hover **只改 icon stroke 颜色** — user 明确要求"hover 时 bg 不变,
    // 只 icon 线条变橙"。v0.19.0.15 我加的"三重视觉反馈"(bg 填橙 + 描边)是过度,
    // user 不要 bg 变化。移除 hover 时的 bg fill + 描边,只保留 icon stroke 改色。
    HBRUSH bgBrush = NULL;
    if (isActive) {
      bgBrush = s_hBrushActive;  // active: 实橙 (RGB 255,95,49) — spec 070 T007 设计
    }
    // hover: NO bg change. Only icon stroke color changes below.

    if (bgBrush) {
      HRGN rgn = CreateRoundRectRgn(x0, y0, x0 + s_btnSize_phys, y0 + s_btnSize_phys,
                                     s_btnRadius_phys, s_btnRadius_phys);
      FillRgn(hdc, rgn, bgBrush);
      DeleteObject(rgn);
    }

    // 1px 描边(只 active,hover 不描边)
    if (isActive) {
      HPEN outlinePen = CreatePen(PS_SOLID, max(1, (int)(1.0f * s_dpr_x + 0.5f)),
                                RGB(220, 60, 30));  // active: 暗橙
      HPEN oldOutline = (HPEN)SelectObject(hdc, outlinePen);
      HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
      RoundRect(hdc, x0, y0, x0 + s_btnSize_phys, y0 + s_btnSize_phys,
                s_btnRadius_phys, s_btnRadius_phys);
      SelectObject(hdc, oldBrush);
      SelectObject(hdc, oldOutline);
      DeleteObject(outlinePen);
    }

    HPEN iconPen;
    if (isActive) iconPen = (HPEN)GetStockObject(WHITE_PEN);
    else if (isHover) iconPen = s_hPenIconAccent;
    else iconPen = s_hPenIconDim;
    HPEN oldPen = (HPEN)SelectObject(hdc, iconPen);

    // L86-fix: 把 pen 颜色 RGB 直接传给 DrawIcon*,而不是 hardcoded kIcoDimC。
    // hover=橙 kAccentC,active=白 (WHITE_PEN),default=灰 kIcoDimC。
    // 之前 v0.19.0.15 DrawIcon* hardcoded kIcoDimC,hover 状态机的 pen 选择完全
    // 被忽略。
    DWORD penRgb = isActive ? RGB(255, 255, 255) :
                    isHover  ? kAccentC :
                               kIcoDimC;

    int iconX = x0 + (s_btnSize_phys - s_icoSize_phys) / 2;
    int iconY = y0 + (s_btnSize_phys - s_icoSize_phys) / 2;
    switch (i) {
      case 0: DrawIconSchema(hdc, iconX, iconY, penRgb); break;
      case 1: DrawIconPhrase(hdc, iconX, iconY, penRgb); break;
      case 2: DrawIconSymbols(hdc, iconX, iconY, penRgb); break;
      case 3: DrawIconSettings(hdc, iconX, iconY, penRgb); break;
      case 4: DrawIconAccount(hdc, iconX, iconY, penRgb); break;
    }
    SelectObject(hdc, oldPen);
  }
}

// ApplyAlphaGradient: v0.19.0.10 新增 per-pixel alpha pipeline。
// 把 32-bit DIB 的 alpha 通道从默认 alpha=255 (GDI 写入默认) 改成 liquid glass 形状:
// - 圆角外 (corner & outside) alpha = 0 → 桌面可见
// - 圆角内,RGB == 纯白 (panel bg / border / top highlight): 按 y 渐变
// - 圆角内,RGB 含色 (icons / logo / active orange bg): 保留 alpha=255 (opaque 图形)
//
// L83-fix: 使用 GDI 的 `PtInRegion(rgn)` 判定 inside/outside,不是手算 IsInsideRoundedRect。
// 原因:sub-100% DPI scale (e.g. dpr=0.667) 下,`CreateRoundRectRgn(0,0,W,H,r,r)` 实际 rgn
// 跟数学 r² 圆不完全一致(尤其在 corners 圆弧),导致 IsInsideRoundedRect 数学判断
// 比 GDI 的 rgn 更严格(认为 outside 的 pixel,GDI 反而认为 inside — 已被 FillRgn
// 填 panel bg)。直接用 PtInRegion(rgn, x, y) 完全匹配 GDI 实际填充区域。
void QuickPanelDialog::ApplyAlphaGradient() {
  if (!s_hBmpMem || s_panelW_phys <= 0 || s_panelH_phys <= 0) return;
  BITMAP bm = {};
  if (!GetObject(s_hBmpMem, sizeof(bm), &bm) || !bm.bmBits) return;

  DWORD* p = static_cast<DWORD*>(bm.bmBits);
  const int W = s_panelW_phys;
  const int H = s_panelH_phys;
  const int r = s_panelRadius_phys;
  const int Hminus1 = (H > 1) ? (H - 1) : 1;

  // 复用 CreateRoundRectRgn 创建的 region,让 GDI 自己判断 inside/outside
  HRGN panelRgn = CreateRoundRectRgn(0, 0, W, H, r, r);

  for (int y = 0; y < H; y++) {
    int aPanel = kAlphaPanelTop +
                 ((int)(kAlphaPanelBot - kAlphaPanelTop) * y / Hminus1);

    for (int x = 0; x < W; x++) {
      DWORD* px = &p[(size_t)y * W + x];
      if (!PtInRegion(panelRgn, x, y)) {
        *px = 0;
        continue;
      }
      BYTE r8 = (*px >> 16) & 0xFF;
      BYTE g8 = (*px >> 8)  & 0xFF;
      BYTE b8 =  *px        & 0xFF;
      // L87-fix: 阈值从 r >= 240 改成 r >= 130 + g >= 150 + b >= 180。
      // 之前 kBgTop = (255, 255, 255) 接近白,255 阈值 work。L87 改 kBgTop = (200, 225, 250)
      // (浅冷蓝),200 < 240,else 分支 (alpha=255) 生效,**gradient 完全没作用**。
      // 新阈值匹配 kBgBot = (155, 195, 240) — b8 >= 180 确保 panel bg 像素 (含冷蓝) 命中
      // gradient alpha 分支。
      if (r8 >= 130 && g8 >= 150 && b8 >= 180) {
        *px = ((DWORD)aPanel << 24) | (r8 << 16) | (g8 << 8) | b8;
      } else {
        *px = 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
      }
    }
  }
  DeleteObject(panelRgn);
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
    // (c) qp-state.txt = s_hoveredIdx / s_activeIdx / s_panelW_phys at paint time
    {
      HANDLE hf = CreateFileW(L"F:\\soft\\00selfmade\\rime_claude\\qp-state.txt",
                              GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (hf != INVALID_HANDLE_VALUE) {
        DWORD written;
        char mb[256];
        int len = _snprintf_s(mb, _TRUNCATE,
          "state: hovered=%d active=%d panelW=%d panelH=%d dpr=%.3f radius_phys=%d\n",
          s_hoveredIdx, s_activeIdx, s_panelW_phys, s_panelH_phys, s_dpr_x, s_panelRadius_phys);
        WriteFile(hf, mb, len, &written, nullptr);
        CloseHandle(hf);
      }
    }
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
  // L84-fix: 启动 hover polling timer (id=2, 100ms)。绕过 WS_EX_LAYERED 下 WM_MOUSEMOVE
  // 投递不可靠问题(L83 sandbox 验证)。Hide() 时 KillTimer。
  SetTimer(s_hwnd, 2, 100, NULL);
  RepaintLayered(s_hwnd);  // 第一次提交 layered surface (带渐变 alpha)
}

void QuickPanelDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, 1);
    KillTimer(s_hwnd, 2);  // L84-fix: kill hover polling timer
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
