#pragma once
//
// spec 100 阶段 F · T501 · 本地词典改动通知槽（进程内直连，不新增 IPC 消息）
//
// 背景（ADR 0056）：
//   rime 的会话与提交发生在 **WeaselServer.exe**（服务端进程），而云同步
//   引擎（CloudSyncEngine）同样活在 WeaselServer.exe ⇒ 「本地词典有改动」
//   这件事**不需要**新增 IPC 消息，直接走进程内函数指针槽即可。
//
//   （本仓库早前的判断是「提交事件发生在 TSF 客户端进程 ⇒ 需要 IPC 四边
//   同步」，该判断已被工程事实推翻：WeaselTSF 不链接 librime，提交发生在
//   RimeWithWeaselHandler 里，而 handler 归 WeaselServer 所有。）
//
// 线程纪律（宪法 P2）：
//   NotifyFluxingLocalChange() 会在**输入响应线程**（服务端 IPC 线程）
//   被调用 ⇒ 回调必须极短：只允许「置标志 + 唤醒」，不得做 I/O、不得等待。
//
// ★ 本头**故意**不包含任何标准库 / windows 头：RimeWithWeasel.cpp 走预编译
//   头（stdafx.h 会先引入 windows.h 的 min/max 宏）。
//

// 本地改动回调（无捕获、无参）。传 nullptr 表示注销。
typedef void (*FluxingLocalChangeHookFn)();

// 注册回调。传 nullptr 表示注销（此后通知变成空操作）。
void RegisterFluxingLocalChangeHook(FluxingLocalChangeHookFn fn);

// 通知「本地词典可能有改动」。未注册回调时是空操作。
void NotifyFluxingLocalChange();
