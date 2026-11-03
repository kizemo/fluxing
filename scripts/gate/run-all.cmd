@echo off
rem Fluxing staged gate entry point (L0 static / L3 mutation / L4 regression cases / L5 sandbox-gate).
rem ASCII only: a UTF-8 BOM here breaks the first line under cmd.exe.
setlocal
set "PS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
where pwsh >nul 2>nul
if not errorlevel 1 set "PS=pwsh"
"%PS%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-all.ps1" %*
set "RC=%ERRORLEVEL%"
endlocal & exit /b %RC%
