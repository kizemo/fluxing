#Requires -Version 5.1
<#
.SYNOPSIS
    Fluxing runtime verification harness (batch AW / release 1.3.5).

.DESCRIPTION
    Drives the ALREADY INSTALLED Fluxing WeaselServer.exe to verify the two
    batch-AW defects on the real machine, without a human at the keyboard:

      Defect B (ADR 0066) : the "Account and Sync" window must be closable on
                            the 2nd / 3rd entry (the reported bug).
      Defect A (ADR 0065) : "IME switched on" alone must NOT block cloud sync.

    Mechanism (no mouse required for the primary path):
      * the tray menu item "Account and Sync" is ID_WEASELTRAY_CLOUD_ACCOUNT
        = 40024 (WeaselServer/resource.h). Posting WM_COMMAND(40024) to the
        server tray window ("TrayIconClass") opens the account window.
      * buttons are located by CONTROL ID (kIdClose = 1010, kIdSyncNow = 1016,
        kIdSyncStatus = 1017 - WeaselServer/Cloud/CloudAccountUi.h), so this
        script contains no non-ASCII literals (PowerShell 5.1 reads BOM-less
        .ps1 files as ANSI -> Chinese literals would be mangled).
      * closing is done with BM_CLICK on the close button, i.e. exactly the
        WM_COMMAND -> parent handler path a real mouse click takes.
      * an optional real synthesized mouse click (-UseRealMouse) is offered as
        an independent user-path probe (needs foreground + Default desktop).

    Gate probe (defect A): the maintenance export file
    <userdata>\sync\<account>\*.userdb.txt is rewritten ONLY when a sync round
    passes the maintenance gate. "Skipped" rounds never touch it (ADR 0062:
    the gate is checked before LoadSidecar / export). So:
        export mtime advanced  => round was NOT skipped => gate allowed it.
        export mtime unchanged => round was skipped (or failed before export).

.EXAMPLE
    pwsh -File scripts\verify\verify-cloud-account-window.ps1 -Mode All
#>
[CmdletBinding()]
param(
    [ValidateSet('Discover', 'DefectB', 'DefectA', 'All')]
    [string]$Mode = 'All',

    [string]$UserDataDir = 'D:\Program Files\fluxing\user1\fluxing',

    [string]$OutDir = '',

    # Use a real synthesized mouse click instead of BM_CLICK for the close
    # button (user-path fidelity probe).
    [switch]$UseRealMouse,

    # Seconds to wait for the maintenance export file to be rewritten after
    # "Sync now" was pressed.
    [int]$SyncWaitSeconds = 150
)

$ErrorActionPreference = 'Stop'

if (-not ('FluxingW32' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class FluxingW32 {
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetThreadDesktop(uint tid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool GetUserObjectInformationW(IntPtr h, int idx, StringBuilder info, int n, out int needed);

    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }

    // Marshalling a struct through [ref] from PowerShell does not work
    // reliably, so expose the rect as a plain int[] instead.
    public static int[] GetRect(IntPtr h) {
        RECT r;
        if (!GetWindowRect(h, out r)) return null;
        return new int[] { r.Left, r.Top, r.Right, r.Bottom };
    }
}
'@
}

# ---------------------------------------------------------------------------
# constants
# ---------------------------------------------------------------------------
$WM_COMMAND = 0x0111
$WM_CLOSE = 0x0010
$BM_CLICK = 0x00F5
$WM_GETTEXT = 0x000D

$ID_MENU_CLOUD_ACCOUNT = 40024
$ID_CLS_TRAY = 'TrayIconClass'
$ID_CLS_ACCOUNT = 'FluxingCloudAccountDialog_v1'

$KID_CLOSE = 1010
$KID_SYNC_NOW = 1016
$KID_SYNC_STATUS = 1017
$KID_STATUS = 1006
$KID_ACCOUNT_LABEL = 1020

$report = [ordered]@{
    started_at = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
    mode = $Mode
    steps = @()
    verdict = 'UNDECIDED'
}

function Write-Step([string]$name, [string]$detail, [string]$state = 'INFO') {
    $script:report.steps += [ordered]@{ step = $name; state = $state; detail = $detail }
    Write-Host ("[{0}] {1} :: {2}" -f $state, $name, $detail)
}

function Get-ClassName([IntPtr]$h) {
    $sb = New-Object System.Text.StringBuilder 256
    [void][FluxingW32]::GetClassNameW($h, $sb, 256)
    $sb.ToString()
}

function Get-Caption([IntPtr]$h) {
    $sb = New-Object System.Text.StringBuilder 1024
    [void][FluxingW32]::GetWindowTextW($h, $sb, 1024)
    $sb.ToString()
}

function Get-TopWindows([uint32]$procId) {
    $list = New-Object System.Collections.ArrayList
    $cb = [FluxingW32+EnumProc]{
        param($h, $p)
        $pid2 = 0
        [void][FluxingW32]::GetWindowThreadProcessId($h, [ref]$pid2)
        if ($pid2 -eq $procId) {
            [void]$list.Add([ordered]@{ hwnd = $h; cls = (Get-ClassName $h); visible = [FluxingW32]::IsWindowVisible($h) })
        }
        return $true
    }
    [void][FluxingW32]::EnumWindows($cb, [IntPtr]::Zero)
    $list
}

function Get-Children([IntPtr]$parent) {
    $list = New-Object System.Collections.ArrayList
    $cb = [FluxingW32+EnumProc]{
        param($h, $p)
        [void]$list.Add([ordered]@{
                hwnd = $h
                id = [FluxingW32]::GetDlgCtrlID($h)
                cls = (Get-ClassName $h)
                visible = [FluxingW32]::IsWindowVisible($h)
                rect = (Get-Rect $h)
            })
        return $true
    }
    [void][FluxingW32]::EnumChildWindows($parent, $cb, [IntPtr]::Zero)
    $list
}

function Get-Rect([IntPtr]$h) {
    $a = [FluxingW32]::GetRect($h)
    if ($null -eq $a) { return '' }
    return ("{0},{1},{2},{3}" -f $a[0], $a[1], $a[2], $a[3])
}

function Get-UiaNames([IntPtr]$h) {
    $out = @{}
    try {
        Add-Type -AssemblyName UIAutomationClient -ErrorAction SilentlyContinue
        Add-Type -AssemblyName UIAutomationTypes -ErrorAction SilentlyContinue
        $root = [System.Windows.Automation.AutomationElement]::FromHandle($h)
        if ($null -eq $root) { return $out }
        $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($e in $all) {
            try {
                $name = $e.Current.Name
                $aid = $e.Current.AutomationId
                $ct = $e.Current.ControlType.ProgrammaticName
                if (-not [string]::IsNullOrEmpty($name)) {
                    $out["$aid|$ct"] = $name
                }
            } catch { }
        }
    } catch {
        Write-Step 'uia' ("UIA unavailable: " + $_.Exception.Message) 'WARN'
    }
    $out
}

function Find-TopWindow([string]$cls, [uint32]$procId, [bool]$visibleOnly = $true) {
    foreach ($w in Get-TopWindows $procId) {
        if ($w.cls -eq $cls) {
            if ((-not $visibleOnly) -or $w.visible) { return $w.hwnd }
        }
    }
    return [IntPtr]::Zero
}

function Find-ChildById([IntPtr]$parent, [int]$ctrlId) {
    foreach ($c in Get-Children $parent) {
        if ($c.id -eq $ctrlId) { return $c.hwnd }
    }
    return [IntPtr]::Zero
}

function Wait-For([scriptblock]$cond, [int]$timeoutMs, [int]$stepMs = 150) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        if (& $cond) { return $true }
        Start-Sleep -Milliseconds $stepMs
    }
    return $false
}

function Save-Shot([IntPtr]$hwnd, [string]$path) {
    try {
        Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue
        $a = [FluxingW32]::GetRect($hwnd)
        if ($null -eq $a) { return $false }
        $w = $a[2] - $a[0]
        $ht = $a[3] - $a[1]
        if ($w -le 0 -or $ht -le 0) { return $false }
        $bmp = New-Object System.Drawing.Bitmap($w, $ht)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $hdc = $g.GetHdc()
        [void][FluxingW32]::PrintWindow($hwnd, $hdc, 2)
        $g.ReleaseHdc($hdc)
        $g.Dispose()
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        return $true
    } catch {
        Write-Step 'screenshot' ("capture failed: " + $_.Exception.Message) 'WARN'
        return $false
    }
}

function Assert-DefaultDesktop {
    $tid = [System.Diagnostics.Process]::GetCurrentProcess().Threads[0].Id
    $desk = [FluxingW32]::GetThreadDesktop([uint32]$tid)
    $sb = New-Object System.Text.StringBuilder 256
    $need = 0
    [void][FluxingW32]::GetUserObjectInformationW($desk, 2, $sb, 256, [ref]$need)
    $name = $sb.ToString()
    if ($name -ne 'Default') {
        Write-Step 'desktop' ("current desktop is '$name' (not 'Default') - real mouse probe unsafe") 'WARN'
        return $false
    }
    Write-Step 'desktop' "Default desktop confirmed" 'OK'
    return $true
}

function Get-ExportFile {
    $dir = Join-Path $UserDataDir 'sync'
    if (-not (Test-Path $dir)) { return $null }
    $files = Get-ChildItem $dir -Recurse -File -Filter '*.userdb.txt' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending
    if ($files.Count -eq 0) { return $null }
    $files[0]
}

# ---------------------------------------------------------------------------
# setup
# ---------------------------------------------------------------------------
if ([string]::IsNullOrEmpty($OutDir)) {
    $OutDir = Join-Path $env:TEMP ('fluxing-ax-verify-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$proc = Get-Process WeaselServer -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $proc) {
    Write-Step 'precondition' 'WeaselServer.exe is not running' 'FAIL'
    $report.verdict = 'FAIL'
    $report | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
    Write-Host "REPORT: $(Join-Path $OutDir 'report.json')"
    exit 2
}
$procId = [uint32]$proc.Id
Write-Step 'precondition' ("WeaselServer pid=$procId started=" + $proc.StartTime.ToString('yyyy-MM-dd HH:mm:ss') +
    " path=" + $proc.Path) 'OK'

$serverExe = $proc.Path
if (Test-Path $serverExe) {
    $vi = (Get-Item $serverExe).VersionInfo
    Write-Step 'binary' ("installed filever=" + $vi.FileVersion + " size=" + (Get-Item $serverExe).Length) 'OK'
}

$tray = Find-TopWindow $ID_CLS_TRAY $procId $false
if ($tray -eq [IntPtr]::Zero) {
    Write-Step 'precondition' ("tray window '$ID_CLS_TRAY' not found in pid $procId") 'FAIL'
    $report.verdict = 'FAIL'
    $report | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
    exit 2
}
Write-Step 'precondition' ("tray window found: hwnd=" + $tray) 'OK'

# WM_COMMAND(40024) must reach the server's OWN main window (m_server.GetHWnd()),
# NOT the tray shell window: CSystemTray's WndProc only owns the tray icon and
# the menu popup, the menu selection is forwarded to the server window.
# We do not hardcode that window's class name (it differs per build); we post
# to every top-level window of the server process instead - only command id
# 40024 has a registered handler, so the extra posts are inert.
$cmdTargets = @()
foreach ($w in Get-TopWindows $procId) {
    Write-Step 'server-window' ("hwnd=" + $w.hwnd + " class='" + $w.cls + "' visible=" + $w.visible) 'INFO'
    if ($w.hwnd -ne $tray) { $cmdTargets += $w.hwnd }
}
$script:cmdTargets = $cmdTargets
Write-Step 'precondition' ("WM_COMMAND targets: " + $cmdTargets.Count) 'OK'

function Open-AccountWindow {
    param([int]$TimeoutMs = 12000)
    foreach ($t in $script:cmdTargets) {
        [void][FluxingW32]::PostMessage($t, $WM_COMMAND, [IntPtr]$ID_MENU_CLOUD_ACCOUNT, [IntPtr]::Zero)
    }
    $found = [IntPtr]::Zero
    [void](Wait-For {
            $script:found = Find-TopWindow $ID_CLS_ACCOUNT $procId $true
            return ($script:found -ne [IntPtr]::Zero)
        } $TimeoutMs 200)
    $script:found
}

function Close-ViaButton([IntPtr]$dlg, [bool]$realMouse) {
    $btn = Find-ChildById $dlg $KID_CLOSE
    if ($btn -eq [IntPtr]::Zero) {
        Write-Step 'close-button' "control id $KID_CLOSE not found" 'FAIL'
        return $false
    }
    if ($realMouse) {
        $ok = Assert-DefaultDesktop
        [void][FluxingW32]::SetForegroundWindow($dlg)
        Start-Sleep -Milliseconds 300
        $a = [FluxingW32]::GetRect($btn)
        if ($null -eq $a) { Write-Step 'close-button' 'GetRect failed for close button' 'FAIL'; return $false }
        $x = [int](($a[0] + $a[2]) / 2)
        $y = [int](($a[1] + $a[3]) / 2)
        [void][FluxingW32]::SetCursorPos($x, $y)
        Start-Sleep -Milliseconds 120
        [FluxingW32]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 60
        [FluxingW32]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
        Write-Step 'close-button' "real mouse click at $x,$y (desktop ok=$ok)" 'OK'
    } else {
        [void][FluxingW32]::SendMessage($btn, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
        Write-Step 'close-button' "BM_CLICK sent to control id $KID_CLOSE" 'OK'
    }
    return $true
}

# ---------------------------------------------------------------------------
# mode: Discover - dump the window / control tree
# ---------------------------------------------------------------------------
if ($Mode -eq 'Discover' -or $Mode -eq 'All' -or $Mode -eq 'DefectB' -or $Mode -eq 'DefectA') {
    $existing = Find-TopWindow $ID_CLS_ACCOUNT $procId $true
    if ($existing -ne [IntPtr]::Zero) {
        Write-Step 'pre-existing' ("account window already open hwnd=" + $existing + " -> closing it") 'INFO'
        [void][FluxingW32]::PostMessage($existing, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        [void](Wait-For { return -not [FluxingW32]::IsWindow($existing) } 4000 150)
    }
}

if ($Mode -eq 'Discover') {
    $dlg = Open-AccountWindow
    if ($dlg -eq [IntPtr]::Zero) {
        Write-Step 'open' 'account window did NOT appear' 'FAIL'
        $report.verdict = 'FAIL'
    } else {
        Write-Step 'open' ("account window opened hwnd=" + $dlg + " rect=" + (Get-Rect $dlg)) 'OK'
        foreach ($c in Get-Children $dlg) {
            Write-Host ("   child id={0,-6} cls={1,-16} visible={2,-5} rect={3}" -f $c.id, $c.cls, $c.visible, $c.rect)
        }
        $names = Get-UiaNames $dlg
        foreach ($k in $names.Keys) { Write-Host ("   uia {0} => {1}" -f $k, $names[$k]) }
        Save-Shot $dlg (Join-Path $OutDir 'discover-account.png') | Out-Null
    }
    $report | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
    Write-Host "REPORT: $(Join-Path $OutDir 'report.json')"
    exit 0
}

# ---------------------------------------------------------------------------
# mode: DefectB - open / close / reopen / close  (the reported bug)
# ---------------------------------------------------------------------------
$bAllPass = $true
if ($Mode -eq 'DefectB' -or $Mode -eq 'All') {
    for ($i = 1; $i -le 3; $i++) {
        $dlg = Open-AccountWindow
        if ($dlg -eq [IntPtr]::Zero) {
            Write-Step "B-cycle$i-open" 'account window did NOT appear' 'FAIL'
            $bAllPass = $false
            continue
        }
        $shot = Join-Path $OutDir ("account-cycle{0}.png" -f $i)
        Save-Shot $dlg $shot | Out-Null
        Write-Step "B-cycle$i-open" ("opened hwnd=" + $dlg + " rect=" + (Get-Rect $dlg) + " shot=" + $shot) 'OK'

        $clicked = Close-ViaButton $dlg $UseRealMouse.IsPresent
        if (-not $clicked) { $bAllPass = $false; continue }

        $gone = Wait-For { return -not [FluxingW32]::IsWindow($dlg) } 6000 120
        if ($gone) {
            Write-Step "B-cycle$i-close" 'window destroyed after close click (IsWindow=FALSE)' 'PASS'
        } else {
            Write-Step "B-cycle$i-close" ("window STILL ALIVE after close click (IsWindow=TRUE) - defect NOT fixed") 'FAIL'
            $bAllPass = $false
            Save-Shot $dlg (Join-Path $OutDir ("account-cycle{0}-stuck.png" -f $i)) | Out-Null
            # leave no half-dead window behind for the next cycle
            [void][FluxingW32]::PostMessage($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 800
        }
    }
    $report['defectB'] = if ($bAllPass) { 'PASS' } else { 'FAIL' }
    if (-not $bAllPass) { $report.verdict = 'FAIL' }
}

# ---------------------------------------------------------------------------
# mode: DefectA - "Sync now" must not be blocked by the maintenance gate
# ---------------------------------------------------------------------------
$aPass = $false
if ($Mode -eq 'DefectA' -or $Mode -eq 'All') {
    $before = Get-ExportFile
    $beforeStamp = if ($before) { $before.LastWriteTimeUtc.Ticks } else { 0 }
    $beforeInfo = if ($before) { "$($before.FullName) mtime=$($before.LastWriteTime.ToString('HH:mm:ss')) size=$($before.Length)" } else { 'NO EXPORT FILE FOUND' }
    Write-Step 'A-baseline' $beforeInfo 'INFO'

    $dlg = Open-AccountWindow
    if ($dlg -eq [IntPtr]::Zero) {
        Write-Step 'A-open' 'account window did NOT appear' 'FAIL'
    } else {
        $syncBtn = Find-ChildById $dlg $KID_SYNC_NOW
        if ($syncBtn -eq [IntPtr]::Zero) {
            Write-Step 'A-sync-button' "control id $KID_SYNC_NOW not found (not logged in?)" 'FAIL'
        } else {
            $names = Get-UiaNames $dlg
            $statusBefore = ''
            foreach ($k in $names.Keys) { if ($k -match 'SyncStatus|1017') { $statusBefore = $names[$k] } }
            Write-Step 'A-status-before' ("sync status label = '" + $statusBefore + "'") 'INFO'

            [void][FluxingW32]::SendMessage($syncBtn, $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
            Write-Step 'A-sync-click' 'BM_CLICK sent to control id 1016 (Sync now)' 'OK'

            $advanced = $false
            $sw = [Diagnostics.Stopwatch]::StartNew()
            while ($sw.Elapsed.TotalSeconds -lt $SyncWaitSeconds) {
                Start-Sleep -Seconds 2
                $now = Get-ExportFile
                if ($null -ne $now -and $now.LastWriteTimeUtc.Ticks -ne $beforeStamp) { $advanced = $true; break }
                # capture the status label mid-flight
                $names = Get-UiaNames $dlg
                foreach ($k in $names.Keys) {
                    if ($k -match 'SyncStatus|1017') {
                        $t = $names[$k]
                        if ($t -ne $statusBefore) { $statusBefore = $t; Write-Step 'A-status-live' ("sync status now = '" + $t + "'") 'INFO' }
                    }
                }
            }
            Save-Shot $dlg (Join-Path $OutDir 'account-after-sync.png') | Out-Null

            $names = Get-UiaNames $dlg
            $statusAfter = ''
            foreach ($k in $names.Keys) { if ($k -match 'SyncStatus|1017') { $statusAfter = $names[$k] } }
            Write-Step 'A-status-after' ("sync status label = '" + $statusAfter + "'") 'INFO'

            $after = Get-ExportFile
            Write-Step 'A-export' ("after: " + $(if ($after) { "$($after.FullName) mtime=$($after.LastWriteTime.ToString('HH:mm:ss')) size=$($after.Length)" } else { 'NO EXPORT FILE' })) 'INFO'

            if ($advanced) {
                Write-Step 'A-verdict' 'maintenance export file was rewritten => the sync round was NOT skipped => gate allowed it' 'PASS'
                $aPass = $true
            } else {
                Write-Step 'A-verdict' ("export file NOT rewritten within $SyncWaitSeconds s => round was skipped (gate closed) or failed before export") 'FAIL'
            }
            $report['defectA'] = if ($aPass) { 'PASS' } else { 'FAIL' }
        }
        # leave the UI tidy
        [void][FluxingW32]::PostMessage($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 600
    }
    if (-not $aPass -and $report.verdict -ne 'FAIL') { $report.verdict = 'FAIL' }
}

if ($report.verdict -eq 'UNDECIDED') { $report.verdict = 'PASS' }
$report.finished_at = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
$report | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
Write-Host ""
Write-Host ("VERDICT: " + $report.verdict)
Write-Host "REPORT: $(Join-Path $OutDir 'report.json')"
if ($report.verdict -eq 'PASS') { exit 0 } else { exit 1 }
