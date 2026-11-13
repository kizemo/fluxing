#include "stdafx.h"
#include <shellapi.h>  // L70-bugfix: FocusIn lambda uses ShellExecuteW
#include <logging.h>
#include <RimeWithWeasel.h>
#include <StringAlgorithm.hpp>
#include <WeaselConstants.h>
#include <WeaselUtility.h>
#include <QuickPanelDialog.h>  // spec 056: FocusOut calls
                               //   QuickPanelDialog::Hide
                               // spec 080: the profile-activated branch
                               //   asks WeaselServerApp to show it
// v0.19.0.52 (Phase K3 T011 Option B): capture user foreground at FocusIn
//   IPC for PhrasesDialogIPC::InjectText to restore before SendInput.
//   GetForegroundWindow() 是 session-global (跟 caller 进程无关),所以
//   IPC worker thread 调 = 拿 user app 当前 foreground (= 最稳的 capture 点)。
#include "ForegroundCapture.h"
// spec 100 阶段 F · T501: 本地词典改动通知槽（进程内直连，不新增 IPC 消息）。
//   提交边界在此通知云同步引擎：只做「置标志 + 唤醒」，
//   不阻塞输入响应线程（宪法 P2）。
#include <FluxingLocalChangeHook.h>
// spec 097: 服务端侧的显隐链路诊断。写盘发生在 IPC 线程,不在 UI 线程。
#include <FluxingDiagLog.h>

#include <filesystem>
#include <map>
#include <array>
#include <vector>
#include <regex>
#include <rime_api.h>
#include <ShiftStateMachine.h>  // ADR 0010: bare Shift disambiguation policy
// spec 090 / ADR 0023: 候选词屏蔽名单(读写 + 可见->引擎索引映射)。
#include <FluxingCandidateIgnore.h>
// 批次 H: 面板「全/半角」的显示口径纯函数(单一事实源,constitution R7)。
#include <FluxingPanelStatus.h>
// 批次 I / ADR 0043: 半角标点(英文标点)状态下的标点语言转换(纯函数,单一事实源)。
#include <FluxingPunctText.h>
// ADR 0062: 维护准入闸门的判据纯函数（**只含 ASCII**，原因见头文件注释）。
#include <FluxingMaintenanceGate.h>
#include <fstream>

// ★ spec 045 修(2026-11-09 · 第三轮):用户显式选定的输入方案必须**持久化**。
//
// 缺陷背景(用户两次报告,第二次是「输入法进行了重启,但仍然只能使用全拼输入」):
//   m_current_schema_id 只活在进程内存里,WeaselServer 一重启就空,于是第一笔
//   AddSession 又把全局镜像设成 librime 的默认方案(schema_list 首项,本机是
//   rime_ice 全拼)。ADR 0048 的第 ⑤ 条因此落不了地 —— 没有持久化就没有
//   「用户显式选方案」的跨进程语义。
//
// 存哪儿:用户数据目录下的单行纯文本,不碰任何 Rime 配置的解析/序列化路径
//   (绝不与 librime switcher 的 var/previously_selected_schema 打架 —— 那是
//   librime 的私有状态,只用于它自己的菜单排序)。
namespace {

const char kPreferredSchemaFile[] = "fluxing.preferred_schema.txt";

std::filesystem::path _PreferredSchemaPath() {
  const std::filesystem::path dir = WeaselUserDataPath();
  if (dir.empty())
    return std::filesystem::path();
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);  // 已存在时无副作用
  return dir / kPreferredSchemaFile;
}

// 返回空串 = 从未显式选过方案(调用方必须保持旧行为,不许猜)。
std::string _LoadPreferredSchemaFromDisk() {
  const std::filesystem::path path = _PreferredSchemaPath();
  if (path.empty())
    return std::string();
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec)
    return std::string();
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return std::string();
  std::string id;
  std::getline(in, id);
  while (!id.empty() && (id.back() == '\r' || id.back() == '\n' ||
                         id.back() == ' ' || id.back() == '\t'))
    id.pop_back();
  return id;
}

void _SavePreferredSchemaToDisk(const std::string& schema_id) {
  if (schema_id.empty())
    return;
  const std::filesystem::path path = _PreferredSchemaPath();
  if (path.empty())
    return;
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out)
    return;
  out << schema_id << "\n";
}

}  // namespace

#define TRANSPARENT_COLOR 0x00000000
#define ARGB2ABGR(value)                                 \
  ((value & 0xff000000) | ((value & 0x000000ff) << 16) | \
   (value & 0x0000ff00) | ((value & 0x00ff0000) >> 16))
#define RGBA2ABGR(value)                                   \
  (((value & 0xff) << 24) | ((value & 0xff000000) >> 24) | \
   ((value & 0x00ff0000) >> 8) | ((value & 0x0000ff00) << 8))
typedef enum { COLOR_ABGR = 0, COLOR_ARGB, COLOR_RGBA } ColorFormat;

using namespace weasel;

static RimeApi* rime_api;
WeaselSessionId _GenerateNewWeaselSessionId(SessionStatusMap sm, DWORD pid) {
  if (sm.empty())
    return (WeaselSessionId)(pid + 1);
  return (WeaselSessionId)(sm.rbegin()->first + 1);
}

int expand_ibus_modifier(int m) {
  return (m & 0xff) | ((m & 0xff00) << 16);
}

RimeWithWeaselHandler::RimeWithWeaselHandler(UI* ui)
    : m_ui(ui),
      m_active_session(0),
      // spec 097: 服务端记录「火流猩是否在场」。初始未知 => 允许显示。
      m_fluxing_presence(FluxingPresence::kUnknown),
      m_disabled(true),
      m_current_dark_mode(false),
      m_global_ascii_mode(false),
      m_global_ascii_configured(false),
      // ADR 0010 P1: bare Shift toggles Chinese / English by default.
      m_bare_shift_toggles_ascii(true),
       m_shift_left_right_swap(false),
      m_show_notifications_time(1200),
      _UpdateUICallback(NULL) {
  m_ui->InServer() = true;
  rime_api = rime_get_api();
  assert(rime_api);
  m_pid = GetCurrentProcessId();
  uint16_t msbit = 0;
  for (auto i = 31; i >= 0; i--) {
    if (m_pid & (1 << i)) {
      msbit = i;
      break;
    }
  }
  m_pid = (m_pid << (31 - msbit));
  _Setup();
}

RimeWithWeaselHandler::~RimeWithWeaselHandler() {
  m_show_notifications.clear();
  m_session_status_map.clear();
  // ADR 0062: 会话表清空 => 活跃会话数归零。
  m_live_session_count.store(0, std::memory_order_relaxed);
  m_app_options.clear();
}

bool add_session = false;
void _UpdateUIStyle(RimeConfig* config, UI* ui, bool initialize);
bool _UpdateUIStyleColor(RimeConfig* config,
                         UIStyle& style,
                         const std::string& color = std::string());
void _LoadAppOptions(RimeConfig* config, AppOptionsByAppName& app_options);

void _RefreshTrayIcon(const RimeSessionId session_id,
                      const std::function<void()> _UpdateUICallback) {
  // Dangerous, don't touch
  static char app_name[256] = {0};
  auto ret = rime_api->get_property(session_id, "client_app", app_name,
                                    sizeof(app_name) - 1);
  if (!ret || u8tow(app_name) == std::wstring(L"explorer.exe"))
    boost::thread th([=]() {
      ::Sleep(100);
      if (_UpdateUICallback)
        _UpdateUICallback();
    });
  else if (_UpdateUICallback)
    _UpdateUICallback();
}

void RimeWithWeaselHandler::_Setup() {
  RIME_STRUCT(RimeTraits, weasel_traits);
  std::string shared_dir = wtou8(WeaselSharedDataPath().wstring());
  std::string user_dir = wtou8(WeaselUserDataPath().wstring());
  weasel_traits.shared_data_dir = shared_dir.c_str();
  weasel_traits.user_data_dir = user_dir.c_str();
  weasel_traits.prebuilt_data_dir = weasel_traits.shared_data_dir;
  std::string distribution_name = wtou8(get_weasel_ime_name());
  weasel_traits.distribution_name = distribution_name.c_str();
  weasel_traits.distribution_code_name = WEASEL_CODE_NAME;
  weasel_traits.distribution_version = FLUXING_VERSION;
  weasel_traits.app_name = "rime.weasel";
  std::string log_dir = WeaselLogPath().u8string();
  weasel_traits.log_dir = log_dir.c_str();
  rime_api->setup(&weasel_traits);
  rime_api->set_notification_handler(&RimeWithWeaselHandler::OnNotify, this);
}

void RimeWithWeaselHandler::Initialize() {
  m_disabled = _IsDeployerRunning();
  if (m_disabled) {
    return;
  }

  LOG(INFO) << "Initializing la rime.";
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(/*full_check = */ False)) {
    m_disabled = true;
    rime_api->join_maintenance_thread();
  }

  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    if (m_ui) {
      _UpdateUIStyle(&config, m_ui, true);
      _UpdateShowNotifications(&config, true);
      m_current_dark_mode = IsUserDarkMode();
      if (m_current_dark_mode) {
        const int BUF_SIZE = 255;
        char buffer[BUF_SIZE + 1] = {0};
        if (rime_api->config_get_string(&config, "style/color_scheme_dark",
                                        buffer, BUF_SIZE)) {
          std::string color_name(buffer);
          _UpdateUIStyleColor(&config, m_ui->style(), color_name);
        }
      }
      m_base_style = m_ui->style();
    }
    // ADR 0012: the yaml flag and the runtime state are two different things.
    // m_global_ascii_configured is the immutable configuration flag
    // ("global_ascii" in weasel.yaml); m_global_ascii_mode is the runtime
    // mirror the QuickPanel reads. Mixing them made a bare Shift in one
    // window switch every other window.
    Bool global_ascii = false;
    m_global_ascii_configured = false;
    if (rime_api->config_get_bool(&config, "global_ascii", &global_ascii)) {
      m_global_ascii_configured = !!global_ascii;
      m_global_ascii_mode = !!global_ascii;
    }
    // ADR 0010: bare Shift -> Chinese / English toggle. An absent key keeps
    // the constructor default (on), so an upgraded install gets the feature
    // without having to edit weasel.yaml.
    Bool bare_shift_toggles_ascii = 0;
    if (rime_api->config_get_bool(&config, "bare_shift_toggles_ascii",
                                  &bare_shift_toggles_ascii))
      m_bare_shift_toggles_ascii = !!bare_shift_toggles_ascii;
    // ADR 0017: the host may hand the IME the two shift keys the other way
    // round. An absent key keeps the default (no swap), so an upgraded
    // install behaves exactly like the previous one.
    Bool shift_left_right_swap = 0;
    if (rime_api->config_get_bool(&config, "shift_left_right_swap",
                                  &shift_left_right_swap))
      m_shift_left_right_swap = !!shift_left_right_swap;
    if (!rime_api->config_get_int(&config, "show_notifications_time",
                                  &m_show_notifications_time))
      m_show_notifications_time = 1200;
    _LoadAppOptions(&config, m_app_options);
    rime_api->config_close(&config);
  }
  m_last_schema_id.clear();
}

void RimeWithWeaselHandler::Finalize() {
  m_active_session = 0;
  m_disabled = true;
  m_session_status_map.clear();
  // ADR 0062: 会话表清空 => 没有活跃会话 => 闸门放行。
  m_live_session_count.store(0, std::memory_order_relaxed);
  LOG(INFO) << "Finalizing la rime.";
  rime_api->finalize();
}

// ADR 0062: 重算「活跃输入会话数」。
//
// 活跃 = 会话表里存在 session_id != 0 的条目。
// ★ 必须显式排除 session_id == 0：to_session_id() 用 operator[] 访问会话表，
//   键不存在时会**插入**一个空 status（session_id = 0）=> 那些是假条目；
//   若不排除，一个从没存在过的 ipc_id 就能把云同步永久堵死。
//
// 只在会话生命周期事件（增 / 删 / 清表）上调用 —— 都是低频事件。
void RimeWithWeaselHandler::_RefreshLiveSessionCount() {
  int live = 0;
  for (const auto& pair : m_session_status_map) {
    if (pair.second.session_id != 0)
      ++live;
  }
  m_live_session_count.store(live, std::memory_order_relaxed);
}

// ADR 0062 决策 1: 维护准入闸门。
//
// 只读原子量、无 I/O、不加锁 => 可从云同步的后台线程安全调用（宪法 P2）。
bool RimeWithWeaselHandler::CanEnterMaintenance(std::string* reason) {
  const int live = m_live_session_count.load(std::memory_order_relaxed);
  if (fluxing::maintenance_gate::MaintenanceGateAllows(live)) {
    if (reason != nullptr)
      reason->clear();
    return true;
  }
  if (reason != nullptr)
    *reason = fluxing::maintenance_gate::MaintenanceGateReasonLiveSession();
  return false;
}

DWORD RimeWithWeaselHandler::FindSession(WeaselSessionId ipc_id) {
  if (m_disabled)
    return 0;
  Bool found = rime_api->find_session(to_session_id(ipc_id));
  DLOG(INFO) << "Find session: session_id = " << to_session_id(ipc_id)
             << ", found = " << found;
  return found ? (ipc_id) : 0;
}

DWORD RimeWithWeaselHandler::AddSession(LPWSTR buffer, EatLine eat) {
  // spec 053 R6 fix: legacy code called EndMaintenance() here, but
  // EndMaintenance only re-Initializes if deployer is *gone* - it did
  // not detect the abandoned-mutex case (taskkill /F / AV). TryLazyRecovery
  // explicitly re-checks _IsDeployerRunning() (now with WAIT_ABANDONED
  // detection) and recovers the user without requiring a reboot.
  TryLazyRecovery();
  if (m_disabled) {
    DLOG(INFO) << "AddSession: m_disabled still true after TryLazyRecovery";
    return 0;
  }
  RimeSessionId session_id = (RimeSessionId)rime_api->create_session();
  // ADR 0012: a new session only inherits the ASCII mode of an existing
  // session when weasel.yaml asks for the global behaviour.
  if (m_global_ascii_configured) {
    for (const auto& pair : m_session_status_map) {
      if (pair.first) {
        rime_api->set_option(session_id, "ascii_mode",
                             !!pair.second.status.is_ascii_mode);
        break;
      }
    }
  }

  WeaselSessionId ipc_id =
      _GenerateNewWeaselSessionId(m_session_status_map, m_pid);
  DLOG(INFO) << "Add session: created session_id = " << session_id
             << ", ipc_id = " << ipc_id;
  SessionStatus& session_status = new_session_status(ipc_id);
  session_status.style = m_base_style;
  session_status.session_id = session_id;
  // ADR 0062: 会话表变化 => 重算活跃会话数（闸门的输入）。
  _RefreshLiveSessionCount();
  // spec 077: ensure ShiftState is fresh per session (R2 cross-session safety).
  m_shiftState[ipc_id] = ShiftState{};
  // ADR 0032: 记录本次会话的客户端身份。ipc_id 会被回收复用
  // (见 _GenerateNewWeaselSessionId),所以必须先清掉同 id 的旧身份,
  // 否则复用后的用户应用会被误判成自家 UI。
  const std::string client_app = _ReadClientInfo(ipc_id, buffer);
  _RememberClientKind(ipc_id, fluxing::ClassifyIpcClient(client_app));
  if (fluxing::IsOwnUiClient(_ClientKind(ipc_id))) {
    fluxing::diag::AppendLine(
        "server",
        "own_ui_session ipc_id=" + std::to_string(ipc_id) + " app=" +
            client_app);
  }

  RIME_STRUCT(RimeStatus, status);
  if (rime_api->get_status(session_id, &status)) {
    std::string schema_id = status.schema_id ? status.schema_id : std::string();

    // ★ 2026-11-09 修(第三轮):服务器重启后恢复用户上次**显式选定**的方案。
    //   m_current_schema_id 是内存镜像,进程重启后为空 ⇒ 下面的新会话继承逻辑
    //   无从继承 ⇒ 每个新会话都回落 librime 默认(schema_list 首项 rime_ice
    //   全拼)⇒ 用户看到「输入法重启后还是只能全拼」。持久化文件是唯一事实来源。
    //   只接受**当前仍然存在**的方案 id(用户可能删过 custom 文件),否则保持
    //   旧行为 —— 绝不把不存在的方案灌进 select_schema。
    if (m_available_schemas.empty())
      _RefreshSchemaList();
    if (m_current_schema_id.empty()) {
      const std::string persisted = _LoadPreferredSchemaFromDisk();
      if (!persisted.empty()) {
        bool still_available = false;
        for (const auto& id : m_available_schemas) {
          if (id == persisted) {
            still_available = true;
            break;
          }
        }
        if (still_available)
          m_current_schema_id = persisted;
      }
    }

    // ★ spec 045 修(2026-11-09):新会话继承**用户当前选中的方案**。
    //
    // 缺陷背景(用户报告「切换到搜狗双拼,仍然是全拼输入」):
    // 新会话的默认 schema 由 librime 决定(= schema_list 里的第一项,
    // 本机是 rime_ice 全拼)。用户在面板上显式选过搜狗双拼之后,新开窗口 /
    // 重新激活编辑框都会建出新会话 ⇒ 又回到全拼 ⇒ 用户看到「切了双拼,
    // 还是只能全拼输入」。
    //
    // 只在**确实不一致**时才切(避免每次聚焦编辑框都重载方案组件);
    // librime 侧的 var/previously_selected_schema 只在引擎内部生效,
    // 不保证每个新会话都继承。
    if (!m_current_schema_id.empty() && m_current_schema_id != schema_id &&
        rime_api->select_schema(session_id, m_current_schema_id.c_str())) {
      // ★ 2026-11-09 修:此处**不能**提前 free_status(&status) ——
      //   下面第 299-300 行还会读 status.schema_id / 拷贝 status,
      //   提前释放就是 use-after-free。status 统一在本块末尾(302 行)释放。
      RIME_STRUCT(RimeStatus, status2);
      if (rime_api->get_status(session_id, &status2)) {
        schema_id = status2.schema_id ? status2.schema_id : std::string();
        session_status.status = status2;
        rime_api->free_status(&status2);
      }
    }

    // spec 045 v0.18.29.0: cache current schema id for QuickPanel Row 3 label.
    //
    // ★ 2026-11-09:只在**全局镜像为空**时初始化它 —— 旧实现无条件覆盖,
    //   于是任何一次新建会话都会把「用户选中的方案」冲回 rime_ice 全拼,
    //   这正是「切了双拼还是全拼」的直接原因之一。
    if (m_current_schema_id.empty())
      m_current_schema_id = schema_id;
    if (m_available_schemas.empty())
      _RefreshSchemaList();
    m_last_schema_id = schema_id;
    m_session_schema_ids[ipc_id] = schema_id;
    _LoadSchemaSpecificSettings(ipc_id, schema_id);
    _LoadAppInlinePreeditSet(ipc_id, true);
    _UpdateInlinePreeditStatus(ipc_id);
    _RefreshTrayIcon(session_id, _UpdateUICallback);
    if (session_status.status.schema_id == nullptr)
      session_status.status = status;
    session_status.__synced = false;
    rime_api->free_status(&status);
  }
  m_ui->style() = session_status.style;
  // show session's welcome message :-) if any
  if (eat) {
    _Respond(ipc_id, eat);
  }
  add_session = true;
  _UpdateUI(ipc_id);
  add_session = false;
  m_active_session = ipc_id;
  // 批次 H:记住最近一次拿到输入焦点的会话。面板的 100ms 轮询要靠它读到
  // live 的 ascii_mode / ascii_punct(见 _ResolveTargetSession)。
  m_last_focus_session = ipc_id;
  return ipc_id;
}

DWORD RimeWithWeaselHandler::RemoveSession(WeaselSessionId ipc_id) {
  if (m_ui)
    m_ui->Hide();
  // spec 097: 活动会话结束 = 「火流猩离场」的兜底信号。
  // 切走输入法时 TSF 前端会 EndSession(),但那条路径上的
  // FocusOut(PROFILE_DEACTIVATED) 可能因为管道错误整条丢失(旧代码把管道
  // 错误静默吞成 0)。会话消失本身是不可伪造的事实:活动会话没了,火流猩就
  // 不在场了,快捷面板必须收起。
  if (ipc_id == m_active_session) {
    // ADR 0032: 自家 UI(用户短语窗口)的会话结束**不代表**「用户切走了输入法」。
    // 它只是自家窗口被销毁,必须既不改在场状态,也不收起快捷设置栏(L171)。
    const fluxing::IpcClientKind client_kind = _ClientKind(ipc_id);
    if (fluxing::ShouldTreatSessionEndAsFluxingAbsent(client_kind)) {
      m_fluxing_presence.store(FluxingPresence::kAbsent,
                                std::memory_order_relaxed);
      if (m_onHideQuickPanel)
        m_onHideQuickPanel();
      fluxing::diag::AppendLine("server",
                                 "remove_session active presence=absent");
    } else {
      fluxing::diag::AppendLine("server",
                                 "remove_session active own_ui presence=kept");
    }
  }
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return 0;
  DLOG(INFO) << "Remove session: session_id = " << to_session_id(ipc_id);
  // TODO: force committing? otherwise current composition would be lost
  rime_api->destroy_session(to_session_id(ipc_id));
  m_session_status_map.erase(ipc_id);
  // ADR 0062: 会话已移除 => 重算活跃会话数。
  _RefreshLiveSessionCount();
  // spec 077: drop ShiftState so no stale state remains if TSF holds a
  // WeaselSessionId that maps to a recycled ipc_id later.
  m_shiftState.erase(ipc_id);
  m_active_session = 0;
  // 批次 H:会话真的没了,「最近焦点会话」这档回退也就失效了 ⇒ 清掉。
  // (FocusOut **不**清 —— 面板轮询要靠它读 live 状态。)
  if (m_last_focus_session == ipc_id)
    m_last_focus_session = 0;
  return 0;
}

void RimeWithWeaselHandler::UpdateColorTheme(BOOL darkMode) {
  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    if (m_ui) {
      _UpdateUIStyle(&config, m_ui, true);
      m_current_dark_mode = darkMode;
      if (darkMode) {
        const int BUF_SIZE = 255;
        char buffer[BUF_SIZE + 1] = {0};
        if (rime_api->config_get_string(&config, "style/color_scheme_dark",
                                        buffer, BUF_SIZE)) {
          std::string color_name(buffer);
          _UpdateUIStyleColor(&config, m_ui->style(), color_name);
        }
      }
      m_base_style = m_ui->style();
    }
    rime_api->config_close(&config);
  }

  for (auto& pair : m_session_status_map) {
    RIME_STRUCT(RimeStatus, status);
    if (rime_api->get_status(to_session_id(pair.first), &status)) {
      _LoadSchemaSpecificSettings(pair.first, std::string(status.schema_id));
      _LoadAppInlinePreeditSet(pair.first, true);
      _UpdateInlinePreeditStatus(pair.first);
      pair.second.status = status;
      pair.second.__synced = false;
      rime_api->free_status(&status);
    }
  }
  m_ui->style() = get_session_status(m_active_session).style;
}

BOOL RimeWithWeaselHandler::ProcessKeyEvent(KeyEvent keyEvent,
                                            WeaselSessionId ipc_id,
                                            EatLine eat) {
  DLOG(INFO) << "Process key event: keycode = " << keyEvent.keycode
             << ", mask = " << keyEvent.mask << ", ipc_id = " << ipc_id;
  TryLazyRecovery();  // spec 053 R6 fix: auto-recover if deployer died
  if (m_disabled)
    return FALSE;
  RimeSessionId session_id = to_session_id(ipc_id);
  Bool handled = rime_api->process_key(session_id, keyEvent.keycode,
                                       expand_ibus_modifier(keyEvent.mask));
  // vim_mode when keydown only
  if (!handled && !(keyEvent.mask & ibus::Modifier::RELEASE_MASK)) {
    bool isVimBackInCommandMode =
        (keyEvent.keycode == ibus::Keycode::Escape) ||
        ((keyEvent.mask & (1 << 2)) &&
         (keyEvent.keycode == ibus::Keycode::XK_c ||
          keyEvent.keycode == ibus::Keycode::XK_C ||
          keyEvent.keycode == ibus::Keycode::XK_bracketleft));
    if (isVimBackInCommandMode &&
        rime_api->get_option(session_id, "vim_mode") &&
        !rime_api->get_option(session_id, "ascii_mode")) {
      rime_api->set_option(session_id, "ascii_mode", True);
    }
  }
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
  // 批次 H:打过字 = 这个会话就是当前输入上下文,记下来给面板回退用。
  m_last_focus_session = ipc_id;
  // spec 077: intervening (non-release, non-Shift) key between ShiftDown
  // and ShiftUp suppresses the release-only select. Per R4: only the
  // **down** event counts as intervening; letter releases don't.
  if (!(keyEvent.mask & ibus::Modifier::RELEASE_MASK) &&
      keyEvent.keycode != ibus::Keycode::Shift_L &&
      keyEvent.keycode != ibus::Keycode::Shift_R) {
    auto it = m_shiftState.find(ipc_id);
    if (it != m_shiftState.end() && it->second.downRecorded) {
      it->second.interveningKey = true;
      DLOG(INFO) << "interveningKey=true ipc_id=" << ipc_id
                 << " keycode=" << keyEvent.keycode;
    }
  }
  return (BOOL)handled;
}

void RimeWithWeaselHandler::CommitComposition(WeaselSessionId ipc_id) {
  DLOG(INFO) << "Commit composition: ipc_id = " << ipc_id;
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  rime_api->commit_composition(to_session_id(ipc_id));
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
}

void RimeWithWeaselHandler::ClearComposition(WeaselSessionId ipc_id) {
  DLOG(INFO) << "Clear composition: ipc_id = " << ipc_id;
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  rime_api->clear_composition(to_session_id(ipc_id));
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
}

void RimeWithWeaselHandler::SelectCandidateOnCurrentPage(
    size_t index,
    WeaselSessionId ipc_id) {
  DLOG(INFO) << "select candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  // spec 090: 客户端传来的 index 是「可见页内次序」(用户在候选窗里看到的
  // 位置)。屏蔽名单非空时,可见列表是引擎当前页的过滤子集,必须先经
  // m_visibleToEngineIdx 换算成引擎页内次序再调引擎;越界则什么都不做
  // (宁可不响应,也不能选中屏幕上没看见的词)。名单为空时原样透传,
  // 行为与改动前一致。
  size_t engine_index = 0;
  if (!_MapVisibleIndexToEngine(index, &engine_index))
    return;
  rime_api->select_candidate_on_current_page(to_session_id(ipc_id),
                                             engine_index);
}

void RimeWithWeaselHandler::DeleteCandidateOnCurrentPage(
    size_t index,
    WeaselSessionId ipc_id,
    EatLine eat) {
  DLOG(INFO) << "delete candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  // spec 090: 可见页内次序 -> 引擎页内次序(同 SelectCandidateOnCurrentPage)。
  size_t engine_index = 0;
  if (!_MapVisibleIndexToEngine(index, &engine_index)) {
    // IPC 是请求/应答制:不回包会把客户端挂住(同 HighlightCandidateOnCurrentPage)。
    _Respond(ipc_id, eat);
    _UpdateUI(ipc_id);
    return;
  }
  RimeSessionId session_id = to_session_id(ipc_id);

  // spec 090: 先取该候选的原文,用于写屏蔽名单。
  std::wstring candidate_text;
  RIME_STRUCT(RimeContext, del_ctx);
  if (rime_api->get_context(session_id, &del_ctx)) {
    if (del_ctx.menu.candidates &&
        static_cast<size_t>(del_ctx.menu.num_candidates) > engine_index &&
        del_ctx.menu.candidates[engine_index].text)
      candidate_text = u8tow(del_ctx.menu.candidates[engine_index].text);
    rime_api->free_context(&del_ctx);
  }

  // spec 090 / ADR 0023 D5(已定方案): 右键 = 「无条件写屏蔽名单 + 照常调
  // 引擎删除」,不按引擎返回值分流。取证依据:
  //   - librime Context::DeleteCandidate 无条件 `return true`
  //     (librime/src/rime/context.cc:146-154,上游注释原文
  //     "CAVEAT: this doesn't mean anything is deleted for sure");
  //   - rime_api_impl.h:982-1000 的 do_with_candidate_on_current_page 只在
  //     调用姿势错误(无 session / 无 menu / index >= page_size)时返回 false;
  //   - is_user_dict 在 librime C API 中不存在,无法区分自造词与系统词典词。
  // 效果: 自造词 -> 引擎删除生效(spec 028 语义保留,从 user.db 移除),名单
  // 里多留一条无害; 系统词典词 -> 删不掉,但名单让它下次不再出现。
  // 撤销入口: 托盘「恢复已忽略的候选项」清空 *.user_ignore.txt
  // (WeaselServer/WeaselServerApp.cpp:324-342)。
  if (!candidate_text.empty()) {
    fluxing::AppendToIgnoreList(WeaselUserDataPath().wstring(),
                                _CurrentSchemaIdW(session_id), candidate_text);
    // 强制下一次 _GetCandidateInfo 重读名单,让屏蔽在本次会话立即生效。
    m_ignoreSchemaId.clear();
    m_ignoreStamp = fluxing::IgnoreListStamp();
  }
  const Bool deleted =
      rime_api->delete_candidate_on_current_page(session_id, engine_index);
  (void)deleted;  // 恒为 true(见上),不用它做任何分流。
  // spec 094: 本次删除算「中间有键」。用户先按 Shift 再按 Ctrl+数字时,
  // 前端来不及吞掉那个 Shift,服务端的 ShiftUp 就会走 kSelectCandidate
  // 分支把候选词误上屏。标记 interveningKey 把这条路径堵死。
  auto shift_it = m_shiftState.find(ipc_id);
  if (shift_it != m_shiftState.end())
    shift_it->second.interveningKey = true;
  // spec 094: 回 body —— 客户端要靠它刷新候选窗(删词后立即生效)。
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
}

bool RimeWithWeaselHandler::HighlightCandidateOnCurrentPage(
    size_t index,
    WeaselSessionId ipc_id,
    EatLine eat) {
  DLOG(INFO) << "highlight candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  // spec 090: 可见页内次序 -> 引擎页内次序。越界时跳过引擎调用,但仍要
  // _Respond/_UpdateUI 按当前(未变)状态回包 —— IPC 是请求/应答制,
  // 不回包会把客户端挂住;返回 false 表示高亮没有发生。
  size_t engine_index = 0;
  if (!_MapVisibleIndexToEngine(index, &engine_index)) {
    _Respond(ipc_id, eat);
    _UpdateUI(ipc_id);
    return false;
  }
  bool res = rime_api->highlight_candidate_on_current_page(
      to_session_id(ipc_id), engine_index);
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  return res;
}

bool RimeWithWeaselHandler::ChangePage(bool backward,
                                       WeaselSessionId ipc_id,
                                       EatLine eat) {
  DLOG(INFO) << "change page, ipc_id = " << ipc_id
             << (backward ? "backward" : "foreward");
  bool res = rime_api->change_page(to_session_id(ipc_id), backward);
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  return res;
}

void RimeWithWeaselHandler::FocusIn(DWORD client_caps, WeaselSessionId ipc_id) {
  DLOG(INFO) << "Focus in: ipc_id = " << ipc_id
             << ", client_caps = " << client_caps;
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  // spec 080 (ADR 0008): "the user just switched to Fluxing" notification.
  //
  // ITfActiveLanguageProfileNotifySink::OnActivated is a per-process
  // subscription: switching the input method once makes every process
  // that loaded the Fluxing TIP send this message. The active session is
  // the gate: only the front most application shows the QuickPanel, the
  // background ones are ignored. Without the gate the panel pops up once
  // per process and the active session gets stolen by a background
  // process (spec 055 bug 2 root cause).
  //
  // This branch deliberately does NOT touch m_active_session.
  // spec 096 / ADR 0027: 判定改用共享纯函数,与 FocusOut 侧的
  // ShouldHideQuickPanelOnFocusOut 同构(宪法 R7)。
  //
  //   kProfileActivated => 用户刚切到火流猩 => 请求显示快捷面板
  //   kAppFocusGained   => 普通的窗口获得焦点 => 不显示,只更新 UI
  //   kUnknown          => 老客户端发 0 => 与今天完全一致
  //
  // profile 位与 app-focus 位同时在场时取 profile 优先。
  const FocusInKind focus_kind = ClassifyFocusIn(client_caps);
  const fluxing::IpcClientKind client_kind = _ClientKind(ipc_id);
  // ADR 0032: 自家 UI 进程(用户短语窗口)的 profile 激活**不是**「用户切到了
  // 火流猩」,既不请求显示快捷设置栏,也不改在场状态(L171)。
  if (fluxing::IsOwnUiClient(client_kind) &&
      focus_kind == FocusInKind::kProfileActivated) {
    fluxing::diag::AppendLine("server",
                               "focus_in own_ui profile presence=kept");
    return;
  }
  if (fluxing::ShouldShowQuickPanelOnFocusInForClient(client_caps,
                                                      client_kind)) {
    if (m_active_session != 0 && ipc_id != m_active_session) {
      DLOG(INFO) << "profile activated ignored, not the active session";
      return;
    }
    // spec 097: 先更新在场状态,再请求显示。
    // UI 线程执行显示之前会复核该状态 —— 若「切走」已经先到服务端,本次
    // 显示会被作废,面板不会在隐藏之后又冒出来。
    m_fluxing_presence.store(
        UpdateFluxingPresence(
            m_fluxing_presence.load(std::memory_order_relaxed), true, false),
        std::memory_order_relaxed);
    DLOG(INFO) << "profile activated, request QuickPanel";
    fluxing::diag::AppendLine("server", "focus_in profile presence=present");
    if (m_onShowQuickPanel)
      m_onShowQuickPanel();
    return;
  }
  if (focus_kind == FocusInKind::kAppFocusGained)
    DLOG(INFO) << "app focus gained, ordinary window focus";
  // v0.19.0.52 (Phase K3 T011 Option B): 抓 user foreground。
  // 此刻: WeaselTSF.dll 已通过 IPC 通知 server 端 user 在 edit field 输入 →
  // server FocusIn handler 在 IPC worker thread 跑 → GetForegroundWindow()
  // 拿 system foreground (= user app 当前 hwnd)。PhrasesDialogIPC::InjectText
  // 后续会读这个,AttachThreadInput + SetForegroundWindow 把 foreground 抢回去。
  fluxing::foreground_restore::CaptureFromCurrentThread();
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
  // 批次 H:拿到输入焦点 ⇒ 这是「当前」的会话,记下来。
  m_last_focus_session = ipc_id;
  // spec 080 (ADR 0008): the old "show the QuickPanel on every
  // FocusIn" call site was disabled with `if (false && ipc_id > 0)`
  // because the GDI+ Bitmap(IStream*) lifetime bug (L67-L69) crashed
  // WeaselServer. QuickPanelDialog now decodes the logo with WIC and
  // paints with plain GDI, so that crash chain is gone (spec 074 / L75).
  // The panel is now shown from the profile-activated branch above,
  // through the server UI thread, which owns the real button callbacks.
}

void RimeWithWeaselHandler::FocusOut(DWORD param, WeaselSessionId ipc_id) {
  DLOG(INFO) << "Focus out: ipc_id = " << ipc_id << ", param = " << param;
  if (m_ui)
    m_ui->Hide();
  m_active_session = 0;
  // 批次 H:FocusOut **不清** m_last_focus_session。
  //
  // 缺陷背景(用户 2026-11-07 需求③):「Shift 切中英文,设置栏的全/半角符号
  // 与中/英标志都不切换」。根因之一就是面板读 live 状态时唯一的入口
  // (m_active_session)在这里被清零 ⇒ GetPanelInputState 只能回落到陈旧
  // 镜像 ⇒ 图标看起来「按了没反应」。
  //
  // 为什么留着安全:m_last_focus_session 只用于**读**当前会话的 live 状态,
  // 它不产生任何副作用;会话真的没了(RemoveSession)时会被清掉。
  //
  // ADR 0032: 自家 UI 进程的 FocusOut 是「自家窗口没了」,不是「用户切走了
  // 输入法」。必须不改在场状态、不收起快捷设置栏(L171)。
  if (fluxing::IsOwnUiClient(_ClientKind(ipc_id))) {
    fluxing::diag::AppendLine(
        "server",
        std::string("focus_out param=") + std::to_string(param) +
            " presence=kept own_ui=1");
    return;
  }
  // spec 091 / ADR 0024:FocusOut 有两种语义,wParam 带标志区分 ——
  //
  //   param 带 PROFILE_DEACTIVATED => 用户切到了别的输入法。Fluxing 已经
  //     不在场,快捷设置栏必须收起,即便用户勾了「常驻」。这是 spec 091 的
  //     需求:切到火流猩显示,切到别的输入法不显示。
  //   param 为 0 => 只是输入焦点离开 / 切窗口 / 应用失焦,尊重常驻开关。
  //
  // 过去两条路径都发 wParam=0,服务端一刀切地被 AlwaysShowMode() 守卫挡住,
  // 于是「切输入法必须消失」被常驻守卫一并挡掉(spec 091 的根因)。
  // 判定逻辑放在共享头 ShouldHideQuickPanelOnFocusOut() 里,生产代码与测试
  // 同源(宪法 R7)。
  //
  // 守卫仍然加在调用点,不动 QuickPanelDialog::Hide() 本体:Hide() 还服务于
  // 需要用户明确手势的路径(Alt+, 收起 / ESC / 点词库按钮),把守卫放进
  // Hide() 内部会让那些路径在长显态下失效(ADR 0013 D7 / ADR 0016 D1)。
  //
  // spec 097: 「切走」消息先落成在场状态,再请求隐藏。
  const bool profile_deactivated =
      HasFocusOutFlag(param, WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED);
  m_fluxing_presence.store(
      UpdateFluxingPresence(
          m_fluxing_presence.load(std::memory_order_relaxed), false,
          profile_deactivated),
      std::memory_order_relaxed);
  fluxing::diag::AppendLine(
      "server",
      std::string("focus_out param=") + std::to_string(param) +
          " presence=" + (profile_deactivated ? "absent" : "kept"));
  if (ShouldHideQuickPanelOnFocusOut(param, QuickPanelDialog::AlwaysShowMode())) {
    // spec 097: 隐藏也走 UI 线程队列,与显示共用同一个队列 => 显示与隐藏
    // 在 UI 线程上定序,不会出现「隐藏之后又被显示」。
    if (m_onHideQuickPanel)
      m_onHideQuickPanel();
    else
      QuickPanelDialog::Hide();
  }
}

void RimeWithWeaselHandler::UpdateInputPosition(RECT const& rc,
                                                WeaselSessionId ipc_id) {
  DLOG(INFO) << "Update input position: (" << rc.left << ", " << rc.top
             << "), ipc_id = " << ipc_id
             << ", m_active_session = " << m_active_session;
  if (m_ui)
    m_ui->UpdateInputPosition(rc);
  TryLazyRecovery();  // spec 053 R6 fix
  if (m_disabled)
    return;
  if (m_active_session != ipc_id) {
    _UpdateUI(ipc_id);
    m_active_session = ipc_id;
  }
}

std::string RimeWithWeaselHandler::m_message_type;
std::string RimeWithWeaselHandler::m_message_value;
std::string RimeWithWeaselHandler::m_message_label;
std::string RimeWithWeaselHandler::m_option_name;
std::mutex RimeWithWeaselHandler::m_notifier_mutex;

void RimeWithWeaselHandler::OnNotify(void* context_object,
                                     uintptr_t session_id,
                                     const char* message_type,
                                     const char* message_value) {
  // may be running in a thread when deploying rime
  RimeWithWeaselHandler* self =
      reinterpret_cast<RimeWithWeaselHandler*>(context_object);
  if (!self || !message_type || !message_value)
    return;
  std::lock_guard<std::mutex> lock(m_notifier_mutex);
  m_message_type = message_type;
  m_message_value = message_value;
  if (RIME_API_AVAILABLE(rime_api, get_state_label) &&
      !strcmp(message_type, "option")) {
    Bool state = message_value[0] != '!';
    const char* option_name = message_value + !state;
    m_option_name = option_name;
    const char* state_label =
        rime_api->get_state_label(session_id, option_name, state);
    if (state_label) {
      m_message_label = std::string(state_label);
    }
  }
}

std::string RimeWithWeaselHandler::_ReadClientInfo(WeaselSessionId ipc_id,
                                                   LPWSTR buffer) {
  std::string app_name;
  // parse request text
  wbufferstream bs(buffer, WEASEL_IPC_BUFFER_LENGTH);
  std::wstring line;
  while (bs.good()) {
    std::getline(bs, line);
    if (!bs.good())
      break;
    // file ends
    if (line == L".")
      break;
    const std::wstring kClientAppKey = L"session.client_app=";
    if (starts_with(line, kClientAppKey)) {
      std::wstring lwr = line;
      to_lower(lwr);
      app_name = wtou8(lwr.substr(kClientAppKey.length()));
    }
  }
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  // set app specific options
  if (!app_name.empty()) {
    rime_api->set_property(session_id, "client_app", app_name.c_str());

    auto it = m_app_options.find(app_name);
    if (it != m_app_options.end()) {
      AppOptions& options(m_app_options[it->first]);
      for (const auto& pair : options) {
        DLOG(INFO) << "set app option: " << pair.first << " = " << pair.second;
        rime_api->set_option(session_id, pair.first.c_str(), Bool(pair.second));
      }
    }
  }
  // inline preedit
  bool inline_preedit = session_status.style.inline_preedit;
  rime_api->set_option(session_id, "inline_preedit", Bool(inline_preedit));
  // show soft cursor on weasel panel but not inline
  rime_api->set_option(session_id, "soft_cursor", Bool(!inline_preedit));
  // ADR 0032: 把客户端自报的名字交回去,调用方据此判定「自家 UI 进程」。
  return app_name;
}

namespace {
// ADR 0032: 客户端身份表的容量上限。身份表不随会话结束删除(必须活过
// RemoveSession),所以自己封顶,防长时间运行无界增长。
const size_t kMaxRememberedClientKinds = 256;
}  // namespace

// ADR 0032: 记住某个 IPC 会话的客户端归属。
//
// 为什么身份必须活过 RemoveSession:TSF 的 Deactivate 路径是「先 EndSession
// (→ RemoveSession)再发 FocusOut(PROFILE_DEACTIVATED)」,FocusOut 到达时
// 会话已经销毁 —— 若那时身份已被删,自家 UI 的 FocusOut 就会被当成
// 「用户切走了输入法」(L171 的 bug 现场)。
void RimeWithWeaselHandler::_RememberClientKind(
    WeaselSessionId ipc_id, fluxing::IpcClientKind kind) {
  m_session_client_kinds.erase(ipc_id);
  m_session_client_kinds[ipc_id] = kind;
  if (m_session_client_kinds.size() > kMaxRememberedClientKinds) {
    // 删最小的 id(最老的会话)。最坏情况只是某条身份丢失,退回到
    // 「按普通用户应用处理」—— 那是安全方向(行为与改动前一致)。
    m_session_client_kinds.erase(m_session_client_kinds.begin());
  }
}

fluxing::IpcClientKind RimeWithWeaselHandler::_ClientKind(
    WeaselSessionId ipc_id) const {
  auto it = m_session_client_kinds.find(ipc_id);
  if (it == m_session_client_kinds.end())
    return fluxing::IpcClientKind::kUserApp;  // 未知 = 保持既有行为
  return it->second;
}

void RimeWithWeaselHandler::_GetCandidateInfo(CandidateInfo& cinfo,
                                              RimeContext& ctx,
                                              RimeSessionId session_id) {
  // spec 090 / ADR 0023 D4: 屏蔽过滤只允许发生在这里(服务端、唯一过滤点)。
  // RimeContext::menu.candidates 本身就是「当前页切片」(rime_api_impl.h
  // do_with_candidate_on_current_page 用 page_start + index 定位;menu 按
  // CreatePage(page_size, page_no) 填充),UI 侧 Layout 不再做二次分页,
  // 用户看到的列表 == 这里的输出。若 UI 再过滤一次,「可见次序 -> 引擎页内
  // 次序」映射就会错位,Shift / 数字键将选中屏幕上没看见的词(E5)。
  // 名单为空时本函数的输出与改动前逐字节一致。
  _RefreshIgnoreList(session_id);

  const int num = ctx.menu.num_candidates;
  const bool has_labels =
      RIME_STRUCT_HAS_MEMBER(ctx, ctx.select_labels) && ctx.select_labels;

  // 过滤与映射必须在同一次遍历里产出(FR-006),否则两套索引各说各话。
  m_visibleToEngineIdx.clear();
  std::vector<int> visible_engine_idx;
  if (num > 0) {
    m_visibleToEngineIdx.reserve(num);
    visible_engine_idx.reserve(num);
    for (int i = 0; i < num; ++i) {
      if (!m_ignoreList.empty() &&
          m_ignoreList.count(u8tow(ctx.menu.candidates[i].text)) > 0)
        continue;  // 命中屏蔽名单:对用户不可见
      visible_engine_idx.push_back(i);
      m_visibleToEngineIdx.push_back(static_cast<size_t>(i));
    }
  }

  const int visible_count = static_cast<int>(visible_engine_idx.size());
  cinfo.candies.resize(visible_count);
  cinfo.comments.resize(visible_count);
  cinfo.labels.resize(visible_count);
  for (int v = 0; v < visible_count; ++v) {
    const int i = visible_engine_idx[v];
    cinfo.candies[v].str = escape_string(u8tow(ctx.menu.candidates[i].text));
    if (ctx.menu.candidates[i].comment) {
      cinfo.comments[v].str =
          escape_string(u8tow(ctx.menu.candidates[i].comment));
    }
    // 标签一律按「可见位置 v」取:数字键/选词键由客户端按可见位置发送,
    // 名单为空时 v == i,输出与改动前逐字节一致。
    if (has_labels) {
      cinfo.labels[v].str = escape_string(u8tow(ctx.select_labels[v]));
    } else if (ctx.menu.select_keys) {
      cinfo.labels[v].str =
          escape_string(std::wstring(1, ctx.menu.select_keys[v]));
    } else {
      cinfo.labels[v].str = std::to_wstring((v + 1) % 10);
    }
  }
  // highlighted 原值是引擎页内索引,必须重映射成可见索引:被高亮的候选若
  // 正好被屏蔽,退到 0;绝不越界。名单为空时映射结果与原值相同。
  int highlighted = 0;
  for (int v = 0; v < visible_count; ++v) {
    if (visible_engine_idx[v] == ctx.menu.highlighted_candidate_index) {
      highlighted = v;
      break;
    }
  }
  cinfo.highlighted = highlighted;
  // currentPage / is_last_page 维持引擎语义;totalPages 全仓无写入点,不设。
  cinfo.currentPage = ctx.menu.page_no;
  cinfo.is_last_page = ctx.menu.is_last_page;
}

// ===== spec 090 / ADR 0023: 候选词屏蔽名单支撑方法 =====

std::wstring RimeWithWeaselHandler::_CurrentSchemaIdW(
    RimeSessionId session_id) {
  // 现取 get_status,不读 m_session_status_map 里的缓存:_Respond 先
  // `session_status.status = status` 浅拷贝、随即 free_status(&status),
  // 缓存里的 schema_id 指针已悬空,不可用。
  RIME_STRUCT(RimeStatus, status);
  std::wstring schema_id;
  if (rime_api->get_status(session_id, &status)) {
    if (status.schema_id)
      schema_id = u8tow(status.schema_id);
    rime_api->free_status(&status);
  }
  return schema_id;
}

void RimeWithWeaselHandler::_RefreshIgnoreList(RimeSessionId session_id) {
  const std::wstring schema_id = _CurrentSchemaIdW(session_id);
  const std::wstring user_dir = WeaselUserDataPath().wstring();
  const fluxing::IgnoreListStamp stamp =
      fluxing::StampIgnoreListFile(user_dir, schema_id);
  // 方案未变且名单文件指纹未变 -> 复用内存名单,不每次出候选都读盘。
  // 指纹变化(托盘「恢复已忽略的候选项」删文件 / 右键追加改写)或切方案
  // -> 重读。读盘失败按空名单静默降级,不弹框、不阻塞。
  if (schema_id == m_ignoreSchemaId && stamp == m_ignoreStamp)
    return;
  m_ignoreSchemaId = schema_id;
  m_ignoreStamp = stamp;
  m_ignoreList.clear();
  fluxing::LoadIgnoreListFrom(user_dir, schema_id, &m_ignoreList);
}

bool RimeWithWeaselHandler::_MapVisibleIndexToEngine(size_t visible_index,
                                                     size_t* engine_index) {
  if (!engine_index)
    return false;
  // 名单为空 => 没有过滤发生 => 可见次序就是引擎次序,原样透传。即使
  // m_visibleToEngineIdx 为空或过期也不受影响,保证未启用屏蔽的场景行为
  // 与改动前逐字节一致。
  if (m_ignoreList.empty()) {
    *engine_index = visible_index;
    return true;
  }
  // 名单非空时严格按映射换算;越界返回 false,调用方必须什么都不做
  // (不选词、不上屏、不关窗,也不退回全局索引 —— ADR 0023 D2)。
  return fluxing::MapVisibleIndexToEngine(m_visibleToEngineIdx, visible_index,
                                          engine_index);
}

void RimeWithWeaselHandler::StartMaintenance() {
  m_session_status_map.clear();
  // ADR 0062: 清表 => 活跃会话数归零（Finalize 里还会再置一次）。
  _RefreshLiveSessionCount();
  Finalize();
  _UpdateUI(0);
}

void RimeWithWeaselHandler::EndMaintenance() {
  if (m_disabled) {
    Initialize();
    _UpdateUI(0);
  }
  m_session_status_map.clear();
  // ADR 0062: 清表 => 活跃会话数归零。
  _RefreshLiveSessionCount();
}

void RimeWithWeaselHandler::SetOption(WeaselSessionId ipc_id,
                                      const std::string& opt,
                                      bool val) {
  // ADR 0012: this is the only place where an ascii_mode change may reach
  // another session. A bare Shift (ADR 0010) never comes here -- it calls
  // rime_api->set_option on its own session -- so it can never switch a
  // second window.
  const bool broadcast_ascii =
      opt == "ascii_mode" &&
      fluxing::DecideAsciiModeScope(/*explicit_control=*/true,
                                    m_global_ascii_configured) ==
          fluxing::AsciiModeScope::kAllSessions;
  // v6 需求②: 先解析出**真实**的目标会话,再 set_option。
  //
  // 缺陷背景(用户 2026-11-07 报告:「点击也无法切换全半角」):原来的
  // !ipc_id 分支直接 `to_session_id(m_active_session)` ——
  //   ① m_active_session == 0(焦点还没落到任何输入窗口)时,to_session_id 会
  //      **插入**一个 key=0 的空 status 并拿 session_id=0 去 set_option ⇒
  //      静默无效(调和 set_option 都不会报错);
  //   ② 下面的缓存块照旧写 m_full_shape = val ⇒ 镜像与真实状态永久劈叉,
  //      从此面板图标再也不跟着变。
  // 修法:按「ipc_id → 活动会话 → 最近焦点会话 → 唯一会话」顺序解析;解析
  // 不出来就直接 return 且**不写缓存** —— 宁可这一下按不动,也不能让镜像说谎。
  //
  // ★ 全程不用 to_session_id() 取 session_id:它的 operator[] 副作用正是 ①
  //   那个假会话的来源。只在已确认存在的 map 条目上读 .session_id。
  //   (批次 H 把这套顺序抽成 _ResolveTargetSession,设置栏与面板共用同源。)
  RimeSessionId target = _ResolveTargetSession(ipc_id);

  // 批次 H:「标点宽度」这组 option 统一走 _ApplyPunctWidth 落点 ——
  // ascii_punct 与 full_shape 必须**同向**,否则引擎里会留下两套互相矛盾的
  // 状态(面板显示半角、引擎还在打中文标点)。
  //   ascii_punct=true ⇒ 半角 ⇒ 英文标点(punctuator 直接 kNoop)
  //   full_shape=true  ⇒ 全角 ⇒ 中文标点
  if (opt == "ascii_punct" || opt == "full_shape") {
    if (!target)
      return;  // 无法解析目标会话 ⇒ 不 set_option,也不写缓存(见上)
    _ApplyPunctWidth(target, opt == "ascii_punct" ? val : !val);
    return;
  }

  if (broadcast_ascii) {
    // ascii_mode 广播到所有会话(ADR 0012 的显式控制路径)。
    if (ipc_id && target)
      rime_api->set_option(target, opt.c_str(), val);
    for (auto& pair : m_session_status_map) {
      if (!pair.second.session_id)
        continue;
      if (ipc_id && pair.first == ipc_id)
        continue;  // 上面已经单独设过,不重复
      rime_api->set_option(pair.second.session_id, "ascii_mode", val);
    }
  } else {
    if (!target)
      return;  // 无法解析目标会话 ⇒ 不 set_option,也不写缓存(见上)
    rime_api->set_option(target, opt.c_str(), val);
  }
  // spec 045: cache 简/繁 / 全/半角 / ascii mode for QuickPanel initial state.
  // (批次 H:full_shape 的镜像改由 _ApplyPunctWidth 写 —— 它与 ascii_punct
  //  同源,不能在这里各写各的。)
  if (opt == "simplification")
    m_simplification = val;
  else if (opt == "ascii_mode")
    m_global_ascii_mode = val;
}

void RimeWithWeaselHandler::SetPunctuationWidth(bool halfwidth,
                                                 WeaselSessionId ipc_id) {
  // 批次 H:设置栏「全/半角」的唯一入口。
  // halfwidth=true  ⇒ ascii_punct=true ⇒ 英文标点(半角)
  // halfwidth=false ⇒ ascii_punct=false ⇒ 中文标点(全角)
  const RimeSessionId target = _ResolveTargetSession(ipc_id);
  if (!target)
    return;  // 解析不出会话 ⇒ 什么都不做(绝不让镜像说谎)
  _ApplyPunctWidth(target, halfwidth);
}

// spec 045 修(2026-11-09):读某个 rime 会话**当前**的 schema_id。
//
// 口径:进程内 C API(读内存),不碰文件 ⇒ 满足 constitution P2(显示路径零 I/O)。
// 与 _GetStatus 里读到的 status.schema_id 同源,但**不依赖 IPC 往返** ——
// 面板的 100ms 轮询与用户的点击都可能发生在任何 IPC 往返之前。
std::string RimeWithWeaselHandler::_LiveSchemaId(RimeSessionId session_id) {
  if (!rime_api || !session_id)
    return std::string();
  RIME_STRUCT(RimeStatus, status);
  if (!rime_api->get_status(session_id, &status))
    return std::string();
  std::string schema_id = status.schema_id ? status.schema_id : std::string();
  rime_api->free_status(&status);
  return schema_id;
}

// spec 045 修(2026-11-09):面板显示的方案名必须是**目标会话**的 live 值。
//
// 缺陷背景(用户报告「切换到搜狗双拼,仍然是全拼输入」):旧实现返回全局单值
// m_current_schema_id,而该值会被 AddSession 用**新会话的默认方案**(rime_ice
// 全拼)覆盖 ⇒ 面板显示的方案名与实际输入会话脱节,且「下一个方案」的起点
// 算错(可能算出来的就是当前方案 ⇒ 用户看到「点了没反应」)。
std::string RimeWithWeaselHandler::GetCurrentSchemaId() {
  const RimeSessionId target = _ResolveTargetSession(0);
  if (target) {
    std::string live = _LiveSchemaId(target);
    if (!live.empty())
      return live;
  }
  return m_current_schema_id;
}

// spec 045 修(2026-11-09):切换输入方案时的**用户可见反馈**。
//
// 背景:spec 099 的不变量是「每个可见槽位点击必产生确定的用户可见效果」。
// 旧实现 select_schema 失败时**全程静默**(返回 False 就走人),用户在面板上
// 点「切换到下一个输入方案」看不到任何变化,只能靠猜。
// 现在:失败 / 无会话 / 无可用方案都给出中文提示(候选窗 aux 提示,与既有
// 「部署完成」提示同一通道),成功时面板标签本身会变成新方案名。
void RimeWithWeaselHandler::_NotifySchemaSwitch(const std::wstring& text) {
  // ★ 2026-11-09 修(第三轮):诊断行移到**早退之前**。
  //   旧位置在 `if (!m_ui)` 之后 ⇒ m_ui 为空时整条切换链在日志里彻底消失,
  //   上一轮据此误判成「切换请求没到服务端」(成功路径也不写行)。现在无论
  //   成败、无论有无 UI,都留下一条不可伪造的事实。
  fluxing::diag::AppendLine("server", "schema_switch notice");
  if (!m_ui || text.empty())
    return;
  weasel::Context ctx;
  weasel::Status status;
  ctx.aux.str = text;
  m_ui->Update(ctx, status);
  m_ui->ShowWithTimeout(1500);
}

void RimeWithWeaselHandler::SelectSchema(const std::string& schema_id) {
  // spec 045 v0.18.29.0: QuickPanel 切换 button handler.
  //
  // ★ 2026-11-09 修(用户报告「切换到搜狗双拼,仍然是全拼输入」):
  //   旧实现用 rime_api->create_session() 造一个**新会话**再 select_schema
  //   —— 改的是那个没人用的新会话,对用户正在输入的窗口**没有任何可见效果**
  //   (槽位点了没反应)。现在与 SwitchToNextSchema 同一口径:作用到目标会话,
  //   并广播到全部活动会话(用户显式选方案 = 全局语义,见 ADR 0048)。
  if (!rime_api)
    return;
  if (schema_id.empty()) {
    // 旧行为:不给目标就循环到下一个。
    SwitchToNextSchema();
    return;
  }
  const RimeSessionId target = _ResolveTargetSession(0);
  if (!target) {
    _NotifySchemaSwitch(L"没有可切换的输入会话");
    return;
  }
  bool applied = false;
  for (auto& pair : m_session_status_map) {
    if (!pair.second.session_id)
      continue;
    if (rime_api->select_schema(pair.second.session_id, schema_id.c_str()))
      applied = true;
  }
  if (!applied)
    applied = rime_api->select_schema(target, schema_id.c_str()) ? true : false;
  // ★ 2026-11-09 修(第三轮):「广播里任意一个会话成功」**不等于**用户成功。
  //   用户正在输入的那个会话就是 target;它没切过去,用户看到的仍然是旧方案。
  //   判据只认**目标会话 live 的 schema_id**,不认 C API 的返回值。
  if (_LiveSchemaId(target) != schema_id)
    rime_api->select_schema(target, schema_id.c_str());
  const std::string live = _LiveSchemaId(target);
  fluxing::diag::AppendLine(
      "server", "schema_switch_requested to=" + schema_id + " target_live=" +
                    (live.empty() ? std::string("<unknown>") : live) +
                    " broadcast_any=" + (applied ? "1" : "0"));
  // live 为空 = get_status 取不到(会话刚好销毁),此时只能信任 C API 返回值。
  const bool verified = live.empty() ? applied : (live == schema_id);
  if (!verified) {
    _NotifySchemaSwitch(L"切换输入方案失败");
    return;
  }
  m_current_schema_id = schema_id;
  // ★ 2026-11-09 修(第三轮):持久化 ⇒ WeaselServer 重启后仍然生效。
  _SavePreferredSchemaToDisk(schema_id);
  if (_UpdateUICallback)
    _UpdateUICallback();
}

// spec 099 P0-3: 快捷面板槽 0 = 「切换到下一个输入方案」。
//
// 与 SelectSchema 的差别(本函数存在的理由):
//   SelectSchema 用 rime_api->create_session() —— 那是**新会话**,改它对当前
//   正在输入的窗口没有任何可见效果(槽位点了没反应)。槽 0 必须作用于**活动
//   会话**(to_session_id(m_active_session)),与 SetOption(0, ...) 同一形状。
//   目标 id 的选择走 fluxing::NextSchemaId(纯函数,可单测)。
void RimeWithWeaselHandler::SwitchToNextSchema() {
  // spec 099 P0-3: 快捷面板槽 0 = 「切换到下一个输入方案」。
  //
  // ★ 2026-11-09 修(用户报告「切换到搜狗双拼,仍然是全拼输入」):
  //   旧实现有三处致命缺陷,合起来正好复现用户看到的「点了没反应」:
  //   ① 目标会话用 to_session_id(m_active_session) —— m_active_session 会被
  //      FocusOut 清零,而 to_session_id 的 operator[] 会**插入**一个 key=0 的
  //      假会话,拿 session_id=0 去 select_schema ⇒ librime 收到非法 session_id
  //      ⇒ 返回 False,**全程静默**(没有任何用户可见反馈)。
  //      现在统一走 _ResolveTargetSession(显式 → 活动 → 最近焦点 → 唯一会话)。
  //   ② 起点方案取全局 m_current_schema_id —— 该值会被别的新会话覆盖
  //      (AddSession 用新会话默认方案 rime_ice 全拼盖掉),于是算出的「下一个」
  //      可能就是当前方案 ⇒ 看起来没切换。
  //      现在起点取**目标会话 live 的 schema_id**。
  //   ③ 只切一个会话 —— 用户在 A 窗口切到双拼、在 B 窗口输入(那个会话还是
  //      rime_ice 全拼)⇒ 仍然是全拼输入。
  //      用户显式选方案是**全局**语义(与商业输入法一致),故广播到全部活动会话。
  if (!rime_api)
    return;
  const RimeSessionId target = _ResolveTargetSession(0);
  if (!target) {
    _NotifySchemaSwitch(L"没有可切换的输入会话");
    return;
  }
  if (m_available_schemas.empty())
    _RefreshSchemaList();
  if (m_available_schemas.empty()) {
    _NotifySchemaSwitch(L"没有可用的输入方案");
    return;
  }
  // 起点 = 目标会话当前的 live 方案(拿不到才回落全局镜像)。
  std::string current = _LiveSchemaId(target);
  if (current.empty())
    current = m_current_schema_id;
  const std::string next_id = fluxing::NextSchemaId(m_available_schemas, current);
  if (next_id.empty()) {
    _NotifySchemaSwitch(L"没有可用的输入方案");
    return;
  }
  bool applied = false;
  for (auto& pair : m_session_status_map) {
    if (!pair.second.session_id)
      continue;
    if (rime_api->select_schema(pair.second.session_id, next_id.c_str()))
      applied = true;
  }
  if (!applied)
    applied = rime_api->select_schema(target, next_id.c_str()) ? true : false;
  // ★ 2026-11-09 修(第三轮):同 SelectSchema —— 只认目标会话 live 的方案。
  //   旧实现的 `applied` 语义是「任意会话成功即算成功」,于是 target 失败时
  //   会被别的会话的成功掩盖 ⇒ 用户面板上点了,自己窗口里还是全拼。
  if (_LiveSchemaId(target) != next_id)
    rime_api->select_schema(target, next_id.c_str());
  const std::string live = _LiveSchemaId(target);
  fluxing::diag::AppendLine(
      "server", "schema_switch_requested to=" + next_id + " target_live=" +
                    (live.empty() ? std::string("<unknown>") : live) +
                    " broadcast_any=" + (applied ? "1" : "0"));
  const bool verified = live.empty() ? applied : (live == next_id);
  if (!verified) {
    _NotifySchemaSwitch(L"切换输入方案失败");
    return;
  }
  m_current_schema_id = next_id;
  // ★ 2026-11-09 修(第三轮):持久化 ⇒ WeaselServer 重启后仍然生效。
  _SavePreferredSchemaToDisk(next_id);
  if (_UpdateUICallback)
    _UpdateUICallback();
}

void RimeWithWeaselHandler::_RefreshSchemaList() {
  m_available_schemas.clear();
  if (!rime_api)
    return;
  RimeSchemaList sl = {0};
  if (rime_api->get_schema_list(&sl)) {
    for (size_t i = 0; i < sl.size; ++i) {
      if (sl.list[i].schema_id)
        m_available_schemas.emplace_back(sl.list[i].schema_id);
    }
    rime_api->free_schema_list(&sl);
  }
}

void RimeWithWeaselHandler::OnUpdateUI(std::function<void()> const& cb) {
  _UpdateUICallback = cb;
}

// spec 053 R6 fix (L55): detect WeaselDeployer process actually alive.
// Before: CreateMutex bInitialOwner=TRUE caused a false negative
// (the call could itself create the mutex and immediately own it, so
// GetLastError()==ERROR_ALREADY_EXISTS was unreliable). And it could
// not detect the *abandoned* state - the exact case where taskkill /F
// or AV quarantine killed WeaselDeployer.exe without releasing the
// named mutex. The kernel keeps the mutex held but the holder is dead;
// legacy code returned true forever, locking out all input.
//
// After: OpenMutex does not create. WaitForSingleObject(_, 0) returns
//   WAIT_OBJECT_0   -> we own it (signaled, no other holder)
//   WAIT_TIMEOUT     -> another thread holds it
//   WAIT_ABANDONED   -> holder died without releasing (R6 signal!)
// We also release ownership we may have acquired in the OBJECT_0 case
// so we do not leak the mutex.
bool RimeWithWeaselHandler::_IsDeployerRunning() {
  HANDLE hMutex = ::OpenMutex(SYNCHRONIZE, FALSE, L"WeaselDeployerMutex");
  if (!hMutex) {
    // mutex does not exist -> no deployer
    return false;
  }
  DWORD result = ::WaitForSingleObject(hMutex, 0);
  if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) {
    // We have ownership in both cases. Release so we do not leak.
    ::ReleaseMutex(hMutex);
    ::CloseHandle(hMutex);
    if (result == WAIT_ABANDONED) {
      // R6 signal: holder died, mutex was abandoned by kernel.
      DLOG(INFO) << "_IsDeployerRunning: WAIT_ABANDONED -> deployer dead";
    }
    return false;
  }
  if (result == WAIT_TIMEOUT) {
    ::CloseHandle(hMutex);
    return true;  // some live thread holds it
  }
  // WAIT_FAILED or other error: be conservative
  ::CloseHandle(hMutex);
  return true;
}

// spec 053 R6 fix (L55): if m_disabled is true but the deployer
// process has actually died (taskkill /F, AV quarantine, crash), the
// named mutex stays held by the kernel with no live holder. Legacy
// _IsDeployerRunning() returned true forever, so every
// ProcessKeyEvent / AddSession / FocusIn / etc. early-returned FALSE
// and the user could not type. TryLazyRecovery re-checks the deployer
// state and, if the deployer has died, re-runs Initialize() to clear
// m_disabled. Called at the top of every public m_disabled early-return
// path so the user does not have to reboot WeaselServer to recover.
void RimeWithWeaselHandler::TryLazyRecovery() {
  if (!m_disabled)
    return;
  if (_IsDeployerRunning()) {
    // deployer is actually running, m_disabled is legitimate
    return;
  }
  DLOG(INFO) << "TryLazyRecovery: deployer not running, auto-Initialize";
  Initialize();
  if (m_disabled) {
    DLOG(WARNING) << "TryLazyRecovery: Initialize still disabled after retry";
    return;
  }
  _UpdateUI(0);
  DLOG(INFO) << "TryLazyRecovery: success, m_disabled = false";
}

void RimeWithWeaselHandler::_UpdateUI(WeaselSessionId ipc_id) {
  // if m_ui nullptr, _UpdateUI meaningless
  if (!m_ui)
    return;

  Status& weasel_status = m_ui->status();
  Context weasel_context;

  RimeSessionId session_id = to_session_id(ipc_id);

  if (ipc_id == 0)
    weasel_status.disabled = m_disabled;

  _GetStatus(weasel_status, ipc_id, weasel_context);

  SessionStatus& session_status = get_session_status(ipc_id);
  if (rime_api->get_option(session_id, "inline_preedit"))
    session_status.style.client_caps |= INLINE_PREEDIT_CAPABLE;
  else
    session_status.style.client_caps &= ~INLINE_PREEDIT_CAPABLE;

  if (!_ShowMessage(weasel_context, weasel_status)) {
    m_ui->Hide();
    m_ui->Update(weasel_context, weasel_status);
  }

  _RefreshTrayIcon(session_id, _UpdateUICallback);

  {
    std::lock_guard<std::mutex> lock(m_notifier_mutex);
    m_message_type.clear();
    m_message_value.clear();
    m_message_label.clear();
    m_option_name.clear();
  }
}

void RimeWithWeaselHandler::_LoadSchemaSpecificSettings(
    WeaselSessionId ipc_id,
    const std::string& schema_id) {
  if (!m_ui)
    return;
  RimeConfig config;
  if (!rime_api->schema_open(schema_id.c_str(), &config))
    return;
  _UpdateShowNotifications(&config);
  m_ui->style() = m_base_style;
  _UpdateUIStyle(&config, m_ui, false);
  SessionStatus& session_status = get_session_status(ipc_id);
  session_status.style = m_ui->style();
  UIStyle& style = session_status.style;
  // load schema color style config
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  const auto update_color_scheme = [&]() {
    std::string color_name(buffer);
    RimeConfigIterator preset = {0};
    if (rime_api->config_begin_map(
            &preset, &config, ("preset_color_schemes/" + color_name).c_str())) {
      _UpdateUIStyleColor(&config, style, color_name);
      rime_api->config_end(&preset);
    } else {
      RimeConfig weaselconfig;
      if (rime_api->config_open("weasel", &weaselconfig)) {
        _UpdateUIStyleColor(&weaselconfig, style, color_name);
        rime_api->config_close(&weaselconfig);
      }
    }
  };
  const char* key =
      m_current_dark_mode ? "style/color_scheme_dark" : "style/color_scheme";
  if (rime_api->config_get_string(&config, key, buffer, BUF_SIZE))
    update_color_scheme();
  // load schema icon start
  {
    const auto load_icon = [](RimeConfig& config, const char* key1,
                              const char* key2) {
      const auto user_dir = WeaselUserDataPath();
      const auto shared_dir = WeaselSharedDataPath();
      const int BUF_SIZE = 255;
      char buffer[BUF_SIZE + 1] = {0};
      if (rime_api->config_get_string(&config, key1, buffer, BUF_SIZE) ||
          (key2 != NULL &&
           rime_api->config_get_string(&config, key2, buffer, BUF_SIZE))) {
        auto resource = u8tow(buffer);
        if (fs::is_regular_file(user_dir / resource))
          return (user_dir / resource).wstring();
        else if (fs::is_regular_file(shared_dir / resource))
          return (shared_dir / resource).wstring();
      }
      return std::wstring();
    };
    style.current_zhung_icon =
        load_icon(config, "schema/icon", "schema/zhung_icon");
    style.current_ascii_icon = load_icon(config, "schema/ascii_icon", NULL);
    style.current_full_icon = load_icon(config, "schema/full_icon", NULL);
    style.current_half_icon = load_icon(config, "schema/half_icon", NULL);
  }
  // load schema icon end
  rime_api->config_close(&config);
}

void RimeWithWeaselHandler::_LoadAppInlinePreeditSet(WeaselSessionId ipc_id,
                                                     bool ignore_app_name) {
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  static char _app_name[50];
  rime_api->get_property(session_id, "client_app", _app_name,
                         sizeof(_app_name) - 1);
  std::string app_name(_app_name);
  if (!ignore_app_name && m_last_app_name == app_name)
    return;
  m_last_app_name = app_name;
  bool inline_preedit = session_status.style.inline_preedit;
  bool found = false;
  if (!app_name.empty()) {
    auto it = m_app_options.find(app_name);
    if (it != m_app_options.end()) {
      AppOptions& options(m_app_options[it->first]);
      for (const auto& pair : options) {
        if (pair.first == "inline_preedit") {
          rime_api->set_option(session_id, pair.first.c_str(),
                               Bool(pair.second));
          session_status.style.inline_preedit = Bool(pair.second);
          found = true;
          break;
        }
      }
    }
  }
  if (!found) {
    session_status.style.inline_preedit = m_base_style.inline_preedit;
    // load from schema.
    RIME_STRUCT(RimeStatus, status);
    if (rime_api->get_status(session_id, &status)) {
      std::string schema_id = status.schema_id;
      RimeConfig config;
      if (rime_api->schema_open(schema_id.c_str(), &config)) {
        Bool value = False;
        if (rime_api->config_get_bool(&config, "style/inline_preedit",
                                      &value)) {
          session_status.style.inline_preedit = value;
        }
        rime_api->config_close(&config);
      }
      rime_api->free_status(&status);
    }
  }
  if (session_status.style.inline_preedit != inline_preedit)
    _UpdateInlinePreeditStatus(ipc_id);
}

bool RimeWithWeaselHandler::_ShowMessage(Context& ctx, Status& status) {
  std::lock_guard<std::mutex> lock(m_notifier_mutex);
  if (m_message_type.empty() || m_message_value.empty())
    return m_ui->IsCountingDown();
  // show as auxiliary string
  std::wstring& tips(ctx.aux.str);
  bool show_icon = false;
  if (m_message_type == "deploy") {
    if (m_message_value == "start")
      if (GetThreadUILanguage() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
        tips = L"Deploying RIME";
      else
        tips = L"正在部署 RIME";
    else if (m_message_value == "success")
      if (GetThreadUILanguage() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
        tips = L"Deployed";
      else
        tips = L"部署完成";
    else if (m_message_value == "failure") {
      if (GetThreadUILanguage() ==
          MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL))
        tips = L"有錯誤，請查看日誌 %TEMP%\\rime.weasel\\rime.weasel.*.INFO";
      else if (GetThreadUILanguage() ==
               MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
        tips = L"有错误，请查看日志 %TEMP%\\rime.weasel\\rime.weasel.*.INFO";
      else
        tips =
            L"There is an error, please check the logs "
            L"%TEMP%\\rime.weasel\\rime.weasel.*.INFO";
    }
  } else if (m_message_type == "schema") {
    tips = /*L"【" + */ status.schema_name /* + L"】"*/;
  } else if (m_message_type == "option") {
    status.type = SCHEMA;
    if (m_message_value == "!ascii_mode") {
      show_icon = true;
    } else if (m_message_value == "ascii_mode") {
      show_icon = true;
    } else
      tips = u8tow(m_message_label);

    if (m_message_value == "full_shape" || m_message_value == "!full_shape")
      status.type = FULL_SHAPE;
  }
  auto counter = m_ui->IsCountingDown();
  if (!show_icon && counter)
    return counter;
  auto foption = m_show_notifications.find(m_option_name);
  auto falways = m_show_notifications.find("always");
  if ((!add_session && (foption != m_show_notifications.end() ||
                        falways != m_show_notifications.end())) ||
      m_message_type == "deploy") {
    m_ui->Update(ctx, status);
    if (m_show_notifications_time)
      m_ui->ShowWithTimeout(m_show_notifications_time);
    return true;
  } else {
    return m_ui->IsCountingDown();
  }
}
inline std::string _GetLabelText(const std::vector<Text>& labels,
                                 int id,
                                 const wchar_t* format) {
  wchar_t buffer[128];
  swprintf_s<128>(buffer, format, labels.at(id).str.c_str());
  return wtou8(std::wstring(buffer));
}

bool RimeWithWeaselHandler::_Respond(WeaselSessionId ipc_id, EatLine eat) {
  std::wstring body;
  body.reserve(4096);
  std::vector<const char*> actions;
  actions.reserve(8);

  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  RIME_STRUCT(RimeCommit, commit);
  if (rime_api->get_commit(session_id, &commit)) {
    actions.push_back("commit");
    // ★ 批次 I / ADR 0043:半角标点(英文标点)状态下,引擎吐出来的仍然是中文
    //   标点 —— librime 的标点表**只**由 full_shape 选项选,而本 fork 的两张
    //   表(output/data/default.yaml)都映射中文标点;ascii_punct 只是让
    //   punctuator 处理器短路,翻译路径根本不看它。
    //   ⇒ 在**提交边界**(全仓唯一一处 get_commit)把纯标点提交里的中文标点
    //     换成 ASCII。状态取 live status(与面板图标同一个事实源),读不到
    //     才回落服务端镜像;get_status 是进程内读内存(无 I/O),且只在
    //     「有提交」这一分支才多读一次。
    std::string commit_text = commit.text ? commit.text : "";
    rime_api->free_commit(&commit);
    bool ascii_punct = m_ascii_punct;
    RIME_STRUCT(RimeStatus, punct_status);
    if (rime_api->get_status(session_id, &punct_status)) {
      ascii_punct = punct_status.is_ascii_punct != 0;
      rime_api->free_status(&punct_status);
    }
    if (ascii_punct)
      commit_text = fluxing::punct_text::ApplyAsciiPunctuation(commit_text);
    std::wstring commit_text_w = escape_string(u8tow(commit_text.c_str()));
    body.append(L"commit=").append(commit_text_w).append(L"\n");
    // ★ spec 100 / ADR 0056:提交边界 = 「本地词典可能已改动」的唯一事件源。
    //   get_commit 读后即清（librime ResetCommitText）⇒ 每笔提交恰好触发一次；
    //   该处覆盖自动上屏 / 选词上屏 / 显式 commit 全部路径。
    //   通知只做「置标志 + 唤醒」；网络 I/O 在引擎后台线程
    //   ⇒ 不阻塞输入响应线程（P2）。
    NotifyFluxingLocalChange();
  }

  bool is_composing = false;
  RIME_STRUCT(RimeStatus, status);
  static const std::wstring Bool_wstring[] = {L"0", L"1"};
  if (rime_api->get_status(session_id, &status)) {
    is_composing = !!status.is_composing;
    actions.push_back("status");
    body.append(L"status.ascii_mode=")
        .append(Bool_wstring[!!status.is_ascii_mode])
        .append(L"\n")
        .append(L"status.composing=")
        .append(Bool_wstring[!!status.is_composing])
        .append(L"\n")
        .append(L"status.disabled=")
        .append(Bool_wstring[!!status.is_disabled])
        .append(L"\n")
        .append(L"status.full_shape=")
        .append(Bool_wstring[!!status.is_full_shape])
        .append(L"\n")
        .append(L"status.schema_id=")
        .append(status.schema_id ? u8tow(status.schema_id) : std::wstring())
        .append(L"\n");
    // ADR 0012: the cross-session propagation used to live here. It made a
    // bare Shift in one window switch every other window, because the
    // comparison against the cached status fired on the next response of the
    // toggled session. The propagation now lives in SetOption() and only
    // happens for an explicit control with "global_ascii: true".
    session_status.status = status;
    rime_api->free_status(&status);
  }

  RIME_STRUCT(RimeContext, ctx);
  if (rime_api->get_context(session_id, &ctx)) {
    bool has_candidates = ctx.menu.num_candidates > 0;
    CandidateInfo cinfo;
    if (has_candidates) {
      _GetCandidateInfo(cinfo, ctx, session_id);
    }
    if (is_composing) {
      const auto& preedit = ctx.composition.preedit;
      const auto& start = ctx.composition.sel_start;
      const auto& end = ctx.composition.sel_end;
      const auto& cursor = ctx.composition.cursor_pos;
      static const auto u8towstring = [](const char* u8str, int len = 0) {
        return std::to_wstring(utf8towcslen(u8str, len));
      };
      actions.push_back("ctx");
      switch (session_status.style.preedit_type) {
        case UIStyle::PREVIEW: {
          if (ctx.commit_text_preview) {
            const char* first_utf8 = ctx.commit_text_preview;
            const size_t first_len = std::strlen(first_utf8);
            const std::wstring first_w = escape_string(u8tow(first_utf8));
            const std::wstring tmp = u8towstring(first_utf8, (int)first_len);
            body.append(L"ctx.preedit=")
                .append(first_w)
                .append(L"\n")
                .append(L"ctx.preedit.cursor=")
                .append(u8towstring(first_utf8, 0))
                .append(L",")
                .append(tmp)
                .append(L",")
                .append(tmp)
                .append(L"\n");
            break;
          }
          // no preview, fall back to composition
        }
        case UIStyle::COMPOSITION: {
          body.append(L"ctx.preedit=")
              .append(escape_string(u8tow(preedit)))
              .append(L"\n");
          if (start <= end) {
            body.append(L"ctx.preedit.cursor=")
                .append(u8towstring(preedit, start))
                .append(L",")
                .append(u8towstring(preedit, end))
                .append(L",")
                .append(u8towstring(preedit, cursor))
                .append(L"\n");
          }
          break;
        }
        case UIStyle::PREVIEW_ALL: {
          body.append(L"ctx.preedit=")
              .append(escape_string(u8tow(preedit)))
              .append(L"  [");
          auto label_valid = session_status.style.label_font_point > 0;
          auto comment_valid = session_status.style.comment_font_point > 0;
          const std::wstring mark_text_w =
              session_status.style.mark_text.empty()
                  ? std::wstring(L"*")
                  : session_status.style.mark_text;
          for (auto i = 0; i < ctx.menu.num_candidates; i++) {
            std::wstring label_w;
            if (label_valid) {
              wchar_t buf_lbl[128];
              swprintf_s<128>(buf_lbl,
                              session_status.style.label_text_format.c_str(),
                              cinfo.labels.at(i).str.c_str());
              label_w = std::wstring(buf_lbl);
            }
            std::wstring comment_w =
                comment_valid ? cinfo.comments.at(i).str : std::wstring();
            std::wstring prefix_w = (i != ctx.menu.highlighted_candidate_index)
                                        ? std::wstring()
                                        : mark_text_w;
            body.append(L" ")
                .append(prefix_w)
                .append(escape_string(label_w))
                .append(escape_string(u8tow(ctx.menu.candidates[i].text)))
                .append(L" ")
                .append(escape_string(comment_w));
          }
          body.append(L" ]\n");
          if (start <= end) {
            body.append(L"ctx.preedit.cursor=")
                .append(u8towstring(preedit, start))
                .append(L",")
                .append(u8towstring(preedit, end))
                .append(L",")
                .append(u8towstring(preedit, cursor))
                .append(L"\n");
          }
          break;
        }
      }
    }
    if (has_candidates) {
      std::wstringstream ss;
      boost::archive::text_woarchive oa(ss);

      oa << cinfo;

      auto s = ss.str();
      body.append(L"ctx.cand=").append(std::move(s)).append(L"\n");
    }
    rime_api->free_context(&ctx);
  }

  // configuration information
  actions.push_back("config");
  body.append(L"config.inline_preedit=")
      .append(std::to_wstring((int)session_status.style.inline_preedit))
      .append(L"\n");

  // style
  if (!session_status.__synced) {
    std::wstringstream ss;
    boost::archive::text_woarchive oa(ss);
    oa << session_status.style;

    actions.push_back("style");
    body.append(L"style=").append(ss.str()).append(L"\n");
    session_status.__synced = true;
  }

  // summarize: send header first to avoid vector head-insert cost
  std::wstring header;
  if (actions.empty()) {
    header = L"action=noop\n";
  } else {
    std::string actionList;
    actionList.reserve(64);
    for (size_t i = 0; i < actions.size(); ++i) {
      if (i > 0)
        actionList += ',';
      actionList += actions[i];
    }
    header = std::wstring(L"action=") + u8tow(actionList) + L"\n";
  }
  if (!eat(header))
    return false;

  body.append(L".\n");
  if (!eat(body))
    return false;

  return true;
}

// Blend foreground and background ARGB colors taking alpha into account.
// Returns an ABGR COLORREF with premultiplied alpha blended result.
static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor) {
  // Extract ARGB channels from both colors.
  BYTE fA = (fcolor >> 24) & 0xFF;
  BYTE fB = (fcolor >> 16) & 0xFF;
  BYTE fG = (fcolor >> 8) & 0xFF;
  BYTE fR = fcolor & 0xFF;
  BYTE bA = (bcolor >> 24) & 0xFF;
  BYTE bB = (bcolor >> 16) & 0xFF;
  BYTE bG = (bcolor >> 8) & 0xFF;
  BYTE bR = bcolor & 0xFF;
  // Convert alpha to [0,1]
  float fAlpha = fA / 255.0f;
  float bAlpha = bA / 255.0f;
  // Result alpha
  float retAlpha = fAlpha + (1 - fAlpha) * bAlpha;
  if (retAlpha <= 1e-6f) {
    // Fully transparent result — return background unchanged as fallback.
    return bcolor;
  }
  auto mix = [&](float fc, float bc) -> BYTE {
    return static_cast<BYTE>((fc * fAlpha + bc * bAlpha * (1 - fAlpha)) /
                             retAlpha);
  };
  BYTE retR = mix(fR, bR);
  BYTE retG = mix(fG, bG);
  BYTE retB = mix(fB, bB);
  BYTE outA = static_cast<BYTE>(retAlpha * 255.0f);
  return (static_cast<COLORREF>(outA) << 24) | (retB << 16) | (retG << 8) |
         retR;
}
// parse color value, with fallback value
static Bool _RimeGetColor(RimeConfig* config,
                          const std::string& key,
                          int& value,
                          const ColorFormat& fmt,
                          const unsigned int& fallback) {
  char color[256] = {0};
  if (!rime_api->config_get_string(config, key.c_str(), color, 256)) {
    value = fallback;
    return False;
  }
  const auto color_str = std::string(color);
  // adjudge if str is 0x 0X # hex color format, return trimmed hex part
  // out part is 6 or 8 length hex string without white space
  const auto parse_color_code = [](const std::string& str, std::string& out) {
    if (str.empty())
      return false;
    size_t start = 0;
    if (str[0] == '#') {
      start = 1;
    } else if (str.size() >= 2 &&
               (str.compare(0, 2, "0x") == 0 || str.compare(0, 2, "0X") == 0)) {
      start = 2;
    } else {
      return false;
    }
    const std::string hex_part = str.substr(start);
    if (hex_part.empty())
      return false;
    if ((start == 1 || start == 2) && hex_part.length() != 3 &&
        hex_part.length() != 4 && hex_part.length() != 6 &&
        hex_part.length() != 8) {
      return false;
    }
    for (char c : hex_part) {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')))
        return false;
    }
    out = str.substr(start).substr(0, 8);
#define _2C(c) std::string(2, c)
    if (out.size() == 3)
      out = _2C(out[0]) + _2C(out[1]) + _2C(out[2]);
    else if (out.size() == 4)
      out = _2C(out[0]) + _2C(out[1]) + _2C(out[2]) + _2C(out[3]);
#undef _2C
    return true;
  };
  auto hex_color = std::string();
  if (parse_color_code(color_str, hex_color)) {
    value = std::stoul(hex_color, 0, 16);
    if (hex_color.length() == 6)
      value = (fmt != COLOR_RGBA) ? (value | 0xff000000)
                                  : (((unsigned int)value << 8) | 0x000000ff);
  } else {
    if (!rime_api->config_get_int(config, key.c_str(), &value)) {
      value = fallback;
      return False;
    }
    if (value <= 0xffffff)
      value = (fmt != COLOR_RGBA) ? (value | 0xff000000)
                                  : (((unsigned int)value << 8) | 0x000000ff);
    else if (value > 0xffffffff)
      value &= 0xffffffff;
  }
  if (fmt == COLOR_ARGB)
    value = ARGB2ABGR(value);
  else if (fmt == COLOR_RGBA)
    value = RGBA2ABGR(value);
  value &= 0xffffffff;
  return True;
}

template <typename T, size_t N>
using Array = std::array<std::pair<const char*, T>, N>;

// parset bool type configuration to T type value trueValue / falseValue
template <typename T>
void _RimeGetBool(RimeConfig* config,
                  const char* key,
                  bool cond,
                  T& value,
                  const T& trueValue = true,
                  const T& falseValue = false) {
  Bool tempb = False;
  if (rime_api->config_get_bool(config, key, &tempb) || cond)
    value = (!!tempb) ? trueValue : falseValue;
}
// parse string option to T type value, with fallback
template <typename T, size_t N>
void _RimeParseStringOptWithFallback(RimeConfig* config,
                                     const char* key,
                                     T& value,
                                     const Array<T, N>& arr,
                                     const T& fallback) {
  char str_buff[256] = {0};
  if (rime_api->config_get_string(config, key, str_buff, 255)) {
    for (size_t i = 0; i < N; ++i) {
      if (strcmp(arr[i].first, str_buff) == 0) {
        value = arr[i].second;
        return;
      }
    }
  }
  value = fallback;
}

template <typename T>
void _RimeGetIntStr(RimeConfig* config,
                    const char* key,
                    T& value,
                    const char* fb_key = nullptr,
                    const void* fb_value = nullptr,
                    const std::function<void(T&)>& func = nullptr) {
  if constexpr (std::is_same<T, int>::value) {
    if (!rime_api->config_get_int(config, key, &value) && fb_key != 0)
      rime_api->config_get_int(config, fb_key, &value);
  } else if constexpr (std::is_same<T, std::wstring>::value) {
    const int BUF_SIZE = 2047;
    char buffer[BUF_SIZE + 1] = {0};
    if (rime_api->config_get_string(config, key, buffer, BUF_SIZE) ||
        rime_api->config_get_string(config, fb_key, buffer, BUF_SIZE)) {
      value = u8tow(buffer);
    } else if (fb_value) {
      value = *(T*)fb_value;
    }
  }
  if (func)
    func(value);
}

// Helper to iterate a Rime map and invoke callback with key/path
static void ForEachRimeMap(
    RimeConfig* config,
    const std::string& path,
    const std::function<void(const char* key, const char* child_path)>& cb) {
  RimeConfigIterator iter;
  if (!rime_api->config_begin_map(&iter, config, path.c_str()))
    return;
  while (rime_api->config_next(&iter)) {
    cb(iter.key, iter.path);
  }
  rime_api->config_end(&iter);
}

// Helper to iterate a Rime list and invoke callback with item path
static void ForEachRimeList(
    RimeConfig* config,
    const std::string& path,
    const std::function<void(const char* item_path)>& cb) {
  RimeConfigIterator iter;
  if (!rime_api->config_begin_list(&iter, config, path.c_str()))
    return;
  while (rime_api->config_next(&iter)) {
    cb(iter.path);
  }
  rime_api->config_end(&iter);
}

void RimeWithWeaselHandler::_UpdateShowNotifications(RimeConfig* config,
                                                     bool initialize) {
  Bool show_notifications = true;
  if (initialize)
    m_show_notifications_base.clear();
  m_show_notifications.clear();

  if (rime_api->config_get_bool(config, "show_notifications",
                                &show_notifications)) {
    // config read as bool, for global all on or off
    if (show_notifications)
      m_show_notifications["always"] = true;
    if (initialize)
      m_show_notifications_base = m_show_notifications;
  } else {
    // read as list using helper
    ForEachRimeList(config, "show_notifications", [&](const char* item_path) {
      char buffer[256] = {0};
      if (rime_api->config_get_string(config, item_path, buffer, 256))
        m_show_notifications[std::string(buffer)] = true;
    });
    if (initialize)
      m_show_notifications_base = m_show_notifications;
    if (m_show_notifications.empty()) {
      // not configured, or incorrect type
      if (initialize)
        m_show_notifications_base["always"] = true;
      m_show_notifications = m_show_notifications_base;
    }
  }
}

// update ui's style parameters, ui has been check before referenced
static void _UpdateUIStyle(RimeConfig* config, UI* ui, bool initialize) {
  UIStyle& style(ui->style());
  const std::function<void(std::wstring&)> rmspace = [](std::wstring& str) {
    str = std::regex_replace(str, std::wregex(L"\\s*(,|:|^|$)\\s*"), L"$1");
  };
  const std::function<void(int&)> _abs = [](int& value) { value = abs(value); };
  // get font faces
  _RimeGetIntStr(config, "style/font_face", style.font_face, 0, 0, rmspace);
  std::wstring* const pFallbackFontFace = initialize ? &style.font_face : NULL;
  _RimeGetIntStr(config, "style/label_font_face", style.label_font_face, 0,
                 pFallbackFontFace, rmspace);
  _RimeGetIntStr(config, "style/comment_font_face", style.comment_font_face, 0,
                 pFallbackFontFace, rmspace);
  // able to set label font/comment font empty, force fallback to font face.
  if (style.label_font_face.empty())
    style.label_font_face = style.font_face;
  if (style.comment_font_face.empty())
    style.comment_font_face = style.font_face;
  // get font points
  _RimeGetIntStr(config, "style/font_point", style.font_point);
  if (style.font_point <= 0)
    style.font_point = 12;
  _RimeGetIntStr(config, "style/label_font_point", style.label_font_point,
                 "style/font_point", 0, _abs);
  _RimeGetIntStr(config, "style/comment_font_point", style.comment_font_point,
                 "style/font_point", 0, _abs);
  _RimeGetIntStr(config, "style/candidate_abbreviate_length",
                 style.candidate_abbreviate_length, 0, 0, _abs);
  _RimeGetBool(config, "style/inline_preedit", initialize,
               style.inline_preedit);
  _RimeGetBool(config, "style/vertical_auto_reverse", initialize,
               style.vertical_auto_reverse);
  static constexpr Array<UIStyle::PreeditType, 3> _preeditArr = {
      {{"composition", UIStyle::COMPOSITION},
       {"preview", UIStyle::PREVIEW},
       {"preview_all", UIStyle::PREVIEW_ALL}}};
  _RimeParseStringOptWithFallback(config, "style/preedit_type",
                                  style.preedit_type, _preeditArr,
                                  style.preedit_type);
  static constexpr Array<UIStyle::AntiAliasMode, 5> _aliasModeArr = {
      {{"force_dword", UIStyle::FORCE_DWORD},
       {"cleartype", UIStyle::CLEARTYPE},
       {"grayscale", UIStyle::GRAYSCALE},
       {"aliased", UIStyle::ALIASED},
       {"default", UIStyle::DEFAULT}}};
  _RimeParseStringOptWithFallback(config, "style/antialias_mode",
                                  style.antialias_mode, _aliasModeArr,
                                  style.antialias_mode);
  static constexpr Array<UIStyle::HoverType, 3> _hoverTypeArr = {
      {{"none", UIStyle::HoverType::NONE},
       {"semi_hilite", UIStyle::HoverType::SEMI_HILITE},
       {"hilite", UIStyle::HoverType::HILITE}}};
  _RimeParseStringOptWithFallback(config, "style/hover_type", style.hover_type,
                                  _hoverTypeArr, style.hover_type);
  static constexpr Array<UIStyle::LayoutAlignType, 3> _alignType = {
      {{"top", UIStyle::ALIGN_TOP},
       {"center", UIStyle::ALIGN_CENTER},
       {"bottom", UIStyle::ALIGN_BOTTOM}}};
  _RimeParseStringOptWithFallback(config, "style/layout/align_type",
                                  style.align_type, _alignType,
                                  style.align_type);
  _RimeGetBool(config, "style/display_tray_icon", initialize,
               style.display_tray_icon);
  _RimeGetBool(config, "style/ascii_tip_follow_cursor", initialize,
               style.ascii_tip_follow_cursor);
  _RimeGetBool(config, "style/horizontal", initialize, style.layout_type,
               UIStyle::LAYOUT_HORIZONTAL, UIStyle::LAYOUT_VERTICAL);
  _RimeGetBool(config, "style/paging_on_scroll", initialize,
               style.paging_on_scroll);
  _RimeGetBool(config, "style/click_to_capture", initialize,
               style.click_to_capture, true, false);
  _RimeGetBool(config, "style/fullscreen", false, style.layout_type,
               ((style.layout_type == UIStyle::LAYOUT_HORIZONTAL)
                    ? UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN
                    : UIStyle::LAYOUT_VERTICAL_FULLSCREEN),
               style.layout_type);
  _RimeGetBool(config, "style/vertical_text", false, style.layout_type,
               UIStyle::LAYOUT_VERTICAL_TEXT, style.layout_type);
  _RimeGetBool(config, "style/vertical_text_left_to_right", false,
               style.vertical_text_left_to_right);
  _RimeGetBool(config, "style/vertical_text_with_wrap", false,
               style.vertical_text_with_wrap);
  static constexpr Array<bool, 2> _text_orientation = {
      {{"horizontal", false}, {"vertical", true}}};
  bool _text_orientation_bool = false;
  _RimeParseStringOptWithFallback(config, "style/text_orientation",
                                  _text_orientation_bool, _text_orientation,
                                  _text_orientation_bool);
  if (_text_orientation_bool)
    style.layout_type = UIStyle::LAYOUT_VERTICAL_TEXT;
  _RimeGetIntStr(config, "style/label_format", style.label_text_format);
  _RimeGetIntStr(config, "style/mark_text", style.mark_text);
  _RimeGetIntStr(config, "style/layout/baseline", style.baseline, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/linespacing", style.linespacing, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/min_width", style.min_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/max_width", style.max_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/min_height", style.min_height, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/max_height", style.max_height, 0, 0,
                 _abs);
  // layout (alternative to style/horizontal)
  static constexpr Array<UIStyle::LayoutType, 5> _layoutArr = {
      {{"vertical", UIStyle::LAYOUT_VERTICAL},
       {"horizontal", UIStyle::LAYOUT_HORIZONTAL},
       {"vertical_text", UIStyle::LAYOUT_VERTICAL_TEXT},
       {"vertical+fullscreen", UIStyle::LAYOUT_VERTICAL_FULLSCREEN},
       {"horizontal+fullscreen", UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN}}};
  _RimeParseStringOptWithFallback(config, "style/layout/type",
                                  style.layout_type, _layoutArr,
                                  style.layout_type);
  // disable max_width when full screen
  if (style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN ||
      style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN) {
    style.max_width = 0;
    style.inline_preedit = false;
  }
  _RimeGetIntStr(config, "style/layout/border", style.border,
                 "style/layout/border_width", 0, _abs);
  _RimeGetIntStr(config, "style/layout/margin_x", style.margin_x);
  _RimeGetIntStr(config, "style/layout/margin_y", style.margin_y);
  _RimeGetIntStr(config, "style/layout/spacing", style.spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/candidate_spacing",
                 style.candidate_spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_spacing", style.hilite_spacing, 0,
                 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_x",
                 style.hilite_padding_x, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_y",
                 style.hilite_padding_y, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/shadow_radius", style.shadow_radius, 0,
                 0, _abs);
  // disable shadow for fullscreen layout
  style.shadow_radius *=
      (!(style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN ||
         style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN));
  _RimeGetIntStr(config, "style/layout/shadow_offset_x", style.shadow_offset_x);
  _RimeGetIntStr(config, "style/layout/shadow_offset_y", style.shadow_offset_y);
  // round_corner as alias of hilited_corner_radius
  _RimeGetIntStr(config, "style/layout/hilited_corner_radius",
                 style.round_corner, "style/layout/round_corner", 0, _abs);
  // corner_radius not set, fallback to round_corner
  _RimeGetIntStr(config, "style/layout/corner_radius", style.round_corner_ex,
                 "style/layout/round_corner", 0, _abs);
  // fix padding and spacing settings
  if (style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT) {
    // hilite_padding vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = max(style.spacing, style.hilite_padding_y * 2);
    // hilite_padding vs candidate_spacing
    if (style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN ||
        style.layout_type == UIStyle::LAYOUT_VERTICAL) {
      // vertical, if hilite_padding_y over candidate spacing,
      // increase candidate spacing
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_y * 2);
    } else {
      // horizontal, if hilite_padding_x over candidate
      // spacing, increase candidate spacing
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_x * 2);
    }
    // hilite_padding_x vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = max(style.hilite_spacing, style.hilite_padding_x);
  } else  // LAYOUT_VERTICAL_TEXT
  {
    // hilite_padding_x vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = max(style.spacing, style.hilite_padding_x * 2);
    // hilite_padding vs candidate_spacing
    // if hilite_padding_x over candidate
    // spacing, increase candidate spacing
    style.candidate_spacing =
        max(style.candidate_spacing, style.hilite_padding_x * 2);
    // vertical_text_with_wrap and hilite_padding_y over candidate_spacing
    if (style.vertical_text_with_wrap)
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_y * 2);
    // hilite_padding_y vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = max(style.hilite_spacing, style.hilite_padding_y);
  }
  // fix padding and margin settings
  int scale = style.margin_x < 0 ? -1 : 1;
  style.margin_x = scale * max(style.hilite_padding_x, abs(style.margin_x));
  scale = style.margin_y < 0 ? -1 : 1;
  style.margin_y = scale * max(style.hilite_padding_y, abs(style.margin_y));
  // get enhanced_position
  _RimeGetBool(config, "style/enhanced_position", initialize,
               style.enhanced_position, true, false);
  // get color scheme
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (initialize && rime_api->config_get_string(config, "style/color_scheme",
                                                buffer, BUF_SIZE))
    _UpdateUIStyleColor(config, style);
}
// load color configs to style, by "style/color_scheme" or specific scheme name
// "color" which is default empty
static bool _UpdateUIStyleColor(RimeConfig* config,
                                UIStyle& style,
                                const std::string& color) {
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  std::string color_mark = "style/color_scheme";
  // color scheme
  if (rime_api->config_get_string(config, color_mark.c_str(), buffer,
                                  BUF_SIZE) ||
      !color.empty()) {
    std::string prefix("preset_color_schemes/");
    prefix += (color.empty()) ? buffer : color;
    // define color format, default abgr if not set
    ColorFormat fmt = COLOR_ABGR;
    static constexpr Array<ColorFormat, 3> _colorFmt = {
        {{"argb", COLOR_ARGB}, {"rgba", COLOR_RGBA}, {"abgr", COLOR_ABGR}}};
    _RimeParseStringOptWithFallback(config, (prefix + "/color_format").c_str(),
                                    fmt, _colorFmt, COLOR_ABGR);
#define COLOR(key, value, fallback) \
  _RimeGetColor(config, (prefix + "/" + key), value, fmt, fallback)
    COLOR("back_color", style.back_color, 0xffffffff);
    COLOR("shadow_color", style.shadow_color, 0);
    COLOR("prevpage_color", style.prevpage_color, 0);
    COLOR("nextpage_color", style.nextpage_color, 0);
    COLOR("text_color", style.text_color, 0xff000000);
    COLOR("candidate_text_color", style.candidate_text_color, style.text_color);
    COLOR("candidate_back_color", style.candidate_back_color, 0);
    COLOR("border_color", style.border_color, style.text_color);
    COLOR("hilited_text_color", style.hilited_text_color, style.text_color);
    COLOR("hilited_back_color", style.hilited_back_color, style.back_color);
    COLOR("hilited_candidate_text_color", style.hilited_candidate_text_color,
          style.hilited_text_color);
    COLOR("hilited_candidate_back_color", style.hilited_candidate_back_color,
          style.hilited_back_color);
    COLOR("hilited_candidate_shadow_color",
          style.hilited_candidate_shadow_color, 0);
    COLOR("hilited_shadow_color", style.hilited_shadow_color, 0);
    COLOR("candidate_shadow_color", style.candidate_shadow_color, 0);
    COLOR("candidate_border_color", style.candidate_border_color, 0);
    COLOR("hilited_candidate_border_color",
          style.hilited_candidate_border_color, 0);
    COLOR("label_color", style.label_text_color,
          blend_colors(style.candidate_text_color, style.candidate_back_color));
    COLOR("hilited_label_color", style.hilited_label_text_color,
          blend_colors(style.hilited_candidate_text_color,
                       style.hilited_candidate_back_color));
    COLOR("comment_text_color", style.comment_text_color,
          style.label_text_color);
    COLOR("hilited_comment_text_color", style.hilited_comment_text_color,
          style.hilited_label_text_color);
    COLOR("hilited_mark_color", style.hilited_mark_color, 0);
#undef COLOR
    return true;
  }
  return false;
}
static void _LoadAppOptions(RimeConfig* config,
                            AppOptionsByAppName& app_options) {
  app_options.clear();
  ForEachRimeMap(
      config, "app_options", [&](const char* app_key, const char* app_path) {
        AppOptions& options(app_options[app_key]);
        ForEachRimeMap(
            config, app_path, [&](const char* opt_key, const char* opt_path) {
              Bool value = False;
              if (rime_api->config_get_bool(config, opt_path, &value)) {
                options[opt_key] = !!value;
              }
            });
      });
}

void RimeWithWeaselHandler::_GetStatus(Status& stat,
                                       WeaselSessionId ipc_id,
                                       Context& ctx) {
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  RIME_STRUCT(RimeStatus, status);
  if (rime_api->get_status(session_id, &status)) {
    std::string schema_id = "";
    if (status.schema_id)
      schema_id = status.schema_id;
    stat.schema_name = u8tow(status.schema_name);
    stat.schema_id = u8tow(status.schema_id);
    stat.ascii_mode = !!status.is_ascii_mode;
    stat.composing = !!status.is_composing;
    stat.disabled = !!status.is_disabled;
    stat.full_shape = !!status.is_full_shape;
    if (schema_id != m_last_schema_id) {
      session_status.__synced = false;
      m_last_schema_id = schema_id;
      if (schema_id != ".default") {  // don't load for schema select menu
        bool inline_preedit = session_status.style.inline_preedit;
        _LoadSchemaSpecificSettings(ipc_id, schema_id);
        _LoadAppInlinePreeditSet(ipc_id, true);
        if (session_status.style.inline_preedit != inline_preedit)
          // in case of inline_preedit set in schema
          _UpdateInlinePreeditStatus(ipc_id);
        // refresh icon after schema changed
        _RefreshTrayIcon(session_id, _UpdateUICallback);
        m_ui->style() = session_status.style;
        if (m_show_notifications.find("schema") != m_show_notifications.end() &&
            m_show_notifications_time > 0) {
          ctx.aux.str = stat.schema_name;
          m_ui->Update(ctx, stat);
          m_ui->ShowWithTimeout(m_show_notifications_time);
        }
      }
    }
    rime_api->free_status(&status);
  }
}

void RimeWithWeaselHandler::_GetContext(Context& weasel_context,
                                        RimeSessionId session_id) {
  RIME_STRUCT(RimeContext, ctx);
  if (rime_api->get_context(session_id, &ctx)) {
    if (ctx.composition.length > 0) {
      weasel_context.preedit.str = u8tow(ctx.composition.preedit);
      if (ctx.composition.sel_start < ctx.composition.sel_end) {
        TextAttribute attr;
        attr.type = HIGHLIGHTED;
        attr.range.start =
            utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
        attr.range.end =
            utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);

        weasel_context.preedit.attributes.push_back(attr);
      }
    }
    if (ctx.menu.num_candidates) {
      CandidateInfo& cinfo(weasel_context.cinfo);
      _GetCandidateInfo(cinfo, ctx, session_id);
    }
    rime_api->free_context(&ctx);
  }
}

void RimeWithWeaselHandler::_UpdateInlinePreeditStatus(WeaselSessionId ipc_id) {
  if (!m_ui)
    return;
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  // set inline_preedit option
  bool inline_preedit = session_status.style.inline_preedit;
  rime_api->set_option(session_id, "inline_preedit", Bool(inline_preedit));
  // show soft cursor on weasel panel but not inline
  rime_api->set_option(session_id, "soft_cursor", Bool(!inline_preedit));
}

bool RimeWithWeaselHandler::IsSimplification() const {
  if (m_cached_options)
    return m_simplification;
  RimeConfig config = {0};
  Bool v = False;
  if (rime_api && rime_api->user_config_open("default", &config)) {
    if (rime_api->config_get_bool(&config, "simplification", &v))
      m_simplification = !!v;
    rime_api->config_close(&config);
  }
  m_cached_options = true;
  return m_simplification;
}

// 批次 H:解析目标会话(顺序与理由见 RimeWithWeasel.h 的声明处注释)。
RimeSessionId RimeWithWeaselHandler::_ResolveTargetSession(
    WeaselSessionId ipc_id) {
  if (ipc_id) {
    auto it = m_session_status_map.find(ipc_id);
    if (it != m_session_status_map.end() && it->second.session_id)
      return it->second.session_id;
    return 0;
  }
  auto it = m_session_status_map.find(m_active_session);
  if (it != m_session_status_map.end() && it->second.session_id)
    return it->second.session_id;
  // 批次 H 第三档:最近一次拿到输入焦点的会话。
  // m_active_session 会被 FocusOut 清零,而面板轮询 / 用户点击完全可能发生在
  // 那之后 —— 没有这一档就永远读不到 live 状态(用户需求③的症状)。
  it = m_session_status_map.find(m_last_focus_session);
  if (it != m_session_status_map.end() && it->second.session_id)
    return it->second.session_id;
  // 回退:只有恰好一个**有效**会话时,才能确定用户指的是谁。
  //
  // ★ 2026-11-09 修:不能直接用 m_session_status_map.size() == 1 ——
  //   历史上 to_session_id() 的 operator[] 副作用会在 map 里插入 key=0 的
  //   假会话(session_id=0),那时 size 变成 2 ⇒ 唯一会话回退永久失效。
  //   改为**只统计 session_id 有效的条目**。
  RimeSessionId only_session = 0;
  size_t valid_count = 0;
  for (const auto& pair : m_session_status_map) {
    if (!pair.second.session_id)
      continue;
    ++valid_count;
    only_session = pair.second.session_id;
  }
  if (valid_count == 1)
    return only_session;
  return 0;
}

void RimeWithWeaselHandler::_ApplyPunctWidth(RimeSessionId session_id,
                                              bool halfwidth) {
  if (!rime_api || !session_id)
    return;
  // 半角 ⇒ ascii_punct=true:librime 的 punctuator 在这个 option 为真时直接
  // `return kNoop`(librime/src/rime/gear/punctuator.cc)⇒ 标点**原样直出
  // ASCII**。这是既有开关里唯一能真正打出英文标点的那个
  // (full_shape / half_shape 两张表在本 fork 里都映射中文标点)。
  rime_api->set_option(session_id, "ascii_punct", halfwidth ? True : False);
  // ★ 批次 H 语义纠正(2026-11-07,用户原话):「我希望的全角,只是中文标点
  //   符号。但数字＼其他符号等,还应该和英文状态一致,应该还是半角的。
  //   参照搜狗输入法等其他输入法的设置。」
  //   ⇒ **标点宽度不再驱动 full_shape**。full_shape 一开,数字与符号会被
  //     librime 换成全角形态,正是用户不要的那个行为。v9 / ADR 0040 把
  //     「全角 = full_shape = !ascii_punct」耦合在一起,那条口径作废
  //     (改由 ADR 0041 记录):「全角」从此**只表示中文标点** = ascii_punct 关。
  //   ⇒ 仍然要**清 0**,不能只是「不写」:旧版本由面板按钮置上的
  //     full_shape=true 已被 default.yaml 的 switcher/save_options 持久化进
  //     user.yaml。librime 里 full_shape 一开,`punctuator/full_shape/` 那张
  //     表就生效(空格变全角空格等),正是用户不要的行为。
  //   ⇒ **两个分支都清**:不能只在「选中文标点」时清 —— 用户机器上残留的
  //     full_shape=true,即使他先按「英文标点」也要一并清掉,否则按了英文
  //     标点还会打出全角空格。清 0 幂等,不改变已经正确的状态。
  //   ⇒ **永不置 1**:full_shape 只管数字/符号的形态,与「标点语言」无关。
  //   ⇒ 只在当前值不同才写,否则每次 Shift 都要多落一次 user.yaml。
  if (rime_api->get_option(session_id, "full_shape") != False)
    rime_api->set_option(session_id, "full_shape", False);
  m_full_shape = false;
  // 服务端镜像(面板读不到 live 状态时用它兜底)。
  m_ascii_punct = halfwidth;
}

// v6 需求② + 批次 H: 面板的「生效输入态」。
//
// 缺陷背景(用户 2026-11-07 报告):「全半角符号随中英文切换自动变化,设置栏的
// 图标也应该变化;当前这个键不会改变」。两轮根因:
//   v6:面板只拿到 IsFullShape()—— full_shape 这个 option 在中英文切换时
//       **并不变**,变的是 punctuator 分支(英文态走 ascii 分支 ⇒ 半角);
//   批次 H:面板的 live 读取入口只有 m_active_session,而它会被 FocusOut
//       清零 ⇒ 面板读不到 live 状态,只能回落到陈旧镜像 ⇒「按了没反应」
//       (用户原话:「当前不仅全半角符号不切换,中英文标志也没有切换」)。
//
// 现在的口径:**「全角」= 生效标点是中文全角标点** = !ascii_mode && !ascii_punct
//   - 英文态:ascii_composer 直接提交 ASCII ⇒ 标点自然是英文 ⇒ 记半角;
//   - 中文态 + ascii_punct:punctuator kNoop ⇒ 原样 ASCII ⇒ 也是半角;
//   - 中文态 + 无 ascii_punct:中文标点表 ⇒ 全角。
//
// 实现约束(勿简化,均为已踩过的坑):
//   - **不调 IsFullShape()**:它首次调用要 rime_api->user_config_open(文件 I/O),
//     而本函数由显示路径每 100ms 调一次,会撞 constitution P2(显示路径零 I/O);
//   - **不写 to_session_id(...)**:它内部是 `m_session_status_map[ipc_id]` ——
//     键不存在时会**插入**一个空 status,把「只有一个会话」的事实破坏掉
//     (SetOption 的唯一会话回退就靠它);统一走 _ResolveTargetSession。
//   - get_status 是进程内 C API 读内存(与 ProcessKeyEvent 热路径同形态),
//     拿到结果必须 free_status,否则泄漏 status 内部指针。
//   - 这里**只读**状态、**绝不**调 set_option —— set_option 会触发 switcher
//     写 user.yaml(文件 I/O),出现在 100ms 轮询里就是 P2 违规。
void RimeWithWeaselHandler::GetPanelInputState(bool* ascii, bool* fullwidth) {
  bool a = m_global_ascii_mode;
  bool p = m_ascii_punct;

  const RimeSessionId sid = _ResolveTargetSession(0);
  if (rime_api && sid) {
    RimeStatus status = {0};
    if (rime_api->get_status(sid, &status)) {
      const bool live_ascii = status.is_ascii_mode != 0;
      const bool live_full = status.is_full_shape != 0;
      const bool live_punct = status.is_ascii_punct != 0;
      rime_api->free_status(&status);
      // 顺手校正镜像:用户按 Shift 切中英、短语触发的开关都走不到 SetOption,
      // 只有这里校正,面板下一帧才会跟着变。
      m_global_ascii_mode = live_ascii;
      m_full_shape = live_full;
      m_ascii_punct = live_punct;
      a = live_ascii;
      p = live_punct;
      // 注意:这里**不能**置 m_cached_options —— 那个标志同时管
      // IsSimplification(),置 true 会让简繁状态被误报成 false。
    }
  }

  if (ascii)
    *ascii = a;
  if (fullwidth)
    *fullwidth = fluxing::panel_status::ShapeIsFullwidth(a, p);
}

bool RimeWithWeaselHandler::IsFullShape() const {
  if (m_cached_options)
    return m_full_shape;
  RimeConfig config = {0};
  Bool v = False;
  if (rime_api && rime_api->user_config_open("default", &config)) {
    if (rime_api->config_get_bool(&config, "full_shape", &v))
      m_full_shape = !!v;
    rime_api->config_close(&config);
  }
  m_cached_options = true;
  return m_full_shape;
}

// spec 077: Shift release-only candidate select via IPC state machine.
// State lives per-WeaselSessionId in m_shiftState; rime_api->select_candidate
// is called only on a clean ShiftUp (no intervening non-Shift key between
// matching Down and Up). On any Up the state is unconditionally reset (C6)
// to prevent subsequent Shift events from misfiring.
void RimeWithWeaselHandler::ShiftDown(bool is_left,
                                      WeaselSessionId ipc_id,
                                      EatLine eat) {
  TryLazyRecovery();
  if (m_disabled)
    return;
  DLOG(INFO) << "ShiftDown: is_left=" << is_left << ", ipc_id=" << ipc_id;
  auto& s = m_shiftState[ipc_id];
  s.downRecorded = true;
  s.interveningKey = false;
  // ADR 0017 D2: the swap switch flips the side when the host reports the
  // two shift keys the other way round.
  s.lastIsLeft = fluxing::ApplyShiftSideSwap(is_left, m_shift_left_right_swap);
  // ADR 0012: answer with a body. The TSF reads the pipe buffer right after
  // the Shift key event (OnTestKeyDown -> _UpdateComposition -> DoEditSession);
  // without a fresh body it would parse the previous response again.
  _Respond(ipc_id, eat);
}

void RimeWithWeaselHandler::ShiftUp(bool is_left,
                                    WeaselSessionId ipc_id,
                                    EatLine eat) {
  TryLazyRecovery();
  if (m_disabled)
    return;
  DLOG(INFO) << "ShiftUp: is_left=" << is_left << ", ipc_id=" << ipc_id;
  (void)is_left;
  auto it = m_shiftState.find(ipc_id);
  if (it == m_shiftState.end())
    return;
  auto& s = it->second;
  // ADR 0010: bare Shift disambiguation.
  //
  // A clean Shift release (Down -> Up with no intervening key) carries two
  // possible intents:
  //   - spec 077: select the 2nd / 3rd candidate (left / right Shift)
  //   - ADR 0010: toggle the Chinese / English input mode
  // They are separated by a single observable predicate -- whether this
  // session currently shows a candidate menu. No timing heuristic and no
  // double-press escape hatch (ADR 0010 P3).
  bool menu_open = false;
  bool is_ascii_mode = false;
  // spec 090: 当前页引擎候选数,在 free_context 之前拷出,供页内索引校验。
  int shift_page_candidate_count = 0;
  RIME_STRUCT(RimeStatus, shift_status);
  if (rime_api->get_status(to_session_id(ipc_id), &shift_status)) {
    is_ascii_mode = !!shift_status.is_ascii_mode;
    rime_api->free_status(&shift_status);
  }
  RIME_STRUCT(RimeContext, shift_ctx);
  if (rime_api->get_context(to_session_id(ipc_id), &shift_ctx)) {
    // librime keeps a one-candidate "raw" segment while ascii_mode is on
    // (librime/src/rime/gear/ascii_segmentor.cc) and masks has_menu out in
    // ascii mode (librime/src/rime/gear/key_binder.cc:255). The menu alone is
    // therefore not a sufficient predicate: in English mode the user sees no
    // candidate list at all.
    menu_open = shift_ctx.menu.num_candidates > 0 && !is_ascii_mode;
    // spec 090: free_context 会释放 menu.candidates,页内候选数必须先拷出。
    // free_context 本身的位置与调用次数保持原样(挪动会泄漏或 double free)。
    shift_page_candidate_count = shift_ctx.menu.num_candidates;
    rime_api->free_context(&shift_ctx);
  }
  fluxing::ShiftUpAction action = fluxing::DecideShiftUpAction(
      s.downRecorded, s.interveningKey, menu_open, m_bare_shift_toggles_ascii);
  // spec 090 / ADR 0023 D1-D3(缺陷 A 修复): 左/右 Shift 选「当前页」第 2/3
  // 个候选。原来走全局次序 select_candidate(idx):全局索引 1/2 永远指向第 1
  // 页的第 2/3 个,翻页之后 Shift 上屏的仍是第一页的词。两个 API 都是
  // 0-based;select_candidate_on_current_page(sid, i) 选当前页第 i+1 个可见
  // 候选(rime_api_impl.h do_with_candidate_on_current_page = page_start +
  // index;上游 rime_api.h:467-473 对两个 API 的注释文字相同、与实现不符,
  // 采信实现)。屏蔽名单非空时 idx 是「可见次序」,还须经 m_visibleToEngineIdx
  // 换算成引擎页内次序(与数字键同一条路,见 _MapVisibleIndexToEngine)。
  size_t shift_engine_idx = 0;
  bool shift_can_select = false;
  if (action == fluxing::ShiftUpAction::kSelectCandidate) {
    const size_t idx = s.lastIsLeft ? 1 : 2;
    const int shift_visible_count =
        m_ignoreList.empty() ? shift_page_candidate_count
                             : static_cast<int>(m_visibleToEngineIdx.size());
    // 越界(这一页没有第 N 个候选)就整段跳过:不选词、不上屏、不 flush、
    // 不回包,函数尾照常重置 ShiftState。明确禁止「退回全局
    // select_candidate」的 fallback —— 那会选中屏幕上没看见的词,比不选
    // 更错(ADR 0023 D2 已记录该否决)。
    shift_can_select = fluxing::IsPageLocalIndexValid(static_cast<int>(idx),
                                                      shift_visible_count) &&
                       _MapVisibleIndexToEngine(idx, &shift_engine_idx);
    if (!shift_can_select)
      DLOG(INFO) << "ShiftUp select skipped: visible=" << shift_visible_count
                 << ", idx=" << idx << ", ipc_id=" << ipc_id;
  }
  if (action == fluxing::ShiftUpAction::kSelectCandidate && shift_can_select) {
    DLOG(INFO) << "select_candidate_on_current_page(idx=" << shift_engine_idx
               << ") ipc_id=" << ipc_id;
    rime_api->select_candidate_on_current_page(to_session_id(ipc_id),
                                               shift_engine_idx);
    // ADR 0012: the selected candidate has to reach the screen. With
    // _auto_commit off, librime's Context::Select only forwards the segment
    // (librime/src/rime/engine.cc:259-270), so a *full-word* pick leaves the
    // composition finished-but-uncommitted and nothing is committed.
    // commit_composition is idempotent: with _auto_commit on, Select already
    // committed and cleared the composition, so this call returns false.
    // spec 087 / ADR 0018: for a *partial* pick — a single character out of a
    // multi-syllable shuangpin input — commit_composition instead flushes the
    // whole context (librime context.cc:18-26: Context::Commit =
    // commit_notifier + Clear), dragging the head candidate of the remaining
    // segment onto the screen next to the picked character. Numeric-key
    // selection (SelectCandidate(), which does not commit) has no such
    // defect, so mirror it: flush only when no candidate menu remains.
    bool flush = true;
    RIME_STRUCT(RimeContext, post_ctx);
    if (rime_api->get_context(to_session_id(ipc_id), &post_ctx)) {
      flush = fluxing::ShouldCommitAfterSelect(true, post_ctx.menu.num_candidates);
      rime_api->free_context(&post_ctx);
    }
    DLOG(INFO) << "ShiftUp select commit_flush=" << flush
               << " ipc_id = " << ipc_id;
    if (flush) {
      rime_api->commit_composition(to_session_id(ipc_id));
    }
    // ADR 0012: answer with a body. It carries the commit text (the selected
    // candidate) and the fresh status, so the client inserts the text and
    // updates the language bar. _Respond has to run before _UpdateUI, exactly
    // like in ProcessKeyEvent, so that the cached status is fresh when the
    // tray icon is refreshed.
    _Respond(ipc_id, eat);
    _UpdateUI(ipc_id);
  } else if (action == fluxing::ShiftUpAction::kToggleAsciiMode) {
    // ADR 0010 P2: the current session only. Never the global mirror, never
    // another session -- exactly the scope of the yaml `Shift+space` toggle.
    bool ascii = rime_api->get_option(to_session_id(ipc_id), "ascii_mode");
    rime_api->set_option(to_session_id(ipc_id), "ascii_mode", !ascii);
    // 批次 H(用户需求③):「使用 shift 快捷键切换中英文,标点符号也会同步切换
    // 全半角」⇒ 切中英文时**同时**拨标点宽度:
    //   切到英文 ⇒ 半角(英文标点);切回中文 ⇒ 全角(中文标点)。
    // 落在 ascii_punct 上(半角 = ascii_punct=true ⇒ punctuator kNoop ⇒
    // 标点原样直出 ASCII)。
    //
    // 作用域仍然限**当前会话**(ADR 0010 P2):裸 Shift 绝不广播到别的会话。
    _ApplyPunctWidth(to_session_id(ipc_id), !ascii);
    // ★ 批次 H 追加修复(2026-11-07,用户原话):「裸 Shift 切中英文后,设置栏
    //   的全/半角图标同步变化,但「中/英」标志不同步」。
    //   根因:本分支**只**写引擎的 live option,不写服务端镜像;而
    //   _ApplyPunctWidth 顺手写了 m_ascii_punct ⇒ 镜像里「标点」是新的、
    //   「中英文」还是旧的。只要 GetPanelInputState 拿不到 live 状态
    //   (_ResolveTargetSession 四档全落空:FocusOut 已把 m_active_session
    //   清零、m_last_focus_session 未设、会话数 ≠ 1),面板就回落镜像 ⇒
    //   表现恰好就是「全半角变了、中/英 没变」。
    //   补写镜像**不产生第二真相源**:GetPanelInputState 一旦拿到 live 状态
    //   就整体覆盖镜像(见该函数里的 live 读取),镜像只在读不到 live 时兜底。
    //   也不违反 ADR 0012:那条规定的是**跨会话广播**的传播判据
    //   (m_global_ascii_configured),这里写的是显示兜底值,不推给别的会话。
    m_global_ascii_mode = !ascii;
    DLOG(INFO) << "toggle ascii_mode -> " << !ascii << " ipc_id=" << ipc_id;
    // ADR 0012: answer with a body so that the client learns the new
    // status.ascii_mode and the language bar icon follows the input mode.
    _Respond(ipc_id, eat);
    _UpdateUI(ipc_id);
  }
  s = ShiftState{};  // C6: unconditionally reset on Up
}

void RimeWithWeaselHandler::SelectCandidate(size_t index,
                                            WeaselSessionId ipc_id) {
  TryLazyRecovery();
  if (m_disabled)
    return;
  DLOG(INFO) << "SelectCandidate: idx=" << index << ", ipc_id=" << ipc_id;
  rime_api->select_candidate(to_session_id(ipc_id), index);
  _UpdateUI(ipc_id);
}