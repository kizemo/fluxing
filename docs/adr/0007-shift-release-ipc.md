# 0007. Shift release-only candidate select via IPC state machine

- **Status**: Accepted
- **Date**: 2026-08-16
- **Deciders**: Fluxing maintainers
- **Related**: spec 077, lessons L18, L19, L21, plan.md §Risk Register

## Context

The `librime` `KeyEvent::operator==` (`librime/include/rime/key_event.h:64`)
strict-compares `keycode + modifier`. A TSF release event for `Shift_L`
arrives with `keycode=Shift_L, modifier=Release` — which never matches a
yaml binding whose accept form is `Shift+Shift_L` (modifier=Shift). The
consequence: the binding row that is supposed to fire on
release-of-Shift_L never fires. Eight yaml-only attempts to fix this
(spec 012 / 014 / 018 / 019 / L18 / L19 / L21) failed because every
attempt was ultimately a yaml-side workaround for a C++ engine bug.

Three architectural responses were on the table:

1. **Patch librime** — modify `KeyEvent::operator==` to allow release to
   match `modifier=Shift` (or relax `accept: Shift+Shift_L` parsing).
   This is a librime submodule change, forbidden by `constitution §P3`
   and the upstream-PR waiver from `AGENTS.md` §8.
2. **Surface the bug at the TSF boundary** — never let Shift events
   reach librime at all. Route Shift down/up through a Server-side
   state machine and have the Server call `rime_api->select_candidate`
   on a clean ShiftUp (no intervening non-Shift key between Down and Up).
3. **Continue yaml-only** — keep adding workarounds; eventually ship
   one that "feels right" but ignores the root cause.

## Decision

We adopt option 2: **3 new IPC commands** (`WEASEL_IPC_SHIFT_DOWN`,
`SHIFT_UP`, `SELECT_CANDIDATE`) plus a **Server-side per-session state
machine** that drives `rime_api->select_candidate` on a clean ShiftUp.
The TSF eats the Shift events (`*pfEaten = TRUE` + early return) so
librime never sees them; the binding-row path is replaced by the
state-machine path.

Three locked interface contracts:

- `WEASEL_IPC_SHIFT_DOWN` / `SHIFT_UP` carry `wParam bit0 = is_left`
  (0=L,1=R) + `lParam = WeaselSessionId`. They are **fire-and-forget**
  (`Client::_SendMessage` without `channel.Transact`) so the TSF thread
  cannot block on the pipe read (per `constitution §P2`).
- `WEASEL_IPC_SELECT_CANDIDATE` carries `wParam = 0-based index`. It is
  reserved for future scripted/test paths and currently is not invoked
  by the TSF.
- `ShiftUp` ALWAYS resets state at the end (`s = ShiftState{}`) — the
  unconditional-reset invariant (plan §C6). This prevents subsequent
  Shift events from misfiring after a partial cycle.

The state machine lives on the Server (per `hotkey-binding.md`
single-source-of-truth rule). TSF holds only a per-instance local
`_shiftDown` / `_shiftDownIsLeft` for pipe-disconnect resilience
(R5 mitigation); the canonical state is on the Server.

The two has_menu yaml bindings (`Shift+Shift_L send 2`,
`Shift+Shift_R send 3`) at `default.yaml:240-241` are deleted
byte-level (per L07/L09/L11 rules — BOM absence + CRLF parity
preserved). Control+1/2 fallback bindings remain in place.

## Consequences

- ✅ Root cause eliminated at the TSF/librime boundary; no submodule
  change, no PPL churn, no user-data migration.
- ✅ Per-session state means alt-tab does not corrupt the state of a
  different session (`OnSessionDestroyed` erases the map entry).
- ⚠️ Any future work that wants to act on Shift+L/R combo keys (e.g.
  Shift+l lowercase toggle — out of scope per spec §3) needs to
  bypass the state machine; the unconditional reset on `ShiftUp` means
  the state machine will not surface release events upstream.
- ⚠️ Maintenance window (`m_disabled`) short-circuits the three
  virtuals. This is the right behavior for shift events, but means
  any future debug log must check the `if (m_disabled) return;` paths
  to interpret why a `select_candidate` log line is missing.
- ⚠️ The deletion of the yaml binding rows means a user's
  `*.custom.yaml` that re-introduces `Shift+Shift_L send 2` would
  silently return to the L19 "按下就上屏" misbehavior. Detection is
  by `TestShiftSelectBinding` (F1-NEG) + `TestDefaultHotkeys` (binding
  count pin = 44) — any custom yaml that flips these tests is a
  regression.

This decision is **reversible** at the cost of re-introducing the
yaml binding rows; if a future librime version adds a `Modifier::Either`
flag (a real upstream fix), the entire state machine becomes a thin
fallback.