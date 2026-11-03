<#
.SYNOPSIS
    Fluxing 回归用例运行器（ADR 0033 / test/regression/README.md）。

.DESCRIPTION
    发现 test/regression/cases/C####-*.ps1，按文件头的 CASE-LEVEL 筛级别，
    逐个用独立 pwsh 子进程跑，收集退出码与 RESULT 行，最后汇总。

    退出码契约（由 lib/CaseHarness.ps1 定义）：
        0 = PASS   1 = FAIL   2 = SKIP   其它 = ERROR（harness 异常 / 崩溃 / 超时）

    任何一个 FAIL / ERROR、或**一个用例都没跑到**，本脚本都以 1 退出。
    「没跑」不等于「通过」—— 这是 RC1 的直接教训。

.PARAMETER Level
    fast | heavy | all。默认 fast。heavy 用例通常需要真机或破坏性开关。

.PARAMETER Case
    只跑指定用例，接受 C0175 或 C0175-version-resource.ps1 两种写法。

.PARAMETER TimeoutSec
    单个用例超时（秒）。超时记 ERROR 并强杀进程树。

.PARAMETER LogDir
    stdout / stderr / result 落盘目录。默认 %TEMP%\fluxing-regression\<时间戳>。

.PARAMETER ListOnly
    只列出选中用例，不执行。

.EXAMPLE
    pwsh -NoProfile -File test/regression/run-cases.ps1 -Level fast

.EXAMPLE
    pwsh -NoProfile -File test/regression/run-cases.ps1 -Level heavy -Case C0174
#>
[CmdletBinding()]
param(
    [ValidateSet('fast', 'heavy', 'all')]
    [string]$Level = 'fast',
    [string[]]$Case = @(),
    [int]$TimeoutSec = 1800,
    [string]$LogDir = '',
    [switch]$ListOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$CasesDir = Join-Path $PSScriptRoot 'cases'

function Write-Summary {
    param([int]$Cases, [int]$Pass, [int]$Fail, [int]$Skip, [int]$Err, [string]$Log)
    Write-Host ''
    Write-Host ("REGRESSION RESULT level={0} cases={1} pass={2} fail={3} skip={4} error={5} log={6}" -f `
            $Level, $Cases, $Pass, $Fail, $Skip, $Err, $(if ($Log) { $Log } else { '-' }))
}

function Get-ResultDetail {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    $lines = @(Get-Content -LiteralPath $Path -Encoding utf8 -ErrorAction SilentlyContinue)
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match '^RESULT case=') {
            $m = [regex]::Match($lines[$i], 'detail=(.*)$')
            if ($m.Success) { return $m.Groups[1].Value.Trim() }
            return ''
        }
    }
    return ''
}

function Get-CaseMeta {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$File)

    if ($File.Name -notmatch '^(C\d{4})-(.+)\.ps1$') { return $null }
    $id = $Matches[1]
    $slug = $Matches[2]

    # 级别从文件头 5 行取，避免与 DEFECTS.yaml 各写一份却悄悄漂移。
    $lvl = 'unknown'
    foreach ($h in @(Get-Content -LiteralPath $File.FullName -TotalCount 5 -Encoding utf8)) {
        if ($h -match '^#\s*CASE-LEVEL:\s*(\S+)') { $lvl = $Matches[1] }
    }
    return [pscustomobject]@{
        Id       = $id
        Slug     = $slug
        Level    = $lvl
        Name     = $File.Name
        FullName = $File.FullName
    }
}

function Invoke-OneCase {
    param(
        [Parameter(Mandatory = $true)]$CaseMeta,
        [Parameter(Mandatory = $true)][string]$PwshExe,
        [Parameter(Mandatory = $true)][string]$LogDir,
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [int]$TimeoutSec = 1800
    )

    $resFile = Join-Path $LogDir ("{0}.result" -f $CaseMeta.Id)
    $outFile = Join-Path $LogDir ("{0}.stdout.log" -f $CaseMeta.Id)
    $errFile = Join-Path $LogDir ("{0}.stderr.log" -f $CaseMeta.Id)
    foreach ($p in @($resFile, $outFile, $errFile)) {
        if (Test-Path -LiteralPath $p) { Remove-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue }
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $oldRes = $env:FLUXING_CASE_RESULT_FILE
    $env:FLUXING_CASE_RESULT_FILE = $resFile

    $timedOut = $false
    $exitCode = -1
    $startError = ''
    try {
        $proc = Start-Process -FilePath $PwshExe `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $CaseMeta.FullName) `
            -WorkingDirectory $RepoRoot -NoNewWindow -PassThru `
            -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
            $timedOut = $true
            try { $proc.Kill($true) } catch { }
            try { $null = $proc.WaitForExit(10000) } catch { }
        }
        try { $exitCode = $proc.ExitCode } catch { $exitCode = -1 }
    }
    catch {
        $startError = $_.Exception.Message
    }
    finally {
        if ($null -eq $oldRes) { Remove-Item Env:\FLUXING_CASE_RESULT_FILE -ErrorAction SilentlyContinue }
        else { $env:FLUXING_CASE_RESULT_FILE = $oldRes }
        $sw.Stop()
    }

    if ($timedOut) {
        $status = 'ERROR'
        $detail = "超时 ${TimeoutSec}s，已强杀"
    }
    elseif ($startError) {
        $status = 'ERROR'
        $detail = "启动失败：$startError"
    }
    else {
        if ($exitCode -eq 0) { $status = 'PASS' }
        elseif ($exitCode -eq 1) { $status = 'FAIL' }
        elseif ($exitCode -eq 2) { $status = 'SKIP' }
        else { $status = 'ERROR' }
        $detail = Get-ResultDetail -Path $resFile
        if ([string]::IsNullOrWhiteSpace($detail)) {
            $detail = "exit=$exitCode（用例没有输出 RESULT 行）"
        }
    }

    return [pscustomobject]@{
        Id     = $CaseMeta.Id
        Name   = $CaseMeta.Name
        Status = $status
        Detail = $detail
        Sec    = [math]::Round($sw.Elapsed.TotalSeconds, 1)
        Exit   = $exitCode
    }
}

# ---------------------------------------------------------------------------
# 1. 发现用例
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $CasesDir)) {
    Write-Summary -Cases 0 -Pass 0 -Fail 0 -Skip 0 -Err 0 -Log ''
    Write-Host "找不到用例目录：$CasesDir" -ForegroundColor Red
    exit 1
}

$all = @(Get-ChildItem -LiteralPath $CasesDir -File -Filter 'C*.ps1' -ErrorAction SilentlyContinue |
        Sort-Object Name |
        ForEach-Object { Get-CaseMeta -File $_ } |
        Where-Object { $null -ne $_ })

# ---------------------------------------------------------------------------
# 2. 选用例
# ---------------------------------------------------------------------------
if ($Case.Count -gt 0) {
    $wanted = @()
    foreach ($c in $Case) {
        if ($c -match '^(C\d{4})') { $wanted += $Matches[1] } else { $wanted += $c.ToUpperInvariant() }
    }
    $missing = @($wanted | Where-Object { $all.Id -notcontains $_ })
    if ($missing.Count -gt 0) {
        Write-Summary -Cases 0 -Pass 0 -Fail 0 -Skip 0 -Err 0 -Log ''
        Write-Host ("-Case 指定的用例不存在：{0}" -f ($missing -join ', ')) -ForegroundColor Red
        Write-Host ("现有用例：{0}" -f (($all | ForEach-Object { $_.Id }) -join ', '))
        exit 1
    }
    $selected = @($all | Where-Object { $wanted -contains $_.Id })
}
elseif ($Level -eq 'all') {
    $selected = $all
}
else {
    $selected = @($all | Where-Object { $_.Level -eq $Level })
}

if ($ListOnly) {
    if ($selected.Count -eq 0) {
        Write-Host '（没有匹配的用例）' -ForegroundColor Yellow
        exit 0
    }
    foreach ($c in $selected) { Write-Host ("{0}  [{1}]  {2}" -f $c.Id, $c.Level, $c.Name) }
    exit 0
}

if ($selected.Count -eq 0) {
    Write-Summary -Cases 0 -Pass 0 -Fail 0 -Skip 0 -Err 0 -Log ''
    Write-Host ("没有任何 level={0} 的用例 —— 一个都没跑不能当绿。" -f $Level) -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
# 3. 准备日志目录与解释器
# ---------------------------------------------------------------------------
if ([string]::IsNullOrWhiteSpace($LogDir)) {
    $LogDir = Join-Path (Join-Path $env:TEMP 'fluxing-regression') (Get-Date -Format 'yyyyMMdd-HHmmss')
}
if (-not (Test-Path -LiteralPath $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}
$LogDir = (Resolve-Path -LiteralPath $LogDir).Path

$pwshExe = Join-Path $PSHOME 'pwsh.exe'
if (-not (Test-Path -LiteralPath $pwshExe)) { $pwshExe = (Get-Process -Id $PID).Path }

Write-Host ("REGRESSION level={0} cases={1} pwsh={2}" -f $Level, $selected.Count, $pwshExe)
Write-Host ("log -> {0}" -f $LogDir)

# ---------------------------------------------------------------------------
# 4. 跑
# ---------------------------------------------------------------------------
$pass = 0
$fail = 0
$skip = 0
$err = 0
$results = @()

foreach ($c in $selected) {
    $r = Invoke-OneCase -CaseMeta $c -PwshExe $pwshExe -LogDir $LogDir -RepoRoot $RepoRoot -TimeoutSec $TimeoutSec
    $results += $r

    switch ($r.Status) {
        'PASS' { $pass++ }
        'FAIL' { $fail++ }
        'SKIP' { $skip++ }
        default { $err++ }
    }
    $color = switch ($r.Status) {
        'PASS' { 'Green' }
        'FAIL' { 'Red' }
        'SKIP' { 'Yellow' }
        default { 'Magenta' }
    }
    Write-Host ("  [{0,-5}] {1}  {2}  ({3}s)" -f $r.Status, $r.Id, $r.Detail, $r.Sec) -ForegroundColor $color
}

# ---------------------------------------------------------------------------
# 5. 汇总
# ---------------------------------------------------------------------------
Write-Summary -Cases $selected.Count -Pass $pass -Fail $fail -Skip $skip -Err $err -Log $LogDir

$jsonPath = Join-Path $LogDir 'summary.json'
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $jsonPath -Encoding utf8

if (($fail + $err) -gt 0) { exit 1 }
exit 0
