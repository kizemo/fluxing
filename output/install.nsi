; weasel installation script
; Attribution: built on the upstream RIME / 中州韻輸入法引擎 (https://rime.im/)
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include x64.nsh
!include winVer.nsh
; v0.19.0.37.1 hotfix: nsExec::ExecToStack 是 NSIS 标准 plugin (Plugins/nsExec.dll),
;   无需 !include, 直接调用。

Unicode true

;--------------------------------
; General

!ifndef FLUXING_VERSION
!define FLUXING_VERSION 0.17.5
!endif

!ifndef WEASEL_BUILD
!define WEASEL_BUILD 0
!endif

!ifndef WEASEL_ROOT
!define WEASEL_ROOT $INSTDIR\weasel
!endif
!define FLUXING_ROOT $INSTDIR\fluxing
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing"

; The name of the installer
Name "Fluxing ${FLUXING_VERSION}"

; The file to write
OutFile "archives\fluxing-${FLUXING_VERSION}.${WEASEL_BUILD}-installer.exe"

VIProductVersion "${FLUXING_VERSION}.${WEASEL_BUILD}"
VIAddVersionKey /LANG=2052 "ProductName" "火流猩输入法"
VIAddVersionKey /LANG=2052 "Comments" "Powered by Fluxing & RIME"
VIAddVersionKey /LANG=2052 "CompanyName" "aiec.fun"
VIAddVersionKey /LANG=2052 "LegalCopyright" "Copyleft RIME Developers"
VIAddVersionKey /LANG=2052 "FileDescription" "火流猩輸入法"
VIAddVersionKey /LANG=2052 "FileVersion" "${FLUXING_VERSION}"

!define MUI_ICON ..\resource\weasel.ico
SetCompressor /SOLID lzma


; Request application privileges for Windows Vista
RequestExecutionLevel admin

;--------------------------------

; Pages

!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE "ForceFluxingSuffix"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

;--------------------------------

; Languages

!insertmacro MUI_LANGUAGE "TradChinese"
LangString DISPLAYNAME ${LANG_TRADCHINESE} "火流猩輸入法"
LangString LNKFORMANUAL ${LANG_TRADCHINESE} "【火流猩輸入法】說明書"
LangString LNKFORSETTING ${LANG_TRADCHINESE} "【火流猩輸入法】輸入法設定"
LangString LNKFORDICT ${LANG_TRADCHINESE} "【火流猩輸入法】用戶詞典管理"
LangString LNKFORSYNC ${LANG_TRADCHINESE} "【火流猩輸入法】用戶資料同步"
LangString LNKFORDEPLOY ${LANG_TRADCHINESE} "【火流猩輸入法】重新部署"
LangString LNKFORSERVER ${LANG_TRADCHINESE} "火流猩輸入法 算法服務"
LangString LNKFORUSERFOLDER ${LANG_TRADCHINESE} "【火流猩輸入法】用戶文件夾"
LangString LNKFORAPPFOLDER ${LANG_TRADCHINESE} "【火流猩輸入法】程序文件夾"
LangString LNKFORUPDATER ${LANG_TRADCHINESE} "【火流猩輸入法】檢查新版本"
LangString LNKFORSETUP ${LANG_TRADCHINESE} "【火流猩輸入法】安裝選項"
LangString LNKFORUNINSTALL ${LANG_TRADCHINESE} "卸載火流猩輸入法"
LangString CONFIRMATION ${LANG_TRADCHINESE} "安裝前，請先卸載舊版本的火流猩輸入法。$\n$\n按下「確定」移除舊版本，按下「取消」放棄本次安裝。"
LangString SYSTEMVERSIONNOTOK ${LANG_TRADCHINESE} "您的系统不被支持，最低系統要求:Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_TRADCHINESE} "自動檢查版本更新？"

!insertmacro MUI_LANGUAGE "SimpChinese"
LangString DISPLAYNAME ${LANG_SIMPCHINESE} "火流猩输入法"
LangString LNKFORMANUAL ${LANG_SIMPCHINESE} "【火流猩输入法】说明书"
LangString LNKFORSETTING ${LANG_SIMPCHINESE} "【火流猩输入法】输入法设定"
LangString LNKFORDICT ${LANG_SIMPCHINESE} "【火流猩输入法】用户词典管理"
LangString LNKFORSYNC ${LANG_SIMPCHINESE} "【火流猩输入法】用户资料同步"
LangString LNKFORDEPLOY ${LANG_SIMPCHINESE} "【火流猩输入法】重新部署"
LangString LNKFORSERVER ${LANG_SIMPCHINESE} "火流猩输入法 算法服务"
LangString LNKFORUSERFOLDER ${LANG_SIMPCHINESE} "【火流猩输入法】用户文件夹"
LangString LNKFORAPPFOLDER ${LANG_SIMPCHINESE} "【火流猩输入法】程序文件夹"
LangString LNKFORUPDATER ${LANG_SIMPCHINESE} "【火流猩输入法】检查新版本"
LangString LNKFORSETUP ${LANG_SIMPCHINESE} "【火流猩输入法】安装选项"
LangString LNKFORUNINSTALL ${LANG_SIMPCHINESE} "卸载火流猩输入法"
LangString CONFIRMATION ${LANG_SIMPCHINESE} '安装前，请先卸载旧版本的火流猩输入法。$\n$\n点击 "确定" 移除旧版本，或点击 "取消" 放弃本次安装。'
LangString SYSTEMVERSIONNOTOK ${LANG_SIMPCHINESE} "您的系統不被支持，最低系统要求:Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_SIMPCHINESE} "自动检查版本更新？"

!insertmacro MUI_LANGUAGE "English"
LangString DISPLAYNAME ${LANG_ENGLISH} "Fluxing"
LangString LNKFORMANUAL ${LANG_ENGLISH} "Fluxing Manual"
LangString LNKFORSETTING ${LANG_ENGLISH} "Fluxing Settings"
LangString LNKFORDICT ${LANG_ENGLISH} "Fluxing Dictionary Manager"
LangString LNKFORSYNC ${LANG_ENGLISH} "Fluxing Sync User Profile"
LangString LNKFORDEPLOY ${LANG_ENGLISH} "Fluxing Deploy"
LangString LNKFORSERVER ${LANG_ENGLISH} "Fluxing Server"
LangString LNKFORUSERFOLDER ${LANG_ENGLISH} "Fluxing User Folder"
LangString LNKFORAPPFOLDER ${LANG_ENGLISH} "Fluxing App Folder"
LangString LNKFORUPDATER ${LANG_ENGLISH} "Fluxing Check for Updates"
LangString LNKFORSETUP ${LANG_ENGLISH} "Fluxing Installation Preference"
LangString LNKFORUNINSTALL ${LANG_ENGLISH} "Uninstall Fluxing"
LangString CONFIRMATION ${LANG_ENGLISH} "Before installation, please uninstall the old version of Fluxing.$\n$\nPress 'OK' to remove the old version, or 'Cancel' to abort installation."
LangString SYSTEMVERSIONNOTOK ${LANG_ENGLISH} "Your system not supported, minimium system required: Windows 8.1!"
LangString AUTOCHKUPDATE ${LANG_ENGLISH} "Automatically check for updates?"


Function .onInit
  ; L13 fix: pre-install cleanup. Force-kill any zombie WeaselServer.exe
  ; BEFORE any other NSIS logic. This is needed because:
  ;  (1) the polite /quit in call_uninstaller may hang on a crashed process
  ;  (2) the user may be running with non-elevated PowerShell where taskkill fails
  ;      with 'Access is denied' (zombie owned by SYSTEM / admin). NSIS is
  ;      RequestExecutionLevel admin (see header), so the in-installer taskkill
  ;      has the right to kill those zombies.
  ;  (3) We do this as the very first line so a fresh install (no prior version)
  ;      also benefits: if some prior install left a WeaselServer zombie, it
  ;      gets cleaned before any file copy is attempted.
  ;  (4) /T also kills child processes spawned by WeaselServer (e.g. fluxing panel).
  ; Phase A.11 (L100+): retry taskkill up to 3 times with 2s delay. This handles
  ; the autorun-respawn race: if a WeaselServer.exe restarts itself between
  ; taskkill and File commands, the next taskkill catches the new instance.
  ; Without retries, the File command silently fails on a locked mmap and
  ; the old binary is retained (user sees no change after upgrade).
  ;
  ; Phase D (v0.19.0.36): extend to 5 retries with escalating backoff (1s/2s/3s/5s).
  ; v0.19.0.35 user report: "still 没改进" even after 3x retry + L72-fix.
  ; Root cause (subagent verify): autorun respawn can happen 3+ times in quick
  ; succession on slow machines when Windows Defender / SearchUI is enumerating
  ; HKLM Run key. 5x retry covers up to ~13s of respawn churn.
  ;
  ; v0.19.0.48 (Phase J installer 加固): WeaselServer.exe 是 PPL 进程,
  ;   taskkill 表面成功但 mmap handle 不释放。10x retries (覆盖 ~30s) + final
  ;   Sleep 3000ms 让 OS 完整释放 mmap 后再进入 File section。
  ${For} $R9 1 10
    nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe /T'
    Pop $0
    Pop $1
    ${If} $R9 == 1
      Sleep 1500
    ${ElseIf} $R9 < 5
      Sleep 2500
    ${ElseIf} $R9 == 5
      Sleep 3000
    ${Else}
      Sleep 3000
    ${EndIf}
  ${Next}

  ; Phase D (v0.19.0.36) Reinforcement E: 老 binary 路径 force delete.
  ; Root cause: even after 5x taskkill, autorun respawn can land a fresh
  ; WeaselServer.exe before our taskkill (race window). When that happens,
  ; the just-killed process still has the mmap file handle pending delete,
  ; so NSIS `File` command silently fails on the locked .exe.
  ; Defense: pre-delete the old .exe at every known Fluxing / RIME legacy
  ; install path using Delete /REBOOTOK. If the file is locked, Windows
  ; will delete it on next boot. We also scan the registry HKLM\Software
  ; \Fluxing\Weasel\InstallDir for the user's actual install location
  ; (in case it is non-default), and force-delete that too.
  StrCpy $R8 "$PROGRAMFILES64\Fluxing\weasel\WeaselServer.exe"
  IfFileExists $R8 0 +2
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of $R8 on reboot (locked)"
  StrCpy $R8 "$PROGRAMFILES32\Fluxing\weasel\WeaselServer.exe"
  IfFileExists $R8 0 +2
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of $R8 on reboot (locked)"
  StrCpy $R8 "D:\Program Files\Fluxing\weasel\WeaselServer.exe"
  IfFileExists $R8 0 +2
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of $R8 on reboot (locked)"
  StrCpy $R8 "C:\Program Files\Rime\weasel\WeaselServer.exe"
  IfFileExists $R8 0 +2
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of legacy $R8 on reboot (locked)"
  StrCpy $R8 "C:\Program Files\WeaselServer\weasel\WeaselServer.exe"
  IfFileExists $R8 0 +2
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of legacy $R8 on reboot (locked)"
  ReadRegStr $R8 HKLM "Software\Fluxing\Weasel" "InstallDir"
  StrCmp $R8 "" skip_oldpath_delete
  StrCpy $R8 "$R8\WeaselServer.exe"
  IfFileExists $R8 0 skip_oldpath_delete
    Delete /REBOOTOK $R8
    DetailPrint "Fluxing: scheduled delete of registered $R8 on reboot (locked)"
  skip_oldpath_delete:

  ; L71-bugfix: ctfmon.exe + TextInputHost.exe 是 TSF 宿主进程,加载 weasel.dll
  ; 作为 32-bit TSF TextInputProcessor(被 notepad/VSCode 等 32-bit 进程加载)。
  ; L13 fix 只 kill WeaselServer.exe, 但 ctfmon/TextInputHost 持有的 weasel.dll
  ; mapped handle 让 NSIS File "weasel.dll" 报"无法打开"错误。
  ; 修:在 .onInit 一并 kill 这两个进程,确保安装时 weasel.dll 没有 mapped handle。
  ; 用户重新登录后 ctfmon.exe + TextInputHost.exe 会被系统自动重启,无副作用。
  ; Phase A.11: retry — sometimes TSF host re-spawns after taskkill.
  ;
  ; v0.19.0.48 (Phase J installer 加固): 5x retries + Sleep 2500ms 让 TSF host
  ;   respawn 收敛后再进入 File section。
  ${For} $R9 1 5
    nsExec::ExecToStack 'taskkill /F /IM ctfmon.exe /T'
    Pop $0
    Pop $1
    nsExec::ExecToStack 'taskkill /F /IM TextInputHost.exe /T'
    Pop $0
    Pop $1
    Sleep 2500
  ${Next}

  ; L14: NSIS has built-in support for /LOG=<file> CLI flag. Users can pass
  ; /LOG=path\to\file.log to NSIS directly to get a full install log - the
  ; primary post-mortem tool for debugging install failures (especially the
  ; L14 0xC000007B arch-mismatch bug). We don't need LogSet (unavailable in
  ; standard NSIS); the CLI flag is the conventional way. See:
  ; https://nsis.sourceforge.io/Docs/Chapter4.html#flags

  ; if not version >= 8.1, quit and MessageBox(if not silent)
  ${IfNot} ${AtLeastWin8.1}
    IfSilent toquit
    MessageBox MB_OK '$(SYSTEMVERSIONNOTOK)'
toquit:
    Quit
  ${EndIf}
  ; L13: detect existing install for upgrade (preserves the user chosen location).
  ; If registry has a prior InstallDir, USE it as the install path (upgrade in place).
  ; L13-fix-2: reject registry paths under known smoke-test roots
  ; (C:\TEMP\, C:\TEMP\test\, C:\Users\test\). These get left behind by
  ; silent-install smoke tests (AGENTS.md §2.5) because uninstall does
  ; not clear HKLM\Software\Fluxing\Weasel\InstallDir. The install-side
  ; guard catches the test path and falls through to default. The matching
  ; uninstall-side fix is tracked as spec 012 C1.
  ReadRegStr $R0 HKLM "Software\Fluxing\Weasel" "InstallDir"
  StrCmp $R0 "" 0 check_reg
  ReadRegStr $R0 HKLM "Software\Rime\Weasel" "InstallDir"
  StrCmp $R0 "" 0 check_reg
  ; No prior install: fall through to default (or /D= if user provided one).
  Goto set_default
check_reg:
  ; Reject registry paths left behind by AGENTS.md §2.5 smoke tests.
  ; These accumulate because uninstall does not clear
  ; HKLM\Software\Fluxing\Weasel\InstallDir. Most-specific prefix first
  ; (longest match wins), then the catch-all, then accept the path.
  ; L17: order matters - if check_reg2 (shorter) ran first,
  ; check_reg3 (longer) would be unreachable dead code.
  ; L17b: StrCpy length N must equal the literal length.
  ; L17c: NSIS InstallDirRegKey directive (line ~236) pre-loads $INSTDIR
  ; from the registry BEFORE .onInit runs. So when check_reg* detects
  ; a smoke-test path, we must explicitly reset $INSTDIR to the default
  ; (otherwise set_default sees a non-empty $INSTDIR and uses the stale
  ; smoke-test value). We do this by setting $R0 to "" and $INSTDIR to
  ; the default in one shot, then jumping to skip.
  StrCpy $R1 $R0 13
  StrCmp $R1 "C:\Users\test" 0 check_reg2
  Goto use_default
check_reg2:
  StrCpy $R1 $R0 12
  StrCmp $R1 "C:\TEMP\test" 0 check_reg3
  Goto use_default
check_reg3:
  StrCpy $R1 $R0 8
  StrCmp $R1 "C:\TEMP\" 0 use_reg
  Goto use_default
use_default:
  ; Smoke-test path detected - reset $INSTDIR to default and skip the
  ; stale value. We use $PROGRAMFILES64\fluxing to match the default
  ; set_default logic below.
  StrCpy $INSTDIR "D:\Program Files\fluxing"
  StrCpy $R0 ""
  Goto skip
use_reg:
  ; spec 053 fix v3: ALWAYS use the registry value (upgrade in place).
  ; The previous logic used StrCmp $INSTDIR " 0 set_default which
  ; would either fall through to set_default (overwriting $INSTDIR with
  ; $PROGRAMFILES64\fluxing = C:\Program Files\fluxing) or preserve a
  ; stale /D= user value. The registry is the source of truth for
  ; upgrade-in-place; use $R0 to set $INSTDIR explicitly here.
  ; L17c: the use_default branch already handles the 'registry path
  ; is a smoke-test root' case (resets $INSTDIR to default), so this
  ; use_reg branch only runs when $R0 is a known-valid path.
  StrCpy $INSTDIR $R0
  Goto skip
set_default:
  ; L14: Default installation directory (only if /D= did not provide one).
  ; The installer is x86 (Win32), so $PROGRAMFILES on x64 Windows resolves to
  ; "C:\Program Files (x86)" via WOW64 redirection. We always want the real
  ; 64-bit Program Files, so use $PROGRAMFILES64 explicitly. This keeps the
  ; default stable across 32/64-bit installer versions.
  StrCmp $INSTDIR "" 0 skip_default
  StrCpy $INSTDIR "D:\Program Files\fluxing"
skip_default:
skip:
  Call ForceFluxingSuffix
  ReadRegStr $R0 HKLM \
  "Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel" \
  "UninstallString"
  StrCmp $R0 "" done

  StrCpy $0 "Upgrade"
  IfSilent uninst 0
  MessageBox MB_OKCANCEL|MB_ICONINFORMATION "$(CONFIRMATION)" IDOK uninst
  Abort

uninst:
  ; Backup data directory from previous installation, user files may exist
  ReadRegStr $R1 HKLM SOFTWARE\Rime\Weasel "WeaselRoot"
  StrCmp $R1 "" call_uninstaller
  IfFileExists $R1\data\*.* 0 call_uninstaller
  CreateDirectory $TEMP\weasel-backup
  CopyFiles $R1\data\*.* $TEMP\weasel-backup

call_uninstaller:
  ; Phase D (v0.19.0.36) Reinforcement B: guard against empty $R1.
  ; v0.19.0.35 user report: legacy install path present but WeaselRoot
  ; registry value missing/empty → ExecWait '"\WeaselServer.exe" /quit'
  ; fires with malformed path. NSIS silently exits 0 but no process is
  ; killed → file lock persists → File commands silently fail.
  ; Fix: skip the polite /quit + taskkill + /u calls when $R1 is empty;
  ; fall through to the registry / file cleanup (which is unconditional).
  StrCmp $R1 "" skip_uninst_runner
  ExecWait '"$R1\WeaselServer.exe" /quit'
skip_uninst_runner:
  ; L13 fix: force-kill any zombie WeaselServer.exe (taskkill /F).
  ; /quit is a polite request; if the process is hung / crashed / lock-held
  ; the polite exit never completes and the file lock persists. taskkill /F
  ; is the unconditional fallback. /T also kills child processes.
  nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe /T'
    Pop $0
    Pop $1
  ExecWait '"$R1\WeaselSetup.exe" /u'
  ; Remove registry keys
  DeleteRegKey HKLM SOFTWARE\Rime
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing"
  ; don't redirect on 64 bit system for auto run setting
  ${If} ${IsNativeARM64}
    SetRegView 64
  ${ElseIf} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer"
  ; recover back to 32bit view
  SetRegView 32
  ; Remove files and uninstaller
  Delete  "$R1\data\opencc\*.*"
  Delete  "$R1\data\preview\*.*"
  Delete  "$R1\data\*.*"
  Delete  "$R1\*.*"
  RMDir   "$R1\data\opencc"
  RMDir   "$R1\data\preview"
  RMDir   "$R1\data"
  RMDir   "$R1"
  SetShellVarContext all
  Delete  "$SMPROGRAMS\$(DISPLAYNAME)\*.*"
  RMDir  "$SMPROGRAMS\$(DISPLAYNAME)"
  ; Prompt reboot
  SetRebootFlag true
  Sleep 800

done:
FunctionEnd

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Fluxing\Weasel" "InstallDir"

; The stuff to install
Section "Fluxing"

  SectionIn RO

  ; Write the new installation path into the registry
  ; redirect on 64 bit system
  ; HKLM SOFTWARE\WOW6432Node\Rime\Weasel "InstallDir" "$INSTDIR"
  WriteRegStr HKLM SOFTWARE\Fluxing\Weasel "InstallDir" "$INSTDIR"

  ; Reset INSTDIR for the new version
  ; spec 002/005: save user-facing install path before $INSTDIR is reset
  StrCpy $R3 "$INSTDIR"
  StrCpy $INSTDIR "${WEASEL_ROOT}"

  ; spec 053 fix v4: CreateDirectory $INSTDIR + subdirs. SetOutPath does NOT
  ; auto-create missing parent dirs, and File commands silently fail if
  ; the parent dir does not exist. Without this, a fresh install to a
  ; non-existent C:\Program Files\fluxing\ (default for empty registry) writes
  ; InstallDir to C:\ but copies ZERO files.
  CreateDirectory $INSTDIR
  CreateDirectory $INSTDIR\data
  CreateDirectory $INSTDIR\data\cn_dicts
  CreateDirectory $INSTDIR\data\en_dicts
  CreateDirectory $INSTDIR\data\lua
  CreateDirectory $INSTDIR\data\lua\cold_word_drop
  CreateDirectory $INSTDIR\data\opencc
  CreateDirectory $INSTDIR\data\build
  CreateDirectory $INSTDIR\data\preview
  ; L13 fix: polite quit then force-kill (handles both clean + hung exit).
  ; Phase A.11: retry taskkill x3 to defeat autorun-respawn race.
  ; Phase D (v0.19.0.36) Reinforcement D: extend to 5 retries (3x→5x).
  ; v0.19.0.37.1 hotfix: REMOVED explorer.exe force-kill + restart (af13cbff
  ;   引入了, 杀 explorer 让 user 桌面 + taskbar 黑屏 2s, 而 explorer.exe
  ;   实际不是 TSF shim host, 杀它没意义。L72-fix Rename-then-File 已能
  ;   释放 file handle, 不需要杀 explorer 兜底)。
  ; taskkill 全部改 nsExec::ExecToStack (静默, 不弹 cmd 窗口)。
  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'
  ${For} $R9 1 5
    nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe /T'
    Pop $0
    Pop $1
    ${If} $R9 == 1
      Sleep 1000
    ${ElseIf} $R9 == 2
      Sleep 2000
    ${ElseIf} $R9 == 3
      Sleep 3000
    ${Else}
      Sleep 5000
    ${EndIf}
  ${Next}
  ; v0.19.0.37.1 hotfix: REMOVED explorer.exe taskkill (af13cbff Phase D
  ; Reinforcement D) — 杀 explorer.exe 让 user desktop / taskbar 黑屏
  ; 2s (commit 79d522b IME hotfix user 报告 "Windows 黑屏")。
  ; 真实 root cause: explorer.exe 不是 TSF shim host (TSF shim 是
  ; WeaselServer.exe / ctfmon.exe / TextInputHost.exe), 杀它无意义。
  ; L72-fix Rename-then-File (9b3e0824) 已经能释放 file handle, 不需要
  ; 杀 explorer 兜底。撤掉 explorer.exe taskkill + 重启段。

  SetOverwrite on
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  ; L79-fix: 装 fluxing-logo.png 到 weasel subdir(QuickPanel 找它)
  ; v0.62 ROLLBACK: fluxing-logo_small.png (L82) not yet in v0.62 source.
  ; SKIP that line to avoid NSIS abort. QuickPanel silently falls back to
  ; fluxing-logo.png (handled in QuickPanelDialog.cpp LoadLogoWIC).

  SetOutPath $INSTDIR\weasel
  File "fluxing-logo.png"
  SetOutPath $INSTDIR

  IfFileExists $TEMP\weasel-backup\*.* 0 program_files
  CreateDirectory $INSTDIR\data
  CopyFiles $TEMP\weasel-backup\*.* $INSTDIR\data
  RMDir /r $TEMP\weasel-backup

program_files:
  File "LICENSE.txt"
  File "README.txt"
  File "7-zip-license.txt"
  File "fluxing-logo.png"
  File "7z.dll"
  File "7z.exe"
  File "COPYING-curl.txt"
  File "curl.exe"
  File "curl-ca-bundle.crt"
  File "rime-install.bat"
  File "rime-install-config.bat"
  File "start_service.bat"
  File "stop_service.bat"
  ; L72-bugfix: Rename-then-File 模式。原 L71 Delete/REBOOTOK 不能立刻删除
  ; (文件还锁),导致后续 File 仍报"无法打开要写入的文件"。
  ; 新模式:Rename 是原子的,即使失败也能 SetOverwrite try 跳过
  ; (NSIS 不会弹错对话框)。当 Rename 成功时,后续 File 一定能写新文件。
  ; 当 Rename 失败(文件仍被 TSF 宿主锁)时,fallback 到 SetOverwrite try +
  ; IfErrors 跳过,装旧版,dwFlags on 还原,DetailPrint 提示用户注销重登。
  Push $R0
  Push $R1
  ${If} ${FileExists} "$INSTDIR\weasel.dll"
    ClearErrors
    Rename "$INSTDIR\weasel.dll" "$INSTDIR\weasel.dll.old.tmp"
    ${If} ${Errors}
      ; Rename 失败(还锁)→ 试 SetOverwrite try,失败就 skip
      SetOverwrite try
      File "weasel.dll"
      IfErrors 0 weasel_done
      DetailPrint "Fluxing: weasel.dll locked by TSF host; old shim retained. Log out -> log in to pick up the new shim."
      SetOverwrite on
      Goto weasel_done
    ${EndIf}
    ; Rename 成功,旧文件已经不在 $INSTDIR\weasel.dll
    File "weasel.dll"
    ; 清理 .old.tmp(用 REBOOTOK 防止正在运行时还占用)
    Delete /REBOOTOK "$INSTDIR\weasel.dll.old.tmp"
  ${Else}
    ; 全新装,直接 File
    File "weasel.dll"
  ${EndIf}
  weasel_done:
  Pop $R1
  Pop $R0

  ${If} ${RunningX64}
    ; L14-fix (spec 012 cleanup): weaselx64.dll is the 64-bit TSF TextInputProcessor.
    ; 同样 L72-bugfix Rename-then-File 模式
    Push $R0
    Push $R1
    ${If} ${FileExists} "$INSTDIR\weaselx64.dll"
      ClearErrors
      Rename "$INSTDIR\weaselx64.dll" "$INSTDIR\weaselx64.dll.old.tmp"
      ${If} ${Errors}
        SetOverwrite try
        File "weaselx64.dll"
        IfErrors 0 weaselx64_done
        DetailPrint "Fluxing: weaselx64.dll locked by TSF host; old shim retained. Log out -> log in to pick up the new shim."
        SetOverwrite on
        Goto weaselx64_done
      ${EndIf}
      File "weaselx64.dll"
      Delete /REBOOTOK "$INSTDIR\weaselx64.dll.old.tmp"
    ${Else}
      File "weaselx64.dll"
    ${EndIf}
    weaselx64_done:
    Pop $R1
    Pop $R0
  ${EndIf}
  ${If} ${IsNativeARM64}
    File /nonfatal "weaselARM.dll"
    File /nonfatal "weaselARM64.dll"
    File /nonfatal "weaselARM64X.dll"
  ${EndIf}
  ; spec 002 / L14: librime is Win32-only (output\rime.dll is x86), so all Weasel
  ; binaries must also be Win32 (x86). 0xC000007B happens when WeaselDeployer
  ; is x64 and tries to load x86 rime.dll - WoW64 is process-level, not
  ; module-level, so 32-bit DLLs cannot be loaded in 64-bit processes.

  ; L100+ (Phase A.10 fix): Apply L72-fix Rename-then-File pattern to
  ; WeaselServer.exe / WeaselDeployer.exe / WeaselSetup.exe.
  ; Root cause (per subagent root-cause analysis): when WeaselServer.exe is
  ; running (e.g. autorun respawn after .onInit taskkill), NSIS `File` silently
  ; fails (locked by mmap) — old binary retained, user sees no change after
  ; upgrade. Apply L72-fix pattern (Rename to .old.tmp first, then File, then
  ; Delete /REBOOTOK the .old.tmp) so the new binary always lands on disk.
  Push $R0
  Push $R1
  ${If} ${FileExists} "$INSTDIR\WeaselServer.exe"
    ClearErrors
    Rename "$INSTDIR\WeaselServer.exe" "$INSTDIR\WeaselServer.exe.old.tmp"
    ${If} ${Errors}
      ; Rename 失败(还锁)→ 试 SetOverwrite try,失败就 skip
      SetOverwrite try
      File "Win32\WeaselServer.exe"
      IfErrors 0 WeaselServer_done
      ; Phase D (v0.19.0.36) Reinforcement C: even on full failure,
      ; schedule Delete /REBOOTOK of the locked file so the next boot
      ; completes the swap. Without this, the user is stuck with the
      ; old binary until they manually delete + reinstall.
      Delete /REBOOTOK "$INSTDIR\WeaselServer.exe"
      DetailPrint "Fluxing: WeaselServer.exe locked; scheduled delete on reboot. Reboot to pick up new binary."
      SetOverwrite on
      Goto WeaselServer_done
    ${EndIf}
    ; Rename 成功,旧文件已经不在
    File "Win32\WeaselServer.exe"
    Delete /REBOOTOK "$INSTDIR\WeaselServer.exe.old.tmp"
  ${Else}
    ; 全新装,直接 File
    File "Win32\WeaselServer.exe"
  ${EndIf}
  WeaselServer_done:
  Pop $R1
  Pop $R0

  ; v0.19.0.52 (Phase K3 T014): out-of-process 短语 dialog exe。
  ; WeaselServer.exe (x86) 启动时通过 PhrasesDialogIPC::LaunchPhrasesDialog
  ; 在同一目录 ($INSTDIR) 调 CreateProcessW 此 binary。文件不存在 → 短语
  ; 功能静默 fail,其他功能正常。
  ; 不是 PPL 进程 → 走简单 File (无 L72-fix Rename-then-File 兜底)。dialog 是
  ; 短时 modal,install 期间被锁的概率极低。
  File "FluxingPhrasesDialog.exe"

  Push $R0
  Push $R1
  ${If} ${FileExists} "$INSTDIR\WeaselDeployer.exe"
    ClearErrors
    Rename "$INSTDIR\WeaselDeployer.exe" "$INSTDIR\WeaselDeployer.exe.old.tmp"
    ${If} ${Errors}
      SetOverwrite try
      File "Win32\WeaselDeployer.exe"
      IfErrors 0 WeaselDeployer_done
      ; Phase D (v0.19.0.36) Reinforcement C
      Delete /REBOOTOK "$INSTDIR\WeaselDeployer.exe"
      DetailPrint "Fluxing: WeaselDeployer.exe locked; scheduled delete on reboot."
      SetOverwrite on
      Goto WeaselDeployer_done
    ${EndIf}
    File "Win32\WeaselDeployer.exe"
    Delete /REBOOTOK "$INSTDIR\WeaselDeployer.exe.old.tmp"
  ${Else}
    File "Win32\WeaselDeployer.exe"
  ${EndIf}
  WeaselDeployer_done:
  Pop $R1
  Pop $R0

  Push $R0
  Push $R1
  ${If} ${FileExists} "$INSTDIR\WeaselSetup.exe"
    ClearErrors
    Rename "$INSTDIR\WeaselSetup.exe" "$INSTDIR\WeaselSetup.exe.old.tmp"
    ${If} ${Errors}
      SetOverwrite try
      File "WeaselSetup.exe"
      IfErrors 0 WeaselSetup_done
      ; Phase D (v0.19.0.36) Reinforcement C
      Delete /REBOOTOK "$INSTDIR\WeaselSetup.exe"
      DetailPrint "Fluxing: WeaselSetup.exe locked; scheduled delete on reboot."
      SetOverwrite on
      Goto WeaselSetup_done
    ${EndIf}
    File "WeaselSetup.exe"
    Delete /REBOOTOK "$INSTDIR\WeaselSetup.exe.old.tmp"
  ${Else}
    File "WeaselSetup.exe"
  ${EndIf}
  WeaselSetup_done:
  Pop $R1
  Pop $R0

  ; Phase D (v0.19.0.36) Reinforcement E (final): post-install sweep.
  ; After all L72-fix File commands, force-delete any leftover .old.tmp
  ; binaries (weasel.dll, weaselx64.dll, WeaselServer.exe, WeaselDeployer.exe,
  ; WeaselSetup.exe). On locked files, REBOOTOK schedules cleanup.
  ; Also, in case the prior install left a different filename (e.g. user
  ; copied WeaselServer.exe manually), do a final best-effort delete of
  ; the live binary too — at worst, REBOOTOK leaves the user with a
  ; clean filesystem after reboot.
  Delete /REBOOTOK "$INSTDIR\weasel.dll.old.tmp"
  Delete /REBOOTOK "$INSTDIR\weaselx64.dll.old.tmp"
  Delete /REBOOTOK "$INSTDIR\WeaselServer.exe.old.tmp"
  Delete /REBOOTOK "$INSTDIR\WeaselDeployer.exe.old.tmp"
  Delete /REBOOTOK "$INSTDIR\WeaselSetup.exe.old.tmp"

  File "Win32\rime.dll"
  File "Win32\WinSparkle.dll"
  ${If} ${AtLeastWin11}
    ${If} ${IsNativeARM64}
      File /nonfatal "weaselARM.dll"
      File /nonfatal "weaselARM64.dll"
      File /nonfatal "weaselARM64X.dll"
    ${Endif}
  ${Endif}

  ; shared data files
  SetOutPath $INSTDIR\data
  File "data\*.yaml"
  File /nonfatal "data\*.txt"
  File /nonfatal "data\*.gram"
  ; cn_dicts (rime_ice dependencies)
  SetOutPath $INSTDIR\data\cn_dicts
  File "data\cn_dicts\*.dict.yaml"
  ; en_dicts (melt_eng dependencies)
  SetOutPath $INSTDIR\data\en_dicts
  File "data\en_dicts\*"
  ; lua extensions (rime_ice features)
  SetOutPath $INSTDIR\data\lua
  File "data\lua\*.lua"
  File "data\lua\*.db"
  File /nonfatal "data\lua\*.txt"
  SetOutPath $INSTDIR\data\lua\cold_word_drop
  File "data\lua\cold_word_drop\*.lua"
  ; opencc data files
  SetOutPath $INSTDIR\data\opencc
  File "data\opencc\*.json"
  File "data\opencc\*.ocd*"
  ; prebuilt compiled dictionaries (instant first-run, no deploy wait)
  SetOutPath $INSTDIR\data\build
  File "data\build\*.bin"
  File "data\build\*.yaml"
  ; images
  SetOutPath $INSTDIR\data\preview
  File "data\preview\*.png"

  SetOutPath $INSTDIR

  ; test /T flag for zh_TW locale
  StrCpy $R2 "/i"
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/S" $R1
  IfErrors +2 0
  StrCpy $R2 "/s"
  ${GetOptions} $R0 "/T" $R1
  IfErrors +2 0
  StrCpy $R2 "/t"
  ; === v2.0: Force user data directory to $R3\fluxing\user1 (spec 002/005) ===
  ; $R3 holds the user-facing install path (saved before $INSTDIR was reset to WEASEL_ROOT).
  ; No user choice: co-located with engine. Future account login can rename user1 per user.
  CreateDirectory "$R3\user1\fluxing"
  WriteRegStr HKCU "Software\Fluxing\Weasel" "RimeUserDir" "$R3\user1\fluxing"
  ; (No exec wait needed; WriteRegStr is synchronous. Kept block for future logging.)

  ; === v0.20.0.0: Deploy Rime user-config custom YAMLs to user RimeUserDir ===
  ; Source: output\data\user-custom\*.custom.yaml (bundled below).
  ; Default: 8 标点 + Shift+ 配对 (per handoff-rime-customization-2026-08-01 + L##-Librime-PatchPriority-PunctuatorHalfShape).
  ; rime_ice + double_pinyin_sogou use __include: default:/punctuator/half_shape in their
  ; source schemas, so the custom MUST restate __include + overrides (librime
  ; DependencyPriority kPatch=2 > kInclude=1 means patch runs before include and
  ; silently drops dict-merge into __include string).
  ; radical_pinyin has no punctuator section (radical lookup scheme, not full IME);
  ; only key_binder Shift+ bindings are patched.
  ; Rime auto-detects custom mtime > schema mtime on first input event after
  ; install and triggers auto-redeploy (no manual 「重新部署」 needed).
  ; Uninstall: these files are user data and persist after Fluxing uninstall
  ; (per L##-PhaseL-9.13 user-data preservation principle).
  SetOutPath "$R3\user1\fluxing"
  File "data\user-custom\rime_ice.custom.yaml"
  File "data\user-custom\double_pinyin_sogou.custom.yaml"
  File "data\user-custom\radical_pinyin.custom.yaml"

  ; === v2.0: Register WeaselTSF as TSF text input processor (fix TSF TIP not registered) ===
  ; regsvr32 calls DllRegisterServer in WeaselTSF.dll which writes HKLM\SOFTWARE\...\CTF\TIP\{GUID}
  ; Without this, Windows doesn't know Fluxing is installed and won't show it in input switcher.
  ;
  ; spec 055 bugfix: BOTH weaselx64.dll (64-bit TSF shim) AND weasel.dll (32-bit TSF shim)
  ; must be registered. weasel.dll is the shim that x86 processes (notepad, VSCode etc)
  ; load via WoW64. Without it, HKCU\Software\Microsoft\CTF\Assemblies\0x00000804
  ; has no entry → user cannot activate Fluxing IME → candidate window never appears.
  ExecWait 'regsvr32 /s "$R3\weasel\weaselx64.dll"' $0
  ${If} $0 != 0
    DetailPrint "Fluxing: regsvr32 weaselx64.dll failed (exit $0); TSF TIP may not be registered. Run 'regsvr32 $R3\weasel\weaselx64.dll' as admin manually."
  ${EndIf}
  ExecWait 'regsvr32 /s "$R3\weasel\weasel.dll"' $1
  ${If} $1 != 0
    DetailPrint "Fluxing: regsvr32 weasel.dll failed (exit $1); x86 TSF TIP may not be registered. Run 'regsvr32 $R3\weasel\weasel.dll' as admin manually."
  ${EndIf}

  ; spec 066: workaround for CoCreateInstance(CLSID_TF_CategoryMgr)
  ; failing in elevated context on Win 10 24H2. Manually
  ; write KnownClasses + HKCU\0x00000804 so user can enable.
  ; L66-fix: these writes MUST run UNCONDITIONALLY (not gated on regsvr32
  ; exit code). regsvr32 success on a non-elevated install still does not
  ; write KnownClasses or the user-input-method binding at HKCU\0x00000804.
  ; These two keys are what the user toggles via "中文(简体, 中国)" in
  ; Settings -> Time & Language; if missing, the IME does not appear in the
  ; language list and QuickPanel never shows. Always write them as a safety
  ; net regardless of whether regsvr32 above succeeded or failed.
  ; L106 hardening: NSIS is a 32-bit installer. On 64-bit Windows, WriteRegStr
  ; HKLM defaults to the WOW6432Node redirected hive. TSF 64-bit processes
  ; (TextInputHost.exe, ctfmon.exe, language switcher) read the NATIVE 64-bit
  ; HKLM hive; if KnownClasses is only in WOW6432Node, the Fluxing TIP is
  ; invisible to the language switcher and QuickPanel never shows. Force
  ; SetRegView 64 for the KnownClasses write, then restore default 32-bit
  ; view so subsequent HKCU writes (which don't need 64-bit view) keep working.
  ${If} ${IsNativeARM64}
    SetRegView 64
  ${ElseIf} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  WriteRegStr HKLM "SOFTWARE\Microsoft\CTF\KnownClasses" "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}" "Fluxing Text Service"
  SetRegView default
  WriteRegStr HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "Default" "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
  WriteRegStr HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "Profile" "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
  WriteRegDWORD HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "KeyboardLayout" 0x08040804
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayName" "$(DISPLAYNAME)"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\WeaselServer.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayVersion" "${FLUXING_VERSION}.${WEASEL_BUILD}"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "Publisher" "aiec.fun"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "URLInfoAbout" "https://rime.im/"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "HelpLink" "https://rime.im/docs/"
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${REG_UNINST_KEY}" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; run as user...
  IfSilent deploy_silently
  ExecWait "$INSTDIR\WeaselDeployer.exe /install"
  GoTo deploy_done

  deploy_silently:
  ExecWait "$INSTDIR\WeaselDeployer.exe /deploy"
  deploy_done:

  ; don't redirect on 64 bit system for auto run setting
  ${If} ${IsNativeARM64}
    SetRegView 64
  ${ElseIf} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  ; v0.19.0.48 (Phase J installer Stage 2): 检测 WeaselServer.exe 写入是否成功。
  ;   如果 PPL 锁住导致 File 命令失败, NSIS Rename-then-File 走 fallback 路径
  ;   留下 WeaselServer.exe.old.tmp (旧 binary) + 没写新 binary. 此时 schedule
  ;   Stage 2 启动项: At-startup 时跑 cmd batch copy staged binary → real binary.
  ;   安装器把新 binary 也写到 %TEMP%\fluxing-staged\ 让 Stage 2 读得到。
  IfFileExists "$INSTDIR\WeaselServer.exe.old.tmp" 0 weasel_write_ok
    DetailPrint "Fluxing v0.19.0.48: WeaselServer.exe PPL-locked; staging binary for boot-time swap"
    CreateDirectory "$TEMP\fluxing-staged"
    CopyFiles /SILENT /FILESONLY "$PLUGINSDIR\..\Win32\WeaselServer.exe" "$TEMP\fluxing-staged\WeaselServer.exe"
    ; Stage 2 batch script — runs at next boot via Task Scheduler
    FileOpen $R9 "$TEMP\fluxing-staged\stage2-install.bat" w
    FileWrite $R9 "@echo off$\n"
    FileWrite $R9 'xcopy /Y /Q "%TEMP%\fluxing-staged\WeaselServer.exe" "$INSTDIR\WeaselServer.exe*"$\n'
    FileWrite $R9 'del /F /Q "$INSTDIR\WeaselServer.exe.old.tmp"$\n'
    FileWrite $R9 'schtasks /Delete /TN FluxingStage2Install /F$\n'
    FileWrite $R9 'del "%TEMP%\fluxing-staged\stage2-install.bat"$\n'
    FileWrite $R9 'del "%TEMP%\fluxing-staged\WeaselServer.exe"$\n'
    FileClose $R9
    ; 注册 Task Scheduler: At-startup (boot), 高权限
    nsExec::ExecToStack 'schtasks /Create /SC ONSTART /TN FluxingStage2Install /TR "cmd /c $\"$TEMP\fluxing-staged\stage2-install.bat$\"" /RL HIGHEST /F'
    Pop $0
    Pop $1
    IfSilent skip_post_install_message
    MessageBox MB_OK|MB_ICONEXCLAMATION "WeaselServer.exe 正在被 PPL 进程锁住,无法在线覆盖。$\r$\n已注册 At-startup Stage 2 启动项, 重启电脑后自动完成 binary swap。$\r$\n$\r$\n(快捷设置栏:按 Alt+, 或点击任务栏中英文图标)"
    Goto skip_post_install_message
  weasel_write_ok:
  ; Write autorun key
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer" "$INSTDIR\WeaselServer.exe"
  ; Start WeaselServer
  Exec "$INSTDIR\WeaselServer.exe"

  ; option CheckForUpdates
  IfSilent DisableAutoCheckUpdate
  MessageBox MB_YESNO|MB_ICONINFORMATION "$(AUTOCHKUPDATE)" IDYES EnableAutoCheckUpdate
  DisableAutoCheckUpdate:
  WriteRegStr HKCU "Software\Rime\Weasel\Updates" "CheckForUpdates" "0"
  GoTo end
  EnableAutoCheckUpdate:
  WriteRegStr HKCU "Software\Rime\Weasel\Updates" "CheckForUpdates" "1"
  end:

  ; Prompt reboot
  StrCmp $0 "Upgrade" 0 +2
  SetRebootFlag true

  ; L51: notify the user to restart WeaselServer / sign out and back in
  ; after the install completes. Silent installs (/S) skip this prompt
  ; because the messagebox blocks unattended mass-deployment.
  IfSilent skip_post_install_message
  MessageBox MB_OK|MB_ICONINFORMATION "火流猩输入法已安装。$\r$\n$\r$\n请重启 WeaselServer.exe 或注销后重新登录以应用新版本。$\r$\n$\r$\n(快捷设置栏:按 Alt+, 或点击任务栏中英文图标)"
  skip_post_install_message:

SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\$(DISPLAYNAME)"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORMANUAL).lnk" "$INSTDIR\README.txt"
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETTING).lnk" "$INSTDIR\WeaselDeployer.exe" "" "$SYSDIR\shell32.dll" 21
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDICT).lnk" "$INSTDIR\WeaselDeployer.exe" "/dict" "$SYSDIR\shell32.dll" 6
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSYNC).lnk" "$INSTDIR\WeaselDeployer.exe" "/sync" "$SYSDIR\shell32.dll" 26
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORDEPLOY).lnk" "$INSTDIR\WeaselDeployer.exe" "/deploy" "$SYSDIR\shell32.dll" 144
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSERVER).lnk" "$INSTDIR\WeaselServer.exe" "" "$INSTDIR\WeaselServer.exe" 0
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUSERFOLDER).lnk" "$INSTDIR\WeaselServer.exe" "/userdir" "$SYSDIR\shell32.dll" 126
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORAPPFOLDER).lnk" "$INSTDIR\WeaselServer.exe" "/weaseldir" "$SYSDIR\shell32.dll" 19
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUPDATER).lnk" "$INSTDIR\WeaselServer.exe" "/update" "$SYSDIR\shell32.dll" 13
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORSETUP).lnk" "$INSTDIR\WeaselSetup.exe" "" "$SYSDIR\shell32.dll" 162
  CreateShortCut "$SMPROGRAMS\$(DISPLAYNAME)\$(LNKFORUNINSTALL).lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0

SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'
  ; L13 fix: force-kill any zombie WeaselServer.exe (see call_uninstaller above).
  nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe /T'
    Pop $0
    Pop $1

  ExecWait '"$INSTDIR\WeaselSetup.exe" /u'

  ; Remove registry keys
  DeleteRegKey HKLM SOFTWARE\Rime
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing"
  ; don't redirect on 64 bit system for auto run setting
  ${If} ${IsNativeARM64}
    SetRegView 64
  ${ElseIf} ${IsNativeAMD64}
    SetRegView 64
  ${Endif}
  DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer"

  ; Remove files and uninstaller
  SetOutPath $TEMP
  Delete  "$INSTDIR\data\opencc\*.*"
  Delete  "$INSTDIR\data\preview\*.*"
  Delete  "$INSTDIR\data\*.*"
  Delete  "$INSTDIR\*.*"
  RMDir  "$INSTDIR\data\opencc"
  RMDir  "$INSTDIR\data\preview"
  RMDir  "$INSTDIR\data"
  RMDir  "$INSTDIR"
  SetShellVarContext all
  Delete  "$SMPROGRAMS\$(DISPLAYNAME)\*.*"
  RMDir  "$SMPROGRAMS\$(DISPLAYNAME)"

  ; Prompt reboot
  SetRebootFlag true

SectionEnd







Function ForceFluxingSuffix
  ; L14: Enforce fluxing suffix on $INSTDIR (idempotent). Bug fix 0.18.3.0.
  ; 0.18.3.0 logic bug: StrCmp fell through to next line, sending every input
  ; to not_fluxing (double-suffix). Fix uses explicit Goto for match case.
  ; NSIS StrCmp is case-insensitive by default, so a single check against
  ; 'fluxing' matches both casings.
  Push $0
  StrCpy $0 '$INSTDIR' '' -7
  StrCmp $0 'fluxing' 0 +2
  Goto suffix_done
  StrCpy $INSTDIR '$INSTDIR\fluxing'
suffix_done:
  Pop $0
FunctionEnd
