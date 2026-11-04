#pragma once
//
// spec 099 P0-5: 快捷设置栏的**键盘可达性**纯状态机(单一真相源)。
//
// 为什么是 header-only 纯逻辑:
//   1) 单一真相源(constitution R7)—— 键位语义(Tab/Shift+Tab/←→ 移焦、
//      Enter|Space 激活、Esc 收起)只写一份;生产代码(QuickPanelDialog.cpp)
//      与单测(test/TestQuickPanelDialog)读同一份。若把键位表抄进测试,
//      改生产代码时测试不会变红(spec 092 记的「假绿」死法)。
//   2) 可判别 —— 单测直接断言「连按 3 次 Tab ⇒ 焦点绕回原槽」这类**序列**
//      行为;变异(把 +1 写成 +2、把 Shift+Tab 方向写反)会立刻变红。
//
// 焦点语义(为什么面板平时收不到按键):
//   面板窗口样式含 WS_EX_NOACTIVATE + ShowWindow(SW_SHOWNOACTIVATE)
//   ⇒ Alt+, 调出面板时**不抢**用户编辑框焦点(否则用户无法继续打字,
//   这是 L89-fix 记的「无法输入中文」事故)。因此键盘可达性靠:
//     - 用户**主动点击面板** ⇒ 生产代码 SetFocus(面板) ⇒ 面板进入键盘交互态;
//     - 键盘交互态下 Tab/←→/Enter/Space/Esc 由本状态机裁决;
//     - 面板失焦或收起 ⇒ 焦点归还用户原来的编辑框(生产代码负责)。
//
// Esc 语义(spec 099 tasks.md P0-5 要求「Esc 收起」, ADR 0019 D2 要求
// 「ESC 不隐藏快捷设置栏」—— 两者用**键盘交互态**调和):
//   - 未进入键盘交互态(面板没拿到焦点,Esc 根本到不了面板)⇒ Esc 只被吞掉,
//     面板不隐藏(守 ADR 0019 D2:用户 v1.1.1 验收口径「显隐只靠 Alt+,」)。
//   - 已进入键盘交互态 ⇒ Esc 收起**面板自身**,且**不触碰任何其他窗口**
//     (守 D0032:按 Esc 关「用户短语」窗口不得连带收起快捷设置栏)。
//
namespace fluxing {
namespace panel_keyboard {

// 键位裁决结果。
enum class Action {
  kNone,      // 不处理(交回 DefWindowProc)
  kSwallow,   // 吞掉(不产生任何用户可见动作)
  kMoveFocus, // 焦点移到新槽位(State.focused 已更新)
  kActivate,  // 激活当前焦点槽位
  kClose,     // 收起面板(只收面板)
};

// 键盘交互状态(纯数据,无 Win32 依赖)。
struct State {
  int focused = -1;       // 当前焦点槽位(-1 = 无焦点)
  int slot_count = 0;     // 可聚焦槽位数(生产代码 = kButtonCount)
  bool interactive = false;  // 是否已进入键盘交互态
};

// VK 虚拟键码(不依赖 windows.h,保证纯逻辑可独立测试)。
namespace vk {
constexpr int kTab = 0x09;
constexpr int kReturn = 0x0D;
constexpr int kShift = 0x10;
constexpr int kSpace = 0x20;
constexpr int kEnd = 0x23;
constexpr int kHome = 0x24;
constexpr int kLeft = 0x25;
constexpr int kUp = 0x26;
constexpr int kRight = 0x27;
constexpr int kDown = 0x28;
constexpr int kEscape = 0x1B;
}  // namespace vk

// 焦点在 [0, slot_count) 内循环移动。slot_count <= 0 ⇒ 无焦点可移。
inline int MoveFocus(int current, int delta, int slot_count) {
  if (slot_count <= 0)
    return -1;
  if (current < 0 || current >= slot_count)
    return (delta < 0) ? (slot_count - 1) : 0;
  int next = current + delta;
  if (next < 0)
    next += slot_count;
  if (next >= slot_count)
    next -= slot_count;
  return next;
}

// 裁决一次按键。shift = 是否按住 Shift(Shift+Tab 反向)。
inline Action HandleKey(State& st, int key, bool shift) {
  switch (key) {
    case vk::kTab:
      st.interactive = true;
      if (st.slot_count <= 0)
        return Action::kSwallow;
      st.focused = MoveFocus(st.focused, shift ? -1 : 1, st.slot_count);
      return Action::kMoveFocus;

    case vk::kLeft:
    case vk::kUp:
      st.interactive = true;
      if (st.slot_count <= 0)
        return Action::kSwallow;
      st.focused = MoveFocus(st.focused, -1, st.slot_count);
      return Action::kMoveFocus;

    case vk::kRight:
    case vk::kDown:
      st.interactive = true;
      if (st.slot_count <= 0)
        return Action::kSwallow;
      st.focused = MoveFocus(st.focused, 1, st.slot_count);
      return Action::kMoveFocus;

    case vk::kHome:
      st.interactive = true;
      if (st.slot_count <= 0)
        return Action::kSwallow;
      st.focused = 0;
      return Action::kMoveFocus;

    case vk::kEnd:
      st.interactive = true;
      if (st.slot_count <= 0)
        return Action::kSwallow;
      st.focused = st.slot_count - 1;
      return Action::kMoveFocus;

    case vk::kReturn:
    case vk::kSpace:
      st.interactive = true;
      // 没有焦点槽位时不激活任何东西(避免「回车误触发槽 0」)。
      if (st.focused < 0 || st.focused >= st.slot_count)
        return Action::kSwallow;
      return Action::kActivate;

    case vk::kEscape:
      // 未进入键盘交互态 ⇒ 只吞掉,面板不隐藏(守 ADR 0019 D2)。
      if (!st.interactive)
        return Action::kSwallow;
      // 已进入键盘交互态 ⇒ 收起**面板自身**(生产代码只 Hide 自己,
      // 不触碰其他窗口 ⇒ 守 D0032 不连带收起「用户短语」窗口)。
      return Action::kClose;

    default:
      return Action::kNone;
  }
}

}  // namespace panel_keyboard
}  // namespace fluxing
