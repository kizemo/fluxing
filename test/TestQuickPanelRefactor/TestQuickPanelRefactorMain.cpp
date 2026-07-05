// TestQuickPanelRefactorMain.cpp - spec 038 T008 (2026-07-05)
//
// Test runner entry point. TestQuickPanelRefactor.cpp exports
// int RunRefactorTest() that returns 0 on success. The main()
// function below calls RunRefactorTest() in turn. There is
// only one test suite per L24 link-probe pattern (test the
// production control wiring, not a per-assertion suite).

#include "stdafx.h"
#include <cstdio>

namespace fluxing_test {
int RunRefactorTest();
}

int main() {
  int total_fail = 0;
  std::printf("=== TestQuickPanelRefactor ===\n");
  int rc = fluxing_test::RunRefactorTest();
  if (rc == 0) {
    std::printf("\n=== 5/5 assertions passed ===\n");
    std::printf("\nTestQuickPanelRefactor: 1/1 PASS\n");
  } else {
    total_fail = rc;
    std::printf("\n=== TestQuickPanelRefactor FAILED (%d) ===\n", rc);
  }
  return total_fail;
}