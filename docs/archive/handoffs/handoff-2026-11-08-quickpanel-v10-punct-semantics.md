# Handoff 2026-11-08（批次 H2）：快捷设置栏图标 v10「字符逗号」+ 全角语义纠正为「中文标点」+ 点按钮不失焦

> 直接上游（批次 H · 图标 v9 + 半角=英文标点 + 中英联动标点）见
> `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v9-fwicon-batch-h.md`
> ——其 §七 的 D2–D8 / D10 **仍然有效**，只有 D1、D9 被本批 ADR 0041 部分作废。本文件只覆盖批次 H2。
> 更早的图标历史（v7 对角布局 / v8 句号收小 / v8.1 描边）见
> `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md`（§十 为终态，**别去推翻**）。

## 一、当前状态（一段话）

用户**五条**新需求 + **一条语义纠正**已全部落地代码并通过自动化验收：①「中/英文标点」图标里的逗号由多边形改为**真字形渲染**
（GDI `CreateFontIndirectW` + `ANTIALIASED_QUALITY` + `GetGlyphOutlineW` 取墨迹盒 + `TextOutW` 落笔），消除锯齿与「圆头与尾巴连接不圆滑」；
②裸 Shift 切中英文后「中/英」标志不同步已修（`kToggleAsciiMode` 补 `m_global_ascii_mode` 同步）；
③「手点面板切半角 ⇒ 中文态打逗号变英文逗号」路径复核确认**语义正确**，并由 ADR 0041 固化为「面板全/半角 = 中文标点/英文标点，只驱动 `ascii_punct`」；
④**全角语义按用户纠正重定义：全角只等于「中文标点」，数字与其他符号始终半角**（对齐搜狗），配置侧 `_ApplyPunctWidth` 在**两个分支**都清
`full_shape`、`default.yaml` 的 `save_options` 移除 `full_shape`；⑤点中英文 / 中英文标点按钮后**不再抢焦点**（`WM_LBUTTONDOWN` 里
`hit >= 0` 时不调 `CaptureKeyboardFocus`），点完可直接在宿主窗口继续输入。
自动化全绿：单测 **254/254（Dialog）+ 40/40（Refactor）**、L0 静态门禁 `checks=14 pass=12 fail=0 skip=2`、全量构建 `ALL_STEPS_OK` /
`NSIS_RC=0`、静默重装 `INSTALL_EXIT=0` 且四个二进制 **MATCH=True**、装机 `default.yaml`（18448 B，与仓库一致）的 `save_options` 已无 `full_shape`。
**唯一未闭环的是人眼 + 手点真机验收**（本机当时处**锁屏态** `LogonUI=1`，UI 自动化与抓屏全部不可用）——见 §八。

## 二、必读文档（全绝对路径，按此顺序）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\AGENTS.md`（§1 / §4 / §5 / §9）
3. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§A / §11）
6. 本文件 `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-08-quickpanel-v10-punct-semantics.md`
7. 本批架构决策 `F:\soft\00selfmade\rime_claude\docs\adr\0041-punct-width-drives-ascii-punct-only.md`
   （**部分作废** `F:\soft\00selfmade\rime_claude\docs\adr\0040-panel-fullwidth-state-source-and-shift-punct-linkage.md`）
8. 需求基线 `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` 与
   `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md`（P0 段含 v10 / 批次 H2 追补项）
9. 若要改 `output/data/**/*.yaml`：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（§5–§7、§9、§10）
10. 批次 H：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v9-fwicon-batch-h.md`；
    批次 G：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md`

## 三、本批进度（逐项，含验证等级）

| # | 事项 | 状态 | 验证等级 |
|---|---|---|---|
| 1 | 图标逗号改**字符逗号**（真字形渲染，v10 几何） | 完成 | 单测 T33q2/T33q3 字形往返 + 墨迹盒 7×13（**锯齿/手感待用户人眼**） |
| 2 | 裸 Shift 切中英文后「中/英」标志不同步 | 完成 | 单测 + 代码审阅（**待真机手按**） |
| 3 | 「手点面板切半角 ⇒ 中文态逗号变英文逗号」语义复核 | 完成（语义正确，ADR 0041 固化） | **待真机手点隔离实验** |
| 4 | 全角语义纠正：全角 = 中文标点；数字/其他符号恒半角 | 完成 | 装机 `default.yaml` 实测 + 单测 |
| 5 | 点面板中英文 / 中英文标点按钮后焦点丢失 | 完成 | 代码审阅（**待真机手点**） |
| 6 | 单测 / L0 门禁 / 全量构建 / NSIS / 静默重装 | 完成 | 全部自动化通过 |
| 7 | 文档（ADR 0041 / tasks.md / 本 handoff / 接力 prompt） | 完成 | 已写入并核对编码行尾 |

## 四、改动文件清单（提交前对照）

**代码（5）+ 配置（1）+ 测试（1）**：

- `include\FluxingPanelFwIcon.h` —— 整文件重写为 **v10**：删 `kSolidPolygon` / `kMaxPolyPoints` / `pts[]`；
  `enum ShapeKind { kHollowEllipse = 0, kSolidRoundRect = 1 }`；`kStrokeWidth=1`、`kCommaFontHeight=48`、`kCommaInkWidth=7`、
  `kCommaInkHeight=13`、`kCommaTop=16`；`Shape{kind,rect}` / `Icon{shapes[2], shapeCount, commaInk, commaChar, ink, hollow}` /
  `GlyphInk{width,height,originX,originY}`；新增**纯函数** `ShapeInkBox` / `ScaleFontHeight` / `GlyphPenPointForInk` / `Build(bool)`
  （生产与单测同源）。全角 = 空心环 `RECT{16,2,28,14}` + `L'\xFF0C'` + `commaInk{3,16,9,28}`；半角 = 实心圆角点 `RECT{17,4,24,11}` +
  `L','` + `commaInk{5,16,11,28}`；`shapeCount` 恒 1；末尾奇偶兜底 `if ((ink_w & 1) == 0) ++ink.right;`。
- `WeaselServer\QuickPanelDialog.cpp` —— `DrawIconFullwidth` 收敛为两分支（全角环 / 半角点）+ 调 `DrawInkPlacedChar`；新增
  `CreateCommaFont(int px)`（`lfHeight=-px`、`FW_NORMAL`、`DEFAULT_CHARSET`、`ANTIALIASED_QUALITY`、`Microsoft YaHei`）、
  `MeasureCommaGlyph`（`FillUnitMat2` + `GGO_METRICS`）、`DrawInkPlacedChar`（seed 48 → 量 → `ScaleFontHeight` → `GlyphPenPointForInk`
  → `SetBkMode(TRANSPARENT)` + `TA_LEFT|TA_BASELINE` → `TextOutW` → 状态还原）；`WM_LBUTTONDOWN` 的
  `CaptureKeyboardFocus(hwnd)` 改为 `if (hit < 0) CaptureKeyboardFocus(hwnd);`。
- `RimeWithWeasel\RimeWithWeasel.cpp` —— `_ApplyPunctWidth`（约 2190–2218 行）**只写** `ascii_punct`；`full_shape = 0` 在
  **两个分支都清**（原来只在 `!halfwidth` 内清）；`kToggleAsciiMode` 分支补 `m_global_ascii_mode = !ascii;`。
- `include\FluxingPanelStatus.h` —— `PanelState.fullwidth` 注释改为「= 中文标点模式，**不是** librime 的 `full_shape`」；
  `ShapeModeText` → **`PunctModeText(bool chinesePunct)`**，返回 `L"中文标点"` / `L"英文标点"`；`ShapeIsFullwidth` **保留函数名**，文档块重写。
- `include\FluxingPanelTooltip.h` —— `SlotName(5)` = `L"中英文标点切换"`；`SlotStateText` 的 `case 5` 改调
  `PunctModeText(st.fullwidth)`；`BuildTooltip` = 「名称 · 当前:xxx」。
- `output\data\default.yaml` —— `switcher/save_options` 移除 `full_shape` 及其 3 行中文注释；字节级验证
  `BOM=False`、`LEN=18448`、`CRLF=440`、`LF=0`。
- `test\TestQuickPanelDialog\TestQuickPanelDialog.cpp` —— T26g/T26h/T26j、T33 整块（T33k/T33l/T33q 的多边形断言改写为
  `commaInk` 7×13 + `commaChar` 码点，新增 T33q2/T33q3 字形往返）、T34a–T34d、T28e2 / T28i3 / T28k2 / T28o3 四条 tooltip 断言全部改新口径。

**文档（3）**：

- `docs\adr\0041-punct-width-drives-ascii-punct-only.md`（**新增**，Status: Accepted）—— 5 条 Decision（只写 `ascii_punct` +
  两分支清 `full_shape`；`full_shape` 永久关闭但保留 9 个 schema 的 switch 声明；用户可见改名 `PunctModeText`；内部标识符
  `ShapeIsFullwidth` / `fullwidth` / `onFullwidth` 保留旧名；`save_options` 移除 `full_shape`）+ Consequences（数字/空格始终半角、
  名称与语义不再字面一致、第三方置 1 会被刻意清掉、可逆）。
- `.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` —— 在「## P1 · 声明式槽位表」前插入「追补 **v10 / 批次 H2**」条目（11 个子项）。
- 本文件 + `docs\archive\prompts\prompt-next-session-2026-11-08-quickpanel-v10-punct-semantics.md`。

**提交时必须排除**：`librime`（` m librime` 是子模块假脏）、`weasel.props`、`env.bat`、`*.obj`、`*.log`、`tmp\`。
**本批尚未提交任何内容**（`git status -sb` 显示 `## Fluxing...kizemo/Fluxing [ahead 2]`，8 改 1 新增）。

## 五、下一阶段执行序（按此顺序，理由随附）

1. **用户人眼 + 手点验收四件事**（本批唯一未闭环项，见 §八）。本机当时锁屏，不能自动化。
2. 若「图标逗号」不过（仍有锯齿 / 连接不圆滑 / 墨迹偏小）：**只改 `include\FluxingPanelFwIcon.h` 的
   `kCommaFontHeight` / `kCommaInkWidth` / `kCommaInkHeight` / `kCommaTop` + 对应 T33 断言**，再同步 `WeaselServer\QuickPanelDialog.cpp`
   的 seed 高度 → 重跑单测 → 重打安装包 → 静默重装 → 复验。**不要退回多边形**，也不要动状态源与联动逻辑。
3. 若「点按钮后仍失焦」：先确认 `WM_LBUTTONDOWN` 里 `hit` 是否真的 `>= 0`（按钮命中矩形可能没覆盖到点到的位置），
   再查 `WM_MOUSEACTIVATE` 返回值与 `SetFocus` 来源，最后才怀疑宿主 TSF 侧。
4. 若「裸 Shift 后中/英标志仍不同步」：查 `m_global_ascii_mode` 是否在 `kToggleAsciiMode` 之外还有别的写入点（会覆盖），
   再看 `GetPanelInputState` 的 `ascii_mode` 取自 live status 还是镜像。
5. 若「中文态打数字/空格仍是全角」：先查**当前方案**是否还写 `full_shape: reset: 1`（`_ApplyPunctWidth` 会清，但部署/重载可能写回），
   再查是否被 `punctuator/full_shape` 段或第三方 `*.custom.yaml` 重新打开。
6. 收尾：`scripts\gate\run-all.ps1 -Level L0`；再进 spec 099 的 **P1 · 声明式槽位表**。
7. 并行线：spec 100 云端账号与多端同步（见
   `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-fluxing-cloud-sync-plan.md`），**与本批互不依赖**。

## 六、证据路径（全部绝对路径）

脚本（在 `tmp\`，已被 `.gitignore` 排除）：

- `F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd` —— 单测构建（三个测试项目只构建 `Release|Win32`）。
- `F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd` —— 全量构建 + NSIS。
- `F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.cmd` —— **只重打安装包**（二进制没变、只改了 yaml 时用它，比全量构建快）。
- `F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1` —— 静默重装 + 四二进制 SHA256 核对（本批已跑通）。
- `F:\soft\00selfmade\rime_claude\tmp\probe-fwicon-v9.ps1` —— 图标像素探针（v10 下 T33 判据已由单测覆盖，探针可复用但**锁屏不可用**）。
- `F:\soft\00selfmade\rime_claude\tmp\probe-ctrl-punct.ps1` —— 真机行为探针（`Ctrl+.` / `Shift+space` / 裸 Shift）。

日志：`tmp\qpanel-tests-v10b.log`（254/254）、`tmp\gate-v10.log`、`tmp\build-batch-h2.log`、`tmp\rebuild-installer.log`、
`tmp\reinstall3-batch-e.log`（`INSTALL_EXIT=0` + 四个 `MATCH=True`）。

机器侧（真机）：

- `D:\Program Files\fluxing\weasel\WeaselServer.exe` —— 2332160 B / `00:24:50` / 与 `output\Win32\` 同 SHA256。
- `D:\Program Files\fluxing\weasel\data\default.yaml` —— 18448 B；`save_options` = `ascii_punct / traditionalization / emoji / search_single_char`
  （**已无 `full_shape`**；文件内仍保留 7 处 `full_shape` 字样，那是 `punctuator/full_shape` 段与注释，属预期）。
- 活动 Rime 用户目录：`D:\Program Files\fluxing\user1\fluxing\`（**不是** `%APPDATA%\Fluxing`）。

可复用命令：

```powershell
# 单测（改 UI / 文案后必跑）⇒ 期望 TEST_BUILD_EXIT=0 且 TEST_DIALOG_EXIT=0
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd" > "F:\soft\00selfmade\rime_claude\tmp\qpanel-tests-v10b.log" 2>&1
Select-String -Path 'F:\soft\00selfmade\rime_claude\tmp\qpanel-tests-v10b.log' -Pattern 'FAIL|TEST_.*EXIT|assertions passed'
# 注意：Dialog 汇总行是 "=== N / N assertions passed ==="，不含 "Dialog" 字样

# L0 门禁 ⇒ checks=14 pass=12 fail=0 skip=2
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1" -Mode source -SkipNsisCompile -SkipGuardUnitTest

# 全量构建 + NSIS（>100s，必须后台跑）⇒ ALL_STEPS_OK / NSIS_RC=0
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd" > "F:\soft\00selfmade\rime_claude\tmp\build-batch-h2.log" 2>&1

# 只重打安装包（只改 yaml 时）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.cmd" > "F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.log" 2>&1

# 静默重装 + 哈希核对 ⇒ INSTALL_EXIT=0；四个二进制 MATCH=True
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1"

# 锁屏判定（任何 UI 自动化 / 抓屏前必做）⇒ 有 LogonUI 即 LOCKED，不可自动化
if (Get-Process LogonUI -ErrorAction SilentlyContinue) { 'LOCKED' } else { 'UNLOCKED' }

# yaml 行尾审计
$p='F:\soft\00selfmade\rime_claude\output\data\default.yaml'
$b=[System.IO.File]::ReadAllBytes($p); $crlf=0; $lf=0
for($i=0;$i -lt $b.Length;$i++){ if($b[$i] -eq 10){ if($i -gt 0 -and $b[$i-1] -eq 13){$crlf++}else{$lf++} } }
"LEN=$($b.Length) CRLF=$crlf LF=$lf"

# git（禁止先 cd；禁止 git add .）
git -C F:/soft/00selfmade/rime_claude status -sb
git -C F:/soft/00selfmade/rime_claude diff --stat
```

## 七、本批决策（改之前先读这里，别推翻重来）

- **D1（ADR 0041，部分作废 ADR 0040 D1）**：面板「全/半角」=「中文标点 / 英文标点」，**只驱动 `ascii_punct`**；
  `full_shape` 不再由面板驱动，`_ApplyPunctWidth` 在**两个分支**都把 `full_shape` 清 0。显示口径仍是
  `fullwidth = !ascii_mode && !ascii_punct`（`fluxing::panel_status::ShapeIsFullwidth`）。
- **D2（语义纠正）**：用户口径 —— **全角只等于中文标点**；数字、空格与其他符号**始终半角**（对齐搜狗）。
  `punctuator/full_shape` 段会把 `' '` 映射成 `'　'`（全角空格），与 D2 直接冲突 ⇒ 必须保持 `full_shape` 关闭。
- **D3（图标技术路线）**：逗号走 **GDI 真字形渲染**（`CreateFontIndirectW` + `ANTIALIASED_QUALITY` + `GetGlyphOutlineW` 墨迹盒 +
  `TextOutW`）；**不用 GDI+ `FillPath`**（记忆 #34 的半透明发暗 / 黑框风险）。环与点继续用 `Ellipse` / `RoundRect`（记忆 #37：
  小尺寸实心椭圆会光栅化成「四角星」）。
- **D4（落笔对齐）**：用**中心对齐**而非左上角对齐（把 ±1 px 误差对称摊开），保留**一次** `ScaleFontHeight` 自校正抗字体替换；
  末尾奇偶兜底保证墨迹盒**奇×奇**（避免补边偏心）。
- **D5（标识符）**：`ShapeIsFullwidth` / `PanelState.fullwidth` / `onFullwidth` **保留旧名**（重命名会波及
  `RimeWithWeasel.cpp` / `WeaselServerApp.cpp` / 测试），只补注释；**用户可见文案**统一走 `PunctModeText`。
- **D6**：9 个 schema（`rime_ice` / `double_pinyin*`）的 `full_shape` switch **声明不删** —— 声明本身不打开它（默认 index 0），
  删了无行为收益、diff 还摊到 9 个文件。
- **D7**：`default.yaml` 的 `save_options` **必须**移除 `full_shape` —— 否则下次部署会把用户当前的 `full_shape` 值写回。
- **D8（焦点）**：`WM_LBUTTONDOWN` 里只有**没命中按钮**（`hit < 0`）时才 `CaptureKeyboardFocus`；命中按钮则不再抢焦点。
- **D9（Shift 同步）**：`kToggleAsciiMode` 分支补 `m_global_ascii_mode = !ascii;`，让「中/英」标志与全/半角图标同源同步。
- **D10（范围）**：本批**不改** IPC 协议（`include\WeaselIPCData.h` 未动 ⇒ 无需四边同步）、不改 `output\install.nsi`、
  不改 `librime\`、不改面板几何（215×32）、不新增第三方依赖。

## 八、需要用户到场的四件事（本批唯一未闭环）

1. **人眼看两态图标逗号**：在全角（空心环）与半角（实心点）两态下，逗号是否**轮廓清晰、无锯齿、圆头与尾巴连接圆滑**。
2. **手按裸 Shift 切中英文**：设置栏的「中/英」标志**与**全/半角（中英文标点）图标是否**同时**跟着变。
3. **手点一次「中英文标点」按钮**切到英文标点，再在**中文态**打逗号 ⇒ 应得到英文逗号 `,`（此路径的合成点击在本机面板上无效，只能手点）。
4. **手点按钮后直接继续打字**：不点输入框也应该能继续输入（不失焦）。
5. 顺带验 D2：中文态打数字 / 空格 ⇒ 应仍为**半角**（证明 `full_shape` 已不参与）。

## 九、本批工具坑（重蹈覆辙会白烧一轮构建）

1. **`fw.shapes[1]` / `hw.shapes[1]` 在 v10 下是未初始化内存**（`shapeCount` 恒 1）；`ptCount` / `pts[]` / `kSolidPolygon` 已删，
   任何引用都会编译失败或读垃圾。
2. **`GetGlyphOutlineW` 传零 `MAT2` 会返回 `GDI_ERROR`**，且业务代码会静默回退 ⇒ 「改公式像素零差异」先怀疑**度量没取到**（记忆 #36）。
   `MAT2` = 16 字节，单位矩阵 `{0x00010000, 0, 0, 0x00010000}`。
3. **`TextOutW` 前必须 `SetBkMode(TRANSPARENT)`**，否则字形单元格出现白块；基线对齐用 `TA_LEFT | TA_BASELINE`。
4. **GDI 小尺寸实心椭圆会光栅化成四角星**（记忆 #37）⇒ 圆点一律 `RoundRect` + 半径 = 半边长。
5. **锁屏态下 UI 自动化与抓屏全废**（记忆 #31）：`LogonUI` 在跑 ⇒ `GetForegroundWindow()` 返回 0、合成输入送不进去。
   任何探针开工前先 `Get-Process LogonUI`。
6. **`TEST_BUILD_EXIT=0` 只代表编译成功**：本轮首轮出现 `TEST_BUILD_EXIT=0` 但 `TEST_DIALOG_EXIT=1`（4 条 T28 tooltip 断言滞后于文案改名），
   必须单看 `TEST_DIALOG_EXIT` / `TEST_REFACTOR_EXIT`。
7. **匿名命名空间可重复书写并合并**：`DrawInkPlacedChar` 的前置声明块与实现块同属一个 unnamed namespace，不构成重定义。
8. **装机数据目录是 `$INSTDIR\weasel\data`** ⇒ 只改仓库 `output\data\*.yaml` 不会到机器，必须重打安装包 + 静默重装。
9. **NSIS 的 `no files found` 警告是既有噪声**（`weaselARM.dll` / `weaselARM64.dll` / `data\*.gram` / `data\lua\*.txt`），看 `NSIS_RC`。
10. **提交前必查**：`git status -sb` + `git diff --stat`，确认 ` m librime` 假脏没被带进去；**禁止 `git add .`**。

## 十、与批次 H / G 的关系 / 本批遗留

- v7（对角布局）、v8（句号收小 + 中文逗号重画）、v8.1（描边 1px）、v9（两图元分离 + 奇×奇墨迹盒）**全部维持**。
- **v9 的「12 点 / 10 点多边形逗号」判据已被 v10 取代**：批次 H §七 D9 里关于多边形点数的部分**不再适用**，新判据 =
  `commaInk` 恒 **7×13** + `commaChar` 码点（全角 `0xFF0C` / 半角 `0x2C`）+ 字形往返（`MeasureCommaGlyph` → `GlyphPenPointForInk` → 回推墨迹盒原点）。
- **v6「字体方案不可行」的结论判定为错**：真因是**字号取小**（seed 高度不够），已在 `include\FluxingPanelFwIcon.h` 文件头 v10 段记录。
- 本批遗留（未深查，不阻塞）：`WeaselTSF` 不链接 `RimeWithWeasel.lib` ⇒ 服务端与前端职责边界仍需后续 ADR 明确；
  历史文档 `gap-matrix.md` 里对 `ShapeModeText` 的引用**按规矩不改写**。
- 建议提交信息：`fix(WeaselServer): 快捷面板图标 v10 字符逗号 + 全角=中文标点 + 点击不失焦（批次 H2）`；
  提交后以 `git log -1` 为准（不要引用本文里的任何哈希猜测）。
- 机器侧注意：本次会话曾按用户要求设置 **2026-11-08 04:00 自动关机**（`shutdown /s /t ...`），需要时用 `shutdown /a` 取消。