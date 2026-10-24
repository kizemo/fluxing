# Fluxing Code Map — 完整盘点 (2026-07-31)

> **Purpose**: 在你下一次安装 / 重启之前, 我已写明每一块代码的职责 + 现状。
> **教训**: 1 周前 v0.36 commit (062734e) 写过 "stop piling fixes" (L73), **我没 internalize**,
> **同样的 meta-pattern 重演在 v0.74.x 系列 (7 ship attempts)**。
> **承诺**: 这次不再让 system state pollution 漏过去。 文档完整覆盖 install / registry / process / file / shim 5 大状态面。

---

## 1. Codebase Vitals (per `codebase-recon` skill)

| Metric | Value |
|---|---|
| HEAD | `fd5a644` (v0.19.0.62) |
| Branch | Fluxing |
| Total commits | 1343 |
| Last 30 days | 214 commits |
| Tracked files | 994 |
| Active contributors | 1 (`duanyi`) |
| Submodules | librime (x86-only, L10) |

## 2. Highest-churn files (90d) → 优先勘察

| File | Commits | 重要性 |
|---|---|---|
| `WeaselServer/QuickPanelDialog.cpp` | 43 | QuickPanel UI, spec 070 rewrite, 含 L67-L69 GDI+ 教训 |
| `output/install.nsi` | 34 | NSIS 装机脚本, 装每个版本都改 |
| `WeaselServer/QuickPanelDialog.h` | 28 | UI header |
| `WeaselServer/PhrasesDialog.cpp` | 24 | 短语 UI (in-process 旧版, 之后改成 out-of-process) |
| `WeaselServer/WeaselServerApp.cpp` | 21 | **Server 主入口, tray icon, SetupMenuHandlers, ImeLangChangeSubclass** |
| `RimeWithWeasel/RimeWithWeasel.cpp` | 10 | **librime 桥, IPC handler, FocusIn/Out/AddSession** |
| `WeaselServer/UserDictionary.cpp` | 9 | user dict (L81-fix) |
| `WeaselUI/WeaselPanel.cpp` | 7 | **输入 panel (GDI+ 残留, 没重写)** |
| `WeaselServer/WeaselServerImpl.cpp` | 4 | (图谱 index 显示 4, 实际 diff 多) |
| `WeaselDeployer/Configurator.cpp` | (未读) | librime deploy (L67 / L55) |
| `WeaselTSF/WeaselTSF.cpp` | (未读) | **TSF shim 装入 explorer/claude/dopus 的入口** |
| `WeaselUI/WeaselUI.cpp` | (未读) | UI main |

## 3. 关键子系统 — 已知职责 + 后续未读

### 3.1 WeaselServer.exe (x86 + PPL, 装在 `D:\Program Files\fluxing\weasel\`)

进程入口 + 主消息循环 + 4 关键 module:

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `WeaselServer/WeaselServer.cpp` | `_tWinMain` + Win32 消息循环 | ❌ 未读 |
| `WeaselServer/WeaselServerApp.cpp` | **Tray icon + RegisterHotKey Alt+, + SetupMenuHandlers + ImeLangChangeSubclass** | ✅ main + ImeLangChangeSubclass 部分 (line 0-280) |
| `WeaselServer/PhrasesDialogIPC.cpp` | **PhrasesDialog out-of-process 子进程管理** (Phase K3, v0.51+) | ✅ 全 (line 0-492) |
| `WeaselServer/QuickPanelDialog.cpp` | 浮动设置栏 (spec 070 pure GDI + WIC) | ✅ line 0-9 + 200-280 |
| `WeaselServer/WeaselServerImpl.cpp` | **ServerImpl IPC handler (OnKeyEvent/OnFocusIn/OnUpdateInputPosition)** | ✅ 全 |
| `WeaselServer/WeaselTrayIcon.cpp` | tray icon registration | ❌ 未读 |
| `WeaselServer/SystemTraySDK.cpp` | SystemTray SDK | ❌ 未读 |
| `WeaselServer/UserDictionary.cpp` | user dict UI | ❌ 未读 |
| `WeaselServer/ShortcutSettings.cpp` | shortcut settings | ❌ 未读 |

**已知关键 process-pollution paths** (从 reads):
- `WeaselServerApp.cpp:174` `m_hwndImeDefault = ::ImmGetDefaultIMEWnd(NULL)` — 装入 default IME window (L##-PhaseL-4 dead path in Win11 24H2)
- `WeaselServerApp.cpp:212-216` `taskkill /F /IM ctfmon.exe /T` × 5 + `taskkill /F /IM TextInputHost.exe /T` × 5 — **taskkill 完 ctfmon 自动 respawn,重新 load 老 shim DLL** (per L##-PhaseL-9.5 commentary)
- `PhrasesDialogIPC.cpp:286-327` `LaunchPhrasesDialog` 启动 `FluxingPhrasesDialog.exe` 子进程 (post-Phase K3, v0.50+)

### 3.2 WeaselTSF.dll (x86 + x64, 装到 x86 / x64 system process)

**TSF shim DLL**, 装入 explorer / claude / dopus / HipsMain / aboboo / ctfmon / TextInputHost 等所有挂 TSF 的进程。

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `WeaselTSF/WeaselTSF.cpp` | TSF shim 入口 (`ITfActiveLanguageProfileNotifySink` 等) | ❌ 未读 |
| `WeaselTSF/KeyEventSink.cpp` | ITfKeyEventSink 实现 | ❌ 未读 |
| `WeaselTSF/ThreadMgrEventSink.cpp` | ITfThreadMgrEventSink (v0.66 sink 4 件套) | ❌ 未读 |
| `WeaselTSF/Register.cpp` | TSF COM server registration | ❌ 未读 |
| `WeaselTSF/WeaselTSF.h` | COM interface 声明 | ✅ grep 部分 |
| `WeaselTSF/Server.cpp` | (grep 显示 dk) | ❌ 未读 |
| `WeaselTSF/EditSession.cpp` | ITfEditSession impl | ❌ 未读 |
| `WeaselTSF/dllmain.cpp` | DLL 入口 | ❌ 未读 |
| `WeaselTSF/ctffunc.h` | CTF helpers | ❌ 未读 |

**CRITICAL 观察 (forensic)**: 老的 `weaselx64.dll` 1.2MB 在 System32, 是 v0.66 era Rime 官方 2.0.0.9 旧 shim。**这个 DLL 跟当前 WeaselServer.exe v0.62 IPC 协议不兼容**, ctfmon 加载它就 crash。**WeaselTSF.dll 的 `weaselx64.dll` 是加载 System32 这一份, 而非 `D:\Program Files\fluxing\weasel\weaselx64.dll`** — 需确认。

### 3.3 WeaselDeployer.exe (x86 + 装在同 $INSTDIR)

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `WeaselDeployer/WeaselDeployer.cpp` | deployer 主入口 | ❌ 未读 |
| `WeaselDeployer/Configurator.cpp` | librime config 部署 / MaintenanceGuard | ❌ 未读 |
| `WeaselDeployer/MaintenanceGuard.h` | L67 / L55 leveldb lock detection | ❌ 未读 |
| `WeaselDeployer/SwitcherSettingsDialog.cpp` | switcher settings | ❌ 未读 |
| `WeaselDeployer/DictManagementDialog.cpp` | dict management | ❌ 未读 |
| `WeaselDeployer/UIStyleSettings.cpp` | UI style settings | ❌ 未读 |
| `WeaselDeployer/UIStyleSettingsDialog.cpp` | UI style dialog | ❌ 未读 |

**已知**: install.nsi 在 `Section "-POST_INSTALL"` 调 `ExecWait "$INSTDIR\WeaselDeployer.exe /install"` 和 `ExecWait "$INSTDIR\WeaselDeployer.exe /deploy"` (line 758, 762)。 这是 L##-PhaseL-7 17 min hang 的关键路径, **PPL 持有 WeaselServer.exe + MaintenanceGuard 等待 EnsureNotMaster 锁**。

### 3.4 WeaselUI (在 WeaselServer.exe 进程内使用的 UI lib)

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `WeaselUI/WeaselPanel.cpp` | **输入 panel (含 GdiplusStartup, 未重写)** | ✅ line 0-120 |
| `WeaselUI/WeaselUI.cpp` | UI main | ❌ 未读 |
| `WeaselUI/FluxingComponents/D2DRenderer.cpp` | D2D renderer | ❌ 未读 |
| `WeaselUI/FluxingComponents/Panel.cpp` | fluxing panel | ❌ 未读 |
| `WeaselUI/FluxingComponents/Label.cpp` | fluxing label | ❌ 未读 |
| `WeaselUI/FluxingComponents/Button.cpp` | fluxing button | ❌ 未读 |
| `WeaselUI/FluxingComponents/Toggle.cpp` | fluxing toggle | ❌ 未读 |
| `WeaselUI/FluxingComponents/FluxingTheme.cpp` | fluxing theme | ❌ 未读 |
| `WeaselUI/StandardLayout.cpp` | layout | ❌ 未读 |
| `WeaselUI/GdiplusBlur.cpp` | GDI+ blur | ❌ 未读 |
| `WeaselUI/DirectWriteResources.cpp` | DirectWrite | ❌ 未读 |

**CRITICAL 已知**: `WeaselUI/WeaselPanel.cpp:107` `GdiplusStartup(&_m_gdiplusToken, &_m_gdiplusStartupInput, NULL);` — WeaselPanel 仍用 GDI+ (spec 074 只重写 QuickPanel, **没重写 WeaselPanel**)。 这是 L67-L69 雷区真正残留路径。

### 3.5 RimeWithWeasel (lib 桥 librime ↔ WeaselServer)

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `RimeWithWeasel/RimeWithWeasel.cpp` | handler 实现 (FocusIn/Out/ProcessKeyEvent) | ✅ 300-530 |
| `RimeWithWeasel/WeaselServerImpl.cpp` | handler 接口 | ❌ 未读 |
| `RimeWithWeasel/FluxingDarkModeBridge.cpp` | dark mode bridge | ❌ 未读 |
| `RimeWithWeasel/WeaselUtility.cpp` | utility | ❌ 未读 |
| `RimeWithWeasel/stdafx.cpp` | stdafx | ❌ 未读 |

**已知**: `RimeWithWeasel.cpp:434-492` 的 `FocusIn` 是 v0.19.0.65 re-enable QuickPanel Show 的入口 — **这次的 stash 里有我的 v0.74.3 + if (false) 撤回改动**。

### 3.6 FluxingPhrasesDialog (out-of-process UI, K3+)

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `FluxingPhrasesDialog/main.cpp` | 主入口 | ❌ 未读 |
| `FluxingPhrasesDialog/PhrasesDialog.cpp` | 短语 UI | ❌ 未读 |
| `FluxingPhrasesDialog/PipeClient.cpp` | named pipe client | ❌ 未读 |
| `FluxingPhrasesDialog/ModalChrome.cpp` | modal chrome | ❌ 未读 |

### 3.7 WeaselIPC (RPC client lib)

| 文件 | 职责 | 阅读状态 |
|---|---|---|
| `WeaselIPC/WeaselClientImpl.cpp` | IPC client impl | ❌ 未读 |
| `WeaselIPC/PipeChannel.cpp` | named pipe transport | ❌ 未读 |
| `WeaselIPC/Configurator.cpp` | config | ❌ 未读 |
| `WeaselIPC/ActionLoader.cpp` | action loader | ❌ 未读 |
| `WeaselIPC/Committer.cpp` | committer | ❌ 未读 |
| `WeaselIPC/ContextUpdater.cpp` | context updater | ❌ 未读 |
| `WeaselIPC/Deserializer.cpp` | deserializer | ❌ 未读 |
| `WeaselIPC/ResponseParser.cpp` | response parser | ❌ 未读 |
| `WeaselIPC/Styler.cpp` | styler | ❌ 未读 |

## 4. Installer state surface (v0.62 install.nsi)

| 状态面 | 路径 / Key | 当前 v0.62 install.nsi 行为 |
|---|---|---|
| **Files in INSTDIR** | `$INSTDIR\weasel\*.exe\|*.dll` | ✅ File / Rename-then-File (L13/L14-fix) |
| **System32 shim DLLs** | `C:\WINDOWS\System32\weasel*.*` | ❌ **完全不碰** — 12 个残留全在 |
| **HKLM InstallDir** | `HKLM\Software\Fluxing\Weasel\InstallDir` | ✅ WriteRegStr (line 384) |
| **HKCU RimeUserDir** | `HKCU\Software\Fluxing\Weasel\RimeUserDir` | ✅ WriteRegStr (line 697) |
| **HKLM KnownClasses** | `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{A3F4CDED...}` | ✅ WriteRegStr (line 739) |
| **HKCU 0x00000804 Default** | `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6...}\Default` | ✅ WriteRegStr (line 741) |
| **HKCU 0x00000804 Profile** | `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6...}\Profile` | ✅ WriteRegStr (line 742) |
| **HKCU 0x00000804 KeyboardLayout** | `HKCU\...\{3D02CAB6...}\KeyboardLayout` | ✅ WriteRegDWORD 0x08040804 (line 743) |
| **HKLM AutoRun** | `HKLM\Software\Microsoft\Windows\CurrentVersion\Run\WeaselServer` | ✅ WriteRegStr (line 798) |
| **HKCU CheckForUpdates** | `HKCU\Software\Rime\Weasel\Updates\CheckForUpdates` | ✅ WriteRegStr (line 806/809) |
| **HKLM Uninstall reg** | `HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing` | ✅ WriteRegStr × 8 (line 745-753) |
| **Process start** | `WeaselServer.exe /quit` | ✅ ExecWait (line 331) |
| **Process kill** | `taskkill /F /IM WeaselServer.exe /T` | ✅ nsExec::ExecToStack (line 337) |
| **Process kill ctfmon** | `taskkill /F /IM ctfmon.exe /T` × 5 | ✅ nsExec::ExecToStack × 5 (line 212-216) |
| **Process kill TextInputHost** | `taskkill /F /IM TextInputHost.exe /T` × 5 | ✅ nsExec::ExecToStack × 5 (line 215) |
| **WeaselDeployer /install** | `ExecWait WeaselDeployer.exe /install` | ✅ ExecWait (line 758) — **L##-PhaseL-7 17 min hang 真因 PPL lock** |
| **WeaselDeployer /deploy** | `ExecWait WeaselDeployer.exe /deploy` | ✅ ExecWait (line 762) |
| **WeaselSetup /u** | `ExecWait WeaselSetup.exe /u` | ✅ ExecWait (line 340) |

**缺失 (但应该)**:
- ❌ `Delete /REBOOTOK $WINDIR\System32\weasel*.dll` (L##-PhaseL-9.6 引入于 v0.74.1)
- ❌ `IfFileExists` + `DetailPrint` cleanup log (L##-PhaseL-9.10 引入于 v0.74.2)
- ❌ System32 glob cleanup (L##-PhaseL-9.7 引入于 v0.74.1)
- ❌ Admin check + SetErrorLevel 6002 (v0.74.3 draft, **未 ship**)
- ❌ Stage 2 atomic swap (L##-PhaseL-9.5 引入于 v0.74.1)

## 5. State pollution sources (12+ 已知)

**No installer** 在 v0.62 自动清这些 (**installer 不知道**, 必须 admin 手动):

| Type | 位置 | 谁留下 | 残留影响 |
|---|---|---|---|
| System32 shim DLL | `C:\WINDOWS\System32\weasel.dll` (204KB) | v0.74.x ship | ctfmon load → IPC mismatch → 0xc0000374 |
| System32 x64 shim | `C:\WINDOWS\System32\weaselx64.dll` (1.2MB) | v0.66 era Rime 2.0.0.9 | TextInputHost load → x64 crash |
| System32 old DLL | `C:\WINDOWS\System32\weasel.old.dll` (1.16MB) | v0.66 era | 老 shim load |
| System32 backup variants | `*.bak`, `*.old`, `*.old.10`, `*.old.135039`, `*.v465`, `*.v466bak-*` | v0.66 era | 老 shim load |
| System32 IME | `C:\WINDOWS\System32\weasel*.ime` | v0.66 era | 32-bit IME compat |
| HKLM InstallDir | `HKLM\Software\Fluxing\Weasel\InstallDir` | v0.62+ | 不冲突 |
| HKLM AdminRequired | `HKLM\Software\Fluxing\Weasel\AdminRequired` | v0.74.3 draft (未 ship) | 仅 v0.74.3 写 |
| HKCU RimeUserDir | `HKCU\Software\Fluxing\Weasel\RimeUserDir` | v0.62+ | 不冲突 |
| HKLM Fluxing unknown keys | `HKLM\Software\Fluxing\*` | v0.74.x | 未识别 |
| HKCU Rime keys | `HKCU\Software\Rime\Weasel\Updates` | v0.59+ | 不冲突 |
| Registry KnownClasses | `HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{...}` | L66-fix | 必须每 install 重写 |
| Registry 0x00000804 | `HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{...}` | L66-fix | 必须每 install 重写 |
| Startup Run keys | `HKLM\Software\Microsoft\Windows\CurrentVersion\Run\WeaselServer` | v0.62+ | 启动项 |
| WeaselServer.exe running | `WeaselServer.exe` PPL process | 上一次 install | 持有 shim DLL mmap handle |
| ctfmon.exe running | `ctfmon.exe` | Windows | `taskkill` 后 auto-respawn |
| TextInputHost.exe running | `TextInputHost.exe` | Windows | `taskkill` 后 auto-respawn |
| FluxingPhrasesDialog running | `FluxingPhrasesDialog.exe` | v0.50+ | 子进程 |
| fluxing\crash dump | `%LOCALAPPDATA%\fluxing\crash\` | L67 (未工作) | L##-PhaseL-9.10 路径 |
| %TEMP% fluxing-stage2-shim | `%TEMP%\fluxing-stage2-shim\` | v0.74.x | L##-PhaseL-9.5 stage2 |
| Stage 2 scheduled tasks | `schtasks /Query` | v0.74.x | L##-PhaseL-9.5 |
| Distribution cache | `Rime\weasel\0.19.0\` | librime | 镜像 |
| user dict | `user1\fluxing` | librime | user dict |

## 6. **Same pattern history** — L73 v0.36 + L##-PhaseL-9.11 v0.74.x

**v0.36 commit (062734e, 2026-07-11)** — L73 教训:
> "5 versions: v0.19.0.0/1/2/3/4/5 each compiled clean and 'fixed' one symptom, but no version works end-to-end on user machine"
> "I did not STOP per debugging-and-error-recovery Step 8 after each failed fix; I piled the next fix instead"
> "L## entries were written as 'fix narratives' not failure post-mortems"

**v0.74.x 系列 (2026-07-25 → 2026-07-31)** — L##-PhaseL-9.11-Retro 教训:
> "v0.66 → v0.67 → v0.68 → v0.69 → v0.70 → v0.71 → v0.74.1 → v0.74.2 → v0.74.3 = 9 ship attempts"
> "Same meta-pattern: patch → fail → patch → fail, no escape valve"
> "L73 was a process failure, not a code failure. v0.74.x is the same process failure."

**关键差异**:
- **L73 写于 v0.36 时** — 没 generalize 到 "all install cycles" — 7 天后 v0.74.x 重演
- **L##-PhaseL-9.10 写于 v0.74.2 时** — 没 generalize 到 "all System32 cleanup" — 7 天后 v0.62 也重演
- **L##-PhaseL-9.11-Retro 写于 v0.74.3** — 应该 generalize 到 **"all inbound state pollution across install cycles"**

## 7. 我**没**读的子系统 (诚实清单)

不会声称掌握了未读的部分:

- ❌ WeaselServer/WeaselServer.cpp (WinMain 入口)
- ❌ WeaselServer/WeaselTrayIcon.cpp (tray icon)
- ❌ WeaselServer/SystemTraySDK.cpp (tray SDK)
- ❌ WeaselServer/UserDictionary.cpp + ShortcutSettings.cpp
- ❌ WeaselTSF/WeaselTSF.cpp / KeyEventSink.cpp / ThreadMgrEventSink.cpp / Register.cpp / Server.cpp / EditSession.cpp / dllmain.cpp (TSF shim 全部)
- ❌ WeaselDeployer/Configurator.cpp / MaintenanceGuard.h / 各 dialog.cpp
- ❌ WeaselUI/WeaselUI.cpp / DirectWriteResources.cpp / GdiplusBlur.cpp / StandardLayout.cpp / HorizontalLayout.cpp / VerticalLayout.cpp / FullScreenLayout.cpp / VHorizontalLayout.cpp
- ❌ WeaselUI/FluxingComponents/D2DRenderer.cpp / Panel.cpp / Label.cpp / Button.cpp / Toggle.cpp / FluxingTheme.cpp
- ❌ WeaselIPC/ActionLoader.cpp / Committer.cpp / ContextUpdater.cpp / Deserializer.cpp / ResponseParser.cpp / Styler.cpp / PipeChannel.cpp / WeaselClientImpl.cpp / Configurator.cpp
- ❌ FluxingPhrasesDialog/main.cpp / PhrasesDialog.cpp / PipeClient.cpp / ModalChrome.cpp
- ❌ WeaselSetup/WeaselSetup.cpp / InstallOptionsDlg.cpp / imesetup.cpp
- ❌ FluxingConfigEditor/*.cpp
- ❌ RimeWithWeasel/WeaselServerImpl.cpp / FluxingDarkModeBridge.cpp / WeaselUtility.cpp
- ❌ 完整 .h files (大部分未读)
- ❌ output/install.nsi 完整 (只扫 4 段, ~25% 覆盖)
- ❌ xmake.lua / weasel.sln 完整
- ❌ CHANGELOG.md 全文 (7 天 ship 关联)
- ❌ .claude/rules/*.md 全文
- ❌ .specify/specs/071-install-freeze-fix/* 完整
- ❌ L##-PhaseL-9.5-H / 9.5-G / 9.6 / 9.7 / 9.8 完整教训详文

## 8. **Recovery plan** (无需你重启 / 重新装的明确路线)

### Step 1: 手动清状态污染 (1 次, 5 min)

你**不要**立刻重新装。 先手动清理状态污染:

```bat
:: 1. Run as admin cmd
:: 2. Execute:
cd /D "F:\soft\00selfmade\rime_claude"
_diag_v0.62_clean_state_admin.bat
```

这会:
- Stop ctfmon + TextInputHost (5x retry)
- Delete 12 System32 shim DLLs (用 /REBOOTOK 兜底)
- Remove HKCU\Software\Fluxing registry
- Remove Stage 2 / fluxing-staged dirs
- Lead 显示 schedule reboot need

### Step 2: 是否 reboot

**只有** if SCHEDULE_REBOOT shown — 因为 System32 shim DLL 被 ctfmon 锁住, del fail。下次 reboot 时 OS 自动 delete.

### Step 3: 重新 install v0.62 (纯净状态)

- 从 `release\fluxing-0.19.0.62-installer.exe` 装
- v0.62 binary md5 验证: **正确** (我已 verify)
- install.nsi 不会 touch System32, **但 System32 现在已空**, 所以这次 ctfmon 加载空 (或者重新 respawn 加载 MS 默认 TSF shim)

### Step 4: 24h 验证

- `_diag_v0.62_state_pollution_audit.ps1` → System32 should be EMPTY
- `_diag_v0741_evidence.ps1` → 0xc0000374 24h count ≤ 5
- `_diag_v0741_dumps.ps1` → 没新 dump

### Step 5: 如果仍崩 → 可能更深层

- Check WeaselTSF.dll 是否被 explorer / claude / dopus 加载 (Process Monitor)
- Check WeaselServer.exe 是否 PPL 启动 (per L17)
- Check librime 1.13 vs 1.14 version compat

## 9. 我**不会**做的 (per user instruction)

1. ❌ **不**让你再装机 / 重启  **直到** 状态污染清理报告确认
2. ❌ **不**声明"我做完了" / "我修好了" **直到** 真机 24h 验证完成
3. ❌ **不**写新补丁  **直到** 现状根因 (v0.74.x 状态污染) 完全公开报告
4. ❌ **不**动 release/ 或 build 任何东西  **直到** 你明确允许

## 10. 还需要你做的 (manual)

- [ ] **手动**跑 `_diag_v0.62_clean_state_admin.bat` (admin cmd)
- [ ] **如果** SCHEDULE_REBOOT shown → reboot
- [ ] **重新装** v0.62 installer
- [ ] **24h 后** 跑 `_diag_v0.62_state_pollution_audit.ps1` + `_diag_v0741_evidence.ps1`
- [ ] **报告** 结果给我

如果**结果**是 "System32 clean + 0xc0000374 count ≤ 5" → 算 fix
如果**结果**是 "System32 仍有 DLL / 0xc0000374 count > 5" → 我们进入**下一轮** 真因调查 (要看 dump / Process Monitor), 还是不让你装新版本

---

承诺: 这是我**最后一次** 让你装机 / 重启 之前的状态。看完这份 map,**让我读完整 lessons-learned + 读完整 .tsf/.ui 代码** 再决定下一步。
