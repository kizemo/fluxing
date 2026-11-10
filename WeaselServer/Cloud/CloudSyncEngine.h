#pragma once
//
// spec 100 阶段 F · T501/T504 同步引擎（调度 + 触发矩阵 + 退避 + 配额）
//
// 职责：决定「什么时候同步、同步哪一头、失败后多久再试、今天还能同步几次」。
// **不持有任何凭据、不认识端点** —— 真正的推拉由注入的 SyncRunner 完成。
// 因此引擎可以脱离网络与账号状态单测（宪法 R7：测试与生产同源）。
//
// 触发矩阵（真相源：plan.md §7）：
//   启动    → 拉一次（不阻塞启动，失败静默）；**这一轮只拉不推**
//   本地改动 → 防抖 5 分钟后推；推送全局间隔不小于 15 分钟
//   定时    → 每 60 分钟一轮（推 + 拉；推送集合按 sidecar 差分算出，
//             没有本地改动时不会发出推送请求）
//   手动    → 「立即同步」推 + 拉
//   失败    → 退避 1 / 5 / 15 / 60 分钟，最多 4 次后交回定时触发
//   断网    → 不排队累积；恢复后按防抖窗口补推
//   配额    → 每日推 100 次 / 拉 50 次；用尽 ⇒ 跳过（不是错误）
//
// 线程模型（宪法 P2）：
//   · 引擎自带一个后台线程，事件唤醒 + 定时唤醒，**无轮询**；
//   · 所有网络 I/O 都在这个后台线程里发生，UI 线程 / TSF 回调永不阻塞；
//   · 状态读写用互斥量保护，status() 可从 UI 线程安全调用。
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

namespace fluxing {
namespace cloud {

// ---------------------------------------------------------------------------
// 枚举与结果
// ---------------------------------------------------------------------------

// 同步方向。
enum class SyncDirection {
  kPullOnly = 0,  // 只拉远端
  kPushOnly,      // 只推本地改动
  kBoth,          // 推 + 拉
};

// 触发来源（状态展示 + 单测断言用）。
enum class SyncTrigger {
  kNone = 0,
  kStartup,      // 启动后拉一次
  kLocalChange,  // 本地词典改动（防抖窗口后）
  kPeriodic,     // 定时拉取
  kManual,       // 用户点「立即同步」
  kRetry,        // 失败后的退避重试
};

// 一轮同步的结果。
enum class SyncOutcome {
  kNever = 0,  // 还没同步过
  kSuccess,
  kFailed,
  kSkipped,  // 配额用尽等被跳过（**不是**错误）
};

struct CloudSyncRoundResult {
  bool ok = false;
  int push_requests = 0;  // 实际发出的推送请求数
  int pull_requests = 0;  // 实际发出的拉取请求数
  int items_pushed = 0;
  int items_pulled = 0;
  bool has_more = false;  // 服务端还有更多条目（分批未取完）
  std::string message;    // 中文说明
};

// 对外可读的状态快照（UI 线程安全读取）。
struct CloudSyncStatus {
  bool ever_synced = false;
  int64_t last_sync_ms = 0;  // Unix 毫秒
  SyncOutcome last_outcome = SyncOutcome::kNever;
  SyncTrigger last_trigger = SyncTrigger::kNone;
  std::string last_message;  // 中文
  int push_count_today = 0;
  int pull_count_today = 0;
  int backoff_step = 0;         // 0 = 无退避
  int64_t next_attempt_ms = 0;  // 下次尝试的 Unix 毫秒（估计）
  bool running = false;
  bool enabled = false;
};

// 调度参数。默认值 = plan.md §7 的触发矩阵。
struct CloudSyncEngineOptions {
  int64_t startup_delay_ms = 0;
  int64_t local_change_debounce_ms = 5 * 60 * 1000;  // 本地改动防抖 5 分钟
  int64_t min_push_interval_ms = 15 * 60 * 1000;  // 推送全局间隔 15 分钟
  int64_t periodic_pull_interval_ms = 60 * 60 * 1000;  // 定时拉取 60 分钟
  std::vector<int64_t> backoff_ladder_ms = {           // 退避阶梯
      60 * 1000, 5 * 60 * 1000, 15 * 60 * 1000, 60 * 60 * 1000};
  int max_backoff_attempts = 4;  // 最多退避重试 4 次
  int daily_push_limit = 100;
  int daily_pull_limit = 50;
};

// ---------------------------------------------------------------------------
// 纯函数（不联网、不起线程，可直接单测）
// ---------------------------------------------------------------------------

// 第 n 次失败的退避时长（n 从 1 起）。超出阶梯长度就取最后一档。
inline int64_t BackoffDelayMs(const CloudSyncEngineOptions& options,
                              int attempt) {
  if (options.backoff_ladder_ms.empty())
    return 0;
  int index = attempt - 1;
  if (index < 0)
    index = 0;
  if (index >= static_cast<int>(options.backoff_ladder_ms.size()))
    index = static_cast<int>(options.backoff_ladder_ms.size()) - 1;
  return options.backoff_ladder_ms[static_cast<size_t>(index)];
}

// 今日推送次数是否还有余额。
inline bool DailyPushAllowed(const CloudSyncEngineOptions& options,
                             int count_today) {
  if (options.daily_push_limit <= 0)
    return false;
  return count_today < options.daily_push_limit;
}

// 今日拉取次数是否还有余额。
inline bool DailyPullAllowed(const CloudSyncEngineOptions& options,
                             int count_today) {
  if (options.daily_pull_limit <= 0)
    return false;
  return count_today < options.daily_pull_limit;
}

// Unix 毫秒 → UTC 日序号（自然日翻篇用）。
inline int64_t DayIndexUtc(int64_t unix_ms) {
  return unix_ms / (24 * 60 * 60 * 1000);
}

// 下一个 UTC 自然日的起点（Unix 毫秒）。
inline int64_t DayRolloverMs(int64_t unix_ms) {
  return (DayIndexUtc(unix_ms) + 1) * (24 * 60 * 60 * 1000);
}

// 本地改动 ⇒ 最早可推送时刻 = max(防抖窗口到期, 推送间隔到期)。
// dirty_since_ms <= 0 表示没有待推的本地改动 ⇒ false。
inline bool ComputePushDeadline(const CloudSyncEngineOptions& options,
                                int64_t dirty_since_ms,
                                int64_t last_push_ms,
                                int64_t* out_deadline_ms) {
  if (dirty_since_ms <= 0)
    return false;
  int64_t deadline = dirty_since_ms + options.local_change_debounce_ms;
  const int64_t throttle = last_push_ms + options.min_push_interval_ms;
  if (throttle > deadline)
    deadline = throttle;
  if (out_deadline_ms != nullptr)
    *out_deadline_ms = deadline;
  return true;
}

// 调度器给出的下一步动作。
enum class SyncAction {
  kWait = 0,          // 什么都没到期
  kRunPull,           // 该拉
  kRunPush,           // 该推
  kRunBoth,           // 该推 + 拉
  kQuotaExceeded,     // 到点了但今日配额用尽 ⇒ 等自然日翻篇
  kBackoffExhausted,  // 退避重试用尽 ⇒ 交回定时触发
};

// 决策输入（由引擎每轮组装，纯函数据此判定）。
struct SyncScheduleInput {
  int64_t now_ms = 0;          // 单调钟毫秒
  int64_t dirty_since_ms = 0;  // 待推本地改动的起始时刻（0 = 无）
  int64_t last_push_ms = 0;
  int64_t last_pull_ms = 0;
  int64_t startup_at_ms = 0;  // 启动任务的到期时刻
  bool startup_pending = false;
  int64_t backoff_until_ms = 0;
  int backoff_step = 0;
  int push_count_today = 0;
  int pull_count_today = 0;
  bool manual_requested = false;
};

// 触发优先级：手动 > 退避重试 > 启动 > 本地改动推送 > 定时拉取。
inline SyncAction DecideNextAction(const CloudSyncEngineOptions& options,
                                   const SyncScheduleInput& in) {
  // 手动同步最高优先，且**不受每日配额限制**（用户显式发起）。
  if (in.manual_requested)
    return SyncAction::kRunBoth;

  if (in.backoff_step > 0) {
    if (in.now_ms < in.backoff_until_ms)
      return SyncAction::kWait;  // 还在退避窗口里
    if (in.backoff_step >= options.max_backoff_attempts)
      return SyncAction::kBackoffExhausted;
    return SyncAction::kRunBoth;  // 退避到期 ⇒ 重试
  }

  bool want_push = false;
  bool want_pull = false;
  int64_t push_deadline = 0;
  if (ComputePushDeadline(options, in.dirty_since_ms, in.last_push_ms,
                          &push_deadline) &&
      in.now_ms >= push_deadline) {
    want_push = true;
  }
  if (in.startup_pending && in.now_ms >= in.startup_at_ms)
    want_pull = true;
  if (in.now_ms >= in.last_pull_ms + options.periodic_pull_interval_ms)
    want_pull = true;

  if (!want_push && !want_pull)
    return SyncAction::kWait;

  bool blocked = false;
  if (want_push && !DailyPushAllowed(options, in.push_count_today)) {
    want_push = false;
    blocked = true;
  }
  if (want_pull && !DailyPullAllowed(options, in.pull_count_today)) {
    want_pull = false;
    blocked = true;
  }
  if (!want_push && !want_pull)
    return blocked ? SyncAction::kQuotaExceeded : SyncAction::kWait;
  if (want_push && want_pull)
    return SyncAction::kRunBoth;
  if (want_push)
    return SyncAction::kRunPush;
  return SyncAction::kRunPull;
}

// ---------------------------------------------------------------------------
// 引擎
// ---------------------------------------------------------------------------

class CloudSyncEngine {
 public:
  // 执行一轮同步。返回 false ⇒ 失败（err 里给中文原因）。
  // 引擎**不持有**凭据：真正的推拉由这个回调去做。
  using SyncRunner = std::function<bool(SyncDirection, CloudSyncRoundResult*)>;

  CloudSyncEngine();
  ~CloudSyncEngine();

  CloudSyncEngine(const CloudSyncEngine&) = delete;
  CloudSyncEngine& operator=(const CloudSyncEngine&) = delete;

  // 启动引擎（起后台线程）。runner 为空 ⇒ 失败。
  bool Start(const SyncRunner& runner, const CloudSyncEngineOptions& options);

  // 停止引擎并回收后台线程。可重复调用。
  void Stop();

  bool running() const;

  // 本地词典有改动（由调用方在写入用户词典后调用）。
  void NotifyLocalChange();

  // 用户点「立即同步」。
  void RequestManualSync();

  // 开关同步（关闭后引擎不发起任何同步轮次，但仍可查询状态）。
  void SetEnabled(bool enabled);
  bool enabled() const;

  CloudSyncStatus status() const;

 private:
  class Impl;
  Impl* impl_ = nullptr;
};

}  // namespace cloud
}  // namespace fluxing
