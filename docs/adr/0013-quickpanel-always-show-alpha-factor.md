# 0013. The QuickPanel fades through a per-pixel opacity factor, and always-show is an opt-in mode

- **Status**: Proposed
- **Date**: 2026-10-28
- **Deciders**: Fluxing maintainers
- **Related**: spec 084、spec 052（长显模式设计）、spec 080（切换输入法自动显示）、ADR 0008、清单 P-E3、`CHANGELOG.md`

## Context

spec 052（2026-07-08，用户本人提的需求）设计了「长显模式」：切到火流猩输入法后快捷设置栏常驻
在屏幕右下角，常态 20% 不透明度，鼠标移上去变 100%，移开再淡回去。

那份设计**从未落地**，而且**原方案已经与今天的渲染器冲突**。

落地缺口的实测事实（逐条核验于 2026-10-28）：

1. `QuickPanelDialog::ApplyAlphaGradient()`（`QuickPanelDialog.cpp:920`）走 per-pixel alpha：
   32-bit BGRA DIB 的 alpha 通道按 y 渐变写成 220→80，圆角外写 0，再 `UpdateLayeredWindow(ULW_ALPHA)`
   提交到屏幕。这是 liquid glass 的形状来源。
2. spec 052 的 plan.md §2.2.1 写的是 `SetLayeredWindowAttributes(hwnd, 0, 51, LWA_ALPHA)` ——
   **uniform alpha**。但当前渲染器的头注释把这条路列为历史雷区：
   「❌ 绝对不用 LWA_ALPHA uniform（L77 — 不是 liquid glass，只是 86% 全白）」。
   spec 052 的方案照抄下去会把整个面板的渐变与圆角一起压平。
3. `s_alpha` / `s_targetAlpha` 两个静态字段已存在（`QuickPanelDialog.h:84-85`），初值 255，
   但**全仓没有任何一处读写** —— 纯死状态。
4. `OnMouseMove` / `OnMouseLeave` 是**空函数体**（`QuickPanelDialog.cpp:1116-1117`），
   spec 052 T003 从未做。真正在跑的是 100ms 轮询定时器（`WM_TIMER` id=2，
   `GetCursorPos` + `ScreenToClient` + `HitTest`），它只管图标 hover 着色。
5. `EnableAlwaysShowMode(...)`（`QuickPanelDialog.cpp:1244`）**没有任何调用方**；
   `ID_QUICKPANEL_ALWAYS_SHOW 40019`（`WeaselServer/resource.h:52`）**没有任何使用点**。
   spec 052 T005 从未做。
6. spec 080（2026-10-23 已 ship，ADR 0008 `Accepted`）在 §4 非目标里**明确写了**
   「不做 spec 052 的 always show mode（常显）。面板是瞬时窗口」。
   ⇒ 长显不是被否决，而是从没被接上；spec 052 的任务清单 16 项全未勾是**过期描述**。
7. `QuickPanelDialog::ToggleMode()` 调 `Show(false, nullptr × 6)`，而 `Show()` 会**无条件**把这
   6 个参数写进静态字段 ⇒ 一次 `ToggleMode()` 就把按钮回调全清成空指针（按钮变哑）。
   这是 L60 同一族的隐患，只是当前调用链绕开了它。

另外，长显有一个**产品层的历史包袱**：`WeaselServerApp::Run()` 曾经调
`EnableAlwaysShowMode()`，结果面板在用户**根本没在用**火流猩输入法时也弹在右下角，
是用户报过的 bug（`WeaselServerApp.cpp:209-213` 的注释记录了这段）。spec 080 修掉了「服务启动
就弹」，但没有给长显留入口。

## Decision

**D1 — 用「整体不透明度系数」乘进逐像素 alpha，不回退 uniform alpha。**
面板已有的 220→80 渐变保留不动，额外乘一个 `factor ∈ [0, 255]`：
`final_alpha = round(base_alpha × factor / 255)`。
LWA_ALPHA 继续保持禁用（雷区不变），liquid glass 的形状不被压平。

**D2 — 系数满值 ⇒ 渲染结果与今天逐像素一致。**
`factor == 255` 时 `final_alpha == base_alpha`。这条是本 ADR 的**零回归保证**：
不开长显的用户看不到任何像素变化。

**D3 — 常态 / 悬停两态之间是渐变。**
由已经存在的 100ms 轮询定时器驱动：`target = 鼠标在面板内 ? 255 : (长显 ? 51 : 255)`，
`s_alpha = StepAlpha(s_alpha, target, 24)`（255↔51 走 9 档 ≈ 0.9 秒）。
**非常显模式下 target 恒为 255**，所以系数永远不动 ⇒ 零重绘、零视觉变化。

**D4 — 长显是「可开关的模式」，默认关。**
入口是托盘菜单里一项**可勾选**的开关（`ID_QUICKPANEL_ALWAYS_SHOW 40019`，此前已定义未用），
勾号反映真实状态。默认关 ⇒ 现有用户行为不变，且避免了 `WeaselServerApp::Run()` 曾经踩过的
「用户没在用输入法时面板也弹出来」的坑。

**D5 — 常显态下屏蔽三条自动隐藏路径。**
轮询器「鼠标在外 1500ms」、`WM_ACTIVATEAPP` 前台切走、`WM_KILLFOCUS` → timer 1，
在长显态下一律不生效。`SetAlwaysShowMode(false)` ⇒ 立即 `Hide()` + 系数复位为满值。

**D6 — 长显状态不持久化。**
不写注册表、不写配置文件，输入法服务重启后回到默认（关）。
理由：spec 080 之后面板已经默认自动弹出，持久化一个「常驻」开关反而会盖掉用户的瞬时面板习惯；
而且 spec 052 的「重启后默认长显」原本是为了解决「面板根本不出现」，那个问题已由 spec 080 解决。

**D7 — 既有行为不改。**
「切到火流猩输入法自动弹出面板」（spec 080）与「按 Alt+, 弹出 / 再按收起」都保持原样。
长显是一个**平行的模式**，不是它们的替代。

**D8 — 顺带修掉 `ToggleMode()` 清空回调的隐患。**
`ToggleMode()` 改为把当前静态回调原样回传给 `Show()`，不再传 `nullptr × 6`。

**D9 — 纯函数放共享单元，验收可不开窗口。**
`ScaleAlpha` / `StepAlpha` / `LerpAlpha` 放在 header-only 的 `include/QuickPanelAlpha.h`
（与 ADR 0011 D1 的 `include/KeyNameMapper.h` 同一惯例），不依赖 `windows.h`，
因此验收门禁可以直接断言系数演算，不必创建窗口。

**D10 — 菜单勾选状态挂在 `WeaselTrayIcon` 的既有钩子上，不改第三方 SDK。**
`CSystemTray` 本来就提供了 `virtual void CustomizeMenu(HMENU)`（菜单弹出前调用一次），
本方案**不修改** `SystemTraySDK.*`（第三方代码），而是在 `WeaselTrayIcon` 里实现这个
虚函数：`WeaselTrayIcon` 增加 `std::function<bool(UINT)> m_menuItemChecked` 与
`SetMenuItemCheckedCallback()`，`WeaselServerApp` 在构造时挂上钩子，逐项询问是否打勾
（只有钩子返回 true 的项才 `CheckMenuItem`，不会误清其他项的勾选状态）。
菜单资源本身仍是资源菜单（3 个语言变体各一条文案）。

> 实现期补充：`WeaselTrayIcon.cpp` 只 `#include <resource.h>`，解析到的是
> `include/resource.h`，那里**没有** `ID_QUICKPANEL_ALWAYS_SHOW`（该常量只在
> `WeaselServer/resource.h`）。因此 `WeaselTrayIcon.cpp` 内不引用该常量，
> 勾选判定由 `WeaselServerApp` 侧传入的 lambda 完成
> （`WeaselServerApp.h` 同时包含 `"resource.h"` 与 `<resource.h>`，两个资源头都能拿到）。

## Consequences

- ✅ 用户可以主动把快捷设置栏变成常驻状态指示器：常态很淡，悬停清晰。
- ✅ 开关默认关 ⇒ 不开长显的用户**零像素变化、零行为变化**（D2 + D3）。
- ✅ 没有引入 uniform alpha，没有碰 L77 雷区。
- ✅ 不新增任何进程间通信消息，不改消息语义（P2 的「两端同步」条款不触发）。
- ✅ 不触及用户数据路径（不读写 `RimeUserDir` 下任何文件），不涉及 P6 豁免。
- ⚠️ 长显态下面板会跨应用停留在屏幕右下角 —— 这是「常驻」的语义，
  由用户主动开启并自行关闭（D4 给了明确入口）。
- ⚠️ 长显态下 `WM_ACTIVATEAPP` 不再隐藏面板 ⇒ 切到 en-US 时面板可能短暂残留。
  这与 spec 052 风险表的「切走火流猩输入法时面板不会自动隐藏（v0.19 再做）」一致。
- ⚠️ 常态 20% 在浅色桌面上偏淡，文字需要悬停才读得清 —— 这正是 spec 052 US052-A/B 的设定。
  系数是单一常量，想调亮只改 `include/QuickPanelAlpha.h` 的 `kAlphaFactorIdle`。
- ✅ 决策可逆：把 `kAlphaFactorIdle` 调回 255 就等于把长显关掉；把托盘菜单项撤掉就等于
  完全回到今天。

## Open questions

- **Q1**：长显态下面板要不要跟随「切到 en-US」消失？**已定**：不消失（D5 + 后果 §4）。
- **Q2**：常态 20% 是否要更亮？**已定**：先按 spec 052 的 alpha=51 落地，后续一个常量即可调。
- **Q3**：长显状态要不要持久化？**已定**：不持久化（D6）。
