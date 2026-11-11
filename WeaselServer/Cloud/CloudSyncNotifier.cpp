// spec 100 阶段 F · T501 · 云同步的「本地改动通知宿主」（实现）
//
// PCH 纪律：本文件不使用预编译头，并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudSyncNotifier.h"

#include "CloudSyncEngine.h"

namespace fluxing {
namespace cloud {

CloudSyncNotifier& CloudSyncNotifier::Instance() {
  static CloudSyncNotifier instance;
  return instance;
}

void CloudSyncNotifier::SetTarget(std::shared_ptr<CloudSyncEngine> engine) {
  std::lock_guard<std::mutex> lock(mutex_);
  target_ = std::move(engine);
}

void CloudSyncNotifier::NotifyLocalChange() {
  // 静态入口 ⇒ 先取进程级单例，再访问实例状态。
  CloudSyncNotifier& self = Instance();
  // 先把 shared_ptr 拷出来再放锁：引擎的 NotifyLocalChange() 只做
  // 「置标志 + 唤醒」，但它会拿引擎自己的锁 —— 不该在持有本锁时调用。
  std::shared_ptr<CloudSyncEngine> engine;
  {
    std::lock_guard<std::mutex> lock(self.mutex_);
    engine = self.target_;
  }
  if (engine)
    engine->NotifyLocalChange();
}

bool CloudSyncNotifier::has_target() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return static_cast<bool>(target_);
}

}  // namespace cloud
}  // namespace fluxing
