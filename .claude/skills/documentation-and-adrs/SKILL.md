---
name: documentation-and-adrs
description: Capture decisions and context, not just code. Use for architectural decisions, public API changes, shipping features, onboarding new engineers/agents.
---

# Documentation and ADRs

> The most valuable documentation captures the **why** — context, constraints, trade-offs. Code shows what; documentation explains why it was built this way and what alternatives were considered.

## When to use

- Architectural decision (e.g., "use libuv vs Boost.Asio")
- Choosing between competing approaches
- Adding or changing a public API
- Shipping a feature that changes user-facing behavior
- Onboarding new team / agent
- Explaining the same thing repeatedly

## Project documentation layers (Fluxing)

| Layer | File | Audience | Updated when |
|---|---|---|---|
| Constitution | `.specify/memory/constitution.md` | All agents / devs | Amendment (P-R rare) |
| Operating manual | `AGENTS.md` | All agents / devs | When build process changes |
| Project knowledge | `.specify/memory/project-knowledge.md` | All agents | When architecture changes |
| Lessons learned | `.specify/memory/lessons-learned.md` | Future agents | After every non-trivial fix |
| Spec | `.specify/specs/NNN-*/spec.md` | Implementer + reviewer | Per feature |
| Plan | `.specify/specs/NNN-*/plan.md` | Implementer | Per feature |
| Tasks | `.specify/specs/NNN-*/tasks.md` | Implementer | Per feature |
| Design | `.specify/specs/NNN-*/design.md` | Implementer | When design needs detail |
| CHANGELOG | `CHANGELOG.md` | End users | Per release |
| Code map | `docs/Fluxing-code-map/*` | New contributors | When architecture changes |
| README | `README.md` | End users | Per release |
| INSTALL | `INSTALL.md` | End users | When install process changes |

## ADR template

```markdown
# ADR-NNN: <title>

## Status
Proposed | Accepted | Deprecated | Superseded by ADR-MMM

## Context
What is the issue? What forces are at play?

## Decision
What did we choose?

## Consequences
What becomes easier? What becomes harder?

## Alternatives considered
- **Option A**: ... rejected because ...
- **Option B**: ... rejected because ...
```

## When NOT to write an ADR

- Trivial implementation detail
- Style choice (use .clang-format)
- Test that passes (that's the test's job)

## Rime/Weasel-specific

- L## entries in lessons-learned.md are **de facto ADRs** for post-mortems
- Each L## includes: symptom, root cause, fix, lessons, related, anti-patterns
- Convention: append new L## at the end, don't renumber
- Format: see existing L01-L55 for template
