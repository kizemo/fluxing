#pragma once
//
// spec 100 阶段 D · T303 凭据存放（Windows Credential Manager / DPAPI）
//
// 目标（tasks.md T303）：
//   · 令牌与派生密钥**不落明文文件**，交给 Windows Credential Manager；
//   · **不在 TSF 回调里调用**（所有调用点都在后台线程，见
//   CloudAccountService）； · 读取失败 ⇒ 回到「未登录」态，而不是崩溃。
//
// 落地方式：Credential Manager 的 Generic Credential（目标名
// Fluxing:CloudAccount）， 持久化级别 CRED_PERSIST_LOCAL_MACHINE —— 该 blob 由
// DPAPI 按机器/用户保护， 别的进程读不到明文。Blob
// 内容是本模块自定义的二进制格式（见 EncodeCredentials）。
//
// 为什么自己编解码（而不是直接存 JSON）：JSON 需要转义规则，且我们要对
// 「字段缺失 / 顺序错乱 / 长度不一致」给出确定性的失败原因；同时纯函数化后
// 编解码可在单测里往返验证（R7 生产与单测同源）。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {

// 一份完整的云端账号凭据。
struct CloudCredentials {
  std::string email;
  std::string account_id;
  std::string device_id;
  std::string access_token;
  std::string refresh_token;
  // 派生出来的 auth_key / enc_key 的 base64 表示（auth_key 是线上格式）。
  std::string auth_key_b64;
  std::string enc_key_b64;
  // 服务端返回的随机 salt：**只做诊断记录，不参与派生**（理由见
  // CloudCrypto.h）。
  std::string server_salt_b64;
  bool email_verified = false;
  int64_t expires_in = 0;
};

// ---------------------------------------------------------------------------
// 编解码（纯函数，可单测往返）
// ---------------------------------------------------------------------------

// 格式：magic "FLCRED1"(8B) + 8 个 [u32 长度 + UTF-8 字节] + u8 email_verified
//       + i64 expires_in（小端）。字段顺序固定，尾部不允许有多余字节。
bool EncodeCredentials(const CloudCredentials& creds,
                       std::vector<uint8_t>* out);

// 解码；任何不符 ⇒ 失败并写中文原因（不崩溃、不半读）。
bool DecodeCredentials(const std::vector<uint8_t>& blob,
                       CloudCredentials* out,
                       std::string* err);

// ---------------------------------------------------------------------------
// 存放
// ---------------------------------------------------------------------------

class CloudCredentialStore {
 public:
  explicit CloudCredentialStore(
      std::string target_name = "Fluxing:CloudAccount");
  ~CloudCredentialStore();

  CloudCredentialStore(const CloudCredentialStore&) = delete;
  CloudCredentialStore& operator=(const CloudCredentialStore&) = delete;

  // 写入（覆盖同名的旧凭据）。
  bool Save(const CloudCredentials& creds, std::string* err);

  // 读取。没有记录 ⇒ false + err「本机没有保存云端账号。」；
  // 读取/解码失败 ⇒ false + err 中文原因（调用方据此回到未登录态）。
  bool Load(CloudCredentials* out, std::string* err);

  // 删除。没有记录也算成功（幂等）。
  bool Delete(std::string* err);

  const std::string& target_name() const { return target_name_; }

 private:
  std::string target_name_;
};

// 显示用脱敏：只保留首 2 字符与末 2 字符，中间用 * 代替；空串返回空。
std::string RedactSecret(const std::string& secret);

}  // namespace cloud
}  // namespace fluxing
