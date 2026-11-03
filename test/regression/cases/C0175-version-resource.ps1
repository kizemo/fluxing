# C0175 —— D0175：PE 版本资源 FileVersion 字符串表必须非空且等于 version.txt（L175）
# CASE-LEVEL: fast
# DEFECT: D0175
#
# 守的是什么：
#   四个 rc 里 `VALUE "FileVersion", FILE_VERSION`（宏展开成无引号数字）会让 rc.exe
#   把该条写成空串；改成 `STRZ(FILE_VERSION)` 才对。这个用例同时断言
#   「固定版本信息」与「字符串表」两层，缺一层就会被掩盖（L175 就是这么漏掉一整轮的）。
#
# 需要什么：output/Win32/ 下的构建产物。没有构建产物 ⇒ SKIP（不是 PASS）。

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')
. (Join-Path $PSScriptRoot '..\lib\PeVersion.ps1')

Initialize-Case -Id 'C0175' -Title 'PE 版本资源：固定信息与 FileVersion 字符串表都等于 version.txt'

# FluxingPhrasesDialog.exe 没有 .rc（独立 vcxproj），是已知缺口，不列入本用例；
# 见 docs/adr/0033-regression-asset-and-staged-gate.md 的「未做项」。
$artifacts = @('WeaselServer.exe', 'WeaselDeployer.exe', 'WeaselSetup.exe', 'weasel.dll', 'weaselx64.dll')

try {
    $root = Get-RepoRoot
    $versionFile = Join-Path $root 'version.txt'
    $artifactDir = Join-Path $root 'output\Win32'

    if (-not (Test-Path -LiteralPath $versionFile)) {
        Assert-Prerequisite $false "缺 version.txt（仓库根），无法判定期望版本" | Out-Null
    }
    $expected = ''
    if (Test-Path -LiteralPath $versionFile) {
        $expected = (Get-Content -LiteralPath $versionFile -Raw -Encoding utf8).Trim()
        Assert-Case ($expected -match '^\d+\.\d+\.\d+$') "version.txt 内容合法：'$expected'"
    }

    $present = @()
    foreach ($name in $artifacts) {
        $p = Join-Path $artifactDir $name
        if (Test-Path -LiteralPath $p) { $present += $p }
    }
    if (-not (Assert-Prerequisite ($present.Count -gt 0) "缺构建产物：$artifactDir 下没有 $($artifacts -join ' / ')（先跑 xbuild.bat weasel）")) {
        # 前置缺失 ⇒ 后面不再断言，直接走 finally 输出 SKIP。
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    foreach ($p in $present) {
        $name = Split-Path -Leaf $p
        $info = Get-PeVersionInfo -Path $p

        Assert-Case $info.HasVersionResource "$name 有版本资源（$($info.FixedVersion)）"
        # ↓ 这条就是 L175 的齿：改回 `VALUE "FileVersion", FILE_VERSION` 会立刻翻红。
        Assert-Case ($info.StringFileVersion -ne '') "$name 的 FileVersion 字符串非空"
        if ($info.StringFileVersion -ne '' -and $expected -ne '') {
            Assert-Case ($info.StringFileVersion -eq "$expected.0" -or $info.StringFileVersion -eq $expected) `
                "$name FileVersion='$($info.StringFileVersion)' 与 version.txt($expected) 一致"
        }
        if ($null -ne $info.FixedVersion -and $expected -ne '') {
            Assert-Equal $info.FixedVersion "$expected.0" "$name 固定版本信息" | Out-Null
        }
        Assert-Case $info.LayersAgree "$name 两层版本一致（固定 vs 字符串表）"
    }
}
catch [System.OperationCanceledException] {
    # 前置缺失已记入 SKIP 列表，交给 Complete-Case 输出 status=SKIP。
}
finally {
    Complete-Case
}
