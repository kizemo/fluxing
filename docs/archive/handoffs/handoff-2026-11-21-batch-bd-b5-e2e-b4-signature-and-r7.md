# 批次 BD · B-5 真机端到端闭环 + B-4 路径 2 签名到手（并否证「会切走搜狗」前提）+ R7 同源改造

> 状态正本。上一批 = [`handoff-2026-11-21-batch-bc-orphan-reclaim-b4-b5.md`](../handoff-2026-11-21-batch-bc-orphan-reclaim-b4-b5.md)。
> 接力 prompt = [`prompt-next-session-2026-11-21-batch-bd-b5-e2e-b4-signature-and-r7.md`](../prompts/prompt-next-session-2026-11-21-batch-bd-b5-e2e-b4-signature-and-r7.md)。
> 会话 ID `mvs_cce05494af384350aaaa73ed2be8637b`。

---

## 0. 一句话结论

BC §5 计划里的三件事全部到手：**B-5 真机端到端 = `PASS_E2E`**（父会话另做了独立复核）；**B-4 路径 2 的判据签名已在「只读、零权限改动」模式下被两轮独立复现**，且 BC 登记的那条阻塞前提（激活 Fluxing profile = 把用户正在用的搜狗切走）**实测不成立**；**R7 同源缺口已补**（`TestWeaselIPC` 的 client_pid 解析副本删除，改调生产侧共享头）。本批**未改产品代码、未改 `librime/`、未改 IPC 协议、未新增依赖、未出包、未装机**；改动**已提交**：`af0195b`（R7）+ `fbf643f`（BD 收口，本批代码 / 探针 / 证据 / 正本）+ 收口后的**正本同步 docs commit**（把「未提交」登记改成本条真值，即含本文件当前版本的 commit），分支 `feat/100-cloud-sync` **尚未 push**（ahead 数以 `git status -sb` 为准，不在此钉死以免自引用失真）。装机版仍是 **1.3.8.0**（`WeaselServer.exe` pid 43172，StartTime 2026-11-21 07:55:35，全程未被 `/quit` 或重启）。

---

## 1. 必读（按序，全路径）

1. 本文件（BD 状态正本）
2. 上一批状态正本 [`handoff-2026-11-21-batch-bc-orphan-reclaim-b4-b5.md`](../handoff-2026-11-21-batch-bc-orphan-reclaim-b4-b5.md) —— 已闭环项清单 + BC §5 计划原文
3. 判据原文 [`docs/release/restart-checklist-1.3.6.md`](../../release/restart-checklist-1.3.6.md) —— A-1…A-5 在 40-48 行，**B-1…B-5 在 50-63 行**
4. B-5 报告 [`reports/fluxing-bb-orphan-reclaim/b5-e2e.md`](../../../reports/fluxing-bb-orphan-reclaim/b5-e2e.md)（11 727 B）+ 证据目录 `reports/fluxing-bb-orphan-reclaim/b5-e2e-20261121-095546/`（`verdict.txt` / `summary.txt`）
5. B-4 报告 [`reports/fluxing-bb-orphan-reclaim/b4-path2-prep.md`](../../../reports/fluxing-bb-orphan-reclaim/b4-path2-prep.md)（15 403 B，§0 结论 / §4.2 已证明·未证明 / §5 RUNBOOK / §6 工具坑）+ B-4 方法论原文 [`reports/fluxing-bb-orphan-reclaim/b4-focusout.md`](../../../reports/fluxing-bb-orphan-reclaim/b4-focusout.md) §7–§10
6. 门禁脚本 [`scripts/verify/b4-tsf-host.ps1`](../../../scripts/verify/b4-tsf-host.ps1)（16 090 B）/ [`scripts/verify/b5-legacy-client-e2e.ps1`](../../../scripts/verify/b5-legacy-client-e2e.ps1)（25 418 B）
7. 工程规则 [`AGENTS.md`](../../../AGENTS.md) §1 §4 §5 §9 · [`CONTEXT.md`](../../../CONTEXT.md) · [`.specify/memory/constitution.md`](../../../.specify/memory/constitution.md) R2/R7/P2/P4

---

## 2. B-5 · 旧客户端（无 `session.client_pid=` 行）不被回收 —— **已闭环（真机端到端）**

- 判据原文 = `reports/fluxing-bb-orphan-reclaim/b5-e2e-20261121-095546/verdict.txt`：
  `B5_VERDICT=PASS_E2E` / `FAILURE_COUNT=0` / `EXIT=0`。
- 关键观测（`summary.txt`）：旧客户端孤儿 `ipc_id=2829320193 pid=637708` 两轮清扫后 `ECHO_after_sweep=2829320193`、`ECHO_after_bare=2829320193`，**仅显式 `END_SESSION` 后才归 0**；现代孤儿 `2829320194 pid=637776` 被清扫后 `ECHO_after_sweep=0`。
  `POSITIVE_CONTROL=True` / `RECLAIM_LINES_FOR_LEGACY=0` / `LEGACY_PID_LINES=0` / `PRESENCE_ABSENT=0`。
  附带观测 `GATE_OBS=ADVANCED_WITH_LEGACY_RESIDUAL` ⇒ 旧残留会话**不堵**同步闸门（只登记，不作 FAIL 判据）。
- **父会话独立复核**（不经子会话切片）：直读 `%TEMP%\fluxing-profile-notify.log` 尾 3000 行 ⇒ `reclaim orphan session` 共 8 条，**含旧 id `2829320193` 的行 = 0 条**。
- 产物新鲜度：`Release\TestFocusOutE2E.exe` 197 632 B / mtime 09:51:07 **≥** 源 09:47:45 ⇒ 排除「工程不在 `weasel.sln` ⇒ 假绿」。
- `-Legacy` 开关非空转：`test/TestOrphanReclaimE2E/TestFocusOutE2E.cpp:199-200` 显式跳过那一行；运行时自证 `legacy ... =0` / `modern ... =1`。
- 单元级基线沿用 BC：云单测 `PASS=611 FAIL=0`（基线 592 ⇒ +19）。
- **已如实登记的 5 条边界**：① 「旧客户端」= 合成客户端省略一行，**不是**真 1.3.4 老 `weaselx64.dll` 混跑 ② 旧孤儿的最终收场机制属代码 + 时间窗推断 ③ 长期不触发维护窗口时旧孤儿驻留多久未测 ④ 双机 AC 7 条 / restore / 输入延迟对照不在本判据范围 ⑤ 按降级条件走 Grep/Read（未用图谱）。

## 3. B-4 路径 2 · TSF 宿主退出触发 `deactivated sent=1` —— **已验收闭环（前提被否证 + 口径已拍板）**

### 3.1 BC 那条阻塞前提 **不成立**

BC 登记「真机执行需用户授权，因为激活 Fluxing profile = 把用户正在用的搜狗切走」。**实测：本机活跃 TSF profile 本来就是火流猩** ⇒ 不存在被切走的搜狗。

- 子会话 dry-run：`ACTIVE_IS_FLUXING=1`、`ACTIVE_KBD_CLSID={A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}`、`ACTIVE_KBD_PROFILE={3D02CAB6-…}`、`ACTIVE_KBD_LANGID=0x0804`（`ITfThreadMgr` 与 `GetActiveLanguageProfile(0x0804)` 两路一致）。
- 父会话另用注册表独立印证：`HKCU\Keyboard Layout\Preload` = `1=00000804 / 2=00000409`；`HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-…}` 的 `Profile` = Fluxing TIP CLSID。**改前改后逐字相同** ⇒ 全程只读、未动用户输入法配置。

### 3.2 判据签名已被**两轮独立复现**

父会话直读原始服务端日志（303 100 B），对时间戳 `10:08:06|10:08:56` 命中 **20 行 = 4 组完整签名**，每组顺序一致：

```
[server] focus_in profile presence=present
[server] focus_out param=2 presence=kept
[server] remove_session active presence=absent
[server] focus_out param=1 presence=absent
[tsf] deactivated sent=1
```

门禁侧同时给出 `SLICE_BYTES=319` / `SLICE_FOCUS_OUT_PARAM1=1` / `SLICE_FOCUS_OUT_PARAM2=1` / `SLICE_REMOVE_SESSION_ABSENT=1` / `SLICE_TSF_DEACTIVATED_SENT1=1` / `SLICE_TSF_DEACTIVATED_SENT0=0` / `SLICE_ORPHAN_RECLAIM_KEPT=0` / `SLICE_RECLAIM_ORPHAN_SESSION=0`（未触发 ADR 0067 分支 ⇒ 判据干净）/ `PROFILE_BEFORE == PROFILE_AFTER` / `B4_VERDICT=PATH2_SIGNATURE_SEEN_IN_READ_ONLY_MODE`。

机制（已在 `b4-path2-prep.md` §4.2 固化）：宿主只要调 `ITfThreadMgr::Activate`，OS 就会把已装 shim 实例化进该宿主；宿主退出即走 `OnSetFocus(FALSE)` + `Deactivate()` ⇒ 自然产出上述签名。**因此路径 2 不需要真机激活新 profile。**

### 3.3 拍板记录（2026-11-21 用户已答，本批就地闭环）

「**宿主退出**」这一触发源已被两轮复现 ⇒ 按 B-4 路径 2 **验收闭环**。
用户 2026-11-21 拍板原话 = 「**按宿主退出口径闭环，不再真跑**」⇒

- **B-4 = 已验收闭环**，判据强度来源 = §3.1 前提否证 + §3.2 两轮完整签名 + 父会话绕开子会话切片直读原始日志的 4 组逐字复现；
- 「用户主动 Alt-Tab 切窗口 / 切到另一个 TIP」这一触发源**不再真跑** ⇒ 登记为 §6 第 1 项的**已知未覆盖面**（既非 FAIL，也不再是待拍板）；
- 该触发源的跑法配方**保留**在 §8（将来若要补证据可直接抄，不必重新摸索），本批**不执行**。

### 3.4 交付物清单（全部经父会话复核）

| 产物 | 大小 / mtime |
|---|---|
| `test/TestTsFocusOutHost/TestTsFocusOutHost.cpp` | 26 609 B（纯 ASCII，无 PCH / 无 ProjectReference / 不链 rime.lib） |
| `test/TestTsFocusOutHost/TestTsFocusOutHost.vcxproj` | 10 928 B，GUID `{7C1F3A94-…}`，**不进 `weasel.sln`（命中数 = 0，符合验收）** |
| `Release/TestTsFocusOutHost.exe` | 166 912 B / mtime 09:59:41 **≥** 源 09:59:25（`FRESH=True`） |
| `scripts/verify/b4-tsf-host.ps1` | 16 090 B / 10:08:36；静态解析 `ERRORS=0`；默认 dry-run 两轮 `FAILURE_COUNT=0 / EXIT=0 / DESKTOP=Default / DESKTOP_GUARD=PASS` |
| `reports/fluxing-bb-orphan-reclaim/b4-path2-prep.md` | 15 403 B / 10:11:32 |
| `b4-msctf-signatures-raw.txt` / `b4-extract-msctf.ps1` | 14 172 B / 3 235 B |
| `b4-path2-dryrun.txt` `b4-tsf-host-gate-dryrun.txt` `-probe-` `-slice-` `-restore-` | 同目录 |
| dry-run 自证 | `PROBE_VERDICT=PASS` / `THREADMGR=OK` / `TIP_ENUM_COUNT=21` / `FLUXING_TIP_REGISTERED=1` / `PID=653216` |

SDK 签名核对结论（两份 `msctf.h` 均在；实际编译依据 = `10.0.26100.0`）：`ITfThreadMgr` **没有** `Initialize`（只有 `Activate`）；`TF_INPUTPROCESSORPROFILE` **没有** `wlDesc`；本机 SDK **不存在 `ITfFocusMgr`**。

## 4. R7 同源缺口 · `TestWeaselIPC` 的 client_pid 解析副本 —— **已补**

- 改动 = `test/TestWeaselIPC/TestWeaselIPC.cpp`（+16/−11）：新增 `#include <FluxingSessionReclaim.h>`；`record_client_pid()` 内的本地解析（`text.find(key)` 任意位置匹配 + 32 位累加，越界值会**回绕**）删除，改调 `fluxing::session_reclaim::ParseClientPid(text, &value)`。
- 生产侧本已同源：`RimeWithWeasel/RimeWithWeasel.cpp:40`（include）、`:1120`（调用）。共享实现 = `include/FluxingSessionReclaim.h:88-135`，命名空间 `fluxing::session_reclaim`（`:40/:41` 起，`:218/:219` 止）。
- **有意的语义收紧（不得静默）**：解析改为「**行首锚定** + 值须非空纯 ASCII 数字 + 64 位累加并**拒绝 > `0xFFFFFFFF`** + 命中取**最后一个**合法值」。mock 记录行为保持不变：`value` 初值 0、未命中仍向 `client_pid.txt` **追加一行 `0`** ⇒ `client_pid.txt` 的行格式与既有断言不受影响。
- 门禁：MSBuild x64 Release `TWIPC_BUILD_EXIT=0` / `ERR=0`；**WARN=2** = `TestWeaselIPC.cpp:143` 的 `ShellExecute` `HINSTANCE→int` 截断（C4311/C4302，**既有旧代码**，AX 期就在，本批未触碰该行）；`clang-format --lines=22:28 --lines=540:560` ⇒ `DRIFT_LINES=0`；产物 `Release/TestWeaselIPC.exe` 806 400 B / mtime 10:25:11 **≥** 源 10:23:55 ⇒ `FRESH=True`。
- 覆盖口径（如实）：共享解析器由云单测 `TestClientPidParse`（`test/TestFluxingCloud/TestFluxingCloud.cpp:3565-3751`，19 断言）直接断言；`record_client_pid()` 这条**调用路径**只有 `TestWeaselIPC.exe /verify-client-pid` 能驱动，而该项**环境受限未跑**（见 §6）⇒ 本批只到「编译 + 生产侧同源 + 单元级解析断言」，**不得写成端到端已验**。

---

## 5. 本轮新增 / 复认的工具坑（下批别重踩）

1. **`write` 工具报「Successfully wrote」不代表盘上变了**，且紧接的 `read` 可能返回**未落盘的覆盖层**内容 ⇒ 判真实状态只用 `[System.IO.File]::ReadAllText` / `Get-Item`；`write` 覆盖已存在文件**必须先 `read`**。
2. **`write` 写长 PowerShell 脚本会产生跨行拼接的损坏语句** ⇒ 长脚本落盘后必须静态解析验 `ERRORS=0`：`[System.Management.Automation.Language.Parser]::ParseInput($raw,[ref]$null,[ref]$errs)`。
3. **`clang-format --lines=` 的语法是 `--lines=START:END`（冒号），不是 `540-560`**；写成连字符直接 `error: invalid <start line>:<end line> pair`，且此时 stdout 为空 ⇒ 「文本不等」是假红（本批踩过）。
4. **探针/单测 exe 一律落在仓库根 `Release\`**，不在 `test\<proj>\x64\Release\`（三工程 `TestFocusOutE2E` / `TestTsFocusOutHost` / `TestWeaselIPC` 均如此）；且**都不在 `weasel.sln`** ⇒ 必须显式 msbuild 并核 `exe.mtime ≥ src.mtime`，否则「全绿」是假绿。
5. **`ECHO=0` 是双义判据**（走 `WEASEL_IPC_ECHO`，weasel 表条目**或** librime 会话对象任一侧消失都回 0）⇒ 只当第二证据通道，须与日志行计数交叉。
6. 服务端有 4 处 `m_session_status_map.clear()`（`RimeWithWeasel.cpp:155/269/1418/1433`），清表**不留逐会话日志**；`remove_session active presence=absent` 行**不带 ipc_id** ⇒ 只能靠时间戳夹逼归因。
7. **ipc_id 表清空后从 `m_pid+1` 重新起号** ⇒ 跨维护窗口会**重号**（本轮裸清扫复用了 `2829320194`）；按 id 断言必须同时核 `client_pid`。
8. PowerShell 内插串里不能把 `{0:o}` 用在 `$_.LastWriteTime:o` 位置（`意外的标记":o"`）⇒ 改 `.ToString('o')`。
9. `cd`/`Set-Location` 之后再写文件被硬策略拦（`write/delete targets cannot be verified safely`）⇒ **一律绝对路径**，git 用 `git -C <repo>`。
10. 子进程调 pwsh 用 PATH 上的 `pwsh`，不要写 `WindowsApps` 绝对路径。
11. 沿用旧坑：`Copy-Item` 还原保留 mtime ⇒ MSBuild 不重编译，须 touch；`*>` 建的日志是 **UTF-16LE**，须 `-Encoding Unicode` 回读；`msbuild.exe` 不在 PATH，用 `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe`，单测工程须显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`。
12. **子会话自述必须由父会话复核并自己重跑判据**（本批两次实证：B-5 的 `ECHO=0` 双义、B-4 的前提否证）；「侦察 + 产文档」必须派 `worker`（`explore`/`verifier` 契约只读会 `ROLE_MISMATCH`）；派子会话 **`effort` 一律不传**（MiniMax M3 只认 `on`/`off`）。
13. **IPC 消息到达序不得按代码调用顺序推断**（本批实证）：`WeaselTSF.cpp:124` 的 `_PostProfileNotify(kProfileNotifyDeactivated)` 只是**投递**，真实 `FocusOut(PROFILE_DEACTIVATED)` 由**通知线程** `:445` 发射 ⇒ 只要有一侧异步，到达序就由调度决定，读代码只能得到「发出序」不是「到达序」。⇒ 判消息序**唯一可靠姿势 = 回读服务端日志的到达时间戳**（`%TEMP%\fluxing-profile-notify.log`）；本批据此否证了自己上一轮写进 `b4-focusout.md` §2 的散文复述（**错的推断，不是笔误**）。同形推论：合成探针按自己写的顺序发消息，**不等于**覆盖了真实到达序 ⇒ 复现脚本的「顺序一致」声明必须先证明该顺序是被实测的、不是被假定的。

---

## 6. 未闭环清单（含口径，勿误读成 FAIL）

| # | 项 | 口径 |
|---|---|---|
| 1 | B-4 路径 2 的**用户主动切窗口/切 TIP** 触发源真跑 | **用户 2026-11-21 拍板不做**（「按宿主退出口径闭环，不再真跑」）⇒ 登记为**已知未覆盖面**：B-4 本身**已闭环**（宿主退出触发源，§3.1/§3.2），只是「主动 Alt-Tab / 切到另一个 TIP」这一触发源无真机像素级证据。配方留 §8，非 FAIL、**不得再写成待拍板**，BE 也不再重跑 |
| 2 | `TestWeaselIPC.exe /verify-client-pid`（含 §4 调用路径端到端） | **环境受限**：IPC 管道名全局固定 `\\.\pipe\<用户>\WeaselNamedPipe` ⇒ 真服务端在跑时 mock 抢不到管道**必然假红** ⇒ 须先 `WeaselServer.exe /quit` 抢停服窗口 ⇒ 需用户在场/授权。登记「未验证」，不是 FAIL |
| 3 | A-4 / A-5 未登录态 | 需用户**明确授权登出**探针账号；探针不得自行登出 |
| 4 | 175% / 200% DPI 真机像素证据 | 用户 2026-11-20 已拍板「先不切，按未闭环收口」，免注销切档五条路径已实测排除 ⇒ **勿再试** |
| 5 | 双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3/G-5 | 需两台机器 + 用户在场 |
| 6 | push | 本批已按用户拍板提交（`af0195b` R7 / `fbf643f` BD 收口 / 另加一个正本同步 docs commit），领先 `kizemo/feat/100-cloud-sync`；**push 未获授权 ⇒ 未做**，配方见 §8。工作区残留 = ` m librime`（既有脏指针，有意不动）+ 三个早于本批的未跟踪 `scripts/verify/*.ps1` 与 `docs/design/quickpanel-v6/design-demos/*.html`、`release/`（不在本批清单内，有意未提交） |
| 7 | 出包 | 本批无用户可见变更 ⇒ 按 BC 口径**不出包**；现有产物 `output/archives/fluxing-1.3.8-installer.exe`（42 967 778 B / sha16 `AB009C20F6304098`） |

## 7. 下一批（BE）有序计划

1. ~~**提交本批**~~ —— **已在本批收口时完成**（用户 2026-11-21 拍板「现在就提交，拆两个 commit」）：`af0195b` `test(fluxing): R7 同源 —— TestWeaselIPC 改调生产侧 client_pid 解析器`（2 files / +37 −11）+ `fbf643f` `test(fluxing): 批次 BD 收口 —— B-5 旧客户端端到端闭环 + B-4 真 TSF 宿主探针与宿主退出口径 + 消息序定案`（59 files / +2 859 −22）。此后另有一个 `docs(fluxing)` 小 commit，只把本批正本里「未提交」的过期登记改写为提交真值 —— 因为 `af0195b` / `fbf643f` 的哈希必须写进正本，而正本自身是被记录对象，折回前两个 commit 会让 HEAD 哈希变化、文档引用立即失真；用户口径「拆两个 commit」针对本批代码 + 探针 + 证据 + 文档主体，仍成立。提交前已过禁提项判别（`librime` / `weasel.props` / `env.bat` / `*.obj` / `weasel.sln` 零命中），`TestTsFocusOutHost` 新工程**确实未进 `weasel.sln`**。**BE 第一件事改为 push**：`git -C 'F:\soft\00selfmade\rime_claude' -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync`（须用户授权；首次 `EXIT=128` 属瞬时，重跑即成）。
2. ~~按 §3.3 的用户选择收 B-4 路径 2~~ —— **已在本批收口时闭环**（用户拍板「按宿主退出口径闭环，不再真跑」；§3.3 拍板记录 + §6 第 1 项 + `restart-checklist-1.3.6.md` B-4 状态注三处已同步）。**BE 不再重跑该项**。若将来要补「主动切窗口」触发源的证据，直接抄 §8 配方，并记住真跑须在**独占时间窗**内留档 `LOG_BYTES_BEFORE`（门禁并发守卫只按进程名判断，兄弟会话用别的进程驱动同一日志仍会把行落进切片窗口）。
3. 用户在场时跑 `/verify-client-pid`（先 `/quit` 抢管道，跑完确认 `WeaselServer` 重新拉起且 `FileVersion` 仍是装机版）。
4. 若 §4 调用路径端到端通过 ⇒ 把「record_client_pid 走生产解析器」从推断升级为实测，并考虑给 `TestClientPidParse` 补「任意位置匹配」与「DWORD 越界回绕」两条**反向**断言（证明收紧有牙）。
5. A-4/A-5（需登出授权）→ 双机 AC 7 条 + restore + 延迟对照 → G-3/G-5。

## 8. 关键配方（可直接抄）

```powershell
# B-4 门禁（默认只读 dry-run，零权限改动）
pwsh -NoProfile -ExecutionPolicy Bypass -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b4-tsf-host.ps1'
# B-4 真机（用户已拍板不做，仅留配方备用）：$env:FLUXING_VERIFY_ALLOW_PROFILE_SWITCH='1' 然后 …b4-tsf-host.ps1 -RealRun -AllowProfileSwitch
# B-5 端到端
pwsh -NoProfile -ExecutionPolicy Bypass -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b5-legacy-client-e2e.ps1'
# R7 改动后的 TestWeaselIPC 编译（显式 msbuild + 核新鲜度）
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestWeaselIPC\TestWeaselIPC.vcxproj' `
  /p:Configuration=Release /p:Platform=x64 '/p:SolutionDir=F:\soft\00selfmade\rime_claude\' /nologo /v:minimal /m
# 判「同步是否放行」只认导出文件 mtime（状态文案自绘且刷新滞后，不可作判据）
Get-Item 'D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt' | Select-Object LastWriteTime
# push 须绕过本机不通的代理（BC §8.1 实测；首次 EXIT=128 属瞬时，重跑即成）
git -C 'F:\soft\00selfmade\rime_claude' -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync
```

## 9. 状态快照 & 代码指针

- HEAD 起点 `78416a8`（docs BC 收口）→ `833d512` → `78e74f4` → `86efdab` → `5ef9a23` → `9e45174`，与 `kizemo/feat/100-cloud-sync` 同步；本批改动已提交 `af0195b` → `fbf643f`（+ 一个正本同步 docs commit），**尚未 push**。
- 提交后 `git status` 非未跟踪项 = ` m librime`（既有子模块脏，有意不碰）⇒ **产品代码零改动**（两次提交的改动集只含测试代码、探针脚本、reports 证据与文档正本）。
- 装机版 1.3.8.0；`HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` **未动**（绝对禁止：写 `C:\Program Files\fluxing\`、改 `user1\`）。
- 测试账号口令等「具体值」只按真相源 `tmp/AN-register-qq.txt` 第 10 行**当场回读**并带校验位（长度 / sha16 前 16 位 / 尾 4 字符），**禁止凭形态脑补**（2026-11-19 曾因此交付假口令）。
- 代码指针：`test/TestWeaselIPC/TestWeaselIPC.cpp:24-27`（新 include）`:542-557`（同源解析）`:584`（`verify_client_pid_main`）；`include/FluxingSessionReclaim.h:55-65`（`FormatReclaimLogLine`）`:88-135`（`ParseClientPid`）`:142-144`（`OwnerPidKnown`，口径 `pid > 4 && pid != self_pid`）；`RimeWithWeasel/RimeWithWeasel.cpp:40/1093-1123/1120/1248-1286/608/659`；`WeaselTSF/WeaselTSF.cpp:114-126` + `:426/:445` —— **真机到达序已定案为「三条夹心」**：`:124` 只是**异步投递** `_PostProfileNotify(kProfileNotifyDeactivated)`，真实的 `FocusOut(PROFILE_DEACTIVATED)` 由**通知线程** `:445` 发射（**早于** `:125 m_client.EndSession()`），服务端实到序 = `focus_out param=2` → `remove_session` → `focus_out param=1`（四组逐字一致，§3.2）⇒ **先前两种口径同时作废**：`b4-focusout.md` §2「先收 END_SESSION、后收 FOCUS_OUT」是**错的推断，不是笔误**；`b4-path2-prep.md` §4.1「升级为有实测支持」**只对 `param=1` 成立** ⇒ 两处均已就地更正；`WeaselServer/Cloud/CloudAccountUi.h:209`（换钥提示文案，真相源）/ `:159`（缺陷 C 的未缩放 `36`）。
- ADR：`0067` 孤儿会话回收 · `0065` 闸门空闲判据（10 s / 组合串 60 s）· `0066` 账号窗槽位状态机 · `0063` 账号页信息架构 + 换钥提示 · `0068` 账号页 DPI 等比缩放。
- 证据目录：`reports/fluxing-bb-orphan-reclaim/b4-focusout-20261121-091312/`（B-4 权威）；`...091248/` **已废弃**、内有 `ABORTED.txt`，**勿引用**。
- 配方脚本：`scripts/verify/orphan-ipc-chain.ps1`、`scripts/verify/orphan-gate-export-mtime.ps1`。
