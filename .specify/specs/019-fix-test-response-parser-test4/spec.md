# 019 - Fix TestResponseParser test_4 (close the WeaselIPC ctx.cand protocol mismatch)

> Scope: make TestResponseParser.exe report 4/4 PASS. The pre-existing test_4 failure
> was misdiagnosed in spec 015 (filed as "WeaselIPC missing ctx.cand.0/1 array-style
> deserialization") but the real production protocol is a single boost-serialized
> CandidateInfo on the ctx.cand= line (RimeWithWeasel.cpp:891). This spec rewrites
> test_4 to reflect the actual protocol, with no production-code change.

## 0. Background

- TDD.md sec 8 known gap: TestResponseParser test_4 fails with
  BOOST_ASSERT(2 == c.candies.size()).
- Spec 015 L22 speculated the cause was a missing array-style deserializer
  in WeaselIPC ContextUpdater, but did not verify against the actual
  production code that GENERATES the wire format (RimeWithWeasel.cpp:891).
- This spec is the diagnosis-and-fix follow-up. It does NOT add new
  test cases; it makes the existing test_4 reflect the real protocol.

## 1. Wire format ground truth (verified 2026-07-03)

Read RimeWithWeasel/RimeWithWeasel.cpp:881-893 (the candidate serialization path):

```cpp
if (has_candidates) {
  std::wstringstream ss;
  boost::archive::text_woarchive oa(ss);
  oa << cinfo;                                          // serialize whole CandidateInfo
  auto s = ss.str();
  body.append(L"ctx.cand=").append(std::move(s)).append(L"\n");
}
```

Correspondingly, WeaselIPC/ContextUpdater.cpp:_StoreCand consumes the value
by piping the boost text_wiarchive into cinfo as a whole struct. There is
NO ctx.cand.0, ctx.cand.1, ctx.cand.cursor, or ctx.cand.length
key format in production.

The test_4 input

  ctx.cand.length=2
  ctx.cand.0=候選甲
  ctx.cand.1=候選乙
  ctx.cand.cursor=1
  ctx.cand.page=0/1

is a fabricated protocol that never existed in Weasel. The keys are silently
ignored by the ContextUpdater.

## 2. Goal

Make TestResponseParser.exe report 4/4 PASS in <1s with the existing
scripts/run-tests.bat build + run loop. No change to production code.

## 3. Acceptance (GWT)

- Given a clean checkout of Fluxing at v0.18.13.0+
- When the developer runs scripts/run-tests.bat
- Then within 60s, all 5 test exes build and run
- And TestResponseParser prints "4 / 4 assertions passed" and exits 0
- And TestBindingResolution still 6/6, TestDefaultHotkeys 35/35,
  TestShiftSelectBinding 13/13, TestWeaselIPC still PASS
- And scripts/run-tests.bat exits 0 (=== ALL TESTS PASSED ===)

- Same command in GitHub Actions windows-2022 image: same 5 test exes
  built and run, same 4/4 PASS for TestResponseParser, same overall exit 0

## 4. Non-goals

- Changing WeaselIPC / RimeWithWeasel production code (out of scope)
- Implementing ctx.cand.0/1 array-style deserialization (the
  boost-serialized wire format is the contract, period)
- Adding new test cases beyond test_4 rewrite
- Refactoring run-tests.bat to run tests in parallel

## 5. Out of scope / not affected

- The 4 integration tests in TDD.md sec 3.1 (TestUserDictUpdate /
  TestPhrasesRoundTrip / TestDarkModeBroadcast / TestYamlRoundTripE2E)
  are split into spec 020-023 placeholders. Production code for
  those (CandidateEdit / PhrasesStore / DarkModeBridge / YamlRoundTrip)
  does not exist yet (spec 007/008/009 are spec-stage only).
- L19 (shift+Shift_L collision) is unrelated to this spec.
- L22 (system(pause)) was already fixed in spec 015.

## 6. Done

- test/TestResponseParser/TestResponseParser.cpp test_4 rewritten
  to use the actual boost-serialized ctx.cand=... wire format
- TestResponseParser.exe reports 4/4 PASS
- run-tests.bat exit code = 0
- v0.18.13.0 installer ships
- L26 lesson recorded: test assumptions must match the code that
  GENERATES the wire format, not the code that consumes it