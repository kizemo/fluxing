# 批次 BC 收口 handoff —— 孤儿会话回收 B-4 / B-5 闭环（2026-11-21）

> 本文件是**状态正本**（AGENTS.md §9：state, not a diary —— 描述交棒时的状态，不是流水账）。
> 接手会话**先读本文件 + §2 必读文档**，**不要重做** §3 标注为「已闭环」的项。

## 1. 当前状态（截至 2026-11-21 收口）

**已闭环（verified）**

1. **B-1 / B-2 / B-3 孤儿会话回收端到端**（上一轮提交并已 push）：合成 IPC 客户端打真机 1.3.8.0 服务端。
   B-1 判据 = **维护导出文件 mtime** 三段式对照（对照轮 0.4 s 推进 / 残留会话存活时 12 s 零推进 / 回收后 0.4 s 推进 ⇒ `VERDICT=GATE_EXPORT_E2E_OK`）；
   B-2 / B-3 = `scripts\verify\orphan-ipc-chain.ps1`（`reclaim orphan session ipc_id=… client_pid=… live=…` + `remove_session active orphan_reclaim presence=kept` + `presence=absent` 0 次）。
2. **B-4（服务端路径）= PASS**：合成 IPC 客户端直发 focus-out ⇒ 五阶段阴性/阳性对照 `FAILURE_COUNT=0` / `B4_VERDICT=PASS_SERVER_PATH` / `EXIT=0`（详见 §4.1）。
3. **B-5（解析层）= 单元级 PASS**：把「从握手文本解析客户端自报 pid」抽成共享纯函数 `fluxing::session_reclaim::ParseClientPid`（生产与单测同源，R7），新增单测 `TestClientPidParse`（19 条断言）⇒ 云单测 **PASS=611 FAIL=0**（基线 592 ⇒ +19）（详见 §4.2）。
4. **账号页「换钥后果」提示口径**：BB handoff §14.9 里那条「待用户拍板」**已过时** —— 该提示早已落地（ADR 0063 补充），真相源 `WeaselServer\Cloud\CloudAccountUi.h:209` `AccountKeyRotationNoteText()`，控件 `kIdKeyRotationNote = 1035`，1.3.6 / 1.3.7 / 1.3.8 均装机实证（144 dpi 单行排下、不裁）。**无需再问用户**。
5. **测试账号口令**：已回读坐实 + 判别性验证（见 §7）。

**未做 / 未判定**：见 §6。

## 2. 必读文档（全路径）

| 顺序 | 路径 | 为什么读它 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-21-batch-bc-orphan-reclaim-b4-b5.md` | 本文件（状态正本） |
| 2 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-20-batch-bb-account-ui-dpi-scale.md` | 上一批（账号页 DPI / 孤儿回收取证全过程 / §13 三轮探针根因 / §14.9 未闭环清单） |
| 3 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站规则与强制条目 |
| 4 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册（§4 危险区 / §5 checklist / **§9 收尾双产物**） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层（R1–R9 / P1–P8） |
| 6 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | 部署 + NSIS 配方 |
| 7 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |
| 8 | `F:\soft\00selfmade\rime_claude\docs\release\restart-checklist-1.3.6.md` | **B-1…B-5 判据原文**（§2 表，行 50–63）；A-1…A-5 判据（行 40–48） |
| 9 | `F:\soft\00selfmade\rime_claude\docs\adr\0067-orphan-ipc-session-reclaim.md` | 孤儿回收决策（`is_orphan_reclaim` 双判据来源） |
| 10 | `F:\soft\00selfmade\rime_claude\docs\adr\0065-cloud-sync-gate-idle-criteria-and-session-revival.md` | 闸门空闲判据（`include\FluxingMaintenanceGate.h` 的来源） |
| 11 | `F:\soft\00selfmade\rime_claude\docs\adr\0063-account-page-information-architecture-and-state-views.md` | 账号页信息架构（换钥提示的来源） |
| 12 | `F:\soft\00selfmade\rime_claude\docs\adr\0068-account-ui-dpi-proportional-scaling.md` | 账号页 DPI 等比缩放（BB 批） |

## 3. 本批任务进度

| 任务 | 状态 | 结论 / 证据 |
|---|---|---|
| B-1 闸门端到端 | ✅ 已闭环（上轮） | `scripts\verify\orphan-gate-export-mtime.ps1` ⇒ `VERDICT=GATE_EXPORT_E2E_OK` |
| B-2 回收日志字面量 | ✅ 已闭环（上轮） | `reclaim orphan session ipc_id=2493775873 client_pid=207096 live=1` |
| B-3 回收不翻 presence | ✅ 已闭环（上轮） | `remove_session active orphan_reclaim presence=kept` + `presence=absent` 0 次 |
| **B-4 正常 FocusOut** | ✅ **PASS（服务端路径）** | 见 §4.1；⚠️ 真实 TSF 宿主侧发射**未验证** |
| **B-5 旧客户端不回收** | ✅ **PASS（单元级）** | 见 §4.2；⚠️ 真机「旧客户端连真服务端」链路**未验证** |
| 「换钥后旧行不可解是否在账号页提示」产品口径 | ✅ **已落地，无需再问** | `CloudAccountUi.h:209` + `kIdKeyRotationNote = 1035` |
| 测试账号口令查询 + 判别性验证 | ✅ 已完成 | 见 §7 |
| push 授权 | ✅ 已执行 | 3 个提交 → `kizemo/feat/100-cloud-sync`（见 §9） |

## 4. 本批详细结论

### 4.1 B-4 —— 正常 FocusOut：服务端路径 PASS

**方法**：新增合成 IPC 客户端 `test\TestOrphanReclaimE2E\TestFocusOutE2E.cpp`（独立工程，不链产品工程）+ 探针 `scripts\verify\b4-focusout-path.ps1`，打真机 1.3.8.0 服务端（pid 43172，`D:\Program Files\fluxing\weasel\WeaselServer.exe`）。

**五阶段（阴性 / 阳性对照）** —— 权威证据目录 `F:\soft\00selfmade\rime_claude\reports\fluxing-bb-orphan-reclaim\b4-focusout-20261121-091312\`（`console.txt` + `log-slice-A..E.txt`）：

| 阶段 | 做法 | 期望 | 实测 |
|---|---|---|---|
| A | 只 `END_SESSION`，不发 focus-out | 阴性：无 `focus_out` 行 | `A_FOCUS_OUT_LINES count=0` ✅ |
| B | `END_SESSION` → `FOCUS_OUT(PROFILE_DEACTIVATED)` | 阳性：`focus_out param=1 presence=absent` + **面板收起** | `focus_out param=1 presence=absent` ✅；`B_PANEL_SEEN_VISIBLE=True` → `B_PANEL_VISIBLE_AFTER=False` ✅ |
| C | 只 focus-out（不发 END_SESSION） | 面板同样收起 | `C_PANEL_VISIBLE_AFTER=False` ✅ |
| D | `FOCUS_OUT(APP_FOCUS_LOST)` | 切窗口 ≠ 离场 | `focus_out param=2 presence=kept` ✅ |
| E | 突发退出、不发 `END_SESSION` | 孤儿回收**不得**翻 presence | `remove_session active orphan_reclaim presence=kept` + `reclaim orphan session ipc_id=11 client_pid=532592 live=9`，`E_ABSENT_LINES count=0` ✅ |

`FAILURE_COUNT=0` / `B4_VERDICT=PASS_SERVER_PATH` / `EXIT=0`（主会话已回读 `console.txt` 复核）。

**覆盖边界（不夸大）**
- 验的是**服务端 focus-out 处理路径**（`RimeWithWeasel.cpp` 的 `FocusOut` / `remove_session` / 面板收起副作用）；**真实 TSF 宿主侧的 FocusOut 发射未验证**（精确缺口见 `reports\fluxing-bb-orphan-reclaim\b4-focusout.md` §7.1）。
- 「与批次 AW 前**逐字一致**」的对拍**未做** —— AW 期没有留下 presence / 面板的机读基线；本判词依据 = ADR 0066 / 0067 的设计意图 + 阴性/阳性对照。
- 阶段 D **缺面板可见性证据**（该场景面板本轮从未可见）；「面板常驻」开关打开时的行为未覆盖。

### 4.2 B-5 —— 旧客户端（不写 `session.client_pid=`）不得被回收

**改动（全部在生产与单测之间同源，R7）**
- `include\FluxingSessionReclaim.h:67–135` —— 新增共享纯函数 `inline bool ParseClientPid(const std::wstring& body, unsigned long* out_pid)`：返回 true = 至少有一行**合法** pid（出参 = 最后一个合法值）；返回 false = 缺行 / 全非法（**出参原样不动** ⇒ 调用方保持「pid 未知」）。该头**保持全 ASCII**（自证 `nonAsciiCount=0`、8484 B、无 BOM）。
- `RimeWithWeasel\RimeWithWeasel.cpp:1093–1123` —— 握手文本逐行原样收集（到 `"."` 行为止，与改前同边界），循环外**单点调用** `ParseClientPid`。
- `test\TestFluxingCloud\TestFluxingCloud.cpp:3565–3751` —— 新增 `TestClientPidParse()`（19 条断言）：旧客户端 body（**没有** `session.client_pid=` 行）⇒ 解析不出 ⇒ pid 保持 0 ⇒ `OwnerPidKnown` 为 false ⇒ `SelectReclaimableSessionIds` **不选中**（不得误杀）；另含空 body / 空值 / 9 类坏值 / 超 DWORD / 多 pid 行取值。

**🔑 顺手堵住一个真 bug（32 位回绕）**
改前那条「`> 0xFFFFFFFF` ⇒ 非法」在 Windows 上是**死代码**：`unsigned long` 恒 32 位 ⇒ `parsed = parsed*10 + digit` 会**回绕**，超范围值被当成合法 pid（实测 `99999999999` → `1215752191` ⇒ 会拿去探测活会话 ⇒ **可能误杀**）。改后用 64 位累加 ⇒ 严格拒绝 ⇒ pid 未知 ⇒ 永不回收（方向 = 只泄漏、不误杀，正合该头自述的安全条款）。触发需客户端上报 >10 位十进制 pid，正常客户端 `std::to_wstring(GetCurrentProcessId())` 永不产生。
**等价性核对**：仓库外一次性差分夹具 `%TEMP%\fluxing-b5-diff\diff_parse.cpp`（改前内联逐字抄一份 vs 新共享函数），27 条语料 **SAME=26 / DIFF=1**（唯一分歧即上项，且是安全方向）。

**门禁（主会话独立复跑，非只信子会话自述）**
- 云单测：`RESULT: PASS=611 FAIL=0` / `VERDICT: ALL PASS` / `EXE_EXIT=0`（基线 592 ⇒ +19）；`Release\TestFluxingCloud.exe` mtime `09:08:07` ≥ 三份源 mtime（`09:06:31` / `09:07:39` / `09:06:50`）⇒ **非假绿**。
- 生产编译：`RimeWithWeasel.vcxproj` Rebuild Release/Win32 ⇒ **0 错误**（唯一警告 `WeaselUI\WeaselPanel.cpp:167` C4267 为既有）；`RimeWithWeasel.obj` mtime > 源 mtime ⇒ 改动确实编进静态库。
- `scripts\gate\L0-static.ps1` ⇒ **15/15 PASS**（含 `version-consistency version=1.3.8 一致`）。

## 5. 下一批计划（有序 + 理由）

1. **B-4 路径 2：真实 TSF 宿主发射 FocusOut**（唯一能把 B-4 从「服务端路径」升级为「端到端」的一项）。缺口已精确定位在 `reports\fluxing-bb-orphan-reclaim\b4-focusout.md` §7.1：① 探针宿主需写 TSF 激活（`ITfThreadMgr` → `ITfInputProcessorProfileMgr::ActivateProfile({A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A})`）；② API 签名必须**逐条核对本机 `msctf.h`**（本机 `10.0.19041.0` / `10.0.26100.0` 均在），**不得凭记忆猜**；③ 当前宿主绑的是**搜狗** TSF ⇒ 须先激活 Fluxing profile。**排在第一的理由**：它解锁的唯一真机前置，其余项不需要它。
2. **B-5 真机端到端**（旧客户端握手里不写 `session.client_pid=` ⇒ 连真服务端 ⇒ 不被回收）：可复用 §4.1 的合成客户端加一个「不写该行」的旧客户端模式 + `orphan-ipc-chain.ps1` 的 `ECHO` 探活断言。**排在第二的理由**：复用现成资产、成本最低，且能把「单元级 PASS」升为端到端。
3. **AZ §6 第 4 项 `/verify-client-pid`**：需先 `WeaselServer.exe /quit` 抢命名管道（真服务端在跑时 mock 必假红）⇒ 只能在停服窗口做 ⇒ 与用户在场项合并。
4. **AZ §6 第 3 项：已运行 TSF 宿主逐 pid 枚举模块验证**（确认宿主确实加载 `weaselx64.1.3.8.0.0.dll`）。
5. **需用户在场的一批**（见 §6）：A-4/A-5、双机 AC 7 条、restore 端到端、输入延迟两组对照、缺陷 A 反向判据 G-3/G-5。
6. **本批剩余收尾**：`test\TestWeaselIPC\TestWeaselIPC.cpp` 内另有一份测试侧 pid 解析副本 `record_client_pid`，**未统一到共享头** ⇒ R7 同源仍有缺口（可选小改）。
7. **出包**：本批零用户可见变更 ⇒ 未出包、未装机（见 §6 第 10 条）。下一批有用户可见变更时一并出包，本批重构自然带上。

## 6. 未闭环清单（继承 + 本批新增）

1. **175% / 200% DPI 两档真机像素证据** —— 用户 2026-11-20 已拍板「先不切，按未闭环收口」；免注销切档五条路径已实测排除（**别再试**），口径见 BB handoff §12.2 与 `reports\fluxing-bb-dpi-scale\dpi-tier-probe-status.md`。
2. **A-4 / A-5**（未登录态）—— 需用户在场并明确授权登出，探针不自行发起。
3. **AZ §6 第 3 项** —— 已运行 TSF 宿主逐 pid 枚举模块验证。
4. **AZ §6 第 4 项** —— `TestWeaselIPC.exe /verify-client-pid`（需先停服务抢管道）。
5. **双机 AC 7 条 + restore 端到端 + 输入延迟两组对照 + 缺陷 A 反向判据 G-3 / G-5**。
6. **B-4 真实 TSF 宿主侧 FocusOut 发射**（服务端路径已 PASS）。
7. **B-5 真机端到端**（单元级已 PASS）。
8. **`test\TestWeaselIPC` 内的 pid 解析副本**未统一到共享头。
9. **工作区未提交资产**（有意保留，勿 `git add .`）：`scripts\verify\orphan-probe-host.ps1`、`scripts\verify\verify-orphan-session-reclaim.ps1`、`scripts\verify\diag-host-window-enum.ps1`、`docs\design\quickpanel-v6\design-demos\{a-benchmark,b-craft,c-roulette}.html`、子模块 `m librime`。
10. **本批未出包装机**：零用户可见变更（解析层等价重构 + 单测；唯一语义差异只在「客户端上报 >10 位十进制 pid」这一不可能输入上）⇒ 产物仍为 `output\archives\fluxing-1.3.8-installer.exe`（42,967,778 B / sha16 `AB009C20F6304098`），装机服务仍 1.3.8.0（pid 43172，start 07:55:35 —— 该 pid 与上一批记录的 304416 不同，**非本批动作导致**，版本仍是 1.3.8.0），用户数据 `HKCU\Software\Fluxing\Weasel\RimeUserDir = D:\Program Files\fluxing\user1\fluxing` 未动。

## 7. 需用户在场 / 需用户决策

- **测试账号口令（已坐实，供用户自测登录与重置密码）**
  - 真相源：`F:\soft\00selfmade\rime_claude\tmp\AN-register-qq.txt` **第 10 行**（第 12 行是命令样例）。
  - 明文：**`Fluxing-QQ-Probe-2026!`** —— 校验位 `LENGTH=22` / `SHA16=30ED7B7DBB9E858C` / `TAIL4=026!`。
  - 判别性验证：`tmp\login-check-2026-11-19.mjs`（脚本自身从上面那个文件读口令，**不含明文**）⇒ `POST https://api.aiec.fun/v1/auth/login` **status=200**，返回 `account_id=d42756f4-866c-465f-a9d4-aa6d47464426`、`email_verified=true`、`kdf=PBKDF2-SHA256/600000`；D1 `users` 行 `email_verified=1`（`served_by=v3-prod`）。
  - 重置口令端点（ADR 0064）：`POST /v1/auth/password-reset/start` → `POST /v1/auth/password-reset/confirm`（验证码 TTL 600 s；服务端永不见明文口令 / `enc_key` ⇒ 换 `enc_key` 后旧密文永不可解）。
- **需用户在场**：A-4/A-5（授权登出）、双机 AC、restore、输入延迟对照。
- **需用户决策**：本批**无**待拍板产品口径（「换钥提示」已落地，见 §1 第 4 条）。

## 8. 本轮工具坑 / 新增认知（别再踩）

1. **`git push` 必须绕过本机代理**：`git config http.proxy/https.proxy = http://127.0.0.1:7897`，该代理不通 ⇒ `git -c http.proxy= -c https.proxy= push kizemo feat/100-cloud-sync`。首次 EXIT=128 属**瞬时**失败，重跑即成功（不要据此判定分支有问题）。
2. **🔑 推翻 AX 的一条登记**：「`RimeWithWeasel.vcxproj` 无 `/utf-8`」**不成立** —— 该工程 `RimeWithWeasel.vcxproj:42` 有 `<Import Project="..\weasel.props" />`，而 `weasel.props` 带 `<AdditionalOptions>/utf-8</AdditionalOptions>` ⇒ **全工程都有 `/utf-8`** ⇒ `.cpp` 里的中文注释安全（文件为 UTF-8 无 BOM）。`include\FluxingSessionReclaim.h` 仍保持全 ASCII（双保险：它还被别的编译单元包含时不会炸）。
3. **`TestFluxingCloud` 不在 `weasel.sln`** ⇒ 必须显式 msbuild 并核 `exe.mtime ≥ src.mtime`，否则假绿（本批已核）。
4. **子会话产出必须主会话复核 + 自己重跑门禁**（本批已做）：PASS=611/FAIL=0、L0 15/15、ASCII 自证、差分夹具等价性，全部由主会话独立复跑；主会话复核时**抓出**了改前 32 位回绕这个真 bug（子会话自己写进报告）。
5. **合成客户端必须持续按键**：一次性按键 ⇒ idle>10 s 后闸门即放行 ⇒ 探针无判别力。
6. **阴性对照必须存在**：B-4 的 A 阶段（只 END_SESSION 不发 focus-out ⇒ `focus_out` 行 0 次）是 B 阶段「面板确实被收起」这一结论的**判别力来源**；没有它，「见到 `presence=absent`」无法区分「是 focus-out 导致」还是「本来就会消失」。
7. **废弃运行目录要显式标注**：首个 B-4 运行目录因 `-WatchPanel` 形参缺陷中断（无有效判据），删除被工具层拦截 ⇒ 改放 `ABORTED.txt` 标记，避免下一批误引。
8. **交接卡会串项目**：本机 `maintenance\task-state\LATEST.md` 曾被**另一条 PractiLog 会话**覆盖（内容是 `date_bar.dart` / `actlog-Setup-1.0.0`）⇒ 读它之前先看会话 ID 与仓库是否匹配，不匹配就忽略。

## 9. 提交与产物

- **本轮之前已 push**（用户授权）：`5ef9a23`（日志补 client_pid/live）、`86efdab`（合成 IPC 客户端探针）、`78e74f4`（1.3.8 收口文档）→ `kizemo/feat/100-cloud-sync`，推送后 `git rev-list --left-right --count HEAD...kizemo/feat/100-cloud-sync` = `0 0`。
- **本批新提交**（B-4 / B-5 代码 + 文档 + CHANGELOG）：见 §9.1 提交哈希（收口时补写）。
- **产物**：未出包（§6 第 10 条）。

### 9.1 本批提交哈希

- `833d512` —— `fix(fluxing): 孤儿会话回收 B-5 解析层共享化 + B-4 服务端 focus-out 端到端取证`（代码 + 单测 + 探针 + 证据，38 文件 / +2270 −21）。
- 文档收口提交 —— `docs(fluxing): 批次 BC 收口（CHANGELOG 1.3.8 续 + 本 handoff + 接力 prompt）`（= 本文件所在提交，哈希见 `git log --oneline -2`）。
