<#
.SYNOPSIS
  Relay chain cross-session mutex lock (relay-port kit).

.DESCRIPTION
  Guarantees that at most ONE relay session may act as writer for a project.
  The lock is a single file created with CreateNew semantics, which is atomic
  on NTFS: the loser of a race gets IOException instead of overwriting.

  Lock file contents (ASCII JSON):
    holder      : session id or "start-relay:<pid>" that owns the lock
    prev_holder : previous owner, for audit trail
    stage       : stage number the owner is working on
    title       : human readable title
    acquired_at : ISO8601
    heartbeat_at: ISO8601, refreshed by -Action renew
    ttl_seconds : stale threshold

  A lock whose heartbeat is older than ttl is considered abandoned and is
  taken over (the stale file is kept as .stale.<epoch_ms> for forensics).

.EXAMPLE
  pwsh -NoProfile -File relay-lock.ps1 -Action acquire -Stage 1 -Title relay-1
  pwsh -NoProfile -File relay-lock.ps1 -Action renew
  pwsh -NoProfile -File relay-lock.ps1 -Action adopt -SessionId mvs_xxx
  pwsh -NoProfile -File relay-lock.ps1 -Action release
  pwsh -NoProfile -File relay-lock.ps1 -Action status
#>
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('acquire', 'renew', 'release', 'status', 'adopt')]
  [string]$Action,

  [string]$SessionId = '',
  [string]$Title = '',
  [int]$Stage = 0,
  [int]$TtlSeconds = 1800,
  [string]$LockPath = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($LockPath)) {
  $LockPath = Join-Path $repoRoot 'docs\relay\.relay.lock'
}

function Get-LockRecord {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { return $null }
  try {
    return (Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json)
  } catch {
    return $null
  }
}

function Save-LockRecord {
  param([string]$Path, $Record)
  $json = $Record | ConvertTo-Json -Depth 4
  $tmp = "$Path.tmp"
  [IO.File]::WriteAllText($tmp, $json, [Text.UTF8Encoding]::new($false))
  Move-Item -LiteralPath $tmp -Destination $Path -Force
}

function New-LockAtomically {
  param([string]$Path, $Record)
  $json = $Record | ConvertTo-Json -Depth 4
  try {
    $fs = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
  } catch [System.IO.IOException] {
    return $false
  }
  try {
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($json)
    $fs.Write($bytes, 0, $bytes.Length)
    $fs.Flush($true)
  } finally {
    $fs.Dispose()
  }
  return $true
}

function New-LockRecord {
  param([string]$Holder, [string]$PrevHolder, [int]$StageNo, [string]$TaskTitle, [int]$Ttl)
  $now = [DateTimeOffset]::Now
  return [ordered]@{
    holder       = $Holder
    prev_holder  = $PrevHolder
    stage        = $StageNo
    title        = $TaskTitle
    acquired_at  = $now.ToString('o')
    heartbeat_at = $now.ToString('o')
    ttl_seconds  = $Ttl
    repo_root    = $repoRoot
  }
}

$now = [DateTimeOffset]::Now

switch ($Action) {

  'acquire' {
    $existing = Get-LockRecord -Path $LockPath
    if ($null -ne $existing) {
      $hb = [DateTimeOffset]::Parse($existing.heartbeat_at)
      $age = ($now - $hb).TotalSeconds
      if ($age -le $TtlSeconds) {
        Write-Host "LOCK_HELD holder=$($existing.holder) stage=$($existing.stage) age_s=$([int]$age) ttl_s=$TtlSeconds path=$LockPath"
        exit 2
      }
      $stale = "$LockPath.stale.$($now.ToUnixTimeMilliseconds())"
      Move-Item -LiteralPath $LockPath -Destination $stale -Force
      Write-Host "LOCK_TAKEOVER stale_holder=$($existing.holder) age_s=$([int]$age) kept=$stale"
    }

    $holder = if ([string]::IsNullOrWhiteSpace($SessionId)) { "start-relay:$PID" } else { $SessionId }
    $rec = New-LockRecord -Holder $holder -PrevHolder '' -StageNo $Stage -TaskTitle $Title -Ttl $TtlSeconds
    if (-not (New-LockAtomically -Path $LockPath -Record $rec)) {
      Write-Host "LOCK_RACE holder=$holder path=$LockPath"
      exit 2
    }
    Write-Host "LOCK_ACQUIRED holder=$holder stage=$Stage ttl_s=$TtlSeconds path=$LockPath"
    exit 0
  }

  'renew' {
    $existing = Get-LockRecord -Path $LockPath
    if ($null -eq $existing) {
      Write-Host 'LOCK_MISSING action=renew'
      exit 3
    }
    if (-not [string]::IsNullOrWhiteSpace($SessionId) -and $existing.holder -ne $SessionId) {
      Write-Host "LOCK_NOT_MINE holder=$($existing.holder) requester=$SessionId"
      exit 5
    }
    $existing.heartbeat_at = $now.ToString('o')
    $existing.ttl_seconds = $TtlSeconds
    Save-LockRecord -Path $LockPath -Record $existing
    Write-Host "LOCK_RENEWED holder=$($existing.holder) heartbeat_at=$($existing.heartbeat_at)"
    exit 0
  }

  'adopt' {
    $existing = Get-LockRecord -Path $LockPath
    if ($null -eq $existing) {
      Write-Host 'LOCK_MISSING action=adopt'
      exit 3
    }
    if ([string]::IsNullOrWhiteSpace($SessionId)) {
      Write-Host 'LOCK_BAD_ARG action=adopt missing -SessionId'
      exit 6
    }
    $existing.prev_holder = $existing.holder
    $existing.holder = $SessionId
    $existing.heartbeat_at = $now.ToString('o')
    if ($Stage -gt 0) { $existing.stage = $Stage }
    if (-not [string]::IsNullOrWhiteSpace($Title)) { $existing.title = $Title }
    Save-LockRecord -Path $LockPath -Record $existing
    Write-Host "LOCK_ADOPTED holder=$SessionId prev_holder=$($existing.prev_holder) stage=$($existing.stage)"
    exit 0
  }

  'release' {
    $existing = Get-LockRecord -Path $LockPath
    if ($null -eq $existing) {
      Write-Host 'LOCK_FREE action=release'
      exit 0
    }
    if (-not [string]::IsNullOrWhiteSpace($SessionId) -and $existing.holder -ne $SessionId) {
      Write-Host "LOCK_NOT_MINE holder=$($existing.holder) requester=$SessionId"
      exit 5
    }
    Remove-Item -LiteralPath $LockPath -Force
    Write-Host "LOCK_RELEASED holder=$($existing.holder)"
    exit 0
  }

  'status' {
    $existing = Get-LockRecord -Path $LockPath
    if ($null -eq $existing) {
      Write-Host "LOCK_FREE path=$LockPath"
      exit 0
    }
    $age = ($now - [DateTimeOffset]::Parse($existing.heartbeat_at)).TotalSeconds
    $state = if ($age -le $TtlSeconds) { 'LIVE' } else { 'STALE' }
    Write-Host "LOCK_STATE state=$state holder=$($existing.holder) prev_holder=$($existing.prev_holder) stage=$($existing.stage) age_s=$([int]$age) ttl_s=$($existing.ttl_seconds) path=$LockPath"
    exit 0
  }
}
