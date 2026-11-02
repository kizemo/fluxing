<#
  Fluxing silent install - HARDENED driver (2026-11-03, L169)

  Usage (run from an ELEVATED PowerShell window):
      powershell -NoProfile -ExecutionPolicy Bypass -File silent-install.ps1
      powershell -NoProfile -ExecutionPolicy Bypass -File silent-install.ps1 -InstallRoot "D:\Program Files\fluxing"
      powershell -NoProfile -ExecutionPolicy Bypass -File silent-install.ps1 -DryRun

  WHAT CHANGED vs the old tmp\silent-install-116.cmd (the 2026-11-03 accident):

    1. UNIT FIX.  The old script passed ONE %TARGET% to BOTH the installer
       (/D= => the NSIS install ROOT) and the acceptance checker (-InstallDir
       => the ENGINE dir).  Those are two different directories:
           install root : D:\Program Files\fluxing      (contains "weasel\")
           engine dir   : <root>\weasel                  (WeaselServer.exe here)
       Passing the engine dir to /D= nests the engine one level too deep
       (...\weasel\weasel) and orphans the real user profile.
       See output/install.nsi:24-25 (WEASEL_ROOT) and :456-457 ($R3 / $INSTDIR).

    2. G1  the install root must NOT live inside the source workspace.
           (The accident: a workspace tmp\ path was passed in as the target
            and the installer really ran, writing 235 files / 220 MB there
            and rewriting 4 registry values.)

    3. G2  the install root must NOT live under a smoke-test / temp root
           (mirrors the check_reg guards in output/install.nsi:301-325).

    4. G3  the install root must look like a Fluxing install root: it either
           holds weasel\WeaselServer.exe, or it is an empty directory.
           "the path exists" is NOT good enough.

    5. -DryRun prints the plan and changes NOTHING.

    6. The verdict comes from acceptance-check.ps1, NOT from $LASTEXITCODE.

  HOW THE INSTALLER IS LAUNCHED (do not "simplify" this):
    L28    "NSIS silent-install: write a .bat wrapper, do not invoke from
            PowerShell &"
    L1214  Start-Process -FilePath installer.exe -ArgumentList '/S /D=...'
           -Wait -PassThru HANGS under PowerShell 5.1 (5 min, no return).
    L15    PS 5.1 merges /D= and /LOG= at the '=' boundary when the args go
           through Start-Process -ArgumentList.
    AGENTS.md S2.5 mandates `cmd /c` for the silent-install smoke test.
    => this script owns the GUARDS only; the installer runs from
       scripts\install\silent-install.cmd via `cmd /c`, which is the shape
       that is known to work (cmd waits for the GUI installer and
       %ERRORLEVEL% is real).
    The install root travels through the FLUXING_INSTALL_ROOT environment
    variable, NOT through a command-line argument: a .bat that receives a
    path with spaces as %1 gets truncated by `cmd /c script <path>`.

  NSIS /D= rules (NSIS manual 3.2.1 / L112 / L113):
    - /D= must be the LAST parameter
    - /D= must NOT be quoted ("D:\Program Files\..." would be truncated to
      "D:\Program" by the NSIS argument parser)

  EXIT CODES: 0 = ok   2 = preflight refused   3 = installer failed   4 = acceptance check failed
#>
[CmdletBinding()]
param(
    [string]$InstallRoot = 'D:\Program Files\fluxing',
    [switch]$DryRun
)

$ErrorActionPreference = 'Continue'

$RepoRoot   = 'F:\soft\00selfmade\rime_claude'
$Installer  = Join-Path $RepoRoot 'output\archives\fluxing-1.1.6-installer.exe'
$LauncherBat = Join-Path $RepoRoot 'scripts\install\silent-install.cmd'
$Checker    = Join-Path $RepoRoot 'scripts\install\acceptance-check.ps1'
$UserDirKey = 'HKCU:\Software\Fluxing\Weasel'

function Fail([string]$msg, [int]$code) {
    Write-Host ""
    Write-Host "[FATAL] $msg"
    exit $code
}

# ---- resolve the two units explicitly --------------------------------------
$InstallRoot = $InstallRoot.TrimEnd('\')
$EngineDir   = Join-Path $InstallRoot 'weasel'

Write-Host "==== Fluxing silent install (hardened) ===="
Write-Host "  installer    : $Installer"
Write-Host "  launcher     : $LauncherBat"
Write-Host "  install root : $InstallRoot"
Write-Host "  engine dir   : $EngineDir"
Write-Host "  checker      : $Checker"
Write-Host "  dry run      : $($DryRun.IsPresent)"
Write-Host ""

# ---- preflight ---------------------------------------------------------------
if (-not (Test-Path -LiteralPath $Installer))   { Fail "installer not found: $Installer" 2 }
if (-not (Test-Path -LiteralPath $LauncherBat)) { Fail "launcher .bat not found: $LauncherBat" 2 }
if (-not (Test-Path -LiteralPath $Checker))     { Fail "acceptance checker not found: $Checker" 2 }

# G1 - never install into the source workspace
$rootFull  = [System.IO.Path]::GetFullPath($InstallRoot)
$engineFull = [System.IO.Path]::GetFullPath($EngineDir)
if ($rootFull.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    $engineFull.StartsWith($RepoRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
    $rootFull -match '(?i)rime_claude' -or
    $rootFull -match '(?i)\tmp(\\|$)') {
    Fail ("G1 refused: the install root lives inside the source workspace / a tmp path: $rootFull" `
          + "`n        the installer WRITES to disk and rewrites HKLM - it must never target the workspace.") 2
}

# G2 - smoke-test / temp roots (mirrors output/install.nsi check_reg*)
if ($rootFull -match '(?i)^C:\\TEMP(\\|$)' -or
    $rootFull -match '(?i)^C:\\Users\\test(\\|$)' -or
    $rootFull.StartsWith($env:TEMP, [System.StringComparison]::OrdinalIgnoreCase)) {
    Fail "G2 refused: the install root is under a known smoke-test / temp root: $rootFull" 2
}

# G3 - the root must look like a Fluxing install root
if (-not (Test-Path -LiteralPath $InstallRoot)) {
    Fail "G3 refused: install root does not exist: $rootFull  (create it first)" 2
}
$engineExe = Join-Path $EngineDir 'WeaselServer.exe'
if (-not (Test-Path -LiteralPath $engineExe)) {
    $children = @(Get-ChildItem -LiteralPath $InstallRoot -Force -ErrorAction SilentlyContinue)
    if ($children.Count -gt 0) {
        Fail ("G3 refused: the target exists and is not a Fluxing install root" `
              + "`n        expected: $engineExe" `
              + "`n        found   : $($children.Count) entries, no weasel\WeaselServer.exe") 2
    }
    Write-Host "  [G3] install root is empty - a fresh install will create the layout"
} else {
    Write-Host "  [G3] existing Fluxing install found - upgrading in place"
}

# ---- dry run -----------------------------------------------------------------
if ($DryRun) {
    Write-Host ""
    Write-Host "==== DRY RUN - nothing was executed ===="
    Write-Host "  would run   : cmd /c `"$LauncherBat`"  (FLUXING_INSTALL_ROOT=$InstallRoot)"
    Write-Host "  would check : $Checker -InstallDir `"$EngineDir`""
    Write-Host "  would restore: HKCU:\Software\Fluxing\Weasel\RimeUserDir"
    exit 0
}

# ---- 1. save the user data dir pointer (never orphan the user profile) -------
$savedUserDir = $null
try {
    $savedUserDir = (Get-ItemProperty -Path $UserDirKey -Name 'RimeUserDir' -ErrorAction SilentlyContinue).RimeUserDir
} catch { $savedUserDir = $null }
if ($savedUserDir) {
    Write-Host "[1/4] saved RimeUserDir = $savedUserDir"
} else {
    Write-Host "[1/4] RimeUserDir was not set - nothing to restore"
}

# ---- 2. run the installer ----------------------------------------------------
Write-Host "[2/4] running installer (silent) via the .bat launcher..."
$prevRoot = $env:FLUXING_INSTALL_ROOT
$prevInst = $env:FLUXING_INSTALLER
$env:FLUXING_INSTALL_ROOT = $InstallRoot
$env:FLUXING_INSTALLER    = $Installer
$out = & cmd.exe /c "`"$LauncherBat`"" 2>&1
$rc = $LASTEXITCODE
$out | ForEach-Object { Write-Host "      $_" }
if ($null -eq $prevRoot) { $env:FLUXING_INSTALL_ROOT = $null }
else { $env:FLUXING_INSTALL_ROOT = $prevRoot }
if ($null -eq $prevInst) { $env:FLUXING_INSTALLER = $null }
else { $env:FLUXING_INSTALLER = $prevInst }

if ($rc -ne 0) {
    Fail "installer returned $rc - inspect $InstallRoot manually" 3
}

# ---- 3. restore the user data dir pointer ------------------------------------
if ($savedUserDir) {
    try {
        New-Item -Path $UserDirKey -Force | Out-Null
        New-ItemProperty -Path $UserDirKey -Name 'RimeUserDir' -Value $savedUserDir -PropertyType String -Force | Out-Null
        Write-Host "[3/4] restored RimeUserDir = $savedUserDir"
    } catch {
        Write-Host "[3/4] WARNING: could not restore RimeUserDir: $($_.Exception.Message)"
    }
} else {
    Write-Host "[3/4] nothing to restore"
}

# ---- 4. acceptance check (the real verdict) ----------------------------------
Write-Host "[4/4] running acceptance check on the ENGINE dir..."
Write-Host ""
& $Checker -InstallDir $EngineDir
$checkRc = $LASTEXITCODE
Write-Host ""
if ($checkRc -eq 0) {
    Write-Host "==== SILENT INSTALL OK - ALL HARD CHECKS PASSED ===="
} else {
    Write-Host "==== SILENT INSTALL DONE BUT CHECK FAILED (exit $checkRc) ===="
}
Write-Host ""
Write-Host "reminder: this script did NOT restart the machine or the IME host."
Write-Host "          log out / log in (or reboot) before judging the TSF front-end."
exit $checkRc
