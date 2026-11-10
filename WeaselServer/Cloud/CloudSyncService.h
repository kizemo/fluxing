#pragma once
//
// spec 100 阶段 D · T304 同步编排（推 / 拉 / 合并 / HLC 嵌套输出）
//
// 端点（真相源：cloud/fluxing-sync/src/index.ts + sync.ts）：
//   POST /v1/sync/push   { items: [ { item_key, type, hlc:{ms,counter,device},
//                                      nonce, cipher } ] }
//                        → { accepted: [item_key...], server_seq }
//   GET  /v1/sync/pull?since=<n>&limit=<n>
//                        → { items: [ { item_key, type,
//                        hlc:{ms,counter,device},
//                                      nonce, cipher, server_seq } ],
//                            server_seq }
//
// **HLC 只以嵌套形态 {ms,counter,device} 出现**（tasks.md T304 的硬要求）：
//   · 本模块的 BuildPushBody 只写 "hlc":{...}，不写扁平的 hlc_ms/hlc_counter；
//   · 服务端 wire.ts 的 parseHlc 兼容扁平别名，但客户端**不依赖**那个兼容
//     分支 —— 一旦依赖，就会与服务端实现漂移。
//
// 冲突合并：本地条目与远端条目都带 HLC，逐条目按 HLC 大者取胜
//   （复用 include/FluxingCloudEnvelope.h 的 MergeEntries）。
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

#include "../../include/FluxingCloudEnvelope.h"

#include "CloudJson.h"
#include "CloudUserDictIo.h"

namespace fluxing {
namespace cloud {

class ICloudTransport;

// ---------------------------------------------------------------------------
// 线格式
// ---------------------------------------------------------------------------

struct CloudSyncItem {
  std::string item_key;
  std::string type;  // config / phrases / userdb
  Hlc hlc;
  std::string nonce_b64;
  std::string cipher_b64;
  int64_t server_seq = 0;
};

// ---------------------------------------------------------------------------
// 纯函数（不联网，可单测）
// ---------------------------------------------------------------------------

// HLC → 嵌套 JSON 对象：{"ms":..,"counter":..,"device":".."}。
std::string HlcToJson(const Hlc& hlc);

// 嵌套 JSON 对象 → HLC。字段缺失 / 类型不符 / 越界 ⇒ false。
bool HlcFromJson(const json::JsonDocument& doc, int node_index, Hlc* out);

// 拼推送请求体。items[i].hlc 一律是嵌套形态。
std::string BuildPushBody(const std::vector<CloudSyncItem>& items);

// 解析拉取响应。失败 ⇒ false + err 中文原因。
bool ParsePullResponse(const std::string& body,
                       std::vector<CloudSyncItem>* out,
                       int64_t* server_seq,
                       std::string* err);

// 本地条目 → 待推送条目（逐条 AES-256-GCM 信封加密，AAD 用 5 字段 BuildAad）。
bool SealEntries(const std::vector<Entry>& entries,
                 ItemType type,
                 const std::vector<uint8_t>& enc_key,
                 std::vector<CloudSyncItem>* out,
                 std::string* err);

// 待拉取条目 → 本地条目（逐条解密）。解密失败 ⇒ false + err 中文原因。
bool OpenItems(const std::vector<CloudSyncItem>& items,
               const std::vector<uint8_t>& enc_key,
               std::vector<Entry>* out,
               std::string* err);

// 合并本地与远端（逐条目 HLC 大者取胜）。
std::vector<Entry> MergeLocalAndRemote(const std::vector<Entry>& local,
                                       const std::vector<Entry>& remote);

// 词典行 ↔ 同步条目。HLC 由调用方（协调器）填，这里只做形状转换。
bool DictRowsToEntries(const std::vector<CloudDictEntry>& rows,
                       std::vector<Entry>* out,
                       std::string* err);
bool EntriesToDictRows(const std::vector<Entry>& entries,
                       std::vector<CloudDictEntry>* out,
                       std::string* err);

// sidecar 的一行：条目键 + 它的 HLC + 条目载荷的 sha256（十六进制）。
struct CloudSidecarRow {
  std::string item_key;
  uint64_t ms = 0;
  uint32_t counter = 0;
  std::string device;
  std::string payload_hash;
};

// ---------------------------------------------------------------------------
// 网络
// ---------------------------------------------------------------------------

class CloudSyncService {
 public:
  CloudSyncService(ICloudTransport* transport, std::string access_token);

  // 推送。server_seq 输出本次推送后的服务端序号（没有条目时保持原值）。
  bool Push(const std::vector<CloudSyncItem>& items,
            int64_t* server_seq,
            std::string* err);

  // 拉取。since 是上次已处理的服务端序号。
  bool Pull(int64_t since,
            int64_t limit,
            std::vector<CloudSyncItem>* out,
            int64_t* new_server_seq,
            std::string* err);

  const std::string& access_token() const { return access_token_; }

 private:
  bool SendJson(const std::string& method,
                const std::string& path,
                const std::string& body,
                int* status,
                std::string* response_body,
                std::string* err);

  ICloudTransport* transport_ = nullptr;
  std::string access_token_;
};

// ---------------------------------------------------------------------------
// 端到端编排（后台线程调用）
// ---------------------------------------------------------------------------

// 一轮同步：
//   导出本地词典 → 给每行分配 / 复用 HLC → 加密推送 → 拉远端 →
//   解密 → MergeEntries 合并 → 有变化才回写本地 → 记录序号。
//
// 本地 HLC 存放在 sidecar 文件里（明文，不含任何密钥）：
//   第一行：#fluxing-cloud-sync-v1
//   第二行：seq\t<服务端序号>
//   其后每行：row\t<item_key>\t<ms>\t<counter>\t<device>\t<payload 的 sha256>
// 有了 payload 的哈希，本地行是否被改过就能判定；改过才分配新的 HLC，
// 否则本地永远盖过远端。
class CloudSyncCoordinator {
 public:
  CloudSyncCoordinator(ICloudTransport* transport,
                       IUserDictBackend* backend,
                       std::string access_token,
                       std::string device_id,
                       std::vector<uint8_t> enc_key,
                       std::string hlc_sidecar_path);
  ~CloudSyncCoordinator();

  bool SyncOnce(std::string* err);

  int64_t last_server_seq() const { return last_server_seq_; }
  const std::string& device_id() const { return device_id_; }

 private:
  bool LoadSidecar(std::string* err);
  bool SaveSidecar(std::string* err);

  ICloudTransport* transport_ = nullptr;
  IUserDictBackend* backend_ = nullptr;
  std::string access_token_;
  std::string device_id_;
  std::vector<uint8_t> enc_key_;
  std::string sidecar_path_;
  int64_t last_server_seq_ = 0;
  HlcClock clock_;
  // item_key → (ms, counter, device, payload 的 sha256)
  std::vector<CloudSidecarRow> sidecar_rows_;
};

}  // namespace cloud
}  // namespace fluxing
