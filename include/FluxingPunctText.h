// Fluxing · 标点语言转换(批次 I · P4)
//
// 背景(用户 2026-11-08 原话):「手点面板『中英文标点』切到半角 → 中文态打 ,
//   确认变成半角逗号;再点回,确认变 ,」。
//
// 引擎侧事实(librime/src/rime/gear/punctuator.cc):
//   - 标点映射表**只**由 full_shape 选项选表(:23-30);ascii_punct 只是让
//     处理器短路(:102 `return kNoop`),翻译路径(:329)完全不看 ascii_punct;
//   - 本 fork 的两张表(output/data/default.yaml:127 / :161)都映射中文标点
//     ⇒ 无论 ascii_punct 怎么切,引擎吐出来的永远是中文标点。
//   ⇒ 面板图标会翻转(读 live status)、引擎输出不翻转,二者必然背离。
//
// 决策(ADR 0043):
//   - **不改 librime**(P3)、**不碰 full_shape**(它一开,数字与符号会被
//     ShapeFormatter 全角化,违反「数字与 @#% 恒半角」这条硬要求);
//   - 由 fork 在**提交边界**做标点语言转换:半角标点状态下,把上屏文本里的
//     中文标点换成对应的 ASCII 标点;
//   - 转换只作用于「整串都是标点」的提交:词库文本(「你好,世界」)、数字与
//     字母都不受影响,避免把词条里的标点也改掉。
//
// 本文件是纯函数(无 I/O、无全局状态),生产代码与单测共用同一份实现(R7)。

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace fluxing {
namespace punct_text {

namespace detail {

// UTF-8 解码一个码点,返回消耗的字节数(非法字节按 1 字节跳过)。
inline std::size_t DecodeUtf8(const std::string& s, std::size_t pos,
                              std::uint32_t* cp) {
  const unsigned char c = static_cast<unsigned char>(s[pos]);
  std::size_t len = 1;
  std::uint32_t v = c;
  if ((c & 0xE0) == 0xC0) {
    len = 2;
    v = c & 0x1Fu;
  } else if ((c & 0xF0) == 0xE0) {
    len = 3;
    v = c & 0x0Fu;
  } else if ((c & 0xF8) == 0xF0) {
    len = 4;
    v = c & 0x07u;
  }
  if (pos + len > s.size()) {
    *cp = c;
    return 1;
  }
  for (std::size_t i = 1; i < len; ++i) {
    const unsigned char cc = static_cast<unsigned char>(s[pos + i]);
    if ((cc & 0xC0) != 0x80) {
      *cp = c;
      return 1;
    }
    v = (v << 6) | (cc & 0x3Fu);
  }
  *cp = v;
  return len;
}

// 单个码点 → ASCII 标点。返回 nullptr 表示「不是可转换的标点」。
inline const char* PunctToAscii(std::uint32_t cp) {
  switch (cp) {
    case 0xFF0C: return ",";    // ，
    case 0x3002: return ".";    // 。
    case 0xFF1F: return "?";    // ？
    case 0xFF01: return "!";    // ！
    case 0xFF1B: return ";";    // ；
    case 0xFF1A: return ":";    // ：
    case 0x3001: return ",";    // 、
    case 0x2018: return "'";    // ‘
    case 0x2019: return "'";    // ’
    case 0x201C: return "\"";   // “
    case 0x201D: return "\"";   // ”
    case 0xFF08: return "(";    // （
    case 0xFF09: return ")";    // ）
    case 0x300A: return "<";    // 《
    case 0x300B: return ">";    // 》
    case 0x3010: return "[";    // 【
    case 0x3011: return "]";    // 】
    case 0x300C: return "[";    // 「
    case 0x300D: return "]";    // 」
    case 0x300E: return "[";    // 『
    case 0x300F: return "]";    // 』
    case 0x00B7: return "`";    // ·(本 fork half_shape 表把 '`' 映射成 '·')
    case 0x00A5: return "$";    // ¥(本 fork half_shape 表把 '$' 映射成 '¥')
    case 0xFFE5: return "$";    // ￥
    case 0xFF3C: return "\\";   // ＼
    case 0xFF5E: return "~";    // ～
    case 0x301C: return "~";    // 〜
    case 0xFF40: return "`";    // ｀
    case 0xFF3F: return "_";    // ＿
    case 0xFF5B: return "{";    // ｛
    case 0xFF5D: return "}";    // ｝
    case 0x2039: return "<";    // ‹
    case 0x203A: return ">";    // ›
    case 0x00AB: return "<";    // «
    case 0x00BB: return ">";    // »
    default: return nullptr;
  }
}

}  // namespace detail

// 整串是否全部由「可转换的标点」组成(空串算 false)。
// 省略号 / 破折号只在成对时转换,单字也算标点。
inline bool IsPunctuationOnly(const std::string& text) {
  if (text.empty())
    return false;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::uint32_t cp = 0;
    pos += detail::DecodeUtf8(text, pos, &cp);
    if (cp == 0x2026 || cp == 0x2014)
      continue;  // … / —
    if (!detail::PunctToAscii(cp))
      return false;
  }
  return true;
}

// 中文标点 → ASCII 标点(成对写法优先:…… → ..., —— → --)。
inline std::string ToAsciiPunctuation(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  std::size_t pos = 0;
  while (pos < text.size()) {
    // 成对写法优先(UTF-8:…=E2 80 A6, —=E2 80 94)
    if (text.compare(pos, 6, "\xe2\x80\xa6\xe2\x80\xa6") == 0) {
      out += "...";
      pos += 6;
      continue;
    }
    if (text.compare(pos, 6, "\xe2\x80\x94\xe2\x80\x94") == 0) {
      out += "--";
      pos += 6;
      continue;
    }
    std::uint32_t cp = 0;
    const std::size_t len = detail::DecodeUtf8(text, pos, &cp);
    if (cp == 0x2026) {
      out += "...";
    } else if (cp == 0x2014) {
      out += "--";
    } else {
      const char* ascii = detail::PunctToAscii(cp);
      if (ascii)
        out += ascii;
      else
        out.append(text, pos, len);
    }
    pos += len;
  }
  return out;
}

// 提交边界入口:半角标点状态下,把「纯标点」提交里的中文标点换成 ASCII。
// 非纯标点文本(词库短语、数字、字母)原样返回。
inline std::string ApplyAsciiPunctuation(const std::string& text) {
  if (!IsPunctuationOnly(text))
    return text;
  return ToAsciiPunctuation(text);
}

}  // namespace punct_text
}  // namespace fluxing
