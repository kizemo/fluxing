// spec 100 阶段 D · T305 云端账号页实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudAccountDialog.h"

#include "CloudAccount.h"
#include "CloudCredentialStore.h"
#include "CloudCrypto.h"
#include "CloudTransport.h"

// spec 100 阶段 F · T505：账号页接入同步引擎（最近同步状态 + 立即同步）。
// spec 100 阶段 G 加固：重新发送邮箱验证码。
#include "CloudSyncEngine.h"
#include "CloudSyncNotifier.h"
#include "CloudSyncService.h"
#include "CloudUserDictIo.h"

// 用户数据目录真相源（同步 sidecar 的落点）。
#include "../../include/FluxingUserPaths.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace fluxing {
namespace cloud {

namespace {

// 默认云端地址（自有域名 api.aiec.fun）。
// 刻意**不用** Cloudflare 默认子域 —— 境内 DNS 会污染它
// （见 cloud/fluxing-sync/test/check-endpoint-policy.mjs 的 T103 红线）。
constexpr char kDefaultBaseUrl[] = "https://api.aiec.fun";
constexpr wchar_t kBaseUrlEnvVar[] = L"FLUXING_CLOUD_BASE_URL";

constexpr UINT WM_APP_REFRESH = WM_APP + 1;

enum ControlId {
  kIdEmail = 1001,
  kIdPassword = 1002,
  kIdLogin = 1003,
  kIdLogout = 1004,
  kIdRefresh = 1005,
  kIdStatus = 1006,
  kIdDevices = 1007,
  kIdRevoke = 1008,
  kIdDeleteData = 1009,
  kIdClose = 1010,
  kIdRefreshDevices = 1011,
  kIdVerifyCode = 1012,
  kIdRegister = 1013,
  kIdVerify = 1014,
  // spec 100 阶段 F · T505 / 阶段 G：追加在末尾，**不要**改动已有值。
  kIdResend = 1015,
  kIdSyncNow = 1016,
  kIdSyncStatus = 1017,
};

std::wstring ToWide(const std::string& s) {
  if (s.empty())
    return std::wstring();
  const int need = ::MultiByteToWideChar(
      CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  if (need <= 0)
    return std::wstring();
  std::wstring w(static_cast<size_t>(need), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], need);
  return w;
}

std::string ToUtf8(const std::wstring& w) {
  if (w.empty())
    return std::string();
  const int need =
      ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                            nullptr, 0, nullptr, nullptr);
  if (need <= 0)
    return std::string();
  std::string s(static_cast<size_t>(need), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &s[0], need, nullptr, nullptr);
  return s;
}

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
      status = L"已恢复上次的登录状态。";
      logged_in = true;
      email_verified = service->info().email_verified;
      account_email = ToWide(service->info().email);
      restored = true;
    } else {
      status = L"未登录。";
    }

    worker = std::thread([this]() { WorkerLoop(); });

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
      HWND hwnd = nullptr;
      {
        std::lock_guard<std::mutex> lock(mutex);
        hwnd = owner;
      }
      if (hwnd != nullptr)
        ::PostMessageW(hwnd, WM_APP_REFRESH, 0, 0);
    }
  }

  void Post(std::function<void()> task) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      tasks.push_back(std::move(task));
    }
    cv.notify_one();
  }

  void SetStatus(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(mutex);
    status = text;
  }

  // 写「最近同步」文案（UI 线程可读）。
  // 锁序铁律：一律「先 sync_mutex / engine_mutex，后 impl->mutex」，
  // 绝不可反（否则死锁）。
  void SetSyncStatus(const std::wstring& text) {
    std::lock_guard<std::mutex> lock(mutex);
    sync_status_text = text;
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
    } else {
      std::string reason = err.empty() ? out->message : err;
      if (reason.empty())
        reason = "未知原因";
      out->message = reason;
      SetSyncStatus(L"最近同步：" + when + L" · 失败（" + ToWide(reason) +
                    L"）");
    }

    // 通知账号页刷新（窗口可能已经关了 ⇒ owner 为空时跳过）。
    HWND hwnd = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex);
      hwnd = owner;
    }
    if (hwnd != nullptr)
      ::PostMessageW(hwnd, WM_APP_REFRESH, 0, 0);
    return ok;
  }

  mutable std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::function<void()>> tasks;
  std::thread worker;
  std::atomic<bool> running{true};

  HWND owner = nullptr;
  std::wstring status = L"未登录。";
  std::vector<CloudDeviceInfo> devices;
  bool logged_in = false;
  bool email_verified = false;
  std::wstring account_email;
  std::wstring base_url;
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

void CloudAccountController::SetOwnerWindow(HWND hwnd) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->owner = hwnd;
}

HWND CloudAccountController::owner_window() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->owner;
}

std::wstring CloudAccountController::base_url() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->base_url;
}

std::wstring CloudAccountController::status() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->status;
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

void CloudAccountController::SubmitLogin(const std::wstring& email,
                                         const std::wstring& password) {
  const std::string email_utf8 = ToUtf8(email);
  std::string password_utf8 = ToUtf8(password);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, password_utf8]() mutable {
    std::string err;
    const bool ok = impl->service->Login(email_utf8, password_utf8, &err);
    // 口令用后清零（不落日志、不留副本）。lambda 必须是 mutable：
    // 捕获的 password_utf8 默认是 const，清零函数要的是可写指针。
    SecureZeroString(&password_utf8);
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      if (ok) {
        impl->logged_in = true;
        impl->email_verified = impl->service->info().email_verified;
        impl->account_email = ToWide(impl->service->info().email);
        impl->status = impl->email_verified
                           ? L"登录成功。"
                           : L"登录成功，但邮箱尚未验证，请先完成邮箱验证。";
        // 只有「已登录**且**邮箱已验证」才清口令框：
        // ① 失败时保留 ⇒ 用户可改错重试；
        // ② 「登录成功但邮箱未验证」也保留 ⇒ 用户验证完邮箱还要用**同一口令**
        //    再登录一次，提前清掉会逼他重敲一遍（真机闭环实测踩到过）。
        impl->clear_password_request = impl->email_verified;
      } else {
        impl->logged_in = false;
        impl->status = L"登录失败：" + ToWide(err);
      }
    }
    if (!ok)
      return;
    // 登录成功 ⇒ 启动同步引擎（幂等：已在跑就直接返回）。
    impl->StartEngine();
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
                                            const std::wstring& password) {
  const std::string email_utf8 = ToUtf8(email);
  std::string password_utf8 = ToUtf8(password);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, password_utf8]() mutable {
    std::string account_id;
    std::string err;
    const bool ok =
        impl->service->Register(email_utf8, password_utf8, &account_id, &err);
    // 口令用后清零（闭包副本 + 服务内部副本都在这里收口）。
    SecureZeroString(&password_utf8);
    // 注册成功**不**置 logged_in：服务端返回 need_email_verify=true，
    // 必须先完成邮箱验证再登录，避免出现「已登录但邮箱未验证」的脏态。
    impl->SetStatus(ok ? L"注册成功：请在验证码框输入 6 位验证码完成邮箱验证。"
                       : (L"注册失败：" + ToWide(err)));
  });
}

void CloudAccountController::SubmitVerifyEmail(const std::wstring& email,
                                               const std::wstring& code) {
  const std::string email_utf8 = ToUtf8(email);
  const std::string code_utf8 = ToUtf8(code);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8, code_utf8]() {
    std::string err;
    const bool ok = impl->service->VerifyEmail(email_utf8, code_utf8, &err);
    // 验证同样不改登录态：验证通过后由用户点「登录」拿令牌。
    impl->SetStatus(ok ? L"邮箱验证成功，请登录。"
                       : (L"邮箱验证失败：" + ToWide(err)));
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
    impl->status = ok ? L"已退出登录。" : (L"退出登录失败：" + ToWide(err));
  });
}

void CloudAccountController::SubmitRefresh() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::string err;
    const bool ok = impl->service->Refresh(&err);
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->logged_in = ok;
    impl->status = ok ? L"登录状态已刷新。" : (L"刷新失败：" + ToWide(err));
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
      impl->status = L"已刷新设备列表。";
    } else {
      impl->status = L"获取设备列表失败：" + ToWide(err);
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
    impl->status = ok ? (L"已撤销设备 " + ToWide(device_id))
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
    impl->status = ok ? L"云端数据已清空（账号本身保留）。"
                      : (L"清空云端数据失败：" + ToWide(err));
  });
}

void CloudAccountController::SubmitResendVerification(
    const std::wstring& email) {
  const std::string email_utf8 = ToUtf8(email);
  Impl* impl = impl_;
  impl->Post([impl, email_utf8]() {
    std::string err;
    const bool ok = impl->service->ResendVerification(email_utf8, &err);
    // 重发不改登录态，也**不清空验证码输入框**：失败时（码过期 / 已被
    // 用掉）用户还能改了再提交。
    impl->SetStatus(ok ? L"验证码已重新发送，请查收。"
                       : (L"重新发送验证码失败：" + ToWide(err)));
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
      impl->SetStatus(L"手动同步已提交，完成后见下方「最近同步」。");
      return;
    }

    // 引擎没起来（未登录 / 恢复态启动失败）：退化为直接跑一轮，
    // 好让用户看到中文原因，而不是点了毫无反馈。
    CloudSyncRoundResult result;
    const bool ok = impl->RunSyncRound(SyncDirection::kBoth, &result);
    if (ok) {
      impl->SetStatus(L"同步完成：推送 " +
                      std::to_wstring(result.items_pushed) + L" 条，拉取 " +
                      std::to_wstring(result.items_pulled) + L" 条。");
    } else {
      const std::wstring reason = result.message.empty()
                                      ? std::wstring(L"未知原因")
                                      : ToWide(result.message);
      impl->SetStatus(L"同步失败：" + reason);
    }
  });
}

// ---------------------------------------------------------------------------
// 窗口
// ---------------------------------------------------------------------------

namespace account_dialog {
namespace {

constexpr wchar_t kClassName[] = L"FluxingCloudAccountDialog_v1";
constexpr int kClientWidth = 480;
// 高度 320 → 410：新增「验证码」一行后，状态框、设备列表与底部两行按钮
// 依次下移，避免与状态框重叠。
// 410 → 472（阶段 F · T505 / 阶段 G）：新增「重新发送验证码」按钮与
// 「最近同步」状态行，设备列表与底部按钮整体下移，底部按钮拆成两行。
constexpr int kClientHeight = 472;

HWND g_window = nullptr;
HFONT g_font = nullptr;

void ApplyFont(HWND hwnd) {
  if (g_font != nullptr && hwnd != nullptr)
    ::SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
}

HWND AddControl(HWND parent,
                const wchar_t* cls,
                const wchar_t* text,
                DWORD style,
                int x,
                int y,
                int w,
                int h,
                int id) {
  HWND hwnd = ::CreateWindowExW(
      0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      ::GetModuleHandleW(nullptr), nullptr);
  ApplyFont(hwnd);
  return hwnd;
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

// 读取控件文本（宽字符）。缓冲区用后立刻抹掉 —— 口令与验证码都走这条路。
std::wstring ReadControlText(HWND parent, int id) {
  HWND ctrl = ::GetDlgItem(parent, id);
  if (ctrl == nullptr)
    return std::wstring();
  wchar_t buf[256] = {0};
  ::GetWindowTextW(ctrl, buf, 256);
  std::wstring value(buf);
  ::SecureZeroMemory(buf, sizeof(buf));
  return value;
}

// 主动清零宽字符串本体：std::wstring 析构不清堆内存，口令会留在页里。
void SecureZeroWide(std::wstring* s) {
  if (s == nullptr || s->empty())
    return;
  ::SecureZeroMemory(&(*s)[0], s->size() * sizeof(wchar_t));
  s->clear();
}

void RefreshUi(HWND hwnd) {
  CloudAccountController& controller = CloudAccountController::Instance();

  ::SetWindowTextW(::GetDlgItem(hwnd, kIdStatus), controller.status().c_str());
  ::SetWindowTextW(::GetDlgItem(hwnd, kIdSyncStatus),
                   controller.sync_status().c_str());
  // 「已登录且邮箱已验证」⇒ 清空口令框（一次性请求，读取即消费）。
  // 失败 / 注册 / 「已登录但邮箱未验证」路径**不**清 ⇒ 用户可以直接改错重试，
  // 验证完邮箱后也能用同一口令直接登录。
  if (controller.ConsumeClearPasswordRequest())
    ::SetWindowTextW(::GetDlgItem(hwnd, kIdPassword), L"");

  HWND list = ::GetDlgItem(hwnd, kIdDevices);
  if (list != nullptr) {
    ::SendMessageW(list, LB_RESETCONTENT, 0, 0);
    const std::vector<CloudDeviceInfo> devices = controller.devices();
    for (size_t i = 0; i < devices.size(); ++i)
      ::SendMessageW(list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(DeviceLabel(devices[i]).c_str()));
  }

  const bool logged_in = controller.logged_in();
  ::EnableWindow(::GetDlgItem(hwnd, kIdLogout), logged_in);
  ::EnableWindow(::GetDlgItem(hwnd, kIdRefresh), logged_in);
  ::EnableWindow(::GetDlgItem(hwnd, kIdDeleteData), logged_in);
  ::EnableWindow(::GetDlgItem(hwnd, kIdLogin), !logged_in);
  ::EnableWindow(::GetDlgItem(hwnd, kIdRevoke), logged_in);
  // 注册与邮箱验证只在未登录时可用：已登录状态下它们没有意义，
  // 也避免用另一个邮箱重复注册把当前会话搅乱。
  ::EnableWindow(::GetDlgItem(hwnd, kIdRegister), !logged_in);
  ::EnableWindow(::GetDlgItem(hwnd, kIdVerify), !logged_in);
  // 重新发送验证码与注册 / 邮箱验证同属「未登录」一组。
  ::EnableWindow(::GetDlgItem(hwnd, kIdResend), !logged_in);
  // 「立即同步」只在已登录时可用（未登录时点了只会得到「尚未登录」提示）。
  ::EnableWindow(::GetDlgItem(hwnd, kIdSyncNow), logged_in);
}

LRESULT CALLBACK WndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) {
  switch (message) {
    case WM_CREATE: {
      CloudAccountController& controller = CloudAccountController::Instance();
      controller.SetOwnerWindow(hwnd);
      g_window = hwnd;

      AddControl(hwnd, L"STATIC", L"邮箱地址", 0, 12, 20, 70, 20, -1);
      AddControl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL, 88, 16, 240, 26, kIdEmail);
      AddControl(hwnd, L"BUTTON", L"登录", BS_PUSHBUTTON, 340, 16, 128, 28,
                 kIdLogin);
      AddControl(hwnd, L"STATIC", L"密码", 0, 12, 54, 70, 20, -1);
      AddControl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | ES_PASSWORD, 88, 50, 240,
                 26, kIdPassword);
      AddControl(hwnd, L"BUTTON", L"退出登录", BS_PUSHBUTTON, 340, 50, 128, 28,
                 kIdLogout);
      AddControl(hwnd, L"STATIC", L"验证码", 0, 12, 88, 70, 20, -1);
      AddControl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | ES_NUMBER, 88, 84, 240,
                 26, kIdVerifyCode);
      AddControl(hwnd, L"BUTTON", L"注册并发送验证码", BS_PUSHBUTTON, 340, 84,
                 128, 28, kIdRegister);
      AddControl(hwnd, L"BUTTON", L"验证邮箱", BS_PUSHBUTTON, 340, 118, 128, 28,
                 kIdVerify);
      AddControl(hwnd, L"BUTTON", L"重新发送验证码", BS_PUSHBUTTON, 340, 152,
                 128, 28, kIdResend);
      AddControl(hwnd, L"STATIC", L"", 0, 12, 188, 456, 40, kIdStatus);
      AddControl(hwnd, L"STATIC", L"最近同步：尚未同步。", 0, 12, 234, 456, 20,
                 kIdSyncStatus);
      AddControl(hwnd, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER, 12,
                 262, 456, 130, kIdDevices);
      AddControl(hwnd, L"BUTTON", L"撤销所选设备", BS_PUSHBUTTON, 12, 398, 140,
                 28, kIdRevoke);
      AddControl(hwnd, L"BUTTON", L"刷新设备列表", BS_PUSHBUTTON, 168, 398, 140,
                 28, kIdRefreshDevices);
      AddControl(hwnd, L"BUTTON", L"刷新登录状态", BS_PUSHBUTTON, 324, 398, 144,
                 28, kIdRefresh);
      AddControl(hwnd, L"BUTTON", L"清空云端数据", BS_PUSHBUTTON, 12, 432, 140,
                 28, kIdDeleteData);
      AddControl(hwnd, L"BUTTON", L"立即同步", BS_PUSHBUTTON, 168, 432, 140, 28,
                 kIdSyncNow);
      AddControl(hwnd, L"BUTTON", L"关闭", BS_PUSHBUTTON, 324, 432, 144, 28,
                 kIdClose);

      RefreshUi(hwnd);
      return 0;
    }

    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      CloudAccountController& controller = CloudAccountController::Instance();
      switch (id) {
        case kIdLogin: {
          // 邮箱保持宽字符：SubmitLogin 内部再转 UTF-8 交给云端服务。
          const std::wstring email = ReadControlText(hwnd, kIdEmail);
          std::wstring password = ReadControlText(hwnd, kIdPassword);
          controller.SubmitLogin(email, password);
          // 口令用完立刻抹掉**堆上**的宽字符串副本。
          // 输入框**不**在此清空：登录失败时要能改错重试（否则用户得
          // 重新敲一遍口令）；成功后再由 RefreshUi 统一清空。
          SecureZeroWide(&password);
          return 0;
        }
        case kIdRegister: {
          // 注册只请求服务端建号并发验证码，**不**建立登录态；
          // 必须「验证邮箱 → 登录」两步才能拿到令牌（need_email_verify）。
          const std::wstring email = ReadControlText(hwnd, kIdEmail);
          std::wstring password = ReadControlText(hwnd, kIdPassword);
          controller.SubmitRegister(email, password);
          // 同上：注册失败保留口令便于重试；注册成功后用户还要用**同一**
          // 口令登录，因此输入框一律不在这里清空。
          SecureZeroWide(&password);
          return 0;
        }
        case kIdVerify: {
          const std::wstring email = ReadControlText(hwnd, kIdEmail);
          std::wstring code = ReadControlText(hwnd, kIdVerifyCode);
          controller.SubmitVerifyEmail(email, code);
          // 验证码用后清零本地副本，但保留输入框内容：
          // 验证失败（码错/过期）时用户还能改了再提交。
          SecureZeroWide(&code);
          return 0;
        }
        case kIdLogout:
          controller.SubmitLogout();
          return 0;
        case kIdRefresh:
          controller.SubmitRefresh();
          return 0;
        case kIdDeleteData:
          controller.SubmitDeleteCloudData();
          return 0;
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
        case kIdResend: {
          // 重发验证码：读邮箱框 → 后台线程发请求（不在 UI 线程做网络 I/O）。
          // **不清空验证码输入框**：失败时用户还能改了再提交。
          const std::wstring email = ReadControlText(hwnd, kIdEmail);
          controller.SubmitResendVerification(email);
          return 0;
        }
        case kIdSyncNow:
          controller.SubmitSyncNow();
          return 0;
        case kIdClose:
          ::DestroyWindow(hwnd);
          return 0;
        default:
          break;
      }
      return 0;
    }

    case WM_APP_REFRESH:
      RefreshUi(hwnd);
      return 0;

    case WM_CLOSE:
      ::DestroyWindow(hwnd);
      return 0;

    case WM_DESTROY: {
      CloudAccountController& controller = CloudAccountController::Instance();
      if (controller.owner_window() == hwnd)
        controller.SetOwnerWindow(nullptr);
      g_window = nullptr;
      return 0;
    }
  }
  return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

bool EnsureClass() {
  static bool registered = false;
  if (registered)
    return true;
  WNDCLASSEXW wc;
  ::ZeroMemory(&wc, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = ::GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  wc.lpszClassName = kClassName;
  wc.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
  if (::RegisterClassExW(&wc) == 0)
    return false;
  registered = true;
  return true;
}

// 保证窗口真的可见。**不用 ShowWindow 来显示**:
//   进程内**首次** ShowWindow 的 nCmdShow 会被启动信息
//   (STARTUPINFO + STARTF_USESHOWWINDOW)覆盖 —— WeaselServer 以隐藏/最小化
//   方式拉起时, 首次 ShowWindow(SW_SHOW) 被改写成 SW_HIDE, 窗口从此停在
//   隐藏态, 而 g_window 已被缓存 ⇒ 之后每次点击都只是"对一个隐藏窗
//   SetForegroundWindow", 用户看到的就是"点菜单毫无反应"(2026-11-11 真机
//   缺陷, 探针实证: 窗口存在但 WS_VISIBLE=0)。
//   SetWindowPos(SWP_SHOWWINDOW) 直接置 WS_VISIBLE 位, 不走 ShowWindow 的
//   nCmdShow 语义, 因此不受该覆盖影响。
void EnsureWindowShown(HWND hwnd) {
  if (hwnd == nullptr || !::IsWindow(hwnd))
    return;
  if (::IsIconic(hwnd))
    ::ShowWindow(hwnd, SW_RESTORE);
  if (!::IsWindowVisible(hwnd))
    ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  ::UpdateWindow(hwnd);
}

}  // namespace

HWND Show() {
  if (g_window != nullptr && !::IsWindow(g_window)) {
    // 缓存的句柄已失效(窗口被销毁但 WM_DESTROY 没走到本缓存): 丢掉重建,
    // 否则此后每次点击都只是对一个死句柄置前。
    g_window = nullptr;
  }
  if (g_window != nullptr) {
    EnsureWindowShown(g_window);
    ::SetForegroundWindow(g_window);
    return g_window;
  }
  if (!EnsureClass())
    return nullptr;

  if (g_font == nullptr)
    g_font = reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));

  RECT rect = {0, 0, kClientWidth, kClientHeight};
  ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd =
      ::CreateWindowExW(WS_EX_APPWINDOW, kClassName, L"Fluxing 云端账号",
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                        rect.right - rect.left, rect.bottom - rect.top, nullptr,
                        nullptr, ::GetModuleHandleW(nullptr), nullptr);
  if (hwnd == nullptr)
    return nullptr;
  EnsureWindowShown(hwnd);
  g_window = hwnd;
  ::SetForegroundWindow(g_window);
  return g_window;
}

HWND ActiveHwnd() {
  return g_window;
}

}  // namespace account_dialog

}  // namespace cloud
}  // namespace fluxing
