# 035 - Tasks - PRD/TDD status snapshot update

> Atomic task list. Each T### is 1-2 files and <1h. Run in order
> T001 -> T006. T001-T003 are independent (each updates a different
> file or section); T004 must run after T001-T003 to verify the
> cumulative byte health; T005 closes R-008 by editing the PRD risk
> table; T006 commits.

## Phase 1 - snapshot append (T001-T003)

- [ ] T001 [P1] Append 17 new status-snapshot entries to PRD.md
      sec 10. Format per plan.md sec 2.2. Insert AFTER the existing
      2026-07-01 entry, BEFORE the `---` separator that closes the
      section (or at end of file if no separator).
      UTF-8 no BOM, CRLF. Verify: byte-level (per L02 / L40 / L44).

- [ ] T002 [P1] Append 17 new status-snapshot entries to TDD.md
      sec 10. Mirror of T001 (TDD content is shorter and has fewer
      bullets per entry, but the structure matches).
      UTF-8 no BOM, CRLF. Verify: byte-level.

- [ ] T003 [P1] Update TDD.md sec 8 test gap table:
      - Row 1 (librime binding acceptance): append "**CLOSED v0.18.10.0** - TestBindingResolution 6/6 PASS"
      - Row 2 (WM_SETTINGCHANGE -> panel切色): append "**CLOSED v0.18.22.0 + v0.18.23.0** - TestPanelDarkModeSubscribe + TestDarkModeBroadcast 14/14 + 3/3 PASS"
      - Row 3 (user_dict_update(-1) 真删): append "**CLOSED v0.18.17.0** - TestUserDictUpdate 4/4 PASS"
      - Row 4 (rime_deployer --debug): change status to "**SPEC-NEEDED** - rime_deployer.exe does not exist in librime dist; WeaselServer uses rime_api->deploy directly. Spec 004 SC-005 wording needs update; deferred to a future spec."
      UTF-8 no BOM, CRLF. Verify: byte-level.

## Phase 2 - R-008 close (T005)

- [ ] T004 [P1] Update PRD.md sec 7 R-008 row: append "**CLOSED v0.18.7.0** - ci.yml test job exists and runs scripts\run-tests.bat (13/13 test projects, post-v0.18.23.0 verified 2026-07-04)" to the mitigation column. Edit the EXACT line at the byte offset found by byte-level search for the R-008 prefix (do NOT use string-layer search per L44).
      UTF-8 no BOM, CRLF. Verify: byte-level; the row line is
      now longer by the appended text.

## Phase 3 - verify (T005)

- [ ] T005 [P1] Byte-level verification of PRD.md and TDD.md:
      - Both files: CR=LF (every LF preceded by CR)
      - Both files: 0xC0/0xC1 count = 0 (no GBK pollution)
      - Both files: 0x3F count = 0 (no literal-`?` substitution)
      - PRD.md CJK char count >= 2370 (baseline) + delta
      - TDD.md CJK char count >= 1599 (baseline) + delta
      - git diff --numstat: only insertions, no deletions
      - git diff <file>: every changed line is + (insertion),
        never - (deletion) of an existing line
      Capture verification output to C:\TEMP\spec-035-verify.log.

## Phase 4 - commit (T006)

- [ ] T006 [P1] Commit (docs-only):
      - `git add .specify/PRD.md .specify/TDD.md` (explicit paths,
        A10)
      - Commit message: `docs(memory): spec 035 - PRD/TDD status
        snapshot update (close R-008, sync 0.18.6 -> 0.18.23.0)`
      - Tag: NONE (no installer, no release - this is docs only)
      - Push to kizemo/Fluxing (P8)

## Anti-patterns to avoid (per plan.md sec 5)

- [ ] AP-035-A: do NOT use `Encoding.UTF8.GetString(bytes).IndexOf()`.
- [ ] AP-035-B: do NOT use here-string `@"..."@` for the new content.
- [ ] AP-035-C: do NOT use `Get-Content` to read PRD/TDD.
- [ ] AP-035-D: do NOT use `Add-Content` to append.
- [ ] AP-035-E: do NOT use `git add .`.
- [ ] AP-035-F: do NOT rewrite historical entries. Append only.
- [ ] AP-035-G: do NOT mark rime_deployer --debug gap as CLOSED.
      Reclassify as SPEC-NEEDED.
- [ ] AP-035-H: do NOT skip the byte-level verification.
