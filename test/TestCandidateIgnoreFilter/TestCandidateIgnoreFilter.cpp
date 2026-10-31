// TestCandidateIgnoreFilter.cpp : spec 031 (2026-07-04)
//
// 5 behavior-level assertions for the candidate ignore filter
// (stage 3 of spec 008). Pattern: boost::detail::lightweight_test.
// We do NOT link WeaselPanel.cpp (WTL/ATL/Gdiplus); we mirror the
// ignore list / filter logic in test-local free functions that
// follow the same byte-level / BOM-aware / CRLF-tolerant contract
// as the production code in WeaselPanel.cpp.
//
// The 5 assertions match spec 031 spec.md section 1:
//   1. LoadIgnoreList reads UTF-8 BOM CRLF file (most common case).
//   2. LoadIgnoreList skips blank / whitespace-only lines.
//   3. LoadIgnoreList reads UTF-16 LE BOM file (rlegacy rime data).
//   4. _FilterIgnoredCandidates removes all candidates in the ignore set.
//   5. _FilterIgnoredCandidates leaves non-ignored candidates alone.
//
// spec 092 (缺陷 A · 无判别力的 PASS 标签):
// 原实现在每组断言后**无条件**打印 "PASS: Tn ..."。同一组 BOOST_TEST
// 断言即使失败,日志里照样出现 PASS —— 标签不携带任何判别信息,读日志的
// 人会被误导(拦住回归的只有 boost::report_errors() 的退出码)。
// 现在 PASS 标签挂到本组断言的**真实结果**上:
//   boost::detail::test_errors() 是 boost lightweight_test 的全局失败
//   计数器(BOOST_TEST* 失败时 +1;boost/detail/lightweight_test.hpp 是
//   boost/core/lightweight_test.hpp 的转发头,后者才有这个符号)。
//   进入一组断言前取一次快照,打印标签时再取一次,两者相等 => 本组全过。
// 退出码仍由 boost::report_errors() 统一决定(有失败 => 非 0)。

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <iostream>
#include <cstdio>

namespace {

// Mirror WeaselPanel::LoadIgnoreList: byte-level + BOM 判别 + CRLF/LF/CR split.
std::unordered_set<std::wstring> ReadIgnoreListFromFile(
    const std::string& path) {
  std::unordered_set<std::wstring> result;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return result;
  std::vector<unsigned char> raw((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());
  if (raw.empty())
    return result;
  std::wstring content;
  if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
    // UTF-8 BOM: 简化为 ASCII 测试用例（test 不调 u8tow，模拟 wide 内容）
    std::string utf8((char*)raw.data() + 3, raw.size() - 3);
    content = std::wstring(utf8.begin(), utf8.end());
  } else if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
    // UTF-16 LE BOM
    content = std::wstring((wchar_t*)(raw.data() + 2),
                           (raw.size() - 2) / sizeof(wchar_t));
  } else {
    // 无 BOM
    std::string utf8((char*)raw.data(), raw.size());
    content = std::wstring(utf8.begin(), utf8.end());
  }
  // 按行 split (CRLF / LF / CR 都支持)
  size_t pos = 0;
  while (pos < content.size()) {
    size_t eol = content.find_first_of(L"\r\n", pos);
    if (eol == std::wstring::npos)
      eol = content.size();
    std::wstring line = content.substr(pos, eol - pos);
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\t'))
      line.pop_back();
    if (!line.empty())
      result.insert(line);
    pos = eol;
    if (pos < content.size() && content[pos] == L'\r')
      pos++;
    if (pos < content.size() && content[pos] == L'\n')
      pos++;
  }
  return result;
}

// Mirror WeaselPanel::_FilterIgnoredCandidates.
void FilterIgnoredCandidates(std::vector<std::wstring>& candidates,
                             const std::unordered_set<std::wstring>& ignore) {
  if (ignore.empty())
    return;
  for (size_t i = candidates.size(); i > 0; --i) {
    if (ignore.count(candidates[i - 1]) > 0) {
      candidates.erase(candidates.begin() + (i - 1));
    }
  }
}

// ===== spec 092: 让 PASS 标签有判别力 ==============================
// boost::detail::test_errors() —— boost lightweight_test 的全局失败计数器。
// BOOST_TEST* 失败时 +1、成功时不变。于是「本组断言前后的快照相等」等价
// 于「本组断言全过」,标签就不再是空口承诺。
class AssertionScope {
 public:
  AssertionScope() : errors_before_(boost::detail::test_errors()) {}
  bool Passed() const {
    return boost::detail::test_errors() == errors_before_;
  }

 private:
  int errors_before_;
};

int g_cases_total = 0;
int g_cases_failed = 0;

void ReportCase(const char* label, bool passed) {
  ++g_cases_total;
  if (!passed)
    ++g_cases_failed;
  std::cout << (passed ? "  PASS: " : "  FAIL: ") << label << std::endl;
}

}  // namespace

// spec 090: 服务端「可见页内索引 -> 引擎索引」映射的行为级断言。
int RunServerSideIgnoreMappingTests();

int main() {
  // T1: LoadIgnoreList reads UTF-8 BOM CRLF file.
  {
    AssertionScope scope;
    const char* path = "test_ignore_utf8.txt";
    {
      std::ofstream out(path, std::ios::binary);
      out << "\xEF\xBB\xBF";  // UTF-8 BOM
      out << "alpha\r\nbeta\r\ngamma\r\n";
    }
    auto set = ReadIgnoreListFromFile(path);
    BOOST_TEST_EQ(set.size(), 3u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    BOOST_TEST(set.count(L"gamma") == 1);
    ReportCase("T1 LoadIgnoreList reads UTF-8 BOM CRLF", scope.Passed());
    std::remove(path);
  }

  // T2: LoadIgnoreList skips blank / whitespace-only lines.
  {
    AssertionScope scope;
    const char* path = "test_ignore_blank.txt";
    {
      std::ofstream out(path, std::ios::binary);
      out << "\xEF\xBB\xBF";
      out << "alpha\r\n\r\n   \r\n\t\t\r\nbeta\r\n";
    }
    auto set = ReadIgnoreListFromFile(path);
    BOOST_TEST_EQ(set.size(), 2u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    ReportCase("T2 LoadIgnoreList skips blank lines", scope.Passed());
    std::remove(path);
  }

  // T3: LoadIgnoreList reads UTF-16 LE BOM file.
  {
    AssertionScope scope;
    const char* path = "test_ignore_utf16.txt";
    {
      std::ofstream out(path, std::ios::binary);
      out << "\xFF\xFE";  // UTF-16 LE BOM
      std::wstring content = L"alpha\r\nbeta\r\n";
      out.write((char*)content.data(),
                (std::streamsize)(content.size() * sizeof(wchar_t)));
    }
    auto set = ReadIgnoreListFromFile(path);
    BOOST_TEST_EQ(set.size(), 2u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    ReportCase("T3 LoadIgnoreList reads UTF-16 LE BOM", scope.Passed());
    std::remove(path);
  }

  // T4: _FilterIgnoredCandidates removes all candidates in the ignore set.
  {
    AssertionScope scope;
    std::vector<std::wstring> cands = {L"alpha", L"beta", L"gamma", L"delta"};
    std::unordered_set<std::wstring> ignore = {L"beta", L"delta"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 2u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"gamma");
    ReportCase("T4 FilterIgnoredCandidates removes all ignored",
               scope.Passed());
  }

  // T5: _FilterIgnoredCandidates leaves non-ignored candidates alone.
  {
    AssertionScope scope;
    std::vector<std::wstring> cands = {L"alpha", L"beta", L"gamma"};
    std::unordered_set<std::wstring> ignore = {L"xxx", L"yyy"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 3u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"beta");
    BOOST_TEST(cands[2] == L"gamma");
    ReportCase("T5 FilterIgnoredCandidates leaves non-ignored alone",
               scope.Passed());
  }

  // spec 092: 汇总行只在**真的**全过时才打 "5 / 5 assertions passed"。
  // 失败时打真实计数,不再无条件报喜。
  if (g_cases_failed == 0) {
    std::cout << "5 / 5 assertions passed" << std::endl;
  } else {
    std::cout << (g_cases_total - g_cases_failed) << " / " << g_cases_total
              << " cases passed, " << boost::detail::test_errors()
              << " failed assertions" << std::endl;
  }

  RunServerSideIgnoreMappingTests();
  return boost::report_errors();
}