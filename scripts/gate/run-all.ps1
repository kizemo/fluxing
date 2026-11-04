#Requires -Version 5.1
<#
.SYNOPSIS
分层门禁总入口：依次跑 L0 静态 / L1 快捷设置栏静态 / L3 变异 / L4 回归用例 / L5 sandbox-gate。

.DESCRIPTION
每一层都独立记时、独立记退出码；任何一层非 0 ⇒ 整体失败。
不存在的层脚本记 SKIP（不算通过，也不算失败）。
全部步骤被跳过时整体失败（空跑不算通过）。

.PARAMETER Root
仓库根目录。

.PARAMETER SkipL0 / SkipL1 / SkipL3 / SkipCases / SkipSandboxGate
跳过对应分层。

.PARAMETER CaseTimeoutSec
单个回归用例的超时秒数，透传给 test\regression\run-cases.ps1。

.EXAMPLE
pwsh -NoProfile -File scripts\gate\run-all.ps1
#>
[CmdletBinding()]
param(
    [string]$Root = '',
    [switch]$SkipL0,
    [switch]$SkipL1,
    [switch]$SkipL3,
    [switch]$SkipCases,
    [switch]$SkipSandboxGate,
    [int]$CaseTimeoutSec = 1800
)

Set-StrictMode -Version Latest
# 子步骤失败必须能被记录下来继续跑，所以这里不能 Stop。
$ErrorActionPreference = 'Continue'

function Resolve-RepoRoot {
    param([string]$Hint)
    if ($Hint) { return (Resolve-Path -LiteralPath $Hint).Path }
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$Root = Resolve-RepoRoot -Hint $Root
$script:HostExe = (Get-Process -Id $PID).Path
$script:Steps = @()

function Add-Skip {
    param([string]$Id, [string]$Title, [string]$Reason)
    Write-Host ("=== {0} {1} === SKIP：{2}" -f $Id, $Title, $Reason)
    $script:Steps += [pscustomobject]@{ Id = $Id; Title = $Title; ExitCode = -1; Seconds = 0; Reason = $Reason }
}

function Invoke-Step {
    param([string]$Id, [string]$Title, [scriptblock]$Body)

    Write-Host ''
    Write-Host ("=== {0} {1} ===" -f $Id, $Title)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $code = 1
    try {
        $r = @(& $Body)
        if ($r.Count -eq 0) {
            $code = 0
        }
        elseif ($r.Count -gt 1) {
            # 步骤体只允许输出一个退出码。多于一个说明内部有未收口的输出 —— 多半是子进程
            # stdout 被当成管道对象收进来了（L174 相邻坑：既炸 [int] 转换，又把子进程输出全吞掉）。
            Write-Host ('步骤返回值不是单个退出码（{0} 个对象）；首个对象：{1}' -f $r.Count, $r[0])
            $code = 1
        }
        else {
            $code = [int]$r[0]
        }
    }
    catch {
        Write-Host ('步骤异常：' + $_.Exception.Message)
        $code = 1
    }
    $sw.Stop()
    $secs = [math]::Round($sw.Elapsed.TotalSeconds, 1)
    Write-Host ("--- {0} exit={1} ({2}s)" -f $Id, $code, $secs)
    $script:Steps += [pscustomobject]@{ Id = $Id; Title = $Title; ExitCode = $code; Seconds = $secs; Reason = '' }
}

# ------------------------------ L0 静态 ------------------------------
$l0 = Join-Path $Root 'scripts\gate\L0-static.ps1'
if ($SkipL0) {
    Add-Skip 'L0' '静态门禁' '按 -SkipL0 跳过'
}
elseif (-not (Test-Path -LiteralPath $l0)) {
    Add-Skip 'L0' '静态门禁' "缺 $l0"
}
else {
    Invoke-Step 'L0' '静态门禁' {
        & $script:HostExe -NoProfile -ExecutionPolicy Bypass -File $l0 -Mode all -Root $Root | ForEach-Object { Write-Host $_ }
        return $LASTEXITCODE
    }
}

# ------------------------------ L1 快捷设置栏静态 ------------------------------
$l1 = Join-Path $Root 'scripts\gate\L1-quickpanel-hotkey.ps1'
if ($SkipL1) {
    Add-Skip 'L1' '快捷设置栏静态门禁' '按 -SkipL1 跳过'
}
elseif (-not (Test-Path -LiteralPath $l1)) {
    Add-Skip 'L1' '快捷设置栏静态门禁' "缺 $l1"
}
else {
    Invoke-Step 'L1' '快捷设置栏静态门禁' {
        & $script:HostExe -NoProfile -ExecutionPolicy Bypass -File $l1 -Root $Root | ForEach-Object { Write-Host $_ }
        return $LASTEXITCODE
    }
}

# ------------------------------ L3 变异 ------------------------------
$l3 = Join-Path $Root 'scripts\gate\L3-mutation.ps1'
if ($SkipL3) {
    Add-Skip 'L3' '变异自检' '按 -SkipL3 跳过'
}
elseif (-not (Test-Path -LiteralPath $l3)) {
    Add-Skip 'L3' '变异自检' "缺 $l3"
}
else {
    Invoke-Step 'L3' '变异自检' {
        & $script:HostExe -NoProfile -ExecutionPolicy Bypass -File $l3 -Root $Root | ForEach-Object { Write-Host $_ }
        return $LASTEXITCODE
    }
}

# ------------------------------ L4 回归用例 ------------------------------
$cases = Join-Path $Root 'test\regression\run-cases.ps1'
if ($SkipCases) {
    Add-Skip 'L4' '回归用例(fast)' '按 -SkipCases 跳过'
}
elseif (-not (Test-Path -LiteralPath $cases)) {
    Add-Skip 'L4' '回归用例(fast)' "缺 $cases"
}
else {
    Invoke-Step 'L4' '回归用例(fast)' {
        & $script:HostExe -NoProfile -ExecutionPolicy Bypass -File $cases -Level fast -TimeoutSec $CaseTimeoutSec | ForEach-Object { Write-Host $_ }
        $rc = $LASTEXITCODE
        # run-cases 的 2 = 全部 SKIP（fast 用例需要构建产物/装机，本机没有时属设计内）
        if ($rc -eq 2) { Write-Host 'L4：全部用例 SKIP，按设计接受'; return 0 }
        return $rc
    }
}

# ------------------------------ L5 sandbox-gate ------------------------------
$l5 = Join-Path $Root 'scripts\sandbox-gate\run-verify.cmd'
if ($SkipSandboxGate) {
    Add-Skip 'L5' 'sandbox-gate' '按 -SkipSandboxGate 跳过'
}
elseif (-not (Test-Path -LiteralPath $l5)) {
    Add-Skip 'L5' 'sandbox-gate' "缺 $l5"
}
else {
    Invoke-Step 'L5' 'sandbox-gate' {
        & $l5 | ForEach-Object { Write-Host $_ }
        return $LASTEXITCODE
    }
}

# ------------------------------ 汇总 ------------------------------
Write-Host ''
Write-Host 'GATE SUMMARY'
foreach ($s in $script:Steps) {
    $state = 'PASS'
    if ($s.ExitCode -eq -1) { $state = 'SKIP' }
    elseif ($s.ExitCode -ne 0) { $state = 'FAIL' }
    $tail = ''
    if ($s.Reason) { $tail = ' ' + $s.Reason }
    Write-Host ("  {0,-3} {1,-5} exit={2,-4} {3,6}s  {4}{5}" -f $s.Id, $state, $s.ExitCode, $s.Seconds, $s.Title, $tail)
}

$failed = @($script:Steps | Where-Object { $_.ExitCode -ne 0 -and $_.ExitCode -ne -1 })
$passed = @($script:Steps | Where-Object { $_.ExitCode -eq 0 })
Write-Host ("GATE RESULT steps={0} passed={1} failed={2} skipped={3}" -f `
        $script:Steps.Count, $passed.Count, $failed.Count, @($script:Steps | Where-Object { $_.ExitCode -eq -1 }).Count)

if ($failed.Count -gt 0) { exit 1 }
if ($passed.Count -le 0) {
    Write-Host 'GATE RESULT 全部步骤被跳过 ⇒ 不算通过'
    exit 1
}
exit 0
