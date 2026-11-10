// spec 100 阶段 D · T303 凭据存放实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 里逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudCredentialStore.h"

#include <windows.h>

#include <wincred.h>

#include <cstring>

namespace fluxing {
namespace cloud {

namespace {

constexpr char kMagic[] = "FLCRED1";
constexpr size_t kMagicBytes = 8;
constexpr size_t kStringFieldCount = 8;

void PutU32(std::vector<uint8_t>* out, uint32_t v) {
  out->push_back(static_cast<uint8_t>(v & 0xFF));
  out->push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  out->push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  out->push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void PutU64(std::vector<uint8_t>* out, uint64_t v) {
  for (int i = 0; i < 8; ++i)
    out->push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

void PutString(std::vector<uint8_t>* out, const std::string& s) {
  PutU32(out, static_cast<uint32_t>(s.size()));
  out->insert(out->end(), s.begin(), s.end());
}

bool GetU32(const std::vector<uint8_t>& in, size_t* pos, uint32_t* out) {
  if (*pos + 4 > in.size())
    return false;
  *out = static_cast<uint32_t>(in[*pos]) |
         (static_cast<uint32_t>(in[*pos + 1]) << 8) |
         (static_cast<uint32_t>(in[*pos + 2]) << 16) |
         (static_cast<uint32_t>(in[*pos + 3]) << 24);
  *pos += 4;
  return true;
}

bool GetU64(const std::vector<uint8_t>& in, size_t* pos, uint64_t* out) {
  if (*pos + 8 > in.size())
    return false;
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i)
    v |= static_cast<uint64_t>(in[*pos + static_cast<size_t>(i)]) << (8 * i);
  *pos += 8;
  *out = v;
  return true;
}

bool GetString(const std::vector<uint8_t>& in, size_t* pos, std::string* out) {
  uint32_t len = 0;
  if (!GetU32(in, pos, &len))
    return false;
  if (*pos + len > in.size())
    return false;
  out->assign(reinterpret_cast<const char*>(&in[*pos]), len);
  *pos += len;
  return true;
}

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

std::string LastErrorText(const char* what) {
  const DWORD code = ::GetLastError();
  char buf[64];
  std::snprintf(buf, sizeof(buf), "（错误码 %lu）",
                static_cast<unsigned long>(code));
  return std::string(what) + buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// 编解码
// ---------------------------------------------------------------------------

bool EncodeCredentials(const CloudCredentials& creds,
                       std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();
  out->reserve(64 + creds.email.size() + creds.access_token.size() +
               creds.refresh_token.size() + creds.enc_key_b64.size());

  out->insert(out->end(), kMagic, kMagic + kMagicBytes);
  PutString(out, creds.email);
  PutString(out, creds.account_id);
  PutString(out, creds.device_id);
  PutString(out, creds.access_token);
  PutString(out, creds.refresh_token);
  PutString(out, creds.auth_key_b64);
  PutString(out, creds.enc_key_b64);
  PutString(out, creds.server_salt_b64);
  out->push_back(creds.email_verified ? 1 : 0);
  PutU64(out, static_cast<uint64_t>(creds.expires_in));
  return true;
}

bool DecodeCredentials(const std::vector<uint8_t>& blob,
                       CloudCredentials* out,
                       std::string* err) {
  auto fail = [&](const char* why) {
    if (err)
      *err = why;
    return false;
  };
  if (out == nullptr)
    return false;
  if (blob.size() < kMagicBytes + 1)
    return fail("凭据数据为空或已损坏。");
  if (std::memcmp(&blob[0], kMagic, kMagicBytes) != 0)
    return fail("凭据数据格式不匹配（可能来自其他版本）。");

  size_t pos = kMagicBytes;
  CloudCredentials c;
  if (!GetString(blob, &pos, &c.email))
    return fail("凭据数据已损坏（邮箱字段）。");
  if (!GetString(blob, &pos, &c.account_id))
    return fail("凭据数据已损坏（账号 ID 字段）。");
  if (!GetString(blob, &pos, &c.device_id))
    return fail("凭据数据已损坏（设备标识字段）。");
  if (!GetString(blob, &pos, &c.access_token))
    return fail("凭据数据已损坏（访问令牌字段）。");
  if (!GetString(blob, &pos, &c.refresh_token))
    return fail("凭据数据已损坏（刷新令牌字段）。");
  if (!GetString(blob, &pos, &c.auth_key_b64))
    return fail("凭据数据已损坏（认证密钥字段）。");
  if (!GetString(blob, &pos, &c.enc_key_b64))
    return fail("凭据数据已损坏（加密密钥字段）。");
  if (!GetString(blob, &pos, &c.server_salt_b64))
    return fail("凭据数据已损坏（盐字段）。");
  if (pos >= blob.size())
    return fail("凭据数据已损坏（验证状态字段）。");
  c.email_verified = blob[pos++] != 0;
  uint64_t exp = 0;
  if (!GetU64(blob, &pos, &exp))
    return fail("凭据数据已损坏（有效期字段）。");
  c.expires_in = static_cast<int64_t>(exp);
  if (pos != blob.size())
    return fail("凭据数据尾部有多余内容。");
  if (c.email.empty())
    return fail("凭据数据缺少邮箱。");

  *out = c;
  return true;
}

// ---------------------------------------------------------------------------
// 存放
// ---------------------------------------------------------------------------

CloudCredentialStore::CloudCredentialStore(std::string target_name)
    : target_name_(std::move(target_name)) {}

CloudCredentialStore::~CloudCredentialStore() = default;

bool CloudCredentialStore::Save(const CloudCredentials& creds,
                                std::string* err) {
  std::vector<uint8_t> blob;
  if (!EncodeCredentials(creds, &blob)) {
    if (err)
      *err = "凭据编码失败。";
    return false;
  }
  if (blob.size() > 5 * 512) {
    if (err)
      *err = "凭据内容过大，无法保存。";
    return false;
  }

  const std::wstring target = ToWide(target_name_);
  CREDENTIALW cred;
  std::memset(&cred, 0, sizeof(cred));
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = const_cast<LPWSTR>(target.c_str());
  cred.CredentialBlobSize = static_cast<DWORD>(blob.size());
  cred.CredentialBlob =
      reinterpret_cast<LPBYTE>(const_cast<uint8_t*>(blob.data()));
  // LocalMachine 持久化：由 DPAPI 按「本机 +
  // 当前用户」保护，别的进程读不到明文。
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
  cred.UserName = const_cast<LPWSTR>(L"Fluxing");

  if (!::CredWriteW(&cred, 0)) {
    if (err)
      *err = LastErrorText("保存云端凭据失败。");
    return false;
  }
  return true;
}

bool CloudCredentialStore::Load(CloudCredentials* out, std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "读取云端凭据失败（内部错误）。";
    return false;
  }
  const std::wstring target = ToWide(target_name_);
  PCREDENTIALW cred = nullptr;
  if (!::CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
    const DWORD code = ::GetLastError();
    if (err) {
      if (code == ERROR_NOT_FOUND)
        *err = "本机没有保存云端账号。";
      else
        *err = LastErrorText("读取云端凭据失败。");
    }
    return false;
  }

  std::vector<uint8_t> blob(cred->CredentialBlobSize);
  if (cred->CredentialBlobSize > 0)
    std::memcpy(blob.data(), cred->CredentialBlob, cred->CredentialBlobSize);
  ::CredFree(cred);

  std::string decode_err;
  if (!DecodeCredentials(blob, out, &decode_err)) {
    if (err)
      *err = decode_err.empty() ? std::string("云端凭据已损坏。") : decode_err;
    return false;
  }
  return true;
}

bool CloudCredentialStore::Delete(std::string* err) {
  const std::wstring target = ToWide(target_name_);
  if (::CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0))
    return true;
  const DWORD code = ::GetLastError();
  if (code == ERROR_NOT_FOUND)
    return true;  // 幂等
  if (err)
    *err = LastErrorText("删除云端凭据失败。");
  return false;
}

std::string RedactSecret(const std::string& secret) {
  if (secret.empty())
    return std::string();
  if (secret.size() <= 8)
    return std::string(secret.size(), '*');
  return secret.substr(0, 2) + std::string(secret.size() - 4, '*') +
         secret.substr(secret.size() - 2);
}

}  // namespace cloud
}  // namespace fluxing
