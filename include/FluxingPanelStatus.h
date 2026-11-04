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
  bool fullwidth = false;    // true = 全角
  bool always_show = false;  // 面板「常驻显示」开关
  std::wstring schema_name;  // 当前输入方案名(可空)
  std::wstring version;      // 产品版本号(可空)
};

// 状态区几何(logical 像素)。
// 两个状态字符的字高与行间距(竖排两字:上=中/英,下=全/半)。
constexpr int kStatusCharHeight = 12;
constexpr int kStatusCharGap = 3;
// 状态区与最后一个槽位之间的间隙(logical)。必须 >= 1,否则状态像素会
// 贴住槽位边框(命中区不变的前提)。
constexpr int kStatusGap = 1;

// 中文态显示「中」;英文态(ASCII)显示「英」。
inline const wchar_t* AsciiModeText(bool asciiMode) {
  return asciiMode ? L"英" : L"中";
}

// 全角显示「全」;半角显示「半」。
inline const wchar_t* ShapeModeText(bool fullwidth) {
  return fullwidth ? L"全" : L"半";
}

}  // namespace panel_status
}  // namespace fluxing
