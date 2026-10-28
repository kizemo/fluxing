# 0012. The Shift IPC answers with a response body, and a bare Shift never leaves its window

- **Status**: Accepted
- **Date**: 2026-10-28
- **Deciders**: Fluxing maintainers
- **Related**: ADR 0010（裸 Shift 切中英文）、spec 077（Shift 选第 2/3 候选）、`CHANGELOG.md` 1.0.0

## Context

0.21.0.9 真机安装测试暴露了三个互相纠缠的现象：

1. **候选窗开着时裸 `Shift` 选不了第 2 / 第 3 候选**。候选窗确实消失了，但文字没有上屏，组合状态还在。
2. **两个窗口各开一会话时，A 窗口裸 `Shift` 会把 B 窗口一起切掉**。
3. **任务栏 / 语言栏的中英文状态图标恒显示「中」**，中英文切换后不跟随。

三者看起来是三个 bug，实际是**同一条链路上的同一个缺口**：`WEASEL_IPC_SHIFT_DOWN` / `WEASEL_IPC_SHIFT_UP`
这两条 IPC **从来不带响应体**。

管道协议的事实（`include/PipeChannel.h` + `WeaselIPC/PipeChannel.cpp`）：

- 服务端一条消息 = `[DWORD result][body]`，一次 `WriteFile` 发出。
- 服务端**写 body** 时，客户端读 4 字节的 `result` 会撞上 `ERROR_MORE_DATA`，于是整条消息被读进
  `ctx->buffer`，客户端缓冲区**被刷新**。
- 服务端**不写 body** 时（只有 4 字节），客户端那次 `ReadFile` 直接成功，`ctx->buffer` **完全不被触碰**，
  客户端保留**上一次**的旧响应体。

于是 `ShiftUp` 这条路：

- 服务端 `select_candidate` 之后**没有** `commit_composition` ⇒ librime 在 `_auto_commit` 关闭时只做
  `composition().Forward()`（`librime/src/rime/engine.cc:168-191`），组合仍在、没有 commit 文本 ⇒
  **候选窗消失但字不上屏**，正是现象 1。
- 服务端**没有**响应体 ⇒ 客户端的 `DoEditSession`（`WeaselTSF/EditSession.cpp`）读到的是**上一次**的
  旧响应体 ⇒ 客户端既拿不到 commit 文本，`_status.ascii_mode` 也永远不刷新 ⇒ 现象 3。

而现象 2 的机制在 `RimeWithWeasel.cpp::_Respond` 里：那里有一段跨会话传播，比较的是
`session_status.status.is_ascii_mode`（**缓存**）与当前状态。裸 `Shift` 走 `set_option` 直线，
不刷新缓存，于是**下一个**响应事件才触发传播，把切换过的会话的 `ascii_mode` 复制给所有其它会话。
它的开关量 `m_global_ascii_mode` 同时扮演了「yaml 配置标志」和「运行时状态镜像」两个角色——
`SetOption` 末尾会把 yaml 读出来的 `global_ascii` 覆盖掉。

TSF 侧其实不缺链路：`OnTestKeyDown` / `OnTestKeyUp` 在 `_ProcessKeyEvent` 之后**必定**调用
`_UpdateComposition(pContext)` → `RequestEditSession` → `DoEditSession` → `GetResponseData(parser)`。
只要服务端在 Shift IPC 里写出 body，客户端就能在同一个个按键里读到 commit 文本与新状态，**不需要改 TSF**。

## Decision

**P1 — Shift IPC 必须带响应体。**
`RequestHandler::ShiftDown` / `ShiftUp` 增加 `EatLine eat` 参数（与 `ChangePage` /
`HighlightCandidateOnCurrentPage` 同构）。服务端在 `ShiftDown` / `ShiftUp` 里调用 `_Respond(ipc_id, eat)`，
客户端的管道缓冲区因此被刷新，不会再解析到陈旧的响应体。

**P2 — 选完候选必须提交。**
`ShiftUp` 的 `kSelectCandidate` 分支在 `select_candidate` 成功后追加
`rime_api->commit_composition(session)`。该调用是幂等的：`_auto_commit` 打开时 `Select` 已经提交并清空，
该调用直接返回 false，不会重复提交。

**P3 — 配置标志与运行时状态分离。**
新增不可变的 `m_global_ascii_configured`（只来自 `weasel.yaml` 的 `global_ascii`），
`m_global_ascii_mode` 退化为纯运行时镜像（QuickPanel 初值用）。两者不再是同一个变量。

**P4 — 跨会话传播只从 `SetOption` 出发，且只在显式控制路径生效。**
`_Respond` 里的跨会话传播块**删除**。传播逻辑搬到 `SetOption`，判据是
`fluxing::DecideAsciiModeScope(explicit_control, global_ascii_configured)`：
显式控制（托盘 / 语言栏菜单）**且** `global_ascii: true` 才广播到所有会话。

**P5 — 裸 `Shift` 永不广播。**
裸 `Shift` 走 `rime_api->set_option(session, ...)` 直线，**绝不经过 `SetOption`**，
因此无论 `weasel.yaml` 怎么配，都只作用本会话。ADR 0010 P2 的「只切当前会话」由此从约定升级为机制保证。

**P6 — 语言栏图标与文本跟随模式。**
`ShiftUp` 的响应体带 `status.ascii_mode` ⇒ 客户端 `_status` 刷新 ⇒ 语言栏图标自动跟随；
另外 `CLangBarItemButton::GetText()` 改为返回「中」/「英」，`UpdateWeaselStatus()` 的
`OnUpdate` 补上 `TF_LBI_TEXT`。

**P7 — 线上协议不变。**
`WEASEL_IPC_SHIFT_DOWN` / `WEASEL_IPC_SHIFT_UP` 的命令号与 `wParam` / `lParam` 布局完全不变，
变的只是「服务端是否回 body」。变更面覆盖接口（`include/WeaselIPC.h`）、服务端
（`WeaselIPCServer/WeaselServerImpl.cpp`）、处理端（`include/RimeWithWeasel.h` +
`RimeWithWeasel/RimeWithWeasel.cpp`）、测试（`test/TestWeaselIPC`）与本 ADR。

**P8 — 版本号对外为三位数字。**
`build.bat` / `xbuild.bat` 的 `VERSION_MAJOR/MINOR/PATCH` 改为 `1.0.0`；
安装包文件名与「应用和功能」里的 `DisplayVersion` 不再带第 4 位（`WEASEL_BUILD`）。
Windows 资源版本与 `VIProductVersion` 仍是 4 段数字（`1.0.0.0`）——这是 Win32 格式要求；
版本化的 shim 名保留 `${WEASEL_BUILD}`，那是内部唯一性需要。

## Consequences

- ✅ 候选窗开着时裸 `Shift` 真的能把第 2 / 第 3 候选上屏。
- ✅ 两个窗口各开一会话时，裸 `Shift` 只切当前窗口。
- ✅ 任务栏 / 语言栏的中英文图标与文本跟随模式切换。
- ✅ 客户端管道缓冲区不再可能读到陈旧响应体（顺带消除了「提交后再按 Shift 可能重复上屏旧
  commit 文本」的既有隐患）。
- ⚠️ `global_ascii: true` 的用户仍保留「一个窗口切、所有窗口跟着切」的**显式控制**行为；
  这与上游 `global_ascii` 的语义一致，只是不再被裸 `Shift` 意外触发。
- ⚠️ 裸 `Shift` 切换后不会写 `m_global_ascii_mode` 全局镜像（ADR 0010 P2 延续），
  因此 QuickPanel 的**初值**可能与当前窗口不一致；这是既有行为，本次不扩大修改面。
- ⚠️ 每次 Shift 按下 / 抬起多一次 IPC 往返与一次 `get_status` / `get_context`。
  这是用一点开销换掉一整类陈旧状态错误，划算。
- ✅ 决策可逆：若将来要在客户端侧统一做「响应体一次性消费」，可以撤掉 P1，
  届时 `ShiftUp` 的 commit 逻辑（P2）仍然成立。
