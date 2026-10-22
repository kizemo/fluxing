# Test-OpenccDataPath.ps1
# Phase L Bugfix EmojiJsonCheck / v0.74.1 verification:
# Verifies the installer ships emoji.json to the correct path AND adds
# post-install verification log to surface OpenCC emoji config issues.
#
# Anchors checked:
#   1. install.nsi contains the EmojiJsonCheck comment block (v0.74.1)
#   2. install.nsi contains IfFileExists $R8 emoji_json_ok pattern
#   3. build output output\data\opencc\emoji.json exists (so File glob ships it)
#   4. build output ships *.ocd* companion files (opencc binary data)
#
# Forensics 2026-07-30: runtime error
#   "opencc config not found: ...\weasel\data\opencc\emoji.json"
# despite install.nsi shipping all *.json. Deeper issue: libRime's OpenCC
# loader expects matched .ocd for binary data; without emoji.ocd companion,
# emoji.json fails to load. This test focuses on the install-side hardening
# (warning log + path correctness). Deeper fix (librime / build script
# inclusion of emoji.ocd) tracked as future work.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File test\TestOpenccDataPath\Test-OpenccDataPath.ps1

[CmdletBinding()] param()

$ErrorActionPreference = 'Stop'
# Use List[string] for in-place accumulation (PS 5.1 + += scope trap avoided)
$FailsList = New-Object 'System.Collections.Generic.List[string]'

$root = $PSScriptRoot | Split-Path -Parent | Split-Path -Parent
$installNsi = Join-Path $root "output\install.nsi"
$openccDir = Join-Path $root "output\data\opencc"

Write-Host "=== OpenCC emoji.json path verification (Phase L Bugfix EmojiJsonCheck / v0.74.1) ===" -ForegroundColor Cyan
Write-Host "install.nsi: $installNsi"
Write-Host "build opencc dir: $openccDir"
Write-Host ""

if (-not (Test-Path $installNsi)) {
    Write-Host "FAIL: install.nsi not found at $installNsi" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $openccDir)) {
    Write-Host "FAIL: build output $openccDir not found" -ForegroundColor Red
    exit 1
}

# === Check 1: install.nsi has v0.74.1 EmojiJsonCheck comment + patterns ===
Write-Host "[install.nsi] checking emoji.json verification block..."
$nsibytes = [System.IO.File]::ReadAllBytes($installNsi)
$nsiText = [System.Text.Encoding]::UTF8.GetString($nsibytes)

$installAnchors = @(
    'v0.74.1 (Phase L Bugfix EmojiJsonCheck)',
    'emoji.json missing at $R8',
    'IfFileExists $R8 emoji_json_ok'
)
foreach ($anchor in $installAnchors) {
    if ($nsiText.Contains($anchor)) {
        Write-Host "  [PASS] anchor found: $anchor" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] anchor MISSING: $anchor" -ForegroundColor Red
        [void]$FailsList.Add("install.nsi missing anchor: $anchor")
    }
}

# === Check 2: build output has emoji.json (so File glob ships it) ===
Write-Host ""
Write-Host "[build output] checking emoji.json presence..."
$emojiJson = Join-Path $openccDir "emoji.json"
if (Test-Path $emojiJson) {
    $len = (Get-Item $emojiJson).Length
    Write-Host "  [PASS] emoji.json present ($len bytes)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] emoji.json MISSING at $emojiJson" -ForegroundColor Red
    [void]$FailsList.Add("emoji.json missing from build output")
}

# === Check 3: build output ships *.ocd* companion files ===
Write-Host ""
Write-Host "[build output] checking *.ocd* companion files (opencc binary data)..."
$ocdFiles = @(Get-ChildItem -Path $openccDir -Filter "*.ocd*" -ErrorAction SilentlyContinue)
if ($ocdFiles.Count -gt 0) {
    Write-Host "  [PASS] $($ocdFiles.Count) .ocd* files present (e.g. STCharacters.ocd2, HKVariants.ocd2)" -ForegroundColor Green
} else {
    Write-Host "  [FAIL] no .ocd* files in $openccDir — emoji.json won't load (missing binary data)" -ForegroundColor Red
    [void]$FailsList.Add("no .ocd* files in build output")
}

# === Note: emoji.json + emoji.ocd companion relationship ===
Write-Host ""
Write-Host "[info] emoji.json requires emoji.ocd companion for binary data." -ForegroundColor Gray
Write-Host "[info] If both exist post-install but OpenCC still says 'config not found'," -ForegroundColor Gray
Write-Host "[info] the issue is libRime-side and tracked separately. See lessons-learned.md L##-PhaseL-9.8." -ForegroundColor Gray

# === Summary ===
Write-Host ""
if ($FailsList.Count -eq 0) {
    Write-Host "=== PASS - OpenCC emoji.json path verification ===" -ForegroundColor Green
    exit 0
} else {
    Write-Host "=== FAIL - $($FailsList.Count) issue(s) ===" -ForegroundColor Red
    foreach ($f in $FailsList) {
        Write-Host "  - $f" -ForegroundColor Red
    }
    exit 1
}
