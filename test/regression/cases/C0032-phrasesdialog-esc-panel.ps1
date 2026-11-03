# C0032 —— D0032：按 Esc 关「用户短语」窗口**不得**连带收起快捷设置栏（ADR 0032）
# CASE-LEVEL: heavy
# DEFECT: D0032
#
# 守的是什么：
#   自家 UI 进程销毁时发出的 profile 事件与「用户真的切走输入法」同形 ⇒ 按焦点顺序判别
#   必然两头都错；ADR 0032 改成按 IPC 会话的客户端身份判别（OwnUiClientExeName）。
#
# 为什么是 heavy：
#   要真机跑 UI 探针（probes/ReproEsc.cs）：起 server、发热键 9002 打开快捷设置栏、
#   打开用户短语窗、按 Esc、读窗口可见性。必须显式开关，否则 SKIP：
#     $env:FLUXING_ALLOW_UI_AUTOMATION = '1'
#
# 有齿（teeth）的做法：
#   同一个探针对「修复前」的行为必须返回 1（复现）、对「修复后」必须返回 0（未复现）。
#   本用例默认断言 0；把 $env:FLUXING_PROBE_EXPECT 设成 'reproduced' 后，
#   换回 pre-0032 的 server 再跑，探针必须返回 1 —— 反向对照不成立就说明探针是平凡探针。
#
# 已知限制（诚实记录，不假装）：
#   - 探针只在启动时枚举一次窗口句柄，轮次之间必须重跑（L173 的教训）；
#   - 覆盖「Esc 不应收起」这一条；「真切走输入法必须收起」需要人眼或另一份探针，见 DEFECTS.yaml 的 note。

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')

Initialize-Case -Id 'C0032' -Title '按 Esc 关用户短语窗口不连带收起快捷设置栏（真机 UI 探针）'

$hotkeyId = 9002
$installDir = 'D:\Program Files\fluxing\weasel'
if ($env:FLUXING_INSTALL_DIR) { $installDir = $env:FLUXING_INSTALL_DIR }
$expect = 'not-reproduced'
if ($env:FLUXING_PROBE_EXPECT) { $expect = $env:FLUXING_PROBE_EXPECT }
$expectRc = if ($expect -eq 'reproduced') { 1 } else { 0 }

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

try {
    $root = Get-RepoRoot
    $probeSrc = Join-Path $root 'test\regression\probes\ReproEsc.cs'
    $installedServer = Join-Path $installDir 'WeaselServer.exe'
    $buildServer = Join-Path $root 'output\Win32\WeaselServer.exe'

    Assert-Prerequisite ([bool]$env:FLUXING_ALLOW_UI_AUTOMATION -and $env:FLUXING_ALLOW_UI_AUTOMATION -eq '1') `
        '未设 FLUXING_ALLOW_UI_AUTOMATION=1：本用例会驱动真实键鼠/窗口，默认不执行' | Out-Null
    Assert-Prerequisite (Test-Path -LiteralPath $probeSrc) "缺探针源码：$probeSrc" | Out-Null
    Assert-Prerequisite (Test-Path -LiteralPath $installedServer) "缺已安装产品：$installedServer" | Out-Null

    if (Test-HasSkips) { throw [System.OperationCanceledException]::new('prerequisite-missing') }

    # 装的是不是当前构建？不是 ⇒ 测的就不是被测代码，判 SKIP（不判 FAIL）。
    if (Test-Path -LiteralPath $buildServer) {
        $same = (Get-Sha256 $installedServer) -eq (Get-Sha256 $buildServer)
        Assert-Prerequisite $same "已安装 WeaselServer.exe != output\Win32（被测代码不是当前构建 ⇒ 先部署再跑）" | Out-Null
        if (Test-HasSkips) { throw [System.OperationCanceledException]::new('prerequisite-missing') }
    }

    # 编译探针：源码进库（probes/），产物落 tmp（不进库）。csc 找不到 ⇒ SKIP。
    $workDir = Join-Path $env:TEMP 'fluxing-regression-c0032'
    if (-not (Test-Path -LiteralPath $workDir)) { New-Item -ItemType Directory -Path $workDir -Force | Out-Null }
    $probeExe = Join-Path $workDir 'ReproEsc.exe'

    $cscCandidates = @(
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'),
        (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe')
    )
    $csc = $cscCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not (Assert-Prerequisite ([bool]$csc) '找不到 csc.exe（.NET Framework 4.x），无法编译探针')) {
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    if (Test-Path -LiteralPath $probeExe) { Remove-Item -LiteralPath $probeExe -Force }
    $cscOut = & $csc /nologo /optimize+ "/out:$probeExe" $probeSrc 2>&1
    if (-not (Test-Path -LiteralPath $probeExe)) {
        Assert-Case $false ("探针编译失败：" + (($cscOut | Select-Object -First 5) -join ' '))
        throw [System.OperationCanceledException]::new('compile-failed')
    }
    Assert-Case $true "探针已编译：$probeExe"

    $server = Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not (Assert-Prerequisite ([bool]$server) 'WeaselServer.exe 没在跑（先启动输入法服务）')) {
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    # 每轮重跑探针（句柄只在启动时枚举一次，复用会得到陈旧结论）。
    & $probeExe $server.Id $hotkeyId | ForEach-Object { Write-CaseLine 'PROBE' $_ }
    $rc = $LASTEXITCODE
    Write-CaseLine 'PROBE' ("exit={0} expect={1} ({2})" -f $rc, $expectRc, $expect)
    Assert-Equal $rc $expectRc "探针结论（0=未复现 / 1=复现），期望 $expect" | Out-Null
}
catch [System.OperationCanceledException] {
}
finally {
    Complete-Case
}
