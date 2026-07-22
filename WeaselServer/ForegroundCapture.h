//
// ForegroundCapture.h — 捕获用户 app 的 foreground 状态 (Phase K3 T011 fix)
//
// v0.19.0.52 (Phase K3 T011 Option B): 解决 out-of-process IPC 后
// INJECT 时 SendInput 命中 dialog 而非 user app 的回归。
//
// 流程:
//   1. RimeWithWeaselHandler::FocusIn (server 端, WeaselServer.exe 的
//      IPC worker thread) 收到 WeaselTSF.dll 转发来的 IPC 时:
//        CaptureFromCurrentThread() — 此调用 GetForegroundWindow() 是
//        session-global 的 (跟 caller 进程无关),因此拿到的就是 user
//        app 的 foreground HWND + thread id。
//      触发时刻: 用户在 edit field 输入 → TSF OnSetFocus → WeaselTSF.dll 发
//      FocusIn IPC → server 端 FocusIn handler 跑 → 此 capture。
//
//   2. PhrasesDialogIPC::InjectText 在做 SendInput 之前:
//        GetHwnd() / GetThreadId() 拿到最近一次 capture 的值
//        → AttachThreadInput(targetTid, currentTid, TRUE) 共享 input state
//        → SetForegroundWindow(targetHwnd) 抢回 user app 的 foreground
//        → SendInput (现在的 foreground 是 user app)
//        → AttachThreadInput FALSE detach
//
// **已知限制** (写进 lessons-learned L##): 若用户在 last FocusIn 后
// Alt+Tab 到别处 (无 edit field), cache 仍指 last edit field — 注入
// 文本仍去那里。在 addSession-on-edit-field-only 时是合理 trade-off;
// 真正 "current foreground at inject time" 需要 CBT hook 或 HK 全局监听
// (out of scope)。
//
#pragma once

#include <windows.h>

namespace fluxing {
namespace foreground_restore {

// 在调用方的当前线程上抓 system foreground。GetForegroundWindow()
// 是 session-global 的,不依赖 caller 进程 — 在 IPC worker thread 上
// 调 = 抓 user app 当前 foreground。
// 推荐调用点: RimeWithWeaselHandler::FocusIn (FocusIn IPC 刚落, user
// 还在 edit field 里)。
void CaptureFromCurrentThread();

// 最近一次 Capture 拿到的 foreground HWND。窗口可能已销毁 — 消费方
// 必须用 IsWindow(hwnd) 验证 (典型用法见 PhrasesDialogIPC.cpp)。
HWND GetHwnd();

// 最近一次 Capture 拿到的 foreground thread id。process 内 AttachThreadInput 用。
DWORD GetThreadId();

}  // namespace foreground_restore
}  // namespace fluxing
