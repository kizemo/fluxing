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
// ★ 本头**故意**不包含任何标准库 / windows 头：WeaselServerApp.cpp 走预编译
//   头（stdafx.h 先引入 windows.h 的 min/max 宏），再包含 <algorithm> 之类
//   会被污染。需要这些头时用 WeaselServer/Cloud/ 下自带
//   `#define NOMINMAX` 的实现文件。
//
class RimeWithWeaselHandler;

// 注册维护态宿主。传 nullptr 表示撤销注册。
void RegisterCloudMaintenanceHost(RimeWithWeaselHandler* handler);
