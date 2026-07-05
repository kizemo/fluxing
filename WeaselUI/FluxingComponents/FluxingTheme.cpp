// FluxingTheme.cpp - spec 037 T006 (2026-07-05)
//
// Implementation of the FluxingTheme singleton adapter.
// Subscribes to fluxing::FluxingDarkModeBridge in the
// constructor; on bridge callback, fans out to all theme
// subscribers with the new palette payload.
//
// Snapshot iteration: NotifyAll() copies the callback list under
// the mutex, then fires callbacks outside the lock. This matches
// the bridge's own pattern (L26 / spec 033 T006 anti-pattern
// AP-033-C) so a callback that Subscribes / Unsubscribes does
// not invalidate iteration.

#include "stdafx.h"
#include "FluxingTheme.h"

#include <mutex>
#include <utility>
#include <vector>

namespace fluxing {
namespace ui {

struct FluxingTheme::Impl {
  std::mutex mu;
  std::vector<std::pair<ThemeSubscriptionHandle, ThemeCallback>>
      subscribers;
  ThemeSubscriptionHandle next_handle = 1;
};

FluxingTheme::FluxingTheme() : impl_(new Impl()) {
  // Subscribe to the bridge. The callback fires on every
  // bridge Refresh() that flips dark/light.
  bridge_handle_ = ::fluxing::FluxingDarkModeBridge::Get()->Subscribe(
      [this](bool is_dark) {
        // Re-fetch the palette (the bridge told us the new
        // bool, but the palette struct lives in the bridge).
        NotifyAll(::fluxing::FluxingDarkModeBridge::Get()
                      ->CurrentPalette());
      });
}

FluxingTheme::~FluxingTheme() {
  // Unsubscribe from the bridge. The bridge's Unsubscribe is
  // a no-op if the handle is unknown, so double-unsubscribe
  // is safe.
  if (bridge_handle_ != 0) {
    ::fluxing::FluxingDarkModeBridge::Get()->Unsubscribe(bridge_handle_);
    bridge_handle_ = 0;
  }
  delete impl_;
  impl_ = nullptr;
}

FluxingTheme& FluxingTheme::Instance() {
  static FluxingTheme instance;
  return instance;
}

::fluxing::Palette FluxingTheme::CurrentPalette() const {
  return ::fluxing::FluxingDarkModeBridge::Get()->CurrentPalette();
}

ThemeSubscriptionHandle FluxingTheme::Subscribe(ThemeCallback cb) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  ThemeSubscriptionHandle h = impl_->next_handle++;
  impl_->subscribers.emplace_back(h, std::move(cb));
  return h;
}

void FluxingTheme::Unsubscribe(ThemeSubscriptionHandle handle) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  for (auto it = impl_->subscribers.begin();
       it != impl_->subscribers.end(); ++it) {
    if (it->first == handle) {
      impl_->subscribers.erase(it);
      return;
    }
  }
  // Unknown handle: no-op (spec 037 T6 idempotence).
}

void FluxingTheme::NotifyAll(::fluxing::Palette const& new_palette) {
  std::vector<ThemeCallback> snapshot;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    snapshot.reserve(impl_->subscribers.size());
    for (const auto& pair : impl_->subscribers) {
      snapshot.push_back(pair.second);
    }
  }  // release mu_ before invoking callbacks
  for (auto& cb : snapshot) {
    cb(new_palette);
  }
}

}  // namespace ui
}  // namespace fluxing