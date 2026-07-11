#include "stdafx.h"
#include "WeaselServerApp.h"
#include <filesystem>
#include "QuickPanelDialog.h"

WeaselServerApp::WeaselServerApp()
    : m_handler(std::make_unique<RimeWithWeaselHandler>(&m_ui)),
      tray_icon(m_ui) {
  // m_handler.reset(new RimeWithWeaselHandler(&m_ui));
  m_server.SetRequestHandler(m_handler.get());
  SetupMenuHandlers();
}

WeaselServerApp::~WeaselServerApp() {}

int WeaselServerApp::Run() {
  if (!m_server.Start())
    return -1;

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

  int ret = m_server.Run();

  m_handler->Finalize();
  m_ui.Destroy();
  tray_icon.RemoveIcon();
  win_sparkle_cleanup();

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
  // spec 036: QuickPanel trigger. Same handler is used by:
  //   - Alt+, global hotkey (via WM_COMMAND post from OnHotkey).
  //   - Left-click tray icon (via WM_COMMAND post from SystemTraySDK).
  //   - "QuickPanel" menu item in the right-click tray menu (rc file).
  // L69-fix: QuickPanel DISABLED to avoid GDI+ heap corruption crash.
  // The handler is kept but it does nothing. To re-enable QuickPanel in
  // a future build, remove this comment block AND the false && gate in
  // RimeWithWeaselHandler::FocusIn AND the unregistration in
  // WeaselServerImpl::OnCreate.
  m_server.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL, [this] {
    // L69-fix: no-op. QuickPanel triggers crash (see L69 entry).
    (void)this;
    return true;
  });
}