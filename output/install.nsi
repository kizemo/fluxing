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
!define FLUXING_VERSION 1.0.0
!endif

!ifndef WEASEL_BUILD
!define WEASEL_BUILD 0
!endif

!ifndef WEASEL_ROOT
!define WEASEL_ROOT $INSTDIR\weasel
!endif
!define FLUXING_ROOT $INSTDIR\fluxing
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing"

; L120: 1 = something could not be replaced online, so the
; boot-time fallback took over and the machine has to restart.
; 0 = everything was replaced online.
Var FluxingDegraded
; L120: 1 = the running WeaselServer.exe is gone.
Var FluxingQuitDone
; L120: the TSF shim paths that are really registered.  Normally
; the canonical names; when a shim was locked and a versioned
; shim had to be used, the versioned names.
Var FluxingWeaselDll
Var FluxingWeaselX64Dll
; L120: the Windows directory and the regsvr32 that matches the
; bitness of the DLL it has to register.
Var FluxingSysRoot
Var FluxingRegSvr32
Var FluxingRegSvr32x64

; The name of the installer
Name "Fluxing ${FLUXING_VERSION}"

; The file to write
OutFile "archives\fluxing-${FLUXING_VERSION}-installer.exe"

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
  ; L120: the old sweep ran taskkill ten times with a 1.5-3s sleep
  ; after each attempt, so the installer sat there for half a
  ; minute before it even started copying.  taskkill cannot stop a
  ; protected process anyway (L103-I); the reliable way to stop
  ; WeaselServer.exe is its own /quit IPC, which the install
  ; section drives with a poll loop.  Keep a short sweep for the
  ; ordinary case only.
  ${For} $R9 1 3
    nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe /T'
    Pop $0
    Pop $1
    Sleep 1000
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
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  StrCpy $R8 "$PROGRAMFILES32\Fluxing\weasel\WeaselServer.exe"
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  StrCpy $R8 "D:\Program Files\Fluxing\weasel\WeaselServer.exe"
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  StrCpy $R8 "C:\Program Files\Rime\weasel\WeaselServer.exe"
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  StrCpy $R8 "C:\Program Files\WeaselServer\weasel\WeaselServer.exe"
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  ReadRegStr $R8 HKLM "Software\Fluxing\Weasel" "InstallDir"
  StrCmp $R8 "" skip_oldpath_delete
  StrCpy $R8 "$R8\WeaselServer.exe"
  IfFileExists $R8 0 skip_oldpath_delete
    ${If} ${FileExists} $R8
      Delete $R8
      ${If} ${Errors}
        DetailPrint "Fluxing: the leftover $R8 is still locked; it stays until the process holding it exits"
      ${Else}
        DetailPrint "Fluxing: removed the leftover $R8"
      ${EndIf}
      ClearErrors
    ${EndIf}
  skip_oldpath_delete:

  ; L120: the ctfmon.exe / TextInputHost.exe taskkill loop that used
  ; to live here is GONE on purpose.  Killing TextInputHost.exe
  ; does not merely stop one input method, it takes the Windows
  ; text-services stack down with it: on 2026-10-23 15:52-15:55
  ; TextInputHost.exe crashed, dwm.exe then crashed three times in
  ; a row and StartMenuExperienceHost.exe crashed as well, which is
  ; what the user saw as a frozen desktop plus a row of error
  ; dialogs.  It was useless too: Windows restarts those hosts
  ; within about two seconds and the restarted host re-maps the
  ; old TSF shim, so the File command further down still failed.
  ; A locked shim is now handled without touching any process: the
  ; new shim is installed under a versioned name and the TSF
  ; registration is pointed at it.

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
  ; stale value.
  ;
  ; 默认安装路径 "D:\Program Files\fluxing" 是**产品要求**（维护者明确指定：
  ; 默认装到这个盘，安装者可以在安装界面改），不是待修的 bug。
  ; 静默安装要改路径只能靠 /D=，而 /D= 的值**绝对不能带引号**
  ; （NSIS 手册 3.2.1；带引号会让 NSIS 直接忽略该参数 —— 见 L112）。
  ;
  ; 不要把这里改成 $PROGRAMFILES64\fluxing：历史注释里写过那句，
  ; 但它与产品要求不符，已经被后人当成「待修的 bug」改过一次（改错方向）。
  ; 保留 D 盘默认，并在 §7 anti-pattern 表里留了记录。
  StrCpy $INSTDIR "D:\Program Files\fluxing"
  StrCpy $R0 ""
  Goto skip
use_reg:
  ; spec 053 v3 → spec 077 follow-up: $INSTDIR was pre-loaded by
  ; InstallDirRegKey (HKLM\Software\Fluxing\Weasel\InstallDir line 374,
  ; upgrade-in-place) OR by /D= command line. spec 053 v3 unconditionally
  ; StrCpy'd $R0 → $INSTDIR here, which clobbered explicit /D= paths in
  ; silent-install scenarios (sandbox-verify T20 8 scenarios blocked
  ; because /D=C:\FluxingTest\fluxing was overwritten by stale
  ; HKLM\Software\Fluxing\Weasel\InstallDir value D:\Program Files\fluxing).
  ; Fix: skip the copy. $INSTDIR already holds the correct value:
  ; - registry value when no /D= (upgrade-in-place, preserved by InstallDirRegKey)
  ; - /D= value when explicit (already set BEFORE .onInit by NSIS command-line parser)
  Goto skip
set_default:
  ; L14: Default installation directory (only if /D= did not provide one).
  ;
  ; 这里写死 "D:\Program Files\fluxing" 是**产品要求**（维护者明确指定默认装到
  ; D 盘，安装者可在安装界面改），不是 bug，也不是"应该用 $PROGRAMFILES64"的历史遗留。
  ; 历史注释曾解释"安装器是 x86、要用 $PROGRAMFILES64 拿真正的 64 位 Program Files"
  ; 并声称下面就是那么写的 —— 实际不是，后来被当成待修项改错过一次（改错方向）。
  ;
  ; 覆盖路径的方式：
  ;   - 交互安装：安装界面里改（.onInit 之前 $INSTDIR 已被用户选择覆盖）
  ;   - 静默安装：命令行 /D=<绝对路径>，**值不能带引号**（NSIS 手册 3.2.1 / L112）
  ;     且必须是最后一个参数。带引号 ⇒ NSIS 忽略 ⇒ 落回这里的 D 盘默认。
  ; L14 的架构约束仍然成立：随包分发的 Weasel*.exe 必须是 x86（rime.dll 是 32 位），
  ; 这一点与默认装到哪个盘无关。
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

  ; L120: the canonical TSF shim names are the ones that get
  ; registered, unless a shim turns out to be locked and a
  ; versioned shim had to be used instead.
  StrCpy $FluxingWeaselDll "$INSTDIR\weasel.dll"
  StrCpy $FluxingWeaselX64Dll "$INSTDIR\weaselx64.dll"
  ; L120: the bare name "regsvr32" resolves to the 32 bit
  ; regsvr32 from this 32 bit installer, and the 32 bit regsvr32
  ; cannot load a 64 bit DLL at all, so the 64 bit TIP
  ; registration never happened from here.  Pick the regsvr32
  ; that matches the DLL instead.
  ReadRegStr $FluxingSysRoot HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "SystemRoot"
  StrCpy $FluxingRegSvr32 "$FluxingSysRoot\SysWOW64\regsvr32.exe"
  IfFileExists $FluxingRegSvr32 0 fluxing_regsvr32_fallback
    Goto fluxing_regsvr32_ready
  fluxing_regsvr32_fallback:
  ; 32 bit Windows (or ARM64 without SysWOW64): the plain system
  ; directory holds the 32 bit regsvr32 there.
  StrCpy $FluxingRegSvr32 "$SYSDIR\regsvr32.exe"
  fluxing_regsvr32_ready:
  StrCpy $FluxingRegSvr32x64 "$FluxingSysRoot\Sysnative\regsvr32.exe"
  IfFileExists $FluxingRegSvr32x64 0 fluxing_regsvr32x64_fallback
    Goto fluxing_regsvr32x64_ready
  fluxing_regsvr32x64_fallback:
  StrCpy $FluxingRegSvr32x64 "$SYSDIR\regsvr32.exe"
  fluxing_regsvr32x64_ready:

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
  ; L120: ask the running server to exit through its own IPC and
  ; wait for it.  ExecWait would hang the installer for good when
  ; that server is stuck (L13), and taskkill cannot stop a
  ; protected process (L103-I), so the request is fired and the
  ; process is polled instead.  taskkill exits with 128 when no
  ; such process exists, which is the signal to stop waiting.
  Exec '"$INSTDIR\WeaselServer.exe" /quit'
  StrCpy $FluxingQuitDone 0
  ${For} $R9 1 20
    ${If} $FluxingQuitDone == 0
      nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe'
      Pop $0
      Pop $1
      ${If} $0 == 128
        StrCpy $FluxingQuitDone 1
      ${EndIf}
      Sleep 500
    ${EndIf}
  ${Next}
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
  ; L82: 装 fluxing-logo_small.png (20x20 PNG icon, QuickPanel 实际使用的 logo)
  ; v0.20.0.0 曾 ROLLBACK 此行 (源文件缺失 -> NSIS abort), 2026-10-22 恢复:
  ; 源文件已从 docs/design/fluxing-logo_small.png 还原到 output/
  ; (SHA256 43B994AD1482D7D0EA5A2F51154080CF63D1AC41CFAB34D2274165865F1825AC)
  ; 注意: copy 必须在 makensis 之前, 否则 File 指令 abort (L82 原始教训).

  ; L120: $INSTDIR already points at the engine directory here, so "$INSTDIR\weasel" nested one level too deep and the QuickPanel logo never landed next to WeaselServer.exe.  SetOutPath $INSTDIR
  File "fluxing-logo.png"
  File "fluxing-logo_small.png"
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
  ; L120: unpack the new binaries under a staging name.
  ; NSIS extracts File to the current SetOutPath, which here is
  ; $INSTDIR -- exactly where the locked old binaries sit.  The
  ; old stage 2 script copied from "$PLUGINSDIR\..\Win32\...",
  ; a path that never existed, so the boot-time swap had nothing
  ; to copy and the old binary survived even the reboot.  Staging
  ; the new binaries here gives every fallback a real source.
  CreateDirectory "$TEMP\fluxing-staged"
  SetOutPath "$TEMP\fluxing-staged"
!if /FileExists "Win32\WeaselServer.exe"
  File "Win32\WeaselServer.exe"
!endif
!if /FileExists "weasel.dll"
  File "weasel.dll"
!endif
!if /FileExists "weaselx64.dll"
  File "weaselx64.dll"
!endif
  SetOutPath $INSTDIR
  ; L120: start from a clean slate for the fallback bookkeeping.
  ; A leftover marker from an earlier install would otherwise
  ; trigger the boot-time swap and the restart prompt again.
  Delete "$INSTDIR\WeaselServer.exe.stage2"
  Delete "$INSTDIR\weaselx64.dll.stage2"
  Delete "$INSTDIR\WeaselServer.exe.old.tmp"
  Delete "$INSTDIR\weasel.dll.old.tmp"
  Delete "$INSTDIR\weaselx64.dll.old.tmp"
  ClearErrors
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
      ; Rename failed (still locked) -> try SetOverwrite try, skip on error
      SetOverwrite try
      ClearErrors
      File "weasel.dll"
      IfErrors 0 weasel_done
      ; L120: the 32 bit shim is locked by a running TSF host.  Do
      ; NOT kill the host: that takes the whole text-services
      ; stack down (see .onInit).  Do NOT ask for a reboot either.
      ; Install the new shim under a versioned name and point the
      ; TSF registration at it: every application started from
      ; now on loads the new shim, the ones already running keep
      ; the shim they mapped.  Reopening an application is all
      ; the user has to do.
      DetailPrint "Fluxing: weasel.dll is locked by a running app; installing a versioned shim instead (no reboot needed)"
      ClearErrors
      CopyFiles /SILENT "$TEMP\fluxing-staged\weasel.dll" "$INSTDIR\weasel.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"
      ${If} ${Errors}
        DetailPrint "Fluxing: the versioned shim could not be copied; the old shim is retained"
        StrCpy $FluxingDegraded 1
      ${Else}
        StrCpy $FluxingWeaselDll "$INSTDIR\weasel.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"
        ExecWait '"$FluxingRegSvr32" /s "$FluxingWeaselDll"' $0
        ${If} $0 != 0
          DetailPrint "Fluxing: regsvr32 on the versioned shim failed (exit $0); the old shim is retained"
          StrCpy $FluxingDegraded 1
        ${EndIf}
      ${EndIf}
      SetOverwrite on
      Goto weasel_done
    ${EndIf}
    ; Rename 成功,旧文件已经不在 $INSTDIR\weasel.dll
    File "weasel.dll"
    ; 清理 .old.tmp(用 REBOOTOK 防止正在运行时还占用)
    Delete "$INSTDIR\weasel.dll.old.tmp"
    ClearErrors
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
    ; v0.20.0.4 F3 (spec 076 reliability): 如果 Rename AND File 都失败 (TSF host 锁住),
    ; 写 $INSTDIR\weaselx64.dll.stage2 marker。Stage 2 段 (line 800+) 会检此 marker 并
    ; 注册 At-startup 启动项, 重启后复制新 binary 完成 swap。
    ; v0.20.0.3 user report: weaselx64.dll 装机时 TSF-locked → 静默保留 v0.20.0.2 旧 binary
    ;   → PipeChannel race 修复 (a289525b) 未部署 → 微信/Claude/dopus 多 app 并发切火流猩
    ;   race 重新触发 → 0xc0000374 HEAP_CORRUPTION crash (L##-PhaseM)。
    Push $R0
    Push $R1
    ${If} ${FileExists} "$INSTDIR\weaselx64.dll"
      ClearErrors
      Rename "$INSTDIR\weaselx64.dll" "$INSTDIR\weaselx64.dll.old.tmp"
      ${If} ${Errors}
        SetOverwrite try
        File "weaselx64.dll"
        IfErrors 0 weaselx64_done
        ; BOTH Rename AND File failed (the TSF host still holds the
        ; mapping).  L120: do not wait for a reboot and do not kill
        ; the host.  Install the new shim under a versioned name
        ; and point the TSF registration at it: every application
        ; started from now on loads the new shim, the ones already
        ; running keep the shim they mapped.  The boot-time swap
        ; stays as the last resort, for the case where even the
        ; versioned copy fails.
        DetailPrint "Fluxing: weaselx64.dll is locked by a running app; installing a versioned shim instead (no reboot needed)"
        ClearErrors
        CopyFiles /SILENT "$TEMP\fluxing-staged\weaselx64.dll" "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"
        ${If} ${Errors}
          DetailPrint "Fluxing: the versioned shim could not be copied; falling back to the boot-time swap"
          FileOpen $R9 "$INSTDIR\weaselx64.dll.stage2" w
          FileWrite $R9 "STAGE2_WEASELX64_NEEDED"
          FileClose $R9
          StrCpy $FluxingDegraded 1
        ${Else}
          StrCpy $FluxingWeaselX64Dll "$INSTDIR\weaselx64.${FLUXING_VERSION}.${WEASEL_BUILD}.dll"
          ExecWait '"$FluxingRegSvr32x64" /s "$FluxingWeaselX64Dll"' $0
          ${If} $0 != 0
            DetailPrint "Fluxing: regsvr32 on the versioned shim failed (exit $0); falling back to the boot-time swap"
            FileOpen $R9 "$INSTDIR\weaselx64.dll.stage2" w
            FileWrite $R9 "STAGE2_WEASELX64_NEEDED"
            FileClose $R9
            StrCpy $FluxingDegraded 1
          ${EndIf}
        ${EndIf}
        SetOverwrite on
        Goto weaselx64_done
      ${EndIf}
      File "weaselx64.dll"
      Delete "$INSTDIR\weaselx64.dll.old.tmp"
      ClearErrors
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
      ; L120: remember that the online replacement failed, so the
      ; boot-time fallback and the restart request really apply.
      FileOpen $R9 "$INSTDIR\WeaselServer.exe.stage2" w
      FileWrite $R9 "STAGE2_WEASELSERVER_NEEDED"
      FileClose $R9
      StrCpy $FluxingDegraded 1
      DetailPrint "Fluxing: WeaselServer.exe locked; scheduled delete on reboot. Reboot to pick up new binary."
      SetOverwrite on
      Goto WeaselServer_done
    ${EndIf}
    ; Rename 成功,旧文件已经不在
    File "Win32\WeaselServer.exe"
    Delete "$INSTDIR\WeaselServer.exe.old.tmp"
    ClearErrors
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
  ; L##-FluxingPhrasesDialog-PathFix (v0.20.0.1): MSBuild /t:Rebuild 产物在
  ; output/Win32/FluxingPhrasesDialog.exe,但 install.nsi 之前从 output/ 根
  ; 读,导致 installer bundle 旧 (Jul 29) 二进制。装机后 WeaselServer (Aug 2 新)
  ; + FluxingPhrasesDialog (Jul 29 旧) 版本错配 → IPC protocol mismatch →
  ; heap corruption → Claude Code / Explorer 多进程连锁 crash (PCH_6E_FROM_ntdll+0x160BB4)。
  ; 修复: 与 WeaselServer.exe / WeaselDeployer.exe 路径一致,使用 Win32\ 前缀。
  File "Win32\FluxingPhrasesDialog.exe"

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
      ; L120: the online replacement failed, so the boot-time
      ; fallback and the restart request really apply.
      StrCpy $FluxingDegraded 1
      DetailPrint "Fluxing: WeaselDeployer.exe locked; scheduled delete on reboot."
      SetOverwrite on
      Goto WeaselDeployer_done
    ${EndIf}
    File "Win32\WeaselDeployer.exe"
    Delete "$INSTDIR\WeaselDeployer.exe.old.tmp"
    ClearErrors
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
      ; L120: the online replacement failed, so the boot-time
      ; fallback and the restart request really apply.
      StrCpy $FluxingDegraded 1
      DetailPrint "Fluxing: WeaselSetup.exe locked; scheduled delete on reboot."
      SetOverwrite on
      Goto WeaselSetup_done
    ${EndIf}
    File "WeaselSetup.exe"
    Delete "$INSTDIR\WeaselSetup.exe.old.tmp"
    ClearErrors
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
  Delete "$INSTDIR\weasel.dll.old.tmp"
  ClearErrors
  Delete "$INSTDIR\weaselx64.dll.old.tmp"
  ClearErrors
  Delete "$INSTDIR\WeaselServer.exe.old.tmp"
  ClearErrors
  Delete "$INSTDIR\WeaselDeployer.exe.old.tmp"
  ClearErrors
  Delete "$INSTDIR\WeaselSetup.exe.old.tmp"
  ClearErrors

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
  ; L120: register the 64 bit shim that is really on disk, with a
  ; 64 bit regsvr32.  See the note next to $FluxingRegSvr32x64.
  ExecWait '"$FluxingRegSvr32x64" /s "$FluxingWeaselX64Dll"' $0
  ${If} $0 != 0
    DetailPrint "Fluxing: regsvr32 on $FluxingWeaselX64Dll failed (exit $0); TSF TIP may not be registered. Run 'regsvr32 $FluxingWeaselX64Dll' as admin manually."
  ${EndIf}
  ; L120: register the 32 bit shim that is really on disk, with a
  ; 32 bit regsvr32.
  ExecWait '"$FluxingRegSvr32" /s "$FluxingWeaselDll"' $1
  ${If} $1 != 0
    DetailPrint "Fluxing: regsvr32 on $FluxingWeaselDll failed (exit $1); x86 TSF TIP may not be registered. Run 'regsvr32 $FluxingWeaselDll' as admin manually."
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
  ; v0.20.0.4 F4: HKCU CTF binding 必须真正写到磁盘。WriteRegStr silent miss
  ;   (e.g. NSIS 路径解析失败 / HKCU hive 在非交互安装下指错 user) 会让用户切不到
  ;   火流猩除非手动改注册表。ReadRegStr verify + 2 次 retry + 失败 loud log。
  ; v0.20.0.3 user report: DisplayVersion=0.20.0.3 但 HKCU\..\CTF\Assemblies\0x00000804\
  ;   {3D02CAB6-...} 缺失 → HKCU 下仍只有 MS Pinyin ({34745C63-...})。用户只能通过
  ;   side-channel 切火流猩 (e.g. regsvr32 副作用), 一旦 Windows 更新或 profile reset 就丢失。
  ${For} $R9 1 2
    WriteRegStr HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "Default" "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
    WriteRegStr HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "Profile" "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
    WriteRegDWORD HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "KeyboardLayout" 0x08040804
    ReadRegStr $R8 HKCU "Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-2B8E-4781-BA20-1C9267529467}" "Default"
    ${If} $R8 == "{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}"
      Goto ctf_binding_done
    ${EndIf}
    Sleep 500
  ${Next}
  DetailPrint "Fluxing v1.0.0 F4: HKCU CTF binding write FAILED after 2 retries. User must enable 火流猩 manually via Settings -> Time & Language -> Chinese (Simplified) -> Options (loud warn)"
  ctf_binding_done:
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayName" "$(DISPLAYNAME)"
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\WeaselServer.exe"'
  WriteRegStr HKLM "${REG_UNINST_KEY}" "DisplayVersion" "${FLUXING_VERSION}"
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
  ; L120: the boot-time fallback is driven by the marker that the
  ; failed replacement writes, not by the presence of a leftover
  ; .old.tmp -- otherwise every install would schedule the swap
  ; and ask for a restart even when nothing was left to do.
  IfFileExists "$INSTDIR\WeaselServer.exe.stage2" 0 weasel_write_ok
    DetailPrint "Fluxing v0.19.0.48: WeaselServer.exe PPL-locked; staging binary for boot-time swap"
    CreateDirectory "$TEMP\fluxing-staged"
    ; L120: the new binary is already staged above, so the old copy
    ; from "$PLUGINSDIR\..\Win32\WeaselServer.exe" (a path that
    ; never existed) is gone.
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
  ; v0.20.0.4 F3: weaselx64.dll Stage 2 trigger (separate task FluxingStage2InstallX64).
  ;   Marker $INSTDIR\weaselx64.dll.stage2 由 line 500-524 在 Rename AND File 都失败时写。
  ;   Stage 2 batch 在下次重启时 copy $TEMP\fluxing-staged\weaselx64.dll 到 $INSTDIR
  ;   并清 marker + 自删 task。
  ${If} ${RunningX64}
    ${If} ${FileExists} "$INSTDIR\weaselx64.dll.stage2"
      DetailPrint "Fluxing v0.20.0.4 F3: weaselx64.dll TSF-locked at install; staging new binary for boot-time swap (spec 076 race fix deployment)"
      CreateDirectory "$TEMP\fluxing-staged"
      ; L120: the new binary is already staged above, so the old copy
      ; from "$PLUGINSDIR\..\weaselx64.dll" (a path that never
      ; existed) is gone.
      FileOpen $R9 "$TEMP\fluxing-staged\stage2-install-x64.bat" w
      FileWrite $R9 "@echo off$\n"
      FileWrite $R9 'xcopy /Y /Q "%TEMP%\fluxing-staged\weaselx64.dll" "$INSTDIR\weaselx64.dll*"$\n'
      FileWrite $R9 'del /F /Q "$INSTDIR\weaselx64.dll.stage2"$\n'
      FileWrite $R9 'schtasks /Delete /TN FluxingStage2InstallX64 /F$\n'
      FileWrite $R9 'del "%TEMP%\fluxing-staged\stage2-install-x64.bat"$\n'
      FileWrite $R9 'del "%TEMP%\fluxing-staged\weaselx64.dll"$\n'
      FileClose $R9
      nsExec::ExecToStack 'schtasks /Create /SC ONSTART /TN FluxingStage2InstallX64 /TR "cmd /c $\"$TEMP\fluxing-staged\stage2-install-x64.bat$\"" /RL HIGHEST /F'
      Pop $0
      Pop $1
      IfSilent skip_post_install_message
      MessageBox MB_OK|MB_ICONEXCLAMATION "weaselx64.dll 正在被 TSF 宿主锁住,无法在线覆盖。$\r$\n已注册 At-startup Stage 2 启动项 (v0.20.0.4 F3), 重启电脑后自动完成 binary swap。$\r$\n$\r$\n(快捷设置栏:按 Alt+, 或点击任务栏中英文图标)"
      Goto skip_post_install_message
    ${EndIf}
  ${EndIf}
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

  ; L120: only ask for a reboot when something really could not be
  ; replaced online.  SetRebootFlag used to be set unconditionally
  ; in Upgrade mode, and on top of that every Delete /REBOOTOK in
  ; this script left a file behind for the next boot, which NSIS
  ; turns into a reboot request.  So even a fully successful
  ; upgrade used to end with "please restart".  The TSF shim now
  ; takes effect through a versioned shim and WeaselServer.exe
  ; exits through its own IPC, so only the boot-time fallback
  ; needs a reboot.
  ${If} $FluxingDegraded == 1
    SetRebootFlag true
  ${EndIf}

  ; L120: the update is live now.  The TSF shim was either replaced in
  ; place or re-registered under a versioned name, and WeaselServer.exe
  ; was restarted from the new binary, so the user only has to reopen
  ; the applications that were running.  Silent installs (/S) skip this
  ; prompt because the messagebox blocks unattended mass-deployment.
  IfSilent skip_post_install_message
  MessageBox MB_OK|MB_ICONINFORMATION "火流猩输入法已安装。$\r$\n$\r$\n请关闭并重新打开正在使用输入法的应用，新打开的应用即刻使用新版本（不需要重启电脑）。$\r$\n$\r$\n(press Alt+, or click the language icon in the task bar)"
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

  ; L120: ask the running server to exit through its own IPC and
  ; wait for it.  ExecWait would hang the installer for good when
  ; that server is stuck (L13), and taskkill cannot stop a
  ; protected process (L103-I), so the request is fired and the
  ; process is polled instead.  taskkill exits with 128 when no
  ; such process exists, which is the signal to stop waiting.
  Exec '"$INSTDIR\WeaselServer.exe" /quit'
  StrCpy $FluxingQuitDone 0
  ${For} $R9 1 20
    ${If} $FluxingQuitDone == 0
      nsExec::ExecToStack 'taskkill /F /IM WeaselServer.exe'
      Pop $0
      Pop $1
      ${If} $0 == 128
        StrCpy $FluxingQuitDone 1
      ${EndIf}
      Sleep 500
    ${EndIf}
  ${Next}
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
  ; L120: remove the versioned shims the fallback path may have left.
  Delete  "$INSTDIR\weasel.*.dll"
  Delete  "$INSTDIR\weaselx64.*.dll"
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
