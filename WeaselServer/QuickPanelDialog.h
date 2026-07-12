#pragma once
//
// QuickPanelDialog v3-rev3 — GDI rewrite + per-pixel alpha + WIC PNG
//
// 设计: docs/design/quickpanel-v3/index.html
// - 6 元素横排 (logo + 5 占位按钮)
// - 真火流猩 PNG:通过 WIC (IWICImagingFactory::CreateDecoderFromFilename) 解码
//   到 32-bit BGRA DIB Section。无 GDI+ Bitmap(IStream*) (L67-L69),无 IStream。
//   v0.19.0.10 之前的 LoadImageW(IMAGE_BITMAP, LR_LOADFROMFILE) 实际**不支持 PNG**,
//   静默失败 → 看到的是纯白 logo 区域(L81 修复)。
// - 5 个矢量图标 (GDI 路径 drawing,纯 MoveTo/LineTo)
// - hover 染品牌橙线条(active 才 fill bg,符合 v3-rev3 设计)
// - per-pixel alpha Liquid Glass via WS_EX_LAYERED + UpdateLayeredWindow +
//   32-bit DIBSection (BGRA),panel 内 alpha 顶 0x88→底 0x52 渐变,
//   圆角外 alpha=0 (桌面可见)
//
// 历史雷区 (不重复犯错):
// ❌ 绝对不用 GDI+ Bitmap(IStream*) (L67-L69 崩溃链)
// ❌ 绝对不 CreateStreamOnHGlobal + Release() (L67 根因)
// ❌ 绝对不用 D2D ID2D1HwndRenderTarget (L74 黑 panel,DComp 未 promote)
// ❌ 绝对不用 LWA_ALPHA uniform (L77 — 不是 liquid glass,只是 86% 全白)
// ❌ 绝对不在 WS_EX_LAYERED 后 BitBlt 到 window DC (双路径会闪烁)
// ❌ 绝对不用 LoadImageW(IMAGE_BITMAP) 加载 PNG (L81 — 支持列表不含 PNG,失败)
// ✅ 用 WIC CreateDecoderFromFilename → FormatConverter (32bppBGRA) →
//   CopyPixels 到 32-bit DIBSection
// ✅ PaintOpaqueContent 调用前 **必须 memset s_hBmpMem pBits 为 0** (L81) —
//   否则 active bg 残留导致 false 状态;UpdateLayeredWindow 提交的是增量,但 pBits
//   是 base raw,不 memset 会被旧位的 RGB/alpha 覆盖
// ✅ Show() 后 SetWindowPos(HWND_TOPMOST) 强制置顶 (L81 — WS_EX_LAYERED +
//   WS_EX_TOPMOST 路径 z-order 不稳,需要 explicit SetWindowPos)
// ✅ WM_ACTIVATEAPP handler:Hide() 在 app 失焦时 (L81 — 切到 en-US 时 panel
//   必须随 IME 切换消失,否则用户多次反映"卡死")
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
  static HBITMAP  s_hBmpLogo;        // Fluxing logo PNG → HBITMAP (WIC 解码)
  static HBRUSH   s_hBrushPanelBg;   // panel 背景(浅玻璃冷色)
  static HBRUSH   s_hBrushIconDim;   // 默认图标色(中灰)
  static HBRUSH   s_hBrushIconAccent;// hover 色(品牌橙)
  static HBRUSH   s_hBrushActive;    // active 背景(橙→紫)
  static HBRUSH   s_hBrushHighlight; // 顶部高光
  static HBRUSH   s_hBrushShadow;    // 底部阴影 (L83 mac 风格)
  static HPEN     s_hPenIconDim;
  static HPEN     s_hPenIconAccent;
  static HPEN     s_hPenHighlight;
  static HDC       s_hdcMem;          // off-screen DC (避免闪烁)
  static HBITMAP  s_hBmpMem;         // off-screen bitmap (32-bit BGRA)
  static int       s_panelW_phys;     // panel 物理像素宽
  static int       s_panelH_phys;     // panel 物理像素高

  // L83-fix: DPI 缩放(scale 全部几何常量到 physical pixels)
  // 之前 v0.19.0.0~0.12 直接用 logical kPanelW=360 / kBtnSize=56 等 GDI 坐标,在 sandbox
  // 显示器为 sub-100% DPI scale (Windows 自动缩放窗口到 240x45 physical) 时,
  // 所有 drawing 用 logical 360 但 surface 只有 240 wide → icons 2-4 出界、HitTest
  // 错位、logo 看不见。修:dpr_x = s_panelW_phys / kPanelW,所有 _phys 常量按 dpr
  // 缩放(整除,>=1)。
  static float     s_dpr_x;            // panel dpr x = s_panelW_phys / kPanelW
  static float     s_dpr_y;            // panel dpr y = s_panelH_phys / kPanelH
  static int       s_panelPadding_phys;
  static int       s_btnSize_phys;
  static int       s_icoSize_phys;
  static int       s_btnRadius_phys;
  static int       s_brandSize_phys;
  static int       s_panelRadius_phys;
  static int       s_btnGap_phys;      // 按钮之间 2px 间距

  // L87-fix: 手动 drag 状态 — 当 s_dragging = true 时,WM_MOUSEMOVE 把 panel
  // 移动到 s_dragStartWindow + cursor 当前位置 offset。
  static bool      s_dragging;          // 是否正在拖动
  static POINT     s_dragStartCursor;   // 拖动开始时 cursor screen pos
  static RECT      s_dragStartWindow;   // 拖动开始时 window screen pos
  static int       s_outsideMs;         // L89-fix: 鼠标在 panel 外的累计毫秒(>=1500→Hide)

  // 设计几何常量 (logical pixels, design — 360x68 panel)
  // 注意:**不要**直接用这些 GDI 坐标;用 _phys 等版本(运行时按 dpr 缩放)。
  // L88-fix: panel 整体 70% (v0.19.0.16 是 60% — user 反馈"宽度偏小,右侧图标离
  // 右边框过近,图标间距也小")。同时增大 kBtnGap 让按钮之间间距更明显。
  // 70% 比例: 360*0.7=252, 68*0.7=47.6→48
  // 56*0.7=39.2→39, 30*0.7=21, 14*0.7=9.8→10, 8*0.7=5.6→5
  // 28*0.7=19.6→20
  // L88-fix2: kBtnGap 改 2 (从 1 增大)— user 反馈"图标间距紧凑,影响美观"
  // 5*39 + 4*2 = 195+8 = 203 + buttonStartX(5+39+4=48) = 251 ≤ 252 panel right (ok)
  static constexpr int kPanelPadding = 5;
  static constexpr int kBtnSize      = 39;
  static constexpr int kBtnGap       = 2;
  static constexpr int kIcoSize      = 21;
  static constexpr int kBtnRadius    = 10;
  static constexpr int kBrandSize    = 39;
  static constexpr int kPanelRadius  = 20;
  static constexpr int kPanelW       = 252;
  static constexpr int kPanelH       = 48;

  // 颜色(0xAABBGGRR)
  // L87-fix: 渐变 加强 — user 反馈"渐变效果不够明显"。L86 RGB(255,255,255)→(180,200,230)
  // 在白背景 alpha gradient (140→82) 下 composite 后 几乎都接近白,肉眼分辨不出渐变。
  // 修复: 改用**更饱和的玻璃色 + 更宽的 alpha 范围**:
  //   顶 (200, 225, 250) 浅冷蓝(肉眼明显蓝,但不刺眼)
  //   底 (155, 195, 240) 略深浅蓝(玻璃冷调底部)
  //   alpha 范围 80-220 (之前 82-140 太窄) — gradient 视觉差 140 step
  // composite 后顶 (196,220,247)→底(180,202,236) 差异 16 step,渐变明显可见。
  static constexpr COLORREF kBgTop    = RGB(200, 225, 250);  // 浅冷蓝顶
  static constexpr COLORREF kBgBot    = RGB(155, 195, 240);  // 略深浅蓝底
  static constexpr COLORREF kIconDim  = RGB(60, 60, 67);     // 灰(legacy alias)
  static constexpr COLORREF kAccent   = RGB(255, 95, 49);    // 品牌橙
  static constexpr COLORREF kAccent2  = RGB(155, 81, 224);  // 品牌紫
  static constexpr COLORREF kHighlight = RGB(255, 255, 255); // 顶部高光

  // v0.19.0.10: per-pixel alpha gradient (替换 L77 uniform kAlphaPanel=220)
  // L87-fix: 顶部 220 (0xDC = 86%) → 底部 80 (0x50 = 31%) — 范围扩大 140 step,
  // 让渐变视觉更明显。之前 140-82 范围 58 step,在白背景下肉眼分辨不出。
  // 圆角外 alpha = 0 (桌面可见)
  static constexpr BYTE kAlphaPanelTop = 220;
  static constexpr BYTE kAlphaPanelBot = 80;

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
  // L86-fix: 增加 COLORREF 参数传入 pen 颜色,让 hover/active 状态机生效。
  // 之前 v0.19.0.15 DrawIcon* hardcoded kIcoDimC (灰) 画 pen,完全忽略 caller
  // 选的 pen(hover 橙 / active 白)。L85-fix 没生效就是这个原因。
  static void DrawIcon(HDC hdc, int idx, int x0, int y0, COLORREF penColor);
  static void DrawIconSchema(HDC hdc, int x, int y, COLORREF penColor);
  static void DrawIconPhrase(HDC hdc, int x, int y, COLORREF penColor);
  static void DrawIconSymbols(HDC hdc, int x, int y, COLORREF penColor);
  static void DrawIconSettings(HDC hdc, int x, int y, COLORREF penColor);
  static void DrawIconAccount(HDC hdc, int x, int y, COLORREF penColor);

  // L81: COM 一次性初始化(WIC 创建需要 STA)。返回 S_OK 表示已初始化,
  // S_FALSE 表示已初始化过(无需重复),失败错误码需要退出。我们仅 initialize
  // 一次,失败也不致命 — LoadLogoWIC 会无 logo 显示但 panel 仍可工作。
  static HRESULT EnsureComInit();

  // v0.19.0.10: per-pixel alpha pipeline (L77 per lessons-learned "Future work")
  // ApplyAlphaGradient 扫描 s_hBmpMem 的 pBits,按 y 轴改 alpha 通道。
  // - 圆角 panel 外: alpha = 0 (桌面可见)
  // - 圆角 panel 内,RGB == 纯白 (panel bg / border / top highlight): 渐变 alpha 写入
  // - 圆角 panel 内,RGB 含色 (icons / logo / active bg): 保留 GDI 默认 alpha=255
  // 为什么按 RGB 区分? GDI Brush 不带 alpha 通道,32-bit DIB 上画出来一律 alpha=255。
  // 重画 bg 渐变 alpha 不破坏 icons / logo 的形状 (RGB ≠ 纯白)。
  static void    ApplyAlphaGradient();
  // RepaintLayered: 调 PaintOpaqueContent 到 s_hdcMem + ApplyAlphaGradient +
  //   UpdateLayeredWindow 把 layered surface 提交到 screen。
  // PaintOpaqueContent 抽出来是为了让 OnPaint / Show() 都能复用同一份绘制逻辑。
  static void    PaintOpaqueContent(HDC hdc);
  static void    RepaintLayered(HWND hwnd);
};
