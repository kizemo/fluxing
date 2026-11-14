# 批次 AS · ADR 0062 实现落地（维护准入闸门 + 会话保护）

- **日期**：2026-11-14
- **分支**：`feat/100-cloud-sync`（**未 push**）
- **上一批**：[批次 AR · 残留缺陷「每轮同步清空当前 TSF 会话」ADR 0062 取证与决策落盘（实现未做）](./handoff-2026-11-14-batch-ar-session-preservation-adr.md)
- **本批性质**：**代码实现批次**（ADR 0062 落码 + 单测 + 门禁 + 文档）——**未改 `librime/`、未改 IPC 协议、未新增第三方依赖**
- **ADR**：[0062-cloud-sync-maintenance-gate-and-session-preservation.md](../../adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md)

---

## 1. 本批目标

把 AR 批只落盘、没实现的 ADR 0062 真正做出来：**没有活跃输入会话时，才允许云同步进入维护态（finalize）** ⇒ 用户正在打字时同步自动延后，输入不再被打断。

同时清掉 AR 批留下的硬前置：`RimeWithWeasel/RimeWithWeasel.cpp` 的 spec 045 脏改动必须先单独提交。

---

## 2. 已完成

### 2.1 提交边界（硬前置）已解除

`8da2f72` `fix(RimeWithWeasel): 持久化用户显式选定的输入方案并按目标会话校验切换结果（spec 045 第三轮）`（1 file / +115 −3）—— 提交前 `cmd /c "build.bat weasel"` 成功（0 警告 0 错误）。ADR 0062 的实现**未混提**。

### 2.2 落码范围（9 个文件 + 1 个新头，33 处补丁）

| 文件 | 内容 |
|---|---|
| `include/FluxingMaintenanceGate.h` | **新增**：判据纯函数（`MaintenanceGateAllows` / `MaintenanceGateReasonLiveSession` / `kMaintenanceSkipRetryIntervalMs` = 5 min）。**纯 ASCII** |
| `include/RimeWithWeasel.h` | 公有 `CanEnterMaintenance(std::string* reason)`；私有 `std::atomic<int> m_live_session_count` + `_RefreshLiveSessionCount()` |
| `RimeWithWeasel/RimeWithWeasel.cpp` | 计数重算（拥有者线程）+ 只读闸门；`AddSession` / `RemoveSession` / 析构 / `Finalize` / `StartMaintenance` / `EndMaintenance` 六处刷新 |
| `WeaselServer/Cloud/CloudUserDictIo.h` | `IUserDictBackend::CheckMaintenanceGate`（默认放行）；三个中文文案 inline 函数 |
| `WeaselServer/Cloud/CloudRimeUserDictBackend.cpp` | 覆写闸门（转问维护态宿主；宿主未注册 ⇒ 放行）；`BeginMaintenance` 纵深防御 |
| `WeaselServer/Cloud/CloudSyncService.cpp` | `SyncOnce` 在 **`LoadSidecar` 之前**查闸门（关闭 ⇒ 整轮跳过、零 I/O、零配额）；`BeginMaintenance` 失败时复查闸门 ⇒ 归类为「跳过」 |
| `WeaselServer/Cloud/CloudSyncEngine.{h,cpp}` | `kSkippedRetry` / `result.skipped` / `status.waiting_for_idle` / `options.skip_retry_interval_ms` / `input.skip_pending` + `skip_retry_at_ms`；`DecideNextAction` 跳过窗口判定；`OnRoundFinishedLocked` 跳过早退分支 |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | 成功 / 已跳过 / 失败三分支；手动跳过提示 + 指引；「机会性同步」静态说明行；窗口高 472 → 490，控件整体下移 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | `FakeGateBackend` / `SkippingSyncRunner` / `TestCloudMaintenanceGate()`（**20 条断言**） |

### 2.3 判据的实现口径（刻意偏离 ADR 字面）

ADR 写的是「只读扫描 `m_session_status_map`」。**实现没这么做**：会话表由 IPC 线程写、云同步后台线程读 ⇒ 读者直接扫 `std::map` 既是数据竞争，又会在 TSF 路径引入锁（踩穿 constitution P2）。

实现 = **原子计数 + 拥有者线程重算**：`m_live_session_count`（`std::atomic<int>`）在会话生命周期事件上由拥有者线程重算并 `store`；读者只 `load()`。

**必须显式排除 `session_id == 0`**：`to_session_id()` 用 `operator[]` 访问会话表，键不存在时会**插入**空 status（`session_id = 0`）⇒ 假条目。若不排除，一个从未存在过的 `ipc_id` 就能把云同步永久堵死。

全仓 grep 确认：`SessionStatus::session_id` **只在 `AddSession` 一处赋值** ⇒ 计数不会被别的路径污染。

### 2.4 「跳过 ≠ 失败」的具体语义

引擎的跳过分支**不**递增 `consecutive_failures_`、**不**进 60 min 失败退避阶梯、**不**置 `ever_synced` / `last_sync_ms`、**不**动推送 / 拉取计数；改 **5 min 短间隔重试**（`SyncTrigger::kSkippedRetry`）。

**启动轮被跳过时必须恢复 `startup_pending_ = true`** —— 否则启动拉取永久丢失（引擎 `ThreadMain` 在跑轮前已把该标记置 false）。

---

## 3. 已验证 / 推断 / 未验证

### ✅ 已验证（可复跑）

| 项 | 证据 |
|---|---|
| 云单测 **`PASS=473 FAIL=0`**（基线 453 ⇒ +20） | `tmp/build-fluxing-cloud-tests.cmd`；输出 `tmp/adr0062-cloudtest.txt` |
| **有牙验证 `TEETH_VERDICT=PASS`**：MUT_A（协调器闸门失效，注入 2 处）⇒ `PASS=471 FAIL=2`；MUT_B（跳过被记成失败 / 不报「等待停止输入」）⇒ `PASS=471 FAIL=2`；还原 ⇒ `PASS=473 FAIL=0` | `tmp/adr0062-teeth2.ps1` → `tmp/adr0062-teeth2-out.txt` |
| 生产构建 **0 警告 0 错误**（格式化前后各一次，`BUILD_EXIT=0`） | `tmp/adr0062-build.txt` / `tmp/adr0062-build2.txt` |
| clang-format 只格式化**本次新增行范围**（`--lines`）⇒ 测试文件**零删除行**；`CloudSyncEngine.h` 的 5 行删除只是**枚举注释对齐**（新增 `kSkippedRetry,` 触发整块对齐，CI 同样要求） | `tmp/adr0062-diff2.txt` |
| 换行风格已归位：`RimeWithWeasel.cpp` `CRLF=2786 LF=0`；`include/RimeWithWeasel.h` `CRLF=377 LF=0`；其余 7 个文件纯 LF | 批内 EOL 自检输出 |
| 闸门在**协调器层**确实做到零 I/O（未进入维护态 / 未导出 / 未发任何请求 / 零推送零拉取） | 单测断言② |

### 🔍 推断（未实测）

- **闸门通过率未知**：真机上「用户正在打字」时 `m_live_session_count` 的实际分布没量过；某些宿主可能长期不销毁文本服务 ⇒ 同步长期挂起（可能需要「宿主级宽限期」二级口径）。
- **「5 min 后自动完成」的用户体感**：引擎短间隔重试走的是**单调钟**唤醒；账号页显示的 `next_attempt_ms` 用墙钟换算，跨系统时间跳变时两者会不一致（未实测）。
- **账号页布局**：490 px 新高度下控件不重叠是靠**坐标算术**保证的（说明行 256→274、列表框 278→408、按钮行 414→442 / 448→476），**未做像素级真机目视**。

### ❌ 未验证

- **未装机**：当前装机版仍是 **1.2.1.0**，ADR 0062 的代码**从未在真机跑过**。
- **真机端到端**：打字中触发同步不丢按键 / 组合串 / 候选窗；切到非输入窗口后 ≤ 5 min 自动完成一轮；账号页手动同步在有会话时给提示且不打断输入。
- **双机 7 条 AC（AC-03/04/05/08/09/13/14）+ restore 端到端**：需要第二台设备或第二个 Windows 账户（`CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 两套 Profile 目录不够）。
- **用户在场 4 条 AC（AC-01/02/06/10）+ 输入延迟两组对照**：需用户注销 / 重启后亲手装 1.3.1 并登录 `kizemo@163.com`。
- **客户端孤儿 `ipc_id` 的最终表现与恢复时机**（AR 批已核到服务端侧链路，真机未验）。

---

## 4. 未闭环清单

1. **用户可见口径待确认**（问卷 `ask_e7d252928e37e4bd34d8ba4c` 发出后未答复）⇒ ADR 0062 仍为 **Proposed**。当前**按推荐项实现**：
   - 跳过提示：「你正在输入，本轮已跳过用户词典同步，稍后会自动重试。」
   - 手动指引：「请先切换到其他窗口，稍后会自动完成同步。」
   - 账号页说明行：「你正在输入时同步会自动延后，停止输入后自动完成。」

   若用户改口径 ⇒ 改 `WeaselServer/Cloud/CloudUserDictIo.h` 三个文案常量 + 账号页静态行 + ADR 0062 + CHANGELOG。
2. **装机 1.3.1 + 用户在场 AC 4 条 + 输入延迟两组对照**（`WeaselServer.exe` 是 **PPL** ⇒ 装机前必须注销 / 重启；`taskkill` 禁用，只能 `WeaselServer.exe /quit`；**1.3.0 勿装**）。
3. **双机 AC 7 条 + restore 路径端到端**（需第二台设备或第二个 Windows 账户）。
4. **闸门通过率**（真机长时间观察）。
5. **孤儿 `ipc_id` 真机表现**。
6. **发布**：本批**未出包**（出包属于「装机后验证」那一轮；1.3.1 已有产物，但不含本批改动 ⇒ 需重新出 **1.3.2** 或更高）。

---

## 5. 证据路径

| 内容 | 路径 |
|---|---|
| ADR 0062（已更新实现记录） | `docs/adr/0062-cloud-sync-maintenance-gate-and-session-preservation.md` |
| 本批完整 diff | `tmp/adr0062-diff.txt`（含引擎 / 布局 / 单测全量）、`tmp/adr0062-diff2.txt`（格式化后的引擎头 + 测试文件） |
| 落码脚本 | `tmp/adr0062-apply.ps1`（首跑，17 处）、`tmp/adr0062-apply2.ps1`（健壮续跑，16 处） |
| 有牙验证 | `tmp/adr0062-teeth2.ps1` → `tmp/adr0062-teeth2-out.txt` |
| 云单测输出 | `tmp/adr0062-cloudtest.txt` |
| 构建输出 | `tmp/adr0062-build.txt`、`tmp/adr0062-build2.txt` |

---

## 6. 工具坑（本批实测）

1. **补丁脚本锚点：ASCII `=>` vs 全角 `⇒`** —— 源码注释用 `⇒`（1 字符），脚本锚点写成 `=>`（2 字符）⇒ 按字符串 `IndexOf` 永远失配。修法：归一化匹配（箭头统一成同一 token）+ **归一化偏移换算回原文偏移**再替换（否则会破坏原文行尾风格）。只做「归一化后替换」会把 CRLF 文件写成 LF。
2. **CRLF 文件被写成 LF**：`RimeWithWeasel.cpp`（2 行）与 `include/RimeWithWeasel.h`（9 行）在替换后变成 LF ⇒ 与全文 CRLF 混用。**判据**：逐文件统计 `CRLF=` 与 `(?<!\r)\n` 的数量。仓库里 CRLF 文件（`RimeWithWeasel*`）与 LF 文件（`WeaselServer/Cloud/*`、`test/*`、`docs/**`）**并存**，不能一刀切。
3. **`Copy-Item` 还原保留旧 mtime ⇒ MSBuild 不重编译 ⇒ 变异验证假红**（第 2 次踩）：还原后必须 `(Get-Item $f).LastWriteTime = Get-Date`。
4. **PowerShell 函数返回值吞掉 `Write-Output`**：`$x = RunTests 'tag'` 会把函数里所有 `Write-Output` 一起捕获进 `$x` ⇒ 屏幕上什么都看不到、判定表达式也失真（`$x -notmatch` 对数组返回的是**不匹配的元素**，非布尔）。修法：用 `$script:` 变量收集日志，或让函数只 `return` 字符串、打印放到调用侧。
5. **变异锚点命中错误位置**：用 `IndexOf` 找 `status_.last_outcome = SyncOutcome::kSkipped;` 会命中**第 193 行**（配额跳过路径）而不是 ADR 0062 的**第 346 行** ⇒ 变异打偏、误判「断言无牙」。修法：改用该分支**独有的**串（如 `status_.waiting_for_idle = true;`），或用 `LastIndexOf`。
6. **`cd` 之后写入被本机安全策略拦**（`write/delete targets cannot be verified safely after a command working directory change`）⇒ 一律用绝对路径 + `git -C <repo>`，不切目录。
7. **`git commit -F -` 不吃 stdin** ⇒ 提交信息先写 `tmp/` 临时文件再 `git commit -F <file>`。
8. **共享头的 `/utf-8` 差异**：`RimeWithWeasel.vcxproj` **没有** `/utf-8`，`WeaselServer` / `TestFluxingCloud` 有 ⇒ 给 `RimeWithWeasel` 用的头**只能放 ASCII**，中文文案只能放云侧 TU。

---

## 7. 下一批建议顺序

1. **用户先拍板「机会性同步」口径** ⇒ ADR 0062 转 Accepted（或按新口径改文案）。
2. **用户注销 / 重启 → 装 1.3.2（已出包，含本批代码）** → 跑用户在场 AC 4 条（AC-01/02/06/10）+ 输入延迟两组对照。
3. **提供第二台设备或第二个 Windows 账户** → 双机 AC 7 条（AC-03/04/05/08/09/13/14）+ restore 端到端。
4. 依据 2/3 的真机结果回写 ADR 0062 的「未闭环」段与 CHANGELOG。

### AS-4 · 出包 1.3.2（含本批 ADR 0062 代码）

- **版本真相源（四处同步，1.3.1 → 1.3.2）**：`version.txt`（5 字节无尾换行 ⇒ **等长字节替换**，`1.3.1` → `1.3.2`）；`env.bat`（`FLUXING_VERSION` / `PRODUCT_VERSION` / `FILE_VERSION` / `VERSION_PATCH`）；`weasel.props`（`VERSION_PATCH` / `PRODUCT_VERSION` / `FILE_VERSION`）。`WEASEL_BUILD` 保持 **0** ⇒ shim 名 `weaselx64.1.3.2.0.dll` 从未存在 ⇒ 无 L107 风险。`env.bat` / `weasel.props` 被 `.gitignore` ⇒ **不入提交**；只有 `version.txt` 受版本控制。
- **构建**：`build.bat weasel installer` ⇒ **BUILD_EXIT=0**；makensis **8 条 `warning 7010`**（ARM/ARM64/ARM64X dll ×6 + `data\*.gram` + `data\lua\*.txt`，既有可选产物告警，与 AQ 批逐条一致）。
- **产物**：`output/archives/fluxing-1.3.2-installer.exe` —— **42 952 698 B** · sha256 `B96F2534636C72962F86FEE31B707E151F61D5A541B2A2551CF4C31231588563` · **sha16 `B96F2534636C7296`**；内嵌 `WeaselServer.exe` **2 704 384 B** / filever `1.3.2.0` / prodver `1.3.2.0`。
- **判别性证据（双向）**：内嵌 `WeaselServer.exe` 含 ADR 0062 独有 ASCII 令牌 **`live_input_session` = True**；同场对照 1.3.1 包（内嵌 **2 700 800 B** / filever `1.3.1.0`）= **False** ⇒ 本批代码**确在 1.3.2 包内**，1.3.1 不含。
- **装机前置（不变）**：`WeaselServer.exe` 是 **PPL** 签名进程 ⇒ **必须注销 / 重启系统**再装，否则仍跑旧映像（磁盘哈希不能证明在跑的是新码）。当前装机版仍是 **1.2.1.0**（含云 ⇒ 每启动约 30 s 崩一次；AQ 批已修但未装）。