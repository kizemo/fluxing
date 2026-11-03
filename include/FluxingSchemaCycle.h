#pragma once
//
// FluxingSchemaCycle.h — 「下一个输入方案」的纯函数(spec 099 P0-3)
//
// 用途:快捷面板槽 0 = 「切换到下一个输入方案」(此前是死槽,见
// docs/archive/handoffs/handoff-v1.0.0-bare-shift-ascii-toggle-2026-10-28.md
// 的槽位表)。切换目标的选择逻辑必须**可单测** —— 端到端要靠装机手点,
// 不能只有端到端一条证据链,所以把纯逻辑抽成 header-only 纯函数:
//
//   - 当前方案在列表里 ⇒ 下一个(到尾绕回第一个)
//   - 当前方案不在列表里 ⇒ 列表第一个
//   - 列表为空 ⇒ 空串(调用方据此「不切」)
//
// 单一事实源(constitution R7):本头文件同时被
//   RimeWithWeasel/RimeWithWeasel.cpp            (生产)
//   test/TestQuickPanelDialog/TestQuickPanelDialog.cpp (测试)
// 消费,两边不允许各写一份循环逻辑。
//
#include <string>
#include <vector>

namespace fluxing {

inline std::string NextSchemaId(const std::vector<std::string>& available,
                                const std::string& current) {
  if (available.empty())
    return std::string();
  for (size_t i = 0; i < available.size(); ++i) {
    if (available[i] == current)
      return available[(i + 1) % available.size()];
  }
  return available.front();
}

}  // namespace fluxing
