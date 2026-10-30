#pragma once
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <map>
#include <string>
#include <mutex>
#include <unordered_map>
// spec 090: 候选词屏蔽名单(std::unordered_set)与页内索引映射(std::vector)。
#include <unordered_set>
#include <vector>

#include <rime_api.h>

// spec 090 / ADR 0023: 屏蔽名单读写、可见->引擎索引映射与名单文件指纹
// (fluxing::IgnoreListStamp)。header-only 纯函数,服务端与单元测试工程共用。
#include <FluxingCandidateIgnore.h>

struct CaseInsensitiveCompare {
  bool operator()(const std::string& str1, const std::string& str2) const {
    std::string str1Lower, str2Lower;
    std::transform(str1.begin(), str1.end(), std::back_inserter(str1Lower),
                   [](char c) { return std::tolower(c); });
    std::transform(str2.begin(), str2.end(), std::back_inserter(str2Lower),
                   [](char c) { return std::tolower(c); });
    return str1Lower < str2Lower;
  }
};

typedef std::map<std::string, bool> AppOptions;
typedef std::map<std::string, AppOptions, CaseInsensitiveCompare>
    AppOptionsByAppName;

struct SessionStatus {
  SessionStatus() : style(weasel::UIStyle()), __synced(false), session_id(0) {
    RIME_STRUCT(RimeStatus, status);
  }
  weasel::UIStyle style;
  RimeStatus status;
  bool __synced;
  RimeSessionId session_id;
};
typedef std::map<DWORD, SessionStatus> SessionStatusMap;
typedef DWORD WeaselSessionId;

// spec 077: per-session Shift state for release-only candidate select via IPC.
// Lives on the Server side (RimeWithWeaselHandler) so the state machine is
// driven by Server-serialized events, not TSF thread-local. Mirrors the
// pure fluxing::ShiftStateMachine but is keyed by WeaselSessionId and is
// responsible for actually calling rime_api->select_candidate when fire.
struct ShiftState {
  bool downRecorded = false;
  bool interveningKey = false;
  bool lastIsLeft = false;
};
class RimeWithWeaselHandler : public weasel::RequestHandler {
 public:
  RimeWithWeaselHandler(weasel::UI* ui);
  virtual ~RimeWithWeaselHandler();
  virtual void Initialize();
  virtual void Finalize();
  virtual DWORD FindSession(WeaselSessionId ipc_id);
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0);
  virtual DWORD RemoveSession(WeaselSessionId ipc_id);
  virtual BOOL ProcessKeyEvent(weasel::KeyEvent keyEvent,
                               WeaselSessionId ipc_id,
                               EatLine eat);
  virtual void CommitComposition(WeaselSessionId ipc_id);
  virtual void ClearComposition(WeaselSessionId ipc_id);
  virtual void SelectCandidateOnCurrentPage(size_t index,
                                            WeaselSessionId ipc_id);
  // spec 028: delete candidate on current page (librime 1.13 delete_candidate
  // API)
  void DeleteCandidateOnCurrentPage(size_t index,
                                    WeaselSessionId ipc_id) override;
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               WeaselSessionId ipc_id,
                                               EatLine eat);
  virtual bool ChangePage(bool backward, WeaselSessionId ipc_id, EatLine eat);
  virtual void FocusIn(DWORD param, WeaselSessionId ipc_id);
  virtual void FocusOut(DWORD param, WeaselSessionId ipc_id);
  virtual void UpdateInputPosition(RECT const& rc, WeaselSessionId ipc_id);
  virtual void StartMaintenance();
  virtual void EndMaintenance();
  virtual void SetOption(WeaselSessionId ipc_id,
                         const std::string& opt,
                         bool val);
  virtual void UpdateColorTheme(BOOL darkMode);

  // spec 077: Shift release-only candidate select via IPC state machine.
  // Override RequestHandler virtuals; state is kept in m_shiftState.
  virtual void ShiftDown(bool is_left, WeaselSessionId ipc_id, EatLine eat);
  virtual void ShiftUp(bool is_left, WeaselSessionId ipc_id, EatLine eat);
  virtual void SelectCandidate(size_t index, WeaselSessionId ipc_id);

  // spec 036: return current global ASCII mode (for QuickPanel initial state).
  // Reads the cached m_global_ascii_mode which is updated by SetOption.
  // ADR 0012: this is the runtime mirror only; the propagation decision uses
  // the separate m_global_ascii_configured flag.
  bool IsAsciiMode() const { return m_global_ascii_mode; }
  // spec 045 v0.18.29.0: read current scheme id + list available
  // schemes + select scheme (for QuickPanel Row 3 schema label +
  // 切换 button). The handler caches m_current_schema_id updated by
  // _GetStatus on each IPC roundtrip; this avoids needing to call
  // rime_api directly from the QP dialog.
  std::string GetCurrentSchemaId() const { return m_current_schema_id; }
  std::vector<std::string> GetAvailableSchemas() const {
    return m_available_schemas;
  }
  void SelectSchema(const std::string& schema_id);
  // Read the cached 简/繁 / 全/半角 option bools.
  bool IsSimplification() const;
  bool IsFullShape() const;

  void OnUpdateUI(std::function<void()> const& cb);

  // spec 080: called when the user switches the active input profile to
  // Fluxing and this session is the server's active session. The owner
  // (WeaselServerApp) marshals the request to the server UI thread,
  // because QuickPanelDialog owns a window, GDI resources and a polling
  // timer and must be driven from that thread.
  void SetOnShowQuickPanel(std::function<void()> const& cb) {
    m_onShowQuickPanel = cb;
  }

 private:
  void _Setup();
  bool _IsDeployerRunning();
  // spec 053 R6 fix (L55): if m_disabled is true but the deployer
  // is actually gone (taskkill /F, AV quarantine, AV-induced crash),
  // the WeaselDeployerMutex stays held by the kernel with no live
  // holder. TryLazyRecovery re-checks the deployer state and, if the
  // deployer has died, re-runs Initialize() to clear m_disabled.
  void TryLazyRecovery();
  void _UpdateUI(WeaselSessionId ipc_id);
  void _LoadSchemaSpecificSettings(WeaselSessionId ipc_id,
                                   const std::string& schema_id);
  void _LoadAppInlinePreeditSet(WeaselSessionId ipc_id,
                                bool ignore_app_name = false);
  bool _ShowMessage(weasel::Context& ctx, weasel::Status& status);
  bool _Respond(WeaselSessionId ipc_id, EatLine eat);
  void _ReadClientInfo(WeaselSessionId ipc_id, LPWSTR buffer);
  // spec 090: 入参多带一个会话标识,用于按当前方案加载对应的屏蔽名单,并在
  // 同一次遍历里产出「可见页内索引 -> 引擎页内索引」映射(FR-005 / FR-006)。
  void _GetCandidateInfo(weasel::CandidateInfo& cinfo,
                         RimeContext& ctx,
                         RimeSessionId session_id);
  void _RefreshSchemaList();
  void _GetStatus(weasel::Status& stat,
                  WeaselSessionId ipc_id,
                  weasel::Context& ctx);
  void _GetContext(weasel::Context& ctx, RimeSessionId session_id);
  void _UpdateShowNotifications(RimeConfig* config, bool initialize = false);

  void _UpdateInlinePreeditStatus(WeaselSessionId ipc_id);

  // ===== spec 090: 候选词屏蔽名单 =====
  // 每次要出候选列表时刷新名单。判据用「方案 id 变了 或 名单文件变了(mtime /
  // 大小 / 消失)」,这样托盘「恢复已忽略的候选项」把 *.user_ignore.txt 删掉之后
  // 不需要重启服务端、也不需要切方案就能立刻生效(T7)。
  void _RefreshIgnoreList(RimeSessionId session_id);
  // 把客户端 / UI 传来的「可见页内索引」翻译成「引擎页内索引」。
  // 返回 false 表示越界 —— 调用方必须什么都不做(不选词、不上屏、不关窗)。
  // 名单为空(即没有过滤发生)时返回 true 并原样透传,保证未启用屏蔽的场景
  // 行为与改动前逐字节一致。
  bool _MapVisibleIndexToEngine(size_t visible_index, size_t* engine_index);
  std::wstring _CurrentSchemaIdW(RimeSessionId session_id);

  RimeSessionId to_session_id(WeaselSessionId ipc_id) {
    return m_session_status_map[ipc_id].session_id;
  }
  SessionStatus& get_session_status(WeaselSessionId ipc_id) {
    return m_session_status_map[ipc_id];
  }
  SessionStatus& new_session_status(WeaselSessionId ipc_id) {
    return m_session_status_map[ipc_id] = SessionStatus();
  }

  AppOptionsByAppName m_app_options;
  weasel::UI* m_ui;  // reference
  DWORD m_active_session;
  // spec 045: cached scheme + option bools (refreshed by _GetStatus).
  std::string m_current_schema_id;
  std::vector<std::string> m_available_schemas;
  mutable bool m_simplification = false;
  mutable bool m_full_shape = false;
  mutable bool m_cached_options = false;
  bool m_disabled;
  std::string m_last_schema_id;
  std::string m_last_app_name;
  weasel::UIStyle m_base_style;
  std::map<std::string, bool> m_show_notifications;
  std::map<std::string, bool> m_show_notifications_base;
  std::function<void()> _UpdateUICallback;
  std::function<void()> m_onShowQuickPanel;

  static void OnNotify(void* context_object,
                       uintptr_t session_id,
                       const char* message_type,
                       const char* message_value);
  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;
  static std::mutex m_notifier_mutex;
  SessionStatusMap m_session_status_map;
  // spec 077: per-session Shift state machine for release-only select.
  std::unordered_map<WeaselSessionId, ShiftState> m_shiftState;
  bool m_current_dark_mode;
  bool m_global_ascii_mode;
  // ADR 0012: the immutable configuration flag (weasel.yaml "global_ascii").
  // m_global_ascii_mode is only the runtime mirror that the QuickPanel reads;
  // the two must not be the same variable, otherwise a bare Shift in one
  // window silently switches every other window.
  bool m_global_ascii_configured;
  // ADR 0010: bare Shift toggles the Chinese / English input mode when no
  // candidate menu is open. On by default (P1); the switch lives in
  // weasel.yaml under [global settings].
  bool m_bare_shift_toggles_ascii;
  // ADR 0017: the host may report the two shift keys the other way round
  // (spec 087 A.7). The switch lives in weasel.yaml under [global settings]
  // and flips the side the state machine works with.
  bool m_shift_left_right_swap;
  // ===== spec 090 / ADR 0023: 候选词屏蔽名单运行时状态 =====
  // 「可见页内索引 -> 引擎页内索引」映射,由 _GetCandidateInfo 在过滤候选的
  // 同一次遍历里产出(FR-006:两者必须同源,否则页内索引与引擎错位)。
  // 名单为空时 _MapVisibleIndexToEngine 不查它、直接透传,未启用屏蔽的场景
  // 行为与改动前一致。
  std::vector<size_t> m_visibleToEngineIdx;
  // 已加载的屏蔽名单(候选原文集合,spec 090 FR-005)。
  std::unordered_set<std::wstring> m_ignoreList;
  // 已加载名单对应的方案 id;切方案时强制重读。
  std::wstring m_ignoreSchemaId;
  // 已加载名单文件的指纹(exists/mtime/size)。文件被外部修改或删除(托盘
  // 「恢复已忽略的候选项」直接 DeleteFileW)后,下一次出候选列表即重读,
  // 不需要重启服务端、也不需要切方案。
  fluxing::IgnoreListStamp m_ignoreStamp;
  int m_show_notifications_time;
  DWORD m_pid;
};
