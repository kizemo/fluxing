#Requires -Version 5.1
<#
.SYNOPSIS
    Fluxing batch BK probe - is the "last sync" status line observable on the
    account page after the "sync now" button is pressed?

.DESCRIPTION
    Product facts (established upstream, NOT re-derived by this probe):
      * kIdSyncStatus = 1017 is the STATIC "last sync" line of the account page
        (WeaselServer/Cloud/CloudAccountUi.h:53).
      * kIdSyncNow   = 1016 is the "sync now" button (same header, line 52).
      * Before batch BK, RefreshUi() never wrote 1017, so the control kept the
        creation-time placeholder forever. After BK, RefreshUi() writes
        CloudAccountController::sync_status() into it
        (WeaselServer/Cloud/CloudAccountDialog.cpp:1489-1491).

    What this probe does (outside-in evidence):
      1. find the account window by CONTROL ID: a top level window of the
         running WeaselServer.exe process that owns a child control with
         GetDlgCtrlID == 1017. No Chinese title / text matching anywhere.
      2. read 1017 with WM_GETTEXT (cross-process safe)            -> BEFORE
      3. send BM_CLICK to the child control with GetDlgCtrlID == 1016
      4. poll 1017 every 2 s (max 90 s) until its text differs      -> AFTER
      5. judge AFTER: it must carry a timestamp AND a result keyword
         (success / failed / skipped) and must not be the placeholder.

    PURE ASCII SOURCE ON PURPOSE: PowerShell 5.1 reads BOM-less .ps1 files as
    ANSI, so any Chinese literal inside the file would be mangled. Every
    Chinese literal is therefore compared by UNICODE CODE POINT (CP_* below).

    Hard rules honoured by this probe:
      * never logs out, never kills a process, never restarts the service,
        never touches the registry, never writes product code.
      * it only opens the account window (when it is not open already) and
        presses the "sync now" button; it closes the window again only when
        the probe itself opened it (and -KeepOpen was not given).

    Machine readable output (ASCII keys, one per line):
      ACCOUNT_WINDOW_FOUND= / SYNC_STATUS_CONTROL_FOUND= / SYNC_BTN_CONTROL_FOUND=
      SYNC_STATUS_BEFORE= / SYNC_STATUS_AFTER= / SYNC_STATUS_CHANGED=
      SYNC_STATUS_HAS_TIMESTAMP= / SYNC_STATUS_HAS_RESULT= / VERDICT= / EXIT=

    Exit code: 0 = PASS (VERDICT=SYNC_STATUS_OBSERVABLE)
               4 = FAIL (VERDICT=SYNC_STATUS_NOT_OBSERVABLE)

.EXAMPLE
    pwsh -NoProfile -File reports\fluxing-bk-syncstatus\probe-sync-status.ps1
#>
[CmdletBinding()]
param(
    # Total seconds to wait for the 1017 text to change after the click.
    [int]$WaitSeconds = 90,
    # Poll period in seconds.
    [int]$PollSeconds = 2,
    # Seconds to wait for the account window to appear after auto-open.
    [int]$OpenTimeoutSec = 15,
    # Where the run log is written (defaults to this script's own folder).
    [string]$OutDir = '',
    # Leave the account window open even if this probe opened it.
    [switch]$KeepOpen
)

$ErrorActionPreference = 'Stop'
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }

if (-not ('FluxingSyncProbe' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class FluxingSyncProbe {
    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    private static extern IntPtr SendMsgText(IntPtr h, uint m, IntPtr w, StringBuilder l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    private static extern IntPtr SendMsgRaw(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll", EntryPoint = "GetWindowLongW")] public static extern int GetWindowLongW(IntPtr h, int i);
    [DllImport("user32.dll", SetLastError = true)] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetThreadDesktop(uint tid);
    [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool GetUserObjectInformationW(IntPtr h, int i, IntPtr buf, int n, out int need);

    public const int GWL_STYLE = -16;
    public const int WS_VISIBLE = 0x10000000;

    public static IntPtr[] TopOf(uint pid) {
        var list = new System.Collections.Generic.List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr p) {
            uint q; GetWindowThreadProcessId(h, out q);
            if (q == pid) list.Add(h);
            return true;
        }, IntPtr.Zero);
        return list.ToArray();
    }

    public static IntPtr[] ChildrenOf(IntPtr parent) {
        var list = new System.Collections.Generic.List<IntPtr>();
        EnumChildWindows(parent, delegate(IntPtr h, IntPtr p) { list.Add(h); return true; }, IntPtr.Zero);
        return list.ToArray();
    }

    public static string ClassOf(IntPtr h) {
        var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString();
    }

    public static string CaptionOf(IntPtr h) {
        var sb = new StringBuilder(1024); GetWindowTextW(h, sb, 1024); return sb.ToString();
    }

    // WM_GETTEXT through SendMessage: USER32 marshals the buffer across the
    // process boundary, so this works for a control of another process.
    public static string MsgTextOf(IntPtr h) {
        try {
            var sb = new StringBuilder(2048);
            SendMsgText(h, 0x000D, (IntPtr)sb.Capacity, sb);
            return sb.ToString();
        } catch { return ""; }
    }

    // Own WS_VISIBLE bit of the control itself (IsWindowVisible would also
    // fold in the visibility of every ancestor window).
    public static bool OwnVisible(IntPtr h) {
        return (GetWindowLongW(h, GWL_STYLE) & WS_VISIBLE) != 0;
    }

    // BM_CLICK from a worker thread with a hard timeout: a SendMessage that
    // never returns would hang the whole probe.
    public static bool ClickWithTimeout(IntPtr h, uint msg, int timeoutMs) {
        try {
            var t = System.Threading.Tasks.Task.Factory.StartNew(delegate() {
                SendMsgRaw(h, msg, IntPtr.Zero, IntPtr.Zero);
            });
            return t.Wait(timeoutMs);
        } catch { return false; }
    }

    public static string DesktopName() {
        uint tid = GetCurrentThreadId();
        IntPtr h = GetThreadDesktop(tid);
        if (h == IntPtr.Zero) { return "<null>"; }
        IntPtr buf = Marshal.AllocHGlobal(1024);
        try {
            int need = 0;
            if (!GetUserObjectInformationW(h, 2, buf, 1024, out need)) { return "<query-failed>"; }
            return Marshal.PtrToStringUni(buf);
        } finally { Marshal.FreeHGlobal(buf); }
    }
}
'@
}

# ---------------------------------------------------------------------------
# constants
# ---------------------------------------------------------------------------
$WM_COMMAND = 0x0111
$WM_CLOSE = 0x0010
$WM_GETTEXT = 0x000D
$BM_CLICK = 0x00F5

$ID_MENU_CLOUD_ACCOUNT = 40024      # WeaselServer/resource.h:71 ID_WEASELTRAY_CLOUD_ACCOUNT
$ID_CLS_TRAY = 'TrayIconClass'

$KID_SYNC_NOW = 1016                # WeaselServer/Cloud/CloudAccountUi.h:52 kIdSyncNow
$KID_SYNC_STATUS = 1017             # WeaselServer/Cloud/CloudAccountUi.h:53 kIdSyncStatus
$KID_STATUS = 1006                  # WeaselServer/Cloud/CloudAccountUi.h kIdStatus (general status line)

# Chinese literals as UNICODE CODE POINTS, space separated hex, no non-ASCII
# byte anywhere in this file.
$CP_PLACEHOLDER = '6700 8FD1 540C 6B65 FF1A 5C1A 672A 540C 6B65 3002'  # "last sync: never synced."
$CP_SUCCESS = '6210 529F'            # success
$CP_FAILED = '5931 8D25'             # failed
$CP_SKIPPED = '5DF2 8DF3 8FC7'       # skipped
$RE_TIMESTAMP = '\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}'

# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
$script:lines = New-Object 'System.Collections.Generic.List[string]'
function L($s) {
    $script:lines.Add([string]$s)
    Write-Host $s
}

# code point key of a string: "XXXX XXXX ..." (ASCII, comparable as text)
function Cp-Key([string]$s) {
    if ([string]::IsNullOrEmpty($s)) { return '' }
    $parts = @()
    foreach ($ch in $s.ToCharArray()) { $parts += ('{0:X4}' -f [int][char]$ch) }
    return ($parts -join ' ')
}

# does the code point key contain the code point sequence (contiguous)?
function Test-CpSeq([string]$key, [string]$needle) {
    if ([string]::IsNullOrEmpty($key) -or [string]::IsNullOrEmpty($needle)) { return $false }
    return $key.Contains($needle)
}

function Find-ChildById([IntPtr]$parent, [int]$ctrlId) {
    foreach ($c in [FluxingSyncProbe]::ChildrenOf($parent)) {
        if ([FluxingSyncProbe]::GetDlgCtrlID($c) -eq $ctrlId) { return $c }
    }
    return [IntPtr]::Zero
}

# The account window is identified by CONTROL ID, never by its title:
# a top level window of the server process that owns a child with id 1017.
function Find-AccountWindow([uint32]$procId) {
    foreach ($h in [FluxingSyncProbe]::TopOf($procId)) {
        foreach ($c in [FluxingSyncProbe]::ChildrenOf($h)) {
            if ([FluxingSyncProbe]::GetDlgCtrlID($c) -eq $KID_SYNC_STATUS) { return $h }
        }
    }
    return [IntPtr]::Zero
}

# Read a control text: WM_GETTEXT first (works for any control of another
# process), GetWindowTextW as fallback (works for a STATIC whose text is the
# window caption).
function Read-CtrlText([IntPtr]$h) {
    if ($h -eq [IntPtr]::Zero) { return '' }
    $t = [FluxingSyncProbe]::MsgTextOf($h)
    if ([string]::IsNullOrEmpty($t)) { $t = [FluxingSyncProbe]::CaptionOf($h) }
    return $t
}

# ---------------------------------------------------------------------------
# facts collected during the run, printed in a fixed order at the end
# ---------------------------------------------------------------------------
$F = [ordered]@{
    ts                      = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
    desktop                 = ''
    srv_pid                 = 0
    srv_ver                 = 'unknown'
    account_window_found    = $false
    account_window_hwnd     = '0'
    account_window_class    = ''
    account_window_visible  = $false
    opened_by_probe         = $false
    status_ctl_found        = $false
    status_ctl_class        = ''
    status_ctl_visible      = $false
    btn_ctl_found           = $false
    btn_ctl_class           = ''
    btn_ctl_visible         = $false
    status_line_found       = $false
    before                  = ''
    before_cp              = ''
    click_sent              = $false
    after                   = ''
    after_cp                = ''
    changed                 = $false
    has_timestamp           = $false
    has_result              = $false
    after_is_placeholder    = $false
    result_kind             = 'NONE'
    status_line_before      = ''
    status_line_after       = ''
    status_line_changed     = $false
    poll_seconds            = 0
    reason                  = 'NOT_RUN'
}

$exitCode = 4
$verdict = 'SYNC_STATUS_NOT_OBSERVABLE'
$script:dlg = [IntPtr]::Zero

try {
    # desktop guard: windows of a virtual desktop are not the ones the user
    # sees; EnumWindows only walks the desktop of the calling thread.
    $F['desktop'] = [FluxingSyncProbe]::DesktopName()
    try { [void][FluxingSyncProbe]::SetProcessDpiAwarenessContext([IntPtr](-4)) } catch { }

    if ($F['desktop'] -ne 'Default') {
        $F['reason'] = 'NOT_DEFAULT_DESKTOP'
    }
    else {
        $procs = @(Get-Process WeaselServer -ErrorAction SilentlyContinue)
        if ($procs.Count -eq 0) {
            $F['reason'] = 'NO_WEASEL_SERVER'
        }
        elseif ($procs.Count -gt 1) {
            $F['reason'] = 'MULTIPLE_WEASEL_SERVER'
        }
        else {
            $srv = $procs[0]
            $F['srv_pid'] = [int]$srv.Id
            try { $F['srv_ver'] = [string]$srv.MainModule.FileVersionInfo.FileVersion } catch { }

            $dlg = Find-AccountWindow ([uint32]$srv.Id)
            if ($dlg -eq [IntPtr]::Zero) {
                # open it the proven way: WM_COMMAND(40024) to the server's own
                # top level windows (the tray shell window only owns the icon).
                foreach ($h in [FluxingSyncProbe]::TopOf([uint32]$srv.Id)) {
                    if ([FluxingSyncProbe]::ClassOf($h) -ne $ID_CLS_TRAY) {
                        [void][FluxingSyncProbe]::PostMessageW($h, $WM_COMMAND, [IntPtr]$ID_MENU_CLOUD_ACCOUNT, [IntPtr]::Zero)
                    }
                }
                $F['opened_by_probe'] = $true
                $waited = 0
                while ($waited -lt ($OpenTimeoutSec * 10) -and $dlg -eq [IntPtr]::Zero) {
                    Start-Sleep -Milliseconds 100
                    $waited++
                    $dlg = Find-AccountWindow ([uint32]$srv.Id)
                }
            }

            if ($dlg -eq [IntPtr]::Zero) {
                $F['reason'] = 'ACCOUNT_WINDOW_NOT_FOUND'
            }
            else {
                $script:dlg = $dlg
                $F['account_window_found'] = $true
                $F['account_window_hwnd'] = [string]$dlg.ToInt64()
                $F['account_window_class'] = [FluxingSyncProbe]::ClassOf($dlg)
                $F['account_window_visible'] = [bool][FluxingSyncProbe]::IsWindowVisible($dlg)

                $statusCtl = Find-ChildById $dlg $KID_SYNC_STATUS
                $btnCtl = Find-ChildById $dlg $KID_SYNC_NOW
                $lineCtl = Find-ChildById $dlg $KID_STATUS

                if ($statusCtl -ne [IntPtr]::Zero) {
                    $F['status_ctl_found'] = $true
                    $F['status_ctl_class'] = [FluxingSyncProbe]::ClassOf($statusCtl)
                    $F['status_ctl_visible'] = [bool][FluxingSyncProbe]::OwnVisible($statusCtl)
                }
                if ($btnCtl -ne [IntPtr]::Zero) {
                    $F['btn_ctl_found'] = $true
                    $F['btn_ctl_class'] = [FluxingSyncProbe]::ClassOf($btnCtl)
                    $F['btn_ctl_visible'] = [bool][FluxingSyncProbe]::OwnVisible($btnCtl)
                }
                if ($lineCtl -ne [IntPtr]::Zero) { $F['status_line_found'] = $true }

                if (-not $F['status_ctl_found']) {
                    $F['reason'] = 'SYNC_STATUS_CONTROL_1017_NOT_FOUND'
                }
                elseif (-not $F['btn_ctl_found']) {
                    $F['reason'] = 'SYNC_BUTTON_1016_NOT_FOUND'
                }
                elseif (-not $F['btn_ctl_visible']) {
                    # 1016 is only part of the logged-in layout table
                    # (CloudAccountUi.h:359) => hidden means "not logged in".
                    $F['reason'] = 'SYNC_BUTTON_1016_HIDDEN_NOT_LOGGED_IN'
                }
                else {
                    $F['before'] = Read-CtrlText $statusCtl
                    $F['before_cp'] = Cp-Key $F['before']
                    if ($lineCtl -ne [IntPtr]::Zero) { $F['status_line_before'] = Read-CtrlText $lineCtl }

                    $F['click_sent'] = [bool][FluxingSyncProbe]::ClickWithTimeout($btnCtl, $BM_CLICK, 15000)

                    $poll = 0
                    while ($poll -lt $WaitSeconds) {
                        Start-Sleep -Seconds $PollSeconds
                        $poll += $PollSeconds
                        $F['poll_seconds'] = $poll
                        if (-not [FluxingSyncProbe]::IsWindow($dlg)) {
                            $F['reason'] = 'ACCOUNT_WINDOW_CLOSED_DURING_WAIT'
                            break
                        }
                        $ctl2 = Find-ChildById $dlg $KID_SYNC_STATUS
                        if ($ctl2 -eq [IntPtr]::Zero) { continue }
                        $t = Read-CtrlText $ctl2
                        if ($t -ne $F['before']) {
                            $F['after'] = $t
                            $F['changed'] = $true
                            break
                        }
                    }

                    if ($F['changed']) {
                        $F['after_cp'] = Cp-Key $F['after']
                        $F['has_timestamp'] = [bool][regex]::IsMatch([string]$F['after'], $RE_TIMESTAMP)
                        $F['after_is_placeholder'] = Test-CpSeq $F['after_cp'] $CP_PLACEHOLDER
                        if (Test-CpSeq $F['after_cp'] $CP_SUCCESS) { $F['result_kind'] = 'SUCCESS'; $F['has_result'] = $true }
                        elseif (Test-CpSeq $F['after_cp'] $CP_FAILED) { $F['result_kind'] = 'FAILED'; $F['has_result'] = $true }
                        elseif (Test-CpSeq $F['after_cp'] $CP_SKIPPED) { $F['result_kind'] = 'SKIPPED'; $F['has_result'] = $true }
                        else { $F['result_kind'] = 'NONE'; $F['has_result'] = $false }

                        if ($lineCtl -ne [IntPtr]::Zero) {
                            $F['status_line_after'] = Read-CtrlText $lineCtl
                            $F['status_line_changed'] = ($F['status_line_after'] -ne $F['status_line_before'])
                        }

                        if ($F['has_timestamp'] -and $F['has_result'] -and (-not $F['after_is_placeholder'])) {
                            $verdict = 'SYNC_STATUS_OBSERVABLE'
                            $exitCode = 0
                            $F['reason'] = 'OK'
                        }
                        else {
                            $verdict = 'SYNC_STATUS_NOT_OBSERVABLE'
                            $exitCode = 4
                            if (-not $F['has_timestamp']) { $F['reason'] = 'AFTER_TEXT_WITHOUT_TIMESTAMP' }
                            elseif (-not $F['has_result']) { $F['reason'] = 'AFTER_TEXT_WITHOUT_RESULT_KEYWORD' }
                            else { $F['reason'] = 'AFTER_TEXT_IS_PLACEHOLDER' }
                        }
                    }
                    else {
                        $verdict = 'SYNC_STATUS_NOT_OBSERVABLE'
                        $exitCode = 4
                        if ($F['reason'] -eq 'NOT_RUN') { $F['reason'] = 'TIMEOUT_STATUS_TEXT_UNCHANGED' }
                        if ($lineCtl -ne [IntPtr]::Zero) {
                            $F['status_line_after'] = Read-CtrlText $lineCtl
                            $F['status_line_changed'] = ($F['status_line_after'] -ne $F['status_line_before'])
                        }
                    }
                }
            }

            # tidy up: close the account window only when this probe opened it.
            # Isolated on purpose: a failure here must never change the verdict.
            try {
                if ($F['opened_by_probe'] -and -not $KeepOpen.IsPresent -and $script:dlg -ne [IntPtr]::Zero) {
                    [void][FluxingSyncProbe]::PostMessageW($script:dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
                    Start-Sleep -Milliseconds 400
                }
            } catch { }
        }
    }
}
catch {
    $F['reason'] = 'EXCEPTION:' + $_.Exception.GetType().Name
    $verdict = 'SYNC_STATUS_NOT_OBSERVABLE'
    $exitCode = 4
}

# ---------------------------------------------------------------------------
# machine readable output (ASCII keys, one per line)
# ---------------------------------------------------------------------------
L "TS=$($F['ts'])"
L "DESKTOP=$($F['desktop'])"
L "SRV_PID=$($F['srv_pid'])"
L "VER=$($F['srv_ver'])"
L "ACCOUNT_WINDOW_FOUND=$($F['account_window_found'])"
L "ACCOUNT_WINDOW_HWND=$($F['account_window_hwnd'])"
L "ACCOUNT_WINDOW_CLASS=$($F['account_window_class'])"
L "ACCOUNT_WINDOW_VISIBLE=$($F['account_window_visible'])"
L "ACCOUNT_WINDOW_OPENED_BY_PROBE=$($F['opened_by_probe'])"
L "SYNC_STATUS_CONTROL_FOUND=$($F['status_ctl_found'])"
L "SYNC_STATUS_CONTROL_CLASS=$($F['status_ctl_class'])"
L "SYNC_STATUS_CONTROL_VISIBLE=$($F['status_ctl_visible'])"
L "SYNC_BTN_CONTROL_FOUND=$($F['btn_ctl_found'])"
L "SYNC_BTN_CONTROL_CLASS=$($F['btn_ctl_class'])"
L "SYNC_BTN_CONTROL_VISIBLE=$($F['btn_ctl_visible'])"
L "STATUS_LINE_1006_FOUND=$($F['status_line_found'])"
L "SYNC_STATUS_BEFORE=$($F['before'])"
L "SYNC_STATUS_BEFORE_CP=$($F['before_cp'])"
L "SYNC_CLICK_SENT=$($F['click_sent'])"
L "SYNC_STATUS_AFTER=$($F['after'])"
L "SYNC_STATUS_AFTER_CP=$($F['after_cp'])"
L "SYNC_STATUS_CHANGED=$($F['changed'])"
L "SYNC_STATUS_HAS_TIMESTAMP=$($F['has_timestamp'])"
L "SYNC_STATUS_HAS_RESULT=$($F['has_result'])"
L "SYNC_STATUS_AFTER_IS_PLACEHOLDER=$($F['after_is_placeholder'])"
L "SYNC_STATUS_RESULT_KIND=$($F['result_kind'])"
L "STATUS_LINE_1006_CHANGED=$($F['status_line_changed'])"
L "POLL_SECONDS=$($F['poll_seconds'])"
L "REASON=$($F['reason'])"
L "VERDICT=$verdict"
L "EXIT=$exitCode"

# ---------------------------------------------------------------------------
# run log (same folder as this script by default)
# ---------------------------------------------------------------------------
if ([string]::IsNullOrEmpty($OutDir)) { $OutDir = $PSScriptRoot }
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$dst = Join-Path $OutDir ('sync-status-' + (Get-Date).ToString('yyyyMMdd-HHmmss') + '.txt')
Set-Content -Path $dst -Value ($script:lines -join "`r`n") -Encoding UTF8
L "WROTE=$dst"

exit $exitCode
