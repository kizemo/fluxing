#Requires -Version 7.0
<#
    dpi-scale.ps1 - pure ASCII control/probe for the Windows per-source DPI scale.

    Mechanism (no third-party tools, no reboot, no logoff):
      QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS)  -> active source ids
      DisplayConfigGetDeviceInfo(GET_DPI_SCALE)  -> min/cur/max relative scale step
      DisplayConfigSetDeviceInfo(SET_DPI_SCALE)  -> change the relative step now

    Two layout traps fixed here, both verified by rc==87 before -> rc==0 after:

    1) DISPLAYCONFIG_PATH_INFO starts directly with sourceInfo. There is NO
       header padding inside it, so the source adapter LUID lives at offset 0:
         PATH_INFO +0  adapterId.LowPart
                   +4  adapterId.HighPart
                   +8  id            (the source id)
                   +12 modeInfoIdx
                   +16 statusFlags
                   +20 targetInfo ...
       Reading them at +8/+12/+16 silently yields id / modeInfoIdx / statusFlags
       and the OS answers ERROR_INVALID_PARAMETER for every device-info call.

    2) DISPLAYCONFIG_DEVICE_INFO_HEADER embeds a LUID, so it is 8-byte aligned
       and its REAL size is 24 bytes (20 bytes of fields + 4 trailing padding).
       Every packet payload therefore starts at offset 24, not 20:
         GET_SOURCE_NAME   24 + 64 (WCHAR[32])        = 88
         GET_DPI_SCALE     24 + 3 x RATIONAL = 48    (RATIONAL layout)
                           24 + 3 x LONG     -> pad = 40   (LONG layout)
         SET_DPI_SCALE     24 + RATIONAL = 32   /   24 + LONG -> pad = 32
       The two GET layouts are told apart by size. A wrong size is rejected with
       ERROR_INVALID_PARAMETER and has no side effect on a GET call. Both SET
       layouts share size 32 and payload offset 24, and writing the RATIONAL
       pair {rel, 1} also satisfies the LONG layout, which reads only rel.

    Modes
      Info     read-only: active sources, device names, rel triple, system DPI
      Set      change the rel step: -Rel <absolute> or -Delta <relative to cur>
      Restore  put back the original rel step recorded in dpi-state.json

    Exit codes: 0 ok, 2 cannot read scale, 3 cannot set scale, 4 verify failed.
#>
[CmdletBinding()]
param(
    [ValidateSet('Info', 'Set', 'Restore')]
    [string] $Mode = 'Info',
    [int] $Rel = [int]::MinValue,
    [int] $Delta = [int]::MinValue,
    [switch] $All
)

$ErrorActionPreference = 'Stop'

$script:StatePath = Join-Path $PSScriptRoot 'dpi-state.json'

$QDC_ONLY_ACTIVE_PATHS  = 2
$PATH_SIZE              = 72
$MODE_SIZE              = 64
$TYPE_GET_SOURCE_NAME   = 1
$TYPE_GET_DPI_SCALE     = -3
$TYPE_SET_DPI_SCALE     = -4
$ERR_INVALID_PARAMETER  = 87

if (-not ('DpiScaleNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class DpiScaleNative {
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x; public int y; }

    [DllImport("user32.dll")]
    public static extern int GetDisplayConfigBufferSizes(uint flags, out uint numPath, out uint numMode);

    [DllImport("user32.dll")]
    public static extern int QueryDisplayConfig(uint flags, ref uint numPath, IntPtr paths,
                                                ref uint numMode, IntPtr modes, IntPtr currentTopologyId);

    [DllImport("user32.dll")]
    public static extern int DisplayConfigGetDeviceInfo(IntPtr packet);

    [DllImport("user32.dll")]
    public static extern int DisplayConfigSetDeviceInfo(IntPtr packet);

    [DllImport("user32.dll")]
    public static extern IntPtr MonitorFromPoint(POINT pt, uint flags);

    [DllImport("user32.dll", EntryPoint = "GetMonitorInfoW")]
    public static extern bool GetMonitorInfoW(IntPtr hMonitor, IntPtr lpmi);

    [DllImport("user32.dll")]
    public static extern uint GetDpiForSystem();
}
'@
}

# Diagnostic sink. Kept on stderr-ish host stream so stdout stays machine readable.
$script:Diag = New-Object System.Collections.Generic.List[string]
function Add-Diag([string] $Text) { $script:Diag.Add($Text) }

function Get-SystemDpi {
    return [int]([DpiScaleNative]::GetDpiForSystem())
}

function Get-SystemDpiText {
    $d = Get-SystemDpi
    return ('{0} ({1}%)' -f $d, [Math]::Round($d * 100.0 / 96.0))
}

function Get-PrimaryDeviceName {
    $pt = [DpiScaleNative+POINT]::new()
    $pt.x = 0
    $pt.y = 0
    $h = [DpiScaleNative]::MonitorFromPoint($pt, 1)
    Add-Diag ('MONITOR_FROM_POINT={0}' -f $h.ToInt64())
    if ($h -eq [IntPtr]::Zero) { return '' }
    $mi = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(104)
    for ($i = 0; $i -lt 104; $i++) { [System.Runtime.InteropServices.Marshal]::WriteByte($mi, $i, 0) }
    [System.Runtime.InteropServices.Marshal]::WriteInt32($mi, 0, 104)
    $ok = [DpiScaleNative]::GetMonitorInfoW($h, $mi)
    Add-Diag ('GET_MONITOR_INFO={0}' -f $ok)
    $dev = ''
    if ($ok) {
        $dev = [System.Runtime.InteropServices.Marshal]::PtrToStringUni([IntPtr]($mi.ToInt64() + 40))
        $rcWin = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($mi.ToInt64() + 4))
        $rcW = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($mi.ToInt64() + 8))
        $rcH = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($mi.ToInt64() + 12))
        Add-Diag ('MONITOR_RECT={0}x{1}' -f $rcW, $rcH)
    }
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($mi)
    if ($null -eq $dev) { $dev = '' }
    return $dev
}

function Get-ActiveSources {
    [uint32]$np = 0
    [uint32]$nm = 0
    $rc = [DpiScaleNative]::GetDisplayConfigBufferSizes($QDC_ONLY_ACTIVE_PATHS, [ref]$np, [ref]$nm)
    if ($rc -ne 0) { throw "GetDisplayConfigBufferSizes rc=$rc" }
    Add-Diag ('BUFFER_SIZES paths={0} modes={1}' -f $np, $nm)
    if ($np -lt 1) { throw 'no active display path' }
    $paths = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([Math]::Max($np, 1) * $PATH_SIZE)
    $modes = [System.Runtime.InteropServices.Marshal]::AllocHGlobal([Math]::Max($nm, 1) * $MODE_SIZE)
    for ($i = 0; $i -lt ([Math]::Max($np, 1) * $PATH_SIZE); $i++) { [System.Runtime.InteropServices.Marshal]::WriteByte($paths, $i, 0) }
    $rc = [DpiScaleNative]::QueryDisplayConfig($QDC_ONLY_ACTIVE_PATHS, [ref]$np, $paths, [ref]$nm, $modes, [IntPtr]::Zero)
    if ($rc -ne 0) { throw "QueryDisplayConfig rc=$rc" }
    Add-Diag ('QUERY_PATHS={0}' -f $np)
    $list = @()
    for ($i = 0; $i -lt $np; $i++) {
        $b = $paths.ToInt64() + ($i * $PATH_SIZE)
        # sourceInfo starts at the very first byte of DISPLAYCONFIG_PATH_INFO
        $lo   = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($b + 0))
        $hi   = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($b + 4))
        $sid  = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($b + 8))
        $list += [pscustomobject]@{ Index = $i; AdapterLow = $lo; AdapterHigh = $hi; SourceId = $sid }
    }
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($paths)
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($modes)
    return $list
}

function New-Packet {
    param([int] $Type, [int] $Size, [int] $Lo, [int] $Hi, [int] $Id)
    $p = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(128)
    for ($i = 0; $i -lt 128; $i++) { [System.Runtime.InteropServices.Marshal]::WriteByte($p, $i, 0) }
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 0, $Type)
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 4, $Size)
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 8, $Lo)
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 12, $Hi)
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 16, $Id)
    return $p
}

function Get-SourceName {
    param($Source)
    $p = New-Packet -Type $TYPE_GET_SOURCE_NAME -Size 88 -Lo $Source.AdapterLow -Hi $Source.AdapterHigh -Id $Source.SourceId
    $rc = [DpiScaleNative]::DisplayConfigGetDeviceInfo($p)
    $name = ''
    if ($rc -eq 0) { $name = [System.Runtime.InteropServices.Marshal]::PtrToStringUni([IntPtr]($p.ToInt64() + 24)) }
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($p)
    if ($null -eq $name) { $name = '' }
    return [pscustomobject]@{ rc = $rc; name = $name }
}

function Get-DpiScale {
    param($Source)
    $lastRc = 0
    foreach ($size in 48, 40) {
        $p = New-Packet -Type $TYPE_GET_DPI_SCALE -Size $size -Lo $Source.AdapterLow -Hi $Source.AdapterHigh -Id $Source.SourceId
        $rc = [DpiScaleNative]::DisplayConfigGetDeviceInfo($p)
        $lastRc = $rc
        if ($rc -eq 0) {
            $base = $p.ToInt64()
            if ($size -eq 48) {
                $res = [pscustomobject]@{
                    rc = 0; size = 48; layout = 'RATIONAL'
                    min = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 24))
                    minDen = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 28))
                    cur = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 32))
                    curDen = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 36))
                    max = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 40))
                    maxDen = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 44))
                }
            }
            else {
                $res = [pscustomobject]@{
                    rc = 0; size = 40; layout = 'LONG'
                    min = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 24)); minDen = 1
                    cur = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 28)); curDen = 1
                    max = [System.Runtime.InteropServices.Marshal]::ReadInt32([IntPtr]($base + 32)); maxDen = 1
                }
            }
            [System.Runtime.InteropServices.Marshal]::FreeHGlobal($p)
            return $res
        }
        [System.Runtime.InteropServices.Marshal]::FreeHGlobal($p)
    }
    return [pscustomobject]@{ rc = $lastRc; size = 0; layout = 'UNKNOWN'; min = 0; minDen = 1; cur = 0; curDen = 1; max = 0; maxDen = 1 }
}

function Set-DpiScale {
    param($Source, [int] $RelValue, [string] $Layout)
    $size = 32
    $p = New-Packet -Type $TYPE_SET_DPI_SCALE -Size $size -Lo $Source.AdapterLow -Hi $Source.AdapterHigh -Id $Source.SourceId
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 24, $RelValue)
    [System.Runtime.InteropServices.Marshal]::WriteInt32($p, 28, 1)
    $rc = [DpiScaleNative]::DisplayConfigSetDeviceInfo($p)
    [System.Runtime.InteropServices.Marshal]::FreeHGlobal($p)
    return $rc
}

function Wait-SystemDpi {
    param([int] $Expect, [int] $TimeoutSec = 12)
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        $d = Get-SystemDpi
        if ($d -eq $Expect) { return $d }
        Start-Sleep -Milliseconds 250
    }
    return (Get-SystemDpi)
}

# ---------------------------------------------------------------- report diag first
foreach ($line in $script:Diag) { }   # placeholder, diag is emitted after collection

# ---------------------------------------------------------------- collect
$primaryName = Get-PrimaryDeviceName
$sysDpiNow = Get-SystemDpi

Write-Host ('MODE={0}' -f $Mode)
foreach ($line in $script:Diag) { Write-Host ('DIAG {0}' -f $line) }
Write-Host ('PRIMARY_DEVICE={0}' -f $primaryName)
Write-Host ('SYSTEM_DPI_NOW={0}' -f (Get-SystemDpiText))
Write-Host ('SYSTEM_DPI_NOW_RAW={0}' -f $sysDpiNow)

$sources = @()
foreach ($s in (Get-ActiveSources)) {
    $nm = Get-SourceName -Source $s
    $gs = Get-DpiScale -Source $s
    $isPrimary = ($primaryName -ne '' -and $nm.name -eq $primaryName)
    $row = [pscustomobject]@{
        Index       = $s.Index
        AdapterLow  = $s.AdapterLow
        AdapterHigh = $s.AdapterHigh
        SourceId    = $s.SourceId
        Name        = $nm.name
        IsPrimary   = $isPrimary
        Layout      = $gs.layout
        MinText     = ('{0}/{1}' -f $gs.min, $gs.minDen)
        CurText     = ('{0}/{1}' -f $gs.cur, $gs.curDen)
        MaxText     = ('{0}/{1}' -f $gs.max, $gs.maxDen)
        Min         = $gs.min
        Cur         = $gs.cur
        CurDen      = $gs.curDen
        Max         = $gs.max
        GetRc       = $gs.rc
    }
    $sources += $row
    Write-Host ('SOURCE idx={0} name={1} primary={2} layout={3} min={4} cur={5} max={6} get_rc={7} luid={8}:{9} id={10}' -f `
        $row.Index, $row.Name, $row.IsPrimary, $row.Layout, $row.MinText, $row.CurText, $row.MaxText, $row.GetRc, `
        $row.AdapterLow, $row.AdapterHigh, $row.SourceId)
}

# Degraded primary marking: if the monitor name lookup failed but there is exactly
# one active source, that source is unambiguously the one the user sees.
if ($primaryName -eq '' -and $sources.Count -eq 1) {
    $sources[0].IsPrimary = $true
    Write-Host 'PRIMARY_FALLBACK=single_active_source'
}

$getable = @($sources | Where-Object { $_.GetRc -eq 0 })
if ($getable.Count -eq 0) {
    Write-Host 'GETDPI_FAILED=all sources rejected both struct sizes'
    exit 2
}

if ($Mode -eq 'Info') {
    Write-Host 'INFO_OK'
    exit 0
}

# ---------------------------------------------------------------- Set / Restore
if ($Mode -eq 'Set') {
    $haveRel = $PSBoundParameters.ContainsKey('Rel')
    $haveDelta = $PSBoundParameters.ContainsKey('Delta')
    if (-not $haveRel -and -not $haveDelta) {
        Write-Host 'SET_FAILED=need -Rel or -Delta'
        exit 3
    }
    $targets = @($getable | Where-Object { $All -or $_.IsPrimary })
    if ($targets.Count -eq 0) {
        Write-Host 'SET_FAILED=no primary source matched; pass -All'
        exit 3
    }

    if (-not (Test-Path $script:StatePath)) {
        $orig = @()
        foreach ($t in $targets) {
            $orig += [pscustomobject]@{
                Name = $t.Name; AdapterLow = $t.AdapterLow; AdapterHigh = $t.AdapterHigh
                SourceId = $t.SourceId; Layout = $t.Layout; Rel = $t.Cur
            }
        }
        $state = [pscustomobject]@{ createdAt = (Get-Date).ToString('o'); systemDpi = $sysDpiNow; sources = $orig }
        $state | ConvertTo-Json -Depth 6 | Set-Content -Path $script:StatePath -Encoding utf8
        Write-Host ('STATE_SAVED={0} system_dpi={1}' -f $script:StatePath, $sysDpiNow)
    }
    else {
        Write-Host ('STATE_KEPT={0} (original step preserved)' -f $script:StatePath)
    }

    foreach ($t in $targets) {
        if ($haveRel) { $want = $Rel } else { $want = $t.Cur + $Delta }
        if ($null -ne $t.Max -and $t.Max -gt 0 -and $want -gt $t.Max) {
            Write-Host ('SET_CLAMPED want={0} max={1}' -f $want, $t.Max)
            $want = $t.Max
        }
        if ($null -ne $t.Min -and $want -lt $t.Min) {
            Write-Host ('SET_CLAMPED want={0} min={1}' -f $want, $t.Min)
            $want = $t.Min
        }
        $rc = Set-DpiScale -Source $t -RelValue $want -Layout $t.Layout
        Write-Host ('SET name={0} from_rel={1} to_rel={2} rc={3}' -f $t.Name, $t.Cur, $want, $rc)
        if ($rc -ne 0) {
            Write-Host ('SET_FAILED rc={0} hint=needs_elevation_or_unsupported' -f $rc)
            exit 3
        }
    }

    Start-Sleep -Milliseconds 1200
    $after = Get-SystemDpi
    Write-Host ('SYSTEM_DPI_AFTER={0}' -f (Get-SystemDpiText))
    Write-Host ('SET_OK system_dpi={0}' -f $after)
    exit 0
}

if ($Mode -eq 'Restore') {
    if (-not (Test-Path $script:StatePath)) {
        Write-Host ('RESTORE_FAILED=no state file at {0}' -f $script:StatePath)
        exit 3
    }
    $state = Get-Content -Path $script:StatePath -Raw -Encoding utf8 | ConvertFrom-Json
    $bad = 0
    foreach ($o in $state.sources) {
        $src = [pscustomobject]@{ AdapterLow = $o.AdapterLow; AdapterHigh = $o.AdapterHigh; SourceId = $o.SourceId }
        $rc = Set-DpiScale -Source $src -RelValue $o.Rel -Layout $o.Layout
        Write-Host ('RESTORE name={0} to_rel={1} rc={2}' -f $o.Name, $o.Rel, $rc)
        if ($rc -ne 0) { $bad++ }
    }
    if ($bad -gt 0) {
        Write-Host ('RESTORE_FAILED count={0}' -f $bad)
        exit 3
    }
    Start-Sleep -Milliseconds 1200
    $now = Get-SystemDpi
    Write-Host ('SYSTEM_DPI_RESTORED={0}' -f (Get-SystemDpiText))
    if ($now -ne [int]$state.systemDpi) {
        Write-Host ('RESTORE_VERIFY_FAILED expected={0} got={1}' -f $state.systemDpi, $now)
        exit 4
    }
    Remove-Item -Path $script:StatePath -Force
    Write-Host ('RESTORE_OK system_dpi={0}' -f $now)
    exit 0
}