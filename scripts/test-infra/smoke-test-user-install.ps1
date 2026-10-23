#!/usr/bin/env pwsh
# v0.19.0.38 smoke test on USER actual install path (NSIS in-place upgrade verified)

$dst = 'D:\Program Files\fluxing'   # actual user install
$failures = @()

# b. WeaselServer.exe at fluxing\weasel (not weasel\)
if (-not (Test-Path "$dst\weasel\WeaselServer.exe")) { $failures += 'b: fluxing\weasel\WeaselServer.exe missing' }
if (Test-Path "D:\Program Files\weasel\WeaselServer.exe") { $failures += 'b: WRONG location D:\Program Files\weasel\WeaselServer.exe' }

# c. user1\fluxing co-located
if (-not (Test-Path "$dst\user1\fluxing")) { $failures += 'c: user1\fluxing missing' }

# d. HKLM InstallDir (post-install)
$installDir = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel' -ErrorAction SilentlyContinue).InstallDir
if (-not $installDir) { $failures += 'd: HKLM InstallDir MISSING (installer regression)' }
else { Write-Host "d. HKLM InstallDir = $installDir" }

# e. HKCU RimeUserDir
$userDir = (Get-ItemProperty 'HKCU:\Software\Fluxing\Weasel' -ErrorAction SilentlyContinue).RimeUserDir
if (-not $userDir) { $failures += 'e: HKCU RimeUserDir MISSING' }
else { Write-Host "e. HKCU RimeUserDir = $userDir" }

# f. rime.dll 2-5 MB
$rimeDll = "$dst\weasel\rime.dll"
if (-not (Test-Path $rimeDll)) { $failures += 'f: rime.dll missing' }
else { $sz = (Get-Item $rimeDll).Length; Write-Host "f. rime.dll size = $sz"; if ($sz -lt 2MB -or $sz -gt 5MB) { $failures += "f: rime.dll size=$sz not in 2-5MB" } }

# g. prebuilt dict
if (-not (Test-Path "$dst\weasel\data\build\rime_ice.table.bin")) { $failures += 'g: prebuilt rime_ice.table.bin missing' }

# h. L14 arch
function Test-Arch($p) {
  $b = [System.IO.File]::ReadAllBytes($p)
  $peOff = [BitConverter]::ToInt32($b[0x3C..0x3F], 0)
  return [BitConverter]::ToUInt16($b[($peOff+4)..($peOff+5)], 0)
}
$expected = @{ 'WeaselServer.exe' = 0x14C; 'WeaselDeployer.exe' = 0x14C; 'WeaselSetup.exe' = 0x14C; 'uninstall.exe' = 0x14C; 'weaselx64.dll' = 0x8664; 'rime.dll' = 0x14C }
foreach ($f in $expected.Keys) {
  $p = Join-Path "$dst\weasel" $f
  if (Test-Path $p) {
    $actual = Test-Arch $p
    if ($actual -ne $expected[$f]) { $failures += "h: $f arch=0x$($actual.ToString('X4')) (expected 0x$($expected[$f].ToString('X4')))" }
  }
}

Write-Host ''
Write-Host '=== Smoke test results ==='
if ($failures.Count -eq 0) { Write-Host 'ALL LAYOUT INVARIANTS PASS' }
else { Write-Host "FAILURES ($($failures.Count)):"; $failures | ForEach-Object { Write-Host "  - $_" } }

Write-Host ''
Write-Host '=== L66 reg query (mandatory per nsis-registry-writes.md) ==='
$l66Keys = @(
  'HKLM\SOFTWARE\Microsoft\CTF\KnownClasses',
  'HKCU\Software\Microsoft\CTF\Assemblies\0x00000804',
  'HKLM\SOFTWARE\Classes\CLSID\{A3F4CDED-B1E9-41EE-9CA6-7B4D0DE6CB0A}\InprocServer32'
)
foreach ($k in $l66Keys) {
  Write-Host "--- $k ---"
  reg query $k 2>&1
  Write-Host ''
}

Write-Host '=== WeaselServer.exe md5 parity (sandbox install vs source build) ==='
$ws = "$dst\weasel\WeaselServer.exe"
if (Test-Path $ws) {
  $md5 = (certutil -hashfile $ws MD5 | Select-Object -Skip 1) -join ''
  Write-Host "installed md5:   $md5"
  Write-Host "expected:       62bf75b119cc1d0a92dfbf68e5706dc6"
  if ($md5 -eq '62bf75b119cc1d0a92dfbf68e5706dc6') {
    Write-Host 'md5 parity: PASS (no L97 stale binary)'
  } else {
    Write-Host 'md5 parity: FAIL'
    $failures += 'md5 parity FAIL'
  }
}
