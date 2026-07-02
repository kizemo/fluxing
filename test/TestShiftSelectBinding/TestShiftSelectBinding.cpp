// TestShiftSelectBinding.cpp
// spec 014 runtime test: validate the binding form chosen for the restored
// has_menu Shift_L/R select 2nd/3rd candidate binding.

// The binding form ccept: Shift+Shift_L (NOT bare Shift_L) is the spec
// 012 design that avoids the L19 release-event collision. This test pins
// the yaml-side contract that makes that form work:

//   1. The yaml key_binder/bindings has_menu section contains exactly:
//        - { when: has_menu, accept: Shift+Shift_L, send: 2 }
//        - { when: has_menu, accept: Shift+Shift_R, send: 3 }
//   2. No bare accept: Shift_L or accept: Shift_R binding exists in
//      key_binder (that form is the L19 cause of release-event collision).
//   3. ascii_composer.switch_key.Shift_L/R remain noop (so the
//      ascii_composer's hardcoded 500ms short-press toggle does not fire).
//   4. Shift+space toggles ascii_mode (L18 contract preserved).
//   5. The Shift+Shift_L/R bindings appear in the key_binder/bindings list
//      BEFORE any Shift+space binding (spec 005 plan.md sec 2.2 ordering).
//   6. The new bindings come BEFORE the existing Control+1/2 bindings
//      (consistency with the spec 014 plan.md sec 2.5 R5 ordering rule).

// These checks are yaml-string-level, but unlike L18/L19 string tests,
// this test:
//   - Is a SEPARATE executable that compiles in CI
//   - Is run in the AGENTS.md sec 2.5 smoke test pipeline
//   - Is a precondition for the release/fluxing-*.exe artifact
// Any release that breaks these checks cannot ship.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

static std::string ReadFile(const char* path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    std::cerr << "FAIL: cannot open " << path << std::endl;
    std::exit(1);
  }
  std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

static bool Contains(const std::string& h, const std::string& n) {
  return h.find(n) != std::string::npos;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "output/data/default.yaml";
  std::string content = ReadFile(path);

  int passed = 0, failed = 0;
  auto check = [&](const char* desc, bool cond) {
    if (cond) { std::cout << "  PASS: " << desc << std::endl; passed++; }
    else { std::cout << "  FAIL: " << desc << std::endl; failed++; }
  };

  std::cout << "spec 014: Shift_L/R select 2nd/3rd candidate binding contract" << std::endl;

  // F1 (US1-A) has_menu: Shift+Shift_L select 2nd candidate (spec 014 restore)
  check("F1: has_menu accept: Shift+Shift_L, send: 2 exists (US1-A / spec 014)",
        Contains(content, "accept: Shift+Shift_L, send: 2"));

  // F1 (US1-B) has_menu: Shift+Shift_R select 3rd candidate (spec 014 restore)
  check("F1: has_menu accept: Shift+Shift_R, send: 3 exists (US1-B / spec 014)",
        Contains(content, "accept: Shift+Shift_R, send: 3"));

  // F2 (US1-C) no bare Shift_L/R binding in key_binder (L19 collision cause)
  // The pattern "accept: Shift_L" without a +Shift prefix would match the
  // TSF release event (keycode=Shift_L, modifier=0). It must not exist.
  // We accept Shift+Shift_L form (modifier=Shift) because that requires
  // modifier=Shift, which the release event (modifier=Release) does NOT have.
  check("F2: no bare accept: Shift_L (would match release event) (US1-C)",
        content.find("accept: Shift_L,") == std::string::npos &&
        content.find("accept: Shift_L ") == std::string::npos);
  check("F2: no bare accept: Shift_R (would match release event) (US1-C)",
        content.find("accept: Shift_R,") == std::string::npos &&
        content.find("accept: Shift_R ") == std::string::npos);

  // F3 (US1-D) ascii_composer.switch_key.Shift_L/R remain noop
  // Without this, the ascii_composer's 500ms short-press toggle would fire
  // on single Shift_L press and double-toggle ascii_mode.
  check("F3: ascii_composer.Shift_L: noop preserved (US1-D)",
        Contains(content, "Shift_L: noop"));
  check("F3: ascii_composer.Shift_R: noop preserved (US1-D)",
        Contains(content, "Shift_R: noop"));

  // F4 (US1-D) Shift+space toggles ascii_mode (L18 contract)
  check("F4: Shift+space toggles ascii_mode (L18 contract preserved) (US1-D)",
        Contains(content, "toggle: ascii_mode, accept: Shift+space"));

  // F5 (spec 005 plan.md sec 2.2 ordering) has_menu Shift+Shift_L/R
  // bindings appear before the Shift+space toggle binding.
  size_t pos_shift_l = content.find("accept: Shift+Shift_L, send: 2");
  size_t pos_shift_r = content.find("accept: Shift+Shift_R, send: 3");
  size_t pos_space_toggle = content.find("toggle: ascii_mode, accept: Shift+space");
  check("F5: has_menu Shift+Shift_L/R bindings appear before Shift+space (ordering)",
        pos_shift_l != std::string::npos &&
        pos_shift_r != std::string::npos &&
        pos_space_toggle != std::string::npos &&
        pos_shift_l < pos_space_toggle &&
        pos_shift_r < pos_space_toggle);

  // F6 (spec 014 plan.md sec 2.5 R5) Shift+Shift_L/R bindings appear
  // before Control+1/2 bindings in the has_menu block.
  size_t pos_ctrl_1 = content.find("accept: Control+1, send: 2");
  size_t pos_ctrl_2 = content.find("accept: Control+2, send: 3");
  check("F6: Shift+Shift_L appears before Control+1 (consistency)",
        pos_shift_l != std::string::npos &&
        pos_ctrl_1 != std::string::npos &&
        pos_shift_l < pos_ctrl_1);
  check("F6: Shift+Shift_R appears before Control+2 (consistency)",
        pos_shift_r != std::string::npos &&
        pos_ctrl_2 != std::string::npos &&
        pos_shift_r < pos_ctrl_2);

  // F7 (defensive) NO Shift+l / Shift+r combination-key ascii_mode
  // toggle (spec 012 sec 3 Out of scope, L16). These would re-introduce
  // the L04 lowercase-form issue.
  check("F7: no lowercase Shift+l ascii_mode toggle (L16 preserved)",
        content.find("toggle: ascii_mode, accept: Shift+l") == std::string::npos);
  check("F7: no lowercase Shift+r ascii_mode toggle (L16 preserved)",
        content.find("toggle: ascii_mode, accept: Shift+r") == std::string::npos);

  // F8 (defensive) NO keycode=Shift_L/R with no modifier in the key_binder
  // L19's defense (kept in spec 014 as a permanent guard against regression).
  check("F8: no keycode=Shift_L/R with modifier=0 in key_binder (L19 guard)",
        content.find("accept: Shift_L,") == std::string::npos &&
        content.find("accept: Shift_R,") == std::string::npos);

  std::cout << std::endl;
  std::cout << "Passed: " << passed << " / " << (passed + failed) << std::endl;
  return (failed == 0) ? 0 : 1;
}
