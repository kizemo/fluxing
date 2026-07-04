// FluxingDarkModeBridge.cpp - spec 033 T002 (2026-07-04)
//
// spec 036: include stdafx.h for WeaselUI vcxproj PCH (was missing in spec 033 release).
#include "stdafx.h"
#include "FluxingDarkModeBridge.h"
//
// Implementation of the F11 dark-mode bridge. See header for
// contract. Key invariants:
//   * ReadAppsUseLightTheme uses KEY_WOW64_64KEY so a 32-bit
//     process on x64 Windows sees the 64-bit registry view
//     (AP-033-B: without it, AppsUseLightTheme may be missing).
//   * Refresh() iterates subscribers on a snapshot copy so
//     callbacks can Subscribe / Unsubscribe without UB
//     (AP-033-C).
//   * The singleton is a function-local static with C++17
//     thread-safe init (AP-033-D).
//   * Palette constants are byte-equal to the original inline
//     values in WeaselUI/WeaselPanel.cpp::_RefreshStylePalette
//     (line 1460, before spec 033 T004 refactor).

#include "FluxingDarkModeBridge.h"

// Uncomment to force-dark for visual debugging only.
// #define FLUXING_FORCE_DARK_FOR_DEBUG 1

namespace fluxing {

namespace {

// Byte-equal to the inline values in WeaselPanel.cpp (pre-033).
const Palette kPaletteDark  = {0x1E1E1E, 0xE0E0E0, 0x2D2D30, 0xFFFFFF};
const Palette kPaletteLight = {0xF0F0F0, 0x000000, 0xD0D0D0, 0x000080};

}  // namespace

FluxingDarkModeBridge::FluxingDarkModeBridge() {
  // Initial state from registry (best-effort; defaults to light).
  current_dark_ = ReadAppsUseLightTheme();
}

FluxingDarkModeBridge* FluxingDarkModeBridge::Get() {
  // C++17 guarantees thread-safe initialization of function-local
  // statics. No explicit mutex needed.
  static FluxingDarkModeBridge instance;
  return &instance;
}

bool FluxingDarkModeBridge::IsDarkMode() const {
  // Wait-free read. current_dark_ is only mutated under mu_ in
  // Refresh(), but a stale read is acceptable: the caller can
  // call Refresh() to force a fresh read.
  return current_dark_;
}

bool FluxingDarkModeBridge::ReadAppsUseLightTheme() const {
  HKEY h_key = nullptr;
  // KEY_WOW64_64KEY: required so a 32-bit process on x64 Windows
  // sees the 64-bit registry view. Without this flag, the
  // AppsUseLightTheme value (written by Windows to the 64-bit
  // view) is not visible.
  LONG rc = RegOpenKeyExW(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &h_key);
  if (rc != ERROR_SUCCESS) {
    return false;  // default light on any error
  }
  DWORD value = 1;  // default = light (1)
  DWORD size = sizeof(value);
  rc = RegQueryValueExW(h_key, L"AppsUseLightTheme", nullptr, nullptr,
                        reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(h_key);
  if (rc != ERROR_SUCCESS) {
    return false;  // default light on any error
  }
  return value == 0;  // 0 = dark, 1 = light
}

bool FluxingDarkModeBridge::Refresh() {
  bool new_dark;
#ifdef FLUXING_FORCE_DARK_FOR_DEBUG
  new_dark = true;
#else
  new_dark = ReadAppsUseLightTheme();
#endif
  bool changed;
  std::vector<DarkModeCallback> snapshot;
  {
    std::lock_guard<std::mutex> lock(mu_);
    changed = (new_dark != current_dark_);
    current_dark_ = new_dark;
    if (changed) {
      // Snapshot the callbacks so a callback that calls
      // Subscribe / Unsubscribe does not invalidate the
      // iteration. (AP-033-C: do not subscribe inside a callback.)
      snapshot.reserve(subscribers_.size());
      for (const auto& pair : subscribers_) {
        snapshot.push_back(pair.second);
      }
    }
  }  // release mu_ before invoking callbacks
  if (changed) {
    for (auto& cb : snapshot) {
      cb(new_dark);
    }
  }
  return changed;
}

SubscriptionHandle FluxingDarkModeBridge::Subscribe(DarkModeCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  SubscriptionHandle h = next_handle_++;
  subscribers_.emplace_back(h, std::move(cb));
  return h;
}

void FluxingDarkModeBridge::Unsubscribe(SubscriptionHandle handle) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto it = subscribers_.begin(); it != subscribers_.end(); ++it) {
    if (it->first == handle) {
      subscribers_.erase(it);
      return;
    }
  }
  // Unknown handle: no-op (not an error; spec 033 T007-T3 covers
  // duplicate Unsubscribe as a no-op).
}

Palette FluxingDarkModeBridge::CurrentPalette() const {
  // Wait-free read of the bool, then branch.
  if (current_dark_) {
    return kPaletteDark;
  } else {
    return kPaletteLight;
  }
}

void FluxingDarkModeBridge::SetDarkForTest(bool dark) {
  // TEST-ONLY: bypass registry read to inject a known state.
  // Production code MUST NOT call this. See header comment.
  std::lock_guard<std::mutex> lock(mu_);
  current_dark_ = dark;
}

}  // namespace fluxing
