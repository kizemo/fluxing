#include "stdafx.h"

#include <WeaselIPCData.h>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <shellapi.h>
#include <tlhelp32.h>
#include "WeaselTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include <FluxingInstallPaths.h>
// spec 097: 快捷设置栏显隐链路的诊断日志。只在前端通知线程写,
// 绝不进 ITfTextInputProcessorEx::* 回调(宪法 P2)。
#include <FluxingDiagLog.h>
// ADR 0031: 补拉留痕要带上当前会话 / console 会话两个 SessionId。
#include <FluxingSessionPolicy.h>
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
  // thread (constitution P2). The thread is idle most of the time.
  //
  // spec 094: 「切走输入法 => 快捷设置栏必须消失」。这条通知必须在
  // EndSession() **之前** 投递:EndSession() 把 session_id 置 0,之后再发
  // 的 FocusOut 已经带不上会话号;而旧的 Deactivated 分支又要求
  // m_client.HasSession() 才肯发 —— 两者一夹,这条通知必被丢弃
  // (spec 094 bug 1 的根因之一)。
  _PostProfileNotify(kProfileNotifyDeactivated);
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
      // spec 097: 先取 pending,再判断 stop。
      //
      // 旧顺序(stop 早于取 pending)会在「TIP 实例被销毁」这条最常见的路径
      // 上把「用户已经切走了」整个丢掉:Deactivate() 先投递
      // kProfileNotifyDeactivated,紧接着析构函数 _JoinProfileNotifyThread()
      // 置 stop,通知线程醒来后直接 return,pending 里的意图一次也没发出去。
      // 服务端因此永远收不到 PROFILE_DEACTIVATED,快捷设置栏留在屏幕上。
      //
      // 新顺序:有 pending 就先取走并发出去;只有在「没有 pending」且 stop
      // 时才退出线程。停机最多多发一条消息,不会少发。
      kind = m_profileNotifyPending;
      m_profileNotifyPending = kProfileNotifyNone;
      if (m_profileNotifyStop && kind == kProfileNotifyNone)
        return;
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
      //
      // spec 094: 这里**不能**再用 m_client.HasSession() 当门禁 ——
      // Deactivate() 先 EndSession() 再发通知的话门禁恒假,通知必丢。
      // ClientImpl::FocusOut 本身没有 _Active() 门禁(session_id=0 也能发),
      // 服务端的 OnFocusOut 也不校验 session_id,直接透传 wParam。
      //
      // spec 097: 送达确认 + 有界重试。
      // 旧代码发完就当成功,而管道错误被 _SendMessage 的 catch(DWORD)->return 0
      // 静默吞掉 —— 「切走了」这条消息丢没丢完全不可见。现在 FocusOut 返回
      // 「是否真的送达」,失败时以 120ms 间隔最多重试 3 次。
      bool sent = false;
      for (int attempt = 0; attempt < 3 && !sent; ++attempt) {
        if (attempt > 0)
          ::Sleep(120);
        sent = m_client.FocusOut(WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);
      }
      fluxing::diag::AppendLine(
          "tsf", std::string("deactivated sent=") + (sent ? "1" : "0"));
    }
    // One-shot notification: drop this thread's pipe connection so
    // WeaselServer does not keep an idle instance around.
    m_client.CloseThreadChannel();
  }
}

STDMETHODIMP WeaselTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  // spec 061: ITfActiveLanguageProfileNotifySink::OnActivated is the
  // CORRECT trigger for "user switched to/from Fluxing IME". Unlike
  // OnKillThreadFocus (which fires on every focus jitter, see the
  // L60 fix that broke Bug 3/4), OnActivated fires exactly once
  // per real IME switch.
  //
  // spec 094 修正: clsid 的语义不是「谁还在活动」,而是「**新的上位者是谁**」。
  //   (自己 clsid, TRUE)  -> 用户切到了火流猩
  //   (自己 clsid, FALSE) -> 火流猩自己被停用
  //   (别的 clsid, TRUE)  -> 用户切到了别的输入法:本进程的火流猩已经不在活动
  //                          配置里 => 等价于「切走」=> 必须收起快捷设置栏
  //   (别的 clsid, FALSE) -> 别的 profile 被停用,与本进程无关,忽略
  //
  // 过去第一句 `if (!IsEqualCLSID(clsid, c_clsidTextService)) return S_OK;`
  // 把后两种全丢掉了,于是切走后快捷设置栏留在屏幕上不消失(spec 094 bug 1)。
  //
  // 判定表抽成共享纯函数 DecideProfileNotify(include/WeaselIPC.h),
  // 生产代码与 TestWeaselIPC 的 /verify-focusout-flags 共用同一份口径
  // (宪法 R7)。
  //
  // spec 080 (ADR 0008): this callback must not block the TSF thread on
  // I/O (constitution P2), so it performs no pipe traffic at all. It only
  // records the intent; the notification thread performs the IPC.
  //
  // OnActivated is a per-process subscription: every process that loaded
  // the Fluxing TIP receives it. The server side de-duplicates by
  // comparing the session id with its active session, so only the front
  // most application ends up showing the QuickPanel.
  const bool is_self = (IsEqualCLSID(clsid, c_clsidTextService) != FALSE);
  switch (DecideProfileNotify(is_self, isActivated != FALSE)) {
    case ProfileNotifyDecision::kNotifyActivated:
      _PostProfileNotify(kProfileNotifyActivated);
      break;
    case ProfileNotifyDecision::kNotifyDeactivated:
      _PostProfileNotify(kProfileNotifyDeactivated);
      break;
    case ProfileNotifyDecision::kIgnore:
      break;
  }
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

// ADR 0031 决策 4 — console 缺位自愈。
//
// 先说清"复用了什么": 用 codebase-memory-mcp 以 trace_path(inbound) 核实过,
// _EnsureServerConnected 的调用方是 KeyEventSink::_ProcessKeyEvent 与
// WeaselTSF::ActivateEx —— 两者都跑在 TSF 回调线程上。该函数在 retry >= 6 时
// **早已存在**"起一个 detached 线程 ShellExecuteW 拉起服务"的机制, 因此按决策 4
// 的明文("若实现审查发现 TSF 侧已有等价拉起机制, 则只做节流 + 留痕增强")
// 不新增机制, 只补三项缺口:
//   1. 最小间隔节流 —— 原来只要 retry 攒满 6 就可能再拉一次, 没有时间下限;
//   2. 跨回调单飞     —— 原来多次回调可各自 detach 一个线程, 互相不知道;
//   3. 结构化留痕     —— 原来整个拉起过程零记录, 事后无法判断前端试过没有。
// 顺带把 CreateMutex / CreateToolhelp32Snapshot / 注册表读 / Echo /
// ShellExecuteW / 写日志全部挪进工作线程: 宪法 P2 禁止 ITfTextInputProcessor::*
// 回调里做阻塞 I/O。 于是回调线程上只剩 GetTickCount64() 比较 + 一次 atomic
// exchange。 注: 工作线程里碰 m_client
// 是本函数改动前就有的行为(原线程尾部已调用 _Reconnect());
// 单飞闸门保证同一时刻最多一个工作线程, 不额外放大竞争面。
static constexpr ULONGLONG kServerRelaunchMinIntervalMs = 60ull * 1000ull;
static std::atomic<ULONGLONG> g_serverRelaunchLastMs(0);
static std::atomic<bool> g_serverRelaunchInFlight(false);

bool WeaselTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    _Reconnect();
    retry++;
    if (retry >= 6) {
      const ULONGLONG now = ::GetTickCount64();
      const ULONGLONG last =
          g_serverRelaunchLastMs.load(std::memory_order_relaxed);
      // last == 0 表示本进程从未补拉过, 不受节流约束。
      const bool throttled =
          (last != 0 && now - last < kServerRelaunchMinIntervalMs);
      bool claimed = false;
      if (!throttled) {
        claimed = (g_serverRelaunchInFlight.exchange(
                       true, std::memory_order_acq_rel) == false);
      }
      if (claimed) {
        g_serverRelaunchLastMs.store(now, std::memory_order_relaxed);
        std::thread th([this]() {
          // 以下一律在工作线程执行。
          struct InFlightRelease {
            ~InFlightRelease() {
              g_serverRelaunchInFlight.store(false, std::memory_order_release);
            }
          } const releaseGuard;

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

          HANDLE hMutex =
              CreateMutex(NULL, TRUE, L"WeaselDeployerExclusiveMutex");
          // 紧跟 CreateMutex 取走, 否则后面任何调用都会把这个码冲掉。
          const DWORD mutexError = ::GetLastError();
          const bool deployerBusy = (mutexError == ERROR_ALREADY_EXISTS);
          const bool stillDown = !m_client.Echo();
          int serverCount = 0;
          bool launched = false;
          bool noTarget = false;
          if (stillDown && !deployerBusy) {
            serverCount = count_server_process();
            if (serverCount == 0) {
              // spec 088: 以前这里无条件拼 "<dir>\start_service.bat",而 dir
              // 来自 只读 "WeaselRoot" 的注册表查询 ⇒ 本 fork
              // 下恒为空路径,拉起静默失败。 现在走统一的"启动目标"选择器(优先
              // bat,退回 WeaselServer.exe)。
              const std::wstring dir = _GetRootDir();
              const std::wstring target =
                  fluxing::WeaselServiceLaunchTarget(dir);
              if (!target.empty()) {
                ShellExecuteW(NULL, L"open", target.c_str(), NULL, dir.c_str(),
                              SW_HIDE);
                launched = true;
                // wait 500ms, then reconnect
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                _Reconnect();
              } else {
                noTarget = true;
              }
            }
          }
          if (hMutex) {
            CloseHandle(hMutex);
          }

          std::string line = fluxing::session::Describe(
              launched ? "server_relaunch_launched"
                       : (noTarget ? "server_relaunch_skipped_no_target"
                                   : "server_relaunch_skipped"),
              fluxing::session::EvaluateCurrent());
          line += " echo_ok=";
          line += (stillDown ? "0" : "1");
          line += " deployer_busy=";
          line += (deployerBusy ? "1" : "0");
          line += " server_process_count=";
          line += std::to_string((long long)serverCount);
          line += " mutex_error=";
          line += std::to_string((unsigned long long)mutexError);
          fluxing::diag::AppendLine("tsf", line);
        });
        th.detach();
      } else {
        // 节流/单飞命中也要留痕, 但写在回调线程上会违反宪法 P2,
        // 因此只在确实被节流时记一个 tick 差值 —— 由工作线程之外的路径
        // 走到这里时没有线程可借, 遂不写盘: 这条分支的信息量(距上次 N ms)
        // 已经由上一条 server_relaunch_* 记录的时间戳隐含。
      }
      retry = 0;
    }
    return (m_client.Echo() != 0);
  } else {
    return true;
  }
}
