//
// TestPipeChannelRace.cpp — Regression test for spec 076
//
// Target: ensure PipeChannelBase's per-thread state initialization is
// race-free when N threads concurrently call _GetContext() / _GetPipeHandle()
// for the first time.
//
// v0.20.0.1 had boost::thread_specific_ptr with a TOCTOU pattern
// (if (!context.get()) context.reset(new ...)) that allowed two threads
// to both allocate + race-reset, leaving one thread holding a
// use-after-free pointer. The crash signature was 0xc0000374 in
// weaselx64!_free_base called from a thread-init lambda. This test
// reproduces the race deterministically without a real pipe server:
// it exercises only the per-thread state initialization hot path.
//
// Build: cl /EHsc /std:c++17 TestPipeChannelRace.cpp /link /OUT:TestPipeChannelRace.exe
//
// Run: TestPipeChannelRace.exe
//

#include <windows.h>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include <random>
#include <chrono>
#include <immintrin.h>
#include "PipeChannel.h"

// v0.20.0.4 fix: PipeChannelBase lives in weasel:: namespace (see
// include/PipeChannel.h line 8). Without `using namespace weasel;`,
// the unqualified base class `PipeChannelBase` is unresolved
// (v0.20.0.3 build hit C2504 "undefined base class" here).
using namespace weasel;

static int s_passed = 0;
static int s_failed = 0;
#define ASSERT(cond, msg) do { \
  if (cond) { s_passed++; printf("  PASS: %s\n", msg); } \
  else { s_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

// We need a concrete PipeChannelBase to exercise. We don't need a real
// pipe, so we instantiate PipeChannelBase with a fake name and never
// call Connect(). Only the thread-local state helpers are exercised.
class TestableChannel : public PipeChannelBase {
 public:
  // Public wrappers for protected members — used by this test only.
  // Return void* and cast at the call site to avoid pulling the nested
  // ChannelContext type into the test signature (helps with forward decl).
  void* GetContextForTest() { return static_cast<void*>(_GetContext()); }
  void* GetPipeHandleForTest() { return static_cast<void*>(_GetPipeHandle()); }
  TestableChannel()
      : PipeChannelBase(std::wstring(L"\\\\.\\pipe\\FluxingTest_RaceCheck"),
                        64 * 1024, nullptr) {}
};

static void TouchContext(TestableChannel& ch, std::vector<uintptr_t>& seen) {
  // 1000 round trips per thread — enough to expose TOCTOU race
  for (int i = 0; i < 1000; ++i) {
    auto* ctx = ch.GetContextForTest();
    seen.push_back(reinterpret_cast<uintptr_t>(ctx));
    auto* h = ch.GetPipeHandleForTest();
    seen.push_back(reinterpret_cast<uintptr_t>(h));
  }
}

static void RunRaceTest(int n_threads) {
  printf("  TestPipeChannelRace: %d threads x 1000 round trips\n", n_threads);
  TestableChannel ch;

  std::vector<std::thread> threads;
  std::vector<std::vector<uintptr_t>> all_seen(n_threads);
  std::atomic<int> ready{0};
  std::atomic<bool> go{false};

  // Barrier so all threads call TouchContext() simultaneously
  for (int i = 0; i < n_threads; ++i) {
    threads.emplace_back([&, i]() {
      ready.fetch_add(1);
      while (!go.load(std::memory_order_acquire)) { _mm_pause(); }
      TouchContext(ch, all_seen[i]);
    });
  }
  while (ready.load() < n_threads) { std::this_thread::yield(); }
  go.store(true, std::memory_order_release);

  for (auto& t : threads) t.join();

  // Per-thread pointer stability: each thread must see the SAME
  // context pointer across all 2000 calls. (If the boost::thread_specific_ptr
  // race were present, two threads would see the same context pointer
  // because the second reset() would overwrite the first — but we cannot
  // easily distinguish that from a working tls. Instead we check: every
  // thread's seen[*] for the context (even indices) is all equal to the
  // first value it saw.)
  bool all_stable = true;
  for (int i = 0; i < n_threads; ++i) {
    if (all_seen[i].empty()) continue;
    uintptr_t first = all_seen[i][0];
    for (size_t j = 2; j < all_seen[i].size(); j += 2) {
      if (all_seen[i][j] != first) { all_stable = false; break; }
    }
  }
  ASSERT(all_stable, "Per-thread context pointer is stable (no replace)");

  // Distinct-per-thread: each thread must see a DIFFERENT context pointer
  // from the other threads. With thread_local this is guaranteed; with the
  // broken boost::thread_specific_ptr pattern, two threads could share the
  // same context (when one reset() overwrote the other's ptr).
  std::set<uintptr_t> unique_ctx;
  for (int i = 0; i < n_threads; ++i) {
    if (!all_seen[i].empty()) unique_ctx.insert(all_seen[i][0]);
  }
  ASSERT(unique_ctx.size() == (size_t)n_threads,
         "Each thread sees its own context (no thread-pool aliasing)");

  // Pipe handle likewise per-thread distinct.
  std::set<uintptr_t> unique_h;
  for (int i = 0; i < n_threads; ++i) {
    if (all_seen[i].size() > 1) unique_h.insert(all_seen[i][1]);
  }
  ASSERT(unique_h.size() == (size_t)n_threads,
         "Each thread sees its own pipe handle (no aliasing)");
}

int main() {
  printf("=== TestPipeChannelRace: spec 076 regression ===\n");
  // Run with multiple thread counts; spec 076's known trigger is "any two
  // threads first-touching concurrently", so 2 threads suffice to expose
  // the bug, but we also try larger counts to amplify.
  for (int n : {2, 8, 32, 100}) {
    RunRaceTest(n);
  }
  printf("\nResult: %d passed, %d failed\n", s_passed, s_failed);
  return s_failed == 0 ? 0 : 1;
}