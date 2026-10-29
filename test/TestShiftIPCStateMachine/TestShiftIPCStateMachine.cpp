// TestShiftIPCStateMachine.cpp
//
// 20 unit tests for fluxing::ShiftStateMachine (11 x spec 077 §2.7.1
// + 6 x ADR 0010 bare-Shift disambiguation
// + 3 x ADR 0012 ascii_mode propagation scope).
// Pure logic test, no IPC / rime_api / GUI dependencies.
//
// The ShiftStateMachine implementation in include/ShiftStateMachine.h is real
// (spec 077 + ADR 0010 + ADR 0012), so this exe must report 0 FAIL. The PASS
// count grows with every assertion: 17 after spec 077, 28 after ADR 0010,
// 34 after ADR 0012.

#include <cstdio>
#include <ShiftStateMachine.h>

using namespace fluxing;

static int s_passed = 0;
static int s_failed = 0;

#define EXPECT(cond, msg)                                              \
  do {                                                                 \
    if (cond) {                                                        \
      ++s_passed;                                                      \
      std::printf("  PASS: %s\n", msg);                                \
    } else {                                                           \
      ++s_failed;                                                      \
      std::printf("  FAIL: %s (at %s:%d)\n", msg, __FILE__, __LINE__); \
    }                                                                  \
  } while (0)

// Test #1 — Shift_L Down→Up fires select index 1 (0-based).
void Test_ShiftDownUp_L_FireSelect1() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, /*is_left=*/true);
  auto r = sm.OnShiftUp(1, true);
  EXPECT(r.fire_select, "Test #1: Shift_L Down->Up fires select");
  EXPECT(r.index == 1u, "Test #1: Shift_L Down->Up fires select index=1");
}

// Test #2 — Shift_R Down→Up fires select index 2 (0-based).
void Test_ShiftDownUp_R_FireSelect2() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, /*is_left=*/false);
  auto r = sm.OnShiftUp(1, false);
  EXPECT(r.fire_select, "Test #2: Shift_R Down->Up fires select");
  EXPECT(r.index == 2u, "Test #2: Shift_R Down->Up fires select index=2");
}

// Test #3 — intervening non-Shift key between Down and Up suppresses fire.
void Test_InterveningBlocks_NoFire() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnInterveningKey(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT(!r.fire_select, "Test #3: intervening key blocks fire");
}

// Test #4 — Up without prior Down is ignored.
void Test_UpWithoutDown_Ignored() {
  ShiftStateMachine sm;
  auto r = sm.OnShiftUp(1, true);
  EXPECT(!r.fire_select, "Test #4: Up without Down is ignored");
}

// Test #5 — double Down overrides lastIsLeft; final Up is what is observed.
void Test_DoubleDown_LastWins() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnShiftDown(1, false);  // override to right
  auto r = sm.OnShiftUp(1, false);
  EXPECT(r.fire_select, "Test #5: double Down second Down wins");
  EXPECT(r.index == 2u, "Test #5: double Down second Down wins index=2");
}

// Test #6 — two intervening keys still suppress fire.
void Test_TwoIntervening_StillNoFire() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnInterveningKey(1);
  sm.OnInterveningKey(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT(!r.fire_select, "Test #6: two intervening keys still no fire");
}

// Test #7 — second cycle after first fire also fires (state machine re-armed).
void Test_CycleTwice_SecondFires() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnShiftUp(1, true);
  sm.OnShiftDown(1, false);
  auto r = sm.OnShiftUp(1, false);
  EXPECT(r.fire_select, "Test #7: cycle twice second fires");
  EXPECT(r.index == 2u, "Test #7: cycle twice second fires index=2");
}

// Test #8 — Reset wipes per-session state; subsequent Up is ignored.
void Test_ResetClearsState_UpAfterResetIgnored() {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.Reset(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT(!r.fire_select, "Test #8: Reset clears state, Up ignored");
}

// Test #9 — intervening key before any Down must not pollute subsequent cycle.
void Test_InterveningBeforeDown_NotPolluting() {
  ShiftStateMachine sm;
  sm.OnInterveningKey(1);  // no prior Down
  sm.OnShiftDown(1, true);
  auto r = sm.OnShiftUp(1, true);
  EXPECT(r.fire_select, "Test #9: intervening before Down does not pollute");
  EXPECT(r.index == 1u,
         "Test #9: intervening before Down Down->Up fires index=1");
}

// Test #10 — fuzz 100 mixed cycles, no crash, single session map entry.
void Test_Fuzz100_NoCrashOrLeak() {
  ShiftStateMachine sm;
  for (int i = 0; i < 100; ++i) {
    sm.OnShiftDown(1, i % 2 == 0);
    sm.OnInterveningKey(1);
    sm.OnShiftUp(1, i % 2 == 0);
  }
  EXPECT(sm.DebugSessionCount() == 1u,
         "Test #10: 100 fuzz cycles keep single session entry");
}

// Test #11 — multi-session independence.
void Test_MultiSessionIndependent_AFiresBNot() {
  ShiftStateMachine sm;
  sm.OnShiftDown(0xA, true);
  sm.OnShiftDown(0xB, false);
  auto ra = sm.OnShiftUp(0xA, true);
  auto rb = sm.OnShiftUp(0xB, false);
  EXPECT(ra.fire_select && ra.index == 1u,
         "Test #11: session A Down_L fires index=1");
  EXPECT(rb.fire_select && rb.index == 2u,
         "Test #11: session B Down_R fires index=2");
}

// ===== ADR 0010 (T1): bare Shift -> Chinese / English toggle =====
//
// The Shift release carries two intents: the spec 077 candidate selection and
// the ADR 0010 mode toggle. They are separated by one observable predicate
// (is a candidate menu open), never by a timing heuristic (ADR 0010 P3).

// Test #12 - no menu open -> toggle the input mode.
void Test_BareShift_NoMenu_TogglesAsciiMode() {
  EXPECT(DecideShiftUpAction(true, false, false, true) ==
             ShiftUpAction::kToggleAsciiMode,
         "Test #12: bare Shift with no menu toggles ascii_mode");
}

// Test #13 - menu open -> keep the spec 077 candidate selection.
void Test_BareShift_MenuOpen_SelectsCandidate() {
  EXPECT(DecideShiftUpAction(true, false, true, true) ==
             ShiftUpAction::kSelectCandidate,
         "Test #13: bare Shift with an open menu selects a candidate");
}

// Test #14 - feature switched off -> today's behaviour, exactly.
void Test_FeatureDisabled_AlwaysSelectsCandidate() {
  EXPECT(DecideShiftUpAction(true, false, false, false) ==
             ShiftUpAction::kSelectCandidate,
         "Test #14: feature disabled falls back to candidate selection");
  EXPECT(DecideShiftUpAction(true, false, true, false) ==
             ShiftUpAction::kSelectCandidate,
         "Test #14: feature disabled with a menu open selects a candidate");
}

// Test #15 - no Shift Down -> nothing happens.
void Test_NoDown_NoAction() {
  EXPECT(DecideShiftUpAction(false, false, false, true) == ShiftUpAction::kNone,
         "Test #15: Shift Up without Down does nothing");
  EXPECT(DecideShiftUpAction(false, false, true, true) == ShiftUpAction::kNone,
         "Test #15: Shift Up without Down does nothing even with a menu");
}

// Test #16 - an intervening key kills the bare Shift (Shift+X, Shift+space).
void Test_Intervening_NoAction() {
  EXPECT(DecideShiftUpAction(true, true, false, true) == ShiftUpAction::kNone,
         "Test #16: intervening key blocks the bare Shift toggle");
  EXPECT(DecideShiftUpAction(true, true, true, true) == ShiftUpAction::kNone,
         "Test #16: intervening key blocks the bare Shift toggle with a menu");
}

// Test #17 - exhaustive decision table over all 16 input combinations.
void Test_DecisionTable_Exhaustive() {
  int toggle = 0;
  int select = 0;
  int none = 0;
  for (int d = 0; d < 2; ++d)
    for (int i = 0; i < 2; ++i)
      for (int m = 0; m < 2; ++m)
        for (int f = 0; f < 2; ++f) {
          auto a = DecideShiftUpAction(d != 0, i != 0, m != 0, f != 0);
          if (a == ShiftUpAction::kToggleAsciiMode)
            ++toggle;
          else if (a == ShiftUpAction::kSelectCandidate)
            ++select;
          else
            ++none;
        }
  EXPECT(toggle == 1, "Test #17: exactly 1 of 16 combinations toggles");
  EXPECT(select == 3, "Test #17: exactly 3 of 16 combinations select");
  EXPECT(none == 12, "Test #17: exactly 12 of 16 combinations do nothing");
}

// ===== ADR 0012 (T1): ascii_mode propagation scope =====
//
// A bare Shift must never reach another window; an explicit control may reach
// every window, but only when weasel.yaml asks for it.

// Test #18 - a bare Shift never broadcasts, whatever global_ascii says.
void Test_AsciiScope_BareShiftNeverBroadcasts() {
  EXPECT(DecideAsciiModeScope(false, false) == AsciiModeScope::kCurrentSession,
         "Test #18: a bare Shift stays in its own session");
  EXPECT(DecideAsciiModeScope(false, true) == AsciiModeScope::kCurrentSession,
         "Test #18: a bare Shift stays in its own session (global_ascii on)");
}

// Test #19 - an explicit control follows the yaml flag.
void Test_AsciiScope_ExplicitControlFollowsYaml() {
  EXPECT(DecideAsciiModeScope(true, false) == AsciiModeScope::kCurrentSession,
         "Test #19: an explicit control stays local without global_ascii");
  EXPECT(DecideAsciiModeScope(true, true) == AsciiModeScope::kAllSessions,
         "Test #19: an explicit control reaches every session with "
         "global_ascii");
}

// Test #20 - exhaustive decision table over all 4 input combinations.
void Test_AsciiScope_DecisionTable() {
  int all = 0;
  int current = 0;
  for (int e = 0; e < 2; ++e)
    for (int g = 0; g < 2; ++g) {
      auto s = DecideAsciiModeScope(e != 0, g != 0);
      if (s == AsciiModeScope::kAllSessions)
        ++all;
      else
        ++current;
    }
  EXPECT(all == 1, "Test #20: exactly 1 of 4 combinations broadcasts");
  EXPECT(current == 3, "Test #20: exactly 3 of 4 combinations stay local");
}

// Test #12 - client encoder and server decoder must share one source
// (ADR 0015 regression). On 2026-10-28 the server inverted bit0 while
// decoding, so a bare Shift selected the mirrored candidate.
void Test_EncodeDecode_RoundTrip() {
  EXPECT(fluxing::EncodeShiftIsLeft(true) == 0u,
         "Test #12: Left encodes to wParam bit0=0 (wire contract 0=L)");
  EXPECT(fluxing::EncodeShiftIsLeft(false) == 1u,
         "Test #12: Right encodes to wParam bit0=1 (wire contract 1=R)");
  // Pin the decoder against the raw bit values as well. A round-trip-only
  // assertion cannot catch the case where BOTH ends are inverted, because the
  // round trip still closes. These two lines are what make the assertion
  // falsifiable in the sense ADR 0015 D3 requires.
  EXPECT(fluxing::DecodeShiftIsLeft(0u),
         "Test #12: wParam bit0=0 decodes to is_left=true (wire contract 0=L)");
  EXPECT(
      !fluxing::DecodeShiftIsLeft(1u),
      "Test #12: wParam bit0=1 decodes to is_left=false (wire contract 1=R)");
  EXPECT(fluxing::DecodeShiftIsLeft(fluxing::EncodeShiftIsLeft(true)),
         "Test #12: Left round-trip decodes back to is_left=true");
  EXPECT(!fluxing::DecodeShiftIsLeft(fluxing::EncodeShiftIsLeft(false)),
         "Test #12: Right round-trip decodes back to is_left=false");
}

// Test #21 - ADR 0017: the user can flip the two shift sides when the host
// reports them the other way round (spec 087 A.7). The switch is a plain
// configuration flag, so the decision stays a pure function.
void Test_ShiftSideSwap() {
  EXPECT(ApplyShiftSideSwap(true, false),
         "Test #21: without the swap the left shift stays on the left");
  EXPECT(!ApplyShiftSideSwap(false, false),
         "Test #21: without the swap the right shift stays on the right");
  EXPECT(!ApplyShiftSideSwap(true, true),
         "Test #21: with the swap the left shift becomes the right one");
  EXPECT(ApplyShiftSideSwap(false, true),
         "Test #21: with the swap the right shift becomes the left one");
}

// Test #22 - ADR 0018 / spec 087 A.8: a bare-Shift selection of a *partial*
// candidate (one character out of a multi-syllable shuangpin input) must not
// flush the whole composition, otherwise the head candidate of the remaining
// segment is committed next to the picked character. The signal that
// separates the two cases is the post-selection candidate menu: empty means
// the composition was consumed whole (ADR 0012 wants a flush), non-empty
// means syllables are left over (numeric-key behaviour: keep composing).
void Test_ShouldCommitAfterSelect() {
  EXPECT(fluxing::ShouldCommitAfterSelect(false, 0),
         "Test #22: unreadable context keeps the ADR 0012 flush");
  EXPECT(fluxing::ShouldCommitAfterSelect(false, 3),
         "Test #22: unreadable context flushes whatever the count claims");
  EXPECT(fluxing::ShouldCommitAfterSelect(true, 0),
         "Test #22: empty menu after select = whole word, flush");
  EXPECT(!fluxing::ShouldCommitAfterSelect(true, 1),
         "Test #22: one candidate left = partial pick, no flush");
  EXPECT(!fluxing::ShouldCommitAfterSelect(true, 5),
         "Test #22: an open menu = partial pick, no flush");
}

int main() {
  std::printf(
      "TestShiftIPCStateMachine (spec 077 §2.7.1 + ADR 0010 + ADR 0012)\n");
  Test_ShiftDownUp_L_FireSelect1();
  Test_EncodeDecode_RoundTrip();
  Test_ShiftDownUp_R_FireSelect2();
  Test_InterveningBlocks_NoFire();
  Test_UpWithoutDown_Ignored();
  Test_DoubleDown_LastWins();
  Test_TwoIntervening_StillNoFire();
  Test_CycleTwice_SecondFires();
  Test_ResetClearsState_UpAfterResetIgnored();
  Test_InterveningBeforeDown_NotPolluting();
  Test_Fuzz100_NoCrashOrLeak();
  Test_MultiSessionIndependent_AFiresBNot();
  Test_BareShift_NoMenu_TogglesAsciiMode();
  Test_BareShift_MenuOpen_SelectsCandidate();
  Test_FeatureDisabled_AlwaysSelectsCandidate();
  Test_NoDown_NoAction();
  Test_Intervening_NoAction();
  Test_DecisionTable_Exhaustive();
  Test_AsciiScope_BareShiftNeverBroadcasts();
  Test_AsciiScope_ExplicitControlFollowsYaml();
  Test_AsciiScope_DecisionTable();
  Test_ShiftSideSwap();
  Test_ShouldCommitAfterSelect();
  std::printf("\nResult: %d PASS / %d FAIL\n", s_passed, s_failed);
  return s_failed == 0 ? 0 : 1;
}