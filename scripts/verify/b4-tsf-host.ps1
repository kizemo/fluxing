#Requires -Version 7.0
<#
.SYNOPSIS
  B-4 path 2 -- the REAL TSF host emitting FocusOut(PROFILE_DEACTIVATED), driven
  by a standalone probe process (test\TestTsFocusOutHost) instead of a synthetic
  IPC client.

.DESCRIPTION
  What path 1 (scripts\verify\b4-focusout-path.ps1) already proves: a synthetic
  IPC client that sends the same bytes in the same order makes the server flip
  presence and hide the panel. What it cannot prove: that a real TSF stack ever
  emits those bytes on a real profile switch. That is the gap this script closes.

  Two modes:

    (default)  read-only dry run -- NEVER changes the active input profile.
               Grabs ITfThreadMgr, enumerates every registered TIP, checks that
               the Fluxing TIP is registered, and reports the currently active
               profile. Green => the probe binary and the judging logic work.

    -RealRun   activates the Fluxing profile and cycles focus. THIS STEALS THE
               USER'S ACTIVE IME for a moment, so it is triple-gated: the switch
               -AllowProfileSwitch AND the environment variable
               FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1 must both be present, and
               the user must have approved it. Without them the script exits 2
               with B4_VERDICT=NEEDS_AUTHORIZATION and changes nothing.

  Evidence channels (both are required, neither alone is enough):

    1. probe stdout, machine-readable key=value (see the parser below)
    2. the server's and the shim's own diagnostic lines in
       %TEMP%\fluxing-profile-notify.log, sliced by BYTE OFFSET so that only the
       lines produced by THIS run are judged. The log is append-only and shared
       with every other Fluxing verification, so:
         - the slice is taken strictly between LOG_BYTES_BEFORE and the new end
         - a concurrency guard refuses to start while another verification
           process is alive (otherwise their lines land inside our window and
           the verdict is meaningless)

  Judged tokens, with the exact product-side sources:
    [server] focus_out param=<n> presence=<s>  WeaselIPC server, param=1 is
                                               PROFILE_DEACTIVATED, param=2 is
                                               APP_FOCUS_LOST
    [server] remove_session active presence=absent      the NORMAL branch
    [server] remove_session ... orphan_reclaim presence=kept  the ADR 0067 branch
    [server] reclaim orphan session ipc_id=... client_pid=...
    [tsf]    deactivated sent=<0|1>            WeaselTSF.cpp:447-448, written by
                                               the shim inside the host process
                                               => direct proof that the TSF side
                                               really fired and that the IPC call
                                               returned success

  Exit codes:
    0  B4_VERDICT=PATH2_CLOSED              real run, signature present, profile restored
    3  B4_VERDICT=OFFLINE_PREP_OK           dry run green, real run not authorised
    2  B4_VERDICT=NEEDS_AUTHORIZATION       -RealRun without both gates (nothing changed)
    4  B4_VERDICT=PARTIAL                   ran, but some criterion failed
    5  B4_VERDICT=ABORT                     environment unusable (desktop, server, build, concurrency)
#>

[CmdletBinding()]
param(
    [switch]$RealRun,
    [switch]$AllowProfileSwitch,
    [int]$HoldMs = 1500,
    [string]$RequireServerVersion = '1.3.8.0',
    [string]$RepoRoot = 'F:\soft\00selfmade\rime_claude',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$proj    = Join-Path $RepoRoot 'test\TestTsFocusOutHost\TestTsFocusOutHost.vcxproj'
$src     = Join-Path $RepoRoot 'test\TestTsFocusOutHost\TestTsFocusOutHost.cpp'
$msbuild = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe'
$exe     = Join-Path $RepoRoot 'Release\TestTsFocusOutHost.exe'
$repDir  = Join-Path $RepoRoot 'reports\fluxing-bb-orphan-reclaim'
$log     = Join-Path ([IO.Path]::GetTempPath()) 'fluxing-profile-notify.log'

$script:Fail = 0
$script:Notes = [Collections.Generic.List[string]]::new()

function Note([string]$s) { $script:Notes.Add($s); Write-Host $s }
function Check([bool]$ok, [string]$label) {
    if ($ok) { Note ("OK   " + $label) } else { $script:Fail++; Note ("FAIL " + $label) }
    return $ok
}
function Kv([hashtable]$h, [string]$k) { if ($h.ContainsKey($k)) { return $h[$k] } return '' }
function Parse-Kv([string]$text) {
    $h = @{}
    foreach ($line in ($text -split "`r?`n")) {
        if ($line -match '^([A-Z][A-Z0-9_]*)=(.*)$') { $h[$matches[1]] = $matches[2].Trim() }
    }
    return $h
}

# ---------------------------------------------------------------- desktop guard
# Real guard, no placeholder signatures: GetThreadDesktop + GetUserObjectInformationW(UOI_NAME=2).
# This machine runs YingDao RPA, which creates virtual desktops; evidence captured
# there is worthless. A wrong desktop name must abort, and so must any exception
# (a swallowed exception here would be a fake guard, worse than no guard).
try {
    if (-not ('FluxB4.Desk' -as [type])) {
        Add-Type -Namespace 'FluxB4' -Name 'Desk' -MemberDefinition @'
[DllImport("user32.dll", SetLastError = true)]
public static extern IntPtr GetThreadDesktop(uint threadId);

[DllImport("kernel32.dll")]
public static extern uint GetCurrentThreadId();

[DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern bool GetUserObjectInformationW(IntPtr hObj, int nIndex,
    System.Text.StringBuilder pvInfo, uint nLength, out uint lpnLengthNeeded);
'@ -UsingNamespace 'System.Text'
    }
    $sb = [Text.StringBuilder]::new(260)
    $need = [uint32]0
    $hDesk = [FluxB4.Desk]::GetThreadDesktop([FluxB4.Desk]::GetCurrentThreadId())
    if ($hDesk -eq [IntPtr]::Zero) { throw "GetThreadDesktop err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())" }
    if (-not [FluxB4.Desk]::GetUserObjectInformationW($hDesk, 2, $sb, [uint32]($sb.Capacity * 2), [ref]$need)) {
        throw "GetUserObjectInformationW err=$([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $desktopName = $sb.ToString()
} catch {
    Write-Host 'DESKTOP_GUARD=ERROR'
    Write-Host ("REASON=" + $_.Exception.Message)
    Write-Host 'B4_VERDICT=ABORT'
    Write-Host 'FAILURE_COUNT=1'
    Write-Host 'EXIT=5'
    exit 5
}
Note ("DESKTOP=$desktopName")
if ($desktopName -ne 'Default') {
    Note 'DESKTOP_GUARD=FAIL (need the Default desktop)'
    Write-Host 'B4_VERDICT=ABORT'
    Write-Host 'FAILURE_COUNT=1'
    Write-Host 'EXIT=5'
    exit 5
}
Note 'DESKTOP_GUARD=PASS'

# ---------------------------------------------------------------- authorization
$envAllow = [Environment]::GetEnvironmentVariable('FLUXING_VERIFY_ALLOW_PROFILE_SWITCH')
Note ("AUTH_SWITCH=" + $(if ($AllowProfileSwitch) { 1 } else { 0 }) + " AUTH_ENV=" + $envAllow)
if ($RealRun -and -not ($AllowProfileSwitch -and $envAllow -eq '1')) {
    Note 'REALRUN_BLOCKED: -RealRun needs -AllowProfileSwitch AND FLUXING_VERIFY_ALLOW_PROFILE_SWITCH=1'
    Note 'REALRUN_BLOCKED: activating the Fluxing profile takes the keyboard away from the user'
    Write-Host 'FAILURE_COUNT=0'
    Write-Host 'B4_VERDICT=NEEDS_AUTHORIZATION'
    Write-Host 'EXIT=2'
    exit 2
}

# ---------------------------------------------------------------- concurrency guard
$peerNames = 'TestOrphanReclaimE2E', 'TestFocusOut', 'TestWeaselIPC', 'WeaselDeployer', 'TestTsFocusOutHost'
$peers = @(Get-Process -Name $peerNames -ErrorAction SilentlyContinue)
if ($peers.Count -gt 0) {
    foreach ($p in $peers) { Note ("PEER_RUNNING pid=$($p.Id) name=$($p.ProcessName)") }
    Note 'CONCURRENCY_GUARD=FAIL (another Fluxing verification owns the log window)'
    Write-Host 'FAILURE_COUNT=1'
    Write-Host 'B4_VERDICT=ABORT'
    Write-Host 'EXIT=5'
    exit 5
}
Note 'CONCURRENCY_GUARD=PASS'

# ---------------------------------------------------------------- server preflight
$srv = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
if ($srv.Count -lt 1) {
    Note 'SERVER=MISSING (the probe is pointless without the running server; do not start it here)'
    Write-Host 'FAILURE_COUNT=1'
    Write-Host 'B4_VERDICT=ABORT'
    Write-Host 'EXIT=5'
    exit 5
}
$srvVer = $srv[0].MainModule.FileVersionInfo.FileVersion
Note ("SERVER pid=$($srv[0].Id) ver=$srvVer expect=$RequireServerVersion")
$serverOk = ($srvVer -eq $RequireServerVersion)
Check $serverOk "server image version is $RequireServerVersion" | Out-Null

# ---------------------------------------------------------------- build
if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $msbuild)) {
        Note "MSBUILD_MISSING=$msbuild"
        Write-Host 'FAILURE_COUNT=1'
        Write-Host 'B4_VERDICT=ABORT'
        Write-Host 'EXIT=5'
        exit 5
    }
    $needBuild = $true
    if (Test-Path -LiteralPath $exe) {
        $e = Get-Item -LiteralPath $exe
        $needBuild = ($e.LastWriteTime -lt (Get-Item -LiteralPath $src).LastWriteTime) -or
                     ($e.LastWriteTime -lt (Get-Item -LiteralPath $proj).LastWriteTime)
    }
    if ($needBuild) {
        $blog = & $msbuild $proj /p:Configuration=Release /p:Platform=x64 "/p:SolutionDir=$RepoRoot" /nologo /v:minimal 2>&1 | Out-String
        $script:BuildExit = $LASTEXITCODE
        [IO.File]::WriteAllText((Join-Path $repDir 'b4-tsf-host-build.log'), $blog, [Text.UTF8Encoding]::new($false))
        Note ("BUILD_EXIT=" + $script:BuildExit)
        Check ($script:BuildExit -eq 0) 'msbuild Release|x64 exit 0' | Out-Null
    } else {
        Note 'BUILD=UP_TO_DATE'
    }
}
if (-not (Test-Path -LiteralPath $exe)) {
    Note "EXE_MISSING=$exe"
    Write-Host 'FAILURE_COUNT=1'
    Write-Host 'B4_VERDICT=ABORT'
    Write-Host 'EXIT=5'
    exit 5
}
$exeItem = Get-Item -LiteralPath $exe
$srcItem = Get-Item -LiteralPath $src
Check ($exeItem.LastWriteTime -ge $srcItem.LastWriteTime) 'exe is at least as new as the source (no stale binary)' | Out-Null
Note ("EXE=$exeItem bytes=$($exeItem.Length) mtime=$($exeItem.LastWriteTime.ToString('s'))")

# ---------------------------------------------------------------- run probe #1
$bytesBefore = 0L
if (Test-Path -LiteralPath $log) { $bytesBefore = (Get-Item -LiteralPath $log).Length }
Note ("LOG=$log LOG_BYTES_BEFORE=$bytesBefore")

$probeArgs = @('-DryRun')
if ($RealRun) { $probeArgs = @('-ActivateAndCycle', '-AllowProfileSwitch', '-HoldMs', "$HoldMs") }
$script:ProbeOut = & $exe @probeArgs 2>&1 | Out-String
$script:ProbeExit = $LASTEXITCODE
$probeTag = if ($RealRun) { 'realrun' } else { 'dryrun' }
[IO.File]::WriteAllText((Join-Path $repDir "b4-tsf-host-probe-$probeTag.txt"), $script:ProbeOut, [Text.UTF8Encoding]::new($false))
$k1 = Parse-Kv $script:ProbeOut
Note ("PROBE_EXIT=" + $script:ProbeExit + " MODE=" + (Kv $k1 'MODE'))

# ---------------------------------------------------------------- slice the log
$sliceText = ''
if (Test-Path -LiteralPath $log) {
    $fs = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        [void]$fs.Seek($bytesBefore, [IO.SeekOrigin]::Begin)
        $sr = [IO.StreamReader]::new($fs, [Text.Encoding]::UTF8)
        try { $sliceText = $sr.ReadToEnd() } finally { $sr.Dispose() }
    } finally { $fs.Dispose() }
}
$sliceBytes = [Text.Encoding]::UTF8.GetByteCount($sliceText)
Note ("SLICE_BYTES=$sliceBytes")
$sliceLines = @($sliceText -split "`r?`n" | Where-Object { $_.Trim().Length -gt 0 })
[IO.File]::WriteAllText((Join-Path $repDir "b4-tsf-host-slice-$probeTag.txt"), ($sliceLines -join "`r`n"), [Text.UTF8Encoding]::new($false))

$f1 = @($sliceLines | Where-Object { $_ -match '\[server\] focus_out param=1' })
$f2 = @($sliceLines | Where-Object { $_ -match '\[server\] focus_out param=2' })
$rmAbs = @($sliceLines | Where-Object { $_ -match '\[server\] remove_session' -and $_ -match 'presence=absent' })
$orphanKept = @($sliceLines | Where-Object { $_ -match 'orphan_reclaim presence=kept' })
$reclaim = @($sliceLines | Where-Object { $_ -match '\[server\] reclaim orphan session' })
$deactSent1 = @($sliceLines | Where-Object { $_ -match '\[tsf\] deactivated sent=1' })
$deactSent0 = @($sliceLines | Where-Object { $_ -match '\[tsf\] deactivated sent=0' })
Note ("SLICE_FOCUS_OUT_PARAM1=" + $f1.Count)
Note ("SLICE_FOCUS_OUT_PARAM2=" + $f2.Count)
Note ("SLICE_REMOVE_SESSION_ABSENT=" + $rmAbs.Count)
Note ("SLICE_ORPHAN_RECLAIM_KEPT=" + $orphanKept.Count)
Note ("SLICE_RECLAIM_ORPHAN_SESSION=" + $reclaim.Count)
Note ("SLICE_TSF_DEACTIVATED_SENT1=" + $deactSent1.Count)
Note ("SLICE_TSF_DEACTIVATED_SENT0=" + $deactSent0.Count)

# ---------------------------------------------------------------- probe assertions
Check ((Kv $k1 'DESKTOP_GUARD') -like 'PASS*') 'probe itself is on the Default desktop' | Out-Null
Check ((Kv $k1 'THREADMGR') -eq 'OK') 'ITfThreadMgr obtained and activated' | Out-Null
Check ((Kv $k1 'FLUXING_TIP_REGISTERED') -eq '1') 'Fluxing TIP is registered with TSF' | Out-Null
Check ((Kv $k1 'ACTIVE_KBD_CLSID').Length -ge 38) 'probe reported an active keyboard profile' | Out-Null
# The probe prints PROBE_VERDICT and EXIT on the SAME line ("PROBE_VERDICT=PASS EXIT=0"),
# so an exact -eq 'PASS' comparison is always false. Compare the leading token only.
Check ((Kv $k1 'PROBE_VERDICT') -like 'PASS*') "probe verdict PASS (exit $($script:ProbeExit))" | Out-Null
Check ((Kv $k1 'FAILURE_COUNT') -eq '0') 'probe internal FAILURE_COUNT=0' | Out-Null

if ($RealRun) {
    Check ((Kv $k1 'ACTIVATE_FLUXING_HR') -eq '0x00000000') 'ActivateLanguageProfile(Fluxing) returned S_OK' | Out-Null
    Check ([int](Kv $k1 'TSF_EVENTS_FLUXING_ON') -ge 1) 'the probe saw OnActivated for the Fluxing CLSID' | Out-Null
    Check ((Kv $k1 'RESTORE_HR') -eq '0x00000000') 'the probe restored the pre-run profile' | Out-Null
    Check ($deactSent1.Count -ge 1) "shim logged deactivated sent=1 (count $($deactSent1.Count))" | Out-Null
    Check ($f1.Count -ge 1) "server logged focus_out param=1 (count $($f1.Count))" | Out-Null
    Check ($rmAbs.Count -ge 1) "server logged remove_session ... presence=absent (count $($rmAbs.Count))" | Out-Null
}

# ---------------------------------------------------------------- profile restored?
# Independent re-query with a second, read-only probe run, AFTER the slice was
# taken (so its own log lines stay outside the judged window).
$beforeClsid = Kv $k1 'ACTIVE_KBD_CLSID'
$beforeProfile = Kv $k1 'ACTIVE_KBD_PROFILE'
$probe2 = & $exe -DryRun 2>&1 | Out-String
[IO.File]::WriteAllText((Join-Path $repDir "b4-tsf-host-restore-$probeTag.txt"), $probe2, [Text.UTF8Encoding]::new($false))
$k2 = Parse-Kv $probe2
$afterClsid = Kv $k2 'ACTIVE_KBD_CLSID'
$afterProfile = Kv $k2 'ACTIVE_KBD_PROFILE'
Note ("PROFILE_BEFORE=$beforeClsid $beforeProfile")
Note ("PROFILE_AFTER =$afterClsid $afterProfile")
Check ($afterClsid -eq $beforeClsid -and $afterProfile -eq $beforeProfile) 'active profile is unchanged after the run' | Out-Null

# ---------------------------------------------------------------- verdict
$signatureSeen = ($deactSent1.Count -ge 1 -and $f1.Count -ge 1 -and $rmAbs.Count -ge 1)
$verdict = 'PARTIAL'
$exitCode = 4
if ($script:Fail -gt 0) {
    $verdict = 'PARTIAL'
    $exitCode = 4
} elseif ($RealRun -and $signatureSeen) {
    $verdict = 'PATH2_CLOSED'
    $exitCode = 0
} elseif ($RealRun) {
    $verdict = 'PARTIAL'
    $exitCode = 4
} elseif ($signatureSeen) {
    $verdict = 'PATH2_SIGNATURE_SEEN_IN_READ_ONLY_MODE'
    $exitCode = 0
} else {
    $verdict = 'OFFLINE_PREP_OK'
    $exitCode = 3
}
Note ("SIGNATURE_SEEN=" + [int]$signatureSeen)
Note ("NOTE the dry run only reads TSF state, but ITfThreadMgr::Activate can still make the OS load the active shim into the probe process; if SIGNATURE_SEEN=1 in dry-run mode that is the cheapest way to close B-4 path 2 and it still needs a second, isolated confirmation before being treated as proof")
Write-Host ('FAILURE_COUNT=' + $script:Fail)
Write-Host ('B4_VERDICT=' + $verdict)
Write-Host ('EXIT=' + $exitCode)
exit $exitCode
