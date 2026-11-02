<#
  Fluxing silent-install ACCEPTANCE CHECK  (READ ONLY - changes nothing)

  Usage:
    powershell -NoProfile -ExecutionPolicy Bypass -File acceptance-check.ps1
    powershell -NoProfile -ExecutionPolicy Bypass -File acceptance-check.ps1 -InstallDir "D:\Program Files\fluxing\weasel"

  NOTE ON THE UNIT (fixed 2026-11-03, L169):
    -InstallDir is the ENGINE dir - the folder that directly holds
    WeaselServer.exe / weasel.dll / weaselx64.dll / data\.  It is NOT the
    install root: output/install.nsi:24-25 defines WEASEL_ROOT = $INSTDIR\weasel,
    so the root is one level up (D:\Program Files\fluxing).
    The old tmp\silent-install-116.cmd fed the SAME path to /D= (root) and to
    -InstallDir (engine) - that unit mismatch is what nested the install.

  Criteria (from handoff-2026-11-03, "XIU-QIU-1" / requirement(1)):
    C1  data\default.yaml = 18217 bytes AND sha256 starts with 917509f6f2460230
    C2  no *.stage2 marker left behind
    C3  versioned shim present; TIP / InprocServer32 path that points at weaselx64
        -> reported as VERSIONED shim or BARE name (bare = the file was not locked)
    C4  auto-start Run entry (HKLM 64-bit view / HKLM WOW6432Node view / HKCU)

  Exit code: 0 = all hard checks passed, 1 = at least one hard check failed.
#>
param(
  [string]$InstallDir = 'D:\Program Files\fluxing\weasel'
)

$ErrorActionPreference = 'Continue'
$fail = 0

function Section($t) { Write-Host "  --- $t ---" }
function Pass($t) { Write-Host ("      {0} : PASS" -f $t) }
function Fail($t) { Write-Host ("      {0} : FAIL" -f $t); $script:fail = 1 }
function Note($t) { Write-Host ("      {0}" -f $t) }

Write-Host "==== acceptance check for: $InstallDir ===="

if (-not (Test-Path -LiteralPath $InstallDir)) {
  Fail "install dir not found: $InstallDir"
  Write-Host ""
  Write-Host "==== RESULT: SOME HARD CHECKS FAILED ===="
  exit 1
}

# ---- C1: data\default.yaml ------------------------------------------------
Section 'C1: data\default.yaml must be 18217 bytes / sha256 917509f6f2460230'
$dy = Join-Path $InstallDir 'data\default.yaml'
if (Test-Path -LiteralPath $dy) {
  $fi = Get-Item -LiteralPath $dy
  Note ("size   = {0}" -f $fi.Length)
  if ($fi.Length -eq 18217) { Pass 'size' } else { Fail ("size (expected 18217, got {0})" -f $fi.Length) }
  $sha = (Get-FileHash -LiteralPath $dy -Algorithm SHA256).Hash
  Note ("sha256 = {0}" -f $sha)
  if ($sha.ToLower().StartsWith('917509f6f2460230')) { Pass 'sha256' } else { Fail 'sha256 prefix' }
} else {
  Fail ("MISSING: {0}" -f $dy)
}

# ---- C2: no *.stage2 marker ----------------------------------------------
Section ("C2: no *.stage2 marker left in {0}" -f $InstallDir)
$stage2 = @(Get-ChildItem -LiteralPath $InstallDir -Filter '*.stage2' -File -ErrorAction SilentlyContinue)
if ($stage2.Count -eq 0) { Pass 'no stage2 marker' }
else { $stage2 | ForEach-Object { Note ("stage2 marker: {0}" -f $_.Name) }; Fail 'stage2 marker present' }

# ---- C3: versioned shim + TIP registration --------------------------------
Section 'C3: versioned shim present'
$shims = @(Get-ChildItem -LiteralPath $InstallDir -Filter 'weasel*.dll' -File -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match '^weasel(x64)?\.\d+\.\d+\.\d+\.dll$' })
if ($shims.Count -eq 0) { Note 'versioned shim : NONE FOUND' }
else { $shims | ForEach-Object { Note $_.Name }; Note ("versioned shim count = {0}" -f $shims.Count) }

Section 'C3b: TIP / InprocServer32 path that points at weaselx64'
#  reg's recursive /f search prints "End of search: N match(es) found." even when N=0,
#  so only lines that actually contain the value are kept.
$tipPaths = @()
foreach ($root in @('HKLM\SOFTWARE\Classes\CLSID', 'HKLM\SOFTWARE\WOW6432Node\Classes\CLSID')) {
  $out = & reg.exe query $root /s /f 'weaselx64' /d 2>$null
  foreach ($line in $out) {
    if ($line -match 'REG_SZ\s+(?<p>.+)$') {
      $p = $Matches['p'].Trim()
      if ($p -like '*weaselx64*') { $tipPaths += $p }
    }
  }
}
if ($tipPaths.Count -eq 0) {
  Note 'TIP path pointing to weaselx64 : NOT FOUND'
} else {
  foreach ($p in ($tipPaths | Select-Object -Unique)) {
    Note ("TIP path : {0}" -f $p)
    $leaf = Split-Path -Leaf $p
    if ($leaf -match '^weaselx64\.\d+\.\d+\.\d+\.dll$') { Note '   -> points at a VERSIONED shim' }
    elseif ($leaf -eq 'weaselx64.dll')             { Note '   -> points at the BARE name (file was not locked, no shim needed)' }
    else                                           { Note '   -> unexpected file name' }
  }
}

# ---- C4: auto-start Run entry ---------------------------------------------
Section 'C4: auto-start Run entry'
$runHits = @()
foreach ($k in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run',
                 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Run',
                 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run')) {
  if (Test-Path $k) {
    $v = (Get-ItemProperty -Path $k -Name 'WeaselServer' -ErrorAction SilentlyContinue).WeaselServer
    if ($v) { Note ("{0} WeaselServer = {1}" -f $k, $v); $runHits += $k }
  }
}
if ($runHits.Count -eq 0) { Note 'no auto-start Run entry : NOT FOUND' } else { Note ("auto-start Run entries found = {0}" -f $runHits.Count) }

Write-Host ""
if ($fail -eq 0) { Write-Host "==== RESULT: ALL HARD CHECKS PASSED ====" } else { Write-Host "==== RESULT: SOME HARD CHECKS FAILED ====" }
exit $fail
