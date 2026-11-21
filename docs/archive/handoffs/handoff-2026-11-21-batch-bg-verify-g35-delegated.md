# 批次 BG · G-3/G-5 维护闸门造态能力与取证脚本（孤儿会话回收反向判据）

> 日期：2026-11-21
> 本批性质：**取证 + 造态能力建设**（**产品代码零改动**）
> 明确未做：**未改 `librime/`、未改 IPC 协议消息类型、未新增依赖、未 bump 版本号、未改 `WeaselServer/**`、未改 `include/**`、未改任何 `.rc` / `install.nsi` / `version.txt` / `.vcxproj` / `weasel.props`**
> 结论口径：**可行性 = CONDITIONAL YES**；**执行判定 = `BLOCKED_HARNESS_PRECONDITION`**（环境前置不可得 ⇒ **非缺陷、非 PASS、也非 FAIL**）
> ⚠️ **BG 自身的实验从未真跑**（G-3 / G-5 真机端到端未做）。本批交付的是「让 G-3/G-5 在真机上**可被观测**」的 harness + 配方，**不是** G-3/G-5 已闭环的结论。
> ⚠️ **本文件为事后重建**：原 handoff 被同工作区另一会话用 `Copy-Item` 还原抹掉，且从未进入 git 历史（`git log --all -- 'docs/archive/handoffs/*bg*'` 0 命中、`maintenance/` 0 命中）⇒ 重建来源见 §10。

---

## 1. 批次目标与背景

G-3 / G-5 是「缺陷 A（开启输入法 ≠ 正在输入）」的**反向判据**，长期**未自动验证**：

- `docs/release/regression-checklist-cloud-sync.md` §1（AC-G 组）：G-3 = 「激活后 10 s 内有按键 ⇒ 该轮**应被跳过**」，判定方式 = 导出文件 mtime 不动；G-5 = 「按键后 ≥10 s 且无组合串 ⇒ 该轮**放行**」，判定方式 = 导出文件 mtime 前进。两行状态列均登记为 ⚠️ **未自动验证**。
- 同文件 §5「待补的自动化判据」：G-3 / G-5 自动验证需要制造「真实输入历史」与「未上屏组合串」，且必须在**副本**用户数据目录 + 独立云账号下做。
- `docs/release/restart-checklist-1.3.6.md` §4（原文）：**「闸门是否放行 = 维护导出文件 mtime 是否前进（状态文案不可作判据）」**。

BG 的目标 = **不改产品码**，把「维护闸门放行与否」从「等真实态出现」变成「**主动造出等价的态**」，从而让 G-3 / G-5 在真机上可被观测。

---

## 2. 结论口径（**必须这样读，勿改写**）

| 口径 | 值 |
|---|---|
| 可行性 | **CONDITIONAL YES** |
| 执行判定 | **`BLOCKED_HARNESS_PRECONDITION`**（环境前置不可得 ⇒ **非缺陷、非 PASS、也非 FAIL**） |
| BG 自身实验 | **从未真跑**（脚本真实模式未跑；只跑过 `-DryRun` 与 harness 独立自检） |

**为什么是「条件可行」而不是「已通过」**：BG 的原始实验要等一个真实出现的机会态（**旧端客户端 + 已老化的导出/备份 mtime**），本机不具备（单机、第二台设备离线、服务端 1.3.4.0→1.3.8.0 升级链从未真实发生）。BG 的做法是**换一条路**：造出等价的态 ⇒ 条件可行；但**造态路线本身也从未真跑过**（BG 批次内禁止真同步），所以两条路线都**不能**写成 PASS。

---

## 3. 闸门判据与唯一可观测面

### 3.1 判据代码位置（**唯一真相源**）

`include/FluxingMaintenanceGate.h:67-84`（ADR 0065）：

```
allows = live_sessions <= 0
      || !has_input_history
      || (composing_sessions > 0 ? idle_ms > 60000 : idle_ms > 10000)
```

- 常量：`kMaintenanceIdleGraceMs = 10*1000`（同文件 **L49**）、`kMaintenanceCompositionGraceMs = 60*1000`（同文件 **L50**）。
- 关键点：`composing == 0` 时闸门只看 **idle > 10 s**；`composing > 0` 时要看 **idle > 60 s**。
- **这正是上一批 `--hold`（组合串一直留着）造不出 G-5 的根因** ⇒ 本批必须新增 `--hold-commit`。

### 3.2 唯一可用判据 = **导出文件 mtime 是否前进**

服务端**没有**「闸门判定结果」的直接可观测点（逐条 file:line 见 `reports/fluxing-bg-g35/harness-report.md` §5）：

| 候选观测点 | 结论 |
|---|---|
| ASCII 原因令牌 `live_input_session` | ❌ 运行时不可观测（令牌定义在 `include/FluxingMaintenanceGate.h:38-40`，但 `CloudSyncService.cpp:878-884` **丢弃** `gate_reason`，换成通用中文文案 `MaintenanceGateSkipMessage()`；全仓无处写进日志 / UI / IPC） |
| 控件 `kIdSyncStatus = 1017` | ❌ 不可作判据（`CloudAccountDialog.cpp` 全文只出现在 `:955-956` 的静态字面量；`RefreshUi`（`:1299-1352`）只写 `kIdStatus`（`:1311-1313`），**从不重写 1017**） |
| 控件 `kIdStatus = 1006` | ⚠️ 只能作**提示**，不能作判据 |
| `CloudSyncStatus` / `Impl::sync_status_text` | ❌ C++ 内部 API，不暴露给脚本 |
| 诊断日志 `%TEMP%\fluxing-profile-notify.log` | ❌ 无闸门记录 |
| **导出文件 mtime** | ✅ **唯一可用的判据** |

⇒ **判据只用 mtime；状态文案只进日志，不进判据。**（这条与 BF 否证「枚举不到控件即通过」是同一种教训：**没有可观测点 ⇒ 必须承认只能间接判定，并在报告里写明间接链路的每一环。**）

---

## 4. 造态四步与自证链

| 步 | 动作 | 自证字段 | 依据 |
|---|---|---|---|
| 1 | `START_SESSION` | `ipc_id`（≠0） | `WeaselIPCServer/WeaselServerImpl.cpp:265-276` |
| 2 | 打 13 个字母 `"fluxingorphan"` | `COMPOSING_AFTER_TYPING=1`（响应体 `status.composing=`） | `RimeWithWeasel.cpp:2055-2063` |
| 3 | 发 `WEASEL_IPC_COMMIT_COMPOSITION` = **0x800B** | `COMMIT_SENT=1`（事务返回 result） | `include/WeaselIPC.h:30`；`RimeWithWeasel.cpp:762-773` |
| 4 | 发一记刷新键 `ibus::Left` = **0xFF51** | `COMPOSING_AFTER_COMMIT=0`（响应体 `status.composing=0`） | `RimeWithWeasel.cpp:740` `_SyncSessionComposing` |

- **提交为什么用 IPC 命令而不是「猜一个键」**：`WEASEL_IPC_COMMIT_COMPOSITION` 是确定性的、与 schema 无关；`Return` / `Escape` 的键语义随 schema / 输入法模式变化。事务配平：`HandlePipeMessage` 末尾无条件 `resp(result)`（`WeaselServerImpl.cpp:544`）⇒ 即使该命令不写 body，客户端仍会收到 4 字节 result，不会挂死。
- **刷新键为什么选 `ibus::Left`**：必须同时满足「刷新最近一次输入」+「不产生组合串」。`Left`（`ch=0xFF51 > 0x7f`）不被 `editor.cc:58-62` 的 char handler 收、`ctx->IsComposing()` 为假时不查 key binding（`librime/src/rime/gear/editor.cc:46-66`）；`Shift` 会在 500 ms 内 down+up 切换 ascii 模式、`Escape` 会清组合串（假绿）、数字键中文模式下可能被 translator 收下 ⇒ 三者禁用。运行时兜底：刷新键每发一次都读一次 `status.composing=`，**一旦出现 1 立即以退出码 3 失败**（`a composition reappeared in the steady state`）。
- **自证为什么可信**：响应体里的 `status.composing=` 由 `rime_api->get_status()` 产生，而闸门读的 `m_composing_session_count` 也正是由同一个 `get_status` 结果同步而来（`RimeWithWeasel.cpp:349-359` `_SyncSessionComposing` → `:338-345` `_SetSessionComposing` → `:328-335` `_RefreshComposingSessionCount`）。**不是「推断组合串没了」，是「服务端自己说组合串没了」**，且不依赖任何状态文案。
- **产物文件**：`<marker>`（一次写：`ipc_id` / `pid` / `phase` / `composing_after_commit` / `last_input_tick_ms`）、`<marker>.state`（每次刷新重写：`last_input_tick_ms` / `composing`）、`<marker>.pause`（存在时 `active` 相位停止刷新 ⇒ **一个进程即可承载 G-3 / G-5 两个相位**）。
- **全程不发 END_SESSION**：会话必须**驻留**（`live_sessions > 0` 才有闸门可言）；harness 被杀时服务端才走 ADR 0067 的孤儿回收路径。

### 4.1 idle 的测量口径（保守）

- harness 用 `GetTickCount64()` 记录**最后一记键的发送时刻**写进 `<marker>.state` 的 `last_input_tick_ms`；脚本用 `[Environment]::TickCount64`（同一系统时钟）算差值。
- 脚本侧取**保守下界**：客户端记录的发送时刻 ≤ 服务端实际处理时刻 ⇒ **我方测得的 idle ≤ 真实 idle**。
  - **G-3 判「idle < 10 s」**：下界 < 10 s ⇒ 真实更小 ⇒ **天然保守**。
  - **G-5 判「idle ≥ 12 s」**：留 2 s 余量覆盖 IPC 往返与调度抖动。
- `active` 相位刷新周期 1500 ms ⇒ 稳态 idle ≈ 1.5 s，距 10 s 门槛有 ~8.5 s 余量。

---

## 5. 已验证 / 推断 / 未验证（三档）

### 5.1 verified（本轮亲自跑过，或有直接源码 / 二进制证据）

| # | 断言 | 证据 |
|---|---|---|
| V1 | `--hold-commit` 模式可运行，四段自证输出齐全，退出码 0 | 实跑输出 |
| V2 | 打完字**真的**有未上屏组合串（`status.composing=1`） | `COMPOSING_AFTER_TYPING=1` |
| V3 | IPC 提交（0x800B）**真的**清掉了组合串，且由服务端响应体自证 | `COMPOSING_AFTER_COMMIT=0` + `RimeWithWeasel.cpp:762-773` |
| V4 | 刷新键 `ibus::Left` 不产生组合串 | 源码 `editor.cc:46-66` + 运行时 composing 恒 0 |
| V5 | 无 body 的命令（COMMIT_COMPOSITION）仍回 DWORD，事务不会挂死 | `WeaselServerImpl.cpp:544`（`resp(result)` 无条件）+ 事务正常完成 |
| V6 | 装机版 1.3.8.0 服务端二进制含 `status.composing=` ⇒ 自证通道真机可用 | 偏移 2 140 528 |
| V7 | 编译成功且产物是新的 | `MSBUILD_EXIT=0`；exe mtime 晚于源码 49.96 s |
| V8 | 单工程构建必须传 `/p:SolutionDir=` | 实测失败 → 加参数后成功；与 `build.bat:339-342` 一致 |
| V9 | 闸门判据的精确形式与常量 10 s / 60 s | `include/FluxingMaintenanceGate.h:49-50,67-84` |
| V10 | 「任何键都刷新 `m_last_input_tick`」 | `RimeWithWeasel.cpp:713-717` |
| V11 | 服务端**没有**闸门判定的直接可观测点 | §3.2 逐条 file:line |
| V12 | 跳过的轮次**不进**失败退避；手动轮次优先级最高 ⇒ G-5 可紧接 G-3 | `CloudSyncEngine.cpp:340-350`；`CloudAccountDialog.cpp:811-814` |
| V13 | `kMaintenanceSkipRetryIntervalMs` 在 `WeaselServer/` 内**零引用**（只在头文件定义） | 全目录 grep 无匹配 |
| V14 | 脚本纯 ASCII、`PARSE_ERRORS=0`、`-DryRun` 退出 0 且未触碰服务端 | 脚本规格 + `-DryRun` 实跑 |
| V15 | `.cpp` 保持纯 ASCII + LF | 逐字节扫描 |

### 5.2 inferred（源码推理 + 局部证据；**真机结果本轮未跑**）

| # | 断言 | 缺什么 |
|---|---|---|
| I1 | `active` 相位（idle < 10 s + `composing==0`）⇒ 闸门关闭 ⇒ 导出文件不前进 | 端到端未跑（脚本真实模式） |
| I2 | `idle` 相位（idle ≥ 12 s + `composing==0`）⇒ 闸门放行 ⇒ 导出文件前进 | 同上 |
| I3 | 同一会话 + `.pause` 切换可承载 G-3 / G-5 的单变量对比 | 同上 |
| I4 | 静止性观察（20 s）足以识别自发轮次 | 引擎自发轮次周期未取证 |
| I5 | 1006 在手动轮次后会刷新出「跳过」文案 | `RefreshUi` 在手动轮次后的触发时机未验证 ⇒ 脚本里只作提示 |
| I6 | 提交用 IPC 比用 `Return` 键更稳 | 未实测 `Return` 路径 |

### 5.3 not verified（本批明确**没做**，不得写成已验）

| # | 项 | 原因 |
|---|---|---|
| N1 | **G-3 真机端到端**（导出文件不前进） | 本批禁止真同步；脚本真实模式未跑 |
| N2 | **G-5 真机端到端**（导出文件前进） | 同上 |
| N3 | 控制轮 C（导出文件前进） | 同上 |
| N4 | G-3 / G-5 **完整**闭环 | 还需双机 AC 7 条、restore 端到端、输入延迟对照（需两台机器 + 用户在场） |
| N5 | 真实 IME 宿主同时打字对全局 `m_last_input_tick` 的污染 | 无直接观测点；脚本靠闸门端到端结果兜底 |
| N6 | 175% / 200% DPI 下的同一判据 | 用户已拍板不主动切档 |
| N7 | `librime` translator 层对方向键的逐一行为 | `translator.cc` 实现文件未定位；已由 editor 层源码 + 运行时 composing 恒 0 兜底 |
| N8 | 「闸门判定结果写入诊断日志」 | **需要改产品代码**，本批明确不做 |

---

## 6. 交付物清单（绝对路径）

| # | 绝对路径 | 字节 | 行 | 性质 |
|---|---|---|---|---|
| A | `F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.cpp` | 29 325 | 740 | **改**（测试工程，非产品码） |
| B | `F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe` | 181 248 | — | 构建产物（新编，mtime 晚于源码 49.96 s） |
| D | `F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1` | 25 706 | 640 | **新建**（纯 ASCII、LF、`PARSE_ERRORS=0`） |
| E | `F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\harness-report.md` | 26 173 | 402 | **新建**（权威取证报告，**已入库**） |
| F | `F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\NOTES-sync-status-not-observable.md` | — | 43 | **新建**（1017 不可观测取证） |
| G | `F:\soft\00selfmade\rime_claude\reports\fluxing-bg-prep\*.txt` | — | — | **新建**（端点可达性 / 凭据 / 登录态预检原始输出） |
| H | `F:\soft\00selfmade\rime_claude\maintenance\BG-G35-conditional-feasibility.md` | — | — | **新建**（可执行配方，本批重建） |
| — | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bg-verify-g35-delegated.md` | — | — | **本文件**（重建） |

- A / D / E / F / G 已随提交 **`e85259c` `test(ci): 批次 BG · G-3/G-5 维护闸门造态能力与取证脚本`** 入库（12 files changed, 1752 insertions(+), 1 deletion(-)）。
- **H（配方）与本文件（handoff）当时未入库** ⇒ 本次重建后仍需单独提交（见 §11）。

---

## 7. 相位编排与脚本用法

### 7.1 三个相位

| 相位 | 合成会话状态 | 期望 | 判据 |
|---|---|---|---|
| **C**（control） | 无合成会话 | 本轮**不被跳过** | 导出文件 mtime **必须前进**（否则整轮 ABORT：没有正控制，G-3/G-5 无意义） |
| **G3**（负向） | 同一会话，`active` 相位（idle < 10 s）、`composing == 0`、输入历史存在 | 闸门**保持关闭** | 导出文件 mtime **必须不前进**（观察窗 = `ceil(控制轮耗时)+15 s`，钳在 15–60 s） |
| **G5**（正向） | **同一会话**，`active` + `<marker>.pause`（停刷新 ⇒ idle ≥ 12 s），`composing == 0` | 闸门**放行** | 导出文件 mtime **必须前进**（窗口 = `-RoundWaitSec`，默认 150 s） |

**G3 与 G5 只差一个变量（用户是否还在打字）⇒ 这一对就是 G-3 / G-5 的证明。**

### 7.2 三条设计决策

1. **一个会话 + `.pause` 切换，而不是两个会话。** `m_last_input_tick` 是**全局**量（`RimeWithWeasel.cpp:715-717`），任何会话的任何键都会刷新它；若 G3 用 `active` 会话、G5 用 `idle` 会话**并存**，那个 `active` 会话会不断刷新全局 tick ⇒ G5 的 idle 窗口永远不成立。（因此脚本统一用 `--hold-commit <marker> active`；harness 的 `idle` 相位仍保留，供独立使用与自检。）
2. **相位顺序 C → G3 → G5。** 自发轮次**不可能**造成 G-3 假阴性以外的误判（被闸门挡住的轮次不会导出）；但**可能**造成 G-5 假阳性（自发轮次自己导出了）⇒ 在触发 G-5 之前做**静止性观察**：停手后不触发任何东西，盯 20 s（`-QuiesceSec`），若导出文件自己前进 ⇒ 引擎不静止 ⇒ 判 **INCONCLUSIVE**（G-5 的前进无法归因），不做 PASS 也不做 FAIL。
3. **判据只用 mtime；文案只进日志。** 1006 的文本只写进 `console.txt` 与 `STATUS_AFTER_*` 行（含一个 `SKIPPED_HINT` 位），**不进判据**。

### 7.3 用法（可照抄）

```powershell
# 预演（不碰服务端、不开窗、不同步）
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All -DryRun

# 正式三相位（★ 会真的开账号窗并点「立即同步」，需要登录态 + 用户在场）
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All

# 单相位
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode C
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G3
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode G5

# 可调参数
pwsh -NoProfile -File F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1 -Mode All `
  -RoundWaitSec 150 -IdleWaitSec 15 -QuiesceSec 20
```

**退出码**：`0` = `VERDICT=GATE_G35_OK`；`4` = `VERDICT=GATE_G35_FAIL`；`5` = ABORT（环境不可用）；`6` = `VERDICT=GATE_G35_INCONCLUSIVE`。
**完整可执行配方（前置核对表 / 构建 / 造态 / 时序 / 排查分支 / 判读规则）见 `F:\soft\00selfmade\rime_claude\maintenance\BG-G35-conditional-feasibility.md`。**

---

## 8. 未闭环清单（**须如实登记，不得写成已闭环**）

| # | 项 | 阻塞原因 | 需什么 |
|---|---|---|---|
| 1 | **G-3 / G-5 真机端到端**（脚本真实模式） | BG 批次内**禁止真同步**（未触发任何云端同步：未开账号窗、未点「立即同步」、只跑了 `-DryRun`）；跑真实模式要求 (a) 账号已登录 (b) **跑的过程中不要在别的窗口打字**（`m_last_input_tick` 是全局量，别处打字会把 idle 拉回 0，G-5 会假红） | **用户在场** + 登录态 |
| 2 | **双机 AC 7 条**（AC-03/04/05/08/09/13/14） | `CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 两套 Profile 目录不够 | **第二台设备** |
| 3 | **restore 端到端** | 同上 | **第二台设备** |
| 4 | **输入延迟两组对照**（1.3.4 vs 1.3.5） | IME 自绘窗跨进程抓不到 ⇒ 用 PrintWindow / UIA | **第二台设备 + 用户在场** |
| 5 | **`kIdSyncStatus`(1017) 不可观测** | 同步结果在界面上不可见（详见 `reports/fluxing-bg-g35/NOTES-sync-status-not-observable.md`） | **待用户拍板**是否修（属独立 ADR 批次；最小修法 ≈ `CloudAccountDialog.cpp:1311` 附近 3 行） |

### 8.1 两条硬前置（BG 批次自登记 ⇒ 执行判定 = `BLOCKED_HARNESS_PRECONDITION`）

1. **旧端须用「未含 R3 守卫的 1.3.5 构建」**（**不是** 1.3.4 装机包）。
2. **须手工老化 `fluxing-rime-backup\profile.tar.gz` 的 mtime。**

- 这两条前置的原文出处：`docs/archive/handoffs/handoff-2026-11-21-batch-bi-c2d-fix-and-commit.md` §2 与 `CLAUDE.md` 的 BG 指针行（两者逐字一致）。
- **本机当前不可得**（2026-11-21 实测）：`D:\Program Files\fluxing` 下**不存在**任何 `*.tar.gz` / `backup` 命中 ⇒ 该路径形态只能在真实双机环境现场定位。
- ⚠️ **「R3 守卫」的具体所指未坐实**：本仓库现存文档中「守卫」只出现在 `docs/release/restart-checklist-1.3.7.md:62`（`install.nsi` 三段升级 + `IfErrors`/`Rename` 守卫）⇒ 最可能指升级链的第三段守卫，但**未证实**，登记为待定位。
- **与 harness 的关系（推断）**：harness 路线**换**掉了「等真实态出现」这条路（改为合成会话主动造态），因此**可能**绕开这两条前置；但**未被证实**，且 BG 的实验两条路线都没真跑 ⇒ 结论口径不变。

---

## 9. 工具坑

1. **单工程构建必须显式传 `$(SolutionDir)`**。第一次不带 `/p:SolutionDir` 直接构建 ⇒ 依赖工程 `WeaselUI` 报错 `WeaselUI\stdxf.h(15,10): error C1083: 无法打开包括文件: "wtl/atlapp.h"`、`MSBUILD_EXIT=1`。根因：各工程用 `$(SolutionDir)\include` 之类相对路径，而 `$(SolutionDir)` **只在构建 sln 时由解决方案注入**。仓库自己已有这条配方（`build.bat:339-342`）。`BOOST_ROOT` 已由 `weasel.props` 的 `UserMacros` 提供（`F:\b183`），无需额外设置。
2. **闸门放行与否只能看导出文件 mtime**；日志文本命中 / 版本号都不作数（`kIdSyncStatus` 1017 永远停在初始文案，详见 `reports/fluxing-bg-g35/NOTES-sync-status-not-observable.md`）。
3. **阴性判据必须配同 harness 的正控制**：没有相位 C 的「mtime 必须前进」，G-3 的「mtime 不前进」什么都不能证明。
4. **脚本必须保持纯 ASCII**：中文「跳过」两个字按 code unit（`U+8DF3 U+8FC7`）匹配，不得写中文字面量（脚本注释与文档都写明这一点）。
5. **工具自述不是证据**：产出任何正本文件必须同轮 `Test-Path` + 字节数 + 行数回读；`git diff | Select-String -SimpleMatch` 返 0 命中**不可信**，判「入库没有」只认 `git log -S <符号> -- <path>` + `git grep <符号> HEAD` + `ReadAllText.Contains`。
6. **并发写同一文件的工作区**：写完的同号文件可能在几分钟后被另一会话用 `Copy-Item` 还原抹掉（mtime 反而变旧）⇒ 判据文档应尽快随代码入库。**本文件与 `maintenance\BG-G35-conditional-feasibility.md` 就是这样丢过一次。**
7. **`-DryRun` 与真实模式的差别**：`DRYRUN=1` 时不开窗、不起 harness、不触发同步轮次，只打印计划 ⇒ 它**不构成**任何 G-3/G-5 结论。

---

## 10. 证据 / 产物路径

```
权威报告    F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\harness-report.md   (26 173 B / 402 行)
1017 取证   F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\NOTES-sync-status-not-observable.md
环境预检    F:\soft\00selfmade\rime_claude\reports\fluxing-bg-prep\{bg-cred-check.txt,bg-client-endpoint.txt,
                        bg-cloud-endpoint2.txt,bg-endpoint-reach.txt,bg-reachability-notes.txt}
自检原文    F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\verify-selfcheck-20261121.txt
            F:\soft\00selfmade\rime_claude\reports\fluxing-bg-g35\verify-observable-point-20261121.txt
脚本        F:\soft\00selfmade\rime_claude\scripts\verify\orphan-gate-g35-export-mtime.ps1   (25 706 B / 640 行)
harness 源  F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestOrphanReclaimE2E.cpp (29 325 B / 740 行)
harness 产物 F:\soft\00selfmade\rime_claude\release\TestOrphanReclaimE2E.exe                (181 248 B)
运行落点    F:\soft\00selfmade\rime_claude\tmp\fluxing-bg-g35-gate\g35-<yyyyMMdd-HHmmss>\
            (console.txt / hold.stdout.txt / hold.stderr.txt / log-slice.txt / hold.marker[.state|.pause])
构建日志    F:\soft\00selfmade\rime_claude\tmp\bg-g35-msbuild.log
判定真相源  F:\soft\00selfmade\rime_claude\include\FluxingMaintenanceGate.h  (:49 / :50 / :67-84)
判据原文    F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md  (§4)
            F:\soft\00selfmade\rime_claude\docs\release\regression-checklist-cloud-sync.md  (§1 AC-G / §5)
```

### 10.1 本文件（及配方）的重建来源

| 内容 | 来源 |
|---|---|
| 闸门判据 / 常量 / 自证链 / 造态四步 / 相位编排 / 三条设计决策 / V·I·N 三档 | `reports/fluxing-bg-g35/harness-report.md`（**逐条核对后改写**，该文件已入库 ⇒ 可复核） |
| 脚本参数 / 退出码 / ABORT·FAIL·INCONCLUSIVE 的具体理由串 | `scripts/verify/orphan-gate-g35-export-mtime.ps1` 与 `test/TestOrphanReclaimE2E/TestOrphanReclaimE2E.cpp`（**逐条核对后改写**） |
| 两条硬前置、结论口径（CONDITIONAL YES / `BLOCKED_HARNESS_PRECONDITION`）、未闭环 4 项 | BI handoff §2 + `CLAUDE.md` BG 指针行（**逐字照录**） |
| G-3 / G-5 原文语义 | `docs/release/regression-checklist-cloud-sync.md` §1 + `docs/release/restart-checklist-1.3.6.md` §4 |
| 「harness 路线与两条硬前置的关系」「R3 守卫 = 升级链第三段守卫」 | **推断**（已在正文标注） |

---

## 11. 下一批建议

1. **先提交**：`maintenance\BG-G35-conditional-feasibility.md` + 本文件随代码入库（BG 的其它产物已在 `e85259c`）⇒ 避免再被并发还原抹掉。
2. **真跑 G-3 / G-5（需用户在场 + 登录态）**：照 `maintenance\BG-G35-conditional-feasibility.md` 走一遍 `-Mode All`；**跑之前把相位 C 的正控制先看懂**（C 不前进 ⇒ 整轮 ABORT，别硬推）。
3. **真跑时的纪律**：全程**不要在别的窗口打字**（`m_last_input_tick` 全局）；证据目录 `tmp\fluxing-bg-g35-gate\g35-<时间戳>\` 要随结论一起留档；退出码以**退出码**为准，**不看 `VERDICT=` 串**（脚本的 `Abort` 分支即使退出码 = 5 也会打印 `VERDICT=GATE_G35_FAIL`）。
4. **不要重跑**：`harness` 自检（已 PASS）、`-DryRun`（已 PASS）、1017 不可观测取证（已完成）、175%/200% DPI（用户已拍板不切档）。
5. **仍需第二台设备 + 用户在场**：双机 AC 7 条、restore 端到端、输入延迟两组对照、两条硬前置的现场满足。
6. **待用户拍板**：1017「最近同步」永不更新 ⇒ 是否单独开一批修（属产品码改动，须 ADR + 四边同步 + 测试）。

---

**装机版 1.3.8.0（未 bump）· BG 批次未改任何产品码 · BG 自身实验从未真跑 ⇒ 结论 = CONDITIONAL YES / `BLOCKED_HARNESS_PRECONDITION`**
