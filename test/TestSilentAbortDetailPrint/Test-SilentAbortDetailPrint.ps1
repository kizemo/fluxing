# Test-SilentAbortDetailPrint.ps1
# Phase L Bugfix SilentAbortLog / v0.74.1 verification:
# Verifies the silent-install Abort path logs both via DetailPrint (for
# NSIS terminal / verify.log) AND via FileWrite to a fallback log
# ($INSTDIR\fluxing-install-silent.log). This addresses R-004 in
# spec 071: NSIS DetailPrint may not flush reliably before Quit on some
# terminals. The synchronous FileWrite ensures the diagnostic message
# reaches disk regardless of terminal flushing behavior.
#
# Anchors checked:
#   1. install.nsi has IfSilent 0 +N (jump over the silent path) — must
#      not be `+4` (the v0.74 value); expected `+7` to accommodate the
#      3 new lines (DetailPrint + FileOpen + FileWrite + FileClose).
#   2. install.nsi has the exact DetailPrint message containing
#      "deferred shim swap pending; silent exit 6001"
#   3. install.nsi has FileOpen $9 "$INSTDIR\fluxing-install-silent.log" a
#   4. install.nsi has FileWrite $9 with fallback log content
#   5. install.nsi has FileClose $9
#
# Why this matters (forensics 2026-07-30):
#   v0.74 silent installer with staged binaries exits 6001 but the
#   pinyin-release CI / sandbox-verify logs show only the exit code,
#   not the cause. Downstream automation can't distinguish "deferred
#   install (reboot required)" from "actual install failure". A grep-able
#   "deferred shim swap" message gives the CI a single-line signal.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File test\TestSilentAbortDetailPrint\Test-SilentAbortDetailPrint.ps1

[CmdletBinding()] param()

$ErrorActionPreference = 'Stop'
# Use List[string] for in-place accumulation (PS 5.1 + += scope trap avoided)
$FailsList = New-Object 'System.Collections.Generic.List[string]'

$root = $PSScriptRoot | Split-Path -Parent | Split-Path -Parent
$installNsi = Join-Path $root "output\install.nsi"

Write-Host "=== Silent Abort DetailPrint verification (Phase L Bugfix SilentAbortLog / v0.74.1) ===" -ForegroundColor Cyan
Write-Host "install.nsi: $installNsi"
Write-Host ""

if (-not (Test-Path $installNsi)) {
    Write-Host "FAIL: install.nsi not found at $installNsi" -ForegroundColor Red
    exit 1
}

# === Read install.nsi ===
$nsibytes = [System.IO.File]::ReadAllBytes($installNsi)
$nsiText = [System.Text.Encoding]::UTF8.GetString($nsibytes)

# === Check 1: IfSilent jump target updated from +4 to +7 ===
# v0.74 used `IfSilent 0 +4` (4 lines: StrCmp / SetErrorLevel / Quit / [If] end).
# v0.74.1 inserts 3 new lines (DetailPrint + FileOpen + FileWrite + FileClose)
# BEFORE the existing 3, so the jump count should now be +7.
$silentJumpOK = $false
if ($nsiText.Contains('IfSilent 0 +7')) {
    Write-Host "  [PASS] IfSilent 0 +7 (3 new lines inserted before SetErrorLevel/Quit)" -ForegroundColor Green
    $silentJumpOK = $true
} elseif ($nsiText.Contains('IfSilent 0 +4')) {
    Write-Host "  [FAIL] IfSilent 0 +4 still present (v0.74 legacy; +7 expected for +3 inserted lines)" -ForegroundColor Red
    [void]$FailsList.Add("IfSilent jump target stale at +4, expected +7")
} else {
    # Could be `IfSilent 0 +<other>`. Look up any IfSilent 0 +N
    $matches = [regex]::Matches($nsiText, 'IfSilent 0 \+(\d+)')
    $maxN = 0
    foreach ($m in $matches) {
        $n = [int]$m.Groups[1].Value
        if ($n -gt $maxN) { $maxN = $n }
    }
    if ($maxN -ge 7) {
        Write-Host "  [PASS] IfSilent 0 +$maxN (>= +7, accommodates the 3 new lines)" -ForegroundColor Green
        $silentJumpOK = $true
    } else {
        Write-Host "  [FAIL] IfSilent 0 +$maxN (< +7, expected >= +7 to skip the 3 inserted lines)" -ForegroundColor Red
        [void]$FailsList.Add("IfSilent jump target +$maxN doesn't skip the new detail-print block")
    }
}

# === Check 2: DetailPrint exact message present ===
$detailPrintMsg = 'deferred shim swap pending; silent exit 6001'
if ($nsiText.Contains($detailPrintMsg)) {
    Write-Host "  [PASS] DetailPrint carries '$detailPrintMsg' (grep-able by sandbox-verify)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] DetailPrint message '$detailPrintMsg' NOT found" -ForegroundColor Red
    [void]$FailsList.Add("DetailPrint message missing")
}

# === Check 3.5 (suggestion 2 hook): Fluxing log prefix on FileWrite ===
# v0.74.1 dropped the [$DATE-$TIME] timestamp attempt (NSIS LangString lookup
# warning; $DATE/$TIME not NSIS built-ins). Replaced with a static Fluxing
# log prefix for cross-install log disambiguation. Sandbox-verify still
# grep-able by anchor text.
if ($nsiText.Contains('[Fluxing v0.74.1 silent install log]')) {
    Write-Host "  [PASS] FileWrite Fluxing log prefix present (cross-install log disambiguation)" -ForegroundColor Green
} else {
    Write-Host "  [WARN] FileWrite lacks Fluxing log prefix (cross-install log lines may collide)" -ForegroundColor Yellow
}

# === Check 3-5: FileOpen / FileWrite / FileClose around fluxing-install-silent.log ===
$syncLogLines = @(
    'FileOpen $9 "$INSTDIR\fluxing-install-silent.log" a',
    'FileWrite $9 "[Fluxing v0.74.1 silent install log] deferred shim swap pending; reboot required after this silent install',
    'FileClose $9'
)
foreach ($anchor in $syncLogLines) {
    if ($nsiText.Contains($anchor)) {
        Write-Host "  [PASS] anchor found: $anchor" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] anchor MISSING: $anchor" -ForegroundColor Red
        [void]$FailsList.Add("silent log fallback anchor missing: $anchor")
    }
}

# === Sanity: SetErrorLevel 6001 + Quit still present (silent abort intact) ===
if ($nsiText.Contains('SetErrorLevel 6001') -and $nsiText.Contains('Quit')) {
    Write-Host "  [PASS] SetErrorLevel 6001 + Quit still present (silent abort contract preserved)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] SetErrorLevel 6001 or Quit missing — silent abort contract broken" -ForegroundColor Red
    [void]$FailsList.Add("silent abort contract broken (missing SetErrorLevel 6001 or Quit)")
}

# === Summary ===
Write-Host ""
if ($FailsList.Count -eq 0) {
    Write-Host "=== PASS - Silent Abort DetailPrint verification ===" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== FAIL - $($FailsList.Count) issue(s) ===" -ForegroundColor Red
    foreach ($f in $FailsList) {
        Write-Host "  - $f" -ForegroundColor Red
    }
    exit 1
}
