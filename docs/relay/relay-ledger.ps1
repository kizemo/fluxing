<#
.SYNOPSIS
  Relay ledger writeback gate: compare-and-swap + append-only invariants.

.DESCRIPTION
  docs\relay.json is the single source of truth for chain progress, and several
  sessions touch it (the finishing stage, the stage it just started, a human).
  Prose that says "reload before merging, append only" does not prevent lost
  updates: PROOF-DOC-DRIVEN-2026-11-24.md records a real cross-session race that
  was survived only because a model happened to notice it.

  This script turns that prose into a gate. A session never hand-edits the
  ledger; it declares its intent in a small ASCII JSON patch file and this
  script applies it under five rules:

    1. writer lock  - docs\relay\.ledger.lock, created with CreateNew (atomic on
                      NTFS). A lock older than -LockStaleSeconds is provably
                      abandoned and gets taken over, kept as .stale.<epoch_ms>
                      for forensics, so a crashed writer cannot wedge the chain.
    2. CAS          - -ExpectSha256 is the hash you read BEFORE you started
                      working. If the ledger on disk differs, your view is stale:
                      LEDGER_FAIL reason=cas_conflict, nothing is written, you
                      must re-read and rebase. Checked twice: before the lock and
                      again under the lock. This is what kills the lost update.
    3. append-only  - stages / sessions / notes / ledger_docs / blocked_on /
                      human_checkpoints / stop_conditions may only grow: every
                      pre-existing element must still be there, byte-identical,
                      at the same index. A stage entry may only be edited by the
                      stage that owns it (stage_done must match its id), and
                      only through the keys listed in stage_fields. Top-level
                      scalars outside the allowlist (status, current_stage,
                      next_session_id, updated_at) must not change, and no new
                      top-level key may be invented.
    4. read-back    - the bytes go to a temp file, move into place, then are
                      re-read and compared verbatim. LEDGER_COMMITED carries the
                      before/after hashes and the element counts.
    5. session trail- a stage_done commit registers the session that ran the
                      stage in sessions[] by itself, so the ledger keeps an
                      independent trail of every session even when a link
                      forgets the sessions_append intent. The registration is
                      append only, and a session id that is already in
                      sessions[] is never added twice.

  Protocol rules are enforced in code, not prose: set_status may never be
  "running" (AGENT-RELAY.md: only a human starts a chain). Unknown patch keys
  are rejected so a typo cannot silently no-op a required writeback.

  Intended flow: -Action status (read sha256) -> do the work -> write a patch
  file -> -DryRun commit to validate -> real commit. Re-running the same patch
  after a cas_conflict is safe: appends are de-duplicated, so a commit that
  already landed is rejected by CAS rather than applied twice.

  Exit codes: 0 committed (or dry-run verified) / 2 bad arguments /
  3 CAS conflict, rule violation, or ledger missing / 4 writer lock timeout /
  5 patch file unparsable.

.EXAMPLE
  pwsh -NoProfile -File relay-ledger.ps1 -Action status

.EXAMPLE
  pwsh -NoProfile -File relay-ledger.ps1 -Action commit -ExpectSha256 <sha> -Patch docs\relay\stage-1-patch.json
#>
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('status', 'commit')]
  [string]$Action,

  [string]$ExpectSha256 = '',
  [string]$Patch = '',
  [string]$LedgerPath = '',
  [string]$LockPath = '',
  [int]$TimeoutMs = 5000,
  [int]$LockStaleSeconds = 30,
  [switch]$DryRun
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($LedgerPath)) { $LedgerPath = Join-Path $repoRoot 'docs\relay.json' }
if ([string]::IsNullOrWhiteSpace($LockPath)) { $LockPath = Join-Path $repoRoot 'docs\relay\.ledger.lock' }

$protectedArrays = @('stages', 'sessions', 'notes', 'ledger_docs', 'blocked_on', 'human_checkpoints', 'stop_conditions')
$scalarAllowlist = @('status', 'current_stage', 'next_session_id', 'updated_at')
$gateOwnedKeys = @($scalarAllowlist + $protectedArrays)   # the gate may create these itself
$patchKeys = @('stage_done', 'stage_fields', 'append_stage', 'sessions_append', 'notes_append',
  'ledger_docs_append', 'blocked_on_append', 'set_status', 'bump_current_stage', 'next_session_id')

function Ser($value) {
  if ($null -eq $value) { return '<null>' }
  return ($value | ConvertTo-Json -Depth 100 -Compress)
}
function Get-LedgerSha { param([string]$Path) (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash }
function Say($text) { Write-Host $text }
function Fail {
  param([string]$Reason, [string]$Detail = '', [int]$Code = 3)
  Say ("LEDGER_FAIL reason=" + $Reason + $(if ($Detail) { ' ' + $Detail } else { '' }))
  exit $Code
}
function Keys($obj) {
  if ($null -eq $obj) { return @() }
  return @($obj.PSObject.Properties.Name)
}

function Read-Ledger {
  param([string]$Path)
  if (-not (Test-Path -LiteralPath $Path)) { Fail 'ledger_missing' ("path=" + $Path) }
  $raw = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
  try { return $raw | ConvertFrom-Json } catch { Fail 'ledger_unparsable' ("path=" + $Path) }
}

function Enter-WriterLock {
  param([string]$Path, [int]$Timeout, [int]$StaleSeconds)
  $dir = Split-Path -Parent $Path
  if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
  $deadline = (Get-Date).AddMilliseconds($Timeout)
  while ($true) {
    $fs = $null
    try {
      $fs = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
      $bytes = [Text.UTF8Encoding]::new($false).GetBytes("pid=$PID host=$env:COMPUTERNAME taken_at=$([DateTimeOffset]::Now.ToString('o'))")
      $fs.Write($bytes, 0, $bytes.Length)
      $fs.Flush($true)
      return $fs
    } catch [System.IO.IOException] {
      if ($null -ne $fs) { $fs.Dispose() }
      $fs = $null
    } catch {
      if ($null -ne $fs) { $fs.Dispose() }
      $fs = $null
      throw
    }
    # Somebody holds it: take it over only if it is provably abandoned.
    try {
      $age = ((Get-Date) - (Get-Item -LiteralPath $Path).LastWriteTime).TotalSeconds
      if ($age -gt $StaleSeconds) {
        $kept = "$Path.stale.$([DateTimeOffset]::Now.ToUnixTimeMilliseconds())"
        Move-Item -LiteralPath $Path -Destination $kept -Force
        Say "LEDGER_LOCK_TAKEOVER age_s=$([int]$age) kept=$kept"
        continue
      }
    } catch { }
    if ((Get-Date) -ge $deadline) { return $null }
    Start-Sleep -Milliseconds 50
  }
}

function To-Map {
  # Accepts {"k":"v"} and [{"k":"v"},{"k2":"v2"}]: ConvertFrom-Json yields PSCustomObjects.
  param($v)
  $map = [ordered]@{}
  if ($null -eq $v) { return $map }
  foreach ($e in @($v)) {
    if ($e -is [System.Collections.IDictionary]) {
      foreach ($k in $e.Keys) { $map[[string]$k] = $e[$k] }
    } else {
      foreach ($k in (Keys $e)) { $map[$k] = $e.$k }
    }
  }
  return $map
}

function Set-NoteProp {
  param($Target, [string]$Name, $Value)
  $Target | Add-Member -Force -NotePropertyName $Name -NotePropertyValue $Value
}

# --- action: status ------------------------------------------------------------
if ($Action -eq 'status') {
  $l = Read-Ledger -Path $LedgerPath
  Say ("LEDGER_STATE path=" + $LedgerPath +
    " sha256=" + (Get-LedgerSha -Path $LedgerPath) +
    " status=" + [string]$l.status +
    " current_stage=" + [string]$l.current_stage +
    " max_stages=" + [string]$l.max_stages +
    " stages=" + @($l.stages).Count +
    " sessions=" + @($l.sessions).Count +
    " notes=" + @($l.notes).Count +
    " updated_at=" + [string]$l.updated_at)
  # B19-a: the chain shape in the same words the launcher prints on its cap gate
  # (FAIL reason=chain_at_stage_cap). stages= here is the per-stage id:status
  # list, not the count on LEDGER_STATE above; the line prefix separates the two.
  # The two renderings are computed by separate code on purpose: the self test
  # feeds both the same ledger and compares them, so a drift in either one goes
  # red instead of being copied over from the other.
  $declaredStages = @(@($l.stages) | Where-Object { $null -ne $_ })
  $chainParts = @()
  $doneStages = 0
  foreach ($s in $declaredStages) {
    $stageStatus = [string]$s.status
    if ([string]::IsNullOrWhiteSpace($stageStatus)) { $stageStatus = 'unknown' }
    if ($stageStatus -eq 'done') { $doneStages++ }
    $chainParts += ('{0}:{1}' -f [string]$s.id, $stageStatus)
  }
  $chainStages = 'none'
  $chainAllDone = 'unknown'
  $chainOpen = 'none'
  if ($declaredStages.Count -gt 0) {
    $chainStages = ($chainParts -join ',')
    $chainOpen = [string]($declaredStages.Count - $doneStages)
    if ($doneStages -eq $declaredStages.Count) { $chainAllDone = 'true' } else { $chainAllDone = 'false' }
  }
  # cap_reached stays unknown when either number is missing: reporting false there
  # would read as "there is room left" on a ledger that cannot even be compared.
  $capReached = 'unknown'
  $curStageNum = 0
  $maxStageNum = 0
  $curStageOk = [int]::TryParse([string]$l.current_stage, [ref]$curStageNum)
  $maxStageOk = [int]::TryParse([string]$l.max_stages, [ref]$maxStageNum)
  if ($curStageOk -and $maxStageOk) {
    if ($curStageNum -ge $maxStageNum) { $capReached = 'true' } else { $capReached = 'false' }
  }
  Say ("LEDGER_CHAIN current_stage=" + [string]$l.current_stage +
    " max_stages=" + [string]$l.max_stages +
    " cap_reached=" + $capReached +
    " stages=" + $chainStages +
    " all_done=" + $chainAllDone +
    " open_stages=" + $chainOpen)
  exit 0
}

# --- action: commit ------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($ExpectSha256)) { Fail 'missing_expect_sha256' 'hint=run_-Action_status_first_and_pass_the_sha256_you_read' 2 }
if ([string]::IsNullOrWhiteSpace($Patch)) { Fail 'missing_patch' 'hint=-Patch_<path_to_ascii_json>' 2 }
if (-not (Test-Path -LiteralPath $Patch)) { Fail 'patch_missing' ("path=" + $Patch) 2 }
if (-not (Test-Path -LiteralPath $LedgerPath)) { Fail 'ledger_missing' ("path=" + $LedgerPath) }

$patchRaw = Get-Content -LiteralPath $Patch -Raw -Encoding UTF8
try { $p = $patchRaw | ConvertFrom-Json } catch { Fail 'patch_unparsable' ("path=" + $Patch) 5 }
foreach ($k in (Keys $p)) {
  if ($patchKeys -notcontains $k) { Fail 'patch_unknown_key' ("key=" + $k + " allowed=" + ($patchKeys -join ',')) 2 }
}

$before = (Get-LedgerSha -Path $LedgerPath)
if ($before -ne $ExpectSha256.Trim()) {
  Fail 'cas_conflict' ("expected=" + $ExpectSha256.Trim() + " disk=" + $before + " hint=reread_the_ledger_and_rebase_your_patch_before_retrying")
}
Say ("LEDGER_CAS_OK sha256=" + $before)

$lock = Enter-WriterLock -Path $LockPath -Timeout $TimeoutMs -StaleSeconds $LockStaleSeconds
if ($null -eq $lock) { Fail 'writer_lock_timeout' ("path=" + $LockPath + " timeout_ms=" + $TimeoutMs + " hint=a_concurrent_writer_is_still_commiting") 4 }

try {
  $old = Read-Ledger -Path $LedgerPath
  $nowSha = Get-LedgerSha -Path $LedgerPath
  if ($nowSha -ne $ExpectSha256.Trim()) {
    Fail 'cas_conflict' ("expected=" + $ExpectSha256.Trim() + " disk=" + $nowSha + " stage=under_lock")
  }

  # Deep clone through a JSON round trip: $old stays pristine for the checks below.
  $new = (Ser $old) | ConvertFrom-Json
  $ownedIndex = -1
  $stageFieldNames = @()

  if ($null -ne $p.stage_done) {
    $doneId = [int]$p.stage_done
    $stageRef = @($new.stages)
    for ($i = 0; $i -lt $stageRef.Count; $i++) {
      if ([int]($stageRef[$i].id) -eq $doneId) { $ownedIndex = $i; break }
    }
    if ($ownedIndex -lt 0) { Fail 'stage_not_found' ("stage_done=" + $doneId + " hint=append_stage_first_or_use_an_id_already_in_the_ledger") }
  }
  if ($null -ne $p.stage_fields -and $ownedIndex -lt 0) {
    Fail 'stage_fields_without_stage_done' 'hint=stage_fields_only_applies_to_the_stage_named_by_stage_done' 2
  }

  if ($ownedIndex -ge 0) {
    $entry = @($new.stages)[$ownedIndex]
    if ($null -ne $p.stage_fields) {
      $stageFieldMap = To-Map $p.stage_fields
      if ($stageFieldMap.Count -lt 1) { Fail 'stage_fields_empty' 'hint=an_object_of_key_value_pairs' 2 }
      $stageFieldNames = @($stageFieldMap.Keys)
      foreach ($k in $stageFieldNames) { Set-NoteProp -Target $entry -Name $k -Value $stageFieldMap[$k] }
    }
    Set-NoteProp -Target $entry -Name 'status' -Value 'done'
    if (-not ((Keys $entry) -contains 'finished_at')) {
      Set-NoteProp -Target $entry -Name 'finished_at' -Value ([DateTimeOffset]::Now.ToString('o'))
    }
  }

  # --- rule 5: session trail -----------------------------------------------------
  # The session trail used to be a prose request only: a link had to add
  # sessions_append by itself, and a link that forgot left sessions[] empty
  # (that is what the B13 live chain ended with). The gate registers the
  # session that ran the stage by itself, so the ledger keeps an independent
  # trail of every session. Only an append is possible, and a session id that
  # is already in sessions[] is never added twice.
  if ($ownedIndex -ge 0) {
    $doneStage = @($new.stages)[$ownedIndex]
    $sessionId = [string]$doneStage.session_id
    $sessionSource = 'stage_field'
    if ([string]::IsNullOrWhiteSpace($sessionId)) {
      # The id of the link that is running right now is the one the previous
      # link wrote into next_session_id (the launcher writes it for the first
      # link of a cli chain).
      $sessionId = [string]$old.next_session_id
      $sessionSource = 'ledger_next_session_id'
    }
    if ([string]::IsNullOrWhiteSpace($sessionId)) {
      Say ("LEDGER_SESSION_SKIPPED stage=" + [string]$doneStage.id + " reason=no_session_id")
    } else {
      $known = $false
      foreach ($s in @($new.sessions)) {
        $probe = $s
        if (-not ($s -is [string])) { $probe = Ser $s }
        if (([string]$probe).IndexOf($sessionId, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) { $known = $true; break }
      }
      if ($known) {
        Say ("LEDGER_SESSION_ALREADY_REGISTERED session=" + $sessionId)
      } else {
        Set-NoteProp -Target $new -Name 'sessions' -Value (@(@($new.sessions) + @($sessionId)))
        Say ("LEDGER_SESSION_REGISTERED session=" + $sessionId +
          " stage=" + [string]$doneStage.id + " source=" + $sessionSource)
      }
    }
  }

  if ($null -ne $p.append_stage) {
    if ($null -eq $p.append_stage.id -or [string]::IsNullOrWhiteSpace([string]$p.append_stage.title)) {
      Fail 'append_stage_incomplete' 'hint=append_stage_needs_id_and_title' 2
    }
    $ids = @(@($new.stages) | ForEach-Object { [int]$_.id })
    if ($ids -contains [int]$p.append_stage.id) { Fail 'append_stage_duplicate_id' ("id=" + [string]$p.append_stage.id) }
    $item = $p.append_stage
    if (-not ((Keys $item) -contains 'status')) { Set-NoteProp -Target $item -Name 'status' -Value 'pending' }
    Set-NoteProp -Target $new -Name 'stages' -Value (@(@($new.stages) + @($item)))
  }

  # Append targets: only gate-owned arrays may be created when a seed lacks one.
  $appendPairs = @(
    @{ n = 'sessions_append';    f = 'sessions' },
    @{ n = 'notes_append';       f = 'notes' },
    @{ n = 'ledger_docs_append'; f = 'ledger_docs' },
    @{ n = 'blocked_on_append';  f = 'blocked_on' }
  )
  foreach ($pair in $appendPairs) {
    if ($null -eq $p.($pair.n)) { continue }
    if ($null -eq $new.($pair.f) -and $protectedArrays -notcontains $pair.f) {
      Fail 'ledger_field_missing' ("field=" + $pair.f + " hint=the_ledger_seed_is_missing_this_array")
    }
    $list = @(@($new.($pair.f)) | ForEach-Object { [string]$_ })
    $fresh = @(@($p.($pair.n) | ForEach-Object { [string]$_ }) | Where-Object { $_ -ne '' -and $list -notcontains $_ })
    if ($fresh.Count -gt 0) {
      Set-NoteProp -Target $new -Name $pair.f -Value (@(@($new.($pair.f)) + $fresh))
    }
  }

  if ($null -ne $p.set_status) {
    $want = [string]$p.set_status
    if ($want -eq 'running') { Fail 'status_running_is_human_only' 'hint=AGENT-RELAY.md_only_a_human_starts_a_chain_use_done_or_blocked' }
    Set-NoteProp -Target $new -Name 'status' -Value $want
  }
  if ($p.bump_current_stage -eq $true) {
    Set-NoteProp -Target $new -Name 'current_stage' -Value ([int]$new.current_stage + 1)
  }
  if ($null -ne $p.next_session_id -and -not [string]::IsNullOrWhiteSpace([string]$p.next_session_id)) {
    Set-NoteProp -Target $new -Name 'next_session_id' -Value ([string]$p.next_session_id)
  }
  Set-NoteProp -Target $new -Name 'updated_at' -Value ([DateTimeOffset]::Now.ToString('o'))

  # --- append-only verification, BEFORE anything is written -------------------
  foreach ($field in $protectedArrays) {
    if ($null -eq $old.$field -and $null -eq $new.$field) { continue }
    $oldList = @($old.$field)
    $newList = @($new.$field)
    if ($newList.Count -lt $oldList.Count) { Fail 'rule_shrunk_array' ("field=" + $field + " before=" + $oldList.Count + " after=" + $newList.Count) }
    for ($i = 0; $i -lt $oldList.Count; $i++) {
      if ($field -eq 'stages' -and $i -eq $ownedIndex) {
        $oldEntry = $oldList[$i]
        $newEntry = $newList[$i]
        foreach ($prop in @($oldEntry)) {
          if ($stageFieldNames -contains $prop.Name) { continue }
          if ($prop.Name -eq 'status') { continue }
          if ((Ser $prop.Value) -ne (Ser $newEntry.($prop.Name))) {
            Fail 'rule_edited_own_stage_key' ("stage_id=" + [string]$oldEntry.id + " key=" + $prop.Name)
          }
        }
        continue
      }
      if ((Ser $oldList[$i]) -ne (Ser $newList[$i])) { Fail 'rule_rewrote_existing_entry' ("field=" + $field + " index=" + $i) }
    }
  }
  foreach ($prop in @($new.PSObject.Properties)) {
    if ($gateOwnedKeys -contains $prop.Name -and -not ((Keys $old) -contains $prop.Name)) { continue }
    if (-not ((Keys $old) -contains $prop.Name)) { Fail 'rule_new_top_level_key' ("key=" + $prop.Name) }
  }
  foreach ($prop in @($old.PSObject.Properties)) {
    $k = $prop.Name
    if ($protectedArrays -contains $k) { continue }
    if ((Ser $old.$k) -eq (Ser $new.$k)) { continue }
    if ($scalarAllowlist -contains $k) { continue }
    Fail 'rule_changed_protected_field' ("field=" + $k)
  }
  Say ("LEDGER_RULES_OK stages=" + @($old.stages).Count + "->" + @($new.stages).Count +
    " sessions=" + @($old.sessions).Count + "->" + @($new.sessions).Count +
    " notes=" + @($old.notes).Count + "->" + @($new.notes).Count)

  $json = $new | ConvertTo-Json -Depth 100
  if ($DryRun) {
    Say ("LEDGER_DRYRUN verified=true before=" + $before + " would_be_stages=" + @($new.stages).Count)
    exit 0
  }

  $tmp = "$LedgerPath.tmp.$PID"
  [IO.File]::WriteAllText($tmp, $json, [Text.UTF8Encoding]::new($false))
  Move-Item -LiteralPath $tmp -Destination $LedgerPath -Force

  $after = Get-LedgerSha -Path $LedgerPath
  $reread = Get-Content -LiteralPath $LedgerPath -Raw -Encoding UTF8
  if ($reread -ne $json) { Fail 'readback_mismatch' ("path=" + $LedgerPath + " sha=" + $after) }
  Say ("LEDGER_COMMITED before=" + $before + " after=" + $after +
    " stages=" + @($old.stages).Count + "->" + @($new.stages).Count +
    " sessions=" + @($old.sessions).Count + "->" + @($new.sessions).Count +
    " current_stage=" + [string]$old.current_stage + "->" + [string]$new.current_stage +
    " path=" + $LedgerPath)
  exit 0
} finally {
  try { $lock.Dispose() } catch { }
  try { Remove-Item -LiteralPath $LockPath -Force -ErrorAction SilentlyContinue } catch { }
}
