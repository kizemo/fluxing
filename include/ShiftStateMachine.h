#pragma once
#include <cstddef>
#include <unordered_map>
#include <cstdint>

namespace fluxing {
using SessionId = std::uint32_t;

// ===== spec 086 / ADR 0015: Shift IPC is_left encode/decode share one source
// =====
//
// Wire contract (spec 077 section 2.3, pinned by test/TestShiftIPCWire):
//   wParam bit0 = 0 -> Shift_L (left); bit0 = 1 -> Shift_R (right).
//
// Incident (2026-10-28, reported by the user): the client encoded the
// contract correctly (is_left ? 0 : 1) but the server decoded bit0 the
// other way round ((wParam & 1u) != 0), so the two ends disagreed and a
// bare Shift selected the mirrored candidate (left Shift took the 3rd
// candidate, right Shift took the 2nd).
//
// Fix: both ends call the same pair of pure functions, so a one-sided
// change of the convention can no longer happen silently.
inline std::uint32_t EncodeShiftIsLeft(bool is_left) {
  return is_left ? 0u : 1u;
}

inline bool DecodeShiftIsLeft(std::uint32_t wparam) {
  return (wparam & 1u) == 0u;
}

// ADR 0017 D2: the user can tell the IME that the host reports the two
// shift keys the other way round.
//
// On 2026-10-29 the physical left Shift selected the 3rd candidate and the
// right one the 2nd (spec 087 A.1). Every link of the chain was proven
// correct (spec 087 A.7), so the inversion has to come from the (vkey,
// scan code) pair the host hands to the IME, or from the driver below it.
// A driver level swap is invisible to software, so the switch is the only
// remedy in that case; it is a plain configuration flag, which keeps the
// decision a pure function.
inline bool ApplyShiftSideSwap(bool is_left, bool swap) {
  return swap ? !is_left : is_left;
}

// Pure, dependency-free Shift state machine.
// Extracted from WeaselServer / RimeWithWeaselHandler so that 11 unit tests
// can drive it without IPC / Rime / GUI surfaces.
//
// Semantics (per spec 077 §2.1, mirrored in test/TestShiftIPCStateMachine.cpp):
// - Down then Up with no intervening key => fire select(index=L?1:R?2:0).
// - Intervening non-Shift key between Down and Up => no fire.
// - Up without prior Down => no fire.
// - Repeated Down overrides lastIsLeft; final Up is what is observed.
// - Reset(sid) wipes per-session state; OnSessionDestroyed erases the map
// entry.
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
    if (it == states_.end())
      return {};
    EventResult r;
    r.fire_select = it->second.downRecorded && !it->second.interveningKey;
    r.index = it->second.lastIsLeft ? 1u : 2u;
    states_[sid] = State{};  // C6: unconditionally reset on Up
    return r;
  }

  void OnInterveningKey(SessionId sid) {
    auto it = states_.find(sid);
    if (it != states_.end() && it->second.downRecorded) {
      it->second.interveningKey = true;
    }
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

// ===== ADR 0010: bare Shift disambiguation policy =====
//
// A clean Shift release (Down -> Up with no intervening key) carries two
// possible intents:
//   - spec 077: select the 2nd / 3rd candidate (left / right Shift)
//   - ADR 0010: toggle the Chinese / English input mode
//
// They are separated by a single observable predicate -- whether the session
// currently shows a candidate menu. There is no timing heuristic and no
// double-press escape hatch (ADR 0010 P3).
//
// Kept dependency-free so that the decision table can be unit-tested without
// rime_api / IPC / GUI surfaces.
enum class ShiftUpAction {
  kNone,             // not a bare Shift (no Down, or an intervening key)
  kSelectCandidate,  // a candidate menu is open -> spec 077 behaviour
  kToggleAsciiMode,  // no menu -> ADR 0010 behaviour
};

inline ShiftUpAction DecideShiftUpAction(bool down_recorded,
                                         bool intervening_key,
                                         bool menu_open,
                                         bool feature_enabled) {
  if (!down_recorded || intervening_key)
    return ShiftUpAction::kNone;
  if (!feature_enabled)
    return ShiftUpAction::kSelectCandidate;
  if (menu_open)
    return ShiftUpAction::kSelectCandidate;
  return ShiftUpAction::kToggleAsciiMode;
}

// ===== ADR 0012: ascii_mode propagation scope =====
//
// Which sessions an ascii_mode change may reach.
//
// - A bare Shift (ADR 0010) is a per-window gesture: it must never switch a
//   second window.
// - An explicit control (the tray or the language bar menu) may reach every
//   session, but only when weasel.yaml asks for it with "global_ascii: true".
//
// Kept dependency-free so that the decision table can be unit-tested without
// rime_api / IPC / GUI surfaces.
enum class AsciiModeScope {
  kCurrentSession,
  kAllSessions,
};

inline AsciiModeScope DecideAsciiModeScope(bool explicit_control,
                                           bool global_ascii_configured) {
  if (!explicit_control)
    return AsciiModeScope::kCurrentSession;
  if (!global_ascii_configured)
    return AsciiModeScope::kCurrentSession;
  return AsciiModeScope::kAllSessions;
}
}  // namespace fluxing