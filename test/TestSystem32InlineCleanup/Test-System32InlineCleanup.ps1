# Test-System32InlineCleanup.ps1
# L##-PhaseL-9.10: v0.74.2 install.nsi System32 cleanup hardening test.
# Validates 7 anchors in output/install.nsi ensure silent-fail visibility
# + explicit weasel.old.dll exact-match + macro completion log.
# Run: pwsh -File test/TestSystem32InlineCleanup/Test-System32InlineCleanup.ps1

$ErrorActionPreference = 'Continue'

function Emit($label, $value) {
    Write-Host ("[{0}] {1}" -f $label, $value)
}

$instPath = 'F:\soft\00selfmade\rime_claude\output\install.nsi'
if (-not (Test-Path $instPath)) {
    Emit 'FATAL' "install.nsi not found at $instPath"
    exit 1
}

$content = Get-Content -Path $instPath -Raw -Encoding UTF8
$fails = New-Object 'System.Collections.Generic.List[string]'

# Test 1: 3 exact-match Delete anchors in CleanupLegacySystem32WeaselImpl macro
Emit 'T1' '--- 3 exact-match Delete anchors ---'
$exactMatches = @(
    'StrCpy $R8 "$WINDIR\System32\weaselx64.dll"',
    'StrCpy $R8 "$WINDIR\System32\weasel.dll"',
    'StrCpy $R8 "$WINDIR\System32\weasel.old.dll"'
)
foreach ($a in $exactMatches) {
    if ($content.Contains($a)) { Emit 'T1_OK' $a } else { Emit 'T1_FAIL' $a; $fails.Add("missing anchor: $a") }
}

# Test 2: 3 IfErrors detection after each exact-match Delete (silent-fail visibility)
Emit 'T2' '--- 3 IfErrors detection anchors ---'
$ifErrorsCount = ([regex]::Matches($content, 'IfErrors 0 \+2\s+DetailPrint "Fluxing v0\.74\.2: WARN Delete')).Count
if ($ifErrorsCount -ge 3) {
    Emit 'T2_OK' "IfErrors count = $ifErrorsCount (>= 3)"
} else {
    Emit 'T2_FAIL' "IfErrors count = $ifErrorsCount (< 3)"
    $fails.Add("IfErrors count insufficient: $ifErrorsCount")
}

# Test 3: macro completion DetailPrint at end of macro
Emit 'T3' '--- macro completion DetailPrint ---'
$completionLog = 'System32 cleanup macro complete (weaselx64.dll + weasel.dll + weasel.old.dll exact-match + weasel*.dll/weasel*.ime glob)'
if ($content.Contains($completionLog)) {
    Emit 'T3_OK' 'macro completion log present'
} else {
    Emit 'T3_FAIL' 'macro completion log missing'
    $fails.Add("missing macro completion log")
}

# Test 4: stage2 + schtasks path still exists (fallback retained)
Emit 'T4' '--- stage2 path fallback retained ---'
$stage2Anchors = @(
    'nsExec::ExecToStack ''schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_weasel',
    'nsExec::ExecToStack ''schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_weaselx64',
    'nsExec::ExecToStack ''schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_server',
    'nsExec::ExecToStack ''schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_deployer',
    'nsExec::ExecToStack ''schtasks /Create /SC ONSTART /TN FluxingStage2ShimSwap_setup'
)
foreach ($a in $stage2Anchors) {
    if ($content.Contains($a)) { Emit 'T4_OK' "stage2 task preserved" } else { Emit 'T4_FAIL' "stage2 task missing: $a"; $fails.Add("stage2 anchor lost: $a") }
}

# Test 5: .onInit still calls CleanupLegacySystem32Weasel + Section Uninstall still calls un.CleanupLegacySystem32Weasel
Emit 'T5' '--- cleanup function call sites ---'
$callSites = @(
    'Call CleanupLegacySystem32Weasel',
    'Call un.CleanupLegacySystem32Weasel'
)
foreach ($a in $callSites) {
    if ($content.Contains($a)) { Emit 'T5_OK' "call site preserved: $a" } else { Emit 'T5_FAIL' "call site missing: $a"; $fails.Add("call site lost: $a") }
}

# Test 6: BOM + CRLF sanity (L09)
Emit 'T6' '--- NSIS file encoding ---'
$bytes = [System.IO.File]::ReadAllBytes($instPath)
if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
    Emit 'T6_OK' 'UTF-8 BOM present (L09)'
} else {
    Emit 'T6_FAIL' 'UTF-8 BOM missing (L09)'
    $fails.Add('missing BOM')
}
$hasCrlf = ($content -match "`r`n").Count -gt 0
$hasLfOnly = ($content -match "(?<!`r)`n").Count -gt 0
if ($hasCrlf -and -not $hasLfOnly) {
    Emit 'T6_OK' 'CRLF only (no LF-only lines)'
} else {
    Emit 'T6_WARN' "CRLF=$hasCrlf LF-only=$hasLfOnly"
}

# Final
Write-Host ''
if ($fails.Count -eq 0) {
    Emit 'RESULT' 'PASS - all anchors OK'
    exit 0
} else {
    Emit 'RESULT_FAIL' ($fails.Count.ToString() + ' failures:')
    foreach ($f in $fails) { Emit 'FAIL_DETAIL' $f }
    exit 1
}