#pragma once
//
// spec 100 阶段 F · T501 · 云同步的「本地改动通知宿主」
//
// 职责：把「rime 提交了文本（本地词典可能变了）」这件事，转成「同步引擎
// 知道有本地改动」。
//
// 为什么要有这一层（而不是把引擎指针直接塞进槽）：
//   · 槽是**无捕获的函数指针**（注册在 RimeWithWeasel 静态库里的全局槽）。
//     若回调捕获了某个控制器 / 对话框的 this，控制器析构后就会留下悬垂
//     指针 —— 而通知来自输入响应线程，生命周期完全不可控。
//   · 本层用 shared_ptr 持有引擎：通知线程取走一份拷贝后，即使引擎正在
//     Stop / 析构，引擎对象也仍然存活 ⇒ 无 use-after-free。
//
// 线程纪律（宪法 P2）：
//   NotifyLocalChange() 在**输入响应线程**被调用 ⇒ 只做「取 shared_ptr +
//   置标志 + 唤醒」，不得做 I/O、不得等待。引擎的内部锁只在短临界区持有
//   （网络 I/O 明确在锁外，见 CloudSyncEngine.cpp ThreadMain），所以这里
//   不会把输入卡住。
//
// PCH 注意：本头不使用预编译头，包含它的 .cpp 必须自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <memory>
#include <mutex>

namespace fluxing {
namespace cloud {

class CloudSyncEngine;

class CloudSyncNotifier {
 public:
  // 进程级单例。
  static CloudSyncNotifier& Instance();

  // 挂上 / 摘掉目标引擎。传 nullptr 表示摘掉（此后通知是空操作）。
  void SetTarget(std::shared_ptr<CloudSyncEngine> engine);

  // 通知（供全局槽直接回调；必须极短、不得做 I/O）。
  static void NotifyLocalChange();

  // 当前是否挂着引擎（单测与诊断用）。
  bool has_target() const;

 private:
  CloudSyncNotifier() = default;
  ~CloudSyncNotifier() = default;
  CloudSyncNotifier(const CloudSyncNotifier&) = delete;
  CloudSyncNotifier& operator=(const CloudSyncNotifier&) = delete;

  mutable std::mutex mutex_;
  std::shared_ptr<CloudSyncEngine> target_;
};

}  // namespace cloud
}  // namespace fluxing
