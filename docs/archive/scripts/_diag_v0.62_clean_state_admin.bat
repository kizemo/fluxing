@echo off
REM _diag_v0.62_clean_state_admin.bat
REM Purpose: clean ONLY state pollution from v0.66+ ship disasters.
REM          MUST run as ADMIN (System32 delete requires admin).
REM          Run BEFORE reinstalling any Fluxing version (v0.62 / v0.74.3 / etc).
REM
REM CRITICAL INVARIANT (per L##-PhaseL-9.13):
REM   This script PRESERVES all critical registry keys:
REM     - HKLM\Software\Fluxing\Weasel\InstallDir
REM     - HKCU\Software\Fluxing\Weasel\RimeUserDir
REM     - HKLM\Software\Microsoft\Windows\CurrentVersion\Run\WeaselServer
REM     - HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{...}
REM     - HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{...}
REM
REM   These keys are CRITICAL for WeaselServer.exe startup and librime init.
REM   v0.62 binary will NOT work without these keys.
REM
REM   This script only deletes:
REM     - System32 shim DLLs (weasel*.dll / weasel*.ime)
REM     - Stage 2 dirs (TEMP\fluxing-stage2-shim / TEMP\fluxing-staged)
REM     - Stage 2 scheduled tasks (Fluxing*)
REM     - Dead marker keys (Stage2Done / AdminRequired) - these are v0.74-specific
REM
REM Date: 2026-07-31
REM Author: Claude (per L##-PhaseL-9.10 + L##-PhaseL-9.6 + L##-PhaseL-9.13 forensics)

setlocal enabledelayedexpansion

REM --- 1. Verify admin ---
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] This script MUST run as Administrator.
    echo         Right-click cmd.exe -^> "Run as administrator" -^> then run this script.
    exit /b 1
)

echo ============================================================
echo  Fluxing state pollution cleanup ^(v0.66+ ship disaster residue^)
echo  PRESERVES critical InstallDir / RimeUserDir / Run / CTF keys
echo  Run as ADMIN. Reinstall Fluxing AFTER this + restart.
echo ============================================================
echo.

REM --- 2. Stop TSF hosts that hold shim DLLs ---
echo [1/6] Stopping ctfmon.exe and TextInputHost.exe...
taskkill /F /IM ctfmon.exe /T 2>&1
taskkill /F /IM TextInputHost.exe /T 2>&1
echo       Wait 5 seconds for handle release...
timeout /t 5 /nobreak >nul
echo.

REM --- 3. Remove System32 shim DLLs (CRITICAL) ---
echo [2/6] Removing System32 shim DLLs (the smoking gun)...
set "SYS32=%WINDIR%\System32"

REM Try Delete first; if fails (locked), use Win32 MoveFileEx with DELAY_UNTIL_REBOOT
for %%f in (
    "%SYS32%\weasel.dll"
    "%SYS32%\weaselx64.dll"
    "%SYS32%\weasel.ime"
    "%SYS32%\weasel.old.dll"
    "%SYS32%\weasel.dll.bak"
    "%SYS32%\weasel.dll.old"
    "%SYS32%\weasel.dll.old.10"
    "%SYS32%\weasel.dll.old.135039"
    "%SYS32%\weasel.dll.v465"
    "%SYS32%\weasel.dll.v466bak-20260623-095658"
    "%SYS32%\weaselx64.dll.old"
    "%SYS32%\weaselx64.dll.old.133147"
    "%SYS32%\weaselx64.ime"
    "%SYS32%\weaselx64.ime.old.133147"
) do (
    if exist %%f (
        echo       Removing %%f ...
        del /F /A:H %%f 2>&1
        if exist %%f (
            echo         ^>^> FIRST DEL FAILED. Using Win32 MoveFileEx with DELAY_UNTIL_REBOOT.
            REM Use PowerShell to call Win32 MoveFileEx with MOVEFILE_DELAY_UNTIL_REBOOT
            powershell -NoProfile -Command "$f = '%%f'; $type = Add-Type -Name W32 -Namespace Win32 -MemberDefinition '[DllImport(\"kernel32.dll\", SetLastError=true, CharSet=CharSet.Unicode)] public static extern bool MoveFileEx(string s, string t, uint f);' -PassThru; $type::MoveFileEx($f, $null, 0x4) | Out-Null"
            if exist %%f (
                echo         ^>^> STILL ON DISK. Will be removed on next reboot.
                set "SCHEDULE_REBOOT=1"
            ) else (
                echo         ^>^> Removed via PowerShell.
            )
        ) else (
            echo         ^>^> Removed.
        )
    )
)
echo.

REM --- 4. Remove Stage 2 atomic swap residue ---
echo [3/6] Removing Stage 2 residue (TEMP dirs)...
set "STAGE2_PATH=%TEMP%\fluxing-stage2-shim"
if exist "%STAGE2_PATH%" (
    rd /S /Q "%STAGE2_PATH%" 2>&1
    echo       Removed %STAGE2_PATH%
)
set "STAGE2_PATH=%TEMP%\fluxing-staged"
if exist "%STAGE2_PATH%" (
    rd /S /Q "%STAGE2_PATH%" 2>&1
    echo       Removed %STAGE2_PATH%
)
echo.

REM --- 5. Remove Fluxing scheduled tasks (Stage 2) ---
echo [4/6] Removing Fluxing scheduled tasks (Stage 2 atomic swap)...
for /f "tokens=*" %%t in ('schtasks /Query /FO LIST 2^>nul ^| findstr /C:"Fluxing"') do (
    set "TASK_LINE=%%t"
    echo       Found: !TASK_LINE!
)
echo       If any task listed above, run as admin:
echo       schtasks /Delete /TN "FluxingStage2ShimSwap_xxx" /F
echo.

REM --- 6. PRESERVE critical keys (DO NOT DELETE per L##-PhaseL-9.13) ---
echo [5/6] PRESERVING critical registry keys (DO NOT delete these)... ^(L##-PhaseL-9.13^)
echo       HKLM\Software\Fluxing\Weasel\InstallDir  - PRESERVED (WeaselServer.exe needs this)
echo       HKCU\Software\Fluxing\Weasel\RimeUserDir - PRESERVED (librime init needs this)
echo       HKLM\Software\Microsoft\Windows\CurrentVersion\Run\WeaselServer - PRESERVED (auto-start)
echo       HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\* - PRESERVED (L66-fix TSF)
echo       HKLM\SOFTWARE\Microsoft\CTF\KnownClasses\{...} - PRESERVED (L66-fix TSF class)
echo.

REM --- 7. Remove only DEAD marker keys (v0.74.3 specific) ---
echo [6/6] Removing dead v0.74.3 marker keys (only those marked with ^<6002^> / ^<6001^> state)...
reg delete "HKLM\Software\Fluxing\Weasel\Stage2Done" /f 2>&1
reg delete "HKLM\Software\Fluxing\Weasel\AdminRequired" /f 2>&1
echo.

REM --- 8. Verify cleanup ---
echo === Verification ===
echo.
echo --- System32 shim DLLs (should be EMPTY) ---
dir /b "%SYS32%\weasel*" 2>&1
echo.
echo --- Stage 2 dirs (should be EMPTY) ---
if exist "%TEMP%\fluxing-stage2-shim" echo STILL PRESENT: %TEMP%\fluxing-stage2-shim
if exist "%TEMP%\fluxing-staged" echo STILL PRESENT: %TEMP%\fluxing-staged
echo (empty = clean)
echo.
echo --- LuxingUserData dir (should still exist for librime) ---
if exist "%APPDATA%\FluxingUserData" (
    echo   %APPDATA%\FluxingUserData exists
) else (
    echo   WARNING: %APPDATA%\FluxingUserData missing - installer will need to recreate
)
echo.

REM --- 9. Final summary ---
echo ============================================================
echo  Cleanup status
echo ============================================================
if defined SCHEDULE_REBOOT (
    echo  SYSTEM32 SHIM DLLS LOCKED - REBOOT REQUIRED FOR FINAL CLEANUP.
    echo  Run: shutdown /r /t 0 ^(after saving open work^)
) else (
    echo  System32 shim DLLs removed successfully.
    echo  CRITICAL registry keys preserved.
    echo  Stage 2 dirs cleared.
    echo.
    echo  NEXT STEP: install Fluxing v0.62 ^(md5 e4186a1affbb4950a1d25cabb257681a^)
    echo              from `release\fluxing-0.19.0.62-installer.exe`
    echo              OR
    echo              install v0.18.41.4 (L69-fix QuickPanel disabled, oldest stable).
    echo.
    echo  After install: verify critical keys still present:
    echo    reg query HKLM\Software\Fluxing\Weasel /v InstallDir
    echo    reg query HKCU\Software\Fluxing\Weasel /v RimeUserDir
    echo    reg query HKLM\Software\Microsoft\Windows\CurrentVersion\Run /v WeaselServer
)
echo.
echo  After install + admin reboot, verify:
echo    - System32 has NO weasel*.dll / weasel*.ime files
echo    - WeaselServer.exe process is running
echo    - 24h Application Error event count 0xc0000374 less than 5
echo    - Input Chinese in any app does not crash Explorer / Claude / dopus
echo ============================================================

endlocal
