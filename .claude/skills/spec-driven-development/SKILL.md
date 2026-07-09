---
name: spec-driven-development
description: Create specs before coding. Use when starting a new project, feature, or significant change. Spec = shared source of truth between you and the user.
---

# Spec-Driven Development

> Write a structured specification before writing any code. Spec is the shared source of truth — defines what we're building, why, and how we'll know it's done. Code without a spec is guessing.

## When to use

- Starting a new project / feature / significant change
- Requirements are ambiguous or incomplete
- Change touches multiple files or modules
- About to make an architectural decision
- Task would take >30 minutes

## When NOT to use

- Single-line fixes
- Typo corrections
- Changes where requirements are unambiguous and self-contained

## The Gated Workflow

Spec-driven development has 4 phases. Do not advance until current is validated.

```
Phase 1: Interview (interview-me)
  ↓ "What does the user actually want?"
Phase 2: Brainstorm (brainstorming)
  ↓ "What are the design options?"
Phase 3: Spec / Plan / Tasks (spec-init)
  ↓ "What exactly are we building and how?"
Phase 4: Implementation (incremental-implementation)
  ↓ "Build, test, ship"
```

## Phase 1: Interview (if needed)

- Use `interview-me` skill
- One question at a time, with best guess attached
- Until you can predict what the user will say

## Phase 2: Brainstorm

- Use `brainstorming` skill
- HARD-GATE: no code before design approved
- Present 2-3 alternatives, recommend one with rationale

## Phase 3: 3 Artifacts (spec-init)

```
.specify/specs/NNN-<short-name>/
├── spec.md    (tech-agnostic, WHAT layer)
├── plan.md    (tech approach, HOW layer)
└── tasks.md   (implementation checklist)
```

R2 hard rule: `spec.md` MUST NOT contain tech-stack words. Tech names only in `plan.md` and `tasks.md`.

After writing 3 files, run `spec-check` to validate.

## Phase 4: Implementation (incremental-implementation)

- Follow `tasks.md` in order
- Mark `[P]` tasks as parallel
- Each slice: code + test + verify + commit
- R6: "done" requires pasted test output or documented manual verification

## Cross-cutting

- **Update PRD.md** if scope changes
- **Update TDD.md** if test approach changes
- **Append L##** to lessons-learned.md after every non-trivial fix
- **Update CHANGELOG.md** at release

## Anti-patterns

- Skip Phase 1 (assume you know what the user wants)
- Skip Phase 2 (code without design)
- Write spec.md with tech names (violates R2)
- Tasks without P1/P2/P3 + [USxxx] tags (violates R3)
- Tasks > 4 hours (violates R5)
- "Done" without test output (violates R6)
