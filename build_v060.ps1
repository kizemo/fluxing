# build_v060.ps1 — Fluxing v0.19.0.60 build wrapper (Phase L 调整 1).
# Two-step pattern (L106 lesson): _buildflow.cmd for MSBuild weasel.sln
# (with explicit SolutionDir so rime.lib resolves), then _nsis_only.cmd
# for the installer.

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$buildflow = "F:\soft\00selfmade\rime_claude\_buildflow.cmd"
$nsis = "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"

Write-Host "Running: _buildflow.cmd (MSBuild weasel.sln, SolutionDir explicit)"
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
