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

#include <atomic>
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
    if (service->RestoreFromStore(&restore_err)) {
      status = L"已恢复上次的登录状态。";
      logged_in = true;
      email_verified = service->info().email_verified;
      account_email = ToWide(service->info().email);
    } else {
      status = L"未登录。";
    }

    worker = std::thread([this]() { WorkerLoop(); });
  }

  ~Impl() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      running = false;
    }
    cv.notify_all();
    if (worker.joinable())
      worker.join();
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

  std::unique_ptr<ICloudTransport> transport;
  std::unique_ptr<CloudAccountService> service;
  CloudCredentialStore store;
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

bool CloudAccountController::email_verified() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->email_verified;
}

std::wstring CloudAccountController::account_email() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->account_email;
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
      } else {
        impl->logged_in = false;
        impl->status = L"登录失败：" + ToWide(err);
      }
    }
    if (!ok)
      return;
    // 登录成功后顺手刷新设备列表。
    std::vector<CloudDeviceInfo> list;
    std::string list_err;
    if (impl->service->ListDevices(&list, &list_err)) {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->devices = list;
    }
  });
}

void CloudAccountController::SubmitLogout() {
  Impl* impl = impl_;
  impl->Post([impl]() {
    std::string err;
    const bool ok = impl->service->Logout(&err);
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

// ---------------------------------------------------------------------------
// 窗口
// ---------------------------------------------------------------------------

namespace account_dialog {
namespace {

constexpr wchar_t kClassName[] = L"FluxingCloudAccountDialog_v1";
constexpr int kClientWidth = 460;
constexpr int kClientHeight = 320;

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

void RefreshUi(HWND hwnd) {
  CloudAccountController& controller = CloudAccountController::Instance();

  ::SetWindowTextW(::GetDlgItem(hwnd, kIdStatus), controller.status().c_str());

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
      AddControl(hwnd, L"STATIC", L"密码", 0, 12, 54, 70, 20, -1);
      AddControl(hwnd, L"EDIT", L"", ES_AUTOHSCROLL | ES_PASSWORD, 88, 50, 240,
                 26, kIdPassword);
      AddControl(hwnd, L"BUTTON", L"登录", BS_PUSHBUTTON, 340, 16, 100, 28,
                 kIdLogin);
      AddControl(hwnd, L"BUTTON", L"退出登录", BS_PUSHBUTTON, 340, 50, 100, 28,
                 kIdLogout);
      AddControl(hwnd, L"BUTTON", L"刷新登录状态", BS_PUSHBUTTON, 340, 84, 100,
                 28, kIdRefresh);
      AddControl(hwnd, L"BUTTON", L"清空云端数据", BS_PUSHBUTTON, 340, 118, 100,
                 28, kIdDeleteData);
      AddControl(hwnd, L"STATIC", L"", 0, 12, 92, 320, 40, kIdStatus);
      AddControl(hwnd, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL | WS_BORDER, 12,
                 138, 320, 140, kIdDevices);
      AddControl(hwnd, L"BUTTON", L"撤销所选设备", BS_PUSHBUTTON, 340, 160, 100,
                 28, kIdRevoke);
      AddControl(hwnd, L"BUTTON", L"刷新设备列表", BS_PUSHBUTTON, 340, 194, 100,
                 28, kIdRefreshDevices);
      AddControl(hwnd, L"BUTTON", L"关闭", BS_PUSHBUTTON, 340, 262, 100, 28,
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
          const std::wstring email = [&]() -> std::wstring {
            HWND edit = ::GetDlgItem(hwnd, kIdEmail);
            wchar_t buf[256] = {0};
            if (edit != nullptr)
              ::GetWindowTextW(edit, buf, 256);
            return std::wstring(buf);
          }();
          std::wstring password;
          {
            HWND edit = ::GetDlgItem(hwnd, kIdPassword);
            wchar_t buf[256] = {0};
            if (edit != nullptr)
              ::GetWindowTextW(edit, buf, 256);
            password.assign(buf);
            // 立刻抹掉读取用的缓冲；口令本体由控制器在用完后清零。
            ::SecureZeroMemory(buf, sizeof(buf));
          }
          controller.SubmitLogin(email, password);
          // 输入框里的口令立刻抹掉。
          ::SetWindowTextW(::GetDlgItem(hwnd, kIdPassword), L"");
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

}  // namespace

HWND Show() {
  if (g_window != nullptr) {
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
  ::ShowWindow(hwnd, SW_SHOW);
  ::UpdateWindow(hwnd);
  g_window = hwnd;
  return hwnd;
}

HWND ActiveHwnd() {
  return g_window;
}

}  // namespace account_dialog

}  // namespace cloud
}  // namespace fluxing
