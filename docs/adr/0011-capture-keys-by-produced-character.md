# 0011. The settings dialog captures keys by the character they actually produce

- **Status**: Proposed
- **Date**: 2026-10-28
- **Deciders**: Fluxing maintainers
- **Related**: spec 083, ADR 0009, ADR 0010, ADR 0014, lessons L148 / L149, checklist T2 / T20

## Context

### Rime's `accept:` names a character, not a key

A Rime `key_binder` entry's `accept:` field names **the character the
front end actually emits for that chord**, using Rime's character key
names (`numbersign`, `dollar`, `parenleft`, `quotedbl`, `bar`, ...).
It is not a virtual key code. The Weasel front end resolves a chord
against the *current keyboard layout* and sends the resulting
character's key code (Rime key codes are character code points, which
is why the gate below reports `0x0028` for `(` rather than a Windows
virtual key).

ADR 0009 removed two bindings for exactly this reason (L148) and
renamed four more (L149), but the **settings dialog** was never
brought in line with it.

### The dialog still builds names from virtual key codes

`FluxingConfigEditor/KeyRecorder.cpp` maps the captured virtual key
code straight to a Rime name (`MapKeyCodeToLibrimeName`): digits
`0x30..0x39` become `0`..`9`, letters `0x41..0x5A` become
**lower-case** `a`..`z`, and OEM keys come from a fixed table
(`semicolon`, `plus`, `grave`, ...). Modifiers are concatenated from
`GetAsyncKeyState`.

Four distinct defects follow:

1. **Shift-modified digits and symbols name the wrong character.**
   `Ctrl+Shift+9` produces `(` on a US layout, but the dialog emits
   `Control+Shift+9`.
2. **Shift-modified letters are lower-cased.** `Ctrl+Shift+A`
   produces `A`, but the dialog emits `Control+Shift+a`.
3. **The Windows key is silently rewritten to `Control+`.**
   Rime has no Windows modifier, so the chord is not even expressible.
4. **The layout is never consulted.** `MapVirtualKeyW(vk,
   MAPVK_VK_TO_SSC)` plus `GetKeyNameTextW` use the *system default*
   layout, not the layout the user is typing in.

### The failure is invisible until the binding never fires

The project already owns the authority to decide this question: the
`TestHotkeyBindings` gate resolves chords against the live layout and
reports `DEAD_KEYCODE` when the front end would never send the named
key code. It also enumerates, per modifier set, **which character
names actually fire**.

A controlled experiment on 2026-10-28 (a copy of
`output/data/default.yaml` with `Control+Shift+numbersign` rewritten to
`Control+Shift+9`, run through the gate):

```
BINDING|...|265|always|Control+Shift+9||toggle|DEAD_KEYCODE|
  the front end never sends keycode 0x0039 (9) with modifier 0x0005;
  the front end sends 0x0028 (parenleft) for that chord;
  keys that do fire with the same modifier: ampersand, asciicircum,
  asciitilde, asterisk, at, bar, braceleft, braceright, colon, dollar,
  exclam, greater, ...
SUMMARY|bindings=27|reachable=26|dead=1|known_dead=0|...|unused=0
RESULT|FAIL
```

The same file's `Control+Shift+numbersign` is `REACHABLE`. ⇒ The
settings dialog can currently mint bindings that are dead on arrival,
and nothing in the UI tells the user.

### Three facts that constrain any fix

- **F1 — A modified binding can only name a character Rime has a name
  for.** `librime/src/rime/key_event.cc:51-82`: a one-character
  `accept:` takes the character code directly, but a modified one goes
  through `RimeGetKeycodeByName`, which does a plain `strcmp` over
  `keys_by_keyval` (`librime/src/rime/key_table.cc:2026-2033`). That
  table carries **upper-case letters only** (`A`–`Z` = `0x41`–`0x5A`);
  lower-case letters are absent. ⇒ `Control+f` is not merely
  unreachable, it **fails to parse**; only `Control+Shift+F` is
  expressible. This is why the built-in shortcut table already uses
  `Control+Shift+F`.
- **F2 — The front end clears Ctrl and Alt before resolving the
  character.** `WeaselTSF/KeyEvent.cpp:44-51` zeroes `VK_CONTROL` and
  `VK_MENU` in the key-state table before `ToUnicodeEx`, so the
  produced character's case is decided by **Shift alone**. A capture
  path that ignores this cannot tell `Ctrl+F` from `Ctrl+Shift+F`.
- **F3 — The front end adds `LOCK_MASK` when CapsLock is on.**
  `WeaselTSF/KeyEvent.cpp:18-19` sets `ibus::LOCK_MASK` (`0x2`,
  `include/KeyEvent.h:225`) whenever `keyState[VK_CAPITAL]` has the
  toggled bit, and `expand_ibus_modifier`
  (`RimeWithWeasel/RimeWithWeasel.cpp:44-46`) preserves the low byte.
  Because librime compares modifiers **exactly**
  (`librime/src/rime/key_event.h:43-44`), every binding that does not
  name `Lock` should stop matching while CapsLock is on. ⇒ Every
  shortcut may be dead for the whole time CapsLock is on. This is
  **F3 — measured and fixed since**: the mismatch is real, but the dominant
  effect is stronger than a mismatch: the engine refuses to process any key
  while the latch bit is set, because the project configuration sets
  `good_old_caps_lock: true`. See ADR 0014 and spec 085.

## Decision

We change the capture path so the name it produces is derived from
**the character the chord actually produces under the current keyboard
layout**, and we make the `TestHotkeyBindings` gate the acceptance
criterion for that change.

- **D1 — One source of truth for character names.** The mapping
  "character → Rime key name" and the resolution "chord → character"
  live in one shared, GUI-free unit. The gate and the capture dialog
  both consume it. Two independent implementations would drift, and
  the dialog would mint dead bindings again.
- **D2 — The capture resolves against the live layout, with the same
  rules the front end uses.** The character comes from the keyboard
  layout attached to the capture window, with Ctrl and Alt cleared
  and Shift applied exactly as `WeaselTSF/KeyEvent.cpp` does — never
  from the bare virtual key code and never from the system default
  layout.
- **D3 — Modifiers are recorded as the front end emits them.** The
  capture emits the modifier set it observed, because librime compares
  modifiers exactly (F1). A chord whose character Rime has no name
  for is **refused with a message** — it is never rewritten.
- **D4 — The gate is the acceptance test.** Every binding the dialog
  can produce must be judged `REACHABLE` by `TestHotkeyBindings`. A
  test writes captured names into a scratch schema and asserts
  `dead=0` for them.
- **D5 — We do not relax the gate and we do not touch `librime/`.** The
  gate's notion of "reachable" is already correct; the dialog was the
  broken side. Constitution P3 keeps `librime/` read-only.
- **D6 — The Windows key is refused, not remapped.** The front end
  turns `VK_LWIN` / `VK_RWIN` into `Meta_L` / `Meta_R`
  (`WeaselTSF/KeyEvent.cpp:143-146`), key codes that no Rime binding
  can name. The capture rejects them with a message.

### Alternatives considered

- **Keep virtual key codes and let the user type the Rime name.** —
  Rejected: the dialog's whole job is to spare the user from knowing
  that names exist, and it cannot offer a name it cannot compute.
- **Refuse any chord whose produced character has no Rime name.** —
  Rejected as a blanket rule: unmodified lower-case letters are
  expressible (F1's one-character path), and named symbols such as
  `numbersign` cover most of the rest.
- **Post-process captured names through the gate until they are
  reachable.** — Rejected: it would rewrite the user's intent (the
  chord) into a different chord that happens to fire.
- **Fix only the dialog and leave the second capture path alone.** —
  Rejected: `WeaselServer/ShortcutSettings.cpp` owns a second,
  independent virtual-key-code mapping (its `VKeyToName` maps
  `VK_OEM_PLUS` to `equal` while the dialog maps it to `plus`); two
  capture paths that disagree are two ways to mint dead bindings.

## Consequences

- ✅ Bindings minted by the settings dialog become reachable by
  construction, and the gate can prove it per binding.
- ✅ The dialog becomes genuinely layout-aware: on a German layout
  `Ctrl+Shift+7` is captured as the character that layout produces.
- ✅ One implementation of "character → Rime key name" removes the
  chance of the dialog, the shortcut panel and the gate drifting apart.
- ⚠️ The capture depends on the keyboard layout in effect when the
  dialog is open; a layout switch after capture does not retro-fix a
  name.
- ⚠️ Chords Rime cannot express (Windows key, `Ctrl` + lower-case
  letter) become refusals rather than silent rewrites — a visible
  behaviour change, and users must hold Shift for `Ctrl`+letter.
- 🔄 **Reversible**: the change is confined to the capture paths plus
  one shared unit; reverting it restores the old behaviour without
  touching any shipped configuration.

## Open questions for the maintainers

- **Q1 — Where does the shared unit live?** It must be reachable from
  both the GUI library and the gate test, without dragging GUI
  dependencies into the test. A header-only unit under `include/` is
  the leading candidate; the gate already compiles the production
  `WeaselTSF/KeyEvent.cpp`.
- **Q2 — What to do about CapsLock (F3)?** Three candidates: drop
  `LOCK_MASK` in the front end (one line, changes behaviour for every
  keystroke), keep it and let the capture emit `Lock+` (changes what
  every shipped binding means), or extend the gate to enumerate
  CapsLock states (changes the gate's baseline). **Must be measured
  before it is chosen.**
  - **Answered (2026-10-28) — see ADR 0014 / spec 085.** Measured first:
    the defect is not a mismatch but the engine refusing to process keys
    at all, because the project configuration sets
    `good_old_caps_lock: true`. The decision takes **candidate 1**
    (the front end stops reporting the latch state) **and candidate 3**
    (the gate enumerates the CapsLock state, with a built-in negative
    control). Candidate 2 is rejected.
- **Q3 — Should the capture teach the `Ctrl`+letter rule (F1) in the
  UI**, or only refuse the chord?
