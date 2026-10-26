# 0010. Bare Shift toggles Chinese/English when no candidate menu is open

- **Status**: Proposed
- **Date**: 2026-10-26
- **Deciders**: Fluxing maintainers
- **Related**: ADR 0007, ADR 0009, spec 077, spec 081, lessons L18, L19, L21, L148

## Context

The request: let a **bare Shift press** switch between the Chinese and the
English input mode, the way Sogou and the Windows IME do.

### The yaml path is closed

`WeaselTSF/KeyEventSink.cpp:44-58` eats `Shift_L` / `Shift_R` **entirely**.
It forwards the two halves to the Server over IPC (`m_client.ShiftDown` /
`ShiftUp`) and returns with `*pfEaten = TRUE`, so the event never reaches
`m_client.ProcessKeyEvent` and therefore never reaches librime.

That was ADR 0007's point -- it is the only way candidate selection on
Shift release could work at all. But it has a second consequence that this
ADR has to own:

- **No `key_binder` binding can ever name Shift.** `accept: "Shift_L"`,
  `accept: "Shift_R"` and `accept: "Shift+Shift_L"` are dead *by
  construction*, not by accident (ADR 0009 already recorded this).
- **No `ascii_composer/switch_key` entry for Shift can fire either.**
  `switch_key` is matched *inside* librime, and librime never sees the key.

So the obvious one-line answer -- put `Shift_L: set_ascii_mode` (or
`commit_text`) into `ascii_composer/switch_key` -- is **not reachable in
this fork**. It *was* reachable in v0.21.0.1, which is why the settings
manual still described it; spec 077 closed that path.

### The historical blocker is obsolete

L18 recorded why bare Shift was considered dangerous: a yaml binding
`accept: Shift+...` would also match the **release** half of `Shift+X`
(modifier `Release` versus `Shift`), so `Shift+X` would fire the Shift
binding. That root cause lived in the front end forwarding Shift to
librime. Since ADR 0007 the front end does not forward Shift at all, so
**the release-mismatch class of bug can no longer occur**. The blocker
recorded in L18 no longer applies.

### But bare Shift is already taken

ADR 0007 / spec 077 gave bare Shift a job. `RimeWithWeaselHandler::ShiftUp`
(`RimeWithWeasel/RimeWithWeasel.cpp:1765-1781`) calls
`rime_api->select_candidate(idx)` with `idx = 1` for the left Shift and
`idx = 2` for the right Shift -- "select the 2nd / 3rd candidate". It
shipped in 0.21.0.1 and is in daily use.

The two features therefore collide: **bare Shift is already a candidate
selector.**

### The collision is not a hard conflict

It would be a hard conflict if "select candidate 2" did something visible
when no menu is open. It does not:

- `Context::Select()` returns `false` immediately when the composition is
  empty -- `librime/src/rime/context.cc:113-115`
  (`if (composition_.empty()) return false;`).
- `Segment::GetCandidateAt(index)` returns null for an out-of-range index,
  so `select_candidate(idx=1)` with a single candidate is also a no-op.

So **"select the 2nd candidate" with no menu open is already a no-op** --
the feature occupies exactly the state where "toggle Chinese/English" would
be useful, and does nothing there.

Two more facts make the split safe:

- The `interveningKey` guard (`RimeWithWeasel/RimeWithWeasel.cpp:319-327`)
  is set by any **non-release, non-Shift** keydown between the Shift down
  and the Shift up. So `Shift+X` can never reach the bare-Shift path.
- `ShiftUp` resets the state unconditionally (`s = ShiftState{}`), so
  there is no stale state to leak between cycles.

## Decision

We propose **context disambiguation** in the Server-side state machine
that ADR 0007 introduced:

- **No candidate menu open** -> bare Shift toggles `ascii_mode`
  (Chinese <-> English).
- **Candidate menu open** -> bare Shift keeps its spec 077 behaviour
  (select the 2nd / 3rd candidate). No change.

Nothing is taken away from any shipped feature. The new behaviour only
fills a state that is currently a no-op.

### Alternatives considered

- **Configure it in yaml (`key_binder` / `ascii_composer/switch_key`).**
  Rejected: impossible. The front end eats Shift; librime never sees it.
  No amount of yaml editing changes that.
- **Let the TSF forward Shift to librime again.** Rejected: that is
  precisely the bug ADR 0007 removed. It would re-open the L18
  release-mismatch class and break candidate selection.
- **Replace candidate selection with mode switching.** Rejected: it
  removes a shipped, in-use feature (spec 077, 0.21.0.1) in order to add
  another one. The disambiguation above achieves both.
- **A dedicated chord** (e.g. `Control+Shift+Space`). Viable fallback, but
  it is not what was asked for, and `Shift+space` already toggles
  `ascii_mode` in the shipped configuration -- a second chord for the same
  job adds confusion, not capability.

### Proposed implementation shape

This is a **C++ change, not a yaml change**. Sketch, in
`RimeWithWeaselHandler::ShiftUp`:

```
if (s.downRecorded && !s.interveningKey) {
  // Is a candidate menu open in this session?
  bool has_menu = <status>.is_composing;   // rime_api->get_status
  if (!has_menu && <the new feature is enabled>) {
    bool ascii = rime_api->get_option(session_id, "ascii_mode");
    rime_api->set_option(session_id, "ascii_mode", !ascii);
    _UpdateUI(ipc_id);
  } else {
    size_t idx = s.lastIsLeft ? 1 : 2;
    rime_api->select_candidate(to_session_id(ipc_id), idx);
    _UpdateUI(ipc_id);
  }
}
```

All the required primitives already exist in the product:

- `rime_api->get_option(session_id, "ascii_mode")` /
  `set_option(..., "ascii_mode", ...)` -- already used on the vim-mode
  path at `RimeWithWeasel.cpp:308-310`.
- `RimeStatus::is_composing` / `is_ascii_mode` -- already read at
  `RimeWithWeasel.cpp:967-973`.
- `SetOption()` (`RimeWithWeasel.cpp:596-614`) keeps `m_global_ascii_mode`
  in sync, and `_UpdateUI()` (`RimeWithWeasel.cpp:985-992`) propagates
  `ascii_mode` to the other sessions when global mode is on -- so the
  toggle behaves like every other `ascii_mode` change in the product.

Estimated size: roughly 20 lines plus a configuration switch, a
`TestShiftIPCStateMachine` extension, and this ADR's follow-up spec.

## Consequences

- ✅ Bare Shift becomes a mode switch in the state where it currently does
  nothing, and the spec 077 candidate selection is untouched.
- ✅ It fixes a long-standing inconsistency: the manual promised this
  behaviour, the product could not deliver it, and the manual blamed
  "the release-event modifier comparison bug" -- a root cause that no
  longer exists.
- ⚠️ **The candidate menu consumes the Shift press.** With a menu open,
  Shift selects a candidate and does not switch the mode. The user has to
  close the menu (Esc) first, or use `Shift+space`. This is inherent to
  the disambiguation, not an oversight.
- ⚠️ The toggle is symmetric: a bare Shift in English mode switches back
  to Chinese. That matches Sogou's Shift behaviour, but it means a user
  who habitually taps Shift while typing English text will flip the mode.
  A configuration switch to disable the behaviour is therefore **required**,
  not optional.
- ⚠️ Both Shift keys toggle. Candidate selection distinguishes left
  (2nd) from right (3rd); mode switching cannot, and must not try to --
  a user pressing either Shift expects the same result.
- ⚠️ `ascii_mode` is a **session** option in librime but the product also
  maintains a global mirror. Which one bare Shift flips depends on
  `global_ascii`; the implementation must pick one and state it.
- The decision is **reversible**: it is a branch in one function, guarded
  by a configuration switch. Removing the switch restores today's
  behaviour exactly.

## Open questions for the maintainers

1. Should the toggle be **on by default**? (Recommended: on, with the
   config switch to turn it off -- that is what the user asked for.)
2. When `global_ascii` is off, should bare Shift flip the **current
   session only** or every session? (Recommended: current session only,
   mirroring `Shift+space`.)
3. Should the candidate menu really consume the Shift press, or should a
   **double** Shift (two clean press/release cycles) switch the mode even
   with a menu open? (Recommended: no -- keep the first cycle simple and
   reversible.)
