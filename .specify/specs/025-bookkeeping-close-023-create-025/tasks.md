# 025 - Tasks - Bookkeeping: close 023, create 025 placeholder, commit pre-staged

> All tasks P3 (housekeeping, sub-15 min each, deterministic).
> `[P]` = parallelizable (different files, no dependency).

## Phase 1 - Pre-flight

- [ ] T001 [P] verify pre-staged materials are intact
  - 7 atlas files in `docs/Fluxing-code-map/`: 00-index, 01..06
  - 8 baseline files in `.specify/specs/001-user-visible-strings/baseline/`
  - 023 spec.md still has `0. Status: BLOCKED on spec 007`
  - 025 spec dir does not yet exist

## Phase 2 - 023 close + 025 placeholder

- [ ] T002 rewrite 023 spec.md `0. Status` to `CLOSED: merged into
  spec 024` (keep rest unchanged)
- [ ] T003 [P] create 025 spec.md (TDD 3.1 #5, blocked on 011)
- [ ] T004 [P] create 025 plan.md (placeholder, no implementation)
- [ ] T005 [P] create 025 tasks.md (placeholder checklist)

## Phase 3 - Commit pre-staged materials

- [ ] T006 [P] stage 7 atlas files to `docs/Fluxing-code-map/`
- [ ] T007 [P] stage 8 baseline files to `001-user-visible-strings/baseline/`
- [ ] T008 byte health on all 15 new + 1 modified files
  (no overlong, CRLF consistency, no GBK pollution per L01)

## Phase 4 - Pre-commit gate + release

- [ ] T009 update CHANGELOG.md with `[0.18.14.1-fluxing]` sub-section
- [ ] T010 run `scripts/run-tests.bat` (expect 6/6 still green)
- [ ] T011 stage 023, 025, atlas, baseline, CHANGELOG (explicit paths)
- [ ] T012 commit `docs(fluxing): spec 025 - close 023, create 025 placeholder, commit atlas + baseline`
- [ ] T013 tag v0.18.14.1 (lightweight) + push branch and tag to kizemo

## Done = evidence

- `git log --oneline -1` shows the new commit on `Fluxing`
- `git tag -l v0.18.14.1` shows the new tag
- `git status` shows working tree clean (excluding librime submodule)
- `scripts/run-tests.bat OUTER_RC=0`
- `git ls-files docs/Fluxing-code-map/ | wc -l` = 7
- `git ls-files .specify/specs/001-user-visible-strings/baseline/ | wc -l` = 8

