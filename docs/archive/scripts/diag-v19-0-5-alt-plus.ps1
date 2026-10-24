# diag-v19-0-5-alt-plus.ps1 (L47 fix: ASCII only, no Chinese in single-quote)
$ErrorActionPreference = 'Continue'

Write-Host '=== 1. WeaselServer.exe running? ===' -ForegroundColor Cyan
$ws = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,StartTime,CPU,WS
if ($ws) {
  $ws | Format-Table -AutoSize
  "PID: $($ws.Id), start: $($ws.StartTime)"
} else {
  Write-Host 'WeaselServer.exe NOT running' -ForegroundColor Red
}

Write-Host ''
Write-Host '=== 2. weasel.dll timestamp + version ===' -ForegroundColor Cyan
$weaselDir = "D:\Program Files\fluxing\weasel"
if (Test-Path "$weaselDir\weasel.dll") {
  $dll = Get-Item "$weaselDir\weasel.dll"
  Write-Host "weasel.dll: mtime=$($dll.LastWriteTime), size=$($dll.Length)"
  $vi = (Get-Item "$weaselDir\weasel.dll").VersionInfo
  Write-Host "  FileVersion:    $($vi.FileVersion)"
  Write-Host "  ProductVersion: $($vi.ProductVersion)"
} else {
  Write-Host 'weasel.dll does not exist' -ForegroundColor Red
}
if (Test-Path "$weaselDir\weasel.dll.old.tmp") {
  Write-Host 'weasel.dll.old.tmp exists (L72 Rename leftover not cleaned)' -ForegroundColor Yellow
}
Write-Host '  Directory contents (weasel* and FLUXING* only):'
Get-ChildItem $weaselDir -ErrorAction SilentlyContinue | Where-Object { $_.Name -match 'weasel|FLUXING|Fluxing' } | Format-Table Name,Length,LastWriteTime -AutoSize | Out-String | Write-Host

Write-Host ''
Write-Host '=== 3. 24h Windows Event Viewer (WeaselServer / Application Error) ===' -ForegroundColor Cyan
Get-WinEvent -LogName Application -MaxEvents 200 -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'WeaselServer|fluxing\.exe|0xc000|0xC000|STATUS_HEAP' -or $_.ProviderName -eq 'Application Error' -or $_.ProviderName -eq 'Windows Error Reporting' } |
  Select-Object -First 10 -Property TimeCreated, ProviderName, Id, LevelDisplayName, @{N='Msg';E={$_.Message.Substring(0,[Math]::Min(500,$_.Message.Length))}} |
  Format-List

Write-Host ''
Write-Host '=== 4. CrashDumps dir ===' -ForegroundColor Cyan
$crashDirs = @(
  "$env:USERPROFILE\AppData\Local\CrashDumps\WeaselServer.*.dmp",
  "$env:USERPROFILE\AppData\Local\fluxing\crash\*.dmp"
)
foreach ($p in $crashDirs) {
  $matches = Get-ChildItem -Path (Split-Path $p) -Filter (Split-Path -Leaf $p) -ErrorAction SilentlyContinue
  if ($matches) {
    Write-Host "  Found: $($matches.Count) dump(s) at $(Split-Path $p)"
    $matches | Sort-Object LastWriteTime -Descending | Select-Object -First 5 | Format-Table Name,Length,LastWriteTime -AutoSize | Out-String | Write-Host
  } else {
    Write-Host "  (none) $p"
  }
}

Write-Host ''
Write-Host '=== 5. WMI process check ===' -ForegroundColor Cyan
$wmi = Get-CimInstance -ClassName Win32_Process -Filter "Name='WeaselServer.exe'" -ErrorAction SilentlyContinue
if ($wmi) {
  $wmi | Select-Object ProcessId, CommandLine | Format-List
} else {
  Write-Host '  WeaselServer.exe not in WMI list either'
}

Write-Host ''
Write-Host '=== 6. Done - please paste the 4 sections above ===' -ForegroundColor Green
