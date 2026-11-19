// spec 100 阶段 D · T305 云端账号页实现
// 批次 AT-1 · 账号页 UI 重构（登录态区分 + 独立子窗口）
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudAccountDialog.h"

#include "CloudAccount.h"
#include "CloudAccountHistory.h"
#include "CloudAccountUi.h"
#include "CloudAccountWidgets.h"
#include "CloudCredentialStore.h"
#include "CloudCrypto.h"
#include "CloudTransport.h"

// spec 100 阶段 F · T505：账号页接入同步引擎（最近同步状态 + 立即同步）。
// spec 100 阶段 G 加固：重新发送邮箱验证码。
#include "CloudSyncEngine.h"
#include "CloudSyncNotifier.h"
#include "CloudSyncService.h"
#include "CloudUserDictIo.h"

// 用户数据目录真相源（同步 sidecar 与历史邮箱的落点）。
#include "../../include/FluxingUserPaths.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace fluxing {
namespace cloud {

namespace {

// 默认云端地址（自有域名 api.aiec.fun）。
// 刻意**不用** Cloudflare 默认子域 —— 境内 DNS 会污染它
// （见 cloud/fluxing-sync/test/check-endpoint-policy.mjs 的 T103 红线）。
constexpr char kDefaultBaseUrl[] = "https://api.aiec.fun";
constexpr wchar_t kBaseUrlEnvVar[] = L"FLUXING_CLOUD_BASE_URL";

constexpr UINT WM_APP_REFRESH = WM_APP + 1;

// 账号页共用的 Win32 小工具（CloudAccountWidgets.h）。
using widgets::AddControl;
using widgets::EnsureWindowShown;
using widgets::ReadControlText;
using widgets::SecureZeroWide;
using widgets::ToUtf8;
using widgets::ToWide;

std::wstring EnvBaseUrl() {
  wchar_t buf[512] = {0};
  const DWORD len = ::GetEnvironmentVariableW(
      kBaseUrlEnvVar, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
  if (len == 0 || len >= sizeof(buf) / sizeof(buf[0]))
    return std::wstring();
  std::wstring value(buf);
  while (!value.empty() && (value.back() == L' ' || value.back() == L'\t'))
    value.pop_back();
  return value;
}

// 墙钟毫秒（Unix 毫秒）。
int64_t NowUnixMs() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  const uint64_t ticks =
      (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  return static_cast<int64_t>(ticks / 10000ull - 11644473600000ull);
}

// Unix 毫秒 → 本地时间文本「2026-11-11 20:15:31」。
//
// 刻意**不用** CRT 的 localtime / localtime_s：localtime 返回的是进程级
// 共享静态缓冲区，而「最近同步」文案会被引擎线程与工作线程两条线同时
// 写入，并发调用会互相踩（宪法 P2 的线程纪律）。改用 Win32 的 FILETIME
// 转换链（全部可重入）：
//   Unix 毫秒 → FILETIME（(unix_ms + 11644473600000) * 10000）
//             → FileTimeToLocalFileTime（UTC → 本地时区）
//             → FileTimeToSystemTime（拆分成年月日时分秒）
std::wstring FormatLocalTimeMs(int64_t unix_ms) {
  if (unix_ms <= 0)
    return std::wstring();
  ULARGE_INTEGER ticks;
  ticks.QuadPart = static_cast<uint64_t>(unix_ms + 11644473600000LL) * 10000ull;
  FILETIME utc_ft;
  utc_ft.dwLowDateTime = ticks.LowPart;
  utc_ft.dwHighDateTime = ticks.HighPart;
  FILETIME local_ft;
  if (!::FileTimeToLocalFileTime(&utc_ft, &local_ft))
    return std::wstring();
  SYSTEMTIME st;
  if (!::FileTimeToSystemTime(&local_ft, &st))
    return std::wstring();
  wchar_t buf[32] = {0};
  ::swprintf(buf, sizeof(buf) / sizeof(buf[0]),
             L"%04u-%02u-%02u %02u:%02u:%02u", static_cast<unsigned>(st.wYear),
             static_cast<unsigned>(st.wMonth), static_cast<unsigned>(st.wDay),
             static_cast<unsigned>(st.wHour), static_cast<unsigned>(st.wMinute),
             static_cast<unsigned>(st.wSecond));
  return std::wstring(buf);
}

}  // namespace

// ---------------------------------------------------------------------------
// 控制器
// ---------------------------------------------------------------------------

class CloudAccountController::Impl {
 public:
  Impl() {
    std::wstring url = EnvBaseUrl();
    if (url.empty())
      url = ToWide(kDefaultBaseUrl);
    base_url = url;

    transport.reset(new WinHttpTransport(ToUtf8(base_url)));
    service.reset(new CloudAccountService(transport.get(), &store));

    std::string restore_err;
    bool restored = false;
    if (service->RestoreFromStore(&restore_err)) {
      status_text[static_cast<int>(CloudStatusScope::kAccount)] =
          L"已恢复上次的登录状态。";
      logged_in = true;
      email_verified = service->info().email_verified;
      account_email = ToWide(service->info().email);
      restored = true;
    } else {
      status_text[static_cast<int>(CloudStatusScope::kAccount)] = L"未登录。";
    }

    worker = std::thread([this]() { WorkerLoop(); });

    // 历史邮箱在**工作线程**上读盘（UI 线程不做磁盘 I/O，宪法 P2）。
    Post([this]() {
      const std::wstring path = account_history::DefaultHistoryPath();
      const std::vector<std::string> loaded =
          account_history::LoadHistoryFile(path);
      std::lock_guard<std::mutex> lock(mutex);
      history = loaded;
    });

    // 恢复态已是登录 ⇒ 构造完成后启动同步引擎（幂等）。
    if (restored)
      StartEngine();
  }

  ~Impl() {
    // ★ 顺序铁律：先停引擎（join 它的后台线程 —— 它可能正在用
    //   transport / service 做网络 I/O），**再**停工作线程。顺序反了会在
    //   引擎线程还在跑的时候把 service / transport 析构掉。
    //   这里**不能**持有 sync_mutex：引擎线程可能正卡在 RunSyncRound 里
    //   等 sync_mutex，持有它再 join 就是死锁。
    StopEngine();
    {
      std::lock_guard<std::mutex> lock(mutex);
      running = false;
    }
    cv.notify_all();
    if (worker.joinable())
      worker.join();
    // 兜底：若工作线程在析构期间接管了引擎（极端时序），这里再停一次。
    StopEngine();
  }

  void WorkerLoop() {
    while (true) {
      std::function<void()> task;
      {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this]() { return !running || !tasks.empty(); });
        if (!running && tasks.empty())
          return;
        task = tasks.front();
        tasks.pop_front();
      }
      if (task)
        task();
      NotifyWindows();
    }
  }

  void Post(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      tasks.push_back(std::move(task));
    }
    cv.notify_one();
  }

  // 写状态文案（**调用方不得持有 mutex**）。
  void SetStatus(CloudStatusScope scope, const std::wstring& text) {
    std::lock_guard<std::mutex> lock(mutex);
    status_text[static_cast<int>(scope)] = text;
  }

  // 写「最近同步」文案（UI 线程可读）。
  // 锁序铁律：一律「先 sync_mutex / engine_mutex，后 impl->mutex」，
  // 绝不可反（否则死锁）。
  void SetSyncStatus(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(mutex);
    sync_status_text = text;
  }

  // 通知所有已登记窗口刷新（后台任务完成 → UI 重绘）。
  void NotifyWindows() {
    std::vector<HWND> targets;
    {
      std::lock_guard<std::mutex> lock(mutex);
      targets.reserve(windows.size());
      for (std::map<HWND, int>::const_iterator it = windows.begin();
           it != windows.end(); ++it)
        targets.push_back(it->first);
    }
    for (size_t i = 0; i < targets.size(); ++i) {
      if (::IsWindow(targets[i]))
        ::PostMessageW(targets[i], WM_APP_REFRESH, 0, 0);
    }
  }

  // 记录历史邮箱（登录 / 注册成功后调用；在工作线程上做磁盘 I/O）。
  void RecordHistory(const std::string& email_utf8) {
    std::vector<std::string> merged;
    {
      std::lock_guard<std::mutex> lock(mutex);
      merged = account_history::MergeHistory(history, email_utf8);
      history = merged;
    }
    std::string err;
    account_history::SaveHistoryFile(account_history::DefaultHistoryPath(),
                                     merged, &err);
  }

  // 惰性创建用户词典后端（WeaselServer 里是 rime levers 实现）。
  // 只在 sync_mutex 内调用 ⇒ 创建 / 使用不会并发。
  IUserDictBackend* EnsureDictBackend() {
    if (!dict_backend)
      dict_backend = CreateRimeUserDictBackend();
    return dict_backend.get();
  }

  // 启动同步引擎（幂等）。
  //   · 引擎只在**已登录**时启动：拿不到令牌 / 密钥 / 设备标识就不启动，
  //     并把状态文案写成「未登录，无法同步。」；
  //   · startup_delay_ms = 30s：启动 30 秒后再拉一次，避免和登录流程抢网络。
  void StartEngine() {
    std::lock_guard<std::mutex> engine_lock(engine_mutex);
    if (engine && engine->running())
      return;

    std::string token;
    std::vector<uint8_t> enc_key;
    const std::string device = service ? service->device_id() : std::string();
    const bool has_token = service && service->GetAccessToken(&token);
    const bool has_key = service && service->GetEncKey(&enc_key);
    if (!has_token || !has_key || token.empty() || enc_key.empty() ||
        device.empty()) {
      SetSyncStatus(L"最近同步：未登录，无法同步。");
      return;
    }

    if (!engine)
      engine = std::make_shared<CloudSyncEngine>();
    CloudSyncEngineOptions options;
    options.startup_delay_ms = 30 * 1000;
    const CloudSyncEngine::SyncRunner runner =
        [this](SyncDirection direction, CloudSyncRoundResult* result) {
          return RunSyncRound(direction, result);
        };
    if (!engine->Start(runner, options)) {
      SetSyncStatus(L"最近同步：同步引擎启动失败。");
      return;
    }
    // spec 100 阶段 F · T501: 把引擎挂到「本地改动通知宿主」上。
    // 之后 rime 每次提交(词典可能变了)都会唤醒引擎 ⇒ 本地改动在防抖窗口
    // 后被推上云端, 不再只靠 60 分钟定时轮兜底。
    // 宿主用 shared_ptr 持有引擎 ⇒ 通知在飞行中时引擎不会先被析构。
    CloudSyncNotifier::Instance().SetTarget(engine);
  }

  // 停掉同步引擎（幂等）。Stop() 会 join 引擎线程 ⇒ 它可能正在跑一轮同步。
  void StopEngine() {
    // ★ 顺序铁律：先摘掉宿主上的目标（此后不再有新通知进来），再停引擎。
    // 已经在飞行中的通知由 shared_ptr 保活引擎 ⇒ 不会 use-after-free。
    CloudSyncNotifier::Instance().SetTarget(nullptr);
    std::shared_ptr<CloudSyncEngine> dying;
    {
      std::lock_guard<std::mutex> lock(engine_mutex);
      dying = std::move(engine);
    }
    if (dying) {
      dying->Stop();
      dying.reset();
    }
  }

  // 执行一轮同步（推 / 拉）。
  // **只**在后台线程调用：要么是引擎自己的后台线程，要么是控制器的工作
  // 线程（手动「立即同步」）；**绝不**在 UI 线程 / TSF 回调里跑（宪法 P2）。
  bool RunSyncRound(SyncDirection direction, CloudSyncRoundResult* result) {
    CloudSyncRoundResult local_result;
    CloudSyncRoundResult* out = result != nullptr ? result : &local_result;

    // 整轮串行化：引擎线程与手动同步可能同时进来。
    std::lock_guard<std::mutex> sync_lock(sync_mutex);

    std::string token;
    std::vector<uint8_t> enc_key;
    const std::string device = service ? service->device_id() : std::string();
    if (!service || !service->GetAccessToken(&token) || token.empty() ||
        !service->GetEncKey(&enc_key) || enc_key.empty() || device.empty()) {
      out->ok = false;
      out->message = "尚未登录，无法同步。";
      SetSyncStatus(L"最近同步：尚未登录，无法同步。");
      return false;
    }

    // sidecar 落点：用户数据目录。
    // 真相源 = ResolveFluxingUserDataPath（读注册表 RimeUserDir，
    // 回退 %AppData%\fluxing）。
    // 协调器内部用窄路径 API（std::ofstream / std::ifstream），
    // 落点必须是 ASCII。当前实际落点：D:\Program Files\fluxing\user1\fluxing。
    const std::wstring sidecar_wide =
        ResolveFluxingUserDataPath() / L"fluxing-cloud-sync.state";
    const std::string sidecar = ToUtf8(sidecar_wide);

    CloudSyncCoordinator coordinator(transport.get(), EnsureDictBackend(),
                                     token, device, enc_key, sidecar);
    std::string err;
    const bool ok = coordinator.SyncOnce(direction, out, &err);

    const std::wstring when = FormatLocalTimeMs(NowUnixMs());
    if (ok) {
      SetSyncStatus(L"最近同步：" + when + L" · 成功（推送 " +
                    std::to_wstring(out->items_pushed) + L" 条，拉取 " +
                    std::to_wstring(out->items_pulled) + L" 条）");
    } else if (out->skipped) {
      // ADR 0062 决策 2: 跳过 ≠ 失败 —— 用「已跳过」措辞，别吓人。
      std::string reason = err.empty() ? out->message : err;
      if (reason.empty())
        reason = MaintenanceGateSkipMessage();
      out->message = reason;
      SetSyncStatus(L"最近同步：" + when + L" · 已跳过（" + ToWide(reason) +
                    L"）");
    } else {
      std::string reason = err.empty() ? out->message : err;
      if (reason.empty())
        reason = "未知原因";
      out->message = reason;
      SetSyncStatus(L"最近同步：" + when + L" · 失败（" + ToWide(reason) +
                    L"）");
    }

    // 通知账号页系列窗口刷新。
    NotifyWindows();
    return ok;
  }

  mutable std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::function<void()>> tasks;
  std::thread worker;
  std::atomic<bool> running{true};

  // 已登记窗口 → 作用域（一个作用域一个窗口）。
  std::map<HWND, int> windows;
  // 每个作用域各有一行状态文案（互不覆盖）。
  std::wstring status_text[static_cast<int>(CloudStatusScope::kCount)];
  std::vector<CloudDeviceInfo> devices;
  bool logged_in = false;
  bool email_verified = false;
  std::wstring account_email;
  std::wstring base_url;
  // 历史邮箱（最近在前；由工作线程维护）。
  std::vector<std::string> history;
  // 「请弹出验证码窗口」的一次性请求（邮箱）。
  std::wstring verify_request_email;
  // 「请 UI 清空口令框」的一次性请求（**只在登录成功**时置位）。
  bool clear_password_request = false;

  std::unique_ptr<ICloudTransport> transport;
  std::unique_ptr<CloudAccountService> service;
  CloudCredentialStore store;

  // ---- 同步引擎（阶段 F · T505） ----
  // engine 由 engine_mutex 保护；dict_backend 只在 sync_mutex 内创建 / 使用。
  // spec 100 阶段 F · T501: 用 shared_ptr —— 本地改动通知宿主
  // (CloudSyncNotifier) 需要持有它, 保证「通知已在飞行中」时引擎对象仍然
  // 存活(无 use-after-free)。
  std::shared_ptr<CloudSyncEngine> engine;
  std::unique_ptr<IUserDictBackend> dict_backend;
  std::mutex engine_mutex;
  std::mutex sync_mutex;
  std::wstring sync_status_text = L"最近同步：尚未同步。";
};

CloudAccountController::CloudAccountController() : impl_(new Impl()) {}

CloudAccountController::~CloudAccountController() {
  delete impl_;
}

CloudAccountController& CloudAccountController::Instance() {
  static CloudAccountController instance;
  return instance;
}

void CloudAccountController::RegisterWindow(HWND hwnd, CloudStatusScope scope) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->windows[hwnd] = static_cast<int>(scope);
}

void CloudAccountController::UnregisterWindow(HWND hwnd) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->windows.erase(hwnd);
}

HWND CloudAccountController::owner_window() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  for (std::map<HWND, int>::const_iterator it = impl_->windows.begin();
       it != impl_->windows.end(); ++it) {
    if (it->second == static_cast<int>(CloudStatusScope::kAccount))
      return it->first;
  }
  return nullptr;
}

std::wstring CloudAccountController::base_url() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->base_url;
}

std::wstring CloudAccountController::status() const {
  return status(CloudStatusScope::kAccount);
}

std::wstring CloudAccountController::status(CloudStatusScope scope) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->status_text[static_cast<int>(scope)];
}

std::vector<CloudDeviceInfo> CloudAccountController::devices() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->devices;
}

bool CloudAccountController::logged_in() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->logged_in;
}

// 消费「请清空口令框」的一次性请求（UI 线程调用）。
// 读取即消费 ⇒ 不会重复清空用户随后新输入的内容。
bool CloudAccountController::ConsumeClearPasswordRequest() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const bool requested = impl_->clear_password_request;
  impl_->clear_password_request = false;
  return requested;
}

bool CloudAccountController::email_verified() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->email_verified;
}

std::wstring CloudAccountController::account_email() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->account_email;
}

std::wstring CloudAccountController::sync_status() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->sync_status_text;
}

// 历史邮箱（最近在前）。
std::vector<std::string> CloudAccountController::history() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->history;
}

// 消费「请弹出验证码窗口」的一次性请求（返回要验证的邮箱；空 = 无请求）。
std::wstring CloudAccountController::ConsumeVerifyRequest() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const std::wstring value = impl_->verify_request_email;
  impl_->verify_request_email.clear();
  return value;
}

void CloudAccountController::SubmitLogin(const std::wstring& email,
                                         const std::wstring& password,
                                         bool save_account) {
  const std::string email_utf8 = ToUtf8(email);
  std::string password_utf8 = ToUtf8(password);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, password_utf8, save_account]() mutable {
    std::string err;
    const bool ok =
        impl->service->Login(email_utf8, password_utf8, &err, save_account);
    // 口令用后清零（不落日志、不留副本）。lambda 必须是 mutable：
    // 捕获的 password_utf8 默认是 const，清零函数要的是可写指针。
    SecureZeroString(&password_utf8);
    bool need_verify = false;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (ok) {
        impl->logged_in = true;
        impl->email_verified = impl->service->info().email_verified;
        impl->account_email = ToWide(impl->service->info().email);
        impl->status_text[static_cast<int>(CloudStatusScope::kAccount)] =
            impl->email_verified
                ? L"登录成功。"
                : L"登录成功，但邮箱尚未验证，请完成邮箱验证。";
        // 只有「已登录**且**邮箱已验证」才清口令框：
        // ① 失败时保留 ⇒ 用户可改错重试；
        // ② 「登录成功但邮箱未验证」也保留 ⇒ 用户验证完邮箱还要用**同一口令**
        //    再登录一次，提前清掉会逼他重敲一遍（真机闭环实测踩到过）。
        impl->clear_password_request = impl->email_verified;
        need_verify = !impl->email_verified;
      } else {
        impl->logged_in = false;
        impl->status_text[static_cast<int>(CloudStatusScope::kAccount)] =
            L"登录失败：" + ToWide(err);
      }
    }
    if (!ok)
      return;
    // 登录成功 ⇒ 启动同步引擎（幂等：已在跑就直接返回）。
    impl->StartEngine();
    // 记住这个邮箱（下次登录可以直接从「历史」里选）。
    impl->RecordHistory(email_utf8);
    // 「首次登录某个账号」需要邮箱验证 ⇒ 弹**独立**验证码窗口
    // （不再是主窗口里内嵌的一行）。
    if (need_verify) {
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->verify_request_email = impl->account_email;
      }
      // 顺手发一次验证码（best-effort：失败时用户可在验证码窗口里点重发）。
      std::string resend_err;
      impl->service->ResendVerification(email_utf8, &resend_err);
    }
    // 登录成功后顺手刷新设备列表。
    std::vector<CloudDeviceInfo> list;
    std::string list_err;
    if (impl->service->ListDevices(&list, &list_err)) {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->devices = list;
    }
  });
}

void CloudAccountController::SubmitRegister(const std::wstring& email,
                                            const std::wstring& password,
                                            CloudStatusScope scope) {
  const std::string email_utf8 = ToUtf8(email);
  std::string password_utf8 = ToUtf8(password);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, password_utf8, scope]() mutable {
    std::string account_id;
    std::string err;
    const bool ok =
        impl->service->Register(email_utf8, password_utf8, &account_id, &err);
    // 口令用后清零（闭包副本 + 服务内部副本都在这里收口）。
    SecureZeroString(&password_utf8);
    // 注册成功**不**置 logged_in：服务端返回 need_email_verify=true，
    // 必须先完成邮箱验证再登录，避免出现「已登录但邮箱未验证」的脏态。
    impl->SetStatus(scope,
                    ok ? L"注册成功：请在弹出的验证码窗口输入 6 位验证码，完成邮箱验证。"
                       : (L"注册失败：" + ToWide(err)));
    if (ok) {
      impl->RecordHistory(email_utf8);
      // 注册成功 ⇒ 直接弹验证码窗口（独立窗口），用户不必回主窗口。
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        impl->verify_request_email = ToWide(email_utf8);
      }
    }
  });
}

void CloudAccountController::SubmitVerifyEmail(const std::wstring& email,
                                               const std::wstring& code,
                                               CloudStatusScope scope) {
  const std::string email_utf8 = ToUtf8(email);
  const std::string code_utf8 = ToUtf8(code);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, code_utf8, scope]() {
    std::string err;
    const bool ok = impl->service->VerifyEmail(email_utf8, code_utf8, &err);
    // 验证同样不改登录态：验证通过后由用户点「登录」拿令牌。
    impl->SetStatus(scope, ok ? L"邮箱验证成功，请登录。"
                              : (L"邮箱验证失败：" + ToWide(err)));
    if (!ok)
      return;
    // 验证通过 ⇒ 服务端已把 email_verified 置 1。同步客户端内存状态，
    // 否则账号页会一直挂着「登录成功，但邮箱尚未验证」的旧文案。
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->email_verified = true;
    // 口令此时已无用途（登录路径下口令框在已登录视图里是隐藏的）⇒ 清掉，
    // 避免明文口令长期滞留在内存控件里。
    impl->clear_password_request = true;
    impl->status_text[static_cast<int>(CloudStatusScope::kAccount)] =
        impl->logged_in ? L"邮箱验证成功。"
                         : L"邮箱验证成功，请回到账号页登录。";
  });
}

void CloudAccountController::SubmitLogout() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::string err;
    const bool ok = impl->service->Logout(&err);
    // 退出登录 ⇒ 停掉同步引擎（它可能正在用令牌 / transport 做同步），
    // 并把「最近同步」文案复位。**先**停引擎再改状态：StopEngine 会
    // join 引擎线程，不能持有 impl->mutex（锁序：engine_mutex → mutex）。
    impl->StopEngine();
    impl->SetSyncStatus(L"最近同步：尚未同步。");
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->logged_in = false;
    impl->email_verified = false;
    impl->account_email.clear();
    impl->devices.clear();
    impl->status_text[static_cast<int>(CloudStatusScope::kAccount)] =
        ok ? L"已退出登录。" : (L"退出登录失败：" + ToWide(err));
  });
}

void CloudAccountController::SubmitRefresh() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::string err;
    const bool ok = impl->service->Refresh(&err);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->logged_in = ok;
    impl->status_text[static_cast<int>(CloudStatusScope::kDevice)] =
        ok ? L"登录状态已刷新。" : (L"刷新失败：" + ToWide(err));
  });
}

void CloudAccountController::SubmitListDevices() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::vector<CloudDeviceInfo> list;
    std::string err;
    const bool ok = impl->service->ListDevices(&list, &err);
    std::lock_guard<std::mutex> lock(impl->mutex);
    if (ok) {
      impl->devices = list;
      impl->status_text[static_cast<int>(CloudStatusScope::kDevice)] =
          L"已刷新设备列表。";
    } else {
      impl->status_text[static_cast<int>(CloudStatusScope::kDevice)] =
          L"获取设备列表失败：" + ToWide(err);
    }
  });
}

void CloudAccountController::SubmitRevokeDevice(size_t index) {
  Impl* impl = impl_;
  impl->Post([impl, index]() {
    std::string device_id;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (index >= impl->devices.size())
        return;
      device_id = impl->devices[index].device_id;
    }
    std::string err;
    const bool ok = impl->service->RevokeDevice(device_id, &err);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->status_text[static_cast<int>(CloudStatusScope::kDevice)] =
        ok ? (L"已撤销设备 " + ToWide(device_id))
           : (L"撤销设备失败：" + ToWide(err));
    if (!ok)
      return;
    std::vector<CloudDeviceInfo> list;
    std::string list_err;
    if (impl->service->ListDevices(&list, &list_err)) {
      std::lock_guard<std::mutex> lock2(impl->mutex);
      impl->devices = list;
    }
  });
}

void CloudAccountController::SubmitDeleteCloudData() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::string err;
    const bool ok = impl->service->DeleteCloudData(&err);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->status_text[static_cast<int>(CloudStatusScope::kDevice)] =
        ok ? L"云端数据已清空（账号本身保留）。"
           : (L"清空云端数据失败：" + ToWide(err));
  });
}

void CloudAccountController::SubmitResendVerification(
    const std::wstring& email,
    CloudStatusScope scope) {
  const std::string email_utf8 = ToUtf8(email);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, scope]() {
    std::string err;
    const bool ok = impl->service->ResendVerification(email_utf8, &err);
    // 重发不改登录态，也**不清空验证码输入框**：失败时（码过期 / 已被
    // 用掉）用户还能改了再提交。
    impl->SetStatus(scope, ok ? L"验证码已重新发送，请查收。"
                               : (L"重新发送验证码失败：" + ToWide(err)));
  });
}

void CloudAccountController::SubmitResetPassword(const std::wstring& email,
                                                 const std::wstring& code,
                                                 const std::wstring& new_password,
                                                 CloudStatusScope scope) {
  const std::string email_utf8 = ToUtf8(email);
  const std::string code_utf8 = ToUtf8(code);
  std::string new_password_utf8 = ToUtf8(new_password);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, code_utf8, new_password_utf8, scope]() mutable {
    std::string err;
    const bool ok = impl->service->ResetPassword(
        email_utf8, code_utf8, new_password_utf8, &err);
    // 新口令用后清零。
    SecureZeroString(&new_password_utf8);
    impl->SetStatus(scope, ok ? L"密码已重置，请用新密码登录。"
                              : (L"重置密码失败：" + ToWide(err)));
  });
}

void CloudAccountController::SubmitSyncNow() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    // 手动同步优先**走引擎**：引擎会在自己的后台线程上跑这一轮，并把
    // 状态快照记成「手动触发」（last_trigger = kManual）。
    // 按引擎设计，手动触发优先级最高且**不受每日配额限制**
    // （DecideNextAction：手动 > 退避 > 启动 > 脏推 > 定时）。
    // 网络 I/O 全程在引擎线程 ⇒ UI 线程不阻塞（宪法 P2）。
    bool delegated = false;
    {
      std::lock_guard<std::mutex> lock(impl->engine_mutex);
      if (impl->engine && impl->engine->running()) {
        impl->engine->RequestManualSync();
        delegated = true;
      }
    }
    if (delegated) {
      impl->SetStatus(CloudStatusScope::kAccount,
                      L"手动同步已提交，完成后见下方「最近同步」。");
      return;
    }

    // 引擎没起来（未登录 / 恢复态启动失败）：退化为直接跑一轮，
    // 好让用户看到中文原因，而不是点了毫无反馈。
    CloudSyncRoundResult result;
    const bool ok = impl->RunSyncRound(SyncDirection::kBoth, &result);
    if (ok) {
      impl->SetStatus(CloudStatusScope::kAccount,
                      L"同步完成：推送 " +
                          std::to_wstring(result.items_pushed) + L" 条，拉取 " +
                          std::to_wstring(result.items_pulled) + L" 条。");
    } else if (result.skipped) {
      // ADR 0062 决策 3: 手动同步**不绕过**闸门 —— 给明确提示 + 指引。
      impl->SetStatus(CloudStatusScope::kAccount,
                      std::wstring(L"同步已跳过：") +
                          ToWide(MaintenanceGateSkipMessage()) +
                          ToWide(MaintenanceGateManualHint()));
    } else {
      const std::wstring reason = result.message.empty()
                                      ? std::wstring(L"未知原因")
                                      : ToWide(result.message);
      impl->SetStatus(CloudStatusScope::kAccount, L"同步失败：" + reason);
    }
  });
}

// ---------------------------------------------------------------------------
// 窗口
// ---------------------------------------------------------------------------

namespace account_dialog {
namespace {

using Role = AccountWindowRole;

// 角色枚举与作用域枚举一一对应（同一套 5 个窗口）。
static_assert(static_cast<int>(Role::kAccount) ==
                  static_cast<int>(CloudStatusScope::kAccount),
              "role/scope 必须一一对应");
static_assert(static_cast<int>(Role::kVerify) ==
                  static_cast<int>(CloudStatusScope::kVerify),
              "role/scope 必须一一对应");
static_assert(static_cast<int>(Role::kRegister) ==
                  static_cast<int>(CloudStatusScope::kRegister),
              "role/scope 必须一一对应");
static_assert(static_cast<int>(Role::kForgot) ==
                  static_cast<int>(CloudStatusScope::kForgot),
              "role/scope 必须一一对应");
static_assert(static_cast<int>(Role::kDevice) ==
                  static_cast<int>(CloudStatusScope::kDevice),
              "role/scope 必须一一对应");

CloudStatusScope ScopeOf(Role role) {
  return static_cast<CloudStatusScope>(static_cast<int>(role));
}

const wchar_t* ClassNameFor(Role role) {
  switch (role) {
    case Role::kVerify:
      return L"FluxingCloudVerifyDialog_v1";
    case Role::kRegister:
      return L"FluxingCloudRegisterDialog_v1";
    case Role::kForgot:
      return L"FluxingCloudForgotDialog_v1";
    case Role::kDevice:
      return L"FluxingCloudDeviceDialog_v1";
    case Role::kAccount:
    default:
      return L"FluxingCloudAccountDialog_v1";
  }
}

const wchar_t* TitleFor(Role role) {
  switch (role) {
    case Role::kVerify:
      return L"Fluxing 邮箱验证";
    case Role::kRegister:
      return L"Fluxing 注册账号";
    case Role::kForgot:
      return L"Fluxing 找回密码";
    case Role::kDevice:
      return L"Fluxing 设备与云数据";
    case Role::kAccount:
    default:
      return L"Fluxing 云端账号";
  }
}

// 控件文案（用户可见 ⇒ 简体中文，宪法 P2/P5）。
std::wstring WidgetText(Role role, int id) {
  switch (id) {
    case kIdEmailLabel:
      return L"邮箱地址";
    case kIdPasswordLabel:
      return L"密码";
    case kIdVerifyCodeLabel:
      return L"验证码";
    case kIdNewPasswordLabel:
      return L"新密码";
    case kIdHistoryLabel:
      return L"历史";
    case kIdAccountCaption:
      return L"当前登录账号";
    case kIdSaveAccount:
      return L"保存账号（下次启动自动登录）";
    case kIdLogin:
      return L"登录";
    case kIdLogout:
      return L"退出登录";
    case kIdSyncNow:
      return L"立即同步";
    case kIdClose:
      return L"关闭";
    case kIdVerify:
      return L"验证";
    case kIdForgotPassword:
      return L"忘记密码";
    case kIdResetPassword:
      return L"重置密码";
    case kIdRevoke:
      return L"撤销所选设备";
    case kIdRefreshDevices:
      return L"刷新设备列表";
    case kIdRefresh:
      return L"刷新登录状态";
    case kIdDeleteData:
      return L"清空云端数据";
    case kIdSyncStatus:
      return L"最近同步：尚未同步。";
    case kIdOpportunityNote:
      return L"你正在输入时同步会自动延后，停止输入后自动完成。";
    case kIdRegister:
      return role == Role::kRegister ? L"注册并发送验证码" : L"注册";
    case kIdResend:
      return role == Role::kForgot ? L"发送验证码" : L"重新发送验证码";
    case kIdVerifyCaption:
      return L"验证码已发送到该邮箱，请输入 6 位验证码。";
    case kIdRegisterCaption:
      return L"注册新账号（注册后需完成邮箱验证）。";
    case kIdForgotCaption:
      return L"通过邮箱验证码重置密码。";
    case kIdDeviceCaption:
      return L"已登录设备与云端数据。";
    default:
      return L"";
  }
}

const wchar_t* WinClassFor(AccountWidgetClass cls) {
  switch (cls) {
    case AccountWidgetClass::kEdit:
    case AccountWidgetClass::kPasswordEdit:
    case AccountWidgetClass::kNumberEdit:
      return L"EDIT";
    case AccountWidgetClass::kButton:
    case AccountWidgetClass::kCheckbox:
      return L"BUTTON";
    case AccountWidgetClass::kCombo:
      return L"COMBOBOX";
    case AccountWidgetClass::kListBox:
      return L"LISTBOX";
    case AccountWidgetClass::kStatic:
    default:
      return L"STATIC";
  }
}

DWORD StyleFor(AccountWidgetClass cls) {
  switch (cls) {
    case AccountWidgetClass::kEdit:
      return ES_AUTOHSCROLL;
    case AccountWidgetClass::kPasswordEdit:
      return ES_AUTOHSCROLL | ES_PASSWORD;
    case AccountWidgetClass::kNumberEdit:
      return ES_AUTOHSCROLL | ES_NUMBER;
    case AccountWidgetClass::kButton:
      return BS_PUSHBUTTON;
    case AccountWidgetClass::kCheckbox:
      return BS_AUTOCHECKBOX;
    case AccountWidgetClass::kCombo:
      return CBS_DROPDOWNLIST | WS_VSCROLL;
    case AccountWidgetClass::kListBox:
      return LBS_NOTIFY | WS_VSCROLL | WS_BORDER;
    case AccountWidgetClass::kStatic:
    default:
      return 0;
  }
}

std::wstring DeviceLabel(const CloudDeviceInfo& device) {
  std::wstring label =
      device.name.empty() ? ToWide(device.device_id) : ToWide(device.name);
  label += L"（";
  label += ToWide(device.device_id);
  label += L"）";
  if (device.current)
    label += L" · 本机";
  if (device.revoked)
    label += L" · 已撤销";
  return label;
}

// 窗口句柄 → 角色（GWLP_USERDATA 在 WM_CREATE 里第一件事就写好了）。
Role RoleOf(HWND hwnd) {
  return static_cast<Role>(
      static_cast<int>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA)));
}

HWND g_windows[static_cast<int>(Role::kCount)] = {nullptr};
// 验证码窗口对应的邮箱（UI 线程独占，不需要加锁）。
std::wstring g_verify_email;

void CreateWidgets(HWND hwnd, Role role) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role);
  for (size_t i = 0; i < specs.size(); ++i) {
    const AccountWidgetSpec& spec = specs[i];
    const std::wstring text = WidgetText(role, spec.id);
    AddControl(hwnd, WinClassFor(spec.cls), text.c_str(), StyleFor(spec.cls),
               spec.x, spec.y, spec.w, spec.h, spec.id);
  }
}

// 按登录态切换控件的可见性与几何（**不重建控件** —— 重建会丢输入内容）。
void ApplyLayout(HWND hwnd, Role role, bool logged_in) {
  const std::vector<AccountWidgetSpec> specs = AccountWidgets(role);
  const std::vector<AccountLayoutEntry> layout =
      AccountVisibleLayout(role, logged_in);
  for (size_t i = 0; i < specs.size(); ++i) {
    HWND ctrl = ::GetDlgItem(hwnd, specs[i].id);
    if (ctrl == nullptr)
      continue;
    const AccountLayoutEntry* target = nullptr;
    for (size_t j = 0; j < layout.size(); ++j) {
      if (layout[j].id == specs[i].id) {
        target = &layout[j];
        break;
      }
    }
    if (target == nullptr) {
      ::ShowWindow(ctrl, SW_HIDE);
      continue;
    }
    ::SetWindowPos(ctrl, nullptr, target->x, target->y, target->w, target->h,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    ::ShowWindow(ctrl, SW_SHOW);
  }
}

// 刷新历史邮箱下拉（保留用户已选的项）。
void RefreshHistoryCombo(HWND hwnd, CloudAccountController& controller) {
  HWND combo = ::GetDlgItem(hwnd, kIdHistoryCombo);
  if (combo == nullptr)
    return;
  wchar_t buf[256] = {0};
  ::GetWindowTextW(combo, buf, 256);
  const std::wstring current(buf);
  ::SecureZeroMemory(buf, sizeof(buf));

  const std::vector<std::string> history = controller.history();
  ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  int restored = CB_ERR;
  for (size_t i = 0; i < history.size(); ++i) {
    const std::wstring item = ToWide(history[i]);
    ::SendMessageW(combo, CB_ADDSTRING, 0,
                   reinterpret_cast<LPARAM>(item.c_str()));
    if (!current.empty() && item == current)
      restored = static_cast<int>(i);
  }
  ::SendMessageW(combo, CB_SETCURSEL, restored, 0);
}

// 「弹验证码窗口」的前置声明（实现在文件后半段；RefreshUi 要用它）。
void ShowVerifyWindow(const std::wstring& email);

void RefreshUi(HWND hwnd, Role role) {
  CloudAccountController& controller = CloudAccountController::Instance();

  // 一次性请求：① 登录成功但该账号邮箱尚未验证 ② 注册成功
  // ⇒ 弹**独立**验证码窗口（不再内嵌在账号页主窗口里）。
  // ConsumeVerifyRequest 取走即清 ⇒ 即使多个窗口同时刷新也只会弹一次。
  const std::wstring verify_email = controller.ConsumeVerifyRequest();
  if (!verify_email.empty())
    ShowVerifyWindow(verify_email);

  const bool logged_in = controller.logged_in();

  HWND status = ::GetDlgItem(hwnd, kIdStatus);
  if (status != nullptr)
    ::SetWindowTextW(status, controller.status(ScopeOf(role)).c_str());

  if (role == Role::kAccount) {
    // 「已登录且邮箱已验证」⇒ 清空口令框（一次性请求，读取即消费）。
    if (controller.ConsumeClearPasswordRequest()) {
      HWND pwd = ::GetDlgItem(hwnd, kIdPassword);
      if (pwd != nullptr)
        ::SetWindowTextW(pwd, L"");
    }
    HWND account = ::GetDlgItem(hwnd, kIdAccountLabel);
    if (account != nullptr)
      ::SetWindowTextW(account, controller.account_email().c_str());
    RefreshHistoryCombo(hwnd, controller);
    // ★ 登录态区分：两套控件互斥，切换靠 ShowWindow，不重建。
    ApplyLayout(hwnd, role, logged_in);
  } else if (role == Role::kDevice) {
    HWND list = ::GetDlgItem(hwnd, kIdDevices);
    if (list != nullptr) {
      ::SendMessageW(list, LB_RESETCONTENT, 0, 0);
      const std::vector<CloudDeviceInfo> devices = controller.devices();
      for (size_t i = 0; i < devices.size(); ++i) {
        const std::wstring label = DeviceLabel(devices[i]);
        ::SendMessageW(list, LB_ADDSTRING, 0,
                       reinterpret_cast<LPARAM>(label.c_str()));
      }
    }
    // 未登录 ⇒ 设备相关操作全部置灰（窗口照样能打开）。
    const std::vector<int> enabled = DeviceDialogEnabledControlIds(logged_in);
    const int device_ids[4] = {kIdDeleteData, kIdRefresh, kIdRefreshDevices,
                               kIdRevoke};
    for (int i = 0; i < 4; ++i) {
      HWND ctrl = ::GetDlgItem(hwnd, device_ids[i]);
      if (ctrl == nullptr)
        continue;
      const bool on = std::find(enabled.begin(), enabled.end(),
                                device_ids[i]) != enabled.end();
      ::EnableWindow(ctrl, on ? TRUE : FALSE);
    }
  }
}

HWND CreateOrShow(Role role);
void ShowVerifyWindow(const std::wstring& email);
void ShowRegisterWindow();
void ShowForgotWindow();

LRESULT CALLBACK WndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) {
  switch (message) {
    case WM_CREATE: {
      CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
      const Role role =
          static_cast<Role>(reinterpret_cast<INT_PTR>(cs->lpCreateParams));
      ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          static_cast<LONG_PTR>(static_cast<int>(role)));
      CloudAccountController::Instance().RegisterWindow(hwnd, ScopeOf(role));
      CreateWidgets(hwnd, role);
      RefreshUi(hwnd, role);
      return 0;
    }

    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      const int code = HIWORD(wparam);
      const Role role = RoleOf(hwnd);
      CloudAccountController& controller = CloudAccountController::Instance();
      switch (role) {
        case Role::kAccount:
          switch (id) {
            case kIdLogin: {
              // 邮箱保持宽字符：SubmitLogin 内部再转 UTF-8 交给云端服务。
              const std::wstring email = ReadControlText(hwnd, kIdEmail);
              std::wstring password = ReadControlText(hwnd, kIdPassword);
              HWND save_box = ::GetDlgItem(hwnd, kIdSaveAccount);
              const bool save_account =
                  save_box != nullptr &&
                  ::SendMessageW(save_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
              controller.SubmitLogin(email, password, save_account);
              // 口令用完立刻抹掉**堆上**的宽字符串副本。
              // 输入框**不**在此清空：登录失败时要能改错重试（否则用户得
              // 重新敲一遍口令）；成功后再由 RefreshUi 统一清空。
              SecureZeroWide(&password);
              return 0;
            }
            case kIdRegister:
              ShowRegisterWindow();
              return 0;
            case kIdForgotPassword:
              ShowForgotWindow();
              return 0;
            case kIdLogout:
              controller.SubmitLogout();
              return 0;
            case kIdSyncNow:
              controller.SubmitSyncNow();
              return 0;
            case kIdHistoryCombo: {
              if (code == CBN_SELCHANGE) {
                HWND combo = ::GetDlgItem(hwnd, kIdHistoryCombo);
                const LRESULT sel =
                    combo ? ::SendMessageW(combo, CB_GETCURSEL, 0, 0) : -1;
                if (sel >= 0) {
                  wchar_t buf[256] = {0};
                  ::SendMessageW(combo, CB_GETLBTEXT,
                                 static_cast<LPARAM>(sel),
                                 reinterpret_cast<LPARAM>(buf));
                  HWND email = ::GetDlgItem(hwnd, kIdEmail);
                  if (email != nullptr)
                    ::SetWindowTextW(email, buf);
                  ::SecureZeroMemory(buf, sizeof(buf));
                }
              }
              return 0;
            }
            case kIdClose:
              ::DestroyWindow(hwnd);
              return 0;
            default:
              break;
          }
          break;

        case Role::kVerify:
          switch (id) {
            case kIdVerify: {
              std::wstring code_text = ReadControlText(hwnd, kIdVerifyCode);
              controller.SubmitVerifyEmail(g_verify_email, code_text,
                                          CloudStatusScope::kVerify);
              // 验证码用后清零本地副本，但保留输入框内容：
              // 验证失败（码错/过期）时用户还能改了再提交。
              SecureZeroWide(&code_text);
              return 0;
            }
            case kIdResend:
              controller.SubmitResendVerification(g_verify_email,
                                                 CloudStatusScope::kVerify);
              return 0;
            case kIdClose:
              ::DestroyWindow(hwnd);
              return 0;
            default:
              break;
          }
          break;

        case Role::kRegister:
          switch (id) {
            case kIdRegister: {
              const std::wstring email = ReadControlText(hwnd, kIdEmail);
              std::wstring password = ReadControlText(hwnd, kIdPassword);
              controller.SubmitRegister(email, password,
                                        CloudStatusScope::kRegister);
              // 同上：注册失败保留口令便于重试；注册成功后用户还要用**同一**
              // 口令登录，因此输入框一律不在这里清空。
              SecureZeroWide(&password);
              return 0;
            }
            case kIdClose:
              ::DestroyWindow(hwnd);
              return 0;
            default:
              break;
          }
          break;

        case Role::kForgot:
          switch (id) {
            case kIdResend:
              controller.SubmitResendVerification(ReadControlText(hwnd, kIdEmail),
                                                 CloudStatusScope::kForgot);
              return 0;
            case kIdResetPassword: {
              const std::wstring email = ReadControlText(hwnd, kIdEmail);
              std::wstring code_text = ReadControlText(hwnd, kIdVerifyCode);
              std::wstring new_password = ReadControlText(hwnd, kIdNewPassword);
              controller.SubmitResetPassword(email, code_text, new_password,
                                            CloudStatusScope::kForgot);
              SecureZeroWide(&code_text);
              SecureZeroWide(&new_password);
              return 0;
            }
            case kIdClose:
              ::DestroyWindow(hwnd);
              return 0;
            default:
              break;
          }
          break;

        case Role::kDevice:
          switch (id) {
            case kIdRevoke: {
              HWND list = ::GetDlgItem(hwnd, kIdDevices);
              const LRESULT index =
                  list ? ::SendMessageW(list, LB_GETCURSEL, 0, 0) : -1;
              if (index >= 0)
                controller.SubmitRevokeDevice(static_cast<size_t>(index));
              return 0;
            }
            case kIdRefreshDevices:
              controller.SubmitListDevices();
              return 0;
            case kIdRefresh:
              controller.SubmitRefresh();
              return 0;
            case kIdDeleteData:
              controller.SubmitDeleteCloudData();
              return 0;
            case kIdClose:
              ::DestroyWindow(hwnd);
              return 0;
            default:
              break;
          }
          break;

        default:
          break;
      }
      return 0;
    }

    case WM_APP_REFRESH:
      RefreshUi(hwnd, RoleOf(hwnd));
      return 0;

    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY: {
      const Role role = RoleOf(hwnd);
      CloudAccountController::Instance().UnregisterWindow(hwnd);
      HWND& slot = g_windows[static_cast<int>(role)];
      if (slot == hwnd)
        slot = nullptr;
      return 0;
    }
  }
  return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

bool EnsureClass() {
  static bool registered = false;
  if (registered)
    return true;
  for (int i = 0; i < static_cast<int>(Role::kCount); ++i) {
    const Role role = static_cast<Role>(i);
    WNDCLASSEXW wc;
    ::ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = ClassNameFor(role);
    wc.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    if (::RegisterClassExW(&wc) == 0 &&
        ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
      return false;
  }
  registered = true;
  return true;
}

HWND CreateOrShow(Role role) {
  HWND& hwnd = g_windows[static_cast<int>(role)];
  if (hwnd != nullptr && !::IsWindow(hwnd)) {
    // 缓存的句柄已失效(窗口被销毁但 WM_DESTROY 没走到本缓存): 丢掉重建,
    // 否则此后每次点击都只是对一个死句柄置前。
    hwnd = nullptr;
  }
  if (hwnd != nullptr) {
    EnsureWindowShown(hwnd);
    ::SetForegroundWindow(hwnd);
    return hwnd;
  }
  if (!EnsureClass())
    return nullptr;

  int width = 480;
  int height = 250;
  AccountClientSize(role, &width, &height);
  RECT rect = {0, 0, width, height};
  ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  hwnd = ::CreateWindowExW(
      WS_EX_APPWINDOW, ClassNameFor(role), TitleFor(role), WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
      rect.bottom - rect.top, nullptr, nullptr, ::GetModuleHandleW(nullptr),
      reinterpret_cast<LPVOID>(static_cast<INT_PTR>(role)));
  if (hwnd == nullptr)
    return nullptr;
  EnsureWindowShown(hwnd);
  if (role != Role::kAccount) {
    // 子窗口摆在主窗口正中（否则会跟主窗口叠在一起，找不到）。
    widgets::CenterOnOwner(hwnd, g_windows[static_cast<int>(Role::kAccount)]);
  }
  ::SetForegroundWindow(hwnd);
  return hwnd;
}

void ShowVerifyWindow(const std::wstring& email) {
  g_verify_email = email;
  HWND hwnd = CreateOrShow(Role::kVerify);
  if (hwnd == nullptr)
    return;
  HWND caption = ::GetDlgItem(hwnd, kIdVerifyCaption);
  if (caption != nullptr) {
    const std::wstring text =
        L"验证码已发送到 " + email + L"，请输入 6 位验证码。";
    ::SetWindowTextW(caption, text.c_str());
  }
  HWND code = ::GetDlgItem(hwnd, kIdVerifyCode);
  if (code != nullptr)
    ::SetWindowTextW(code, L"");
}

void ShowRegisterWindow() {
  HWND hwnd = CreateOrShow(Role::kRegister);
  if (hwnd == nullptr)
    return;
  HWND owner = g_windows[static_cast<int>(Role::kAccount)];
  HWND email = ::GetDlgItem(hwnd, kIdEmail);
  if (email != nullptr && owner != nullptr)
    ::SetWindowTextW(email, ReadControlText(owner, kIdEmail).c_str());
}

void ShowForgotWindow() {
  HWND hwnd = CreateOrShow(Role::kForgot);
  if (hwnd == nullptr)
    return;
  HWND owner = g_windows[static_cast<int>(Role::kAccount)];
  HWND email = ::GetDlgItem(hwnd, kIdEmail);
  if (email != nullptr && owner != nullptr)
    ::SetWindowTextW(email, ReadControlText(owner, kIdEmail).c_str());
}

}  // namespace

HWND Show() {
  return CreateOrShow(Role::kAccount);
}

HWND ShowDevice() {
  return CreateOrShow(Role::kDevice);
}

HWND ActiveHwnd() {
  return g_windows[static_cast<int>(Role::kAccount)];
}

}  // namespace account_dialog

}  // namespace cloud
}  // namespace fluxing
