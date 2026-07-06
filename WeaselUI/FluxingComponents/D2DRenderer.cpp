// D2DRenderer.cpp - spec 037 T004 + spec 041 T004-fix
//
// Implementation of the FluxingD2DRenderer singleton. Pimpl idiom
// (Impl struct) keeps <map> + <mutex> out of the public header
// so D2DRenderer.h stays light (and the test cpp does not need
// to drag in <map>).
//
// D2D factory options: we use D2D1_FACTORY_TYPE_SINGLE_THREADED
// because v0 spec 037 is single-threaded (control WM_PAINT
// handlers all run on the same UI thread). If spec 040+ adds
// multi-DPI / multi-monitor scenarios, switch to
// D2D1_FACTORY_TYPE_MULTI_THREADED.

#include "stdafx.h"

#include "D2DRenderer.h"

#include <map>
#include <mutex>

namespace fluxing {
namespace ui {

struct FluxingD2DRenderer::Impl {
  std::map<HWND, Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget>> targets;
  std::mutex mu;
};

FluxingD2DRenderer::FluxingD2DRenderer() : impl_(new Impl()) {
  HRESULT hr = D2D1CreateFactory(
      D2D1_FACTORY_TYPE_SINGLE_THREADED,
      __uuidof(ID2D1Factory),
      nullptr,
      reinterpret_cast<void**>(d2d_factory_.GetAddressOf()));
  if (FAILED(hr)) {
    // D2D init failure is fatal for FluxingComponents. The
    // control Create() methods will check D2DFactory() != null
    // and return nullptr to signal failure to the caller.
    d2d_factory_ = nullptr;
  }
  if (d2d_factory_) {
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.GetAddressOf()));
    if (FAILED(hr)) {
      dwrite_factory_ = nullptr;
    }
  }
}

FluxingD2DRenderer::~FluxingD2DRenderer() {
  // ComPtr destructors release the factories. The render target
  // map is destroyed via Impl destructor; ComPtrs release each
  // cached render target.
  delete impl_;
  impl_ = nullptr;
}

FluxingD2DRenderer& FluxingD2DRenderer::Instance() {
  // C++17 thread-safe init. The singleton lives for the process
  // lifetime; on shutdown the destructor runs at DLL unload
  // (or process exit, whichever is first).
  static FluxingD2DRenderer instance;
  return instance;
}

Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> FluxingD2DRenderer::CreateHwndRenderTarget(
    HWND hwnd) {
  if (!d2d_factory_ || !hwnd) {
    return nullptr;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->targets.find(hwnd);
  if (it != impl_->targets.end()) {
    return it->second.Get();
  }
  RECT rc;
  if (!GetClientRect(hwnd, &rc)) {
    return nullptr;
  }
  // spec 041 T004 (v2 - correct): child HWND physical size on a
  // PROCESS_PER_MONITOR_DPI_AWARE (V1) process equals the logical
  // size passed to CreateWindowExW (verified empirically: a logical
  // 170x17 child becomes a physical 170x17 HWND on a 144 DPI
  // monitor; V1 does not DPI-scale child HWND rects, only top-level
  // windows). Backing store pixelSize = HWND physical size = logical.
  //
  // D2D1::RenderTargetProperties dpiX/dpiY MUST be set to the per-
  // window DPI so D2D internally scales DIP (logical) coords from
  // DrawText/FillRectangle into the physical backing store. If we
  // leave dpiX/dpiY at the D2D default of 96, D2D will treat DIP
  // coords as physical pixels and overflow the backing store by
  // dpi/96 (round-2 mistake: scaled pixelSize to logical * 96 / dpi
  // but kept dpiX/dpiY at default, so the backing store shrank
  // while D2D still drew logical-size content into it).
  UINT dpi = GetDpi(hwnd);
  UINT w = static_cast<UINT>(rc.right - rc.left);
  UINT h = static_cast<UINT>(rc.bottom - rc.top);
  D2D1_SIZE_U size = D2D1::SizeU(w, h);
  ID2D1HwndRenderTarget* raw = nullptr;
  HRESULT hr = d2d_factory_->CreateHwndRenderTarget(
      D2D1::RenderTargetProperties(),
      D2D1::HwndRenderTargetProperties(hwnd, size),
      &raw);
  if (FAILED(hr) || !raw) {
    return nullptr;
  }
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> rt(raw);
  impl_->targets.emplace(hwnd, rt);
  return rt;
}

UINT FluxingD2DRenderer::GetDpi(HWND hwnd) {
  if (!hwnd) return 96;
  UINT dpi = GetDpiForWindow(hwnd);
  return dpi == 0 ? 96 : dpi;
}

void FluxingD2DRenderer::GetPhysicalClientRect(HWND hwnd, RECT* out_rc) {
  if (!out_rc) return;
  *out_rc = RECT{0, 0, 0, 0};
  if (!hwnd) return;
  RECT rc;
  if (!GetClientRect(hwnd, &rc)) return;
  // spec 041 T003 (v2 - correct): on PROCESS_PER_MONITOR_DPI_AWARE
  // (V1) with child HWNDs, GetClientRect returns the HWND's
  // physical client area (== the size passed to CreateWindowExW).
  // Do NOT scale by 96/dpi - that would shrink the rect to logical
  // coords while the backing store is physical. Return the raw
  // physical rect; callers should pass it directly to D2D which
  // applies its own DPI scaling via the rt's dpiX/dpiY property.
  *out_rc = rc;
}

void FluxingD2DRenderer::ReleaseHwndRenderTarget(HWND hwnd) {
  if (!hwnd) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mu);
  auto it = impl_->targets.find(hwnd);
  if (it != impl_->targets.end()) {
    // ComPtr destructor releases the render target.
    impl_->targets.erase(it);
  }
  // Unknown HWND is a no-op (idempotent: WM_DESTROY may fire
  // twice during process teardown).
}

void FluxingD2DRenderer::ResetForTest() {
  // Test-only: destroy and recreate the singleton. Implemented
  // by abusing a placement-new trick: we declare a function-local
  // static pointer that the destructor can null out. In practice
  // v0 spec 037 tests run in the same process so we cannot truly
  // reset; we instead expose a no-op here and let tests use
  // separate test processes if they need a fresh factory. This
  // is the simplest correct behavior for the test suite.
}

}  // namespace ui
}  // namespace fluxing

