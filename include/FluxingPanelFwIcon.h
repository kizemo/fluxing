#pragma once
//
// FluxingPanelFwIcon — 快捷设置栏「中文标点 / 英文标点」键的**图元表**与几何公式
//
// 单一事实源(constitution R7):生产代码(WeaselServer/QuickPanelDialog.cpp 的
// DrawIconFullwidth)与单测(test/TestQuickPanelDialog)读**同一份**实现,
// 任何一边都不许另抄一份公式 —— 复制即漂移(spec 092 记的「假绿」死法)。
//
// ── 版本沿革(每一版都是用户真机人眼反馈驱动的定向修正,不是重构)──────────
// v7 (用户指令 2026-11-07):「设置栏图标中,全半角符号太小,不具有可视度。
//    可以调整排版,参考搜狗输入法,按钮内对角线布局,从而可以放大。」
//    ⇒ 右上「。」+ 左下「，」对角排布,互不遮挡又各自可辨。
// v8 (同批真机人眼反馈):「句号(中英文)都偏大,而中文逗号则偏小,且变形,
//    看不出是中文逗号。」⇒ 环外径 16→12、点 10→7、逗号放大到 11×13 并合成
//    **单一 10 点凹形轮廓**(v7 的「头+尾」两个分离图元读成「点 + 划痕」)。
// v8.1:「全半角符号的线,需要调细,才能更清楚分辨出来」⇒ 环笔宽 2→1
//    (环墨量 50 px→32 px、洞径 6×6→10×10;「空」必须比「实」轻才分得开)。
// v9 (批次 H):「全半角符号不清晰,糊成一块的感觉」⇒ 病灶不是线粗,而是
//    **两图元斜向相接**(环墨迹左边 x=16 与逗号墨迹右边 x=15 之间有 0 空列,
//    环底 y=14 与逗号顶 y=15 相邻)⇒ 判据改为「≥2 空列 + ≥2 空行、x/y 均
//    不相交」,全角墨迹 25×27、半角 19×25(两轴奇数 ⇒ 居中后中心恰 (15,15))。
// v10 (用户指令 2026-11-07,批次 H 追加):
//    「全半角图标中,逗号仍然形状不佳,有锯齿,且圆形部分和尾巴部分连接不圆滑。
//     建议直接使用字符逗号。」⇒ **逗号改为真字形**,环/点保持矢量图元不动。
// v12 (用户指令 2026-11-08,批次 J):「中文标点辨识度已经很高,但英文的逗号,
//    辨识度仍然需要改进」⇒ 英文态逗号目标墨迹盒 6×9 → **8×12**
//    (Times New Roman 52px, origin=(2,3)),墨量 +78%,与中文态 8×13 同量级。
//
// ── 为什么 v9 的多边形一定有锯齿(别再用手绘轮廓修)─────────────────────
// GDI 的 `Polygon` / 扫描线填充族**没有抗锯齿**:每条边都被光栅化成整像素台阶。
// 12 点轮廓在 11×13 尺寸里,一段边只跨 1~2 px,折角与台阶全部落在可见像素上
// ⇒ 用户看到的就是「锯齿」;而「圆头与尾巴连接不圆滑」是同一件事 —— 折线没有
// 曲率。**加大点数只能把台阶变密,变不出平滑边**。
//
// ── 为什么 v6 的「字体方案不可行」结论是错的 ─────────────────────────────
// v6 用 `Microsoft YaHei -15px` 画「,。」,真机实测全角墨迹仅 18×4 raw px、
// 半角 8×5,浅灰压在浅蓝底上几乎不可见 ⇒ 当时判「字体方案不可行」。
// 真相是**字号取小了**(CJK 标点墨迹 ≈ 0.25 em),不是方案错。
// v10 用 **-48 px** 重测(tmp/probe-glyph-comma.ps1,96 dpi,raw px):
//   Microsoft YaHei h=48 ⇒ U+FF0C「，」墨迹 **7×13** origin=(7,6)
//                         U+002C「,」 墨迹 **7×13** origin=(2,6)
//   ⇒ 恰等于 v9 手绘逗号的目标高度 **13**,字形可直接用。
//   (h=40 → 6×11;h=56 → 8×15;h=72 → 11×19 —— 字号线性可调。)
// ★ 后人别再被 v6 的注释误导:字体方案能行,前提是字号够大。
//
// ── 字体抗锯齿为什么不会重现「黑框 / 发暗」──────────────────────────────
// v4 品牌图标黑框(记忆 #34)的病灶是 GDI **AlphaBlend**:它把半透明源像素按
// 反向权重算,`255 − src_alpha` 的形状就是黑边。`TextOutW` 的抗锯齿是**把灰阶
// 直接混合进已有底色后写入**,输出像素 alpha 恒 255,不产生半透明像素
// ⇒ 与 AlphaBlend 完全不同的一条路径,不会复发。
//
// ── 坐标系 ─────────────────────────────────────────────────────────────
// **viewBox 局部坐标**(raw px,不随 dpr 缩放),与快捷面板另外 5 个线描图标
// 同一约定(viewBox 30×30 在 WeaselServer/QuickPanelDialog.h,中心 (15,15)——
// 此处不复制那个常量,constitution R7)。
//
#include <windows.h>

namespace fluxing {
namespace panel_fwicon {

enum ShapeKind {
  kHollowEllipse = 0,  // 空心环:1px 描边 + NULL_BRUSH(内部留面板底色)
  kSolidRoundRect = 1, // 实心圆:RoundRect + 圆角半径 = 半边长
};

// 描边笔宽(raw px):**1**。
//   用户 2026-11-07:「全半角符号的线,需要调细,才能更清楚分辨出来」。
//   只影响**唯一带描边的图元**(全角空心环);实心点与逗号字形都不走这支笔。
//   为什么 1 而不是 2:见文件头 v8.1 段的墨量实测(50 px → 32 px)。
constexpr int kStrokeWidth = 1;

// 逗号字形的**种子字号**(px,实际传给 LOGFONTW.lfHeight 时取负)。
// 48 = 实测基准:各态真字形在 48 px 下量出墨迹盒,再由 ScaleFontHeight
// 反推实际字号(见文件头 v11 段)。换字体后**必须**重测目标墨迹盒。
constexpr int kCommaFontHeight = 48;

// ── v11:两态各用**自己文字体系**的正字字形 ─────────────────────────────
// v10 病灶:两态都用 Microsoft YaHei,而 YaHei 的 U+FF0C「，」与 U+002C「,」
// **是同一个楔形**(48 px 下两者墨迹盒同为 7×13,只差 origin.x)
// ⇒ 归一居中后两态像素几乎全同 ⇒ 用户判「无法区分中英文逗号」
//   (离线对照 tmp/v11b-sheet.png A 行:两态逗号肉眼同一形状)。
// v11 修法:**换字体** —— 中文态用宋体「，」(带书法尾的正经中文逗号),
// 英文态用 Times New Roman「,」(卷尾拉丁逗号);两者**形状**不同、
// **尺寸**也不同(13 vs 9)⇒ 形状 + 尺寸双重可区分。
// (用户诉求原文:「逗号形态仍然不好,且无法区分中英文逗号…建议直接使用输入字符」)
//
// 目标墨迹盒 = 实测值(探针 tmp/probe-fwicon-v11b.ps1 / tmp/probe-fwicon-v12.ps1,
// 96 dpi raw px):
//   宋体 SimSun 48px U+FF0C ⇒ 8×13  origin=(6,11)   (v11 定稿)
//   Times New Roman 52px U+002C ⇒ 8×12  origin=(2,3)  (v12:英文态放大)
//   (v11 的英文态是 TNR **39px** ⇒ 6×9 origin=(2,3),被用户判「辨识度仍需改进」)
// ★ 宽度常量是**字体替换敏感项**:目标机器若缺该字体,GDI 换别的字体 ⇒
//   墨迹宽度会变、字形相对目标框横向偏 0~1 px(ScaleFontHeight 只自校正
//   **高度**)。后果不影响可读性;要彻底修得把宽度也改成运行时测量.
//
// ── v12:英文态逗号放大到与中文态**同量级** ─────────────────────────────
// 用户原话(2026-11-08):「中文标点辨识度已经很高,但英文的逗号,辨识度仍然
// 需要改进」。病灶 = 英文态目标墨迹盒只有 6×9 ⇒ 墨量 54 px,是中文态
// (8×13 = 104 px)的 **52%**;缩到面板实际像素后笔画只剩 1~2 px 宽,
// 肉眼看得出「有个笔画」但读不出「是逗号」。
// 修法:6×9 → **8×12**(Times New Roman **52px** 实测 8×12、origin=(2,3)),
// 墨量 96 px(+78%),与中文态 104 px 同量级。
// ★ 为什么是 52px 而不是种子 48px:TNR 在 48px 下逗号墨迹只有 7×11,
//   ScaleFontHeight(48,11,12)=52 才落到 8×12(实测表见 tmp/v12-notes.txt)。
// ★ 代价(明写,别当没看见):两态逗号**宽度**同为 8(高度 13 vs 12)⇒ v11 的
//   「形状 + 尺寸双重可区分」退化为「形状 + 句号图元(空心环 vs 实心点)」;
//   环(12px 空心) 与 点(7px 实心) 的对比远强于逗号的 1 px 尺寸差,
//   两态仍可分辨;换来的是英文逗号与中文逗号同样的可读性。
constexpr int kCommaInkWidthCn = 8;
constexpr int kCommaInkHeightCn = 13;
constexpr int kCommaInkWidthEn = 8;
constexpr int kCommaInkHeightEn = 12;

// 两态逗号各自用的字体(中文字体出中文逗号,拉丁字体出拉丁逗号)。
constexpr const wchar_t* kCommaFaceCn = L"SimSun";  // 宋体
constexpr const wchar_t* kCommaFaceEn = L"Times New Roman";

// 逗号墨迹盒的**下沿**(viewBox 局部坐标):两态都底对齐到 28,
// 上沿随各自高度上移(中文 16 / 英文 20)⇒ 逗号与句号的对角关系不变。
constexpr int kCommaBottom = 28;

struct Shape {
  ShapeKind kind;
  RECT rect;  // 图元**路径框**(GDI 覆盖约定:只着墨 left..right-1 / top..bottom-1)
};

struct Icon {
  Shape shapes[2];  // [0] 句号(环/点);逗号是字形,不再占图元位
  int shapeCount;
  RECT commaInk;      // 逗号字形墨迹盒应落到哪里(闭区间,viewBox 局部坐标)
  wchar_t commaChar;  // 全角 U+FF0C「，」/ 半角 U+002C「,」
  const wchar_t* commaFace;  // v11:该态逗号用的字体(中文字体 / 拉丁字体)
  RECT ink;           // 墨迹盒并集(= 各图元与逗号的实际覆盖,无外扩)
  bool hollow;        // true = 中文标点(空心环) / false = 英文标点(实心点)
};

// 字形的 GDI 度量快照 —— 把 GLYPHMETRICS 的三个字段摘出来,
// 好让「由度量算落笔点」是**不依赖 <wingdi.h> 内部结构**的纯函数(可单测)。
struct GlyphInk {
  int width = 0;    // gmBlackBoxX
  int height = 0;   // gmBlackBoxY
  int originX = 0;  // gmptGlyphOrigin.x
  int originY = 0;  // gmptGlyphOrigin.y
};

// 单个图元的墨迹盒(GDI 覆盖约定,勿改成「含右/下边界」的直觉写法):
//   RoundRect(l,t,r,b) / Ellipse(l,t,r,b) **都只覆盖**
//   x∈[l, r-1]、y∈[t, b-1] —— 右/下边界那一列/行**不着墨**;
//   笔宽**不向外溢出**:描边的内外边界都落在路径框内(1px 与 2px 实测同结论)。
// ★ 这条与直觉相反,是本文件最容易写错的一处。v8 初版按「笔沿路径居中、向外
//   溢出 stroke/2」建模并给环加了 ±e,实测证伪(tmp/probe-pen1.ps1:pen=2 与
//   pen=1 用同一路径框都只有 10×10 墨迹,px 数 50 / 24)。错的模型偏心 0.5 px。
inline RECT ShapeInkBox(const Shape& s) {
  return RECT{s.rect.left, s.rect.top, s.rect.right - 1, s.rect.bottom - 1};
}

// 字号自校正:实测墨迹高度与目标不符时(典型原因 = 目标机器没有
// Microsoft YaHei,被替换成别的字体),按比例反推一次字号。
// 正常机器上 measured == target ⇒ 原样返回 seed,不触发第二次建字体。
inline int ScaleFontHeight(int seedHeight,
                           int measuredInkHeight,
                           int targetInkHeight) {
  if (seedHeight <= 0)
    seedHeight = 1;
  if (measuredInkHeight <= 0 || targetInkHeight <= 0)
    return seedHeight;
  const int scaled =
      (int)((long long)seedHeight * targetInkHeight / measuredInkHeight);
  return scaled > 0 ? scaled : 1;
}

// 由字形度量反推落笔点,使**实际墨迹**落在 targetInk 的**正中**。
// 返回 {x = TextOutW 的 penX, y = 基线}(配合 SetTextAlign(TA_LEFT|TA_BASELINE))。
//
// 为什么按「中心对齐」而不是「左上角对齐」:实测墨迹可能与 kCommaInkWidth/Height
// 差 1 px(字体替换 / GDI 舍入),中心对齐让误差对称摊在两侧,不会把字形顶出
// 目标框、也不会破坏图标整体的奇偶居中。
//
// GDI 墨迹盒模型:字形原点在基线上方 originY 处,黑盒自原点向下延伸 height 行 ⇒
//   ink_left = penX + originX        ink_top = baseline − originY
// ⇒ penX = 目标中心x − (w−1)/2 − originX ; baseline = 目标中心y − (h−1)/2 + originY
inline POINT GlyphPenPointForInk(const RECT& targetInk, const GlyphInk& m) {
  const int cx = (targetInk.left + targetInk.right) / 2;
  const int cy = (targetInk.top + targetInk.bottom) / 2;
  POINT p;
  p.x = cx - (m.width - 1) / 2 - m.originX;
  p.y = cy - (m.height - 1) / 2 + m.originY;
  return p;
}

// 中文标点态(v11 墨迹 25×27):
//   句号「。」= 空心环,路径框 (16,2)-(28,14) ⇒ 覆盖 16..27,外径 **12**
//     (1px 描边 ⇒ 壁厚 1、洞径 10×10)。外径沿用 v8/v8.1(用户判「偏大」的是
//     v7 的 16),只把框整体上移 1 px,给下方的逗号让出空行。
//   逗号「，」= **真字形** U+FF0C,字体 **宋体**(v11),墨迹盒
//     (3,16)-(10,28) = **8×13**(v10 是 YaHei 7×13 —— 楔形,与英文态几乎一样)。
//   两图元在 x 上不相交(逗号右 10 < 环左 16 ⇒ 5 空列),y 上也不相交
//     (环底 13 < 逗号顶 16 ⇒ 2 空行)⇒ 不重现 v9 之前的「糊成一块」。
//   墨迹并集 = (3,2)-(27,28) = 25×27(两轴奇数 ⇒ 居中后中心恰为 15)。
// 英文标点态(v11 墨迹 19×25):
//   句号「.」= 实心点,路径框 (17,4)-(24,11) ⇒ 覆盖 **7×7**;
//     GDI 必须用 RoundRect(圆角 = 半边长),`Ellipse` 在 ≤10px 会光栅化成四角星
//     (证据 tmp/gdi-shape2.txt:10px 与 12px Ellipse 顶点只有 1 px)。
//   逗号「,」= **真字形** U+002C,字体 **Times New Roman**(v11),
//     墨迹盒 (5,17)-(12,28) = **8×12**(v12 由 6×9 放大;下沿与中文态同为 28,
//     上沿随高度上移)。
//   两图元不相交(6 空列 / 9 空行)。
//   墨迹并集 = (5,4)-(23,28) = 19×25。
inline Icon Build(bool fullwidth) {
  Icon ic = {};
  ic.shapeCount = 1;
  ic.hollow = fullwidth;
  if (fullwidth) {
    ic.shapes[0].kind = kHollowEllipse;
    ic.shapes[0].rect = RECT{16, 2, 28, 14};
    ic.commaChar = L'\xFF0C';  // 全角逗号「，」
    ic.commaFace = kCommaFaceCn;
    ic.commaInk.left = 3;
    ic.commaInk.top = kCommaBottom - kCommaInkHeightCn + 1;
    ic.commaInk.right = ic.commaInk.left + kCommaInkWidthCn - 1;
    ic.commaInk.bottom = kCommaBottom;
  } else {
    ic.shapes[0].kind = kSolidRoundRect;
    ic.shapes[0].rect = RECT{17, 4, 24, 11};
    ic.commaChar = L',';  // 半角逗号
    ic.commaFace = kCommaFaceEn;
    ic.commaInk.left = 5;
    ic.commaInk.top = kCommaBottom - kCommaInkHeightEn + 1;
    ic.commaInk.right = ic.commaInk.left + kCommaInkWidthEn - 1;
    ic.commaInk.bottom = kCommaBottom;
  }

  // 墨迹盒由坐标**算出来**,不手抄常量 —— 手抄的常量一改坐标就漂。
  RECT ink = ShapeInkBox(ic.shapes[0]);
  if (ic.commaInk.left < ink.left)
    ink.left = ic.commaInk.left;
  if (ic.commaInk.top < ink.top)
    ink.top = ic.commaInk.top;
  if (ic.commaInk.right > ink.right)
    ink.right = ic.commaInk.right;
  if (ic.commaInk.bottom > ink.bottom)
    ink.bottom = ic.commaInk.bottom;

  // CenteringOffset 的「中心恰为 viewBox 中心」以**奇数**边长成立
  // (offset = (viewBox - w + 1)/2 ⇒ 中心 = (31 - w)/2 + (w - 1)/2)。
  // 当前两组坐标已自然为奇数;若后续微调出现偶数边,向右/下补 1 px 兜底,
  // 宁可墨迹盒多一行空行,也不要让图标整体偏 0.5 px。
  // ★ 判据方向别写反:width = right-left+1,故「宽为奇」⇔「right-left 为偶」。
  //   初版写成 (right-left)&1 == 0 正好写反 —— 反而把墨迹补成偶数边,
  //   居中整体偏 1 px(T33c/T33d 当场变红,这就是保留该断言的价值)。
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