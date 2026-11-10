// spec 100 阶段 F · T501/T504 同步引擎实现
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudSyncEngine.h"

#include <windows.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace fluxing {
namespace cloud {

namespace {

// 单调钟毫秒（调度用，免疫墙钟跳变）。
int64_t SteadyNowMs() {
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// 墙钟毫秒（状态显示 + 自然日配额翻篇用）。
int64_t WallNowMs() {
  FILETIME ft;
  ::GetSystemTimeAsFileTime(&ft);
  const uint64_t ticks =
      (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
  return static_cast<int64_t>(ticks / 10000ull - 11644473600000ull);
}

}  // namespace

// ---------------------------------------------------------------------------
// 实现
// ---------------------------------------------------------------------------

class CloudSyncEngine::Impl {
 public:
  ~Impl() { Stop(); }

  bool Start(const SyncRunner& runner, const CloudSyncEngineOptions& options) {
    if (!runner)
      return false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (running_)
        return false;
      options_ = options;
      runner_ = runner;
      running_ = true;
      enabled_ = true;
      startup_pending_ = true;
      startup_at_ms_ = SteadyNowMs() + options.startup_delay_ms;
      dirty_since_ms_ = 0;
      // 「上次拉取 / 上次推送」必须从**现在**起算，不能从 0 起算：
      // DecideNextAction 里「now >= last_pull_ms + 周期」在 last_pull_ms=0 时
      // 恒成立，定时条件会在启动瞬间就命中，startup_delay_ms 就形同虚设。
      const int64_t boot_ms = SteadyNowMs();
      last_push_ms_ = boot_ms;
      last_pull_ms_ = boot_ms;
      backoff_step_ = 0;
      backoff_until_ms_ = 0;
      consecutive_failures_ = 0;
      manual_requested_ = false;
      wakeup_ = false;
      push_quota_blocked_ = false;
      pull_quota_blocked_ = false;
      day_index_ = DayIndexUtc(WallNowMs());
      push_count_today_ = 0;
      pull_count_today_ = 0;
      status_ = CloudSyncStatus();
      status_.enabled = true;
      status_.running = true;
    }
    thread_ = std::thread([this]() { ThreadMain(); });
    return true;
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_ && !thread_.joinable())
        return;
      running_ = false;
      status_.running = false;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
      // 从引擎线程自身调用时不能 join（会死锁）。
      if (thread_.get_id() == std::this_thread::get_id())
        thread_.detach();
      else
        thread_.join();
    }
  }

  bool running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
  }

  void NotifyLocalChange() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const int64_t now = SteadyNowMs();
      if (dirty_since_ms_ == 0)
        dirty_since_ms_ = now;
      wakeup_ = true;
    }
    cv_.notify_all();
  }

  void RequestManualSync() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      manual_requested_ = true;
      wakeup_ = true;
    }
    cv_.notify_all();
  }

  void SetEnabled(bool enabled) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      enabled_ = enabled;
      status_.enabled = enabled;
      wakeup_ = true;
    }
    cv_.notify_all();
  }

  bool enabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
  }

  CloudSyncStatus status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
  }

 private:
  void ThreadMain() {
    while (true) {
      SyncAction action = SyncAction::kWait;
      SyncDirection dir = SyncDirection::kBoth;
      SyncTrigger trigger = SyncTrigger::kNone;
      SyncRunner runner;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
          if (!running_)
            return;
          const int64_t now = SteadyNowMs();
          RollDayLocked();
          if (!enabled_) {
            action = SyncAction::kWait;
            break;
          }
          const SyncScheduleInput input = BuildInputLocked(now);
          action = DecideNextAction(options_, input);
          if (action == SyncAction::kBackoffExhausted) {
            // 退避重试用尽：交回定时触发，并把防抖窗口重新起算，
            // 避免「本地改动仍然待推」导致立刻再撞一次失败。
            backoff_step_ = 0;
            consecutive_failures_ = 0;
            dirty_since_ms_ = now;
            status_.backoff_step = 0;
            continue;
          }
          trigger = TriggerForLocked(input, action);
          break;
        }
        if (!running_)
          return;

        if (action == SyncAction::kWait ||
            action == SyncAction::kQuotaExceeded) {
          const int64_t deadline = NextWakeMsLocked(action);
          // ★ 只有「配额用尽」才改写状态：普通等待不能把上一轮的结果
          //   覆盖成「已跳过」。
          if (action == SyncAction::kQuotaExceeded) {
            status_.last_outcome = SyncOutcome::kSkipped;
            status_.last_message = "今日同步次数已达上限，已跳过本轮。";
          }
          status_.next_attempt_ms = WallNowMs() + (deadline - SteadyNowMs());
          cv_.wait_until(lock,
                         std::chrono::steady_clock::time_point(
                             std::chrono::milliseconds(deadline)),
                         [this]() { return !running_ || wakeup_; });
          wakeup_ = false;
          continue;
        }

        // 方向策略：**启动轮只拉不推**（plan §7「启动 → 拉一次」），
        // 其余轮次一律「推 + 拉」。
        //   · 推送集合由 sidecar 差分算出（只有本地改动过的条目才进集合），
        //     没有本地改动时**不会发出推送请求** ⇒ 不会造成写放大；
        //   · 若定时轮只拉不推，本地新增 / 修改的词条永远传不上云端
        //     （sidecar 差分能发现改动，但没人触发推送）。
        dir =
            startup_pending_ ? SyncDirection::kPullOnly : SyncDirection::kBoth;
        startup_pending_ = false;
        manual_requested_ = false;
        runner = runner_;
      }

      // 锁外执行同步：网络 I/O 既不在锁内，也不在 UI / TSF 回调里。
      CloudSyncRoundResult result;
      bool ok = false;
      if (runner)
        ok = runner(dir, &result);

      {
        std::lock_guard<std::mutex> lock(mutex_);
        OnRoundFinishedLocked(dir, trigger, ok, result);
      }
    }
  }

  // 自然日翻篇 ⇒ 配额清零。
  void RollDayLocked() {
    const int64_t day = DayIndexUtc(WallNowMs());
    if (day == day_index_)
      return;
    day_index_ = day;
    push_count_today_ = 0;
    pull_count_today_ = 0;
    push_quota_blocked_ = false;
    pull_quota_blocked_ = false;
  }

  SyncScheduleInput BuildInputLocked(int64_t now) const {
    SyncScheduleInput in;
    in.now_ms = now;
    in.dirty_since_ms = dirty_since_ms_;
    in.last_push_ms = last_push_ms_;
    in.last_pull_ms = last_pull_ms_;
    in.startup_pending = startup_pending_;
    in.startup_at_ms = startup_at_ms_;
    in.backoff_step = backoff_step_;
    in.backoff_until_ms = backoff_until_ms_;
    in.push_count_today = push_count_today_;
    in.pull_count_today = pull_count_today_;
    in.manual_requested = manual_requested_;
    return in;
  }

  SyncTrigger TriggerForLocked(const SyncScheduleInput& in,
                               SyncAction action) const {
    if (in.manual_requested)
      return SyncTrigger::kManual;
    if (in.backoff_step > 0)
      return SyncTrigger::kRetry;
    if (in.startup_pending)
      return SyncTrigger::kStartup;
    int64_t deadline = 0;
    if (ComputePushDeadline(options_, in.dirty_since_ms, in.last_push_ms,
                            &deadline) &&
        in.now_ms >= deadline) {
      return SyncTrigger::kLocalChange;
    }
    return SyncTrigger::kPeriodic;
  }

  static SyncDirection DirectionFor(SyncAction action) {
    switch (action) {
      case SyncAction::kRunPull:
        return SyncDirection::kPullOnly;
      case SyncAction::kRunPush:
        return SyncDirection::kPushOnly;
      default:
        return SyncDirection::kBoth;
    }
  }

  // 下次唤醒的**单调钟**截止时刻。
  int64_t NextWakeMsLocked(SyncAction action) {
    const int64_t now = SteadyNowMs();
    if (!enabled_)
      return now + 3600 * 1000;  // 关闭状态下只等事件（SetEnabled 会唤醒）

    push_quota_blocked_ = !DailyPushAllowed(options_, push_count_today_);
    pull_quota_blocked_ = !DailyPullAllowed(options_, pull_count_today_);

    int64_t best = now + 3600 * 1000;
    auto consider = [&best](int64_t candidate) {
      if (candidate < best)
        best = candidate;
    };

    if (backoff_step_ > 0)
      consider(backoff_until_ms_);
    if (startup_pending_)
      consider(startup_at_ms_);
    int64_t push_deadline = 0;
    if (!push_quota_blocked_ &&
        ComputePushDeadline(options_, dirty_since_ms_, last_push_ms_,
                            &push_deadline)) {
      consider(push_deadline);
    }
    if (!pull_quota_blocked_)
      consider(last_pull_ms_ + options_.periodic_pull_interval_ms);
    if (push_quota_blocked_ || pull_quota_blocked_) {
      // 配额用尽 ⇒ 等自然日翻篇（墙钟目标换算成单调钟 deadline）。
      const int64_t wall_now = WallNowMs();
      int64_t delta = DayRolloverMs(wall_now) - wall_now;
      if (delta < 0)
        delta = 0;
      consider(now + delta);
    }
    if (best <= now)
      best = now + 1000;  // 安全网：避免决策与唤醒时刻不一致时热循环
    return best;
  }

  void OnRoundFinishedLocked(SyncDirection dir,
                             SyncTrigger trigger,
                             bool ok,
                             const CloudSyncRoundResult& result) {
    const int64_t now = SteadyNowMs();
    status_.ever_synced = true;
    status_.last_sync_ms = WallNowMs();
    status_.last_trigger = trigger;
    status_.running = running_;
    status_.enabled = enabled_;

    if (ok) {
      status_.last_outcome = SyncOutcome::kSuccess;
      status_.last_message =
          result.message.empty() ? "同步成功。" : result.message;
      consecutive_failures_ = 0;
      backoff_step_ = 0;
      backoff_until_ms_ = 0;
      if (dir == SyncDirection::kPushOnly || dir == SyncDirection::kBoth) {
        push_count_today_ += result.push_requests;
        last_push_ms_ = now;
        dirty_since_ms_ = 0;  // 本地改动已推送
      }
      if (dir == SyncDirection::kPullOnly || dir == SyncDirection::kBoth) {
        pull_count_today_ += result.pull_requests;
        last_pull_ms_ = now;
      }
    } else {
      status_.last_outcome = SyncOutcome::kFailed;
      status_.last_message =
          result.message.empty() ? "同步失败。" : result.message;
      consecutive_failures_ += 1;
      backoff_step_ = consecutive_failures_;
      if (backoff_step_ > options_.max_backoff_attempts)
        backoff_step_ = options_.max_backoff_attempts;
      backoff_until_ms_ = now + BackoffDelayMs(options_, backoff_step_);
    }

    status_.push_count_today = push_count_today_;
    status_.pull_count_today = pull_count_today_;
    status_.backoff_step = backoff_step_;
  }

  mutable std::mutex mutex_;
  std::condition_variable cv_;
  std::thread thread_;
  CloudSyncEngineOptions options_;
  SyncRunner runner_;
  CloudSyncStatus status_;

  bool running_ = false;
  bool enabled_ = false;
  bool wakeup_ = false;
  bool manual_requested_ = false;
  bool startup_pending_ = false;
  bool push_quota_blocked_ = false;
  bool pull_quota_blocked_ = false;

  int64_t startup_at_ms_ = 0;
  int64_t dirty_since_ms_ = 0;
  int64_t last_push_ms_ = 0;
  int64_t last_pull_ms_ = 0;
  int64_t backoff_until_ms_ = 0;
  int backoff_step_ = 0;
  int consecutive_failures_ = 0;

  int64_t day_index_ = 0;
  int push_count_today_ = 0;
  int pull_count_today_ = 0;
};

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

CloudSyncEngine::CloudSyncEngine() : impl_(new Impl()) {}

CloudSyncEngine::~CloudSyncEngine() {
  delete impl_;
  impl_ = nullptr;
}

bool CloudSyncEngine::Start(const SyncRunner& runner,
                            const CloudSyncEngineOptions& options) {
  return impl_->Start(runner, options);
}

void CloudSyncEngine::Stop() {
  impl_->Stop();
}

bool CloudSyncEngine::running() const {
  return impl_->running();
}

void CloudSyncEngine::NotifyLocalChange() {
  impl_->NotifyLocalChange();
}

void CloudSyncEngine::RequestManualSync() {
  impl_->RequestManualSync();
}

void CloudSyncEngine::SetEnabled(bool enabled) {
  impl_->SetEnabled(enabled);
}

bool CloudSyncEngine::enabled() const {
  return impl_->enabled();
}

CloudSyncStatus CloudSyncEngine::status() const {
  return impl_->status();
}

}  // namespace cloud
}  // namespace fluxing
