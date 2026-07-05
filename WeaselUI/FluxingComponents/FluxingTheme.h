// FluxingTheme.h - spec 037 T005 (2026-07-05)
//
// Singleton adapter that wraps fluxing::FluxingDarkModeBridge
// for the FluxingComponents UI. Exposes a Subscribe() API
// mirroring the bridge's API but with the new Palette payload
// (rather than just a bool), so control subscribers can redraw
// directly with the new palette without an extra
// CurrentPalette() call.
//
// API summary:
//   auto& theme = fluxing::ui::FluxingTheme::Instance();
//   auto pal = theme.CurrentPalette();
//   auto h = theme.Subscribe([](Palette const& p) { redraw(p); });
//   theme.Unsubscribe(h);
//
// Out of scope for this header: animated transitions. spec 039
// will extend the callback to (old_palette, new_palette, progress)
// for the 200ms gradient. For now, controls invalidate on every
// callback (which is fine for v0 since dark-mode toggles are
// rare events).

#pragma once

#include "stdafx.h"
#include "D2DRenderer.h"

namespace fluxing {
namespace ui {

// Callback signature: receives the new palette. spec 039 will
// add old_palette + progress for animation; v0 is single-shot.
using ThemeCallback = std::function<void(::fluxing::Palette const&)>;

// Opaque subscription handle. Monotonically increasing.
using ThemeSubscriptionHandle = std::uint64_t;

class FluxingTheme {
 public:
  // Process-singleton. C++17 thread-safe init. Lifetime is
  // process-lifetime.
  static FluxingTheme& Instance();

  // Wait-free read of the current palette. Returns the bridge's
  // CurrentPalette() (i.e. the cached dark/light palette).
  ::fluxing::Palette CurrentPalette() const;

  // Register a callback. The callback fires on every subsequent
  // bridge Refresh() that changes the dark state (i.e. not on
  // no-op refreshes). Multiple subscribers may register; they
  // fire in registration order (FIFO). Returns a handle to
  // pass to Unsubscribe().
  ThemeSubscriptionHandle Subscribe(ThemeCallback cb);

  // Remove a previously-subscribed callback. Safe to call from
  // within a callback. No-op if handle is unknown.
  void Unsubscribe(ThemeSubscriptionHandle handle);

 private:
  FluxingTheme();
  ~FluxingTheme();
  FluxingTheme(const FluxingTheme&) = delete;
  FluxingTheme& operator=(const FluxingTheme&) = delete;

  // Internal: called by the bridge callback. Notifies all
  // theme subscribers (synchronously, on the same thread that
  // called bridge->Refresh()).
  void NotifyAll(::fluxing::Palette const& new_palette);

  // The subscription handle we got from the bridge. Stored so
  // we can unsubscribe in the destructor.
  ::fluxing::SubscriptionHandle bridge_handle_ = 0;

  struct Impl;
  Impl* impl_;
};

}  // namespace ui
}  // namespace fluxing