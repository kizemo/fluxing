$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# v0.19.0.55 (Phase K3 T019 hotfix): build wrapper.
# Hardcoded path to avoid $PSScriptRoot quirks when invoked via Start-Process
# from background runner.
$wrapperPath = "F:\soft\00selfmade\rime_claude\_build_v055_step1.cmd"

@"
@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d F:\soft\00selfmade\rime_claude
call "F:\soft\00selfmade\rime_claude\env.bat"
echo.
echo === After env.bat ===
echo WEASEL_BUILD=%WEASEL_BUILD%
echo PRODUCT_VERSION=%PRODUCT_VERSION%
echo FLUXING_VERSION=%FLUXING_VERSION%
echo RELEASE_BUILD=%RELEASE_BUILD%
echo.
echo === Calling xbuild.bat ===
call "F:\soft\00selfmade\rime_claude\xbuild.bat" weasel installer
echo.
echo Exit code: %ERRORLEVEL%
"@ | Out-File -FilePath $wrapperPath -Encoding ASCII

Write-Host "Running: xbuild.bat weasel installer (v0.19.0.55)"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "`"$wrapperPath`""
) -NoNewWindow -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

Remove-Item $wrapperPath -Force
exit $proc.ExitCode
