# 批次 BB · B-4「用户主动切走 IME ⇒ presence 翻 absent + 快捷面板收起」判词

> **判词：`B4_VERDICT=PASS_SERVER_PATH`**
>
> 实际走的路径：**路径 1 —— 合成 IPC 客户端直接向运行中的 `WeaselServer.exe` 发 focus-out 消息**
> （路径 2「真实 TSF 宿主产生真实 FocusOut」**未做**，见 §7.1）。
>
> 权威证据目录：`reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091312/`
> （`console.txt` = 判据汇总；`log-slice-A/B/C/D/E.txt` = 服务端诊断日志逐阶段切片）。
>
> 批次性质：**纯取证批次**。未改 `librime/`、未改 IPC 协议消息类型、**未改任何产品代码**、
> 未新增第三方依赖、未重装、未重启电脑、未注销账号、未杀任何用户应用进程、未改注册表、
> **未 commit、未 push**。

---

## 0. 一句话

服务端在收到「正常结束 / 主动切走」类 IPC 消息时，走的仍是**批次 AW 之前的那条老路径**
（`remove_session active presence=absent` / `focus_out param=1 presence=absent`），
**没有**被 ADR 0067 的孤儿回收分支截胡；同一批运行里的孤儿对照组仍然停在
`orphan_reclaim presence=kept`（不翻 presence、不收面板）⇒ 两条分支判别力成立。

---

## 1. 验收项原文与判定口径

| # | 验收点（批次 AW 回归项） | 判据 | 本批证据 | 结论 |
|---|---|---|---|---|
| B-4-1 | 用户主动切走 IME ⇒ presence 翻转为 absent | 服务端诊断日志出现 `presence=absent`，且**不**出现 `orphan_reclaim presence=kept` | 阶段 B/C 的 `focus_out param=1 presence=absent`；阶段 A/B 的 `remove_session active presence=absent` | ✅ |
| B-4-2 | 快捷面板收起仍发生 | 面板窗口「运行中可见 → focus-out 后不可见」 | 阶段 B/C：`B_PANEL_SEEN_VISIBLE=True` → `B_PANEL_VISIBLE_AFTER=False` | ✅ |
| B-4-3 | 与批次 AW 前一致（未被 ADR 0067 改变） | 正常路径与孤儿路径必须走**不同分支** | 阶段 E 孤儿对照 `remove_session active orphan_reclaim presence=kept`，同一切片内 `remove_session active presence=absent` 计数 = 0 | ✅ |
| B-4-4 | 阴性对照有效（focus-out 未发生时不得翻 absent） | 只发 END_SESSION、不发 FOCUS_OUT 时，**不得**出现 `focus_out param=` 行 | 阶段 A：`A_FOCUS_OUT_LINES count=0` | ✅ |

**口径澄清（必须在判词里区分，否则会误读）**

1. `presence=absent` 有**两个**触发点，判词要分别承认：
   `remove_session active presence=absent`（会话正常结束时随会话结束一起翻）与
   `focus_out param=1 presence=absent`（收到 `PROFILE_DEACTIVATED` focus-out 时翻）。
   两者都出现在本批阳性运行里。
2. **普通切窗口（`APP_FOCUS_LOST`）本就不该翻 presence**，这是产品语义
   （只有 `PROFILE_DEACTIVATED` 才意味「Fluxing 离场」，见
   `RimeWithWeasel.cpp` 中 `UpdateFluxingPresence` / `ShouldHideQuickPanelOnFocusOut` 的 param 判定），
   **不是**回归。阶段 D 实测 `focus_out param=2 presence=kept`，与该语义一致。
3. 「快捷面板收起」受「面板常驻」开关（`AlwaysShowMode()`）约束；本批在
   `PROFILE_DEACTIVATED` 场景下实测到收起（阶段 B/C），在 `APP_FOCUS_LOST` 场景下
   **面板本轮未曾可见**（阶段 D 未激活 profile），故 D 的「收起」**未取得可见性证据**，
   只取得 presence 语义证据。

---

## 2. 实际走的路径：为什么是路径 1

| 维度 | 路径 1（合成 IPC 客户端） | 路径 2（真实 TSF 宿主） |
|---|---|---|
| 覆盖范围 | **服务端**对 focus-out / end-session 的**全部处理逻辑**（`OnFocusOut` → `RequestHandler::FocusOut` → presence / 面板 / 会话表） | 额外覆盖 **TSF 侧发射**（`WeaselTSF::Deactivate()` → `EndSession()` + `_PostProfileNotify`） |
| 本批是否做 | ✅ 已做 | ❌ 未做 |
| 结论 | 服务端路径已闭环；**TSF 侧发射顺序仍是「按代码阅读推断」，未在真机 TSF 宿主上取证** | 见 §7.1 的精确缺口 |

路径 1 的覆盖度之所以够，是因为本批**主动复刻了真实 TSF 的消息顺序**：
`WeaselTSF::Deactivate()`（`WeaselTSF/WeaselTSF.cpp:114-126`）先同步 `m_client.EndSession()`、
再异步投递 profile-deactivated 通知 ⇒ 服务端**先收 END_SESSION、后收 FOCUS_OUT(PROFILE_DEACTIVATED)**。
阶段 B 就是按这个顺序发的。

---

## 3. 方法

### 3.1 新增文件（全部为新增，**未触碰任何他人文件**）

| 文件 | 作用 |
|---|---|
| `test/TestOrphanReclaimE2E/TestFocusOutE2E.cpp` | 合成 IPC 客户端（ASCII-only，21.5 KB）：握手 / 发 focus-in / focus-out / end-session / 突发退出 / 持续按键 |
| `test/TestOrphanReclaimE2E/TestFocusOutE2E.vcxproj` | **独立工程**：无 `rime.lib`、无 `ProjectReference`、不用 PCH ⇒ 不触发 `RimeWithWeasel.cpp` 重编、不破坏 `Release\stdafx.pch`、不碰 B-1/B-2/B-3 依赖的 `Release\TestOrphanReclaimE2E.exe` |
| `scripts/verify/b4-focusout-path.ps1` | 五阶段探针（20 KB）：每阶段独立日志切片基线 + C# 面板窗口枚举 |

客户端的常量与管道名**取自 `include/WeaselIPC.h` 与 `weasel::GetPipeName()`**（宪法 R7 单一事实源），
不沿用兄弟工具里手写的魔数。

**关键实现细节**

* 握手体带 `session.client_pid=<pid>` ⇒ ADR 0067 的回收判据「适用」，
  孤儿对照组才有判别力（对照组与阳性组**唯一差别**就是缺 END_SESSION）。
* 握手体**不带** `session.client_app=FluxingPhrasesDialog.exe` ⇒ 保持 `kUserApp` 身份，
  使 `ShouldTreatSessionEndAsFluxingAbsent` 成立，避开 ADR 0032 的 own-UI 例外。
* 客户端**持续按键**（每 700 ms）⇒ 维护闸门保持「有活跃输入」，
  避免 idle > 10 s 后闸门放行导致本次探针**无判别力**。
* 阶段 E 的 sweep 触发会话**故意不发 END_SESSION**（否则会在判定切片里插入
  `remove_session active presence=absent` 污染阴性判据），改为判定完成后用
  `--end-session-by-id` 清理，避免残留会话堵住维护闸门。

### 3.2 五阶段设计

| 阶段 | 模式 | 发的消息序列 | 期望 | 实测 |
|---|---|---|---|---|
| A | `--end-session` | FOCUS_IN(APP_FOCUS_GAINED) → END_SESSION | **无** `focus_out param=` 行；`remove_session active presence=absent` ≥1 | ✅ 见 §4.1 |
| B | `--focusout-deactivate` | FOCUS_IN(PROFILE_ACTIVATED) → END_SESSION → FOCUS_OUT(PROFILE_DEACTIVATED) | `presence=present` → `presence=absent`；面板可见→不可见 | ✅ 见 §4.2 |
| C | `--focusout-only` | FOCUS_IN(PROFILE_ACTIVATED) → FOCUS_OUT(PROFILE_DEACTIVATED) → END_SESSION | 同上，且**无** reclaim 行（证明会话按正常路径结束） | ✅ 见 §4.3 |
| D | `--focusout-appfocus` | FOCUS_IN(APP_FOCUS_GAINED) → FOCUS_OUT(APP_FOCUS_LOST) → END_SESSION | `focus_out param=2 presence=kept`；**不**出现 `param=1` | ✅ 见 §4.4 |
| E | `--abrupt-exit` + `--sweep-trigger` | 孤儿会话（**不发** END_SESSION）→ 新会话触发惰性清扫 | `reclaim orphan session` + `orphan_reclaim presence=kept`；切片内 `presence=absent` 计数 = 0 | ✅ 见 §4.5 |

### 3.3 构建与运行（可复制）

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestFocusOutE2E.vcxproj' `
  /p:Configuration=Release /p:Platform=x64 '/p:SolutionDir=F:\soft\00selfmade\rime_claude' /nologo /v:minimal

& pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b4-focusout-path.ps1'
```

本批权威运行：探针产物 `Release\TestFocusOutE2E.exe`（196 096 B，2026/11/21 09:10:33）；
被测服务端 `pid=43172`，路径 `D:\Program Files\fluxing\weasel\WeaselServer.exe`（装机版 1.3.8）。
探针进程**全部已退出**；本次运行未残留任何探针会话。

---

## 4. 原始日志（阴性 / 阳性对照逐行）

服务端诊断日志：`%TEMP%\fluxing-profile-notify.log`（本机 `C:\Users\Duanyi\AppData\Local\Temp\fluxing-profile-notify.log`），
行格式 `[YYYY-MM-DD HH:MM:SS.mmm][server] <msg>`。

### 4.1 阴性对照（阶段 A：只结束会话，**不发** focus-out）

```
2026-11-21 09:13:15.258][server] remove_session active presence=absent
```

判据汇总（`console.txt`）：

```
A_IPC_ID=11
A_NORMAL_PATH_ABSENT count=1 (expected >= 1)
A_FOCUS_OUT_LINES count=0 (expected 0 - negative control)
A_RECLAIM_LINES count=0 (expected 0)
A_ORPHAN_KEPT_LINES count=0 (expected 0)
A_ABSENT_LINE=remove_session active presence=absent
```

⇒ **阴性对照成立**：不发生 focus-out 时，日志里**一条** `focus_out param=` 都没有；
同时「会话正常结束 ⇒ presence=absent」这条老路径**照常发生**。

### 4.2 阳性对照（阶段 B：真实 IME 切换顺序 END_SESSION → FOCUS_OUT(PROFILE_DEACTIVATED)）

```
2026-11-21 09:13:16.620][server] focus_in profile presence=present
2026-11-21 09:13:18.784][server] remove_session active presence=absent
2026-11-21 09:13:19.013][server] focus_out param=1 presence=absent
```

判据汇总：

```
B_FOCUS_IN_PRESENT count=1 (expected >= 1)
B_NORMAL_PATH_ABSENT count=1 (expected >= 1)
B_FOCUS_OUT_ABSENT count=1 (expected >= 1)
B_RECLAIM_LINES count=0 (expected 0)
B_ORPHAN_KEPT_LINES count=0 (expected 0)
B_PANEL_FOUND=True B_PANEL_VISIBLE_AFTER=False (expected hidden) B_PANEL_SEEN_VISIBLE=True
B_FOCUS_OUT_LINE=focus_out param=1 presence=absent
```

客户端侧原始输出（同一次运行）：

```
CLIENT_STDOUT mode=--focusout-deactivate exit=0 SESSION_STARTED ipc_id=11 pid=531976 | FOCUS_IN_SENT caps=1 ipc_id=11 | END_SESSION_SENT ipc_id=11 | FOCUS_OUT_SENT caps=1 ipc_id=11 | SCENARIO_DONE ipc_id=11
CLIENT_PANEL mode=--focusout-deactivate seen_visible=True after_found=True after_visible=False
```

⇒ **阳性对照成立**：presence 走 `present → absent`；面板「运行中可见 → focus-out 后不可见」；
**没有**任何 reclaim / orphan_reclaim 行 ⇒ 这条路径与 ADR 0067 的孤儿分支**互不干扰**。

### 4.3 focus-out 单独生效（阶段 C：FOCUS_OUT 先到、END_SESSION 后到）

```
2026-11-21 09:13:20.370][server] focus_in profile presence=present
2026-11-21 09:13:22.535][server] focus_out param=1 presence=absent
```

```
C_FOCUS_IN_PRESENT count=1 (expected >= 1)
C_FOCUS_OUT_ABSENT count=1 (expected >= 1)
C_RECLAIM_LINES count=0 (expected 0 - the session ended normally)
C_ORPHAN_KEPT_LINES count=0 (expected 0)
C_ECHO_AFTER=0 (expected 0 - the session is gone)
C_PANEL_FOUND=True C_PANEL_VISIBLE_AFTER=False (expected hidden) C_PANEL_SEEN_VISIBLE=True
C_FOCUS_OUT_LINE=focus_out param=1 presence=absent
```

⇒ 「会话按正常路径结束」用**三重证据**确认：无 `reclaim orphan session ipc_id=<n>` 行、
无 `remove_session active orphan_reclaim presence=kept` 行、`ECHO_AFTER=0`（该 ipc_id 已无会话）。

### 4.4 普通切窗口（阶段 D：APP_FOCUS_LOST）

```
2026-11-21 09:13:26.190][server] focus_out param=2 presence=kept
```

```
D_FOCUS_OUT_KEPT count=1 (expected >= 1)
D_FOCUS_OUT_ABSENT count=0 (expected 0 - no profile deactivation here)
D_RECLAIM_LINES count=0 (expected 0)
D_ORPHAN_KEPT_LINES count=0 (expected 0)
D_FOCUS_OUT_LINE=focus_out param=2 presence=kept
```

⇒ 切窗口**不**翻 presence，符合产品语义（`kept` ≠ 回归）。
**未取得**面板可见性证据（该场景下 profile 未激活、面板本轮未曾可见）。

### 4.5 孤儿路径对照（阶段 E：突发退出，不发 END_SESSION）

```
2026-11-21 09:13:31.097][server] remove_session active orphan_reclaim presence=kept
2026-11-21 09:13:31.098][server] reclaim orphan session ipc_id=11 client_pid=532592 live=9
```

```
E_RECLAIM_LINE count=1 (expected >= 1)
E_ORPHAN_KEPT_LINES count=1 (expected >= 1)
E_ABSENT_LINES count=0 (expected 0 - the orphan path must not flip presence)
E_RECLAIM_LINE=reclaim orphan session ipc_id=11 client_pid=532592 live=9
E_ORPHAN_KEPT_LINE=remove_session active orphan_reclaim presence=kept
E_CLEANUP=END_SESSION_SENT ipc_id=12
```

⇒ **孤儿分支仍然不翻 presence、不收面板**；与 §4.2/§4.3 的正常路径**形成对照**，
证明本批的「正常路径仍翻 absent」不是「无条件翻 absent」。

---

## 5. 判别力论证（为什么这些日志不是「看着像」）

1. **阴性对照有效**：阶段 A 不发 focus-out ⇒ `focus_out param=` 计数 **0**。
   若 focus-out 消息根本没被服务端处理，这条计数也会是 0 —— 所以阶段 A 单独不足以证明什么，
   它只用来证明「阶段 B 的那行确实是 focus-out 引起的，不是别的路径顺手写的」。
2. **阳性对照有判别力**：阶段 B/C 有 `focus_out param=1 presence=absent`，而阶段 A/D 没有 ⇒
   `focus_out` 行是**由本客户端的 focus-out 消息触发**的，不是环境噪声。
3. **正常 vs 孤儿分支可区分**：阶段 E 切片内 `remove_session active presence=absent` 计数 **0**，
   而阶段 A/B 同类切片内计数 **1** ⇒ 两条分支**行为不同**，「正常路径翻 absent」不是常量。
4. **服务端确实在跑、且是被测版本**：`SERVER pid=43172 path=D:\Program Files\fluxing\weasel\WeaselServer.exe`；
   IPC 管道名取自 `weasel::GetPipeName()`，命令号取自 `include/WeaselIPC.h`。
5. **没有用「状态文案」当判据**（批次 AX 已登记：自绘文案刷新滞后）——
   本批判据全部落在**服务端诊断日志**（服务端自己写的行）与**窗口可见性**（`IsWindowVisible` 语义）上。

---

## 6. 已验证清单

- [x] 正常结束会话 ⇒ `remove_session active presence=absent`（阶段 A/B）
- [x] `PROFILE_DEACTIVATED` focus-out ⇒ `focus_out param=1 presence=absent`（阶段 B/C）
- [x] `APP_FOCUS_LOST` focus-out ⇒ `focus_out param=2 presence=kept`，**不**产生 `param=1`（阶段 D）
- [x] 快捷面板在 `PROFILE_DEACTIVATED` 后由「可见」变「不可见」（阶段 B/C，运行时观测）
- [x] 正常路径**不**触发 ADR 0067 孤儿回收分支（阶段 B/C 无 reclaim / orphan_reclaim 行）
- [x] 孤儿路径**仍**不翻 presence（阶段 E：`orphan_reclaim presence=kept`）
- [x] 正常路径与孤儿路径**可区分**（阶段 E 切片内 absent 计数 = 0）
- [x] 阴性对照（不发 focus-out ⇒ 无 `focus_out param=` 行）
- [x] 本批**未改动** `Release\TestOrphanReclaimE2E.exe`（mtime 仍 2026/11/21 00:41:29，167 936 B）
      ⇒ B-1（`scripts/verify/orphan-gate-export-mtime.ps1`）、B-2/B-3（`scripts/verify/orphan-ipc-chain.ps1`）
      已有的 PASS 证据不受影响
- [x] 未 commit、未 push；`git status` 中本批产物全部为 `??` 未跟踪

## 7. 未验证清单

- [ ] **路径 2：真实 TSF 宿主产生真实 FocusOut**（详见 §7.1）
- [ ] 真实 TSF 侧的消息**发射顺序**（END_SESSION 先、FOCUS_OUT 后）目前仍是**代码阅读推断**
      （`WeaselTSF/WeaselTSF.cpp:114-126`），未在真机 TSF 宿主上取证
- [ ] 阶段 D（`APP_FOCUS_LOST`）下快捷面板「收起」的可见性证据（该场景面板本轮未曾可见）
- [ ] 「面板常驻」开关（`AlwaysShowMode()`）**打开**时的行为（受用户设置约束，本轮未覆盖）
- [ ] 用户在场的人工验收（视觉确认「切走后状态文案变为火流猩不在场」）
- [ ] 非 `kUserApp` 客户端（如 Fluxing 自己的账号窗 `FluxingPhrasesDialog.exe`）触发的
      `PROFILE_DEACTIVATED` 行为（ADR 0032 own-UI 例外路径）—— 本批刻意未覆盖
- [ ] 175% / 200% 缩放等高 DPI 场景下账号页布局（批次 BB 缺陷 C，**与本项无关**，另批处理）

### 7.1 路径 2 还差哪一步（精确缺口）

要做路径 2，必须让**真的 TSF 宿主**加载 Fluxing TIP 并由 TSF 栈发射 FocusOut。具体还差：

1. **写一个真 TSF 宿主**：拿到 `ITfThreadMgr` → `ITfInputProcessorProfileMgr::ActivateProfile({A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A})`
   → 在宿主里建一个可接收输入的窗口/文本控件 → `ITfFocusMgr` 做**真实焦点切换**
   （不是发 IPC 消息，而是让 `weasel.dll` 的 `ITfKeyEventSink::OnSetFocus(fForeground=false)` /
   `ITfActiveLanguageProfileNotifySink::OnActivated` 真正被调用）。
   本机已有 Fluxing TIP 注册：
   `HKLM\SOFTWARE\Microsoft\CTF\TIP\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}`。
2. **逐条核对 API 签名**（**不得凭记忆猜**）：头文件本机存在，可直接核对 ——
   `C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um\msctf.h`
   与 `C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\msctf.h`；
   需核对 `ITfInputProcessorProfileMgr::ActivateProfile` / `ITfFocusMgr::SetFocus` 的真实签名与调用序。
3. **让宿主真的绑到 Fluxing**：当前探针宿主绑的是搜狗 TSF（`Sogou_TSF_UI` / `SoPY_Status` / `SoPY_Hint` / `SoPY_UI`），
   键入进不了 Fluxing ⇒ 必须先激活 Fluxing profile，否则不会产生 Fluxing 的 focus-out。
4. **判词里必须标注**：真实 TSF 宿主侧的 FocusOut 发射**仍未验证**。

---

## 8. 废弃运行目录（勿引用）

| 目录 | 状态 | 原因 |
|---|---|---|
| `reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091248/` | **中断的废弃运行** | 首次运行时探针脚本的 `-WatchPanel` 形参缺陷（`[bool]` 形参不能用 `-Switch` 形式传值）导致在阶段 A 之后中断；已修，**不含有效判据**。目录内已放 `ABORTED.txt` 标记 |
| `reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091312/` | ✅ **权威运行** | `FAILURE_COUNT=0` / `B4_VERDICT=PASS_SERVER_PATH` / `EXIT=0` |

---

## 9. 复现

```powershell
# 1) 构建合成客户端（独立工程，不触碰产品工程）
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestOrphanReclaimE2E\TestFocusOutE2E.vcxproj' `
  /p:Configuration=Release /p:Platform=x64 '/p:SolutionDir=F:\soft\00selfmade\rime_claude' /nologo /v:minimal

# 2) 跑五阶段探针（独立 pwsh 进程；脚本内 exit 只结束该进程）
& pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b4-focusout-path.ps1'

# 3) 看判据
Get-Content 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-orphan-reclaim\b4-focusout-*\console.txt' -Tail 3
```

期望末尾三行：

```
FAILURE_COUNT=0
B4_VERDICT=PASS_SERVER_PATH
EXIT=0
```

---

## 10. 本批工具坑（勿重试）

| 坑 | 现象 | 正解 |
|---|---|---|
| MSBuild 漏 import | `error MSB4057: 该项目中不存在目标"Build"` | `.vcxproj` 末尾必须有 `<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />` |
| `[bool]` 形参不能按 switch 传 | `参数 'WatchPanel' 缺少自变量` | 改 `[switch]$WatchPanel` |
| 阻塞 `WaitForExit` 使面板轮询永不执行 | 面板可见性恒为 False（假阴性） | 轮询 `$proc.HasExited` |
| `Start-Process` 重定向产物在子进程存活期被锁 | `ReadAllText` 抛 IOException | `File.Open(p,'Open','Read','ReadWrite')` 共享读 |
| 面板枚举用 PowerShell 回调 | 作用域丢失 ⇒ 枚举不到 | 改用 C# 方法（`Win32PanelProbe.FindPanel`） |
| 服务端会话表 `size() <= 1` 时不跑孤儿清扫 | 孤儿对照看不到 reclaim | 需要「孤儿会话 + 新会话」两条会话才触发惰性清扫 |
| 诊断日志单文件上限 2 MB，超限后停止追加 | 后续切片为空 | 每阶段用独立基线切片，必要时轮转日志 |

---

## 11. 文件清单

**本批新增（未跟踪，未 commit）**

| 路径 | 类型 | 说明 |
|---|---|---|
| `test/TestOrphanReclaimE2E/TestFocusOutE2E.cpp` | 新增 | 合成 IPC 客户端 |
| `test/TestOrphanReclaimE2E/TestFocusOutE2E.vcxproj` | 新增 | 独立工程（无 rime.lib / 无 PCH） |
| `scripts/verify/b4-focusout-path.ps1` | 新增 | 五阶段探针脚本 |
| `reports/fluxing-bb-orphan-reclaim/b4-focusout.md` | 新增 | 本报告 |
| `reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091312/**` | 新增 | 权威运行证据 |
| `reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091248/ABORTED.txt` | 新增 | 废弃运行标记 |

**本批未触碰（他人并行修改中，已避开）**

`include/FluxingSessionReclaim.h` · `RimeWithWeasel/RimeWithWeasel.cpp` · `test/TestFluxingCloud/TestFluxingCloud.cpp`
· `Release/TestOrphanReclaimE2E.exe`（B-1/B-2/B-3 证据依赖）· `librime/` · IPC 协议定义。

**只读参考**

`include/WeaselIPC.h` · `RimeWithWeasel/RimeWithWeasel.cpp` · `WeaselIPCServer/WeaselServerImpl.cpp` ·
`WeaselTSF/WeaselTSF.cpp` · `WeaselTSF/KeyEventSink.cpp` · `WeaselServer/QuickPanelDialog.cpp` ·
`include/FluxingSessionReclaim.h` · `include/FluxingDiagLog.h`
