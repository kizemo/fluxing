# 025 - Bookkeeping: close spec 023, create 025 placeholder, commit pre-staged materials

> **Scope**: pure bookkeeping. No production code change. Closes
> the spec 023 placeholder (already done by spec 024 / v0.18.14.0),
> creates the spec 025 placeholder for TDD.md sec 3.1 integration
> test #5 (TestBootstrapperStdio, blocked on spec 011), and commits
> two pre-staged untracked directories that have been sitting in the
> working tree since before v0.18.10.0:
> `docs/Fluxing-code-map/` (7-file project atlas) and
> `.specify/specs/001-user-visible-strings/baseline/` (8-file
> pre-image snapshot per spec 001 T016).

## 0. Why now (intent before implementation)

TDD.md sec 3.1 lists 6 integration tests. After v0.18.14.0:
- 2 shipped (TestBindingResolution spec 016/017/018,
  TestYamlRoundTripE2E spec 024).
- 3 placeholders exist (spec 020/021/022) for tests blocked on
  their parent specs (008/009/004 sec 9).
- 1 placeholder missing (TestBootstrapperStdio, blocked on
  spec 011 Fluxing Bootstrapper).
- 1 orphan placeholder (spec 023) that v0.18.14.0 effectively
  closed by shipping its work as spec 024.

Three housekeeping items are needed before any new P1 spec
starts: (a) close 023, (b) create 025 placeholder, (c) commit
the pre-staged docs and baseline so future agents can rely on
them being in git. None of these are code changes; all are
sub-15-minute, deterministic, and verifiable.

## 1. Acceptance criteria

- `.specify/specs/023-integration-test-yaml-roundtrip/spec.md` has
  its `0. Status` section rewritten to `CLOSED: merged into spec 024
  (v0.18.14.0, commit 4dbcced)`. The file remains in the tree as
  historical record.
- `.specify/specs/025-bootstrapper-stdio-placeholder/{spec,plan,tasks}.md`
  exist and follow the 023 placeholder template (status: BLOCKED on
  spec 011; plan when unblocked; acceptance when unblocked).
- `docs/Fluxing-code-map/{00..06}-*.md` (7 files) are committed to
  the `Fluxing` branch.
- `.specify/specs/001-user-visible-strings/baseline/*` (8 files)
  are committed to the `Fluxing` branch.
- AGENTS.md sec 5 five-step pre-commit gate passes: byte health,
  unit tests (6/6 still green), build hygiene, scope, format.
- A new commit is created on `Fluxing` with a single `docs(fluxing):`
  scope per P4.
- `CHANGELOG.md` gets a new `[0.18.14.1-fluxing]` entry under
  the existing 0.18.14.0 entry (bookkeeping-only, same code version).

## 2. Non-goals

- No production code change.
- No new test code (025 placeholder is text-only).
- No unblocking of 020/021/022 (still blocked on their parents).
- No scope expansion of spec 011 to allow 025 to ship.
- No edits to `librime` submodule, `env.bat`, `weasel.props`,
  `release/`, or any tracked production source.

## 3. References

- AGENTS.md sec 3.2 (P4 scope) and sec 5 (pre-commit gate).
- `.specify/specs/023-integration-test-yaml-roundtrip/spec.md`
  (the placeholder being closed).
- `.specify/TDD.md` sec 3.1 (6 integration tests).
- `.specify/specs/011-fluxing-bootstrapper/` (parent of 025).
- `docs/Fluxing-code-map/00-index.md` (atlas being committed).
- `.specify/specs/001-user-visible-strings/tasks.md` (T016 byte-level
  diff baseline).

