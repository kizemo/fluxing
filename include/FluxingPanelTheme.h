#pragma once
//
// FluxingPanelTheme.h — 方向 A(微软拼音 IME 工具栏 / Fluent Design 2 标杆迁移)
// 的**调色板 / 几何 / 分层合成** —— 单一真相源(constitution R7:生产代码与单测同源)。
//
// 设计稿: docs/design/quickpanel-v6/design-demos/a-benchmark.html
// 批准记录: docs/design/quickpanel-v6/direction-approved.md
//
// ── 为什么需要这个头 ────────────────────────────────────────────────
// 方向 A 的观感 =「半透明纯色面板 + 1px 发丝描边 + 分层软投影 + 分组轨道」。
// 这些**层次**各自带不同 alpha(面板体 0.72 / 描边 0.28 / 轨道底 0.07 /
// 高光 0.55 / 阴影 0.05+0.04+0.03),而 GDI 的画笔/画刷**没有 alpha** ⇒
// 必须自己按层做 src-over 预乘合成,把结果写进 32-bit BGRA DIB 的 alpha 通道
// (UpdateLayeredWindow 按 per-pixel alpha 合成到桌面)。
//
// 圆角语义:**SVG rx 语义**(角圆心在 (l+r, t+r),半径 r)。
// ⚠ GDI 的 CreateRoundRectRgn(l,t,w,h,ellipseW,ellipseH) 的角半径是
//   ellipseW/2 ⇒ 与 SVG **不等价**(传 14 只有 7 的圆角)。旧代码用 GDI 画圆角
//   ⇒ 实际比设计稿「方」;方向 A 改用本文件的解析判定,圆角才真正是 rx=14。
//
// 历史雷区(勿重犯):
//   ❌ 不用 GDI AlphaBlend 合成半透明层(半透明像素被当反向权重 ⇒ 品牌 logo 黑框)
//   ❌ 不用 GDI+ / D2D / blur / backdrop-filter(交付形态就是 Win32 GDI 自绘)
//   ❌ 不用 LWA_ALPHA 统一透明度(不是 liquid glass)
//   ❌ 不用 GDI+ Bitmap(IStream*) / CreateStreamOnHGlobal+Release
//
#include <cstddef>
#include <cstdint>

// DPI 缩放的单一真相源(与生产布局共用同一套就近舍入口径)。
#include "FluxingPanelDpi.h"

namespace fluxing {
namespace panel_theme {

// ===== 颜色(逻辑色,不随 DPI 变) =====
struct Rgb {
  unsigned char r, g, b;
};

inline Rgb MakeRgb(int r, int g, int b) {
  Rgb c;
  c.r = (unsigned char)r;
  c.g = (unsigned char)g;
  c.b = (unsigned char)b;
  return c;
}

// 面板体(暖白半透明)
inline constexpr Rgb kBodyColor = {0xFA, 0xF7, 0xF2};
// 面板外描边(发丝)
inline constexpr Rgb kBorderColor = {0x4A, 0x45, 0x3D};
// 顶部高光发丝
inline constexpr Rgb kHighlightColor = {0xFF, 0xFF, 0xFF};
// 品牌区 → 动作键 分隔发丝
inline constexpr Rgb kSeparatorColor = {0x4A, 0x45, 0x3D};
// 状态轨道底
inline constexpr Rgb kTrackColor = {0x4A, 0x44, 0x3A};
// 状态轨道内描边
inline constexpr Rgb kTrackStrokeColor = {0xFF, 0xFF, 0xFF};
// 动作图标墨色
inline constexpr Rgb kIconInkColor = {0x6F, 0x6A, 0x62};
// 状态 · 中文侧(品牌橙)
inline constexpr Rgb kAccentColor = {0xFF, 0x5F, 0x31};
// 状态 · 英文侧
inline constexpr Rgb kDarkColor = {0x24, 0x1F, 0x1A};
// 分层软投影
inline constexpr Rgb kShadowColor = {0x1B, 0x18, 0x14};
// hover 键帽描边
inline constexpr Rgb kHoverColor = {0xFF, 0x5F, 0x31};

// ===== alpha(0..255;设计稿小数 × 255 后取整) =====
inline constexpr int kAlphaBody = 184;        // 0.72
inline constexpr int kAlphaBorder = 71;       // 0.28
inline constexpr int kAlphaHighlight = 140;   // 0.55
inline constexpr int kAlphaSeparator = 46;    // 0.18
inline constexpr int kAlphaTrack = 18;        // 0.07
inline constexpr int kAlphaTrackStroke = 56;  // 0.22
inline constexpr int kAlphaHover = 128;       // 0.50
inline constexpr int kAlphaShadow[3] = {13, 10, 8};  // 0.05 / 0.04 / 0.03

// ===== 几何(logical px;生产代码按缩放系数换算成物理像素) =====
// 状态轨道:胶囊,包住视觉位 1(中/英)与视觉位 2(全/半角)
inline constexpr int kTrackX = 61;
inline constexpr int kTrackY = 1;
inline constexpr int kTrackW = 62;
inline constexpr int kTrackH = 30;
inline constexpr int kTrackR = 11;
// 品牌区分隔发丝(竖线,设计稿 x=31.5)
inline constexpr double kSeparatorX = 31.5;
inline constexpr int kSeparatorY1 = 9;
inline constexpr int kSeparatorY2 = 23;
// 顶部高光发丝(设计稿 y=0.5,x 14..141)
inline constexpr double kHighlightY = 0.5;
inline constexpr int kHighlightX1 = 14;
inline constexpr int kHighlightX2 = 141;
// 底部投影占位高度(画布比面板体多出来的部分)
inline constexpr int kShadowH = 6;

// ===== 预乘像素 =====
struct Pixel {
  unsigned char r, g, b, a;  // r/g/b 已乘 alpha(a=0 时无意义)
};

struct Layer {
  Rgb color;
  int alpha;  // 0..255
};

inline Layer MakeLayer(Rgb c, int alpha) {
  Layer l;
  l.color = c;
  l.alpha = alpha;
  return l;
}

// src-over:src 叠在 dst 之上(两者都是预乘 alpha)。
inline Pixel Over(const Pixel& dst, const Layer& src) {
  if (src.alpha <= 0)
    return dst;
  const int sa = src.alpha;
  const int inv = 255 - sa;
  Pixel o;
  o.r = (unsigned char)((sa * (int)src.color.r + inv * (int)dst.r + 127) / 255);
  o.g = (unsigned char)((sa * (int)src.color.g + inv * (int)dst.g + 127) / 255);
  o.b = (unsigned char)((sa * (int)src.color.b + inv * (int)dst.b + 127) / 255);
  o.a = (unsigned char)(sa + (inv * (int)dst.a + 127) / 255);
  return o;
}

// 预乘 → 直通 alpha(UpdateLayeredWindow 的 AC_SRC_ALPHA 期望非预乘 RGB)。
inline void Unpremultiply(const Pixel& p,
                          unsigned char* rgb,
                          unsigned char* alpha) {
  *alpha = p.a;
  if (p.a == 0) {
    rgb[0] = rgb[1] = rgb[2] = 0;
    return;
  }
  rgb[0] = (unsigned char)(((int)p.r * 255 + (int)p.a / 2) / (int)p.a);
  rgb[1] = (unsigned char)(((int)p.g * 255 + (int)p.a / 2) / (int)p.a);
  rgb[2] = (unsigned char)(((int)p.b * 255 + (int)p.a / 2) / (int)p.a);
}

// SVG rx 语义的圆角矩形命中测试(角圆心 = (l+r, t+r),半径 r)。
inline bool InsideRoundRect(int x, int y, int l, int t, int w, int h, int r) {
  if (w <= 0 || h <= 0)
    return false;
  if (x < l || x >= l + w || y < t || y >= t + h)
    return false;
  if (r <= 0)
    return true;
  const int rx1 = l + r, rx2 = l + w - 1 - r;
  const int ry1 = t + r, ry2 = t + h - 1 - r;
  int cx = (x < rx1) ? rx1 : ((x > rx2) ? rx2 : x);
  int cy = (y < ry1) ? ry1 : ((y > ry2) ? ry2 : y);
  const int dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= r * r;
}

// 1px 发丝描边命中(外轮廓之内、内轮廓之外)。
inline bool OnRoundRectStroke(int x, int y, int l, int t, int w, int h, int r,
                              int sw) {
  if (sw <= 0)
    return false;
  if (!InsideRoundRect(x, y, l, t, w, h, r))
    return false;
  if (w <= 2 * sw || h <= 2 * sw)
    return true;  // 太窄 ⇒ 整个轮廓都是边
  return !InsideRoundRect(x, y, l + sw, t + sw, w - 2 * sw, h - 2 * sw,
                          (r > sw) ? (r - sw) : 0);
}

// ===== chrome 规格(全部物理像素) =====
struct ChromeSpec {
  int canvas_w = 0;  // 画布宽(含面板体)
  int canvas_h = 0;  // 画布高(面板体 + 底部投影区)
  int body_w = 0;
  int body_h = 0;
  int body_r = 0;  // 圆角(SVG rx)
  int highlight_y = 0;
  int highlight_x1 = 0, highlight_x2 = 0;
  int separator_x = 0, separator_y1 = 0, separator_y2 = 0;
  int track_x = 0, track_y = 0, track_w = 0, track_h = 0, track_r = 0;
  bool has_hover = false;
  int hover_x = 0, hover_y = 0, hover_w = 0, hover_h = 0, hover_r = 0;
  int shadow_offsets[3] = {0, 0, 0};  // 三层投影的物理 y 偏移
};

// ===== chrome 规格构造(生产与单测**同源**) =====
//
// 为什么放这里:方向 A 的几何(轨道 / 分隔线 / 高光 / 投影偏移)在生产代码里
// 要按 DPI 换算成物理像素。若把换算写在 WeaselServer/QuickPanelDialog.cpp 里,
// 单测就只能断言「写死的那几个数」⇒ 生产改了测试不会变红(spec 092 记的
// 「假绿」死法)。这里把「逻辑值 + dpr → 物理值」整段收成纯函数,生产与单测
// 都调它 ⇒ 改这里两边同时变。
//
// ★ 三层投影的 y 偏移按 **dpr_x** 换算:底部投影带的物理高度
//   (s_panelShadow_phys)也是按 dpr_x 算的(见 ApplyPhysicalLayout)⇒
//   偏移与带宽同源,偏移永远 ≤ 带宽(否则投影会被画到画布外)。

// 三层投影的物理 y 偏移(设计稿 1.5 / 3.5 / 6;6 = 投影带全高)。
inline constexpr double kShadowOffset1 = 1.5;
inline constexpr double kShadowOffset2 = 3.5;

struct ChromeInputs {
  int canvas_w = 0;  // 画布宽
  int canvas_h = 0;  // 画布高(面板体 + 底部投影带)
  int body_w = 0;    // 面板体宽
  int body_h = 0;    // 面板体高
  int body_r = 0;    // 面板体圆角(SVG rx)
  double dpr_x = 1.0;
  double dpr_y = 1.0;
  bool has_hover = false;
  int hover_x = 0, hover_y = 0, hover_w = 0, hover_h = 0, hover_r = 0;
};

inline ChromeSpec BuildChromeSpec(const ChromeInputs& in) {
  ChromeSpec s;
  s.canvas_w = in.canvas_w;
  s.canvas_h = in.canvas_h;
  s.body_w = in.body_w;
  s.body_h = in.body_h;
  s.body_r = in.body_r;
  // 顶部高光发丝(y 用 dpr_y,x 用 dpr_x)
  s.highlight_y = static_cast<int>(kHighlightY * in.dpr_y + 0.5);
  s.highlight_x1 = fluxing::panel_dpi::ScaleByFactor(kHighlightX1, in.dpr_x);
  s.highlight_x2 = fluxing::panel_dpi::ScaleByFactor(kHighlightX2, in.dpr_x);
  // 品牌区 → 动作键 分隔发丝(x 是小数 31.5 ⇒ 不能走整数缩放函数)
  s.separator_x = static_cast<int>(kSeparatorX * in.dpr_x + 0.5);
  s.separator_y1 = fluxing::panel_dpi::ScaleByFactor(kSeparatorY1, in.dpr_y);
  s.separator_y2 = fluxing::panel_dpi::ScaleByFactor(kSeparatorY2, in.dpr_y);
  // 状态轨道(胶囊,包住视觉位 1「中/英」与视觉位 2「全/半角」)
  s.track_x = fluxing::panel_dpi::ScaleByFactor(kTrackX, in.dpr_x);
  s.track_y = fluxing::panel_dpi::ScaleByFactor(kTrackY, in.dpr_y);
  s.track_w = fluxing::panel_dpi::ScaleByFactor(kTrackW, in.dpr_x);
  s.track_h = fluxing::panel_dpi::ScaleByFactor(kTrackH, in.dpr_y);
  s.track_r = fluxing::panel_dpi::ScaleByFactor(kTrackR, in.dpr_x);
  // hover 键帽轮廓(物理矩形由调用方给 —— 命中区口径在布局层)
  s.has_hover = in.has_hover;
  s.hover_x = in.hover_x;
  s.hover_y = in.hover_y;
  s.hover_w = in.hover_w;
  s.hover_h = in.hover_h;
  s.hover_r = in.hover_r;
  // 三层投影的物理 y 偏移(与投影带同源 ⇒ 偏移 ≤ 带宽)
  s.shadow_offsets[0] = static_cast<int>(kShadowOffset1 * in.dpr_x + 0.5);
  s.shadow_offsets[1] = static_cast<int>(kShadowOffset2 * in.dpr_x + 0.5);
  s.shadow_offsets[2] = fluxing::panel_dpi::ScaleByFactor(kShadowH, in.dpr_x);
  return s;
}

// 把方向 A 的 chrome(投影 → 面板体 → 高光 → 描边 → 分隔线 → 轨道 → hover)
// 逐层合成进 out(预乘 alpha;out 必须是 canvas_w × canvas_h 个 Pixel)。
inline void RasterizeChrome(const ChromeSpec& s, Pixel* out) {
  if (!out || s.canvas_w <= 0 || s.canvas_h <= 0)
    return;
  const size_t n = (size_t)s.canvas_w * (size_t)s.canvas_h;
  for (size_t i = 0; i < n; ++i)
    out[i] = Pixel{0, 0, 0, 0};

  // ① 分层软投影(设计稿顺序:偏移小的先画、偏移大的后画 ⇒ 深度最大的压在上面)
  //
  // ⚠ 投影形状 = **下移后的面板体** ⇒ 它与面板体本身重叠。面板体是半透明
  //   (α .72)⇒ 若不裁剪,影子会**透过面板体**把整块面板压暗(实测:面板体
  //   alpha 从 184 抬到 ~200+,整体发灰,不再是设计稿的暖白)。
  //   ⇒ 投影只保留**面板体轮廓之外**的部分(即底部那圈软影)。
  for (int k = 0; k < 3; ++k) {
    const Layer L = MakeLayer(kShadowColor, kAlphaShadow[k]);
    const int off = s.shadow_offsets[k];
    const int rr = s.body_r + k;
    for (int y = 0; y < s.canvas_h; ++y) {
      const int ly = y - off;
      for (int x = 0; x < s.canvas_w; ++x) {
        if (!InsideRoundRect(x, ly, 0, 0, s.body_w, s.body_h, rr))
          continue;
        if (InsideRoundRect(x, y, 0, 0, s.body_w, s.body_h, s.body_r))
          continue;  // 面板体之内 ⇒ 不落影
        Pixel& px = out[(size_t)y * s.canvas_w + x];
        px = Over(px, L);
      }
    }
  }

  // ② 面板体(半透明暖白)
  const Layer body = MakeLayer(kBodyColor, kAlphaBody);
  for (int y = 0; y < s.canvas_h; ++y) {
    for (int x = 0; x < s.canvas_w; ++x) {
      if (!InsideRoundRect(x, y, 0, 0, s.body_w, s.body_h, s.body_r))
        continue;
      Pixel& px = out[(size_t)y * s.canvas_w + x];
      px = Over(px, body);
    }
  }

  // ③ 顶部高光发丝(设计稿 y=0.5,x 14..141)
  const Layer hl = MakeLayer(kHighlightColor, kAlphaHighlight);
  if (s.highlight_y >= 0 && s.highlight_y < s.canvas_h) {
    for (int x = s.highlight_x1; x <= s.highlight_x2 && x < s.canvas_w; ++x) {
      if (x < 0)
        continue;
      Pixel& px = out[(size_t)s.highlight_y * s.canvas_w + x];
      px = Over(px, hl);
    }
  }

  // ④ 面板外描边(1px 发丝)
  const Layer border = MakeLayer(kBorderColor, kAlphaBorder);
  for (int y = 0; y < s.canvas_h; ++y) {
    for (int x = 0; x < s.canvas_w; ++x) {
      if (!OnRoundRectStroke(x, y, 0, 0, s.body_w, s.body_h, s.body_r, 1))
        continue;
      Pixel& px = out[(size_t)y * s.canvas_w + x];
      px = Over(px, border);
    }
  }

  // ⑤ 品牌区 → 动作键 分隔发丝
  const Layer sep = MakeLayer(kSeparatorColor, kAlphaSeparator);
  if (s.separator_x >= 0 && s.separator_x < s.canvas_w) {
    for (int y = s.separator_y1; y <= s.separator_y2 && y < s.canvas_h; ++y) {
      if (y < 0)
        continue;
      Pixel& px = out[(size_t)y * s.canvas_w + s.separator_x];
      px = Over(px, sep);
    }
  }

  // ⑥ 状态轨道底
  const Layer track = MakeLayer(kTrackColor, kAlphaTrack);
  for (int y = 0; y < s.canvas_h; ++y) {
    for (int x = 0; x < s.canvas_w; ++x) {
      if (!InsideRoundRect(x, y, s.track_x, s.track_y, s.track_w, s.track_h,
                           s.track_r))
        continue;
      Pixel& px = out[(size_t)y * s.canvas_w + x];
      px = Over(px, track);
    }
  }

  // ⑦ 状态轨道内描边(1px)
  const Layer trackStroke = MakeLayer(kTrackStrokeColor, kAlphaTrackStroke);
  for (int y = 0; y < s.canvas_h; ++y) {
    for (int x = 0; x < s.canvas_w; ++x) {
      if (!OnRoundRectStroke(x, y, s.track_x, s.track_y, s.track_w, s.track_h,
                             s.track_r, 1))
        continue;
      Pixel& px = out[(size_t)y * s.canvas_w + x];
      px = Over(px, trackStroke);
    }
  }

  // ⑧ hover 键帽轮廓浮出(只描边,不改键帽底色 —— 守 L86 用户指令)
  const Layer hover = MakeLayer(kHoverColor, kAlphaHover);
  if (s.has_hover) {
    for (int y = 0; y < s.canvas_h; ++y) {
      for (int x = 0; x < s.canvas_w; ++x) {
        if (!OnRoundRectStroke(x, y, s.hover_x, s.hover_y, s.hover_w,
                               s.hover_h, s.hover_r, 1))
          continue;
        Pixel& px = out[(size_t)y * s.canvas_w + x];
        px = Over(px, hover);
      }
    }
  }
}

}  // namespace panel_theme
}  // namespace fluxing
