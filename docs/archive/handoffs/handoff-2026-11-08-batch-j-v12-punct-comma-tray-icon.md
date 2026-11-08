# 批次 J 交接 · 图标 v12 英文逗号 + 摘除常驻托盘「中英文」图标

- 日期：2026-11-08
- 会话：`mvs_84f1342f4533439d9e2bf3ed56d4192c`
- 分支：`Fluxing`（起点 HEAD `caa6e1f`，ahead 6；本批改动另起提交）
- 上游：[批次 I 交接](handoff-2026-11-08-batch-i-v11-punct-session-gate.md) · [批次 H2](handoff-2026-11-08-quickpanel-v10-punct-semantics.md) · [图标 v7/v8/v8.1 历史](handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md)
- 决策：[ADR 0041](../../adr/0041-fullwidth-equals-chinese-punct.md)（全角=中文标点）· [ADR 0042](../../adr/0042-session-gate-precedes-server-shutdown.md)（会话门禁）· [ADR 0043](../../adr/0043-punct-language-conversion-at-commit-boundary.md)（提交边界标点转换）· **[ADR 0044](../../adr/0044-language-bar-item-exits-persistent-tray.md)（本批新建：语言栏项退出常驻托盘）**
- 需求基线：`.specify/specs/099-quickpanel-sogou-benchmark/spec.md` · `tasks.md`「批次 J 追补」T-9J1…T-9J7

---

## 1. 本批目标（用户原话拆成的两条验收）

1. 快捷设置栏「中英文标点」图标：**中文标点辨识度已经很高，但英文的逗号辨识度仍需改进** ⇒ 图标迭代 v12。
2. **去除任务栏托盘区常驻的输入法「中英文」图标**（该图标不需要常驻）。

## 2. 进度总览

| 项 | 状态 |
|---|---|
| v12 图标几何 + 单测（T-9J1/J2） | ✅ 已落盘、已装机生效 |
| 摘除 `TF_LBI_STYLE_SHOWNINTRAY`（T-9J6） | ✅ 代码已落、已编进安装包；❌ **64 位装机副本未能替换** |
| 全量构建 + NSIS | ✅ |
| 静默重装 + 装机核对 | ✅ 服务端与 32 位 TSF dll 已更新 |
| L0 门禁 | ✅ `checks=14 pass=12 fail=0 skip=2` |
| 单测复跑（删诊断代码后） | ✅ 255/255 + 40/40 |
| 运行时像素取证（P4） | ✅ PASS，且相对批次 I 是**由 FAIL 转 PASS** |
| 托盘真机验收 | ❌ **未闭环，卡在 17 个宿主占用 x64 dll** |
| 文档 / CHANGELOG / ADR / 提交 | ✅ 本批已写 |

## 3. 结论（按可信度分级）

### 已验证（有产物、可复跑）

- **v12 几何**：`include/FluxingPanelFwIcon.h` 新增 `kCommaInkWidthEn = 8` / `kCommaInkHeightEn = 12` / `kCommaFaceEn = L"Times New Roman"`（中文态 8×13 不动）。英文态目标墨迹盒 `hw_target = {5,17,12,28}`（逻辑 8×12），期望 `fw_pen(-3,27)` / `hw_pen(3,20)`；单测 `T33k(v12)` 与 `T33q2` 同源断言，**删掉诊断代码后复跑 255/255 全绿**（`tmp/qpanel-tests-v12c.log`）。
- **构建**：`output\Win32\WeaselServer.exe` 2,336,768 B @14:44:30；`output\archives\fluxing-1.2.1-installer.exe` 42,825,241 B @14:46:43（`tmp/build-batch-j.log`，`ALL_DONE`）。
- **装机**：静默重装 `EXIT=0 / INSTALL_EXIT=0`，`WeaselServer.exe` / `weasel.dll` / `WeaselDeployer.exe` **MATCH=True** ⇒ v12 已在服务端与 32 位 TSF 生效（`tmp/reinstall-j.log`）。
- **★ 运行时像素（验收①的产物证据）**：带 `FLUXING_QP_DIAG_DUMP=1` 重启服务端后跑 `tmp/probe-batch-i3.ps1 -OutBase v12b`（`tmp/probe-v12b.log`）：
  - `P4 VERDICT punct_follows_state = PASS`（state0 `，`U+FF0C / state1、state2 `,`U+002C）—— **批次 I 同一步是 FAIL**（三态都回 U+FF0C），⇒ ADR 0043 的「提交边界标点语言转换」在装机版上确实生效。
  - slot2 墨迹：state0（中文标点）`ink=67 bbox size=23x26`；state1（英文标点）`ink=59 size=18x24`（物理像素 @scale 1.5023，即逻辑约 15×17 vs 12×16）⇒ 两态形状已分离，裁片 `tmp/v12b-P4-slot2-state0.png` / `-state1.png`。
- **★ 托盘现状（验收②的关键证据）**：抓屏任务栏右下角并原生裁片（`tmp/v12b-fullscreen.png` 3840×2160、`tmp/v12b-tray-ime.png` 160×70 @3450,2090）+ UIA 枚举明细（`tmp/v12-tray-j.txt`）：通知区域**并排两个**「任务栏输入指示」按钮 ——
  - `rect=3472` name=`任务栏输入指示 左键切换模式，右键打开菜单`，图标是**方框「中」** ⇒ **这是火流猩自己的语言栏项**（tooltip 文案逐字命中 `WeaselTSF/LanguageBar.cpp:150`）⇒ 它**仍在**，因为 64 位宿主仍加载旧 `weaselx64.dll`。
  - `rect=3520` name=`任务栏输入指示 简体中文(中国大陆) 火流猩输入法…`，图标是**红猿头 logo** ⇒ 这是 **Windows 自带的 IME 指示器**，显示当前输入法 logo，**不受本仓库代码控制**（要隐藏只能改 Windows 设置/策略）。
- **x64 未落地根因**：`tmp/x64-probe.txt` ⇒ 打开 `D:\Program Files\fluxing\weasel\weaselx64.dll` 报 `LOCK=HELD :: being used by another process`；枚举到 **17 个**进程把它作为已映射映像占用（`explorer` / `msedge` / `msedgewebview2`×3 / `Feishu` / `WeChatAppEx` / `WindowsTerminal` / `MiniMax Code` / `Claude Code Haha` / `alpha-file-manager` / `cherry-markdown-client` / `electron`×2 / `SearchHost` / `ShadowBot.VirtualDesktop` / `UGREEN NAS`）。
- **L0 门禁**：`tmp/gate-batch-j.log` ⇒ `L0 RESULT mode=source checks=14 pass=12 fail=0 skip=2`、`GATE_EXIT=0`。
- **T12d/T12e 结案为测试侧间歇性 flaky**：诊断版实测 `hit0=0 hit1=1 hit2=2 hit3=3 ascii_toggle=1 full_toggle=1 ascii_target=1 expect_target=1 state_ascii=0 state_full=0 cb_ascii=1 cb_full=1` ⇒ 命中区 / 回调注册 / 目标值全对，只是 `clicked()` 未落到 `OnAscii/OnFullShape`；**不改生产代码、不放宽断言**；诊断代码已删除不入仓。HEAD 形态复跑 40/40。

### 推断（合理但未证实）

- 重装并重启全部宿主后，方框「中」图标应消失（代码路径已确认：`dwStyle` 不再含 `TF_LBI_STYLE_SHOWNINTRAY`，项本身保留 ⇒ 左键开面板 / 右键菜单 / 状态文字零损失）。
- P6（点面板按钮后不点宿主直接打字）在批次 I 与本批**逐字符相同的 FAIL** ⇒ 判定为探针自身缺陷（读到的是别的控件 / 残留文本），不是本批引入的回归。

### 未验证

- **英文逗号「辨识度改进」本身** —— 人眼标准，Agent 不能自行判定通过。裁片已备（`tmp/v12b-P4-slot2-state0.png` / `-state1.png`、`tmp/v12-sheet.png`），**等用户目视终确认**。
- **验收② 真机效果** —— 见上，被 x64 占用卡住。
- tooltip 是否遮挡面板槽位（人眼项）。
- 「开机首次切换卡顿」复验（ADR 0042）需重启 Windows，Agent 不可执行。

## 4. 下一阶段执行序（新会话照此做）

1. **先请用户目视两件事**：① `tmp/v12b-P4-slot2-state0.png` vs `-state1.png` 的英文逗号是否够辨识；② 关掉占用宿主（或注销 / 重启 Windows）后重装 `output\archives\fluxing-1.2.1-installer.exe`，确认**方框「中」**图标消失、且红猿头（Windows 自带指示器）是否也要一并处理 —— 后者属 Windows 设置，不在本仓库。
2. 若 v12 仍不满意：只改 `include/FluxingPanelFwIcon.h` 的 `kCommaInk*En` / `kCommaFaceEn`，**同步** `test/TestQuickPanelDialog` 的 `T33k` / `T33q2`（`hw_target` 与两个 pen 期望值），重跑门禁 + 构建 + 重装。**禁止**在别处另抄公式（R7 单一事实源）。
3. x64 落地后复跑托盘取证：`pwsh -NoProfile -File tmp/probe-tray-j.ps1`（判据行 `VERDICT TRAY_PINNED_COUNT=`）+ `tmp/grab-tray-j.ps1` 抓屏裁片，两者互证。
4. 遗留人眼项：tooltip 遮挡、开机首切卡顿（需重启）。
5. 若本批两条验收均通过：把 spec 099 的批次 J 段落标记完成，并按 AGENTS.md §9 再出一份 handoff。

## 5. 本轮工具坑（务必别再踩）

- **`cmd /c "xxx.ps1"` 会把 `.ps1` 交给记事本**（文件关联），脚本根本没执行 ⇒ 服务端没带上 `FLUXING_QP_DIAG_DUMP=1`，探针报 `DUMP ... -> NO FRESH DUMP`，窜出的记事本窗口还污染了 P6。**跑 `.ps1` 一律 `pwsh -NoProfile -File`**；带 env 重启服务端要在同一次 pwsh 里 `$env:FLUXING_QP_DIAG_DUMP='1'` 再 `Start-Process`。
- **UIA 托盘枚举的 needle 不能用「英/中」**：我们的项在 UIA 里以 **tooltip 文案**出现（`左键切换模式，右键打开菜单`），glyph 名是空的 ⇒ 用 needle 判「0 命中」会给出**假阴性 PASS**。正确判据：按 tooltip 文案匹配，或直接数 `任务栏输入指示` 按钮个数（改前 2 个、改后应 1 个）。
- **抓屏必须 DPI-aware**：`SetProcessDpiAwarenessContext(-4)` 之后 `GetSystemMetrics(0/1)` 才返回物理 3840×2160，否则裁片坐标全错。
- pwsh 7 的 C# P/Invoke 声明 `extern IntPtr GetDpiForWindow(...)` 会让 `int dpi = GetDpiForWindow(h)` 报 `CS0266 nint→int` ⇒ 声明改成 `extern int`。
- **后台子会话的 task id 不跨压缩存活**（`task_output` 报 not found）⇒ 验收一律读子会话**落盘的日志文件**，并先确认它已到终态（铁律 30）。
- `CHANGELOG.md` 的 `edit` 锚点：`## [Unreleased]`（L1）与批次 I 那行标题**相邻无空行**，且全文另有一处 `## [Unreleased] - 2026-06-28`（L5682）⇒ 必须带上批次 I 标题才唯一。
- 权限闸：`Remove-Item` / `del` 会被拦（回收站策略）；`Set-Location` 之后再 `git` 会被拦 ⇒ 全程 `git -C $r`，产物写**新文件名**而不是先删后写。
- 本轮工具调用 79 次（预算 10）⇒ 根因是「探针假失败后逐条试探」。下一会话请**一次取齐**：跑取证前先把「带 env 重启服务端 + 探针 + 抓屏 + 汇总」串成一条命令。

## 6. 证据路径速查（全部在 `tmp/`，未入仓）

`build-batch-j.log` · `build-j-check.txt` · `reinstall-j.log` · `x64-probe.txt` · `gate-batch-j.log` · `qpanel-tests-v12c.log` · `qpanel-tests-diag.log` · `diag-tail.txt` · `probe-tray-j.ps1` · `v12-tray-j.txt`（UIA 明细）· `grab-tray-j.ps1` · `v12b-fullscreen.png` · `v12b-tray-ime.png` · `v12b-P4-slot2-state0/1/2.bmp|.png` · `probe-v12b.log` · `v12b-brief.txt` · `probe-contract.txt` · `yaml-brief.txt` · `t9-switches-check.txt`

## 7. 改动清单（本批）

| 文件 | 改动 |
|---|---|
| `include/FluxingPanelFwIcon.h` | v12 英文态逗号墨迹盒 + 字体常量 |
| `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | `T33k(v12)` / `T33q2` 同源断言 + 契约注释 |
| `WeaselTSF/LanguageBar.cpp` | `dwStyle` 摘除 `TF_LBI_STYLE_SHOWNINTRAY`（附「别用 `TF_LBI_STYLE_INVISIBLE`」警示注释） |
| `docs/adr/0044-language-bar-item-exits-persistent-tray.md` | 新建 |
| `.specify/specs/099-quickpanel-sogou-benchmark/tasks.md` | 追加「批次 J 追补」T-9J1…T-9J7 |
| `CHANGELOG.md` | `## [Unreleased]` 下新增批次 J 条目 |
| `docs/archive/handoffs/handoff-2026-11-08-batch-j-v12-punct-comma-tray-icon.md` | 本文件 |
| `docs/archive/prompts/prompt-next-session-2026-11-08-batch-j-v12-punct-comma-tray-icon.md` | 接力 prompt |
| `CLAUDE.md` | §2「本批交接指针」换成本批，批次 I 降为「上一批」 |
