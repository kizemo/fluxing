# 071 - Plan

## Constitution Check

| 原则 | 状态 | 说明 |
|---|---|---|
| I - TDD | ✓ | 5 修复每项加 test (FR-001 走 `_check_system32_cleanup.ps1` 升级;FR-002 new `TestOpenccDataPath`;FR-003 升级 `TestInstallerShimStage`;FR-004 new `TestSilentAbortDetailPrint`;FR-005 new `TestReadyFlagMessageBox`) |
| II - Local-First | ✓ | 无 push;仅 sftp release 时同步 |
| III - Conventional Commits | ✓ | 单一 commit `fix(installer): v0.74.1 freeze mitigation 9.6-A glob + emoji.json path + F4 schtasks + silent abort + Ready-flag UX`, 或拆 2 commit (A+B 一, C+D+E 一) 看 diff size |
| P4 - 安全 + NSIS WriteRegStr safety net | ✓ | 5 修复无新 `WriteReg*`,仅 cleanup + SetOutPath + DetailPrint / MessageBox |
| P5 - 中文 + 用户可见字符串 | ✓ | MessageBox / DetailPrint / Task name / Stage2 batch log 全中文 (`/C:`) |
| R5 单任务 ≤ 4h / 1-3 files / 有 I/O | ✓ | tasks.md 每条 < 4h,大多数 < 30 min,1-3 files |
| R2 spec.md tech-agnostic | ✓ | spec.md 仅描述 WHAT,no WTL/Boost/C++/WeaselTSF/cpp/h |

## Tech approach

5 修复都涉及 `install.nsi` (NSIS 3.x) + `stage2-shim-install.bat.cmd` (cmd batch)
+ ship-gate scripts (PowerShell)。直接增量编辑 `.nsi` 与 `.cmd`,不重写。

### FR-001 9.6 cleanup glob 扩展

**算法** (NSIS 没有 glob,用 `FindFirst` / `FindNext` loop):

```nsi
FindFirst $R0 $R1 "$WINDIR\System32\weasel*.dll"
${If} $R0 != ""
  loop_weasel_dll:
    ${If} ${FileExists} "$WINDIR\System32\$R1"
      ; size check: > 1MB 才删(防御 Windows built-in 同前缀小 DLL)
      ${GetSize} "$WINDIR\System32\$R1" "S" $R2 $R3
      ${If} $R2 > 1000000
        Delete /REBOOTOK "$WINDIR\System32\$R1"
        DetailPrint "Fluxing v0.74.1: cleaned $WINDIR\System32\$R1 ($R2 bytes)"
      ${EndIf}
    ${EndIf}
    FindNext $R0 $R1
    ${If} $R0 != ""
      Goto loop_weasel_dll
    ${EndIf}
${EndIf}
FindClose $R0

; 同样 pattern for weasel*.ime
FindFirst $R0 $R1 "$WINDIR\System32\weasel*.ime"
... (same loop)
FindClose $R0
```

- `.onInit` (line 240-247) 跟 `Section Uninstall` (line 1078-1085) 镜像替换
- `output/uninstall.nsi` 同步 mirror

### FR-002 opencc 数据路径补装

最小改动 (避免破坏 `$INSTDIR\data\` 默认路径):在 line 818 之后新增一段:

```nsi
SetOutPath $INSTDIR\weasel\data\opencc
File "data\opencc\*.json"
File "data\opencc\*.ocd*"
SetOutPath $INSTDIR
```

- uninstall 端 (line 1062) 增加 `Delete "$INSTDIR\weasel\data\opencc\*.*"` mirror cleanup
- 两个路径都保留,backwards compat 不破坏

### FR-003 5 个 unique schtasks

**算法**:

```nsi
; pre-install defensive cleanup (无论存在与否)
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap /F 2>nul'
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weasel /F 2>nul'
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_weaselx64 /F 2>nul'
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_server /F 2>nul'
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_deployer /F 2>nul'
nsExec::ExecToStack 'schtasks /Delete /TN FluxingStage2ShimSwap_setup /F 2>nul'

; weaselx64 stage block:
FileWrite $R8 'weaselx64.dll$\r$\n'
FileClose $R8
CopyFiles /SILENT /FILESONLY "$INSTDIR\stage2-shim-install.bat.cmd" "$TEMP\fluxing-stage2-shim\stage2-shim-install.bat.cmd"
nsExec::ExecToStack 'schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_weaselx64 /TR "cmd /c set SHIM=weaselx64 && $\"$TEMP\fluxing-stage2-shim\stage2-shim-install.bat.cmd$\"" /RL HIGHEST /F'
StrCpy $FluxingStage2Needed "1"
```

stage2 batch 改:

```bat
rem Stage2-shim-install.bat.cmd — 单 shim 操作
set "SHIM=%SHIM%"
if "%SHIM%"=="" set "SHIM=%~1"  ; 支持 arg 形式
if "%SHIM%"=="" (
    echo [%DATE% %TIME%] FATAL: SHIM not set >> "%LOG%"
    exit /b 2
)

set "PENDING=%STAGE_DIR%\%SHIM%.json"  ; 每个 shim 独立 json
... (swap logic for FILE=%SHIM% only)
```

5 个 stage blocks 各 set 自己的 SHIM,每个 task name 不同,boot 时 5 任务并行
跑(各自独立 lock release 等待,no race on file)。

### FR-004 silent Abort + DetailPrint

line 1003-1006 替换为:

```nsi
IfSilent 0 +6
  StrCmp $FluxingStage2Needed "1" 0 skip_post_install_message
  DetailPrint "Fluxing v0.74.1: deferred shim swap pending; silent exit 6001 (sandbox/CI detect reboot required)"
  ; 双保险 file log 落盘 (R-004 缓解)
  FileOpen $9 "$INSTDIR\fluxing-install-silent.log" a
  FileWrite $9 "Fluxing v0.74.1: deferred shim swap pending; reboot required$\r$\n"
  FileClose $9
  SetErrorLevel 6001
  Quit
```

### FR-005 Ready-flag MessageBox (non-silent only)

line 970-974 内 wrap:

```nsi
${If} $0 >= 30
  ; silent 路径走 silent safe path
  IfSilent skip_warning_ready
    MessageBox MB_OK|MB_ICONWARNING "火流猩输入法安装完成,但 WeaselServer 没有在 30s 内 Ready。$\r$\n$\r$\n请检查 $\r$\n  C:\Users\<user>\AppData\Local\Temp\rime.weasel\ 中的 log 以诊断。$\r$\n$\r$\n这通常意味着 rime engine 仍在后台初始化,请等待几秒后切输入法。如果问题持续,请重启 WeaselServer.exe。"
  skip_warning_ready:
  DetailPrint "Fluxing v0.74.1: WeaselServer did not signal Ready within 30s"
  ${ExitDo}
${EndIf}
```

## File-level changes (exact paths)

| Path | 类型 | 估算 diff |
|---|---|---|
| `output/install.nsi` | modify 5 sections (.onInit / Section Fluxing / pre-install cleanup / Ready-flag / silent abort) | ~80 lines |
| `output/stage2-shim-install.bat.cmd` | add SHIM arg parsing + per-shim pending.json | ~25 lines |
| `output/uninstall.nsi` | mirror 9.6 glob 扩展 | ~40 lines |
| `_check_system32_cleanup.ps1` | extend anchor list (weasel*.ime + 老 backup 模式) | ~10 lines |
| `test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` | 加 FR-003 5 unique schtasks test | ~30 lines |
| `test/TestOpenccDataPath/test.ps1` (new) | 验证 emoji.json 路径 | ~50 lines |
| `test/TestSilentAbortDetailPrint/test.ps1` (new) | 验证 silent exit 6001 + DetailPrint | ~60 lines |
| `test/TestReadyFlagMessageBox/test.ps1` (new) | 验证 MessageBox on 30s timeout | ~50 lines |
| `docs/adr/0003-installer-shim-stage-at-startup.md` | amend schtasks race + glob cleanup | ~40 lines |
| `.specify/memory/lessons-learned.md` | add L##-PhaseL-9.7 + L##-PhaseL-9.8 | ~80 lines |
| `env.bat` + `weasel.props` | bump v0.74 → v0.74.1 | 2 lines |
| `output/archives/fluxing-0.19.0.74.1-installer.exe` | new build | ~43 MB |

总 diff ≈ 13 files,~ 500-600 lines (含 tests + lessons), 处于 R5 ≤ 4h 单任务上限内。

## Build / Test / Release impact

### Build (CLAUDE.md L10 §6: 必须 `RELEASE_BUILD=1` + 禁 `git describe --tags`)

1. `env.bat RELEASE_BUILD=1` 显式 set 强制 release mode
2. `_buildflow.cmd` (L108 fix: rebuild from scratch,不依赖增量)
3. `_nsis_only.cmd` rebuild installer after `output/install.nsi` 修改
4. 5 binary md5 ≠ v0.74 (L17 ship gate)
5. TestWeaselServerStartup + 4 新 tests PASS

### Test

1. `pwsh -File _check_system32_cleanup.ps1 -Mode Audit` PASS (0 FAIL)
2. `pwsh -File _check_system32_cleanup.ps1 -Mode Live` 真机 PASS
3. `pwsh -File test/TestInstallerShimStage/Test-Stage2ShimInstall.ps1` PASS (11/11)
4. `pwsh -File test/TestOpenccDataPath/test.ps1` 真机装后 grep log = 0
5. `pwsh -File test/TestSilentAbortDetailPrint/test.ps1` PASS (silent path)
6. `pwsh -File test/TestReadyFlagMessageBox/test.ps1` 真机 FAIL → MESSAGEBOX 检验

### Release (CLAUDE.md P4 + AGENTS.md §3)

1. 5 binary md5 ≠ v0.74 (L17 ship gate)
2. `git status -sb` + `git diff --stat` clean (avoid weasel.props / env.bat miscommit,CLAUDE.md §2)
3. Conventional commit: `fix(installer): v0.74.1 freeze mitigation 9.6-A glob + emoji.json path + F4 schtasks + silent abort + Ready-flag UX`
   (或拆 2 commits: A+B / C+D+E, 看 diff size)
4. **NO push** (P3 等 Task #6 PASS + 网络恢复)
5. sftp atomic upload → `www.aiec.fun/pinyin/` (走 `feedback_pinyin_release_workflow.md`)
6. CHANGELOG.md bump

## Risk mitigation

详见 spec.md R-001 ~ R-005。在 plan 这层执行细节:

| 风险 | 实施动作 |
|---|---|
| R-001 glob 误伤 | size > 1 MB 阈值 + DetailPrint 列出每个 candidate |
| R-002 data path mirror | uninstall 段 mirror 双路径清理 |
| R-003 schtasks race | pre-install 6 次 schtasks /Delete (含老 task name 防御)|
| R-004 DetailPrint 异步 | 同步 FileWrite 到 `$INSTDIR\fluxing-install-silent.log` 落盘备份 |
| R-005 MessageBox 阻塞 silent | IfSilent skip_warning jump 跳到 silent-safe path |
