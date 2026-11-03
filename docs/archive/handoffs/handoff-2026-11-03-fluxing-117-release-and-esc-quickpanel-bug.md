# 交接卡 · 火流猩 1.1.7 发布完成 + 「Esc 连带收起快捷设置栏」根因坐实

- 生成时间：2026-11-03（15:50 前后）
- 仓库：`F:\soft\00selfmade\rime_claude`（分支 `Fluxing`，remote `https://github.com/kizemo/fluxing.git`）
- 图谱 project：`F-soft-00selfmade-rime-claude`（12,715 节点 / 38,206 边）
- 上一张卡：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-03-fluxing-quickpanel-session-and-alt-dot.md`

---

## 1. 背景与目标

长期目标：火流猩输入法（Fluxing，Weasel/小狼毫 Rime 前端 fork）对齐搜狗功能但去除广告/关联/无关功能、降低系统占用。
路线已由用户拍板并写入 ADR 0030：**继续火流猩、不做逆向重写**。

本阶段两个目标：

1. **发布 1.1.7**（用户签字指令：「push 18 个 commit；版本号 1.1.7 + makensis 打包」）。
2. **定位用户新报缺陷**：「调出用户短语界面时，使用 esc 退出用户短语界面，也会导致快捷设置栏消失」。

两项目标均已完成；缺陷**已坐实根因但未修**（见 §4）。

---

## 2. 本轮已完成

### 2.1 push 与 1.1.7 打包（全部验证通过）

- **push**：18 个 commit 已推至 `https://github.com/kizemo/fluxing.git`（`4e1b8b6..d8c2909`，tip `d8c2909`，ahead 归零）。
- **版本号**：`env.bat`（`FLUXING_VERSION=1.1.7` / `PRODUCT_VERSION=1.1.7.0` / `FILE_VERSION=1.1.7.0` / `VERSION_PATCH=7`）与 `weasel.props`（`VERSION_PATCH=7` / `1.1.7.0`）。两者实测均 git-ignored ⇒ 改版本号零提交风险（与 1.1.6 发布口径一致）。
- **发布配方**：`build.bat weasel installer`（**不是** `xbuild.bat` —— 后者无 `:stage_text_assets`，缺 LICENSE/README 素材会让 makensis 报 `LicenseData: open failed`）。
- **构建结果**：`BUILD117_EXIT=0`，日志零编译错误（`tmp\build117.log`，1155 行）。
- **产物**：`output\archives\fluxing-1.1.7-installer.exe`（42,795,316 B，NSIS 版本 1.1.7）；**1.1.6 已发布产物未动**。
- **新引擎版本资源**：`WeaselServer.exe` / `WeaselDeployer.exe` / `WeaselSetup.exe` / `weasel.dll` / `weaselx64.dll` 全部 `ProductVersion=1.1.7.0`。
  （`FileVersion` 资源为空是**历史口径**，1.1.6 装机版同样为空，非回归。）

### 2.2 装机（提权静默安装，成功）

- 命令形态（L54 规则 4 + install.nsi L373 注释）：`/S /LOG=<日志> /D=D:\Program Files\fluxing`，
  **`/D=` 值不带引号且必须是最后一个参数**（带引号 NSIS 直接忽略，L112）。
- 结果：`INSTALL_EXIT=0`；`HKLM\...\Uninstall\Fluxing` 的 `DisplayVersion=1.1.7`；`InstallDir=D:\Program Files\fluxing`。
- 注册表 InstallDir 与 `/D` 一致 ⇒ 原地升级（**未清注册表键**，L54 规则 2 只在不一致时才要求清）。
- 装机 server 自动拉起：pid 153304，session 1。

### 2.3 真机三验收（全绿，证据 `tmp\accept117-evidence.txt`）

| 项 | 结果 | 关键数据 |
|---|---|---|
| DPI 缩放 | ✅ | 面板物理 **368×72 @ `GetDpiForWindow`=144**（逻辑 245×48 正确放大），截图图标清晰 |
| 会话隔离 | ✅ | `fluxing-profile-notify.log` L128：`startup_allowed allowed=1 reason=console_session_match`（1.1.6 引擎从不写此行 ⇒ 确证新代码在跑）；判别探针 `TestWeaselIPC.exe /verify-console-session` → PASS |
| 三数字基线 | ✅ | **1 进程** · private **24.6 MB**（workset 35.8 MB，<40）· 空闲 CPU **0.00%** 单核（8 s 采样，<0.1%） |

### 2.4 缺陷根因坐实（本轮新增，见 §3）

### 2.5 文档入库

- `CHANGELOG.md`：`## [Unreleased]` → 立 `## [1.1.7] - 2026-11-03` 段（含 DPI 修复、会话隔离、installer 守卫、spec 097 等既有条目），文件顶部保留空 `## [Unreleased]` 锚点。
- `.specify/memory/lessons-learned.md`：**L171**（根因沉淀，含复现时间线、完整链路、可迁移规则）。

---

## 3. 新缺陷与根因（已坐实，**未修**）

### 3.1 症状

调出「用户短语」窗口（`Alt+.`）后按 `Esc` 关闭它 → 屏幕上的**快捷设置栏**（QuickPanel）跟着一起消失。
用户并未切走输入法，面板却按「已切走」处理。

### 3.2 运行时证据（不是读码推测）

自建探针 `tmp\quickpanel-evidence\ReproEsc.cs`（编译为 `ReproEsc.exe`）流程：

1. `WM_COMMAND 40020` 显示快捷面板 → 记录可见性；
2. `WM_HOTKEY 9002`（`ID_HOTKEY_PHRASES_DOT`，与真实 `Alt+.` **同一条路径**）调出短语窗口；
3. 逐 200 ms 采样两个窗口可见性（2 s）；
4. 向短语窗口注入**真实 Esc 键**（`keybd_event`）；
5. 再逐 200 ms 采样 3 s。

时间线（`tmp\quickpanel-evidence\repro-esc-run1.txt`）：

```
15:47:02.395  panel.visible=True
15:47:03.905  phrases.visible=True        ← 短语窗口弹出
15:47:03.905~15:47:05.992  panel.visible=True  ← 弹出后 2 秒面板仍在（不是"弹出即消失"）
15:47:05.992  >>> injecting ESC
15:47:06.378  panel.visible=False  phrases.visible=False   ← 同帧翻转 = BUG REPRODUCED
```

同刻诊断留痕（`%TEMP%\fluxing-profile-notify.log`，ADR 0029 决策 7 既有通道）新增四行，**顺序很关键**：

```
[server] focus_in profile presence=present
[server] remove_session active presence=absent
[server] focus_out param=1 presence=absent
[tsf] deactivated sent=1
```

`param=1` = `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED`。

### 3.3 完整链路

1. 短语窗口是**独立进程** `FluxingPhrasesDialog.exe`，自己 `SetForegroundWindow` 抢前台
   （`FluxingPhrasesDialog\PhrasesDialog.cpp:321-322`）。
2. `Esc` → `PhrasesDialog::Hide()` → `DestroyWindow`（`PhrasesDialog.cpp:328-351`，`967-969`）。
3. 窗口销毁那一瞬，前台回到原 app，Windows 把 profile 状态判成「火流猩不在场」⇒
   TSF 前端 `WeaselTSF::OnActivated` → `DecideProfileNotify` → `kNotifyDeactivated`
   （`WeaselTSF\WeaselTSF.cpp:456-500`）⇒ 发 `FocusOut(PROFILE_DEACTIVATED)`。
4. server `RimeWithWeaselHandler::FocusOut`（`RimeWithWeasel\RimeWithWeasel.cpp:580-621`）
   走 `ShouldHideQuickPanelOnFocusOut(param, AlwaysShowMode())`（`include\WeaselIPC.h:185-225`）
   ⇒ 判 **true** ⇒ **连长显态也收起**快捷设置栏。

### 3.4 缺口在哪（ADR 0019 为什么拦不住）

ADR 0019 只在 `QuickPanelDialog::OnKeyDown` 吞掉「**面板自己**收到的 `Esc`」
（`WeaselServer\QuickPanelDialog.cpp:1449-1455`）。
它管不到「**别的窗口**收到 `Esc` → 焦点/profile 事件 → 服务端隐藏面板」这条链路。
两处口径各管一段，缺口在中间。

### 3.5 协议层根因（决定修复形态）

当前 IPC 只有两个 profile 位（`PROFILE_ACTIVATED` / `PROFILE_DEACTIVATED`），
**无法区分**「用户切到别的输入法」与「切到火流猩自己的 UI 窗口」。
⇒ 任何修复要么在 server 端做「自家 UI 往返」抑制，要么升级协议加第三个态
（后者按 CLAUDE.md §2 触发**四边同步 + ADR + 测试**）。

---

## 4. 下阶段计划（下一会话执行）

### 4.1 主任务：修「Esc 连带收起快捷设置栏」

**用户口径**（ADR 0019 / 用户 1.1.1 验收）：显隐只靠 `Alt+,` 快捷键，**ESC 不应收起快捷设置栏**。
本缺陷正是该口径在另一条链路上的漏网。

**推荐方案（server 端最小改动，优先选它）**：

- 在 `include\WeaselIPC.h` 增设**共享纯函数**（宪法 R7：生产/测试同源），
  判定「这次 `PROFILE_DEACTIVATED` 是不是自家 UI 造成的焦点往返」；
  判据可基于现有日志顺序：`activated(present)` 先到、紧接着 `deactivated`，
  且期间自家 UI 窗口（`PhrasesDialogIPC::IsVisible()`）刚关闭 ⇒ 视为往返，**保持 present、不隐藏**。
- 在 `RimeWithWeaselHandler::FocusOut` 使用该判定：`include\WeaselIPC.h` 的
  `ShouldHideQuickPanelOnFocusOut` 需要新增「自家 UI 往返」入参或并列判定，
  **生产与 `test\TestWeaselIPC` 必须共用同一实现**。
- 若实施中发现「自家 UI 往返」与「用户真切走」在现有信号下不可区分
  （例如顺序判据不可靠），则**升级为协议方案**：新增一个 focus 标志位，
  并按 CLAUDE.md §2 走 **server / TSF / deployer 三端 + 一个 ADR + 一个测试**一次性提交。

**验收口径（必须双向）**：

- 正向：`ReproEsc.exe` 从 `BUG REPRODUCED` 变为 `NOT REPRODUCED`；
  且瞬时态、长显态（勾「快捷面板常驻」）**两种模式都要验**。
- 反向对照（不能把守卫拆太狠）：「用户真的切到别的输入法」时快捷设置栏**仍必须收起**
  （spec 091/094 口径），必须有一条探针证明这一点没被破坏。
- 变异测试：把判定反过来，确认断言翻红（沿用 T4 的做法）。

### 4.2 次任务：1.1.7 发布收尾

- `CHANGELOG` 的 `[1.1.7]` 段已立（§2.5）。
- **是否发布 GitHub Release / 上传安装器到对外渠道 ⇒ 需用户签字**（对外发布属必须先问的三类之一）。
- 管理员跑 `tmp\cleanup-install-leftovers.cmd` 清引擎目录安装残留（P0，需用户点头）。

### 4.3 仍未验证项（ROADMAP §6.4）

- 96 DPI 零回归（本机 150% 屏）。
- 跨屏动态 DPI（本机单屏，无法验）。
- 第二会话会话隔离实测（无第二登录会话；现以留痕 + 变异测试反证为限）—— 若要真机验，**需用户另开一个登录会话**。

---

## 5. 已验证 / 推断 / 未验证

### 已验证（运行时取证）

- 1.1.7 构建 `BUILD117_EXIT=0`、零编译错误、产物存在且版本资源 1.1.7.0。
- 装机 `INSTALL_EXIT=0`、注册表 `DisplayVersion=1.1.7`、引擎 ProdVer 1.1.7.0、server 自动拉起。
- 真机三验收全绿（DPI 368×72@144 / 会话隔离留痕+判别探针 PASS / 1 进程 24.6 MB 0.00% CPU）。
- **缺陷复现**：Esc 后 `panel.visible` 与 `phrases.visible` 同帧 True→False，探针退出码 1 并打印 `BUG REPRODUCED`。
- 缺陷同刻 server 收到 `focus_out param=1`（PROFILE_DEACTIVATED）。
- push：18 commit 已上远程，ahead 归零。

### 推断（未直接观测，但有强证据）

- 「短语窗口销毁 → Windows 把 profile 判成停用 → TSF 发 deactivated」这一段：
  由代码路径（`OnActivated`→`DecideProfileNotify`→`FocusOut`→`ShouldHideQuickPanelOnFocusOut`）
  与日志中 `param=1` 的实际到达共同坐实；但**「Windows 具体为何在此刻停用 profile」**未做
  TSF 级逐帧取证（需 ETW/调试器），修复时若信号不足需补这一步。

### 未验证

- 修复方案是否能在**长显态**下同样成立（长显态本就不该被「切走必隐」误伤，需专门验）。
- `env.bat` / `weasel.props` 的 1.1.7 值**未入库**（git-ignored），换机重建不会自动得到 1.1.7。
- 安装器未做「全新安装（无历史 1.1.6）」路径验证，只做了原地升级。

---

## 6. 证据与锚点（完整路径）

**本轮新建/修改**

- `F:\soft\00selfmade\rime_claude\tmp\quickpanel-evidence\ReproEsc.cs`（复现探针源码）
- `F:\soft\00selfmade\rime_claude\tmp\quickpanel-evidence\ReproEsc.exe`（编译产物）
- `F:\soft\00selfmade\rime_claude\tmp\quickpanel-evidence\repro-esc-run1.txt`（复现时间线）
- `F:\soft\00selfmade\rime_claude\tmp\accept117-evidence.txt`（1.1.7 三验收汇总）
- `F:\soft\00selfmade\rime_claude\tmp\quickpanel-evidence\installed117-panel.png`（装机 1.1.7 面板截图 @144dpi）
- `F:\soft\00selfmade\rime_claude\tmp\build117.cmd` / `tmp\build117.log`（构建包装与日志）
- `F:\soft\00selfmade\rime_claude\tmp\install117.cmd`（提权装机脚本）
- `F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.7-installer.exe`（1.1.7 安装器）
- `F:\soft\00selfmade\rime_claude\CHANGELOG.md`（`[1.1.7]` 段）
- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md`（L171，**UTF-8 with BOM**）

**根因代码锚点**

- `F:\soft\00selfmade\rime_claude\WeaselTSF\WeaselTSF.cpp:456-500`（`OnActivated` → `DecideProfileNotify`）
- `F:\soft\00selfmade\rime_claude\RimeWithWeasel\RimeWithWeasel.cpp:580-621`（`FocusOut` → 隐藏判定）
- `F:\soft\00selfmade\rime_claude\include\WeaselIPC.h:185-225`（共享纯函数 `ShouldHideQuickPanelOnFocusOut` 等）
- `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp:1449-1455`（ADR 0019 的 ESC 拦截，只管面板自己）
- `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp:772-781`（`WM_ACTIVATEAPP` 路径）、`:791-847`（polling auto-hide）
- `F:\soft\00selfmade\rime_claude\FluxingPhrasesDialog\PhrasesDialog.cpp:321-322`（抢前台）、`:328-351`（`Hide`）、`:967-969`（Esc → Hide）
- `F:\soft\00selfmade\rime_claude\WeaselServer\PhrasesDialogIPC.cpp:30-97`（`Show`）、`:99-126`（`Hide`）
- `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServerApp.cpp:439-471`（`ShowQuickPanel` 的 `onPhrases` 回调：点词库按钮会先 `QuickPanelDialog::Hide()`）

**必读文档（进站顺序，勿跳）**

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
3. `F:\soft\00selfmade\rime_claude\AGENTS.md`
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`（R1–R9 / P1–P8）
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` §A 与 §11（部署 + NSIS 配方）
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md`
7. `F:\soft\00selfmade\rime_claude\docs\adr\README.md` + ADR 0013 / 0016 / 0019 / 0030 / 0031
8. `F:\soft\00selfmade\rime_claude\docs\research\sogou-reverse-2026-11\ROADMAP.md`

**图谱**：`codebase-memory-mcp`，project `F-soft-00selfmade-rime-claude`；
顺序 `search_graph → trace_path → get_code_snippet → query_graph → get_architecture`。

---

## 7. 本轮工具坑（勿重试）

1. **`.specify\memory\lessons-learned.md` 是 UTF-8 with BOM（`EF BB BF`），不是 UTF-16** ——
   此前误记为 UTF-16 LE。`Get-Content -Encoding unicode` 因 BOM 自动探测仍能读出正常中文，
   所以误判一直没暴露；**按 UTF-16 追加会把文件写成混合编码**。
   追加前用 `New-Object System.Text.UTF8Encoding($false,$true)` 严格解码探测真实编码。
2. 追加写坏时的唯一安全撤销手段：`git -C <root> checkout -- <file>`（混合编码无法用文本工具修）。
3. `bash` 里正则含 `\\`（如 `'\\(msbuild|output|build)\\'`）会被当 UNC 路径拦下 ——
   改用 `glob` / `grep` 工具。
4. PowerShell 删文件被硬安全策略拦（`Remove-Item` 不允许）—— 脚本里用 `>` 覆盖既有文件，**别先删**。
5. `csc` 编译回调委托：必须 `static bool ...(..) { ... return true; }`（写 `void` + `return;` 会报 CS0407/CS0127）。
6. `New-Object System.Text.Encoding Unicode` 无效（无该构造器）；用 `[System.Text.Encoding]::Unicode`。
7. 静默安装 `/D=` 值**不能带引号**，且必须是**最后一个**参数（NSIS 会静默忽略带引号的 `/D=`）。
8. `build.bat weasel installer` 才是发布配方；`xbuild.bat` 无 `:stage_text_assets`，缺素材会报 `LicenseData: open failed`。
9. `git status` 里 `m librime` 是子模块脏改动 —— **不要提交**（P3 / L10：无 ADR + 用户授权禁提交）。
   `env.bat` / `weasel.props` / `tmp\` 均 git-ignored（`git check-ignore` 可验），改它们天然不入库。

---

## 8. 仓库状态（交棒时）

- 分支 `Fluxing`，remote `https://github.com/kizemo/fluxing.git`；本轮有一个新 commit（「立 [1.1.7] 发布段 + L171 根因」）待 push（**push 属对外操作，需用户签字**）。
- 工作树脏项应仅剩 `m librime`（子模块，不提交）。
- 装机版本：**1.1.7**（`D:\Program Files\fluxing`），server pid 153304（session 1）。
