# 0008. The IME profile switch is reported by the TSF over a dedicated notification thread and gated by the Server's active session

- **Status**: Accepted
- **Date**: 2026-10-23
- **Deciders**: Fluxing maintainers
- **Related**: spec 080, spec 052 (US052-A), lessons L57bis / L59-L62 / L67-L69 / L75 / L110, commit 78e1109, constitution P2, `.claude/rules/ipc-boundary.md`

## Context

The user wants the QuickPanel (the "shortcut settings bar", `Alt+,`) to
appear automatically when the active input profile is switched to Fluxing.
The feature was designed and built before: spec 052 (US052-A) introduced
"always show mode", spec 056/060/061/066 wired it to
`ITfActiveLanguageProfileNotifySink::OnActivated`, and it reached
v0.18.41.0. It was then switched off — not because the design was wrong,
but because the GDI+ `Bitmap(IStream*)` lifetime bug (L67-L69) crashed
`WeaselServer.exe` with `STATUS_HEAP_CORRUPTION` on the first paint after
the panel was shown. v0.18.41.4 disabled every trigger with a literal
`if (false && ipc_id > 0)` in `RimeWithWeaselHandler::FocusIn`. Spec 070 /
L75 later rewrote the panel to decode its logo with WIC and paint with
plain GDI, which cut that crash chain, but the kill switch was never
removed. The feature is therefore *designed but switched off*, not
missing.

Rebuilding it is constrained by four facts, all verified on the machine
before deciding:

1. **The TSF thread must not block on I/O** (`constitution P2`: "Never
   block the TSF thread for I/O"; `.claude/rules/ipc-boundary.md`:
   no `CreateFile` / synchronous registry / `BeginWaitForSingleObject`
   inside a TSF callback). The old `OnActivated` implementation called
   `m_client.Echo()` + `m_client.FocusIn()` — two synchronous pipe round
   trips. Commit 78e1109 already had to revert an `OnActivated` change
   for exactly this reason ("worst case the TSF thread hangs for
   seconds, which deadlocks the TS manager and every IME call").

2. **`OnActivated` is a per-process subscription.** Switching the input
   method once makes *every* process that has loaded the Fluxing TIP
   receive the callback, so N processes each report the same switch.
   The historical "the QuickPanel always shows up in the bottom-right
   corner" bug (spec 055 bug 2) is a direct consequence of the panel
   being driven from all of them.

3. **The Server cannot detect the switch by itself.** The Fluxing TIP
   registers **no keyboard layout** (`HKLM\SYSTEM\CurrentControlSet\Control\Keyboard Layouts`
   contains no Fluxing entry, verified on the machine), so the foreground
   thread's `HKL` does not change when Fluxing becomes active. A TSF
   input profile is a per-thread, in-process concept and is not
   observable from another process. So the front-end has to report it.

4. **`QuickPanelDialog` is thread-affine.** It creates a window, owns GDI
   resources and runs a 100 ms hover polling timer, so it must only be
   driven from the thread that owns the server message loop. The
   existing language-bar path (`WEASEL_IPC_TRAY_COMMAND` → menu handler)
   already violated this: the panel was created on an IPC pipe thread,
   whose message queue is never pumped, so the buttons and the timer
   could not work.

5. **The sink was never registered.** `WeaselTSF` inherited
   `ITfActiveLanguageProfileNotifySink` and implemented `OnActivated`, but
   its `QueryInterface` had no branch for
   `IID_ITfActiveLanguageProfileNotifySink`, and nothing in the tree ever
   called `AdviseSink(IID_ITfActiveLanguageProfileNotifySink, ...)`. TSF
   therefore never learned about the sink and `OnActivated` was dead code:
   the trigger could not fire no matter what the callback body did. The
   fix existed once, in v0.19.0.66 (commit 02b90a6), but that Phase L line
   was rolled back after a 17-minute install hang and never reached this
   branch (`git branch -a --contains 02b90a6` is empty; the commit is a
   dangling object). The Windows SDK exposes exactly one registration path
   for this sink, `ITfSource::AdviseSink`; there is no
   `ITfInputProcessorProfiles::RegisterActiveLanguageProfileNotifySink`.

## Decision

We report the switch over the **existing WeaselIPC pipe** and keep the
TSF callback free of I/O.

- **One new flag bit, no new message type.**
  `WEASEL_IPC_FOCUS_IN`'s `wParam` was an unused "client caps" field.
  It now carries `WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED` (bit 0),
  which distinguishes "the user just switched to Fluxing" from "this
  window gained focus" (the latter arrives through the same message
  from `ITfKeyEventSink::OnSetFocus`). No wire struct changes, no new
  command, no deployer change — `WeaselDeployer` never sends
  `WEASEL_IPC_FOCUS_IN`.

- **The TIP advises the sink to TSF.** `WeaselTSF` registers
  `ITfActiveLanguageProfileNotifySink` through `ITfSource::AdviseSink` on
  the thread manager, in `ActivateEx` and *after*
  `_EnsureServerConnected()`: the notification carries the session id, so
  the session has to exist before any notification can arrive. The
  registration is unadvised in `Deactivate`, guarded by the usual
  `TF_INVALID_COOKIE` check. A failed registration is **not** fatal:
  losing the QuickPanel trigger is acceptable, losing Chinese input is not.

- **The TSF callback does zero I/O.** `OnActivated` only records the
  intent and wakes a dedicated notification thread
  (`WeaselTSF::_ProfileNotifyThread`). The thread performs the pipe
  round trip. Because `PipeChannel` keeps its pipe handle in a
  `thread_local` slot, the thread owns a separate connection; the
  notification is a one-shot, so the thread drops that connection right
  after sending it (`ClientImpl::CloseThreadChannel`) instead of keeping
  an idle pipe instance alive for every front-end process.
  `ClientImpl::session_id` became `std::atomic<UINT>` because the
  notification thread reads it while the TSF thread writes it.

- **The Server gates the request on its active session.**
  `RimeWithWeaselHandler::FocusIn` compares the notifying session id
  with `m_active_session`. The front-most application's session matches
  and the panel is shown; background processes are ignored. The
  profile-activated branch deliberately **does not** touch
  `m_active_session`, so a background process can never steal it.

- **The Server shows the panel on its message-loop thread.** The request
  handler runs on an IPC pipe thread; it posts `WM_COMMAND` to the
  server window, and the message-loop thread calls
  `WeaselServerApp::ShowQuickPanel()`. The pre-existing
  `ID_WEASELTRAY_QUICK_PANEL` handler was marshalled the same way,
  which also repairs the language-bar left click.

### Alternatives that were considered and rejected

| Alternative | Why it was rejected |
|---|---|
| Server-side detection by polling the foreground thread's keyboard layout | The TIP registers no keyboard layout, so the HKL never changes (fact 3). |
| Server-side detection through its own `ITfInputProcessorProfileMgr` | The active profile is per-thread; a thread without an input context does not observe another app's switch, and the semantics are undocumented. |
| A second cross-process channel (named event) instead of the pipe | `constitution P2` requires front-ends to talk to `WeaselServer` through the existing WeaselIPC protocol; a second mechanism would need its own lifecycle, ACL and failure modes. |
| Keep the IPC on the TSF thread (one round trip) | Violates P2 and repeats the trap that 78e1109 fell into. |
| A new `WEASEL_IPC_SHOW_QUICK_PANEL` command | Works, but adds a command to the protocol for a signal that is a *variant of an existing event*. A flag bit keeps the protocol surface unchanged and makes an old server degrade gracefully. |

## Consequences

- ✅ The TSF callback is I/O-free again, and the "switch to Fluxing"
  path can no longer stall the host application's UI thread.
- ✅ Exactly one panel appears, in the application the user is actually
  typing in, at the moment of the switch.
- ✅ The GDI+ crash chain (L67-L69) stays cut: the panel is shown from
  the profile-activated branch only, and the logo is decoded with WIC.
- ✅ An **old** `WeaselServer.exe` ignores the unknown flag bit and
  treats the message as a plain `FocusIn`: no panel, no crash. The TSF
  and the server can therefore be upgraded independently, and a mixed
  installation degrades to "the panel does not auto-show".
- ⚠️ Every front-end process that activates the TIP owns one extra idle
  thread while the text service is active. It is created in
  `ActivateEx`; `Deactivate` only *signals* it to stop, because joining a
  thread on the TSF thread would block the TSF thread (P2). The bounded
  join (3 s wait, then `TerminateThread` as a last resort) happens in the
  destructor. A wedged server must never be able to freeze the host
  application while the text service is torn down.
- ✅ The notification is proven end to end on the machine, not only
  reasoned about: `TestWeaselIPC.exe /verify-tsf-profile` drives
  `ITfInputProcessorProfileMgr` to really activate and deactivate the
  Fluxing profile, so Windows itself delivers `OnActivated`, and then
  asserts that the QuickPanel window became visible. The same driver run
  against a build without the sink registration reports
  `visible=0 notifications=4` -- the notifications arrive, the panel does
  not -- which makes the registration the single causal difference.
- ⚠️ The panel is a *transient* window at the bottom-right of the work
  area with the existing 2 s grace period and cursor-outside auto-hide.
  It is deliberately **not** the "always show mode" of spec 052, which
  the user rejected in spec 055.
- ⚠️ `spec 077`'s ADR claims `WEASEL_IPC_SHIFT_DOWN/UP` are
  fire-and-forget, but the code path (`ClientImpl::_SendMessage` →
  `PipeChannel::Transact`) still waits for the pipe response. That
  discrepancy is untouched by this decision and is a separate issue.
