# ADR 0067 — 孤儿 IPC 会话回收：客户端自报 pid + 惰性清扫

- **Status**: Accepted（ADR 0065 残余风险第 3 条的收口；失败方向单一，代价可逆）
- **Date**: 2026-11-20
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)
- **Revises**: —（新增；不改 ADR 0062 / ADR 0065 的闸门判据与会话复活口径）
- **Lessons**: `.specify/memory/lessons-learned.md`（本批条目待写）
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 宿主崩溃 ⇒ 孤儿会话永久残留

TSF 前端（`weasel.dll`）跑在**宿主进程内**。宿主正常退出 / 失焦时会发
`remove_session`，服务端 `WeaselServerImpl.cpp` 的 `OnEndSession` →
`m_pRequestHandler->RemoveSession(ipc_id)`（`RimeWithWeasel.cpp:566`）把会话条目、
rime 会话、组合串标记一并回收。

宿主**崩溃**时这条消息根本不会发出 ⇒ `m_session_status_map` /
`m_session_composing` / `m_session_client_kinds` 与背后的 librime 会话**永久残留**。

### 事实 2 — 残留不再堵死同步，但仍是资源泄漏

ADR 0065 把维护准入闸门从「有会话」改成「最近有输入（空闲宽限 10 s）或未上屏
组合串（宽限 60 s）」⇒ 残留会话**不再**把同步永久堵死。

但残留仍在：

1. `m_live_session_count` / `m_composing_session_count` 长期偏高；
2. 只要残留里有一条「有未上屏组合串」的会话，组合串宽限期就一直取 **60 s**
   而不是 10 s ⇒ 同步恢复得更慢；
3. librime 会话对象一直不释放。

这是 ADR 0065 明确写下的**残余风险第 3 条**。

### 事实 3 — 客户端自报身份是既有手法（ADR 0032 先例）

`WeaselIPC/WeaselClientImpl.cpp` 的 `_WriteClientInfo()` 已经在既有文本请求体里
自报 `session.client_app=<宿主 exe 名小写>` 与 `session.client_type=`；服务端在
`RimeWithWeasel.cpp` 的 `_ReadClientInfo()` 里解析（ADR 0032 的结论是
「线格式零变化 ⇒ deployer 端零改动」）。

`weasel.dll` 是**进程内加载**⇒ 客户端拿到的 `GetCurrentProcessId()` 就是**宿主
进程**的 pid。

### 事实 4 — 会话表只在 IPC 线程被改

`WeaselServerImpl.cpp` 的 pipe 线程在 `HandlePipeMessage` 外套了
`std::lock_guard guard(g_api_mutex)` ⇒ `AddSession` / `RemoveSession` 串行，
会话表（`std::map`）**不是线程安全的**。

---

## 决策

### 决策 1 — 用 pid，不用「闲置时长」

客户端在既有请求体里多写一行 `session.client_pid=<pid>`；服务端在
`AddSession` 里对**会话表**做一次惰性清扫：某条会话的宿主 pid **已知**、且该
进程**确实已经不在了** ⇒ 用**既有** `RemoveSession(ipc_id)` 回收它。

**为什么不用「按闲置时长回收」**：闲置时长**无法区分**「宿主开着但用户没打字」
和「宿主已经没了」。前者是**活会话**——回收它就是误杀活会话 ⇒ 该窗口哑到重新
聚焦为止（本仓库最严重的 bug 类：按键被吞，见 ADR 0062 的现场记录）。闲置时长
只能配一个「长到不太可能误伤」的阈值，而那个阈值要么迟迟不回收（泄漏照旧），
要么迟早误杀一次。pid 是**事实**：进程在就是宿主在。

### 决策 2 — 判据抽成纯函数共享头（constitution R7）

新增 `include/FluxingSessionReclaim.h`（**纯 ASCII**，含注释；RimeWithWeasel 的
编译单元没有 `/utf-8`）：

```
OwnerPidKnown(pid, self_pid) -> bool
ShouldReclaimSession(owner_pid_known, owner_process_alive) -> bool
ShouldReclaimOwner(owner, self_pid) -> bool
SelectReclaimableSessionIds(table, self_pid, out) -> void
```

生产代码（`RimeWithWeasel.cpp`）与单测（`TestFluxingCloud.cpp`）**共用这一份**，
单测不复制第二份判据。

### 决策 3 — 不改 `include/WeaselIPCData.h`，只在既有文本请求体里多写一行 key

- 线格式**零变化**：不新增字段、不新增消息类型、不改 `WeaselIPC.h` 的接口 ⇒
  **不触发** CLAUDE.md 的「IPC 四边同步」红线。
- **向后兼容**：旧客户端不发明这个 key ⇒ 服务端按「pid 未知」处理 ⇒ **永不回收**
  （退回到改动前的行为，安全方向）。
- **deployer 端零代码改动**：`WeaselIPC/WeaselClientImpl.cpp` 是 TSF 客户端、
  部署器客户端、测试客户端**共用**的实现 ⇒ 一处改三端生效。部署器
  （`WeaselDeployer/Configurator.cpp`）用的就是 `weasel::Client` ⇒ 它发出去的
  `StartMaintenance` / `EndMaintenance` 也会带上这个 key，但服务端对**维护态**的
  请求根本不走 `AddSession` ⇒ 不受影响。

### 决策 4 — 不做定时器、不起后台线程

清扫放在 **`AddSession` 内部**（IPC 线程，与正常 `RemoveSession` 同线程同语义，
`g_api_mutex` 串行）。

- 会话表是 `std::map`，**只在 IPC 线程被改** ⇒ 后台线程扫它就是数据竞争。
- 宪法 P2：后台线程不得做 UI 线程 I/O；`RemoveSession` 会 `m_ui->Hide()`、
  会改在场状态 ⇒ 放到后台线程会踩 UI 线程。
- 不起定时器 ⇒ 服务端不新增常驻线程 / 唤醒源。

清扫的约束：跳过**正在创建的本次会话**；维护期（`m_maintenance_active`）**不做**
清扫（会抢 userdb 的 leveldb LOCK，破坏云同步的 export / import）；回收动作一律
走**既有** `RemoveSession(ipc_id)`，不另写一套清理逻辑。

### 决策 5 — pid 表与 ADR 0032 的身份表同口径

- `ipc_id` 会被回收复用 ⇒ `AddSession` **先清同 id 旧值再记新值**（照抄
  `_RememberClientKind` 的做法与注释口径）。否则复用后的会话会带着**上一个宿主**
  的 pid ⇒ 清扫会把一条活会话当成孤儿回收掉。
- 身份必须活过 `RemoveSession`（TSF 的 Deactivate 是先 `EndSession` 再发
  `FocusOut`）⇒ **`RemoveSession` 不删这张 pid 表**。
- pid 表自己封顶（`kMaxRememberedOwnerPids = 256`，与
  `kMaxRememberedClientKinds` 同口径）⇒ 防长时间运行无界增长。表项被淘汰的最坏
  结果只是「该会话 pid 未知 ⇒ 永不回收」，是安全方向。

### 决策 6 — 进程存活探测：只在能**证明**死亡时才报死

生产侧（`RimeWithWeasel.cpp` 的 `_IsProcessAlive`）：

- `OpenProcess(SYNCHRONIZE, FALSE, pid)` 拿到句柄 ⇒ `WaitForSingleObject(h, 0)`：
  `WAIT_TIMEOUT` ⇒ 还活着；`WAIT_OBJECT_0` ⇒ 已退出（崩溃也算）。
- **句柄拿不到** ⇒ **不等于**「进程不在了」：宿主以管理员身份运行时，非提权的
  服务端连 `SYNCHRONIZE` 都会被拒（`ERROR_ACCESS_DENIED`）⇒ 报**活**（不回收）。
  只有「这个 pid 根本不存在」（`ERROR_INVALID_PARAMETER` / `ERROR_NOT_FOUND`）
  才报**死**。
- 其余异常（`WAIT_FAILED` 等）一律按**活**处理。

### 决策 7 — 豁免：pid <= 4 与服务端自身 pid 一律不回收

- `pid == 0` ⇒ 客户端没报（未知）⇒ 永不回收。
- `pid <= 4` ⇒ 0..4 不会是宿主应用 ⇒ 当未知处理。
- `pid == GetCurrentProcessId()` ⇒ 是我们自己 ⇒ 永不回收。

### 决策 8 — 最坏失败模式与防护

| 失败模式 | 方向 | 防护 |
|---|---|---|
| pid 复用（旧宿主死了，新进程拿到同一个 pid） | 只会「假活」⇒ **漏回收**（无害，泄漏照旧） | 复用出来的 pid 属于一个**活着的**进程 ⇒ 探测必然报活 |
| 权限不足拿不到句柄 | 报活 ⇒ **漏回收**（无害） | 决策 6 |
| 探测逻辑写反 / 判据写错 | 可能「假死」⇒ **误杀活会话** ⇒ 按键被吞 | 决策 2（判据只有一份、生产与单测共用）+ 决策 7 的豁免 + 单测覆盖 |

**唯一允许的失败方向是泄漏，不是误杀。**

---

## 落码状态

| 文件 | 内容 |
|---|---|
| `include/FluxingSessionReclaim.h` | **新建**：判据纯函数（纯 ASCII）+ 表级选择 |
| `WeaselIPC/WeaselClientImpl.cpp` | `_WriteClientInfo()` 多写一行 `session.client_pid=` |
| `include/RimeWithWeasel.h` | 新增私有成员 `m_session_owner_pids` + 三个私有方法声明 |
| `RimeWithWeasel/RimeWithWeasel.cpp` | `_ReadClientInfo()` 解析 pid；`_RememberOwnerPid()` / `_IsProcessAlive()` / `_ReclaimOrphanSessions()`；`AddSession()` 里调用清扫 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 新增 `TestSessionReclaimCriteria()`（判据真值表 + 空表 / 单条表 / 混合表） |
| `test/TestWeaselIPC/TestWeaselIPC.cpp` | 新增 `/verify-client-pid`：mock server 记录客户端自报的 pid，客户端读回并与自己的 pid 比对 |

**不改**：`include/WeaselIPCData.h`、`include/WeaselIPC.h`、任何消息类型、
任何用户可见文案、`librime/`。

---

## 残余风险（未闭环）

1. **宿主进程存活但窗口已销毁**：会话仍保留（宿主还活着，只是那个编辑框没了）。
   这是上游 weasel 的既有行为，**不在本 ADR 范围**。
2. **pid 复用 ⇒ 漏回收**：旧宿主的会话可能长期残留（被新进程「顶」掉了 pid）。
   代价 = 事实 2 的那三条（计数偏高 / 组合串宽限 60 s / rime 会话不释放），
   不堵死同步。
3. **清空只在 `AddSession` 触发**：崩溃后如果**再没有任何客户端建会话**，残留
   不会被清扫。真实使用中每次激活输入法都会建会话 ⇒ 事实上会被清掉；但
   「只崩溃、之后不用输入法」的场景下残留仍在。
4. **旧客户端不发明该 key** ⇒ 旧客户端的会话永不回收（向后兼容的代价）。
5. **判据与实现的对应关系靠单测兜底**：Win32 探测本身（`_IsProcessAlive`）在单测
   里**没有**直接覆盖（它是 Win32 副作用，不适合放进纯函数单测），只覆盖了喂给
   它的判据。

---

## 已驳回的备选方案

1. **按闲置时长回收会话** —— 驳回：无法区分「宿主活着但用户没打字」与「宿主已经
   没了」；阈值短了误杀活会话（本仓库最严重的 bug 类：按键被吞），阈值长了回收
   不掉（泄漏照旧）。见决策 1。
2. **改 `include/WeaselIPCData.h` 新增 pid 字段** —— 驳回：触发 CLAUDE.md 的
   「IPC 四边同步」红线（server / TSF / deployer 三端 + 一个 ADR + 一个
   `test/TestWeaselIPC/*` 测试，一次性提交），而收益为零：既有文本请求体已经能
   带任意 key（ADR 0032 的先例）。见决策 3。
3. **加定时器 / 后台线程定期清扫** —— 驳回：会话表非线程安全（只在 IPC 线程被
   改），后台线程扫它 = 数据竞争；且 `RemoveSession` 会碰 UI 与在场状态，踩
   宪法 P2。见决策 4。
4. **服务端主动 ping 客户端确认存活** —— 驳回：需要新增 IPC 消息（红线），且
   崩溃的宿主**根本不会回**—— ping 失败与「客户端暂时忙」无法区分，判据仍然是
   「猜」，不如直接问操作系统「这个 pid 还在不在」。
5. **由客户端在崩溃前登记一个「看门狗」句柄，服务端据此判断** —— 驳回：需要
   新增 IPC 消息与内核对象命名约定，复杂度远高于「问进程管理器 pid 还在不在」。
