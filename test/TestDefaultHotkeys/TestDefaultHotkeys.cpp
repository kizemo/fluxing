// TestDefaultHotkeys.cpp
// spec 005 验收: 验证 default.yaml 已按 v2 规范调整快捷键
// 编译: cl /EHsc /std:c++17 /I include TestDefaultHotkeys.cpp /Fe:test.exe

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cassert>
#include <cstdlib>

static std::string ReadFile(const char* path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "FAIL: cannot open " << path << std::endl;
    std::exit(1);
  }
  std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static bool Contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "output/data/default.yaml";
  std::string content = ReadFile(path);

  int passed = 0, failed = 0;
  auto check = [&](const char* desc, bool cond) {
    if (cond) { std::cout << "  PASS: " << desc << std::endl; passed++; }
    else { std::cout << "  FAIL: " << desc << std::endl; failed++; }
  };

  std::cout << "spec 005: default hotkeys rev2" << std::endl;

  // F1 翻页: , = Page_Down, . = Page_Up
  check("翻页上页 = comma (',')", Contains(content, "accept: comma, send: Page_Up"));
  check("翻页下页 = period ('.')", Contains(content, "accept: period, send: Page_Down"));

  // 旧 -/= 翻页必须已废弃 (注释或不在 active 段)
  // 注: 注释里保留 "," "." 是说明, 我们检查不在 has_menu active 段
  // 简化: 验证 "accept: minus, send: Page_Up" 不存在
  check("旧 - 翻页 (minus Page_Up) 已替换",
        !Contains(content, "accept: minus, send: Page_Up"));
  check("旧 = 翻页 (equal Page_Down) 已替换",
        !Contains(content, "accept: equal, send: Page_Down"));

  // F1 切换中英标点 / 简繁: Control+Shift+9 / 0
  check("切换中英标点 = Control+Shift+9",
        Contains(content, "toggle: ascii_punct, accept: Control+Shift+9"));
  check("切换简繁 = Control+Shift+0",
        Contains(content, "toggle: traditionalization, accept: Control+Shift+0"));

  // 旧 3/4 已替换
  check("旧 Control+Shift+3 已替换",
        !Contains(content, "toggle: ascii_punct, accept: Control+Shift+3 "));
  check("旧 Control+Shift+4 已替换",
        !Contains(content, "toggle: traditionalization, accept: Control+Shift+4 "));

  // F1 中英文切换 + 上屏候选: Shift_L/Shift_R
  check("Shift_L 上屏第 2 候选 (has_menu)",
        Contains(content, "accept: shift+l, send: 2"));
  check("Shift_R 上屏第 3 候选 (has_menu)",
        Contains(content, "accept: shift+r, send: 3"));
  check("Shift_L 中英切换 (always ascii_mode)",
        Contains(content, "toggle: ascii_mode, accept: shift+l"));
  check("Shift_R 中英切换 (always ascii_mode)",
        Contains(content, "toggle: ascii_mode, accept: shift+r"));

  // F1 中英切换: 单键 Shift_L / Shift_R 单独按 (搜狗拼音 习惯)
  check("Shift_L 单键 has_menu 上屏第 2 候选", Contains(content, "accept: Shift_L, send: 2"));
  check("Shift_R 单键 has_menu 上屏第 3 候选", Contains(content, "accept: Shift_R, send: 3"));
  check("Shift_L 单键 always 切中英", Contains(content, "toggle: ascii_mode, accept: Shift_L"));
  check("Shift_R 单键 always 切中英", Contains(content, "toggle: ascii_mode, accept: Shift_R"));

  // F1 ascii_composer.switch_key.Shift_L/R 必须是 noop (避免与 key_binder 冲突)
  check("ascii_composer.Shift_L: noop (让 key_binder 接管)", Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop", Contains(content, "Shift_R: noop"));
  check("ascii_composer.Shift_L: commit_code 已废弃", !Contains(content, "Shift_L: commit_code"));

  // 顺序检查: has_menu 必须在 always 之前 (RIME 引擎按列表顺序 匹配)
  size_t pos_menu_l = content.find("accept: shift+l, send: 2");
  size_t pos_always_l = content.find("toggle: ascii_mode, accept: shift+l");
  check("Shift_L 顺序: has_menu 在 always 之前 (优先级正确)",
        pos_menu_l != std::string::npos && pos_always_l != std::string::npos &&
        pos_menu_l < pos_always_l);

  std::cout << std::endl;
  std::cout << "Passed: " << passed << " / " << (passed + failed) << std::endl;
  return (failed == 0) ? 0 : 1;
}