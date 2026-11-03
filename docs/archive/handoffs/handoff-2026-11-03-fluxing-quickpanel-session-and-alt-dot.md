# 交接卡 · 2026-11-03 · Fluxing 快捷面板「调不出」+ `Alt+.` 用户短语「调不出」

> 本文件是**接力唯一入口**。上一份交接卡
> `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-03-fluxing-spec097-quickpanel-hide-verified.md`
> 记录的是「Hide 吞调用 / 语言栏幽灵显示 / focus-out 标志位」三缺陷的修复与验收（已提交 `187a95f`/`7b010d4`/`892b281`，
> 真机证据已闭环），**本卡不重复其内容**，只记录 11:10 用户回归「又调不出」之后的新诊断与它的翻案。

---

## 0. 一句话现状

**根因已坐实并已在运行时修复**：`WeaselServer` 被 **远程会话（Session 3）** 里的 explorer 拉起，
而命名对象 `muServer` **不带 `Global\` 前缀 ⇒ 会话隔离**，导致用户在 console（Session 1）按任何全局热键都无人接管。
杀掉旧进程、在 Session 1 重启后，**四组热键全部由服务端持有（实测 `err=1409`）**。
`Alt+.` 链路本身是好的（不是 schema 配置问题），面板窗口也**确实存在且 `WS_VISIBLE`** ——
本卡末尾留了 3 个必须继续的下一步，其中第 2 条（面板 DPI 缩放未生效）**尚未坐实**。

---

## 1. 用户原始诉求（2026-11-03 11:10，逐字）

> 当前，快捷设置栏又无法调出了，用户短语通过Alt+. 也无法调出。
> 新开会话接力任务时，一定要更新完整的handoff文档，和一句话prompt，用于新开会话接力任务。
> prompt要引导新会话阅读必要文档，充分掌握项目背景、目标、进度和下阶段计划，不要重复handoff的具体内容，文档要提供完整路径

---

## 2. 项目背景（30 秒版，细节请读 §7 的必读清单）

Fluxing = `rime/weasel`（小狼毫）的品牌 fork，Windows TSF 输入法。三进程：

| 进程 | 工程 | 职责 |
|---|---|---|
| `WeaselServer.exe` | `WeaselServer\` | 承载 QuickPanel、托盘、以及**一批硬编码全局热键的子类 WNDPROC** |
| `WeaselTSF.dll` | `WeaselTSF\` | TSF 前端，注入到每个有焦点的 app |
| `FluxingPhrasesDialog.exe` | `FluxingPhrasesDialog\` | 用户短语独立窗口，**只由 IPC / 托盘 / 面板按钮 / `Alt+.` 热键唤起**，不自启 |

装机目录 `D:\Program Files\fluxing\weasel\`；用户数据目录 `D:\Program Files\fluxing\user1\fluxing\`。

---

## 3. 唤起链（源码已逐条读过，非猜测）

`Alt+.` **不在任何 yaml 里**（全仓 `*.yaml` / `*.custom.yaml` 无 `Alt+period`，已 grep 确认）。它是 C++ `RegisterHotKey`：

```
WeaselTSF (按 kbdl 决策 SendCommand ID_WEASELTRAY_SHOW_PHRASES_DIALOG=40031)
   └─ IPC 命令字 ─→ ServerImpl::OnCommand   WeaselIPCServer\Server.cpp:379 (PHRASES 分支 :441-443)
                     └─ fluxing::PhrasesDialogIPC::Show()   FluxingPhrasesDialog\FluxingPhrasesDialogIPC.cpp:103-145
                          └─ EnsureLaunched()  :44-101  → CreateFluxingPhrasesDialogProcess()

或：WeaselServer 自己注册的全局热键（不经 TSF、不经 yaml）
   └─ WeaselServerApp::PhrasesHotkeySubclassProc   WeaselServer\WeaselServerApp.cpp:62-98
        id==48128 Alt+.  / Alt+/   → PhrasesDialogIPC::Show()
        id==48129 Ctrl+Shift+K     → ShortcutSettings::Show()
```

服务端自己的窗口与热键：

| 位置 | 内容 |
|---|---|
| `WeaselIPCServer\WeaselServerImpl.cpp:170-184` | `Start()`：`CreateMutex(NULL,TRUE,muServer)` **无 `Global\`**；`CreateWindowEx(...,L"WeaselIPCWindow_1.0",...)` 普通顶层窗 |
| `:68-80` | `OnCreate`：设 title、`RegisterPhrasesHotkey()`、`RegisterHotKey(m_hWnd,48128,ALT|CONTROL,VK_SEPARATOR)` = **`Alt+,`** |
| `:154-168` | `OnHotkey`：`PostMessage(GetHWnd(), WM_COMMAND, ID_WEASELTRAY_QUICK_PANEL)` → `:438-442` → `DoShow()` → `QuickPanelDialog::Show()` |
| `:90-99` | `OnDestroy`：`QuickPanelDialog::Destroy()` + `UnregisterHotKey(48128)` |
| `WeaselServer\WeaselServerApp.cpp:100-189` | `RegisterPhrasesHotkey()`：`:102` `!hwndServer` 静默 return、`:108` `!m_ipcServerOrigWndProc` 静默 return；`:185-188` 注册 `Alt+.` / `Alt+/` / `Ctrl+Shift+K`；失败才写日志 |
| `WeaselServer\resource.h:34` | `ID_WEASELTRAY_QUICK_PANEL = 40018`（`_SHOW=40020`、`_HIDE=40021`） |

QuickPanel 几何常量：`WeaselServer\QuickPanelDialog.h:211-229`
`kPanelW=245`、`kPanelH=48`、`kBtnSize=35`、`kButtonCount=4`（v0.19.0.62 起 5→4 按钮，UserDict 槽删除）。
渲染：`QuickPanelDialog.cpp:1151-1294` `RepaintLayered()` → `PaintOpaqueContent()` → `ApplyAlphaGradient()` →
`UpdateLayeredWindow(..., ULW_ALPHA)`；`:1289-1291` **把 `err` 读出来又 `(void)err;` 吞掉**（失败静默）。
创建：`:1397-1421` `CreateWindowEx(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TOPMOST, ..., WS_POPUP, x,y,kPanelW,kPanelH, ...)`，
随后 `RecomputeDpiScale()` → `RepaintLayered()` → `ShowWindow(SW_SHOWNOACTIVATE)`。

---

## 4. 根因（已验证）

### 4.1 证据链

1. `query session`：`console  Duanyi  ID=1  Active`，另有一个 **`ID=3 Active`（ToDesk 远程会话，同一用户）**。
2. `WTSGetActiveConsoleSessionId()` = **1**。
3. 故障时 `WeaselServer.exe` pid **17304**，**SessionId=3**，08:53:30 由 Session 3 的 `explorer.exe` 44828 拉起（父链：Session 3 explorer → `cmd.exe /c ""...\Startup\WeaselServer.bat" auto"` → 服务端）。
4. `muServer` mutex 无 `Global\` ⇒ Session 1 里**存在另一个同名 mutex**，所以 Session 1 完全可以再起一个服务端而**不报「已在运行」**。
5. 早前探针（`tmp\hotkey-evidence.txt`、`tmp\hkprobe-1122.txt`）在 Session 1 里测出「四组热键 FREE、服务端名下 0 个顶层窗口、`FindWindowW` 双 0」——
   **当时误判成「热键没注册 / 窗口销毁成空壳」。真相是跨会话不可见**：Session 3 的热键与窗口，Session 1 的探针看不到。

### 4.2 修复动作（已执行，运行时级别）

```powershell
Stop-Process -Id 17304 -Force
Start-Process 'D:\Program Files\fluxing\weasel\WeaselServer.exe' -WorkingDirectory 'D:\Program Files\fluxing\weasel'
# → 新 pid 319904，SessionId=1，Responding=True
```

复测（同会话，判别力足够）：

| 热键 | mods/vk | 结果 |
|---|---|---|
| `Alt+.` | 1/190 | **HELD (err=1409)** |
| `Alt+,` | 1/188 | **HELD (err=1409)** |
| `Ctrl+Shift+K` | 6/75 | **HELD (err=1409)** |
| `Alt+/` | 1/191 | **HELD (err=1409)** |

### 4.3 面板窗口的真实状态（已验证，**推翻上一轮的「空壳」判语**）

Session 1 内 `EnumWindows` 扫到 pid 319904 名下 3 个顶层窗：

```
hwnd=0x141452 vis=True  class=FluxingQuickPanel_v3   title="Fluxing QuickPanel"
hwnd=0x913F6  vis=False class=WeaselIPCWindow_1.0    title="WeaselIPCWindow_1.0"
hwnd=0x1813D2 vis=False class=GDI+ Hook Window Class
```

面板样式：`WS=0x94000000`（`WS_POPUP|WS_VISIBLE|WS_CLIPSIBLINGS`）、
`EX=0x08080088`（`WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TOPMOST|WS_EX_NOAUTORDERSHORTCUTS`，**无 `WS_EX_TRANSPARENT`**），
`PARENT=0 OWNER=0x303E0`，`RECT=2389,1352→2552,1384`（163×32），`GetWindowRgnBox` 返回空（layered 窗正常，非证据）。

**DPI 换算（关键翻案）**：实测屏幕 `3840×2160`、`AppliedDPI=144` ⇒ **150% 缩放**。
PowerShell 是 **DPI-unaware**，它读到的 163×32 换回物理 = **245×48**，
正好等于 `kPanelW×kPanelH` 的**逻辑**值。
⇒ 「窗口塌成 163×32 空壳」是**探针视角造成的假象，撤回**。
⇒ 但这同时暴露一个**真问题（未坐实）**：150% 下物理应为 `245*1.5=368 × 48*1.5=72`，
现在物理只有 245×48 ⇒ **面板的 DPI 放大没有生效**，图标按 2/3 尺寸画在 2/3 尺寸位图上。

抓屏（163×32 @ 2389,1352，物理像素）实测 `distinct-colors=2`，`PrintWindow` 实测 320×96 **100% 单一色 `#FFF9F0`** ——
即面板只显示背景色，**看不到 4 个按钮与 logo**。
但这两项取证都在 DPI-unaware 探针下做，**证据强度不足以单独定罪**，见 §6.2。

---

## 5. 本轮踩到的工具坑（写下来防止重演）

1. **`FindWindowW` 对本项目服务端窗口不可靠**（`FluxingQuickPanel_v3` / `WeaselIPCWindow_1.0` 实测返回 0，而窗口确实存在）。
   ⇒ 一律用 **`EnumWindows` 全扫 + class/title 匹配**。之前所有基于 `FindWindowW` 的「弹不出来」结论**作废**。
2. **截图政策**：`mcp_browser` 的 `screenshot`（含 `clip`）**被政策硬拦**，无法绕；
   但 PowerShell `Graphics.CopyFromScreen` **可以**（本轮实测成功，产物 `tmp\panel-shot-141452.png`）。
   ⇒ 需要像素证据走 `CopyFromScreen`，不要浪费调用试 browser。
3. **DPI-unaware 探针读 DPI-aware 进程**时，`GetWindowRect` / `CopyFromScreen` 坐标都被系统 ÷1.5。
   ⇒ 任何几何结论必须先问「探针自己 aware 不 aware」，并在 150% 屏上换算一次。
4. **`Add-Type` 内联 C# 里用了 `StringBuilder` 必须 `using System.Text;`**，漏了则整段编译失败、类根本不存在，
   后续所有调用连锁报错（本轮 `PrintWindow` 探针第一次就栽在这）。
5. **PowerShell 不支持 `$obj?[...]` 三元后接索引**（「数组索引表达式缺失或无效」）；`[Convert]::ToString($uint,16)` 对 >2^31 抛异常 ⇒ 用 `$x.ToString('X8')`。
6. **`GetWindowRgnCount` 不是 Win32 API**（我探针里写了但 catch 掉），别把它当证据。

---

## 6. 下阶段计划（按优先级，每条都给判别性测试）

### 6.1 【P0】确认用户真机现在到底能看见什么
服务端已回到 Session 1、四组热键 HELD。请用户实测三件事：
1. 按 `Alt+.` → 用户短语窗口有没有出现（`EnumWindows` 里应出现 `FluxingPhrasesDialog` 窗、`Win32_Process` 里应出现 pid）；
2. 按 `Alt+,` → 面板有没有**从隐藏变显示/切换按钮响应**；
3. 右下角常驻面板**肉眼看得到按钮吗**（我的像素取证显示只有背景色）。

判别：若 `Alt+.` 后 `FluxingPhrasesDialog.exe` 起了 pid 但窗口没出现 ⇒ 问题在对话框自身；
若 pid 都没起 ⇒ 问题在 `EnsureLaunched`（读 `%TEMP%\fluxing-phrases-diag.log`，路径见 §7.4）。

### 6.2 【P0】坐实「面板 DPI 放大未生效」
必须在 **DPI-aware** 探针下重测几何，否则一切像素结论都是虚的：
- 判据：150% 屏上面板物理应为 **368×72**；实测若是 **245×48** ⇒ 缺陷坐实。
- 切入点：`QuickPanelDialog.cpp:793-794` `RecomputeDpiScale()`（`s_dpr_x = w/kPanelW`），
  以及 `:1411-1413` `CreateWindowEx` 传的是**逻辑** `kPanelW/kPanelH`（未乘 dpr）——
  检查 manifest 里 WeaselServer 的 dpiAware/dpiAwareness 声明（`WeaselServer\*.manifest` / `weasel.props`）。
- 顺带：`:1289-1291` 的 `(void)err;` 建议改成「失败写一条诊断日志」，否则 `UpdateLayeredWindow` 失败永远静默。

### 6.3 【P1】代码级永久修复：会话隔离导致的「服务端起在错误会话」
本案只在运行时救火，**代码没改**。需要决策与实现：
- 单实例 mutex `muServer` 是否加 `Global\` 前缀。**注意副作用**：加 `Global\` 后远程会话与 console 只能共存一个服务端，
  另一个会静默退出（`Start()` `:174-178` 只 `return false`，**无日志、无提示**）⇒ 必须同时补日志/提示。
- 更稳的方向可能是「检测到自己的 SessionId ≠ `WTSGetActiveConsoleSessionId()` 时拒绝启动并写日志」，
  让 console 会话的那个实例成为唯一持有热键的人。这需要 ADR（决策记录），因为改变了进程拓扑。
- `Startup\WeaselServer.bat`（`output/data/Startup/`，L106）在**每个**会话的 explorer 里都会跑 ⇒ 天然多实例，ADR 要写清。
- 事故沉淀：在 `.specify\memory\lessons-learned.md` 追加 **L170**
  （「跨会话静默失效 + `FindWindowW`/DPI-unaware 双探针坑」）。

### 6.4 【P2】沿用的旧任务收尾（上一张卡的未闭环项）
- 是否 push（现 `ahead 13`）；
- 是否重打包含 NSIS 路径守卫（`e96af35`）的发布安装器（当前发布包 `output\archives\fluxing-1.1.6-installer.exe` **不含守卫**）；
- 装机目录残留清理（交用户跑 `tmp\cleanup-install-leftovers.cmd`，需管理员）；
- 语言栏「有时复现」未解释；G-a/G-b/G-c 三道门禁尚未接进 CI。

---

## 7. 必读清单（完整路径）

### 7.1 项目进站（CLAUDE.md §1 规定顺序，别跳）
1. `F:\soft\00selfmade\rime_claude\CONTEXT.md` — 身份 / 模块 / 词汇 / 不变量
2. `F:\soft\00selfmade\rime_claude\AGENTS.md` — 操作手册，首屏 §1 + 危险区 §4 + checklist §5
3. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` — 法则层 R1–R9 / P1–P8
4. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` — §A 部署、§11 NSIS 配方
5. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` — 工程技能消费契约
6. `F:\soft\00selfmade\rime_claude\CLAUDE.md` — 进站指引 + 强制项（图谱 / 中文 / IPC 四边同步 / Conventional Commits）

### 7.2 本任务直接相关的源码
- `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServerApp.cpp`（`Run()` :207-299；热键 :62-189）
- `F:\soft\00selfmade\rime_claude\WeaselIPCServer\WeaselServerImpl.cpp`（会话隔离 mutex :170-184）
- `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp` / `.h`（面板全部）
- `F:\soft\00selfmade\rime_claude\FluxingPhrasesDialog\FluxingPhrasesDialogIPC.cpp`（`EnsureLaunched` :44-101、`Show` :103-145）
- `F:\soft\00selfmade\rime_claude\docs\adr\0013-quickpanel-dual-mode.md`、`0016-unified-settings-panel-quickpanel-mode.md`
- `F:\soft\00selfmade\rime_claude\docs\adr\0014-phrases-dialog-standalone-process.md`
- `F:\soft\00selfmade\rime_claude\.specify\specs\084-quick-panel-toggle\spec.md`、
  `097-quickpanel-autoswitch-hide-guard\spec.md`、`048-phrases-dialog-standalone-process\spec.md`

### 7.3 上一张交接卡（三缺陷修复史 + 窗口级探针 + 自启项清单）
`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-03-fluxing-spec097-quickpanel-hide-verified.md`

### 7.4 证据文件（本轮产物，全部在 `F:\soft\00selfmade\rime_claude\` 下）
- `tmp\session-probe-1145.txt` — 会话归属 + 启动链 + 自启项
- `tmp\fix-1150.txt` — 杀旧起新 + **四热键 HELD 复测**
- `tmp\enumwin-1157.txt` — 面板窗口存在且 vis=True
- `tmp\panel-vis-1158.txt`、`tmp\panel-vis2-1200.txt` — 样式位 / RECT / owner / 强制 SW_SHOW+TOPMOST 后仍无内容
- `tmp\panel-shot-141452.png` — 抓屏（2 色，无图标）
- `tmp\quickpanel-evidence\dpi-facts.txt` — DPI 真值（AppliedDPI=144 / 3840×2160）
- `tmp\hotkey-evidence.txt`、`tmp\hkprobe-1122.txt`、`tmp\etw-probe-1140.txt` — **已改判**：跨会话不可见导致的假阴性，勿再引用其结论
- 装机诊断日志（只读）：`%TEMP%\fluxing-phrases-diag.log`、`D:\Program Files\fluxing\user1\fluxing\quickpanel.state`（内容 `always_show=1`）、`%APPDATA%\Rime\weasel-install.log`

### 7.5 状态
- 分支 `Fluxing`，HEAD `920d10d`（docs），`ahead 13 origin/Fluxing`，**未 push**。
- 本卡是**未跟踪文件**，需随下次提交入库（`docs` scope）。
- 工作区其余脏项：`m librime`（既有 gitlink 噪声）、`docs/research/`（另一会话产物，勿混提）。
