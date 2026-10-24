$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# v0.19.0.52 (Phase K3 T014): build wrapper that ensures vcvars + cwd are
# correct before xbuild.bat picks them up. Direct powershell `cmd /c "call
# vcvars & cd & call xbuild"` 解析时 cmd 不识别 "call" 后面的路径, 必须
# 用 .cmd wrapper 把 vcvars + cd + xbuild 串成一段, 然后 powershell 调
# wrapper。这样也避开 vcvars 输出污染 stdout。
$wrapperPath = Join-Path $PSScriptRoot "_build_v052_step1.cmd"

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

Write-Host "Running: xbuild.bat weasel installer (v0.19.0.52)"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "`"$wrapperPath`""
) -NoNewWindow -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

Remove-Item $wrapperPath -Force
exit $proc.ExitCode
