// D2DRenderer.h - spec 037 T003 (2026-07-05)
//
// Process-singleton wrapper for the ID2D1Factory + IDWriteFactory
// + ID2D1HwndRenderTarget cache. WeaselPanel.cpp creates its own
// ID2D1Factory; FluxingComponents wants to share the same factory
// across 4 control types + multiple HWND render targets, so we
// centralize here.
//
// Thread-safety: Get() returns a process-singleton; ID2D1Factory
// is thread-safe for CreateHwndRenderTarget. The HWND render
// target map is guarded by a critical section (control WM_PAINT
// handlers can run on different threads if the host pumps
// messages in a worker, but v0 spec 037 v0 is single-threaded
// per spec 037 R3).
//
// Lifecycle: the singleton lives for the process lifetime. The
// 4 base controls register/unregister their HWND render targets
// via CreateHwndRenderTarget / ReleaseHwndRenderTarget in their
// Create / Destroy methods. On WM_DESTROY the control calls
// ReleaseHwndRenderTarget(hwnd) before DestroyWindow.

#pragma once

#include "stdafx.h"

namespace fluxing {
namespace ui {

class FluxingD2DRenderer {
 public:
  // Process-singleton (C++17 thread-safe init). First caller
  // constructs the D2D + DirectWrite factories; subsequent
  // callers share.
  static FluxingD2DRenderer& Instance();

  // Raw pointer access. Lifetime is process-lifetime; do not
  // Release(). The underlying D2D objects are reference-counted
  // and thread-safe.
  ID2D1Factory* D2DFactory() const { return d2d_factory_.Get(); }
  IDWriteFactory* WriteFactory() const { return dwrite_factory_.Get(); }

  // Create (or fetch from cache) the ID2D1HwndRenderTarget for
  // the given HWND. Render targets are cached per-HWND; the same
  // HWND always returns the same render target pointer. Caller
  // does NOT own the pointer; do not Release.
  Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> CreateHwndRenderTarget(HWND hwnd);

  // Release the cached ID2D1HwndRenderTarget for the given HWND.
  // Called by control Destroy() (which is called from WM_DESTROY)
  // before DestroyWindow. Idempotent: safe to call twice. No-op
  // if HWND is unknown.
  void ReleaseHwndRenderTarget(HWND hwnd);

  // spec 041 T002: convert logical-pixel client rect (returned by
  // GetClientRect in a PerMonitor DPI aware process) to PHYSICAL
  // device pixels for D2D drawing. ID2D1HwndRenderTarget is sized
  // in physical pixels; D2D FillRectangle/RectF coordinates are
  // physical device units. Without this helper, the backing store
  // ends up 1.5x too large on a 144 DPI monitor and text/rounded
  // rects overflow the child HWND bounds (the v0.18.27.x visual
  // bug root cause).
  static void GetPhysicalClientRect(HWND hwnd, RECT* out_rc);

  // spec 041 T002: per-window DPI lookup (Win 10 1607+). Returns
  // 96 if hwnd is null or the API is unavailable.
  static UINT GetDpi(HWND hwnd);

  // Test-only: reset the singleton (for unit tests that need
  // a clean Instance()). Production code MUST NOT call this.
  // Forward-declared in the .cpp to avoid pulling <mutex> into
  // the header.
  static void ResetForTest();

 private:
  FluxingD2DRenderer();
  ~FluxingD2DRenderer();
  FluxingD2DRenderer(const FluxingD2DRenderer&) = delete;
  FluxingD2DRenderer& operator=(const FluxingD2DRenderer&) = delete;

  Microsoft::WRL::ComPtr<ID2D1Factory> d2d_factory_;
  Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory_;

  // Per-HWND render target cache. CRITICAL_SECTION guards the
  // map; D2D render targets are NOT thread-safe so the same
  // HWND's target must be accessed from one thread.
  struct Impl;
  Impl* impl_;
};

}  // namespace ui
}  // namespace fluxing