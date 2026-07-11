#pragma once
//
// QuickPanelDialog v3-rev3 — GDI rewrite (spec 074 L75)
//
// 设计: docs/design/quickpanel-v3/index.html
// - 6 元素横排 (logo + 5 占位按钮)
// - 真火流猩 PNG (WIC → HBITMAP,无 GDI+ 无 IStream)
// - 5 个矢量图标 (GDI 路径 drawing,纯 MoveTo/LineTo)
// - hover 染品牌橙,active 橙→紫渐变(用 brush 替换)
// - uniform alpha 86% via WS_EX_LAYERED + LWA_ALPHA(220)
//
// 历史雷区:
// ❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)
// ❌ 绝对不 CreateStreamOnHGlobal + Release() (L67 根因)
// ❌ 绝对不用 D2D ID2D1HwndRenderTarget (L74 黑 panel,DComp 未 promote)
//
#include <functional>
#include <string>
#include <windows.h>

class QuickPanelDialog {
 public:
  using OnClick = std::function<void()>;
  using OnToggle = std::function<void(bool)>;

  enum class Mode { kHidden, kAlwaysShow };

  // Public API (与 D2D 版本完全兼容)
  static void Show(bool currentFullwidth,
                   OnClick onSchema,
                   OnClick onUserFolder,
                   OnClick onPhrases,
                   OnToggle onFullwidth,
                   OnClick onSymbols,
                   OnClick onLogin);
  static void Hide();
  static void ToggleMode();
  static void EnableAlwaysShowMode(
      OnClick onSchema,
      OnClick onUserFolder,
      OnClick onPhrases,
      OnToggle onFullwidth,
      OnClick onSymbols,
      OnClick onLogin);

  static Mode CurrentMode() { return s_mode; }
  static HWND ActiveHwnd() { return s_hwnd; }
  static int  CurrentAlpha() { return s_alpha; }
  static bool IsFullwidth() { return s_fullwidth; }

  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state (D2D 字段全部移除) =====
  static HWND     s_hwnd;
  static Mode     s_mode;
  static bool     s_fullwidth;
  static bool     s_mouseTracked;
  static int      s_alpha;
  static int      s_targetAlpha;
  static OnClick  s_onSchema;
  static OnClick  s_onUserFolder;
  static OnClick  s_onPhrases;
  static OnToggle s_onFullwidth;
  static OnClick  s_onSymbols;
  static OnClick  s_onLogin;

  // T005: hover/active 索引 (-1 = none)
  static int      s_hoveredIdx;
  static int      s_activeIdx;

  // ===== GDI 资源(无 D2D,无 GDI+) =====
  static HBITMAP  s_hBmpLogo;        // Fluxing logo PNG → HBITMAP
  static HBRUSH   s_hBrushPanelBg;   // panel 背景(白色半透)
  static HBRUSH   s_hBrushIconDim;   // 默认图标色(中灰)
  static HBRUSH   s_hBrushIconAccent;// hover 色(品牌橙)
  static HBRUSH   s_hBrushActive;    // active 背景(橙→紫)
  static HBRUSH   s_hBrushHighlight; // 顶部高光
  static HPEN     s_hPenIconDim;
  static HPEN     s_hPenIconAccent;
  static HPEN     s_hPenHighlight;
  static HDC       s_hdcMem;          // off-screen DC (避免闪烁)
  static HBITMAP  s_hBmpMem;         // off-screen bitmap
  static int       s_panelW_phys;     // panel 物理像素宽
  static int       s_panelH_phys;     // panel 物理像素高

  // 几何常量(物理像素,not logical)
  static constexpr int kPanelPadding = 8;
  static constexpr int kBtnSize      = 56;
  static constexpr int kIcoSize      = 30;
  static constexpr int kBtnRadius    = 14;
  static constexpr int kBrandSize    = 56;
  static constexpr int kPanelRadius  = 28;
  static constexpr int kPanelW       = 360;
  static constexpr int kPanelH       = 68;

  // 颜色(0xAABBGGRR)
  static constexpr COLORREF kBgTop    = RGB(255, 255, 255);  // Liquid Glass 顶部(高 alpha)
  static constexpr COLORREF kBgBot    = RGB(220, 220, 220);  // 底部稍暗
  static constexpr COLORREF kIconDim  = RGB(60, 60, 67);     // 灰
  static constexpr COLORREF kAccent   = RGB(255, 95, 49);    // 品牌橙
  static constexpr COLORREF kAccent2  = RGB(155, 81, 224);  // 品牌紫
  static constexpr COLORREF kHighlight = RGB(255, 255, 255); // 顶部高光
  static constexpr int kAlphaPanel  = 220;                  // 86% uniform

  // ===== Internal =====
  static LRESULT OnCreate(HWND);
  static LRESULT OnPaint(HWND);
  static LRESULT OnLButtonUp(HWND, int, int);
  static LRESULT OnLButtonDown(HWND, int, int);
  static void    OnMouseMove(HWND);
  static void    OnMouseLeave(HWND);
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnDestroy(HWND);
  static LRESULT OnKillFocus(HWND);
  static LRESULT OnKeyDown(HWND, WPARAM);

  // T002: WIC 解码 PNG (无 IStream,无 GDI+)
  static HRESULT LoadLogoWIC(HWND hwnd, HBITMAP& hBmpOut);

  // 命中测试(物理像素)
  static int HitTest(int x, int y);

  // off-screen DC setup
  static HRESULT CreateOffscreenDC(int w, int h);
  static void    DestroyOffscreenDC();

  // 5 个图标 drawing
  static void DrawIcon(HDC hdc, int idx, int x0, int y0);
  static void DrawIconSchema(HDC hdc, int x, int y);
  static void DrawIconPhrase(HDC hdc, int x, int y);
  static void DrawIconSymbols(HDC hdc, int x, int y);
  static void DrawIconSettings(HDC hdc, int x, int y);
  static void DrawIconAccount(HDC hdc, int x, int y);
};
