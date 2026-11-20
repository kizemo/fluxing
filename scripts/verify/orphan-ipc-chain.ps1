#Requires -Version 7.0
<#
.SYNOPSIS
  End-to-end verification of ADR 0067 orphan IPC session reclaim, driven through
  the real, already running WeaselServer.exe over the real named pipe.

.DESCRIPTION
  This script never starts or stops the server. It only drives a synthetic IPC
  client (test/TestOrphanReclaimE2E) against the running server:

    1. locate the tool and the running server, refuse to run otherwise
    2. record the diagnostic log baseline offset
    3. start "--hold": the client opens a session, types letters, then sleeps
       forever. It never sends END_SESSION.
    4. read the ipc_id of that session from the marker file
    5. teeth check: ECHO of the held session must still be non zero, i.e. the
       session is really alive, so that a later ECHO=0 carries information
    6. kill the hold process (this simulates a crashed TSF host)
    7. start "--trigger": a new session is created, which makes the server run
       its lazy orphan sweep
    8. ECHO of the held session must now be 0 (the session was reclaimed)
    9. read the diagnostic log slice and check the three criteria lines

  Exit codes:
    0  VERDICT=ORPHAN_RECLAIM_OK
    4  VERDICT=ORPHAN_RECLAIM_FAIL  (the chain ran but a criterion failed)
    5  ABORT                        (environment unusable: no tool, no server,
                                    no diagnostic log, rotated log, ...)
#>

$ErrorActionPreference = 'Stop'

$script:RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Tool = Join-Path $script:RepoRoot 'Release\TestOrphanReclaimE2E.exe'
$script:DiagLog = Join-Path $env:TEMP 'fluxing-profile-notify.log'
$script:OutRoot = Join-Path $script:RepoRoot 'reports\fluxing-bb-orphan-reclaim'

$script:OutDir = $null
$script:ConsoleFile = $null

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
  Log 'VERDICT=ORPHAN_RECLAIM_FAIL'
  Log "EXIT=$Code"
  exit $Code
}

function Fail {
  param([string]$Reason)
  Log "FAIL=$Reason"
  Log 'VERDICT=ORPHAN_RECLAIM_FAIL'
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
  Log "ECHO_RAW=$text (exit=$code)"
  $m = [regex]::Match($text, 'ECHO=(\d+)')
  if (-not $m.Success) {
    return [pscustomobject]@{ Raw = $text; Value = $null; Exit = $code }
  }
  return [pscustomobject]@{ Raw = $text; Value = [long]$m.Groups[1].Value; Exit = $code }
}

# ---------------------------------------------------------------- setup ----
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:OutDir = Join-Path $script:OutRoot "ipc-chain-$stamp"
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
  Log "SERVER pid=$($sp.Id) path=$($sp.Path)"
}

if (-not (Test-Path -LiteralPath $script:DiagLog)) {
  Abort "DIAG_LOG_MISSING ($script:DiagLog)" 5
}
$baseline = (Get-Item -LiteralPath $script:DiagLog).Length
Log "LOG_BASELINE_BYTES=$baseline"

# ------------------------------------------------------------ step 1: hold ---
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

# --------------------------------------------- step 2: teeth check (alive) ---
$echoBefore = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoBefore.Value) {
  Fail "ECHO_BEFORE_UNPARSED raw='$($echoBefore.Raw)'"
}
if ($echoBefore.Value -eq 0) {
  Fail 'ECHO_BEFORE_ZERO (held session was already gone before the reclaim)'
}
Log "ECHO_BEFORE=$($echoBefore.Value) (non zero => session really alive)"

# ------------------------------------------------- step 3: kill the holder ---
Stop-Process -Id $hold.Id -Force
try {
  Wait-Process -Id $hold.Id -Timeout 15 -ErrorAction SilentlyContinue
} catch {
  # already gone
}
Start-Sleep -Seconds 1
if ($null -ne (Get-Process -Id $hold.Id -ErrorAction SilentlyContinue)) {
  Fail 'HOLD_STILL_ALIVE (could not kill the hold process)'
}
Log 'HOLD_KILLED=1'

# ------------------------------------------- step 4: trigger a new session ---
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
Log "TRIGGER_STDOUT=$(Get-Content -LiteralPath $triggerStdout -Raw -ErrorAction SilentlyContinue)"
$triggerId = Read-MarkerValue -Path $triggerMarker -Key 'ipc_id'
Log "TRIGGER_IPC_ID=$triggerId"

Start-Sleep -Seconds 2

# ----------------------------------------------- step 5: echo after reclaim ---
$echoAfter = Invoke-Echo -SessionId $orphanId
if ($null -eq $echoAfter.Value) {
  Fail "ECHO_AFTER_UNPARSED raw='$($echoAfter.Raw)'"
}
if ($echoAfter.Value -ne 0) {
  Fail "ECHO_AFTER_NOT_ZERO value=$($echoAfter.Value) (orphan session was not reclaimed)"
}
Log 'ECHO_AFTER=0 (orphan session is gone on the server)'

# ------------------------------------------------- step 6: read log slice ---
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
if ($read -lt $sliceBytes.Length) {
  $sliceBytes = $sliceBytes[0..($read - 1)]
}
$slice = [System.Text.Encoding]::UTF8.GetString($sliceBytes)
$sliceFile = Join-Path $script:OutDir 'log-slice.txt'
Set-Content -LiteralPath $sliceFile -Value $slice -Encoding utf8
Log "LOG_SLICE_BYTES=$($sliceBytes.Length)"
Log "LOG_SLICE_FILE=$sliceFile"

# ------------------------------------------------------- step 7: criteria ---
# B-2 的字面量必须**逐字**成立: ipc_id / client_pid / live 三个字段都在,
# 且 client_pid 必须等于**被强杀的那个宿主进程**的 pid。
# 只 grep 前缀会假绿 —— 1.3.7 的服务端本来就写 'reclaim orphan session ipc_id=<n>',
# 缺 client_pid/live 时前缀 grep 照样命中(这正是 1.3.8 要补的东西)。
if ($null -eq $orphanPid) {
  Fail 'ORPHAN_PID_UNKNOWN (hold.marker has no pid=, cannot check client_pid=)'
}
$reclaimPattern = 'reclaim orphan session ipc_id=' + $orphanId +
                   ' client_pid=' + $orphanPid + ' live=\d+'
$reclaimHits = [regex]::Matches($slice, $reclaimPattern)
# 弱判据只用于区分失败原因(压根没回收 vs 回收了但格式不对),不参与判定。
$reclaimPrefixHits = [regex]::Matches(
  $slice, 'reclaim orphan session ipc_id=' + $orphanId + '\b')
$keptHits = [regex]::Matches($slice, 'remove_session active orphan_reclaim presence=kept')
$absentHits = [regex]::Matches($slice, 'remove_session active presence=absent')

Log "CRITERION_1_RECLAIM_LINE count=$($reclaimHits.Count) pattern='$reclaimPattern'"
Log "INFO_RECLAIM_PREFIX_ONLY count=$($reclaimPrefixHits.Count) (prefix-only hits; if > 0 but CRITERION_1 = 0 the line lacks client_pid/live)"
Log "CRITERION_2_ORPHAN_PRESENCE_KEPT count=$($keptHits.Count)"
Log "CRITERION_3_NO_ABSENT_FLIP count=$($absentHits.Count) (must be 0)"
foreach ($h in $reclaimHits) {
  Log "RECLAIM_LINE=$($h.Value)"
}
foreach ($h in $reclaimPrefixHits) {
  Log "RECLAIM_PREFIX_LINE=$($h.Value)"
}
foreach ($h in $absentHits) {
  Log "ABSENT_LINE=$($h.Value)"
}

if ($reclaimHits.Count -lt 1) {
  Fail "CRITERION_1_FAILED (no 'reclaim orphan session ipc_id=$orphanId client_pid=$orphanPid live=<n>' in the log slice; prefix-only hits = $($reclaimPrefixHits.Count))"
}
if ($keptHits.Count -lt 1) {
  Fail 'CRITERION_2_FAILED (no "remove_session active orphan_reclaim presence=kept" in the log slice)'
}
if ($absentHits.Count -ne 0) {
  Fail 'CRITERION_3_FAILED ("remove_session active presence=absent" appeared, presence was flipped)'
}

Log 'VERDICT=ORPHAN_RECLAIM_OK'
Log 'EXIT=0'
exit 0
