// TestTsFocusOutHost.cpp -- B-4 path 2 probe: a real, self-owned TSF host.
//
// Purpose
//   B-4 ("real TSF host emits FocusOut") needs a *genuine* TSF host process whose
//   focus / profile transitions drive the installed WeaselTSF shim
//   (weaselx64.<ver>.dll) through ITfTextInputProcessor::Deactivate() and
//   ITfKeyEventSink::OnSetFocus(FALSE). That shim then sends FOCUS_OUT /
//   END_SESSION to the running WeaselServer over IPC. WeaselTSF sources are NOT
//   touched by this probe: we only host them.
//
// This file is deliberately structured so the *safe* part can run unattended and
// the *user-visible* part cannot:
//
//   -DryRun (default)  READ-ONLY. Activates TSF on OUR thread only, enumerates the
//                      registered TIPs, reports which keyboard profile is currently
//                      active system-wide, and AdviseSink's the profile-notify sink.
//                      It never switches the user's input method and never steals
//                      foreground. Safe to run any time.
//
//   -ActivateAndCycle  MUTATING. Creates a window + edit control, associates a TSF
//                      document manager, ACTIVATES THE FLUXING PROFILE (=> takes the
//                      input method away from whatever the user is typing with),
//                      forces a focus-out, then restores the previously active
//                      profile. Requires three independent guards to pass:
//                        1) explicit -AllowProfileSwitch on the command line
//                        2) env  FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1
//                        3) not an RDP/console-shadow session (or -AllowRemoteSession)
//                      The desktop guard (below) applies to BOTH modes.
//
// Desktop guard: this machine runs Yingdao RPA, which creates virtual desktops. A
// probe that runs off the "Default" desktop observes a different input stack, so its
// evidence is worthless. We check for real: GetThreadDesktop + 
// GetUserObjectInformationW(UOI_NAME) must be L"Default", else exit 3.
// (No placeholder signatures -- a fake guard is worse than none.)
//
// Output contract (consumed by scripts\verify\b4-tsf-host.ps1): one key=value line
// per observation on stdout, ASCII except profile descriptions which are UTF-8.
//
// Exit codes
//   0 ok / 1 probe failure / 2 profile-switch not authorized
//   3 not on Default desktop / 4 remote session / 5 CoInitializeEx failed
//   6 ITfThreadMgr unavailable

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <ole2.h>
#include <msctf.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Fluxing TIP identity -- literal values verified against this machine's registry, not
// guessed. Source of truth:
//   HKLM\SOFTWARE\Microsoft\CTF\TIP\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}
//     LanguageProfile\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}  Enable=1
//   (0x00000404 / 0x00000c04 / 0x00001004 / 0x00001404 all exist with Enable=0)
// and WeaselTSF\Globals.cpp:10-22 (c_clsidTextService / c_guidProfile).
// ---------------------------------------------------------------------------
const GUID kClsidFluxingTip = {
    0xa3f4cded,
    0xb1e9,
    0x41ee,
    {0x9c, 0xa6, 0x7b, 0x4d, 0x0d, 0xe6, 0xcb, 0x0a}};
const GUID kGuidFluxingProfile = {
    0x3d02cab6,
    0x2b8e,
    0x4781,
    {0xba, 0x20, 0x1c, 0x92, 0x67, 0x52, 0x94, 0x67}};
// The registered-and-enabled LANGID. Matches TEXTSERVICE_LANGID_HANS
// (WeaselTSF\Globals.h:14-15) = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED).
const LANGID kFluxingLangid = 0x0804;

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------
std::string WideToUtf8(const wchar_t* w) {
  if (w == nullptr || w[0] == L'\0') {
    return std::string();
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) {
    return std::string();
  }
  std::vector<char> buf(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, buf.data(), n, nullptr, nullptr);
  return std::string(buf.data());
}

std::string GuidToStr(const GUID& g) {
  OLECHAR buf[64] = {0};
  if (FAILED(StringFromGUID2(g, buf, 64))) {
    return std::string("{unavailable}");
  }
  return WideToUtf8(buf);
}

unsigned long long NowMs() { return static_cast<unsigned long long>(GetTickCount64()); }

bool HasSwitch(int argc, wchar_t** argv, const wchar_t* name) {
  for (int i = 1; i < argc; ++i) {
    if (_wcsicmp(argv[i], name) == 0) {
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// desktop guard (real API calls)
// ---------------------------------------------------------------------------
bool ThreadDesktopName(std::string* out) {
  HDESK desk = GetThreadDesktop(GetCurrentThreadId());
  if (desk == nullptr) {
    return false;
  }
  WCHAR name[256] = {0};
  DWORD needed = 0;
  if (GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name), &needed) == 0) {
    return false;
  }
  *out = WideToUtf8(name);
  return true;
}

// ---------------------------------------------------------------------------
// profile-notify sink: records the order TSF dispatches to *this* thread.
// ---------------------------------------------------------------------------
class NotifyProbe : public ITfActiveLanguageProfileNotifySink {
 public:
  NotifyProbe() : refs_(1), on_(0), off_(0), fluxing_on_(0), fluxing_off_(0) {}
  virtual ~NotifyProbe() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_INVALIDARG;
    }
    *ppv = nullptr;
    if (riid == __uuidof(IUnknown) ||
        riid == __uuidof(ITfActiveLanguageProfileNotifySink)) {
      *ppv = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
    } else {
      return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
  }

  ULONG STDMETHODCALLTYPE AddRef() override {
    return static_cast<ULONG>(InterlockedIncrement(&refs_));
  }

  ULONG STDMETHODCALLTYPE Release() override {
    const LONG r = InterlockedDecrement(&refs_);
    if (r == 0) {
      delete this;
    }
    return static_cast<ULONG>(r);
  }

  // msctf.h (19041 L8379 / 26100 L8834):
  //   virtual HRESULT STDMETHODCALLTYPE OnActivated(REFCLSID clsid,
  //                                                 REFGUID guidProfile,
  //                                                 BOOL fActivated) = 0;
  HRESULT STDMETHODCALLTYPE OnActivated(REFCLSID clsid,
                                        REFGUID guidProfile,
                                        BOOL fActivated) override {
    const bool fluxing = IsEqualGUID(clsid, kClsidFluxingTip) ? true : false;
    if (fActivated) {
      ++on_;
      if (fluxing) {
        ++fluxing_on_;
      }
    } else {
      ++off_;
      if (fluxing) {
        ++fluxing_off_;
      }
    }
    std::printf("EVENT tsf_on_activated t=%llu fluxing=%d activated=%d clsid=%s profile=%s\n",
                NowMs(), fluxing ? 1 : 0, fActivated ? 1 : 0,
                GuidToStr(clsid).c_str(), GuidToStr(guidProfile).c_str());
    std::fflush(stdout);
    return S_OK;
  }

  int on() const { return on_; }
  int off() const { return off_; }
  int fluxing_on() const { return fluxing_on_; }
  int fluxing_off() const { return fluxing_off_; }

 private:
  volatile LONG refs_;
  int on_;
  int off_;
  int fluxing_on_;
  int fluxing_off_;
};

// ---------------------------------------------------------------------------
// host window (only created in -ActivateAndCycle)
// ---------------------------------------------------------------------------
const wchar_t kWndClass[] = L"FluxingB4TsFocusOutHost";
HWND g_edit = nullptr;

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_COMMAND:
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND CreateHostWindow() {
  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = HostWndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<UINT_PTR>(COLOR_WINDOW + 1));
  wc.lpszClassName = kWndClass;
  if (RegisterClassExW(&wc) == 0) {
    const DWORD err = GetLastError();
    if (err != ERROR_CLASS_ALREADY_EXISTS) {
      std::printf("FAIL register_class err=%lu\n", err);
      return nullptr;
    }
  }
  HWND hwnd = CreateWindowExW(0, kWndClass, L"Fluxing B-4 TSF host probe",
                              WS_OVERLAPPEDWINDOW, 60, 60, 480, 260, nullptr,
                              nullptr, inst, nullptr);
  if (hwnd == nullptr) {
    std::printf("FAIL create_window err=%lu\n", GetLastError());
    return nullptr;
  }
  g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           12, 12, 440, 28, hwnd,
                           reinterpret_cast<HMENU>(static_cast<UINT_PTR>(1001)), inst, nullptr);
  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  return hwnd;
}

void PumpMessages(DWORD ms) {
  const unsigned long long end = NowMs() + ms;
  for (;;) {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE) != 0) {
      TranslateMessage(&m);
      DispatchMessageW(&m);
    }
    if (NowMs() >= end) {
      break;
    }
    Sleep(10);
  }
}

// ---------------------------------------------------------------------------
// read-only reporting shared by both modes
// ---------------------------------------------------------------------------
int g_failures = 0;
void NoteFailure(const char* why) {
  ++g_failures;
  std::printf("FAIL %s\n", why);
}

bool ReportFluxingRegistration(ITfInputProcessorProfiles* profiles) {
  IEnumGUID* penum = nullptr;
  const HRESULT hr = profiles->EnumInputProcessorInfo(&penum);
  if (FAILED(hr) || penum == nullptr) {
    std::printf("TIP_ENUM_HR=0x%08lx\n", static_cast<unsigned long>(hr));
    NoteFailure("EnumInputProcessorInfo");
    return false;
  }
  int total = 0;
  bool found = false;
  GUID item;
  ULONG got = 0;
  while (penum->Next(1, &item, &got) == S_OK && got == 1) {
    ++total;
    if (IsEqualGUID(item, kClsidFluxingTip)) {
      found = true;
    }
  }
  penum->Release();
  std::printf("TIP_ENUM_COUNT=%d\n", total);
  std::printf("FLUXING_TIP_REGISTERED=%d\n", found ? 1 : 0);
  if (!found) {
    NoteFailure("Fluxing TIP CLSID absent from EnumInputProcessorInfo");
  }
  return found;
}

void ReportFluxingActiveState(ITfInputProcessorProfiles* profiles) {
  // msctf.h (19041 L7761 / 26100 L8169):
  //   GetActiveLanguageProfile(REFCLSID rclsid, LANGID *plangid, GUID *pguidProfile)
  // S_OK = active somewhere, S_FALSE = registered but not active.
  LANGID langid = 0;
  GUID guid;
  ZeroMemory(&guid, sizeof(guid));
  const HRESULT hr = profiles->GetActiveLanguageProfile(kClsidFluxingTip, &langid, &guid);
  std::printf("FLUXING_GETACTIVE_HR=0x%08lx\n", static_cast<unsigned long>(hr));
  if (hr == S_OK) {
    std::printf("FLUXING_ACTIVE=1 langid=0x%04x profile=%s\n", langid,
                GuidToStr(guid).c_str());
  } else if (hr == S_FALSE) {
    std::printf("FLUXING_ACTIVE=0\n");
  } else {
    std::printf("FLUXING_ACTIVE=unknown\n");
    NoteFailure("GetActiveLanguageProfile hard failure");
  }
}

void ReportSystemActiveKeyboardProfile(ITfInputProcessorProfileMgr* mgr) {
  // msctf.h (19041 L8750 / 26100 L9221):
  //   GetActiveProfile(REFGUID catid, TF_INPUTPROCESSORPROFILE *pProfile)
  // TF_INPUTPROCESSORPROFILE members verified identical in both SDK copies
  // (19041 L8665-8677 / 26100 L9136-9147): dwProfileType, langid, clsid,
  // guidProfile, catid, hklSubstitute, dwCaps, hkl, dwFlags.
  TF_INPUTPROCESSORPROFILE prof;
  ZeroMemory(&prof, sizeof(prof));
  const HRESULT hr = mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &prof);
  std::printf("ACTIVE_KBD_PROFILE_HR=0x%08lx\n", static_cast<unsigned long>(hr));
  if (FAILED(hr)) {
    // Not fatal for dry-run truthfulness, but it means we cannot prove what the
    // user is currently typing with, so restore-on-exit would be unsafe.
    NoteFailure("GetActiveProfile(TFCAT_TIP_KEYBOARD)");
    return;
  }
  const bool is_fluxing = IsEqualGUID(prof.clsid, kClsidFluxingTip) ? true : false;
  std::printf("ACTIVE_KBD_TYPE=0x%04lx\n", static_cast<unsigned long>(prof.dwProfileType));
  std::printf("ACTIVE_KBD_CLSID=%s\n", GuidToStr(prof.clsid).c_str());
  std::printf("ACTIVE_KBD_LANGID=0x%04x\n", prof.langid);
  std::printf("ACTIVE_KBD_PROFILE=%s\n", GuidToStr(prof.guidProfile).c_str());
  std::printf("ACTIVE_KBD_HKL=0x%016llx\n",
              static_cast<unsigned long long>(reinterpret_cast<unsigned long long>(prof.hkl)));
  std::printf("ACTIVE_IS_FLUXING=%d\n", is_fluxing ? 1 : 0);
}

void Usage() {
  std::printf(
      "TestTsFocusOutHost - B-4 path 2 real-TSF-host probe\n"
      "  (default)            -DryRun : read-only, never changes the user's IME\n"
      "  -ActivateAndCycle    MUTATING: activates Fluxing, forces focus-out, restores\n"
      "                       requires -AllowProfileSwitch AND\n"
      "                       FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1\n"
      "  -UseLegacyMgr        activate via ITfInputProcessorProfileMgr::ActivateProfile\n"
      "                       (known to activate WITHOUT dispatching OnActivated)\n"
      "  -HoldMs <n>          dwell between transitions (default 1500)\n"
      "  -AllowRemoteSession  permit a non-console session\n"
      "  -Help\n");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  SetConsoleOutputCP(CP_UTF8);

  const bool dry_run = !HasSwitch(argc, argv, L"-ActivateAndCycle");
  const bool allow_switch = HasSwitch(argc, argv, L"-AllowProfileSwitch");
  const bool allow_remote = HasSwitch(argc, argv, L"-AllowRemoteSession");
  const bool use_legacy_mgr = HasSwitch(argc, argv, L"-UseLegacyMgr");
  if (HasSwitch(argc, argv, L"-Help") || HasSwitch(argc, argv, L"--help") ||
      HasSwitch(argc, argv, L"/?")) {
    Usage();
    return 0;
  }
  DWORD hold_ms = 1500;
  for (int i = 1; i < argc - 1; ++i) {
    if (_wcsicmp(argv[i], L"-HoldMs") == 0) {
      const long v = _wtol(argv[i + 1]);
      if (v >= 200 && v <= 60000) {
        hold_ms = static_cast<DWORD>(v);
      }
    }
  }

  std::printf("PROBE=TestTsFocusOutHost\n");
  std::printf("PID=%lu\n", GetCurrentProcessId());
  std::printf("BITNESS=%s\n", (sizeof(void*) == 8) ? "x64" : "x86");
  std::printf("MODE=%s\n", dry_run ? "DryRun" : "ActivateAndCycle");
  std::printf("ACTIVATE_PATH=%s\n", use_legacy_mgr ? "legacy-mgr" : "profiles-AL");
  std::printf("HOLD_MS=%lu\n", hold_ms);

  // ---- guard 0: desktop (both modes) -------------------------------------
  std::string desk;
  if (!ThreadDesktopName(&desk)) {
    std::printf("DESKTOP_GUARD=UNKNOWN err=%lu\n", GetLastError());
    std::printf("PROBE_VERDICT=FAIL EXIT=3\n");
    return 3;
  }
  std::printf("DESKTOP=%s\n", desk.c_str());
  if (desk != "Default") {
    std::printf("DESKTOP_GUARD=FAIL (need Default; a virtual desktop invalidates evidence)\n");
    std::printf("PROBE_VERDICT=FAIL EXIT=3\n");
    return 3;
  }
  std::printf("DESKTOP_GUARD=PASS\n");

  // ---- guard 1: remote session -------------------------------------------
  const BOOL remote = GetSystemMetrics(SM_REMOTESESSION);
  std::printf("REMOTE_SESSION=%d\n", remote ? 1 : 0);
  if (remote && !allow_remote) {
    std::printf("REMOTE_GUARD=FAIL\n");
    std::printf("PROBE_VERDICT=FAIL EXIT=4\n");
    return 4;
  }
  std::printf("REMOTE_GUARD=PASS\n");

  // ---- guard 2: mutation authorization -----------------------------------
  if (!dry_run) {
    const bool env_ok = [] {
      WCHAR buf[8] = {0};
      const DWORD n = GetEnvironmentVariableW(L"FLUXING_VERIFY_ALLOW_PROFILE_SWITCH",
                                              buf, ARRAYSIZE(buf));
      return n > 0 && n < ARRAYSIZE(buf) && wcscmp(buf, L"1") == 0;
    }();
    std::printf("SWITCH_FLAG=%d\n", allow_switch ? 1 : 0);
    std::printf("SWITCH_ENV=%d\n", env_ok ? 1 : 0);
    if (!allow_switch || !env_ok) {
      std::printf("SWITCH_GUARD=FAIL (need -AllowProfileSwitch and env=1: this mode steals the user's active IME)\n");
      std::printf("PROBE_VERDICT=FAIL EXIT=2\n");
      return 2;
    }
    std::printf("SWITCH_GUARD=PASS\n");
  } else {
    std::printf("SWITCH_GUARD=N/A (read-only)\n");
  }

  // ---- COM ----------------------------------------------------------------
  const HRESULT hr_co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (FAILED(hr_co)) {
    std::printf("COINIT_HR=0x%08lx\n", static_cast<unsigned long>(hr_co));
    std::printf("PROBE_VERDICT=FAIL EXIT=5\n");
    return 5;
  }
  std::printf("COINIT_HR=0x%08lx\n", static_cast<unsigned long>(hr_co));

  int exit_code = 0;
  ITfThreadMgr* thread_mgr = nullptr;
  ITfInputProcessorProfiles* profiles = nullptr;
  ITfInputProcessorProfileMgr* profile_mgr = nullptr;
  NotifyProbe* probe = new NotifyProbe();
  DWORD sink_cookie = 0;
  bool sink_advised = false;
  ITfDocumentMgr* dim = nullptr;
  ITfDocumentMgr* prev_dim = nullptr;
  HWND hwnd = nullptr;
  TF_INPUTPROCESSORPROFILE before;
  ZeroMemory(&before, sizeof(before));
  bool have_before = false;

  do {
    // ---- thread manager (needed to receive TSF notifications on this thread)
    const HRESULT hr1 = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr,
                                         CLSCTX_INPROC_SERVER,
                                         __uuidof(ITfThreadMgr),
                                         reinterpret_cast<void**>(&thread_mgr));
    std::printf("THREADMGR_HR=0x%08lx\n", static_cast<unsigned long>(hr1));
    if (FAILED(hr1) || thread_mgr == nullptr) {
      NoteFailure("CoCreateInstance(CLSID_TF_ThreadMgr)");
      exit_code = 6;
      break;
    }
    // msctf.h 19041 L1069 / 26100 L1079: Activate(TfClientId *ptid).
    // NOTE: neither SDK copy declares the legacy Initialize() -- it is gone.
    TfClientId tid = 0;
    const HRESULT hr2 = thread_mgr->Activate(&tid);
    std::printf("TM_ACTIVATE_HR=0x%08lx\n", static_cast<unsigned long>(hr2));
    std::printf("TM_CLIENT_ID=%lu\n", static_cast<unsigned long>(tid));
    if (FAILED(hr2)) {
      NoteFailure("ITfThreadMgr::Activate");
      exit_code = 6;
      break;
    }
    std::printf("THREADMGR=OK\n");

    // ---- profiles / profile manager --------------------------------------
    const HRESULT hr3 = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                         CLSCTX_INPROC_SERVER,
                                         __uuidof(ITfInputProcessorProfiles),
                                         reinterpret_cast<void**>(&profiles));
    std::printf("PROFILES_HR=0x%08lx\n", static_cast<unsigned long>(hr3));
    if (FAILED(hr3) || profiles == nullptr) {
      NoteFailure("CoCreateInstance(CLSID_TF_InputProcessorProfiles)");
      exit_code = 1;
      break;
    }
    const HRESULT hr4 = profiles->QueryInterface(__uuidof(ITfInputProcessorProfileMgr),
                                                 reinterpret_cast<void**>(&profile_mgr));
    std::printf("PROFILEMGR_HR=0x%08lx\n", static_cast<unsigned long>(hr4));
    if (FAILED(hr4) || profile_mgr == nullptr) {
      NoteFailure("QI ITfInputProcessorProfileMgr");
      exit_code = 1;
      break;
    }

    // ---- advise the profile-notify sink on OUR thread manager ------------
    // msctf.h 19041 L11063 / 26100 L11665: AdviseSink(REFIID, IUnknown*, DWORD*)
    ITfSource* source = nullptr;
    const HRESULT hr5 = thread_mgr->QueryInterface(__uuidof(ITfSource),
                                                   reinterpret_cast<void**>(&source));
    std::printf("SOURCE_HR=0x%08lx\n", static_cast<unsigned long>(hr5));
    if (SUCCEEDED(hr5) && source != nullptr) {
      const HRESULT hr6 = source->AdviseSink(__uuidof(ITfActiveLanguageProfileNotifySink),
                                             static_cast<IUnknown*>(probe), &sink_cookie);
      std::printf("SINK_ADVISE_HR=0x%08lx\n", static_cast<unsigned long>(hr6));
      std::printf("SINK_COOKIE=%lu\n", static_cast<unsigned long>(sink_cookie));
      sink_advised = SUCCEEDED(hr6);
      if (!sink_advised) {
        // Without the sink we cannot see OnActivated on this thread. The product
        // path (WeaselTSF) registers its own sink inside the *host that loads the
        // shim*, so this is a probe-side capability, not a product judgement.
        std::printf("SINK_ADVISE=DEGRADED\n");
      }
      source->Release();
    } else {
      std::printf("SINK_ADVISE=DEGRADED (no ITfSource)\n");
    }

    // ---- read-only observations ------------------------------------------
    ReportFluxingRegistration(profiles);
    ReportFluxingActiveState(profiles);
    ReportSystemActiveKeyboardProfile(profile_mgr);

    // snapshot what to restore to
    {
      TF_INPUTPROCESSORPROFILE snap;
      ZeroMemory(&snap, sizeof(snap));
      if (SUCCEEDED(profile_mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &snap))) {
        before = snap;
        have_before = true;
      }
    }

    if (dry_run) {
      break;
    }

    // ---- mutating sequence -----------------------------------------------
    hwnd = CreateHostWindow();
    if (hwnd == nullptr) {
      NoteFailure("host window creation");
      exit_code = 1;
      break;
    }
    std::printf("HOST_HWND=0x%llx\n", static_cast<unsigned long long>(
                                             reinterpret_cast<unsigned long long>(hwnd)));

    const HRESULT hr7 = thread_mgr->CreateDocumentMgr(&dim);
    std::printf("CREATE_DOCCMGR_HR=0x%08lx\n", static_cast<unsigned long>(hr7));
    if (FAILED(hr7) || dim == nullptr) {
      NoteFailure("ITfThreadMgr::CreateDocumentMgr");
      exit_code = 1;
      break;
    }
    // AssociateFocus(hwnd, pdimNew, ppdimPrev) -- 19041 L1086 / 26100 L1096
    const HRESULT hr8 = thread_mgr->AssociateFocus(hwnd, dim, &prev_dim);
    std::printf("ASSOCIATE_FOCUS_HR=0x%08lx\n", static_cast<unsigned long>(hr8));
    const HRESULT hr9 = thread_mgr->SetFocus(dim);
    std::printf("TM_SETFOCUS_IN_HR=0x%08lx\n", static_cast<unsigned long>(hr9));
    SetForegroundWindow(hwnd);
    if (g_edit != nullptr) {
      SetFocus(g_edit);
    }
    PumpMessages(400);

    const int ev_before = probe->on() + probe->off();
    // Activate the Fluxing profile.
    //   Path A (default): ITfInputProcessorProfiles::ActivateLanguageProfile --
    //     19041 L7756 / 26100 L8164. Per test\TestWeaselIPC\TsfProfileVerify.cpp
    //     header findings, this is the path that DISPATCHES OnActivated.
    //   Path C (-UseLegacyMgr): ITfInputProcessorProfileMgr::ActivateProfile --
    //     19041 L8698 / 26100 L9169. Known to activate WITHOUT dispatching.
    HRESULT hr_act = E_FAIL;
    if (use_legacy_mgr) {
      hr_act = profile_mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                            kFluxingLangid, kClsidFluxingTip,
                                            kGuidFluxingProfile, nullptr, 0);
    } else {
      hr_act = profiles->ActivateLanguageProfile(kClsidFluxingTip, kFluxingLangid,
                                                 kGuidFluxingProfile);
    }
    std::printf("ACTIVATE_FLUXING_HR=0x%08lx\n", static_cast<unsigned long>(hr_act));
    std::fflush(stdout);
    PumpMessages(hold_ms);
    ReportFluxingActiveState(profiles);

    // Now force the focus-away that should make the loaded shim emit FocusOut.
    // Two independent levers, both applied so the host really loses TSF focus.
    const HRESULT hr_lost = thread_mgr->SetFocus(nullptr);
    std::printf("TM_SETFOCUS_OUT_HR=0x%08lx\n", static_cast<unsigned long>(hr_lost));
    if (prev_dim != nullptr) {
      thread_mgr->SetFocus(prev_dim);
    }
    PumpMessages(hold_ms);
    ReportFluxingActiveState(profiles);

    // Restore the user's previously active profile -- mandatory, and only safe
    // if we actually captured it.
    if (have_before && !IsEqualGUID(before.clsid, kClsidFluxingTip)) {
      const HRESULT hr_rs = profiles->ActivateLanguageProfile(before.clsid, before.langid,
                                                              before.guidProfile);
      std::printf("RESTORE_HR=0x%08lx\n", static_cast<unsigned long>(hr_rs));
      std::fflush(stdout);
      PumpMessages(hold_ms);
      ReportFluxingActiveState(profiles);
      ReportSystemActiveKeyboardProfile(profile_mgr);
    } else {
      std::printf("RESTORE_SKIPPED=%d\n", have_before ? 1 : 0);
      if (!have_before) {
        NoteFailure("cannot restore: active profile snapshot failed");
      }
    }

    const int ev_after = probe->on() + probe->off();
    std::printf("TSF_EVENTS_TOTAL=%d\n", probe->on() + probe->off());
    std::printf("TSF_EVENTS_FLUXING_ON=%d\n", probe->fluxing_on());
    std::printf("TSF_EVENTS_FLUXING_OFF=%d\n", probe->fluxing_off());
    std::printf("TSF_EVENTS_DELTA=%d\n", ev_after - ev_before);
  } while (false);

  // ---- teardown -----------------------------------------------------------
  if (sink_advised && thread_mgr != nullptr) {
    ITfSource* source = nullptr;
    if (SUCCEEDED(thread_mgr->QueryInterface(__uuidof(ITfSource),
                                             reinterpret_cast<void**>(&source))) &&
        source != nullptr) {
      source->UnadviseSink(sink_cookie);
      source->Release();
    }
  }
  if (hwnd != nullptr) {
    DestroyWindow(hwnd);
    PumpMessages(100);
  }
  if (prev_dim != nullptr) {
    prev_dim->Release();
  }
  if (dim != nullptr) {
    dim->Release();
  }
  if (thread_mgr != nullptr) {
    thread_mgr->Deactivate();
    thread_mgr->Release();
  }
  if (profile_mgr != nullptr) {
    profile_mgr->Release();
  }
  if (profiles != nullptr) {
    profiles->Release();
  }
  if (probe != nullptr) {
    probe->Release();
  }
  CoUninitialize();

  std::printf("FAILURE_COUNT=%d\n", g_failures);
  if (exit_code == 0 && g_failures != 0) {
    exit_code = 1;
  }
  std::printf("PROBE_VERDICT=%s EXIT=%d\n", exit_code == 0 ? "PASS" : "FAIL", exit_code);
  std::fflush(stdout);
  return exit_code;
}
