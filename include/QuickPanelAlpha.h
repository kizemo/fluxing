#pragma once
//
// QuickPanelAlpha - shared alpha math for the Fluxing QuickPanel.
//
// spec 084 / ADR 0013: the QuickPanel draws its "liquid glass" surface with
// per-pixel alpha (QuickPanelDialog::ApplyAlphaGradient writes the alpha
// channel of a 32-bit BGRA DIB, then UpdateLayeredWindow(ULW_ALPHA) submits
// it). spec 052 wanted a second, much fainter state - "always show" idle at
// 85% opacity at idle, 100% on hover - but the only knob spec 052 knew about
// was SetLayeredWindowAttributes(LWA_ALPHA), a *uniform* alpha that the current
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

// Opacity factors. 255 == 100% (no scaling).
constexpr int kAlphaFactorFull = 255;
// ADR 0013 D3 set the idle factor to 51 (20%). That factor multiplies the
// per-pixel alpha that ApplyAlphaGradient() already recessed (140 -> 82 down
// the panel), so the idle surface landed at roughly 28 -> 16 of 255 -- about
// 6-11% over the desktop. The 2026-10-29 field report "面板在有输入动作后
// 仍然消失" (spec 087) is that value, not a Hide(): the 100ms poller parks
// s_targetAlphaFactor at the idle factor whenever the cursor is outside the
// panel, and while typing the cursor is by definition outside (it is over the
// target application's edit field). Nothing hides the panel; it just becomes
// too faint to see, and every Show() reuse starts from that stale value.
// ADR 0018 raises the idle floor to 153 (60%): still visibly recessed against
// the hover state, no longer mistaken for hidden.
constexpr int kAlphaFactorIdle = 217;

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

// Fade step per 100ms timer tick. Walking 255 -> 217 takes 2 ticks (~0.2s)
// after spec 089 / ADR 0022 raised the idle floor from 153 to 217 (38 / 24
// = 1.58 -> 2); the reverse walk is 2 ticks as well.
constexpr int kAlphaFadeStep = 24;

}  // namespace fluxing
