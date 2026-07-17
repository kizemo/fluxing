#include "stdafx.h"
#include "WeaselServerApp.h"
#include <filesystem>
#include <iostream>  // v0.19.0.25-fix(spec 042 §12 风险):RegisterHotKey 失败 wcerr log
#include "QuickPanelDialog.h"
#include "PhrasesDialog.h"  // v0.19.0.25-fix(spec 042 §3):Track 3 wiring
#include "UserDictionary.h"  // v0.19.0.28(spec 044 §3.2):Track 3 wiring
#include "ShortcutSettings.h"  // spec 045 v0.19.0.28: Track 3 wiring
// spec 070 T007: D2D factory 创建 (在 WeaselServerApp::Run 入口)
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

// v0.19.0.29:QuickPanel button 2/3 callback 转发 setter。static 实现,
// 把 fn 存到 QuickPanelDialog 内部 static 字段(QuickPanelDialog::s_onUserDict /
// s_onShortcut),WeaselServerApp::Run() 在 m_server.Start 后调一次。
void WeaselServerApp::SetQuickPanelUserDictCallback(
    QuickPanelDialog::OnShowUserDict fn) {
  QuickPanelDialog::SetOnUserDict(fn);
}

void WeaselServerApp::SetQuickPanelShortcutCallback(
    QuickPanelDialog::OnShowShortcut fn) {
  QuickPanelDialog::SetOnShortcut(fn);
}

WeaselServerApp::WeaselServerApp()
    : m_handler(std::make_unique<RimeWithWeaselHandler>(&m_ui)),
      tray_icon(m_ui) {
  // m_handler.reset(new RimeWithWeaselHandler(&m_ui));
  m_server.SetRequestHandler(m_handler.get());
  SetupMenuHandlers();
}

WeaselServerApp::~WeaselServerApp() {}

// v0.19.0.25-fix(spec 042 §3 + §10.4):单实例,子类 WNDPROC 静态用。
// Run 期间唯一,Stop 后 nullptr;子类 WNDPROC 命中 ALT+. 时通过此指针调 instance。
WeaselServerApp* WeaselServerApp::s_phrasesHotkeyOwner = nullptr;

// v0.19.0.25-fix(spec 042 §10.4):子类 WNDPROC,拦截 WM_HOTKEY (ID_HOTKEY_PHRASES_DOT)
// → PhrasesDialog::Show();其它 message 透传给原 WNDPROC(其中含 ServerImpl::OnHotkey
// 处理 ID_HOTKEY_QUICK_PANEL)。不能改 ServerImpl(在 WeaselIPCServer/ 下,scope 之外),
// 用 SetWindowLongPtr(GWLP_WNDPROC) 在 IPC server window 上子类化。
LRESULT CALLBACK WeaselServerApp::PhrasesHotkeySubclassProc(HWND hwnd,
                                                           UINT msg,
                                                           WPARAM w,
                                                           LPARAM l) {
  if (msg == WM_HOTKEY) {
    if (w == ID_HOTKEY_PHRASES_DOT) {
      // spec 042 §12 风险:Alt+. 全局热键可能跟其他 app 冲突 → RegisterHotKey 失败时
      // 仅 log warning,不 crash。这里成功路径就直接调。
      PhrasesDialog::Show();
      return 0;
    }
    if (w == ID_HOTKEY_USER_DICT) {
      // spec 044 §3.2:Ctrl+Shift+U → UserDictionary::Show
      UserDictionary::Show();
      return 0;
    }
    if (w == ID_HOTKEY_SHORTCUT) {
      // spec 045 v0.19.0.28:Ctrl+Shift+K → ShortcutSettings::Show
      ShortcutSettings::Show();
      return 0;
    }
    // v0.19.0.32-fix (Bug 3b): Alt+/ → UserDictionary::Show()
    // (跟 QuickPanel Button 2 (UserDict) 共享入口)
    if (w == ID_HOTKEY_USER_DICT_ALT_SLASH) {
      UserDictionary::Show();
      return 0;
    }
  }
  // 透传:必须 CallWindowProc 回原 WNDPROC(否则破坏 ServerImpl OnHotkey/WM_COMMAND 等)
  WeaselServerApp* owner = s_phrasesHotkeyOwner;
  if (owner && owner->m_ipcServerOrigWndProc) {
    return ::CallWindowProc(owner->m_ipcServerOrigWndProc, hwnd, msg, w, l);
  }
  return ::DefWindowProc(hwnd, msg, w, l);
}

void WeaselServerApp::RegisterPhrasesHotkey() {
  HWND hwndServer = m_server.GetHWnd();
  if (!hwndServer) {
    // IPC server 还没启起来(spec 036 会在 ServerImpl::OnCreate 注册 Alt+,),spec 042
    // 也得在 IPC server window 上注册,延迟到 Run 之后调用即可。
    return;
  }
  // 1) 子类化前先保存原 WNDPROC
  m_ipcServerOrigWndProc = reinterpret_cast<WNDPROC>(
      ::GetWindowLongPtr(hwndServer, GWLP_WNDPROC));
  if (!m_ipcServerOrigWndProc) return;
  s_phrasesHotkeyOwner = this;
  ::SetWindowLongPtr(hwndServer, GWLP_WNDPROC,
                     reinterpret_cast<LONG_PTR>(
                         &WeaselServerApp::PhrasesHotkeySubclassProc));

  // 2) 注册全局热键 ALT+. (VK_OEM_PERIOD 是 US 键盘的 . 键)
  //    AP-036-F: 失败仅 log warning,不 crash。失败可能是被其他 app 占用了。
  //    Phase A.2: 增强 log — 标 id + label + 解释 ERROR_HOTKEY_ALREADY_REGISTERED=1409
  //    是最常见原因 (其他 app 占用了 Alt+. 全局热键)。
  if (!::RegisterHotKey(hwndServer, ID_HOTKEY_PHRASES_DOT, MOD_ALT, VK_OEM_PERIOD)) {
    DWORD err = ::GetLastError();
    LPCWSTR cause = (err == 1409) ? L"ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用 Alt+.)"
                                  : L"see Win32 error codes";
    std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(Alt+., id=" << ID_HOTKEY_PHRASES_DOT
               << L") failed, err=" << err << L" — " << cause << std::endl;
  }

  // 3) v0.19.0.28(spec 044 §3.2):注册全局热键 Ctrl+Shift+U → UserDictionary::Show
  //    字母 U 的虚拟键码是 0x55 (ASCII 'U')
  if (!::RegisterHotKey(hwndServer, ID_HOTKEY_USER_DICT,
                        MOD_CONTROL | MOD_SHIFT, 0x55)) {
    DWORD err = ::GetLastError();
    LPCWSTR cause = (err == 1409) ? L"ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用 Ctrl+Shift+U)"
                                  : L"see Win32 error codes";
    std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(Ctrl+Shift+U, id=" << ID_HOTKEY_USER_DICT
               << L") failed, err=" << err << L" — " << cause << std::endl;
  }

  // 4) spec 045 v0.19.0.28: Ctrl+Shift+K → ShortcutSettings::Show
  //    字母 K 的虚拟键码是 0x4B (ASCII 'K')
  if (!::RegisterHotKey(hwndServer, ID_HOTKEY_SHORTCUT,
                        MOD_CONTROL | MOD_SHIFT, 0x4B)) {
    DWORD err = ::GetLastError();
    LPCWSTR cause = (err == 1409) ? L"ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用 Ctrl+Shift+K)"
                                  : L"see Win32 error codes";
    std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(Ctrl+Shift+K, id=" << ID_HOTKEY_SHORTCUT
               << L") failed, err=" << err << L" — " << cause << std::endl;
  }

  // 5) v0.19.0.32-fix (Bug 3b): Alt+/ → UserDictionary::Show()
  //    / 键的虚拟键码是 VK_OEM_2 (= 0xBF,US 键盘的 / 键)
  if (!::RegisterHotKey(hwndServer, ID_HOTKEY_USER_DICT_ALT_SLASH,
                        MOD_ALT, VK_OEM_2)) {
    DWORD err = ::GetLastError();
    LPCWSTR cause = (err == 1409) ? L"ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用 Alt+/)"
                                  : L"see Win32 error codes";
    std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(Alt+/, id=" << ID_HOTKEY_USER_DICT_ALT_SLASH
               << L") failed, err=" << err << L" — " << cause << std::endl;
  }
}

void WeaselServerApp::UnregisterPhrasesHotkey() {
  HWND hwndServer = m_server.GetHWnd();
  if (hwndServer) {
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_PHRASES_DOT);
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_USER_DICT);  // spec 044
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_SHORTCUT);   // spec 045
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_USER_DICT_ALT_SLASH);  // v0.19.0.32
    if (m_ipcServerOrigWndProc) {
      ::SetWindowLongPtr(hwndServer, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(m_ipcServerOrigWndProc));
      m_ipcServerOrigWndProc = nullptr;
    }
  }
  s_phrasesHotkeyOwner = nullptr;
}

int WeaselServerApp::Run() {
  if (!m_server.Start())
    return -1;

  // spec 074 (L75): QuickPanel 改纯 GDI 后,不再需要 ID2D1Factory 注入
  // QuickPanelDialog::InitializeD2D / ShutdownD2D 已删除

  // win_sparkle_set_appcast_url("http://localhost:8000/weasel/update/appcast.xml");
  win_sparkle_set_registry_path("Software\\Rime\\Weasel\\Updates");
  if (GetThreadUILanguage() ==
      MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL))
    win_sparkle_set_lang("zh-TW");
  else if (GetThreadUILanguage() ==
           MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
    win_sparkle_set_lang("zh-CN");
  else
    win_sparkle_set_lang("en");
  win_sparkle_init();
  m_ui.Create(m_server.GetHWnd());

  m_handler->Initialize();
  m_handler->OnUpdateUI([this]() { tray_icon.Refresh(); });

  tray_icon.Create(m_server.GetHWnd());
  tray_icon.Refresh();

  // spec 055 bugfix: QuickPanel MUST NOT auto-show on service start.
  // spec 052's "always-show mode" was a design intention; the original
  // implementation called EnableAlwaysShowMode() here, which made the
  // panel appear in the bottom-right corner even when the user was not
  // using the Fluxing IME (user-reported bug).
  //
  // Correct behavior (per spec 052 US052-A, US052-D, US052-E):
  // - Show on Alt+, hotkey (spec 036)         - handled by tray menu
  // - Show on left-click tray icon            - handled by SystemTraySDK
  // - Hide on Alt+, when visible              - handled by QuickPanelDialog::ToggleMode
  // - Show on tray "QuickPanel" menu item    - handled by ID_WEASELTRAY_QUICK_PANEL handler
  //
  // No auto-show call here. EnableAlwaysShowMode() remains as a public
  // API for the "remembered" state across IPC reconnects (if needed
  // later), but is no longer called on every Run().

  // v0.19.0.25-fix(spec 042 §3 + §10.4):Alt+. 全局热键注册。
  // 必须在 m_server.Run() 之前调(m_server.Run() 进入消息循环前 IPC server window 已经
  // 在 m_server.Start() 时创建,WM_CREATE 已经发出 → ServerImpl::OnCreate 注册了 Alt+,;
  // 现在再加 Alt+.)。失败仅 log warning,不 crash(spec 042 §12 风险)。
  RegisterPhrasesHotkey();

  // v0.19.0.29(mockups-v0.19.0.28 设计稿):把 QuickPanel hit==2 (Symbols) → UserDict,
  // hit==3 (Settings) → Shortcut 接线。QuickPanelDialog::SetOn* setter 内部存 static
  // std::function,callback 在 QuickPanelDialog::WndProc WM_LBUTTONUP click 路由触发。
  // 这里调一次,SetOn* 是 idempotent(fn null 即清空),但 lifecycle 与 WeaselServerApp
  // 同,只在 Run 入口调一次即可。
  SetQuickPanelUserDictCallback([]() { UserDictionary::Show(); });
  SetQuickPanelShortcutCallback([]() { ShortcutSettings::Show(); });

  int ret = m_server.Run();

  // m_server.Run() 返回后消息循环已退出,unregister 子类化(避免下次 start 残留状态)
  UnregisterPhrasesHotkey();

  m_handler->Finalize();
  m_ui.Destroy();
  tray_icon.RemoveIcon();
  win_sparkle_cleanup();

  // spec 074 (L75): QuickPanel 改纯 GDI 后,不需要 ShutdownD2D 也没有 factory 释放

  return ret;
}

void WeaselServerApp::SetupMenuHandlers() {
  std::filesystem::path dir = install_dir();
  m_server.AddMenuHandler(ID_WEASELTRAY_QUIT,
                          [this] { return m_server.Stop() == 0; });
  m_server.AddMenuHandler(ID_WEASELTRAY_DEPLOY,
                          std::bind(execute, dir / L"WeaselDeployer.exe",
                                    std::wstring(L"/deploy")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SETTINGS,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring()));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_DICT_MANAGEMENT,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/dict")));
  m_server.AddMenuHandler(
      ID_WEASELTRAY_SYNC,
      std::bind(execute, dir / L"WeaselDeployer.exe", std::wstring(L"/sync")));
  m_server.AddMenuHandler(ID_WEASELTRAY_WIKI,
                          std::bind(open, L"https://rime.im/docs/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_HOMEPAGE,
                          std::bind(open, L"https://rime.im/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_FORUM,
                          std::bind(open, L"https://rime.im/discuss/"));
  m_server.AddMenuHandler(ID_WEASELTRAY_CHECKUPDATE, check_update);
  m_server.AddMenuHandler(ID_WEASELTRAY_INSTALLDIR, std::bind(explore, dir));
  m_server.AddMenuHandler(ID_WEASELTRAY_USERCONFIG,
                          std::bind(explore, WeaselUserDataPath()));
  m_server.AddMenuHandler(ID_WEASELTRAY_LOGDIR,
                          std::bind(explore, WeaselLogPath()));

  m_server.AddMenuHandler(
      ID_WEASELTRAY_RESTORE_IGNORED, [this] {
    std::wstring userDir = WeaselUserDataPath().wstring();
    if (userDir.empty()) return true;  // no user-data dir -> no-op
    WIN32_FIND_DATAW fd;
    std::wstring pattern = userDir + L"\\*.user_ignore.txt";
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return true;  // no ignore files -> success
    do {
      std::wstring path = userDir + L"\\" + fd.cFileName;
      DeleteFileW(path.c_str());
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    // Spec 032 R2: no auto-refresh of the candidate list.
    // Next input event / schema switch triggers Refresh,
    // restoring previously-ignored candidates.
    return true;
  });
  // spec 070 T007: 解禁 QuickPanel 触发 (L69 临时关闭已解除)
  // QuickPanel 触发的 3 个来源:
  //   - Alt+, 全局热键 (via WM_COMMAND post from OnHotkey)
  //   - 左键单击托盘图标 (via WM_COMMAND post from SystemTraySDK)
  //   - 右键托盘菜单 "QuickPanel" 项 (rc file)
  m_server.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL, [this] {
    // L70-bugfix: 用 ToggleMode 替代 Show,让 Alt+, 第二次按能隐藏
    bool currentFull = m_handler ? m_handler->IsFullShape() : false;
    if (QuickPanelDialog::ActiveHwnd() &&
        IsWindowVisible(QuickPanelDialog::ActiveHwnd())) {
      QuickPanelDialog::Hide();
    } else {
      QuickPanelDialog::Show(
          currentFull,
          [this]() {
            fs::path deployer = install_dir() / L"WeaselDeployer.exe";
            ShellExecuteW(NULL, NULL, deployer.c_str(), L"/hotkey", NULL, SW_SHOWNORMAL);
          },
          [this]() { explore(WeaselUserDataPath()); },
          [this]() { PhrasesDialog::Show(); },
          [this](bool newFull) {
            if (m_handler) m_handler->SetOption(0, "full_shape", newFull);
          },
          [this]() { UserDictionary::Show(); },
          []() {});
    }
    return true;
  });
}