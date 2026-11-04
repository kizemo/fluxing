# 0036. 快捷设置栏回显当前输入状态、键盘可达性与每槽 tooltip；状态区从「最后一个槽位右边界」推导；Esc 语义用「键盘交互态」调和

- **状态**: Accepted
- **日期**: 2026-11-05
- **决策者**: Fluxing maintainers
- **相关**: spec 099（参考逆向搜狗输入法优化快捷设置栏）P0-4 / P0-5 / P0-6；ADR 0019（ESC 不再收起面板）/ 0032（自家 UI 身份判定）/ 0034 / 0035；constitution P2 / P5 / R7；lessons-learned L180

## Context

快捷设置栏自 ADR 0022 起是常驻面板，4 个槽（切换输入方案 / 用户短语 / 快捷键设置 /
关于）只有图标，**没有**任何「我现在处于什么状态」的回显：

1. **状态不可见**：用户无法从面板上看出当前是中文态还是英文态、是全角还是半角。
   切换全/半角只能靠盲试。
2. **键盘不可达**：面板用 `WS_EX_NOACTIVATE` + `SW_SHOWNOACTIVATE` 显示（这是
   L89-fix 的事故修复：抢焦点会让用户打不了中文），代价是**面板收不到按键**
   ⇒ 纯键盘用户完全用不了面板。
3. **槽位语义不可见**：图标没有文字说明，新用户无法知道 4 个槽分别干什么。

同时存在两条互相冲突的既定约束：

- **ADR 0019 D2**：ESC **不**收起快捷设置栏（用户 v1.1.1 验收口径「显隐只靠 Alt+,」）。
- **ADR 0032 / D0032**：在「用户短语」窗口按 Esc 关它，**不得**连带收起快捷设置栏。

spec 099 P0-5 又要求「Esc 收起」。三者不能同时按字面成立 ⇒ 必须有一个**可判定的状态**
把「面板有没有拿到键盘焦点」区分开。

还有两条硬约束：

1. **P2 红线**：面板显示路径**零阻塞 I/O**。而 Rime 的「当前全/半角」若现读用户配置
   会触发 `user_config_open`（文件 I/O）⇒ 状态必须在**启动期**预热。
2. **R7 单一真相源**：「常驻显示」开关的真相源是 `QuickPanelDialog::AlwaysShowMode()`
   （`include/FluxingAlwaysShow.h`）。状态回显**不得**另立第二个来源。

合规红线：只做**黑盒可观察的用户可见行为**借鉴，**不提取、不复用**第三方的二进制、
资源、词库、皮肤或专有文案；本 ADR 的所有实现均为自有。

## Decision

**一、状态回显（中/英 + 全/半角）**

面板右侧新增**状态区**，竖排两字：上 = 中/英，下 = 全/半。

- 状态的**语义与文字映射**下沉到 header-only `include/FluxingPanelStatus.h`
  （namespace `fluxing::panel_status`）：`struct PanelState{ascii_mode, fullwidth,
  always_show, schema_name, version}` + `AsciiModeText()` / `ShapeModeText()`。
  生产绘制与单元测试**同源**（R7）。
- 状态的**来源**是服务端注入的 `StateProvider`（`std::function<PanelState()>`），
  内部只读 `RimeWithWeaselHandler` 的**内存镜像字段**：`IsAsciiMode()`
  （`m_global_ascii_mode`）、`IsFullShape()`（`m_cached_options` 缓存）、
  `GetCurrentSchemaId()`、`AboutDialog::VersionText()`（启动期已读好的 PE 版本资源缓存）。
- **拉取时机**：面板**已有的** 100ms 轮询 timer（`WM_TIMER` id=2）里拉一次快照，
  比对 5 个字段，**变了才** `RepaintLayered` ⇒ 无变化时零重绘。
- **零 I/O 硬要求**：`WeaselServerApp::Run()` 在 `m_handler->Initialize();` 之后
  **立即**调用一次 `(void)m_handler->IsFullShape();` 与
  `(void)m_handler->IsSimplification();`，把 Rime 的选项缓存置为「已缓存」。
  此后 `IsFullShape()` 只读内存 ⇒ 显示路径不碰文件（P2）。
- **常驻开关不另立真相源**：`RefreshInputState()` 用 `AlwaysShowMode()` **强制覆盖**
  provider 带来的 `always_show` ⇒ provider 只是镜像，不是来源（R7）。

**二、状态区几何：从「最后一个槽位右边界」推导，而不是写死**

状态区左界 = 最后一个槽位的右边界 + 间隙（`kStatusGap = 1` logical），右界 =
内容右界（面板宽 − padding）。

- 100% DPI：内容右界 240，最后一槽右界 224 ⇒ 状态区 x=225..240（宽 15）。
- 150% DPI：槽位与状态区各自按 dpr 舍入 ⇒ x=341..360（宽 19）。
- 200% DPI：x=450..480（宽 30）。

**为什么不能写死 x = 225**：写死的坐标在 150% 下会压到槽 3 上 ⇒ 槽位命中区被改
⇒ 违 spec 099 P0-4「命中区不变」。从槽位右边界推导则**在任意 DPI 下都不相交**。

几何的单一事实源是 `WeaselServer/QuickPanelDialog.cpp` 内的 `ComputeLayout()` /
`SlotRectForLayout()` / `StatusRectForLayout()`；对外暴露
`SlotRectForSize(w, h, i, …)` 与 `StatusRectForSize(w, h, …)` 供单测在
100% / 150% / 200% 三档断言 —— **测试不复制布局公式**（R7）。

**三、DPI 缩放下沉到共享纯函数**

新增 header-only `include/FluxingPanelDpi.h`（namespace `fluxing::panel_dpi`）：
`ScaleLogicalByDpi(logical, dpi)` = `(logical*dpi + 48) / 96`（就近舍入，.5 向上；
`dpi <= 0` 退化 96；结果不大于 0 时回退 logical）与 `PanelPhysicalSize(...)`。

生产代码原本的本地缩放函数改为**调用**它 ⇒ 150% ⇒ 245×48 逻辑尺寸 =
**368×72** 物理像素（spec 099 P0-6 的核心判据）。

**四、键盘可达性**

键位语义下沉到 header-only `include/FluxingPanelKeyboard.h`
（namespace `fluxing::panel_keyboard`）：

- `State{focused, slot_count, interactive}`
- `Action{kNone, kSwallow, kMoveFocus, kActivate, kClose}`
- `MoveFocus(current, delta, slot_count)`：在 `[0, slot_count)` 内循环；`current`
  越界时按 delta 方向落到首/末槽。
- `HandleKey(state, key, shift)`：
  - `Tab` / `Shift+Tab` / `←` / `→` / `Home` / `End` ⇒ `kMoveFocus`；
  - `Enter` / `Space` ⇒ `kActivate`（**无焦点槽位时只吞掉**，绝不误触发槽 0）；
  - `Esc` ⇒ 未交互 `kSwallow`、已交互 `kClose`；
  - 其余 ⇒ `kNone`。

`WM_CHAR` 一律吞掉（面板不是输入框 ⇒ 不产生系统提示音、不进 IME）。

**五、焦点方案：保留 `WS_EX_NOACTIVATE`，用户点击面板时才接管**

面板显示时仍用 `SW_SHOWNOACTIVATE`，**不抢**用户编辑框焦点（L89-fix 的事故形态）。
用户在面板上**点击**时（`WM_LBUTTONDOWN`）调用 `CaptureKeyboardFocus()`：

1. `GetForegroundWindow()` + `GetWindowThreadProcessId()` 拿到前台线程；
2. 跨线程时用 `AttachThreadInput` 接上输入队列，读出并记住原焦点窗口
   `s_prevFocusHwnd`；
3. `SetFocus(panel)` ⇒ 触发 `WM_SETFOCUS` ⇒ 进入键盘交互态。

面板收起时 `RestoreKeyboardFocus()` 归还焦点，**且仅当焦点此刻仍在面板上**才归还
（用户若已点了别的窗口，绝不抢回来）。

**六、Esc 语义：用「键盘交互态」调和 ADR 0019 D2 / D0032 / spec 099 P0-5**

- **未进入键盘交互态**（面板没拿到焦点，Esc 根本到不了面板）⇒ Esc **只被吞掉**，
  面板不隐藏 ⇒ 守 ADR 0019 D2（显隐只靠 Alt+,）。
- **已进入键盘交互态** ⇒ Esc 收起**面板自身**（`Hide()` 只关自己，**不触碰任何其他
  窗口**）⇒ 守 D0032（关「用户短语」不得连带收起快捷设置栏），同时满足 spec 099
  P0-5 的「Esc 收起」。

判据是**状态**（`s_keyboard.interactive`）而非事件顺序 ⇒ 可单测、可判别。

**七、每槽 tooltip：自绘轻量顶层窗**

hover 某槽时在其上方显示「名称 · 当前态」：

- 槽 0 切换到下一个输入方案 · 当前:<方案名>
- 槽 1 用户短语 · 当前输入:中/英
- 槽 2 快捷键设置 · 常驻显示:开/关
- 槽 3 关于 / 账号说明 · 版本:<x.y.z>

文本由 `include/FluxingPanelTooltip.h`（namespace `fluxing::panel_tooltip`）生成
（`SlotName()` / `SlotStateText()` / `BuildTooltip()`），生产与测试同源。

窗口是**自绘顶层窗**（类名 `FluxingQuickPanel_Tooltip`，
`WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT`）：
不引 comctl32（不改 `.vcxproj`）、不抢焦点、不产生系统 tooltip 的黄色边框。

**hover 的真相源是轮询而非 `WM_MOUSEMOVE`**：L84-fix 已证 `WS_EX_LAYERED` +
`WS_EX_NOACTIVATE` 面板下 `WM_MOUSEMOVE` 投递不可靠（实测 hovered 恒为 -1）
⇒ tooltip 的显示/隐藏挂在**已有的 100ms 轮询 timer**里。

## 被否决的替代方案

| 替代方案 | 否决理由 |
| --- | --- |
| 状态区写死 x = 225 | 150% DPI 下会压到槽 3 上 ⇒ 槽位命中区被改（违 P0-4「命中区不变」） |
| 在 `WM_TIMER` 轮询里现读 Rime 选项 | 首次调用触发 `user_config_open` ⇒ 显示路径文件 I/O（违 P2）⇒ 改在启动期预热 |
| tooltip 用 comctl32 `TOOLTIPS_CLASS` | 需改 `.vcxproj` 引入 comctl32 依赖；系统 tooltip 样式不可控 ⇒ 自绘轻量顶层窗 |
| tooltip 靠 `WM_MOUSEMOVE` 触发 | L84-fix 已证投递不可靠 ⇒ 挂轮询 timer |
| 面板显示时直接 `SetFocus` | 会抢用户编辑框焦点 ⇒ 重演 L89-fix「无法输入中文」事故 |
| Esc 一律收起面板 | 违 ADR 0019 D2（用户口径「显隐只靠 Alt+,」） |
| Esc 一律吞掉 | 违 spec 099 P0-5「Esc 收起」⇒ 改用键盘交互态分流 |
| 键盘语义写在 `QuickPanelDialog.cpp` 里 | 测试要复制一份 ⇒ 改键位表时测试不红（spec 092 的「假绿」死法）⇒ 下沉共享头 |
| 「常驻显示」从 provider 拿 | 会形成第二个真相源（违 R7）⇒ 强制用 `AlwaysShowMode()` 覆盖 |

## Consequences

- ✅ 面板能回显「中/英 + 全/半角」当前态；状态源是**内存镜像**，显示路径零 I/O（P2）。
- ✅ 纯键盘用户可完整操作面板：Tab / Shift+Tab / ← → / Home / End 移焦，
  Enter / Space 激活，Esc 收起。
- ✅ 每槽 hover 显示「名称 · 当前态」，新用户可自解释。
- ✅ 150% DPI 的物理几何有单测锚点（368×72），DPI 缩放公式与布局公式均**单一
  真相源**（R7）。
- ✅ Esc 语义不再有冲突：未交互只吞掉（ADR 0019 D2），已交互只收自己（D0032）。
- ✅ `include/WeaselIPCData.h` **未改动** ⇒ 不触发 IPC 四边同步。
- ✅ 键盘焦点有焦点环（蓝色虚线）可见，用户知道焦点在哪。
- ⚠️ **未验证（真机）**：`CaptureKeyboardFocus()` / `RestoreKeyboardFocus()` 的跨线程
  `AttachThreadInput` 行为、以及面板作为 `WeaselServer.exe` 进程内窗口能否稳定拿到
  键盘焦点，只能在装机环境确认。
- ⚠️ **未验证（真机）**：真实 100% / 150% / 200% DPI 切换下，368×72 的物理尺寸与
  状态区是否与肉眼一致；多显示器拔插时的落点（P0-7 遗留）仍需人工复验。
- ⚠️ 面板**出现时**仍收不到按键（`WS_EX_NOACTIVATE` 的固有代价）⇒ 用户必须先点
  一下面板才能用键盘。彻底解决需要改面板窗口模型，超出本 ADR 范围。
- ↩️ 可逆：四个共享头是纯 header-only，删除即可回退；不涉及线格式与协议。
