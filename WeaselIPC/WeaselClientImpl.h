#pragma once
#include <WeaselIPC.h>
#include <PipeChannel.h>

#include <atomic>

namespace weasel {

class ClientImpl {
 public:
  ClientImpl();
  ~ClientImpl();

  bool Connect(ServerLauncher const& launcher);
  void Disconnect();
  void ShutdownServer();
  void StartSession();
  void EndSession();
  void StartMaintenance();
  void EndMaintenance();
  bool Echo();
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  bool CommitComposition();
  bool ClearComposition();
  bool SelectCandidateOnCurrentPage(size_t index);
  // spec 077: raw Shift_L/R notification to Server-side state machine.
  // Fire-and-forget (C2): do not block on pipe response.
  bool ShiftDown(bool is_left);
  bool ShiftUp(bool is_left);
  // spec 077: bypass state machine and select absolute candidate index.
  bool SelectCandidate(size_t index);
  bool HighlightCandidateOnCurrentPage(size_t index);
  bool DeleteCandidateOnCurrentPage(size_t index);
  bool ChangePage(bool backward);
  void UpdateInputPosition(RECT const& rc);
  // spec 080: client_caps carries WEASEL_IPC_FOCUS_IN_FLAG_* bit flags.
  void FocusIn(DWORD client_caps = 0);
  void FocusOut();
  // spec 080: close the pipe connection owned by the CALLING thread only.
  // PipeChannel keeps its pipe handle in a thread_local slot, so every
  // thread that talks to WeaselServer owns a separate connection. The TSF
  // notification thread uses this to drop its connection right after a
  // one-shot notification, so WeaselServer does not keep an idle pipe
  // instance (plus a server thread) alive per front-end process.
  // Does not touch the session and does not affect other threads.
  void CloseThreadChannel();
  // True when this client owns a live session.
  bool HasSession() const { return session_id.load() != 0u; }
  void TrayCommand(UINT menuId);
  bool GetResponseData(ResponseHandler const& handler);

 protected:
  void _InitializeClientInfo();
  bool _WriteClientInfo();

  LRESULT _SendMessage(WEASEL_IPC_COMMAND Msg, DWORD wParam, DWORD lParam);

  bool _Connected() const { return channel.Connected(); }
  bool _Active() const {
    return channel.Connected() && session_id.load() != 0u;
  }

 private:
  // spec 080: atomic because the TSF notification thread reads the session
  // id while the TSF thread writes it (StartSession / EndSession).
  std::atomic<UINT> session_id;
  std::wstring app_name;
  bool is_ime;

  PipeChannel<PipeMessage> channel;
};

}  // namespace weasel