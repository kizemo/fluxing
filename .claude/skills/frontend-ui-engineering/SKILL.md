---
name: frontend-ui-engineering
description: Build production-quality UIs that look hand-crafted by a design-aware engineer, not AI-generated. Use when building or modifying user-facing interfaces in FluxingComponents, WeaselPanel, or QuickPanelDialog.
---

# Frontend UI Engineering

> Goal: UI that looks like it was built by a design-aware engineer at a top company — not AI-generated. Means real design system adherence, proper accessibility, thoughtful interaction, no generic "AI aesthetic."

## When to use

- Building new FluxingComponents (Button / Toggle / Panel / Label / Dropdown / Slider)
- Modifying WeaselPanel (candidate box at 60 FPS)
- Modifying QuickPanelDialog
- Layout, spacing, typography decisions
- Color / theme changes

## When NOT to use

- Pure backend logic
- Test code
- Build infrastructure

## FluxingComponents design rules

### Visual language

- **mac 风** (project choice per spec 037 / 049): 14px rounded corners, 4% black hover overlay, SF Symbols style icons
- **Card style**: 8px rounded, subtle background tint
- **Typography**: Microsoft YaHei UI (Chinese environment default)
- **Spacing**: 8/16/24 grid (8px base)

### States (always design all 5)

- Default (idle)
- Hover (mouse over, 4% overlay)
- Pressed/Active (mouse down, 8% overlay)
- Focus (keyboard, 2px ring at `--accent`)
- Disabled (40% opacity, no pointer)

### Performance (L41/L52)

- 60 FPS at 100/150/200 DPI (SC-003)
- Don't redraw whole panel on state change
- D2D backing store must call `rt->Clear()` (L52)
- WM_DPICHANGED handler must ReleaseHwndRenderTarget + InvalidateRect (L52)

### DPI handling (L52)

- V1 child physical = logical × 96/dpi
- V1 top-level physical = logical × dpi/96
- D2D1_HwndRenderTargetProperties.pixelSize = HWND physical
- D2D1_RenderTargetProperties dpiX/dpiY default 96
- Use `GetDpiForWindow` (per-monitor, not system)

### GDI fallback (L50 / L51)

If D2D fails (driver, ACL, GPU virtualization), FluxingComponents MUST have GDI fallback:
- `if (!rt) { /* GDI paint */ }`
- Use `CreateSolidBrush` + `FillRect` + `DeleteObject`
- Use `CreateFontIndirectW("Segoe UI", N pt)` + `DrawTextW`

## Dark mode (spec 033 + 037)

- Subscribe to `FluxingDarkModeBridge::CurrentPalette()`
- 200ms transition (spec 004 §9.1)
- Don't re-create render target on theme change (use Clear + Repaint)

## Anti-patterns

- "Modern gradient" with 7 colors
- Animations > 300ms (feels slow)
- Pure black background (use #1E1E1E for dark, L42 byte pattern)
- Hard-coded colors (use FluxingTheme)
- 16px padding where 8 would do
- 200ms transition on every property change
- Animations that can't be cancelled
