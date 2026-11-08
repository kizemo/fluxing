#pragma once
//
// ShortcutSettingsT110.h — 全局快捷键表基线 (baseline) 机制 (spec 045 · T110)
//
// 为什么需要它 (librime 语义, 勿再试探):
//  - `patch:` 是「按 key 覆盖」: librime config_compiler.cc EditNode 对普通键做
//    *target = value ⇒ patch 里一旦出现 key_binder/bindings, 就把 default.yaml
//    的**整张** bindings 列表换成 patch 给的那一份; 只有 `/+` 后缀才是追加。
//  - key_binder.cc ProcessKeyEvent 对同一 KeyEvent 取**第一个**条件匹配的
//    binding ⇒ 用 `/+` 追加不能「改键」, 旧绑定会继续生效。
//  ⇒ 落盘必须写「完整合并列表」: 基线全表 + 用户改动, 否则一次保存就把全局
//    快捷键表清成剩下的那几行 (T110 的破坏性 patch)。
//
// 本文件只放与 UI 无关的基线机制 (定位 / 解析 / 渲染), 不依赖
// ShortcutSettings 的 Hotkey 结构 ⇒ 单测可直接拿它造 fixture。
//
// 基线来源顺序: SetBaselinePath() 显式注入 (单测) > <user_data>/default.yaml
// > <WeaselServer.exe 所在目录>/data/default.yaml。读不到基线 ⇒ 拒绝写入
// (宁可不写, 也不能再清空全局表)。
//
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fluxing {
namespace shortcut_baseline {

// 基线里的一条 binding (来自 default.yaml 的 key_binder/bindings)
struct Binding {
  std::wstring accept;       // accept 值 (已去引号)
  std::wstring when;         // when 值 (空 = always)
  std::wstring action_yaml;  // "toggle: ascii_mode" / "send: 2"
  std::wstring raw;          // 原行 (去行首缩进), 未改的行逐字写回
  // false = 这条没解析成可用的 binding (块式写法 / 未知动作)。
  // 它照样留在基线里, 落盘时逐字写回 ⇒「解析不了」不等于「可以丢」。
  bool parsed = true;
};

// 基线状态 (进程内缓存: 加载与落盘必须看到同一份基线)
struct State {
  std::wstring override_path;  // SetBaselinePath 显式注入
  std::wstring path;           // 实际使用的文件
  std::vector<Binding> bindings;
  bool tried = false;
  bool ok = false;
};

inline State& BaselineState() {
  static State s;
  return s;
}

// 换基线文件时清缓存
inline void ResetBaseline() {
  State& s = BaselineState();
  s.path.clear();
  s.bindings.clear();
  s.tried = false;
  s.ok = false;
}

inline std::wstring TrimWs(const std::wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r'))
    ++a;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r'))
    --b;
  return s.substr(a, b - a);
}

inline std::wstring Unquote(const std::wstring& s) {
  std::wstring v = TrimWs(s);
  if (v.size() >= 2 && v.front() == L'"' && v.back() == L'"')
    v = v.substr(1, v.size() - 2);
  return v;
}

// when 归一: 空 = always (librime 语义)
inline std::wstring WhenOf(const std::wstring& when) {
  return when.empty() ? std::wstring(L"always") : when;
}

inline bool FileExists(const std::wstring& path) {
  if (path.empty())
    return false;
  const DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// 读文件 → wstring (去 UTF-8 BOM)
inline bool ReadTextFileUtf8(const std::wstring& path, std::wstring& out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
  if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }
  if (content.empty())
    return true;
  const int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                                       static_cast<int>(content.size()),
                                       nullptr, 0);
  if (wlen <= 0)
    return false;
  out.resize(wlen);
  MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                      static_cast<int>(content.size()), &out[0], wlen);
  return true;
}

// 拆 inline dict 的内容部分: "k: v, k: v" (键序任意, 值可带引号,
// 引号内的逗号不算分隔符)
inline bool ParseInlineDict(const std::wstring& body,
                            std::map<std::wstring, std::wstring>& kv) {
  size_t cur = 0;
  while (cur <= body.size()) {
    size_t seg_end = body.size();
    bool in_quote = false;
    for (size_t i = cur; i < body.size(); ++i) {
      const wchar_t c = body[i];
      if (c == L'"') {
        in_quote = !in_quote;
      } else if (!in_quote && c == L',') {
        seg_end = i;
        break;
      }
    }
    const std::wstring seg = TrimWs(body.substr(cur, seg_end - cur));
    if (!seg.empty()) {
      const size_t colon = seg.find(L':');
      if (colon != std::wstring::npos) {
        const std::wstring k = TrimWs(seg.substr(0, colon));
        if (!k.empty())
          kv[k] = TrimWs(seg.substr(colon + 1));
      }
    }
    if (seg_end >= body.size())
      break;
    cur = seg_end + 1;
  }
  return !kv.empty();
}

inline std::wstring DictGet(const std::map<std::wstring, std::wstring>& kv,
                            const wchar_t* key) {
  const auto it = kv.find(key);
  return it == kv.end() ? std::wstring() : it->second;
}

// librime key_binder 支持的动作 + 本仓应用级 custom: 动作
inline std::wstring ActionOfDict(const std::map<std::wstring, std::wstring>& kv) {
  static const wchar_t* kActions[] = {L"send",         L"send_sequence",
                                      L"toggle",       L"set_option",
                                      L"unset_option", L"select",
                                      L"custom"};
  for (const wchar_t* key : kActions) {
    const auto it = kv.find(key);
    if (it == kv.end())
      continue;
    const std::wstring v = Unquote(it->second);
    if (v.empty())
      continue;
    return std::wstring(key) + L": " + v;
  }
  return L"";
}

// 从 default.yaml 取 key_binder/bindings 段:
// 顶层 `key_binder:` → 段内 `bindings:` → 缩进 ≥4 的 "- { … }" 行。
// 注释行跳过; 行尾注释保留在 raw 里 (未改的行逐字写回)。
inline bool LoadBaselineFromFile(const std::wstring& path,
                                 std::vector<Binding>& out) {
  out.clear();
  std::wstring text;
  if (!ReadTextFileUtf8(path, text) || text.empty())
    return false;

  std::vector<std::wstring> lines;
  {
    size_t pos = 0;
    while (pos <= text.size()) {
      const size_t eol = text.find(L'\n', pos);
      if (eol == std::wstring::npos) {
        lines.push_back(text.substr(pos));
        break;
      }
      std::wstring ln = text.substr(pos, eol - pos);
      if (!ln.empty() && ln.back() == L'\r')
        ln.pop_back();
      lines.push_back(ln);
      pos = eol + 1;
    }
  }

  // 1) 顶层 key_binder:
  size_t kb = lines.size();
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::wstring& s = lines[i];
    if (s.empty() || s[0] == L' ' || s[0] == L'\t' || s[0] == L'#')
      continue;
    if (TrimWs(s) == L"key_binder:") {
      kb = i;
      break;
    }
  }
  if (kb == lines.size())
    return false;

  // 2) key_binder 段尾: 下一个行首无缩进的非空非注释行
  size_t kb_end = lines.size();
  for (size_t i = kb + 1; i < lines.size(); ++i) {
    const std::wstring& s = lines[i];
    if (s.empty() || s[0] == L' ' || s[0] == L'\t' || s[0] == L'#')
      continue;
    kb_end = i;
    break;
  }

  // 3) 段内 bindings:
  size_t bind_idx = lines.size();
  for (size_t i = kb + 1; i < kb_end; ++i) {
    if (TrimWs(lines[i]) == L"bindings:") {
      bind_idx = i;
      break;
    }
  }
  if (bind_idx == lines.size())
    return false;

  // 4) 条目: 缩进 ≥4 的 "- { … }" 行
  for (size_t i = bind_idx + 1; i < kb_end; ++i) {
    const std::wstring& s = lines[i];
    const std::wstring t = TrimWs(s);
    if (t.empty() || t[0] == L'#')
      continue;
    size_t indent = 0;
    while (indent < s.size() && (s[indent] == L' ' || s[indent] == L'\t'))
      ++indent;
    if (indent < 4)
      break;  // 回到 bindings 同级 ⇒ 段结束
    if (t[0] != L'-')
      continue;
    // 这一条 + 它的续行 (块式写法的第二行起): 续行逐字保留, 落盘时原样写回
    std::wstring raw = s.substr(indent);
    size_t next = i + 1;
    while (next < kb_end) {
      const std::wstring& s2 = lines[next];
      const std::wstring t2 = TrimWs(s2);
      if (t2.empty() || t2[0] == L'#')
        break;
      size_t ind2 = 0;
      while (ind2 < s2.size() && (s2[ind2] == L' ' || s2[ind2] == L'\t'))
        ++ind2;
      if (ind2 <= indent)
        break;
      raw += L"\n" + s2;
      ++next;
    }
    i = next - 1;  // for 循环自己会 ++i

    const size_t open = t.find(L'{');
    size_t close = std::wstring::npos;
    if (open != std::wstring::npos) {
      bool in_quote = false;
      for (size_t j = open + 1; j < t.size(); ++j) {
        const wchar_t c = t[j];
        if (c == L'"')
          in_quote = !in_quote;
        else if (!in_quote && c == L'}') {
          close = j;
          break;
        }
      }
    }
    std::map<std::wstring, std::wstring> kv;
    bool inlined = false;
    if (open != std::wstring::npos && close != std::wstring::npos)
      inlined = ParseInlineDict(t.substr(open + 1, close - open - 1), kv);
    Binding b;
    b.raw = raw;
    if (inlined) {
      b.accept = Unquote(DictGet(kv, L"accept"));
      b.when = Unquote(DictGet(kv, L"when"));
      b.action_yaml = ActionOfDict(kv);
      if (b.accept.empty() || b.action_yaml.empty()) {
        // 解析不出来的条目也要留在基线里: 落盘时逐字写回, 少一行就是丢一行。
        b.parsed = false;
        b.accept.clear();
        b.action_yaml.clear();
      }
    } else {
      // 不是 inline dict (块式写法) ⇒ 原样收进基线, 落盘时逐字写回
      b.parsed = false;
    }
    out.push_back(b);
  }
  return true;
}

// 基线路径: 显式注入 > <user_data>/default.yaml > <exe 目录>/data/default.yaml
inline std::wstring ResolveBaselinePath(const std::wstring& user_data_dir) {
  const State& s = BaselineState();
  if (!s.override_path.empty())
    return s.override_path;
  if (!user_data_dir.empty()) {
    const std::wstring p = user_data_dir + L"\\default.yaml";
    if (FileExists(p))
      return p;
  }
  wchar_t exe[MAX_PATH] = {0};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) > 0) {
    std::wstring dir(exe);
    const size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos)
      dir = dir.substr(0, pos);
    const std::wstring p = dir + L"\\data\\default.yaml";
    if (FileExists(p))
      return p;
  }
  return L"";
}

// 基线只加载一次; 空表 ⇒ 不可用 (宁可不写, 也不能清空全局表)
inline bool EnsureBaseline(const std::wstring& user_data_dir) {
  State& s = BaselineState();
  if (s.tried)
    return s.ok;
  s.tried = true;
  s.ok = false;
  s.path = ResolveBaselinePath(user_data_dir);
  if (s.path.empty())
    return false;
  std::vector<Binding> b;
  if (!LoadBaselineFromFile(s.path, b) || b.empty())
    return false;
  // 至少要有一条能用的 binding 才算基线可用 (一条都解析不出来 = 基线不可信)
  bool any_parsed = false;
  for (const auto& x : b) {
    if (x.parsed) {
      any_parsed = true;
      break;
    }
  }
  if (!any_parsed)
    return false;
  s.bindings = b;
  s.ok = true;
  return true;
}

// 修饰键签名: "Control+1" → "Control"; 同动作有多条时用它挑最像的那一条
// (send: 2 这类重名动作在基线里可能出现两次)
inline std::wstring ModifierSignature(const std::wstring& accept) {
  std::vector<std::wstring> mods;
  size_t pos = 0;
  while (true) {
    const size_t plus = accept.find(L'+', pos);
    if (plus == std::wstring::npos)
      break;
    mods.push_back(accept.substr(pos, plus - pos));
    pos = plus + 1;
  }
  std::sort(mods.begin(), mods.end());
  std::wstring sig;
  for (const auto& m : mods) {
    if (!sig.empty())
      sig += L"+";
    sig += m;
  }
  return sig;
}

// canonical 行文本 (4 空格缩进): "- { when: W, accept: "A", action }"
inline std::wstring RenderRow(const std::wstring& accept,
                              const std::wstring& when,
                              const std::wstring& action_yaml) {
  if (accept.empty() || action_yaml.empty())
    return L"";
  std::wstring a = accept;
  bool need_quote = false;
  for (wchar_t c : a) {
    if (c == L' ' || c == L':' || c == L'+' || c == L'-') {
      need_quote = true;
      break;
    }
  }
  if (need_quote)
    a = L"\"" + a + L"\"";
  return L"    - { when: " + WhenOf(when) + L", accept: " + a + L", " +
         action_yaml + L" }";
}

}  // namespace shortcut_baseline
}  // namespace fluxing
