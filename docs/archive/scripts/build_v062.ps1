# build_v062.ps1 — Fluxing v0.19.0.62 build wrapper (Phase L 调整 1.2).
# Two-step pattern (L106 lesson): _buildflow.cmd + _nsis_only.cmd.
# Removes QuickPanel button slot entirely (5→4 buttons, kPanelW 289→245).

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$buildflow = "F:\soft\00selfmade\rime_claude\_buildflow.cmd"
$nsis = "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"

Write-Host "Running: _buildflow.cmd (MSBuild weasel.sln)"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "`"$buildflow`""
) -NoNewWindow -Wait -PassThru
Write-Host "BuildFlow exit code: $($proc.ExitCode)"
if ($proc.ExitCode -ne 0) {
  exit $proc.ExitCode
}

Write-Host "Running: _nsis_only.cmd (NSIS installer build)"
$proc = Start-Process -FilePath "cmd.exe" -ArgumentList @(
    "/c"
    "`"$nsis`""
) -NoNewWindow -Wait -PassThru
Write-Host "NSIS exit code: $($proc.ExitCode)"
exit $proc.ExitCode