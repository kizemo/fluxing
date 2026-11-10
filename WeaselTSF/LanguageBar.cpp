#include "stdafx.h"
#include <resource.h>
#include <thread>
#include <shellapi.h>
#include "WeaselTSF.h"
#include "LanguageBar.h"
#include "CandidateList.h"
#include <WeaselUtility.h>
#include <FluxingInstallPaths.h>
#include <FluxingLangBarStyle.h>

static const DWORD LANGBARITEMSINK_COOKIE = 0x42424242;

// 本 DLL 内的静态锚点 ——
// GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)
// 需要一个落在本模块内的地址;成员函数指针不能直接转成 LPCWSTR,故用数据锚点。
static int g_module_address_anchor = 0;

static void HMENU2ITfMenu(HMENU hMenu, ITfMenu* pTfMenu) {
  /* NOTE: Only limited functions are supported */
  int N = GetMenuItemCount(hMenu);
  for (int i = 0; i < N; i++) {
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
    mii.dwTypeData = NULL;
    if (GetMenuItemInfo(hMenu, i, TRUE, &mii)) {
      UINT id = mii.wID;
      if (mii.fType == MFT_SEPARATOR)
        pTfMenu->AddMenuItem(id, TF_LBMENUF_SEPARATOR, NULL, NULL, NULL, 0,
                             NULL);
      else if (mii.fType == MFT_STRING) {
        mii.dwTypeData = (LPWSTR)malloc(sizeof(WCHAR) * (mii.cch + 1));
        mii.cch++;
        if (GetMenuItemInfo(hMenu, i, TRUE, &mii))
          pTfMenu->AddMenuItem(id, 0, NULL, NULL, mii.dwTypeData, mii.cch,
                               NULL);
        free(mii.dwTypeData);
      }
    }
  }
}

// spec 088:原来这里有一个 GetWeaselRegName(),32 位分支读的是上游键
// Software\WOW6432Node\Rime\Weasel、64 位分支读 Software\Fluxing\Weasel,而
// 读的值 "WeaselRoot" 本 fork 的安装器从不写入 ⇒ 拿到空串后「重启算法服务」
// 与「程序文件夹」静默无反应。现已下沉到 include/FluxingInstallPaths.h
// (显式遍历 32/64 两个注册表视图 + 按安装根推导程序目录)。

static bool open(const std::wstring& path) {
  std::wstring quoted_path = L"\"" + path + L"\"";
  return (uintptr_t)ShellExecuteW(NULL, L"open", quoted_path.c_str(), NULL,
                                  NULL, SW_SHOWNORMAL) > 32;
}

CLangBarItemButton::CLangBarItemButton(com_ptr<WeaselTSF> pTextService,
                                       REFGUID guid,
                                       weasel::UIStyle& style)
    : _status(0),
      _style(style),
      _current_schema_zhung_icon(),
      _current_schema_ascii_icon() {
  DllAddRef();

  _pLangBarItemSink = NULL;
  _cRef = 1;
  _pTextService = pTextService;
  _guid = guid;
  ascii_mode = false;
}

CLangBarItemButton::~CLangBarItemButton() {
  DllRelease();
}

STDAPI CLangBarItemButton::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) ||
      IsEqualIID(riid, IID_ITfLangBarItemButton))
    *ppvObject = (ITfLangBarItemButton*)this;
  else if (IsEqualIID(riid, IID_ITfSource))
    *ppvObject = (ITfSource*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) CLangBarItemButton::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) CLangBarItemButton::Release() {
  LONG cr = --_cRef;
  assert(_cRef >= 0);
  if (_cRef == 0)
    delete this;
  return cr;
}

STDAPI CLangBarItemButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
  pInfo->clsidService = c_clsidTextService;
  pInfo->guidItem = _guid;
  // ADR 0053(修订 ADR 0044):彻底把本语言栏项从通知区域(托盘)去掉。
  //
  // 事实链:
  //   ① 通知区域里那个方框「中」/「英」图标 = 本语言栏项
  //      (GetIcon 返回 IDI_ZH/IDI_EN,tooltip 为「左键切换模式，右键打开菜单」)。
  //   ② ADR 0044 的做法 = 摘掉 dwStyle 里的 TF_LBI_STYLE_SHOWNINTRAY。
  //      微软文档对该标志写明「目前不支援此标志」⇒ 摘掉它是 no-op,
  //      图标不会消失(ADR 0044 前提被证伪,由 ADR 0053 修订)。
  //   ③ 文档里唯一「动态隐藏语言栏项」的机制 = dwStyle 置
  //      TF_LBI_STYLE_HIDDENSTATUSCONTROL,再经 SetLangbarStatus 置
  //      TF_LBI_STATUS_HIDDEN。
  //
  // ⚠ 别改用 TF_LBI_STYLE_INVISIBLE:那会把语言栏里的入口也一并藏掉。
  // ⚠ 保留 BTN_BUTTON / BTN_MENU:左键行为(打开快捷设置栏)不变。
  pInfo->dwStyle = fluxing::kLangBarItemStyle;
  pInfo->ulSort = 1;
  lstrcpyW(pInfo->szDescription, L"WeaselTSF Button");
  return S_OK;
}

STDAPI CLangBarItemButton::GetStatus(DWORD* pdwStatus) {
  *pdwStatus = _status;
  return S_OK;
}

STDAPI CLangBarItemButton::Show(BOOL fShow) {
  // ADR 0053:永久隐藏 —— 通知区域(托盘)里那个方框「中」/「英」图标
  // 就是本语言栏项,用户指令要求去除。
  // ⚠ 忽略 fShow:用户可以在「语言栏选项」里重新启用本项,系统随后会
  //   Show(TRUE) 把托盘图标复活 ⇒ 这里恒置隐藏位。
  (void)fShow;
  SetLangbarStatus(fluxing::kLangBarStatusHidden, TRUE);
  return S_OK;
}

static LANGID GetActiveProfileLangId() {
  CComPtr<ITfInputProcessorProfileMgr> pInputProcessorProfileMgr;
  HRESULT hr = pInputProcessorProfileMgr.CoCreateInstance(
      CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_ALL);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

  TF_INPUTPROCESSORPROFILE profile;
  hr = pInputProcessorProfileMgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,
                                                   &profile);
  if (FAILED(hr))
    return MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
  return profile.langid;
}

STDAPI CLangBarItemButton::GetTooltipString(BSTR* pbstrToolTip) {
  LANGID langid = get_language_id();
  if (langid == TEXTSERVICE_LANGID_HANS) {
    *pbstrToolTip = SysAllocString(L"左键切换模式，右键打开菜单");
  } else if (langid == TEXTSERVICE_LANGID_HANT) {
    *pbstrToolTip = SysAllocString(L"左鍵切換模式，右鍵打開菜單");
  } else {
    *pbstrToolTip = SysAllocString(
        L"Left-click to switch modes\n\nRight-click for more options");
  }

  return (*pbstrToolTip == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDAPI CLangBarItemButton::OnClick(TfLBIClick click,
                                   POINT pt,
                                   const RECT* prcArea) {
  if (click == TF_LBI_CLK_LEFT) {
    // L51: spec 036 US036-B - left-click on the lang bar item triggers
    // the QuickPanelDialog instead of toggling ASCII mode. The ASCII
    // toggle is still reachable via:
    //   - Shift+Space global hotkey (spec 005)
    //   - The ASCII toggle inside the QuickPanelDialog itself
    // This matches user expectation (visible tray item produces the
    // QuickPanel when clicked) and matches the WeaselServer tray icon
    // behavior that spec 036 already wired in SystemTraySDK.cpp.
    // Routing: _HandleLangBarMenuSelect default case -> m_client.TrayCommand
    // -> IPC -> WeaselServer.AddMenuHandler(ID_WEASELTRAY_QUICK_PANEL) ->
    // QuickPanelDialog::Show.
    _pTextService->_HandleLangBarMenuSelect(ID_WEASELTRAY_QUICK_PANEL);
    if (_pLangBarItemSink) {
      _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
    }
  } else if (click == TF_LBI_CLK_RIGHT) {
    // spec 100 / ADR 0038:语言栏(任务栏输入指示器)右键弹菜单**已下线**。
    // 原因:菜单唯一化到 WeaselServer 的 IDR_MENU_POPUP —— 快捷设置栏品牌图标
    // 右键 / 托盘右键共用同一份菜单资源、同一套命令路由与勾选态。此前
    // WeaselTSF 另带三套 MENU 资源(英/繁/简),与托盘菜单措辞不一致,
    // 造成「同一功能两个入口、两份文案」,用户容易误改。
    // 现在这里**不再**弹任何菜单;任务栏右键只剩系统自己的语言列表。
    return S_OK;
  }
  return S_OK;
}

STDAPI CLangBarItemButton::InitMenu(ITfMenu* pMenu) {
  // spec 100 / ADR 0038:语言栏不再拥有任何菜单项 —— WeaselTSF.rc 里三套
  // MENU 资源(IDR_MENU_POPUP / _HANT / _HANS)已删除,LoadMenuW 必然返回
  // NULL,继续 GetSubMenu 会直接崩(所以这里**必须**连 LoadMenu 一起停掉)。
  // 返回 S_OK 且不注册任何项 ⇒ 系统只显示它自己的默认语言列表。
  (void)pMenu;
  return S_OK;
}

STDAPI CLangBarItemButton::OnMenuSelect(UINT wID) {
  _pTextService->_HandleLangBarMenuSelect(wID);
  return S_OK;
}

STDAPI CLangBarItemButton::GetIcon(HICON* phIcon) {
  if (ascii_mode) {
    if (_style.current_ascii_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_EN), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_ascii_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  } else {
    if (_style.current_zhung_icon.empty())
      *phIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_ZH), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON),
                                  GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    else
      *phIcon =
          (HICON)LoadImageW(NULL, _style.current_zhung_icon.c_str(), IMAGE_ICON,
                            GetSystemMetrics(SM_CXSMICON),
                            GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
  }
  return (*phIcon == NULL) ? E_FAIL : S_OK;
}

STDAPI CLangBarItemButton::GetText(BSTR* pbstrText) {
  // ADR 0012: the label follows the input mode, so the language bar shows
  // which mode is active instead of a fixed product name.
  // U+82F1 = 英, U+4E2D = 中 (escaped: this file is UTF-8 without BOM).
  *pbstrText = SysAllocString(ascii_mode ? L"\u82f1" : L"\u4e2d");
  return (*pbstrText == NULL) ? E_OUTOFMEMORY : S_OK;
}

STDAPI CLangBarItemButton::AdviseSink(REFIID riid,
                                      IUnknown* punk,
                                      DWORD* pdwCookie) {
  if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
    return CONNECT_E_CANNOTCONNECT;
  if (_pLangBarItemSink != NULL)
    return CONNECT_E_ADVISELIMIT;

  if (punk->QueryInterface(IID_ITfLangBarItemSink,
                           (LPVOID*)&_pLangBarItemSink) != S_OK) {
    _pLangBarItemSink = NULL;
    return E_NOINTERFACE;
  }
  *pdwCookie = LANGBARITEMSINK_COOKIE;
  return S_OK;
}

STDAPI CLangBarItemButton::UnadviseSink(DWORD dwCookie) {
  if (dwCookie != LANGBARITEMSINK_COOKIE || _pLangBarItemSink == NULL)
    return CONNECT_E_NOCONNECTION;
  _pLangBarItemSink = NULL;
  return S_OK;
}

void CLangBarItemButton::UpdateWeaselStatus(weasel::Status stat) {
  if (stat.ascii_mode != ascii_mode) {
    ascii_mode = stat.ascii_mode;
  }
  if (_current_schema_zhung_icon != _style.current_zhung_icon) {
    _current_schema_zhung_icon = _style.current_zhung_icon;
  }
  if (_current_schema_ascii_icon != _style.current_ascii_icon) {
    _current_schema_ascii_icon = _style.current_ascii_icon;
  }
  if (_pLangBarItemSink) {
    // ADR 0012: TF_LBI_TEXT as well, otherwise the language bar keeps the
    // old label after the input mode changed.
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON | TF_LBI_TEXT);
  }
}

void CLangBarItemButton::SetLangbarStatus(DWORD dwStatus, BOOL fSet) {
  BOOL isChange = FALSE;

  if (fSet) {
    if (!(_status & dwStatus)) {
      _status |= dwStatus;
      isChange = TRUE;
    }
  } else {
    if (_status & dwStatus) {
      _status &= ~dwStatus;
      isChange = TRUE;
    }
  }

  if (isChange && _pLangBarItemSink) {
    _pLangBarItemSink->OnUpdate(TF_LBI_STATUS | TF_LBI_ICON);
  }

  return;
}

std::wstring WeaselTSF::_GetRootDir() {
  std::wstring dir = fluxing::ResolveFluxingWeaselRoot();
  if (!dir.empty() && fluxing::LooksLikeWeaselRoot(dir))
    return dir;
  // 兜底:本 DLL 自身所在目录就是程序目录
  //   (weasel.dll / weaselx64.dll 与 WeaselServer.exe 同在 <安装根>\weasel)
  HMODULE hmod = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&g_module_address_anchor),
                         &hmod)) {
    WCHAR path[MAX_PATH] = {0};
    if (GetModuleFileNameW(hmod, path, _countof(path)) > 0) {
      const std::wstring dll_path(path);
      const size_t sep = dll_path.find_last_of(L"\\/");
      if (sep != std::wstring::npos) {
        const std::wstring dll_dir = dll_path.substr(0, sep);
        if (fluxing::LooksLikeWeaselRoot(dll_dir))
          return dll_dir;
      }
    }
  }
  return dir;
}

void WeaselTSF::_HandleLangBarMenuSelect(UINT wID) {
  std::wstring dir{};
  switch (wID) {
    case ID_WEASELTRAY_RERUN_SERVICE:
    case ID_WEASELTRAY_INSTALLDIR: {
      const std::wstring root = _GetRootDir();
      if (!fluxing::LooksLikeWeaselRoot(root)) {
        // spec 088:以前这里是「读不到就整段跳过」—— 用户点了完全没反应。
        MessageBoxW(NULL, L"未找到火流猩程序目录，请重新安装后再试。",
                    L"火流猩输入法", MB_ICONERROR | MB_OK);
        break;
      }
      if (wID == ID_WEASELTRAY_RERUN_SERVICE) {
        const std::wstring target = fluxing::WeaselServiceLaunchTarget(root);
        if (target.empty()) {
          MessageBoxW(NULL, L"未找到算法服务启动脚本，请重新安装后再试。",
                      L"火流猩输入法", MB_ICONERROR | MB_OK);
          break;
        }
        std::thread th([root, target]() {
          ShellExecuteW(NULL, L"open", target.c_str(), NULL, root.c_str(),
                        SW_HIDE);
        });
        th.detach();
      } else {
        open(root);
      }
      break;
    }
    case ID_WEASELTRAY_USERCONFIG:
      if (FAILED(RegGetStringValue(HKEY_CURRENT_USER,
                                   L"Software\\Fluxing\\Weasel", L"RimeUserDir",
                                   dir)) ||
          dir.empty()) {
        WCHAR _path[MAX_PATH] = {0};
        ExpandEnvironmentStringsW(L"%AppData%\\Rime", _path, _countof(_path));
        dir = std::wstring(_path);
      }
      if (!dir.empty() && fs::exists(dir))
        open(dir);
      else
        MessageBoxW(NULL, (L"Not found: " + dir).c_str(), L"RimeUserDir",
                    MB_ICONERROR | MB_OK);
      break;
    case ID_WEASELTRAY_LOGDIR:
      open(WeaselLogPath().wstring());
      break;
    case ID_WEASELTRAY_WIKI:
      open(L"https://rime.im/docs/");
      break;
    case ID_WEASELTRAY_FORUM:
      open(L"https://rime.im/discuss/");
      break;
    default:
      m_client.TrayCommand(wID);
      break;
  }
}

HWND WeaselTSF::_GetFocusedContextWindow() {
  HWND hwnd = NULL;
  ITfDocumentMgr* pDocMgr;
  if (_pThreadMgr->GetFocus(&pDocMgr) == S_OK && pDocMgr != NULL) {
    ITfContext* pContext;
    if (pDocMgr->GetTop(&pContext) == S_OK && pContext != NULL) {
      ITfContextView* pContextView;
      if (pContext->GetActiveView(&pContextView) == S_OK &&
          pContextView != NULL) {
        pContextView->GetWnd(&hwnd);
        pContextView->Release();
      }
      pContext->Release();
    }
    pDocMgr->Release();
  }

  if (hwnd == NULL) {
    HWND hwndForeground = GetForegroundWindow();
    if (GetWindowThreadProcessId(hwndForeground, NULL) == GetCurrentThreadId())
      hwnd = hwndForeground;
  }

  return hwnd;
}

BOOL WeaselTSF::_InitLanguageBar() {
  // v14 / ADR 0053 修订(2026-11-10 真机双证):通知区域那个方框「中」/「英」
  // 就是本语言栏项。两条文档化的隐藏路径都已被真机证伪 ——
  //   ① ADR 0044 摘 TF_LBI_STYLE_SHOWNINTRAY:微软文档标注「目前不支援此标志」;
  //   ② ADR 0053 置 HIDDENSTATUSCONTROL + 恒 TF_LBI_STATUS_HIDDEN:装机副本
  //      SHA256 与构建产物一致、注册表指向同一文件、explorer 已重启(跑新镜像)、
  //      火流猩为活动 IME 的前提下,图标**依然在**。
  // ⇒ 唯一有效手段:根本不注册该项(不 new、不 AddItem)。
  // 代价:托盘上的左键「切换模式/开快捷设置栏」与右键菜单入口随项一起消失;
  //      快捷设置栏仍可由热键打开,中英状态由 compartment 承担(见 _UpdateLanguageBar)。
  // ⚠ 本函数必须返回 TRUE:WeaselTSF.cpp 的 ActivateEx 里
  //   `if (!_InitLanguageBar()) goto ExitError;` 会走 Deactivate(),
  //   整个文本服务失活、中文输入直接死掉。
  if (!fluxing::kLangBarRegisterItem)
    return TRUE;

  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;
  BOOL fRet = FALSE;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) != S_OK)
    return FALSE;

  if ((_pLangBarButton = new CLangBarItemButton(this, GUID_LBI_INPUTMODE,
                                                _cand->style())) == NULL)
    return FALSE;

  if (pLangBarItemMgr->AddItem(_pLangBarButton) != S_OK) {
    _pLangBarButton = NULL;
    return FALSE;
  }

  // ADR 0053:注册后立即请求隐藏(Show(FALSE) = 隐藏语义)。
  // ⚠ 绝不能因为隐藏而让本函数返回 FALSE —— WeaselTSF.cpp 的 ActivateEx 里
  //   `if (!_InitLanguageBar()) goto ExitError;` 会走 Deactivate(),
  //   整个文本服务失活、中文输入直接死掉。
  _pLangBarButton->Show(FALSE);
  fRet = TRUE;

  return fRet;
}

void WeaselTSF::_UninitLanguageBar() {
  com_ptr<ITfLangBarItemMgr> pLangBarItemMgr;

  if (_pLangBarButton == NULL)
    return;

  if (_pThreadMgr->QueryInterface(&pLangBarItemMgr) == S_OK) {
    pLangBarItemMgr->RemoveItem(_pLangBarButton);
  }

  _pLangBarButton = NULL;
}

void WeaselTSF::_UpdateLanguageBar(weasel::Status stat) {
  // ADR 0053:compartment(中/英、全/半 对系统的声明)与语言栏项**解耦**。
  // 以前这段代码挂在 `if (!_pLangBarButton) return;` 之后 —— 语言栏项一旦
  // 不可用,系统就收不到 native / full shape 的状态声明(中英状态会丢)。
  DWORD flags;
  _GetCompartmentDWORD(flags, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);
  if (stat.ascii_mode)
    flags &= (~TF_CONVERSIONMODE_NATIVE);
  else
    flags |= TF_CONVERSIONMODE_NATIVE;
  if (stat.full_shape)
    flags |= TF_CONVERSIONMODE_FULLSHAPE;
  else
    flags &= (~TF_CONVERSIONMODE_FULLSHAPE);
  _SetCompartmentDWORD(flags, GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION);

  if (_pLangBarButton)
    _pLangBarButton->UpdateWeaselStatus(stat);
}

void WeaselTSF::_ShowLanguageBar(BOOL show) {
  if (!_pLangBarButton)
    return;
  _pLangBarButton->Show(show);
}

void WeaselTSF::_EnableLanguageBar(BOOL enable) {
  if (!_pLangBarButton)
    return;
  _pLangBarButton->SetLangbarStatus(TF_LBI_STATUS_DISABLED, !enable);
}
