#include "stdafx.h"

#include <WeaselIPCData.h>
#include <chrono>
#include <thread>
#include <shellapi.h>
#include <tlhelp32.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include <FluxingInstallPaths.h>
#include "Compartment.h"
#include "ResponseParser.h"

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_weasel_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

WeaselTSF::WeaselTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

WeaselTSF::~WeaselTSF() {
  // spec 080: the notification thread lives as long as this object, so it is
  // stopped here. Deactivate() only signals it, because Deactivate runs on
  // the TSF thread and must not block (constitution P2).
  _JoinProfileNotifyThread();
  DllRelease();
}

STDAPI WeaselTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfActiveLanguageProfileNotifySink))
    *ppvObject = (ITfActiveLanguageProfileNotifySink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDAPI_(ULONG) WeaselTSF::AddRef() {
  return ++_cRef;
}

STDAPI_(ULONG) WeaselTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDAPI WeaselTSF::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDAPI WeaselTSF::Deactivate() {
  // spec 080: the notification thread is NOT stopped here. Deactivate runs
  // on the TSF thread, and joining a thread there would block the TSF
  // thread (constitution P2). The thread is idle most of the time and it
  // re-checks m_client.HasSession() before every send, so a notification
  // that arrives after the session ended is simply dropped.
  m_client.EndSession();

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  // spec 080: stop advising the language profile sink while _pThreadMgr is
  // still valid (it is cleared further down).
  _UninitActiveLanguageProfileSink();

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitThreadMgrEventSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  _cand->DestroyAll();

  return S_OK;
}

STDAPI WeaselTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                             TfClientId tfClientId,
                             DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  _activateFlags = dwFlags;

  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink())
    goto ExitError;

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
      (pDocMgrFocus != NULL)) {
    _InitTextEditSink(pDocMgrFocus);
  }

  if (!_InitKeyEventSink())
    goto ExitError;

  // if (!_InitDisplayAttributeGuidAtom())
  //	goto ExitError;
  //	some app might init failed because it not provide DisplayAttributeInfo,
  // like some opengl stuff
  _InitDisplayAttributeGuidAtom();

  if (!_InitPreservedKey())
    goto ExitError;

  if (!_InitLanguageBar())
    goto ExitError;

  if (!_IsKeyboardOpen())
    _SetKeyboardOpen(TRUE);

  if (!_InitCompartment())
    goto ExitError;
  if (!_InitThreadFocusSink())
    goto ExitError;

  _EnsureServerConnected();

  // spec 080 (ADR 0008): the profile switch notification thread. Started
  // here so it exists exactly while the text service is active for this
  // thread.
  _StartProfileNotifyThread();

  // spec 080 (ADR 0008): advise the language profile notification sink.
  // This is what makes ITfActiveLanguageProfileNotifySink::OnActivated
  // fire when the user switches to or away from Fluxing. The interface was
  // implemented but never registered, so the callback was dead code.
  //
  // Advising happens AFTER _EnsureServerConnected() on purpose: the
  // notification handler sends IPC carrying the session id, so the session
  // has to exist before any notification can arrive.
  //
  // A failure here is NOT fatal. Losing the QuickPanel trigger is
  // acceptable, losing Chinese input is not.
  _InitActiveLanguageProfileSink();

  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

STDMETHODIMP WeaselTSF::OnSetThreadFocus() {
  std::wstring _ToggleImeOnOpenClose{};
  RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Rime\\weasel",
                    L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
  _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
  if (m_client.Echo()) {
    m_client.ProcessKeyEvent(0);
    weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
    bool ok = m_client.GetResponseData(std::ref(parser));
    if (ok)
      _UpdateLanguageBar(_status);
  }
  return S_OK;
}
STDMETHODIMP WeaselTSF::OnKillThreadFocus() {
  // spec 061: do NOT call _AbortComposition() here. OnKillThreadFocus
  // fires on EVERY TSF focus event (cursor move, menu pop, compartment
  // state change) - way more often than "user switched to a different
  // IME". The previous behavior aborted the composition on every focus
  // jitter, which broke Chinese input (the composition state was lost
  // on every focus event).
  //
  // _AbortComposition is now called ONLY from:
  //   1. Deactivate() (clean shutdown)
  //   2. OnKillThreadFocus was the only "jitter" caller - removed.
  //
  // Composition state is preserved across focus events and only
  // reset when the user explicitly commits (Enter / number key) or
  // actually deactivates the IME.
  return S_OK;
}
BOOL WeaselTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}
void WeaselTSF::_UninitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    return;
}

// spec 080 (ADR 0008): advise / unadvise ITfActiveLanguageProfileNotifySink.
//
// ITfSource::AdviseSink supports IID_ITfActiveLanguageProfileNotifySink
// (Windows SDK msctf.h). The sink fires OnActivated when the active
// language profile of this thread changes, which is exactly the "the user
// switched to or away from Fluxing" event.
//
// Lifetime notes:
//   - The cookie starts at TF_INVALID_COOKIE. UnadviseSink must never be
//     called with an invalid cookie, TSF treats that as internal
//     corruption. Same guard as _InitThreadMgrEventSink.
//   - AdviseSink is called once per ActivateEx. A failure is reported but
//     never aborts activation, so the IME keeps working even when the
//     QuickPanel trigger cannot be installed.
BOOL WeaselTSF::_InitActiveLanguageProfileSink() {
  if (_dwActiveLanguageProfileSinkCookie != TF_INVALID_COOKIE)
    return TRUE;
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfActiveLanguageProfileNotifySink,
                                 (ITfActiveLanguageProfileNotifySink*)this,
                                 &_dwActiveLanguageProfileSinkCookie))) {
    _dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;
    return FALSE;
  }
  return TRUE;
}

void WeaselTSF::_UninitActiveLanguageProfileSink() {
  if (_dwActiveLanguageProfileSinkCookie == TF_INVALID_COOKIE)
    return;
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  pSource->UnadviseSink(_dwActiveLanguageProfileSinkCookie);
  _dwActiveLanguageProfileSinkCookie = TF_INVALID_COOKIE;
}

// ===== spec 080: IME profile switch notification (ADR 0008) =====
//
// Why a dedicated thread:
//   constitution P2 forbids blocking the TSF thread for I/O. OnActivated is
//   a TSF callback, so it must not touch the pipe. It used to call
//   m_client.Echo() + m_client.FocusIn() directly (two synchronous pipe
//   round trips); commit 78e1109 already had to revert a similar change
//   for exactly this reason.
//
// Why the thread owns its own pipe connection:
//   PipeChannel keeps the pipe handle in a thread_local slot, so a thread
//   that talks to WeaselServer owns a separate connection. The
//   notification is a one-shot, so the thread drops the connection right
//   after sending it (ClientImpl::CloseThreadChannel) instead of keeping
//   an idle pipe instance alive for every front-end process.

void WeaselTSF::_StartProfileNotifyThread() {
  if (m_profileNotifyThread != nullptr)
    return;
  {
    std::lock_guard<std::mutex> lock(m_profileNotifyMutex);
    m_profileNotifyStop = false;
    m_profileNotifyPending = kProfileNotifyNone;
  }
  m_profileNotifyThread =
      CreateThread(NULL, 0, _ProfileNotifyProc, this, 0, NULL);
  if (m_profileNotifyThread == nullptr) {
    // Thread creation failed: the feature degrades to "manual Alt+," only,
    // which is better than breaking input altogether.
  }
}

void WeaselTSF::_StopProfileNotifyThread() {
  // Signal only. This must never wait: it can be reached from the TSF
  // thread, and constitution P2 forbids blocking the TSF thread.
  {
    std::lock_guard<std::mutex> lock(m_profileNotifyMutex);
    m_profileNotifyStop = true;
  }
  m_profileNotifyCv.notify_all();
}

void WeaselTSF::_JoinProfileNotifyThread() {
  if (m_profileNotifyThread == nullptr)
    return;
  _StopProfileNotifyThread();
  if (WaitForSingleObject(m_profileNotifyThread, 3000) == WAIT_TIMEOUT) {
    // Last resort: the thread is stuck in a pipe read while WeaselServer
    // stopped answering. Waiting forever would hang the host application
    // while the text service is being torn down, so cut it loose. The pipe
    // handle owned by that thread leaks (one handle, only on this path).
    TerminateThread(m_profileNotifyThread, 0);
  }
  CloseHandle(m_profileNotifyThread);
  m_profileNotifyThread = nullptr;
}

void WeaselTSF::_WaitForSession(DWORD timeout_ms) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (!m_client.HasSession() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

void WeaselTSF::_PostProfileNotify(ProfileNotifyKind kind) {
  {
    std::lock_guard<std::mutex> lock(m_profileNotifyMutex);
    if (m_profileNotifyStop)
      return;
    // Latest intent wins: a rapid activate/deactivate pair must not show a
    // stale QuickPanel.
    m_profileNotifyPending = kind;
  }
  m_profileNotifyCv.notify_one();
}

DWORD WINAPI WeaselTSF::_ProfileNotifyProc(LPVOID param) {
  static_cast<WeaselTSF*>(param)->_ProfileNotifyLoop();
  return 0;
}

void WeaselTSF::_ProfileNotifyLoop() {
  for (;;) {
    ProfileNotifyKind kind = kProfileNotifyNone;
    {
      std::unique_lock<std::mutex> lock(m_profileNotifyMutex);
      m_profileNotifyCv.wait(lock, [this] {
        return m_profileNotifyStop ||
               m_profileNotifyPending != kProfileNotifyNone;
      });
      if (m_profileNotifyStop)
        return;
      kind = m_profileNotifyPending;
      m_profileNotifyPending = kProfileNotifyNone;
    }
    if (kind == kProfileNotifyActivated) {
      // The flag tells the server "the user just switched to Fluxing",
      // which is a different event from "this window got focus".
      //
      // The notification carries the session id, so the session has to
      // exist. It normally does (the sink is advised after the session is
      // established), but a profile switch that reactivates the text
      // service can deliver the notification while the session is being
      // re-established. Wait a bounded amount of time for it instead of
      // dropping the event.
      _WaitForSession(2000);
      if (m_client.HasSession())
        m_client.FocusIn(WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED);
    } else if (kind == kProfileNotifyDeactivated) {
      // spec 091 / ADR 0024: tell the server that the user switched to
      // another input method. Without this flag the server cannot tell
      // "switched away from Fluxing" apart from "the edit field lost
      // focus", so the always-show guard swallows the hide request.
      if (m_client.HasSession())
        m_client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);
    }
    // One-shot notification: drop this thread's pipe connection so
    // WeaselServer does not keep an idle instance around.
    m_client.CloseThreadChannel();
  }
}

STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  // spec 061: ITfActiveLanguageProfileNotifySink::OnActivated is the
  // CORRECT trigger for "user switched to/from Fluxing IME". Unlike
  // OnKillThreadFocus (which fires on every focus jitter, see the
  // L60 fix that broke Bug 3/4), OnActivated fires exactly once
  // per real IME switch.
  //
  // But: isActivated is "is Fluxing now the active text service",
  // NOT "is the user typing". So:
  //   isActivated = true  -> user just switched TO Fluxing
  //   isActivated = false -> user just switched AWAY from Fluxing
  //
  // spec 080 (ADR 0008): this callback must not block the TSF thread on
  // I/O (constitution P2), so it performs no pipe traffic at all. It only
  // records the intent; the notification thread performs the IPC.
  //
  //   isActivated = true  -> user just switched TO Fluxing
  //   isActivated = false -> user just switched AWAY from Fluxing
  //
  // OnActivated is a per-process subscription: every process that loaded
  // the Fluxing TIP receives it. The server side de-duplicates by
  // comparing the session id with its active session, so only the front
  // most application ends up showing the QuickPanel.
  _PostProfileNotify(isActivated ? kProfileNotifyActivated
                                 : kProfileNotifyDeactivated);
  return S_OK;
}

void WeaselTSF::_Reconnect() {
  m_client.Disconnect();
  m_client.Connect(NULL);
  m_client.StartSession();
  weasel::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style());
  bool ok = m_client.GetResponseData(std::ref(parser));
  if (ok) {
    _UpdateLanguageBar(_status);
    // spec 061: tell server "client just connected" so it can
    // EnableAlwaysShowMode (spec 052 US052-A). This replaces the
    // L60 OnActivated trigger which had a race condition (TSF
    // sink subscribed before m_client.Connect/StartSession).
    m_client.FocusIn();
  } else {
    // Connection failed - ensure server-side QuickPanel is hidden
    // (don't show stale QuickPanel from a previous session).
    m_client.FocusOut();
  }
}

static unsigned int retry = 0;

bool WeaselTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    _Reconnect();
    retry++;
    if (retry >= 6) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
      const auto count_server_process = []() -> int {
        int count = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
          return 0;
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
          do {
            if (_wcsicmp(pe.szExeFile, L"WeaselServer.exe") == 0)
              count++;
          } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return count;
      };
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS &&
          !count_server_process()) {
        // spec 088: 以前这里无条件拼 "<dir>\start_service.bat",而 dir 来自
        // 只读 "WeaselRoot" 的注册表查询 ⇒ 本 fork 下恒为空路径,拉起静默失败。
        // 现在走统一的"启动目标"选择器(优先 bat,退回 WeaselServer.exe)。
        const std::wstring dir = _GetRootDir();
        const std::wstring target = fluxing::WeaselServiceLaunchTarget(dir);
        if (!target.empty()) {
          std::thread th([dir, target, this]() {
            ShellExecuteW(NULL, L"open", target.c_str(), NULL, dir.c_str(),
                          SW_HIDE);
            // wait 500ms, then reconnect
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            _Reconnect();
          });
          th.detach();
        }
      }
      if (hMutex) {
        CloseHandle(hMutex);
      }
      retry = 0;
    }
    return (m_client.Echo() != 0);
  } else {
    return true;
  }
}
