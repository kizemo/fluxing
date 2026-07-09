---
name: doubt-driven-development
description: Adversarial review of non-trivial decisions BEFORE they stand. Use when correctness matters, in unfamiliar code, when stakes are high (production, security, irreversible). A confident answer is not a correct one.
---

# Doubt-Driven Development

> Confident answer ≠ correct answer. Long sessions accumulate context that turns assumptions into "facts" without anyone noticing. Materialize a fresh-context reviewer — biased to **disprove**, not approve.

## When to use (decision is non-trivial if ≥1 true)

- Introduces or modifies branching logic
- Crosses module/service boundary
- Asserts a property the type system or compiler cannot verify
- Correctness depends on context the future reader cannot see
- Blast radius is irreversible (production deploy, data migration, public API)

## When NOT to use

- Trivial one-line change
- Mechanical refactor (use clean-code-guard)
- Style nit

## Procedure

### 1. Articulate the decision

State it as: "I am about to do X because Y. The blast radius is Z."

### 2. Spawn an adversarial reviewer

Use `Agent` (or a sub-agent / fresh-context task) to:
- Read the relevant code (with NO prior context)
- Try to **disprove** the decision
- Specifically attack: correctness, missing cases, blast radius, simpler alternative

### 3. Apply the review

For each finding:
- **BLOCKER**: decision is wrong, must change
- **WARNING**: decision is suboptimal, should consider alternative
- **OK**: reviewer's argument was weaker than your reasoning, proceed

### 4. Document the resolution

If the reviewer found something, record in spec.md or commit message:
> "Decision X was reviewed by [reviewer]; raised concern Y; resolved by Z."

## Project-specific concerns

- **L10 / L14**: easy to "just re-enable x64" — reviewer should ask "what's the blast radius if librime is still x86?"
- **L17**: install.nsi changes look harmless but `${If} ${RunningX64}` footgun (L11) bites silently
- **L19 / L21**: Shift binding edge cases — reviewer should check spec 005 SC-005-5
- **L49**: ATL message map is runtime — reviewer should ask "is the function actually reachable from msg map?"

## Anti-patterns

- "I've been working on this for hours, I'm sure it's right" (overconfidence)
- Reviewer = same context = no new info (use a sub-agent)
- "The reviewer didn't find a bug, ship it" (false negative possible — look for absent cases, not just present errors)
