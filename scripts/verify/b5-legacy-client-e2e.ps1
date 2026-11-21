#Requires -Version 7.0
<#
.SYNOPSIS
  B-5 end-to-end: an OLD client (no session.client_pid line) must never have
  its orphaned session reclaimed, while a modern client's orphan in the same
  table must be reclaimed - in ONE run, against the real running server.

.DESCRIPTION
  Criterion (docs/release/restart-checklist-1.3.6.md section 2, item B-5):
    "an old client (does not write session.client_pid=) => the server treats
     the pid as unknown and does NOT reclaim; it must not be killed by mistake"

  Discriminating pair (neither half proves anything alone):
    POSITIVE  modern client --abrupt-exit  (session M, pid known, process dead)
              => the next AddSession sweeps and reclaims M
              => log line "reclaim orphan session ipc_id=M client_pid=<pM> live=..."
                 plus "remove_session active orphan_reclaim presence=kept"
    NEGATIVE  old client    --abrupt-exit -Legacy  (session L, no pid line)
              => the very same sweep must NOT reclaim L
              => zero lines matching "reclaim orphan session ipc_id=L "
              => zero lines with "client_pid=<pL>"
              => ECHO=L stays non-zero (session still on the server)
              => no "presence=absent" line in the whole run (no false flip)

  Third phase makes the negative criterion non-vacuous: after the modern
  trigger sessions are ended, ONE more sweep trigger runs while the ONLY
  foreign session left in the table is the legacy orphan L.  The sweep
  demonstrably executes (map size >= 2, AddSession calls it unconditionally
  outside a maintenance window) and produces ZERO victims.

  ipc_id / pid come from the client's own marker files (keys ipc_id, pid) -
  no new server log field was invented for this probe.

  Side observation (recorded, never a FAIL criterion): a leftover legacy
  orphan session must not block the cloud-sync gate forever; the export file
  mtime must keep advancing while L is still registered.

  Nothing here starts, stops, restarts or installs a server.  All sessions
  this probe creates are cleaned up with --end-session-by-id.

  Exit codes:
    0  B5_VERDICT=PASS_E2E
    4  B5_VERDICT=FAIL            (a criterion was violated)
    5  B5_VERDICT=ABORT           (environment unusable)
    6  B5_VERDICT=INCONCLUSIVE    (no reclaim was ever observed => the method
                                   has no teeth on this machine, nothing proven)
#>

$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Tool = Join-Path $RepoRoot 'Release\TestFocusOutE2E.exe'
$ToolCpp = Join-Path $RepoRoot 'test\TestOrphanReclaimE2E\TestFocusOutE2E.cpp'
$DiagLog = Join-Path $env:TEMP 'fluxing-profile-notify.log'
$OutRoot = Join-Path $RepoRoot 'reports\fluxing-bb-orphan-reclaim'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$OutDir = Join-Path $OutRoot "b5-e2e-$stamp"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$ConsoleFile = Join-Path $OutDir 'console.txt'

$script:Failures = 0
$script:AbortReason = $null
$script:ExportFile = $null

function Log {
  param([string]$Text)
  Write-Host $Text
  Add-Content -LiteralPath $ConsoleFile -Value $Text -Encoding utf8
}

function Add-Fail {
  param([string]$Reason)
  $script:Failures++
  Log "FAIL=$Reason"
}

function Emit-Verdict {
  param([string]$Verdict, [int]$Code)
  Log "FAILURE_COUNT=$script:Failures"
  Log "B5_VERDICT=$Verdict"
  Log "EXIT=$Code"
  Set-Content -LiteralPath (Join-Path $OutDir 'verdict.txt') -Value @(
    "B5_VERDICT=$Verdict", "FAILURE_COUNT=$script:Failures", "EXIT=$Code",
    "OUT_DIR=$OutDir") -Encoding utf8
  exit $Code
}

function Abort {
  param([string]$Reason, [int]$Code = 5)
  Log "ABORT=$Reason"
  Emit-Verdict -Verdict 'ABORT' -Code $Code
}

# ------------------------------------------------------------ primitives --
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public static class FluxB5Win {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int max);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);

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
    var sb = new StringBuilder(256); GetClassNameW(h, sb, sb.Capacity); return sb.ToString();
  }
  public static string TitleOf(IntPtr h) {
    var sb = new StringBuilder(512); GetWindowTextW(h, sb, sb.Capacity); return sb.ToString();
  }
  public static string TextOf(IntPtr h) {
    var sb = new StringBuilder(2048);
    SendMessageGetText(h, sb);
    return sb.ToString();
  }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)]
  static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr w, StringBuilder l);
  static void SendMessageGetText(IntPtr h, StringBuilder sb) {
    SendMessageW(h, 0x000D, (IntPtr)sb.Capacity, sb);
    if (sb.Length == 0) GetWindowTextW(h, sb, sb.Capacity);
  }
}
'@

function Get-SharedText {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
                               [System.IO.FileAccess]::Read,
                               [System.IO.FileShare]::ReadWrite)
  try {
    $sr = New-Object System.IO.StreamReader($fs, [System.Text.Encoding]::UTF8)
    $txt = $sr.ReadToEnd()
    $sr.Dispose()
    return $txt
  } finally { $fs.Close() }
}

function Get-LogBaseline {
  if (-not (Test-Path -LiteralPath $DiagLog)) {
    Abort "DIAG_LOG_MISSING ($DiagLog)" 5
  }
  return [long](Get-Item -LiteralPath $DiagLog).Length
}

function Get-LogSlice {
  param([long]$From)
  $item = Get-Item -LiteralPath $DiagLog
  if ($item.Length -lt $From) { return $null }   # rotated: baseline invalid
  $len = [int]($item.Length - $From)
  if ($len -le 0) { return '' }
  $bytes = New-Object byte[] $len
  $fs = [System.IO.File]::Open($DiagLog, [System.IO.FileMode]::Open,
                               [System.IO.FileAccess]::Read,
                               [System.IO.FileShare]::ReadWrite)
  try {
    $null = $fs.Seek($From, [System.IO.SeekOrigin]::Begin)
    $read = $fs.Read($bytes, 0, $len)
  } finally { $fs.Close() }
  if ($read -lt $len) { $bytes = $bytes[0..($read - 1)] }
  return [System.Text.Encoding]::UTF8.GetString($bytes)
}

function Read-MarkerValue {
  param([string]$Path, [string]$Key, [int]$TimeoutSec = 15)
  $end = (Get-Date).AddSeconds($TimeoutSec)
  while ((Get-Date) -lt $end) {
    $raw = Get-SharedText -Path $Path
    if ($raw -and $raw.Length -gt 0) {
      $m = [regex]::Match($raw, [regex]::Escape($Key) + '=(\d+)')
      if ($m.Success) { return [long]$m.Groups[1].Value }
    }
    Start-Sleep -Milliseconds 100
  }
  return $null
}

function Invoke-Client {
  param([string[]]$ToolArgs, [string]$Tag, [int]$TimeoutSec = 90)
  $so = Join-Path $OutDir "$Tag.stdout.txt"
  $se = Join-Path $OutDir "$Tag.stderr.txt"
  $p = Start-Process -FilePath $Tool -ArgumentList $ToolArgs -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput $so -RedirectStandardError $se
  $end = (Get-Date).AddSeconds($TimeoutSec)
  while ((Get-Date) -lt $end -and -not $p.HasExited) { Start-Sleep -Milliseconds 150 }
  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Log "$Tag TIMEOUT_KILLED pid=$($p.Id)"
  }
  try { $null = $p.WaitForExit(5000) } catch { }
  $exit = if ($p.HasExited) { $p.ExitCode } else { -1 }
  Start-Sleep -Milliseconds 200
  $out = (Get-SharedText -Path $so) + (Get-SharedText -Path $se)
  Log ("{0} exit={1} pid={2} argv=[{3}]" -f $Tag, $exit, $p.Id, ($ToolArgs -join ' '))
  foreach ($line in ($out -split "`r?`n")) {
    if ($line.Trim().Length -gt 0) { Log "  $Tag | $line" }
  }
  return [pscustomobject]@{ Exit = $exit; Out = $out }
}

function Invoke-Echo {
  param([long]$SessionId, [string]$Tag)
  $r = Invoke-Client -ToolArgs @('--echo', "$SessionId") -Tag "echo-$Tag" -TimeoutSec 30
  $m = [regex]::Match($r.Out, 'ECHO=(\d+)')
  if (-not $m.Success) { return $null }
  return [long]$m.Groups[1].Value
}

function Get-ReclaimLineCount {
  param([string]$Text, [long]$IpcId)
  if ([string]::IsNullOrEmpty($Text)) { return 0 }
  # The production format is always "ipc_id=<n> client_pid=", so the trailing
  # space makes the id match exact (id 12 must not match 123).
  return [regex]::Matches($Text, "reclaim orphan session ipc_id=$IpcId\b ").Count
}

function Get-ReclaimLinesByText {
  param([string]$Text)
  if ([string]::IsNullOrEmpty($Text)) { return @() }
  return @([regex]::Matches($Text, 'reclaim orphan session ipc_id=\d+ client_pid=\d+ live=\d+') |
           ForEach-Object { $_.Value })
}

function Get-ExportStamp {
  if ($null -eq $script:ExportFile) { return [long]::MinValue }
  if (-not (Test-Path -LiteralPath $script:ExportFile)) { return [long]::MinValue }
  return [long](Get-Item -LiteralPath $script:ExportFile).LastWriteTimeUtc.Ticks
}

# ------------------------------------------------------------------ setup --
Log "REPO_ROOT=$RepoRoot"
Log "OUT_DIR=$OutDir"
Log "TOOL=$Tool"
Log "CPP=$ToolCpp"

if (-not (Test-Path -LiteralPath $Tool)) { Abort 'TOOL_MISSING (build test/TestOrphanReclaimE2E/TestFocusOutE2E.vcxproj first)' 5 }
if ((Get-Item -LiteralPath $Tool).LastWriteTime -lt (Get-Item -LiteralPath $ToolCpp).LastWriteTime) {
  Abort 'TOOL_STALE (exe older than TestFocusOutE2E.cpp => the -Legacy flag is not in the binary)' 5
}
$servers = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
if ($servers.Count -eq 0) { Abort 'SERVER_NOT_RUNNING (B-5 must run against the real server)' 5 }
foreach ($s in $servers) {
  Log ("SERVER pid={0} path={1} ver={2}" -f $s.Id, $s.Path, $s.MainModule.FileVersionInfo.FileVersion)
}
$serverPid = [uint32]$servers[0].Id

$diagItem = Get-Item -LiteralPath $DiagLog -ErrorAction SilentlyContinue
if ($null -eq $diagItem) { Abort "DIAG_LOG_MISSING ($DiagLog)" 5 }
if ($diagItem.Length -gt 1900000) {
  Log "WARN_DIAG_LOG_NEAR_CAP=$($diagItem.Length) (fluxing::diag stops appending at 2 MB => slices may be empty)"
}
$runBaseline = Get-LogBaseline
Log "LOG_BASELINE_BYTES=$runBaseline"
Log ("TOOL_BYTES={0} TOOL_MTIME={1}" -f (Get-Item -LiteralPath $Tool).Length,
     (Get-Item -LiteralPath $Tool).LastWriteTime)

# --------------------------------------------- phase 1: legacy orphan (L) --
Log '=== PHASE 1: legacy client abrupt exit (no session.client_pid line) ==='
$legacyMarker = Join-Path $OutDir 'legacy.marker'
$p1 = Invoke-Client -ToolArgs @('--abrupt-exit', $legacyMarker, '-Legacy') -Tag 'legacy-orphan'
if ($p1.Exit -ne 0) { Add-Fail "LEGACY_CLIENT_EXIT=$($p1.Exit)" }
if ($p1.Out -notmatch 'CLIENT_MODE=legacy handshake_client_pid_line=0') {
  Add-Fail 'LEGACY_FLAG_NOT_IN_EFFECT (no CLIENT_MODE=legacy line => the handshake still carried the pid and the negative test would be vacuous)'
}
$idL = Read-MarkerValue -Path $legacyMarker -Key 'ipc_id'
$pidL = Read-MarkerValue -Path $legacyMarker -Key 'pid'
if ($null -eq $idL) { Abort 'LEGACY_MARKER_NO_IPC_ID' 5 }
if ($null -eq $pidL) { Abort 'LEGACY_MARKER_NO_PID' 5 }
Log "LEGACY_IPC_ID=$idL LEGACY_CLIENT_PID=$pidL"
$echoL0 = Invoke-Echo -SessionId $idL -Tag 'legacy-before'
if ($null -eq $echoL0) { Add-Fail 'LEGACY_ECHO_BEFORE_UNPARSED' }
elseif ($echoL0 -eq 0) { Add-Fail "LEGACY_ECHO_BEFORE=0 (the legacy session was already gone before any sweep => nothing to protect; ipc_id=$idL)" }
else { Log "LEGACY_ECHO_BEFORE=$echoL0 (session really is registered on the server)" }

# --------------------------------------------- phase 2: modern orphan (M) --
Log '=== PHASE 2: modern client abrupt exit (pid reported) ==='
$modernMarker = Join-Path $OutDir 'modern.marker'
$p2 = Invoke-Client -ToolArgs @('--abrupt-exit', $modernMarker) -Tag 'modern-orphan'
if ($p2.Exit -ne 0) { Add-Fail "MODERN_CLIENT_EXIT=$($p2.Exit)" }
if ($p2.Out -notmatch 'CLIENT_MODE=modern handshake_client_pid_line=1') {
  Add-Fail 'MODERN_MODE_FLAG_MISSING (no CLIENT_MODE=modern line)'
}
$idM = Read-MarkerValue -Path $modernMarker -Key 'ipc_id'
$pidM = Read-MarkerValue -Path $modernMarker -Key 'pid'
if ($null -eq $idM) { Abort 'MODERN_MARKER_NO_IPC_ID' 5 }
if ($null -eq $pidM) { Abort 'MODERN_MARKER_NO_PID' 5 }
Log "MODERN_IPC_ID=$idM MODERN_CLIENT_PID=$pidM"
if ($idM -eq $idL) { Add-Fail 'MODERN_AND_LEGACY_SAME_IPC_ID (ids not separable => the negative match would be ambiguous)' }
$echoM0 = Invoke-Echo -SessionId $idM -Tag 'modern-before'
if ($null -eq $echoM0) { Add-Fail 'MODERN_ECHO_BEFORE_UNPARSED' }
elseif ($echoM0 -eq 0) { Add-Fail "MODERN_ECHO_BEFORE=0 (the modern orphan is already gone => the positive control has nothing to reclaim)" }
else { Log "MODERN_ECHO_BEFORE=$echoM0" }

# ------------------------------ phase 3: sweep trigger => reclaim of M only --
Log '=== PHASE 3: sweep trigger (AddSession) => positive control ==='
$positiveFound = $false
$positiveText = ''
$sweepText = ''
$triggerIds = @()
for ($attempt = 1; $attempt -le 4; $attempt++) {
  $baseline = Get-LogBaseline
  $tMarker = Join-Path $OutDir "trigger$attempt.marker"
  $pt = Invoke-Client -ToolArgs @('--sweep-trigger', $tMarker) -Tag "sweep-trigger-$attempt"
  $tid = Read-MarkerValue -Path $tMarker -Key 'ipc_id'
  if ($null -ne $tid) { $triggerIds += $tid } else { Log "TRIGGER${attempt}_IPC_ID=unknown" }
  Start-Sleep -Seconds 2
  $slice = Get-LogSlice -From $baseline
  if ($null -eq $slice) {
    Log "SLICE_ROTATED attempt=$attempt"
    $slice = ''
  }
  $sweepText += $slice
  $hits = Get-ReclaimLineCount -Text $slice -IpcId $idM
  Log "SWEEP_ATTEMPT=$attempt slice_bytes=$($slice.Length) reclaim_for_modern=$hits"
  if ($hits -gt 0) {
    $positiveFound = $true
    $positiveText = $slice
    Set-Content -LiteralPath (Join-Path $OutDir 'evidence-positive.txt') -Value $slice -Encoding utf8
    Log "POSITIVE_CONTROL=OBSERVED attempt=$attempt ipc_id=$idM"
    break
  }
}
if (-not $positiveFound) {
  Log 'POSITIVE_CONTROL=NOT_OBSERVED (no sweep reclaimed the modern orphan within 4 attempts => the negative result carries no information)'
}

# --------------------------------- phase 4: the negative criterion (B-5) ---
Log '=== PHASE 4: negative criterion - the legacy orphan must survive ==='
$negCount = Get-ReclaimLineCount -Text $sweepText -IpcId $idL
Log "RECLAIM_LINES_FOR_LEGACY=$negCount (must be 0)"
if ($negCount -gt 0) { Add-Fail "LEGACY_SESSION_RECLAIMED ipc_id=$idL (=> the server killed an old client's session = the exact defect B-5 forbids)" }
$pidHit = 0
if ($sweepText) { $pidHit = [regex]::Matches($sweepText, "client_pid=$pidL\b").Count }
Log "RECLAIM_LINES_CARRYING_LEGACY_PID=$pidHit (must be 0)"
if ($pidHit -gt 0) { Add-Fail "LEGACY_PID_APPEARS_IN_RECLAIM_LINE pid=$pidL" }
$orphanKeptLines = 0
if ($sweepText) { $orphanKeptLines = [regex]::Matches($sweepText, 'remove_session active orphan_reclaim presence=kept').Count }
Log "ORPHAN_RECLAIM_PRESENCE_KEPT_LINES=$orphanKeptLines"
if ($positiveFound -and $orphanKeptLines -eq 0) {
  Add-Fail 'ORPHAN_RECLAIM_LOG_BRANCH_MISSING (a reclaim happened without the presence=kept branch line => cannot show presence was preserved)'
}
$absentCount = 0
if ($sweepText) { $absentCount = [regex]::Matches($sweepText, 'presence=absent').Count }
Log "PRESENCE_ABSENT_LINES=$absentCount (must be 0: a reclaim must never read as 'Fluxing left')"
if ($absentCount -gt 0) { Add-Fail "PRESENCE_FLIPPED_BY_RECLAIM count=$absentCount" }
$echoL1 = Invoke-Echo -SessionId $idL -Tag 'legacy-after-sweep'
if ($null -eq $echoL1) { Add-Fail 'LEGACY_ECHO_AFTER_UNPARSED' }
elseif ($echoL1 -eq 0) { Add-Fail "LEGACY_ECHO_AFTER=0 (the legacy session is gone on the server although no reclaim line names it)" }
else { Log "LEGACY_ECHO_AFTER=$echoL1 (still alive => not reclaimed, second evidence channel)" }
$echoM1 = Invoke-Echo -SessionId $idM -Tag 'modern-after-sweep'
if ($null -eq $echoM1) { Log 'MODERN_ECHO_AFTER=unparsed' }
elseif ($positiveFound) {
  if ($echoM1 -ne 0) { Add-Fail "MODERN_ECHO_AFTER=$echoM1 (the modern orphan survived the sweep although a reclaim line was logged)" }
  else { Log 'MODERN_ECHO_AFTER=0 (reclaimed => positive control confirmed on the second channel)' }
}

# ------------- phase 5: bare sweep with ONLY the legacy orphan in the table --
Log '=== PHASE 5: sweep while the legacy orphan is the only foreign session ==='
foreach ($tid in $triggerIds) {
  $null = Invoke-Client -ToolArgs @('--end-session-by-id', "$tid") -Tag "cleanup-trigger-$tid" -TimeoutSec 30
}
$slice5Baseline = Get-LogBaseline
$t2Marker = Join-Path $OutDir 'trigger-bare.marker'
$pt2 = Invoke-Client -ToolArgs @('--sweep-trigger', $t2Marker) -Tag 'sweep-trigger-bare'
$idT2 = Read-MarkerValue -Path $t2Marker -Key 'ipc_id'
Log "BARE_TRIGGER_IPC_ID=$idT2"
Start-Sleep -Seconds 2
$slice5 = Get-LogSlice -From $slice5Baseline
if ($null -eq $slice5) { $slice5 = '' }
Set-Content -LiteralPath (Join-Path $OutDir 'evidence-negative.txt') -Value $slice5 -Encoding utf8
$all5 = Get-ReclaimLinesByText -Text $slice5
Log "BARE_SWEEP_RECLAIM_LINES=$($all5.Count)"
if ($all5.Count -gt 0) {
  Add-Fail "BARE_SWEEP_RECLAIMED_SOMETHING ($($all5.Count) line(s) while only the legacy orphan was a candidate: $($all5 -join ' | '))"
}
$echoL2 = Invoke-Echo -SessionId $idL -Tag 'legacy-after-bare-sweep'
if ($null -eq $echoL2) { Add-Fail 'LEGACY_ECHO_BARE_UNPARSED' }
elseif ($echoL2 -eq 0) { Add-Fail 'LEGACY_ECHO_BARE=0 (the legacy orphan vanished during the bare sweep)' }
else { Log "LEGACY_ECHO_BARE=$echoL2 (survived a sweep that had it as its only candidate)" }
if ($idT2) { $triggerIds += $idT2 }

# ------ phase 6: side observation (never a FAIL): the gate is not blocked ----
Log '=== PHASE 6: side observation - legacy residual vs the sync gate ==='
$gateObs = 'SKIPPED'
try {
  $userDir = $null
  try {
    $userDir = [string](Get-ItemProperty -LiteralPath 'HKCU:\Software\Fluxing\Weasel' -Name 'RimeUserDir' -ErrorAction Stop).RimeUserDir
  } catch { $userDir = $null }
  if ([string]::IsNullOrWhiteSpace($userDir)) { $userDir = 'D:\Program Files\fluxing\user1\fluxing' }
  $cands = @(Get-ChildItem -LiteralPath (Join-Path $userDir 'sync') -Recurse -File -Filter '*.userdb.txt' -ErrorAction SilentlyContinue)
  if ($cands.Count -eq 0) {
    $gateObs = 'SKIPPED_NO_EXPORT_FILE'
    Log "GATE_OBS=$gateObs (no sync export under $userDir\sync => logged, not a criterion)"
  } else {
    $script:ExportFile = ($cands | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
    Log "GATE_EXPORT_FILE=$($script:ExportFile)"
    $AccountClass = 'FluxingCloudAccountDialog_v1'
    $wins = [FluxB5Win]::TopLevelOfPid($serverPid)
    $account = $null
    foreach ($w in $wins) { if ([FluxB5Win]::ClassOf($w) -eq $AccountClass) { $account = $w; break } }
    $openedByUs = $false
    if ($null -eq $account) {
      foreach ($w in $wins) {
        [void][FluxB5Win]::PostMessageW($w, [uint32]0x0111, [IntPtr]40024, [IntPtr]0)
        Start-Sleep -Milliseconds 1200
        foreach ($c in [FluxB5Win]::TopLevelOfPid($serverPid)) {
          if ([FluxB5Win]::ClassOf($c) -eq $AccountClass) { $account = $c; $openedByUs = $true; break }
        }
        if ($account) { break }
      }
    }
    if ($null -eq $account) {
      $gateObs = 'SKIPPED_NO_ACCOUNT_WINDOW'
      Log "GATE_OBS=$gateObs (the account window did not appear => no way to trigger a round)"
    } else {
      Log ("GATE_ACCOUNT_HWND=0x{0:X} opened_by_us={1} title='{2}'" -f $account.ToInt64(), $openedByUs, [FluxB5Win]::TitleOf($account))
      Log 'GATE_ENGINE_SETTLE_SEC=30'
      Start-Sleep -Seconds 30
      $ctl = Get-ExportStamp
      [void][FluxB5Win]::PostMessageW($account, [uint32]0x0111, [IntPtr]1016, [IntPtr]0)
      $end = (Get-Date).AddSeconds(150); $latCtl = -1
      while ((Get-Date) -lt $end) { if ((Get-ExportStamp) -gt $ctl) { $latCtl = [math]::Round(((Get-Date) - $end.AddSeconds(-150)).TotalSeconds, 1); break }; Start-Sleep -Milliseconds 400 }
      if ($latCtl -lt 0) {
        $gateObs = 'INCONCLUSIVE_CONTROL_ROUND_DID_NOT_EXPORT'
        Log "GATE_OBS=$gateObs (even with no gate-relevant session of ours the export never advanced => the trigger/login/export path is unusable here)"
      } else {
        Log "GATE_CONTROL_EXPORT_ADVANCED_SEC=$latCtl"
        Log "GATE_LEGACY_IPC_ID_STILL_ALIVE=$(if ($echoL2) { $echoL2 } else { 'unknown' })"
        $obs = Get-ExportStamp
        [void][FluxB5Win]::PostMessageW($account, [uint32]0x0111, [IntPtr]1016, [IntPtr]0)
        $end2 = (Get-Date).AddSeconds(90); $latObs = -1
        while ((Get-Date) -lt $end2) { if ((Get-ExportStamp) -gt $obs) { $latObs = [math]::Round(((Get-Date) - $end2.AddSeconds(-90)).TotalSeconds, 1); break }; Start-Sleep -Milliseconds 400 }
        if ($latObs -ge 0) {
          $gateObs = 'ADVANCED_WITH_LEGACY_RESIDUAL'
          Log "GATE_OBS=$gateObs after_sec=$latObs (the leftover legacy orphan session does not block the round)"
        } else {
          $gateObs = 'BLOCKED_WITH_LEGACY_RESIDUAL'
          Log "GATE_OBS=$gateObs (no export within 90 s while L was registered)"
          $null = Invoke-Client -ToolArgs @('--end-session-by-id', "$idL") -Tag 'cleanup-legacy-for-gate' -TimeoutSec 30
          $obs2 = Get-ExportStamp
          [void][FluxB5Win]::PostMessageW($account, [uint32]0x0111, [IntPtr]1016, [IntPtr]0)
          $end3 = (Get-Date).AddSeconds(120); $latObs2 = -1
          while ((Get-Date) -lt $end3) { if ((Get-ExportStamp) -gt $obs2) { $latObs2 = [math]::Round(((Get-Date) - $end3.AddSeconds(-120)).TotalSeconds, 1); break }; Start-Sleep -Milliseconds 400 }
          if ($latObs2 -ge 0) {
            $gateObs += '_AND_ADVANCES_AFTER_END_SESSION (not permanent)'
            Log "GATE_RESOLVED_AFTER_END_SESSION_SEC=$latObs2"
          } else { Log 'GATE_STILL_NOT_ADVANCING_AFTER_END_SESSION (inconclusive)' }
        }
      }
      if ($openedByUs) {
        [void][FluxB5Win]::PostMessageW($account, [uint32]0x0010, [IntPtr]0, [IntPtr]0)
        Log 'GATE_ACCOUNT_WINDOW_CLOSED_BY_PROBE=1'
      }
    }
  }
} catch {
  $gateObs = "ERROR $($_.Exception.Message)"
  Log "GATE_OBS=$gateObs"
}

# --------------------------- phase 7: how the legacy orphan actually ends ---
Log '=== PHASE 7: cleanup - only an explicit END_SESSION clears a legacy orphan ==='
$echoL3 = Invoke-Echo -SessionId $idL -Tag 'legacy-before-end'
if ($null -ne $echoL3 -and $echoL3 -eq 0) {
  Log "LEGACY_GONE_WITHOUT_END_SESSION=1 (echo=0 before we ended it => something else removed ipc_id=$idL)"
}
$pe = Invoke-Client -ToolArgs @('--end-session-by-id', "$idL") -Tag 'end-legacy-orphan' -TimeoutSec 30
if ($pe.Exit -ne 0) { Add-Fail "END_LEGACY_EXIT=$($pe.Exit)" }
Start-Sleep -Seconds 1
$echoL4 = Invoke-Echo -SessionId $idL -Tag 'legacy-after-end'
if ($null -eq $echoL4) { Add-Fail 'LEGACY_ECHO_END_UNPARSED' }
else { Log "LEGACY_ECHO_AFTER_END_SESSION=$echoL4 (0 => END_SESSION is what finishes a legacy orphan; ADR 0067 never does)" }
foreach ($tid in ($triggerIds | Select-Object -Unique)) {
  $null = Invoke-Client -ToolArgs @('--end-session-by-id', "$tid") -Tag "final-cleanup-$tid" -TimeoutSec 30
}

# ------------------------------------------------------------- evidence -----
$runSlice = Get-LogSlice -From $runBaseline
if ($null -eq $runSlice) {
  Log 'RUN_SLICE=ROTATED (writing whatever the per-attempt buffers hold instead)'
  $runSlice = $sweepText + $slice5
}
Set-Content -LiteralPath (Join-Path $OutDir 'log-slice.txt') -Value $runSlice -Encoding utf8
$allReclaim = Get-ReclaimLinesByText -Text $runSlice
Log "RUN_RECLAIM_LINES=$($allReclaim.Count)"
foreach ($l in $allReclaim) { Log "  $l" }
$handshake = @(
  "LEGACY_IPC_ID=$idL LEGACY_PID=$pidL ECHO_before=$echoL0 ECHO_after_sweep=$echoL1 ECHO_after_bare=$echoL2 ECHO_after_end=$echoL4"
  "MODERN_IPC_ID=$idM MODERN_PID=$pidM ECHO_before=$echoM0 ECHO_after_sweep=$echoM1"
  "POSITIVE_CONTROL=$positiveFound RECLAIM_LINES_FOR_LEGACY=$negCount LEGACY_PID_LINES=$pidHit PRESENCE_ABSENT=$absentCount"
  "GATE_OBS=$gateObs"
)
Set-Content -LiteralPath (Join-Path $OutDir 'summary.txt') -Value $handshake -Encoding utf8

# --------------------------------------------------------------- verdict ----
if ($script:Failures -gt 0) {
  Emit-Verdict -Verdict 'FAIL' -Code 4
} elseif (-not $positiveFound) {
  Emit-Verdict -Verdict 'INCONCLUSIVE' -Code 6
} else {
  Emit-Verdict -Verdict 'PASS_E2E' -Code 0
}
