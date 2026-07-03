# 025 - Plan - Bookkeeping: close 023, create 025 placeholder, commit pre-staged

## Technical context

- **Files touched**: ~17 new (3 spec 025 + 7 atlas + 8 baseline),
  1 modified (spec 023 status), 1 appended (CHANGELOG).
- **No source code, no test code, no installer change.**
- **No version bump** (env.bat / weasel.props are gitignored; 025
  is a 0.18.14.1 bookkeeping sub-release of 0.18.14.0).
- **Build system**: none invoked. No `xbuild.bat`, no `msbuild`.
  Pre-commit gate runs the existing test suite only.

## Step-by-step

### T001 - Close spec 023 placeholder

Rewrite the `0. Status` section of
`.specify/specs/023-integration-test-yaml-roundtrip/spec.md` from
`BLOCKED on spec 007` to
`CLOSED: merged into spec 024 (v0.18.14.0, commit 4dbcced) - the
YamlRoundTrip module + TestYamlRoundTripE2E test were shipped as
spec 024 per the user's "选项 1" decision on spec 019; this
placeholder remains in the tree as historical record`. Keep the
rest of the spec unchanged.

### T002 - Create 025 placeholder spec

Write `.specify/specs/025-bootstrapper-stdio-placeholder/spec.md`,
`plan.md`, `tasks.md` mirroring the 023 template (TDD 3.1 #5,
blocked on spec 011).

### T003 - Commit docs/Fluxing-code-map/

Stage the 7 atlas files (00-index.md, 01-overview.md through
06-customization-points.md). Verify each is CRLF, UTF-8 (no GBK
pollution per L01).

### T004 - Commit 001 baseline/

Stage the 8 baseline files (CHANGELOG.md, install.nsi,
PRE_COMMIT_SHA, 4 .rc files, WeaselUtility.h). Verify encoding
and line endings per AGENTS.md sec 4.1 / L09.

### T005 - Update CHANGELOG

Add `[0.18.14.1-fluxing]` sub-section under the existing
`[0.18.14.0-fluxing]` heading. Note: bookkeeping-only, same code
version, 0 production changes.

### T006 - Pre-commit gate

Run AGENTS.md sec 5 five steps: byte health on all touched files,
`scripts/run-tests.bat` (expect 6/6 still green), build hygiene
(no new .log/.token), format (no clang-format locally; CI will
check), scope check (single `docs(fluxing):` scope per P4).

### T007 - Commit, tag, push

Single commit on `Fluxing` with `docs(fluxing):` scope. Tag
v0.18.14.1 (lightweight) per the brand-fork release convention.
Push to `kizemo/Fluxing` and `kizemo/v0.18.14.1`.

## Constitution check

| Rule | Status | Note |
|---|---|---|
| R1 intent+acceptance | OK | Section 0+1 of spec.md |
| R2 spec separate from plan | OK | spec.md is text-only, plan.md lists files |
| R3 single priority | OK | All tasks P3 (housekeeping) |
| R4 constitution gate | OK | This table |
| R5 task size < 4h, 1-3 files | OK | Each T### is one file or one directory |
| R6 done = evidence | OK | Pre-commit gate output is the evidence |
| R7 one source of truth | OK | 023 status update is in spec.md, not duplicated |
| R8 specs versioned in git | OK | spec/plan/tasks committed |
| R9 lookup beats memory | OK | This spec is the result of repo recon |

