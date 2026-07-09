---
name: writing-plans
description: Use when you have a spec or requirements for a multi-step task, before touching code. Write a comprehensive plan that an engineer with zero context can execute. Bite-sized tasks, DRY, YAGNI, TDD, frequent commits.
---

# Writing Plans

> **Announce at start**: "I'm using the writing-plans skill to create the implementation plan."

> Write plans assuming the engineer has zero context for the codebase and questionable taste. Document everything they need.

## Scope Check

If the spec covers multiple independent subsystems, it should be broken into sub-project specs. Each plan produces working, testable software on its own.

## File Structure

Save plans to: `F:\soft\00selfmade\rime_claude\.specify\specs\NNN-name\plan.md`

(Override this default per project convention.)

## Plan Template

```markdown
# NNN - Plan: <Feature Name>

## Context (1 paragraph)
Why this matters. What spec.md covers. What this plan delivers.

## Architecture Decisions

### Decision 1: <title>
- **Choice**: ...
- **Alternatives**: ...
- **Why**: ...

### Decision 2: ...

## Constitution Check (R4 hard rule)

| Principle | Status | Notes |
|---|---|---|
| I. Intent Before Implementation | ✓ | spec.md has What/Why/Acceptance |
| II. Test-Backed Change | ✓ | tasks.md has regression tests |
| III. Spec-Artifact Discipline | ✓ | 3 files complete |
| IV. Structured Clarification | ✓ | no [NEEDS CLARIFICATION] markers |
| V. Incremental Delivery | ✓ | each US shippable alone |
| R1-R9 | ✓ | ... |
| P1-P8 | ✓ | ... |

## File-level Changes

| File | Change Type | Detail |
|---|---|---|
| WeaselServer/Foo.cpp | Modify | L120-150: add OnHotkey handler |
| WeaselServer/Foo.h | Modify | L31: add MESSAGE_HANDLER |
| test/TestFoo/ | New | 5 test cases |
| ... | | |

## Build / Test / Release Impact

- Build: xmake (incremental, 30-90s)
- Test: scripts\test-infra\run-test-suite.bat (must add new test)
- Release: 0.18.34.0 (or not ship this version)

## Risk Mitigation

| Risk | Mitigation |
|---|---|
| R-001: <risk> | <mitigation> |
| ... | |

## Out of Scope (deferred)

- ... (list explicitly to avoid scope creep)

## Open Questions (if any)

- [NEEDS CLARIFICATION: ...]
```

## Plan Quality Bar

A good plan:
- Can be executed by an engineer with zero codebase context
- Has specific file paths and line numbers
- Has bite-sized tasks (1-3 files, ≤4 hours each)
- Includes verification per task
- Documents non-obvious decisions
- References L## when related to known issues

## Anti-patterns

- "Implement the feature" (no file paths, no I/O)
- "Refactor X" (no scope)
- "Add tests" (which tests? for what code?)
- No verification step
- No risk mitigation
