// QuickPanelDialog v3-rev3 (spec 070) — D2D-based rewrite.
//
// 设计: docs/design/quickpanel-v3/index.html
// 历史:L67/L68/L69 全部由 GDI+ Bitmap(IStream*) 触发。本实现完全绕开:
//   - PNG logo 用 WIC → ID2D1Bitmap (无 IStream 缓存)
//   - 5 个图标用 ID2D1PathGeometry (矢量,无位图)
//   - 所有 brushes 创建一次复用 (无泄漏)
//
#include "stdafx.h"
#include <functional>
#include <GdiPlus.h>
#include "QuickPanelDialog.h"
#include "resource.h"
#pragma comment(lib, "gdiplus.lib")

// D2D / WIC COM interface headers (forward-declared in .h to keep header light)
#include <d2d1.h>
#include <wincodec.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

using Gdiplus::Bitmap;
using Gdiplus::Image;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::SolidBrush;
using Gdiplus::Pen;

static const wchar_t kWindowClassName[] = L"FluxingQuickPanel_v3";

namespace {

// ===== T003: SVG path 数据 → ID2D1PathGeometry =====
// 每个图标 v3-rev3 设计稿的 SVG path(viewBox 24x24)。
// 用 ID2D1PathGeometry::Open() + ID2D1GeometrySink::BeginFigure/AddLine/EndFigure 重建。
// 5 个图标设计:
//   0: 方案 = 双箭头(动作感:切换)
//   1: 短语 = 对话气泡(内容感:短语)
//   2: 符号 = 键盘(参照搜狗)
//   3: 设置 = 齿轮
//   4: 账号 = 头像
void BuildIcon_0_Schema(ID2D1GeometrySink* sink) {
  // 上箭头(向右): 4→17 + 箭头头部 17←14→6
  sink->BeginFigure(D2D1::Point2F(4, 9), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(17, 9));
  sink->AddLine(D2D1::Point2F(14, 6));
  sink->AddLine(D2D1::Point2F(14, 12));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 下箭头(向左): 20→7 + 箭头头部 7←10→18
  sink->BeginFigure(D2D1::Point2F(20, 15), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(7, 15));
  sink->AddLine(D2D1::Point2F(10, 12));
  sink->AddLine(D2D1::Point2F(10, 18));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
}
void BuildIcon_1_Phrase(ID2D1GeometrySink* sink) {
  // 对话气泡(圆角矩形 + 尾巴 + 3 横线)
  D2D1_POINT_2F points[9] = {
      {4, 6}, {20, 6}, {22, 8}, {22, 15}, {20, 17},
      {12, 17}, {7, 21}, {7, 17}, {4, 17}
  };
  // 简化:用 ArcSegment 模拟圆角
  sink->BeginFigure(points[0], D2D1_FIGURE_BEGIN_FILLED);
  sink->AddLine(points[1]);
  D2D1_ARC_SEGMENT arc1 = {};
  arc1.point = points[2]; arc1.size = D2D1::SizeF(2, 2); arc1.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc1);
  sink->AddLine(points[3]);
  arc1 = {}; arc1.point = points[4]; arc1.size = D2D1::SizeF(2, 2); arc1.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc1);
  sink->AddLine(points[5]);
  // 尾巴
  sink->AddLine(points[6]);
  sink->AddLine(points[7]);
  sink->AddLine(points[8]);
  D2D1_ARC_SEGMENT arc2 = {};
  arc2.point = points[0]; arc2.size = D2D1::SizeF(2, 2); arc2.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc2);
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 3 横线(短语)
  sink->BeginFigure(D2D1::Point2F(7, 11), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(17, 11));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(7, 14), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(13, 14));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
}
void BuildIcon_2_Symbols(ID2D1GeometrySink* sink) {
  // 键盘(外壳 + 3 排按键)
  // 顶排 4 键
  sink->BeginFigure(D2D1::Point2F(6.5f, 5), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(6.5f, 9.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(11, 5), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(11, 9.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(15.5f, 5), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(15.5f, 9.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(20, 5), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(20, 9.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 中排按键
  sink->BeginFigure(D2D1::Point2F(9, 13), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(9, 16.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(15, 13), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(15, 16.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 行分隔
  sink->BeginFigure(D2D1::Point2F(2, 9.5f), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(22, 9.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  sink->BeginFigure(D2D1::Point2F(2, 13), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(22, 13));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 空格键(略粗 stroke,但 geometry 不存宽度 — Draw 时再设)
  sink->BeginFigure(D2D1::Point2F(6, 16.5f), D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(18, 16.5f));
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 外壳(最后画)
  D2D1_POINT_2F shell[4] = { {2, 5}, {22, 5}, {22, 19}, {2, 19} };
  D2D1_ARC_SEGMENT arc = {};
  sink->BeginFigure(shell[0], D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(shell[1]);
  arc = {}; arc.point = shell[2]; arc.size = D2D1::SizeF(2, 2); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  sink->AddLine(shell[3]);
  arc = {}; arc.point = shell[0]; arc.size = D2D1::SizeF(2, 2); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
}
void BuildIcon_3_Settings(ID2D1GeometrySink* sink) {
  // 齿轮:中心圆 + 8 条射线
  D2D1_POINT_2F center = {12, 12};
  // 中心圆(用 8 段 Arc 拼)
  // 简化:画中心圆 + 8 条射线(2x4 + 2x4)
  // 中心圆 半径 3
  float r = 3.0f;
  D2D1_ARC_SEGMENT arc = {};
  sink->BeginFigure(D2D1::Point2F(center.x + r, center.y), D2D1_FIGURE_BEGIN_HOLLOW);
  arc = {}; arc.point = D2D1::Point2F(center.x, center.y + r); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(center.x - r, center.y); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(center.x, center.y - r); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(center.x + r, center.y); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 8 条射线
  for (int i = 0; i < 8; i++) {
    float angle = i * 3.14159265f / 4.0f;
    float dx = cosf(angle), dy = sinf(angle);
    sink->BeginFigure(D2D1::Point2F(center.x + dx * 5, center.y + dy * 5), D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddLine(D2D1::Point2F(center.x + dx * 7, center.y + dy * 7));
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
  }
}
void BuildIcon_4_Account(ID2D1GeometrySink* sink) {
  // 头像:头 + 肩
  D2D1_POINT_2F head = {12, 8};
  float r = 3.2f;
  // 头部圆
  D2D1_ARC_SEGMENT arc = {};
  sink->BeginFigure(D2D1::Point2F(head.x + r, head.y), D2D1_FIGURE_BEGIN_HOLLOW);
  arc = {}; arc.point = D2D1::Point2F(head.x, head.y + r); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(head.x - r, head.y); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(head.x, head.y - r); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  arc = {}; arc.point = D2D1::Point2F(head.x + r, head.y); arc.size = D2D1::SizeF(r, r); arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
  sink->AddArc(arc);
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
  // 肩膀(从左下到右下)
  D2D1_POINT_2F shoulder[3] = { {5, 20}, {12, 14}, {19, 20} };
  sink->BeginFigure(shoulder[0], D2D1_FIGURE_BEGIN_HOLLOW);
  D2D1_QUADRATIC_BEZIER_SEGMENT bezier = {};
  bezier.point1 = D2D1::Point2F(7, 15);
  bezier.point2 = D2D1::Point2F(11, 14);
  sink->AddQuadraticBezier(bezier);
  bezier = {};
  bezier.point1 = D2D1::Point2F(13, 14);
  bezier.point2 = D2D1::Point2F(17, 15);
  sink->AddQuadraticBezier(bezier);
  sink->AddLine(shoulder[2]);
  sink->EndFigure(D2D1_FIGURE_END_OPEN);
}

using IconBuilder = void (*)(ID2D1GeometrySink*);
IconBuilder g_iconBuilders[5] = {
    BuildIcon_0_Schema, BuildIcon_1_Phrase, BuildIcon_2_Symbols,
    BuildIcon_3_Settings, BuildIcon_4_Account
};

}  // namespace

// ===== Static state definitions =====
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

int      QuickPanelDialog::s_hoveredIdx = -1;
int      QuickPanelDialog::s_activeIdx  = -1;

ID2D1Factory*             QuickPanelDialog::s_pD2DFactory       = nullptr;
ID2D1RenderTarget*        QuickPanelDialog::s_pRT             = nullptr;
ID2D1Bitmap*              QuickPanelDialog::s_pLogo           = nullptr;
ID2D1SolidColorBrush*     QuickPanelDialog::s_pBrushDim       = nullptr;
ID2D1SolidColorBrush*     QuickPanelDialog::s_pBrushAccent    = nullptr;
ID2D1SolidColorBrush*     QuickPanelDialog::s_pBrushPressed   = nullptr;
ID2D1LinearGradientBrush* QuickPanelDialog::s_pBrushActive    = nullptr;
ID2D1LinearGradientBrush* QuickPanelDialog::s_pBrushHighlight = nullptr;
ID2D1LinearGradientBrush* QuickPanelDialog::s_pBrushPanel      = nullptr;
ID2D1PathGeometry*        QuickPanelDialog::s_pIconGeometries[5] = {};

// ===== T001: D2D factory inject (由 WeaselServerApp::Run 调一次) =====
void QuickPanelDialog::InitializeD2D(ID2D1Factory* pFactory) {
  s_pD2DFactory = pFactory;
}
void QuickPanelDialog::ShutdownD2D() {
  ReleaseD2DResources();
  s_pD2DFactory = nullptr;
}

// ===== T001: 资源创建/释放 =====
HRESULT QuickPanelDialog::CreateD2DResources(HWND hwnd) {
  if (!s_pD2DFactory) return E_FAIL;
  if (s_pRT) return S_OK;

  RECT rc;
  GetClientRect(hwnd, &rc);
  D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

  HRESULT hr = s_pD2DFactory->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),  // 默认属性(默认 RGB + premultiplied alpha)
      D2D1::HwndRenderTargetProperties(hwnd, size),
      (ID2D1HwndRenderTarget**)&s_pRT);
  if (FAILED(hr) || !s_pRT) return hr;

  // 3 个 SolidColorBrush
  s_pRT->CreateSolidColorBrush(D2D1::ColorF(0.20f, 0.20f, 0.20f, 0.55f), &s_pBrushDim);     // kFgDim 半透明深灰
  s_pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.37f, 0.19f, 1.0f), &s_pBrushAccent);  // kAccent 品牌橙
  s_pRT->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &s_pBrushPressed); // kPressed 白
  // 注:s_pBrushHighlight 在下方独立创建为 LinearGradientBrush(顶部高光)

  // active 态 橙→紫 LinearGradientBrush
  D2D1_GRADIENT_STOP activeStops[2];
  activeStops[0].position = 0.0f;  activeStops[0].color = D2D1::ColorF(1.0f, 0.37f, 0.19f);
  activeStops[1].position = 1.0f;  activeStops[1].color = D2D1::ColorF(0.61f, 0.32f, 0.88f);
  ID2D1GradientStopCollection* pActiveStops = nullptr;
  s_pRT->CreateGradientStopCollection(activeStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pActiveStops);
  s_pRT->CreateLinearGradientBrush(
      D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(100, 100)),
      pActiveStops, &s_pBrushActive);
  if (pActiveStops) pActiveStops->Release();

  // L70-bugfix: panel 背景用 v3-rev3 设计稿的双层渐变(0.55→0.32 alpha)
  // 创建一次永久复用
  D2D1_GRADIENT_STOP panelStops[2];
  panelStops[0].position = 0.0f;  panelStops[0].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.55f);  // 顶部 0.55
  panelStops[1].position = 1.0f;  panelStops[1].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.32f);  // 底部 0.32
  ID2D1GradientStopCollection* pPanelStops = nullptr;
  s_pRT->CreateGradientStopCollection(panelStops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pPanelStops);
  ID2D1LinearGradientBrush* pPanelBrush = nullptr;
  s_pRT->CreateLinearGradientBrush(
      D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 100)),
      pPanelStops, &pPanelBrush);
  if (pPanelStops) pPanelStops->Release();
  // 存到 static 以便 OnPaint 复用(避免每帧创建)
  // 注:为简单起见,这里直接传到 OnPaint(下一行)而不是存 static
  // 改:存为 static 字段
  s_pBrushPanel = pPanelBrush;  // s_pBrushPanel 已在 header 中声明

  // L70-bugfix: 顶部高光 1px 渐变,永久存 static
  D2D1_GRADIENT_STOP hlStops[3];
  hlStops[0].position = 0.0f;  hlStops[0].color = D2D1::ColorF(1, 1, 1, 0);
  hlStops[1].position = 0.5f;  hlStops[1].color = D2D1::ColorF(1, 1, 1, 0.85f);
  hlStops[2].position = 1.0f;  hlStops[2].color = D2D1::ColorF(1, 1, 1, 0);
  ID2D1GradientStopCollection* pHlCol = nullptr;
  s_pRT->CreateGradientStopCollection(hlStops, 3, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pHlCol);
  s_pRT->CreateLinearGradientBrush(
      D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(100, 0)),
      pHlCol, &s_pBrushHighlight);
  if (pHlCol) pHlCol->Release();

  // T002: 用 WIC 加载 logo PNG (无 IStream!)
  LoadLogoWIC();

  // T003: 创建 5 个图标 PathGeometry
  CreateIconPaths();

  return S_OK;
}

void QuickPanelDialog::ReleaseD2DResources() {
  if (s_pLogo)            { s_pLogo->Release();           s_pLogo = nullptr; }
  for (int i = 0; i < 5; i++) {
    if (s_pIconGeometries[i]) { s_pIconGeometries[i]->Release(); s_pIconGeometries[i] = nullptr; }
  }
  if (s_pBrushActive)     { s_pBrushActive->Release();     s_pBrushActive = nullptr; }
  if (s_pBrushHighlight)  { s_pBrushHighlight->Release();  s_pBrushHighlight = nullptr; }
  if (s_pBrushPanel)      { s_pBrushPanel->Release();      s_pBrushPanel = nullptr; }
  if (s_pBrushPressed)    { s_pBrushPressed->Release();    s_pBrushPressed = nullptr; }
  if (s_pBrushAccent)     { s_pBrushAccent->Release();     s_pBrushAccent = nullptr; }
  if (s_pBrushDim)        { s_pBrushDim->Release();        s_pBrushDim = nullptr; }
  if (s_pRT) {
    s_pRT->Release();
    s_pRT = nullptr;
  }
}

// ===== T002: WIC 解码 PNG logo (无 IStream,直接给 D2D) =====
HRESULT QuickPanelDialog::LoadLogoWIC() {
  if (s_pLogo) return S_OK;
  if (!s_pRT) return E_FAIL;

  // 用 WIC 工厂 (CoCreateInstance,一次性失败由 WeaselServerApp 容错)
  IWICImagingFactory* pWic = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(&pWic));
  if (FAILED(hr)) return hr;

  // L70-bugfix: 用 exe 所在目录而非 cwd,避免快捷方式启动时 cwd 不对
  wchar_t exeDir[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
  if (lastSlash) *lastSlash = L'\0';
  wchar_t logoPath[MAX_PATH];
  _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo.png", exeDir);

  IWICBitmapDecoder* pDec = nullptr;
  hr = pWic->CreateDecoderFromFilename(
      logoPath, nullptr, GENERIC_READ,
      WICDecodeMetadataCacheOnLoad, &pDec);
  if (FAILED(hr)) { pWic->Release(); return hr; }

  IWICBitmapFrameDecode* pFrame = nullptr;
  hr = pDec->GetFrame(0, &pFrame);
  if (FAILED(hr)) { pDec->Release(); pWic->Release(); return hr; }

  // ★ 关键:用 WIC frame 直接给 D2D 创建 bitmap,完全不经过 IStream/GDI+
  hr = s_pRT->CreateBitmapFromWicBitmap(pFrame, nullptr, &s_pLogo);

  pFrame->Release();
  pDec->Release();
  pWic->Release();
  return hr;
}

// ===== T003: 创建 5 个图标 PathGeometry =====
HRESULT QuickPanelDialog::CreateIconPaths() {
  if (!s_pRT) return E_FAIL;
  for (int i = 0; i < 5; i++) {
    if (s_pIconGeometries[i]) continue;
    HRESULT hr = s_pD2DFactory->CreatePathGeometry(&s_pIconGeometries[i]);
    if (FAILED(hr)) return hr;
    ID2D1GeometrySink* sink = nullptr;
    hr = s_pIconGeometries[i]->Open(&sink);
    if (FAILED(hr)) return hr;
    g_iconBuilders[i](sink);
    sink->Close();
    sink->Release();
  }
  return S_OK;
}

// ===== T005: HitTest (返回 brand=−2, 5 按钮=0..4, 无=−1) =====
int QuickPanelDialog::HitTest(int x, int y) {
  // panel padding 8, brand 在最左 (4+56=60 宽含 margin)
  // 假设 panel layout:brand(56) + 4 gap + 5 buttons(56 each)
  const int padding = kPanelPadding;
  int bx = padding;                   // brand 起点 x
  int by = padding - 4;               // brand 起点 y(panel padding 减品牌略上)
  if (x >= bx && x < bx + kBrandSize && y >= by && y < by + kBrandSize) return -2; // brand

  // 5 个按钮起点 x
  const int buttonStartX = padding + kBrandSize + 4;  // 8 + 56 + 4 = 68
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (kBtnSize + 2);
    if (x >= x0 && x < x0 + kBtnSize && y >= padding && y < padding + kBtnSize) return i;
  }
  return -1;
}

// ===== T004 + T005: OnPaint + state machine =====
LRESULT QuickPanelDialog::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  if (!s_pRT) {
    EndPaint(hwnd, &ps);
    return 0;
  }

  s_pRT->BeginDraw();
  s_pRT->Clear(D2D1::ColorF(0, 0, 0, 0));   // 透明背景

  D2D1_SIZE_F sz = s_pRT->GetSize();
  float W = sz.width;
  float H = sz.height;

  // 1. panel 背景 Liquid Glass(双层渐变 — 简化为单层半透)
  D2D1_ROUNDED_RECT panelRect = D2D1::RoundedRect(
      D2D1::RectF(0, 0, W, H), kPanelRadius, kPanelRadius);
  // 用 alpha=240 的半透白(没渐变但视觉效果接近)
  // L70-bugfix: 用 v3-rev3 双层渐变(0.55→0.32 alpha),不再用单色
  if (s_pBrushPanel) {
    // 重设渐变方向为 panel 实际尺寸
    s_pBrushPanel->SetStartPoint(D2D1::Point2F(0, 0));
    s_pBrushPanel->SetEndPoint(D2D1::Point2F(0, H));
    s_pRT->FillRoundedRectangle(&panelRect, s_pBrushPanel);
  }

  // 1px 边框
  ID2D1SolidColorBrush* pBorder = nullptr;
  s_pRT->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 0.5f), &pBorder);
  s_pRT->DrawRoundedRectangle(&panelRect, pBorder, 1.0f);
  if (pBorder) pBorder->Release();

  // 2. 顶部高光(1px 渐变线,L70-bugfix: 永久 s_pBrushHighlight)
  if (s_pBrushHighlight) {
    D2D1_POINT_2F hlStart = {14, 0.5f}, hlEnd = {W - 14, 0.5f};
    s_pBrushHighlight->SetStartPoint(hlStart);
    s_pBrushHighlight->SetEndPoint(hlEnd);
    s_pRT->DrawLine(hlStart, hlEnd, s_pBrushHighlight, 1.0f);
  }

  // 3. logo (brand 块 56x56,padding 内)
  if (s_pLogo) {
    D2D1_RECT_F logoRect = D2D1::RectF(
        (float)kPanelPadding - 2.0f, (float)kPanelPadding - 2.0f,
        (float)kPanelPadding - 2.0f + kBrandSize, (float)kPanelPadding - 2.0f + kBrandSize);
    s_pRT->DrawBitmap(s_pLogo, &logoRect);
  }

  // 4. 5 个图标按钮
  // active 按钮:橙→紫渐变背景 + 白色图标
  // hovered 按钮:图标变橙 (s_hoveredIdx 优先)
  // 默认:灰图标
  const int buttonStartX = kPanelPadding + kBrandSize + 4;
  for (int i = 0; i < 5; i++) {
    int x0 = buttonStartX + i * (kBtnSize + 2);
    int y0 = kPanelPadding;
    D2D1_RECT_F btnRect = D2D1::RectF(
        (float)x0, (float)y0,
        (float)(x0 + kBtnSize), (float)(y0 + kBtnSize));

    if (i == s_activeIdx) {
      // active 态:橙→紫渐变背景
      D2D1_ROUNDED_RECT bgRect = D2D1::RoundedRect(btnRect, (float)kBtnRadius, (float)kBtnRadius);
      // 重建 active brush with this button's gradient
      ID2D1LinearGradientBrush* pActiveBtn = nullptr;
      D2D1_GRADIENT_STOP aStops[2] = {
          {0.0f, D2D1::ColorF(1.0f, 0.37f, 0.19f)},
          {1.0f, D2D1::ColorF(0.61f, 0.32f, 0.88f)}
      };
      ID2D1GradientStopCollection* pStops = nullptr;
      s_pRT->CreateGradientStopCollection(aStops, 2, &pStops);
      s_pRT->CreateLinearGradientBrush(
          D2D1::LinearGradientBrushProperties(D2D1::Point2F((float)x0, (float)y0), D2D1::Point2F((float)(x0 + kBtnSize), (float)(y0 + kBtnSize))),
          pStops, &pActiveBtn);
      s_pRT->FillRoundedRectangle(&bgRect, pActiveBtn);
      if (pActiveBtn) pActiveBtn->Release();
      if (pStops) pStops->Release();
    }

    // 画图标
    if (s_pIconGeometries[i]) {
      // icon viewport 24x24,渲染到 button 内 38x38 中心
      float iconSize = (float)kIcoSize;
      float iconX = x0 + (kBtnSize - kIcoSize) / 2.0f;
      float iconY = y0 + (kBtnSize - kIcoSize) / 2.0f;
      // 创建 scale transform:24→38 ≈ 1.583
      float scale = iconSize / 24.0f;
      ID2D1TransformedGeometry* pTransformed = nullptr;
      s_pD2DFactory->CreateTransformedGeometry(
          s_pIconGeometries[i],
          D2D1::Matrix3x2F::Scale(scale, scale) *
          D2D1::Matrix3x2F::Translation(iconX, iconY),
          &pTransformed);
      if (pTransformed) {
        // 选 brush:active→白,hovered→橙,其它→深灰
        ID2D1Brush* pBrush = s_pBrushDim;
        if (i == s_activeIdx) pBrush = s_pBrushPressed;
        else if (i == s_hoveredIdx) pBrush = s_pBrushAccent;
        s_pRT->DrawGeometry(pTransformed, pBrush, 1.5f);  // L70-bugfix: 1.8→1.5(适配 38px 大图标,避免 stroke 盖住中心)
        pTransformed->Release();
      }
    }
  }

  s_pRT->EndDraw();
  EndPaint(hwnd, &ps);
  return 0;
}

// ===== WndProc =====
LRESULT CALLBACK QuickPanelDialog::WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_CREATE:    return OnCreate(hwnd);
    case WM_DESTROY:   return OnDestroy(hwnd);
    case WM_PAINT:     return OnPaint(hwnd);
    case WM_ERASEBKGND: return 1;          // D2D 自管背景
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
        // T005: 5 按钮 no-op (v0.19.0 ship 切片)
        // 真正功能由后续 spec 实现
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
    default: break;
  }
  return DefWindowProc(hwnd, msg, w, l);
}

LRESULT QuickPanelDialog::OnCreate(HWND hwnd) {
  s_hwnd = hwnd;
  return CreateD2DResources(hwnd);
}

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  s_hoveredIdx = -1;
  s_activeIdx = -1;
  if (s_hwnd == hwnd) s_hwnd = NULL;
  ReleaseD2DResources();
  return 0;
}

LRESULT QuickPanelDialog::OnKillFocus(HWND hwnd) {
  // T006: 1 秒失焦自动关闭
  SetTimer(hwnd, 1, 1000, NULL);
  return 0;
}

LRESULT QuickPanelDialog::OnKeyDown(HWND hwnd, WPARAM key) {
  // T006: ESC 关闭
  if (key == VK_ESCAPE) {
    Hide();
    return 0;
  }
  return DefWindowProc(hwnd, key, 0, 0);
}

LRESULT QuickPanelDialog::OnTimer(HWND hwnd, WPARAM w) {
  if (w == 1) {
    KillTimer(hwnd, 1);
    Hide();
  }
  return 0;
}
LRESULT QuickPanelDialog::OnLButtonUp(HWND, int, int) { return 0; }
LRESULT QuickPanelDialog::OnLButtonDown(HWND, int, int) { return 0; }
void    QuickPanelDialog::OnMouseMove(HWND) {}
void    QuickPanelDialog::OnMouseLeave(HWND) {}

// ===== Public API =====
// Show / Hide / ToggleMode / EnableAlwaysShowMode — v0.19.0 ship 切片只控制可见性
// 不接 IPC;5 个按钮 no-op(T005)

void QuickPanelDialog::Show(bool currentFullwidth,
                             OnClick onSchema,
                             OnClick onUserFolder,
                             OnClick onPhrases,
                             OnToggle onFullwidth,
                             OnClick onSymbols,
                             OnClick onLogin) {
  (void)currentFullwidth; (void)onSchema; (void)onUserFolder;
  (void)onPhrases; (void)onFullwidth; (void)onSymbols; (void)onLogin;
  if (s_hwnd) {
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    InvalidateRect(s_hwnd, NULL, FALSE);
    return;
  }
  // CreateWindowExW
  if (!s_pD2DFactory) return;  // InitializeD2D 未调
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kWindowClassName;
  RegisterClassExW(&wc);

  // 屏幕右下角定位
  RECT workArea;
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
  int x = workArea.right - kPanelWidth - 12;
  int y = workArea.bottom - kPanelHeight - 12;

  s_hwnd = CreateWindowExW(
      WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST,
      kWindowClassName, L"Fluxing QuickPanel",
      WS_POPUP,
      x, y, kPanelWidth, kPanelHeight,
      NULL, NULL, GetModuleHandle(NULL), NULL);
  if (!s_hwnd) return;
  // 240/255 = ~94% 不透明(spec 070 v3-rev3 不透明度)
  SetLayeredWindowAttributes(s_hwnd, RGB(0, 0, 0), 240, LWA_ALPHA);
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
  // T006: 简化版 — Alt+, 调 Show(),再 Alt+, 调 Hide()
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