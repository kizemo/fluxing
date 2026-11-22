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
#include <string>
#include <vector>

#include "../../include/FluxingCloudEnvelope.h"

#include "CloudJson.h"
#include "CloudSyncEngine.h"
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
  bool Push(const std::vector<CloudSyncItem>& items,
            int64_t* server_seq,
            std::string* err,
            std::vector<std::string>* accepted = nullptr,
            std::vector<std::string>* skipped = nullptr);

  // 拉取。since 是上次已处理的服务端序号。
  // has_more 输出「服务端还有更多条目」（用于分批）。
  bool Pull(int64_t since,
            int64_t limit,
            std::vector<CloudSyncItem>* out,
            int64_t* new_server_seq,
            std::string* err,
            bool* has_more = nullptr);

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
// 一轮同步（阶段 F 之后的顺序）：
//   1) 导出本地词典 → 给每行分配 / 复用 HLC（sidecar 哈希变了 ⇒ 新的 HLC，
//      并标记为「本地改动」）；
//   2) 拉远端（按页分批，游标只增不减）；
//   3) 合并（MergeEntries：同键 HLC 大者胜）；
//   4) 把合并结果回写本地（远端带来的新词 / 更新过的词条）；
//   5) 推送「本地改动过、且合并后仍是本地版本」的条目（分批）；
//   6) 更新 sidecar（只把「已同步」的条目标记为已同步；没推成功的保留
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
bool ResetCloudSyncSidecarFile(const std::string& sidecar_path, std::string* err);

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

 private:
  bool LoadSidecar(std::string* err);
  bool SaveSidecar(std::string* err);

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
};

}  // namespace cloud
}  // namespace fluxing
