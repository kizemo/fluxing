# 批次 AW · 交接文档 —— 闸门空闲判据（ADR 0065）+ 账号窗槽位状态机（ADR 0066）

- **日期**：2026-11-20
- **批次代号**：AW
- **分支**：`feat/100-cloud-sync`（upstream `kizemo/feat/100-cloud-sync`）
- **起点 HEAD**：`da33fa7`（批次 AV：AT-2 方案 C「客户端两段式全量重推」+ 出包 1.3.4，已 push）
- **主题**：修两个真机缺陷 —— ①「**仅开启输入法**（无输入动作）就报『你正在输入，本轮已跳过用户词典同步』」；②「『账号与同步』窗口**第二次进入后，已登录窗口无法关闭**」
- **本批性质**：代码实现批次（**未改 `librime/`、未改 IPC 协议、未新增第三方依赖**）

---

## 1. 背景与决策

### 1.1 用户诉求（2026-11-20 原文）

> 已安装新版，未重启，但已经观察到 账号与同步，点击进入，已登录状态满足要求。但在第二次进入后，已登录窗口无法关闭。且点击同步，提示在输入时不能同步。而当时并无输入动作，可能仅仅是输入法开启了。**希望开启输入法，不要影响同步。** 请检查、修复。**需要重启后进行的测试内容，请为我列出来。**

⇒ 口径已明确：**「开启输入法」本身不得阻塞云同步**。按用户既有偏好（明显 bug 直接按推荐方案修），本批直接落码，不发问卷。

### 1.2 缺陷 A 根因（已坐实 · 源码级）

ADR 0062 的闸门判据把「**有活跃会话**」当「**用户在打字**」：

- `include/FluxingMaintenanceGate.h`（批次 AS 落码）：
  `MaintenanceGateAllows(int live_sessions) { return live_sessions <= 0; }`
- TSF 侧 `ActivateEx` → `StartSession`：**激活即建会话**（与按键无关）；`FocusOut` 只清 `m_active_session`，`EndSession` 仅 `Deactivate` 时触发。
- 本机唯一加载 `weasel.dll` 的进程是 `YXCalendar`（pid 180284）⇒ `live_sessions` **恒 ≥ 1** ⇒ 闸门**恒关闭**。
- 消费侧 `WeaselServer/Cloud/CloudSyncService.cpp` 在 `LoadSidecar` **之前**查闸门 ⇒ 闸门关闭即 `skipped=true`、**零 I/O / 零配额**。
- `WeaselServer/Cloud/CloudSyncEngine.cpp:353-357` 走 `waiting_for_idle` + **5 min 短间隔重试** ⇒ 用户看到「你正在输入，本轮已跳过用户词典同步，稍后会自动重试。」

⇒ **「输入法开着」与「用户正在打字」被混为一谈**，这就是缺陷 A 的全部根因。

### 1.3 缺陷 A 修法（ADR 0065 · 修订 ADR 0062 决策 1）

判据从「无活跃输入会话才放行」改为「**最近有输入**（空闲宽限 **10 s**）**或未上屏组合串**（宽限 **60 s**）才阻塞」：

| 场景 | ADR 0062 旧判据 | ADR 0065 新判据 |
| --- | --- | --- |
| 输入法已激活、用户从未打字 | **阻塞**（错） | **放行** |
| 5 s 前刚打过字 | 阻塞 | 阻塞（正确） |
| 15 s 前打过字 | 阻塞 | 放行（已空闲 > 10 s） |
| 15 s 前打过字 + 有未上屏组合串 | 阻塞 | 阻塞（组合串走 60 s 宽限） |
| 55 s 前打过字 + 有组合串 | 阻塞 | 阻塞 |
| 65 s 前打过字 + 有组合串 | 阻塞 | 放行 |

- **不放宽到「忽略组合串」**：组合串会被 `finalize` 丢弃、代价高 ⇒ 单独给 60 s 宽限期。
- **必须配会话复活**：放宽闸门后维护**真的**会销毁会话 ⇒ 不复活就重引入 ADR 0062 要避免的「按键被吞」⇒ 复活只能走**按键路径惰性复活**（见 §2.2）。
- **不放宽到「放弃维护」**：维护（导出 / 备份 / 回写）仍必须做，只是准入判据更准。
- **用户可见文案不动**（ADR 0062 已于 2026-11-18 由用户定稿「保留当前文案」）；原因令牌沿用唯一 `live_input_session`。
- **零 IPC 变更** ⇒ 不触发「IPC 四边同步」红线。

### 1.4 缺陷 B 根因（部分坐实 · 防御性全覆盖修）

账号页是**裸 Win32 modeless 顶层窗**，进程级 `HWND g_windows[Role::kCount]`（`WeaselServer/Cloud/CloudAccountDialog.cpp`）。已排除的假设：modal 阻塞、关闭按钮被隐藏、菜单 toggle、`EnableWindow(FALSE)` 致残。**未 100% 坐实**的分支：① 复用半死窗；② 新建即坏；③ UI 线程在 `WM_DESTROY` 阻塞于 `impl_->mutex`。⇒ 本批用**防御性全覆盖 + 诊断日志**修，不阻塞落码。

已坐实的两个真缺陷：

1. `WM_DESTROY`（旧 `:1341-1347`）**先** `UnregisterWindow`（取 `impl_->mutex`）**后**清槽位 ⇒ 一旦 Unregister 阻塞，槽位残留**半死 HWND** ⇒ 第二次进入菜单复用它 ⇒ 关闭按钮点不动。
2. 关闭按钮在子控件 `WM_LBUTTONUP` **尚未返回**时，由父窗 `WM_COMMAND` 同步 `DestroyWindow(parent)` ⇒ 把**正在处理消息的那个按钮控件**一起销毁 ⇒ 关闭按钮失灵。

### 1.5 缺陷 B 修法（ADR 0066 · 账号窗槽位状态机）

- 抽**纯函数头** `WeaselServer/Cloud/CloudAccountWindowState.h`（纯 ASCII、无 `windows.h`）：
  `SlotState{kEmpty,kStale,kLive}` + `ClassifySlot(has_handle,is_window,responsive)` + `SlotReusable` + `MustClearSlotBeforeUnregister` + `kSlotProbeTimeoutMs = 200`。
  ⇒ 生产与单测**共用同一份判据**（constitution R7）。
- 槽位表**独立一把锁** `g_windows_mutex`（**不复用** `impl_->mutex`——UnregisterWindow 会取它，同锁即自锁）；纪律：**任何 Win32 调用都在不持锁时进行**（`IsWindow` / `SendMessageTimeout` / `PostMessage` / `DestroyWindow`）。
- `WM_DESTROY`：**先清槽位（幂等 `SlotClearIfMatches`）→ 后 `UnregisterWindow`**；`WM_NCDESTROY` 双保险兜底。
- 复用前**探活**：`SendMessageTimeoutW(WM_NULL, SMTO_ABORTIFHUNG, kSlotProbeTimeoutMs)` ⇒ 半死窗（`IsWindow` 仍为 TRUE 但不响应）判为 `kStale` ⇒ 丢弃旧句柄并重建。
- 关闭路径**统一**走 `PostMessage(WM_CLOSE)`（唯一销毁入口 = `WM_CLOSE` → `DestroyWindow`）。
- 诊断日志前缀 `[FluxingAccountDlg]`（`OutputDebugStringW`，ASCII）。

---

## 2. 落码文件级表

| 文件 | 归属 | 改动要点 |
| --- | --- | --- |
| `include/FluxingMaintenanceGate.h` | 缺陷 A | 新增 `kMaintenanceIdleGraceMs = 10*1000`、`kMaintenanceCompositionGraceMs = 60*1000`、`MaintenanceGateIdleMs(now,last)`、`MaintenanceGateAllowsWithIdle(live,has_history,idle_ms,composing)`；**旧三符号（`MaintenanceGateAllows` / `MaintenanceGateReasonLiveSession` / `kMaintenanceSkipRetryIntervalMs`）一字未改** ⇒ 既有单测不破。★ 本头**保持纯 ASCII**（该编译单元无 `/utf-8`）。 |
| `include/RimeWithWeasel.h` | 缺陷 A | 新增 `std::atomic<int64_t> m_last_input_tick{0}`、`std::atomic<int> m_composing_session_count{0}`、`std::atomic<bool> m_maintenance_active{false}`、`m_session_composing` / `m_session_ascii_modes` 镜像表；`to_session_id` 由 `operator[]` 改 **`find`**（不再插入 `session_id==0` 假条目）；新增 `_RefreshComposingSessionCount` / `_SetSessionComposing` / `_SyncSessionComposing` / `_TryResurrectSession` 声明。 |
| `RimeWithWeasel/RimeWithWeasel.cpp` | 缺陷 A | `CanEnterMaintenance` 改用共享判据；`Finalize/StartMaintenance/EndMaintenance` 清组合串表并置 `m_maintenance_active`；`AddSession` 维护期不建会话；`RemoveSession` **先 erase + 重算计数再早退**（修会话回收漏洞）；`ProcessKeyEvent` 记 `m_last_input_tick`，孤儿 `session_id==0` 时 `_TryResurrectSession`；`CommitComposition/ClearComposition` 清组合串标记；`TryLazyRecovery` 维护期早退；复活用 `TryAcquire(DefaultLockName(),0)` 探测回合进行中 + `select_schema` + 恢复 `ascii_mode`。 |
| `WeaselServer/Cloud/CloudAccountDialog.cpp` | 缺陷 B | +100 行：槽位表独立 mutex、`SlotGet/SlotSet/SlotClearIfMatches/ProbeResponsive/DiscardStaleWindow`、`WM_DESTROY` 先清槽位后 Unregister、`WM_NCDESTROY` 兜底、复用前 `WM_NULL` 探活、关闭统一 `PostMessage(WM_CLOSE)`、`[FluxingAccountDlg]` 诊断日志。 |
| `WeaselServer/Cloud/CloudAccountWindowState.h` | 缺陷 B（**新建**） | 纯 ASCII 纯函数头（槽位状态机），生产与单测共用。 |
| `test/TestFluxingCloud/TestFluxingCloud.cpp` | 门禁 | 新增 include + `TestMaintenanceGateIdleCriteria()` + `TestAccountWindowSlotState()`，已在 `main()` 注册；既有 `TestCloudMaintenanceGate()` 结构未动。 |
| `docs/adr/0065-cloud-sync-gate-idle-criteria-and-session-revival.md` | 文档（**新建**） | 修订 ADR 0062 决策 1；含事实 1–5 / 决策 1–7 / 落码状态 / 残余风险 4 条 / 已驳回备选 4 条。 |
| `docs/adr/0066-account-window-slot-state-machine.md` | 文档（**新建**） | 账号窗槽位状态机；含事实 1–5 / 决策 1–6 / 残余风险 2 条 / 已驳回备选 3 条。 |
| `docs/adr/README.md` | 文档 | 新增 0065 / 0066 两行；0062 行状态改为 `Accepted（决策 1 由 0065 修订）`。 |
| `docs/release/restart-checklist-1.3.5.md` | 交付物（**新建**） | **用户要的「重启后测试清单」正文**：0 自检 / A 同步判据 / B 账号窗 / 批次 AV 换钥重推 / C 双机 7 条 / D 性能 / E 反馈格式 / F「不是缺陷」的已知现象。 |
| `version.txt` / `env.bat` / `weasel.props` / `CHANGELOG.md` | 版本 | 四处同步 **1.3.4 → 1.3.5**（`WEASEL_BUILD=0` 保持 ⇒ 无 L107 风险）。 |

**并行写者归属**：闸门三文件（`FluxingMaintenanceGate.h` / `RimeWithWeasel.h` / `RimeWithWeasel.cpp`）↔ 账号窗两文件（`CloudAccountDialog.cpp` / `CloudAccountWindowState.h`），互不重叠；`test/TestFluxingCloud/TestFluxingCloud.cpp` 由主会话独占。

---

## 3. 门禁

| 门禁 | 结果 |
| --- | --- |
| 云单测 | **`PASS=552 FAIL=0` / `VERDICT: ALL PASS`**（基线 526 ⇒ **+26**） |
| ├ 新增 `TestMaintenanceGateIdleCriteria` | 12 条断言全绿（两个宽限期常量、三个 `MaintenanceGateIdleMs` 场景含**时钟回拨**、五个准入象限） |
| └ 新增 `TestAccountWindowSlotState` | 10 条断言全绿（四象限分类 × 复用性 × 清槽位时机） |
| **变异有牙** | **PASS** —— 注入 `kMaintenanceIdleGraceMs=0` + `kMaintenanceCompositionGraceMs=0` + `SlotReusable` 恒真 ⇒ `PASS=544 FAIL=8`（两条断言恰好抓住三处缺陷：常量值、组合串 60 s、5 s 关闭、15/55 s 组合串关闭、陈旧/空槽位不可复用）；还原 ⇒ 回到 `PASS=552 FAIL=0` |
| `build.bat weasel` | **0 警告 0 错误** |
| 定点 clang-format | 5 个改动文件按 `git diff -U0` 解析的变更行区间格式化；**复核重算区间后 `--dry-run --Werror` 全部 `exit=0`**（`FORMAT_BAD=0`）；新文件整体格式化 |
| L0 静态门禁 | 15 项 **14 过 1 失败** ⇒ 唯一失败项是 `version-consistency`（`CHANGELOG.md` 顶部仍是 1.3.4，**本批收尾时补写 1.3.5 条目后复跑**） |
| IPC 四边同步 | **不适用** —— 本批零 IPC 消息变更、零协议变更 |
| `librime/` submodule | **未改**（工作区 `m librime` 是**既有脏项**，与本批无关，**不得提交**） |

> ⚠️ **变异验证的坑（已踩）**：`Copy-Item` 还原会**保留源文件 mtime** ⇒ MSBuild 认为头文件没变 ⇒ **不重编译** ⇒ 还原后复跑仍是变异版的 FAIL（假红）。**还原后必须 `touch` 头文件再重编**。本批已按此修正并复跑确认。

---

## 4. 出包 1.3.5

- **产物**：`output/archives/fluxing-1.3.5-installer.exe` —— **42 973 772 B · sha16 `471C7C9B255D1E33`**（sha256 `471C7C9B255D1E3360E5B62280F7BEB740A2F02154B29B634AD25611D035D809`）
- **内嵌 `WeaselServer.exe`**：**2 766 336 B / filever `1.3.5.0`**（1.3.4 包内为 2 755 072 B / `1.3.4.0` ⇒ 判别通过）；与 `output\Win32\WeaselServer.exe` sha16 逐字一致（`741C13F78170D856`）⇒ 包内容即本次构建产物
- **包内令牌差集（判别证据）**：

  | 令牌 | 编码 | 1.3.5 包 | 1.3.4 包 |
  | --- | --- | --- | --- |
  | `resurrect session`（缺陷 A 新代码） | ASCII | **True** | False |
  | `FluxingAccountDlg`（缺陷 B 新代码） | **UTF-16LE** | **True** | False |
  | `live_input_session`（ADR 0062 旧令牌） | ASCII | True | True |

- **构建**：`build.bat weasel installer` ⇒ **0 警告 0 错误**；makensis 8 条既有 `warning 7010`（ARM/ARM64 产物 + `data\*.gram` + `data\lua\*.txt`）
- **版本真相源**：`version.txt`（5 字节无尾换行）/ `env.bat`（4 个键位）/ `weasel.props` / `CHANGELOG.md` 四处同步 1.3.4 → 1.3.5；`WEASEL_BUILD` 保持 0 ⇒ 无 L107 风险；**已 touch 4 个 `.rc`**
- ⚠️ **判别要看 `WeaselServer.exe`，不是 `weasel.dll`**：`RimeWithWeasel` 是**静态库**（`RimeWithWeasel.vcxproj` = `StaticLibrary`），只链进 `WeaselServer.exe`；TSF 前端 `weasel.dll` 与 1.3.4 包内**仅差 17 字节**（只有版本资源）⇒ **属正常**，不是「代码没进包」。宽字符串令牌必须按 **UTF-16LE** 搜（按 ASCII 搜会**假红**）。

---

## 5. 已验证 / 推断 / 未验证

### 5.1 已验证（本次实测，证据可复跑）

- 云单测 `PASS=552 FAIL=0`；变异注入 ⇒ `FAIL=8`；还原 + touch ⇒ 回到 552-0。
- `build.bat weasel` **0 警告 0 错误**（生产代码含 `RimeWithWeasel.cpp` 与 `CloudAccountDialog.cpp` 真编译通过）。
- 定点 clang-format 复核 `exit=0`。
- L0 静态门禁 15 项中 14 项 PASS（唯一 FAIL = CHANGELOG 版本号，补写后复跑）。
- 缺陷 A 根因链（ADR 0062 判据 → 激活即建会话 → `live_sessions` 恒 ≥1 → 闸门恒关 → `LoadSidecar` 前查 ⇒ 整轮跳过）。
- 缺陷 B 已坐实的两个真缺陷（`WM_DESTROY` 清槽位次序 + 关闭按钮自毁）。

### 5.2 推断（源码级推导，尚未经真机证伪/证实）

- 缺陷 A 的真机表现推断：仅开输入法（不打字）⇒ `has_input_history=false` ⇒ 闸门放行 ⇒ 本轮正常同步。
- 缺陷 A 的复活路径推断：维护 finalize 后客户端 `ipc_id` 成孤儿 ⇒ 首次按键时 `_TryResurrectSession` 建新会话 ⇒ 不再出现 ADR 0062 的「按键被吞」。
- 缺陷 B 推断：复用前探活 + 槽位清理次序修正 ⇒ 第二次进入不再复用半死窗；关闭按钮改走 `WM_CLOSE` ⇒ 不再自毁。
- 组合串 60 s 宽限的必要性（`finalize` 会丢弃未上屏组合串）—— 取自 ADR 0062 的既有事实，非本批新证。

### 5.3 未验证（必须重启后由用户在场验）

- **未装机**：1.3.5 产物**从未在真机跑过**；`WeaselServer.exe` 是 **PPL** ⇒ 装机前**必须注销 / 重启**（磁盘哈希不能证明在跑的是新码）。
- **用户在场 AC**：清单见 `docs/release/restart-checklist-1.3.5.md`（含缺陷 A / B 的判据与**反向判据**、批次 AV 换钥全量重推、双机 AC 7 条、restore 端到端、输入延迟两组对照）。
- **双机 AC 7 条**（`CloudCredentialStore` 为 Windows 用户级全局单例 ⇒ **两套 Profile 目录不够**）。
- **restore 端到端**（需第二台设备）。
- **输入延迟两组对照**。
- **闸门通过率**（ADR 0062 遗留：真实宿主上「会话说话但用户没打字」的判别准确率）。
- **孤儿 `ipc_id` 真机表现**（ADR 0062 遗留）。
- **产品口径待用户拍板**：换钥后云端旧行仍留、且**永不可解**（服务端从不见 `enc_key`）⇒ 是否在账号页给一句提示。**本批不代决**。

---

## 6. 本轮工具坑（勿重试）

1. **`Copy-Item` 还原保留 mtime ⇒ MSBuild 不重编译 ⇒ 变异验证假红**（还原后必须 touch）。
2. **`msbuild.exe` 不在本机 PATH**（`build.bat` 内部有 VS 环境）⇒ 单测工程直编要用
   `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`，且单测工程**必须**显式 `/p:SolutionDir=<repo>\`（否则 `$(SolutionDir)` 未定义 ⇒ C1083）。
3. **`build.bat weasel` 不构建 `TestFluxingCloud.exe`**（该 exe 的 mtime 会停在旧值 ⇒ 误以为「单测已含新用例」）。
4. **`Set-Location` 后写文件被本地硬安全策略拦**（写/删目标无法校验）⇒ 一律用**绝对路径**。
5. **`clang-format --lines=` 在 `-i` 后区间会偏移** ⇒ 复核必须**重算**区间（`git diff -U0` 解析 hunk 头）。
6. **测试工程有 516 处历史格式漂移** ⇒ 只能按 `--lines` 定点格式化，不能整文件格式化。
7. **`git diff --numstat` 前后对比**用来确认「格式化只动了本次改动行区间」（本批：行数变化仅 ±3，且文件集合不变）。
8. **`grep` 的 `files_with_matches` 模式不返回上下文** ⇒ 要上下文必须 `output_mode=content`。
9. **子会话派工要写清「唯一允许的写操作」**；本批两个 worker 只改自己的文件域，**被明确禁止构建/提交**（构建由主会话统一跑，避免并发编译产物打架）。

---

## 7. 下一批建议

1. **装机 1.3.5 + 重启** ⇒ 按 `docs/release/restart-checklist-1.3.5.md` 跑用户在场 AC（缺陷 A / B 判据优先）。
2. **把「开启输入法 ≠ 正在输入」写进回归清单**（新增 AC 条目，防止后续再被改回旧判据）。
3. **管道异常断开的孤儿会话回收**（ADR 0065 残余风险）：`weasel.dll` 宿主崩溃后 `ipc_id` 残留 ⇒ 本批放宽闸门后它不再阻塞同步，但仍应回收。
4. **闸门判据的准确率实测**：统计「会话存在但未打字」在真实宿主的占比，必要时把 10 s 宽限调到更贴合手速的值。
5. **缺陷 B 第三分支取证**：若 1.3.5 装机后账号窗仍关不掉，用 `[FluxingAccountDlg]` 诊断日志区分「复用半死窗 / 新建即坏 / UI 线程卡在 `WM_DESTROY`」三态。
6. **产品口径**：等用户拍板「换钥后旧行不可解是否在账号页提示」，再决定是否补 UI 文案（会影响用户可见文案，需走 ADR + 用户确认）。
