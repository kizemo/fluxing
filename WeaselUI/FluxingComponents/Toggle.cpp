// Toggle.cpp - spec 037 T010 (2026-07-05)
//
// Implementation of FluxingToggle. The 200ms slide animation
// uses SetTimer(hwnd, 1, 10, NULL) and stops the timer when
// progress reaches 0.0 (off) or 1.0 (on). progress is
// interpolated linearly from the start state to the target
// state at each tick.

#include "stdafx.h"

#include "Toggle.h"

namespace fluxing {
namespace ui {

namespace {

const wchar_t* kToggleClassName = L"FluxingToggleClass";

void RegisterToggleClass(HINSTANCE hInst) {
  static bool registered = false;
  if (registered) return;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &FluxingToggle::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kToggleClassName;
  RegisterClassExW(&wc);  // ERROR_CLASS_ALREADY_EXISTS tolerated
  registered = true;
}

}  // namespace

std::unique_ptr<FluxingToggle> FluxingToggle::Create(
    HWND hwndParent, RECT const& rect, bool initial) {
  if (!hwndParent) return nullptr;
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hwndParent, GWLP_HINSTANCE));
  if (!hInst) hInst = GetModuleHandle(nullptr);
  RegisterToggleClass(hInst);

  auto tgl = std::unique_ptr<FluxingToggle>(new FluxingToggle());
  tgl->is_on_ = initial;
  tgl->progress_ = initial ? 1.0f : 0.0f;

  HWND hwnd = CreateWindowExW(
      0, kToggleClassName, L"",
      WS_CHILD | WS_VISIBLE,
      rect.left, rect.top,
      rect.right - rect.left, rect.bottom - rect.top,
      hwndParent, nullptr, hInst, tgl.get());
  if (!hwnd) {
    return nullptr;
  }
  tgl->hwnd_ = hwnd;
  tgl->SubscribeTheme();
  return tgl;
}

FluxingToggle::~FluxingToggle() { Destroy(); }

void FluxingToggle::Destroy() {
  if (timer_id_ != 0) {
    KillTimer(hwnd_, timer_id_);
    timer_id_ = 0;
    animating_ = false;
  }
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

void FluxingToggle::SetOn(bool on) {
  if (is_on_ == on) return;
  is_on_ = on;
  if (on_changed_) {
    on_changed_(on);
  }
  // Start the 200ms slide animation (10ms ticks, 20 ticks).
  if (timer_id_ != 0) KillTimer(hwnd_, timer_id_);
  animating_ = true;
  timer_id_ = SetTimer(hwnd_, 1, 10, nullptr);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void FluxingToggle::SetOnChanged(std::function<void(bool)> cb) {
  on_changed_ = std::move(cb);
}

void FluxingToggle::SubscribeTheme() {
  theme_handle_ = FluxingTheme::Instance().Subscribe(
      [this](::fluxing::Palette const&) {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
      });
}

LRESULT CALLBACK FluxingToggle::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam) {
  FluxingToggle* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<FluxingToggle*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<FluxingToggle*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }
  if (!self || self->hwnd_ != hwnd) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  switch (msg) {
    case WM_PAINT: return self->HandlePaint();
    case WM_LBUTTONUP: return self->HandleLButtonUp();
    case WM_TIMER: return self->HandleTimer(wParam);
    case WM_DESTROY: return self->HandleDestroy();
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

LRESULT FluxingToggle::HandlePaint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  if (!hdc) return 0;
  auto rt =
      FluxingD2DRenderer::Instance().CreateHwndRenderTarget(hwnd_);
  if (!rt) { EndPaint(hwnd_, &ps); return 0; }
  rt->BeginDraw();

  auto pal = FluxingTheme::Instance().CurrentPalette();
  // Track color: hilited_back when on, mid-gray when off.
  // Use palette.hilited_back at progress and a computed
  // neutral color at progress=0.
  DWORD track_off = 0x808080;  // mid-gray (not theme-aware)
  DWORD track_color = static_cast<DWORD>(
      (1.0f - progress_) * track_off + progress_ * pal.hilited_back);

  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
  rt->CreateSolidColorBrush(
      D2D1::ColorF(track_color, 1.0f), &brush);

  RECT rc;
  GetClientRect(hwnd_, &rc);
  FLOAT h = static_cast<FLOAT>(rc.bottom - rc.top);
  D2D1_ROUNDED_RECT track;
  track.rect = D2D1::RectF(0.0f, 0.0f,
                            static_cast<FLOAT>(rc.right - rc.left), h);
  track.radiusX = h * 0.5f;
  track.radiusY = h * 0.5f;
  rt->FillRoundedRectangle(track, brush.Get());

  // Knob: circle at progress position.
  FLOAT knob_r = h * 0.4f;
  FLOAT cx = knob_r + progress_ * (static_cast<FLOAT>(rc.right - rc.left)
                                   - 2.0f * knob_r);
  FLOAT cy = h * 0.5f;
  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> knob_brush;
  rt->CreateSolidColorBrush(
      D2D1::ColorF(0xFFFFFF, 1.0f), &knob_brush);
  rt->FillEllipse(
      D2D1::Ellipse(D2D1::Point2F(cx, cy), knob_r, knob_r),
      knob_brush.Get());

  HRESULT hr = rt->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  }
  EndPaint(hwnd_, &ps);
  return 0;
}

LRESULT FluxingToggle::HandleLButtonUp() {
  SetOn(!is_on_);
  return 0;
}

LRESULT FluxingToggle::HandleTimer(WPARAM timer_id) {
  if (timer_id != 1 || !animating_) return 0;
  // 20 ticks total, 10ms each = 200ms.
  const float kStep = 1.0f / 20.0f;
  if (is_on_) {
    progress_ += kStep;
    if (progress_ >= 1.0f) {
      progress_ = 1.0f;
      animating_ = false;
      KillTimer(hwnd_, timer_id_);
      timer_id_ = 0;
    }
  } else {
    progress_ -= kStep;
    if (progress_ <= 0.0f) {
      progress_ = 0.0f;
      animating_ = false;
      KillTimer(hwnd_, timer_id_);
      timer_id_ = 0;
    }
  }
  InvalidateRect(hwnd_, nullptr, FALSE);
  return 0;
}

LRESULT FluxingToggle::HandleDestroy() {
  if (timer_id_ != 0) {
    KillTimer(hwnd_, timer_id_);
    timer_id_ = 0;
  }
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  return 0;
}

}  // namespace ui
}  // namespace fluxing