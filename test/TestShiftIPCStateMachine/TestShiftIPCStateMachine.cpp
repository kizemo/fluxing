// TestShiftIPCStateMachine.cpp
//
// 11 unit tests for fluxing::ShiftStateMachine (spec 077 §2.7.1).
// Pure logic test, no IPC / rime_api / GUI dependencies.
//
// TDD phase: RED — the skeleton in include/ShiftStateMachine.h has empty
// implementations, so all 11 tests must fail here at HEAD ff03ad8.
// Once T02..T04 fill the implementations in, this exe reports 11 PASS / 0 FAIL.

#include <cstdio>
#include <ShiftStateMachine.h>

using namespace fluxing;

static int s_passed = 0;
static int s_failed = 0;

#define EXPECT(cond, msg) do {                              \
  if (cond) { ++s_passed;                                   \
    std::printf("  PASS: %s\n", msg); }                     \
  else { ++s_failed;                                        \
    std::printf("  FAIL: %s (at %s:%d)\n",                  \
                msg, __FILE__, __LINE__); }                  \
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
  EXPECT(r.index == 1u, "Test #9: intervening before Down Down->Up fires index=1");
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

int main() {
  std::printf("TestShiftIPCStateMachine (spec 077 §2.7.1)\n");
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
  std::printf("\nResult: %d PASS / %d FAIL\n", s_passed, s_failed);
  return s_failed == 0 ? 0 : 1;
}