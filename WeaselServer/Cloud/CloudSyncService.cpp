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

#include <windows.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace fluxing {
namespace cloud {

namespace {

// nodes 是平铺数组，Member() 只回指针；需要「节点下标」时用它换算回来。
int NodeIndex(const json::JsonDocument& doc, const json::JsonNode* node) {
  if (node == nullptr)
    return -1;
  return static_cast<int>(node - doc.nodes.data());
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
  const uint32_t n = (static_cast<uint32_t>(
                          static_cast<uint8_t>(plain[0]))
                      << 24) |
                     (static_cast<uint32_t>(
                          static_cast<uint8_t>(plain[1]))
                      << 16) |
                     (static_cast<uint32_t>(
                          static_cast<uint8_t>(plain[2]))
                      << 8) |
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
    const Entry& entry = entries[i];
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
    CloudSyncItem item;
    item.item_key = item_key_id;
    item.type = ItemTypeName(type);
    item.hlc = entry.hlc;
    item.nonce_b64 = Base64Encode(nonce);
    item.cipher_b64 = Base64Encode(cipher);
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
      *err = "网络请求失败：" + (resp.error.empty() ? "未知原因" : resp.error);
    return false;
  }
  if (status)
    *status = resp.status;
  if (response_body)
    *response_body = resp.body;
  if (resp.status >= 200 && resp.status < 300)
    return true;
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
      message = "服务器返回错误（HTTP " + std::to_string(resp.status) + "）。";
    *err = message;
  }
  return false;
}

bool CloudSyncService::Push(const std::vector<CloudSyncItem>& items,
                            int64_t* server_seq,
                            std::string* err,
                            std::vector<std::string>* accepted,
                            std::vector<std::string>* skipped) {
  if (items.empty())
    return true;  // 服务端拒绝空数组

  int status = 0;
  std::string response;
  std::string send_err;
  if (!SendJson("POST", "/v1/sync/push", BuildPushBody(items), &status,
                &response, &send_err)) {
    if (err)
      *err = send_err.empty() ? "推送失败。" : send_err;
    return false;
  }

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
  if (sidecar_path_.empty())
    return true;

  std::ifstream in(sidecar_path_.c_str(), std::ios::binary);
  if (!in)
    return true;  // 没有 sidecar 是正常的（第一次同步）

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
      last_server_seq_ = std::strtoll(cols[1].c_str(), nullptr, 10);
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
      sidecar_rows_.push_back(row);
      continue;
    }
  }
  return true;
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
  out << "#fluxing-cloud-sync-v1\n";
  out << "seq\t" << last_server_seq_ << "\n";
  for (size_t i = 0; i < sidecar_rows_.size(); ++i) {
    const CloudSidecarRow& row = sidecar_rows_[i];
    out << "row\t" << EscapeKey(row.item_key) << "\t" << row.ms << "\t"
        << row.counter << "\t" << row.device << "\t" << row.payload_hash
        << "\n";
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

// 单次推送的条目上限（分批，避免超大请求）。
constexpr size_t kPushBatchSize = 200;
// 单次拉取的条目上限（服务端 MAX_PULL_LIMIT = 100）。
constexpr int64_t kPullPageLimit = 100;
// 单轮最多拉取的页数（超出部分留到下一轮，状态里说明「还有更多」）。
constexpr int kMaxPullPages = 10;

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
  out->ok = false;
  out->push_requests = 0;
  out->pull_requests = 0;
  out->items_pushed = 0;
  out->items_pulled = 0;
  out->has_more = false;
  // ADR 0062: 「跳过」标记（闸门关闭 => 整轮跳过，不是失败）。
  out->skipped = false;
  out->message.clear();

  if (transport_ == nullptr || backend_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    out->message = "云端服务尚未初始化。";
    return false;
  }
  if (access_token_.empty()) {
    if (err)
      *err = "尚未登录，无法同步。";
    out->message = "尚未登录，无法同步。";
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
      return false;
    }
  }

  std::string ignored;
  LoadSidecar(&ignored);

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
      return false;
    }
    const std::string reason =
        begin_err.empty() ? "无法进入用户词典维护模式。" : begin_err;
    if (err)
      *err = reason;
    out->message = reason;
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
    return false;
  }

  std::vector<UserDictSnapshot> snapshots;
  std::string parse_err;
  if (!ParseUserDictSnapshots(text, &snapshots, &parse_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = parse_err;
    out->message = parse_err;
    return false;
  }

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
    return false;
  }

  // 给每行分配 / 复用 HLC：sidecar 里哈希没变 ⇒ 沿用旧 HLC（本地没改过）；
  // 变了 ⇒ 分配新的（本地改动比远端新），并标记为「待推送」。
  const uint64_t now_ms = UnixTimeMs();
  std::vector<bool> locally_modified(local_entries.size(), false);
  for (size_t i = 0; i < local_entries.size(); ++i) {
    Entry& entry = local_entries[i];
    const std::string hash = Sha256Hex(entry.payload);
    const CloudSidecarRow* known = FindSidecarRow(sidecar_rows_, entry.key);
    if (known != nullptr && known->payload_hash == hash) {
      entry.hlc.ms = known->ms;
      entry.hlc.counter = known->counter;
      entry.hlc.device = known->device;
      HlcObserve(&clock_, entry.hlc);
    } else {
      entry.hlc = HlcTick(&clock_, now_ms);
      locally_modified[i] = true;
    }
  }

  // 1) 拉远端条目（按页分批；游标只增不减）。
  std::vector<Entry> remote_entries;
  int64_t pulled_seq = last_server_seq_;
  bool has_more = false;
  if (direction == SyncDirection::kPullOnly ||
      direction == SyncDirection::kBoth) {
    CloudSyncService service(transport_, access_token_);
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
        return false;
      }
      out->items_pulled += static_cast<int>(batch.size());
      std::vector<Entry> page_entries;
      if (!OpenItems(batch, enc_key_, &page_entries, &parse_err)) {
        backend_->EndMaintenance(&ignored);
        if (err)
          *err = parse_err;
        out->message = parse_err;
        return false;
      }
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
  }

  // 2) 合并本地与远端（逐条目 HLC 大者取胜）。
  const std::vector<Entry> merged =
      MergeLocalAndRemote(local_entries, remote_entries);

  // 3) 回写合并结果（远端带来的新词 / 更新过的词条）。
  bool changed = merged.size() != local_entries.size();
  if (!changed) {
    for (size_t i = 0; i < merged.size(); ++i) {
      const size_t index = FindEntryIndex(local_entries, merged[i].key);
      if (index == static_cast<size_t>(-1) ||
          local_entries[index].payload != merged[i].payload) {
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

  // 4) 推送集合：只推「本地改动过、且合并后仍是本地版本」的条目。
  //    · 本地没改过 ⇒ 服务端已有的就是同一份，不推（T502：只带变化条目）；
  //    · 远端版本胜出 ⇒ 服务端已经握着它，不推（避免两端来回乒乓）；
  //    · 其余（本地改动过 / 远端没有这条）⇒ 必须推。
  std::vector<size_t> push_indices;  // 本轮要推送的**本地条目下标**
  std::vector<bool> pushed(local_entries.size(), false);  // 本轮真的推成功的
  // 本轮被服务端跳过的（云端已有更新版本）—— 不是失败，但也不能算「推送」。
  std::vector<bool> skipped_flags(local_entries.size(), false);
  if (direction == SyncDirection::kPushOnly ||
      direction == SyncDirection::kBoth) {
    for (size_t i = 0; i < local_entries.size(); ++i) {
      if (!locally_modified[i])
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
      if (!service.Push(sealed, &pushed_seq, &push_err, &accepted_keys,
                        &skipped_keys)) {
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
      row.payload_hash = Sha256Hex(entry.payload);
      rows.push_back(row);
    } else {
      const CloudSidecarRow* old = FindSidecarRow(sidecar_rows_, entry.key);
      if (old != nullptr)
        rows.push_back(*old);
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
    return false;
  }

  std::string end_err;
  if (!backend_->EndMaintenance(&end_err)) {
    const std::string reason =
        end_err.empty() ? "退出用户词典维护模式失败。" : end_err;
    if (err)
      *err = reason;
    out->message = reason;
    return false;
  }

  if (!push_ok) {
    const std::string reason = push_err.empty() ? "推送失败。" : push_err;
    if (err)
      *err = reason;
    out->message = reason;
    return false;
  }

  out->ok = true;
  out->has_more = has_more;
  std::string message = "同步完成：推送 ";
  message += std::to_string(out->items_pushed);
  message += " 条，拉取 ";
  message += std::to_string(out->items_pulled);
  message += " 条";
  if (out->items_skipped > 0) {
    message += "，云端已有更新版本跳过 ";
    message += std::to_string(out->items_skipped);
    message += " 条";
  }
  if (has_more)
    message += "（云端还有更多条目，下次同步继续）";
  message += "。";
  out->message = message;
  return true;
}

bool CloudSyncCoordinator::ResetLocalSyncState(std::string* err) {
  sidecar_rows_.clear();
  last_server_seq_ = 0;
  if (sidecar_path_.empty())
    return true;
  // 用「只有版本行 + 游标归零」覆盖 sidecar，而不是删文件 ——
  // 路径可能含非 ASCII 字符，Win32 的 ANSI 版 API 会踩坑。
  std::ofstream out(sidecar_path_.c_str(), std::ios::binary | std::ios::trunc);
  if (!out) {
    if (err)
      *err = "无法重置同步状态文件：" + sidecar_path_;
    return false;
  }
  out << "#fluxing-cloud-sync-v1\n";
  out << "seq\t0\n";
  return true;
}

}  // namespace cloud
}  // namespace fluxing
