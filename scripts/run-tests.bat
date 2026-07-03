@echo off
rem ====================================================================
rem scripts\run-tests.bat
rem Thin wrapper around scripts\test-infra\run-test-suite.bat.
rem Kept for backward-compat with anyone calling the old entry point.
rem
rem Spec 027 (2026-07-03) - moved the build+run work to
rem scripts\test-infra\run-test-suite.bat; this file is now 5 lines.
rem See .specify\specs\027-test-infra-hardening\spec.md.
rem ====================================================================
cmd /c "%~dp0test-infra\run-test-suite.bat" %*
exit /b %ERRORLEVEL%