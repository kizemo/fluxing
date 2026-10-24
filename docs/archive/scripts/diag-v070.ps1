# Diag script for v0.70 freeze — gather runtime + registry state
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$out = [System.Text.StringBuilder]::new()

function W($s) { [void]$out.AppendLine($s) }

W "=== 1. WeaselServer/TSF/Deployer processes ==="
Get-Process | Where-Object { $_.Name -match 'weasel|fluxing' } |
  Select-Object Id, Name, CPU, WS, StartTime |
  Format-Table -AutoSize | Out-String -Stream | ForEach-Object { W $_ }

W "=== 2. Named pipes (weasel*) ==="
try {
  $pipes = [System.IO.Directory]::GetFiles('\\.\pipe\') | Where-Object { $_ -match 'weasel' }
  if ($pipes.Count -eq 0) { W "  (no weasel* pipes)" }
  else { $pipes | ForEach-Object { W "  $_" } }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 3. HKCU\Keyboard Layout\Preload (default IME order) ==="
try {
  reg query 'HKCU\Keyboard Layout\Preload' 2>&1 | ForEach-Object { W "  $_" }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 4. HKCU\Software\Fluxing\Weasel ==="
try {
  reg query 'HKCU\Software\Fluxing' /s 2>&1 | ForEach-Object { W "  $_" }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 5. HKLM\SOFTWARE\Classes\CLSID\Fluxing text service ==="
try {
  reg query 'HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}' /s 2>&1 | ForEach-Object { W "  $_" }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 6. HKLM\CTF\KnownClasses + HKCU\CTF\Assemblies\0x00000804 ==="
try {
  reg query 'HKLM\SOFTWARE\Microsoft\CTF\KnownClasses' 2>&1 | ForEach-Object { W "  $_" }
  W "  ---"
  reg query 'HKCU\Software\Microsoft\CTF\Assemblies\0x00000804' /s 2>&1 | ForEach-Object { W "  $_" }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 7. Event log (Application, last 30 min) — weasel/tsf/fluxing errors ==="
try {
  Get-WinEvent -FilterHashtable @{ LogName='Application'; StartTime=(Get-Date).AddMinutes(-30) } -ErrorAction SilentlyContinue |
    Where-Object { $_.Message -match 'weasel|fluxing|ime|text service|0x00000804' -or $_.ProviderName -match 'weasel|tsf|ime' } |
    Select-Object -First 15 TimeCreated, ProviderName, Id, LevelDisplayName, Message |
    Format-List | Out-String -Stream | ForEach-Object { W $_ }
} catch { W "  err: $($_.Exception.Message)" }

W "=== 8. Installed binary md5 vs ship md5 ==="
$binDir = 'D:\Program Files\fluxing\weasel'
$expected = @{
  'WeaselServer.exe'   = 'BAD4E61A3624B4DFD516BA561B23BED6'
  'WeaselDeployer.exe' = 'ED0EAEAA5F099F68374F53A535FE9BAD'
  'weasel.dll'         = '82e9e924b25634ca7ffe869856dc8bb7'
  'weaselx64.dll'      = '80615749557d4e04c6e7451ad00e37c5'
}
foreach ($name in $expected.Keys) {
  $path = Join-Path $binDir $name
  if (Test-Path $path) {
    $h = (Get-FileHash $path -Algorithm MD5).Hash.ToLower()
    $exp = $expected[$name].ToLower()
    $tag = if ($h -eq $exp) { 'MATCH' } else { 'STALE' }
    W "  $name  actual=$h  expected=$exp  $tag"
  } else {
    W "  $name  MISSING"
  }
}

W "=== 9. weasel-install.log tail (last 30 lines) ==="
try {
  $log = Join-Path $env:APPDATA 'Rime\weasel-install.log'
  if (Test-Path $log) {
    Get-Content $log -Tail 30 -Encoding Unicode | ForEach-Object { W "  $_" }
  } else { W "  log not found" }
} catch { W "  err: $($_.Exception.Message)" }

$out.ToString()
