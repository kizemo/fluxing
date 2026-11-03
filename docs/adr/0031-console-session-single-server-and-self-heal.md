# ADR 0031 — 进程拓扑定案：全局热键只属于 console 会话；非 console 拒绝启动并留痕；console 缺位自愈

- **Status**: Accepted
- **Date**: 2026-11-03
- **Spec**: 事故驱动修复，无独立 spec；验收探针见本 ADR 决策 4
- **Lessons**: L170（`.specify/memory/lessons-learned.md`）
- **Incident**: `docs/archive/handoffs/handoff-2026-11-03-fluxing-quickpanel-session-and-alt-dot.md` §0/§4
- **Supersedes / superseded by**: —（新增；与 ADR 0022「脱离式服务重启」互补）

---

## 背景

2026-11-03 事故：用户报「快捷设置栏调不出、`Alt+.` 用户短语调不出」。运行时取证坐实根因**不在热键、不在 yaml**：

1. `WeaselServer.exe`（pid 17304）于 08:53 被 **ToDesk 远程会话 Session 3** 的 explorer 经
   `Startup\WeaselServer.bat auto` 拉起；而用户在 **console（`WTSGetActiveConsoleSessionId()=1`）**。
2. 单实例 mutex `muServer`（`WeaselIPCServer\WeaselServerImpl.cpp:170-184` `Start()`）**不带 `Global\` 前缀 ⇒ 会话隔离**：
   Session 3 的服务端注册了它那个会话的全部全局热键；`RegisterHotKey` 是会话级作用域，console 用户按键无人接管。
3. `Startup\WeaselServer.bat`（`output/data/Startup/`，装机自启，L106）在**每个**会话的 explorer 登录时都会执行 ⇒ 天然多实例。
4. 当时 Session 1 没有任何服务端实例；`Start()` 的失败路径只 `return false`，**无日志无提示**——静默失效。
5. 取证过程还踩到两个探针坑（详见 L170）：`FindWindowW` 对本项目窗口恒返回 0（须 `EnumWindows`）；
   DPI-unaware 探针读 150% 屏几何全被 ÷1.5（曾误判「面板塌成 163×32 空壳」，实为物理 245×48 逻辑未放大，另案）。

杀旧 pid、在 Session 1 重启后，`Alt+.` / `Alt+,` / `Ctrl+Shift+K` / `Alt+/` 全部 `HELD (err=1409)`，症状消失
⇒ 根因结论成立（运行时层面）。**代码未改**，本 ADR 定案代码级修复。

## 决策

### 决策 1 — console 会话是全局热键的唯一持有人

`WeaselServerImpl::Start()` 在创建 mutex 之前检查：
`WTSGetActiveConsoleSessionId() != 当前进程 SessionId` ⇒ **拒绝启动，退出前写结构化日志**
（诊断走独立 append-only 文件，口径同 ADR 0029 决策 7）。
远程会话（ToDesk/RDP）里的自启 bat 由此不再产生「抢注热键但服务不到用户」的僵尸服务端。

### 决策 2 — 单实例 mutex **不加** `Global\` 前缀，维持会话局部

加 `Global\` 会让「远程会话先启动的那个实例」占死全局唯一锁，console 会话反而静默无服务 —— 方向性错误，否决。
配合决策 1，每个时刻最多只有 console 会话能活着，会话局部 mutex 足够保证 console 内单实例。

### 决策 3 — `Start()` 任何失败路径必须留痕

现状 `return false` 无日志 ⇒ 一律补日志（拒绝原因、SessionId、console SessionId、mutex 错误码）。
「什么都没发生」在本项目已被反复证明是最难查的症状（L168/L170）。

### 决策 4 — console 缺位自愈：前端发现无服务时异步补拉，禁止在 TSF 回调里同步 CreateProcess

若 console 会话里服务端缺位（自启 bat 未跑、崩溃后无人拉起），`WeaselTSF` 在 IPC 连接失败时
**在自己的工作线程/消息队列上**节流补拉一次 `WeaselServer.exe`（带去重与最小间隔）。
硬约束（constitution P2）：`ITfTextInputProcessor::*` 回调内禁止阻塞 I/O，CreateProcess/等待一律移出回调；
若实现审查发现 TSF 侧已有等价拉起机制，则只做「节流 + 留痕」增强而不是新增（实施者须先以图谱核实，不得凭记忆断言）。

判别性验收：console 会话杀掉服务端后，仅靠打字触发前端连接失败，应观察到（a）新 WeaselServer pid 出现在
SessionId=console（`Win32_Process.SessionId`），（b）诊断日志有补拉记录；在 Session≠console 的环境重复则应观察到
拒绝启动日志且**不**注册热键。

## 后果

- **正面**：消除「服务端起在错误会话 ⇒ 一切热键静默失效」这一类事故；启动失败不再是黑箱；
  console 单持有人规则明确，远程会话不再抢注。
- **负面/代价**：远程会话用户将**没有**火流猩服务 UI（面板/短语窗口/托盘）——打字仍走 TSF+librime 本地链路；
  若未来要支持「远程会话完整体验」，需要另立 spec 把服务改为真 per-session 多实例（含 IPC 管道名去冲突），
  本 ADR 只负责把它挡在门外并留痕。
- **文档义务**：`Rime-设置说明书-重点快捷键.md` §9（本机现状）与 project-knowledge §A（部署）需补一句
  「全局热键仅在 console 会话生效」。

## 被否决的替代方案

1. **mutex 加 `Global\`**：远程实例占锁 → console 静默无服务，把事故换成更安静的事故。否决（见决策 2）。
2. **只加日志不改拓扑**：症状（错误会话抢热键）依旧。否决。
3. **per-session 多实例全功能**：管道名、托盘、热键作用域都要重做，成本远超当前诉求；留作未来 spec 触发条件。否决（暂缓）。
