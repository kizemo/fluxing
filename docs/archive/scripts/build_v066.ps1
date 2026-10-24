# build_v066.ps1 — Fluxing v0.19.0.66 build wrapper (Phase L Bugfix 3 真修).
# 修复内容: WeaselTSF 注册 ITfActiveLanguageProfileNotifySink AdviseSink
#   -> OnActivated 真正触发 -> m_client.FocusIn() -> IPC -> QuickPanelDialog::Show.
# L107/L108 lesson: _buildflow.cmd 内已用 /t:Rebuild (避免增量漏链), _nsis_only.cmd 单独跑.

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$buildflow = "F:\soft\00selfmade\rime_claude\_buildflow.cmd"
$nsis = "F:\soft\00selfmade\rime_claude\_nsis_only.cmd"

Write-Host "Running: _buildflow.cmd (MSBuild weasel.sln /t:Rebuild)"
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