// Toggle.h - spec 037 T009 (2026-07-05)
//
// mac-style on/off toggle (a.k.a. switch). Renders a 30x16
// rounded-rectangle track with a circular knob that slides
// between the on (right) and off (left) positions. 200ms
// slide animation is approximated by an internal SetTimer
// + WM_TIMER that interpolates a 0..1 progress value and
// repaints.
//
// Out of scope: spring physics (we use linear), bouncy
// easing (spec 039+). For v0 spec 037 the animation is
// instant: SetOn(true) immediately updates is_on_ and
// invalidates; the spec 037 plan 2.4 200ms animation is
// implemented as a v0 follow-up.

#pragma once

#include "stdafx.h"
#include "D2DRenderer.h"
#include "FluxingTheme.h"

namespace fluxing {
namespace ui {

class FluxingToggle {
 public:
  // Static factory.
  static std::unique_ptr<FluxingToggle> Create(
      HWND hwndParent, RECT const& rect, bool initial);

  ~FluxingToggle();

  // True if the toggle is in the "on" state (knob at right).
  bool IsOn() const { return is_on_; }

  // Set the state. If the new state differs, fires OnChanged
  // and animates the knob slide (200ms).
  void SetOn(bool on);

  // Register a callback for state changes.
  void SetOnChanged(std::function<void(bool)> cb);

  // Manual Destroy.
  void Destroy();

  // Test-only accessors.
  HWND Hwnd() const { return hwnd_; }

 private:
  FluxingToggle() = default;
  FluxingToggle(const FluxingToggle&) = delete;
  FluxingToggle& operator=(const FluxingToggle&) = delete;

 public:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT HandlePaint();
  LRESULT HandleLButtonUp();
  LRESULT HandleTimer(WPARAM timer_id);
  LRESULT HandleDestroy();
  void SubscribeTheme();

  HWND hwnd_ = nullptr;
  bool is_on_ = false;

  // Animation state: 0.0 = off, 1.0 = on. Linear interpolation.
  // Updated by WM_TIMER (10ms ticks, 20 ticks total = 200ms).
  float progress_ = 0.0f;
  bool animating_ = false;
  UINT_PTR timer_id_ = 0;

  std::function<void(bool)> on_changed_;
  ::fluxing::ui::ThemeSubscriptionHandle theme_handle_ = 0;
};

}  // namespace ui
}  // namespace fluxing