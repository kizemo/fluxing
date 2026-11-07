# Handoff 2026-11-07（批次 H）：快捷设置栏「全/半角」图标 v9 + 半角=英文标点 + 中英切换联动标点

> 上一批（批次 G · 全半角按钮图标 v7 对角矢量布局，含 v8 / v8.1 追补）见
> `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md`
> （其 §十 是 v8 / v8.1 终态，**别去推翻**）。本文件只覆盖批次 H。

## 一、当前状态（一段话）

用户的**三条**快捷设置栏需求已全部落地代码并通过自动化验收：①「全/半角」图标重做成 v9 几何（两图元分离、逗号改 12/10 点多边形）；
②面板的「全/半角」状态源由裸 `full_shape` 改为 `ascii_punct`，**半角 = 英文标点**（服务端新增 `SetPunctuationWidth`）；
③裸 Shift 切中英文时同步切换标点宽度（`ascii_punct` / `full_shape` 同向），面板图标与「中/英」标志随之变化。
自动化层面全绿：单测 **252/252 + 40/40**、L0 静态门禁 `checks=14 pass=12 fail=0 skip=2`、静默重装后四个二进制 **MATCH=True**、
真机 `Ctrl+.` 隔离实验 **六段全绿**（含「裸 Shift ⇒ 英文标点」）。**唯一未闭环的是人眼视觉验收**：设置栏图标清晰度与 Shift 后图标同步，
以及「点设置栏切半角」这一路径的点击不可自动化（合成点击在本机面板上无效）——这三件事需要用户到现场看一眼。

## 二、必读文档（全绝对路径，按此顺序）

1. `F:\soft\00selfmade\rime_claude\CLAUDE.md`
2. `F:\soft\00selfmade\rime_claude\AGENTS.md`（§1 / §4 / §5 / §9）
3. `F:\soft\00selfmade\rime_claude\CONTEXT.md`
4. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md`
5. `F:\soft\00selfmade\rime_claude\.specify\memory\project-knowledge.md`（§A / §11）
6. 本文件 `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v9-fwicon-batch-h.md`
7. 本批架构决策 `F:\soft\00selfmade\rime_claude\docs\adr\0040-panel-fullwidth-state-source-and-shift-punct-linkage.md`
8. 需求基线 `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\spec.md` 与
   `F:\soft\00selfmade\rime_claude\.specify\specs\099-quickpanel-sogou-benchmark\tasks.md`（P0 段含 v9 / 批次 H 追补项）
9. 若要改 `output/data/**/*.yaml`：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`（§5–§7、§9、§10）
10. 批次 G 历史：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md`

## 三、本批进度（逐项，含验证等级）

| # | 事项 | 状态 | 验证等级 |
|---|---|---|---|
| 1 | 图标 v9 几何（分离 + 12/10 点逗号 + 奇×奇墨迹盒） | 完成 | 单测 + 像素探针（**待用户人眼**） |
| 2 | 面板「全/半角」状态源改 `ascii_punct`（半角=英文标点） | 完成 | 真机 `Ctrl+.` 隔离实验（**点击路径待用户**） |
| 3 | 裸 Shift 联动标点宽度 + 面板图标/中英标志同源 | 完成 | 真机 F 段（**图标同步待用户人眼**） |
| 4 | `melt_eng` / `radical_pinyin` 补 `ascii_punct` 声明 | 完成 | 装机目录 + 部署产物 `build\*.yaml` 已确认 |
| 5 | 单测 / L0 门禁 / 全量构建 / NSIS / 静默重装 | 完成 | 全部自动化通过 |
| 6 | 文档（CHANGELOG / tasks / lessons L197 / ADR 0040） | 完成 | 已写入并逐文件校验编码行尾 |

## 四、改动文件清单（提交前对照）

代码（8 个）：

- `include\FluxingPanelFwIcon.h` —— 图标唯一事实源，v9 几何（`kMaxPolyPoints` 10→12；全角环 `RECT{16,2,28,14}`；全角逗号 12 点；
  半角点 `RECT{17,4,24,11}`；半角逗号 10 点；并集墨迹盒 全角 25×27 / 半角 19×25）。
- `include\FluxingPanelStatus.h` —— 新增 `fluxing::panel_status::ShapeIsFullwidth(ascii_mode, ascii_punct)`（header-only 纯函数，生产 + 单测同源）。
- `include\RimeWithWeasel.h` —— 新增 public `SetPunctuationWidth(bool halfwidth, WeaselSessionId ipc_id = 0)`；
  private `_ResolveTargetSession` / `_ApplyPunctWidth`；成员 `m_ascii_punct` / `m_last_focus_session`。
- `RimeWithWeasel\RimeWithWeasel.cpp` —— `AddSession/ProcessKeyEvent/FocusIn` 记录 `m_last_focus_session`（`FocusOut` **不**清）；
  `SetOption` 的 `ascii_punct`/`full_shape` 统一走 `_ApplyPunctWidth`；`GetPanelInputState` 改为 live 读取（读
  `status.is_ascii_mode/is_full_shape/is_ascii_punct`，输出口径 `ShapeIsFullwidth`，**不写 to_session_id、必须 free_status、绝不 set_option**）；
  `ShiftUp` 的 `kToggleAsciiMode` 分支追加 `_ApplyPunctWidth(to_session_id(ipc_id), !ascii)`。
- `WeaselServer\WeaselServerApp.cpp` —— 全/半角回调由 `SetOption(0,"full_shape",newFull)` 改为 `m_handler->SetPunctuationWidth(!newFull, 0)`。
- `output\data\melt_eng.schema.yaml` —— `switches:` 段补 `ascii_punct`（**不写 `reset`** ⇒ 默认 index 0 = ¥ = 中文标点）。
- `output\data\radical_pinyin.schema.yaml` —— 新增 `switches:` 段（同上）。
- `test\TestQuickPanelDialog\TestQuickPanelDialog.cpp` —— T33a/T33b 墨迹盒改 25×27 / 19×25、T33k 改 8×13；
  新增 T33o（间隙）、T33p（x/y 不相交）、T33q（12 点/10 点）、T34a–T34d（`ShapeIsFullwidth` 四组合）。

文档（5 个）：`CHANGELOG.md`（`[Unreleased]` 新条目）、`.specify\specs\099-quickpanel-sogou-benchmark\tasks.md`（P0 追补项）、
`.specify\memory\lessons-learned.md`（**L197**）、`docs\adr\0040-panel-fullwidth-state-source-and-shift-punct-linkage.md`（新）、`docs\adr\README.md`（索引）。

**提交时必须排除**：`librime`（` m librime` 是子模块假脏）、`weasel.props`、`env.bat`、`*.obj`、`*.log`、`tmp\`。

## 五、下一阶段执行序（按此顺序，理由随附）

1. **用户人眼验收三件事**（本批唯一未闭环项）：
   a. 全角 / 半角两态图标是否清晰可辨（不再糊成一块）；
   b. 裸 Shift 切中英文后，设置栏的全/半角图标与「中/英」标志是否同步变化；
   c. 点一次设置栏「全/半角」切半角，再在中文态打逗号，是否变成英文逗号。
2. 若 a 不过（视觉）：**只改 `include\FluxingPanelFwIcon.h` 几何 + 对应 T33 断言** → 重跑单测 → 重打安装包 → 静默重装 → 复验。
   不要动状态源与联动逻辑（那部分已真机验证）。
3. 若 c 不过（点击路径）：先查 `WeaselServerApp.cpp` 的全/半角回调是否仍走 `SetPunctuationWidth`，
   再查目标方案是否声明 `ascii_punct`（不声明 ⇒ `set_option` 静默失败），最后才怀疑 `ascii_composer` / `punctuator`。
4. 收尾：`scripts\gate\run-all.ps1 -Level L0`；再进 spec 099 的 **P1 · 声明式槽位表**。
5. 另一条并行线：spec 100 云端账号与多端同步（见
   `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-fluxing-cloud-sync-plan.md`），**与本批互不依赖**。

## 六、证据路径（全部绝对路径）

脚本（在 `tmp\`，已被 `.gitignore` 排除）：

- `F:\soft\00selfmade\rime_claude\tmp\probe-fwicon-v9.ps1` —— 图标像素探针（间隙 / 墨迹盒）。
- `F:\soft\00selfmade\rime_claude\tmp\probe-ctrl-punct.ps1` —— 真机行为探针（`Ctrl+.` / `Shift+space` / 裸 Shift）。
- `F:\soft\00selfmade\rime_claude\tmp\probe-shift-punct.ps1` —— 早期探针（其 P4 失败已被隔离实验证伪为**时序假象**）。
- `F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd` —— 单测构建。
- `F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd` —— 全量构建 + NSIS。
- `F:\soft\00selfmade\rime_claude\tmp\build-batch-h2.cmd` —— 按正确顺序重编静态库 + 重链 TSF + makensis。
- `F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.cmd` —— **只重打安装包**（二进制没变时用它，比全量构建快）。
- `F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1` —— 静默重装 + 四二进制 SHA256 核对。
- `F:\soft\00selfmade\rime_claude\tmp\add-docs-batch-h.ps1`、`F:\soft\00selfmade\rime_claude\tmp\fix-lessons.ps1` —— 本轮文档写入与行尾修复。

日志：`tmp\qpanel-tests-v9.log`、`tmp\build-batch-h.log`、`tmp\build-batch-h2.log`、`tmp\gate-batch-h.log`、
`tmp\reinstall-batch-h.log`、`tmp\probe-ctrl-punct.txt`、`tmp\rebuild-installer.log`。

机器侧（真机）：`D:\Program Files\fluxing\weasel\data\melt_eng.schema.yaml`（装机后的 yaml）、
`D:\Program Files\fluxing\user1\fluxing\build\melt_eng.schema.yaml`（部署产物，已含 `ascii_punct`）。

可复用命令：

```powershell
# 单测（改 UI 后必跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-qpanel-tests.cmd" > 'F:\soft\00selfmade\rime_claude\tmp\qpanel-tests-v9.log' 2>&1
# ⇒ BUILD_EXIT=0；Dialog 252/252、Refactor 40/40

# L0 门禁
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\scripts\gate\L0-static.ps1" -Mode source -SkipNsisCompile -SkipGuardUnitTest
# ⇒ GATE_EXIT=0；checks=14 pass=12 fail=0 skip=2

# 全量构建 + NSIS（>100s，必须后台跑）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\build-batch-e.cmd" > "F:\soft\00selfmade\rime_claude\tmp\build-batch-h.log" 2>&1

# 只重打安装包（yaml 改动后用）
cmd /c "F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.cmd" > "F:\soft\00selfmade\rime_claude\tmp\rebuild-installer.log" 2>&1

# 静默重装 + 哈希核对
& pwsh -NoProfile -File "F:\soft\00selfmade\rime_claude\tmp\reinstall3-batch-e.ps1"
# ⇒ INSTALL_EXIT=0；四个二进制 MATCH=True；随后确认 WeaselServer 驻留

# 真机探针
& pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\tmp\probe-fwicon-v9.ps1'
& pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\tmp\probe-ctrl-punct.ps1'

# 锁屏判定（探针开工前必做）
if (Get-Process LogonUI -ErrorAction SilentlyContinue) { 'LOCKED' } else { 'UNLOCKED' }

# git（禁止先 cd；禁止 git add .）
git -C F:/soft/00selfmade/rime_claude status -sb
git -C F:/soft/00selfmade/rime_claude diff --stat
git -C F:/soft/00selfmade/rime_claude diff --numstat
```

## 七、本批决策（改之前先读这里，别推翻重来）

- **D1（ADR 0040）**：面板「全/半角」的状态源由裸 `full_shape` 换成 `ascii_punct`；显示口径
  `fullwidth = !ascii_mode && !ascii_punct`，下沉为 `fluxing::panel_status::ShapeIsFullwidth`（生产 + 单测同源）。
- **D2**：`ascii_punct = true` ⇔「半角 / 英文标点」（librime `punctuator` 命中即 `kNoop` ⇒ 标点原样 ASCII）。
- **D3**：裸 Shift 的标点联动**只作用于当前会话**，不广播、不写全局镜像（沿 ADR 0010 / 0012 的作用域约定）。
- **D4**：设置栏的「中/英」按钮**不**强制改 `ascii_punct`；只有裸 Shift 联动。
- **D5**：显示路径（100 ms 轮询的 provider）**零 `set_option`、零文件 I/O**（宪法 P2 / spec 099）。
- **D6**：`m_active_session` 会被 `FocusOut` 清零 ⇒ 新增「最近焦点会话」`m_last_focus_session` 回退（`FocusOut` 不清它）。
- **D7**：不改 `include\WeaselIPCData.h`、不改 IPC 协议、不改 `output\install.nsi`、不改 `librime\`、不改面板几何（215×32）。
- **D8**：yaml 的 `ascii_punct` **不写 `reset`** ⇒ 默认 index 0（¥ = 中文标点），与 `rime_ice.schema.yaml` 同口径；
  `reset: 1` 会把默认改成英文标点，**不要图省事写 reset**。
- **D9**：图标判据 = 两态各自两图元 **≥2 空列 + ≥2 空行**、x/y 皆不相交、墨迹盒保持**奇×奇**（避免补边偏心）。
- **D10**：**不改 `build.bat`** —— 已证 `WeaselTSF.vcxproj` 不链接 `RimeWithWeasel.lib`，
  本批改动全部落在服务端 `WeaselServer.exe`（先前「x64 TSF 静态库陈旧」的怀疑是红鲱鱼）。

## 八、需要用户到场的三件事

1. **人眼看全/半角两态图标**：是否还「糊成一块」？必要时截两态图。
2. **人眼看 Shift 后同步**：裸 Shift 切中英文，设置栏的全/半角图标与「中/英」标志是否都跟着变。
3. **手点一次设置栏「全/半角」**：中文态打逗号，看是否变成英文逗号（此路径点击不可自动化，只能手点）。

## 九、本批工具坑（重蹈覆辙会白烧一轮构建）

1. **编辑 yaml 必须保行尾**：`melt_eng.schema.yaml` 原为 LF，被整体写成 CRLF ⇒ **743 行假 diff**；
   修法 = `git checkout -- <file>` 后按**原行尾**精确重插，用 `git diff --numstat` 判据（本批最终 2/0 与 9/0）。
2. **PowerShell 里别对整篇 CRLF 文本做 `-replace "`n","`r`n"`** ⇒ 会产生 `\r\r\n`，整文件假 diff
   （本轮 `lessons-learned.md` 中招，14308 行假 diff；修法：按行 split/join，或只对新增块换行尾）。
3. **反引号是 PowerShell 的转义符**：含 ``` `` ``` 的模式在双引号串里会被吃掉一个反引号 ⇒ 用行级定位或单引号串。
4. **活动 Rime 用户目录不是 `%APPDATA%\Fluxing`**（那是 7/8 的旧目录），而是
   `D:\Program Files\fluxing\user1\fluxing\`；读错副本比不读更危险。
5. **装机数据目录是 `$INSTDIR\weasel\data`**：`install.nsi` 在 `StrCpy $INSTDIR "${WEASEL_ROOT}"` 之后才铺数据
   ⇒ **只改仓库的 `output\data\*.yaml` 不会到机器**，必须重打安装包 + 静默重装，再等部署产物 `build\*.yaml` 刷新。
6. **英文态标点天然是 ASCII** ⇒ 仅凭「英文态打出 `,`」不能证明 `ascii_punct` 生效；必须用中文态 `Ctrl+.` 隔离实验（lessons L197）。
7. **NSIS 的 `no files found` 警告是既有的**（`weaselARM64.dll` / `data\*.gram` 等），看 `NSIS_RC` 即可，别追。
8. **探针偶发不生效是时序假象**：`keybd_event` 组合键需 40 ms 按下间隔 + 700 ms 等待；失败先重跑再怀疑逻辑。
9. **`read` 读图片返回空 ⇒ 模型看不到截图**：视觉结论只能靠逐像素 ASCII / 数值 + 用户人眼。
10. **提交前必查**：`git status -sb` + `git diff --stat`，确认 ` m librime` 假脏没被带进去；禁止 `git add .`。

## 十、与批次 G 的关系 / 本批遗留

- v7（对角布局）、v8（句号收小 + 中文逗号重画）、v8.1（描边 1px）全部**维持**，见批次 G handoff §十；**v9 是本批新增**。
- 本批遗留观察项（未深查，不阻塞）：`WeaselTSF` 不链接 `RimeWithWeasel.lib` ⇒ 服务端与前端职责边界需在后续 ADR 里明确；
  TSF 状态栏图标不在本批范围。
- 本批提交信息：`fix(WeaselServer): 快捷面板全半角图标 v9 + 半角对齐英文标点 + 中英联动标点（批次 H）`；
  提交后以 `git log -1` 为准（不要引用本文里的任何哈希猜测）。
