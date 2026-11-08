# 批次 K 交接 · 快捷面板「中英文标点」图标 v13（英文逗号左缘裁切）

- 日期：2026-11-08
- 会话：`mvs_ee479146260b48e5ba75f1a3c313b405`
- 分支：`Fluxing`（本批改动另起提交）
- 上游：[批次 J 交接](handoff-2026-11-08-batch-j-v12-punct-comma-tray-icon.md)（v12 图标 + ADR 0044 托盘摘除）· [批次 I](handoff-2026-11-08-batch-i-v11-punct-session-gate.md)
- 需求基线：`.specify/specs/099-quickpanel-sogou-benchmark/spec.md` · `tasks.md`「批次 K 追补」段
- 本批**没有**新建 ADR：v13 只是既有几何常量的一次迭代，未改架构、未改 IPC 协议、未改 build 链。

---

## 1. 本批目标（用户 2026-11-08 三条反馈拆解）

1. **「输入法中英文图标」指哪个** → 用户确认 = 通知区域的**方框「中」**（火流猩自己的语言栏项，tooltip「左键切换模式，右键打开菜单」，`WeaselTSF/LanguageBar.cpp:150`）。旁边那个**红猿头 logo 是 Windows 自带的 IME 指示器**，本仓库代码控制不了（只能改 Windows 设置 / 策略）⇒ 处置对象锁定方框「中」，红猿头不动。
2. **tooltip 遮挡** → 用户答「暂时没发现遮挡」⇒ **结案**。该 tooltip 是快捷面板**自身的槽位说明气泡**（自绘顶层窗 `FluxingQuickPanel_Tooltip`，`WeaselServer/QuickPanelDialog.cpp:1177-1336`，hover 槽位才出现，`:1604/:1742` 触发），不是托盘系统气泡。
3. **v13 英文逗号几何** → 用户原话：「英文逗号圆点部分过于偏左，可以右移一点，从而减少尾巴的长度，和中文逗号区分度更大。」

## 2. 进度总览

| 项 | 状态 |
|---|---|
| 图标 v13（英文逗号左缘裁切 2 列）代码 + 单测 | ✅ 已落盘 |
| 单测 `TestQuickPanelDialog` | ✅ **258/258**（255 + T33s/T33s2/T33s3） |
| 单测 `TestQuickPanelRefactor` | ✅ 40/40（`TestWeaselIPC` rc=0） |
| L0 静态门禁 | ✅ `checks=14 pass=14 fail=0 skip=0` |
| 全量构建 + NSIS | ✅ 安装包 42,825,119 B @2026-11-08 17:02:08 |
| 静默重装 + 装机核对 | ✅ `WeaselServer.exe` / `weasel.dll` / `WeaselDeployer.exe` **MATCH=True**（v13 已上机）；`weaselx64.dll` **MATCH=False** |
| 运行时像素取证（P4） | ✅ PASS；英文态图标左边界右移 2 物理像素 |
| tooltip 遮挡项结案 | ✅ 写回 tasks.md |
| **用户目视终确认（v13 辨识度）** | ❌ **待用户**（对照图已备） |
| **托盘方框「中」真机验收** | ❌ **未闭环**（x64 dll 被 64 位宿主占用，Agent 无法替换） |
| 文档 / CHANGELOG / handoff | ✅ 本批已写 |

## 3. 结论（按可信度分级）

### 已验证（有产物、可复跑）

- **v13 修法（唯一事实源 `include/FluxingPanelFwIcon.h`）**：`kCommaCutLeftEn = 2`；`Icon` 增 `commaCutLeft`（中文态 = 0，英文态 = 2）；新增纯函数
  `GlyphClipRect(pen, GlyphInk, cutLeft)` → 返回 `l = pen.x+originX+cutLeft`、`t = pen.y-originY`、`r = pen.x+originX+width`、`b = pen.y-originY+height`，
  **右/下为 GDI 开区间**（`IntersectClipRect` 语义 `x∈[left,right)`），**基准取落笔点推出的实际墨迹盒**而非目标盒（目标机缺字体、GDI 替换后墨迹会差 ±1 px，按实际盒裁才稳定）。
  生产侧 `WeaselServer/QuickPanelDialog.cpp::DrawInkPlacedChar` 用 `SaveDC` + `IntersectClipRect` + `RestoreDC` 实现裁切（**不碰 AlphaBlend、不换字体**）。
  中文态逗号、字体、目标盒尺寸、句号图元**全部未动**（v12 契约 `hw_target={5,17,12,28}` / `hw_pen(3,20)` / `fw_pen(-3,27)` 保持原值）。
- **候选裁决（离线 GDI 探针，`tmp/probe-fwicon-v13d.ps1` → `tmp/v13-sheet2.png` + `tmp/v13-metrics.txt`）**：
  - Times New Roman 52px `U+002C` = **8×12，origin=(2,3)**，圆头**横跨墨迹盒整宽且偏左** + 长尾左钩；
  - SimSun 48px `U+FF0C` = **8×13，origin=(6,11)**，大圆头 + 长弯尾 ⇒ 两态轮廓相近，肉眼难分；
  - **Segoe UI 12 / Cambria 13 / Palatino 13 的 `U+002C` 结构反而更像宋体逗号**（斜直笔画），实测**更差** ⇒ 换字体路线被证伪；
  - 唯一同时满足「圆头右移 + 尾巴缩短」的是 **TNR + 左缘裁切 2 列**（可见墨迹 7..12，质量 54→43）。
- **单测**：`T33s`（英文态 `commaCutLeft==2`、中文态 `==0`、常量 `==2`）、`T33s2`（裁切矩形精确 `(7,17)-(13,29)`）、`T33s3`（不裁切时 `(5,17)-(13,29)`，且 `clip.left == full.left + 2`）⇒ 生产与单测共用同一纯函数（R7）。
  **258/258** 全绿，`TestQuickPanelRefactor` 40/40。
- **L0 门禁**：`scripts/gate/L0-static.ps1 -Mode source` ⇒ `checks=14 pass=14 fail=0 skip=0`，`L0_RC=0`（本批比批次 J 多 2 项 PASS：skip 由 2 降到 0）。
- **构建**：`tmp/build-v13.log` ⇒ STEP1 x64 TSF rc=0、STEP2 x86 SLN rc=0、STEP3 NSIS rc=0；
  `output/archives/fluxing-1.2.1-installer.exe` **42,825,119 B @2026-11-08 17:02:08**。
- **装机**：`tmp/reinstall-v13.log` ⇒ `INSTALL_EXIT=0`；
  `WeaselServer.exe`（2,337,280 B @16:50:54）**MATCH=True**、`weasel.dll` MATCH=True、`WeaselDeployer.exe` MATCH=True ⇒ **v13 已在服务端生效**；
  `weaselx64.dll`（装机副本 mtime **09:59:44**）**MATCH=False** ⇒ x64 仍是旧件（ADR 0044 的托盘摘除**未上机**）。
- **★ 运行时像素（v13 效果证据）**：`tmp/probe-batch-i3.ps1 -OutBase batch-v13` ⇒
  - `P4 VERDICT punct_follows_state = PASS`（state0 `，` U+FF0C；state1、state2 `,` U+002C）；
  - slot2 英文态墨迹 **ink=56，bbox=(14,9)-(29,32)，16×24**（v12 同项为 **ink=59，18×24**）⇒ 图标整体左边界**右移 2 物理像素**、尾巴变短；
  - 裁片 `tmp/batch-v13-P4-slot2-state0.png` / `-state1.png`；与 v12 并排对照拼图 **`tmp/v13-runtime-sheet.png`**（v12 中文 / v12 英文 / v13 中文 / v13 英文，各 6× 最近邻放大）。
  - `P6` 两步仍 FAIL，与批次 I / J 基线**逐字符相同** ⇒ 探针既有缺陷，非本批回归。
- **托盘现状（沿用批次 J 结论）**：通知区域并存两个「任务栏输入指示」—— 我们的（方框「中」，本次摘除对象，改动未上机故仍在）与 Windows 自带 IME 指示器（红猿头，本仓库改不掉）。

### 推断（合理但未证实）

- 按**实际墨迹盒**裁切 ⇒ 目标机若缺 Times New Roman、GDI 替换字体，裁切位置仍稳定，不会出现 ±1 px 漂移。
- 裁切后英文逗号与中文逗号（大圆头 + 长弯尾）在**轮廓层面**已明显分离（对照图可见），但**「一眼可分」仍是人眼标准**，需用户终确认。

### 未验证

- **v13 英文逗号辨识度的人眼终确认**（用户）—— 对照图 `tmp/v13-runtime-sheet.png` 已备。
- **托盘方框「中」消失** —— 被 x64 dll 占用卡住，需用户注销 / 重启 Windows（或关闭占用宿主）后重装。
- **开机首次切换卡顿（ADR 0042）** —— 需重启 Windows，Agent 不可执行。
- tooltip 遮挡项按用户答复**结案**（不再追查托盘系统 tooltip）。

## 4. 下一阶段执行序（新会话照此做）

1. **请用户目视确认** `tmp/v13-runtime-sheet.png`：v13 英文逗号是否够辨识（圆头右移、尾巴缩短，与中文「大圆头 + 长弯尾」一眼可分）。若仍不满意 ⇒ 只改 `include/FluxingPanelFwIcon.h` 的 `kCommaCutLeftEn`（0~4 之间试），**同步** `T33s/T33s2/T33s3` 的期望值，重跑门禁 + 构建 + 重装。禁止在别处另抄公式（R7）。
2. **x64 落地**：用户注销 / 重启 Windows（或关闭占用宿主）后重装 `output/archives/fluxing-1.2.1-installer.exe` ⇒ 跑 `pwsh -NoProfile -File tmp/probe-tray-j.ps1`（判据行 `VERDICT TRAY_PINNED_COUNT=`，正确口径 = **「任务栏输入指示」按钮个数**，改前 2 → 改后应 1）与 `tmp/grab-tray-j.ps1` 抓屏互证。**不得**按「英/中」字形计数（UIA 里我们的项 glyph 名为空 ⇒ 必然假阴性）。
3. **开机首切卡顿**（ADR 0042）复验：重启后首次切到火流猩，观察延迟并查 `[server]` 诊断日志。
4. 两条验收都通过后：把 spec 099 的批次 K 段落标记完成，并按 AGENTS.md §9 再出一份 handoff。

## 5. 本轮工具坑（务必别再踩）

- **PowerShell 补丁脚本的单元素数组会被展开**：`tmp/patch-v13.ps1` 用替换对列表做批量替换，头文件 6 对成功、**测试文件 4 对全部空替换**（脚本每对都自报 OK）⇒ 单测新断言**根本没落盘**，而「单测 255/255 全绿」看起来毫无破绽。⇒ 落盘后**必须** ① `grep` 目标标识符（`T33s`）；② 核对**断言计数变化**（255 → 258）。**「脚本自报成功」不是证据。**
- `New-Object System.Drawing.Rectangle($a,$b,$c,$d)` 会把参数当数组 ⇒ 用 `[System.Drawing.Rectangle]::new(...)`。
- 离线 GDI 探针的坑（上一轮已修、复用时别退化）：C# 内**必须**自带 `CreatePen`/`Ellipse` 等 P/Invoke 与 `PS_SOLID` 常量；`GetGlyphOutlineW` 的 `MAT2` 必须是**单位矩阵**（零矩阵返回 `GDI_ERROR`）；**分析区要按字形实际墨迹盒取**，按目标盒取会被环/点图元污染。
- NSIS 静默安装：GUI 子系统 exe **必须经 `cmd /c`** 启动；`/D=` 必须**最后一个参数且不带引号**。
- `Start-Process WeaselServer.exe` 要带 `-WorkingDirectory <repo>`，否则 `qp-dump.bmp` 落到别处，探针报「NO FRESH DUMP」。
- L0 门禁入口是 `scripts\gate\L0-static.ps1 -Mode source`；`scripts\gate\run-all.ps1` **没有** `-Level` 参数（只有 `-Root/-SkipL0/-SkipL1/-SkipL3/-SkipCases/-SkipSandboxGate/-CaseTimeoutSec`）。
- 单工程编译必须补 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`（否则 WeaselIPC 的 include 找不到）。
- 铁律 30：后台子代理写文件期间读它的产物 = 读到半截内容；验收前先确认它已到终态。

## 6. 证据路径速查（全部在 `tmp/`，未入仓）

`build-v13.log` · `reinstall-v13.log` · `run-v13-verify.ps1` · `make-v13-sheet.ps1` · `probe-fwicon-v13d.ps1` · `v13-sheet2.png`（候选对照）· `v13-metrics.txt`（候选量化）· `v13-runtime-sheet.png`（★ v12/v13 四格对照）· `batch-v13-P4-slot2-state0.png` / `-state1.png` / `.bmp` · `v12b-P4-slot2-state0.png` / `-state1.png`（v12 旧裁片）

## 7. 改动清单（本批）

| 文件 | 改动 |
|---|---|
| `include/FluxingPanelFwIcon.h` | v13：`kCommaCutLeftEn=2`、`Icon::commaCutLeft`、纯函数 `GlyphClipRect()`、文件头 v13 病灶/修法注释 |
| `WeaselServer/QuickPanelDialog.cpp` | `DrawInkPlacedChar` 增 `cutLeft` 参数；`SaveDC` + `IntersectClipRect` + `RestoreDC` 实现裁切 |
| `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` | 新增 `T33s` / `T33s2` / `T33s3`（同源断言） |
| `.specify/specs/099-quickpanel-sogou-benchmark/tasks.md` | T-9J6③ 结案；追加「批次 K 追补」段 |
| `.specify/memory/lessons-learned.md` | 新增 L201（补丁脚本单元素数组展开 ⇒ 空替换） |
| `CHANGELOG.md` | `## [Unreleased]` 下新增批次 K 条目 |
| `docs/archive/handoffs/handoff-2026-11-08-batch-k-v13-punct-comma-cut.md` | 本文件 |
| `docs/archive/prompts/prompt-next-session-2026-11-08-batch-k-v13-punct-comma-cut.md` | 接力 prompt |
| `CLAUDE.md` | §2「本批交接指针」换成本批，批次 J 降为「上一批」 |
