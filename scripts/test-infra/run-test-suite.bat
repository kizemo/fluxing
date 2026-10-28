@echo off
rem ====================================================================
rem scripts\test-infra\run-test-suite.bat
rem Build + run all test projects for Fluxing. spec 031 adds TestCandidateIgnoreFilter. spec 033 adds TestDarkModeBridge. spec 034 adds TestDarkModeBroadcast. spec 036 adds TestQuickPanelDialog. spec 037 adds TestFluxingComponents. spec 042 adds TestOrphanRecovery. spec 076 adds TestPipeChannelRace. spec 077 adds TestShiftIPCStateMachine + TestShiftIPCWire.
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

rem ---------------------------------------------------------------------------
rem Visual Studio discovery (L137).
rem
rem This used to hardcode one developer machine's BuildTools install:
rem   C:\PROGRA~2\MICROS~2\2022\BUILDT~1\VC\AUXILI~1\Build\vcvars32.bat
rem The GitHub-hosted windows runners install the same Visual Studio 2022
rem under the "Enterprise" edition folder instead:
rem   C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat
rem The hardcoded path aborted the suite on its very first step, so the test
rem suite had never executed in CI at all -- see lessons-learned.md L137.
rem
rem Discovery order, first hit wins:
rem   1. an explicit VCVARS / MSBUILD override in env.bat - never clobbered
rem   2. the legacy Visual Studio 2022 BuildTools folder - the known-good
rem      toolchain for this repo (PLATFORM_TOOLSET=v143)
rem   3. vswhere, preferring an installation under a \2022\ directory
rem   4. vswhere, any installation at all
rem
rem "Latest" is deliberately NOT the rule: a machine that also carries a
rem newer Visual Studio would silently switch toolchains. Nor can a
rem vswhere version RANGE be used -- the ")" of "[17.0,18.0)" truncates a
rem for /f backquoted command.
rem ---------------------------------------------------------------------------
if not defined VCVARS call :discover_vcvars
if not defined MSBUILD call :discover_msbuild

if not defined MSBUILD goto no_msbuild
if defined VCVARS goto have_vcvars
goto no_vcvars

:have_vcvars
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars32.bat failed to initialize the environment.
    exit /b 1
)
echo [INFO] Visual Studio environment: %VCVARS%
goto toolchain_ready

:no_vcvars
echo [WARN] vcvars32.bat not found - continuing with the environment as-is.
echo        Set VCVARS in env.bat if the MSVC tools are not already set up.
goto toolchain_ready

:no_msbuild
echo [ERROR] MSBuild.exe not found.
echo         Install Visual Studio 2022 - any edition works - or set MSBUILD
echo         and VCVARS in env.bat to point at your own installation.
exit /b 1

:toolchain_ready

rem L49 regression guard: WeaselServerImpl.h MUST have MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)
rem in its msg map. spec 036 (0.18.24.0) shipped without it (function declared but
rem never routed), making Alt+, global hotkey a no-op for 3 release versions
rem (0.18.24.0 / 0.18.25.0 / 0.18.26.0 / 0.18.27.0) before L49 + 0.18.27.1 fixed it.
rem This pre-flight check catches any future commit that removes the line again.
findstr /C:"MESSAGE_HANDLER(WM_HOTKEY, OnHotkey)" WeaselIPCServer\WeaselServerImpl.h >nul
if errorlevel 1 (
    echo [L49 GUARD FAIL] WeaselServerImpl.h missing MESSAGE_HANDLER^(WM_HOTKEY, OnHotkey^).
    echo                 Alt+, global hotkey will not route to OnHotkey. Restore the line
    echo                 in BEGIN_MSG_MAP^(WEASEL_IPC_WINDOW^) ^< END_MSG_MAP^(^).
    exit /b 1
)

set "FAIL=0"

rem Build each test project standalone with explicit SolutionDir.
rem Note: SolutionDir must be the absolute path WITHOUT a trailing
rem backslash (otherwise MSBuild parses the escaped quote wrong and
rem the OutDir ends up nested under test\<name>\Release\ instead of
rem the top-level Release\). See L31.
for %%P in (test\TestDefaultHotkeys test\TestShiftSelectBinding test\TestBindingResolution test\TestResponseParser test\TestWeaselIPC test\TestYamlRoundTripE2E test\TestUserDictUpdate test\TestCandidateRButtonDown test\TestCandidateIgnoreFilter test\TestPanelDarkModeSubscribe test\TestTrayRestoreIgnored test\TestDarkModeBridge test\TestDarkModeBroadcast test\TestQuickPanelDialog test\TestFluxingComponents test\TestQuickPanelRefactor test\TestOrphanRecovery test\TestShiftIPCStateMachine test\TestShiftIPCWire test\TestPipeChannelRace test\TestHotkeyBindings) do (
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
for %%E in (TestDefaultHotkeys TestShiftSelectBinding TestBindingResolution TestResponseParser TestYamlRoundTripE2E TestUserDictUpdate TestCandidateRButtonDown TestCandidateIgnoreFilter TestPanelDarkModeSubscribe TestTrayRestoreIgnored TestDarkModeBridge TestDarkModeBroadcast TestQuickPanelDialog TestFluxingComponents TestQuickPanelRefactor TestOrphanRecovery TestShiftIPCStateMachine TestShiftIPCWire TestPipeChannelRace) do (
    echo === Running %%E.exe ===
    rem Use NEQ 0 (not "if errorlevel 1") because BOOST_ASSERT
    rem failures in optimized Release builds raise 0xC0000005 - signed
    rem -1073741819 - which is less than 1 numerically, so "if errorlevel 1"
    rem misinterprets a real failure as a pass; see L22.
    "Release\%%E.exe" < nul
    if !errorlevel! NEQ 0 set "FAIL=1"
)

rem === TestHotkeyBindings hotkey acceptance gate ===
rem Two invocations, both of which must stay green:
rem   1) the negative control -- proves the gate can tell a healthy binding
rem      from a broken one. A gate that flagged everything would fail here,
rem      because the fixture also contains a binding that really works.
rem   2) the shipped configuration -- every binding that can never fire has
rem      to be listed in known-dead-bindings.txt, otherwise the gate is red.
rem      The scan covers default.yaml plus all three user-custom overrides:
rem      each override ships its own copy of the Shift+punctuation bindings,
rem      so scanning only one of them would leave the other two unchecked.
rem      The baseline is empty on purpose (ADR 0009 / L149), so the gate must
rem      report dead=0 and known_dead=0.
rem The generated keysym table is checked against librime's own key_table.cc
rem so it cannot silently drift away from the library it mirrors.
echo === Running TestHotkeyBindings.exe (negative control) ===
"Release\TestHotkeyBindings.exe" --expectations "test\TestHotkeyBindings\fixtures\negative-controls.expect.txt" "test\TestHotkeyBindings\fixtures\negative-controls.yaml" < nul
if !errorlevel! NEQ 0 set "FAIL=1"
echo === Running TestHotkeyBindings.exe (shipped configuration) ===
"Release\TestHotkeyBindings.exe" --baseline "test\TestHotkeyBindings\known-dead-bindings.txt" "output\data\default.yaml" "output\data\user-custom\rime_ice.custom.yaml" "output\data\user-custom\double_pinyin_sogou.custom.yaml" "output\data\user-custom\radical_pinyin.custom.yaml" < nul
if !errorlevel! NEQ 0 set "FAIL=1"
echo === Checking keysyms_generated.inc against librime key_table.cc ===
powershell -NoProfile -ExecutionPolicy Bypass -File "test\TestHotkeyBindings\gen-keysyms.ps1" -Check
if !errorlevel! NEQ 0 set "FAIL=1"
rem === TestWeaselIPC integration test (spec 026) ===
rem The test exe has 3 modes: /start (spawn server in background), no-arg (client),
rem /stop (shutdown server). We must spawn /start first, give the named pipe time
rem to come up, then run the client, then /stop. Without this orchestration, the
rem client mode returns -2 (STATUS_INVALID_HANDLE) because the named pipe has no
rem listener. See L30 for the full post-mortem of the silent -2 failure that
rem was mis-classified as "smoke test" for 4 sessions.
rem ADR 0012 guard: the mock server has to own the named pipe. If a
rem real WeaselServer.exe is already running, the mock server fails to
rem bind, dies silently, and the integration test then measures the real
rem server instead of the mock - the L30 class of bug, where a degraded
rem test still looks green. Detect the squatting process up front and
rem fail loudly instead of producing a bogus PASS or FAIL.
set "SQUAT=0"
tasklist /FI "IMAGENAME eq WeaselServer.exe" 2>NUL | findstr /I /C:"WeaselServer.exe" >NUL
if not errorlevel 1 set "SQUAT=1"
tasklist /FI "IMAGENAME eq TestWeaselIPC.exe" 2>NUL | findstr /I /C:"TestWeaselIPC.exe" >NUL
if not errorlevel 1 set "SQUAT=1"
if "!SQUAT!"=="1" (
    echo [ERROR] A WeaselServer.exe or TestWeaselIPC.exe process is already running.
    echo         It owns the named pipe, so the TestWeaselIPC mock server cannot
    echo         bind and the integration test would measure the wrong server.
    echo         Stop it and re-run. See ADR 0012.
    set "FAIL=1"
    goto ipc_done
)
echo === Setting up TestWeaselIPC server (background, /start) ===
start "" /B "Release\TestWeaselIPC.exe" /start
rem ping -n 3 127.0.0.1 >nul gives ~2s wait. Using ping (not timeout / sleep)
rem because Windows may not have timeout / sleep on PATH. Per L22 "Windows-isms".
ping -n 3 127.0.0.1 >nul
echo === Running TestWeaselIPC.exe (client mode) ===
"Release\TestWeaselIPC.exe" < nul
set "IPC_RC=!errorlevel!"
rem ADR 0012: the Shift IPC must carry a response body (the fresh status
rem and the commit text of a selected candidate). Run it while the
rem server is up, right after the client mode.
echo === Running TestWeaselIPC.exe (/verify-shift-body) ===
"Release\TestWeaselIPC.exe" /verify-shift-body < nul
if !errorlevel! NEQ 0 set "FAIL=1"
echo === Shutting down TestWeaselIPC server (/stop) ===
"Release\TestWeaselIPC.exe" /stop < nul
if !IPC_RC! NEQ 0 set "FAIL=1"
:ipc_done

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

rem ---------------------------------------------------------------------------
rem :discover_vcvars / :discover_msbuild - Visual Studio discovery for L137.
rem These are subroutines on purpose: the main flow stays free of nested
rem parentheses, because a path under "Program Files (x86)" would otherwise
rem close an if-block early and break the parser.
rem ---------------------------------------------------------------------------
:discover_vcvars
if defined VCVARS exit /b 0
call :discover_vsroot
if not defined VSROOT exit /b 0
if not exist "!VSROOT!\VC\Auxiliary\Build\vcvars32.bat" exit /b 0
set "VCVARS=!VSROOT!\VC\Auxiliary\Build\vcvars32.bat"
exit /b 0

:discover_msbuild
if defined MSBUILD exit /b 0
if not defined VSROOT call :discover_vsroot
if defined VSROOT if exist "!VSROOT!\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=!VSROOT!\MSBuild\Current\Bin\MSBuild.exe"
if defined MSBUILD exit /b 0
for /f "usebackq tokens=*" %%i in (`where MSBuild.exe`) do if not defined MSBUILD set "MSBUILD=%%i"
exit /b 0

:discover_vsroot
if defined VSROOT exit /b 0
set "VSROOT="
set "VSROOT_ANY="
if not defined LEGACY_VS set "LEGACY_VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if exist "%LEGACY_VS%\VC\Auxiliary\Build\vcvars32.bat" set "VSROOT=%LEGACY_VS%"
if defined VSROOT exit /b 0
rem vswhere.exe ships in either Program Files tree depending on the
rem install; the arm64x_wrapper\build.bat in this repo looks in both.
set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" exit /b 0
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -all -products * -property installationPath`) do call :consider_vsroot "%%i"
if defined VSROOT exit /b 0
set "VSROOT=%VSROOT_ANY%"
exit /b 0

:consider_vsroot
set "CAND=%~1"
if not defined VSROOT_ANY if exist "%~1\VC\Auxiliary\Build\vcvars32.bat" set "VSROOT_ANY=%~1"
if defined VSROOT exit /b 0
set "STRIPPED=%~1"
if not "!STRIPPED:\2022\=!"=="!STRIPPED!" if exist "%~1\VC\Auxiliary\Build\vcvars32.bat" set "VSROOT=%~1"
exit /b 0
