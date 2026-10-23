# 079 verification record

> Captured 2026-10-23, revision `HEAD` + this spec working tree.
> Reproduce with the commands quoted under each block.

## 1. byte-level damage markers

```
python scripts/test-infra/scan-encoding-damage.py --path <target>
```

| target | U+FFFD | NUL | C0 ctrl | scanner |
|---|---|---|---|---|
| `.specify/PRD.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/TDD.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/memory/lessons-learned.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/001-user-visible-strings/tasks.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/014-.../plan.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/014-.../spec.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/014-.../tasks.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/015-fix-test-infrastructure/tasks.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/016-behavior-level-test-framework/spec.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/037-fluxing-components-v0/plan.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/037-fluxing-components-v0/spec.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `.specify/specs/037-fluxing-components-v0/tasks.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `CHANGELOG.md` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |
| `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` | 0 | 0 | 0 | damaged_lines=0 fffd=0 nul=0 c0=0 tab=0 exit=0 |

all targets clean: **YES** (14 files, 87 repaired lines)

## 2. the TAB class detector, both directions

```
python scripts/test-infra/verify-tab-detector.py
```

The gate checks both directions in one run. Forward: the repaired tree must
report nothing. Reverse: the damaged revisions of the five affected files
are copied into a throw-away git repository and scanned there, where they
must report exactly the lines that were repaired -- no more, no less.

```
1. forward: no TAB class damage in the current tree
  PASS: 261 tracked markdown files report nothing
2. reverse: the damaged revisions report exactly their damage
  PASS: .specify/memory/lessons-learned.md reports tab=5 damaged=5
  PASS: .specify/specs/014-.../plan.md reports tab=5 damaged=11
  PASS: .specify/specs/015-.../tasks.md reports tab=1 damaged=1
  PASS: .specify/specs/037-.../tasks.md reports tab=4 damaged=4
  PASS: CHANGELOG.md reports tab=4 damaged=4

ALL CHECKS PASSED
```

The 19 TAB class lines are 5 + 5 + 1 + 4 + 4; four of the five in
`014/.../plan.md` were already repaired by the first map, one (line 15, a
*leading* TAB) was missed by it and is repaired here.

## 3. toolchain regression test

```
python scripts/test-infra/test_encoding_toolchain.py
```

11 groups, 30 assertions, ending in `ALL TESTS PASSED`:

```
6. the TAB class detector finds an un-escaped backslash-t
  PASS: inline TAB damage is detected
  PASS: leading TAB damage is detected
  PASS: a repaired line is clean
7. the TAB class detector does not fire on legitimate TABs
  PASS: a TSV data block is exempt
  PASS: an inline code span is exempt
  PASS: a fenced code block is exempt
  PASS: indentation-only TAB is exempt
  PASS: the - OLD: transcript marker is exempt
8. the applier repairs a TAB class line and keeps the line ending
  PASS: applier accepted the TAB repair in a LF file
  PASS: the LF file kept its own line ending
  PASS: applier accepted the TAB repair in a CRLF file
  PASS: the CRLF file kept its own line ending
9. the applier refuses a replacement that still carries damage
  PASS: applier refused the U+FFFD replacement
10. the verifier baselines on HEAD while the repair is uncommitted
  PASS: the baseline is HEAD for an uncommitted repair
  PASS: the verifier passes on the repaired file
11. the verifier finds the scanner even when it checks another tree
  PASS: the scanner was found in the tool repository

ALL TESTS PASSED
```

## 4. the acceptance gate of every repaired file

```
python scripts/test-infra/verify-encoding-repair.py --target <file> --map <map>
```

| target | before rev | rewritten (baseline side) | verdict |
|---|---|---|---|
| `CHANGELOG.md` | HEAD | 4 | ALL CHECKS PASSED |
| `.specify/memory/lessons-learned.md` | HEAD | 5 | ALL CHECKS PASSED |
| `.specify/specs/015-fix-test-infrastructure/tasks.md` | HEAD | 1 | ALL CHECKS PASSED |
| `.specify/specs/037-fluxing-components-v0/tasks.md` | HEAD | 4 | ALL CHECKS PASSED |
| `.specify/specs/014-.../plan.md` | HEAD | 11 | ALL CHECKS PASSED |
| `.specify/PRD.md` | HEAD | 5 | ALL CHECKS PASSED |
| `.specify/TDD.md` | HEAD | 3 | ALL CHECKS PASSED |
| `.specify/specs/001-user-visible-strings/tasks.md` | HEAD | 9 | ALL CHECKS PASSED |
| `.specify/specs/014-.../spec.md` | HEAD | 18 | ALL CHECKS PASSED |
| `.specify/specs/014-.../tasks.md` | HEAD | 15 | ALL CHECKS PASSED |
| `.specify/specs/037-fluxing-components-v0/plan.md` | HEAD | 3 | ALL CHECKS PASSED |
| `.specify/specs/037-fluxing-components-v0/spec.md` | HEAD | 3 | ALL CHECKS PASSED |
| `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` | HEAD | 1 | ALL CHECKS PASSED |

**13 / 13 passed.** The `CHANGELOG.md` entry is the one that used to fail: the
0.21.0.2 section had been added after the spec 078 repair, and the gate
compared lines by position against the parent of the newest commit touching
the file, so the 51 added lines shifted every following line number and
5497 lines were reported as undeclared. The gate now takes HEAD as the
baseline while the repair is uncommitted and aligns the two files with
`difflib`, so the added section no longer moves any line number.

## 5. the spec 078 gate is not broken

The spec 078 acceptance command cannot be replayed in this tree: it verifies
"the tree as it was", and spec 079 repaired 15 more lines in the same files.
What has to hold is that the *capability* is intact, so the spec 078 tree
(the commit 489863cd8) is rebuilt in a throw-away repository -- the pre-repair
state as the first commit, the repaired state as the second -- and the gate
is asked to compare them with the spec 078 map:

```
--- .specify/memory/lessons-learned.md
   rewritten  : 180 baseline line(s)
   scanner    : damage-scan: ... damaged_lines=5 fffd=0 nul=0 c0=0 tab=5 exit=1
   PROBLEM: scanner exit code 1
--- CHANGELOG.md
   rewritten  : 5 baseline line(s)
   scanner    : damage-scan: ... damaged_lines=4 fffd=0 nul=0 c0=0 tab=4 exit=1
   PROBLEM: scanner exit code 1
```

The map match is exact: 180 and 5 baseline lines, the same numbers the spec
078 maps declare, with no undeclared and no missing line. The only remaining
PROBLEM is the scanner reporting the 5 + 4 TAB class lines that spec 078
left behind -- that is the scanner telling the truth about a tree that
really does carry damage, and those lines are exactly what spec 079
repaired.

## 6. what is still open

- The scanner still reports GBK round-trip hits on `output/data/**` (the
  dictionary data). Those are false positives on correct Chinese and the
  directory is out of scope for this spec; not one line there was touched.
- The 64 bit Shift release-to-select-candidate behaviour still has no
  hands-on check in a 64 bit application. The evidence so far is the PDB
  symbols plus the static test suite.
