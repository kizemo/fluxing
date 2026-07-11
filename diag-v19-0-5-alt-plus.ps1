# diag-v19-0-5-alt-plus.ps1
# 装完 v0.19.0.5 + 重启后,Alt+, 失灵。需 4 项证据。
$ErrorActionPreference = 'Continue'

Write-Host '=== 1. WeaselServer.exe 是否在跑? ===' -ForegroundColor Cyan
$ws = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,StartTime,CPU,WS
if ($ws) {
  $ws | Format-Table -AutoSize
  "PID: $($ws.Id), start: $($ws.StartTime)"
} else {
  Write-Host 'WeaselServer.exe NOT running' -ForegroundColor Red
}

Write-Host ''
Write-Host '=== 2. weasel.dll 在哪 + 是不是 v0.19.0.5 那个 ===' -ForegroundColor Cyan
$weaselDir = "D:\Program Files\fluxing\weasel"
if (Test-Path "$weaselDir\weasel.dll") {
  $dll = Get-Item "$weaselDir\weasel.dll"
  Write-Host "weasel.dll: mtime=$($dll.LastWriteTime), size=$($dll.Length)"
  # file version
  $vi = (Get-Item "$weaselDir\weasel.dll").VersionInfo
  Write-Host "  FileVersion: $($vi.FileVersion)"
  Write-Host "  ProductVersion: $($vi.ProductVersion)"
} else {
  Write-Host "weasel.dll 不存在" -ForegroundColor Red
}
if (Test-Path "$weaselDir\weasel.dll.old.tmp") {
  Write-Host "weasel.dll.old.tmp 存在(可能没被删)" -ForegroundColor Yellow
}
Write-Host "  目录内容:"
Get-ChildItem $weaselDir -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "weasel|FLUXING|Fluxing" } | Format-Table Name,Length,LastWriteTime -AutoSize

Write-Host ''
Write-Host '=== 3. Windows Event Viewer (24h 内 WeaselServer.exe / Application Error) ===' -ForegroundColor Cyan
Get-WinEvent -LogName Application -MaxEvents 200 -ErrorAction SilentlyContinue |
  Where-Object { $_.Message -match 'WeaselServer|fluxing\.exe|0xc000|0xC000|STATUS_HEAP' -or $_.ProviderName -eq 'Application Error' -or $_.ProviderName -eq 'Windows Error Reporting' } |
  Select-Object -First 10 -Property TimeCreated, ProviderName, Id, LevelDisplayName, @{N='Msg';E={$_.Message.Substring(0,[Math]::Min(500,$_.Message.Length))}} |
  Format-List

Write-Host ''
Write-Host '=== 4. 装包相关日志(可填) ===' -ForegroundColor Cyan
$logDirs = @(
  "$env:USERPROFILE\AppData\Local\Temp\fluxing*.log",
  "$env:USERPROFILE\AppData\Local\Temp\nsis*.log",
  "$env:USERPROFILE\AppData\Local\CrashDumps\WeaselServer.*.dmp",
  "$env:USERPROFILE\AppData\Local\fluxing\crash\*.dmp"
)
foreach ($p in $logDirs) {
  if (Test-Path $p -ErrorAction SilentlyContinue) {
    Write-Host "匹配: $p"
    if (Test-Path $p -PathType Container) {
      Get-ChildItem $p -ErrorAction SilentlyContinue | Format-Table Name,Length,LastWriteTime -AutoSize | Out-String | Write-Host
    } else {
      Get-Item $p | Format-Table Name,Length,LastWriteTime -AutoSize | Out-String | Write-Host
    }
  } else {
    Write-Host "  (没有) $p"
  }
}

Write-Host ''
Write-Host '=== 5. Alt+, 后的额外 trace: 重新按一次 Alt+, 看 hook 触发没 ===' -ForegroundColor Cyan
Write-Host '如果你能看到 Task Manager > Details > WeaselServer.exe,'
Write-Host '右键 > Properties > 检查命令行'
Write-Host '另外,打开 services.msc,看 "Windows IME-related services" 状态'

Write-Host ''
Write-Host '=== 6. 注册表看 alt+, 的 hotkey 注册 ===' -ForegroundColor Cyan
$reg = Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' -ErrorAction SilentlyContinue
Write-Host "  (此路径用于 boot,无关 hotkey)"
$allUsers = Get-WmiObject -Class Win32_Process -Filter "Name='WeaselServer.exe'" -ErrorAction SilentlyContinue | Select-Object ProcessId,CommandLine
if ($allUsers) {
  $allUsers | Format-List
} else {
  Write-Host "  WeaselServer.exe not in WMI process list either"
}
