# C0177 —— D0177：Ctrl+Shift+数字 删候选词功能（含双拼 chord 场景）整块丢失
# CASE-LEVEL: fast
# DEFECT: D0177
#
# 守的是什么（三条，缺一条就会被「以为还活着」骗过去）：
#   1. 判定表：MatchCandidateDeleteShortcut / ShouldInterceptCandidateDelete 的
#      组合与拦截边界。由 Release\TestCandidateRButtonDown.exe 的 T1–T11 断言。
#   2. 接线：TSF 按键链真的还在调这两个函数。单元测试不 link KeyEventSink.cpp
#      （那是 TSF 宿主的一部分），删掉调用点单测依然全绿 ⇒ 必须做源码结构断言。
#   3. 配置：default.yaml 的 ascii_composer 不能再接管左右 Shift/Ctrl。
#      改回 commit_code/clear ⇒ 按下第二个修饰键时候选窗消失，chord 打不出来。
#
# 需要什么：Release\TestCandidateRButtonDown.exe。
#   build.bat release weasel 会把测试项目一起建到 Release\（不在 output\Win32\）。
#   缺二进制 ⇒ SKIP，不是 PASS。
#
# 边界（诚实写明）：单元级 + 接线级 + 配置级守卫，**不替代**真机复验。
#   真机判据（用户 2026-11-06 已实测通过）：左修饰键，两种顺序各测一次
#   （Ctrl→Shift→1/2/3 与 Shift→Ctrl→1/2/3）都能删词，且按下第二个修饰键时
#   候选窗不消失；双拼下打完 chord 出现候选词后再打一个字母不上屏。

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')

# 取 ascii_composer.switch_key 块里的修饰键取值。到下一个列 0 的顶层键为止。
function Get-AsciiComposerSwitchKeys {
    param([string]$Path)
    $lines = @(Get-Content -LiteralPath $Path -Encoding utf8)
    $inBlock = $false
    $seen = [ordered]@{}
    foreach ($line in $lines) {
        if ($line -match '^ascii_composer:\s*$') { $inBlock = $true; continue }
        if ($inBlock -and $line -match '^\S') { break }
        if (-not $inBlock) { continue }
        if ($line -match '^\s+(Shift_L|Shift_R|Control_L|Control_R):\s*(\S+)') {
            $seen[$Matches[1]] = $Matches[2]
        }
    }
    return $seen
}

try {
    $root = Get-RepoRoot
    $header = Join-Path $root 'include\FluxingCandidateDeleteShortcut.h'
    $sink = Join-Path $root 'WeaselTSF\KeyEventSink.cpp'
    $yaml = Join-Path $root 'output\data\default.yaml'

    foreach ($p in @($header, $sink, $yaml)) {
        if (-not (Assert-Prerequisite (Test-Path -LiteralPath $p) "缺 $p")) {
            throw [System.OperationCanceledException]::new('prerequisite-missing')
        }
    }

    # --- 齿 1：共享判定头的 API 面 -------------------------------------------
    $hdrText = Get-Content -LiteralPath $header -Raw -Encoding utf8
    Assert-Case ($hdrText -match 'MatchCandidateDeleteShortcut\s*\(') `
        'FluxingCandidateDeleteShortcut.h 仍提供 MatchCandidateDeleteShortcut'
    Assert-Case ($hdrText -match 'ShouldInterceptCandidateDelete\s*\(') `
        'FluxingCandidateDeleteShortcut.h 仍提供 ShouldInterceptCandidateDelete'

    # --- 齿 2：TSF 按键链接线（删掉调用点 ⇒ 单测照样绿，所以必须查源码） -------
    $sinkText = Get-Content -LiteralPath $sink -Raw -Encoding utf8
    Assert-Case ($sinkText -match '#include\s*<FluxingCandidateDeleteShortcut\.h>') `
        'KeyEventSink.cpp 仍包含该头'
    Assert-Case ($sinkText -match 'fluxing::MatchCandidateDeleteShortcut\s*\(') `
        'KeyEventSink.cpp 仍调用 MatchCandidateDeleteShortcut'
    Assert-Case ($sinkText -match 'fluxing::ShouldInterceptCandidateDelete\s*\(') `
        'KeyEventSink.cpp 仍调用 ShouldInterceptCandidateDelete'
    Assert-Case ($sinkText -match '_DeleteCandidateOnCurrentPage\s*\(') `
        'KeyEventSink.cpp 仍落到 _DeleteCandidateOnCurrentPage'

    # --- 齿 3：ascii_composer 不许抢修饰键 -----------------------------------
    $keys = Get-AsciiComposerSwitchKeys -Path $yaml
    foreach ($name in @('Shift_L', 'Shift_R', 'Control_L', 'Control_R')) {
        $val = $null
        if ($keys.Contains($name)) { $val = $keys[$name] }
        Assert-Case ($val -eq 'noop') `
            "default.yaml ascii_composer.switch_key.$name = '$val'（必须 noop，否则按下第二个修饰键时候选窗消失）"
    }

    # --- 齿 4：判定表单元二进制真的跑绿 ---------------------------------------
    $bin = Join-Path $root 'Release\TestCandidateRButtonDown.exe'
    if (-not (Test-Path -LiteralPath $bin)) {
        Assert-Prerequisite $false "缺 Release\TestCandidateRButtonDown.exe（先跑 build.bat release weasel）" | Out-Null
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }
    $out = & $bin 2>&1 | Out-String
    $code = $LASTEXITCODE
    $m = [regex]::Match($out, '(\d+)\s*/\s*(\d+)\s+assertions passed')
    Assert-Case ($m.Success) '单元二进制输出了断言汇总行（格式变了要更新本用例，不许静默跳过）'
    if ($m.Success) {
        $passedN = [int]$m.Groups[1].Value
        $totalN = [int]$m.Groups[2].Value
        Assert-Equal $code 0 'TestCandidateRButtonDown 退出码' | Out-Null
        # T7 是 Ctrl/Shift 两种顺序的判定表，T8 是拦截边界，T10 是 hover_type:none 下的坐标命中。
        Assert-Case ($passedN -eq $totalN -and $totalN -ge 11) `
            "TestCandidateRButtonDown 全绿：$passedN / $totalN"
    }
}
catch [System.OperationCanceledException] {
    # 前置缺失已记入 SKIP 列表，交给 Complete-Case 输出 status=SKIP。
}
finally {
    Complete-Case
}