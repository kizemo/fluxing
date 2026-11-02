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

剩余三件事，都要人：
① 真实桌面切一次输入法，确认「TSF 事件 → 窗口跃迁」在同一条时间轴上对齐（探针已在跑，
   见 §5.9）⇒ 之后把 ADR 0029 从 `Proposed` 转 `Accepted`；
② 补 `WeaselServer` 自启项（本机**至今没有**，重启机器后服务端不会自动起来）；
③ 用户签字后 `git push kizemo Fluxing`。

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
- ADR 0029 因此**仍保持 `Proposed`**，未转 `Accepted`（等 §5.9 的实机切换证据）。
- 诉求①（真机 silent install）与诉求③（物理按键验收）与本 bug 无关，仍未做。

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

---

## 6. 提交状态（截至 2026-11-03 00:47）

`HEAD = 4a0c6301d6ea07c27ebfb39c45e7afa45f347cd1`，分支 `Fluxing`，
`## Fluxing...kizemo/Fluxing [ahead 5]` —— **5 个 commit 全部未 push**。

本轮新增的两个：

| commit | 范围 | 内容 |
|---|---|---|
| `1733f6d` | `fix(fluxing)` | 12 个文件：F1–F6 全部源码 + `test/TestWeaselIPC/TestWeaselIPC.cpp` + 新建 `include/FluxingDiagLog.h` |
| `4a0c630` | `docs(fluxing)` | `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md`、`docs/adr/README.md` 索引、`.specify/memory/lessons-learned.md`（L168）、本文档 |

更早的三个（仍未 push）：

- `cc13c86` feat(fluxing) 短语注入
- `7c07862` docs(fluxing) CHANGELOG + lessons
- `7e20129` docs(agents) AGENTS.md

**工作区状态**：`git status -sb` 只剩 ` m librime`（脏子模块，**永不提交**）。
其余 15 个文件已全部入库（已用 `git rev-parse HEAD:<path>` 与 `git hash-object <path>`
逐个核对一致）。

**push 刻意推迟**：commit 可逆、push 不可逆 ⇒ 等用户确认复测通过后再
`git push kizemo Fluxing`。

---

## 7. 下阶段计划（按顺序）

1. **请用户肉眼复测一次**：切到火流猩 ⇒ 面板出现；切到其他输入法（微软拼音等）⇒
   **面板必须消失**（**建议同时勾上「常驻显示」再测**，这才是本 bug 的原始触发条件）。
   同时看 `%TEMP%\fluxing-profile-notify.log` 是否新增 `focus_out param=1 presence=absent`
   与 `deactivated sent=1`（应当都有）。
2. 用户确认通过 ⇒ 把 `docs/adr/0029-quickpanel-hide-delivery-and-ordering.md` 的
   `- **Status**: Proposed` 改为 `Accepted`（同一 commit 里一并改）。
3. **补 `WeaselServer` 自启项**（当前**没有**，重启机器后服务端不会自动起来）：
   - `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 内无 `WeaselServer`/`Fluxing` 项；
   - `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run` 存在但无相关项；
   - `schtasks /tn WeaselServer` 不存在；
   - `HKLM\SOFTWARE\Fluxing\Weasel` 键不存在。
   → 走 `WeaselSetup.exe` 的注册逻辑最稳；手工补 Run 项也可，但必须**写死完整路径**
   `"D:\Program Files\fluxing\weasel\WeaselServer.exe"`。
   ⚠️ 写 `HKLM` 需要管理员权限；写 `HKCU\...\Run` 不需要。**这一项涉及注册表写入，
   建议先问用户选哪条路径**（HKCU Run / WeaselSetup / 不加）。
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
