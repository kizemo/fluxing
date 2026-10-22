# Architecture Decision Records

This directory holds Architecture Decision Records (ADRs) for the
**Fluxing / 火流猩输入法** brand fork of `rime/weasel`. ADRs are
short, durable records of an architectural choice: what was decided,
what context forced the decision, and what became easier or harder
because of it.

ADRs are **post-hoc records**, not pre-implementation plans. The
implementation plan lives in `.specify/specs/NNN-*/`; the ADR captures
*why* the plan was chosen, in language that outlives the plan and
the spec.

Engineering skills read this directory opportunistically — a skill
only consults ADRs that are topically relevant to the current task.
Do not dump all ADRs into every prompt.

## When to write an ADR

Write one when the decision:

- pins the project to a constraint that future changes will have to
  honor (e.g. "we ship only x86 because of PPL + librime Win32-only"),
- introduces a new vocabulary term that belongs in `CONTEXT.md` §3,
- changes an existing invariant (`CONTEXT.md` §4) or constitution
  principle (`.specify/memory/constitution.md`),
- is the kind of choice where "we did this six months ago, why?"
  is a real question future-you (or a future skill) will ask.

Do **not** write an ADR for:

- a bug fix with a clear root cause (use `.specify/memory/lessons-learned.md`),
- an implementation detail inside a single module (use a code comment),
- a routine feature spec (use `.specify/specs/NNN-*/`).

## File format

Filename: `NNNN-kebab-case-summary.md`, zero-padded to four digits.
Start at `0001`. Pick the next number that does not collide with an
existing file in this directory.

Each ADR is **self-contained** and uses the four-section MADR shape:

```markdown
# NNNN. Title

- **Status**: Proposed | Accepted | Superseded by NNNN
- **Date**: YYYY-MM-DD
- **Deciders**: <name, agent, or "Fluxing maintainers">
- **Related**: <issue #>, <spec NNN>, <PR #> (any of these, as applicable)

## Context

What was the situation? What forces were at play? What constraints
(constitutional, technical, user-data) shaped the choice?

## Decision

What was chosen? State it in the imperative ("We …", "The fork …").
If alternatives were considered, list them with a one-line rejection
reason each.

## Consequences

What becomes easier? What becomes harder? What new constraints does
this impose on future changes? If the decision is reversible, say so;
if it is not, say *that* too.
```

### Worked example (skeleton, do **not** add unless a real decision matches)

```markdown
# 0001. WeaselServer runs as Protected Process Light on Windows

- **Status**: Accepted
- **Date**: 2026-07-09
- **Deciders**: Fluxing maintainers
- **Related**: constitution §P7, lessons L17, L18

## Context

WeaselServer supervises the RIME engine and must survive arbitrary
user logon activity. On Windows, PPL prevents standard user processes
from terminating or injecting into the supervisor, which protects
user-dictionary integrity against tampering by malware running as the
same user. The cost is that legitimate replacement of WeaselServer.exe
during an upgrade also fails — `taskkill /F` is denied, and the new
binary will not load until the user logs out or reboots.

## Decision

WeaselServer.exe ships as a PPL binary. Installer upgrades replace
the file on disk but the running process is replaced only at the next
logon or reboot. The user is informed via a "logout to apply update"
prompt at the end of the installer.

## Consequences

- ✅ User-dictionary integrity is preserved against same-user tampering.
- ✅ Crash-recovery is simpler (WeaselServer cannot be killed mid-deploy).
- ⚠️ Inner-loop testing of WeaselServer changes requires a logout/reboot
  per iteration; this is the dominant cost in PPL-aware development.
- ⚠️ Auto-updates cannot apply in-session; users on long-running RDP
  sessions must explicitly log out for upgrades to take effect.
```

## Lifecycle

- `Status: Proposed` — drafted but not yet adopted; expect revisions.
- `Status: Accepted` — adopted. This is the canonical record of the
  decision. Do not edit the body of an accepted ADR; supersede it
  with a new ADR that links back.
- `Status: Superseded by NNNN` — replaced by a later decision. The
  file stays in the directory for history; do not delete.

## Cross-references

- **Vocabulary updates**: if an ADR introduces a new term, add it to
  `CONTEXT.md` §3 in the same change.
- **Invariant updates**: if an ADR changes a rule in `CONTEXT.md` §4
  or `.specify/memory/constitution.md`, the change is **major** and
  requires an amendment to the constitution (governance §Amendment
  Process) and a fresh ADR.
- **Spec linkage**: if a spec implements a decision recorded here,
  cite the ADR number in `spec.md` and `plan.md`.

## See also

- [CONTEXT.md §7](../CONTEXT.md#7-adrs-architecture-decision-records) —
  the consumer-facing description of this directory
- [AGENTS.md §9.3](AGENTS.md#9-agent-skills) — the engineering-skills
  pointer to this directory
- [docs/agents/domain.md](docs/agents/domain.md) — the consumer rules
  for `CONTEXT.md` and `docs/adr/`
- [.specify/memory/constitution.md](.specify/memory/constitution.md) —
  binding principles and the amendment process
