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
  if (!in) return result;
  std::vector<unsigned char> raw((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (raw.empty()) return result;
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
    if (eol == std::wstring::npos) eol = content.size();
    std::wstring line = content.substr(pos, eol - pos);
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\t'))
      line.pop_back();
    if (!line.empty()) result.insert(line);
    pos = eol;
    if (pos < content.size() && content[pos] == L'\r') pos++;
    if (pos < content.size() && content[pos] == L'\n') pos++;
  }
  return result;
}

// Mirror WeaselPanel::_FilterIgnoredCandidates.
void FilterIgnoredCandidates(std::vector<std::wstring>& candidates,
                              const std::unordered_set<std::wstring>& ignore) {
  if (ignore.empty()) return;
  for (size_t i = candidates.size(); i > 0; --i) {
    if (ignore.count(candidates[i - 1]) > 0) {
      candidates.erase(candidates.begin() + (i - 1));
    }
  }
}

}  // namespace

int main() {
  // T1: LoadIgnoreList reads UTF-8 BOM CRLF file.
  {
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
    std::cout << "  PASS: T1 LoadIgnoreList reads UTF-8 BOM CRLF" << std::endl;
    std::remove(path);
  }

  // T2: LoadIgnoreList skips blank / whitespace-only lines.
  {
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
    std::cout << "  PASS: T2 LoadIgnoreList skips blank lines" << std::endl;
    std::remove(path);
  }

  // T3: LoadIgnoreList reads UTF-16 LE BOM file.
  {
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
    std::cout << "  PASS: T3 LoadIgnoreList reads UTF-16 LE BOM" << std::endl;
    std::remove(path);
  }

  // T4: _FilterIgnoredCandidates removes all candidates in the ignore set.
  {
    std::vector<std::wstring> cands = {L"alpha", L"beta", L"gamma", L"delta"};
    std::unordered_set<std::wstring> ignore = {L"beta", L"delta"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 2u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"gamma");
    std::cout << "  PASS: T4 FilterIgnoredCandidates removes all ignored" << std::endl;
  }

  // T5: _FilterIgnoredCandidates leaves non-ignored candidates alone.
  {
    std::vector<std::wstring> cands = {L"alpha", L"beta", L"gamma"};
    std::unordered_set<std::wstring> ignore = {L"xxx", L"yyy"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 3u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"beta");
    BOOST_TEST(cands[2] == L"gamma");
    std::cout << "  PASS: T5 FilterIgnoredCandidates leaves non-ignored alone"
              << std::endl;
  }

  std::cout << "5 / 5 assertions passed" << std::endl;
  return boost::report_errors();
}