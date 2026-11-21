# ADR 0069 — 账号页「最近一次同步时间与结果」可见性：把 `sync_status()` 写进 `kIdSyncStatus`(1017)

- **Status**: Accepted
- **Date**: 2026-11-21
- **Spec**: [100-fluxing-cloud-account-sync](../../.specify/specs/100-fluxing-cloud-account-sync/spec.md) —— **不改规格**：FR-016 早已明写「账号页可查看……最近一次同步时间与结果」，本 ADR 只补**实现缺失**，不动 FR 编号与措辞
- **Revises**: ADR 0063（账号页信息架构）中 **1017 的语义** —— 从「创建时写死的占位文案」改为「承载真实同步轮次结果」；**可见性规则、控件 ID、布局几何一律不变**
- **Lessons**: `.specify/memory/lessons-learned.md`
- **Supersedes / superseded by**: —

---

## 背景

### 事实 1 — 1017 的控件、布局、几何**全都存在**，缺的只是「写值」这一步

- `kIdSyncStatus = 1017`（`WeaselServer/Cloud/CloudAccountUi.h`，枚举 `AccountControlId`）——
  注释明写它属 **T305 / T505 时代定下的存量 ID**（历史上被外部脚本与探针引用过）⇒ **不得改动已有值**。
- 布局表里有它：已登录态条目 `{kIdSyncStatus, 12, 108, 456, 20}`（`AccountLoggedInLayoutBase()`）。
- `AccountRoleLayout(kAccount)` = 未登录态 ∪ 已登录态 ⇒ **控件在两态都被创建**（未登录态隐藏）。
- 批次 BJ 真机实测（`reports/fluxing-bf-a4-a5/`，`GEOMETRY_MISMATCH_COUNT=0`、9 条 `G_*` 全 `ok=True`）
  ⇒ 已登录态 9 个控件几何逐字相符 ⇒ **1017 确实被创建且可见**。

⇒ 结论：**不存在「控件没建」这类问题**；缺的是刷新路径上的赋值。

### 事实 2 — 真实同步结果一直存在，但只活在一个成员变量里

`CloudAccountController::sync_status()`（`CloudAccountDialog.cpp`）返回 `Impl::sync_status_text`；
该成员由 `Impl::SetSyncStatus()` 在**七个**时点更新：

| 时机 | 文案 |
|---|---|
| 引擎启动前缺令牌 / 缺加密密钥 / 缺设备号 | 最近同步：未登录，无法同步。 |
| 引擎启动失败 | 最近同步：同步引擎启动失败。 |
| 同步轮次开始前未登录 | 最近同步：尚未登录，无法同步。 |
| 一轮成功 | 最近同步：<时间> · 成功（推送 N 条，拉取 N 条） |
| 一轮被维护闸门跳过 | 最近同步：<时间> · 已跳过（<原因>） |
| 一轮失败 | 最近同步：<时间> · 失败（<原因>） |
| 登出复位 | 最近同步：尚未同步。 |

（跳过 ≠ 失败：措辞口径来自 ADR 0062 决策 2。）

⇒ **轮次结果一直在算、也一直在存，只是没有任何控件承载它。**

### 事实 3 — 刷新路径从不写 1017（缺陷本体）

`RefreshUi()`（`CloudAccountDialog.cpp`）刷新 1006 状态行 / 1020 账号标签 / 1018 历史下拉 / 1007 设备列表，
**只**写 1006；1017 的文本停留在创建时 `WidgetText()` 给的**静态字面量**「最近同步：尚未同步。」。

⇒ 「读 1017 看同步结果」这条路**在结构上不可能成立**。

### 事实 4 — 这条缺陷此前被登记成「探针判据无效」，**不是**产品缺陷

批次 BC 的 CHANGELOG 条目（`CHANGELOG.md`，1.3.8 节第一条）写道：

> 「账号页『最近同步』控件 `kIdSyncStatus = 1017` 的文案取自 `WidgetText()` 的**静态字面量**……而 `RefreshUi()` **只**刷新 1006 状态行 / 账号标签 / 历史下拉 / 设备列表，**从不写 1017** ⇒ 轮次结果只存在 `Impl::sync_status_text` 成员里、**没有任何可见控件承载** ⇒『读 1017 看是否被跳过』这条路**在结构上不可能成立**（前两轮探针据此判 FAIL 属**判据无效**，不是产品缺陷）。」

**该描述在事实层面完全正确**，但它同时暴露了一个**当时未被登记的产品缺陷**：
FR-016 的界面落点从未接线，1017 是一个永远不变的装饰件。

⇒ 本 ADR 的定性：**这是产品缺陷（实现缺失），不是判据问题。**
修掉它，让 1017 成为真正的观测面；「绕开 1017 改看导出文件 mtime」的口径**继续有效**（见决策 4）。

### 事实 5 — 规格早已要求（FR-016），**故不改 spec**

spec 100 FR-016：「**界面可见**：账号页可查看登录状态、账号邮箱、设备列表（可移除设备）、
**最近一次同步时间与结果**。」

⇒ 1017 就是 FR-016 的落点；**规格没缺，实现缺** ⇒ 本 ADR **不新增 FR、不改 FR 措辞**。

---

## 决策

### 决策 1 — 「同步状态文案」收成**共享真相源函数组**（R7）

把七种文案的拼装从 `CloudAccountDialog.cpp` 的字面量搬到
`WeaselServer/Cloud/CloudAccountUi.h` 的 inline 函数组：

| 函数 | 文案形态 |
|---|---|
| `AccountSyncStatusTextNeverSynced()` | 占位 / 登出复位 |
| `AccountSyncStatusTextNotLoggedIn()` | 启动引擎前未登录 |
| `AccountSyncStatusTextRoundNotLoggedIn()` | 轮次开始前未登录（措辞与上一条**不同**，逐字保留） |
| `AccountSyncStatusTextEngineStartFailed()` | 引擎启动失败 |
| `AccountSyncStatusTextSuccess(when, pushed, pulled)` | 成功 |
| `AccountSyncStatusTextSkipped(when, reason)` | 已跳过 |
| `AccountSyncStatusTextFailed(when, reason)` | 失败 |

**为什么**：constitution **R7（生产与单测同源）**。原先七处文案散在 `.cpp` 里，
单测要么复制第二份字面量（R7 红线），要么只能测别的 ⇒ 1017 的文案**没有任何回归护栏**。
搬进共享头后，生产侧与单测调用**同一份实现**，单测用**独立写死的期望值**做逐字比对
⇒ 生产侧字面量被改动时单测必红。

**为什么放 `CloudAccountUi.h`**：它是账号页 UI 的既有真相源头（布局表 / 控件 ID / DPI 缩放 /
换钥提示文案 `AccountKeyRotationNoteText()` 都已在此），单测**已经**包含它 ⇒ 不新增依赖、不新增工程。

### 决策 2 — `RefreshUi()` 写 1017（3 行产品码）

在账号页刷新分支里，与 1020 账号标签并列地写 1017：

```cpp
HWND sync_status = ::GetDlgItem(hwnd, kIdSyncStatus);
if (sync_status != nullptr)
  ::SetWindowTextW(sync_status, controller.sync_status().c_str());
```

**为什么放在 `RefreshUi`**：它是账号页**唯一的**界面刷新入口
（`WM_APP_REFRESH` / 登录态切换 / 每轮同步结束后都会走到）⇒ 一处接线覆盖全部时机，
不新增轮次、不新增窗口消息、不新增线程。

**为什么无条件写（不判登录态）**：控件在两态都创建（事实 1）⇒ 未登录态写文本只影响隐藏控件的
内容、不影响任何可见性；登录后该控件显示的即为**最新值**，无需等下一次刷新。

**为什么用 `controller.sync_status()` 而不是自己拼**：轮次文案是**引擎线程/工作线程**写、
UI 线程读的共享状态；`sync_status()` 自带互斥（读 `Impl::mutex`）⇒ 不引入数据竞争。
（锁序铁律「先 sync_mutex / engine_mutex，后 impl->mutex」不破：`RefreshUi` 只取 `impl->mutex`。）

### 决策 3 — 不动控件 ID、不动布局表、不加控件、不改 IPC 协议

- 1017 是**存量 ID**（事实 1）⇒ 不改值；探针与外部脚本引用过它。
- 布局表**逐字不动** ⇒ 几何、可见性、ADR 0068 的 DPI 等比缩放结论全部延续。
- **不新增控件**：FR-016 要的是「最近一次同步时间与结果」，1017 一行足够；
  新增控件会扩大布局表与 ADR 0068 的缩放面，无必要。
- **不改 `include/WeaselIPCData.h`、不新增 IPC 消息类型** ⇒ 不触发 IPC 四边同步红线。
- **未改 `librime/`、未新增第三方依赖**。

### 决策 4 — 1017 变可见，但**不**升级为维护闸门的判据

⚠️ 边界必须写死：`docs/release/restart-checklist-1.3.6.md` §4 明写
「闸门是否放行 = **维护导出文件 mtime 是否前进**（**状态文案不可作判据**）」，
ADR 0062 / 0065 的闸门语义亦以 mtime 与会话记账为准。

⇒ 本 ADR 让 1017 成为**给人看的**观测面；**机器判据仍以导出文件 mtime 为准**。
两条口径并存不矛盾（一个是人眼可见，一个是机读可判）。

### 决策 5 — 归一化与登出入口的措辞差异**逐字保留**

`AccountSyncStatusTextNotLoggedIn()`（未登录，无法同步）与
`AccountSyncStatusTextRoundNotLoggedIn()`（尚未登录，无法同步）
**只差一个「尚」字**。它们来自两处不同语境的既有字面量 ⇒ 本 ADR **不合并、不改写**，
并各自单列一个函数，避免将来「顺手统一」把两处文案改坏。

---

## 影响

### 用户可见

- 账号页（已登录态）多了一行**会变**的「最近同步：<时间> · <结果>」——
  此前它永远显示「最近同步：尚未同步。」。
- **不新增行、不改布局**：1017 本来就占着 `{12, 108, 456, 20}` 这一格，只是文本不变。

### 不变的东西

- 控件 ID、布局几何、窗口角色、两态可见性规则（ADR 0063）。
- DPI 等比缩放口径（ADR 0068）。
- IPC 协议、`librime/`、依赖、版本号（装机版仍 1.3.8.0）。
- 维护闸门判据口径（决策 4）。

### 风险

| 风险 | 处置 |
|---|---|
| `RefreshUi` 在 UI 线程读 `sync_status()` 阻塞？ | 读的是内存里的 `wstring` + 一次互斥，**无 I/O** ⇒ 不违反 P2「TSF 路径不做阻塞 I/O」 |
| 与引擎线程并发写同一控件？ | 控件文本只在 UI 线程被写（`RefreshUi` 唯一写点）⇒ 无跨线程写控件 |
| 死锁（锁序） | `RefreshUi` 只取 `impl->mutex`；`SetSyncStatus` 也只取 `impl->mutex` ⇒ 同序，无环 |
| 文案改动破坏既有期望值 | 七种文案搬进共享头 + 单测逐字比对 ⇒ 改动必红 |

---

## 验证

- **单元级**：`test/TestFluxingCloud/TestFluxingCloud.cpp` 新增 `TestSyncStatusVisibility()`
  —— 12 条断言：1017 已登录态可见 / 未登录态不可见 / 控件被创建 /
  七种文案逐字相符（判据全部取自生产头，R7）。
- **端到端（真机）**：账号页控件 1017 的文本由占位文案变为「最近同步：<时间> · <结果>」，
  证据落 `reports/fluxing-bk-syncstatus/`。
- **门禁**：云单测 `PASS/FAIL` 全绿 · `build.bat release weasel installer` 0 错误 ·
  `L0-static.ps1` 15/15 · 定点 `clang-format` 0 漂移。

---

## 与既有文档的关系

- **ADR 0063**（账号页信息架构）：1017 的**可见性规则不变**，只是语义由「占位」改为「承载结果」。
- **ADR 0062 / 0065**（维护闸门）：**判据口径不变**（决策 4）；本 ADR 只解决「用户看不到结果」。
- **`CHANGELOG.md` 1.3.8 节批次 BC 的那条登记**：其中「1017 无可见承载面」的描述**在事实层面
  仍然成立**（那是当时的状态），但由此推出的「不是产品缺陷」**已被本 ADR 推翻** ⇒ 该条由
  批次 BK 的 CHANGELOG 条目就地更正。
