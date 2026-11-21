#Requires -Version 7.0
<#
.SYNOPSIS
  Acceptance item B-4 - the NORMAL focus-out / session-end path of the server
  must still flip the presence state to "absent" and still hide the quick
  panel (behaviour identical to the state before batch AW / ADR 0067).

.DESCRIPTION
  This script never starts, stops, restarts or reinstalls anything. It only
  drives a synthetic IPC client (test/TestOrphanReclaimE2E/TestFocusOutE2E)
  against the ALREADY RUNNING WeaselServer.exe over the real named pipe and
  then reads the server's own diagnostic log
  (%TEMP%\fluxing-profile-notify.log).

  The risk under test (ADR 0067): RemoveSession was split into
  _RemoveSessionInternal(ipc_id, is_orphan_reclaim). The orphan reclaim path
  must NOT flip presence and must NOT hide the panel; the NORMAL path must
  still do both. So every scenario below is judged against BOTH branches:

    normal path   -> "[server] remove_session active presence=absent"
    orphan path   -> "[server] remove_session active orphan_reclaim presence=kept"

  Phases (each phase owns one log slice, so the phases cannot contaminate
  each other):

    A  --end-session         negative control for the focus-out criterion:
                              no focus-out message is sent at all
    B  --focusout-deactivate positive, real TSF order (WeaselTSF::Deactivate
                              sends END_SESSION first, the focus-out
                              notification arrives afterwards)
    C  --focusout-only       the focus-out message ALONE flips presence
    D  --focusout-appfocus   "user switched window" (APP_FOCUS_LOST)
    E  --abrupt-exit +       the orphan control: no END_SESSION => the
       --sweep-trigger       session is reclaimed by the lazy sweep

  Phase B and C additionally watch the real quick panel window
  ("Fluxing QuickPanel", owned by WeaselServer.exe): it must become visible
  after the profile-activated FocusIn and invisible again after the focus-out.

  Exit codes:
    0  B4_VERDICT=PASS_SERVER_PATH
    4  B4_VERDICT=PARTIAL        (some criteria failed, see the report)
    5  ABORT                     (environment unusable)
#>

$ErrorActionPreference = 'Stop'

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Tool = Join-Path $script:RepoRoot 'Release\TestFocusOutE2E.exe'
$script:DiagLog = Join-Path $env:TEMP 'fluxing-profile-notify.log'
$script:OutRoot = Join-Path $script:RepoRoot 'reports\fluxing-bb-orphan-reclaim'
$script:PanelTitle = 'Fluxing QuickPanel'

$script:OutDir = $null
$script:ConsoleFile = $null
$script:ServerPid = 0
$script:Failures = New-Object System.Collections.Generic.List[string]

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
  Log 'B4_VERDICT=NOT_JUDGED'
  Log "EXIT=$Code"
  exit $Code
}

function Fail {
  param([string]$Reason)
  $script:Failures.Add($Reason)
  Log "FAIL=$Reason"
}

# --------------------------------------------------------------- helpers ----

function Read-MarkerValue {
  param([string]$Path, [string]$Key)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  $raw = Get-Content -LiteralPath $Path -Raw -Encoding utf8
  if ($null -eq $raw) { return $null }
  $m = [regex]::Match($raw, [regex]::Escape($Key) + '=(\d+)')
  if (-not $m.Success) { return $null }
  return [long]$m.Groups[1].Value
}

# Redirected stdout/stderr of a child process is still locked by the child
# while it runs => open it with shared access instead of ReadAllText.
function Read-SharedText {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  try {
    $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
                                 [System.IO.FileAccess]::Read,
                                 [System.IO.FileShare]::ReadWrite)
    try {
      $sr = New-Object System.IO.StreamReader($fs)
      return $sr.ReadToEnd()
    } finally {
      $fs.Close()
    }
  } catch {
    return ''
  }
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

function Get-LogBaseline {
  if (-not (Test-Path -LiteralPath $script:DiagLog)) { return 0 }
  return [long](Get-Item -LiteralPath $script:DiagLog).Length
}

function Get-LogSlice {
  param([long]$Baseline)
  $item = Get-Item -LiteralPath $script:DiagLog
  if ($item.Length -lt $Baseline) {
    Abort 'DIAG_LOG_ROTATED (the baseline offset is invalid)' 5
  }
  $sliceBytes = New-Object byte[] ($item.Length - $Baseline)
  $fs = [System.IO.File]::Open($script:DiagLog, [System.IO.FileMode]::Open,
                              [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
  try {
    $null = $fs.Seek($Baseline, [System.IO.SeekOrigin]::Begin)
    $read = $fs.Read($sliceBytes, 0, $sliceBytes.Length)
  } finally {
    $fs.Close()
  }
  if ($read -lt $sliceBytes.Length) {
    $sliceBytes = $sliceBytes[0..($read - 1)]
  }
  return [System.Text.Encoding]::UTF8.GetString($sliceBytes)
}

function Save-Slice {
  param([string]$Name, [string]$Content)
  $path = Join-Path $script:OutDir ("log-slice-$Name.txt")
  Set-Content -LiteralPath $path -Value $Content -Encoding utf8
  return $path
}

function Get-PanelState {
  # The quick panel is a top level WS_POPUP window of WeaselServer.exe whose
  # caption is "Fluxing QuickPanel". The enumeration lives in C# so that no
  # PowerShell callback scope can swallow the result.
  $raw = [Win32PanelProbe]::FindPanel([uint32]$script:ServerPid, $script:PanelTitle)
  $found = $raw.Contains('FOUND=1')
  $visible = $raw.Contains('VISIBLE=1')
  return [pscustomobject]@{ Found = $found; Visible = $visible }
}

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public class Win32PanelProbe {
  private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

  [DllImport("user32.dll", SetLastError = true)]
  private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll", CharSet = CharSet.Unicode, ExactSpelling = true, SetLastError = true)]
  private static extern int GetWindowTextW(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  [DllImport("user32.dll", SetLastError = true)]
  private static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll", SetLastError = true)]
  private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

  // Returns "FOUND=<0|1>;VISIBLE=<0|1>" for the first top level window of
  // the given process whose caption equals the given title.
  public static string FindPanel(uint targetPid, string title) {
    bool found = false;
    bool visible = false;
    EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
      StringBuilder sb = new StringBuilder(256);
      GetWindowTextW(hWnd, sb, sb.Capacity);
      if (sb.ToString() == title) {
        uint pid = 0;
        GetWindowThreadProcessId(hWnd, out pid);
        if (pid == targetPid) {
          found = true;
          if (IsWindowVisible(hWnd)) { visible = true; }
        }
      }
      return true;
    }, IntPtr.Zero);
    return "FOUND=" + (found ? "1" : "0") + ";VISIBLE=" + (visible ? "1" : "0");
  }
}
'@

# Runs the synthetic client and returns its marker values plus stdout.
function Invoke-Client {
  param([string]$Mode, [string]$Marker, [switch]$WatchPanel, [int]$TimeoutSec = 60)
  $stdout = Join-Path $script:OutDir ("$Mode.stdout.txt")
  $stderr = Join-Path $script:OutDir ("$Mode.stderr.txt")
  $proc = Start-Process -FilePath $script:Tool `
    -ArgumentList @($Mode, $Marker) `
    -PassThru -WindowStyle Hidden `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
  Log "CLIENT_STARTED mode=$Mode pid=$($proc.Id)"

  $panelBefore = Get-PanelState
  $panelSeen = $panelBefore.Visible
  # Poll while the client runs: the profile activated FocusIn shows the quick
  # panel and the focus-out hides it again, so the panel must be observed
  # visible in between.
  $deadline = (Get-Date).AddSeconds($TimeoutSec)
  while ((Get-Date) -lt $deadline) {
    if ($WatchPanel) {
      $st = Get-PanelState
      if ($st.Visible) { $panelSeen = $true }
    }
    if ($proc.HasExited) { break }
    Start-Sleep -Milliseconds 100
  }
  if (-not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Fail "CLIENT_TIMEOUT mode=$Mode (no exit within $TimeoutSec s)"
    return [pscustomobject]@{ Exit = -1; IpcId = $null; Pid = $null; PanelSeen = $panelSeen; PanelBefore = $panelBefore }
  }
  $panelAfter = Get-PanelState
  $text = Read-SharedText $stdout
  Log "CLIENT_STDOUT mode=$Mode exit=$($proc.ExitCode) $((($text -split "`n") | Where-Object { $_ -match '^(SESSION_STARTED|FOCUS_IN_SENT|FOCUS_OUT_SENT|END_SESSION_SENT|ABRUPT_EXIT|SWEEP_TRIGGERED|SCENARIO_DONE|ERROR|WARN)' }) -join ' | ')"
  Log "CLIENT_PANEL mode=$Mode seen_visible=$panelSeen after_found=$($panelAfter.Found) after_visible=$($panelAfter.Visible)"
  return [pscustomobject]@{
    Exit = $proc.ExitCode
    IpcId = (Read-MarkerValue -Path $Marker -Key 'ipc_id')
    Pid = (Read-MarkerValue -Path $Marker -Key 'pid')
    PanelSeen = $panelSeen
    PanelBefore = $panelBefore
    PanelAfter = $panelAfter
  }
}

# ---------------------------------------------------------------- setup ----
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:OutDir = Join-Path $script:OutRoot "b4-focusout-$stamp"
New-Item -ItemType Directory -Force -Path $script:OutDir | Out-Null
$script:ConsoleFile = Join-Path $script:OutDir 'console.txt'

Log "REPO_ROOT=$script:RepoRoot"
Log "OUT_DIR=$script:OutDir"
Log "TOOL=$script:Tool"
Log "DIAG_LOG=$script:DiagLog"

if (-not (Test-Path -LiteralPath $script:Tool)) {
  Abort "TOOL_MISSING (build test/TestOrphanReclaimE2E/TestFocusOutE2E.vcxproj first)" 5
}

$serverProcs = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
if ($serverProcs.Count -eq 0) {
  Abort 'SERVER_NOT_RUNNING (the real WeaselServer must be running)' 5
}
foreach ($sp in $serverProcs) {
  Log "SERVER pid=$($sp.Id) path=$($sp.Path)"
}
$script:ServerPid = $serverProcs[0].Id

if (-not (Test-Path -LiteralPath $script:DiagLog)) {
  Abort "DIAG_LOG_MISSING ($script:DiagLog)" 5
}

# =========================================================== phase A ========
# Negative control: the very same client, the very same handshake, the very
# same typing - but NO focus-out message at all.
Log ''
Log '=== PHASE A: session end only (no focus-out) ==='
$baseA = Get-LogBaseline
$a = Invoke-Client -Mode '--end-session' -Marker (Join-Path $script:OutDir 'a.marker')
if ($a.Exit -ne 0) { Fail "PHASE_A_CLIENT_FAILED exit=$($a.Exit)" }
Start-Sleep -Seconds 1
$sliceA = Get-LogSlice -Baseline $baseA
$null = Save-Slice -Name 'A' -Content $sliceA
$aAbsent = [regex]::Matches($sliceA, 'remove_session active presence=absent')
$aFocusOut = [regex]::Matches($sliceA, 'focus_out param=')
$aReclaim = [regex]::Matches($sliceA, 'reclaim orphan session ipc_id=' + $a.IpcId + '\b')
$aOrphanKept = [regex]::Matches($sliceA, 'remove_session active orphan_reclaim presence=kept')
Log "A_IPC_ID=$($a.IpcId)"
Log "A_NORMAL_PATH_ABSENT count=$($aAbsent.Count) (expected >= 1)"
Log "A_FOCUS_OUT_LINES count=$($aFocusOut.Count) (expected 0 - negative control)"
Log "A_RECLAIM_LINES count=$($aReclaim.Count) (expected 0)"
Log "A_ORPHAN_KEPT_LINES count=$($aOrphanKept.Count) (expected 0)"
foreach ($h in $aAbsent) { Log "A_ABSENT_LINE=$($h.Value)" }
if ($aAbsent.Count -lt 1) { Fail 'A1_FAILED (normal session end did not flip presence to absent)' }
if ($aFocusOut.Count -ne 0) { Fail "A2_FAILED ($($aFocusOut.Count) focus_out lines in a run that sent none)" }
if ($aReclaim.Count -ne 0) { Fail 'A3_FAILED (the gracefully ended session was orphan reclaimed)' }
if ($aOrphanKept.Count -ne 0) { Fail 'A4_FAILED (orphan_reclaim branch was taken on the normal path)' }

# =========================================================== phase B ========
# Positive, real TSF order: END_SESSION first, then FOCUS_OUT(deactivated).
Log ''
Log '=== PHASE B: real IME-switch order (END_SESSION then FOCUS_OUT) ==='
$baseB = Get-LogBaseline
$b = Invoke-Client -Mode '--focusout-deactivate' -Marker (Join-Path $script:OutDir 'b.marker') -WatchPanel
if ($b.Exit -ne 0) { Fail "PHASE_B_CLIENT_FAILED exit=$($b.Exit)" }
Start-Sleep -Seconds 1
$sliceB = Get-LogSlice -Baseline $baseB
$null = Save-Slice -Name 'B' -Content $sliceB
$bFocusIn = [regex]::Matches($sliceB, 'focus_in profile presence=present')
$bAbsent = [regex]::Matches($sliceB, 'remove_session active presence=absent')
$bFocusOut = [regex]::Matches($sliceB, 'focus_out param=1 presence=absent')
$bReclaim = [regex]::Matches($sliceB, 'reclaim orphan session ipc_id=' + $b.IpcId + '\b')
$bOrphanKept = [regex]::Matches($sliceB, 'remove_session active orphan_reclaim presence=kept')
$bPanelAfter = Get-PanelState
Log "B_IPC_ID=$($b.IpcId)"
Log "B_FOCUS_IN_PRESENT count=$($bFocusIn.Count) (expected >= 1)"
Log "B_NORMAL_PATH_ABSENT count=$($bAbsent.Count) (expected >= 1)"
Log "B_FOCUS_OUT_ABSENT count=$($bFocusOut.Count) (expected >= 1)"
Log "B_RECLAIM_LINES count=$($bReclaim.Count) (expected 0)"
Log "B_ORPHAN_KEPT_LINES count=$($bOrphanKept.Count) (expected 0)"
Log "B_PANEL_FOUND=$($bPanelAfter.Found) B_PANEL_VISIBLE_AFTER=$($bPanelAfter.Visible) (expected hidden) B_PANEL_SEEN_VISIBLE=$($b.PanelSeen)"
foreach ($h in $bFocusOut) { Log "B_FOCUS_OUT_LINE=$($h.Value)" }
if ($bFocusIn.Count -lt 1) { Fail 'B1_FAILED (the profile activated FocusIn did not set presence=present)' }
if ($bAbsent.Count -lt 1) { Fail 'B2_FAILED (the normal session end did not flip presence to absent)' }
if ($bFocusOut.Count -lt 1) { Fail 'B3_FAILED (no "focus_out param=1 presence=absent" line)' }
if ($bReclaim.Count -ne 0) { Fail 'B4_FAILED (the session was orphan reclaimed instead of ended normally)' }
if ($bOrphanKept.Count -ne 0) { Fail 'B5_FAILED (orphan_reclaim branch was taken on the normal path)' }
if (-not $b.PanelSeen) { Log 'WARN_B_PANEL_NEVER_VISIBLE (the panel visibility observation has no discriminating power in this phase)' }
if ($bPanelAfter.Found -and $bPanelAfter.Visible) { Fail 'B6_FAILED (the quick panel is still visible after the focus-out)' }

# =========================================================== phase C ========
# The focus-out message alone must flip presence (no END_SESSION before it).
Log ''
Log '=== PHASE C: focus-out alone flips presence ==='
$baseC = Get-LogBaseline
$c = Invoke-Client -Mode '--focusout-only' -Marker (Join-Path $script:OutDir 'c.marker') -WatchPanel
if ($c.Exit -ne 0) { Fail "PHASE_C_CLIENT_FAILED exit=$($c.Exit)" }
Start-Sleep -Seconds 1
$sliceC = Get-LogSlice -Baseline $baseC
$null = Save-Slice -Name 'C' -Content $sliceC
$cFocusIn = [regex]::Matches($sliceC, 'focus_in profile presence=present')
$cFocusOut = [regex]::Matches($sliceC, 'focus_out param=1 presence=absent')
$cReclaim = [regex]::Matches($sliceC, 'reclaim orphan session ipc_id=' + $c.IpcId + '\b')
$cOrphanKept = [regex]::Matches($sliceC, 'remove_session active orphan_reclaim presence=kept')
$cEcho = Invoke-Echo -SessionId $c.IpcId
$cPanelAfter = Get-PanelState
Log "C_IPC_ID=$($c.IpcId)"
Log "C_FOCUS_IN_PRESENT count=$($cFocusIn.Count) (expected >= 1)"
Log "C_FOCUS_OUT_ABSENT count=$($cFocusOut.Count) (expected >= 1)"
Log "C_RECLAIM_LINES count=$($cReclaim.Count) (expected 0 - the session ended normally)"
Log "C_ORPHAN_KEPT_LINES count=$($cOrphanKept.Count) (expected 0)"
Log "C_ECHO_AFTER=$($cEcho.Value) (expected 0 - the session is gone)"
Log "C_PANEL_FOUND=$($cPanelAfter.Found) C_PANEL_VISIBLE_AFTER=$($cPanelAfter.Visible) (expected hidden) C_PANEL_SEEN_VISIBLE=$($c.PanelSeen)"
foreach ($h in $cFocusOut) { Log "C_FOCUS_OUT_LINE=$($h.Value)" }
if ($cFocusIn.Count -lt 1) { Fail 'C1_FAILED (the profile activated FocusIn did not set presence=present)' }
if ($cFocusOut.Count -lt 1) { Fail 'C2_FAILED (no "focus_out param=1 presence=absent" line)' }
if ($cReclaim.Count -ne 0) { Fail 'C3_FAILED (the session was orphan reclaimed instead of ended normally)' }
if ($cOrphanKept.Count -ne 0) { Fail 'C4_FAILED (orphan_reclaim branch was taken on the normal path)' }
if ($null -eq $cEcho.Value) { Fail "C5_FAILED (ECHO after the session end was unparsable: $($cEcho.Raw))" }
elseif ($cEcho.Value -ne 0) { Fail "C6_FAILED (the session is still alive after END_SESSION: ECHO=$($cEcho.Value))" }
if (-not $c.PanelSeen) { Log 'WARN_C_PANEL_NEVER_VISIBLE (the panel visibility observation has no discriminating power in this phase)' }
if ($cPanelAfter.Found -and $cPanelAfter.Visible) { Fail 'C7_FAILED (the quick panel is still visible after the focus-out)' }

# =========================================================== phase D ========
# "user switched window" - the app focus lost focus-out must NOT flip the
# presence (spec 097: only a profile deactivation means "Fluxing is gone").
Log ''
Log '=== PHASE D: window switch (APP_FOCUS_LOST) ==='
$baseD = Get-LogBaseline
$d = Invoke-Client -Mode '--focusout-appfocus' -Marker (Join-Path $script:OutDir 'd.marker')
if ($d.Exit -ne 0) { Fail "PHASE_D_CLIENT_FAILED exit=$($d.Exit)" }
Start-Sleep -Seconds 1
$sliceD = Get-LogSlice -Baseline $baseD
$null = Save-Slice -Name 'D' -Content $sliceD
$dFocusOutKept = [regex]::Matches($sliceD, 'focus_out param=2 presence=kept')
$dFocusOutAbsent = [regex]::Matches($sliceD, 'focus_out param=1 presence=absent')
$dReclaim = [regex]::Matches($sliceD, 'reclaim orphan session ipc_id=' + $d.IpcId + '\b')
$dOrphanKept = [regex]::Matches($sliceD, 'remove_session active orphan_reclaim presence=kept')
Log "D_IPC_ID=$($d.IpcId)"
Log "D_FOCUS_OUT_KEPT count=$($dFocusOutKept.Count) (expected >= 1)"
Log "D_FOCUS_OUT_ABSENT count=$($dFocusOutAbsent.Count) (expected 0 - no profile deactivation here)"
Log "D_RECLAIM_LINES count=$($dReclaim.Count) (expected 0)"
Log "D_ORPHAN_KEPT_LINES count=$($dOrphanKept.Count) (expected 0)"
foreach ($h in $dFocusOutKept) { Log "D_FOCUS_OUT_LINE=$($h.Value)" }
if ($dFocusOutKept.Count -lt 1) { Fail 'D1_FAILED (no "focus_out param=2 presence=kept" line)' }
if ($dFocusOutAbsent.Count -ne 0) { Fail 'D2_FAILED (a profile deactivation appeared in the window switch scenario)' }
if ($dReclaim.Count -ne 0) { Fail 'D3_FAILED (the session was orphan reclaimed instead of ended normally)' }
if ($dOrphanKept.Count -ne 0) { Fail 'D4_FAILED (orphan_reclaim branch was taken on the normal path)' }

# =========================================================== phase E ========
# The orphan control: the SAME client and handshake, but no END_SESSION => the
# server must reclaim the session and must NOT flip the presence.
Log ''
Log '=== PHASE E: orphan control (no END_SESSION) ==='
$baseE = Get-LogBaseline
$e = Invoke-Client -Mode '--abrupt-exit' -Marker (Join-Path $script:OutDir 'e.marker')
if ($e.Exit -ne 0) { Fail "PHASE_E_CLIENT_FAILED exit=$($e.Exit)" }
Start-Sleep -Seconds 1
$eSweep = Invoke-Client -Mode '--sweep-trigger' -Marker (Join-Path $script:OutDir 'e-sweep.marker')
if ($eSweep.Exit -ne 0) { Fail "PHASE_E_SWEEP_FAILED exit=$($eSweep.Exit)" }
Start-Sleep -Seconds 2
$sliceE = Get-LogSlice -Baseline $baseE
$null = Save-Slice -Name 'E' -Content $sliceE
$eReclaim = [regex]::Matches($sliceE, 'reclaim orphan session ipc_id=' + $e.IpcId + ' client_pid=' + $e.Pid + ' live=\d+')
$eOrphanKept = [regex]::Matches($sliceE, 'remove_session active orphan_reclaim presence=kept')
$eAbsent = [regex]::Matches($sliceE, 'remove_session active presence=absent')
Log "E_IPC_ID=$($e.IpcId) E_HOST_PID=$($e.Pid) E_SWEEP_IPC_ID=$($eSweep.IpcId)"
Log "E_RECLAIM_LINE count=$($eReclaim.Count) (expected >= 1)"
Log "E_ORPHAN_KEPT_LINES count=$($eOrphanKept.Count) (expected >= 1)"
Log "E_ABSENT_LINES count=$($eAbsent.Count) (expected 0 - the orphan path must not flip presence)"
foreach ($h in $eReclaim) { Log "E_RECLAIM_LINE=$($h.Value)" }
foreach ($h in $eOrphanKept) { Log "E_ORPHAN_KEPT_LINE=$($h.Value)" }
if ($eReclaim.Count -lt 1) { Fail 'E1_FAILED (no "reclaim orphan session ipc_id=.. client_pid=.. live=.." line)' }
if ($eOrphanKept.Count -lt 1) { Fail 'E2_FAILED (no "remove_session active orphan_reclaim presence=kept" line)' }
if ($eAbsent.Count -ne 0) { Fail "E3_FAILED ($($eAbsent.Count) presence=absent lines on the orphan path)" }

# Cleanup: the sweep trigger session was deliberately left open so that the
# phase E log slice stays clean. End it now so no session is left behind
# (a resident session would keep the maintenance gate closed).
if ($null -ne $eSweep.IpcId) {
  $out = & $script:Tool --end-session-by-id $eSweep.IpcId 2>&1 | Out-String
  Log "E_CLEANUP=$($out.Trim())"
}

# ============================================================ verdict ======
Log ''
Log "FAILURE_COUNT=$($script:Failures.Count)"
foreach ($f in $script:Failures) { Log "FAILED=$f" }
if ($script:Failures.Count -eq 0) {
  Log 'B4_VERDICT=PASS_SERVER_PATH'
  Log 'EXIT=0'
  exit 0
}
Log 'B4_VERDICT=PARTIAL'
Log 'EXIT=4'
exit 4
