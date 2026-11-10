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
                       std::string* err) {
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

  int64_t seq = 0;
  json::MemberInt64(doc, doc.root, "server_seq", &seq);
  if (server_seq)
    *server_seq = seq;

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
    json::MemberInt64(doc, index, "server_seq", &item.server_seq);
    out->push_back(item);
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
    const std::string aad = BuildAad(entry.key, type, entry.hlc);
    std::vector<uint8_t> cipher;
    std::string seal_err;
    if (!crypto::AesGcmSeal(enc_key, nonce, aad, entry.payload, &cipher,
                            &seal_err)) {
      if (err)
        *err = "加密条目失败：" +
               (seal_err.empty() ? std::string("未知原因") : seal_err);
      return false;
    }
    CloudSyncItem item;
    item.item_key = entry.key;
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
    Entry entry;
    entry.key = item.item_key;
    entry.payload = plain;
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
                            std::string* err) {
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

  json::JsonDocument doc;
  if (!json::Parse(response, &doc) || doc.root < 0) {
    if (err)
      *err = "服务器返回的数据无法解析。";
    return false;
  }
  int64_t seq = 0;
  json::MemberInt64(doc, doc.root, "server_seq", &seq);
  if (server_seq)
    *server_seq = seq;
  return true;
}

bool CloudSyncService::Pull(int64_t since,
                            int64_t limit,
                            std::vector<CloudSyncItem>* out,
                            int64_t* new_server_seq,
                            std::string* err) {
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
  return ParsePullResponse(response, out, new_server_seq, err);
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

bool CloudSyncCoordinator::SyncOnce(std::string* err) {
  if (transport_ == nullptr || backend_ == nullptr) {
    if (err)
      *err = "云端服务尚未初始化。";
    return false;
  }
  if (access_token_.empty()) {
    if (err)
      *err = "尚未登录，无法同步。";
    return false;
  }

  std::string ignored;
  LoadSidecar(&ignored);

  // 维护模式：锁住用户库（否则 leveldb LOCK 失败）。
  std::string begin_err;
  if (!backend_->BeginMaintenance(&begin_err)) {
    if (err)
      *err = begin_err.empty() ? "无法进入用户词典维护模式。" : begin_err;
    return false;
  }

  std::string text;
  std::string export_err;
  if (!backend_->ExportUserDict(&text, &export_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = export_err.empty() ? "导出用户词典失败。" : export_err;
    return false;
  }

  std::vector<UserDictSnapshot> snapshots;
  std::string parse_err;
  if (!ParseUserDictSnapshots(text, &snapshots, &parse_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = parse_err;
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
    return false;
  }

  // 给每行分配 / 复用 HLC：sidecar 里哈希没变 ⇒ 沿用旧 HLC；
  // 变了 ⇒ 分配新的（本地改动比远端新）。
  const uint64_t now_ms = UnixTimeMs();
  std::vector<CloudSidecarRow> new_rows;
  for (size_t i = 0; i < local_entries.size(); ++i) {
    Entry& entry = local_entries[i];
    const std::string hash = Sha256Hex(entry.payload);
    const CloudSidecarRow* known = nullptr;
    for (size_t k = 0; k < sidecar_rows_.size(); ++k) {
      if (sidecar_rows_[k].item_key == entry.key) {
        known = &sidecar_rows_[k];
        break;
      }
    }
    if (known != nullptr && known->payload_hash == hash) {
      entry.hlc.ms = known->ms;
      entry.hlc.counter = known->counter;
      entry.hlc.device = known->device;
      HlcObserve(&clock_, entry.hlc);
    } else {
      entry.hlc = HlcTick(&clock_, now_ms);
    }
    CloudSidecarRow row;
    row.item_key = entry.key;
    row.ms = entry.hlc.ms;
    row.counter = entry.hlc.counter;
    row.device = entry.hlc.device;
    row.payload_hash = hash;
    new_rows.push_back(row);
  }

  // 1) 推送本地条目。
  CloudSyncService service(transport_, access_token_);
  std::vector<CloudSyncItem> sealed;
  std::string seal_err;
  if (!SealEntries(local_entries, ItemType::kUserDb, enc_key_, &sealed,
                   &seal_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = seal_err;
    return false;
  }

  int64_t pushed_seq = last_server_seq_;
  std::string push_err;
  if (!sealed.empty() && !service.Push(sealed, &pushed_seq, &push_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = push_err.empty() ? "推送失败。" : push_err;
    return false;
  }

  // 2) 拉远端条目。
  std::vector<CloudSyncItem> pulled;
  int64_t pulled_seq = pushed_seq;
  std::string pull_err;
  if (!service.Pull(pushed_seq, 100, &pulled, &pulled_seq, &pull_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = pull_err.empty() ? "拉取失败。" : pull_err;
    return false;
  }

  std::vector<Entry> remote_entries;
  if (!OpenItems(pulled, enc_key_, &remote_entries, &seal_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = seal_err;
    return false;
  }

  // 3) 合并 + 回写。
  const std::vector<Entry> merged =
      MergeLocalAndRemote(local_entries, remote_entries);

  std::vector<CloudDictEntry> merged_rows;
  if (!EntriesToDictRows(merged, &merged_rows, &seal_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = seal_err;
    return false;
  }

  bool changed = merged.size() != local_entries.size();
  if (!changed) {
    for (size_t i = 0; i < merged.size(); ++i) {
      if (!(merged[i] == local_entries[i])) {
        changed = true;
        break;
      }
    }
  }
  if (changed) {
    std::string tsv;
    if (!BuildUserDictSnapshots(merged_rows, &tsv, &seal_err)) {
      backend_->EndMaintenance(&ignored);
      if (err)
        *err = seal_err;
      return false;
    }
    int imported = 0;
    std::string import_err;
    if (!backend_->ImportUserDict(tsv, &imported, &import_err)) {
      backend_->EndMaintenance(&ignored);
      if (err)
        *err = import_err.empty() ? "回写用户词典失败。" : import_err;
      return false;
    }
  }

  last_server_seq_ = pulled_seq > pushed_seq ? pulled_seq : pushed_seq;
  sidecar_rows_ = new_rows;
  std::string save_err;
  if (!SaveSidecar(&save_err)) {
    backend_->EndMaintenance(&ignored);
    if (err)
      *err = save_err;
    return false;
  }

  std::string end_err;
  if (!backend_->EndMaintenance(&end_err)) {
    if (err)
      *err = end_err.empty() ? "退出用户词典维护模式失败。" : end_err;
    return false;
  }
  return true;
}

}  // namespace cloud
}  // namespace fluxing
