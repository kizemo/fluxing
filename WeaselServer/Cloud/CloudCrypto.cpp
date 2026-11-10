// CloudCrypto.cpp — SHA-256 / HMAC / PBKDF2 / HKDF 实现 + 密钥派生。
//
// 为什么自己实现 SHA-256 而不用 CNG 的 BCrypt：
//   1. PBKDF2 需要 600000 次迭代。CNG 的 PBKDF2 提供程序在默认配置下
//      使用 **SHA-1** 作 PRF（要换成 SHA-256 得另配 hash 算法属性，
//      不同 SDK 版本行为不一致）⇒ 与服务端 WebCrypto 的
//      PBKDF2-HMAC-SHA256 对不齐。
//   2. 逐次调用 BCrypt 的 handle 创建 / 销毁在 60 万次量级下开销显著，
//      自实现是一次纯计算。
//   3. 自实现可被**标准测试向量**校验（FIPS 180-4 / RFC 4231 / RFC 6070 /
//      RFC 5869），不存在「实现和实现互相证明」的假绿。
//   AES-GCM 仍然走系统 CNG（include/FluxingCloudEnvelope.h 的
//   crypto::AesGcmSeal / AesGcmOpen），不在这里重复实现。
//
// 口令 / 密钥缓冲用后清零；本文件不写任何日志。
//
#include "CloudCrypto.h"

#include <cstring>

#include "../../include/FluxingCloudEnvelope.h"

namespace fluxing {
namespace cloud {
namespace {

// ---------------------------------------------------------------------------
// SHA-256（FIPS 180-4）
// ---------------------------------------------------------------------------

const uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

inline uint32_t RotR(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

void Sha256Compress(uint32_t state[8], const uint8_t block[64]) {
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
           (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const uint32_t s0 =
        RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 =
        RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (int i = 0; i < 64; ++i) {
    const uint32_t s1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
    const uint32_t ch = (e & f) ^ (~e & g);
    const uint32_t temp1 = h + s1 + ch + kSha256K[i] + w[i];
    const uint32_t s0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

// 就地把一段缓冲覆盖为 0（编译器不会把它优化掉）。
void WipeBytes(uint8_t* data, size_t len) {
  if (data == nullptr || len == 0)
    return;
  ::SecureZeroMemory(data, len);
}

}  // namespace

// ---------------------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------------------

bool Sha256(const uint8_t* data, size_t len, std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();
  if (len > 0 && data == nullptr)
    return false;

  uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                       0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};

  const size_t full = len / 64;
  for (size_t i = 0; i < full; ++i)
    Sha256Compress(state, data + i * 64);

  // 尾块 + 0x80 + 补零 + 64 位大端比特长度。
  uint8_t tail[128];
  std::memset(tail, 0, sizeof(tail));
  const size_t rest = len - full * 64;
  if (rest > 0)
    std::memcpy(tail, data + full * 64, rest);
  tail[rest] = 0x80;
  const size_t tail_len = (rest < 56) ? 64 : 128;
  const uint64_t bits = static_cast<uint64_t>(len) * 8u;
  for (int i = 0; i < 8; ++i)
    tail[tail_len - 1 - static_cast<size_t>(i)] =
        static_cast<uint8_t>((bits >> (8 * i)) & 0xFFu);
  Sha256Compress(state, tail);
  if (tail_len == 128)
    Sha256Compress(state, tail + 64);

  out->resize(32);
  for (int i = 0; i < 8; ++i) {
    (*out)[static_cast<size_t>(i * 4)] =
        static_cast<uint8_t>((state[i] >> 24) & 0xFFu);
    (*out)[static_cast<size_t>(i * 4 + 1)] =
        static_cast<uint8_t>((state[i] >> 16) & 0xFFu);
    (*out)[static_cast<size_t>(i * 4 + 2)] =
        static_cast<uint8_t>((state[i] >> 8) & 0xFFu);
    (*out)[static_cast<size_t>(i * 4 + 3)] =
        static_cast<uint8_t>(state[i] & 0xFFu);
  }
  return true;
}

// ---------------------------------------------------------------------------
// HMAC-SHA256（RFC 2104）
// ---------------------------------------------------------------------------

bool HmacSha256(const std::vector<uint8_t>& key,
                const uint8_t* data,
                size_t len,
                std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();
  constexpr size_t kBlock = 64;

  uint8_t normalized[64];
  std::memset(normalized, 0, sizeof(normalized));
  if (key.size() > kBlock) {
    std::vector<uint8_t> digest;
    if (!Sha256(key.data(), key.size(), &digest))
      return false;
    std::memcpy(normalized, digest.data(), digest.size());
    WipeBytes(digest.data(), digest.size());
  } else if (!key.empty()) {
    std::memcpy(normalized, key.data(), key.size());
  }

  uint8_t ipad[64];
  uint8_t opad[64];
  for (size_t i = 0; i < kBlock; ++i) {
    ipad[i] = static_cast<uint8_t>(normalized[i] ^ 0x36u);
    opad[i] = static_cast<uint8_t>(normalized[i] ^ 0x5Cu);
  }

  // 内层：SHA256(ipad || msg)
  std::vector<uint8_t> inner_input;
  inner_input.reserve(kBlock + len);
  inner_input.insert(inner_input.end(), ipad, ipad + kBlock);
  if (len > 0 && data != nullptr)
    inner_input.insert(inner_input.end(), data, data + len);
  std::vector<uint8_t> inner;
  if (!Sha256(inner_input.data(), inner_input.size(), &inner))
    return false;

  // 外层：SHA256(opad || inner)
  uint8_t outer_input[64 + 32];
  std::memcpy(outer_input, opad, kBlock);
  std::memcpy(outer_input + kBlock, inner.data(), inner.size());
  std::vector<uint8_t> digest;
  const bool ok = Sha256(outer_input, sizeof(outer_input), &digest);

  WipeBytes(normalized, sizeof(normalized));
  WipeBytes(ipad, sizeof(ipad));
  WipeBytes(opad, sizeof(opad));
  WipeBytes(outer_input, sizeof(outer_input));
  if (!inner.empty())
    WipeBytes(inner.data(), inner.size());
  if (!inner_input.empty())
    WipeBytes(inner_input.data(), inner_input.size());
  if (!ok)
    return false;

  *out = std::move(digest);
  return true;
}

// ---------------------------------------------------------------------------
// PBKDF2-HMAC-SHA256（RFC 8018）
// ---------------------------------------------------------------------------

bool Pbkdf2HmacSha256(const std::string& password,
                      const std::vector<uint8_t>& salt,
                      uint32_t iterations,
                      std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();
  if (iterations == 0)
    return false;

  std::vector<uint8_t> pwd(password.begin(), password.end());
  std::vector<uint8_t> u;
  std::vector<uint8_t> t;
  std::vector<uint8_t> block;

  // DK = T1（32 字节输出只需要一块）
  for (uint32_t i = 1; i <= 1; ++i) {
    block.clear();
    block.insert(block.end(), salt.begin(), salt.end());
    block.push_back(static_cast<uint8_t>((i >> 24) & 0xFFu));
    block.push_back(static_cast<uint8_t>((i >> 16) & 0xFFu));
    block.push_back(static_cast<uint8_t>((i >> 8) & 0xFFu));
    block.push_back(static_cast<uint8_t>(i & 0xFFu));

    if (!HmacSha256(pwd, block.data(), block.size(), &u))
      goto fail;
    t = u;
    for (uint32_t iter = 1; iter < iterations; ++iter) {
      if (!HmacSha256(pwd, u.data(), u.size(), &u))
        goto fail;
      for (size_t k = 0; k < t.size() && k < u.size(); ++k)
        t[k] = static_cast<uint8_t>(t[k] ^ u[k]);
    }
    out->insert(out->end(), t.begin(), t.end());
  }

  WipeBytes(pwd.data(), pwd.size());
  if (!u.empty())
    WipeBytes(u.data(), u.size());
  if (!t.empty())
    WipeBytes(t.data(), t.size());
  if (!block.empty())
    WipeBytes(block.data(), block.size());
  return true;

fail:
  if (!pwd.empty())
    WipeBytes(pwd.data(), pwd.size());
  if (!u.empty())
    WipeBytes(u.data(), u.size());
  if (!t.empty())
    WipeBytes(t.data(), t.size());
  if (!block.empty())
    WipeBytes(block.data(), block.size());
  out->clear();
  return false;
}

// ---------------------------------------------------------------------------
// HKDF-SHA256（RFC 5869）
// ---------------------------------------------------------------------------

bool HkdfSha256(const std::vector<uint8_t>& ikm,
                const std::string& info,
                std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();

  // 盐固定为 32 字节全 0（RFC 5869 允许「未提供盐」时用 HashLen 个 0）。
  std::vector<uint8_t> salt(32, 0);
  std::vector<uint8_t> prk;
  if (!HmacSha256(salt, ikm.data(), ikm.size(), &prk)) {
    WipeBytes(salt.data(), salt.size());
    return false;
  }
  WipeBytes(salt.data(), salt.size());

  // OKM = T1 = HMAC(PRK, info || 0x01)（输出 32 字节，只需一块）
  std::vector<uint8_t> message(info.begin(), info.end());
  message.push_back(0x01);
  std::vector<uint8_t> okm;
  const bool ok = HmacSha256(prk, message.data(), message.size(), &okm);
  if (!prk.empty())
    WipeBytes(prk.data(), prk.size());
  if (!message.empty())
    WipeBytes(message.data(), message.size());
  if (!ok)
    return false;

  *out = std::move(okm);
  return true;
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

std::string NormalizeEmail(const std::string& email) {
  size_t begin = 0;
  size_t end = email.size();
  while (begin < end && (email[begin] == ' ' || email[begin] == '\t' ||
                         email[begin] == '\r' || email[begin] == '\n'))
    ++begin;
  while (end > begin && (email[end - 1] == ' ' || email[end - 1] == '\t' ||
                         email[end - 1] == '\r' || email[end - 1] == '\n'))
    --end;
  std::string out = email.substr(begin, end - begin);
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] >= 'A' && out[i] <= 'Z')
      out[i] = static_cast<char>(out[i] - 'A' + 'a');
  }
  return out;
}

std::vector<uint8_t> DeriveSalt(const std::string& email) {
  const std::string normalized = NormalizeEmail(email);
  std::string material(kSaltPrefix);
  material += normalized;
  std::vector<uint8_t> digest;
  Sha256(reinterpret_cast<const uint8_t*>(material.data()), material.size(),
         &digest);
  return digest;
}

bool DeriveKeys(const std::string& email,
                const std::string& password,
                uint32_t iterations,
                DerivedKeys* out,
                std::string* err) {
  if (out == nullptr)
    return false;
  out->auth_key.clear();
  out->enc_key.clear();
  if (password.empty()) {
    if (err)
      *err = "口令不能为空";
    return false;
  }
  if (iterations == 0)
    iterations = kPbkdf2Iterations;

  const std::vector<uint8_t> salt = DeriveSalt(email);
  std::vector<uint8_t> master;
  if (!Pbkdf2HmacSha256(password, salt, iterations, &master)) {
    if (err)
      *err = "密钥派生失败";
    return false;
  }
  if (master.size() != kDerivedKeyBytes) {
    WipeBytes(master.data(), master.size());
    if (err)
      *err = "密钥派生结果长度异常";
    return false;
  }

  std::vector<uint8_t> auth_key;
  std::vector<uint8_t> enc_key;
  const bool auth_ok = HkdfSha256(master, kAuthKeyInfo, &auth_key);
  const bool enc_ok = HkdfSha256(master, kEncKeyInfo, &enc_key);
  WipeBytes(master.data(), master.size());
  if (!auth_ok || !enc_ok) {
    if (!auth_key.empty())
      WipeBytes(auth_key.data(), auth_key.size());
    if (!enc_key.empty())
      WipeBytes(enc_key.data(), enc_key.size());
    if (err)
      *err = "密钥派生失败";
    return false;
  }

  out->auth_key = std::move(auth_key);
  out->enc_key = std::move(enc_key);
  return true;
}

std::string EncodeAuthKey(const std::vector<uint8_t>& auth_key) {
  return Base64Encode(auth_key);
}

bool GenerateDeviceId(std::string* out, std::string* err) {
  if (out == nullptr)
    return false;
  out->clear();
  std::vector<uint8_t> raw(8, 0);
  const NTSTATUS st =
      ::BCryptGenRandom(nullptr, raw.data(), static_cast<ULONG>(raw.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (!NT_SUCCESS(st)) {
    if (err)
      *err = "生成设备标识失败";
    return false;
  }
  static const char kHex[] = "0123456789abcdef";
  std::string text;
  text.reserve(16);
  for (size_t i = 0; i < 8; ++i) {
    text.push_back(kHex[(raw[i] >> 4) & 0x0F]);
    text.push_back(kHex[raw[i] & 0x0F]);
  }
  out->assign(text);
  return true;
}

void SecureZeroString(std::string* s) {
  if (s == nullptr || s->empty())
    return;
  ::SecureZeroMemory(&(*s)[0], s->size());
  s->clear();
}

void SecureZeroBytes(std::vector<uint8_t>* v) {
  if (v == nullptr)
    return;
  crypto::SecureZero(*v);
}

}  // namespace cloud
}  // namespace fluxing
