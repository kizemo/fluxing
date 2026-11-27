// spec 100 阶段 D · T304 同步编排实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudSyncService.h"

#include "CloudCrypto.h"
#include "CloudJson.h"
#include "CloudTransport.h"
#include "CloudUserDictIo.h"

#include "../../include/FluxingDiagLog.h"

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fluxing {
namespace cloud {

namespace {

// nodes 是平铺数组，Member() 只回指针；需要「节点下标」时用它换算回来。
// ★ 实现委托给公开的 CloudJsonNodeIndex ⇒ 桶级解析（CloudBucketSync.cpp）
//   与条目级解析共用同一份换算逻辑（宪法 R7「不复制第二份实现」）。
int NodeIndex(const json::JsonDocument& doc, const json::JsonNode* node) {
  return CloudJsonNodeIndex(doc, node);
}

std::string HexLower(const std::vector<uint8_t>& bytes) {
  static const char* kHex = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (size_t i = 0; i < bytes.size(); ++i) {
    out += kHex[(bytes[i] >> 4) & 0x0F];
    out += kHex[bytes[i] & 0x0F];
  }
  return out;
}

std::string Sha256Hex(const std::string& text) {
  std::vector<uint8_t> digest;
  if (!Sha256(reinterpret_cast<const uint8_t*>(text.data()), text.size(),
              &digest))
    return std::string();
  return HexLower(digest);
}

// Unix 纪元毫秒（HLC 用）。
uint64_t UnixTimeMs() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  const uint64_t ticks = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) |
                         static_cast<uint64_t>(ft.dwLowDateTime);
  return ticks / 10000ull - 11644473600000ull;
}

// sidecar 的键做最小转义，避免制表符 / 换行破坏行结构。
std::string EscapeKey(const std::string& key) {
  std::string out;
  out.reserve(key.size());
  for (size_t i = 0; i < key.size(); ++i) {
    if (key[i] == '\\') {
      out += "\\\\";
    } else if (key[i] == '\t') {
      out += "\\t";
    } else if (key[i] == '\n') {
      out += "\\n";
    } else if (key[i] == '\r') {
      out += "\\r";
    } else {
      out += key[i];
    }
  }
  return out;
}

std::string UnescapeKey(const std::string& key) {
  std::string out;
  out.reserve(key.size());
  for (size_t i = 0; i < key.size(); ++i) {
    if (key[i] == '\\' && i + 1 < key.size()) {
      const char next = key[++i];
      if (next == 't') {
        out += '\t';
      } else if (next == 'n') {
        out += '\n';
      } else if (next == 'r') {
        out += '\r';
      } else {
        out += next;
      }
    } else {
      out += key[i];
    }
  }
  return out;
}

std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> cols;
  size_t pos = 0;
  while (true) {
    size_t tab = line.find('\t', pos);
    if (tab == std::string::npos) {
      cols.push_back(line.substr(pos));
      break;
    }
    cols.push_back(line.substr(pos, tab - pos));
    pos = tab + 1;
  }
  return cols;
}

// 诊断行里的错误文本截断：最多 max_bytes **字节**，且不把 UTF-8 多字节序列
// 切一半（切一半会让日志里出现半个汉字）。
std::string TruncateUtf8(const std::string& text, size_t max_bytes) {
  if (text.size() <= max_bytes)
    return text;
  size_t cut = max_bytes;
  while (cut > 0 && ((static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80))
    --cut;
  return text.substr(0, cut);
}

// 侧车是不是 v2 格式：首行是 v2 版本行，**或**正文里出现 phase 行
// （ResetCloudSyncSidecarFile 写的重置文件带 phase\t0 ⇒ 也算 v2）。
// ⇒ 阶段 0 且侧车是 v1 时，这轮继续写 v1（与本批之前逐字一致）。
bool IsSidecarV2Text(const std::string& text) {
  if (text.compare(0, 22, "#fluxing-cloud-sync-v2") == 0)
    return true;
  return text.find("\nphase\t") != std::string::npos;
}

}  // namespace

// ---------------------------------------------------------------------------
// 纯函数
// ---------------------------------------------------------------------------

std::string HlcToJson(const Hlc& hlc) {
  std::string s = "{\"ms\":";
  s += std::to_string(hlc.ms);
  s += ",\"counter\":";
  s += std::to_string(static_cast<uint64_t>(hlc.counter));
  s += ",\"device\":";
  s += json::Quote(hlc.device);
  s += "}";
  return s;
}

bool HlcFromJson(const json::JsonDocument& doc, int node_index, Hlc* out) {
  if (out == nullptr)
    return false;
  const json::JsonNode* node = doc.Node(node_index);
  if (node == nullptr || node->type != json::JsonType::kObject)
    return false;
  int64_t ms = 0;
  int64_t counter = 0;
  std::string device;
  if (!json::MemberInt64(doc, node_index, "ms", &ms) || ms < 0)
    return false;
  if (!json::MemberInt64(doc, node_index, "counter", &counter) || counter < 0 ||
      counter > static_cast<int64_t>(kHlcCounterMax))
    return false;
  if (!json::MemberString(doc, node_index, "device", &device) || device.empty())
    return false;
  out->ms = static_cast<uint64_t>(ms);
  out->counter = static_cast<uint32_t>(counter);
  out->device = device;
  return true;
}

std::string BuildPushBody(const std::vector<CloudSyncItem>& items) {
  std::string s = "{\"items\":[";
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      s += ",";
    s += "{\"item_key\":";
    s += json::Quote(items[i].item_key);
    s += ",\"type\":";
    s += json::Quote(items[i].type);
    // HLC 只以嵌套形态出现（tasks.md T304 的硬要求）。
    s += ",\"hlc\":";
    s += HlcToJson(items[i].hlc);
    s += ",\"nonce\":";
    s += json::Quote(items[i].nonce_b64);
    s += ",\"cipher\":";
    s += json::Quote(items[i].cipher_b64);
    s += "}";
  }
  s += "]}";
  return s;
}

bool ParsePullResponse(const std::string& body,
                       std::vector<CloudSyncItem>* out,
                       int64_t* server_seq,
                       std::string* err,
                       bool* has_more) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：拉取结果输出为空。";
    return false;
  }
  out->clear();

  json::JsonDocument doc;
  if (!json::Parse(body, &doc) || doc.root < 0) {
    if (err)
      *err = "服务器返回的数据无法解析" +
             (doc.error.empty() ? std::string("。") : ("：" + doc.error));
    return false;
  }
  const json::JsonNode* root = doc.Node(doc.root);
  if (root == nullptr || root->type != json::JsonType::kObject) {
    if (err)
      *err = "服务器返回的拉取结果格式不正确。";
    return false;
  }

  // ★ 顶层 server_seq 缺失时**保留调用方传入的值**（旧实现无条件写 0）：
  //   写 0 会让游标永不前进 ⇒ 每轮重复拉取同一批条目 ⇒ 直接冲击每日
  //   50 次拉取配额。条目自带的 server_seq 更可靠，一并参与推进。
  int64_t top_seq = 0;
  const bool has_top_seq =
      json::MemberInt64(doc, doc.root, "server_seq", &top_seq);
  if (has_more != nullptr)
    json::MemberBool(doc, doc.root, "has_more", has_more);

  int64_t max_item_seq = 0;
  bool has_item_seq = false;

  const json::JsonNode* items = json::Member(doc, doc.root, "items");
  const int items_index = NodeIndex(doc, items);
  if (items_index < 0)
    return true;  // 没有 items 字段 = 没有新条目
  const json::JsonNode* array = doc.Node(items_index);
  if (array == nullptr || array->type != json::JsonType::kArray) {
    if (err)
      *err = "服务器返回的条目列表格式不正确。";
    return false;
  }

  for (size_t i = 0; i < array->children.size(); ++i) {
    const int index = array->children[i];
    CloudSyncItem item;
    if (!json::MemberString(doc, index, "item_key", &item.item_key) ||
        item.item_key.empty()) {
      if (err)
        *err = "服务器返回的条目缺少 item_key。";
      return false;
    }
    if (!json::MemberString(doc, index, "type", &item.type) ||
        item.type.empty()) {
      if (err)
        *err = "服务器返回的条目缺少 type。";
      return false;
    }
    const json::JsonNode* hlc_node = json::Member(doc, index, "hlc");
    const int hlc_index = NodeIndex(doc, hlc_node);
    if (hlc_index < 0 || !HlcFromJson(doc, hlc_index, &item.hlc)) {
      if (err)
        *err = "服务器返回的条目 HLC 格式不正确。";
      return false;
    }
    if (!json::MemberString(doc, index, "nonce", &item.nonce_b64) ||
        !json::MemberString(doc, index, "cipher", &item.cipher_b64)) {
      if (err)
        *err = "服务器返回的条目缺少 nonce 或 cipher。";
      return false;
    }
    if (json::MemberInt64(doc, index, "server_seq", &item.server_seq)) {
      has_item_seq = true;
      if (item.server_seq > max_item_seq)
        max_item_seq = item.server_seq;
    }
    out->push_back(item);
  }

  // 游标推进：条目自带的 server_seq 更可靠（顶层字段可能缺失）。
  // 只增不减 —— 绝不把调用方已经推进过的游标退回去。
  if (server_seq != nullptr) {
    if (has_item_seq) {
      if (max_item_seq > *server_seq)
        *server_seq = max_item_seq;
    } else if (has_top_seq && top_seq > *server_seq) {
      *server_seq = top_seq;
    }
  }
  return true;
}

// 读一个 item_key 字符串数组（缺字段 ⇒ 空数组；类型不符 ⇒ false）。
static bool ReadItemKeyArray(const json::JsonDocument& doc,
                             const char* key,
                             std::vector<std::string>* out) {
  if (out != nullptr)
    out->clear();
  const json::JsonNode* node = json::Member(doc, doc.root, key);
  const int index = NodeIndex(doc, node);
  if (index < 0)
    return true;  // 字段缺失 ⇒ 空数组（老服务端没有该字段）
  const json::JsonNode* array = doc.Node(index);
  if (array == nullptr || array->type != json::JsonType::kArray)
    return false;
  for (size_t i = 0; i < array->children.size(); ++i) {
    const json::JsonNode* item = doc.Node(array->children[i]);
    if (item == nullptr || item->type != json::JsonType::kString)
      return false;
    if (out != nullptr)
      out->push_back(item->string_value);
  }
  return true;
}

// ---------------------------------------------------------------------------
// item_key 的线上身份（spec 100 AC-07 / ADR 0059）
// ---------------------------------------------------------------------------

// HKDF info 标签：enc_key → item_key 的 MAC 密钥。
// ★ 必须与 ADR 0059 及 scripts/cloud/live-e2e-probe.mjs 里的第三份独立实现
//   逐字一致；任何一侧漂移都会导致「服务端认得、客户端认不得」的静默分叉。
static constexpr char kItemKeyMacInfo[] = "fluxing-itemkey-mac-v1";

// 摘要 → 64 位小写十六进制。
static std::string ToLowerHex(const std::vector<uint8_t>& bytes) {
  static const char kHex[] = "0123456789abcdef";
  std::string out(bytes.size() * 2, '\0');
  for (size_t i = 0; i < bytes.size(); ++i) {
    out[2 * i] = kHex[(bytes[i] >> 4) & 0x0F];
    out[2 * i + 1] = kHex[bytes[i] & 0x0F];
  }
  return out;
}

// HMAC-SHA256(mac_key, text) → 十六进制。text 为空 ⇒ 空串（调用方当错误）。
static std::string HmacHex(const std::vector<uint8_t>& mac_key,
                           const std::string& text) {
  if (text.empty())
    return std::string();
  std::vector<uint8_t> digest;
  if (!HmacSha256(mac_key, reinterpret_cast<const uint8_t*>(text.data()),
                  text.size(), &digest))
    return std::string();
  if (digest.size() != 32) {
    SecureZeroBytes(&digest);
    return std::string();
  }
  std::string out = ToLowerHex(digest);
  SecureZeroBytes(&digest);
  return out;
}

bool DeriveItemKeyMacKey(const std::vector<uint8_t>& enc_key,
                         std::vector<uint8_t>* out,
                         std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：条目标识密钥输出为空。";
    return false;
  }
  out->clear();
  if (enc_key.size() != kKeyBytes) {
    if (err)
      *err = "加密密钥长度不正确，无法派生条目标识密钥。";
    return false;
  }
  if (!HkdfSha256(enc_key, kItemKeyMacInfo, out) || out->size() != kKeyBytes) {
    out->clear();
    if (err)
      *err = "派生条目标识密钥失败。";
    return false;
  }
  return true;
}

std::string ComputeItemKeyId(const std::vector<uint8_t>& enc_key,
                             const std::string& item_key) {
  if (item_key.empty())
    return std::string();
  std::vector<uint8_t> mac_key;
  if (!DeriveItemKeyMacKey(enc_key, &mac_key, nullptr))
    return std::string();
  std::string id = HmacHex(mac_key, item_key);
  SecureZeroBytes(&mac_key);
  return id;
}

// 密封载荷内部的封包：[4 字节大端 key_len][明文 item_key][payload]。
// 用长度前缀而不是分隔符：词条里可能出现任意字节（含 TAB），分隔符不可靠。
static void PackSealedPlain(const std::string& key,
                            const std::string& payload,
                            std::string* out) {
  out->clear();
  const uint32_t n = static_cast<uint32_t>(key.size());
  out->reserve(4 + key.size() + payload.size());
  out->push_back(static_cast<char>((n >> 24) & 0xFFu));
  out->push_back(static_cast<char>((n >> 16) & 0xFFu));
  out->push_back(static_cast<char>((n >> 8) & 0xFFu));
  out->push_back(static_cast<char>(n & 0xFFu));
  out->append(key);
  out->append(payload);
}

static bool UnpackSealedPlain(const std::string& plain,
                              std::string* key,
                              std::string* payload,
                              std::string* err) {
  if (plain.size() < 4) {
    if (err)
      *err = "云端条目的明文过短，无法还原条目键。";
    return false;
  }
  const uint32_t n =
      (static_cast<uint32_t>(static_cast<uint8_t>(plain[0])) << 24) |
      (static_cast<uint32_t>(static_cast<uint8_t>(plain[1])) << 16) |
      (static_cast<uint32_t>(static_cast<uint8_t>(plain[2])) << 8) |
      static_cast<uint32_t>(static_cast<uint8_t>(plain[3]));
  if (n == 0 || plain.size() < 4 + static_cast<size_t>(n)) {
    if (err)
      *err = "云端条目的明文结构不合法。";
    return false;
  }
  key->assign(plain, 4, n);
  payload->assign(plain, 4 + static_cast<size_t>(n),
                  plain.size() - 4 - static_cast<size_t>(n));
  return true;
}

// item_key 列表里是否含某个键（比对服务端返回的 accepted / skipped 用）。
static bool ContainsItemKey(const std::vector<std::string>& keys,
                            const std::string& key) {
  for (size_t i = 0; i < keys.size(); ++i) {
    if (keys[i] == key)
      return true;
  }
  return false;
}

PushDisposition ClassifyPushResult(const std::vector<std::string>& accepted,
                                   const std::vector<std::string>& skipped,
                                   const std::string& item_key) {
  if (ContainsItemKey(accepted, item_key))
    return PushDisposition::kAccepted;
  if (ContainsItemKey(skipped, item_key))
    return PushDisposition::kSkipped;
  return PushDisposition::kUnknown;
}

bool ParsePushResponse(const std::string& body,
                       std::vector<std::string>* accepted,
                       std::vector<std::string>* skipped,
                       int64_t* server_seq,
                       std::string* err) {
  json::JsonDocument doc;
  if (!json::Parse(body, &doc) || doc.root < 0) {
    if (err)
      *err = "服务器返回的数据无法解析。";
    return false;
  }
  if (server_seq != nullptr) {
    int64_t seq = 0;
    json::MemberInt64(doc, doc.root, "server_seq", &seq);
    *server_seq = seq;
  }
  const bool ok_accepted = ReadItemKeyArray(doc, "accepted", accepted);
  const bool ok_skipped = ReadItemKeyArray(doc, "skipped", skipped);
  if (!ok_accepted || !ok_skipped) {
    if (err)
      *err = "服务器返回的推送结果格式不正确。";
    return false;
  }
  return true;
}

// ★ spec 103 · BR-①g：单条条目 → 信封（桶级 blob 的构件）。
//   与 SealEntries **同一套**密码学口径：AES-256-GCM +
//   AAD = BuildAad(MAC 身份, type, hlc)，被加密的明文 =
//   [4 字节大端 key_len][明文 item_key][payload]
//   ⇒ 桶 blob 里**不出现明文词条**（AC-07 不破）。
bool SealEntryEnvelope(const Entry& entry,
                       ItemType type,
                       const std::vector<uint8_t>& enc_key,
                       Envelope* out,
                       std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：信封输出为空。";
    return false;
  }
  if (enc_key.size() != kKeyBytes) {
    if (err)
      *err = "加密密钥长度不正确，无法加密条目。";
    return false;
  }
  std::vector<uint8_t> nonce;
  if (!crypto::GenerateNonce(&nonce) || nonce.size() != kNonceBytes) {
    if (err)
      *err = "无法生成随机数。";
    return false;
  }
  // AC-07：明文键换成 MAC 线上身份，明文键本身移入密文内部。
  const std::string item_key_id = ComputeItemKeyId(enc_key, entry.key);
  if (item_key_id.empty()) {
    if (err)
      *err = "无法计算条目在云端的标识。";
    return false;
  }
  std::string packed;
  PackSealedPlain(entry.key, entry.payload, &packed);
  const std::string aad = BuildAad(item_key_id, type, entry.hlc);
  std::vector<uint8_t> cipher;
  std::string seal_err;
  if (!crypto::AesGcmSeal(enc_key, nonce, aad, packed, &cipher, &seal_err)) {
    SecureZeroString(&packed);
    if (err)
      *err = "加密条目失败：" +
             (seal_err.empty() ? std::string("未知原因") : seal_err);
    return false;
  }
  SecureZeroString(&packed);
  out->item_key = item_key_id;
  out->type = type;
  out->hlc = entry.hlc;
  out->nonce = nonce;
  out->cipher = cipher;
  return true;
}

// 信封 → 待推送条目（线上形态与 SealEntries **逐字一致** ⇒ 可直接喂
// OpenItemsTolerant 解密，与老端点同一条解密路径）。
bool CloudSyncItemFromEnvelope(const Envelope& env,
                               CloudSyncItem* out,
                               std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：条目输出为空。";
    return false;
  }
  if (env.item_key.empty()) {
    if (err)
      *err = "云端条目的标识为空。";
    return false;
  }
  CloudSyncItem item;
  item.item_key = env.item_key;
  item.type = ItemTypeName(env.type);
  item.hlc = env.hlc;
  item.nonce_b64 = Base64Encode(env.nonce);
  item.cipher_b64 = Base64Encode(env.cipher);
  *out = item;
  return true;
}

bool SealEntries(const std::vector<Entry>& entries,
                 ItemType type,
                 const std::vector<uint8_t>& enc_key,
                 std::vector<CloudSyncItem>* out,
                 std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：加密结果输出为空。";
    return false;
  }
  out->clear();
  if (entries.empty())
    return true;
  if (enc_key.size() != kKeyBytes) {
    if (err)
      *err = "加密密钥长度不正确，无法加密条目。";
    return false;
  }

  for (size_t i = 0; i < entries.size(); ++i) {
    // ★ 委托给单条实现 ⇒ 条目级与桶级共用同一份密封逻辑（宪法 R7）。
    Envelope env;
    if (!SealEntryEnvelope(entries[i], type, enc_key, &env, err))
      return false;
    CloudSyncItem item;
    item.item_key = env.item_key;
    item.type = ItemTypeName(env.type);
    item.hlc = env.hlc;
    item.nonce_b64 = Base64Encode(env.nonce);
    item.cipher_b64 = Base64Encode(env.cipher);
    out->push_back(item);
  }
  return true;
}

bool OpenItems(const std::vector<CloudSyncItem>& items,
               const std::vector<uint8_t>& enc_key,
               std::vector<Entry>* out,
               std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：解密结果输出为空。";
    return false;
  }
  out->clear();
  if (items.empty())
    return true;
  if (enc_key.size() != kKeyBytes) {
    if (err)
      *err = "加密密钥长度不正确，无法解密条目。";
    return false;
  }

  for (size_t i = 0; i < items.size(); ++i) {
    const CloudSyncItem& item = items[i];
    ItemType type = ItemType::kPhrases;
    if (!ParseItemType(item.type, &type)) {
      if (err)
        *err = "云端条目类型无法识别：" + item.type;
      return false;
    }
    std::vector<uint8_t> nonce;
    std::vector<uint8_t> cipher;
    if (!Base64Decode(item.nonce_b64, &nonce) || nonce.size() != kNonceBytes) {
      if (err)
        *err = "云端条目的随机数不合法。";
      return false;
    }
    if (!Base64Decode(item.cipher_b64, &cipher) || cipher.size() < kTagBytes) {
      if (err)
        *err = "云端条目的密文不合法。";
      return false;
    }
    const std::string aad = BuildAad(item.item_key, type, item.hlc);
    std::string plain;
    std::string open_err;
    if (!crypto::AesGcmOpen(enc_key, nonce, aad, cipher, &plain, &open_err)) {
      if (err)
        *err = "解密云端条目失败：" +
               (open_err.empty() ? std::string("未知原因") : open_err);
      return false;
    }
    // AC-07：明文键封在密文内部；线上的 item_key 只是 MAC 身份。
    // 拆封后必须重算 MAC 并与线值比对：AAD 里绑的是 item.item_key，
    // 若不复核，「A 的密文 + B 的身份」这种替换就解不出来也发现不了。
    Entry entry;
    std::string unpack_err;
    if (!UnpackSealedPlain(plain, &entry.key, &entry.payload, &unpack_err)) {
      SecureZeroString(&plain);
      if (err)
        *err = unpack_err;
      return false;
    }
    SecureZeroString(&plain);
    if (ComputeItemKeyId(enc_key, entry.key) != item.item_key) {
      SecureZeroString(&entry.payload);
      if (err)
        *err = "云端条目的标识与其内容不一致，已拒绝该条目。";
      return false;
    }
    entry.hlc = item.hlc;
    out->push_back(entry);
  }
  return true;
}

std::vector<Entry> MergeLocalAndRemote(const std::vector<Entry>& local,
                                       const std::vector<Entry>& remote) {
  return MergeEntries(local, remote);
}

bool DictRowsToEntries(const std::vector<CloudDictEntry>& rows,
                       std::vector<Entry>* out,
                       std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：条目输出为空。";
    return false;
  }
  out->clear();
  for (size_t i = 0; i < rows.size(); ++i) {
    Entry entry;
    entry.key = rows[i].text;
    entry.payload = rows[i].payload;
    out->push_back(entry);
  }
  return true;
}

bool EntriesToDictRows(const std::vector<Entry>& entries,
                       std::vector<CloudDictEntry>* out,
                       std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：词典行输出为空。";
    return false;
  }
  out->clear();
  for (size_t i = 0; i < entries.size(); ++i) {
    CloudDictEntry row;
    row.text = entries[i].key;
    row.payload = entries[i].payload;
    out->push_back(row);
  }
  return true;
}

// ---------------------------------------------------------------------------
// 网络
// ---------------------------------------------------------------------------

CloudSyncService::CloudSyncService(ICloudTransport* transport,
                                   std::string access_token)
    : transport_(transport), access_token_(std::move(access_token)) {}

bool CloudSyncService::SendJson(const std::string& method,
                                const std::string& path,
                                const std::string& body,
                                int* status,
                                std::string* response_body,
                                std::string* err) {
  if (transport_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }
  // ★ 401 → 续期 → 重试（最多一次）：同步路径此前**没有**这层，访问令牌一过期
  //   同步就持续失败（批次 BK 真机实测：两轮同步都报「访问令牌已过期，请用
  //   刷新令牌续期。」）；只有设备管理三处（ListDevices / RevokeDevice /
  //   DeleteCloudData）有。续期回调由账号页注入（刷新令牌只在本机）。
  //   · 续期成功 ⇒ 用**新令牌**重发同一个请求；
  //   · 续期失败 ⇒ 直接把中文原因交给界面（不再重试，避免死循环）；
  //   · 重试后仍是 401 ⇒ 走下面的通用错误分支（如实报服务端原因）。
  for (int attempt = 0; attempt < 2; ++attempt) {
    CloudHttpRequest req;
    req.method = method;
    req.path = path;
    req.headers.push_back({"Accept", "application/json"});
    if (!body.empty())
      req.headers.push_back({"Content-Type", "application/json"});
    if (!access_token_.empty())
      req.headers.push_back({"Authorization", "Bearer " + access_token_});
    req.body = body;

    const CloudHttpResponse resp = transport_->Send(req);
    if (!resp.transport_ok) {
      if (err)
        *err =
            "网络请求失败：" + (resp.error.empty() ? "未知原因" : resp.error);
      return false;
    }
    if (status)
      *status = resp.status;
    if (response_body)
      *response_body = resp.body;
    if (resp.status >= 200 && resp.status < 300)
      return true;
    if (resp.status == 401 && attempt == 0 && refresher_) {
      std::string new_token;
      std::string refresh_err;
      if (!refresher_(&new_token, &refresh_err) || new_token.empty()) {
        if (err)
          *err = refresh_err.empty() ? "访问令牌已过期，请重新登录。"
                                     : refresh_err;
        return false;
      }
      access_token_ = new_token;
      token_refreshed_ = true;
      // ★ 批次 BM / ADR 0071：无条件诊断行（不依赖调试器 —— ADR 0066 已坐实
      //   OutputDebugStringW 在没有调试器的机器上抓不到）。
      //   **绝不落任何条目明文**：只记录「发生过续期」这一事实。
      diag::AppendLine(
          "cloudsync",
          "access token refreshed after 401; retry with new token");
      continue;  // 用新令牌重发同一个请求
    }
    if (err) {
      json::JsonDocument doc;
      std::string message;
      if (json::Parse(resp.body, &doc) && doc.root >= 0) {
        const json::JsonNode* error_node = json::Member(doc, doc.root, "error");
        const int error_index = NodeIndex(doc, error_node);
        if (error_index >= 0)
          json::MemberString(doc, error_index, "message", &message);
      }
      if (message.empty())
        message =
            "服务器返回错误（HTTP " + std::to_string(resp.status) + "）。";
      *err = message;
    }
    return false;
  }
  return false;
}

bool CloudSyncService::Push(const std::vector<CloudSyncItem>& items,
                            int64_t* server_seq,
                            std::string* err,
                            std::vector<std::string>* accepted,
                            std::vector<std::string>* skipped,
                            int* http_status) {
  if (items.empty())
    return true;  // 服务端拒绝空数组

  int status = 0;
  std::string response;
  std::string send_err;
  if (!SendJson("POST", "/v1/sync/push", BuildPushBody(items), &status,
                &response, &send_err)) {
    if (http_status)
      *http_status = status;  // 传输层失败 ⇒ 0；HTTP 错误 ⇒ 服务端状态码
    if (err)
      *err = send_err.empty() ? "推送失败。" : send_err;
    return false;
  }
  if (http_status)
    *http_status = status;

  // ★ 必须把 accepted / skipped 解析出来（AH-4 ⑤）：服务端 HLC 守卫会把
  //   「存储版本 ≥ 入参版本」的条目放进 skipped，客户端不能当成功。
  return ParsePushResponse(response, accepted, skipped, server_seq, err);
}

bool CloudSyncService::Pull(int64_t since,
                            int64_t limit,
                            std::vector<CloudSyncItem>* out,
                            int64_t* new_server_seq,
                            std::string* err,
                            bool* has_more) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：拉取结果输出为空。";
    return false;
  }
  std::string path =
      "/v1/sync/pull?since=" + std::to_string(since < 0 ? 0 : since);
  if (limit > 0)
    path += "&limit=" + std::to_string(limit);

  int status = 0;
  std::string response;
  std::string send_err;
  if (!SendJson("GET", path, "", &status, &response, &send_err)) {
    if (err)
      *err = send_err.empty() ? "拉取失败。" : send_err;
    return false;
  }
  return ParsePullResponse(response, out, new_server_seq, err, has_more);
}

// ---------------------------------------------------------------------------
// 桶级端点（spec 103 · BR-①g · ADR 0077）
// ---------------------------------------------------------------------------
//
// ⚠ 回滚口径：桶端点与老端点**并存**，由灰度阶段决定这一轮发哪些请求
//   （阶段 0 = 只走老端点 ⇒ 行为与本批之前逐字不变）。

// 桶级推送：POST /v1/sync/buckets/push
// device_id **一律不上报**（服务端拿它跟登录设备的行主键比对，不等即 400）。
bool CloudSyncService::PushBuckets(
    const std::vector<CloudBucketPushItem>& buckets,
    int64_t* server_seq,
    std::string* err,
    std::vector<std::string>* accepted,
    std::vector<std::string>* skipped,
    int* http_status) {
  if (buckets.empty())
    return true;  // 服务端拒绝空数组

  int status = 0;
  std::string response;
  std::string send_err;
  if (!SendJson("POST", "/v1/sync/buckets/push",
                BuildBucketPushBody(buckets, std::string()), &status, &response,
                &send_err)) {
    if (http_status)
      *http_status = status;  // 传输层失败 ⇒ 0；HTTP 错误 ⇒ 服务端状态码
    if (err)
      *err = send_err.empty() ? "推送失败。" : send_err;
    return false;
  }
  if (http_status)
    *http_status = status;
  // 复用条目级的响应解析（accepted / skipped 都是字符串数组；这里装的是
  // bucket_key）⇒ 服务端整桶跳过（HLC 守卫命中）时客户端同样能识别。
  return ParsePushResponse(response, accepted, skipped, server_seq, err);
}

// 桶级拉取：GET /v1/sync/buckets/pull?since=<n>&limit=<n>[&cursor=<c>]
bool CloudSyncService::PullBuckets(int64_t since,
                                   int64_t limit,
                                   const std::string& cursor,
                                   std::vector<CloudBucketPullItem>* out,
                                   int64_t* new_server_seq,
                                   std::string* err,
                                   bool* has_more,
                                   std::string* next_cursor,
                                   int* http_status) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：拉取结果输出为空。";
    return false;
  }
  const std::string path = BuildBucketPullPath(since, limit, cursor);

  int status = 0;
  std::string response;
  std::string send_err;
  if (!SendJson("GET", path, "", &status, &response, &send_err)) {
    if (http_status)
      *http_status = status;  // 传输层失败 ⇒ 0；HTTP 错误 ⇒ 服务端状态码
    if (err)
      *err = send_err.empty() ? "拉取失败。" : send_err;
    return false;
  }
  if (http_status)
    *http_status = status;
  return ParseBucketPullResponse(response, out, new_server_seq, err, has_more,
                                 next_cursor);
}

// ---------------------------------------------------------------------------
// 编排
// ---------------------------------------------------------------------------

CloudSyncCoordinator::CloudSyncCoordinator(ICloudTransport* transport,
                                           IUserDictBackend* backend,
                                           std::string access_token,
                                           std::string device_id,
                                           std::vector<uint8_t> enc_key,
                                           std::string hlc_sidecar_path)
    : transport_(transport),
      backend_(backend),
      access_token_(std::move(access_token)),
      device_id_(std::move(device_id)),
      enc_key_(std::move(enc_key)),
      sidecar_path_(std::move(hlc_sidecar_path)) {
  clock_.device = device_id_;
}

CloudSyncCoordinator::~CloudSyncCoordinator() = default;

bool CloudSyncCoordinator::LoadSidecar(std::string* err) {
  sidecar_rows_.clear();
  last_server_seq_ = 0;
  key_rotated_ = false;
  bucket_rows_.clear();
  bucket_seq_ = 0;
  bucket_phase_ = CloudBucketPhase::kLegacy;
  sidecar_v2_ = false;
  if (sidecar_path_.empty())
    return true;

  std::ifstream in(sidecar_path_.c_str(), std::ios::binary);
  if (!in)
    return true;  // 没有 sidecar 是正常的（第一次同步）

  std::stringstream buf;
  buf << in.rdbuf();
  const std::string text = buf.str();
  sidecar_v2_ = IsSidecarV2Text(text);
  int phase = 0;
  // ★ v1 / v2 同一套解析器 ⇒ v1 文件读出来 phase=0、bseq=0、无 brow。
  const bool ok = ParseSidecarTextV2(text, enc_key_, &sidecar_rows_,
                                     &last_server_seq_, &bucket_seq_, &phase,
                                     &bucket_rows_, &key_rotated_, err);
  if (phase < 0 || phase > 4)
    phase = 0;
  bucket_phase_ = static_cast<CloudBucketPhase>(phase);
  return ok;
}

bool CloudSyncCoordinator::SaveSidecar(std::string* err) {
  if (sidecar_path_.empty())
    return true;
  std::ofstream out(sidecar_path_.c_str(), std::ios::binary | std::ios::trunc);
  if (!out) {
    if (err)
      *err = "无法写入同步状态文件：" + sidecar_path_;
    return false;
  }
  // ★ 阶段 0 且侧车仍是 v1 ⇒ 写 v1（与本批之前**逐字一致**）；
  //   其余情形（阶段 ≠ 0，或侧车已经是 v2）⇒ 写 v2（带 bseq / phase / brow）。
  if (sidecar_v2_ || bucket_phase_ != CloudBucketPhase::kLegacy) {
    out << BuildSidecarTextV2(sidecar_rows_, last_server_seq_, bucket_seq_,
                              BucketPhaseValue(bucket_phase_), bucket_rows_,
                              enc_key_);
  } else {
    out << BuildSidecarText(sidecar_rows_, last_server_seq_, enc_key_);
  }
  if (!out) {
    if (err)
      *err = "写入同步状态文件失败：" + sidecar_path_;
    return false;
  }
  return true;
}

namespace {

// 在条目表里找 key 的下标（找不到返回 static_cast<size_t>(-1)）。
size_t FindEntryIndex(const std::vector<Entry>& entries,
                      const std::string& key) {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].key == key)
      return i;
  }
  return static_cast<size_t>(-1);
}

// 在 sidecar 行里找 key。
const CloudSidecarRow* FindSidecarRow(const std::vector<CloudSidecarRow>& rows,
                                      const std::string& key) {
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].item_key == key)
      return &rows[i];
  }
  return nullptr;
}

// 远端版本是否在合并里胜出。规则与 MergeEntries 一致：
// 先比 HLC（ms → counter → device），HLC 完全相同时比 payload 字典序。
bool RemoteWins(const Entry& remote, const Entry& local) {
  const int c = CompareHlc(remote.hlc, local.hlc);
  if (c != 0)
    return c > 0;
  return remote.payload > local.payload;
}

// 单次推送的条目上限见头文件（kPushBatchSize，与服务端 MAX_PUSH_ITEMS 对齐，
// 那里有 static_assert 兜着「改大即编译失败」）。
// 单次拉取的条目上限（服务端 MAX_PULL_LIMIT = 100）。
constexpr int64_t kPullPageLimit = 100;
// 单轮最多拉取的页数（超出部分留到下一轮，状态里说明「还有更多」）。
constexpr int kMaxPullPages = 10;

// 批次 BR-1h: early-exit tagging. stats may be nullptr (sole call site passes &stats).
// Codes come from the CloudSyncEarlyExit closed set; a new early-return branch that
// forgets to tag is caught by SyncOnce fallback (records unattributed, never a fake
// zero-write round).
inline void MarkEarlyExit(CloudReconcileStats* stats, CloudSyncEarlyExit code) {
  if (stats != nullptr)
    stats->early_exit = code;
}
}  // namespace

bool CloudSyncCoordinator::SyncOnce(std::string* err) {
  CloudSyncRoundResult result;
  return SyncOnce(SyncDirection::kBoth, &result, err);
}

bool CloudSyncCoordinator::SyncOnce(SyncDirection direction,
                                    CloudSyncRoundResult* result,
                                    std::string* err) {
  CloudSyncRoundResult local_result;
  CloudSyncRoundResult* out = result != nullptr ? result : &local_result;
  CloudReconcileStats stats;
  const bool ok = SyncRound(direction, out, err, &stats);
  stats.pushed = out->items_pushed;
  // ★ BR-①h：兜底归因 —— 本轮失败却没有任何早退打标 ⇒ 记 unattributed
  //   （将来新增早退分支若漏打标，诊断行会**自曝**，绝不静默伪装成真空轮）。
  if (!ok && stats.early_exit == CloudSyncEarlyExit::kNone)
    stats.early_exit = CloudSyncEarlyExit::kUnattributed;
  // 本轮计数留存一份（失败轮也刷新）⇒ 对外读取口与诊断行**同源**。
  last_stats_ = stats;
  // ★ ADR 0076 D2-5a-6（R5 · 观测口径）：**无条件**诊断行（本轮失败也照打）
  //   ⇒ 判据取证不依赖界面显示（1017 那行只显示成功态摘要）。
  diag::AppendLine("cloudsync", FormatReconcileDiagLine(stats));
  // ★ spec 103 · BR-①g：桶级诊断行（**无条件**打印，本轮失败也照打 ⇒
  //   与既有 reconcile / push batch 行并存，判据取证不依赖界面显示）。
  diag::AppendLine("cloudsync", FormatBucketDiagLine(last_bucket_stats_));
  return ok;
}

bool CloudSyncCoordinator::SyncRound(SyncDirection direction,
                                     CloudSyncRoundResult* result,
                                     std::string* err,
                                     CloudReconcileStats* stats) {
  CloudSyncRoundResult local_result;
  CloudSyncRoundResult* out = result != nullptr ? result : &local_result;
  out->ok = false;
  out->push_requests = 0;
  out->pull_requests = 0;
  out->items_pushed = 0;
  out->items_pulled = 0;
  out->has_more = false;
  // ADR 0062: 「跳过」标记（闸门关闭 => 整轮跳过，不是失败）。
  out->skipped = false;
  out->message.clear();
  // ADR 0072: 本轮的可选提示（成功态界面要显示它们）。
  out->notices.clear();
  // ★ spec 103 · BR-①g：本轮桶计数先归零（失败轮也打一行 ⇒ 不能留上一轮的数）。
  last_bucket_stats_ = CloudBucketRoundStats();

  if (transport_ == nullptr || backend_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    out->message = "云端服务尚未初始化。";
    MarkEarlyExit(stats, CloudSyncEarlyExit::kNotReady);
    return false;
  }
  if (access_token_.empty()) {
    if (err)
      *err = "尚未登录，无法同步。";
    out->message = "尚未登录，无法同步。";
    MarkEarlyExit(stats, CloudSyncEarlyExit::kNotLoggedIn);
    return false;
  }

  // ADR 0062 决策 1: 维护准入闸门 —— **整轮跳过**。
  //
  // 检查放在 LoadSidecar 之前 => 闸门关闭时**零 I/O、零配额消耗**：
  // 不读 sidecar、不 finalize、不导出 / 备份 / 回写、不发任何网络请求，
  // 也**不打断用户正在进行的输入**。
  {
    std::string gate_reason;
    if (!backend_->CheckMaintenanceGate(&gate_reason)) {
      const std::string reason = MaintenanceGateSkipMessage();
      if (err)
        *err = reason;
      out->message = reason;
      out->skipped = true;
      MarkEarlyExit(stats, CloudSyncEarlyExit::kMaintenanceGate);
      return false;
    }
  }

  std::string ignored;
  LoadSidecar(&ignored);

  // ★ spec 103 · BR-①g：解析本轮**生效**的灰度阶段。
  //   口径（FR-208）：外部环境变量优先、否则侧车值、否则默认逐条；
  //   环境变量生效 ⇒ 本轮结束时写回侧车 ⇒ 回滚 = 把变量设成 0 再跑一轮。
  {
    const char* env_raw = std::getenv(kBucketPhaseEnvVar);
    CloudBucketPhase resolved = CloudBucketPhase::kLegacy;
    bool from_env = false;
    ResolveBucketPhase(
        env_raw != nullptr ? std::string(env_raw) : std::string(),
        env_raw != nullptr, bucket_phase_, &resolved, &from_env);
    bucket_phase_ = resolved;
    bucket_phase_from_env_ = from_env;
  }
  // 本轮生效阶段写进诊断行（阶段 0 ⇒ 「bucket phase=0 …」）。
  last_bucket_stats_.phase = BucketPhaseValue(bucket_phase_);
  // 阶段 0 = 逐条端点单跑（**默认**）⇒ 与本批之前逐字等价（AC-210）。
  const bool bucket_mode = bucket_phase_ != CloudBucketPhase::kLegacy;
  // 阶段 3 / 4 = 切读 / 退役 ⇒ 逐条端点完全不发。
  const bool bucket_only = bucket_phase_ == CloudBucketPhase::kSwitchRead ||
                           bucket_phase_ == CloudBucketPhase::kRetired;
  // 逐条端点是否仍可用（阶段 0 / 1 / 2）。
  const bool legacy_endpoints = !bucket_only;

  // 维护模式：锁住用户库（否则 leveldb LOCK 失败）。
  std::string begin_err;
  if (!backend_->BeginMaintenance(&begin_err)) {
    // ADR 0062: 进入维护失败时再问一次闸门 —— 若失败是因为「此刻有活跃
    // 输入会话」，本轮算**跳过**（不是失败）：不累计失败次数、不进退避。
    std::string gate_reason;
    if (!backend_->CheckMaintenanceGate(&gate_reason)) {
      const std::string reason = MaintenanceGateSkipMessage();
      if (err)
        *err = reason;
      out->message = reason;
      out->skipped = true;
      MarkEarlyExit(stats, CloudSyncEarlyExit::kBeginMaintenanceGate);
      return false;
    }
    const std::string reason =
        begin_err.empty() ? "无法进入用户词典维护模式。" : begin_err;
    if (err)
      *err = reason;
    out->message = reason;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kBeginMaintenance);
    return false;
  }

  std::string text;
  std::string export_err;
  if (!backend_->ExportUserDict(&text, &export_err)) {
    backend_->EndMaintenance(&ignored);
    const std::string reason =
        export_err.empty() ? "导出用户词典失败。" : export_err;
    if (err)
      *err = reason;
    out->message = reason;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kExportUserDict);
    return false;
  }

  std::vector<UserDictSnapshot> snapshots;
  std::string parse_err;
  if (!ParseUserDictSnapshots(text, &snapshots, &parse_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = parse_err;
    out->message = parse_err;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kParseUserDict);
    return false;
  }

  // ★ ADR 0078 D2：**本地导出为空**时，本轮**不推送任何用户词典内容**
  //   （本地条目集为空 ⇒ 推送集合自然为空 ⇒ 空快照不会被推上云），
  //   但**不判本轮失败** —— 因为「本地为空 + 云端有内容」正是
  //   **新设备从云端还原**的正常路径（拉取侧必须能走通）。
  //   ⇒ 这里只做**事件驱动的诊断登记**（计数，不含条目明文）。
  const size_t local_data_rows = CountUserDictDataRows(text);
  if (local_data_rows == 0)
    diag::AppendLine("cloudsync", "userdict export_empty_local=1");

  // 多个词典的快照摊平成一组条目（item_key 里带词典名，避免跨词典撞键）。
  std::vector<CloudDictEntry> local_rows;
  for (size_t i = 0; i < snapshots.size(); ++i) {
    for (size_t k = 0; k < snapshots[i].rows.size(); ++k)
      local_rows.push_back(snapshots[i].rows[k]);
  }

  std::vector<Entry> local_entries;
  if (!DictRowsToEntries(local_rows, &local_entries, &parse_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = parse_err;
    out->message = parse_err;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kDictRowsToEntries);
    return false;
  }

  // ★ ADR 0076 D2-5a-1：**先拉远端，后分配 HLC**。
  //   HLC 的分配 / 复用挪到拉取之后（见下面「对账」段）：分配时必须**看得见
  //   远端**，否则侧车丢失后全部本地条目都被盖上新的墙钟 HLC ⇒ 服务端 HLC
  //   守卫全部落空 ⇒ 整库重写（本批要根治的写放大）。
  //
  // 1) 拉远端条目（按页分批；游标只增不减）。
  std::vector<Entry> remote_entries;
  int64_t pulled_seq = last_server_seq_;
  bool has_more = false;
  // 旧密钥加密、解不开的云端行数（换口令后的正常现象：只计数、不报错）
  int undecryptable = 0;
  // ★ 批次 BM / ADR 0071：云端**形态非法**（payload 无制表符）的条目数 ——
  //   这类条目会被跳过（不再毒化整轮），但必须**单独计数**并如实报给用户。
  int malformed = 0;
  // ★ 批次 BM / ADR 0071：本轮是否发生过「访问令牌自动续期」。
  bool token_refreshed = false;
  if (legacy_endpoints && (direction == SyncDirection::kPullOnly ||
                           direction == SyncDirection::kBoth)) {
    CloudSyncService service(transport_, access_token_);
    service.SetAccessTokenRefresher(refresher_);
    int64_t since = last_server_seq_;
    for (int page = 0; page < kMaxPullPages; ++page) {
      std::vector<CloudSyncItem> batch;
      bool page_more = false;
      int64_t next_seq = since;
      std::string pull_err;
      ++out->pull_requests;
      if (!service.Pull(since, kPullPageLimit, &batch, &next_seq, &pull_err,
                        &page_more)) {
        backend_->EndMaintenance(&ignored);
        const std::string reason = pull_err.empty() ? "拉取失败。" : pull_err;
        if (err)
          *err = reason;
        out->message = reason;
        MarkEarlyExit(stats, CloudSyncEarlyExit::kPullFailed);
        return false;
      }
      out->items_pulled += static_cast<int>(batch.size());
      std::vector<Entry> page_entries;
      // ★ 批次 BM：分页计数必须**累加**。OpenItemsTolerant 保留「单次调用把
      //   出参清零」的语义，若把同一个变量直接传进去，多页拉取会**逐页覆盖**
      //   前页计数 ⇒ 最终只报最后一页的条数（文案失真，且 undecryptable
      //   这个既有计数同样受害）。
      int page_undecryptable = 0;
      int page_malformed = 0;
      if (!OpenItemsTolerant(batch, enc_key_, &page_entries,
                             &page_undecryptable, &page_malformed,
                             &parse_err)) {
        backend_->EndMaintenance(&ignored);
        if (err)
          *err = parse_err;
        out->message = parse_err;
        MarkEarlyExit(stats, CloudSyncEarlyExit::kPullDecodeFailed);
        return false;
      }
      undecryptable += page_undecryptable;
      malformed += page_malformed;
      remote_entries.insert(remote_entries.end(), page_entries.begin(),
                            page_entries.end());
      pulled_seq = next_seq;
      has_more = page_more;
      if (!page_more)
        break;
      if (next_seq <= since) {
        // 游标没有前进 ⇒ 再拉也是同一批，停手（避免死循环刷配额）。
        break;
      }
      since = next_seq;
    }
    if (service.token_refreshed())
      token_refreshed = true;  // ★ 批次 BM：401 → 续期 → 重试真的发生过
  }

  // 1.5) ★ spec 103 · BR-①g：桶级拉取（阶段 ≥ 1）。
  //   · 桶端点与逐条端点**互不参照**（两套编号空间）；桶游标 bseq 只从
  //     **拉取响应**推进。
  //   · 桶内条目走**逐条端点同一条解密路径**（OpenItemsTolerant）⇒ 条目级
  //     可验性与「解不开只计数」的语义逐字不变（R7 单一真相源）。
  bool bucket_scan_complete = false;
  // ★ 规则 1 v2（relay-02of06 · 阶段 1 修复轮）：本轮拉到、但**远端视图不完整**
  //   的桶键集合。证据取自**桶空间**（与服务端整桶覆盖的风险方向一致：服务端
  //   按 bucket_id 先 DELETE 后 INSERT ⇒ 未列出的远端条目被静默删除）⇒ 与
  //   legacy 编号空间的 unproven_keys 互不参照、不可混用（ADR 0077 / §P-B-4）。
  std::set<std::string> incomplete_buckets;
  if (bucket_mode && (direction == SyncDirection::kPullOnly ||
                      direction == SyncDirection::kBoth)) {
    bool bucket_more = false;
    std::string bucket_err;
    if (!PullBucketsIntoRemote(snapshots.size(), &remote_entries, &bucket_more,
                               &last_bucket_stats_, &undecryptable, &malformed,
                               &incomplete_buckets, &bucket_err)) {
      backend_->EndMaintenance(&ignored);
      const std::string reason =
          bucket_err.empty() ? "拉取云端桶失败。" : bucket_err;
      if (err)
        *err = reason;
      out->message = reason;
      MarkEarlyExit(stats, CloudSyncEarlyExit::kBucketPullFailed);
      return false;
    }
    bucket_scan_complete = !bucket_more;
    // 阶段 3 / 4：逐条端点不参与 ⇒ 「还有更多」的判定改由桶端点给出。
    if (bucket_only)
      has_more = bucket_more;
  }

  // 2) ★ 对账并分配 / 复用 HLC（ADR 0076 D2-5a-2 / -3 / -5）。
  //
  //    先让本机 HLC 时钟**观测**本轮远端版本 ⇒ 之后分配的墙钟 HLC 必然新于
  //    远端（否则设备时钟超前 / 本机时钟回拨时，本地分歧条目会输给远端，
  //    R4「本地胜出」落空）。
  for (size_t i = 0; i < remote_entries.size(); ++i)
    HlcObserve(&clock_, remote_entries[i].hlc);

  //    ★ R2 闸门（ADR D2-5a-3 · 库存未证实不推送）：闸门成立 ⇒ 本轮**不得**
  //    推送「既无侧车记录、又不在本轮远端集合里」的本地条目（延后到后续轮次）。
  //    闸门 = **本轮拉取没有扫完整个云端库存**（has_more == true）。
  //    ADR 字面口径是「pull_start_cursor == 0（本轮从零做全量库存扫描）**且**
  //    has_more == true」—— 本实现取其**超集**（pull_start_cursor == 0
  //    是子情形： 第一轮从零扫描）。偏离理由见
  //    reports/fluxing-br-1c/impl-report.md 「设计歧义点 ①」：服务端单轮只吐
  //    1,000 条 ⇒ 大账号要**多轮**才扫完库存 （ADR D2-5a-8 ①
  //    已登记）；若闸门只在第一轮成立，第二轮就会把「本轮尚未
  //    见到」的条目全推上去 ⇒ 写放大原样复现 ⇒ D2-5a-7 的判据（今日写入增量
  //    = 0）不成立。侧车完好时（游标 > 0）本闸门只在「云端库存比游标多出 1,000
  //    条以上」时生效 ⇒ 新增本地词条被**延后**到扫描完成的那一轮才推送
  //    （不丢数据，只延后）。
  const bool inventory_unproven = has_more;
  if (stats != nullptr) {
    // scan_complete = 本轮拉取**扫完了整个云端库存**（只推不拉的轮次没有
    // 库存视图 ⇒ 记 0，不谎称扫完）。
    stats->scan_complete =
        !has_more && (direction == SyncDirection::kPullOnly ||
                      direction == SyncDirection::kBoth);
  }

  std::vector<bool> locally_modified(local_entries.size(), false);
  std::vector<bool> deferred(local_entries.size(), false);
  // ★ BR-①g 规则 1（**legacy 编号空间**）：记录被「库存未证实」压住的条目键。
  //   ⚠ relay-02of06 起，**桶路径不再消费本集合**：规则 1 v2 改用桶空间的
  //     incomplete_buckets（真实库存证据）。本集合保留 ⇒ legacy 路径的 R2
  //     延后语义与既有诊断口径不变；**不得**据此认为桶推受它保护。
  std::set<std::string> unproven_keys;
  // R1 命中（沿用了远端 HLC）的本地条目下标 ⇒ 侧车重建时要写进去（R3）。
  std::vector<bool> remote_matched(local_entries.size(), false);
  // ★ ADR 0076 D2-5a-9 ③：侧车里存的是**旧口径**（全载荷哈希）的那条 ⇒
  //   基线与语义指纹**不可比** ⇒ 这些条目本轮**不推送**（避免升级首轮一次性
  //   4 万条洪峰），但要**抬高本地 HLC**（本地胜出 ⇒ 防云端旧版本回灌覆盖
  //   本地），并在侧车重建时**就地重锚**为 s2: 口径（下一轮起恢复正常）。
  std::vector<bool> legacy_baseline(local_entries.size(), false);
  const uint64_t now_ms = UnixTimeMs();
  int matched = 0;
  int divergent = 0;
  int deferred_count = 0;
  int legacy_rows = 0;  // 旧口径基线条数
  int legacy_held = 0;  // 其中「判成本地改动、但本轮压住不推」的条数

  for (size_t i = 0; i < local_entries.size(); ++i) {
    Entry& entry = local_entries[i];
    // ★ ADR 0076 D2-5a-9 ①：一切「是否改过」的判定改用**语义指纹**。
    const std::string sem = SemanticFingerprint(entry.payload);
    const std::string sem_hash = SemanticHashHex(entry.payload);
    const CloudSidecarRow* known = FindSidecarRow(sidecar_rows_, entry.key);
    const size_t remote_index = FindEntryIndex(remote_entries, entry.key);
    const Entry* remote = remote_index == static_cast<size_t>(-1)
                              ? nullptr
                              : &remote_entries[remote_index];

    // 侧车基线（语义口径）：
    //   kNone   = 侧车没这条 ⇒ 本地新增；
    //   kMatch  = 侧车是 s2: 口径且与本地语义指纹一致 ⇒ 本地没改过；
    //   kDiff   = 侧车是 s2: 口径但语义指纹变了 ⇒ 本地改过；
    //   kLegacy = 侧车是**旧口径**（裸哈希）⇒ 与语义指纹不可比 ⇒ 未知。
    enum class Baseline { kNone, kMatch, kDiff, kLegacy };
    Baseline baseline = Baseline::kNone;
    if (known != nullptr) {
      if (!IsSemanticHash(known->payload_hash))
        baseline = Baseline::kLegacy;
      else if (known->payload_hash == sem_hash)
        baseline = Baseline::kMatch;
      else
        baseline = Baseline::kDiff;
    }
    if (baseline == Baseline::kLegacy) {
      legacy_baseline[i] = true;
      ++legacy_rows;
    }

    if (remote != nullptr && SemanticFingerprint(remote->payload) == sem) {
      // ★ R1（ADR D2-5a-2 + D2-5a-9 ①）：**语义**一致 ⇒ 本地这条就是云端
      //   那条的当前版本 ⇒ 沿用**远端 HLC**、不标记本地改动、**不进推送集合**
      //   ⇒ 该条目**零写入**（服务端 HLC 守卫必然命中：stored == R.hlc）。
      //   ★ 口径变化：明文**逐字节一致**只是**充分条件**；回写重算出来的
      //     d= / t= 差异**不是**用户改动 ⇒ 也算一致（否则每轮都判「本地
      //     改动」⇒ 每轮新墙钟 HLC ⇒ 服务端 HLC 守卫失效 ⇒ 写放大）。
      entry.hlc = remote->hlc;
      HlcObserve(&clock_, entry.hlc);
      remote_matched[i] = true;
      ++matched;
      continue;
    }

    if (remote != nullptr) {
      // ★ R4（ADR D2-5a-5 · 语义口径）：同 item_key 但语义不同 ⇒ 分歧。
      //   · 侧车基线与本地一致（kMatch）⇒ 本地没改过 ⇒ 远端胜出、不回推
      //     （与现行行为逐字一致：侧车在 ⇒ 知道谁更新）。
      //   · 其余（侧车无记录 / 本地改过 / 旧口径未知）⇒ 视作本地改动：分配
      //     新墙钟 HLC + 推送（与现行行为逐字一致）。已知代价见
      //     ADR D2-5a-8 ②。
      if (baseline == Baseline::kMatch) {
        entry.hlc.ms = known->ms;
        entry.hlc.counter = known->counter;
        entry.hlc.device = known->device;
        HlcObserve(&clock_, entry.hlc);
        continue;
      }
      entry.hlc = HlcTick(&clock_, now_ms);
      locally_modified[i] = true;
      ++divergent;
      if (baseline == Baseline::kLegacy)
        ++legacy_held;  // 旧口径 ⇒ 本轮压住不推（见 legacy_baseline）
      continue;
    }

    if (baseline == Baseline::kMatch) {
      // 侧车有记录且语义指纹未变 ⇒ 本地没改过 ⇒ 沿用旧 HLC。
      entry.hlc.ms = known->ms;
      entry.hlc.counter = known->counter;
      entry.hlc.device = known->device;
      HlcObserve(&clock_, entry.hlc);
      continue;
    }

    // 本地新增 / 本地改过（侧车无记录，或语义指纹变了）⇒ 分配新墙钟 HLC。
    entry.hlc = HlcTick(&clock_, now_ms);
    locally_modified[i] = true;
    if (baseline == Baseline::kLegacy)
      ++legacy_held;  // 旧口径 ⇒ 本轮压住不推（见 legacy_baseline）
    if (known == nullptr && inventory_unproven) {
      // ★ R2（ADR D2-5a-3）：既无侧车记录、又不在本轮远端集合里，且本轮
      //   拉取**没有扫完整个云端库存** ⇒ 库存未证实 ⇒ 本轮**不推送**
      //   （延后到库存扫完后的轮次）。
      deferred[i] = true;
      unproven_keys.insert(entry.key);
      ++deferred_count;
    }
  }
  if (stats != nullptr) {
    stats->matched = matched;
    stats->divergent = divergent;
    stats->deferred = deferred_count;
  }
  // ★ ADR 0076 D2-5a-9 ③ 观测：旧口径基线条数 / 其中本轮被压住不推的条数。
  //   （**无条件**打一行 ⇒ 判据可从诊断日志直接读出来，不靠推测。）
  diag::AppendLine("cloudsync",
                   "semantic legacy_rows=" + std::to_string(legacy_rows) +
                       " legacy_held=" + std::to_string(legacy_held));

  // 3) 合并本地与远端（逐条目 HLC 大者取胜）。
  const std::vector<Entry> merged =
      MergeLocalAndRemote(local_entries, remote_entries);

  // 3) 回写合并结果（远端带来的新词 / 更新过的词条）。
  //    ★ ADR 0076 D2-5a-9 ④：判定口径改成**语义指纹** —— 回写重算出来的
  //      d= / t= 差异**不是**用户改动 ⇒ 不能触发回写。否则整库回写会不断
  //      刷新本地载荷的派生字段 ⇒ 每轮都判「本地改动」⇒ 写放大（漂移源头）。
  bool changed = merged.size() != local_entries.size();
  if (!changed) {
    for (size_t i = 0; i < merged.size(); ++i) {
      const size_t index = FindEntryIndex(local_entries, merged[i].key);
      if (index == static_cast<size_t>(-1) ||
          SemanticFingerprint(local_entries[index].payload) !=
              SemanticFingerprint(merged[i].payload)) {
        changed = true;
        break;
      }
    }
  }
  if (changed) {
    std::vector<CloudDictEntry> merged_rows;
    if (!EntriesToDictRows(merged, &merged_rows, &parse_err)) {
      backend_->EndMaintenance(&ignored);
      if (err)
        *err = parse_err;
      out->message = parse_err;
      return false;
    }
    std::string tsv;
    if (!BuildUserDictSnapshots(merged_rows, &tsv, &parse_err)) {
      backend_->EndMaintenance(&ignored);
      if (err)
        *err = parse_err;
      out->message = parse_err;
      return false;
    }
    // ★ ADR 0078 D4：**空合并集不得覆盖本地词典**。
    //   合并集为空却「有变化」（例如远端全胜出而本地本轮读成空）时，写回
    //   会用一份空快照盖住本地 ⇒ 用户词典被清空。
    //   ⇒ 0 数据行 ⇒ 跳过回写（**不是**整轮失败：拉取与推送仍可继续）。
    const size_t tsv_data_rows = CountUserDictDataRows(tsv);
    if (tsv_data_rows == 0) {
      diag::AppendLine("cloudsync", "userdict import_empty_rejected=1");
    } else {
      int imported = 0;
      std::string import_err;
      if (!backend_->ImportUserDict(tsv, &imported, &import_err)) {
        backend_->EndMaintenance(&ignored);
        const std::string reason =
            import_err.empty() ? "回写用户词典失败。" : import_err;
        if (err)
          *err = reason;
        out->message = reason;
        return false;
      }
    }
  }

  // 4) 推送集合：只推「本地改动过、且合并后仍是本地版本」的条目。
  //    · 本地没改过 ⇒ 服务端已有的就是同一份，不推（T502：只带变化条目）；
  //    · 远端版本胜出 ⇒ 服务端已经握着它，不推（避免两端来回乒乓）；
  //    · 其余（本地改动过 / 远端没有这条）⇒ 必须推。
  std::vector<size_t> push_indices;  // 本轮要推送的**本地条目下标**
  std::vector<bool> pushed(local_entries.size(), false);  // 本轮真的推成功的
  // 本轮被服务端跳过的（云端已有更新版本）—— 不是失败，但也不能算「推送」。
  std::vector<bool> skipped_flags(local_entries.size(), false);
  if (legacy_endpoints && (direction == SyncDirection::kPushOnly ||
                           direction == SyncDirection::kBoth)) {
    for (size_t i = 0; i < local_entries.size(); ++i) {
      if (!locally_modified[i])
        continue;
      // ★ R2（ADR D2-5a-3）：库存未证实的条目（deferred）本轮**不进推送集合**
      //   ⇒ 既不写放大，也不丢数据（延后到库存扫完后的轮次再推）。
      if (deferred[i])
        continue;
      // ★ ADR 0076 D2-5a-9 ③：侧车基线是**旧口径**（全载荷哈希）⇒ 与语义
      //   指纹不可比 ⇒ 本轮**压住不推**（升级首轮不产生 4 万条洪峰）。
      //   本地 HLC 已抬高（本地胜出）⇒ 云端旧版本不会回灌覆盖本地。
      if (legacy_baseline[i])
        continue;
      const size_t remote_index =
          FindEntryIndex(remote_entries, local_entries[i].key);
      if (remote_index != static_cast<size_t>(-1) &&
          RemoteWins(remote_entries[remote_index], local_entries[i])) {
        continue;  // 远端胜出 ⇒ 服务端已有，不回推
      }
      push_indices.push_back(i);
    }
  }

  // 5) 推送（分批：单批 kPushBatchSize 条）。
  bool push_ok = true;
  std::string push_err;
  if (!push_indices.empty()) {
    CloudSyncService service(transport_, access_token_);
    service.SetAccessTokenRefresher(refresher_);
    for (size_t offset = 0; offset < push_indices.size();
         offset += kPushBatchSize) {
      const size_t rest = push_indices.size() - offset;
      const size_t count = rest < kPushBatchSize ? rest : kPushBatchSize;
      std::vector<Entry> chunk;
      chunk.reserve(count);
      for (size_t k = offset; k < offset + count; ++k)
        chunk.push_back(local_entries[push_indices[k]]);
      std::vector<CloudSyncItem> sealed;
      std::string seal_err;
      if (!SealEntries(chunk, ItemType::kUserDb, enc_key_, &sealed,
                       &seal_err)) {
        push_ok = false;
        push_err = seal_err;
        break;
      }
      int64_t pushed_seq = last_server_seq_;
      ++out->push_requests;
      std::vector<std::string> accepted_keys;
      std::vector<std::string> skipped_keys;
      // ★ ADR 0074：每推一批都落一条**无条件**诊断行（条目数 + 耗时 +
      //   HTTP 状态）—— 12002（WinHTTP 超时）表现为「整批没回音」，没有这行
      //   就分不清「没到服务端」与「到了但慢到超时」。失败也照打。
      const unsigned long long batch_t0 = ::GetTickCount64();
      int batch_http_status = 0;
      const bool batch_ok =
          service.Push(sealed, &pushed_seq, &push_err, &accepted_keys,
                       &skipped_keys, &batch_http_status);
      const unsigned long long batch_elapsed_ms = ::GetTickCount64() - batch_t0;
      {
        std::string diag_line =
            "push batch items=" + std::to_string(sealed.size()) +
            " elapsed_ms=" + std::to_string(batch_elapsed_ms) + " http=";
        if (batch_http_status > 0)
          diag_line += std::to_string(batch_http_status);
        else
          diag_line += "failed";
        if (!batch_ok) {
          diag_line += " err=";
          diag_line += TruncateUtf8(push_err, 80);
        }
        diag::AppendLine("cloudsync", diag_line);
      }
      if (!batch_ok) {
        push_ok = false;
        break;
      }
      // ★ 只有服务端**真的收下**的条目（accepted）才算「已推送」；
      //   skipped = 云端已有更新版本（HLC 守卫）⇒ 不计入推送数，但也不算失败。
      // ★ 服务端回传的是**线上身份**（AC-07 的 MAC 值），本地必须换算后再比，
      //   否则每一条都落进 kUnknown ⇒ 永不标记已同步 ⇒ 无限重推。
      for (size_t k = offset; k < offset + count; ++k) {
        const std::string& key = local_entries[push_indices[k]].key;
        const std::string key_id = ComputeItemKeyId(enc_key_, key);
        switch (ClassifyPushResult(accepted_keys, skipped_keys, key_id)) {
          case PushDisposition::kAccepted:
            ++out->items_pushed;
            pushed[push_indices[k]] = true;  // 服务端收下了 ⇒ 已同步
            break;
          case PushDisposition::kSkipped:
            ++out->items_skipped;
            skipped_flags[push_indices[k]] =
                true;  // 云端版本更新 ⇒ 本地不必再推
            break;
          case PushDisposition::kUnknown:
            break;  // 服务端没提这条 ⇒ 不算推送成功，下一轮重试
        }
      }
      if (pushed_seq > last_server_seq_)
        last_server_seq_ = pushed_seq;
    }
    if (service.token_refreshed())
      token_refreshed = true;  // ★ 批次 BM：401 → 续期 → 重试真的发生过
  }

  // 5.5) ★ spec 103 · BR-①g：桶级推送（阶段 ≥ 1）。
  //   · **库存扫描未完成 ⇒ 本轮不推桶**（整桶覆盖闸门）：桶内容 = 合并视图
  //     的整桶，视图不完整时推上去会覆盖掉没看到的远端条目。
  //   · 只推**指纹变化**的桶 ⇒ 稳态零请求、零写入（AC-206）。
  if (bucket_mode &&
      (direction == SyncDirection::kPushOnly ||
       direction == SyncDirection::kBoth) &&
      bucket_scan_complete) {
    std::string bucket_err;
    if (!PushChangedBuckets(merged, remote_entries, incomplete_buckets,
                            bucket_scan_complete, legacy_endpoints,
                            &last_bucket_stats_, out, &bucket_err)) {
      backend_->EndMaintenance(&ignored);
      const std::string reason =
          bucket_err.empty() ? "推送云端桶失败。" : bucket_err;
      if (err)
        *err = reason;
      out->message = reason;
      return false;
    }
  }

  // 6) 更新 sidecar：只把「已同步」的条目标记成已同步。
  //    没推成功的条目保留旧行 ⇒ 下一轮仍被识别为「本地改动」并重推。
  std::vector<CloudSidecarRow> rows;
  for (size_t i = 0; i < merged.size(); ++i) {
    const Entry& entry = merged[i];
    const size_t local_index = FindEntryIndex(local_entries, entry.key);
    const size_t remote_index = FindEntryIndex(remote_entries, entry.key);
    bool synced = false;
    if (remote_index != static_cast<size_t>(-1) &&
        (local_index == static_cast<size_t>(-1) ||
         RemoteWins(remote_entries[remote_index],
                    local_entries[local_index]))) {
      synced = true;  // 远端胜出 ⇒ 服务端已经握着它
    }
    if (!synced && local_index != static_cast<size_t>(-1) &&
        remote_matched[local_index]) {
      // ★ R3（ADR D2-5a-4）：R1 命中条目（本地明文与远端**逐字节一致**）⇒
      //   云端握着的版本就是本地这条 ⇒ 记为「已同步」，侧车写入**远端 HLC**
      //   （merged 条目的 HLC 此时已等于远端 HLC）。
      //   侧车丢失后重建靠这一条 ⇒ 否则这些条目下一轮又会被当成「本地新增」。
      synced = true;
    }
    if (!synced && local_index != static_cast<size_t>(-1) &&
        (pushed[local_index] || skipped_flags[local_index])) {
      // 本地改动过、且合并后仍是本地版本、且**本轮真的推成功了** ⇒ 已同步。
      // skipped 也算「已同步」：云端握着的版本 ≥ 本地这条 ⇒ 本地不必再推；
      // 下一轮拉取会把云端版本带回来覆盖本地，并按云端版本重记 sidecar。
      // ★ 不能只看 locally_modified：只拉不推的轮次（启动轮）里本地改动
      //   从未上传，若此时就标记成「已同步」，这条改动永远再也推不上去。
      synced = true;
    }
    if (synced) {
      CloudSidecarRow row;
      row.item_key = entry.key;
      row.ms = entry.hlc.ms;
      row.counter = entry.hlc.counter;
      row.device = entry.hlc.device;
      // ★ ADR 0076 D2-5a-9 ②：侧车哈希字段改存**语义指纹**哈希（s2: 前缀）。
      row.payload_hash = SemanticHashHex(entry.payload);
      rows.push_back(row);
    } else {
      const CloudSidecarRow* old = FindSidecarRow(sidecar_rows_, entry.key);
      if (old != nullptr) {
        if (IsSemanticHash(old->payload_hash)) {
          rows.push_back(*old);
        } else {
          // ★ ADR 0076 D2-5a-9 ③：旧口径行（裸哈希）与语义指纹**不可比**
          //   ⇒ 不能继续沿用（否则每轮都判「本地改动」⇒ 每轮新墙钟 HLC ⇒
          //   服务端 HLC 守卫失效 ⇒ 既有行被反复 UPDATE = 写放大）。
          //   这里**就地重锚**：写语义指纹哈希 + 当前条目的 HLC（本地已抬高
          //   ⇒ 本地胜出）⇒ 下一轮起走正常语义口径。
          CloudSidecarRow row;
          row.item_key = entry.key;
          row.ms = entry.hlc.ms;
          row.counter = entry.hlc.counter;
          row.device = entry.hlc.device;
          row.payload_hash = SemanticHashHex(entry.payload);
          rows.push_back(row);
        }
      }
    }
  }

  // 7) 落盘 sidecar（含游标）。
  if (pulled_seq > last_server_seq_)
    last_server_seq_ = pulled_seq;
  sidecar_rows_ = rows;
  std::string save_err;
  if (!SaveSidecar(&save_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = save_err;
    out->message = save_err;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kSidecarSaveFailed);
    return false;
  }

  std::string end_err;
  if (!backend_->EndMaintenance(&end_err)) {
    const std::string reason =
        end_err.empty() ? "退出用户词典维护模式失败。" : end_err;
    if (err)
      *err = reason;
    out->message = reason;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kEndMaintenanceFailed);
    return false;
  }

  if (!push_ok) {
    const std::string reason = push_err.empty() ? "推送失败。" : push_err;
    if (err)
      *err = reason;
    out->message = reason;
    MarkEarlyExit(stats, CloudSyncEarlyExit::kPushFailed);
    return false;
  }

  out->ok = true;
  out->has_more = has_more;
  std::string message;
  // ★ 批次 BN / ADR 0072：本轮的**可选提示**（结构化）。
  //    每条 notice 的文案与 message 里对应片段**逐字同源**（同一个字符串
  //    变量）⇒ 单测可断言「每条 notice 都逐字出现在 message 中」。
  //    ⚠️ message 的**逐字输出不变**（既有探针/日志期望值不受影响）。
  std::vector<std::string> notices;
  if (key_rotated_) {
    // 换口令 ⇒ enc_key 变 ⇒ 旧 sidecar 的「已同步」标记全部作废。
    const std::string text = "加密密钥已变化，已重置本地同步状态并全量重推";
    message += text;
    message += "。";
    notices.push_back(text);
  }
  message += "同步完成：推送 ";
  message += std::to_string(out->items_pushed);
  message += " 条，拉取 ";
  message += std::to_string(out->items_pulled);
  message += " 条";
  if (out->items_skipped > 0) {
    const std::string count = std::to_string(out->items_skipped);
    message += "，云端已有更新版本跳过 ";
    message += count;
    message += " 条";
    notices.push_back("云端已有更新版本跳过 " + count + " 条");
  }
  if (undecryptable > 0) {
    const std::string count = std::to_string(undecryptable);
    message += "，旧密钥加密的行无法解密跳过 ";
    message += count;
    message += " 条";
    notices.push_back("旧密钥加密的行无法解密跳过 " + count + " 条");
  }
  if (malformed > 0) {
    // ★ 批次 BM / ADR 0071：形态非法的云端条目**已跳过**，不再毒化整轮。
    const std::string count = std::to_string(malformed);
    message += "，云端有 ";
    message += count;
    message += " 条格式非法的条目已跳过";
    notices.push_back("云端有 " + count + " 条格式非法的条目已跳过");
  }
  if (token_refreshed) {
    // ★ 批次 BM / ADR 0071：把「401 → 续期 → 重试」变成**人眼可见的直接
    //   证据**（此前只有日志里的强推断）。
    message += "，访问令牌已自动续期";
    notices.push_back("访问令牌已自动续期");
  }
  if (has_more) {
    message += "（云端还有更多条目，下次同步继续）";
    notices.push_back("云端还有更多条目，下次同步继续");
  }
  message += "。";
  out->message = message;
  out->notices = notices;
  return true;
}

// ---------------------------------------------------------------------------
// ★ spec 103 · BR-①g：桶级编排（拉取 / 分桶 / 指纹 / 推送）
// ---------------------------------------------------------------------------
//
// ★ 三条不可破的边界：
//   1) 桶 blob = **既有逐条信封块直接串接** ⇒ 条目级 AAD / 可验性逐字不变
//      （I2 / I4），桶内条目走**逐条端点同一条**解密路径（OpenItemsTolerant）。
//   2) 桶内容 = **合并后本地视图的全集**按 (词典名, 分片) 分组（FR-206）
//      ⇒ 只推变化桶，服务端整桶覆盖也不会丢同桶其它条目。
//   3) 桶游标 bseq 与逐条游标 seq **互不参照**（AC-211）：bseq 只从**拉取
//      响应**推进，绝不归零 seq，也绝不从推送响应推进。

namespace {

// 桶内容指纹（语义口径，与逐条侧车**同源** ⇒ 只改 d= / t= 不变、只改 c= 变）。
std::string ComputeBucketContentFingerprint(const std::vector<Entry>& entries) {
  std::vector<CloudBucketFingerprintItem> items;
  items.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    CloudBucketFingerprintItem item;
    item.item_key = entries[i].key;
    item.payload_hash = SemanticHashHex(entries[i].payload);
    item.hlc = entries[i].hlc;
    items.push_back(item);
  }
  return BucketFingerprint(items);
}

// 字符串数组里是否含某个值（accepted / skipped 的判定）。
bool ContainsString(const std::vector<std::string>& list,
                    const std::string& value) {
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i] == value)
      return true;
  }
  return false;
}

// 桶行指纹的比较口径：降级桶的指纹带 "d:" 前缀（已改走逐条端点）⇒ 比较时
//   剥掉前缀，避免每轮重复降级。
std::string BucketFingerprintBase(const std::string& stored) {
  if (stored.size() > 2 && stored[0] == 'd' && stored[1] == ':')
    return stored.substr(2);
  return stored;
}

// ★ 阶段 2 修复轮（缺陷 ①-1）：门禁用例的退避压缩开关。
//   -1 = 用生产退避（BucketPullRetryDelayMs）；>= 0 = 固定等待该毫秒数。
//   只改「等多久」，不改「重不重试 / 什么状态码重试」。
int g_bucket_pull_retry_delay_override_ms = -1;

}  // namespace

void SetBucketPullRetryDelayOverrideMsForTest(int delay_ms) {
  g_bucket_pull_retry_delay_override_ms = delay_ms;
}

bool CloudSyncCoordinator::PullBucketsIntoRemote(
    size_t dict_count,
    std::vector<Entry>* remote_entries,
    bool* has_more,
    CloudBucketRoundStats* stats,
    int* undecryptable,
    int* malformed,
    std::set<std::string>* incomplete_buckets,
    std::string* err) {
  if (remote_entries == nullptr || stats == nullptr) {
    if (err)
      *err = "内部错误：桶拉取输出为空。";
    return false;
  }
  CloudSyncService service(transport_, access_token_);
  service.SetAccessTokenRefresher(refresher_);

  // ★ 切读阶段（3 / 4）必须**扫完整库存**：FR-215 的「云端视图缺失」判定
  //   只有在全量视图上才成立 ⇒ 首屏 since = 0；其余阶段用 bseq 增量拉取。
  const bool full_scan = (bucket_phase_ == CloudBucketPhase::kSwitchRead ||
                          bucket_phase_ == CloudBucketPhase::kRetired);
  int64_t since = full_scan ? 0 : bucket_seq_;
  std::string cursor;
  bool more = false;
  const int max_pages = BucketPullMaxPages(dict_count);
  int pages = 0;
  while (pages < max_pages) {
    std::vector<CloudBucketPullItem> page;
    bool page_more = false;
    int64_t next_seq = since;
    std::string next_cursor;
    std::string pull_err;
    ++pages;
    // ★ F3（relay-02of06）：桶端点的 HTTP 状态此前**不可观测** ⇒ 503 / 401
    //   这类失败无法从诊断日志区分。失败页也打状态行（传输层失败 ⇒ http=0）。
    //
    // ★ 阶段 2 修复轮（缺陷 ①-1 · §4.4）：同一 page 的**有限重试**。
    //   旧行为 = 一次失败就 `return false` ⇒ 单次 5xx 让整轮作废。
    //   新行为 = 5xx / 传输层失败在同 page 重试 ≤ kBucketPullMaxRetries 次
    //   （退避 1 s / 3 s）；4xx 与
    //   2xx+解析失败**不重试**（BucketPullRetryable）。 重试一律沿用**同一**
    //   since / cursor（游标不推进，避免跳页丢桶）。 重试用尽 ⇒ 仍走既有
    //   kBucketPullFailed 早退，诊断行带累计 `retry=<n>`。
    int page_http_status = 0;
    int page_retries = 0;
    bool page_ok = false;
    for (;;) {
      // 每次尝试前复位 ⇒ 失败尝试的残留输出不会污染重试结果。
      page.clear();
      page_more = false;
      next_seq = since;
      next_cursor.clear();
      pull_err.clear();
      page_http_status = 0;
      ++stats->pull_requests;  // 判据①按**HTTP 尝试**取证，不按页取证
      page_ok = service.PullBuckets(since, kBucketPullPageLimit, cursor, &page,
                                    &next_seq, &pull_err, &page_more,
                                    &next_cursor, &page_http_status);
      if (page_ok)
        break;
      if (page_retries >= kBucketPullMaxRetries)
        break;  // 重试预算用尽
      if (!BucketPullRetryable(page_http_status))
        break;  // 4xx / 2xx+解析失败 ⇒ 不重试（保持原有直接早退）
      // 记一次失败尝试（http 状态逐条留证），再退避重试同页。
      diag::AppendLine("cloudsync",
                       FormatBucketPullDiagLine(pages, page_http_status, 0, 0,
                                                false, page_retries));
      const int delay_ms = g_bucket_pull_retry_delay_override_ms >= 0
                               ? g_bucket_pull_retry_delay_override_ms
                               : BucketPullRetryDelayMs(page_retries);
      ++page_retries;
      if (delay_ms > 0)
        ::Sleep(static_cast<DWORD>(delay_ms));
    }
    if (!page_ok) {
      // 只有**真发生过重试**才追加 `
      // retry=<n>`（未重试的失败行与旧日志逐字一致）。
      diag::AppendLine("cloudsync", FormatBucketPullDiagLine(
                                        pages, page_http_status, 0, 0, false,
                                        page_retries > 0 ? page_retries : -1));
      if (err)
        *err = pull_err.empty() ? "拉取云端桶失败。" : pull_err;
      return false;
    }
    int page_items = 0;
    for (size_t i = 0; i < page.size(); ++i) {
      std::vector<Envelope> envelopes;
      std::string unpack_err;
      std::vector<uint8_t> blob_bytes;
      if (!Base64Decode(page[i].cipher_b64, &blob_bytes)) {
        // base64 畸形 ⇒ 跳过该桶（**不毒化整轮**），单独计数。
        if (malformed != nullptr)
          ++(*malformed);
        // ★ 规则 1 v2：该桶的远端视图**读不全** ⇒ 本轮整桶不推。
        if (incomplete_buckets != nullptr)
          incomplete_buckets->insert(page[i].bucket_key);
        continue;
      }
      const std::string blob =
          blob_bytes.empty()
              ? std::string()
              : std::string(reinterpret_cast<const char*>(blob_bytes.data()),
                            blob_bytes.size());
      if (!UnpackBucketBlob(blob, &envelopes, &unpack_err)) {
        // 单桶畸形 ⇒ 跳过该桶（**不毒化整轮**），单独计数。
        if (malformed != nullptr)
          ++(*malformed);
        if (incomplete_buckets != nullptr)
          incomplete_buckets->insert(page[i].bucket_key);
        continue;
      }
      std::vector<CloudSyncItem> items;
      bool item_bad = false;
      for (size_t k = 0; k < envelopes.size(); ++k) {
        CloudSyncItem item;
        if (!CloudSyncItemFromEnvelope(envelopes[k], &item, &unpack_err)) {
          item_bad = true;
          break;
        }
        items.push_back(item);
      }
      if (item_bad) {
        if (malformed != nullptr)
          ++(*malformed);
        if (incomplete_buckets != nullptr)
          incomplete_buckets->insert(page[i].bucket_key);
        continue;
      }
      std::vector<Entry> entries;
      int page_undecryptable = 0;
      int page_malformed = 0;
      // ★ R7：桶内条目走**同一条**解密路径（OpenItemsTolerant）。
      if (!OpenItemsTolerant(items, enc_key_, &entries, &page_undecryptable,
                             &page_malformed, &unpack_err)) {
        if (err)
          *err = unpack_err;
        return false;
      }
      // ★ 规则 1 v2：该桶的远端视图是否**完整** = 解出条数 == 服务端
      // item_count，
      //   且桶内没有解不开 / 畸形条目。视图不完整 ⇒ 本轮整桶不推（服务端按
      //   bucket_id 先 DELETE 后 INSERT ⇒ 推上去会静默删掉没读到的远端条目）。
      if (page[i].item_count > 0 && entries.size() != page[i].item_count) {
        if (incomplete_buckets != nullptr)
          incomplete_buckets->insert(page[i].bucket_key);
      }
      if (page_undecryptable > 0 || page_malformed > 0) {
        if (incomplete_buckets != nullptr)
          incomplete_buckets->insert(page[i].bucket_key);
      }
      if (undecryptable != nullptr)
        *undecryptable += page_undecryptable;
      if (malformed != nullptr)
        *malformed += page_malformed;
      stats->pulled_buckets += 1;
      stats->pulled_items += static_cast<int>(entries.size());
      page_items += static_cast<int>(entries.size());
      remote_entries->insert(remote_entries->end(), entries.begin(),
                             entries.end());
    }
    // ★ F3：本页成功 ⇒ 打一条含 HTTP 状态的桶拉取诊断行（无条件打印）。
    //   重试过的页追加 ` retry=<n>`（未重试的页与旧日志逐字一致）。
    diag::AppendLine(
        "cloudsync",
        FormatBucketPullDiagLine(
            pages, page_http_status, static_cast<int>(page.size()), page_items,
            page_more, page_retries > 0 ? page_retries : -1));
    more = page_more;
    if (next_seq > since)
      since = next_seq;
    if (!page_more)
      break;
    if (next_cursor.empty())
      break;  // 没有续读位置 ⇒ 停手（避免死循环刷配额）
    cursor = next_cursor;
  }
  if (has_more != nullptr)
    *has_more = more;
  // ★ bseq 只从**拉取响应**推进（推送返回的 server_seq 不推进它）。
  if (since > bucket_seq_)
    bucket_seq_ = since;
  return true;
}

bool CloudSyncCoordinator::PushChangedBuckets(
    const std::vector<Entry>& merged,
    const std::vector<Entry>& remote_entries,
    const std::set<std::string>& incomplete_buckets,
    bool remote_complete,
    bool legacy_endpoint_active,
    CloudBucketRoundStats* stats,
    CloudSyncRoundResult* result,
    std::string* err) {
  if (stats == nullptr) {
    if (err)
      *err = "内部错误：桶统计输出为空。";
    return false;
  }
  std::vector<uint8_t> bucket_mac_key;
  // ★ BR-①g 规则 1 v2（relay-02of06 · 阶段 1 修复轮）：闸门改用**桶空间的真实
  //   库存证据**（incomplete_buckets），**不再**使用 legacy 空间的
  //   unproven_keys。 旧口径恒不成立：inventory_complete 直接取自
  //   bucket_scan_complete，而调用方 只在 bucket_scan_complete 为真时才进本函数
  //   ⇒ `!inventory_complete` 恒假 ⇒ 规则 1 从未生效（真机 held_unproven=0
  //   实证；handoff §P-B-2）。 remote_complete
  //   仍保留：FR-215「云端视图缺失」判定需要它（切读阶段）。
  // ★ BR-①g 规则 2：每轮桶推行数配额（估算写入行）。
  const size_t quota_rows = kBucketPushRowsPerRound;
  size_t held_unproven = 0, held_quota = 0, rows_pushed = 0, rows_held = 0;
  if (!DeriveBucketMacKey(enc_key_, &bucket_mac_key, err))
    return false;

  // 1) 分桶：桶键 = (词典名, 分片) ⇒ 改一条词条只重写 1 个桶（AC-201）。
  std::map<std::string, std::vector<Entry> > groups;
  for (size_t i = 0; i < merged.size(); ++i) {
    const Entry& entry = merged[i];
    std::string dict_name;
    std::string phrase;
    if (!SplitDictItemKey(entry.key, &dict_name, &phrase))
      continue;  // 形态非法 ⇒ 不进桶（写回阶段另有闸门）
    std::string bucket_key;
    std::string key_err;
    if (!ComputeBucketKeyForItem(bucket_mac_key, dict_name, entry.key,
                                 &bucket_key, nullptr, nullptr, &key_err))
      continue;
    groups[bucket_key].push_back(entry);
  }

  // 2) 侧车里的桶行索引（bucket_key → 行）。
  std::map<std::string, const CloudSidecarBucketRow*> brow_index;
  for (size_t i = 0; i < bucket_rows_.size(); ++i)
    brow_index[bucket_rows_[i].bucket_key] = &bucket_rows_[i];

  // 3) FR-215：切读阶段检出「本地已同步但云端视图缺失」的条目 ⇒ 强制重推其桶。
  //    该判定**仅在库存扫描完整时**生效（扫描不完整 ⇒ 云端视图不可信）。
  std::set<std::string> missing_keys;
  if (remote_complete && (bucket_phase_ == CloudBucketPhase::kSwitchRead ||
                          bucket_phase_ == CloudBucketPhase::kRetired)) {
    std::set<std::string> remote_keys;
    for (size_t i = 0; i < remote_entries.size(); ++i)
      remote_keys.insert(remote_entries[i].key);
    for (size_t i = 0; i < sidecar_rows_.size(); ++i) {
      if (remote_keys.find(sidecar_rows_[i].item_key) == remote_keys.end())
        missing_keys.insert(sidecar_rows_[i].item_key);
    }
  }

  // 4) 逐桶判定：指纹变化（或强制重推）才密封 ⇒ 稳态零 AES、零请求（AC-206）。
  std::vector<CloudBucketPushItem> push_items;
  std::vector<std::string> push_fingerprints;
  std::vector<Entry> degraded_entries;
  std::map<std::string, std::string> degraded_fingerprints;
  for (std::map<std::string, std::vector<Entry> >::const_iterator it =
           groups.begin();
       it != groups.end(); ++it) {
    const std::string& bucket_key = it->first;
    const std::vector<Entry>& entries = it->second;
    const std::string fingerprint = ComputeBucketContentFingerprint(entries);
    std::map<std::string, const CloudSidecarBucketRow*>::const_iterator brow =
        brow_index.find(bucket_key);
    const bool known = (brow != brow_index.end());
    bool changed = !known;
    if (known)
      changed = BucketFingerprintBase(brow->second->fingerprint) != fingerprint;
    if (!changed && !missing_keys.empty()) {
      for (size_t i = 0; i < entries.size(); ++i) {
        if (missing_keys.find(entries[i].key) != missing_keys.end()) {
          changed = true;
          break;
        }
      }
    }
    if (!changed)
      continue;
    // 超限降级（FR-214）：条目数或字节数超限 ⇒ 改走逐条端点推该桶条目，
    //   且**不得**因此使整批失败。
    if (entries.size() > kBucketMaxItems) {
      degraded_entries.insert(degraded_entries.end(), entries.begin(),
                              entries.end());
      degraded_fingerprints[bucket_key] = fingerprint;
      ++stats->degraded;
      continue;
    }
    std::vector<Envelope> envelopes;
    envelopes.reserve(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      Envelope env;
      std::string seal_err;
      if (!SealEntryEnvelope(entries[i], ItemType::kUserDb, enc_key_, &env,
                             &seal_err)) {
        if (err)
          *err = seal_err;
        return false;
      }
      envelopes.push_back(env);
    }
    const std::string blob = PackBucketBlob(envelopes);
    if ((!envelopes.empty() && blob.empty()) ||
        blob.size() > kBucketMaxBlobBytes) {
      degraded_entries.insert(degraded_entries.end(), entries.begin(),
                              entries.end());
      degraded_fingerprints[bucket_key] = fingerprint;
      ++stats->degraded;
      continue;
    }
    // 桶级时序 = 桶内条目时序的最大值；**必须严格大于**已存值，否则服务端的
    //   整桶守卫会把一次真实变更整桶跳过（AC-205）。
    // ★ BR-①g 规则 1 v2：桶的**远端视图不完整** ⇒ 整桶本轮不推。
    // 服务端按 bucket_id 先 DELETE 后 INSERT，未列出的远端条目会被静默删除，
    // 因此宁可本轮零写入，也不允许「半库存」覆盖整桶。被压的桶不写 brow，
    // 指纹保持旧值 ⇒ 下一轮自动续推（与既有「未推桶保留旧指纹」同一机制）。
    //   证据 = 本轮拉到该桶、但解出条数 != 服务端 item_count，或桶内出现畸形 /
    //   解不开条目（由 PullBucketsIntoRemote 采集 ⇒ incomplete_buckets）。
    if (BucketRemoteViewIncomplete(incomplete_buckets, bucket_key)) {
      ++held_unproven;
      continue;
    }
    Hlc bucket_hlc = AggregateBucketHlc(envelopes);
    if (known) {
      Hlc stored;
      stored.ms = brow->second->ms;
      stored.counter = static_cast<uint16_t>(brow->second->counter);
      stored.device = brow->second->device;
      if (CompareHlc(bucket_hlc, stored) <= 0)
        bucket_hlc = MakeHlcStrictlyGreater(stored);
    }
    CloudBucketPushItem item;
    item.bucket_key = bucket_key;
    item.item_count = envelopes.size();
    item.cipher_b64 = Base64Encode(blob);
    item.hlc = bucket_hlc;
    push_items.push_back(item);
    push_fingerprints.push_back(fingerprint);
  }

  // 5) 分批推送（单请求 ≤ 64 桶 = 服务端 MAX_PUSH_BUCKETS）。
  // ★ BR-①g 规则 2：每轮桶推行数配额。按声明序累计估算写入行，超出配额的
  // 桶本轮不推；被压的桶同样不写 brow ⇒ 下一轮自动续推，无需另造水位。
  std::vector<size_t> bucket_write_rows;
  size_t bucket_rows_total = 0;
  for (size_t i = 0; i < push_items.size(); ++i) {
    const size_t est = EstimateBucketWriteRows(1, false);
    bucket_write_rows.push_back(est);
    bucket_rows_total += est;
  }
  size_t quota_rows_used = 0;
  std::vector<size_t> quota_slots =
      SelectBucketsWithinQuota(bucket_write_rows, quota_rows, &quota_rows_used);
  rows_pushed = quota_rows_used;
  rows_held = bucket_rows_total - quota_rows_used;
  if (quota_slots.size() < push_items.size()) {
    held_quota = push_items.size() - quota_slots.size();
    std::vector<CloudBucketPushItem> quota_items;
    std::vector<std::string> quota_fingerprints;
    for (size_t i = 0; i < quota_slots.size(); ++i) {
      quota_items.push_back(push_items[quota_slots[i]]);
      quota_fingerprints.push_back(push_fingerprints[quota_slots[i]]);
    }
    push_items.swap(quota_items);
    push_fingerprints.swap(quota_fingerprints);
  }
  std::vector<CloudSidecarBucketRow> new_brow;
  bool push_ok = true;
  std::string push_err;
  if (!push_items.empty()) {
    CloudSyncService service(transport_, access_token_);
    service.SetAccessTokenRefresher(refresher_);
    int push_page = 0;
    for (size_t offset = 0; offset < push_items.size();
         offset += kBucketPushBatch) {
      ++push_page;
      const size_t rest = push_items.size() - offset;
      const size_t count = rest < kBucketPushBatch ? rest : kBucketPushBatch;
      std::vector<CloudBucketPushItem> chunk(
          push_items.begin() + offset, push_items.begin() + offset + count);
      int64_t pushed_seq = bucket_seq_;
      int http_status = 0;
      std::vector<std::string> accepted_keys;
      std::vector<std::string> skipped_keys;
      ++stats->push_requests;
      const bool ok =
          service.PushBuckets(chunk, &pushed_seq, &push_err, &accepted_keys,
                              &skipped_keys, &http_status);
      // ★ F3（relay-02of06 · 阶段 1 修复轮）：本页桶推诊断行（含 HTTP 状态）。
      //   无条件打印 ⇒ 失败页也留痕；传输层失败 ⇒ http_status = 0（与
      //   PushBuckets 的既有口径逐字一致）。前缀 `bucket push ` 与既有的
      //   `bucket phase=` / `bucket quota ` / `bucket pull ` 均不冲突。
      diag::AppendLine("cloudsync", FormatBucketPushDiagLine(
                                        push_page, http_status,
                                        static_cast<int>(chunk.size()),
                                        static_cast<int>(accepted_keys.size()),
                                        static_cast<int>(skipped_keys.size())));
      if (!ok) {
        push_ok = false;
        break;
      }
      for (size_t k = offset; k < offset + count; ++k) {
        const CloudBucketPushItem& item = push_items[k];
        CloudSidecarBucketRow row;
        row.bucket_key = item.bucket_key;
        row.ms = item.hlc.ms;
        row.counter = item.hlc.counter;
        row.device = item.hlc.device;
        row.item_count = item.item_count;
        if (ContainsString(accepted_keys, item.bucket_key)) {
          ++stats->pushed;
          row.fingerprint = push_fingerprints[k - offset];
          new_brow.push_back(row);
        } else if (ContainsString(skipped_keys, item.bucket_key)) {
          ++stats->skipped;
          // ★ 服务端跳过 ⇒ 云端已存版本 ≥ 本次上报值 ⇒ 本次内容**未被采纳**
          //   ⇒ 桶行指纹留空（未确认）⇒ 下一轮按更大的桶级时序重试。
          row.fingerprint.clear();
          new_brow.push_back(row);
        }
        // 服务端既没 accepted 也没 skipped ⇒ 不写桶行 ⇒ 下一轮重试。
      }
    }
  }

  // 6) 降级桶：改走逐条端点推该桶条目（FR-214），不因此使整批失败。
  bool degraded_pushed = false;
  if (!degraded_entries.empty() && legacy_endpoint_active) {
    degraded_pushed = true;
    CloudSyncService service(transport_, access_token_);
    service.SetAccessTokenRefresher(refresher_);
    for (size_t offset = 0; offset < degraded_entries.size();
         offset += kPushBatchSize) {
      const size_t rest = degraded_entries.size() - offset;
      const size_t count = rest < kPushBatchSize ? rest : kPushBatchSize;
      std::vector<Entry> chunk(degraded_entries.begin() + offset,
                               degraded_entries.begin() + offset + count);
      std::vector<CloudSyncItem> sealed;
      std::string seal_err;
      if (!SealEntries(chunk, ItemType::kUserDb, enc_key_, &sealed,
                       &seal_err)) {
        push_ok = false;
        push_err = seal_err;
        degraded_pushed = false;
        break;
      }
      int64_t pushed_seq = last_server_seq_;
      std::vector<std::string> accepted_keys;
      std::vector<std::string> skipped_keys;
      if (result != nullptr)
        ++result->push_requests;
      if (!service.Push(sealed, &pushed_seq, &push_err, &accepted_keys,
                        &skipped_keys, nullptr)) {
        push_ok = false;
        degraded_pushed = false;
        break;
      }
      if (result != nullptr) {
        for (size_t k = 0; k < sealed.size(); ++k) {
          const std::string key_id = ComputeItemKeyId(enc_key_, chunk[k].key);
          switch (ClassifyPushResult(accepted_keys, skipped_keys, key_id)) {
            case PushDisposition::kAccepted:
              ++result->items_pushed;
              break;
            case PushDisposition::kSkipped:
              ++result->items_skipped;
              break;
            case PushDisposition::kUnknown:
              break;
          }
        }
      }
      if (pushed_seq > last_server_seq_)
        last_server_seq_ = pushed_seq;
    }
  }

  // 7) 重建桶行：推成功的记新指纹；没变的沿用旧行；失败的**不写**（下轮重试）。
  for (std::map<std::string, std::vector<Entry> >::const_iterator it =
           groups.begin();
       it != groups.end(); ++it) {
    bool written = false;
    for (size_t i = 0; i < new_brow.size(); ++i) {
      if (new_brow[i].bucket_key == it->first) {
        written = true;
        break;
      }
    }
    if (written)
      continue;
    std::map<std::string, const CloudSidecarBucketRow*>::const_iterator brow =
        brow_index.find(it->first);
    if (brow != brow_index.end())
      new_brow.push_back(*brow->second);
  }
  // 降级桶：只有在「已改走逐条端点且推送成功」时才记「已降级」（指纹带 "d:"
  //   前缀）⇒ 不再每轮重复降级；逐条端点已退役时**不记** ⇒ 下一轮重试。
  if (degraded_pushed) {
    for (std::map<std::string, std::string>::const_iterator it =
             degraded_fingerprints.begin();
         it != degraded_fingerprints.end(); ++it) {
      bool written = false;
      for (size_t i = 0; i < new_brow.size(); ++i) {
        if (new_brow[i].bucket_key == it->first) {
          written = true;
          break;
        }
      }
      if (written)
        continue;
      CloudSidecarBucketRow row;
      row.bucket_key = it->first;
      row.fingerprint = "d:" + it->second;
      new_brow.push_back(row);
    }
  }
  stats->rows_pushed = rows_pushed;
  stats->rows_held = rows_held;
  // ★ BR-①g 规则 1/2 的无条件诊断行（只新增，不删改既有 phase 行）。
  diag::AppendLine("cloudsync",
                   FormatBucketQuotaLine(static_cast<int>(held_unproven),
                                         static_cast<int>(held_quota),
                                         rows_pushed, rows_held, quota_rows));
  bucket_rows_ = new_brow;

  if (!push_ok) {
    if (err)
      *err = push_err.empty() ? "推送云端桶失败。" : push_err;
    return false;
  }
  return true;
}

bool OpenItemsTolerant(const std::vector<CloudSyncItem>& items,
                       const std::vector<uint8_t>& enc_key,
                       std::vector<Entry>* out,
                       int* undecryptable,
                       int* malformed,
                       std::string* err) {
  if (out == nullptr || undecryptable == nullptr || malformed == nullptr) {
    if (err)
      *err = "内部错误：解密输出参数为空";
    return false;
  }
  out->clear();
  *undecryptable = 0;
  *malformed = 0;
  if (items.empty())
    return true;
  if (enc_key.size() != 32) {
    if (err)
      *err = "内部错误：加密密钥长度不是 32 字节";
    return false;
  }
  for (size_t i = 0; i < items.size(); ++i) {
    const std::vector<CloudSyncItem> one(1, items[i]);
    std::vector<Entry> opened;
    std::string one_err;
    if (!OpenItems(one, enc_key, &opened, &one_err)) {
      ++(*undecryptable);
      continue;
    }
    // ★ 批次 BM / ADR 0071：解得开**还不算**可用 —— 云端可能躺着**形态
    //   非法**的旧条目（payload 无制表符）。它会一路走到写回阶段，让
    //   BuildUserDictSnapshots 整体失败 ⇒ 一条脏数据毒化**每一轮**同步
    //   （批次 BK 真机复现：云端一条旧条目 ⇒ 每一轮同步都失败）。
    //   故这里按**写回口径**校验形态，非法者跳过并**单独计数**
    //   （malformed），不与「解不开」（undecryptable）混计。
    //   ★ 只有 userdb 才按「词典行」校验：phrases / config 的 payload
    //     不是词典行形状，拿词典口径去卡它们会误伤（BM 单测 ⑤ 正控制）。
    ItemType item_type = ItemType::kConfig;
    const bool type_known = ParseItemType(items[i].type, &item_type);
    if (type_known && item_type == ItemType::kUserDb) {
      bool shape_ok = true;
      for (size_t k = 0; k < opened.size(); ++k) {
        if (!IsValidUserDictEntryShape(opened[k].key, opened[k].payload)) {
          shape_ok = false;
          break;
        }
      }
      if (!shape_ok) {
        ++(*malformed);
        diag::AppendLine(
            "cloudsync",
            "skipped malformed cloud userdb entry (payload shape invalid)");
        continue;
      }
    }
    out->insert(out->end(), opened.begin(), opened.end());
  }
  return true;
}

namespace {

// sidecar 的 keyfp 行内容：enc_key 的指纹（只落指纹，不落密钥本体）。
// 复用条目身份的 HKDF + MAC 派生，不引入新的密码学常量。
const char kSidecarKeyFpProbe[] = "fluxing-cloud-sync-keyfp-v1";
// 显式「尚未绑定密钥」标记：ResetCloudSyncSidecarFile 写它，使
// 「清空云端后的第一轮」不会被误判成密钥轮换。
const char kSidecarKeyFpUnbound[] = "-";

std::string SidecarKeyFp(const std::vector<uint8_t>& enc_key) {
  if (enc_key.size() != 32)
    return kSidecarKeyFpUnbound;
  return ComputeItemKeyId(enc_key, kSidecarKeyFpProbe);
}

}  // namespace

// ADR 0076 · D2-5a-9：语义指纹（口径说明见 CloudSyncService.h）。
std::string SemanticFingerprint(const std::string& payload) {
  // ★ ADR 0076 D2-5a-9 ①：复用既有切分函数 SplitDictPayload
  //   （WeaselServer/Cloud/CloudUserDictIo.cpp:326-337）⇒ 不新增第二套
  //   payload 形态解析器（宪法 R7 · 单一真相源）。
  std::string code;
  std::string value;
  if (!SplitDictPayload(payload, &code, &value))
    return payload;  // 形状异常（无制表符）⇒ 原样退化（不丢信息、不误判）
  std::string kept;
  kept.reserve(value.size());
  size_t pos = 0;
  while (pos < value.size()) {
    size_t end = value.find(' ', pos);
    if (end == std::string::npos)
      end = value.size();
    const std::string token = value.substr(pos, end - pos);
    // 只剔除**派生**的 d= / t= 两个 token；c=（commits）保留。
    const bool derived = token.size() >= 2 && token[1] == '=' &&
                         (token[0] == 'd' || token[0] == 't');
    if (!derived) {
      if (!kept.empty())
        kept += ' ';
      kept += token;
    }
    pos = end + 1;
  }
  std::string out = code;
  out += '\t';
  out += kept;
  return out;
}

std::string SemanticHashHex(const std::string& payload) {
  const std::string digest = Sha256Hex(SemanticFingerprint(payload));
  if (digest.empty())
    return std::string();  // 摘要失败 ⇒ 不产出「半截」标记
  return "s2:" + digest;
}

bool IsSemanticHash(const std::string& payload_hash) {
  return payload_hash.size() > 3 && payload_hash.compare(0, 3, "s2:") == 0;
}

std::string BuildSidecarText(const std::vector<CloudSidecarRow>& rows,
                             int64_t server_seq,
                             const std::vector<uint8_t>& enc_key) {
  std::string out;
  out.reserve(64 + rows.size() * 128);
  out += "#fluxing-cloud-sync-v1\n";
  out += "seq\t";
  out += std::to_string(server_seq);
  out += "\n";
  out += "keyfp\t";
  out += SidecarKeyFp(enc_key);
  out += "\n";
  for (size_t i = 0; i < rows.size(); ++i) {
    out += "row\t";
    out += EscapeKey(rows[i].item_key);
    out += "\t";
    out += std::to_string(rows[i].ms);
    out += "\t";
    out += std::to_string(rows[i].counter);
    out += "\t";
    out += rows[i].device;
    out += "\t";
    out += rows[i].payload_hash;
    out += "\n";
  }
  return out;
}

// ★ spec 103 · BR-①g：侧车 v2 文本。
//   **老端点游标 seq 的语义与取值一律不变**（绝不归零 ⇒ 老端点不会全量重扫）；
//   新增 bseq（桶端点游标，编号空间独立）、phase（灰度阶段）与 brow（桶行）。
std::string BuildSidecarTextV2(
    const std::vector<CloudSidecarRow>& rows,
    int64_t server_seq,
    int64_t bucket_seq,
    int bucket_phase,
    const std::vector<CloudSidecarBucketRow>& bucket_rows,
    const std::vector<uint8_t>& enc_key) {
  std::string out;
  out.reserve(96 + rows.size() * 128 + bucket_rows.size() * 160);
  out += "#fluxing-cloud-sync-v2\n";
  out += "seq\t";
  out += std::to_string(server_seq);
  out += "\n";
  out += "bseq\t";
  out += std::to_string(bucket_seq);
  out += "\n";
  out += "phase\t";
  out += std::to_string(bucket_phase);
  out += "\n";
  out += "keyfp\t";
  out += SidecarKeyFp(enc_key);
  out += "\n";
  for (size_t i = 0; i < rows.size(); ++i) {
    out += "row\t";
    out += EscapeKey(rows[i].item_key);
    out += "\t";
    out += std::to_string(rows[i].ms);
    out += "\t";
    out += std::to_string(rows[i].counter);
    out += "\t";
    out += rows[i].device;
    out += "\t";
    out += rows[i].payload_hash;
    out += "\n";
  }
  for (size_t i = 0; i < bucket_rows.size(); ++i) {
    out += "brow\t";
    out += EscapeKey(bucket_rows[i].bucket_key);
    out += "\t";
    out += std::to_string(bucket_rows[i].ms);
    out += "\t";
    out += std::to_string(bucket_rows[i].counter);
    out += "\t";
    out += bucket_rows[i].device;
    out += "\t";
    out += std::to_string(bucket_rows[i].item_count);
    out += "\t";
    out += bucket_rows[i].fingerprint;
    out += "\n";
  }
  return out;
}

// ★ 解析侧车文本（v1 / v2 通用）。
//   v1 文件 ⇒ bucket_seq = 0、bucket_phase = 0、无 brow（AC-109 字面口径）。
bool ParseSidecarTextV2(const std::string& text,
                        const std::vector<uint8_t>& enc_key,
                        std::vector<CloudSidecarRow>* rows,
                        int64_t* server_seq,
                        int64_t* bucket_seq,
                        int* bucket_phase,
                        std::vector<CloudSidecarBucketRow>* bucket_rows,
                        bool* key_rotated,
                        std::string* err) {
  if (rows == nullptr || server_seq == nullptr || key_rotated == nullptr) {
    if (err)
      *err = "内部错误：sidecar 解析输出参数为空";
    return false;
  }
  rows->clear();
  *server_seq = 0;
  *key_rotated = false;
  if (bucket_seq != nullptr)
    *bucket_seq = 0;
  if (bucket_phase != nullptr)
    *bucket_phase = 0;
  if (bucket_rows != nullptr)
    bucket_rows->clear();

  std::string stored_keyfp;
  bool seen_keyfp = false;
  std::stringstream in(text);
  std::string line;
  bool first = true;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (first) {
      first = false;
      continue;  // 版本行
    }
    if (line.empty())
      continue;
    const std::vector<std::string> cols = SplitTab(line);
    if (cols.size() >= 2 && cols[0] == "seq") {
      *server_seq = std::strtoll(cols[1].c_str(), nullptr, 10);
      continue;
    }
    if (cols.size() >= 2 && cols[0] == "bseq") {
      if (bucket_seq != nullptr)
        *bucket_seq = std::strtoll(cols[1].c_str(), nullptr, 10);
      continue;
    }
    if (cols.size() >= 2 && cols[0] == "phase") {
      const long value = std::strtol(cols[1].c_str(), nullptr, 10);
      if (bucket_phase != nullptr && value >= 0 && value <= 4)
        *bucket_phase = static_cast<int>(value);
      continue;
    }
    if (cols.size() >= 2 && cols[0] == "keyfp") {
      stored_keyfp = cols[1];
      seen_keyfp = true;
      continue;
    }
    if (cols.size() >= 6 && cols[0] == "row") {
      CloudSidecarRow row;
      row.item_key = UnescapeKey(cols[1]);
      row.ms = std::strtoull(cols[2].c_str(), nullptr, 10);
      row.counter =
          static_cast<uint32_t>(std::strtoul(cols[3].c_str(), nullptr, 10));
      row.device = cols[4];
      row.payload_hash = cols[5];
      rows->push_back(row);
      continue;
    }
    if (cols.size() >= 7 && cols[0] == "brow") {
      CloudSidecarBucketRow brow;
      brow.bucket_key = UnescapeKey(cols[1]);
      brow.ms = std::strtoull(cols[2].c_str(), nullptr, 10);
      brow.counter =
          static_cast<uint32_t>(std::strtoul(cols[3].c_str(), nullptr, 10));
      brow.device = cols[4];
      brow.item_count =
          static_cast<size_t>(std::strtoull(cols[5].c_str(), nullptr, 10));
      brow.fingerprint = cols[6];
      if (bucket_rows != nullptr)
        bucket_rows->push_back(brow);
      continue;
    }
  }

  if (seen_keyfp && stored_keyfp == kSidecarKeyFpUnbound)
    return true;  // 「未绑定密钥」：不是轮换
  const bool rotated = seen_keyfp ? (stored_keyfp != SidecarKeyFp(enc_key))
                                  : !(rows->empty() && *server_seq == 0);
  if (!rotated)
    return true;
  rows->clear();
  *server_seq = 0;
  *key_rotated = true;
  // ★ 换口令 ⇒ 桶键的派生密钥也变了 ⇒ 桶行基线与桶端点游标一并作废
  //   （旧桶用的是旧密钥，云端那批桶再也解不开 ⇒ 本机从头重扫 + 重推）。
  if (bucket_rows != nullptr)
    bucket_rows->clear();
  if (bucket_seq != nullptr)
    *bucket_seq = 0;
  return true;
}

// ★ v1 语义的薄封装：**签名与语义逐字不变**（既有调用方与单测不受影响）。
bool ParseSidecarText(const std::string& text,
                      const std::vector<uint8_t>& enc_key,
                      std::vector<CloudSidecarRow>* rows,
                      int64_t* server_seq,
                      bool* key_rotated,
                      std::string* err) {
  int64_t bucket_seq = 0;
  int bucket_phase = 0;
  std::vector<CloudSidecarBucketRow> bucket_rows;
  return ParseSidecarTextV2(text, enc_key, rows, server_seq, &bucket_seq,
                            &bucket_phase, &bucket_rows, key_rotated, err);
}

bool ResetCloudSyncSidecarFile(const std::string& sidecar_path,
                               std::string* err) {
  if (sidecar_path.empty())
    return true;
  // 用「只有版本行 + 游标归零」覆盖 sidecar，而不是删文件 ——
  // 路径可能含非 ASCII 字符，Win32 的 ANSI 版 API 会踩坑。
  std::ofstream out(sidecar_path.c_str(), std::ios::binary | std::ios::trunc);
  if (!out) {
    if (err)
      *err = "无法重置同步状态文件：" + sidecar_path;
    return false;
  }
  out << "#fluxing-cloud-sync-v1\n";
  out << "seq\t0\n";
  // ★ spec 103 · BR-①g：桶端点游标 bseq 与灰度阶段 phase **一并归零**。
  //   （老端点游标 seq 本来就是 0；这里**不动**别的既有语义。）
  out << "bseq\t0\n";
  out << "phase\t0\n";
  // 显式「未绑定密钥」：下一轮 LoadSidecar 不会把这次重置误判成密钥轮换。
  out << "keyfp\t-\n";
  return true;
}

bool CloudSyncCoordinator::ResetLocalSyncState(std::string* err) {
  sidecar_rows_.clear();
  last_server_seq_ = 0;
  return ResetCloudSyncSidecarFile(sidecar_path_, err);
}

}  // namespace cloud
}  // namespace fluxing
