---
name: debugging-and-error-recovery
description: Step-by-step debug workflow. Use after systematic-debugging identifies the root cause and you need to apply the fix. Covers rollback, re-test, L## documentation.
---

# Debugging and Error Recovery

> Companion to `systematic-debugging`. Use AFTER root cause is identified, to apply the fix and recover.

## When to use

- Root cause already known (per systematic-debugging Phase 1-2)
- About to apply a non-trivial fix
- Want to avoid introducing new bugs
- Want a clear rollback path

## The Fix Procedure

### Step 1: Pre-fix check

Before changing any code, verify:
- [ ] Do I understand the root cause? (Phase 1-2 of systematic-debugging)
- [ ] Is there a regression test that currently fails? (TDD red)
- [ ] Do I have a clear rollback plan? (git commit before fix)
- [ ] Is the fix < 4 hours? (R5)

### Step 2: Atomic commit before fix

```bash
git add -A
git commit -m "wip: pre-fix state for L## investigation"
```

Or, on a feature branch:
```bash
git checkout -b fix/L##
```

### Step 3: Minimal fix

The smallest change that addresses the root cause. Not:
- "While I'm here, let me also refactor X" (scope creep → new bugs)
- "Add a try/catch to hide the error" (hides symptom)
- "Move code around for clarity" (refactor is separate work)

### Step 4: Verify fix

- Run regression test (TDD green)
- Run full test suite: `cmd /c "scripts\test-infra\run-test-suite.bat"`
- For installer / NSIS: AGENTS.md §2.5 silent-install smoke test
- For linker / PE: L14 byte-verify
- For line-ending: L47 byte-verify
- For dark-mode: L42 byte-verify

### Step 5: Commit fix

```bash
git add <specific files>
git commit -m "fix(scope): short description (L##)

Root cause: <one sentence>
Fix: <one sentence>
Regression test: <test name>"
```

### Step 6: Document L## (if non-trivial)

Append new L## to `.specify/memory/lessons-learned.md`:
- Symptom (1 paragraph)
- Root cause
- Fix
- Lessons (3-5 numbered)
- Related (L## cross-references)
- Anti-patterns (AP-##-A, B, C...)

### Step 7: Push

```bash
git push kizemo Fluxing
```

### Step 8: If fix doesn't work

**STOP. Re-enter Phase 1 of systematic-debugging.** Don't pile on fixes.

## Recovery patterns

| Symptom | Recovery |
|---|---|
| Build broken | `xmake clean && xmake` |
| Test hangs | Ctrl-C, check `ExitProcess` in test (L48) |
| Git conflict | `git rebase --abort` then re-merge |
| installer crashes on user machine | L17 / L13 / L58 re-check |
| Wrong binary in installer | L14 byte-verify |
| LevelDB lock timeout | L55: MaintenanceGuard |

## Anti-patterns

- Fixing 3 things in one commit (revert is impossible)
- "It worked on my machine" (L14 byte-verify on real installer)
- Removing failing test (NEVER — that's hiding the bug)
- "Just add a comment // TODO" (TODO that never gets done)
- "This is good enough" (R6: "done" requires test evidence)
