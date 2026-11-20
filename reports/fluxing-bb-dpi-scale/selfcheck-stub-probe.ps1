# selfcheck-stub-probe.ps1 -- pure ASCII. Teeth test for run-dpi-tier.ps1 guard 3.
# Writes a synthetic report.json whose dialog_client the caller chooses, so guard 3 can be driven
# into its rejection branches without a real dpi change. NOT used for evidence.
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [int]$ControlIdOfKnownSingleLine = 0,
    # run-dpi-tier.ps1 only forwards -OutDir and -ControlIdOfKnownSingleLine, so the synthetic
    # geometry arrives through the environment.
    [string]$Client = $env:STUB_CLIENT
)
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$obj = [ordered]@{ dialog_client = $Client; stub = $true; control_id = $ControlIdOfKnownSingleLine }
$obj | ConvertTo-Json -Compress | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
Write-Host ("STUB_WROTE dialog_client={0}" -f $Client)
exit 0