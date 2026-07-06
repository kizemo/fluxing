# spec 041 计划 - FluxingComponents 多 DPI 验证（v0.18.28+）

## 1. 技术上下文

> - **Win32 HWND + D2D/DirectWrite 栈**（与 spec 037 相同）：ID2D1Factory /
>   IDWriteFactory / ID2D1HwndRenderTarget。
> - **修改文件**：
>   - `WeaselUI/FluxingComponents/D2DRenderer.{h,cpp}` - 添加 GetPhysicalClientRect helper
>   - `WeaselUI/FluxingComponents/{Label,Panel,Button,Toggle}.{h,cpp}` - HandlePaint 用 physical rect + WM_DPICHANGED handler
>   - `WeaselUI/FluxingComponents/targetver.h` - 升 _WIN32_WINNT 到 WIN10
>   - `test/TestFluxingComponents/Test*.cpp` - 加 DPI 100/150/200 测试用例
> - **新建文件**：无（仅修改既有文件）
> - **依赖**：
>   - GetDpiForWindow (Win 10 1607+, _WIN32_WINNT >= 0x0A00)
>   - ID2D1HwndRenderTarget::Resize() (D2D 1.0+)
>   - WM_DPICHANGED message (Win Vista+)

## 2. 技术方案

### 2.1 D2DRenderer GetPhysicalClientRect helper

```cpp
// D2DRenderer.h
static void GetPhysicalClientRect(HWND hwnd, RECT* out_rc);
```

```cpp
// D2DRenderer.cpp
void FluxingD2DRenderer::GetPhysicalClientRect(HWND hwnd, RECT* out_rc) {
  if (!out_rc) return;
  *out_rc = RECT{0, 0, 0, 0};
  if (!hwnd) return;
  RECT rc;
  if (!GetClientRect(hwnd, &rc)) return;
  // In PerMonitor V1 DPI aware processes, child HWNDs are
  // DPI-virtualized by Windows: GetClientRect returns LOGICAL
  // coords while the HWND physical surface is logical/(dpi/96).
  UINT dpi = GetDpiForWindow(hwnd);
  if (dpi == 0) dpi = 96;
  out_rc->left = MulDiv(rc.left, 96, dpi);
  out_rc->top = MulDiv(rc.top, 96, dpi);
  out_rc->right = MulDiv(rc.right, 96, dpi);
  out_rc->bottom = MulDiv(rc.bottom, 96, dpi);
}
```

### 2.2 CreateHwndRenderTarget backing store size

```cpp
// D2DRenderer.cpp
RECT rc;
if (!GetClientRect(hwnd, &rc)) return nullptr;
UINT dpi = GetDpiForWindow(hwnd);
if (dpi == 0) dpi = 96;
UINT physical_w = MulDiv(rc.right - rc.left, 96, dpi);
UINT physical_h = MulDiv(rc.bottom - rc.top, 96, dpi);
D2D1_SIZE_U size = D2D1::SizeU(physical_w, physical_h);
```

### 2.3 WM_DPICHANGED handler (4 controls)

```cpp
// Label/Panel/Button/Toggle.cpp WndProc:
case WM_DPICHANGED: {
  // L52 follow-up: release cached rt, force recreate on next paint
  FluxingD2DRenderer::Instance().ReleaseHwndRenderTarget(hwnd);
  RECT* new_rc = reinterpret_cast<RECT*>(lParam);
  SetWindowPos(hwnd, nullptr, new_rc->left, new_rc->top,
               new_rc->right - new_rc->left, new_rc->bottom - new_rc->top,
               SWP_NOZORDER | SWP_NOACTIVATE);
  InvalidateRect(hwnd, nullptr, FALSE);
  return 0;
}
```

### 2.4 HandlePaint 用 physical rect

```cpp
// 4 controls: replace GetClientRect(hwnd_, &rc) with
//             FluxingD2DRenderer::GetPhysicalClientRect(hwnd_, &rc)
```

## 3. 风险分析

- **R1**: D2D backing store size 必须用 physical pixel，否则文字溢出（spec 037
  R2 推迟导致的 v0.18.27.x visual bug 根因）。Mitigation: 严格按 physical
  size 转换。
- **R2**: WM_DPICHANGED 中 ReleaseHwndRenderTarget 必须先于 Resize，否则
  D2DERR_RECREATE_TARGET race。Mitigation: 在 handler 里 Release + InvalidateRect
  让下一个 WM_PAINT 重新 Create。
- **R3**: 测试 DPI 150 baseline 截图需要 consistent display setup（144 DPI
  dev workstation + 96 DPI test display）。Mitigation: TestFluxingComponents
  内部用 `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`
  + `GetDpiForWindow(test_hwnd)` 取实际 DPI，断言 backing store size 正确。

## 4. 任务分解

参见 tasks.md。