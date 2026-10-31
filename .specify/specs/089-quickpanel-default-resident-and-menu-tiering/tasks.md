# 089 · Tasks — 快速面板默认常驻、状态回显、静止不透明度与菜单分级

> 单一垂直切片。优先级 P1/P2/P3，用户故事标签 [US1] [US2] [US3] [US4] [US5] [US6]。
> 状态标记：[ ] 未开始 · [~] 进行中 · [x] 完成。

## T1 — 抽出常驻状态的纯函数单元 [P1] [US1] [US2]

- **输入**：`WeaselServer/QuickPanelDialog.cpp:1472-1559`（状态文件路径、读写、三处 Load/Restore）、
  `include/FluxingUserPaths.h`、`test/TestQuickPanelDialog/TestQuickPanelDialog.cpp:258-345`（T13/T14 既有断言）。
- **输出**：新文件 `include/FluxingQuickPanelState.h`（header-only，`namespace fluxing`，无 Windows 依赖的
  纯函数 + 一个只做「存在则读」的文件读取函数）。
- **做什么**：实现 `ParseAlwaysShowLine` / `ResolveAlwaysShowOnStartup` / `QuickPanelStateFilePath` /
  `ReadAlwaysShowStateFile` / `ReadAlwaysShowStateWithLegacy`。`ResolveAlwaysShowOnStartup(found, value)`
  返回 `found ? value : true`。文件读取只 `open` 不 `create`，绝不建目录。
- **验收**：`TestQuickPanelDialog` 新增断言组全部通过 ——
  `ResolveAlwaysShowOnStartup(false, false) == true`、`(false, true) == true`、
  `(true, false) == false`、`(true, true) == true`；`ParseAlwaysShowLine` 对
  `always_show=1` / `always_show=0` / 带空白 / 非法行 的判定符合预期；不存在的文件 ⇒ `found == false`。

## T2 — 面板默认常驻 + 1.5 秒常量 [P1] [US1]

- **输入**：T1 的 header；`QuickPanelDialog.cpp:1533-1559`、`:742-747`；`QuickPanelDialog.h:205-219`。
- **输出**：`WeaselServer/QuickPanelDialog.h` / `.cpp` 修改。
- **做什么**：① 头文件新增 `static constexpr DWORD kAutoHideMs = 1500;`，`.cpp:744` 的裸 `1500`
  换成该常量；② 新增三态 API `LoadPersistedAlwaysShowState` / `LoadPersistedAlwaysShowStateFrom`；
  ③ 既有 `LoadPersistedAlwaysShowMode*` 改为三态的薄封装（`found ? value : false`，**契约不变**）；
  ④ `RestoreAlwaysShowMode()` 改用 `ResolveAlwaysShowOnStartup`；⑤ `SetAlwaysShowMode()` 不动。
- **验收**：`TestQuickPanelDialog` 中 T13a–T13h / T16a / T16b 原断言**不改**且仍通过；新增断言
  「状态文件缺失时 `RestoreAlwaysShowMode()` 后 `AlwaysShowMode() == true`」、
  「状态文件写 0 后 `RestoreAlwaysShowMode()` 后 `AlwaysShowMode() == false`」、
  「`kAutoHideMs == 1500`」全部通过。

## T3 — 语言栏菜单回显勾选态 [P1] [US2]

- **输入**：T1 的 header；`WeaselTSF/LanguageBar.cpp:1-16`（include 区）、`:154-195`（`OnClick`）；
  `include/FluxingUserPaths.h`；`include/resource.h:39`（`ID_QUICKPANEL_ALWAYS_SHOW = 40019`）。
- **输出**：`WeaselTSF/LanguageBar.cpp` 修改。
- **做什么**：在 `TrackPopupMenuEx` 之前求 primary/legacy 状态文件路径（legacy 用
  `GetEnvironmentVariableW(L"APPDATA")`，不引入 `shell32`），读回并 `ResolveAlwaysShowOnStartup`，
  然后 `CheckMenuItem(popupMenu, ID_QUICKPANEL_ALWAYS_SHOW, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED))`。
- **验收**：`build.bat weasel` 通过；真机按 §24.5 第 2 条验收（两处菜单勾选态一致）；
  `WeaselTSF` 的链接依赖未新增 `shell32.lib`（`git diff` 复核）。

## T4 — 静止态整块 85% 不透明 [P1] [US3]

- **输入**：`include/QuickPanelAlpha.h:29-96`；`QuickPanelDialog.h:34-39,258-259`；
  `QuickPanelDialog.cpp:718-750,1101-1149,1343-1385`；`TestQuickPanelDialog.cpp:228-283`。
- **输出**：`include/QuickPanelAlpha.h`、`WeaselServer/QuickPanelDialog.h`、
  `test/TestQuickPanelDialog/TestQuickPanelDialog.cpp` 修改。
- **做什么**：① `kAlphaFactorIdle` 153 → 217；② 新增 `constexpr int kAlphaPanelBase = 255;`；
  ③ `QuickPanelDialog.h` 补 `#include <QuickPanelAlpha.h>`，`kAlphaPanelTop`/`kAlphaPanelBot`
  双双改为 `fluxing::kAlphaPanelBase`；④ `ApplyAlphaGradient` 算法本体、`kAlphaFactorFull`、
  `kAlphaFadeStep`、悬停渐变循环**不动**；⑤ 同步 T12d/T12e 期望值（153→217、132→187）。
- **验收**：`TestQuickPanelDialog` 通过，含新增断言 `kAlphaPanelTop == kAlphaPanelBot == kAlphaPanelBase`、
  `ScaleAlpha(255, kAlphaFactorIdle) == 217`、`ScaleAlpha(220, kAlphaFactorIdle) == 187`、
  「背景顶/中/底三处有效不透明度按 `ApplyAlphaGradient` 公式复算均为 217（85%）」；
  T12a–T12n / T14a–T14f 既有断言不回归。

## T5 — 「高级」子菜单（两个 .rc × 三语言） [P2] [US4]

- **输入**：`WeaselTSF/WeaselTSF.rc:66-139`（ENU/HANT/HANS 三块，UTF-16LE+BOM）；
  `WeaselServer/WeaselServer.rc:157-180 / 251-274 / 345-368`（CHS/CHT/ENU 三块，UTF-16LE+BOM）；
  `include/resource.h:15-39`。
- **输出**：两个 `.rc` 修改（编码与行尾必须原样保持）。
- **做什么**：六块菜单各自把「重新部署」「重启输入法服务」「退出输入法服务」移进新建
  `POPUP "高级(&V)"`（ENU `Advanced (&V)`、HANT `進階(&V)`）；托盘三块**新增**
  `ID_WEASELTRAY_RERUN_SERVICE`；「重启算法服务」文案统一改为「重启输入法服务」/
  「重啓輸入法服務」/ `Restart Input Method Service (&E)`；「退出算法服务（会停掉输入法）」改为
  「退出输入法服务(&Q)」。命令 ID 一律沿用既有值，不新增 ID。
- **验收**：`build.bat weasel` 通过（资源编译无误）；`git diff --numstat` 显示两个 `.rc` 仍为整体
  改写而非编码翻转，PowerShell 复核首字节仍是 `FF FE`；真机按 §24.5 第 5 条验收。

## T6 — 服务端「重启输入法服务」处理器 [P1] [US5]

- **输入**：`WeaselServer/WeaselServerApp.cpp:1-11,295-385`；`WeaselServer/WeaselServerApp.h:23-59,112-122`；
  `include/FluxingInstallPaths.h:183-213`；`output/start_service.bat`、`output/stop_service.bat`。
- **输出**：`WeaselServer/WeaselServerApp.cpp`（必要时 `.h` 增一个私有辅助函数声明）修改。
- **做什么**：注册 `ID_WEASELTRAY_RERUN_SERVICE` 处理器：IPC 线程 → `PostMessage` 弹 UI 线程；
  UI 线程做前置检查（管理员权限 / `LooksLikeWeaselRoot` / `stop_service.bat` 与 `start_service.bat`
  均存在），任一失败弹简体中文 `MessageBoxW` 并返回；通过则在 `%TEMP%` 写一次性 helper 批处理
  （延迟约 2 秒 → `call stop_service.bat` → 轮询 `tasklist /FI "IMAGENAME eq WeaselServer.exe"`
  至进程消失，上限约 15 秒 → `start "" start_service.bat`；诊断写
  `%TEMP%\fluxing-rerun-service.log`），用 `ShellExecuteW(cmd.exe /c ...)`、`SW_HIDE` 脱离服务进程
  生命周期。**禁止**「起第二个实例顶掉第一个」。
- **验收**：真机按 §24.5 第 4 条验收（服务 PID 变化、输入法数秒内恢复）；把 `stop_service.bat`
  改名后点击 ⇒ 弹简体中文失败提示且服务仍在运行。

## T7 — 语言栏重启改为转发 + 恢复探活 [P1] [US5]

- **输入**：`WeaselTSF/LanguageBar.cpp:331-388`；`include/WeaselClient.h`（`weasel::Client` 的
  `Connect()` 探活能力）。
- **输出**：`WeaselTSF/LanguageBar.cpp` 修改。
- **做什么**：把 `ID_WEASELTRAY_RERUN_SERVICE` 从「与 `ID_WEASELTRAY_INSTALLDIR` 共用的本地
  `ShellExecuteW` 分支」摘出，改走 `m_client.TrayCommand(wID)`（与 `default:` 分支一致）；
  另起 detached 线程用 `weasel::Client::Connect()` 轮询服务恢复，超时约 20 秒弹简体中文提示；
  探活线程不得阻塞 UI、不得在 `ITfTextInputProcessor::*` 回调内同步等待。
- **验收**：`build.bat weasel` 通过；真机从语言栏点「重启输入法服务」⇒ 服务 PID 变化且与托盘路径
  行为一致；`git diff` 显示 `LooksLikeWeaselRoot` 检查仍保留在 INSTALLDIR 分支。

## T8 — 退出确认框 [P2] [US6]

- **输入**：`WeaselServer/WeaselServerApp.cpp:297-298`（QUIT 处理器）、`:348-359`（`PostMessage`
  弹 UI 线程的既有模式）、`WeaselServerApp.h:122`（`m_uiThreadId`）。
- **输出**：`WeaselServer/WeaselServerApp.cpp` 修改。
- **做什么**：QUIT 处理器改为先确保在 UI 线程（否则 `PostMessage` 弹回），再弹简体中文确认框
  「退出后输入法将不可用，需要重新登录或重启电脑才能恢复。确定要退出输入法服务吗？」，
  选「是」才 `m_server.Stop()`。确认框只在服务端这一处，语言栏与托盘两条路径自动一致。
- **验收**：真机从托盘与语言栏分别点「退出输入法服务」⇒ 各只弹一次确认框；选「否」后
  `Get-Process WeaselServer` 仍在；选「是」后服务停止。

## T9 — 门禁 [P1]

- **输入**：全部 T1–T8 改动。
- **输出**：`tmp/build-115.log`、`tmp/suite-089.log`、`tmp/fmt-089.log`。
- **做什么**：① `./clang-format.sh -i` 或 `.\clang-format.ps1 -i` 过一遍改动的 C++ 文件；
  ② `Stop-Process -Name WeaselServer -Force` 后跑 `scripts\test-infra\run-test-suite.bat`；
  ③ `build.bat weasel`；④ 跑完用 `start_service.bat` 恢复服务。
- **验收**：`SUITE_EXIT=0` 且日志出现 `=== ALL TESTS PASSED ===`；`BUILD_EXIT=0`；
  `git diff --numstat` 无 `weasel.props` / `env.bat` / `*.obj` 误入。