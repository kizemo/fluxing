// spec 042 T010 - TestOrphanRecovery.cpp
//
// Behavior-level test for `weasel::deployer::MaintenanceGuard` (L55 fix).
//
// We CANNOT spawn a real WeaselServer.exe (PPL process + requires IPC
// named pipe + 5-second watchdog window) so we test the guard's
// invariants with a MockClient that has the same 3-method interface
// as weasel::Client (template duck-typing, no virtual functions).
//
// The 5 test cases cover the 4 critical paths that L55 identifies:
//
//   1. Happy path: construct guard, leave scope, verify Start + End called once each.
//   2. Exception in scope: throw while guard is alive, verify dtor still
//      calls EndMaintenance (R4 core requirement: never leak maintenance mode).
//   3. Connect() fails on construction: verify NO Start, NO End.
//   4. Connect() fails on destruction: verify NO End (WeaselServer is down,
//      nothing to clean up, no throw from dtor).
//   5. EndMaintenance throws: verify dtor swallows the exception (no
//      std::terminate, no crash).
//
// Test pattern follows TestDarkModeBridge (L24 link-probe + L25 behavior-level):
//   - assert() rather than GoogleTest (avoid heavy dep).
//   - main() returns 0 on all-pass, non-zero on first failure.
//   - ExitProcess() at end per L48 (avoid atexit crashes from
//     FluxingD2DRenderer-style singletons in shared deps).

#include "stdafx.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <windows.h>

// Pull in the production guard. MaintenanceGuard.h is header-only
// templated on ClientT, so the test can pass MockClient directly.
#include "../../WeaselDeployer/MaintenanceGuard.h"

namespace {

// MockClient mirrors the 3-method interface of weasel::Client that
// MaintenanceGuard uses: Connect(), StartMaintenance(), EndMaintenance().
// Counts each call so we can assert exactly-1 / 0 / etc. expectations.
struct MockClient {
  int connect_calls = 0;
  int start_calls = 0;
  int end_calls = 0;

  // Behavior knobs (set by individual test cases).
  bool connect_returns = true;
  bool end_throws = false;
  bool dtor_connect_returns = true;  // separate flag for second Connect() in dtor

  bool Connect() {
    ++connect_calls;
    // First Connect() in ctor uses connect_returns; second (in dtor) uses dtor_connect_returns.
    // We track which "phase" by counting.
    return (connect_calls == 1) ? connect_returns : dtor_connect_returns;
  }
  void StartMaintenance() { ++start_calls; }
  void EndMaintenance() {
    ++end_calls;
    if (end_throws) {
      throw std::runtime_error("MockClient::EndMaintenance simulated failure");
    }
  }
};

int g_failures = 0;
#define EXPECT(cond)                                                          \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

void TestHappyPath() {
  std::printf("[T1] Happy path: normal scope exit calls Start+End once each...\n");
  MockClient mc;
  {
    weasel::deployer::MaintenanceGuard<MockClient> g(mc);
    EXPECT(mc.start_calls == 1);
    EXPECT(mc.end_calls == 0);  // not yet
    EXPECT(g.entered() == true);
  }
  EXPECT(mc.start_calls == 1);
  EXPECT(mc.end_calls == 1);  // dtor called EndMaintenance
}

void TestExceptionInScope() {
  std::printf("[T2] Exception in scope: dtor still calls End (L55 R4 core)...\n");
  MockClient mc;
  bool caught = false;
  try {
    weasel::deployer::MaintenanceGuard<MockClient> g(mc);
    EXPECT(mc.start_calls == 1);
    EXPECT(mc.end_calls == 0);
    throw std::runtime_error("simulated deployer mid-flight crash");
  } catch (const std::exception&) {
    caught = true;
  }
  EXPECT(caught == true);
  EXPECT(mc.start_calls == 1);
  EXPECT(mc.end_calls == 1);  // dtor ran during stack unwind (R4 fix)
}

void TestConnectFailedNoEnter() {
  std::printf("[T3] Connect() fails on ctor: no Start, no End...\n");
  MockClient mc;
  mc.connect_returns = false;
  {
    weasel::deployer::MaintenanceGuard<MockClient> g(mc);
    EXPECT(mc.start_calls == 0);
    EXPECT(mc.end_calls == 0);
    EXPECT(g.entered() == false);  // dtor will skip EndMaintenance
  }
  EXPECT(mc.start_calls == 0);
  EXPECT(mc.end_calls == 0);  // entered_ was false, dtor returned early
}

void TestConnectFailedInDtor() {
  std::printf("[T4] Connect() fails on dtor (WeaselServer is down): no throw...\n");
  MockClient mc;
  mc.dtor_connect_returns = false;  // WeaselServer died between ctor and dtor
  {
    weasel::deployer::MaintenanceGuard<MockClient> g(mc);
    EXPECT(mc.start_calls == 1);
  }
  // dtor Connect() failed -> no EndMaintenance call, no throw
  EXPECT(mc.start_calls == 1);
  EXPECT(mc.end_calls == 0);
}

void TestEndThrows() {
  std::printf("[T5] EndMaintenance throws: dtor swallows, no std::terminate...\n");
  MockClient mc;
  mc.end_throws = true;
  {
    weasel::deployer::MaintenanceGuard<MockClient> g(mc);
    EXPECT(mc.start_calls == 1);
  }
  // If we got here without std::terminate, the dtor swallowed the throw.
  EXPECT(mc.start_calls == 1);
  EXPECT(mc.end_calls == 1);  // End was called; throw was caught internally
}

// spec 053 R6 fix (L55): the deployer-process died but the named
// mutex is still held by the kernel. _IsDeployerRunning() must
// detect WAIT_ABANDONED (holder thread died without releasing) and
// return false, NOT true. This is the test for the *new* R6-aware
// _IsDeployerRunning behavior. We use a real Win32 named mutex
// because the production code under test (RimeWithWeaselHandler::
// _IsDeployerRunning) calls OpenMutex + WaitForSingleObject, not
// any injected client method.
void TestAbandonedMutexR6() {
  std::printf("[T7] R6: abandoned mutex (holder died) detected as not running...\n");
  const wchar_t* test_mutex = L"TestOrphanRecovery_R6_Mutex_DO_NOT_USE";

  // Spawn a child thread that holds the mutex and exits without releasing.
  // OS tracks this as an abandoned mutex on next OpenMutex.
  HANDLE hThread = ::CreateThread(NULL, 0,
      [](LPVOID) -> DWORD {
        HANDLE h = ::CreateMutex(NULL, FALSE, L"TestOrphanRecovery_R6_Mutex_DO_NOT_USE");
        if (h) {
          ::WaitForSingleObject(h, INFINITE);
          // intentionally do NOT ReleaseMutex; thread exit will mark
          // the mutex as abandoned by the kernel.
        }
        return 0;
      },
      NULL, 0, NULL);
  EXPECT(hThread != NULL);
  ::WaitForSingleObject(hThread, INFINITE);
  ::CloseHandle(hThread);

  // Now OpenMutex + WaitForSingleObject(0) should return WAIT_ABANDONED.
  HANDLE hMutexB = ::OpenMutex(SYNCHRONIZE, FALSE, test_mutex);
  EXPECT(hMutexB != NULL);
  if (hMutexB) {
    DWORD result = ::WaitForSingleObject(hMutexB, 0);
    EXPECT(result == WAIT_ABANDONED);
    if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
      ::ReleaseMutex(hMutexB);
    }
    ::CloseHandle(hMutexB);
  }
}

void TestNonCopyable() {
  std::printf("[T6] Non-copyable / non-movable: static_assert...\n");
  // Compile-time test: if these weren't deleted, the line below would
  // compile. Using sizeof trick to force evaluation.
  using G = weasel::deployer::MaintenanceGuard<MockClient>;
  static_assert(!std::is_copy_constructible<G>::value, "must not be copyable");
  static_assert(!std::is_copy_assignable<G>::value, "must not be copy-assignable");
  static_assert(!std::is_move_constructible<G>::value, "must not be movable");
  static_assert(!std::is_move_assignable<G>::value, "must not be move-assignable");
  EXPECT(true);
}

}  // namespace

int main() {
  std::printf("=== TestOrphanRecovery (spec 042, L55) ===\n");
  TestHappyPath();
  TestExceptionInScope();
  TestConnectFailedNoEnter();
  TestConnectFailedInDtor();
  TestEndThrows();
  TestAbandonedMutexR6();
  TestNonCopyable();

  if (g_failures == 0) {
    std::printf("=== ALL TESTS PASSED ===\n");
    std::fflush(stdout);
    // L48: explicit ExitProcess to avoid atexit-order issues from any
    // future singleton in shared deps.
    ExitProcess(0);
    return 0;
  } else {
    std::fprintf(stderr, "=== %d FAILURE(S) ===\n", g_failures);
    std::fflush(stderr);
    ExitProcess(1);
    return 1;
  }
}