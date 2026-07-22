//
// ForegroundCapture.cpp — 实现
//
#include "stdafx.h"
#include "ForegroundCapture.h"

namespace fluxing {
namespace foreground_restore {

namespace {
// 全局缓存。简单 critical section 保护 — 调用方 (IPC worker thread +
// PipeThreadProc) 都是 WeaselServer.exe 内的线程,锁开销可忽略。
HWND g_hwnd = nullptr;
DWORD g_tid = 0;
CRITICAL_SECTION g_cs;

struct _CriticalSectionInit {
  _CriticalSectionInit() { InitializeCriticalSection(&g_cs); }
  ~_CriticalSectionInit() { DeleteCriticalSection(&g_cs); }
} _csInit;
}  // namespace

void CaptureFromCurrentThread() {
  // GetForegroundWindow() 是 session-global API — 在 IPC worker thread 调 = 抓
  // user app 当前 foreground。因为 GetWindowThreadProcessId 需要 HWND 非 NULL 才
  // 有效,前置 nullptr check。
  HWND hwnd = GetForegroundWindow();
  if (!hwnd) return;

  DWORD tid = GetWindowThreadProcessId(hwnd, nullptr);
  if (tid == 0) return;

  EnterCriticalSection(&g_cs);
  g_hwnd = hwnd;
  g_tid = tid;
  LeaveCriticalSection(&g_cs);
}

HWND GetHwnd() {
  EnterCriticalSection(&g_cs);
  HWND h = g_hwnd;
  LeaveCriticalSection(&g_cs);
  return h;
}

DWORD GetThreadId() {
  EnterCriticalSection(&g_cs);
  DWORD t = g_tid;
  LeaveCriticalSection(&g_cs);
  return t;
}

}  // namespace foreground_restore
}  // namespace fluxing
