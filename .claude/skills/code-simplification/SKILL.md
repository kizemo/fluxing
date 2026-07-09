---
name: code-simplification
description: Simplify code for clarity without changing behavior. Use after a feature works, during review, or when code is harder to read/maintain than it should be.
---

# Code Simplification

> Goal is not fewer lines — it's code easier to read, understand, modify, debug. Test: "Would a new team member understand this faster than the original?"

## When to use

- Feature works, tests pass, but feels heavier than needed
- Code review flags readability
- Deeply nested logic / long functions / unclear names
- Refactoring code written under time pressure
- Consolidating scattered logic

## When NOT to use

- Code is already clean
- Behavior would change (refactor is not simplification)
- Tests are missing (write tests first, then simplify)

## Refactoring patterns (priority order)

### 1. Rename for clarity

| Before | After |
|---|---|
| `m_ssm` | `m_session_status_map` |
| `int i, j, k` | `int row, col, depth` |
| `process()` | `commit_composition()` |
| `flag = true` | `is_user_initiated = true` |

### 2. Extract method

```cpp
// Before: 80-line function
void ProcessKeyEvent() {
  // ... 20 lines setup ...
  // ... 30 lines of work ...
  // ... 30 lines cleanup ...
}

// After: 3 named methods
void ProcessKeyEvent() {
  AcquireContext();
  ApplyRimeLogic();
  NotifyClients();
}
```

### 3. Replace conditional with polymorphism

```cpp
// Before
if (mode == ASCII) handleAscii();
else if (mode == CHINESE) handleChinese();
else if (mode == MIXED) handleMixed();

// After (if mode is an enum, use a strategy or dispatch table)
```

### 4. Replace magic numbers with named constants

```cpp
if (size > 4096)             // ❌
if (size > kMaxBufferSize)   // ✓ (constexpr in WeaselConstants.h)
```

### 5. Remove dead code

- Commented-out code → delete (git history preserves)
- Unused functions → delete
- Unreachable branches → delete
- TODO comments older than 6 months → either implement or convert to issue

### 6. Inline trivial wrappers

```cpp
// Before
int GetCount() const { return m_count; }

// After (if there's no invariant to preserve)
int count() const { return m_count; }
```

(But if `GetCount()` is a public API, keep it.)

### 7. Replace `auto` with explicit type (when it improves clarity)

```cpp
// Use auto when type is obvious from RHS
auto it = map.find(key);  // ✓

// Use explicit type when it's not
SessionStatus status = ...;  // ✓ (not `auto status = ...;`)
```

## Constraints

- **Behavior must not change** — if simplification changes behavior, it's a refactor, not simplification
- **Tests must still pass** — run after each simplification
- **Don't simplify in a single huge PR** — small atomic commits
- **Don't simplify legacy code without test coverage** — risk of regression

## What NOT to do

- Don't change API surface
- Don't rename public methods (breaking change)
- Don't "improve" performance without profiling
- Don't apply patterns just to be clever (KISS)
