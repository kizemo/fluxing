// TestShiftIPCStateMachine.cpp
//
// 17 unit tests for fluxing::ShiftStateMachine (11 x spec 077 §2.7.1
// + 6 x ADR 0010 bare-Shift disambiguation).
// Pure logic test, no IPC / rime_api / GUI dependencies.
//
// The ShiftStateMachine implementation in include/ShiftStateMachine.h is real
// (spec 077 + ADR 0010), so this exe must report 0 FAIL. The PASS count
// grows with every assertion: 17 after spec 077, 28 after ADR 0010.

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

int main() {
  std::printf("TestShiftIPCStateMachine (spec 077 §2.7.1 + ADR 0010)\n");
  Test_ShiftDownUp_L_FireSelect1();
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
  std::printf("\nResult: %d PASS / %d FAIL\n", s_passed, s_failed);
  return s_failed == 0 ? 0 : 1;
}