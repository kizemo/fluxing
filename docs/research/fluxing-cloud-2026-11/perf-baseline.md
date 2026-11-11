# perf-baseline.md — 性能三数字与输入延迟（T602）

> 批次 AN · 2026-11-11 实测 · 口径来源 [ADR 0030](../../adr/0030-continue-fluxing-fork-no-rewrite-perf-baseline.md) 决策 2
> 平台：Windows 10.0.26300 x64 · 20 逻辑核 · 装机版 `WeaselServer.exe`（`D:\Program Files\fluxing\weasel`）

## 1. 口径（ADR 0030 决策 2 原表）

| 指标 | 基线 | 判据 | 测量命令 |
|---|---|---|---|
| 常驻进程数 | 1 | 仅 `WeaselServer.exe` 常驻；`WeaselTSF.dll` 注入宿主不计 | `Get-Process WeaselServer` |
| Server 内存 | Private Working Set **< 40 MB** | — | `Get-Process WeaselServer \| Measure PrivateMemorySize64` |
| 空闲 CPU | **< 0.1%** | 静止 ≥ 8 s，两次 `CPU` 增量 / 窗口秒数 / **逻辑核折算** | `Get-Process WeaselServer` 两次采样求差 |

对照物：搜狗输入法 6 进程 / 合计 private 183.4 MB / 空闲 8 s 采样 CPU 0.78%（单核）。

## 2. 实测数字

采样对象：`WeaselServer.exe` **PID 257348**，进程启动时刻 `2026-11-11 15:45:17`（装机版）。
采样窗口内**无**编译 / 无同步任务 / 未登录账号。

| 指标 | 实测 | 基线 | 结论 |
|---|---|---|---|
| 常驻进程数 | **1** | 1 | ✅ 未破线 |
| 私有内存（PrivateMemorySize64） | **136.0 MB**（5 s 窗口采样）／**138.5 MB**（10 s 窗口采样） | < 40 MB | ❌ **破线（3.4×）** |
| 工作集（WorkingSet64） | 200.6 MB / 204.8 MB | — | 参考值 |
| 空闲 CPU · 逻辑核折算 | **0.039 %** | < 0.1 % | ✅ 未破线 |
| 空闲 CPU · 单核口径 | **0.7804 %** | < 0.1 % | ⚠️ **口径歧义**，见 §3 |
| 句柄数 / 线程数 | 878 / 40 | — | 参考值 |

原始命令（可复跑）：

```powershell
$p = Get-Process WeaselServer
$c0 = $p.TotalProcessorTime.TotalSeconds; $t0 = Get-Date
Start-Sleep -Seconds 10            # ADR 要求 ≥ 8 s
$p.Refresh(); $c1 = $p.TotalProcessorTime.TotalSeconds
$dt = ((Get-Date) - $t0).TotalSeconds
"PrivateMB={0} CPUpct_1core={1} CPUpct_logicalnorm={2}" -f `
  [math]::Round($p.PrivateMemorySize64/1MB,1), `
  [math]::Round((($c1-$c0)/$dt)*100,4), `
  [math]::Round((($c1-$c0)/$dt)/[Environment]::ProcessorCount*100,4)
```

## 3. 口径歧义（需 ADR 修订裁定，不在本轮擅改）

ADR 0030 的判据栏写「空闲 CPU < 0.1%（**占单核**）」，而「做法」栏写「两次 `CPU` 增量 / 窗口秒数 / **逻辑核折算**」—— 两者互斥：

- 按**逻辑核折算**（做法栏）：本机 0.039 % ⇒ **未破线**；
- 按**占单核**（判据栏，搜狗对照值 0.78 % 也是这个口径）：0.7804 % ⇒ **破线**。

本轮按「逻辑核折算」记为未破线，但**口径必须由一次 ADR 修订裁定**（见 §6 遗留 ①）。注意本机 20 逻辑核，两口径相差 20 倍。

## 4. 同步开启 / 关闭的输入延迟对照

**未闭环（需用户在场）**。原因与可测替代：

1. 「输入延迟」必须由真实打字链路测量（IME 自绘窗跨进程抓不到，需要人工或专用宿主），自动化拿不到判据；
2. 当前客户端处于**未登录**状态 ⇒ 同步引擎根本未启动（AC-12 实测 0 出向请求），「同步开启」这一组需要登录态 + 真实账号（`25940969@qq.com` 的 6 位码尚未回贴）。

结构性证据（**不等价于实测**，仅说明代码路径上同步不占用输入线程）：

- `CloudSyncEngine` 跑在**独立后台线程**，节流/退避/推拉全在该线程；
- `RimeWithWeaselHandler::NotifyLocalChange()` 只**置标志 + 唤醒条件变量**，不发网络请求；
- `CloudSyncService::SyncOnce` 的维护态包裹 `StartMaintenance` / `EndMaintenance`，写入在引擎后台线程完成；
- 推拉走 `WinHttpTransport`（WinHTTP），不在 `ITfTextInputProcessor::*` 回调内发起 ⇒ 符合 constitution P2「TSF 回调内禁阻塞 I/O」。

## 5. 结论

- ✅ 进程数未破线；
- ❌ **私有内存破线**：138.5 MB ≫ 40 MB 基线 ⇒ **AC-11 不通过**；
- ⚠️ 空闲 CPU 按「逻辑核折算」未破线，**按「占单核」破线**，口径待裁定；
- ⏳ 输入延迟两组对照 **未闭环**（需登录态 + 用户在场）。

## 6. 遗留（本批未做，需下一批）

1. **ADR 0030 决策 2 修订**：裁定空闲 CPU 口径（建议统一为「占单核」，与搜狗对照值同口径，否则基线不可比）；
2. **内存归因**（发布阻塞项）：需用 VMMap / 堆快照拆解 138.5 MB 的归属 —— 重点区分「librime 词库 / 方案常驻」与「spec 100 新增云同步模块」。**注意**：本轮**没有** spec 100 之前的对照基线（装机版已是含云模块的构建），因此**不能**断言「内存增长由云同步造成」；下批应构建一个不含云模块的对照版本量同一口径；
3. 若归因落在云模块 ⇒ 评估 `CloudSyncEngine` 是否可在**未登录**时不构造（当前按 AG 批次要求在 `WeaselServerApp` 构造期即构造控制器，以保证「不依赖用户是否打开过账号页」）；
4. 输入延迟两组对照（需登录态 + 用户在场）。
