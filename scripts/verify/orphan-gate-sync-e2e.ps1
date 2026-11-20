#Requires -Version 7.0
<#
.SYNOPSIS
  B-1 end-to-end verification: a residual (orphaned) IPC session must NOT keep the
  cloud-sync maintenance gate closed, so later sync rounds are no longer skipped.

.DESCRIPTION
  Drives the real, already running WeaselServer.exe (never starts/stops it):

    1. open the real account window: WM_COMMAND 40024 -> the server main window
       (WeaselIPCWindow_1.0); the tray menu handler re-posts it to
       m_server.GetHWnd()
    2. observable surface = the real "recent sync" control, id 1017
       (kIdSyncStatus); the "sync now" command is id 1016 (kIdSyncNow)
    3. start the synthetic --hold client: live session + typing => gate CLOSED
    4. trigger a sync round (WM_COMMAND 1016 on the account window) => the round
       must be SKIPPED by the maintenance gate
    5. kill the hold client (simulates a crashed TSF host) => session becomes orphan
    6. start the --trigger client => the server reclaims the orphan (ADR 0067)
    7. trigger a sync round again => the round must NOT be skipped any more

  Chinese literals are avoided on purpose (the script stays pure ASCII):
    * U+8DF3  ("skip") occurs only inside the gate skip message
    * U+65E0 U+6CD5 U+540C U+6B65 ("cannot sync") marks a not-logged-in account
    * a round result always carries a "YYYY-MM-DD HH:mm:ss" timestamp

  Exit codes:
    0  VERDICT=GATE_SYNC_E2E_OK
    4  VERDICT=GATE_SYNC_E2E_FAIL
    5  ABORT (environment unusable: no tool, no server, no diag log, ...)
#>

$ErrorActionPreference = 'Stop'

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Tool = Join-Path $script:RepoRoot 'release\TestOrphanReclaimE2E.exe'
$script:DiagLog = Join-Path $env:TEMP 'fluxing-profile-notify.log'
$script:OutRoot = Join-Path $script:RepoRoot 'reports\fluxing-bb-orphan-reclaim'

$script:OutDir = $null
$script:ConsoleFile = $null
$script:Account = [IntPtr]::Zero
$script:SyncNow = [IntPtr]::Zero
$script:SyncStatus = [IntPtr]::Zero

# control ids (CloudAccountUi.h) and messages
$script:IdStatus = 1006
$script:IdSyncNow = 1016
$script:IdSyncStatus = 1017
$script:MsgCommand = 0x0111
$script:BmClick = 0x00F5
$script:AccountClass = 'FluxingCloudAccountDialog_v1'

# Chinese markers, built from code points so the script file stays ASCII.
$script:SkipChar = [char]0x8DF3                       # skip
$script:NoSyncMarker = ([string][char]0x65E0) + [char]0x6CD5 + [char]0x540C + [char]0x6B65

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public static class FluxWin {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);

  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left, Top, Right, Bottom; }

  public static List<IntPtr> TopLevelOfPid(uint want) {
    var list = new List<IntPtr>();
    EnumWindows(delegate(IntPtr h, IntPtr l) {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == want) list.Add(h);
      return true;
    }, IntPtr.Zero);
    return list;
  }

  public static List<IntPtr> ChildrenOf(IntPtr parent) {
    var list = new List<IntPtr>();
    EnumChildWindows(parent, delegate(IntPtr h, IntPtr l) { list.Add(h); return true; }, IntPtr.Zero);
    return list;
  }

  public static string ClassOf(IntPtr h) {
    var sb = new StringBuilder(256);
    GetClassNameW(h, sb, sb.Capacity);
    return sb.ToString();
  }

  public static string TitleOf(IntPtr h) {
    var sb = new StringBuilder(512);
    GetWindowTextW(h, sb, sb.Capacity);
    return sb.ToString();
  }

  // Cross process control text: WM_GETTEXT is marshalled by the system for
  // controls owned by another process, GetWindowText is the fallback.
  public static string TextOf(IntPtr h) {
    var sb = new StringBuilder(2048);
    SendMessageW(h, 0x000D, (IntPtr)sb.Capacity, sb);
    if (sb.Length > 0) return sb.ToString();
    var sb2 = new StringBuilder(2048);
    GetWindowTextW(h, sb2, sb2.Capacity);
    return sb2.ToString();
  }

  public static string RectOf(IntPtr h) {
    RECT r;
    if (!GetWindowRect(h, out r)) return "?";
    return r.Left + "," + r.Top + "," + (r.Right - r.Left) + "x" + (r.Bottom - r.Top);
  }
}
'@

function Log {
  param([string]$Text)
  Write-Host $Text
  if ($script:ConsoleFile) {
    Add-Content -LiteralPath $script:ConsoleFile -Value $Text -Encoding utf8
  }
}

function Abort {
  param([string]$Reason, [int]$Code = 5)
  Log "ABORT=$Reason"
  Log 'VERDICT=GATE_SYNC_E2E_FAIL'
  Log "EXIT=$Code"
  exit $Code
}

function Fail {
  param([string]$Reason)
  Log "FAIL=$Reason"
  Log 'VERDICT=GATE_SYNC_E2E_FAIL'
  Log 'EXIT=4'
  exit 4
}

function Read-MarkerValue {
  param([string]$Path, [string]$Key)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  $raw = Get-Content -LiteralPath $Path -Raw -Encoding utf8
  if ($null -eq $raw) { return $null }
  $m = [regex]::Match($raw, [regex]::Escape($Key) + '=(\d+)')
  if (-not $m.Success) { return $null }
  return [long]$m.Groups[1].Value
}

function Invoke-Echo {
  param([long]$SessionId)
  $out = & $script:Tool --echo $SessionId 2>&1 | Out-String
  $code = $LASTEXITCODE
  $text = $out.Trim()
  $m = [regex]::Match($text, 'ECHO=(\d+)')
  if (-not $m.Success) {
    return [pscustomobject]@{ Raw = $text; Value = $null; Exit = $code }
  }
  return [pscustomobject]@{ Raw = $text; Value = [long]$m.Groups[1].Value; Exit = $code }
}

function Get-SyncStatusText {
  if ($script:SyncStatus -eq [IntPtr]::Zero) { return '' }
  return [FluxWin]::TextOf($script:SyncStatus)
}

function Trigger-SyncRound {
  # The account window handles WM_COMMAND with LOWORD(wParam) == kIdSyncNow.
  # Posting the command to the window works even when the button is disabled.
  [void][FluxWin]::PostMessageW($script:Account, [uint32]$script:MsgCommand,
                               [IntPtr]$script:IdSyncNow, [IntPtr]0)
  if ($script:SyncNow -ne [IntPtr]::Zero) {
    [void][FluxWin]::SendMessageW($script:SyncNow, [uint32]$script:BmClick,
                                  [IntPtr]::Zero, [IntPtr]::Zero)
  }
}

function Watch-SyncStatus {
  param([int]$Seconds)
  $lines = New-Object System.Collections.Generic.List[string]
  $end = (Get-Date).AddSeconds($Seconds)
  $last = ''
  while ((Get-Date) -lt $end) {
    $t = Get-SyncStatusText
    if ($t -ne $last) {
      $lines.Add(((Get-Date).ToString('HH:mm:ss.fff')) + ' | ' + $t)
      $last = $t
    }
    Start-Sleep -Milliseconds 250
  }
  return $lines
}

function Write-Timeline {
  param([string]$Name, $Lines)
  $file = Join-Path $script:OutDir $Name
  Set-Content -LiteralPath $file -Value ($Lines -join "`n") -Encoding utf8
  Log "TIMELINE_FILE=$file"
  foreach ($l in $Lines) { Log "  $l" }
}

function Get-Payloads {
  param($Lines)
  $out = New-Object System.Collections.Generic.List[string]
  foreach ($l in $Lines) {
    $idx = $l.IndexOf(' | ')
    if ($idx -ge 0) { $out.Add($l.Substring($idx + 3)) } else { $out.Add($l) }
  }
  return $out
}

# ---------------------------------------------------------------- setup ----
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:OutDir = Join-Path $script:OutRoot "gate-sync-$stamp"
New-Item -ItemType Directory -Force -Path $script:OutDir | Out-Null
$script:ConsoleFile = Join-Path $script:OutDir 'console.txt'

Log "REPO_ROOT=$script:RepoRoot"
Log "OUT_DIR=$script:OutDir"
Log "TOOL=$script:Tool"
Log "DIAG_LOG=$script:DiagLog"

if (-not (Test-Path -LiteralPath $script:Tool)) {
  Abort "TOOL_MISSING (build test/TestOrphanReclaimE2E first)" 5
}

$serverProcs = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
if ($serverProcs.Count -eq 0) {
  Abort 'SERVER_NOT_RUNNING (start the real WeaselServer first)' 5
}
foreach ($sp in $serverProcs) {
  Log "SERVER pid=$($sp.Id) path=$($sp.Path) ver=$($sp.MainModule.FileVersionInfo.FileVersion)"
}
$serverPid = [uint32]$serverProcs[0].Id

if (-not (Test-Path -LiteralPath $script:DiagLog)) {
  Abort "DIAG_LOG_MISSING ($script:DiagLog)" 5
}
$baseline = (Get-Item -LiteralPath $script:DiagLog).Length
Log "LOG_BASELINE_BYTES=$baseline"

# --------------------------------------------------- window inventory ------
$wins = [FluxWin]::TopLevelOfPid($serverPid)
Log "TOP_LEVEL_WINDOWS=$($wins.Count)"
foreach ($w in $wins) {
  Log ("  WIN hwnd=0x{0:X} class='{1}' visible={2} rect={3} title='{4}'" -f
       $w.ToInt64(), [FluxWin]::ClassOf($w), [FluxWin]::IsWindowVisible($w),
       [FluxWin]::RectOf($w), [FluxWin]::TitleOf($w))
}

# ------------------------------------------------- open account window -----
$account = $null
foreach ($w in $wins) {
  if ([FluxWin]::ClassOf($w) -eq $script:AccountClass) { $account = $w; break }
}
if ($null -eq $account) {
  Log 'ACCOUNT_WINDOW_PREEXISTING=0 (posting WM_COMMAND 40024 to every candidate window)'
  foreach ($w in $wins) {
    if ([FluxWin]::ClassOf($w) -eq $script:AccountClass) { continue }
    [void][FluxWin]::PostMessageW($w, [uint32]$script:MsgCommand, [IntPtr]40024, [IntPtr]0)
    Start-Sleep -Milliseconds 1200
    foreach ($c in [FluxWin]::TopLevelOfPid($serverPid)) {
      if ([FluxWin]::ClassOf($c) -eq $script:AccountClass) { $account = $c; break }
    }
    if ($null -ne $account) {
      Log "ACCOUNT_WINDOW_OPENED_BY_CLASS='$([FluxWin]::ClassOf($w))'"
      break
    }
  }
} else {
  Log 'ACCOUNT_WINDOW_PREEXISTING=1'
}
if ($null -eq $account) {
  Abort 'ACCOUNT_WINDOW_NOT_OPEN (WM_COMMAND 40024 did not create the account window)' 5
}
$script:Account = $account
Log ("ACCOUNT_HWND=0x{0:X} visible={1} rect={2} title='{3}'" -f $account.ToInt64(),
     [FluxWin]::IsWindowVisible($account), [FluxWin]::RectOf($account),
     [FluxWin]::TitleOf($account))

$script:SyncNow = [FluxWin]::GetDlgItem($account, $script:IdSyncNow)
$script:SyncStatus = [FluxWin]::GetDlgItem($account, $script:IdSyncStatus)
Log ("SYNC_NOW_HWND=0x{0:X} visible={1} enabled={2}" -f $script:SyncNow.ToInt64(),
   [FluxWin]::IsWindowVisible($script:SyncNow), [FluxWin]::IsWindowEnabled($script:SyncNow))
Log ("SYNC_STATUS_HWND=0x{0:X} visible={1} enabled={2}" -f $script:SyncStatus.ToInt64(),
   [FluxWin]::IsWindowVisible($script:SyncStatus), [FluxWin]::IsWindowEnabled($script:SyncStatus))
$statusCtrl = [FluxWin]::GetDlgItem($account, $script:IdStatus)
Log "ACCOUNT_STATUS(id $($script:IdStatus))=$([FluxWin]::TextOf($statusCtrl))"
if ($script:SyncNow -eq [IntPtr]::Zero) {
  Abort "SYNC_NOW_BUTTON_MISSING (id $($script:IdSyncNow) not found)" 5
}
if ($script:SyncStatus -eq [IntPtr]::Zero) {
  Abort "SYNC_STATUS_CONTROL_MISSING (id $($script:IdSyncStatus) not found)" 5
}

# child control inventory (ids / visible / enabled / text)
$childLines = New-Object System.Collections.Generic.List[string]
foreach ($c in [FluxWin]::ChildrenOf($account)) {
  $id = [FluxWin]::GetDlgCtrlID($c)
  $txt = [FluxWin]::TextOf($c)
  $childLines.Add("id=$id class='$([FluxWin]::ClassOf($c))' visible=$([FluxWin]::IsWindowVisible($c)) enabled=$([FluxWin]::IsWindowEnabled($c)) text='$txt'")
}
$childFile = Join-Path $script:OutDir 'account-children.txt'
Set-Content -LiteralPath $childFile -Value ($childLines -join "`n") -Encoding utf8
Log "CHILDREN_FILE=$childFile count=$($childLines.Count)"
foreach ($l in $childLines) { Log "  CHILD $l" }

$baselineText = Get-SyncStatusText
Log "BASELINE_SYNC_STATUS=$baselineText"

# ------------------------------------------------------------- phase A -----
# Live session (synthetic host) => the maintenance gate is closed => the round
# must be skipped. This is the "before" half of B-1.
$holdMarker = Join-Path $script:OutDir 'hold.marker'
$holdStdout = Join-Path $script:OutDir 'hold.stdout.txt'
$holdStderr = Join-Path $script:OutDir 'hold.stderr.txt'

$hold = Start-Process -FilePath $script:Tool `
  -ArgumentList @('--hold', $holdMarker) `
  -PassThru -WindowStyle Hidden `
  -RedirectStandardOutput $holdStdout -RedirectStandardError $holdStderr
Log "HOLD_STARTED pid=$($hold.Id)"

$deadline = (Get-Date).AddSeconds(30)
$orphanId = $null
while ((Get-Date) -lt $deadline) {
  if ($hold.HasExited) {
    Log "HOLD_STDOUT=$(Get-Content -LiteralPath $holdStdout -Raw -ErrorAction SilentlyContinue)"
    Log "HOLD_STDERR=$(Get-Content -LiteralPath $holdStderr -Raw -ErrorAction SilentlyContinue)"
    Fail "HOLD_EXITED_EARLY exit=$($hold.ExitCode)"
  }
  $orphanId = Read-MarkerValue -Path $holdMarker -Key 'ipc_id'
  if ($null -ne $orphanId) { break }
  Start-Sleep -Milliseconds 100
}
if ($null -eq $orphanId) {
  Fail 'HOLD_MARKER_TIMEOUT (no ipc_id in hold.marker within 30 s)'
}
$orphanPid = Read-MarkerValue -Path $holdMarker -Key 'pid'
Log "ORPHAN_IPC_ID=$orphanId"
Log "ORPHAN_PID=$orphanPid"

$echoBefore = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoBefore.Value) { Fail "ECHO_BEFORE_UNPARSED raw='$($echoBefore.Raw)'" }
if ($echoBefore.Value -eq 0) { Fail 'ECHO_BEFORE_ZERO (held session was already gone)' }
Log "ECHO_BEFORE=$($echoBefore.Value) (non zero => session really alive)"

Trigger-SyncRound
Log 'SYNC_ROUND_TRIGGERED_A=1'
$timelineA = Watch-SyncStatus -Seconds 40
Write-Timeline -Name 'timeline-A-gate-closed.txt' -Lines $timelineA

# ------------------------------------------------------------- phase B -----
# Kill the host => the session becomes an orphan. Reclaim it, then the very same
# round must NOT be skipped any more.
Stop-Process -Id $hold.Id -Force
try { Wait-Process -Id $hold.Id -Timeout 15 -ErrorAction SilentlyContinue } catch { }
Start-Sleep -Seconds 1
if ($null -ne (Get-Process -Id $hold.Id -ErrorAction SilentlyContinue)) {
  Fail 'HOLD_STILL_ALIVE (could not kill the hold process)'
}
Log 'HOLD_KILLED=1'

$triggerMarker = Join-Path $script:OutDir 'trigger.marker'
$triggerStdout = Join-Path $script:OutDir 'trigger.stdout.txt'
$triggerStderr = Join-Path $script:OutDir 'trigger.stderr.txt'
$trigger = Start-Process -FilePath $script:Tool `
  -ArgumentList @('--trigger', $triggerMarker) `
  -PassThru -WindowStyle Hidden `
  -RedirectStandardOutput $triggerStdout -RedirectStandardError $triggerStderr
Log "TRIGGER_STARTED pid=$($trigger.Id)"
$null = $trigger.WaitForExit(30000)
if (-not $trigger.HasExited) {
  Stop-Process -Id $trigger.Id -Force -ErrorAction SilentlyContinue
  Fail 'TRIGGER_TIMEOUT (trigger did not exit within 30 s)'
}
Log "TRIGGER_EXIT=$($trigger.ExitCode)"
$triggerId = Read-MarkerValue -Path $triggerMarker -Key 'ipc_id'
Log "TRIGGER_IPC_ID=$triggerId"
Start-Sleep -Seconds 2

$echoAfter = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoAfter.Value) { Fail "ECHO_AFTER_UNPARSED raw='$($echoAfter.Raw)'" }
if ($echoAfter.Value -ne 0) {
  Fail "ECHO_AFTER_NOT_ZERO value=$($echoAfter.Value) (orphan session was not reclaimed)"
}
Log 'ECHO_AFTER=0 (orphan session is gone on the server)'

Trigger-SyncRound
Log 'SYNC_ROUND_TRIGGERED_B=1'
$timelineB = Watch-SyncStatus -Seconds 45
Write-Timeline -Name 'timeline-B-after-reclaim.txt' -Lines $timelineB

# ------------------------------------------------------- diag log slice ----
$logItem = Get-Item -LiteralPath $script:DiagLog
if ($logItem.Length -lt $baseline) {
  Abort 'DIAG_LOG_ROTATED (log shrank, the baseline offset is invalid)' 5
}
$sliceBytes = New-Object byte[] ($logItem.Length - $baseline)
$fs = [System.IO.File]::Open($script:DiagLog, [System.IO.FileMode]::Open,
                              [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
try {
  $null = $fs.Seek($baseline, [System.IO.SeekOrigin]::Begin)
  $read = $fs.Read($sliceBytes, 0, $sliceBytes.Length)
} finally {
  $fs.Close()
}
if ($read -lt $sliceBytes.Length) { $sliceBytes = $sliceBytes[0..($read - 1)] }
$slice = [System.Text.Encoding]::UTF8.GetString($sliceBytes)
Set-Content -LiteralPath (Join-Path $script:OutDir 'log-slice.txt') -Value $slice -Encoding utf8
Log "LOG_SLICE_BYTES=$($sliceBytes.Length)"

$reclaimPattern = 'reclaim orphan session ipc_id=' + $orphanId +
                   ' client_pid=' + $orphanPid + ' live=\d+'
$reclaimHits = [regex]::Matches($slice, $reclaimPattern)
$keptHits = [regex]::Matches($slice, 'remove_session active orphan_reclaim presence=kept')
$absentHits = [regex]::Matches($slice, 'remove_session active presence=absent')
Log "CRITERION_B2_RECLAIM_LINE count=$($reclaimHits.Count) pattern='$reclaimPattern'"
Log "CRITERION_B3_PRESENCE_KEPT count=$($keptHits.Count)"
Log "CRITERION_B3_NO_ABSENT_FLIP count=$($absentHits.Count) (must be 0)"
foreach ($h in $reclaimHits) { Log "RECLAIM_LINE=$($h.Value)" }

# ------------------------------------------------------------- criteria -----
$payloadsA = Get-Payloads -Lines $timelineA
$payloadsB = Get-Payloads -Lines $timelineB
$skipA = @($payloadsA | Where-Object { $_.Contains($script:SkipChar) })
$skipB = @($payloadsB | Where-Object { $_.Contains($script:SkipChar) })
$roundB = @($payloadsB | Where-Object { $_ -match '\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}' })
$noSync = @(($payloadsA + $payloadsB) | Where-Object { $_.Contains($script:NoSyncMarker) })

Log "B1A_SKIP_LINES count=$($skipA.Count)"
Log "B1B_SKIP_LINES count=$($skipB.Count) (must be 0)"
Log "B1B_ROUND_LINES count=$($roundB.Count) (a real round result must show up)"
Log "LOGIN_PROBLEM_LINES count=$($noSync.Count)"

if ($noSync.Count -gt 0) {
  foreach ($l in $noSync) { Log "  NOSYNC=$l" }
  Abort 'NOT_LOGGED_IN (the account window reports that it cannot sync at all)' 5
}
if ($skipA.Count -lt 1) {
  Fail 'B1A_FAILED (the round was not skipped while the synthetic host was alive; no skip text in the timeline)'
}
if ($skipB.Count -ne 0) {
  foreach ($l in $skipB) { Log "  SKIP_B=$l" }
  Fail 'B1B_FAILED (the round was still skipped after the orphan session was reclaimed)'
}
if ($roundB.Count -lt 1) {
  Fail 'B1B_FAILED (no round result at all after the reclaim; the sync round did not run)'
}
if ($reclaimHits.Count -lt 1) {
  Fail 'B2_FAILED (the reclaim log line with client_pid/live is missing)'
}
if ($keptHits.Count -lt 1) {
  Fail 'B3_FAILED (no "remove_session active orphan_reclaim presence=kept" in the log slice)'
}
if ($absentHits.Count -ne 0) {
  Fail 'B3_FAILED ("remove_session active presence=absent" appeared, presence was flipped)'
}

Log 'VERDICT=GATE_SYNC_E2E_OK'
Log 'EXIT=0'
exit 0
