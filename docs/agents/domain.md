# Domain docs

This repo is **single-context**: one `CONTEXT.md` at the project root,
one `docs/adr/` directory at the root, and one `docs/agents/` directory
(this folder) for the Matt Pocock engineering-skills configuration.

There is no monorepo layout. Even though the code mixes upstream Weasel
sources with the Fluxing brand fork, the *domain* is one product —
**Fluxing / 火流猩输入法** — so a single context file covers both.

## Files engineering skills read

| Path | Read by | Purpose |
|---|---|---|
| `CONTEXT.md` (root) | `/improve-codebase-architecture`, `/diagnosing-bugs`, `/tdd`, `/huff` | Project vocabulary, terminology, invariants |
| `docs/adr/NNNN-*.md` | `/improve-codebase-architecture`, `/diagnosing-bugs` | Past architectural decisions and their rationale |
| `docs/agents/issue-tracker.md` | `/triage`, `/to-tickets`, `/to-spec`, `/qa` | Where issues live and how to read/write them |
| `docs/agents/triage-labels.md` | `/triage` | Triage state machine label strings |
| `docs/agents/domain.md` (this file) | `/improve-codebase-architecture`, `/diagnosing-bugs`, `/tdd` | Where to find domain context |

## Consumer rules

`CONTEXT.md` (when it exists) MUST be read **before** any of the skills
listed above runs against this repo. If `CONTEXT.md` is missing, the
skill should still proceed but should emit a warning that domain
context is absent.

`docs/adr/` is read **opportunistically** — a skill only reads the
ADRs that are topically relevant to the current task. Do not dump all
ADRs into every prompt.

`AGENTS.md` and `.specify/memory/constitution.md` are **not** the
domain context; they are the operating manual and the project
constitution respectively. They sit *above* the domain, not inside it.

## Related but distinct directories

These directories look like domain docs but serve different roles —
the engineering skills should treat them as **references**, not as
the source of domain truth:

| Path | Role | Read when |
|---|---|---|
| `docs/Fluxing-code-map/` | Read-only map of the upstream Weasel codebase (snapshot @ `93eec2d` / weasel 0.17.4). Describes the *code*; does not describe *this project's decisions*. | The skill needs to understand the upstream API surface or module layout. |
| `docs/design/` | Design HTML mockups for the Quick Settings panel (v1/v2/v3). | The skill is working on a UI feature, especially tray/quick-settings. |
| `.specify/memory/constitution.md` | Project constitution: 5 principles, 9 hard rules, 8 project rules. **Binding.** | Every task — the skill should cite the relevant principle/rule in its plan. |
| `.specify/memory/lessons-learned.md` | Post-mortems `L01`–`L15` from real incidents. | The skill is fixing a bug or recovering from a regression; check the relevant L## first. |
| `.specify/memory/project-knowledge.md` | Free-form project knowledge and links. | The skill is exploring how the project is organized. |
| `.specify/specs/NNN-*/` | Per-feature specs, plans, and tasks. | The skill is implementing a feature that has a spec; the spec is the authoritative breakdown. |
| `CHANGELOG.md` | Per-release changelog entries (already published up through `0.18.35.0`). | The skill is preparing a release; `extract_changelog.ps1` reads from here. |

## Adding a new ADR

When `/improve-codebase-architecture` or a manual decision produces a
record-worthy architectural choice:

1. Pick the next four-digit number (`docs/adr/0001-...md` already
   exists if the directory is populated; otherwise start at `0001`).
2. Filename: `NNNN-kebab-case-summary.md`.
3. Required sections: `## Status` (Proposed / Accepted / Superseded),
   `## Context` (what was the situation), `## Decision` (what was
   chosen), `## Consequences` (what becomes easier/harder).
4. Update `CONTEXT.md` if the decision introduces new vocabulary or
   changes an invariant.
5. Cite the originating issue/spec number in the file header.

## See also

- `docs/agents/issue-tracker.md` — issue tracker setup
- `docs/agents/triage-labels.md` — triage label vocabulary
- `AGENTS.md` — operating manual (build, test, lint, anti-patterns)
- `.specify/memory/constitution.md` — project constitution
