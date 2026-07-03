@echo off
rem ====================================================================
rem scripts\run-tests.bat
rem Build + run all 5 test projects for Fluxing.
rem Usage: scripts\run-tests.bat
rem Exit code: 0 = all pass, non-zero = at least one failed
rem
rem Why a script:
rem - Test vcxproj files use $(SolutionDir)\include for headers.
rem - When built standalone, $(SolutionDir) defaults to the
rem   vcxproj's own dir, breaking the include path.
rem - This script passes /p:SolutionDir=<repo-root>\ to override.
rem
rem Spec 015 (2026-07-02) - replaces ad-hoc developer recall of
rem the msbuild-with-SolutionDir incantation. See
rem .specify\specs\015-fix-test-infrastructure\spec.md for context.
rem
rem Spec 016 (2026-07-02) - added TestBindingResolution (behavior-level
rem test framework scaffold). See
rem .specify\specs\016-behavior-level-test-framework\spec.md for context.
rem ====================================================================

setlocal enableextensions enabledelayedexpansion

rem Resolve repo root from script location (script is in <repo>\scripts\)
set "SOL_DIR=%~dp0.."
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
rem the top-level Release\).
for %%P in (test\TestDefaultHotkeys test\TestShiftSelectBinding test\TestBindingResolution test\TestResponseParser test\TestWeaselIPC test\TestYamlRoundTripE2E) do (
    echo === Building %%P ===
    "%MSBUILD%" "%%P\%%~nP.vcxproj" /t:Build /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="%SOL_DIR%" /m:1 /nologo /v:minimal
    if !errorlevel! NEQ 0 set "FAIL=1"
)

rem Run each test exe. Stdin redirected to nul to avoid system(pause) hangs
rem (defensive - the spec 015 fix removed system(pause) from all test sources,
rem but this is cheap insurance against future test code regression).
for %%E in (TestDefaultHotkeys TestShiftSelectBinding TestBindingResolution TestResponseParser TestWeaselIPC TestYamlRoundTripE2E) do (
    echo === Running %%E.exe ===
    rem Use NEQ 0 (not "if errorlevel 1") because BOOST_ASSERT
    rem failures in optimized Release builds raise 0xC0000005 (signed
    rem -1073741819), which is < 1 numerically, so "if errorlevel 1"
    rem misinterprets a real failure as a pass.
    "Release\%%E.exe" < nul
    if !errorlevel! NEQ 0 set "FAIL=1"
)

if "!FAIL!"=="0" (
    echo.
    echo === ALL TESTS PASSED ===
) else (
    echo.
    echo === TESTS FAILED ===
)

rem Proven pattern: capture FAIL into a local var BEFORE the endlocal,
rem then have endlocal + set propagate the value to outer scope.
set "FINAL_RC=!FAIL!"
endlocal & set "OUTER_RC=%FINAL_RC%"
exit /b %OUTER_RC%
