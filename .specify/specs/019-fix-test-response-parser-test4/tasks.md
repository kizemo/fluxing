# 019 - TestResponseParser test_4 fix (implementation checklist)

> Tasks T001..T005. Each <4h, 1-2 files. P1 priority per spec 019 sec 4 + AGENTS.md R3.

## T001 - P1 - Rewrite test_4 in TestResponseParser.cpp
- **Files**: test/TestResponseParser/TestResponseParser.cpp (only)
- **Change**:
  - Replace the 5-line ctx.cand.0/1/length/cursor/page input with
    a single ctx.cand=<boost text_woarchive serialized CandidateInfo> line.
  - Use \x hex escapes for CJK in L"..." strings (no raw UTF-8 bytes).
  - Build a known CandidateInfo (2 candidates "候選甲" / "候選乙",
    highlighted=1, currentPage=0, totalPages=1) inside the test, serialize
    it via boost::archive::text_woarchive, embed in the response, feed via
    ResponseParser::operator() or ResponseParser::Feed.
  - Keep the preedit line and cursor attribute assertions unchanged
    (they currently work in test_3 and we want the preedit code path
    exercised alongside the candidate code path).
- **Acceptance**: 0/1 BOOST_ASSERTs in the new test_4; all other
  assertions on the cinfo restore pass.
- **Bytes**: 5210 -> ~5700 bytes expected (modest grow; <1000 bytes delta).
  CRLF count == line count; 0 lone LF/CR. No BOM (per L11, .cpp files
  do not want BOM in this repo). No 0xC0/0xC1 overlong.

## T002 - P1 - Add L26 lesson
- **Files**: .specify/memory/lessons-learned.md
- **Add** (in the same shape as L25):
  ```
  ## L26 - Test assumptions must match the code that GENERATES the wire format, not the code that consumes it
  ```
- **Content**:
  - Incident: TestResponseParser test_4 has been failing silently for
    years. spec 015 L22 attributed it to a "missing ctx.cand.0/1
    array-style deserializer in WeaselIPC ContextUpdater" without
    reading RimeWithWeasel.cpp:881-893 (the actual code that writes
    the wire format).
  - Root cause: The test_4 input assumed a fabricated protocol
    (ctx.cand.0=候選甲\nctx.cand.1=候選乙\n...) that never existed in
    Weasel. Production has always used a single boost-serialized
    CandidateInfo on ctx.cand=. The keys in the test were silently
    ignored by ContextUpdater::_StoreCand.
  - Lesson: When a test fails on what looks like a missing code path,
    trace the code that GENERATES the wire format first, not the code
    that consumes it. Spec 015 had access to RimeWithWeasel.cpp and
    missed it; the right diagnostic question is "what bytes does
    the writer produce, and what do those bytes look like when read
    by the consumer?"
  - 3 anti-patterns:
    - AP-L26-A: Diagnose from the consumer side. The consumer may be
      correctly rejecting an input that the writer never sends.
    - AP-L26-B: Trust the user's description of the test failure.
      spec 015 saw "BOOST_ASSERT(2 == c.candies.size())" and inferred
      "deserializer is incomplete" without checking what the writer
      emits.
    - AP-L26-C: Skip reading the writer because "we already know the
      protocol". The test's input format was assumed correct.
  - 2 sentence prevention: when reviewing a test failure, read both
    the writer and the consumer before forming a hypothesis. If the
    writer emits X and the consumer reads X, the test must exercise X.
- **Acceptance**: L26 entry in lessons-learned.md, single ## heading,
  no overlong UTF-8, follows L25's structure.
- **Bytes**: lessons-learned.md currently ~50KB; +1.5KB expected for L26.

## T003 - P1 - Create spec 019 spec/plan/tasks (3-piece)
- **Files**:
  - .specify/specs/019-fix-test-response-parser-test4/spec.md (this spec)
  - .specify/specs/019-fix-test-response-parser-test4/plan.md (Constitution Check + verification matrix)
  - .specify/specs/019-fix-test-response-parser-test4/tasks.md (this file)
- **Acceptance**: All 3 files present, BOM-less, CRLF, R4 Constitution Check
  filled, R5 task granularity 1-2 files each.
- **Bytes**: spec ~3.7KB, plan ~8.2KB, tasks ~2KB (this file).

## T004 - P1 - Create spec 020-023 placeholder 3-piece sets
- **Files**:
  - .specify/specs/020-integration-test-user-dict-update/{spec,plan,tasks}.md
  - .specify/specs/021-integration-test-phrases-roundtrip/{spec,plan,tasks}.md
  - .specify/specs/022-integration-test-dark-mode-broadcast/{spec,plan,tasks}.md
  - .specify/specs/023-integration-test-yaml-roundtrip/{spec,plan,tasks}.md
- **Content per spec**: 1 short note + 1 "out of scope until v2.x" section.
  Each one references TDD sec 3.1 and points to the parent spec (008/009/etc).
- **Acceptance**: 12 files created. Each spec.md <2KB, each plan.md <1KB
  ("blocked on parent spec" only), each tasks.md <500B ("n/a until
  parent spec ships"). No code in this spec.
- **Bytes**: ~5KB total for the 12 placeholder files.

## T005 - P1 - Build, test, commit, tag, push v0.18.13.0
- **Steps** (in order):
  1. `git status` clean except for: TestResponseParser.cpp,
     lessons-learned.md, 5 spec dirs (019 + 020-023),
     CHANGELOG.md, release/0.18.13.0 installer.
  2. env.bat: 0.18.12 -> 0.18.13 (gitignored).
  3. weasel.props: VERSION_PATCH 12 -> 13 (gitignored).
  4. CHANGELOG.md: add [0.18.13.0-fluxing] section.
  5. `xbuild.bat weasel installer` -> output\archives\fluxing-0.18.13.0-installer.exe.
  6. Copy installer to release\fluxing-0.18.13.0-installer.exe.
  7. `scripts\run-tests.bat` -> "=== ALL TESTS PASSED ===" with 4/4 for
     TestResponseParser, 35/35 for TestDefaultHotkeys, 13/13 for
     TestShiftSelectBinding, 6/6 for TestBindingResolution, PASS for
     TestWeaselIPC. exit code 0.
  8. Stage explicitly: TestResponseParser.cpp, lessons-learned.md,
     CHANGELOG.md, release/fluxing-0.18.13.0-installer.exe,
     .specify/specs/019-*/{spec,plan,tasks}.md x3,
     .specify/specs/020-*/{spec,plan,tasks}.md x3,
     .specify/specs/021-*/{spec,plan,tasks}.md x3,
     .specify/specs/022-*/{spec,plan,tasks}.md x3,
     .specify/specs/023-*/{spec,plan,tasks}.md x3.
  9. `git commit` with the spec 019 commit message.
  10. `git tag -a v0.18.13.0 -m "..."`.
  11. `git push kizemo Fluxing` and `git push kizemo v0.18.13.0`.
- **Acceptance**: tag v0.18.13.0 visible in `git ls-remote kizemo`.

## Done (cross-reference)
- AGENTS.md sec 5 pre-commit checklist (5 steps) all pass
- L26 + spec 019 in git history
- 4 placeholder specs (020-023) tracked but not implemented