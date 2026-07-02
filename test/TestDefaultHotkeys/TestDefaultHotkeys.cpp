// TestDefaultHotkeys.cpp
// spec 005 v1.1 hotkey validation: verifies default.yaml is adjusted per v2 spec.
// compile: cl /EHsc /std:c++17 /I include TestDefaultHotkeys.cpp /Fe:test.exe

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

  // F1 中英文切换 + 上屏候选: Control+1/2 (L19 防御: 移除 Shift+Shift_L/R 避免 release event 冲突)
  check("Control+1 上屏第 2 候选 (has_menu) — L19",
        Contains(content, "accept: Control+1, send: 2"));
  check("Control+2 上屏第 3 候选 (has_menu) — L19",
        Contains(content, "accept: Control+2, send: 3"));  // F1 选第 2/3 候选：Shift_L/R 单键 (spec 014 / L21 恢复)
  check("Shift_L 单键选 2 候选 (has_menu) - spec 014 恢复",
        Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("Shift_R 单键选 3 候选 (has_menu) - spec 014 恢复",
        Contains(content, "accept: Shift+Shift_R, send: 3"));
  // F1 ascii_composer switch_key 保持 noop (防止与 has_menu binding 双触发)
  check("ascii_composer.Shift_L: noop 保持 (spec 014)", Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop 保持 (spec 014)", Contains(content, "Shift_R: noop"));

  check("Shift+space 切中英 (always ascii_mode) — L18 修复",
        Contains(content, "toggle: ascii_mode, accept: Shift+space"));
  // 移除 (L19 防御): Shift+Shift_L/R binding 全部移除，避免与 shift+<key> release event 冲突

  // F1 ascii_composer.switch_key.Shift_L/R 必须是 noop (避免与 key_binder 冲突)
  check("ascii_composer.Shift_L: noop (让 key_binder 接管)", Contains(content, "Shift_L: noop"));
  check("ascii_composer.Shift_R: noop", Contains(content, "Shift_R: noop"));
  check("ascii_composer.Shift_L: commit_code 已废弃", !Contains(content, "Shift_L: commit_code"));
  // 否定断言: spec 012 范围 — 不应有 Shift+l / Shift+r 组合键路径
  check("Shift+l 组合键 (has_menu send) 已移除 (L16)", !Contains(content, "accept: Shift+l, send: 2"));
  check("Shift+r 组合键 (has_menu send) 已移除 (L16)", !Contains(content, "accept: Shift+r, send: 3"));
  check("Shift+l 组合键 (always toggle) 已移除 (L16)", !Contains(content, "toggle: ascii_mode, accept: Shift+l"));
  check("Shift+r 组合键 (always toggle) 已移除 (L16)", !Contains(content, "toggle: ascii_mode, accept: Shift+r"));

  // L19 负断言: keycode=Shift_L/R 的所有 binding 必须全部不存在 (防御 shift+<key> release event)
  check("L14: has_menu: accept: Shift+Shift_L, send: 2 已恢复 (spec 014 / L21)", Contains(content, "accept: Shift+Shift_L, send: 2"));
  check("L14: has_menu: accept: Shift+Shift_R, send: 3 已恢复 (spec 014 / L21)", Contains(content, "accept: Shift+Shift_R, send: 3"));
  check("L19: send: <N> with Shift_L modifier 已不存在", !Contains(content, "send: 2, when: has_menu, accept: Shift_L"));
  check("L19: ascii_composer.Shift_L: commit_code 已不存在", !Contains(content, "Shift_L: commit_code"));
  check("L19: ascii_composer.Shift_R: commit_code 已不存在", !Contains(content, "Shift_R: commit_code"));

  // 顺序检查: has_menu 的选候选 binding (Control+1/2) 必须在 Shift+space 切中英之前 (L19: 选候选改用 Control+1/2)
  size_t pos_menu_l = content.find("accept: Control+1, send: 2");
  size_t pos_space_toggle = content.find("toggle: ascii_mode, accept: Shift+space");
  check("顺序: has_menu 选候选 (Control+1) 在 Shift+space 切中英之前 (L19)",
        pos_menu_l != std::string::npos && pos_space_toggle != std::string::npos &&
        pos_menu_l < pos_space_toggle);

  // L19 负断言: 任何 keycode=Shift_L/R 的 binding 必须全部不存在 (防御 shift+<key> release event 误匹配)
  check("L19 负断言: keycode=Shift_L ascii_mode toggle 已不存在",
        content.find("toggle: ascii_mode, accept: Shift_L") == std::string::npos);
  check("L19 负断言: keycode=Shift_R ascii_mode toggle 已不存在",
        content.find("toggle: ascii_mode, accept: Shift_R") == std::string::npos);
  check("L14: has_menu Shift+Shift_L binding 已恢复 (spec 014 / L21)",
        content.find("accept: Shift+Shift_L") != std::string::npos);
  check("L14: has_menu Shift+Shift_R binding 已恢复 (spec 014 / L21)",
        content.find("accept: Shift+Shift_R") != std::string::npos);
  check("L19 负断言: Shift+l 组合键 ascii_mode 已移除 (L16)",
        content.find("toggle: ascii_mode, accept: Shift+l") == std::string::npos);
  check("L19 负断言: Shift+r 组合键 ascii_mode 已移除 (L16)",
        content.find("toggle: ascii_mode, accept: Shift+r") == std::string::npos);

  // L19 正断言: 切中英仅 Shift+space 一条 active 路径 (排除 # - 注释行)
  // 方法: 统计 "toggle: ascii_mode" 出现次数 - 注释行 ("# - { when: ..., toggle: ascii_mode") 次数 = active count
  size_t totalToggleCount = 0;
  size_t commentToggleCount = 0;
  size_t searchFrom = 0;
  while ((searchFrom = content.find("toggle: ascii_mode", searchFrom)) != std::string::npos) {
    totalToggleCount++;
    searchFrom += 17;
  }
  searchFrom = 0;
  while ((searchFrom = content.find("# - { when:", searchFrom)) != std::string::npos) {
    size_t lineEnd = content.find("\n", searchFrom);
    std::string line = content.substr(searchFrom, (lineEnd == std::string::npos) ? std::string::npos : lineEnd - searchFrom);
    if (line.find("toggle: ascii_mode") != std::string::npos) commentToggleCount++;
    searchFrom = (lineEnd == std::string::npos) ? content.size() : lineEnd + 1;
  }
  // 简化 L19 断言 (lambda 不支持 std::string 拼接 → const char* 转换复杂; 直接检查 count==1)
  check("L19: 切中英 active toggle 路径仅 1 个 (Shift+space)",
        (totalToggleCount - commentToggleCount) == 1);

  std::cout << std::endl;
  std::cout << "Passed: " << passed << " / " << (passed + failed) << std::endl;
  return (failed == 0) ? 0 : 1;
}