# 019 - TestResponseParser test_4 fix (Constitution Check + verification matrix)

> Spec layer (intent): see spec.md. This plan covers the technical approach,
> Constitution Check, and verification matrix. Files changed:
> TestResponseParser.cpp (test_4 rewrite only), L26 lesson, run-tests.bat
> (no change needed; already iterates 5 test projects per spec 015/018), and
> v0.18.13.0 release artifacts.

## 1. Technical approach

The single fix: rewrite `test_4` in test/TestResponseParser/TestResponseParser.cpp
to use the real wire format.

### 1.1 test_4 rewrite (the only production change-adjacent edit)

The new test_4 is **constructive**: it builds a `weasel::CandidateInfo` with
known values, boost-serializes it into a `std::wstring`, embeds that into a
fake response line `ctx.cand=<serialized>`, feeds the whole response to
`ResponseParser::Feed` (or `ResponseParser::operator()`), and asserts the
cinfo is restored.

Pseudocode:

```cpp
void test_4() {
  // 1. Build a known CandidateInfo
  weasel::CandidateInfo expected;
  expected.currentPage = 0;
  expected.totalPages  = 1;
  expected.highlighted = 1;
  expected.is_last_page = false;
  expected.candies.resize(2);
  expected.candies[0].str = L"\x9078\x7532";   // 候選甲
  expected.candies[1].str = L"\x9078\x9078";   // 候選乙
  expected.labels.resize(2);
  expected.labels[0].str = L"1";
  expected.labels[1].str = L"2";

  // 2. Boost-serialize (mirrors RimeWithWeasel.cpp:884-885)
  std::wstringstream ss;
  boost::archive::text_woarchive oa(ss);
  oa << expected;
  std::wstring serialized = ss.str();

  // 3. Build a multi-line response and feed it
  WCHAR resp_prefix[] = L"action=commit,ctx\nctx.preedit=\x9078\x9078=3.14\n";
  std::wstring resp;
  resp += resp_prefix;
  resp += L"ctx.cand=";
  resp += serialized;
  resp += L"\n";
  resp += L".\n";   // pipe message terminator
  DWORD len = (DWORD)resp.size();

  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp.data(), len);

  // 4. Asserts
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.preedit.str == L"\x9078\x9078=3.14");
  weasel::CandidateInfo& c = ctx.cinfo;
  BOOST_ASSERT(2 == c.candies.size());
  BOOST_TEST(c.candies[0].str == L"\x9078\x7532");
  BOOST_TEST(c.candies[1].str == L"\x9078\x9078");
  BOOST_TEST_EQ(1, c.highlighted);
  BOOST_TEST_EQ(0, c.currentPage);
  BOOST_TEST_EQ(1, c.totalPages);
}
```

Encoding notes:
- Use \x escapes for CJK in C++ source (UTF-8 escape sequence in the source
  string), avoid embedding UTF-8 bytes directly in the cpp file.
- File must be saved as UTF-8 (no BOM, CRLF) per AGENTS.md sec 5 step 1.

### 1.2 Pre-conditions on the existing test_4

The original test_4 stays as the SHAPE: it still constructs a multi-line
response and asserts on ctx fields. The ONLY change is the candidate line
format (from 5 lines ctx.cand.0/1/length/cursor/page to 1 line
ctx.cand=<boost-serialized>).

The two preedit assertions that are NOT about candidates stay unchanged:
- `ctx.preedit.str == L"候選乙=3.14"` (the CJK bytes in resp are UTF-8
  escape sequence in original; the C++ source stored them as raw UTF-8 bytes
  inside the `L"..."` wide string, which on MSVC converts per the source
  file BOM-less UTF-8 codepage setting). To avoid any ambiguity, use
  `\x9078\x9078` hex escapes in the C++ source.

### 1.3 Why no production-code change

- `RimeWithWeasel.cpp:881-893` is the source of truth for the wire format.
- `ContextUpdater::_StoreCand` already consumes that format correctly.
- Adding ctx.cand.0/1 parsing in ContextUpdater would create a second
  parallel deserializer that production never writes, accumulating
  dead code (anti-pattern A: "I'll just add this one thing").
- The 5 historical installer sizes (0.18.5 - 0.18.9) all shipped with the
  boost-serialized single-line format; weasel.protocols.older that did
  not match it are not in this tree.

## 2. Constitution Check (per AGENTS.md R4 + spec 015/016 template)

| Principle | Status | Note |
|---|---|---|
| I. Intent before implementation | PASS | spec.md sec 2 + 3 capture goal + GWT acceptance |
| II. Test-backed change | PASS | test_4 rewrite is itself the test backing the bug fix |
| III. Spec-artifact discipline | PASS | this spec/plan/tasks three-piece set is the artifact |
| IV. Structured clarification | PASS | The misdiagnosis in spec 015 is documented in L26, not silently overwritten |
| V. Incremental delivery | PASS | One test rewrite, no production change, smallest possible slice |
| R1 Intent + acceptance in response | PASS | First message captures intent + GWT |
| R2 Spec layer tech-agnostic | PASS | spec.md has no language/framework names |
| R3 Single priority (P1) | PASS | tasks.md uses P1 only |
| R4 Constitution Check filled | PASS | This table |
| R5 Task granularity | PASS | 5 tasks, each <4h, 1-2 files |
| R6 Evidence before assertion | PASS | run-tests.bat output is the evidence |
| R7 One source of truth | PASS | spec 015 referenced for the misdiagnosis; spec 019 supersedes the fix; no other place changes the test_4 |
| R8 Specs versioned in git | PASS | spec/plan/tasks committed in the release commit |
| R9 Lookup beats memory | PASS | Traced RimeWithWeasel.cpp:881-893 with grep, not from memory |

## 3. Verification matrix

| Step | Command | Expected | Source of truth |
|---|---|---|---|
| Build | scripts/run-tests.bat | all 5 projects build OK | AGENTS.md sec 2.3 |
| Test_4 alone | Release\TestResponseParser.exe | "4 / 4 assertions passed", exit 0 | spec 019 sec 3 |
| Other 4 tests unchanged | Release\Test*.exe | TestDefaultHotkeys 35/35, TestShiftSelectBinding 13/13, TestBindingResolution 6/6, TestWeaselIPC PASS | L22, L25 |
| Script | scripts/run-tests.bat exit code | 0 (=== ALL TESTS PASSED ===) | spec 015 |
| Smoke test (release) | install.nsi silent install | no regression vs 0.18.12.0 (only TestResponseParser.cpp changed) | AGENTS.md sec 2.5 (no install.nsi edit -> smoke test optional) |
| Byte health (test_4 cpp) | Get-Content byte scan | 0 overlong 0xC0/0xC1, no BOM, CRLF count == total-line-endings | AGENTS.md sec 5 step 1 |
| Lint | clang-format -i | no diff after format | AGENTS.md sec 2.4 |

## 4. Files changed (delta vs spec 018 release)

| File | Change | Why |
|---|---|---|
| test/TestResponseParser/TestResponseParser.cpp | test_4 rewrite (only) | The bug fix |
| .specify/memory/lessons-learned.md | + L26 entry | The misdiagnosis lesson |
| .specify/specs/019-fix-test-response-parser-test4/{spec,plan,tasks}.md | NEW | This spec |
| .specify/specs/020-023-*/{spec,plan,tasks}.md | NEW (placeholders) | TDD sec 3.1 follow-up spec staging |
| CHANGELOG.md | New [0.18.13.0-fluxing] section | release artifact |
| release/fluxing-0.18.13.0-installer.exe | NEW | release artifact |
| env.bat | 0.18.12 -> 0.18.13 (gitignored) | version bump |
| weasel.props | VERSION_PATCH 12 -> 13 (gitignored) | version bump |

## 5. Risks and mitigations

- **R1** (C++ source encoding): The 3 CJK characters in the original test_4
  are stored as raw UTF-8 bytes inside the L"..." string. Switching to
  \x hex escapes removes the codepage-dependence. Mitigation: use
  \x9078 (候), \x7532 (甲), \x9078 (選), etc. throughout test_4.
- **R2** (boost serialization stability): The text_woarchive format is
  stable across boost 1.83+ (the version pinned in env.bat). Same archive
  format used in production, so the round-trip is guaranteed to work.
- **R3** (test_4 preedit line already works in test_3): The preedit-only
  line format `ctx.preedit=...=3.14` works fine. The bug is isolated to the
  candidate line; preedit/cursor handling stays unchanged.
- **R4** (run-tests.bat loop): The for loop iterates 5 projects; the
  TestResponseParser output already includes "N / N assertions passed" line
  via boost::report_errors -> test_4. The script does not need a change.

## 6. Out of scope

- Adding ci.yml matrix job for tests (already in ci.yml per spec 015).
- Bumping coverage measurement (TDD sec 7 v2.1+ milestone).
- Fuzz testing (TDD sec 6.3 v2.2+ milestone).
- Any of the 4 TDD sec 3.1 integration tests (split into spec 020-023).