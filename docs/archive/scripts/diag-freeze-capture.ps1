# Diag — capture WeaselServer/TSF state at freeze time
# Run in PowerShell when system is hung (mouse loading, no input)
# Will not fix anything; just dump process + log state to a file
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$out = [System.Text.StringBuilder]::new()
$ts = Get-Date -Format "yyyyMMdd-HHmmss"
$dumpDir = "C:\Users\Duanyi\AppData\Local\Temp\fluxing-freeze-capture-$ts"
New-Item -ItemType Directory -Path $dumpDir -Force | Out-Null

function W($s) { [void]$out.AppendLine($s); Write-Host $s }

W "=== fluxing-freeze-capture ==="
W "Captured at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
W "Output dir: $dumpDir"
W "Machine: $env:COMPUTERNAME User: $env:USERNAME"
W ""

W "=== 1. Weasel/Fluxing process state (CPU, threads, handle count) ==="
Get-Process | Where-Object { $_.Name -match 'weasel|fluxing' } | ForEach-Object {
  $cpu = if ($_.CPU) { [math]::Round($_.CPU, 2) } else { 0 }
  W ("  PID={0,-6} Name={1,-20} CPU={2,8}s Threads={3,4} Handles={4,5} WS={5,8}KB StartTime={6}" -f `
     $_.Id, $_.Name, $cpu, $_.Threads.Count, $_.HandleCount, [int]($_.WS/1KB), $_.StartTime)
  W "    MainWindowTitle: $($_.MainWindowTitle)"
  W "    Responding: $($_.Responding)"
  W "    Path: $($_.Path)"
}
W ""

W "=== 2. Named pipes (weasel*) ==="
try {
  $pipes = [System.IO.Directory]::GetFiles('\\.\pipe\') | Where-Object { $_ -match 'weasel' }
  if ($pipes.Count -eq 0) { W "  (no weasel* pipes — WeaselServer probably dead)" }
  else { $pipes | ForEach-Object { W "  $_" } }
} catch { W "  err: $($_.Exception.Message)" }
W ""

W "=== 3. WMI process details (parent, commandline) ==="
Get-WmiObject -Class Win32_Process | Where-Object { $_.Name -match 'weasel|fluxing' } | ForEach-Object {
  W "  PID=$($_.ProcessId) ParentPID=$($_.ParentProcessId) Name=$($_.Name)"
  W "    CreationDate: $($_.CreationDate)"
  W "    CommandLine: $($_.CommandLine)"
  W "    WorkingSetSize: $($_.WorkingSetSize)"
}
W ""

W "=== 4. weasel-install.log (last 80 lines) ==="
$weaselLog = Join-Path $env:APPDATA 'Rime\weasel-install.log'
if (Test-Path $weaselLog) {
  W "  Path: $weaselLog"
  W "  LastWriteTime: $((Get-Item $weaselLog).LastWriteTime)"
  Get-Content $weaselLog -Tail 80 -Encoding Unicode | ForEach-Object { W "    $_" }
  Copy-Item $weaselLog "$dumpDir\weasel-install.log" -Force
} else { W "  (not found)" }
W ""

W "=== 5. rime.weasel log dir (last 5 files, tail of newest) ==="
$rimeLogDir = Join-Path $env:TEMP 'rime.weasel'
if (Test-Path $rimeLogDir) {
  $files = Get-ChildItem $rimeLogDir -Filter '*.INFO.*.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 3
  foreach ($f in $files) {
    W "  ----- $($f.Name) (LastWriteTime: $($f.LastWriteTime), Size: $($f.Length)) -----"
    Get-Content $f.FullName -Tail 30 -Encoding Unicode | ForEach-Object { W "    $_" }
    Copy-Item $f.FullName "$dumpDir\$($f.Name)" -Force
  }
} else { W "  (not found)" }
W ""

W "=== 6. Windows Application Event log (weasel/tsf/ime errors, last 30 min) ==="
try {
  Get-WinEvent -FilterHashtable @{ LogName='Application'; StartTime=(Get-Date).AddMinutes(-30) } -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'weasel|fluxing|ime|text service|0x00000804' -or $_.ProviderName -match 'weasel|tsf|ime' } |
    Select-Object -First 15 TimeCreated, ProviderName, Id, LevelDisplayName, Message |
    ForEach-Object { W "  [$($_.TimeCreated)] $($_.ProviderName) Id=$($_.Id) $($_.LevelDisplayName): $($_.Message)" }
} catch { W "  err: $($_.Exception.Message)" }
W ""

W "=== 7. Default IME registry + Fluxing registry ==="
try {
  W "  HKCU\Keyboard Layout\Preload:"
  reg query 'HKCU\Keyboard Layout\Preload' 2>&1 | ForEach-Object { W "    $_" }
  W "  HKCU\Software\Fluxing:"
  reg query 'HKCU\Software\Fluxing' 2>&1 | ForEach-Object { W "    $_" }
} catch { W "  err: $($_.Exception.Message)" }
W ""

W "=== 8. Installed binary md5 (verify no stale binary) ==="
$binDir = 'D:\Program Files\fluxing\weasel'
$expected = @{
  'WeaselServer.exe'   = 'BAD4E61A3624B4DFD516BA561B23BED6'
  'weaselx64.dll'      = '80615749557d4e04c6e7451ad00e37c5'
}
foreach ($name in $expected.Keys) {
  $path = Join-Path $binDir $name
  if (Test-Path $path) {
    $h = (Get-FileHash $path -Algorithm MD5).Hash.ToLower()
    $exp = $expected[$name].ToLower()
    $tag = if ($h -eq $exp) { 'MATCH' } else { 'STALE' }
    W "  $name  actual=$h  expected=$exp  $tag"
  } else { W "  $name  MISSING" }
}
W ""

W "=== 9. Stack hint (use procdump / wmic for full stacks) ==="
W "  procdump not bundled; for full stacks, run as admin:"
W "    procdump -ma <PID> $dumpDir\weasel-server-$ts.dmp"
W "  Or: wmic process where name='WeaselServer.exe' get ProcessId,ThreadCount,KernelModeTime,UserModeTime"
W ""

$outFile = "$dumpDir\freeze-capture-$ts.txt"
$out.ToString() | Out-File -FilePath $outFile -Encoding UTF8
W "=== DONE. Full output: $outFile ==="
