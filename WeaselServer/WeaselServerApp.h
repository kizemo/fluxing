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
#include "ShortcutSettings.h"  // spec 045 v0.19.0.28: Track 3 wiring
#include "UserDictionary.h"    // spec 044 v0.19.0.28: Track 3 wiring
#include "QuickPanelDialog.h"  // v0.19.0.29: SetOnUserDict / SetOnShortcut setter call

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

  // spec 045 v0.19.0.28: Ctrl+Shift+K → ShortcutSettings::Show()
  void RegisterShortcutHotkey();
  void UnregisterShortcutHotkey();

  // v0.19.0.29(mockups-v0.19.0.28 设计稿):把 Shortcut 入口从 QuickPanel
  // 接到 ShortcutSettings::Show。Setter 转发给 QuickPanel 内部静态字段,
  // 与 s_onPhrases 注入路径平行。
  // v0.19.0.60 (Phase L 调整 1): SetQuickPanelUserDictCallback 删除 — UserDictionary 模块下线。
  static void SetQuickPanelShortcutCallback(QuickPanelDialog::OnShowShortcut fn);

  // IPC server window 原 WNDPROC(子类化前保存,UnregisterPhrasesHotkey 时还原)
  static LRESULT CALLBACK PhrasesHotkeySubclassProc(HWND, UINT, WPARAM, LPARAM);

  // 当前子类化的实例(Run 期间唯一,Stop 后 nullptr)
  static WeaselServerApp* s_phrasesHotkeyOwner;
  // v0.19.0.63 (Phase L 调整 3): 启动时记录的 Fluxing HKL + 上一次 WM_INPUTLANGCHANGE 看到的 HKL。
  // WM_INPUTLANGCHANGE 钩子用这对检测"user 切到 Fluxing"。
  static HKL s_fluxingHkl;
  static HKL s_prevLangHkl;

  weasel::Server m_server;
  weasel::UI m_ui;
  WeaselTrayIcon tray_icon;
  std::unique_ptr<RimeWithWeaselHandler> m_handler;

  // IPC server window 原 WNDPROC(子类化时保存,UnregisterPhrasesHotkey 时还原)
  WNDPROC m_ipcServerOrigWndProc = nullptr;
  // v0.19.0.64 (Phase L 调整 3 bugfix): Win+Space 切输入法时,WM_INPUTLANGCHANGE 只发给
  //   当前 focus 所在 top-level 窗口(foreground app),**不**发给 WeaselServer 的 hidden
  //   IPC server window → 装机后浮动栏不弹 (v0.19.0.63 ship 后 user 反馈)。
  //   修法: 另外子类化 ImmGetDefaultIMEWnd(NULL) 返回的默认 IME 窗口,该窗口是 Windows
  //   自动管理的 hidden window,**专门**接收 IME 切换广播(包括 WM_INPUTLANGCHANGE)。
  WNDPROC m_imeDefaultOrigWndProc = nullptr;
  HWND m_hwndImeDefault = nullptr;
  // WM_INPUTLANGCHANGE 子类化 WndProc — 仅在 m_imeDefaultOrigWndProc 钩子上跑
  static LRESULT CALLBACK ImeLangChangeSubclassProc(HWND, UINT, WPARAM, LPARAM);
};
