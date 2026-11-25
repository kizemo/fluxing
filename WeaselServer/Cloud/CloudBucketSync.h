#pragma once
//
// spec 103 · BR-①g 客户端桶化：桶键派生 / 分桶 / 桶 blob 打包与拆包 / 桶级 HLC
//
// 本头是**纯函数层**（不联网、不碰磁盘）⇒ 生产代码与单测共用同一份实现
// （宪法 R7「生产与单测同源」）。
//
// 桶键口径（ADR 0077 D2 的客户端落地版 · 与服务端 wire.ts 对齐）：
//   · 桶成员**只由条目本身决定**（改一条词条只重写 1 个桶，不重写整册）；
//   · 分片索引取**加密域**的 HMAC 摘要字节 —— 明文 sha256 可被字典穷举
//     反推「哪个词条落在哪个桶」⇒ 不可用；
//   · 桶键本身用**独立 HKDF 标签**（fluxing-bucketkey-mac-v1）派生的 MAC 密钥
//     再做一次 MAC ⇒ 桶键与 item_key 身份**不可互推**，也不与
//     fluxing-itemkey-mac-v1 复用同一把密钥。
//
// 分桶粒度：一级 64 片 × 二级 16 片 = **1024 桶 / 词典**
//   · 54,536 条 / 1024 ≈ 53 条/桶（服务端上限 MAX_BUCKET_ITEMS =
//   4096，余量充足）； · 首导 D1 行数 ≈ 1024（替代 54,536 行，约 53 倍改善）；
//   · 单请求 ≤ 64 桶（服务端 MAX_PUSH_BUCKETS）⇒ 16 个请求完成首导。
//
// 桶 blob 格式（ADR 0077 D3）：**N 个既有 8 行信封文本块直接串接**。
// 服务端不解析 blob（只见密文）⇒ E2E 不变量与条目级 AAD **逐字不变**，
// 老密文在新协议里仍可逐条验证。
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

namespace fluxing {
namespace cloud {

// ---------------------------------------------------------------------------
// 常量（与服务端 cloud/fluxing-sync/src/sync-buckets.ts 逐字对齐）
// ---------------------------------------------------------------------------

// 一级分片数（64）。
constexpr int kBucketShardCount = 64;
// 二级分片数（16）⇒ 每词典 1024 个桶。
constexpr int kBucketSubShardCount = 16;
// 每词典的桶总数。
constexpr int kBucketCount = kBucketShardCount * kBucketSubShardCount;
// 服务端 MAX_BUCKET_ITEMS：单个桶的条目数上限。
constexpr size_t kBucketMaxItems = 4096;
// 服务端 MAX_PUSH_BUCKETS：单次推送请求的桶数上限。
constexpr size_t kBucketPushBatch = 64;
// 服务端 MAX_BUCKET_BLOB_BYTES：单个桶 blob 的字节上限。
constexpr size_t kBucketMaxBlobBytes = 4u * 1024u * 1024u;
// 服务端 MAX_BUCKET_KEY_BYTES：桶键字节上限。
constexpr size_t kBucketMaxKeyBytes = 512;
// 桶键的十六进制长度（HMAC-SHA256 = 32 字节 = 64 位十六进制）。
constexpr size_t kBucketKeyHexLength = 64;

// ---------------------------------------------------------------------------
// 桶键派生
// ---------------------------------------------------------------------------

// 派生桶键 MAC 密钥：HKDF-SHA256(enc_key, "fluxing-bucketkey-mac-v1")。
// enc_key 长度不是 32 字节 / HKDF 失败 ⇒ false + 中文原因。
bool DeriveBucketMacKey(const std::vector<uint8_t>& enc_key,
                        std::vector<uint8_t>* out,
                        std::string* err);

// 条目 → (一级分片, 二级分片)。
// 输入是**明文 item_key**（"<词典名>\t<词条>"）⇒ 只有本机知道它。
bool ComputeBucketShard(const std::vector<uint8_t>& bucket_mac_key,
                        const std::string& item_key,
                        int* level1,
                        int* level2,
                        std::string* err);

// 桶键（线上身份）：64 位小写十六进制。
// 参数非法（分片越界 / MAC 失败）⇒ 返回空串（调用方按错误处理）。
std::string ComputeBucketKeyId(const std::vector<uint8_t>& bucket_mac_key,
                               const std::string& dict_name,
                               int level1,
                               int level2);

// ---------------------------------------------------------------------------
// 桶 blob：打包 / 拆包 / 桶级 HLC / 内容指纹
// ---------------------------------------------------------------------------

// 打包：N 个信封块串接（每块 8 行、以 '\n' 结尾 ⇒ 合计 8N 行）。
// 任一信封序列化失败 ⇒ 返回空串。
std::string PackBucketBlob(const std::vector<Envelope>& envelopes);

// 拆包：按 8 行切块后逐块 ParseEnvelope。
// 行数不是 8 的倍数 / 任一块畸形 ⇒ false + 中文原因。
bool UnpackBucketBlob(const std::string& blob,
                      std::vector<Envelope>* out,
                      std::string* err);

// 桶级 HLC = 桶内所有条目 HLC 的最大值（ADR 0077 D4）。
// 空桶 ⇒ 返回全零 Hlc。
Hlc AggregateBucketHlc(const std::vector<Envelope>& envelopes);

// 桶内容指纹的一行输入。
// payload_hash 用**语义指纹哈希**（s2: 前缀）—— 必须是语义口径，否则
// rime 每轮回写重算的 d= / t= 会让指纹每轮都变 ⇒ 每轮重推 ⇒ 写放大回归。
struct CloudBucketFingerprintItem {
  std::string item_key;
  std::string payload_hash;
  Hlc hlc;
};

// 桶内容指纹：按 item_key 升序拼行后取 SHA-256（十六进制）。
// 输入为空 ⇒ 返回 64 位十六进制（空内容的指纹），不是空串。
std::string BucketFingerprint(
    const std::vector<CloudBucketFingerprintItem>& items);

// 把 base 抬到**严格大于** floor 的位置（counter 溢出则推进 ms）。
// 用于「桶内容变了但条目 HLC 没有变大」的兜底：保证服务端 HLC 守卫
// 不会把一次真实变更整桶跳过。
Hlc MakeHlcStrictlyGreater(const Hlc& floor);

// ---------------------------------------------------------------------------
// 阶段状态机（spec 103 · BR-①g · ADR 0077 D6 灰度开关）
// ---------------------------------------------------------------------------
//
// 阶段决定「这一轮走哪些端点」：
//   kLegacy(0)    老端点单跑（**默认值** ⇒ 未开闸时行为与本批之前逐字不变）
//   kDualWrite(1) 老端点 +
//   桶端点**双写**（推两边；拉两边，桶拉到的并入远端集合） kVerify(2)    同双写
//   + **交叉校验**（老端点条目数 vs 桶端点解包条目数） kSwitchRead(3)
//   **切读**：只拉桶、只推桶，老端点完全不发 kRetired(4)   老端点退役（行为等价
//   3，保留枚举以便回滚到 0）
//
// 生效口径：**环境变量 FLUXING_CLOUD_SYNC_BUCKET_PHASE 优先**（值非法则忽略
// 并回退到侧车里的 phase）；环境变量给定且有效时，本轮结束后写回侧车 ⇒
// **回滚手段 = 把该环境变量设为 0 跑一轮**（不必重装、不必清数据）。
enum class CloudBucketPhase {
  kLegacy = 0,
  kDualWrite = 1,
  kVerify = 2,
  kSwitchRead = 3,
  kRetired = 4,
};

// 阶段的环境变量名（唯一的覆盖入口；生产代码读它，测试也直接用它）。
constexpr char kBucketPhaseEnvVar[] = "FLUXING_CLOUD_SYNC_BUCKET_PHASE";

// 阶段的整数值（诊断行 / 侧车落盘用）。
constexpr int BucketPhaseValue(CloudBucketPhase phase) {
  return static_cast<int>(phase);
}

// 解析环境变量的取值。合法（"0".."4"）⇒ true + out；
// 非法（空 / 非数字 / 越界）⇒ false（调用方**回退到侧车里的 phase**）。
bool ParseBucketPhaseValue(const std::string& raw, CloudBucketPhase* out);

// 解析**生效**阶段：环境变量优先（非法 ⇒ 忽略并回退到侧车 phase）。
// out_from_env 为 true ⇒ 生效值来自环境变量 ⇒ 本轮结束后写回侧车。
bool ResolveBucketPhase(const std::string& env_raw,
                        bool env_present,
                        CloudBucketPhase sidecar_phase,
                        CloudBucketPhase* out,
                        bool* out_from_env);

// ---------------------------------------------------------------------------
// 桶级线格式（wire）
// ---------------------------------------------------------------------------
//
// 服务端契约（真相源：cloud/fluxing-sync/src/sync-buckets.ts + wire.ts）：
//   POST /v1/sync/buckets/push
//        body: { "buckets": [ { "bucket_key", "item_count", "cipher", "hlc" }
//        ],
//                "device_id": "<可省略>" }
//        resp: { "accepted": [...], "skipped": [...], "server_seq": n }
//   GET  /v1/sync/buckets/pull?since=<n>&limit=<n>&cursor=<base64url(id)>
//        resp: { "buckets": [ { "bucket_key", "device_id", "hlc", "cipher",
//                              "item_count", "server_seq" } ],
//                "next_cursor": "...", "has_more": bool, "server_seq": n }
//
// ⚠ device_id：客户端**一律不上报**。服务端拿它跟「当前登录设备的行主键」
//   比对，不等即 400 bad_device_id ⇒ 客户端的设备标识与服务端的行主键不是
//   同一个东西 ⇒ 只能省略（服务端按登录设备归属桶行）。

// 一个待推送的桶（线上形态）。
struct CloudBucketPushItem {
  std::string bucket_key;  // 64 位小写十六进制
  size_t item_count = 0;  // 桶内条目数（服务端只校验上界、不核验）
  std::string cipher_b64;  // blob 的标准 base64
  Hlc hlc;                 // 桶级 HLC（ADR 0077 D4）
};

// 拼桶级推送请求体。device_id 为空 ⇒ 不带该字段。
std::string BuildBucketPushBody(const std::vector<CloudBucketPushItem>& buckets,
                                const std::string& device_id);

// 服务端回吐的一个桶（**跨设备**视图：不含本机过滤）。
struct CloudBucketPullItem {
  std::string bucket_key;
  std::string device_id;
  Hlc hlc;
  std::string cipher_b64;
  size_t item_count = 0;
  int64_t server_seq = 0;
};

// 解析桶级拉取响应。
// · server_seq：只增不减（服务端在空页时回填请求游标 ⇒ 游标不倒退）。
// · next_cursor：续读位置（不透明，服务端给什么就原样回传）。
// 失败 ⇒ false + err 中文原因。
bool ParseBucketPullResponse(const std::string& body,
                             std::vector<CloudBucketPullItem>* out,
                             int64_t* server_seq,
                             std::string* err,
                             bool* has_more,
                             std::string* next_cursor);

// ---------------------------------------------------------------------------
// 桶键派生（组合入口）与桶拉取路径
// ---------------------------------------------------------------------------

// 条目 → 桶键（组合入口）。
// 步骤：① 按**明文 item_key**（"<词典名>\t<词条>"）算分片；
//       ② 按「词典名 + 分片」算桶键（线上身份，64 位小写十六进制）。
// 失败（密钥为空 / 分片越界 / MAC 失败）⇒ false + err 中文原因。
bool ComputeBucketKeyForItem(const std::vector<uint8_t>& bucket_mac_key,
                             const std::string& dict_name,
                             const std::string& item_key,
                             std::string* bucket_key,
                             int* level1,
                             int* level2,
                             std::string* err);

// 桶级拉取路径：GET /v1/sync/buckets/pull?since=<n>&limit=<n>[&cursor=<c>]
// cursor 是服务端给的不透明串（base64url(id)，可能带 '=' 填充）⇒ 逐字做
// 百分号编码（只放行 unreserved 字符）。
std::string BuildBucketPullPath(int64_t since,
                                int64_t limit,
                                const std::string& cursor);

}  // namespace cloud
}  // namespace fluxing
