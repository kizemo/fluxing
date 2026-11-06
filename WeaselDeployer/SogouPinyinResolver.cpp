// SogouPinyinResolver.cpp — 搜狗词条拼音反查实现。
//
// 纯 C++17：只用 <cstdlib>/<filesystem>/<fstream>/<string>/<vector>，
// 不引入任何 Windows API，保证 test/TestSogouUserDict 能独立编译本文件。

#include "SogouPinyinResolver.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace weasel {
namespace sogou {

namespace {

const char kBaseDictFile[] = "base.dict.yaml";
const char kCharDictFile[] = "8105.dict.yaml";

// 统计 UTF-8 字符数。
size_t CountUtf8Chars(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) {
    if ((c & 0xC0) != 0x80) {
      ++n;
    }
  }
  return n;
}

// 拆一行「词条 \t 码 [\t 权重]」。
// 注意：码内部用空格分隔音节（"a a"），因此只能剥掉行尾残留的
// \r / \n，绝不能对码做整体 trim。
bool SplitDictRow(const std::string& line,
                  std::string* phrase,
                  std::string* code,
                  double* weight) {
  const size_t t1 = line.find('\t');
  if (t1 == std::string::npos) {
    return false;
  }
  const size_t t2 = line.find('\t', t1 + 1);
  phrase->assign(line, 0, t1);
  if (t2 == std::string::npos) {
    code->assign(line, t1 + 1, std::string::npos);
    *weight = 0.0;
  } else {
    code->assign(line, t1 + 1, t2 - t1 - 1);
    *weight = std::strtod(line.c_str() + t2 + 1, nullptr);
  }
  while (!code->empty() && (code->back() == '\r' || code->back() == '\n')) {
    code->pop_back();
  }
  return !phrase->empty() && !code->empty();
}

// 取 UTF-8 字符串第 index 个字符的字节长度。
size_t Utf8CharLen(unsigned char c) {
  if (c >= 0xF0) return 4;
  if (c >= 0xE0) return 3;
  if (c >= 0xC0) return 2;
  return 1;
}

}  // namespace

SogouPinyinResolver::SogouPinyinResolver() = default;

bool SogouPinyinResolver::Build(
    const std::string& cn_dicts_dir,
    const std::unordered_set<std::string>& wanted) {
  phrase_map_.clear();
  char_map_.clear();
  phrase_count_ = 0;
  char_count_ = 0;

  const std::filesystem::path dir = std::filesystem::u8path(cn_dicts_dir);
  bool ok = true;
  if (!ScanBase((dir / kBaseDictFile).u8string(), wanted)) {
    ok = false;
  }
  if (!ScanChars((dir / kCharDictFile).u8string())) {
    ok = false;
  }
  return ok;
}

bool SogouPinyinResolver::ScanBase(
    const std::string& path,
    const std::unordered_set<std::string>& wanted) {
  std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
  if (!in) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string phrase;
    std::string code;
    double weight = 0.0;
    if (!SplitDictRow(line, &phrase, &code, &weight)) {
      continue;
    }
    if (wanted.find(phrase) == wanted.end()) {
      continue;
    }
    auto it = phrase_map_.find(phrase);
    if (it == phrase_map_.end()) {
      phrase_map_.emplace(phrase, std::make_pair(code, weight));
      ++phrase_count_;
    } else if (weight > it->second.second) {
      it->second.first = code;
      it->second.second = weight;
    }
  }
  return true;
}

bool SogouPinyinResolver::ScanChars(const std::string& path) {
  std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
  if (!in) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string phrase;
    std::string code;
    double weight = 0.0;
    if (!SplitDictRow(line, &phrase, &code, &weight)) {
      continue;
    }
    if (CountUtf8Chars(phrase) != 1) {
      continue;
    }
    auto it = char_map_.find(phrase);
    if (it == char_map_.end()) {
      char_map_.emplace(phrase, std::make_pair(code, weight));
      ++char_count_;
    } else if (weight > it->second.second) {
      it->second.first = code;
      it->second.second = weight;
    }
  }
  return true;
}

bool SogouPinyinResolver::Resolve(const std::string& phrase,
                                  std::string* code) const {
  if (!code) {
    return false;
  }
  code->clear();
  if (phrase.empty()) {
    return false;
  }

  // 整词优先：base 词表里有整词且注音明确时直接采用。
  auto it = phrase_map_.find(phrase);
  if (it != phrase_map_.end()) {
    *code = it->second.first;
    return true;
  }

  // 逐字回退：单字表是《通用规范汉字表》注音，覆盖度最高。
  std::string out;
  size_t i = 0;
  while (i < phrase.size()) {
    const size_t len = Utf8CharLen(static_cast<unsigned char>(phrase[i]));
    if (i + len > phrase.size()) {
      return false;
    }
    const std::string ch = phrase.substr(i, len);
    auto ci = char_map_.find(ch);
    if (ci == char_map_.end()) {
      return false;
    }
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += ci->second.first;
    i += len;
  }
  if (out.empty()) {
    return false;
  }
  *code = out;
  return true;
}

}  // namespace sogou
}  // namespace weasel
