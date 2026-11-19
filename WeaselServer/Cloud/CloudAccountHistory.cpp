// 批次 AT-1 · 账号页「历史邮箱」存储实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudAccountHistory.h"

// 用户数据目录真相源（header-only）。
#include "../../include/FluxingUserPaths.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {
namespace account_history {

namespace {

// 去首尾空白（ASCII 空白）。
std::string Trim(const std::string& s) {
  size_t begin = 0;
  size_t end = s.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])))
    ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
    --end;
  return s.substr(begin, end - begin);
}

// 邮箱的比较键：ASCII 小写（邮箱语义上不区分大小写）。
std::string FoldKey(const std::string& s) {
  std::string key = s;
  for (size_t i = 0; i < key.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(key[i]);
    if (c >= 'A' && c <= 'Z')
      key[i] = static_cast<char>(c - 'A' + 'a');
  }
  return key;
}

// 去掉列表里重复的邮箱（保留第一次出现的位置），并截断到上限。
std::vector<std::string> Normalize(const std::vector<std::string>& in) {
  std::vector<std::string> out;
  std::vector<std::string> seen;
  for (size_t i = 0; i < in.size(); ++i) {
    const std::string trimmed = Trim(in[i]);
    if (trimmed.empty())
      continue;
    const std::string key = FoldKey(trimmed);
    if (std::find(seen.begin(), seen.end(), key) != seen.end())
      continue;
    seen.push_back(key);
    out.push_back(trimmed);
    if (out.size() >= kMaxHistoryEntries)
      break;
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// 纯逻辑
// ---------------------------------------------------------------------------

std::vector<std::string> MergeHistory(const std::vector<std::string>& history,
                                      const std::string& email) {
  std::vector<std::string> out = Normalize(history);
  const std::string trimmed = Trim(email);
  if (trimmed.empty())
    return out;

  const std::string key = FoldKey(trimmed);
  // 已存在 ⇒ 提到最前（最近在前），并保留**新**的大小写形态。
  for (size_t i = 0; i < out.size(); ++i) {
    if (FoldKey(out[i]) == key) {
      out.erase(out.begin() + static_cast<long>(i));
      break;
    }
  }
  out.insert(out.begin(), trimmed);
  if (out.size() > kMaxHistoryEntries)
    out.resize(kMaxHistoryEntries);
  return out;
}

std::string SerializeHistory(const std::vector<std::string>& history) {
  const std::vector<std::string> list = Normalize(history);
  std::string text;
  for (size_t i = 0; i < list.size(); ++i) {
    text += list[i];
    text += "\n";
  }
  return text;
}

std::vector<std::string> ParseHistory(const std::string& text) {
  std::vector<std::string> raw;
  std::string line;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      raw.push_back(line);
      line.clear();
      continue;
    }
    if (text[i] == '\r')
      continue;
    line.push_back(text[i]);
  }
  if (!line.empty())
    raw.push_back(line);
  return Normalize(raw);
}

// ---------------------------------------------------------------------------
// 磁盘读写
// ---------------------------------------------------------------------------

std::wstring DefaultHistoryPath() {
  return ResolveFluxingUserDataPath() / L"fluxing-cloud-accounts.history";
}

bool SaveHistoryFile(const std::wstring& path,
                     const std::vector<std::string>& history,
                     std::string* err) {
  if (path.empty()) {
    if (err)
      *err = "历史邮箱落盘路径为空。";
    return false;
  }
  // 目录不存在就建一个（用户数据目录一般已存在，但换机器/换路径时未必）。
  std::error_code ec;
  const std::filesystem::path parent = std::filesystem::path(path).parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec && !std::filesystem::is_directory(parent)) {
      if (err)
        *err = "无法创建历史邮箱目录：" + parent.string();
      return false;
    }
  }

  const std::string payload = SerializeHistory(history);
  HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    if (err)
      *err = "无法写入历史邮箱文件。";
    return false;
  }
  DWORD written = 0;
  const BOOL ok = ::WriteFile(file, payload.data(),
                               static_cast<DWORD>(payload.size()), &written,
                               nullptr);
  ::CloseHandle(file);
  if (!ok || written != payload.size()) {
    if (err)
      *err = "写入历史邮箱文件失败。";
    return false;
  }
  return true;
}

std::vector<std::string> LoadHistoryFile(const std::wstring& path) {
  std::vector<std::string> out;
  if (path.empty())
    return out;
  HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return out;  // 文件不存在 ⇒ 空列表（不是错误）
  std::string content;
  char buffer[4096];
  DWORD read = 0;
  while (::ReadFile(file, buffer, sizeof(buffer), &read, nullptr) &&
         read > 0) {
    content.append(buffer, read);
  }
  ::CloseHandle(file);
  return ParseHistory(content);
}

}  // namespace account_history
}  // namespace cloud
}  // namespace fluxing
