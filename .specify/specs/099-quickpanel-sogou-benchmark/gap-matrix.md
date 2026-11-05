# 099 · 差距矩阵 · 搜狗快捷设置栏 × 我方现状 × 设计意图（P0-2）

> 输入：`docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`（P0-1 黑盒行为观察）
> 与 `docs\research\sogou-reverse-2026-11\REPORT.md`（P0-0 静态 PE 侧）。
> 输出：逐行「**借鉴 / 改造 / 不抄**」判定 + **借鉴来源（证据文件全路径）** + **差异化实现**。
> **规则**：填不出「来源 + 差异化实现」的行一律删除；本轮不出现「未验证」的行为规格。

---

## 0. 三列口径

| 列 | 含义 | 取值来源 |
|---|---|---|
| **搜狗行为** | 观察到的形态事实（只写观察，不写推测） | `quickpanel-behavior.md` §1–§5 |
| **我方现状（源码位置）** | 我方当前实现 + 精确到文件（必要时行号） | 仓内源码 |
| **设计意图** | 既有设计稿的取向 | `docs\design\quickpanel-v3\index.html`（v3 · Liquid Glass 强玻璃，~392×68）、`quickpanel-v4\index.html`（v4 · Pure Icon Row，~196×30）、`quickpanel-v5\index.html`（v5 · Branded Bento Grid 2×3，280×138） |

**设计稿基线速记**

- **v3**：强玻璃拟态，左侧品牌区换为火流猩 PNG logo，右侧图标 38 px，D2D + WIC。
- **v4**：纯图标行，GDI only，~196×30，最窄最轻，贴右下角也不挡视线；稳定性优先。
- **v5**：品牌占左侧整列 + 5 个按钮（方案 / 短语 / 符号 / 设置 / 账号）在右 2×3 网格。

**我方现状速记**

- 面板：`WeaselServer\QuickPanelDialog.{h,cpp}`，GDI + per-pixel alpha + WIC PNG；`kButtonCount = 4`
  （Schema / Phrase / Settings / Account），`kPanelW=245 / kPanelH=48 / kBtnSize=35 / kBtnRadius=10`。
- 布局单一事实源：`SlotRect()`（`QuickPanelDialog.h:100`、`.cpp:647`）→ `SlotRectForLayout()`（`.cpp:149`），
  命中判定 `HitTest()`（`.cpp:983`），点击路由 `DispatchSlotClick()`（`.cpp:952`）。
- 状态回显：`include\FluxingPanelStatus.h`（`PanelState` / `AsciiModeText` / `ShapeModeText` / `kStatusGap`），
  面板右侧状态区几何见 `QuickPanelDialog.cpp:182-185`。
- 键盘 + tooltip：`include\FluxingPanelKeyboard.h`（`MoveFocus` / `HandleKey`）、
  `include\FluxingPanelTooltip.h`（`SlotName` / `SlotStateText` / `BuildTooltip`），入口 `QuickPanelDialog.cpp:941`。
- 常驻真相源：`include\FluxingAlwaysShow.h`（`AlwaysShowEnabled` / `ResolveStatePath`）→ `quickpanel.state`。
- 位置：`include\FluxingPanelPosition.h`（`ClampToVisibleArea` / `EnumerateWorkAreas`）→ `quickpanel.position`。
- DPI：`include\FluxingPanelDpi.h`（`ScaleLogicalByDpi` / `PanelPhysicalSize`）。

---

## 1. 差距矩阵（10 行）

### 行 1 · 状态栏常驻浮位（右下角、任务栏上方）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | `SoPY_Status` 可见，物理 353×66，rect `[3369,1964,3722,2030]` ⇒ 距屏右 118 px、距屏底 130 px；**候选窗隐藏时依然可见** ⇒ 常驻工具条与候选窗是**两套独立生命周期** |
| **我方现状** | `include\FluxingAlwaysShow.h`（`AlwaysShowEnabled()` / `ResolveStatePath()`，真相源 `quickpanel.state`）；落点 `include\FluxingPanelPosition.h`（`ClampToVisibleArea()` / `EnumerateWorkAreas()`），文件 `quickpanel.position` |
| **设计意图** | v4：「最窄最轻，**贴右下角也不挡视线**」；v3：~392×68 玻璃胶囊 |
| **判定** | **借鉴**（行为规格：常驻浮位 + 位置固定在右下工作区） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §1、§2<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-status-region.png`<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-window-enum.txt` |
| **差异化实现** | 搜狗是**固定右下角**；我方是**用户拖动后持久化 + 多屏工作区钳制**（`quickpanel.position` 独立文件，**不污染 `quickpanel.state`**，守 R7）。常驻开关仍以 `quickpanel.state` 为**唯一**真相源，`RestoreAlwaysShowMode()` **绝不 auto-show**（spec 055）。 |

### 行 2 · 一槽一图标的横排胶囊（7 项 + 1 分隔线）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 白底圆角胶囊，单行 **7 个可点项 + 1 条竖分隔线**；物理 353×66（平均每槽 ≈ 50 px，推算）；槽 6 是唯一彩色项 |
| **我方现状** | `QuickPanelDialog.h:352` `kButtonCount = 4`；`:338` `kBtnSize=35`；`:341` `kBtnRadius=10`；`:346` `kPanelH=48`；品牌区命中返回 `-2`（`QuickPanelDialog.cpp:998-1000`）；图标 PNG 唯一性由 `scripts\gate\L1-quickpanel-icon.ps1` 守 |
| **设计意图** | v3：「左侧品牌区换为真正的火流猩 PNG logo，**右侧图标 38 px**」；v4：「Pure Icon Row」 |
| **判定** | **改造**（借「一槽一图标 + 胶囊底」的结构，**不抄槽位数量**） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §2<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-status-printwin.png`（原生 353×66，槽位顺序）<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-status-zoom.png`（3× 放大） |
| **差异化实现** | 品牌区用**自有** `docs\design\fluxing-logo.png`，**绝不引入搜狗橙标**；**槽位数保持 4**（P0-3 用户定稿：`kButtonCount` 不变），**不扩到 7**；分隔线只做视觉分组、不做可点项；所有图标为**自有 PNG**，不复制搜狗图标资源，且必须过「无重复 base64」门禁。 |

### 行 3 · 中/英、全/半角在面板上直接可见

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证（形态）+ 推断（语义）】 | 状态栏槽 2 = `中`、槽 3 = `。,`；`≡` 面板另有「英 / 英文输入法」「半 / 全半角切换」两个开关 —— 形态一一对应 |
| **我方现状** | P0-4 已实装：`include\FluxingPanelStatus.h`（`PanelState` / `AsciiModeText` / `ShapeModeText` / `kStatusGap=1`），面板右侧状态区几何 `QuickPanelDialog.cpp:182-185`；状态由服务端 **100 ms timer 拉取的内存快照**注入 |
| **设计意图** | v3 / v4 / v5 **均未规定状态区**（v5 右侧 2×3 网格 5 按钮）⇒ 状态回显是**我方超出设计稿的增量** |
| **判定** | **借鉴**（借「当前输入状态必须可见」这一规格） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §2、§4<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-menu-panel.png` |
| **差异化实现** | 搜狗用**独立图标槽**占位；我方用**面板内文字状态区**，**不占槽位、不改 `kButtonCount`**。状态来源是**服务端内存快照**（`FluxingPanelStatus.h` 由 `WeaselServerApp.cpp` 注入 `StateProvider`），显示路径**零阻塞 I/O**（守 P2），**不经 TSF 查询**。`quickpanel.state` 语义未动。 |

### 行 4 · 二级「更多」面板承载常用开关（**只借排版手法，不建同款面板**）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | `≡` → 新建顶层窗 `CherryIMEWindowClass` 480×974（pid 与宿主同进程），三段式：**账号区 + 4 开关 + 10 项列表**；4 个开关是「图标字（简/半/英/👁）+ **下方一行小字说明**」，**没有一项是纯文字标签** |
| **我方现状** | **无二级面板**；槽位点击**直达**（`DispatchSlotClick()`，`QuickPanelDialog.cpp:952-981`）；常驻开关走 `include\FluxingAlwaysShow.h` |
| **设计意图** | v3 / v4 / v5 **全部是一级**（v5 仍是 2×3 一级网格） |
| **判定** | **改造**（借鉴「开关必须带当前态说明」这一手法；**不引入二级面板**） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §4、§5<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-menu-panel.png`<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-menu-hit.txt` |
| **差异化实现** | **不建二级面板**（避免菜单深度与状态分叉）。可落地的具体手法**只有一条**：**每个可切换项都要「名称 + 当前态」** —— 我方已由 tooltip 承载（`include\FluxingPanelTooltip.h` `BuildTooltip()`，P0-5）。若日后要做**Toggle 型**开关（如「隐藏状态栏」），必须另立 Toggle 入口，**不得与「点击执行」混在 `DispatchSlotClick()` 同一分支**，且状态仍以 `quickpanel.state` 为唯一真相源（R7）。 |

### 行 5 · 面板内功能入口分组 + 分隔线

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 10 项列表、3 条横分隔线分组、`›` 表示二级页（语音输入 / 符号大全 / 软键盘 / 图片表情 / 更多输入 / 皮肤商城 / 工具箱 / 定制状态栏 / 帮助 / 全局设置） |
| **我方现状** | 一级直达，**无二级菜单**；对应能力散在各自独立入口（短语库 / 快捷键设置 / 关于） |
| **设计意图** | v3 / v4 / v5 均**无二级菜单** |
| **判定** | **不抄** |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §4<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-menu-panel.png` |
| **差异化实现** | 保持「**点槽位直达**」。搜狗那 10 项多数对应我们**各自已有的独立功能入口**，聚合进一个菜单只增加层级、不增加能力，且与 v3/v4/v5 的一级取向冲突。 |

### 行 6 · 面板顶部账号区（头像 + 用户名 + 今日输入字数）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 账号区含头像 + 用户名 + 「今日输入277字」；其中「277 字」是**本地统计还是云端统计未验证**（`quickpanel-behavior.md` §6-10） |
| **我方现状** | **无云端账号**：`WeaselServer\AboutDialog.{h,cpp}` 文案事实为「本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。」 |
| **设计意图** | v5：把品牌当第一公民，**显性露出「我在用什么输入法」** |
| **判定** | **不抄**（账号区） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §4、§6-10<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\sogou-menu-panel.png` |
| **差异化实现** | **不引入账号体系**（与「无广告 / 无关联 / 不联网」定位冲突，且触碰 `evidence\urls-hosts.txt` 的联网红线）。若要表达使用强度，只能做**纯本地计数器**（不上报、不联网）；因搜狗该数字口径**未验证**，**不作为规格依据**，仅登记为 P1 候选项。 |

### 行 7 · 皮肤 / 主题入口

| 列 | 内容 |
|---|---|
| **搜狗行为**【形态已验证】 | 状态栏槽 6 = 彩色渐变块（推断为皮肤 / 主题）；`≡` 面板有「皮肤商城」入口（**二级页内容未观察**） |
| **我方现状** | **无皮肤面板入口**；`REPORT.md §2` 记录 `UIPlugin.dll` 导出 `SendSkinPreviewMessage`（皮肤预览是独立消息面），但本项目**未实现皮肤系统** |
| **设计意图** | v3 / v4 / v5 均**无皮肤入口** |
| **判定** | **不抄**（本轮） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §2、§6-9<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\REPORT.md` §2<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\rea-uiplugin-resources.json` |
| **差异化实现** | 皮肤系统本身是 P1 议题（**自有格式，绝不复制 `.cupf`**）。快捷设置栏**不加**皮肤入口 —— 加了就会把「面板 → 皮肤 → 商城」这条链引进来，而 `REPORT.md §1` 已实测搜狗 42% 的体积是不参与输入的联网 / 商业 / 组件层。面板不得引入任何默认联网行为。「定制状态栏」入口已观察但**内容未验证**，**不作为改造依据**。 |

### 行 8 · 完全自绘 ⇒ 不可访问（**反向借鉴：借它的反面**）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 三个窗口**子窗口数均为 0**；UIA 顶层枚举**拿不到搜狗任何 AX 节点**（只枚举到无关进程的 `MSCTFIME Composition`）⇒ 这条 UI **完全不可被 UI 自动化 / 读屏软件访问** |
| **我方现状** | P0-5 已实装：键盘可达（`include\FluxingPanelKeyboard.h` `MoveFocus()` / `HandleKey()`，ADR 0036 的「键盘交互态」分流）+ 每槽 tooltip（`include\FluxingPanelTooltip.h` `BuildTooltip()`）；命中区由 `SlotRect()` 给出**每槽独立矩形** |
| **设计意图** | v3 / v4 / v5 全部**以鼠标为主**，未规定键盘可达 |
| **判定** | **反向借鉴**（明确**不抄**其缺陷） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §7、§1<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-uia-empty.txt`<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-window-enum.txt` |
| **差异化实现** | 我方**不得**把面板做成「单一自绘位图」：必须保持**每槽独立命中区**（`SlotRect()`）+ **键盘焦点环**（Tab / 方向键移焦，Enter / Space 激活，Esc 只收面板自身、**不连带收候选窗**，守 D0032）+ tooltip 文本可被辅助技术读取。**这是我方超出搜狗的点**，不是不可行 —— 搜狗是「没做」，不是「做不到」。 |

### 行 9 · 候选条自带 `≡` 菜单

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 候选条右端两个按钮：`⌄`（展开更多候选，**效果未验证**）+ `≡`（菜单，实测可点开 480×974 面板） |
| **我方现状** | 候选条在 **WeaselUI**（`WeaselUI\`），右端**无菜单入口**；面板与候选条是**两个独立模块** |
| **设计意图** | v3 / v4 / v5 **只覆盖快捷设置栏本身**，不含候选条 |
| **判定** | **不抄**（本轮） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §3、§5、§6-8<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-menu-hit.txt` |
| **差异化实现** | 本轮**不动候选条 UI**：WeaselUI 是独立模块，改动面与本 spec 无关；且 P3 明令**不改 `librime/` 子模块**。候选条菜单属**独立议题**，不进 099。 |

### 行 10 · DPI 缩放比例口径（1.5 @ 150%）

| 列 | 内容 |
|---|---|
| **搜狗行为**【已验证】 | 状态栏逻辑 235×44 ⇒ 物理 **353×66**；候选条逻辑 475×81 ⇒ 物理 **713×122**；两者比值**同为 1.5**（@150%）⇒ 搜狗按 **logical × dpr** 缩放 |
| **我方现状** | `include\FluxingPanelDpi.h`（`ScaleLogicalByDpi()` / `PanelPhysicalSize()`），P0-6 装机实测 150% ⇒ **368×72**，与断言**逐像素一致**；三档断言 245×48 / 368×72 / 490×96 |
| **设计意图** | v3 ~392×68、v4 ~196×30、v5 280×138（**设计基准宽**，非强制值） |
| **判定** | **借鉴**（缩放**口径**一致，**绝对尺寸不追**） |
| **借鉴来源（证据文件全路径）** | `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md` §1、§3、§6-4<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-window-enum.txt`<br>`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel\log-capture.txt` |
| **差异化实现** | 缩放口径本就一致（logical × dpr），**无需改造**；差异只在**绝对尺寸**（我们 368×72 vs 搜狗 353×66 @150%），而 v4 明确取向就是「更小更轻」，**不追这个尺寸**。真正的差距在**缩放切换**与**多屏**：搜狗在 100% / 200% 与多屏下的表现**未验证**（§6-4、§6-5），**不能拿它当基准**；我们已有 `WM_DPICHANGED` 重算 + `quickpanel.position` 多屏工作区钳制。 |

---

## 2. 判定汇总

| 判定 | 行号 | 数量 |
|---|---|---|
| **借鉴** | 1、3、10 | 3 |
| **改造** | 2、4 | 2 |
| **不抄** | 5、6、7、9 | 4 |
| **反向借鉴（不抄其缺陷）** | 8 | 1 |
| | **合计** | **10** |

**可执行结论（下一阶段）**

1. **无需新增槽位**：`kButtonCount = 4` 保持不变（行 2、3）。
2. **无需新增面板**：不引入二级菜单面板（行 4、5）。
3. **不引入账号 / 皮肤入口**（行 6、7）。
4. **可加强的只有两处**：① Toggle 型开关的「名称 + 当前态」呈现（行 4，已由 tooltip 落地）；
   ② 状态区文字回显（行 3，P0-4 已落地，**本轮不重复实现**）。
5. **保持不变的两条硬约束**：常驻真相源仍是 `quickpanel.state`（R7）；显示路径零阻塞 I/O（P2）。

---

## 3. 本轮**不列入**的候选项（无观察 ⇒ 不写行）

按「无证据不下结论」规则，以下候选**因本轮未观察到行为而未成为矩阵行**，
留待补采后再评（不预判结论）：

| 候选项 | 为什么没写行 |
|---|---|
| 状态栏右键菜单 | 未成功触发（`quickpanel-behavior.md` §6-1） |
| 搜狗的「快捷设置 / 属性配置」独立分页面板 | 只观察到 `≡` 面板，未确认是否存在另一套（§6-2） |
| 各槽点击后的生效时机 / 开关态回显 | 逐项点击未执行（§6-3） |
| 状态栏拖拽与落点持久化 | 未做拖拽实验（§6-6、§6-7） |
| `⌄` 展开更多候选的效果 | 点位未命中（§6-8） |
| 面板二级页（更多输入 / 帮助 / 定制状态栏 / 全局设置） | 未点击进入（§6-9） |
| 搜狗皮肤预览消息面 | 仅见 `SendSkinPreviewMessage` 导出名（静态证据），**无运行时行为观察** |

---

## 4. 与不变量 / 合规的对照

| 不变量 | 本矩阵如何守住 |
|---|---|
| **P2**（TSF 侧禁阻塞 I/O） | 行 3 明确状态来源为服务端内存快照 + 100 ms timer，**不经 TSF 查询** |
| **R7**（`quickpanel.state` 为常驻唯一真相源） | 行 1、行 4：位置另存 `quickpanel.position`，**不新增第二份常驻状态**；Toggle 若做也必须同步同一真相源 |
| **spec 055**（`RestoreAlwaysShowMode()` 绝不 auto-show） | 行 1 明确保留该语义 |
| **IPC 四边同步** | 本矩阵**未新增任何 IPC 命令** ⇒ 不触发四边同步；若后续新增须一次性补 server / TSF / deployer + ADR + `test\TestWeaselIPC\*` |
| **P3**（不改 `librime/`） | 行 9 明确不动候选条 / librime |
| **图标唯一性** | 行 2：所有图标自有 PNG，过 `L1-quickpanel-icon.ps1`「无重复 base64」 |
| **不抄搜狗资产** | 行 2 / 6 / 7：不引入搜狗 logo、账号、皮肤包、`.cupf`、专有文案；只取**形态级行为规格** |
| **永不连接 185 主机** | 行 6 / 7：明确拒绝任何需要联网的入口 |

---

## 5. 收尾状态

- P0-1 证据：`F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\evidence\quickpanel-behavior.md`
  （+ `evidence\quickpanel\` 16 个截图 / 日志 / 脚本）
- P0-2 矩阵：本文件
- **本轮未改动任何生产代码**，故无单测 / 门禁需重跑；`kButtonCount` 与 IPC 边界均未触碰。
