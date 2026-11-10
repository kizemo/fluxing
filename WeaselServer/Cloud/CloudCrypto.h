#pragma once
//
// spec 100 阶段 D · T301 密钥派生（CNG / 自实现 SHA-256）与信封加密接线
//
// 派生链（plan.md §3.1）：
//   salt     = SHA256("fluxing-enc-v1|" + lower(email))
//   master   = PBKDF2-HMAC-SHA256(password, salt, 600000, 32)
//   auth_key = HKDF-SHA256(master, info="fluxing-auth-v1", 32)
//   enc_key  = HKDF-SHA256(master, info="fluxing-enc-v1", 32)
//
// **为什么 salt 是「按邮箱确定」的，而不是服务端返回的随机值**（ADR 0054）：
//   服务端 `handleRegister` 生成的 salt 是随机 16 字节，但 grep 全量确认：
//   该 salt 只出现在 users 表的 INSERT / SELECT 与 login/register 响应里，
//   **从不参与任何校验**（服务端比对的是 HMAC(pepper, auth_key)）。
//   而登录请求体里只能带 email / auth_key / device，客户端在登录**之前**
//   拿不到服务端的 salt ⇒ 若客户端用服务端 salt 派生，新设备将永远算不出
//   同一个 auth_key ⇒ 无法登录。
//   ⇒ 客户端按邮箱确定性派生 salt（与 plan.md §3.1 原式一致），
//     服务端返回的 salt 只做诊断记录，不参与派生。代价是 salt
//     不再「每账号随机」 （预计算抗性略降），换来的是「任意设备可登录」。取舍见
//     ADR 0054。
//
// 安全约束（tasks.md T301）：
//   · 口令 / 密钥缓冲用后清零（SecureZeroString / SecureZeroBytes）；
//   · 全程不落日志（本模块不写任何日志，也不接受日志回调）。
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

// ---------------------------------------------------------------------------
// KDF 参数（plan.md §3.1）
// ---------------------------------------------------------------------------

// PBKDF2 迭代次数。服务端 KDF_ITERS
// 默认值同（cloud/fluxing-sync/src/auth.ts）。
constexpr uint32_t kPbkdf2Iterations = 600000;

// 派生密钥长度（AES-256）。
constexpr size_t kDerivedKeyBytes = 32;

// 盐的前缀（salt = SHA256(kSaltPrefix + lower(email))）。
constexpr char kSaltPrefix[] = "fluxing-enc-v1|";

// HKDF 的 info 标签：把 master 拆成两把用途不同的密钥。
constexpr char kAuthKeyInfo[] = "fluxing-auth-v1";
constexpr char kEncKeyInfo[] = "fluxing-enc-v1";

struct DerivedKeys {
  std::vector<uint8_t> auth_key;  // 送服务端做身份校验
  std::vector<uint8_t> enc_key;   // 本地 AES-256-GCM 信封加密
};

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

// 规范化邮箱：去首尾空白 + ASCII 小写（与服务端 normalizeEmail 对齐）。
std::string NormalizeEmail(const std::string& email);

// 按邮箱确定性派生盐（32 字节）。
std::vector<uint8_t> DeriveSalt(const std::string& email);

// 派生 auth_key / enc_key。
//   · iterations 传 0 时用 kPbkdf2Iterations；
//   · 失败时 out 被清空并写中文原因；
//   · 本函数**不**清零调用方的 password（const 引用做不到）⇒ 调用方必须
//     在用完后自己调 SecureZeroString(&password_copy)。
bool DeriveKeys(const std::string& email,
                const std::string& password,
                uint32_t iterations,
                DerivedKeys* out,
                std::string* err);

// auth_key 的线上表示：base64（服务端按字符串收）。
std::string EncodeAuthKey(const std::vector<uint8_t>& auth_key);

// 生成本机设备标识：16 位小写十六进制（8 字节随机）。
bool GenerateDeviceId(std::string* out, std::string* err);

// 敏感缓冲清零。
void SecureZeroString(std::string* s);
void SecureZeroBytes(std::vector<uint8_t>* v);

// ---------------------------------------------------------------------------
// 低层原语（暴露出来是为了让单测用**标准测试向量**校验，而不是自证自洽）
// ---------------------------------------------------------------------------

// SHA-256（FIPS 180-4）。
bool Sha256(const uint8_t* data, size_t len, std::vector<uint8_t>* out);

// HMAC-SHA256（RFC 2104）。
bool HmacSha256(const std::vector<uint8_t>& key,
                const uint8_t* data,
                size_t len,
                std::vector<uint8_t>* out);

// PBKDF2-HMAC-SHA256（RFC 8018）。
bool Pbkdf2HmacSha256(const std::string& password,
                      const std::vector<uint8_t>& salt,
                      uint32_t iterations,
                      std::vector<uint8_t>* out);

// HKDF-SHA256（RFC 5869）。本实现固定 32 字节零盐 + 单块输出（32 字节）。
bool HkdfSha256(const std::vector<uint8_t>& ikm,
                const std::string& info,
                std::vector<uint8_t>* out);

}  // namespace cloud
}  // namespace fluxing
