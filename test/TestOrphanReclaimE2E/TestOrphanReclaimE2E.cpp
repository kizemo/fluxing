// TestOrphanReclaimE2E.cpp
//
// Synthetic IPC client for ADR 0067 (orphan IPC session reclaim), driven end to
// end against a RUNNING WeaselServer.exe.
//
// The tool is a plain Win32 named-pipe client. It does NOT link weasel::Client,
// because weasel::Client keeps its session id private (WeaselIPC/WeaselClientImpl.h
// :: session_id) and the whole point of this tool is to report the ipc_id of the
// synthetic session and to probe that id with a raw ECHO afterwards.
//
// Wire format (include/WeaselIPC.h + WeaselIPC/PipeChannel.cpp):
//   request  : 12 byte header {DWORD Msg, DWORD wParam, DWORD lParam}
//              (+ optional body, UTF-16LE text, terminated by a "." line)
//   response : DWORD result (+ optional body)
// The pipe is a message pipe (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT),
// so one WriteFile == one message.
//
// Command numbers (include/WeaselIPC.h, enum WEASEL_IPC_COMMAND):
//   WEASEL_IPC_ECHO             = WM_APP + 1 = 0x8001
//   WEASEL_IPC_START_SESSION    = 0x8002
//   WEASEL_IPC_END_SESSION      = 0x8003
//   WEASEL_IPC_PROCESS_KEY_EVENT= 0x8004
//
// Modes:
//   --hold <markerFile>    connect, START_SESSION, type letters, write the marker,
//                          then sleep forever. Never sends END_SESSION: the parent
//                          script kills this process to simulate a crashed TSF host.
//   --trigger <markerFile> connect, START_SESSION (this is what makes the server
//                          sweep for orphans), write the marker, sleep 1 s, close the
//                          pipe, exit. Deliberately no END_SESSION (see below).
//   --echo <sessionId>     raw ECHO probe. Prints "ECHO=<value>"; 0 means the
//                          session no longer exists on the server.
//   --hold-commit <marker> <active|idle> [seconds]
//                          Manufacture the maintenance gate input state for the
//                          G-3 / G-5 reverse criteria: composing == 0, input
//                          history present, session resident. "active" refreshes
//                          the last keystroke every 1500 ms (idle < 10 s => the
//                          gate must stay closed), "idle" stops typing (idle >
//                          10 s => the gate must open). The composition state
//                          is proven by "status.composing=" in the IPC
//                          response body, not by a status text.
//   --help
//
// Why --trigger does not send END_SESSION:
//   The server logs "remove_session active presence=absent" when the ACTIVE session
//   ends for a reason other than an orphan reclaim. The trigger session becomes the
//   active session (RimeWithWeasel.cpp AddSession ends with m_active_session = ipc_id),
//   so a graceful END_SESSION from the trigger would emit exactly that line and
//   would make the "presence=absent must not appear" judgement meaningless.
//   Closing the pipe without END_SESSION keeps the judgement clean; the trigger
//   session then becomes an orphan itself and is swept by the next session.
//
// This file must stay pure ASCII (the project has no /utf-8 guarantee for every
// toolchain, and Chinese literals would be mojibake).

#include "stdafx.h"

namespace {

const DWORD kCmdEcho = 0x8001;
const DWORD kCmdStartSession = 0x8002;
const DWORD kCmdEndSession = 0x8003;
const DWORD kCmdProcessKeyEvent = 0x8004;
// include/WeaselIPC.h, enum WEASEL_IPC_COMMAND: ECHO is WM_APP+1, so
// COMMIT_COMPOSITION is WM_APP+10 = 0x800B.
const DWORD kCmdCommitComposition = 0x800B;

// ibus::Keycode::Left (include/KeyEvent.h). The refresh key has to
//  - refresh m_last_input_tick on the server (RimeWithWeasel.cpp:715-717:
//    ANY key does that), and
//  - NOT create a composition (librime editor.cc:46-62 only looks at key
//    bindings while ctx->IsComposing()).
// Shift is unusable (ascii_composer.cc:79-104 toggles the ascii mode within
// 500 ms) and Escape is unusable (it would clear the composition and hide a
// failed commit).
const UINT kKeyLeft = 0xFF51;

#pragma pack(push, 1)
struct PipeHeader {
  DWORD msg;
  DWORD wparam;
  DWORD lparam;
};
#pragma pack(pop)

std::wstring CurrentUserName() {
  DWORD len = 0;
  GetUserNameW(NULL, &len);
  if (len == 0)
    return std::wstring();
  std::wstring name(len, L'\0');
  DWORD copied = len;
  if (!GetUserNameW(&name[0], &copied))
    return std::wstring();
  // GetUserNameW is not consistent about whether pnSize comes back with or
  // without the terminating NUL, so cut at the first NUL instead of trusting
  // the counter. A stray NUL inside the name would silently truncate the pipe
  // path (and every message that embeds it).
  const size_t n = ::wcsnlen(name.c_str(), name.size());
  name.resize(n);
  return name;
}

// Same shape as weasel::GetPipeName() in include/WeaselIPC.h.
std::wstring PipeName() {
  return L"\\\\.\\pipe\\" + CurrentUserName() + L"\\WeaselNamedPipe";
}

// Lower cased exe name of this process, exactly like weasel::ClientImpl::_InitializeClientInfo
// does (the server lower cases it again before comparing).
std::wstring ClientAppName() {
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

// Diagnostic dump that bypasses stdio completely: prints the size and the code
// units of a wide string as ASCII, so an embedded NUL is visible as a 0000 entry.
void DumpNarrow(const char* tag, const std::wstring& v) {
  std::string s = tag;
  char num[32];
  sprintf_s(num, "%u", static_cast<unsigned long>(v.size()));
  s += " size=";
  s += num;
  s += " codes=";
  for (size_t i = 0; i < v.size(); ++i) {
    char h[8];
    sprintf_s(h, "%04X", static_cast<unsigned>(v[i]));
    s += h;
    s += ' ';
  }
  s += "\n";
  DWORD w = 0;
  ::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), s.data(),
              static_cast<DWORD>(s.size()), &w, NULL);
}

class IpcClient {
 public:
  ~IpcClient() { Close(); }

  bool Connect(std::wstring* err) {
    const std::wstring name = PipeName();
    for (int attempt = 0; attempt < 30; ++attempt) {
      HANDLE h = ::CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, 0, NULL);
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
        *err = L"CreateFileW failed on [" + name + L"] len=" +
               std::to_wstring(name.size()) + L" error=" + std::to_wstring(e);
        DumpNarrow("DBG_NAME", name);
        DumpNarrow("DBG_ERR", *err);
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

  bool Transact(DWORD msg, DWORD wparam, DWORD lparam, const std::wstring& body,
                DWORD* result, std::wstring* err) {
    std::wstring ignored;
    return Transact(msg, wparam, lparam, body, result, &ignored, err);
  }

  // Same, but also returns the response body: the trailing UTF-16LE bytes of
  // the single pipe message after the leading DWORD result. That body is what
  // carries "status.composing=" (RimeWithWeasel.cpp:2061-2063), i.e. the self
  // proof that the composition is really gone.
  bool Transact(DWORD msg, DWORD wparam, DWORD lparam, const std::wstring& body,
                DWORD* result, std::wstring* resp_body, std::wstring* err) {
    if (pipe_ == INVALID_HANDLE_VALUE) {
      *err = L"not connected";
      return false;
    }
    resp_body->clear();
    const size_t body_bytes = body.size() * sizeof(wchar_t);
    std::vector<char> payload(sizeof(PipeHeader) + body_bytes);
    PipeHeader header;
    header.msg = msg;
    header.wparam = wparam;
    header.lparam = lparam;
    memcpy(payload.data(), &header, sizeof(header));
    if (body_bytes)
      memcpy(payload.data() + sizeof(header), body.data(), body_bytes);

    DWORD written = 0;
    if (!::WriteFile(pipe_, payload.data(), static_cast<DWORD>(payload.size()),
                     &written, NULL)) {
      *err = L"WriteFile failed, error=" + std::to_wstring(::GetLastError());
      return false;
    }

    // One ReadFile of a big buffer reads exactly one message: the leading DWORD is
    // the result, any trailing bytes are the optional response body.
    std::vector<char> resp(64 * 1024);
    DWORD got = 0;
    if (!::ReadFile(pipe_, resp.data(), static_cast<DWORD>(resp.size()), &got, NULL)) {
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
    if (got > sizeof(DWORD)) {
      const size_t wchars = (got - sizeof(DWORD)) / sizeof(wchar_t);
      resp_body->assign(
          reinterpret_cast<const wchar_t*>(resp.data() + sizeof(DWORD)), wchars);
    }
    return true;
  }

  bool StartSession(DWORD* ipc_id, std::wstring* err) {
    // Same body as weasel::ClientImpl::_WriteClientInfo(). The client_pid line is
    // the fact source for ADR 0067: the server only reclaims a session whose owner
    // pid is known and provably dead.
    std::wstring body;
    body += L"action=session\n";
    body += L"session.client_app=" + ClientAppName() + L"\n";
    body += L"session.client_type=tsf\n";
    body += L"session.client_pid=" + std::to_wstring(::GetCurrentProcessId()) + L"\n";
    body += L".\n";
    DWORD result = 0;
    if (!Transact(kCmdStartSession, 0, 0, body, &result, err))
      return false;
    *ipc_id = result;
    return true;
  }

  // Key event packing is the same as weasel::KeyEvent / WeaselIPC.h:
  // weasel::KeyEvent is {UINT keycode:16; UINT mask:16;}, so the packed 32 bit value
  // is keycode | (mask << 16). The server unpacks it with KeyEvent(wParam) and feeds
  // it to rime_api->process_key(session_id, keycode, expand_ibus_modifier(mask)).
  bool SendKey(DWORD ipc_id, UINT keycode, UINT mask, std::wstring* err) {
    return SendKey(ipc_id, keycode, mask, NULL, err);
  }

  // Same, but also returns the response body ("status.composing=" ...).
  bool SendKey(DWORD ipc_id, UINT keycode, UINT mask, std::wstring* resp_body,
               std::wstring* err) {
    const DWORD packed = (keycode & 0xFFFFu) | ((mask & 0xFFFFu) << 16);
    DWORD result = 0;
    if (resp_body == NULL)
      return Transact(kCmdProcessKeyEvent, packed, ipc_id, std::wstring(), &result,
                      err);
    return Transact(kCmdProcessKeyEvent, packed, ipc_id, std::wstring(), &result,
                    resp_body, err);
  }

  // WEASEL_IPC_COMMIT_COMPOSITION: the server calls rime_api->commit_composition
  // and clears its composing mirror (RimeWithWeasel.cpp:762-773). The reply
  // carries no body (WeaselServerImpl.cpp:382-388) but always the DWORD result
  // (WeaselServerImpl.cpp:544), so the transaction stays balanced.
  bool CommitComposition(DWORD ipc_id, std::wstring* err) {
    DWORD result = 0;
    return Transact(kCmdCommitComposition, 0, ipc_id, std::wstring(), &result,
                    err);
  }

  bool EndSession(DWORD ipc_id, std::wstring* err) {
    DWORD result = 0;
    return Transact(kCmdEndSession, 0, ipc_id, std::wstring(), &result, err);
  }

 private:
  HANDLE pipe_ = INVALID_HANDLE_VALUE;
};

// Marker files hold plain ASCII (numbers and short names), so a narrow copy of
// the wide value is enough and keeps the file readable by any script.
std::string Narrow(const std::wstring& w) {
  if (w.empty())
    return std::string();
  const int need = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                          static_cast<int>(w.size()), NULL, 0,
                                          NULL, NULL);
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
  const BOOL ok = ::WriteFile(h, text.data(), static_cast<DWORD>(text.size()),
                              &written, NULL);
  const DWORD e = ::GetLastError();
  ::CloseHandle(h);
  if (!ok) {
    *err = L"WriteFile failed on " + path + L", error=" + std::to_wstring(e);
    return false;
  }
  return true;
}

// Reads "<key>0" / "<key>1" out of a response body. Returns 1, 0, or -1 when
// the field is not present (the server fills it only when
// rime_api->get_status worked, RimeWithWeasel.cpp:2055-2063).
int ParseBoolField(const std::wstring& body, const wchar_t* key) {
  const size_t pos = body.find(key);
  if (pos == std::wstring::npos)
    return -1;
  const size_t at = pos + ::wcslen(key);
  if (at >= body.size())
    return -1;
  if (body[at] == L'1')
    return 1;
  if (body[at] == L'0')
    return 0;
  return -1;
}

// ---------------------------------------------------------------------------
// --hold-commit: manufacture the maintenance gate input state for the G-3 /
// G-5 reverse criteria (ADR 0065, include/FluxingMaintenanceGate.h:67-84).
//
// Target state: composing == 0, has_input_history == true, session resident.
//   phase "active": one refresh key every 1500 ms => idle < 10 s => the gate
//                   must stay CLOSED (G-3, the negative case).
//   phase "idle"  : no key at all after the setup  => idle > 10 s => the gate
//                   must OPEN (G-5, the positive case).
//
// How the state is built:
//   1) START_SESSION
//   2) type letters => the engine holds an unfinished composition
//      (status.composing=1 in the response body is the proof)
//   3) WEASEL_IPC_COMMIT_COMPOSITION => the composition is committed
//   4) one refresh key => the server re-reads the REAL engine state
//      (RimeWithWeasel.cpp:740 _SyncSessionComposing) and the response body
//      proves status.composing=0.
// The proof comes from the response body, never from a status text: that body
// is produced by the same rime_api->get_status call that feeds the gate.
//
// The refresh key is ibus::Left: it refreshes m_last_input_tick (any key
// does, RimeWithWeasel.cpp:715-717) and cannot create a composition
// (librime editor.cc:46-62 handles keys only while ctx->IsComposing()).
//
// Marker files:
//   <marker>        written once: ipc_id, pid, phase, composing flags
//   <marker>.state  rewritten on every refresh: last_input_tick_ms
//   <marker>.pause  when it exists the active phase stops refreshing, so the
//                   idle grows and one process can cover both phases
//
// Never sends END_SESSION: the session has to stay resident.
//
// Exit codes: 0 ok, 1 connect/protocol error, 2 usage, 3 the state could not
// be manufactured.
// ---------------------------------------------------------------------------
int RunHoldCommit(const std::wstring& marker, const std::wstring& phase,
                  int seconds) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ERROR connect failed: %s\n", err.c_str());
    return 1;
  }
  DWORD ipc_id = 0;
  if (!client.StartSession(&ipc_id, &err)) {
    wprintf(L"ERROR start_session failed: %s\n", err.c_str());
    return 1;
  }
  if (ipc_id == 0) {
    wprintf(L"ERROR start_session returned ipc_id=0\n");
    return 1;
  }
  wprintf(L"HOLDCOMMIT_SESSION ipc_id=%lu pid=%lu phase=%s\n",
          static_cast<unsigned long>(ipc_id),
          static_cast<unsigned long>(::GetCurrentProcessId()), phase.c_str());
  fflush(stdout);

  // (2) type letters until the engine really holds a composition.
  std::wstring resp;
  const char* keys = "fluxingorphan";
  int composing = -1;
  int ascii_mode = -1;
  for (int round = 0; round < 2 && composing != 1; ++round) {
    for (const char* p = keys; *p; ++p) {
      if (!client.SendKey(ipc_id,
                          static_cast<UINT>(static_cast<unsigned char>(*p)), 0,
                          &resp, &err)) {
        wprintf(L"ERROR key event failed: %s\n", err.c_str());
        return 1;
      }
      ::Sleep(30);
      composing = ParseBoolField(resp, L"status.composing=");
      ascii_mode = ParseBoolField(resp, L"status.ascii_mode=");
    }
  }
  wprintf(L"COMPOSING_AFTER_TYPING=%d ASCII_MODE=%d\n", composing, ascii_mode);
  fflush(stdout);
  if (composing != 1) {
    wprintf(L"ERROR the engine holds no composition => the state cannot be manufactured\n");
    return 3;
  }

  // (3) commit the composition.
  if (!client.CommitComposition(ipc_id, &err)) {
    wprintf(L"ERROR commit_composition failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"COMMIT_SENT=1\n");
  fflush(stdout);

  // (4) one refresh key: the server re-reads the real engine state and the
  // response body proves that the composition is really gone.
  int composing_after = -1;
  for (int i = 0; i < 3 && composing_after != 0; ++i) {
    if (!client.SendKey(ipc_id, kKeyLeft, 0, &resp, &err)) {
      wprintf(L"ERROR refresh key failed: %s\n", err.c_str());
      return 1;
    }
    ::Sleep(80);
    composing_after = ParseBoolField(resp, L"status.composing=");
  }
  wprintf(L"COMPOSING_AFTER_COMMIT=%d\n", composing_after);
  fflush(stdout);
  if (composing_after != 0) {
    wprintf(L"ERROR the composition survived the commit => the state cannot be manufactured\n");
    return 3;
  }

  int64_t last_input_tick = static_cast<int64_t>(::GetTickCount64());
  const std::wstring pause_file = marker + L".pause";
  const std::wstring state_file = marker + L".state";

  std::vector<std::pair<std::wstring, std::wstring> > kv;
  kv.push_back(std::make_pair(std::wstring(L"ipc_id"), std::to_wstring(ipc_id)));
  kv.push_back(std::make_pair(std::wstring(L"pid"),
                               std::to_wstring(::GetCurrentProcessId())));
  kv.push_back(std::make_pair(std::wstring(L"phase"), phase));
  kv.push_back(std::make_pair(std::wstring(L"composing_after_commit"),
                               std::to_wstring(composing_after)));
  kv.push_back(std::make_pair(std::wstring(L"last_input_tick_ms"),
                               std::to_wstring(last_input_tick)));
  if (!WriteMarker(marker, kv, &err)) {
    wprintf(L"ERROR marker failed: %s\n", err.c_str());
    return 1;
  }
  std::vector<std::pair<std::wstring, std::wstring> > skv;
  skv.push_back(std::make_pair(std::wstring(L"last_input_tick_ms"),
                               std::to_wstring(last_input_tick)));
  skv.push_back(std::make_pair(std::wstring(L"composing"),
                               std::to_wstring(composing_after)));
  std::wstring serr;
  WriteMarker(state_file, skv, &serr);

  wprintf(L"HOLDCOMMIT_STARTED ipc_id=%lu pid=%lu last_input_tick_ms=%lld\n",
          static_cast<unsigned long>(ipc_id),
          static_cast<unsigned long>(::GetCurrentProcessId()),
          static_cast<long long>(last_input_tick));
  fflush(stdout);

  // ---------------------------------------------------------- steady state --
  const bool refresh = (phase != L"idle");
  const int64_t deadline =
      seconds > 0 ? last_input_tick + static_cast<int64_t>(seconds) * 1000 : 0;
  for (;;) {
    ::Sleep(1500);
    const int64_t now = static_cast<int64_t>(::GetTickCount64());
    if (deadline != 0 && now >= deadline) {
      wprintf(L"HOLDCOMMIT_DONE idle_ms=%lld\n",
              static_cast<long long>(now - last_input_tick));
      fflush(stdout);
      return 0;
    }
    if (!refresh)
      continue;
    if (::GetFileAttributesW(pause_file.c_str()) != INVALID_FILE_ATTRIBUTES)
      continue;
    if (!client.SendKey(ipc_id, kKeyLeft, 0, &resp, &err)) {
      wprintf(L"WARN refresh key failed: %s\n", err.c_str());
      fflush(stdout);
      continue;
    }
    const int c = ParseBoolField(resp, L"status.composing=");
    if (c != 0) {
      wprintf(L"ERROR a composition reappeared in the steady state (status.composing=%d)\n",
              c);
      fflush(stdout);
      return 3;
    }
    last_input_tick = static_cast<int64_t>(::GetTickCount64());
    std::vector<std::pair<std::wstring, std::wstring> > st;
    st.push_back(std::make_pair(std::wstring(L"last_input_tick_ms"),
                                std::to_wstring(last_input_tick)));
    st.push_back(std::make_pair(std::wstring(L"composing"), std::to_wstring(c)));
    std::wstring werr;
    WriteMarker(state_file, st, &werr);
  }
}

void PrintHelp() {
  wprintf(L"TestOrphanReclaimE2E -- synthetic WeaselServer IPC client (ADR 0067)\n");
  wprintf(L"\n");
  wprintf(L"  --hold <markerFile>     connect, START_SESSION, type letters, write\n");
  wprintf(L"                          ipc_id=<n> and pid=<n> to the marker, then sleep\n");
  wprintf(L"                          forever (killed by the parent script).\n");
  wprintf(L"  --hold-commit <marker> <active|idle> [seconds]\n");
  wprintf(L"                          manufacture the maintenance gate input state:\n");
  wprintf(L"                          composing==0, input history, session resident.\n");
  wprintf(L"                          active = refresh key every 1500 ms (idle < 10 s)\n");
  wprintf(L"                          idle   = no key at all      (idle > 10 s)\n");
  wprintf(L"                          [seconds] = exit after N s of steady state;\n");
  wprintf(L"                          without it the process runs until it is killed.\n");
  wprintf(L"                          Uses marker, marker.state and marker.pause.\n");
  wprintf(L"  --trigger <markerFile>  connect, START_SESSION (triggers the server side\n");
  wprintf(L"                          orphan sweep), write ipc_id=<n> to the marker,\n");
  wprintf(L"                          sleep 1 s, close the pipe, exit. No END_SESSION.\n");
  wprintf(L"  --echo <sessionId>      raw ECHO probe. Prints ECHO=<value>; 0 means the\n");
  wprintf(L"                          session no longer exists on the server.\n");
  wprintf(L"  --help                  this text.\n");
}

int RunEcho(DWORD session_id) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ECHO=ERR connect failed: %s\n", err.c_str());
    return 1;
  }
  DWORD result = 0;
  if (!client.Transact(kCmdEcho, 0, session_id, std::wstring(), &result, &err)) {
    wprintf(L"ECHO=ERR %s\n", err.c_str());
    return 1;
  }
  wprintf(L"ECHO=%lu\n", static_cast<unsigned long>(result));
  fflush(stdout);
  return 0;
}

int RunHold(const std::wstring& marker) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ERROR connect failed: %s\n", err.c_str());
    return 1;
  }
  DWORD ipc_id = 0;
  if (!client.StartSession(&ipc_id, &err)) {
    wprintf(L"ERROR start_session failed: %s\n", err.c_str());
    return 1;
  }
  if (ipc_id == 0) {
    wprintf(L"ERROR start_session returned ipc_id=0\n");
    return 1;
  }
  // Type plain letters so the server side rime session ends up holding an
  // uncommitted composition (no space, no enter). The keycode of a letter is its
  // ASCII value, which is what the TSF front end produces for a letter key
  // (WeaselTSF/KeyEvent.cpp: ConvertKeyEvent resolves the produced character).
  const char* keys = "fluxingorphan";
  for (const char* p = keys; *p; ++p) {
    if (!client.SendKey(ipc_id, static_cast<UINT>(static_cast<unsigned char>(*p)), 0,
                        &err)) {
      wprintf(L"ERROR key event failed: %s\n", err.c_str());
      return 1;
    }
    ::Sleep(30);
  }
  std::vector<std::pair<std::wstring, std::wstring> > kv;
  kv.push_back(std::make_pair(std::wstring(L"ipc_id"), std::to_wstring(ipc_id)));
  kv.push_back(std::make_pair(std::wstring(L"pid"),
                               std::to_wstring(::GetCurrentProcessId())));
  if (!WriteMarker(marker, kv, &err)) {
    wprintf(L"ERROR marker failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"HOLD_STARTED ipc_id=%lu pid=%lu\n", static_cast<unsigned long>(ipc_id),
          static_cast<unsigned long>(::GetCurrentProcessId()));
  fflush(stdout);
  // Never leave gracefully: the parent script kills this process so that the
  // server never sees an END_SESSION for this session.
  //
  // Keep typing (one key every 1500 ms) so that the maintenance gate facts
  // stay fresh: "the last keystroke" (m_last_input_tick) and "an unfinished
  // composition" (m_session_composing).  When the parent script creates
  // "<marker>.pause" the typing stops but the process stays alive => the
  // session stays resident while the idle grace (10 s) runs out.  That is
  // exactly the state the B-1 probe needs: a residual session that still holds
  // the gate because of its composition.
  const std::wstring pause_file = marker + L".pause";
  int tick = 0;
  for (;;) {
    ::Sleep(1500);
    if (::GetFileAttributesW(pause_file.c_str()) != INVALID_FILE_ATTRIBUTES)
      continue;
    const char key = static_cast<char>('a' + (tick % 26));
    ++tick;
    if (!client.SendKey(ipc_id, static_cast<UINT>(static_cast<unsigned char>(key)), 0,
                        &err)) {
      // The server may have dropped the session (maintenance finalize).
      // Stay alive anyway: the parent script kills this process.
      wprintf(L"WARN key event failed: %s\n", err.c_str());
      fflush(stdout);
    }
  }
}

int RunTrigger(const std::wstring& marker) {
  IpcClient client;
  std::wstring err;
  if (!client.Connect(&err)) {
    wprintf(L"ERROR connect failed: %s\n", err.c_str());
    return 1;
  }
  DWORD ipc_id = 0;
  if (!client.StartSession(&ipc_id, &err)) {
    wprintf(L"ERROR start_session failed: %s\n", err.c_str());
    return 1;
  }
  if (ipc_id == 0) {
    wprintf(L"ERROR start_session returned ipc_id=0\n");
    return 1;
  }
  std::vector<std::pair<std::wstring, std::wstring> > kv;
  kv.push_back(std::make_pair(std::wstring(L"ipc_id"), std::to_wstring(ipc_id)));
  if (!WriteMarker(marker, kv, &err)) {
    wprintf(L"ERROR marker failed: %s\n", err.c_str());
    return 1;
  }
  wprintf(L"TRIGGER_STARTED ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
  fflush(stdout);
  ::Sleep(1000);
  // No END_SESSION on purpose, see the file header.
  client.Close();
  wprintf(L"TRIGGER_DONE ipc_id=%lu\n", static_cast<unsigned long>(ipc_id));
  fflush(stdout);
  return 0;
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
      wprintf(L"ERROR usage: TestOrphanReclaimE2E --echo <sessionId>\n");
      return 2;
    }
    // WeaselSessionId is an unsigned 32 bit value, so it does not fit into an
    // int; parse it as an unsigned long.
    const DWORD session_id =
        static_cast<DWORD>(::wcstoul(argv[2], NULL, 10));
    return RunEcho(session_id);
  }
  if (mode == L"--hold") {
    if (argc < 3) {
      wprintf(L"ERROR usage: TestOrphanReclaimE2E --hold <markerFile>\n");
      return 2;
    }
    return RunHold(argv[2]);
  }
  if (mode == L"--hold-commit") {
    if (argc < 4) {
      wprintf(L"ERROR usage: TestOrphanReclaimE2E --hold-commit <markerFile> <active|idle> [seconds]\n");
      return 2;
    }
    const std::wstring phase = argv[3];
    if (phase != L"active" && phase != L"idle") {
      wprintf(L"ERROR usage: the phase must be active or idle, got: %s\n",
              phase.c_str());
      return 2;
    }
    int seconds = 0;
    if (argc >= 5) {
      seconds = static_cast<int>(::wcstol(argv[4], NULL, 10));
      if (seconds < 0)
        seconds = 0;
    }
    return RunHoldCommit(argv[2], phase, seconds);
  }
  if (mode == L"--trigger") {
    if (argc < 3) {
      wprintf(L"ERROR usage: TestOrphanReclaimE2E --trigger <markerFile>\n");
      return 2;
    }
    return RunTrigger(argv[2]);
  }
  wprintf(L"ERROR unknown mode: %s\n", mode.c_str());
  PrintHelp();
  return 2;
}
