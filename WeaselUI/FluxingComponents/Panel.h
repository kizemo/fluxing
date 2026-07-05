// Panel.h - spec 037 T011 (2026-07-05)
//
// Rounded-rectangle container. Two styles:
//   * Card  - 8px corner radius, filled with palette.back
//   * Plain - no rounding, no fill (transparent)
//
// The panel is a passive container; it does not handle
// mouse/keyboard input. It serves as a background and clipping
// region for child FluxingButton / FluxingToggle / FluxingLabel
// controls.

#pragma once

#include "stdafx.h"
#include "D2DRenderer.h"
#include "FluxingTheme.h"

namespace fluxing {
namespace ui {

class FluxingPanel {
 public:
  enum class Style { Card, Plain };

  // Corner radius accessor (test-only).
  static constexpr int kCardRadius = 8;

  static std::unique_ptr<FluxingPanel> Create(
      HWND hwndParent, RECT const& rect, Style style);

  ~FluxingPanel();

  void Destroy();

  // Test-only accessors.
  HWND Hwnd() const { return hwnd_; }
  Style GetStyle() const { return style_; }

 private:
  FluxingPanel() = default;
  FluxingPanel(const FluxingPanel&) = delete;
  FluxingPanel& operator=(const FluxingPanel&) = delete;

 public:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT HandlePaint();
  LRESULT HandleDestroy();
  void SubscribeTheme();

  HWND hwnd_ = nullptr;
  Style style_ = Style::Card;
  ::fluxing::ui::ThemeSubscriptionHandle theme_handle_ = 0;
};

}  // namespace ui
}  // namespace fluxing