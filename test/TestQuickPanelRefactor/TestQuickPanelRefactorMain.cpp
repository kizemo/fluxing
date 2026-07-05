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
  // L48 防御性测试退出模式: ExitProcess(rc) bypasses atexit static
  // destructors. The FluxingD2DRenderer singleton destructor calls
  // ReleaseHwndRenderTarget() during process exit; if the D2D
  // factory has already finalized, Release triggers access violation.
  // link-probe tests have no GUI message loop, so OS-managed
  // shutdown ordering does not apply. ExitProcess is the documented
  // cure. spec 038+ 所有 FluxingComponents link-probe 测试都用此模式.
  if (rc == 0) {
    std::printf("\n=== 5/5 assertions passed ===\n");
    std::printf("\nTestQuickPanelRefactor: 1/1 PASS\n");
    std::fflush(stdout);
    ExitProcess(0);
  } else {
    std::printf("\n=== TestQuickPanelRefactor FAILED (%d) ===\n", rc);
    std::fflush(stdout);
    ExitProcess(rc);
  }
}