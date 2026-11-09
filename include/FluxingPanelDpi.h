#pragma once
//
// spec 099 P0-6: 快捷设置栏的 DPI 缩放纯逻辑(单一真相源)。
//
// 为什么下沉成 header-only:
//   1) 单一真相源(constitution R7)—— 生产代码(WeaselServer/QuickPanelDialog.cpp)
//      与单测(test/TestQuickPanelDialog)必须用**同一份**缩放公式。若把
//      「150% ⇒ 368×72」这条判据抄进测试,就是第二份真相源,改生产代码时
//      测试不会变红(这正是 spec 092 记的「假绿」死法)。
//   2) 可判别 —— 单测直接断言 ScaleLogicalByDpi(245, 144) == 368;
//      变异(把就近舍入改成整除)会立刻变红。
//
// 口径与旧的 inline int ScaleLogicalByDpi(int, UINT) 逐位一致:
//   MulDiv(logical, dpi, 96) 对正数等价于 (logical*dpi + 48) / 96(就近舍入,
//   .5 向上)。这里用纯整数实现,不依赖 windows.h(生产与测试都能直接包含)。
//
namespace fluxing {
namespace panel_dpi {

// logical 像素 → physical 像素。dpi <= 0 视为 96(=100%)。
// 结果不大于 0 时回退到 logical(与旧实现的兜底一致:异常 DPI 不塌成 0)。
inline int ScaleLogicalByDpi(int logical, int dpi) {
  if (dpi <= 0)
    dpi = 96;
  const long long scaled =
      (static_cast<long long>(logical) * static_cast<long long>(dpi) + 48) / 96;
  return scaled > 0 ? static_cast<int>(scaled) : logical;
}

// 面板尺寸一次性换算(宽高各自用自己的 dpi,横竖 DPI 不同时也正确)。
inline void PanelPhysicalSize(int logicalW,
                              int logicalH,
                              int dpiX,
                              int dpiY,
                              int* outW,
                              int* outH) {
  if (outW)
    *outW = ScaleLogicalByDpi(logicalW, dpiX);
  if (outH)
    *outH = ScaleLogicalByDpi(logicalH, dpiY);
}

// ===== 面板缩放系数(单一真相源,2026-11-09 新增)=====
//
// 起因(用户真机反馈,远程桌面换分辨率后):
//   ① 品牌 logo「只显示一部分」—— logo 位图只在建窗时按当时的尺寸加载一次,
//      DPI 变化后**从不重建**,绘制却按 1:1 BitBlt 取新矩形 ⇒ 只剩旧图一角;
//   ② 按钮符号「偏大、溢出键帽」—— 键帽按 dpr 缩放,图标却按 raw 像素画,
//      两者比例失衡。
//
// 口径(三条不变量):
//   1) 面板只有**一个**缩放系数来源:dpi / 96;
//   2) 系数**下限 1.0** —— 低于 100% 不做缩小(键帽会小到亚像素、命中区不可靠、
//      图标比例反常);由此窗口物理尺寸**恒 ≥ 逻辑尺寸**(下限与尺寸下限同时成立);
//   3) 键帽 / 槽位 / 窗口尺寸 / **图标几何** / fwicon 几何 用**同一系数**。
//
// ★ ScaleLogicalByDpi 的语义**不变**(单测 T25 锁死其逐位口径):本节只新增
//   「面板用 DPI 钳制」与「按系数换算坐标」两族函数,不改旧函数。

// 面板缩放系数的 DPI 区间:下限 100%(96),上限 400%(384)。
inline constexpr int kPanelMinDpi = 96;
inline constexpr int kPanelMaxDpi = 384;

// 面板用 DPI:钳到 [96, 384];dpi <= 0 视为 96(=100%)。
inline int ClampPanelDpi(int dpi) {
  if (dpi <= 0)
    dpi = kPanelMinDpi;
  if (dpi < kPanelMinDpi)
    dpi = kPanelMinDpi;
  if (dpi > kPanelMaxDpi)
    dpi = kPanelMaxDpi;
  return dpi;
}

// 面板缩放系数(= 钳后 dpi / 96)。**下限 1.0**(低于 100% 不缩小)。
inline double PanelScaleFactor(int dpi) {
  return ClampPanelDpi(dpi) / 96.0;
}

// logical 坐标 × 缩放系数 → 物理像素(就近舍入,.5 远离 0)。
// 系数下限 1.0:避免 <100% 时出现亚像素几何(不可点击、符号反常)。
inline int ScaleByFactor(int logical, double factor) {
  if (factor < 1.0)
    factor = 1.0;
  const double v = logical * factor;
  return static_cast<int>(v >= 0 ? v + 0.5 : v - 0.5);
}

}  // namespace panel_dpi
}  // namespace fluxing
