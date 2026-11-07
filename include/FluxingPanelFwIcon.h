#pragma once
//
// FluxingPanelFwIcon — 快捷设置栏「全/半角」键的**矢量图元表**与居中公式
//
// 单一事实源(constitution R7):生产代码(WeaselServer/QuickPanelDialog.cpp 的
// DrawIconFullwidth)与单测(test/TestQuickPanelDialog)读**同一份**实现,
// 任何一边都不许另抄一份公式 —— 复制即漂移(spec 092 记的「假绿」死法)。
//
// v7 需求(用户指令 2026-11-07):「设置栏图标中,全半角符号太小,不具有可视度。
// 可以调整排版,参考搜狗输入法,按钮内对角线布局,从而可以放大。」
//
#include <windows.h>

namespace fluxing {
namespace panel_fwicon {

// ★ 为什么不再用字体字形(v6 方案):CJK 标点的墨迹只占 em 盒的极小一块 ——
//   v6 用 Microsoft YaHei -15px 画「,。」,真机实测全角墨迹仅 **18×4 raw
//   px**、半角仅 **8×5**,浅灰 RGB(130,130,140) 压在浅蓝底 RGB(200,225,250)
//   上几乎不可见(用户原话:「全半角符号太小,不具有可视度」)。
//   字号调大也长不大(标点墨迹 ≈ 0.2 em),且会撑破 28 logical 的键帽。
//   改用 GDI 矢量图元后直径/线宽可精确控制 ⇒ 全角空心环直径 15 raw px。
//
// ★ 为什么是对角线布局:参考搜狗输入法状态栏(spec 099 P0-1 采集到的槽 3 =
//   「。」+「,」)—— 右上「。」空心环 + 左下「,」尾,两个标点沿对角排布:
//   各占约 25 raw px 见方,互不遮挡,又各自可辨。
//
// 坐标系:**viewBox 局部坐标**(raw px,不随 dpr 缩放),与快捷面板另外 5 个
// 线描图标同一约定(viewBox 30×30,中心 (15,15))。

enum ShapeKind {
  kHollowEllipse = 0,  // 空心环:2px 描边 + NULL_BRUSH(内部留面板底色)
  kSolidRoundRect = 1, // 实心圆:RoundRect + 圆角半径 = 半边长
  kSolidPolygon = 2,   // 实心多边形(逗号尾巴)
};

struct Shape {
  ShapeKind kind;
  RECT rect;     // kHollowEllipse / kSolidRoundRect:图元**路径框**(不含笔宽外扩)
  POINT pts[3];  // kSolidPolygon:三角形顶点(viewBox 局部坐标)
};

struct Icon {
  Shape shapes[3];
  int shapeCount;
  RECT ink;      // 墨迹盒并集(viewBox 局部坐标,已含笔宽外扩)
  bool hollow;   // true = 全角(空心环) / false = 半角(实心点)
};

// 描边笔宽(raw px):2 —— 与另外 5 个线描图标同口径。
constexpr int kStrokeWidth = 2;

// 全角态:空心环「。」路径框 (12,4)-(26,18)(路径直径 14 px);
//   逗号尾 = 实心头椭圆 (5,16)-(11,22) + 尾巴三角 (4,20)(10,22)(3,27);
//   声明墨迹盒 ink = (3,3)-(27,27) = 25×25 raw px(含 2px 描边外扩,
//   右侧多留 1px 作安全边界)⇒ 居中后中心恰为 (15,15)。
// 半角态:实心点「.」路径框 (14,5)-(24,15)(路径直径 10 px);
//   小逗号 = 头椭圆 (7,19)-(12,24) + 尾巴三角 (6,22)(11,24)(4,27);
//   声明墨迹盒 ink = (4,5)-(24,27) = 21×23 raw px。
inline Icon Build(bool fullwidth) {
  Icon ic;
  ic.shapeCount = 3;
  ic.hollow = fullwidth;
  if (fullwidth) {
    ic.shapes[0].kind = kHollowEllipse;
    ic.shapes[0].rect = RECT{12, 4, 26, 18};
    ic.shapes[1].kind = kSolidRoundRect;
    ic.shapes[1].rect = RECT{5, 16, 11, 22};
    ic.shapes[2].kind = kSolidPolygon;
    ic.shapes[2].pts[0] = POINT{4, 20};
    ic.shapes[2].pts[1] = POINT{10, 22};
    ic.shapes[2].pts[2] = POINT{3, 27};
    ic.ink = RECT{3, 3, 27, 27};
  } else {
    ic.shapes[0].kind = kSolidRoundRect;
    ic.shapes[0].rect = RECT{14, 5, 24, 15};
    ic.shapes[1].kind = kSolidRoundRect;
    ic.shapes[1].rect = RECT{7, 19, 12, 24};
    ic.shapes[2].kind = kSolidPolygon;
    ic.shapes[2].pts[0] = POINT{6, 22};
    ic.shapes[2].pts[1] = POINT{11, 24};
    ic.shapes[2].pts[2] = POINT{4, 27};
    ic.ink = RECT{4, 5, 24, 27};
  }
  return ic;
}

// 把墨迹盒并集**就近舍入**居中到 viewBox 中心(viewBox 边长由调用方传入,
// 此处不复制生产代码的 kIconViewBox —— constitution R7 单一事实源)。
//
// 为什么是就近舍入而不是向下取整:30 宽的 viewBox 里放 25 宽的墨迹,
// 向下取整会把中心推到 14(偏左 1 px);就近舍入 ⇒ 中心正好 15,
// 与另外 5 个线描图标(viewBox 局部 bbox 中心恒为 15)完全对齐。
inline POINT CenteringOffset(const Icon& ic, int viewBox) {
  const int w = ic.ink.right - ic.ink.left + 1;
  const int h = ic.ink.bottom - ic.ink.top + 1;
  POINT off;
  off.x = (viewBox - w + 1) / 2 - ic.ink.left;
  off.y = (viewBox - h + 1) / 2 - ic.ink.top;
  return off;
}

}  // namespace panel_fwicon
}  // namespace fluxing
