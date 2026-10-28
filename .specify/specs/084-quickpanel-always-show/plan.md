# 084 实施计划：快捷设置栏长显模式（常驻 + 悬停变清晰）

> 配套：[spec.md](spec.md)、[ADR 0013](../../../docs/adr/0013-quickpanel-always-show-alpha-factor.md)、
> spec 052 / spec 080、ADR 0008、清单 P-E3。

## 1. 技术上下文

### 1.1 现状（已核验）

| 事实 | 位置 |
|---|---|
| 面板是 per-pixel alpha 的 liquid glass：把 32-bit BGRA DIB 的 alpha 通道按 y 渐变写成 220→80，圆角外写 0，再 `UpdateLayeredWindow(ULW_ALPHA)` 提交 | `WeaselServer/QuickPanelDialog.cpp:920` `ApplyAlphaGradient()` |
| **禁止 uniform alpha**（`SetLayeredWindowAttributes(LWA_ALPHA)`），头注释写明「❌ 绝对不用 LWA_ALPHA uniform」 | `WeaselServer/QuickPanelDialog.h:21` |
| `s_alpha` / `s_targetAlpha` 两个静态字段已存在但**从未被读写**（死状态），初值 255 | `QuickPanelDialog.h:84-85` / `.cpp:171-172` |
| `OnMouseMove` / `OnMouseLeave` 是**空函数体** | `QuickPanelDialog.cpp:1116-1117` |
| 悬停判定其实在 100ms 轮询定时器里（`WM_TIMER` id=2，`GetCursorPos` + `ScreenToClient` + `HitTest`） | `QuickPanelDialog.cpp:601-634` |
| 三条自动隐藏路径：轮询器鼠标在外 1500ms、`WM_ACTIVATEAPP` 失活、`WM_KILLFOCUS` → timer 1 | `QuickPanelDialog.cpp:621-629` / `:584-592` / `:647` + `OnKillFocus` `:1127` |
| `EnableAlwaysShowMode(...)` 已存在但**没有任何调用方** | `QuickPanelDialog.cpp:1244` |
| `ID_QUICKPANEL_ALWAYS_SHOW 40019` 已定义但**没有任何使用点** | `WeaselServer/resource.h:52` |
| `ToggleMode()` 调 `Show(false, nullptr × 6)`，会把已保存的静态回调清成空指针 | `QuickPanelDialog.cpp:1240` |
| 托盘菜单是**资源菜单**，每次弹出时 `LoadMenu` 重新加载；共 3 个语言变体 | `SystemTraySDK.cpp:778` / `WeaselServer.rc:157,250,343` |
| `WeaselServerApp` 直接持有 `WeaselTrayIcon tray_icon`（`CSystemTray` 子类） | `WeaselServerApp.h:105` |
| `WeaselServerApp::ShowQuickPanel()` 是唯一注入真实回调的地方 | `WeaselServerApp.cpp:353-378` |

### 1.2 spec 052 的原方案为什么不能用

spec 052 plan.md §2.2.1 写的是 `SetLayeredWindowAttributes(hwnd, 0, 51, LWA_ALPHA)`。
当前渲染器**明确禁止**这条路（L77：那种 uniform 半透明不是 liquid glass，只是一张 86% 的
白纸，会把整个面板的渐变与圆角一起压平）。所以 spec 052 的方案必须**换实现**，而不是照抄。

## 2. 技术方案

### 2.1 不透明度系数（新增共享单元 `include/QuickPanelAlpha.h`）

面板已有的逐像素 alpha 渐变保留不动，额外乘一个**整体不透明度系数** `factor ∈ [0, 255]`：

```
final_alpha = round(base_alpha × factor / 255)
```

- `factor == 255` ⇒ `final_alpha == base_alpha` ⇒ **渲染结果与今天逐像素一致**（零回归保证）。
- `factor == 51` ⇒ 常态 20% 不透明度（spec 052 US052-A 的 alpha=51）。
- 系数为满值时 `base_alpha` 本身仍是 220→80 的 liquid glass 渐变，**不**退化成 uniform alpha。

纯函数放在 header-only 共享单元里（与 ADR 0011 D1 的 `include/KeyNameMapper.h` 同一惯例），
不依赖 `windows.h`，所以验收门禁可以不开窗口就断言：

| 函数 | 作用 |
|---|---|
| `ScaleAlpha(base, factor)` | 系数换算，四舍五入到最近整数 |
| `StepAlpha(current, target, step)` | 朝目标走一步且**不越过**（渐变用） |
| `LerpAlpha(from, to, t, steps)` | 线性插值（备用/测试） |

### 2.2 常态 / 悬停两态与渐变

轮询定时器（`WM_TIMER` id=2，100ms）本来就在算鼠标位置，顺手算「鼠标是否在面板矩形内」：

```
target = 鼠标在面板内 ? 255 : (常显模式 ? 51 : 255)
s_alpha = StepAlpha(s_alpha, target, 24)
```

- 步长 24 ⇒ 255↔51 走 9 档 ≈ 0.9 秒，符合「约 1 秒」的观感。
- **非常显模式下 target 恒为 255** ⇒ `s_alpha` 恒为 255 ⇒ 渲染逐像素不变。
- `s_alpha` 变了才 `RepaintLayered()`，避免无谓重绘。

### 2.3 常显模式

`QuickPanelDialog` 增加 `s_alwaysShow`（bool）与 `SetAlwaysShowMode(bool)` / `AlwaysShowMode()`。

常显模式打开时：

| 行为 | 常显关（今天） | 常显开 |
|---|---|---|
| 轮询器鼠标在外 1500ms 自动隐藏 | 隐藏 | **不隐藏** |
| `WM_ACTIVATEAPP` 前台切走 | 隐藏 | **不隐藏** |
| `WM_KILLFOCUS` → timer 1 隐藏 | 隐藏 | **不隐藏** |
| 常态不透明度 | 满值（100%） | 20%（系数 51） |
| 悬停不透明度 | 满值（100%） | 100%（系数 255） |

`SetAlwaysShowMode(false)` ⇒ 面板立即 `Hide()` + 系数复位为满值 ⇒ 回到今天的行为。

### 2.4 开关入口：托盘菜单

- `WeaselServer.rc` 的 3 个语言变体各加一条菜单项（`ID_QUICKPANEL_ALWAYS_SHOW 40019`）：
  - 简体「快捷设置栏常驻 (&A)」
  - 繁体「快捷設置欄常駐 (&A)」
  - 英文「Keep QuickPanel visible (&A)」
- `CSystemTray` 增加一个可选钩子 `std::function<bool(UINT)> m_menuItemChecked`，在菜单弹出前
  逐项询问「这个命令现在是不是开的」，为真就打勾。**不改第三方 SDK 的其余行为。**
- `WeaselServerApp` 在构造时挂上这个钩子，答案取 `QuickPanelDialog::AlwaysShowMode()`。
- `WeaselServerApp` 增加 `ID_QUICKPANEL_ALWAYS_SHOW` 的菜单处理器：
  - 非界面线程 ⇒ `PostMessage` 弹回界面线程（与既有 `ID_WEASELTRAY_QUICK_PANEL` 同款）
  - 打开 ⇒ 先 `SetAlwaysShowMode(true)` 再 `ShowQuickPanel()`（**复用既有真实回调注入**，
    避免长显态下面板按钮变哑）
  - 关闭 ⇒ `SetAlwaysShowMode(false)`

### 2.5 顺带修掉的地雷：`ToggleMode()` 清空回调

`ToggleMode()` 现在调 `Show(false, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)`，
而 `Show()` 会**无条件**把这 6 个参数写进静态字段 ⇒ 一次 `ToggleMode()` 就把按钮回调全清成
空指针（按钮变成哑巴）。改为把当前静态字段原样回传。

### 2.6 版本号

本次是**用户可见的新功能**，按语义化版本升 `1.1.0`（`env.bat` 的 `FLUXING_VERSION` /
`PRODUCT_VERSION`，gitignored）。`1.0.0` 的安装包作为历史载荷保留在 `release/`。

## 3. 里程碑

### Q1 · 不透明度系数 + 渐变（核心）

| # | 任务 | 验收 |
|---|---|---|
| T1.1 | 新增 `include/QuickPanelAlpha.h` | 纯函数，无 `windows.h` 依赖 |
| T1.2 | `ApplyAlphaGradient()` 乘上系数 | 系数满值时逐像素不变 |
| T1.3 | 轮询器接常态/悬停目标 + 渐变步进 | 非常显态下系数恒 255 |

**依赖**：无。**风险**：低。

### Q2 · 常显模式 + 开关接线

| # | 任务 | 验收 |
|---|---|---|
| T2.1 | `SetAlwaysShowMode()` / `AlwaysShowMode()` | 三条自动隐藏路径在常显态下全部失效 |
| T2.2 | 托盘菜单项（3 语言） | 菜单能弹出、能点 |
| T2.3 | 菜单勾选钩子 | 勾号跟随真实状态 |
| T2.4 | `WeaselServerApp` handler + 真实回调注入 | 常显态下面板按钮可用 |

**依赖**：Q1。**风险**：中（菜单资源是 UTF-16LE + BOM + LF，脚本改写必须保编码）。

### Q3 · 验证与发布

| # | 任务 | 验收 |
|---|---|---|
| T3.1 | `test/TestQuickPanelDialog` 加系数演算用例 | 新断言全过、既有断言不退化 |
| T3.2 | 全套件 + 整解构建 | 全绿 |
| T3.3 | 版本升 1.1.0 + 打包 + 载荷复验 | 装包可安装、载荷哈希一致 |
| T3.4 | 交用户真机验收 | 用户确认 |

**依赖**：Q2。**风险**：中。

## 4. Constitution Check

| 原则 | 结论 | 依据 |
|---|---|---|
| **I 先定意图** | ✅ | spec.md §1 目标 G1–G4 / §2 US084-A–F / §5 SC-001-1–SC-003-2 全部先写；ADR 0013 记录决策 |
| **II 测试支撑（证据先行）** | ✅ | 系数演算写成 `include/QuickPanelAlpha.h` 里的纯函数，门禁可不开窗口断言；T3.1 是本轮验收证据，T3.4 交真机 |
| **III ≥3 文件需 spec/plan/tasks** | ✅ | 本目录 spec.md / plan.md / tasks.md 三件套（本次改 8 个源文件 + 5 个文档） |
| **IV 澄清** | ✅ | 长显的默认态、入口位置、是否持久化三条歧义已在 spec.md §3 FR-004/FR-006/FR-007 与 ADR D4/D6 定死，无阻塞提问 |
| **V 增量交付** | ✅ | Q1（内核）→ Q2（接线）→ Q3（验证与发布）三段，每段可独立编译与验证 |

**必披露项**：

- **是否触及遗留用户数据路径**：**否**。本轮不改 `RimeUserDir` 下任何文件，不读写用户词典、
  不改 schema、不改 `weasel.yaml`。常显状态**不持久化**（FR-007），故不涉及 P6 豁免条款。
- **是否新增进程间通信消息**：**否**。不新增任何消息类型，通信面零改动（P2 的「两端同步」条款
  不触发）。
- **是否需要 CHANGELOG 条目**：**是**。用户可见的新增功能（托盘菜单项 + 面板常驻 + 悬停渐变）
  ⇒ 必须写进 `CHANGELOG.md` 的 `### 主要更新`（P5）。
- **构建目标同步**：本轮**不新增**构建目标（共享单元是 header-only），`weasel.sln` 与
  `xmake.lua` 的工程成员表不变（P3）。

## 5. 未决问题

| # | 问题 | 处置 |
|---|---|---|
| Q1 | 常显态下面板要不要跟随「切到 en-US」消失 | **已定**：不消失。spec 052 风险表里也把这条列为「v0.19 再做」；常显是用户主动开的，由用户自己关 |
| Q2 | 常态 20% 是否要更亮 | **已定**：先按 spec 052 的 alpha=51 落地。系数是单一常量，改一个数即可调 |
| Q3 | 开关状态要不要持久化 | **已定**：不持久化（FR-007），跟 spec 052「重启后默认长显」的原意相反 —— 因为 spec 080 之后面板已默认自动弹出，持久化常显反而会盖住用户的瞬时面板习惯 |

## 6. 提交纪律

- 每步都要 **clang-format 整文件 clean**（CI lint 按整文件判）。
- `style(fluxing):` 与功能改动分两个提交。
- `librime/` **只读**（宪法 P3）。
- 提交消息里断言的数字必须真实落地；未实跑的结论不得写死。
- 菜单资源是 UTF-16LE + BOM + LF，改写用脚本并回读校验，禁止用会重编码的工具。
