//
// spec 103 · BR-①g 客户端桶化：纯函数实现（桶键派生 / 打包拆包 / 指纹）
//
// 只依赖 CloudCrypto.h（SHA-256 / HMAC / HKDF）、include/FluxingCloudEnvelope.h
// （信封编解码 / HLC 比较）与 CloudSyncService.h（**仅**为了复用其中的 JSON /
// HLC 真相源：json::Quote、HlcToJson、HlcFromJson、CloudJsonNodeIndex）
// ⇒ 不联网、不碰磁盘、可独立单测。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "CloudBucketSync.h"

#include <algorithm>

#include "CloudCrypto.h"
#include "CloudSyncService.h"

namespace fluxing {
namespace cloud {

namespace {

std::string ToHex(const std::vector<uint8_t>& data) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(data.size() * 2);
  for (size_t i = 0; i < data.size(); ++i) {
    out += kHex[(data[i] >> 4) & 0x0F];
    out += kHex[data[i] & 0x0F];
  }
  return out;
}

}  // namespace

bool DeriveBucketMacKey(const std::vector<uint8_t>& enc_key,
                        std::vector<uint8_t>* out,
                        std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：桶密钥输出为空。";
    return false;
  }
  out->clear();
  if (enc_key.size() != kKeyBytes) {
    if (err)
      *err = "内部错误：加密密钥长度不是 32 字节。";
    return false;
  }
  // ★ 独立 HKDF 标签：桶键与条目身份**不复用**同一把 MAC 密钥 ⇒
  //   「拿到 item_key 身份」推不出「桶键」，反之亦然。
  if (!HkdfSha256(enc_key, "fluxing-bucketkey-mac-v1", out)) {
    out->clear();
    if (err)
      *err = "派生桶密钥失败。";
    return false;
  }
  return true;
}

bool ComputeBucketShard(const std::vector<uint8_t>& bucket_mac_key,
                        const std::string& item_key,
                        int* level1,
                        int* level2,
                        std::string* err) {
  if (level1 == nullptr || level2 == nullptr) {
    if (err)
      *err = "内部错误：分片输出为空。";
    return false;
  }
  if (bucket_mac_key.empty()) {
    if (err)
      *err = "内部错误：桶密钥为空。";
    return false;
  }
  // 前缀 "shard|" 保证与桶键的输入域不混淆。
  const std::string probe = std::string("shard|") + item_key;
  std::vector<uint8_t> digest;
  if (!HmacSha256(bucket_mac_key,
                  reinterpret_cast<const uint8_t*>(probe.data()), probe.size(),
                  &digest) ||
      digest.size() < 2) {
    if (err)
      *err = "计算桶分片失败。";
    return false;
  }
  // 摘要前 10 位：一级 6 位（0..63）+ 二级 4 位（0..15）。
  *level1 = static_cast<int>(digest[0] >> 2);
  *level2 = static_cast<int>(((digest[0] & 0x03) << 2) | (digest[1] >> 6));
  return true;
}

std::string ComputeBucketKeyId(const std::vector<uint8_t>& bucket_mac_key,
                               const std::string& dict_name,
                               int level1,
                               int level2) {
  if (bucket_mac_key.empty())
    return std::string();
  if (level1 < 0 || level1 >= kBucketShardCount)
    return std::string();
  if (level2 < 0 || level2 >= kBucketSubShardCount)
    return std::string();
  // 词典名带长度前缀 ⇒ 「词典名 + 分片」的拼接无歧义。
  const std::string probe = "bucket|" + std::to_string(dict_name.size()) + "|" +
                            dict_name + "|" + std::to_string(level1) + "|" +
                            std::to_string(level2);
  std::vector<uint8_t> digest;
  if (!HmacSha256(bucket_mac_key,
                  reinterpret_cast<const uint8_t*>(probe.data()), probe.size(),
                  &digest))
    return std::string();
  return ToHex(digest);
}

std::string PackBucketBlob(const std::vector<Envelope>& envelopes) {
  std::string out;
  for (size_t i = 0; i < envelopes.size(); ++i) {
    std::string block;
    if (SerializeEnvelope(envelopes[i], &block) != EnvelopeError::kOk)
      return std::string();
    out += block;
  }
  return out;
}

bool UnpackBucketBlob(const std::string& blob,
                      std::vector<Envelope>* out,
                      std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：桶内容输出为空。";
    return false;
  }
  out->clear();
  if (blob.empty())
    return true;  // 空桶是合法的（桶内 0 条）
  const std::vector<std::string> lines = detail::SplitLines(blob);
  if (lines.empty() || (lines.size() % 8) != 0) {
    if (err)
      *err = "桶内容的行数不是 8 的倍数。";
    return false;
  }
  std::vector<Envelope> parsed;
  parsed.reserve(lines.size() / 8);
  for (size_t i = 0; i + 8 <= lines.size(); i += 8) {
    std::string block;
    block.reserve(512);
    for (size_t k = 0; k < 8; ++k) {
      block += lines[i + k];
      block += '\n';
    }
    Envelope env;
    const EnvelopeError code = ParseEnvelope(block, &env);
    if (code != EnvelopeError::kOk) {
      if (err)
        *err = std::string("解析桶内容失败：") + EnvelopeErrorName(code);
      return false;
    }
    parsed.push_back(std::move(env));
  }
  *out = std::move(parsed);
  return true;
}

Hlc AggregateBucketHlc(const std::vector<Envelope>& envelopes) {
  Hlc best;
  bool has = false;
  for (size_t i = 0; i < envelopes.size(); ++i) {
    if (!has || CompareHlc(envelopes[i].hlc, best) > 0) {
      best = envelopes[i].hlc;
      has = true;
    }
  }
  return best;
}

std::string BucketFingerprint(
    const std::vector<CloudBucketFingerprintItem>& items) {
  std::vector<CloudBucketFingerprintItem> sorted = items;
  std::sort(sorted.begin(), sorted.end(),
            [](const CloudBucketFingerprintItem& a,
               const CloudBucketFingerprintItem& b) {
              return a.item_key < b.item_key;
            });
  std::string text;
  text.reserve(sorted.size() * 96);
  for (size_t i = 0; i < sorted.size(); ++i) {
    text += sorted[i].item_key;
    text += '\t';
    text += sorted[i].payload_hash;
    text += '\t';
    text += std::to_string(sorted[i].hlc.ms);
    text += '\t';
    text += std::to_string(static_cast<unsigned>(sorted[i].hlc.counter));
    text += '\t';
    text += sorted[i].hlc.device;
    text += '\n';
  }
  std::vector<uint8_t> digest;
  if (!Sha256(reinterpret_cast<const uint8_t*>(text.data()), text.size(),
              &digest))
    return std::string();
  return ToHex(digest);
}

Hlc MakeHlcStrictlyGreater(const Hlc& floor) {
  Hlc out = floor;
  if (out.counter < kHlcCounterMax) {
    out.counter = static_cast<uint16_t>(out.counter + 1);
  } else {
    out.ms += 1;
    out.counter = 0;
  }
  return out;
}

// ---------------------------------------------------------------------------
// 阶段状态机
// ---------------------------------------------------------------------------

bool ParseBucketPhaseValue(const std::string& raw, CloudBucketPhase* out) {
  if (out == nullptr)
    return false;
  // 只接受单个十进制数字 0..4（前后空白忽略；'10' / '-1' / 'x' 一律非法）。
  std::string digits;
  for (size_t i = 0; i < raw.size(); ++i) {
    const char c = raw[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      continue;
    digits += c;
  }
  if (digits.size() != 1)
    return false;
  if (digits[0] < '0' || digits[0] > '4')
    return false;
  *out = static_cast<CloudBucketPhase>(digits[0] - '0');
  return true;
}

bool ResolveBucketPhase(const std::string& env_raw,
                        bool env_present,
                        CloudBucketPhase sidecar_phase,
                        CloudBucketPhase* out,
                        bool* out_from_env) {
  if (out == nullptr)
    return false;
  CloudBucketPhase parsed = CloudBucketPhase::kLegacy;
  if (env_present && ParseBucketPhaseValue(env_raw, &parsed)) {
    *out = parsed;
    if (out_from_env != nullptr)
      *out_from_env = true;
    return true;
  }
  // 环境变量缺失或取值非法 ⇒ **忽略**并回退到侧车里的阶段（不猜、不报错）。
  *out = sidecar_phase;
  if (out_from_env != nullptr)
    *out_from_env = false;
  return true;
}

// ---------------------------------------------------------------------------
// 桶键派生（组合入口）与桶拉取路径
// ---------------------------------------------------------------------------

bool ComputeBucketKeyForItem(const std::vector<uint8_t>& bucket_mac_key,
                             const std::string& dict_name,
                             const std::string& item_key,
                             std::string* bucket_key,
                             int* level1,
                             int* level2,
                             std::string* err) {
  if (bucket_key == nullptr) {
    if (err)
      *err = "内部错误：桶键输出为空。";
    return false;
  }
  bucket_key->clear();
  int l1 = 0;
  int l2 = 0;
  if (!ComputeBucketShard(bucket_mac_key, item_key, &l1, &l2, err))
    return false;
  const std::string key = ComputeBucketKeyId(bucket_mac_key, dict_name, l1, l2);
  if (key.empty()) {
    if (err)
      *err = "计算桶键失败。";
    return false;
  }
  if (level1 != nullptr)
    *level1 = l1;
  if (level2 != nullptr)
    *level2 = l2;
  *bucket_key = key;
  return true;
}

namespace {

// 百分号编码：只放行 unreserved 字符（RFC 3986）。
std::string PercentEncode(const std::string& raw) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(raw.size() * 3);
  for (size_t i = 0; i < raw.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(raw[i]);
    const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                            c == '.' || c == '~';
    if (unreserved) {
      out += static_cast<char>(c);
    } else {
      out += '%';
      out += kHex[(c >> 4) & 0x0F];
      out += kHex[c & 0x0F];
    }
  }
  return out;
}

}  // namespace

std::string BuildBucketPullPath(int64_t since,
                                int64_t limit,
                                const std::string& cursor) {
  std::string path =
      "/v1/sync/buckets/pull?since=" + std::to_string(since < 0 ? 0 : since);
  if (limit > 0)
    path += "&limit=" + std::to_string(limit);
  if (!cursor.empty())
    path += "&cursor=" + PercentEncode(cursor);
  return path;
}

// ---------------------------------------------------------------------------
// 桶级线格式
// ---------------------------------------------------------------------------

std::string BuildBucketPushBody(const std::vector<CloudBucketPushItem>& buckets,
                                const std::string& device_id) {
  std::string s = "{\"buckets\":[";
  for (size_t i = 0; i < buckets.size(); ++i) {
    if (i > 0)
      s += ",";
    s += "{\"bucket_key\":";
    s += json::Quote(buckets[i].bucket_key);
    s += ",\"item_count\":";
    s += std::to_string(buckets[i].item_count);
    s += ",\"cipher\":";
    s += json::Quote(buckets[i].cipher_b64);
    s += ",\"hlc\":";
    s += HlcToJson(buckets[i].hlc);
    s += "}";
  }
  s += "]";
  // ⚠ device_id：客户端**一律不上报**（服务端拿它跟登录设备的行主键比对，
  //   不等即 400 bad_device_id）⇒ device_id 为空时**不带该字段**。
  if (!device_id.empty()) {
    s += ",\"device_id\":";
    s += json::Quote(device_id);
  }
  s += "}";
  return s;
}

bool ParseBucketPullResponse(const std::string& body,
                             std::vector<CloudBucketPullItem>* out,
                             int64_t* server_seq,
                             std::string* err,
                             bool* has_more,
                             std::string* next_cursor) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：桶拉取结果输出为空。";
    return false;
  }
  out->clear();
  if (has_more != nullptr)
    *has_more = false;
  if (next_cursor != nullptr)
    next_cursor->clear();

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
      *err = "服务器返回的桶拉取结果格式不正确。";
    return false;
  }

  // 顶层 server_seq：只增不减（服务端空页时回填请求游标 ⇒ 游标不倒退）。
  int64_t top_seq = 0;
  if (server_seq != nullptr &&
      json::MemberInt64(doc, doc.root, "server_seq", &top_seq) &&
      top_seq > *server_seq) {
    *server_seq = top_seq;
  }
  if (has_more != nullptr)
    json::MemberBool(doc, doc.root, "has_more", has_more);
  std::string cursor;
  json::MemberString(doc, doc.root, "next_cursor", &cursor);
  if (next_cursor != nullptr)
    *next_cursor = cursor;

  const json::JsonNode* buckets = json::Member(doc, doc.root, "buckets");
  const int buckets_index = CloudJsonNodeIndex(doc, buckets);
  if (buckets_index < 0)
    return true;  // 没有 buckets 字段 = 没有桶
  const json::JsonNode* array = doc.Node(buckets_index);
  if (array == nullptr || array->type != json::JsonType::kArray) {
    if (err)
      *err = "服务器返回的桶列表格式不正确。";
    return false;
  }

  for (size_t i = 0; i < array->children.size(); ++i) {
    const int index = array->children[i];
    CloudBucketPullItem item;
    if (!json::MemberString(doc, index, "bucket_key", &item.bucket_key) ||
        item.bucket_key.empty()) {
      if (err)
        *err = "服务器返回的桶缺少 bucket_key。";
      return false;
    }
    json::MemberString(doc, index, "device_id", &item.device_id);
    const json::JsonNode* hlc_node = json::Member(doc, index, "hlc");
    const int hlc_index = CloudJsonNodeIndex(doc, hlc_node);
    if (hlc_index < 0 || !HlcFromJson(doc, hlc_index, &item.hlc)) {
      if (err)
        *err = "服务器返回的桶 HLC 格式不正确。";
      return false;
    }
    if (!json::MemberString(doc, index, "cipher", &item.cipher_b64)) {
      if (err)
        *err = "服务器返回的桶缺少 cipher。";
      return false;
    }
    int64_t count = 0;
    if (json::MemberInt64(doc, index, "item_count", &count) && count >= 0)
      item.item_count = static_cast<size_t>(count);
    json::MemberInt64(doc, index, "server_seq", &item.server_seq);
    out->push_back(item);
  }
  return true;
}

}  // namespace cloud
}  // namespace fluxing
