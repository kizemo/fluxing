// spec 100 阶段 D · T304 账号客户端实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudAccount.h"

#include "CloudCrypto.h"
#include "CloudCredentialStore.h"
#include "CloudJson.h"
#include "CloudTransport.h"

// base64 用信封头里的实现（同一真相源，已有 134 条单测覆盖）。
#include "../../include/FluxingCloudEnvelope.h"

namespace fluxing {
namespace cloud {

namespace {

// nodes 是平铺数组，Member() 只回指针；需要「节点下标」时用它换算回来。
int NodeIndex(const json::JsonDocument& doc, const json::JsonNode* node) {
  if (node == nullptr)
    return -1;
  return static_cast<int>(node - doc.nodes.data());
}

constexpr char kDeviceName[] = "Fluxing";

}  // namespace

const char* CloudAccountStateName(CloudAccountState state) {
  switch (state) {
    case CloudAccountState::kLoggedOut:
      return "未登录";
    case CloudAccountState::kAuthenticating:
      return "登录中";
    case CloudAccountState::kLoggedIn:
      return "已登录";
    case CloudAccountState::kNeedsRelogin:
      return "需要重新登录";
  }
  return "未知";
}

CloudAccountService::CloudAccountService(ICloudTransport* transport,
                                         CloudCredentialStore* store)
    : transport_(transport), store_(store) {}

CloudAccountService::~CloudAccountService() = default;

CloudAccountState CloudAccountService::state() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return state_;
}

CloudAccountInfo CloudAccountService::info() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return info_;
}

std::string CloudAccountService::device_id() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return info_.device_id;
}

bool CloudAccountService::GetAccessToken(std::string* out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!has_creds_ || creds_.access_token.empty())
    return false;
  if (out)
    *out = creds_.access_token;
  return true;
}

bool CloudAccountService::GetEncKey(std::vector<uint8_t>* out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!has_creds_ || creds_.enc_key_b64.empty())
    return false;
  if (out == nullptr)
    return false;
  return Base64Decode(creds_.enc_key_b64, out);
}

// ---------------------------------------------------------------------------
// 错误消息
// ---------------------------------------------------------------------------

std::string CloudAccountService::ExtractServerError(const std::string& body,
                                                    int status) {
  json::JsonDocument doc;
  if (json::Parse(body, &doc) && doc.root >= 0) {
    const json::JsonNode* err_node = json::Member(doc, doc.root, "error");
    const int err_index = NodeIndex(doc, err_node);
    if (err_index >= 0) {
      std::string message;
      if (json::MemberString(doc, err_index, "message", &message) &&
          !message.empty())
        return message;
    }
  }
  return "服务器返回错误（HTTP " + std::to_string(status) + "）。";
}

// ---------------------------------------------------------------------------
// 请求
// ---------------------------------------------------------------------------

bool CloudAccountService::SendJson(const std::string& method,
                                   const std::string& path,
                                   const std::string& body,
                                   bool need_auth,
                                   int* status,
                                   std::string* response_body,
                                   std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  CloudHttpRequest req;
  req.method = method;
  req.path = path;
  req.headers.push_back({"Accept", "application/json"});
  if (!body.empty())
    req.headers.push_back({"Content-Type", "application/json"});

  if (need_auth) {
    std::string token;
    if (!GetAccessToken(&token)) {
      if (err)
        *err = "尚未登录，无法访问云端服务。";
      return false;
    }
    req.headers.push_back({"Authorization", "Bearer " + token});
  }
  req.body = body;

  const CloudHttpResponse resp = transport_->Send(req);
  if (!resp.transport_ok) {
    if (err)
      *err = "网络请求失败：" + (resp.error.empty() ? "未知原因" : resp.error);
    return false;
  }
  if (status)
    *status = resp.status;
  if (response_body)
    *response_body = resp.body;

  if (resp.status >= 200 && resp.status < 300)
    return true;
  if (err)
    *err = ExtractServerError(resp.body, resp.status);
  return false;
}

// ---------------------------------------------------------------------------
// 登录 / 登出 / 刷新
// ---------------------------------------------------------------------------

bool CloudAccountService::Login(const std::string& email,
                                const std::string& password,
                                std::string* err,
                                bool persist) {
  if (transport_ == nullptr || store_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }
  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }
  if (norm.find('@') == std::string::npos) {
    if (err)
      *err = "邮箱格式不正确。";
    return false;
  }
  if (password.empty()) {
    if (err)
      *err = "请输入密码。";
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kAuthenticating;
  }

  // 口令副本：派生完立刻清零（原始入参是 const 引用，清不了）。
  std::string password_copy = password;
  DerivedKeys keys;
  std::string derive_err;
  const bool derived = DeriveKeys(norm, password_copy, 0, &keys, &derive_err);
  SecureZeroString(&password_copy);
  if (!derived) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = derive_err.empty() ? "密钥派生失败。" : derive_err;
    return false;
  }

  std::string device_id;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    device_id = info_.device_id;
  }
  if (device_id.empty() && !GenerateDeviceId(&device_id, &derive_err)) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = derive_err.empty() ? "无法生成设备标识。" : derive_err;
    return false;
  }

  const std::string auth_key_b64 = EncodeAuthKey(keys.auth_key);
  const std::string body = "{\"email\":" + json::Quote(norm) +
                           ",\"auth_key\":" + json::Quote(auth_key_b64) +
                           ",\"device\":{\"id\":" + json::Quote(device_id) +
                           ",\"name\":" + json::Quote(kDeviceName) + "}}";

  int status = 0;
  std::string response_body;
  std::string send_err;
  const bool ok = SendJson("POST", "/v1/auth/login", body, false, &status,
                           &response_body, &send_err);
  SecureZeroBytes(&keys.auth_key);
  if (!ok) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = send_err.empty() ? "登录失败。" : send_err;
    return false;
  }

  json::JsonDocument doc;
  if (!json::Parse(response_body, &doc) || doc.root < 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = "服务器返回的数据无法解析：" +
             (doc.error.empty() ? std::string("未知原因") : doc.error);
    return false;
  }

  CloudCredentials creds;
  creds.email = norm;
  creds.device_id = device_id;
  creds.auth_key_b64 = auth_key_b64;
  creds.enc_key_b64 = Base64Encode(keys.enc_key);
  if (!json::MemberString(doc, doc.root, "access_token", &creds.access_token) ||
      creds.access_token.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = "服务器没有返回访问令牌。";
    return false;
  }
  if (!json::MemberString(doc, doc.root, "refresh_token",
                          &creds.refresh_token) ||
      creds.refresh_token.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = "服务器没有返回刷新令牌。";
    return false;
  }
  json::MemberInt64(doc, doc.root, "expires_in", &creds.expires_in);
  json::MemberString(doc, doc.root, "account_id", &creds.account_id);
  json::MemberString(doc, doc.root, "salt", &creds.server_salt_b64);
  // email_verified：服务器附加字段（plan §6.1 未列），账号页据此提示
  // 「请先完成邮箱验证」。
  json::MemberBool(doc, doc.root, "email_verified", &creds.email_verified);
  SecureZeroBytes(&keys.enc_key);

  // 落盘（批次 AT-1）：persist=false 时**不写**凭据存储 —— 登录照样成功，
  // 但本次登录只在内存会话生效；已有的凭据记录原样保留（不覆盖、不删除）。
  if (persist) {
    std::string save_err;
    if (!store_->Save(creds, &save_err)) {
      std::lock_guard<std::mutex> lock(mutex_);
      state_ = CloudAccountState::kLoggedOut;
      if (err)
        *err = save_err.empty() ? "保存云端凭据失败。" : save_err;
      return false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    creds_ = creds;
    has_creds_ = true;
    creds_persisted_ = persist;
    state_ = CloudAccountState::kLoggedIn;
    info_.email = creds.email;
    info_.account_id = creds.account_id;
    info_.device_id = creds.device_id;
    info_.email_verified = creds.email_verified;
  }
  return true;
}

// ---------------------------------------------------------------------------
// 注册 / 邮箱验证（阶段 E 补口：此前客户端只有登录，注册链路完全缺失）
// ---------------------------------------------------------------------------

bool CloudAccountService::Register(const std::string& email,
                                   const std::string& password,
                                   std::string* out_account_id,
                                   std::string* err) {
  if (out_account_id != nullptr)
    out_account_id->clear();
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }
  if (norm.find('@') == std::string::npos ||
      norm.find('.') == std::string::npos) {
    if (err)
      *err = "邮箱格式不正确。";
    return false;
  }
  if (password.empty()) {
    if (err)
      *err = "请输入密码。";
    return false;
  }

  // 与 Login 同一套派生参数（ADR 0054：PBKDF2-HMAC-SHA256 60 万次，
  // salt 按邮箱确定性派生）。注册只上传 auth_key，口令本体不出本机。
  std::string password_copy = password;
  DerivedKeys keys;
  std::string derive_err;
  const bool derived = DeriveKeys(norm, password_copy, 0, &keys, &derive_err);
  SecureZeroString(&password_copy);
  if (!derived) {
    if (err)
      *err = derive_err.empty() ? "密钥派生失败。" : derive_err;
    return false;
  }

  // 设备标识：本机已有就沿用（随后 Login 复用同一个），没有就生成并记住，
  // 让「注册 → 验证 → 登录」三步在服务端看到的是同一台设备。
  std::string device_id;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    device_id = info_.device_id;
  }
  if (device_id.empty()) {
    if (!GenerateDeviceId(&device_id, &derive_err)) {
      SecureZeroBytes(&keys.auth_key);
      SecureZeroBytes(&keys.enc_key);
      if (err)
        *err = derive_err.empty() ? "无法生成设备标识。" : derive_err;
      return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    info_.device_id = device_id;
  }

  const std::string body =
      "{\"email\":" + json::Quote(norm) +
      ",\"auth_key\":" + json::Quote(EncodeAuthKey(keys.auth_key)) +
      ",\"device\":{\"id\":" + json::Quote(device_id) +
      ",\"name\":" + json::Quote(kDeviceName) + "}}";

  int status = 0;
  std::string response_body;
  std::string send_err;
  const bool ok = SendJson("POST", "/v1/auth/register", body, false, &status,
                           &response_body, &send_err);
  SecureZeroBytes(&keys.auth_key);
  SecureZeroBytes(&keys.enc_key);
  if (!ok) {
    if (err)
      *err = send_err.empty() ? "注册失败。" : send_err;
    return false;
  }

  // 注册成功 = 服务端已建号并生成了邮箱验证码。
  // **不落凭据、不改 state_**：必须 VerifyEmail + Login 才拿到令牌，
  // 避免出现「半登录」脏态（服务端也是这个语义：need_email_verify）。
  json::JsonDocument doc;
  std::string account_id;
  if (json::Parse(response_body, &doc) && doc.root >= 0)
    json::MemberString(doc, doc.root, "account_id", &account_id);
  if (out_account_id != nullptr)
    *out_account_id = account_id;
  return true;
}

bool CloudAccountService::VerifyEmail(const std::string& email,
                                      const std::string& code,
                                      std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }
  // 服务端 auth.ts handleVerify 只接受 /^\d{6}$/：本地先挡掉，省一次无效往返。
  if (code.size() != 6 ||
      code.find_first_not_of("0123456789") != std::string::npos) {
    if (err)
      *err = "验证码格式不正确（应为 6 位数字）。";
    return false;
  }

  const std::string body = "{\"email\":" + json::Quote(norm) +
                           ",\"code\":" + json::Quote(code) + "}";
  int status = 0;
  std::string response_body;
  std::string send_err;
  if (!SendJson("POST", "/v1/auth/verify", body, false, &status, &response_body,
                &send_err)) {
    if (err)
      *err = send_err.empty() ? "邮箱验证失败。" : send_err;
    return false;
  }
  return true;
}

bool CloudAccountService::ResendVerification(const std::string& email,
                                             std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }

  // 不需要鉴权：重发只证明「这个邮箱存在且尚未验证」。
  const std::string body = "{\"email\":" + json::Quote(norm) + "}";
  int status = 0;
  std::string response_body;
  std::string send_err;
  if (!SendJson("POST", "/v1/auth/resend", body, false, &status, &response_body,
                &send_err)) {
    if (err)
      *err = send_err.empty() ? "重新发送验证码失败。" : send_err;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// 发起口令重置（批次 AT-2）
// ---------------------------------------------------------------------------
//
//   POST /v1/auth/password-reset/start  { email } → { ok: true, email }
//
// 发出的验证码用途为 **reset**，与注册验证码（purpose=register）隔离。
// 本方法不改状态机（不落凭据、不改登录态）；失败原因写进 err（中文）。
bool CloudAccountService::StartPasswordReset(const std::string& email,
                                             std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }

  const std::string body = "{\"email\":" + json::Quote(norm) + "}";
  int status = 0;
  std::string response_body;
  std::string send_err;
  if (!SendJson("POST", "/v1/auth/password-reset/start", body, false, &status,
                &response_body, &send_err)) {
    if (err)
      *err = send_err.empty() ? "发送重置验证码失败。" : send_err;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// 重置口令（批次 AT-2）
// ---------------------------------------------------------------------------
//
//   POST /v1/auth/password-reset/confirm
//     { email, code, auth_key } → { ok: true, email }
//   · code  ：6 位数字邮箱验证码（用途必须为 reset）
//   · auth_key：用**新口令**按 ADR 0054 派生的 auth_key（服务端只收派生值，
//              永不见明文口令；落库口径与登录一致：
//              verifier = HMAC-SHA256(AUTH_PEPPER, auth_key)）。
bool CloudAccountService::ResetPassword(const std::string& email,
                                        const std::string& code,
                                        const std::string& new_password,
                                        std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }

  const std::string norm = NormalizeEmail(email);
  if (norm.empty()) {
    if (err)
      *err = "请输入邮箱地址。";
    return false;
  }
  if (code.size() != 6 ||
      code.find_first_not_of("0123456789") != std::string::npos) {
    if (err)
      *err = "验证码格式不正确（应为 6 位数字）。";
    return false;
  }
  if (new_password.empty()) {
    if (err)
      *err = "请输入新密码。";
    return false;
  }

  // 与登录同一套派生参数（ADR 0054：PBKDF2-HMAC-SHA256，盐按邮箱确定性派生）。
  std::string password_copy = new_password;
  DerivedKeys keys;
  std::string derive_err;
  const bool derived = DeriveKeys(norm, password_copy, 0, &keys, &derive_err);
  SecureZeroString(&password_copy);
  if (!derived) {
    if (err)
      *err = derive_err.empty() ? "密钥派生失败。" : derive_err;
    return false;
  }

  const std::string body = "{\"email\":" + json::Quote(norm) +
                           ",\"code\":" + json::Quote(code) +
                           ",\"auth_key\":" +
                           json::Quote(EncodeAuthKey(keys.auth_key)) + "}";

  int status = 0;
  std::string response_body;
  std::string send_err;
  const bool ok = SendJson("POST", "/v1/auth/password-reset/confirm", body,
                            false, &status, &response_body, &send_err);
  SecureZeroBytes(&keys.auth_key);
  SecureZeroBytes(&keys.enc_key);
  if (!ok) {
    if (err) {
      std::string reason = send_err;
      // 服务端若尚未部署该端点 ⇒ 404 且没有 JSON 错误体 ⇒ 把兜底文案换成
      // 更准确的中文（「原样转成中文状态行」：服务端真给了消息就原样透传）。
      if (status == 404 &&
          (reason.empty() || reason == "服务器返回错误（HTTP 404）。"))
        reason = "服务端未支持重置口令功能（404），请确认已更新到最新版本。";
      *err = reason.empty() ? "重置口令失败。" : reason;
    }
    return false;
  }
  return true;
}

bool CloudAccountService::Logout(std::string* err) {
  // 批次 AT-1：只在**本次会话的凭据确实落过盘**时才删凭据库记录。
  //   「保存账号」未勾选的登录（persist=false）不落盘 ⇒ 退出登录**不能**
  //   把凭据库里原有的那条记录一起删掉（那可能是另一个账号）。
  bool persisted = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    persisted = creds_persisted_;
  }
  std::string delete_err;
  const bool ok =
      (store_ == nullptr || !persisted) ? true : store_->Delete(&delete_err);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    creds_ = CloudCredentials();
    has_creds_ = false;
    creds_persisted_ = false;
    state_ = CloudAccountState::kLoggedOut;
    info_ = CloudAccountInfo();
  }
  if (!ok) {
    if (err)
      *err = delete_err.empty() ? "退出登录失败。" : delete_err;
    return false;
  }
  return true;
}

bool CloudAccountService::Refresh(std::string* err) {
  std::string refresh_token;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_creds_ || creds_.refresh_token.empty()) {
      state_ = CloudAccountState::kNeedsRelogin;
      if (err)
        *err = "登录已过期，请重新登录。";
      return false;
    }
    refresh_token = creds_.refresh_token;
  }

  const std::string body =
      "{\"refresh_token\":" + json::Quote(refresh_token) + "}";
  int status = 0;
  std::string response_body;
  std::string send_err;
  if (!SendJson("POST", "/v1/auth/refresh", body, false, &status,
                &response_body, &send_err)) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kNeedsRelogin;
    if (err)
      *err = send_err.empty() ? "刷新登录状态失败，请重新登录。" : send_err;
    return false;
  }

  json::JsonDocument doc;
  if (!json::Parse(response_body, &doc) || doc.root < 0) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kNeedsRelogin;
    if (err)
      *err = "服务器返回的数据无法解析。";
    return false;
  }

  CloudCredentials updated;
  bool persisted = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    updated = creds_;
    persisted = creds_persisted_;
  }
  if (!json::MemberString(doc, doc.root, "access_token", &updated.access_token))
    updated.access_token.clear();
  if (!json::MemberString(doc, doc.root, "refresh_token",
                          &updated.refresh_token))
    updated.refresh_token.clear();
  json::MemberInt64(doc, doc.root, "expires_in", &updated.expires_in);
  if (updated.access_token.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kNeedsRelogin;
    if (err)
      *err = "服务器没有返回新的访问令牌，请重新登录。";
    return false;
  }

  // 批次 AT-1：「保存账号」未勾选的会话（persisted=false）**不写**凭据存储 ——
  // 否则一次后台的令牌刷新就把用户明确不想保存的凭据落盘了。
  std::string save_err;
  if (persisted && store_ != nullptr && !store_->Save(updated, &save_err)) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CloudAccountState::kNeedsRelogin;
    if (err)
      *err = save_err.empty() ? "保存云端凭据失败。" : save_err;
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    creds_ = updated;
    has_creds_ = true;
    state_ = CloudAccountState::kLoggedIn;
  }
  return true;
}

bool CloudAccountService::RestoreFromStore(std::string* err) {
  if (store_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }
  CloudCredentials creds;
  std::string load_err;
  if (!store_->Load(&creds, &load_err)) {
    // 读取失败 ⇒ 回到未登录态，不崩溃（宪法 P2）。
    std::lock_guard<std::mutex> lock(mutex_);
    creds_ = CloudCredentials();
    has_creds_ = false;
    state_ = CloudAccountState::kLoggedOut;
    if (err)
      *err = load_err;
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    creds_ = creds;
    has_creds_ = true;
    // 从凭据库恢复 ⇒ 这条记录确实在盘上。
    creds_persisted_ = true;
    state_ = CloudAccountState::kLoggedIn;
    info_.email = creds.email;
    info_.account_id = creds.account_id;
    info_.device_id = creds.device_id;
    info_.email_verified = creds.email_verified;
  }
  return true;
}

// ---------------------------------------------------------------------------
// 设备管理
// ---------------------------------------------------------------------------

bool CloudAccountService::EnsureFreshToken(std::string* err) {
  std::string refresh_err;
  if (Refresh(&refresh_err))
    return true;
  if (err)
    *err = refresh_err;
  return false;
}

bool CloudAccountService::ListDevices(std::vector<CloudDeviceInfo>* out,
                                      std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：设备列表输出为空。";
    return false;
  }
  out->clear();

  int status = 0;
  std::string body;
  std::string send_err;
  if (!SendJson("GET", "/v1/devices", "", true, &status, &body, &send_err)) {
    if (status == 401) {
      std::string refresh_err;
      if (EnsureFreshToken(&refresh_err) &&
          SendJson("GET", "/v1/devices", "", true, &status, &body, &send_err))
        goto parsed;
    }
    if (err)
      *err = send_err.empty() ? "获取设备列表失败。" : send_err;
    return false;
  }

parsed:
  json::JsonDocument doc;
  if (!json::Parse(body, &doc) || doc.root < 0) {
    if (err)
      *err = "服务器返回的数据无法解析。";
    return false;
  }
  const json::JsonNode* root = doc.Node(doc.root);
  if (root == nullptr || root->type != json::JsonType::kArray) {
    if (err)
      *err = "服务器返回的设备列表格式不正确。";
    return false;
  }
  for (size_t i = 0; i < root->children.size(); ++i) {
    const int index = root->children[i];
    CloudDeviceInfo device;
    json::MemberString(doc, index, "id", &device.id);
    json::MemberString(doc, index, "device_id", &device.device_id);
    json::MemberString(doc, index, "name", &device.name);
    json::MemberInt64(doc, index, "last_seen", &device.last_seen);
    json::MemberInt64(doc, index, "created", &device.created);
    json::MemberBool(doc, index, "revoked", &device.revoked);
    json::MemberBool(doc, index, "current", &device.current);
    out->push_back(device);
  }
  return true;
}

bool CloudAccountService::RevokeDevice(const std::string& device_id,
                                       std::string* err) {
  if (device_id.empty()) {
    if (err)
      *err = "设备标识不能为空。";
    return false;
  }
  // 路径参数用 device_id（客户端稳定标识）；服务端同时接受设备表的 id。
  const std::string path = "/v1/devices/" + UrlEncodePathSegment(device_id);

  int status = 0;
  std::string body;
  std::string send_err;
  if (!SendJson("DELETE", path, "", true, &status, &body, &send_err)) {
    if (status == 401) {
      std::string refresh_err;
      if (EnsureFreshToken(&refresh_err) &&
          SendJson("DELETE", path, "", true, &status, &body, &send_err))
        return true;
    }
    if (err)
      *err = send_err.empty() ? "撤销设备失败。" : send_err;
    return false;
  }
  return true;
}

bool CloudAccountService::DeleteCloudData(std::string* err) {
  int status = 0;
  std::string body;
  std::string send_err;
  if (!SendJson("DELETE", "/v1/account/data", "", true, &status, &body,
                &send_err)) {
    if (status == 401) {
      std::string refresh_err;
      if (EnsureFreshToken(&refresh_err) &&
          SendJson("DELETE", "/v1/account/data", "", true, &status, &body,
                   &send_err))
        return true;
    }
    if (err)
      *err = send_err.empty() ? "清空云端数据失败。" : send_err;
    return false;
  }
  return true;
}

}  // namespace cloud
}  // namespace fluxing
