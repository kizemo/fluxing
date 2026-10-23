# Test-ReadyFlagMessageBox.ps1
# Spec 071 FR-005 / T201: verify the v0.74.1 Ready-flag MessageBox ships
# in output/install.nsi and triggers on (real-machine + skip Ready write).
#
# Two checks:
#   1. Source-level: install.nsi contains the non-silent warning block:
#      - "skip_warning_ready" label
#      - "IfSilent skip_warning_ready" jump
#      - MessageBox MB_OK|MB_ICONEXCLAMATION with "Ready" + "30" + "Fluxing"
#      - "$1 != \"1\"" or StrCmp-style check for non-Ready
#      Catches commit-time ADR/code divergence without a real install.
#   2. Manual real-machine procedure (documented for the user): how to
#      verify the MessageBox actually fires when WeaselServer does NOT
#      set the Ready key within 30s. This requires interactive installer
#      run on a real Windows machine with WeaselServer startup blocked.
#
# Why a hybrid test: source-level catches ADR drift; manual real-machine
# catches runtime behavior (NSIS MessageBox + polling loop integration).
# Both must pass before v0.74.1 ships.

$ErrorActionPreference = 'Stop'

$installNsi = (Resolve-Path "$PSScriptRoot\..\..\output\install.nsi").Path
if (-not (Test-Path $installNsi)) {
    Write-Host "FAIL: $installNsi not found" -ForegroundColor Red
    exit 1
}

$nsiBytes = [System.IO.File]::ReadAllBytes($installNsi)
$nsiText = [System.Text.Encoding]::UTF8.GetString($nsiBytes)

Write-Host "[1/2] Source-level ship-gate (install.nsi Ready-flag MessageBox)..."

$shipGateAnchors = @(
    @('skip_warning_ready label present',              'skip_warning_ready:'),
    @('IfSilent skip_warning_ready jump',              'IfSilent skip_warning_ready'),
    @('Ready-flag MessageBox MB_OK|MB_ICONEXCLAMATION', 'MessageBox MB_OK|MB_ICONEXCLAMATION "Fluxing 服务未在 30 秒内响应 Ready 信号'),
    @('Ready-flag MessageBox has $1 != "1" check',      '$1 != "1"'),
    @('Ready-flag handshake poll loop (pre-existing)',  'ReadRegStr $1 HKCU "Software\Rime\Weasel" "Ready"'),
    @('Ready-flag 30s timeout message (pre-existing)',  'did not signal Ready within 30s')
)

$srcFails = New-Object 'System.Collections.Generic.List[string]'
foreach ($pair in $shipGateAnchors) {
    $label = $pair[0]
    $anchor = $pair[1]
    if ($nsiText.Contains($anchor)) {
        Write-Host "  [PASS] $label" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $label -- anchor MISSING: $anchor" -ForegroundColor Red
        [void]$srcFails.Add("$label -- missing: $anchor")
    }
}

# Ordering check: MessageBox must appear AFTER the ${Loop} (poll loop close)
# and BEFORE the next major section (e.g. CheckForUpdates).
$loopCloseLine = ($nsiText -split "`n" | Select-String -Pattern '^  \$\{Loop\}' | Select-Object -Last 1).LineNumber
$msgBoxLine = ($nsiText -split "`n" | Select-String -SimpleMatch -Pattern 'MessageBox MB_OK|MB_ICONEXCLAMATION "Fluxing 服务未在 30 秒内响应 Ready 信号' | Select-Object -First 1).LineNumber
$skipLabelLine = ($nsiText -split "`n" | Select-String -SimpleMatch -Pattern 'skip_warning_ready:' | Select-Object -First 1).LineNumber
if ($loopCloseLine -gt 0 -and $msgBoxLine -gt 0 -and $skipLabelLine -gt 0) {
    if ($loopCloseLine -lt $msgBoxLine -and $msgBoxLine -lt $skipLabelLine) {
        Write-Host "  [PASS] Ordering: poll loop close (line $loopCloseLine) < MessageBox (line $msgBoxLine) < skip_warning_ready (line $skipLabelLine)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] Ordering: poll close=$loopCloseLine, MessageBox=$msgBoxLine, skip=$skipLabelLine -- REGRESSION" -ForegroundColor Red
        [void]$srcFails.Add("Ordering regression: poll $loopCloseLine, msgBox $msgBoxLine, skip $skipLabelLine")
    }
} else {
    Write-Host "  [WARN] Ordering check could not run (loop=$loopCloseLine, msgBox=$msgBoxLine, skip=$skipLabelLine)" -ForegroundColor Yellow
}

if ($srcFails.Count -gt 0) {
    Write-Host "FAIL: $($srcFails.Count) source-level ship-gate check(s) failed" -ForegroundColor Red
    foreach ($f in $srcFails) { Write-Host "  $f" -ForegroundColor Red }
    exit 1
}

Write-Host "[1/2] All assertions passed." -ForegroundColor Green

# === Test 2: Manual real-machine verification procedure (documented) ===
Write-Host ""
Write-Host "[2/2] Manual real-machine procedure (documented below; requires user)" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Pre-requisites:" -ForegroundColor Cyan
Write-Host "    - Windows 10/11 machine with Fluxing previously installed (or ready to install)"
Write-Host "    - fluxing-0.19.0.74.1-installer.exe in output\archives\"
Write-Host "    - 5-10 minutes for the install + 30s timeout window"
Write-Host ""
Write-Host "  Steps to verify the MessageBox fires when Ready is never set:"
Write-Host "    1. Open Task Manager -> Details tab"
Write-Host "    2. Right-click WeaselServer.exe -> End task (if running)"
Write-Host "    3. Rename HKCU\Software\Rime\Weasel so the new install can't read"
Write-Host "       the previous Ready key. cmd /c:"
Write-Host "         reg delete HKCU\Software\Rime\Weasel /f"
Write-Host '    4. Optional: rename WeaselServer.exe in the install target dir BEFORE'
Write-Host '       running installer, so the new install Exec WeaselServer'
Write-Host '       fails silently (skip Ready write). e.g.:'
Write-Host "         ren \"C:\Program Files\Fluxing\WeaselServer.exe\" WeaselServer.exe.disabled"
Write-Host "    5. Run installer INTERACTIVELY (no /S flag):"
Write-Host "         output\archives\fluxing-0.19.0.74.1-installer.exe"
Write-Host "    6. After the installer copies files, observe the Ready-flag poll"
Write-Host "       starts (DetailPrint 'Waiting for WeaselServer Ready flag')."
Write-Host "    7. After ~30 seconds, a MessageBox MUST appear with title"
Write-Host "       'Fluxing 服务未在 30 秒内响应 Ready 信号' (or similar wording)."
Write-Host "    8. Click OK to dismiss; install completes (silent Abort still fires"
Write-Host "       if FluxingStage2Needed=1, but the MessageBox precedes it)."
Write-Host ""
Write-Host "  Negative test (silent mode must NOT show MessageBox):"
Write-Host "    9. Re-run installer SILENTLY (with /S /D=<path>):"
Write-Host "         output\archives\fluxing-0.19.0.74.1-installer.exe /S /D=C:\Fluxing_Test"
Write-Host "   10. During the Ready-flag poll, NO MessageBox should appear (the"
Write-Host "       IfSilent skip_warning_ready jump skips the warning block)."
Write-Host "   11. Verify exit code is 0 if Ready was set, or 6001 if"
Write-Host "       FluxingStage2Needed=1 (silent Abort path, pre-existing)."
Write-Host ""
Write-Host "  Expected outcomes:"
Write-Host "    Interactive + skip Ready: MessageBox visible (spec 071 FR-005 PASS)"
Write-Host "    Interactive + normal Ready: NO MessageBox (Ready was set within 30s)"
Write-Host "    Silent + skip Ready: NO MessageBox, exit code 0 or 6001"
Write-Host ""
Write-Host "[2/2] Manual verification procedure documented." -ForegroundColor Green

Write-Host ""
Write-Host "Source-level: PASS (1/2). Real-machine: requires manual run."
exit 0