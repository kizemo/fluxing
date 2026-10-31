#pragma once
#include <WeaselIPCData.h>
#include <WeaselUtility.h>
#include <windows.h>
#include <functional>
#include <memory>
#include <KeyEvent.h>

#define WEASEL_IPC_WINDOW L"WeaselIPCWindow_1.0"
#define WEASEL_IPC_PIPE_NAME L"WeaselNamedPipe"

#define WEASEL_IPC_METADATA_SIZE 1024
#define WEASEL_IPC_BUFFER_SIZE (4 * 1024)
#define WEASEL_IPC_BUFFER_LENGTH (WEASEL_IPC_BUFFER_SIZE / sizeof(WCHAR))
#define WEASEL_IPC_SHARED_MEMORY_SIZE \
  (sizeof(PipeMessage) + WEASEL_IPC_BUFFER_SIZE)

enum WEASEL_IPC_COMMAND {
  WEASEL_IPC_ECHO = (WM_APP + 1),
  WEASEL_IPC_START_SESSION,
  WEASEL_IPC_END_SESSION,
  WEASEL_IPC_PROCESS_KEY_EVENT,
  WEASEL_IPC_SHUTDOWN_SERVER,
  WEASEL_IPC_FOCUS_IN,
  WEASEL_IPC_FOCUS_OUT,
  WEASEL_IPC_UPDATE_INPUT_POS,
  WEASEL_IPC_START_MAINTENANCE,
  WEASEL_IPC_END_MAINTENANCE,
  WEASEL_IPC_COMMIT_COMPOSITION,
  WEASEL_IPC_CLEAR_COMPOSITION,
  WEASEL_IPC_TRAY_COMMAND,
  WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_CHANGE_PAGE,
  WEASEL_IPC_SHIFT_DOWN,
  WEASEL_IPC_SHIFT_UP,
  WEASEL_IPC_SELECT_CANDIDATE,
  WEASEL_IPC_LAST_COMMAND
};

// WEASEL_IPC_FOCUS_IN 的 wParam 位标志(spec 080 / ADR 0008)。
// 低 8 位是 flags,高 24 位预留给 client capabilities(历史 TODO,至今未启用)。
//
// bit0 = 用户刚把活动输入配置切换到 Fluxing。
//
// 为什么需要这个标志:ITfActiveLanguageProfileNotifySink::OnActivated 是
// **per-process** 订阅 —— 切换一次输入法,所有已加载 Fluxing TIP 的进程
// 各收到一次。所以「切换到 Fluxing」与「窗口获得焦点」(ITfKeyEventSink
// ::OnSetFocus → 同一个 FocusIn 消息)必须能被 server 端区分,否则
// QuickPanel 会在每次切窗口时弹出(spec 055 bug 2 的根因)。
enum {
  WEASEL_IPC_FOCUS_IN_FLAG_PROFILE_ACTIVATED = 1u << 0,
};

// WEASEL_IPC_FOCUS_OUT 的 wParam 位标志(spec 091 / ADR 0024)。
// 与上面的 PROFILE_ACTIVATED 完全对称:FocusIn 用「切到 Fluxing」把
// 「用户刚切换输入法」和「窗口获得焦点」区分开,FocusOut 用「切离 Fluxing」
// 做同一件事。
//
// bit0 = 用户刚把活动输入配置切离 Fluxing。
//
// 为什么需要这个标志:FocusOut 有两条来源 ——
//   (a) 切窗口 / 应用失焦:
//       ITfKeyEventSink::OnSetFocus(fForeground=false)
//   (b) 切到别的输入法:
//       ITfActiveLanguageProfileNotifySink::OnActivated(false)
// 过去两条都发 wParam=0,服务端无法区分,于是「切到别的输入法必须收起快捷
// 面板」被常驻守卫一并挡掉(spec 091 的根因)。
//
// 只复用既有 wParam 槽位,线格式(命令号 / 字段布局)零变化;旧客户端发 0
// 仍然是旧语义,安全降级。
enum {
  WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED = 1u << 0,
};

// spec 091 / ADR 0024:快捷面板在 FocusOut 时是否应收起。
//
// param 带 WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED
//   => 用户切到了别的输入法,Fluxing 已经不在场 => 必须收起(即便勾了常驻)。
// 否则
//   => 只是输入焦点离开 / 切窗口 / 应用失焦 => 尊重常驻开关。
//
// 生产代码与测试用同一份口径(宪法 R7),不要在别处再抄一遍。
inline bool ShouldHideQuickPanelOnFocusOut(DWORD param, bool always_show) {
  if (param & WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED)
    return true;
  return !always_show;
}

namespace weasel {
struct PipeMessage {
  WEASEL_IPC_COMMAND Msg;
  DWORD wParam;
  DWORD lParam;
};

struct IPCMetadata {
  enum { WINDOW_CLASS_LENGTH = 64 };
  UINT32 server_hwnd;
  WCHAR server_window_class[WINDOW_CLASS_LENGTH];
};

// 處理請求之物件
struct RequestHandler {
  using EatLine = std::function<bool(std::wstring&)>;
  RequestHandler() {}
  virtual ~RequestHandler() {}
  virtual void Initialize() {}
  virtual void Finalize() {}
  virtual DWORD FindSession(DWORD session_id) { return 0; }
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0) { return 0; }
  virtual DWORD RemoveSession(DWORD session_id) { return 0; }
  virtual BOOL ProcessKeyEvent(KeyEvent keyEvent,
                               DWORD session_id,
                               EatLine eat) {
    return FALSE;
  }
  virtual void CommitComposition(DWORD session_id) {}
  virtual void ClearComposition(DWORD session_id) {}
  virtual void SelectCandidateOnCurrentPage(size_t index, DWORD session_id) {}
  virtual void DeleteCandidateOnCurrentPage(size_t index, DWORD session_id) {}
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               DWORD session_id,
                                               EatLine eat) {
    return false;
  }
  // spec 077: Shift release-only candidate select via IPC state machine.
  // Virtuals are no-op defaults so ServerImpl (T08) builds before
  // RimeWithWeaselHandler overrides them (T09/T10).
  // ADR 0012: the Shift IPC carries a response body now. The server hands the
  // body to the client through the same EatLine channel that ChangePage and
  // HighlightCandidateOnCurrentPage use, so the client can read the fresh
  // status (and the commit text of the selected candidate) without a second
  // round trip. Without a body the client keeps the previous response in its
  // pipe buffer, because PipeChannel only refreshes that buffer when the
  // server actually writes one.
  virtual void ShiftDown(bool is_left, DWORD session_id, EatLine eat) {}
  virtual void ShiftUp(bool is_left, DWORD session_id, EatLine eat) {}
  virtual void SelectCandidate(size_t index, DWORD session_id) {}
  virtual bool ChangePage(bool backward, DWORD session_id, EatLine eat) {
    return false;
  }
  virtual void FocusIn(DWORD param, DWORD session_id) {}
  virtual void FocusOut(DWORD param, DWORD session_id) {}
  virtual void UpdateInputPosition(RECT const& rc, DWORD session_id) {}
  virtual void StartMaintenance() {}
  virtual void EndMaintenance() {}
  virtual void SetOption(DWORD session_id, const std::string& opt, bool val) {}
  virtual void UpdateColorTheme(BOOL darkMode) {}
};

// 處理server端回應之物件
typedef std::function<bool(LPWSTR buffer, DWORD length)> ResponseHandler;

// 事件處理函數
typedef std::function<bool()> CommandHandler;

// 啟動服務進程之物件
typedef CommandHandler ServerLauncher;

// IPC實現類聲明

class ClientImpl;
class ServerImpl;

// IPC接口類

class Client {
 public:
  Client();
  virtual ~Client();

  // 连接到服务，必要时启动服务进程
  bool Connect(ServerLauncher launcher = 0);
  // 断开连接
  void Disconnect();
  // 终止服务
  void ShutdownServer();
  // 發起會話
  void StartSession();
  // 結束會話
  void EndSession();
  // 進入維護模式
  void StartMaintenance();
  // 退出維護模式
  void EndMaintenance();
  // 测试连接
  bool Echo();
  // 请求服务处理按键消息
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  // 上屏正在編輯的文字
  bool CommitComposition();
  // 清除正在編輯的文字
  bool ClearComposition();
  // 选择当前页面编号为index的候选
  bool SelectCandidateOnCurrentPage(size_t index);
  // spec 077: notify Server of raw Shift_L/R down/up so Server-side state
  // machine can drive release-only select_candidate. is_left: true=Shift_L,
  // false=Shift_R. ADR 0012: the server answers with a response body now
  // (the fresh status and, for a selected candidate, the commit text); the
  // return value only reports whether the request was sent.
  bool ShiftDown(bool is_left);
  bool ShiftUp(bool is_left);
  // spec 077: bypass Server-side state machine and select candidate at
  // absolute 0-based index. Reserved for future scripted/test paths.
  bool SelectCandidate(size_t index);
  // 高亮当前页面编号为index的候选
  bool HighlightCandidateOnCurrentPage(size_t index);
  // 删除当前页面编号为index的候选（右键删除）
  bool DeleteCandidateOnCurrentPage(size_t index);
  // 翻页，backward = true 向前翻，false向后翻
  bool ChangePage(bool backward);
  // 更新输入位置
  void UpdateInputPosition(RECT const& rc);
  // 输入窗口获得焦点
  // client_caps: WEASEL_IPC_FOCUS_IN_FLAG_* 位标志(见文件上方定义)
  void FocusIn(DWORD client_caps = 0);
  // 输入窗口失去焦点
  // client_caps: WEASEL_IPC_FOCUS_OUT_FLAG_* 位标志(见文件上方定义)
  void FocusOut(DWORD client_caps = 0);
  // spec 080: close the pipe connection owned by the calling thread only.
  void CloseThreadChannel();
  // spec 080: true when this client owns a live session.
  bool HasSession() const;
  // 托盤菜單
  void TrayCommand(UINT menuId);
  // 读取server返回的数据
  bool GetResponseData(ResponseHandler handler);

 private:
  ClientImpl* m_pImpl;
};

class Server {
 public:
  Server();
  virtual ~Server();

  // 初始化服务
  HWND Start();
  // 结束服务
  int Stop();
  // 消息循环
  int Run();

  void SetRequestHandler(RequestHandler* pHandler);
  void AddMenuHandler(UINT uID, CommandHandler handler);
  HWND GetHWnd();

 private:
  ServerImpl* m_pImpl;
};

inline std::wstring GetPipeName() {
  std::wstring pipe_name;
  pipe_name += L"\\\\.\\pipe\\";
  pipe_name += getUsername();
  pipe_name += L"\\";
  pipe_name += WEASEL_IPC_PIPE_NAME;
  return pipe_name;
}
}  // namespace weasel
