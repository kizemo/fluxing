// SogouUserDictParser.cpp — 搜狗用户词库解析器实现。
//
// 纯 C++17：只用 <cstdint>/<fstream>/<string>/<vector>，不引入任何
// Windows API，保证 test/TestSogouUserDict 能独立编译本文件。

#include "SogouUserDictParser.h"

#include <cstdio>
#include <fstream>

namespace weasel {
namespace sogou {

namespace {

// 单条记录允许的最大字数（搜狗自定义词最长 16 字）。
constexpr size_t kMaxChars = 16;
// 词条字节数上限（32 个汉字）。
constexpr size_t kMaxBytes = 64;
// 后缀固定部分长度：flag(2) + seq(2) + reserved(4) + count(2)
constexpr size_t kSuffixHeaderBytes = 10;

inline uint16_t ReadU16(const unsigned char* p) {
  return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(p[1] << 8);
}

// 统计 UTF-8 字符数（词条已经是 UTF-8）。
inline size_t CountUtf8Chars(const std::string& s) {
  size_t n = 0;
  for (unsigned char c : s) {
    if ((c & 0xC0) != 0x80) {
      ++n;
    }
  }
  return n;
}

// 是否是统一表意文字（CJK）。覆盖基本区与扩展 A/B。
inline bool IsCjk(uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x20000 && cp <= 0x2A6DF);
}

// 把 UTF-16LE 片段转成 UTF-8。全部字符必须是 CJK，否则返回 false。
bool DecodeCjkPhrase(const unsigned char* p,
                      size_t byte_len,
                      std::string* out) {
  if (byte_len == 0 || (byte_len % 2) != 0) {
    return false;
  }
  const size_t char_count = byte_len / 2;
  if (char_count == 0 || char_count > kMaxChars) {
    return false;
  }
  out->clear();
  out->reserve(char_count * 3);
  for (size_t i = 0; i < char_count; ++i) {
    const uint32_t cp = ReadU16(p + i * 2);
    if (!IsCjk(cp)) {
      return false;
    }
    if (cp < 0x800) {
      out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return true;
}

}  // namespace

bool ParseSogouUserDict(const unsigned char* data,
                        size_t size,
                        SogouParseStats* stats,
                        std::vector<SogouEntry>* entries) {
  if (stats) {
    *stats = SogouParseStats();
  }
  if (entries) {
    entries->clear();
  }
  if (!data || size == 0) {
    return false;
  }

  size_t record_count = 0;
  size_t skipped = 0;

  // 签名扫描：逐字节找 (u16 A, u16 B) 满足 A == B + 2。
  for (size_t i = 0; i + 4 <= size; ++i) {
    const uint16_t a = ReadU16(data + i);
    const uint16_t b = ReadU16(data + i + 2);
    if (b < 2 || b > kMaxBytes || (b & 1) != 0) {
      continue;
    }
    if (a != static_cast<uint16_t>(b + 2)) {
      continue;
    }
    const size_t phrase_offset = i + 4;
    if (phrase_offset + b > size) {
      continue;
    }

    std::string phrase;
    if (!DecodeCjkPhrase(data + phrase_offset, b, &phrase)) {
      continue;
    }

    SogouEntry entry;
    entry.phrase = phrase;

    // 尝试解析后缀：只有 K == 字数时才认为音节索引可信。
    const size_t suffix_offset = phrase_offset + b;
    if (suffix_offset + kSuffixHeaderBytes <= size) {
      const uint16_t flag = ReadU16(data + suffix_offset);
      const uint16_t count = ReadU16(data + suffix_offset + 8);
      if (flag == 1 && count == phrase.size() / 3) {
        const size_t need = suffix_offset + kSuffixHeaderBytes +
                            static_cast<size_t>(count) * 2;
        if (need <= size) {
          entry.syllables.reserve(count);
          for (uint16_t k = 0; k < count; ++k) {
            entry.syllables.push_back(
                ReadU16(data + suffix_offset + kSuffixHeaderBytes + k * 2));
          }
        } else {
          ++skipped;
        }
      } else {
        ++skipped;
      }
    } else {
      ++skipped;
    }

    if (entries) {
      entries->push_back(std::move(entry));
    }
    ++record_count;
  }

  if (stats) {
    stats->bytes_scanned = size;
    stats->record_count = record_count;
    stats->skipped = skipped;
  }
  return record_count > 0;
}

bool ReadWholeFile(const std::string& path, std::vector<unsigned char>* out) {
  if (!out) {
    return false;
  }
  out->clear();
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  if (size <= 0) {
    return false;
  }
  in.seekg(0, std::ios::beg);
  out->resize(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(out->data()), size)) {
    out->clear();
    return false;
  }
  return true;
}

const char* const kSogouUserDictFileNames[] = {
    "sgim_gd_usr.bin",
    "sgim_gd_usr_od.bin",
};
const size_t kSogouUserDictFileNameCount =
    sizeof(kSogouUserDictFileNames) / sizeof(kSogouUserDictFileNames[0]);

}  // namespace sogou
}  // namespace weasel
