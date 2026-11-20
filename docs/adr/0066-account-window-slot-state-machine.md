# ADR 0066 — 账号与同步窗口：槽位状态机 + 清槽位提前 + 复用前探活

- **Status**: Accepted（用户诉求明确：「第二次进入后，已登录窗口无法关闭」；修法为防御性全覆盖）
- **Date**: 2026-11-20
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md)（批次 AW）
- **Revises**: —（新增；不改 ADR 0063 的账号页信息架构与两套互斥视图口径）
- **Lessons**: `.specify/memory/lessons-learned.md`（本批条目待写）
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 真机现象

1.3.4 装机后：首次点「账号与同步」⇒ 窗口正常打开、已登录态显示正确。
**第二次**进入该菜单 ⇒ 那个已登录窗口**点关闭按钮关不掉**（窗口还在，点了没反应）。

### 事实 2 — 窗口是裸 Win32 顶层窗，靠进程级句柄表复用

`WeaselServer/Cloud/CloudAccountDialog.cpp`：账号页是 modeless 顶层窗
（`CreateWindowExW`，父 `nullptr`），在 `WeaselServer` 的 UI 线程创建；
进程级静态表 `HWND g_windows[Role::kCount]` 保存每种角色的窗口句柄。
**第二次进入时复用同一个 HWND 是设计意图**（ADR 0063 的四个独立子窗口模型），不是缺陷。

### 事实 3 — 缺陷在「复用前不校验实例是否还活着」

`CreateOrShow` 的复用分支**只判 `IsWindow(hwnd)`**，不校验该窗口是否仍可交互、是否处于销毁流程
⇒ 第二次进入可能复用**半死窗口**；此时点关闭（`kIdClose` → `DestroyWindow(hwnd)`）因窗口已在
销毁流程中而**失效**。

### 事实 4 — `WM_DESTROY` 的清理顺序让槽位残留

`WM_DESTROY` 处理**先**调 `UnregisterWindow(hwnd)`（内部取 `impl_->mutex` 阻塞 `std::lock_guard`）
**后**清 `g_windows[]` 槽位 ⇒
① 槽位清理晚于一次可能阻塞的取锁；② 二者不幂等 ⇒ 若 `UnregisterWindow` 阻塞或异常，
**槽位残留指向半死 HWND**（正是事实 3 的来源）。

### 事实 5 — 关闭按钮在子控件消息未返回时同步销毁父窗

关闭按钮的 `WM_LBUTTONUP` 尚未返回时，父窗的 `WM_COMMAND` 同步 `DestroyWindow(parent)`
⇒ 连带销毁了**正在处理消息的按钮控件**。

### 已排除的可能（不必再查）

- 不是 modal 阻塞；关闭路径没有同步网络请求或等锁。
- 不是「关闭按钮被隐藏」：`kIdClose` 在已登录布局里存在（`CloudAccountUi.h` 的
  `AccountLoggedInLayout()`：368,210,100,28），且在 480×250 客户区内。
- 菜单入口不是 toggle（关闭后不会自动重开）。
- 没有 `EnableWindow(FALSE)` 造成的残留（全 `WeaselServer/` 唯一一处 `EnableWindow` 只作用于设备窗 4 个按钮）。

---

## 决策

### 决策 1 — 槽位状态机抽成纯函数，落在共享头（R7）

新增 `WeaselServer/Cloud/CloudAccountWindowState.h`（**纯 ASCII**、**不依赖 windows.h**，任何 TU 可 include）：

```
enum class SlotState { kEmpty, kStale, kLive };
ClassifySlot(has_handle, is_window, responsive) -> SlotState
SlotReusable(state) -> bool
MustClearSlotBeforeUnregister(state) -> bool
```

生产代码与单测**共用同一份实现**（constitution R7），单测不复制第二份判据。

### 决策 2 — 清槽位必须早于 `UnregisterWindow`，且登记表用**独立 mutex**

- `WM_DESTROY`：**先**在登记表 mutex 下清 `g_windows[]` 槽位（只在槽位确实指向本窗口时才清），
  **再**调 `UnregisterWindow`。
- `UnregisterWindow` 取 `impl_->mutex`（控制器状态锁）的动作**不得**在持有登记表 mutex 时进行
  ⇒ 两把锁不交叉，消除「UI 线程在 `WM_DESTROY` 阻塞于控制器锁」的路径。
- `WM_NCDESTROY` 再清一次（双保险），同样「槽位指向本窗口才清」。

### 决策 3 — 复用前必须探活

复用分支除 `IsWindow` 外，必须做一次**带超时的探活**：
`SendMessageTimeoutW(hwnd, WM_NULL, 0, 0, SMTO_ABORTIFHUNG, 200 /*ms*/, nullptr)`。
探活失败 ⇒ 判为**陈旧槽位** ⇒ 丢弃旧句柄并**新建**窗口（并写诊断信息）；探活成功才复用并显示。

### 决策 4 — 关闭路径统一走 `PostMessage(WM_CLOSE)`

所有关闭触发点（各 Role 的 `kIdClose`、按钮 `WM_LBUTTONUP`、标题栏关闭）统一改成
`PostMessageW(hwnd, WM_CLOSE, 0, 0)`，由 `WM_CLOSE` → `DestroyWindow(hwnd)` 走同一条路径。
**禁止**在按钮的 `WM_LBUTTONUP` 处理里同步 `DestroyWindow(parent)`（事实 5）。

### 决策 5 — 留诊断证据

「复用陈旧窗口」「重建窗口」「销毁时清槽位」三处写诊断日志（ASCII 前缀 `[FluxingAccountDlg]`），
供下次真机复现时区分「复用半死窗」「新建即坏」「UI 线程在 `WM_DESTROY` 阻塞」三种情形。

### 决策 6 — 不动 IPC、不动账号页布局与文案

不改任何 IPC 消息；不改 `CloudAccountUi.h` / `CloudAccountWidgets.h` 的布局、控件与文案；
不改登录逻辑。

---

## 残余风险（未闭环）

1. **根因未 100% 坐实**：三种情形（「复用半死窗」「新建即坏」「UI 线程阻塞于 `impl_->mutex`」）
   在源码上都能导致同一现象，本次按**防御性全覆盖**修（顺序 + 独立锁 + 探活 + 统一关闭路径）
   并加诊断日志；真机复测后据日志可进一步定位。
2. **探活阈值 200 ms**：窗口被长时间阻塞时会被误判为陈旧 ⇒ 会重建一个窗口（旧的仍会随进程退出消失）。
   代价是极端情况下多一个窗口实例，可接受。

---

## 已驳回的备选方案

1. **每次进入都新建窗口、废弃槽位表** —— 驳回：会破坏 ADR 0063 的四窗模型与「切换到别的子窗再回来
   仍保留登录态」的行为，且窗口会越开越多。
2. **在 `WM_DESTROY` 里用 `try/catch` 包住 `UnregisterWindow`** —— 驳回：C++ 异常不是这里的失败模式
   （失败模式是**阻塞**），`catch` 抓不到。
3. **把关闭按钮从子控件改成自绘菜单项** —— 驳回：改动面大、与既有 UI 体系不符。
