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
};

// ---------------------------------------------------------------------------
// 纯函数（不依赖引擎，可单测）
// ---------------------------------------------------------------------------

// 解析快照文本（backup_user_dict 的产物，可能串联了多个词典）。
// 空行与 '#' 注释行被跳过；'#@/db_name' 决定当前段属于哪个词典；
// '#@/tick' 记录库级 tick。失败 ⇒ false + err 中文原因。
bool ParseUserDictSnapshots(const std::string& text,
                            std::vector<UserDictSnapshot>* out,
                            std::string* err);

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
std::string JoinDictRow(const CloudDictEntry& entry);

// item_key 拼装 / 拆解：item_key = "<词典名>\t<词条>"。
std::string MakeDictItemKey(const std::string& dict, const std::string& phrase);
bool SplitDictItemKey(const std::string& item_key,
                      std::string* dict,
                      std::string* phrase);

// payload 拆解：payload = "<code>\t<value>"。
bool SplitDictPayload(const std::string& payload,
                      std::string* code,
                      std::string* value);

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
