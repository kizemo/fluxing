#pragma once
//
// spec 100 阶段 D · T304 云端账号客户端（状态机 + 设备管理）
//
// 端点（真相源：cloud/fluxing-sync/src/index.ts 的 route()）：
//   POST   /v1/auth/login    { email, auth_key, device:{id,name} }
//                               → { access_token, refresh_token, expires_in,
//                                   salt, kdf:{algo,iters}, account_id,
//                                   email_verified }
//   POST   /v1/auth/refresh  { refresh_token } → { access_token, ... }
//   GET    /v1/devices       → [ { id, device_id, name, last_seen, created,
//                                  revoked, current } ]
//   DELETE /v1/devices/:id   → { ok, id }   （:id 传
//   device_id，客户端稳定标识） DELETE /v1/account/data  → 清空云端数据
//
// 鉴权：Authorization: Bearer <access_token>（服务端 auth.ts 正则
// /^Bearer\s+(\S+)$/i）。
//
// 线程纪律（宪法 P2）：本类的所有阻塞方法（Login / Refresh / ListDevices /
//   RevokeDevice / DeleteCloudData）**必须在后台线程调用**，不得出现在
//   TSF 回调或 UI 线程的按键路径上。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。本头**故意**不包含
//   FluxingCloudEnvelope.h（那个头会拉进 <algorithm>，与 PCH 下的
//   windows.h min/max 宏冲突）；账号页（WTL，走 PCH）因此可以安全地包含本头。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "CloudCredentialStore.h"

namespace fluxing {
namespace cloud {

class ICloudTransport;

// ---------------------------------------------------------------------------
// 状态机
// ---------------------------------------------------------------------------

enum class CloudAccountState {
  kLoggedOut = 0,   // 未登录 / 本机没有凭据
  kAuthenticating,  // 登录中（正在做 60 万次 PBKDF2 与网络请求）
  kLoggedIn,        // 已登录且令牌可用
  kNeedsRelogin,  // 令牌失效（refresh 失败 / 401），需要用户重新登录
};

const char* CloudAccountStateName(CloudAccountState state);

struct CloudAccountInfo {
  std::string email;
  std::string account_id;
  std::string device_id;
  bool email_verified = false;
};

struct CloudDeviceInfo {
  std::string id;  // 服务端 UUID（DELETE 的另一合法入参）
  std::string device_id;  // 客户端稳定标识（本项目生成的 16 位十六进制）
  std::string name;
  int64_t last_seen = 0;
  int64_t created = 0;
  bool revoked = false;
  bool current = false;  // 是否是本机
};

// ---------------------------------------------------------------------------
// 服务
// ---------------------------------------------------------------------------

class CloudAccountService {
 public:
  // transport / store 的生命周期由调用方保证（必须长于本对象）。
  CloudAccountService(ICloudTransport* transport, CloudCredentialStore* store);
  ~CloudAccountService();

  CloudAccountService(const CloudAccountService&) = delete;
  CloudAccountService& operator=(const CloudAccountService&) = delete;

  CloudAccountState state() const;
  CloudAccountInfo info() const;
  std::string device_id() const;

  // 登录：派生密钥 → POST /v1/auth/login → 落盘凭据。成功后 state = kLoggedIn。
  // email_verified 为 false 时也会登录成功，但 info().email_verified 会是 false
  // （账号页据此提示「请先完成邮箱验证」）。
  bool Login(const std::string& email,
             const std::string& password,
             std::string* err);

  // 退出登录：清掉本机凭据，state = kLoggedOut。
  bool Logout(std::string* err);

  // 刷新令牌：成功则更新令牌并保持 kLoggedIn；失败 ⇒ state = kNeedsRelogin。
  bool Refresh(std::string* err);

  // 从 Credential Manager 恢复登录态。读取失败（含没有记录）⇒ 回到
  // kLoggedOut，**不算错误**（err 写中文说明，由调用方决定是否展示）。
  bool RestoreFromStore(std::string* err);

  // 列出设备。成功后 state 若是 kNeedsRelogin 会被刷新令牌救回来。
  bool ListDevices(std::vector<CloudDeviceInfo>* out, std::string* err);

  // 撤销设备：DELETE /v1/devices/<device_id>（路径参数用 device_id）。
  bool RevokeDevice(const std::string& device_id, std::string* err);

  // 清空云端数据：DELETE /v1/account/data。
  bool DeleteCloudData(std::string* err);

  // 给同步服务用的凭据读取（线程安全）。
  bool GetAccessToken(std::string* out) const;
  bool GetEncKey(std::vector<uint8_t>* out) const;

 private:
  // 统一的请求执行：加 Authorization 头、解析错误体、写中文原因。
  bool SendJson(const std::string& method,
                const std::string& path,
                const std::string& body,
                bool need_auth,
                int* status,
                std::string* response_body,
                std::string* err);

  // 从响应里取服务端的中文错误消息（{ "error": { "code", "message" } }）。
  static std::string ExtractServerError(const std::string& body, int status);

  bool EnsureFreshToken(std::string* err);

  ICloudTransport* transport_ = nullptr;
  CloudCredentialStore* store_ = nullptr;

  mutable std::mutex mutex_;
  CloudAccountState state_ = CloudAccountState::kLoggedOut;
  CloudAccountInfo info_;
  CloudCredentials creds_;
  bool has_creds_ = false;
};

}  // namespace cloud
}  // namespace fluxing
