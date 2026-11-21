# B-4 路径 2 离线准备（批次 BD 第 2 项）

生成时间：2026-11-21 · 工作目录 `F:\soft\00selfmade\rime_claude` · 基线 `78416a8`（分支 `feat/100-cloud-sync`）

## 0. 一句话结论

**路径 2 的「真实 TSF 宿主确实发射 FocusOut(PROFILE_DEACTIVATED)」这条判据，本机已用只读探针两次独立坐实，不需要切换用户输入法。** 任务书里那条硬阻塞前提（「激活 Fluxing profile = 切走用户正在用的搜狗」）**被实测否证**：本机当前活跃 profile 就是 Fluxing。

```
B4_VERDICT=PATH2_SIGNATURE_SEEN_IN_READ_ONLY_MODE   FAILURE_COUNT=0   EXIT=0
```

## 1. 签名核对（第 1 项交付）

### 1.1 依据哪份头文件

本机两份 SDK 头都在，编译期**实际选中的是 26100** —— 判据不是猜的：MSBuild 的报错原文引用
`C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\winuser.h(4441,1)`（见
`reports\fluxing-bb-orphan-reclaim\b4-path2-build.log` 首次失败记录）。

| 头文件 | 大小 | 行数 | mtime |
|---|---|---|---|
| `10.0.19041.0\um\msctf.h` | 423 667 B | 12 874 | 2025-03-12 |
| `10.0.26100.0\um\msctf.h` | 463 094 B | 13 593 | 2026-04-15 |

**两份的签名逐字一致，只有行号偏移** ⇒ 本机无论 MSBuild 选哪份都无签名风险。全表（含每个方法行号）已落盘：
`reports\fluxing-bb-orphan-reclaim\b4-msctf-signatures-raw.txt`（抽取器 `b4-extract-msctf.ps1`，只读 SDK）。

### 1.2 本探针用到的方法（下表行号 = 两份头文件各一列）

| 接口 · 方法 | 19041 | 26100 | 签名 |
|---|---|---|---|
| `ITfThreadMgr::Activate` | L1069 | L1079 | `HRESULT (TfClientId *ptid)` |
| `ITfThreadMgr::Deactivate` | L1072 | L1082 | `HRESULT (void)` |
| `ITfThreadMgr::CreateDocumentMgr` | L1074 | L1084 | `HRESULT (ITfDocumentMgr **ppdim)` |
| `ITfThreadMgr::GetFocus` | L1080 | L1090 | `HRESULT (ITfDocumentMgr **ppdimFocus)` |
| `ITfThreadMgr::SetFocus` | L1083 | L1093 | `HRESULT (ITfDocumentMgr *pdimFocus)` |
| `ITfThreadMgr::AssociateFocus` | L1086 | L1096 | `HRESULT (HWND, ITfDocumentMgr *pdimNew, ITfDocumentMgr **ppdimPrev)` |
| `ITfInputProcessorProfiles::ActivateLanguageProfile` | L7756 | L8164 | `HRESULT (REFCLSID, LANGID, REFGUID)` |
| `ITfInputProcessorProfiles::GetActiveLanguageProfile` | L7761 | L8169 | `HRESULT (REFCLSID, LANGID *, GUID *)` |
| `ITfInputProcessorProfiles::EnumInputProcessorInfo` | L7742 | L8150 | `HRESULT (IEnumGUID **ppEnum)` |
| `ITfInputProcessorProfileMgr::ActivateProfile` | L8698 | L9169 | `HRESULT (DWORD, LANGID, REFCLSID, REFGUID, HKL, DWORD)` |
| `ITfInputProcessorProfileMgr::GetActiveProfile` | L8750 | L9221 | `HRESULT (REFGUID catid, TF_INPUTPROCESSORPROFILE *)` |
| `ITfActiveLanguageProfileNotifySink::OnActivated` | L8379 | L8834 | `HRESULT (REFCLSID, REFGUID, BOOL fActivated)` |
| `ITfSource::AdviseSink` | L11063 | L11665 | `HRESULT (REFIID, IUnknown *, DWORD *pdwCookie)` |
| IID 声明行 | `IID_ITfThreadMgr` L1061 / `IID_ITfActiveLanguageProfileNotifySink` L8371 / `IID_ITfSource` L11055 | L1071 / L8826 / L11657 | — |

IID 实测值（两份相同）：`ITfThreadMgr` = `aa80e801-2021-11d2-93e0-0060b067b86e`；
`ITfActiveLanguageProfileNotifySink` = `b246cb75-a93e-4652-bf8c-b3fe0cfd7e57`。

### 1.3 两条否证（任务书假设不成立，须登记，不得照抄）

1. **`ITfFocusMgr` 在本机 SDK 根本不存在。** 全量扫 `10.0.26100.0` 下 **3636** 个 `.h`，`ITfFocusMgr`
   命中 **0**；同法正控制 `ITfThreadMgrEx` 命中 `um\msctf.h:57` ⇒ 搜索法有效。
   ⇒ 任务书要求的 `ITfFocusMgr::SetFocus / PushFocus` 核对项**改为「已否证」**；焦点只能用
   `ITfThreadMgr::SetFocus(ITfDocumentMgr*)` 与 `AssociateFocus(HWND, ITfDocumentMgr*, ITfDocumentMgr**)`。
2. **`ITfThreadMgr` 没有 `Initialize()`**，两份头都只有 `Activate`（L1069 / L1079）
   ⇒ 「新旧两套初始化」的假设在本地 SDK 上不成立；扩展入口是**另一个接口** `ITfThreadMgrEx::ActivateEx(ptid, dwFlags)`（L1253 / L1277）。

另：`TF_INPUTPROCESSORPROFILE`（19041 L8665 / 26100 L9136）**没有 `wlDesc` 字段**，成员只有
`dwProfileType / langid / clsid / guidProfile / catid / hklSubstitute / dwCaps / hkl / dwFlags`
⇒ 探针不打印 profile 描述文本，避免凭记忆写出不存在的字段。

## 2. 探针工程（第 2、3 项交付）

新增 `test\TestTsFocusOutHost\`：`TestTsFocusOutHost.cpp`（26 603 B，UTF-8 无 BOM，LF）+
`TestTsFocusOutHost.vcxproj`（10 917 B，GUID `{7C1F3A94-2D6B-4E18-A0C5-9B47D2E8F631}`）。

刻意与 `test\TestOrphanReclaimE2E` 不同：**不进 `weasel.sln`、无 `ProjectReference`、无 PCH、不链 `rime.lib`**，
只链 `kernel32;user32;gdi32;advapi32;ole32;oleaut32;uuid`。理由：本探针要测的对象是
**操作系统 TSF 栈去加载已安装的 shim**，链产品工程会测错对象。

两种模式：默认 `-DryRun`（纯读）；`-ActivateAndCycle`（激活 + 焦点循环），后者三层守卫：
命令行 `-AllowProfileSwitch` + 环境变量 `FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1` + 远程会话守卫。
激活走路径 A（`ITfInputProcessorProfiles::ActivateLanguageProfile`，会派发 `OnActivated`）；
`-UseLegacyMgr` 走路径 C（`ITfInputProcessorProfileMgr::ActivateProfile`，已知**激活但不派发通知**）。
Fluxing 身份用实测字面值：CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}`、
profile `{3D02CAB6-2B8E-4781-BA20-1C9267529467}`、LANGID `0x0804`（注册表里唯一 `Enable=0x1`）。
退出码 0/1/2/3/4/5/6；3 = 桌面守卫失败，2 = 缺授权开关。

构建（**工程不在 sln ⇒ 必须显式构建并核 mtime**，这是 BB 批登记过的假绿坑）：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  'F:\soft\00selfmade\rime_claude\test\TestTsFocusOutHost\TestTsFocusOutHost.vcxproj' `
  /p:Configuration=Release /p:Platform=x64 '/p:SolutionDir=F:\soft\00selfmade\rime_claude' /nologo /v:minimal
```

结果：`BUILD_EXIT=0` `warning=0 error=0`；产物 `Release\TestTsFocusOutHost.exe` **166 912 B**，
`mtime 2026-11-21 09:59:41` **≥** 源码 mtime ⇒ `FRESH=True`。日志
`reports\fluxing-bb-orphan-reclaim\b4-path2-build.log`。
平台取向：真机宿主加载 `weaselx64.*.dll` ⇒ **x64 为主用**；Win32 配置对应 WoW64 宿主加载 `weasel.dll`。

## 3. `-DryRun` 本机实跑（第 4 项交付）

`reports\fluxing-bb-orphan-reclaim\b4-path2-dryrun.txt`（`EXIT=0`）。验收要求的三件事全部有值：

| 要求 | 实测 |
|---|---|
| 拿到 `ITfThreadMgr` | `THREADMGR=OK`（`THREADMGR_HR=0`、`TM_ACTIVATE_HR=0`、`TM_CLIENT_ID=32`） |
| 枚举到 Fluxing TIP | `TIP_ENUM_COUNT=21` · `FLUXING_TIP_REGISTERED=1` |
| 报出当前活跃 profile | `ACTIVE_KBD_CLSID={A3F4CDED-…}` · `ACTIVE_KBD_PROFILE={3D02CAB6-…}` · `ACTIVE_KBD_LANGID=0x0804` · **`ACTIVE_IS_FLUXING=1`** |

桌面守卫真值：`DESKTOP=Default` `DESKTOP_GUARD=PASS`（`GetThreadDesktop`+`GetUserObjectInformationW(UOI_NAME=2)`）。

**⚠️ 推翻一条旧前提**：任务书与本仓库多份 handoff 记的「本机活跃 profile 是搜狗」在当前机器上**不成立**——
`ITfThreadMgr` 报出的活跃键盘 profile 就是火流猩（与 `ITfInputProcessorProfiles::GetActiveLanguageProfile(0x0804)`
一致：`FLUXING_ACTIVE=1`）。⇒ 「探针会切走你在用的输入法」这一阻塞理由至少对**当前这台机器**需要重新评估。

## 4. 门禁脚本与实测签名（第 5 项交付）

新增 `scripts\verify\b4-tsf-host.ps1`（默认只读；`-RealRun` 需 `-AllowProfileSwitch` +
`FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1`，否则 `EXIT=2 NEEDS_AUTHORIZATION`，什么都不动）。
它做：桌面真守卫 → **并发守卫**（发现有 `TestOrphanReclaimE2E` / `TestFocusOut` / `TestWeaselIPC` /
`WeaselDeployer` 在跑就 `EXIT=5 ABORT`，因为日志切片会被兄弟会话污染）→ 服务端版本预检 →
必要时显式构建并核 mtime → **按字节偏移**取 `%TEMP%\fluxing-profile-notify.log` 切片 →
解析探针 `key=value` → 跑第二个只读探针**独立复查活跃 profile 未被改动** → 打印
`FAILURE_COUNT=` / `B4_VERDICT=` / `EXIT=`。

两轮实跑（`reports\fluxing-bb-orphan-reclaim\b4-tsf-host-gate-dryrun.txt`）：`FAILURE_COUNT=0`
`B4_VERDICT=PATH2_SIGNATURE_SEEN_IN_READ_ONLY_MODE` `EXIT=0`；`SERVER pid=43172 ver=1.3.8.0`；
`PROFILE_BEFORE == PROFILE_AFTER`（`{A3F4CDED…} {3D02CAB6…}` ⇒ 只读模式确实没换输入法）。

两次切片（各 319 B、5 行，`b4-tsf-host-slice-dryrun.txt`）逐字同构：

```
10:08:06.090][server] focus_in profile presence=present
10:08:06.093][server] focus_out param=2 presence=kept
10:08:06.093][server] remove_session active presence=absent
10:08:06:095][server] focus_out param=1 presence=absent
10:08:06.095][tsf] deactivated sent=1
```
```
10:08:56.746 / .749 / .750 / .751 / .753   （同一序列，第二次独立复现）
```

切片中 `SLICE_ORPHAN_RECLAIM_KEPT=0` `SLICE_RECLAIM_ORPHAN_SESSION=0` ⇒ 本轮**没有**触发 ADR 0067
的孤儿回收，presence 翻 absent 是**正常** `remove_session` 分支干的，判据干净。

### 4.1 消息序：以真机到达序为准 —— **三条消息夹心**（本轮定案，推翻两种旧说法）

源码顺序：`WeaselTSF\WeaselTSF.cpp:124` 先 `_PostProfileNotify(kProfileNotifyDeactivated)`（投递，异步经
`_ProfileNotifyLoop` 在 `:445` 发 `FOCUS_OUT(param=2 PROFILE_DEACTIVATED)`），`:125` 才 `m_client.EndSession()`。
spec 094 注释 `:119-123` 明确要求这条通知**必须在 `EndSession()` 之前投递**（否则 session_id 已置 0，FocusOut 带不上会话号）。

真机四组（`10:08:06.090` 组A-1 / `06.225` A-2 / `56.746` B-1 / `56.884` B-2）服务端**到达序逐字一致**：

```
focus_in profile presence=present
focus_out param=2 presence=kept      ← PROFILE_DEACTIVATED，**早于** END_SESSION
remove_session active presence=absent ← END_SESSION，presence 在此翻成 absent（只翻一次）
focus_out param=1 presence=absent     ← APP_FOCUS_LOST，**晚于** END_SESSION，幂等不重复翻
[tsf] deactivated sent=1
```

⇒ **旧的两处说法都要更正**：① `b4-focusout.md` §2「先收 END_SESSION、后收 FOCUS_OUT(PROFILE_DEACTIVATED)」
**被否证**（deactivated 早于 END_SESSION）；② 本节先前那句「`remove_session`@.093 早于 `focus_out param=1`@.095
⇒『END_SESSION 先 / FOCUS_OUT 后』升级为有实测支持」**只对 `param=1` 成立**，
不得推广到 `param=2`（两者不是同一条消息，`param=2` 在 END_SESSION **之前**）。
「与 §2 散文复述应视为笔误」的判词一并作废：**§2 的顺序推断本身是错的**，不是笔误。

### 4.2 已证明 / 未证明（口径务必分清）

已证明（本机，x64，装机版 1.3.8.0）：

1. 真实 OS TSF 栈会把已安装的 Fluxing shim 实例化进一个真实宿主进程（探针进程自身），
   并走 `OnActivated` → `focus_in profile presence=present`；
2. 该宿主经历 `OnSetFocus(FALSE)`（`focus_out param=2`，**单独不翻 presence**）与 `Deactivate()`
   （`remove_session presence=absent` + `focus_out param=1`），
   且 shim 自己写下 `[tsf] deactivated sent=1`（`WeaselTSF.cpp:447-448`）
   ⇒ **路径 1 只能伪造的那两个调用，真实 TSF 路径确实发射且送达**；
3. 顺序与产品代码行号一致；
4. 全程只读：活跃 profile 前后逐字不变。

**未证明**（不得当成已闭环）：

1. **触发源是宿主进程退出/线程拆除**，不是「用户在 Fluxing 激活时 Alt-Tab 到别的应用」或
   「用户切到另一个 TIP 导致 profile 切换」。两者最终都进 `Deactivate()`，但入口条件不同；
   若验收口径要求「用户切窗口」，仍需 `-ActivateAndCycle`（或人手动切窗口）另做一轮；
2. `-ActivateAndCycle` 分支**只编译通过、从未执行**（本任务禁止真跑）；
3. WoW64/Win32 宿主（加载 `weasel.dll`）路径未测；
4. 切片归属虽有两轮复现，但**并发守卫只按进程名判断**，兄弟会话若用别的进程驱动同一日志仍可能落进窗口；
   正式闭环建议在一次独占时间窗内跑，并把 `LOG_BYTES_BEFORE` 与报告时间戳一起留档。

## 5. RUNBOOK（下一批或主会话直接抄）

```powershell
# 1) 只读取证（无需授权、不换输入法、可随时跑）——本批已跑通
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b4-tsf-host.ps1'
#    期望 FAILURE_COUNT=0 / EXIT=0；若 EXIT=5 且 PEER_RUNNING=... 说明兄弟验证在跑，等它结束

# 2) 需要「用户切窗口/切 profile」口径时，才做真跑（会短暂把键盘交给火流猩再交回）
$env:FLUXING_VERIFY_ALLOW_PROFILE_SWITCH = '1'
pwsh -NoProfile -File 'F:\soft\00selfmade\rime_claude\scripts\verify\b4-tsf-host.ps1' -RealRun -AllowProfileSwitch -HoldMs 1500
#    期望 B4_VERDICT=PATH2_CLOSED EXIT=0
#    跑完务必复查：探针第 2 轮报的 PROFILE_AFTER 与跑前逐字相同（脚本已自动做并打印）

# 3) 单独看切片原文
notepad 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-orphan-reclaim\b4-tsf-host-slice-dryrun.txt'
```

红线不变：不 `/quit`/重启 `WeaselServer`、不杀宿主、不改注册表持久项、不动 `WeaselTSF\**` /
`include\**` / `librime\**` / `install.nsi` / 版本号。

## 6. 本轮工具坑（新增，均可复现）

1. **P/Invoke 的 DLL 名写错不会在 `Add-Type` 期报错，用到才抛** —— `GetThreadDesktop` 与
   `GetUserObjectInformationW` 都在 **user32.dll**（`winuser.h`），不是 kernel32。第一次跑就在调用点炸。
   ⇒ 这正是 BB 批登记的「假守卫」家族的另一形态；本脚本把异常**显式**转成 `B4_VERDICT=ABORT EXIT=5`，
   绝不 `try{}catch{}` 吞掉（吞掉 = 形同虚设）。
2. **`[int]$SwitchParameter` 不能转换**（PowerShell 无此转换）⇒ 用 `$(if ($sw) {1} {0})`。
   布尔变量 `[int]$bool` 则是合法的，别一概而论。
3. **探针把两个键打在同行**（`PROBE_VERDICT=PASS EXIT=0`）⇒ 门禁里 `-eq 'PASS'` **恒 false**，
   会把绿的跑成 `FAILURE_COUNT=1`。判 `key=value` 契约时须先确认「一行一键」这条前提。
   ⇒ 已改 `-like 'PASS*'`。
4. **`IntToPtr` 不是 Win32 头文件里的宏**（那是 .NET/`IntPtr` 习惯）⇒ `error C2061 IntToPtr`，
   并连带把 `CreateWindowExW` 的参数表解析打断成 9 个参数（`C2660`）。改 `static_cast<UINT_PTR>(…)`
   再 `reinterpret_cast<HMENU/HBRUSH>`。
5. **`.vcxproj` 里 `AdditionalIncludeDirectories` 被错闭合为 `</PrecompiledHeader>`**（只在 `Debug|x64`
   一组）⇒ MSBuild 解析整个 XML 失败，**主用 Release 配置也一起构建不出来**。改前核对每个 ItemDefinitionGroup。
6. `Select-String -LiteralPath` 直接吃绝对路径最稳；跨进程取服务端版本用
   `Get-Process WeaselServer | % MainModule.FileVersionInfo.FileVersion` 在 PPL 进程上**本机可用**（不必 `cmd /c`）。

## 7. 证据路径清单

| 内容 | 路径 |
|---|---|
| 签名索引（两份头文件、逐方法行号） | `reports\fluxing-bb-orphan-reclaim\b4-msctf-signatures-raw.txt` |
| 签名抽取器（只读 SDK） | `reports\fluxing-bb-orphan-reclaim\b4-extract-msctf.ps1` |
| 构建日志 | `reports\fluxing-bb-orphan-reclaim\b4-path2-build.log` |
| 探针 dry-run 原始 stdout | `reports\fluxing-bb-orphan-reclaim\b4-path2-dryrun.txt` |
| 门禁完整输出 | `reports\fluxing-bb-orphan-reclaim\b4-tsf-host-gate-dryrun.txt` |
| 门禁内探针输出 / 日志切片 / 恢复复查 | `reports\fluxing-bb-orphan-reclaim\b4-tsf-host-probe-dryrun.txt` · `b4-tsf-host-slice-dryrun.txt` · `b4-tsf-host-restore-dryrun.txt` |
| 探针源码与工程 | `test\TestTsFocusOutHost\TestTsFocusOutHost.cpp` · `.vcxproj` |
| 门禁脚本 | `scripts\verify\b4-tsf-host.ps1` |
| 服务端/shim 诊断日志 | `%TEMP%\fluxing-profile-notify.log`（`[server]` / `[tsf]` 切片） |
| 产品侧只读依据（未改） | `WeaselTSF\WeaselTSF.cpp:114-151` `:283-306` `:389-454`（447-448 写 `deactivated sent=`） · `WeaselTSF\KeyEventSink.cpp:137-146` · `test\TestWeaselIPC\TsfProfileVerify.cpp` 头 1-80 行 |
