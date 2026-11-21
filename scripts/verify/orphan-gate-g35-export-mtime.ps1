#Requires -Version 7.0
<#
.SYNOPSIS
  G-3 / G-5 reverse criteria for the cloud sync maintenance gate.

.DESCRIPTION
  Observation surface = the mtime of the export file
      <RimeUserDir>\sync\<account>\rime_ice.userdb.txt
  A status text is NEVER the criterion (docs/release/restart-checklist-1.3.6.md
  section 4: "a status text must not be used as the criterion").  The status
  control text is only logged, as a hint.

  Gate facts (include/FluxingMaintenanceGate.h:67-84, ADR 0065):
      allows = live_sessions <= 0
            || !has_input_history
            || (composing_sessions > 0 ? idle_ms > 60000 : idle_ms > 10000)
  The constant kMaintenanceIdleGraceMs is 10 s (same header, line 49).

  The synthetic session is built by
      release\TestOrphanReclaimE2E.exe --hold-commit <marker> active
  which produces exactly the state the criteria need:
      composing == 0, has_input_history == true, session resident.
  The composition state is proven by "status.composing=" in the IPC response
  body (RimeWithWeasel.cpp:2061-2063), which comes from the same
  rime_api->get_status call that feeds the gate.  It is never read from a
  status text.

  Phases (all against the real, already running WeaselServer.exe; nothing is
  started, stopped, installed or logged out):

    C   control    no synthetic session at all
                   => the round must NOT be skipped => the export mtime MUST
                      advance.  Positive control: without it the G-3 / G-5
                      result would be meaningless.
    G3  negative   synthetic session alive, typing refreshed every 1500 ms
                   (idle < 10 s), composing == 0
                   => the gate must stay CLOSED => the export mtime must NOT
                      advance.  This is the G-3 case.
    G5  positive   the SAME session, typing paused (idle > 10 s), composing == 0
                   => the gate must OPEN => the export mtime MUST advance.
                   This is the G-5 case.

  G3 and G5 differ in exactly one variable (is the user still typing), so the
  pair is the G-3 / G-5 proof.

  Order is C -> G3 -> G5.  A round that fires on its own can never produce a
  false G-3 (a round that is blocked by the gate cannot export), but it could
  produce a false G-5.  Therefore the script watches the export file for a
  quiescence window BEFORE the G-5 trigger; if the export still advances
  there, the verdict is INCONCLUSIVE, because the advance could not be
  attributed to the triggered round.

  A skipped round does not enter the failure backoff
  (WeaselServer/Cloud/CloudSyncEngine.cpp:340-350), and a manual round has
  the highest priority (WeaselServer/Cloud/CloudAccountDialog.cpp:811-814),
  so the G-5 round right after a skipped G-3 round is expected to run.

  Modes:  -Mode C | G3 | G5 | All        (default All)
          -DryRun                        print the plan, touch nothing
  Options: -RoundWaitSec  how long to wait for an export advance (default 150)
           -IdleWaitSec   extra idle margin on top of the 12 s threshold
           -QuiesceSec    quiescence watch length before the G-5 trigger

  Exit codes:
    0  VERDICT=GATE_G35_OK
    4  VERDICT=GATE_G35_FAIL
    5  ABORT (environment unusable)
    6  VERDICT=GATE_G35_INCONCLUSIVE

  This file must stay pure ASCII: the two Chinese characters that are matched
  for the "skipped" hint are written as code units, not as literals.
#>

param(
  [ValidateSet('C', 'G3', 'G5', 'All')]
  [string]$Mode = 'All',
  [switch]$DryRun,
  [int]$RoundWaitSec = 150,
  [int]$IdleWaitSec = 15,
  [int]$QuiesceSec = 20
)

$ErrorActionPreference = 'Stop'

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Tool = Join-Path $script:RepoRoot 'release\TestOrphanReclaimE2E.exe'
$script:DiagLog = Join-Path $env:TEMP 'fluxing-profile-notify.log'
$script:OutRoot = Join-Path $script:RepoRoot 'tmp\fluxing-bg-g35-gate'

$script:OutDir = $null
$script:ConsoleFile = $null
$script:Account = [IntPtr]::Zero
$script:SyncNow = [IntPtr]::Zero
$script:StatusCtrl = [IntPtr]::Zero
$script:ExportFile = $null
$script:Hold = $null

# control ids (WeaselServer/Cloud/CloudAccountUi.h) and messages
$script:IdStatus = 1006
$script:IdSyncNow = 1016
$script:MsgCommand = 0x0111
$script:BmClick = 0x00F5
$script:AccountClass = 'FluxingCloudAccountDialog_v1'

# The idle grace is 10 s (include/FluxingMaintenanceGate.h:49).  The script
# uses a margin on both sides so that a measurement jitter cannot flip a
# verdict.  The idle is measured with [Environment]::TickCount64, which is the
# same clock as GetTickCount64 on the server and in the harness.
$script:IdleGateMs = 10000
$script:IdleMarginMs = 12000
$script:IdleRefreshMs = 1500

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
  Log 'VERDICT=GATE_G35_FAIL'
  Log "EXIT=$Code"
  exit $Code
}

function Fail {
  param([string]$Reason)
  Log "FAIL=$Reason"
  Log 'VERDICT=GATE_G35_FAIL'
  Log 'EXIT=4'
  exit 4
}

function Inconclusive {
  param([string]$Reason)
  Log "INCONCLUSIVE=$Reason"
  Log 'VERDICT=GATE_G35_INCONCLUSIVE'
  Log 'EXIT=6'
  exit 6
}

function Read-MarkerValue {
  param([string]$Path, [string]$Key)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  $raw = Get-Content -LiteralPath $Path -Raw -Encoding utf8 -ErrorAction SilentlyContinue
  if ($null -eq $raw) { return $null }
  $m = [regex]::Match($raw, [regex]::Escape($Key) + '=(\d+)')
  if (-not $m.Success) { return $null }
  return [long]$m.Groups[1].Value
}

# The harness rewrites the state file every refresh tick, so a read can hit a
# half written file: retry for a moment instead of failing.
function Read-StateValue {
  param([string]$Path, [string]$Key)
  for ($i = 0; $i -lt 20; $i++) {
    if (Test-Path -LiteralPath $Path) {
      $raw = Get-Content -LiteralPath $Path -Raw -Encoding utf8 -ErrorAction SilentlyContinue
      if ($null -ne $raw) {
        $m = [regex]::Match($raw, [regex]::Escape($Key) + '=(\d+)')
        if ($m.Success) { return [long]$m.Groups[1].Value }
      }
    }
    Start-Sleep -Milliseconds 50
  }
  return $null
}

function Get-IdleMs {
  param([string]$StateFile)
  $last = Read-StateValue -Path $StateFile -Key 'last_input_tick_ms'
  if ($null -eq $last) { return $null }
  return [long]([Environment]::TickCount64 - $last)
}

function Wait-IdleAbove {
  param([string]$StateFile, [long]$TargetMs, [int]$TimeoutSec)
  $deadline = (Get-Date).AddSeconds($TimeoutSec)
  $idle = $null
  while ((Get-Date) -lt $deadline) {
    $idle = Get-IdleMs -StateFile $StateFile
    if ($null -ne $idle -and $idle -ge $TargetMs) { return $idle }
    Start-Sleep -Milliseconds 500
  }
  return $idle
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

function Read-StatusText {
  if ($script:StatusCtrl -eq [IntPtr]::Zero) { return '' }
  return [FluxWin]::TextOf($script:StatusCtrl)
}

function Test-SkippedHint {
  # The manual sync path writes a "skipped" status
  # (WeaselServer/Cloud/CloudAccountDialog.cpp:839-844).  The two Chinese
  # characters are matched by code unit so that this script stays ASCII.
  $text = Read-StatusText
  if ([string]::IsNullOrEmpty($text)) { return $false }
  $needle = [string]([char]0x8DF3) + [string]([char]0x8FC7)
  return $text.Contains($needle)
}

function Open-AccountWindow {
  $wins = [FluxWin]::TopLevelOfPid($script:ServerPid)
  Log "TOP_LEVEL_WINDOWS=$($wins.Count)"
  foreach ($w in $wins) {
    Log ("  WIN hwnd=0x{0:X} class='{1}' visible={2} rect={3} title='{4}'" -f
         $w.ToInt64(), [FluxWin]::ClassOf($w), [FluxWin]::IsWindowVisible($w),
         [FluxWin]::RectOf($w), [FluxWin]::TitleOf($w))
  }
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
      foreach ($c in [FluxWin]::TopLevelOfPid($script:ServerPid)) {
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
  $script:StatusCtrl = [FluxWin]::GetDlgItem($account, $script:IdStatus)
  Log ("SYNC_NOW_HWND=0x{0:X} visible={1} enabled={2}" -f $script:SyncNow.ToInt64(),
       [FluxWin]::IsWindowVisible($script:SyncNow), [FluxWin]::IsWindowEnabled($script:SyncNow))
  Log "ACCOUNT_STATUS(id $($script:IdStatus))=$([FluxWin]::TextOf($script:StatusCtrl))"
}

function Stop-Harness {
  if ($null -eq $script:Hold) { return }
  try {
    if (-not $script:Hold.HasExited) {
      Stop-Process -Id $script:Hold.Id -Force -ErrorAction SilentlyContinue
      try { Wait-Process -Id $script:Hold.Id -Timeout 15 -ErrorAction SilentlyContinue } catch { }
    }
    if ($null -ne (Get-Process -Id $script:Hold.Id -ErrorAction SilentlyContinue)) {
      Log "HOLD_STILL_ALIVE pid=$($script:Hold.Id)"
    } else {
      Log 'HOLD_STOPPED=1'
    }
  } catch {
    Log "HOLD_STOP_ERROR=$($_.Exception.Message)"
  }
  $script:Hold = $null
}

function Write-DiagSlice {
  param([long]$BaselineBytes)
  if (-not (Test-Path -LiteralPath $script:DiagLog)) { return }
  $item = Get-Item -LiteralPath $script:DiagLog
  if ($item.Length -lt $BaselineBytes) {
    Log 'DIAG_LOG_ROTATED=1 (the log shrank, the baseline offset is invalid)'
    return
  }
  $sliceBytes = New-Object byte[] ($item.Length - $BaselineBytes)
  $fs = [System.IO.File]::Open($script:DiagLog, [System.IO.FileMode]::Open,
                               [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try {
    $null = $fs.Seek($BaselineBytes, [System.IO.SeekOrigin]::Begin)
    $read = $fs.Read($sliceBytes, 0, $sliceBytes.Length)
  } finally {
    $fs.Close()
  }
  if ($read -lt $sliceBytes.Length) { $sliceBytes = $sliceBytes[0..($read - 1)] }
  $slice = [System.Text.Encoding]::UTF8.GetString($sliceBytes)
  $file = Join-Path $script:OutDir 'log-slice.txt'
  Set-Content -LiteralPath $file -Value $slice -Encoding utf8
  Log "LOG_SLICE_FILE=$file bytes=$($sliceBytes.Length)"
}

# --------------------------------------------------------------- setup -----
$phases = @()
if ($Mode -eq 'All') { $phases = @('C', 'G3', 'G5') } else { $phases = @($Mode) }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:OutDir = Join-Path $script:OutRoot "g35-$stamp"
New-Item -ItemType Directory -Force -Path $script:OutDir | Out-Null
$script:ConsoleFile = Join-Path $script:OutDir 'console.txt'

Log "REPO_ROOT=$script:RepoRoot"
Log "OUT_DIR=$script:OutDir"
Log "TOOL=$script:Tool"
Log "MODE=$Mode PHASES=$($phases -join ',')"
Log "ROUND_WAIT_SEC=$RoundWaitSec IDLE_WAIT_SEC=$IdleWaitSec QUIESCE_SEC=$QuiesceSec"
Log "IDLE_GATE_MS=$($script:IdleGateMs) IDLE_MARGIN_MS=$($script:IdleMarginMs) IDLE_REFRESH_MS=$($script:IdleRefreshMs)"

# user dir (the registry is the truth source) -> export file
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
  if ($DryRun) {
    Log "EXPORT_FILE_NOT_FOUND under $syncRoot"
  } else {
    Abort "EXPORT_FILE_NOT_FOUND under $syncRoot" 5
  }
} else {
  $script:ExportFile = ($cands | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
  $exportItem = Get-Item -LiteralPath $script:ExportFile
  Log "EXPORT_FILE=$script:ExportFile"
  Log ("EXPORT_SIZE={0} EXPORT_MTIME_INITIAL={1}" -f $exportItem.Length, $exportItem.LastWriteTime)
}

$toolPresent = (Test-Path -LiteralPath $script:Tool)
Log "TOOL_PRESENT=$toolPresent"
$serverProcs = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
Log "SERVER_PROCESSES=$($serverProcs.Count)"
foreach ($sp in $serverProcs) {
  Log ("SERVER pid={0} path={1} ver={2}" -f $sp.Id, $sp.Path, $sp.MainModule.FileVersionInfo.FileVersion)
}

if ($DryRun) {
  Log 'DRYRUN=1 (no window, no harness, no sync round, nothing changed)'
  Log "WOULD_RUN=$script:Tool --hold-commit <marker> active"
  Log 'WOULD_OPEN_ACCOUNT_WINDOW=1 (WM_COMMAND 40024 to the server top level window)'
  Log 'WOULD_TRIGGER_ROUND=1 (WM_COMMAND 1016 + BM_CLICK on the sync now button)'
  Log "WOULD_EXPORT_FILE=$script:ExportFile"
  Log 'CRITERION_C_EXPORT_ADVANCED=1'
  Log 'CRITERION_G3_EXPORT_NOT_ADVANCED=1'
  Log 'CRITERION_G5_EXPORT_ADVANCED=1'
  Log 'VERDICT=GATE_G35_DRYRUN'
  Log 'EXIT=0'
  exit 0
}

if (-not $toolPresent) {
  Abort 'TOOL_MISSING (build test/TestOrphanReclaimE2E first)' 5
}
if ($serverProcs.Count -eq 0) {
  Abort 'SERVER_NOT_RUNNING (start the real WeaselServer first)' 5
}
if ($null -eq $script:ExportFile) {
  Abort 'EXPORT_FILE_NOT_FOUND' 5
}
if (-not (Test-Path -LiteralPath $script:DiagLog)) {
  Log "DIAG_LOG_MISSING=$($script:DiagLog) (the slice is skipped, not fatal)"
} else {
  $diagBaseline = (Get-Item -LiteralPath $script:DiagLog).Length
  Log "LOG_BASELINE_BYTES=$diagBaseline"
}

$script:ServerPid = [uint32]$serverProcs[0].Id
Open-AccountWindow

# The engine is a member of the account window => opening the window starts
# the engine, and the engine runs a startup round ~30 s later.  Let that pass
# before the control phase so that it cannot be mistaken for our own round.
Log 'ENGINE_SETTLE_WAIT_SEC=40'
Start-Sleep -Seconds 40

$holdMarker = Join-Path $script:OutDir 'hold.marker'
$pauseFile = "$holdMarker.pause"
$stateFile = "$holdMarker.state"
$holdStdout = Join-Path $script:OutDir 'hold.stdout.txt'
$holdStderr = Join-Path $script:OutDir 'hold.stderr.txt'
$observeSec = 30
$g3Blocked = $null
$g5Advanced = $false

try {
  # ------------------------------------------------------ phase C (control)
  if ($phases -contains 'C') {
    $stampC = Get-ExportStamp
    Trigger-SyncRound
    Log 'ROUND_TRIGGERED_C=1 (control: no synthetic session)'
    Log "STATUS_AFTER_C=$([FluxWin]::TextOf($script:StatusCtrl))"
    $latC = Wait-ExportAdvance -BaselineTicks $stampC -Seconds $RoundWaitSec
    if ($latC -lt 0) {
      Abort 'CONTROL_ROUND_DID_NOT_EXPORT (the trigger, the login state or the export path is broken => the G-3 / G-5 result would be meaningless)' 5
    }
    Log "CONTROL_EXPORT_ADVANCED_AFTER_SEC=$latC"
    $observeSec = [int][math]::Ceiling($latC) + 15
    if ($observeSec -gt 60) { $observeSec = 60 }
    if ($observeSec -lt 15) { $observeSec = 15 }
    Log "OBSERVE_WINDOW_SEC=$observeSec"
  }

  # ------------------------------------------- the synthetic session (G3/G5)
  if ($phases -contains 'G3' -or $phases -contains 'G5') {
    $quoted = '"' + $holdMarker + '"'
    $script:Hold = Start-Process -FilePath $script:Tool `
      -ArgumentList @('--hold-commit', $quoted, 'active') `
      -PassThru -WindowStyle Hidden `
      -RedirectStandardOutput $holdStdout -RedirectStandardError $holdStderr
    Log "HOLD_STARTED pid=$($script:Hold.Id) mode=active"

    $deadline = (Get-Date).AddSeconds(40)
    $orphanId = $null
    while ((Get-Date) -lt $deadline) {
      if ($script:Hold.HasExited) {
        Log "HOLD_STDOUT=$(Get-Content -LiteralPath $holdStdout -Raw -ErrorAction SilentlyContinue)"
        Log "HOLD_STDERR=$(Get-Content -LiteralPath $holdStderr -Raw -ErrorAction SilentlyContinue)"
        Fail "HOLD_EXITED_EARLY exit=$($script:Hold.ExitCode)"
      }
      $orphanId = Read-MarkerValue -Path $holdMarker -Key 'ipc_id'
      if ($null -ne $orphanId) { break }
      Start-Sleep -Milliseconds 100
    }
    if ($null -eq $orphanId) {
      Fail 'HOLD_MARKER_TIMEOUT (no ipc_id in the marker within 40 s)'
    }
    Log "SYNTHETIC_IPC_ID=$orphanId"
    Log "SYNTHETIC_PID=$(Read-MarkerValue -Path $holdMarker -Key 'pid')"
    $echo = & $script:Tool --echo $orphanId 2>&1 | Out-String
    Log "ECHO_BEFORE=$($echo.Trim())"

    # The composition state is the core of the whole criterion: it must be 0
    # and it must be proven by the harness response body, not by a text.
    $composing = Read-StateValue -Path $stateFile -Key 'composing'
    if ($null -eq $composing) {
      Fail 'HOLD_STATE_MISSING (the harness wrote no state file => the state was not manufactured)'
    }
    Log "SYNTHETIC_COMPOSING=$composing (must be 0)"
    if ($composing -ne 0) {
      Fail "SYNTHETIC_COMPOSING_NOT_ZERO value=$composing (the state was not manufactured)"
    }
  }

  # ------------------------------------------------- phase G3 (negative) ----
  if ($phases -contains 'G3') {
    $idleG3 = Get-IdleMs -StateFile $stateFile
    Log "G3_IDLE_MS=$idleG3 (must be < $($script:IdleGateMs))"
    if ($null -eq $idleG3 -or $idleG3 -ge $script:IdleGateMs) {
      Fail 'G3_IDLE_NOT_BELOW_GATE (the session was not refreshed recently => the state was not manufactured)'
    }
    $stampG3 = Get-ExportStamp
    Trigger-SyncRound
    Log 'ROUND_TRIGGERED_G3=1 (synthetic session alive, typing refreshed, composing == 0)'
    $latG3 = Wait-ExportAdvance -BaselineTicks $stampG3 -Seconds $observeSec
    $g3Blocked = ($latG3 -lt 0)
    if ($g3Blocked) {
      Log "G3_NO_EXPORT_WITHIN_SEC=$observeSec (the gate stayed closed)"
    } else {
      Log "G3_EXPORT_ADVANCED_AFTER_SEC=$latG3 (the gate did NOT stay closed)"
    }
    Log "STATUS_AFTER_G3=$([FluxWin]::TextOf($script:StatusCtrl))"
    Log "STATUS_AFTER_G3_SKIPPED_HINT=$([int](Test-SkippedHint))"
    if (-not $g3Blocked) {
      Inconclusive 'G3_DID_NOT_BLOCK (the round was not skipped while the synthetic session was alive and recently refreshed => the gate did not close => the G-3 / G-5 pair proves nothing)'
    }
  }

  # ------------------------------------------------- phase G5 (positive) ----
  if ($phases -contains 'G5') {
    Set-Content -LiteralPath $pauseFile -Value 'pause' -Encoding ascii
    Log 'HOLD_TYPING_PAUSED=1'
    $idleG5 = Wait-IdleAbove -StateFile $stateFile -TargetMs $script:IdleMarginMs -TimeoutSec 90
    Log "G5_IDLE_MS=$idleG5 (must be >= $($script:IdleMarginMs))"
    if ($null -eq $idleG5 -or $idleG5 -lt $script:IdleMarginMs) {
      Fail 'G5_IDLE_NOT_ABOVE_GATE (the idle window was not reached => the state was not manufactured)'
    }

    # Quiescence watch: a round that fires on its own would make the G-5
    # advance unattributable => detect it before the trigger.
    $quiet = $false
    for ($try = 1; $try -le 2; $try++) {
      $q0 = Get-ExportStamp
      Start-Sleep -Seconds $QuiesceSec
      if ((Get-ExportStamp) -gt $q0) {
        Log "SPONTANEOUS_EXPORT_ADVANCED=1 (watch $try) => the engine is not quiescent"
        Start-Sleep -Seconds $QuiesceSec
      } else {
        Log "QUIESCENT_WATCH_OK=1 (watch $try, $QuiesceSec s)"
        $quiet = $true
        break
      }
    }
    if (-not $quiet) {
      Inconclusive 'ENGINE_NOT_QUIESCENT (the export advanced without a trigger => the G-5 advance cannot be attributed)'
    }

    $stampG5 = Get-ExportStamp
    Trigger-SyncRound
    Log 'ROUND_TRIGGERED_G5=1 (same session, typing paused, idle > 10 s, composing == 0)'
    $latG5 = Wait-ExportAdvance -BaselineTicks $stampG5 -Seconds $RoundWaitSec
    if ($latG5 -lt 0) {
      Fail 'G5_FAILED (the round was still skipped after the idle window: the export file did not advance)'
    }
    $g5Advanced = $true
    Log "G5_EXPORT_ADVANCED_AFTER_SEC=$latG5"
    Log "STATUS_AFTER_G5=$([FluxWin]::TextOf($script:StatusCtrl))"
    Log "STATUS_AFTER_G5_SKIPPED_HINT=$([int](Test-SkippedHint))"
  }
} finally {
  Stop-Harness
  if (Test-Path -LiteralPath $pauseFile) { Remove-Item -LiteralPath $pauseFile -Force -ErrorAction SilentlyContinue }
  if ($null -ne $diagBaseline) { Write-DiagSlice -BaselineBytes $diagBaseline }
}

# ----------------------------------------------------------- criteria ------
# Reaching this line means the control round advanced the export file (the
# C phase aborts otherwise), so the criterion is recorded as satisfied.
Log 'CRITERION_C_EXPORT_ADVANCED=1'
Log "CRITERION_G3_EXPORT_NOT_ADVANCED=$([int]($null -eq $g3Blocked -or $g3Blocked))"
Log "CRITERION_G5_EXPORT_ADVANCED=$([int]$g5Advanced)"

if ($phases -contains 'G3' -and -not $g3Blocked) {
  Inconclusive 'G3_DID_NOT_BLOCK (the gate did not close => the pair proves nothing)'
}
if ($phases -contains 'G5' -and -not $g5Advanced) {
  Fail 'G5_FAILED (the round did not pass the gate after the idle window)'
}

Log 'VERDICT=GATE_G35_OK'
Log 'EXIT=0'
exit 0
