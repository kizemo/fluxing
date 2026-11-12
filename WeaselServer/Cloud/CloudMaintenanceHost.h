#pragma once
//
// spec 100 阶段 D · 云同步的「维护态宿主」
//
// 背景（CLAUDE.md 硬约束 + leveldb LOCK）：
//   备份 / 还原用户词典时，服务端 rime 必须先释放用户库，否则 leveldb 拿不到
//   锁。释放的动作是 RimeWithWeaselHandler::StartMaintenance / EndMaintenance
//   （它会一并清掉会话表），而 handler 归 WeaselServerApp 所有 ⇒ 由宿主在
//   构造期把 handler 注册进来。
//
// ★ ADR 0061 之后的新语义（务必按这个口径理解）：
//   这两个钩子被**成对**调用 —— Start 之后**立刻** End。End 在 m_disabled 为真时
//   会 Initialize() ⇒ rime 回到可用态、组件重新注册 ⇒ 调用方随后在
//   「rime 可用 + 用户库无人占用」的窗口里做备份 / 还原。
//   （旧语义「finalize 之后 levers 仍可用」已被证伪：Registry 一清空，
//    UserDb::Require("userdb") 返回 nullptr ⇒ 空指针解引用 = 0xC0000005。）
//   任何残留的并发访问冲突，由调用方的 SEH 护栏降级为「本轮失败」，进程不崩。
//
// ★ 本头**故意**不包含任何标准库 / windows 头：WeaselServerApp.cpp 走预编译
//   头（stdafx.h 先引入 windows.h 的 min/max 宏），再包含 <algorithm> 之类
//   会被污染。需要这些头时用 WeaselServer/Cloud/ 下自带
//   `#define NOMINMAX` 的实现文件。
//
class RimeWithWeaselHandler;

// 注册维护态宿主。传 nullptr 表示撤销注册。
void RegisterCloudMaintenanceHost(RimeWithWeaselHandler* handler);
