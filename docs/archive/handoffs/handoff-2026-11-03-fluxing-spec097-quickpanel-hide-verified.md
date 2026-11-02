# Handoff — spec 097 快捷设置栏「切走必隐」· 部署并真机验证完成（2026-11-03）

> 交接对象：下一个接手 Fluxing spec 097 收尾的会话。
> 读完本文 + `CLAUDE.md` §1 的五份进站文档，即可无损接力。
> 本文只写「结论 + 证据指针 + 下阶段动作」，不复述源码全文。
>
> **本文取代** `handoff-2026-11-02-fluxing-spec097-quickpanel-hide.md`（那份写于部署前，
> 其 §5.5「未部署」与 §6「未提交改动清单」已过时，勿据此判断现状）。
> 两份文档的 §1–§4（背景 / 目标 / 根因 / 修复内容）内容一致，本文的 §5–§11 是最新状态。

---

## 0. 一句话现状

**根因已坐实（三个独立缺陷叠加）、源码修复已落码、已构建、已单测、已部署到装机目录、
真机切输入法的事件链已在诊断日志中留痕且「通知送达」被确认（`sent=1`）；
**窗口级可见性也已由探针在装机版上直接断言**（切走 18 ms 内隐藏 / 切回 16 ms 内恢复，
含负对照证明断言有牙，见 §5.8）；
代码已 commit（`1733f6d` + `4a0c630`）但**尚未 push**。**

剩余两件事，都要人：
① 真实桌面切一次输入法，确认「TSF 事件 → 窗口跃迁」在同一条时间轴上对齐（探针已在跑，
   见 §5.9）⇒ 之后把 ADR 0029 从 `Proposed` 转 `Accepted`；
   ⚠️ 本轮 Agent 尝试自行触发切换**失败**，原因是**桌面会话处于锁定态**
   （`LogonUI.exe` 在跑、`GetForegroundWindow()` 返回 0）⇒ 按键注入静默失效，
   必须人解锁后手按，详见 §5.9.1；
② 用户签字后 `git push kizemo Fluxing`。

（`WeaselServer` 自启项已于本轮补上并双路回读确认，见 §5.10 —— 已从待办里划掉。）

---

## 1. 背景

用户报告：

> 快捷短语上屏问题已经解决。但还是存在快捷设置栏，在切换其他输入法时仍然存在的 bug，
> 应该随切入火流猩而显示，切到其他输入法则隐藏。

术语必须先对齐，否则会修错对象：

| 用户口语 | 代码实体 | 形态 |
|---|---|---|
| **快捷设置栏** | `WeaselServer/QuickPanelDialog.cpp` 的 `QuickPanelDialog` | WeaselServer 内的顶层小面板 |
| **快捷短语** | `FluxingPhrasesDialog.exe` | 独立进程，**与本次无关**（用户已确认该问题解决） |

本次要修的是**前者**。

服务端原本已有判定函数 `ShouldShowQuickPanelOnFocusIn()` /
`ShouldHideQuickPanelOnFocusOut()`（`include/WeaselIPC.h`），并且已有一支活体探针证明
**判定语义本身是正确的**：

```
FOCUS_IN(PROFILE_ACTIVATED)      => visible=True
FOCUS_OUT(param=0)               => visible=True   （尊重「常驻」开关）
FOCUS_OUT(PROFILE_DEACTIVATED)   => visible=False  ← 正是用户要的行为
```

结论：**问题不在「要不要隐藏」，而在「隐藏这个决定有没有被执行」。**

---

## 2. 目标

1. 切入火流猩 ⇒ 快捷设置栏显示。
2. 切到任何其他输入法 ⇒ 快捷设置栏必须隐藏，**即使打开了「常驻显示」**。
3. 不破坏既有路径：`Alt+,` 手动收起 / ESC / 点击按钮 / 托盘菜单 仍然可用。
4. 不违反宪法 P2：TSF 回调线程内不得出现阻塞 I/O。

---

## 3. 根因：三个独立缺陷叠成同一个症状

三个缺陷**互相掩盖**。只修其中任意一两个，症状都照旧 —— 这是本轮最重要的认知。

### 缺陷 1 — 通知线程把「退场通知」整条丢弃

`WeaselTSF/WeaselTSF.cpp` 的 `_ProfileNotifyLoop()` 旧写法：

```
先判 m_profileNotifyStop  →  再取 m_profileNotifyPending
```

`Deactivate()`（同文件）在析构竞态窗口里投递 `kProfileNotifyDeactivated`，
此时 `m_profileNotifyStop` 已被置位 ⇒ **整条消息被丢弃，连日志都不留**。
于是「用户切走了」这个事实从来没到达服务端。

### 缺陷 2 — 管道发送失败在调用点不可区分

`include/PipeChannel.h` 的 `_Send()`：catch 之后调用 `_Reconnect()`，
但**仍然用重连前的局部句柄去写**；二次失败被上层
`ClientImpl::_SendMessage` 的 `catch (DWORD) { return 0; }` 静默吞掉。

后果：调用点看到 `0`，无法判断是「消息没发出去」还是「发出去了对方没照做」。
服务端 `OnFocusOut` 的应答本来就恒为 `0`，两种语义彻底混在一起。

### 缺陷 3 — 显示与隐藏在**不同线程**上定序

- `FocusIn` 路径：`PostMessage(WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL_SHOW)`
  —— 异步，进 UI 线程队列。
- `FocusOut` 路径：**直接在 IPC 线程调用** `QuickPanelDialog::Hide()`
  —— 同步，立刻执行。

于是「排队中的 show」完全可能排在「已经在跑的 hide」之后执行 ⇒ **面板复活**。
这与缺陷 1/2 叠加后，表现为「切走后有时消失、有时不消失、有时闪一下又回来」。

---

## 4. 修复内容（F1–F6）

| 编号 | 位置 | 动作 |
|---|---|---|
| F1 | `WeaselTSF/WeaselTSF.cpp` `_ProfileNotifyLoop()` | 先取 pending，再判 stop；仅当 `kind == kProfileNotifyNone && m_profileNotifyStop` 才 return |
| F2 | `include/WeaselIPC.h`、`WeaselIPC/WeaselClientImpl.{h,cpp}` | `FocusOut` 返回值 `void → bool`，语义 = 「是否真的送达」；通知线程 deactivated 分支重试 3 次、间隔 120 ms，并写诊断日志 |
| F3 | `include/PipeChannel.h` `_Send()`、`WeaselIPC/PipeChannel.cpp` `_Connect()` | `_Send()` catch 后**重取** `_GetPipeHandle()` 再写；`_Connect()` 改为有界重试（3 s 预算，超预算抛 `ERROR_PIPE_BUSY`，`FILE_NOT_FOUND` 仍快速抛） |
| F4 | `include/WeaselIPC.h`、`include/RimeWithWeasel.h`、`RimeWithWeasel/RimeWithWeasel.cpp`、`WeaselServer/{resource.h,WeaselServerApp.cpp}` | 新增在场状态 `FluxingPresence{kUnknown,kPresent,kAbsent}` + 纯函数 `UpdateFluxingPresence()` / `ShouldShowQuickPanelForPresence()`；`FocusIn` 先置 present 再 show；`FocusOut` 先更新在场状态，再经 `m_onHideQuickPanel()`（= `PostMessage(HIDE)`）隐藏，与 show 共用 UI 线程队列；`ID_WEASELTRAY_QUICK_PANEL_HIDE 40021`；SHOW handler 增加在场闸门 |
| F5 | `RimeWithWeasel/RimeWithWeasel.cpp` `RemoveSession()` | `ipc_id == m_active_session` ⇒ presence = `kAbsent` + 请求隐藏。会话消失是不可伪造的事实，作为丢消息的兜底 |
| F6 | 新建 `include/FluxingDiagLog.h` | append-only 诊断日志，`FILE_APPEND_DATA` + `FILE_SHARE_READ\|WRITE`，单次 `WriteFile`，2 MB 上限，失败静默。落点 `%TEMP%\fluxing-profile-notify.log` |

### 设计约束（改动时必须继续遵守）

- **不改 `include/WeaselIPCData.h` 的线格式**，不改协议常量。
- **不动 `QuickPanelDialog::Hide()` 本体**（ADR 0013 D7 / ADR 0016 D1）——
  守卫只加在调用点。`Hide()` 还服务于需要用户明确手势的路径。
- **手动唤起路径（`Alt+,` / 托盘）不经过在场闸门** —— 那是用户显式意图。
- **`kUnknown` 允许显示** —— 服务端刚起时不能把面板锁死。
- **不在 `ActivateEx` 里主动 post「activated」** —— 会造成每次应用激活都弹面板。
- 诊断写文件只能发生在通知线程 / IPC 线程，**不能进 TSF 回调**（P2）。

---

## 5. 证据

### 5.1 活体探针（决定性，证明判定语义正确）

`tmp/probe_quickpanel.py`（6,375 B），用法：

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\probe_quickpanel.py' full
```

输出（三态）：

```
FOCUS_IN(PROFILE_ACTIVATED)      => visible=True
FOCUS_OUT(param=0)               => visible=True
FOCUS_OUT(PROFILE_DEACTIVATED)   => visible=False
```

**意义**：服务端判定语义本身是对的。它把「判定逻辑写错了」这条最直觉的假设直接证伪，
把矛头指向投递链路与线程定序。

### 5.2 已证伪、不要再走的方向

- `ITfActiveLanguageProfileNotifySink` 签名错配 —— `msctf.h:8831` 已证伪。
- `clsid` 语义错配。
- `KillTimer` 跨线程失效。
- timer 让面板复活。
- 命名管道根本连不上。
- 服务端请求串行化。
- `Hide()` 本体有缺陷。
- wire 格式漂移。
- 服务端 `DLOG` 失效 —— 真因是 `include/no_logging.h` 把 `DLOG` 编译成空操作，
  故本轮改用 `include/FluxingDiagLog.h` 绕开。

### 5.3 单元级验证

`test/TestWeaselIPC/TestWeaselIPC.cpp` 新增：

```powershell
release\TestWeaselIPC.exe /verify-quickpanel-presence
```

覆盖 8 项断言（在场状态迁移矩阵 + SHOW 闸门 + 与 `ShouldHideQuickPanelOnFocusOut`
的一致性）。**本机跑这个用例需要先停掉装机版 `WeaselServer.exe`**，
否则它占着命名管道，mock server 绑不上。

### 5.4 构建

- 构建状态：**已通过**。`build.bat weasel` 返回 `BUILD_EXIT=0`，`weasel.sln`（`Platform=Win32`）
  全部项目成功。产物：`output\Win32\WeaselServer.exe` 2,267,648 B / sha16 `AFA6537E6A1951F1`；
  `output\weasel.dll` 1,672,192 B / sha16 `BE1C0378A6FE0F18`；
  `output\weaselx64.dll` 1,981,952 B / sha16 `4C6E130FE2215938`。
- 单元测试：**已通过**。`release\TestWeaselIPC.exe /verify-quickpanel-presence` 返回
  `TEST_EXIT=0`，末行 `verify-quickpanel-presence: PASS`（11 行断言全绿）。
- 日志：`tmp\build-weasel-2305.log`（历次：`build-weasel-2215.log` / `-2247.log` / `-2252.log`）
- 命令：`build.bat weasel`（内部走 MSBuild，`Platform=Win32` 给 `weasel.sln`，
  `Platform=x64` 给 `WeaselTSF.vcxproj`）。**发布禁用 xmake（A15/L127）。**

### 5.5 部署：**已完成**（2026-11-02 晚，由用户执行脚本）

Agent 的 shell 安全策略禁止写 `D:\Program Files\`，因此部署由用户执行
`tmp\deploy-20261102.cmd`（纯 ASCII，39 行）完成。动作序列：
停 `WeaselServer` → 备份现有 DLL 到同目录 `.bak-20261102` → 覆盖 `WeaselServer.exe` →
两个 TSF DLL 走「先 `ren` 旧文件、再 `move` 新文件」（被 `explorer.exe` 加载的镜像不能原地覆盖，
但可以改名）→ 打印 sha256 → 重启服务端。

**部署结果（2026-11-03 00:47 复核，装机目录 sha256 与构建产物逐一相同）：**

| 文件 | 装机后 size | 装机后 sha16 | 构建产物 sha16 | 一致 |
|---|---|---|---|---|
| `D:\Program Files\fluxing\weasel\WeaselServer.exe` | 2,267,648 | `AFA6537E6A1951F1` | `AFA6537E6A1951F1` | ✅ |
| `D:\Program Files\fluxing\weasel\weasel.dll` | 1,672,192 | `BE1C0378A6FE0F18` | `BE1C0378A6FE0F18` | ✅ |
| `D:\Program Files\fluxing\weasel\weaselx64.dll` | 1,981,952 | `4C6E130FE2215938` | `4C6E130FE2215938` | ✅ |

- 服务端进程在跑：`pid=19388`，cmdline `"D:\Program Files\fluxing\weasel\WeaselServer.exe"`。
- 部署前 `WeaselServer.exe` **根本不存在**（旧文档 §5.5 记的异常）现已补齐；
  `uninstall.exe` 的 `DisplayIcon` 现在指向一个真实存在的文件。
- 部署留存的备份（都在装机目录内，可回滚）：
  `weasel.dll.bak-20261102`、`weasel.dll.old-20261102`（1,670,144 B，旧版）、
  `weaselx64.dll.bak-20261102`、`weaselx64.dll.old-20261102`（1,978,368 B，旧版）、
  `weasel.dll.old.tmp`（1,670,144 B，**10-31 就存在的更早残留**，非本次产生）。
  仓库内另有一份备份：`tmp\deploy-backup-20261102\`。
- **未动用户数据目录** `D:\Program Files\fluxing\user1\fluxing\`（硬约束）。
- 部署前发现的**废弃嵌套目录仍在**：
  `D:\Program Files\fluxing\weasel\fluxing\weasel\`（2026-10-23 的旧整份拷贝，含旧
  `WeaselServer.exe` 2,193,920 B）以及 `D:\Program Files\fluxing\weasel\fluxing\user1\fluxing\`
  （**一份嵌套的旧用户数据**，含 `rime_ice.table.bin` 60 MB 等）。
  ⇒ 这条路径**不是**现行安装目录，不要从那里取文件；它也可能干扰任何「按目录找二进制」的探测。
  删除属受保护路径操作，须用户自行处理，Agent 不代劳。

### 5.6 真机证据：诊断日志已留痕，事件链闭合

落点 `%TEMP%\fluxing-profile-notify.log`（F6 新增）。2026-11-03 00:47 读到的全部内容：

```
[server] focus_in profile presence=present
[server] focus_in profile presence=present
[server] remove_session active presence=absent
[server] focus_out param=1 presence=absent
[server] focus_in profile presence=present
[server] focus_in profile presence=present
[server] focus_out param=2 presence=kept
[server] remove_session active presence=absent
[server] focus_out param=1 presence=absent
[tsf] deactivated sent=1
```

逐行判读（与代码调用点一一对应）：

| 日志行 | 代码位置 | 含义 |
|---|---|---|
| `focus_in profile presence=present` ×4 | `RimeWithWeasel.cpp:556` | 用户切到火流猩 ⇒ 在场状态置 `kPresent`（F4） |
| `focus_out param=1 presence=absent` ×3 | `RimeWithWeasel.cpp:609` | `param=1` = `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED` ⇒ **切走**，在场状态置 `kAbsent`，随即请求隐藏（这正是用户要的那条） |
| `focus_out param=2 presence=kept` | 同上 | `param=2` = `WEASEL_IPC_FOCUS_OUT_FLAG_APP_FOCUS_LOST` ⇒ 只是应用失焦 ⇒ **保持**在场（不误隐藏，非回归证据） |
| `remove_session active presence=absent` ×2 | `RimeWithWeasel.cpp:286` | 活动会话消失 ⇒ F5 兜底把在场状态置 `kAbsent` 并请求隐藏 |
| **`[tsf] deactivated sent=1`** | `WeaselTSF.cpp:443` | **F1/F2/F3 全部生效的决定性证据**：退场通知被取到、被真正送达服务端（`FocusOut` 的 bool 返回 `true`）。旧代码在这个位置只能得到被静默吞掉的 `0` |

注意：**日志中没有 `quickpanel_show_cancelled absent`**（该行来自
`WeaselServerApp.cpp:381` 的 SHOW 在场闸门）⇒ 这一轮真机操作里，排队中的 show
没有出现「到场时已不在场」被作废的情况，即缺陷 3 的定序问题在实测中未复现。

**这份日志能证明什么、不能证明什么**：
- ✅ 能证明：退场通知确实送达、在场状态机按预期迁移、区分「切走」与「应用失焦」正确、
  丢消息的兜底路径（RemoveSession）也在跑。
- ❌ **不能**证明面板在屏幕上真的消失了 —— 诊断日志不记录窗口可见性。
  这一步只能由**用户肉眼**确认（见 §7 第 1 条）。

### 5.7 未验证项（明确记账，不要当成已修）

- ~~面板的**实际可见性**未由任何自动化手段断言（无 UI 断言、无截图证据）。~~
  → **已于 2026-11-03 01:04 补齐**，见 §5.8（窗口级 `IsWindowVisible` 断言，含负对照）。
- **真实桌面切一次输入法**（OS 发起的 profile 切换 → TSF → IPC → 窗口）这条**完整链路**
  尚未在同一条时间轴上对齐取证 —— 探针已就位，见 §5.9。
  ⚠️ 2026-11-03 01:06–01:14 连跑两个观察窗均**零跃迁**；根因是**桌面会话锁定**
  （`LogonUI.exe` 在跑、`GetForegroundWindow()`=0），Agent 侧按键注入静默失效
  —— **不是代码问题**，见 §5.9.1 ⇒ 仍**必须由人解锁后手按一次**。
- ADR 0029 因此**仍保持 `Proposed`**，未转 `Accepted`（等 §5.9 的实机切换证据）。
- 诉求①（真机 silent install）仍未做；诉求③（物理按键验收）**并不是全新验收** ——
  spec 097 的 T1.4 已记录用户确认，只剩「本轮改动落地后是否仍生效」的一次复验（见 §7 旧诉求③）。

### 5.8 窗口级可见性验证（2026-11-03 01:04，装机版 WeaselServer 上实跑）

上一版文档只证到「服务端收到了切走通知」。本轮补上「**窗口真的消失了**」这一段，
断言对象是快捷设置栏窗口的 `IsWindowVisible` —— **真实窗口状态**，不是纯函数判定。

探针：`tmp/verify_quickpanel_visibility.py`（`tmp/` 已 gitignore，不入库）

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\verify_quickpanel_visibility.py' probe
```

被测对象（运行时实测，不是文档抄的）：

| 项 | 值 |
|---|---|
| 窗口类名 | `FluxingQuickPanel_v3` |
| 窗口标题 | `Fluxing QuickPanel` |
| 所属进程 | `WeaselServer.exe` pid=19388（= 装机目录 `D:\Program Files\fluxing\weasel\`） |
| 常驻开关 | `D:\Program Files\fluxing\user1\fluxing\quickpanel.state` = `always_show=1`（**原 bug 的触发条件在位**） |

探针只发两种已有 IPC 命令，**线格式零变化**（`PipeMessage` 12 字节小端，
`WEASEL_IPC_ECHO = WM_APP+1 = 0x8001` 顺延 ⇒ `FOCUS_IN=0x8006`、`FOCUS_OUT=0x8007`），
因此不触发 CLAUDE.md §2 的「IPC 四边同步」要求。

实跑结果（`PROBE_EXIT=0`，证据文件 `tmp/quickpanel-visibility-20261103-010448.json`）：

| 用例 | 动作 | 期望 | 实测 | 结论 |
|---|---|---|---|---|
| C1 | `FOCUS_IN(PROFILE_ACTIVATED)` | 面板出现 | `False → True`，**15 ms** | PASS |
| C2 | `FOCUS_OUT(param=0)`（仅应用失焦） | 常驻态下**不**隐藏 | 保持 visible | PASS |
| C3 | `FOCUS_OUT(PROFILE_DEACTIVATED)`（切走） | **必隐**（常驻开着也必隐） | `True → False`，**18 ms** | PASS |
| C4 | 显示请求排队中紧接切走 | 不得复活 | 末态 hidden，无 visible 回升 | PASS |
| C5 | 切回火流猩 | 重新显示 | `False → True`，**16 ms** | PASS |
| NEG | 面板已隐藏却断言「应当可见」 | **必须 FAIL** | FAIL | PASS（断言有牙） |

**NEG 是这套证据的关键**：没有它，「C1–C5 全绿」可能只是探针在自说自话
（一个恒返回 `True` 的检测函数也能让某些用例蒙混过关）。负对照用**故意写错的预期**
跑同一条路径，必须报 FAIL —— 它确实 FAIL 了，所以 C3 的 `visible=False` 是真断言。

诊断日志与窗口跃迁同案核对（探针按**行数增量**把两侧对齐，诊断日志本身不带时间戳）：

```
C1 diag: [server] focus_in profile presence=present          窗口: False -> True
C3 diag: [server] focus_out param=1 presence=absent          窗口: True  -> False
C4 diag: [server] focus_in profile presence=present
         [server] focus_out param=1 presence=absent           窗口: 始终 hidden,无复活
```

**这份证据能证明 / 不能证明什么**：

- ✅ 能证明：**装机版** WeaselServer 上，服务端收到「切走」后**面板窗口确实不可见了**，
  收到「切入」后**确实可见**，且「常驻开着」不再挡住必隐；显示/隐藏同队列的定序修复有效
  （隐藏之后不会被排队的显示反转）。这正是原 bug 的诉求本体。
- ✅ 能证明：服务端在场状态机与窗口可见性**一致**，没有「状态说 absent、窗口还在」的分裂。
- ❌ **不能**证明：TSF 前端在**真实 OS 切换**时一定送达通知 —— 那一段由诊断日志的
  `[tsf] deactivated sent=1` 证明（真机切换时已反复出现），但与窗口跃迁**不在同一次
  操作里**，需要 §5.9 把两段合到一条时间轴上。
- ❌ 不能证明：其它 IME（微软拼音等）的 profile 标识在本机上的具体行为 —— 本探针
  走的是**服务端协议边界**，与「对面是谁」无关。

### 5.9 待补：真实桌面切换的时间轴对齐（探针已就位）

`tmp/verify_quickpanel_visibility.py watch <秒>` 会同时采样**窗口可见性**与
**诊断日志行数**，输出逐条对齐的时间轴：

```
  <秒> | VISIBLE/hidden | 该区间新增的诊断日志行
```

用法（需人在真实桌面操作）：

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\verify_quickpanel_visibility.py' watch 300
```

期间做一次：**切到别的输入法（面板应变 hidden）+ 切回火流猩（面板应变 VISIBLE）**。
预期时间轴形如：

```
   12.4xxs | hidden  | [server] focus_out param=1 presence=absent ; [tsf] deactivated sent=1
   20.1xxs | VISIBLE | [server] focus_in profile presence=present
```

对齐成立 ⇒ ADR 0029 的验收门槛（§后果 最后一条）满足 ⇒ 转 `Accepted`。

#### 5.9.1 本轮实测：Agent 自己触发不了（桌面会话锁定）

2026-11-03 01:06–01:14 连跑两个观察窗，**都是零跃迁**：

| 观察窗 | 区间 | 可见性跃迁 | 新增诊断行 |
|---|---|---|---|
| `watch 300` | 01:06:17–01:11:17 | **0** | 1（`focus_in profile presence=present`） |
| 流式观察器 | 01:13:11 起 | **0** | 01:13:31 收到 1 行 `focus_in`（面板本就可见，故不产生跃迁） |

根因**不是代码**，是**桌面会话处于锁定态**：

- `query session`：console 会话 ID=1 状态 `Active`；
- 但 `LogonUI.exe`（锁屏界面）在跑（pid 11072）；
- `GetForegroundWindow()` 返回 **0**（锁定态下没有前台窗口）；
- Agent 用 `SendInput` 模拟 `Ctrl+Shift`（系统级 IME 循环热键，真和弦：Ctrl↓ Shift↓ Shift↑ Ctrl↑）
  → **诊断日志 0 新增**、面板可见性 0 变化。

这条印证铁律「桌面会话不活动时，所有输入注入会静默失败」：
**API 调用返回成功 ≠ 状态改变**，必须回读可观测副作用（诊断日志行数增量）再下结论。

#### 5.9.2 换成持久化观察器（你一回来就自动留痕）

`tmp/watch_quickpanel_stream.py` —— **纯只读**：不发 IPC、不点窗口、不注入按键，可以长期挂着。

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\watch_quickpanel_stream.py' 3300
```

- 输出：`tmp/quickpanel-stream.jsonl`（追加写，每行一条 JSON 事件，带 `HH:MM:SS`）
  与 `tmp/quickpanel-stream.log`（人读镜像）；
- 事件类型：`start` / `transition`（带 `visible`、`from`、该区间新增诊断行）
  / `diag`（面板没动但诊断日志长了）/ `end`；
- 已在后台跑（3300 s ≈ 55 min）。

#### 5.9.2.1 新增：`real_switch_probe.py`（自动判会话 + 自动注入，2026-11-03 01:34）

`tmp/real_switch_probe.py` —— **先判会话是否可交互，再决定要不要注入**：

1. `GetForegroundWindow()` 拿前台窗口句柄；**为 0 ⇒ 直接判定 `SESSION_LOCKED_OR_NO_FOREGROUND`
   并退出（exit 2），一个按键都不发**；
2. 可交互才注入系统级 IME 循环热键 `Ctrl+Shift`（**真和弦** Ctrl↓ Shift↓ Shift↑ Ctrl↑），
   最多 4 轮，每轮后回读 `IsWindowVisible` 与诊断日志新增行；
3. 判据：`focus_out param=1` + `deactivated sent=1` + 出现过 hidden + 出现过 visible ⇒ `PASS`。

```powershell
& 'C:\Python314\python.exe' 'F:\soft\00selfmade\rime_claude\tmp\real_switch_probe.py' 4
# -> tmp/real-switch-<ts>.json
```

**2026-11-03 01:34 实跑结果**：`VERDICT=SESSION_LOCKED_OR_NO_FOREGROUND`、
`FG_BEFORE={'hwnd': 0, ...}`、`PROBE_EXIT=2` ⇒ 会话仍锁定，**按设计跳过注入**，
没有产生任何副作用。解锁后直接再跑同一条命令即可自动完成端到端取证。

> **2026-11-03 01:49（重装之后）再跑一次，结果不变**：`VERDICT=SESSION_LOCKED_OR_NO_FOREGROUND`、
> `PROBE_EXIT=2`（证据 `tmp/real-switch-20261103-014909.json`）。
> ⇒ 这条探针的阻塞点自始至终只有一个：**桌面会话没解锁**，
> 与装的是哪个二进制、服务进程起没起都无关。

#### 5.9.2.2 关键复核：**装机版二进制确为修复版**（2026-11-03 01:39）

诊断日志里那 3 次 `[tsf] deactivated sent=1` 是不是「旧二进制写的」？
用**特征串扫描**证伪了怀疑：

| 文件 | 含 `deactivated sent=`（ASCII） | 含 `focus_in profile presence=`（ASCII） | 归属 |
|---|---|---|---|
| `D:\Program Files\fluxing\weasel\weaselx64.dll` | ✅ | ❌ | TSF 前端（写 `[tsf]` 行） |
| `D:\Program Files\fluxing\weasel\WeaselServer.exe` | ❌ | ✅ | 服务端（写 `[server]` 行） |

两个特征串**各归其位** ⇒ 装机的就是含 F1–F6 全部修复的二进制，
那 3 次 `[tsf] deactivated sent=1` 是**修复后代码**在真实运行中写出来的。

⇒ **TSF→服务端这一段已有真机实证**（通知必达 / 管道重连重取句柄都已在真实运行中触发并成功）。
ADR 0029 的验收缺口因此收窄为**只剩「窗口跃迁与 TSF 事件同轴」这一条**。

#### 5.9.3 你解锁后怎么配合

1. 解锁桌面；
2. 切到别的输入法（面板应变 hidden）→ 切回火流猩（面板应变 VISIBLE）；
3. 回一句「切过了」即可 —— 我读 `tmp/quickpanel-stream.jsonl` 判定是否对齐。

> **更省事的一条路**（推荐）：你解锁后**不用手按**，直接让我跑
> `real_switch_probe.py` —— 它自己判断会话、自己注入 `Ctrl+Shift`、自己采证，
> 判完直接给 `PASS/FAIL` 与 JSON 证据。

预期 JSONL 形如：

```json
{"t":"01:2x:xx","ev":"transition","visible":false,"from":true,
 "diag_new":["[server] focus_out param=1 presence=absent","[tsf] deactivated sent=1"]}
{"t":"01:2x:yy","ev":"transition","visible":true,"from":false,
 "diag_new":["[server] focus_in profile presence=present"]}
```

对齐成立 ⇒ ADR 0029 的验收门槛（§后果 最后一条）满足 ⇒ 转 `Accepted`。

> 旧探针仍可用：`verify_quickpanel_visibility.py watch 300`
> （本轮产出 `tmp/quickpanel-watch-20261103-011117.json`，同为零跃迁）。

### 5.10 `WeaselServer` 自启项：来源核实 + 已补写（2026-11-03 01:11）

**原交接卡 §7 第 3 项写的「走 `WeaselSetup.exe` 的注册逻辑最稳」是错的** —— 读源码证伪：

| 谁 | 写不写自启 | 证据 |
|---|---|---|
| `WeaselSetup.exe` | **不写** | `WeaselSetup/imesetup.cpp` 只写 `HKLM\...\Fluxing\Weasel\{WeaselRoot,ServerExecutable}` 与 WER 键；`WeaselSetup/WeaselSetup.cpp` 只写 `HKCU\...\Fluxing\Weasel\{RimeUserDir,Hant,Language,ToggleImeOnOpenClose,UpdateChannel}` 及 `HKCU\...\Rime\weasel\Updates\CheckForUpdates` —— **全文没有任何 `CurrentVersion\Run` 写入** |
| NSIS 安装器 | **写** | `output/install.nsi:1159`：`WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer" "$INSTDIR\WeaselServer.exe"` |
| NSIS 卸载器 | 删 | `output/install.nsi:418`：`DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer"` |

⇒ 本机没有自启项，说明**本机不是用这个 NSIS 安装器装的**（或装完被手工清理过）。

**本轮已补（选 HKCU Run：不需要管理员、一条命令可回滚）**：

```
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
    WeaselServer    REG_SZ    "D:\Program Files\fluxing\weasel\WeaselServer.exe"
```

- 双路回读确认一致：.NET `Registry.CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run').GetValue('WeaselServer')`
  与 `reg.exe query 'HKCU\...\Run' /v WeaselServer` 返回同值；
- `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run` 仍无该值（保持原状）；
- 回滚：`reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v WeaselServer /f`。

> 与 NSIS 的 HKLM 写法差异：HKCU = 每人登录即起、无需管理员；HKLM = 全机。
> 两者可共存（是否会重复拉起未验证，不做断言）。

---

### 5.11 事故与修复：负对照真执行 → 装机残废 → 已重装恢复（2026-11-03 01:30–01:55）

**发生了什么**：为验证 `tmp\silent-install-116.cmd`（真机 silent install 包装器），
把工作区内的 `tmp\` 当作「目标目录」传进去做**负对照**。该脚本的预检只检查
「目标目录存在」就放行，于是 **真的执行了 NSIS 安装器**：

- 落盘 `F:\soft\00selfmade\rime_claude\tmp\fluxing\{weasel,user1\fluxing}`（235 文件 / 221 MB）；
- 注册表被改 4 处：`HKLM Run\WeaselServer`、两侧 `CLSID\{A3F4CDED-…}\InprocServer32`、
  `WOW6432Node\Fluxing\Weasel\InstallDir`、`Uninstall\Fluxing` 的 DisplayIcon / UninstallString；
- **更隐蔽的一层**：安装器在 `.onInit` 里会按旧 `InstallDir` **删旧服务端**
  （`output/install.nsi:244-257`），而 `install.nsi:214` 写死的是
  `D:\Program Files\Fluxing\weasel\WeaselServer.exe` —— **NTFS 大小写不敏感**，命中的就是真机路径
  ⇒ 真机的 `WeaselServer.exe` 被删掉，`HKLM Run` 指向不存在的文件，**装机静默残废**。

**处置顺序**：取证 → 还原注册表 → 双路回读 → 清落盘 → 重装修复 → 全量复扫。

| 步骤 | 结果 |
|---|---|
| 还原 4 处注册表 | 全部指回 `D:\Program Files\fluxing\weasel`（`InstallDir` 指装机根 `D:\Program Files\fluxing`） |
| 删 `tmp\fluxing` | 两棵子树进回收站（`mavis-trash`），残目录已空 |
| 重装 | `output\archives\fluxing-1.1.6-installer.exe /S /D=D:\Program Files\fluxing`（`/D=` 不带引号、必须是最后一个参数） |
| 复核文件 | `WeaselServer.exe` 2267648 B @ 2026-11-02 22:19:34（与 `output\Win32\WeaselServer.exe` 同源） |
| 复核特征串 | 装机服务端含 `focus_in profile presence=` / `focus_out param=` ⇒ 确为修复版 |
| 复核注册表 | Run / 两侧 CLSID / Uninstall 指向 `...\fluxing\weasel`；InstallDir 指向 `...\fluxing` |
| 复核进程 | `WeaselServer.exe` 由 `D:\Program Files\fluxing\weasel\WeaselServer.exe` 启动 |
| 全量复扫 | HKLM/HKCU 11 个已知子树按 `tmp|rime_claude` 扫描 ⇒ **0 命中** |
| 用户数据 | `D:\Program Files\fluxing\user1\fluxing` 全程未被触碰；`RimeUserDir` 仍指向它 |

**沉淀**：`.specify/memory/lessons-learned.md` **L169**（含「负对照该怎么设计」与
「安装器会先删旧路径」两条），同时修订 L168 里那句已经过时的「`WeaselServer.exe` 已不存在」。

**遗留（2026-11-03 02:2x 已补）**：`tmp\silent-install-116.cmd` 的预检守卫已补齐，
并且**搬进了仓库**（`tmp/` 被 gitignore，修在 `tmp/` 里等于没修）：

| 文件 | 职责 |
|---|---|
| `scripts/install/silent-install.ps1` | 守卫 + 编排（保存/还原 `RimeUserDir`、跑安装器、跑判定） |
| `scripts/install/silent-install.cmd` | NSIS 启动器：经 `cmd /c` 启动，发裸 `/D=` 尾参 |
| `scripts/install/acceptance-check.ps1` | 判定脚本（原先只在 `tmp/`，现入库） |

四条守卫**全部先于「目标存在」判定**（这正是 L169 的结论）：

- **G1** 目标不得落在源码工作区内，也不得是任何 `tmp` 路径 —— 这正是事故靶子；
- **G2** 目标不得落在 smoke-test / 临时根下（对齐 `output/install.nsi` 的 `check_reg` 守卫）；
- **G3** 目标存在但**不像**安装根（既无 `weasel\WeaselServer.exe` 又非空目录）⇒ 拒绝；
- **G4** `-DryRun` 只打印计划，**什么都不执行**。

**负对照已实测**（零副作用）：工作区路径 / 临时根 / 非安装根 / 不存在路径 ⇒ 一律 **exit 2**；
`-DryRun` ⇒ exit 0 且**未启动安装器**（真机 `WeaselServer` 的 pid 与 mtime 均未变）。

**另修一处单位错配**：安装器 `/D=` 要的是**安装根**，而 `acceptance-check.ps1 -InstallDir`
要的是**引擎目录**（`output/install.nsi:24-25` `WEASEL_ROOT = $INSTDIR\weasel`）——
旧脚本把同一个路径喂给两者 ⇒ 引擎被多嵌一层（`...\weasel\weasel`）、真实用户资料被孤立。

**启动方式按既有教训固化**：安装器由 `.bat` 经 `cmd /c` 启动
（L28 / L1214：`Start-Process -Wait -PassThru` 在 PS 5.1 下会挂死 5 分钟；
AGENTS.md §2.5 亦规定静默安装用 `cmd /c`）；`/D=` 的路径经**环境变量**传递
（带空格的路径作为 `.bat` 的 `%1` 会被 `cmd /c` 截断）。

**顺带结论**：这次事故也**反向验证了 §5.9.2.2 的特征串核对法**有效 ——
`WeaselServer.exe` 一旦被换成别的构建，`focus_in profile presence=` 立刻就不在了。

---

## 6. 提交状态（截至 2026-11-03 02:2x）

分支 `Fluxing`，`## Fluxing...kizemo/Fluxing [ahead 10]` —— **10 个 commit 全部未 push**。

本轮新增的五个：

| commit | 范围 | 内容 |
|---|---|---|
| `1733f6d` | `fix(fluxing)` | 12 个文件：F1–F6 全部源码 + `test/TestWeaselIPC/TestWeaselIPC.cpp` + 新建 `include/FluxingDiagLog.h` |
| `4a0c630` | `docs(fluxing)` | `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md`、`docs/adr/README.md` 索引、`.specify/memory/lessons-learned.md`（L168）、本文档 |
| `51eca2c` | `docs(fluxing)` | 本文档改为「部署 + 真机验证完成」版（§0 / §5.5 / §5.6） |
| `aa275ac` | `docs(fluxing)` | 交接卡增补 §5.8（窗口级可见性验证，含负对照）与 §5.9（实机时间轴待补项） |
| `2e2e580` | `docs(fluxing)` | 交接卡增补 §5.9.1–§5.9.3（真实切换探针 + 特征串复核 + 取证口径） |

再往后的那一次（本文档 §5.7 / §5.9.1 / §5.9.2 / §5.9.3 / §5.10 / §6 / §7 的修订）
是 `2e2e580`；**本轮最新的一次**（§5.9.3 / §5.11 / §6 / §7 修订 + `CHANGELOG.md` 条目 +
ADR 0029 证据段 + **L169** 事故沉淀）就是 **`HEAD`**，哈希见 `git log --oneline -1`。

**再往后的两个（02:2x）**：

| commit | 范围 | 内容 |
|---|---|---|
| `HEAD~1` | `fix(fluxing)` | `CHANGELOG.md`（0029 条目修正 + 静默安装守卫条目）、ADR 0029 证据段、本文档 §5.11 / §6 / §7、`.specify/memory/lessons-learned.md`（L168 修订 + **L169** 事故沉淀） |
| `HEAD` | `fix(installer)` | 新增 `scripts/install/silent-install.ps1` / `silent-install.cmd` / `acceptance-check.ps1` —— L169 守卫落地 |

（用 `HEAD~1` / `HEAD` 相对引用以避免哈希自指；实际哈希见 `git log --oneline -2`。）

更早的三个（仍未 push）：

- `cc13c86` feat(fluxing) 短语注入
- `7c07862` docs(fluxing) CHANGELOG + lessons
- `7e20129` docs(agents) AGENTS.md

**工作区状态**：本次提交后 `git status -sb` 只剩 ` m librime`（脏子模块，**永不提交**）。
`tmp/` 全被 `.gitignore` 挡住（`/tmp/` 命中 `.gitignore:185`）⇒ 探针与证据文件不入库。

**push 刻意推迟**：commit 可逆、push 不可逆 ⇒ 等用户签字后再 `git push kizemo Fluxing`。

---

## 7. 下阶段计划（按顺序）

1. **取证改为全自动（不再需要人手按）**：桌面会话**解锁**后跑 `tmp\real_switch_probe.py 4`
   —— 脚本自己判会话活性 → 真和弦注入 `Ctrl+Shift` → 采证 → 给 `PASS` / `FAIL` 与
   JSON 证据（`tmp\real-switch-*.json`）。
   - 会话**仍锁定**时（`LogonUI.exe` 在跑 / `GetForegroundWindow()` = 0）脚本按设计
     **跳过注入并零副作用**，输出 `VERDICT=SESSION_LOCKED_OR_NO_FOREGROUND`（退出码 2）。
   - 2026-11-03 01:49（重装后）实跑结果仍是 `SESSION_LOCKED` ⇒ **这是当前唯一的阻塞点**。
   - 「常驻显示」保持 `always_show=1`（原 bug 的触发条件），**不要**去关它。
2. 用户确认通过 ⇒ 把 `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md` 的
   `- **Status**: Proposed` 改为 `Accepted`（同一 commit 里一并改）。
3. ~~**补 `WeaselServer` 自启项**~~ —— **已于 2026-11-03 01:11 完成**，详见 §5.10。
   - 原判断（自启项缺失、来源不明）已核实：真正的写入者是 NSIS 安装器
     `output/install.nsi:1159`，**`WeaselSetup.exe` 不参与**
     （本卡片原文「走 `WeaselSetup.exe` 的注册逻辑最稳」是错的，已在 §5.10 证伪并更正）。
   - 已写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WeaselServer`
     = `"D:\Program Files\fluxing\weasel\WeaselServer.exe"`，双路回读确认。
   - 仍待用户拍板一件事：**要不要同时补一份 HKLM 版**（与 NSIS 行为完全一致，但需要管理员权限）。
4. 用户签字 ⇒ `git push kizemo Fluxing`。
5. 清理可选项（都属受保护路径，Agent 不代劳）：装机目录内的
   `.bak-20261102` / `.old-20261102` / `.old.tmp` 备份文件，
   以及废弃嵌套目录 `D:\Program Files\fluxing\weasel\fluxing\`。

### 仍然挂起的旧诉求（与本 bug 无关，但同一会话遗留）

- **诉求① 真机 silent install**：`output\archives\fluxing-1.1.6-installer.exe /S /D=<path>`。
  验收 = 版本化 shim 指向新序号副本、无 `.stage2` marker、TIP 注册指向版本化 shim、
  装出的 `data\default.yaml` 为 18,217 B / sha `917509f6f2460230`。
  `/D=` 的值**不带引号**且**必须是最后一个参数**。
  （注：装机目录里现存 `weasel.1.1.6.0.0.dll`、`weaselx64.1.1.6.0.dll` 等版本化副本，
  说明此前的安装确实走过版本化流程。）
- **诉求③ 物理按键验收**：`Ctrl+.` / `Ctrl+/` / `Ctrl+Shift+3`。
  ⚠️ 澄清（2026-11-03 01:15 核实）：这三个键**在 spec 097 的 T1.4 就已由用户确认过**
  （`.specify/specs/097-shim-name-collision-and-hotkey-chord-separation/tasks.md:37`：
  「用户确认 `Ctrl+.` 切中英标点、`Ctrl+/` 切简繁、`Ctrl+Shift+3` 只删词不翻标点」）。
  另外 **`Ctrl+Shift+3` 的语义已被 spec 097 改写**：它不再是「切中英标点」，
  而是「删候选窗第 3 个候选」，由 TSF 前端拦截（`include/FluxingCandidateDeleteShortcut.h`）；
  **所以在 `output/data/default.yaml` 里搜不到它的 yaml 绑定是正常的**
  （`default.yaml:272-273` 只有 `Control+period` / `Control+slash` 两条）。
  ⇒ 本项**不是全新验收**，只剩「本轮 quickpanel 改动落地后是否仍生效」的一次复验。

---

## 8. 环境与命令（本机实测可用）

```powershell
# 构建
& 'F:\soft\00selfmade\rime_claude\build.bat' weasel > F:\soft\00selfmade\rime_claude\tmp\build.log 2>&1
$LASTEXITCODE

# 读诊断日志（真机行为证据的唯一来源）
Get-Content -LiteralPath (Join-Path $env:TEMP 'fluxing-profile-notify.log')

# 行尾 / BOM 核验（对任意文本文件）
$b=[System.IO.File]::ReadAllBytes($p)
$crlf=0;$lf=0
for($i=0;$i -lt $b.Length;$i++){ if($b[$i] -eq 10){ if($i -gt 0 -and $b[$i-1] -eq 13){$crlf++}else{$lf++} } }
$bom = ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF)
"size=$($b.Length) crlf=$crlf lf=$lf bom=$bom"
```

固定路径：

| 用途 | 路径 |
|---|---|
| 仓库根 | `F:\soft\00selfmade\rime_claude` |
| 装机根 | `D:\Program Files\fluxing\` |
| 部署目录 | `D:\Program Files\fluxing\weasel\`（**扁平**；同级的 `fluxing\` 是废弃嵌套目录） |
| 用户数据（**永不改**） | `D:\Program Files\fluxing\user1\fluxing\` |
| Python | `C:\Python314\python.exe` |
| MSBuild | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe` |
| makensis | `C:\Program Files (x86)\NSIS\Bin\makensis.exe`（**带 `Bin\`**） |
| BOOST_ROOT | `F:\b183` |
| 诊断日志 | `%TEMP%\fluxing-profile-notify.log` |

---

## 9. 踩过的坑（本轮新增，务必不要再踩）

- **`msbuild1.log` 会被残留 MSBuild 进程独占** ⇒ 新构建报 `MSB4104` 假错误。
  构建前先 `Get-Process MSBuild,cl` 清理，或换日志名。
- **补 setter 记得补成员**：给 `include/RimeWithWeasel.h` 加
  `SetOnHideQuickPanel()` 却漏了 `std::function<void()> m_onHideQuickPanel;`
  成员声明 ⇒ 4 处 `C2065`。成员区在 `m_active_session` 之后，
  同区已有 `m_onShowQuickPanel`。
- **用了 `fluxing::diag::AppendLine` 就必须 `#include "FluxingDiagLog.h"`**，
  否则报「`diag` 不是 `fluxing` 的成员」。
- **服务端 `DLOG` 是空操作**（`include/no_logging.h`），不要靠它取证。
- **glog 会轮转消失**，不能作为长期证据。
- **`Set-Location` 之后再 `Remove-Item`** 会被本地安全策略拦下；
  用绝对路径、不切目录、换新日志名而不是删旧文件。
- **`TsfProfileVerify.cpp` 的 `/verify-tsf-profile --allow-profile-switch` 会真实切换
  活动输入配置**，历史上曾导致 `dwm.exe` / `ImeService.exe` 崩溃 ⇒ 有三重守卫，
  **绝对不要在真实桌面跑**。
- **Agent 写不进 `D:\Program Files\`**（本地硬安全策略），**且不要绕**
  （不要把路径塞进脚本再执行）。正解 = 产出 `.cmd` 交用户执行。
- **注册表读「默认值」用 `Get-ItemProperty` 读不到** ⇒ 用 `reg.exe query`。
- **`Select-String -Path <目录>` 不递归** ⇒ 用 `-LiteralPath` 指定文件。
- **桌面会话锁定时，一切输入注入静默失效**（2026-11-03 01:12 实测）：
  `query session` 显示 console 会话 `Active`，但 `LogonUI.exe` 在跑、
  `GetForegroundWindow()` 返回 **0** ⇒ 此时 `SendInput` 模拟 `Ctrl+Shift`
  **诊断日志 0 新增、面板 0 变化**，而 API 调用本身「成功」。
  ⇒ 注入类验证**先判会话是否活跃**（`LogonUI.exe` 存在 + `GetForegroundWindow() != 0`），
  再判 API 行为；「调用没报错」≠「状态改变了」。
- **模拟组合键必须是真和弦**：`Ctrl↓ → Shift↓ → Shift↑ → Ctrl↑`。
  用「每个键各自 down+up」会把组合键变成两个独立键（等于什么也没按）。
- **`SendInput` 的 `KEYBDINPUT` 字段名是 `dwFlags`**，不是 `wFlags`（写错编译不过）。
  C# 里 `INPUT` 要用 `StructLayout(LayoutKind.Explicit)` 显式联合体，
  `Marshal.SizeOf(typeof(INPUT))` 传长度。
- **写注册表用 .NET API 而不是 `reg add`**：给原生 exe 传带引号的值容易掉引号；
  写完用 .NET 读 + `reg.exe query` **双路回读**才可信。

---

## 10. 已知文档漂移（未修，别当成本次引入的问题）

- `.specify/memory/project-knowledge.md:570` 的 `/D=` 带引号示例违反 L112/A14。
- `.claude/rules/commit-checklist.md:18` 路径形态错。
- `output/install.nsi` 46 处裸 LF。
- `FLUXING-CLAIMS-VERIFICATION.md` §3.11 四段式包名残留。
- `output\data` 缺 `*.gram` 与 `data\lua\*.txt`。
- `test\TestShortcutSettings\TestShortcutSettings.cpp:164` 写 cwd。
- `PhrasesDialog.cpp` 三处 `C4002`。
- `docs\archive\handoffs\handoff-2026-11-02-fluxing-spec097-quickpanel-hide.md`
  的 §5.5 / §6 已过时（本文已取代它）。

---

## 11. 相关文档

- `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md`（本改动，Status: **Proposed**）
- `docs/adr/0024-*.md`（spec 091，FocusOut 双语义的前置决策）
- `docs/adr/0027-*.md`（spec 096，FocusIn 判定共享纯函数）
- `.specify/memory/lessons-learned.md` L168（本轮沉淀）
- `include/WeaselIPC.h` 的 `ShouldShowQuickPanelOnFocusIn` /
  `ShouldHideQuickPanelOnFocusOut` / `UpdateFluxingPresence` /
  `ShouldShowQuickPanelForPresence` / `FluxingPresence`
- `include/FluxingDiagLog.h`（F6 诊断日志实现）

**本轮新增的验证工具（都在 `tmp/`，被 `.gitignore` 挡住，不入库）**：

- `tmp/verify_quickpanel_visibility.py` —— `probe`（确定性语义矩阵 + 负对照）
  / `watch <秒>`（窗口跃迁 ↔ 诊断日志行对齐）；
- `tmp/watch_quickpanel_stream.py` —— 持久化流式观察器（只读，追加写
  `tmp/quickpanel-stream.jsonl` + `.log`）；
- 证据：`tmp/quickpanel-visibility-20261103-010448.json`（5/5 PASS + NEG PASS）、
  `tmp/quickpanel-watch-20261103-011117.json`（300 s 窗，零跃迁 —— 会话锁定所致）。
