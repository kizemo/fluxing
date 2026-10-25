# 0009. The TSF front end sends the produced character, not the physical key

- **Status**: Accepted
- **Date**: 2026-10-25
- **Deciders**: Fluxing maintainers
- **Related**: spec 081, lessons L131, `WeaselTSF/KeyEvent.cpp`, `librime/src/rime/key_event.h`, verification report §3.14

## Context

The TSF front end translates a Windows `vkey` into the `(keycode, modifier)`
pair that librime consumes. For **printable** keys it does not send the
physical key: `WeaselTSF/KeyEvent.cpp:51` calls `ToUnicodeEx()` and sends the
**character the key produced under the active keyboard layout**. Only
non-printable keys (Tab, arrows, `KP_*`, space, …) keep their keycode.

librime matches a `key_binder` binding by **exact equality** on that pair:
`librime/src/rime/key_event.h:43` compares keycode *and* modifier, and
`librime/src/rime/gear/key_binder.cc:276` looks the pair up in a
`std::map`. There is no normalisation step in between.

The two conventions therefore disagree, and the disagreement is silent:

| `accept:` | wants keycode | front end sends | fires? |
|---|---|---|---|
| `comma` | `0x2C` (`,`) | `0x2C` | yes |
| `Control+1` | `0x31` (`1`) | `0x31` | yes |
| `Shift+comma` | `0x2C` (`,`) | `0x3C` (`<`) | **never** |
| `Control+Shift+9` | `0x39` (`9`) | `0x28` (`(`) | **never** |

A second, independent rule compounds it: since spec 077 the front end
**eats the Shift key itself** (`WeaselTSF/KeyEventSink.cpp:44`), so any
binding naming `Shift_L` / `Shift_R` / `Shift+Shift_L` can never fire
either.

The shipped configuration contained **six** such dead bindings. Two of them
(`Control+Shift+9`, `Control+Shift+0`) were redundant duplicates of
functionality that was also bound to a reachable chord; four of them
(`Shift+apostrophe`, `Shift+backslash`, `Shift+bracketleft`,
`Shift+bracketright`) were the only implementation of a user-facing feature
and had **never** worked, while a comment in the same file claimed they
worked.

The tempting "architecturally correct" fix is to make the front end follow
the IBus convention and send the **base keycode plus a Shift mask**, so
`accept: "Shift+comma"` reads literally. That would change the meaning of
every shifted keystroke in the product.

## Decision

We keep the "send the produced character" semantics and fix the
**configuration** instead.

- The TSF front end continues to send the character a key produced. We do
  **not** change `WeaselTSF/KeyEvent.cpp` to send base keysyms.
- A `key_binder` `accept:` pattern that involves a modifier must name the
  **character the chord produces**, not the base key. `Shift+comma` is
  wrong; `Shift+less` (the `<` character) is right for `Shift+,`.
- `test/TestHotkeyBindings` is the guard: it compiles the product's own
  `WeaselTSF/KeyEvent.cpp` into the test binary, enumerates every
  `vkey × modifier` combination, and fails when a binding cannot fire.
  Known-dead bindings are listed in `known-dead-bindings.txt` and must be
  removed from that file as they are fixed.

### Alternatives considered

- **Change the front end to send base keycodes + Shift mask (IBus
  convention).** Rejected: librime's `Punctuator` maps the *unshifted*
  ASCII and picks between `half_shape` and `full_shape` by the character
  it receives; `ascii_composer` likewise reasons about characters. Feeding
  librime base keys plus a modifier would strip the meaning of every
  `Shift+punctuation` and `Shift+letter` keystroke — a change to the
  product's input semantics, not a bug fix.
- **Patch librime to accept either form.** Rejected: `librime/` is a
  submodule and must not be edited (P3).
- **Delete the four dead `Shift+punctuation` bindings and drop the
  feature.** Rejected: the feature (Shift+punctuation commits an ASCII
  symbol, bypassing the punctuator) has user value; it was only written
  with the wrong key name. Fixing the name keeps the feature.
- **Keep the six dead bindings and document them.** Rejected: two of them
  are redundant and mislead users; four of them are a broken promise.

## Implementation status (2026-10-25)

**Accepted and partly implemented.** The maintainers approved the direction
(keep the "send the produced character" semantics, fix the configuration) and
released the half that needs no further product decision:

| Part | State | Where |
|---|---|---|
| Delete the two redundant dead chords `Control+Shift+9` / `Control+Shift+0` | **done** | `output/data/default.yaml` |
| Point `test/TestDefaultHotkeys` at the reachable twins, and pin the dead ones with negative assertions | **done** | `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` |
| Shrink `known-dead-bindings.txt` from 6 entries to 4 | **done** | `test/TestHotkeyBindings/known-dead-bindings.txt` |
| Re-pin the binding count that `TestYamlRoundTripE2E` asserts (29 -> 27) | **done** | `test/TestYamlRoundTripE2E/TestYamlRoundTripE2E.cpp` |
| Keep the settings UI's builtin / default shortcut table in sync | **done** | `WeaselServer/ShortcutSettings.cpp` |
| Rename the four `Shift+punctuation` bindings to the character key names | **not started** | still in `known-dead-bindings.txt`; spec 081 T1.1-T1.4 |

Gate output after the change: `bindings=31|reachable=27|dead=0|known_dead=4`.

### Two follow-ups this decision exposed

1. **The settings UI can still manufacture a dead binding.**
   `ShortcutSettings::NormalizeCombo()` builds the `accept:` string from the
   *virtual key* (`VKeyToName`), so a captured `Ctrl+Shift+9` becomes
   `Control+Shift+9` -- the chord this ADR proves can never fire. A captured
   `Ctrl+Shift+A` becomes `Control+Shift+a`, and librime's key table has no
   lowercase `a` at all, so that binding is dropped as invalid. Fixing it
   means making the capture path layout-aware (`ToUnicodeEx`, exactly what
   `WeaselTSF/KeyEvent.cpp` does), which is a product change of its own and
   is **not** part of this change. Tracked as spec 081 T1.9 / T1.12.

2. **`full_shape` and `emoji` have no binding at all.**
   The shipped `default.yaml` has both toggles commented out, yet the
   settings UI still offers `Control+Shift+F` (full/half width) and
   `Control+Shift+e` (emoji) as if they were bound. Whether to bind them --
   and to what chord -- is an open product decision.

## Consequences

> The consequences below describe the decision **as a whole**. What has
> actually shipped so far is in the implementation status table above.

- ✅ The four `Shift+punctuation` shortcuts start working, and the
  configuration no longer lies about it.
- ✅ The hotkey gate turns a class of silent, invisible defects into a
  build-breaking signal.
- ⚠️ The `accept:` vocabulary is **layout-dependent** for printable keys:
  the same physical chord produces different characters under different
  keyboard layouts. The gate therefore pins the US layout (0409) and
  reports the layout it used; a product that must support other layouts
  has to name the character per layout or use a non-printable chord.
- ⚠️ Anyone writing a new binding has to think in characters, not keys.
  This is now stated in `CONTEXT.md` §4 (invariant 16) and in L131.
- ⚠️ The gate only sees what the front end can emit; it cannot prove that
  a binding produces the *intended user-visible effect* (for example that
  `send: Shift+Left` moves the cursor). Those need a real-machine test.
- The decision is reversible: switching to base-keycode semantics later
  would require re-writing every printable binding and re-tuning
  `Punctuator` behaviour — expensive, so it is deliberately not taken now.
