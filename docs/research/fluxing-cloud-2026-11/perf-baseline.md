# perf-baseline.md — 性能三数字与输入延迟（T602）

> 批次 AN 初测（2026-11-11）· **批次 AP 归因闭环（2026-11-12）** · 口径来源 [ADR 0030](../../adr/0030-continue-fluxing-fork-no-rewrite-perf-baseline.md) 决策 2，**口径修订见 [ADR 0060](../../adr/0060-idle-cpu-single-core-window-and-attribution-method.md)**
> 平台：Windows 10.0.26300 x64 · 20 逻辑核 · 装机目录 `D:\Program Files\fluxing\weasel`

## 1. 口径（ADR 0030 决策 2 + ADR 0060 修订）

| 指标 | 基线 | 判据 | 测量命令 |
|---|---|---|---|
| 常驻进程数 | 1 | 仅 `WeaselServer.exe` 常驻；`WeaselTSF.dll` 注入宿主不计 | `Get-Process WeaselServer` |
| Server 内存 | Private Working Set **< 40 MB**（**仅对应「无 TSF 会话」稳态**） | — | `Get-Process WeaselServer \| Measure PrivateMemorySize64` |
| 空闲 CPU | **< 0.1%（占单核）** | 静止 **≥ 16 s**（ADR 0060），两次 `TotalProcessorTime` 增量 / 窗口墙钟秒数，**不除逻辑核数** | `Get-Process WeaselServer` 两次采样求差 |

- ADR 0060 修订要点：① 空闲 CPU 统一「占单核」口径（与搜狗对照值 0.78 % 同口径），阈值 **< 0.1 %** 不放宽（比搜狗严 7.8 倍）；② 单核口径下窗口下限 **16 s**；③ 性能归因必须有 **同机 / 同用户数据 / 同脚本 / 同窗口 A/B 对照**，且对照件须先经**字符串/符号判别**证明不含该模块，否则只能登记「不可归因」。
- **ADR 0030 的 40 MB 内存基线对应「无 TSF 会话、方案未加载」稳态**；带 TSF 会话时 librime 方案 / 词库常驻属另一量级，两者**不可直接比较**（本轮坐实，见 §2.2）。

对照物：搜狗输入法 6 进程 / 合计 private 183.4 MB / 空闲 8 s 采样 CPU 0.78%（单核）。

## 2. 实测数字

### 2.1 AN 批次原始读数（装机版 · 带 TSF 会话 · 未登录）

采样对象：装机版 `WeaselServer.exe` **PID 257348**，启动 `2026-11-11 15:45:17`。

| 指标 | 实测 | 基线 | 结论 |
|---|---|---|---|
| 常驻进程数 | **1** | 1 | ✅ 未破线 |
| 私有内存（PrivateMemorySize64） | **136.0 MB**（5 s）／**138.5 MB**（10 s） | < 40 MB | ❌ 破线 3.4×（**口径错配**，见 §2.2） |
| 工作集（WorkingSet64） | 200.6 / 204.8 MB | — | 参考值 |
| 空闲 CPU · 逻辑核折算 | **0.039 %** | — | （口径已被 ADR 0060 废止） |
| 空闲 CPU · 单核口径 | **0.7804 %** | < 0.1 % | ⚠️ 见 §3 裁定 |
| 句柄数 / 线程数 | 878 / 40 | — | 参考值 |

### 2.2 归因闭环（批次 AP）—— 单变量 A/B 对照

**对照件甄别**（字符串/符号判别，标记 `/v1/sync/push`、`/v1/auth/login`、`CloudSyncEngine`、`fluxing-itemkey-mac-v1`、`CloudAccountDialog`、`/v1/sync/pull`）：

- **真 pre-cloud 对照件** = 装机目录内 `WeaselServer.exe.bakL`（2 337 280 B，2026-11-08 16:50:54）—— 6 个标记**全部 False**；
- ⚠️ `output/archives/fluxing-1.2.1-installer.exe` **已被证伪含云**（含 `/v1/sync/push` 等）⇒ **不可**作对照；
- 装机目录另有 7 个 pre-cloud 候选（`bak-20261103-*`、`bak-20261107-qpv4*`、`.bakL`），含云件为 `WeaselServer.exe` 与 `old-261111-*`。

**对照组构造**：`tmp\AP-ctl-run4` = 装机目录**全量递归拷贝**（**459 文件 / 458.1 MB**，含 `data\rime_ice.schema.yaml`、`data\essay.txt` 6 330 573 B、`data\build\`），**仅**替换 `WeaselServer.exe` = `bakL`。
（v3 教训：只拷 3 个文件 ⇒ 缺 67 个依赖、进程 1.8 MB / 1 线程；缺 `data` ⇒ `missing input schema: rime_ice` 降级态。）

**对照组有效性核验**（`tmp/AP-logcheck.txt`）：B 组日志中 `rime_ice` / `melt_eng` / `radical_pinyin` / `double_pinyin_sogou` **四个方案全部 `dictionary '…' is ready`**，`missing input schema` **0 命中**、`Error reading file information` **0 命中** ⇒ B 组**非降级态**，对照有效。

**v4 A/B 实测**（`tmp/AP-ctrl4.txt`；无 TSF 会话态、未登录）：

| 组 | 峰值 PrivMB | 终态（MB / 线程 / 句柄） | 窗口 | 单核 CPU | ΔCPU / 节拍 |
|---|---|---|---|---|---|
| A 含云（装机目录） | **24.8** | 24.8 / WS 36.3 / Thr 22 / Hd 329 | 25 s | 0.1409 % | 31.2 ms / 2 |
| A 含云 | 同上 | 同上 | 26.2 s | **0.1192 %** | 31.2 ms / 2 |
| B pre-cloud（含 data） | **26.3** | 26.1 / WS 37.8 / Thr 18 / Hd 327 | 25 s | 0.2111 % | 46.9 ms / 3 |
| B pre-cloud | 同上 | 同上 | 26.2 s | 0.1786 % | 46.9 ms / 3 |
| B pre-cloud | 同上 | 同上 | 56.5 s | **0.1106 %** | 62.5 ms / 4 |

⇒ **云模块常驻内存开销 ≈ 0 MB**（B 组比 A 组**还高 1.5 MB**，且 B 处于更重的「工作区重建」态）。
⇒ **空闲 CPU 同窗口下 pre-cloud 反而更高**（26.2 s：0.1786 % vs 0.1192 %）⇒ CPU 亦**不归因于云模块**。

### 2.3 138.5 MB 的真正归属（会话证据）

`tmp/AP-session-hist.txt`：AN 采样窗口（启动 `2026-11-11 15:45:17` → `16:10:50`）内 **48 次 `focus_in`**；全日志累计 `focus_in` 2092 / `focus_out` 854 / `remove_session` 416 / `startup_allowed` 147 / `own_ui_session` 114。
而 2026-11-12 00:27–00:37 的**全部对照测量期间只有 `startup_allowed`、0 次 `focus_in`** ⇒ AN 的 138.5 MB 属「**TSF 会话 + librime 方案 / 词库加载**」态，与云同步模块无关。

### 2.4 CPU 量化地板（为什么 8 s 窗口判不了 < 0.1 %）

`TotalProcessorTime` 步长 **15.625 ms**（100 Hz）⇒ 8 s 窗口读数地板 **0.195 %**、10 s **0.156 %**、16 s **0.098 %** ⇒ ADR 0030 原「≥ 8 s」方法在单核口径下**原理上无法**判定 < 0.1 % ⇒ ADR 0060 将窗口下限提到 16 s。

### 2.5 A/B 复现命令

```powershell
# A 组（装机目录，含云） / B 组（tmp\AP-ctl-run4，pre-cloud 全量拷贝）
pwsh -NoProfile -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\tmp\AP-ctrl4.ps1
#   ⇒ tmp\AP-ctrl4.txt
# 对照组有效性核验（missing input schema / Error reading file information 必须 0 命中）
pwsh -NoProfile -ExecutionPolicy Bypass -File F:\soft\00selfmade\rime_claude\tmp\AP-logcheck.ps1
#   ⇒ tmp\AP-logcheck.txt

# 单核口径 CPU 采样（ADR 0060：窗口 ≥ 16 s）
$p = Get-Process WeaselServer
$c0 = $p.TotalProcessorTime.TotalSeconds; $t0 = Get-Date
Start-Sleep -Seconds 30
$p.Refresh(); $c1 = $p.TotalProcessorTime.TotalSeconds
$dt = ((Get-Date) - $t0).TotalSeconds
"单核={0:N4}% ΔCPU={1:N1}ms 节拍≈{2:N2}" -f ((($c1-$c0)/$dt)*100), (($c1-$c0)*1000), (($c1-$c0)*1000/15.625)
```

> ⚠️ **测量纪律**：空闲 CPU 采样期间**不得并发跑构建**（会污染读数）；A/B 脚本内**不得**用 `-LiteralPath` 配通配符（不展开）。

## 3. 口径歧义 —— 已裁定（ADR 0060）

ADR 0030 判据栏写「占单核」、做法栏写「逻辑核折算」，两者互斥（本机 20 核 ⇒ 相差 20 倍）。**裁定：统一为「占单核」**（与搜狗对照值同口径，阈值 < 0.1 % 不下调），窗口下限 16 s，并新增「归因必须 A/B 对照」两条硬规则。

## 4. 同步开启 / 关闭的输入延迟对照

**未闭环（需用户在场）**。原因与可测替代：

1. 「输入延迟」必须由真实打字链路测量（IME 自绘窗跨进程抓不到，需要人工或专用宿主），自动化拿不到判据；
2. 当前客户端处于**未登录**状态 ⇒ 同步引擎根本未启动（AC-12 实测 0 出向请求），「同步开启」这一组需要登录态 + 真实账号（`25940969@qq.com` 的 6 位码尚未回贴）。

结构性证据（**不等价于实测**，仅说明代码路径上同步不占用输入线程）：

- `CloudSyncEngine` 跑在**独立后台线程**，节流 / 退避 / 推拉全在该线程；
- `RimeWithWeaselHandler::NotifyLocalChange()` 只**置标志 + 唤醒条件变量**，不发网络请求；
- `CloudSyncService::SyncOnce` 的维护态包裹 `StartMaintenance` / `EndMaintenance`，写入在引擎后台线程完成；
- 推拉走 `WinHttpTransport`（WinHTTP），不在 `ITfTextInputProcessor::*` 回调内发起 ⇒ 符合 constitution P2「TSF 回调内禁阻塞 I/O」。

## 5. 结论

- ✅ 进程数未破线（1）；
- ✅ **私有内存不归因于云同步**：同机 / 同用户数据 / 同脚本 / 同窗口 A/B 下，云模块常驻开销 **≈ 0 MB**（pre-cloud 26.3 MB vs 含云 24.8 MB）；
- ✅ **空闲 CPU 不归因于云同步**：同窗口 pre-cloud 0.1786 % vs 含云 0.1192 %（56.5 s 窗口 pre-cloud 0.1106 % vs 含云 0.1192 %）；
- ⚠️ **ADR 0030 的 40 MB 内存基线与 138.5 MB 属口径错配**（前者「无会话稳态」≈ 25 MB，后者「TSF 会话 + 方案 / 词库加载」态）⇒ **不是云同步回归**，而是**基线定义未覆盖会话态**；
- ⚠️ 无会话态 CPU 读数 A 0.1192 % / B 0.1106 % 均略高于 0.1 % 线，但 pre-cloud 同样如此 ⇒ 非云同步引入；该阈值需补「测量态」定义（静置到方案加载完成 + 存在会话）；
- ⏳ 输入延迟两组对照 **未闭环**（需登录态 + 用户在场）。

## 6. 遗留

1. ~~ADR 0030 决策 2 修订~~ ⇒ **✅ 已完成**（批次 AP，[ADR 0060](../../adr/0060-idle-cpu-single-core-window-and-attribution-method.md)）；
2. ~~内存归因（需不含云模块的对照构建）~~ ⇒ **✅ 已完成**（批次 AP，单变量 A/B；结论：云模块 ≈ 0 MB）；
3. **ADR 0030 内存基线口径补正**：40 MB 应显式限定为「无 TSF 会话稳态」并补一条会话态参考值（约 135–140 MB），否则后续批次会重复踩同一口径坑；
4. **「带 TSF 会话」的 A/B 差值仍未实测** —— 自动化无法建立 TSF 会话（Notepad 启动成功但**不产生 `focus_in`**，Threads 21→18 说明未 attach）⇒ 需真实 TSF 宿主 + 用户在场；
5. **严格同状态 A/B**：本轮 B 组处于「工作区重建」态、A 组为稳态，方向性结论（pre-cloud 不低于含云）已成立；若要升级为「严格同状态 ≈ 0」，需在 `tmp\AP-ctl-run4\data\build` 稳定后复跑一次 B（**须在构建任务之外**）；
6. **含云构建无会话态约 30.2 s 自退**（v3/v4 两次一致；pre-cloud 同条件存活 ≥ 70 s），原因未查明，已登记；
7. 输入延迟两组对照（需登录态 + 用户在场）。
