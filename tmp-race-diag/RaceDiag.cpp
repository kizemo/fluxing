// RaceDiag.cpp — instrumented copy of test/TestPipeChannelRace/TestPipeChannelRace.cpp
// Purpose: decide whether the intermittent failure of
//   "Each thread sees its own context (no thread-pool aliasing)"
// is a REAL defect in PipeChannelBase::_GetContext() or an artifact of the
// original test's assumptions.
//
// Added diagnostics:
//   - prints unique_ctx.size() vs n_threads
//   - prints every duplicated address and which threads saw it
//   - prints, per thread, whether the FIRST context pointer equals the LAST
//   - prints the thread's own TLS address so we can tell thread identity apart
//
// Build: cl /EHsc /std:c++17 /MD RaceDiag.cpp ..\..\WeaselIPC\PipeChannel.cpp
//        /I<repo>\include /I<boost> /link /OUT:RaceDiag.exe

#include <windows.h>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <map>
#include <mutex>
#include <random>
#include <chrono>
#include <immintrin.h>
#include "PipeChannel.h"

using namespace weasel;

class TestableChannel : public PipeChannelBase {
 public:
  void* GetContextForTest() { return static_cast<void*>(_GetContext()); }
  void* GetPipeHandleForTest() { return static_cast<void*>(_GetPipeHandle()); }
  TestableChannel()
      : PipeChannelBase(std::wstring(L"\\\\.\\pipe\\FluxingTest_RaceCheck"),
                        64 * 1024, nullptr) {}
};

struct ThreadResult {
  std::vector<uintptr_t> ctxs;   // context pointer per iteration
  std::vector<uintptr_t> tls;    // address of this thread's own TLS slot
  std::vector<DWORD> tid;
};

static void TouchContext(TestableChannel& ch, ThreadResult& r) {
  for (int i = 0; i < 1000; ++i) {
    r.ctxs.push_back(reinterpret_cast<uintptr_t>(ch.GetContextForTest()));
    r.tls.push_back(reinterpret_cast<uintptr_t>(ch.GetPipeHandleForTest()));
    r.tid.push_back(GetCurrentThreadId());
  }
}

// MODE 2: end barrier. Every thread stays ALIVE until every thread has
// finished its iterations. While they are alive, their thread_local
// ChannelContext objects are still owned, so no context can be freed and its
// heap address reused by another thread. Any address collision observed in
// this mode would be a REAL aliasing defect.
static void TouchContextHeld(TestableChannel& ch, ThreadResult& r,
                             std::atomic<int>& done, int n_threads,
                             std::atomic<bool>& release) {
  for (int i = 0; i < 1000; ++i) {
    r.ctxs.push_back(reinterpret_cast<uintptr_t>(ch.GetContextForTest()));
    r.tls.push_back(reinterpret_cast<uintptr_t>(ch.GetPipeHandleForTest()));
    r.tid.push_back(GetCurrentThreadId());
  }
  done.fetch_add(1);
  while (!release.load(std::memory_order_acquire)) { _mm_pause(); }
}

static int RunRaceTest(int n_threads) {
  printf("  ---- %d threads x 1000 round trips ----\n", n_threads);
  TestableChannel ch;

  std::vector<std::thread> threads;
  std::vector<ThreadResult> res(n_threads);
  std::atomic<int> ready{0};
  std::atomic<bool> go{false};
  std::atomic<int> done{0};
  std::atomic<bool> release{false};

  for (int i = 0; i < n_threads; ++i) {
    threads.emplace_back([&, i]() {
      ready.fetch_add(1);
      while (!go.load(std::memory_order_acquire)) { _mm_pause(); }
      TouchContextHeld(ch, res[i], done, n_threads, release);
    });
  }
  while (ready.load() < n_threads) { std::this_thread::yield(); }
  go.store(true, std::memory_order_release);

  // Wait until every thread finished its iterations, but NO thread has exited
  // yet -> every thread_local ChannelContext is still alive and owned.
  while (done.load() < n_threads) { std::this_thread::yield(); }

  // === compare WHILE ALL CONTEXTS ARE STILL ALIVE ===
  {
    std::map<uintptr_t, std::vector<int>> owner;
    for (int i = 0; i < n_threads; ++i)
      if (!res[i].ctxs.empty()) owner[res[i].ctxs[0]].push_back(i);
    size_t uniq = owner.size();
    printf("    [LIVE] unique ctx addresses (all threads alive): %zu (expected %d) -> %s\n",
           uniq, n_threads, uniq == (size_t)n_threads ? "PASS" : "FAIL");
    if (uniq != (size_t)n_threads) {
      for (auto& kv : owner)
        if (kv.second.size() > 1) {
          printf("        LIVE-DUPLICATE ctx 0x%llX seen by threads:", (unsigned long long)kv.first);
          for (int t : kv.second) printf(" %d", t);
          printf("\n");
        }
    }
    std::set<uintptr_t> uniq_tls;
    for (int i = 0; i < n_threads; ++i)
      if (!res[i].tls.empty()) uniq_tls.insert(res[i].tls[0]);
    printf("    [LIVE] unique TLS slot addresses          : %zu (expected %d) -> %s\n",
           uniq_tls.size(), n_threads,
           uniq_tls.size() == (size_t)n_threads ? "PASS" : "FAIL");
  }

  release.store(true, std::memory_order_release);
  for (auto& t : threads) t.join();

  // A) per-thread stability
  bool all_stable = true;
  for (int i = 0; i < n_threads; ++i) {
    if (res[i].ctxs.empty()) continue;
    uintptr_t first = res[i].ctxs[0];
    for (size_t j = 1; j < res[i].ctxs.size(); ++j)
      if (res[i].ctxs[j] != first) { all_stable = false; break; }
  }
  printf("    [A] per-thread ctx stable          : %s\n", all_stable ? "YES" : "NO");

  // B) distinct ctx per thread
  std::map<uintptr_t, std::vector<int>> owner;
  for (int i = 0; i < n_threads; ++i)
    if (!res[i].ctxs.empty()) owner[res[i].ctxs[0]].push_back(i);

  size_t uniq = owner.size();
  printf("    [B] unique ctx addresses           : %zu (expected %d) -> %s\n",
         uniq, n_threads, uniq == (size_t)n_threads ? "PASS" : "FAIL");

  if (uniq != (size_t)n_threads) {
    for (auto& kv : owner) {
      if (kv.second.size() > 1) {
        printf("        DUPLICATE ctx 0x%llX seen by threads:",
               (unsigned long long)kv.first);
        for (int t : kv.second) printf(" %d(tid=%lu)", t, (unsigned long)res[t].tid[0]);
        printf("\n");
      }
    }
    // how many distinct OS threads actually reported
    std::set<DWORD> tids;
    for (int i = 0; i < n_threads; ++i) if (!res[i].tid.empty()) tids.insert(res[i].tid[0]);
    printf("        distinct OS thread ids reported: %zu (expected %d)\n",
           tids.size(), n_threads);
    // are the ctx addresses of the DUPLICATE group equal to a *previous*
    // round's address? print all addresses sorted for eyeballing
    printf("        ctx addresses (sorted):\n         ");
    std::vector<uintptr_t> all;
    for (auto& kv : owner) all.push_back(kv.first);
    std::sort(all.begin(), all.end());
    for (size_t i = 0; i < all.size(); ++i) {
      printf("0x%llX ", (unsigned long long)all[i]);
      if ((i + 1) % 6 == 0) printf("\n         ");
    }
    printf("\n");
  }

  // C) distinct TLS slot per thread
  std::set<uintptr_t> uniq_tls;
  for (int i = 0; i < n_threads; ++i)
    if (!res[i].tls.empty()) uniq_tls.insert(res[i].tls[0]);
  printf("    [C] unique TLS slot addresses      : %zu (expected %d) -> %s\n",
         uniq_tls.size(), n_threads,
         uniq_tls.size() == (size_t)n_threads ? "PASS" : "FAIL");

  // D) the address that the ORIGINAL test used for the "handle" assertion
  std::set<uintptr_t> uniq_h;
  for (int i = 0; i < n_threads; ++i)
    if (res[i].tls.size() > 1) uniq_h.insert(res[i].tls[1]);
  printf("    [D] original test's handle index[1]: %zu (expected %d) -> %s\n",
         uniq_h.size(), n_threads,
         uniq_h.size() == (size_t)n_threads ? "PASS" : "FAIL");

  return (all_stable && uniq == (size_t)n_threads) ? 0 : 1;
}

int main() {
  printf("=== RaceDiag: instrumented spec 076 regression ===\n");
  int fails = 0;
  for (int n : {2, 8, 32, 100}) fails += RunRaceTest(n);
  printf("\nResult: %d round(s) with a failing condition\n", fails);
  return fails == 0 ? 0 : 1;
}
