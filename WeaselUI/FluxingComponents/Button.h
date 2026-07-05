// Button.h - spec 037 T007 (2026-07-05)
//
// mac-style rounded button. Renders a 6px-radius rounded
// rectangle with a centered label, subscribed to FluxingTheme
// for dark/light palette changes. Three visual styles:
//   * Primary       - filled with palette.hilited_back, white text
//   * Secondary     - filled with palette.back, text color
//   * Destructive   - filled with red (#D04545), white text
//
// Owns its own child HWND; SetOnClick registers a callback
// fired on WM_LBUTTONUP. spec 037 R3: WM_LBUTTONUP is fired on
// mouse release, not WM_LBUTTONDOWN, so the click is only
// registered after the user releases the button over the
// control (matches macOS button behavior).
//
// Out of scope for spec 037: hover/press visual states, focus
// rings, keyboard accelerators, mnemonics. spec 039+.

#pragma once

#include "stdafx.h"
#include "D2DRenderer.h"
#include "FluxingTheme.h"

namespace fluxing {
namespace ui {

class FluxingButton {
 public:
  enum class Style { Primary, Secondary, Destructive };

  // Static factory: creates a child HWND under hwndParent, with
  // the given bounding rect, label, and style. Returns a
  // unique_ptr (nullptr on failure). The control's lifetime is
  // tied to the unique_ptr; Destroy() must be called before
  // destruction (or simply let the destructor run, which calls
  // Destroy + DestroyWindow).
  static std::unique_ptr<FluxingButton> Create(
      HWND hwndParent, RECT const& rect, std::wstring const& label,
      Style style);

  // Destructor: destroys the child HWND and releases the cached
  // render target. Safe to call from WM_DESTROY of the parent.
  ~FluxingButton();

  // Set the click callback. Fires on every WM_LBUTTONUP that
  // occurs within the control's bounding rect.
  void SetOnClick(std::function<void()> cb);

  // Update the label text. Triggers a redraw.
  void SetLabel(std::wstring const& label);

  // Manual Destroy (called by destructor; also callable directly
  // to release the HWND before the unique_ptr goes out of scope).
  void Destroy();

  // Accessors (test-only and for spec 038 use).
  HWND Hwnd() const { return hwnd_; }
  std::wstring const& Label() const { return label_; }
  Style GetStyle() const { return style_; }

 private:
  FluxingButton() = default;
  FluxingButton(const FluxingButton&) = delete;
  FluxingButton& operator=(const FluxingButton&) = delete;

  // WndProc (static, forwards to the FluxingButton instance).
 public:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // Actual message handlers.
  LRESULT HandlePaint();
  LRESULT HandleLButtonUp();
  LRESULT HandleDestroy();

  // Subscribe to the theme and cache our handle. Called by
  // Create().
  void SubscribeTheme();

  HWND hwnd_ = nullptr;
  std::wstring label_;
  Style style_ = Style::Primary;
  std::function<void()> on_click_;
  ::fluxing::ui::ThemeSubscriptionHandle theme_handle_ = 0;
};

}  // namespace ui
}  // namespace fluxing