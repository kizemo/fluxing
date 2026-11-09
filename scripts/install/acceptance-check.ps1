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
    C3  versioned shim present (informational; absent = nothing was locked)
        shim name form: weasel[x64].<ver>.<build>[.<seq>].dll
        => 3 to 5 numeric segments.  The old 3-segment regex never matched the
        live names (weaselx64.1.2.1.2.2.dll) and printed "NONE FOUND" forever.
    C3b TIP / InprocServer32 path that points at weaselx64 - HARD CHECK:
        VERSIONED shim (file must exist) or BARE name both pass; a
        registration pointing at an unexpected or missing file fails.
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
$dy = Join-Path $InstallDir 'data\default.yaml'
$src = 'F:\soft\00selfmade\rime_claude\output\data\default.yaml'
Section ("C1: data\default.yaml must match repo source: {0}" -f $src)
if (Test-Path -LiteralPath $dy) {
  $fi = Get-Item -LiteralPath $dy
  Note ("size   = {0}" -f $fi.Length)
  $sha = (Get-FileHash -LiteralPath $dy -Algorithm SHA256).Hash
  Note ("sha256 = {0}" -f $sha)
  if (-not (Test-Path -LiteralPath $src)) {
    Fail ("missing repo source for comparison: {0}" -f $src)
  } else {
    $si = Get-Item -LiteralPath $src
    $srcSha = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash
    if ($fi.Length -eq $si.Length) { Pass ("size (source {0})" -f $si.Length) } else { Fail ("size (source {0}, installed {1})" -f $si.Length, $fi.Length) }
    if ($sha -eq $srcSha) { Pass 'sha256 == source' } else { Fail 'sha256 differs from repo source' }
  }
} else {
  Fail ("MISSING: {0}" -f $dy)
}

# ---- C2: no *.stage2 marker ----------------------------------------------
Section ("C2: no *.stage2 marker left in {0}" -f $InstallDir)
$stage2 = @(Get-ChildItem -LiteralPath $InstallDir -Filter '*.stage2' -File -ErrorAction SilentlyContinue)
if ($stage2.Count -eq 0) { Pass 'no stage2 marker' }
else { $stage2 | ForEach-Object { Note ("stage2 marker: {0}" -f $_.Name) }; Fail 'stage2 marker present' }

# ---- C3: versioned shim + TIP registration --------------------------------
#  Shim naming comes from output/install.nsi:739 / 823
#  'weasel[x64].<FLUXING_VERSION>.<WEASEL_BUILD>.<seq>.dll'.  FLUXING_VERSION is
#  itself N.N.N, so the live name carries 4-5 numeric segments
#  (weaselx64.1.2.1.2.2.dll).  Keep the range, do not pin one exact arity.
$ShimRx     = '^weasel(x64)?(\.\d+){3,5}\.dll$'
$X64ShimRx  = '^weaselx64(\.\d+){3,5}\.dll$'
Section 'C3: versioned shim present'
$shims = @(Get-ChildItem -LiteralPath $InstallDir -Filter 'weasel*.dll' -File -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match $ShimRx })
if ($shims.Count -eq 0) { Note 'versioned shim : NONE FOUND (nothing was locked; bare name is in use)' }
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
  Fail 'TIP path pointing to weaselx64 : NOT FOUND (TSF registration missing)'
} else {
  foreach ($p in ($tipPaths | Select-Object -Unique)) {
    Note ("TIP path : {0}" -f $p)
    $leaf = Split-Path -Leaf $p
    if ($leaf -match $X64ShimRx) {
      if (Test-Path -LiteralPath $p) { Pass 'points at a VERSIONED shim (file exists)' }
      else { Fail 'points at a VERSIONED shim but the file is MISSING' }
    }
    elseif ($leaf -eq 'weaselx64.dll') {
      if (Test-Path -LiteralPath $p) { Pass 'points at the BARE name (file was not locked, no shim needed)' }
      else { Fail 'points at the BARE name but the file is MISSING' }
    }
    else { Fail ("unexpected registered file name: {0}" -f $leaf) }
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
