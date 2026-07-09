---
name: tdd
description: Test-Driven Development. Write failing test first, then code to make it pass. Use when implementing any logic, fixing any bug, or changing any behavior. "Seems right" is NOT done.
---

# TDD: Red → Green → Refactor

> A codebase with good tests is an AI agent's superpower. A codebase without tests is a liability.

## The Cycle

```
    RED                GREEN              REFACTOR
 Write a test    Write minimal code    Clean up the
 that fails  ──→  to make it pass  ──→  implementation  ──→  (repeat)
      │                  │                    │
      ▼                  ▼                    ▼
   Test FAILS        Test PASSES         Tests still PASS
```

## When to use

- Implementing new logic
- Fixing a bug (reproduce the bug with a test first — "Prove-It Pattern")
- Modifying existing behavior
- Adding edge case handling

## When NOT to use

- Pure config changes
- Documentation updates
- Static content with no behavioral impact
- Throwaway prototype scripts

## The Prove-It Pattern (for bug fixes)

```
1. REPRODUCE: write a test that fails because of the bug
2. VERIFY: confirm the test fails (it should fail before the fix)
3. FIX: minimal code change
4. VERIFY: test now passes
5. REGRESSION TEST: add the test to permanent test suite
```

If you can't write a failing test, you don't fully understand the bug.

## Project Test Stack

- **Framework**: Plain C++ with `assert()` or custom `EXPECT_*` macros (NO GoogleTest)
- **Test projects** under `test/TestXxx/` — built with `weasel.sln` msbuild path
- **Run all tests**: `cmd /c "scripts\test-infra\run-test-suite.bat"`
- **L48 defensive exit**: test exes using `FluxingD2DRenderer` singleton MUST use `ExitProcess(rc)` to skip atexit

## Test Project Conventions (R5 compliance)

- Single test project, single concern
- Output to `test/<Name>/Release/`
- Added to `weasel.sln` as a new Project entry
- Linked via `run-test-suite.bat` loop
- main() returns 0 = pass, non-zero = fail

## Spec-Specific Patterns

| Pattern | When | File template |
|---|---|---|
| Link-probe | Spec tests if code is **linked** into .dll/.exe (L24, L42) | Link a .cpp from WeaselServer/, verify symbol/byte in output binary |
| Behavior-level | Tests actual behavior with WTL/ATL lifecycle (L25) | Mock WndProc filter (3 lines wcscmp) |
| L46 3-path gate | Every release spec | xmake + msbuild + test suite all PASS |

## Common Anti-patterns

- Writing implementation first, test second (NOT TDD)
- Skipping the red phase ("I'll just add a test later")
- Test that mocks everything and asserts nothing real
- Tests that depend on each other (each must be independently runnable)
- 0/20 test failure assumed "test bug" (usually a real regression — see L04/L18/L19)
