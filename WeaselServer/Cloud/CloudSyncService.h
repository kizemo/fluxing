#pragma once
//
// spec 100 阶段 D · T304 同步编排（推 / 拉 / 合并 / HLC 嵌套输出）
// spec 100 阶段 F · T502/T503 收敛修复（只推变化条目 + 游标修复 + 分批）
//
// 端点（真相源：cloud/fluxing-sync/src/index.ts + sync.ts）：
//   POST /v1/sync/push   { items: [ { item_key, type, hlc:{ms,counter,device},
//                                      nonce, cipher } ] }
//                        → { accepted: [item_key...], skipped: [item_key...],
//                            server_seq }
//   GET  /v1/sync/pull?since=<n>&limit=<n>
//                        → { items: [ { item_key, type,
//                        hlc:{ms,counter,device},
//                                      nonce, cipher, server_seq } ],
//                            server_seq, next_cursor, has_more }
//
// **HLC 只以嵌套形态 {ms,counter,device} 出现**（tasks.md T304 的硬要求）：
//   · 本模块的 BuildPushBody 只写 "hlc":{...}，不写扁平的 hlc_ms/hlc_counter；
//   · 服务端 wire.ts 的 parseHlc 兼容扁平别名，但客户端**不依赖**那个兼容
//     分支 —— 一旦依赖，就会与服务端实现漂移。
//
// 冲突合并：本地条目与远端条目都带 HLC，逐条目按 HLC 大者取胜
//   （复用 include/FluxingCloudEnvelope.h 的 MergeEntries）。
//
// ★ 阶段 F 的收敛约定（T502 / T503）：
//   · **只推「本地改动过」的条目**（sidecar 里的载荷哈希变了才算改动）；
//   · 远端胜出的条目**不回推** —— 服务端已经握着它；
//   · 服务端 push 侧还有 HLC 守卫（存储版本 ≥ 入参版本 ⇒ 跳过），
//     因此客户端回传也不会把服务端版本推回旧版；
//   · 推请求按条目分批（单批 kPushBatchSize 条），拉请求按页分批
//     （单页 100 条，单轮最多 kMaxPullPages 页），超出部分下一轮继续。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "../../include/FluxingCloudEnvelope.h"

#include "CloudBucketSync.h"
#include "CloudJson.h"
#include "CloudSyncEngine.h"
#include "CloudUserDictIo.h"

namespace fluxing {
namespace cloud {

// JSON 节点下标（nodes 是平铺数组，Member() 只回指针；需要「节点下标」时
// 用它换算回来）。
// ★ 公开成 inline：桶级解析（CloudBucketSync.cpp）与条目级解析共用**同一份**
//   下标换算 ⇒ 不出现第二份实现（宪法 R7）。
inline int CloudJsonNodeIndex(const json::JsonDocument& doc,
                              const json::JsonNode* node) {
  if (node == nullptr)
    return -1;
  return static_cast<int>(node - doc.nodes.data());
}

// ★ 与服务端的硬契约：单次推送条目上限。
//   服务端真源：cloud/fluxing-sync/src/wire.ts 的 MAX_PUSH_ITEMS = 64。
//   超出即整批 400 拒收（too_many_items：「单次推送条目数上限为 64（实收
//   N）。」） —— 客户端曾按 200 分批，导致本地待推条目 >= 65
//   的轮次**永久同步失败**。
constexpr size_t kServerMaxPushItems = 64;

// ★ 客户端的**分批口径**（与服务端上限不是同一个数）：单批 32 条。
//   理由（ADR 0074 决策）：服务端单批 64 条并发化后为秒级，客户端在
//   30 s 接收超时窗口内仍留出余量 —— 单批越小，一批失败时的重试代价越小。
//   必须 <= kServerMaxPushItems（服务端上限 64，超出即整批 400 拒收）。
constexpr size_t kPushBatchSize = 32;

static_assert(kPushBatchSize <= kServerMaxPushItems,
              "推送单批不得超过服务端 MAX_PUSH_ITEMS，否则整批被拒");

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
//
// ★ server_seq 的语义（阶段 F 修复点）：
//   字段缺失时**保留调用方传入的值**，不再写 0 —— 写 0 会让游标永不前进，
//   每轮重复拉取同一批条目，直接冲击每日 50 次拉取配额。
//   条目自带的 server_seq 也会参与推进（取最大值，只增不减）。
bool ParsePullResponse(const std::string& body,
                       std::vector<CloudSyncItem>* out,
                       int64_t* server_seq,
                       std::string* err,
                       bool* has_more = nullptr);

// 解析推送响应：accepted / skipped（item_key 数组）+ 顶层 server_seq。
//
// ★ 为什么客户端必须消费 skipped（AH-4 ⑤）：
//   服务端的 HLC 守卫会把「存储版本 ≥ 入参版本」的条目放进 skipped（既不写
//   R2 也不写 D1）。客户端若把 skipped 当成 accepted，就会把**服务端根本没
//   收下**的本地改动记成「已同步」⇒ sidecar 写进本地载荷的哈希，而云端
//   握着的是另一个版本 ⇒ 两端静默分叉、本地改动再也推不上去。
//   字段缺失一律视作空数组（老服务端没有 skipped 时等价于「全部接受」）。
bool ParsePushResponse(const std::string& body,
                       std::vector<std::string>* accepted,
                       std::vector<std::string>* skipped,
                       int64_t* server_seq,
                       std::string* err);

// 服务端对某一条目的处置（AH-4 ⑤）。
enum class PushDisposition {
  kUnknown = 0,  // 服务端既没 accepted 也没 skipped（异常）⇒ 视为没推上去
  kAccepted,  // 服务端收下了
  kSkipped,   // 服务端跳过（云端已有 ≥ 本地的版本）
};

// 判定服务端对某个条目键的处置。纯函数 ⇒ 可单测精确覆盖。
PushDisposition ClassifyPushResult(const std::vector<std::string>& accepted,
                                   const std::vector<std::string>& skipped,
                                   const std::string& item_key);

// ---------------------------------------------------------------------------
// item_key 的线上身份（spec 100 AC-07 / ADR 0059）
// ---------------------------------------------------------------------------
//
// AC-07 判据（spec.md）：服务端存储与传输抓包中**均无明文词条 / 短语 /
// 配置内容**；无口令时无法还原。
//
// 旧实现的缺陷：item_key = MakeDictItemKey(...) = "<词典名>\t<词条>"
//   （例如 `luna_pinyin\t你好`）以**明文**出现在三处：
//     · push 请求体的 `item_key` 字段（抓包即可读）；
//     · D1 `sync_items.item_key` 列（查库即可读）；
//     · R2 对象键 = sha256Hex(item_key)（可被字典穷举反推）。
//   载荷虽是密文，但「用户装了哪些词、查过哪些词」已由键名泄露 ⇒ AC-07 不通过。
//
// 修法（零服务端改动 · 零表结构改动 · 零重新部署）：
//   服务端只把 item_key 当**不透明字符串**用（唯一身份维度 / HLC 守卫比较
//   基准 / R2 键的输入），因此客户端把「线上身份」换成：
//       item_key_id = HMAC-SHA256(item_key_mac_key, item_key)
//                     → 64 位小写十六进制
//   并把明文 item_key **移入已加密载荷内部**（AEAD 覆盖范围），pull 时解密
//   还原。服务端 `wire.ts` 的 `assertHeaderString` 只关心「是字符串且不含
//   控制字符」，64 位十六进制完全合规 ⇒ 服务端一行不改。
//
// 为什么用 MAC 而不是裸 SHA-256：
//   词条熵极低（常用词 + 固定词典名），裸哈希可被字典穷举还原；MAC 密钥
//   只由用户口令派生（口令不出本机）⇒ 满足 AC-07「无口令时无法还原」。
//
// 密钥链：master → HKDF("fluxing-enc-v1") → enc_key
//                → HKDF("fluxing-itemkey-mac-v1") → item_key_mac_key
//   复用现成 HkdfSha256，不引入独立密钥管理；enc_key 换口令即整体轮换。

// 派生 item_key 的 MAC 密钥（32 字节）。
// enc_key 长度不为 kKeyBytes 或 HKDF 失败 ⇒ false + 中文原因。
bool DeriveItemKeyMacKey(const std::vector<uint8_t>& enc_key,
                         std::vector<uint8_t>* out,
                         std::string* err);

// item_key → 线上身份（64 位小写十六进制）。
// 同一 enc_key + 同一 item_key 恒定输出 ⇒ 服务端去重与 HLC 守卫跨轮次稳定。
// item_key 为空 / enc_key 非法 ⇒ 返回空串（调用方按错误处理）。
std::string ComputeItemKeyId(const std::vector<uint8_t>& enc_key,
                             const std::string& item_key);

// 本地条目 → 待推送条目（逐条 AES-256-GCM 信封加密，AAD 用 5 字段 BuildAad）。
//
// ★ AC-07 之后的线格式：
//   · item.item_key      = ComputeItemKeyId(enc_key, entry.key)（MAC 十六进制）
//   · AAD                = BuildAad(MAC, type, hlc)
//   · 被加密的明文       = [4 字节大端 key_len][明文 item_key][entry.payload]
//   ⇒ 明文键只存在于密文内部；entry.payload 在密文内保持原样（不加壳），
//     封包/拆包完全封闭在本函数的密码学边界内。
bool SealEntries(const std::vector<Entry>& entries,
                 ItemType type,
                 const std::vector<uint8_t>& enc_key,
                 std::vector<CloudSyncItem>* out,
                 std::string* err);

// 待拉取条目 → 本地条目（逐条解密 + 拆封 + 身份自校验）。
//
// 顺序：AAD = BuildAad(item.item_key, type, hlc) → AES-GCM 解密 → 拆出明文
//   item_key 与 payload → **重算 MAC 并与 item.item_key 比对**。
// 最后一步防的是「把 A 条目的密文配上 B 条目的身份」这类替换：AAD 只绑定
//   传输身份，若不重算 MAC，两条合法条目互换身份后 AEAD 依然校验通过。
// 解密失败 / 封包格式非法 / 身份不一致 ⇒ false + err 中文原因。
bool OpenItems(const std::vector<CloudSyncItem>& items,
               const std::vector<uint8_t>& enc_key,
               std::vector<Entry>* out,
               std::string* err);

// 容忍旧密钥的变体：enc_key 换过之后，云端仍躺着用上一把密钥加密的行 ——
// 它们**永远解不开**；若照 OpenItems 的语义整批失败，同步会被这些作废数据
// 永久卡死（换口令后自动全量重推的必要条件）。
// 逐条解密：解得开的进 out，解不开的只计数（undecryptable），不报错。
// 只有「密钥长度不对」这类配置错误才返回 false。
//
// ★ 批次 BM / ADR 0071：解得开**还不算**可用 —— 云端可能躺着**形态非法**的
//   旧条目（payload 无制表符）。它会一路走到写回阶段，让
//   BuildUserDictSnapshots 整体失败 ⇒ 一条脏数据毒化**每一轮**同步。
//   故对 **userdb** 类条目额外做形态校验（与写回口径同一个函数），非法者
//   跳过并**单独计数**（malformed），不与「解不开」混计。
//   ★ 只有 userdb 才按「词典行」校验：phrases / config 的 payload 不是
//     词典行形状，不能拿词典口径去卡它们。
bool OpenItemsTolerant(const std::vector<CloudSyncItem>& items,
                       const std::vector<uint8_t>& enc_key,
                       std::vector<Entry>* out,
                       int* undecryptable,
                       int* malformed,
                       std::string* err);

// 合并本地与远端（逐条目 HLC 大者取胜）。
std::vector<Entry> MergeLocalAndRemote(const std::vector<Entry>& local,
                                       const std::vector<Entry>& remote);

// ADR 0076 · D2-5a-9：语义指纹（semantic
// fingerprint）——「本地是否改过」的判据。
//
// ★ 背景（BR-①c · AC-09 第 1 次复跑定位到的写放大根因）：rime 每次回写
//   （restore_user_dict → UserDbMerger）都会**重算** d=（formula_d）并把 t=
//   改写成当前 db 的 tick ⇒ 同一条**用户没改过**的词条，每轮回写后载荷字节
//   都不同 ⇒ 用**全载荷哈希**当判据 ⇒ 每轮都判成「本地改动」⇒ 每轮分配
//   新墙钟 HLC ⇒ 服务端 HLC 守卫全部落空 ⇒ 既有行被反复 UPDATE（≈ 4 万行/轮）。
//
// ★ 口径：载荷形状是 "<code>\t<value>"，value 段是空格分隔的 token 列表
//   （形如 "c=<commits> d=<dee> t=<tick>"）。语义指纹 = code 段 + value 段
//   **剔除 d= 与 t= 两个 token** 后的剩余（**保留 c=**）。
//   · c=（commits）：rime 的 merger / importer 两条路径都是 max 语义
//     （UserDbMerger::Put / UserDbImporter::Put）⇒ 只在用户**真正学习**该词
//     时增长 ⇒ 保留 c= 即保留「用户是否改过」的信息。
//   · d=（dee）与 t=（tick）：回写重算出来的**派生值** ⇒ 与用户意图无关
//     ⇒ 必须剔除，否则每轮都漂。
//
// 载荷形状异常（没有制表符）时**原样退化**为全载荷 ⇒ 不丢信息、不误判。
std::string SemanticFingerprint(const std::string& payload);

// 语义指纹的哈希（侧车里存的就是它）："s2:" + sha256(语义指纹)。
//
// ★ "s2:" 是**版本标记**：旧版侧车存的是**全载荷**哈希（裸 64 位十六进制），
//   与语义指纹哈希**不可比** ⇒ 必须能区分（ADR 0076 D2-5a-9 ③ 的迁移依据）。
std::string SemanticHashHex(const std::string& payload);

// 侧车里的哈希字段是不是**语义口径**（带 "s2:" 前缀）。
bool IsSemanticHash(const std::string& payload_hash);

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

// 令牌续期回调（可选）：同步请求收到 401 时调用一次；成功时把**新的访问
// 令牌**写进 out_token（调用方负责去账号服务取 —— 刷新令牌只在本机）。
// ★ 为什么放在这一层：同步路径（推 / 拉）此前**没有** 401→续期→重试，
//   访问令牌一过期同步就持续失败（批次 BK 真机实测：两轮同步都报
//   「访问令牌已过期，请用刷新令牌续期。」）；只有设备管理三处有这层。
using AccessTokenRefresher =
    std::function<bool(std::string* out_token, std::string* err)>;

class CloudSyncService {
 public:
  CloudSyncService(ICloudTransport* transport, std::string access_token);

  // 设置令牌续期回调（可选）。设了之后，请求收到 401 会续期一次，
  // 并用**新令牌**重发同一个请求（最多重试一次，不循环）。
  void SetAccessTokenRefresher(const AccessTokenRefresher& refresher) {
    refresher_ = refresher;
  }

  // 推送。server_seq 输出本次推送后的服务端序号（没有条目时保持原值）。
  // accepted / skipped 输出服务端对每条目的处置（item_key 列表，可为空）。
  // http_status 输出本次推送的 HTTP 状态码（传输层失败 ⇒ 0）。
  bool Push(const std::vector<CloudSyncItem>& items,
            int64_t* server_seq,
            std::string* err,
            std::vector<std::string>* accepted = nullptr,
            std::vector<std::string>* skipped = nullptr,
            int* http_status = nullptr);

  // 拉取。since 是上次已处理的服务端序号。
  // has_more 输出「服务端还有更多条目」（用于分批）。
  bool Pull(int64_t since,
            int64_t limit,
            std::vector<CloudSyncItem>* out,
            int64_t* new_server_seq,
            std::string* err,
            bool* has_more = nullptr);

  // ★ spec 103 · BR-①g：桶级推送（POST /v1/sync/buckets/push）。
  // accepted / skipped 输出服务端对每个**桶**的处置（bucket_key 列表）。
  // device_id 客户端**一律不上报**（服务端与登录设备行主键比对，不等即 400）。
  bool PushBuckets(const std::vector<CloudBucketPushItem>& buckets,
                   int64_t* server_seq,
                   std::string* err,
                   std::vector<std::string>* accepted = nullptr,
                   std::vector<std::string>* skipped = nullptr,
                   int* http_status = nullptr);

  // ★ spec 103 · BR-①g：桶级拉取（GET /v1/sync/buckets/pull）。
  // cursor 是不透明续读位置（服务端给什么就原样回传）。
  bool PullBuckets(
      int64_t since,
      int64_t limit,
      const std::string& cursor,
      std::vector<CloudBucketPullItem>* out,
      int64_t* new_server_seq,
      std::string* err,
      bool* has_more = nullptr,
      std::string* next_cursor = nullptr,
      // ★ F3（relay-02of06）：HTTP 状态出参（传输层失败 ⇒ 0）。
      //   桶端点的状态码此前**不可观测** ⇒ 503 / 401 这类失败无法从
      //   诊断日志区分（批次 BR-①f 判据链的取证缺口）。
      int* http_status = nullptr);

  const std::string& access_token() const { return access_token_; }

  // ★ 批次 BM / ADR 0071：本实例（本轮同步）是否发生过「401 → 续期成功
  //   → 用新令牌重试」。
  //   账号页用它把「访问令牌已自动续期」写进本轮 message ——
  //   把批次 BL §13 的**强推断**升级为 1017 上**人眼可见的直接证据**。
  bool token_refreshed() const { return token_refreshed_; }

 private:
  bool SendJson(const std::string& method,
                const std::string& path,
                const std::string& body,
                int* status,
                std::string* response_body,
                std::string* err);

  ICloudTransport* transport_ = nullptr;
  std::string access_token_;
  AccessTokenRefresher refresher_;
  // 本实例是否发生过「401 → 续期成功」（批次 BM / ADR 0071）。
  bool token_refreshed_ = false;
};

// ---------------------------------------------------------------------------
// 端到端编排（后台线程调用）
// ---------------------------------------------------------------------------
//
// 一轮同步（ADR 0076 D2-5a-1 改序后的顺序）：
//   1) 导出本地词典；
//   2) 拉远端（按页分批，游标只增不减）；
//   3) 对账并分配 / 复用 HLC（R1 远端 HLC 沿用 / R2 库存未证实不推送 /
//      R4 分歧条目本地胜出）—— 必须在**拉完之后**：现行顺序是「先分配 HLC
//      后拉远端」⇒ 分配时看不到远端 ⇒ 侧车丢失后整库被盖上墙钟 HLC；
//   4) 合并（MergeEntries：同键 HLC 大者胜）；
//   5) 把合并结果回写本地（远端带来的新词 / 更新过的词条）；
//   6) 推送「本地改动过、且合并后仍是本地版本」的条目（分批）；
//   7) 更新 sidecar（只把「已同步」的条目标记为已同步；没推成功的保留
//      旧行，下一轮继续推）。
//
// 本地 HLC 存放在 sidecar 文件里（明文，不含任何密钥）：
//   第一行：#fluxing-cloud-sync-v1
//   第二行：seq\t<服务端序号>
//   其后每行：row\t<item_key>\t<ms>\t<counter>\t<device>\t<payload 的 sha256>
// 有了 payload 的哈希，本地行是否被改过就能判定；改过才分配新的 HLC，
// 否则本地永远盖过远端。
// 重置本地同步状态文件（sidecar）：覆盖成「版本行 + 游标归零」。
//
// 何时**必须**调用：用户点「清空云端数据」之后 —— 否则云端已清空、本地
// sidecar 仍标记「已同步」，本地词典再也传不上去（与
// CloudSyncCoordinator::ResetLocalSyncState 是同一条契约）。
//
// 为什么是自由函数而不只是协调器成员：协调器是**每轮同步现建现用**的
// （见 CloudAccountDialog.cpp 的 RunSyncRound），UI 侧没有长期持有的实例
// 可调；sidecar 是文件态的，重置语义只依赖落盘内容 ⇒ 抽成自由函数，
// 成员函数与 UI 复用同一份实现，不会各写一遍。
bool ResetCloudSyncSidecarFile(const std::string& sidecar_path,
                               std::string* err);

// sidecar 文本的序列化 / 解析（纯函数，便于单测与排障）。
// keyfp 行记录写出该 sidecar 的 enc_key 指纹：用户重置口令后 enc_key 变化，
// 旧 sidecar 里的「已同步」标记全部作废 —— 见 ParseSidecarText 的 key_rotated。
std::string BuildSidecarText(const std::vector<CloudSidecarRow>& rows,
                             int64_t server_seq,
                             const std::vector<uint8_t>& enc_key);

// 解析 sidecar 文本。key_rotated 为 true 时 rows / server_seq 已被清空
// （调用方据此把全部本地条目当新增重推）。
// keyfp 行缺失（老版本写的 sidecar）⇒ 有内容就判轮换；空 sidecar 不算。
// keyfp 为 "-"（ResetCloudSyncSidecarFile 写的「未绑定密钥」）⇒ 不轮换。
bool ParseSidecarText(const std::string& text,
                      const std::vector<uint8_t>& enc_key,
                      std::vector<CloudSidecarRow>* rows,
                      int64_t* server_seq,
                      bool* key_rotated,
                      std::string* err);

// ---------------------------------------------------------------------------
// ADR 0076 · D2-5a：远端 HLC 沿用（BR-①c）· 对账统计与观测口径
// ---------------------------------------------------------------------------
//
// ★ 计数口径（与 reports/fluxing-br-1c/impl-report.md 一致）：
//   scan_complete = 本轮拉取**扫完了整个云端库存**（has_more == false）。
//                   只拉不推的轮次也算「扫完」；只推不拉的轮次没有库存视图
//                   ⇒ 记 0（不谎称扫完）。
//   matched       = R1 命中条数：本地与远端同 item_key 且**明文 payload
//                   逐字节一致** ⇒ 沿用远端 HLC、不进推送集合（零写入主体）。
//   deferred      = R2 延后条数：既无侧车记录、又不在本轮远端集合里，且本轮
//                   拉取**没有扫完整个云端库存** ⇒ 库存未证实 ⇒ 本轮不推送。
//   divergent     = 本轮走「本地胜出 + 进推送集合」路径的分歧条数
//                   （同 item_key 但明文 payload 不同）。
//   pushed        = 本轮**被服务端接受**的条目数（accepted；skipped 不计）。
// ★ 批次 BR-①h：本轮**早退**原因码（闭集）。
//
// 缺陷（写放大排查中被实证）：对账诊断行由 SyncOnce **无条件**打印，而
// SyncRound 在 `local_entries` 计算点（DictRowsToEntries 成功）之前的任何早退
// 都让 stats 保持默认 0 ⇒ **早退轮与真空轮日志完全同形**（2026-11-24 实测：
// 拉取阶段 503 早退的两轮被读成「零推送」，真因是根本没走到对账段）。
// 数据本身不受影响（ADR 0078 三重护栏 + ADR 0076 D2-5a-9 语义指纹已覆盖），
// 缺陷纯粹在**观测面**：判据取证无法区分「没做」与「做了但为零」。
//
// 口径：诊断行**只在非 kNone 时**追加 ` early_exit=<code>` ⇒ 既有五字段逐字
// 不变（既有断言与真机探针期望值不受影响），且「行里没有 early_exit」就等于
// 「本轮已越过 local_entries 计算点」——无歧义。
enum class CloudSyncEarlyExit {
  // 未早退（本轮已越过 local_entries 计算点）
  kNone = 0,
  // 早退但未归因：新增早退分支漏打标时的**自曝**值（绝不伪装成真空轮）
  kUnattributed,
  // transport_ / backend_ 未初始化
  kNotReady,
  // access_token_ 为空
  kNotLoggedIn,
  // 维护闸门关闭（第一处检查：零 I/O、零配额消耗）
  kMaintenanceGate,
  // BeginMaintenance 失败且闸门已关闭（第二处检查：此前已读过侧车）
  kBeginMaintenanceGate,
  // BeginMaintenance 其它失败
  kBeginMaintenance,
  // 导出用户词典失败
  kExportUserDict,
  // 用户词典快照解析失败
  kParseUserDict,
  // 条目构造失败（local_entries 计算前的最后一处）
  kDictRowsToEntries,
  // 逐条端点拉取失败
  kPullFailed,
  // 拉取响应解密 / 形态失败
  kPullDecodeFailed,
  // 桶端点拉取失败
  kBucketPullFailed,
  // 侧车落盘失败
  kSidecarSaveFailed,
  // 退出用户词典维护模式失败
  kEndMaintenanceFailed,
  // 推送失败
  kPushFailed,
};

// 早退码 → 诊断行取值（**单一真相源**：诊断行与单测共用本函数，不复制第二份）。
inline const char* EarlyExitCodeText(CloudSyncEarlyExit code) {
  switch (code) {
    case CloudSyncEarlyExit::kNone:
      return "none";
    case CloudSyncEarlyExit::kUnattributed:
      return "unattributed";
    case CloudSyncEarlyExit::kNotReady:
      return "not_ready";
    case CloudSyncEarlyExit::kNotLoggedIn:
      return "not_logged_in";
    case CloudSyncEarlyExit::kMaintenanceGate:
      return "maintenance_gate";
    case CloudSyncEarlyExit::kBeginMaintenanceGate:
      return "begin_maintenance_gate";
    case CloudSyncEarlyExit::kBeginMaintenance:
      return "begin_maintenance";
    case CloudSyncEarlyExit::kExportUserDict:
      return "export_userdict";
    case CloudSyncEarlyExit::kParseUserDict:
      return "parse_userdict";
    case CloudSyncEarlyExit::kDictRowsToEntries:
      return "dictrows_to_entries";
    case CloudSyncEarlyExit::kPullFailed:
      return "pull_failed";
    case CloudSyncEarlyExit::kPullDecodeFailed:
      return "pull_decode_failed";
    case CloudSyncEarlyExit::kBucketPullFailed:
      return "bucket_pull_failed";
    case CloudSyncEarlyExit::kSidecarSaveFailed:
      return "sidecar_save_failed";
    case CloudSyncEarlyExit::kEndMaintenanceFailed:
      return "end_maintenance_failed";
    case CloudSyncEarlyExit::kPushFailed:
      return "push_failed";
  }
  // 闭集外（含将来新增枚举却忘了补文案）⇒ 自曝，不静默回落到 none。
  return "unattributed";
}

struct CloudReconcileStats {
  bool scan_complete = false;
  int matched = 0;
  int deferred = 0;
  int divergent = 0;
  int pushed = 0;
  // ★ BR-①h：本轮早退原因（默认 kNone = 未早退）
  CloudSyncEarlyExit early_exit = CloudSyncEarlyExit::kNone;
};

// 对账诊断行（ADR 0076 D2-5a-6 · R5 · 观测口径）：
//   reconcile scan_complete=<0|1> matched=<n> deferred=<n> divergent=<n>
//   pushed=<n>[ early_exit=<code>]
// ★ 逐字格式放在**生产头**里 ⇒ 单测断言的就是生产会写出的那一行（R7：
//   生产与单测共用一套实现，不复制第二份格式串）。
// ★ BR-①h：` early_exit=<code>` **仅早退轮**追加（kNone 不出现该字段）。
inline std::string FormatReconcileDiagLine(const CloudReconcileStats& stats) {
  std::string line = "reconcile scan_complete=" +
                     std::to_string(stats.scan_complete ? 1 : 0) +
                     " matched=" + std::to_string(stats.matched) +
                     " deferred=" + std::to_string(stats.deferred) +
                     " divergent=" + std::to_string(stats.divergent) +
                     " pushed=" + std::to_string(stats.pushed);
  if (stats.early_exit != CloudSyncEarlyExit::kNone) {
    line += " early_exit=";
    line += EarlyExitCodeText(stats.early_exit);
  }
  return line;
}

// ---------------------------------------------------------------------------
// spec 103 · BR-①g：桶化同步新增（侧车 v2 / 信封 / 桶级计数）
// ---------------------------------------------------------------------------
//
// ★ 侧车 v2（**新增**字段；老端点游标 seq 的语义与取值一律不变）：
//   #fluxing-cloud-sync-v2
//   seq\t<n>            ← 老端点游标（原义不变，**绝不归零**）
//   bseq\t<n>           ← 桶端点游标（编号空间独立，从 0 起）
//   phase\t<n>          ← 灰度阶段（0..4）
//   keyfp\t<fp|->
//   row\t...            ← 条目行（与 v1 逐字同格式）
//   brow\t<bucket_key>\t<ms>\t<counter>\t<device>\t<item_count>\t<fingerprint>
//
//   v1 文件用**同一套**解析器读 ⇒ phase=0、bseq=0、无 brow（AC-109 字面口径）。
//   ResetCloudSyncSidecarFile 保留 v1 版本行（既有单测断言），但**额外**把
//   bseq 与 phase 一并归零。

// 侧车 v2 的一行：桶键 + 桶级 HLC + 桶内条目数 + 桶内容指纹。
struct CloudSidecarBucketRow {
  std::string bucket_key;
  uint64_t ms = 0;
  uint32_t counter = 0;
  std::string device;
  size_t item_count = 0;
  std::string fingerprint;
};

// 侧车 v2 文本（序列化）。
std::string BuildSidecarTextV2(
    const std::vector<CloudSidecarRow>& rows,
    int64_t server_seq,
    int64_t bucket_seq,
    int bucket_phase,
    const std::vector<CloudSidecarBucketRow>& bucket_rows,
    const std::vector<uint8_t>& enc_key);

// 解析侧车文本（v1 / v2 通用）。
// · bucket_seq：桶端点游标（v1 ⇒ 0）；bucket_phase：灰度阶段（v1 ⇒ 0）。
// · key_rotated 为 true 时 rows / server_seq 已被清空（与 v1 语义逐字一致）。
bool ParseSidecarTextV2(const std::string& text,
                        const std::vector<uint8_t>& enc_key,
                        std::vector<CloudSidecarRow>* rows,
                        int64_t* server_seq,
                        int64_t* bucket_seq,
                        int* bucket_phase,
                        std::vector<CloudSidecarBucketRow>* bucket_rows,
                        bool* key_rotated,
                        std::string* err);

// 单条本地条目 → 信封（线上身份用 MAC 身份；明文键封在密文内部）。
bool SealEntryEnvelope(const Entry& entry,
                       ItemType type,
                       const std::vector<uint8_t>& enc_key,
                       Envelope* out,
                       std::string* err);

// 信封 → 待推送条目（线上形态与 SealEntries 逐字一致 ⇒ 可直接喂
// OpenItemsTolerant，与老端点同一条解密路径）。
bool CloudSyncItemFromEnvelope(const Envelope& env,
                               CloudSyncItem* out,
                               std::string* err);

// 本轮桶级计数（诊断行口径）。
struct CloudBucketRoundStats {
  int phase = 0;    // 本轮生效的灰度阶段（0 = 老端点单跑）
  int pushed = 0;   // 本轮被服务端**接受**的桶数
  int skipped = 0;  // 本轮被服务端跳过（HLC 守卫命中）的桶数
  int pulled_buckets = 0;  // 本轮拉到的桶数
  int pulled_items = 0;    // 本轮从桶里解出的条目数
  // ↓ 以下三个**不进诊断行**（诊断行格式逐字固定），只供单测与排障断言。
  int push_requests =
      0;  // 本轮发出的**桶级**推送请求数（AC-208 用它判「零请求」）
  int pull_requests = 0;  // 本轮发出的**桶级**拉取请求数
  int degraded = 0;  // 本轮降级（超限 ⇒ 改走逐条端点）的桶数
  // ★ spec 103 · BR-①g 补排轮（ADR 0077 D12 / handoff §M.8.4）：两条生产闸门的
  //   计数。**不进** FormatBucketDiagLine（既有五字段逐字不动 ⇒ AC-09 探针 v4
  //   的前缀正则不破），改由 FormatBucketQuotaLine 另打一行。
  int held_unproven = 0;  // 因「库存未证实」被整桶压住的桶数（规则 1）
  int held_quota = 0;     // 因「每轮配额用尽」被延后的桶数（规则 2）
  size_t rows_pushed = 0;  // 本轮**送出**的桶的行数估算（D1 写行口径）
  size_t rows_held = 0;    // 本轮**因配额延后**的桶的行数估算
};

// ★ spec 103 · BR-①g：单页桶数上限（服务端 MAX_PULL_LIMIT = 100）。
constexpr int64_t kBucketPullPageLimit = 100;

// ★ spec 103 · BR-①g：单轮桶拉取页数上限（AC-203 逐字口径）。
//   max(16, 词典数 × 11 + 2)，硬上限 32 页。
//   · 11 = ⌈1024 桶 / 100 桶每页⌉（每词典 1024 桶）；+2 是余量。
//   · 单词典 ⇒ 16 页；词典数 ≥ 3 ⇒ 触到 32
//   页硬上限（Q2：多词典可能扫不完库存）。
inline int BucketPullMaxPages(size_t dict_count) {
  int pages = 16;
  if (dict_count > 0) {
    const long long computed = static_cast<long long>(dict_count) * 11 + 2;
    pages = computed > 16 ? static_cast<int>(computed) : 16;
  }
  if (pages > 32)
    pages = 32;
  return pages;
}

// 桶级诊断行（逐字格式放在**生产头**里 ⇒ 单测断言的就是生产会写出的那一行）。
// 与既有 `reconcile ...` / `push batch items=.. elapsed_ms=..` 行并存。
inline std::string FormatBucketDiagLine(const CloudBucketRoundStats& s) {
  return "bucket phase=" + std::to_string(s.phase) +
         " pushed=" + std::to_string(s.pushed) +
         " skipped=" + std::to_string(s.skipped) +
         " pulled_buckets=" + std::to_string(s.pulled_buckets) +
         " pulled_items=" + std::to_string(s.pulled_items);
}

// ★ F3（relay-02of06 · 阶段 1 修复轮）：桶端点 **HTTP 状态**诊断行。
//   与既有 `bucket phase=` / `bucket quota ` 行并存 ⇒ 前缀不冲突
//   （AC-09 探针 v4 的 `bucket phase=` 前缀正则不破）。
//   传输层失败 ⇒ http=0（与 PushBuckets 的既有口径逐字一致）。
inline std::string FormatBucketPullDiagLine(int page,
                                            int http_status,
                                            int buckets,
                                            int items,
                                            bool has_more) {
  return "bucket pull page=" + std::to_string(page) +
         " http=" + std::to_string(http_status) +
         " buckets=" + std::to_string(buckets) +
         " items=" + std::to_string(items) +
         " has_more=" + (has_more ? "1" : "0");
}

inline std::string FormatBucketPushDiagLine(int page,
                                            int http_status,
                                            int buckets,
                                            int accepted,
                                            int skipped) {
  return "bucket push page=" + std::to_string(page) +
         " http=" + std::to_string(http_status) +
         " buckets=" + std::to_string(buckets) +
         " accepted=" + std::to_string(accepted) +
         " skipped=" + std::to_string(skipped);
}

// ★ 规则 1 v2（relay-02of06 · 阶段 1 修复轮）：桶空间的**真实库存证据**。
//
//   旧口径（legacy 编号空间的 unproven_keys）**恒不成立**：判定用的
//   inventory_complete 直接取自 bucket_scan_complete，而调用方只在
//   bucket_scan_complete 为真时才进 PushChangedBuckets ⇒ `!inventory_complete`
//   恒假 ⇒ 规则 1 在阶段 1/2 下**从不生效**（真机日志 held_unproven=0 实证；
//   handoff §P-B-2）。
//
//   新口径 = 「本轮拉到该桶、但**远端视图不完整**」：解出条数 != 服务端
//   item_count，或该桶出现畸形 / 解不开条目 ⇒ 整桶不推（held_unproven++）。
//   为什么证据必须取自**桶空间**：服务端按 bucket_id 先 DELETE 后 INSERT ⇒
//   未列出的远端条目会被静默删除 ⇒ 风险方向是「服务器有、我们没读到」；
//   legacy 空间的 unproven_keys 方向相反（「我们有、服务器可能没有」），
//   且两套编号空间互不参照（ADR 0077 / handoff §P-B-4）⇒ 不可混用。
inline bool BucketRemoteViewIncomplete(
    const std::set<std::string>& incomplete_buckets,
    const std::string& bucket_key) {
  return incomplete_buckets.find(bucket_key) != incomplete_buckets.end();
}

// ============================================================================
// ★ spec 103 · BR-①g 补排轮（handoff §M.8.4 规则 1 / 规则 2 · ADR 0077 D12）
//   生产闸门的**判定与算式**放在生产头里 ⇒ 单测断言的就是生产真正跑的东西
//   （与 FormatBucketDiagLine / BucketPullMaxPages 同一手法）。
// ============================================================================

// 每轮桶推行数配额（D1 写行口径）——**单一真相源**：实现常量 = 本注释 =
// ADR 0077 D29 = spec 103 AC-215，四处**逐字相等**（AC-215 的判据之一）。
// ★ 唯一值 = **2,000 行/轮**（1.3.10 真机轮实测生效值；批次 BR-①g）。
//   **已裁定**（2026-11-25，用户拍板采纳推荐值）⇒ 实现常量保持 2,000，
//   **未改常量、未重出包装机** ⇒ 与已验证的 1.3.10 运行时行为逐字一致。
//   落选候选（留档；改任一值都要**重出包装机**）：
//   · 4,096 —— 单词典全量补推 = 1,024 桶 = 64 分片 × 2 行 = 1,280 行，
//     删除批次再 + 最多 1,024 行 ⇒ ≈2,304 行 ⇒ 一轮扫完一个词典的全量补推；
//   · 8,000 —— handoff §M.8.4 / §O-1.1 的建议值，导入更快但单轮写入上界更高。
//   三者共同的硬约束（改任一值都要**重出包装机**）：
//   · 生产写预算护栏已定为 **每账号 20,000 行/日**（handoff §O-1.3）⇒
//     2,000 行/轮 ⇒ 一天最多 10 轮触到配额上限，护栏不会被桶推送单独击穿；
//   · D1 免费额度是**每个数据库 100,000 行/日**（全局口径，非每账号）⇒
//     留出的余量要覆盖真实用户，客户端闸门是第一道防线。
// ⚠ 配额只约束**本轮推送行数**；稳态零写靠「桶指纹不变 ⇒ 不推」，而桶指纹
//   （brow）目前**不落盘**（handoff §P-H.4）⇒ 现网实测写入量级远高于本配额的
//   设计意图；修好 brow 落盘后，本配额才回到「首导 / 补偿」的定位。
constexpr size_t kBucketPushRowsPerRound = 2000;

// 规则 1 的**桶级压制判定**（handoff §M.8.4 规则 1）：桶内只要含**任一**未证实
// 条目 ⇒ 整桶不推、侧车桶行不更新 ⇒ 下一轮仍判 changed ⇒ 自然续推。
// 压制粒度取「整桶」而非「剔除单条」：桶是**整桶覆盖**的权威写（服务端按
// bucket_id 先 DELETE 后 INSERT），只剔除一条仍可能把本轮没拉到的远端条目抹掉。
// unproven_keys 口径 = 本轮判定为 deferred 的条目**身份键集合**（Entry.key，
// std::string）；库存已证实时集合为空 ⇒ 行为与本闸门之前逐字一致。
// 墓碑（删除条目）不走逐键比对：其 item_key 是词典明文，与 64 位十六进制身份键
// 不同属同一空间 ⇒ 库存未证实时由调用方整组压住（deleted.clear()）。
inline bool BucketHasUnprovenEntry(
    const std::set<std::string>& unproven_keys,
    const std::vector<Entry>& entries) {
  if (unproven_keys.empty())
    return false;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (unproven_keys.count(entries[i].key) != 0)
      return true;
  }
  return false;
}

// 一个桶批次预计的 D1 **行写入数**（与规则 2 的配额同口径）：服务端每分片
// 一次 DELETE+INSERT 计 2 行；墓碑按整桶一次 DELETE+INSERT 再计 2 行。
inline size_t EstimateBucketWriteRows(size_t shard_count, bool has_deletions) {
  return shard_count * 2 + (has_deletions ? 2 : 0);
}

// 规则 2「每轮桶推行数配额」的**选择算式**：按候选顺序（= 桶序，稳定可复现）
// 逐个累加预计行数，**一旦某个桶批次会超出配额就停下**（后续桶一律延后，
// 不做插队重排，⇒ 续传顺序与探针可预测）。返回被选中的下标。
//   · rows_in_order：与候选桶同序的每桶预计行数；
//   · quota_rows：本轮配额（0 = 不限）；
//   · rows_used：出参，选中桶的行数合计。
inline std::vector<size_t> SelectBucketsWithinQuota(
    const std::vector<size_t>& rows_in_order, size_t quota_rows,
    size_t* rows_used) {
  std::vector<size_t> picked;
  size_t used = 0;
  for (size_t i = 0; i < rows_in_order.size(); ++i) {
    if (quota_rows != 0 && used + rows_in_order[i] > quota_rows)
      break;
    used += rows_in_order[i];
    picked.push_back(i);
  }
  if (rows_used)
    *rows_used = used;
  return picked;
}

// 两条闸门的诊断行（**无条件打印**，即使 pushed=0）：复跑探针要靠它区分
// 「因未证实被压住」/「因配额被延后」/「因无改动零写」这三种零写。
// 与 FormatBucketDiagLine 一样放在生产头里 ⇒ 单测断言生产写出的那一行。
inline std::string FormatBucketQuotaLine(int held_unproven, int held_quota,
                                         size_t rows_pushed, size_t rows_held,
                                         size_t quota_rows) {
  return "bucket quota held_unproven=" + std::to_string(held_unproven) +
         " held_quota=" + std::to_string(held_quota) +
         " rows_pushed=" + std::to_string(rows_pushed) +
         " rows_held=" + std::to_string(rows_held) +
         " quota_rows=" + std::to_string(quota_rows);
}

class CloudSyncCoordinator {
 public:
  CloudSyncCoordinator(ICloudTransport* transport,
                       IUserDictBackend* backend,
                       std::string access_token,
                       std::string device_id,
                       std::vector<uint8_t> enc_key,
                       std::string hlc_sidecar_path);
  ~CloudSyncCoordinator();

  // 同 CloudSyncService：设置令牌续期回调（可选），透传给内部创建的
  // CloudSyncService（推 / 拉各一个）⇒ 同步请求遇到 401 时续期并重试。
  void SetAccessTokenRefresher(const AccessTokenRefresher& refresher) {
    refresher_ = refresher;
  }

  // 推 + 拉一轮（旧接口，保留兼容）。
  bool SyncOnce(std::string* err);

  // 一轮同步（可指定方向）。result 回报本轮实际发出的请求数与条目数。
  bool SyncOnce(SyncDirection direction,
                CloudSyncRoundResult* result,
                std::string* err);

  // 清空本地同步状态（sidecar 里的行与游标）：下次同步会把全部本地条目
  // 当作「新增」重新推上去。
  // 用户点「清空云端数据」后**必须**调用 —— 否则云端已清空、本地 sidecar
  // 仍标记「已同步」，本地词典再也传不上去。
  bool ResetLocalSyncState(std::string* err);

  int64_t last_server_seq() const { return last_server_seq_; }
  const std::string& device_id() const { return device_id_; }
  // 本轮的**对账计数**（ADR 0076 D2-5a-6 · R5 观测口径）。
  // 每轮 SyncOnce 结束都会刷新（失败也刷新 ⇒ 与诊断行同源）⇒ 单测可直接
  // 断言「生产会写出的那一行」里的四个计数。
  const CloudReconcileStats& last_reconcile_stats() const {
    return last_stats_;
  }

  // ★ spec 103 · BR-①g：本轮的**桶级计数**（ADR 0077 观测口径）。
  //   每轮 SyncOnce 结束都会刷新（失败也刷新 ⇒ 与诊断行同源）。
  const CloudBucketRoundStats& last_bucket_stats() const {
    return last_bucket_stats_;
  }

 private:
  bool LoadSidecar(std::string* err);
  bool SaveSidecar(std::string* err);
  // 一轮同步的**主体**（ADR 0076 D2-5a-1 改序后仍是同一段流程）。
  // stats 收集本轮对账计数（R5 观测口径）；由 SyncOnce 在轮次结束后
  // **无条件**打一条诊断行（本轮失败也照打）。
  bool SyncRound(SyncDirection direction,
                 CloudSyncRoundResult* result,
                 std::string* err,
                 CloudReconcileStats* stats);

  // ★ spec 103 · BR-①g：桶级拉取（分页 → 解包 → **复用逐条端点同一条解密
  //   路径 OpenItemsTolerant** → 并入远端视图）。
  // · dict_count：本地词典数（决定单轮页数上限，AC-203）。
  // · has_more：服务端还有更多桶（⇒ 库存未扫完 ⇒ 本轮不推桶）。
  // · 桶游标只从**拉取响应**推进（推送返回的 server_seq 不推进它）。
  bool PullBucketsIntoRemote(
      size_t dict_count,
      std::vector<Entry>* remote_entries,
      bool* has_more,
      CloudBucketRoundStats* stats,
      int* undecryptable,
      int* malformed,
      // ★ 规则 1 v2（relay-02of06）：本轮**远端视图不完整**的
      //   桶键集合（桶空间证据 ⇒ 与逐条端点的编号空间无关）。
      std::set<std::string>* incomplete_buckets,
      std::string* err);

  // ★ spec 103 · BR-①g：桶级推送（分桶 → 语义指纹 → **只推变化桶** → 写桶行）。
  // · merged：合并后的本地视图全集（桶内容 = 它的分组，FR-206）。
  // · remote_complete：本轮库存扫描是否完整（FR-215 判定的前提）。
  // · legacy_endpoint_active：逐条端点是否仍可用（超限桶降级时用）。
  // · incomplete_buckets：★ 规则 1 v2 的**桶空间库存证据**（视图不完整的桶 ⇒
  //   整桶不推）。取代旧口径 unproven_keys（legacy 编号空间 ⇒ 恒不成立）。
  bool PushChangedBuckets(const std::vector<Entry>& merged,
                          const std::vector<Entry>& remote_entries,
                          const std::set<std::string>& incomplete_buckets,
                          bool remote_complete,
                          bool legacy_endpoint_active,
                          CloudBucketRoundStats* stats,
                          CloudSyncRoundResult* result,
                          std::string* err);

  ICloudTransport* transport_ = nullptr;
  IUserDictBackend* backend_ = nullptr;
  std::string access_token_;
  std::string device_id_;
  std::vector<uint8_t> enc_key_;
  std::string sidecar_path_;
  // 令牌续期回调（可选）：透传给内部创建的 CloudSyncService。
  AccessTokenRefresher refresher_;
  int64_t last_server_seq_ = 0;
  HlcClock clock_;
  // item_key → (ms, counter, device, payload 的 sha256)
  std::vector<CloudSidecarRow> sidecar_rows_;
  // 本轮 LoadSidecar 是否判定「enc_key 已变化 ⇒ sidecar 作废」
  bool key_rotated_ = false;
  // 最近一轮的对账计数（R5 观测口径；失败轮也会刷新）。
  CloudReconcileStats last_stats_;
  // ★ spec 103 · BR-①g：桶端点游标（与老端点游标 last_server_seq_
  //   **编号空间独立**：从 0 起，不沿用老游标，也绝不归零老游标）。
  int64_t bucket_seq_ = 0;
  // 灰度阶段（0 = 老端点单跑 ⇒ 行为与本批之前逐字一致）。
  CloudBucketPhase bucket_phase_ = CloudBucketPhase::kLegacy;
  // 本轮生效阶段是否来自环境变量（来自环境变量 ⇒ 本轮结束写回侧车 ⇒
  // 回滚手段 = 把环境变量设成 0 再跑一轮）。
  bool bucket_phase_from_env_ = false;
  // 侧车里的桶行（bucket_key → 桶级 HLC + 桶内容指纹）。
  std::vector<CloudSidecarBucketRow> bucket_rows_;
  // 侧车文件是不是 v2 格式（决定这轮写 v1 还是 v2）。
  bool sidecar_v2_ = false;
  // 最近一轮的桶级计数（失败轮也会刷新 ⇒ 与诊断行同源）。
  CloudBucketRoundStats last_bucket_stats_;
};

}  // namespace cloud
}  // namespace fluxing
