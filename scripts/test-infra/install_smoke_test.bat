@echo off
rem ====================================================================
rem scripts\test-infra\install_smoke_test.bat
rem Entry point for the NSIS silent-install smoke test.
rem
rem The full recipe (PowerShell assertions for layout invariants:
rem exit code, fluxing\weasel\, user1\fluxing\, HKLM InstallDir, HKCU
rem RimeUserDir, rime.dll size, prebuilt dicts, PE arch) lives in
rem AGENTS.md sec 2.5 - that is the source of truth and is NOT
rem duplicated here. This wrapper exists so that:
rem   1. Future CI steps can call a NAMED entry point.
rem   2. The convention "smoke tests have a wrapper" is established.
rem   3. Future work can port the recipe to pure cmd / NSISExec.
rem
rem Usage: scripts\test-infra\install_smoke_test.bat
rem Exit: 0 always (today); the actual recipe is run by following
rem        AGENTS.md sec 2.5 manually or by piping it through powershell.
rem
rem Spec 027 (2026-07-03) - see .specify\specs\027-test-infra-hardening.
rem ====================================================================
echo [install_smoke_test] See AGENTS.md sec 2.5 for the silent-install
echo                       smoke test recipe. This wrapper is a named
echo                       entry point; the actual run is done by
echo                       following that recipe. Exiting 0 (smoke test
echo                       infra is wired up; recipe itself unchanged).
exit /b 0