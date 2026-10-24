$ErrorActionPreference = "Stop"
$nsisScript = "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"
Write-Host "Running: $nsisScript"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c", "`"$nsisScript`""
) -NoNewWindow -Wait -PassThru -RedirectStandardOutput "C:\Users\Duanyi\AppData\Local\Temp\fluxing-nsis-stdout.log" -RedirectStandardError "C:\Users\Duanyi\AppData\Local\Temp\fluxing-nsis-stderr.log"
Write-Host "NSIS exit code: $($proc.ExitCode)"
Write-Host ""
Write-Host "=== stdout ==="
Get-Content "C:\Users\Duanyi\AppData\Local\Temp\fluxing-nsis-stdout.log" -ErrorAction SilentlyContinue | Select-Object -Last 50
Write-Host ""
Write-Host "=== stderr ==="
Get-Content "C:\Users\Duanyi\AppData\Local\Temp\fluxing-nsis-stderr.log" -ErrorAction SilentlyContinue | Select-Object -Last 30
Write-Host ""
Write-Host "=== installer check ==="
Get-ChildItem "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.66-installer.exe" -ErrorAction SilentlyContinue | Format-Table FullName, Length, LastWriteTime