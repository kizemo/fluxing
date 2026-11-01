# 0026. 快捷面板「常驻」开关收进统一设置面板；运行期状态与落盘策略下沉共享头

- **状态**: Accepted
- **日期**: 2026-11-01
- **决策者**: Fluxing maintainers
- **相关**: spec 095、ADR 0019、ADR 0020、ADR 0022、ADR 0024、lessons L162

## 背景

交接卡 §6.3 的开发要求 ① 是：**把「快捷面板常驻」开关收进统一设置面板**。

接手时的现状有三个问题：

1. **入口对不上**。统一设置面板（`ShortcutSettings`）的窗口标题与分组是「快捷键」，
   用户在里面找不到常驻开关；真正能改常驻态的入口在托盘右键菜单里。
   ADR 0024 已经把「快速面板常驻」从语言栏菜单移除，面板侧却没有补上入口，
   于是「常驻」成了一个只能靠记忆托盘菜单才能打开的隐藏开关。

2. **真源分裂**。常驻态同时存在两份：
   - `QuickPanelDialog::s_mode`（`static` 成员，进程内运行期真源）；
   - `<用户数据目录>\quickpanel.state`（跨进程落盘真源）。
   两者由 `QuickPanelDialog` 内部的读写函数手工保持同步，
   一旦有第二个写入方（本次就是设置面板），同步逻辑立刻成为新的缺陷面。

3. **副作用与状态缠在一起**。「切到常驻 / 切出常驻」要顺带做两件视觉上的事
   （切出时把 alpha 复位为满值、切回时重绘面板），它们挂在 `QuickPanelDialog`
   的实现里，导致「只改状态」这件事无法单独测试 —— 测状态就得建窗口。

同时接手时还发现 `WeaselServer/ShortcutSettings.cpp` 的中文是**逐字节 `\xNN` 转义**写进去的
（见 lessons L162）。MSVC 不会重组宽字面量里的字节转义，装机后整张设置面板是乱码。
既然本轮要交付这个界面，顺手把编码还原成真 UTF-8 是同一件工作。

## 决策

**我们把常驻态的运行期状态、落盘、读盘策略下沉到一个 header-only 共享头
`include/FluxingAlwaysShow.h`（namespace `fluxing::always_show`），成为唯一真源。**

共享头提供：

- 运行期状态：`AlwaysShowFlagRef()` / `AlwaysShowEnabled()` / `SetAlwaysShowFlag()`；
- 落盘：`FormatStateLine()` / `ParseStateLine()` / `WriteStateFile()`
  （**写后回读自证**：读回不一致就删掉坏文件，绝不留下半截状态）；
- 读盘：`enum class StateReadStatus { kMissing, kEmpty, kRead }` + `ReadStateLine()` 三态，
  以及决策纯函数 `ResolveAlwaysShowFromFiles(primary, legacy, default_value)`；
- 路径：`DefaultStatePath()` / `StatePathOverrideRef()` / `ResolveStatePath()`；
- 通知：`ModeChangedCallback`（POD 函数指针）+ `SetModeChangedCallback()` /
  `NotifyModeChanged()`。

**我们删除 `QuickPanelDialog::s_mode`。** `QuickPanelDialog::CurrentMode()` /
`AlwaysShowMode()` 改为读共享头，**不保留第二真源**。`QuickPanelDialog` 不再自带
`WriteQuickPanelState` / `ReadAlwaysShowStateLine` / `QuickPanelStatePath`。

**我们用观察者模式承担视觉副作用。** 共享头只管状态与通知；
「切出常驻 ⇒ alpha 复位为满值」「常驻开启且面板可见 ⇒ 重绘」这些 UI 动作
由 `QuickPanelDialog::OnAlwaysShowModeChanged` 处理，它在进程启动时通过
文件作用域的静态 registrar 注册为观察者。这样**共享头不依赖任何 UI 头**，
可以单独测试；UI 侧也不必知道状态是怎么存的。

**我们在统一设置面板新增「界面」分组。** `ShortcutSettings` 的窗口标题与
自绘标题改为「设置」；副标题改为「快捷键与界面设置 · N 条快捷键」；
在工具栏行之后、状态行之前插入「界面」分组（自绘标题 + 分隔线），
分组内放一个 `BS_AUTOCHECKBOX` 复选框「快捷面板常驻」，
控件 ID `2130`（落在 2100–2199 段内）。复选框初值取共享头的运行期状态，
`BN_CLICKED` 时写回共享头（⇒ 落盘 + 通知观察者）。

**我们给生产代码开两个不建窗口的公开 API**：
`ShortcutSettings::AlwaysShowChecked()` / `SetAlwaysShowChecked(bool)`，
供测试直接读写状态。UI 侧的复选框句柄仍由 `Hide()` 与 `OnDestroy()` 复位。

**legacy 落点仍留在 `QuickPanelDialog`。** 旧路径 `%APPDATA%\Rime\quickpanel.state`
需要 `SHGetFolderPathW`（shell32），共享头只管 primary 路径（fork 用户数据目录，
ADR 0020）；旧路径只读兜底。

### 被否决的替代方案

- **让 `ShortcutSettings` 直接读写 `quickpanel.state` 文件**：绕过共享头 ⇒
  两份写入逻辑，状态与落盘可能不一致，正是本决策要消灭的问题。
- **保留 `s_mode` 作为运行期真源、共享头只管落盘**：仍是两份真源，
  「设置面板改了值但面板不知道」会重演。
- **把复选框做成「勾上就显示面板、取消就收起」**：与托盘菜单语义重叠但不同源。
  本次统一为**只改状态与持久化**，面板显隐仍由既有路径决定，差异写进交付报告。
- **用 `std::function` 存观察者回调**：`FluxingAlwaysShow.h` 需要能同时被
  `WeaselServer` 与测试工程引用，共享头里用 POD 函数指针（`void(*)(bool)`）
  更省依赖。

## 后果

- ✅ 常驻态**只有一处真相**（`include/FluxingAlwaysShow.h`），
  设置面板与托盘菜单写的是同一份状态、落的是同一个文件。
- ✅ 状态读写**不需要建窗口** ⇒ 回归测试可以在无 UI 环境下断言，
  且与生产代码同源（宪法 R7）。
- ✅ 读盘语义与 ADR 0020 的旧实现**逐条等价**
  （primary 读到行 ⇒ 用它；读到但不认识 ⇒ false；读不到行 ⇒ 试 legacy；
  都读不到 ⇒ 默认值），既有 `TestQuickPanelDialog` 断言**一行未改**仍全绿，
  这是等价性的机器证据。
- ✅ 「界面」分组让常驻开关有了可发现入口，与 ADR 0024 从语言栏移除菜单项的决定互补。
- ⚠️ 复选框**不主动 Show/Hide 面板**。勾选后面板是否出现，取决于既有路径
  （托盘菜单 / 语言栏）。这是有意取舍，差异已写入交付报告与本 ADR。
- ⚠️ 共享头的观察者是**单槽位**（后注册覆盖先注册）。当前只有
  `QuickPanelDialog` 一个注册者，够用；若将来出现第二个消费者，
  需要改成注册表形式。
- ⚠️ `QuickPanelDialog::s_mode` 被删除 ⇒ 任何仍引用它的代码都会编译失败。
  本次改动覆盖了全部引用点，`build.bat weasel` 0 错误通过。
- ↩️ 本决策**可逆**：`FluxingAlwaysShow.h` 是纯头文件，删除它并把逻辑搬回
  `QuickPanelDialog` 即可；IPC 线格式零变化。
