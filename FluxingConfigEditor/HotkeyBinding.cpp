#include "stdafx.h"
// spec 050 T001 - HotkeyBinding.cpp
//
// Implementation of the parsing/serialization helpers declared in
// HotkeyBinding.h. Mostly pure C++; the UTF-8 <-> UTF-16 conversion
// helpers (WideToUtf8 / Utf8ToWide) use Win32 MultiByteToWideChar
// so we need windows.h. When this file is built inside WeaselDeployer
// the stdafx.h has already pulled windows.h in; we re-include guarded
// to also work standalone in TestHotkeyEditor / other test targets.

#include "HotkeyBinding.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace fluxing {

namespace {

std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return std::string();
  int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                    nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return std::string();
  std::string out(needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], needed,
                      nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return std::wstring();
  int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(),
                                    nullptr, 0);
  if (needed <= 0) return std::wstring();
  std::wstring out(needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], needed);
  return out;
}

std::string Trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && std::isspace((unsigned char)s[b])) ++b;
  while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
  return s.substr(b, e - b);
}

std::wstring ToUpperFirst(const std::wstring& s) {
  std::wstring out = s;
  if (!out.empty()) out[0] = (wchar_t)towupper(out[0]);
  return out;
}

}  // namespace

const wchar_t* WhenToString(HotkeyWhen w) {
  switch (w) {
    case HotkeyWhen::Always:    return L"always";
    case HotkeyWhen::Composing: return L"composing";
    case HotkeyWhen::HasMenu:   return L"has_menu";
    case HotkeyWhen::Paging:    return L"paging";
    case HotkeyWhen::Empty:     return L"";
  }
  return L"";
}

HotkeyWhen WhenFromString(const std::wstring& s) {
  if (s == L"always")    return HotkeyWhen::Always;
  if (s == L"composing") return HotkeyWhen::Composing;
  if (s == L"has_menu")  return HotkeyWhen::HasMenu;
  if (s == L"paging")    return HotkeyWhen::Paging;
  return HotkeyWhen::Empty;
}

const char* ActionToString(HotkeyAction a) {
  switch (a) {
    case HotkeyAction::Send:   return "send";
    case HotkeyAction::Toggle: return "toggle";
    case HotkeyAction::Select: return "select";
  }
  return "send";
}

HotkeyAction ActionFromString(const std::string& s) {
  if (s == "send")   return HotkeyAction::Send;
  if (s == "toggle") return HotkeyAction::Toggle;
  if (s == "select") return HotkeyAction::Select;
  return HotkeyAction::Send;
}

std::wstring NormalizeAccept(const std::wstring& accept) {
  if (accept.empty()) return L"";
  // Tokenize on '+'. Each token: expand common aliases + capitalize
  // first char (librime uses CamelCase for key names: Shift, Tab,
  // Return, Page_Up, etc.).
  std::vector<std::wstring> tokens;
  size_t start = 0;
  for (size_t i = 0; i <= accept.size(); ++i) {
    if (i == accept.size() || accept[i] == L'+') {
      if (i > start) {
        std::wstring tok = accept.substr(start, i - start);
        // Trim
        size_t tb = 0, te = tok.size();
        while (tb < te && iswspace(tok[tb])) ++tb;
        while (te > tb && iswspace(tok[te - 1])) --te;
        tok = tok.substr(tb, te - tb);
        // Lowercase first to apply aliases uniformly
        std::wstring lower;
        lower.reserve(tok.size());
        for (auto c : tok) lower.push_back((wchar_t)towlower(c));
        if (lower == L"ctrl" || lower == L"control") tok = L"Control";
        else if (lower == L"shift") tok = L"Shift";
        else if (lower == L"alt") tok = L"Alt";
        else if (lower == L"win" || lower == L"super" || lower == L"meta") tok = L"Control";  // remap to Control (librime uses Control, not Win/Super)
        else if (lower == L"caps") tok = L"Caps_Lock";
        else if (!tok.empty()) {
          // Capitalize first letter
          tok[0] = (wchar_t)towupper(tok[0]);
        }
        if (!tok.empty()) tokens.push_back(tok);
      }
      start = i + 1;
    }
  }
  if (tokens.empty()) return L"";
  std::wstring out;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0) out += L"+";
    out += tokens[i];
  }
  return out;
}

const wchar_t* LintAcceptForRiskyBindings(const std::wstring& accept) {
  // L18/L19 lesson: keycode=Shift_L/R single-key binding collides with
  // TSF release events (shift+<other key> release fires Shift_L up).
  // librime 1.13 KeyEvent::operator== compares keycode + modifier
  // strictly. A binding like { accept: Shift_L } (no modifier) will
  // match release events. The safe forms are:
  //   { accept: Shift+Shift_L } - modifier present, doesn't match
  //                               release events with modifier=0.
  // Detect bare Shift_L / Shift_R / Control_L / etc. (no '+' in
  // accept means no modifier).
  if (accept.find(L'+') == std::wstring::npos) {
    if (accept == L"Shift_L" || accept == L"Shift_R" ||
        accept == L"Control_L" || accept == L"Control_R" ||
        accept == L"Alt_L" || accept == L"Alt_R") {
      return L"\x26A0  \u8b66\u544a\uff1a\u5355\u952e Shift_L/R / Control_L/R \u7ed1\u5b9a\u4e0e TSF release event \u4f1a\u8bef\u5339\u914d\u3002\n"
             L"\u63a8\u8350\u6539\u4e3a\u201cShift+Shift_L\u201d \u6216\u201cControl+1\u201d\u7b49\u5e26\u4fee\u9970\u952e\u7684\u5f62\u5f0f\u3002\n"
             L"\n"
             L"\u8981\u7ee7\u7eed\u4fdd\u5b58\u5417\uff1f";
    }
  }
  // L17: Shift+Shift_L style is the L21-recommended form. No warning.
  return nullptr;
}

std::string HotkeyBinding::ToYamlInline(int indent) const {
  std::ostringstream os;
  os << std::string(indent, ' ') << "{ ";
  if (when != HotkeyWhen::Empty) {
    os << "when: " << WideToUtf8(WhenToString(when)) << ", ";
  }
  os << "accept: " << WideToUtf8(accept) << ", ";
  os << ActionToString(action) << ": " << action_value << " }";
  return os.str();
}

bool HotkeyBinding::FromYamlInline(const std::string& line,
                                    HotkeyBinding* out) {
  if (!out) return false;
  out->when = HotkeyWhen::Empty;
  out->accept.clear();
  out->action = HotkeyAction::Send;
  out->action_value.clear();

  // Strip leading "{ " and trailing " }"
  std::string s = Trim(line);
  if (s.size() < 2 || s.front() != '{' || s.back() != '}') return false;
  s = s.substr(1, s.size() - 2);

  // Tokenize on commas - each token is "key: value" (or just "key" in
  // the case of single-key entries). We do a simple split, ignoring
  // quoted values for v0 (librime doesn't use quoted values in
  // bindings). For values with spaces in them (rare), this would
  // mis-tokenize - acceptable for v0 since `accept` strings never
  // contain commas.
  std::vector<std::pair<std::string, std::string>> kv;
  std::stringstream ss(s);
  std::string segment;
  while (std::getline(ss, segment, ',')) {
    segment = Trim(segment);
    size_t colon = segment.find(':');
    if (colon == std::string::npos) continue;
    std::string k = Trim(segment.substr(0, colon));
    std::string v = Trim(segment.substr(colon + 1));
    kv.push_back({k, v});
  }

  for (auto& p : kv) {
    if (p.first == "when") {
      out->when = WhenFromString(Utf8ToWide(p.second));
    } else if (p.first == "accept") {
      out->accept = Utf8ToWide(p.second);
    } else if (p.first == "send") {
      out->action = HotkeyAction::Send;
      out->action_value = p.second;
    } else if (p.first == "toggle") {
      out->action = HotkeyAction::Toggle;
      out->action_value = p.second;
    } else if (p.first == "select") {
      out->action = HotkeyAction::Select;
      out->action_value = p.second;
    }
  }
  return !out->accept.empty();
}

}  // namespace fluxing