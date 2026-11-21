# B-5 真机端到端取证：旧客户端（不写 `session.client_pid=`）的孤儿会话**绝不**被 ADR 0067 回收

- 批次：BB 第 1 项（清单 `docs/release/restart-checklist-1.3.6.md` §2 B-5 的判据，装机版 1.3.8.0）
- 运行时间：2026-11-21 09:55:46 → 09:58:0x（一次运行内阳性 + 阴性同时成立）
- 证据目录：`reports/fluxing-bb-orphan-reclaim/b5-e2e-20261121-095546/`
- 探针脚本：`scripts/verify/b5-legacy-client-e2e.ps1`
- 合成客户端改动：`test/TestOrphanReclaimE2E/TestFocusOutE2E.cpp`（新增 `-Legacy` 模式）
- 编译日志：`reports/fluxing-bb-orphan-reclaim/b5-build.log`

## 1. 判词（机读原文）

```
FAILURE_COUNT=0
B5_VERDICT=PASS_E2E
EXIT=0
```

`verdict.txt` 原文：

```
B5_VERDICT=PASS_E2E
FAILURE_COUNT=0
EXIT=0
OUT_DIR=F:\soft\00selfmade\rime_claude\reports\fluxing-bb-orphan-reclaim\b5-e2e-20261121-095546
```

## 2. 环境与前置事实（脚本自证输出）

```
SERVER pid=43172 path=D:\Program Files\fluxing\weasel\WeaselServer.exe ver=1.3.8.0
LOG_BASELINE_BYTES=300782
TOOL_BYTES=197632 TOOL_MTIME=2026/11/21 9:51:07        # exe.mtime >= cpp.mtime(9:47:45) => 非假绿
```

未启动 / 未停止 / 未重启 / 未安装任何服务端；未改注册表；未改 `librime/`；未改 IPC 协议；未新增服务端日志字段。

## 3. 方法

### 3.1 旧客户端模式怎么造

`IpcClient::StartSession(..., bool legacy = false)` 只在握手体里**逐字省略**一行，其余四行完全一致：

```
action=session
session.client_app=<本 exe 名小写>
session.client_type=tsf
session.client_pid=<pid>        <-- 仅现代客户端写
.
```

开关带**自证行**（防止开关静默失效 ⇒ 阴性判据变成空测）：

```
CLIENT_MODE=legacy handshake_client_pid_line=0 pid=637708
CLIENT_MODE=modern handshake_client_pid_line=1 pid=637776
```

默认参数 ⇒ `b4-focusout-path.ps1` 的既有调用向后兼容。

### 3.2 消息序

| 角色 | 命令行 | 消息序 | 结果 |
|---|---|---|---|
| 旧客户端孤儿 **L** | `--abrupt-exit legacy.marker -Legacy` | START_SESSION（无 pid 行）→ FOCUS_IN(APP_FOCUS_GAINED) → 打字 2100 ms → **直接 Close()，不发 END_SESSION** | ipc_id=**2829320193**，pid=637708（进程已死，服务端记 pid 未知） |
| 现代客户端孤儿 **M** | `--abrupt-exit modern.marker` | 同上，但握手写 pid | ipc_id=**2829320194**，pid=637776（进程已死） |
| 清扫触发 **T1** | `--sweep-trigger trigger1.marker` | START_SESSION → 立即退出（`AddSession` 已发生 ⇒ 惰性清扫已跑） | ipc_id=2829320195，随后被显式 `END_SESSION` |
| 裸清扫触发 **T2** | `--sweep-trigger trigger-bare.marker` | 同上 | ipc_id=2829320194（表清空后的新 epoch 复用值，见 §6） |

ipc_id / pid 全部由客户端自己写进 marker 文件（`ipc_id=` / `pid=` 两行），**不依赖服务端新日志**。

### 3.3 断言口径

- 阳性：`reclaim orphan session ipc_id=<M>\b ` 命中 + `remove_session active orphan_reclaim presence=kept`。
- 阴性（B-5 本体）：`reclaim orphan session ipc_id=<L>\b ` **计数 0**；`client_pid=<pL>\b ` 计数 **0**；`ECHO=<L>` 非 0（第二条独立证据通道）；清扫切片内 `presence=absent` 计数 0。
- 判据行格式单一事实源 = `include/FluxingSessionReclaim.h:55-65`（`ipc_id=` 后必带空格 ⇒ 精确 id 匹配安全）。
- 阳性对照打不出来 ⇒ `INCONCLUSIVE`（exit 6），**不**算 PASS 也不算 FAIL。

## 4. 两条对照的原始日志行（`evidence-positive.txt` / `log-slice.txt`，逐字）

**阳性对照 —— 现代客户端 M 被回收：**

```
2026-11-21 09:55:54.966][server] remove_session active orphan_reclaim presence=kept
2026-11-21 09:55:54.969][server] reclaim orphan session ipc_id=2829320194 client_pid=637776 live=2
```

**阴性判据 —— 旧客户端 L 全程零回收行：**整轮运行级切片里 `reclaim orphan session` **只有 1 行**，即上面那行（`RUN_RECLAIM_LINES=1`），指向 `ipc_id=2829320194 / client_pid=637776`；对 `2829320193` 与 `637708` 的匹配计数均为 0。

**同轮清扫的机器读数（阶段 3–5 控制台）：**

```
SWEEP_ATTEMPT=1 slice_bytes=183 reclaim_for_modern=1
POSITIVE_CONTROL=OBSERVED attempt=1 ipc_id=2829320194
RECLAIM_LINES_FOR_LEGACY=0 (must be 0)
RECLAIM_LINES_CARRYING_LEGACY_PID=0 (must be 0)
ORPHAN_RECLAIM_PRESENCE_KEPT_LINES=1
PRESENCE_ABSENT_LINES=0 (must be 0: a reclaim must never read as 'Fluxing left')
LEGACY_ECHO_AFTER=2829320193 (still alive => not reclaimed, second evidence channel)
MODERN_ECHO_AFTER=0 (reclaimed => positive control confirmed on the second channel)
BARE_SWEEP_RECLAIM_LINES=0
LEGACY_ECHO_BARE=2829320193 (survived a sweep that had it as its only candidate)
```

裸清扫阶段（先显式 `END_SESSION` 清掉全部 trigger 会话 ⇒ 表里唯一外来会话就是 L）零回收，使阴性判据**不是空测**：同一方法一轮前刚产出过 victims。

## 5. 附带观测：旧客户端残留会话不堵云同步闸门

非 FAIL 判据，已记录：

```
GATE_EXPORT_FILE=D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-.../rime_ice.userdb.txt
GATE_CONTROL_EXPORT_ADVANCED_SEC=48.6
GATE_LEGACY_IPC_ID_STILL_ALIVE=2829320193
GATE_OBS=ADVANCED_WITH_LEGACY_RESIDUAL after_sec=0 (the leftover legacy orphan session does not block the round)
```

导出文件 mtime 在 L 仍注册的情况下继续前进 ⇒ 残留旧客户端会话**不会**把闸门永久堵死。
（`after_sec=0` 表示第二次触发轮在检查时 mtime 已经前进；对照轮 48.6 s 才是新鲜一次导出。判据只用导出文件 mtime，未使用状态文案。）

## 6. 「旧客户端会话在真机上如何收场」——如实登记

**观察到的事实**：阶段 7 我们**还没发** `END_SESSION`，`ECHO=2829320193` 就已经回 0（`LEGACY_GONE_WITHOUT_END_SESSION=1`）。也就是说它在某个时刻从服务端消失了。

**排除的一条**：绝不是 ADR 0067 回收的 —— 整轮只有一行 `reclaim orphan session`，且指名 `client_pid=637776`（现代那个）。旧客户端的 ipc_id 与 pid 两个匹配计数都是 0。

**指认的机制（代码 + 时间戳，非直接日志）**：服务端在维护窗口**整表清空会话、逐会话不留一行日志**：

- `RimeWithWeasel.cpp:1411-1425` `StartMaintenance()` ⇒ `m_maintenance_active=true` + `m_session_status_map.clear()` + `Finalize()`
- `RimeWithWeasel.cpp:1428-1440` `EndMaintenance()` ⇒ 再次 `m_session_status_map.clear()`
- `RimeWithWeasel.cpp:266-276` `Finalize()` ⇒ 同样清表

磁盘上该导出文件的最后写入时间是 **09:57:25**，正落在「仍存活的那次 ECHO」与「已消失的那次 ECHO」之间 —— 而阶段 6 正是我们主动触发的云同步回合（按 ADR 0062/0061，export/import userdb 必须由 Start/EndMaintenance 包裹）。⇒ 旧客户端孤儿的真机收场 = **被下一次维护窗口整表清掉**，不是被孤儿回收。

**未证明的部分（不夸大为已证）**：诊断日志在 09:56–09:59 没有任何服务端行（清表路径本身不打日志），所以「09:57:25 那次导出确实清了 2829320193」是**代码 + 时间窗推断**，不是直接取证。

**顺带挖出的一条口径约束**：`_GenerateNewWeaselSessionId` = 表空时 `m_pid + 1`、否则 `rbegin()->first + 1`。⇒ **ipc_id 只在一个表 epoch 内单调唯一，清表后新 epoch 会重号**（本轮裸清扫触发就拿到了与已回收的现代孤儿相同的 2829320194）。以后凡跨维护窗口用 ipc_id 断言，必须**同时**核 `client_pid`，否则可能张冠李戴。本轮判据不受影响：阳性那行 `client_pid=637776` 与阶段 2 的现代客户端 pid 逐字相同。

**另一条如实登记**：`presence=absent` 在本轮**确实出现过一次**（09:55:59.414），但它来自我们自己的显式 `END_SESSION` 收尾（正常路径，合法翻「火流猩不在场」），**不在**清扫切片内，也不指向任何回收 —— 阶段 4 的 `PRESENCE_ABSENT_LINES=0` 断言口径是「清扫切片的回收路径不得误翻在场」，不是「整轮任何一行都不许出现 absent」。这个区别已写进脚本注释与本节。

## 7. 覆盖边界（不夸大）

已证：
1. 旧客户端握手不写 `session.client_pid=` ⇒ 服务端把该会话当「pid 未知」⇒ **同一轮清扫把现代客户端孤儿收走、把这个旧客户端孤儿放过**（同一次运行、同一张会话表、同一批 `AddSession` 触发）。
2. 放过它时**没有**误翻在场（`orphan_reclaim presence=kept` 命中、清扫切片无 `presence=absent`）。
3. 表里只剩它一个外来会话时清扫仍零 victims（非空测）。
4. 它的残留不堵云同步闸门。

未证 / 不在本判据范围：
- 真实老版本 TSF shim（如 1.3.4 的 `weaselx64.dll`）与新服务端混用的实机组合 —— 本轮用**合成客户端逐字省略一行**代表「旧客户端」，不是拿真旧 DLL 跑的。
- 旧客户端孤儿在**长期不触发维护窗口**的场景下会驻留多久（本轮它是在 2 分钟内被维护窗口清掉的）。
- 1.3.8 真机双机 AC 7 条、restore 端到端、输入延迟对照等其他清单项。
- 未装机、未出包、未提交、未 push（本批禁止）。

## 8. 复现命令

```powershell
# 1) 编译合成客户端（该工程不在 weasel.sln 里，必须显式构建并核 exe 新鲜度）
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestFocusOutE2E.vcxproj' `
  /p:Configuration=Release /p:Platform=x64 `
  '/p:SolutionDir=F:\soft\00selfmade\rime_claude' /nologo /v:minimal

# 2) 跑探针（一次运行 ≈ 2–3 min，含闸门观测；脚本自带 TOOL_STALE 闸门）
pwsh -NoProfile -ExecutionPolicy Bypass `
  -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b5-legacy-client-e2e.ps1'
```

退出码：`0`=PASS_E2E / `4`=FAIL / `5`=ABORT（环境不可用）/ `6`=INCONCLUSIVE（没观测到任何回收 ⇒ 方法在本机无牙）。

证据文件：`console.txt`（全程）、`verdict.txt`、`summary.txt`、`log-slice.txt`、`evidence-positive.txt`、`evidence-negative.txt`、`*.marker`、`*.stdout.txt`。

## 9. 本轮工具坑

1. **`write` 大脚本会产生跨行拼接的损坏语句**：本轮写出 `$slice5 = Get-LogSlice -From $slice5baselineB = $null`（把两行黏成一句），**在 pwsh 里靠运行时才炸**。⇒ 长脚本落盘后必须先 `[System.Management.Automation.Language.Parser]::ParseFile()` 核 `PARSE_ERRORS=0`，再跑（本轮 PARSE_ERRORS=0 后确认纯 ASCII、无 BOM、LF）。
2. **`pwsh` 不能用 WindowsApps 的打包绝对路径**调（`C:\Program Files\WindowsApps\Microsoft.PowerShell_7.5.4.0_x64__...\pwsh.exe` 在该 shell 里不存在 ⇒ 命令直接 not-recognized）。⇒ 用 PATH 上的 `pwsh`。
3. **`ECHO=0` 不是「一定会话表里没这条目」的单义判据**：`--echo` 走 `WEASEL_IPC_ECHO`，返回 0 既可能是 weasel 会话表条目没了，也可能是 librime 会话对象没了。本轮它作为**第二证据通道**用（与日志行计数交叉），单独不足以定案。
4. **清表路径零日志** ⇒ 「某会话什么时候、被哪条路径拿掉」在诊断日志里查不到，只能用代码位置 + 磁盘导出文件 mtime 夹逼。要判「旧客户端会话的收场」，必须额外去看 `Start/EndMaintenance`，不能只看日志。
5. `m_session_status_map` 的 `remove_session active presence=absent` 行**不带 ipc_id** ⇒ 无法按 id 归因，只能按时间戳与自己的动作序列对齐。
