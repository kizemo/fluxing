# 0014. The front end stops reporting the CapsLock latch state as a key modifier

- **状态**: Proposed
- **日期**: 2026-10-28
- **取代**: ADR 0011 Q2（未决问题）——由本 ADR 回答
- **关联**: spec 085、任务总账 T20

## Context

The task ledger records **T20**: with CapsLock on, the input method's hotkeys
appear to stop working entirely.

The first hypothesis was "the bindings do not match any more". Measuring it
turned out to be wrong, and worse than that:

1. The front end puts the **CapsLock latch state** into the key event's
   modifier field whenever the latch is on.
2. The modifier survives the hand-off to the input engine unchanged.
3. The engine compares key events **exactly** (key code *and* modifier must be
   equal), so one extra modifier bit makes the binding miss.
4. The engine's ASCII composer reads that bit. With this project's
   configuration (`good_old_caps_lock: true`) it answers **`kRejected`**.
5. **`kRejected` breaks the whole processor chain** in the engine's key
   dispatch (`if (ret == kRejected) break;`), so the candidate selector, the
   pager and the hotkey binder never see the key.

So the failure is not "some bindings miss"; it is "**the engine stops
processing every key** while the latch is on". That is why the symptom reads
as "everything is dead".

That branch exists to serve the CapsLock *key* itself. On the platforms the
engine normally runs on, the latch state is carried by the **case of the
produced character**, not by the modifier field of ordinary key events, so
ordinary keys never reach it. Our front end reported both, and hit a path
that is meant for the latch key alone.

The gate did not catch it, for a reason worth recording: the gate enumerates
the front end's reachable states over vkey / extended / shift / ctrl / alt /
key-up — and **never sets the CapsLock latch bit**. The dimension in which
the defect lives did not exist in the gate at all.

ADR 0011 Q2 offered three candidates and required measuring before choosing:
drop the lock bit in the front end, keep it and make the capture emit `Lock+`,
or extend the gate to enumerate CapsLock states.

## Decision

**We take the first and the third candidate, and reject the second.**

- **D1 — The front end stops reporting the latch state.** The CapsLock
  latch is a persistent hardware state, not a modifier of the key being
  pressed. It is removed from the key event's modifier field: neither set on
  ordinary keys nor toggled back on the latch key itself.
  *Both* sites go together — the set and the toggle-back are an
  `|=`/`^=` pair, and leaving the `^=` alone would make the latch key carry
  a bit that was never set (`0 ^ LOCK == LOCK`), which silently flips
  "switch to English" into "switch to Chinese".

- **D2 — The latch key itself is unchanged.** It is still reported as its own
  key. Its two consequences — switching the input mode from Chinese, and
  handing the key back to the system while already in English — stay exactly
  as they are today. The latch state is removed from the *modifier field
  only*; the latch key's own behaviour is untouched.

- **D3 — The character keeps the case.** Character resolution is not
  touched: with the latch on, a letter key still produces its upper-case
  character. The case travels with the character, not with the modifier
  field, which is the whole point.

- **D4 — The gate learns the dimension, and can be wrong.** The hotkey gate
  enumerates the latch on and latch off states separately, requires every
  non-letter binding to stay reachable with the latch on, and keeps a
  **negative control**: a second reachability model that puts the latch bit
  back, which must score strictly worse. If the negative control ever stops
  scoring worse, the gate fails — it has become blind to this class of
  defect and says so.

- **D5 — Letter bindings stay a documented limitation.** With the latch on,
  a letter key produces the upper-case character, so a binding written with
  the lower-case character cannot fire in that state. This is the semantics
  of the latch, not a defect in the front end. The way around it is to spell
  the upper-case variant in the configuration. We do **not** attempt to make
  one binding match both cases.

- **D6 — The engine is not touched.** The engine is read-only reference
  material (constitution P3). The fix lives entirely in the front end.

## Consequences

- **What gets fixed.** With the latch on, the hotkeys that matter — picking
  candidates, paging, switching input mode, switching punctuation, switching
  between simplified and traditional — work again, exactly as they do with
  the latch off.

- **What the user may notice.** The latch key still switches the input mode
  from Chinese to English, as before. Nothing about the mode switching
  changes.

- **What stays broken on purpose.** A binding written with a lower-case
  letter (`Control+a`) does not fire while the latch is on. Users who need
  it write the upper-case variant (`Control+A`) in their configuration. This
  is D5, and the task ledger records it as a known limitation rather than an
  open bug.

- **What is no longer expressible.** A binding that explicitly asks for the
  latch modifier (`Lock+X`) never fires, because the front end does not
  report that modifier any more. No shipped configuration used such a
  binding, so nothing regresses; the option is closed rather than kept
  half-working.

- **The gate's baseline.** The hotkey gate now carries a second set of
  numbers (reachable counts with the latch on, and the negative control's
  counts). The existing baseline numbers — bindings, reachable, dead,
  mismatches, unused — are unchanged by D1, because they describe the latch
  off state. That invariance is asserted, not assumed.

- **Risk of the change itself.** Small and bounded: two deleted lines in one
  function, plus a gate that now fails when the front end reintroduces the
  bit. The engine's own behaviour on the latch key is unchanged (D2), and
  the case of produced characters is unchanged (D3).

## Evidence

The gate now reports the latch-on dimension next to the unchanged baseline.
Each row is one gate run; the third column is the fixed front end, the
fourth is the built-in negative control (the latch bit put back):

| run | baseline (latch off) | latch on (after the fix) | negative control |
|---|---|---|---|
| 源门禁 · shipped configuration | `bindings=39 / reachable=39 / dead=0 / mismatches=0 / unused=0` | `caps_bindings=39 / caps_reachable=39 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=0` | `caps_legacy_reachable=0 / caps_legacy_dead=39` |
| 负控 fixture · negative-controls | `bindings=9 / reachable=3 / dead=6 / mismatches=0 / unused=0` | `caps_bindings=6 / caps_reachable=5 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=1` | `caps_legacy_reachable=0 / caps_legacy_dead=6` |
| 抓键 fixture · recorder-output | `bindings=21 / reachable=20 / dead=1 / mismatches=0 / unused=0` | `caps_bindings=21 / caps_reachable=20 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=1` | `caps_legacy_reachable=0 / caps_legacy_dead=21` |
| 部署产物 · 装器载荷 `output/data/build/*.yaml` | `bindings=337 / reachable=337 / dead=0 / mismatches=0 / unused=0` | `caps_bindings=337 / caps_reachable=337 / caps_dead=0 / caps_limited=0 / caps_baseline_dead=0` | `caps_legacy_reachable=0 / caps_legacy_dead=337` |

The negative control scores `caps_legacy_reachable=0` everywhere: with the
latch bit put back, **every** binding dies. That is what makes D4
falsifiable — if the control ever stops scoring worse, the gate says so
instead of passing silently.

## Open questions

- Should the gate suggest the upper-case variant (D5) when it finds a letter
  binding? Useful, but it is a different feature; it is not part of this
  decision.
- If a future engine version changes the ASCII composer's handling of the
  latch bit, the gate's model of that behaviour has to be updated with it.
  The model cites the upstream file and line it was derived from, so the
  update is a deliberate, checkable act rather than a silent drift.
