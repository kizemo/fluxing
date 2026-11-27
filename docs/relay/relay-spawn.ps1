<#
.SYNOPSIS
  Start the next relay link for a CLI-based agent runtime (Claude Code / Codex CLI).

.DESCRIPTION
  The MCode runtime starts the next link with its built-in session_create tool, which no
  other runtime has. A CLI runtime starts the next link with a headless process instead:

    claude-code : claude -p "<prompt>" --output-format json --session-id <uuid> ...
    codex       : codex exec "<prompt>" --json ...

  This script builds that command, prints it, and only runs it when -Execute is given.
  The prompt is read from a file (the launcher writes docs\relay\next-relay-prompt.txt),
  so a long prompt never has to survive a shell quoting round trip.

  The session id is extracted from the runtime output; for claude-code the id is also
  requested up front with --session-id, so the id is known even when the runtime prints
  nothing parseable.

  The runtime is started with the chain workspace as its working directory: a headless
  CLI takes its working directory from the process, and --add-dir only widens the
  permission scope. The permission mode never asks (bypassPermissions by default) and
  the interactive question box is switched off, because a headless link has nobody to
  answer either of them. Those rules travel in a settings FILE
  (docs\relay\relay-settings.json) that is handed over with --settings, and the
  prompt is the LAST argument of the command line: the argument parser of the CLI
  build measured in B10 stops at the first positional argument, so every flag
  placed after the prompt is dropped. The raw runtime output is kept under
  docs\relay\relay-spawn-output\ so that a link which did nothing can be told
  apart from a link which worked.

  With -AdoptLock the relay lock is handed over to the new link by calling the sibling
  relay-lock.ps1 -Action adopt, which is the only supported way to transfer it. On
  claude-code the hand-over happens BEFORE the child process starts, so the new link
  owns the lock from its very first second and renew always names the real owner. If
  the child then fails, the lock is released again, so a broken link never leaves the
  chain behind a dead holder.

  Exit codes
    0  the command was built (dry run) or the link started and the session id was found
    2  invalid parameters, unknown runtime, missing prompt file, prompt too large
    3  the runtime CLI is missing, or the run failed, or the lock hand-over failed
    4  the run finished but no session id could be extracted

.EXAMPLE
  pwsh -NoProfile -ExecutionPolicy Bypass -File relay-spawn.ps1 -Runtime claude-code -PromptFile C:\repo\docs\relay\next-relay-prompt.txt
.EXAMPLE
  pwsh -NoProfile -ExecutionPolicy Bypass -File relay-spawn.ps1 -Runtime claude-code -PromptFile C:\repo\docs\relay\next-relay-prompt.txt -Execute -AdoptLock
#>
param(
  [Parameter(Mandatory = $true)]
  [string]$Runtime,

  [Parameter(Mandatory = $true)]
  [string]$PromptFile,

  [string]$WorkspaceDir = '',
  [string]$Model = '',
  # A headless link cannot answer a permission prompt, so the default must be a
  # mode that never asks. acceptEdits only auto-accepts file edits: every Bash
  # call would still wait for a human that is not there, which is how a chain
  # dies on its first command.
  [string]$PermissionMode = 'bypassPermissions',
  [string]$Sandbox = 'workspace-write',
  [string]$Approval = 'never',
  # Tools the headless runtime may use. A headless run cannot answer a
  # permission prompt, so anything not listed here is simply denied.
  [string]$AllowedTools = 'Bash,Read,Write,Edit,Glob,Grep',
  # Cost cap for one link. 0 means no cap.
  [double]$MaxBudgetUsd = 0,
  # Session id to use for the new link. Must be a UUID on claude-code.
  [string]$SessionId = '',
  [int]$MaxTurns = 0,
  [switch]$Execute,
  [switch]$AdoptLock
)

$ErrorActionPreference = 'Stop'

function Say([string]$Text) { Write-Host $Text }

# --- 1. parameters --------------------------------------------------------------
$cliRuntimes = @('claude-code', 'codex')
if ($cliRuntimes -notcontains $Runtime) {
  Say ("FAIL reason=runtime_not_cli runtime=" + $Runtime + " cli_runtimes=" + ($cliRuntimes -join ','))
  exit 2
}
if (-not (Test-Path -LiteralPath $PromptFile -PathType Leaf)) {
  Say ("FAIL reason=prompt_file_missing path=" + $PromptFile)
  exit 2
}
$promptPath = (Resolve-Path -LiteralPath $PromptFile).Path
$promptText = [IO.File]::ReadAllText($promptPath)
if ($promptText.Length -gt 60000) {
  Say ("FAIL reason=prompt_too_large chars=" + $promptText.Length + " limit=60000")
  exit 2
}
if ([string]::IsNullOrWhiteSpace($WorkspaceDir)) { $WorkspaceDir = (Get-Location).Path }
if (-not (Test-Path -LiteralPath $WorkspaceDir -PathType Container)) {
  Say ("FAIL reason=workspace_missing path=" + $WorkspaceDir)
  exit 2
}
$WorkspaceDir = (Resolve-Path -LiteralPath $WorkspaceDir).Path
if ($MaxTurns -lt 0) { $MaxTurns = 0 }

# --- 2. build the command ------------------------------------------------------
# Session id resolution. On claude-code the id is known BEFORE the run, and that
# is what makes an unattended chain work: the lock can be handed over to the new
# link before it starts, and the previous link can already record
# next_session_id in the ledger before spawning. Precedence: explicit
# -SessionId, then the ledger's next_session_id (the previous link wrote it),
# then a fresh UUID.
$requestedSessionId = ''
$sessionIdSource = 'none'
if ($Runtime -eq 'claude-code') {
  if (-not [string]::IsNullOrWhiteSpace($SessionId)) {
    $parsedId = [guid]::Empty
    if ([guid]::TryParse($SessionId, [ref]$parsedId)) {
      $requestedSessionId = $parsedId.ToString()
      $sessionIdSource = 'parameter'
    } else {
      Say ("FAIL reason=session_id_not_uuid value=" + $SessionId)
      exit 2
    }
  }
  if ($requestedSessionId -eq '') {
    # The ledger lives in the chain workspace, not next to this script: the driver
    # is a template that also runs from tool\templates, so the workspace is the
    # only trustworthy anchor.
    $ledgerNext = ''
    $ledgerFile = Join-Path $WorkspaceDir 'docs\relay.json'
    if (Test-Path -LiteralPath $ledgerFile -PathType Leaf) {
      try {
        $ledgerObj = Get-Content -LiteralPath $ledgerFile -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($null -ne $ledgerObj -and -not [string]::IsNullOrWhiteSpace([string]$ledgerObj.next_session_id)) {
          $ledgerNext = [string]$ledgerObj.next_session_id
        }
      } catch {
        $ledgerNext = ''
      }
    }
    $parsedLedger = [guid]::Empty
    if ($ledgerNext -ne '' -and [guid]::TryParse($ledgerNext, [ref]$parsedLedger)) {
      $requestedSessionId = $parsedLedger.ToString()
      $sessionIdSource = 'ledger'
    } else {
      $requestedSessionId = [guid]::NewGuid().ToString()
      $sessionIdSource = 'generated'
    }
  }
}

# A headless link cannot answer a question, and a question nobody answers turns
# into a multi-minute stall in the middle of a stage.
#
# The permission rules travel in a settings FILE, not in CLI flags. Measured on
# claude 999.0.0-local (2026-11-25) two things decide whether they are honoured:
# the key has to sit inside "permissions" (a top level "defaultMode" is read and
# ignored, which is why the machine wide ~/.claude/settings.json never actually
# put the machine into bypass mode), and the settings file is only read at all
# when the prompt is the LAST argument of the command line, because the argument
# parser of that build stops at the first positional argument. The file doubles
# as evidence of what the link was allowed to do.
$headlessSettings = '{"autoQuestion":{"enabled":false},"defaultMode":"bypassPermissions","permissions":{"defaultMode":"bypassPermissions","allow":["Bash","Bash(pwsh:*)","Bash(powershell:*)","Read","Write","Edit","Glob","Grep"]}}'
$budgetArg = ''
if ($MaxBudgetUsd -gt 0) {
  $budgetArg = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, '{0:0.####}', $MaxBudgetUsd)
}

$settingsFile = Join-Path $WorkspaceDir 'docs\relay\relay-settings.json'

$cmdArgs = @()
$exe = ''

if ($Runtime -eq 'claude-code') {
  $exe = 'claude'
  # The prompt goes LAST, after every option. On the CLI build measured in B10 the
  # argument parser stops at the first positional argument, so a flag that follows
  # the prompt is dropped: with the prompt first, --settings, --permission-mode,
  # --add-dir, --session-id, --output-format and --max-budget-usd were all
  # ignored, the link ran in default mode and every pwsh call came back as
  # "This command requires approval". With the prompt last the same command
  # honours every flag.
  $cmdArgs = @('--output-format', 'json', '--session-id', $requestedSessionId, '--add-dir', $WorkspaceDir)
  if (-not [string]::IsNullOrWhiteSpace($Model)) { $cmdArgs += @('--model', $Model) }
  if (-not [string]::IsNullOrWhiteSpace($PermissionMode)) { $cmdArgs += @('--permission-mode', $PermissionMode) }
  if (-not [string]::IsNullOrWhiteSpace($AllowedTools)) { $cmdArgs += @('--allowedTools', $AllowedTools) }
  $cmdArgs += @('--settings', $settingsFile)
  if ($budgetArg -ne '') { $cmdArgs += @('--max-budget-usd', $budgetArg) }
  if ($MaxTurns -gt 0) { $cmdArgs += @('--max-turns', [string]$MaxTurns) }
  $cmdArgs += @('-p', $promptText)
} else {
  $exe = 'codex'
  $cmdArgs = @('exec', '--json', '--sandbox', $Sandbox, '-a', $Approval)
  if (-not [string]::IsNullOrWhiteSpace($Model)) { $cmdArgs += @('--model', $Model) }
  $cmdArgs += $promptText
}

# The printed command MIRRORS the executed one: the real argument list is copied
# and only the prompt itself is replaced by a file reference, because a prompt can
# be tens of kilobytes and a log line that long is unreadable and unquotable.
# The order comes from the real list on purpose: a printed command that does not
# match the command that runs is worse than no log at all, and in B10 the wrong
# order in this very line is what hid the bug for hours.
$display = @()
if ($cmdArgs.Count -gt 1) { $display = $cmdArgs[0..($cmdArgs.Count - 2)] }
$display += '<prompt from file>'

Say ("SPAWN_RUNTIME runtime=" + $Runtime)
Say ("SPAWN_COMMAND " + $exe + ' ' + ($display -join ' '))
Say ("SPAWN_PROMPT_FILE file=" + $promptPath + " chars=" + $promptText.Length)
Say ("SPAWN_WORKSPACE dir=" + $WorkspaceDir)
Say ("SPAWN_SESSION_ID id=" + $requestedSessionId + " source=" + $sessionIdSource)
if ($Runtime -eq 'claude-code') {
  # The rules are printed as well as written: the log of a run says exactly which
  # permissions the link had, and a dry run shows them without touching the disk.
  Say ("SPAWN_SETTINGS_JSON " + $headlessSettings)
}

if (-not $Execute) {
  Say 'SPAWN_EXECUTE=false (add -Execute to run the command above)'
  exit 0
}

# --- 2b. permission settings file ----------------------------------------------
# The file has to exist before the runtime starts: a --settings path that is not
# there is a hard error in the CLI, and a chain that starts with a missing
# permission layer dies on its very first command.
if ($Runtime -eq 'claude-code') {
  try {
    $settingsDir = Split-Path -Parent $settingsFile
    if (-not (Test-Path -LiteralPath $settingsDir -PathType Container)) {
      New-Item -ItemType Directory -Path $settingsDir -Force | Out-Null
    }
    [IO.File]::WriteAllText($settingsFile, $headlessSettings, [Text.UTF8Encoding]::new($false))
    Say ("SPAWN_SETTINGS_FILE path=" + $settingsFile + " bytes=" + (Get-Item -LiteralPath $settingsFile).Length)
  } catch {
    Say ("FAIL reason=settings_write_failed path=" + $settingsFile + " error=" + $_.Exception.Message)
    exit 3
  }
}

# --- 3. hand the lock over BEFORE the child starts -----------------------------
# Only claude-code can do this: its session id is known up front, so the lock can
# name the new owner before the new owner exists. A lock that still names the
# previous link while the new link is already running makes renew meaningless and
# lets a second chain slip in.
$preAdopted = $false
if ($AdoptLock) {
  $lockScript = Join-Path $PSScriptRoot 'relay-lock.ps1'
  if (-not (Test-Path -LiteralPath $lockScript -PathType Leaf)) {
    Say ("FAIL reason=lock_script_missing path=" + $lockScript)
    exit 2
  }
  if ($Runtime -eq 'claude-code' -and $requestedSessionId -ne '') {
    & (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File $lockScript -Action adopt -SessionId $requestedSessionId
    $lockExit = $LASTEXITCODE
    if ($lockExit -ne 0) {
      Say ("FAIL reason=lock_adopt_failed exit=" + $lockExit + " session_id=" + $requestedSessionId)
      exit 3
    }
    Say ("SPAWN_LOCK_PRE_ADOPTED session_id=" + $requestedSessionId)
    $preAdopted = $true
  }
}

function Release-PreAdoptedLock {
  if (-not $preAdopted) { return }
  $lockScript = Join-Path $PSScriptRoot 'relay-lock.ps1'
  if (-not (Test-Path -LiteralPath $lockScript -PathType Leaf)) { return }
  & (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File $lockScript -Action release -SessionId $requestedSessionId | Out-Null
  if ($LASTEXITCODE -eq 0) {
    Say ("SPAWN_LOCK_RELEASED session_id=" + $requestedSessionId + " reason=link_failed")
  }
}

# --- 4. run ---------------------------------------------------------------------
$exeCmd = Get-Command $exe -ErrorAction SilentlyContinue
if (-not $exeCmd) {
  Release-PreAdoptedLock
  Say ("FAIL reason=runtime_cli_missing exe=" + $exe)
  exit 3
}
$outLines = @()
# The runtime must run INSIDE the chain workspace: a headless CLI takes its
# working directory from the process, and --add-dir only widens the permission
# scope. A link that starts in the wrong directory reads the wrong repo, writes
# the wrong files and still reports a clean run.
$prevCwd = (Get-Location).Path
try {
  Set-Location -LiteralPath $WorkspaceDir
  $outLines = & $exeCmd.Source @cmdArgs 2>&1 | ForEach-Object { [string]$_ }
} catch {
  Release-PreAdoptedLock
  Say ("FAIL reason=runtime_run_threw exe=" + $exe + " error=" + $_.Exception.Message)
  exit 3
} finally {
  Set-Location -LiteralPath $prevCwd
}
$runExit = $LASTEXITCODE
$outText = ($outLines -join "`n")

# Keep the raw runtime output on disk: without it a link that did nothing is
# indistinguishable from a link that worked, because the driver only reports
# the exit code of the runtime.
$outFile = ''
try {
  $outDir = Join-Path $WorkspaceDir 'docs\relay\relay-spawn-output'
  if (-not (Test-Path -LiteralPath $outDir -PathType Container)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
  }
  $outFile = Join-Path $outDir ($requestedSessionId + '.txt')
  [IO.File]::WriteAllText($outFile, $outText, [Text.UTF8Encoding]::new($false))
  Say ("SPAWN_OUTPUT_FILE path=" + $outFile + " bytes=" + (Get-Item -LiteralPath $outFile).Length)
} catch {
  Say ("WARN reason=output_not_written error=" + $_.Exception.Message)
}

if ($runExit -ne 0) {
  Release-PreAdoptedLock
  Say ("FAIL reason=runtime_run_failed exe=" + $exe + " exit=" + $runExit)
  foreach ($l in ($outLines | Select-Object -Last 20)) { Say ("SPAWN_OUTPUT " + $l) }
  exit 3
}

# --- 5. extract the session id --------------------------------------------------
$sessionId = ''
if ($Runtime -eq 'claude-code') {
  $m = [regex]::Match($outText, '"session_id"\s*:\s*"([^"]+)"')
  if ($m.Success) { $sessionId = $m.Groups[1].Value }
  if ($sessionId -eq '') { $sessionId = $requestedSessionId }
} else {
  $m = [regex]::Match($outText, '"thread_id"\s*:\s*"([^"]+)"')
  if ($m.Success) { $sessionId = $m.Groups[1].Value }
}
if ($sessionId -eq '') {
  Say ("FAIL reason=session_id_not_found runtime=" + $Runtime)
  foreach ($l in ($outLines | Select-Object -Last 20)) { Say ("SPAWN_OUTPUT " + $l) }
  exit 4
}

# --- 6. hand the lock over (only when it was not done up front) -----------------
if ($AdoptLock -and -not $preAdopted) {
  $lockScript = Join-Path $PSScriptRoot 'relay-lock.ps1'
  if (-not (Test-Path -LiteralPath $lockScript -PathType Leaf)) {
    Say ("FAIL reason=lock_script_missing path=" + $lockScript)
    exit 2
  }
  & (Get-Process -Id $PID).Path -NoProfile -ExecutionPolicy Bypass -File $lockScript -Action adopt -SessionId $sessionId
  $lockExit = $LASTEXITCODE
  if ($lockExit -ne 0) {
    Say ("FAIL reason=lock_adopt_failed exit=" + $lockExit + " session_id=" + $sessionId)
    exit 3
  }
  Say ("SPAWN_LOCK_ADOPTED session_id=" + $sessionId)
} elseif ($preAdopted) {
  Say ("SPAWN_LOCK_ADOPTED session_id=" + $sessionId + " mode=pre")
}

Say ("RELAY_SPAWN_OK runtime=" + $Runtime + " session_id=" + $sessionId)
exit 0
