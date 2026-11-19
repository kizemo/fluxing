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
//     未验证时明确提示「请先完成邮箱验证」。
//   · spec 100 阶段 F · T505：接入同步引擎 —— 顶部显示「最近同步」状态
//     （时间 + 结果 + 原因），并提供「立即同步」按钮（手动推 + 拉一轮）；
//   · spec 100 阶段 G 加固：提供「重新发送验证码」按钮
//     （POST /v1/auth/resend，服务端已实现）。
//
// 批次 AT-1（账号页 UI 重构）：账号页拆成 5 个窗口（主窗口 + 验证码 /
// 注册 / 找回口令 / 设备与云数据）。它们共用一个控制器 ⇒ 状态文案必须
// **分作用域**，否则主窗口与子窗口会互相覆盖对方的提示行。
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
// 状态文案的作用域
// ---------------------------------------------------------------------------
//
// 一个控制器服务多个窗口（批次 AT-1）。每个窗口有自己的一行状态提示，
// 必须互不覆盖 ⇒ 控制器按作用域分别保存文案。
enum class CloudStatusScope {
  kAccount = 0,  // 账号页主窗口（登录 / 退出 / 同步结果）
  kVerify = 1,   // 验证码窗口
  kRegister = 2, // 注册窗口
  kForgot = 3,   // 找回口令窗口
  kDevice = 4,   // 设备与云数据窗口
  kCount = 5,
};

// ---------------------------------------------------------------------------
// 控制器（进程级单例，工作线程的所有者）
// ---------------------------------------------------------------------------

class CloudAccountController {
 public:
  static CloudAccountController& Instance();

  // 窗口登记：控制器把后台任务完成消息（WM_APP_REFRESH）发给**所有**
  // 已登记的窗口，并按各自的作用域回填状态文案。
  void RegisterWindow(HWND hwnd, CloudStatusScope scope);
  void UnregisterWindow(HWND hwnd);

  // 账号页主窗口句柄（用于工作线程回传消息）。
  HWND owner_window() const;

  // 云端服务地址。默认取环境变量 FLUXING_CLOUD_BASE_URL；没有则用
  // 自有域名 api.aiec.fun（**不是** Cloudflare 默认子域 —— 境内 DNS 会污染
  // 那个，见 cloud/fluxing-sync/test/check-endpoint-policy.mjs）。
  std::wstring base_url() const;

  // ---- 提交任务（UI 线程调用，立即返回） ----
  //
  // save_account = false 时：登录**仍然成功**，但**不写**凭据存储
  // （本次登录只在内存会话生效），且**不动**已有的凭据记录。
  void SubmitLogin(const std::wstring& email,
                   const std::wstring& password,
                   bool save_account);

  // 注册（服务端生成并投递邮箱验证码）与邮箱验证。
  // 与 SubmitLogin 同样「提交即返回」：PBKDF2 与网络都在工作线程里跑，
  // UI 线程不阻塞（宪法 P2）。
  void SubmitRegister(const std::wstring& email,
                      const std::wstring& password,
                      CloudStatusScope scope);
  void SubmitVerifyEmail(const std::wstring& email,
                         const std::wstring& code,
                         CloudStatusScope scope);
  void SubmitLogout();
  void SubmitRefresh();
  void SubmitListDevices();
  void SubmitRevokeDevice(size_t index);
  void SubmitDeleteCloudData();

  // 重新发送邮箱验证码（阶段 G 加固）。
  void SubmitResendVerification(const std::wstring& email,
                                CloudStatusScope scope);
  // 立即同步：手动触发一轮「推 + 拉」。
  void SubmitSyncNow();

  // 重置口令（批次 AT-1）：POST /v1/auth/password-reset。
  // ⚠️ **服务端端点尚不存在**（由批次 AT-2 实现）⇒ 当前必然 404，
  //    客户端把服务端返回的失败原样转成中文状态行。
  void SubmitResetPassword(const std::wstring& email,
                           const std::wstring& code,
                           const std::wstring& new_password,
                           CloudStatusScope scope);

  // ---- 读取状态（UI 线程调用，内部加锁） ----
  std::wstring status() const;  // 主窗口作用域（kAccount）
  std::wstring status(CloudStatusScope scope) const;
  std::vector<CloudDeviceInfo> devices() const;
  bool logged_in() const;
  bool email_verified() const;
  std::wstring account_email() const;
  // 最近一次同步的中文描述（时间 + 结果 + 原因）；未同步过时给占位文案。
  std::wstring sync_status() const;
  // 消费「请清空口令框」的一次性请求（UI 线程调用）。
  // 只有**登录成功**才会置位；失败时口令留在输入框里，便于改错重试。
  bool ConsumeClearPasswordRequest();

  // 批次 AT-1：历史登录过的邮箱列表（UTF-8，最近在前，最多 10 条，去重）。
  // 落盘在用户数据目录下的纯文本文件里，与 Credential Manager 解耦。
  std::vector<std::string> history() const;

  // 消费「请弹验证码窗口」的请求（UI 线程调用）。登录成功但该账号邮箱
  // 尚未验证时置位；取走即清（一次性），避免重复弹窗。
  std::wstring ConsumeVerifyRequest();

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

// 打开「设备与云数据」窗口（modeless；已打开则置前）。
// 未登录时窗口照样能打开（菜单项常在），但会改云端数据的按钮置灰。
HWND ShowDevice();

}  // namespace account_dialog

}  // namespace cloud
}  // namespace fluxing
