# Handoff 2026-11-07（批次 F · 收尾版）：全半角图标改「，。」字形组合 + 全半角显示/点击两缺陷 + 面板缩小到 215×32

> **状态：批次 F 三项用户指令全部执行完毕并通过真机验收** —— 代码 / 单测 / 全量构建 / NSIS 打包 / 静默装机 / 150% DPI 像素验收全部走完；L0 静态门禁 PASS（12 通过 / 0 失败 / 2 跳过）。**唯一未完成的是提交与推送**（见 §五 执行序第 1 步）。
> 本文是**新建**文件（未改写 `handoff-2026-11-07-import-cancel-bug-ime-switch-window-quickpanel-v5.md` 等更早文档）。下一会话请按 §五 执行序接手，**勿重做已完成的取证与构建**，**勿改写本文以外的历史 handoff / prompt**。
>
> **分支 / 提交**：分支 `Fluxing`，本批 3 个 commit —— `38b4c73` fix(RimeWithWeasel) / `1df3fad` feat(WeaselServer) / 其后一个 docs commit（即本文与接力 prompt，其 hash 会随本文修订而变，**以 `git log -1` 为准**）；**尚未推送**，`kizemo/Fluxing` 仍停在 `d4aa63a`，本地 ahead 3；工作区仅剩 ` m librime` 子模块假脏（**不得提交**）。

---

## 一、当前状态（一段话）

批次 F 的三项用户指令（① 全半角按钮改成「中英文逗号 + 句号」字形组合；② 修「全半角图标不随中英文切换变化」与「点击无法切换全半角」两个缺陷；③ 快捷设置栏整体再缩小、参考搜狗设置栏）**代码全部落地并通过真机像素验收**：面板物理尺寸 323×48 @150% DPI（逻辑 215×32，原 239×36），全角态 `，。` 墨迹 18×4 raw px、半角态 `,.` 墨迹 8×5 raw px，两者在 28px 键帽内水平+垂直居中（偏差 ≤1px），点击可双向切换且图标随 `ascii_mode` 自动变化，可逆性 4 条断言全 True（`tmp\batchf5.log`、`tmp\batchf6.log` 均 `VERDICT=PASS`）。**门禁 L0/L3 均已通过**（L3 8 个变异全部 KILLED、0 存活），代码已分 3 个 commit 落地。**未做**：推送到 `kizemo/Fluxing`（等用户确认）、「，。」观感的人眼确认、Bug #4 真机复现。

---

## 二、必读文档（全绝对路径，按此顺序）

| # | 文档 | 为什么必读 |
|---|---|---|
| 1 | `F:\soft\00selfmade\rime_claude\CLAUDE.md` | 进站导航：强制项、分流规则、专项规则表 |
| 2 | `F:\soft\00selfmade\rime_claude\AGENTS.md` | 操作手册（§1 首屏 / §4 危险区 / §5 checklist / §9 交接双产物） |
| 3 | `F:\soft\00selfmade\rime_claude\CONTEXT.md` | 身份 / 模块 / 词汇 / 不变量 |
| 4 | `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` | 法则层 R1–R9 / P1–P8（P2 禁 TSF 阻塞 I/O、P4 commit 规范、P5 中文化） |
| 5 | `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md` | §A 部署、§11 NSIS 配方 |
| 6 | `F:\soft\00selfmade\rime_claude\docs\agents\domain.md` | 工程技能消费契约 |
| 7 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` | 本面板的需求基线（含「显示路径零 I/O」约束） |
| 8 | `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md` | 阶段任务与验收判据 |
| 9 | `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-import-cancel-bug-ime-switch-window-quickpanel-v5.md` | 上一批（批次 E）完成态，含 Bug #4 根因与 §八 执行序；**本文不重复其内容** |
| 10 | `F:\soft\00selfmade\rime_claude\.specify\memory\lessons-learned.md` | L196 = Bug #4 根因；L## 事故溯源 |
| 11 | `F:\soft\00selfmade\rime_claude\docs\adr\0037-*.md`、`0038-*.md` | 面板几何 / 图标层决策（改几何前必读） |
| 12 | `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md` | 仅当要改 `output/data/**/*.yaml` 时读 §5–§7、§9、§10 |

---

## 三、本批进度（逐项，含验证等级）

| # | 用户指令 | 状态 | 结论与落点 | 验证等级 |
|---|---|---|---|---|
| 1 | 全半角按钮改「中英文逗号 + 句号」组合 | ✅ 完成 | `DrawIconFullwidth` 整段重写为字形组合：全角 `kFullPair = L"\uFF0C\u3002"`、半角 `kHalfPair = L",."`；逐字 `GetGlyphOutlineW(GGO_METRICS, …, &mat)` 累进笔位、求墨迹并集，`penX = x + (kIconViewBox − inkW)/2 − inkL`、`baselineY = y + (kIconViewBox − inkH)/2 − inkT`，`SetTextAlign(TA_LEFT\|TA_BASELINE)` + `TextOutW`。新增 `kIconViewBox = 30` 与 `static HFONT s_hFontPunct`（`-15` raw px / FW_BOLD / Microsoft YaHei，`ApplyPhysicalLayout` 重建、`OnDestroy` 释放）。**未用 `AlphaBlend`**（v4 logo 黑框前科）。 | **verified**（真机像素：全角 18×4、半角 8×5、`DIFF_PIXELS B_vs_E=0 / C_vs_F=0`） |
| 2-① | 全半角符号随中英文自动切换，设置栏图标应随之变化（当前不变） | ✅ 完成 | 新增状态源 `RimeWithWeasel::GetPanelInputState(bool* ascii, bool* fullwidth)`：`a = m_global_ascii_mode`、`f = m_full_shape`，命中 `m_session_status_map` 时用 `get_status`/`free_status` 校正；输出 `*fullwidth = f && !a`（生效标点宽度）。`WeaselServerApp::ShowQuickPanel()` 首帧改用该口径。`get_status` 是进程内 C API 读内存，**不违反 spec 099「显示路径零 I/O」**。 | **verified**（点击中英文键后 `DIFF_PIXELS A_vs_B=51`，图标宽度 8→18） |
| 2-② | 点击无法切换全半角输入 | ✅ 完成 | 根因：`SetOption` 的会话解析只认 `ipc_id`，快捷面板来自 server 自身消息循环、拿不到 ipc_id ⇒ 全部 return（不写缓存）。改为 `ipc_id` → `m_active_session` → 仅当 `m_session_status_map.size()==1` 取 begin 的三级回退；`broadcast_ascii` 遍历 `pair.second.session_id`。**解析不出就 return 且不写缓存**（代码注释：宁可按不动，也不能让镜像说谎）。点击语义（D3）：`newFull` 且 `panel_ascii` 时先 `SetOption(0,"ascii_mode",false)` 再 `SetOption(0,"full_shape",newFull)`。 | **verified**（`CLICK_CHANGES_ICON`、`FULLWIDTH_TOGGLE_REVERSIBLE`、`ASCII_TOGGLE_REVERSIBLE` 全 True） |
| 3 | 快捷设置栏再缩小、参考搜狗设置栏 | ✅ 完成 | 几何 239×36 → **215×32**（`kBtnSize` 下界 28：Symbols 线描最宽 `X[3..27]` = 25 raw px 不缩放）；`test/TestQuickPanelDialog` T25 段硬编码同步为 `215×32 / 323×48 / 430×64`。 | **verified**（`PANEL client=323x48 dpi=144 scale=1.5 logical=215x32`，`GEOM_215x32=True`） |
| 4 | 单测 | ✅ 全绿 | `TEST_DIALOG_EXIT=0` Dialog 231/231；`TEST_REFACTOR_EXIT=0` Refactor 40/40；`ALL_DONE`。 | **verified** |
| 5 | 全量构建 + NSIS 打包 | ✅ 完成 | `X64_TSF_OK / X86_SLN_OK / NSIS_RC=0 / ALL_STEPS_OK`，退出码 0；产物 `output\archives\fluxing-1.2.1-installer.exe` = 42,827,816 B / 2026-11-07 14:09:16 / sha256 `CC812697AFA709279D1172758B51FD8FE1789E696966A0AC66BC8B9A5C0E3E26`。 | **verified** |
| 6 | 静默装机 | ✅ 完成 | `INSTALL_EXIT=0`；四文件 MATCH：`WeaselServer.exe` 2,330,624 B(14:07:57↔14:07:56)、`weaselx64.dll` 1,987,072 B、`weasel.dll` 1,675,264 B、`WeaselDeployer.exe` 1,004,544 B；explorer 重启 pid=594684，`WeaselServer` pid=546184、`LogonUI`=0。 | **verified** |
| 7 | 门禁 L0 | ✅ PASS | `mode=source checks=14 pass=12 fail=0 skip=2`（跳过 `nsis-compile` / `guard-unit-test`，因构建已在第 5 步单独跑过）。日志 `tmp\gate-L0-batchf.log`。 | **verified** |
| 8 | 门禁 L3 突变 | ✅ PASS | `mutations=9 killed=8 survived=0 invalid=0 skipped=1`（M06 需 makensis，按 `-SkipCompile` 跳过），`L3_EXIT=0`；M01/M02 安装器清理块、M03 版本一致性、M04 缺陷登记、M05a/M05b 编码行尾、M07/M08 对话框取消与中文化全部 KILLED。日志 `tmp\gate-L3-batchf.log`。 | **verified** |
| 9 | 提交 | ✅ 完成 | 3 个 commit：`38b4c73` fix(RimeWithWeasel) 2 文件 / `1df3fad` feat(WeaselServer) 5 文件 / docs commit 2 文件（本文与接力 prompt）。提交前已核 `git status -sb` + `git diff --stat`（8 files, +316/−67），`weasel.props` / `env.bat` / `*.obj` 未误入。 | **verified** |
| 10 | 推送 `kizemo/Fluxing` | ❌ 未做 | 远端仍在 `d4aa63a`，本地 ahead 3。命令：`git push origin Fluxing`。 | — |

---

## 四、改动文件清单（提交前对照）

绝对路径 + 本批性质：

1. `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.cpp` — `DrawIconFullwidth` 重写；`s_hFontPunct` 定义/重建/释放；`ApplyPhysicalLayout` 字体创建；绘制 switch 注释同步 `kVisualToLogical`。
2. `F:\soft\00selfmade\rime_claude\WeaselServer\QuickPanelDialog.h` — `kIconViewBox = 30`、`s_hFontPunct` 声明、几何常量块（`kPanelW=215`、`kPanelH=32` 等）。
3. `F:\soft\00selfmade\rime_claude\WeaselServer\WeaselServerApp.cpp` — `ShowQuickPanel()` 首帧口径改 `GetPanelInputState`；全半角 lambda 改 D3 语义。
4. `F:\soft\00selfmade\rime_claude\include\RimeWithWeasel.h` — 新增 `GetPanelInputState` 声明。
5. `F:\soft\00selfmade\rime_claude\RimeWithWeasel\RimeWithWeasel.cpp` — `SetOption` 三级会话解析 + `broadcast_ascii` 遍历；`GetPanelInputState` 实现。
6. `F:\soft\00selfmade\rime_claude\test\TestQuickPanelDialog\TestQuickPanelDialog.cpp` — T25g–T25j 硬编码尺寸同步。
7. `F:\soft\00selfmade\rime_claude\CHANGELOG.md` — `## [Unreleased]` 下三条（`feat(WeaselServer)` 图标+缩小 / `fix(WeaselServer)` 两缺陷+D3 / `chore(build)` 构建号）。

**不进 git**：`env.bat`（本地忽略文件，本批 `WEASEL_BUILD` 1→2、`FLUXING_VERSION=1.2.1`）、`tmp\**`（证据与探针）、` m librime`（子模块假脏）。

---

## 五、下一阶段执行序（按此顺序，理由随附）

1. **推送批次 F 到 `kizemo/Fluxing`** —— 理由：本地已 ahead 3（`38b4c73` / `1df3fad` / 一个 docs commit），远端仍在 `d4aa63a`；先对齐远端，下一会话才有一致基线。命令 `git push origin Fluxing`，推送前再核一次 `git status -sb`（只应剩 ` m librime`）。
2. **把 §八 的「人眼确认」交给用户** —— 理由：本会话模型读不了图，观感只能数值间接成立；这是唯一还需人参与的验收项。
3. **Bug #4（切输入法后一小段时间只能打英文）真机复现** —— 理由：根因已在 `lessons-learned.md` L196 坐实但**缺运行时佐证**；需用户在场配合「切换后立即打字」，并按批次 E 未完成项补日志。
4. **清理旧探针坐标** —— `tmp\qpanel-analyze.ps1`、`tmp\probe-qpanel-menu.ps1` 内仍按旧的 239×36 写，直接跑会裁错区域；要么改 215×32，要么删。理由：留着会制造假失败。
5. **x86 TSF shim 增量链接遗留** —— `D:\Program Files\fluxing\weasel\weasel.dll` 仍是 00:34:02 旧件（批次 E/F 未改 WeaselTSF，增量跳过链接）；下次动 TSF 时必须确认它真的被重链接并装进安装目录。理由：静默跳过的增量链接会让真机验错对象。

---

## 六、证据路径（全部绝对路径）

| 用途 | 路径 |
|---|---|
| 取证闭环 + 决策 D1–D4 | `F:\soft\00selfmade\rime_claude\tmp\batch-f-evidence.md` |
| 构建 + 打包日志 | `F:\soft\00selfmade\rime_claude\tmp\build-batch-f.log` |
| 真机探针日志（**两跑均 PASS，首选证据**） | `F:\soft\00selfmade\rime_claude\tmp\batchf5.log`、`F:\soft\00selfmade\rime_claude\tmp\batchf6.log` |
| 失败/中间态探针对照（说明为何改判据） | `F:\soft\00selfmade\rime_claude\tmp\batchf.log`、`batchf2.log`、`batchf4.log` |
| 离线帧分析数值（直方、墨迹 bbox、diff mask） | `F:\soft\00selfmade\rime_claude\tmp\batchf5-analyze.log` |
| 帧位图（32bpp top-down BMP） | `F:\soft\00selfmade\rime_claude\tmp\batchf6-{Apre,A,A2,B,C,E,F}.bmp` |
| 放大裁片（本会话读不了图，供人眼复查） | `F:\soft\00selfmade\rime_claude\tmp\crop-batchf5-slot2-{A,B,C,E,F}.png`、`tmp\crop-batchf5-panel-B.png` |
| L0 / L3 门禁日志 | `F:\soft\00selfmade\rime_claude\tmp\gate-L0-batchf.log`、`tmp\gate-L3-batchf.log` |
| 探针脚本（**第四版，唯一可信形态**） | `F:\soft\00selfmade\rime_claude\tmp\probe-batchf-state.ps1` |
| 离线分析 / 裁片脚本 | `F:\soft\00selfmade\rime_claude\tmp\analyze-batchf5.ps1`、`tmp\crop-batchf5.ps1` |
| 构建 / 装机 / 单测脚本 | `tmp\build-batch-e.cmd`（本批直接复用）、`tmp\reinstall3-batch-e.ps1`、`tmp\build-qpanel-tests.cmd` |
| 用户参考图 | `E:\Users\Duanyi\.minimax\.minimax\v2\assets\2026\11\07\12-34-03-830-asset_20261107-123403-830_10a122f227ed_7cf33548-image.png`、`…-12-34-03-834-…fadf4bff-image.png` |

**可直接复用的命令（勿重造）**

```powershell
# 单测（改 UI 后必跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd"

# 全量构建 + NSIS（>100s，必须后台跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd" 2>&1 | Tee-Object -FilePath "F:\soft\00selfmade\rime_claude\tmp\build-batch-f.log"

# 静默重装 + 哈希核对
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1"

# 真机探针（脚本跑一次，再单独读日志）
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\probe-batchf-state.ps1" -OutBase 'batchf7'
Get-Content 'F:\soft\00selfmade\rime_claude\tmp\batchf7.log' -Encoding utf8

# 门禁
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1" -Mode source -SkipNsisCompile -SkipGuardUnitTest
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\scripts\gate\L3-mutation.ps1" -SkipCompile
```

---

## 七、本批决策（改之前先读这里，别推翻重来）

| 编号 | 决策 | 依据 |
|---|---|---|
| D1 | 状态源用 `GetPanelInputState()`，不用 IPC 新字段 | `get_status` 是进程内 C API 读内存，不违反 spec 099「显示路径零 I/O」；改 `WeaselIPCData.h` 要四边同步 + ADR + 新测试，代价远高 |
| D2 | `fullwidth = full_shape && !ascii_mode`（生效标点宽度） | 英文态下 `full_shape` 即使为 true，实际输出的也是半角标点；图标必须反映「看得见的效果」 |
| D3 | 点「全半角」的意图 = 要全角标点 ⇒ 英文态下单击先退出英文态再置全角 | 用户原话「点击无法切换全半角输入」；此语义已写入 `CHANGELOG.md`，**探针的 `A_vs_D==0` 判据因此作废**（数学上不成立，不是缺陷） |
| D4 | 图标 = 全角 `，。` / 半角 `,.`；两字形共用一条基线；墨迹并集在 30×30 viewBox 内双轴居中；纯 GDI 文本 | 用户新指令覆盖批次 E 的宽/窄圆角框方案；复用 `fluxing::panel_ink::FillUnitMat2()`（**禁止零 MAT2**）；字体固定 raw px 不随 dpr 缩放，避免 150% 下糊 |
| D5 | 面板 215×32（不是更小） | `kBtnSize` 下界 28 由 Symbols 线描最宽 25 raw px 决定；再小就要缩线描，视觉失真 |
| D6 | 版本只 bump `env.bat` 的 `WEASEL_BUILD`（1→2），`PRODUCT_VERSION`/`FILE_VERSION` 保持 `1.2.1.0` | `install.nsi` 全部用 `${FLUXING_VERSION}.${WEASEL_BUILD}` 派生 |

---

## 八、需要用户到场的两件事

1. **人眼确认「，。」观感是否够清晰** —— 数值上墨迹 18×4 / 8×5 raw px、居中偏差 ≤1px、颜色 `#82828C`（与同级图标一致）；但标点墨迹天然比 17px 线描图标轻，**是否"看着够清楚"只能由人判断**。放大裁片见 §六。
2. **Bug #4 复现配合** —— 切换输入法后**立即**打字，观察是否出中文；同时留意 `WeaselServer` 是否在跑。探针开工前先 `Get-Process LogonUI` 判锁屏。

---

## 九、本批工具坑（重蹈覆辙会白烧一轮构建）

1. **本会话模型读不了图** —— `read` PNG 返回“Image not sent to the summary request”。所有视觉结论必须走数值（像素 / 亮度 / bbox），或交由用户目视。
2. **固定墨迹阈值会造假阴性** —— 底色 lum≈220、字形 lum≈131，固定阈值 110 恒得 `ink=0`，曾把 `FULLWIDTH_WIDER_THAN_HALF` 误判成 False。已改**自适应**：取裁剪区亮度众数 −12（实测 `thr=208`）。
3. **首帧是淡入中间态** —— 直接抓 A 得到 91% 全画布差，同时产生假阳性与假阴性。必须先丢一帧 + `Sleep 900` + 连抓 A/A2 断言 `STABILITY==0` 才认账。
4. **写了诊断函数要确认它真被调用** —— 第三版 `Measure-Lum` 是死代码，日志里一条 lum 行都没有。
5. **PS 7 调 `ClientToScreen` 会 ParserError**（`[ref]$pt` 触发「方法调用中缺少 “)”」）⇒ 在 C# 侧封装 `public static int OX,OY; public static void ClientOrigin(IntPtr h)`。
6. **写文件后必须回读校验** —— `[ref]$pt`、`Grab($pt.X,…)` 都出现过变量引用失效，导致静默抓屏 (0,0)。
7. **`WM_COMMAND 40018` 是 toggle** —— 面板已开再发会关闭；必须「先 `Find-Panel`，未开才发」。
8. **没有输入会话时 `SetOption` 按设计直接 return** —— 探针必须先拉起并激活记事本，否则点击零变化会被误判成缺陷。
9. `GetGlyphOutlineW` 老参数顺序在 SDK 10.0.26100 下 **C2664**，正确是 `(hdc, uChar, fuFormat, lpgm, c, pv, lpmat)`；**零 MAT2 ⇒ `GDI_ERROR`**；`MAT2` = 16 字节、单位矩阵 32bit 字 `0x00010000`；`ink_top = baseline − gmptGlyphOrigin.y`（**不是** `baseline − gmBlackBoxY`）；`RimeStatus` 字段名是 `is_full_shape`，**没有** `full_shape`。
10. **桌面自动化前置检查** —— `Get-Process LogonUI` 判锁屏；DPI-非感知进程 `GetWindowRect` 返回逻辑坐标（抓屏要 ×`GetDpiForWindow/96`）；抓屏用 `CreateDC("DISPLAY")`。
11. **BMP 高度为负（top-down）** —— 逐像素循环 0 次 ⇒ diff=0 假象；离线分析脚本已按行序处理。
12. **PowerShell 别名遮蔽** —— `cls` / `R` / `Measure` 都被占用，自建函数须改名（`Get-WCls` / `Get-WTxt` / `Rep` / `Measure-Ink`）；`Write-Host "…" -f a,b` 必须整体加括号。
13. **本批中断原因** —— 上一轮工具调用累计 400+，上下文被压缩两次后终止。预防：每阶段结束立即落盘、单轮 ≤10 次调用（本批已反复触发 hook 告警，务必遵守）。