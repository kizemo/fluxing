#pragma once
#include <cstddef>
#include <unordered_map>
#include <cstdint>

namespace fluxing {
using SessionId = std::uint32_t;

// Pure, dependency-free Shift state machine.
// Extracted from WeaselServer / RimeWithWeaselHandler so that 11 unit tests
// can drive it without IPC / Rime / GUI surfaces.
//
// Semantics (per spec 077 §2.1, mirrored in test/TestShiftIPCStateMachine.cpp):
// - Down then Up with no intervening key => fire select(index=L?1:R?2:0).
// - Intervening non-Shift key between Down and Up => no fire.
// - Up without prior Down => no fire.
// - Repeated Down overrides lastIsLeft; final Up is what is observed.
// - Reset(sid) wipes per-session state; OnSessionDestroyed erases the map entry.
// - Session state is keyed by SessionId (per-WeaselSessionId).
class ShiftStateMachine {
 public:
  struct EventResult {
    bool fire_select = false;
    std::size_t index = 0;  // 0-based; valid only if fire_select==true
  };

  void Reset(SessionId sid) { states_.erase(sid); }

  void OnShiftDown(SessionId sid, bool is_left) {
    auto& s = states_[sid];
    s.downRecorded = true;
    s.interveningKey = false;
    s.lastIsLeft = is_left;
  }

  EventResult OnShiftUp(SessionId sid, bool is_left) {
    (void)is_left;
    auto it = states_.find(sid);
    if (it == states_.end()) return {};
    EventResult r;
    r.fire_select = it->second.downRecorded && !it->second.interveningKey;
    r.index = it->second.lastIsLeft ? 1u : 2u;
    states_[sid] = State{};  // C6: unconditionally reset on Up
    return r;
  }

  void OnInterveningKey(SessionId sid) {
    // Implemented in T03.
  }

  void OnSessionDestroyed(SessionId sid) { states_.erase(sid); }

  std::size_t DebugSessionCount() const { return states_.size(); }

 private:
  struct State {
    bool downRecorded = false;
    bool interveningKey = false;
    bool lastIsLeft = false;
  };
  std::unordered_map<SessionId, State> states_;
};
}  // namespace fluxing