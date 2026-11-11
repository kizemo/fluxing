// spec 100 阶段 F · T501 · 本地词典改动通知槽（实现）
//
// 落点必须在 RimeWithWeasel 静态库**内部**，不能放在 Cloud 侧：
//   · WeaselServer.exe 通过 ProjectReference 链接本库；
//   · test/TestWeaselIPC 也链接本库 —— 若本库反过来引用 Cloud 侧符号，
//     TestWeaselIPC 会 LNK2019（未解析外部符号）。
//   ⇒ 依赖方向必须是「Cloud → Rime」，不能是「Rime → Cloud」。
//
// 本文件不使用预编译头（工程里逐文件设 PrecompiledHeader=NotUsing），
// 这样任何工程（含单测工程）都能直接编译它。

#include <atomic>

#include "FluxingLocalChangeHook.h"

namespace {

// 全局槽。注册发生在启动期 / 账号页，通知发生在输入响应线程 —— 两者可能
// 不是同一个线程 ⇒ 用 std::atomic 保证「注册后可见」且免锁。
// release/acquire 配对：注册方 store(release) ⇒ 通知方 load(acquire) 能看到。
std::atomic<FluxingLocalChangeHookFn> g_local_change_hook{nullptr};

}  // namespace

void RegisterFluxingLocalChangeHook(FluxingLocalChangeHookFn fn) {
  g_local_change_hook.store(fn, std::memory_order_release);
}

void NotifyFluxingLocalChange() {
  const FluxingLocalChangeHookFn fn =
      g_local_change_hook.load(std::memory_order_acquire);
  if (fn != nullptr)
    fn();
}
