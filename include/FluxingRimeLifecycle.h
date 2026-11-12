#ifndef FLUXING_RIME_LIFECYCLE_H_
#define FLUXING_RIME_LIFECYCLE_H_

// 批次 AQ · rime 生命周期互斥体（ADR 0061）
//
// 用途：备份 / 还原用户词典必须**独占** leveldb 用户库，而独占的唯一手段是让
// rime 释放它（finalize）。finalize 与「正在用 levers 读用户库」是互斥的，两
// 者撞上就是 0xC0000005。本锁让两类操作互斥：
//   · 云同步轮（WeaselServer/Cloud/CloudRimeUserDictBackend.cpp）
//   · 部署器维护（WeaselIPCServer/WeaselServerImpl.cpp 的 OnStartMaintenance）
//
// ★ 为什么用 Win32 **命名信号量**，而不是 C++ inline static：
//   WeaselServer.exe 里 WeaselIPCServer 与 WeaselServer/Cloud 可能是两个模块，
//   inline static 会被各模块各持一份 ⇒ 互斥形同虚设。命名对象由内核按名字
//   唯一化 ⇒ 跨模块指向同一把锁。
//
// ★ 为什么是「信号量」而不是「互斥体」：
//   互斥体只能由持有它的**同一线程**释放；本锁的获取方（云同步线程）与释放方
//   （IPC 处理线程）可能不是同一个线程 ⇒ 用信号量，谁调 Release 都行。
//
// ★ 不会永久锁死：进程退出时内核销毁命名对象，计数随之复位。

#include <windows.h>

namespace fluxing {
namespace rime_lifecycle {

// 生产环境用的锁名。测试请用别的名字，避免与运行中的 WeaselServer 抢锁。
inline const wchar_t* DefaultLockName() {
  return L"Local\\Fluxing.RimeLifecycle";
}

// 获取（带等待）。成功返回**持有计数**的句柄；失败返回 nullptr。
// ★ 配对释放必须走 ReleaseSemaphoreHandle()：信号量的占用计数不会因为
//   CloseHandle 而复位，直接 CloseHandle 会把锁永久留在「已占用」状态。
inline HANDLE TryAcquire(const wchar_t* name, DWORD timeout_ms) {
  if (name == nullptr || name[0] == L'\0')
    return nullptr;
  HANDLE h = ::CreateSemaphoreW(nullptr, 1, 1, name);
  if (h == nullptr)
    return nullptr;
  const DWORD r = ::WaitForSingleObject(h, timeout_ms);
  if (r != WAIT_OBJECT_0 && r != WAIT_ABANDONED) {
    ::CloseHandle(h);
    return nullptr;
  }
  return h;
}

inline void ReleaseSemaphoreHandle(HANDLE h) {
  if (h == nullptr)
    return;
  ::ReleaseSemaphore(h, 1, nullptr);
  ::CloseHandle(h);
}

// RAII：析构时自动释放。
class ScopedRimeLifecycleLock {
 public:
  ScopedRimeLifecycleLock() : handle_(nullptr) {}
  ~ScopedRimeLifecycleLock() { Release(); }
  ScopedRimeLifecycleLock(const ScopedRimeLifecycleLock&) = delete;
  ScopedRimeLifecycleLock& operator=(const ScopedRimeLifecycleLock&) = delete;

  bool Acquire(const wchar_t* name, DWORD timeout_ms) {
    Release();
    handle_ = TryAcquire(name, timeout_ms);
    return handle_ != nullptr;
  }

  void Release() {
    if (handle_ != nullptr) {
      ReleaseSemaphoreHandle(handle_);
      handle_ = nullptr;
    }
  }

  bool held() const { return handle_ != nullptr; }

 private:
  HANDLE handle_;
};

}  // namespace rime_lifecycle
}  // namespace fluxing

#endif  // FLUXING_RIME_LIFECYCLE_H_
