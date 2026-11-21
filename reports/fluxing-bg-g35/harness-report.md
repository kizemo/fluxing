# 批次 BG · 缺陷 A 反向判据 G-3 / G-5 —— 闸门输入状态制造器（harness）报告

> 生成时间：2026-11-21 16:51:56
> 仓库：`F:\soft\00selfmade\rime_claude`（Fluxing / 火流猩，分支 `feat/100-cloud-sync`）
> 本批性质：**测试代码 + 取证脚本 + 文档**（**产品代码零改动**）
> 依据文档：`docs/release/restart-checklist-1.3.6.md` §4（G-3 / G-5 原文）、`docs/release/regression-checklist-cloud-sync.md`

---

## 0. 边界声明（先说没做什么）

**本批唯一改动/新建的文件**（除构建产物外）：

| 文件 | 性质 |
|---|---|
| `test/TestOrphanReclaimE2E/TestOrphanReclaimE2E.cpp` | **改**（测试工程，非产品码） |
| `scripts/verify/orphan-gate-g35-export-mtime.ps1` | **新建** |
| `tmp/bg-g35-harness-report.md` | **新建**（本文件） |
| `tmp/fluxing-bg-g35-gate/**`、`tmp/bg-g35-msbuild.log`、`tmp/holdcommit.marker*` | 临时证据 |

**明确未做**（逐条自查）：

- 未改 `WeaselServer/**`、`RimeWithWeasel/**`、`include/**`、`librime/**`、任何 `.rc`、`install.nsi`、`version.txt`、`env.bat`、任何 `.vcxproj`、`weasel.props`。
- **未触发任何云端同步**：未打开账号窗、未点「立即同步」、未跑脚本真实模式（只跑了 `-DryRun`）。
- 未执行 `/quit`、未杀任何宿主进程、未重启服务、未卸载/覆盖已安装的 1.3.8.0。
- 未改注册表；未 `git add` / `commit` / `push`。
- 未把任何账号 / 口令 / 令牌写进任何文件或本报告。

**环境快照**（本轮实测）：`WeaselServer.exe` 唯一 pid **1025692**，路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe`，`FileVersion` = **1.3.8.0**，登录态保持。

---

## 1. 交付物清单

| # | 绝对路径 | 字节 | 行 | mtime (UTC+8) |
|---|---|---|---|---|
| A | `F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.cpp` | 29 325 | 740 | 2026-11-21 16:46:53.9468093 |
| B | `F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe`（构建产物） | 181 248 | — | 2026-11-21 16:47:43.9096010 |
| D | `F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1` | 25 706 | 640 | 2026-11-21 16:50:23 |
| E | `F:\soft\00selfmade\rime_claude\tmp\bg-g35-harness-report.md` | 25 638（修订前） | 397（修订前） | 2026-11-21 16:51:56 |

> 行尾：`.cpp` 与 `.ps1` 均为 **LF**（`crlf=0`），与同目录既有脚本 `orphan-gate-export-mtime.ps1`（LF）一致。
> `.cpp` 与 `.ps1` 均**纯 ASCII**（逐字节扫描 `>127` 计数 = 0）。

---

## 2. A 项：`--hold-commit <marker> <active|idle> [seconds]`

### 2.1 要造的状态

`include/FluxingMaintenanceGate.h:67-84` 的判据（唯一真相源，ADR 0065）：

```
allows = live_sessions <= 0
      || !has_input_history
      || (composing_sessions > 0 ? idle_ms > 60000 : idle_ms > 10000)
```

常量：`kMaintenanceIdleGraceMs = 10*1000`（`:49`）、`kMaintenanceCompositionGraceMs = 60*1000`（`:50`）。

**目标状态 = `composing == 0` + `has_input_history == true` + 会话驻留（live > 0）**：

| 相位 | 行为 | idle | 闸门期望 | 对应判据 |
|---|---|---|---|---|
| `active` | 每 1500 ms 发一记刷新键 | < 10 s | **关闭**（拦下本轮） | **G-3（负向）** |
| `idle` | 造好状态后完全停手 | > 10 s（脚本按 ≥ 12 s 判定） | **放行** | **G-5（正向）** |

> 关键点：`composing == 0` 时闸门只看 **idle > 10 s** 这一条；`composing > 0` 时要看 **idle > 60 s**。
> 这就是上一批 `--hold`（组合串一直留着）**造不出 G-5** 的根因，也是本批必须新增 `--hold-commit` 的原因。

### 2.2 制造流程（4 步，全部有自证）

| 步 | 动作 | 自证字段 | 依据 |
|---|---|---|---|
| 1 | `START_SESSION` | `ipc_id`（≠0） | `WeaselIPCServer/WeaselServerImpl.cpp:265-276` |
| 2 | 打 13 个字母 `"fluxingorphan"` | `COMPOSING_AFTER_TYPING=1`（响应体 `status.composing=`） | `RimeWithWeasel.cpp:2055-2063` |
| 3 | 发 `WEASEL_IPC_COMMIT_COMPOSITION` | `COMMIT_SENT=1`（事务返回 result） | `include/WeaselIPC.h:30`；`RimeWithWeasel.cpp:762-773` |
| 4 | 发一记刷新键 `ibus::Left` | `COMPOSING_AFTER_COMMIT=0`（响应体 `status.composing=0`） | `RimeWithWeasel.cpp:740` `_SyncSessionComposing` |

**自证为什么可信**：响应体里的 `status.composing=` 由 `rime_api->get_status()` 产生，
而闸门读的 `m_composing_session_count` 也正是由同一个 `get_status` 结果同步而来
（`RimeWithWeasel.cpp:349-359` `_SyncSessionComposing` → `:338-345` `_SetSessionComposing` → `:328-335` `_RefreshComposingSessionCount`）。
**不是「推断组合串没了」，是「服务端自己说组合串没了」**——且不依赖任何状态文案。

### 2.3 为什么提交用 IPC 命令，而不是「猜一个键」

- 源码确认：`include/WeaselIPC.h:19-41` 枚举自 `WM_APP+1` 起算 ⇒ `WEASEL_IPC_COMMIT_COMPOSITION` = **0x800B**。
- 服务端 `WeaselServerImpl.cpp:382-388`：`OnCommitComposition` 取 **lParam 作 session id**（与 END_SESSION / PROCESS_KEY_EVENT 一致），调 `m_pRequestHandler->CommitComposition(lParam)`。
- 落地 `RimeWithWeasel.cpp:762-773`：`rime_api->commit_composition(session_id)` + `_SetSessionComposing(ipc_id, false)`（:769-770）⇒ `m_composing_session_count` 归 0。
- 事务配平：`HandlePipeMessage` 末尾无条件 `resp(result)`（`WeaselServerImpl.cpp:544`）⇒ 即使该命令**不写 body**，客户端仍会收到 4 字节 result，不会挂死。
- keysym 语义上 `Return=0xFF0D` 会提交、`Escape=0xFF1B` 会清组合（`include/KeyEvent.h`），但**键的语义随 schema / 输入法模式变化**；IPC 命令是确定性的，与 schema 无关 ⇒ **选 IPC**。

### 2.4 为什么刷新键选 `ibus::Left`（0xFF51）

刷新键必须同时满足两条：**刷新「最近一次输入」** + **不产生组合串**。

| 候选键 | 刷新 tick？ | 会建组合串？ | 结论 |
|---|---|---|---|
| `ibus::Left` 0xFF51 | ✅ 任何键都会 | ❌ `ch=0xFF51 > 0x7f` ⇒ `editor.cc:58-62` 的 char handler 不收；`ctx->IsComposing()` 为假 ⇒ `editor.cc:51-57` 不查 key binding | **选用** |
| `Shift` | ✅ | ⚠️ `ascii_composer.cc:79-104`：500 ms 内 down+up 会**切换 ascii 模式** | 禁用（会改输入法状态） |
| `Escape` 0xFF1B | ✅ | ⚠️ 会**清掉**组合串 ⇒ 若提交失败反而被掩盖 ⇒ 假绿 | 禁用 |
| 数字键 | ✅ | ⚠️ 中文模式下会被 translator 收下，可能建组合 | 禁用 |

- 「任何键都刷新 tick」：`RimeWithWeasel.cpp:713-717`，`m_last_input_tick` 在**每个** `ProcessKeyEvent` 无条件写入（0 保留为「从未按过键」）。
- 「Left 不建组合串」：`librime/src/rime/gear/editor.cc:46-66`（:47-48 release 被拒；:51-57 仅在组合态查 key binding；:58-62 char handler 要求 `0x20 < ch < 0x7f`）。
- 运行时兜底：刷新键每发一次都读一次 `status.composing=`，**一旦出现 1 立即以退出码 3 失败**（`a composition reappeared in the steady state`）。

### 2.5 idle 的测量口径

- harness 用 `GetTickCount64()` 记录**最后一记键的发送时刻**，写进 `<marker>.state` 的 `last_input_tick_ms`；脚本用 `[Environment]::TickCount64`（同一系统时钟）算差值。
- 脚本侧取**保守下界**：客户端记录的发送时刻 ≤ 服务端实际处理时刻 ⇒ **我方测得的 idle ≤ 真实 idle**。
  - G-3 判「idle < 10 s」：下界 < 10 s ⇒ 真实更小 ⇒ **天然保守**。
  - G-5 判「idle ≥ 12 s」：留 2 s 余量覆盖 IPC 往返与调度抖动。
- `active` 相位刷新周期 1500 ms ⇒ 稳态 idle ≈ 1.5 s，距 10 s 门槛有 ~8.5 s 余量。

### 2.6 产物文件与退出码

| 文件 / 退出码 | 含义 |
|---|---|
| `<marker>` | 一次写：`ipc_id` / `pid` / `phase` / `composing_after_commit` / `last_input_tick_ms` |
| `<marker>.state` | 每次刷新重写：`last_input_tick_ms` / `composing` |
| `<marker>.pause` | 存在时 `active` 相位停止刷新（⇒ idle 增长）⇒ **一个进程即可承载 G-3 / G-5 两个相位** |
| 0 | 正常（造好状态；给了 `seconds` 则跑满后自行退出） |
| 1 | 连接 / 协议错误 |
| 2 | 用法错误（沿用既有约定） |
| 3 | **状态没造出来**（打完字 `composing≠1`，或提交后组合串仍在） |

### 2.7 全程不发 END_SESSION

会话必须**驻留**（`live_sessions > 0` 才有闸门可言）；harness 被杀时服务端才走 ADR 0067 的孤儿回收路径。

---

## 3. B 项：显式 msbuild（只此一个工程）

### 3.1 命令与结果

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.vcxproj' `
  /p:Configuration=Release /p:Platform=Win32 "/p:SolutionDir=F:\soft\00selfmade\rime_claude\" `
  /nologo /v:minimal /m:1 /fl "/flp:logfile=F:\soft\00selfmade\rime_claude\tmp\bg-g35-msbuild.log;verbosity=normal"
```

- **`MSBUILD_EXIT=0`**
- 依赖工程（`ProjectReference`）连带编译：`WeaselUI.lib` / `RimeWithWeasel.lib` / `WeaselIPCServer.lib` / `WeaselIPC.lib` ⇒ 落在 `msbuild\Release\Win32\`。
- **未构建 `weasel.sln`**；未改任何 `.vcxproj`。
- 产物：`F:\soft\00selfmade\rime_claude\Release\TestOrphanReclaimE2E.exe`（`OutDir = $(SolutionDir)\$(Configuration)\` ⇒ 仓库根 `release\`）。

### 3.2 mtime 核验（exe 必须 ≥ 源码）

| 项 | mtime (UTC+8) | 判定 |
|---|---|---|
| `TestOrphanReclaimE2E.cpp` | 2026-11-21 16:46:53.9468093 | — |
| `release\TestOrphanReclaimE2E.exe` | 2026-11-21 16:47:43.9096010 | ✅ **晚 49.96 s** ⇒ 是新编的，不是旧产物 |

### 3.3 ⚠️ 踩坑（必读，否则下次还会踩）

**单工程构建必须显式传 `$(SolutionDir)`。**
第一次不带 `/p:SolutionDir` 直接构建 ⇒ 依赖工程 `WeaselUI` 报错：

```
WeaselUI\stdafx.h(15,10): error C1083: 无法打开包括文件: “wtl/atlapp.h”
MSBUILD_EXIT=1
```

根因：所有工程用 `$(SolutionDir)\include` 之类的相对路径（`WeaselUI.vcxproj:151`、
`TestOrphanReclaimE2E.vcxproj:157` 等），而 `$(SolutionDir)` **只在构建 sln 时由解决方案注入**；
单工程构建时它为空 ⇒ `include\wtl`（实际存在于 `F:\soft\00selfmade\rime_claude\include\wtl`）找不到。

**仓库自己已有这条配方**（`build.bat:339-342`，原文）：

```
rem NOTE: building a .vcxproj outside the solution leaves $(SolutionDir)
rem $(SolutionDir)-relative -> pass it explicitly.
msbuild.exe WeaselTSF\WeaselTSF.vcxproj ... /p:SolutionDir=%WEASEL_ROOT%\ ...
```

`BOOST_ROOT` 已由 `weasel.props` 的 `UserMacros` 提供（`F:\b183`），无需额外设置。

---

## 4. B' 项：不联云自检（本轮实跑）

### 4.1 命令

```powershell
F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe `
  --hold-commit F:\soft\00selfmade\rime_claude\tmp\holdcommit.marker idle 6
```

### 4.2 实际输出（原文）

```
HOLDCOMMIT_SESSION ipc_id=3 pid=482580 phase=idle
COMPOSING_AFTER_TYPING=1 ASCII_MODE=0
COMMIT_SENT=1
COMPOSING_AFTER_COMMIT=0
HOLDCOMMIT_STARTED ipc_id=3 pid=482580 last_input_tick_ms=32017875
HOLDCOMMIT_DONE idle_ms=6031
SELFTEST_EXIT=0 ELAPSED_SEC=7.1
```

### 4.3 这次自检证明了什么

| 观察 | 结论 |
|---|---|
| `COMPOSING_AFTER_TYPING=1` | 引擎**真的**持有未上屏组合串（不是 ascii 模式，`ASCII_MODE=0`） |
| `COMMIT_SENT=1` | IPC 提交命令被服务端接受，事务未挂死（无 body 也回 DWORD） |
| `COMPOSING_AFTER_COMMIT=0` | 提交后**刷新键的响应体自证**组合串已清 ⇒ `m_composing_session_count` 归 0 |
| `idle_ms=6031` | `idle` 相位确实停手（6 s 窗口内 idle 增长到 6.031 s） |
| `SELFTEST_EXIT=0` / 7.1 s | 定时退出路径正常 |

### 4.4 二进制层面的补证（装机版能不能自证）

用 ASCII 令牌扫描两个二进制：

| 文件 | 令牌 | 结果 |
|---|---|---|
| `D:\Program Files\fluxing\weasel\WeaselServer.exe`（装机 **1.3.8.0**） | `status.composing=` | ✅ 命中，偏移 **2 140 528** |
| `release\TestOrphanReclaimE2E.exe`（新编） | `status.composing=` / `--hold-commit` | ✅ 命中 158 544 / 160 476 |

⇒ **自证通道在真机装机版上可用**，不依赖未安装的版本。

---

## 5. C 项：闸门「判定结果」的可观测点（只读查找，未改任何产品码）

### 5.1 候选点逐个核实

| 候选观测点 | 结论 | 依据 |
|---|---|---|
| ASCII 原因令牌 `live_input_session` | ❌ **运行时不可观测** | 令牌定义在 `include/FluxingMaintenanceGate.h:38-40`，赋给 `reason`（`RimeWithWeasel.cpp:321`），但 `CloudSyncService.cpp:878-884` **丢弃** `gate_reason`、换成通用中文文案 `MaintenanceGateSkipMessage()`（`CloudUserDictIo.h:113-120`）。`CloudRimeUserDictBackend.cpp:249-266` 只是转发。全仓无任何地方把它写进日志 / UI / IPC。 |
| 控件 `kIdSyncStatus = 1017` | ❌ **不可作判据** | `kIdSyncStatus` 在 `CloudAccountDialog.cpp` 全文**只出现在 :955-956**（静态 `WidgetText()` 字面量「最近同步：尚未同步。」）；`RefreshUi`（:1299-1352）只写 `kIdStatus`（:1311-1313），**从不重写 1017** ⇒ 永远读回初始串。 |
| 控件 `kIdStatus = 1006` | ⚠️ **只能作提示，不能作判据** | 手动同步路径 `CloudAccountDialog.cpp:839-844` 用 `SetStatus(CloudStatusScope::kAccount, …)` 写「同步已跳过：…」，`RefreshUi` 再把它写进 1006（:1311-1313）。但 `RefreshUi` 的触发时机未验证 ⇒ 只能当提示。（脚本里用 code unit `U+8DF3 U+8FC7` 匹配「跳过」两字，**保持脚本纯 ASCII**。） |
| `CloudSyncStatus` / `Impl::sync_status_text` | ❌ 脚本不可达 | `CloudAccountDialog.cpp:232-235`、`:501-504` 是 C++ 内部 API，不暴露给脚本。 |
| 诊断日志 `%TEMP%\fluxing-profile-notify.log` | ❌ 无闸门记录 | `CloudAccountDialog.cpp:1068-1079` 的 `LogAccountDlg` → `fluxing::diag::AppendLine("acctdlg", …)`（`include/FluxingDiagLog.h:27-32`），其调用点全是窗口 / DPI / 销毁类消息，**未发现闸门判定被写入**。 |
| **导出文件 mtime** | ✅ **唯一可用的判据** | 既有脚本 `scripts/verify/orphan-gate-export-mtime.ps1` 已确立此口径；`docs/release/restart-checklist-1.3.6.md` §4：「闸门是否放行 = **维护导出文件 mtime** 是否前进（状态文案不可作判据）」。 |

### 5.2 结论（**verified**）

**服务端没有「闸门判定结果」的直接可观测点**——ASCII 原因令牌在云侧被丢弃，1017 是静态字面量，诊断日志无闸门记录。
⇒ **只能用导出文件 mtime 作代理判据**；1006 的文本仅作辅助提示。

> 这与上一批（BF）否证的那条「枚举不到控件即通过」是同一种教训：
> **没有可观测点 ⇒ 必须承认只能间接判定，并在报告里写明间接链路的每一环。**

---

## 6. D 项：`scripts/verify/orphan-gate-g35-export-mtime.ps1`

### 6.1 规格

| 项 | 值 |
|---|---|
| 路径 | `F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1` |
| 字节 / 行 | 25 706 / 640 |
| 编码 | **纯 ASCII**（逐字节扫描 `>127` = 0）；行尾 LF（与同目录既有脚本一致） |
| 语法 | `[Parser]::ParseFile` ⇒ **`PARSE_ERRORS=0`** |
| 依赖 | 运行中的 `WeaselServer.exe` + `release\TestOrphanReclaimE2E.exe`（新编）+ 已登录账号 + 导出文件存在 |
| 副作用 | 只开账号窗、只发 WM_COMMAND 1016、只杀**自己起的** harness 子进程；不 `/quit`、不重启、不改注册表、不卸载 |
| 证据落点 | `tmp\fluxing-bg-g35-gate\g35-<时间戳>\`（`console.txt` / `hold.stdout.txt` / `hold.stderr.txt` / `log-slice.txt`） |

### 6.2 三个相位

| 相位 | 合成会话状态 | 期望 | 判据 |
|---|---|---|---|
| **C**（control） | 无合成会话 | 本轮**不被跳过** | 导出文件 mtime **必须前进**（否则整轮 ABORT：没有正控制，G-3/G-5 无意义） |
| **G3**（负向） | 同一会话，`active` 相位（idle < 10 s）、`composing == 0`、输入历史存在 | 闸门**保持关闭** | 导出文件 mtime **必须不前进**（观察窗 = `ceil(控制轮耗时)+15 s`，15–60 s） |
| **G5**（正向） | **同一会话**，`active` + `<marker>.pause`（停刷新 ⇒ idle ≥ 12 s），`composing == 0` | 闸门**放行** | 导出文件 mtime **必须前进**（窗口 = `-RoundWaitSec`，默认 150 s） |

**G3 与 G5 只差一个变量（用户是否还在打字）** ⇒ 这一对就是 G-3 / G-5 的证明。

### 6.3 三个关键设计决策

1. **一个会话 + `.pause` 切换，而不是两个会话。**
   `m_last_input_tick` 是**全局**量（`RimeWithWeasel.cpp:715-717`），任何会话的任何键都会刷新它。
   若 G3 用 `active` 会话、G5 用 `idle` 会话**并存**，那个 `active` 会话会不断刷新全局 tick ⇒ G5 的 idle 窗口永远不成立。
   ⇒ 单会话 + pause 切换是唯一能让「idle」真正成立的做法。
   （因此脚本统一用 `--hold-commit <marker> active`；harness 的 `idle` 相位仍保留，供独立使用与自检。）
2. **相位顺序 C → G3 → G5。**
   自发轮次**不可能**造成 G-3 假阴性以外的误判（被闸门挡住的轮次不会导出）；
   但**可能**造成 G-5 假阳性（自发轮次自己导出了）。⇒ 在触发 G-5 之前做**静止性观察**：
   停手后不触发任何东西，盯 20 s（`-QuiesceSec`），若导出文件自己前进 ⇒ 引擎不静止 ⇒ 判 **INCONCLUSIVE**（G-5 的前进无法归因），不做 PASS 也不做 FAIL。
3. **判据只用 mtime；文案只进日志。**
   1006 的文本读出来只写进 `console.txt` 与 `STATUS_AFTER_*` 行（含一个 `SKIPPED_HINT` 位），**不进判据**（§5 + `restart-checklist-1.3.6.md` §4）。

### 6.4 命令用法

```powershell
# 预演（不碰服务端、不开窗、不同步）—— 本轮已跑通，退出 0
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All -DryRun

# 正式三相位（★ 会真的开账号窗并点「立即同步」，需要登录态 + 用户在场）
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All

# 单相位
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode C
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G3
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G5

# 可调参数
pwsh -NoProfile -File scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All `
  -RoundWaitSec 150 -IdleWaitSec 15 -QuiesceSec 20
```

**退出码**：`0` = `VERDICT=GATE_G35_OK`；`4` = `VERDICT=GATE_G35_FAIL`；
`5` = ABORT（环境不可用）；`6` = `VERDICT=GATE_G35_INCONCLUSIVE`。

### 6.5 `-DryRun` 实跑输出（本轮）

```
REPO_ROOT=F:\soft\00selfmade\rime_claude
TOOL=F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe
MODE=All PHASES=C,G3,G5
IDLE_GATE_MS=10000 IDLE_MARGIN_MS=12000 IDLE_REFRESH_MS=1500
USER_DIR=D:\Program Files\fluxing\user1\fluxing
EXPORT_FILE=D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt
EXPORT_SIZE=4139102 EXPORT_MTIME_INITIAL=2026/11/21 16:47:27
TOOL_PRESENT=True
SERVER_PROCESSES=1
SERVER pid=1025692 path=D:\Program Files\fluxing\weasel\WeaselServer.exe ver=1.3.8.0
DRYRUN=1 (no window, no harness, no sync round, nothing changed)
VERDICT=GATE_G35_DRYRUN
EXIT=0
```

---

## 7. 判据分级总表

### 7.1 verified（本轮亲自跑过，或有直接源码 / 二进制证据）

| # | 断言 | 证据 |
|---|---|---|
| V1 | `--hold-commit` 模式可运行，四段自证输出齐全，退出码 0 | §4.2 实跑输出 |
| V2 | 打完字**真的**有未上屏组合串（`status.composing=1`） | §4.2 `COMPOSING_AFTER_TYPING=1` |
| V3 | IPC 提交（0x800B）**真的**清掉了组合串，且由服务端响应体自证 | §4.2 `COMPOSING_AFTER_COMMIT=0` + `RimeWithWeasel.cpp:762-773` |
| V4 | 刷新键 `ibus::Left` 不产生组合串 | 源码 `editor.cc:46-66` + 运行时 composing 恒 0（V3 同一通道） |
| V5 | 无 body 的命令（COMMIT_COMPOSITION）仍回 DWORD，事务不会挂死 | `WeaselServerImpl.cpp:544`（`resp(result)` 无条件）+ §4.2 事务正常完成 |
| V6 | 装机版 1.3.8.0 服务端二进制含 `status.composing=` ⇒ 自证通道真机可用 | §4.4 偏移 2 140 528 |
| V7 | 编译成功且产物是新的 | `MSBUILD_EXIT=0`；exe mtime 16:47:43 > 源码 16:46:53（§3.2） |
| V8 | 单工程构建必须传 `/p:SolutionDir=` | §3.3 实测失败 → 加参数后成功；与 `build.bat:339-342` 一致 |
| V9 | 闸门判据的精确形式（`live<=0` / `!has_history` / `composing?60s:10s`）与常量 10 s / 60 s | `include/FluxingMaintenanceGate.h:49-50,67-84` |
| V10 | 「任何键都刷新 `m_last_input_tick`」 | `RimeWithWeasel.cpp:713-717` |
| V11 | 服务端**没有**闸门判定的直接可观测点（原因令牌被丢弃 / 1017 静态 / 日志无记录） | §5 逐条 file:line |
| V12 | 跳过的轮次**不进**失败退避；手动轮次优先级最高 ⇒ G-5 可紧接 G-3 | `CloudSyncEngine.cpp:340-350`；`CloudAccountDialog.cpp:811-814` |
| V13 | `kMaintenanceSkipRetryIntervalMs` 在 `WeaselServer/` 内**零引用**（只在头文件定义） | 全目录 grep 无匹配 |
| V14 | 脚本纯 ASCII、`PARSE_ERRORS=0`、`-DryRun` 退出 0 且未触碰服务端 | §6.1 / §6.5 |
| V15 | `.cpp` 保持纯 ASCII + LF | 逐字节扫描 |

### 7.2 inferred（源码推理 + 局部证据；**真机结果本轮未跑**）

| # | 断言 | 推理链 | 缺什么 |
|---|---|---|---|
| I1 | `active` 相位（idle < 10 s + `composing==0`）⇒ 闸门关闭 ⇒ 导出文件不前进 | V9 判据 + V3 状态成立 ⇒ 闸门必关；闸门关 ⇒ `SyncOnce` 整轮跳过（`CloudSyncService.cpp:876-886`，**零 I/O / 零导出**）⇒ mtime 不动 | 端到端未跑（脚本真实模式） |
| I2 | `idle` 相位（idle ≥ 12 s + `composing==0`）⇒ 闸门放行 ⇒ 导出文件前进 | V9 判据 + idle > 10 s ⇒ allow ⇒ 走 `ExportUserDict`（`CloudSyncService.cpp:913-923`） | 同上 |
| I3 | 同一会话 + `.pause` 切换可承载 G-3 / G-5 的单变量对比 | §6.3 第 1 条（全局 tick 语义） | 同上 |
| I4 | 静止性观察（20 s）足以识别自发轮次 | 依赖引擎自发轮次周期 > 20 s 且不落在观察窗内 | 引擎自发轮次周期未取证 |
| I5 | 1006 在手动轮次后会刷新出「跳过」文案 | `CloudAccountDialog.cpp:839-844` → `RefreshUi` :1311-1313 | `RefreshUi` 在手动轮次后的触发时机未验证 ⇒ 脚本里只作提示 |
| I6 | 提交用 IPC 比用 `Return` 键更稳 | 键语义随 schema / 输入模式变化（`include/KeyEvent.h` 语义 vs librime 实际行为） | 未实测 `Return` 路径 |

### 7.3 not verified（本轮明确**没做**，不得写成已验）

| # | 项 | 原因 |
|---|---|---|
| N1 | **G-3 真机端到端**（导出文件不前进） | 本轮禁止真同步；脚本真实模式未跑 |
| N2 | **G-5 真机端到端**（导出文件前进） | 同上 |
| N3 | 控制轮 C（导出文件前进） | 同上 |
| N4 | G-3 / G-5 **完整**闭环 | 还需双机 AC 7 条、restore 端到端、输入延迟对照（需两台机器 + 用户在场） |
| N5 | 真实 IME 宿主同时打字对全局 `m_last_input_tick` 的污染 | 无直接观测点；脚本靠闸门端到端结果兜底 |
| N6 | 175% / 200% DPI 下的同一判据 | 用户已拍板不主动切档 |
| N7 | `librime` translator 层对方向键的逐一行为 | `translator.cc` 实现文件未定位；已由 editor 层源码 + 运行时 composing 恒 0 兜底 |
| N8 | 「闸门判定结果写入诊断日志」 | **需要改产品代码**，本批明确不做（见 §8） |

---

## 8. 未闭环 / 待主会话或用户决策

1. **G-3 / G-5 的真机闭环需要一次「用户在场」的窗口**：
   跑脚本真实模式要求 (a) 账号已登录（当前是，pid 1025692 未重启）；(b) **跑的过程中不要在别的窗口打字**——`m_last_input_tick` 是全局量，别的宿主打字会把 idle 拉回 0，G-5 会假红。
2. **建议（不在本批实施）**：若希望判据**可直读**而不是靠导出文件 mtime 代理，需要在服务端把 `gate_reason`（`live_input_session`）写进 `%TEMP%\fluxing-profile-notify.log` —— 那是**产品代码改动**，按 constitution 需单独一批（ADR + 四边同步 + 测试），本批不做。
3. **本批改动尚未提交**：按 AGENTS.md §9，本批完成一个阶段后应同步 handoff + 接力 prompt；**提交与 push 由主会话决定**（本会话未执行 `git add` / `commit` / `push`）。
4. **未闭环清单（承接上一批）**：双机 AC 7 条、restore 端到端、输入延迟对照、缺陷 A 反向判据 G-3/G-5 的完整闭环。装机版仍为 **1.3.8.0**。

---

## 9. 证据文件

| 文件 | 内容 |
|---|---|
| `tmp\bg-g35-msbuild.log` | 本次 msbuild 完整日志（`verbosity=normal`） |
| `tmp\holdcommit.marker` / `.marker.state` | 自检留下的 marker（`ipc_id=3` / `pid=482580` / `phase=idle` / `composing=0` / `last_input_tick_ms=32017875`） |
| `tmp\fluxing-bg-g35-gate\g35-20261121-165038\console.txt` | `-DryRun` 的控制台落盘 |
| `release\TestOrphanReclaimE2E.exe` | 新编产物（181 248 B，mtime 16:47:43.9096010） |

> 回读纪律（已执行）：三个交付文件均以 `Test-Path` + 字节数 + 行数同轮回读确认 —
> A `.cpp` = `EXISTS=True` / 29 325 B / 740 行 / mtime 16:46:53；
> D `.ps1` = `EXISTS=True` / 25 706 B / 640 行 / mtime 16:50:23；
> E 本文件 = `EXISTS=True` / 25 638 B / 397 行 / mtime 16:51:56（该数字为占位符修订**之前**的实测值，修订后最终字节/行数见主会话最终回答）。
> 另核：`release\TestOrphanReclaimE2E.exe` mtime 16:47:43 **晚于** 源码 mtime 16:46:53 ⇒ `EXE_NEWER=True`；
> `.cpp` 与 `.ps1` 逐字节扫描 `>127` 计数均为 **0**（纯 ASCII 成立）。
