@echo off
rem ====================================================================
rem scripts\sandbox-gate\run-verify.cmd
rem One-shot entry point for the Fluxing sandbox claims gate.
rem Pure ASCII on purpose: the file is launched by CI and by double click,
rem and a UTF-8 BOM in a .cmd breaks the first line on some hosts.
rem
rem Usage:
rem   run-verify.cmd                                  (uses the repo next to this script)
rem   run-verify.cmd -RepoRoot D:\other\repo
rem   run-verify.cmd -NoWait
rem
rem Exit code: 0 = all claims passed, 1 = some claims failed, 2 = timeout
rem ====================================================================
setlocal
set "SELF_DIR=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%SELF_DIR%run-verify.ps1" %*
set "RC=%ERRORLEVEL%"
endlocal & set "OUTER_RC=%RC%"
exit /b %OUTER_RC%
