// FluxingDarkModeBridge.h - spec 033 T001 (2026-07-04)
//
// Extracted from WeaselUI/WeaselPanel.cpp::OnSettingChange +
// _RefreshStylePalette (the inline dark-mode detection + palette
// refresh). Lifted into a standalone module so it can be linked
// into any panel (candidate list, tray menu, config UI, phrases
// list) without dragging in WTL/ATL/Gdiplus, and can be
// behavior-level tested without mirroring the production code.
//
// API summary:
//   auto* bridge = fluxing::FluxingDarkModeBridge::Get();
//   bool dark = bridge->IsDarkMode();
//   auto handle = bridge->Subscribe([](bool new_dark) { ... });
//   bridge->Unsubscribe(handle);
//   bool changed = bridge->Refresh();  // re-reads HKCU, fires callbacks
//   auto pal = bridge->CurrentPalette();
//
// Thread-safety: Get() returns a process-singleton; Subscribe /
// Unsubscribe / Refresh are thread-safe (mu_ guards subscribers_).
// IsDarkMode / CurrentPalette are wait-free (read of bool / struct).
//
// Out of scope for this header: the subscriber payload (Palette +
// bool is_dark) is sufficient for v2 panel needs; future spec 036
// may extend to a richer event struct if needed.

#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>
#include <windows.h>

namespace fluxing {

// 4-color palette matching the inline values previously hardcoded
// in WeaselUI/WeaselPanel.cpp::_RefreshStylePalette (line 1460).
// Byte-equal to the original (verified by visual diff during
// spec 033 authoring).
struct Palette {
  DWORD back;            // panel background
  DWORD text;            // primary text
  DWORD hilited_back;    // highlighted (selected) candidate background
  DWORD hilited_text;    // highlighted candidate text
};
// Compile-time invariant: Palette 4×DWORD = 16 bytes. Any future field
// added (e.g. spec 040 10 new color tokens) MUST update this assert so the
// ABI break is caught at build, not via silent sizeof mismatch.
static_assert(sizeof(Palette) == 16,
              "Palette size changed — update ABI & dependencies (L100)");


// Subscriber callback signature: receives the new dark state.
// The bridge guarantees the callback fires exactly once per
// Refresh() call (per spec 033 T007-T2). The callback runs on
// the same thread that called Refresh(); the bridge does not
// dispatch to a UI thread.
using DarkModeCallback = std::function<void(bool is_dark)>;

// Opaque subscription handle returned by Subscribe(). Pass to
// Unsubscribe() to remove. Handles are monotonically increasing
// uint64_t values (1, 2, 3, ...); they are never reused during
// the process lifetime.
using SubscriptionHandle = std::uint64_t;

class FluxingDarkModeBridge {
 public:
  // Process-singleton. C++17 thread-safe initialization. The
  // first caller constructs the bridge; subsequent callers
  // receive the same pointer. Lifetime is process-lifetime
  // (function-local static, never destroyed).
  static FluxingDarkModeBridge* Get();

  // Read the current cached dark-mode state. Wait-free.
  // Returns the most recent value from Refresh() (or the
  // initial registry read at first construction).
  bool IsDarkMode() const;

  // Re-read the Windows registry (HKCU\...\Themes\Personalize
  // \AppsUseLightTheme) and, if the new value differs from the
  // cached value, fire all subscribers. Returns true if the
  // state changed, false if it was already at the new value.
  // Thread-safe: subscriber list is iterated under a stable
  // snapshot to avoid reentrancy bugs if a callback calls
  // Subscribe/Unsubscribe (per spec 033 T006 anti-pattern AP-033-C).
  bool Refresh();

  // Register a callback. The callback fires on every subsequent
  // Refresh() call that changes the state (i.e. the same state
  // is not re-broadcast). Multiple subscribers may register;
  // they fire in registration order (FIFO). Returns an opaque
  // handle to pass to Unsubscribe().
  SubscriptionHandle Subscribe(DarkModeCallback cb);

  // Remove a previously-subscribed callback. Safe to call from
  // within a callback (the snapshot iteration in Refresh() does
  // not see pending removals; the removal takes effect on the
  // next Refresh()). No-op if handle is unknown.
  void Unsubscribe(SubscriptionHandle handle);

  // Return the palette for the current cached state. Wait-free.
  Palette CurrentPalette() const;

  // TEST-ONLY: override the cached state without touching the
  // registry. The next Refresh() re-reads the registry and
  // overwrites this value; subscribers fire if the value changes
  // back. Production code MUST NOT call this method.
  void SetDarkForTest(bool dark);

 private:
  FluxingDarkModeBridge();

  // Disable copy / move. Singleton.
  FluxingDarkModeBridge(const FluxingDarkModeBridge&) = delete;
  FluxingDarkModeBridge& operator=(const FluxingDarkModeBridge&) = delete;

  // Read HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\
  // Personalize\AppsUseLightTheme. Returns true if dark (value
  // == 0), false if light (value != 0). Returns false (light)
  // on any error (key missing, permission denied, etc.) so the
  // UI defaults to the historically-safe light palette.
  bool ReadAppsUseLightTheme() const;

  mutable std::mutex mu_;
  std::vector<std::pair<SubscriptionHandle, DarkModeCallback>> subscribers_;
  SubscriptionHandle next_handle_ = 1;
  bool current_dark_ = false;
};

}  // namespace fluxing
