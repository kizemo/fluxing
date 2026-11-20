#pragma once
//
// ADR 0066 - account window slot state machine (pure, ASCII-only).
//
// The account dialog is a bare Win32 top-level window kept in a process-wide
// slot table so that a second menu entry reuses the same HWND. A stale slot
// (window destroyed but the slot still points at the old handle) makes the
// second entry reuse a half-dead window whose close button no longer works.
//
// Pure logic lives here so production code and unit tests share one source of
// truth (constitution R7). No windows.h dependency on purpose: this header
// must be includable from any translation unit.
//

#include <cstdint>

namespace fluxing {
namespace account_window {

// State of one role slot in the process-wide window table.
enum class SlotState {
  kEmpty,  // no handle stored
  kStale,  // a handle is stored but the window is gone or unresponsive
  kLive,   // the handle refers to an interactive window
};

// Classify a slot from the three facts the production code can observe.
//   has_handle : the slot holds a handle
//   is_window  : IsWindow(handle) is true
//   responsive : the window answered a WM_NULL probe within the timeout
inline SlotState ClassifySlot(bool has_handle,
                              bool is_window,
                              bool responsive) {
  if (!has_handle) {
    return SlotState::kEmpty;
  }
  if (!is_window || !responsive) {
    return SlotState::kStale;
  }
  return SlotState::kLive;
}

// Only a confirmed-live slot may be reused.
inline bool SlotReusable(SlotState state) {
  return state == SlotState::kLive;
}

// The slot must be cleared BEFORE the window is unregistered, otherwise a
// blocking unregister can leave the slot pointing at a half-dead window.
inline bool MustClearSlotBeforeUnregister(SlotState state) {
  return state != SlotState::kEmpty;
}

// Probe timeout (milliseconds) for the WM_NULL liveness probe. The production
// code uses this exact value so tests and production agree on the threshold.
constexpr unsigned int kSlotProbeTimeoutMs = 200;

}  // namespace account_window
}  // namespace fluxing
