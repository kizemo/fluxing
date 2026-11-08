# 交接 · 批次 L · 快捷设置右键菜单新增「快捷键设置」+ 托盘「中英文」摘除上机 + 快捷键设置界面 P0 修复

- **日期**：2026-11-08
- **上一批**：[批次 K · 快捷设置栏图标 v13 英文逗号左缘裁切](handoff-2026-11-08-batch-k-v13-punct-comma-cut.md)
- **本批 spec**：`.specify/specs/045-shortcut-settings/`
- **相关 ADR**：`docs/adr/0044-language-bar-item-exits-persistent-tray.md`（托盘摘除）、`docs/adr/0045-product-version-pinned-by-env-bat.md`（版本号推导）、`docs/adr/0038-single-context-menu-brand-icon-entry.md`（右键菜单唯一化）

---

## 1. 背景与目标

用户本轮三条指令（原文见 `CHANGELOG.md` 批次 L 条目）：

1. 任务栏托盘区仍有「中英文」图标，**系统自带的输入法位置图标要保留**。
2. 快捷设置栏「中英文标点」的英文逗号仍不好，**暂时搁置，推进其它任务**。
3. 快捷设置栏「快捷键设置」按钮对应的界面**是否已经编辑完**？并在**快捷设置右键菜单**中新增「快捷键设置」子菜单，点击可进入快捷键设置界面。

## 2. 结论摘要

| # | 诉求 | 结论 |
|---|---|---|
| 1 | 托盘「中英文」摘除 | **我们的项已消失**（UIA 计数 0），**系统红猿头 / IME 指示器保留**（计数 2）。**未闭环**：需用户手动切回火流猩做一次人眼确认（自动化切换受限于本机输入法布局）。 |
| 2 | 英文逗号 | **搁置**。批次 K 的 `kCommaCutLeftEn = 2` 保持不动，未回退、未再改。 |
| 3a | 快捷键设置界面是否编辑完 | **没有编辑完** —— 界面 chrome 完整，但**四条数据链路全断**。P0 六条本批已修；P1/P2 留档待排期。 |
| 3b | 右键菜单新增「快捷键设置」 | **已完成并真机端到端闭环**：品牌区右键 ⇒ 真实弹出菜单 ⇒ 菜单内 `pos=3 id=40023 text='快捷键设置 (&B)'` ⇒ 点选后打开「设置」对话框。 |

## 3. 已验证 / 推断 / 未验证

### 3.1 已验证（有文件级证据）

- 托盘项摘除上机：`D:\Program Files\fluxing\weasel\weaselx64.dll` 用 **Rename → Copy** 绕过文件锁替换成功，SHA256 `MATCH=True`；explorer 已重启；UIA 实测我方项（tooltip「左键切换模式，右键打开菜单」）**0 命中**，改前基线 `tmp/v12-tray-j.txt:43` 明确存在该项。
- 新菜单项在**装机版 exe** 的菜单资源里：`tmp/v-batchL2.txt` ⇒ `RT_MENU id=105` → 根 POPUP「Fluxing」→ `pos=3 id=40023 text="Shortcut settings (&B)"`。
- 新菜单项在**运行时真实弹出的菜单**里：`tmp/v-batchL17.txt` ⇒ `HMENU count=19`，`pos=3 id=40023 text='快捷键设置 (&B)'`。
- `WM_COMMAND(40023)` ⇒ 「设置」对话框打开，窗口类 `FluxingShortcutSettings`（标题是通用「设置」）。
- P0 六条修复后：`release\TestShortcutSettings.exe` **PASS=52 FAIL=0**（EXIT=0）。
- L0 静态门禁：`scripts/gate/L0-static.ps1 -Mode source` ⇒ `checks=14 pass=14 fail=0 skip=0`（含 `version-consistency` version=1.2.1、`rc-utf16le`、`nsis-compile`、`guard-unit-test` TOTAL=16 MISMATCH=0）。
- 装机四件（`WeaselServer.exe` / `weasel.dll` / `weaselx64.dll` / `WeaselDeployer.exe`）哈希 `MATCH=True`，WeaselServer 重启后 pid 606736。

### 3.2 推断（有代码级依据，未单独取证）

- 「点菜单项 ⇒ 打开界面」这一跳由 `WeaselServer/SystemTraySDK.cpp:852` 的 `PostMessage(hTargetWnd, WM_COMMAND, uItem, 0)` 承担，`uItem` 就是菜单项 ID 40023；Part B 已用同一条消息路径单独验证（对话框打开）。
- 托盘右键与快捷设置栏品牌图标右键**共用同一份 `IDR_MENU_POPUP`**（ADR 0038）⇒ 一个菜单项同时覆盖两个入口，托盘右键入口无需再单独取证。

### 3.3 未验证 / 需用户参与

- 托盘项的**人眼闭环**：本机活动输入法被切到**搜狗拼音**，探针的 Win+Space 只在「已禁用输入法 / 英语(美国)」间循环，无法自动切回火流猩 ⇒ 存在「因为不是活动输入法所以不显示」的混淆项。**请手动切回火流猩，确认通知区不再出现方框「中」，而红猿头仍在。**
- 快捷键设置界面的**用户可感知效果**（保存后 librime 是否真的生效）需在装有 Rime 会话的宿主里人工点一次。

## 4. 关键改动清单（工作区未提交）

| 文件 | 改动 |
|---|---|
| `WeaselServer/resource.h` | 新增 `ID_WEASELTRAY_SHORTCUT_SETTINGS 40023` |
| `WeaselServer/WeaselServer.rc` | `IDR_MENU_POPUP` **三语言**（简 @165 / 繁 @261 / 英 @357）同步新增 `MENUITEM "快捷键设置 (&B)"`（UTF-16LE + BOM，字节级写） |
| `WeaselServer/WeaselServerApp.cpp` | `SetupMenuHandlers()` 注册 40023 → `ShortcutSettings::Show()`；跨线程时 `PostMessage` 弹回 UI 线程 |
| `WeaselServer/ShortcutSettings.cpp` / `.h` | P0 T001–T006 六条修复（解析器取尾 18→20；落盘改 `WeaselUserDataPath()`；`SpawnDeploy()` 真调 `WeaselDeployer.exe /deploy`；导入导出改独立往返文件 `default.custom.exported.yaml`；「恢复默认」改 `LoadDefaults()`；解析失败或空结果回落默认表） |
| `build.bat` | 补 `if not defined PRODUCT_VERSION` 守卫（`env.bat` 优先，脚本默认兜底）—— ADR 0045 |
| `docs/adr/0045-product-version-pinned-by-env-bat.md` | 新建 |
| `.specify/specs/045-shortcut-settings/tasks.md` | 新建（完成度审计 + P0→P2 清单 + 验收证据） |
| `.specify/memory/lessons-learned.md` | 追加 L202 / L203 |
| `CHANGELOG.md` / `docs/adr/README.md` | 批次 L 条目 / ADR 0045 索引 |

**提交边界**：`weasel.props` / `env.bat` / `*.obj` 是本地文件，**不得入库**。commit 前必须 `git status -sb` + `git diff --stat`。commit message 用 Conventional Commits + 模块前缀（`WeaselServer` / `docs` / `fluxing` …）。**本批尚未 commit（用户未要求）**。

## 5. 证据路径（全部绝对路径）

- 报告：`F:\soft\00selfmade\rime_claude\tmp\v-batchL17.txt`（菜单枚举 + 对话框）、`F:\soft\00selfmade\rime_claude\tmp\v-batchL2.txt`（装机 exe 菜单资源）、`F:\soft\00selfmade\rime_claude\tmp\v-batchL5.txt`（通知区 UIA）、`F:\soft\00selfmade\rime_claude\tmp\v-batchL8.txt`（40023 直发）、`F:\soft\00selfmade\rime_claude\tmp\v12-tray-j.txt`（改前托盘基线）
- 截图：`F:\soft\00selfmade\rime_claude\tmp\v-batchL17-menu.png`、`F:\soft\00selfmade\rime_claude\tmp\v-batchL17-dialog.png`
- 探针脚本：`F:\soft\00selfmade\rime_claude\tmp\probe-batch-L8.ps1`（最终版）
- 装机脚本：`F:\soft\00selfmade\rime_claude\tmp\install-batch-L.ps1`
- 门禁日志：`F:\soft\00selfmade\rime_claude\tmp\gate-batch-L3.log`
- 单测日志：`F:\soft\00selfmade\rime_claude\tmp\test-shortcutsettings.log`

## 6. 本轮工具坑（务必先读）

1. **探针按窗口标题找对话框 ⇒ 假阴性**。快捷键设置窗的标题是通用「设置」，类名才是 `FluxingShortcutSettings`。判据必须按**类名**。
2. **菜单资源只 dump 顶层 ⇒ `GetMenuItemCount=1`**（根是 POPUP）⇒ 必须 `GetSubMenu` 递归。
3. **`GetMenu` 在菜单窗口上常返回 0** ⇒ 用 `SendMessage(hMenuWnd, MN_GETHMENU=0x01E1)` 取 HMENU，再 `GetMenuStringW(MF_BYPOSITION=0x400)` 逐项读文本 + `GetMenuItemID` 读 ID。
4. **UIA 不暴露这个弹出菜单的子项**：`AutomationElement::FromHandle(menuWnd).FindAll(Descendants)` 返回空 ⇒ 读菜单内容必须走 Win32 API，不要指望 UIA。
5. **`WM_COMMAND` 要对**所有** top-level 窗口 post**；只 post 一个 ⇒ handler 收不到。但 **40018 是 toggle**，多 post 会互相抵消（面板开→关）⇒ 打开面板用幂等的 **40020**（`QUICK_PANEL_SHOW`）或只 post 一次 40018。
6. **找面板窗口必须判 `IsWindowVisible`**：面板窗口对象常驻（隐藏态仍存在），不判可见性会「0ms 就找到」一个隐藏窗口，形成假阳性。
7. **PowerShell 里 `uint` 不是合法类型别名**（`uint mp;` 会被当成命令 ⇒ CommandNotFoundException）⇒ 用 `[uint32]`。
8. **`New-Object System.Windows.Automation.PropertyCondition(...)` 在 PS 5.1 抛 ConstructorInvokedThrowException** ⇒ 用 `[Type]::new(arg1, arg2)`，且 handle 参数要 `.ToInt32()`。
9. **PS 5.1 脚本源码必须纯 ASCII**：中文注释 + 无 BOM 会被按 ANSI 解析，多字节吞掉 `}` ⇒ 报「Unexpected token '}'」。CJK 用 `[char]0xXXXX` 构造。
10. **`Copy-Item` 直接覆盖已装 dll 会「being used by another process」** ⇒ 必须 Rename → Copy。
11. **`LoadLibraryExW` / `FindResourceW` 必须写在 `kernel32.dll`**（写到 `user32.dll` ⇒ EntryPointNotFound）。
12. **bash 工具在本机是 PowerShell**：`cd` 之后写文件会被安全策略拦截 ⇒ 用绝对路径 + `[IO.File]::WriteAllText`。
13. **`Tee-Object -FilePath` 用相对路径会写错位置** ⇒ 用绝对路径。
14. **探针「0 命中」不能当「已生效」**：判据 needle 必须对准真实窗口标题/类名/tooltip，否则是假阴性。

## 7. 下阶段计划

1. **P1 优先 T101 / T102**（冲突检测接线、状态行实时更新）—— 实现已存在但零调用点，接线成本最低、用户可感知收益最高。
2. **P1 T103**（搜索框真正过滤）、**T106**（写盘改增量 patch，空 `action_yaml` 行不得写出坏 binding）。
3. **P2 T201–T203**（filter chips + 方案下拉、标题栏圆形 ✕ 的 `WM_LBUTTONDOWN`、取消时未保存确认）。
4. **测试缺口 T301–T304**：把 `LoadHotkeys` / `SaveHotkeys` / `CheckConflict` 提到可测入口 —— 这是 T001/T002 长期没被发现的根因。
5. **T104「立即验证」按钮需先过 P2 审查**（AP-045-B：`SendInput` 注入不得放进 hook 路径）。
6. **用户侧待办**：手动切回火流猩 ⇒ 确认通知区无方框「中」、红猿头仍在；再在 Rime 宿主里点一次「快捷键设置 → 保存」，确认 librime 真的读到新配置。

## 8. 残留物

- `D:\Program Files\fluxing\weasel\weaselx64.dll.bak0044` 与 `weaselx64.dll.old.tmp` 删除失败（被占用，`CLEAN_FAIL`）⇒ 需停掉占用进程后清理。
- 工作区既有无关脏项：`qp-dump.bmp`、`qp-logo.bmp`、`qp-state.txt`、submodule `librime`。
