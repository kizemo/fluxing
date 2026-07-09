---
name: spec-init
description: Bootstrap a Spec-Driven Development feature — create spec.md / plan.md / tasks.md in `.specify/specs/NNN-name/`. Use when starting a new feature, building something non-trivial, or whenever a task will touch more than 3 files. Do NOT use for trivial fixes (typo, single-line change).
---

# Spec-Init: Bootstrap a Spec-Driven Feature

> Adapted from `github/spec-kit` v0.11.9. Produces the 3 core artifacts that turn intent into executable work.

## When to use

- User says: "build X", "add Y feature", "let's design Z"
- Task will touch > 3 files, OR
- Task will span multiple sessions, OR
- Task is ambiguous (needs clarification first)

## When NOT to use

- Bug fixes → use `systematic-debugging` first
- Single-file edits, typos, trivial refactors
- Pure research / read-only questions

## Pre-flight (MANDATORY - 4 questions)

Before creating any artifacts, answer inline (1-2 lines each):

1. **What is being built?** (capability, behavior, deliverable)
2. **Who benefits and how?** (user, value, success signal)
3. **What's the smallest shippable slice?** (MVP definition)
4. **What is OUT of scope for v1?** (explicit non-goals)

If any is unclear, run `brainstorming` or `interview-me` first. **Never** proceed with vague answers.

## Artifacts to produce

```
.specify/specs/NNN-<short-name>/
├── spec.md    (tech-agnostic, WHAT layer)
├── plan.md    (tech approach, HOW layer)
└── tasks.md   (implementation checklist)
```

- `NNN` = next available 3-digit number (scan existing `specs/`)
- Use Conventional Commits scope in commit messages (per AGENTS.md §3.2)

## spec.md (R2 compliance)

> **R2 hard rule**: `spec.md` MUST NOT contain tech-stack words (WTL/Boost/C++/MFC/ATL/WeaselTSF/...). Tech names only in `plan.md` and `tasks.md`.

Structure:
```markdown
# NNN - <Name>

## 0. Background (1-2 paragraphs)
## 1. Goals
## 2. User stories (US001-A etc, with P1/P2/P3 priority)
## 3. Functional requirements (FR-001 ...)
## 4. Non-goals
## 5. Acceptance criteria (SC-001-1, SC-001-2...)
## 6. Risks (R-001 ...)
## 7. Dependencies
## 8. Out of scope
```

## plan.md structure

```markdown
# NNN - Plan

## Constitution Check (Principle I-V + R1-R9 + P1-P8 - all 5 principles must be ✓)
## Tech approach (free to use WTL/Boost/C++ etc)
## File-level changes (exact paths)
## Build / Test / Release impact
## Risk mitigation
```

## tasks.md structure (R5 + R3 compliance)

```markdown
# NNN - Tasks

## P1 (must) - all marked [ ] at start
- [ ] T001: <action> (P1, [US001-A], 1-3 files, <4h)
- [ ] T002: <action> ...
## P2 (should)
- [ ] T101: ...
## P3 (could)
- [ ] T201: ...

## Verification
- [ ] V001: <command + expected output>
```

R5: 单任务 ≤ 4 小时,1-3 文件,有 I/O。
R3: 每任务有 P1/P2/P3 + [USxxx] 标签。

## After writing 3 files

1. Run `spec-check` to validate
2. Get user approval before implementation
3. **Do not start coding** until user signs off on the spec/plan/tasks
