# ADR 0029 — 快捷设置栏「切走必隐」：通知必达、管道重连重取句柄、显隐同线程排序

- **Status**: Proposed
- **Date**: 2026-11-02
- **Spec**: [097-shim-name-collision-and-hotkey-chord-separation](../../.specify/specs/097-shim-name-collision-and-hotkey-chord-separation/spec.md)（同批次发现，不属该 spec 验收项）
- **Lessons**: L168
- **Supersedes / superseded by**: —（新增；与 ADR 0024、ADR 0025 互补，不取代）

---

## 背景

快捷设置栏（`QuickPanelDialog`，WeaselServer 内的顶层小面板）的正确语义早已在
ADR 0024 定下：**切入火流猩时显示、切到其他输入法时隐藏**。ADR 0025 又把「切走」
的判定表收敛成了共享纯函数。

语义是对的，判定也是对的 —— 活体探针直接向服务端发
`FOCUS_OUT(PROFILE_DEACTIVATED)`，面板立刻 `visible=False`；发 `FOCUS_IN(PROFILE_ACTIVATED)`
则可见。数据平面没有问题。

问题全在**承载路径**上。三个互相独立的缺陷叠在一起，任一个单独存在就足以让
「切走必隐」失效，而且三者都表现为同一种症状：**什么都没发生，也不报错**。

**缺陷一：客户端通知线程在退出路径上把消息丢了。**

TSF 通知线程的循环体先判停止标志、再取待办种类。于是 `Deactivate()` 投递的
「已切走」只要落在「停止标志已置、线程尚未真正退出」这个窗口里，就会被整条丢弃。
输入法被切换、宿主进程被销毁、语言配置被改 —— 这几条路径都会把用户带进这个窗口。

**缺陷二：管道层重连后仍用重连前的句柄，异常又被上层静默吞掉。**

`PipeChannel::_Send()` 捕获写失败后会重连，但重连之后写的是**重连前那个局部句柄**，
不是新句柄。这个二次失败冒出到 `ClientImpl::_SendMessage()` 的 `catch (DWORD) { return 0; }`，
于是调用点看到的是「返回 0」—— 而服务端 `OnFocusOut` 的应答本来就恒为 0。
**「发送失败」与「发送成功但对方没照做」在调用点上完全不可区分**，这是本条最难查的原因。

**缺陷三：服务端把显示投递到队列、把隐藏就地执行。**

`FocusIn` 走 `PostMessage(WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL_SHOW)`（异步，进 UI 线程队列），
`FocusOut` 却直接在 IPC 线程上调 `QuickPanelDialog::Hide()`。两者没有共同顺序 ——
**已经排队的显示完全可能在隐藏之后才执行，面板就此复活**。

---

## 决策

### 决策 1 — 通知线程先取待办，再判停止

只有「待办为空」时才允许因停止标志退出。已经入队的种类（尤其是 `kProfileNotifyDeactivated`）
必须在收尾时被处理一次。

### 决策 2 — 焦点离开的送达判据是「是否真的发出去」，不是返回值的内容

`Client::FocusOut()` / `ClientImpl::FocusOut()` 由 `void` 改为 `bool`，返回**是否真的送达**。
单独 try/catch 包住 `channel.Transact(req)`，不让它被 `_SendMessage` 的兜底 catch 吞成 0。

通知线程的 deactivated 分支据此最多**重试 3 次、间隔 120 ms**。**不以返回值 0 判失败** ——
服务端对此命令的应答恒为 0，返回值只说明「发出去了没有」。

### 决策 3 — 管道层修的是 stale 句柄，不是 `Transact()` 的异常行为

`_Send()` 在捕获异常后**重新取 `_GetPipeHandle()`** 再写一次；`_Connect()` 改为**有界重试**
（预算 3 s，超预算 `throw DWORD(ERROR_PIPE_BUSY)`；`FILE_NOT_FOUND` 仍由 `_TryConnect` 快速抛出，
不把「服务端根本没起」拖成 3 秒等待）。

`Transact()` 的异常行为**不动**：grep 确认它只被客户端 `_SendMessage` 使用，服务端不调用。

### 决策 4 — 服务端显示与隐藏都走 UI 线程队列

隐藏不再在 IPC 线程上就地执行，改为 `PostMessage(WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL_HIDE)`
交给 UI 线程。显示与隐藏在**同一条消息队列**上，FIFO 顺序由队列本身保证。

显示处理器增加**在场闸门**：取 `ShouldShowQuickPanelForPresence(GetFluxingPresence())`，
已经不在场就把本次显示作废。这样即使某个显示请求仍排在隐藏之后，也不会把面板重新点亮。

手动唤起路径（`Alt+,`、托盘菜单）**不经过闸门** —— 那是用户的显式意图，不应被在场状态否掉。

### 决策 5 — 在场状态收敛为共享纯函数

`FluxingPresence { kUnknown, kPresent, kAbsent }`、`UpdateFluxingPresence(prev, focus_in_profile, focus_out_profile)`、
`ShouldShowQuickPanelForPresence(presence)` 落在 `include/WeaselIPC.h`，
生产代码与 `test/TestWeaselIPC` 同源 —— 宪法 R7 禁止同一判定出现两份实现。

`FocusIn` 的 profile 分支**先置 `kPresent` 再请求显示**；`FocusOut` **先更新 presence 再隐藏**。
`kUnknown` 允许显示（服务端刚起、客户端还没报过任何 profile 事件时不能把面板锁死）。

### 决策 6 — 会话结束兜底，不依赖可能丢失的 FocusOut

`RemoveSession()` 里若结束的是 `m_active_session`，直接置 `kAbsent` 并请求隐藏。
即使整条 FocusOut 路径都丢了，切走之后面板也不会挂在那里。

### 决策 7 — 诊断走独立 append-only 文件

`include/FluxingDiagLog.h` → `%TEMP%\fluxing-profile-notify.log`。
不用 glog（会轮转，事后取不到证），不用服务端 `DLOG`（被 `include/no_logging.h` 编译成空操作）。
写入用 `FILE_APPEND_DATA` + `FILE_SHARE_READ|WRITE`，单次 `WriteFile` 保证行级原子，
2 MB 上限后停止追加，写失败静默忽略 —— 诊断代码绝不能反过来影响输入法。

### 被否决的替代方案

- **在 `OnActivated` / `Deactivate` 里同步发 IPC**：违反宪法 P2（TSF 回调内禁止阻塞 I/O）。
- **给 `QuickPanelDialog::Hide()` 本体加守卫**：违反 ADR 0013 D7 / ADR 0016 D1 —— 守卫只加在调用点。
- **让显示与隐藏都在 IPC 线程执行**：显示要触碰窗口创建与消息循环，跨线程更危险。
- **COM 看门狗（`ITfActiveLanguageProfileNotifySink`）**：原假设「签名错配」已被 `msctf.h:8831` 证伪；
  且 GUID 常量在 SDK 头里定位不到，语义不确定，风险大于收益。
- **在 `ActivateEx` 里主动 post「activated」**：会让每次应用激活都弹面板。

---

## 后果

- ✅ 「切走必隐」不再依赖某一条消息恰好没丢：通知必达（决策 1/2）、管道不静默吞（决策 3）、
  显隐同队列（决策 4）、会话结束兜底（决策 6）—— 四道独立防线。
- ✅ 判定逻辑只有一份实现（决策 5），`/verify-quickpanel-presence` **十一条**断言可在无输入法环境下跑
  （2026-11-03 01:5x 实跑 11/11 ok、exit 0；其中一条断言的原文就是
  「profile-deactivated hides even in always-show mode」——诉求 2 的可执行判据）。
  `/verify-focusout-flags` 同日实跑 **PASS**：线上 plain=[0] / flagged=[1]，隐藏判定随事件种类走。
- ✅ 现场可取证：`%TEMP%\fluxing-profile-notify.log` 记下每次投递、重试、隐藏、闸门否决。
- ⚠️ **新增一条跨进程状态**（presence）。它是内存态、随服务端进程重启归零，不落盘、不需要迁移，
  但读它的人必须知道 `kUnknown` 是合法初值。
- ⚠️ `FocusOut` 的返回类型变了（`void → bool`）。虚函数表 `RequestHandler::FocusOut` **保持 `void`** 不动，
  只有客户端侧接口变化 —— 线格式零变化，`include/WeaselIPCData.h` 未改。
- ⚠️ `_Connect()` 的有界重试会让「服务端正在启动」从「立刻失败」变成「最多等 3 秒」。
  这是刻意的取舍：冷启动窗口里丢一条切走通知，代价比多等 3 秒高。
- ✅ **真机 TSF→服务端这一段已有实证**（2026-11-03 01:4x 复核）：装机版
  `D:\Program Files\fluxing\weasel\weaselx64.dll`（2026-11-02 22:15:26 构建）字节内含
  `deactivated sent=` 特征串 ⇒ 装机的就是含本 ADR 修复的二进制；而
  `%TEMP%\fluxing-profile-notify.log` 里出现了 **3 组完整的真实切换序列**：
  `[server] focus_out param=1 presence=absent` 紧跟 `[tsf] deactivated sent=1`。
  `[tsf]` 前缀只由 `WeaselTSF.cpp:444` 写出，探针（只发 IPC）写不出它
  ⇒ 这 3 次是**真实 TSF 前端**送达的，不是探针合成的。
  即：决策 1/2（通知必达）与决策 3（管道重连重取句柄）在真实运行中已被触发并成功。
- ✅ **装机完整性已复核并恢复**（2026-11-03 01:48）：用重打包安装器
  `output\archives\fluxing-1.1.6-installer.exe /S /D=D:\Program Files\fluxing` 就地重装，
  `D:\Program Files\fluxing\weasel\WeaselServer.exe` 回到 2267648 B（与 `output\Win32\WeaselServer.exe`
  同源）；该二进制字节内含 `focus_in profile presence=` 与 `focus_out param=` 特征串
  ⇒ 装机的**服务端**确为本 ADR 修复版（此前只对 `weaselx64.dll` 做过同样核对）。
  注册表四点复核：`Run` / 两侧 `CLSID` / `Uninstall` 全部指向 `D:\Program Files\fluxing\weasel`，
  `InstallDir` 指向装机根 `D:\Program Files\fluxing`（`WEASEL_ROOT = $INSTDIR\weasel`，
  `output/install.nsi:25` 与 `:457`）。
- ✅ **会话结束兜底（决策 6）在真机上确实会被触发**（2026-11-03 01:51:30，
  流式观察器 55 min 全程留痕）：装机事故中重装安装器替换了旧 `WeaselServer`，
  服务端随后写下 `[server] remove_session active presence=absent`
  ⇒ F5 兜底（`RimeWithWeasel.cpp:286`）不只是单元测试里的可达分支，
  在「服务端被强杀 / 前端会话骤失」这条真实路径上被走到了。
- ⚠️ **观察器在 01:42:01 记录到的那次 `visible: true → false` 跃迁，不能算作本 ADR 的同轴证据**：
  它落在 §5.11 的装机事故窗口内（旧服务端被安装器替换，面板窗口随旧进程消失），
  且该次跃迁**没有伴随任何诊断增量**（`diag_new: []`）⇒ 与「切走必隐」机制无关。
  同理 01:51:30 的 `remove_session` 记为 `diag` 而非 `transition`
  （面板自 01:42:01 起已是 hidden，状态未变）。
  ⇒ **ADR 0029 的验收缺口不变，仍只有「一次真实切换里 TSF 事件与窗口跃迁同轴」。**
- ⚠️ **仍缺的只有「窗口跃迁与 TSF 事件同轴」这一条**（需要用户在真实桌面切一次输入法，
  且要求全程有可见性采样）。2026-11-03 01:0x–01:3x 的三段采样窗口（`watch 300` /
  `watch 900` / 流式观察器）**零跃迁**，根因是**桌面会话处于锁定态**
  （`LogonUI.exe` 在跑、`GetForegroundWindow()` 返回 0）⇒ 期间根本没有发生 profile 切换，
  面板自始至终 `visible=True`，「无跃迁」不能读作「隐藏失效」。
  服务端→窗口这一段另有独立实证：向服务端直发 `FOCUS_OUT(PROFILE_DEACTIVATED)`
  后 `IsWindowVisible` 在 18 ms 内由 `True` 变 `False`（5/5 PASS + 负对照 PASS）。
- ⚠️ 因此本 ADR 状态**暂留 `Proposed`**：两条边界（TSF→服务端、服务端→窗口）各自有实证，
  但「一次真实切换里三者同轴」尚未取证。取证**不需要人手按**：解锁桌面后跑
  `tmp\real_switch_probe.py 4`，它自动完成「切走 → 切回」，同时断言 `IsWindowVisible` 的跃迁
  与诊断日志增量；会话仍锁定时它直接跳过注入并 exit 2（零副作用）。
  判定口径见 handoff §5.9.3。
  已完成的验证是：`/verify-quickpanel-presence` 单元断言（11/11）、`/verify-focusout-flags`
  线格式断言、构建通过、服务端判定语义的活体探针、以及上述真机 TSF 送达实证。
- 可逆性：单点可逆。决策 3 与决策 4 各自独立，撤掉任一条不影响其余；决策 5 的枚举一旦被
  其它模块引用，撤销就需要一次性的接口迁移。