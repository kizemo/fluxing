# 035 - Plan - PRD/TDD status snapshot update

> Plan layer. Docs-only spec. NO production code or test changes.

## 1. Technical approach

### 1.1 Byte-level splice at UTF-8 offset

Both PRD.md and TDD.md are UTF-8 with CRLF line endings. The status
snapshot section (§10) is at a known byte offset (find via byte-
level search for the section header `## 10. 状态快照` - the UTF-8
byte sequence for that header is 5 bytes per Chinese char + 9 ASCII
chars; precise offset varies as the file grows).

The splice:
1. Read the entire file as bytes via `[IO.File]::ReadAllBytes`.
2. Find the END of the existing §10 content (the last CRLF before
   the next `## ` or before the end of file).
3. Encode the new snapshot entries as UTF-8 with explicit CRLF
   line endings (using a `StringBuilder` loop with `Append("`r`n")`
   at the end of each line - NEVER a here-string, which is LF-only).
4. Splice the new bytes INTO the existing byte array at the end of
   §10.
5. Write back via `[IO.File]::WriteAllBytes`.
6. Verify: bytes have CR=LF, loneLF=0, 0xC0/0xC1=0, 0x3F count
   unchanged from baseline, CJK count increased by the CJK chars
   in the new content.

### 1.2 Why byte-level (not string-level)

Per L02 / L37 / L44:

- `[Text.Encoding]::UTF8.GetString(bytes).IndexOf("marker")`
  returns a CHAR offset, not a byte offset. For Chinese text the
  delta is ~2x per CJK char. Using the char offset as a byte
  offset corrupts the file.
- `-replace` operates on the decoded string, not on bytes.
  Round-trip via string layer loses 0x3F / 0xC0 / 0xC1 fidelity.
- here-string `@"..."@` writes LF only on PS 5.1 (L37).

### 1.3 What to NOT do

- Do NOT use `Get-Content` (L01: GBK codepage).
- Do NOT use `Add-Content` for the first line of an entry (it
  appends to the existing file but the existing file may have a
  different encoding; use `WriteAllBytes` for the whole file).
- Do NOT use `git add .` (A10) - stage by explicit path.
- Do NOT use a here-string with mixed `**` and backtick (PS
  parser trips; use `StringBuilder` or `[string]::Format`).

### 1.4 Verification matrix

| Check | Before | After | Method |
|---|---|---|---|
| Bytes | 14456 (PRD), 12322 (TDD) | 14456+N, 12322+M | ReadAllBytes |
| CR=LF? | yes | yes | byte count |
| 0x3F count | 0 | 0 | byte count |
| 0xC0/0xC1 count | 0 | 0 | byte count |
| CJK count | 2370 (PRD), 1599 (TDD) | +N (new entries) | byte scan |
| loneLF | 0 | 0 | adjacent-byte scan |

## 2. Section-specific edits

### 2.1 PRD.md sec 7 R-008 row

Current (line 6950 in HEAD):
```
R-008 | CI 不执行单测（`ci.yml` 缺 test job） | 高 | 高 | **TDD.md §6 提出补 test job 方案** | 全局 |
```
New:
```
R-008 | CI 不执行单测（`ci.yml` 缺 test job） | 高 | 高 | **TDD.md §6 提出补 test job 方案** | 全局 | **CLOSED v0.18.7.0** - ci.yml test job exists and runs scripts\run-tests.bat (13/13 test projects, post-v0.18.23.0 verified 2026-07-04) |
```

Edit method: byte-level replace of the exact line content. The
line is ASCII (no CJK), so the offset arithmetic is trivial.

### 2.2 PRD.md sec 10 + TDD.md sec 10

Append 17 new entries. Format (one entry per release):
```
### YYYY-MM-DD: v0.18.X.Y 已 ship

- **v0.18.X.Y 已 ship**（commit <sha> + tag v0.18.X.Y） -
  installer release/fluxing-0.18.X.Y-installer.exe (~40 MB).
- **spec NNN: <title>** - <one-line description>.
- <one-line sub-bullet per significant change>
- **smoke test / test count / lessons-learned refs** as relevant.

---
```

New entries are mostly English (commit SHAs, paths, hex bytes),
with Chinese punctuation in the section headers. The Chinese is
copied from the existing 2026-07-01 entry for style consistency.

### 2.3 TDD.md sec 8 test gap table

Current:
```
| Gap | 关联 lesson | 状态 |
```

Update each row:
- gap 1 (librime binding acceptance): add "**CLOSED v0.18.10.0** - TestBindingResolution 6/6 PASS"
- gap 2 (WM_SETTINGCHANGE -> panel切色): add "**CLOSED v0.18.22.0 + v0.18.23.0** - TestPanelDarkModeSubscribe + TestDarkModeBroadcast 14/14 + 3/3 PASS"
- gap 3 (user_dict_update(-1) 真删): add "**CLOSED v0.18.17.0** - TestUserDictUpdate 4/4 PASS"
- gap 4 (rime_deployer --debug): reclassify as "**SPEC-NEEDED** - rime_deployer.exe does not exist in librime dist; WeaselServer uses rime_api->deploy directly. Spec 004 SC-005 wording needs update; deferred to a future spec."

Edit method: byte-level replace per row (each row is a single
line in the markdown table; ASCII + Chinese, but no CJK char-
offset delta because we are appending English to the end of an
existing line, not inserting in the middle).

## 3. Constitution check

| Rule | Status | Notes |
|------|--------|-------|
| I. Intent | OK | spec.md sections 0-4 cover goal + out-of-scope + dependencies + success |
| II. Test | OK | NO new tests; existing 13/13 test pass coverage verifies the docs are not lying |
| III. Spec-artifact | OK | spec.md + plan.md + tasks.md in .specify/specs/035-.../ |
| IV. Clarification | OK | No [NEEDS CLARIFICATION] markers |
| V. Incremental | OK | docs-only, no code/test change |
| R1 | OK | spec.md articulates intent + acceptance criteria |
| R2 | OK | spec.md is tech-agnostic; plan.md has the byte-level specifics |
| R3 | OK | All T001-T004 marked P1 |
| R4 | OK | This file is the constitution check |
| R5 | OK | Each task <4h, 1-2 files |
| R6 | OK | T005 is the evidence requirement (byte-level verify per L40/L44) |
| R7 | OK | spec/plan/tasks consistent (cross-checked during authoring); one source of truth: only INSERTIONS, no MODIFICATIONS of existing lines |
| R8 | OK | spec versioned in git via the T006 commit |
| R9 | OK | L01/L02/L40/L44 cited; ci.yml inspected |
| P1-P8 | OK | P8 (fluxing: scope) - docs-only commit, kizemo only |

## 4. Risks

- **R1**: miscalculating the byte offset for the splice point. If
  the offset is wrong by N CJK chars, N*3 bytes get misaligned and
  the file is corrupted (L40-style damage). Mitigation: byte-level
  search for the exact section header, NOT `[Encoding]::UTF8.GetString().IndexOf()` (which returns char offset, L44). Verify by
  reading the post-splice file and confirming the section headers
  are at known positions.
- **R2**: encoding drift in the new content. The new entries mix
  English (commit SHAs, paths) and Chinese (section headers). The
  StringBuilder pattern with explicit `"`r`n"` preserves CRLF. The
  UTF-8 encoding of the new bytes is automatic via the
  `[Text.UTF8Encoding]::new($false)` constructor (no BOM).
- **R3**: forgetting to also update TDD.md (mirror operation).
  Mitigation: T003 is split into T003a (PRD) and T003b (TDD) so
  the task list makes the parallel obvious.
- **R4**: accidentally modifying existing lines (R7 violation).
  Mitigation: byte-level operations ONLY insert at the end of
  sections; existing lines are never touched. Verify with
  `git diff --numstat` showing only insertions, no deletions.
- **R5**: R-008 CLOSED marker is misleading (R-008 in PRD is
  "CI does not run tests"; we are saying it does). Mitigation: the
  CLOSED marker includes the version (v0.18.7.0) and the evidence
  (13/13 tests verified). The user can audit `ci.yml` line 194 to
  confirm the test job exists.

## 5. Anti-patterns to avoid

- AP-035-A: do NOT use `Encoding.UTF8.GetString(bytes).IndexOf()`
  (L44 char-offset trap).
- AP-035-B: do NOT use here-string `@"..."@` for the new content
  (L37 LF-only trap).
- AP-035-C: do NOT use `Get-Content` to read the existing PRD/TDD
  (L01 GBK pollution).
- AP-035-D: do NOT use `Add-Content` to append (round-trip via
  string layer may corrupt CRLF/BOM).
- AP-035-E: do NOT use `git add .` to commit. Stage by path (A10).
- AP-035-F: do NOT rewrite historical entries. Append only (R7).
- AP-035-G: do NOT update TDD sec 8 row 4 (rime_deployer --debug)
  by marking it CLOSED. Reclassify as SPEC-NEEDED - the test as
  written is not implementable.
- AP-035-H: do NOT skip the byte-level verification (R6 evidence-
  before-assertion).
