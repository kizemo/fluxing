# ADR 0062 — 云同步的维护准入闸门：无活跃输入会话才允许 finalize，用户会话不得被同步打断

- **Status**: Proposed（决策已成；**用户可见口径待确认**；实现留后续批次）
- **Date**: 2026-11-14
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（阶段 G · T601 / T602 / T604）
- **Revises**: [ADR 0061](0061-rime-lifecycle-paired-maintenance-and-seh-guards.md) 的「残留缺陷」段（**不推翻** ADR 0061 的任何决策；本 ADR 只补上它显式留下的缺口）
- **Lessons**: `.specify/memory/lessons-learned.md` L219–L220
- **Supersedes / superseded by**: —（新增）

---

## 背景

批次 AQ（ADR 0061）消除了「含云构建启动约 30.2 s 崩溃 `0xC0000005`」，但**显式留下**一个残留缺陷：

> 每轮同步仍会 `finalize` + `Initialize` rime ⇒ **当前 TSF 会话被清空**（用户进行中的输入可能被中断）。

本批（AR）对该缺陷做源码级取证与决策推导。取证结论全部来自本仓源码，不依赖真机。

### 事实 1 — 会话被打断的机制（比「组合串丢失」更严重）

| # | 位置 | 事实 |
|---|---|---|
| 1 | `RimeWithWeasel/RimeWithWeasel.cpp:258-264` | `Finalize()` 清空 `m_session_status_map`（会话表） |
| 2 | `RimeWithWeasel/RimeWithWeasel.cpp:234-236` | `to_session_id()` 用 `operator[]` 访问会话表 ⇒ 表被清后对任意 client id 都得到 **session_id = 0** |
| 3 | `RimeWithWeasel/RimeWithWeasel.cpp:482-492` | `ProcessKeyEvent()` 把 0 传给 `process_key` ⇒ **按键被吞**（输入串进不了候选窗） |
| 4 | `WeaselIPC/WeaselClientImpl.cpp:196-201` | 客户端 `StartSession()` 只在**连接 / 激活**时发一次并缓存 `ipc_id` |
| 5 | `WeaselTSF/WeaselTSF.cpp:501-504` | 客户端**只在 `_Reconnect()` 时**才重新 `StartSession()` |

⇒ 服务端清表后，客户端缓存的 `ipc_id` 成为**孤儿** ⇒ 该输入窗口**哑到重新聚焦 / 重连为止**（不是丢一个组合串，而是整段输入失效）。

### 事实 2 — 为什么不能「备份完马上重建会话」：librime 的用户库是独占的 leveldb

| # | 位置 | 事实 |
|---|---|---|
| 6 | `librime/src/rime/lever/user_dict_manager.cc:51-71` | `UserDictManager::Backup` **自己新建一个 leveldb 实例**（`user_db_component_->Create(dict_name)` + `OpenReadOnly()`） |
| 7 | `librime/src/rime/dict/level_db.cc:54-58` | `LevelDbWrapper::Open(file_path, readonly)` 的 `readonly` **只影响 `create_if_missing`** ⇒ `OpenReadOnly()` 仍以**读写**方式打开、**仍吃 LOCK**；librime **没有真只读路径** |
| 8 | `librime/src/rime/dict/user_dictionary.cc:168-177` | `UserDictionary::Load()` 在字典加载时就 `db_->Open()` ⇒ 引擎**持有用户库 LOCK 直到字典卸载** |

⇒ 只要 rime 还活着（用户还在打字），备份就**必然锁失败**；要让备份成功就必须 `finalize()` 掉 rime ⇒ **必然清会话**。
**这两个需求在当前 librime 设计下互斥**，不是「顺序安排一下」能解决的问题。

### 事实 3 — 现有的失败 / 跳过语义

`WeaselServer/Cloud/CloudSyncEngine.cpp` 已有 `SyncOutcome::kSkipped`（配额跳过一类场景用过）；引擎现有节流 = 启动延迟 30 s / 防抖 5 min / 推送全局间隔 15 min / 定时 60 min 拉取 / 失败退避阶梯（末档 60 min）。

## 决策

### 决策 1 — 引入「维护准入闸门」：无活跃输入会话才允许进入维护

新增 `RimeWithWeaselHandler::CanEnterMaintenance(std::string* reason)`：

- **只读、无 I/O、不加锁**（遵 constitution P2：TSF 回调内禁阻塞 I/O）。
- 判据（取最严的充分条件）：`m_session_status_map` 中**不存在 `session_id != 0` 的条目**。
- 不通过 ⇒ 本轮同步**整轮跳过**：不 `finalize`、不导出 / 备份 / 回写、**不打断任何输入**。

**为什么取「无会话」而不是「无组合串」**：事实 1 表明副作用是「按键被吞直到重新聚焦」，后果远超组合串丢失 ⇒ 判据必须覆盖整条会话链。

### 决策 2 — 「跳过」不是「失败」

用已有的 `SyncOutcome::kSkipped` 表达「本轮不进入维护」：

- **不**递增 `consecutive_failures_`、**不**进入 60 min 失败退避阶梯（否则用户一次长时间打字就会把同步彻底停摆）；
- 改为**短间隔重试**（建议 **5 min**）—— 下一轮重新判闸门；
- 用户可见提示（简体中文）：「你正在输入，本轮已跳过用户词典同步，稍后会自动重试。」

### 决策 3 — 手动「立即同步」不绕过闸门

账号页手动同步在闸门不通过时：给明确中文提示 + 指引「请先切换到其他窗口，稍后会自动完成同步」，由短间隔重试自动完成。
**理由**：手动绕过 = 把「打断输入」重新引回 —— 正是本 ADR 要消除的用户可见缺陷。

### 决策 4 — 驳回三个替代方案（带源码依据）

1. **复制 userdb 目录后另开 leveldb** ⇒ 否决：需在 `WeaselServer` 引入 leveldb 依赖；**热拷贝一个活着的 leveldb 没有一致性保证**（memtable / WAL 可能在拷贝中途变化）；且**回写方向**仍需独占写入 ⇒ 仍然要 finalize。
2. **独立进程执行备份** ⇒ 否决：同样需要先复制目录（无一致性保证）；额外引入进程生命周期 + 取源库句柄的通道，复杂度与风险高于收益。
3. **改 librime 增加「释放 / 重开用户库」API** ⇒ 否决：`librime/` 是 submodule，**未经独立 PR 不得修改**（AGENTS.md §1）；需走上游，周期不可控 ⇒ 现不做。

### 决策 5 — 口径变更：同步变为「机会性（opportunistic）」

云端同步不再保证「固定间隔内一定完成」，而是**在用户没有活跃输入会话时完成**。

- **代价**：用户长时间停留在输入框时，云端可见性滞后（可能推迟数分钟乃至更久）。
- **收益**：输入不再被打断、输入法不再「哑掉」。
- 该口径**必须**写进用户可见文案与发版说明 —— 不可静默改变用户对同步时效的预期。

### 决策 6 — IPC 协议不变

本 ADR **不新增 / 不修改任何 IPC 消息** ⇒ **不触发** CLAUDE.md「IPC 四边同步」红线（server / TSF / deployer 三端 + ADR + 测试）。
闸门信息在 server 进程内即可获得（`RimeWithWeaselHandler` 与云同步引擎同在 `WeaselServer.exe`）。

## 后果

### ✅ 正面

- 同步不再打断输入：按键不丢、候选窗不消失、输入法不再哑到重新聚焦。
- 崩溃面收窄：备份只在「无会话」窗口进行 ⇒ leveldb 锁冲突窗口最小。
- 失败退避阶梯不再被「用户正在打字」污染（不再出现「同步静默停摆」）。

### ⚠️ 负面 / 代价

- **同步变成机会性**：用户持续输入时同步可能推迟（用户可见）。
- **闸门通过率未知**：部分宿主应用可能长期不销毁文本服务 ⇒ 长期不通过 ⇒ 同步长期挂起（待真机观察）。
- 需要新增短间隔重试路径 + 一个可观测状态（供账号页显示「等待你停止输入」）。

### 实现范围（下一批）

| 文件 | 改动 |
|---|---|
| `include/RimeWithWeasel.h` | 声明 `CanEnterMaintenance(std::string* reason)` |
| `RimeWithWeasel/RimeWithWeasel.cpp` | 实现（只读扫描 `m_session_status_map`） |
| `WeaselServer/Cloud/CloudRimeUserDictBackend.{h,cpp}` | `BeginMaintenance` 前置闸门判定；返回「跳过」语义 |
| `WeaselServer/Cloud/CloudSyncService.{h,cpp}` | 透传「跳过」语义 |
| `WeaselServer/Cloud/CloudSyncEngine.{h,cpp}` | `kSkipped` 不进退避、改短间隔重试；账号页展示中文状态 |
| `test/TestFluxingCloud/*` | 闸门判定 / `kSkipped` 不进退避 / 短间隔重试 单测 |

> ⚠️ **提交边界（硬约束）**：`RimeWithWeasel/RimeWithWeasel.cpp` 当前带 spec 045 的**未提交**改动（+118 行）⇒ 实现批次开工前**必须先单独提交或 stash**，**不得混提**。

### 验证计划

- **单测**：闸门判定纯逻辑（有会话 ⇒ 拒绝、无会话 ⇒ 放行）；`kSkipped` **不**累加 `consecutive_failures_`；跳过后按短间隔重试；基线 `PASS=453 FAIL=0` 不退化。
- **真机**：① 打字过程中触发同步 → 按键不丢、组合串与候选窗不消失；② 切到非输入窗口后 ≤ 5 min 自动完成一轮；③ 账号页手动同步在有会话时给中文提示且**不打断**当前输入。
- **门禁**：`build.bat weasel` **0 警告 0 错误**；云单测 `PASS ≥ 453 FAIL = 0`。

### 未闭环（落盘时如实登记）

- **闸门在真实宿主上的通过率未知**（需真机长时间观察；若某宿主长期不销毁文本服务 ⇒ 同步长期挂起 ⇒ 可能需要「宿主级宽限期」等二级口径）。
- **客户端侧自愈路径未逐条验证**：已核到服务端侧 `to_session_id` → 0 ⇒ 按键被吞；客户端仅在 `_Reconnect()` 时 `StartSession()` ⇒ 需真机确认「孤儿 `ipc_id`」的最终表现与恢复时机。
- **用户可见口径未定稿**：决策 5 的「机会性同步」表述与决策 3 的提示文案需产品侧确认（ADR 现为 Proposed 即因此）。
