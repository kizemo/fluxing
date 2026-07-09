---
name: incremental-implementation
description: Deliver changes in thin vertical slices. Each slice = working, testable code. Avoid implementing entire feature in one pass.
---

# Incremental Implementation

> Build in thin vertical slices — implement one piece, test it, verify it, then expand. Each increment should leave the system in a working, testable state.

## When to use

- Multi-file change
- Building a feature from tasks.md
- Refactoring existing code
- Tempted to write >100 lines before testing

## When NOT to use

- Single-file, single-function change

## The Increment Cycle

```
┌──────────────────────────────────────┐
│ 1. Pick the smallest useful slice    │
│    from tasks.md (1-3 files)         │
├──────────────────────────────────────┤
│ 2. Write the failing test (TDD red)  │
├──────────────────────────────────────┤
│ 3. Write minimal code (TDD green)    │
├──────────────────────────────────────┤
│ 4. Run tests + manual verify         │
├──────────────────────────────────────┤
│ 5. Refactor (clean-code-guard)       │
├──────────────────────────────────────┤
│ 6. Commit (conventional commits)     │
├──────────────────────────────────────┤
│ 7. Repeat for next slice             │
└──────────────────────────────────────┘
```

## Slice criteria

Good slice:
- Touches 1-3 files
- Has clear I/O
- Can be tested in isolation
- Doesn't depend on later slices
- ~30 min - 4 hours of work

Bad slice (split it):
- "Implement the whole feature"
- "Refactor module X"
- "Add all tests"

## For Rime/Weasel

Each slice should map to 1 task in tasks.md. After each slice:

```bash
# Verify
cmd /c "scripts\test-infra\run-test-suite.bat" 2>&1 | tail -10
# Or for one test
test\TestXxx\Release\TestXxx.exe
# Or for byte-verify
powershell -Command "[IO.File]::ReadAllBytes('output\weasel.dll') | Select-String -Pattern '\x1E\x1E\x1E' -Encoding Byte"
# Commit
git add <specific files>
git commit -m "fix(WeaselTSF): add OnHotkey MESSAGE_HANDLER (L49)"
git push kizemo Fluxing
```

## Why this matters for AI agents

- AI can produce 500+ lines in one go — but if it's wrong, you have to debug 500 lines
- With slicing, you catch errors per ~30-line increment
- L## lessons are most often "I should have sliced smaller"

## Anti-patterns

- "Let me just write the whole thing and test at the end" (50% of L## are this mistake)
- "I'll commit at the end when it all works" (L04 / L18 / L19)
- "Tests can wait until implementation is done" (R-violation)
- "I'll push everything at once" (no fallback)
