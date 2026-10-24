$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# v0.19.0.57 (Phase K4 UX polish): build wrapper, 走 MSBuild 路径 (L106 教训
# 后, build.bat weasel installer 不调 xmake after_build, msbuild 直接 link)。
# Hardcoded path 跟 build_v055.ps1 一致, 规避 $PSScriptRoot + Start-Process 怪癖。
$wrapperPath = "F:\soft\00selfmade\rime_claude\_build_v057_step1.cmd"

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
echo === Calling build.bat weasel installer (MSBuild path, L106 hardening) ===
call "F:\soft\00selfmade\rime_claude\build.bat" weasel installer
echo.
echo Exit code: %ERRORLEVEL%
"@ | Out-File -FilePath $wrapperPath -Encoding ASCII

Write-Host "Running: build.bat weasel installer (v0.19.0.57, MSBuild path)"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "`"$wrapperPath`""
) -NoNewWindow -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

Remove-Item $wrapperPath -Force
exit $proc.ExitCode