# 批次 AR 收尾 handoff — 残留缺陷「每轮同步清空当前 TSF 会话」的 ADR 取证与决策落盘

- **批次**: AR（接续 AQ）
- **日期**: 2026-11-14
- **分支**: `feat/100-cloud-sync`（**未 push**）
- **本批性质**: **纯文档批次**（ADR + 索引 / 矩阵 / 任务 / 经验 / 指针 / handoff 回写）⇒ **未改任何产品代码，未跑构建与单测**（AQ 基线仍有效：云单测 `PASS=453 FAIL=0`、`build.bat weasel` 0 警告 0 错误、1.3.1 产物在位）
- **本批唯一交付物**: [ADR 0062](../../../adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md)（Status = **Proposed**）

---

## 1. 背景与身份

spec 100「云账号与多端同步」在批次 AQ 已把最致命的一环闭环：**含云构建启动约 30.2 s 崩溃 `0xC0000005`**（根因 = 云同步 `finalize()` → `Registry::Clear()` 销毁 `userdb` ⇒ levers 空指针解引用；修法 = Start/End 配对 + 命名信号量互斥 + 5 个 POD-only SEH 护栏），并重新出包 **1.3.1**。

AQ 的 ADR 0061 在「后果」段**显式留下**一个残留缺陷：

> 每轮同步仍会 finalize + Initialize rime ⇒ **当前 TSF 会话被清空**（用户进行中的输入可能被中断）；彻底解法需另立 ADR。

批次 AR 的任务之一就是「另立 ADR 处理这条残留缺陷」。**本批完成了取证与决策推导并落盘 ADR 0062，但未实现**（实现必然触碰脏文件 `RimeWithWeasel/RimeWithWeasel.cpp`，混提风险不可接受 ⇒ 留给下一批）。

## 2. 本批目标 / 范围 / 明确没做

| | 内容 |
|---|---|
| ✅ 做了 | ① 读取 AQ handoff / tasks T601–T604 / CLAUDE.md §2 指针，掌握背景与进度；② 复核仓库状态（分支、脏文件、提交历史、1.3.1 安装包哈希）；③ 对残留缺陷做**源码级取证**（会话被打断的真实机制、librime leveldb LOCK 的硬约束、现有失败/跳过语义）；④ 推导并落盘 **ADR 0062**（6 条决策 + 4 个被驳回替代方案 + 实现范围 + 验证计划）；⑤ 回写 ADR 索引 / 验收矩阵 / tasks T604 / lessons L219–L220 / CLAUDE.md 交接指针；⑥ 写本 handoff |
| ❌ 明确没做 | ① **未改任何产品代码**（闸门未实现）；② **未跑构建与单测**（无代码改动 ⇒ 不重跑 AQ 已验证项，遵「已验证部分不要重做」）；③ **未装机**（PPL ⇒ 须用户注销/重启）；④ **未跑用户在场 AC 4 条**、**未跑双机 AC 7 条**、**未跑 restore 端到端**（均需用户在场 / 第二台设备）；⑤ **未测输入延迟两组对照**（需登录态） |

## 3. 已验证 / 推断 / 未验证

### 3.1 已验证（本批实测，源码级复核 + 仓库状态复核）

| # | 事实 | 证据 |
|---|---|---|
| V1 | 会话被打断的**真实机制**不是「组合串丢失」，而是**按键被吞直到重新聚焦**：`Finalize()` 清 `m_session_status_map`（`RimeWithWeasel.cpp:258-264`）⇒ `to_session_id()` 用 `operator[]` 返回 **session_id = 0**（`:234-236`）⇒ `ProcessKeyEvent()` 把 0 传给 `process_key`（`:482-492`）⇒ 按键被吞 | 源码复读 |
| V2 | 客户端**只在连接/激活时 `StartSession()` 一次并缓存 `ipc_id`**（`WeaselClientImpl.cpp:196-201`），**只有 `_Reconnect()` 才重建会话**（`WeaselTSF.cpp:501-504`）⇒ 服务端清表后客户端 `ipc_id` 成**孤儿** | 源码复读 |
| V3 | **librime 没有真只读路径**：`LevelDbWrapper::Open(file_path, readonly)` 的 `readonly` 只影响 `create_if_missing`（`level_db.cc:54-58`）⇒ `OpenReadOnly()` 仍吃 LOCK | 源码复读 |
| V4 | `UserDictManager::Backup` **自己新建 leveldb 实例**（`user_dict_manager.cc:51-71`）⇒ 必须独占 LOCK；而 `UserDictionary::Load()` 在字典加载时就 `db_->Open()`（`user_dictionary.cc:168-177`）⇒ 引擎持锁到字典卸载 | 源码复读 |
| V5 | ⇒ **「备份成功」与「会话存活」在当前 librime 设计下互斥**（不是顺序问题） | V3+V4 推论（源码已闭合） |
| V6 | `SyncOutcome::kSkipped` **已存在**（`CloudSyncEngine.cpp:192-194`，配额跳过用过）⇒ 表达「跳过而非失败」**无需新增枚举** | 源码复读 |
| V7 | 仓库状态：分支 `feat/100-cloud-sync`；脏文件 = `M RimeWithWeasel/RimeWithWeasel.cpp`（spec 045，+118 行，未提交）+ `m librime`（submodule）+ 3 个未跟踪 `docs/design/quickpanel-v6/design-demos/*.html` | `git status -sb` |
| V8 | 提交历史：`da671a6`（AQ 补记）/ `54f9d29`（AQ 修复 + 1.3.1 出包）/ `3e612d7`（AP 收尾），**均未 push** | `git log --oneline -3` |
| V9 | 1.3.1 安装包在位：`output/archives/fluxing-1.3.1-installer.exe` = **42 963 252 B** · mtime 2026-11-12 20:00:50 · sha16 **`71286E7671052F37`**（与 AQ 记录一致） | `Get-Item` + `Get-FileHash` |

### 3.2 推断（源码级推导，**未真机验证**）

- I1：V1+V2 ⇒ 同步轮次落在输入中时，该窗口会「哑到重新聚焦 / 重连为止」。**未真机复现**（本批无代码改动，无法验证）。
- I2：闸门（无活跃会话才放行）在大多数桌面场景下会在**用户切到非输入窗口后**很快通过 ⇒ 短间隔重试 5 min 足够覆盖；**但部分宿主可能长期不销毁文本服务** ⇒ 可能长期不通过 ⇒ 同步长期挂起（未验证，见 §4）。
- I3：手动「立即同步」若绕过闸门，会把「打断输入」重新引回 ⇒ 决策 3 选择「不绕过 + 中文提示 + 自动重试」。**未做用户体感评估**。
- I4：短间隔 5 min 与现有「防抖 5 min / 定时 60 min」节流的叠加关系未做数值仿真，可能出现「跳过轮次吃掉定时槽」的交互（**未验证**）。

### 3.3 未验证（明确登记，不留「待定」）

- U1：闸门实现与其单测（**未实现**）。
- U2：闸门在真实宿主上的通过率；「宿主长期不销毁文本服务」的对策。
- U3：客户端孤儿 `ipc_id` 的真机表现与恢复时机。
- U4：ADR 0062 的**用户可见口径**（「机会性同步」表述、决策 3 的提示文案）⇒ ADR 状态保持 **Proposed**。
- U5：装机 1.3.1 后的「不再 30 s 崩溃」实机确认（需注销/重启）。
- U6：用户在场 AC 4 条、双机 AC 7 条、restore 端到端、输入延迟两组对照。

## 4. 未闭环清单（下一批的输入）

| # | 事项 | 阻塞原因 |
|---|---|---|
| 1 | **处理 `RimeWithWeasel/RimeWithWeasel.cpp` 的脏改动**（spec 045，+118 行，未提交） | 实现批次必然要改该文件 ⇒ 必须先**单独提交或 stash**，否则与 spec 100 混提 |
| 2 | **ADR 0062 实施**：闸门 + `kSkipped` 不进退避 + 短间隔重试 + 单测 | 依赖 #1；且需先确认 U4 的用户可见口径（否则实现后文案还要再改） |
| 3 | **装机 1.3.1**（`output/archives/fluxing-1.3.1-installer.exe`，sha16 `71286E7671052F37`） | `WeaselServer.exe` 是 **PPL** ⇒ 必须**注销 / 重启系统**；**1.3.0 勿装**；当前装机版仍是 1.2.1.0（含云 ⇒ 每启动约 30 s 崩一次） |
| 4 | **用户在场 AC 4 条**（AC-01 亲手登录 `kizemo@163.com` / AC-02 重启后自动续期 / AC-06 断网 / AC-10 清空云端数据） | 需要用户在场 + 登录态（口令明文不存，无法代登） |
| 5 | **双机 AC 7 条**（AC-03/04/05/08/09/13/14）+ **restore 路径端到端** | 需要第二台设备或**第二个 Windows 账户**（`CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 两套 Profile 目录**不够**） |
| 6 | **输入延迟两组对照** | 需登录态；IME 自绘窗跨进程抓不到 |
| 7 | ADR 0062 状态 Proposed → Accepted | 依赖 U4（用户可见口径确认） |
| 8 | AC-07「真实客户端 HTTPS 抓包」补强 / 严格同状态 A/B 复跑 | 可选补强项 |

## 5. 关键决策（ADR 0062 摘要）

- **决策 1** 新增 `RimeWithWeaselHandler::CanEnterMaintenance(std::string* reason)`：只读、无 I/O、不加锁；判据 = 会话表中**不存在 `session_id != 0`** 的条目；不通过 ⇒ 本轮**整轮跳过**（不 finalize / 不备份 / 不回写）。
- **决策 2** 「跳过」≠「失败」：用已有 `SyncOutcome::kSkipped`；**不**递增 `consecutive_failures_`、**不**进 60 min 退避；改**短间隔重试（建议 5 min）**；中文提示「你正在输入，本轮已跳过用户词典同步，稍后会自动重试。」
- **决策 3** 手动「立即同步」**不绕过**闸门 ⇒ 中文提示 + 指引「请先切换到其他窗口，稍后会自动完成同步」。
- **决策 4** 驳回：① 复制 userdb 目录另开 leveldb（无一致性保证 + 回写方向仍要独占）；② 独立进程备份（同样要先拷贝 + 进程生命周期/IPC 复杂度）；③ 改 librime 加「释放/重开用户库」API（submodule 策略，需上游 PR）。
- **决策 5** **口径变更**：同步变为「**机会性**」—— 用户持续输入时云端可能滞后；必须写进用户可见文案与发版说明。
- **决策 6** **IPC 协议不变** ⇒ 不触发 CLAUDE.md「IPC 四边同步」红线。

## 6. 工具坑（本批新增 / 沿用）

**本批新增**

1. **编码分两类**：`docs/**`、`.specify/specs/**`、`CLAUDE.md` = **UTF-8 无 BOM + LF**；`.specify/memory/lessons-learned.md` = **UTF-8 带 BOM + CRLF** ⇒ 追加必须区别处理（写错 ⇒ L0 判红）。
2. **CLAUDE.md 的交接指针是超长单行**（§2 里每批一条，动辄上千字）⇒ 改写必须**按行处理**（脚本），不要用字符串匹配长文。
3. **批次日期取会话当日**（本批 2026-11-14），与 AQ（2026-11-12）不同 ⇒ 文件名 / ADR Date / 索引都要同步。

**沿用 AQ（不重复展开，详见 AQ handoff §6）**

- `git commit -F -` **不吃 stdin**（消息写临时文件再 `-F <file>`）；提交前必须 `git status -sb` + `git diff --stat` 排除三类脏文件。
- `version.txt` 在 HEAD 里 **6 字节无尾换行** ⇒ 改版本须以 HEAD 字节为模板**等长替换**。
- `env.bat` 用 `set "K=V"` 带引号格式（正则 `^SET\s+K=` 整组漏匹配）。
- MSVC **C2712**：含 `__try` 的函数只能有 POD 局部变量。
- 命名信号量**先 `ReleaseSemaphore` 再 `CloseHandle`**；取锁上限 2 s，失败即跳过本轮。
- `WeaselServer.exe` 是 **PPL** ⇒ `taskkill` 禁用，只能 `WeaselServer.exe /quit`；工作区副本才可 `Stop-Process -Force`。
- `/utf-8` 窄字符串 ⇒ 产物里中文是 **UTF-8 字节**（用 UTF-16 搜索会假红）。
- 无调试器可用（cdb / windbg / procdump / dumpchk 全缺）⇒ 靠源码 + 自建行级 trace。
- 空闲 CPU 测量不得与构建并发；窗口下限 16 s、单核口径（ADR 0060）。

## 7. 下一批建议顺序（沿用 AQ §7 顺序 + 本批新增）

1. **处理 `RimeWithWeasel.cpp` 脏改动**（单独提交或 stash）—— 这是实现批次的硬前置。
2. **请用户确认「机会性同步」口径**（ADR 0062 决策 5）与手动同步提示文案（决策 3）⇒ ADR 转 Accepted。
3. **实现 ADR 0062**：`CanEnterMaintenance` 闸门 + `kSkipped` 不进退避 + 5 min 短间隔重试 + 账号页中文状态 + 单测（基线 `PASS=453 FAIL=0` 不退化）+ `build.bat weasel` 0 警告 0 错误。
4. **装机 1.3.1**（注销 / 重启后）+ **用户在场 AC 4 条**（AC-01/02/06/10）。
5. **双机 AC 7 条** + **restore 路径端到端**（第二台设备或第二个 Windows 账户）。
6. **输入延迟两组对照**（依赖登录态）。
7. AC-07 真实客户端抓包补强 / 严格同状态 A/B 复跑（可选）。

## 8. 路径索引

| 内容 | 路径 |
|---|---|
| **本批唯一交付物（ADR 0062）** | `docs/adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md` |
| ADR 索引 | `docs/adr/README.md` |
| 验收矩阵（§2.1 批次 AR 段） | `docs/research/fluxing-cloud-2026-11/acceptance-matrix.md` |
| 任务台账（T604 批次 AR 段） | `.specify/specs/100-fluxing-cloud-account-sync/tasks.md` |
| 经验沉淀（L219–L220） | `.specify/memory/lessons-learned.md` |
| 进站指针 | `CLAUDE.md` §2 |
| 上一批 handoff（本批起点） | `docs/archive/handoffs/handoff-2026-11-12-batch-aq-rime-lifecycle-crash-fix.md` |
| 前一批 handoff（AP） | `docs/archive/handoffs/handoff-2026-11-12-batch-ap-ac11-attribution-and-release-1-3-0.md` |
| 1.3.1 安装包 | `output/archives/fluxing-1.3.1-installer.exe`（42 963 252 B · sha16 `71286E7671052F37`） |
| AQ 证据 | `tmp/AQ-trace-probe.txt`、`tmp/AQ-probe-run.log`、`tmp/AQ-probe-run2.log`、`tmp/AQ-build3.log`、`tmp/AQ-cloudtest.log`、`tmp/AQ-installer.log` |

> ⚠️ **接力 prompt 按用户 2026-11-08 指示直接写在当轮回答正文**，**不**新建 `docs/archive/prompts/` 文件。
