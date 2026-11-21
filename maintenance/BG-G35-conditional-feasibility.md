# BG · G-3/G-5 维护闸门条件可行性配方（可执行）

> 批次：BG（2026-11-21）· 状态：**可行性 = CONDITIONAL YES**；**执行判定 = `BLOCKED_HARNESS_PRECONDITION`**（两条硬前置不可得 ⇒ 非缺陷、非 PASS、也非 FAIL）
> 本文件是**配方**，不是叙述文：每条命令都能照抄执行，每条判据都能在产物里逐字对到。
> 配套档案：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bg-verify-g35-delegated.md`
> 权威取证报告（已入库）：`F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\harness-report.md`
> ⚠️ **BG 自身的实验从未真跑** ⇒ 本配方首次照抄执行时，产出的是**第一份真机结果**，不得引用任何「BG 已 PASS」的说法。

---

## 0. 结论口径（先定死，避免执行时被带偏）

| 口径 | 值 |
|---|---|
| BG 自评可行性 | **CONDITIONAL YES** |
| BG 自评执行判定 | **`BLOCKED_HARNESS_PRECONDITION`**（环境前置不可得 ⇒ **非缺陷、非 PASS、也非 FAIL**） |
| 判据真相源 | `F:\soft\00selfmade\rime_claude\include\FluxingMaintenanceGate.h:67-84`（常量同文件 **L49** / **L50**） |
| **唯一可用判据** | **维护导出文件 mtime 是否前进**（日志文本命中 / 状态文案 / 版本号**一律不作数**） |
| 相位编排 | **C → G3 → G5** |

闸门判据原文：

```
allows = live_sessions <= 0
      || !has_input_history
      || (composing_sessions > 0 ? idle_ms > 60000 : idle_ms > 10000)
```

⇒ `composing == 0` 时只看 **idle > 10 s**；`composing > 0` 时看 **idle > 60 s**。

---

## 1. 前置条件核对表

### 1.1 P 类 —— BG 登记的**两条硬前置**（阻塞的是「等真实态出现」这条原始路线）

| # | 前置 | 现场操作步骤 | 本机（2026-11-21）状态 |
|---|---|---|---|
| **P-a** | 旧端须用**未含 R3 守卫的 1.3.5 构建**（**不是** 1.3.4 装机包） | ① 在**第二台设备**上部署 1.3.5 的**构建产物**（**不得**用 1.3.4 装机包）；② 核 `WeaselServer.exe` 的 `FileVersion` = `1.3.5.0`；③ 确认该构建**不含 R3 守卫**（守卫的所指尚未坐实 ⇒ 现场判据只能取「升级 / 还原路径未被守卫拦住」，判据原文出处待补） | ❌ **不可得**（第二台设备离线；服务端 1.3.4.0→1.3.8.0 升级链从未真实发生） |
| **P-b** | 须**手工老化** `fluxing-rime-backup\profile.tar.gz` 的 mtime | ① 定位该文件（登记路径形态 `…\fluxing-rime-backup\profile.tar.gz`）；② 记录原 mtime；③ 把它改成一个**更早**的时间（见下方命令）；④ 核对改后 mtime | ❌ **不可得**（`D:\Program Files\fluxing` 下无任何 `*.tar.gz` / `backup` 命中 ⇒ 只能到真实双机环境现场定位） |

P-b 的老化操作（**只能在文件真实存在的机器上执行**）：

```powershell
$p = '<现场定位到的 fluxing-rime-backup\profile.tar.gz 绝对路径>'
$orig = (Get-Item -LiteralPath $p).LastWriteTime
"ORIG_MTIME=$orig"
(Get-Item -LiteralPath $p).LastWriteTime = (Get-Date).AddDays(-30)   # 老化
"NEW_MTIME=$((Get-Item -LiteralPath $p).LastWriteTime)"
```

> ⚠️ 两条硬前置的**原文出处**：`docs\archive\handoffs\handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md` §2 与 `CLAUDE.md` 的 BG 指针行（两者逐字一致）。
> ⚠️ 「R3 守卫」的具体所指**未坐实**：本仓库现存文档中「守卫」只出现在 `docs\release\restart-checklist-1.3.7.md:62`（`install.nsi` 三段升级 + `IfErrors`/`Rename` 守卫）⇒ 最可能是升级链的第三段守卫，但**未证实**，登记为待定位。
> ℹ️ **harness 路线与 P 类的关系（推断）**：§2–§5 的造态路线是「主动造出等价的态」，**可能**绕开 P-a / P-b；但未被证实 ⇒ 无论走哪条路线，**结论口径不变**（CONDITIONAL YES / `BLOCKED_HARNESS_PRECONDITION`），直到 §7 跑出真机结果。

### 1.2 R 类 —— harness 路线的**运行前置**（本机可满足，逐条可自检）

| # | 前置 | 自检命令 / 判据 | 不满足时的后果 |
|---|---|---|---|
| **R-1** | harness 可执行文件已构建且**比源码新** | `Test-Path 'F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe'`；且 `exe.LastWriteTime -ge cpp.LastWriteTime` | 脚本 `Abort`：`TOOL_MISSING`（退出码 5） |
| **R-2** | 服务端**在跑**且唯一 | `@(Get-Process WeaselServer -EA SilentlyContinue).Count` = 1；`FileVersion` 记录在案 | 脚本 `Abort`：`SERVER_NOT_RUNNING`（5） |
| **R-3** | **账号已登录** + 云端端点可达 | 由**相位 C 正控制**兜底判定（C 不前进 ⇒ 整轮 ABORT）；端点预检原文见 `reports\fluxing-bg-prep\{bg-cred-check.txt,bg-endpoint-reach.txt}` | 脚本 `Abort`：`CONTROL_ROUND_DID_NOT_EXPORT`（5） |
| **R-4** | 维护导出文件存在 | 脚本自找 `<RimeUserDir>\sync\**\*.userdb.txt`（取最新）；手工核对：`Get-Item 'D:\Program Files\fluxing\user1\fluxing\sync\<account>\rime_ice.userdb.txt' \| Select-Object LastWriteTime` | 脚本 `Abort`：`EXPORT_FILE_NOT_FOUND`（5） |
| **R-5** | **用户在场**且**全程不在别的窗口打字** | 纪律项：`m_last_input_tick` 是**全局**量，别处打字会把 idle 拉回 0 ⇒ G-5 假红 | G-5 假红（脚本无法自愈） |
| **R-6** | 诊断日志存在（**可选**，只影响切片留档） | `Test-Path "$env:TEMP\fluxing-profile-notify.log"` | 仅少一份 `log-slice.txt`，不致命 |
| **R-7** | 脚本语法完好 + 纯 ASCII | `[Parser]::ParseFile('F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1',[ref]$null,[ref]$err)` ⇒ `PARSE_ERRORS=0` | 直接报错退出 |

### 1.3 一次跑完的预检（照抄这一段）

```powershell
$ErrorActionPreference = 'Stop'
$root = 'F:\soft\00selfmade\rime_claude'
"TOOL_EXISTS=$(Test-Path "$root\release\TestOrphanReclaimE2E.exe")"
"SERVER_COUNT=$(@(Get-Process WeaselServer -EA SilentlyContinue).Count)"
$err = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
  "$root\scripts\verify\orphan-gate-g35-export-mtime.ps1", [ref]$null, [ref]$err)
"PARSE_ERRORS=$($err.Count)"
"DIAG_LOG=$(Test-Path "$env:TEMP\fluxing-profile-notify.log")"
Get-ChildItem 'D:\Program Files\fluxing\user1\fluxing\sync' -Recurse -Filter '*.userdb.txt' -EA SilentlyContinue |
  Select-Object FullName, Length, LastWriteTime
```

---

## 2. 构建（harness 单工程）

**单工程构建必须显式传 `/p:SolutionDir=`**，否则依赖工程 `WeaselUI` 会报
`WeaselUI\stdxf.h(15,10): error C1083: 无法打开包括文件: "wtl/atlapp.h"`（`MSBUILD_EXIT=1`）——
根因：`$(SolutionDir)` 只在构建 sln 时由解决方案注入。仓库自带同款配方（`build.bat:339-342`）。

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.vcxproj' `
  /p:Configuration=Release /p:Platform=Win32 "/p:SolutionDir=F:\soft\00selfmade\rime_claude\" `
  /nologo /v:minimal /m:1 /fl "/flp:logfile=F:\soft\00selfmade\rime_claude\tmp\bg-g35-msbuild.log;verbosity=normal"
"MSBUILD_EXIT=$LASTEXITCODE"     # 期望 0
```

产物落点 = `F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe`
（`OutDir = $(SolutionDir)\$(Configuration)\` ⇒ 仓库根 `release\`）。

**mtime 核验（exe 必须 ≥ 源码，否则就是旧产物）**：

```powershell
$cpp = Get-Item 'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.cpp'
$exe = Get-Item 'F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe'
"EXE_NEWER=$($exe.LastWriteTime -ge $cpp.LastWriteTime)  cpp=$($cpp.LastWriteTime.ToString('o'))  exe=$($exe.LastWriteTime.ToString('o'))"
```

`BOOST_ROOT` 已由 `weasel.props` 的 `UserMacros` 提供（`F:\b183`），无需额外设置。

### 2.1 harness 独立自检（不联云、不开窗，先证明造态链本身活着）

```powershell
F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe `
  --hold-commit F:\soft\00selfmade\rime_claude\tmp\holdcommit.marker idle 6
"EXIT=$LASTEXITCODE"
```

期望输出（形状固定，数值随机器变）：

```
HOLDCOMMIT_SESSION ipc_id=<n> pid=<n> phase=idle
COMPOSING_AFTER_TYPING=1 ASCII_MODE=0
COMMIT_SENT=1
COMPOSING_AFTER_COMMIT=0
HOLDCOMMIT_STARTED ipc_id=<n> pid=<n> last_input_tick_ms=<n>
HOLDCOMMIT_DONE idle_ms=<n>            # 6 s 窗口内 idle 增长到 ~6.0 s
```

判据：`COMPOSING_AFTER_TYPING=1`（真的有未上屏组合串）+ `COMMIT_SENT=1`（IPC 提交被接受、事务未挂死）+ `COMPOSING_AFTER_COMMIT=0`（提交后由**服务端响应体**自证组合串已清）+ `EXIT=0`。
**任何一项不满足 ⇒ 造态链本身坏了 ⇒ 停，不要往下跑 G-3/G-5。**

---

## 3. 造态四步（具体调用与自证）

由 `TestOrphanReclaimE2E.exe --hold-commit <marker> <active|idle> [seconds]` 自动完成四步：

| 步 | 动作 | 调用/常量 | 自证字段 | 依据 |
|---|---|---|---|---|
| 1 | `START_SESSION` | IPC | `ipc_id`（≠0） | `WeaselIPCServer/WeaselServerImpl.cpp:265-276` |
| 2 | 打 13 个字母 `fluxingorphan` | 常量串 `"fluxingorphan"` | `COMPOSING_AFTER_TYPING=1` | `RimeWithWeasel.cpp:2055-2063` |
| 3 | 发 `WEASEL_IPC_COMMIT_COMPOSITION` | **0x800B** | `COMMIT_SENT=1` | `include/WeaselIPC.h:30`；`RimeWithWeasel.cpp:762-773` |
| 4 | 发刷新键 `ibus::Left` | **0xFF51** | `COMPOSING_AFTER_COMMIT=0` | `RimeWithWeasel.cpp:740` `_SyncSessionComposing` |

- **为什么提交用 IPC 命令**：键语义随 schema / 输入法模式变化，IPC 命令确定性与 schema 无关。
  `WEASEL_IPC_COMMIT_COMPOSITION` 枚举自 `WM_APP+1` 起算 ⇒ `0x800B`；服务端 `WeaselServerImpl.cpp:382-388` 取 **lParam 作 session id**；
  `HandlePipeMessage` 末尾无条件 `resp(result)`（`WeaselServerImpl.cpp:544`）⇒ 无 body 也会回 4 字节 result，不会挂死。
- **为什么刷新键选 `Left`**：既要刷新「最近一次输入」（`m_last_input_tick` 在每个 `ProcessKeyEvent` 无条件写入，`RimeWithWeasel.cpp:713-717`），又不能产生组合串
  （`librime/src/rime/gear/editor.cc:46-66`：`:47-48` release 被拒；`:51-57` 仅组合态查 key binding；`:58-62` char handler 要求 `0x20 < ch < 0x7f`，`0xFF51` 超界）。
  `Shift` 会切 ascii 模式、`Escape` 会清组合串（假绿）、数字键中文模式下可能被 translator 收下 ⇒ 三者禁用。
- **运行时兜底**：刷新键每发一次都读一次 `status.composing=`，**一旦出现 1 立即以退出码 3 失败**（`a composition reappeared in the steady state`）。
- **产物文件**（marker 基路径 = `<证据目录>\hold.marker`）：
  | 文件 | 写入时机 | 内容 |
  |---|---|---|
  | `<marker>` | 一次 | `ipc_id` / `pid` / `phase` / `composing_after_commit` / `last_input_tick_ms` |
  | `<marker>.state` | 每次刷新重写 | `last_input_tick_ms` / `composing` |
  | `<marker>.pause` | 相位 G5 触发前由脚本创建 | 存在时 `active` 相位**停止刷新** ⇒ idle 增长 ⇒ **一个进程即可承载 G-3 / G-5 两个相位** |
- **全程不发 END_SESSION**：会话必须**驻留**（`live_sessions > 0` 才有闸门可言）。

### 3.1 idle 的保守口径

- harness 记 `GetTickCount64()` 的**发送时刻** ⇒ 我方测得的 idle ≤ 真实 idle ⇒ **测得值是下界**。
- **G-3 判「idle < 10 s」**（下界 < 10 s ⇒ 真实更小 ⇒ 天然保守）；**G-5 判「idle ≥ 12 s」**（留 2 s 余量覆盖 IPC 往返与调度抖动）。
- `active` 相位刷新周期 **1500 ms** ⇒ 稳态 idle ≈ 1.5 s，距 10 s 门槛 ~8.5 s 余量。
- 脚本用 `[Environment]::TickCount64`（与服务端 / harness 同一系统时钟）。

---

## 4. 相位编排时序

脚本一次跑完三相位，**顺序固定 C → G3 → G5**：

| 时刻 | 动作 | 脚本日志行（判据锚点） | 时间预算 |
|---|---|---|---|
| T0 | 打开账号窗（`WM_COMMAND 40024` 给服务端顶层窗） | `ACCOUNT_WINDOW_PREEXISTING=0/1` → `ACCOUNT_HWND=0x... visible=...` → `SYNC_NOW_HWND=0x...` → `ACCOUNT_STATUS(id 1006)=` | ~2 s |
| T0+ | **等引擎自己的启动轮次过去** | `ENGINE_SETTLE_WAIT_SEC=40` | **40 s**（不排掉它，它会被误认成我们的轮次） |
| T1 | **相位 C**：触发同步（`WM_COMMAND 1016` + `BM_CLICK`），等导出 mtime 前进 | `ROUND_TRIGGERED_C=1` → `STATUS_AFTER_C=` → `CONTROL_EXPORT_ADVANCED_AFTER_SEC=<s>` → `OBSERVE_WINDOW_SEC=<15..60>` | ≤ `-RoundWaitSec`（默认 **150 s**） |
| T2 | 起 harness `--hold-commit <marker> active`，等 marker 出现，读合成态 | `HOLD_STARTED pid=... mode=active` → `SYNTHETIC_IPC_ID=` → `SYNTHETIC_PID=` → `ECHO_BEFORE=` → `SYNTHETIC_COMPOSING=0 (must be 0)` | marker 等待 ≤ 40 s |
| T3 | **相位 G3**：读 idle（必须 < 10 s）→ 触发 → 观察窗内导出**不得**前进 | `G3_IDLE_MS=... (must be < 10000)` → `ROUND_TRIGGERED_G3=1` → `G3_NO_EXPORT_WITHIN_SEC=<s>`（期望）或 `G3_EXPORT_ADVANCED_AFTER_SEC=<s>`（异常） | 观察窗 = `ceil(控制轮耗时)+15 s`，钳在 **15–60 s** |
| T4 | **相位 G5**：写 `.pause` → 等 idle ≥ 12 s → 静止观察 → 触发 → 等导出前进 | `HOLD_TYPING_PAUSED=1` → `G5_IDLE_MS=... (must be >= 12000)` → `QUIESCENT_WATCH_OK=1 (watch 1, 20 s)`（或 `SPONTANEOUS_EXPORT_ADVANCED=1`）→ `ROUND_TRIGGERED_G5=1` → `G5_EXPORT_ADVANCED_AFTER_SEC=<s>` | idle 等待 ≤ 90 s；静止观察 **20 s**（`-QuiesceSec`，最多试 2 轮）；导出等待 ≤ `-RoundWaitSec` |
| T5 | 收尾：杀 harness、删 `.pause`、切诊断日志 | `HOLD_STOPPED=1` → `LOG_SLICE_FILE=... bytes=<n>` | ≤ 15 s |

**为什么顺序固定 C → G3 → G5**：自发轮次**不可能**造成 G-3 假阴性（被闸门挡住的轮次不会导出），但**可能**造成 G-5 假阳性（自发轮次自己导出了）⇒ 触发 G-5 前必须做**静止性观察**；引擎不静止 ⇒ 判 INCONCLUSIVE。

---

## 5. 每个相位的判据与期望输出

| 相位 | 期望 | 判据（唯一） | 期望日志行 |
|---|---|---|---|
| **C**（control） | 本轮**不被跳过** | 导出文件 mtime **必须前进** | `CONTROL_EXPORT_ADVANCED_AFTER_SEC=<s>` + `CRITERION_C_EXPORT_ADVANCED=1` |
| **G3**（负向） | 闸门**保持关闭** | 导出文件 mtime **必须不前进** | `G3_NO_EXPORT_WITHIN_SEC=<s> (the gate stayed closed)` + `CRITERION_G3_EXPORT_NOT_ADVANCED=1` |
| **G5**（正向） | 闸门**放行** | 导出文件 mtime **必须前进** | `G5_EXPORT_ADVANCED_AFTER_SEC=<s>` + `CRITERION_G5_EXPORT_ADVANCED=1` + `VERDICT=GATE_G35_OK` + `EXIT=0` |

- **G3 与 G5 只差一个变量（用户是否还在打字）⇒ 这一对就是 G-3 / G-5 的证明。**
- **状态文案只进日志，不进判据**：`STATUS_AFTER_C/G3/G5=` 与 `STATUS_AFTER_*_SKIPPED_HINT=` 仅作辅助提示（脚本用 code unit `U+8DF3 U+8FC7` 匹配「跳过」两字，以保持脚本纯 ASCII）。
- **观察窗从 15 s 到 60 s 是脚本钳出来的**（`OBSERVE_WINDOW_SEC=`），不是随机值；它同时是 G-3 的「闸门必须继续关着」的观察时长。

### 5.1 三个相位各自的“为什么这么设计”

1. **单会话 + `.pause` 切换，而不是两个会话**：`m_last_input_tick` 是**全局**量（`RimeWithWeasel.cpp:715-717`），任何会话的任何键都会刷新它 ⇒ 若 G3 的 `active` 会话与 G5 的 `idle` 会话并存，那个 `active` 会话会不断刷新全局 tick，G5 的 idle 窗口永远不成立。（脚本统一用 `--hold-commit <marker> active`；harness 的 `idle` 相位保留给独立使用与自检。）
2. **G-5 前 20 s 静止观察，否则判 INCONCLUSIVE**：不静止 ⇒ G-5 的「mtime 前进」无法归因给本轮触发。
3. **文案只进日志、不进判据**：服务端**没有**闸门判定结果的可观测点（原因令牌 `live_input_session` 在 `CloudSyncService.cpp:878-884` 被丢弃；`kIdSyncStatus`(1017) 是静态字面量永不变；诊断日志无闸门记录）⇒ 只能靠 mtime 代理。

---

## 6. 跑法与退出码

### 6.1 命令

```powershell
# 预演（不碰服务端、不开窗、不起 harness、不同步）—— 只验环境，不构成任何 G-3/G-5 结论
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All -DryRun
"EXIT=$LASTEXITCODE"

# 正式三相位（★ 会真的开账号窗并点「立即同步」⇒ 需登录态 + 用户在场 + 全程不碰别的窗口打字）
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All
"EXIT=$LASTEXITCODE"

# 单相位
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode C
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G3
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G5

# 可调参数
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All `
  -RoundWaitSec 150 -IdleWaitSec 15 -QuiesceSec 20
```

参数含义：`-Mode C|G3|G5|All`（默认 `All`）、`-DryRun`（开关）、`-RoundWaitSec`（等导出前进的秒数，默认 **150**）、`-IdleWaitSec`（12 s 门槛之上的额外余量，默认 **15**）、`-QuiesceSec`（G-5 前的静止观察窗，默认 **20**）。

**`-DryRun` 的期望输出（逐行可对）**：

```
REPO_ROOT=F:\soft\00selfmade\rime_claude
TOOL=F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe
MODE=All PHASES=C,G3,G5
IDLE_GATE_MS=10000 IDLE_MARGIN_MS=12000 IDLE_REFRESH_MS=1500
USER_DIR=D:\Program Files\fluxing\user1\fluxing
EXPORT_FILE=<RimeUserDir>\sync\<account>\rime_ice.userdb.txt
EXPORT_SIZE=<n> EXPORT_MTIME_INITIAL=<时间>
TOOL_PRESENT=True
SERVER_PROCESSES=1
SERVER pid=<n> path=D:\Program Files\fluxing\weasel\WeaselServer.exe ver=1.3.8.0
DRYRUN=1 (no window, no harness, no sync round, nothing changed)
VERDICT=GATE_G35_DRYRUN
EXIT=0
```

### 6.2 退出码语义表

| 退出码 | 脚本日志 | 含义 | 判读 |
|---|---|---|---|
| **0** | `VERDICT=GATE_G35_OK` | 三相位判据全部满足 | ✅ 本次 G-3/G-5 闭环（**仅当 P 类前置也已满足时，才可升级为整体闭环**） |
| **4** | `FAIL=<原因>` + `VERDICT=GATE_G35_FAIL` | 判据不满足，或**状态没被造出来** | 判 FAIL / 排查（§7.2） |
| **5** | `ABORT=<原因>` + `VERDICT=GATE_G35_FAIL` | **环境不可用**（不是判据失败） | 判 **ABORT**（§7.3） |
| **6** | `INCONCLUSIVE=<原因>` + `VERDICT=GATE_G35_INCONCLUSIVE` | 结果**不可归因** | 判 **未判定**（§7.4） |

> ⚠️⚠️ **必读坑：脚本的 `Abort` 分支即使退出码 = 5，也会打印 `VERDICT=GATE_G35_FAIL`**（`Abort` 函数先写 `ABORT=` 再写 `VERDICT=GATE_G35_FAIL` 再写 `EXIT=<code>`）⇒ **一律以退出码为准，不要按 `VERDICT=` 串判读**。

---

## 7. 失败排查分支

### 7.1 分支一：**「闸门不放行」**（闸门该关的时候它放行了）

| 现象 | 日志 | 退出码 | 处置 |
|---|---|---|---|
| G-3 相位里导出文件**前进了** | `G3_EXPORT_ADVANCED_AFTER_SEC=<s>` + `INCONCLUSIVE=G3_DID_NOT_BLOCK` | **6** | 这不是「闸门有 bug」，而是**这一对证明不了任何东西**（若闸门本该关闭却放行，说明合成态没被闸门看到）。先查 §7.2 的合成态自证，再查 `m_last_input_tick` 是否被**别的窗口打字**污染（全局量） |

### 7.2 分支二：**「会话没被造出来」**（状态不成立 ⇒ 判据无意义）

| 现象 | 日志 | 退出码 | 处置 |
|---|---|---|---|
| harness 早退 | `FAIL=HOLD_EXITED_EARLY exit=<n>` | 4 | 看 `hold.stdout.txt` / `hold.stderr.txt`：`<n>=3` ⇒ **组合串状态不对**（打字后没 composing=1，或提交后仍有组合串）⇒ 换 schema/输入模式再试；`<n>=1` ⇒ 连接/协议错误 ⇒ 查服务端是否在跑、管道名是否被占 |
| marker 40 s 内没出现 | `FAIL=HOLD_MARKER_TIMEOUT (no ipc_id in the marker within 40 s)` | 4 | 同上（连接层） |
| 没有 state 文件 | `FAIL=HOLD_STATE_MISSING` | 4 | harness 没跑起来 ⇒ 看 `hold.stderr.txt` |
| 合成态组合串不为 0 | `FAIL=SYNTHETIC_COMPOSING_NOT_ZERO value=<v>` | 4 | 提交没生效 ⇒ 查 `WEASEL_IPC_COMMIT_COMPOSITION`=0x800B 是否被正确下发（`RimeWithWeasel.cpp:762-773`） |
| G-3 的 idle ≥ 10 s | `FAIL=G3_IDLE_NOT_BELOW_GATE` | 4 | 刷新键没在发 ⇒ 查 `active` 相位与 `.pause` 文件是否残留 |
| G-5 的 idle < 12 s | `FAIL=G5_IDLE_NOT_ABOVE_GATE` | 4 | `.pause` 没生效，或 90 s 内没到 12 s ⇒ 查 `.pause` 文件是否创建、是否被提前删掉 |

### 7.3 分支三：**「导出文件 mtime 没前进」** —— 必须先分清是哪一相位

| 相位 | mtime 没前进 | 含义 | 退出码 |
|---|---|---|---|
| **C（正控制）** | `ABORT=CONTROL_ROUND_DID_NOT_EXPORT` | **环境链路坏了**：触发没生效 / 未登录 / 导出路径不对 / 云端不可达 ⇒ **没有正控制，G-3/G-5 无意义** ⇒ 整轮 ABORT，先修环境 | **5** |
| **G3（负向）** | `G3_NO_EXPORT_WITHIN_SEC=<s> (the gate stayed closed)` | ✅ **这是期望结果**（闸门正确拦下） | 继续 |
| **G5（正向）** | `FAIL=G5_FAILED (the round was still skipped after the idle window: the export file did not advance)` | ❌ idle 窗口后**仍被跳过** ⇒ 闸门条件不成立 | **4** |

其它 ABORT 原因（同为退出码 5）：`EXPORT_FILE_NOT_FOUND`、`TOOL_MISSING`、`SERVER_NOT_RUNNING`、`ACCOUNT_WINDOW_NOT_OPEN (WM_COMMAND 40024 did not create the account window)`、`SYNC_NOW_BUTTON_MISSING (id 1016 not found)`。

### 7.4 INCONCLUSIVE / ABORT 的判读规则（**不允许多解释**）

1. **ABORT（5）= 环境不可用**：**不是**缺陷、**不是** PASS、**不是** FAIL。登记「ABORT + 原因串」，修环境后重跑，**不得**把 ABORT 写成「闸门有问题」。
2. **INCONCLUSIVE（6）= 结果不可归因**：只有两个来源 ——
   - `G3_DID_NOT_BLOCK`（G-3 没被拦住 ⇒ 这一对证明不了任何东西 ⇒ **既不记 PASS 也不记 FAIL**）；
   - `ENGINE_NOT_QUIESCENT`（触发前导出自己前进 ⇒ G-5 的前进无法归因 ⇒ 同上）。
   ⇒ 登记「INCONCLUSIVE + 原因串」，**禁止**升级为 PASS。
3. **FAIL（4）分两类**，登记时必须分开写：
   - **判据类**：`G5_FAILED`（idle 后仍被跳过）⇒ 这是**真结论**，指向闸门条件或 idle 口径有问题；
   - **造态类**：`HOLD_*` / `SYNTHETIC_*` / `G3_IDLE_*` / `G5_IDLE_*` ⇒ 状态没造出来，**结论无效**，修状态后重跑，**不得**当成闸门缺陷。
4. **任何一轮都必须同时留档**：退出码 + `console.txt` 全文 + `hold.stdout.txt` / `hold.stderr.txt` + `log-slice.txt` + 当轮导出文件 mtime 前后值。
5. **只有退出码 0 且 P 类前置（§1.1）也已满足时**，才可把 G-3/G-5 记为「已闭环」；否则按 **CONDITIONAL YES / `BLOCKED_HARNESS_PRECONDITION`** 口径登记。

---

## 8. 证据落点与留档纪律

| 产物 | 路径 |
|---|---|
| 每轮证据目录 | `F:\soft\00selfmade\rime_claude\tmp\fluxing-bg-g35-gate\g35-<yyyyMMdd-HHmmss>\` |
| 控制台落盘 | `…\console.txt`（utf8，含全部 `Log` 行） |
| harness 输出 | `…\hold.stdout.txt` / `…\hold.stderr.txt` |
| 诊断日志切片 | `…\log-slice.txt`（自本轮起点的字节偏移切到末尾） |
| 合成会话产物 | `…\hold.marker` / `…\hold.marker.state` / `…\hold.marker.pause` |
| 构建日志 | `F:\soft\00selfmade\rime_claude\tmp\bg-g35-msbuild.log` |
| 判定真相源 | `F:\soft\00selfmade\rime_claude\include\FluxingMaintenanceGate.h`（`:49` / `:50` / `:67-84`） |
| 判据原文 | `F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md` §4；`F:\soft\00selfmade\rime_claude\docs\release\regression-checklist-cloud-sync.md` §1 / §5 |

**留档纪律**：结论、退出码、`console.txt` 与导出文件 mtime 前后值必须**同轮**落盘；写完立刻 `Test-Path` + 字节数 + 行数回读（工具自述不算证据）；判据文档尽早随代码入库（BG 的 handoff 与本配方曾被并发还原抹掉一次）。

---

## 9. 硬前置缺失时怎么登记（照抄这段话）

> G-3 / G-5 的**条件可行性** = **CONDITIONAL YES**：闸门判据与唯一可观测面已定位（`include/FluxingMaintenanceGate.h:67-84`），造态链已具备（`--hold-commit` 四步 + 组合串自证 + 单会话 `.pause` 切换），真机判据口径已定（**维护导出文件 mtime 是否前进** + 相位 C 正控制）。
> **执行判定 = `BLOCKED_HARNESS_PRECONDITION`**：BG 的实验**从未真跑**；阻塞项 = ① 旧端须用**未含 R3 守卫的 1.3.5 构建**（非 1.3.4 装机包）、② 须**手工老化** `fluxing-rime-backup\profile.tar.gz` 的 mtime；两条均属**环境前置不可得**，⇒ **非缺陷、非 PASS、也非 FAIL**。
> 解除阻塞需：**第二台设备 + 用户在场**（另需双机 AC 7 条、restore 端到端、输入延迟两组对照）。

---

**BG 批次性质 = 取证 / 造态能力建设（未改 `librime/`、未改 IPC 协议消息类型、未新增依赖、未 bump 版本号）· 装机版 1.3.8.0 · BG 自身实验从未真跑 ⇒ CONDITIONAL YES / `BLOCKED_HARNESS_PRECONDITION`**
