$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"

# Spawn a fresh cmd via Start-Process so it picks up new env vars
Write-Host "Running: cmd /c 'vcvars32 && xbuild.bat weasel installer'"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "call `"$vcvars`" >nul 2>&1 & cd /d F:\soft\00selfmade\rime_claude & call F:\soft\00selfmade\rime_claude\xbuild.bat weasel installer"
) -NoNewWindow -Wait -PassThru
Write-Host "Exit code: $($proc.ExitCode)"
exit $proc.ExitCode
