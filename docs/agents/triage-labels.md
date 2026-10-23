# Triage labels

The `/triage` skill runs incoming issues (and external PRs — see
`issue-tracker.md`) through a five-state machine. Each state maps to a
single GitHub label on `kizemo/fluxing`. **The label string equals the
state name** — no prefix, no color coding beyond a neutral grey.

## The five canonical states

| State | Label string | Applied when | Resolves to |
|---|---|---|---|
| Needs evaluation | `needs-triage` | New issue/PR; a maintainer must read and decide direction | One of the four below |
| Waiting on reporter | `needs-info` | Maintainer asked the author a question; can't proceed without an answer | `ready-for-agent` / `ready-for-human` / `wontfix` after reply |
| AFK agent pick-up | `ready-for-agent` | Fully specified, can be implemented by an autonomous agent with no human context | `wontfix` (or close as completed) after delivery |
| Human-only | `ready-for-human` | Requires a human in the loop (UX call, product decision, breaking change) | `wontfix` (or close as completed) after delivery |
| Will not action | `wontfix` | Out of scope, duplicate, or explicitly declined | Terminal |

## Why default strings?

`kizemo/fluxing` currently carries only the GitHub default labels
(`bug`, `enhancement`, `documentation`, `duplicate`, `question`,
`wontfix`, `invalid`, `help wanted`, `good first issue`) — **no triage
labels exist yet**. Using the role name as the label string:

- avoids creating near-duplicates of the existing `wontfix` (e.g. a
  prefixed `triage:wontfix` would split the same intent across two
  labels),
- keeps the `gh issue list --label ...` query short,
- matches what the engineering skills generate programmatically.

If the repo later adopts a prefix convention (e.g. `triage:*`), update
this file and the engineering skills' label constants together.

## How `/triage` uses these

1. New issue/PR → `gh issue edit <num> --add-label "needs-triage"`.
2. Maintainer (or `/triage` re-evaluation) picks one of the four
   terminal-or-pending states; **exactly one of the four** is applied at
   a time, replacing `needs-triage` (use `--add-label` then
   `--remove-label` in the same edit call).
3. `wontfix` is terminal — the issue/PR is closed in the same step
   (use `gh issue close <num> --comment "..."` after applying the label).
4. State transitions are append-only on the issue timeline; never
   delete triage labels, only swap.

## Compatibility with existing labels

- `bug`, `enhancement`, `documentation`, etc. continue to be applied as
  *category* labels (orthogonal to the triage state). A single issue
  can be `bug` + `needs-triage`, or `enhancement` + `ready-for-agent`.
- `wontfix` is a triage state **and** an existing GitHub default label
  with the same name — this is intentional; `gh issue list --label
  wontfix` will surface both, which is what we want.
- `good first issue` and `help wanted` remain human-curated and are
  applied by maintainers, not by `/triage`.

## See also

- `docs/agents/issue-tracker.md` — where these labels are read/written
- `docs/agents/domain.md` — how triage output feeds into specs/ADRs
