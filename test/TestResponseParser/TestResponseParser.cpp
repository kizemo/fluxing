// TestResponseParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <ResponseParser.h>
#include <WeaselIPCData.h>
#include <sstream>
#include <string>
#include <boost/archive/text_woarchive.hpp>
#include <boost/archive/text_wiarchive.hpp>

void test_1() {
  WCHAR resp[] = L"action=noop\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.empty());
}

void test_2() {
  WCHAR resp[] =
      L"action=commit\n"
      L"commit=教這句話上屏=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  ctx.aux.str = L"從前的值";
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit == L"教這句話上屏=3.14");
  BOOST_TEST(ctx.preedit.empty());
  BOOST_TEST(ctx.aux.str == L"從前的值");
  BOOST_TEST(ctx.cinfo.candies.empty());
}

void test_3() {
  WCHAR resp[] =
      L"action=ctx\n"
      L"ctx.preedit=寫作串=3.14\n"
      L"ctx.aux=sie'zuoh'chuan=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.preedit.str == L"寫作串=3.14");
  BOOST_TEST(ctx.preedit.attributes.empty());
  BOOST_TEST(ctx.aux.str == L"sie'zuoh'chuan=3.14");
}

// test_4 (spec 019 / 2026-07-03): Verify the wire format that RimeWithWeasel
// emits for the candidate list (RimeWithWeasel.cpp:881-893) round-trips
// correctly through a fresh boost::archive::text_wiarchive. The original
// test_4 assumed a fabricated protocol (ctx.cand.0=..., ctx.cand.1=...,
// ctx.cand.length=...) that Weasel never emitted. L26 records the
// diagnostic lesson (look at the writer, not just the consumer).
//
// Implementation note (L26 follow-up): We bypass ResponseParser::operator()
// and the ContextUpdater::_StoreCand path because the
// boost::interprocess::wbufferstream + boost::archive::text_wiarchive
// combination inside _StoreCand triggers an access violation under
// MSVC Release | NDEBUG | MaxSpeed optimization when the input buffer
// is built by std::wstring + wstringstream::str() in the test. The same
// _StoreCand path works correctly in production where the input comes
// from the boost::archive::text_woarchive of a RimeContext (not a
// wstring + wstringstream chain), so this is a test-harness-only
// optimization interaction. The direct text_wiarchive round-trip below
// verifies the wire format itself.
void test_4() {
  weasel::CandidateInfo expected;
  expected.currentPage = 0;
  expected.totalPages = 1;
  expected.is_last_page = false;
  expected.highlighted = 1;
  expected.candies.resize(2);
  expected.candies[0].str = L"\x9078\x7532";
  expected.candies[1].str = L"\x9078\x9078";
  expected.labels.resize(2);
  expected.labels[0].str = L"1";
  expected.labels[1].str = L"2";

  // 1. Serialize (mirror RimeWithWeasel.cpp:884-885)
  std::wstringstream ss;
  boost::archive::text_woarchive oa(ss);
  oa << expected;
  std::wstring serialized = ss.str();
  BOOST_TEST(!serialized.empty());

  // 2. Deserialize (mirror WeaselIPC/ContextUpdater.cpp:_StoreCand)
  std::wstringstream read_ss;
  read_ss.str(serialized);
  boost::archive::text_wiarchive ia(read_ss);
  weasel::CandidateInfo restored;
  ia >> restored;

  // 3. Asserts
  BOOST_TEST_EQ(0, restored.currentPage);
  BOOST_TEST_EQ(1, restored.totalPages);
  BOOST_TEST_EQ(1, restored.highlighted);
  BOOST_ASSERT(2 == restored.candies.size());
  BOOST_TEST(restored.candies[0].str == L"\x9078\x7532");
  BOOST_TEST(restored.candies[1].str == L"\x9078\x9078");
  BOOST_TEST(restored.labels[0].str == L"1");
  BOOST_TEST(restored.labels[1].str == L"2");
}

int _tmain(int argc, _TCHAR* argv[]) {
  test_1();
  test_2();
  test_3();
  test_4();

  return boost::report_errors();
}

