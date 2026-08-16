// TestShiftSelectBinding.cpp
// spec 014 runtime test: validate the binding form chosen for the restored
// has_menu Shift_L/R select 2nd/3rd candidate binding.
//
// spec 077 update: the has_menu Shift+Shift_L/R send 2/3 bindings were
// removed from default.yaml:240-241 (T15) because the State-machine
// release-only select via IPC (T01-T12) replaces them. The yaml-side
// contract is now inverted — those bindings MUST NOT exist. The defensive
// guards (no bare Shift_L/R, ascii_composer noop, Shift+space toggle)
// remain because they protect against the L19 release-event collision.

// The binding form ccept: Shift+Shift_L (NOT bare Shift_L) is the spec
// 012 design that avoids the L19 release-event collision. This test pins
// the yaml-side contract that makes that form work:

//   1. [spec 077 inverted] has_menu Shift+Shift_L/R send 2/3 bindings
//      MUST NOT exist (replaced by IPC state machine).
//   2. No bare accept: Shift_L or accept: Shift_R binding exists in
//      key_binder (that form is the L19 cause of release-event collision).
//   3. ascii_composer.switch_key.Shift_L/R remain noop (so the
//      ascii_composer's hardcoded 500ms short-press toggle does not fire).
//   4. Shift+space toggles ascii_mode (L18 contract preserved).
//   7. NO Shift+l / Shift+r combination-key ascii_mode toggle (spec 012).
//   8. NO keycode=Shift_L/R with no modifier in key_binder (L19 guard).

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

  // F1 (spec 077 inverted) has_menu: Shift+Shift_L select 2nd candidate
  // bindings were REMOVED in T15. The State-machine release-only select via
  // IPC (T01-T12) replaces them. These NEGATIVE assertions guard against
  // accidental re-introduction of the binding that librime's key_event.h:64
  // could not match the release half of.
  check("F1-NEG: has_menu accept: Shift+Shift_L, send: 2 must NOT exist (spec 077 IPC take-over)",
        !Contains(content, "accept: Shift+Shift_L, send: 2"));

  check("F1-NEG: has_menu accept: Shift+Shift_R, send: 3 must NOT exist (spec 077 IPC take-over)",
        !Contains(content, "accept: Shift+Shift_R, send: 3"));

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

  // F5/F6 removed (spec 077): the Shift+Shift_L/R bindings no longer exist
  // in default.yaml (T15 removed them), so the ordering checks relative to
  // them are moot. Control+1/2 fallback bindings are still in place.

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
