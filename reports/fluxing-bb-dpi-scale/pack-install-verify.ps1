$ErrorActionPreference = 'Continue'
$repo = 'F:\soft\00selfmade\rime_claude'
$log = Join-Path $repo 'reports\fluxing-bb-dpi-scale\pack-install-verify.log'
$out = New-Object System.Collections.Generic.List[string]
function L([string]$s) { $out.Add($s) }

function Sha16([string]$p) {
  $h = (Get-FileHash -Path $p -Algorithm SHA256).Hash
  return $h.Substring(0, 16)
}
function HasWide([string]$p, [string]$needle) {
  if (-not (Test-Path $p)) { return 'FILE_MISSING' }
  $s = [System.Text.Encoding]::Unicode.GetString([System.IO.File]::ReadAllBytes($p))
  return [string]$s.Contains($needle)
}
function FileVer([string]$p) {
  if (-not (Test-Path $p)) { return 'FILE_MISSING' }
  return (Get-Item $p).VersionInfo.FileVersion
}

L "=== PACK/INSTALL/VERIFY START $(Get-Date -Format o) ==="

# ---------- 1. authoritative build (build.bat is the ONLY release path) ----------
L '--- step1: build.bat weasel installer ---'
$buildOut = & cmd /c "cd /d `"$repo`" && build.bat weasel installer 2>&1"
$buildExit = $LASTEXITCODE
L "BUILD_EXIT=$buildExit"
$errs = @(); $warns = @()
foreach ($x in $buildOut) {
  $s = [string]$x
  if ($s -match '(?i)error C\d|error LNK\d|error MSB\d|: error|fatal error') { $errs += $s }
  elseif ($s -match '(?i)warning C\d|warning LNK\d|warning 7010|: warning') { $warns += $s }
}
L "BUILD_ERROR_LINES=$($errs.Count) BUILD_WARNING_LINES=$($warns.Count)"
$errs | Select-Object -First 30 | ForEach-Object { L "  BUILD-ERR: $_" }
$warns | Select-Object -First 30 | ForEach-Object { L "  BUILD-WARN: $_" }
$buildOut | Select-Object -Last 15 | ForEach-Object { L "  build-tail: $_" }

# ---------- 2. artifact verification ----------
L '--- step2: artifact ---'
$inst = Join-Path $repo 'output\archives\fluxing-1.3.7-installer.exe'
$built = Join-Path $repo 'output\Win32\WeaselServer.exe'
$installed136 = 'D:\Program Files\fluxing\weasel\WeaselServer.exe'
if (Test-Path $inst) {
  L "INSTALLER=$inst BYTES=$((Get-Item $inst).Length) SHA16=$(Sha16 $inst)"
} else { L "INSTALLER_MISSING=$inst" }
if (Test-Path $built) {
  L "BUILT_SERVER=$built BYTES=$((Get-Item $built).Length) FILEVER=$(FileVer $built) SHA16=$(Sha16 $built)"
} else { L "BUILT_SERVER_MISSING=$built" }
# token differential: this batch's diagnostic strings are wide (UTF-16LE) literals
foreach ($tok in @('dpi-mismatch system=', 'note-margin-short dpi=')) {
  $inBuilt = HasWide $built $tok
  $inOld = HasWide $installed136 $tok
  L "TOKEN[$tok] built137=$inBuilt installed136=$inOld"
}
if (Test-Path $installed136) {
  L "INSTALLED_136_FILEVER=$(FileVer $installed136) SHA16=$(Sha16 $installed136) BYTES=$((Get-Item $installed136).Length)"
}

# ---------- 3. silent install (overwrite in place) ----------
L '--- step3: silent install ---'
L "RUNNING_BEFORE=$((Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'" | ForEach-Object { "$($_.ProcessId)@$($_.CreationDate)" }) -join ',')"
if (Test-Path $inst) {
  $iOut = & cmd /c "`"$inst`" /S /D=D:\Program Files\fluxing 2>&1"
  $iExit = $LASTEXITCODE
  L "INSTALL_EXIT=$iExit"
  $iOut | Select-Object -First 20 | ForEach-Object { L "  install-out: $_" }
} else { L 'INSTALL_SKIPPED_NO_ARTIFACT' }

Start-Sleep -Seconds 6
L '--- step4: post-install state ---'
Get-CimInstance Win32_Process -Filter "Name='WeaselServer.exe'" | ForEach-Object {
  L "PROC pid=$($_.ProcessId) start=$($_.CreationDate) path=$($_.ExecutablePath)"
}
if (Test-Path $installed136) {
  L "INSTALLED_AFTER_FILEVER=$(FileVer $installed136) BYTES=$((Get-Item $installed136).Length) SHA16=$(Sha16 $installed136)"
}
$after = 'D:\Program Files\fluxing\weasel\WeaselServer.exe'
if (Test-Path $after) {
  foreach ($tok in @('dpi-mismatch system=', 'note-margin-short dpi=')) {
    L "TOKEN_AFTER[$tok]=$(HasWide $after $tok)"
  }
}
L "STAGE2_MARKERS=$((Get-ChildItem 'D:\Program Files\fluxing' -Recurse -Filter '*.stage2' -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName }) -join ',')"

# ---------- 5. re-run BA probe ----------
L '--- step5: BA probe re-run ---'
$probe = Join-Path $repo 'reports\fluxing-ba-a1-note-wrap\probe-a1-note-wrap.ps1'
if (Test-Path $probe) {
  $pOut = & pwsh -NoProfile -ExecutionPolicy Bypass -File $probe 2>&1
  L "PROBE_EXIT=$LASTEXITCODE"
  $pOut | Select-Object -Last 45 | ForEach-Object { L "  probe: $_" }
} else { L 'PROBE_MISSING' }

L "=== PACK/INSTALL/VERIFY END $(Get-Date -Format o) ==="
[System.IO.File]::WriteAllText($log, (($out -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
'--- SUMMARY ---'
$out | Where-Object { $_ -match 'BUILD_EXIT|BUILD_ERROR_LINES|BUILD_WARNING_LINES|BUILD-ERR|INSTALLER=|BUILT_SERVER=|TOKEN|INSTALLED_136|INSTALL_EXIT|PROC pid=|STAGE2|PROBE_EXIT|VERDICT|DEFCIT|HEADROOM|judge|A1_' }
