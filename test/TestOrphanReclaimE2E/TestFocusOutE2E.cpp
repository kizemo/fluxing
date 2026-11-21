// TestFocusOutE2E.cpp
//
// Synthetic WeaselServer IPC client for acceptance item B-4:
//   "the user switches window / switches away from Fluxing => the behaviour
//    must be the same as before batch AW: presence flips to absent and the
//    quick panel is still hidden".
//
// It is a plain Win32 named pipe client, same wire format as the sibling
// tool test/TestOrphanReclaimE2E/TestOrphanReclaimE2E.cpp (that file proved
// the format against the real server; this file reuses it verbatim):
//   request  : 12 byte header {DWORD Msg, DWORD wParam, DWORD lParam}
//              (+ optional body, UTF-16LE text, terminated by a "." line)
//   response : DWORD result (+ optional body)
// The pipe is a message pipe (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE |
// PIPE_WAIT), so one WriteFile == one message.
//
// Command numbers and focus flags come from include/WeaselIPC.h (R7: the
// production code and this test must share one source of truth) - this file
// includes that header instead of hard coding the numbers.
//
// The server side answers are observed in %TEMP%\fluxing-profile-notify.log:
//   [server] focus_in profile presence=present
//   [server] focus_out param=<n> presence=absent|kept
//   [server] remove_session active presence=absent
//   [server] remove_session active orphan_reclaim presence=kept
//   [server] reclaim orphan session ipc_id=<n> client_pid=<n> live=<n>
//
// Real TSF event order (WeaselTSF.cpp Deactivate):
//   _PostProfileNotify(kProfileNotifyDeactivated);  // async thread
//   m_client.EndSession();                          // synchronous
// => the server sees END_SESSION **before** FOCUS_OUT(PROFILE_DEACTIVATED).
//    The "--focusout-deactivate" mode reproduces exactly that order.
//
// Modes:
//   --end-session <marker>       START_SESSION, FOCUS_IN(APP_FOCUS_GAINED),
//                                keep typing, END_SESSION, exit.
//                                (negative control: no focus-out is sent)
//   --focusout-deactivate <m>    START_SESSION, FOCUS_IN(PROFILE_ACTIVATED),
//                                keep typing, END_SESSION,
//                                FOCUS_OUT(PROFILE_DEACTIVATED), exit.
//                                (real "user switched to another IME" order)
//   --focusout-only <marker>     START_SESSION, FOCUS_IN(PROFILE_ACTIVATED),
//                                keep typing, FOCUS_OUT(PROFILE_DEACTIVATED),
//                                END_SESSION, exit.
//                                (the focus-out message alone flips presence)
//   --focusout-appfocus <m>     START_SESSION, FOCUS_IN(APP_FOCUS_GAINED),
//                                keep typing, FOCUS_OUT(APP_FOCUS_LOST),
//                                END_SESSION, exit.
//                                ("user switched window" order)
//   --abrupt-exit <marker>       START_SESSION, keep typing, exit WITHOUT
//                                END_SESSION (simulates a crashed host).
//   --sweep-trigger <marker>     START_SESSION only (this is what makes the
//                                server run its lazy orphan sweep), exit
//                                WITHOUT END_SESSION.
//
// Flag (combinable with any mode above; the mode keeps its argument order):
//   -Legacy                      omit the "session.client_pid=<pid>" line
//                                from the START_SESSION body, byte for byte.
//                                Everything else (the other body lines, the
//                                message order, the typing, the abrupt exit)
//                                stays identical.  That IS the old client
//                                ADR 0067 must leave untouched (checklist
//                                item B-5). Without this flag the handshake
//                                reports the pid, so the server may reclaim
//                                the session once the client is provably dead.
//   --end-session-by-id <id>     END_SESSION for an existing ipc_id (cleanup).
//   --echo <sessionId>           raw ECHO probe; ECHO=0 means the session is
//                                gone on the server.
//   --help
//
// All modes report the session id in a marker file (ipc_id=<n>, pid=<n>).
//
// Why the client keeps typing: the maintenance gate facts ("last keystroke"
// and "unfinished composition") go stale after the 10 s idle grace, and a
// probe that only presses one key would let the gate open and lose its
// discriminating power (batch B-1 lesson). Every mode therefore types one
// letter every 700 ms until the focus event it is about to send.
//
// This file must stay pure ASCII (the project does not guarantee /utf-8 for
// every toolchain and Chinese literals would become mojibake).

#include <windows.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <WeaselIPC.h>

namespace {

#pragma pack(push, 1)
struct PipeHeader {
  DWORD msg;
  DWORD wparam;
  DWORD lparam;
};
#pragma pack(pop)

// One letter every 700 ms keeps the "user is typing" facts fresh.
const DWORD kTypeIntervalMs = 700;

class IpcClient {
 public:
  ~IpcClient() { Close(); }

  bool Connect(std::wstring* err) {
    const std::wstring name = weasel::GetPipeName();
    for (int attempt = 0; attempt < 30; ++attempt) {
      HANDLE h = ::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                               NULL, OPEN_EXISTING, 0, NULL);
      if (h != INVALID_HANDLE_VALUE) {
        DWORD mode = PIPE_READMODE_MESSAGE;
        if (!::SetNamedPipeHandleState(h, &mode, NULL, NULL)) {
          const DWORD e = ::GetLastError();
          ::CloseHandle(h);
          *err = L"SetNamedPipeHandleState failed, error=" + std::to_wstring(e);
          return false;
        }
        pipe_ = h;
        return true;
      }
      const DWORD e = ::GetLastError();
      if (e != ERROR_PIPE_BUSY) {
        *err = L"CreateFileW failed on [" + name + L"] error=" +
               std::to_wstring(e);
        return false;
      }
      ::WaitNamedPipeW(name.c_str(), 500);
    }
    *err = L"pipe stayed busy for 15s: " + name;
    return false;
  }

  void Close() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
      ::DisconnectNamedPipe(pipe_);
      ::CloseHandle(pipe_);
      pipe_ = INVALID_HANDLE_VALUE;
    }
  }

  bool Transact(DWORD msg, DWORD wparam, DWORD lparam,
                const std::wstring& body, DWORD* result, std::wstring* err) {
    if (pipe_ == INVALID_HANDLE_VALUE) {
      *err = L"not connected";
      return false;
    }
    const size_t body_bytes = body.size() * sizeof(wchar_t);
    std::vector<char> payload(sizeof(PipeHeader) + body_bytes);
    PipeHeader header;
    header.msg = msg;
    header.wparam = wparam;
    header.lparam = lparam;
    memcpy(payload.data(), &header, sizeof(header));
    if (body_bytes)
      memcpy(payload.data() + sizeof(PipeHeader), body.data(), body_bytes);

    DWORD written = 0;
    if (!::WriteFile(pipe_, payload.data(),
                     static_cast<DWORD>(payload.size()), &written, NULL)) {
      *err = L"WriteFile failed, error=" + std::to_wstring(::GetLastError());
      return false;
    }

    std::vector<char> resp(64 * 1024);
    DWORD got = 0;
    if (!::ReadFile(pipe_, resp.data(), static_cast<DWORD>(resp.size()), &got,
                    NULL)) {
      *err = L"ReadFile failed, error=" + std::to_wstring(::GetLastError());
      return false;
    }
    if (got < sizeof(DWORD)) {
      *err = L"short response, bytes=" + std::to_wstring(got);
      return false;
    }
    DWORD value = 0;
    memcpy(&value, resp.data(), sizeof(value));
    *result = value;
    return true;
  }

  bool StartSession(DWORD* ipc_id, std::wstring* err, bool legacy = false) {
    // Same body as weasel::ClientImpl::_WriteClientInfo(). session.client_pid
    // is the fact source for ADR 0067: the server may only reclaim a session
    // whose owner pid is known and provably dead. Reporting it here is what
    // makes the orphan control run meaningful (the pid IS known, so the
    // reclaim criteria are applicable - the only difference to the normal
    // path is the missing END_SESSION).
    //
    // legacy=true drops exactly that one line: the body becomes what a client
    // built before ADR 0067 sends, so the server must file the session as
    // "pid unknown" and never reclaim it (item B-5).
    std::wstring body;
    body += L"action=session\n";
    body += L"session.client_app=" + ClientAppName() + L"\n";
    body += L"session.client_type=tsf\n";
    if (!legacy) {
      body += L"session.client_pid=" +
              std::to_wstring(::GetCurrentProcessId()) + L"\n";
    }
    body += L".\n";
    DWORD result = 0;
    if (!Transact(WEASEL_IPC_START_SESSION, 0, 0, body, &result, err))
      return false;
    *ipc_id = result;
    return true;
  }

  bool SendKey(DWORD ipc_id, UINT keycode, UINT mask, std::wstring* err) {
    const DWORD packed = (keycode & 0xFFFFu) | ((mask & 0xFFFFu) << 16);
    DWORD result = 0;
    return Transact(WEASEL_IPC_PROCESS_KEY_EVENT, packed, ipc_id,
                    std::wstring(), &result, err);
  }

  bool FocusIn(DWORD client_caps, DWORD ipc_id, std::wstring* err) {
    DWORD result = 0;
    return Transact(WEASEL_IPC_FOCUS_IN, client_caps, ipc_id, std::wstring(),
                    &result, err);
  }

  bool FocusOut(DWORD client_caps, DWORD ipc_id, std::wstring* err) {
    DWORD result = 0;
    return Transact(WEASEL_IPC_FOCUS_OUT, client_caps, ipc_id, std::wstring(),
                    &result, err);
  }

  bool EndSession(DWORD ipc_id, std::wstring* err) {
    DWORD result = 0;
    return Transact(WEASEL_IPC_END_SESSION, 0, ipc_id, std::wstring(), &result,
                    err);
  }

 private:
  // Lower cased exe name of this process, exactly like
  // weasel::ClientImpl::_InitializeClientInfo does (the server lower cases it
  // again before comparing).
  static std::wstring ClientAppName() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring p(path);
    size_t pos = p.find_last_of(L"\\/");
    std::wstring name = (pos == std::wstring::npos) ? p : p.substr(pos + 1);
    for (size_t i = 0; i < name.size(); ++i) {
      if (name[i] >= L'A' && name[i] <= L'Z')
        name[i] = static_cast<wchar_t>(name[i] - L'A' + L'a');
    }
    return name;
  }

  HANDLE pipe_ = INVALID_HANDLE_VALUE;
};

// Marker files hold plain ASCII (numbers and short names), so a narrow copy
// of the wide value is enough and keeps the file readable by any script.
std::string Narrow(const std::wstring& w) {
  if (w.empty())
    return std::string();
  const int need = ::WideCharToMultiByte(
      CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), NULL, 0, NULL, NULL);
  if (need <= 0)
    return std::string();
  std::string out;
  out.resize(static_cast<size_t>(need));
  ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &out[0], need, NULL, NULL);
  return out;
}

bool WriteMarker(const std::wstring& path,
                 const std::vector<std::pair<std::wstring, std::wstring> >& kv,
                 std::wstring* err) {
  std::string text;
  for (size_t i = 0; i < kv.size(); ++i) {
    text += Narrow(kv[i].first);
    text += "=";
    text += Narrow(kv[i].second);
    text += "\n";
  }
  HANDLE h = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    *err = L"CreateFileW failed on " + path + L", error=" +
           std::to_wstring(::GetLastError());
    return false;
  }
  DWORD written = 0;
  const BOOL ok = ::WriteFile(h, text.data(),
                              static_cast<DWORD>(text.size()), &written, NULL);
  const DWORD e = ::GetLastError();
  ::CloseHandle(h);
  if (!ok) {
    *err = L"WriteFile failed on " + path + L", error=" + std::to_wstring(e);
    return false;
  }
  return true;
}

// Keep the "user is typing" facts fresh until the focus event is sent.
void TypeUntil(IpcClient& client, DWORD ipc_id, int milliseconds,
               std::wstring* err) {
  int tick = 0;
  const DWORD start = ::GetTickCount();
  for (;;) {
    const DWORD now = ::GetTickCount();
    if (now - start >= static_cast<DWORD>(milliseconds))
      break;
    const char key = static_cast<char>('a' + (tick % 26));
    ++tick;
    if (!client.SendKey(ipc_id, static_cast<UINT>(static_cast<unsigned char>(key)),
                        0, err)) {
      wprintf(L"WARN key event failed: %s\n", err->c_str());
      fflush(stdout);
      break;
    }
    ::Sleep(kTypeIntervalMs);
  }
}

bool WriteSessionMarker(const std::wstring& marker, DWORD ipc_id,
                        std::wstring* err) {
  std::vector<std::pair<std::wstring, std::wstring> > kv;
  kv.push_back(std::make_pair(std::wstring(L"ipc_id"), std::to_wstring(ipc_id)));
  kv.push_back(std::make_pair(std::wstring(L"pid"),
                               std::to_wstring(::GetCurrentProcessId())));
  return WriteMarker(marker, kv, err);
}

// One "scenario" = start a session, focus in, type, then run the focus /
// session end sequence the mode asks for.
enum class Scenario {
  kEndSessionOnly,      // no focus-out at all (negative control)
  kDeactivateOrder,     // END_SESSION then FOCUS_OUT(PROFILE_DEACTIVATED)
  kFocusOutOnly,        // FOCUS_OUT(PROFILE_DEACTIVATED) then END_SESSION
  kAppFocusLost,        // FOCUS_OUT(APP_FOCUS_LOST) then END_SESSION
  kAbruptExit,          // no END_SESSION at all (orphan control)
  kSweepTrigger,        // START_SESSION only (triggers the orphan sweep)
};

int RunScenario(Scenario scenario, const std::wstring& marker, bool legacy) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ERROR connect failed: %s\n", err.c_str());
    return 1;
  }
  // The script asserts on this line: a -Legacy flag that silently did nothing
  // would turn the negative criterion into an empty test.
  wprintf(L"CLIENT_MODE=%s handshake_client_pid_line=%d pid=%lu\n",
          legacy ? L"legacy" : L"modern", legacy ? 0 : 1,
          static_cast<unsigned long>(::GetCurrentProcessId()));
  fflush(stdout);
  DWORD ipc_id = 0;
  if (!client.StartSession(&ipc_id, &err, legacy)) {
    wprintf(L"ERROR start_session failed: %s\n", err.c_str());
    return 1;
  }
  if (ipc_id == 0) {
    wprintf(L"ERROR start_session returned ipc_id=0\n");
    return 1;
  }
  wprintf(L"SESSION_STARTED ipc_id=%lu pid=%lu\n",
          static_cast<unsigned long>(ipc_id),
          static_cast<unsigned long>(::GetCurrentProcessId()));
  fflush(stdout);

  if (scenario == Scenario::kSweepTrigger) {
    // A new session is what makes the server run its lazy orphan sweep.
    // No END_SESSION here: the caller judges the log slice first and cleans
    // this session up afterwards with --end-session-by-id.
    if (!WriteSessionMarker(marker, ipc_id, &err)) {
      wprintf(L"ERROR marker failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"SWEEP_TRIGGERED ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    ::Sleep(1000);
    client.Close();
    wprintf(L"SWEEP_TRIGGER_DONE ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    return 0;
  }

  // Focus in first: APP_FOCUS_GAINED for the window-switch scenarios,
  // PROFILE_ACTIVATED for the IME-switch scenarios (that is the event that
  // puts the server into the "presence = present" state).
  const bool profile_activated =
      (scenario == Scenario::kDeactivateOrder ||
       scenario == Scenario::kFocusOutOnly);
  const DWORD focus_in_flags =
      profile_activated ? WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED
                        : WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED;
  if (!client.FocusIn(focus_in_flags, ipc_id, &err)) {
    wprintf(L"ERROR focus_in failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"FOCUS_IN_SENT caps=%lu ipc_id=%lu\n",
          static_cast<unsigned long>(focus_in_flags),
          static_cast<unsigned long>(ipc_id));
  fflush(stdout);

  // Keep typing while the user "works", so the gate facts stay fresh.
  TypeUntil(client, ipc_id, 2100, &err);

  if (scenario == Scenario::kAbruptExit) {
    if (!WriteSessionMarker(marker, ipc_id, &err)) {
      wprintf(L"ERROR marker failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"ABRUPT_EXIT ipc_id=%lu (no END_SESSION)\n",
            static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    // Close the pipe and leave: the server never sees an END_SESSION.
    client.Close();
    return 0;
  }

  if (scenario == Scenario::kDeactivateOrder) {
    // Real order (WeaselTSF::Deactivate): END_SESSION goes out synchronously
    // on the TSF thread, the focus-out notification is posted to another
    // thread and arrives afterwards.
    if (!client.EndSession(ipc_id, &err)) {
      wprintf(L"ERROR end_session failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"END_SESSION_SENT ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    ::Sleep(200);
    if (!client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED, ipc_id,
                          &err)) {
      wprintf(L"ERROR focus_out failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"FOCUS_OUT_SENT caps=%lu ipc_id=%lu\n",
            static_cast<unsigned long>(
                WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED),
            static_cast<unsigned long>(ipc_id));
    fflush(stdout);
  } else if (scenario == Scenario::kFocusOutOnly) {
    if (!client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED, ipc_id,
                          &err)) {
      wprintf(L"ERROR focus_out failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"FOCUS_OUT_SENT caps=%lu ipc_id=%lu\n",
            static_cast<unsigned long>(
                WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED),
            static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    ::Sleep(200);
    if (!client.EndSession(ipc_id, &err)) {
      wprintf(L"ERROR end_session failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"END_SESSION_SENT ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
  } else if (scenario == Scenario::kAppFocusLost) {
    if (!client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST, ipc_id,
                          &err)) {
      wprintf(L"ERROR focus_out failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"FOCUS_OUT_SENT caps=%lu ipc_id=%lu\n",
            static_cast<unsigned long>(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST),
            static_cast<unsigned long>(ipc_id));
    fflush(stdout);
    ::Sleep(200);
    if (!client.EndSession(ipc_id, &err)) {
      wprintf(L"ERROR end_session failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"END_SESSION_SENT ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
  } else {  // kEndSessionOnly - negative control, no focus-out at all.
    if (!client.EndSession(ipc_id, &err)) {
      wprintf(L"ERROR end_session failed: %s\n", err.c_str());
      return 1;
    }
    wprintf(L"END_SESSION_SENT ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
    fflush(stdout);
  }

  if (!WriteSessionMarker(marker, ipc_id, &err)) {
    wprintf(L"ERROR marker failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"SCENARIO_DONE ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
  fflush(stdout);
  return 0;
}

int RunEcho(DWORD session_id) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ECHO=ERR connect failed: %s\n", err.c_str());
    return 1;
  }
  DWORD result = 0;
  if (!client.Transact(WEASEL_IPC_ECHO, 0, session_id, std::wstring(), &result,
                       &err)) {
    wprintf(L"ECHO=ERR %s\n", err.c_str());
    return 1;
  }
  wprintf(L"ECHO=%lu\n", static_cast<unsigned long>(result));
  fflush(stdout);
  return 0;
}

int RunEndSessionById(DWORD session_id) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ERROR connect failed: %s\n", err.c_str());
    return 1;
  }
  if (!client.EndSession(session_id, &err)) {
    wprintf(L"ERROR end_session failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"END_SESSION_SENT ipc_id=%lu\n", static_cast<unsigned long>(session_id));
  fflush(stdout);
  return 0;
}

void PrintHelp() {
  wprintf(L"TestFocusOutE2E -- synthetic WeaselServer IPC client (B-4)\n");
  wprintf(L"\n");
  wprintf(L"  --end-session <marker>       session end only, no focus-out\n");
  wprintf(L"  --focusout-deactivate <m>    END_SESSION then FOCUS_OUT(deactivated)\n");
  wprintf(L"  --focusout-only <marker>     FOCUS_OUT(deactivated) then END_SESSION\n");
  wprintf(L"  --focusout-appfocus <m>      FOCUS_OUT(app focus lost) then END_SESSION\n");
  wprintf(L"  --abrupt-exit <marker>       session end without END_SESSION (orphan)\n");
  wprintf(L"  --sweep-trigger <marker>     new session only (triggers orphan sweep)\n");
  wprintf(L"  -Legacy                      with any mode above: do not send the\n");
  wprintf(L"                               session.client_pid line (old client, B-5)\n");
  wprintf(L"  --end-session-by-id <id>     END_SESSION for an existing ipc_id\n");
  wprintf(L"  --echo <sessionId>           raw ECHO probe, 0 means the session is gone\n");
  wprintf(L"  --help                       this text.\n");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 2) {
    PrintHelp();
    return 2;
  }
  const std::wstring mode = argv[1];
  if (mode == L"--help" || mode == L"-h" || mode == L"/?") {
    PrintHelp();
    return 0;
  }
  if (mode == L"--echo") {
    if (argc < 3) {
      wprintf(L"ERROR usage: TestFocusOutE2E --echo <sessionId>\n");
      return 2;
    }
    const DWORD session_id =
        static_cast<DWORD>(::wcstoul(argv[2], NULL, 10));
    return RunEcho(session_id);
  }
  if (mode == L"--end-session-by-id") {
    if (argc < 3) {
      wprintf(L"ERROR usage: TestFocusOutE2E --end-session-by-id <sessionId>\n");
      return 2;
    }
    const DWORD session_id =
        static_cast<DWORD>(::wcstoul(argv[2], NULL, 10));
    return RunEndSessionById(session_id);
  }
  // -Legacy may appear in any position; the marker is the first argument that
  // is neither the mode nor a flag.
  bool legacy = false;
  std::wstring marker;
  for (int i = 2; i < argc; ++i) {
    if (_wcsicmp(argv[i], L"-Legacy") == 0 || _wcsicmp(argv[i], L"/Legacy") == 0) {
      legacy = true;
      continue;
    }
    if (marker.empty())
      marker = argv[i];
  }
  if (marker.empty()) {
    wprintf(L"ERROR usage: TestFocusOutE2E %s <markerFile> [-Legacy]\n",
            mode.c_str());
    return 2;
  }
  if (mode == L"--end-session")
    return RunScenario(Scenario::kEndSessionOnly, marker, legacy);
  if (mode == L"--focusout-deactivate")
    return RunScenario(Scenario::kDeactivateOrder, marker, legacy);
  if (mode == L"--focusout-only")
    return RunScenario(Scenario::kFocusOutOnly, marker, legacy);
  if (mode == L"--focusout-appfocus")
    return RunScenario(Scenario::kAppFocusLost, marker, legacy);
  if (mode == L"--abrupt-exit")
    return RunScenario(Scenario::kAbruptExit, marker, legacy);
  if (mode == L"--sweep-trigger")
    return RunScenario(Scenario::kSweepTrigger, marker, legacy);
  wprintf(L"ERROR unknown mode: %s\n", mode.c_str());
  PrintHelp();
  return 2;
}
