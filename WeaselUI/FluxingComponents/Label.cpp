// Label.cpp - spec 037 T012 (2026-07-05)
//
// Implementation of FluxingLabel. Text is rendered centered
// (DWRITE_TEXT_ALIGNMENT_CENTER + DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
// using the requested font size and palette.text color.

#include "stdafx.h"

#include "Label.h"

namespace fluxing {
namespace ui {

namespace {

const wchar_t* kLabelClassName = L"FluxingLabelClass";

void RegisterLabelClass(HINSTANCE hInst) {
  static bool registered = false;
  if (registered) return;
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &FluxingLabel::WndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kLabelClassName;
  RegisterClassExW(&wc);
  registered = true;
}

}  // namespace

std::unique_ptr<FluxingLabel> FluxingLabel::Create(
    HWND hwndParent, RECT const& rect, std::wstring const& text,
    FontSize size) {
  if (!hwndParent) return nullptr;
  HINSTANCE hInst = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(hwndParent, GWLP_HINSTANCE));
  if (!hInst) hInst = GetModuleHandle(nullptr);
  RegisterLabelClass(hInst);

  auto lbl = std::unique_ptr<FluxingLabel>(new FluxingLabel());
  lbl->text_ = text;
  lbl->size_ = size;

  HWND hwnd = CreateWindowExW(
      0, kLabelClassName, text.c_str(),
      WS_CHILD | WS_VISIBLE,
      rect.left, rect.top,
      rect.right - rect.left, rect.bottom - rect.top,
      hwndParent, nullptr, hInst, lbl.get());
  if (!hwnd) {
    return nullptr;
  }
  lbl->hwnd_ = hwnd;
  lbl->SubscribeTheme();
  return lbl;
}

FluxingLabel::~FluxingLabel() { Destroy(); }

void FluxingLabel::SetText(std::wstring const& text) {
  text_ = text;
  if (hwnd_) {
    SetWindowTextW(hwnd_, text_.c_str());
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void FluxingLabel::Destroy() {
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

void FluxingLabel::SubscribeTheme() {
  theme_handle_ = FluxingTheme::Instance().Subscribe(
      [this](::fluxing::Palette const&) {
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
      });
}

LRESULT CALLBACK FluxingLabel::WndProc(HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam) {
  FluxingLabel* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<FluxingLabel*>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<FluxingLabel*>(
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

LRESULT FluxingLabel::HandlePaint() {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd_, &ps);
  if (!hdc) return 0;
  auto rt =
      FluxingD2DRenderer::Instance().CreateHwndRenderTarget(hwnd_);
  if (!rt) { EndPaint(hwnd_, &ps); return 0; }
  rt->BeginDraw();

  auto pal = FluxingTheme::Instance().CurrentPalette();
  Microsoft::WRL::ComPtr<IDWriteTextFormat> fmt;
  if (SUCCEEDED(FluxingD2DRenderer::Instance().WriteFactory()
                    ->CreateTextFormat(
                        L"Segoe UI", nullptr,
                        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                        DWRITE_FONT_STRETCH_NORMAL, FontSizeToPt(size_),
                        L"zh-CN", &fmt))) {
    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    rt->CreateSolidColorBrush(
        D2D1::ColorF(pal.text, 1.0f), &brush);
    RECT rc;
    GetClientRect(hwnd_, &rc);
    rt->DrawText(
        text_.c_str(), static_cast<UINT32>(text_.size()), fmt.Get(),
        D2D1::RectF(0.0f, 0.0f,
                    static_cast<FLOAT>(rc.right - rc.left),
                    static_cast<FLOAT>(rc.bottom - rc.top)),
        brush.Get());
  }

  HRESULT hr = rt->EndDraw();
  if (hr == D2DERR_RECREATE_TARGET) {
    FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  }
  EndPaint(hwnd_, &ps);
  return 0;
}

LRESULT FluxingLabel::HandleDestroy() {
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd_);
  return 0;
}

}  // namespace ui
}  // namespace fluxing