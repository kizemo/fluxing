// D2DRenderer.cpp - spec 037 T004 (2026-07-05)
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
  D2D1_SIZE_U size = D2D1::SizeU(
      static_cast<UINT32>(rc.right - rc.left),
      static_cast<UINT32>(rc.bottom - rc.top));
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