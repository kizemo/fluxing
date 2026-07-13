#pragma once

#include "resource.h"
#include <resource.h>
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <RimeWithWeasel.h>
#include <WeaselUtility.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <winsparkle.h>

#include "WeaselTrayIcon.h"

namespace fs = std::filesystem;

class WeaselServerApp {
 public:
  static bool execute(const fs::path& cmd, const std::wstring& args) {
    return (uintptr_t)ShellExecuteW(NULL, NULL, cmd.c_str(), args.c_str(), NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static bool explore(const fs::path& path) {
    std::wstring quoted_path(L"\"" + path.wstring() + L"\"");
    return (uintptr_t)ShellExecuteW(NULL, L"explore", quoted_path.c_str(), NULL,
                                    NULL, SW_SHOWNORMAL) > 32;
  }

  static bool open(const fs::path& path) {
    return (uintptr_t)ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL,
                                    SW_SHOWNORMAL) > 32;
  }

  static bool check_update() {
    // when checked manually, show testing versions too
    std::string feed_url = GetCustomResource("ManualUpdateFeedURL", "APPCAST");
    std::wstring channel{};
    auto ret = RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Rime\\Weasel",
                                 L"UpdateChannel", channel);
    if (!ret && channel == L"testing") {
      feed_url = GetCustomResource("TestingManualUpdateFeedURL", "APPCAST");
    }
    if (!feed_url.empty()) {
      win_sparkle_set_appcast_url(feed_url.c_str());
    }
    win_sparkle_check_update_with_ui();
    return true;
  }

  static fs::path install_dir() {
    WCHAR exe_path[MAX_PATH] = {0};
    GetModuleFileNameW(GetModuleHandle(NULL), exe_path, _countof(exe_path));
    return fs::path(exe_path).remove_filename();
  }

 public:
  WeaselServerApp();
  ~WeaselServerApp();
  int Run();

 protected:
  void SetupMenuHandlers();

  // v0.19.0.25-fix(spec 042 §3 + §10.4):Alt+. 全局热键 hook。
  // - RegisterHotKey / UnregisterHotKey 绑 m_server.GetHWnd()(IPC server window)
  // - 不改 WeaselIPCServer/ServerImpl 的 OnHotkey,而是用 SetWindowLongPtr 子类化 IPC
  //   server window,在子类 WndProc 拦截 WM_HOTKEY (wParam == ID_HOTKEY_PHRASES_DOT)
  //   → PhrasesDialog::Show();其他 WM_HOTKEY / 其他 message 透传给原 WndProc。
  // - 子类 WNDPROC 是 static 的(无法捕获 this),所以通过一个 thread-local g_phrasesSubclass
  //   指针存 WeaselServerApp 实例,避免依赖 thunk。Run 期间 lifecycle 与 m_server 同,
  //   Stop 路径在 m_server.Run() 返回后,UnregisterHotKey + 还原 WNDPROC。
  void RegisterPhrasesHotkey();
  void UnregisterPhrasesHotkey();

  // IPC server window 原 WNDPROC(子类化前保存,UnregisterPhrasesHotkey 时还原)
  static LRESULT CALLBACK PhrasesHotkeySubclassProc(HWND, UINT, WPARAM, LPARAM);

  // 当前子类化的实例(Run 期间唯一,Stop 后 nullptr)
  static WeaselServerApp* s_phrasesHotkeyOwner;

  weasel::Server m_server;
  weasel::UI m_ui;
  WeaselTrayIcon tray_icon;
  std::unique_ptr<RimeWithWeaselHandler> m_handler;

  // IPC server window 原 WNDPROC(子类化时保存,UnregisterPhrasesHotkey 时还原)
  WNDPROC m_ipcServerOrigWndProc = nullptr;
};
