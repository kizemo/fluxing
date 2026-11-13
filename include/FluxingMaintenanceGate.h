#pragma once
//
// ADR 0062 · 云同步的维护准入闸门（判据纯函数 + 间隔常量）
//
// 背景（真相源：docs/adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md）：
//   每轮云同步都会 BeginMaintenance → rime_api->finalize()，而 finalize() 会
//   清空会话表（m_session_status_map）⇒ 客户端缓存的 ipc_id 变成孤儿 ⇒
//   to_session_id() 返回 0 ⇒ ProcessKeyEvent() 把 0 交给 process_key ⇒
//   **按键被吞**，该输入窗口哑到重新聚焦为止。
//
//   ⇒ 判据必须是「有没有活跃输入会话」，而不是「有没有组合串」。
//
// ★ 本头**只含 ASCII**（原因码 + 常量），一个中文字符都没有：
//   RimeWithWeasel.vcxproj **没有** /utf-8 ⇒ 窄字符串里的中文在那个编译
//   单元里会乱码。中文文案统一放在 WeaselServer/Cloud/CloudUserDictIo.h
//   （WeaselServer 与 TestFluxingCloud 都有 /utf-8）。
//
// 单一事实源（constitution R7）：生产（会话计数）与单测共用这里的同一份实现，
// 单测**不**复制第二份判据。
//

#include <cstdint>

namespace fluxing {
namespace maintenance_gate {

// 闸门判据：无活跃输入会话才允许进入维护态。
//
//   live_sessions = 会话表里 session_id != 0 的条目个数。
//   ★ 必须显式排除 session_id == 0：to_session_id() 用 operator[] 访问会话表，
//     键不存在时会**插入**一个空 status（session_id = 0）—— 那些是假条目，
//     若不排除，一个从没存在过的 ipc_id 就能把同步永久堵死。
inline bool MaintenanceGateAllows(int live_sessions) {
  return live_sessions <= 0;
}

// 闸门关闭的原因令牌（ASCII）。中文文案由云侧翻译（见 CloudUserDictIo.h）。
inline const char* MaintenanceGateReasonLiveSession() {
  return "live_input_session";
}

// 闸门关闭被跳过后，短间隔重试的等待时长（ADR 0062 决策 2）。
//
// ★ 必须是**短**间隔（5 分钟），不是失败退避阶梯的末档（60 分钟）：
//   失败退避的前提是「出了错」，而「用户在打字」不是错误 ⇒ 用失败退避会让
//   用户连续打一会儿字就把同步彻底停摆。
constexpr int64_t kMaintenanceSkipRetryIntervalMs = 5 * 60 * 1000;

}  // namespace maintenance_gate
}  // namespace fluxing
