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
// v8 需求(用户真机人眼反馈 2026-11-07,同一批次):
//   「全半角符号,句号(中英文)都偏大,而中文逗号则偏小,且变形,
//     看不出是中文逗号。」
//   ⇒ 三处定向修正,不推翻 v7 的架构:
//     ① 句号收小:全角空心环外径 16 → **12** raw px;半角实心点 10 → **7**。
//     ② 逗号放大:占位从 9×12 扩到 **11×13**(全角)/ **8×12**(半角)。
//     ③ 逗号改形:v7 是「头(圆)+ 尾(3 点三角)」两个**分离图元**,小尺寸下
//        头是点、尾是划痕,读不出「，」;v8 改为**单一 10 点凹形轮廓**
//        (宽头 + 渐细向左下扫的尾),无接缝、无碎块。
//
// v8.1 需求(用户指令 2026-11-07,同批):「全半角符号的线,需要调细,才能更
//   清楚分辨出来」⇒ 只收细**全角空心环的壁**(kStrokeWidth 2 → **1**),并把
//   环框外扩 1 px 以保住外径 12;实心点与两个逗号本是填充件、无描边,不动。
//   依据(tmp/probe-pen1.ps1 真机 GDI 实测,非推演):笔宽 2 时环墨量 **50 px**、
//   洞径 6×6,比半角实心点还重 ——「空」比「实」粗,两态一眼分不出;笔宽 1 时
//   环墨量 **32 px**、洞径 **10×10**,轻于点且一眼见底,空心/实心才立得住。
//
#include <windows.h>

namespace fluxing {
namespace panel_fwicon {

// ★ 为什么不再用字体字形(v6 方案):CJK 标点的墨迹只占 em 盒的极小一块 ——
//   v6 用 Microsoft YaHei -15px 画「,。」,真机实测全角墨迹仅 **18×4 raw
//   px**、半角仅 **8×5**,浅灰 RGB(130,130,140) 压在浅蓝底 RGB(200,225,250)
//   上几乎不可见(用户原话:「全半角符号太小,不具有可视度」)。
//   字号调大也长不大(标点墨迹 ≈ 0.2 em),且会撑破 28 logical 的键帽。
//   改用 GDI 矢量图元后直径/线宽可精确控制。
//
// ★ 为什么是对角线布局:参考搜狗输入法状态栏(spec 099 P0-1 采集到的槽 3 =
//   「。」+「,」)—— 右上「。」空心环 + 左下「，」自左下扫出的尾,两个标点沿
//   对角排布,互不遮挡,又各自可辨。
//
// 坐标系:**viewBox 局部坐标**(raw px,不随 dpr 缩放),与快捷面板另外 5 个
// 线描图标同一约定(viewBox 30×30,中心 (15,15))。

enum ShapeKind {
  kHollowEllipse = 0,  // 空心环:1px 描边 + NULL_BRUSH(内部留面板底色)
  kSolidRoundRect = 1, // 实心圆:RoundRect + 圆角半径 = 半边长
  kSolidPolygon = 2,   // 实心多边形(中文逗号「，」的整条轮廓)
};

// 多边形顶点上限:10 —— 逗号轮廓需要「宽头 + 渐细尾」两段弧度,
// 3 点的三角形在 11×13 尺寸下只能画出一根划痕(v7 的观感病根)。
constexpr int kMaxPolyPoints = 10;

struct Shape {
  ShapeKind kind;
  RECT rect;                 // kHollowEllipse / kSolidRoundRect:图元**路径框**
  POINT pts[kMaxPolyPoints]; // kSolidPolygon:轮廓顶点(viewBox 局部坐标,顺时针)
  int ptCount;               // kSolidPolygon 有效顶点数(3..kMaxPolyPoints)
};

struct Icon {
  Shape shapes[2]; // [0] 句号(环/点) [1] 中文逗号轮廓 —— 全角与半角同序
  int shapeCount;
  RECT ink;    // 墨迹盒并集(viewBox 局部坐标,= 各图元**实际覆盖**,无外扩)
  bool hollow; // true = 全角(空心环) / false = 半角(实心点)
};

// 描边笔宽(raw px):**1**。
//   用户 2026-11-07:「全半角符号的线,需要调细,才能更清楚分辨出来」。
//   只影响**唯一带描边的图元**(全角空心环);实心点与两个逗号都是填充件,
//   不走这支笔,想「加细」它们只能改轮廓,风险是重新触发「变形」抱怨。
//   为什么 1 而不是 2 或 1.5:见文件头 v8.1 段的墨量实测(50 px → 32 px)。
constexpr int kStrokeWidth = 1;

// 单个图元的墨迹盒(GDI 覆盖约定,勿改成「含右/下边界」的直觉写法):
//   RoundRect(l,t,r,b) / Ellipse(l,t,r,b) / Polygon **都只覆盖**
//   x∈[l, r-1]、y∈[t, b-1] —— 右/下边界那一列/行**不着墨**;
//   笔宽**不向外溢出**:描边的内外边界都落在路径框内(1px 与 2px 实测同结论)。
// ★ 这条与直觉相反,是本文件最容易写错的一处。v8 初版按「笔沿路径居中、向外
//   溢出 stroke/2」建模并给环加了 ±e,实测证伪(tmp/probe-pen1.ps1:pen=2 与
//   pen=1 用同一路径框都只有 10×10 墨迹,px 数 50 / 24)。错的模型偏心 0.5 px。
inline RECT ShapeInkBox(const Shape& s) {
  if (s.kind == kHollowEllipse || s.kind == kSolidRoundRect)
    return RECT{s.rect.left, s.rect.top, s.rect.right - 1, s.rect.bottom - 1};
  RECT r = RECT{s.pts[0].x, s.pts[0].y, s.pts[0].x, s.pts[0].y};
  for (int i = 1; i < s.ptCount; ++i) {
    if (s.pts[i].x < r.left)
      r.left = s.pts[i].x;
    if (s.pts[i].x > r.right)
      r.right = s.pts[i].x;
    if (s.pts[i].y < r.top)
      r.top = s.pts[i].y;
    if (s.pts[i].y > r.bottom)
      r.bottom = s.pts[i].y;
  }
  return r;
}

// 全角态(v8.1 实测墨迹 23×25):
//   句号「。」= 空心环,路径框 (16,3)-(28,15) ⇒ 覆盖 16..27,外径 **12**
//     (1px 描边 ⇒ 壁厚 1、洞径 10×10);v7 的 16 被用户判为「偏大」,v8 收小
//     到 12;v8.1 只把壁由 2 px 收细到 1 px,并把框外扩 1 px 保住外径不变。
//   逗号「，」= 单一 10 点凹形轮廓,bbox (5,15)-(15,27) = **11×13**;
//     宽头落在 y≈18-20(x 5..15),自 (9,26)/(6,27) 渐细向左下扫出尾尖,
//     读起来是「，」而不是 v7 的「点 + 划痕」。
//   墨迹并集 = (5,3)-(27,27) = 23×25(两轴均为奇数 ⇒ 居中后中心恰为 15)。
// 半角态(v8 墨迹 17×23):
//   句号「.」= 实心点,路径框 (17,5)-(24,12) ⇒ 覆盖 **7×7**;
//     GDI 必须用 RoundRect(圆角 = 半边长),`Ellipse` 在 ≤10px 会光栅化成四角星。
//   逗号「,」= 同一轮廓收窄版,bbox (7,16)-(14,27) = **8×12**。
//   墨迹并集 = (7,5)-(23,27) = 17×23。
inline Icon Build(bool fullwidth) {
  Icon ic;
  ic.shapeCount = 2;
  ic.hollow = fullwidth;
  if (fullwidth) {
    ic.shapes[0].kind = kHollowEllipse;
    ic.shapes[0].rect = RECT{16, 3, 28, 15};
    ic.shapes[0].ptCount = 0;

    ic.shapes[1].kind = kSolidPolygon;
    ic.shapes[1].ptCount = 10;
    const POINT fw[10] = {
        {7, 15}, {12, 15}, {15, 18}, {14, 22}, {12, 24},
        {9, 26}, {6, 27},  {5, 24},  {5, 20},  {6, 16},
    };
    for (int i = 0; i < 10; ++i)
      ic.shapes[1].pts[i] = fw[i];
  } else {
    ic.shapes[0].kind = kSolidRoundRect;
    ic.shapes[0].rect = RECT{17, 5, 24, 12};
    ic.shapes[0].ptCount = 0;

    ic.shapes[1].kind = kSolidPolygon;
    ic.shapes[1].ptCount = 10;
    const POINT hw[10] = {
        {9, 16}, {13, 17}, {14, 19}, {13, 22}, {12, 24},
        {10, 26}, {7, 27}, {7, 24},  {7, 21},  {8, 17},
    };
    for (int i = 0; i < 10; ++i)
      ic.shapes[1].pts[i] = hw[i];
  }

  // 墨迹盒由图元本身**算出来**,不手抄常量 —— 手抄的常量一改坐标就漂。
  RECT ink = ShapeInkBox(ic.shapes[0]);
  for (int i = 1; i < ic.shapeCount; ++i) {
    const RECT r = ShapeInkBox(ic.shapes[i]);
    if (r.left < ink.left)
      ink.left = r.left;
    if (r.top < ink.top)
      ink.top = r.top;
    if (r.right > ink.right)
      ink.right = r.right;
    if (r.bottom > ink.bottom)
      ink.bottom = r.bottom;
  }
  // CenteringOffset 的「中心恰为 viewBox 中心」以**奇数**边长成立
  // (offset = (viewBox - w + 1)/2 ⇒ 中心 = (31 - w)/2 + (w - 1)/2)。
  // 当前两组坐标已自然为奇数;若后续微调出现偶数边,向右/下补 1 px 兜底,
  // 宁可墨迹盒多一行空行,也不要让图标整体偏 0.5 px。
  // ★ 判据方向别写反:width = right-left+1,故「宽为奇」⇔「right-left 为偶」。
  // 初版写成 (right-left)&1 == 0 正好写反 —— 反而把墨迹补成偶数边,
  // 居中整体偏 1 px(T33c/T33d 当场变红,这就是保留该断言的价值)。
  const int ink_w = ink.right - ink.left + 1;
  const int ink_h = ink.bottom - ink.top + 1;
  if ((ink_w & 1) == 0)
    ++ink.right;
  if ((ink_h & 1) == 0)
    ++ink.bottom;
  ic.ink = ink;
  return ic;
}

// 把墨迹盒并集**就近舍入**居中到 viewBox 中心(viewBox 边长由调用方传入,
// 此处不复制生产代码的 kIconViewBox —— constitution R7 单一事实源)。
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