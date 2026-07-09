---
name: planning-and-task-breakdown
description: Break work into ordered tasks. Use when you have a spec and need to break work into implementable units. A task should be implementable, testable, and verifiable in a single focused session.
---

# Planning and Task Breakdown

> Decompose work into small, verifiable tasks with explicit acceptance criteria. Every task should be small enough to implement, test, and verify in a single focused session.

## When to use

- Have a spec, need to break into tasks
- A task feels too large to start
- Work needs parallelization
- Implementation order isn't obvious

## When NOT to use

- Single-file changes with obvious scope
- Spec already contains well-defined tasks (use spec-init's output directly)

## The Planning Process

### Step 1: Identify the work streams

Read the spec, identify:
- Production code changes (per module)
- Test changes (per test project)
- Build / config / infra changes
- Documentation changes
- Migration / rollback considerations

### Step 2: Order by dependency

Bottom-up:
1. Schema changes / new types
2. Implementation behind those types
3. Integration with existing modules
4. UI / user-facing
5. Tests for each layer
6. Documentation / CHANGELOG

### Step 3: Apply R5 / R3 / P4

For each task:
- **R5**: ≤ 4 hours, 1-3 files, clear I/O
- **R3**: Priority (P1/P2/P3) + user story tag (`[US001-A]`)
- **P4**: Conventional Commits scope

### Step 4: Add verification per task

Each task needs:
- Acceptance criteria (what "done" looks like)
- Verification command (test name, byte-verify recipe, etc.)
- Cross-reference to L## if related to a known issue

### Step 5: Add parallel markers

Mark tasks that can run in parallel: `[P]` (P for Parallel).

## tasks.md template

```markdown
# NNN - Tasks

## P1 (must) - all marked [ ] at start
- [ ] T001: <action> (P1, [US001-A], 1-3 files, <4h)
  - Files: Foo.cpp:42-50, Foo.h:10
  - Verify: `xmake build WeaselServer` → 0 errors
  - Lesson: L## cross-ref if applicable
- [ ] T002: <action> (P1, [US001-B], 1-3 files, <4h)
- [ ] T003: <action> (P2, parallel) (P1, [US002-A], 1-3 files, <4h)

## P2 (should)
- [ ] T101: ...

## P3 (could)
- [ ] T201: ...

## Verification (L46 3-path gate)
- [ ] V001: `xbuild.bat weasel installer` → exit 0
- [ ] V002: `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32` → 0 errors
- [ ] V003: `scripts\test-infra\run-test-suite.bat` → 16/16 PASS
- [ ] V004: AGENTS.md §2.5 silent-install smoke test (if install.nsi changed)
- [ ] V005: L## byte-verify (if applicable)
```

## Anti-patterns

- "Implement everything" (one mega-task)
- "Refactor while we're at it" (scope creep)
- "Add tests" (separate task per concern — not "all tests")
- Vague acceptance ("looks good")
- No verification step
