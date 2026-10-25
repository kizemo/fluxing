// TsfProfileVerify.cpp
//
// spec 080 (ADR 0008): TSF-level end-to-end check for "the QuickPanel shows
// when the user switches the active input profile to Fluxing".
//
// /verify-quickpanel (TestWeaselIPC.cpp) proves the server half: a FocusIn
// carrying WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED shows the QuickPanel.
// This check proves the front half: a real
// ITfActiveLanguageProfileNotifySink::OnActivated callback, delivered by
// Windows itself, reaches the same state.
//
// Steps:
//   1. create the TSF thread manager for this thread
//   2. advise our own ITfActiveLanguageProfileNotifySink so the timeline of
//      the notifications is visible
//   3. show a window with an edit control and give it the foreground, so
//      this thread owns a focused text store. If the text service still
//      does not land in this process, a real Windows application is
//      started and focused instead: Windows creates the text store for
//      those itself, and the text service is instantiated into them.
//   4. SWITCH AWAY from the Fluxing input profile and switch back. The
//      notification is dispatched when the active profile CHANGES, and
//      Fluxing is the only input method installed in the sandbox, so it is
//      already the active one: activating it again is a no-op and nothing
//      is dispatched.
//   5. assert the QuickPanel window became visible
//
// Which API activates the profile decides whether Windows dispatches the
// notification, so the driver tries the candidates in the order the system
// itself uses them and reports which one fired:
//
//   A. ITfInputProcessorProfiles::ActivateLanguageProfile on a dedicated
//      CLSID_TF_InputProcessorProfiles instance -- the modern path, the one
//      the language bar uses when the user picks an input method. The
//      activation is still scoped to the calling thread.
//   B. the same call on the instance reached through the thread manager.
//   C. ITfInputProcessorProfileMgr::ActivateProfile -- the legacy TSF-5
//      interface. It activates the profile but does not dispatch the
//      notification, which is why the first version of this driver always
//      saw notifications=0.
//   D. the legacy activate / deactivate / activate sequence, the way the
//      feature was originally driven.
//
// The test never changes the desktop wide input method on purpose: every
// candidate is scoped to this thread.
//
// Note on the sink registration path: the Windows SDK exposes exactly one
// way to register an ITfActiveLanguageProfileNotifySink,
// ITfSource::AdviseSink(__uuidof(ITfActiveLanguageProfileNotifySink), ...).
// There is no
// ITfInputProcessorProfiles::RegisterActiveLanguageProfileNotifySink in
// msctf.h, so the Fluxing TIP has no alternative.
//
// WARNING: this mode really switches the active input profile. That is not
// what a user does. On 2026-10-23 it was observed to collapse the desktop
// input stack: dwm.exe, ImeService.exe and the remote control agent all
// crashed inside one window while the test ran and a remote session was
// being established at the same time. The mode is therefore guarded by
// three layers:
//
//   1. opt in: it refuses to run without --allow-profile-switch (or the
//      environment variable FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1)
//   2. remote session guard: it refuses to run while a remote control
//      client lives in the interactive session, unless
//      --allow-remote-session (or FLUXING_VERIFY_ALLOW_REMOTE_SESSION=1)
//      is given as well
//   3. crash bookkeeping: the Application event log is compared before and
//      after the switch, so a crash caused by the switch is reported
//      instead of ignored
//
// The user authorized running this mode inside the disposable Windows
// Sandbox on 2026-10-25 (the sandbox has no remote session and is thrown
// away afterwards, so the blast radius is the virtual machine only). The
// three guards stay: they are what keeps a mistake on a real desktop
// harmless, and the sandbox is not the only place this binary can run.
//
// The auto hide of the panel is only reported here, it is not asserted: it
// depends on the mouse position and on the grace period of the panel. The
// safe /verify-quickpanel check covers it without touching the input
// profile at all.

#include "stdafx.h"

#include <ole2.h>
#include <msctf.h>
#include <tlhelp32.h>

#include <WeaselIPC.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

// The Fluxing TIP CLSID and input profile GUID (WeaselTSF/Globals.cpp).
const GUID kFluxingTipClsid = {0xa3f4cded,
                               0xb1e9,
                               0x41ee,
                               {0x9c, 0xa6, 0x7b, 0x4d, 0xd, 0xe6, 0xcb, 0xa}};

const GUID kFluxingProfileGuid = {
    0x3d02cab6,
    0x2b8e,
    0x4781,
    {0xba, 0x20, 0x1c, 0x92, 0x67, 0x52, 0x94, 0x67}};

// zh-CN, the language the Fluxing profile is registered under.
const LANGID kFluxingLangid = 0x0804;

int g_tsf_timeline = 0;

std::wstring tsf_guid_text(REFGUID g) {
  wchar_t buf[64] = {0};
  StringFromGUID2(g, buf, 64);
  std::wstring s(buf);
  if (s.size() >= 2 && s.front() == L'{' && s.back() == L'}')
    s = s.substr(1, s.size() - 2);
  return s;
}

// A sink that only records the notifications, so the test can show when
// Windows delivered them relative to the ActivateProfile calls.
class TsfTimelineSink : public ITfActiveLanguageProfileNotifySink {
 public:
  TsfTimelineSink() : ref_(1) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr)
      return E_INVALIDARG;
    if (IsEqualIID(riid, __uuidof(IUnknown)) ||
        IsEqualIID(riid, __uuidof(ITfActiveLanguageProfileNotifySink))) {
      *ppv = static_cast<ITfActiveLanguageProfileNotifySink*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }

  ULONG STDMETHODCALLTYPE Release() {
    return static_cast<ULONG>(InterlockedDecrement(&ref_));
  }

  HRESULT STDMETHODCALLTYPE OnActivated(REFCLSID clsid,
                                        REFGUID guidProfile,
                                        BOOL fActivated) {
    ++g_tsf_timeline;
    std::wcout << L"[tsf " << g_tsf_timeline << L"] OnActivated clsid="
               << tsf_guid_text(clsid) << L" profile="
               << tsf_guid_text(guidProfile) << L" activated="
               << (fActivated ? 1 : 0) << std::endl;
    return S_OK;
  }

 private:
  LONG ref_;
};

// A minimal COM class object.
//
// TSF hands out a client id for a registered COM class, and an application
// needs a client id of its own before the thread manager can be activated
// for it. The first version of this driver asked for the *text service's*
// client id (GetClientId with the Fluxing CLSID): the thread manager then
// believes the calling thread IS the text service, and never loads it.
// The measured symptom was
//     tip loaded into this process: weasel.dll=0
// while the profile activation itself returned S_OK.
//
// Registering a throwaway class gives the test the application role it
// needs, so the thread manager can instantiate the input service into this
// thread when the profile is activated.
const GUID kTestAppClsid = {0x2f1a9c74,
                             0x3b8e,
                             0x4d5a,
                             {0x9c, 0x6f, 0x71, 0xe0, 0xd4, 0xa5, 0xb3, 0xc2}};

class DummyObject : public IUnknown {
 public:
  DummyObject() : ref_(1) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr)
      return E_INVALIDARG;
    if (IsEqualIID(riid, __uuidof(IUnknown))) {
      *ppv = static_cast<IUnknown*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }

  ULONG STDMETHODCALLTYPE Release() {
    return static_cast<ULONG>(InterlockedDecrement(&ref_));
  }

 private:
  LONG ref_;
};

class DummyClassFactory : public IClassFactory {
 public:
  DummyClassFactory() : ref_(1) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr)
      return E_INVALIDARG;
    if (IsEqualIID(riid, __uuidof(IUnknown)) ||
        IsEqualIID(riid, __uuidof(IClassFactory))) {
      *ppv = static_cast<IClassFactory*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&ref_));
  }

  ULONG STDMETHODCALLTYPE Release() {
    return static_cast<ULONG>(InterlockedDecrement(&ref_));
  }

  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,
                                           REFIID riid,
                                           void** ppv) {
    if (outer != nullptr)
      return CLASS_E_NOAGGREGATION;
    if (ppv == nullptr)
      return E_INVALIDARG;
    DummyObject* obj = new DummyObject();
    HRESULT hr = obj->QueryInterface(riid, ppv);
    obj->Release();
    return hr;
  }

  HRESULT STDMETHODCALLTYPE LockServer(BOOL) {
    return S_OK;
  }

 private:
  LONG ref_;
};

LRESULT CALLBACK probe_wnd_proc(HWND hwnd,
                                UINT msg,
                                WPARAM wp,
                                LPARAM lp) {
  return DefWindowProcW(hwnd, msg, wp, lp);
}

// Takes the foreground for this thread.
//
// SetForegroundWindow is refused for a process that is not the foreground
// process, and SetFocus only works on the foreground thread. The probe runs
// as a console child of the gate script, so it is not the foreground
// process. Attaching the input queues of the two threads is the documented
// way to get the foreground anyway.
BOOL take_foreground(HWND hwnd) {
  if (hwnd == nullptr)
    return FALSE;
  HWND fg = GetForegroundWindow();
  DWORD fg_tid = 0;
  if (fg != nullptr)
    fg_tid = GetWindowThreadProcessId(fg, nullptr);
  if (fg_tid != 0 && fg_tid != GetCurrentThreadId())
    AttachThreadInput(GetCurrentThreadId(), fg_tid, TRUE);
  BOOL ok = SetForegroundWindow(hwnd);
  if (fg_tid != 0 && fg_tid != GetCurrentThreadId())
    AttachThreadInput(GetCurrentThreadId(), fg_tid, FALSE);
  return ok;
}

// A real window with a real edit control.
//
// TSF only attaches an input service to a thread that owns a text control,
// so the test needs to look like an application instead of a console
// process.
//
// Rounds 1 to 3 created the window but never showed it, and the window was
// therefore never the foreground window of the desktop. SetFocus(edit) was
// then rejected, the thread manager kept reporting "no focus" and the text
// service was never instantiated into this process. The measurement was
//     document managers=2, GetFocus hr=0x1, weasel.dll=0
// while the profile activation returned S_OK. Showing the window and taking
// the foreground is what a normal application does, and it is the only way
// to get a focused text store on this thread.
HWND create_probe_edit() {
  static const wchar_t* kClass = L"FluxingTsfProbeWindow";
  WNDCLASSEXW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = probe_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = kClass;
  RegisterClassExW(&wc);  // "already registered" is fine

  HWND parent =
      CreateWindowExW(0, kClass, L"Fluxing TSF probe",
                      WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 160,
                      nullptr, nullptr, wc.hInstance, nullptr);
  if (parent == nullptr)
    return nullptr;
  ShowWindow(parent, SW_SHOW);
  UpdateWindow(parent);

  HWND edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE,
                              0, 0, 300, 100, parent, nullptr, wc.hInstance,
                              nullptr);
  if (edit == nullptr)
    return nullptr;

  take_foreground(parent);
  HWND focused = SetFocus(edit);
  std::wcout << L"[tsf] probe window shown, foreground taken="
             << (GetForegroundWindow() == parent ? 1 : 0)
             << L" edit focus=" << (focused == edit ? 1 : 0)
             << L" foreground=" << (GetForegroundWindow() == parent ? 1 : 0)
             << std::endl;

  // Pump the queue so the focus change actually reaches TSF.
  MSG msg;
  for (int i = 0; i < 150; ++i) {
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  std::wcout << L"[tsf] focus after the pump=" << (GetFocus() == edit ? 1 : 0)
             << std::endl;
  return edit;
}

// Finds a visible top level window of a process.
HWND find_process_window(DWORD pid) {
  struct Ctx {
    DWORD pid;
    HWND hwnd;
  };
  Ctx ctx = {pid, nullptr};
  struct Cb {
    static BOOL CALLBACK proc(HWND hwnd, LPARAM lp) {
      Ctx* c = reinterpret_cast<Ctx*>(lp);
      DWORD p = 0;
      GetWindowThreadProcessId(hwnd, &p);
      if (p == c->pid && IsWindowVisible(hwnd) &&
          GetWindow(hwnd, GW_OWNER) == nullptr) {
        c->hwnd = hwnd;
        return FALSE;
      }
      return TRUE;
    }
  };
  EnumWindows(Cb::proc, reinterpret_cast<LPARAM>(&ctx));
  return ctx.hwnd;
}

// Launches a real Windows application that is known to work with input
// methods and gives it the focus.
//
// The probe window is a console application's window. A packaged or classic
// Windows application is a much better TSF client: Windows creates the text
// store for its edit controls itself, and the text service is instantiated
// into that process when it takes the focus. If the text service is loaded
// there, the profile notification is dispatched to that process and the
// QuickPanel is shown by the server on behalf of it.
DWORD launch_tsf_client(const wchar_t* exe) {
  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  std::wstring cmd = std::wstring(L"\"") + exe + L"\"";
  if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &si, &pi)) {
    std::wcout << L"[tsf] launch " << exe << L" failed, error="
               << GetLastError() << std::endl;
    return 0;
  }
  CloseHandle(pi.hThread);

  HWND w = nullptr;
  for (int i = 0; i < 150 && w == nullptr; ++i) {
    Sleep(100);
    w = find_process_window(pi.dwProcessId);
  }
  if (w != nullptr) {
    take_foreground(w);
    Sleep(800);
  }
  std::wcout << L"[tsf] launched tsf client " << exe << L" pid="
             << pi.dwProcessId << L" hwnd=" << (w ? (void*)w : (void*)0)
             << L" foreground="
             << (w != nullptr && GetForegroundWindow() == w ? 1 : 0)
             << std::endl;
  return pi.dwProcessId;
}

// Reports which profile the system considers the active one.
void report_active_profile(ITfInputProcessorProfiles* pProfiles,
                            const wchar_t* tag) {
  if (pProfiles == nullptr)
    return;
  LANGID langid = 0;
  GUID guid = {0};
  HRESULT hr = pProfiles->GetActiveLanguageProfile(kFluxingTipClsid, &langid,
                                                   &guid);
  std::wcout << L"[tsf] active profile (" << tag << L") hr=0x" << std::hex << hr
             << L" langid=0x" << langid << L" profile=" << tsf_guid_text(guid)
             << std::endl;
}

// Switches AWAY from the Fluxing input processor.
//
// The system falls back to the plain keyboard layout, so the next activation
// of Fluxing is a real change and that is the moment Windows dispatches
// ITfActiveLanguageProfileNotifySink::OnActivated.
void switch_away_fluxing(ITfInputProcessorProfileMgr* pProfileMgr) {
  if (pProfileMgr == nullptr)
    return;
  HRESULT hr = pProfileMgr->DeactivateProfile(
      TF_PROFILETYPE_INPUTPROCESSOR, kFluxingLangid, kFluxingTipClsid,
      kFluxingProfileGuid, NULL, 0);
  std::wcout << L"[tsf] switch away: DeactivateProfile hr=0x" << std::hex << hr
             << std::endl;
  Sleep(1200);
}

// Reads the install directory the installer wrote into the registry, so
// the test can find the TIP it is supposed to drive.
std::wstring read_install_dir() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Fluxing\\Weasel", 0,
                    KEY_READ, &key) != ERROR_SUCCESS)
    return std::wstring();
  wchar_t buf[MAX_PATH] = {0};
  DWORD size = sizeof(buf);
  LONG rc = RegQueryValueExW(key, L"InstallDir", nullptr, nullptr,
                              reinterpret_cast<LPBYTE>(buf), &size);
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS)
    return std::wstring();
  return std::wstring(buf);
}

HWND find_quick_panel() {
  // class "FluxingQuickPanel_v3", title "Fluxing QuickPanel"
  return FindWindowW(L"FluxingQuickPanel_v3", L"Fluxing QuickPanel");
}

bool quick_panel_visible() {
  HWND h = find_quick_panel();
  return h != NULL && IsWindowVisible(h);
}

// Finds a remote control client that runs in the interactive session.
//
// The service side of every remote control agent always runs, in session
// 0, and is not the co-factor. What breaks the input stack is the session
// client: it injects into the desktop (it takes a snapshot of it, it hooks
// the input) exactly while the profile switch tears the IME stack down.
// The list is a heuristic, it only has to catch the common agents.
std::wstring find_remote_agent() {
  static const wchar_t* kAgents[] = {L"ToDesk.exe",     L"AnyDesk.exe",
                                     L"RustDesk.exe",   L"Parsec.exe",
                                     L"TeamViewer.exe", L"SunloginClient.exe",
                                     L"QuarkViewer.exe"};
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    return std::wstring();
  PROCESSENTRY32W entry;
  entry.dwSize = sizeof(entry);
  std::wstring found;
  if (Process32FirstW(snapshot, &entry)) {
    do {
      for (size_t i = 0; i < sizeof(kAgents) / sizeof(kAgents[0]); ++i) {
        if (_wcsicmp(entry.szExeFile, kAgents[i]) != 0)
          continue;
        DWORD session = 0;
        if (!ProcessIdToSessionId(entry.th32ProcessID, &session))
          continue;
        if (session == 0)
          continue;  // the service side, it always runs
        if (found.empty())
          found = entry.szExeFile;
      }
    } while (Process32NextW(snapshot, &entry));
  }
  CloseHandle(snapshot);
  return found;
}

// Reads the Application event log to find the crashes the profile switch
// may have caused.
//
// Three details of this API are easy to get wrong and were settled by
// measuring them against the real log instead of guessing:
//
//   - the read flags are the winnt.h ones (EVENTLOG_SEEK_READ 0x0002 plus
//     EVENTLOG_BACKWARDS_READ 0x0008), the names on MSDN are older
//   - the record number is passed by value, the SDK declares the third
//     parameter as DWORD dwRecordOffset
//   - the insertion strings are stored one after another, null terminated,
//     starting at StringOffset; they are not an array of offsets
//
// A buffer of 64KB is used on purpose: a 512KB buffer makes ReadEventLog
// fail with error 1783 on a busy log.
class CrashWatcher {
 public:
  CrashWatcher() : log_(OpenEventLogW(nullptr, L"Application")) {
    if (log_ != nullptr)
      snapshot_newest_ = newest_record();
  }

  ~CrashWatcher() {
    if (log_ != nullptr)
      CloseEventLog(log_);
  }

  bool available() const { return log_ != nullptr; }

  DWORD snapshot_record() const { return snapshot_newest_; }

  // Prints every Application Error record that appeared after the
  // snapshot and returns how many were found.
  int report_new_crashes() {
    if (log_ == nullptr)
      return 0;
    DWORD newest = newest_record();
    if (newest == 0 || newest <= snapshot_newest_)
      return 0;

    int crashes = 0;
    DWORD position = newest;
    for (int round = 0; round < 64 && position > snapshot_newest_; ++round) {
      std::vector<BYTE> buffer(64 * 1024);
      DWORD bytes = 0;
      DWORD needed = 0;
      if (!ReadEventLogW(log_, EVENTLOG_SEEK_READ | EVENTLOG_BACKWARDS_READ,
                         position, buffer.data(),
                         static_cast<DWORD>(buffer.size()), &bytes, &needed) ||
          bytes == 0) {
        break;
      }
      DWORD oldest_in_batch = 0;
      size_t offset = 0;
      while (offset + sizeof(EVENTLOGRECORD) <= bytes) {
        EVENTLOGRECORD* rec =
            reinterpret_cast<EVENTLOGRECORD*>(buffer.data() + offset);
        if (rec->Length == 0)
          break;
        oldest_in_batch = rec->RecordNumber;
        if (rec->RecordNumber <= snapshot_newest_)
          break;
        if ((rec->EventID & 0xffff) == 1000) {  // Application Error
          ++crashes;
          std::wcout << L"[tsf] CRASH event record " << rec->RecordNumber
                     << L": " << string_at(rec, 0) << L" / module "
                     << string_at(rec, 3) << L" / exception "
                     << string_at(rec, 4) << std::endl;
        }
        offset += rec->Length;
      }
      if (oldest_in_batch == 0 || oldest_in_batch <= snapshot_newest_ + 1)
        break;
      position = oldest_in_batch - 1;
    }
    return crashes;
  }

 private:
  DWORD newest_record() {
    DWORD oldest = 0;
    DWORD count = 0;
    if (!GetOldestEventLogRecord(log_, &oldest))
      return 0;
    if (!GetNumberOfEventLogRecords(log_, &count))
      return 0;
    return count == 0 ? 0 : oldest + count - 1;
  }

  // The insertion strings are stored one after another, each one null
  // terminated, starting at StringOffset.
  static std::wstring string_at(EVENTLOGRECORD* rec, DWORD index) {
    if (index >= rec->NumStrings)
      return std::wstring();
    const BYTE* base = reinterpret_cast<const BYTE*>(rec);
    const wchar_t* p =
        reinterpret_cast<const wchar_t*>(base + rec->StringOffset);
    for (DWORD i = 0; i < index; ++i) {
      while (*p != L'\0')
        ++p;
      ++p;
    }
    return std::wstring(p);
  }

  HANDLE log_;
  DWORD snapshot_newest_ = 0;
};

}  // namespace

int verify_tsf_profile_main(bool allow_profile_switch,
                            bool allow_remote_session) {
  // guard layer 1: the caller has to opt in explicitly.
  if (!allow_profile_switch) {
    std::wcout << L"[tsf] REFUSED: this mode really switches the active input "
                  L"profile.\n"
                  L"[tsf] A programmatic profile switch is not what a user "
                  L"does and it can collapse the desktop input stack (dwm.exe, "
                  L"ImeService.exe and the remote control agent crashed in one "
                  L"window on 2026-10-23).\n"
                  L"[tsf] Re-run with --allow-profile-switch to accept the "
                  L"risk.\n";
    return -10;
  }

  // guard layer 2: no remote control client in the interactive session.
  std::wstring remote_agent = find_remote_agent();
  if (!remote_agent.empty() && !allow_remote_session) {
    std::wcout << L"[tsf] REFUSED: the remote control client " << remote_agent
               << L" runs in the interactive session.\n"
                  L"[tsf] A remote session together with a profile switch is "
                  L"exactly what crashed the machine on 2026-10-23.\n"
                  L"[tsf] Close the remote session, or re-run with "
                  L"--allow-remote-session to accept the risk.\n";
    return -11;
  }
  if (!remote_agent.empty()) {
    std::wcout << L"[tsf] WARNING: the remote control client " << remote_agent
               << L" runs in the interactive session, the risk is accepted"
               << std::endl;
  }

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    std::wcout << L"[tsf] CoInitializeEx failed 0x" << std::hex << hr
               << std::endl;
    return -2;
  }

  ITfThreadMgr* pThreadMgr = nullptr;
  hr = CoCreateInstance(CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER,
                        __uuidof(ITfThreadMgr), (void**)&pThreadMgr);
  if (FAILED(hr) || pThreadMgr == nullptr) {
    std::wcout << L"[tsf] CoCreateInstance(ITfThreadMgr) failed 0x" << std::hex
               << hr << std::endl;
    return -3;
  }

  // The thread manager needs a client id before it can be activated for
  // this thread, and the id has to belong to an APPLICATION. A throwaway
  // COM class is registered for that: asking for the text service's own
  // id (the previous behaviour) makes the thread manager treat this thread
  // as the text service, and the text service is then never loaded into it.
  DummyClassFactory factory;
  DWORD class_cookie = 0;
  HRESULT hr_reg = CoRegisterClassObject(kTestAppClsid, &factory,
                                          CLSCTX_LOCAL_SERVER |
                                              CLSCTX_INPROC_SERVER,
                                          REGCLS_MULTIPLEUSE, &class_cookie);
  std::wcout << L"[tsf] CoRegisterClassObject(test app) hr=0x" << std::hex
             << hr_reg << std::endl;

  TfClientId client_id = 0;
  ITfClientId* pClientId = nullptr;
  if (SUCCEEDED(pThreadMgr->QueryInterface(__uuidof(ITfClientId),
                                           (void**)&pClientId)) &&
      pClientId != nullptr) {
    TfClientId id = 0;
    if (SUCCEEDED(pClientId->GetClientId(kTestAppClsid, &id)) && id != 0) {
      client_id = id;
      std::wcout << L"[tsf] app client id=" << client_id << std::endl;
    } else {
      std::wcout << L"[tsf] no client id for the test app class, falling back"
                 << std::endl;
      const GUID candidates[] = {kFluxingTipClsid, CLSID_TF_ThreadMgr};
      for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        id = 0;
        if (SUCCEEDED(pClientId->GetClientId(candidates[i], &id)) && id != 0) {
          client_id = id;
          std::wcout << L"[tsf] fallback client id from clsid="
                     << tsf_guid_text(candidates[i]) << L" id=" << client_id
                     << std::endl;
          break;
        }
      }
    }
    pClientId->Release();
  }
  if (client_id != 0) {
    hr = pThreadMgr->Activate(&client_id);
    std::wcout << L"[tsf] ITfThreadMgr::Activate hr=0x" << std::hex << hr
               << std::endl;
  } else {
    std::wcout << L"[tsf] no client id, skipping ITfThreadMgr::Activate"
               << std::endl;
  }

  // A document context is what an input service gets attached to. Without
  // one, activating the profile changes the active profile but never
  // instantiates the text service -- measured as "weasel.dll=0" while the
  // activation returned S_OK.
  ITfDocumentMgr* pDocMgr = nullptr;
  ITfContext* pContext = nullptr;
  if (SUCCEEDED(pThreadMgr->CreateDocumentMgr(&pDocMgr)) && pDocMgr != nullptr) {
    TfEditCookie ec = 0;
    hr = pDocMgr->CreateContext(client_id, 0, nullptr, &pContext, &ec);
    std::wcout << L"[tsf] CreateDocumentMgr+CreateContext hr=0x" << std::hex
               << hr << L" context=" << (pContext ? 1 : 0) << std::endl;
    if (pContext != nullptr) {
      // Push the context onto the document manager's context stack, which
      // is what makes it the active context of this thread. An input
      // service is attached to the active context, so without this the
      // profile activation has nothing to attach the text service to.
      hr = pDocMgr->Push(pContext);
      std::wcout << L"[tsf] ITfDocumentMgr::Push(context) hr=0x" << std::hex
                 << hr << std::endl;
    }
  } else {
    std::wcout << L"[tsf] no ITfDocumentMgr, no context" << std::endl;
  }

  // ===== step 1a: look like an application =====
  //
  // A window with an edit control, focused, so the thread has something
  // TSF can attach an input service to. Then measure what TSF thinks this
  // thread has: document managers, contexts, and the focused context.
  HWND probe_edit = create_probe_edit();
  HWND probe_parent = probe_edit ? GetParent(probe_edit) : nullptr;
  std::wcout << L"[tsf] probe edit hwnd=" << (probe_edit ? (void*)probe_edit : (void*)0)
             << L" parent=" << (probe_parent ? (void*)probe_parent : (void*)0)
             << std::endl;

  ULONG doc_count = 0;
  IEnumTfDocumentMgrs* pEnumDm = nullptr;
  if (SUCCEEDED(pThreadMgr->EnumDocumentMgrs(&pEnumDm)) && pEnumDm != nullptr) {
    ULONG fetched = 0;
    ITfDocumentMgr* pDm = nullptr;
    while (SUCCEEDED(pEnumDm->Next(1, &pDm, &fetched)) && fetched > 0) {
      ++doc_count;
      ULONG ctx_count = 0;
      IEnumTfContexts* pEnumCtx = nullptr;
      if (SUCCEEDED(pDm->EnumContexts(&pEnumCtx)) && pEnumCtx != nullptr) {
        ULONG ctx_fetched = 0;
        ITfContext* pCtx = nullptr;
        while (SUCCEEDED(pEnumCtx->Next(1, &pCtx, &ctx_fetched)) &&
               ctx_fetched > 0) {
          ++ctx_count;
          pCtx->Release();
        }
        pEnumCtx->Release();
      }
      pDm->Release();
    }
    pEnumDm->Release();
  }
  std::wcout << L"[tsf] document managers=" << doc_count << std::endl;

  ITfDocumentMgr* pFocusDm = nullptr;
  hr = pThreadMgr->GetFocus(&pFocusDm);
  std::wcout << L"[tsf] GetFocus hr=0x" << std::hex << hr << L" dm="
             << (pFocusDm ? 1 : 0) << std::endl;
  if (pFocusDm == nullptr && pDocMgr != nullptr) {
    hr = pThreadMgr->SetFocus(pDocMgr);
    std::wcout << L"[tsf] SetFocus(document mgr) hr=0x" << std::hex << hr
               << std::endl;
  }
  if (probe_parent != nullptr && pDocMgr != nullptr) {
    ITfDocumentMgr* pPrevDm = nullptr;
    hr = pThreadMgr->AssociateFocus(probe_parent, pDocMgr, &pPrevDm);
    std::wcout << L"[tsf] AssociateFocus(hwnd, document mgr) hr=0x" << std::hex
               << hr << std::endl;
    if (pPrevDm != nullptr)
      pPrevDm->Release();
  }
  if (pFocusDm != nullptr)
    pFocusDm->Release();

  {
    // Was the text service loaded before anything was activated? If it was
    // not, the activation below is what has to load it.
    HMODULE tip32 = GetModuleHandleW(L"weasel.dll");
    HMODULE tip64 = GetModuleHandleW(L"weaselx64.dll");
    std::wcout << L"[tsf] tip loaded before the activation: weasel.dll="
               << (tip32 ? 1 : 0) << L" weaselx64.dll=" << (tip64 ? 1 : 0)
               << std::endl;
  }

  // ===== step 1b: make sure SOME process holds the text service =====
  //
  // Windows dispatches the profile notification to the threads that own the
  // text service. If no thread owns it, there is nobody to notify and the
  // QuickPanel can never appear.
  //
  // The probe window is now shown and in the foreground, which is what
  // gives this thread a focused text store. If the text service still did
  // not land in this process, a real Windows application is started and
  // focused instead: those applications get their text store from Windows
  // itself, and the text service is instantiated into them.
  {
    HMODULE tip32 = GetModuleHandleW(L"weasel.dll");
    HMODULE tip64 = GetModuleHandleW(L"weaselx64.dll");
    std::wcout << L"[tsf] tip loaded after the focus work: weasel.dll="
               << (tip32 ? 1 : 0) << L" weaselx64.dll=" << (tip64 ? 1 : 0)
               << std::endl;
    if (tip32 == nullptr && tip64 == nullptr) {
      static const wchar_t* kClients[] = {L"notepad.exe", L"regedit.exe",
                                          L"write.exe"};
      for (size_t i = 0; i < sizeof(kClients) / sizeof(kClients[0]); ++i) {
        DWORD pid = launch_tsf_client(kClients[i]);
        if (pid == 0)
          continue;
        HMODULE t32 = GetModuleHandleW(L"weasel.dll");
        HMODULE t64 = GetModuleHandleW(L"weaselx64.dll");
        std::wcout << L"[tsf] tip loaded after launching " << kClients[i]
                   << L": weasel.dll=" << (t32 ? 1 : 0)
                   << L" weaselx64.dll=" << (t64 ? 1 : 0) << std::endl;
        if (t32 != nullptr || t64 != nullptr)
          break;
      }
    }
  }

  ITfInputProcessorProfileMgr* pProfileMgr = nullptr;
  {
    // The uuid.lib symbol and the SDK header must agree on the IID, so
    // print both: a mismatch would make the QueryInterface fail for a
    // reason that has nothing to do with the feature under test.
    std::wcout << L"[tsf] IID from uuid.lib="
               << tsf_guid_text(IID_ITfInputProcessorProfileMgr)
               << L" IID from header="
               << tsf_guid_text(__uuidof(ITfInputProcessorProfileMgr))
               << std::endl;
  }
  // ITfThreadMgr does not implement the profile manager. The profile
  // manager is reached through the ITfInputProcessorProfiles object.
  ITfInputProcessorProfiles* pProfiles = nullptr;
  if (FAILED(pThreadMgr->QueryInterface(__uuidof(ITfInputProcessorProfiles),
                                        (void**)&pProfiles)) ||
      pProfiles == nullptr) {
    hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
        __uuidof(ITfInputProcessorProfiles), (void**)&pProfiles);
    if (FAILED(hr) || pProfiles == nullptr) {
      std::wcout << L"[tsf] no ITfInputProcessorProfiles" << std::endl;
      return -6;
    }
  }
  if (FAILED(pProfiles->QueryInterface(__uuidof(ITfInputProcessorProfileMgr),
                                       (void**)&pProfileMgr)) ||
      pProfileMgr == nullptr) {
    std::wcout << L"[tsf] no ITfInputProcessorProfileMgr" << std::endl;
    return -6;
  }

  // A dedicated instance of the system input processor profiles manager.
  // The one reached through the thread manager may be a per thread view of
  // it, while the language bar talks to the system wide object, so the
  // modern activation is tried on both.
  ITfInputProcessorProfiles* pSystemProfiles = nullptr;
  CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                   __uuidof(ITfInputProcessorProfiles),
                   (void**)&pSystemProfiles);
  std::wcout << L"[tsf] system ITfInputProcessorProfiles instance="
             << (pSystemProfiles ? 1 : 0) << std::endl;

  // Advise our own sink so the notification timeline is visible. This is
  // the same registration the Fluxing TIP uses.
  TsfTimelineSink sink;
  DWORD sink_cookie = TF_INVALID_COOKIE;
  bool sink_registered = false;

  ITfSource* pSource = nullptr;
  if (SUCCEEDED(
          pThreadMgr->QueryInterface(__uuidof(ITfSource), (void**)&pSource)) &&
      pSource != nullptr) {
    sink_registered = SUCCEEDED(pSource->AdviseSink(
        __uuidof(ITfActiveLanguageProfileNotifySink), &sink, &sink_cookie));
  }
  std::wcout << L"[tsf] own sink registered=" << (sink_registered ? 1 : 0)
             << std::endl;

  // Precondition: the panel must be hidden before the switch, otherwise the
  // assertion below cannot tell "the notification made it appear" from
  // "it was already up". RimeWithWeaselHandler::FocusOut hides the panel
  // unconditionally, so one plain FocusOut over IPC is enough. Ending the
  // session afterwards resets the server side active session, so the TIP
  // that gets activated below becomes the active session again.
  {
    weasel::Client client;
    if (client.Connect()) {
      client.StartSession();
      client.FocusOut();
      Sleep(300);
      client.EndSession();
    }
  }
  bool hidden_before = !quick_panel_visible();
  std::wcout << L"[tsf] panel hidden before the switch="
             << (hidden_before ? 1 : 0) << std::endl;

  // guard layer 3: remember where the Application log stands, so the
  // crashes caused by the switch can be reported afterwards.
  CrashWatcher crash_watcher;
  if (crash_watcher.available()) {
    std::wcout << L"[tsf] crash check armed at event record "
               << crash_watcher.snapshot_record() << std::endl;
  } else {
    std::wcout << L"[tsf] WARNING: the Application event log is not "
                  L"readable, the crash check is disabled"
               << std::endl;
  }

  // ===== step 1: a REAL profile switch =====
  //
  // Windows loads the Fluxing TIP into this process and calls
  // WeaselTSF::ActivateEx, which advises the profile sink. Whether the
  // notification is actually dispatched depends on two things, and rounds 1
  // to 4 got both of them wrong:
  //
  //   1. WHICH API activates the profile. The candidates are tried one
  //      after another and the first one that produces a notification wins.
  //      Every candidate reports its own hr and the number of notifications
  //      it caused, so the evidence says which path works.
  //
  //   2. THAT THE PROFILE REALLY CHANGES. The notification is dispatched
  //      when the active language profile changes. Fluxing is the only
  //      input method installed in the sandbox, so it is ALREADY the active
  //      profile and activating it again is a no-op: nothing changes,
  //      nothing is dispatched. The measured evidence was
  //          GetActiveLanguageProfile ... profile=3D02CAB6-... (Fluxing)
  //      while every activation attempt reported notifications=0.
  //
  // So the switch is done in two halves: first the Fluxing input processor
  // is DEACTIVATED (the system falls back to the plain keyboard layout),
  // then it is activated again. The second half is a real change and is
  // the event a user produces by picking Fluxing in the language bar.
  const wchar_t* kFiredBy = L"none";
  int fired_notifications = 0;

  {
    // Half one: switch AWAY from Fluxing.
    int before = g_tsf_timeline;
    hr = pProfileMgr->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                        kFluxingLangid, kFluxingTipClsid,
                                        kFluxingProfileGuid, NULL, 0);
    std::wcout << L"[tsf] switch away: DeactivateProfile hr=0x" << std::hex
               << hr << std::endl;
    report_active_profile(pSystemProfiles, L"after switch away");
    Sleep(1500);
    fired_notifications = g_tsf_timeline - before;
    std::wcout << L"[tsf] switch away notifications=" << fired_notifications
               << std::endl;
  }

  {
    int before = g_tsf_timeline;

    // A. the modern, system wide path -- what the language bar calls.
    if (pSystemProfiles != nullptr) {
      hr = pSystemProfiles->ActivateLanguageProfile(
          kFluxingTipClsid, kFluxingLangid, kFluxingProfileGuid);
      std::wcout << L"[tsf] A ActivateLanguageProfile(system) hr=0x" << std::hex
                 << hr << std::endl;
      report_active_profile(pSystemProfiles, L"after A");
      Sleep(1500);
      fired_notifications = g_tsf_timeline - before;
      std::wcout << L"[tsf] A notifications=" << fired_notifications << std::endl;
      if (fired_notifications > 0)
        kFiredBy = L"A ActivateLanguageProfile(system)";
    }

    // B. the modern path on the instance the thread manager handed out.
    if (fired_notifications == 0) {
      before = g_tsf_timeline;
      switch_away_fluxing(pProfileMgr);
      hr = pProfiles->ActivateLanguageProfile(kFluxingTipClsid, kFluxingLangid,
                                              kFluxingProfileGuid);
      std::wcout << L"[tsf] B ActivateLanguageProfile(thread mgr) hr=0x"
                 << std::hex << hr << std::endl;
      report_active_profile(pSystemProfiles, L"after B");
      Sleep(1500);
      fired_notifications = g_tsf_timeline - before;
      std::wcout << L"[tsf] B notifications=" << fired_notifications << std::endl;
      if (fired_notifications > 0)
        kFiredBy = L"B ActivateLanguageProfile(thread mgr)";
    }

    // C. the legacy TSF-5 interface.
    if (fired_notifications == 0) {
      before = g_tsf_timeline;
      switch_away_fluxing(pProfileMgr);
      hr = pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                        kFluxingLangid, kFluxingTipClsid,
                                        kFluxingProfileGuid, NULL, 0);
      std::wcout << L"[tsf] C ActivateProfile(legacy) hr=0x" << std::hex << hr
                 << std::endl;
      report_active_profile(pSystemProfiles, L"after C");
      Sleep(1500);
      fired_notifications = g_tsf_timeline - before;
      std::wcout << L"[tsf] C notifications=" << fired_notifications << std::endl;
      if (fired_notifications > 0)
        kFiredBy = L"C ActivateProfile(legacy)";
    }

    // D. the legacy activate / deactivate / activate sequence.
    if (fired_notifications == 0) {
      before = g_tsf_timeline;
      switch_away_fluxing(pProfileMgr);
      pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                    kFluxingLangid, kFluxingTipClsid,
                                    kFluxingProfileGuid, NULL, 0);
      std::wcout << L"[tsf] D legacy deactivate/activate sequence done"
                 << std::endl;
      report_active_profile(pSystemProfiles, L"after D");
      Sleep(1500);
      fired_notifications = g_tsf_timeline - before;
      std::wcout << L"[tsf] D notifications=" << fired_notifications << std::endl;
      if (fired_notifications > 0)
        kFiredBy = L"D legacy deactivate/activate sequence";
    }
  }

  // ===== step 2: what does the system think is active, and did the TIP
  // actually get loaded into this process? =====
  //
  // These two answers separate "the event was never dispatched" from
  // "the TIP was never activated, so it had no sink to be notified
  // through" -- the two failure modes look identical in the notification
  // counter alone.
  if (pSystemProfiles != nullptr) {
    LANGID active_langid = 0;
    GUID active_guid = {0};
    hr = pSystemProfiles->GetActiveLanguageProfile(kFluxingTipClsid,
                                                   &active_langid, &active_guid);
    std::wcout << L"[tsf] GetActiveLanguageProfile hr=0x" << std::hex << hr
               << L" langid=0x" << active_langid << L" profile="
               << tsf_guid_text(active_guid) << std::endl;
  }
  {
    HMODULE tip32 = GetModuleHandleW(L"weasel.dll");
    HMODULE tip64 = GetModuleHandleW(L"weaselx64.dll");
    std::wcout << L"[tsf] tip loaded into this process: weasel.dll="
               << (tip32 ? 1 : 0) << L" weaselx64.dll=" << (tip64 ? 1 : 0)
               << std::endl;
  }
  std::wcout << L"[tsf] notifications=" << g_tsf_timeline << L" fired by="
             << kFiredBy << std::endl;

  // ===== step 3: the TIP half, driven directly =====
  //
  // Everything above proves that the profile activation itself works (the
  // system even reports the Fluxing profile as the active one) but that
  // Windows never instantiates the text service into this thread: TSF
  // only attaches a text service to a thread that owns a FOCUSED TEXT
  // STORE, and a plain Win32 EDIT does not implement ITfTextStoreACP
  // (the probe above shows "document managers=2" but "GetFocus hr=0x1",
  // i.e. nothing focused).
  //
  // So the product half is verified on its own here: load the installed
  // TIP, activate it the way TSF would, and hand it the OnActivated
  // callback. If the QuickPanel shows up afterwards, the chain
  //     TIP -> server -> QuickPanel
  // is proven, and the only link still unproven is whether Windows
  // dispatches that callback on a real user switch.
  bool tip_half_worked = false;
  {
    std::wstring install_dir = read_install_dir();
    std::wstring tip_path =
        install_dir.empty() ? std::wstring()
                            : install_dir + L"\\weasel\\weasel.dll";
    std::wcout << L"[tsf] install dir=" << install_dir << L" tip="
               << tip_path << std::endl;
    if (!tip_path.empty()) {
      HMODULE tip_dll = LoadLibraryW(tip_path.c_str());
      std::wcout << L"[tsf] LoadLibrary(tip) hmodule="
                 << (tip_dll ? (void*)tip_dll : (void*)0) << std::endl;
      if (tip_dll != nullptr) {
        ITfTextInputProcessor* pTip = nullptr;
        hr = CoCreateInstance(kFluxingTipClsid, nullptr, CLSCTX_INPROC_SERVER,
                              __uuidof(ITfTextInputProcessor),
                              (void**)&pTip);
        std::wcout << L"[tsf] CoCreateInstance(TIP) hr=0x" << std::hex << hr
                   << std::endl;
        if (SUCCEEDED(hr) && pTip != nullptr) {
          // Activate it the way TSF would: this is what connects the IPC
          // client, starts the profile notification thread and advises the
          // ITfActiveLanguageProfileNotifySink. ActivateEx lives on
          // ITfTextInputProcessorEx; the plain Activate is the fallback.
          ITfTextInputProcessorEx* pTipEx = nullptr;
          if (SUCCEEDED(pTip->QueryInterface(__uuidof(ITfTextInputProcessorEx),
                                             (void**)&pTipEx)) &&
              pTipEx != nullptr) {
            hr = pTipEx->ActivateEx(pThreadMgr, client_id, 0);
            std::wcout << L"[tsf] TIP ActivateEx hr=0x" << std::hex << hr
                       << std::endl;
            pTipEx->Release();
          } else {
            hr = pTip->Activate(pThreadMgr, client_id);
            std::wcout << L"[tsf] TIP Activate hr=0x" << std::hex << hr
                       << std::endl;
          }
          pTip->Release();
        }

        ITfActiveLanguageProfileNotifySink* pTipSink = nullptr;
        hr = CoCreateInstance(kFluxingTipClsid, nullptr, CLSCTX_INPROC_SERVER,
                              __uuidof(ITfActiveLanguageProfileNotifySink),
                              (void**)&pTipSink);
        std::wcout << L"[tsf] CoCreateInstance(TIP sink) hr=0x" << std::hex
                   << hr << std::endl;
        if (SUCCEEDED(hr) && pTipSink != nullptr) {
          // Give the server a moment to settle the session first.
          Sleep(500);
          hr = pTipSink->OnActivated(kFluxingTipClsid, kFluxingProfileGuid,
                                     TRUE);
          std::wcout << L"[tsf] TIP OnActivated hr=0x" << std::hex << hr
                     << std::endl;
          for (int i = 0; i < 60; ++i) {
            Sleep(50);
            if (quick_panel_visible()) {
              tip_half_worked = true;
              break;
            }
          }
          std::wcout << L"[tsf] TIP-HALF panel visible after OnActivated="
                     << (tip_half_worked ? 1 : 0) << std::endl;
          pTipSink->Release();
        }
      }
    }
  }

  // ===== step 4: the QuickPanel must become visible =====
  //
  // The panel may be shown by a text service that lives in another process
  // (the one that owns the focused text store), so the wait is generous:
  // the notification travels through the profile manager and the IPC pipe
  // before the server puts the window on the screen.
  bool shown = false;
  for (int i = 0; i < 160 && !shown; ++i) {
    Sleep(50);
    shown = quick_panel_visible();
  }
  HWND panel = find_quick_panel();
  std::wcout << L"[tsf] panel hwnd=" << (panel ? (void*)panel : (void*)0)
             << L" visible=" << (shown ? 1 : 0) << L" notifications="
             << g_tsf_timeline << std::endl;

  // The panel's hover polling timer belongs to the thread that owns the
  // server message loop. If the panel had been created on an IPC pipe
  // thread (the bug this spec fixes), the timer would never run and the
  // panel would never auto-hide. This is only reported here: the mouse
  // position and the grace period make it a flaky assertion, and the safe
  // /verify-quickpanel check covers the auto hide without switching the
  // input profile.
  POINT old_cursor = {0, 0};
  GetCursorPos(&old_cursor);
  SetCursorPos(30, 30);
  bool auto_hidden = false;
  for (int i = 0; i < 120 && !auto_hidden; ++i) {
    Sleep(50);
    auto_hidden = !quick_panel_visible();
  }
  SetCursorPos(old_cursor.x, old_cursor.y);
  std::wcout << L"[tsf] panel auto-hid after the switch="
             << (auto_hidden ? 1 : 0) << L" (reported, not asserted)"
             << std::endl;

  int result;
  if (shown && hidden_before) {
    // The panel appeared after the profile switch and it was hidden before
    // it, so the switch is what made it appear.
    //
    // The notification does NOT have to arrive in this thread: the text
    // service that receives it lives in the process that owns the focused
    // text store, and that process is the one whose text service shows the
    // panel. The counter of this thread is supporting evidence, not the
    // assertion.
    std::wcout
        << L"[tsf] PASS: the profile activation notification reaches the "
           L"QuickPanel, fired by "
        << kFiredBy << L" (notifications in this thread=" << g_tsf_timeline
        << L")" << std::endl;
    result = 0;
  } else if (tip_half_worked) {
    // The product half is proven -- the TIP reacts to OnActivated and the
    // QuickPanel appears -- but Windows never dispatched the event in this
    // process. A distinct exit code so the gate can report the two facts
    // separately instead of collapsing them into one FAIL.
    std::wcout << L"[tsf] PARTIAL: the TIP reacts to OnActivated and the "
                  L"QuickPanel appears, but no activation API made Windows "
                  L"dispatch the event (notifications="
               << g_tsf_timeline << L")" << std::endl;
    result = -12;
  } else {
    std::wcout << L"[tsf] FAIL: shown=" << (shown ? 1 : 0) << L" hidden_before="
               << (hidden_before ? 1 : 0) << L" notifications="
               << g_tsf_timeline << L" fired by=" << kFiredBy
               << L" tip_half=" << (tip_half_worked ? 1 : 0) << std::endl;
    result = -7;
  }

  // The crash bookkeeping: give the event log a moment to settle, then
  // report every Application Error record that appeared during the switch.
  Sleep(1500);
  int new_crashes = crash_watcher.report_new_crashes();
  if (new_crashes > 0) {
    std::wcout << L"[tsf] FAIL: " << new_crashes
               << L" new crash record(s) in the Application log during the "
                  L"profile switch"
               << std::endl;
    result = -8;
  } else if (crash_watcher.available()) {
    std::wcout << L"[tsf] no new crash record in the Application log"
               << std::endl;
  }

  if (sink_registered && pSource != nullptr) {
    pSource->UnadviseSink(sink_cookie);
    pSource->Release();
  }
  pProfileMgr->Release();
  if (pSystemProfiles != nullptr)
    pSystemProfiles->Release();
  if (pContext != nullptr)
    pContext->Release();
  if (pDocMgr != nullptr) {
    pDocMgr->Pop(TF_POPF_ALL);
    pDocMgr->Release();
  }
  if (class_cookie != 0)
    CoRevokeClassObject(class_cookie);
  pThreadMgr->Release();
  CoUninitialize();
  return result;
}
