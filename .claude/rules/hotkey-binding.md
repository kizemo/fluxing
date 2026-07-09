---
paths:
  - "Weasel**/Hotkey*"
  - "Weasel**/Key*"
  - "Weasel**/Shift*"
  - "test/Test*Hotkey*"
  - "test/Test*Shift*"
  - "test/Test*Binding*"
  - "test/TestDefaultHotkeys/**"
  - "test/TestShiftSelectBinding/**"
  - "test/TestCandidate*"
  - "test/TestResponseParser/**"
---

# Hotkey, key binding & shift-modifier rules

The hotkey handling model on the `Fluxing` branch has been incrementally
fixed since `0.18.x`. Several bugfix batches (spec 012, 014, 018, 019) and
incidents (L18, L19, L21) all touched this layer. **When the prompt is
"hotkeys behave wrong" or "shift doesn't do X", read this file first.**

## Canonical rules

- **`ascii_composer/switch_key` must set `Shift_L: noop` and `Shift_R: noop`**
  to neutralize the default modifier behaviour. Actual shift-select logic
  lives in `key_binder/bindings`, not in `switch_key`. The two layers have
  different semantics in librime.
- **The 2nd / 3rd candidate select-by-shift flows are now bound at the
  `key_binder` level** (per spec 014). Use `WeaselIPCData.h::Hotkey` to
  drive the activate-2nd-candidate message; do not call into librime
  directly.
- **Hotkey state is owned by `WeaselIPC::Client::HotkeyBinding`** and shared
  via the IPC pipe (single source of truth). Do not duplicate state in TSF
  or Deployer local memory — they will drift and create phantom shifts.

## Tests that must pass for any change in this layer

Located under `test/`:

- `TestDefaultHotkeys` — the canonical hotkey table.
- `TestShiftSelectBinding` — spec 014 regression.
- `TestBindingResolution` — resolution logic for compound bindings.
- `TestCandidateIgnoreFilter`, `TestCandidateRButtonDown`,
  `TestUserDictUpdate` — candidate-list state machine, all touched by hotkey
  chains.
- `TestResponseParser` — confirms the wire-side response when shift-select
  fires.

## Anti-patterns

- **Do not** invent a private hotkey table in `WeaselTSF` or `WeaselServer`
  — they diverge within a week. Always go through `HotkeyBinding`.
- **Do not** change the TSF-thread-vs-IPC-thread split when wiring a new
  binding — P2 still applies. Shifts are wired through key events, not by
  posting to TSF directly.
- **Do not** "fix" `Shift_L` to do nothing as a global default in
  `default.yaml` — `ascii_composer/switch_key/Shift_L: noop` is the only
  acceptable form; touching the global would break other schemas.
- **Do not** mix Yaml-level binding edits with C++ hotkey code in the same
  commit; they have different reviewers and different test runs.
