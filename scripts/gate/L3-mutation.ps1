#Requires -Version 5.1
<#
.SYNOPSIS
L3 变异自检：故意把被守护的资产改坏，证明门禁检查真的会翻红。

.DESCRIPTION
门禁最大的风险不是「漏检」，而是「检查恒绿」——看起来在跑，其实永远返回通过。
本脚本对每个受守护的资产造一个变异体，要求：

  * 控制组（未变异的副本）必须绿  —— 否则无法区分「检查有齿」和「检查坏掉」
  * 变异组（改坏的副本）必须红    —— 否则该检查是假牙

判定：控制组不绿 ⇒ INVALID；变异组仍绿或返回 $null ⇒ SURVIVED；变异组被判红 ⇒ KILLED。
survived > 0 或 invalid > 0 时退出码 1（门禁失败）。

.PARAMETER Root
仓库根目录。默认由 $PSScriptRoot\..\.. 推导。

.PARAMETER WorkDir
变异体工作目录。每轮使用 run-<时间戳> 子目录，天然隔离，不删除工作目录本身。

.PARAMETER MakensisPath
makensis.exe 路径。留空则自动解析。仅 M06 需要。

.PARAMETER SkipCompile
跳过 M06（需要调用 makensis 编译 NSIS）。CI 上建议跳过。

.EXAMPLE
pwsh -NoProfile -File scripts\gate\L3-mutation.ps1
#>
[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$WorkDir = '',
    [string]$MakensisPath = '',
    [switch]$SkipCompile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    param([string]$Hint)
    if ($Hint) { return (Resolve-Path -LiteralPath $Hint).Path }
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$script:RepoRoot = Resolve-RepoRoot -Hint $Root
$Root = $script:RepoRoot

if (-not $WorkDir) { $WorkDir = Join-Path $env:TEMP 'fluxing-l3' }
$runDir = Join-Path $WorkDir ('run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

. (Join-Path $PSScriptRoot 'lib\GateChecks.ps1')

$script:Rows = @()

function Add-Row {
    param([string]$Id, [string]$Outcome, [string]$Detail)
    $script:Rows += [pscustomobject]@{ Id = $Id; Outcome = $Outcome; Detail = $Detail }
}

function New-CaseDir {
    param([string]$Name)
    $d = Join-Path $runDir $Name
    if (-not (Test-Path -LiteralPath $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
    return $d
}

function Copy-Into {
    param([string]$Source, [string]$DestDir)
    if (-not (Test-Path -LiteralPath $Source)) { return $null }
    if (-not (Test-Path -LiteralPath $DestDir)) { New-Item -ItemType Directory -Path $DestDir -Force | Out-Null }
    $dest = Join-Path $DestDir (Split-Path -Leaf $Source)
    [System.IO.File]::WriteAllBytes($dest, [System.IO.File]::ReadAllBytes($Source))
    return $dest
}

function Write-TextFile {
    param([string]$Path, [string]$Text, [bool]$Bom)
    $enc = New-Object System.Text.UTF8Encoding($Bom)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

function Test-Pair {
    param([string]$Id, [scriptblock]$OnControl, [scriptblock]$OnMutant)

    $ctl = $null
    try { $ctl = & $OnControl }
    catch { Add-Row $Id 'INVALID' ('控制组异常 ⇒ 变异结论不可信：' + $_.Exception.Message); return }
    if ($null -eq $ctl) { Add-Row $Id 'INVALID' '控制组返回 $null（检查无法评估）'; return }
    if ($ctl.Ok -ne $true) { Add-Row $Id 'INVALID' ('控制组未通过 ⇒ 变异结论不可信：' + $ctl.Detail); return }

    $mut = $null
    try { $mut = & $OnMutant }
    catch { Add-Row $Id 'INVALID' ('变异组异常（非「检出」，而是检查崩了）：' + $_.Exception.Message); return }
    if ($null -eq $mut) { Add-Row $Id 'SURVIVED' '变异组返回 $null ⇒ 无法证明检出'; return }
    if ($mut.Ok -eq $true) { Add-Row $Id 'SURVIVED' ('变异后检查仍然绿：' + $mut.Detail); return }

    Add-Row $Id 'KILLED' ('变异后被判红：' + $mut.Detail)
}

$ownUi = $null
$nsiPath = Join-Path $Root 'output\install.nsi'

# ---------------------------------------------------------------------------
# M01 / M02：安装器的「按映像名杀自家 UI 进程」裸块（L174 的齿）
# ---------------------------------------------------------------------------
if (Test-Path -LiteralPath (Join-Path $Root 'include\WeaselIPC.h')) {
    $ownUi = Get-OwnUiExeNameFromHeader -Path (Join-Path $Root 'include\WeaselIPC.h')
}

if (-not $ownUi) {
    Add-Row 'M01' 'INVALID' '读不到 include\WeaselIPC.h 的 OwnUiClientExeName()'
    Add-Row 'M02' 'INVALID' '同上'
}
elseif (-not (Test-Path -LiteralPath $nsiPath)) {
    Add-Row 'M01' 'INVALID' "缺 $nsiPath"
    Add-Row 'M02' 'INVALID' "缺 $nsiPath"
}
else {
    $nsiText = [System.IO.File]::ReadAllText($nsiPath)
    $needle = "taskkill /F /IM $ownUi'"
    $idx = $nsiText.IndexOf($needle)

    if ($idx -lt 0) {
        Add-Row 'M01' 'INVALID' ('找不到裸杀块，无法造变异：' + $needle)
        Add-Row 'M02' 'INVALID' '同上'
    }
    else {
        $m01Dir = New-CaseDir 'M01'
        $m01Ctl = Join-Path $m01Dir 'control-install.nsi'
        $m01Mut = Join-Path $m01Dir 'mutant-install.nsi'
        Write-TextFile -Path $m01Ctl -Text $nsiText -Bom $true
        # 删掉一处裸杀块（保留注释），裸块数 4 -> 3
        $m01Text = $nsiText.Remove($idx, $needle.Length).Insert($idx, "rem l3-removed'")
        Write-TextFile -Path $m01Mut -Text $m01Text -Bom $true
        Test-Pair 'M01' {
            Test-InstallerOwnUiKillBlocks -NsiPath $m01Ctl -ExpectedExeName $ownUi
        } {
            Test-InstallerOwnUiKillBlocks -NsiPath $m01Mut -ExpectedExeName $ownUi
        }

        $m02Dir = New-CaseDir 'M02'
        $m02Ctl = Join-Path $m02Dir 'control-install.nsi'
        $m02Mut = Join-Path $m02Dir 'mutant-install.nsi'
        Write-TextFile -Path $m02Ctl -Text $nsiText -Bom $true
        # 给第一处裸块插 /T：孤儿不在任何活进程树里，/T 版本必须被判红
        $m02Text = $nsiText.Insert($idx + $needle.Length - 1, ' /T')
        Write-TextFile -Path $m02Mut -Text $m02Text -Bom $true
        Test-Pair 'M02' {
            Test-InstallerOwnUiKillBlocks -NsiPath $m02Ctl -ExpectedExeName $ownUi
        } {
            Test-InstallerOwnUiKillBlocks -NsiPath $m02Mut -ExpectedExeName $ownUi
        }
    }
}

# ---------------------------------------------------------------------------
# M03：version.txt 与 weasel.props / env.bat / CHANGELOG.md 的一致性
# ---------------------------------------------------------------------------
$m03Src = @('version.txt', 'weasel.props', 'env.bat', 'CHANGELOG.md')
$m03CtlRoot = New-CaseDir 'M03\control'
$m03MutRoot = New-CaseDir 'M03\mutant'
$m03Copied = 0
foreach ($f in $m03Src) {
    $srcFile = Join-Path $Root $f
    if (Test-Path -LiteralPath $srcFile) {
        Copy-Into $srcFile $m03CtlRoot | Out-Null
        Copy-Into $srcFile $m03MutRoot | Out-Null
        $m03Copied++
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $m03CtlRoot 'version.txt'))) {
    Add-Row 'M03' 'INVALID' '缺 version.txt，无法造变异'
}
else {
    Write-TextFile -Path (Join-Path $m03MutRoot 'version.txt') -Text '9.9.9' -Bom $false
    Test-Pair 'M03' {
        Test-VersionConsistency -Root $m03CtlRoot
    } {
        Test-VersionConsistency -Root $m03MutRoot
    }
    Add-Row 'M03-note' 'INFO' ("临时根拷入 {0} 个版本文件（缺失的 git-ignored 文件自动跳过）" -f $m03Copied)
}

# ---------------------------------------------------------------------------
# M04：DEFECTS.yaml 的 guard.case 指向不存在的用例（登记表 ⇄ 用例双向一致）
# ---------------------------------------------------------------------------
$m04CtlRoot = New-CaseDir 'M04\control'
$m04MutRoot = New-CaseDir 'M04\mutant'
$regRel = 'test\regression\DEFECTS.yaml'
$casesRel = 'test\regression\cases'
$regSrc = Join-Path $Root $regRel
$casesSrc = Join-Path $Root $casesRel

if (-not (Test-Path -LiteralPath $regSrc)) {
    Add-Row 'M04' 'INVALID' "缺 $regRel"
}
else {
    foreach ($pair in @(@($regSrc, $m04CtlRoot), @($regSrc, $m04MutRoot))) {
        $destDir = Join-Path $pair[1] (Split-Path -Parent $regRel)
        Copy-Into $pair[0] $destDir | Out-Null
    }
    if (Test-Path -LiteralPath $casesSrc) {
        foreach ($f in @(Get-ChildItem -LiteralPath $casesSrc -File -Filter 'C*.ps1' -ErrorAction SilentlyContinue)) {
            Copy-Into $f.FullName (Join-Path $m04CtlRoot $casesRel) | Out-Null
            Copy-Into $f.FullName (Join-Path $m04MutRoot $casesRel) | Out-Null
        }
    }

    $regText = [System.IO.File]::ReadAllText($regSrc)
    $regMatch = [regex]::Match($regText, '(?m)^(\s*)case:\s*(C\d{4})')
    if (-not $regMatch.Success) {
        Add-Row 'M04' 'INVALID' 'DEFECTS.yaml 里找不到 case: C#### 锚点'
    }
    else {
        $mutText = $regText.Remove($regMatch.Index, $regMatch.Length).Insert($regMatch.Index, ($regMatch.Groups[1].Value + 'case: C9999'))
        Write-TextFile -Path (Join-Path $m04MutRoot $regRel) -Text $mutText -Bom $false
        Test-Pair 'M04' {
            Test-DefectsRegistry -Root $m04CtlRoot
        } {
            Test-DefectsRegistry -Root $m04MutRoot
        }
    }
}

# ---------------------------------------------------------------------------
# M05a / M05b：文件形状（UTF-8 BOM / CRLF）
# ---------------------------------------------------------------------------
$shapeSrc = Join-Path $Root '.specify\memory\lessons-learned.md'
if (-not (Test-Path -LiteralPath $shapeSrc)) {
    Add-Row 'M05a' 'INVALID' '缺 .specify\memory\lessons-learned.md'
    Add-Row 'M05b' 'INVALID' '同上'
}
else {
    $m05Ctl = Copy-Into $shapeSrc (New-CaseDir 'M05')
    Test-Pair 'M05a' {
        Test-TextFileShape -Path $m05Ctl -ExpectBom $true
    } {
        $bytes = [System.IO.File]::ReadAllBytes($m05Ctl)
        $mut = Join-Path (Split-Path -Parent $m05Ctl) 'no-bom.md'
        [System.IO.File]::WriteAllBytes($mut, $bytes[3..($bytes.Length - 1)])
        Test-TextFileShape -Path $mut -ExpectBom $true
    }

    $m05Text = [System.IO.File]::ReadAllText($shapeSrc)
    $crlfAt = $m05Text.IndexOf("`r`n")
    if ($crlfAt -lt 0) {
        Add-Row 'M05b' 'INVALID' '文件里找不到 CRLF，无法造裸 LF 变异'
    }
    else {
        Test-Pair 'M05b' {
            Test-TextFileShape -Path $m05Ctl -ExpectBom $true
        } {
            $mutText = $m05Text.Remove($crlfAt, 2).Insert($crlfAt, "`n")
            $mut = Join-Path (Split-Path -Parent $m05Ctl) 'bare-lf.md'
            Write-TextFile -Path $mut -Text $mutText -Bom $true
            Test-TextFileShape -Path $mut -ExpectBom $true
        }
    }
}

# ---------------------------------------------------------------------------
# M06：NSIS 编译门禁（!error 注入必须让 makensis 失败）
# 变异体必须放在 output\ 内，否则 !include 的相对路径断裂，控制组也会红。
# ---------------------------------------------------------------------------
if ($SkipCompile) {
    Add-Row 'M06' 'SKIPPED' '按 -SkipCompile 跳过（需 makensis）'
}
elseif (-not (Test-Path -LiteralPath $nsiPath)) {
    Add-Row 'M06' 'SKIPPED' "缺 $nsiPath"
}
else {
    if (-not $MakensisPath) {
        foreach ($cand in @('C:\Program Files (x86)\NSIS\Bin\makensis.exe', 'C:\Program Files\NSIS\Bin\makensis.exe')) {
            if (Test-Path -LiteralPath $cand) { $MakensisPath = $cand; break }
        }
    }
    if (-not $MakensisPath -or -not (Test-Path -LiteralPath $MakensisPath)) {
        Add-Row 'M06' 'SKIPPED' '找不到 makensis.exe'
    }
    else {
        $ctlNsi = Join-Path $Root 'output\.l3-control.nsi'
        $mutNsi = Join-Path $Root 'output\.l3-mutant.nsi'
        $outCtl = Join-Path (New-CaseDir 'M06') 'control.exe'
        $outMut = Join-Path (New-CaseDir 'M06') 'mutant.exe'
        try {
            $text = [System.IO.File]::ReadAllText($nsiPath)
            Write-TextFile -Path $ctlNsi -Text $text -Bom $true
            Write-TextFile -Path $mutNsi -Text ('!error "L3 mutation"' + "`r`n" + $text) -Bom $true
            Test-Pair 'M06' {
                Test-NsiCompiles -NsiPath $ctlNsi -MakensisPath $MakensisPath -OutFile $outCtl
            } {
                Test-NsiCompiles -NsiPath $mutNsi -MakensisPath $MakensisPath -OutFile $outMut
            }
        }
        finally {
            foreach ($t in @($ctlNsi, $mutNsi)) {
                if (Test-Path -LiteralPath $t) { Remove-Item -LiteralPath $t -Force -ErrorAction SilentlyContinue }
            }
        }
    }
}

# ---------------------------------------------------------------------------
# M07 / M08：对话框「取消」按钮 ⇄ 消息映射（Bug #3 的齿）+ 模板文案中文化（P2 的齿）
# ---------------------------------------------------------------------------
function Copy-TreeFiles {
    param([string]$SourceDir, [string]$DestDir, [string[]]$Include)
    if (-not (Test-Path -LiteralPath $SourceDir)) { return }
    foreach ($inc in $Include) {
        foreach ($f in @(Get-ChildItem -LiteralPath $SourceDir -Recurse -File -Filter $inc -ErrorAction SilentlyContinue)) {
            $rel = $f.FullName.Substring($SourceDir.Length).TrimStart([char]'\', [char]'/')
            $dest = Join-Path $DestDir $rel
            $dd = Split-Path -Parent $dest
            if (-not (Test-Path -LiteralPath $dd)) { New-Item -ItemType Directory -Path $dd -Force | Out-Null }
            [System.IO.File]::WriteAllBytes($dest, [System.IO.File]::ReadAllBytes($f.FullName))
        }
    }
}

$m07CtlRoot = New-CaseDir 'M07\control'
$m07MutRoot = New-CaseDir 'M07\mutant'
foreach ($r in @($m07CtlRoot, $m07MutRoot)) {
    Copy-TreeFiles -SourceDir (Join-Path $Root 'WeaselDeployer') -DestDir (Join-Path $r 'WeaselDeployer') -Include @('*.h', '*.rc')
}
$m07Hdr = Join-Path $m07MutRoot 'WeaselDeployer\SogouImportDialog.h'
$m07CtlRc = Join-Path $m07CtlRoot 'WeaselDeployer\WeaselDeployer.rc'
$m07MutRc = Join-Path $m07MutRoot 'WeaselDeployer\WeaselDeployer.rc'
if (-not (Test-Path -LiteralPath $m07Hdr)) {
    Add-Row 'M07' 'INVALID' '拷不到 SogouImportDialog.h，无法造变异'
}
elseif (-not (Test-Path -LiteralPath $m07CtlRc)) {
    Add-Row 'M07' 'INVALID' '拷不到 WeaselDeployer.rc，无法造变异'
}
else {
    # 变异：把 IDCANCEL handler 从消息映射里删掉 ⇒ 必须被判红
    $t = [System.IO.File]::ReadAllText($m07Hdr)
    $kept = @($t -split "`r?`n" | Where-Object { $_ -notmatch 'COMMAND_ID_HANDLER\(\s*IDCANCEL' })
    $mut = $kept -join "`r`n"
    if ($mut -eq $t) {
        Add-Row 'M07' 'INVALID' '头文件里找不到 IDCANCEL handler 行，无法造变异'
    }
    else {
        [System.IO.File]::WriteAllText($m07Hdr, $mut, (New-Object System.Text.UTF8Encoding($false)))
        Test-Pair 'M07' {
            Test-DialogCancelHandlerCoverage -DeployerDir (Join-Path $m07CtlRoot 'WeaselDeployer') -RcPath $m07CtlRc
        } {
            Test-DialogCancelHandlerCoverage -DeployerDir (Join-Path $m07MutRoot 'WeaselDeployer') -RcPath $m07MutRc
        }
    }
}

$m08CtlRc = $m07CtlRc
$m08MutRc = Join-Path $m07MutRoot 'WeaselDeployer\WeaselDeployer.rc'
if (-not (Test-Path -LiteralPath $m08MutRc)) {
    Add-Row 'M08' 'INVALID' '缺 rc 副本，无法造变异'
}
else {
    # 变异：把英文语言块的 CAPTION 改回英文 ⇒ 中文化检查必须被判红
    $bytes = [System.IO.File]::ReadAllBytes($m08MutRc)
    $txt = [System.Text.Encoding]::Unicode.GetString($bytes, 2, $bytes.Length - 2)
    $needle = 'CAPTION "【火流猩输入法】导入搜狗用户词库"'
    $idx = $txt.LastIndexOf($needle)
    if ($idx -lt 0) {
        Add-Row 'M08' 'INVALID' 'rc 里找不到 IDD_SOGOU_IMPORT 的中文 CAPTION，无法造变异'
    }
    else {
        $newTxt = $txt.Remove($idx, $needle.Length).Insert($idx, 'CAPTION "【Fluxing】Import Sogou User Dictionary"')
        $body = [System.Text.Encoding]::Unicode.GetBytes($newTxt)
        $out = New-Object byte[] (2 + $body.Length)
        $out[0] = 0xFF
        $out[1] = 0xFE
        [Array]::Copy($body, 0, $out, 2, $body.Length)
        [System.IO.File]::WriteAllBytes($m08MutRc, $out)
        Test-Pair 'M08' {
            Test-DialogTemplateChinese -RcPath $m08CtlRc -DialogId 'IDD_SOGOU_IMPORT'
        } {
            Test-DialogTemplateChinese -RcPath $m08MutRc -DialogId 'IDD_SOGOU_IMPORT'
        }
    }
}

# ---------------------------------------------------------------------------
# 汇总
# ---------------------------------------------------------------------------
foreach ($r in $script:Rows) {
    Write-Host ("MUTATION {0,-9} {1,-9} {2}" -f $r.Id, $r.Outcome, $r.Detail)
}

$counted = @($script:Rows | Where-Object { $_.Outcome -ne 'INFO' })
$killed = @($counted | Where-Object { $_.Outcome -eq 'KILLED' }).Count
$survived = @($counted | Where-Object { $_.Outcome -eq 'SURVIVED' }).Count
$invalid = @($counted | Where-Object { $_.Outcome -eq 'INVALID' }).Count
$skipped = @($counted | Where-Object { $_.Outcome -eq 'SKIPPED' }).Count

Write-Host ("L3 RESULT mutations={0} killed={1} survived={2} invalid={3} skipped={4} workdir={5}" -f `
        $counted.Count, $killed, $survived, $invalid, $skipped, $runDir)

if ($survived -gt 0 -or $invalid -gt 0) { exit 1 }
if (($counted.Count - $skipped) -le 0) {
    Write-Host 'L3 RESULT 全部变异被跳过 ⇒ 不算通过'
    exit 1
}
exit 0
