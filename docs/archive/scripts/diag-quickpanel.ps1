$ErrorActionPreference = 'Continue'

Write-Host '=== 1. WeaselServer status (BEFORE Alt+,) ===' -ForegroundColor Cyan
$b = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,StartTime,CPU,WS
if ($b) {
  $b | Format-Table -AutoSize
  "PID_before = $($b.Id)"
} else {
  Write-Host 'WeaselServer.exe not running' -ForegroundColor Red
}

Write-Host ''
Write-Host 'Press Alt+, ONCE, wait 8 seconds, press Alt+, ONCE more, wait 5 seconds. Then press Enter to continue...' -ForegroundColor Yellow
Read-Host

Write-Host '=== 1b. WeaselServer status (AFTER Alt+,) ===' -ForegroundColor Cyan
$a = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,StartTime,CPU,WS
if ($a) {
  $a | Format-Table -AutoSize
  "PID_after = $($a.Id)"
} else {
  Write-Host 'WeaselServer.exe NOT running anymore' -ForegroundColor Red
}

Write-Host ''
if ($b -and $a) {
  if ($b.Id -ne $a.Id) {
    Write-Host "CONCLUSION: PID changed ($($b.Id) -> $($a.Id)) = WeaselServer restarted" -ForegroundColor Magenta
  } else {
    Write-Host "CONCLUSION: PID unchanged = WeaselServer stayed alive" -ForegroundColor Green
  }
} elseif ($b -and -not $a) {
  Write-Host 'CONCLUSION: WeaselServer disappeared AFTER Alt+, = CRASHED' -ForegroundColor Magenta
} elseif (-not $b -and $a) {
  Write-Host 'CONCLUSION: WeaselServer appeared during the test (was not running before Alt+,)' -ForegroundColor Yellow
} else {
  Write-Host 'CONCLUSION: WeaselServer not running either time - fluxing IME will not work at all' -ForegroundColor Red
}

Write-Host ''
Write-Host '=== 2. rime.weasel INFO log files ===' -ForegroundColor Cyan
$dirs = @("$env:TEMP\rime.weasel", "$env:LOCALAPPDATA\rime.weasel")
$hit = $false
foreach ($d in $dirs) {
  if (Test-Path $d) {
    $fs = Get-ChildItem -Path $d -Filter '*.INFO' -EA SilentlyContinue | Sort-Object LastWriteTime -Descending
    foreach ($f in $fs) {
      $hit = $true
      Write-Host "--- file: $($f.FullName) (size=$($f.Length), mtime=$($f.LastWriteTime)) ---"
      Get-Content -Path $f.FullName -Tail 80
      Write-Host ''
    }
  }
}
if (-not $hit) {
  Write-Host 'no INFO file found' -ForegroundColor Yellow
}

Write-Host ''
Write-Host '=== 3. Recent Application Error events (last 10) ===' -ForegroundColor Cyan
Get-WinEvent -LogName Application -MaxEvents 300 -ErrorAction SilentlyContinue |
  Where-Object { $_.LevelDisplayName -eq 'Error' -or $_.ProviderName -match 'WerFault' } |
  Select-Object -First 10 -Property TimeCreated,ProviderName,Id,LevelDisplayName,@{N='Msg';E={$_.Message.Substring(0,[Math]::Min(500,$_.Message.Length))}} |
  Format-List

Write-Host ''
Write-Host '=== 4. Recent events mentioning WeaselServer ===' -ForegroundColor Cyan
Get-WinEvent -LogName Application -MaxEvents 500 -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'WeaselServer' } |
  Select-Object -First 5 -Property TimeCreated,ProviderName,Id,LevelDisplayName,@{N='Msg';E={$_.Message.Substring(0,[Math]::Min(800,$_.Message.Length))}} |
  Format-List
