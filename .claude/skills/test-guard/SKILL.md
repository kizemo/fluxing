---
name: test-guard
description: Review generated/changed test code against universal testing rules before it ships. Reject AI-generated test bloat. Best used after a coding agent writes tests.
---

# Test Guard

> Coding agents over-generate tests. Mock-heavy unit tests that assert implementation details, near-duplicate test bodies, and tests that re-verify the framework are all bloat.

## When to activate

- A coding agent has just written test functions
- Editing existing tests
- Reviewing a PR diff containing test changes
- "Write tests for X" / "Add tests" requests

## Adapt to the project first

Project-specific test rules win:
- This project uses **plain C++** with `assert()` / custom `EXPECT_*` — NOT GoogleTest / Catch2
- Test exes built via msbuild, output to `test/<Name>/Release/`
- Each test is `int main()` returning 0/1
- L48: link-probe tests using FluxingD2DRenderer must use `ExitProcess(rc)` to skip atexit

## Universal rules (apply in priority order)

### BLOCKER (must fix before ship)

- **B1**: Test doesn't fail when the bug/feature is missing (test asserts nothing real)
- **B2**: Test passes immediately (no actual assertion; e.g., just `printf("OK\n"); return 0;`)
- **B3**: Test depends on global state from another test (no `setUp`/`tearDown`)
- **B4**: Test is never added to `weasel.sln` or `run-test-suite.bat` (won't run in CI)
- **B5**: Test for production code that was never written (orphan test)

### WARNING

- **W1**: Test mocks everything → can't catch real regressions (use behavior-level + link-probe per L24/L25)
- **W2**: Test asserts implementation details (private member access, internal function call order) rather than behavior
- **W3**: Near-duplicate test bodies differing by one value (collapse into table-driven test)
- **W4**: Test re-verifies framework / OS / STL (e.g., "std::vector::push_back works") — pointless
- **W5**: Test name doesn't describe what it tests (`Test1`, `foo`)
- **W6**: Test is >200 lines without section comments (split)
- **W7**: Test uses `sleep()` / `Sleep()` for timing (use condition-based waiting)
- **W8**: Test lacks L## regression marker (if it tests a specific lesson, link to it in comment)

### INFO (style)

- **I1**: Inconsistent ASSERT vs EXPECT choice (project uses EXPECT_* typically)
- **I2**: Magic numbers without named constants
- **I3**: Commented-out code instead of deleted

## Rime/Weasel-specific gotchas

- **L04**: `librime 1.13` key_binder action names are case-sensitive
- **L16**: lowercase modifier `shift+l` is silently dropped
- **L18**: `shift+<letter>` release events must NOT toggle ascii_mode
- **L19**: L19 regression tests must verify `keycode=Shift_L` bindings removed
- **L42**: dark-mode byte-pattern (0x001E1E1E) is in weasel.dll when bridge is linked
- **L49**: WeaselServerImpl.h must have `MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)` — test guards this

## Output format

```
Review of test/TestXxx/TestXxx.cpp (lines X-Y):

🔴 BLOCKER B1: TestFoo passes immediately, no real assertion
   Fix: add `EXPECT_EQ(0,1)` or actual call to function under test

🟡 WARNING W5: Test name "Test1" doesn't describe what it tests
   Fix: rename to "EXPECT_Notebook_Status_Foo_DoesNot_Crash"

🟢 INFO I2: Magic number 42 in TestBar
   Fix: `constexpr int kExpectedCount = 42;`

Tests: 0 fail, 3 added, 1 renamed
```
