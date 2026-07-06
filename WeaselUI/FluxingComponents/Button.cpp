// Button.cpp - spec 037 T008 (2026-07-05)
//
// Implementation of FluxingButton. Uses a static WndProc that
// dispatches to the per-instance handler via GWLP_USERDATA.
// Subscribe to FluxingTheme on Create; Unsubscribe on Destroy.

#include "stdafx.h"

#include "Button.h"

namespace fluxing {
namespace ui {

namespace {

// HWND class name (registered once per process).
const wchar_t* kButtonClassName = L"FluxingButtonClass";

// One-time class registration. Idempotent (RegisterClassEx
// returns 0 atom if the class is already registered, which
// we ignore).
void RegisterButtonClass(HINSTANCE hInst) {
  static bool registered = false;
  if (registered) return;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &FluxingButton::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kButtonClassName;
  if (!RegisterClassExW(&wc)) {
    DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) {
      // fatal: cannot register FluxingButton class. Subsequent
      // Create() calls will fail to create the HWND and return
      // nullptr.
    }
  }
  registered = true;
}

// Convert a Style + Palette into the (fill, text) color pair.
struct ButtonColors {
  DWORD fill;
  DWORD text;
};
ButtonColors ComputeColors(FluxingButton::Style style,
                          ::fluxing::Palette const& pal) {
  switch (style) {
    case FluxingButton::Style::Primary:
      return {pal.hilited_back, pal.hilited_text};
    case FluxingButton::Style::Secondary:
      return {pal.back, pal.text};
    case FluxingButton::Style::Destructive:
      // Hard-coded red for destructive actions; not theme-aware
      // because destructive actions must be visually distinct
      // across light and dark mode.
      return {0xD04545, 0xFFFFFF};
  }
  return {pal.back, pal.text};
}

}  // namespace

std::unique_ptr<FluxingButton> FluxingButton::Create(
    HWND hwndParent, RECT const& rect, std::wstring const& label,
    Style style) {
  if (!hwndParent) return nullptr;
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hwndParent, GWLP_HINSTANCE));
  if (!hInst) {
    hInst = GetModuleHandle(nullptr);
  }
  RegisterButtonClass(hInst);

  auto btn = std::unique_ptr<FluxingButton>(new FluxingButton());
  btn->label_ = label;
  btn->style_ = style;

  HWND hwnd = CreateWindowExW(
      0, kButtonClassName, label.c_str(),
      WS_CHILD | WS_VISIBLE,
      rect.left, rect.top,
      rect.right - rect.left, rect.bottom - rect.top,
      hwndParent, nullptr, hInst, btn.get());
  if (!hwnd) {
    return nullptr;
  }
  btn->hwnd_ = hwnd;
  btn->SubscribeTheme();
  return btn;
}

FluxingButton::~FluxingButton() {
  Destroy();
}

void FluxingButton::Destroy() {
  if (theme_handle_ != 0) {
    FluxingTheme::Instance().Unsubscribe(theme_handle_);
    theme_handle_ = 0;
  }
  if (hwnd_) {
    HWND h = hwnd_;
    hwnd_ = nullptr;
    // Release the cached render target BEFORE DestroyWindow
    // (the HWND is the map key; after DestroyWindow the key is
    // invalid).
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(h);
    DestroyWindow(h);
  }
}

void FluxingButton::SetOnClick(std::function<void()> cb) {
  on_click_ = std::move(cb);
}

void FluxingButton::SetLabel(std::wstring const& label) {
  label_ = label;
  if (hwnd_) {
    SetWindowTextW(hwnd_, label_.c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void FluxingButton::SubscribeTheme() {
  theme_handle_ = FluxingTheme::Instance().Subscribe(
      [this](::fluxing::Palette const&) {
        if (hwnd_) {
          InvalidateRect(hwnd_, nullptr, FALSE);
        }
      });
}

LRESULT CALLBACK FluxingButton::WndProc(HWND hwnd, UINT msg,
                                         WPARAM wParam, LPARAM lParam) {
  FluxingButton* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<FluxingButton*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<FluxingButton*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }
  if (!self || self->hwnd_ != hwnd) {
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  switch (msg) {
    case WM_PAINT: return self->HandlePaint();
    case WM_DPICHANGED: return self->HandleDpiChanged(wParam, lParam);
    case WM_LBUTTONUP: return self->HandleLButtonUp();
    case WM_DESTROY: return self->HandleDestroy();
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

LRESULT FluxingButton::HandlePaint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  if (!hdc) {
    return 0;
  }
  auto rt =
      FluxingD2DRenderer::Instance().CreateHwndRenderTarget(hwnd_);
  if (!rt) {
    // L50 fallback: D2D unavailable. Fall back to GDI rounded FillRect +
    // DrawText so the button is still visible (otherwise the user sees
    // a transparent region showing the dialog background through).
    auto pal = FluxingTheme::Instance().CurrentPalette();
    auto colors = ComputeColors(style_, pal);
    RECT rc;
    FluxingD2DRenderer::GetPhysicalClientRect(hwnd_, &rc);
    HBRUSH bg = CreateSolidBrush(colors.fill);
    if (bg) {
      FillRect(hdc, &rc, bg);
      DeleteObject(bg);
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colors.text);
    HFONT hf = nullptr;
    LOGFONTW lf = {};
    lf.lfHeight = -(LONG)(14.0f * 96.0f / 72.0f + 0.5f);
    lf.lfWeight = FW_NORMAL;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    hf = CreateFontIndirectW(&lf);
    HFONT old_hf = nullptr;
    if (hf) old_hf = (HFONT)SelectObject(hdc, hf);
    DrawTextW(hdc, label_.c_str(), (int)label_.size(), &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (hf) { SelectObject(hdc, old_hf); DeleteObject(hf); }
    EndPaint(hwnd_, &ps);
    return 0;
  }
  rt->BeginDraw();
  // spec 041 fix: clear to dialog window color (D2D backing store
  // defaults to opaque black, which would show as a black bar
  // where the button's rounded corners are transparent).
  rt->Clear(D2D1::ColorF(GetSysColor(COLOR_WINDOW), 1.0f));
  auto pal = FluxingTheme::Instance().CurrentPalette();
  auto colors = ComputeColors(style_, pal);

  Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
  rt->CreateSolidColorBrush(
      D2D1::ColorF(colors.fill, 1.0f), &brush);

  D2D1_ROUNDED_RECT rr;
  RECT rc;
  FluxingD2DRenderer::GetPhysicalClientRect(hwnd_, &rc);
  rr.rect = D2D1::RectF(
      static_cast<FLOAT>(rc.left), static_cast<FLOAT>(rc.top),
      static_cast<FLOAT>(rc.right), static_cast<FLOAT>(rc.bottom));
  rr.radiusX = 6.0f;
  rr.radiusY = 6.0f;
  rt->FillRoundedRectangle(rr, brush.Get());

  // Draw label using DirectWrite.
  Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
  if (SUCCEEDED(FluxingD2DRenderer::Instance().WriteFactory()
                    ->CreateTextFormat(
                        L"Segoe UI", nullptr,
                        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                        DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"zh-CN",
                        &fmt))) {
    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> text_brush;
    rt->CreateSolidColorBrush(
        D2D1::ColorF(colors.text, 1.0f), &text_brush);
    rt->DrawText(
        label_.c_str(), static_cast<UINT32>(label_.size()), fmt.Get(),
        D2D1::RectF(
            static_cast<FLOAT>(rc.left), static_cast<FLOAT>(rc.top),
            static_cast<FLOAT>(rc.right), static_cast<FLOAT>(rc.bottom)),
        text_brush.Get());
  }

  HRESULT hr = rt->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    // HWND render target was lost; force a fresh one on next
    // paint by releasing the cached one.
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  }
  EndPaint(hwnd_, &ps);
  return 0;
}

LRESULT FluxingButton::HandleLButtonUp() {
  if (on_click_) {
    on_click_();
  }
  return 0;
}

LRESULT FluxingButton::HandleDpiChanged(WPARAM, LPARAM lParam) {
  // spec 041 T006: on DPI change, release the cached backing store
  // (the old physical size is now wrong) and trigger a repaint so
  // the next WM_PAINT re-creates the rt at the new DPI. The new
  // window rect from lParam is already applied by Windows before
  // WM_DPICHANGED fires; we just need to invalidate.
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  InvalidateRect(hwnd_, nullptr, FALSE);
  return 0;
}
LRESULT FluxingButton::HandleDestroy() {
  // WM_DESTROY fires before the destructor; release the render
  // target here so the next paint (on a re-Create) starts clean.
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  return 0;
}

}  // namespace ui
}  // namespace fluxing
