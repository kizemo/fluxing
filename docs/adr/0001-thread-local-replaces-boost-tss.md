# ADR 0001 — C++11 thread_local replaces boost::thread_specific_ptr in PipeChannel

## Status

Accepted — 2026-08-07. Implementation: spec 076, commit (v0.20.0.2).

## Context

`include/PipeChannel.h` provided per-thread state via two
`boost::thread_specific_ptr<T>` members:

- `hpipe_ptr` (HANDLE) — named-pipe handle
- `context` (ChannelContext) — 64 KB request/response buffer + state

The access pattern was a "check-then-create" idiom:

```cpp
ChannelContext* _GetContext() const {
  if (!context.get()) {
    context.reset(new ChannelContext(buff_size));
  }
  return context.get();
}
```

In multi-threaded host processes (Claude Code, 微信, dopus, ToDesk,
Explorer — every app that loads `weaselx64.dll`), TSF callbacks fire
on **concurrent threads**: the UI thread, the TextInputHost RPC
thread, the app's own worker thread, plus any thread that calls
into the pipe channel. When two threads first touch the channel
simultaneously, both observe `context.get() == nullptr`, both
allocate, the second `reset()` overwrites the first's pointer, and
the first thread's returned `ChannelContext*` becomes a
use-after-free handle. The first thread's eventual destructor
double-frees the second thread's allocation, ntdll detects heap
metadata corruption, and the entire process is terminated with
`STATUS_HEAP_CORRUPTION` (0xc0000374).

This bug shipped in v0.19.0.x and bit every host process that
loaded `weaselx64.dll`. Crash reports in v0.20.0.0/0.20.0.1 had
multiple candidate explanations (NSIS path mismatch, librime
`key_binder` parse error, missing `emoji.json` data files) and
three ship attempts were made before a WinDbg `!analyze -v` on a
full crash dump identified the actual root cause: a 24-byte
`std::thread::invoker` allocation freed from inside a thread-init
lambda (`weaselx64!<lambda_306bd6bd9716a5176553a603b909d585>::<lambda_invoker_cdecl>+0x91`).

## Decision

Replace the two `boost::thread_specific_ptr` members with C++11
`static thread_local` variables. The compiler generates a
thread-safe lazy-initialization guard for `static thread_local`,
eliminating the TOCTOU race at the language level:

```cpp
HANDLE* _GetPipeHandle() const {
  static thread_local HANDLE h = INVALID_HANDLE_VALUE;
  return &h;
}

ChannelContext* _GetContext() const {
  static thread_local std::unique_ptr<ChannelContext> ctx(
      new ChannelContext(buff_size));
  return ctx.get();
}
```

The `<boost/thread.hpp>` and `<boost/thread/tss.hpp>` includes are
removed.

## Consequences

### Easier

- **Race eliminated at the language level.** The C++ standard
  guarantees that `static thread_local` initialization is
  thread-safe across concurrent first-access from multiple threads.
  No mutex, no check-then-create, no second allocation overwriting
  the first.
- **Same per-thread semantics.** Each thread still gets its own
  pipe handle and its own buffer, with the same lifetime
  characteristics (destruction on thread exit).
- **Smaller dependency surface.** Removes the `<boost/thread.hpp>`
  and `<boost/thread/tss.hpp>` includes. `weaselx64.dll` size
  dropped 67 KB (2,017,280 → 1,949,184 bytes) after rebuild, and
  build time decreased.
- **Faster first-touch path.** `static thread_local` is guarded by
  a single atomic flag in the compiler's runtime; the previous
  `boost::thread_specific_ptr` implementation did a hash lookup
  into a thread-local map for every `get()`.

### Harder

- **DLL-unload thread-local destruction ordering.** With
  `boost::thread_specific_ptr`, the per-thread values are owned by
  the boost runtime, which is shared across DLLs. With
  `static thread_local`, the per-thread destructors are owned by
  the MSVC C++ runtime inside the DLL that declared them. If a
  thread that touched `weaselx64.dll` is still alive when
  `weaselx64.dll` unloads (i.e., `FreeLibrary` while threads
  exist), the runtime invokes the destructors with the DLL's
  already-unloaded code, which is a hard crash. In practice
  `weaselx64.dll` is loaded for the lifetime of the host process
  (Chrome-style lifetime — never unloaded), so this is theoretical,
  not practical, for the TSF shim. If this ever becomes a real
  concern, the fix is to require host processes to call
  `FreeLibrary` only on threads that have not touched the
  channel, or to leak the per-thread state on DLL unload.

## Alternatives considered

1. **Mutex around the `if (!ptr) reset(new)` pattern.** Would
   serialize every `get()` call across all threads. Unacceptable
   latency on the input hot path (every key event is an IPC
   round-trip through `_GetContext`).
2. **`std::call_once` per thread.** Standard idiom for
   one-time-per-thread init, but the underlying
   `boost::thread_specific_ptr::reset()` is still racy — a
   second thread's `reset()` wins and loses the first thread's
   pointer. `call_once` does not solve this; it only ensures the
   lambda runs exactly once per thread, but the first-thread's
   `reset()` and second-thread's `reset()` still race on the
   internal `unique_ptr` write.
3. **Process-level singleton with mutex.** Single
   `ChannelContext` shared by all threads. The buffer is reused
   across requests, which is fine for the request/response
   pattern, but every `Write()` / `Transact()` call would have to
   hold the mutex for the full IPC round-trip, serializing all
   threads through one shared buffer. This loses the parallel
   request throughput the per-thread buffers provided.
4. **Switch to `std::shared_ptr<ChannelContext>` + mutex around
   the `if (!ptr) make_shared(new ...)` pattern.** `shared_ptr`
   has atomic refcount, so a thread returning a local copy of
   the shared pointer keeps the underlying object alive even if
   another thread's `reset()` later replaces the
   thread-specific slot. The atomic refcount is overhead but
   bounded. Rejected as overkill — the per-thread state is
   truly per-thread and the refcount buys nothing.

## Verification

- **Static analysis**: `grep -aE 'thread_specific_ptr|boost::tss' weaselx64.dll` returns
  no matches. `boost::thread_specific_ptr` is fully removed from
  the x64 TSF shim binary.
- **Crash repro (pending user)**: install v0.20.0.2, open Claude
  Code + 微信 + dopus, switch to Fluxing IME, type Chinese. The
  pre-fix scenario that reliably produced 11 dumps in
  `C:\fluxing-dumps` should now produce 0 dumps. The user's own
  flow is required here because the race only fires when *real*
  host apps load `weaselx64.dll` and concurrently call into it;
  a single-app smoke test does not exercise the race.
- **PageHeap verification (out of scope for v0.20.0.2)**: enable
  PageHeap on a test host (`gflags /i claude.exe +hpa`) and
  reproduce the original trigger. PageHeap makes any out-of-bounds
  write break at the write site, not at the eventual free site.
  Useful as a regression test once we want to prove that no
  *other* heap bugs in the same vicinity remain.

## Related

- `L##-ThreadSpecificPtr-Race` in `.specify/memory/lessons-learned.md`
- `.specify/specs/076-thread-specific-ptr-race/{spec,plan,tasks}.md`
- `CHANGELOG.md` `[0.20.0.2-fluxing]` entry
