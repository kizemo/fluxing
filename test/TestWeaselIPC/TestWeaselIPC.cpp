// TestWeaselIPC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <WeaselIPC.h>
#include <RimeWithWeasel.h>

#include <boost/interprocess/streams/bufferstream.hpp>
using namespace boost::interprocess;

#include <functional>
#include <iostream>
#include <memory>
#include <string>

CAppModule _Module;

int console_main();
int client_main();
int server_main();
int verify_quickpanel_main();
int verify_focusout_flags_main();
int verify_shift_body_main();
int verify_tsf_profile_main(bool allow_profile_switch,
                            bool allow_remote_session);

// Reads an opt in flag from the environment. The value has to look like a
// yes ("1", "y", "yes", "true"), anything else is a no.
bool env_opt_in(const wchar_t* name) {
  wchar_t buf[16] = {0};
  DWORD n = GetEnvironmentVariableW(name, buf, 16);
  if (n == 0 || n >= 16)
    return false;
  if (!wcscmp(buf, L"1") || !wcscmp(buf, L"y") || !wcscmp(buf, L"Y"))
    return true;
  if (!wcscmp(buf, L"yes") || !wcscmp(buf, L"YES"))
    return true;
  if (!wcscmp(buf, L"true") || !wcscmp(buf, L"TRUE"))
    return true;
  return false;
}

// usage: TestWeaselIPC.exe [/start | /stop | /console]

int _tmain(int argc, _TCHAR* argv[]) {
  if (argc == 1)  // no args
  {
    return client_main();
  } else if (argc > 1 && !wcscmp(L"/start", argv[1])) {
    return server_main();
  } else if (argc > 1 && !wcscmp(L"/stop", argv[1])) {
    weasel::Client client;
    if (!client.Connect()) {
      std::cerr << "server not running." << std::endl;
      return 0;
    }
    client.ShutdownServer();
    return 0;
  } else if (argc > 1 && !wcscmp(L"/console", argv[1])) {
    return console_main();
    return 0;
  } else if (argc > 1 && !wcscmp(L"/verify-shift-body", argv[1])) {
    return verify_shift_body_main();
  } else if (argc > 1 && !wcscmp(L"/verify-quickpanel", argv[1])) {
    return verify_quickpanel_main();
  } else if (argc > 1 && !wcscmp(L"/verify-focusout-flags", argv[1])) {
    return verify_focusout_flags_main();
  } else if (argc > 1 && !wcscmp(L"/verify-tsf-profile", argv[1])) {
    // spec 080 (crash guard): this mode really switches the active input
    // profile, which is not what a user does and which was observed to
    // collapse the desktop input stack. It refuses to run unless the
    // caller opts in explicitly.
    bool allow_profile_switch =
        env_opt_in(L"FLUXING_VERIFY_ALLOW_PROFILE_SWITCH");
    bool allow_remote_session =
        env_opt_in(L"FLUXING_VERIFY_ALLOW_REMOTE_SESSION");
    for (int i = 2; i < argc; ++i) {
      if (!wcscmp(argv[i], L"--allow-profile-switch"))
        allow_profile_switch = true;
      else if (!wcscmp(argv[i], L"--allow-remote-session"))
        allow_remote_session = true;
    }
    return verify_tsf_profile_main(allow_profile_switch, allow_remote_session);
  }

  return -1;
}

bool launch_server() {
  int ret = (int)ShellExecute(NULL, L"open", L"TestWeaselIPC.exe", L"/start",
                              NULL, SW_NORMAL);
  if (ret <= 32) {
    std::cerr << "failed to launch server." << std::endl;
    return false;
  }
  return true;
}

bool read_buffer(LPWSTR buffer, UINT length, LPWSTR dest) {
  wbufferstream bs(buffer, length);
  bs.read(dest, WEASEL_IPC_BUFFER_LENGTH);
  return bs.good();
}

const char* wcstomb(const wchar_t* wcs) {
  const int buffer_len = 8192;
  static char buffer[buffer_len];
  WideCharToMultiByte(CP_OEMCP, NULL, wcs, -1, buffer, buffer_len, NULL, FALSE);
  return buffer;
}

int console_main() {
  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "failed to connect to server." << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "failed to start session." << std::endl;
    return -3;
  }

  while (std::cin.good()) {
    int ch = std::cin.get();
    if (!std::cin.good())
      break;
    bool eaten = client.ProcessKeyEvent(weasel::KeyEvent(ch, 0));
    std::cout << "server replies: " << eaten << std::endl;
    if (eaten) {
      WCHAR response[WEASEL_IPC_BUFFER_LENGTH];
      bool ret = client.GetResponseData(
          std::bind<bool>(read_buffer, std::placeholders::_1,
                          std::placeholders::_2, std::ref(response)));
      std::cout << "get response data: " << ret << std::endl;
      std::cout << "buffer reads: " << std::endl
                << wcstomb(response) << std::endl;
    }
  }

  client.EndSession();

  return 0;
}

int client_main() {
  // launch_server();
  Sleep(1000);
  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "failed to connect to server." << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "failed to login." << std::endl;
    return -3;
  }
  bool eaten = client.ProcessKeyEvent(weasel::KeyEvent(L'a', 0));
  std::cout << "server replies: " << eaten << std::endl;
  if (eaten) {
    WCHAR response[WEASEL_IPC_BUFFER_LENGTH];
    bool ret = client.GetResponseData(
        std::bind<bool>(read_buffer, std::placeholders::_1,
                        std::placeholders::_2, std::ref(response)));
    std::cout << "get response data: " << ret << std::endl;
    std::cout << "buffer reads: " << std::endl
              << wcstomb(response) << std::endl;
  }
  client.EndSession();

  return 0;
}

// spec 080: end-to-end check for "the QuickPanel shows when the user
// switches the active input profile to Fluxing".
//
// The client impersonates exactly what the TSF notification thread does:
//   1. a plain FocusIn              -> the server makes this session active
//   2. FocusIn(PROFILE_ACTIVATED)   -> the server shows the QuickPanel
// and then checks that the panel window really exists and is visible.
//
// The panel is a singleton top level window titled "Fluxing QuickPanel",
// so the client can observe it with FindWindowW.
struct auto_hide_result {
  bool hidden = false;          // the panel went away
  bool survived_grace = false;  // it was still up after the grace period
};

// Waits for the QuickPanel to hide itself while the mouse stays outside
// of it. The panel hides after 2s of grace period plus 1.5s of accumulated
// outside time (QuickPanelDialog.cpp: kShowGraceMs and the WM_TIMER id 2
// branch), so it has to be visible right after the grace period and gone
// a moment later. The mouse is moved out of the way and put back after.
auto_hide_result wait_auto_hide(std::function<bool()> panel_visible) {
  auto_hide_result result;
  POINT old_cursor = {0, 0};
  GetCursorPos(&old_cursor);
  SetCursorPos(10, 10);
  for (int i = 0; i < 12; ++i) {  // 1200ms, still inside the grace period
    Sleep(100);
    if (!panel_visible()) {
      result.hidden = true;
      break;
    }
  }
  if (!result.hidden) {
    result.survived_grace = true;
    for (int i = 0; i < 60; ++i) {  // up to 6s more
      Sleep(100);
      if (!panel_visible()) {
        result.hidden = true;
        break;
      }
    }
  }
  SetCursorPos(old_cursor.x, old_cursor.y);
  return result;
}

// ADR 0012: the Shift IPC has to answer with a response body.
//
// The TSF reads the pipe buffer right after every key event (OnTestKeyDown
// and OnTestKeyUp call _UpdateComposition, which calls DoEditSession). If
// the server does not write a body, the client keeps the previous response
// and parses it again, so a bare Shift would never reach the screen and the
// language bar would never learn the new input mode. This mode proves that a
// Shift request really does carry a body back to the client.
int verify_shift_body_main() {
  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "[SHIFT] connect failed" << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "[SHIFT] session failed" << std::endl;
    return -3;
  }

  // Identity check: the mock server answers a key event with the
  // "Greeting=" marker, while a real WeaselServer.exe answers with a full
  // "action=status,ctx,config" block. Without this check the test would
  // silently measure whatever process happens to own the named pipe - the
  // L30 class of bug, where a degraded integration test still looks green.
  std::wstring probe_body;
  client.ProcessKeyEvent(weasel::KeyEvent(L'a', 0));
  client.GetResponseData([&probe_body](LPWSTR buffer, DWORD length) -> bool {
    probe_body.assign(buffer, length);
    return true;
  });
  bool talking_to_mock = probe_body.find(L"Greeting=") != std::wstring::npos;
  if (!talking_to_mock) {
    std::cerr << "[SHIFT] FAIL: the named pipe is served by another "
                 "process, not the TestWeaselIPC mock server. A real "
                 "WeaselServer.exe is probably running; stop it and re-run."
              << std::endl;
    client.EndSession();
    return -6;
  }

  std::wstring down_body;
  client.ShiftDown(true);
  bool got_down =
      client.GetResponseData([&down_body](LPWSTR buffer, DWORD length) -> bool {
        down_body.assign(buffer, length);
        return true;
      });
  bool down_has_status =
      down_body.find(L"status.ascii_mode=0") != std::wstring::npos;

  std::wstring up_body;
  client.ShiftUp(true);
  bool got_up =
      client.GetResponseData([&up_body](LPWSTR buffer, DWORD length) -> bool {
        up_body.assign(buffer, length);
        return true;
      });
  bool up_has_commit = up_body.find(L"commit=Fluxing") != std::wstring::npos;
  bool up_has_status =
      up_body.find(L"status.ascii_mode=1") != std::wstring::npos;

  client.EndSession();

  std::cout << "[SHIFT] down=" << (got_down ? 1 : 0)
            << " down_status=" << (down_has_status ? 1 : 0)
            << " up=" << (got_up ? 1 : 0)
            << " up_commit=" << (up_has_commit ? 1 : 0)
            << " up_status=" << (up_has_status ? 1 : 0) << std::endl;
  if (got_down && down_has_status && got_up && up_has_commit && up_has_status) {
    std::cout << "[SHIFT] PASS: the Shift IPC carries a response body"
              << std::endl;
    return 0;
  }
  std::cerr << "[SHIFT] FAIL" << std::endl;
  return -4;
}

int verify_quickpanel_main() {
  auto panel_hwnd = []() -> HWND {
    // class "FluxingQuickPanel_v3", title "Fluxing QuickPanel"
    return FindWindowW(L"FluxingQuickPanel_v3", L"Fluxing QuickPanel");
  };
  auto panel_visible = [&panel_hwnd]() -> bool {
    HWND h = panel_hwnd();
    return h != NULL && IsWindowVisible(h);
  };

  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "[QP] connect failed" << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "[QP] session failed" << std::endl;
    return -3;
  }

  // precondition: the panel must be hidden before the notification
  client.FocusOut();
  Sleep(500);
  if (panel_visible()) {
    std::cerr << "[QP] FAIL panel already visible before the notification"
              << std::endl;
    client.EndSession();
    return -4;
  }

  // step 1: this session becomes the server's active session
  client.FocusIn();
  Sleep(300);

  // step 2: the profile-activated notification (what the TSF sends when
  // the user switches the active input profile to Fluxing)
  client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED);
  Sleep(800);

  HWND panel = panel_hwnd();
  bool shown = (panel != NULL && IsWindowVisible(panel));
  std::cout << "[QP] panel hwnd=" << (panel ? (void*)panel : (void*)0)
            << " visible=" << (shown ? 1 : 0) << std::endl;

  // step 3: the panel must hide itself again when the mouse stays out of
  // it. This is the regression check for the v0.21.0.5 fix: Show() kills
  // the hover polling timer (id 2) in Hide(), and the reuse path used to
  // return without restarting it, so every panel shown after the first
  // show/hide cycle stayed on the screen for good.
  auto_hide_result first = wait_auto_hide(panel_visible);
  std::cout << "[QP] first auto-hide hidden=" << (first.hidden ? 1 : 0)
            << " survived_grace=" << (first.survived_grace ? 1 : 0)
            << std::endl;

  // step 4: show it again. The window still exists, so this is the reuse
  // path, and it has to auto hide as well.
  client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED);
  Sleep(800);
  bool shown_again = panel_visible();
  auto_hide_result second = wait_auto_hide(panel_visible);
  std::cout << "[QP] second auto-hide shown=" << (shown_again ? 1 : 0)
            << " hidden=" << (second.hidden ? 1 : 0)
            << " survived_grace=" << (second.survived_grace ? 1 : 0)
            << std::endl;

  // step 5: switching away hides the panel again
  client.FocusOut();
  Sleep(600);
  bool hidden = !panel_visible();

  client.EndSession();

  bool auto_hide_ok = first.hidden && first.survived_grace && shown_again &&
                      second.hidden && second.survived_grace;
  if (shown && hidden && auto_hide_ok) {
    std::cout << "[QP] PASS: profile-activated notification shows the "
                 "QuickPanel, it auto hides, and focus-out hides it"
              << std::endl;
    return 0;
  }
  std::cerr << "[QP] FAIL shown=" << shown << " hidden=" << hidden
            << " shown_again=" << shown_again
            << " auto_hide=" << (auto_hide_ok ? 1 : 0) << std::endl;
  return -5;
}

// spec 091: helpers for the FocusOut event kind test.
//
// The mock server and the client are the same binary, so the record file
// lives next to the exe: the directory is the same no matter what the
// current working directory happens to be.
std::wstring focusout_record_path() {
  wchar_t exe_path[MAX_PATH] = {0};
  DWORD exe_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
  if (exe_len == 0 || exe_len >= MAX_PATH)
    return std::wstring();
  std::wstring exe_dir(exe_path, exe_len);
  size_t slash = exe_dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos)
    return std::wstring();
  return exe_dir.substr(0, slash + 1) + L"focusout_param.txt";
}

std::string read_text_file(const std::wstring& path) {
  if (path.empty())
    return std::string();
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return std::string();
  char buf[4096];
  DWORD n = 0;
  std::string out;
  for (;;) {
    if (!ReadFile(h, buf, sizeof(buf), &n, NULL) || n == 0)
      break;
    out.append(buf, n);
  }
  CloseHandle(h);
  return out;
}

int count_lines(const std::string& text) {
  int n = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n')
      ++n;
  }
  return n;
}

std::string nth_line(const std::string& text, int index) {
  int cur = 0;
  size_t start = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '\n')
      continue;
    if (cur == index)
      return text.substr(start, i - start);
    ++cur;
    start = i + 1;
  }
  return std::string();
}

// Waits until the record file holds at least min_lines lines.
bool wait_focusout_lines(const std::wstring& path,
                         int min_lines,
                         int timeout_ms) {
  for (int i = 0; i < timeout_ms / 50; ++i) {
    if (count_lines(read_text_file(path)) >= min_lines)
      return true;
    Sleep(50);
  }
  return false;
}

// spec 091: the FocusOut event kinds must be distinguishable end to end.
//
// The mock server records the raw wParam of every FocusOut it receives into
// "focusout_param.txt" next to the exe, one decimal value per line. The
// client then sends two FocusOut messages - one plain ("the edit field
// lost focus") and one flagged ("the user switched to another input
// method") - and reads the recorded values back. If the two kinds travel
// as the same value, the server can never tell "switched away from
// Fluxing" apart from "lost focus", and the always-show guard swallows
// the hide request (spec 091 root cause).
//
// The pure decision function is asserted here as well, with exactly the
// same arguments the server passes, so production code and test share one
// source of truth (constitution R7).
int verify_focusout_flags_main() {
  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "[FOCUSOUT] connect failed" << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "[FOCUSOUT] session failed" << std::endl;
    return -3;
  }

  // Identity probe: the mock server answers a key event with the
  // "Greeting=" marker, while a real WeaselServer.exe answers with a full
  // "action=status,ctx,config" block. Without this check the test could
  // silently measure another process that owns the named pipe.
  std::wstring probe_body;
  client.ProcessKeyEvent(weasel::KeyEvent(L'a', 0));
  client.GetResponseData([&probe_body](LPWSTR buffer, DWORD length) -> bool {
    probe_body.assign(buffer, length);
    return true;
  });
  if (probe_body.find(L"Greeting=") == std::wstring::npos) {
    std::cerr << "[FOCUSOUT] FAIL: the named pipe is served by another "
                 "process, not the TestWeaselIPC mock server. A real "
                 "WeaselServer.exe is probably running; stop it and re-run."
              << std::endl;
    client.EndSession();
    return -6;
  }

  std::wstring record = focusout_record_path();
  if (record.empty()) {
    std::cerr << "[FOCUSOUT] FAIL: cannot resolve the exe directory"
              << std::endl;
    client.EndSession();
    return -7;
  }

  // Start from a clean record: a leftover file from an earlier run would
  // make the baseline depend on the previous execution.
  DeleteFileW(record.c_str());
  int baseline = count_lines(read_text_file(record));
  std::cout << "[FOCUSOUT] baseline lines=" << baseline << std::endl;

  // 1. the plain FocusOut ("the edit field lost focus") carries wParam 0
  client.FocusOut();
  bool got_plain = wait_focusout_lines(record, baseline + 1, 5000);
  std::string plain_line =
      got_plain ? nth_line(read_text_file(record), baseline) : std::string();

  // 2. the flagged FocusOut ("the user switched to another input method")
  //    carries WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED
  client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);
  bool got_flagged = wait_focusout_lines(record, baseline + 2, 5000);
  std::string flagged_line =
      got_flagged ? nth_line(read_text_file(record), baseline + 1)
                  : std::string();

  client.EndSession();

  // 3..8: the pure decision function, exactly as the server calls it
  const DWORD kDeactivated = WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED;
  bool plain_hides_when_not_always = ShouldHideQuickPanelOnFocusOut(0, false);
  bool plain_keeps_when_always = !ShouldHideQuickPanelOnFocusOut(0, true);
  bool deactivated_hides_when_not_always =
      ShouldHideQuickPanelOnFocusOut(kDeactivated, false);
  bool deactivated_hides_when_always =
      ShouldHideQuickPanelOnFocusOut(kDeactivated, true);
  bool unknown_bit_alone_keeps = !ShouldHideQuickPanelOnFocusOut(0x2u, true);
  bool unknown_bit_with_flag_hides =
      ShouldHideQuickPanelOnFocusOut(kDeactivated | 0x2u, true);

  bool plain_is_zero = (plain_line == "0");
  bool flagged_is_set = (flagged_line == std::to_string(kDeactivated));

  std::cout << "[FOCUSOUT] plain=[" << plain_line << "] flagged=["
            << flagged_line << "]" << std::endl;

  bool ok = plain_is_zero && flagged_is_set && got_plain && got_flagged &&
            plain_hides_when_not_always && plain_keeps_when_always &&
            deactivated_hides_when_not_always &&
            deactivated_hides_when_always && unknown_bit_alone_keeps &&
            unknown_bit_with_flag_hides;
  if (ok) {
    std::cout << "[FOCUSOUT] PASS: the FocusOut event kinds are "
                 "distinguishable and the hide decision follows them"
              << std::endl;
    return 0;
  }
  std::cerr << "[FOCUSOUT] FAIL plain_is_zero=" << plain_is_zero
            << " flagged_is_set=" << flagged_is_set
            << " got_plain=" << got_plain << " got_flagged=" << got_flagged
            << " pure=" << plain_hides_when_not_always
            << plain_keeps_when_always << deactivated_hides_when_not_always
            << deactivated_hides_when_always << unknown_bit_alone_keeps
            << unknown_bit_with_flag_hides << std::endl;
  return -4;
}

class TestRequestHandler : public weasel::RequestHandler {
 public:
  TestRequestHandler() : m_counter(0) {
    std::cerr << "handler ctor." << std::endl;
  }
  virtual ~TestRequestHandler() {
    std::cerr << "handler dtor: " << m_counter << std::endl;
  }
  virtual DWORD FindSession(DWORD session_id) override {
    std::cerr << "FindSession: " << session_id << std::endl;
    return (session_id <= m_counter ? session_id : 0);
  }
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0) override {
    std::cerr << "AddSession: " << m_counter + 1 << std::endl;
    return ++m_counter;
  }
  virtual DWORD RemoveSession(DWORD session_id) override {
    std::cerr << "RemoveClient: " << session_id << std::endl;
    return 0;
  }
  // ADR 0012: DWORD + override. The base virtual takes a DWORD
  // session_id; UINT is a different type on Windows (LLP64), so the
  // mock used to hide the base virtual instead of overriding it and
  // the server called the base's empty implementation - the key
  // event path was never exercised by the suite.
  virtual BOOL ProcessKeyEvent(weasel::KeyEvent keyEvent,
                               DWORD session_id,
                               EatLine eat) override {
    std::cerr << "ProcessKeyEvent: " << session_id
              << " keycode: " << keyEvent.keycode << " mask: " << keyEvent.mask
              << std::endl;
    eat(std::wstring(L"Greeting=Hello, 小狼毫.\n"));
    return TRUE;
  }

  // ADR 0012: the Shift handlers answer with a body, so the client can read
  // the fresh status (and the commit text of a selected candidate).
  virtual void ShiftDown(bool is_left, DWORD session_id, EatLine eat) override {
    std::cerr << "ShiftDown: " << session_id << std::endl;
    if (eat) {
      std::wstring msg = L"status.ascii_mode=0\n";
      eat(msg);
    }
  }
  virtual void ShiftUp(bool is_left, DWORD session_id, EatLine eat) override {
    std::cerr << "ShiftUp: " << session_id << std::endl;
    if (eat) {
      std::wstring msg = L"commit=Fluxing\nstatus.ascii_mode=1\n";
      eat(msg);
    }
  }

  // spec 091: the FocusOut event kinds must be distinguishable on the
  // wire. The handler records the raw wParam of every FocusOut it gets
  // into "focusout_param.txt" next to the exe, one decimal value per
  // line, so the client can read the values back and assert that
  // "switched away from Fluxing" and "the edit field lost focus" really
  // travel as different values.
  virtual void FocusOut(DWORD param, DWORD session_id) override {
    std::cerr << "FocusOut: " << session_id << " param=" << param << std::endl;
    std::wstring exe_path_buf;
    wchar_t exe_path[MAX_PATH] = {0};
    DWORD exe_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (exe_len == 0 || exe_len >= MAX_PATH)
      return;
    std::wstring exe_dir(exe_path, exe_len);
    size_t slash = exe_dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos)
      return;
    exe_dir = exe_dir.substr(0, slash + 1);
    std::wstring record = exe_dir + L"focusout_param.txt";
    // FILE_APPEND_DATA, not GENERIC_WRITE: a freshly opened GENERIC_WRITE
    // handle starts at file offset 0, so the second FocusOut overwrote the
    // first recorded line and the test could only ever observe one value.
    HANDLE h = CreateFileW(record.c_str(), FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
      return;
    std::string text = std::to_string(param) + "\n";
    DWORD written = 0;
    WriteFile(h, text.c_str(), static_cast<DWORD>(text.size()), &written, NULL);
    FlushFileBuffers(h);
    CloseHandle(h);
  }

 private:
  unsigned int m_counter;
};

int server_main() {
  HRESULT hRes = _Module.Init(NULL, GetModuleHandle(NULL));
  ATLASSERT(SUCCEEDED(hRes));

  weasel::Server server;
  // weasel::UI ui;
  // const std::unique_ptr<weasel::RequestHandler> handler(new
  // RimeWithWeaselHandler(&ui));
  const std::unique_ptr<weasel::RequestHandler> handler(new TestRequestHandler);

  server.SetRequestHandler(handler.get());
  if (!server.Start())
    return -4;
  std::cerr << "server running." << std::endl;
  int ret = server.Run();
  std::cerr << "server quitting." << std::endl;
  return ret;
}
