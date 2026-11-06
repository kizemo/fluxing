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
//   - ApplyAlphaGradient() 改 pBits alpha 通道:圆角内 alpha 恒定(spec 089:
//   梯度已拉平为 255/255),圆角外 0
//   - WS_EX_LAYERED + UpdateLayeredWindow(ULW_ALPHA) 提交到 screen
//
#include "stdafx.h"
#include <functional>
#include "QuickPanelDialog.h"
// spec 084 / ADR 0013 D9: 纯函数的共享单元(header-only,不依赖 windows.h)。
#include <QuickPanelAlpha.h>
// spec 099 P0-6: DPI 缩放的纯逻辑(单一真相源:生产与单测同源)。
#include <FluxingPanelDpi.h>
// spec 099 P0-5: tooltip 文本(单一真相源:名称 + 当前态)。
#include <FluxingPanelTooltip.h>
// v4 需求⑦: 品牌 logo 预合成纯函数(生产与单测同源, R7 单一真相源)。
#include <FluxingPanelLogo.h>

// L81-fix: PNG logo 加载改用 WIC 不用 GDI+ Bitmap(IStream*) (L67-L69 雷区)
// 也不用 LoadImageW(IMAGE_BITMAP) (L81 — IMAGE_BITMAP 不含 PNG,静默失败)。 需要
// <wincodec.h> + windowscodecs.lib (在 xmake.lua 已加)。
#include <wincodec.h>
#include <wrl/client.h>
#include <shlobj.h>
#include <fstream>
#include <string>
// spec 097 诊断日志口径 (UpdateLayeredWindow 失败不再静默吞错)
#include "FluxingDiagLog.h"
#pragma comment(lib, "windowscodecs.lib")

static const wchar_t kWindowClassName[] = L"FluxingQuickPanel_v3";
// spec 099 P0-5: tooltip 是自绘顶层窗(不引 comctl32 —— 不往 vcxproj 里加
// 新依赖,避免构建链变动)。
static const wchar_t kTooltipClassName[] = L"FluxingQuickPanel_Tooltip";
namespace {
// tooltip 视觉常量(logical 像素;按 dpr 缩放)。
constexpr int kTooltipPadPx = 6;          // 文字四周内边距
constexpr int kTooltipGapPx = 4;          // 与面板的垂直间距
constexpr int kTooltipFontHeightPx = 14;  // 字号
}  // namespace

namespace {
// ===== DPI 缩放辅助 =====
// 缺陷: 快捷面板在 150% (DPI=144) 屏上物理只有 245×48,未按缩放放大。
// 根因: WeaselServer 是 PerMonitor aware (manifest True/PM),Windows **不会**
// 替它拉伸窗口 —— 自动 scale 只对 DPI-unaware 进程成立;而 Show() /
// WM_DPICHANGED 把 logical 常量 kPanelW/kPanelH 直接当物理像素用了。
// 参照实现: FluxingPhrasesDialog/PhrasesDialog.cpp:600-621 (dpi + clamp 风格)。
struct PanelDpi {
  UINT x;
  UINT y;
};

// Show() 建窗之前还没有 hwnd,只能走 monitor API。GetDpiForMonitor **不静态
// 链接** —— spec 064 教训 (WeaselUI/WeaselPanel.cpp:16-39 同一手法): 静态导入
// api-ms-win-shcore-scaling-l1-1-1.dll 在该 DLL 缺失的系统上会让进程直接起不
// 来。运行时解析失败就退 GetDpiForSystem() (Win10 1607+;WeaselServer stdafx.h
// 已把 _WIN32_WINNT 定在 0x0A00)。
PanelDpi QueryPanelDpi(POINT anchor) {
  typedef HRESULT(WINAPI * GetDpiForMonitor_t)(HMONITOR, INT, UINT*, UINT*);
  static GetDpiForMonitor_t fn = []() -> GetDpiForMonitor_t {
    HMODULE h = LoadLibraryW(L"shcore.dll");
    if (!h)
      return nullptr;
    return (GetDpiForMonitor_t)GetProcAddress(h, "GetDpiForMonitor");
  }();
  UINT sys = GetDpiForSystem();
  if (sys == 0)
    sys = 96;
  PanelDpi out = {sys, sys};
  HMONITOR mon = MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST);
  if (fn && mon) {
    UINT dx = 0, dy = 0;
    if (SUCCEEDED(fn(mon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy)) && dx != 0 &&
        dy != 0) {
      out.x = dx;
      out.y = dy;
    }
  }
  // clamp 50%..400%,与 PhrasesDialog 的 dpiScale clamp 同口径
  if (out.x < 48)
    out.x = 48;
  if (out.x > 384)
    out.x = 384;
  if (out.y < 48)
    out.y = 48;
  if (out.y > 384)
    out.y = 384;
  return out;
}

// logical → physical。spec 099 P0-6: 缩放口径下沉到 include/FluxingPanelDpi.h,
// 生产代码与单测读**同一份**公式(测试复制公式就是漂移源,constitution R7)。
// 旧实现是 MulDiv(logical, dpi, 96);对正数它等价于 (logical*dpi + 48) / 96
// (就近舍入,.5 向上)—— include/FluxingPanelDpi.h 用纯整数复刻同一口径。
inline int ScaleLogicalByDpi(int logical, UINT dpi) {
  return fluxing::panel_dpi::ScaleLogicalByDpi(logical, (int)dpi);
}

// ===== spec 099 P0-6: 物理布局的推演(单一真相源,constitution R7) =====
// ApplyPhysicalLayout / SlotRect / StatusRect / *ForSize 全部走这一份;
// 单测通过 SlotRectForSize / StatusRectForSize 走同一条路径 ⇒
// 测试**不复制**布局公式(复制即漂移 —— spec 092 记的「假绿」死法)。
struct PanelLayout {
  float dpr_x;
  float dpr_y;
  int padding;
  int brand_size;
  int btn_size;
  int btn_gap;
  int btn_y_offset;
  int brand_gap;  // brand → 第一个槽位的间隙
};

// logical → physical(就近舍入;最小 1 —— 与旧的 scale_x/scale_y 同口径)。
int ScaleRound(int logical, float dpr) {
  const int v = (int)(logical * dpr + 0.5f);
  return v < 1 ? 1 : v;
}

PanelLayout ComputeLayout(int panel_w_phys, int panel_h_phys) {
  PanelLayout L;
  L.dpr_x = (float)panel_w_phys / (float)QuickPanelDialog::kPanelW;
  L.dpr_y = (float)panel_h_phys / (float)QuickPanelDialog::kPanelH;
  L.padding = ScaleRound(QuickPanelDialog::kPanelPadding, L.dpr_x);
  L.brand_size = ScaleRound(QuickPanelDialog::kBrandSize, L.dpr_x);
  L.btn_size = ScaleRound(QuickPanelDialog::kBtnSize, L.dpr_x);
  L.btn_gap = ScaleRound(QuickPanelDialog::kBtnGap, L.dpr_x);
  L.btn_y_offset = ScaleRound(QuickPanelDialog::kPanelVPadding, L.dpr_y);
  // v4: brand→btn gap 取 kBrandGap 单一来源(与绘制面一致, AGENTS.md R7)。
  L.brand_gap = max(QuickPanelDialog::kBrandGap,
                    (int)(QuickPanelDialog::kBrandGap * L.dpr_x + 0.5f));
  return L;
}

// 第 index 个槽位的矩形(物理像素)。
bool SlotRectForLayout(const PanelLayout& L,
                        int index,
                        int* x,
                        int* y,
                        int* w,
                        int* h) {
  if (index < 0 || index >= QuickPanelDialog::kButtonCount)
    return false;
  const int button_start_x = L.padding + L.brand_size + L.brand_gap;
  const int x0 = button_start_x + index * (L.btn_size + L.btn_gap);
  if (x)
    *x = x0;
  if (y)
    *y = L.btn_y_offset;
  if (w)
    *w = L.btn_size;
  if (h)
    *h = L.btn_size;
  return true;
}

// 状态区(中/英 + 全/半角回显)的矩形(物理像素)。
// 关键约束:**必须与全部槽位矩形不相交** —— 否则状态像素会压到槽位上,
// 槽位命中区就不可能保持不变(spec 099 P0-4「命中区不变」)。
// 算法:左界 = 最后一个槽位的右边界 + 间隙;右界 = 内容右界(面板宽 - padding)。
// 这样在任意 DPI 下都不会压到槽位(把 x 写死成 225 在 150% 会压到槽位 3 上)。
// Fluxing QuickPanel v4 需求⑥: 竖排状态回显区已删除, 状态改由槽 4/5 键帽承担。
// v4 几何下槽位行铺满整宽 ⇒ right <= left 恒成立 ⇒ 本函数恒返回 false。
bool StatusRectForLayout(const PanelLayout& L,
                          int panel_w_phys,
                          int* x,
                          int* y,
                          int* w,
                          int* h) {
  int lx = 0, ly = 0, lw = 0, lh = 0;
  if (!SlotRectForLayout(L, QuickPanelDialog::kButtonCount - 1, &lx, &ly, &lw,
                         &lh))
    return false;
  const int gap = ScaleRound(fluxing::panel_status::kStatusGap, L.dpr_x);
  const int left = lx + lw + gap;
  const int right = panel_w_phys - L.padding;
  if (right <= left)
    return false;  // 极端窄面板 ⇒ 不画状态区(而不是画到槽位上)
  if (x)
    *x = left;
  if (y)
    *y = L.btn_y_offset;
  if (w)
    *w = right - left;
  if (h)
    *h = L.btn_size;
  return true;
}

// 面板贴 workArea 右下角;尺寸与 margin 一起缩放。clamp 保证低分辨率/超大
// 缩放下不出屏 (L92-fix)。所有入参一律物理像素。
void PlacePanelAtBottomRight(const RECT& workArea,
                             int panelW,
                             int panelH,
                             int margin,
                             int* x,
                             int* y) {
  int px = workArea.right - panelW - margin;
  int py = workArea.bottom - panelH - margin;
  if (px < workArea.left)
    px = workArea.left;
  if (py < workArea.top)
    py = workArea.top;
  if (px + panelW > workArea.right)
    px = workArea.right - panelW;
  if (py + panelH > workArea.bottom)
    py = workArea.bottom - panelH;
  *x = px;
  *y = py;
}
}  // namespace

namespace {
// 圆角掩码测试:4 corner cells 用 circle equation,其余 (W-r)..(H-r) 范围内的
// inside=true。 per-pixel 内调用 ~54K 次 (360x68,1x DPI),避免
// CreateRoundRectRgn / PtInRegion 开销。
inline bool IsInsideRoundedRect(int x, int y, int W, int H, int r) {
  if (x < 0 || x >= W || y < 0 || y >= H)
    return false;
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
// 浅玻璃色 仍保留 macOS Liquid Glass 视觉感,但**任何背景下**都能看出 panel
// 形状。 L91-fix: kIcoDimC RGB(50,50,60) → RGB(130,130,140) 浅灰 — user
// 反馈"边框颜色深度太深" 任何背景下不刺眼,仍能看出 panel 形状。DrawIcon* 全部用
// kIcoDimC 创建 pen, 必须修这里(不在 header 那个 alias)。
constexpr COLORREF kIcoDimC = RGB(130, 130, 140);  // L91-fix: 边框颜色降低深度
constexpr COLORREF kAccentC = RGB(255, 95, 49);    // 品牌橙
constexpr COLORREF kAccent2C = RGB(155, 81, 224);  // 品牌紫
constexpr COLORREF kWhiteC = RGB(255, 255, 255);   // 白色(active icon)

void QuickPanelDialog::DrawIconSchema(HDC hdc,
                                      int x,
                                      int y,
                                      COLORREF penColor) {
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

void QuickPanelDialog::DrawIconPhrase(HDC hdc,
                                      int x,
                                      int y,
                                      COLORREF penColor) {
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

void QuickPanelDialog::DrawIconSymbols(HDC hdc,
                                       int x,
                                       int y,
                                       COLORREF penColor) {
  // 键盘(viewBox 24x24)
  // L86-fix: pen color 从 caller 传入
  HPEN pen = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 2, penColor));
  // 外壳
  RoundRect(hdc, x + 3, y + 6, x + 27, y + 24, 2, 2);
  // 顶行按键分隔(4个)
  MoveToEx(hdc, x + 8, y + 6, nullptr);
  LineTo(hdc, x + 8, y + 12);
  MoveToEx(hdc, x + 13, y + 6, nullptr);
  LineTo(hdc, x + 13, y + 12);
  MoveToEx(hdc, x + 18, y + 6, nullptr);
  LineTo(hdc, x + 18, y + 12);
  MoveToEx(hdc, x + 22, y + 6, nullptr);
  LineTo(hdc, x + 22, y + 12);
  // 中行分隔
  MoveToEx(hdc, x + 3, y + 12, nullptr);
  LineTo(hdc, x + 27, y + 12);
  MoveToEx(hdc, x + 3, y + 16, nullptr);
  LineTo(hdc, x + 27, y + 16);
  // 中行内部
  MoveToEx(hdc, x + 11, y + 16, nullptr);
  LineTo(hdc, x + 11, y + 20);
  MoveToEx(hdc, x + 18, y + 16, nullptr);
  LineTo(hdc, x + 18, y + 20);
  // 空格(底部加粗)
  HPEN penBold = (HPEN)SelectObject(hdc, CreatePen(PS_SOLID, 3, kIcoDimC));
  MoveToEx(hdc, x + 8, y + 22, nullptr);
  LineTo(hdc, x + 22, y + 22);
  DeleteObject(SelectObject(hdc, penBold));
  DeleteObject(SelectObject(hdc, pen));
}

void QuickPanelDialog::DrawIconSettings(HDC hdc,
                                        int x,
                                        int y,
                                        COLORREF penColor) {
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

void QuickPanelDialog::DrawIconAccount(HDC hdc,
                                       int x,
                                       int y,
                                       COLORREF penColor) {
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
HWND QuickPanelDialog::s_hwnd = NULL;
bool QuickPanelDialog::s_fullwidth = false;
bool QuickPanelDialog::s_mouseTracked = false;
bool QuickPanelDialog::s_dragging = false;  // L87-fix 手动 drag 状态
// L178-fix: 按下时是否真的拿到了有效光标位置(GetCursorPos 可能失败)。
bool QuickPanelDialog::s_dragCursorValid = false;
POINT QuickPanelDialog::s_dragStartCursor = {0,
                                             0};  // L87-fix drag 开始时 cursor
RECT QuickPanelDialog::s_dragStartWindow = {
    0, 0, 0, 0};                        // L87-fix drag 开始时 window pos
int QuickPanelDialog::s_outsideMs = 0;  // L89-fix: 鼠标在 panel 外累计 ms
DWORD QuickPanelDialog::s_showTime = 0;  // v0.19.0.24:Show() 时刻 grace 计时
// spec 084 / ADR 0013: 整体不透明度系数,不是 alpha 通道值本身。
// 255 = 满值(逐像素与 spec 084 之前一致),217 = 85%(长显常态, spec 089 / ADR
// 0022)。
int QuickPanelDialog::s_alphaFactor = fluxing::kAlphaFactorFull;
int QuickPanelDialog::s_targetAlphaFactor = fluxing::kAlphaFactorFull;
QuickPanelDialog::OnClick QuickPanelDialog::s_onSchema;
QuickPanelDialog::OnClick QuickPanelDialog::s_onUserFolder;
QuickPanelDialog::OnClick QuickPanelDialog::s_onPhrases;
QuickPanelDialog::OnToggle QuickPanelDialog::s_onFullwidth;
// Fluxing QuickPanel v4 需求④: 中英文切换键的回调(Setter 注入)。
QuickPanelDialog::OnToggle QuickPanelDialog::s_onAscii;
QuickPanelDialog::OnClick QuickPanelDialog::s_onSymbols;
QuickPanelDialog::OnClick QuickPanelDialog::s_onLogin;
// v0.19.0.29:hit==3 走 SetOn* setter 注入 callback。
// v0.19.0.60 (Phase L 调整 1): s_onUserDict / SetOnUserDict 整体删除 —
// UserDictionary 模块下线。
QuickPanelDialog::OnShowShortcut QuickPanelDialog::s_onShortcut;

// spec 100 / ADR 0038: 品牌图标右键入口。
QuickPanelDialog::OnBrandRightClick QuickPanelDialog::s_onBrandRightClick;
bool QuickPanelDialog::s_menuOpen = false;

void QuickPanelDialog::SetOnBrandRightClick(OnBrandRightClick fn) {
  s_onBrandRightClick = fn;
}

// Fluxing QuickPanel v4 需求④: 中英文切换键的回调注入(Setter 路径)。
// 与全半角键(Show() 第 5 参)对称; 语义 = 传**目标值**(true = 英文态)。
void QuickPanelDialog::SetOnAscii(OnToggle fn) {
  s_onAscii = fn;
}

// spec 100 / ADR 0038: 品牌图标的物理矩形 —— **单一事实源**。
// 与 HitTest 的 -2(brand)判定同源(同一份 ComputeLayout)⇒ 命中区与右键区
// 永远重合;否则用户右键品牌图标却弹不出菜单(这种 bug 极难靠肉眼发现)。
bool QuickPanelDialog::BrandRectForSize(int panel_w_phys,
                                        int panel_h_phys,
                                        int* x,
                                        int* y,
                                        int* w,
                                        int* h) {
  if (panel_w_phys <= 0 || panel_h_phys <= 0)
    return false;
  const PanelLayout L = ComputeLayout(panel_w_phys, panel_h_phys);
  if (x)
    *x = L.padding;
  if (y)
    *y = L.padding;
  if (w)
    *w = L.brand_size;
  if (h)
    *h = L.brand_size;
  return true;
}

bool QuickPanelDialog::BrandRect(int* x, int* y, int* w, int* h) {
  return BrandRectForSize(s_panelW_phys, s_panelH_phys, x, y, w, h);
}

// v0.19.0.29 setter 实现。SetOn* 在 WeaselServerApp::Run() 末尾(after m_server
// Start + RegisterPhrasesHotkey,before m_server.Run 进入消息循环)调一次,
// 跟 m_server 同 lifecycle。
void QuickPanelDialog::SetOnShortcut(OnShowShortcut fn) {
  s_onShortcut = fn;
}

int QuickPanelDialog::s_hoveredIdx = -1;
int QuickPanelDialog::s_activeIdx = -1;

HBITMAP QuickPanelDialog::s_hBmpLogo = NULL;
HBRUSH QuickPanelDialog::s_hBrushPanelBg = NULL;
HBRUSH QuickPanelDialog::s_hBrushIconDim = NULL;
HBRUSH QuickPanelDialog::s_hBrushIconAccent = NULL;
HBRUSH QuickPanelDialog::s_hBrushActive = NULL;
HBRUSH QuickPanelDialog::s_hBrushHighlight = NULL;
HBRUSH QuickPanelDialog::s_hBrushShadow = NULL;
HPEN QuickPanelDialog::s_hPenIconDim = NULL;
HPEN QuickPanelDialog::s_hPenIconAccent = NULL;
HPEN QuickPanelDialog::s_hPenHighlight = NULL;
HDC QuickPanelDialog::s_hdcMem = NULL;
HBITMAP QuickPanelDialog::s_hBmpMem = NULL;
int QuickPanelDialog::s_panelW_phys = 0;
int QuickPanelDialog::s_panelH_phys = 0;
float QuickPanelDialog::s_dpr_x = 1.0f;
float QuickPanelDialog::s_dpr_y = 1.0f;
int QuickPanelDialog::s_panelPadding_phys = 0;
int QuickPanelDialog::s_btnYOffset_phys = 0;  // v0.19.0.24 新增
int QuickPanelDialog::s_btnSize_phys = 0;
int QuickPanelDialog::s_icoSize_phys = 0;
int QuickPanelDialog::s_btnRadius_phys = 0;
int QuickPanelDialog::s_brandSize_phys = 0;
int QuickPanelDialog::s_panelRadius_phys = 0;
int QuickPanelDialog::s_btnGap_phys = 0;
// spec 099 P0-4: 当前输入状态的内存快照 + 状态提供者(零 I/O,P2)。
fluxing::panel_status::PanelState QuickPanelDialog::s_inputState;
QuickPanelDialog::StateProvider QuickPanelDialog::s_stateProvider;
// spec 099 P0-4: 状态文字字体(按 dpr 重建;OnDestroy 释放)。
HFONT QuickPanelDialog::s_hFontStatus = NULL;
// spec 099 P0-5: 键盘交互状态(焦点槽位 / 是否已进入键盘交互态)。
fluxing::panel_keyboard::State QuickPanelDialog::s_keyboard;
// spec 099 P0-5: tooltip 顶层窗 + 字体 + 接管键盘焦点前的原焦点窗口。
HWND QuickPanelDialog::s_hwndTooltip = NULL;
HFONT QuickPanelDialog::s_hFontTooltip = NULL;
HWND QuickPanelDialog::s_prevFocusHwnd = NULL;

// ===== 内部 =====
// L81 + L82: LoadLogoWIC 用 WIC 解码 PNG(不用
// LoadImageW/IMAGE_BITMAP,后者不支持 PNG)。
// 流程:CoCreateInstance(IWICImagingFactory) → CreateDecoderFromFilename →
// GetFrame(0) → FormatConverter(32bppBGRA) → CopyPixels 到 32-bit DIBSection。
//
// L82: 文件名改 `fluxing-logo_small.png` (用户实际 logo,20x20 PNG icon)。
// 之前 v0.19.0.10/0.11 用 `fluxing-logo.png` (700x700 大 logo,中心裁切后
// 渲染出来是橘红色块,不是 design 上的小 icon)。
//
// L83-fix: 加 IWICBitmapScaler 在 WIC 阶段把 PNG 预缩放到 s_brandSize_phys
// (DPI-aware 物理像素)。原因:GDI 的 AlphaBlend / BitBlt **都不支持拉伸**
// (MS docs 对 AlphaBlend 显式说 "AlphaBlend does not support stretching or
// compressing"),L82 误以为会拉伸。预缩放到 dest 大小后 1:1 贴图 OK。
HRESULT QuickPanelDialog::LoadLogoWIC(HWND /*hwnd*/, HBITMAP& hBmpOut) {
  hBmpOut = NULL;
  wchar_t exeDir[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, exeDir, MAX_PATH);
  wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
  if (lastSlash)
    *lastSlash = L'\0';
  wchar_t logoPath[MAX_PATH];
  // L84-fix: try fluxing-logo_small.png first (20x20 PNG icon, actual user
  // logo), fall back to fluxing-logo.png (700x700 big logo) if small not found.
  // Some user installs may be missing the small file (older v0.19.0.x installs
  // didn't have it), and the small was added in v0.19.0.13 — fallback ensures
  // logo is always shown after upgrade.
  _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo_small.png", exeDir);
  if (GetFileAttributesW(logoPath) == INVALID_FILE_ATTRIBUTES) {
    _snwprintf_s(logoPath, _TRUNCATE, L"%s\\fluxing-logo.png", exeDir);
  }

  // 确保 thread 上 COM 已初始化(WIC STA)。失败也不致命 → 仅显示空 logo
  HRESULT hrInit = EnsureComInit();
  if (FAILED(hrInit))
    return hrInit;

  Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&wic));
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
  hr = wic->CreateDecoderFromFilename(logoPath, nullptr, GENERIC_READ,
                                      WICDecodeMetadataCacheOnLoad, &dec);
  if (FAILED(hr))
    return hr;  // 文件可能不存在 — 用户没装 logo,或路径错

  Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
  hr = dec->GetFrame(0, &frame);
  if (FAILED(hr))
    return hr;

  // L83: IWICBitmapScaler 预缩放到 brand area 物理大小。保证 GDI 1:1 贴图
  // (BitBlt/AlphaBlend 都不支持拉伸)。 用 s_brandSize_phys(DPI
  // 缩放后的物理像素),不是 logical kBrandSize。
  Microsoft::WRL::ComPtr<IWICBitmapScaler> scaler;
  hr = wic->CreateBitmapScaler(&scaler);
  if (FAILED(hr))
    return hr;
  UINT targetSize = (UINT)s_brandSize_phys;
  if (targetSize < 1)
    targetSize = 1;  // 保护 0
  hr = scaler->Initialize(frame.Get(), targetSize, targetSize,
                          WICBitmapInterpolationModeFant);  // v4: 缩到 30px 避免黑框发暗
  if (FAILED(hr))
    return hr;

  Microsoft::WRL::ComPtr<IWICFormatConverter> fmtConv;
  hr = wic->CreateFormatConverter(&fmtConv);
  if (FAILED(hr))
    return hr;

  hr = fmtConv->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA,
                           WICBitmapDitherTypeNone, nullptr, 0.0,
                           WICBitmapPaletteTypeCustom);
  if (FAILED(hr))
    return hr;

  UINT w = 0, h = 0;
  hr = fmtConv->GetSize(&w, &h);
  if (FAILED(hr) || w == 0 || h == 0)
    return hr;

  // 创建 32-bit BGRA DIB Section (top-down,负 height) 接收 CopyPixels 输出。
  // 与 CreateOffscreenDC 的 s_hBmpMem 用同样的 masks,保证 BitBlt/AlphaBlend 时
  // alpha 通道结构一致。
  BITMAPV5HEADER bi = {};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = w;
  bi.bV5Height = -(LONG)h;  // top-down
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;

  void* bits = nullptr;
  HBITMAP hbmp = CreateDIBSection(nullptr, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                                  &bits, nullptr, 0);
  if (!hbmp || !bits) {
    if (hbmp)
      DeleteObject(hbmp);
    return E_OUTOFMEMORY;
  }

  UINT stride = w * 4;
  UINT total = stride * h;
  hr = fmtConv->CopyPixels(nullptr, stride, total, (BYTE*)bits);
  if (FAILED(hr)) {
    DeleteObject(hbmp);
    return hr;
  }
  // ===== v4 需求⑦ (品牌标志上的黑色框) —— 二次根因与修复 =====
  //
  // 【先前的结论已被运行时证据推翻】旧注释断言「素材自带 36px 近黑 keyline
  // 描边 ⇒ 黑框根因」, 并据此只做了素材层改色(a04cc41)。2026-11-07 的真机
  // 渲染落盘取证(服务端把 s_hBmpMem 全图与 s_hBmpLogo 原图各存一份 BMP)证明:
  //   - s_hBmpLogo 位图本身 **不含任何不透明近黑像素**(opaque near-black == 0,
  //     边缘列是 (254,254,254, α≈242~251) 的半透明白);
  //   - 但渲染后的面板品牌区出现 **203 个近黑像素**, 位置恰好等于 logo 的
  //     **半透明边缘列**, 像素值 ≈ 255 − 源 alpha;
  //   ⇒ 病灶是 **GDI 的 alpha 合成路径**(AlphaBlend 把半透明源像素画成了
  //     近黑), 与素材无关。素材改色保留(去掉了一个真实暗像素来源), 但它不是修复。
  //
  // 【修复】两步, 彻底绕开 GDI alpha:
  //   1) 在这里(WIC 解码后、交给绘制层之前)把 logo **预合成到面板底色
  //      kBgTop** 上, 得到 alpha 恒 255 的不透明位图;
  //      纯函数在 include/FluxingPanelLogo.h(生产与单测同源, R7)。
  //   2) 绘制阶段改用 BitBlt(SRCCOPY) 1:1 贴图, **不再调用 AlphaBlend**
  //      (见 PaintOpaqueContent)。
  //
  // 【为什么预合成是必要的】GDI 的 SRCCOPY 不看 alpha 通道, 源里 alpha=0 的
  // 像素会按其 RGB 原样贴上去 —— 这正是历史上「黑底 + 红 logo」的成因。
  // 预合成之后每个像素的 RGB 已是最终融合色、alpha 恒 255, SRCCOPY 行为确定。
  fluxing::panel_logo::CompositeOpaqueOverBg(
      static_cast<uint8_t*>(bits), static_cast<int>(w), static_cast<int>(h),
      static_cast<uint8_t>(GetRValue(kBgTop)),
      static_cast<uint8_t>(GetGValue(kBgTop)),
      static_cast<uint8_t>(GetBValue(kBgTop)));

  hBmpOut = hbmp;
  return S_OK;
}

// L81:COM 一次性初始化。CoInitializeEx 返回 S_OK 表示初始化成功,
// S_FALSE 表示**已**初始化过(无需重复)。我们 idempotent 调用一次。
HRESULT QuickPanelDialog::EnsureComInit() {
  static bool s_initialized = false;
  if (s_initialized)
    return S_OK;
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
  if (!s_hdcMem) {
    ReleaseDC(NULL, hdcScreen);
    return E_FAIL;
  }

  // L78-fix: 改用 32-bit DIB section(带 alpha channel)
  // 之前 CreateCompatibleBitmap 是 24-bit DDB,GradientFill 的 alpha 被丢掉 →
  // 全黑 L81-fix: 用 top-down DIB (负 height) 让 y=0 ↔ top, y=H-1 ↔ bottom
  // 直观一致, 否则 ApplyAlphaGradient 的 gradient 计算和实际像素位置是反的。
  BITMAPV5HEADER bi = {};
  bi.bV5Size = sizeof(bi);
  bi.bV5Width = w;
  bi.bV5Height = -h;  // top-down (负值)
  bi.bV5Planes = 1;
  bi.bV5BitCount = 32;
  bi.bV5Compression = BI_BITFIELDS;
  bi.bV5RedMask = 0x00FF0000;
  bi.bV5GreenMask = 0x0000FF00;
  bi.bV5BlueMask = 0x000000FF;
  bi.bV5AlphaMask = 0xFF000000;
  void* pBits = nullptr;
  s_hBmpMem = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS,
                               &pBits, NULL, 0);
  if (!s_hBmpMem) {
    DeleteDC(s_hdcMem);
    s_hdcMem = NULL;
    ReleaseDC(NULL, hdcScreen);
    return E_FAIL;
  }
  SelectObject(s_hdcMem, s_hBmpMem);
  ReleaseDC(NULL, hdcScreen);
  return S_OK;
}

void QuickPanelDialog::DestroyOffscreenDC() {
  if (s_hdcMem) {
    DeleteDC(s_hdcMem);
    s_hdcMem = NULL;
  }
  if (s_hBmpMem) {
    DeleteObject(s_hBmpMem);
    s_hBmpMem = NULL;
  }
}

// spec 099 P0-3: 槽位矩形(物理像素)—— 布局的**单一事实源**。
// HitTest 与 test/TestQuickPanelDialog 都从这里取值,避免测试复制一份
// 布局公式(复制即漂移)。
bool QuickPanelDialog::SlotRect(int index, int* x, int* y, int* w, int* h) {
  if (s_panelW_phys == 0)
    return false;
  return SlotRectForSize(s_panelW_phys, s_panelH_phys, index, x, y, w, h);
}

// spec 099 P0-6: 给定物理面板尺寸推演槽位矩形 —— 单测用它断言
// 100% / 150% / 200% 三档 DPI 下的几何,而不必真的切系统 DPI。
// 与运行期 SlotRect 走**同一条**公式(单一真相源,constitution R7)。
bool QuickPanelDialog::SlotRectForSize(int panel_w_phys,
                                        int panel_h_phys,
                                        int index,
                                        int* x,
                                        int* y,
                                        int* w,
                                        int* h) {
  if (panel_w_phys <= 0 || panel_h_phys <= 0)
    return false;
  const PanelLayout L = ComputeLayout(panel_w_phys, panel_h_phys);
  return SlotRectForLayout(L, index, x, y, w, h);
}

// spec 099 P0-4: 状态区(中/英 + 全/半角回显)矩形 —— 运行期。
bool QuickPanelDialog::StatusRect(int* x, int* y, int* w, int* h) {
  if (s_panelW_phys == 0)
    return false;
  return StatusRectForSize(s_panelW_phys, s_panelH_phys, x, y, w, h);
}

// spec 099 P0-4: 给定物理面板尺寸推演状态区矩形。
bool QuickPanelDialog::StatusRectForSize(int panel_w_phys,
                                          int panel_h_phys,
                                          int* x,
                                          int* y,
                                          int* w,
                                          int* h) {
  if (panel_w_phys <= 0 || panel_h_phys <= 0)
    return false;
  const PanelLayout L = ComputeLayout(panel_w_phys, panel_h_phys);
  return StatusRectForLayout(L, panel_w_phys, x, y, w, h);
}

// ===== spec 099 P0-4: 当前输入状态回显(状态区「中/英」+「全/半」) =====
// 状态的**语义**在 include/FluxingPanelStatus.h(单一真相源,constitution R7);
// 这里只负责「从哪里拿」与「什么时候重绘」。
//  - 来源:服务端注入的 StateProvider(读 handler 的缓存字段,纯内存);
//  - 时机:面板**已有的** 100ms 轮询 timer ⇒ 显示路径零 I/O(守 P2)。
void QuickPanelDialog::SetStateProvider(StateProvider fn) {
  s_stateProvider = fn;
  RefreshInputState();
}

void QuickPanelDialog::SetInputState(bool ascii_mode, bool fullwidth) {
  s_inputState.ascii_mode = ascii_mode;
  s_inputState.fullwidth = fullwidth;
  s_inputState.always_show = AlwaysShowMode();
  if (s_hwnd && IsWindowVisible(s_hwnd))
    RepaintLayered(s_hwnd);
}

fluxing::panel_status::PanelState QuickPanelDialog::CurrentInputState() {
  return s_inputState;
}

// 拉一次快照并与当前快照比对:变了才重绘(避免每 100ms 无谓重画)。
void QuickPanelDialog::RefreshInputState() {
  if (!s_stateProvider)
    return;
  fluxing::panel_status::PanelState next = s_stateProvider();
  // 「常驻显示」开关的真相源是 include/FluxingAlwaysShow.h(运行期内存),
  // 不从 provider 拿(否则就是第二个真相源,违 R7)。
  next.always_show = AlwaysShowMode();
  const fluxing::panel_status::PanelState& cur = s_inputState;
  const bool changed = (next.ascii_mode != cur.ascii_mode) ||
                       (next.fullwidth != cur.fullwidth) ||
                       (next.always_show != cur.always_show) ||
                       (next.schema_name != cur.schema_name) ||
                       (next.version != cur.version);
  if (!changed)
    return;
  s_inputState = next;
  if (s_hwnd && IsWindowVisible(s_hwnd))
    RepaintLayered(s_hwnd);
}

// ===== spec 099 P0-5: 键盘可达性 =====
// 键位语义在 include/FluxingPanelKeyboard.h(单一真相源);这里只负责把裁决
// 结果落到真实窗口行为上。
void QuickPanelDialog::ResetKeyboardState() {
  s_keyboard.focused = -1;
  s_keyboard.slot_count = kButtonCount;
  s_keyboard.interactive = false;
}

// 接管键盘焦点。
// 背景:面板窗口带 WS_EX_NOACTIVATE + ShowWindow(SW_SHOWNOACTIVATE)
// ⇒ 面板出现时**不抢**用户编辑框焦点(否则用户没法继续打字 —— L89-fix
// 记的「无法输入中文」事故)。代价是面板收不到按键。
// 折中:用户**主动点击**面板时才接管焦点;收起时归还。
// 真机行为(跨线程 SetFocus / AttachThreadInput)只能在装机环境确认 ——
// 见 ADR 0036「未验证(真机)」。
void QuickPanelDialog::CaptureKeyboardFocus(HWND hwnd) {
  if (!hwnd || !IsWindow(hwnd))
    return;
  HWND fg = GetForegroundWindow();
  s_prevFocusHwnd = NULL;
  if (fg) {
    const DWORD fgThread = GetWindowThreadProcessId(fg, nullptr);
    const DWORD curThread = GetCurrentThreadId();
    if (fgThread && fgThread != curThread) {
      // 跨线程:临时把两个线程的输入队列接起来,才能读到/归还别的线程的焦点。
      AttachThreadInput(curThread, fgThread, TRUE);
      s_prevFocusHwnd = GetFocus();
      AttachThreadInput(curThread, fgThread, FALSE);
    } else {
      s_prevFocusHwnd = GetFocus();
    }
  }
  SetFocus(hwnd);
  s_keyboard.slot_count = kButtonCount;
  s_keyboard.interactive = true;
}

// 归还焦点。只在「焦点此刻仍在面板上」时归还 —— 用户若已经点了别的窗口,
// 绝不能把它抢回来。
void QuickPanelDialog::RestoreKeyboardFocus() {
  HWND target = s_prevFocusHwnd;
  s_prevFocusHwnd = NULL;
  if (!target || !IsWindow(target))
    return;
  if (s_hwnd && GetFocus() != s_hwnd)
    return;
  const DWORD targetThread = GetWindowThreadProcessId(target, nullptr);
  const DWORD curThread = GetCurrentThreadId();
  if (targetThread && targetThread != curThread) {
    AttachThreadInput(curThread, targetThread, TRUE);
    SetFocus(target);
    AttachThreadInput(curThread, targetThread, FALSE);
  } else {
    SetFocus(target);
  }
}

// ===== spec 099 P0-5: 每槽 tooltip(自绘顶层窗) =====
std::wstring QuickPanelDialog::s_tooltipText;
int QuickPanelDialog::s_tooltipSlot = -1;

void QuickPanelDialog::HideTooltip() {
  if (s_hwndTooltip && IsWindow(s_hwndTooltip))
    ShowWindow(s_hwndTooltip, SW_HIDE);
  s_tooltipText.clear();
  s_tooltipSlot = -1;
}

void QuickPanelDialog::ShowTooltip(int slot) {
  if (!s_hwnd || !IsWindow(s_hwnd) || slot < 0 || slot >= kButtonCount) {
    HideTooltip();
    return;
  }
  const std::wstring text = TooltipText(slot);
  if (text.empty()) {
    HideTooltip();
    return;
  }
  if (!s_hwndTooltip || !IsWindow(s_hwndTooltip)) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &QuickPanelDialog::TooltipWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = kTooltipClassName;
    RegisterClassExW(&wc);
    s_hwndTooltip = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        kTooltipClassName, L"", WS_POPUP, 0, 0, 10, 10, NULL, NULL,
        GetModuleHandle(NULL), NULL);
    if (!s_hwndTooltip)
      return;
  }
  if (!s_hFontTooltip) {
    const int fh = ScaleRound(kTooltipFontHeightPx, s_dpr_y);
    s_hFontTooltip = CreateFontW(-fh, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
  }
  s_tooltipText = text;
  s_tooltipSlot = slot;

  // 量文字尺寸 ⇒ 决定 tooltip 窗口大小。
  int text_w = 0;
  int text_h = 0;
  HDC hdc = GetDC(s_hwndTooltip);
  if (hdc) {
    HGDIOBJ old = SelectObject(hdc, s_hFontTooltip);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()),
                          &sz);
    SelectObject(hdc, old);
    ReleaseDC(s_hwndTooltip, hdc);
    text_w = sz.cx;
    text_h = sz.cy;
  }
  const int pad = ScaleRound(kTooltipPadPx, s_dpr_x);
  const int gap = ScaleRound(kTooltipGapPx, s_dpr_y);
  const int w = text_w + pad * 2;
  const int h = text_h + pad * 2;

  // 位置:面板**上方**、水平对准槽位中心;钳制在面板所在显示器的工作区内。
  int px = 0;
  int py = 0;
  int sx = 0, sy = 0, sw = 0, sh = 0;
  RECT wr = {};
  GetWindowRect(s_hwnd, &wr);
  if (SlotRect(slot, &sx, &sy, &sw, &sh)) {
    px = wr.left + sx + sw / 2 - w / 2;
    py = wr.top + sy - h - gap;
  } else {
    px = wr.left;
    py = wr.top - h - gap;
  }
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  HMONITOR mon = MonitorFromWindow(s_hwnd, MONITOR_DEFAULTTONEAREST);
  if (GetMonitorInfoW(mon, &mi)) {
    if (px < mi.rcWork.left)
      px = mi.rcWork.left;
    if (px + w > mi.rcWork.right)
      px = mi.rcWork.right - w;
    if (py < mi.rcWork.top)
      py = mi.rcWork.top;
    if (py + h > mi.rcWork.bottom)
      py = mi.rcWork.bottom - h;
  }
  SetWindowPos(s_hwndTooltip, HWND_TOPMOST, px, py, w, h,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  InvalidateRect(s_hwndTooltip, NULL, FALSE);
  UpdateWindow(s_hwndTooltip);
}

void QuickPanelDialog::PaintTooltip(HDC hdc) {
  if (!s_hwndTooltip)
    return;
  RECT rc = {};
  GetClientRect(s_hwndTooltip, &rc);
  HBRUSH bg = CreateSolidBrush(RGB(38, 40, 48));
  if (bg) {
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);
  }
  HPEN border = CreatePen(PS_SOLID, 1, RGB(92, 96, 112));
  if (border) {
    HGDIOBJ oldPen = SelectObject(hdc, border);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(border);
  }
  if (!s_hFontTooltip || s_tooltipText.empty())
    return;
  HGDIOBJ oldFont = SelectObject(hdc, s_hFontTooltip);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(238, 240, 246));
  RECT tr = rc;
  DrawTextW(hdc, s_tooltipText.c_str(), -1, &tr,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  SelectObject(hdc, oldFont);
}

LRESULT CALLBACK QuickPanelDialog::TooltipWndProc(HWND hwnd,
                                                  UINT msg,
                                                  WPARAM w,
                                                  LPARAM l) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      if (hdc)
        PaintTooltip(hdc);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    // 鼠标穿透:tooltip 不抢点击,也不打断面板的 hover。
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, w, l);
}

std::wstring QuickPanelDialog::TooltipText(int slot) {
  return fluxing::panel_tooltip::BuildTooltip(slot, s_inputState);
}

// spec 099 P0-3: 槽位点击路由 —— 每个可见槽位点击都必须产生确定的
// 用户可见效果(spec 099 §7 P0-3 验收判据)。
//   0 Schema  → 切换到下一个输入方案(此前是死槽:注释写着 no-op)
//   1 Phrase  → 短语库
//   2 Settings→ 快捷键设置(走 SetOnShortcut 注入)
//   3 Account → 关于/账号说明页(此前是死槽)
//   4 中英文切换(Fluxing v4 需求④) / 5 全半角切换(Fluxing v4 需求③)
// 返回 true 表示该槽位确实触发了回调。
bool QuickPanelDialog::DispatchSlotClick(int index) {
  switch (index) {
    case 0:
      if (s_onSchema) {
        s_onSchema();
        return true;
      }
      return false;
    case 1:
      if (s_onPhrases) {
        s_onPhrases();
        return true;
      }
      return false;
    case 2:
      if (s_onShortcut) {
        s_onShortcut();
        return true;
      }
      return false;
    case 3:
      if (s_onLogin) {
        s_onLogin();
        return true;
      }
      return false;
    case 4: {
      // Fluxing v4 需求④: 中英文切换键。点击瞬间先刷一次快照, 避免落在
      // 100ms 轮询窗口内时按**陈旧**状态算目标值(那会让按键「点了没反应」)。
      // 快照来自 provider(只读内存, 显示路径零 I/O, constitution P2)。
      RefreshInputState();
      if (!s_onAscii)
        return false;
      s_onAscii(!s_inputState.ascii_mode);
      return true;
    }
    case 5: {
      // Fluxing v4 需求③: 全半角切换键。复用 Show() 注入的既有 s_onFullwidth
      // (不新增回调 ⇒ Show() 签名与所有调用点不变)。
      RefreshInputState();
      if (!s_onFullwidth)
        return false;
      const bool target = !s_inputState.fullwidth;
      // 只更新公开访问器的镜像; **不**乐观改 s_inputState —— 轮询器会拿
      // provider 的真值比对后重绘(乐观改会让「无变化 ⇒ 不重绘」吃掉刷新)。
      s_fullwidth = target;
      s_onFullwidth(target);
      return true;
    }
    default:
      return false;
  }
}

int QuickPanelDialog::HitTest(int x, int y) {
  if (s_panelW_phys == 0)
    return -1;
  if (x < 0 || y < 0 || x >= s_panelW_phys || y >= s_panelH_phys)
    return -1;
  // L83-fix: WM_MOUSEMOVE lParam 在 PerMonitor DPI Aware 进程下是 **physical
  // pixels** (Windows 自动缩放从 logical → physical 投递)。HitTest 必须用
  // physical 像素常量 (s_*_phys 按 dpr 缩放后的)。之前用 logical kPanelPadding
  // 等,在 sub-100% DPI 显示器 上 mouse 物理 222 → logical 148 → 按 360x68
  // layout 算 hit = btn2,但 user 实际在物理 btn1 (x=148/240=61%
  // 横向位置)。Hover 完全错位。改用 physical 常量后,physical 222 → btn1 (btn1
  // 在 physical x=83..120)。
  // spec 100 / ADR 0038: brand 命中区改由 BrandRect() 给出(单一事实源) ——
  // 右键弹菜单的命中区必须与这里**逐像素**一致,否则用户右键图标却没反应。
  int brandX = 0, brandY = 0, brandW = 0, brandH = 0;
  if (BrandRect(&brandX, &brandY, &brandW, &brandH) && x >= brandX &&
      x < brandX + brandW && y >= brandY && y < brandY + brandH)
    return -2;  // brand
  // spec 099 P0-3: 槽位矩形改由 SlotRect() 统一给出(布局单一事实源)。
  for (int i = 0; i < kButtonCount; i++) {
    int x0 = 0, y0 = 0, w = 0, h = 0;
    if (!SlotRect(i, &x0, &y0, &w, &h))
      break;
    if (x >= x0 && x < x0 + w && y >= y0 && y < y0 + h)
      return i;
  }
  return -1;
}

// ===== Window procedure =====
LRESULT CALLBACK QuickPanelDialog::WndProc(HWND hwnd,
                                           UINT msg,
                                           WPARAM w,
                                           LPARAM l) {
  switch (msg) {
    case WM_CREATE:
      return OnCreate(hwnd);
    case WM_DESTROY:
      return OnDestroy(hwnd);
    case WM_PAINT:
      return OnPaint(hwnd);
    case WM_ERASEBKGND:
      return 1;  // GDI 双缓冲,不让 Windows 清背景
    // L87-fix: panel 拖动支持 — 改用 **手动 drag** 替代 v0.19.0.16 L86 的
    // WM_NCHITTEST
    // + HTCAPTION。L86 的 HTCAPTION 在 WS_POPUP + WS_EX_LAYERED 窗口下 Windows
    // DefWindowProc 没有处理 system drag(L86 报告"无法拖动"是 user 反馈)。
    // 手动 drag 方法:
    //   WM_LBUTTONDOWN:HitTest 不在 button 内,SetCapture + 记录原 cursor pos +
    //   原 window pos WM_MOUSEMOVE:如果 captured,计算 delta + SetWindowPos 移动
    //   window WM_LBUTTONUP:ReleaseCapture 结束 drag
    // 这样 button 区域正常 click,空白区域可以拖动。
    case WM_NCHITTEST: {
      POINT p = {LOWORD(l), HIWORD(l)};
      ScreenToClient(hwnd, &p);
      int hit = HitTest(p.x, p.y);
      if (hit == -1)
        return HTCAPTION;  // 空白/品牌区 — 拖动
      return HTCLIENT;     // 按钮区 — 正常 click/hover
    }
    // L92-fix + 本次缺陷修复: WM_DPICHANGED 要重算 **尺寸** 和位置。
    // 之前的注释以为「保持 logical 尺寸不变,Windows 自动 scale」—— 那只对
    // DPI-unaware 进程成立;WeaselServer 是 PerMonitor aware,系统不会改窗口
    // 尺寸,所以必须自己按新 DPI 缩放,并重建随尺寸走的 physical 布局。
    case WM_DPICHANGED: {
      UINT dpiX = LOWORD(w);
      UINT dpiY = HIWORD(w);
      if (dpiX == 0)
        dpiX = 96;
      if (dpiY == 0)
        dpiY = 96;
      const int panelW = ScaleLogicalByDpi(kPanelW, dpiX);
      const int panelH = ScaleLogicalByDpi(kPanelH, dpiY);
      const int margin = ScaleLogicalByDpi(12, dpiX);
      // 新 monitor 的工作区 —— SPI_GETWORKAREA 只返回 primary,跨屏 DPI 切换
      // 时会算错,所以这里用 MonitorFromWindow + rcWork。
      RECT newRc = {};
      MONITORINFO mi = {};
      mi.cbSize = sizeof(mi);
      if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                           &mi))
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &newRc, 0);
      else
        newRc = mi.rcWork;
      int newX = 0;
      int newY = 0;
      if (HasSavedPosition()) {
        // spec 099 P0-7: 用户自己摆过位置 ⇒ DPI 变化时**保留**用户位置,只把它
        // 钳回可见区(否则面板会跑到看不见的地方);没有持久化位置 ⇒ 沿用右下角
        // 默认锚点(行为与本特性之前一致)。
        RECT cur = {};
        GetWindowRect(hwnd, &cur);
        const std::vector<fluxing::panel_position::Rect> areas =
            fluxing::panel_position::EnumerateWorkAreas();
        if (!areas.empty()) {
          const fluxing::panel_position::Point p =
              fluxing::panel_position::ClampToVisibleArea(cur.left, cur.top,
                                                          panelW, panelH, areas);
          newX = p.x;
          newY = p.y;
        } else {
          PlacePanelAtBottomRight(newRc, panelW, panelH, margin, &newX, &newY);
        }
      } else {
        PlacePanelAtBottomRight(newRc, panelW, panelH, margin, &newX, &newY);
      }
      SetWindowPos(s_hwnd, HWND_TOPMOST, newX, newY, panelW, panelH,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      // 尺寸变了: 重算 dpr / _phys 常量并重建 offscreen DIB,否则
      // UpdateLayeredWindow 提交的是旧大小的 surface
      ApplyPhysicalLayout(hwnd);
      // 重新画 panel
      InvalidateRect(s_hwnd, NULL, FALSE);
      RepaintLayered(s_hwnd);
      return 0;
    }
    case WM_LBUTTONDOWN: {
      POINT p = {LOWORD(l), HIWORD(l)};
      int hit = HitTest(p.x, p.y);
      // L88-fix: drag **任何位置**都启动(button + brand + 空白)。之前
      // v0.19.0.17 L87 只在 hit==-1 时启动,user 长按 brand area
      // 不响应。修复:无论 hit 是什么, SetCapture + record,这样整个 panel
      // 任何地方长按都拖动。button click 仍然 通过 s_activeIdx 处理(LButtonUp
      // 时检查 s_activeIdx 是否释放前还在同一按钮)。
      // spec 099 P0-5: 用户**主动点击**面板 ⇒ 接管键盘焦点(面板平时是
      // WS_EX_NOACTIVATE,收不到按键)。收起时 RestoreKeyboardFocus() 归还。
      CaptureKeyboardFocus(hwnd);
      SetCapture(hwnd);
      s_dragging = TRUE;
      // L178-fix: GetCursorPos 可能失败(无输入设备 / 非交互 window station)。
      // 失败时不能把未初始化的栈内存当成光标位置 —— 否则 LButtonUp 会拿垃圾值
      // 算位移,把「点击」误判成「拖拽」而吞掉回调(槽位点击随机失效)。
      // 记 s_dragCursorValid,抬起时按「无位移」处理。
      POINT start = {0, 0};
      s_dragCursorValid = (GetCursorPos(&start) != FALSE);
      if (s_dragCursorValid)
        s_dragStartCursor = start;
      RECT rc;
      GetWindowRect(hwnd, &rc);
      s_dragStartWindow = rc;
      if (hit >= 0) {
        s_activeIdx = hit;  // button click 仍然 work
        // L97-fix(Fix A):WS_EX_LAYERED 路径下 InvalidateRect 是死代码
        // (cpp:1060 注释:layered window 不在那画),必须直接 RepaintLayered。
        // 之前每像素 mouse 都触发 WM_MOUSEMOVE/InvalidateRect 但 screen 不更新,
        // 100ms 后 timer 才补画 → hover/active 滞后 + 重叠残留 → icons 扭曲感。
        RepaintLayered(hwnd);
      }
      return 0;
    }
    // spec 100 / ADR 0038: 品牌图标右键 ⇒ 弹出与托盘**同一份**菜单。
    // 面板只做命中 + 转发屏幕坐标;菜单本身由 WeaselServer 弹出
    // (CSystemTray::ShowPopupMenuAt),所以菜单资源/命令路由/勾选态
    // 与托盘右键天然一致,不存在「两套菜单」。
    case WM_RBUTTONDOWN: {
      POINT p = {LOWORD(l), HIWORD(l)};
      int bx = 0, by = 0, bw = 0, bh = 0;
      const bool on_brand = BrandRect(&bx, &by, &bw, &bh) && p.x >= bx &&
                            p.x < bx + bw && p.y >= by && p.y < by + bh;
      if (on_brand && s_onBrandRightClick) {
        POINT sp = p;
        ClientToScreen(hwnd, &sp);
        // TrackPopupMenu 是阻塞的(内部模态循环直到菜单被关掉)⇒ 期间
        // s_menuOpen 必须保持 true,否则轮询 timer 会把面板收走。
        s_menuOpen = true;
        s_onBrandRightClick(sp);
        s_menuOpen = false;
      }
      return 0;
    }
    case WM_MOUSEMOVE: {
      if (s_dragging) {
        // L178-fix: 取不到真实光标就无法算位移(否则窗口会瞬移)。
        POINT cur = {0, 0};
        if (!GetCursorPos(&cur))
          return 0;
        int dx = cur.x - s_dragStartCursor.x;
        int dy = cur.y - s_dragStartCursor.y;
        SetWindowPos(hwnd, NULL, s_dragStartWindow.left + dx,
                     s_dragStartWindow.top + dy, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      } else {
        // L87-fix 替代 v0.19.0.14 L84 polling timer 的 WM_MOUSEMOVE 路径
        // (L86 仍保留 polling timer,这里双保险)
        POINT p = {LOWORD(l), HIWORD(l)};
        int hit = HitTest(p.x, p.y);
        if (hit != s_hoveredIdx) {
          s_hoveredIdx = hit;
          // spec 099 P0-5: 悬停槽位 ⇒ 显示该槽 tooltip(名称 + 当前态);
          // 移出面板 ⇒ 收起 tooltip。
          if (hit >= 0)
            ShowTooltip(hit);
          else
            HideTooltip();
          // L97-fix(Fix A):WS_EX_LAYERED 路径必须 RepaintLayered,InvalidateRect
          // 死代码。
          RepaintLayered(hwnd);
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
        // v0.19.0.25-fix(ship blocker,Test Results Analyzer v0.19.0.25
        // 验收发现): L88 drag-any-area (cpp:449) + L94 Fix A (cpp:504) 让
        // LButtonUp 几乎总走 if-d 分支,且 if-d 分支只 reset state 不 invoke
        // click,导致 else 分支的 s_onPhrases() 调用永远走不到 — Phrase 按钮
        // click 死代码。 修法:在 if-d 分支内检测鼠标实际位移 (cur -
        // s_dragStartCursor),未超过 dragThreshold (4 物理像素) 视为 click →
        // fall through invoke 回调。 注意:必须 reset s_activeIdx 之后再判断
        // (L94 Fix A 是 reset,不能漏), 但要在 reset 之前用 oldActiveIdx 记住
        // index 用于路由。
        // L178-fix: GetCursorPos 失败时 cur 是未初始化内存 ⇒ 位移是垃圾值 ⇒
        // 点击被误判成拖拽而吞掉(槽位点击随机失效)。取不到光标、或按下时
        // 没拿到有效光标 ⇒ 一律按「无位移」处理,保证点击一定派发。
        POINT cur = {0, 0};
        const bool curValid = (GetCursorPos(&cur) != FALSE);
        int dx = 0, dy = 0;
        if (curValid && s_dragCursorValid) {
          dx = cur.x - s_dragStartCursor.x;
          dy = cur.y - s_dragStartCursor.y;
        }
        int dragDist2 = dx * dx + dy * dy;
        constexpr int kDragThresholdPx = 4;  // 4 物理像素,经验值(防手指抖动)
        bool actualDrag = (dragDist2 >= kDragThresholdPx * kDragThresholdPx);
        int oldActiveIdx = s_activeIdx;  // 记住 index 用于 click 路由

        s_dragging = FALSE;
        ReleaseCapture();
        // drag 结束后清 hover
        s_hoveredIdx = -1;
        // v0.19.0.24-fix(issue 3):补 s_activeIdx = -1。L88-fix (cpp:449) 让
        // LButtonDown 无条件 s_dragging=TRUE,所以 LButtonUp 几乎总走 if-d
        // 分支;之前 if-d 分支只 reset s_dragging + s_hoveredIdx,没 reset
        // s_activeIdx,导致 PaintOpaqueContent 看到 isActive=true 画橙
        // bg,永久残留。Fix A:补这一行(对称 else 分支已有)。
        s_activeIdx = -1;
        // L97-fix(Fix A):WS_EX_LAYERED 路径必须 RepaintLayered。
        RepaintLayered(hwnd);

        // v0.19.0.25-fix: drag 分支 fall-through click
        // 路由。如果鼠标未实际位移, 等同"按下并释放同一按钮" → invoke 回调。
        // v0.19.0.29:扩展为 5 按钮部分接通 — 0/4 仍 no-op (历史
        // placeholder,follow-up spec 加 ASCII mode toggle / 登录);1 Phrase / 3
        // Shortcut 真接通。 v0.19.0.60 (Phase L 调整 1): 2 (UserDict) 删除 —
        // UserDictionary 模块下线。 见 docs/design/mockups-v0.19.0.28/ 设计稿。
        // v0.19.0.62 (Phase L 调整 1.2): 5→4 buttons, hit 索引重映射:
        //   0 Schema, 1 Phrase, 2 Settings→Shortcut, 3 Account。
        // spec 099 P0-3: 0/3 不再是 no-op —— 每个可见槽位点击都必须产生
        // 确定的用户可见效果(0 = 切换到下一个输入方案,3 = 关于/账号说明页)。
        if (!actualDrag) {
          DispatchSlotClick(oldActiveIdx);
        } else {
          // spec 099 P0-7: 真的拖动了 ⇒ 把新位置落盘(跨重启保留)。
          // 只在 actualDrag 时写:纯点击不该产生写盘。
          SavePosition();
        }
      } else {
        POINT p = {LOWORD(l), HIWORD(l)};
        int hit = HitTest(p.x, p.y);
        if (hit >= 0 && hit == s_activeIdx) {
          // v0.19.0.25-fix(spec 042 §3 调用链):接通 s_onPhrases callback。
          // L88-fix (cpp:449) 让 LButtonDown 无条件 s_dragging=TRUE,所以 click
          // 分支 只在 hit==s_activeIdx 时才表示"按下与释放同一按钮"(否则算 drag
          // start)。 L94-fix A (cpp:504) 在 drag 分支 reset
          // s_activeIdx,所以这里对称: click 分支先检查 hit==s_activeIdx 再
          // invoke。 v0.19.0.29:扩展 hit 分支 — 1 Phrase / 3 Shortcut。0/2/4 仍
          // no-op。 v0.19.0.62 (Phase L 调整 1.2): 5→4 buttons, hit 索引重映射:
          //   hit==1 Phrase (不变), hit==2 Settings → s_onShortcut (was hit==3
          //   in v0.19.0.61) hit==3 Account (was hit==4)
          // spec 099 P0-3: 0/3 不再是 no-op —— 每个可见槽位点击都必须产生
          // 确定的用户可见效果(0 = 切换到下一个输入方案,3 = 关于/账号说明页)。
          DispatchSlotClick(hit);
        }
        s_activeIdx = -1;
        // L97-fix(Fix A):WS_EX_LAYERED 路径必须 RepaintLayered。
        RepaintLayered(hwnd);
      }
      return 0;
    }
    // L81-fix: 切到其他 IME (en-US 等) 时 WeaselServer 进程会失焦 →
    // WM_ACTIVATEAPP 触发。 自动 Hide() 让 panel 跟着前台 app
    // 切走,不残留屏幕上。 v0.19.0.30-fix(issue 1): 之前 polling timer (id=2)
    // 路径在 grace 期内不累加 outsideMs (cpp:611-612 模式),但 WM_ACTIVATEAPP
    // 路径直接 Hide,无 grace guard。 触发链:用户 Alt+. → Show() → 切回前台 app
    // → WM_ACTIVATEAPP(0) → 立即 Hide, user 还没看到 panel 就消失。修法:与
    // PhrasesDialog.cpp:722-730 同 pattern, 在 grace 期 (nowTick - s_showTime)
    // < kShowGraceMs 内不 Hide。
    case WM_ACTIVATEAPP: {
      if (w == FALSE) {  // app 被 deactivate (前台切走)
        DWORD nowTick = GetTickCount();
        // spec 084 / ADR 0013 D5: 长显态屏蔽「前台切走就隐藏」。
        if (!AlwaysShowMode() && !s_menuOpen && (nowTick - s_showTime) >=
                                       kShowGraceMs) {
          Hide();
        }
      }
      return 0;
    }
    // L84-fix: 用 polling timer (100ms) 替代仅靠 WM_MOUSEMOVE 更新
    // s_hoveredIdx。 原因:WS_EX_LAYERED + WS_EX_NOACTIVATE panel 下
    // WM_MOUSEMOVE 投递不可靠 (L83 sandbox 验证 hovered 实际仍 -1 即使我们
    // SendMessage WM_MOUSEMOVE)。 Polling GetCursorPos + ScreenToClient
    // 自己查鼠标位置,绕过 WM 投递。 L84-fix: 用 polling timer (id 2, 100ms)
    // 替代仅靠 WM_MOUSEMOVE 更新 s_hoveredIdx。 原因:WS_EX_LAYERED +
    // WS_EX_NOACTIVATE panel 下 WM_MOUSEMOVE 投递不可靠 (L83 sandbox 验证
    // hovered 实际仍 -1 即使我们 SendMessage WM_MOUSEMOVE)。 Polling
    // GetCursorPos + ScreenToClient 自己查鼠标位置,绕过 WM 投递。
    case WM_TIMER: {
      if (w == 2) {  // hover polling timer (id 2, 100ms) — 同时负责 auto-hide
        // spec 099 P0-4: 在**已有的**轮询 timer 里拉一次状态快照 ⇒
        // 显示路径零 I/O(守 P2)。快照来自服务端注入的 provider(纯内存读)。
        RefreshInputState();
        POINT p;
        if (GetCursorPos(&p) && ScreenToClient(hwnd, &p)) {
          int hit = HitTest(p.x, p.y);
          if (hit != s_hoveredIdx) {
            s_hoveredIdx = hit;
            // spec 099 P0-5: hover 真相源是本轮询(L84-fix 已证 WM_MOUSEMOVE
            // 投递不可靠),所以 tooltip 也在这里同步,不能只靠 WM_MOUSEMOVE。
            // ShowTooltip 内部做槽位越界保护。
            if (hit >= 0)
              ShowTooltip(hit);
            else
              HideTooltip();
            // L97-fix(Fix A):WS_EX_LAYERED 路径必须 RepaintLayered。
            RepaintLayered(hwnd);
          }
          // L89-fix: auto-hide 当鼠标在 panel 外超过 1.5 秒。之前 v0.19.0.18
          // panel 显示后没有 auto-hide 时机(WS_EX_NOACTIVATE 收不到
          // OnKillFocus),挡 user 输入区 → "无法输入中文"。修复:polling timer
          // 检查 hit==-1 (panel 外) 时累加 s_outsideMs,达到 1500ms 自动 Hide。
          // v0.19.0.24-fix(issue 4):Show() 后 grace period
          // (kShowGraceMs=2000ms)。 之前 L89-fix 无 grace,首次 Show 时 cursor
          // 还在 panel 外,直接 1.5s 内 Hide。 修法:在 grace 期间 (now -
          // s_showTime) < kShowGraceMs 不累加 outsideMs, 但 polling 仍更新
          // s_hoveredIdx(让 user 移 cursor 进 panel 时立即 hover)。
          DWORD nowTick = GetTickCount();
          bool inGrace = (nowTick - s_showTime) < kShowGraceMs;
          // spec 084 / ADR 0013 D3: 整体不透明度系数。
          // 目标:鼠标在面板矩形内 => 满值;否则长显态 85%,瞬时态仍是满值。
          // 瞬时态 target 恒为满值 => 系数永不动 => 零重绘、零像素变化。
          // 用 GetClientRect 而不是 HitTest:按钮之间的空隙 HitTest 返回 -1,
          // 但鼠标确实还在面板上。
          RECT clientRect;
          GetClientRect(hwnd, &clientRect);
          const bool cursorInside =
              p.x >= clientRect.left && p.x < clientRect.right &&
              p.y >= clientRect.top && p.y < clientRect.bottom;
          s_targetAlphaFactor =
              cursorInside ? fluxing::kAlphaFactorFull
                           : (AlwaysShowMode() ? fluxing::kAlphaFactorIdle
                                               : fluxing::kAlphaFactorFull);
          const int nextFactor = fluxing::StepAlpha(
              s_alphaFactor, s_targetAlphaFactor, fluxing::kAlphaFadeStep);
          if (nextFactor != s_alphaFactor) {
            s_alphaFactor = nextFactor;
            // WS_EX_LAYERED 路径必须 RepaintLayered,InvalidateRect 是死代码。
            RepaintLayered(hwnd);
          }
          // spec 084 / ADR 0013 D5: 长显态屏蔽「鼠标在外 1500ms 自动隐藏」。
          if (!AlwaysShowMode() && !s_menuOpen && hit == -1 && !s_dragging &&
              !inGrace) {
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
      // spec 099 P0-5: 鼠标离开面板 ⇒ 收起 tooltip。
      HideTooltip();
      if (s_hoveredIdx != -1) {
        s_hoveredIdx = -1;
        // L97-fix(Fix A):WS_EX_LAYERED 路径必须 RepaintLayered。
        RepaintLayered(hwnd);
      }
      return 0;
    }
    case WM_KILLFOCUS:
      return OnKillFocus(hwnd);
    // spec 099 P0-5: 面板拿到键盘焦点 ⇒ 进入键盘交互态。
    // 此时 Esc 才收起面板(未交互时 Esc 只被吞掉,守 ADR 0019 D2)。
    case WM_SETFOCUS:
      s_keyboard.interactive = true;
      s_keyboard.slot_count = kButtonCount;
      if (s_hwnd)
        RepaintLayered(s_hwnd);
      return 0;
    // spec 099 P0-5: 面板不是输入框 ⇒ 字符一律吞掉(不产生 beep、不进 IME)。
    case WM_CHAR:
      return 0;
    case WM_KEYDOWN:
      return OnKeyDown(hwnd, w);
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, w, l);
}

// 由窗口客户区尺寸(物理像素)推导 physical 布局。
// Show() 建窗时已按 monitor DPI 把窗口缩放到物理尺寸,所以这里从客户区反推
// dpr —— OnCreate 与 WM_DPICHANGED(DPI 切换后尺寸变了)都走这一条路。
bool QuickPanelDialog::ApplyPhysicalLayout(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int w = rc.right - rc.left;
  int h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0)
    return false;

  // L83-fix: DPI 缩放。计算 dpr 并把所有设计常量 (logical 245x48) 缩放到
  // 物理 surface 大小。显示器 sub-100% DPI 时窗口被系统压小,不缩放的话
  // drawing 用 logical 值出界 → icons 看不见 / HitTest 错位;150% DPI 时反过来
  // 需要把常量放大到 1.5 倍物理像素。
  // spec 099 P0-6: 布局推演下沉到 ComputeLayout(单一真相源)—— 槽位矩形、
  // 状态区矩形、单测用的 *ForSize 全部读同一份。
  const PanelLayout L = ComputeLayout(w, h);
  s_panelW_phys = w;
  s_panelH_phys = h;
  s_dpr_x = L.dpr_x;
  s_dpr_y = L.dpr_y;
  s_panelPadding_phys = L.padding;
  // v0.19.0.24-fix(issue 1):Y 方向 padding 独立算,scale_y 让跨 DPI 跟随
  s_btnYOffset_phys = L.btn_y_offset;
  s_btnSize_phys = L.btn_size;
  s_icoSize_phys = ScaleRound(kIcoSize, L.dpr_y);
  s_btnRadius_phys = ScaleRound(kBtnRadius, L.dpr_x);
  s_brandSize_phys = L.brand_size;
  s_panelRadius_phys = ScaleRound(kPanelRadius, L.dpr_x);
  s_btnGap_phys = L.btn_gap;
  // spec 099 P0-4: 状态文字字体随 dpr 重建(旧字体必须先释放,否则 GDI 泄漏)。
  if (s_hFontStatus) {
    DeleteObject(s_hFontStatus);
    s_hFontStatus = NULL;
  }
  {
    const int char_h =
        ScaleRound(fluxing::panel_status::kStatusCharHeight, L.dpr_y);
    s_hFontStatus = CreateFontW(-char_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
  }
  // off-screen DC 尺寸必须跟着窗口走,否则 UpdateLayeredWindow 提交旧 surface
  CreateOffscreenDC(w, h);
  return true;
}

LRESULT QuickPanelDialog::OnCreate(HWND hwnd) {
  s_hwnd = hwnd;
  if (!ApplyPhysicalLayout(hwnd))
    return 0;

  // 创建资源
  LoadLogoWIC(hwnd, s_hBmpLogo);
  s_hBrushPanelBg = CreateSolidBrush(kBgTop);
  s_hBrushIconDim = CreateSolidBrush(kIcoDimC);
  s_hBrushIconAccent = CreateSolidBrush(kAccentC);
  s_hBrushActive = CreateSolidBrush(kAccentC);
  s_hBrushHighlight = CreateSolidBrush(kHighlight);
  s_hBrushShadow =
      CreateSolidBrush(RGB(200, 215, 235));  // 底部阴影 (L85:浅蓝,避免黑线)
  s_hPenIconDim = CreatePen(PS_SOLID, 2, kIcoDimC);
  s_hPenIconAccent = CreatePen(PS_SOLID, 2, kAccentC);
  s_hPenHighlight = CreatePen(PS_SOLID, 1, kHighlight);

  // off-screen DC 与 physical 布局已由 ApplyPhysicalLayout 建好
  return 0;
}

LRESULT QuickPanelDialog::OnDestroy(HWND hwnd) {
  s_hoveredIdx = -1;
  s_activeIdx = -1;
  // spec 099 P0-5: tooltip 窗口与字体随面板一起销毁(不留孤儿顶层窗)。
  HideTooltip();
  if (s_hwndTooltip && IsWindow(s_hwndTooltip)) {
    DestroyWindow(s_hwndTooltip);
  }
  s_hwndTooltip = NULL;
  if (s_hFontTooltip) {
    DeleteObject(s_hFontTooltip);
    s_hFontTooltip = NULL;
  }
  // spec 099 P0-4: 状态文字字体。
  if (s_hFontStatus) {
    DeleteObject(s_hFontStatus);
    s_hFontStatus = NULL;
  }
  DestroyOffscreenDC();
  if (s_hBmpLogo) {
    DeleteObject(s_hBmpLogo);
    s_hBmpLogo = NULL;
  }
  if (s_hBrushPanelBg) {
    DeleteObject(s_hBrushPanelBg);
    s_hBrushPanelBg = NULL;
  }
  if (s_hBrushIconDim) {
    DeleteObject(s_hBrushIconDim);
    s_hBrushIconDim = NULL;
  }
  if (s_hBrushIconAccent) {
    DeleteObject(s_hBrushIconAccent);
    s_hBrushIconAccent = NULL;
  }
  if (s_hBrushActive) {
    DeleteObject(s_hBrushActive);
    s_hBrushActive = NULL;
  }
  if (s_hBrushHighlight) {
    DeleteObject(s_hBrushHighlight);
    s_hBrushHighlight = NULL;
  }
  if (s_hBrushShadow) {
    DeleteObject(s_hBrushShadow);
    s_hBrushShadow = NULL;
  }
  if (s_hPenIconDim) {
    DeleteObject(s_hPenIconDim);
    s_hPenIconDim = NULL;
  }
  if (s_hPenIconAccent) {
    DeleteObject(s_hPenIconAccent);
    s_hPenIconAccent = NULL;
  }
  if (s_hPenHighlight) {
    DeleteObject(s_hPenHighlight);
    s_hPenHighlight = NULL;
  }
  if (s_hwnd == hwnd)
    s_hwnd = NULL;
  return 0;
}

// PaintOpaqueContent: v0.19.0.10 新抽出。把原 OnPaint 的"画 opaque 内容到
// s_hdcMem" 部分抽出来 — roundRgn WHITE bg + 1px border + top highlight + logo
// + 5 icons。 不再 BitBlt 到 window DC (WS_EX_LAYERED 后双路径会闪烁,见 file
// header 雷区)。
void QuickPanelDialog::PaintOpaqueContent(HDC hdc) {
  // L83-fix: 全部用 physical 像素。surface 物理大小 = s_panelW_phys ×
  // s_panelH_phys, layout 物理常量已按 dpr 缩放。
  const int W = s_panelW_phys;
  const int H = s_panelH_phys;
  const int pad = s_panelPadding_phys;
  const int radius = s_panelRadius_phys;

  // 1a. 圆角 panel 背景 — 浅玻璃冷色,L82-fix:任何背景下都可见
  HRGN panelRgn = CreateRoundRectRgn(0, 0, W, H, radius, radius);
  FillRgn(hdc, panelRgn, s_hBrushPanelBg);
  DeleteObject(panelRgn);

  // 1b. 1px 深灰 border — L82-fix:border RGB(50,50,60) opaque 永远可见
  // L83-fix: FrameRgn 在 sub-100% DPI scale (e.g. dpr=0.667) 下 1px brush =
  // 0.667 physical = sub-pixel → GDI 不绘制。改用 **RoundRect() with pen +
  // NULL_BRUSH** 画 outline。 RoundRect() 在所有 DPI 下都会画出
  // outline(因为它是 line primitive,不是 brush fill)。注意 RoundRect 边界是
  // (x1, y1) 到 (x2, y2) inclusive,不填内部。 border 宽度至少 1 physical
  // pixel:对于 dpr=0.667,取 2 logical (1.33 physical); 对于 dpr=1.0,取 1
  // logical (=1 physical);对于 dpr>=1.5,取 max(1, 1)=1 logical。
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

  // 1c. 顶部 1px 高光(L83 mac 风格:顶部反射,白色) — L93-fix(v0.19.0.23):
  // 物理缩放:min 1px(低 DPI 不能画 0px)。
  // 历史:v0.19.0.18 之前只占 1 物理像素(y=1 那行),v0.19.0.20 误改 2 px 让 icons
  // 视觉偏下。L93 (v0.19.0.23 撤销的 wrong 方案) 也回 1 px — **v0.19.0.23 保留
  // 这次 1 px 修改**(同时改了真正的 icon 居中算法,1px highlight 不再需要 ±1
  // fudge)。
  int hlHeight = max(1, (int)(1 * s_dpr_y + 0.5f));
  RECT topHL = {radius, 1, W - radius, 1 + hlHeight};
  FillRect(hdc, &topHL, s_hBrushHighlight);

  // 1d. 底部 1px 阴影 (L83 mac 风格:底部阴影线)
  int shHeight = max(1, (int)(1 * s_dpr_y + 0.5f));
  RECT botShadow = {radius, H - 4 - shHeight, W - radius, H - 4};
  FillRect(hdc, &botShadow, s_hBrushShadow);

  // 2. 画 logo (Fluxing 红色猿猴)
  // v4 需求⑦: 改用 **BitBlt(SRCCOPY) 1:1 贴图**, 不再走 GDI 的 alpha 合成。
  //
  // 【历史】v0.19.0.10 用 BitBlt(SRCCOPY), 但那时源 PNG 含 alpha=0 且 RGB=0 的
  // 透明像素 —— GDI 的 SRCCOPY 不看 alpha 通道, 直接把 RGB 复制过去, 于是整张
  // logo 变成「黑底 + 红 logo」。为此 v0.19.0.10 改用 AlphaBlend(AC_SRC_ALPHA)。
  //
  // 【v4 的二次根因】真机渲染落盘取证证明 AlphaBlend 这条路径在本面板上把 logo
  // 的**半透明边缘像素画成了近黑**(面板像素 ≈ 255 − 源 alpha), 这才是用户看到
  // 的「品牌标志上的黑色框」—— 与素材本身无关。
  //
  // 【现在的做法】LoadLogoWIC 已把 logo 预合成到面板底色 kBgTop 上(alpha 恒
  // 255 的不透明位图), 每个像素的 RGB 已是最终融合色 ⇒ SRCCOPY 不会出错,
  // 且完全绕开 alpha 合成。src/dst 都是 s_brandSize_phys ⇒ 1:1, 不需拉伸
  // (BitBlt/AlphaBlend 都不支持拉伸, 预缩放在 WIC 阶段做)。
  if (s_hBmpLogo) {
    HDC hdcMemLogo = CreateCompatibleDC(hdc);
    if (hdcMemLogo) {
      HGDIOBJ prev = SelectObject(hdcMemLogo, s_hBmpLogo);
      BitBlt(hdc, pad, pad, s_brandSize_phys, s_brandSize_phys, hdcMemLogo,
             0, 0, SRCCOPY);
      SelectObject(hdcMemLogo, prev);
      DeleteDC(hdcMemLogo);
    }
  }

  // 3. 画 5 个按钮 — L83-fix: 用 s_*_phys 常量
  // L79-fix: hover 只改 icon stroke 颜色(不画 bg 填充,符合 v3-rev3 设计)
  // L87-fix: brand→btns 间距 4 logical (dpr=1 → 4 物理,dpr=0.7 → 2.8→2 物理)。
  // 给个 max(2, ...) 保证 ≥ 2 物理像素(在 dpr=0.7 时也能看到间距)。
  // L91-fix: buttonStartX 起始 gap 4→2 (v0.19.0.20 是 4 dpr=1 → 4 logical = 4
  // 物理 px)。 现在 kBtnGap 改 4,buttons 之间有足够间距,起始 gap 不用再大。减到
  // 2 px 让 brand→btns 间距合理,btn4 右边距 +2 px (panelW=252
  // 5*37+4*4=201+44+5=250 < 252)
  int buttonStartX = pad + s_brandSize_phys + max(QuickPanelDialog::kBrandGap, (int)(QuickPanelDialog::kBrandGap * s_dpr_x + 0.5f));
  // v0.19.0.62 (Phase L 调整 1.2): 5→4 buttons (UserDict 槽位删除)
  for (int i = 0; i < kButtonCount; i++) {
    int x0 = buttonStartX + i * (s_btnSize_phys + s_btnGap_phys);
    // v0.19.0.24-fix(issue 1):Y 起点改用
    // s_btnYOffset_phys(=(kPanelH-kBtnSize)/2 dpr 缩放)。 之前 y0=pad=5,btn at
    // y=[5..40] 距顶 5 距底 8 偏上 1.5 px。改 y0=6 → btn at y=[6..41] 距顶 6
    // 距底 7(整数下最接近对称);btn 中心 23.5 vs panel 中心 24,差 0.5 px
    // 不可避免(48-35=13 奇数,1 px 离散)。同步 HitTest Y 范围。
    int y0 = s_btnYOffset_phys;

    bool isActive = (i == s_activeIdx);
    bool isHover = (i == s_hoveredIdx);

    // L86-fix: hover **只改 icon stroke 颜色** — user 明确要求"hover 时 bg
    // 不变, 只 icon 线条变橙"。v0.19.0.15 我加的"三重视觉反馈"(bg 填橙 +
    // 描边)是过度, user 不要 bg 变化。移除 hover 时的 bg fill + 描边,只保留
    // icon stroke 改色。
    HBRUSH bgBrush = NULL;
    if (isActive) {
      bgBrush =
          s_hBrushActive;  // active: 实橙 (RGB 255,95,49) — spec 070 T007 设计
    }
    // hover: NO bg change. Only icon stroke color changes below.

    if (bgBrush) {
      HRGN rgn =
          CreateRoundRectRgn(x0, y0, x0 + s_btnSize_phys, y0 + s_btnSize_phys,
                             s_btnRadius_phys, s_btnRadius_phys);
      FillRgn(hdc, rgn, bgBrush);
      DeleteObject(rgn);
    }

    // 1px 描边(只 active,hover 不描边)
    if (isActive) {
      HPEN outlinePen =
          CreatePen(PS_SOLID, max(1, (int)(1.0f * s_dpr_x + 0.5f)),
                    RGB(220, 60, 30));  // active: 暗橙
      HPEN oldOutline = (HPEN)SelectObject(hdc, outlinePen);
      HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
      RoundRect(hdc, x0, y0, x0 + s_btnSize_phys, y0 + s_btnSize_phys,
                s_btnRadius_phys, s_btnRadius_phys);
      SelectObject(hdc, oldBrush);
      SelectObject(hdc, oldOutline);
      DeleteObject(outlinePen);
    }

    // spec 099 P0-5: 键盘焦点环。只在**已进入键盘交互态**时画(否则鼠标用户
    // 会看到莫名其妙的虚线框)。PS_DOT 虚线 + 蓝色,画在按钮外侧 2px。
    if (s_keyboard.interactive && i == s_keyboard.focused) {
      const int ring = max(1, (int)(2 * s_dpr_x + 0.5f));
      HPEN focusPen = CreatePen(PS_DOT, max(1, (int)(1.0f * s_dpr_x + 0.5f)),
                                 RGB(0, 120, 215));
      HPEN oldFocusPen = (HPEN)SelectObject(hdc, focusPen);
      HBRUSH oldFocusBrush =
          (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
      RoundRect(hdc, x0 - ring, y0 - ring, x0 + s_btnSize_phys + ring,
                y0 + s_btnSize_phys + ring, s_btnRadius_phys + ring,
                s_btnRadius_phys + ring);
      SelectObject(hdc, oldFocusBrush);
      SelectObject(hdc, oldFocusPen);
      DeleteObject(focusPen);
    }

    HPEN iconPen;
    if (isActive)
      iconPen = (HPEN)GetStockObject(WHITE_PEN);
    else if (isHover)
      iconPen = s_hPenIconAccent;
    else
      iconPen = s_hPenIconDim;
    HPEN oldPen = (HPEN)SelectObject(hdc, iconPen);

    // L86-fix: 把 pen 颜色 RGB 直接传给 DrawIcon*,而不是 hardcoded kIcoDimC。
    // hover=橙 kAccentC,active=白 (WHITE_PEN),default=灰 kIcoDimC。
    // 之前 v0.19.0.15 DrawIcon* hardcoded kIcoDimC,hover 状态机的 pen 选择完全
    // 被忽略。
    DWORD penRgb = isActive  ? RGB(255, 255, 255)
                   : isHover ? kAccentC
                             : kIcoDimC;

    // L93-fix(v0.19.0.23,7 轮 ±1 fudge 失败的根本修法):
    // 真正视觉居中 = 把每个 icon 的 viewBox bbox 中心对准按钮中心。
    // 之前 7 轮 fix 一直在 fudge `iconX = x0 + (s_btnSize_phys -
    // s_icoSize_phys)/2 ± 1`, 假设 icon visual bbox = 19×19 (kIcoSize
    // box)。错!DrawIcon* 描线实际 bbox:
    //   Schema X[5..25] Y[8..22]    → mid (15, 15)
    //   Phrase X[5..25] Y[5..25]    → mid (15, 15)
    //   Symbols X[3..27] Y[6..24]   → mid (15, 15)
    //   Settings X[4..26] Y[4..26]  → mid (15, 15)
    //   Account X[5..25] Y[5..28]   → mid (15, 16.5)
    // 实际 bbox 21..25 × 15..24,中心 (15, 15),远大于 icoSize box(19)。btn=35 时
    // 旧算法 iconX = x0+8,bbox X 落 [x0+13..x0+33],mid 23 vs btn mid 17.5 →
    // 偏右 5.5 px(等同 user "水平未居中,图标居于右下角")。Y 同样 ±1 fudge 后 =
    // x0+9,bbox Y [y0+17..y0+31],mid y0+24 vs btn mid y0+17.5 → 偏下 6.5 px。
    // 任何 ±1 fudge 都不可能让 35-btn 中心对齐 21-wide bbox 中心。
    //
    // 真修法:iconX = x0 + s_btnSize_phys/2 − kIconBboxCxOff(不 × dpr)
    //        iconY = y0 + s_btnSize_phys/2 − kIconBboxCyOff(不 × dpr)
    // kIconBboxCxOff=15 是**屏幕像素**不是 viewBox logical 比例 — 因为
    // DrawIcon* 内部坐标(5, 25 等)是 raw pixel 偏移,不乘 dpr。所以居中常量也用
    // raw pixel。 btn=35 dpr=1:iconX = x0 + 17.5 − 15 = x0 + 2.5(int
    // truncation: 17 - 15 = 2), bbox X [x0+7..x0+27] mid x0+17(int 中点),btn
    // pixel mid x0+17 ✓。 dpr=1.5:s_btnSize=53,iconX = x0 + 26 − 15 = x0 +
    // 11,bbox [x0+16..x0+36] mid x0+26 (int 中点),btn pixel mid x0+26 ✓(53/2
    // = 26.5,int mid = 26)。 跨 DPI 都对齐。注意:icon 描线 raw pixel
    // 不缩放,设计固定 5..25 宽度 (20 px) 在所有 DPI 都一样,designer 刻意 — 高
    // DPI 不会按比例放大,保持视觉清晰度。
    int iconX = x0 + s_btnSize_phys / 2 - kIconBboxCxOff;
    int iconY = y0 + s_btnSize_phys / 2 - kIconBboxCyOff;
    // v0.19.0.62 (Phase L 调整 1.2): 5→4 buttons,case 重新映射:
    //   0 Schema / 1 Phrase / 2 Settings (was 3) / 3 Account (was 4)
    //   — case 2 (UserDict) 整槽已删,无对应 icon 绘制。
    switch (i) {
      case 0:
        DrawIconSchema(hdc, iconX, iconY, penRgb);
        break;
      case 1:
        DrawIconPhrase(hdc, iconX, iconY, penRgb);
        break;
      case 2:
        DrawIconSettings(hdc, iconX, iconY, penRgb);
        break;  // was hit==3 in v0.19.0.61
      case 3:
        DrawIconAccount(hdc, iconX, iconY, penRgb);
        break;  // was hit==4 in v0.19.0.61
      case 4:
      case 5: {
        // Fluxing v4 需求③⑤: 全半角 / 中英文两个**文字键**。
        // 键帽上直接画当前态单字(「中」/「英」, 「全」/「半」), 取代 v4 之前
        // 面板右侧的竖排文字回显(需求⑥)。复用 s_hFontStatus(YaHei 粗体),
        // 不新增 GDI 字体资源。
        if (!s_hFontStatus)
          break;
        const wchar_t* label =
            (i == 4)
                ? fluxing::panel_status::AsciiModeText(s_inputState.ascii_mode)
                : fluxing::panel_status::ShapeModeText(s_inputState.fullwidth);
        HGDIOBJ oldFont = SelectObject(hdc, s_hFontStatus);
        const int oldBk = SetBkMode(hdc, TRANSPARENT);
        const COLORREF oldColor = SetTextColor(hdc, penRgb);
        RECT tr = {x0, y0, x0 + s_btnSize_phys, y0 + s_btnSize_phys};
        DrawTextW(hdc, label, -1, &tr,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SetTextColor(hdc, oldColor);
        SetBkMode(hdc, oldBk);
        SelectObject(hdc, oldFont);
        break;
      }
    }
    SelectObject(hdc, oldPen);
  }

  // Fluxing v4 (用户指令⑥): 取消右侧竖排「中/英 · 全/半」文字回显 ——
  // 该回显是面板宽度 245 的主要占用者; 状态显示改由槽位键帽直接承担
  // (槽 4 中英文键 + 槽 5 全半角键, 见上方 switch(i) 的 case 4/5)。
  // StatusRect 定义保留(恒返回 false), 命中区语义不变。
}

// ApplyAlphaGradient: v0.19.0.10 新增 per-pixel alpha pipeline。
// 把 32-bit DIB 的 alpha 通道从默认 alpha=255 (GDI 写入默认) 改成 liquid glass
// 形状:
// - 圆角外 (corner & outside) alpha = 0 → 桌面可见
// - 圆角内,RGB == 纯白 (panel bg / border / top highlight): 按 y 渐变
// - 圆角内,RGB 含色 (icons / logo / active orange bg): 保留 alpha=255 (opaque
// 图形)
//
// L83-fix: 使用 GDI 的 `PtInRegion(rgn)` 判定 inside/outside,不是手算
// IsInsideRoundedRect。 原因:sub-100% DPI scale (e.g. dpr=0.667)
// 下,`CreateRoundRectRgn(0,0,W,H,r,r)` 实际 rgn 跟数学 r² 圆不完全一致(尤其在
// corners 圆弧),导致 IsInsideRoundedRect 数学判断 比 GDI 的 rgn 更严格(认为
// outside 的 pixel,GDI 反而认为 inside — 已被 FillRgn 填 panel bg)。直接用
// PtInRegion(rgn, x, y) 完全匹配 GDI 实际填充区域。
void QuickPanelDialog::ApplyAlphaGradient() {
  if (!s_hBmpMem || s_panelW_phys <= 0 || s_panelH_phys <= 0)
    return;
  BITMAP bm = {};
  if (!GetObject(s_hBmpMem, sizeof(bm), &bm) || !bm.bmBits)
    return;

  DWORD* p = static_cast<DWORD*>(bm.bmBits);
  const int W = s_panelW_phys;
  const int H = s_panelH_phys;
  const int r = s_panelRadius_phys;
  const int Hminus1 = (H > 1) ? (H - 1) : 1;

  // 复用 CreateRoundRectRgn 创建的 region,让 GDI 自己判断 inside/outside
  HRGN panelRgn = CreateRoundRectRgn(0, 0, W, H, r, r);

  // spec 084 / ADR 0013 D1/D2: 系数乘进逐像素 alpha。
  // factor == 255 => final_alpha == base_alpha => 与 spec 084 之前逐像素
  // 一致(零回归保证)。LWA_ALPHA uniform 仍然是雷区,不碰。
  const int factor = s_alphaFactor;
  for (int y = 0; y < H; y++) {
    int aPanel =
        kAlphaPanelTop + ((int)(kAlphaPanelBot - kAlphaPanelTop) * y / Hminus1);

    for (int x = 0; x < W; x++) {
      DWORD* px = &p[(size_t)y * W + x];
      if (!PtInRegion(panelRgn, x, y)) {
        *px = 0;
        continue;
      }
      BYTE r8 = (*px >> 16) & 0xFF;
      BYTE g8 = (*px >> 8) & 0xFF;
      BYTE b8 = *px & 0xFF;
      // L87-fix: 阈值从 r >= 240 改成 r >= 130 + g >= 150 + b >= 180。
      // 之前 kBgTop = (255, 255, 255) 接近白,255 阈值 work。L87 改 kBgTop =
      // (200, 225, 250) (浅冷蓝),200 < 240,else 分支 (alpha=255)
      // 生效,**gradient 完全没作用**。 新阈值匹配 kBgBot = (155, 195, 240) — b8
      // >= 180 确保 panel bg 像素 (含冷蓝) 命中 gradient alpha 分支。
      if (r8 >= 130 && g8 >= 150 && b8 >= 180) {
        *px = ((DWORD)fluxing::ScaleAlpha(aPanel, factor) << 24) | (r8 << 16) |
              (g8 << 8) | b8;
      } else {
        *px = ((DWORD)fluxing::ScaleAlpha(255, factor) << 24) | (r8 << 16) |
              (g8 << 8) | b8;
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
// WS_EX_LAYERED 后 WM_PAINT 路径不再有效 (BeginPaint/EndPaint 拿到 DC 但
// layered window 不在那画) — 改成走 UpdateLayeredWindow 一次性提交。Screen
// 上看是 透明 + 半透明白底图标,符合 Liquid Glass 设计意图。
void QuickPanelDialog::RepaintLayered(HWND hwnd) {
  if (!hwnd || !s_hdcMem || !s_hBmpMem || s_panelW_phys <= 0 ||
      s_panelH_phys <= 0)
    return;

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
    // (c) qp-state.txt = s_hoveredIdx / s_activeIdx / s_panelW_phys at paint
    // time
    {
      HANDLE hf =
          CreateFileW(L"F:\\soft\\00selfmade\\rime_claude\\qp-state.txt",
                      GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (hf != INVALID_HANDLE_VALUE) {
        DWORD written;
        char mb[256];
        int len = _snprintf_s(mb, _TRUNCATE,
                              "state: hovered=%d active=%d panelW=%d panelH=%d "
                              "dpr=%.3f radius_phys=%d\n",
                              s_hoveredIdx, s_activeIdx, s_panelW_phys,
                              s_panelH_phys, s_dpr_x, s_panelRadius_phys);
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
        HANDLE fL = CreateFileW(
            L"F:\\soft\\00selfmade\\rime_claude\\qp-logo.bmp", GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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

  // window position: client → screen, 给 UpdateLayeredWindow 的 destination
  // origin
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

  if (!UpdateLayeredWindow(hwnd, NULL, &ptPos, &sizeWnd, s_hdcMem, &ptSrc, 0,
                           &blend, ULW_ALPHA)) {
    // UpdateLayeredWindow 失败 — 调试钩子 (L74 教训)。之前 (void)err 把错误码
    // 吞掉,现场无法归因;现按 spec 097 的 include/FluxingDiagLog.h 口径落一行
    // (写失败本身静默忽略,诊断通道不会成为故障源)。
    const DWORD err = GetLastError();
    std::string line = "quickpanel_updatelayered_failed err=";
    line += std::to_string((unsigned long long)err);
    line += " size=";
    line += std::to_string(sizeWnd.cx);
    line += "x";
    line += std::to_string(sizeWnd.cy);
    line += " pos=";
    line += std::to_string((long long)ptPos.x);
    line += ",";
    line += std::to_string((long long)ptPos.y);
    line += " dpr=";
    line += std::to_string((double)s_dpr_x);
    fluxing::diag::AppendLine("server", line);
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

// v0.19.0.32-fix (Bug 2): 删除 OnLButtonUp / OnLButtonDown stub 函数(原
// cpp:1108-1109)。 这两个 stub 在 v0.19.0.10 layered 重构时留作 placeholder,但
// WndProc cpp:562-569 已经把 WM_LBUTTONUP 真 fire 路径(s_onPhrases /
// s_onUserDict / s_onShortcut)内联 实现了,所以 stub 永远不会被调用 — 是 dead
// code。删除以免误导后续 reader 以为 按钮路由通过 stub 走(实际是 WndProc 内联
// case)。
void QuickPanelDialog::OnMouseMove(HWND) {}
void QuickPanelDialog::OnMouseLeave(HWND) {}

LRESULT QuickPanelDialog::OnTimer(HWND hwnd, WPARAM w) {
  if (w == 1) {
    KillTimer(hwnd, 1);
    // spec 084 / ADR 0013 D5: 长显态不自动隐藏。
    if (!AlwaysShowMode()) {
      Hide();
    }
  }
  return 0;
}

LRESULT QuickPanelDialog::OnKillFocus(HWND hwnd) {
  // spec 084 / ADR 0013 D5: 长显态不自动隐藏 => 不 arm timer 1。
  if (AlwaysShowMode())
    return 0;
  SetTimer(hwnd, 1, 1000, NULL);
  return 0;
}

LRESULT QuickPanelDialog::OnKeyDown(HWND hwnd, WPARAM key) {
  // spec 099 P0-5: 键盘可达性统一走 include/FluxingPanelKeyboard.h 的状态机
  // (单一真相源,constitution R7)。Esc 语义由「键盘交互态」调和:
  //   - 未交互 ⇒ 只吞掉,面板不隐藏(守 ADR 0019 D2「显隐只靠 Alt+,」);
  //   - 已交互 ⇒ 只收起**面板自身**,不触碰任何其他窗口(守 D0032:
  //     按 Esc 关「用户短语」窗口不得连带收起快捷设置栏)。
  const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
  const fluxing::panel_keyboard::Action act =
      fluxing::panel_keyboard::HandleKey(s_keyboard, static_cast<int>(key),
                                          shift);
  switch (act) {
    case fluxing::panel_keyboard::Action::kSwallow:
      return 0;
    case fluxing::panel_keyboard::Action::kMoveFocus:
      if (s_hwnd)
        RepaintLayered(s_hwnd);
      return 0;
    case fluxing::panel_keyboard::Action::kActivate:
      if (s_keyboard.focused >= 0)
        DispatchSlotClick(s_keyboard.focused);
      return 0;
    case fluxing::panel_keyboard::Action::kClose:
      Hide();
      return 0;
    case fluxing::panel_keyboard::Action::kNone:
      break;
  }
  return DefWindowProc(hwnd, key, 0, 0);
}

// ===== D0176:屏外面板自愈 =====
namespace {

// 面板当前矩形若与**所有**显示器工作区零交集 ⇒ 挪回主屏右下角默认锚点。
// 返回 true 表示真的重定位过。
//
// 为什么需要它(2026-11-06 事故):Show() 的复用路径原本只 ShowWindow + 置顶,
// 零重定位。用户把面板拖到屏外松手 ⇒ WM_LBUTTONUP 把屏外坐标写盘 ⇒ 本进程内
// 之后每次 Alt+, 都是「把一个不可见窗口设为可见」,用户的体感是快捷键坏了;
// 只有重启 server 才被首建钳制救回。所以复用路径必须先自愈。
//
// 只在**零交集**时动手:部分可见(用户故意拖到半屏)不动,尊重用户位置。
// 调用面是 WeaselServer 的窗口消息/拖动回调,不在 TSF 回调里 ⇒ 不触及 P2 红线。
bool RepairOffscreenPanel(HWND hwnd) {
  RECT rc = {};
  if (!GetWindowRect(hwnd, &rc))
    return false;
  const std::vector<fluxing::panel_position::Rect> areas =
      fluxing::panel_position::EnumerateWorkAreas();
  if (areas.empty())
    return false;  // 枚举失败 ⇒ 不猜(钳制算法同样依赖 areas)
  if (fluxing::panel_position::RectIntersectsVisibleArea(
          rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, areas))
    return false;
  RECT workArea = {};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0)) {
    workArea.left = areas[0].left;
    workArea.top = areas[0].top;
    workArea.right = areas[0].right;
    workArea.bottom = areas[0].bottom;
  }
  // 尺寸**沿用当前窗口**(DPI 缩放由 WM_DPICHANGED 负责),这里只搬位置。
  const int panelW = rc.right - rc.left;
  const int panelH = rc.bottom - rc.top;
  const POINT anchor = {workArea.right - 1, workArea.bottom - 1};
  const PanelDpi dpi = QueryPanelDpi(anchor);
  const int margin = ScaleLogicalByDpi(12, dpi.x);
  int x = 0;
  int y = 0;
  PlacePanelAtBottomRight(workArea, panelW, panelH, margin, &x, &y);
  SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
  return true;
}

}  // namespace

// ===== Public API =====
void QuickPanelDialog::Show(bool currentFullwidth,
                            OnClick onSchema,
                            OnClick onUserFolder,
                            OnClick onPhrases,
                            OnToggle onFullwidth,
                            OnClick onSymbols,
                            OnClick onLogin) {
  // v0.19.0.32-fix: Show() 必须保存 callback, 否则 onPhrases 永远 null,
  // click button 1 死代码 (同 L96 路径)。原 (void)X 注释掉了。
  s_onSchema = onSchema;
  s_onUserFolder = onUserFolder;
  s_onPhrases = onPhrases;
  s_onFullwidth = onFullwidth;
  s_onSymbols = onSymbols;
  s_onLogin = onLogin;
  // spec 099 P0-4: 全/半角当前态必须**真正**写进静态字段(此前是
  // (void)currentFullwidth —— 状态区永远显示「半」)。同时同步状态快照。
  s_fullwidth = currentFullwidth;
  s_inputState.fullwidth = currentFullwidth;
  s_inputState.always_show = AlwaysShowMode();
  // Fluxing v4 需求④⑤: 面板一出现就要显示**当前**中英/全半角态 ⇒ 立即拉
  // 一次快照(provider 只读内存, 零 I/O), 否则首帧最多 100ms 显示的是旧态。
  RefreshInputState();
  // spec 099 P0-5: 面板重新显示 ⇒ 键盘状态复位(焦点槽位清空、退出键盘交互态)。
  ResetKeyboardState();
  // spec 084 / ADR 0013 D3: 长显态以 85% 系数起步,再由 100ms 轮询器按
  // 鼠标位置渐变到满值;瞬时态保持满值 => 渲染与今天逐像素一致。
  s_alphaFactor =
      AlwaysShowMode() ? fluxing::kAlphaFactorIdle : fluxing::kAlphaFactorFull;
  s_targetAlphaFactor = s_alphaFactor;
  if (s_hwnd) {
    // D0176 (2026-11-06): 复用路径原本零重定位 —— 面板矩形若已在屏外
    // (拖动落点被写盘 / 显示器布局变化),ShowWindow 只是把一个用户看不见的
    // 窗口设为可见,体感就是「Alt+, 没反应」。先自愈再显示,顺序不可交换。
    RepairOffscreenPanel(s_hwnd);
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    // L81-fix: reuse 路径也强制置顶(防止其他窗口在 hide→show 间隙盖上面板)
    SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // v0.19.0.24-fix(issue 4):reuse 路径也要重置 grace period 和 outsideMs。
    // 之前 Hide() reset 了 outsideMs,但 Show() 复用路径没 reset,导致第二次 Show
    // 立刻累加 outsideMs 到 1500 再 Hide。同时 s_showTime 必须更新,grace period
    // 重新计时。
    s_showTime = GetTickCount();
    s_outsideMs = 0;
    // v0.21.0.5 (spec 080): the hover polling timer has to be restarted on
    // the reuse path as well. Hide() kills it (KillTimer id=2) and the reuse
    // path returned before SetTimer, so after the very first show/hide cycle
    // the timer was gone for good: every later Show() put the panel on screen
    // with no auto-hide at all, and it stayed there until something else hid
    // it. The first-show path below already starts the timer, so the reuse
    // path has to do the same.
    SetTimer(s_hwnd, 2, 100, NULL);
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
  // 缺陷修复: kPanelW/kPanelH 是 **logical** 设计值,PerMonitor aware 进程必须
  // 自己按 monitor DPI 放大窗口。锚点取面板将要落到的右下角,据此选 monitor。
  const POINT anchor = {workArea.right - 1, workArea.bottom - 1};
  const PanelDpi dpi = QueryPanelDpi(anchor);
  const int panelW = ScaleLogicalByDpi(kPanelW, dpi.x);
  const int panelH = ScaleLogicalByDpi(kPanelH, dpi.y);
  const int margin = ScaleLogicalByDpi(12, dpi.x);
  // L92-fix: clamp 到 workArea,避免低分辨率下 panel 出屏(Phase 1 root cause)
  int x = 0;
  int y = 0;
  PlacePanelAtBottomRight(workArea, panelW, panelH, margin, &x, &y);
  // spec 099 P0-7: 有持久化位置 ⇒ 用它(并钳制回当前可见区:分辨率/DPI 变化后
  // 旧位置可能落到屏外)。没有持久化位置 ⇒ 保持右下角默认锚点(行为不变)。
  // D0176 (2026-11-06): 钳制改为**无条件**执行 —— 原写法把 ClampToVisibleArea
  // 关在 HasSavedPosition() 门内,一旦 EnumerateWorkAreas() 返回空,持久化的
  // 屏外坐标就直接进了 CreateWindowExW,没有任何兜底。
  {
    const std::vector<fluxing::panel_position::Rect> areas =
        fluxing::panel_position::EnumerateWorkAreas();
    if (!areas.empty()) {
      const POINT target = HasSavedPosition() ? SavedPosition() : POINT{x, y};
      const fluxing::panel_position::Point p =
          fluxing::panel_position::ClampToVisibleArea(target.x, target.y, panelW,
                                                      panelH, areas);
      x = p.x;
      y = p.y;
    }
  }

  s_hwnd = CreateWindowExW(
      WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST,
      kWindowClassName, L"Fluxing QuickPanel", WS_POPUP, x, y, panelW, panelH,
      NULL, NULL, GetModuleHandle(NULL), NULL);
  if (!s_hwnd)
    return;
  // D0176: 建窗后再复核一次 —— 钳制被绕过时兜底,零交集就挪回默认锚点。
  RepairOffscreenPanel(s_hwnd);
  // v0.19.0.10: 不再 SetLayeredWindowAttributes(LWA_ALPHA) — 那种 uniform
  // 半透明 不是 liquid glass (面板整体 86% 不透明)。 per-pixel alpha 走
  // RepaintLayered → ApplyAlphaGradient → UpdateLayeredWindow(ULW_ALPHA)。
  ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
  // L81-fix: WS_EX_TOPMOST + WS_EX_LAYERED 路径 z-order
  // 不稳,经常被其他窗口盖住。 显式 SetWindowPos(HWND_TOPMOST) 强制置顶,与
  // WS_EX_TOPMOST 等价但更可靠。 SWP_NOMOVE | SWP_NOSIZE |
  // SWP_NOACTIVATE:不改变 geometry 也不抢焦。
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  // L84-fix: 启动 hover polling timer (id=2, 100ms)。绕过 WS_EX_LAYERED 下
  // WM_MOUSEMOVE 投递不可靠问题(L83 sandbox 验证)。Hide() 时 KillTimer。
  SetTimer(s_hwnd, 2, 100, NULL);
  // v0.19.0.24-fix(issue 4):首次 Show 也设 s_showTime,reset s_outsideMs。
  // L89-fix auto-hide 用 s_outsideMs 累加,首次 Show 时 cursor 还在 panel
  // 外,直接 累加到 1500ms → Hide。Fix:Show 启动 grace period
  // (kShowGraceMs=2000ms),polling timer 在 (now - s_showTime) < grace 时不累加
  // outsideMs。s_outsideMs reset 保证 不带旧值。
  s_showTime = GetTickCount();
  s_outsideMs = 0;
  RepaintLayered(s_hwnd);  // 第一次提交 layered surface (带渐变 alpha)
}

void QuickPanelDialog::Hide() {
  // spec 099 P0-5: 收起时先收起 tooltip、归还键盘焦点(否则用户的编辑框
  // 永远拿不回焦点 ⇒ 打不了字 —— L89-fix 记的事故形态)。
  HideTooltip();
  RestoreKeyboardFocus();
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, 1);
    KillTimer(s_hwnd, 2);  // L84-fix: kill hover polling timer
    ShowWindow(s_hwnd, SW_HIDE);
  }
  // spec 084 / ADR 0013 D5: 收起后系数复位为满值,下次显示不带着旧系数。
  s_alphaFactor = fluxing::kAlphaFactorFull;
  s_targetAlphaFactor = fluxing::kAlphaFactorFull;
  s_hoveredIdx = -1;
  s_activeIdx = -1;
  // spec 099 P0-5: 键盘状态复位 —— 下次显示时不带旧焦点槽 / 旧交互态。
  ResetKeyboardState();
}

void QuickPanelDialog::ToggleMode() {
  if (s_hwnd && IsWindow(s_hwnd) && IsWindowVisible(s_hwnd)) {
    Hide();
  } else {
    // ADR 0013 D8: 把当前静态回调原样回传。Show() 会无条件把这 6 个参数
    // 写进静态字段 => 传 nullptr 会把按钮回调清空(按钮变哑)。
    Show(s_fullwidth, s_onSchema, s_onUserFolder, s_onPhrases, s_onFullwidth,
         s_onSymbols, s_onLogin);
  }
}

// ===== v1.1.2 (ADR 0019):「常驻」勾选态跨 WeaselServer 重启持久化 =====
// 根因: 运行期状态原是纯内存态 ⇒ 装机换 DLL 所需的重启把勾选清零,面板退回
// 瞬时模式(grace 2s + 鼠标在面板外累计 1.5s ⇒ 用户看到的「调出几秒后消失」)。
// v1.1.3 (ADR 0020): 介质改跟随用户数据目录 —— 原来硬编码 %APPDATA%\Rime\,
// 与 WeaselUserDataPath() 分叉(用户数据目录被指到 D 盘时,状态会单独住一份,
// 而且会在本 fork 已迁走的旧路径上重新造目录)。
// 现在落点 = <用户数据目录>\quickpanel.state,单行 ASCII "always_show=0|1\n";
// 旧落点 %APPDATA%\Rime\quickpanel.state 仅作只读兜底(存量勾选不丢)。
// spec 095: 运行期状态 + 落盘 + 读盘策略统一下沉到 include/FluxingAlwaysShow.h
// (QuickPanelDialog 与 ShortcutSettings 共用单一事实源,constitution R7)。
// 本文件只剩「旧落点」留在 QuickPanelDialog —— 它要 SHGetFolderPathW(shell32),
// 共享头不引入 shell32。
#pragma comment(lib, "shell32.lib")

// 旧落点(1.1.2 及更早):%APPDATA%\Rime\quickpanel.state —— 只读兜底,不再写。
std::wstring QuickPanelDialog::LegacyStatePath() {
  wchar_t appdata[MAX_PATH] = {};
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)))
    return std::wstring(appdata) + L"\\Rime\\quickpanel.state";
  return L"";
}

// 默认落点:用户数据目录下的 quickpanel.state(与 WeaselUserDataPath() 同源)。
std::wstring QuickPanelDialog::DefaultStatePath() {
  return fluxing::always_show::DefaultStatePath();
}

// spec 099 P0-7: 测试注入位置落点(空串 = 回默认落点)。
void QuickPanelDialog::SetPositionPath(const std::wstring& path) {
  fluxing::panel_position::SetPositionPathOverride(path);
}

// spec 099 P0-7: 拖动结束写盘 —— 取当前窗口的屏幕坐标左上角。
// 写盘发生在 WeaselServer 进程的拖动回调里(不是 TSF 回调)⇒ 不违反 P2 红线。
bool QuickPanelDialog::SavePosition() {
  if (!s_hwnd || !IsWindow(s_hwnd))
    return false;
  RECT r = {};
  if (!GetWindowRect(s_hwnd, &r))
    return false;
  // D0176 (2026-11-06):落盘前先问一句「这个矩形用户看得见吗」。
  // 事故链:拖动落点被**无条件**写盘 ⇒ 下一个进程读到一个屏外坐标 ⇒ 面板永远
  // 停在桌面外,用户只看到「Alt+, 没反应」;重启后靠首建钳制才自愈,所以这个
  // 缺陷看起来是「时好时坏」。零交集 ⇒ 拒写(污染值不进文件),并当场救回面板。
  // 部分可见 ⇒ 照写,尊重用户故意把面板拖到半屏的自由。
  // 不放进 SavePositionAt:那是裸写盘口,单测(T24o)依赖它与真实多屏几何解耦。
  const std::vector<fluxing::panel_position::Rect> areas =
      fluxing::panel_position::EnumerateWorkAreas();
  if (!areas.empty() &&
      !fluxing::panel_position::RectIntersectsVisibleArea(
          r.left, r.top, r.right - r.left, r.bottom - r.top, areas)) {
    RepairOffscreenPanel(s_hwnd);
    return false;
  }
  // 枚举失败(areas 为空)⇒ 不拦:宁可照旧写盘,也不凭空否决用户的一次拖动。
  return SavePositionAt(r.left, r.top);
}

// 读一个状态文件的第一行。文件缺失/读不出 ⇒ false(由调用方决定是否兜底)。
// ADR 0020: primary 优先;primary 读不到才读 legacy(1.1.2 旧落点只读兜底)。
bool QuickPanelDialog::LoadPersistedAlwaysShowModeFrom(
    const std::wstring& primary,
    const std::wstring& legacy) {
  // ADR 0020: primary 优先;primary 读不到才读 legacy(1.1.2 旧落点只读兜底);
  // 都读不到 ⇒ 默认常驻(spec 089 / ADR 0022)。
  return fluxing::always_show::ResolveAlwaysShowFromFiles(primary, legacy,
                                                         true);
}

bool QuickPanelDialog::LoadPersistedAlwaysShowMode() {
  // 测试注入路径时不做旧落点兜底,免得读到真实用户目录里的文件(测试必须确定)。
  const std::wstring legacy = fluxing::always_show::HasStatePathOverride()
                                  ? std::wstring()
                                  : LegacyStatePath();
  return fluxing::always_show::ResolveAlwaysShowFromFiles(
      fluxing::always_show::ResolveStatePath(), legacy, true);
}

void QuickPanelDialog::SetStatePath(const std::wstring& path) {
  fluxing::always_show::StatePathOverrideRef() = path;
}

void QuickPanelDialog::RestoreAlwaysShowMode() {
  // 只做模式回填: 不 Show、不回写盘 —— spec 055「启动不自动显示面板」不变。
  fluxing::always_show::SetAlwaysShowFlag(LoadPersistedAlwaysShowMode());
}

// spec 084 / ADR 0013 D4/D5: 长显模式开关。
// - 打开:进入长显态,屏蔽三条自动隐藏路径,系数由轮询器接管。
// - 关闭:回到瞬时态,系数复位为满值。
// v1.1.2 (ADR 0019): 勾选态变化时同步落盘,重启后由 Restore 恢复。
void QuickPanelDialog::SetAlwaysShowMode(bool enabled) {
  if (AlwaysShowMode() == enabled)
    return;
  // spec 095: 状态 + 落盘 + 通知观察者(alpha 复位 / 重绘)统一走共享头。
  fluxing::always_show::ApplyAlwaysShow(enabled);
}

// spec 095: 模式变化观察者 —— 由 include/FluxingAlwaysShow.h 的
// ModeChangedCallback 机制触发。
// 关 ⇒ alpha 复位为满值(否则面板会停在淡显状态);开关都 ⇒ 面板可见时重绘。
void QuickPanelDialog::OnAlwaysShowModeChanged(bool enabled) {
  if (!enabled) {
    s_alphaFactor = fluxing::kAlphaFactorFull;
    s_targetAlphaFactor = fluxing::kAlphaFactorFull;
  }
  if (s_hwnd && IsWindowVisible(s_hwnd)) {
    RepaintLayered(s_hwnd);
  }
}

namespace {
// 注册观察者。文件作用域静态对象 ⇒ 进程启动即注册,早于任何一次
// SetAlwaysShowMode()(托盘菜单 / 设置面板)。
struct AlwaysShowCallbackRegistrar {
  AlwaysShowCallbackRegistrar() {
    fluxing::always_show::SetModeChangedCallback(
        &QuickPanelDialog::OnAlwaysShowModeChanged);
  }
};
const AlwaysShowCallbackRegistrar g_always_show_callback_registrar;
}  // namespace

void QuickPanelDialog::EnableAlwaysShowMode(OnClick onSchema,
                                            OnClick onUserFolder,
                                            OnClick onPhrases,
                                            OnToggle onFullwidth,
                                            OnClick onSymbols,
                                            OnClick onLogin) {
  // spec 084 / ADR 0013: 打开长显模式并注入回调,再显示面板。
  SetAlwaysShowMode(true);
  s_onSchema = onSchema;
  s_onUserFolder = onUserFolder;
  s_onPhrases = onPhrases;
  s_onFullwidth = onFullwidth;
  s_onSymbols = onSymbols;
  s_onLogin = onLogin;
  Show(false, onSchema, onUserFolder, onPhrases, onFullwidth, onSymbols,
       onLogin);
}
