// Label.h - spec 037 T012 (2026-07-05)
//
// Single-line text label rendered with DirectWrite. Three font
// sizes (13/15/17 pt). Text color follows palette.text.

#pragma once

#include "stdafx.h"
#include "D2DRenderer.h"
#include "FluxingTheme.h"

namespace fluxing {
namespace ui {

class FluxingLabel {
 public:
  enum class FontSize { Small, Medium, Large };

  static std::unique_ptr<FluxingLabel> Create(
      HWND hwndParent, RECT const& rect, std::wstring const& text,
      FontSize size);

  ~FluxingLabel();

  void SetText(std::wstring const& text);
  void Destroy();

  // Test-only accessors.
  HWND Hwnd() const { return hwnd_; }
  std::wstring const& GetText() const { return text_; }
  FontSize GetFontSize() const { return size_; }

 private:
  FluxingLabel() = default;
  FluxingLabel(const FluxingLabel&) = delete;
  FluxingLabel& operator=(const FluxingLabel&) = delete;

 public:
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
  LRESULT HandlePaint();
  LRESULT HandleDestroy();
  void SubscribeTheme();

  static float FontSizeToPt(FontSize s) {
    switch (s) {
      case FontSize::Small:  return 13.0f;
      case FontSize::Medium: return 15.0f;
      case FontSize::Large:  return 17.0f;
    }
    return 13.0f;
  }

  HWND hwnd_ = nullptr;
  std::wstring text_;
  FontSize size_ = FontSize::Small;
  ::fluxing::ui::ThemeSubscriptionHandle theme_handle_ = 0;
};

}  // namespace ui
}  // namespace fluxing