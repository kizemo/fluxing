// Panel.cpp - spec 037 T011 (2026-07-05)
//
// Implementation of FluxingPanel. Card style draws a filled
// rounded rectangle in the current palette.back color; Plain
// style is a no-op (the HWND is transparent so the parent
// shows through).

#include "stdafx.h"

#include "Panel.h"

namespace fluxing {
namespace ui {

namespace {

const wchar_t* kPanelClassName = L"FluxingPanelClass";

void RegisterPanelClass(HINSTANCE hInst) {
  static bool registered = false;
  if (registered) return;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &FluxingPanel::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kPanelClassName;
  RegisterClassExW(&wc);
  registered = true;
}

}  // namespace

std::unique_ptr<FluxingPanel> FluxingPanel::Create(
    HWND hwndParent, RECT const& rect, Style style) {
  if (!hwndParent) return nullptr;
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hwndParent, GWLP_HINSTANCE));
  if (!hInst) hInst = GetModuleHandle(nullptr);
  RegisterPanelClass(hInst);

  auto panel = std::unique_ptr<FluxingPanel>(new FluxingPanel());
  panel->style_ = style;

  // Plain panels get WS_EX_TRANSPARENT so mouse events pass through
  // to siblings/parents. Card panels do not (they may want
  // hit-testing in spec 040+ for drag).
  DWORD ex_style = 0;
  if (style == Style::Plain) {
    ex_style = WS_EX_TRANSPARENT;
  }

  HWND hwnd = CreateWindowExW(
      ex_style, kPanelClassName, L"",
      WS_CHILD | WS_VISIBLE,
      rect.left, rect.top,
      rect.right - rect.left, rect.bottom - rect.top,
      hwndParent, nullptr, hInst, panel.get());
  if (!hwnd) {
    return nullptr;
  }
  panel->hwnd_ = hwnd;
  panel->SubscribeTheme();
  return panel;
}

FluxingPanel::~FluxingPanel() { Destroy(); }

void FluxingPanel::Destroy() {
  if (theme_handle_ != 0) {
    FluxingTheme::Instance().Unsubscribe(theme_handle_);
    theme_handle_ = 0;
  }
  if (hwnd_) {
    HWND h = hwnd_;
    hwnd_ = nullptr;
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(h);
    DestroyWindow(h);
  }
}

void FluxingPanel::SubscribeTheme() {
  theme_handle_ = FluxingTheme::Instance().Subscribe(
      [this](::fluxing::Palette const&) {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
      });
}

LRESULT CALLBACK FluxingPanel::WndProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam) {
  FluxingPanel* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<FluxingPanel*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<FluxingPanel*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }
  if (!self || self->hwnd_ != hwnd) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  switch (msg) {
    case WM_PAINT: return self->HandlePaint();
    case WM_DESTROY: return self->HandleDestroy();
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

LRESULT FluxingPanel::HandlePaint() {
  if (style_ == Style::Plain) {
    // Nothing to draw; the HWND is transparent.
    ValidateRect(hwnd_, nullptr);
    return 0;
  }
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  if (!hdc) return 0;
  auto rt =
      FluxingD2DRenderer::Instance().CreateHwndRenderTarget(hwnd_);
  if (!rt) {
    // L50 fallback: D2D unavailable. Fall back to GDI FillRect with
    // palette.back so the card is still visually distinct from the
    // dialog background (COLOR_WINDOW+1). Without this fallback the user
    // sees a transparent region showing the dialog background through.
    auto pal = FluxingTheme::Instance().CurrentPalette();
    HBRUSH bg = CreateSolidBrush(pal.back);
    if (bg) {
      RECT rc;
      GetClientRect(hwnd_, &rc);
      FillRect(hdc, &rc, bg);
      DeleteObject(bg);
    }
    EndPaint(hwnd_, &ps);
    return 0;
  }
  rt->BeginDraw();

  auto pal = FluxingTheme::Instance().CurrentPalette();
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
  rt->CreateSolidColorBrush(
      D2D1::ColorF(pal.back, 1.0f), &brush);

  RECT rc;
  GetClientRect(hwnd_, &rc);
  D2D1_ROUNDED_RECT rr;
  rr.rect = D2D1::RectF(0.0f, 0.0f,
                         static_cast<FLOAT>(rc.right - rc.left),
                         static_cast<FLOAT>(rc.bottom - rc.top));
  rr.radiusX = static_cast<FLOAT>(kCardRadius);
  rr.radiusY = static_cast<FLOAT>(kCardRadius);
  rt->FillRoundedRectangle(rr, brush.Get());

  HRESULT hr = rt->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  }
  EndPaint(hwnd_, &ps);
  return 0;
}

LRESULT FluxingPanel::HandleDestroy() {
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  return 0;
}

}  // namespace ui
}  // namespace fluxing