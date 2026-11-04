#Requires -Version 7.0
# 宿主要求：PowerShell 7（pwsh）。
# 原因：共享库 scripts/gate/lib/GateChecks.ps1 是「UTF-8 无 BOM + 中文」，
#       Windows PowerShell 5.1 会按 ANSI(GBK) 读它，中文字面量的右引号被吃掉 ⇒ ParserError。
# 依据：CI(.github/workflows/*.yml) 全部 `shell: pwsh`；run-all.ps1 用当前进程路径当子门禁宿主。
<#
.SYNOPSIS
    L1 静态门禁(spec 099):快捷设置栏(quick panel)的 DPI 单一真相源 / 状态回显 / 键盘可达性 / tooltip 接线。

.DESCRIPTION
    这一层只读源码 —— 不编译、不装机、不碰用户数据。守的是 spec 099 P0-4/P0-5/P0-6 的**接线**:

      A  四个新 header 存在且非空(纯逻辑单一真相源,constitution R7)
      B  DPI 缩放只走 include/FluxingPanelDpi.h —— 生产代码不自己抄一份公式
      C  状态回显接线:provider 注入 + 轮询 timer 拉快照 + 启动期预热(显示路径零 I/O,constitution P2)
      D  键盘可达性接线:HandleKey 裁决 + WM_CHAR 吞键 + WM_SETFOCUS 进交互态 + 焦点归还
      E  tooltip 接线:ShowTooltip / HideTooltip / DestroyWindow + header include
      F  显示路径零 I/O:QuickPanelDialog.cpp 里不许有 CreateFile / 等待 / Sleep / 同步注册表
      G  用户可见文案简体中文(constitution P5)
      H  IPC 边界未被破坏:FluxingPanel*.h 不得引入 WeaselIPCData.h

    退出码:0 = 没有 FAIL(SKIP 不算失败);1 = 有 FAIL,或一个检查都没真正跑到(空跑不算通过)。

    编码说明:本文件用 UTF-8 + BOM + CRLF。**期望文案由码点构造**(见 $script:RequiredZh),
    不依赖脚本文件自身的编码 —— 即使 BOM 被某个编辑器抹掉,检查结果也不变。

    为什么不用 lib\GateChecks.ps1 里的现成函数:spec 099 的判据是这一批新资产独有的,
    写在本文件里(不污染共享库);共享库只借用 New-CheckResult 的结果形状。

.PARAMETER Root
    被检查的源码树根目录。默认由 $PSScriptRoot\..\.. 推导。
    指向临时副本 ⇒ 可对副本做变异测试,证明这门禁真的有牙。

.PARAMETER SkipText
    跳过 G 组(用户可见文案检查)。

.EXAMPLE
    pwsh -NoProfile -File scripts\gate\L1-quickpanel-hotkey.ps1
    pwsh -NoProfile -File scripts\gate\L1-quickpanel-hotkey.ps1 -Root C:\temp\mutant
#>
[CmdletBinding()]
param(
    [string]$Root = '',
    [switch]$SkipText
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    param([string]$Hint)
    if ($Hint) { return (Resolve-Path -LiteralPath $Hint).Path }
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$script:RepoRoot = Resolve-RepoRoot -Hint $Root

# 被检查的文件(相对 $script:RepoRoot)。故意只列这一小撮 ⇒ 变异测试只要复制这几个文件。
$script:RelDpi      = 'include\FluxingPanelDpi.h'
$script:RelStatus   = 'include\FluxingPanelStatus.h'
$script:RelKeyboard = 'include\FluxingPanelKeyboard.h'
$script:RelTooltip  = 'include\FluxingPanelTooltip.h'
$script:RelPanelCpp = 'WeaselServer\QuickPanelDialog.cpp'
$script:RelAppCpp   = 'WeaselServer\WeaselServerApp.cpp'
$script:RelIpcData  = 'include\WeaselIPCData.h'

$script:NewHeaders = @($script:RelDpi, $script:RelStatus, $script:RelKeyboard, $script:RelTooltip)

# 用户可见文案必须出现的中文串(码点构造 ⇒ 不依赖脚本文件编码)。
$script:RequiredZh = @(
    [pscustomobject]@{ Hex = '5207 6362 5230 4E0B 4E00 4E2A 8F93 5165 65B9 6848'; Text = '切换到下一个输入方案' },
    [pscustomobject]@{ Hex = '7528 6237 77ED 8BED';                                   Text = '用户短语' },
    [pscustomobject]@{ Hex = '5FEB 6377 952E 8BBE 7F6E';                             Text = '快捷键设置' },
    [pscustomobject]@{ Hex = '5173 4E8E 0020 002F 0020 8D26 53F7 8BF4 660E';         Text = '关于 / 账号说明' },
    [pscustomobject]@{ Hex = '5F53 524D 8F93 5165 003A';                             Text = '当前输入:' },
    [pscustomobject]@{ Hex = '5E38 9A7B 663E 793A 003A';                             Text = '常驻显示:' },
    [pscustomobject]@{ Hex = '7248 672C 003A';                                       Text = '版本:' }
)

# 用户可见文案里不许出现的英文关键词(大小写不敏感,只在**字符串字面量**里查)。
$script:ForbiddenEn = @(
    'Show tooltip', 'Press to', 'Click here', 'Right-click', 'Left-click', 'Double-click',
    'Hover to', 'Switch to next', 'User phrases', 'Shortcut settings', 'About / Account',
    'Current input:', 'Always show:', 'Version:', 'Current:', 'Always show'
)

# 显示路径不许出现的阻塞 I/O / 等待 API。
$script:ForbiddenIo = @(
    'CreateFile', 'WaitForSingleObject', 'BeginWaitForSingleObject',
    'Sleep(', 'RegOpenKey', 'RegQueryValue'
)

# 唯一允许出现在「显示路径」里的写盘点:显式 env 门控的诊断 dump
# (QuickPanelDialog.cpp 里的 FLUXING_QP_DIAG_DUMP 块,v0.19.0.10 起就有)。
# 该块不在正常显示路径上跑(要显式设环境变量),所以不算 P2 违规 —— 但它**必须**是
# env 门控的:一旦门控被摘掉,写盘就落回显示路径,本检查会立刻翻红。
$script:DiagGateMarker = 'GetEnvironmentVariableW(L"FLUXING_QP_DIAG_DUMP"'

. (Join-Path $PSScriptRoot 'lib\GateChecks.ps1')

$script:Rows = @()

function Add-CheckRow {
    param([string]$Id, [string]$Status, [string]$Detail)
    $script:Rows += [pscustomobject]@{ Id = $Id; Status = $Status; Detail = $Detail }
    $color = 'Red'
    if ($Status -eq 'PASS') { $color = 'Green' }
    elseif ($Status -eq 'SKIP') { $color = 'Yellow' }
    Write-Host ('[{0,-4}] {1,-26} {2}' -f $Status, $Id, $Detail) -ForegroundColor $color
}

function Add-SkipRow {
    param([string]$Id, [string]$Detail)
    Add-CheckRow -Id $Id -Status 'SKIP' -Detail $Detail
}

# 统一收口:检查自身抛异常不能把整层打挂,也不能被当成「没事」。
function Invoke-Check {
    param([string]$Id, [scriptblock]$Body)
    $r = $null
    try {
        $r = & $Body
    }
    catch {
        Add-CheckRow -Id $Id -Status 'FAIL' -Detail ('检查自身异常:' + $_.Exception.Message)
        return
    }
    if ($null -eq $r) {
        Add-CheckRow -Id $Id -Status 'FAIL' -Detail '检查没有返回结果($null)'
        return
    }
    if ($r.Ok) { Add-CheckRow -Id $Id -Status 'PASS' -Detail $r.Detail }
    else { Add-CheckRow -Id $Id -Status 'FAIL' -Detail $r.Detail }
}

function Read-SourceText {
    param([string]$Rel)
    $p = Join-Path $script:RepoRoot $Rel
    if (-not (Test-Path -LiteralPath $p)) { return $null }
    return [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)
}

function ConvertFrom-CodePoints {
    param([string]$Hex)
    $sb = New-Object System.Text.StringBuilder
    foreach ($tok in ($Hex -split '\s+')) {
        if (-not $tok) { continue }
        [void]$sb.Append([char][Convert]::ToInt32($tok, 16))
    }
    return $sb.ToString()
}

# 扫一遍 C++ 源码,一次拿到两样东西:
#   Code     —— 注释与字符串内容都被抹成空格(长度不变、行号不变)的纯代码
#   Literals —— 全部字符串字面量的内容(宽/窄分别标记)
# 为什么要抹注释:「MulDiv(」之类的判据不能被注释里的说明文字命中。
# 为什么要收集字面量:「用户可见文案」只存在于字面量里,注释里的英文说明不算违规。
function Get-CSourceScan {
    param([string]$Text)

    $sb = New-Object System.Text.StringBuilder
    $lits = New-Object System.Collections.ArrayList
    $i = 0
    $n = $Text.Length
    while ($i -lt $n) {
        $c = $Text[$i]

        # 行注释
        if ($c -eq '/' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '/') {
            while ($i -lt $n -and $Text[$i] -ne "`n") { [void]$sb.Append(' '); $i++ }
            continue
        }
        # 块注释
        if ($c -eq '/' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '*') {
            [void]$sb.Append('  '); $i += 2
            while ($i -lt $n -and -not ($Text[$i] -eq '*' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '/')) {
                if ($Text[$i] -eq "`n") { [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }
                $i++
            }
            if ($i -lt $n) { [void]$sb.Append('  '); $i += 2 }
            continue
        }
        # 字符串字面量("..." / L"...")
        if ($c -eq '"') {
            $isWide = ($i -gt 0 -and ($Text[$i - 1] -eq 'L' -or $Text[$i - 1] -eq 'u'))
            [void]$sb.Append($c); $i++
            $content = New-Object System.Text.StringBuilder
            while ($i -lt $n) {
                $ch = $Text[$i]
                if ($ch -eq '\') {
                    [void]$sb.Append(' '); [void]$content.Append($ch); $i++
                    if ($i -lt $n) { [void]$sb.Append(' '); [void]$content.Append($Text[$i]); $i++ }
                    continue
                }
                if ($ch -eq '"') { [void]$sb.Append($c); $i++; break }
                if ($ch -eq "`n") { [void]$sb.Append("`n") } else { [void]$sb.Append(' ') }
                [void]$content.Append($ch)
                $i++
            }
            [void]$lits.Add([pscustomobject]@{ Text = $content.ToString(); Wide = $isWide })
            continue
        }
        # 字符字面量('x' / '\0' / '\\'):只认这两种形状,免得把普通撇号当成字面量起点。
        if ($c -eq "'") {
            $isChar = $false
            if (($i + 2) -lt $n -and $Text[$i + 2] -eq "'") { $isChar = $true }
            elseif (($i + 3) -lt $n -and $Text[$i + 1] -eq '\' -and $Text[$i + 3] -eq "'") { $isChar = $true }
            if ($isChar) {
                [void]$sb.Append($c); $i++
                while ($i -lt $n) {
                    $ch = $Text[$i]
                    if ($ch -eq '\') {
                        [void]$sb.Append(' '); $i++
                        if ($i -lt $n) { [void]$sb.Append(' '); $i++ }
                        continue
                    }
                    if ($ch -eq "'") { [void]$sb.Append($c); $i++; break }
                    if ($ch -eq "`n") { [void]$sb.Append("`n"); break }
                    [void]$sb.Append(' '); $i++
                }
                continue
            }
        }

        [void]$sb.Append($c); $i++
    }

    return [pscustomobject]@{ Code = $sb.ToString(); Literals = @($lits) }
}

# 找出 env 门控诊断块的文本跨度(用于把它从「显示路径」里排除掉)。
# 用 Code(已抹字符串/注释)做花括号配对 ⇒ 括号不会被字符串或注释里的花括号带偏。
function Get-GatedSpan {
    param([string]$Text, [string]$Code, [string]$Marker)

    $m = [regex]::Match($Text, [regex]::Escape($Marker))
    if (-not $m.Success) { return $null }
    $open = $Code.IndexOf('{', $m.Index)
    if ($open -lt 0) { return $null }
    $depth = 0
    for ($j = $open; $j -lt $Code.Length; $j++) {
        if ($Code[$j] -eq '{') { $depth++ }
        elseif ($Code[$j] -eq '}') {
            $depth--
            if ($depth -eq 0) { return [pscustomobject]@{ Start = $m.Index; End = $j } }
        }
    }
    return $null
}

function Get-LineOf {
    param([string]$Code, [int]$Index)
    return 1 + ([regex]::Matches($Code.Substring(0, $Index), "`n")).Count
}

Write-Host ''
Write-Host ('L1 quickpanel gate  root={0}' -f $script:RepoRoot) -ForegroundColor Cyan
Write-Host ''

# 一次性把被检查的源码读进来(缺文件 ⇒ 对应检查 FAIL,不在这里炸)。
$panelText = Read-SourceText $script:RelPanelCpp
$appText   = Read-SourceText $script:RelAppCpp
$panelScan = $null
if ($null -ne $panelText) { $panelScan = Get-CSourceScan -Text $panelText }
$appScan = $null
if ($null -ne $appText) { $appScan = Get-CSourceScan -Text $appText }

# ---------------------------------------------------------------------------
# A 四个新 header 存在且非空
# ---------------------------------------------------------------------------
Invoke-Check 'A1 panel-headers-exist' {
    $problems = @()
    $sizes = @()
    foreach ($rel in $script:NewHeaders) {
        $p = Join-Path $script:RepoRoot $rel
        if (-not (Test-Path -LiteralPath $p)) { $problems += ('缺文件:' + $rel); continue }
        $len = (Get-Item -LiteralPath $p).Length
        if ($len -le 0) { $problems += ('文件为空:' + $rel); continue }
        $sizes += ('{0}({1}B)' -f (Split-Path -Leaf $rel), $len)
    }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true ('4 个 header 均存在且非空:' + ($sizes -join ' '))
}

# ---------------------------------------------------------------------------
# B DPI 单一真相源(constitution R7)
# ---------------------------------------------------------------------------
Invoke-Check 'B1 dpi-header-api' {
    $text = Read-SourceText $script:RelDpi
    if ($null -eq $text) { return New-CheckResult $false ('缺文件:' + $script:RelDpi) }
    $need = @('namespace panel_dpi', 'ScaleLogicalByDpi', 'PanelPhysicalSize')
    $missing = @($need | Where-Object { -not $text.Contains($_) })
    if ($missing.Count -gt 0) {
        return New-CheckResult $false ('FluxingPanelDpi.h 缺:' + ($missing -join ' '))
    }
    return New-CheckResult $true 'FluxingPanelDpi.h 含 ScaleLogicalByDpi / PanelPhysicalSize(panel_dpi 命名空间内)'
}

Invoke-Check 'B2 dpi-single-source' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $problems = @()

    # 必须真的调到共享实现。
    if (-not $code.Contains('fluxing::panel_dpi::ScaleLogicalByDpi')) {
        $problems += 'QuickPanelDialog.cpp 没有调用 fluxing::panel_dpi::ScaleLogicalByDpi(缩放真相源没被用上)'
    }

    # 不得自己抄一份缩放公式。共享口径是 (logical*dpi + 48)/96,
    # 抄一份必然露出 MulDiv( / 96 / + 48 之一。
    $formulaMarks = @('MulDiv(', '/ 96', '/96', '+ 48')
    foreach ($mark in $formulaMarks) {
        foreach ($m in [regex]::Matches($code, [regex]::Escape($mark))) {
            $problems += ('本地缩放公式嫌疑 {0} @line {1}' -f $mark, (Get-LineOf -Code $code -Index $m.Index))
        }
    }

    if ($problems.Count -gt 0) { return New-CheckResult $false (($problems | Select-Object -First 5) -join ';') }
    return New-CheckResult $true '缩放只走 fluxing::panel_dpi::ScaleLogicalByDpi,无本地抄写的公式'
}

# ---------------------------------------------------------------------------
# C 状态回显接线
# ---------------------------------------------------------------------------
Invoke-Check 'C1 state-echo-refresh' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $defs = ([regex]::Matches($code, 'void\s+QuickPanelDialog::RefreshInputState\s*\(')).Count
    $calls = ([regex]::Matches($code, 'RefreshInputState\s*\(')).Count - $defs
    $problems = @()
    if ($defs -lt 1) { $problems += '找不到 RefreshInputState() 的定义' }
    # 至少两处调用:provider 注入时拉一次 + 轮询 timer 里拉一次。
    # 少一处 ⇒ 状态回显会停在注入那一瞬的快照(过期)。
    if ($calls -lt 2) { $problems += ('RefreshInputState() 调用点只有 {0} 处(应 >= 2:provider 注入 + 轮询 timer)' -f $calls) }
    if (-not $code.Contains('WM_TIMER')) { $problems += '找不到 WM_TIMER 轮询(状态回显没有刷新节拍)' }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true ('RefreshInputState() 定义 {0} 处 / 调用 {1} 处 + WM_TIMER 轮询就位' -f $defs, $calls)
}

Invoke-Check 'C2 state-provider-wiring' {
    if ($null -eq $appScan) { return New-CheckResult $false ('缺文件:' + $script:RelAppCpp) }
    if (-not $appScan.Code.Contains('QuickPanelDialog::SetStateProvider(')) {
        return New-CheckResult $false 'WeaselServerApp.cpp 没有 QuickPanelDialog::SetStateProvider( —— 面板拿不到状态快照'
    }
    return New-CheckResult $true 'WeaselServerApp.cpp 注入 QuickPanelDialog::SetStateProvider('
}

Invoke-Check 'C3 startup-prewarm' {
    if ($null -eq $appScan) { return New-CheckResult $false ('缺文件:' + $script:RelAppCpp) }
    $code = $appScan.Code
    $problems = @()
    $initIdx = $code.IndexOf('m_handler->Initialize()')
    if ($initIdx -lt 0) { $problems += '找不到 m_handler->Initialize()' }
    foreach ($call in @('m_handler->IsFullShape()', 'm_handler->IsSimplification()')) {
        $idx = $code.IndexOf($call)
        if ($idx -lt 0) { $problems += ('缺启动期预热 ' + $call); continue }
        if ($initIdx -ge 0 -and $idx -lt $initIdx) {
            $problems += ('启动期预热 ' + $call + ' 出现在 Initialize() 之前(读不到已缓存的形态)')
        }
    }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true 'Initialize() 之后有 IsFullShape() / IsSimplification() 启动期预热(显示路径不触发 user_config_open)'
}

# ---------------------------------------------------------------------------
# D 键盘可达性接线
# ---------------------------------------------------------------------------
Invoke-Check 'D1 keyboard-handler' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $problems = @()
    if (-not $code.Contains('fluxing::panel_keyboard::HandleKey(')) {
        $problems += '没有走 fluxing::panel_keyboard::HandleKey( —— 键位裁决逻辑被复制进生产代码了'
    }
    if (-not $code.Contains('fluxing::panel_keyboard::State')) {
        $problems += '没有 fluxing::panel_keyboard::State(焦点状态没走共享状态机)'
    }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true '键位裁决走 fluxing::panel_keyboard::HandleKey( + State)'
}

Invoke-Check 'D2 keyboard-messages' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $problems = @()
    if (-not $code.Contains('WM_CHAR')) { $problems += '没有 WM_CHAR 处理(按键会漏进编辑框)' }
    if (-not $code.Contains('WM_SETFOCUS')) { $problems += '没有 WM_SETFOCUS 处理(面板拿不到键盘交互态)' }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true 'WM_CHAR 吞键 + WM_SETFOCUS 进键盘交互态'
}

Invoke-Check 'D3 focus-lifecycle' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $problems = @()
    foreach ($fn in @('CaptureKeyboardFocus(', 'RestoreKeyboardFocus(', 'ResetKeyboardState(')) {
        if (-not $code.Contains($fn)) { $problems += ('缺 ' + $fn) }
    }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true 'CaptureKeyboardFocus / RestoreKeyboardFocus / ResetKeyboardState 三件套就位'
}

# ---------------------------------------------------------------------------
# E tooltip 接线
# ---------------------------------------------------------------------------
Invoke-Check 'E1 tooltip-wiring' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    $code = $panelScan.Code
    $problems = @()
    if (-not $code.Contains('ShowTooltip(')) { $problems += '缺 ShowTooltip(' }
    if (-not $code.Contains('HideTooltip(')) { $problems += '缺 HideTooltip(' }
    if (-not $code.Contains('DestroyWindow(s_hwndTooltip)')) { $problems += '缺 DestroyWindow(s_hwndTooltip)(tooltip 顶层窗会泄漏)' }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true 'ShowTooltip / HideTooltip / DestroyWindow(s_hwndTooltip) 就位'
}

Invoke-Check 'E2 tooltip-include' {
    if ($null -eq $panelScan) { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    if (-not [regex]::IsMatch($panelScan.Code, '#include\s*[<"]FluxingPanelTooltip\.h[>"]')) {
        return New-CheckResult $false 'QuickPanelDialog.cpp 没有 include FluxingPanelTooltip.h'
    }
    return New-CheckResult $true 'QuickPanelDialog.cpp include 了 FluxingPanelTooltip.h'
}

# ---------------------------------------------------------------------------
# F 显示路径零 I/O(constitution P2)
# ---------------------------------------------------------------------------
Invoke-Check 'F1 display-path-zero-io' {
    $targets = @()
    if ($null -ne $panelScan) { $targets += [pscustomobject]@{ Rel = $script:RelPanelCpp; Scan = $panelScan; Text = $panelText } }
    else { return New-CheckResult $false ('缺文件:' + $script:RelPanelCpp) }
    foreach ($rel in $script:NewHeaders) {
        $t = Read-SourceText $rel
        if ($null -ne $t) { $targets += [pscustomobject]@{ Rel = $rel; Scan = (Get-CSourceScan -Text $t); Text = $t } }
    }

    $problems = @()
    $excluded = 0
    foreach ($target in $targets) {
        $code = $target.Scan.Code
        $gated = Get-GatedSpan -Text $target.Text -Code $code -Marker $script:DiagGateMarker
        $seen = New-Object 'System.Collections.Generic.HashSet[int]'
        foreach ($api in $script:ForbiddenIo) {
            foreach ($m in [regex]::Matches($code, [regex]::Escape($api))) {
                if ($seen.Contains($m.Index)) { continue }
                [void]$seen.Add($m.Index)
                if ($null -ne $gated -and $m.Index -ge $gated.Start -and $m.Index -le $gated.End) {
                    $excluded++
                    continue
                }
                $problems += ('{0}: {1} @line {2}' -f $target.Rel, $api, (Get-LineOf -Code $code -Index $m.Index))
            }
        }
    }
    if ($problems.Count -gt 0) { return New-CheckResult $false (($problems | Select-Object -First 8) -join ';') }
    $note = ''
    if ($excluded -gt 0) {
        $note = ('(已排除 env 门控诊断 dump 内的 {0} 处)' -f $excluded)
    }
    return New-CheckResult $true ('显示路径无阻塞 I/O / 等待 / 同步注册表 ' + $note)
}

# ---------------------------------------------------------------------------
# G 用户可见文案简体中文(constitution P5)
# ---------------------------------------------------------------------------
if ($SkipText) {
    Add-SkipRow 'G1 zh-required-strings' '按 -SkipText 跳过'
    Add-SkipRow 'G2 zh-no-english-visible' '按 -SkipText 跳过'
}
else {

    Invoke-Check 'G1 zh-required-strings' {
        $blob = New-Object System.Text.StringBuilder
        foreach ($rel in ($script:NewHeaders + @($script:RelPanelCpp))) {
            $t = Read-SourceText $rel
            if ($null -ne $t) { [void]$blob.Append($t) }
        }
        $all = $blob.ToString()
        $missing = @()
        foreach ($item in $script:RequiredZh) {
            $want = ConvertFrom-CodePoints -Hex $item.Hex
            if (-not $all.Contains($want)) { $missing += $item.Text }
        }
        if ($missing.Count -gt 0) {
            return New-CheckResult $false ('缺中文文案:' + ($missing -join ' / '))
        }
        return New-CheckResult $true ('{0} 条中文文案全部就位' -f $script:RequiredZh.Count)
    }

    Invoke-Check 'G2 zh-no-english-visible' {
        $problems = @()
        $scanned = 0

        # 四个 header 的宽字符串字面量 = 用户可见文案:不许出现英文单词。
        foreach ($rel in $script:NewHeaders) {
            $t = Read-SourceText $rel
            if ($null -eq $t) { continue }
            $scan = Get-CSourceScan -Text $t
            foreach ($lit in $scan.Literals) {
                if (-not $lit.Wide) { continue }
                $scanned++
                if ($lit.Text -match '[A-Za-z]{2,}') {
                    $problems += ('{0}: 宽字面量含英文单词 "{1}"' -f $rel, $lit.Text)
                }
            }
        }

        # 所有字面量(窄的也算)都不许出现禁用的英文文案关键词。
        $scanAll = @()
        foreach ($rel in ($script:NewHeaders + @($script:RelPanelCpp))) {
            $t = Read-SourceText $rel
            if ($null -eq $t) { continue }
            $scanAll += (Get-CSourceScan -Text $t).Literals
        }
        foreach ($lit in $scanAll) {
            foreach ($bad in $script:ForbiddenEn) {
                if ($lit.Text -like ('*' + $bad + '*')) {
                    $problems += ('英文用户可见文案 "{0}"(在 "{1}" 里)' -f $bad, $lit.Text)
                }
            }
        }

        if ($problems.Count -gt 0) { return New-CheckResult $false (($problems | Select-Object -First 6) -join ';') }
        return New-CheckResult $true ('{0} 个宽字面量全中文,禁用的英文文案 0 处' -f $scanned)
    }

}

# ---------------------------------------------------------------------------
# H IPC 边界未被破坏(spec 099 P0-4/P0-5 不许动 include/WeaselIPCData.h)
# ---------------------------------------------------------------------------
Invoke-Check 'H1 ipc-boundary-intact' {
    $problems = @()

    # 本轮新增的纯逻辑 header 不得引入 IPC 头 —— 它们是纯逻辑,不该知道 IPC 长什么样。
    foreach ($rel in $script:NewHeaders) {
        $t = Read-SourceText $rel
        if ($null -eq $t) { continue }
        if ([regex]::IsMatch($t, '#include\s*[<"]WeaselIPCData\.h[>"]')) {
            $problems += ($rel + ' 引入了 WeaselIPCData.h')
        }
        if ($t.Contains('WeaselIPCData')) {
            $problems += ($rel + ' 提到了 WeaselIPCData')
        }
    }

    # 反向:IPC 头不得反向依赖本轮新增的 header。
    $ipc = Read-SourceText $script:RelIpcData
    if ($null -ne $ipc) {
        foreach ($rel in $script:NewHeaders) {
            $leaf = Split-Path -Leaf $rel
            if ($ipc.Contains($leaf)) { $problems += ('WeaselIPCData.h 反向依赖 ' + $leaf) }
        }
    }

    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join ';') }
    return New-CheckResult $true 'FluxingPanel*.h 与 WeaselIPCData.h 互不依赖(IPC 边界未被本轮破坏)'
}

# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
$total = $script:Rows.Count
$pass = @($script:Rows | Where-Object { $_.Status -eq 'PASS' }).Count
$fail = @($script:Rows | Where-Object { $_.Status -eq 'FAIL' }).Count
$skip = @($script:Rows | Where-Object { $_.Status -eq 'SKIP' }).Count

Write-Host ''
foreach ($row in @($script:Rows | Where-Object { $_.Status -eq 'FAIL' })) {
    Write-Host ('FAIL {0}: {1}' -f $row.Id, $row.Detail) -ForegroundColor Red
}
Write-Host ('L1 RESULT checks={0} pass={1} fail={2} skip={3}' -f $total, $pass, $fail, $skip)

if ($fail -gt 0) { exit 1 }
if (($total - $skip) -le 0) {
    Write-Host 'L1 没有任何检查真正跑起来 —— 空跑不算通过。' -ForegroundColor Red
    exit 1
}
exit 0
