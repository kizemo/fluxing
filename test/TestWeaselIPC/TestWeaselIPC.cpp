// TestWeaselIPC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <WeaselIPC.h>
#include <stdlib.h>
#include <RimeWithWeasel.h>

#include <boost/interprocess/streams/bufferstream.hpp>
using namespace boost::interprocess;

#include <functional>
#include <iostream>
#include <memory>
#include <fstream>
#include <string>
// ADR 0031: console 会话门禁的判别性探针。生产代码(WeaselIPCServer)与本测试
// 共用 include/FluxingSessionPolicy.h 同一实现 (constitution R7)。
#include <FluxingDiagLog.h>
#include <FluxingSessionPolicy.h>

CAppModule _Module;

int console_main();
int verify_console_session_main();
int verify_own_ui_events_main();
int hold_presence_main(int hold_ms, int focusout_at_ms);
int client_main();
int server_main();
int verify_quickpanel_main();
int verify_quickpanel_presence_main();
int verify_focusout_flags_main();
int verify_focusin_flags_main();
int verify_shift_body_main();
int verify_delete_body_main();
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
  } else if (argc > 1 &&
             !wcscmp(L"/verify-quickpanel-presence", argv[1])) {
    // spec 097: the "is Fluxing present" state machine.
    // The production code and this test share the very same implementation
    // in include/WeaselIPC.h (constitution R7).
    return verify_quickpanel_presence_main();
  } else if (argc > 1 && !wcscmp(L"/verify-focusout-flags", argv[1])) {
    return verify_focusout_flags_main();
  } else if (argc > 1 && !wcscmp(L"/verify-focusin-flags", argv[1])) {
    return verify_focusin_flags_main();
  } else if (argc > 1 && !wcscmp(L"/verify-delete-body", argv[1])) {
    return verify_delete_body_main();
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
  } else if (argc > 1 && !wcscmp(L"/verify-console-session", argv[1])) {
    // ADR 0031 决策 4 的验收探针: console 会话门禁的判定真值表 +
    // 启动留痕是否真的落进诊断文件。只读判定与写自己的探针行,
    // 不启动/不杀任何 WeaselServer.exe。
    return verify_console_session_main();
  } else if (argc > 1 && !wcscmp(L"/verify-own-ui-events", argv[1])) {
    // ADR 0032: 自家 UI 进程(用户短语窗口)的 IME 生命周期不得影响快捷设置栏。
    // 生产代码与本测试共用 include/WeaselIPC.h 的同一份纯函数 (constitution R7)。
    return verify_own_ui_events_main();
  } else if (argc > 1 && !wcscmp(L"/hold-presence", argv[1])) {
    // ADR 0032 验收探针的「保持在场」客户端:以**普通用户应用**身份连上服务端,
    // 发 FocusIn(PROFILE_ACTIVATED) 把「火流猩在场」置为 present 并请求显示
    // 快捷设置栏;到点再发 FocusOut(PROFILE_DEACTIVATED)(= 用户真的切到别的
    // 输入法)把面板收起来,用来做反向对照。
    int hold_ms = 14000;
    int focusout_at_ms = 9000;
    if (argc > 2) hold_ms = _wtoi(argv[2]);
    if (argc > 3) focusout_at_ms = _wtoi(argv[3]);
    return hold_presence_main(hold_ms, focusout_at_ms);
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

// spec 096: FocusIn 的两类事件也要能在线上区分,记录文件与 FocusOut 同构。
std::wstring focusin_record_path() {
  wchar_t exe_path[MAX_PATH] = {0};
  DWORD exe_len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
  if (exe_len == 0 || exe_len >= MAX_PATH)
    return std::wstring();
  std::wstring exe_dir(exe_path, exe_len);
  size_t slash = exe_dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos)
    return std::wstring();
  return exe_dir.substr(0, slash + 1) + L"focusin_param.txt";
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
  // spec 094: 删词命令必须回 body(commit/status/ctx/候选列表)。
  // 客户端拿到响应后立刻刷新候选窗 —— 不回 body 的话,用户看到的还是上一次
  // 的候选,会以为「右键 / Ctrl+Shift+数字 根本没生效」。
  // body 里带一个可判别的标记:status.composing=1(Status 默认是 0)。
  virtual void DeleteCandidateOnCurrentPage(size_t index,
                                            DWORD session_id,
                                            EatLine eat) override {
    std::cerr << "DeleteCandidateOnCurrentPage: " << index << std::endl;
    if (eat) {
      std::wstring msg = L"status.ascii_mode=0\n";
      msg += L"status.composing=1\n";
      eat(msg);
    }
  }

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
  // spec 096: FocusIn 的两类事件必须能在线上区分。
  // 与 FocusOut 同构:把原始 wParam 逐行写进 focusin_param.txt。
  virtual void FocusIn(DWORD param, DWORD session_id) override {
    std::cerr << "FocusIn: " << session_id << " param=" << param << std::endl;
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
    std::wstring record = exe_dir + L"focusin_param.txt";
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

// spec 094: 删词命令必须回 body。
//
// 用户侧的现象是「右键 / Ctrl+Shift+数字 点了没反应,候选还在」——候选还在
// 是因为客户端读到的还是上一次的响应。这个模式用 mock server 直接证明
// DELETE 命令的响应里带得回新的状态。
// spec 096 / ADR 0027: FocusIn 的两类事件必须能在线上区分,且两侧的位标志
// 必须对称 —— 同一 bit 编号代表同一语义维度。
//
// 线上部分:mock server 把每次 FocusIn 的原始 wParam 写进 focusin_param.txt,
// 客户端发两个 FocusIn(普通窗口焦点 / 切到火流猩),读回两个值必须不同。
// 纯函数部分:协议表逐行断言对称性、取值、以及分类器优先级。
// 判定口径与生产代码同一份实现(宪法 R7)。
int verify_focusin_flags_main() {
  // --- 1. 协议表的对称性 ---
  size_t slot_count = 0;
  const FocusFlagSlot* slots = FocusFlagProtocolTable(slot_count);
  bool table_ok = (slot_count == 2);
  bool dim_ok = true;
  bool bit_ok = true;
  bool sym_ok = true;
  for (size_t i = 0; i < slot_count; ++i) {
    if (slots[i].dimension == NULL || slots[i].dimension[0] == '\0')
      dim_ok = false;
    // 每一行都必须是单 bit
    if (slots[i].in_bit == 0 || (slots[i].in_bit & (slots[i].in_bit - 1)) != 0)
      bit_ok = false;
    if (slots[i].out_bit == 0 || (slots[i].out_bit & (slots[i].out_bit - 1)) != 0)
      bit_ok = false;
    // 同一 bit 编号在两侧代表同一语义维度
    if (slots[i].in_bit != slots[i].out_bit)
      sym_ok = false;
  }
  // bit0 的取值必须与既有常量一致(线格式零变化的回归保护)
  bool bit0_ok = slots[0].in_bit == WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED &&
                 slots[0].out_bit == WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED;
  bool bit1_ok = slots[1].in_bit == WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED &&
                 slots[1].out_bit == WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST;
  // 两个维度的名字不能重复
  bool dim_unique =
      (slot_count == 2) &&
      (std::string(slots[0].dimension) != std::string(slots[1].dimension));

  // --- 2. 分类器 ---
  bool cls_ok =
      ClassifyFocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED) ==
          FocusInKind::kProfileActivated &&
      ClassifyFocusIn(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED) ==
          FocusInKind::kAppFocusGained &&
      ClassifyFocusIn(0) == FocusInKind::kUnknown &&
      // profile 优先:两个位同时在场时取更具体的那个
      ClassifyFocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED |
                      WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED) ==
          FocusInKind::kProfileActivated &&
      ClassifyFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED) ==
          FocusOutKind::kProfileDeactivated &&
      ClassifyFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST) ==
          FocusOutKind::kAppFocusLost &&
      ClassifyFocusOut(0) == FocusOutKind::kUnknown;

  // --- 3. 与 FocusOut 侧对称的判定函数 ---
  bool show_ok =
      ShouldShowQuickPanelOnFocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED) &&
      !ShouldShowQuickPanelOnFocusIn(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED) &&
      !ShouldShowQuickPanelOnFocusIn(0) &&
      HasFocusInFlag(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED,
                     WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED) &&
      HasFocusOutFlag(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST,
                      WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST);

  // --- 4. 语义保护:应用失焦尊重常驻开关,切走输入法必收起 ---
  // 加了 bit1 之后这两条必须与加之前完全一致(行为不变的回归保护)。
  bool hide_ok =
      !ShouldHideQuickPanelOnFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST, true) &&
      ShouldHideQuickPanelOnFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST, false) &&
      ShouldHideQuickPanelOnFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED, true);

  if (!(table_ok && dim_ok && bit_ok && sym_ok && bit0_ok && bit1_ok && dim_unique &&
        cls_ok && show_ok && hide_ok)) {
    std::cerr << "[FOCUSIN] FAIL table=" << table_ok << dim_ok << bit_ok
              << sym_ok << bit0_ok << bit1_ok << dim_unique
              << " cls=" << cls_ok << " show=" << show_ok
              << " hide=" << hide_ok << std::endl;
    return -2;
  }

  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "[FOCUSIN] connect failed" << std::endl;
    return -3;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "[FOCUSIN] session failed" << std::endl;
    return -4;
  }

  // Identity probe: the mock server answers a key event with the "Greeting="
  // marker, while a real WeaselServer.exe answers with a full
  // "action=status,ctx,config" block. Without this check the test could
  // silently measure whatever process happens to own the named pipe.
  std::wstring probe_body;
  client.ProcessKeyEvent(weasel::KeyEvent(L'a', 0));
  client.GetResponseData([&probe_body](LPWSTR buffer, DWORD length) -> bool {
    probe_body.assign(buffer, length);
    return true;
  });
  if (probe_body.find(L"Greeting=") == std::wstring::npos) {
    std::cerr << "[FOCUSIN] FAIL: the named pipe is served by another process, "
                 "not the TestWeaselIPC mock server."
              << std::endl;
    client.EndSession();
    return -8;
  }

  std::wstring record = focusin_record_path();
  if (record.empty()) {
    std::cerr << "[FOCUSIN] FAIL: cannot resolve the exe directory" << std::endl;
    client.EndSession();
    return -7;
  }

  // Start from a clean record: a leftover file from an earlier run would
  // make the baseline depend on the previous execution.
  DeleteFileW(record.c_str());
  int baseline = count_lines(read_text_file(record));

  // 1. the ordinary window focus event carries APP_FOCUS_GAINED
  client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED);
  bool got_app = wait_focusout_lines(record, baseline + 1, 5000);
  std::string app_line =
      got_app ? nth_line(read_text_file(record), baseline) : std::string();

  // 2. the profile activation carries PROFILE_ACTIVATED
  client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED);
  bool got_profile = wait_focusout_lines(record, baseline + 2, 5000);
  std::string profile_line =
      got_profile ? nth_line(read_text_file(record), baseline + 1)
                  : std::string();

  client.EndSession();

  bool app_is_set =
      (app_line == std::to_string(WEASEL_IPC_FOCUS_IN_FLAG_APP_FOCUS_GAINED));
  bool profile_is_set =
      (profile_line == std::to_string(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED));
  bool distinct =
      !app_line.empty() && !profile_line.empty() && (app_line != profile_line);

  std::cout << "[FOCUSIN] app=[" << app_line << "] profile=[" << profile_line
            << "]" << std::endl;

  if (got_app && got_profile && app_is_set && profile_is_set && distinct) {
    std::cout << "[FOCUSIN] PASS: the FocusIn event kinds are distinguishable "
                 "on the wire and the flag protocol table is symmetric"
              << std::endl;
    return 0;
  }
  std::cerr << "[FOCUSIN] FAIL got_app=" << got_app << " got_profile="
            << got_profile << " app_is_set=" << app_is_set
            << " profile_is_set=" << profile_is_set << " distinct=" << distinct
            << std::endl;
  return -4;
}

int verify_delete_body_main() {
  // spec 094: 「切走到别的输入法」时 TSF 投递的是 (别的 clsid, TRUE),而不是
  // (自己 clsid, FALSE)。判定表抽成共享纯函数,这里逐条断言;生产代码
  // WeaselTSF::OnActivated 用的是同一份(宪法 R7)。
  if (DecideProfileNotify(true, true) !=
      ProfileNotifyDecision::kNotifyActivated) {
    std::cerr << "[DELETE] profile notify: self+activated != kNotifyActivated"
              << std::endl;
    return -2;
  }
  if (DecideProfileNotify(true, false) !=
      ProfileNotifyDecision::kNotifyDeactivated) {
    std::cerr << "[DELETE] profile notify: self+deactivated != kNotifyDeactivated"
              << std::endl;
    return -2;
  }
  if (DecideProfileNotify(false, true) !=
      ProfileNotifyDecision::kNotifyDeactivated) {
    std::cerr << "[DELETE] profile notify: other+activated != kNotifyDeactivated"
              << std::endl;
    return -2;
  }
  if (DecideProfileNotify(false, false) != ProfileNotifyDecision::kIgnore) {
    std::cerr << "[DELETE] profile notify: other+deactivated != kIgnore"
              << std::endl;
    return -2;
  }

  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "[DELETE] connect failed" << std::endl;
    return -3;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "[DELETE] session failed" << std::endl;
    return -4;
  }

  // Identity check: the mock server answers a key event with the "Greeting="
  // marker, while a real WeaselServer.exe answers with a full
  // "action=status,ctx,config" block. Without this check the test would
  // silently measure whatever process happens to own the named pipe.
  std::wstring probe_body;
  client.ProcessKeyEvent(weasel::KeyEvent(L'a', 0));
  client.GetResponseData([&probe_body](LPWSTR buffer, DWORD length) -> bool {
    probe_body.assign(buffer, length);
    return true;
  });
  if (probe_body.find(L"Greeting=") == std::wstring::npos) {
    std::cerr << "[DELETE] FAIL: the named pipe is served by another process, "
                 "not the TestWeaselIPC mock server."
              << std::endl;
    client.EndSession();
    return -8;
  }

  // 删词命令本体:必须真的发得出去。
  //
  // 注意:ServerImpl::OnDeleteCandidateOnCurrentPage 的返回值**恒为 0** ——
  // 与 HighlightCandidateOnCurrentPage / ChangePage 同构,上游语义是
  // 「0 = 无错误」(PIPE_MSG_HANDLE 把 handler 返回值直接当事务 LRESULT 回给
  // 客户端)。所以客户端 DeleteCandidateOnCurrentPage 的返回值**不能**用
  // 「非 0」判成功;生产代码 WeaselTSF::_DeleteCandidateOnCurrentPage 同样
  // 不看返回值。真正的判据在下面:只有 DELETE handler 会写 status.composing=1。
  client.DeleteCandidateOnCurrentPage(2);
  std::wstring delete_body;
  bool got = client.GetResponseData(
      [&delete_body](LPWSTR buffer, DWORD length) -> bool {
        delete_body.assign(buffer, length);
        return true;
      });
  // 判别标记:mock 只在 DELETE 的响应里写 status.composing=1(Status 默认 0)。
  bool body_has_status =
      delete_body.find(L"status.composing=1") != std::wstring::npos;

  client.EndSession();

  std::cout << "[DELETE] got=" << (got ? 1 : 0)
            << " body_status=" << (body_has_status ? 1 : 0) << std::endl;
  if (got && body_has_status) {
    std::cout << "[DELETE] PASS: the delete-candidate IPC carries a response "
                 "body"
              << std::endl;
    return 0;
  }
  std::cerr << "[DELETE] FAIL" << std::endl;
  return -6;
}

// spec 097: verify the shared "is Fluxing present" state machine.
//
// The production code (RimeWithWeaselHandler) and this test call the same
// inline functions from include/WeaselIPC.h, so a drift between the two is
// impossible by construction (constitution R7).
int verify_quickpanel_presence_main() {
  int failures = 0;
  auto check = [&](bool cond, const char* what) {
    if (cond) {
      std::cout << "ok   : " << what << std::endl;
    } else {
      std::cerr << "FAIL : " << what << std::endl;
      ++failures;
    }
  };

  // 1. Unknown state must not forbid the panel: a client that never sends
  //    a profile notification still has to be able to show the panel.
  check(ShouldShowQuickPanelForPresence(FluxingPresence::kUnknown),
        "unknown presence allows show");

  // 2. Switching TO Fluxing => present => show allowed.
  FluxingPresence p =
      UpdateFluxingPresence(FluxingPresence::kUnknown, true, false);
  check(p == FluxingPresence::kPresent, "profile focus-in -> present");
  check(ShouldShowQuickPanelForPresence(p), "present allows show");

  // 3. Switching AWAY from Fluxing => absent => show forbidden.
  p = UpdateFluxingPresence(p, false, true);
  check(p == FluxingPresence::kAbsent, "profile focus-out -> absent");
  check(!ShouldShowQuickPanelForPresence(p), "absent forbids show");

  // 4. An ordinary app focus change must not touch the presence state.
  p = UpdateFluxingPresence(p, false, false);
  check(p == FluxingPresence::kAbsent, "app focus keeps presence");
  p = UpdateFluxingPresence(p, true, false);
  check(p == FluxingPresence::kPresent, "profile focus-in restores present");

  // 5. When both arrive in the same batch, "focus out" wins: the panel
  //    must never reappear after the user already left Fluxing.
  p = UpdateFluxingPresence(FluxingPresence::kPresent, true, true);
  check(p == FluxingPresence::kAbsent, "focus-out wins over focus-in");
  check(!ShouldShowQuickPanelForPresence(p), "absent still forbids show");

  // 6. The existing focus flag protocol keeps working unchanged.
  check(ClassifyFocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED) ==
            FocusOutKind::kProfileDeactivated,
        "focus-out flag classifies as profile-deactivated");
  check(ShouldHideQuickPanelOnFocusOut(
            WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED, true),
        "profile-deactivated hides even in always-show mode");

  std::cout << (failures == 0 ? "verify-quickpanel-presence: PASS"
                              : "verify-quickpanel-presence: FAIL")
            << std::endl;
  return failures == 0 ? 0 : 1;
}

// ADR 0031 决策 1/3/4 的判别性验收。
//
// 为什么这些断言"有牙":
//   * Evaluate() 是生产代码 WeaselIPCServer\WeaselServerImpl.cpp:Start() 调的
//     **同一个函数**(include/FluxingSessionPolicy.h, constitution R7)。
//     (current=3, console=1) 这一组正是 2026-11-03 事故的精确复刻 ——
//     ToDesk Session 3 里的服务端抢注了它那个会话的全局热键, console Session 1
//     无人服务。比较一旦被写成恒真、或两个 SessionId 取反, 这里立刻红。
//   * 留痕断言不是"格式对不对"的软检查: 它把 Describe() 产出的生产口径原样
//     写进 %TEMP%\fluxing-profile-notify.log(ADR 0029 决策 7 的同一通道)
//     再整文件读回来比对。写通道坏掉(路径取不到 / 2 MB 上限命中 / 静默吞异常)
//     会直接 FAIL, 而不是"看起来过了"。
//   * 全程只读判定 + 只写自己那一行探针, 不启动也不杀任何 WeaselServer.exe。
int verify_console_session_main() {
  int failures = 0;
  const auto expect = [&](bool cond, const char* what) {
    if (!cond) {
      ++failures;
      std::cout << "  FAIL: " << what << std::endl;
    }
  };

  // --- 1. 判定真值表(伪造 SessionId, 无需真实多会话环境) ---
  const fluxing::session::Verdict incident =
      fluxing::session::Evaluate(3, 1, true);
  expect(incident.allowed == false,
         "server in Session 3 while console is Session 1 must be refused");
  expect(std::string(incident.reason) == "not_console_session",
         "refusal reason key is not_console_session");
  expect(incident.current_session == 3 && incident.console_session == 1,
         "refusal keeps both session ids for forensics");

  const fluxing::session::Verdict mirror =
      fluxing::session::Evaluate(1, 3, true);
  expect(mirror.allowed == false,
         "mirror case current=1 console=3 refused too");

  const fluxing::session::Verdict match =
      fluxing::session::Evaluate(1, 1, true);
  expect(match.allowed == true, "console session is allowed to start");
  expect(std::string(match.reason) == "console_session_match",
         "allow reason key is console_session_match");

  // 会话 0 是服务会话, 不是任何人的 console。
  const fluxing::session::Verdict service =
      fluxing::session::Evaluate(0, 1, true);
  expect(service.allowed == false, "Session 0 service context must be refused");

  // 取不到 console SessionId(无主控台机器 / WTS 失败)时 fail-open:
  // 宁可放行留痕, 也不能让正常 console 用户因为一次 API 失败彻底失去服务。
  const fluxing::session::Verdict failOpen =
      fluxing::session::Evaluate(1, fluxing::session::kInvalidSessionId, false);
  expect(failOpen.allowed == true, "unknown console session fails open");
  expect(std::string(failOpen.reason) == "console_session_unknown_fail_open",
         "fail-open reason key");
  expect(failOpen.console_session == 0,
         "fail-open hides the unusable console id");

  // --- 2. 生产日志行的确切形状 ---
  const std::string refusalLine =
      fluxing::session::Describe("startup_refused", incident);
  expect(
      refusalLine ==
          "startup_refused allowed=0 reason=not_console_session "
          "current_session_id=3 console_session_id=1",
      "refusal line shape is <event> allowed=0 reason=... current/console ids");
  const std::string failOpenLine =
      fluxing::session::Describe("startup_allowed", failOpen);
  expect(failOpenLine.find("console_session_id=unknown") != std::string::npos,
         "unknown console session is printed as unknown, not 4294967295");

  // --- 3. 本进程的真实判定必须与纯函数一致(防止两边各写一套) ---
  bool known = false;
  const DWORD console = fluxing::session::ConsoleSessionId(&known);
  const DWORD current = fluxing::session::CurrentSessionId();
  expect(current != 0, "ProcessIdToSessionId must resolve this test process");
  const fluxing::session::Verdict live = fluxing::session::EvaluateCurrent();
  const fluxing::session::Verdict recomputed =
      fluxing::session::Evaluate(current, console, known);
  expect(live.allowed == recomputed.allowed,
         "EvaluateCurrent agrees with Evaluate over the same ids");
  expect(std::string(live.reason) == std::string(recomputed.reason),
         "EvaluateCurrent reports the same reason key");
  std::cout << "  info: current_session_id=" << (unsigned long long)current
            << " console_session_id="
            << (known ? std::to_string((unsigned long long)console)
                      : std::string("unknown"))
            << " allowed=" << (live.allowed ? "1" : "0")
            << " reason=" << live.reason << std::endl;

  // --- 4. 留痕通道: 写进去再读回来 ---
  const std::wstring path = fluxing::diag::DiagLogPath();
  expect(!path.empty(), "diag log path resolves");
  expect(fluxing::diag::DiagLogShouldWrite(),
         "diag log below its 2 MB cap, otherwise the probe line cannot be "
         "written");
  const std::string marker =
      refusalLine + " probe_pid=" +
      std::to_string((unsigned long long)::GetCurrentProcessId());
  fluxing::diag::AppendLine("server", marker);

  std::ifstream in(path.c_str(), std::ios::binary);
  expect(in.good(), "diag log is readable back");
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  const std::string needle = "[server] " + marker + "\n";
  expect(content.find(needle) != std::string::npos,
         "the production-shaped refusal line really lands in the diag log");
  // 反向自检: 同一套查找必须能"查不到", 证明上一条命中不是恒真。
  expect(content.find(needle + "XX_NO_SUCH_MARKER_XX") == std::string::npos,
         "substring search can still miss (probe is not vacuous)");

  std::cout << (failures == 0 ? "verify-console-session: PASS"
                              : "verify-console-session: FAIL")
            << std::endl;
  return failures == 0 ? 0 : 1;
}

// ADR 0032: 自家 UI 进程的身份判定 + 快捷设置栏联动口径。
//
// 为什么这些断言「有牙」:
//   * 生产代码(RimeWithWeasel.cpp 的 FocusIn / FocusOut / RemoveSession)调的就是
//     这几个纯函数,不是另抄一份(include/WeaselIPC.h,constitution R7);
//   * 断言同时覆盖**两个方向**:自家 UI 不影响面板,用户应用仍按老口径影响面板。
//     只测前一半的话,把判定反过来(自家 UI 也收起)测试照样会绿 —— 那是假绿;
//   * 变异反证:把 IsOwnUiClient 的判定取反,第 2/3/4 组断言必须翻红。
//
// 本模式是纯函数测试:不需要跑服务端,不写任何文件,不碰进程。
int verify_own_ui_events_main() {
  int failures = 0;
  const auto expect = [&](bool cond, const char* what) {
    if (!cond) {
      ++failures;
      std::cout << "  FAIL: " << what << std::endl;
    }
  };

  const DWORD kActivated = WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED;
  const DWORD kDeactivated = WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED;
  const fluxing::IpcClientKind kOwnUi = fluxing::IpcClientKind::kOwnUi;
  const fluxing::IpcClientKind kUserApp = fluxing::IpcClientKind::kUserApp;

  // --- 1. 身份判定 ---
  expect(fluxing::ClassifyIpcClient("fluxingphrasesdialog.exe") == kOwnUi,
         "the phrases dialog exe is classified as our own UI");
  expect(fluxing::ClassifyIpcClient("FluxingPhrasesDialog.exe") == kOwnUi,
         "the classification is case insensitive");
  expect(fluxing::ClassifyIpcClient("notepad.exe") == kUserApp,
         "a normal application is classified as a user app");
  expect(fluxing::ClassifyIpcClient("") == kUserApp,
         "an unknown client falls back to the user-app behaviour");
  expect(std::string(fluxing::OwnUiClientExeName()) ==
             "FluxingPhrasesDialog.exe",
         "the canonical own-UI exe name is the one on disk");
  expect(fluxing::OwnUiClientExeNameLower() ==
             "fluxingphrasesdialog.exe",
         "the lower-case form matches the session.client_app convention");

  // --- 2. FocusIn:自家 UI 的 profile 激活不请求显示面板 ---
  expect(!fluxing::ShouldShowQuickPanelOnFocusInForClient(kActivated, kOwnUi),
         "our own UI must not request the QuickPanel");
  expect(fluxing::ShouldShowQuickPanelOnFocusInForClient(kActivated, kUserApp),
         "a user app switching to Fluxing still requests the QuickPanel");
  expect(!fluxing::ShouldShowQuickPanelOnFocusInForClient(0, kUserApp),
         "an ordinary focus change never requests the QuickPanel");

  // --- 3. FocusOut:自家 UI 的失焦不收起面板;用户应用仍按老口径收起 ---
  expect(!fluxing::ShouldHideQuickPanelOnFocusOutForClient(kDeactivated, false,
                                                            kOwnUi),
         "our own UI must not hide the QuickPanel");
  expect(!fluxing::ShouldHideQuickPanelOnFocusOutForClient(kDeactivated, true,
                                                            kOwnUi),
         "our own UI must not hide the QuickPanel even in always-show mode");
  expect(fluxing::ShouldHideQuickPanelOnFocusOutForClient(kDeactivated, false,
                                                          kUserApp),
         "a user app switching away hides the QuickPanel");
  expect(fluxing::ShouldHideQuickPanelOnFocusOutForClient(kDeactivated, true,
                                                          kUserApp),
         "a user app switching away hides the QuickPanel even in always-show "
         "mode (spec 091)");
  expect(fluxing::ShouldHideQuickPanelOnFocusOutForClient(0, false, kUserApp),
         "an ordinary focus loss hides the QuickPanel when not always-show");
  expect(!fluxing::ShouldHideQuickPanelOnFocusOutForClient(0, true, kUserApp),
         "an ordinary focus loss keeps the QuickPanel in always-show mode");

  // --- 4. 会话结束:自家 UI 不等于「火流猩离场」 ---
  expect(!fluxing::ShouldTreatSessionEndAsFluxingAbsent(kOwnUi),
         "our own UI session end is not a Fluxing-absent signal");
  expect(fluxing::ShouldTreatSessionEndAsFluxingAbsent(kUserApp),
         "a user app session end still means Fluxing is gone");

  std::cout << (failures == 0 ? "verify-own-ui-events: PASS"
                              : "verify-own-ui-events: FAIL")
            << std::endl;
  return failures == 0 ? 0 : 1;
}
// ADR 0032 验收探针:「保持在场」客户端。
//
// 为什么需要它:真机验收必须在「快捷设置栏本来就该可见」的正常场景下复现。
// 而修复之后,自家 UI 的 focus_in(profile) 不再请求显示面板(那正是被抑制的
// 副作用),于是「先弹面板、再开短语窗」的老探针前提已经不存在 —— 面板从没
// 显示过,探针却会报「没复现」,那是假绿。本客户端以**普通用户应用**身份
// 把在场状态置成 present 并请求显示面板,恢复真实前提。
//
// 客户端身份口径:weasel::ClientImpl 把**宿主进程 exe 名**(小写)写进
// session.client_app(WeaselClientImpl.cpp:30-37 / :238),所以本进程自报
// "testweaselipc.exe" ⇒ 服务端判定为普通用户应用,与自家 UI 无关。
int hold_presence_main(int hold_ms, int focusout_at_ms) {
  weasel::Client client;
  if (!client.Connect()) {
    std::cerr << "hold-presence: server not running." << std::endl;
    return 3;
  }
  client.StartSession();
  if (!client.HasSession()) {
    std::cerr << "hold-presence: StartSession failed." << std::endl;
    return 3;
  }
  std::cout << "hold-presence: session started, FocusIn(PROFILE_ACTIVATED)"
            << std::endl;
  client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED);
  if (focusout_at_ms > 0 && focusout_at_ms < hold_ms) {
    Sleep(static_cast<DWORD>(focusout_at_ms));
    bool delivered =
        client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);
    std::cout << "hold-presence: FocusOut(PROFILE_DEACTIVATED) delivered="
              << (delivered ? 1 : 0) << std::endl;
    Sleep(static_cast<DWORD>(hold_ms - focusout_at_ms));
  } else {
    Sleep(static_cast<DWORD>(hold_ms));
  }
  client.EndSession();
  std::cout << "hold-presence: session ended." << std::endl;
  return 0;
}
