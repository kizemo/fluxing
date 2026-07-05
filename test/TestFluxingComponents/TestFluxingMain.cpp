// TestFluxingMain.cpp - spec 037 T016/T019 (2026-07-05)
//
// Test runner entry point. Each individual test file
// (TestFluxingButton, TestFluxingToggle, TestFluxingPanel,
// TestFluxingTheme) exports a RunTest() function with the
// C signature `int RunTest(void)` returning 0 on success
// and non-zero on failure. The main() function below calls
// each RunTest() in order, accumulating pass/fail counts.

#include "stdafx.h"
#include <cstdio>

namespace fluxing_test {
int RunButtonTest();
int RunToggleTest();
int RunPanelTest();
int RunThemeTest();
}

int main() {
  int total_fail = 0;
  int pass = 0;
  int fail = 0;
  struct { const char* name; int (*fn)(); } tests[] = {
    {"TestFluxingButton", fluxing_test::RunButtonTest},
    {"TestFluxingToggle", fluxing_test::RunToggleTest},
    {"TestFluxingPanel",  fluxing_test::RunPanelTest},
    {"TestFluxingTheme",  fluxing_test::RunThemeTest},
  };
  for (const auto& t : tests) {
    int rc = t.fn();
    if (rc == 0) ++pass;
    else { ++fail; ++total_fail; }
  }
  std::printf("\nTestFluxingComponents: %d/%d PASS\n", pass, pass + fail);
  return total_fail;
}