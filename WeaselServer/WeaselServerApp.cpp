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

  // spec 052: auto-show QuickPanel in always-show mode on service start
  QuickPanelDialog::EnableAlwaysShowMode();

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
  m_server.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL, [this] {
    // spec 052: Alt+, toggles always-show mode (hide if visible, show if hidden)
    if (QuickPanelDialog::CurrentMode() != QuickPanelDialog::Mode::kHidden) {
      QuickPanelDialog::Hide();
      return true;
    }
    bool currentFull = m_handler ? m_handler->IsFullShape() : false;
    QuickPanelDialog::Show(
        currentFull,
        // 1. 方案 -> launch deployer /hotkey (spec 050 visual editor)
        [this]() {
          std::filesystem::path deployer = install_dir() / L"WeaselDeployer.exe";
          ShellExecuteW(NULL, NULL, deployer.c_str(), L"/hotkey", NULL, SW_SHOWNORMAL);
        },
        // 2. 词典 -> open user data folder
        [this]() {
          explore(WeaselUserDataPath());
        },
        // 3. 短语 -> placeholder (spec 009 F5)
        [this]() {
          explore(install_dir());
        },
        // 4. 全半角 -> toggle full/half width
        [this](bool newFull) {
          if (m_handler) m_handler->SetOption(0, "full_shape", newFull);
        },
        // 5. 符号 -> placeholder (deploy for now)
        [this]() {
          std::filesystem::path deployer = install_dir() / L"WeaselDeployer.exe";
          ShellExecuteW(NULL, NULL, deployer.c_str(), L"/deploy", NULL, SW_SHOWNORMAL);
        },
        // 6. 登录 -> placeholder (v2.1+ cloud sync)
        []() {});
    return true;
  });
}