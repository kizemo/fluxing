# 批次 AZ · 1.3.6 装机 + 生效判据闭环（TSF shim 注册真值坐实）

- 日期：2026-11-20
- 会话：`mvs_74150cd7376a4bbfb29e8b8603dc04c6`
- 上一批：[批次 AY · 出包 1.3.6](handoff-2026-11-20-batch-ay-release-1-3-6.md)（提交 `8a508b0` + `3dc4eaf`，均已 push）
- 本批性质：**装机 + 取证收口批次**（未改产品代码、未改 `librime/`、未改 IPC 协议、未新增依赖、未提交）
- 用户拍板链条：16:32「先给重启装机窗口，装 1.3.6」⇒ 本轮执行装机

---

## 1. 背景与目标

AY 批次已出包 `fluxing-1.3.6-installer.exe`，包内载荷含三项代码改动：ADR 0067 孤儿 IPC 会话回收（`8accf52`）、账号页「换钥后果」常驻提示（`02c67d6`）、`[acctdlg]` 诊断日志改接落盘。AY 的未闭环首条即「**1.3.6 未装机**（装机版仍是 1.3.5.0）」。

本批目标 = 把 1.3.6 装到本机，并**坐实"新进程是否真在跑 1.3.6"**——口径必须是**运行中映像版本 + TSF 注册真值**，不是磁盘哈希、不是包内文本令牌。

### 装机前置决策（本轮自行判定，未打扰用户）

| 决策 | 结论 | 依据 |
|---|---|---|
| 是否清注册表（CLAUDE.md / project-knowledge **L54**） | **不清** | `HKCU:\SOFTWARE\Fluxing\Weasel\RimeUserDir` 承载用户词典/同步数据目录指针 ⇒ 删之属不可逆破坏；且 `WOW6432Node\Fluxing\Weasel\InstallDir` 现值与期望根**逐字相同**，L54 要防的 stale 路径劫持在本机不成立 |
| 是否先 `/quit` 服务端 | **不做** | 实测证明不必要（见 §3.2，服务映像被在线换掉）；L109 亦禁止杀 TSF 宿主 |
| silent 命令是否加 `/NCMS` `/R...` | **不加** | grep `output\install.nsi` 与 `.specify\memory\project-knowledge.md` 均 `No matches found` ⇒ 先前摘记错误，本批作废该记录 |
| `/D=` 写法 | `/D=D:\Program Files\fluxing`，**不带引号、放最后** | `install.nsi:403-404` + L112（带引号 ⇒ NSIS **忽略整个 `/D=`**） |

---

## 2. 本批交付

- ✅ 1.3.6 **已装机**：`INSTALL_EXIT=0`，耗时 41 s（16:41:39.38 → 16:42:20.35）
- ✅ 生效判据**闭环**：服务层 + TSF shim 层双双坐实为 1.3.6.0（§3.2 / §3.3）
- ✅ 修正 AX/AY 前提：「装机必须注销/重启」**只对已运行的 TSF 宿主成立**，对 PPL 服务端不成立
- ✅ 本 handoff + [接力 prompt](../prompts/prompt-next-session-2026-11-20-batch-az-install-1-3-6.md)
- ✅ CLAUDE.md 交接指针新增 AZ 条目
- ❌ 未提交任何代码/文档改动（本批只落文档，提交待用户授权）

---

## 3. 实测结果

### 3.1 装机命令（可直接复制）

先 `write` 一个**纯 ASCII** `.cmd`（路径 `E:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-install-136.cmd`）：

```bat
@echo off
echo INSTALL_START=%DATE% %TIME%
"F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.3.6-installer.exe" /S /D=D:\Program Files\fluxing
echo INSTALL_EXIT=%ERRORLEVEL%
echo INSTALL_DONE=%DATE% %TIME%
```

再 `cmd /c` 包裹调用（`run_in_background` + 同轮 `task_output` 等终态）：

```powershell
cmd /c "E:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-install-136.cmd"
```

> `cmd /c` 包裹 `.cmd` 文件而非把整条命令塞进 PS 字符串 ⇒ 绕开 `cmd /c` 的引号剥离歧义。**本批坐实可用**。
> L20 已实测不可用的两条：`Start-Process -FilePath <installer> -Wait`、`Start-Process cmd.exe '/c','start /B ...'`（PS 5.1 下永不返回 / 卡 60 s）。

silent 分支既有行为已读实并确认安全：`:418 IfSilent uninst 0`（跳过升级确认框，不会 Abort）· `:1187-1192`（静默走 `WeaselDeployer.exe /deploy`）· `:1272-1275`（静默把 `CheckForUpdates` 写 `0`）· `:1299`（静默跳过「已安装」提示）· `:322`（系统版本不满足 ⇒ 静默 `Quit`）。

### 3.2 磁盘 + 运行中服务（已验证）

装机目录 `D:\Program Files\fluxing\weasel\`：

| 文件 | 字节 | FileVersion | mtime |
|---|---|---|---|
| `WeaselServer.exe` | 2 780 672 | **1.3.6.0** | 11-20 15:57:06 |
| `WeaselDeployer.exe` | 1 004 544 | 1.3.6.0 | 11-20 15:57:10 |
| `WeaselSetup.exe` | 287 232 | 1.3.6.0 | 11-20 15:57:12 |
| `uninstall.exe` | 138 884 | 1.3.6 | 11-20 16:42:17 |
| `weasel.1.3.6.0.0.dll` | 1 676 800 | 1.3.6.0 | 11-20 15:56:58 |
| `weaselx64.1.3.6.0.0.dll` | 1 988 608 | 1.3.6.0 | 11-20 15:56:50 |
| `weasel.dll`（规范名） | 1 676 288 | **1.3.4.0** | 11-19 22:48:38 |
| `weaselx64.dll`（规范名） | 1 987 072 | **1.3.4.0** | 11-19 22:48:30 |
| `rime.dll` | 3 040 768 | 无版本资源 | 10-25 20:35:32 |
| `FluxingPhrasesDialog.exe` | 1 098 752 | — | 11-19 10:10:30 |

**运行中服务 = 唯一实例**：`pid=173132 start=11-20 16:42:20 ver=1.3.6.0 path=D:\Program Files\fluxing\weasel\WeaselServer.exe`；旧 `pid=158212`（1.3.5.0，11:32:18 起）已不在。

⇒ **旧 PPL 服务并未挡住在线覆盖与重启**。`WeaselServer.exe` 是 PPL（project-knowledge:478-479，`taskkill` 禁用，只能 `/quit`），但安装器不杀它也能完成换件并由新映像接管 —— 这是对 AX/AY「装机判据必须等注销/重启」这条前提的**实测修正**：重启现在**只为已运行的 TSF 宿主**。

### 3.3 TSF 注册真值（已验证，本批最关键）

TIP CLSID `{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}`（真值来自 `install.nsi:1154/1163`）：

```
HKLM:\SOFTWARE\Classes\CLSID\{A3F4CDED-...}\InprocServer32  (default)
  = D:\Program Files\fluxing\weasel\weaselx64.1.3.6.0.0.dll      ThreadingModel = Apartment
HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID\{A3F4CDED-...}\InprocServer32  (default)
  = D:\Program Files\fluxing\weasel\weasel.1.3.6.0.0.dll         ThreadingModel = Apartment
HKLM:\SOFTWARE\Microsoft\CTF\KnownClasses  [{A3F4CDED-...}] = "Fluxing Text Service"
HKCU:\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}
  Default = {A3F4CDED-...}   Profile = {A3F4CDED-...}   KeyboardLayout = 0x08040804
```

两个版本号 shim 回读 `FileVersion = 1.3.6.0` ⇒ **新打开的应用即刻加载 1.3.6 shim，无需重启电脑**（与 `install.nsi:1300` 收尾文案口径一致）。

**规范名 `weasel.dll` / `weaselx64.dll` 停在 1.3.4.0 不是回归**：`install.nsi:735-753`（weasel.dll）与 `:825-845`（weaselx64）在规范名被宿主锁定时走降级路径 —— 以 `weasel.${FLUXING_VERSION}.${WEASEL_BUILD}.${FluxingShimSeq}.dll` 落盘新件（本机 `WEASEL_BUILD=0`、`FluxingShimSeq=0`）、改写 `$FluxingWeaselDll` / `$FluxingWeaselX64Dll`，并由 `:1119-1130` **注册真实落盘的 shim**；`:758` 的 `Delete weasel.dll.old.tmp` 位于 rename 成功分支 ⇒ 本轮目录下 `.old.tmp`（均 1.3.2.0）残留，说明降级路径被走过，但注册目标正确。

Stage 2 无残留：`*.stage2` marker **无**；`schtasks /Query /TN FluxingStage2Install` 及 `...X64` 均 `ERROR: The system cannot find the file specified.` ⇒ **无待开机换件**。触发 Stage2 只由 marker 决定（`:1210` `WeaselServer.exe.stage2` / `:1243` `weaselx64.dll.stage2`），不由 `.old.tmp` 存在与否决定（L120 明确）。

`%TEMP%\fluxing-staged\` 里有 `weasel.dll` 1 676 800 / `WeaselServer.exe` 2 780 672 / `weaselx64.dll` 1 988 608（= 新载荷）—— 系 `install.nsi:662-671` **无条件预置**的兜底源，**非残留故障**。

### 3.4 注册表与用户数据（已验证，未被破坏）

- `HKLM:\SOFTWARE\Fluxing\Weasel`（64 位视图）**不存在**
- `HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel\InstallDir` = `D:\Program Files\fluxing` ✓
- `HKCU:\SOFTWARE\Fluxing\Weasel\RimeUserDir` = `D:\Program Files\fluxing\user1\fluxing` **未变** ⇒ 用户数据指针完好
- project-knowledge:99-102 两条绝对禁止均未触碰：未往 `C:\Program Files\fluxing\` 写数据、未改 `D:\Program Files\fluxing\user1\`

---

## 4. 判据口径（本批固化）

| 目的 | ✅ 正确判据 | ❌ 假判据（勿用） |
|---|---|---|
| 「新进程是否用新 shim」 | 读 `InprocServer32`（64 视图 + `WOW6432Node`）指向的文件，再回读**那个文件**的 `FileVersion` | 看规范名 `weasel.dll` 的版本（可能长期落后，被宿主锁定） |
| 「服务端是否新码」 | `Get-Process WeaselServer` 的 `MainModule.FileVersionInfo.FileVersion` + `StartTime` | 磁盘哈希（只能证「装到了盘上」） |
| 「包内是否含某改动」 | 搜**解压载荷** `output\Win32\WeaselServer.exe` 的令牌（ASCII + UTF-16LE） | 在 42 MB `installer.exe` 里搜文本（压缩容器 ⇒ 只能证真不能证伪，AY 实测） |
| 「闸门是否放行」 | 维护导出文件 mtime 前进（`...\sync\adbd9f3b-.../rime_ice.userdb.txt`） | 账号页状态文案（自绘 + 刷新滞后） |
| 「装机是否成功」 | `INSTALL_EXIT=0`（权威） | 探针脚本整体 exit code（`schtasks` 未命中即 `NativeCommandError` 把整脚本判 1） |

---

## 5. 本轮踩坑（登记，勿重犯）

1. **三处取证用了错键 ⇒ 结论无效，不得当 FAIL**：凭记忆猜 TIP GUID（`{E6D0C586-...}`）、按 `Fluxing|Weasel` 名字过滤 `HKLM:\SOFTWARE\Classes\CLSID`（真 GUID 不含这两个词）、读 `...\CurrentVersion\Uninstall\Weasel` —— 三处全部空输出。真值一律回 `install.nsi` grep 取。
2. `cmd /c` 引号剥离歧义 ⇒ 用**独立 `.cmd` 文件**承载命令，实测 41 s 干净返回。
3. `schtasks /Query` 未命中让脚本整体 `exit code = 1`（即使 `$ErrorActionPreference='Continue'`）⇒ 不影响判读，但别误当装机失败。
4. `/NCMS` 是**上一轮的摘记错误**，`install.nsi` 与 `project-knowledge.md` 都无此串。
5. `grep` 不带 `output_mode='content'` 只回文件名 ⇒ 取行号必须显式 `output_mode="content"`。

---

## 6. 未闭环（按优先级）

**须用户在场（重开应用后，按 [`docs/release/restart-checklist-1.3.6.md`](../../release/restart-checklist-1.3.6.md)）**
1. §1 A-1–A-5 账号页换钥提示是否被裁（`h=36` / 窗高 290 是保守取值，**折行宽度未实测**）
2. §2 B-1/B-2/B-3 ADR 0067 孤儿 IPC 会话回收**端到端**（宿主崩溃 ⇒ 会话被回收、presence 不被误翻）
3. 已运行 TSF 宿主是否仍在旧 shim 映像 —— 本批**未逐 pid 枚举模块**验证，只证了注册目标

**环境受限 / 需授权**
4. `TestWeaselIPC.exe /verify-client-pid`（IPC 管道名全局固定 `\\.\pipe\<user>\WeaselNamedPipe` ⇒ 真服务端在跑时 mock 抢不到 ⇒ 须停服窗口：`WeaselServer.exe /quit` → 跑（期望 EXIT=0）→ 重新拉起）
5. 双机 AC 7 条（AC-03/04/05/08/09/13/14）+ restore 端到端 —— `CloudCredentialStore` 是 Windows 用户级全局单例 ⇒ 两套 Profile 目录不够，需第二台设备
6. 输入延迟 1.3.4 / 1.3.5 两组对照；缺陷 A 反向判据 G-3（打字 10 s 内应拦）/ G-5（组合串 60 s 内应拦），见 `docs/release/regression-checklist-cloud-sync.md:32/34/37/99/107`
7. 亲手登录 `kizemo@163.com`、重启后验自动续期、按 exe 路径防火墙出站阻断验断网、点「清空云端数据」后回查 D1 + R2

**推断未坐实**
8. `build.bat weasel installer` 的 `RELEASE_BUILD_EXIT=` 行未读回（混合编码日志）⇒ exit=0 仍登记为**推断**
9. 本批文档改动**未提交**（待用户授权 push；仓库代理配置走不通直连，AY 实测）

---

## 7. 证据路径

- 装机脚本：`E:\Users\Duanyi\AppData\Local\Temp\fluxing-ay-install-136.cmd`
- 包：`output/archives/fluxing-1.3.6-installer.exe`（42 951 049 B · sha256 `47909B9174D304838C0A21A7383441D1C0033B75B750F9DD290BE369A54B7ED1`）
- 载荷：`output/Win32/WeaselServer.exe`（2 780 672 B · 1.3.6.0 · sha16 `3CF15A582B2453F1`）
- 装机目录：`D:\Program Files\fluxing\weasel\`；用户数据 `D:\Program Files\fluxing\user1\`（**不得改**）
- 闸门代理判据：`D:\Program Files\fluxing\user1\fluxing\sync\adbd9f3b-d5b9-4dfc-9929-85eac8509db6\rime_ice.userdb.txt`
- 诊断日志：`%TEMP%\fluxing-profile-notify.log`（`[acctdlg]` 通道，AX 起改接落盘）
- 口径真源：`output/install.nsi` · `.specify/memory/lessons-learned.md`（L13=687-716 · L09 pitfall 5=372-400 · L20=1205-1252 · L112=10659-10700 · L120/L126-I=9115）· `.specify/memory/project-knowledge.md`（§10 NSIS 陷阱=537-544 · 绝对禁止=99-102 · PPL=478-479 · 重装配方=567-570）
- 代码锚点：`RimeWithWeasel.cpp`（`_RememberOwnerPid:1170` / `_IsProcessAlive:1191` / `_ReclaimOrphanSessions:1216` / `_RemoveSessionInternal(ipc_id,true):1284` / `kClientPidKey:1111`）· `include/FluxingSessionReclaim.h:104/114`（**须全 ASCII**）· `WeaselServer/Cloud/CloudAccountUi.h`(`kIdKeyRotationNote=1035`) · `CloudAccountDialog.cpp:953-957`
- 上一批：[`handoff-2026-11-20-batch-ay-release-1-3-6.md`](handoff-2026-11-20-batch-ay-release-1-3-6.md)
