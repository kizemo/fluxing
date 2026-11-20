# run-dpi-tier.ps1 -- pure ASCII. Batch BB follow-up: capture A-1 pixel evidence at one DPI tier.
#
# WHY THIS EXISTS
#   Defect D (CloudAccountDialog.cpp AccountUiDpi) caches the *value* of GetDpiForSystem for the
#   lifetime of WeaselServer.exe. So after the user changes the system scale, the already-running
#   server keeps laying the account page out at the OLD dpi. WeaselServer is PPL and cannot be
#   killed, and WM_DPICHANGED is swallowed on purpose (ADR 0068), so the proven way to make the
#   server re-read the system dpi is a same-version silent overwrite install (batch AZ: 41 s,
#   INSTALL_EXIT=0, pid changes, no sign-out needed).
#
#   This script does the whole chain for ONE tier and refuses to run when the tier is not really
#   active, so it can never emit misleading evidence.
#
# USAGE
#   pwsh -NoProfile -File reports\fluxing-bb-dpi-scale\run-dpi-tier.ps1 `
#        -ExpectedDpi 168 -OutDir reports\fluxing-bb-dpi-scale\evidence-dpi-175
#
# EXIT CODES
#   0 = probe ran (read the printed RPT/VERDICT lines; probe's own code is printed as PROBE_EXIT)
#   3 = precondition failed (desktop not Default / registry AppliedDPI != ExpectedDpi)
#   4 = installer missing or INSTALL_EXIT != 0
#   5 = server did not come back (or wrong version) after the overwrite install
#   6 = probe script missing, or report.json carried no parsable client geometry
#   7 = the measurement is not this tier: client geometry came back as the 96 dpi base value
#       (DPI virtualization) or as another tier's size (stale cached dpi in the running server)

param(
    [Parameter(Mandatory = $true)][int]$ExpectedDpi,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$Installer = 'F:\soft\00selfmade\rime_claude\output\archives\fluxing-1.3.7-installer.exe',
    [string]$InstallDir = 'D:\Program Files\fluxing',
    [string]$Probe = 'F:\soft\00selfmade\rime_claude\reports\fluxing-ba-a1-note-wrap\probe-a1-note-wrap.ps1',
    [string]$ExpectServerVersion = '1.3.7.0',
    [int]$RestartTimeoutSec = 240,
    [switch]$SkipRestart
)

$ErrorActionPreference = 'Stop'

function Step([string]$name, [string]$what) { Write-Host ("[{0}] {1}" -f $name, $what) }

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class TierNative {
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
    [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hdc, int index);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetThreadDesktop(uint dwThreadId);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool GetUserObjectInformationW(IntPtr hObj, int nIndex, StringBuilder pvInfo, int nLength, out int lpnLengthNeeded);
}
'@

function Get-DesktopName {
    $sb = New-Object System.Text.StringBuilder 260
    $need = 0
    $td = [TierNative]::GetThreadDesktop([TierNative]::GetCurrentThreadId())
    $ok = [TierNative]::GetUserObjectInformationW($td, 2, $sb, 520, [ref]$need)
    if (-not $ok) { return '<unknown>' }
    return $sb.ToString()
}

function Get-ServerList {
    $list = @()
    $ps = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue
    if ($ps) {
        foreach ($p in $ps) {
            $ver = '<n/a>'
            try { $ver = $p.MainModule.FileVersionInfo.FileVersion } catch { $ver = '<denied>' }
            $list += [pscustomobject]@{ Id = $p.Id; Start = $p.StartTime; Version = $ver }
        }
    }
    return $list
}

$resolvedOut = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutDir)
if (-not (Test-Path $resolvedOut)) { New-Item -ItemType Directory -Path $resolvedOut -Force | Out-Null }
$summaryPath = Join-Path $resolvedOut 'tier-summary.txt'
$consolePath = Join-Path $resolvedOut 'probe-console.txt'
$summary = New-Object System.Collections.Generic.List[string]
function Note([string]$line) { Write-Host $line; $summary.Add($line) | Out-Null }

Note ("TIER_OUTDIR={0}" -f $resolvedOut)
Note ("EXPECTED_DPI={0}  (scale={1}%)" -f $ExpectedDpi, [math]::Round($ExpectedDpi / 96 * 100))

# ---------- guard 1: real desktop (never capture on a virtual/RPA desktop) ----------
$desk = Get-DesktopName
Note ("DESKTOP={0}" -f $desk)
if ($desk -ne 'Default') {
    Note 'ABORT_NOT_DEFAULT_DESKTOP'
    $summary | Set-Content -Path $summaryPath -Encoding UTF8
    exit 3
}

# ---------- guard 2: the tier is really active right now ----------
$dpi = [int][TierNative]::GetDpiForSystem()
$logPixelsX = [int][TierNative]::GetDeviceCaps([TierNative]::GetDC([IntPtr]::Zero), 88)
$regLogPixels = '<n/a>'
try { $regLogPixels = (Get-ItemProperty -Path 'HKCU:\Control Panel\Desktop' -Name LogPixels -ErrorAction Stop).LogPixels } catch { $regLogPixels = '<none>' }
$regApplied = '<n/a>'
try { $regApplied = (Get-ItemProperty -Path 'HKCU:\Control Panel\Desktop\WindowMetrics' -Name AppliedDPI -ErrorAction Stop).AppliedDPI } catch { $regApplied = '<none>' }
Note ("ACTUAL_DPI={0}  LOGPIXELSX={1}  REG_LogPixels={2}  REG_AppliedDPI={3}" -f $dpi, $logPixelsX, $regLogPixels, $regApplied)
# The verdict source is the registry: AppliedDPI is written at logon and is the authoritative dpi of
# this session. GetDpiForSystem and LOGPIXELSX only describe THIS host process -- a DPI-unaware host
# reports 96 whatever the session is (batch BB measured exactly that in pwsh 7), so they can neither
# confirm nor deny a tier. LogPixels can be registered without being applied at all (batch BA:
# LogPixels=168 while AppliedDPI stayed 144 and the desktop was still 150%).
$appliedValue = 0
$appliedParsed = [int]::TryParse([string]$regApplied, [ref]$appliedValue)
if (-not $appliedParsed -or $appliedValue -ne $ExpectedDpi) {
    Note ("ABORT_WRONG_DPI applieddpi={0} expected={1}" -f $regApplied, $ExpectedDpi)
    Note '  the scale is registered but not applied -- set the preset, and sign out/in if AppliedDPI still disagrees'
    $summary | Set-Content -Path $summaryPath -Encoding UTF8
    exit 3
}
if ($appliedValue -ne $dpi) {
    Note ("WARN_HOST_DPI host={0} applied={1} (host awareness only, not a tier mismatch)" -f $dpi, $appliedValue)
}

# ---------- pre-state ----------
$before = Get-ServerList
foreach ($s in $before) { Note ("SERVER_BEFORE pid={0} start={1} ver={2}" -f $s.Id, $s.Start.ToString('HH:mm:ss'), $s.Version) }
if ($before.Count -eq 0) { Note 'SERVER_BEFORE=NONE' }
$oldIds = @($before | ForEach-Object { $_.Id })

if (-not $SkipRestart) {
    if (-not (Test-Path $Installer)) { Note ("ABORT_INSTALLER_MISSING {0}" -f $Installer); $summary | Set-Content -Path $summaryPath -Encoding UTF8; exit 4 }

    # ---------- restart WeaselServer by same-version overwrite install (proven path, batch AZ) ----------
    # The .cmd must stay pure ASCII; /D must be last and unquoted (NSIS rule).
    $cmdPath = Join-Path $resolvedOut 'reinstall.cmd'
    $cmdBody = "@echo off`r`n`"$Installer`" /S /D=$InstallDir`r`nexit /b %ERRORLEVEL%`r`n"
    Set-Content -Path $cmdPath -Value $cmdBody -Encoding Ascii -NoNewline
    Step 'install' ("silent overwrite install: {0} -> {1}" -f $Installer, $InstallDir)
    $t0 = Get-Date
    & cmd /c $cmdPath
    $installExit = $LASTEXITCODE
    $elapsed = [int]((Get-Date) - $t0).TotalSeconds
    Note ("INSTALL_EXIT={0} elapsed={1}s" -f $installExit, $elapsed)
    if ($installExit -ne 0) { Note 'ABORT_INSTALL_FAILED'; $summary | Set-Content -Path $summaryPath -Encoding UTF8; exit 4 }

    # ---------- wait for a NEW server process ----------
    Step 'server' ("waiting up to {0}s for a new WeaselServer" -f $RestartTimeoutSec)
    $deadline = (Get-Date).AddSeconds($RestartTimeoutSec)
    $fresh = @()
    while ((Get-Date) -lt $deadline) {
        $now = Get-ServerList
        $fresh = @($now | Where-Object { $oldIds -notcontains $_.Id })
        if ($fresh.Count -gt 0) { break }
        Start-Sleep -Milliseconds 700
    }
    if ($fresh.Count -eq 0) {
        Note 'ABORT_NO_NEW_SERVER (open any app and type, then re-run with -SkipRestart)'
        $summary | Set-Content -Path $summaryPath -Encoding UTF8
        exit 5
    }
    foreach ($s in $fresh) { Note ("SERVER_AFTER pid={0} start={1} ver={2}" -f $s.Id, $s.Start.ToString('HH:mm:ss'), $s.Version) }
    $badVer = @($fresh | Where-Object { $_.Version -ne $ExpectServerVersion })
    if ($badVer.Count -gt 0) {
        Note ("ABORT_SERVER_VERSION_MISMATCH expected={0}" -f $ExpectServerVersion)
        $summary | Set-Content -Path $summaryPath -Encoding UTF8
        exit 5
    }
    # let the message window finish creating
    Start-Sleep -Seconds 2
}

# ---------- the pixel probe ----------
if (-not (Test-Path $Probe)) { Note ("ABORT_PROBE_MISSING {0}" -f $Probe); $summary | Set-Content -Path $summaryPath -Encoding UTF8; exit 6 }

Step 'probe' ("running {0}" -f (Split-Path -Leaf $Probe))
$probeLines = & pwsh -NoProfile -File $Probe -OutDir $resolvedOut -ControlIdOfKnownSingleLine 1022 2>&1
$probeExit = $LASTEXITCODE
$probeText = @($probeLines | ForEach-Object { $_.ToString() })
$probeText | Set-Content -Path $consolePath -Encoding UTF8
Note ("PROBE_EXIT={0} console={1}" -f $probeExit, $consolePath)

# echo only the decision-bearing lines, so the transcript stays readable
foreach ($line in $probeText) {
    if ($line -match 'VERDICT|HEADROOM|CLIP|FAIL|PASS|REPORT') { Note ("  > " + $line) }
}

# flatten report.json leaves whose key names matter, without assuming the schema
$reportPath = Join-Path $resolvedOut 'report.json'
$clientRaw = $null
if (Test-Path $reportPath) {
    try {
        $json = Get-Content -Path $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
        $flat = New-Object System.Collections.Generic.List[string]
        $walk = $null
        $walk = {
            param($node, $prefix)
            if ($null -eq $node) { return }
            if ($node -is [System.Management.Automation.PSCustomObject]) {
                foreach ($prop in $node.PSObject.Properties) { & $walk $prop.Value ($prefix + '.' + $prop.Name) }
            } elseif ($node -is [System.Array]) {
                for ($i = 0; $i -lt $node.Count; $i++) { & $walk $node[$i] ($prefix + '[' + $i + ']') }
            } else {
                $flat.Add(($prefix.TrimStart('.') + '=' + $node)) | Out-Null
            }
        }
        & $walk $json ''
        $clientRaw = $json.dialog_client
        $keyed = @($flat | Where-Object { $_ -match 'dpi|width|height|verdict|headroom|clip|client|line|required|scroll|rect|note|font' })
        $n = 0
        foreach ($l in $keyed) { if ($n -ge 90) { break }; Note ("RPT " + $l); $n++ }
    } catch {
        Note ("RPT_PARSE_FAILED " + $_.Exception.Message)
    }
} else {
    Note 'RPT_MISSING report.json was not produced'
}

# ---------- guard 3: the measured geometry belongs to THIS tier ----------
# Two different lies can still pass guard 2: the probe read a DPI-virtualized rect (always the 96 dpi
# base 480x290), or the running server laid the page out with a cached old dpi (some other tier's
# size -- that is defect D). Both are recognizable by scaling the base client size with MulDiv and
# comparing it against the measured client rect.
$baseW = 480
$baseH = 290
if ([string]::IsNullOrWhiteSpace([string]$clientRaw)) {
    Note 'ABORT_NO_CLIENT_GEOMETRY report.json carried no dialog_client'
    $summary | Set-Content -Path $summaryPath -Encoding UTF8
    exit 6
}
$nums = @([regex]::Matches([string]$clientRaw, '-?\d+') | ForEach-Object { [int]$_.Value })
if ($nums.Count -lt 4) {
    Note ("ABORT_NO_CLIENT_GEOMETRY unparsable dialog_client={0}" -f $clientRaw)
    $summary | Set-Content -Path $summaryPath -Encoding UTF8
    exit 6
}
$clientW = $nums[2] - $nums[0]
$clientH = $nums[3] - $nums[1]
$expectW = [int][math]::Round($baseW * $ExpectedDpi / 96.0, 0, [System.MidpointRounding]::AwayFromZero)
$expectH = [int][math]::Round($baseH * $ExpectedDpi / 96.0, 0, [System.MidpointRounding]::AwayFromZero)
Note ("CLIENT={0}x{1}  EXPECTED={2}x{3}  tier={4}dpi" -f $clientW, $clientH, $expectW, $expectH, $ExpectedDpi)
if ($clientW -eq $expectW -and $clientH -eq $expectH) {
    Note 'CLIENT_MATCHES_TIER'
} else {
    $matched = '<none>'
    foreach ($cand in @(96, 120, 144, 168, 192, 216, 240)) {
        $cw = [int][math]::Round($baseW * $cand / 96.0, 0, [System.MidpointRounding]::AwayFromZero)
        if ($cw -eq $clientW) { $matched = ('{0}dpi' -f $cand); break }
    }
    if ($clientW -eq $baseW) {
        Note ("ABORT_DPI_VIRTUALIZED client={0}x{1} equals the 96 dpi base -- the probe read a virtualized rect" -f $clientW, $clientH)
    } else {
        Note ("ABORT_STALE_DPI client={0}x{1} matches {2}, not {3}dpi -- the running server still uses a cached dpi" -f $clientW, $clientH, $matched, $ExpectedDpi)
    }
    $summary | Set-Content -Path $summaryPath -Encoding UTF8
    exit 7
}

Note ("SUMMARY={0}" -f $summaryPath)
$summary | Set-Content -Path $summaryPath -Encoding UTF8
exit 0