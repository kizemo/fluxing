#Requires -Version 7.0
<#
.SYNOPSIS
  B-1 end-to-end verification: an orphaned IPC session must not keep the
  cloud-sync maintenance gate closed (ADR 0067 reclaim releases the gate).

.DESCRIPTION
  Observation surface = the mtime of the export file
      <RimeUserDir>\sync\<account>\rime_ice.userdb.txt

  Why not the account window text: control id 1017 (kIdSyncStatus) is filled
  from the static WidgetText() literal and RefreshUi() never rewrites it
  (WeaselServer/Cloud/CloudAccountDialog.cpp:1299-1352) => reading it can
  only ever return the initial string "recent sync: not synced yet".
  The real round result lives in Impl::sync_status_text, which no visible
  control shows.  The project criterion for "the gate let the round through"
  is the export file mtime, never a status text
  (docs/release/restart-checklist-1.3.6.md, section 4: "a status text must
  not be used as the criterion").

  Phases (all against the real, already running WeaselServer.exe; nothing is
  started, stopped or installed):

    C   control    no synthetic session at all
                   => the round must NOT be skipped => export mtime MUST
                      advance.  Positive control: without it the A/B result
                      would be meaningless.
    A2  residual   synthetic host alive, typing paused (idle > 10 s) and an
                   unfinished composition still held
                   => the gate is still closed => export mtime must NOT
                      advance.  This is the "before" half of B-1.
    B   reclaimed  the same residual session was reclaimed (ADR 0067)
                   => the round must NOT be skipped any more => export mtime
                      MUST advance.  This is the "after" half of B-1.

  A2 vs B differ in exactly one variable (the reclaim) => the pair is the
  B-1 proof.  If A2 advances as well, the synthetic session never held the
  gate (no composition registered) => the verdict is INCONCLUSIVE, never a
  pass.

  Gate facts (include/FluxingMaintenanceGate.h, ADR 0065):
    allows = live <= 0
             || !has_input_history
             || idle_ms > 60s  (an unfinished composition is held)
             || idle_ms > 10s  (no composition)
  Note: live_sessions does NOT change across the reclaim (the reclaiming
  client takes a slot itself) => the ONLY discriminating input is
  composing_sessions.  That is why the synthetic client keeps typing.

  Exit codes:
    0  VERDICT=GATE_EXPORT_E2E_OK
    4  VERDICT=GATE_EXPORT_E2E_FAIL
    5  ABORT (environment unusable)
    6  VERDICT=GATE_EXPORT_E2E_INCONCLUSIVE (the synthetic session never
       closed the gate => B-1 cannot be proven with this method)
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
$script:ExportFile = $null

# control ids (WeaselServer/Cloud/CloudAccountUi.h) and messages
$script:IdStatus = 1006
$script:IdSyncNow = 1016
$script:MsgCommand = 0x0111
$script:BmClick = 0x00F5
$script:AccountClass = 'FluxingCloudAccountDialog_v1'

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
  Log 'VERDICT=GATE_EXPORT_E2E_FAIL'
  Log "EXIT=$Code"
  exit $Code
}

function Fail {
  param([string]$Reason)
  Log "FAIL=$Reason"
  Log 'VERDICT=GATE_EXPORT_E2E_FAIL'
  Log 'EXIT=4'
  exit 4
}

function Inconclusive {
  param([string]$Reason)
  Log "INCONCLUSIVE=$Reason"
  Log 'VERDICT=GATE_EXPORT_E2E_INCONCLUSIVE'
  Log 'EXIT=6'
  exit 6
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
  $text = $out.Trim()
  $m = [regex]::Match($text, 'ECHO=(\d+)')
  if (-not $m.Success) {
    return [pscustomobject]@{ Raw = $text; Value = $null }
  }
  return [pscustomobject]@{ Raw = $text; Value = [long]$m.Groups[1].Value }
}

function Get-ExportStamp {
  if ($null -eq $script:ExportFile) { return [long]::MinValue }
  if (-not (Test-Path -LiteralPath $script:ExportFile)) { return [long]::MinValue }
  return [long](Get-Item -LiteralPath $script:ExportFile).LastWriteTimeUtc.Ticks
}

function Wait-ExportAdvance {
  # Seconds until the export file mtime advances; -1 when it does not.
  param([long]$BaselineTicks, [int]$Seconds)
  $start = Get-Date
  $end = $start.AddSeconds($Seconds)
  while ((Get-Date) -lt $end) {
    if ((Get-ExportStamp) -gt $BaselineTicks) {
      return [math]::Round(((Get-Date) - $start).TotalSeconds, 1)
    }
    Start-Sleep -Milliseconds 400
  }
  return -1
}

function Trigger-SyncRound {
  # The account window handles WM_COMMAND with LOWORD(wParam) == kIdSyncNow,
  # so posting the command works even when the button is disabled.
  [void][FluxWin]::PostMessageW($script:Account, [uint32]$script:MsgCommand,
                               [IntPtr]$script:IdSyncNow, [IntPtr]0)
  if ($script:SyncNow -ne [IntPtr]::Zero) {
    [void][FluxWin]::SendMessageW($script:SyncNow, [uint32]$script:BmClick,
                                  [IntPtr]::Zero, [IntPtr]::Zero)
  }
}

# ---------------------------------------------------------------- setup ----
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:OutDir = Join-Path $script:OutRoot "gate-export-$stamp"
New-Item -ItemType Directory -Force -Path $script:OutDir | Out-Null
$script:ConsoleFile = Join-Path $script:OutDir 'console.txt'

Log "REPO_ROOT=$script:RepoRoot"
Log "OUT_DIR=$script:OutDir"
Log "TOOL=$script:Tool"

if (-not (Test-Path -LiteralPath $script:Tool)) {
  Abort 'TOOL_MISSING (build test/TestOrphanReclaimE2E first)' 5
}

$serverProcs = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
if ($serverProcs.Count -eq 0) {
  Abort 'SERVER_NOT_RUNNING (start the real WeaselServer first)' 5
}
foreach ($sp in $serverProcs) {
  $ver = $sp.MainModule.FileVersionInfo.FileVersion
  Log ("SERVER pid={0} path={1} ver={2}" -f $sp.Id, $sp.Path, $ver)
}
$serverPid = [uint32]$serverProcs[0].Id

# user dir (registry is the truth source) -> export file
$userDir = $null
try {
  $prop = Get-ItemProperty -LiteralPath 'HKCU:\Software\Fluxing\Weasel' -Name 'RimeUserDir' -ErrorAction Stop
  $userDir = [string]$prop.RimeUserDir
} catch {
  $userDir = $null
}
if ([string]::IsNullOrWhiteSpace($userDir)) {
  $userDir = 'D:\Program Files\fluxing\user1\fluxing'
}
Log "USER_DIR=$userDir"
$syncRoot = Join-Path $userDir 'sync'
$cands = @(Get-ChildItem -LiteralPath $syncRoot -Recurse -File -Filter '*.userdb.txt' -ErrorAction SilentlyContinue)
if ($cands.Count -eq 0) {
  Abort "EXPORT_FILE_NOT_FOUND under $syncRoot" 5
}
$script:ExportFile = ($cands | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
$exportItem = Get-Item -LiteralPath $script:ExportFile
Log "EXPORT_FILE=$script:ExportFile"
Log ("EXPORT_SIZE={0} EXPORT_MTIME_INITIAL={1}" -f $exportItem.Length, $exportItem.LastWriteTime)

if (-not (Test-Path -LiteralPath $script:DiagLog)) {
  Abort "DIAG_LOG_MISSING ($script:DiagLog)" 5
}
$diagBaseline = (Get-Item -LiteralPath $script:DiagLog).Length
Log "LOG_BASELINE_BYTES=$diagBaseline"

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
if ($script:SyncNow -eq [IntPtr]::Zero) {
  Abort "SYNC_NOW_BUTTON_MISSING (id $($script:IdSyncNow) not found)" 5
}
Log ("SYNC_NOW_HWND=0x{0:X} visible={1} enabled={2}" -f $script:SyncNow.ToInt64(),
     [FluxWin]::IsWindowVisible($script:SyncNow), [FluxWin]::IsWindowEnabled($script:SyncNow))
$statusCtrl = [FluxWin]::GetDlgItem($account, $script:IdStatus)
Log "ACCOUNT_STATUS(id $($script:IdStatus))=$([FluxWin]::TextOf($statusCtrl))"

$childLines = New-Object System.Collections.Generic.List[string]
foreach ($c in [FluxWin]::ChildrenOf($account)) {
  $id = [FluxWin]::GetDlgCtrlID($c)
  $txt = [FluxWin]::TextOf($c)
  $childLines.Add("id=$id class='$([FluxWin]::ClassOf($c))' visible=$([FluxWin]::IsWindowVisible($c)) enabled=$([FluxWin]::IsWindowEnabled($c)) text='$txt'")
}
$childFile = Join-Path $script:OutDir 'account-children.txt'
Set-Content -LiteralPath $childFile -Value ($childLines -join "`n") -Encoding utf8
Log "CHILDREN_FILE=$childFile count=$($childLines.Count)"

# The engine is a member of the account window => opening the window starts the
# engine, and the engine runs a startup round ~30 s later.  Let that pass
# before the control phase so that it cannot be mistaken for our own round.
Log 'ENGINE_SETTLE_WAIT_SEC=40'
Start-Sleep -Seconds 40

# ------------------------------------------------------ phase C (control) --
# No synthetic session at all => the gate must allow the round => the export
# file must be rewritten.  Positive control for the whole criterion.
$stampC = Get-ExportStamp
Trigger-SyncRound
Log 'SYNC_ROUND_TRIGGERED_C=1 (control: no synthetic session)'
$latC = Wait-ExportAdvance -BaselineTicks $stampC -Seconds 150
if ($latC -lt 0) {
  Abort 'CONTROL_ROUND_DID_NOT_EXPORT (the export file never advanced => the trigger, the login state or the export path is broken; the A/B comparison would be meaningless)' 5
}
Log "CONTROL_EXPORT_ADVANCED_AFTER_SEC=$latC"
$controlWindow = [int][math]::Ceiling($latC) + 10
if ($controlWindow -gt 45) { $controlWindow = 45 }
if ($controlWindow -lt 12) { $controlWindow = 12 }
Log "A2_OBSERVATION_WINDOW_SEC=$controlWindow"

# ------------------------------------------------- phase A2 (residual) ------
$holdMarker = Join-Path $script:OutDir 'hold.marker'
$pauseFile = "$holdMarker.pause"
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
$holdStart = Get-Date

$echoBefore = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoBefore.Value) { Fail "ECHO_BEFORE_UNPARSED raw='$($echoBefore.Raw)'" }
if ($echoBefore.Value -eq 0) { Fail 'ECHO_BEFORE_ZERO (the held session was already gone)' }
Log "ECHO_BEFORE=$($echoBefore.Value) (non zero => the session really is alive)"

# Stop the typing => the idle grace (10 s) runs out while the session still
# holds its unfinished composition => the gate must stay closed.
Set-Content -LiteralPath $pauseFile -Value 'pause' -Encoding ascii
Log 'HOLD_TYPING_PAUSED=1'
Start-Sleep -Seconds 12

$stampA2 = Get-ExportStamp
Trigger-SyncRound
Log 'SYNC_ROUND_TRIGGERED_A2=1 (residual session alive, idle > 10 s)'
$latA2 = Wait-ExportAdvance -BaselineTicks $stampA2 -Seconds $controlWindow
$a2Blocked = ($latA2 -lt 0)
if ($a2Blocked) {
  Log "A2_NO_EXPORT_WITHIN_SEC=$controlWindow (the gate stayed closed because of the residual session)"
} else {
  Log "A2_EXPORT_ADVANCED_AFTER_SEC=$latA2 (the residual session did NOT hold the gate)"
}

# ------------------------------------------------- phase B (reclaimed) ------
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
  Fail 'TRIGGER_TIMEOUT (the trigger did not exit within 30 s)'
}
Log "TRIGGER_EXIT=$($trigger.ExitCode)"
$triggerId = Read-MarkerValue -Path $triggerMarker -Key 'ipc_id'
Log "TRIGGER_IPC_ID=$triggerId"
Start-Sleep -Seconds 2

$echoAfter = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoAfter.Value) { Fail "ECHO_AFTER_UNPARSED raw='$($echoAfter.Raw)'" }
if ($echoAfter.Value -ne 0) {
  Fail "ECHO_AFTER_NOT_ZERO value=$($echoAfter.Value) (the orphan session was not reclaimed)"
}
Log 'ECHO_AFTER=0 (the orphan session is gone on the server)'

# The idle grace (10 s) must have run out, otherwise the round would be
# blocked by "recent input" for a reason unrelated to the residual session.
$sinceKey = ((Get-Date) - $holdStart).TotalSeconds
$need = 12 - $sinceKey
if ($need -gt 0) { Start-Sleep -Seconds ([int][math]::Ceiling($need)) }
$idleB = ((Get-Date) - $holdStart).TotalSeconds
Log ("IDLE_BEFORE_B_SEC={0}" -f [math]::Round($idleB, 1))

$stampB = Get-ExportStamp
Trigger-SyncRound
Log 'SYNC_ROUND_TRIGGERED_B=1 (the residual session was reclaimed)'
$latB = Wait-ExportAdvance -BaselineTicks $stampB -Seconds 150
if ($latB -lt 0) {
  Fail 'B1B_FAILED (the round was still skipped after the residual session was reclaimed: the export file did not advance within 150 s)'
}
Log "B1B_EXPORT_ADVANCED_AFTER_SEC=$latB"

# ------------------------------------------------------- diag log slice ----
$logItem = Get-Item -LiteralPath $script:DiagLog
if ($logItem.Length -lt $diagBaseline) {
  Abort 'DIAG_LOG_ROTATED (the log shrank, the baseline offset is invalid)' 5
}
$sliceBytes = New-Object byte[] ($logItem.Length - $diagBaseline)
$fs = [System.IO.File]::Open($script:DiagLog, [System.IO.FileMode]::Open,
                              [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
try {
  $null = $fs.Seek($diagBaseline, [System.IO.SeekOrigin]::Begin)
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
Log "CRITERION_B1_CONTROL_ADVANCED=1"
Log "CRITERION_B1_RESIDUAL_BLOCKED=$([int]$a2Blocked)"
Log "CRITERION_B1_RECLAIMED_ADVANCED=1"

if (-not $a2Blocked) {
  Inconclusive 'RESIDUAL_DID_NOT_BLOCK (the round was not skipped while the residual session was alive => the synthetic session never held the gate => B-1 cannot be proven with this method; A2 vs B is not a discriminating comparison)'
}
if ($idleB -gt 60) {
  Inconclusive ("COMPOSING_GRACE_EXPIRED (idle {0} s > 60 s => the residual session could not have blocked the round any more => the comparison is void)" -f [math]::Round($idleB, 1))
}
if ($reclaimHits.Count -lt 1) {
  Fail 'B2_FAILED (no "reclaim orphan session" line for the orphan in the diag log)'
}
if ($keptHits.Count -lt 1) {
  Fail 'B3_FAILED (no "orphan_reclaim presence=kept" line in the diag log)'
}
if ($absentHits.Count -ne 0) {
  Fail 'B3_FAILED (the presence flipped to absent during the reclaim)'
}

Log 'VERDICT=GATE_EXPORT_E2E_OK'
Log 'EXIT=0'
exit 0
