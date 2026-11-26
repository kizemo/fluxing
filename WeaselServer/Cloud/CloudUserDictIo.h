#pragma once
//
// spec 100 阶段 D · T305 用户词典的导出 / 导入（与引擎互斥）
//
// ★ 传输通道的选择（T102 取证结论，真相源：
//   docs/research/fluxing-cloud-2026-11/userdict-merge-semantics.md）
//
//   **不用** `export_user_dict` + `import_user_dict`：
//     ① export 只导 commits，dee 与逐条 tick 全部丢失；
//     ② export 跳过墓碑 ⇒ 删除无法跨设备传播；
//     ③ import 不写 tick，还把 -1 放大成 -2、把 0 写成垃圾条目；
//     ④ import 对不存在的库静默建库（探测即污染）。
//   **用** `backup_user_dict` + `restore_user_dict`（内部即 UserDbMerger）：
//   快照是完整列族（含墓碑 / dee / 逐条 tick / /tick），restore 按
//   「abs(commits) 择优、dee 取 max、tick 抬升」合并，且幂等。
//
// 因此本模块的行格式 = **快照行**（不是 export 的 TSV）：
//     <code><空格><TAB><phrase><TAB>c=<n> d=<x> t=<tick>
//   快照头（每个词典一段）：
//     # Rime user dictionary
//     #@/db_name<TAB><词典名>
//     #@/db_type<TAB>userdb
//     #@/tick<TAB><库级 tick>
//   多个词典的快照文本可以**串联**（还原时按 #@/db_name 分段）。
//
// 云端条目形状（T304 逐条目 HLC 合并的最小单位）：
//     item_key = "<词典名>\t<词条>"
//     payload  = "<code>\t<c=.. d=.. t=..>"
//
// 红线（CLAUDE.md · 宪法 P2）：
//   · 备份 / 还原必须裹在维护态（StartMaintenance / EndMaintenance）之间，
//     否则 leveldb 拿不到锁（LOCK 失败）。
//   · 不得在 TSF 回调 / UI 线程做 ⇒ 所有入口都是阻塞调用，由后台线程驱动。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {

// 一条词典行：item_key + 载荷。
//   text    = "<词典名>\t<词条>"
//   payload = "<code>\t<c=.. d=.. t=..>"
struct CloudDictEntry {
  std::string text;
  std::string payload;
};

// 一个词典的快照（备份通道的产物）。
struct UserDictSnapshot {
  std::string dict;   // 词典名（快照头 #@/db_name）
  uint64_t tick = 0;  // 快照头 #@/tick（库级 tick）
  std::vector<CloudDictEntry> rows;
};

// ---------------------------------------------------------------------------
// 引擎后端抽象
// ---------------------------------------------------------------------------

// WeaselServer 里用 rime levers 实现（CloudRimeUserDictBackend.cpp）；
// 单测用假实现。
class IUserDictBackend {
 public:
  virtual ~IUserDictBackend() = default;

  // 进入 / 退出维护模式（锁住用户库）。必须成对调用。
  virtual bool BeginMaintenance(std::string* err) = 0;
  virtual bool EndMaintenance(std::string* err) = 0;

  // 导出全部用户词典的快照文本（多词典时串联）。
  virtual bool ExportUserDict(std::string* out, std::string* err) = 0;

  // 还原快照文本（可含多个词典的快照段）。返回被还原的词典数。
  virtual bool ImportUserDict(const std::string& text,
                              int* imported,
                              std::string* err) = 0;

  // ADR 0062: 维护准入闸门。
  //
  // 语义：**没有活跃输入会话**时才允许进入维护态（finalize 会清空会话表
  // => 客户端缓存的 ipc_id 变成孤儿 => 按键被吞）。
  //
  // 默认实现放行：单测用的假后端不必关心会话；生产实现
  // （CloudRimeUserDictBackend）转问 RimeWithWeaselHandler::
  // CanEnterMaintenance。
  // 只读、无 I/O、不加锁（宪法 P2）=> 可从同步后台线程调用。
  virtual bool CheckMaintenanceGate(std::string* reason) const {
    if (reason != nullptr)
      reason->clear();
    return true;
  }
};

// ---------------------------------------------------------------------------
// ADR 0062 · 用户可见文案（简体中文）
//
// ★ 只放在本头的原因：RimeWithWeasel.vcxproj **没有** /utf-8，
//   窄字符串里的中文在那个编译单元里会乱码；本头只被 WeaselServer 与
//   TestFluxingCloud 编译（两者都有 /utf-8）。
// ---------------------------------------------------------------------------

// 闸门关闭时的用户可见提示（ADR 0062 决策 2）。
inline const char* MaintenanceGateSkipMessage() {
  return "你正在输入，本轮已跳过用户词典同步，稍后会自动重试。";
}

// 手动「立即同步」被闸门拦下时的指引（ADR 0062 决策 3）。
inline const char* MaintenanceGateManualHint() {
  return "请先切换到其他窗口，稍后会自动完成同步。";
}

// 「机会性同步」的用户可见口径（ADR 0062 决策 5）—— 账号页静态说明行。
inline const char* MaintenanceGateOpportunisticStatement() {
  return "你正在输入时同步会自动延后，停止输入后自动完成。";
}

// ---------------------------------------------------------------------------
// 纯函数（不依赖引擎，可单测）
// ---------------------------------------------------------------------------

// 解析快照文本（backup_user_dict 的产物，可能串联了多个词典）。
// 空行与 '#' 注释行被跳过；'#@/db_name' 决定当前段属于哪个词典；
// '#@/tick' 记录库级 tick。失败 ⇒ false + err 中文原因。
bool ParseUserDictSnapshots(const std::string& text,
                            std::vector<UserDictSnapshot>* out,
                            std::string* err);

// ★ ADR 0078 D2/D3：统计快照文本里的**有效数据行**条数。
//
// 口径**完全复用** ParseUserDictSnapshots（单一真相源 · 宪法 R7）：
//   · '#' 注释行 / 空行不算；
//   · 形态非法的行不算（IsValidUserDictEntryShape 已判过）；
//   · 文本里连一个 '#@/db_name' 段都没有 ⇒ 0 行。
//
// 用途（两处护栏的判据）：
//   · 导出侧：备份文本 0 数据行 ⇒ 该词典的备份是空的 ⇒ 不算导出成功；
//   · 还原侧：某一段 0 数据行 ⇒ 拒绝把这一段送进 rime 的 restore。
std::size_t CountUserDictDataRows(const std::string& text);

// ★ ADR 0078 D2/D3：把「若干段文本」过滤成「至少含一条数据行的段」。
//
// 导出侧（逐词典的备份文本）与还原侧（逐词典的快照段）**共用这一份口径**
// （单一真相源 · 宪法 R7）：判据就是 CountUserDictDataRows。
//   · out     = 保留下来的段（保持原顺序，调用方按序使用）；
//   · skipped = 被丢弃的段数（0 数据行 ⇒ 空 / 退化快照）；
//   · 返回值 = 是否至少留下了一段（false ⇒ 调用方应判失败）。
bool FilterUserDictTextsByContent(const std::vector<std::string>& texts,
                                  std::vector<std::string>* out,
                                  std::size_t* skipped);

// 由条目集合重建快照文本（供 restore_user_dict 用）。
// 同一个词典的条目会归到同一段；段的 tick 取该词典条目里的最大 tick。
bool BuildUserDictSnapshots(const std::vector<CloudDictEntry>& rows,
                            std::string* out,
                            std::string* err);

// 把串联的多词典快照文本按 "#@/db_name" 切成一段一段。
// 每段都从自己的 "#@/db_name" 行开始（restore_user_dict 靠它定位目标词典），
// 段内保留原有的 '#@/tick' 等元信息行。
bool SplitUserDictSnapshotSections(const std::string& text,
                                   std::vector<std::string>* sections,
                                   std::string* err);

// 从「一行快照」还原出「词条 + 其余列」。
// 形如 "<code>\t<phrase>\t<value>" ⇒ text="<词典名>\t<phrase>"。
bool SplitDictRow(const std::string& row,
                  const std::string& dict,
                  CloudDictEntry* out,
                  std::string* err);

// 从「词条 + 其余列」还原出「一行快照」。
// 形态非法 ⇒ false + err 中文原因。
//
// ★ 批次 BM / ADR 0071：旧签名在形态非法时返回**空串**，调用方会把它写成
//   **空行**（静默丢数据）。该路径在当前调用链上**不可达**（写回阶段第一轮
//   已 fail loud），但它仍是**公开 API 层面的陷阱** ⇒ 改成显式报错。
bool JoinDictRow(const CloudDictEntry& entry,
                 std::string* out,
                 std::string* err);

// item_key 拼装 / 拆解：item_key = "<词典名>\t<词条>"。
std::string MakeDictItemKey(const std::string& dict, const std::string& phrase);
bool SplitDictItemKey(const std::string& item_key,
                      std::string* dict,
                      std::string* phrase);

// payload 拆解：payload = "<code>\t<value>"。
bool SplitDictPayload(const std::string& payload,
                      std::string* code,
                      std::string* value);

// 判定一条「用户词典条目」的形态是否合法：
//   item_key = "<词典名>\t<词条>"   （可被 SplitDictItemKey 拆开）
//   payload  = "<code>\t<value>"     （可被 SplitDictPayload 拆开）
//
// ★ 单一真相源（宪法 R7 · 批次 BM / ADR 0071）：写回阶段
//   BuildUserDictSnapshots 的合法性判定就是这两条拆解，所以**开箱侧**
//   （云端条目）与**解析侧**（本地快照）必须复用本函数 ——
//   否则「谁能进合并集」与「谁能把合并集写回去」两套口径漂移，
//   脏数据又会重新毒化整轮同步（BM 的结构性根因）。
bool IsValidUserDictEntryShape(const std::string& item_key,
                               const std::string& payload);

// ---------------------------------------------------------------------------
// 维护态钩子
// ---------------------------------------------------------------------------

// 云同步需要与「服务端持有的 rime 实例」互斥：备份 / 还原用户词典时必须
// 先让服务端 rime 释放用户库（否则 leveldb LOCK 失败），结束后再恢复。
//
// 维护态必须经由 RimeWithWeaselHandler（它会一并清掉会话表），而 handler
// 归 WeaselServerApp 所有 ⇒ 这里用一对函数指针把「开始 / 结束维护态」
// 注入给云模块。未注册时云同步会明确失败（而不是默默跳过互斥）。
using CloudMaintenanceFn = std::function<bool(std::string*)>;
void SetCloudMaintenanceHooks(CloudMaintenanceFn begin, CloudMaintenanceFn end);
bool HasCloudMaintenanceHooks();
bool CallCloudBeginMaintenance(std::string* err);
bool CallCloudEndMaintenance(std::string* err);

// ---------------------------------------------------------------------------
// rime 后端（WeaselServer 专用）
// ---------------------------------------------------------------------------

// 创建基于 rime levers 的用户词典后端。
// 实现见 CloudRimeUserDictBackend.cpp —— 该文件**只进 WeaselServer 工程**，
// 不进单测工程（单测用假实现，见 test/TestFluxingCloud）。
std::unique_ptr<IUserDictBackend> CreateRimeUserDictBackend();

}  // namespace cloud
}  // namespace fluxing
