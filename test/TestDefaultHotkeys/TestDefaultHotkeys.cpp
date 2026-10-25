// TestDefaultHotkeys.cpp
// spec 005 v1.1 hotkey validation: verifies default.yaml is adjusted per v2
// spec. compile: cl /EHsc /std:c++17 /I include TestDefaultHotkeys.cpp
// /Fe:test.exe

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <system_error>

namespace fs = std::filesystem;

static std::string ReadFile(const char* path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "FAIL: cannot open " << path << std::endl;
    std::exit(1);
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

static bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

// ---------------------------------------------------------------------------
// L113 helpers. Everything below matches on ASCII tokens only, so the
// assertions never depend on how this source file's own encoding is
// interpreted by the compiler.
// ---------------------------------------------------------------------------

static bool TryReadFile(const std::string& path, std::string& out) {
  std::ifstream f(path);
  if (!f.is_open())
    return false;
  std::stringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

static bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// A YAML comment line starts with '#' after optional leading whitespace.
// Blank lines are not active either.
static bool IsActiveLine(const std::string& line) {
  size_t first = line.find_first_not_of(" \t");
  if (first == std::string::npos)
    return false;  // blank
  return line[first] != '#';
}

static bool HasNonAsciiByte(const std::string& s) {
  for (unsigned char c : s) {
    if (c >= 0x80)
      return true;
  }
  return false;
}

// Value of `send:` on a line (everything after "send:", trimmed).
// Empty string when the line carries no `send:` at all.
static std::string SendValueOf(const std::string& line) {
  size_t p = line.find("send:");
  if (p == std::string::npos)
    return std::string();
  std::string v = line.substr(p + 5);
  size_t b = v.find_first_not_of(" \t");
  if (b == std::string::npos)
    return std::string();
  size_t e = v.find_last_not_of(" \t\r");
  return v.substr(b, e - b + 1);
}

// Number of ACTIVE (non-comment) lines containing `token`.
static size_t CountActiveLinesWith(const std::string& text,
                                   const std::string& token) {
  size_t count = 0, pos = 0;
  while (pos <= text.size()) {
    size_t lineEnd = text.find('\n', pos);
    std::string line =
        text.substr(pos, (lineEnd == std::string::npos) ? std::string::npos
                                                        : lineEnd - pos);
    if (IsActiveLine(line) && line.find(token) != std::string::npos)
      count++;
    if (lineEnd == std::string::npos)
      break;
    pos = lineEnd + 1;
  }
  return count;
}

// Number of ACTIVE lines whose `send:` value carries a non-ASCII byte.
// This is the static precondition for librime's "invalid key binding".
static size_t CountActiveNonAsciiSendValues(const std::string& text) {
  size_t count = 0, pos = 0;
  while (pos <= text.size()) {
    size_t lineEnd = text.find('\n', pos);
    std::string line =
        text.substr(pos, (lineEnd == std::string::npos) ? std::string::npos
                                                        : lineEnd - pos);
    if (IsActiveLine(line)) {
      std::string sv = SendValueOf(line);
      if (!sv.empty() && HasNonAsciiByte(sv))
        count++;
    }
    if (lineEnd == std::string::npos)
      break;
    pos = lineEnd + 1;
  }
  return count;
}

// Directory part of `p`, with a trailing separator kept.
static std::string DirNameOf(const std::string& p) {
  size_t slash = p.find_last_of("/\\");
  if (slash == std::string::npos)
    return std::string();
  return p.substr(0, slash + 1);
}

// Directory part of `p`, without a trailing separator.
static std::string ParentDirOf(const std::string& p) {
  std::string s = p;
  while (!s.empty() && (s.back() == '/' || s.back() == '\\'))
    s.pop_back();
  size_t slash = s.find_last_of("/\\");
  if (slash == std::string::npos)
    return std::string();
  return s.substr(0, slash);
}

// All *.custom.yaml file names directly inside `dir`, sorted.
static std::vector<std::string> ListCustomYaml(const std::string& dir,
                                               bool& ok) {
  std::vector<std::string> out;
  ok = false;
  std::error_code ec;
  if (!fs::exists(dir, ec) || ec)
    return out;
  for (fs::directory_iterator
           it(dir, fs::directory_options::skip_permission_denied, ec),
       end;
       it != end; it.increment(ec)) {
    if (ec)
      break;
    std::string name = it->path().filename().string();
    if (EndsWith(name, ".custom.yaml"))
      out.push_back(name);
  }
  ok = true;
  std::sort(out.begin(), out.end());
  return out;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "output/data/default.yaml";
  std::string content = ReadFile(path);

  int passed = 0, failed = 0;
  auto check = [&](const char* desc, bool cond) {
    if (cond) {
      std::cout << "  PASS: " << desc << std::endl;
      passed++;
    } else {
      std::cout << "  FAIL: " << desc << std::endl;
      failed++;
    }
  };

  std::cout << "spec 005: default hotkeys rev2" << std::endl;

  // F1 翻页: , = Page_Down, . = Page_Up
  check("翻页上页 = comma (',')",
        Contains(content, "accept: comma, send: Page_Up"));
  check("翻页下页 = period ('.')",
        Contains(content, "accept: period, send: Page_Down"));

  // 旧 -/= 翻页必须已废弃 (注释或不在 active 段)
  // 注: 注释里保留 "," "." 是说明, 我们检查不在 has_menu active 段
  // 简化: 验证 "accept: minus, send: Page_Up" 不存在
  check("旧 - 翻页 (minus Page_Up) 已替换",
        !Contains(content, "accept: minus, send: Page_Up"));
  check("旧 = 翻页 (equal Page_Down) 已替换",
        !Contains(content, "accept: equal, send: Page_Down"));

  // F1 切换中英标点 / 简繁: Control+Shift+numbersign / dollar
  // ADR 0009 (L148): an accept: pattern with a modifier has to name the
  // CHARACTER the chord produces, not the base key. The number chords
  // asked for '9' / '0' while the TSF front end sends '(' / ')' => they
  // could never fire and were deleted from default.yaml; the reachable
  // twins below carry the function.
  check("切换中英标点 = Control+Shift+numbersign",
        Contains(content,
                 "toggle: ascii_punct, accept: Control+Shift+numbersign"));
  check("切换简繁 = Control+Shift+dollar",
        Contains(content,
                 "toggle: traditionalization, accept: Control+Shift+dollar"));
  check("死绑定 Control+Shift+9 已删除 (ADR 0009)",
        !Contains(content, "accept: Control+Shift+9"));
  check("死绑定 Control+Shift+0 已删除 (ADR 0009)",
        !Contains(content, "accept: Control+Shift+0"));

  // 旧 3/4 已替换
  check("旧 Control+Shift+3 已替换",
        !Contains(content, "toggle: ascii_punct, accept: Control+Shift+3 "));
  check("旧 Control+Shift+4 已替换",
        !Contains(content,
                  "toggle: traditionalization, accept: Control+Shift+4 "));

  // F1 中英文切换 + 上屏候选: Control+1/2 (L19 防御: 移除 Shift+Shift_L/R 避免
  // release event 冲突)
  check("Control+1 上屏第 2 候选 (has_menu) — L19",
        Contains(content, "accept: Control+1, send: 2"));
  check("Control+2 上屏第 3 候选 (has_menu) — L19",
        Contains(content, "accept: Control+2, send: 3"));
  // spec 077 inverted (T15 deleted lines 240-241): these bindings MUST NOT
  // exist. The state-machine release-only select via IPC (T01-T12) replaces
  // them. Control+1/2 above remains as fallback.
  check("spec 077: has_menu accept: Shift+Shift_L, send: 2 must NOT exist",
        !Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("spec 077: has_menu accept: Shift+Shift_R, send: 3 must NOT exist",
        !Contains(content, "accept: Shift+Shift_R, send: 3"));
  // F1 ascii_composer switch_key 保持 noop (防止与 has_menu binding 双触发)
  check("ascii_composer.Shift_L: noop 保持 (spec 014)",
        Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop 保持 (spec 014)",
        Contains(content, "Shift_R: noop"));

  check("Shift+space 切中英 (always ascii_mode) — L18 修复",
        Contains(content, "toggle: ascii_mode, accept: Shift+space"));
  // 移除 (L19 防御): Shift+Shift_L/R binding 全部移除，避免与 shift+<key>
  // release event 冲突

  // F1 ascii_composer.switch_key.Shift_L/R 必须是 noop (避免与 key_binder 冲突)
  check("ascii_composer.Shift_L: noop (让 key_binder 接管)",
        Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop", Contains(content, "Shift_R: noop"));
  check("ascii_composer.Shift_L: commit_code 已废弃",
        !Contains(content, "Shift_L: commit_code"));
  // 否定断言: spec 012 范围 — 不应有 Shift+l / Shift+r 组合键路径
  check("Shift+l 组合键 (has_menu send) 已移除 (L16)",
        !Contains(content, "accept: Shift+l, send: 2"));
  check("Shift+r 组合键 (has_menu send) 已移除 (L16)",
        !Contains(content, "accept: Shift+r, send: 3"));
  check("Shift+l 组合键 (always toggle) 已移除 (L16)",
        !Contains(content, "toggle: ascii_mode, accept: Shift+l"));
  check("Shift+r 组合键 (always toggle) 已移除 (L16)",
        !Contains(content, "toggle: ascii_mode, accept: Shift+r"));

  // L19 负断言: keycode=Shift_L/R 的所有 binding 必须全部不存在 (防御
  // shift+<key> release event) spec 077 inverted (replaces old L14 positive
  // assertions, deleted in T15).
  check("spec 077: has_menu: accept: Shift+Shift_L, send: 2 已不存在",
        !Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("spec 077: has_menu: accept: Shift+Shift_R, send: 3 已不存在",
        !Contains(content, "accept: Shift+Shift_R, send: 3"));
  check("L19: send: <N> with Shift_L modifier 已不存在",
        !Contains(content, "send: 2, when: has_menu, accept: Shift_L"));
  check("L19: ascii_composer.Shift_L: commit_code 已不存在",
        !Contains(content, "Shift_L: commit_code"));
  check("L19: ascii_composer.Shift_R: commit_code 已不存在",
        !Contains(content, "Shift_R: commit_code"));

  // 顺序检查: has_menu 的选候选 binding (Control+1/2) 必须在 Shift+space
  // 切中英之前 (L19: 选候选改用 Control+1/2)
  size_t pos_menu_l = content.find("accept: Control+1, send: 2");
  size_t pos_space_toggle =
      content.find("toggle: ascii_mode, accept: Shift+space");
  check("顺序: has_menu 选候选 (Control+1) 在 Shift+space 切中英之前 (L19)",
        pos_menu_l != std::string::npos &&
            pos_space_toggle != std::string::npos &&
            pos_menu_l < pos_space_toggle);

  // L19 负断言: 任何 keycode=Shift_L/R 的 binding 必须全部不存在 (防御
  // shift+<key> release event 误匹配)
  check(
      "L19 负断言: keycode=Shift_L ascii_mode toggle 已不存在",
      content.find("toggle: ascii_mode, accept: Shift_L") == std::string::npos);
  check(
      "L19 负断言: keycode=Shift_R ascii_mode toggle 已不存在",
      content.find("toggle: ascii_mode, accept: Shift_R") == std::string::npos);
  // spec 077 inverted (replaces old L14 positive, deleted in T15).
  check("spec 077: has_menu Shift+Shift_L binding 已不存在",
        content.find("accept: Shift+Shift_L") == std::string::npos);
  check("spec 077: has_menu Shift+Shift_R binding 已不存在",
        content.find("accept: Shift+Shift_R") == std::string::npos);
  check(
      "L19 负断言: Shift+l 组合键 ascii_mode 已移除 (L16)",
      content.find("toggle: ascii_mode, accept: Shift+l") == std::string::npos);
  check(
      "L19 负断言: Shift+r 组合键 ascii_mode 已移除 (L16)",
      content.find("toggle: ascii_mode, accept: Shift+r") == std::string::npos);

  // L19 正断言: 切中英仅 Shift+space 一条 active 路径 (排除 # - 注释行)
  // 方法: 统计 "toggle: ascii_mode" 出现次数 - 注释行 ("# - { when: ...,
  // toggle: ascii_mode") 次数 = active count
  size_t totalToggleCount = 0;
  size_t commentToggleCount = 0;
  size_t searchFrom = 0;
  while ((searchFrom = content.find("toggle: ascii_mode", searchFrom)) !=
         std::string::npos) {
    totalToggleCount++;
    searchFrom += 17;
  }
  searchFrom = 0;
  while ((searchFrom = content.find("# - { when:", searchFrom)) !=
         std::string::npos) {
    size_t lineEnd = content.find("\n", searchFrom);
    std::string line = content.substr(searchFrom, (lineEnd == std::string::npos)
                                                      ? std::string::npos
                                                      : lineEnd - searchFrom);
    if (line.find("toggle: ascii_mode") != std::string::npos)
      commentToggleCount++;
    searchFrom = (lineEnd == std::string::npos) ? content.size() : lineEnd + 1;
  }
  // 简化 L19 断言 (lambda 不支持 std::string 拼接 → const char* 转换复杂;
  // 直接检查 count==1)
  check("L19: 切中英 active toggle 路径仅 1 个 (Shift+space)",
        (totalToggleCount - commentToggleCount) == 1);

  // spec 077 T17: binding count must drop by exactly 2 after T15 deleted
  // the two has_menu Shift+Shift_L/R send 2/3 lines. Count active binding
  // entries (lines starting with "- { when:" outside of comment lines).
  size_t activeBindingCount = 0;
  size_t searchB = 0;
  while ((searchB = content.find("- { when:", searchB)) != std::string::npos) {
    // Reject comment-line bindings ("# - { when: ..." in commented blocks).
    size_t prevChar = (searchB > 0) ? searchB - 1 : std::string::npos;
    if (prevChar != std::string::npos && content[prevChar] != '#') {
      activeBindingCount++;
    }
    searchB += 10;
  }
  // spec 014 + L21 baseline had two extra has_menu Shift+Shift_L/R send
  // 2/3 bindings (activeBindingCount == 46); after T15 they are removed
  // (activeBindingCount == 44). ADR 0009 / L148 then removed the two
  // never-firing Control+Shift+9 / Control+Shift+0 duplicates
  // (activeBindingCount == 42). Pin the count so future yaml edits cannot
  // silently re-introduce or remove bindings.
  const size_t EXPECTED_BINDINGS_AFTER_ADR0009 = 42;
  check(
      "spec 077 T17 + ADR 0009: binding count == 42 (spec 014 baseline 46 - 2 "
      "- 2)",
      activeBindingCount == EXPECTED_BINDINGS_AFTER_ADR0009);

  // ==========================================================================
  // L113: guard the user override files that actually ship to the user.
  //
  // rime_ice.custom.yaml and its siblings are the files that override the
  // schema for the end user, yet until now NO assertion looked at them.
  // They used to carry four bindings whose `send:` value was a Chinese
  // full-width character, e.g.
  //
  //   - { when: always, accept: "Shift+comma", send: "<fullwidth char>" }
  //
  // librime's key_binder `send:` must be a KEY NAME, not a character. A
  // multi-byte UTF-8 character fails to parse in
  // librime/src/rime/key_event.cc ("parse error: unrecognized key"), so
  // librime/src/rime/gear/key_binder.cc drops the whole binding with
  // "invalid key binding #N, with invalid send pattern".
  //
  // Two consequences; the second is what users actually notice:
  //   1. the shortcut never worked at all;
  //   2. every single engine start-up printed 4 error lines.
  //
  // Why a STATIC assertion is the right guard: the symptom lives in the
  // engine log, which a unit test cannot read. So instead we assert the
  // static precondition that guarantees librime will NOT reject the
  // binding -- every ACTIVE `send:` value must be pure ASCII. A non-ASCII
  // `send:` value is precisely what makes librime emit "invalid key
  // binding".
  //
  // There is no way to KEEP such a binding working: librime 1.13 has no
  // native path for "Shift+punctuation -> Chinese symbol"
  // (gear/punctuator.cc ignores the Shift modifier entirely;
  // gear/ascii_composer.cc has no key->text mapping). Implementing it would
  // require patching librime/, which this project forbids (P3).
  // See .specify/memory/lessons-learned.md L113.
  // ==========================================================================
  {
    // data dir = <repo>/output/data/ ; custom dir =
    // <repo>/output/data/user-custom/
    std::string dataDir = DirNameOf(path);
    std::string customDir = dataDir + "user-custom";
    std::string repoRoot = ParentDirOf(ParentDirOf(dataDir));

    bool dirOk = false;
    std::vector<std::string> customFiles = ListCustomYaml(customDir, dirOk);
    check("L113: user-custom 目录存在且可枚举 (output/data/user-custom)",
          dirOk && !customFiles.empty());
    // install.nsi ships every *.custom.yaml in user-custom/ (3 files today).
    // Pin the count so a new override file cannot silently ship unchecked.
    check(
        "L113: user-custom 覆盖文件数量 == 3 (rime_ice / double_pinyin_sogou / "
        "radical_pinyin)",
        customFiles.size() == 3);

    // Cross-check the shipped file list against install.nsi: a custom yaml
    // that is not referenced by install.nsi never reaches the user, and a
    // File line pointing at a missing file breaks the installer build.
    std::string nsiPath = repoRoot + "output/install.nsi";
    std::string nsi;
    bool nsiOk = TryReadFile(nsiPath, nsi);
    check("L113: install.nsi 可读 (用于核对打包清单)", nsiOk);

    for (size_t i = 0; i < customFiles.size(); ++i) {
      const std::string& name = customFiles[i];
      std::string fpath = customDir + "/" + name;
      std::string text;
      bool ok = TryReadFile(fpath, text);
      std::string tag = "L113 " + name;

      check((tag + ": 可读").c_str(), ok);
      if (!ok)
        continue;

      // The core guard: no ACTIVE send: value may carry a non-ASCII byte,
      // otherwise librime drops that binding and logs "invalid key binding".
      size_t bad = CountActiveNonAsciiSendValues(text);
      check((tag +
             ": 无非 ASCII 的 send: 值 (librime 不得报 invalid key binding)")
                .c_str(),
            bad == 0);

      // The four removed bindings must stay removed. Their accept tokens are
      // pure ASCII, so these checks are encoding-independent.
      check((tag + ": 已移除 Shift+comma 的中文全角绑定").c_str(),
            CountActiveLinesWith(text, "Shift+comma") == 0);
      check((tag + ": 已移除 Shift+period 的中文全角绑定").c_str(),
            CountActiveLinesWith(text, "Shift+period") == 0);
      check((tag + ": 已移除 Shift+slash 的中文全角绑定").c_str(),
            CountActiveLinesWith(text, "Shift+slash") == 0);
      check((tag + ": 已移除 Shift+semicolon 的中文全角绑定").c_str(),
            CountActiveLinesWith(text, "Shift+semicolon") == 0);

      // The four ASCII bindings DO work and must be kept.
      check((tag + ": 保留 Shift+apostrophe 绑定").c_str(),
            CountActiveLinesWith(text, "accept: \"Shift+apostrophe\"") == 1);
      check((tag + ": 保留 Shift+backslash 绑定").c_str(),
            CountActiveLinesWith(text, "accept: \"Shift+backslash\"") == 1);
      check((tag + ": 保留 Shift+bracketleft 绑定").c_str(),
            CountActiveLinesWith(text, "accept: \"Shift+bracketleft\"") == 1);
      check((tag + ": 保留 Shift+bracketright 绑定").c_str(),
            CountActiveLinesWith(text, "accept: \"Shift+bracketright\"") == 1);

      // Exact binding inventory: 8 before L113, 4 after. Pins the file so a
      // future edit cannot silently add or drop a binding.
      check((tag + ": active 绑定数 == 4 (L113 前为 8)").c_str(),
            CountActiveLinesWith(text, "accept:") == 4);

      // Every override file in user-custom/ must be referenced by
      // install.nsi, otherwise it never reaches the user.
      if (nsiOk) {
        bool inNsi = Contains(nsi, "user-custom\\" + name) ||
                     Contains(nsi, "user-custom/" + name);
        check((tag + ": install.nsi 已打包该覆盖文件").c_str(), inNsi);
      }
    }
  }

  std::cout << std::endl;
  std::cout << "Passed: " << passed << " / " << (passed + failed) << std::endl;
  return (failed == 0) ? 0 : 1;
}
