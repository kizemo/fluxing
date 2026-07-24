#include "stdafx.h"
#include "WeaselServerApp.h"
#include <filesystem>
#include <iostream>  // v0.19.0.25-fix(spec 042 §12 风险):RegisterHotKey 失败 wcerr log
#include "QuickPanelDialog.h"
#include "PhrasesDialogIPC.h"  // v0.19.0.52 (Phase K3 T010): out-of-process IPC 替代 in-process PhrasesDialog
#include "UserDictionary.h"  // v0.19.0.28(spec 044 §3.2):Track 3 wiring
#include "ShortcutSettings.h"  // spec 045 v0.19.0.28: Track 3 wiring
// spec 070 T007: D2D factory 创建 (在 WeaselServerApp::Run 入口)
#include <d2d1.h>
#pragma comment(lib, "d2d1.lib")

// v0.19.0.29:QuickPanel button 3 callback 转发 setter (Shortcuts)。
// static 实现, 把 fn 存到 QuickPanelDialog 内部 static 字段
// (QuickPanelDialog::s_onShortcut),WeaselServerApp::Run() 在 m_server.Start 后调一次。
// v0.19.0.60 (Phase L 调整 1): SetQuickPanelUserDictCallback 删除 — UserDictionary 模块下线。
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

// v0.19.0.63 (Phase L 调整 3): 启动时记录的 Fluxing HKL (即 IPC server 进程当前 active
// TIP 的 HKL); WM_INPUTLANGCHANGE 检测"切换到 Fluxing"用。
//   假设 WeaselServer 启动时 Fluxing 是 active IME — 这跟安装逻辑一致(用户装机后
//   通常设火流猩为默认中文 IME)。
HKL WeaselServerApp::s_fluxingHkl = NULL;
HKL WeaselServerApp::s_prevLangHkl = NULL;

// v0.19.0.25-fix(spec 042 §10.4):子类 WNDPROC,拦截 WM_HOTKEY (ID_HOTKEY_PHRASES_DOT)
// → PhrasesDialogIPC::Show();其它 message 透传给原 WNDPROC(其中含 ServerImpl::OnHotkey
// 处理 ID_HOTKEY_QUICK_PANEL)。不能改 ServerImpl(在 WeaselIPCServer/ 下,scope 之外),
// 用 SetWindowLongPtr(GWLP_WNDPROC) 在 IPC server window 上子类化。
// v0.19.0.52 (Phase K3 T010):PhrasesDialog 走 out-of-process IPC,见 PhrasesDialogIPC.cpp。
// v0.19.0.63 (Phase L 调整 3): WM_INPUTLANGCHANGE 钩子 — 检测 user 切换到火流猩输入法,
//   触发 QuickPanelDialog::Show() 让浮动设置栏自动弹出。
//   Alt+, 仍走 toggle (在 spec 036 + v0.19.0.60+ QuickPanelDialog::ToggleMode 已 work)。
// 设计参考 spec 074 v0.18.31.0 always-show mode 撤回原因 (用户反馈开机就弹太烦);
// 这次只在 user 主动切到 Fluxing 时弹,Win+Space / 点语言栏 都会触发。
LRESULT CALLBACK WeaselServerApp::PhrasesHotkeySubclassProc(HWND hwnd,
                                                           UINT msg,
                                                           WPARAM w,
                                                           LPARAM l) {
  if (msg == WM_HOTKEY) {
    // v0.19.0.25-fix (spec 042 §10.4): Alt+. → PhrasesDialogIPC::Show()
    //   v0.19.0.52 (Phase K3 T010): out-of-process IPC 路径
    if (w == ID_HOTKEY_PHRASES_DOT) {
      fluxing::PhrasesDialogIPC::Show();
      return 0;
    }
    // v0.19.0.60 (Phase L 调整 1): ID_HOTKEY_USER_DICT 分支删除 — Ctrl+Shift+U 热键
    // 已下线 (UserDictionary 模块整体移除)。spec 044 §3.2 路由到 UserDictionary::Show
    // 不再可达。
    if (w == ID_HOTKEY_SHORTCUT) {
      // spec 045 v0.19.0.28:Ctrl+Shift+K → ShortcutSettings::Show
      ShortcutSettings::Show();
      return 0;
    }
    // v0.19.0.33 (Phase A.1 真改 alt+/ 路由 - 之前 turn 没真改):
    //   之前: Alt+/ → UserDictionary::Show() (v0.19.0.32 cd6f61a9 Bug 3b 自己承认错)
    //   修后: Alt+/ → PhrasesDialogIPC::Show() (跟 Alt+. 同一路径, 跟 user 装机后 Alt+. 应一致)
    //   v0.19.0.52 (Phase K3 T010): IPC 路径
    if (w == ID_HOTKEY_USER_DICT_ALT_SLASH) {
      fluxing::PhrasesDialogIPC::Show();
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

// v0.19.0.64 (Phase L 调整 3 bugfix): WM_INPUTLANGCHANGE 子类化 — 单独 hook 在
//   ImmGetDefaultIMEWnd(NULL) 默认 IME 窗口上。
//   原因:WM_INPUTLANGCHANGE 只发给当前 focus 所在 top-level 窗口,**不**发给 WeaselServer
//   的 hidden IPC server window。v0.19.0.63 在 IPC server window 上子类化 = 装机后
//   Win+Space 切到 Fluxing 浮动栏不弹 (user 反馈)。
//   ImmGetDefaultIMEWnd 返回的是 Windows 自动管理的 hidden IME 窗口,**专门**接收 IME 切换
//   广播(包括 WM_INPUTLANGCHANGE)。子类化它即可拿到所有 IME 切换事件。
//   这个 proc **只**处理 WM_INPUTLANGCHANGE,其他 message 全部透传。
// v0.19.0.65 (Phase L 装机 bugfix 2): 加埋点 — 验证 hypothesis 该路径在 TSF-aware Win11
//   是否真的收到 WM_INPUTLANGCHANGE。如果装机后 log 显示 IMM32 路径根本没触发,
//   即可确认 v0.19.0.64 fix 失效,真修在 RimeWithWeasel::FocusIn(同时改了)。
//   Log 写 %APPDATA%\Rime\weasel-install.log (跟 install log 共享,便于排查)。
LRESULT CALLBACK WeaselServerApp::ImeLangChangeSubclassProc(HWND hwnd,
                                                            UINT msg,
                                                            WPARAM w,
                                                            LPARAM l) {
  if (msg == WM_INPUTLANGCHANGE) {
    HKL newHkl = reinterpret_cast<HKL>(l);
    // v0.19.0.65: 埋点 — 记录 WM_INPUTLANGCHANGE 实际接收情况
    {
      wchar_t path[MAX_PATH];
      ExpandEnvironmentStringsW(L"%APPDATA%\\Rime\\weasel-install.log", path, MAX_PATH);
      HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (h != INVALID_HANDLE_VALUE) {
        SYSTEMTIME st; GetSystemTime(&st);
        wchar_t wbuf[512];
        int n = _snwprintf(wbuf, _countof(wbuf),
                           L"[%04d-%02d-%02d %02d:%02d:%02d] [ImeLangSubclass] WM_INPUTLANGCHANGE "
                           L"newHkl=%p s_fluxingHkl=%p s_prevLangHkl=%p\n",
                           st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                           newHkl, s_fluxingHkl, s_prevLangHkl);
        if (n > 0) {
          DWORD written;
          size_t bytes = static_cast<size_t>(n) * sizeof(wchar_t);
          WriteFile(h, wbuf,
                    bytes > MAXDWORD ? MAXDWORD : static_cast<DWORD>(bytes),
                    &written, nullptr);
        }
        CloseHandle(h);
      }
    }
    if (s_fluxingHkl != NULL && newHkl != NULL) {
      bool nowIsFluxing = (newHkl == s_fluxingHkl);
      bool wasFluxing = (s_prevLangHkl == s_fluxingHkl);
      if (nowIsFluxing && !wasFluxing) {
        QuickPanelDialog::Show(false, nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr);
      }
      s_prevLangHkl = newHkl;
    }
    // 透传到 default WndProc,return 0 会阻断 IME 状态同步给其他组件
  }
  WeaselServerApp* owner = s_phrasesHotkeyOwner;
  if (owner && owner->m_imeDefaultOrigWndProc) {
    return ::CallWindowProc(owner->m_imeDefaultOrigWndProc, hwnd, msg, w, l);
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

  // v0.19.0.64 (Phase L 调整 3 bugfix): 额外子类化 ImmGetDefaultIMEWnd(NULL) 拿 IME
  //   切换广播。ImmGetDefaultIMEWnd 返回的 hwnd 在 v0.19.0.63 ship 时尚未在 IPC server
  //   创建,通常要等第一次有 IME 上下文才创建。我们延后到 Run() 入口(此时 WeaselServer
  //   已被设置成 active IME 之一, IME context 已就绪)再调一次 Register 保险。
  // 装机实测:ImmGetDefaultIMEWnd(NULL) 在 WeaselServer 启动后立即可用,直接拿就行。
  // v0.19.0.65 (Phase L 装机 bugfix 2): 埋点 — 验证 ImmGetDefaultIMEWnd 是否真的返回
  //   non-NULL hwnd (即这条 path 是否能 hook 上)。如果 hwnd=NULL,IMM32 路径直接死亡,
  //   无需关心 WM_INPUTLANGCHANGE 是否被广播。
  m_hwndImeDefault = ::ImmGetDefaultIMEWnd(NULL);
  {
    wchar_t path[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\Rime\\weasel-install.log", path, MAX_PATH);
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SYSTEMTIME st; GetSystemTime(&st);
      wchar_t wbuf[512];
      int n = _snwprintf(wbuf, _countof(wbuf),
                         L"[%04d-%02d-%02d %02d:%02d:%02d] [RegisterPhrasesHotkey] "
                         L"ImmGetDefaultIMEWnd(NULL) = 0x%p\n",
                         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                         m_hwndImeDefault);
      if (n > 0) {
        DWORD written;
        size_t bytes = static_cast<size_t>(n) * sizeof(wchar_t);
        WriteFile(h, wbuf,
                  bytes > MAXDWORD ? MAXDWORD : static_cast<DWORD>(bytes),
                  &written, nullptr);
      }
      CloseHandle(h);
    }
  }
  if (m_hwndImeDefault) {
    m_imeDefaultOrigWndProc = reinterpret_cast<WNDPROC>(
        ::GetWindowLongPtr(m_hwndImeDefault, GWLP_WNDPROC));
    if (m_imeDefaultOrigWndProc) {
      ::SetWindowLongPtr(m_hwndImeDefault, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(
                             &WeaselServerApp::ImeLangChangeSubclassProc));
    }
  }

  // Phase A.2 (改进): 失败也写 log file (GUI app 无 console, stderr 看不到)
  // 路径: %APPDATA%\Rime\weasel-install.log (跟 installer 的 install log 共享, 便于排查)
  // 失败最常见原因: ERROR_HOTKEY_ALREADY_REGISTERED (kErrHotkeyAlreadyRegistered)
  //                = 另一个 app 占了这个 hotkey
  // v0.19.0.33 (review 修 B-blocker-A): 整个 buffer 全 wchar_t — 之前 _snprintf
  //   走窄 char buf, %ls 转换在没 _wsetlocale 的 GUI process 上输出 ? 字节,
  //   现在统一 UTF-16, WriteFile 也按 byte-count of wide chars 写。GUI app
  //   看不到 stderr → this file 是 ground truth, 必须 wide-char-clean。
  static constexpr DWORD kErrHotkeyAlreadyRegistered = 1409;  // I-A 抽 named constant

  auto LogHotkeyFailure = [](LPCWSTR label, int id, DWORD err) {
    wchar_t path[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\Rime\\weasel-install.log", path, MAX_PATH);
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st; GetSystemTime(&st);
    const wchar_t* hint = (err == kErrHotkeyAlreadyRegistered)
        ? L" - ERROR_HOTKEY_ALREADY_REGISTERED (another app occupies this hotkey)"
        : L" - see Win32 error codes";
    wchar_t wbuf[512];
    int n = _snwprintf(wbuf, _countof(wbuf),
                       L"[%04d-%02d-%02d %02d:%02d:%02d] RegisterHotKey(%ls, id=%d) failed, err=%lu%ls\n",
                       st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                       label, id, err, hint);
    if (n > 0) {
      DWORD written;
      // wide char byte count = n * sizeof(wchar_t), 截断到 DWORD 安全范围
      size_t bytes = static_cast<size_t>(n) * sizeof(wchar_t);
      WriteFile(h, wbuf,
                bytes > MAXDWORD ? MAXDWORD : static_cast<DWORD>(bytes),
                &written, nullptr);
    }
    CloseHandle(h);
  };

  // W-B (review 修): 抽 RegisterOrLog helper, 4 个失败分支共用同一段 log + 文件
  // 输出。 之前 4 段 ~13 行 × 4 = 52 行重复 (label/cause/LogHotkeyFailure 全相同)。
  // 行为不变 (label + 中文 cause 文案一字不差), 只是去重。
  auto RegisterOrLog = [&](LPCWSTR label, int id, UINT mods, UINT vk) -> bool {
    if (::RegisterHotKey(hwndServer, id, mods, vk)) return true;
    DWORD err = ::GetLastError();
    LPCWSTR cause = (err == kErrHotkeyAlreadyRegistered)
        ? L"ERROR_HOTKEY_ALREADY_REGISTERED (另一个 app 已占用)"
        : L"see Win32 error codes";
    std::wcerr << L"[WeaselServerApp] WARN: RegisterHotKey(" << label
               << L", id=" << id << L") failed, err=" << err
               << L" — " << cause << std::endl;
    LogHotkeyFailure(label, id, err);
    return false;
  };

  // 2) ALT+. (VK_OEM_PERIOD) → PhrasesDialogIPC::Show()  (v0.19.0.52 out-of-process)
  // 3) Ctrl+Shift+K (0x4B) → ShortcutSettings::Show  (spec 045 v0.19.0.28)
  // 4) Alt+/ (VK_OEM_2) → PhrasesDialogIPC::Show  (v0.19.0.33 Phase A.1,
  //    之前 v0.19.0.32 cd6f61a9 错接 UserDictionary, Bug 3b 已撤回)
  // v0.19.0.60 (Phase L 调整 1): Ctrl+Shift+U 移除 (用户决定删除 UserDictionary 模块)
  RegisterOrLog(L"Alt+.",         ID_HOTKEY_PHRASES_DOT,        MOD_ALT, VK_OEM_PERIOD);
  RegisterOrLog(L"Ctrl+Shift+K",  ID_HOTKEY_SHORTCUT,           MOD_CONTROL | MOD_SHIFT, 0x4B);
  RegisterOrLog(L"Alt+/",         ID_HOTKEY_USER_DICT_ALT_SLASH, MOD_ALT, VK_OEM_2);
}

void WeaselServerApp::UnregisterPhrasesHotkey() {
  HWND hwndServer = m_server.GetHWnd();
  if (hwndServer) {
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_PHRASES_DOT);
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_SHORTCUT);   // spec 045
    ::UnregisterHotKey(hwndServer, ID_HOTKEY_USER_DICT_ALT_SLASH);  // v0.19.0.33
    if (m_ipcServerOrigWndProc) {
      ::SetWindowLongPtr(hwndServer, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(m_ipcServerOrigWndProc));
      m_ipcServerOrigWndProc = nullptr;
    }
  }
  // v0.19.0.64 (Phase L 调整 3 bugfix): 还原 IME default window 子类化
  if (m_hwndImeDefault && m_imeDefaultOrigWndProc) {
    ::SetWindowLongPtr(m_hwndImeDefault, GWLP_WNDPROC,
                       reinterpret_cast<LONG_PTR>(m_imeDefaultOrigWndProc));
    m_imeDefaultOrigWndProc = nullptr;
    m_hwndImeDefault = nullptr;
  }
  s_phrasesHotkeyOwner = nullptr;
}

int WeaselServerApp::Run() {
  if (!m_server.Start())
    return -1;

  // v0.19.0.63 (Phase L 调整 3): 启动时记录当前 Fluxing HKL (假设装机后 Fluxing 是 active)。
  //   s_prevLangHkl 也初始化为相同值 — 这样启动即 active 时不会触发 "切换到 Fluxing"。
  //   后续 WM_INPUTLANGCHANGE 钩子在 PhrasesHotkeySubclassProc 比对。
  // v0.19.0.65 (Phase L 装机 bugfix 2): 埋点 — 记录启动时 HKL,装机验证时跟 Win+Space
  //   切到 Fluxing 后的 HKL 比对,确认 hypothesis。
  s_fluxingHkl = ::GetKeyboardLayout(0);
  s_prevLangHkl = s_fluxingHkl;
  {
    wchar_t path[MAX_PATH];
    ExpandEnvironmentStringsW(L"%APPDATA%\\Rime\\weasel-install.log", path, MAX_PATH);
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      SYSTEMTIME st; GetSystemTime(&st);
      wchar_t wbuf[512];
      int n = _snwprintf(wbuf, _countof(wbuf),
                         L"[%04d-%02d-%02d %02d:%02d:%02d] [Run] s_fluxingHkl=%p (initial)\n",
                         st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                         s_fluxingHkl);
      if (n > 0) {
        DWORD written;
        size_t bytes = static_cast<size_t>(n) * sizeof(wchar_t);
        WriteFile(h, wbuf,
                  bytes > MAXDWORD ? MAXDWORD : static_cast<DWORD>(bytes),
                  &written, nullptr);
      }
      CloseHandle(h);
    }
  }

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

  // v0.19.0.52 (Phase K3 T010):设置 phrases.yaml 路径。
  // 必须在 RegisterPhrasesHotkey() 之前调 — 热键按下 → PhrasesDialogIPC::Show() →
  // 内部用 s_yamlPath 加载 YAML。若此时为空会 fallback 到默认 "你好"。
  // 路径跟原 PhrasesDialog.cpp Show() 一致:%APPDATA%\Rime\phrases.yaml (从
  // HKCU\Software\Rime\Weasel\RimeUserDir 读,可被 user 配置改写)。
  {
    fs::path phrasesYaml = WeaselUserDataPath() / L"phrases.yaml";
    fluxing::PhrasesDialogIPC::SetYamlPath(phrasesYaml.wstring());
  }

  // v0.19.0.25-fix(spec 042 §3 + §10.4):Alt+. 全局热键注册。
  // 必须在 m_server.Run() 之前调(m_server.Run() 进入消息循环前 IPC server window 已经
  // 在 m_server.Start() 时创建,WM_CREATE 已经发出 → ServerImpl::OnCreate 注册了 Alt+,;
  // 现在再加 Alt+.)。失败仅 log warning,不 crash(spec 042 §12 风险)。
  RegisterPhrasesHotkey();

  // v0.19.0.29(mockups-v0.19.0.28 设计稿):把 QuickPanel hit==3 (Settings) → Shortcut 接线。
  // QuickPanelDialog::SetOn* setter 内部存 static std::function,
  // callback 在 QuickPanelDialog::WndProc WM_LBUTTONUP click 路由触发。
  // 这里调一次,SetOn* 是 idempotent(fn null 即清空),但 lifecycle 与 WeaselServerApp
  // 同,只在 Run 入口调一次即可。
  // v0.19.0.60 (Phase L 调整 1): UserDict callback (hit==2) 删除 — UserDictionary 模块下线。
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
          [this]() {
            // v0.19.0.39 (Phase F fix Bug 1): 按钮路径下, QuickPanelDialog 仍是
            //   foreground, 直接 Show() PhrasesDialog 会让 keyboard events (↑↓/Enter)
            //   仍发到 QuickPanel. 修法 (Option C): 先 Hide() QuickPanelDialog 释放
            //   foreground, 再 Show() PhrasesDialog. Show() 内部 (PhrasesDialog.cpp
            //   Phase F fix) 调 AllowSetForegroundWindow + SetForegroundWindow 兜底
            //   抢 foreground, 让 keyboard 路由到 PhrasesDialog.
            // v0.19.0.52 (Phase K3 T010): 走 IPC 路径
            if (QuickPanelDialog::ActiveHwnd() &&
                IsWindowVisible(QuickPanelDialog::ActiveHwnd())) {
              QuickPanelDialog::Hide();
            }
            fluxing::PhrasesDialogIPC::Show();
          },
          [this](bool newFull) {
            if (m_handler) m_handler->SetOption(0, "full_shape", newFull);
          },
          [this]() { UserDictionary::Show(); },
          []() {});
    }
    return true;
  });
}