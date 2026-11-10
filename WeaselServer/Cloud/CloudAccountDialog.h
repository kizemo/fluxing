#pragma once
//
// spec 100 阶段 D · T305 云端账号页
//
// 内容（对应 tasks.md T305）：
//   · 登录 / 退出登录 / 刷新登录；
//   · 设备列表 + 撤销设备（DELETE /v1/devices/<device_id>，路径参数用
//     客户端稳定标识 device_id）；
//   · 清空云端数据（DELETE /v1/account/data）；
//   · 消费 login 响应的 email_verified ——
//   未验证时明确提示「请先完成邮箱验证」。
//   · spec 100 阶段 F · T505：接入同步引擎 —— 顶部显示「最近同步」状态
//     （时间 + 结果 + 原因），并提供「立即同步」按钮（手动推 + 拉一轮）；
//   · spec 100 阶段 G 加固：提供「重新发送验证码」按钮
//     （POST /v1/auth/resend，服务端已实现）。
//
// 线程纪律（宪法 P2）：
//   · **所有网络调用都在后台线程**（CloudAccountController 的工作线程），
//     UI 线程只收消息、只画界面；
//   · 不在 TSF 回调里做任何 I/O。
//
// 界面范式：跟 AboutDialog 一致 —— 普通 Win32 窗口（modeless），不引 WTL，
//   也不用 MessageBox（它会阻塞服务端 UI 线程）。
//
// PCH 注意：本 .cpp 使用 `PrecompiledHeader=NotUsing` 并自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>
#include <vector>

#include "CloudAccount.h"

namespace fluxing {
namespace cloud {

// ---------------------------------------------------------------------------
// 控制器（进程级单例，工作线程的所有者）
// ---------------------------------------------------------------------------

class CloudAccountController {
 public:
  static CloudAccountController& Instance();

  // 账号页窗口句柄（用于工作线程回传消息）。
  void SetOwnerWindow(HWND hwnd);
  HWND owner_window() const;

  // 云端服务地址。默认取环境变量 FLUXING_CLOUD_BASE_URL；没有则用
  // 自有域名 api.aiec.fun（**不是** Cloudflare 默认子域 —— 境内 DNS 会污染
  // 那个，见 cloud/fluxing-sync/test/check-endpoint-policy.mjs）。
  std::wstring base_url() const;

  // ---- 提交任务（UI 线程调用，立即返回） ----
  void SubmitLogin(const std::wstring& email, const std::wstring& password);

  // 注册（服务端生成并投递邮箱验证码）与邮箱验证。
  // 与 SubmitLogin 同样「提交即返回」：PBKDF2 与网络都在工作线程里跑，
  // UI 线程不阻塞（宪法 P2）。
  void SubmitRegister(const std::wstring& email, const std::wstring& password);
  void SubmitVerifyEmail(const std::wstring& email, const std::wstring& code);
  void SubmitLogout();
  void SubmitRefresh();
  void SubmitListDevices();
  void SubmitRevokeDevice(size_t index);
  void SubmitDeleteCloudData();

  // 重新发送邮箱验证码（阶段 G 加固）。
  void SubmitResendVerification(const std::wstring& email);
  // 立即同步：手动触发一轮「推 + 拉」。
  void SubmitSyncNow();

  // ---- 读取状态（UI 线程调用，内部加锁） ----
  std::wstring status() const;
  std::vector<CloudDeviceInfo> devices() const;
  bool logged_in() const;
  bool email_verified() const;
  std::wstring account_email() const;
  // 最近一次同步的中文描述（时间 + 结果 + 原因）；未同步过时给占位文案。
  std::wstring sync_status() const;

 private:
  CloudAccountController();
  ~CloudAccountController();
  CloudAccountController(const CloudAccountController&) = delete;
  CloudAccountController& operator=(const CloudAccountController&) = delete;

  void WorkerMain();

  class Impl;
  Impl* impl_ = nullptr;
};

// ---------------------------------------------------------------------------
// 窗口
// ---------------------------------------------------------------------------

namespace account_dialog {

// 打开账号页（modeless；已打开则置前）。返回窗口句柄（失败为 NULL）。
HWND Show();

// 已打开的窗口句柄（没有则 NULL）。
HWND ActiveHwnd();

}  // namespace account_dialog

}  // namespace cloud
}  // namespace fluxing
