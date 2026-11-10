// TestFluxingLangBar.cpp - ADR 0053 (2026-11-10)
//
// Covers the language-bar item style bits + hide policy. Single source of
// truth: include/FluxingLangBarStyle.h, shared with the production code
// (WeaselTSF/LanguageBar.cpp).
//
// NOT covered here: whether the Windows shell really removes the icon from
// the notification area. That is shell behaviour and can only be verified on
// a real machine (install -> restart the host -> enumerate the notification
// area through UIA). See the batch handoff doc for the evidence trail.

#include "stdafx.h"
#include "FluxingLangBarStyle.h"
#include <cstdio>

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", name);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", name);
  }
}

}  // namespace

namespace fluxing_test {

int RunLangBarTest() {
  using fluxing::kLangBarItemStyle;
  using fluxing::kLangBarStatusHidden;
  using fluxing::kLangBarItemForceHidden;

  // T1: HIDDENSTATUSCONTROL must be present, otherwise TF_LBI_STATUS_HIDDEN
  //     has no effect (MS docs: without it the item cannot be hidden this
  //     way).
  Check((kLangBarItemStyle & TF_LBI_STYLE_HIDDENSTATUSCONTROL) != 0,
        "T1: style carries TF_LBI_STYLE_HIDDENSTATUSCONTROL");

  // T2: keep the button behaviour (left click opens the quick panel).
  Check((kLangBarItemStyle & TF_LBI_STYLE_BTN_BUTTON) != 0 &&
            (kLangBarItemStyle & TF_LBI_STYLE_BTN_MENU) != 0,
        "T2: style keeps BTN_BUTTON and BTN_MENU");

  // T3: TF_LBI_STYLE_SHOWNINTRAY must not be relied upon -- MS docs say
  //     "this flag is currently not supported" (ADR 0044 premise falsified).
  Check((kLangBarItemStyle & TF_LBI_STYLE_SHOWNINTRAY) == 0,
        "T3: style does not rely on TF_LBI_STYLE_SHOWNINTRAY");

  // T4: no style bits outside the reviewed whitelist.
  const DWORD allowed = TF_LBI_STYLE_HIDDENSTATUSCONTROL | TF_LBI_STYLE_BTN_BUTTON |
                        TF_LBI_STYLE_BTN_MENU | TF_LBI_STYLE_SHOWNINTRAY;
  Check((kLangBarItemStyle & ~allowed) == 0,
        "T4: style carries no unreviewed bits");

  // T5: the hide status bit is the documented one.
  Check(kLangBarStatusHidden == TF_LBI_STATUS_HIDDEN &&
            kLangBarStatusHidden != 0,
        "T5: hide status bit == TF_LBI_STATUS_HIDDEN");

  // T6: the force-hide policy is on, so Show(TRUE) cannot revive the icon.
  //     v14: only meaningful on the fallback (registered) path.
  Check(kLangBarItemForceHidden, "T6: force-hide policy is enabled");

  // T7: cross-check -- the policy is only meaningful together with the
  //     style bit; if the style ever loses it, the icon would come back.
  Check(!kLangBarItemForceHidden ||
            (kLangBarItemStyle & TF_LBI_STYLE_HIDDENSTATUSCONTROL) != 0,
        "T7: force-hide policy is consistent with the style bits");

  // T8 (v14, decisive): the item must NOT be registered at all. Verified the
  //     hard way on 2026-11-10 -- fresh DLL (SHA256 == build output),
  //     registry -> same file, restarted explorer (so the new image is
  //     mapped), Fluxing as the active IME: the boxed 中 icon was STILL there
  //     under both ADR 0044 and ADR 0053. Only "no AddItem" is left.
  Check(!fluxing::kLangBarRegisterItem,
        "T8: langbar item is not registered (v14 force-absent)");

  const int total = g_pass + g_fail;
  std::printf("\nTestFluxingLangBar: %d/%d PASS\n", g_pass, total);
  return g_fail == 0 ? 0 : 1;
}
}  // namespace fluxing_test