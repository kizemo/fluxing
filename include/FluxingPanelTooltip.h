#pragma once
//
// spec 099 P0-5: 快捷设置栏**每槽 tooltip**的纯文本生成(单一真相源)。
//
// 契约(tasks.md P0-5):tooltip 内容 = 「名称 + 当前态」。
//   槽 0 切换到下一个输入方案 · 当前:<输入方案名>
//   槽 1 用户短语             · (无当前态)
//   槽 2 快捷键设置           · 常驻显示:开/关
//   槽 3 关于 / 账号说明      · 版本:<x.y.z>
//   槽 4 中英文切换           · 当前:中/英      ← v4 需求④⑤
//   槽 5 全半角切换           · 当前:全/半      ← v4 需求③⑤
//
// v4 变更说明(QuickPanel 重设计 stage2):
//   槽 1 原先借用「当前输入:中/英」回显中英状态 —— 但槽 1 实际是「用户短语」,
//   状态与槽位语义不符(R23:文档/标签说的必须字面成立)。v4 把中英状态迁到
//   新增的槽 4(中英文键),槽 1 改为无当前态。
//
// 为什么是 header-only:
//   1) 单一真相源(constitution R7)—— 槽名与「当前态」措辞只写一份,
//      生产绘制与单测读同一份;抄一份进测试就是漂移源(spec 092「假绿」)。
//   2) 用户可见字符串保持简体中文(constitution P5)。
//   3) 纯逻辑 ⇒ 可离线单测,不依赖窗口/贴图。
//
#include <string>

// spec 099 P0-4: 当前输入状态的内存快照(单一真相源)。
#include "FluxingPanelStatus.h"

namespace fluxing {
namespace panel_tooltip {

// 槽位名称(用户可见,简体中文)。索引越界 ⇒ 空串。
inline const wchar_t* SlotName(int slot) {
  switch (slot) {
    case 0:
      return L"切换到下一个输入方案";
    case 1:
      return L"用户短语";
    case 2:
      return L"快捷键设置";
    case 3:
      return L"关于 / 账号说明";
    case 4:
      return L"中英文切换";
    case 5:
      return L"全半角切换";
    default:
      return L"";
  }
}

// 槽位「当前态」文案。索引越界 ⇒ 空串。
inline std::wstring SlotStateText(int slot,
                                  const panel_status::PanelState& st) {
  switch (slot) {
    case 0: {
      const std::wstring name =
          st.schema_name.empty() ? std::wstring(L"未知") : st.schema_name;
      return L"当前:" + name;
    }
    case 1:
      // v4: 槽 1 是「用户短语」, 与中/英状态无关 ⇒ 无当前态
      // (tooltip 只给名称)。中英状态由槽 4 承担。
      return L"";
    case 2:
      return std::wstring(L"常驻显示:") + (st.always_show ? L"开" : L"关");
    case 3: {
      const std::wstring ver =
          st.version.empty() ? std::wstring(L"未知") : st.version;
      return L"版本:" + ver;
    }
    case 4:
      return std::wstring(L"当前:") + panel_status::AsciiModeText(st.ascii_mode);
    case 5:
      return std::wstring(L"当前:") + panel_status::ShapeModeText(st.fullwidth);
    default:
      return L"";
  }
}

// 组装完整 tooltip 文本:「名称 · 当前态」。没有当前态时只给名称。
inline std::wstring BuildTooltip(int slot,
                                 const panel_status::PanelState& st) {
  const wchar_t* name = SlotName(slot);
  if (!name || !*name)
    return L"";
  const std::wstring state = SlotStateText(slot, st);
  if (state.empty())
    return std::wstring(name);
  return std::wstring(name) + L" · " + state;
}

}  // namespace panel_tooltip
}  // namespace fluxing
