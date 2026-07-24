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

  // v0.19.0.29(spec 070 T007 follow-up + mockups-v0.19.0.28 设计稿):
  // 5 按钮中 0/4 仍 no-op (历史 placeholder,follow-up spec 加 ASCII mode toggle
  // / 登录);1 Phrase / 3 Shortcut (NEW)。
  // Shortcut 通过 SetOn* setter 注入,与现有 s_onPhrases (Show 时传) 路径平行。
  // v0.19.0.61 (Phase L 调整 1.1): OnShowUserDict / SetOnUserDict 整体删除 —
  // UserDictionary 模块下线,button 2 完全 invisible + no-op。
  using OnShowShortcut = std::function<void()>;
  static void SetOnShortcut(OnShowShortcut fn);

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
  // v0.19.0.29:hit==3 → Shortcut。Setter 注入(与 Show() 7 参 路径并存;
  // Show 路径的 onSymbols/onLogin 仍是 placeholder)。
  // v0.19.0.60 (Phase L 调整 1): s_onUserDict 删除 — UserDictionary 模块下线。
  // hit==2 (UserDict button) 仍画但 click no-op (后续会话可整槽删)。
  static OnShowShortcut s_onShortcut;

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
  static int       s_btnYOffset_phys;   // v0.19.0.24 新增:Y 方向按钮起点(=(kPanelH-kBtnSize)/2,dpr 缩放)
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
  static DWORD     s_showTime;          // v0.19.0.24-fix:Show() 时刻(GetTickCount),auto-hide grace

  // 设计几何常量 (logical pixels)。
  // 注意:**不要**直接用这些 GDI 坐标;用 _phys 等版本(运行时按 dpr 缩放)。
  //
  // v0.19.0.24-fix(issue 1+2):user 反馈 v0.19.0.23 后"按钮偏上 + 间距不够"。
  // 几何重算:
  //   - issue 1 按钮偏上:btn y0 之前用 pad=5,但 (48-35)/2=6.5 取整 6 才让 btn 几何
  //     中心 23.5 接近 panel 中心 24。新增 `kPanelVPadding = (kPanelH - kBtnSize) / 2 = 6`,
  //     跟 `kPanelPadding=5` 区分(X/Y 独立 padding)。
  //   - issue 2 间距+3:user 再要 +3 px。保守路线保留 rightPad=16 不变,扩 panelW:
  //     pad 5 + brand 35 + brandGap 2 + 5*35 + 4*kBtnGap + 16 = 5+35+2+175+4*kBtnGap+16
  //     = 233 + 4*kBtnGap;kBtnGap=14 → 233+56=289
  //   激进备选(§3.2.3):kPanelW=277,rightPad=4,user 觉得 289 太宽时切回
  static constexpr int kPanelPadding = 5;          // X 方向 padding (L93,v0.19.0.24 不变)
  static constexpr int kPanelVPadding = 6;         // v0.19.0.24 新增:Y 方向 padding=(48-35)/2
  static constexpr int kBtnSize      = 35;         // v0.19.0.24 不变
  static constexpr int kBtnGap       = 14;         // v0.19.0.24:11→14 (L93 6→11→14 累计 +8)
  static constexpr int kIcoSize      = 19;         // L93:30→19
  static constexpr int kBtnRadius    = 10;         // L93:14→10
  static constexpr int kBrandSize    = 35;         // L93:56→35
  static constexpr int kPanelRadius  = 20;         // L93:28→20
  static constexpr int kPanelW       = 289;        // v0.19.0.24:277→289 (保守 +12 = 4×3 gap)
  static constexpr int kPanelH       = 48;         // L93:68→48
  // v0.19.0.24 新增:Show() 后 auto-hide grace period(毫秒)。
  // 之前 L89-fix 直接累加 outsideMs,Show 启动时 cursor 在 panel 外 → 1.5s 内 Hide。
  // 改:Show 时记 s_showTime,polling timer 在 (now - s_showTime) < kShowGraceMs 时
  // 不累加 outsideMs。2s = 跟 design-md "show 出来能看一会儿" 期望。
  static constexpr DWORD kShowGraceMs = 2000;

  // L93-fix(图标真视觉居中根本 fix,7 轮 fudge ±1 失败的原因):
  // 所有 DrawIcon* 描线 viewBox:Schema X[5..25] Y[8..22] / Phrase X[5..25] Y[5..25] /
  // Symbols X[3..27] Y[6..24] / Settings X[4..26] Y[4..26] / Account X[5..25] Y[5..28]。
  // 所有 5 个 icon 的 X bbox mid = 15,前 4 个 Y bbox mid = 15,Account Y bbox mid = 16.5
  // (肩弧延到 y+28)。统一用 kIconBboxCyOff=15(Account 1.5 px 视觉差可忽略)。
  // 之前 7 轮 fix 都用 `iconX = x0 + (s_btnSize_phys - s_icoSize_phys) / 2 ± 1`,
  // 假设 icon visual bbox = kIcoSize box(19×19)。错!实际描线 bbox 21..25×15..24
  // 中心在 viewBox (15, 15),远大于 kIcoSize box。这导致:btn 35 时 iconX = x0 + 8,
  // bbox X 落在 [x0+13..x0+33],bbox mid 23 vs btn mid 17.5 → 偏右 5.5 px(等同
  // user 报告"水平未居中,图标居于右下角")。Y 同样:iconY = y0 + 9,bbox Y [y0+17..y0+31],
  // mid y0+24 vs btn mid y0+17.5 → 偏下 6.5 px。
  // 真正修法:iconX = x0 + btn_center − bbox_center × dpr。
  //   btn=35, dpr=1: iconX = x0 + 17.5 − 15 = x0 + 2.5(bbox 真正居中)
  //   旧算法:iconX = x0 + 8(偏右 5.5)。ico bbox X [x0+7.5..x0+27.5] mid x0+17.5(btn mid 一致)。
  static constexpr int kIconBboxCxOff = 15;        // L93 新增:icon visual bbox X 中心(viewBox 局部)
  static constexpr int kIconBboxCyOff = 15;        // L93 新增:icon visual bbox Y 中心(Account 略 16,统一 15)

  // 颜色(0xAABBGGRR)
  // L90-fix: 边框颜色降低深度 — user 反馈"边框颜色深度太深"。
  // v0.19.0.19 边框 kIcoDimC = RGB(50, 50, 60) 几乎纯黑,视觉突兀。
  // 改用 RGB(130, 130, 140) 浅灰,任何背景下都不刺眼,仍能看出 panel 形状。
  static constexpr COLORREF kBgTop    = RGB(200, 225, 250);  // 浅冷蓝顶
  static constexpr COLORREF kBgBot    = RGB(155, 195, 240);  // 略深浅蓝底
  static constexpr COLORREF kIconDim  = RGB(130, 130, 140);  // 浅灰(L90-fix:边框颜色降低深度,任何背景下不刺眼)
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
  // v0.19.0.32-fix (Bug 2): 删除 OnLButtonUp/OnLButtonDown dead-stub 声明。
  // WndProc cpp:562-569 已内联 WM_LBUTTONUP 真 fire 路径,stub 永远不被调用。
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
  // v0.19.0.61 (Phase L 调整 1.1): DrawIconUserDict 已彻底删除 — UserDictionary 模块下线。
  // button 2 hit + paint 都 no-op (case 2 在 paint loop 跳过)。

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
