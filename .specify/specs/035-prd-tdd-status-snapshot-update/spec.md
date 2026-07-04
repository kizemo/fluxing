# 035 - PRD/TDD status snapshot update (close stale R-008 + sync 0.18.6 -> 0.18.23.0)

> Docs-only spec. The v2 PRD.md and TDD.md status snapshots are
> frozen at 2026-07-01 (v0.18.6.0) but the project has shipped 17
> more releases through v0.18.23.0. R-008 (CI not running tests) is
> listed as a high/high risk but is actually closed: ci.yml has a
> test job that runs `scripts\run-tests.bat` -> wrapper ->
> `scripts\test-infra\run-test-suite.bat` -> 13/13 test projects.

## 0. Status: READY

- spec 034 (TestDarkModeBroadcast) shipped in v0.18.23.0.
- All 13 test projects PASS (verified 2026-07-04 post-0.18.23.0).
- ci.yml test job: exists, runs all 13 tests on push (per spec 027).
- L40 (PRD/TDD corruption lesson) was about a historical
  literal-`?`-substitution bug; the CURRENT PRD.md and TDD.md
  files are healthy Chinese UTF-8 (1599 + 2370 CJK characters, 0
  0x3F bytes - byte-verified 2026-07-04).

## 1. Goal

Update `.specify/PRD.md` and `.specify/TDD.md` to reflect the real
current state without rewriting history:

1. **PRD.md sec 10 状态快照** - append entries for v0.18.7.0
   through v0.18.23.0 (17 versions). Existing 2026-07-01 entry
   preserved (per R7 / one source of truth / do not rewrite
   history).
2. **TDD.md sec 10 状态快照** - same pattern.
3. **PRD.md sec 7 R-008** - mark as CLOSED (mitigation actually
   shipped in v0.18.7.0-v0.18.23.0; the test job exists and runs).
4. **TDD.md sec 8 已知测试 gap** - mark 3 of 4 gaps as CLOSED
   (gap 1: TestBindingResolution shipped v0.18.10.0; gap 2:
   TestPanelDarkModeSubscribe shipped v0.18.22.0 + spec 034
   TestDarkModeBroadcast shipped v0.18.23.0; gap 3:
   TestUserDictUpdate shipped v0.18.17.0). Gap 4 (rime_deployer
   --debug) is reclassified: there is no rime_deployer.exe binary
   in librime dist; the test as written is not implementable
   without a spec change. Reclassify as a spec-needed (deferred
   to a future spec) rather than a test gap.

## 2. Out of scope (deferred)

- Do NOT rewrite historical entries. The 2026-07-01 entry stays
  as it was written; we add new entries below it.
- Do NOT change the 3.1 integration test list (6 tests, 4 done,
  2 blocked) - that is the architecture, not a snapshot.
- Do NOT change the risk table structure; only update the R-008
  status from open to closed.
- Do NOT change the test pyramid or coverage strategy - those are
  forward-looking design, not snapshots.

## 3. Dependencies

- The git log (commits db70099 .. 696628e for the snapshot range).
- The 17 release commits: db70099 (v0.18.6.0), 5da33be (v0.18.7.0),
  9ae8f5b (v0.18.8.0), e4095f2 (v0.18.9.0), bf6b4e1 (v0.18.10.0),
  83a91c2 (v0.18.11.0), 2863c8a (v0.18.12.0), 0a3e8c4 (v0.18.13.0),
  4dbcced (v0.18.14.0), f2a7e15 (v0.18.15.0), e8d9c5e (v0.18.16.0),
  8b8c2d4 (v0.18.17.0), 3e6c5fa (v0.18.18.0), 4ae2c1b (v0.18.19.0),
  e50b2d3 (v0.18.20.0), 46ee0b7 (v0.18.20.1), 0b29703 (v0.18.20.0
  re-ship), 80e209d (v0.18.21.0), 1149a63 (v0.18.22.0), 696628e
  (v0.18.23.0). Actual SHAs are looked up at impl time; the
  patterns above are illustrative.
- L01 (PowerShell 5.1 + GBK) - the docs-only update uses
  byte-level writes (no `Get-Content` / `Set-Content`).
- L02 (Chinese edits use byte-level replace) - the entry append
  is a single byte-level splice at the correct UTF-8 offset.
- L40 (PRD/TDD corruption - historical, not current) - the
  current files are healthy; the L40 lesson is referenced for
  the verification discipline (CJK byte count + 0x3F count +
  visual sample, NOT just git hash-object).
- L44 (Encoding.UTF8.GetString returns char-indexed) - the
  offset arithmetic is done on bytes directly, NOT on the
  string-decoded version.

## 4. Success criteria

### 4.1 Functional

- PRD.md sec 10 has 18 status-snapshot entries (1 existing 2026-
  07-01 + 17 new ones).
- TDD.md sec 10 has the same 18 entries (mirror of PRD).
- PRD.md sec 7 R-008 status changes from open to CLOSED (in
  place; the column for status is the rightmost; update the
  value from the open marker to a closed marker).
- TDD.md sec 8 gap table: 3 of 4 gaps marked CLOSED; gap 4
  reclassified as spec-needed.

### 4.2 Verification (byte-level, per L40 + L44)

- Both files end with CR=LF, loneLF=0 (CRLF only, per L09 / L37).
- 0xC0 / 0xC1 count: 0 (no GBK pollution per L01).
- 0x3F count: 0 (no literal-`?` substitution per L40; the
  current values are 0 and must stay 0 after the append).
- CJK char count: increases by the count of new CJK characters
  added (the new entries are mostly English, so the increase is
  small but non-zero because of section numbers / references).
- diff against HEAD: only INSERTIONS, no MODIFICATIONS of
  existing lines (R7 one source of truth).
- git diff --numstat: each file shows insertions only, no
  deletions.

## 5. References

- PRD.md sec 7 (risk table) and sec 10 (status snapshot).
- TDD.md sec 8 (test gap table) and sec 10 (status snapshot).
- spec 004 sec 7 SC-005 (the rime_deployer --debug reference;
  reclassified as spec-needed because rime_deployer.exe does not
  exist in librime dist).
- L01 (PowerShell 5.1 + GBK codepage trap).
- L02 (Chinese edits use byte-level replace).
- L09 (NSIS BOM + OutFile + line endings - we do not touch
  install.nsi, but the CRLF discipline applies).
- L40 (PRD/TDD corruption historical - documents why this spec
  exists and what the verification discipline is).
- L44 (Encoding.UTF8.GetString returns char-indexed, not byte).
- ci.yml (the test job that closes R-008).
- git log for v0.18.7.0 through v0.18.23.0 (the 17 commits
  to summarize in the new snapshot entries).

## 6. Plan / tasks (in `plan.md` and `tasks.md`)

See `plan.md` for the technical approach (byte-level splice at
the correct UTF-8 offset, preserving existing CR=LF line endings)
and `tasks.md` for the atomic task list (T001-T004).
