# 000 · Tasks — Fluxing Brand Fork, Slice 1

> Organization: Phase 1 = pre-flight validation; Phase 2 = foundational
> (none needed; the three files are independent); Phase 3 = per-user-story
> implementation; Final Phase = verification & commit.
> Each task: `- [ ] TNNN [P?] [Story] Description` where `[P]` = parallelizable,
> `[Story]` = US1 / US2 / US3 tag from `spec.md`.
> MVP = Phase 1 + Phase 3 US1 + Phase 3 US2 (i.e. all of T001–T010 except
> the US3 final-phase commit hygiene steps, which are still required).

## Phase 1 — Pre-flight validation (no code change)

- [ ] T001 [US2] Validate the supplied brand asset
  - Read `F:\soft\02office\rimetrae\hlx.ico`.
  - Assert: file exists; size == 67 646 bytes; ICO header type == 0x0001;
    image count == 1; first image 128×128, 32 bpp, BMP-in-ICO
    (PNG-in-ICO also acceptable).
  - Failure mode: stop and report which check failed. Do not modify any
    file.
- [ ] T002 [US1] Capture current `git status` and `git rev-parse HEAD`
  on the `Fluxing` branch. Save the output for the verification report.
  Expected: branch `Fluxing`, HEAD == `93eec2dc33dfcf04c356cce87732b638888fff4d`,
  working tree clean apart from the already-untracked `.specify/` and
  `docs/` directories.

## Phase 2 — Foundational (skipped)

No foundational task is required: the three targets (header constants,
primary icon, installer icon) are independent of one another. The
spec/plan/tasks documents can be written in any order; their final
ordering inside the single commit is decided in T011.

## Phase 3 — Per-user-story implementation

### User Story 1 — Internal product identity renamed (US1, P1, MVP)

- [ ] T003 [P] [US1] Edit `include/WeaselConstants.h`:
  - Change `WEASEL_CODE_NAME` from `"Weasel"` to `"Fluxing"`.
  - Change `WEASEL_REG_KEY` from `L"Software\\Rime\\Weasel"` to
    `L"Software\\Fluxing\\Fluxing"`.
  - Change `RIME_REG_KEY` from `L"Software\\Rime"` to
    `L"Software\\Fluxing"`.
  - Verify: `git diff -- include/WeaselConstants.h` shows exactly these
    three lines changed; no other line in the file is touched.
  - **FR-001 / FR-002 / FR-003 acceptance**.

### User Story 2 — Primary product icon swapped (US2, P1, MVP)

- [ ] T004 [P] [US2] Replace `resource/weasel.ico` with the brand asset
  - Copy `F:\soft\02office\rimetrae\hlx.ico` byte-for-byte to
    `F:\soft\00selfmade\rime\resource\weasel.ico`.
  - Verify: `git hash-object resource/weasel.ico` equals
    `git hash-object F:/soft/02office/rimetrae/hlx.ico`; `git status`
    shows the file as modified, not untracked (since the path already
    exists); file size == 67 646 bytes.
  - **FR-004 acceptance**.
- [ ] T005 [P] [US2] Replace `WeaselSetup/WeaselSetup.ico` with the
  brand asset
  - Copy `F:\soft\02office\rimetrae\hlx.ico` byte-for-byte to
    `F:\soft\00selfmade\rime\WeaselSetup\WeaselSetup.ico`.
  - Verify: same as T004 but for the installer-icon path.
  - **FR-005 acceptance**.

### User Story 3 — Changelog and docs untouched (US3, P2, MVP)

- [ ] T006 [US3] Confirm no out-of-scope files were modified
  - Run `git status` and `git diff --stat`; verify that the only
    modified paths are: `include/WeaselConstants.h`,
    `resource/weasel.ico`, `WeaselSetup/WeaselSetup.ico`.
  - Verify `CHANGELOG.md` is untouched (`git diff -- CHANGELOG.md`
    is empty) — the entry will be added in the next slice per
    deferral D-1 in `plan.md`.
  - **FR-006 / US3 acceptance**.

## Final Phase — Verification & commit

- [ ] T007 [US1] Self-check the `spec.md` against R2 (no tech-stack
  words) — see `plan.md` Complexity Tracking. This is a one-line
  `Select-String` over `spec.md` for the disallowed words. Pass = no
  hits in the user-story / FR / SC / Out-of-scope sections.
- [ ] T008 [US1] Probe build system availability, then attempt a
  clean build
  - Probe: `where.exe xmake` (or `xmake --version`) and
    `where.exe cl` (or MSBuild path). If neither is available,
    record the manual verification steps and skip the build run
    (per spec.md E2 and R6).
  - If xmake is available: run
    `xmake f -a x64 -m release && xmake` from the repo root and
    capture stdout/stderr. The expected outcome is a successful
    build of all `xmake.lua` targets. **SC-001 acceptance**.
  - **Do not** claim success if the build was not actually run; paste
    the captured output instead.
- [ ] T009 [US1] Confirm diff scope
  - Run `git diff --name-only <PRE_COMMIT_SHA>..` (where
    `PRE_COMMIT_SHA` is the SHA captured in T002) and verify the
    set is exactly: the 3 code/icon files plus the 3 new spec
    documents (6 modified + 3 untracked-before-add = 7 paths in
    the final commit). **SC-002 acceptance**.
- [ ] T010 [US1] Run the code-style formatter
  - `clang-format.ps1 -i` (or the equivalent `clang-format -i` on
    each `.cpp`/`.h` file in the diff) — there is exactly one such
    file in this slice: `include/WeaselConstants.h`.
  - `git diff -- include/WeaselConstants.h` MUST be byte-identical
    to its pre-format state. **SC-003 acceptance**.
- [ ] T011 [US1] Cross-check the three spec/plan/tasks documents
  - Run a quick `spec-check`-style audit (the agent reads the
    three documents and reports findings):
    1. Every FR in `spec.md` is addressed by at least one T### in
       `tasks.md`.
    2. Every `T###` in `tasks.md` references a real file path that
       exists in the repository.
    3. `plan.md` Constitution Check has no FAIL row.
  - **R7 acceptance**.
- [ ] T012 [US1] Stage and commit the slice as a single atomic commit
  - `git add include/WeaselConstants.h resource/weasel.ico WeaselSetup/WeaselSetup.ico .specify/specs/000-fluxing-brand-fork/`
  - `git commit -m "feat(fluxing): rename distribution code to Fluxing; swap brand icon to hlx.ico" -m "- include/WeaselConstants.h: WEASEL_CODE_NAME / WEASEL_REG_KEY / RIME_REG_KEY -> Fluxing namespace" -m "- resource/weasel.ico, WeaselSetup/WeaselSetup.ico: replaced with user-supplied hlx.ico (128x128 32-bpp BMP-in-ICO)" -m "- .specify/specs/000-fluxing-brand-fork/: spec / plan / tasks for Slice 1 of the brand fork" -m "- see plan.md D-1 (CHANGELOG deferral) and D-2 (P6 path activation deferral) for out-of-scope items intentionally left for the next slice"`
  - **FR-007 acceptance**.
- [ ] T013 [US1] Post-commit verification
  - `git log -1 --stat` to show the commit summary.
  - `git show --stat HEAD` to list the touched paths.
  - `git rev-parse HEAD` to capture the new commit SHA for the next
    slice's CHANGELOG reference (per deferral D-1).
  - **R6 / R8 acceptance**.
