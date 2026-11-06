# 0038. 右键菜单唯一化：语言栏三套 MENU 资源下线，右键入口迁到「快捷设置栏」品牌图标

- **状态**: Accepted
- **日期**: 2026-11-07
- **决策者**: Fluxing maintainers
- **相关**: ADR 0024（语言栏菜单去掉「快速面板常驻」项）；ADR 0037（搜狗用户词库导入入口）；spec 091（快捷面板跟随 IME 激活）；spec 037；constitution R7（单一事实源）/ P2（TSF 回调内不开阻塞 I/O）/ P4（提交规范）/ R23（文档铁律必须与代码字面一致）；lessons-learned L188（构建假失败）

## Context

用户在火流猩上右键输入法图标，发现菜单里**没有**「导入搜狗用户词库」，并提出一个
更基础的判断：**火流猩是不是有两套右键菜单？如果有，其中一套是无效的，应该去掉，
免得混淆和改错地方。**

二进制资源级取证（`LoadLibraryEx(LOAD_AS_IMAGE_RESOURCE)` + `EnumResourceNames(RT_MENU)`
+ `LoadMenu` + `GetSubMenu(0)` + 逐项 `GetMenuString`）坐实了「两套」这个事实，
但**推翻了「其中一套无效」这个前提**：

| 归属 | 资源 | 条目数 | 是否含「导入搜狗用户词库」 |
|---|---|---|---|
| `WeaselServer.exe` | `RT_MENU` id=105（仅此一套） | 18 | ✅ id=40022 |
| `weaselx64.dll` / `weasel.dll` | id=105（英）/ 108（繁）/ 109（简） | 各 18 | ✅ id=40022 |

两套**都有效、都含搜狗导入项**。真正的问题不是「一套无效」，而是：

1. **两套菜单措辞不一致**（TSF 版「快捷设置栏 (&K) Alt+,」「退出算法服务 (Q)」；
   Server 版「快速面板 (&K)」「退出 (&Q)」），同一功能两个文案。
2. **用户只发现其中一套**：用户右键的是 Win11 任务栏的**输入指示器**（TSF 语言栏按钮），
   不是系统托盘里的 `WeaselServer` 图标 —— 于是「找不到搜狗导入项」的观感来自
   **截图时宿主进程仍持有改前 DLL**，而不是菜单内容缺失。
3. 用户明确要求：**把右键菜单转移到右键点击「快捷设置栏」的品牌图标**，并
   **去除在任务栏图标右键调出**。

## Decision

**D1 — 菜单资源唯一化到 `WeaselServer.exe` 的 `IDR_MENU_POPUP`。**
`WeaselTSF/WeaselTSF.rc` 里三套 MENU 资源（id=105/108/109）**整体删除**，
`include/resource.h` 中对应的三个 `#define` 一并删除。菜单只剩一份定义。

**D2 — 新入口：「快捷设置栏」品牌图标右键。**
`QuickPanelDialog` 新增 `SetOnBrandRightClick` 回调与 `BrandRect` / `BrandRectForSize`
（品牌图标物理矩形的**单一事实源**，与 `HitTest` 的 `-2`（brand）判定同源 ⇒
命中区与右键区逐像素重合）。面板**只做命中 + 转发屏幕坐标**，不自己 `LoadMenu`。
回调由 `WeaselServerApp` 注入，调用 `CSystemTray::ShowPopupMenuAt(POINT)` ——
即把托盘右键那段 `LoadMenu → GetSubMenu → SetMenuDefaultItem → CustomizeMenu →
SetForegroundWindow → TrackPopupMenu` 抽成公开方法后**由两个入口共用**。
于是菜单资源、命令目标窗口（`GetTargetWnd()`）、勾选态（`CustomizeMenu`）三者
天然一致，**不再存在两套菜单各说各话**。

**D3 — 任务栏语言栏右键弹菜单下线。**
`WeaselTSF/LanguageBar.cpp` 删除 `TF_LBI_CLK_RIGHT` 分支；`InitMenu` 不再
`LoadMenuW`（资源已删，`LoadMenuW` 必返 NULL，继续 `GetSubMenu` 会崩），
改为**不注册任何菜单项**。任务栏输入指示器右键只剩系统自己的语言列表。

**D4 — 不新增任何 IPC 协议。**
快捷设置栏与菜单同在 `WeaselServer` 进程内，菜单命令经既有 `WM_COMMAND` →
`m_server.GetHWnd()` → `AddMenuHandler` 体系路由；`ID_WEASELTRAY_SOGOU_IMPORT`
（40022）等 handler 原样复用。⇒ **不动 `include/WeaselIPCData.h`**，不触发
IPC 四边同步。

**D5 — 本轮不扩展 Server 侧三语言。**
`WeaselServer.rc` 的三块菜单由 `#if !defined(AFX_RESOURCE_DLL) || defined(AFX_TARG_CHS/CHT/ENU)`
包裹，是**构建期语言**，当前安装版只编入 CHS 一套。删除 TSF 三语言菜单后，
繁体/英文用户只剩 Server 的构建语言菜单 —— 这与**改动前托盘菜单的行为完全一致**
（托盘菜单本来就只有构建语言），属**非相对现状的回退**，故本轮不扩。

**D6 — spec 091 AC-5 作废。**
spec 091 AC-5 假定「语言栏右键仍能弹出菜单」，与本决策直接冲突。AC-5 改为
「语言栏不再承载菜单；右键入口在托盘图标与快捷设置栏品牌图标，二者同一份菜单」。

## Consequences

- **正面**：菜单定义唯一、措辞唯一；用户想找「导入搜狗用户词库」时，无论从托盘还是
  从品牌图标进，看到的都是同一份菜单、同一个 ID（40022）；任务栏输入指示器不再
  弹一份措辞不同的菜单。
- **正面**：新增入口落在**用户真正看得见的「快捷设置栏」**上，而不是藏在系统托盘里。
- **代价**：任务栏输入指示器右键不再有火流猩菜单项（只剩系统语言列表）——
  这是用户明确要求的「去除在任务栏图标右键调出」。
- **代价**：繁体/英文用户会失去 TSF 那份本地化菜单（只剩 Server 构建语言菜单），
  见 D5；后续若要恢复多语言，正确做法是给 Server 侧补三语言资源，而不是把
  TSF 那套菜单复活。
- **代价**：菜单弹出期间面板不得自动隐藏 —— 用 `s_menuOpen` 屏蔽两条自动隐藏路径
  （`WM_ACTIVATEAPP` 前台切走 / 鼠标移出 1500 ms），否则鼠标移到菜单上时面板会被收走、
  菜单失去依附窗口。

## Alternatives considered

- **A. 保留两套，只对齐文案**：不解决「用户在任务栏找不到搜狗导入项」的观感问题，
  也不满足「去掉一套」的明确要求 ⇒ 否决。
- **B. 在 TSF 侧实现品牌图标右键**：品牌图标画在 `WeaselServer` 的
  `QuickPanelDialog`（纯 GDI + WIC）里，TSF 进程根本碰不到它 ⇒ 技术上不成立。
- **C. 菜单下沉到共享库，让两边都能用**：等于再引入一份菜单资源，与 D1 冲突；
  且 TSF 侧弹菜单需要跨进程命令转发（新增 IPC）⇒ 否决。
- **D. 保留 `InitMenu` 注册空菜单**：会让系统弹一个空白菜单框，体验更差 ⇒ 改为
  不注册任何项。

## Verification

- **资源级**：重建后 `output/weaselx64.dll` / `output/weasel.dll` 的 `RT_MENU` 应
  **枚举不到任何模板**（三套资源已删）；`output/WeaselServer.exe` 的 `RT_MENU` 仍为
  id=105 一套、18 项、含 `id=40022 导入搜狗用户词库 (&G)`。
- **构建链**：x64 `WeaselTSF.vcxproj` + `weasel.sln /p:Platform=Win32` + `makensis`
  三步全绿（`env.bat` 须 `RELEASE_BUILD=1`，禁用 `git describe --tags` 取版本号）。
- **单测**：`test/TestQuickPanelDialog` 增加 `BrandRectForSize` 的判别性断言 ——
  起点 = padding、与槽位 0 不相交、宽高 = brand_size，在 100% / 150% / 200% 三档
  物理尺寸下成立。
- **真机**：装包后右键「快捷设置栏」品牌图标应弹出与托盘一致的菜单（含
  「导入搜狗用户词库 (&G)」）；右键任务栏输入指示器应**不再**出现火流猩菜单项。
- **未验证（如实标注）**：Win11 输入指示器右键后是否只剩系统默认语言列表，
  需真机目视确认。
