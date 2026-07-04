@echo off
rem ====================================================================
rem scripts\test-infra\verify-test-binaries-fresh.bat - 14 test projects (spec 033 adds TestDarkModeBridge; spec 034 adds TestDarkModeBroadcast)
rem L31 stale-binary detector. For each of the 14 test projects, compares
rem the mtime of 'Release\<Name>.exe' against the newest source mtime
rem under 'test\<Name>\'. Exits 0 if all are fresh, 1 if any are stale.
rem
rem Why: L31 documented that a wrong vcxproj OutDir path caused the
rem linker to write the .exe to a malformed path like
rem F:\soft\00selfmade\rimemsbuild\..., and run-tests.bat silently
rem ran the STALE binary from a prior build. This script makes that
rem class of bug re-detectable in seconds rather than 4 specs later.
rem
rem Why a sibling .ps1 file (not inline PowerShell -Command):
rem cmd parses the '$' in PowerShell -Command "..." strings and drops
rem them, so PowerShell sees a syntax error like 'if (-not ){' and exits
rem 1 regardless of mtimes. Calling a .ps1 file via -File avoids the
rem cmd-vs-PowerShell '$' boundary entirely. The .ps1 sibling is
rem gitignored via 'verify-stale-temp.ps1' in .gitignore (the .bat
rem wrapper is the tracked entry point; the .ps1 is a cmd-vs-PowerShell
rem interop detail).
rem
rem Why enabledelayedexpansion is required: the for-loop reads
rem '!errorlevel!' after each 'powershell' call. Without
rem enabledelayedexpansion, '!errorlevel!' is expanded at parse time
rem (when it is empty / 0) and the FAIL flag never gets set. This
rem bit spec 027 the first time; see L33.
rem
rem Usage: scripts\test-infra\verify-test-binaries-fresh.bat - 14 test projects (spec 033 adds TestDarkModeBridge; spec 034 adds TestDarkModeBroadcast)
rem Exit code: 0 = all fresh, 1 = at least one stale
rem
rem Spec 027 (2026-07-03) - see .specify\specs\027-test-infra-hardening.
rem ====================================================================

setlocal enableextensions enabledelayedexpansion

rem Resolve repo root from script location.
set "SOL_DIR=%~dp0..\.."
pushd "%SOL_DIR%"
set "SOL_DIR=%CD%"
popd
cd /d "%SOL_DIR%"

set "PS_SCRIPT=%~dp0verify-stale-temp.ps1"
if not exist "%PS_SCRIPT%" (
    echo [ERROR] Missing %PS_SCRIPT%
    echo         This file should be present in the repo. If you deleted
    echo         it, restore from git. The .bat cannot generate the .ps1
    echo         safely due to the cmd DOLLAR parsing bug (see spec 027, line 49.
    exit /b 1
)

set "FAIL=0"

for %%P in (TestDefaultHotkeys TestShiftSelectBinding TestBindingResolution TestResponseParser TestWeaselIPC TestYamlRoundTripE2E TestUserDictUpdate TestCandidateRButtonDown TestCandidateIgnoreFilter TestPanelDarkModeSubscribe TestTrayRestoreIgnored TestDarkModeBridge TestDarkModeBroadcast TestQuickPanelDialog) do (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %%P
    if !errorlevel! NEQ 0 set "FAIL=1"
)

if "!FAIL!"=="0" (
    echo.
    echo === ALL TEST BINARIES FRESH ===
) else (
    echo.
    echo === STALE TEST BINARIES DETECTED - rebuild and retry ===
)

rem L30 cure - propagate real exit code past endlocal.
set "FINAL_RC=!FAIL!"
endlocal & set "OUTER_RC=%FINAL_RC%"
exit /b %OUTER_RC%
