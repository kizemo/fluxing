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
- ✅ 判定逻辑只有一份实现（决策 5），`/verify-quickpanel-presence` 八条断言可在无输入法环境下跑。
- ✅ 现场可取证：`%TEMP%\fluxing-profile-notify.log` 记下每次投递、重试、隐藏、闸门否决。
- ⚠️ **新增一条跨进程状态**（presence）。它是内存态、随服务端进程重启归零，不落盘、不需要迁移，
  但读它的人必须知道 `kUnknown` 是合法初值。
- ⚠️ `FocusOut` 的返回类型变了（`void → bool`）。虚函数表 `RequestHandler::FocusOut` **保持 `void`** 不动，
  只有客户端侧接口变化 —— 线格式零变化，`include/WeaselIPCData.h` 未改。
- ⚠️ `_Connect()` 的有界重试会让「服务端正在启动」从「立刻失败」变成「最多等 3 秒」。
  这是刻意的取舍：冷启动窗口里丢一条切走通知，代价比多等 3 秒高。
- ⚠️ **本 ADR 的端到端结论尚未在真机输入法切换下复测**（需要用户在真实桌面切一次输入法）。
  验收前状态保持 `Proposed`。已完成的验证是：`/verify-quickpanel-presence` 单元断言、
  构建通过、以及服务端判定语义的活体探针（这一项在改动前就已证明正确）。
- 可逆性：单点可逆。决策 3 与决策 4 各自独立，撤掉任一条不影响其余；决策 5 的枚举一旦被
  其它模块引用，撤销就需要一次性的接口迁移。