# CaseHarness.ps1 —— 回归用例骨架
#
# 契约（详见 test/regression/README.md §2）：
#   case-id / 前置可判（缺前置 ⇒ SKIP + exit 2）/ finally 自清理 / 退出码即结论 / 不依赖 tmp/
#
# 用法：
#   . (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')
#   Initialize-Case -Id 'C0175' -Title '版本资源 FileVersion 非空'
#   try {
#       Assert-Case -Condition (...) -Message '...'
#   } finally {
#       Complete-Case        # 跑清理栈 → 写结果 → exit
#   }
#
# 退出码：0=PASS 1=FAIL 2=SKIP 3=harness 异常

Set-StrictMode -Version Latest

$script:CaseId = 'C0000'
$script:CaseTitle = ''
$script:CaseFailures = New-Object System.Collections.Generic.List[string]
$script:CaseSkips = New-Object System.Collections.Generic.List[string]
$script:Cleanups = New-Object System.Collections.Generic.List[object]
$script:CaseCompleted = $false

function Get-RepoRoot {
    # lib -> regression -> test -> repo root
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
}

function Write-CaseLine {
    param([string]$Level, [string]$Message)
    Write-Host ("[{0}] {1}" -f $Level, $Message)
}

function Initialize-Case {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][string]$Title
    )
    $script:CaseId = $Id
    $script:CaseTitle = $Title
    Write-CaseLine 'CASE' ("{0} {1}" -f $Id, $Title)
    Write-CaseLine 'ENV' ("pwsh={0} os={1} repo={2}" -f $PSVersionTable.PSVersion, [Environment]::OSVersion.VersionString, (Get-RepoRoot))
}

# 清理栈：后注册的先执行；单条清理失败不影响其它清理，但要记进 CLEANUP_FAIL。
function Register-Cleanup {
    param(
        [Parameter(Mandatory = $true)][scriptblock]$Action,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $script:Cleanups.Add([pscustomobject]@{ Name = $Name; Action = $Action })
}

function Assert-Case {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if ($Condition) {
        Write-CaseLine 'PASS' $Message
    }
    else {
        Write-CaseLine 'FAIL' $Message
        $script:CaseFailures.Add($Message)
    }
    return $Condition
}

# 断言两值相等，失败时把两边都打出来（排查时最有用的一条信息）。
function Assert-Equal {
    param(
        [Parameter(Mandatory = $true)]$Actual,
        [Parameter(Mandatory = $true)]$Expected,
        [Parameter(Mandatory = $true)][string]$Message
    )
    $ok = ("$Actual" -ceq "$Expected")
    if ($ok) {
        Write-CaseLine 'PASS' ("{0} (={1})" -f $Message, $Actual)
    }
    else {
        Write-CaseLine 'FAIL' ("{0}: actual='{1}' expected='{2}'" -f $Message, $Actual, $Expected)
        $script:CaseFailures.Add($Message)
    }
    return $ok
}

# 缺前置：不静默通过，也不判失败，单独一档。
function Assert-Prerequisite {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        Write-CaseLine 'SKIP' $Message
        $script:CaseSkips.Add($Message)
    }
    return $Condition
}

function Test-HasSkips { return ($script:CaseSkips.Count -gt 0) }

function Complete-Case {
    if ($script:CaseCompleted) { return }
    $script:CaseCompleted = $true

    $cleanupFailures = 0
    for ($i = $script:Cleanups.Count - 1; $i -ge 0; $i--) {
        $c = $script:Cleanups[$i]
        try {
            & $c.Action
            Write-CaseLine 'CLEAN' ("{0} ok" -f $c.Name)
        }
        catch {
            $cleanupFailures++
            Write-CaseLine 'CLEAN' ("{0} FAILED: {1}" -f $c.Name, $_.Exception.Message)
        }
    }

    $status = 'PASS'
    $code = 0
    if ($script:CaseFailures.Count -gt 0) { $status = 'FAIL'; $code = 1 }
    elseif ($script:CaseSkips.Count -gt 0) { $status = 'SKIP'; $code = 2 }
    elseif ($cleanupFailures -gt 0) { $status = 'FAIL'; $code = 1 }

    $detail = if ($script:CaseFailures.Count -gt 0) {
        ($script:CaseFailures -join ' | ')
    }
    elseif ($script:CaseSkips.Count -gt 0) {
        ($script:CaseSkips -join ' | ')
    }
    else { 'ok' }

    $line = "RESULT case={0} status={1} cleanup_failures={2} detail={3}" -f `
        $script:CaseId, $status, $cleanupFailures, $detail
    Write-Host $line

    # 运行器（run-cases.ps1）用这个环境变量把结果行汇总成一张表。
    if ($env:FLUXING_CASE_RESULT_FILE) {
        try { Add-Content -LiteralPath $env:FLUXING_CASE_RESULT_FILE -Value $line -Encoding utf8 }
        catch { Write-CaseLine 'WARN' ("could not append result line: {0}" -f $_.Exception.Message) }
    }

    exit $code
}
