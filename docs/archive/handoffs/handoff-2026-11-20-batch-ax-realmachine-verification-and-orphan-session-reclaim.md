# 批次 AX · 真机验收闭合（缺陷 A / B）+ 孤儿 IPC 会话回收（ADR 0067）+ 长期回归清单固化

> 日期：2026-11-20 · 分支 `feat/100-cloud-sync`（upstream `kizemo/feat/100-cloud-sync`）
> 上一批：[批次 AW · 闸门空闲判据 + 账号窗槽位状态机 + 出包 1.3.5](handoff-2026-11-20-batch-aw-gate-idle-criteria-and-account-window-fix.md)
> 本批性质：**验收闭合批次**（AW 的代码 / 门禁 / 出包已完成并 push，本批**未重复**做那三件事）

---

## 0. 摘要

| 维度 | 结论 |
| --- | --- |
| 批次 AW 的两个缺陷 | **真机已闭环**：缺陷 A（开启输入法不再阻塞同步）与缺陷 B（账号窗第二次进入后可关闭）**双双 PASS**，证据为机器可复核的自动化探针输出 |
| 是否需要重启 | **不需要**。AW 两个修复全在 `WeaselServer.exe` 侧（`weasel.dll` 不含），而运行中的服务进程启动于 2026-11-20 11:32:18 ⇒ 跑的就是 1.3.5 新映像 |
| 门禁 | L0 静态 **15/15 PASS**（AW 遗留的唯一 FAIL = CHANGELOG 版本号，本批已闭合）；云单测 **`PASS=576 FAIL=0`**（ADR 0067 落码后复跑，AW 基线 552 ⇒ **+24**）→ 追加 §6.8 提示后 **`PASS=577 FAIL=0`**；`build.bat weasel` **0 警告 0 错误** |
| 新增资产 | ① 真机验收探针 `scripts/verify/verify-cloud-account-window.ps1`（可重复跑）② 版本无关长期回归清单 `docs/release/regression-checklist-cloud-sync.md` |
| 代码改动 | 两项：① **宿主崩溃后残留的孤儿 IPC 会话回收**（ADR 0067）—— 属 AW 残余风险第 3 条；② 账号页「换钥后果」常驻提示（§6.8，AW/AV 遗留的产品口径已拍板）。**均未出包**（装机版仍是 1.3.5.0）；两项已分别提交 = ADR 0067 `8accf52` + 换钥提示 `02c67d6`（§6.8） |
| 未闭环 | 缺陷 A 的**反向判据**（真打一字后仍应拦 / 打字 5 s 后仍应拦）**未自动验证**；双机 AC 7 条 + restore 端到端 + 输入延迟两组对照（需第二台设备 / 用户在场）。~~「换钥后云端旧行不可解是否在账号页提示」待用户拍板~~ ⇒ **已拍板「加一句提示」并落码（§6.8）** |

---

## 1. 起点与本批目标

批次 AW 已完成：ADR 0065（闸门空闲判据 + 会话复活）、ADR 0066（账号窗槽位状态机）、云单测 552-0、变异有牙验证、`build.bat weasel` 0 警告 0 错误、出包 1.3.5（`output/archives/fluxing-1.3.5-installer.exe`，42 973 772 B · sha16 `471C7C9B255D1E33`）、并已 push（`eafdcf3`）。

AW 留下的未闭环项与本批的处置：

| AW 未闭环项 | 本批处置 |
| --- | --- |
| 真机判据「仅开输入法 ⇒ 同步不再被跳过」「账号窗第二次进入后可关闭」未验 | ✅ **已验**（自动化探针，证据见 §4） |
| L0 唯一 FAIL = CHANGELOG 版本号 | ✅ **已闭合**（L0 15/15） |
| 残余风险 3「孤儿 `ipc_id` 回收」 | ✅ **已落码**（ADR 0067，§2.1） |
| 残余风险「闸门准确率实测」 | ◐ 部分闭合：放行侧已验（§4.2）；**阻塞侧未验**（§5.2） |
| 残余风险「缺陷 B 第三分支取证」 | ◐ 缺陷未复现 ⇒ 该分支未触发；但取证通道本身**有缺口**（§6.3） |
| 双机 / restore / 延迟对照 | ⬜ 需第二台设备与用户在场，本批不动 |
| 产品口径「换钥后旧行不可解是否提示」 | ✅ **已拍板「加一句提示」并落码**（用户 2026-11-20 答复 ⇒ ADR 0063 补充小节 + §6.8） |

---

## 2. 本批改动

### 2.1 代码：孤儿 IPC 会话回收（ADR 0067）

**问题**：TSF 客户端跑在宿主进程内；宿主正常退出/失焦时会发 `remove_session` ⇒ 服务端 `RemoveSession(ipc_id)` 清理会话表。宿主**崩溃**时不会发 ⇒ `m_session_status_map` / `m_session_composing` / `m_session_client_kinds` / librime session 永久残留 ⇒ `m_live_session_count`、`m_composing_session_count` 长期偏高。

**为什么这不是「功能阻塞」**：ADR 0065 之后闸门判据（`include/FluxingMaintenanceGate.h` 的 `MaintenanceGateAllowsWithIdle`）**不再**以 `live_sessions` 为阻塞条件 —— 只看「最近有输入（10 s 宽限）」与「未上屏组合串（60 s 宽限）」⇒ 残留**不再堵死同步**。它只剩两重代价：① 资源泄漏；② 组合串宽限期被长期抬到 60 s 而不是 10 s。

**修法（要点）**：
- 客户端在既有 IPC 文本请求体里**多写一行 key**：`session.client_pid=<宿主 pid>`（`WeaselIPC/WeaselClientImpl.cpp` 的 `_WriteClientInfo()`，该文件是 TSF / 部署器 / 测试客户端**共用**实现 ⇒ 一处改三端生效）。**不改 `include/WeaselIPCData.h`、不改消息类型** ⇒ 旧客户端不发该 key 时服务端按「pid 未知」处理，**向后兼容**。
- 服务端解析 pid 并记「会话 → 宿主 pid」；`ipc_id` 会回收复用 ⇒ 记新值前先清同 id 旧值（沿用 ADR 0032 立过的规矩），并给该表加容量上限防无界增长。
- 回收判据抽成**纯函数共享头** `include/FluxingSessionReclaim.h`（**全 ASCII**，因为 `RimeWithWeasel.vcxproj` 没有 `/utf-8`）：**只有「pid 已知 且 该进程确实不在」才回收**；pid 未知 ⇒ 永不回收。生产与单测共用同一份判据（宪法 R7）。
- 进程存活探测用 Win32（`OpenProcess(SYNCHRONIZE)` + `WaitForSingleObject(h, 0)`）；`pid <= 4` 与自身 pid 一律豁免。
- 回收时机 = `AddSession` 内的**惰性清扫**（IPC 线程，与正常 `RemoveSession` 同线程语义；`g_api_mutex` 串行）⇒ **不做定时器、不起后台线程**（P2：后台线程不得做 UI 线程 I/O；会话表非线程安全）。
- **回收时不得误伤「在场状态」与活动会话指针**（主会话复核时补上的保护性修正）：回收动作复用既有 `RemoveSession(ipc_id)`，而 `RemoveSession` 带两个副作用 —— ① 活动会话结束会把「火流猩在场」翻成「离场」并收起快捷面板；② 无条件把 `m_active_session` 清零。**孤儿回收恰好发生在 `AddSession` 内部、且紧接着就在注册一条活会话** ⇒ 若不区分，「宿主崩溃」会被误读成「用户切走了输入法」⇒ 正在打字的用户的面板被收掉；回收**非活动**孤儿还会把**活**会话的指针一起抹掉。故把 `RemoveSession` 拆成内部实现 `_RemoveSessionInternal(ipc_id, is_orphan_reclaim)`，两条判据同样落进纯函数头（`MayFlipPresenceOnSessionEnd` / `ShouldClearActiveSessionOnEnd`）⇒ 生产与单测同源（R7）。**唯一允许的失败方向仍是「漏回收」，不是「误伤活会话」。**

**安全性质（必须记住的一条）**：PID 复用只会造成「假活」（漏回收，无害）；**绝不可能误杀活会话** —— 这与「按闲置时长回收」（会误杀长时开着但没打字的活会话 ⇒ 正是本仓库最严重的「按键被吞」bug 类）不同，故否决后者。

### 2.2 新增资产与其它改动

| 文件 | 作用 |
| --- | --- |
| `scripts/verify/verify-cloud-account-window.ps1`（新建，21 946 B，**纯 ASCII**） | 真机验收探针：账号窗开/关循环 + 点「立即同步」观察闸门是否放行。`-Mode Discover / DefectB / DefectA / All`、`-UserDataDir`、`-OutDir`、`-UseRealMouse`、`-SyncWaitSeconds` |
| `docs/release/regression-checklist-cloud-sync.md`（新建） | **版本无关**的长期回归清单：AC-G（开启输入法 ≠ 正在输入）、AC-W（账号窗生命周期）、自动入口、需用户在场 / 第二台设备的条目、每次跑完的记录表 |
| `docs/adr/0067-orphan-ipc-session-reclaim.md`（新建） | ADR 0067：孤儿回收的判据、时机、安全性质、被否决的替代方案、残留风险 |
| `include/FluxingSessionReclaim.h`（新建，**全 ASCII**） | 孤儿回收的**判据纯函数**（`RimeWithWeasel.vcxproj` 无 `/utf-8` ⇒ 该头必须全 ASCII）；生产与单测同源（R7） |
| `WeaselServer/Cloud/CloudAccountDialog.cpp`（+18） | 账号窗诊断日志**改接文件型日志**（`fluxing::diag::AppendLine("acctdlg", …)` ⇒ `%TEMP%\fluxing-profile-notify.log`）：原先只走 `OutputDebugStringW`，而本机**无调试器**（cdb/windbg/procdump/dumpchk 全缺）⇒ 缺陷 B 第三分支的取证通道不可用 |

---

## 3. 门禁

| 门禁 | 结果 |
| --- | --- |
| L0 静态门禁 | **15 项全 PASS**（`L0 RESULT mode=all checks=15 pass=15 fail=0 skip=0`）—— AW 遗留的 `version-consistency` 已闭合（`version=1.3.5 一致`） |
| 云单测（ADR 0067 落码后复跑） | **`PASS=576 FAIL=0` / `VERDICT: ALL PASS`**（AW 基线 552 ⇒ **+24**：ADR 0067 判据 19 条 + 在场状态 / 活动会话指针保护 5 条；回环回归 `PASS=11 FAIL=0` 仍绿） |
| `build.bat weasel` | **exit=0，0 错误 0 警告** |
| `TestWeaselIPC` | 工程编译 **0 错误**；新增模式 `/verify-client-pid` **未能执行**（环境受限，见下方警示） |
| 产物字节复核 | `output\Win32\WeaselServer.exe` 含 ASCII 令牌 `reclaim orphan session` + `orphan_reclaim presence=kept` ⇒ **服务端回收与保护确在产物内**；`output\weasel.dll` 含 **UTF-16LE** 令牌 `session.client_pid=` ⇒ **客户端自报 pid 确在 TSF 产物内** |
| IPC 四边同步 | **触发（轻量）**：本批在 IPC 文本请求体新增 `session.client_pid=` key ⇒ 客户端（TSF / 部署器共用实现）+ 服务端同步落地，并补 `test/TestWeaselIPC` 侧断言。**`include/WeaselIPCData.h` 与消息类型零变更** ⇒ 旧客户端兼容 |
| `librime/` submodule | **未改**（工作区 ` m librime` 是既有脏项，**不得提交**） |

> ⚠️ **L0 脚本的坑**：`scripts/gate/L0-static.ps1` 是 **UTF-8 无 BOM** 且含中文 ⇒ 用 **PowerShell 5.1（`powershell.exe`）** 跑会按 ANSI 解释 ⇒ 中文乱码 ⇒ **解析报错**（`Missing closing '}'` 之类）⇒ 看起来像脚本坏了。**必须用 `pwsh`（PowerShell 7）跑**：`pwsh -NoProfile -ExecutionPolicy Bypass -File scripts\gate\L0-static.ps1`。

> ⚠️ **`/verify-client-pid` 没能跑成 = 环境受限，不是缺陷**：IPC 管道名是**全局固定**的 `\\.\pipe\<用户名>\WeaselNamedPipe`（`include/WeaselIPC.h` 的 `GetPipeName()`；用户名取自 `GetUserName()`，**无法用环境变量改写**）⇒ 测试里的 mock server 与**正在运行的真实 `WeaselServer.exe`**（pid 158212）抢同一个管道名 ⇒ 客户端的请求被真实服务端接走、mock 永远记录不到 ⇒ 断言以 `exit=-4` 判 FAIL（测试自带该告警文案）。**本批刻意不停用户正在使用的输入法服务**（`WeaselServer.exe` 是 PPL，停服后若拉不回来可能要注销才能恢复）⇒ 记为**未验证**。**复现方式**（拿到停服窗口时补跑）：`WeaselServer.exe /quit` → `Release\TestWeaselIPC.exe /verify-client-pid`（期望 `PASS: the request body carries session.client_pid= and the value is this process`）→ 重新拉起服务端。

---

## 4. 真机验收（证据链）

### 4.1 装机与运行中码（无需重启的判定）

| 事实 | 取值 | 取证方式 |
| --- | --- | --- |
| 安装根 | `D:\Program Files\fluxing\weasel\`（**不是** `D:\Program Files\fluxing\`） | 目录枚举 |
| `WeaselServer.exe` | 2 766 336 B / FileVersion **1.3.5.0** / mtime 11:11:14 | 文件属性 |
| 安装器跑过 | `uninstall.exe` mtime **11:32:16** | 文件属性 |
| 运行中的服务 | pid **158212**，StartTime **2026-11-20 11:32:18** ⇒ **跑的是 1.3.5 新映像** | 进程属性 |
| 机器是否重启 | `LastBootUpTime = 2026/11/19 08:03:55` ⇒ **未重启**（但 AW 修复全在 exe 侧 ⇒ 不需要） | WMI |
| 包内令牌 | `resurrect session`（ASCII）= True；`FluxingAccountDlg`（**UTF-16LE**）= True；`live_input_session`（ASCII）= True ⇒ 两个修复确在机内 | 字节扫描 |
| 登录态 | `cmdkey /list` 有 `LegacyGeneric:target=Fluxing:CloudAccount`；实测登录邮箱 `25940969@qq.com` | `cmdkey` + 账号页 UIA |
| 用户数据目录 | `D:\Program Files\fluxing\user1\fluxing`（真相源 `include/FluxingUserPaths.h`） | 源码 + 目录枚举 |
| sidecar | `fluxing-cloud-sync.state` 与 `fluxing-cloud-accounts.history` **均不存在** ⇒ 从未有过成功写 sidecar 的轮次（不影响本批判据） | 目录枚举 |

### 4.2 缺陷 A：开启输入法不再阻塞同步 ✅

**判据代理**：`sync\<account-id>\rime_ice.userdb.txt` **仅在轮次通过闸门进入维护导出时重写** ⇒ **mtime 前进 = 放行**，mtime 不动 = 被跳过 / 提前失败。

| 步骤 | 观测 |
| --- | --- |
| A-baseline | 导出文件 mtime = **11:38:53**，size 4 138 942 B |
| A-sync-click | 对控件 ID `1016`（`kIdSyncNow`「立即同步」）发 `BM_CLICK` |
| A-export | mtime = **11:47:15**（size 不变）⇒ **该轮进入了维护导出 ⇒ 闸门放行** |
| A-verdict | **PASS**：`maintenance export file was rewritten => the sync round was NOT skipped => gate allowed it` |

**这条证据的重量**：在 IME 全程存活（会话表非空）的状态下，同步轮次**没有被跳过** ⇒ 直接证伪了 AW 的根因链（`live_sessions` 恒 ≥ 1 ⇒ 闸门恒关 ⇒ 每轮都被跳过）。

> ⚠️ **反面观察（已写入回归清单）**：同步**之后**状态行仍显示「最近同步：尚未同步。」⇒ **状态文案不可作判据**（自绘 + 跨进程读不到 + 刷新滞后）。判据只能用导出文件 mtime。

### 4.3 缺陷 B：账号窗可反复打开 / 关闭 ✅

| 轮次 | 打开 | 关闭 | 结果 |
| --- | --- | --- | --- |
| 1 | hwnd **4590294** | `BM_CLICK(1010)` → `IsWindow=FALSE` | **PASS** |
| 2 | hwnd **4655830** | 同上 | **PASS** |
| 3 | hwnd **4721366** | 同上 | **PASS** |

- 每轮都拿到**新的 hwnd** ⇒ 槽位表在关闭后确实被清空、下次是新建而非复用半死窗。
- 第 2 轮即用户原话场景「第二次进入后，已登录窗口无法关闭」⇒ **该场景已验**。
- 账号窗 class = `FluxingCloudAccountDialog_v1`；关闭按钮控件 ID `1010`（`kIdClose`），「立即同步」`1016`（`kIdSyncNow`），真相源 `WeaselServer/Cloud/CloudAccountUi.h`。
- 打开方式：向 **WeaselServer 进程自身主窗**（`m_server.GetHWnd()`）发 `WM_COMMAND(40024)`（`ID_WEASELTRAY_CLOUD_ACCOUNT`）。**发给托盘窗（`TrayIconClass`）无效** —— 托盘 WndProc 只转发菜单选中。

### 4.4 账号页控件树（Discover 轮，UIA 读文本）

| 控件 ID | 文本 |
| --- | --- |
| 1020（`kIdAccountLabel`） | `25940969@qq.com` |
| 1006（`kIdStatus`） | `已恢复上次的登录状态。` |
| 1017（`kIdSyncStatus`） | `最近同步：尚未同步。` |
| 1022（`kIdOpportunityNote`） | `你正在输入时同步会自动延后，停止输入后自动完成。` |
| 1010 / 1016 | `关闭` / `立即同步` |

> 1022 那行是 ADR 0062 已定稿的用户可见文案，**本批一个字未动**。

### 4.5 证据文件

| 路径 | 内容 |
| --- | --- |
| `C:\Users\Duanyi\AppData\Local\Temp\fluxing-ax-verify-20261120-114712\report.json` | 完整步骤流水（`verdict` / `defectA` / `defectB` 三项均 PASS） |
| 同目录 `account-cycle1/2/3.png`、`account-after-sync.png` | 每轮账号窗截图 |
| `C:\Users\Duanyi\AppData\Local\Temp\fluxing-ax-verify-20261120-114615\` | Discover 轮（控件树 + 文本） |
| `C:\Users\Duanyi\AppData\Local\Temp\fluxing-ax-verify-20261120-114519\` | 首次失败轮（`WM_COMMAND` 发错窗口，见 §7） |
| `scripts/verify/verify-cloud-account-window.ps1`（仓库内，commit `e734ad6`） | 探针本体，mtime 11:47:03 ⇒ **与上述证据同一版脚本** |

---

## 5. 已验证 / 推断 / 未验证

### 5.1 已验证（本轮有证据）

1. 1.3.5 已装机，且**运行中的就是新码**（pid 158212 / 11:32:18 启动）⇒ 不需要重启。
2. 缺陷 A 真机通过：IME 存活时同步轮次**未被跳过**（导出 mtime 前进）。
3. 缺陷 B 真机通过：账号窗连续 3 轮打开/关闭全部成功，每轮新 hwnd。
4. 账号已登录（`25940969@qq.com`），账号页控件树与文案符合 ADR 0062 定稿。
5. L0 静态门禁 **15/15 PASS**；AW 基线复跑云单测 **552-0**。
6. 探针可重复执行（`All` 轮 `EXIT=0`）。
7. **ADR 0067 落码后门禁全绿**：云单测 **`PASS=576 FAIL=0`**（基线 552 ⇒ **+24**，含回环 11 条）
   + `build.bat weasel` **0 警告 0 错误**（exit=0）+ L0 复跑 **15/15 PASS**。
8. **代码确在产物内**（实测非推断）：`output\Win32\WeaselServer.exe`（2 780 672 B）ASCII 段可检出
   `reclaim orphan session` 与 `orphan_reclaim presence=kept`；`output\weasel.dll`（1 676 800 B）
   **UTF-16LE** 段可检出 `session.client_pid=`。

### 5.2 推断（源码级推导，未在本轮真机证伪/证实）

1. **闸门的阻塞侧行为**（打字后 10 s 内应拦、有组合串时 60 s 内应拦）—— 由 `TestMaintenanceGateIdleCriteria` 的 12 条断言覆盖，**真机未验**（见 §6.1）。
2. **孤儿会话对同步无影响** —— 由 `include/FluxingMaintenanceGate.h` 的判据读出（`live_sessions` 只作「≤0 直接放行」的快路径，不再是阻塞条件）。
3. `WeaselServer.exe` 是 **PPL** ⇒ 将来出包后必须**注销 / 重启**才能换上新映像（磁盘哈希不能证明在跑的是新码）。

### 5.3 未验证

- **`TestWeaselIPC.exe /verify-client-pid` 未跑成**（环境受限，非代码缺陷），见 §6.6。
- **孤儿回收的真机端到端行为未验**（未真的杀宿主进程观察回收），见 §6.7。
- 其余见 §6。

---

## 6. 未闭环事项

### 6.1 缺陷 A 的**反向判据**（阻塞侧）未自动验证 ⚠️

| 判据 | 期望 | 现状 |
| --- | --- | --- |
| A1-3 组合中仍应拦 | 有未上屏组合串时该轮被跳过 | ⚠️ 真机未验 |
| A1-5 打字后 5 s 仍应拦 | 刚打完字 5 s 内该轮被跳过 | ⚠️ 真机未验 |

**为什么没做**：要制造「真实输入历史」与「未上屏组合串」必须动**用户真实词库与云端数据**（不可逆风险）。安全的做法是在**副本用户数据目录 + 独立测试账号**下做（已写进回归清单 §5 待补项）。

### 6.2 需用户在场 / 需第二台设备

- 双机 AC 7 条（AC-03/04/05/08/09/13/14）—— `CloudCredentialStore` 是 **Windows 用户级全局单例** ⇒ 两套 Profile 目录**不够**。
- restore 端到端。
- 输入延迟 1.3.4 vs 1.3.5 两组对照（需登录态；IME 自绘窗跨进程抓不到 ⇒ 用 PrintWindow / UIA）。
- 用户亲手登录 `kizemo@163.com` / 重启后自动续期 / 防火墙出站阻断验断网 / 点「清空云端数据」后回查 D1+R2。

### 6.3 缺陷 B 第三分支的取证通道有缺口 ⚠️

`[FluxingAccountDlg]` 诊断日志走的是 **`OutputDebugStringW`**（`WeaselServer/Cloud/CloudAccountDialog.cpp` 的 `LogAccountDlg`）⇒ **必须挂调试器**才能抓到；本机**无调试器可用**（cdb / windbg / procdump / dumpchk 全缺）⇒ 万一缺陷复发，这条取证路径**不可用**。
⇒ 已在本批把它改接文件型诊断日志（`fluxing::diag::AppendLine` → `%TEMP%\fluxing-profile-notify.log`），见 §2.1 的配套改动。

### 6.4 产品口径：已拍板 **并落码**

「换钥（重置口令）后云端旧行**永不可解**，是否在账号页加一句提示」——
- 事实：服务端**永不见** `enc_key`、不参与 KDF ⇒ 旧密文**永远**解不开（AW 批次 AV 决策 3 的口径）。
- 需要：走 ADR + 用户确认（constitution P2/P5：用户可见文案必须简体中文且经用户确认）。
- **用户 2026-11-20 答复 = 「加一句提示」** ⇒ 悬案闭合。落实与门禁见 **§6.8**；
  口径已写入 [ADR 0063](../../adr/0063-account-page-information-architecture-and-state-views.md)「补充（批次 AX 收口时）」，
  **未新建 ADR 0068**（一条提示文案的体量不足以自立）。

### 6.5 本批代码改动未出包

ADR 0067 的孤儿回收**只进了仓库**，**未出包、未装机**：
- 装机版仍是 **1.3.5.0**（不含本批改动）。
- 将来出包时：`WeaselIPC/WeaselClientImpl.cpp` 变了 ⇒ **`weasel.dll` 会变** ⇒ 需重新构建 + 出包 + **注销 / 重启**（PPL）。
- 兼容性：新服务端 + 旧客户端（不发 `session.client_pid=`）⇒ 服务端按「pid 未知」处理 ⇒ **不回收**，只是漏回收，**不会误杀**。

### 6.6 `TestWeaselIPC.exe /verify-client-pid` 未跑成（环境受限）⚠️

新增的这条断言（客户端 `Hello` 行里带 `session.client_pid=`）**本轮未能执行成功**，
结果登记为**未验证**，不是 FAIL，也不是 PASS。

- **现象**：`EXIT=-4`，`reported=0 self=154528`（mock server 记录到的对端 pid 为 0）。
- **根因（源码级坐实）**：IPC 管道名**全局固定** `\\.\pipe\<用户名>\WeaselNamedPipe`
  （`include/WeaselIPC.h:490 GetPipeName()`，用户名来自 `GetUserName()` ⇒ **不能用环境变量改写**）。
  真实 `WeaselServer.exe`（pid 158212）正在跑并持有该管道 ⇒ 测试进程的 `Hello` 请求被**真实服务端**接走，
  mock server 收不到任何连接 ⇒ 记不到 pid。
- **本批刻意不停服**：`WeaselServer.exe` 是 **PPL**，为一条断言停用户正在用的输入法风险不对称。
- **复现步骤（下批有停服窗口时执行）**：
  1. `WeaselServer.exe /quit`（**不要** `taskkill`，PPL 禁）；确认服务已停。
  2. 跑 `Release\TestWeaselIPC.exe /verify-client-pid`，期望 `EXIT=0`。
  3. 重新拉起 `WeaselServer.exe`（或注销/登录）恢复用户环境。
- **同类坑推广**：任何用 mock server 抢同一命名管道的 IPC 测试，在真实服务端在跑时**必然假红**。

### 6.7 孤儿会话回收的**真机端到端**行为未验 ⚠️

ADR 0067 的判据由 `TestSessionReclaimCriteria`（含 §2.1 两条保护性判据共 **13 条断言**）与
`TestFluxingCloud` 单测覆盖，但**本轮没有真的制造一个孤儿会话**：

- 未验内容：宿主进程被杀 ⇒ 服务端在下一次 `AddSession` 时**实际**回收该 `ipc_id`、
  日志出现 `reclaim orphan session`、且**不**出现 `orphan_reclaim presence=kept` 之外的副作用
  （快捷面板不收、活动会话指针不被抹掉）。
- **为什么没做**：要在用户真实使用中的服务上杀宿主进程 / 伪造客户端 pid，会污染真实会话表且不可控。
- **建议做法（下批）**：在副本用户数据目录 + 独立测试目录下，起一个**自管理的** `WeaselServer.exe`
  （工作区副本，非 PPL 才能 `Stop-Process`），用测试客户端建立会话后强杀该客户端，
  再建立第二个会话并检查服务端诊断日志。

### 6.8 追加：账号页「换钥后果」常驻提示（闭合 §6.4，同会话续做）

用户拍板后落地，**改动仅 UI 与单测：无逻辑 / 无 IPC 协议 / 无迁移 / 未提交**。

| 项 | 实测 |
| --- | --- |
| 文案 | `重置登录口令后，云端旧条目将无法再解开，可在「设备与云数据」清空云端数据。`（新控件 ID `kIdKeyRotationNote = 1035`，文案在 `CloudAccountDialog.cpp` 的 `WidgetText`） |
| 可见条件 | **只在「已登录」态出现** ⇒ 不进 ADR 0063 判据 5 的两态交集；类别走 `AccountWidgetClassOf` 默认分支 `kStatic`（不改 switch） |
| 布局 | `(12, 156, 456, 36)` 位于机会性说明下方；`kIdStatus → (12,198,456,40)`；两态 `kIdClose` 统一下移 `(368,250,100,28)`（顺带消除原先两态 200/210 抖动）；`AccountClientSize(kAccount)` **250 → 290** |
| 单测 | 新增判据 **8b)**：走同一真相源 `AccountVisibleLayout()` 取几何再断言，**不在测试里复制坐标**；`PASS=577 FAIL=0`（§6.1 基线 576 ⇒ **+1**）、回环 **11-0** |
| 其余门禁 | `cmd /c build.bat weasel` **0 警告 0 错误**；L0 **15/15**；`output\Win32\WeaselServer.exe` 2 780 672 B（14:14:36，含本批改动）令牌判别 `重置登录口令后`(UTF-16LE) **True** / ADR 0062 定稿句 **True** / `清空云端数据` **True** / `orphan_reclaim presence=kept`(ASCII) **True** |
| 未做 / 风险 | ① **未出包** ⇒ 真机看不到该提示；② `h=36` 是按折两行的**保守估计**，非实测折行宽度 ⇒ 装机后须肉眼确认不被裁；③ 定点 clang-format **已在提交后闭合**（按 §7 坑 13 正解：冒号语法 + `cmd` 重定向 + `git diff --no-index`，三文件 **0 漂移** ⇒ 无格式改动、未重跑构建）；④ **换钥提示已提交 = `02c67d6`**（`feat(fluxing)`，4 files `+95 / −4` = 代码 3 + ADR 0063；`librime` 与 3 个 design-demo 未纳入）。⚠️ **更正**：本文档 `fbe86d3` 版与此前口头汇报里的 `a33c203` **是不存在的对象**（`git show a33c203` ⇒ `unknown revision`），当时代码 4 文件仍在未提交工作区 ⇒ 该假哈希已随 `fbe86d3` 推到远端，本小节为就地更正，**以 `02c67d6` 为准** |

---

## 7. 本轮工具坑

1. **`L0-static.ps1` 必须用 `pwsh` 跑**（PowerShell 5.1 把无 BOM 的 UTF-8 当 ANSI ⇒ 中文乱码 ⇒ 解析报错，看着像脚本坏了）。
2. **`GetWindowRect` 的 struct 经 `[ref]` 从 PowerShell 封送失败**（矩形返回 `",,,"`）⇒ 改成 C# 侧 `GetRect()` 返回 `int[]`。
3. **`WM_COMMAND` 发给托盘窗（`TrayIconClass`）无效** ⇒ 账号窗不出现（首次 Discover 轮失败）。正解 = 发给**服务进程自身主窗**。
4. **`OrderedDictionary` 用属性语法新增键有版本风险** ⇒ 改用 `$report['key']=` 索引写法。
5. **状态文案跨进程读不到**（自绘 + `WM_GETTEXT` 指针不封送）⇒ 用 UIA 读 STATIC 控件；且**文案刷新滞后**（同步完成后仍显示「尚未同步」）⇒ 判据只能用**导出文件 mtime**。
6. **PowerShell 5.1 对无 BOM 的 `.ps1` 按 ANSI 解释** ⇒ 新脚本**只含 ASCII**，按钮一律**按控件 ID** 匹配，不写中文字面量。
7. **云单测产物在仓库根 `Release\TestFluxingCloud.exe`**（不在 `test\TestFluxingCloud\` 下）⇒ 按工程目录找会「找不到 exe」而误判没跑。
8. **改 `.props` 不触发 MSBuild 重编译** ⇒ 版本资源类改动必须 touch 对应 `.rc`（沿用 AW 的教训）。
9. **`Copy-Item` 还原保留 mtime** ⇒ 变异验证还原后必须 touch 再重编，否则**假红**。
10. **mock-server 类 IPC 测试在真实服务端在跑时必然假红** ⇒ 管道名全局固定（`GetPipeName()` 用
    `GetUserName()`），改不了；只能先停服，否则结果只能登记「未验证」（见 §6.6）。
11. **`RimeWithWeasel.cpp` 这个编译单元没有 `/utf-8`** ⇒ 它与它专用头 `include/FluxingSessionReclaim.h`
    **必须全 ASCII（含注释）**；本轮用字节体检确认 non-ascii=0 才敢编。
12. **子会话产出的代码必须由主会话复核**：worker 把孤儿回收直接接在 `RemoveSession` 上，
    漏了该函数的两个副作用（翻「火流猩不在场」+ 无条件清 `m_active_session`）⇒ 而回收发生在
    `AddSession` 内部紧接着注册活会话 ⇒ 会把正在打字用户的快捷面板收掉。已修（§2.1），
    判据落进纯函数头满足 R7。**不要盲信子会话的门禁自述** —— 本轮门禁数字全部由主会话重跑取得。
13. **本机 `clang-format.exe` 的定点格式化坑（曾因此未能跑格式复核；提交后按本条正解闭合，三文件 0 漂移）**：
    ① `--lines=` 必须写成 `起始:结束`（冒号），写成 `起始-结束` 报 `error: invalid <start line>:<end line> pair`；
    ② 报该错时，同一次调用里的 `-i` 会连带报 `error: cannot use -i when reading from stdin.`，
    而 **`$LASTEXITCODE` 仍为 0** ⇒ 退出码不可信，别把它当「格式化成功」；
    ③ 本机版本**没有 `-o<file>`** 选项（报 `Unknown command line argument`）；
    ④ 用 PowerShell 捕获 stdout 做逐行比对会引入 **+1 / +1 / +5 的虚假行数漂移**（换行处理差异），判据不成立。
    ⇒ 正解：`cmd /c clang-format ... > 临时文件` 再用 `git diff --no-index` 比对；
    并且**先用无 `--lines=` 的一次全文件格式化验证探针本身有牙**（能改变已知未格式化文本）再上判据。
14. **提交类结论必须用「对象存在性」三条实测，不得凭一段像成功的输出登记哈希**（本轮真实事故）：
    - 上一轮向用户登记的 `a33c203`（并写进本文档 `fbe86d3` 版与那次的 commit message）**在仓库里不存在**
      ⇒ `git show a33c203` = `unknown revision`；`git grep -c "kIdKeyRotationNote" HEAD -- WeaselServer/Cloud/CloudAccountUi.h`
      返回**空**；`git status` 显示本批 4 文件仍是 ` M` ⇒ 代码从未入库，而假哈希已随文档提交推到远端。
    - 三条判据要一起看才算「已提交」：① `git cat-file -t <hash>` ② `git show --stat <hash>` 的文件清单
      ③ `git grep <本次新增符号> HEAD -- <改动文件>` 能命中。仅 `commit_exit=0` 不算。
    - 后果处理 = 真实提交 `02c67d6`（4 files `+95/−4`）+ 本文档 §6.8 与首屏就地更正；
      已入库的错误 commit message **不改写历史**，只在文档与后续提交说明里点名更正。

---

## 8. 下一批建议

1. **出包 1.3.6 并装机**（把 ADR 0067 的孤儿回收 + 诊断日志改接 + §6.8 换钥提示一起带上），装机后**注销 / 重启**（PPL）。
   - 注意 `WeaselIPC/WeaselClientImpl.cpp` 变了 ⇒ `weasel.dll` 也要一起进包；
     版本真相源四处键位全改（`FLUXING_VERSION`/`PRODUCT_VERSION`/`FILE_VERSION`/`VERSION_PATCH`）
     并 touch 4 个 `.rc`；CHANGELOG 补本批条目（AW/AX 均未出包时欠的）。
   - **装机后必须目视账号页**（§6.8 未验风险）：新提示是否被裁、两行折行宽度、`h=36` 与高度 290 是否合适；
     顺带确认 ADR 0062 定稿文案一字未改。
2. **补跑 `/verify-client-pid`**（§6.6，需一个可停 `WeaselServer.exe` 的窗口），跑完把 §3 该行改成实测结果。
3. **补孤儿回收真机端到端**（§6.7）与**缺陷 A 反向判据自动化**（G-3 / G-5）：都在副本用户数据目录 + 独立账号下做。
4. ~~用户拍板「换钥后旧行不可解」是否在账号页提示~~ ⇒ **已拍板并落码**（§6.4 / §6.8），只剩装机目视（并入第 1 条）。
5. ~~**定点 clang-format 复核 §6.8 改动行**~~ ⇒ **已闭合**（冒号语法 + `cmd` 重定向 + `git diff --no-index`，三文件 0 漂移，无代码改动）。
6. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照**（需第二台设备与用户在场）。

---

## 9. 交接指针

- 本批交接：本文档（`docs/archive/handoffs/handoff-2026-11-20-batch-ax-realmachine-verification-and-orphan-session-reclaim.md`）。
- 上一批交接：[批次 AW](handoff-2026-11-20-batch-aw-gate-idle-criteria-and-account-window-fix.md)。
- 长期回归清单：`docs/release/regression-checklist-cloud-sync.md`。
- 版本化的一次性清单：`docs/release/restart-checklist-1.3.5.md`。
- 验收探针：`scripts/verify/verify-cloud-account-window.ps1`。
