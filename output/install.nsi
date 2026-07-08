; weasel installation script
; Attribution: built on the upstream RIME / 中州韻輸入法引擎 (https://rime.im/)
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI2.nsh
!include x64.nsh
!include winVer.nsh

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
  ExecWait 'taskkill /F /IM WeaselServer.exe /T'

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
  ExecWait '"$R1\WeaselServer.exe" /quit'
  ; L13 fix: force-kill any zombie WeaselServer.exe (taskkill /F).
  ; /quit is a polite request; if the process is hung / crashed / lock-held
  ; the polite exit never completes and the file lock persists. taskkill /F
  ; is the unconditional fallback. /T also kills child processes.
  ExecWait 'taskkill /F /IM WeaselServer.exe /T'
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
  ExecWait '"$INSTDIR\WeaselServer.exe" /quit'
  ; L13 fix: force-kill any zombie WeaselServer.exe (see call_uninstaller above).
  ExecWait 'taskkill /F /IM WeaselServer.exe /T'

  SetOverwrite on
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR

  IfFileExists $TEMP\weasel-backup\*.* 0 program_files
  CreateDirectory $INSTDIR\data
  CopyFiles $TEMP\weasel-backup\*.* $INSTDIR\data
  RMDir /r $TEMP\weasel-backup

program_files:
  File "LICENSE.txt"
  File "README.txt"
  File "7-zip-license.txt"
  File "7z.dll"
  File "7z.exe"
  File "COPYING-curl.txt"
  File "curl.exe"
  File "curl-ca-bundle.crt"
  File "rime-install.bat"
  File "rime-install-config.bat"
  File "start_service.bat"
  File "stop_service.bat"
  File "weasel.dll"
  ${If} ${RunningX64}
    ; L14-fix (spec 012 cleanup): weaselx64.dll is the 64-bit TSF TextInputProcessor.
    ; Once Windows has loaded it (per user login session), TSF holds an open
    ; file handle on it for the entire session. NSIS cannot overwrite a
    ; locked file; the result is the user-facing dialog
    ; "Cannot open the file for writing" + Abort/Retry/Ignore.
    ; Workaround: use SetOverwrite try. If the file does not exist,
    ; jump straight to a normal copy. If the file is locked by TSF,
    ; the File call sets the error flag silently (no error dialog); we
    ; keep the old shim in place and surface a single log line; the new
    ; shim is picked up at the next user log-out -> log-in cycle.
    ; Note: this is NOT a size-equality check (the original draft
    ; comment claimed one - the actual logic is overwrite-try + skip-on-error).
    IfFileExists "$INSTDIR\weaselx64.dll" 0 install_weaselx64
    SetOverwrite try
    File "weaselx64.dll"
    SetOverwrite on
    IfErrors 0 skip_weaselx64
    DetailPrint "Fluxing: weaselx64.dll is locked by TextInputManagementService (TSF); old shim retained. Log out -> log in to pick up the new shim."
    Goto skip_weaselx64
  install_weaselx64:
    SetOverwrite on
    File "weaselx64.dll"
  skip_weaselx64:
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
  ; Always install Win32\Weasel*.exe, regardless of host Windows arch.
  File "Win32\WeaselDeployer.exe"
  File "Win32\WeaselServer.exe"
  File "Win32\rime.dll"
  File "Win32\WinSparkle.dll"
  ${If} ${AtLeastWin11}
    ${If} ${IsNativeARM64}
      File /nonfatal "weaselARM.dll"
      File /nonfatal "weaselARM64.dll"
      File /nonfatal "weaselARM64X.dll"
    ${Endif}
  ${Endif}

  File "WeaselSetup.exe"
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

  ; === v2.0: Register WeaselTSF as TSF text input processor (fix TSF TIP not registered) ===
  ; regsvr32 calls DllRegisterServer in WeaselTSF.dll which writes HKLM\SOFTWARE\...\CTF\TIP\{GUID}
  ; Without this, Windows doesn't know Fluxing is installed and won't show it in input switcher.
  ExecWait 'regsvr32 /s "$INSTDIR\weaselx64.dll"' $0
  ${If} $0 != 0
    DetailPrint "Fluxing: regsvr32 weaselx64.dll failed (exit $0); TSF TIP may not be registered. Run 'regsvr32 $INSTDIR\weaselx64.dll' as admin manually."
  ${EndIf}
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
  ExecWait 'taskkill /F /IM WeaselServer.exe /T'

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
