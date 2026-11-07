# Handoff 2026-11-07（批次 E）：导入窗口取消 bug + 切换输入法英文窗口 bug + 快捷面板三项 UI 调整

- 交棒会话：`mvs_dafd1ca094964ea0b534a97f9e439dd8`（右键菜单统一 + 品牌图标入口批次）
- 接手会话：**新建**（用户明确要求新开会话执行本批任务）
- 分支 `Fluxing`，HEAD = `e438fa1`，已推送 `kizemo/Fluxing`；工作区仅剩 ` m librime` 假脏
- 装机版本：`weaselx64.1.2.1.0.1.dll`（1,987,072 B）+ `WeaselServer.exe`（2,328,576 B），目录 `D:\Program Files\fluxing\weasel\`；旧版 `.0.0` dll 已由用户手动删除（Agent 备份 `F:\soft\00selfmade\rime_claude\tmp\backup\weaselx64.1.2.1.0.0.dll.bak`，SHA256 `C708E86C…8BFDC`）

---

## 一、背景脉络（三段话）

1. **Fluxing 1.2.1**（火流猩输入法，Weasel fork）已发布；`40022 导入搜狗用户词库` 功能（ADR 0037）实现在 WeaselDeployer：`/sogou` 命令行入口 → `Configurator.cpp` 里 MaintenanceGuard 包裹 `SogouImportDialog::DoModal()`。用户已真机执行导入并确认成功。
2. 上一批（ADR 0038，commit `d991005`）：上下文菜单唯一化到 `WeaselServer.exe` 的 `IDR_MENU_POPUP`(105)，并把右键入口转移到快捷设置栏**品牌图标**（`QuickPanelDialog` 的 `WM_RBUTTONDOWN` → 回调 → `ShowPopupMenuAt`）；TSF 语言栏右键菜单与三套 rc 资源已删。真机程序化验收通过（`#32768` 18 项含 40022，截图证据齐全）。
3. 用户 09:45 复验后给出 7 项指令（下文 §二），其中 1/2 已完成，3–7 交新会话执行。

## 二、用户指令原文与状态

1. ✅ 已手动删除 `weaselx64.1.2.1.0.0.dll`。
2. ✅ 右键菜单转移到快捷设置栏品牌图标 —— 查看成功。
3. 🐞 **导入窗口点「取消」无法关闭，右上角 X 可以**（导入本身成功）。要求查 bug 原因。
4. 🐞 **切换到火流猩后一小段时间只能输入英文、无法切中文**；切换数次后恢复。要求查是否存在 bug。
5. 🎨 品牌图标**上移 1px**（当前压住面板下边线）；「中」「半」**文字键也适当上移**（当前未垂直居中）。
6. ✨ 「半」键改用**符号/图标**：两个图标分别代表全角/半角符号，显示当前状态，点击切换。
7. 🎨 布局：「中」「全/半角」两个按钮**移到「切换输入法」之后、「用户短语」之前**。

## 三、本会话（交棒前）已完成的取证 —— 直接采信，勿重做

### Bug #3 根因【已坐实，证据级】

- 对话框模板 `WeaselDeployer/WeaselDeployer.rc` 第 **494** 行（UTF-16LE）：
  `PUSHBUTTON "Cancel",IDCANCEL,320,186,60,16` —— 取消按钮 id 就是 `IDCANCEL`。
- 消息映射 `WeaselDeployer/SogouImportDialog.h:30-36` 只注册了
  `WM_INITDIALOG / WM_CLOSE / IDC_SOGOU_IMPORT_BTN / LBN_SELCHANGE / CBN_SELCHANGE`，
  **没有 `COMMAND_ID_HANDLER(IDCANCEL, …)`** ⇒ 点「取消」（含按 Esc）发出的 `WM_COMMAND/IDCANCEL` 无人处理 ⇒ 不 `EndDialog`，窗口不关；而右上角 X 走 `WM_CLOSE` → `OnClose` → `EndDialog(IDCANCEL)`（`SogouImportDialog.cpp:227-230`），所以 X 能关。症状完全吻合。
- **修复方案**（对照正确样例 `WeaselDeployer/HotkeyEditorDialog.h:32`）：
  在 `SogouImportDialog.h` 消息映射加 `COMMAND_ID_HANDLER(IDCANCEL, OnCancel)`，`.cpp` 实现 `EndDialog(IDCANCEL); return 0;`。注意导入进行中（`OnImport` 同步执行时对话框本就不重入）无需特殊处理，但**不得**绕过 `Configurator.cpp:214-227` 的 MaintenanceGuard（`import_user_dict` 必须在 Start/EndMaintenance 之间，ADR 0037）。
- **顺带排查同类**：`DictManagementDialog.h` 消息映射同样只见 `WM_CLOSE` 未见 `IDCANCEL` handler —— 检查其模板是否有 Cancel 按钮；有则同修。
- **顺带修 P2 违规（constitution 必须简体中文）**：`IDD_SOGOU_IMPORT` 模板内 `Cancel`、`Import (&I)`、`Sogou user dictionary source:`、`Import into user dictionary:` 全是英文硬编码（rc 第 488-494 行）；`IDS_STR_SOGOU_*` 字符串资源倒是中文。改 rc 注意该文件是 **UTF-16LE + BOM + CRLF**，编辑必须走 `[Text.Encoding]::Unicode` 读写。

### UI #5/#6/#7 的代码落点【已定位，行为级事实】

- 槽位派发：`WeaselServer/QuickPanelDialog.cpp` `DispatchSlotClick`（cpp:1028 起），HEAD 注释（cpp:1023-1026）称
  `1 Phrase→用户短语, 2 Settings→快捷键设置, 3 Account→关于, 4 中英文切换, 5 全半角切换`。
- 文字键绘制：绘制 `switch(i)` 的 `case 4/case 5`（cpp:1844-1869，「中」「英」「全」「半」单字，复用 `s_hFontStatus`）；cpp:1871-1875 注明右侧竖排回显已取消、`StatusRect` 恒 false。
- 全/半角状态源：`s_fullwidth` + `RefreshInputState()`（cpp:2230-2237，provider 只读内存零 I/O）；切换回调 `s_onFullwidth`（槽 5，cpp:1064-1068）——**#6 的点击切换逻辑现成，只需把「全」「半」文字换成两个图标**。
- 品牌矩形单一事实源：`BrandRectForSize`（cpp:439-446）/ `BrandRect()`，绘制（cpp:1520/1526 附近）与 `HitTest`(-2) 逐像素同源；逻辑值 `(kPanelPadding=5, 5, kBrandSize=30, 30)`，面板 `239×36`、`kPanelVPadding=3`、槽步进 33px。**#5 上移 1px 只改同一常量 → T30 测试仍过**；若只挪绘制不挪 BrandRect 会被 T30 抓住。
- ⚠️ **槽位顺序存在两份互相矛盾的二手描述**：上面 HEAD 代码注释 vs 并发会话（v4 批次）报告的「0 中英,1 中日,2 符号,3 用户短语,4 切换输入法,5 末槽」。**新会话开工第一步：以 HEAD 源码为准（`SlotRectForLayout` + 绘制 switch + `DispatchSlotClick` 三处对表），并截一张运行时面板图，把 6 个可见按钮从左到右的实际内容列成表**，再确定 #7 目标顺序「…切换输入法 → 中 → 全/半角图标 → 用户短语…」（「中」按钮与「切换输入法」是否为同一按钮需在表里澄清；若判不动，用一次问卷让用户指认，别猜着改）。
- 测试影响面：`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp`（**LF + UTF-8 BOM**；本批次 T30 = brand 命中不变量；并发会话的槽位断言用 `kButtonCount-1`）+ `test/TestQuickPanelRefactor/`。改槽位序/图标必须同步改测试，判据「测试先行且先红后绿」。

### Bug #4 取证状态【未复现，只能静态排查】

当前已不复现（用户切换数次后恢复），无运行时证据。时间线线索：机器 04:00 定时关机 → 07:50 冷启动 → 安装 `.0.1` shim 后重启过 explorer → 09:45 用户报告「切换后短暂英文」。候选方向（按优先级）：
1. **冷启动/部署窗口**：WeaselServer 启动时 librime 首次 setup/用户词库同步（刚导入过搜狗词库，`synchronizer` 可能要重建快照）期间，TSF 拿不到 IPC session 或 rime 拒绝输入 → 英文直通。查 `WeaselTSF` Activate→IPC 建立时序（`WeaselTSF/TextStore.cpp`、`WeaselServer/WeaselServer.cpp`）+ 日志时间戳。
2. **ascii_mode 初始态**：新 context 的 `ascii_mode` 默认值 / `ascii_composer` 首次按键行为（查 `output/data/weasel.yaml` 与 `RimeWithWeasel/RimeWithWeasel.cpp`）。**改任何 yaml 前必读 `F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md` §5–§7、§9（本机现状）、§10（常见坑）**。
3. 复现手段：程序化反复切换 IME 并立即探测中文输入是否生效（注意**先判锁屏** `Get-Process LogonUI`；合成输入须前台锁处理，见 §五 探针坑），并留日志时间窗。定位不到根因前**不许改代码**（systematic-debugging）。

## 四、下阶段计划（建议执行序）

1. **Bug #3**：按上文修复（含同类对话框排查 + rc 中文化），补一条防回归手段（WTL 消息映射缺 IDCANCEL 无法单测 ⇒ 至少写一条静态检查脚本扫 `WeaselDeployer/*.h` 模板含 IDCANCEL 按钮但消息映射无 handler 的对；脚本自己要用坏数据验它有牙）。真机验收 = 打开导入窗 → 点取消 → 窗口关闭且 deployer 进程正常退出（维护态解除：随后中文输入正常）。
2. **UI #5/#6/#7** 一个批次做（同文件 `QuickPanelDialog.{h,cpp}`）：先对表六槽现状 → 测试先行改槽位序 + 1px 上移 + 全/半角双图标（图标绘制**不要走 GDI `AlphaBlend`**——v4 有前科：半透明合成会把 logo 边缘渲染成黑框，见 handoff-2026-11-07-quickpanel-v4-redesign；简单图形直接 GDI 矢量绘制或预合成 BitBlt）。真机验收：150% DPI 截图 + 像素级检查（放大品牌区确认不再压下边线、文字垂直居中、图标随全/半角状态切换、点击生效）。
3. **Bug #4**：按 §三 路径排查；若确认为「启动部署窗口」类固有行为，向用户解释 + 评估可否改善（如部署完成前保留上次 ascii_mode / 加快首次 session 建立），结论写 ADR 或 lessons-learned。
4. 构建打包（模板抄 `tmp/build-tsf-menu.cmd` 的 `set /p < env.bat` 段 + `msbuild /p:PlatformToolset=v143 /p:SolutionDir=F:\soft\00selfmade\rime_claude\`）→ 单测 → `output\archives\fluxing-1.2.1-installer.exe` → 清 `HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel`、`HKCU\Software\Fluxing` → 静默装 `/S /D=D:\Program Files\fluxing` → `Stop-Process explorer -Force` 重启 → 真机验收。版本号 `env.bat` `WEASEL_BUILD` +1（现为 1，本次应 2）。
5. 收尾：CHANGELOG、（若动 IPC/架构才要）ADR、本批 handoff + 接力 prompt、`git status -sb` + `git diff --stat` 后按 Conventional Commits 提交；**push 前先问用户**。

## 五、已验证 / 推断 / 未验证

| 级别 | 条目 |
|---|---|
| 已验证（代码/模板级证据） | #3 根因（IDCANCEL 无 handler，X 走 WM_CLOSE）；#5/#6/#7 代码落点与现成切换回调；装机 1.2.1.0.1 与旧 dll 删除完成；菜单 18 项含 40022（真机探针） |
| 推断（未证） | #4 与冷启动部署窗口相关（时间线吻合，无日志实证）；#3 修复后「Esc 也关窗」将一并恢复 |
| 未验证 | 六槽当前真实可见顺序（两份二手描述冲突）；「切换输入法」按钮与「中」按钮是否同一；DictManagementDialog 是否同类取消 bug；rc 英文区是否还有 IDD_SOGOU_IMPORT 重复模板（改时全文搜 `IDD_SOGOU_IMPORT` 确认只有一份模板体） |

## 六、证据文件路径（全部绝对）

- `F:\soft\00selfmade\rime_claude\tmp\probe-qpanel-menu.log` / `.png` / `.bmp` —— 品牌图标右键真机证据（18 项含 40022 + 同屏截图）
- `F:\soft\00selfmade\rime_claude\tmp\verify-menu-unique.log` —— 资源级菜单唯一化核对
- `F:\soft\00selfmade\rime_claude\tmp\probe-tsfbefore.txt` / `probe-tsfafter.txt` —— CLSID → `.1.0.1` dll 指向对照
- `F:\soft\00selfmade\rime_claude\tmp\installer-silent.log`、`build-tsf-menu.log`、`build-test-qpanel.log`
- `F:\soft\00selfmade\rime_claude\tmp\backup\weaselx64.1.2.1.0.0.dll.bak`
- 复用脚本：`tmp\probe-qpanel-menu.ps1`（品牌右键探针）、`tmp\close-menu2.ps1`（Escape 关残留菜单）、`tmp\verify-menu-unique.ps1`、`tmp\build-tsf-menu.cmd` / `tmp\build-test-qpanel.cmd`（构建模板）

## 七、前序文档（背景必读，全部绝对路径）

- 进站五件套：`F:\soft\00selfmade\rime_claude\CLAUDE.md`、`AGENTS.md`、`CONTEXT.md`、`.specify\memory\constitution.md`、`.specify\memory\project-knowledge.md`（§A/§11）+ `docs\agents\domain.md`
- 上一批 handoff（含完整工具坑实录）：`F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-07-context-menu-unification-brand-icon.md`
- 面板 v4 重构 handoff（#34 铁律「黑框=AlphaBlend」实勘出处；其接力 prompt 不要改写）：`docs\archive\handoffs\handoff-2026-11-07-quickpanel-v4-redesign.md`
- 搜狗导入背景：`docs\adr\0037-sogou-user-dictionary-import.md`、`docs\archive\handoffs\handoff-2026-11-06-fluxing-121-release-and-sogou-import.md`
- 菜单唯一化决策：`docs\adr\0038-single-context-menu-brand-icon-entry.md`
- yaml/快捷键改动前必读：`F:\soft\00selfmade\rime_claude\Rime-设置说明书-重点快捷键.md`

## 八、工具链坑速查（本会话实测，详版在上一批 handoff §五）

- `D:\Program Files\` 受保护：Agent 删不了（`rm` 与 `mavis-trash` 均被策略拒），需删除/覆盖时交用户手动；备份可用 `Copy-Item -LiteralPath … -Destination <工作区> -Force`（必须单独成条命令）。
- 禁 `Remove-Item`；删工作区文件用 `E:\Users\Duanyi\.minimax\.minimax\bin\mavis-trash.cmd -- <绝对路径>`（文件级；目录级会报「路径无效」需逐子项）。
- UI 自动化探针：先 `Get-Process LogonUI` 判锁屏；跨进程右键须 `AttachThreadInput`+`SetForegroundWindow(目标)`；`#32768` 要 10ms 轮询；菜单内容用 `SendMessage(h, MN_GETHMENU=0x01E1)`；关菜单用 `PostMessage` Esc（`EndMenu` 跨线程无效）；非 DPI 感知进程坐标是逻辑值，抓屏乘 `GetDpiForWindow/96`（本机 144dpi=×1.5）；多屏抓屏用 `CreateDC("DISPLAY")`。
- `WeaselTSF.rc`/`WeaselDeployer.rc` = UTF-16LE+BOM+CRLF，PowerShell 读写必须 `-Encoding Unicode`；ripgrep 对 UTF-16 无效，用 `Select-String`。`output/install.nsi` = UTF-8+BOM+CRLF。
- `msbuild` 必须 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\` 否则报假缺依赖；构建 >100s，改 UI 一次改对、写盘前自检（占位符清零、注释别插进未闭合括号）。
- PowerShell `Report` 型函数别用 `[void]()` 调用（吞输出）；子进程 stderr 用 `2>&1` 捕获。

## 九、文件所有权与协调

- `WeaselServer/QuickPanelDialog.{h,cpp}`、`include/FluxingPanelLogo.h`：v4 会话（`mvs_f89666e92862467586aec0ec3dccbf5a`）已声明交还，HEAD 提交为 `f458d84/a40a766/a04cc41/00dad0b`；本批可安全接手。开工前 `git status -sb` 确认除 ` m librime` 外干净。
- 若期间用户再开面板相关会话，先 `mavis session list` 查活跃同文件会话并打招呼。
- `librime/` 子模块不得提交；`WeaselIPCData.h` 不得擅改。
