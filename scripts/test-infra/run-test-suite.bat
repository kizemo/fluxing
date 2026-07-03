@echo off
rem ====================================================================
rem scripts\test-infra\run-test-suite.bat
rem Build + run all 7 test projects for Fluxing. spec 030 adds TestCandidateRButtonDown.
rem
rem L30 awareness: PowerShell `cmd /c "..."` reports the cmd parent
rem process exit code, not the real %ERRORLEVEL% of the batch. The
rem L30 cure is the `endlocal & set "OUTER_RC=%FINAL_RC%"` pattern at
rem the bottom of this file. See lessons-learned.md L30 for the full
rem post-mortem of the silent -2 false-positive that motivated this.
rem
rem Usage: scripts\test-infra\run-test-suite.bat
rem Exit code: 0 = all pass, non-zero = at least one failed
rem
rem Spec 015 (2026-07-02) - replaced ad-hoc developer recall of the
rem   msbuild-with-SolutionDir incantation.
rem Spec 016 (2026-07-02) - added TestBindingResolution scaffold.
rem Spec 026 (2026-07-03) - added TestWeaselIPC orchestration (L30+L31).
rem Spec 027 (2026-07-03) - moved here from scripts\run-tests.bat;
rem   scripts\run-tests.bat is now a thin wrapper that calls this file.
rem ====================================================================

setlocal enableextensions enabledelayedexpansion

rem Resolve repo root from script location (script is in
rem <repo>\scripts\test-infra\, so %~dp0..\.. is the repo root).
set "SOL_DIR=%~dp0..\.."
pushd "%SOL_DIR%"
set "SOL_DIR=%CD%"
popd

rem Toolchain locations (matches AGENTS.md sec 2.1 prerequisite environment)
if "%BOOST_ROOT%"=="" set "BOOST_ROOT=F:\b183"
set "VCVARS=C:\PROGRA~2\MICROS~2\2022\BUILDT~1\VC\AUXILI~1\Build\vcvars32.bat"
set "MSBUILD=C:\PROGRA~2\MICROS~2\2022\BUILDT~1\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%VCVARS%" (
    echo [ERROR] vcvars32.bat not found at: %VCVARS%
    echo         Adjust VCVARS in this script or install VS 2022 BuildTools.
    exit /b 1
)

call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars32.bat failed to initialize environment.
    exit /b 1
)

set "FAIL=0"

rem Build each test project standalone with explicit SolutionDir.
rem Note: SolutionDir must be the absolute path WITHOUT a trailing
rem backslash (otherwise MSBuild parses the escaped quote wrong and
rem the OutDir ends up nested under test\<name>\Release\ instead of
rem the top-level Release\). See L31.
for %%P in (test\TestDefaultHotkeys test\TestShiftSelectBinding test\TestBindingResolution test\TestResponseParser test\TestWeaselIPC test\TestYamlRoundTripE2E test\TestUserDictUpdate test\TestCandidateRButtonDown) do (
    echo === Building %%P ===
    "%MSBUILD%" "%%P\%%~nP.vcxproj" /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="%SOL_DIR%" /m:1 /nologo /v:minimal
    if !errorlevel! NEQ 0 set "FAIL=1"
)

rem Run each non-integration test exe. Stdin redirected to nul to avoid system(pause) hangs
rem (defensive - the spec 015 fix removed system(pause) from all test sources,
rem but this is cheap insurance against future test code regression).
rem TestWeaselIPC is intentionally NOT in this loop: it is an integration test
rem that requires server-spawn + client + shutdown orchestration. See the
rem dedicated TestWeaselIPC block below (spec 026).
for %%E in (TestDefaultHotkeys TestShiftSelectBinding TestBindingResolution TestResponseParser TestYamlRoundTripE2E TestUserDictUpdate TestCandidateRButtonDown) do (
    echo === Running %%E.exe ===
    rem Use NEQ 0 (not "if errorlevel 1") because BOOST_ASSERT
    rem failures in optimized Release builds raise 0xC0000005 - signed
    rem -1073741819 - which is less than 1 numerically, so "if errorlevel 1"
    rem misinterprets a real failure as a pass; see L22.
    "Release\%%E.exe" < nul
    if !errorlevel! NEQ 0 set "FAIL=1"
)

rem === TestWeaselIPC integration test (spec 026) ===
rem The test exe has 3 modes: /start (spawn server in background), no-arg (client),
rem /stop (shutdown server). We must spawn /start first, give the named pipe time
rem to come up, then run the client, then /stop. Without this orchestration, the
rem client mode returns -2 (STATUS_INVALID_HANDLE) because the named pipe has no
rem listener. See L30 for the full post-mortem of the silent -2 failure that
rem was mis-classified as "smoke test" for 4 sessions.
echo === Setting up TestWeaselIPC server (background, /start) ===
start "" /B "Release\TestWeaselIPC.exe" /start
rem ping -n 3 127.0.0.1 >nul gives ~2s wait. Using ping (not timeout / sleep)
rem because Windows may not have timeout / sleep on PATH. Per L22 "Windows-isms".
ping -n 3 127.0.0.1 >nul
echo === Running TestWeaselIPC.exe (client mode) ===
"Release\TestWeaselIPC.exe" < nul
set "IPC_RC=!errorlevel!"
echo === Shutting down TestWeaselIPC server (/stop) ===
"Release\TestWeaselIPC.exe" /stop < nul
if !IPC_RC! NEQ 0 set "FAIL=1"

if "!FAIL!"=="0" (
    echo.
    echo === ALL TESTS PASSED ===
) else (
    echo.
    echo === TESTS FAILED ===
)

rem Proven pattern: capture FAIL into a local var BEFORE the endlocal,
rem then have endlocal + set propagate the value to outer scope. The
rem L30 cure - see lessons-learned.md L30 for the full post-mortem.
set "FINAL_RC=!FAIL!"
endlocal & set "OUTER_RC=%FINAL_RC%"
exit /b %OUTER_RC%