# 0044 · 语言栏项退出常驻托盘：中英文图标不再钉在通知区域

- 状态：已接受 (accepted)
- 日期：2026-11-08
- 取代 / 修订：ADR 0030（快捷设置栏与状态图标入口）、ADR 0038（品牌图标右键菜单与左键面板）
- 关联：ADR 0012（语言栏文字「英/中」）、`.specify/specs/099-quickpanel-sogou-benchmark/`（批次 E / 批次 J）
- 代码：`WeaselTSF/LanguageBar.cpp` `CLangBarItemButton::GetInfo()`

## Context

任务栏通知区域长期钉着一个「英/中」小图标。它的来源不是小狼毫自己的托盘
（`output/data/weasel.yaml` 的 `display_tray_icon: false` 已关闭 WeaselServer 托盘），
而是 **TSF 语言栏项** `CLangBarItemButton`：批次 E 为「找回被 spec 073 取消的状态入口」，
给 `GetInfo()` 的 `dwStyle` 加了 `TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY`。

副作用是用户没预期到的两点：

1. **常驻**：`SHOWNINTRAY` 的语义是「该项始终显示在通知区域」，与 Windows 默认的语言栏
   行为（跟随输入法界面设置，可折叠 / 可隐藏）相反。用户要的是「状态入口」，
   得到的是「又一个关不掉的钉子」——与 `display_tray_icon: false` 的意图直接冲突。
2. **职责重复**：批次 H2 / I 已经把状态入口做进快捷设置栏（品牌图标左键开面板、
   面板内「英/中」「半/全」两键 + 悬停 tooltip 报当前标点态）。托盘那个图标
   不提供任何面板没有的能力，只多占一格任务栏。

约束：

- **左键开快捷设置栏**这条链路依赖语言栏项存在（`OnClick()` 走
  `PostMessage(WeaselTSF::serverHwnd(), WM_USER_TRAY_ICON, ...)` 给服务端），
  同时 `GetText()` 的「英/中」文字仍是 IME 状态条的可见状态显示（ADR 0012）。
- `TF_LBI_STYLE_INVISIBLE`（批 A 曾用）会**把整个语言栏项删掉**，
  一并失去状态文字与左键入口 ⇒ 不是本意的解法。

## Decision

**只摘掉 `TF_LBI_STYLE_SHOWNINTRAY`，其余风格位与交互全部保留。**

1. `dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_BTN_MENU`
   —— 项仍在输入法语言栏里（状态文字「英/中」可见、tooltip 可用、左键仍可开面板），
   但不再被钉在通知区域；是否显示交给 Windows 的「语言栏」设置与系统托盘折叠策略。
2. `GetIcon()`、`OnClick()`（左键开面板 / 右键弹维护菜单，ADR 0038）、`Show()`、`SetDefault()`
   一律不动 —— 本决策只改变「常驻与否」，不改变「有哪些能力」。
3. 快捷设置栏成为状态与切换的**唯一常驻入口**；托盘不再有第二份重复入口。
4. 明确否决 `TF_LBI_STYLE_INVISIBLE`（删项，丢状态显示与左键）与
   `TF_LBI_STYLE_TEXTONLY`（批 B 已否：TSF 字体渲染与 IME 原生段不一致）。

## Verification (2026-11-08 实测)

- UIA 枚举通知区域（`tmp/v12-tray-j.txt`）：**并存两个「任务栏输入指示」按钮** ——
  ① 火流猩自己的，`name='任务栏输入指示 左键切换模式，右键打开菜单'`（tooltip 逐字等于 `LanguageBar.cpp:150`），图标当前是方框「中」；
  ② Windows 自带 IME 指示器，`name='任务栏输入指示 简体中文(中国大陆) 火流猩输入法…'`，图标是红猿头 logo（由框架 / 系统按输入配置文件创建，**不是本仓库 `LanguageBar.cpp` 的按钮项**），本 ADR 的改动对它无效，要关只能走 Windows 设置 / 组策略。
- ⇒ **判据修正**：不得按「英/中」字形计数（我们的项在 UIA 中以 tooltip 文案出现、glyph 名为空，必然 0 命中 = 假阴性）。正确判据 = 按 tooltip「左键切换模式，右键打开菜单」匹配，或数「任务栏输入指示」按钮个数（改前 2 个，改后应 1 个），并与抓屏裁片互证。
- 本改动**尚未上机**：装机副本 `weaselx64.dll` 被 17 个在跑 64 位进程占用、无法替换（x86 `weasel.dll` 已替换）。故 ① 号按钮当前仍在；生效需注销 / 重启 Windows 后重装。

## Consequences

- ✅ 通知区域不再常驻「英/中」图标，符合用户对「这个图标不需要常驻」的诉求，
  也和 `display_tray_icon: false` 的意图一致。
- ✅ 面板、左键、状态文字、右键菜单等既有能力零损失；改动是**单点、可回退**的
  （加回一个 flag 即恢复批次 E 形态）。
- ⚠️ 语言栏项的可见性从此**由宿主/系统决定**，不再是 Fluxing 说了算：
  部分用户环境可能连状态条一起隐藏，届时「英/中」文字也看不到。
  本机是否连带隐藏状态条，需在重装 + 重启宿主后真机确认（批次 J handoff 记录取证结果）。
- ⚠️ 该项由宿主进程的 TSF 线程加载 —— **改动要重装 `WeaselTSF` 组件并重启宿主进程**才生效，
  必要时注销/重登。任何「图标还在不在」的结论必须以重装后的真机为准。
- ⚠️ 若后续需要「常驻但可折叠」，Windows 没有提供第三种语言栏风格位；
  只能改为 Fluxing 自己画一个托盘图标（新 ADR），不要重新加回 `SHOWNINTRAY`。

## 验证锚点

```powershell
# 源码侧：风格位里必须没有 SHOWNINTRAY
Select-String -Path WeaselTSF\LanguageBar.cpp -Pattern 'SHOWNINTRAY|TF_LBI_STYLE'
# 真机侧：重装后枚举通知区域，确认无「英/中」项（见 tmp/ 托盘取证脚本与截图）
```

## 修订（ADR 0053, 2026-11-10）

**本 ADR 的机制前提被证伪，由 [ADR 0053](0053-langbar-item-permanently-hidden.md) 修订。**

- `TF_LBI_STYLE_SHOWNINTRAY` 在现代 Windows 上是 **no-op** —— 微软官方文档对该标志写明
  「除语言栏外，项目还将显示在通知图标托盘中。**目前不支援此标志。**」
  ⇒ 摘掉它**不会**让通知区域少一个图标。
- 2026-11-10 真机复核：装机 `weaselx64.dll` / `weasel.dll` 的 SHA256 与构建产物**逐字节一致**、
  注册表 CLSID `InprocServer32` 指向装机副本、x64 `LanguageBar.obj` 的 mtime 晚于源文件
  ⇒ **本 ADR 的改动确实在机**，但通知区域里那个
  「任务栏输入指示 左键切换模式，右键打开菜单」（= 方框「中」/「英」图标）**依然在**
  ⇒ 再次证明「没上机」不是原因，**机制本身**才是。
- 现行做法（ADR 0053）：`dwStyle` 置 `TF_LBI_STYLE_HIDDENSTATUSCONTROL`，
  `Show()` 恒置 `TF_LBI_STATUS_HIDDEN` ⇒ 该项被文档化地隐藏。
- 本 ADR 里仍然成立、继续生效的部分：**否决 `TF_LBI_STYLE_INVISIBLE`**（会删掉语言栏入口）
  —— ADR 0053 同样不采用它。
