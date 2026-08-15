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

  void Reset(SessionId sid) {}
  void OnShiftDown(SessionId sid, bool is_left) {}
  EventResult OnShiftUp(SessionId sid, bool is_left) { return {}; }
  void OnInterveningKey(SessionId sid) {}
  void OnSessionDestroyed(SessionId sid) {}
  std::size_t DebugSessionCount() const { return 0; }

 private:
  struct State {
    bool downRecorded = false;
    bool interveningKey = false;
    bool lastIsLeft = false;
  };
  std::unordered_map<SessionId, State> states_;
};
}  // namespace fluxing