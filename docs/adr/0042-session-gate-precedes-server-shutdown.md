# 0042. 会话门禁前置到「杀旧实例」之前；前端补拉改为连续失败阈值 + 按会话计数；诊断日志带毫秒时间戳

- **Status**: Accepted
- **Date**: 2026-11-08
- **Deciders**: Fluxing maintainers
- **Related**: ADR 0031（console 会话单一服务端）、ADR 0029（诊断日志通道）、ADR 0041、
  spec 099、lessons L##（本条同时补记为事故条目）

## Context

用户报告（2026-11-08）：

> 开机后第一次切换到火流猩输入法时，会出现比较久的延时，才能正常使用输入法。使用快捷键，
> 无法立刻调出设置栏（开始不显示）和用户短语。也无法输入中文。但延时一段时间后恢复正常。

**症状不是「慢」，是「服务真空」**——某个时刻 console 会话的服务端被杀掉了，而恢复要等前端补拉成功。

现场取证（诊断日志 2718 行 + 进程时间线 + rime 日志）：

- 开机 08:49:05；boot 拉起的 `WeaselServer.exe` 实例 PID 20524 起于 08:49:46；
  rime 日志末写 08:55:54（≈ RDP 登录 08:55:41 之后 13 秒）；现 PID 353904 起于 09:20:26。
- 诊断日志中 `startup_allowed` 60+ 次、`refused(not_console_session)` + `exit=-1` 5 次，
  且**每一次前端补拉之前 `server_process_count=0`** ⇒ 前端确实认为「服务端不在」，
  一直在补拉 ⇒ 与真空吻合。
- 无 crash dump、无 Application Error 1000 ⇒ 干净退出，非崩溃；
  `build/` 词表未重建、rime INFO 无 `changes detected` ⇒ 开机没有部署；
  `win_sparkle` appcast 被注释 ⇒ 启动无网络 ⇒ **部署/更新/网络都不是原因**。

根因在 **ADR 0031 定的拓扑上，门禁的「位置」错了**：

- `WeaselServer\WeaselServer.cpp` WinMain 里的「杀旧实例」块
  （`Connect()` → `ShutdownServer()` → 轮询到断开）**先**执行；
- `WeaselIPCServer\WeaselServerImpl.cpp` `Start()` 里的会话门禁**后**执行。

⇒ 一个由远程 / RDP 会话拉起的实例（`Startup\WeaselServer.bat`）**先把 console 会话的健康
服务端杀掉**，自己随后才被门禁拒掉并 `exit=-1`。热键是**会话级**注册的
（`WeaselServerImpl.cpp` / `WeaselServerApp.cpp`），服务端一死 ⇒ 热键、快捷设置栏、用户短语
一起消失；TSF 连不上 ⇒ 中文打不出。

第二段延时来自前端补拉本身太慢（`WeaselTSF\WeaselTSF.cpp` `_EnsureServerConnected`）：

1. 触发门槛是 `retry >= 6` ⇒ 要连按 6 次键才可能补拉一次；
2. `retry` **只增不减**（连上了也不清零）⇒ 它其实是「累计失败数」而不是「连续失败数」；
3. `WeaselServer.exe` 进程计数是**全系统**统计 ⇒ 别的会话里残留的实例会让本会话误判
   「服务端还活着」⇒ 干脆不补拉 ⇒ 真空持续。

第三段是**取证缺口**：`include\FluxingDiagLog.h` 的 `AppendLine` **不写时间戳**
（只写 `[tag] message`）⇒ 2718 行日志能说「`startup_refused` 出现过 5 次」，
却说不出「谁在什么时候杀掉了 console 会话的服务端」，严谨的时序断言做不了。

## Decision

1. **会话门禁前置到「杀旧实例」之前。** 判定复用同一份纯函数
   `fluxing::session::EvaluateCurrent()`（`include\FluxingSessionPolicy.h`），并新增纯函数
   `MayShutdownExisting(Verdict, quit)`：
   - `quit == true`（`/quit` 运维显式停服）⇒ **放行**；
   - 否则 `Verdict.allowed == false`（本进程不在 console 会话）⇒ **不许**杀旧实例，
     本进程直接以 `exit=-1` 结束（它本来就会被 `Start()` 的门禁拒掉，区别只是不再先动手）。
   - 留痕：`startup_refused allowed=0 reason=not_console_session … detail=pre_kill_gate`。
   - 被否决的备选：① 只在 `Start()` 里拒绝（杀伤已发生，否决）；
     ② 把 `/quit` 也纳入门禁（改运维语义，超出本次范围，否决）；
     ③ 把「杀旧实例」改成「只杀同会话的旧实例」（要动 IPC 客户端协议，改动面更大，
     本期不做，保留为后续备选）。
2. **前端补拉改口径**（`WeaselTSF\WeaselTSF.cpp`）：
   - 门槛从「累计 6 次」改成「**连续**失败 2 次」（`kServerRelaunchFailureThreshold`）；
   - `Echo()` 成功即把连续失败计数清零；
   - 进程计数**按会话**统计（只数与本进程同会话的 `WeaselServer.exe`）；取不到会话号的
     按「算它活着」处理（保守，不重复拉起）；留痕同时记 `server_process_count`（同会话）
     与 `server_process_count_all`（全系统）。
   - ADR 0031 决策 4 的 **60 s 节流**与**单飞闸门**保持不变。
3. **诊断日志带毫秒时间戳。** `AppendLine` 的行形状从 `[tag] message` 改成
   `[YYYY-MM-DD HH:MM:SS.mmm][tag] message`。时间戳**前置**在 tag 之前 ⇒ 既有 grep 口径
   `[server] ` 仍然是子串，存量取证脚本不破。
4. **判定逻辑单一事实源。** `MayShutdownExisting` 落在共享头 `include\FluxingSessionPolicy.h`，
   生产代码（`WeaselServer.cpp` WinMain）与 `TestWeaselIPC.exe /verify-console-session`
   共用同一实现（宪法 R7）。

## Consequences

- ✅ 远程 / RDP 会话拉起的实例**不再**杀伤 console 会话的服务端 ⇒ 真空这条根因路径被从根上断掉。
- ✅ 前端在 2 次连续失败后就补拉，且补拉判定只认本会话的服务端 ⇒ 恢复时间从
  「连按 6 次键 + 等一次补拉」降到「2 次连续失败」。
- ✅ 诊断日志自带毫秒时间戳 ⇒ 下次同类事故可以直接做时序断言，不用再靠推断。
- ⚠️ `/quit` 仍可从任何会话停掉服务端（运维语义未改）。若将来出现「远程会话的启动项误带
  `/quit`」，需另立 ADR 收口。
- ⚠️ 判定用的 `WTSGetActiveConsoleSessionId` 仍然 **fail-open**（取不到 console 会话时放行），
  与 ADR 0031 一致，本 ADR 未改。
- ⚠️ 诊断日志**行形状**变了：任何按「行首即 `[server] `」做正则匹配的脚本需要调整
  （存量取证脚本用的是子串查找，不受影响）。
- ↩️ 可逆：门禁前置是纯顺序调整，把 `MayShutdownExisting` 改成恒返回 `true` 即回到旧行为；
  阈值 2、时间戳、会话化计数都是常量 / 格式级改动，回退成本低。
