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

}  // namespace panel_dpi
}  // namespace fluxing
