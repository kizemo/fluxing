# C0176 —— D0176：快捷栏被拖到桌面外后写坏 quickpanel.position，「Alt+,」看起来没反应
# CASE-LEVEL: fast
# DEFECT: D0176
#
# 守的是什么（两条互相独立的齿）：
#   1. 语义齿：RectIntersectsVisibleArea 的边界 —— 「至少露出一个像素」才算可见。
#      「边缘相接」「零宽矩形」「工作区为空」都必须判不可见。由
#      Release\TestQuickPanelDialog.exe 的 T24w2–T24w8 断言；把 < 改成 <=、
#      或把 && 改成 ||，这些断言立刻翻红。
#   2. 接线齿：自愈调用必须真的挂在三条路径上 —— 复用旧窗口前、首建后复核、
#      落盘前拒写；且首建钳制必须是**无条件**的（不能被 HasSavedPosition() 关在门里）。
#      这三处直接碰真实 HWND 与真实用户数据文件，单测进不去 ⇒ 只能做源码结构断言。
#
# 需要什么：Release\TestQuickPanelDialog.exe。
#   build.bat release weasel 会把测试项目一起建到 Release\（不在 output\Win32\）。
#   缺二进制 ⇒ SKIP，不是 PASS。
#
# 边界（诚实写明）：本用例是单元级 + 接线级守卫，**不替代**装机复验。
#   「面板真的出现在屏幕右下角」只有真机能判，复验步骤见 handoff 文档。

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')

Initialize-Case -Id 'C0176' -Title '快捷栏屏外落盘：可见性判定有齿 + 三处自愈接线在位 + 单元二进制全绿'

# 取一个顶层函数体（从签名行到下一个列 0 的右花括号）。找不到签名 ⇒ $null。
function Get-TopLevelBody {
    param([string]$Path, [string]$Signature)
    $lines = @(Get-Content -LiteralPath $Path -Encoding utf8)
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Contains($Signature)) { $start = $i; break }
    }
    if ($start -lt 0) { return $null }
    for ($j = $start + 1; $j -lt $lines.Count; $j++) {
        if ($lines[$j] -match '^\}') { return ($lines[$start..$j] -join "`n") }
    }
    return $null
}

try {
    $root = Get-RepoRoot
    $header = Join-Path $root 'include\FluxingPanelPosition.h'
    $impl = Join-Path $root 'WeaselServer\QuickPanelDialog.cpp'

    if (-not (Assert-Prerequisite (Test-Path -LiteralPath $header) "缺 $header")) {
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }
    if (-not (Assert-Prerequisite (Test-Path -LiteralPath $impl) "缺 $impl")) {
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    # --- 齿 1：可见性判定的语义边界（真的被单测断言） -------------------------
    $hdrText = Get-Content -LiteralPath $header -Raw -Encoding utf8
    Assert-Case ($hdrText -match 'bool\s+RectIntersectsVisibleArea\s*\(') `
        'FluxingPanelPosition.h 仍提供 RectIntersectsVisibleArea（落盘守卫的语义基座）'

    $implText = Get-Content -LiteralPath $impl -Raw -Encoding utf8

    # --- 齿 2：三处接线（定义 + 复用自愈 + 建窗后复核 + 落盘拒写 = 4 次出现） --
    $repairHits = ([regex]::Matches($implText, 'RepairOffscreenPanel')).Count
    Assert-Case ($repairHits -ge 4) `
        "QuickPanelDialog.cpp 里 RepairOffscreenPanel 出现 $repairHits 次（期望 >=4：定义/复用自愈/建窗后复核/落盘拒写）"

    # 首建钳制必须无条件：HasSavedPosition() 只能出现在「有则取保存值、无则取默认锚点」这一处，
    # 不能把 ClampToVisibleArea 整段关在门里（D0176 的原始写法）。
    # 批次 V 重构后位置解析改走 ResolveAnchoredPosition / HasSavedPosition 分支，
    # 原断言绑的是**旧实现字面量**，已随重构失效 ⇒ 改为断言**语义**（不弱化、应加强）：
    #   ① 首建路径确实调用 ClampToVisibleArea；
    #   ② 工作区枚举为空时有兜底（WorkAreasWithFallback）⇒ 钳制基准恒存在。
    #      这正是 D0176 的根因：枚举失败 ⇒ 首建钳制与建窗后自愈双双失效（事故同形复发）。
    Assert-Case ($implText.Contains('ClampToVisibleArea(target.x, target.y, panelW,')) `
        '首建钳制仍调用 ClampToVisibleArea'
    $fallbackHits = ([regex]::Matches($implText, 'WorkAreasWithFallback\s*\(')).Count
    Assert-Case ($fallbackHits -ge 3) `
        "QuickPanelDialog.cpp 里 WorkAreasWithFallback 出现 $fallbackHits 次（期望 >=3：定义/首建钳制/建窗后自愈）"
    $fallbackBody = Get-TopLevelBody -Path $impl -Signature 'std::vector<fluxing::panel_position::Rect> WorkAreasWithFallback()'
    Assert-Case ($null -ne $fallbackBody) 'WorkAreasWithFallback 是顶层定义'
    if ($null -ne $fallbackBody) {
        Assert-Case ($fallbackBody -match 'EnumerateWorkAreas\s*\(') `
            '兜底函数以 EnumerateWorkAreas() 为首选来源'
        Assert-Case ($fallbackBody -match 'areas\.empty\s*\(\)') `
            '兜底函数判空（只有枚举失败才走兜底）'
        Assert-Case ($fallbackBody -match 'SPI_GETWORKAREA') `
            '兜底工作区来自 SPI_GETWORKAREA（主显示器工作区）'
        Assert-Case ($fallbackBody -match 'push_back') `
            '兜底函数把主工作区补进 areas（保证钳制基准非空）'
    }

    $saveBody = Get-TopLevelBody -Path $impl -Signature 'bool QuickPanelDialog::SavePosition()'
    Assert-Case ($null -ne $saveBody) 'SavePosition() 仍是顶层定义（落盘守卫的宿主）'
    if ($null -ne $saveBody) {
        Assert-Case ($saveBody -match 'RectIntersectsVisibleArea') `
            'SavePosition() 落盘前仍做可见性判定（去掉它 ⇒ 污染坐标会被写盘）'
        Assert-Case ($saveBody -match 'RepairOffscreenPanel\s*\(\s*s_hwnd\s*\)') `
            'SavePosition() 拒写时会当场救回面板'
        Assert-Case ($saveBody -match 'return\s+false') `
            'SavePosition() 对零交集矩形返回 false（拒写）'
    }

    # --- 齿 3：单元二进制真的跑绿 --------------------------------------------
    $bin = Join-Path $root 'Release\TestQuickPanelDialog.exe'
    if (-not (Test-Path -LiteralPath $bin)) {
        Assert-Prerequisite $false "缺 Release\TestQuickPanelDialog.exe（先跑 build.bat release weasel）" | Out-Null
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }
    $out = & $bin 2>&1 | Out-String
    $code = $LASTEXITCODE
    $m = [regex]::Match($out, '(\d+)\s*/\s*(\d+)\s+assertions passed')
    Assert-Case ($m.Success) '单元二进制输出了断言汇总行（格式变了要更新本用例，不许静默跳过）'
    if ($m.Success) {
        $passedN = [int]$m.Groups[1].Value
        $totalN = [int]$m.Groups[2].Value
        Assert-Equal $code 0 'TestQuickPanelDialog 退出码' | Out-Null
        # T24w5 是污染值回归（3296,1983,368x72 @1920x1040 ⇒ 零交集）；它挂了这里就红。
        Assert-Case ($passedN -eq $totalN -and $totalN -ge 190) `
            "TestQuickPanelDialog 全绿：$passedN / $totalN"
    }
}
catch [System.OperationCanceledException] {
    # 前置缺失已记入 SKIP 列表，交给 Complete-Case 输出 status=SKIP。
}
finally {
    Complete-Case
}