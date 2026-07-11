#pragma once
//
// QuickPanelDialog v3-rev3 (spec 070) — D2D-based rewrite.
//
// 设计: docs/design/quickpanel-v3/index.html (v3-rev3)
//      - 6 元素横排 (logo + 5 占位)
//      - 真火流猩 PNG (WIC 解码,非 GDI+ Bitmap/IStream)
//      - 5 个矢量图标 (ID2D1PathGeometry)
//      - hover 染品牌橙 + scale
//      - active 橙→紫渐变背景
//
// 历史雷区(plan.md 强制):
//   ❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)
//   ❌ 绝对不 CreateStreamOnHGlobal + Release() (L67 根因)
//   ✅ WIC 解码 → ID2D1Bitmap (无 IStream 缓存)
//
#include <functional>
#include <string>
#include <vector>
#include <windows.h>

// Forward declarations to keep header light
struct ID2D1Factory;
struct ID2D1RenderTarget;
struct ID2D1Bitmap;
struct ID2D1SolidColorBrush;
struct ID2D1LinearGradientBrush;
struct ID2D1PathGeometry;
struct ID2D1TransformedGeometry;

class QuickPanelDialog {
 public:
  using OnClick = std::function<void()>;
  using OnToggle = std::function<void(bool)>;

  enum class Mode { kHidden, kAlwaysShow };

  // T006: WeaselServerApp::Run 启动时调一次,注入共享 D2D factory。
  // 生命周期 = WeaselServerApp 生命周期,QuickPanelDialog 不 Release。
  static void InitializeD2D(ID2D1Factory* pFactory);
  static void ShutdownD2D();

  // Public API(保持向后兼容,L69 已在 4 个调用点 no-op 解禁,本 spec 解禁它们)
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

  // ===== Static state =====
  // WndProc / 回调访问
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

  // T001: D2D / WIC resources
  // 注意:不用 ComPtr,避免引入 <wrl/client.h> 头;手动 Release
  static ID2D1Factory*             s_pD2DFactory;
  // 基类指针:用 ID2D1RenderTarget* 而非 ID2D1HwndRenderTarget*,让 FillRoundedRect/DrawGeometry 等继承方法可见
  static ID2D1RenderTarget*        s_pRT;
  static ID2D1Bitmap*              s_pLogo;
  static ID2D1SolidColorBrush*     s_pBrushDim;       // hover 前灰
  static ID2D1SolidColorBrush*     s_pBrushAccent;    // hover 后品牌橙 #FF5F31
  static ID2D1SolidColorBrush*     s_pBrushPressed;   // active 白
  static ID2D1LinearGradientBrush* s_pBrushActive;    // active 橙→紫渐变
  static ID2D1LinearGradientBrush* s_pBrushHighlight; // 顶部高光 1px 渐变(transparent→white→transparent)
  static ID2D1LinearGradientBrush* s_pBrushPanel;     // L70-bugfix: panel 背景 0.55→0.32 alpha 渐变
  // 5 个图标几何:0=方案,1=短语,2=符号,3=设置,4=账号
  static ID2D1PathGeometry*        s_pIconGeometries[5];

  // 几何尺寸常量(spec 070)
  static constexpr int kPanelPadding = 8;
  static constexpr int kBtnSize      = 56;
  static constexpr int kIcoSize      = 38;
  static constexpr int kBtnRadius    = 14;
  static constexpr int kBrandSize    = 56;
  static constexpr int kPanelRadius  = 28;
  // 6 元素总宽:brand(56) + sep(4) + 5 buttons(56+2*5) = 56+4+280+10 = 350
  // 高: 56 + 12 padding = 68
  static constexpr int kPanelWidth   = 360;
  static constexpr int kPanelHeight  = 68;

  // 颜色常量
  static constexpr DWORD kBgTop      = 0x8CFFFFFF;  // rgba(255,255,255,0.55)
  static constexpr DWORD kBgBot      = 0x52FFFFFF;  // rgba(255,255,255,0.32)
  static constexpr DWORD kBorder     = 0x80FFFFFF;  // rgba(255,255,255,0.50)
  static constexpr DWORD kHighlight  = 0xD9FFFFFF;  // rgba(255,255,255,0.85)
  static constexpr DWORD kFgDim      = 0x8C3C3C43;  // rgba(60,60,67,0.55)
  static constexpr DWORD kAccent     = 0xFFFF5F31;  // #FF5F31
  static constexpr DWORD kAccent2    = 0xFF9B51E0;  // #9B51E0
  static constexpr DWORD kShadowCol  = 0x2E000000;  // rgba(0,0,0,0.18)

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

  // T002: WIC 解码 logo (无 IStream,无 GDI+)
  static HRESULT LoadLogoWIC();

  // T003: 创建 5 个 PathGeometry 图标
  static HRESULT CreateIconPaths();

  // T001: 创建/释放 D2D brushes
  static HRESULT CreateD2DResources(HWND hwnd);
  static void    ReleaseD2DResources();

  // 命中测试:返回 hover/active 索引(0=方案, 1=短语, 2=符号, 3=设置, 4=账号, -1=brand/无)
  static int HitTest(int x, int y);
};