$dst = 'D:\TEMP\fluxing-test'
$failures = @()

if (-not (Test-Path "$dst\ProgramFiles\fluxing\weasel\WeaselServer.exe")) { $failures += 'b: fluxing\weasel\WeaselServer.exe missing' }
if (Test-Path "$dst\ProgramFiles\weasel\WeaselServer.exe") { $failures += 'b: weasel\WeaselServer.exe at wrong location' }
if (-not (Test-Path "$dst\ProgramFiles\fluxing\user1\fluxing")) { $failures += 'c: user1\fluxing missing' }

$installDir = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel' -ErrorAction SilentlyContinue).InstallDir
if ($installDir -ne "$dst\ProgramFiles\fluxing") { $failures += "d: InstallDir='$installDir' (expected '$dst\ProgramFiles\fluxing')" }

$userDir = (Get-ItemProperty 'HKCU:\Software\Fluxing\Weasel' -ErrorAction SilentlyContinue).RimeUserDir
if ($userDir -ne "$dst\ProgramFiles\fluxing\user1\fluxing") { $failures += "e: RimeUserDir='$userDir' (expected '$dst\ProgramFiles\fluxing\user1\fluxing')" }

$rimeDll = "$dst\ProgramFiles\fluxing\weasel\rime.dll"
if (-not (Test-Path $rimeDll)) { $failures += 'f: rime.dll missing' }
else { $sz = (Get-Item $rimeDll).Length; if ($sz -lt 2MB -or $sz -gt 5MB) { $failures += "f: rime.dll size=$sz not in 2-5MB" } }

if (-not (Test-Path "$dst\ProgramFiles\fluxing\weasel\data\build\rime_ice.table.bin")) { $failures += 'g: prebuilt rime_ice.table.bin missing' }

function Test-Arch($p) {
  $b = [System.IO.File]::ReadAllBytes($p)
  $peOff = [BitConverter]::ToInt32($b[0x3C..0x3F], 0)
  return [BitConverter]::ToUInt16($b[($peOff+4)..($peOff+5)], 0)
}
$expected = @{ 'WeaselServer.exe' = 0x14C; 'WeaselDeployer.exe' = 0x14C; 'WeaselSetup.exe' = 0x14C; 'uninstall.exe' = 0x14C; 'weaselx64.dll' = 0x8664; 'rime.dll' = 0x14C }
foreach ($f in $expected.Keys) {
  $p = Join-Path "$dst\ProgramFiles\fluxing\weasel" $f
  if (Test-Path $p) {
    $actual = Test-Arch $p
    if ($actual -ne $expected[$f]) { $failures += "h: $f arch=0x$($actual.ToString('X4')) (expected 0x$($expected[$f].ToString('X4')))" }
  }
}

$l66Keys = @(
  'HKLM\SOFTWARE\Microsoft\CTF\KnownClasses',
  'HKCU\Software\Microsoft\CTF\Assemblies\0x00000804',
  'HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32'
)

Write-Host '=== Smoke test results ==='
if ($failures.Count -eq 0) { Write-Host 'ALL LAYOUT INVARIANTS PASS' }
else { Write-Host "FAILURES ($($failures.Count)):"; $failures | ForEach-Object { Write-Host "  - $_" } }
Write-Host ''
Write-Host '=== L66 reg query results ==='
foreach ($k in $l66Keys) {
  Write-Host "--- $k ---"
  reg query $k 2>&1
  Write-Host ''
}

Write-Host '=== Hash check ==='
$ws = "$dst\ProgramFiles\fluxing\weasel\WeaselServer.exe"
if (Test-Path $ws) {
  Write-Host "installed WeaselServer.exe md5:"
  certutil -hashfile $ws MD5 | Select-Object -Skip 1
  Write-Host "expected: 62bf75b119cc1d0a92dfbf68e5706dc6 (commit 79d522b source build)"
}
