# run-verify.ps1 - 主机侧触发器:「宣称完成的任务」沙箱复验 (仓库内版)
#
# 这个脚本是 CI 和本机共用的入口。它做四件事:
#   1. staging: 待测安装包 (新 + 升级基线) + 测试 exe -> <StagingRoot>\
#   2. 生成 Windows Sandbox 的 .wsb (路径运行时给, 不写死)
#   3. 启动沙箱 (一次性虚机, 崩了也只损失沙箱)
#   4. 等 claims.json 落盘并稳定, 打印逐条结论
#
# 用法:
#   .\run-verify.ps1
#   .\run-verify.ps1 -NoWait
#   .\run-verify.ps1 -TimeoutMinutes 30
#   .\run-verify.ps1 -RepoRoot D:\somewhere\else
#
# 退出码: 0 = 全部通过, 1 = 有未达标项, 2 = 超时或沙箱没跑起来

[CmdletBinding()]
param(
    [string]$InstallerPath    = '',
    [string]$OldInstallerPath = '',
    [string]$RepoRoot         = '',
    [string]$StagingRoot      = "$env:USERPROFILE\sandbox-artifacts\rime",
    [int]$TimeoutMinutes      = 25,
    [switch]$NoWait
)

$ErrorActionPreference = 'Stop'

# 仓库根 = 脚本所在目录的上一级 (scripts\sandbox-gate\ -> repo root)。
if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

# ---------- 0. 前置校验 ----------
# 待验安装包: 没有显式指定时, 自动挑 output\archives 里版本号最高的那个。
# 出新版本只要把包丢进 output\archives\ 即可, 不必改脚本。
if (-not $InstallerPath) {
    $archivesDir = Join-Path $RepoRoot 'output\archives'
    $cand = Get-ChildItem $archivesDir -Filter 'fluxing-*-installer.exe' -ErrorAction SilentlyContinue |
        ForEach-Object {
            $m = [regex]::Match($_.Name, '^fluxing-(\d+\.\d+\.\d+)\.(\d+)-installer\.exe$')
            if ($m.Success) {
                [pscustomobject]@{ File = $_; Ver = [version]($m.Groups[1].Value + '.' + $m.Groups[2].Value) }
            }
        } | Sort-Object Ver -Descending
    if (-not $cand) { throw "output\archives 里没有 fluxing-*-installer.exe: $archivesDir" }
    $InstallerPath = $cand[0].File.FullName
    Write-Host "[OK] auto-selected installer: $($cand[0].File.Name)" -ForegroundColor DarkGray
}
if (-not (Test-Path $InstallerPath)) { throw "Installer not found: $InstallerPath" }

# 升级基线 (用来跑「覆盖升级」那一组断言)。默认取 archives 里最老的那个 0.21.0.x,
# 但必须排除刚选中的待验包 —— CI 里往往只有一个包, 拿它自己当基线会让「覆盖升级」
# 变成 0.21.0.6 -> 0.21.0.6, 断言看着过了, 其实没验到任何东西。基线缺失时沙箱
# 会把 P8.x 标成 SKIP, 那比假 PASS 诚实。
if (-not $OldInstallerPath) {
    $old = Get-ChildItem (Join-Path $RepoRoot 'output\archives') -Filter 'fluxing-*-installer.exe' -ErrorAction SilentlyContinue |
        ForEach-Object {
            $m = [regex]::Match($_.Name, '^fluxing-(\d+\.\d+\.\d+)\.(\d+)-installer\.exe$')
            if ($m.Success) {
                [pscustomobject]@{ File = $_; Ver = [version]($m.Groups[1].Value + '.' + $m.Groups[2].Value) }
            }
        } | Where-Object { $_.File.FullName -ne (Resolve-Path $InstallerPath).Path } |
        Sort-Object Ver
    if ($old) { $OldInstallerPath = $old[0].File.FullName }
    else { Write-Host "[WARN] 没有可用的升级基线安装包, 覆盖升级那组断言会被标成 SKIP" -ForegroundColor Yellow }
}

# Windows Sandbox 的可选功能状态。查 -Online 需要管理员权限, CI 的
# self-hosted agent 不一定提权 —— 查不到不等于没装, 所以只有明确
# Disabled 才当失败, 查不到只告警 (probe-sandbox.ps1 会把原因说清楚)。
$feat = Get-WindowsOptionalFeature -Online -FeatureName "Containers-DisposableClientVM" -ErrorAction SilentlyContinue
if ($feat -and $feat.State -ne 'Enabled') {
    throw "Windows Sandbox 未启用 (状态 = $($feat.State)): Enable-WindowsOptionalFeature -Online -FeatureName Containers-DisposableClientVM"
}
if (-not $feat) {
    Write-Host "[WARN] 查不到 Windows Sandbox 的功能状态 (多半是没有管理员权限), 继续尝试启动" -ForegroundColor Yellow
}
if (-not (Test-Path (Join-Path $RepoRoot 'output'))) { throw "build output 不存在: $RepoRoot\output" }

# ---------- 1. 目录 ----------
$installers = Join-Path $StagingRoot "installers"
$tests      = Join-Path $StagingRoot "tests"
$logs       = Join-Path $StagingRoot "logs"
$screens    = Join-Path $StagingRoot "screenshots"
foreach ($d in @($StagingRoot, $installers, $tests, $logs, $screens)) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
}

# 清空上一轮结果 (关键: 否则会读到旧的 claims.json 误判成功)
foreach ($d in @($logs, $screens)) {
    Get-ChildItem $d -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
}
Get-ChildItem $installers -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem $tests     -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

# ---------- 2. staging 安装包 ----------
$staged = @()
foreach ($p in @($InstallerPath, $OldInstallerPath)) {
    if ($p -and (Test-Path $p)) {
        $dst = Join-Path $installers (Split-Path $p -Leaf)
        Copy-Item -Path $p -Destination $dst -Force
        $staged += $dst
        Write-Host "[OK] staged installer: $(Split-Path $dst -Leaf)" -ForegroundColor DarkGray
    } elseif ($p) {
        Write-Host "[WARN] 缺少安装包 (该阶段将跳过): $p" -ForegroundColor Yellow
    }
}

# ---------- 3. staging 测试 exe ----------
# 三处来源:
#   a) <repo>\Release\            —— 常规测试产物
#   b) <repo>Release\             —— 路径拼接 bug 落地的三个 Shift 测试
#   c) 单独构建的 TestShiftIPCStateMachine / TestShiftIPCWire / TestPipeChannelRace
$srcDirs = @(
    (Join-Path $RepoRoot 'Release'),
    ("$RepoRoot" + "Release")
)
$copied = 0
foreach ($d in $srcDirs) {
    if (-not (Test-Path $d)) { continue }
    Get-ChildItem $d -Filter 'Test*.exe' -ErrorAction SilentlyContinue | ForEach-Object {
        $dst = Join-Path $tests $_.Name
        if (-not (Test-Path $dst)) {
            Copy-Item -Path $_.FullName -Destination $dst -Force
            $copied++
        }
    }
}
Write-Host "[OK] staged $copied 个测试 exe -> $tests" -ForegroundColor DarkGray

# ---------- 4. 生成沙箱配置 + 启动 ----------
$wsb = Join-Path $StagingRoot "fluxing-sandbox.wsb"
& (Join-Path $PSScriptRoot 'New-SandboxConfig.ps1') `
    -ToolingDir $PSScriptRoot `
    -RepoRoot   $RepoRoot `
    -StagingRoot $StagingRoot `
    -OutputPath $wsb

# 4a. 关掉上一轮遗留的沙箱实例。
#     WindowsSandbox.exe 是启动器: 如果已有实例在跑, 再次启动只会把旧实例
#     拉到前台, LogonCommand 不再执行 —— 表现为「沙箱进程秒退 + 没有任何日志」。
#     必须先清干净, 保证这一轮是全新的一次性虚机。
$stale = @(Get-Process -Name 'WindowsSandboxClient','WindowsSandbox','vmmemWindowsSandbox' -ErrorAction SilentlyContinue)
if ($stale.Count) {
    Write-Host "[>] 清理上一轮遗留的沙箱实例 ($($stale.Count) 个)..." -ForegroundColor DarkGray
    foreach ($p in $stale) { try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {} }
    Start-Sleep -Seconds 5
    $still = @(Get-Process -Name 'WindowsSandboxClient','WindowsSandbox','vmmemWindowsSandbox' -ErrorAction SilentlyContinue)
    if ($still.Count) {
        Write-Host "[>] 强制重启 vmcompute (Win11 24H2 LogonCommand flake 的既定处置)" -ForegroundColor DarkGray
        Stop-Service vmcompute -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
        Start-Service vmcompute -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 5
    }
}

Write-Host ""
Write-Host "=== 启动 Windows Sandbox ===" -ForegroundColor Cyan
Write-Host "    config : $wsb"
Write-Host "    挂载 buildout = $RepoRoot\output"
Write-Host "    沙箱内自动执行: 升级场景 -> 干净装机 -> 不变式 -> TIP 注册比对 -> 单元测试 -> TSF 切 profile -> 面板自动隐藏 -> 卸载"
Write-Host ""

$proc = Start-Process -FilePath "WindowsSandbox.exe" -ArgumentList "`"$wsb`"" -PassThru
if ($NoWait) {
    Write-Host "[i] NoWait: 沙箱已启动, 结果稍后看 $logs\claims.json" -ForegroundColor Yellow
    return
}

# ---------- 5. 等待 ----------
$jsonPath  = Join-Path $logs "claims.json"
$logPath   = Join-Path $logs "claims.log"
$deadline  = (Get-Date).AddMinutes($TimeoutMinutes)
$lastSize  = -1
$stableCnt = 0
$found     = $false

Write-Host "[>] 等待沙箱结果 (上限 $TimeoutMinutes 分钟)..." -ForegroundColor Cyan
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 5
    if (Test-Path $jsonPath) {
        $size = (Get-Item $jsonPath).Length
        if ($size -eq $lastSize -and $size -gt 0) {
            $stableCnt++
            if ($stableCnt -ge 3) { $found = $true; break }
        } else {
            $stableCnt = 0
            $lastSize = $size
        }
    }
    # 沙箱窗口被关掉 -> 提前结束。
    # 注意: WindowsSandbox.exe 只是启动器, 它退出不代表沙箱结束;
    #       真正的沙箱进程是 WindowsSandboxClient.exe, 以它为准。
    if ($proc.HasExited) {
        $clientAlive = @(Get-Process -Name 'WindowsSandboxClient' -ErrorAction SilentlyContinue).Count -gt 0
        if (-not $clientAlive -and -not (Test-Path $jsonPath)) {
            Write-Host "[!] 沙箱客户端已退出但没有结果文件" -ForegroundColor Red
            break
        }
    }
}

if (-not $found) {
    Write-Host ""
    Write-Host "[FAIL] 超时或沙箱提前退出, 未拿到 claims.json" -ForegroundColor Red
    if (Test-Path $logPath) {
        Write-Host "--- 已落盘的部分日志 (最后 60 行) ---" -ForegroundColor DarkGray
        Get-Content $logPath -Tail 60 | ForEach-Object { Write-Host "  $_" }
    }
    exit 2
}

# ---------- 6. 报告 ----------
Write-Host ""
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " 沙箱复验结果 (claims.json)" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
$report = Get-Content $jsonPath -Raw -Encoding UTF8 | ConvertFrom-Json

Write-Host ("  通过 {0} / 未达标 {1} / 跳过 {2}  (共 {3} 项)" -f $report.passed, $report.failed, $report.skipped, $report.total) -ForegroundColor Yellow
Write-Host ""
foreach ($r in $report.results) {
    $color = switch ($r.status) { 'PASS' {'Green'} 'FAIL' {'Red'} 'SKIP' {'DarkYellow'} default {'Gray'} }
    Write-Host ("  [{0}] {1}" -f $r.status, $r.claim) -ForegroundColor $color
    if ($r.detail) { Write-Host ("         {0}" -f $r.detail) -ForegroundColor DarkGray }
}
Write-Host ""
Write-Host "  完整日志 : $logPath" -ForegroundColor DarkGray
Write-Host "  JSON 报告: $jsonPath" -ForegroundColor DarkGray
Write-Host "  截图     : $screens" -ForegroundColor DarkGray

if ($report.failed -gt 0) {
    Write-Host ""
    Write-Host "=== 未达标明细 ===" -ForegroundColor Red
    foreach ($f in $report.failures) { Write-Host "  - $f" -ForegroundColor Red }
    exit 1
}
Write-Host ""
Write-Host "=== 全部通过 ===" -ForegroundColor Green
exit 0
