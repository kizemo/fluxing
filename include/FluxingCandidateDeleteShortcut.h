#pragma once
// spec 094 (2026-11-01):「Ctrl+Shift+数字」删除候选词的按键判定。
//
// 为什么单独成文件(与 include/FluxingCandidateIgnore.h 同一条理由):删词快捷键
// 的判定必须同时被生产代码(WeaselTSF/KeyEventSink.cpp)与单元测试工程
// (TestCandidateRButtonDown)编译。include/WeaselUtility.h 依赖 stdafx 预置头,
// 还把 COM/wrl 一并拖进来,单元测试工程吃不下;所以这里只依赖 windows.h + STL,
// 全部实现为 inline 纯函数,header-only ⇒ 测试工程零 .vcxproj 改动。
//
// 不变量(FR-101 / FR-102):
//   - 判定只依据「VK 码 + Ctrl/Shift 是否按下 + 本次是不是 key-down」。
//     绝不能用 ConvertKeyEvent 产出的 keycode:那是「产生的字符」
//     (Ctrl+Shift+1 -> '!'),反推不出数字键。
//   - 只有「候选窗真的有可见候选」时才拦截;否则 Ctrl+Shift+3/4 仍然归
//     Rime 引擎(ascii_punct / traditionalization),不抢引擎的绑定。
//   - 序号 1..N 指「当前可见页内」的第 N 个,与右键删词同一条 IPC 通路
//     (WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE),由服务端负责
//     「可见页内次序 -> 引擎页内次序」的换算。

#include <windows.h>

#include <cstddef>

namespace fluxing {

// 最多能删第 9 个候选(数字键 1..9)。
constexpr int kCandidateDeleteMaxNumber = 9;

// lParam 的 bit31 = 1 表示这是一次 key-up(Win32 键盘消息约定)。
// 注意 LPARAM 在 x64 上是 64 位,必须用 LONG_PTR 收,不能截成 long。
inline bool IsKeyDownLParam(LONG_PTR lparam) {
  return (lparam & (static_cast<LONG_PTR>(1) << 31)) == 0;
}

// GetKeyboardState 缓冲区里某个虚拟键是否处于「按下」状态(高位 bit)。
inline bool IsKeyStateDown(const BYTE* key_state, int vkey) {
  if (!key_state || vkey < 0 || vkey > 255)
    return false;
  return (key_state[vkey] & 0x80) != 0;
}

// 数字键 1..9 的 VK 码就是 ASCII '1'..'9'(VK_1 == 0x31)。
inline bool IsCandidateDeleteDigitVk(int vkey) {
  return vkey >= '1' && vkey <= '9';
}

// 判定一次按键是否命中「Ctrl+Shift+数字」删词快捷键。
// 命中返回候选序号(1..9),未命中返回 0。
inline int MatchCandidateDeleteShortcut(int vkey,
                                        bool ctrl_down,
                                        bool shift_down,
                                        bool key_down) {
  if (!key_down)
    return 0;
  if (!ctrl_down || !shift_down)
    return 0;
  if (!IsCandidateDeleteDigitVk(vkey))
    return 0;
  return vkey - '0';
}

// 序号是否落在当前可见页的范围内。
// visible_count == 0 表示候选窗没有可见候选 => 一律不拦截。
inline bool ShouldInterceptCandidateDelete(int number,
                                           std::size_t visible_count) {
  if (number < 1 || number > kCandidateDeleteMaxNumber)
    return false;
  if (visible_count == 0)
    return false;
  return static_cast<std::size_t>(number) <= visible_count;
}

}  // namespace fluxing
