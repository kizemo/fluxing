// WeaselServer.cpp : main source file for WeaselServer.exe
//
//	WTL MessageLoop 封装了消息循环. 实现了 getmessage/dispatchmessage....

#include "stdafx.h"
#include "resource.h"
#include <GdiPlus.h>
#pragma comment(lib, "gdiplus.lib")
#include "WeaselService.h"
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <winsparkle.h>
#include <functional>
#include <ShellScalingApi.h>
#include <WinUser.h>
#include <memory>
#include <atlstr.h>
#pragma comment(lib, "Shcore.lib")

// L67-fix: unhandled-exception safety net. The release build had no
// SetUnhandledExceptionFilter and no DLOG/LOG output (WEASEL_ENABLE_LOGGING
// not defined), so when WeaselServer.exe crashed we could not diagnose the
// root cause from a post-mortem. Now we capture a minidump to a stable
// path under %LOCALAPPDATA%\\fluxing\\crash\\<timestamp>.dmp and let the OS
// continue with its default handler (which usually pops up the WER dialog).
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace {

LONG WINAPI WriteMinidumpOnCrash(EXCEPTION_POINTERS* ex) {
  // Pick a writable location: %LOCALAPPDATA%\fluxing\crash\
  wchar_t dir[MAX_PATH] = {0};
  if (!GetEnvironmentVariableW(L"LOCALAPPDATA", dir, _countof(dir)))
    return EXCEPTION_EXECUTE_HANDLER;
  wcscat_s(dir, L"\\fluxing\\crash");
  CreateDirectoryW(dir, NULL);  // ignore failure - if it exists, that's fine

  wchar_t path[MAX_PATH];
  SYSTEMTIME st;
  GetLocalTime(&st);
  _snwprintf_s(path, _TRUNCATE, L"%s\\fluxing-%04u%02u%02u-%02u%02u%02u-%04x.dmp",
               dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
               ex ? ex->ExceptionRecord->ExceptionCode : 0);

  HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) return EXCEPTION_EXECUTE_HANDLER;

  MINIDUMP_EXCEPTION_INFORMATION mei;
  mei.ThreadId = GetCurrentThreadId();
  mei.ExceptionPointers = ex;
  mei.ClientPointers = FALSE;

  // MiniDumpWithDataSegs gives us stacks + global memory; enough to debug.
  // MiniDumpWithIndirectlyReferencedMemory adds even more if available.
  MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                    MiniDumpWithDataSegs, &mei, NULL, NULL);
  CloseHandle(file);
  // Return to default OS handler (WER dialog / nothing).
  return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int nCmdShow) {
  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  if (!IsWindowsBlueOrLaterEx()) {
    CString info, cap;
    info.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING);
    cap.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING_CAPTION);
    MessageBoxExW(NULL, info, cap, MB_ICONERROR, langId);
    return 0;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // 防止服务进程开启输入法
  ImmDisableIME(-1);

  WCHAR user_name[20] = {0};
  DWORD size = _countof(user_name);
  GetUserName(user_name, &size);
  if (!_wcsicmp(user_name, L"SYSTEM")) {
    return 1;
  }

  // L67-fix: install crash-dump safety net BEFORE any non-trivial init.
  // Without this, a crash inside WeaselServerApp ctor / app.Run() leaves us
  // with no post-mortem (release build has DLOG=no-op). Now we get a
  // minidump in %LOCALAPPDATA%\fluxing\crash\ for the next round of
  // diagnosis (Phase 4 — convert symptoms to verifiable C++ frames).
  SetUnhandledExceptionFilter(WriteMinidumpOnCrash);

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  if (!wcscmp(L"/userdir", lpstrCmdLine)) {
    CreateDirectory(WeaselUserDataPath().c_str(), NULL);
    WeaselServerApp::explore(WeaselUserDataPath());
    return 0;
  }
  if (!wcscmp(L"/weaseldir", lpstrCmdLine)) {
    WeaselServerApp::explore(WeaselServerApp::install_dir());
    return 0;
  }
  if (!wcscmp(L"/ascii", lpstrCmdLine) || !wcscmp(L"/nascii", lpstrCmdLine)) {
    weasel::Client client;
    bool ascii = !wcscmp(L"/ascii", lpstrCmdLine);
    if (client.Connect())  // try to connect to running server
    {
      if (ascii)
        client.TrayCommand(ID_WEASELTRAY_ENABLE_ASCII);
      else
        client.TrayCommand(ID_WEASELTRAY_DISABLE_ASCII);
    }
    return 0;
  }

  // command line option /q stops the running server
  bool quit = !wcscmp(L"/q", lpstrCmdLine) || !wcscmp(L"/quit", lpstrCmdLine);
  // restart if already running
  {
    weasel::Client client;
    if (client.Connect())  // try to connect to running server
    {
      client.ShutdownServer();
      if (quit)
        return 0;
      int retry = 0;
      while (client.Connect() && retry < 10) {
        client.ShutdownServer();
        retry++;
        Sleep(50);
      }
      if (retry >= 10)
        return 0;
    } else if (quit)
      return 0;
  }

  bool check_updates = !wcscmp(L"/update", lpstrCmdLine);
  if (check_updates) {
    WeaselServerApp::check_update();
  }

  CreateDirectory(WeaselUserDataPath().c_str(), NULL);

  int nRet = 0;
  try {
    WeaselServerApp app;
    RegisterApplicationRestart(NULL, 0);
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);
    nRet = app.Run();
    Gdiplus::GdiplusShutdown(gdiplusToken);
  } catch (...) {
    // bad luck...
    nRet = -1;
  }

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}