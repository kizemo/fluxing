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
        Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("Shift_R 上屏第 3 候选 (has_menu)",
        Contains(content, "accept: Shift+Shift_R, send: 3"));
  check("Shift+space 切中英 (always ascii_mode) — L18 修复",
        Contains(content, "toggle: ascii_mode, accept: Shift+space"));
  // 移除 (L18 修复): 单键 Shift_R 切中英与 shift+= 等 release 事件冲突

  // F1 中英切换: 单键 Shift_L / Shift_R 单独按 (搜狗拼音 习惯)
  check("Shift_L 单键 has_menu 上屏第 2 候选", Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("Shift_R 单键 has_menu 上屏第 3 候选", Contains(content, "accept: Shift+Shift_R, send: 3"));
  // 移除 (L18 修复): 合并到上面的 Shift+space 切中英断言
  // 移除 (L18 修复): 合并到上面的 Shift+space 切中英断言

  // F1 ascii_composer.switch_key.Shift_L/R 必须是 noop (避免与 key_binder 冲突)
  check("ascii_composer.Shift_L: noop (让 key_binder 接管)", Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop", Contains(content, "Shift_R: noop"));
  check("ascii_composer.Shift_L: commit_code 已废弃", !Contains(content, "Shift_L: commit_code"));
  // 否定断言: spec 012 范围 — 不应有 Shift+l / Shift+r 组合键路径
  check("Shift+l 组合键 (has_menu send) 已移除", !Contains(content, "accept: Shift+l, send: 2"));
  check("Shift+r 组合键 (has_menu send) 已移除", !Contains(content, "accept: Shift+r, send: 3"));
  check("Shift+l 组合键 (always toggle) 已移除", !Contains(content, "toggle: ascii_mode, accept: Shift+l"));
  check("Shift+r 组合键 (always toggle) 已移除", !Contains(content, "toggle: ascii_mode, accept: Shift+r"));

  // 顺序检查: has_menu 的选候选 binding 必须在 Shift+space 切中英之前 (L18 修复后切中英键位已迁移)
  size_t pos_menu_l = content.find("accept: Shift+Shift_L, send: 2");
  size_t pos_space_toggle = content.find("toggle: ascii_mode, accept: Shift+space");
  check("顺序: has_menu 选候选 (Shift+Shift_L) 在 Shift+space 切中英之前",
        pos_menu_l != std::string::npos && pos_space_toggle != std::string::npos &&
        pos_menu_l < pos_space_toggle);
  // 负断言: 单键 Shift 切中英已移除 (L18)
  check("负断言: Shift_L 单键 ascii_mode 已移除",
        content.find("toggle: ascii_mode, accept: Shift_L") == std::string::npos);
  check("负断言: Shift_R 单键 ascii_mode 已移除",
        content.find("toggle: ascii_mode, accept: Shift_R") == std::string::npos);
  check("负断言: Shift+l 组合键 ascii_mode 已移除 (L16 修复)",
        content.find("toggle: ascii_mode, accept: Shift+l") == std::string::npos);
  check("负断言: Shift+r 组合键 ascii_mode 已移除 (L16 修复)",
        content.find("toggle: ascii_mode, accept: Shift+r") == std::string::npos);

  std::cout << std::endl;
  std::cout << "Passed: " << passed << " / " << (passed + failed) << std::endl;
  return (failed == 0) ? 0 : 1;
}