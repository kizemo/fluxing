---
name: spec-check
description: Cross-validate spec.md / plan.md / tasks.md against the project constitution for consistency, coverage, and drift. Use after spec-init produced the 3 files, before implementation. Do NOT use during early brainstorming (use interview-me first).
---

# Spec-Check: Cross-Document Consistency Audit

> Adapted from `github/spec-kit`'s `/speckit.analyze`. Reads the 3 core artifacts and reports findings by severity.

## When to use

- After `spec-init` produced spec.md / plan.md / tasks.md
- Before starting implementation of a feature
- When user reports a conflict between documents

## Required inputs

- `.specify/specs/NNN-*/spec.md` (required)
- `.specify/specs/NNN-*/plan.md` (required)
- `.specify/specs/NNN-*/tasks.md` (required)
- `.specify/memory/constitution.md` (project constitution - binding rules)
- `AGENTS.md` (project operating manual)

## Procedure

### 1. Pre-flight

- Locate spec directory
- Confirm 3 required files exist
- If any missing → report and abort

### 2. Constitution Check (R4 hard rule)

`plan.md` MUST have a "## Constitution Check" section with **all 5 principles**:

| Principle | Status | Notes |
|---|---|---|
| I. Intent Before Implementation | ✓/✗ | spec.md has What/Why/Acceptance? |
| II. Test-Backed Change | ✓/✗ | tasks include regression tests? |
| III. Spec-Artifact Discipline | ✓/✗ | 3 files complete? |
| IV. Structured Clarification | ✓/✗ | Unknowns marked [NEEDS CLARIFICATION]? |
| V. Incremental Delivery | ✓/✗ | Each user story shippable alone? |

Plus R1-R9 (Hard Rules) and P1-P8 (Project-Specific Rules):
- R2: spec.md has no tech-stack words (grep WTL|Boost|C++|MFC|ATL returns 0)
- R3: tasks have P1/P2/P3 + [USxxx] labels
- R4: Constitution Check table is filled
- R5: Tasks ≤ 4h, 1-3 files
- R6: Verification (test output or manual steps) documented
- R7: One source of truth per concept
- R8: Specs versioned in git
- R9: Library/framework lookups done (include/*.h, librime/README)
- P1-P8: Weasel-specific rules (per constitution)

### 3. Cross-File Consistency Audit

| Check | What to look for |
|---|---|
| Scope coverage | Every FR in spec.md has ≥1 task in tasks.md |
| Tech consistency | plan.md tech names match task file paths |
| Risk coverage | Each R-### in spec.md has a mitigation in plan.md |
| Acceptance | Every SC-### in spec.md is verifiable per V### in tasks.md |
| Out-of-scope | spec.md "Out of Scope" items DO NOT appear in tasks.md |

### 4. Report findings by severity

- **🔴 BLOCKER**: Constitution violation (R2 / R4 / R5)
- **🟡 WARNING**: Inconsistency between files, or missing acceptance criteria
- **🟢 INFO**: Style nit, optional improvement

### 5. Remediation offer

For each BLOCKER and WARNING, suggest specific fix (file + line + new text).

**Do not start implementation** until all BLOCKERs are resolved.
