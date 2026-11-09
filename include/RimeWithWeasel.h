#pragma once
#include <WeaselIPC.h>
#include <WeaselUI.h>
#include <map>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>
// spec 090: 候选词屏蔽名单(std::unordered_set)与页内索引映射(std::vector)。
#include <unordered_set>
#include <vector>

#include <rime_api.h>

// spec 090 / ADR 0023: 屏蔽名单读写、可见->引擎索引映射与名单文件指纹
// (fluxing::IgnoreListStamp)。header-only 纯函数,服务端与单元测试工程共用。
#include <FluxingCandidateIgnore.h>
// spec 099 P0-3: 「下一个输入方案」的纯函数(header-only,服务端与测试共用)。
#include <FluxingSchemaCycle.h>

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
  // spec 094: 加上 EatLine —— 删除候选词后必须回 body(commit/status/ctx/
  // 候选列表),客户端才能立刻刷新候选窗;否则它拿到的还是上一次的管道缓冲。
  void DeleteCandidateOnCurrentPage(size_t index,
                                    WeaselSessionId ipc_id,
                                    EatLine eat = 0) override;
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
  // spec 045 修(2026-11-09):不再返回「全局单值」,改为**按目标会话 live 读**。
  //
  // ★ 缺陷背景(用户报告「切换到搜狗双拼,仍然是全拼输入」):旧实现返回
  //   m_current_schema_id —— 该值会被 AddSession 用**新会话的默认方案**
  //   (rime_ice 全拼)覆盖,于是「用户已切到搜狗双拼」也会被面板显示成全拼,
  //   且「下一个方案」的起点算错(可能算出的就是当前方案 ⇒ 看起来没切换)。
  //
  // 口径:目标会话(显式 ipc_id → 活动 → 最近焦点 → 唯一会话)的 live
  // schema_id;读不到才回落到 m_current_schema_id 镜像。
  std::string GetCurrentSchemaId();
  std::vector<std::string> GetAvailableSchemas() const {
    return m_available_schemas;
  }
  void SelectSchema(const std::string& schema_id);
  // spec 099 P0-3: 快捷面板槽 0 = 「切换到下一个输入方案」。
  // 与 SelectSchema 的差别:作用于**活动会话**(当前正在输入的窗口立即
  // 生效),而不是 create_session() 造出来的新会话。
  void SwitchToNextSchema();
  // Read the cached 简/繁 / 全/半角 option bools.
  bool IsSimplification() const;
  bool IsFullShape() const;
  // v6 需求②: 快捷面板要的「**生效**输入态」。
  //
  // 面板的全半角图标必须随中英文切换变化,而「此刻真正打出的标点宽度」是
  // full_shape && !ascii_mode —— 英文态下 rime 的 punctuator 走 ascii 分支,
  // 即使 full_shape 为 true 也打半角。所以**不能**把 IsFullShape() 直接丢给
  // 面板(那正是「切到英文,图标仍是全角」这个缺陷的根因)。
  //
  // 实现要点(勿简化):
  //   - 内部**不调 IsFullShape()**:它首次调用要 user_config_open(文件 I/O),
  //     而本函数由显示路径每 100ms 调一次 ⇒ 违反 constitution P2;
  //   - 只有活动会话确实登记在 m_session_status_map 里才调 get_status(进程内
  //     C API 读内存,与 ProcessKeyEvent 热路径同形态);
  //   - 非 const:校正镜像需要写 m_global_ascii_mode / m_full_shape。
  void GetPanelInputState(bool* ascii, bool* fullwidth);

  // 批次 H(用户需求②):设置栏「全/半角」⇒ **英文标点** 的落点。
  //
  // ★ 为什么不是 full_shape:本 fork 的 output/data/default.yaml 里
  //   punctuator/full_shape 与 punctuator/half_shape **两张表都把 , . ? ; : ( [
  //   { 映射成中文标点**(上游 librime 也是这个约定),所以「切 full_shape」在
  //   中文标点语义下切不出任何差别 —— 这正是用户说的「点了半角,输入的仍然是
  //   中文标点」。
  // ★ 为什么是 ascii_punct:librime 的 punctuator 在 ascii_punct=true 时
  //   直接 `return kNoop`(librime/src/rime/gear/punctuator.cc),标点**原样
  //   直出 ASCII**。这是既有开关里唯一能真正产出英文标点的那个。
  // 口径:halfwidth=true ⇒ ascii_punct=true ⇒ 英文标点;
  //       halfwidth=false ⇒ ascii_punct=false ⇒ 回到中文标点。
  void SetPunctuationWidth(bool halfwidth, WeaselSessionId ipc_id = 0);

  void OnUpdateUI(std::function<void()> const& cb);

  // spec 080: called when the user switches the active input profile to
  // Fluxing and this session is the server's active session. The owner
  // (WeaselServerApp) marshals the request to the server UI thread,
  // because QuickPanelDialog owns a window, GDI resources and a polling
  // timer and must be driven from that thread.
  void SetOnShowQuickPanel(std::function<void()> const& cb) {
    m_onShowQuickPanel = cb;
  }
  // spec 097: 隐藏快捷面板也必须走 UI 线程队列。
  // 旧代码在服务端 IPC 线程里直接调 QuickPanelDialog::Hide(),而显示走的是
  // PostMessage(UI 线程队列) —— 两条不同线程的路径没有共同的定序点,于是
  // 「先显示后隐藏」会被 UI 线程在隐藏之后才执行的那次显示覆盖掉,面板留在
  // 屏幕上不消失(spec 097 bug 的服务端一半根因)。
  void SetOnHideQuickPanel(std::function<void()> const& cb) {
    m_onHideQuickPanel = cb;
  }
  // spec 097: 服务端当前记录的「火流猩是否在场」。UI 线程执行显示之前
  // 复核它:已经切走(kAbsent)就作废本次显示。
  FluxingPresence GetFluxingPresence() const {
    return m_fluxing_presence.load(std::memory_order_relaxed);
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
  // ADR 0032: 读出本次会话的客户端身份(小写 exe 名),供「自家 UI 进程」判定。
  std::string _ReadClientInfo(WeaselSessionId ipc_id, LPWSTR buffer);
  // ADR 0032: 记录 / 查询 IPC 会话的客户端归属。
  // 身份必须活过 RemoveSession(TSF 的 Deactivate 先 EndSession 再发 FocusOut),
  // 因此 RemoveSession 不删这张表;ipc_id 会被回收复用,所以 AddSession 先清同 id 旧值。
  void _RememberClientKind(WeaselSessionId ipc_id, fluxing::IpcClientKind kind);
  fluxing::IpcClientKind _ClientKind(WeaselSessionId ipc_id) const;
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
  // 批次 H:解析「这次操作该作用到哪个 rime 会话」。
  //
  // 顺序:显式 ipc_id → 活动会话(m_active_session)→ **最近一次获得焦点的
  // 会话**(m_last_focus_session)→ 唯一会话。
  //
  // ★ 第三档是批次 H 新增的,直接对应用户需求③的症状
  //   「Shift 切中英文,设置栏的中/英与全/半角都不切换」:
  //   面板的 100ms 轮询与用户的点击都可能发生在 m_active_session 已被
  //   FocusOut 清零之后,没有这一档就永远读不到 **live** 状态,只能回落到
  //   陈旧镜像 ⇒ 图标看起来「按了没反应」。
  //
  // ★ 全程用 find() 读,**不用 to_session_id()** —— 它的 operator[] 会在键
  //   不存在时**插入**一个假会话,把「只有一个会话」这个事实破坏掉
  //   (SetOption 的唯一会话回退就靠它)。
  RimeSessionId _ResolveTargetSession(WeaselSessionId ipc_id);
  // spec 045 修(2026-11-09):读某个 rime 会话当前的 schema_id(内存态,无 I/O)。
  std::string _LiveSchemaId(RimeSessionId session_id);
  // spec 045 修(2026-11-09):切换输入方案时的用户可见反馈(中文提示)。
  void _NotifySchemaSwitch(const std::wstring& text);
  // 批次 H:把「标点宽度」落到某个会话,并同步服务端镜像。
  // halfwidth=true ⇒ ascii_punct=true(英文标点)且 full_shape=false;
  // halfwidth=false ⇒ ascii_punct=false(中文标点)且 full_shape=true。
  // 两个 option **同向**拨,避免引擎里留下两套互相矛盾的状态。
  void _ApplyPunctWidth(RimeSessionId session_id, bool halfwidth);
  SessionStatus& get_session_status(WeaselSessionId ipc_id) {
    return m_session_status_map[ipc_id];
  }
  SessionStatus& new_session_status(WeaselSessionId ipc_id) {
    return m_session_status_map[ipc_id] = SessionStatus();
  }

  AppOptionsByAppName m_app_options;
  weasel::UI* m_ui;  // reference
  DWORD m_active_session;
  // spec 097: 「火流猩是否在场」的服务端状态。IPC 线程写、UI 线程读,
  // 必须是无锁原子(宪法 P2:不得在 TSF/IO 路径上加锁)。
  std::atomic<FluxingPresence> m_fluxing_presence;
  // spec 045: cached scheme + option bools (refreshed by _GetStatus).
  std::string m_current_schema_id;
  // spec 045 修(2026-11-09):per-session 的当前方案(WeaselSessionId -> schema id)。
  //
  // 为什么需要:「用户选中的方案」在 librime 里是**每个会话各自持有**的,而
  // 新建会话的默认方案 = schema_list 第一项(rime_ice 全拼)。用户显式选过
  // 搜狗双拼之后,新窗口 / 新会话又回到全拼 ⇒ 「切了双拼还是全拼输入」。
  //
  // 这张表记录「每个会话实际在用哪个方案」,让新会话能继承用户的选择,
  // 也让面板显示的方案名与实际输入会话一致。
  std::map<WeaselSessionId, std::string> m_session_schema_ids;
  std::vector<std::string> m_available_schemas;
  mutable bool m_simplification = false;
  mutable bool m_full_shape = false;
  // 批次 H:ascii_punct 运行时镜像(true = 英文标点 = 面板上的「半角」)。
  // 与 m_full_shape 不同向:面板显示的是「**生效**标点是不是英文」,
  // 判据是 !ascii_mode && !ascii_punct(英文态本身就打 ASCII 标点)。
  mutable bool m_ascii_punct = false;
  // 批次 H:最近一次获得输入焦点的会话(WeaselSessionId)。
  // FocusOut **不清**它 —— 面板轮询要靠它读到 live 的 ascii_mode /
  // ascii_punct;只有 RemoveSession(会话真的没了)才清。
  WeaselSessionId m_last_focus_session = 0;
  mutable bool m_cached_options = false;
  bool m_disabled;
  std::string m_last_schema_id;
  std::string m_last_app_name;
  weasel::UIStyle m_base_style;
  std::map<std::string, bool> m_show_notifications;
  std::map<std::string, bool> m_show_notifications_base;
  std::function<void()> _UpdateUICallback;
  std::function<void()> m_onShowQuickPanel;
  // spec 097: 隐藏回调。与 m_onShowQuickPanel 成对,由服务端注入
  // (PostMessage 到 UI 线程),保证 show / hide 在同一线程排队。未注入时
  // FocusOut 直接回退到 QuickPanelDialog::Hide()。
  std::function<void()> m_onHideQuickPanel;

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
  // ADR 0032: IPC 会话 -> 客户端归属。身份必须活过 RemoveSession(FocusOut 在
  // 其后到达),所以不随会话结束删除;用容量上限(kMaxRememberedClientKinds)
  // 防无界增长。
  std::map<WeaselSessionId, fluxing::IpcClientKind> m_session_client_kinds;
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
