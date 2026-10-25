# probe-sandbox.ps1 - 探测这台机器能不能跑 Windows Sandbox 门禁
#
# CI 用它做前置检查。GitHub 官方的 windows runner 没有嵌套虚拟化,
# 跑不了 Windows Sandbox; 所以门禁 job 指向带 sandbox 标签的自托管
# runner。这个探针把「跑不了」变成一句明确的结论, 而不是让 job 静悄悄地
# 绿过去 —— 静绿等于没验。
#
# 退出码: 0 = 可以跑, 1 = 跑不了

[CmdletBinding()]
param()

$ErrorActionPreference = 'Continue'

$problems = @()

# 1. Windows Sandbox 这个可选功能装了没、开了没。
$feat = Get-WindowsOptionalFeature -Online -FeatureName 'Containers-DisposableClientVM' -ErrorAction SilentlyContinue
if (-not $feat) {
    $problems += 'Get-WindowsOptionalFeature 查不到 Containers-DisposableClientVM (需要管理员权限?)'
} elseif ($feat.State -ne 'Enabled') {
    $problems += "Windows Sandbox 功能状态 = $($feat.State) (需要 Enabled)"
}

# 2. 嵌套虚拟化: 沙箱本身是一台一次性虚机, 宿主机必须支持嵌套虚拟化。
$hv = Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction SilentlyContinue
if ($hv) {
    Write-Host ("host: {0}  HypervisorPresent={1}" -f $hv.Name, $hv.HypervisorPresent) -ForegroundColor DarkGray
}
$proc = Get-Process -Name 'vmcompute','vmms' -ErrorAction SilentlyContinue
Write-Host ("hyper-v services: vmcompute={0} vmms={1}" -f `
    (@($proc | Where-Object { $_.Name -eq 'vmcompute' }).Count), `
    (@($proc | Where-Object { $_.Name -eq 'vmms' }).Count)) -ForegroundColor DarkGray

# 3. 启动器在不在。
$launcher = Get-Command 'WindowsSandbox.exe' -ErrorAction SilentlyContinue
if (-not $launcher) {
    $problems += '找不到 WindowsSandbox.exe (沙箱启动器)'
}

if ($problems.Count -gt 0) {
    Write-Host ''
    Write-Host '[SKIP] 这台机器跑不了 Windows Sandbox 门禁:' -ForegroundColor Yellow
    foreach ($p in $problems) { Write-Host "       - $p" -ForegroundColor Yellow }
    Write-Host ''
    Write-Host '       需要一台带嵌套虚拟化的 Windows 11 机器 (自托管 runner, 标签 sandbox)。' -ForegroundColor Yellow
    exit 1
}

Write-Host ''
Write-Host '[OK] 这台机器可以跑 Windows Sandbox 门禁。' -ForegroundColor Green
exit 0
