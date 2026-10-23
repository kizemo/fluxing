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
//   3. activate the Fluxing input profile: Windows loads the Fluxing TIP
//      (weasel.dll) into this process and calls WeaselTSF::ActivateEx,
//      which advises the profile sink and starts the notification thread
//   4. deactivate the profile (switch away) and activate it again
//   5. assert the QuickPanel window became visible
//
// The profile switch is done with ITfInputProcessorProfileMgr, which is
// thread scoped: the test never changes the desktop wide input method.
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

  // A client id is needed to activate the thread manager. ITfClientId hands
  // out the id registered for a class, so try the classes this thread may
  // know about. Not being able to get one is not fatal: the profile
  // activation below can still work.
  TfClientId client_id = 0;
  ITfClientId* pClientId = nullptr;
  if (SUCCEEDED(pThreadMgr->QueryInterface(__uuidof(ITfClientId),
                                           (void**)&pClientId)) &&
      pClientId != nullptr) {
    const GUID candidates[] = {kFluxingTipClsid, CLSID_TF_ThreadMgr};
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
      TfClientId id = 0;
      if (SUCCEEDED(pClientId->GetClientId(candidates[i], &id)) && id != 0) {
        client_id = id;
        std::wcout << L"[tsf] client id from clsid="
                   << tsf_guid_text(candidates[i]) << L" id=" << client_id
                   << std::endl;
        break;
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

  // step 1: activate the Fluxing profile. Windows loads the Fluxing TIP into
  // this process and calls WeaselTSF::ActivateEx.
  hr = pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                    kFluxingLangid, kFluxingTipClsid,
                                    kFluxingProfileGuid, NULL, 0);
  std::wcout << L"[tsf] ActivateProfile(Fluxing) hr=0x" << std::hex << hr
             << std::endl;
  Sleep(1500);

  // step 2: switch away from Fluxing.
  hr = pProfileMgr->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                      kFluxingLangid, kFluxingTipClsid,
                                      kFluxingProfileGuid, NULL, 0);
  std::wcout << L"[tsf] DeactivateProfile hr=0x" << std::hex << hr << std::endl;
  Sleep(1500);

  // step 3: switch back to Fluxing. This is the "the user switched to
  // Fluxing" event the feature reacts to.
  hr = pProfileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,
                                    kFluxingLangid, kFluxingTipClsid,
                                    kFluxingProfileGuid, NULL, 0);
  std::wcout << L"[tsf] ActivateProfile(Fluxing) again hr=0x" << std::hex << hr
             << std::endl;

  // step 4: the QuickPanel must become visible.
  bool shown = false;
  for (int i = 0; i < 60 && !shown; ++i) {
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
  if (shown && hidden_before && g_tsf_timeline > 0) {
    std::wcout
        << L"[tsf] PASS: the profile activation notification reaches the "
           L"QuickPanel"
        << std::endl;
    result = 0;
  } else {
    std::wcout << L"[tsf] FAIL: shown=" << (shown ? 1 : 0) << L" hidden_before="
               << (hidden_before ? 1 : 0) << L" notifications="
               << g_tsf_timeline << std::endl;
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
  pThreadMgr->Release();
  CoUninitialize();
  return result;
}
