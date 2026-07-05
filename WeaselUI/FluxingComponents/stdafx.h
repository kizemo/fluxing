// stdafx.h - WeaselUI/FluxingComponents subdir PCH (spec 037)
//
// L33 minimal-deps pattern with d2d1/dwrite: the .h files
// in this subdir (D2DRenderer.h, Button.h, Toggle.h, Panel.h,
// Label.h, FluxingTheme.h) reference D2D/DirectWrite types
// (ID2D1Factory, IDWriteFactory, ID2D1HwndRenderTarget, etc.)
// so the PCH must include <d2d1.h> + <dwrite.h>.
//
// Include order matters (Windows SDK 10.0.26100.0):
//   <windows.h> first (base Win32)
//   <unknwn.h> next (IUnknown base for COM)
//   <d2d1.h> next (defines ID2D1Factory etc., requires IUnknown)
//   <dwrite.h> next (defines IDWriteFactory etc.)
//   <wrl/client.h> last (ComPtr<T> wrapper)

#pragma once

#include "targetver.h"

#include <windows.h>
#include <unknwn.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "FluxingDarkModeBridge.h"