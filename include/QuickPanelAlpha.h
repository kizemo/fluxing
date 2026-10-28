#pragma once
//
// QuickPanelAlpha - shared alpha math for the Fluxing QuickPanel.
//
// spec 084 / ADR 0013: the QuickPanel draws its "liquid glass" surface with
// per-pixel alpha (QuickPanelDialog::ApplyAlphaGradient writes the alpha
// channel of a 32-bit BGRA DIB, then UpdateLayeredWindow(ULW_ALPHA) submits
// it). spec 052 wanted a second, much fainter state - "always show" idle at
// 20% opacity, 100% on hover - but the only knob spec 052 knew about was
// SetLayeredWindowAttributes(LWA_ALPHA), a *uniform* alpha that the current
// renderer explicitly forbids (QuickPanelDialog.h: "绝对不用 LWA_ALPHA
// uniform" - it is not liquid glass, just a flat 86% white sheet).
//
// So the fade is expressed as an **opacity factor** multiplied into the
// per-pixel alpha gradient. factor == 255 means "no scaling" and therefore
// reproduces today's pixels byte for byte - that is the zero-regression
// guarantee of spec 084.
//
// The math lives in this header-only unit (no windows.h, no GUI) so the
// acceptance gate can assert it without creating a window.
//
// Naming: the namespace is `fluxing`, matching include/KeyNameMapper.h
// (ADR 0011 D1 - shared units live under include/ and are included with
// angle brackets).

namespace fluxing {

// Opacity factors. 255 == 100% (no scaling), 51 == 20%.
constexpr int kAlphaFactorFull = 255;
constexpr int kAlphaFactorIdle = 51;

// Denominator of the opacity factor scale.
constexpr int kAlphaScaleNum = 255;

// Scale a per-pixel alpha by an opacity factor.
//
// factor == kAlphaFactorFull -> result == base (byte identical to the
// pre-spec-084 rendering). factor == 0 -> result == 0 (fully transparent).
// Rounding is half-up so that factor == 255 never loses a level.
inline int ScaleAlpha(int base, int factor) {
  if (base <= 0)
    return 0;
  if (factor <= 0)
    return 0;
  if (base > 255)
    base = 255;
  if (factor >= kAlphaScaleNum)
    return base;
  return (base * factor + kAlphaScaleNum / 2) / kAlphaScaleNum;
}

// Move `current` towards `target` by at most `step`, never overshooting.
// Used by the 100ms fade timer: the panel walks from the idle factor to the
// full factor instead of snapping, which is what "渐变" means here.
inline int StepAlpha(int current, int target, int step) {
  if (step < 0)
    step = 0;
  if (current < target) {
    const int next = current + step;
    return next > target ? target : next;
  }
  if (current > target) {
    const int next = current - step;
    return next < target ? target : next;
  }
  return current;
}

// Linear interpolation helper (from -> to, t in [0, steps]).
inline int LerpAlpha(int from, int to, int t, int steps) {
  if (steps <= 0)
    return to;
  if (t <= 0)
    return from;
  if (t >= steps)
    return to;
  return from + (to - from) * t / steps;
}

// Fade step per 100ms timer tick. Walking 255 -> 51 takes 9 ticks (~0.9s),
// walking 51 -> 255 takes 9 ticks as well (204 / 24 = 8.5 -> 9).
// Fade step per 100ms timer tick. Walking 255 -> 51 takes 9 ticks (~0.9s),
// walking 51 -> 255 takes 9 ticks as well (204 / 24 = 8.5 -> 9).
constexpr int kAlphaFadeStep = 24;

}  // namespace fluxing
