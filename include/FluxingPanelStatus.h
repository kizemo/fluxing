#pragma once
//
// spec 099 P0-4: 快捷设置栏「当前输入状态回显(中/英 + 全/半角)」的纯逻辑
// + 状态区布局常量(单一真相源,constitution R7)。
//
// 为什么是 header-only:
//   1) 生产绘制(WeaselServer/QuickPanelDialog.cpp::PaintOpaqueContent)与
//      单测(test/TestQuickPanelDialog)读**同一份**文字映射与几何常量 ——
//      复制即漂移(spec 092 记的「假绿」死法)。
//   2) 用户可见字符串保持简体中文(constitution P5)。
//
// 布局约束(spec 099 P0-4「命中区不变」):
//   状态区必须落在**所有槽位矩形之外**,否则状态像素会压到槽位上,
//   槽位命中区就不可能保持不变。
//   状态区的**实际**矩形由生产代码计算(QuickPanelDialog::StatusRect),
//   算法 = 「最后一个槽位的右边界 + 间隙」→ 「内容右界(= 面板宽 - padding)」。
//   算式(logical 像素,100% DPI 实例):
//     面板宽 245,左/右 padding 5 ⇒ 内容右界 240
//     最后一个槽位右边界 = 5(pad) + 35(brand) + 2(gap)
//                          + 3*(35(btn) + 14(gap)) + 35(btn) = 224
//     状态间隙 1 ⇒ 状态区 x = 225 ⇒ 与全部槽位**不相交**(225 > 224)。
//   换算到 150% DPI 时槽位与状态区各自按 dpr 舍入,右边界可能比 100% 时
//   多占 1~2 物理像素 —— 这正是「状态区从槽位右边界推导」而不是「写死
//   x = 225」的原因:写死 225 在 150% 会压到槽位 3 上。
//
#include <string>

namespace fluxing {
namespace panel_status {

// 当前输入状态的**内存快照**(显示路径零 I/O,constitution P2)。
// 由服务端在内存里填好后交给面板;面板只读副本。
struct PanelState {
  bool ascii_mode = false;   // true = 英文(ASCII)态
  // true = **中文标点**模式(panel 内部沿用旧字段名 fullwidth,避免重命名波及
  // RimeWithWeasel / WeaselServerApp / 单测三端;语义见下方 ShapeIsFullwidth)。
  // ★ 不是 librime 的 full_shape:full_shape 会把数字与符号换成全角形态,
  //   用户已明确不要那个行为(ADR 0041)。
  bool fullwidth = false;
  bool always_show = false;  // 面板「常驻显示」开关
  std::wstring schema_name;  // 当前输入方案名(可空)
  std::wstring version;      // 产品版本号(可空)
};

// 状态区几何(logical 像素)。
// Fluxing QuickPanel v4 需求⑥: 右侧竖排状态回显区已删除 —— 状态改由
// 槽 4 / 槽 5 的**键帽单字**承担 ⇒ kStatusCharHeight 的语义从「竖排字高」
// 变成「键帽标签字高」(竖排行间距常量 kStatusCharGap 随竖排区一并删除)。
constexpr int kStatusCharHeight = 16;
// 状态区与最后一个槽位之间的间隙(logical)。必须 >= 1,否则状态像素会
// 贴住槽位边框(命中区不变的前提)。
constexpr int kStatusGap = 1;

// 中文态显示「中」;英文态(ASCII)显示「英」。
inline const wchar_t* AsciiModeText(bool asciiMode) {
  return asciiMode ? L"英" : L"中";
}

// 槽 5 状态文案:**中文标点** / **英文标点**。
//
// 批次 H 语义纠正(2026-11-07,用户原话):「可能我对全半角的理解有误。我希望的
// 全角,只是中文标点符号。但数字＼其他符号等,还应该和英文状态一致,应该还是
// 半角的。参照搜狗输入法等其他输入法的设置。」
// ⇒ 这个槽**不再叫「全/半角」**:它切的是**标点语言**,不是字符宽度。
//   字符宽度(数字/符号是否全角)由 librime 的 full_shape 决定,而本 fork
//   永不开启它 ⇒ 数字与符号**始终半角**,与英文态一致。
// ⇒ 旧名 ShapeModeText 一并改掉:名字叫 Shape 会让人继续把两个概念混在一起。
inline const wchar_t* PunctModeText(bool chinesePunct) {
  return chinesePunct ? L"中文标点" : L"英文标点";
}

// 面板「中文标点」的**显示口径**(单一事实源,constitution R7)。
//
// 「中文标点」= 生效的标点是中文标点:
//   - 英文态(ascii_mode):ascii_composer 直接提交 ASCII ⇒ 标点自然是英文
//     ⇒ 面板记「英文标点」;
//   - 中文态 + ascii_punct:librime 的 punctuator 直接 kNoop ⇒ 原样 ASCII
//     ⇒ 也是「英文标点」(这正是用户需求②要的「半角 = 英文标点」);
//   - 中文态 + 无 ascii_punct:走中文标点表 ⇒ 「中文标点」。
//
// ★ 为什么不能沿用 v6 的 `full_shape && !ascii_mode`:本 fork 的
//   punctuator/full_shape 与 punctuator/half_shape **两张表都映射中文标点**
//   (output/data/default.yaml),而且 full_shape 在中英切换时**根本不变** ——
//   拿它当判据就是「切到英文图标仍是全角」那个缺陷的根因(v6)。
// ★ 函数名保留 ShapeIsFullwidth:调用点跨 RimeWithWeasel / 单测两处,重命名
//   风险大于收益;语义按上面三条读。
inline bool ShapeIsFullwidth(bool ascii_mode, bool ascii_punct) {
  return !ascii_mode && !ascii_punct;
}

}  // namespace panel_status
}  // namespace fluxing
