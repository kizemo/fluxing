#pragma once
//
// spec 100 阶段 B（T201–T204）：云端同步的**信封编解码 / HLC 定序 / 条目合并**
// 单一真相源（single source of truth）。
//
// 为什么 header-only + `inline`：
//   宪法 R7「生产与单测同源」。本文件被 WeaselServer 的同步引擎与
//   test/TestFluxingCloud 同时 include；若把实现抄进测试，改生产代码时测试
//   不会变红（spec 092 记的「假绿」死法）。T204 的 Done 判据就要求 grep 证明
//   测试文件里没有第二份合并 / HLC 实现。
//
// 依赖纪律（宪法 P3「不新增第三方依赖」）：
//   只用 C++17 标准库 + 系统自带 CNG（bcrypt.lib）做 AES-256-GCM 的
//   认证加密。不引 OpenSSL / libsodium。WinHTTP 那一层在同步引擎里，本文件
//   不碰网络、不碰磁盘 ⇒ 纯逻辑可独立单测。
//
// 线格式（信封）— 逐行 `<字段名>\t<值>`，首行是版本号 magic：
//
//   v1
//   item_key<TAB><转义串>
//   type<TAB>config|phrases|userdb
//   hlc_ms<TAB><十进制 u64>
//   hlc_counter<TAB><十进制 u16>
//   device<TAB><转义串>
//   nonce<TAB><base64>
//   cipher<TAB><base64>
//
// 为什么用「文本 + 逐行定名」而不是二进制长度前缀：
//   出问题时能直接 dump 出来看（排障成本低），且「畸形输入」可以被单测
//   逐字段构造（丢行 / 重名 / 改类型 / 坏 base64 / 错 nonce 长度）。
//   解析器对**行序与行名都严格**：加字段必须升 magic 版本，旧解析器遇到
//   未知行**显式失败**而不是静默忽略（避免「新写旧的读一半丢字段」）。
//
// AAD 绑定（plan.md §4.1）：`aad = item_key|type|hlc_ms|hlc_counter|device`
//   ⇒ 条目被换位、被改版本（降级攻击）、被改文件名，解密端都会拒绝。
//
// 重要前提：`item_key` 与 `device` **不得包含 '|'**。item_key 是 Rime 用户
//   目录内的相对路径（Windows 文件名本来就禁止 '|'），device 是十六进制
//   设备标识（生产代码生成时只允许 [0-9a-f]）⇒ 前提成立。
//
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace fluxing {
namespace cloud {

// ===========================================================================
// 常量
// ===========================================================================

// AES-GCM 的 nonce 长度。plan.md §4.1 定为 12 B（GCM 推荐的 96-bit IV）。
constexpr size_t kNonceBytes = 12;

// GCM 认证标签长度，16 B 是 GCM 的最大强度（也是 CNG 的默认值）。
constexpr size_t kTagBytes = 16;

// 密钥长度：AES-256。
constexpr size_t kKeyBytes = 32;

// HLC 计数器的位宽上限（plan.md §4.2 定为 uint16）。
constexpr uint16_t kHlcCounterMax = 0xFFFF;

// 单个信封 / 单份条目表的内容上限。
//
// 来源说明：plan.md **没有**规定具体字节数（只规定了「推请求只带变化条目」
// 的量级纪律，见 §10 与 §7 的配额表）。8 MiB 是本实现的防御性上限，取值依据：
//   - 最大的真实载荷是 `config` 类的整份用户配置（本机实测在数百 KB 量级）；
//   - 8 MiB 给足 20 倍余量，同时仍能挡住「恶意大载荷撑爆内存」。
// 超限 ⇒ `EnvelopeError::kTooLong`，由调用方决定是拒绝还是分片。
constexpr size_t kMaxPayloadBytes = 8u * 1024u * 1024u;

// ===========================================================================
// 条目类型
// ===========================================================================

enum class ItemType {
  kConfig = 0,   // 整份配置文件（按 HLC 整份取大者）
  kPhrases = 1,  // 用户短语，条目化（按条目键合并）
  kUserDb = 2,   // 用户词典快照，条目化（按条目键合并）
};

inline const char* ItemTypeName(ItemType t) {
  switch (t) {
    case ItemType::kConfig:
      return "config";
    case ItemType::kPhrases:
      return "phrases";
    case ItemType::kUserDb:
      return "userdb";
  }
  return "unknown";
}

inline bool ParseItemType(const std::string& s, ItemType* out) {
  if (out == nullptr)
    return false;
  if (s == "config") {
    *out = ItemType::kConfig;
    return true;
  }
  if (s == "phrases") {
    *out = ItemType::kPhrases;
    return true;
  }
  if (s == "userdb") {
    *out = ItemType::kUserDb;
    return true;
  }
  return false;
}

// `config` 是整份取大者的语义；`phrases` / `userdb` 是条目合并语义。
inline bool IsEntryMergedType(ItemType t) {
  return t == ItemType::kPhrases || t == ItemType::kUserDb;
}

// ===========================================================================
// 错误码（单测要能断言「因为什么理由失败」，不能只断言「失败了」）
// ===========================================================================

enum class EnvelopeError {
  kOk = 0,
  kEmpty,          // 输入为空
  kBadMagic,       // 首行不是 v1 / fluxing-entries-v1
  kBadField,       // 缺行、行序错、字段名错、字段重名、空行
  kBadType,        // type 不是 config/phrases/userdb
  kBadNumber,      // 十进制数字解析失败或溢出
  kBadNonce,       // nonce 长度 != kNonceBytes
  kBadBase64,      // base64 畸形（字符非法 / 长度不是 4 的倍数 / 填充位非 0）
  kBadEscape,      // 转义序列非法
  kTooLong,        // 载荷超 kMaxPayloadBytes
  kCryptoFailed,   // CNG 调用失败
  kAuthFailed,     // GCM 认证失败（AAD 被改 / 密文被改 / 密钥不对）
};

inline const char* EnvelopeErrorName(EnvelopeError e) {
  switch (e) {
    case EnvelopeError::kOk:
      return "kOk";
    case EnvelopeError::kEmpty:
      return "kEmpty";
    case EnvelopeError::kBadMagic:
      return "kBadMagic";
    case EnvelopeError::kBadField:
      return "kBadField";
    case EnvelopeError::kBadType:
      return "kBadType";
    case EnvelopeError::kBadNumber:
      return "kBadNumber";
    case EnvelopeError::kBadNonce:
      return "kBadNonce";
    case EnvelopeError::kBadBase64:
      return "kBadBase64";
    case EnvelopeError::kBadEscape:
      return "kBadEscape";
    case EnvelopeError::kTooLong:
      return "kTooLong";
    case EnvelopeError::kCryptoFailed:
      return "kCryptoFailed";
    case EnvelopeError::kAuthFailed:
      return "kAuthFailed";
  }
  return "unknown";
}

// ===========================================================================
// base64（严格模式）
// ===========================================================================
//
// 为什么要自己写：P3 不引第三方依赖，标准库没有 base64。严格模式的目的是
// 让「畸形输入必须报错」（T201 反例 ④）成立：
//   - 只允许 A-Za-z0-9+/ 与**末尾**的 '='（最多 2 个）；
//   - 总长必须是 4 的倍数（空串除外）；
//   - '=' 之后的填充位必须为 0（宽松解码器会静默丢弃，那种行为会掩盖
//     「密文被截断」类问题）。

namespace detail {

constexpr char kB64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 返回值：0..63 = 合法字符；-1 = 非法；-2 = padding '='。
inline int B64Value(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  if (c == '=')
    return -2;
  return -1;
}

// 把一串字节切成「行」（不含行尾 '\n'）。末尾单个 '\n' 不产生空尾行。
inline std::vector<std::string> SplitLines(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start < s.size()) {
    size_t nl = s.find('\n', start);
    if (nl == std::string::npos) {
      out.push_back(s.substr(start));
      break;
    }
    out.push_back(s.substr(start, nl - start));
    start = nl + 1;
  }
  return out;
}

// 按第一个 '\t' 切成 (name, value)。没有 '\t' ⇒ false。
inline bool SplitField(const std::string& line, std::string* name,
                       std::string* value) {
  size_t t = line.find('\t');
  if (t == std::string::npos)
    return false;
  *name = line.substr(0, t);
  *value = line.substr(t + 1);
  return true;
}

// 严格十进制解析（不允许 '+' / '-' / 空格 / 前后缀），并检测溢出。
inline bool ParseU64(const std::string& s, uint64_t* out) {
  if (out == nullptr || s.empty() || s.size() > 20)
    return false;
  const uint64_t kMax = 0xFFFFFFFFFFFFFFFFull;
  uint64_t v = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (c < '0' || c > '9')
      return false;
    uint64_t d = static_cast<uint64_t>(c - '0');
    if (v > (kMax - d) / 10)
      return false;
    v = v * 10 + d;
  }
  *out = v;
  return true;
}

inline bool ParseU16(const std::string& s, uint16_t* out) {
  uint64_t v = 0;
  if (!ParseU64(s, &v))
    return false;
  if (v > 0xFFFFull)
    return false;
  *out = static_cast<uint16_t>(v);
  return true;
}

// 字段值转义：'\' 'TAB' 'LF' 'CR' ⇒ 两字符序列。
// 目的是让 item_key（路径）、device（标识）在一行内可读且无歧义。
inline std::string EscapeField(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

inline bool UnescapeField(const std::string& s, std::string* out) {
  if (out == nullptr)
    return false;
  out->clear();
  out->reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c != '\\') {
      *out += c;
      continue;
    }
    if (i + 1 >= s.size())
      return false;  // 悬空反斜杠
    const char n = s[++i];
    switch (n) {
      case '\\':
        *out += '\\';
        break;
      case 't':
        *out += '\t';
        break;
      case 'n':
        *out += '\n';
        break;
      case 'r':
        *out += '\r';
        break;
      default:
        return false;  // 未知转义 ⇒ 显式失败，不猜
    }
  }
  return true;
}

}  // namespace detail

inline std::string Base64Encode(const uint8_t* data, size_t len) {
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i + 3 <= len) {
    const uint32_t n =
        (static_cast<uint32_t>(data[i]) << 16) |
        (static_cast<uint32_t>(data[i + 1]) << 8) |
        static_cast<uint32_t>(data[i + 2]);
    out += detail::kB64Table[(n >> 18) & 0x3F];
    out += detail::kB64Table[(n >> 12) & 0x3F];
    out += detail::kB64Table[(n >> 6) & 0x3F];
    out += detail::kB64Table[n & 0x3F];
    i += 3;
  }
  const size_t rest = len - i;
  if (rest == 1) {
    const uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    out += detail::kB64Table[(n >> 18) & 0x3F];
    out += detail::kB64Table[(n >> 12) & 0x3F];
    out += "==";
  } else if (rest == 2) {
    const uint32_t n = (static_cast<uint32_t>(data[i]) << 16) |
                       (static_cast<uint32_t>(data[i + 1]) << 8);
    out += detail::kB64Table[(n >> 18) & 0x3F];
    out += detail::kB64Table[(n >> 12) & 0x3F];
    out += detail::kB64Table[(n >> 6) & 0x3F];
    out += '=';
  }
  return out;
}

inline std::string Base64Encode(const std::string& s) {
  return Base64Encode(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

inline std::string Base64Encode(const std::vector<uint8_t>& v) {
  return Base64Encode(v.data(), v.size());
}

inline bool Base64Decode(const std::string& in, std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->clear();
  if (in.empty())
    return true;  // 空串是合法的空内容
  if ((in.size() % 4) != 0)
    return false;

  out->reserve((in.size() / 4) * 3);
  for (size_t i = 0; i < in.size(); i += 4) {
    int v[4];
    int pads = 0;
    for (int k = 0; k < 4; ++k) {
      v[k] = detail::B64Value(in[i + static_cast<size_t>(k)]);
      if (v[k] == -2) {
        // '=' 只允许出现在本组最后两个位置，且一旦出现后面必须全是 '='
        if (k < 2)
          return false;
        ++pads;
        v[k] = 0;
      } else if (v[k] < 0) {
        return false;  // 非法字符
      } else if (pads > 0) {
        return false;  // '=' 之后又出现有效字符
      }
    }
    if (pads > 2)
      return false;
    // 填充位必须为 0（严格模式：抓「密文被截断 / 编码器 bug」）
    if (pads == 2 && (v[1] & 0x0F) != 0)
      return false;
    if (pads == 1 && (v[2] & 0x03) != 0)
      return false;

    const uint32_t n = (static_cast<uint32_t>(v[0]) << 18) |
                       (static_cast<uint32_t>(v[1]) << 12) |
                       (static_cast<uint32_t>(v[2]) << 6) |
                       static_cast<uint32_t>(v[3]);
    out->push_back(static_cast<uint8_t>((n >> 16) & 0xFF));
    if (pads < 2)
      out->push_back(static_cast<uint8_t>((n >> 8) & 0xFF));
    if (pads < 1)
      out->push_back(static_cast<uint8_t>(n & 0xFF));
  }
  return true;
}

// ===========================================================================
// HLC（Hybrid Logical Clock）— plan.md §4.2
// ===========================================================================
//
// 全序：`ms` → `counter` → `device` 字典序，**大者胜**。
// `device` 参与比较是为了让「两台设备在同一毫秒、同一 counter 各写一次」
// 仍能分出确定赢家（否则两侧会互相覆盖，收敛不了）。

struct Hlc {
  uint64_t ms = 0;
  uint16_t counter = 0;
  std::string device;
};

// 返回 -1 / 0 / 1。严格弱序（可用于 std::sort / std::map）。
inline int CompareHlc(const Hlc& a, const Hlc& b) {
  if (a.ms != b.ms)
    return a.ms < b.ms ? -1 : 1;
  if (a.counter != b.counter)
    return a.counter < b.counter ? -1 : 1;
  if (a.device != b.device)
    return a.device < b.device ? -1 : 1;
  return 0;
}

inline bool HlcLess(const Hlc& a, const Hlc& b) {
  return CompareHlc(a, b) < 0;
}

inline bool HlcEqual(const Hlc& a, const Hlc& b) {
  return CompareHlc(a, b) == 0;
}

// 本机时钟状态。`last_ms` / `counter` 需要在本地持久化（生产代码写配置文件），
// 本文件只提供纯状态转移，不碰磁盘。
struct HlcClock {
  uint64_t last_ms = 0;
  uint16_t counter = 0;
  std::string device;
};

// 本机写入一次，返回新版本号。
// 规则（plan.md §4.2）：`ms = max(now_ms, last_ms)`；`ms == last_ms` 时
// `counter++`；否则 `counter = 0`。
//
// 两处防御性补充（plan 未写，但不违反其意图）：
//   1) `counter` 打满 0xFFFF ⇒ 推进 `ms` 一毫秒并归零，保证「每次写入都严格
//      大于上一次」，不会因为同毫秒内写 65536 次而退化成同一个版本号；
//   2) `now_ms < last_ms`（系统时钟回拨）⇒ 沿用 `last_ms` 并自增 counter，
//      保证单调性不被墙钟回拨破坏。
inline Hlc HlcTick(HlcClock* c, uint64_t now_ms) {
  if (now_ms > c->last_ms) {
    c->last_ms = now_ms;
    c->counter = 0;
  } else {
    if (c->counter == kHlcCounterMax) {
      c->last_ms += 1;
      c->counter = 0;
    } else {
      c->counter = static_cast<uint16_t>(c->counter + 1);
    }
  }
  Hlc h;
  h.ms = c->last_ms;
  h.counter = c->counter;
  h.device = c->device;
  return h;
}

// 收到远端版本号后调用：让本机时钟不落后于远端，避免下一次本机写入的版本号
// 比已同步过来的旧。
inline void HlcObserve(HlcClock* c, const Hlc& remote) {
  if (remote.ms > c->last_ms) {
    c->last_ms = remote.ms;
    c->counter = remote.counter;
  } else if (remote.ms == c->last_ms && remote.counter > c->counter) {
    c->counter = remote.counter;
  }
}

// ===========================================================================
// AAD（Additional Authenticated Data）
// ===========================================================================
//
// `aad = item_key | type | hlc_ms | hlc_counter | device`
// 换位 / 改版本 / 改文件名 ⇒ AAD 变 ⇒ GCM 校验失败。这是 T201 反例 ② 的判据。
inline std::string BuildAad(const std::string& item_key, ItemType type,
                            const Hlc& hlc) {
  std::string aad;
  aad.reserve(item_key.size() + hlc.device.size() + 48);
  aad += item_key;
  aad += '|';
  aad += ItemTypeName(type);
  aad += '|';
  aad += std::to_string(hlc.ms);
  aad += '|';
  aad += std::to_string(static_cast<unsigned>(hlc.counter));
  aad += '|';
  aad += hlc.device;
  return aad;
}

// ===========================================================================
// AES-256-GCM（CNG / bcrypt.lib）
// ===========================================================================
//
// 密文布局：`cipher = 密文 || 16B tag`（拼接，省一次字段）。
// 这样信封只需 nonce + cipher 两个字段，与 plan.md §4.1 一致。

namespace crypto {

// `STATUS_AUTH_TAG_MISMATCH` = 0xC000A002，但它只存在于 <ntstatus.h>；
// 而 ntstatus.h 与 windows.h 同时包含会撞状态码定义（除非先定义 WIN32_NO_STATUS）。
// 本头文件是 header-only 且被生产与单测共用，不能给每个使用方塞包含顺序地雷
// ⇒ 直接写字面值，语义与官方常量一致（Windows Kits 10.0.26100 `km/ntstatus.h`）。
constexpr NTSTATUS kStatusAuthTagMismatch = static_cast<NTSTATUS>(0xC000A002L);

// 清零敏感缓冲（避免密钥 / 明文残留在内存里被 dump）。
// 必须定义在所有使用点之前 —— 本文件是 header-only，没有独立的声明头。
inline void SecureZero(std::vector<uint8_t>& v) {
  if (!v.empty()) {
    ::SecureZeroMemory(v.data(), v.size());
    v.clear();
  }
}

inline bool GenerateNonce(std::vector<uint8_t>* out) {
  if (out == nullptr)
    return false;
  out->assign(kNonceBytes, 0);
  NTSTATUS st = ::BCryptGenRandom(nullptr, out->data(),
                                  static_cast<ULONG>(out->size()),
                                  BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (!NT_SUCCESS(st)) {
    out->clear();
    return false;
  }
  return true;
}

// 打开一次 GCM 计算所需的 alg + key 句柄。成功返回 true 并填 alg/key。
inline bool OpenGcm(const std::vector<uint8_t>& key, BCRYPT_ALG_HANDLE* alg,
                    BCRYPT_KEY_HANDLE* key_handle, std::string* err) {
  if (key.size() != kKeyBytes) {
    if (err)
      *err = "key 必须 32 字节（AES-256）";
    return false;
  }
  // CNG 没有独立的 "AES-GCM" 算法标识：打开 BCRYPT_AES_ALGORITHM，
  // 再通过 BCRYPT_CHAINING_MODE = BCRYPT_CHAIN_MODE_GCM 切到 GCM 模式。
  NTSTATUS st = ::BCryptOpenAlgorithmProvider(
      alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
  if (!NT_SUCCESS(st) || *alg == nullptr) {
    if (err)
      *err = "BCryptOpenAlgorithmProvider(GCM) 失败";
    return false;
  }
  st = ::BCryptSetProperty(*alg, BCRYPT_CHAINING_MODE,
                           reinterpret_cast<PUCHAR>(
                               const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                           sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
  if (!NT_SUCCESS(st)) {
    if (err)
      *err = "BCryptSetProperty(CHAINING_MODE_GCM) 失败";
    ::BCryptCloseAlgorithmProvider(*alg, 0);
    *alg = nullptr;
    return false;
  }
  st = ::BCryptGenerateSymmetricKey(*alg, key_handle, nullptr, 0,
                                    const_cast<PUCHAR>(key.data()),
                                    static_cast<ULONG>(key.size()), 0);
  if (!NT_SUCCESS(st) || *key_handle == nullptr) {
    if (err)
      *err = "BCryptGenerateSymmetricKey 失败";
    ::BCryptCloseAlgorithmProvider(*alg, 0);
    *alg = nullptr;
    return false;
  }
  return true;
}

inline void CloseGcm(BCRYPT_ALG_HANDLE alg, BCRYPT_KEY_HANDLE key_handle) {
  if (key_handle)
    ::BCryptDestroyKey(key_handle);
  if (alg)
    ::BCryptCloseAlgorithmProvider(alg, 0);
}

// 认证加密。输出 `out_cipher = 密文 || tag`。
inline bool AesGcmSeal(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& nonce, const std::string& aad,
                       const uint8_t* plain, size_t plain_len,
                       std::vector<uint8_t>* out_cipher, std::string* err) {
  if (out_cipher == nullptr)
    return false;
  out_cipher->clear();
  if (nonce.size() != kNonceBytes) {
    if (err)
      *err = "nonce 必须 12 字节";
    return false;
  }
  if (plain_len > kMaxPayloadBytes) {
    if (err)
      *err = "载荷超过 kMaxPayloadBytes";
    return false;
  }
  if (plain_len > 0xFFFFFFF0ull) {
    if (err)
      *err = "载荷超出 CNG 单次调用上限（>4GiB）";
    return false;
  }

  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_KEY_HANDLE kh = nullptr;
  if (!OpenGcm(key, &alg, &kh, err))
    return false;

  std::vector<uint8_t> buf(plain_len + kTagBytes);
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
  BCRYPT_INIT_AUTH_MODE_INFO(info);
  info.pbNonce = const_cast<PUCHAR>(nonce.data());
  info.cbNonce = static_cast<ULONG>(nonce.size());
  info.pbAuthData =
      aad.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(aad.data()));
  info.cbAuthData = static_cast<ULONG>(aad.size());
  info.pbTag = buf.data() + plain_len;
  info.cbTag = static_cast<ULONG>(kTagBytes);

  ULONG produced = 0;
  NTSTATUS st = ::BCryptEncrypt(
      kh, const_cast<PUCHAR>(plain), static_cast<ULONG>(plain_len), &info,
      nullptr, 0, buf.data(), static_cast<ULONG>(buf.size()), &produced, 0);

  const bool ok = NT_SUCCESS(st) && produced == plain_len;
  if (!ok && err)
    *err = "BCryptEncrypt 失败";
  CloseGcm(alg, kh);
  if (!ok) {
    SecureZero(buf);
    return false;
  }
  *out_cipher = std::move(buf);
  return true;
}

inline bool AesGcmSeal(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& nonce, const std::string& aad,
                       const std::string& plain,
                       std::vector<uint8_t>* out_cipher, std::string* err) {
  return AesGcmSeal(key, nonce, aad,
                    reinterpret_cast<const uint8_t*>(plain.data()),
                    plain.size(), out_cipher, err);
}

// 认证解密。`in_cipher = 密文 || tag`，长度必须 >= kTagBytes。
// AAD / 密文 / 密钥任一不匹配 ⇒ 返回 false 且 err 写 "GCM 认证失败"。
inline bool AesGcmOpen(const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& nonce, const std::string& aad,
                       const std::vector<uint8_t>& in_cipher,
                       std::string* out_plain, std::string* err) {
  if (out_plain == nullptr)
    return false;
  out_plain->clear();
  if (nonce.size() != kNonceBytes) {
    if (err)
      *err = "nonce 必须 12 字节";
    return false;
  }
  if (in_cipher.size() < kTagBytes) {
    if (err)
      *err = "密文长度小于认证标签长度";
    return false;
  }

  const size_t body_len = in_cipher.size() - kTagBytes;
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_KEY_HANDLE kh = nullptr;
  if (!OpenGcm(key, &alg, &kh, err))
    return false;

  std::vector<uint8_t> buf(body_len);
  BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
  BCRYPT_INIT_AUTH_MODE_INFO(info);
  info.pbNonce = const_cast<PUCHAR>(nonce.data());
  info.cbNonce = static_cast<ULONG>(nonce.size());
  info.pbAuthData =
      aad.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(aad.data()));
  info.cbAuthData = static_cast<ULONG>(aad.size());
  info.pbTag = const_cast<PUCHAR>(in_cipher.data() + body_len);
  info.cbTag = static_cast<ULONG>(kTagBytes);

  ULONG produced = 0;
  NTSTATUS st = ::BCryptDecrypt(
      kh, const_cast<PUCHAR>(in_cipher.data()), static_cast<ULONG>(body_len),
      &info, nullptr, 0, buf.empty() ? nullptr : buf.data(),
      static_cast<ULONG>(buf.size()), &produced, 0);

  CloseGcm(alg, kh);
  if (!NT_SUCCESS(st) || produced != body_len) {
    // 认证标签不匹配是这个分支最常见的成因（AAD 被改 / 密文被改 / 密钥错）。
    if (err)
      *err = (st == kStatusAuthTagMismatch)
                 ? "GCM 认证失败（AAD 或密文被篡改）"
                 : "BCryptDecrypt 失败";
    SecureZero(buf);
    return false;
  }
  out_plain->assign(reinterpret_cast<const char*>(buf.data()), buf.size());
  SecureZero(buf);
  return true;
}

}  // namespace crypto

// ===========================================================================
// 信封
// ===========================================================================

struct Envelope {
  std::string item_key;
  ItemType type = ItemType::kConfig;
  Hlc hlc;
  std::vector<uint8_t> nonce;   // 必须 kNonceBytes
  std::vector<uint8_t> cipher;  // 密文 || tag
};

inline EnvelopeError SerializeEnvelope(const Envelope& env, std::string* out) {
  if (out == nullptr)
    return EnvelopeError::kEmpty;
  if (env.nonce.size() != kNonceBytes)
    return EnvelopeError::kBadNonce;
  if (env.cipher.size() > kMaxPayloadBytes + kTagBytes)
    return EnvelopeError::kTooLong;

  std::string s;
  s.reserve(env.cipher.size() * 4 / 3 + env.item_key.size() + 128);
  s += "v1\n";
  s += "item_key\t";
  s += detail::EscapeField(env.item_key);
  s += "\ntype\t";
  s += ItemTypeName(env.type);
  s += "\nhlc_ms\t";
  s += std::to_string(env.hlc.ms);
  s += "\nhlc_counter\t";
  s += std::to_string(static_cast<unsigned>(env.hlc.counter));
  s += "\ndevice\t";
  s += detail::EscapeField(env.hlc.device);
  s += "\nnonce\t";
  s += Base64Encode(env.nonce);
  s += "\ncipher\t";
  s += Base64Encode(env.cipher);
  s += "\n";
  *out = std::move(s);
  return EnvelopeError::kOk;
}

inline EnvelopeError ParseEnvelope(const std::string& wire, Envelope* out) {
  if (out == nullptr)
    return EnvelopeError::kEmpty;
  if (wire.empty())
    return EnvelopeError::kEmpty;

  const std::vector<std::string> lines = detail::SplitLines(wire);
  // 严格 8 行：magic + 7 字段。
  if (lines.size() != 8)
    return EnvelopeError::kBadField;
  if (lines[0] != "v1")
    return EnvelopeError::kBadMagic;

  static const char* const kExpected[7] = {
      "item_key", "type", "hlc_ms", "hlc_counter", "device", "nonce", "cipher"};
  std::string val[7];
  for (size_t i = 0; i < 7; ++i) {
    std::string name;
    if (!detail::SplitField(lines[i + 1], &name, &val[i]))
      return EnvelopeError::kBadField;
    if (name != kExpected[i])
      return EnvelopeError::kBadField;  // 行序/行名必须严格匹配
  }

  Envelope env;
  if (!detail::UnescapeField(val[0], &env.item_key))
    return EnvelopeError::kBadEscape;
  if (!ParseItemType(val[1], &env.type))
    return EnvelopeError::kBadType;
  if (!detail::ParseU64(val[2], &env.hlc.ms))
    return EnvelopeError::kBadNumber;
  if (!detail::ParseU16(val[3], &env.hlc.counter))
    return EnvelopeError::kBadNumber;
  if (!detail::UnescapeField(val[4], &env.hlc.device))
    return EnvelopeError::kBadEscape;
  if (!Base64Decode(val[5], &env.nonce))
    return EnvelopeError::kBadBase64;
  if (env.nonce.size() != kNonceBytes)
    return EnvelopeError::kBadNonce;
  if (!Base64Decode(val[6], &env.cipher))
    return EnvelopeError::kBadBase64;
  if (env.cipher.size() > kMaxPayloadBytes + kTagBytes)
    return EnvelopeError::kTooLong;

  *out = std::move(env);
  return EnvelopeError::kOk;
}

// ---------------------------------------------------------------------------
// 一步完成「加密 + 装信封」与「拆信封 + 解密」
// ---------------------------------------------------------------------------
//
// 生产代码与单测共用这两个入口 ⇒ 「AAD 怎么拼的」只有一份定义，
// 单测里改 `env.hlc.ms` 再 Open 必然失败（T201 反例 ②）。

inline EnvelopeError SealEnvelope(const std::vector<uint8_t>& enc_key,
                                  const std::string& item_key, ItemType type,
                                  const Hlc& hlc, const std::string& plain,
                                  Envelope* out, std::string* err) {
  if (out == nullptr)
    return EnvelopeError::kEmpty;
  if (plain.size() > kMaxPayloadBytes)
    return EnvelopeError::kTooLong;

  Envelope env;
  env.item_key = item_key;
  env.type = type;
  env.hlc = hlc;
  if (!crypto::GenerateNonce(&env.nonce))
    return EnvelopeError::kCryptoFailed;

  const std::string aad = BuildAad(item_key, type, hlc);
  if (!crypto::AesGcmSeal(enc_key, env.nonce, aad, plain, &env.cipher, err))
    return err && err->find("kMaxPayloadBytes") != std::string::npos
               ? EnvelopeError::kTooLong
               : EnvelopeError::kCryptoFailed;

  *out = std::move(env);
  return EnvelopeError::kOk;
}

inline EnvelopeError OpenEnvelope(const std::vector<uint8_t>& enc_key,
                                  const Envelope& env, std::string* out_plain,
                                  std::string* err) {
  if (out_plain == nullptr)
    return EnvelopeError::kEmpty;
  if (env.nonce.size() != kNonceBytes)
    return EnvelopeError::kBadNonce;
  if (env.cipher.size() < kTagBytes)
    return EnvelopeError::kBadBase64;

  const std::string aad = BuildAad(env.item_key, env.type, env.hlc);
  if (!crypto::AesGcmOpen(enc_key, env.nonce, aad, env.cipher, out_plain, err)) {
    if (err && err->find("认证失败") != std::string::npos)
      return EnvelopeError::kAuthFailed;
    return EnvelopeError::kCryptoFailed;
  }
  return EnvelopeError::kOk;
}

// ===========================================================================
// 条目表（phrases / userdb 的条目化载体）
// ===========================================================================
//
// 线格式：
//   fluxing-entries-v1
//   <key 转义>\t<payload base64>\t<ms>\t<counter>\t<device 转义>
//   ...
//
// payload 一律 base64：`phrases` 是文本、`userdb` 是二进制，统一走 base64
// 就没有「哪种类型该转义、哪种该原样」的分支，也不会因为 payload 里出现
// TAB / 换行而破坏行结构。

struct Entry {
  std::string key;
  std::string payload;  // 任意字节
  Hlc hlc;

  bool operator==(const Entry& o) const {
    return key == o.key && payload == o.payload && HlcEqual(hlc, o.hlc);
  }
};

inline std::string SerializeEntries(const std::vector<Entry>& entries) {
  std::string s;
  s += "fluxing-entries-v1\n";
  for (size_t i = 0; i < entries.size(); ++i) {
    const Entry& e = entries[i];
    s += detail::EscapeField(e.key);
    s += '\t';
    s += Base64Encode(e.payload);
    s += '\t';
    s += std::to_string(e.hlc.ms);
    s += '\t';
    s += std::to_string(static_cast<unsigned>(e.hlc.counter));
    s += '\t';
    s += detail::EscapeField(e.hlc.device);
    s += '\n';
  }
  return s;
}

inline EnvelopeError ParseEntries(const std::string& wire,
                                  std::vector<Entry>* out) {
  if (out == nullptr)
    return EnvelopeError::kEmpty;
  out->clear();
  if (wire.empty())
    return EnvelopeError::kEmpty;

  const std::vector<std::string> lines = detail::SplitLines(wire);
  if (lines.empty())
    return EnvelopeError::kEmpty;
  if (lines[0] != "fluxing-entries-v1")
    return EnvelopeError::kBadMagic;

  std::vector<Entry> parsed;
  parsed.reserve(lines.size() - 1);
  for (size_t i = 1; i < lines.size(); ++i) {
    const std::string& line = lines[i];
    if (line.empty())
      return EnvelopeError::kBadField;  // 空行 = 畸形，不静默跳过

    // 5 个 TAB 分隔字段
    std::vector<std::string> parts;
    size_t start = 0;
    bool truncated = false;
    for (int k = 0; k < 4; ++k) {
      size_t t = line.find('\t', start);
      if (t == std::string::npos) {
        truncated = true;
        break;
      }
      parts.push_back(line.substr(start, t - start));
      start = t + 1;
    }
    if (truncated)
      return EnvelopeError::kBadField;
    parts.push_back(line.substr(start));
    if (parts.size() != 5)
      return EnvelopeError::kBadField;

    Entry e;
    if (!detail::UnescapeField(parts[0], &e.key))
      return EnvelopeError::kBadEscape;
    std::vector<uint8_t> raw;
    if (!Base64Decode(parts[1], &raw))
      return EnvelopeError::kBadBase64;
    e.payload.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
    if (!detail::ParseU64(parts[2], &e.hlc.ms))
      return EnvelopeError::kBadNumber;
    if (!detail::ParseU16(parts[3], &e.hlc.counter))
      return EnvelopeError::kBadNumber;
    if (!detail::UnescapeField(parts[4], &e.hlc.device))
      return EnvelopeError::kBadEscape;
    parsed.push_back(std::move(e));
  }

  *out = std::move(parsed);
  return EnvelopeError::kOk;
}

// ---------------------------------------------------------------------------
// 条目合并 — plan.md §4.3
// ---------------------------------------------------------------------------
//
// 语义：异键取并集；同键取 HLC 较大者（`ms` → `counter` → `device`）。
//
// 输出**恒按 key 升序**，并保证每个 key 只出现一次。这一点是下面三条性质
// 能成立的全部原因（T202 判据 ④ 结合律 / ⑤ 两端收敛）：
//   - 幂等：    merge(x,x) == x  ⇔ 输出是 x 的规范形态
//   - 交换：    merge(a,b) == merge(b,a)   ⇔ 对 a∪b 做集合运算
//   - 结合：    merge(merge(a,b),c) == merge(a,merge(b,c))  ⇔ 同一条全序
//
// 「同 key 且 HLC 完全相同、payload 却不同」在正确实现下不会发生（同一设备
// 同一时刻不可能写出两个不同值）。真的发生了说明有客户端 bug 或恶意输入，
// 这里用 payload 字典序大者胜做兜底 —— 目标是**确定性与收敛**，而不是判谁对。
inline std::vector<Entry> MergeEntries(const std::vector<Entry>& a,
                                       const std::vector<Entry>& b) {
  std::vector<Entry> all;
  all.reserve(a.size() + b.size());
  all.insert(all.end(), a.begin(), a.end());
  all.insert(all.end(), b.begin(), b.end());

  std::sort(all.begin(), all.end(), [](const Entry& x, const Entry& y) {
    if (x.key != y.key)
      return x.key < y.key;      // key 升序
    const int c = CompareHlc(x.hlc, y.hlc);
    if (c != 0)
      return c > 0;              // HLC 大者在前 ⇒ 去重时首个即胜者
    return x.payload > y.payload;  // 同 HLC 兜底：确定性收敛
  });

  std::vector<Entry> out;
  out.reserve(all.size());
  for (size_t i = 0; i < all.size(); ++i) {
    if (!out.empty() && out.back().key == all[i].key)
      continue;  // 同键后续项一律让位给排序后的首个
    out.push_back(all[i]);
  }
  return out;
}

}  // namespace cloud
}  // namespace fluxing