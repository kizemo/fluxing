# Fluxing（火流猩输入法）· 交接文档（spec 092 / 093 已交付，§6.3 ①② 待做）

> 生成时间：2026-11-01 00:20（会话 `mvs_a2dbe55c2b7e4855970a5c4f9aa1a1bc` 收尾时写）
> 上一份：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md`
> 本文档**只写增量与当前状态**；1.1.5/1.1.6 主轴（spec 089–091）仍以前一份为准。

---

## 0. 三分钟速览

- **项目**：Fluxing = RIME 前端 `rime/weasel`（小狼毫）的品牌 fork（火流猩）。安装路径 / 用户数据 / CLSID 全部替换，与上游互不冲突。
- **三块改动面**：`WeaselServer`（常驻进程：配置 / 部署 / 托盘）、`WeaselTSF`（TSF 前端）、`WeaselDeployer` + `RimeWithWeasel`（服务端业务 / librime 胶水层）。
- **仓库**：`F:\soft\00selfmade\rime_claude`，分支 `Fluxing`，远端 `kizemo/Fluxing`。
- **当前 HEAD**：`f6cf185`（spec 093 第三提交），**已推送**，divergence `0 0`。
- **版本真源**：`env.bat` → `FLUXING_VERSION=1.1.6` / `RELEASE_BUILD=1`（`env.bat` 被 gitignore，**不要提交**）。
- **本轮新增两批交付**：spec 092（交接文档 §6.2 系统性问题清单 7 项 + 第 8 项）、spec 093（`/quit` 等 7 个命令行选项此前**全部静默失效**的修复）。
- **剩余工作**：§6.3 的 ①（常驻开关收进设置面板，形态已定）与 ②（FocusIn/FocusOut 对称位标志协议表，涉 IPC 协议变更），外加 1.1.6 装机实测。
- **1.1.6 安装包已打好**：`F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.6-installer.exe`（见 §4.3）。

---

## 1. 新会话必读顺序（全部绝对路径）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md` —— 进站指引（§1 是五份必读、§2 强制项、§4 专项规则表）。
2. `F:\soft\00selfmade\rime_claude\CONTEXT.md` —— 身份 / 模块 / 词汇 / 不变量。
3. `F:\soft\00selfmade\rime_claude\AGENTS.md` —— 操作手册（首屏 §1 + 危险区 §4 + checklist §5）。
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` —— 宪法（R1–R9 / P1–P8；**P8 三问在 213–219 行**，Quality Gates 在 226–238 行）。
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` —— 部署 + NSIS 配方（§A 与 §11）。
6. `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` —— 工程技能消费契约。
7. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-01-fluxing-1.1.6-spec093-delivery.md` —— **本文档**。
8. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md` —— 1.1.6 主轴（§6.2 清单在 198–205 行、§6.3 三方向在 206–209 行、§7 环境陷阱在 213–296 行、§8 工作流在 300–317 行）。

按需再读：

- `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md` —— 改 `output/data/**/*.yaml` 前必读（§3 补靪语法、§6/§7 action 列表、§9.3 `RELEASE_MASK` 坑）。
- `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` —— 事故溯源（**13016 行 / 810 KB**，最新一节 `L160`）。
- `F:\soft\00selfmade\rime_claude\.specify\specs\093-server-command-line-argument-parsing\{spec,plan,tasks}.md` —— 上一批的完整设计（plan.md 含 Constitution Check + P8 三问）。

---

## 2. 当前状态（2026-11-01 00:20 实测）

| 项 | 值 |
|---|---|
| 分支 / HEAD | `Fluxing` @ `f6cf185`，已推 `kizemo/Fluxing`，divergence `0 0` |
| 工作树 | **干净**（仅 ` m librime` —— 子模块本就脏，**不可提交**） |
| 索引 | `codebase-memory-mcp`：`F-soft-00selfmade-rime-claude`，**12,715 节点 / 38,206 边**，`status=ready`，`head_sha=f6cf185`（与 HEAD 一致） |
| 本机装着的版本 | **1.1.5**（`D:\Program Files\fluxing\weasel\fluxing\weasel\WeaselServer.exe`，2,193,920 B）—— **1.1.6 尚未装机** |
| WeaselServer 进程 | 无在跑 |
| 旧版嵌套残留 | `D:\Program Files\fluxing\weasel\fluxing\weasel\weasel\` 内是 0.21.0.0 —— 用户已决定**只记录不清理** |

---

## 3. 已完成的工作面

### 3.1 spec 092 —— §6.2 系统性问题清单 7 项 + 第 8 项（已提交并推送）

交接文档 §6.2 的 7 项，加上同根因查出的第 8 项，合并成一批交付。定性：**两代设计漂移**（不是单点 bug）。

- 28 个 `test/*.vcxproj` 全部加 `/utf-8`（本轮改 27 个；`TestPipeProtocol` 原本就有）。
- `TestCandidateIgnoreFilter` / `TestServerSideIgnoreMapping`：断言标签改为**由该断言块实测结果决定**（原先无条件打印 `PASS:`，探针无判别性）。
- `TestQuickPanelRefactor`：从整段 `return 0` 的 SKIP 改为按**现行契约**实跑断言（spec 045 把控件创建移到 `Show()`，其后面板整体重写为 GDI v3-rev3，老期待的「11 Fluxing + 1 native close」布局已是两代之前的设计）。
- `TestQuickPanelDialog`：同病同治 —— 删 SKIP 段 + 删 `ExitProcess` 之后的不可达死代码 + 补 T17–T19（窗口类名 == `FluxingQuickPanel_v3` / 子窗口数 == 0 / `Hide()` 不销毁且二次 `Show()` 复用同一 HWND），全文件 50/50 全过。
- `release/` 载荷仓回填 1.1.1 / 1.1.2（从 `output/archives/` 逐字节复制，SHA256 已比对；这两个包**从未进入 git 历史**）。
- `docs/adr/README.md` 补 ADR 编号索引（19 篇：0001、0007–0024）。
- `CHANGELOG.md` `[Unreleased]` 清理 + 段序归位。

提交：`e9e662a` / `f5a6d2a` / `3faa8ea`（已推）。

### 3.2 spec 093 —— `/quit` 等命令行选项加固（已提交并推送，**本轮主体**）

**根因（已坐实，不是猜测）**：`_tWinMain` 收到的是 WinMain 的**原始** `lpCmdLine`，参数前带一个空格（`WeaselServer.exe /quit` ⇒ `" /quit"`）。旧代码 7 处**整串比较** `!wcscmp(L"/quit", lpstrCmdLine)` 判定 ⇒ **恒为假** ⇒ `/q` `/quit` `/ascii` `/nascii` `/userdir` `/weaseldir` `/update` 全是**死选项**；其中 `/quit` 落进「已在运行则重启服务」分支 ⇒ **语义与用户意图相反**（要退出，结果是重启）。
对照组 `test/TestWeaselIPC` 用 CRT 的 `argv[1]`（已被 CRT 剥掉空白）所以一直正常 ⇒ 缺陷被**测试替身与生产入口契约不一致**掩盖。

**修法**：按宪法 **R7**（生产代码与测试同源，共用共享头纯函数）把解析收敛为 `WeaselServer/ServerCmdLine.h`：

- `enum class Option { None, UserDir, WeaselDir, Ascii, Nascii, Quit, Update }`
- `detail::IsSpace` / `detail::Normalize`（去首尾空白 + 剥成对双引号）/ `FirstToken`（取首个记号）/ `Parse`（记号 → Option）
- 纯函数、只依赖 `<cwchar>`、不依赖 ATL / Windows API
- `WeaselServer/WeaselServer.cpp` 的 7 处判定改读 `fluxing::servercmd::Parse(lpstrCmdLine)`；**控制流与语义不变**（判定顺序不变、`/update` 仍在重启分支之后、`/ascii`/`/nascii` 开关方向不变）
- 解析保持**精确匹配（区分大小写）**，不扩大为不区分大小写；**不新增选项**

**双层防护**：

- T1–T5 测共享头（T1 带前导空格时 7 选项全可达 / T2 `/q`≡`/quit` 且大小写精确匹配 / T3 空·空白·未知 → None / T4 记号化 / T5 **缺陷见证**：复刻修复前的整串比较，断言其对 `" /quit"` 恒不匹配）。
- T6 **调用点守卫**：运行时读 `WeaselServer/WeaselServer.cpp` 源码，断言必须出现 `servercmd::Parse` 且**不得出现任何 `wcscmp(L"/`** —— 否则把调用点改回旧写法时 T1–T5 仍会全绿。

提交：`d047e6a`（fix + 回归测试）/ `94494ec`（登记）/ `f6cf185`（文档），已推。

**归档分流**：定性为 **bugfix** ⇒ 写进 `.specify\memory\lessons-learned.md` 的 **`L160`**（`## L160 - 命令行参数带前导空格时，「整串比较」型解析会让全部选项静默失效`），**不进 ADR**。零 IPC 协议变更 ⇒ **不触发** server/TSF/deployer 三端一次性提交。

---

## 4. 验证证据

### 4.1 spec 093 的 TDD 三段

| 关卡 | 文件 | 结果 |
|---|---|---|
| RED-1 | `tmp/093-red.log` | 共享头不存在 ⇒ `error C1083: 无法打开包括文件: "../../WeaselServer/ServerCmdLine.h"`（失败原因正确） |
| RED-2（朴素基线） | `tmp/093-red2-run.log` | **16 条行为性断言失败**，精确命中 T1 的 7 个选项 + T2 + T4×4 + T5×2 + T6×2；T3 的 None 场景**保持绿** ⇒ 证明测试有判别性、不是恒绿探针 |
| GREEN | `tmp/093-green-run.log` | `=== ALL TESTS PASSED ===`，`RUN_EXIT=0`，6/6 全过（含 T6） |

### 4.2 构建与套件

- 产品构建：`tmp/093-product-build.log` —— `build.bat weasel` 走 `weasel.sln`，**0 个错误**（1 个既有 C4099 `UserDictEntry` class/struct 混用警告）。
- 全量套件：`tmp/093-suite.log` —— `=== ALL TESTS PASSED ===` + **`SUITE_EXIT=0`**；套件内含 `=== Running TestServerCmdLine.exe ===` 与 `Result: 12 passed, 0 failed`。
- 打包：`tmp/1101-package.log` —— `build.bat weasel installer`，`PACKAGE_EXIT=0`。

### 4.3 1.1.6 安装包（**本轮新产出，待装机**）

```
F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.6-installer.exe
SIZE   = 42,782,281 字节
SHA256 = 9833490705111359053B6277CFC7839680BAAC959A28F3F279E8995AEC91A21B
MTIME  = 2026-11-01 00:15:27
```

### 4.4 §6.1 验收第 8 条 —— **本轮不复验**（用户已决定）

`*.user_ignore.txt` 落盘 + 托盘「恢复被忽略的候选项」撤销链路：代码侧证据已足（ADR 0023 + `TestCandidateIgnoreFilter` 5/5 + `TestServerSideIgnoreMapping` 14/14），**真机证据留到 1.1.6 装机时一并取**。

---

## 5. 需要装机验证吗？—— 需要，且包已就绪

**判断**：需要。理由有三：
1. spec 093 改的 `/quit` **直接改变服务启停行为** —— 此前 `/quit` 实为「重启」，真机必须验证「退得干净、重启得起来」。
2. spec 092 改了 28 个测试工程的编译选项与 3 个测试的断言逻辑，**产品构建虽 0 错误，但运行期行为需要真机兜底**。
3. §6.1 第 8 条（候选屏蔽落盘 + 托盘撤销）**本来就是装机项**，一直没验。

**装机步骤**（静默安装必须显式 `/D=`，并先清注册表旧键，见 §7.12）：

```powershell
# 1) 停服务
Start-Process -FilePath 'D:\Program Files\fluxing\weasel\fluxing\weasel\WeaselServer.exe' -ArgumentList '/quit' -PassThru
# 2) 清旧键
Remove-Item -Recurse -Force 'HKLM:\SOFTWARE\Fluxing\Weasel' -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force 'HKCU:\SOFTWARE\Fluxing'      -ErrorAction SilentlyContinue
# 3) 静默安装（D= 必须是最后一个参数、无引号）
& 'F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.6-installer.exe' /S /D='D:\Program Files\fluxing\weasel'
```

**装机后逐条勾（1.1.6 的 8 条 + 本轮新增 1 条）**：

| # | 验收项 | 备注 |
|---|---|---|
| 1 | 切到火流猩 → 快捷设置栏**显示** | spec 091 |
| 2 | 切到别的输入法 → 快捷设置栏**消失**，不残留 | spec 091 |
| 3 | 切走再切回 → 快捷设置栏**重新显示** | spec 091 |
| 4 | 切窗口 / 点其他应用 → 常驻开启时**不消失** | spec 091 |
| 5 | 语言栏弹出菜单中**无**「快捷面板常驻」项 | spec 091 |
| 6 | 托盘右键菜单**仍有**「快捷面板常驻」项，可取消 / 恢复 | spec 091；**① 实施后托盘项仍在，设置窗口里也会多一个开关** |
| 7 | 全新配置下快捷面板**默认常驻** | spec 089 / ADR 0022 |
| 8 | 右键屏蔽 → 名单落盘 → 托盘「恢复被忽略的候选项」撤销 | **一直没验，本轮装机必须取** |
| **9（新增）** | **`WeaselServer.exe /quit` 真能退出服务**（此前恒为「重启」） | **spec 093 的核心验收**；退出后进程消失，再次启动能起来 |

---

## 6. 下阶段计划（明确下一步）

### 6.1 ① 常驻开关收进「统一设置面板」 —— 形态已定，待实施

**形态决策（用户 2026-11-01 已批：方案 A）**：复用现有「快捷键设置」弹窗 `ShortcutSettings`，**改标题为「设置」**，新增「界面」分组放「快捷面板常驻」开关。

- 托盘「快速面板常驻」项**保留**（spec 091 验收第 6 条）⇒ ① 是「**也**收进」，不是搬家。
- 已否决：新建独立设置窗口（成本最高）、放进 `QuickPanelDialog` 面板本体（GDI v3-rev3 无子窗口，改成可点控件会触碰 spec 092 刚清理过的面板结构与 `TestQuickPanelDialog` 的 50 条断言）。
- **仓库里原本没有「统一设置面板」**，只有三个分散入口：托盘「输入法设定」→ 拉起 `WeaselDeployer.exe`；托盘「用户词典管理」→ `WeaselDeployer.exe /dict`；项目自建 `ShortcutSettings`（Ctrl+Shift+K / 面板快捷键按钮打开，**无托盘项**）。
- **详细实施方案已落盘**：`F:\soft\00selfmade\rime_claude\tmp\094-recon.md`（含精确落点行号、布局坐标公式、控件 ID 段、语义差异、测试策略、ADR/雷区）。⚠️ `tmp/` 被 gitignore，**若换机器/清 tmp 需重做这一步取证**。

关键落点速查：

| 项 | 位置 |
|---|---|
| 托盘菜单项 | `WeaselServer/WeaselServer.rc:164`（简）/ `:258`（繁）`ID_QUICKPANEL_ALWAYS_SHOW` |
| 托盘勾选态回调 | `WeaselServer/WeaselServerApp.cpp:27-30`（读 `QuickPanelDialog::AlwaysShowMode()` ⇒ **与设置面板天然同步，无需改代码**） |
| 托盘命令 / 切换 | `WeaselServer/WeaselServerApp.cpp:375-384` / `:399-405` |
| 状态载体 | `WeaselServer/QuickPanelDialog.h:48-50, 86-87`（`Mode{kHidden,kAlwaysShow}` / 静态 `s_mode`） |
| 落盘 | `WeaselServer/QuickPanelDialog.cpp:1466-1552`（`<WeaselUserDataPath()>\quickpanel.state`，单行 ASCII `always_show=0|1`；旧落点 `%APPDATA%\Rime\quickpanel.state` 只读兜底） |
| 写盘 | `QuickPanelDialog.cpp:1568-1580` `SetAlwaysShowMode` —— **内部已含写盘 + alpha 复位 + 面板重绘**；**不含** Show/Hide |
| 默认值 | `QuickPanelDialog.cpp:1548-1552` 文件缺失/损坏 ⇒ **true**（默认常驻） |
| 布局常量 | `WeaselServer/ShortcutSettings.cpp:50-63`（`kDialogW=800` `kDialogH=680` `kTitleH=56` `kToolbarH=44` `kStatusBarH=28` `kBtnH=34` `kGap=10`） |
| 表格高 / 底栏 Y 公式 | `ShortcutSettings.cpp:1004-1006` |
| 控件创建 | 搜索框 `:1029` / 状态行 `:1040` / 表格 `:1054-1058` / 底栏按钮 `:1097` |
| 标题栏绘制 | `:1157`（titleRc 21px bold）/ `:1173`（subRc 13px 副标题） |
| 窗口类名 | `:187` `FluxingShortcutSettings` —— **建议不改**（避免破坏既有 e2e 断言） |
| 现有测试 | `test/TestShortcutSettings/`（8 个纯逻辑用例，`s_pass`/`s_fail` 风格）；窗口 e2e 先例 `test/Verifier2_G2_G12/`（G10/G11）、`test/v0_19_0_32_e2e/`（P1c/P2c） |

实施要点：

1. 新增 `kPrefsH = 30`（「界面」分组行高），插在工具栏行之后、状态行之前；表格高公式减它、`kBtnY_phys` 加它。
2. 控件 `BUTTON` + `BS_AUTOCHECKBOX`，ID `ID_CHK_ALWAYS_SHOW = 2130`（2100-2120 已用，PhrasesDialog 用 1010-1205，错开）。
3. 初始态 `SendMessageW(hChk, BM_SETCHECK, QuickPanelDialog::AlwaysShowMode() ? BST_CHECKED : BST_UNCHECKED, 0)`；`OnCommand` 处理 `BN_CLICKED` ⇒ 调 `QuickPanelDialog::SetAlwaysShowMode(checked)`（**既有函数已含落盘**，不要自己写盘）。
4. **语义差异（写进交付报告）**：托盘切换附带「开 ⇒ 显示面板 / 关 ⇒ 收起面板」；设置窗口的勾选框**只改状态与持久化**，不主动 Show/Hide（避免打断用户操作）。
5. **下一轮第一件事**：确认 `test/TestShortcutSettings/TestShortcutSettings.vcxproj` 当前链接了哪些 `.cpp`。若链不到 `QuickPanelDialog.cpp`，就把「勾选态 ⇄ 状态文件」读写**下沉成共享头纯函数**（宪法 R7）⇒ 更稳。
6. **文档分流**：窗口职责从「快捷键」扩到「通用设置」属**架构决策** ⇒ 走 `docs/adr/0025-*.md`（MADR）+ `docs/adr/README.md` 补索引；功能扩展不是事故 ⇒ **不进 L161**（实施中若踩坑再记）。
7. **改动 ≥3 文件 ⇒ 必须建** `.specify/specs/094-settings-panel-always-show-toggle/{spec,plan,tasks}.md`，plan.md 含 Constitution Check（P1–P8 逐条对照）+ P8 三问。

### 6.2 ② FocusIn / FocusOut 对称位标志协议表 —— 待做

- 现状：`include/WeaselIPC.h` 里 `FOCUS_IN` / `FOCUS_OUT` **各 1 位**；spec 091 已让「切输入法」路径的 `FocusOut()` 带标志位，判定收敛为共享纯函数 `ShouldHideQuickPanelOnFocusOut(DWORD param, bool always_show)`（同文件）。
- ② = 补一组**对称的位标志协议表**（`FocusIn` 同构支持）⇒ **IPC 协议变更** ⇒ **必须** ADR + **server / TSF / deployer 三端同步** + `test/TestWeaselIPC/*` 一次性提交（CLAUDE.md §2 铁律）。
- 下一步取证：`include/WeaselIPC.h` 的位定义现状 + `WeaselTSF` / `WeaselServer` 两侧发送 / 消费点。

### 6.3 其它待办

- §6.1 验收第 8 条真机复验（装机时一并取，见 §5）。
- `D:\Program Files\fluxing\weasel\fluxing\weasel\` 内 0.21.0.0 嵌套残留 —— **只记录不清理**（用户已定）。

---

## 7. 环境与工具链陷阱（本节最值钱，逐条都是踩过的）

### 7.1 两个代码图谱 MCP 功能重叠 —— 本项目只认 `codebase-memory-mcp`

| | `codebase-memory-mcp` | `graphify` |
|---|---|---|
| 原理 | tree-sitter AST → 持久化知识图谱 | tree-sitter AST → **静态快照** `graphify-out/` |
| 工具数 | **14** | 7 |
| 增量更新 | `detect_changes` / `auto_watch=true` **跟随 HEAD** | **无**，只能重跑 graphify |
| 查询能力 | search_graph / trace_path / get_code_snippet / query_graph / get_architecture / search_code / manage_adr / … | query_graph / get_node / get_neighbors / get_community / god_nodes / graph_stats / shortest_path |
| 本机状态 | 原生二进制 v0.9.0（`C:\Users\Duanyi\.local\bin\codebase-memory-mcp.exe`），**已注册进 MiniMax Code，`enabled=true`** | 已注册但 **`enabled=false`** |
| 本项目数据 | `F-soft-00selfmade-rime-claude`，12,715 节点 / 38,206 边，`head_sha` 与 HEAD 一致 | `graphify-out/` 387 文件 / `graph.json` 7.3 MB，**生成于 2026-10-22，内容停在 spec 028 时代，已过期** |

**结论**：功能高度重叠，**只启用一个即可**；本项目以 `codebase-memory-mcp` 为准，`graphify` 保持 disabled。
⚠️ `graphify-out/` 由 `.gitignore:151` 隔离（未跟踪），不会污染提交。
⚠️ **项目名是连字符 `F-soft-00selfmade-rime-claude`**（CLAUDE.md 与 `.claude/rules/codebase-graph.md` 此前写的是下划线版，**已在本轮修正**）。
⚠️ `codebase-memory-mcp cli <tool>` **必须带 `--project <name>`**；raw JSON 传参在 0.9.0 已 deprecated，用 flags（`codebase-memory-mcp cli search_graph --project <name> --name-pattern '<regex>'`）。
⚠️ `npm install -g codebase-memory-mcp` **会失败**（要下 v0.12.1，本机装的是原生 0.9.0）；**不用管**，原生二进制已可用且已索引本项目。0.9.0 的工具名与 CLAUDE.md 规则（`search_graph` → `trace_path` → `get_code_snippet` → `query_graph` → `get_architecture`）**完全对齐**。
⚠️ `codebase-memory-mcp install` 的自动检测列表**不含 MiniMax Code** ⇒ 只能用 `mavis mcp create` 手动注册（已做）。
⚠️ `.codebase-memory/` 目录有 **3 个文件已被 git 跟踪**（既有状态，非本轮产物），别误当垃圾删掉。

### 7.2 渐进式披露（token 成本）

- MCP server 的工具 schema 在 **`enabled=true` 时常驻上下文**；`enabled=false` 时零成本。⇒ 不需要图谱时：`mavis mcp update codebase-memory-mcp --enabled false`。
- 更省的路子：完全绕开 MCP，走 CLI `codebase-memory-mcp cli <tool> --project <name> ...`（零常驻成本）。
- 注册时用 `--ui=false` 关掉 HTTP 图谱 UI（默认会起 9749 端口的常驻 HTTP server）。
- 图谱的价值是**替代**大范围 grep/read（README：5 个结构化查询 ≈ 3,400 tokens vs 逐文件搜索 ≈ 412,000 tokens）⇒ 用起来净 token 多半是**下降**。
- 通用铁律：**单轮工具调用 ≤8**（≥8 的 turn 占块消耗 87%）；互不依赖的读操作一次并行发起；长输出落文件、对话只留指针 + 结论。

### 7.3 版本真源是 `env.bat`，`weasel.props` 不是

`env.bat` 被 gitignore，内容：`FLUXING_VERSION=1.1.6` / `RELEASE_BUILD=1`。`weasel.props` 每次构建被 `build.bat` 的 `:render_props` **重渲染**，**不可手改**、**不可提交**。

### 7.4 打包必须 `build.bat weasel installer`

`build.bat` 参数：`weasel` / `installer` / `all` / `debug` / `release` / `rebuild`。产物落在 `output/archives/fluxing-<ver>-installer.exe`。**必须 `RELEASE_BUILD=1`**；**禁用 `git describe --tags` 取版本号**。

### 7.5 `.cmd` 必须 CRLF 行尾，否则 `/quit` 会变成「启动服务器」

写 `.cmd` 用 `-join "`r`n"`。注意这条在 spec 093 之前是**真事故**：`.cmd` 的 LF 行尾让 `WeaselServer.exe /quit` 参数带上多余字符 ⇒ 旧代码整串比较恒假 ⇒ **「退出」变成了「重启」**。spec 093 之后参数解析已加固，但 `.cmd` 仍要 CRLF。

### 7.6 服务会「复活」

`WeaselServer.exe` 被 kill 后可能被 TSF 前端自动拉起。**停服用** `Start-Process -FilePath '<abs>\WeaselServer.exe' -ArgumentList '/quit' -PassThru`（**不要 taskkill PPL 进程**），恢复用 `start_service.bat`。

### 7.7 套件「管道占用」事故与看门狗

**跑测试前必须停 WeaselServer**（`TestWeaselIPC` 要求独占命名管道，已装的服务占着它 ⇒ 误报失败），跑完用 `start_service.bat` 恢复。
⚠️ **编译失败的工程仍会用上一轮陈旧 exe 跑套件** ⇒ 套件全绿不等于新代码被编过，务必核对构建日志。

### 7.8 管道名不可绕过

`TestWeaselIPC` 的 squat 检查不能靠改管道名绕过。

### 7.9 文本编辑工具会重写混合行尾文件

大文件（`CHANGELOG.md` / `lessons-learned.md` / `CLAUDE.md` / `.rc`）必须**字节级 API** 改写（`ReadAllBytes` / `WriteAllBytes` + 编码透传），落盘后回读断言 **BOM / 行尾 / EOF 不漂移**。
⚠️ **`[Text.Encoding]::UTF8Encoding($true).GetBytes()` 不含 BOM**（`GetPreamble` 才有）⇒ 写有 BOM 的文件必须**手动 prepend `EF BB BF`**，否则 BOM 被吞（本轮真踩过）。
⚠️ `Path.write_text` 会把 LF 转成 CRLF，且可能写成 0 字节 ⇒ **写 bytes 或 `newline=""`**，写完断言 `size > 0`。
⚠️ `.rc` = UTF-16LE + BOM。
⚠️ `git show HEAD:<file>` 会规范化行尾，**不能做字节取证**；用 `git cat-file blob HEAD:<path>`，**路径必须正斜杠**（反斜杠会报 `exists on disk, but not in 'HEAD'` 并返回空字节）。
⚠️ 读 `git cat-file blob` 的 stdout 用 `cmd /c "... > 文件"` 落盘再读字节 —— `Process.StandardOutput.BaseStream.CopyTo()` 会因 StreamReader 预读缓冲**吞字节**。

### 7.10 bash 工具的硬安全策略

- **禁止 `Push-Location` / `Pop-Location` 与写入组合**（"write/delete targets cannot be verified safely after a command working directory change"）。
- `cmd /c "cd … && copy"` 被拦 ⇒ 载荷收录用 `cmd /c copy /Y "<abs>" "<abs>"`（**不带 cd**）。
- `Get-ChildItem 'C:\' -Filter … -Recurse` 被拦（驱动根递归）。
- 写文件用**绝对路径**；载荷收录用 `cmd /c copy /Y`。

### 7.11 MCP / 图谱的取证纪律

- 判定「插件 / MCP / 技能是否还在生效」**只能读真实请求的 tools 数组**；官方清单次之；名称缓存会过期骗人。
- 本机 `codebase-memory-mcp` 曾长期**不存在**（本轮才发现它其实**早就装好且已索引本项目**）⇒ **先查权威源（`cli list_projects` / `index_status`），再下「没装 / 没生效」的结论**。

### 7.12 静默安装的硬约定

- **必须 `/D=<path>` 显式**（`/D` 必须是最后一个参数、**无引号**），且先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing`。
- `output/install.nsi` **不可碰**（UTF-8 + `EF BB BF` + CRLF）。
- 改动 NSIS **先跑 makensis 当门禁**：`File /oname=` 才是改名语法；`SetOverwrite try` 静默跳过且不置错误位。

### 7.13 其他硬性约定

- 用户可见字符串保持**简体中文**（constitution P2/P5）。
- `ITfTextInputProcessor::*` 回调内**禁止** `CreateFile` / 同步注册表 / `BeginWaitForSingleObject`（P2）。
- `RimeLeversApi::{export,import}_user_dict` 必须被 `StartMaintenance` / `EndMaintenance` 包裹，否则 leveldb LOCK 失败。
- 残留 API `s_fullwidth` / `currentFullwidth` / `s_onFullwidth` 定性为**死参数**：不改生产代码、不删参数，只在测试里钉住并在交付报告披露。

---

## 8. 工作流约定（每次改动都走这套）

1. **改动 ≥3 文件 ⇒ 必须建** `.specify/specs/NNN-<kebab>/{spec,plan,tasks}.md` 三件套；plan.md 含 **Constitution Check（P1–P8 逐条对照）+ P8 三问**。
2. **决策分流**：
   - 架构决策（改 PPL？改 IPC？换构建？）⇒ `docs/adr/NNNN-<kebab>.md`（MADR，格式见 `docs/adr/README.md`）+ 在 `docs/adr/README.md` 补编号索引。
   - 事故 / bugfix ⇒ `.specify/memory/lessons-learned.md` 的 `L##` 节（**下一可用编号 L161**），引用 `CONTEXT.md §5`。
   - 不要把决策埋在 commit message / PR body —— 会被 commit history 覆盖。
3. **提交格式**：Conventional Commits + 模块前缀，模块前缀白名单 = `WeaselTSF | WeaselServer | WeaselDeployer | WeaselSetup | WeaselUI | WeaselIPC | RimeWithWeasel | installer | docs | fluxing | ci`。
4. **提交用多个小提交**（house style），每个提交前用 **allowlist** 校验暂存区（`git diff --cached --name-only` ⊆ allowlist，否则 `git reset -q` 并中止）。
5. **提交前必须** `git status -sb` + `git diff --stat` 复核，挡住 `weasel.props` / `env.bat` / `*.obj` / `librime` 误提交。
6. 提交信息写 `tmp/commit-XXX.txt` 再 `git commit -F`。
7. **IPC 协议变更 ⇒ server / TSF / deployer 三端 + 一个 ADR + 一个 `test/TestWeaselIPC/*` 测试，一次性提交**。
8. `clang-format.ps1 -n` **不可全仓跑**；`git clang-format` 需先 `git add`。
9. **决策优先自己定**：根因明确 + 代价可逆 + 有判别性测试兜底 ⇒ 直接按推荐方案做；只有**真正的产品方向决策**才发问卷（如 ① 的「统一设置面板」落在哪个窗口）。
10. **状态外置**：中间产物写文件，对话只留指针 + 结论；不把长输出 / 完整代码 / 大段日志回灌对话。

---

## 9. 文档索引

| 文档 | 路径 |
|---|---|
| 本交接文档 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-01-fluxing-1.1.6-spec093-delivery.md` |
| 上一份交接 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-10-31-fluxing-1.1.6-spec091-delivery.md` |
| 快捷键交接 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-v1.0.0-bare-shift-ascii-toggle-2026-10-28.md` |
| 进站指引 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` |
| 身份 / 不变量 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` |
| 操作手册 | `F:\soft\00selfmade\rime_claude\AGENTS.md` |
| 宪法 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` |
| 部署 + NSIS 配方 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` |
| 事故溯源（L001–L160） | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` |
| 领域契约 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` |
| ADR 索引 | `F:\soft\00selfmade\rime_claude\docs\adr\README.md` |
| 专项规则 | `F:\soft\00selfmade\rime_claude\.claude\rules\{codebase-graph,ipc-boundary,build-toolchain,commit-checklist,hotkey-binding,rime-settings}.md` |
| spec 092 | `F:\soft\00selfmade\rime_claude\.specify\specs\092-*\` |
| spec 093 | `F:\soft\00selfmade\rime_claude\.specify\specs\093-server-command-line-argument-parsing\` |
| ① 实施方案（临时） | `F:\soft\00selfmade\rime_claude\tmp\094-recon.md` —— **tmp 被 gitignore，会丢** |
| ③ 交付摘要（临时） | `F:\soft\00selfmade\rime_claude\tmp\093-delivery-and-next.md` —— 同上 |

---

## 10. 声明清单（交接必读）

- 本文档描述的是 **2026-11-01 00:20** 的实测状态。**1.1.6 未装机**（本机装的是 1.1.5），所以 1.1.6 的 8 条验收**一条都没勾**。
- 仓库里所有「已通过 / 全绿」的表述都指**本地构建与本地套件**，**不等于真机行为已验收**。
- `tmp/` 下的日志与实施方案**不在 git 里**；换机器或清理 tmp 后需重新取证。
- `librime/` 子模块本轮起就是脏的（` m librime`），**不要提交、不要 reset**。
- 装机双层嵌套残留（`D:\Program Files\fluxing\weasel\fluxing\weasel\` 内 0.21.0.0）**只记录不清理**。
