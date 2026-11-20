# ADR 0065 — 云同步维护准入闸门：判据从「有无会话」改为「最近有输入」+ 会话惰性复活

- **Status**: Accepted（用户诉求明确：「希望开启输入法，不要影响同步」；判据方向与文案口径已定）
- **Date**: 2026-11-20
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（批次 AW）
- **Revises**: [ADR 0062](0062-cloud-sync-maintenance-gate-and-session-preservation.md) **决策 1**（判据由「无活跃输入会话」改为「无最近输入**且**无未上屏组合串」）。ADR 0062 的其余决策**保持有效**：决策 2（跳过后 5 分钟短间隔重试）、决策 3/5（用户可见中文文案，已由用户定稿「保留当前文案」）、决策 4（不复制 userdb / 不开独立进程 / 不改 librime）、决策 6（不新增 IPC 消息）
- **Lessons**: `.specify/memory/lessons-learned.md`（本批条目待写）
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 真机现象

装机 1.3.4 后，用户点「账号与同步 → 立即同步」，账号页状态行报：

> 你正在输入，本轮已跳过用户词典同步，稍后会自动重试。

而**当时并无任何输入动作**，只是输入法处于开启状态。用户诉求：「希望开启输入法，不要影响同步。」

### 事实 2 — ADR 0062 的判据把「有会话」当成了「在打字」

ADR 0062 决策 1 的判据是 `live_sessions <= 0` 才放行（`include/FluxingMaintenanceGate.h`）。
`live_sessions` = 会话表里 `session_id != 0` 的条目数。

而 TSF 文本服务**激活即建会话，与是否按键无关**：`WeaselTSF.cpp` 的 `ActivateEx` → `_EnsureServerConnected()`
→ `StartSession` → 服务端 `AddSession`。`FocusOut` 只清 `m_active_session`，`EndSession` 仅由
`Deactivate` 触发 ⇒ **只要输入法被激活过，会话就长期存在**。

⇒ 判据把「有一个 IME 会话」等同于「用户正在打字」，是**判据选错**，不是计数器 bug。

### 事实 3 — 长期驻留的宿主让会话永不消失

模块枚举实测（1.3.4 装机件）：`WeaselServer.exe` **不加载** `weasel.dll`（账号窗不自建 TSF 会话）；
全机唯一加载 `weasel.dll` 的进程是 `YXCalendar`（长驻日历窗口）⇒ 该宿主持有一个会话
⇒ `live_sessions` 恒 ≥ 1 ⇒ 闸门恒关闭 ⇒ **云同步永远不跑**。

### 事实 4 — 两条会话回收漏洞让「有会话」更粘

1. `RimeWithWeasel.cpp` 的 `RemoveSession` 在 `TryLazyRecovery()` / `m_disabled` 检查处**提前 return**，
   会话条目与 `m_live_session_count` **都不回收** ⇒ 失效会话永久驻留。
2. `to_session_id()` 用 `operator[]` 访问会话表，键不存在时**插入**一条 `session_id == 0` 的假条目
   （ADR 0062 已在注释里登记了这个坑，但没修）。
3. 管道异常断开时（`WeaselIPCServer.cpp` 管道线程）只 `_FinalizePipe`、**不 RemoveSession**
   ⇒ 孤儿会话永久驻留（本批暂缓，见「残余风险」）。

### 事实 5 — 维护必然销毁会话（ADR 0062 决策 4 已定）

`OpenReadOnly()` 的 `readonly` 只影响 `create_if_missing`，仍然吃 leveldb 的 LOCK；
`UserDictManager::Backup` 自建实例 ⇒ 必须先 finalize ⇒ **「备份成功」与「会话存活」互斥**。
⇒ 放宽闸门后，维护**真的会**销毁会话；不补复活机制就会把 ADR 0062 想避免的「按键被吞」重新引进来。

---

## 决策

### 决策 1 — 判据改为「最近有输入」+「未上屏组合串」，纯函数落在共享头

`include/FluxingMaintenanceGate.h`（**唯一事实源**，constitution R7：生产与单测共用同一份实现）：

```
MaintenanceGateAllowsWithIdle(live_sessions, has_input_history,
                              ms_since_last_input, composing_sessions)
```

| 条件 | 放行？ |
| --- | --- |
| `live_sessions <= 0` | 放行（根本没有 IME 会话） |
| `has_input_history == false` | **放行**（★ IME 已激活但从未按键 ⇒ 没有可丢的东西） |
| 有组合串 | 关闭，直到空闲超过 **60 s** |
| 无组合串 | 关闭，直到空闲超过 **10 s** |

配套常量与助手：`kMaintenanceIdleGraceMs = 10 s`、`kMaintenanceCompositionGraceMs = 60 s`、
`MaintenanceGateIdleMs(now_tick, last_input_tick)`（0 表示「从未按键」，时钟回拨按 0 处理）。

- **保留** `MaintenanceGateAllows(int)` / `MaintenanceGateReasonLiveSession()` /
  `kMaintenanceSkipRetryIntervalMs` 三个既有符号**一字不改** ⇒ 既有单测断言不破。
- **原因令牌仍是单一 `live_input_session`**：云侧中文文案是固定文案而非按令牌映射
  （`WeaselServer/Cloud/CloudUserDictIo.h`），新判据下三处文案语义仍准确 ⇒ **不动用户可见文案**。

### 决策 2 — 事实由生产侧以原子量提供，读者只读、不加锁、不做 I/O

`RimeWithWeasel` 侧新增三个 `std::atomic`：

| 成员 | 含义 | 写者 |
| --- | --- | --- |
| `m_last_input_tick` | 最近一次按键的 `GetTickCount64()` 毫秒值（0 = 从未按键） | IPC 线程（`ProcessKeyEvent`） |
| `m_composing_session_count` | 当前持有未上屏组合串的会话数 | IPC 线程 |
| `m_maintenance_active` | 维护态进行中 | 云同步线程（`StartMaintenance`/`EndMaintenance`） |

闸门检查发生在**云同步线程**，判据应用在 `RimeWithWeasel::CanEnterMaintenance()`，
读的都是 `load()` ⇒ 满足 constitution P2（TSF/读者侧禁阻塞 I/O、禁加锁、禁扫 `std::map`）。

### 决策 3 — 组合串用更长的宽限期

finalize 会丢掉未上屏的组合串 ⇒ 组合串按 60 s 宽限、普通按键按 10 s 宽限。
理由：组合串是**用户正在看着、还没上屏**的内容，丢弃的代价高于「多等一会儿再同步」。

### 决策 4 — 维护窗口内禁止创建/销毁 rime 会话

云回合期间（`BeginMaintenance` 取到 `Local\Fluxing.RimeLifecycle` 生命周期锁直到回合结束）
任何 `rime_api->create_session()` 都会加载词典、抢 userdb 的 leveldb LOCK ⇒ **破坏 export/import**。

⇒ 两道闸：
1. `m_maintenance_active` 为真时，IPC 线程**不**创建/销毁 rime 会话；
2. 会话复活前用 `fluxing::rime_lifecycle::TryAcquire(DefaultLockName(), 0)` 探测回合是否在进行
   （拿不到 ⇒ 回合中 ⇒ 不复活），拿到则持锁完成复活再释放。

### 决策 5 — 会话惰性复活（不放宽闸门就必须补，否则重引入「按键被吞」）

`ProcessKeyEvent` 里当解析出的 `session_id == 0`（孤儿）时**先尝试复活再处理按键**：

- 复活用 `m_session_schema_ids`（跨维护保留）恢复方案，必要时另存 ascii 模式；
- 复活失败 ⇒ 保持既有行为（不引入新的失败路径）。

复活**不得**放在 `EndMaintenance`（回合开头）—— 那里会重建会话、加载词典、抢走 userdb LOCK，
**直接破坏 export/import**。放在按键路径上则天然避开 export/import 窗口。

### 决策 6 — 修会话回收的两条漏洞

- `RemoveSession`：把「erase 条目 + 重算计数」提到早退检查**之前** ⇒ 失效会话必被回收。
- `to_session_id`：改用 `find`，**不再插入** `session_id == 0` 的假条目。

### 决策 7 — IPC 协议不变、用户可见文案不变

本决策**不新增/不修改任何 IPC 消息** ⇒ 不触发「四边同步 + ADR + IPC 测试」红线。
不新增第三方依赖；不改 `librime/`。

---

## 落码状态（2026-11-20 · 批次 AW）

- 共享判据：`include/FluxingMaintenanceGate.h`（新增 2 常量 + 2 纯函数）。
- 生产侧：`include/RimeWithWeasel.h` + `RimeWithWeasel/RimeWithWeasel.cpp`
  （3 个原子量 + 判据应用 + 回收漏洞修复 + 惰性复活）。
- 单测：`test/TestFluxingCloud/TestFluxingCloud.cpp` 新增 `TestMaintenanceGateIdleCriteria()`
  （判据矩阵 + 常量 + 空闲毫秒助手），与既有 `TestCloudMaintenanceGate()`（旧判据 / 整轮跳过 /
  跳过≠失败）并存 ⇒ 旧断言不破、新判据有独立断言。

---

## 残余风险（未闭环）

1. **管道异常断开的孤儿会话回收**（事实 4 第 3 条）本批**暂缓**：放宽闸门后孤儿不再阻塞同步，
   但它们仍会占用 `live_sessions` 计数（不影响放行，因为判据只看「最近有输入」）。
   彻底修需要 per-pipe 追踪会话归属，改动面偏大。
2. **真机端到端未做**：1.3.5 装机 + 重启前，TSF 侧仍是旧 `weasel.dll` ⇒
   依赖 TSF 会话行为的验收结论不可信。
3. **`live_sessions` 与「实际在打字」的偏差**：判据用「最近一次按键」的毫秒时间戳，
   不区分「哪个窗口在打字」⇒ 用户在 A 窗口打字、焦点在 B 窗口时，闸门仍会关闭 10 s。
   这是**有意的保守取舍**（宁可少同步一次，也不要打断正在输入的窗口）。
4. **维护期间新建会话被挡**：IME 恰好在云回合窗口内被激活时，`AddSession` 不创建 rime 会话
   ⇒ 该窗口要重新聚焦才恢复（既有行为不变）。

---

## 已驳回的备选方案

1. **让 TSF 上报「正在输入」状态（新增 IPC 消息）** —— 驳回：触发「server/TSF/deployer 四边同步 +
   ADR + `test/TestWeaselIPC/*` 测试」红线，收益不抵成本。
2. **判据改成「有没有组合串」（ADR 0062 决策 1 当时放弃的备选）** —— 驳回：
   `RimeStatus::is_composing` 只在**有组合串时**为真，而「用户刚打完字、组合串已上屏」的那 10 s 里
   它为假 ⇒ 单靠它会让同步在用户连续打字时频繁打断输入窗口。
3. **不销毁会话（复制 userdb 另开 leveldb / 独立进程备份 / 改 librime）** —— 沿用 ADR 0062 决策 4 的驳回理由。
4. **把「最近有输入」做成 IPC 消息** —— 同 1。
