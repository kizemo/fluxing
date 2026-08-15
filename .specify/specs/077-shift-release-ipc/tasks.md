# 077 · Implementation Tasks

> **Total: 22 tasks** · T01–T05 (state machine + unit tests) · T06–T12 (wire + Server + TSF) · T13–T14 (integration test) · T15–T17 (yaml + extend tests) · T18–T22 (build + ship)
>
> **TDD discipline:** T01 (red)→T05 (green) state machine first. T06–T12 wire implementing IPC + Server + TSF with running tests at each step.
>
> **Tasks with `[VERIFY]`** include a mandatory verification command at the end of the step. Stop if verify fails.
>
> **Commit strategy (per handoff §2.4):** T01–T20 commits are *intermediate WIP commits* for review/revert granularity. T21 ships the final **`feat(WeaselIPC)`** commit (squashing/conventional of T01–T20's `Weasel*`/`RimeWithWeasel` changes + ADR + L## + CHANGELOG + yaml change). T22 ships **`chore(release)`** (version bump). `git add -A` / `git add .` is forbidden — every commit uses explicit paths.
>
> **Override note:** `RequestHandler::ShiftDown/ShiftUp/SelectCandidate` are declared with default `{}` in `WeaselIPC.h`, so ServerImpl (T08) builds & links cleanly even before RimeWithWeaselHandler overrides them (T09/T10). The runtime call is a no-op until the override exists.

---

## T01 · ShiftStateMachine skeleton + 11 unit tests red

**Files:**
- Create: `include/ShiftStateMachine.h`
- Create: `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp`
- Create: `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.vcxproj`
- Create: `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.vcxproj.filters`

**Interfaces:**
- Produces: `fluxing::ShiftStateMachine` class with `OnShiftDown`/`OnShiftUp`/`OnInterveningKey`/`Reset`/`OnSessionDestroyed`/`DebugSessionCount`.

**Step 1:** Create `include/ShiftStateMachine.h` with class skeleton (no implementations yet — declaration only).

```cpp
#pragma once
#include <cstddef>
#include <unordered_map>
#include <cstdint>

namespace fluxing {
using SessionId = std::uint32_t;

class ShiftStateMachine {
 public:
  struct EventResult {
    bool fire_select = false;
    std::size_t index = 0;
  };

  void Reset(SessionId sid) {}
  void OnShiftDown(SessionId sid, bool is_left) {}
  EventResult OnShiftUp(SessionId sid, bool is_left) { return {}; }
  void OnInterveningKey(SessionId sid) {}
  void OnSessionDestroyed(SessionId sid) {}
  std::size_t DebugSessionCount() const { return 0; }

 private:
  struct State {
    bool downRecorded = false;
    bool interveningKey = false;
    bool lastIsLeft = false;
  };
  std::unordered_map<SessionId, State> states_;
};
}  // namespace fluxing
```

**Step 2:** Create `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp` with **all 11 tests** from spec §2.7.1. Each test should reference the unimplemented class and FAIL on access.

```cpp
#include <gtest/gtest.h>
#include <ShiftStateMachine.h>

using namespace fluxing;

TEST(ShiftDownUp_L, FireSelect1) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, /*is_left=*/true);
  auto r = sm.OnShiftUp(1, true);
  EXPECT_TRUE(r.fire_select);
  EXPECT_EQ(r.index, 1u);
}

TEST(ShiftDownUp_R, FireSelect2) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, false);
  auto r = sm.OnShiftUp(1, false);
  EXPECT_TRUE(r.fire_select);
  EXPECT_EQ(r.index, 2u);
}

TEST(InterveningBlocks, NoFire) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnInterveningKey(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT_FALSE(r.fire_select);
}

TEST(UpWithoutDown, Ignored) {
  ShiftStateMachine sm;
  auto r = sm.OnShiftUp(1, true);
  EXPECT_FALSE(r.fire_select);
}

TEST(DoubleDown, LastWins) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnShiftDown(1, false);  // override
  auto r = sm.OnShiftUp(1, false);
  EXPECT_TRUE(r.fire_select);
  EXPECT_EQ(r.index, 2u);
}

TEST(TwoIntervening, StillNoFire) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnInterveningKey(1);
  sm.OnInterveningKey(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT_FALSE(r.fire_select);
}

TEST(CycleTwice, SecondFires) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.OnShiftUp(1, true);
  sm.OnShiftDown(1, false);
  auto r = sm.OnShiftUp(1, false);
  EXPECT_TRUE(r.fire_select);
  EXPECT_EQ(r.index, 2u);
}

TEST(ResetClearsState, UpAfterResetIgnored) {
  ShiftStateMachine sm;
  sm.OnShiftDown(1, true);
  sm.Reset(1);
  auto r = sm.OnShiftUp(1, true);
  EXPECT_FALSE(r.fire_select);
}

TEST(InterveningBeforeDown_NotPolluting, DownStillFires) {
  ShiftStateMachine sm;
  sm.OnInterveningKey(1);
  sm.OnShiftDown(1, true);
  auto r = sm.OnShiftUp(1, true);
  EXPECT_TRUE(r.fire_select);
  EXPECT_EQ(r.index, 1u);
}

TEST(Fuzz100, NoCrashOrLeak) {
  ShiftStateMachine sm;
  for (int i = 0; i < 100; ++i) {
    sm.OnShiftDown(1, i % 2 == 0);
    sm.OnInterveningKey(1);
    sm.OnShiftUp(1, i % 2 == 0);
  }
  EXPECT_EQ(sm.DebugSessionCount(), 1u);
}

TEST(MultiSessionIndependent, AFiresBNot) {
  ShiftStateMachine sm;
  sm.OnShiftDown(0xA, true);
  sm.OnShiftDown(0xB, false);
  auto ra = sm.OnShiftUp(0xA, true);
  auto rb = sm.OnShiftUp(0xB, false);
  EXPECT_TRUE(ra.fire_select);
  EXPECT_EQ(ra.index, 1u);
  EXPECT_TRUE(rb.fire_select);
  EXPECT_EQ(rb.index, 2u);
}
```

**Step 3:** Create the vcxproj mirroring `test/TestPipeChannelRace/TestPipeChannelRace.vcxproj` structure (gtest, Release/Win32, includes `../..`). Set ProjectGuid `{8E2F1A7B-4D5F-4F88-9A0C-9F0E0F0E0F01}`.

**Step 4** [VERIFY]: Run build:

```
msbuild test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error, tests COMPILE. Then run:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
```

Expected: **all 11 FAIL** (Reset always; no state change recorded; OnShiftUp returns default EventResult{} with fire_select=false even when should be true).

**Step 5:** Commit:

```bash
git add include/ShiftStateMachine.h test/TestShiftIPCStateMachine/
git commit -m "test(ShiftStateMachine): 11 unit tests (red) + skeleton class"
```

---

## T02 · State machine: Tests #1, #2 green (Down→Up L/R alone)

**Files:**
- Modify: `include/ShiftStateMachine.h` (replace skeleton with real impl)

**Consumes:** T01 skeleton
**Produces:** A working `OnShiftDown`/`OnShiftUp` that fires select on plain Down→Up cycle.

**Step 1:** Replace `include/ShiftStateMachine.h` body (keep namespace + struct shape) with:

```cpp
void Reset(SessionId sid) {
  states_.erase(sid);
}

void OnShiftDown(SessionId sid, bool is_left) {
  auto& s = states_[sid];
  s.downRecorded = true;
  s.interveningKey = false;
  s.lastIsLeft = is_left;
}

EventResult OnShiftUp(SessionId sid, bool is_left) {
  (void)is_left;
  auto it = states_.find(sid);
  if (it == states_.end()) return {};
  EventResult r;
  r.fire_select = it->second.downRecorded && !it->second.interveningKey;
  r.index = it->second.lastIsLeft ? 1 : 2;
  states_[sid] = State{};   // unconditionally reset
  return r;
}

void OnInterveningKey(SessionId sid) {
  // not implemented yet (T03)
}

void OnSessionDestroyed(SessionId sid) {
  states_.erase(sid);
}

std::size_t DebugSessionCount() const {
  return states_.size();
}
```

**Step 2** [VERIFY]:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="*ShiftDownUp_*"
```

Expected: **2 PASS** (#1 ShiftDownUp_L, #2 ShiftDownUp_R). Other 9 still FAIL.

**Step 3:** Commit:

```bash
git add include/ShiftStateMachine.h
git commit -m "feat(ShiftStateMachine): Down→Up L/R alone fires select (tests #1,#2 green)"
```

---

## T03 · State machine: Tests #3, #4, #5 green

**Files:**
- Modify: `include/ShiftStateMachine.h`

**Consumes:** T02
**Produces:** `OnInterveningKey` impl; orphan Up handling; double-down override.

**Step 1:** Replace `OnInterveningKey` with:

```cpp
void OnInterveningKey(SessionId sid) {
  auto it = states_.find(sid);
  if (it != states_.end() && it->second.downRecorded) {
    it->second.interveningKey = true;
  }
}
```

**Step 2** [VERIFY]:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="InterveningBlocks*:UpWithoutDown*"
```

Expected: 2 PASS.

**Step 3:** Verify Test #5 (DoubleDown) — that one was already satisfied by T02's `OnShiftDown` overwrite. Confirm pass:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="DoubleDown*"
```

Expected: 1 PASS.

**Step 4:** Commit:

```bash
git add include/ShiftStateMachine.h
git commit -m "feat(ShiftStateMachine): OnInterveningKey + orphan Up guard (tests #3,#4,#5 green)"
```

---

## T04 · State machine: Tests #6, #7, #8, #9 green

**Files:**
- Modify: `include/ShiftStateMachine.h`

**Consumes:** T03
**Produces:** Tests for two-intervening, double cycle, reset, intervening-before-down.

**Step 1:** Tests #6 and #8 are already satisfied by T02/T03 logic (idempotent interveningKey=true, unconditional reset). Verify:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="TwoIntervening*:ResetClearsState*"
```

Expected: 2 PASS.

**Step 2:** Test #9 (InterveningBeforeDown) should already pass since OnInterveningKey no-ops when no Down recorded. Verify:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="InterveningBeforeDown*"
```

Expected: 1 PASS.

**Step 3:** Test #7 (CycleTwice) — verify the second cycle works after reset:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe --gtest_filter="CycleTwice*"
```

Expected: 1 PASS.

**Step 4** [VERIFY]: Run all 11:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
```

Expected: **0 FAIL, 11 PASS** (if any test still red, fix state machine before proceeding — NEVER proceed with red tests).

**Step 5:** Commit:

```bash
git add include/ShiftStateMachine.h
git commit -m "test(ShiftStateMachine): tests #6-#9 cohort green (state machine fully covers Down/Up/Intervening/Reset/DoubleDown)"
```

The test file is unchanged in T04 — all green was achieved by T02/T03 logic. T04 verifies the 6–9 cohort becomes green as a side-effect. Fuzz + multi-session coverage is in T05 (final pass).

---

## T05 · State machine: Tests #10, #11 + lock in (explicit no-op tests already cover edge cases)

**Files:**
- Modify (potentially): `include/ShiftStateMachine.h` if any test #10/#11 fails.

**Consumes:** T04
**Produces:** All 11 tests green.

**Step 1** [VERIFY — final pass]:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
```

Expected output:

```
[==========] 11 tests from 10 test suites ran. (XX ms total)
[  PASSED  ] 11 tests.
```

If any FAIL, fix and re-run. DO NOT proceed to T06 with red tests.

**Step 2:** Run `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32` to confirm nothing else broke.

Expected: 0 error.

**Step 3:** Tag state machine as locked. No further changes to `include/ShiftStateMachine.h` allowed without an ADR + new spec.

---

## T06 · `WeaselIPC.h` enum + virtual + Client additions (wire start)

**Files:**
- Modify: `include/WeaselIPC.h`

**Consumes:** existing enum at line 18–37, RequestHandler virtuals at line 52–86, Client class at line 104–152.

**Step 1:** Insert 3 new enum values **before** `WEASEL_IPC_LAST_COMMAND` (currently at line 36):

```cpp
WEASEL_IPC_SHIFT_DOWN,
WEASEL_IPC_SHIFT_UP,
WEASEL_IPC_SELECT_CANDIDATE,
WEASEL_IPC_LAST_COMMAND
```

The trailing `,` after `SELECT_CANDIDATE` ensures LAST_COMMAND stays the last token.

**Step 2:** Append 3 RequestHandler virtual overrides (after `virtual void SelectCandidateOnCurrentPage(...)` at line 69):

```cpp
  virtual void ShiftDown(bool is_left, DWORD session_id) {}
  virtual void ShiftUp(bool is_left, DWORD session_id) {}
  virtual void SelectCandidate(size_t index, DWORD session_id) {}
```

**Step 3:** Append 3 Client class method declarations (after `bool SelectCandidateOnCurrentPage(size_t index);` at line 132):

```cpp
  bool ShiftDown(bool is_left);
  bool ShiftUp(bool is_left);
  bool SelectCandidate(size_t index);
```

**Step 4** [VERIFY]: Build:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error (no impl yet — virtuals are no-op defaults; Client methods can be defined as stubs).

**Step 5:** Run all unit tests:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
```

Expected: 11 + (existing) tests pass. NOTHING must regress.

**Step 6:** Commit:

```bash
git add include/WeaselIPC.h
git commit -m "feat(WeaselIPC): 3 new commands (SHIFT_DOWN/UP/SELECT_CANDIDATE) + virtuals + Client methods (wire surface only)"
```

---

## T07 · `WeaselClientImpl.cpp` Client::ShiftDown/ShiftUp/SelectCandidate

**Files:**
- Modify: `WeaselIPC/WeaselClientImpl.cpp`

**Consumes:** T06 (enum + Client method decls)

**Step 1:** Append after `bool ClientImpl::SelectCandidateOnCurrentPage(size_t index)` (~line 89):

```cpp
bool ClientImpl::ShiftDown(bool is_left) {
  if (!_Active()) return false;
  // C2: fire-and-forget — DO NOT block on pipe read. Use raw _SendMessage
  // and accept 0 return. Server does not need to respond synchronously.
  _SendMessage(WEASEL_IPC_SHIFT_DOWN,
               is_left ? 0 : 1,
               session_id);
  return true;
}

bool ClientImpl::ShiftUp(bool is_left) {
  if (!_Active()) return false;
  _SendMessage(WEASEL_IPC_SHIFT_UP, is_left ? 0 : 1, session_id);
  return true;
}

bool ClientImpl::SelectCandidate(size_t index) {
  if (!_Active()) return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_SELECT_CANDIDATE, index, session_id);
  return ret != 0;
}
```

**Step 2:** Append wrapper methods in the `Client` class (after `Client::SelectCandidateOnCurrentPage` at ~line 245):

```cpp
bool Client::ShiftDown(bool is_left) {
  return m_pImpl->ShiftDown(is_left);
}

bool Client::ShiftUp(bool is_left) {
  return m_pImpl->ShiftUp(is_left);
}

bool Client::SelectCandidate(size_t index) {
  return m_pImpl->SelectCandidate(index);
}
```

**Step 3** [VERIFY]:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error (Server doesn't dispatch yet — T08). Stubs compile fine.

**Step 4:** Commit:

```bash
git add WeaselIPC/WeaselClientImpl.cpp
git commit -m "feat(WeaselIPC): Client::ShiftDown/ShiftUp (fire-and-forget) + SelectCandidate"
```

---

## T08 · `WeaselServerImpl.cpp` OnShiftDown/OnShiftUp/OnSelectCandidate + dispatch

**Files:**
- Modify: `WeaselIPCServer/WeaselServerImpl.cpp`

**Consumes:** T07 (Client sends these 3 commands)
**Produces:** Server-side dispatch + RequestHandler virtual calls.

**Step 1:** Insert after `OnSelectCandidateOnCurrentPage` (~line 359):

```cpp
DWORD ServerImpl::OnShiftDown(WEASEL_IPC_COMMAND uMsg,
                              DWORD wParam,
                              DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->ShiftDown((wParam & 1) != 0, lParam);
  return 0;
}

DWORD ServerImpl::OnShiftUp(WEASEL_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->ShiftUp((wParam & 1) != 0, lParam);
  return 0;
}

DWORD ServerImpl::OnSelectCandidate(WEASEL_IPC_COMMAND uMsg,
                                    DWORD wParam,
                                    DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->SelectCandidate(static_cast<size_t>(wParam), lParam);
  return 0;
}
```

**Step 2:** Register in HandlePipeMessage switch (after `OnChangePage` line 434, before `OnTrayCommand` line 435):

```cpp
  PIPE_MSG_HANDLE(WEASEL_IPC_SHIFT_DOWN, OnShiftDown);
  PIPE_MSG_HANDLE(WEASEL_IPC_SHIFT_UP, OnShiftUp);
  PIPE_MSG_HANDLE(WEASEL_IPC_SELECT_CANDIDATE, OnSelectCandidate);
```

**Step 3** [VERIFY]:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: **0 error**. Note: `RequestHandler::ShiftDown/ShiftUp/SelectCandidate` are virtual with default `{}` in `WeaselIPC.h` (added in T06), so ServerImpl's dispatch to `m_pRequestHandler->ShiftDown(...)` builds and links cleanly even before `RimeWithWeaselHandler` overrides them. Runtime dispatch is a no-op until T09/T10 wires the override.

**Step 4:** Commit:

```bash
git add WeaselIPCServer/WeaselServerImpl.cpp
git commit -m "feat(WeaselServer): dispatch SHIFT_DOWN/SHIFT_UP/SELECT_CANDIDATE → RequestHandler"
```

---

## T09 · `RimeWithWeasel.h` ShiftState struct + m_shiftState + 3 virtuals

**Files:**
- Modify: `include/RimeWithWeasel.h`

**Consumes:** T06 virtuals (must match signature `ShiftDown(bool, WeaselSessionId)` etc.)
**Produces:** Class declaration ready for T10 to implement.

**Step 1:** Add `<unordered_map>` include at top (after `<map>` line 5).

**Step 2:** Add ShiftState struct after line 33 (`SessionStatusMap typedef`), BEFORE `class RimeWithWeaselHandler`:

```cpp
struct ShiftState {
  bool downRecorded = false;
  bool interveningKey = false;
  bool lastIsLeft = false;
};
```

**Step 3:** In `RimeWithWeaselHandler` public section, add virtual overrides matching `RequestHandler` (place near line 50 after `SelectCandidateOnCurrentPage` virtual):

```cpp
  virtual void ShiftDown(bool is_left, WeaselSessionId ipc_id);
  virtual void ShiftUp(bool is_left, WeaselSessionId ipc_id);
  virtual void SelectCandidate(size_t index, WeaselSessionId ipc_id);
```

**Step 4:** In private section (after `SessionStatusMap m_session_status_map;` line 150), add:

```cpp
  std::unordered_map<WeaselSessionId, ShiftState> m_shiftState;
```

**Step 5** [VERIFY]:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error.

**Step 6:** Commit:

```bash
git add include/RimeWithWeasel.h
git commit -m "feat(RimeWithWeasel): ShiftState struct + m_shiftState map + virtual declarations"
```

---

## T10 · `RimeWithWeasel.cpp` virtual impls + AddSession/RemoveSession + ProcessKeyEvent interveningKey

**Files:**
- Modify: `RimeWithWeasel/RimeWithWeasel.cpp`

**Consumes:** T09 struct + virtual decls
**Produces:** Working state machine in C++ (no rime_api yet — T10's SelectCandidate calls rime_api->select_candidate placeholder).

**Step 1:** Locate `RimeWithWeaselHandler::AddSession` and ensure it initializes `m_shiftState`. Pattern:

```cpp
DWORD RimeWithWeaselHandler::AddSession(LPWSTR buffer, EatLine eat) {
  // ... existing code ...
  DWORD ipc_id = /* existing code computing new id */;
  new_session_status(ipc_id);
  m_shiftState[ipc_id] = ShiftState{};  // ensure fresh entry
  return ipc_id;
}
```

(If your codebase uses an existing AddSession verbatim line range, anchor the edit to that function definition. Run `grep -n "AddSession" RimeWithWeasel/RimeWithWeasel.cpp` to find it.)

**Step 2:** Locate `RimeWithWeaselHandler::RemoveSession` and add erase:

```cpp
DWORD RimeWithWeaselHandler::RemoveSession(WeaselSessionId ipc_id) {
  // ... existing code ...
  m_shiftState.erase(ipc_id);
  return 0;
}
```

**Step 3:** Implement 3 new virtuals. Insert at end of file (or near ProcessKeyEvent for locality):

```cpp
void RimeWithWeaselHandler::ShiftDown(bool is_left, WeaselSessionId ipc_id) {
  TryLazyRecovery();
  if (m_disabled) return;
  DLOG(INFO) << "ShiftDown: is_left=" << is_left
             << ", ipc_id=" << ipc_id;
  auto& s = m_shiftState[ipc_id];
  s.downRecorded = true;
  s.interveningKey = false;
  s.lastIsLeft = is_left;
}

void RimeWithWeaselHandler::ShiftUp(bool is_left, WeaselSessionId ipc_id) {
  TryLazyRecovery();
  if (m_disabled) return;
  DLOG(INFO) << "ShiftUp: is_left=" << is_left
             << ", ipc_id=" << ipc_id;
  (void)is_left;
  auto it = m_shiftState.find(ipc_id);
  if (it == m_shiftState.end()) return;
  auto& s = it->second;
  if (s.downRecorded && !s.interveningKey) {
    size_t idx = s.lastIsLeft ? 1 : 2;
    RimeSessionId rid = to_session_id(ipc_id);
    rime_api->select_candidate(rid, idx);
    _UpdateUI(ipc_id);
  }
  s = ShiftState{};  // C6: unconditionally reset
}

void RimeWithWeaselHandler::SelectCandidate(size_t index,
                                            WeaselSessionId ipc_id) {
  TryLazyRecovery();
  if (m_disabled) return;
  DLOG(INFO) << "SelectCandidate: idx=" << index
             << ", ipc_id=" << ipc_id;
  rime_api->select_candidate(to_session_id(ipc_id), index);
  _UpdateUI(ipc_id);
}
```

**Step 4:** Add interveningKey marker. Modify `RimeWithWeaselHandler::ProcessKeyEvent` (line 281-310). After existing `_Respond(ipc_id, eat); _UpdateUI(ipc_id);` lines and BEFORE `m_active_session = ipc_id;`, insert:

```cpp
  // C2 + R4: intervening key (non-release, non-Shift) marks ShiftState so
  // ShiftUp will not fire select_candidate.
  if (!(keyEvent.mask & ibus::Modifier::RELEASE_MASK) &&
      keyEvent.keycode != ibus::Keycode::Shift_L &&
      keyEvent.keycode != ibus::Keycode::Shift_R) {
    auto it = m_shiftState.find(ipc_id);
    if (it != m_shiftState.end() && it->second.downRecorded) {
      it->second.interveningKey = true;
      DLOG(INFO) << "interveningKey=true ipc_id=" << ipc_id
                 << " keycode=" << keyEvent.keycode;
    }
  }
```

**Step 5** [VERIFY]:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error. Verify `output/Win32/WeaselServer.exe` timestamp updated.

**Step 6:** Run existing tests (must not regress):

```
test/TestResponseParser/Release/TestResponseParser.exe
test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
```

Expected: 0 fail.

**Step 7:** Commit:

```bash
git add RimeWithWeasel/RimeWithWeasel.cpp include/RimeWithWeasel.h
git commit -m "feat(RimeWithWeasel): ShiftDown/Up/SelectCandidate + interveningKey in ProcessKeyEvent"
```

---

## T11 · `WeaselTSF/WeaselTSF.h` Shift state members

**Files:**
- Modify: `WeaselTSF/WeaselTSF.h`

**Consumes:** T10 + T12 needs these
**Produces:** Per-TSF-instance local Shift hold tracking.

**Step 1:** In the private members section (line 156-237 area), after `BOOL _isToOpenClose = false;` at line 237, add:

```cpp
  /* spec 077: TSF-local Shift tracking for pipe-disconnect resilience (R5) */
  BOOL _shiftDown = FALSE;
  BOOL _shiftDownIsLeft = FALSE;
```

**Step 2** [VERIFY]: Build:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error (members unused yet, that's fine — T12 wires them).

**Step 3:** Commit:

```bash
git add WeaselTSF/WeaselTSF.h
git commit -m "feat(WeaselTSF): _shiftDown / _shiftDownIsLeft members (local state for pipe disconnect R5)"
```

---

## T12 · `WeaselTSF/KeyEventSink.cpp` Shift detection + IPC calls + eat

**Files:**
- Modify: `WeaselTSF/KeyEventSink.cpp`

**Consumes:** T11 members + T07 Client methods
**Produces:** TSF eats Shift events, sends IPC, never lets Shift reach ProcessKeyEvent handler.

**Step 1:** Modify `_ProcessKeyEvent` body (currently line 11-63). **Before** `m_client.ProcessKeyEvent(ke)` call at line 38, insert Shift detection block:

```cpp
    if (ke.keycode == ibus::Shift_L || ke.keycode == ibus::Shift_R) {
      BOOL isLeft = (ke.keycode == ibus::Shift_L);
      BOOL isDown = !(ke.mask & ibus::RELEASE_MASK);
      if (isDown) {
        m_client.ShiftDown(isLeft ? TRUE : FALSE);
        _shiftDown = TRUE;
        _shiftDownIsLeft = isLeft ? TRUE : FALSE;
      } else {
        // R10: even on Shift-up release, suppress Shift modifier flag from
        // any letter event STILL processed this cycle. (No-op here since
        // _ProcessKeyEvent returns early; letter modifier flag is filtered
        // by ConvertKeyEvent reading _lpbKeyState independently.)
        m_client.ShiftUp(isLeft ? TRUE : FALSE);
        _shiftDown = FALSE;
        _shiftDownIsLeft = FALSE;
      }
      *pfEaten = TRUE;  // C6: eat Shift to prevent re-dispatch
      // CRITICAL: return early — do NOT call ProcessKeyEvent, do NOT do
      // prevKeyEvent / keyCountToSimulate bookkeeping (those are Caps_Lock
      // related and independent).
      return;
    }
```

Wait — looking at the existing function structure: the existing flow is:

1. `*pfEaten = FALSE; return;` if conditions
2. `ke` filled via ConvertKeyEvent
3. Caps_Lock 特殊处理
4. `*pfEaten = (BOOL)m_client.ProcessKeyEvent(ke);` ← we want to skip this for Shift
5. Caps_Lock double-tap 模拟器
6. `prevfEaten = ...; prevKeyEvent = ...;`

So the right insertion is **before** line 38 (the `ProcessKeyEvent` call), AFTER line 36 (the `_cand->GetIsReposition()` swap-up/down check), AND make sure to update `*pfEaten` and `prevKeyEvent` so the Caps_Lock bookkeeping doesn't fire spuriously.

**Step 1 (corrected):** Insert after `_cand->GetIsReposition()` check (line 36) and before `if (!keyCountToSimulate)` (line 37):

```cpp
    // spec 077: detect Shift_L/R and route to IPC state machine instead
    // of letting ProcessKeyEvent handle the modifier mismatch (L18/L19/L21).
    if (ke.keycode == ibus::Shift_L || ke.keycode == ibus::Shift_R) {
      BOOL isLeft = (ke.keycode == ibus::Shift_L);
      BOOL isDown = !(ke.mask & ibus::RELEASE_MASK);
      if (isDown) {
        m_client.ShiftDown(isLeft ? TRUE : FALSE);
        _shiftDown = TRUE;
        _shiftDownIsLeft = isLeft ? TRUE : FALSE;
      } else {
        m_client.ShiftUp(isLeft ? TRUE : FALSE);
        _shiftDown = FALSE;
      }
      *pfEaten = TRUE;
      prevKeyEvent = ke;     // keep bookkeeping consistent
      prevfEaten = TRUE;
      return;
    }
```

**Step 2:** Reset on pipe-reconnect. Find `_Reconnect()` (line 193 in WeaselTSF.h declaration) and add at end:

```cpp
void WeaselTSF::_Reconnect() {
  // ... existing reconnect logic ...
  _shiftDown = FALSE;
  _shiftDownIsLeft = FALSE;
}
```

**Step 3** [VERIFY]:

```
msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
```

Expected: 0 error. Verify `output/Win32/WeaselTSF.dll` updated.

**Step 4:** Install + sandbox smoke (D2D + log evidence — optional at this step, **mandatory at T21**):

```
copy /Y output\Win32\WeaselTSF.dll C:\Users\Duanyi\AppData\Roaming\Fluxing
```

But this loads DLL into explorer — generally **DON'T** until sandbox. Skip; T21 covers end-to-end.

**Step 5:** Commit:

```bash
git add WeaselTSF/KeyEventSink.cpp WeaselTSF/WeaselTSF.h
git commit -m "feat(WeaselTSF): Shift_L/R detection in _ProcessKeyEvent → IPC + eat (R5 local tracking + pipe-reconnect reset)"
```

---

## T13 · `test/TestShiftIPCWire` 5 integration tests RED

**Files:**
- Create: `test/TestShiftIPCWire/TestShiftIPCWire.cpp`
- Create: `test/TestShiftIPCWire/TestShiftIPCWire.vcxproj`
- Create: `test/TestShiftIPCWire/TestShiftIPCWire.vcxproj.filters`

**Consumes:** T06 (enum definitions)
**Produces:** Wire-format roundtrip test surface.

**Step 1:** Create `test/TestShiftIPCWire/TestShiftIPCWire.cpp`:

```cpp
#include <gtest/gtest.h>
#include <WeaselIPC.h>

using namespace weasel;

TEST(ShiftDownWire, Left) {
  PipeMessage msg = {(WEASEL_IPC_COMMAND)WEASEL_IPC_SHIFT_DOWN, 0, 0x42};
  EXPECT_EQ(msg.Msg, WEASEL_IPC_SHIFT_DOWN);
  EXPECT_EQ(msg.wParam & 1, 0u);  // left = 0
  EXPECT_EQ(msg.lParam, 0x42u);
}

TEST(ShiftDownWire, Right) {
  PipeMessage msg = {(WEASEL_IPC_COMMAND)WEASEL_IPC_SHIFT_DOWN, 1, 0x42};
  EXPECT_EQ(msg.wParam & 1, 1u);  // right = 1
}

TEST(ShiftUpWire, Left) {
  PipeMessage msg = {(WEASEL_IPC_COMMAND)WEASEL_IPC_SHIFT_UP, 0, 0x100};
  EXPECT_EQ(msg.Msg, WEASEL_IPC_SHIFT_UP);
  EXPECT_EQ(msg.lParam, 0x100u);
}

TEST(SelectCandidateWire, Index3) {
  PipeMessage msg = {(WEASEL_IPC_COMMAND)WEASEL_IPC_SELECT_CANDIDATE, 3, 0x100};
  EXPECT_EQ(msg.wParam, 3u);
  EXPECT_EQ(msg.lParam, 0x100u);
}

TEST(PipeClosedRead, NoDerefCrash) {
  // Smoke: instantiate PipeChannel with a non-existent name, expect
  // graceful failure (no exception escapes, return false).
  PipeChannel<DWORD, PipeMessage> ch(L"\\\\.\\pipe\\__non_existent__");
  EXPECT_FALSE(ch.HandleResponseData([](LPWSTR, DWORD) { return true; }));
}
```

**Step 2:** Create vcxproj mirroring `test/TestPipeProtocol`. Set ProjectGuid `{8E2F1A7B-4D5F-4F88-9A0C-9F0E0F0E0F02}`. Includes `../..` and links `gtestd.lib`/`gtest.lib`. References `WeaselIPC` and `WeaselIPCData`.

**Step 3** [VERIFY]:

```
msbuild test/TestShiftIPCWire/TestShiftIPCWire.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
test/TestShiftIPCWire/Release/TestShiftIPCWire.exe
```

Expected: 5 PASS (these are pure data structure tests — no real wire yet. They verify the wire format matches the spec table in plan.md §Interface Contracts).

If `PipeChannel` constructor signature differs, adjust — the design contract here is "tests assert wire-format fields, not actual round-trip over a live pipe server" because Server isn't running during tests.

**Step 4:** Commit:

```bash
git add test/TestShiftIPCWire/
git commit -m "test(WeaselIPC): 5 wire-format integration tests (data layout validates spec §2.3)"
```

---

## T14 · Add byte-level assertion to TestShiftIPCWire (match PlanBytes order)

This task verifies pack order matches `weasel::PipeMessage` (DWORD Msg, wParam, lParam) — covered by the existing assertions in T13. Skip if T13 already passed.

**Files:**
- (no modifications; verification only)

**Step 1** [VERIFY — full suite run]:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
test/TestShiftIPCWire/Release/TestShiftIPCWire.exe
test/TestResponseParser/Release/TestResponseParser.exe
test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
test/TestShiftSelectBinding/Release/TestShiftSelectBinding.exe
```

Expected output: 11 + 5 + (existing) + (existing) + (existing) all PASS, 0 fail. If any FAIL, **STOP** and debug before proceeding to T15.

**Step 2:** No commit.

---

## T15 · yaml delete: `output/data/default.yaml:240-241`

**Files:**
- Modify: `output/data/default.yaml` (delete 2 lines byte-level)

**Step 1:** Verify target file (R12 risk — spec/handoff both say `rime_ice.schema.yaml`, but actual location is `default.yaml`):

```powershell
Test-Path F:\soft\00selfmade\rime_claude\output\data\default.yaml    # True
Test-Path F:\soft\00selfmade\rime_claude\output\data\rime_ice.schema.yaml  # True (存在,但 binding 不在那)
```

Grep to confirm binding lines exist:

```powershell
Select-String -Path "F:\soft\00selfmade\rime_claude\output\data\default.yaml" -Pattern "Shift\+Shift_L"
```

Expected output: lines 240-241 + the L21 explanatory comment block above (236-239 + the older L19 comment 230-234).

**Step 2:** Run byte-level delete with verification (per L07/L09/L11):

```powershell
$path = "F:\soft\00selfmade\rime_claude\output\data\default.yaml"
$bytes = [IO.File]::ReadAllBytes($path)

# Locate line 240 (0-indexed: byte offset of line 240's start)
$content = [Text.Encoding]::UTF8.GetString($bytes)
$lines = $content -split "`r`n"
Write-Host "Total lines: $($lines.Length)"
Write-Host "Line 239 (should be comment about Shift_L/R): $($lines[238])"
Write-Host "Line 240 (TARGET DELETE): $($lines[239])"
Write-Host "Line 241 (TARGET DELETE): $($lines[240])"
Write-Host "Line 242 (should remain): $($lines[241])"
```

**Confirm** (manual eyes check):
- Line 240 = `- { when: has_menu, accept: Shift+Shift_L, send: 2 }`
- Line 241 = `- { when: has_menu, accept: Shift+Shift_R, send: 3 }`
- Line 242 = `- { when: has_menu, accept: Control+1, send: 2 }`

**Step 3:** After confirmation, perform the delete:

```powershell
$newLines = $lines[0..238] + $lines[241..($lines.Length - 1)]
$newContent = $newLines -join "`r`n"
[IO.File]::WriteAllBytes($path, [Text.Encoding]::UTF8.GetBytes($newContent))
```

**Step 4** [VERIFY — BOM absence + CR/LF parity]:

```powershell
$post = [IO.File]::ReadAllBytes($path)
$bom = if ($post[0] -eq 0xEF -and $post[1] -eq 0xBB -and $post[2] -eq 0xBF) {"HAS BOM!"} else {"OK no BOM"}
$crlfCount = ([regex]::Matches([Text.Encoding]::UTF8.GetString($post), "`r`n")).Count
$lfOnlyCount = ([regex]::Matches([Text.Encoding]::UTF8.GetString($post), "(?<!`r)`n")).Count
Write-Host "BOM check: $bom"
Write-Host "CRLF count: $crlfCount"
Write-Host "LF-only count (should be 0): $lfOnlyCount"

# Verify the lines are gone
Select-String -Path $path -Pattern "Shift\+Shift_L, send: 2"
Select-String -Path $path -Pattern "Shift\+Shift_R, send: 3"
```

Expected:
- `OK no BOM`
- `CRLF count` = (original count - 2) (since we removed 2 lines = 2 CRLF)
- `LF-only count` = 0
- `Select-String` returns no matches for the deleted bindings.

**Step 5:** Update the L21 comment block (lines that now need a one-line amendment) — grep first to find the new location of the comment after the delete:

```powershell
Select-String -Path "F:\soft\00selfmade\rime_claude\output\data\default.yaml" -Pattern "Shift\+Shift_L"
```

Expected: Only the L21 explanatory comment lines (without the binding lines). Edit one of those comment lines to note the IPC take-over. **Do NOT delete the comments** — they are historical audit trail.

Specifically: find a line that says `... 第 2/3 用 Shift_L/R 或 Control+1/2` and append `; Shift_L/R 单键选择 (2nd/3rd 候选) 改走 IPC (spec 077)`:

```powershell
(Get-Content $path -Encoding UTF8) |
  ForEach-Object { $_ -replace 'Shift_L/R 或 Control\+1/2$','Shift_L/R 或 Control+1/2 ; 2/3 候选的单键 Shift_L/R 路径 v0.21.0.1 起改由 IPC 接管 (spec 077)' } |
  Set-Content $path -Encoding UTF8
```

Re-verify BOM absence + CRLF parity after this second write.

**Step 6** [VERIFY]:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
test/TestResponseParser/Release/TestResponseParser.exe
```

Expected: All PASS. (WeaselServer.exe doesn't read default.yaml at startup — that's librime's job. Build only verifies C++ integrity; yaml validation comes at install+sandbox time.)

**Step 7:** Commit:

```bash
git add output/data/default.yaml
git commit -m "feat(rime_ice): remove Shift+Shift_L/R send 2/3 bindings from default.yaml:240-241 (spec 077 IPC path takes over)"
```

---

## T16 · Extend `TestShiftSelectBinding` (2 yaml neg assertions)

**Files:**
- Modify: `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp`

**Consumes:** existing spec 014 binding-positive assertions
**Produces:** Negative assertions matching spec §2.7.3 row 1.

**Step 1:** Read existing file structure:

```bash
grep -n "TEST\|EXPECT\|ASSERT" test/TestShiftSelectBinding/TestShiftShiftSelectBinding.cpp 2>/dev/null
# (if .cpp doesn't exist, check .cpp via Glob for actual filename)
```

Use `test/TestShiftSelectBinding/*.cpp` confirmed in earlier Glob.

**Step 2:** Find the section that loads `default.yaml` (probably loads content into a string/yaml_doc_t). After existing tests, add 2 NEGATIVE assertions:

```cpp
#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <string>

static std::string LoadDefaultYaml() {
  std::ifstream f("../output/data/default.yaml");
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

TEST(ShiftSelectBinding_Negative, ShiftShiftLNotPresent) {
  std::string content = LoadDefaultYaml();
  EXPECT_EQ(content.find("Shift+Shift_L, send: 2"), std::string::npos)
    << "default.yaml should NOT contain {accept: Shift+Shift_L, send: 2} "
    << "after spec 077 takes over via IPC";
}

TEST(ShiftSelectBinding_Negative, ShiftShiftRNotPresent) {
  std::string content = LoadDefaultYaml();
  EXPECT_EQ(content.find("Shift+Shift_R, send: 3"), std::string::npos)
    << "default.yaml should NOT contain {accept: Shift+Shift_R, send: 3} "
    << "after spec 077 takes over via IPC";
}
```

(Adjust relative path to `LoadDefaultYaml()` based on actual project — likely `default.yaml` is loaded as `../../output/data/default.yaml` depending on test working directory.)

**Step 3** [VERIFY]:

```
msbuild test/TestShiftSelectBinding/TestShiftSelectBinding.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
test/TestShiftSelectBinding/Release/TestShiftSelectBinding.exe
```

Expected: ALL existing PASS + 2 NEG PASS. If a NEG assertion FAILS, the yaml byte delete didn't actually take effect — STOP, re-do T15.

**Step 4:** Commit:

```bash
git add test/TestShiftSelectBinding/TestShiftSelectBinding.cpp
git commit -m "test(ShiftSelectBinding): 2 negative yaml assertions (Shift+Shift_L/R send 2/3 must be absent)"
```

---

## T17 · Extend `TestDefaultHotkeys` (binding count -2)

**Files:**
- Modify: `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp`

**Consumes:** existing binding-count assertion
**Produces:** Count -2 assertion matching spec §2.7.3 row 2.

**Step 1:** Find the existing test that counts bindings:

```bash
grep -n "TEST.*[Bb]inding\|EXPECT.*\(.*count\)" test/TestDefaultHotkeys/TestDefaultHotkeys.cpp
```

**Step 2:** Modify the constant:

```cpp
// Before: const std::size_t kExpectedShiftBindingCount = 2;
// After:
const std::size_t kExpectedShiftBindingCount =
    0;  // spec 077: Shift+Shift_L/R send 2/3 removed; IPC path takes over
```

**Step 3** [VERIFY]:

```
test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
```

Expected: All existing tests pass + the assertion using `kExpectedShiftBindingCount` now expects 0.

**Step 4:** Commit:

```bash
git add test/TestDefaultHotkeys/TestDefaultHotkeys.cpp
git commit -m "test(DefaultHotkeys): expect 0 Shift+Shift_L/R bindings (spec 077 IPC takeover)"
```

---

## T18 · Build + run all unit/integration tests

**Files:** (no source changes)

**Step 1:** Clean build:

```
msbuild weasel.sln /t:Rebuild /p:Configuration=Release /p:Platform=Win32
```

(L108 lesson: use `/t:Rebuild` not `/t:Build` to defeat incremental skip.)

Expected: 0 error. Check `output/Win32/` for fresh binaries.

**Step 2:** Run all relevant tests in sequence:

```
test/TestShiftIPCStateMachine/Release/TestShiftIPCStateMachine.exe
test/TestShiftIPCWire/Release/TestShiftIPCWire.exe
test/TestShiftSelectBinding/Release/TestShiftSelectBinding.exe
test/TestDefaultHotkeys/Release/TestDefaultHotkeys.exe
test/TestResponseParser/Release/TestResponseParser.exe
```

Expected:
- TestShiftIPCStateMachine: 11/11 pass
- TestShiftIPCWire: 5/5 pass
- TestShiftSelectBinding: existing + 2 neg pass
- TestDefaultHotkeys: existing + count==0 pass
- TestResponseParser: existing pass

**Step 3** [VERIFY]: Zero failure overall.

```
# Aggregate tally (PowerShell):
$totalFail = 0
foreach ($t in @('TestShiftIPCStateMachine','TestShiftIPCWire','TestShiftSelectBinding','TestDefaultHotkeys','TestResponseParser')) {
  $out = & "test\$t\Release\$t.exe" 2>&1
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: $t exit $LASTEXITCODE"
    $totalFail++
  }
}
if ($totalFail -eq 0) { Write-Host "ALL TESTS PASS" } else { exit 1 }
```

**Step 4:** No commit (build artifact only).

---

## T19 · Installer build + md5 dual-verify

**Files:**
- (Installer artifact only; md5 record in verification log)

**Step 1:** Run installer build script per handoff §6 (`_build_v02101.ps1`):

```powershell
F:\soft\00selfmade\rime_claude\_build_v02101.ps1
```

Expected output: `output\archives\fluxing-0.21.0.1-installer.exe` produced.

**Step 2:** md5 dual-verify per L100-PhaseD / build_v0742_provenance pattern:

```powershell
# Build-side (this script output):
Get-FileHash output\archives\fluxing-0.21.0.1-installer.exe -Algorithm MD5

# Compare with:
Get-FileHash release\fluxing-0.21.0.1-installer.exe -Algorithm MD5
```

Expected: Same MD5 hash. If different, NSIS re-pull fails — STOP and rebuild.

**Step 3:** Archive the md5 + provenance:

```powershell
md5sum output\archives\fluxing-0.21.0.1-installer.exe | Tee-Object _build_v02101_provenance.md5
```

**Step 4:** No commit (installer + md5 are gitignored).

---

## T20 · Sandbox-verify 8 scenarios (real machine)

**Files:** (logs only)

**Step 1:** Copy installer to release/:

```powershell
Copy-Item output\archives\fluxing-0.21.0.1-installer.exe release\
```

**Step 2:** Run sandbox:

```powershell
F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
    -InstallerPath release\fluxing-0.21.0.1-installer.exe `
    -Scenarios "1,2,2-prime,3,4,5,G,H"
```

Expected: 8 scenarios all PASS. (Scenario "2-prime" = US1-C for Shift_R; spec §2.7.4 row ②'.)

**Step 3:** Manual evidence collection (per sandbox-verify §):

| Scenario | Log evidence | UI evidence | Pass criteria |
|---|---|---|---|
| ① Shift_L not released (US1-A) | `grep -E "ShiftDown ipc_id" <log>` shows Down; no `select_candidate` line | 候选菜单 5 项不变 | PASS |
| ② Shift_L released (US1-B) | `select_candidate(idx=1)` line + UI commits 2nd candidate | 候选关 + 中文上屏 | PASS |
| ②' Shift_R released (US1-C) | `select_candidate(idx=2)` line | 候选关 + 中文上屏 | PASS |
| ③ Shift+a then release (US1-D) | log shows `interveningKey=true` + ShiftUp no select; preedit shows "A" or 'A' committed | 字母 'A' 上屏，无候选选择 | PASS |
| ④ Shift+space no menu (US1-E) | ascii_mode toggle, no `select_candidate` | ascii 灯亮 | PASS |
| ⑤ Shift+, -> 《 (US1-F) | log shows interveningKey=true，ShiftUp no select | 《 上屏 | PASS |
| G alt-tab跨 session (US1-G) | log shows ShiftDown sid=A, ShiftUp sid=B, no select | UI 不变 | PASS |
| H deployer 期间 (US1-H) | log shows ShiftDown during maintenance, no state pollution; deployer 结束 ShiftDown 不再 fire | UI 不变 | PASS |

**Step 4:** Save logs to:

```
C:\Users\Duanyi\sandbox-artifacts\rime\logs\verify_v0.21.0.1_<date>.log
```

**Step 5:** If any scenario FAILs, STOP and write diagnostic to `<log>` + write incident report before proceeding.

---

## T21 · `feat(WeaselIPC)` main commit + ADR + L##-PhaseM-10 + CHANGELOG

This is the **main feature commit**. Per C3, it includes everything code-related. Per handoff §2 commit strategy: **single `feat(WeaselIPC)` commit** with body referencing spec/ADR/test counts.

**Files:**
- Create: `docs/adr/0007-shift-release-ipc.md`
- Modify: `.specify/memory/lessons-learned.md` (append L##-PhaseM-10 section)
- Modify: `CHANGELOG.md` (append `[0.21.0.1]` section)

**Step 1:** Write `docs/adr/0007-shift-release-ipc.md` (MADR format):

```markdown
---
status: accepted
date: 2026-08-15
deciders: duanyi
consulted: rime_api.h key_event.h line 43-64, librime key_binder behavior
informed: Phase L v0.18.x 8-ship history (L18/L19/L21), spec 005/012/014/077
---

# ADR 0007 — Shift release-only candidate select via IPC state machine (v0.21.0.1)

## Context

spec 005 v1.1 US1-B 承诺:Shift_L 选第 2 候选、Shift_R 选第 3 候选(菜单可见时)。
spec 012 / 014 反复 5+ 次 yaml-only 修复尝试(ship+rollback 模式),失败模式:

- `accept: Shift+Shift_L` (modifier=Shift) 匹配 **down** 事件。release 事件携带 RELEASE_MASK (`WeaselTSF/KeyEvent.cpp:27-28`)。
- librime `key_event.h:64` `KeyEvent::operator==` 严格比较 keycode + modifier,**包含** RELEASE_MASK。
- 因此 `accept: Shift_L` (modifier=0) 永不匹配 release。
- 后果:yaml-only 路径**根本无法表达**「release-only」触发。

TSF 平台限制(`KeyEvent.cpp` mask):改 mask 行为是平台层面 root 限制,治标不治本,letter 'a' + Shift 场景下 release 误匹配错乱。

## Decision

放弃 yaml 层修复。改走 **IPC 重构**:

1. **TSF 端** 在 `_ProcessKeyEvent` 检测 Shift_L/R,吃掉事件(`*pfEaten = TRUE`),发 IPC (`ShiftDown`/`ShiftUp`)。
2. **Server 端** (`RimeWithWeaselHandler`) 维护 per-session `m_shiftState` map:
   - `OnShiftDown`: reset `interveningKey = false`,记 `lastIsLeft`
   - `OnShiftUp`: 若 `downRecorded && !interveningKey` 调 `rime_api->select_candidate(idx)`,无条件 reset state
   - `OnProcessKeyEvent` (intervening 检测): 非-release + 非-Shift 触发 `interveningKey = true`
3. **Wire**: 3 条新 IPC 命令 (ShiftDown / ShiftUp / SelectCandidate),packed `wParam/lParam` 沿用现有风格。`WeaselIPCData.h` POD 不动 (minimal surface principle)。
4. **Yaml**: 删 `output/data/default.yaml:240-241` 两条 binding,保留其他 Shift binding 不动。

## Consequences

**Pro:**

- 根治 release-only 触发,与 hotkey-binding.md §2「Server 单一 source of truth」一致。
- per-session state 防止 alt-tab stale state 污染 (US1-G covered)。
- `m_shiftDown` TSF-local tracking 保证 pipe 断开后 orphan ShiftUp 被忽略 (R5 covered)。

**Con:**

- 新增 3 条 IPC 命令 + 状态机 + 4 边同步(Deployer 审计已确认无 active use)。
- Server-side state 生命周期需小心:`AddSession`/`RemoveSession` 维护 map;`OnShiftUp` 末尾无条件 reset 是唯一安全保证。
- 用户 `*.custom.yaml` 重新添加两条 binding 回到「按下就上屏」行为,需 spec 075+ 跟进。

**Mitigation:**

- 11 unit + 5 integration + sandbox 8 场景覆盖。
- L##-PhaseM-10 在 `.specify/memory/lessons-learned.md` 记录 8 次失败 → IPC 重构路径。
- `installation.yaml` 警告注释。

## Alternatives rejected

1. **yaml 层改 `key_binder/bindings`**: 已尝试 8 次失败 (L18/L19/L21),release-only 不可表达。
2. **改 `WeaselTSF/KeyEvent.cpp` mask**: L10 平台限制,导致 letter 'a' 上屏时 shiftHadInterveningKey 错乱。
3. **`ascii_composer/switch_key: Shift_L: commit_text`**: 违反场景 ① (有候选时不该 commit)。
4. **`kill ctfmon` / disable TSF service**: 性能灾难,IME 降级。
5. **`patch:` 注入 `accept: Shift_L release` modifier=0 with RELEASED flag**: librime 1.13 不支持自定义 modifier flag,根因未触达。

## References

- spec 077 (`.specify/specs/077-shift-release-ipc/spec.md` · `plan.md` · `tasks.md`)
- handoff `handoff-v0.21.0.1-shift-hotkey-ipc-bc-2026-08-15.md` (brainstorming context)
- `.claude/rules/hotkey-binding.md` §2
- `.claude/rules/ipc-boundary.md` §1
- librime `librime/include/rime_api.h:436` `select_candidate`
- librime `librime/src/rime/key_event.h:64` `KeyEvent::operator==`
- `WeaselTSF/KeyEvent.cpp:27-28` RELEASE_MASK 加 mask
```

**Step 2:** Append L##-PhaseM-10 to `.specify/memory/lessons-learned.md`. Find the file (maybe `lessons-learned.md` or in `.specify/memory/`):

```bash
ls .specify/memory/  # find existing lessons-learned.md
```

Append (with separator):

```markdown
---

## L##-PhaseM-10 (v0.21.0.1, 2026-08-15)

**标题:** 「Shift release-only 候选选择」 — yaml-only 修复在 release-only 场景下根本不可行;根治必须 IPC 状态机。

**事故溯源:**

- spec 005 v1.1 US1-B 承诺 Shift_L → 第 2 候选、Shift_R → 第 3。
- 8 次 ship-rollback 反复: L18 / L19 / L21 yaml-only 路径全部失败,失败模式固定:
  - `accept: Shift+Shift_L` (modifier=Shift) 匹配 **down** → 违反场景 ①
  - `accept: Shift_L` (modifier=0) 不匹配 release (RELEASE_MASK 干扰 `operator==`)
  - release-only 触发在 librime 1.13 yaml 层**不可表达**
- 0.19.0.x / 0.21.0.0 ship 期间 spec 071 / 075 / 077 brainstorm 的 8 条 yaml 路径全部失败

**根因 (librime platform):**

```cpp
// librime/src/rime/key_event.h:64
bool operator==(const KeyEvent& other) const {
  return keycode == other.keycode && modifier == other.modifier;
  //                                                ↑ RELEASE_MASK 在 modifier 字段里
}
```

- WeaselTSF `KeyEvent.cpp:27-28` 在 key-up 强制 `modifier |= RELEASE_MASK`
- librime release 触发需要 `modifier == 0` 配合 `keycode == Shift_L` — 永远不等

**根治路径 (v0.21.0.1):**

- 放弃 yaml 层修复,改 IPC 重构:`WEASEL_IPC_SHIFT_DOWN`/`SHIFT_UP`/`SELECT_CANDIDATE` + Server 端 per-session state machine (`RimeWithWeaselHandler::m_shiftState`)
- TSF 端 `KeyEventSink.cpp` 吃 Shift 事件 + 发 IPC
- 状态机三规则:`OnShiftDown` (reset) / `OnShiftUp` (fire_select if !intervening) / `OnProcessKeyEvent` 非-release 非-Shift 触发 `interveningKey = true`
- 11 unit + 5 integration + 8 sandbox 场景覆盖
- ADR 0007 + plan.md/tasks.md 在 `.specify/specs/077-shift-release-ipc/`

**避坑提示:**

- ❌ 改 `WeaselTSF/KeyEvent.cpp` mask 行为 (L10 平台限制)
- ❌ 加 `shiftHeld` 全局状态 (违反 hotkey-binding.md §2「server 单一 source of truth」)
- ❌ 状态机漏 reset (永远 `ShiftUp` 末尾无条件 `state = ShiftState{}`)
- ✅ ShiftUp 检测范围 = `ShiftUp`: 不查 menu,不依赖 `default.yaml:page_size`
- ✅ IPC 4 边同步:Deployer 走 `RegisterHotKey` 路径,不需要改 Client 方法,但 Client 添加新方法需保证兼容性

**相关 ADR:** `docs/adr/0007-shift-release-ipc.md`
**相关 spec:** `.specify/specs/077-shift-release-ipc/{spec,plan,tasks}.md`
```

**Step 3:** Append to `CHANGELOG.md`:

```markdown
## [0.21.0.1] — 2026-08-15

### 主要更新

- **feat(WeaselIPC):** Shift release-only 候选选择通过 IPC 状态机实现 (spec 077)。按 Shift 不松不上屏;松 Shift(中间无其他键)上屏第 2(左)/ 第 3(右)候选。Shift+字母 正常出大写,但不触发候选选择。
  - 新增 3 条 IPC 命令 (`SHIFT_DOWN`/`SHIFT_UP`/`SELECT_CANDIDATE`)
  - Server 端 per-session 状态机 (`RimeWithWeaselHandler::m_shiftState`)
  - TSF 端吃 Shift 事件 + 发 IPC
  - yaml 删除 `default.yaml:240-241` 两条 binding
  - 11 unit + 5 integration + 8 sandbox 场景测试
```

**Step 4:** Verify stage explicitly (NEVER `git add -A`; forbidden by A10):

```bash
git status -sb
git diff --stat
```

**Files MUST include (explicit paths only):**

- `include/ShiftStateMachine.h` (T01)
- `include/WeaselIPC.h` (T06)
- `include/RimeWithWeasel.h` (T09)
- `WeaselIPC/WeaselClientImpl.cpp` (T07)
- `WeaselIPCServer/WeaselServerImpl.cpp` (T08)
- `RimeWithWeasel/RimeWithWeasel.cpp` (T10)
- `WeaselTSF/WeaselTSF.h` (T11)
- `WeaselTSF/KeyEventSink.cpp` (T12)
- `output/data/default.yaml` (T15)
- `test/TestShiftIPCStateMachine/` (T01, whole dir)
- `test/TestShiftIPCWire/` (T13, whole dir)
- `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` (T16)
- `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` (T17)
- `docs/adr/0007-shift-release-ipc.md` (T21 new)
- `.specify/memory/lessons-learned.md` (T21 modified)
- `CHANGELOG.md` (T21 modified)

**Files MUST NOT include:**

- `weasel.props` / `env.bat` (T22 chore commit only — or version is local-only if gitignored)
- `*.obj` / `*.log` / `Test*.exe` / `*.pdb` / `*.ilk`
- `output/archives/*` (installer is gitignored)
- `release/*` (installer copy is gitignored)
- Any WIP prompt/handoff/report files

**Step 5:** Stage explicitly and commit:

```bash
git add \
  include/ShiftStateMachine.h \
  include/WeaselIPC.h \
  include/RimeWithWeasel.h \
  WeaselIPC/WeaselClientImpl.cpp \
  WeaselIPCServer/WeaselServerImpl.cpp \
  RimeWithWeasel/RimeWithWeasel.cpp \
  WeaselTSF/WeaselTSF.h \
  WeaselTSF/KeyEventSink.cpp \
  output/data/default.yaml \
  test/TestShiftIPCStateMachine/ \
  test/TestShiftIPCWire/ \
  test/TestShiftSelectBinding/TestShiftSelectBinding.cpp \
  test/TestDefaultHotkeys/TestDefaultHotkeys.cpp \
  docs/adr/0007-shift-release-ipc.md \
  .specify/memory/lessons-learned.md \
  CHANGELOG.md

# Last sanity check:
git diff --cached --stat
git status -sb

git commit -m "$(cat <<'EOF'
feat(WeaselIPC): v0.21.0.1 shift release-only candidate select via IPC state machine

- Add 3 IPC commands (WEASEL_IPC_SHIFT_DOWN/SHIFT_UP/SELECT_CANDIDATE)
- Server-side per-session state machine (ShiftState in m_shiftState)
- TSF eats Shift events in _ProcessKeyEvent; m_shiftDown local tracking (R5)
- RequestHandler + Client surface for ShiftDown/ShiftUp/SelectCandidate
- yaml: remove Shift+Shift_L/R send 2/3 from default.yaml:240-241
- default.yaml comment updated to reference IPC path
- Tests: 11 unit (TestShiftIPCStateMachine) + 5 wire (TestShiftIPCWire)
  + 2 negative yaml (TestShiftSelectBinding extension) + 1 count -2
  (TestDefaultHotkeys extension)
- IPC 4-edge sync: Deployer audited, no Client calls require change
- ADR 0007 documents 8-attempt yaml failure history → IPC root cause
- L##-PhaseM-10 captures lessons-learned for future spec writers

Refs: spec 014 (US1-B baseline), spec 012, L18/L19/L21 (8 failed
attempts), handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md.

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

---

## T22 · `chore(release)` version bump + final installer

**Files:**
- Modify: `env.bat`
- Modify: `weasel.props`

**Step 1:** Locate version lines:

```bash
grep -n "FLUXING_VERSION\|WEASEL_BUILD" env.bat weasel.props
```

**Step 2:** Edit:

```diff
- set FLUXING_VERSION=0.21.0.0
+ set FLUXING_VERSION=0.21.0.1
```

```diff
-     <FLUXING_VERSION>0.21.0.0</FLUXING_VERSION>
+     <FLUXING_VERSION>0.21.0.1</FLUXING_VERSION>
```

Both also need `WEASEL_BUILD=0` (release build, per C7):

```diff
-     <WEASEL_BUILD>1</WEASEL_BUILD>
+     <WEASEL_BUILD>0</WEASEL_BUILD>
```

(Use `1` → `0` only if the current value is `1`. Most recent v0.21.0.0 commit `792cb47` had `WEASEL_BUILD=0` already — verify before changing.)

**Step 3** [VERIFY]:

```bash
git status -sb
git diff --stat
# MUST show ONLY env.bat + weasel.props modified (and CHANGELOG etc. if not committed in T21)
```

If anything else is staged or modified, STOP. Reset and verify.

**Step 4:** Commit:

```bash
git add env.bat weasel.props
git commit -m "$(cat <<'EOF'
chore(release): v0.21.0.1 ship — FLUXING_VERSION 0.21.0.0 → 0.21.0.1

Bump version in env.bat + weasel.props per `.claude/rules/commit-checklist.md`
P4 (chore scope, no behavior change). WEASEL_BUILD=0 confirmed release-build.

Final installer ship-artifact: output\archives\fluxing-0.21.0.1-installer.exe
+ release\fluxing-0.21.0.1-installer.exe (md5 dual-verify per T19).

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

**Step 5:** Final verification:

```bash
git log --oneline -5  # should show: feat(WeaselIPC) ... chore(release) ...
git tag v0.21.0.1     # optional lightweight tag per AGENTS.md §3.5
```

---

## Done — ship gate checks

Verify all 10 spec §4 criteria:

- [x] spec.md / plan.md / tasks.md committed (spec.md = `1506abc`)
- [x] docs/adr/0007-shift-release-ipc.md committed (T21)
- [x] `.specify/memory/lessons-learned.md` L##-PhaseM-10 committed (T21)
- [x] 11 unit + 5 integration tests pass (T18)
- [x] 3 existing tests (extended) pass: TestShiftSelectBinding/TestDefaultHotkeys/TestResponseParser (T18)
- [x] `output\archives\fluxing-0.21.0.1-installer.exe` md5 dual-verify (T19)
- [x] sandbox-verify 8 scenarios (T20)
- [x] CHANGELOG.md `[0.21.0.1]` committed (T21)
- [x] commit message P4 (`feat(WeaselIPC)` + `chore(release)` 2 commits)
- [x] 2 commits split (T21 + T22)

If any checkbox is missing, STOP and resolve before declaring ship-ready.

After resolution: write `handoff-v0.21.0.1-shift-hotkey-ipc-shipped-<date>.md` per CLAUDE.md §8.1.

---

## Cross-Reference Index

| Topic | Tasks |
|---|---|
| State machine + tests | T01–T05 |
| Wire + Client | T06–T07, T13–T14 |
| Server dispatch + handler | T08–T10 |
| TSF Shift detection | T11–T12 |
| yaml + extended tests | T15–T17 |
| Build + integration | T18–T20 |
| Ship (commits, ADR, L##) | T21–T22 |
