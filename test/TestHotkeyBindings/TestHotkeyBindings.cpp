// TestHotkeyBindings.cpp -- runtime acceptance gate for the Fluxing TSF hotkey
// layer.
//
// WHAT IT PROVES
// --------------
// A key_binder binding only fires when the (keycode, modifier) pair the
// TSF front end sends to librime is *exactly* the pair librime parsed out
// of `accept:` -- librime compares both fields for equality
// (librime/src/rime/key_event.h:43 KeyEvent::operator==, and
// librime/src/rime/gear/key_binder.cc:276 does a std::map lookup on that
// pair).
//
// The Fluxing TSF front end does NOT send the physical key. For a
// printable key it sends the *character* the key produced
// (WeaselTSF/KeyEvent.cpp:51 ToUnicodeEx), so `accept: "Shift+comma"`
// asks for keycode 0x2C (',') while the front end sends 0x3C ('<') --
// the binding can never fire.
//
// This gate measures that gap with the product's own code: the very same
// WeaselTSF/KeyEvent.cpp is compiled into this binary, so the set of
// reachable (keycode, modifier) pairs is produced by production code, not
// by a re-implementation that could drift.
//
// The front end also swallows the Shift key itself
// (WeaselTSF/KeyEventSink.cpp:44 -- spec 077), so a binding that waits
// for Shift_L / Shift_R can never fire either. That rule is modelled
// here too.
//
// USAGE
//   TestHotkeyBindings.exe [options] <yaml-file> [<yaml-file> ...]
//
//   --baseline <file>   known-dead bindings that must not fail the gate
//   --layout <hex-lcid> activate this keyboard layout (default: 0409 US)
//   --no-layout         keep the current keyboard layout
//
// EXIT CODE
//   0  every binding is reachable (or is an accepted known-dead entry)
//   1  at least one binding can never fire
//   2  the tool itself failed (bad arguments, unreadable file, ...)

#include "stdafx.h"

#include <KeyEvent.h>  // weasel::KeyEvent, ibus::Keycode, ibus::Modifier

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <KeyNameMapper.h>

namespace {

const char* const kToolVersion = "1.0";

// ---------------------------------------------------------------------------
// librime key tables (generated from librime/src/rime/key_table.cc)
// ---------------------------------------------------------------------------

// Canonical name for a keysym, for diagnostics. The librime key tables and
// their parse rules live in the shared unit now (ADR 0011 D1), so the gate
// reads them through it instead of keeping a second copy that could drift.
std::string KeyName(int keycode) {
  return fluxing::KeyName(keycode);
}

std::string Hex(int v) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(v) & 0xFFFFu);
  return std::string(buf);
}

// ---------------------------------------------------------------------------
// librime KeyEvent::Parse -- librime/src/rime/key_event.cc:51
// ---------------------------------------------------------------------------

struct ParsedKey {
  bool ok = false;
  int keycode = 0;
  int modifier = 0;
  std::string error;
};

ParsedKey ParseRimeKey(const std::string& repr) {
  // librime's own parse rules live in the shared unit (ADR 0011 D1).
  const fluxing::AcceptKey parsed = fluxing::ParseAccept(repr);
  ParsedKey out;
  out.ok = parsed.ok;
  out.keycode = parsed.keycode;
  out.modifier = parsed.modifier;
  out.error = parsed.error;
  return out;
}

// ---------------------------------------------------------------------------
// ibus mask -> librime mask -- RimeWithWeasel.cpp:43 expand_ibus_modifier()
// Copied verbatim; the self-check below pins the well-known values.
// ---------------------------------------------------------------------------

int ExpandIbusModifier(int m) {
  return (m & 0xff) | ((m & 0xff00) << 16);
}

// ---------------------------------------------------------------------------
// The set of (keycode, librime-modifier) pairs the TSF front end can send.
// ---------------------------------------------------------------------------

typedef std::pair<int, int> KeyPair;

// Everything the enumeration learned about the front end.
struct FrontEndMap {
  std::set<KeyPair> reachable;                   // (keycode, librime modifier)
  std::map<int, int> base_to_vkey;               // unshifted keycode -> vkey
  std::map<std::pair<int, int>, int> vkey_mask;  // (vkey, ibus mask) -> keycode
};

FrontEndMap BuildReachable() {
  FrontEndMap map;
  BYTE state[256];
  for (int vkey = 0x08; vkey <= 0xFE; ++vkey) {
    for (int extended = 0; extended <= 1; ++extended) {
      for (int shift = 0; shift <= 1; ++shift) {
        for (int ctrl = 0; ctrl <= 1; ++ctrl) {
          for (int alt = 0; alt <= 1; ++alt) {
            for (int keyup = 0; keyup <= 1; ++keyup) {
              std::memset(state, 0, sizeof(state));
              state[vkey] = 0x80;
              if (shift) {
                state[VK_SHIFT] = 0x80;
                state[VK_LSHIFT] = 0x80;
                state[VK_RSHIFT] = 0x80;
              }
              if (ctrl) {
                state[VK_CONTROL] = 0x80;
                state[VK_LCONTROL] = 0x80;
                state[VK_RCONTROL] = 0x80;
              }
              if (alt) {
                state[VK_MENU] = 0x80;
                state[VK_LMENU] = 0x80;
                state[VK_RMENU] = 0x80;
              }
              LPARAM lp = static_cast<LPARAM>(1) |
                          (static_cast<LPARAM>(extended) << 24) |
                          (static_cast<LPARAM>(keyup) << 31);
              KeyInfo kinfo(lp);
              weasel::KeyEvent ev;
              if (!ConvertKeyEvent(static_cast<UINT>(vkey), kinfo, state, ev)) {
                continue;  // unknown key for the front end
              }
              const int keycode = static_cast<int>(ev.keycode);
              const int ibus_mask = static_cast<int>(ev.mask);
              if (!shift && !ctrl && !alt && !keyup) {
                if (!map.base_to_vkey.count(keycode))
                  map.base_to_vkey[keycode] = vkey;
              }
              map.vkey_mask[KeyPair(vkey, ibus_mask)] = keycode;
              // WeaselTSF/KeyEventSink.cpp:44 -- spec 077 eats Shift_L /
              // Shift_R so they never reach librime.
              if (keycode == ibus::Shift_L || keycode == ibus::Shift_R) {
                continue;
              }
              map.reachable.insert(
                  KeyPair(keycode, ExpandIbusModifier(ibus_mask)));
            }
          }
        }
      }
    }
  }
  return map;
}

// ---------------------------------------------------------------------------
// Minimal YAML reader for the key_binder bindings list.
// ---------------------------------------------------------------------------

std::string Trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
    ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' ||
                   s[e - 1] == '\n'))
    --e;
  return s.substr(b, e - b);
}

int IndentOf(const std::string& s) {
  int n = 0;
  while (n < static_cast<int>(s.size()) && s[n] == ' ')
    ++n;
  return n;
}

// Strip YAML quoting. Returns false when the value is not a quoted scalar.
bool Unquote(const std::string& raw, std::string* out) {
  std::string s = Trim(raw);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    std::string body = s.substr(1, s.size() - 2);
    std::string r;
    for (size_t i = 0; i < body.size(); ++i) {
      if (body[i] == '\\' && i + 1 < body.size()) {
        char n = body[++i];
        if (n == 'n')
          r += '\n';
        else if (n == 't')
          r += '\t';
        else
          r += n;
      } else {
        r += body[i];
      }
    }
    *out = r;
    return true;
  }
  if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
    std::string body = s.substr(1, s.size() - 2);
    std::string r;
    for (size_t i = 0; i < body.size(); ++i) {
      if (body[i] == '\'' && i + 1 < body.size() && body[i + 1] == '\'') {
        r += '\'';
        ++i;
      } else {
        r += body[i];
      }
    }
    *out = r;
    return true;
  }
  *out = s;
  return false;
}

// Split a flow mapping body "{a: 1, b: 2}" on top-level commas.
//
// A double quoted scalar may contain an escaped quote ("\""), so the quote
// state machine has to honour backslash escapes. Without that, the
// compiled form of
//     - {accept: "Shift+apostrophe", send: "\"", when: always}
// is split in the wrong place and the binding is mis-read -- which is
// exactly the shape librime's YAML writer produces for a `send: '"'`.
std::vector<std::string> SplitFlow(const std::string& body) {
  std::vector<std::string> parts;
  std::string cur;
  char quote = 0;
  for (size_t i = 0; i < body.size(); ++i) {
    char c = body[i];
    if (quote == '"' && c == '\\' && i + 1 < body.size()) {
      cur += c;
      cur += body[++i];
      continue;
    }
    if (quote) {
      cur += c;
      if (c == quote)
        quote = 0;
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      cur += c;
      continue;
    }
    if (c == ',') {
      parts.push_back(cur);
      cur.clear();
      continue;
    }
    cur += c;
  }
  if (!Trim(cur).empty())
    parts.push_back(cur);
  return parts;
}

struct BindingRow {
  std::string file;
  int line = 0;
  std::string when;
  std::string accept;
  std::string send;
  std::string
      action;  // toggle / select / set_option / unset_option / send_sequence
  std::string action_arg;
};

bool SplitKeyValue(const std::string& text,
                   std::string* key,
                   std::string* value) {
  std::string t = Trim(text);
  if (t.empty())
    return false;
  size_t colon = std::string::npos;
  char quote = 0;
  for (size_t i = 0; i < t.size(); ++i) {
    char c = t[i];
    if (quote == '"' && c == '\\' && i + 1 < t.size()) {
      ++i;  // skip the escaped character
      continue;
    }
    if (quote) {
      if (c == quote)
        quote = 0;
      continue;
    }
    if (c == '"' || c == '\'') {
      quote = c;
      continue;
    }
    if (c == ':') {
      colon = i;
      break;
    }
  }
  if (colon == std::string::npos)
    return false;
  *key = Trim(t.substr(0, colon));
  *value = Trim(t.substr(colon + 1));
  return true;
}

void ApplyPair(BindingRow* row,
               const std::string& raw_key,
               const std::string& raw_value) {
  std::string k, v;
  Unquote(raw_key, &k);
  Unquote(raw_value, &v);
  if (k == "when")
    row->when = v;
  else if (k == "accept")
    row->accept = v;
  else if (k == "send")
    row->send = v;
  else if (k == "send_sequence") {
    row->action = "send_sequence";
    row->action_arg = v;
  } else if (k == "toggle") {
    row->action = "toggle";
    row->action_arg = v;
  } else if (k == "set_option") {
    row->action = "set_option";
    row->action_arg = v;
  } else if (k == "unset_option") {
    row->action = "unset_option";
    row->action_arg = v;
  } else if (k == "select") {
    row->action = "select";
    row->action_arg = v;
  }
}

// Scan one file for key_binder binding lists. Handles both the nested form
//   key_binder:
//     bindings:
//       - { when: ..., accept: ... }
// and the slash-path patch form
//   patch:
//     key_binder/bindings/+:
//       - { when: ..., accept: ... }
// and block-style list items.
std::vector<BindingRow> ScanBindings(const std::string& path,
                                     const std::string& content) {
  std::vector<BindingRow> rows;
  std::vector<std::string> lines;
  {
    std::string cur;
    for (size_t i = 0; i <= content.size(); ++i) {
      if (i == content.size() || content[i] == '\n') {
        lines.push_back(cur);
        cur.clear();
      } else if (content[i] != '\r') {
        cur += content[i];
      }
    }
  }

  struct StackEntry {
    int indent;
    std::string key;
  };
  std::vector<StackEntry> stack;

  for (size_t i = 0; i < lines.size(); ++i) {
    std::string raw = lines[i];
    std::string stripped = Trim(raw);
    if (stripped.empty() || stripped[0] == '#')
      continue;
    if (stripped[0] == '-')
      continue;  // list item, not a key line
    int indent = IndentOf(raw);
    std::string key, value;
    if (!SplitKeyValue(stripped, &key, &value))
      continue;
    if (key.empty())
      continue;

    while (!stack.empty() && stack.back().indent >= indent)
      stack.pop_back();

    std::string path_here;
    for (size_t s = 0; s < stack.size(); ++s) {
      if (s)
        path_here += "/";
      path_here += stack[s].key;
    }

    // The list may be addressed as a slash path ("key_binder/bindings/+")
    // or as a nested key ("bindings:" under "key_binder:").
    std::string norm = key;
    if (norm.size() >= 2 && norm.compare(norm.size() - 2, 2, "/+") == 0) {
      norm = norm.substr(0, norm.size() - 2);
    }
    bool is_bindings = (norm == "bindings") ||
                       (norm.size() >= 9 &&
                        norm.compare(norm.size() - 9, 9, "/bindings") == 0);
    bool in_key_binder = (key.find("key_binder") != std::string::npos) ||
                         (path_here.find("key_binder") != std::string::npos);

    stack.push_back(StackEntry{indent, key});

    if (!is_bindings || !in_key_binder)
      continue;

    // Collect the list items that follow.
    for (size_t j = i + 1; j < lines.size(); ++j) {
      std::string item = Trim(lines[j]);
      if (item.empty() || item[0] == '#')
        continue;
      int item_indent = IndentOf(lines[j]);
      if (item_indent <= indent)
        break;  // next key at or above -> list ended
      if (item[0] != '-')
        break;  // not a list item

      BindingRow row;
      row.file = path;
      row.line = static_cast<int>(j) + 1;
      std::string rest = Trim(item.substr(1));
      if (!rest.empty() && rest[0] == '{') {
        std::string body = rest.substr(1);
        size_t close = body.rfind('}');
        if (close != std::string::npos)
          body = body.substr(0, close);
        for (const std::string& part : SplitFlow(body)) {
          std::string k, v;
          if (SplitKeyValue(part, &k, &v))
            ApplyPair(&row, k, v);
        }
      } else {
        std::string k, v;
        if (SplitKeyValue(rest, &k, &v))
          ApplyPair(&row, k, v);
        // continuation lines of the same block mapping
        for (size_t m = j + 1; m < lines.size(); ++m) {
          std::string cont = Trim(lines[m]);
          if (cont.empty() || cont[0] == '#')
            continue;
          if (IndentOf(lines[m]) <= item_indent)
            break;
          if (cont[0] == '-')
            break;
          std::string ck, cv;
          if (SplitKeyValue(cont, &ck, &cv))
            ApplyPair(&row, ck, cv);
        }
      }
      if (!row.accept.empty() || !row.when.empty())
        rows.push_back(row);
    }
  }
  return rows;
}

// ---------------------------------------------------------------------------
// librime's binding conditions -- key_binder.cc:33
// ---------------------------------------------------------------------------

bool KnownCondition(const std::string& when) {
  return when == "always" || when == "has_menu" || when == "composing" ||
         when == "paging" || when == "predicting";
}

// ---------------------------------------------------------------------------
// Baseline of already-known dead bindings.
// ---------------------------------------------------------------------------

typedef std::set<std::string> BaselineSet;

std::string BaselineKey(const BindingRow& row) {
  // " :: " cannot appear in a when condition or in a key name, and the
  // send value may contain a bare '|' -- so the separator has to be
  // something a key name cannot contain.
  return row.when + " :: " + row.accept;
}

BaselineSet LoadBaseline(const std::string& path) {
  BaselineSet out;
  std::ifstream f(path.c_str());
  if (!f.is_open())
    return out;
  std::string line;
  while (std::getline(f, line)) {
    std::string t = Trim(line);
    if (t.empty() || t[0] == '#')
      continue;
    size_t p = t.find(" :: ");
    if (p == std::string::npos)
      continue;
    std::string when = Trim(t.substr(0, p));
    std::string rest = t.substr(p + 4);
    size_t p2 = rest.find(" :: ");
    std::string accept =
        Trim(p2 == std::string::npos ? rest : rest.substr(0, p2));
    out.insert(when + " :: " + accept);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Expected verdicts -- the negative control of the gate itself.
// ---------------------------------------------------------------------------

std::string BaseName(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

typedef std::map<std::string, std::string> ExpectationMap;

ExpectationMap LoadExpectations(const std::string& path) {
  ExpectationMap out;
  std::ifstream f(path.c_str());
  if (!f.is_open())
    return out;
  std::string line;
  while (std::getline(f, line)) {
    std::string t = Trim(line);
    if (t.empty() || t[0] == '#')
      continue;
    size_t b1 = t.find('|');
    if (b1 == std::string::npos)
      continue;
    size_t b2 = t.find('|', b1 + 1);
    if (b2 == std::string::npos)
      continue;
    // key is "<file-name>|<accept>", value is the expected verdict
    out[Trim(t.substr(0, b2))] = Trim(t.substr(b2 + 1));
  }
  return out;
}

// ---------------------------------------------------------------------------
// Suggestions for a dead binding: which key names could fire instead.
// ---------------------------------------------------------------------------

std::string SuggestAlternatives(const std::set<KeyPair>& reachable,
                                int modifier,
                                const std::string& accept) {
  std::vector<std::string> names;
  for (const KeyPair& p : reachable) {
    if (p.second != modifier)
      continue;
    int kc = p.first;
    if (kc < 0x20 || kc > 0x7E)
      continue;  // printable ASCII only
    std::string name = KeyName(kc);
    if (name.empty())
      continue;
    // A single letter or digit is never a useful alternative for a
    // punctuation binding; keep the suggestions actionable.
    if (name.size() == 1)
      continue;
    // The accept pattern already mentions this key -> not a suggestion.
    if (accept.find(name) != std::string::npos)
      continue;
    names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  std::string out;
  for (size_t i = 0; i < names.size() && i < 12; ++i) {
    if (i)
      out += ", ";
    out += names[i];
  }
  if (names.size() > 12)
    out += ", ...";
  return out;
}

// What does the front end actually send when the user presses the
// physical key the accept pattern names? The TSF front end sends the
// *character*, not the key, so the answer is often a different keycode.
std::string DiagnosePhysicalKey(const FrontEndMap& map,
                                int want_keycode,
                                int want_modifier) {
  std::map<int, int>::const_iterator it = map.base_to_vkey.find(want_keycode);
  if (it == map.base_to_vkey.end())
    return std::string();
  // The low modifier bits (Shift / Lock / Control / Alt) have the same
  // values in the ibus and in the librime mask.
  const int ibus_mask = want_modifier & 0xFF;
  std::map<std::pair<int, int>, int>::const_iterator got =
      map.vkey_mask.find(KeyPair(it->second, ibus_mask));
  if (got == map.vkey_mask.end())
    return std::string();
  if (got->second == want_keycode)
    return std::string();
  return "the front end sends " + Hex(got->second) + " (" +
         KeyName(got->second) + ") for that chord";
}

// ---------------------------------------------------------------------------
// Keyboard layout handling
// ---------------------------------------------------------------------------

// Name of the keyboard layout that is active for the calling thread.
// GetKeyboardLayoutNameW writes at most KL_NAMELENGTH (8) wchar_t.
std::string LayoutName() {
  wchar_t buf[KL_NAMELENGTH];
  buf[0] = 0;
  GetKeyboardLayoutNameW(buf);
  char narrow[64];
  narrow[0] = 0;
  WideCharToMultiByte(CP_UTF8, 0, buf, -1, narrow, sizeof(narrow), NULL, NULL);
  return std::string(narrow);
}

bool ActivateLayout(const std::string& hex_lcid,
                    std::string* name,
                    std::string* detail) {
  HKL hkl = LoadKeyboardLayoutA(hex_lcid.c_str(), KLF_ACTIVATE);
  if (!hkl) {
    *detail = "LoadKeyboardLayoutA(" + hex_lcid + ") failed";
    return false;
  }
  *name = LayoutName();
  return true;
}

// ---------------------------------------------------------------------------

struct Counts {
  int total = 0;
  int reachable = 0;
  int dead = 0;
  int known = 0;
  int mismatches = 0;
};

std::string BaselinePath;
std::string ExpectationsPath;
std::string LayoutSpec = "00000409";
bool UseLayout = true;
bool SelfCheckMapper = false;

// ---------------------------------------------------------------------------
// Mapper self-check -- ADR 0011 D1/D4
//
// The shared unit has to agree with three parties at the same time:
//   * librime's parse rules -- what an accept pattern means;
//   * librime's key table   -- which names exist at all;
//   * the front end         -- which (key code, modifier) pairs it sends.
// Any disagreement between them is how a binding gets minted that can never
// fire, so the gate checks all three here.
// ---------------------------------------------------------------------------

int RunMapperSelfCheck() {
  int failures = 0;
  int reported = 0;
  const int report_limit = 20;

  // 1. Every name in librime's own table parses back to its own key code.
  size_t name_count = 0;
  const fluxing::keysyms::NamedKey* names =
      fluxing::keysyms::KeyNames(&name_count);
  for (size_t i = 0; i < name_count; ++i) {
    const fluxing::AcceptKey parsed = fluxing::ParseAccept(names[i].name);
    if (!parsed.ok || parsed.keycode != names[i].keycode ||
        parsed.modifier != 0) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|name|%s|expected 0x%04X\n", names[i].name,
                    names[i].keycode);
      }
    }
  }
  std::printf("MAPPER|table names=%zu\n", name_count);

  // 2. The accept pattern built for a key code parses back to that key code.
  for (size_t i = 0; i < name_count; ++i) {
    const int keycode = names[i].keycode;
    std::string accept;
    fluxing::AcceptError error = fluxing::kAcceptOk;
    if (!fluxing::BuildAccept(keycode, 0, &accept, &error)) {
      // Refusing a modifier key is the contract, not a defect: such a chord
      // can never fire, so it must not be named as a binding (ADR 0011 D6).
      if (error != fluxing::kAcceptModifierKey ||
          !fluxing::IsModifierKeyCode(keycode)) {
        ++failures;
        if (reported++ < report_limit) {
          std::printf("MAPPER-FAIL|build|%s|%s\n", names[i].name,
                      fluxing::AcceptErrorText(error));
        }
      }
      continue;
    }
    const fluxing::AcceptKey parsed = fluxing::ParseAccept(accept);
    if (!parsed.ok || parsed.keycode != keycode || parsed.modifier != 0) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|roundtrip|%s|%s\n", accept.c_str(),
                    names[i].name);
      }
    }
  }

  // 3. The chords that do not depend on the keyboard layout.
  struct PinnedChord {
    UINT vkey;
    int keycode;
    const char* name;
  };
  const PinnedChord pinned[] = {
      {VK_BACK, ibus::BackSpace, "BackSpace"},
      {VK_TAB, ibus::Tab, "Tab"},
      {VK_RETURN, ibus::Return, "Return"},
      {VK_ESCAPE, ibus::Escape, "Escape"},
      {VK_SPACE, ibus::space, "space"},
      {VK_LEFT, ibus::Left, "Left"},
      {VK_UP, ibus::Up, "Up"},
      {VK_RIGHT, ibus::Right, "Right"},
      {VK_DOWN, ibus::Down, "Down"},
      {VK_HOME, ibus::Home, "Home"},
      {VK_END, ibus::End, "End"},
      {VK_PRIOR, ibus::Prior, "Prior"},
      {VK_NEXT, ibus::Next, "Next"},
      {VK_INSERT, ibus::Insert, "Insert"},
      {VK_DELETE, ibus::Delete, "Delete"},
      {VK_F1, ibus::F1, "F1"},
      {VK_F12, ibus::F12, "F12"},
      {VK_NUMPAD0, ibus::KP_0, "KP_0"},
  };
  for (size_t i = 0; i < sizeof(pinned) / sizeof(pinned[0]); ++i) {
    BYTE state[256];
    std::memset(state, 0, sizeof(state));
    state[pinned[i].vkey] = 0x80;
    KeyInfo kinfo(static_cast<LPARAM>(1));
    const int keycode =
        fluxing::ResolveCharacter(pinned[i].vkey, kinfo, state, NULL);
    if (keycode != pinned[i].keycode) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|chord|%s|expected 0x%04X|got 0x%04X\n",
                    pinned[i].name, pinned[i].keycode, keycode);
      }
      continue;
    }
    std::string accept;
    fluxing::AcceptError error = fluxing::kAcceptOk;
    if (!fluxing::BuildAccept(keycode, 0, &accept, &error)) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|chord-accept|%s|%s\n", pinned[i].name,
                    fluxing::AcceptErrorText(error));
      }
      continue;
    }
    const fluxing::AcceptKey parsed = fluxing::ParseAccept(accept);
    if (!parsed.ok || parsed.keycode != keycode) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|chord-accept|%s|%s\n", pinned[i].name,
                    accept.c_str());
      }
    }
  }

  // 4. librime's parse rules on the tricky patterns. A lower case letter
  //    only has a name in the one character form, so Control+a does not
  //    parse at all (ADR 0011 F1).
  struct ParseCase {
    const char* repr;
    bool ok;
    int keycode;
    int modifier;
  };
  const ParseCase parse_cases[] = {
      {"a", true, 0x61, 0},
      {"A", true, 0x41, 0},
      {"0", true, 0x30, 0},
      {"+", true, 0x2B, 0},
      {"Control+a", false, 0, 0},
      {"Control+A", true, 0x41, 0x4},
      {"Shift+comma", true, 0x2C, 0x1},
      {"Control+Shift+numbersign", true, 0x23, 0x5},
      {"Control+Alt+Shift+F", true, 0x46, 0xD},
      {"Release+BackSpace", true, ibus::BackSpace, 0x40000000},
      {"Control+", false, 0, 0},
      {"", false, 0, 0},
      {"Control+nosuchkey", false, 0, 0},
      {"nosuchmodifier+a", false, 0, 0},
  };
  for (size_t i = 0; i < sizeof(parse_cases) / sizeof(parse_cases[0]); ++i) {
    const ParseCase& c = parse_cases[i];
    const fluxing::AcceptKey parsed = fluxing::ParseAccept(c.repr);
    const bool bad =
        parsed.ok != c.ok || (c.ok && (parsed.keycode != c.keycode ||
                                       parsed.modifier != c.modifier));
    if (bad) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|parse|%s|expected %s\n", c.repr,
                    c.ok ? "ok" : "fail");
      }
    }
  }

  // 5. Every chord the front end can produce must round trip: the accept
  //    pattern the mapper builds has to mean exactly the (key code,
  //    modifier) pair the front end sends, and that pair has to be
  //    reachable. A refusal is only allowed where librime really has no
  //    name for the character (ADR 0011 D3).
  const FrontEndMap frontend = BuildReachable();
  BYTE state[256];
  int chords = 0;
  int refused = 0;
  for (int vkey = 0x08; vkey <= 0xFE; ++vkey) {
    for (int extended = 0; extended <= 1; ++extended) {
      for (int shift = 0; shift <= 1; ++shift) {
        for (int ctrl = 0; ctrl <= 1; ++ctrl) {
          for (int alt = 0; alt <= 1; ++alt) {
            for (int keyup = 0; keyup <= 1; ++keyup) {
              std::memset(state, 0, sizeof(state));
              state[vkey] = 0x80;
              if (shift) {
                state[VK_SHIFT] = 0x80;
                state[VK_LSHIFT] = 0x80;
                state[VK_RSHIFT] = 0x80;
              }
              if (ctrl) {
                state[VK_CONTROL] = 0x80;
                state[VK_LCONTROL] = 0x80;
                state[VK_RCONTROL] = 0x80;
              }
              if (alt) {
                state[VK_MENU] = 0x80;
                state[VK_LMENU] = 0x80;
                state[VK_RMENU] = 0x80;
              }
              LPARAM lp = static_cast<LPARAM>(1) |
                          (static_cast<LPARAM>(extended) << 24) |
                          (static_cast<LPARAM>(keyup) << 31);
              KeyInfo kinfo(lp);
              weasel::KeyEvent ev;
              if (!ConvertKeyEvent(static_cast<UINT>(vkey), kinfo, state, ev)) {
                continue;
              }
              const int keycode = static_cast<int>(ev.keycode);
              const int modifier =
                  ExpandIbusModifier(static_cast<int>(ev.mask));
              // The front end eats the shift keys (spec 077).
              if (keycode == ibus::Shift_L || keycode == ibus::Shift_R) {
                continue;
              }
              ++chords;
              std::string accept;
              fluxing::AcceptError error = fluxing::kAcceptOk;
              if (!fluxing::BuildAccept(keycode, modifier, &accept, &error)) {
                ++refused;
                bool allowed = false;
                if (error == fluxing::kAcceptModifierKey) {
                  allowed = fluxing::IsModifierKeyCode(keycode);
                } else if (error == fluxing::kAcceptNoName) {
                  allowed = fluxing::KeyName(keycode).empty() && modifier != 0;
                } else if (error == fluxing::kAcceptUnprintable) {
                  allowed = fluxing::KeyName(keycode).empty() &&
                            modifier == 0 && (keycode < 0x20 || keycode > 0x7e);
                }
                if (!allowed) {
                  ++failures;
                  if (reported++ < report_limit) {
                    std::printf("MAPPER-FAIL|refusal|0x%04X|%s\n", keycode,
                                fluxing::AcceptErrorText(error));
                  }
                }
                continue;
              }
              const fluxing::AcceptKey parsed = fluxing::ParseAccept(accept);
              if (!parsed.ok || parsed.keycode != keycode ||
                  parsed.modifier != modifier) {
                ++failures;
                if (reported++ < report_limit) {
                  std::printf("MAPPER-FAIL|chord-accept|%s|0x%04X|0x%04X\n",
                              accept.c_str(), keycode, modifier);
                }
                continue;
              }
              if (frontend.reachable.find(KeyPair(keycode, modifier)) ==
                  frontend.reachable.end()) {
                ++failures;
                if (reported++ < report_limit) {
                  std::printf("MAPPER-FAIL|unreachable|%s|0x%04X\n",
                              accept.c_str(), keycode);
                }
              }
            }
          }
        }
      }
    }
  }
  std::printf("MAPPER|chords=%d|refused=%d\n", chords, refused);
  // 6. The capture path has to agree with the front end. Whatever the
  //    capture dialog writes down must mean exactly the (key code,
  //    modifier) pair the front end sends for the same chord, otherwise the
  //    dialog mints a binding that can never fire (ADR 0011 D3).
  const int capture_modifier_mask = fluxing::ModifierByName("Shift") |
                                    fluxing::ModifierByName("Control") |
                                    fluxing::ModifierByName("Alt");
  int captured = 0;
  int capture_refused = 0;
  int dead_key_skipped = 0;
  for (int vkey = 0x08; vkey <= 0xFE; ++vkey) {
    for (int extended = 0; extended <= 1; ++extended) {
      for (int shift = 0; shift <= 1; ++shift) {
        for (int ctrl = 0; ctrl <= 1; ++ctrl) {
          for (int alt = 0; alt <= 1; ++alt) {
            std::memset(state, 0, sizeof(state));
            state[vkey] = 0x80;
            if (shift) {
              state[VK_SHIFT] = 0x80;
              state[VK_LSHIFT] = 0x80;
              state[VK_RSHIFT] = 0x80;
            }
            if (ctrl) {
              state[VK_CONTROL] = 0x80;
              state[VK_LCONTROL] = 0x80;
              state[VK_RCONTROL] = 0x80;
            }
            if (alt) {
              state[VK_MENU] = 0x80;
              state[VK_LMENU] = 0x80;
              state[VK_RMENU] = 0x80;
            }
            LPARAM lp =
                static_cast<LPARAM>(1) | (static_cast<LPARAM>(extended) << 24);
            KeyInfo kinfo(lp);
            // Resolve the same chord twice: on a layout with a dead key the
            // answer changes between two consecutive calls, and the
            // comparison below would be meaningless there.
            weasel::KeyEvent first;
            weasel::KeyEvent second;
            const bool produced_first =
                ConvertKeyEvent(static_cast<UINT>(vkey), kinfo, state, first);
            const bool produced_second =
                ConvertKeyEvent(static_cast<UINT>(vkey), kinfo, state, second);
            if (produced_first != produced_second ||
                (produced_first && (first.keycode != second.keycode ||
                                    first.mask != second.mask))) {
              ++dead_key_skipped;
              continue;
            }
            const fluxing::CapturedAccept cap = fluxing::CaptureAccept(
                static_cast<UINT>(vkey), kinfo, state, NULL);
            if (!produced_first) {
              // The front end sends nothing for this key, so a binding on it
              // could never fire: the capture has to refuse it as well.
              if (cap.ok) {
                ++failures;
                if (reported++ < report_limit) {
                  std::printf("MAPPER-FAIL|capture-orphan|0x%04X|%ls\n",
                              cap.keycode, cap.pattern.c_str());
                }
              }
              continue;
            }
            const int front_keycode = static_cast<int>(first.keycode);
            const int front_modifier =
                ExpandIbusModifier(static_cast<int>(first.mask));
            if (!cap.ok) {
              ++capture_refused;
              // The Windows key is refused on purpose: the front end never
              // reports it (ADR 0011 D6).
              if (cap.error == fluxing::kAcceptWinKey)
                continue;
              // Every other refusal has to be justified: the front end's own
              // pair must be inexpressible as well.
              std::string front_accept;
              fluxing::AcceptError front_error = fluxing::kAcceptOk;
              if (fluxing::BuildAccept(front_keycode, front_modifier,
                                       &front_accept, &front_error)) {
                ++failures;
                if (reported++ < report_limit) {
                  std::printf("MAPPER-FAIL|capture-refusal|0x%04X|%s\n",
                              front_keycode,
                              fluxing::AcceptErrorText(cap.error));
                }
              }
              continue;
            }
            ++captured;
            if (cap.keycode != front_keycode) {
              ++failures;
              if (reported++ < report_limit) {
                std::printf("MAPPER-FAIL|capture-keycode|0x%04X|0x%04X\n",
                            cap.keycode, front_keycode);
              }
            }
            if (cap.modifier != (front_modifier & capture_modifier_mask)) {
              ++failures;
              if (reported++ < report_limit) {
                std::printf("MAPPER-FAIL|capture-modifier|0x%04X|0x%04X\n",
                            cap.modifier, front_modifier);
              }
            }
            const fluxing::AcceptKey parsed =
                fluxing::ParseAcceptWide(cap.pattern);
            if (!parsed.ok || parsed.keycode != cap.keycode ||
                parsed.modifier != cap.modifier) {
              ++failures;
              if (reported++ < report_limit) {
                std::printf("MAPPER-FAIL|capture-roundtrip|%ls|0x%04X\n",
                            cap.pattern.c_str(), cap.keycode);
              }
            }
          }
        }
      }
    }
  }

  // 7. The chords the product promises, pinned down.
  struct CaptureCase {
    UINT vkey;
    int shift;
    int ctrl;
    int win;
    bool ok;
    const wchar_t* pattern;
    fluxing::AcceptError error;
  };
  const CaptureCase capture_cases[] = {
      {'F', 1, 1, 0, true, L"Shift+Control+F", fluxing::kAcceptOk},
      {'F', 0, 1, 0, false, L"", fluxing::kAcceptNoName},
      {'F', 0, 0, 1, false, L"", fluxing::kAcceptWinKey},
      {'A', 0, 0, 0, true, L"a", fluxing::kAcceptOk},
      {'A', 1, 0, 0, true, L"Shift+A", fluxing::kAcceptOk},
      {VK_SPACE, 0, 0, 0, true, L"space", fluxing::kAcceptOk},
      {VK_RETURN, 0, 0, 0, true, L"Return", fluxing::kAcceptOk},
      {VK_F1, 0, 0, 0, true, L"F1", fluxing::kAcceptOk},
  };
  for (size_t i = 0; i < sizeof(capture_cases) / sizeof(capture_cases[0]);
       ++i) {
    const CaptureCase& c = capture_cases[i];
    std::memset(state, 0, sizeof(state));
    state[c.vkey] = 0x80;
    if (c.shift) {
      state[VK_SHIFT] = 0x80;
      state[VK_LSHIFT] = 0x80;
      state[VK_RSHIFT] = 0x80;
    }
    if (c.ctrl) {
      state[VK_CONTROL] = 0x80;
      state[VK_LCONTROL] = 0x80;
      state[VK_RCONTROL] = 0x80;
    }
    if (c.win) {
      state[VK_LWIN] = 0x80;
      state[VK_RWIN] = 0x80;
    }
    KeyInfo kinfo(static_cast<LPARAM>(1));
    const fluxing::CapturedAccept cap =
        fluxing::CaptureAccept(c.vkey, kinfo, state, NULL);
    if (cap.ok != c.ok || cap.error != c.error ||
        (c.ok && cap.pattern != c.pattern)) {
      ++failures;
      if (reported++ < report_limit) {
        std::printf("MAPPER-FAIL|capture-case|0x%02X|expected %ls|got %ls|%s\n",
                    c.vkey, c.ok ? c.pattern : L"(refused)",
                    cap.pattern.c_str(), fluxing::AcceptErrorText(cap.error));
      }
    }
  }
  std::printf("MAPPER|captured=%d|refused=%d|dead_key_skipped=%d\n", captured,
              capture_refused, dead_key_skipped);
  return failures;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<std::string> files;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--baseline") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "--baseline needs a file argument\n");
        return 2;
      }
      BaselinePath = argv[++i];
    } else if (arg == "--expectations") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "--expectations needs a file argument\n");
        return 2;
      }
      ExpectationsPath = argv[++i];
    } else if (arg == "--layout") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "--layout needs an argument\n");
        return 2;
      }
      LayoutSpec = argv[++i];
    } else if (arg == "--no-layout") {
      UseLayout = false;
    } else if (arg == "--selfcheck-mapper") {
      SelfCheckMapper = true;
    } else if (!arg.empty() && arg[0] == '-') {
      std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
      return 2;
    } else {
      files.push_back(arg);
    }
  }
  if (SelfCheckMapper) {
    // The mapper self-check stands on its own: it needs neither a
    // configuration file nor a keyboard layout.
    std::printf("HOTKEY-BINDINGS %s (mapper self-check)\n", kToolVersion);
    const int failures = RunMapperSelfCheck();
    std::printf("MAPPER-SUMMARY|failures=%d\n", failures);
    std::printf("RESULT|%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
  }

  if (files.empty()) {
    files.push_back("output\\data\\default.yaml");
    files.push_back("output\\data\\user-custom\\rime_ice.custom.yaml");
  }

  std::printf("HOTKEY-BINDINGS %s\n", kToolVersion);

  std::string layout_name = LayoutName();
  std::string layout_detail;
  if (UseLayout) {
    if (!ActivateLayout(LayoutSpec, &layout_name, &layout_detail)) {
      std::printf("LAYOUT|%s|UNAVAILABLE|%s\n", LayoutSpec.c_str(),
                  layout_detail.c_str());
    } else {
      std::printf("LAYOUT|%s|%s\n", LayoutSpec.c_str(), layout_name.c_str());
    }
  } else {
    std::printf("LAYOUT|current|%s\n", layout_name.c_str());
  }

  // --- self check: the mask conversion must match librime's constants ---
  struct MaskProbe {
    const char* name;
    int ibus;
    int librime;
  };
  const MaskProbe probes[] = {
      {"Shift", ibus::SHIFT_MASK, 0x1},
      {"Lock", ibus::LOCK_MASK, 0x2},
      {"Control", ibus::CONTROL_MASK, 0x4},
      {"Alt", ibus::ALT_MASK, 0x8},
      {"Super", ibus::SUPER_MASK, 0x4000000},
      {"Hyper", ibus::HYPER_MASK, 0x8000000},
      {"Meta", ibus::META_MASK, 0x10000000},
      {"Release", ibus::RELEASE_MASK, 0x40000000},
  };
  for (const MaskProbe& p : probes) {
    if (ExpandIbusModifier(p.ibus) != p.librime) {
      std::printf("FATAL|mask expansion mismatch for %s: got 0x%X want 0x%X\n",
                  p.name, ExpandIbusModifier(p.ibus), p.librime);
      return 2;
    }
  }
  // The generated tables must agree with librime's own constants.
  {
    if (fluxing::ModifierByName("Shift") != 0x1 ||
        fluxing::ModifierByName("Release") != 0x40000000) {
      std::printf(
          "FATAL|generated modifier table does not match librime constants\n");
      return 2;
    }
    if (fluxing::KeycodeByName("comma") != 0x2C) {
      std::printf(
          "FATAL|generated key table does not match librime constants\n");
      return 2;
    }
  }

  FrontEndMap frontend = BuildReachable();
  const std::set<KeyPair>& reachable = frontend.reachable;
  // Layout sanity probe: Shift + the apostrophe key must produce '"' (0x22)
  // under a US layout. When it does not, layout dependent verdicts are
  // unreliable and the tool says so.
  {
    BYTE state[256];
    std::memset(state, 0, sizeof(state));
    state[VK_OEM_7] = 0x80;
    state[VK_SHIFT] = 0x80;
    LPARAM lp = 1;
    KeyInfo kinfo(lp);
    weasel::KeyEvent ev;
    bool ok = ConvertKeyEvent(VK_OEM_7, kinfo, state, ev);
    if (!ok || ev.keycode != 0x22) {
      std::printf("LAYOUT-PROBE|Shift+quote produced %s (expected 0x22)",
                  ok ? Hex(static_cast<int>(ev.keycode)).c_str() : "nothing");
      std::printf(" -- results for printable keys are layout dependent\n");
    } else {
      std::printf("LAYOUT-PROBE|Shift+quote=0x22 OK\n");
    }
  }

  BaselineSet baseline;
  std::set<std::string> known_keys;
  if (!BaselinePath.empty()) {
    baseline = LoadBaseline(BaselinePath);
    std::printf("BASELINE|%zu known-dead entries from %s\n", baseline.size(),
                BaselinePath.c_str());
  }

  ExpectationMap expectations;
  std::set<std::string> used_expectations;
  if (!ExpectationsPath.empty()) {
    expectations = LoadExpectations(ExpectationsPath);
    std::printf("EXPECTATIONS|%zu pinned verdicts from %s\n",
                expectations.size(), ExpectationsPath.c_str());
    if (expectations.empty()) {
      std::printf("FATAL|no expectations read from %s\n",
                  ExpectationsPath.c_str());
      return 2;
    }
  }

  Counts counts;
  for (const std::string& file : files) {
    std::ifstream in(file.c_str(), std::ios::binary);
    if (!in.is_open()) {
      std::printf("FATAL|cannot read %s\n", file.c_str());
      return 2;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    std::vector<BindingRow> rows = ScanBindings(file, content);
    for (const BindingRow& row : rows) {
      ++counts.total;
      const char* verdict = "REACHABLE";
      std::string detail;

      if (row.when.empty()) {
        verdict = "DEAD_NO_WHEN";
        detail = "librime drops bindings without a 'when' condition";
      } else if (!KnownCondition(row.when)) {
        verdict = "DEAD_WHEN";
        detail = "unknown condition '" + row.when +
                 "' -- librime maps it to kNever and drops the binding";
      } else if (row.accept.empty()) {
        verdict = "DEAD_NO_ACCEPT";
        detail = "librime drops bindings without an 'accept' pattern";
      } else {
        ParsedKey accept = ParseRimeKey(row.accept);
        if (!accept.ok) {
          verdict = "DEAD_ACCEPT";
          detail = accept.error +
                   " -- librime logs 'invalid key binding' and drops it";
        } else {
          // librime evaluates send / send_sequence / toggle / set_option /
          // unset_option / select in that order and drops the binding when
          // the chosen one does not parse (key_binder.cc:186-222).
          if (!row.send.empty()) {
            ParsedKey send = ParseRimeKey(row.send);
            if (!send.ok) {
              verdict = "DEAD_SEND";
              detail = send.error +
                       " -- librime logs 'invalid key binding' and drops it";
            }
          } else if (row.action == "send_sequence") {
            // A sequence is a run of {braced} or single key events; every
            // element has to parse.
            std::string seq = row.action_arg;
            bool bad = false;
            std::string cur;
            std::vector<std::string> parts;
            for (size_t i = 0; i <= seq.size(); ++i) {
              if (i == seq.size() || seq[i] == '{') {
                if (!cur.empty()) {
                  parts.push_back(cur);
                  cur.clear();
                }
                if (i < seq.size()) {
                  size_t close = seq.find('}', i);
                  if (close == std::string::npos) {
                    bad = true;
                    break;
                  }
                  parts.push_back(seq.substr(i + 1, close - i - 1));
                  i = close;
                }
              } else if (seq[i] != '}') {
                cur += seq[i];
              }
            }
            for (const std::string& part : parts) {
              if (!ParseRimeKey(part).ok) {
                bad = true;
                break;
              }
            }
            if (bad) {
              verdict = "DEAD_SEND";
              detail = "send_sequence '" + seq + "' does not parse";
            }
          } else if (row.action.empty()) {
            verdict = "DEAD_NO_ACTION";
            detail =
                "no send / send_sequence / toggle / set_option / unset_option "
                "/ "
                "select -- librime logs 'invalid key binding' and drops it";
          }

          if (std::string(verdict) == "REACHABLE") {
            if (reachable.find(KeyPair(accept.keycode, accept.modifier)) ==
                reachable.end()) {
              verdict = "DEAD_KEYCODE";
              std::string physical = DiagnosePhysicalKey(
                  frontend, accept.keycode, accept.modifier);
              detail = "the front end never sends keycode " +
                       Hex(accept.keycode) + " (" + KeyName(accept.keycode) +
                       ") with modifier " + Hex(accept.modifier);
              if (!physical.empty())
                detail += "; " + physical;
              std::string alt =
                  SuggestAlternatives(reachable, accept.modifier, row.accept);
              if (!alt.empty())
                detail += "; keys that do fire with the same modifier: " + alt;
            }
          }
        }
      }

      bool known_dead = (std::string(verdict) != "REACHABLE") &&
                        baseline.count(BaselineKey(row)) > 0;
      const std::string raw_verdict = verdict;
      if (std::string(verdict) == "REACHABLE") {
        ++counts.reachable;
      } else if (known_dead) {
        ++counts.known;
        known_keys.insert(BaselineKey(row));
        verdict = "DEAD_KNOWN";
        detail += " (accepted in the baseline as a known defect)";
      } else {
        ++counts.dead;
      }

      // Negative control: when an expectation pins this binding, the
      // verdict has to match it exactly.
      if (!expectations.empty()) {
        const std::string exp_key = BaseName(row.file) + "|" + row.accept;
        std::map<std::string, std::string>::const_iterator e =
            expectations.find(exp_key);
        if (e != expectations.end()) {
          used_expectations.insert(exp_key);
          if (e->second != raw_verdict) {
            ++counts.mismatches;
            std::printf("EXPECT-MISMATCH|%s|%s|expected=%s|actual=%s\n",
                        row.file.c_str(), row.accept.c_str(), e->second.c_str(),
                        raw_verdict.c_str());
          }
        }
      }

      std::printf("BINDING|%s|%d|%s|%s|%s|%s|%s|%s\n", row.file.c_str(),
                  row.line, row.when.c_str(), row.accept.c_str(),
                  row.send.c_str(),
                  row.action.empty() ? "-" : row.action.c_str(), verdict,
                  detail.c_str());
    }
  }

  int unused = 0;
  if (!expectations.empty()) {
    for (std::map<std::string, std::string>::const_iterator e =
             expectations.begin();
         e != expectations.end(); ++e) {
      if (used_expectations.count(e->first))
        continue;
      ++unused;
      std::printf("EXPECT-UNUSED|%s\n", e->first.c_str());
    }
  }

  std::printf(
      "SUMMARY|bindings=%d|reachable=%d|dead=%d|known_dead=%d|known_dead_keys=%"
      "d|expectations=%d|mismatches=%d|unused=%d\n",
      counts.total, counts.reachable, counts.dead, counts.known,
      (int)known_keys.size(), (int)expectations.size(), counts.mismatches,
      unused);
  // With --expectations the expectation file *is* the verdict spec for
  // that run, so only a mismatch (or an expectation that never matched)
  // fails it. Without it, any dead binding fails the gate.
  const bool fail = expectations.empty()
                        ? (counts.dead > 0)
                        : (counts.mismatches > 0 || unused > 0);
  std::printf("RESULT|%s\n", fail ? "FAIL" : "PASS");
  return fail ? 1 : 0;
}
