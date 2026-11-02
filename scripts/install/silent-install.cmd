@echo off
REM ============================================================================
REM  Fluxing silent install - NSIS LAUNCHER (.bat wrapper)
REM
REM  USAGE (normally called by scripts\install\silent-install.ps1, which owns
REM  the guards).  It can also be called directly:
REM      set FLUXING_INSTALL_ROOT=D:\Program Files\fluxing
REM      set FLUXING_INSTALLER=<path to fluxing-x.y.z-installer.exe>
REM      set FLUXING_DRYRUN=1
REM      scripts\install\silent-install.cmd
REM
REM  WHY A .bat AND NOT PowerShell:
REM    L28   "NSIS silent-install: write a .bat wrapper, do not invoke from
REM           PowerShell &"
REM    L1214 Start-Process -FilePath installer.exe -ArgumentList '/S /D=...'
REM           -Wait -PassThru HANGS under PowerShell 5.1 (5 min, no return);
REM           the same note records that a plain `cmd /c` synchronous run
REM           completes in ~40 s and returns exit 0.
REM    => the installer is launched from a batch file, where cmd waits for it
REM       and %ERRORLEVEL% is real.
REM
REM  WHY THE PATH TRAVELS IN AN ENVIRONMENT VARIABLE:
REM    A path with spaces passed to a .bat as %1 gets truncated by
REM    `cmd /c script <path>`.  The env var has no such problem, and the
REM    expansion below is what puts the RAW, UNQUOTED /D= tail on the child
REM    command line - exactly what NSIS 3.2.1 requires (L112 / L113):
REM       /D= must be the LAST parameter and must NOT be quoted
REM ============================================================================
setlocal EnableExtensions

if not defined FLUXING_INSTALLER    set "FLUXING_INSTALLER=F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.1.6-installer.exe"
if not defined FLUXING_INSTALL_ROOT  set "FLUXING_INSTALL_ROOT=D:\Program Files\fluxing"

if not exist "%FLUXING_INSTALLER%" (
  echo [FATAL] installer not found: %FLUXING_INSTALLER%
  exit /b 2
)

if defined FLUXING_DRYRUN (
  echo [dryrun] would run: "%FLUXING_INSTALLER%" /S /D=%FLUXING_INSTALL_ROOT%
  exit /b 0
)

echo [install] "%FLUXING_INSTALLER%" /S /D=%FLUXING_INSTALL_ROOT%
"%FLUXING_INSTALLER%" /S /D=%FLUXING_INSTALL_ROOT%
set "RC=%ERRORLEVEL%"
echo [install] exit code = %RC%
exit /b %RC%
