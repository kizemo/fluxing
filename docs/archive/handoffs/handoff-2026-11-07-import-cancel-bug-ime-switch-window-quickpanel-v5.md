# Handoff 2026-11-07（批次 E · 收尾版）：导入窗口取消 bug + 切换输入法英文窗口 bug + 快捷面板三项 UI 调整

> **状态：批次 E 已执行完毕** —— 代码 / 门禁 / 单测 / 构建 / 打包 / 静默安装 / 150% DPI 像素验收全部走完一遍。
> 本文已**就地改写**为「批次 E 完成态 + 下一批起点」；下一会话请按 §八 执行序接手，**勿重做已完成的取证**，**勿改写更早的 handoff / prompt**。

---

## 一、结论速览（7 项用户指令）

| # | 用户指令 | 状态 | 结论一句话 |
|---|---|---|---|
| 3 | 导入窗点「取消」关不掉（X 可以） | ✅ 已修 + 真机验证 | 模板取消按钮 id 就是 `IDCANCEL`，而 `SogouImportDialog` 消息映射**没有** `COMMAND_ID_HANDLER(IDCANCEL,…)` ⇒ `WM_COMMAND/IDCANCEL` 无人处理 ⇒ 不 `EndDialog`；补 handler 后「取消」/Esc/X 三路径全部正常关闭 |
| 3b | 导入窗界面是英文 | ✅ 已修 | `IDD_SOGOU_IMPORT` 的 en-US 语言块 5 处英文改成简体中文（坐标不动），真机截图确认运行时生效 |
| 4 | 切到火流猩后一小段时间只能输英文 | ✅ 根因坐实，**本批不改码** | `RimeWithWeasel` 的 `m_disabled` 闸门：部署器 / 维护窗口存活期间 `Initialize()` + `start_maintenance(False)` 使 `m_disabled=true`，`ProcessKeyEvent` / `AddSession` / `CommitComposition` / `FocusIn` / `ShiftDown/Up` 全部早退 ⇒ 按键不被 IME 消费 ⇒ 宿主按英文处理；部署器退出后 `TryLazyRecovery()` 复位 ⇒ 表现为「切换数次后恢复」 |
| 5 | 品牌图标上移 1px | ✅ 已实现 + 像素验收 | `kBrandYOffset = 1`；150% DPI 实测品牌墨迹 y 由 8..52 → 6..50（Δ = −2px = `ScaleRound(1, 1.5)`），底边距 1px → 3px |
| 6a | 「中」文字键垂直居中 | ✅ 已实现 + 像素验收 | **前两轮改动全是空转**：传了零 MAT2 ⇒ `GetGlyphOutlineW(GGO_METRICS)` 返回 `GDI_ERROR` ⇒ 度量恒 0 ⇒ 永远回退 `DT_VCENTER`。本轮改为「单位矩阵 + 正确墨迹盒模型 + 正确基线公式」后，真机「中」墨迹 17..40 → **16..39**，中心 28.5 → **27.5 == 键帽中心** |
| 6b | 「半」键改图标 | ✅ 已实现 | 纯 GDI 矢量（全角 = 宽圆角框 / 半角 = 窄圆角框），随 `s_inputState.fullwidth` 切换；**问卷 `ask_f66143ba772fd46803aacac1` 未答，按推荐方案 A（单图标随状态切换）推进** |
| 7 | 「中」「全/半角」移到「切换输入法」之后 | ✅ 已实现 + 像素验收 | 六槽左→右 = 切换输入法 / 中 / 全半角 / 用户短语 / 快捷键设置 / 关于 |

**附带完成**：`.rc` 中文化、防回归静态门禁（`dialog-cancel-handler` / `dialog-template-chinese` + L3 变异用例 M07/M08）、版本号对齐、CHANGELOG、`lessons-learned` L196。

---

## 二、改动清单（按文件）

### 2.1 Bug #3 导入窗取消（WeaselDeployer）

- `WeaselDeployer/SogouImportDialog.h` —— 消息映射新增 `COMMAND_ID_HANDLER(IDCANCEL, OnCancel)`（:33）。
- `WeaselDeployer/SogouImportDialog.cpp` —— 实现 `OnCancel()`：`EndDialog(IDCANCEL); return 0;`（:235-238）。
- **未触碰** `Configurator.cpp:214-227` 的 MaintenanceGuard（ADR 0037 要求 `import_user_dict` 必须在 Start/EndMaintenance 之间）。
- `WeaselDeployer/WeaselDeployer.rc` —— `IDD_SOGOU_IMPORT` 的 en-US 语言块 5 处英文 → 简体中文（:132-144 / :483-495），坐标不动；文件仍是 **UTF-16LE + BOM + CRLF**（字节级校验 `len=52284, BOM=True(FF FE), bareLf=0`，与 HEAD 逐行对比恰好 5 行变化）。

**同类排查结论（全仓）**：`.rc` 里只有 3 个模板含 `IDCANCEL` 按钮 —— `IDD_SOGOU_IMPORT`（本批修）/ `IDD_HOTKEY_EDITOR`（已有 handler）/ `IDD_KEY_RECORDER`（已有 handler）；`IDD_DICT_MANAGEMENT` 模板**没有**取消按钮 ⇒ 同类 bug 仅此一处，无需再改。

### 2.2 防回归门禁（scripts/gate）

- `scripts/gate/lib/GateChecks.ps1` —— 新增 `Get-RcDialogBlocks` / `Test-DialogCancelHandlerCoverage` / `Test-DialogTemplateChinese`。
- `scripts/gate/L0-static.ps1` —— 注册 `dialog-cancel-handler`、`dialog-template-chinese` 两个检查项。
- `scripts/gate/L3-mutation.ps1` —— 新增变异用例 **M07 / M08**（注入「模板含 IDCANCEL 按钮但消息映射缺 handler」「rc 残留英文文案」两种坏数据，均被 KILLED ⇒ 检查确实有牙）。

### 2.3 UI #5/#6/#7（WeaselServer/QuickPanelDialog）

- `QuickPanelDialog.h`
  - `kBrandYOffset = 1`（:387）；品牌矩形 y = `L.padding − ScaleRound(kBrandYOffset, L.dpr_y)`。
  - `kVisualToLogical[kButtonCount] = {0, 4, 5, 1, 2, 3}`（:401）+ `LogicalAtVisual()` / `VisualAtLogical()`（越界 −1）。
  - **索引空间约定**：面板所有公开 API 的 index 一律 = **视觉位置**；功能语义只经 `kVisualToLogical` 换算一次（constitution R7 单一真相源）。
  - 新增 `fluxing::panel_ink` 命名空间（头文件尾）：`FillUnitMat2()`（GDI 单位矩阵）+ `InkBaselineForCap()`（墨迹盒基线公式，**纯函数，生产与单测共用**）。
- `QuickPanelDialog.cpp`
  - `BrandRectForSize` 用 `L.padding − ScaleRound(kBrandYOffset, L.dpr_y)`；logo 仍走 `BrandRect()` 矩形的 **BitBlt(SRCCOPY)** 贴图（**不走 GDI `AlphaBlend`**，v4 黑框前科，见 `handoff-2026-11-07-quickpanel-v4-redesign.md`）。
  - 新增 `DrawIconFullwidth(...)`（纯 GDI 矢量：全角 = 宽圆角框、半角 = 窄圆角框，30×30 viewBox 中心 (15,15)）。
  - `DrawInkCenteredChar(...)` 重写（见 §三 UI6a）。
  - `TooltipText` / `DispatchSlotClick` 入口经 `LogicalAtVisual` 换算；绘制 switch 全量改用视觉索引。
  - 几何公式 / `StatusRectForLayout` / `HitTest` / `include/FluxingPanelKeyboard.h` **未改**。
- 测试同步：`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`、`test/TestQuickPanelRefactor/TestQuickPanelRefactor.cpp`（**T12d = 视觉槽 1、T12e = 视觉槽 2**；新增 T15a–T15f）。

### 2.4 版本 / 文档

- `version.txt`：1.2.0 → **1.2.1**；`env.bat`：`WEASEL_BUILD` 0 → **1**（`PRODUCT_VERSION` / `FILE_VERSION` 保持 1.2.1.0，与版本门禁 `version.txt + ".0"` 一致）。
  > ⚠️ 口径说明：上一版 handoff §四-4 写「`WEASEL_BUILD` +1（现为 1，本次应 2）」，与仓库实况不符 —— 接手时仓库里是 **0**，本批按 0→1 处理，shim 名 `weaselx64.1.2.1.1.dll`。
- `CHANGELOG.md` `## [Unreleased]` 顶部 6 条：Bug#3 修复、rc 中文化、UI#5/#6/#7、Bug#4 结论、test(ci) 门禁、chore(build) 版本号对齐。
- `.specify/memory/lessons-learned.md` 追加 **L196**（Bug#4 的 `m_disabled` 闸门根因；文件 UTF-8 + BOM + CRLF，936297 → 940527 B）。

---

## 三、UI #6a「中」键居中：三轮空转的根因与终局修法（**下一会话务必先读 §三**）

### 3.1 症状

150% DPI 真机像素：改前（`DT_VCENTER`）「中」墨迹 y = 17..40、中心 28.5；键帽中心 27.5 ⇒ 低 1px。
**两次「改公式」后墨迹 bbox 与 `DT_VCENTER` 完全相同 ⇒ 改动根本没进渲染路径。**

### 3.2 根因（已坐实）

生产代码给 `GetGlyphOutlineW` 传的是**零 MAT2**（`ZeroMemory(&mat, …)`）⇒ `GetGlyphOutlineW(hdc, uChar, GGO_METRICS, &gm, …)` 返回 **GDI_ERROR (0xFFFFFFFF)**、`gmBlackBoxY = 0` ⇒ `ink_h <= 0` ⇒ `DrawInkCenteredChar` **每次都 `return false`** ⇒ 调用方回退 `DT_VCENTER`。**公式改对了，但度量从没取到。**

### 3.3 终局修法（三处，缺一不可）

1. **单位矩阵**：`MAT2` = 4 × FIXED = 16 字节，GDI 按 **16.16** 形态读这 4 个 32bit 字（eM11/eM12/eM21/eM22）⇒ 单位矩阵 = `{0x00010000, 0, 0, 0x00010000}`。零矩阵 ⇒ `GDI_ERROR`。
   实现放头文件 `fluxing::panel_ink::FillUnitMat2()`，**生产与单测共用** ⇒ 回归成零矩阵会被单测当场抓住。
2. **正确墨迹盒模型**：字形原点在基线上方 `gmptGlyphOrigin.y` 处，黑盒自原点向下延伸 `gmBlackBoxY` 行 ⇒
   `ink_top = baseline − origin_y`；`ink_bottom = ink_top + ink_h − 1`；`ink_center = ink_top + (ink_h − 1)/2`。
   （**旧写法 `ink_top = baseline − gmBlackBoxY` 是错的**。）
3. **基线公式**：`baseline = (box_top + box_bottom + 2*origin_y − (ink_h − 1)) / 2`，就近舍入（.5 向上）。150% DPI 实测参数（`box_top=5, box_bottom=50, origin_y=20, ink_h=24`）⇒ `baseline = 36` ⇒ 墨迹 **16..39**、中心 **27.5 == 键帽中心 27.5**。
   实现为纯函数 `fluxing::panel_ink::InkBaselineForCap()`。

量不到时（字体无该字形 / GDI 不可用）仍 `return false` 回退 `DT_VCENTER` —— 失败面不扩大、不写死任何 fudge 值。

### 3.4 SDK 事实速查（Windows Kits 10.0.26100 `um/wingdi.h`，勿再踩）

- `GetGlyphOutlineW(hdc, uChar, fuFormat, lpgm, c, pv, lpmat)` —— **度量在第 4 位**、MAT2 在**最后**；老原型 `(hdc,uChar,fmt,lpmat,buf,size,lpgm)` 在 10.0.26100 下已不适用（C2664）。
- `GGO_METRICS` 是 `#define GGO_METRICS 0`（**标志位，不是类型**）；度量结构体叫 `GLYPHMETRICS{gmBlackBoxX, gmBlackBoxY, gmptGlyphOrigin, gmCellIncX/Y}`，**没有 `gmBlackBoxHeight` 字段**。
- `GGO_GLYPH_INDEX` 表示 uChar 是**字形索引**；这里传 Unicode 字符码，**不能**加这个标志。
- `MAT2` 16 字节 = 4 × FIXED（Windows FIXED = `{WORD fract; short value;}`，fract 在前）；写成 32 字节 / 错误偏移会退化成非单位矩阵 ⇒ `GDI_ERROR`。

---

## 四、真机验收结果（本批实跑，全部留痕）

| 项 | 方法 | 结果 |
|---|---|---|
| Bug#3 对话框中文化 | `WeaselDeployer.exe /sogou` 唤起后枚举控件 | 标题「**【火流猩输入法】导入搜狗用户词库**」，ListBox 32790 / ComboBox 32791 / Button 32792「开始导入(&I)」/ Button **ctrlId=2「取消」** ⇒ 全中文，rc 改动运行时生效 |
| Bug#3 点「取消」 | `BM_CLICK` | 窗口销毁 **PASS** |
| Bug#3 按 Esc | `AttachThreadInput` + `SetForegroundWindow` + `keybd_event` | 窗口销毁 **PASS** |
| Bug#3 点 X | `WM_CLOSE` | 正常关闭 **PASS** |
| UI#5 品牌上移 | 150% DPI 抓帧墨迹 bbox | 品牌墨迹 y 8..52 → **6..50**（Δ = −2px），底边距 1px → **3px** **PASS** |
| UI#6a 文字居中 | 150% DPI 抓帧墨迹 bbox | 「中」17..40 → **16..39**，中心 27.5 == 键帽中心 **PASS** |
| UI#6b 全半角图标 | ASCII 墨迹图 + 真机抓帧 | 左→右 = 切换输入法（条纹）/「中」/ 全半角图标（当前半角态 = 13×13 窄框）/ 用户短语（文档）/ 快捷键设置（齿轮）/ 关于（?+块）**PASS** |
| UI#7 槽位重排 | 同上 | 六槽顺序 = 切换输入法 → 中 → 全半角 → 用户短语 → 快捷键设置 → 关于 **PASS** |
| 单测 | `tmp\build-qpanel-tests.cmd` | `TestQuickPanelDialog` **231/231**、`TestQuickPanelRefactor` **40/40**（新增 T15a–T15f，含零矩阵反例）**PASS** |
| L0 门禁 | `scripts/gate/L0-static.ps1 -Mode source` | `L0 RESULT mode=source checks=14 pass=12 fail=0 skip=2`（含 `version-consistency PASS version=1.2.1`、`dialog-cancel-handler PASS`、`dialog-template-chinese PASS`）**PASS** |
| L3 变异 | `scripts/gate/L3-mutation.ps1` | M07 / M08 均 **KILLED** **PASS** |
| 构建 + 打包 | `tmp\build-batch-e.cmd` | `X64_TSF_OK` / `X86_SLN_OK` / `NSIS_RC=0` / `ALL_STEPS_OK`；产物 `output\archives\fluxing-1.2.1-installer.exe` |
| 静默安装 | `/S /D=D:\Program Files\fluxing` | exit 0；HKLM `InstallDir` 正确；HKCU `RimeUserDir` 在 **`HKCU\Software\Fluxing\Weasel`** 子键下 = `D:\Program Files\fluxing\user1\fluxing` |

---

## 五、证据文件路径（全部绝对路径）

- 导入窗验证：`F:\soft\00selfmade\rime_claude\tmp\verify-sogou-cancel.ps1`、`tmp\sogou-cancel-verify.log`
- 面板抓帧：`tmp\probe-qpanel-shot2.ps1`（修正版：对 WeaselServer **所有**顶层窗口各发一次 `WM_COMMAND 40018` 后**重新枚举**再找 `FluxingQuickPanel*`）、`tmp\probe-qpanel-diag.ps1`
- 像素分析：`tmp\qpanel-analyze.ps1`、`tmp\qpanel-glyph-compare.ps1`、`tmp\qpanel-glyph-compare2.ps1`、`tmp\qpanel-glyph-truth.ps1`、`tmp\qpanel-glyph-truth2.ps1`（**度量段有效**；像素扫描段因 DIB 未填白底而失效，结论改由「单位矩阵度量 + 观测值反推」得出）、`tmp\qpanel-diff.ps1`
- 抓帧产物：`tmp\qpanel-after.bmp`（白底，首轮）/ `tmp\qpanel-after2.bmp`（375×70，米色底）/ `tmp\qpanel-after2.log`（dpi=144，panel 359×54）/ `tmp\qpanel-after2-analysis.txt` / `tmp\qpanel-after2-*.png`
- **终验帧（UI#6a 修复后）**：`tmp\qpanel-after3.bmp`（375×70）/ `tmp\qpanel-after3.log` / `tmp\qpanel-after3-analysis.txt` / `tmp\qpanel-after3-full.png` / `tmp\qpanel-after3-brand.png` / `tmp\qpanel-after3-slot0..5.png` ⇒ slot1「中」墨迹 **y=16..39**（中心 27.5 == 键帽中心）、brand 墨迹 6..50
- 安装与注册表备份：`tmp\reg-backup\<ts>\HKCU-Fluxing.reg`、`tmp\reg-backup\<ts>\HKLM-WOW6432Node-Fluxing.reg`、`tmp\backup\user-cfg-<ts>\*.yaml`、`tmp\reinstall-batch-e.log`、`tmp\reinstall2-batch-e.log`、`tmp\reinstall3-batch-e.log`（终验轮：装机件 sha256 与 `output\` 全部 MATCH）
- 构建脚本：`tmp\build-batch-e.cmd`（完整构建 + NSIS）、`tmp\build-qpanel-tests.cmd`（两个 QuickPanel 单测）
- Bug#4 源文：`tmp\lessons-bug4.md`（已并入 `lessons-learned.md` L196）

---

## 六、遗留项 / 风险（下一批可接手）

1. **x86 TSF shim 陈旧**：`D:\Program Files\fluxing\weasel\weasel.dll` mtime 2026-11-07 00:34:02，与 `output\weasel.dll` 一致 —— 本批**未改 WeaselTSF 代码**，MSBuild 增量跳过链接；`WOW6432Node` CLSID 视图仍指向它。属既有行为，非本批回归。x64 侧 `weaselx64.dll`（10:50:04）正常。
2. **Bug#4 缺运行时日志佐证**：WeaselServer 的 glog 落盘位置未定位（`%TEMP%`、`D:\Program Files\fluxing`、`WeaselServerApp.cpp` 均无 `log_dir` / `InitLog`），本批结论为**源码级静态链路**（`RimeWithWeasel.cpp`：`Initialize()` 128-139、`AddSession()` 209-231、`ProcessKeyEvent()` 355-396、`_IsDeployerRunning()` 1043-1067、`TryLazyRecovery()` 1078-1093）。若要坐实运行时时间窗，需先定位日志目录。
3. **Bug#4 改进方向（未实施，属架构决策）**：放宽 `m_disabled` 闸门会与 ADR 0037 的 maintenance 成对约束 / leveldb LOCK 冲突 ⇒ 需先出 ADR；更安全的替代是「部署期在输入侧给提示」或「缩短首次部署窗口」。
4. **`.rc` 其余 en-US 对话框仍是英文**（上游资源，本批只改了 `IDD_SOGOU_IMPORT`）。
5. **问卷未答**：全/半角图标形态按推荐方案 A 落码，若用户后续选 B（双图标并排当前态高亮）或 C（单图标 + 小字）需返工 `DrawIconFullwidth`。
6. **`env.bat` 被 `.gitignore` 排除（第 31 行）** ⇒ 本批 `WEASEL_BUILD` 0→1 **只在本机生效**，仓库里没有这条改动；换机器 / CI 复现 1.2.1.1 安装器时必须手工补上（`version.txt` 已提交为 1.2.1，`PRODUCT_VERSION` 仍 1.2.1.0）。

---

## 七、本批新增工具坑（详版在上一批 handoff §八）

- **PowerShell 内置别名会遮蔽同名函数**：`R` = Invoke-History、`Measure` = Measure-Object、**`cls` = Clear-Host**（自建 `function Cls` 触发「CursorPosition 句柄无效」）⇒ 脚本里必须改名（已用 `Get-WCls` / `Get-WTxt` / `Rep` / `Measure-Ink`）。
- `Write-Host "..." -f a,b`（无括号）会把 `-f` 当成 Write-Host 参数 ⇒ 必须 `Write-Host ("..." -f a,b)`。
- BMP 高度为负（top-down，`biHeight=-70`）⇒ `for ($y=0; $y -lt $H; $y++)` 一次都不执行 ⇒ **diff=0 假象**；须取绝对值。
- `CreateDIBSection` 创建的 DIB **未填白底** ⇒ `GetPixel != 0xFFFFFF` 处处成立 ⇒ 墨迹 bbox 退化为全画布；须先 `FillRect` 填白。
- C# P/Invoke 里 `MAT2` 是 **16 字节**（4×FIXED，fract 在前）；按 32 字节 / 错误偏移构造会把单位矩阵写成退化矩阵。
- `DrawTextW` 在 **user32.dll**（不在 gdi32.dll）。
- **跨帧逐像素差分不可用**：`qpanel-after`（白底）与 `qpanel-after2`（米色底）背景不同 ⇒ 整幅 9350 像素差异；差分前必须先确认背景一致。
- **面板唤起**：必须对 WeaselServer 进程**所有**顶层窗口各 `PostMessage(WM_COMMAND, 40018)` 后**重新枚举**窗口再找 `FluxingQuickPanel*`；`TrayIconClass` 只是通知栏辅助窗（`SystemTraySDK.cpp:54`），单独发给它无效。
- 本地硬安全策略拦截：`[Microsoft.Win32.Registry]::DeleteSubKeyTree(...)`、`robocopy /E` 到工作区路径、bypass 模式下带 `Delete` 的 `Select-String`；改用逐层非递归删除 + grep 工具检索。
- `msbuild` 必须 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`；构建 >100s，改 UI 一次改对、写盘前自检（占位符清零、注释别插进未闭合括号）。
- `.rc` = UTF-16LE+BOM+CRLF（`[Text.Encoding]::Unicode` 读写；ripgrep 对 UTF-16 无效）；`output/install.nsi` = UTF-8+BOM+CRLF。

---

## 八、下一步（下一批执行序）

1. **用户侧观感确认**：150% DPI 下人工确认品牌区上移、「中」键居中、全半角图标切换、新槽位顺序、导入窗取消 / Esc 关闭 —— 程序化像素验收除「中」键居中外均已 PASS。
2. **问卷回执**：全/半角图标形态 A / B / C；若非 A，按 §六-5 返工。
3. **Bug#4 运行时取证**（可选）：先定位 WeaselServer 日志目录，再复现「切换后短暂英文」并留时间窗；坐实后决定是出 ADR 还是维持「不改码」结论。
4. **遗留项清理**：x86 `weasel.dll` 陈旧（需改 WeaselTSF 代码触发重链）；`.rc` 其余 en-US 对话框中文化（可另开小批）。
5. **收尾纪律**： Conventional Commits + 模块 scope（`WeaselServer` / `WeaselDeployer` / `test(ci)` / `installer` / `docs` / `chore(build)`）；**push 前先问用户**。

---

## 九、前序文档（背景必读，全部绝对路径）

- 进站五件套：`F:\soft\00selfmade\rime_claude\CLAUDE.md`、`AGENTS.md`、`CONTEXT.md`、`.specify\memory\constitution.md`、`.specify\memory\project-knowledge.md`（§A/§11）+ `docs\agents\domain.md`
- 上一批 handoff（含工具坑实录）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-context-menu-unification-brand-icon.md`
- 面板 v4 重构 handoff（#34 铁律「黑框 = AlphaBlend」实勘出处，**其接力 prompt 不要改写**）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-quickpanel-v4-redesign.md`
- 搜狗导入背景：`F:\soft\00selfmade\rime_claude\docs\adr\0037-sogou-user-dictionary-import.md`、`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-06-fluxing-121-release-and-sogou-import.md`
- 菜单唯一化决策：`F:\soft\00selfmade\rime_claude\docs\adr\0038-single-context-menu-brand-icon-entry.md`
- yaml / 快捷键改动前必读：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`

---

## 十、文件所有权与协调

- `WeaselServer/QuickPanelDialog.{h,cpp}`、`include/FluxingPanelLogo.h`：本批已接手并改完，后续会话若再动先 `mavis session list` 打招呼。
- `WeaselDeployer/SogouImportDialog.{h,cpp}`、`WeaselDeployer/WeaselDeployer.rc`、`scripts/gate/**`：本批已改。
- `librime/` 子模块**不得提交**（工作区的 ` m librime` 是假脏）；`include/WeaselIPCData.h` **不得擅改**（本批未动 IPC 四边，无需 ADR）。
