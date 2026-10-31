# 089 · Plan — 快速面板默认常驻、状态回显、静止不透明度与菜单分级

> 配套 [spec.md](spec.md)（WHAT 层）与 [tasks.md](tasks.md)（执行层）。本文件是 HOW 层。

## 背景与取证

| 编号 | 取证结论 | 证据 |
| --- | --- | --- |
| E1 | 托盘菜单**已有**勾选态回显；语言栏菜单**没有** | `WeaselServer/WeaselTrayIcon.cpp:32` `CheckMenuItem(hMenu, id, MF_BYCOMMAND \| MF_CHECKED)` + `WeaselServer/WeaselServerApp.cpp:27-30` 注入 `SetMenuItemCheckedCallback`；`WeaselTSF/LanguageBar.cpp:186-189` 在 `TrackPopupMenuEx` 前**无**任何 `CheckMenuItem`。全仓 `CheckMenuItem` 只命中 `WeaselTrayIcon.cpp:32` 一处 ⇒ 用户报错点是**语言栏**右键菜单 |
| E2 | 常驻默认值当前是「关」 | `WeaselServer/QuickPanelDialog.cpp:1556-1559` `RestoreAlwaysShowMode()` → `s_mode = LoadPersistedAlwaysShowMode() ? kAlwaysShow : kHidden`；`:1533-1543` `LoadPersistedAlwaysShowModeFrom()` 读不到状态文件即 `return false` |
| E3 | 1.5 秒自动消失是裸字面量 | `WeaselServer/QuickPanelDialog.cpp:742-747`：`if (!AlwaysShowMode() && hit == -1 && !s_dragging && !inGrace) { s_outsideMs += 100; if (s_outsideMs >= 1500) { s_outsideMs = 0; Hide(); } }`；`1500` 无常量名。`:219`（头文件）另有 `kShowGraceMs = 2000` |
| E4 | 静止态不透明度不是均匀的 60% | `include/QuickPanelAlpha.h:41` `kAlphaFactorIdle = 153`（60%）；`QuickPanelDialog.cpp:1101-1149` `ApplyAlphaGradient()` 对背景像素（`r8>=130 && g8>=150 && b8>=180`）用 `ScaleAlpha(aPanel, factor)`，其中 `aPanel = kAlphaPanelTop + (kAlphaPanelBot-kAlphaPanelTop)*y/(H-1)`，`kAlphaPanelTop=220` / `kAlphaPanelBot=80`（`QuickPanelDialog.h:258-259`）⇒ 实测有效不透明度：前景 60%、背景顶 51.8%、背景底 18.8%。这就是用户说「过淡」且「不是整体一致」的原因 |
| E5 | 三套菜单语言的当前结构 | `WeaselTSF/WeaselTSF.rc`：ENU `IDR_MENU_POPUP:66-89`、HANT `IDR_MENU_POPUP_HANT:91-114`、HANS `IDR_MENU_POPUP_HANS:116-139`，三块都**有** `ID_WEASELTRAY_RERUN_SERVICE`（文案 `Restart (&E)` / `重啓算法服務 (&E)` / `重启算法服务 (&E)`）。`WeaselServer/WeaselServer.rc`：CHS `:157-180`、CHT `:251-274`、ENU `:345-368`，三块都**没有** `ID_WEASELTRAY_RERUN_SERVICE` |
| E6 | 服务端根本没有「重启服务」的处理器 | `WeaselServer/WeaselServerApp.cpp:295-385` `SetupMenuHandlers()` 注册了 QUIT / DEPLOY / RERUN / USERCONFIG / CHECKUPDATE / INSTALLDIR / RESTORE_IGNORED / QUICK_PANEL / QUICK_PANEL_SHOW / ALWAYS_SHOW，**唯独没有** `ID_WEASELTRAY_RERUN_SERVICE`；未注册命令走兜底，最终落到部署器而非服务重启 |
| E7 | 语言栏的「重启」实际只是「再启动一次」 | `WeaselTSF/LanguageBar.cpp:331-388` `_HandleLangBarMenuSelect()`，`ID_WEASELTRAY_RERUN_SERVICE` 与 `ID_WEASELTRAY_INSTALLDIR` 共用一个分支：`LooksLikeWeaselRoot` 检查 → `WeaselServiceLaunchTarget(root)` → detached `std::thread` + `ShellExecuteW(NULL, L"open", target, NULL, root, SW_HIDE)`。`include/FluxingInstallPaths.h:183-193` 显示该 target 优先是 `start_service.bat`，而 `output/start_service.bat` 只有 `cd "%~dp0"` + `start "" WeaselServer.exe` ⇒ **不停旧进程**。`WeaselServer.cpp:145-163` 单实例逻辑在 `retry>=10` 时 `return 0` ⇒「起第二个实例顶掉第一个」会把服务静默弄没，该路线禁用 |
| E8 | 退出没有确认框 | `WeaselServer/WeaselServerApp.cpp:297-298`：`AddMenuHandler(ID_WEASELTRAY_QUIT, [this] { return m_server.Stop() == 0; })` |
| E9 | 两个 `.rc` 都是 UTF-16LE + BOM | `WeaselTSF/WeaselTSF.rc` 与 `WeaselServer/WeaselServer.rc` 首字节均为 `FF FE`。旧交接记录里「`.rc` 是 Shift-JIS」的说法**作废** |
| E10 | 测试工程能直接吃到 `include/` 下的新头文件 | `test/TestQuickPanelDialog.vcxproj:52,72` 的 `AdditionalIncludeDirectories` 含 `$(SolutionDir)\include`，`ClCompile` 已含 `..\..\WeaselServer\QuickPanelDialog.cpp` ⇒ 新增 header-only 纯函数**零 vcxproj 改动**即可被测 |
| E11 | 现有 alpha 断言会被 217 打破，必须同步 | `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp:243-244`：`ScaleAlpha(255, kAlphaFactorIdle) == 153`、`ScaleAlpha(220, kAlphaFactorIdle) == 132` |
| E12 | 默认常驻语义改动会打破「缺文件 ⇒ 关」断言，必须同步 | `TestQuickPanelDialog.cpp` T13a–T13h（`:289-325`）断言 `RestoreAlwaysShowMode()` / `LoadPersistedAlwaysShowModeFrom()` 在文件缺失时给出 `false` |

## 技术方案

### 089-1 常驻默认值改为「开」（三态语义，不动旧 API 的返回值契约）

新增 header-only 共享单元 `include/FluxingQuickPanelState.h`（`namespace fluxing`），只放**可脱机
测试的纯函数**：

- `ParseAlwaysShowLine(const std::wstring& line, bool* out_value) -> bool`：从状态文件的一行文本解析
  布尔值，沿用现有 `always_show=1/0` 格式的容错规则（前后空白、大小写、`true/false`）。
- `ResolveAlwaysShowOnStartup(bool found, bool value) -> bool`：**唯一的默认值判定点**，
  `found ? value : true`。把「文件缺失 ⇒ 常驻」收敛成一个可测函数。
- `QuickPanelStateFilePath(const std::wstring& user_data_dir) -> std::wstring`
- `ReadAlwaysShowStateFile(const std::wstring& path, bool* out_value) -> bool`
- `ReadAlwaysShowStateWithLegacy(primary, legacy, bool* out_found, bool* out_value) -> bool`：
  primary = 用户数据目录下的状态文件；legacy = 旧版漫游目录下的状态文件（保持 1.1.x 迁移行为）。
  两者都只做「存在则读」，**绝不创建目录**。

`WeaselServer/QuickPanelDialog.*` 的改法（**关键：不破坏既有 API 契约**）：

- 新增三态 API `LoadPersistedAlwaysShowState(bool* out_found, bool* out_value)` 与
  `LoadPersistedAlwaysShowStateFrom(const wchar_t* path, bool* out_found, bool* out_value)`，
  内部复用 `ReadAlwaysShowStateFile` / `ParseAlwaysShowLine`。
- **保留** `LoadPersistedAlwaysShowMode()` / `LoadPersistedAlwaysShowModeFrom()` 的现有语义
  （缺失 ⇒ `false`），改成三态 API 的薄封装 `found ? value : false`。这样 T13a–T13h / T16a / T16b
  的既有断言全部不动，回归面收到最小（E12）。
- `RestoreAlwaysShowMode()` 改走三态 + `ResolveAlwaysShowOnStartup`：
  `s_mode = fluxing::ResolveAlwaysShowOnStartup(found, value) ? Mode::kAlwaysShow : Mode::kHidden;`
- `SetAlwaysShowMode()`（`:1565-1577`）的同值早退与写盘行为**不变**：早退时不写盘是可接受的
  —— 默认值就是「开」，用户从未改动过便无需落盘；一旦用户改动，值必然与当前不同，会走写盘分支。
- `QuickPanelDialog.h` 新增 `static constexpr DWORD kAutoHideMs = 1500;`，
  `QuickPanelDialog.cpp:744` 的裸 `1500` 换成该常量（FR-003）。

### 089-2 语言栏菜单勾选态回显

`WeaselTSF/LanguageBar.cpp` 的 `OnClick()`（`:154-195`）在 `TrackPopupMenuEx`（`:187-189`）**之前**插入：

1. 用 `include/FluxingUserPaths.h` 的 `ResolveFluxingUserDataPath()` 求用户数据目录，拼出 primary
   状态文件路径；legacy 路径用 `GetEnvironmentVariableW(L"APPDATA")` 拼（**不用** `SHGetFolderPathW`，
   避免给 `WeaselTSF` 引入 `shell32` 依赖）。
2. `ReadAlwaysShowStateWithLegacy(...)` → `ResolveAlwaysShowOnStartup(found, value)` 得到勾选态。
3. `CheckMenuItem(popupMenu, ID_QUICKPANEL_ALWAYS_SHOW, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED))`。

托盘侧不动（E1 已证其回显正常）。默认值规则与 089-1 共用同一个 `ResolveAlwaysShowOnStartup`，
保证「两处菜单 + 面板实际行为」三方同源（FR-004）。

**P2 合规论证**（FR-016）：语言栏 `ITfLangBarItemButton::OnClick` **不属于** `ITfTextInputProcessor::*`
回调族，不在 P2 禁令范围内；即便如此，这里读的也只是一个几十字节的本地文本文件，且只在用户主动
右键时触发一次，不做轮询、不做网络、不做注册表遍历。该论证写入 ADR 0022。

### 089-3 静止态整块 85% 不透明

按 §24.3.2 原文执行「**背景基数拉平 + 因子 217**」两处同改：

- `include/QuickPanelAlpha.h:41`：`kAlphaFactorIdle` 由 `153` 改为 `217`（= round(255 × 0.85)）。
- `include/QuickPanelAlpha.h`：新增 `constexpr int kAlphaPanelBase = 255;`
  （静止态面板底色的基数；255 表示不再对底色做额外的上下渐变）。
- `WeaselServer/QuickPanelDialog.h`：补 `#include <QuickPanelAlpha.h>`（当前 `:34-39` 的 include 列表
  **没有**它，只有 `<functional> <string> <windows.h> <FluxingUserPaths.h>`），把
  `kAlphaPanelTop = 220` / `kAlphaPanelBot = 80` 双双改为 `fluxing::kAlphaPanelBase`。

`ApplyAlphaGradient()` 算法本体、悬停渐变循环、`kAlphaFactorFull = 255`、`kAlphaFadeStep = 24`
全部不动 ⇒ FR-007 满足。改完后底色与前景都走 `ScaleAlpha(x, 217)`，整块均匀 85%（FR-006）。
**视觉代价**：静止态上下渐变被有意牺牲（这正是用户要的「整块一致」），在 CHANGELOG 与 ADR 披露。

测试同步（E11）：`TestQuickPanelDialog.cpp` T12d/T12e 期望值改为
`ScaleAlpha(255, kAlphaFactorIdle) == 217`、`ScaleAlpha(220, kAlphaFactorIdle) == 187`，并新增
① 底色基数断言 `kAlphaPanelTop == kAlphaPanelBot == fluxing::kAlphaPanelBase`；
② 「背景顶/中/底三处有效不透明度均为 85%」的公式复算断言；③ `kAutoHideMs == 1500`。

### 089-4 菜单分级：「高级(&V)」子菜单

`WeaselTSF/WeaselTSF.rc` 三块 + `WeaselServer/WeaselServer.rc` 三块，统一改成：

```
POPUP "高级(&V)"        // ENU: POPUP "Advanced (&V)"   HANT: POPUP "進階(&V)"
{
    MENUITEM "重新部署(&D)",        ID_WEASELTRAY_DEPLOY
    MENUITEM "重启输入法服务(&E)",  ID_WEASELTRAY_RERUN_SERVICE
    MENUITEM SEPARATOR
    MENUITEM "退出输入法服务(&Q)",  ID_WEASELTRAY_QUIT
}
```

- 托盘三块**新增** `ID_WEASELTRAY_RERUN_SERVICE` 项（E5：原本没有）。
- 「重启算法服务」文案统一改为「重启输入法服务」（简体）/「重啓輸入法服務」（繁体）/
  `Restart Input Method Service (&E)`（英文）。
- 「退出算法服务（会停掉输入法）」改为「退出输入法服务(&Q)」，后果说明移进 US6 的确认框正文
  （菜单项文案过长会被系统截断）。
- 命令 ID 全部沿用 `include/resource.h` 既有值，**不新增 ID**（`ID_WEASELTRAY_RERUN_SERVICE = 40015`）。
- `WeaselTSF/WeaselTSF.rc:15-16` 的 `#ifndef APSTUDIO_INVOKED` + `#include <../include/resource.h>` 不动。
- 改完必须 `git diff --numstat` 复核两个 `.rc` 仍是 UTF-16LE + BOM、行尾未被改写（E9）。

### 089-5 可靠的服务重启（唯一执行体放在服务端）

**服务端**（`WeaselServer/WeaselServerApp.cpp` `SetupMenuHandlers()`）新增
`ID_WEASELTRAY_RERUN_SERVICE` 处理器：

1. IPC 线程收到命令 → `PostMessage` 弹回 UI 线程（复用 `ID_WEASELTRAY_QUICK_PANEL:348-359` 的既有
   模式，`m_uiThreadId` 已在 `WeaselServerApp.h:122` 备好），避免在 IPC 线程上做进程/文件操作。
2. UI 线程做前置检查（FR-012）：当前进程是否以管理员权限运行、`ResolveFluxingWeaselRoot()` 是否可信
   （`LooksLikeWeaselRoot`）、`stop_service.bat` 与 `start_service.bat` 是否都存在。任一不满足 ⇒
   简体中文 `MessageBoxW` 说明原因并**直接返回**，绝不先停后失败。
3. 检查通过 ⇒ 在 `%TEMP%` 写一次性 helper 批处理，`ShellExecuteW(NULL, L"open", L"cmd.exe",
   L"/c \"<tempbat>\"", NULL, SW_HIDE)` 起一个**脱离服务进程生命周期**的执行链（FR-011）：
   - `ping -n 3 127.0.0.1 > nul`（约 2 秒延迟，让菜单消息循环先返回）
   - `call "<root>\stop_service.bat"`
   - `for /L` 轮询 `tasklist /FI "IMAGENAME eq WeaselServer.exe"`，用**退出码/查找计数**判断进程是否
     消失，上限约 15 秒（不解析本地化文本）
   - `start "" "<root>\start_service.bat"`
   - 超时未退出 ⇒ 仍尝试启动，并写一行诊断到 `%TEMP%\fluxing-rerun-service.log`
4. 明确**不采用**「起第二个实例顶掉第一个」（E7）。

**语言栏**（`WeaselTSF/LanguageBar.cpp:331-388`）把 `ID_WEASELTRAY_RERUN_SERVICE` 从「与 INSTALLDIR
共用的本地 ShellExecute 分支」里摘出来，改为 `m_client.TrayCommand(wID)`（即 `default:` 分支既有做法）
⇒ 两条菜单路径共用服务端唯一执行体（FR-013），消除两份事实源。摘出后该分支只剩
`ID_WEASELTRAY_INSTALLDIR`，`LooksLikeWeaselRoot` 检查与中文提示保留。语言栏侧另起 detached 线程
轮询服务是否恢复（用 `include/WeaselClient.h` 的 `weasel::Client` 探活，超时约 20 秒），超时弹
简体中文提示；探活不阻塞 UI。

**托盘**：`WeaselTrayIcon` 的命令派发已走 `SetupMenuHandlers()`，新增处理器后自动生效。

### 089-6 退出确认框

`WeaselServer/WeaselServerApp.cpp:297-298` 的 QUIT 处理器改为：先 `PostMessage` 弹回 UI 线程
（若当前不在 UI 线程），在 UI 线程弹简体中文确认框：

> 退出后输入法将不可用，需要重新登录或重启电脑才能恢复。确定要退出输入法服务吗？

选择「是」才 `return m_server.Stop() == 0;`。确认框落在**服务端唯一处理器**里，语言栏路径
（`_HandleLangBarMenuSelect` 的 `default:` → `TrayCommand`）与托盘路径自动一致且只弹一次（FR-014）。

### 版本线与产物（由父任务统一执行，不属本 spec 的并行写者）

- `build.bat:44` 与 `xbuild.bat:55` 的 `VERSION_PATCH` 由 `4` 改为 `5`。
- `CHANGELOG.md` 新增 `[1.1.5]` 节；新增 ADR `docs/adr/0022-*.md`；交接卡新增 §25。

## Constitution Check

| 原则 | 判定 | 依据 |
| --- | --- | --- |
| I. Intent Before Implementation | ✅ | 4 条用户真机反馈原文 + §24 根因分析已在 spec.md 的 Intent 复述；E1–E12 全部是源码行级取证，无推测项。「三个开发向项收进高级子菜单」「重启做成真重启」已由用户在 §24 计划第 4/5 条拍板 |
| II. Test-Backed Change | ✅ | 新增 `include/FluxingQuickPanelState.h` 的纯函数全部在 `TestQuickPanelDialog` 内脱机断言（默认值三态、行解析、legacy 回退）；alpha 改动同步改 T12d/T12e 并新增底色基数与「背景顶/中/底 == 85%」断言；`kAutoHideMs` 常量入断言。E10 证明新 header 零 vcxproj 改动即可被测 |
| III. Spec-Artifact Discipline | ✅ | 三件套齐备；spec.md 全文无技术栈词（R2）；tasks.md 每条带 P1/P2/P3 与 `[USx]`（R3）；架构决策另立 ADR 0022，不埋在提交信息里（CLAUDE.md §3 分流） |
| IV. Structured Clarification | ✅ | 无待澄清项。「取消这个选项」的歧义已由 §24 与用户反馈原文锁定为「取消默认勾选语义」而非「删掉菜单项」；透明度「整块一致 vs 只提图标」已由 §24.3.2 锁定为前者 |
| V. Incremental Delivery | ✅ | 拆成 6 个可独立验收的垂直切片（默认常驻 / 勾选回显 / 不透明度 / 菜单分级 / 服务重启 / 退出确认），每片有自己的验收命令；1.1.5 一次发布但每片可单独回滚 |

### P 规则逐条

| 规则 | 判定 | 依据 |
| --- | --- | --- |
| P1 单一事实源 | ✅ | 常驻默认值规则收敛到 `ResolveAlwaysShowOnStartup()` 一个函数，面板与语言栏菜单共用；服务重启逻辑收敛到服务端一个处理器，语言栏改为转发，消除 E7 的第二份实现 |
| P2 TSF 不做阻塞 I/O | ✅ | `ITfTextInputProcessor::*` 回调链路零改动。语言栏 `OnClick` 不属该接口族，且只读一个几十字节本地文件（论证入 ADR 0022）；服务重启动作全部 `PostMessage` 弹到 UI 线程后执行 |
| P3 不碰 librime | ✅ | 本 spec 零 `librime/` 改动 |
| P4 提交规范 | ✅ | 中文提交信息 + `WeaselServer \| WeaselTSF \| docs \| fluxing` 模块前缀，走 `tmp/commit-N.txt` + `git commit -F` |
| P5 用户可见字符串简体中文 | ✅ | 新增确认框、失败提示全部简体中文；菜单三语言块同步本地化（FR-009 / FR-015） |
| P6 门禁 | ✅ | `build.bat weasel installer` + `scripts/test-infra/run-test-suite.bat` 双门禁；两个 `.rc` 改完 `git diff --numstat` 复核编码 |
| P7 状态外置 | ✅ | 构建/测试日志落 `tmp/build-115.log`、`tmp/suite-115.log`，对话里只留结论 |
| P8 披露 | ✅ | 见下方三问 |

### P8 必披露项

1. **是否触及旧用户数据路径？** 是，但**只读不写**。语言栏菜单读 primary（用户数据目录）与 legacy
   （旧版漫游目录）两处状态文件以求勾选态；写入路径不变，仍只写 primary。`%APPDATA%\Rime` 依然
   MUST NOT 被创建 —— legacy 路径沿用既有 `LegacyStatePath()`（`QuickPanelDialog.cpp:1476-1481`）的
   语义，且只做「存在则读」，绝不 `CreateDirectory`。
2. **是否引入新的进程间通信消息类型？** 否（FR-017）。勾选态走状态文件读回；重启服务走
   `WeaselTrayIcon` 既有命令派发通道（`ID_WEASELTRAY_RERUN_SERVICE = 40015` 早已存在于
   `include/resource.h`）；退出确认走服务端处理器内部。⇒ 不触发 IPC 四边同步。
3. **CHANGELOG 条目**：`[1.1.5]` 六条 —— 快速面板默认常驻（首次使用无需手动勾选）；语言栏状态菜单
   回显常驻勾选态；面板静止态整块统一 85% 不透明度（**牺牲静止态上下渐变**）；三个开发向菜单项
   收进「高级」子菜单并统一「重启输入法服务」文案；「重启输入法服务」改为真正的停-等-起、失败可见、
   托盘菜单新增该项；「退出输入法服务」新增简体中文确认框。

## 风险与回滚

| 风险 | 概率 | 缓解 | 回滚 |
| --- | --- | --- | --- |
| 默认常驻被误读为「开机自动弹面板」 | 中 | spec.md US1 与 FR-001 明确区分「常驻」= 打开后不自动消失；spec 055 的「不自动弹出」不变量不动；CHANGELOG 措辞避开「开机显示」 | 无需回滚，属文档口径 |
| 改 `kAlphaPanelTop/Bot` 打到悬停渐变 | 低 | 只改基数常量，`ApplyAlphaGradient` 算法与 `kAlphaFactorFull`/`kAlphaFadeStep` 不动；T12a–T12n / T14a–T14f 全量跑 | 单独 revert `QuickPanelDialog.h` 两个常量 |
| helper 批处理在部分机器不生效（`tasklist` 输出本地化、`cmd` 策略） | 中 | 轮询用 `tasklist /FI` + `find /c` 的**计数/退出码**而非解析文本；超时后仍执行启动步骤，保证不会「停了起不来」；失败提示明确指向手工执行 `stop_service.bat` / `start_service.bat` | 语言栏恢复本地 `ShellExecuteW` 分支（单文件 revert） |
| 托盘新增 RERUN_SERVICE 项后兜底路径变化 | 低 | 处理器注册与 `.rc` 改动同一次提交；`run-test-suite.bat` 的 21 工程构建门禁覆盖资源编译 | revert `WeaselServer.rc` + `WeaselServerApp.cpp` |
| `.rc` 被写成 UTF-8 | 低 | 只用字节级写入（`[Text.Encoding]::Unicode`），改完复核首字节 `FF FE` 与 `git diff --numstat` | `git checkout -- <rc>` |
| 退出确认框打扰高级用户 | 低 | 用户明确要求（US6）；确认框只在点击菜单项时出现，不影响 `weaselserver.exe /q` 命令行路径 | 单点 revert QUIT 处理器 |

## 复杂度跟踪

无超出既有架构的新增复杂度。新增一个 header-only 纯函数单元（`include/FluxingQuickPanelState.h`）
与一个服务端菜单处理器；净**减少**一份重复实现（语言栏的服务重启分支被删除，改为转发）。