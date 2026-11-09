#pragma once
//
// FluxingPanelIconGeom — 快捷面板**键帽符号**的几何与笔宽缩放口径(单一真相源)
//
// 为什么下沉成 header-only(constitution R7):
//   生产代码(WeaselServer/QuickPanelDialog.cpp 的 DrawIcon* / DrawIconFullwidth)
//   与单测(test/TestQuickPanelDialog)必须读**同一份**缩放公式。把「150% ⇒
//   符号宽 30 px、笔宽 3 px」抄进测试就是第二份真相源 —— constitution R7 明令
//   禁止(spec 092 记的「假绿」死法:测试复制公式,改生产代码时测试不会变红)。
//
// ── 病灶(用户真机反馈 2026-11-09,批次 T 修完 logo 之后仍复现)────────────
//   用户原话:「改分辨率后,logo 完整,但键帽符号还是没有同比缩放,线条也还是偏粗。」
//
//   几何侧:六个键帽的**几何**确实都乘了同一个系数(icon_scale = dpr_x)。
//   但「比例」被两处**取整**破坏,用户看到的正是这两处的后果:
//
//   ① 笔宽取整 —— fwicon 空心环的 raw 笔宽是 **1px**(v8.1 设计:细线要比实心点轻)。
//      按「就近舍入」缩放到 150%(系数 1.5023)时 round(1×1.5023) = **2px**
//      ⇒ 笔宽变成 **2.0×**,而几何只有 1.5× ⇒ 环相对粗 33% ⇒「线条偏粗」。
//      同理 raw3 的加粗线会得 5px(理想 4.5)⇒ 相对粗 11%。
//
//   ② 字形字号二次取整 —— 逗号是**真字形**,字号由
//      ScaleFontHeight(seed=48, 实测墨迹高, 目标墨迹高) 反推,而目标墨迹高取自
//      **已被取整**的缩放墨迹盒(13 → 19)⇒ 字号 48×19/13 = **70px**
//      (理想 48×1.5023 = 72px)⇒ 符号只有 **1.46×** 而不是 1.5×,且字形宽度
//      同样不同比 ⇒「符号没有同比缩放」。
//
// ── 修法(三条口径都写成纯函数,生产与单测同源)─────────────────────────
//   ① ScaleStroke:笔宽**向下取整**(floor),下限 1px。
//      理由:GDI 的笔宽只能是整数,1.5px 不可表达;「1px 的线」在 150% 下
//      取 1px(相对细)或 2px(相对粗 33%)二选一 —— 用户明确抱怨「偏粗」,
//      故取**细**的一侧。raw≥2 的线在 100/150/200% 下正好是 2/3/4px(严格同比)。
//      「宁可略细不可略粗」是这条口径的唯一理由,改它必须同时改单测。
//   ② ScaleCoord:坐标**就近舍入(.5 向上)**,与 include/FluxingPanelDpi.h 的
//      ScaleByFactor 逐位一致(既有布局口径不变,单测 T25 锁死)。
//   ③ 逗号字号:目标字号直接按系数给(48 × factor),字体自校正退化为兜底。
//
// ── 坐标系 ─────────────────────────────────────────────────────────────
//   raw = viewBox 30×30 内的设计稿坐标(不随 DPI 变)。
//   物理 = raw × factor(factor = 面板缩放系数 = dpr_x,下限 1.0)。
//
#include <windows.h>

namespace fluxing {
namespace panel_icon_geom {

// 面板缩放系数下限 1.0(与 include/FluxingPanelDpi.h 的 PanelScaleFactor 同口径:
// 低于 100% 不缩小 ⇒ 键帽不会缩到亚像素、符号比例不会失衡)。
inline double ClampFactor(double factor) {
  return factor < 1.0 ? 1.0 : factor;
}

// raw 坐标 → 物理像素(**就近舍入,.5 向上**)。
// 与 fluxing::panel_dpi::ScaleByFactor 逐位一致(口径单一,constitution R7)。
inline int ScaleCoord(int raw, double factor) {
  const double v = raw * ClampFactor(factor);
  return static_cast<int>(v >= 0 ? v + 0.5 : v - 0.5);
}

// raw 笔宽 → 物理像素(**向下取整**,下限 1)。
// 详见文件头 ①:「宁可略细不可略粗」。
inline int ScaleStroke(int rawWidth, double factor) {
  if (rawWidth <= 0)
    return 1;
  const int scaled = static_cast<int>(rawWidth * ClampFactor(factor));
  return scaled < 1 ? 1 : scaled;
}

// ===== 各线描图标的 raw 描线墨迹盒(viewBox 局部坐标,闭区间)=====
//
// 这些数字是**设计稿基准形状**,与 WeaselServer/QuickPanelDialog.h 里 L93-fix
// 注释记录的 bbox 一致。集中在这里是为了让单测能对三档 DPI 直接断言
// 「符号尺寸同比」,而不必去数像素。
//
// ★ 与生产绘制的一致性:DrawIcon* 的坐标就来自下面这些 bbox
//   (Schema X[5..25] Y[8..22] / Phrase X[5..25] Y[5..25] /
//    Symbols X[3..27] Y[6..24] / Settings X[4..26] Y[4..26] /
//    Account X[5..25] Y[5..28])。若改设计稿形状,这里与生产必须同步改。
struct RawBox {
  int left;
  int top;
  int right;
  int bottom;
};

inline RawBox SchemaInkBox()   { return RawBox{5, 8, 25, 22}; }
inline RawBox PhraseInkBox()   { return RawBox{5, 5, 25, 25}; }
inline RawBox SymbolsInkBox()  { return RawBox{3, 6, 27, 24}; }
inline RawBox SettingsInkBox() { return RawBox{4, 4, 26, 26}; }
inline RawBox AccountInkBox()  { return RawBox{5, 5, 25, 28}; }

// raw 墨迹盒 → 缩放后的物理墨迹盒(闭区间)。
inline RECT ScaledInkBox(const RawBox& b, double factor) {
  RECT r;
  r.left = ScaleCoord(b.left, factor);
  r.top = ScaleCoord(b.top, factor);
  r.right = ScaleCoord(b.right, factor);
  r.bottom = ScaleCoord(b.bottom, factor);
  return r;
}

// 缩放后墨迹盒的**尺寸**(宽/高;闭区间 ⇒ +1)。
inline void ScaledInkSize(const RawBox& b, double factor, int* w, int* h) {
  const RECT r = ScaledInkBox(b, factor);
  if (w)
    *w = r.right - r.left + 1;
  if (h)
    *h = r.bottom - r.top + 1;
}

}  // namespace panel_icon_geom
}  // namespace fluxing
