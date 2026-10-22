# Issue tracker

## Where issues live

GitHub Issues on the `kizemo/fluxing` fork:
<https://github.com/kizemo/fluxing/issues>

`origin` points to the upstream `rime/weasel` and is **read-only** for this
project — do not file issues there from this repo. All engineering skills
read and write through the `gh` CLI authenticated against the `kizemo`
remote.

## Primary CLI

- Read: `gh issue list --repo kizemo/fluxing`, `gh issue view <num>`
- Write: `gh issue create --repo kizemo/fluxing`, `gh issue edit <num> ...`
- Auth: `gh auth status` (must show the `kizemo` account; if not, run
  `gh auth switch` or re-login with `gh auth login --scopes "repo,read:org"`).

## Branch context

| Remote | Role | Use for |
|---|---|---|
| `kizemo/Fluxing` | Active development (this fork) | All issue filing, all triage work, all `gh` reads/writes |
| `origin/master` | Upstream `rime/weasel` | Read-only reference; do not push, do not file issues |

The `Fluxing` branch is the brand-fork worktree (P8 in
`.specify/memory/constitution.md`). Issues filed here describe the
**Fluxing / 火流猩输入法** product, not the upstream Weasel.

## Pull requests as a triage surface

**External PRs ARE a triage surface.** `/triage` pulls *external* pull
requests into the same queue and runs them through the same label and
state machine as issues. Collaborators' in-flight PRs (i.e. branches
that already have a `kizemo/` push or a linked issue authored by a
maintainer) are left alone.

Operational meaning:
- A new external PR gets a `needs-triage` label until evaluated.
- After evaluation, it carries exactly one of: `needs-info`,
  `ready-for-agent`, `ready-for-human`, `wontfix`.
- `/triage` will close or move to `wontfix` external PRs that duplicate
  an existing issue or are out of scope for the Fluxing brand fork.

## Tracker-specific gotchas

- **Chinese in issue body**: see L08 — `gh issue create --body "..."` may
  mangle CJK on Windows. Use `--body-file` with a UTF-8 file, or pass
  through `gh api --method POST repos/kizemo/fluxing/issues -f body=@file`.
- **Label color**: triage labels are created with neutral grey
  (`#cccccc`) so they sort after colored `bug` / `enhancement` labels
  in the GitHub UI.
- **Cross-link with specs**: when an issue becomes a spec, add a comment
  linking `.specify/specs/NNN-*` and apply the `spec` label. Conversely,
  spec `plan.md` should reference the originating issue number in its
  header.

## See also

- `docs/agents/triage-labels.md` — the labels `/triage` applies
- `docs/agents/domain.md` — how domain context (CONTEXT.md, ADRs) is read
- `.specify/memory/lessons-learned.md` L08 — GitHub API 422 on CJK
