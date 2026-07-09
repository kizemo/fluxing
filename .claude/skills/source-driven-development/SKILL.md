---
name: source-driven-development
description: Verify implementation against official documentation. Use when touching public APIs (RimeTraits, IPC semantics, librime C API). Training data goes stale — always cite sources.
---

# Source-Driven Development

> Every framework-specific code decision must be backed by official documentation. Don't implement from memory — verify, cite, and let the user see your sources.

## When to use

- Touching public API (RimeTraits, RimeLeversApi, WeaselIPC message semantics)
- Building with framework / library where correctness matters
- Implementing schema / RIME behavior
- IPC message type changes

## When NOT to use

- Pure logic (loops, conditions, variable rename)
- User explicitly says "just do it quickly"
- Trivial implementation

## Fluxing source references

| Topic | Source |
|---|---|
| RIME engine API | `librime/include/rime_api.h` (vendored via submodule) |
| librime C++ internals | `librime/src/rime/...` (read-only) |
| RIME key tables | `librime/src/rime/key_table.h` |
| Weasel IPC protocol | `include/WeaselIPC.h` + `WeaselIPCData.h` |
| Fluxing brand fork rules | `.specify/memory/constitution.md` §P8 |
| ATL / WTL | Microsoft docs (learn.microsoft.com) |
| Boost | boost.org docs |
| Win32 / TSF | Microsoft Learn |

## Procedure

### Step 1: Find the authoritative source

For Fluxing:
- librime → `librime/include/rime_api.h` + .cc files in `librime/src/rime/`
- RIME schema → `librime/data/...` for examples
- Weasel → upstream `rime/weasel` GitHub (read-only)

### Step 2: Read the source (not from memory)

```bash
# librime key tables
cat librime/src/rime/key_table.h

# librime C API
grep -n "RIME_API" librime/include/rime_api.h
```

### Step 3: Cite in the code

```cpp
// Per librime/src/rime/key_table.h:60, Shift_L = 0x1B (kKeyShiftL)
// See RIME issue #102 for modifier handling
```

### Step 4: Cite in the spec / plan

```markdown
## Tech approach

For RIME 1.13.1 key handling:
- `Shift_L` = 0x1B (per `librime/src/rime/key_table.h:60`)
- `Shift_R` = 0x1C
- Modifier mask `kShiftMask = 0x01` (per `librime/src/rime/key_event.h:52`)

RIME accepts `keycode=Shift_L, modifier=Shift` but NOT `keycode=shift_l`
(case-sensitive — L16 / L19 lessons).
```

## L## cross-ref

- **L03 / L04**: librime 1.13 key_binder action types
- **L16**: lowercase modifier `shift+l` is silently dropped
- **L18 / L19 / L21**: Shift binding edge cases
- **L43**: link-probe pattern (verify .lib is actually linked)

## Anti-patterns

- "I know RIME supports X" (verify, cite)
- "Use the old API, it still works" (check deprecation)
- "Boost is the same as STL" (no, it has different semantics)
- "TSF is just like XPC" (no, it's a Microsoft COM model)
