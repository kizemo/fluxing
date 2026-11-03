#Requires -Version 5.1
<#
.SYNOPSIS
    L0 静态门禁：不需要重建、不需要装机就能跑起来的「齿」。

.DESCRIPTION
    按 Mode 分层，避免把一个门禁写成「要么全跑要么全不跑」：

      source    —— version.txt / CHANGELOG.md / lessons-learned.md / install.nsi 的文件形状，
                   rc 文件必须是 UTF-16LE + BOM，安装器必须按映像名清理自家 UI 进程，
                   版本号五处一致，脚本语法 + .cmd 纯 ASCII，缺陷登记表双向引用。
      artifact  —— output\Win32 五个产物的 PE 版本资源（字符串表 FileVersion 必须非空且与 version.txt 一致）。
      all       —— 两者都跑。

    另有两个可选重项，默认跑、可用开关关掉：
      -SkipNsisCompile     跳过 NSIS 编译冒烟（CI 上没有 NSIS 时用）
      -SkipGuardUnitTest   跳过安装路径守卫单测（需要 makensis）

    退出码：0 = 没有 FAIL（SKIP 不算失败）；1 = 有 FAIL，或一个检查都没真正跑到
    （防止「空跑变绿」—— 检查全被跳过时不能算通过）。

.NOTES
    对应 ADR 0033 / 复盘 RC1・RC7 / lessons-learned L174・L175。
    所有实现都在 lib\GateChecks.ps1 里做成「接受显式路径的纯函数」，
    因为 L3 变异自检要把它们指向临时副本 —— 检查一旦写死仓库路径就不可变异，
    不可变异就等于无法证明它还有牙。
#>
[CmdletBinding()]
param(
    [ValidateSet('source', 'artifact', 'all')]
    [string]$Mode = 'all',

    [string]$Root = '',
    [string]$WorkDir = '',
    [string]$MakensisPath = '',
    [string]$ArtifactDir = '',

    [switch]$SkipNsisCompile,
    [switch]$SkipGuardUnitTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:RepoRoot = if ($Root) { (Resolve-Path -LiteralPath $Root).Path } else { (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path }
if (-not $WorkDir) { $WorkDir = Join-Path ([System.IO.Path]::GetTempPath()) 'fluxing-l0' }
if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null }
if (-not $ArtifactDir) { $ArtifactDir = Join-Path $script:RepoRoot 'output\Win32' }

. (Join-Path $PSScriptRoot 'lib\GateChecks.ps1')

# 安装产物清单：与 AGENTS.md §4.3 的版本核验命令、C0175 用例保持同一批文件。
$script:ProductFiles = @('WeaselServer.exe', 'WeaselDeployer.exe', 'WeaselSetup.exe', 'weasel.dll', 'weaselx64.dll')

$script:Rows = New-Object System.Collections.ArrayList

function Add-CheckRow {
    param([string]$Id, [string]$Status, [string]$Detail)
    [void]$script:Rows.Add([pscustomobject]@{ Id = $Id; Status = $Status; Detail = $Detail })
    $color = 'Red'
    if ($Status -eq 'PASS') { $color = 'Green' }
    elseif ($Status -eq 'SKIP') { $color = 'Yellow' }
    Write-Host ('[{0,-4}] {1,-26} {2}' -f $Status, $Id, $Detail) -ForegroundColor $color
}

function Add-SkipRow {
    param([string]$Id, [string]$Detail)
    Add-CheckRow -Id $Id -Status 'SKIP' -Detail $Detail
}

# 统一收口：检查自身抛异常不能把整层打挂，也不能被当成「没事」。
function Invoke-Check {
    param([string]$Id, [scriptblock]$Body)
    $r = $null
    try {
        $r = & $Body
    }
    catch {
        Add-CheckRow -Id $Id -Status 'FAIL' -Detail ('检查自身异常：' + $_.Exception.Message)
        return
    }
    if ($null -eq $r) {
        Add-CheckRow -Id $Id -Status 'FAIL' -Detail '检查没有返回结果（$null）'
        return
    }
    if ($r.Ok) { Add-CheckRow -Id $Id -Status 'PASS' -Detail $r.Detail }
    else { Add-CheckRow -Id $Id -Status 'FAIL' -Detail $r.Detail }
}

function Resolve-MakensisPath {
    param([string]$Explicit)
    if ($Explicit) {
        if (Test-Path -LiteralPath $Explicit) { return $Explicit }
        return $null
    }
    $candidates = @(
        'C:\Program Files (x86)\NSIS\Bin\makensis.exe',
        'C:\Program Files\NSIS\Bin\makensis.exe'
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    $cmd = Get-Command makensis -ErrorAction SilentlyContinue
    if ($null -ne $cmd) { return $cmd.Source }
    return $null
}

Write-Host ''
Write-Host ('L0 static gate  root={0}  mode={1}' -f $script:RepoRoot, $Mode) -ForegroundColor Cyan
Write-Host ''

# ---------------------------------------------------------------------------
# source 层
# ---------------------------------------------------------------------------
if ($Mode -eq 'source' -or $Mode -eq 'all') {

    Invoke-Check 'version.txt' {
        Test-TextFileShape -Path (Join-Path $script:RepoRoot 'version.txt') -ExpectBom $false
    }

    Invoke-Check 'CHANGELOG.md' {
        Test-TextFileShape -Path (Join-Path $script:RepoRoot 'CHANGELOG.md') -ExpectBom $false
    }

    Invoke-Check 'lessons-learned.md' {
        Test-TextFileShape -Path (Join-Path $script:RepoRoot '.specify\memory\lessons-learned.md') -ExpectBom $true
    }

    Invoke-Check 'install.nsi' {
        Test-TextFileShape -Path (Join-Path $script:RepoRoot 'output\install.nsi') -ExpectBom $true
    }

    # rc 文件是 UTF-16LE：一旦被某次编辑写成 UTF-8 或混进裸 LF，msbuild 会静默丢版本资源。
    Invoke-Check 'rc-utf16le' {
        $rcs = @()
        foreach ($d in @(Get-ChildItem -LiteralPath $script:RepoRoot -Directory -Filter 'Weasel*' -ErrorAction SilentlyContinue)) {
            $rcs += @(Get-ChildItem -LiteralPath $d.FullName -Recurse -File -Filter '*.rc' -ErrorAction SilentlyContinue)
        }
        if ($rcs.Count -lt 4) {
            return New-CheckResult $false ('只找到 {0} 个 .rc（至少 4 个：Server/TSF/Setup/Deployer）' -f $rcs.Count)
        }
        $bad = @()
        foreach ($f in $rcs) {
            $r = Test-Utf16LeFileShape -Path $f.FullName
            if (-not $r.Ok) { $bad += ('{0}: {1}' -f $f.Name, $r.Detail) }
        }
        if ($bad.Count -gt 0) { return New-CheckResult $false ($bad -join '；') }
        return New-CheckResult $true ('rc {0} 个全部 UTF-16LE + CRLF' -f $rcs.Count)
    }

    # L174 的齿：清理必须按映像名，且不能带 /T（/T 只走活着的进程树，打不到孤儿）。
    # 映像名的真相源是 include\WeaselIPC.h 的 OwnUiClientExeName()，不在这里重写一遍。
    Invoke-Check 'installer-ownui-kill' {
        $hdr = Join-Path $script:RepoRoot 'include\WeaselIPC.h'
        $exeName = Get-OwnUiExeNameFromHeader -Path $hdr
        if (-not $exeName) {
            return New-CheckResult $false ('无法从 {0} 提取 OwnUiClientExeName()' -f $hdr)
        }
        return Test-InstallerOwnUiKillBlocks -NsiPath (Join-Path $script:RepoRoot 'output\install.nsi') -ExpectedExeName $exeName
    }

    # L176 的齿：升级段 Rename 成功后 File 失败必须回滚，且收尾 Delete 要被守卫。
    Invoke-Check 'installer-upgrade-rollback' {
        Test-InstallerUpgradeRollback -NsiPath (Join-Path $script:RepoRoot 'output\install.nsi')
    }

    # RC7 / L107：版本号散落在 env.bat / weasel.props / rc / CHANGELOG，
    # version.txt 是新增的单一真相源，其余都必须与它对齐。
    Invoke-Check 'version-consistency' {
        Test-VersionConsistency -Root $script:RepoRoot
    }

    Invoke-Check 'script-syntax' {
        Test-ScriptSyntax -Root $script:RepoRoot
    }

    Invoke-Check 'defects-registry' {
        Test-DefectsRegistry -Root $script:RepoRoot
    }

    $mk = Resolve-MakensisPath -Explicit $MakensisPath

    if ($SkipNsisCompile) {
        Add-SkipRow 'nsis-compile' '按 -SkipNsisCompile 跳过'
    }
    elseif (-not $mk) {
        Add-SkipRow 'nsis-compile' '本机找不到 makensis（可用 -MakensisPath 显式指定）'
    }
    else {
        $outFile = Join-Path $WorkDir 'install-l0-smoke.exe'
        Invoke-Check 'nsis-compile' {
            Test-NsiCompiles -NsiPath (Join-Path $script:RepoRoot 'output\install.nsi') -MakensisPath $mk -OutFile $outFile
        }
    }

    if ($SkipGuardUnitTest) {
        Add-SkipRow 'guard-unit-test' '按 -SkipGuardUnitTest 跳过'
    }
    elseif (-not $mk) {
        Add-SkipRow 'guard-unit-test' '本机找不到 makensis（可用 -MakensisPath 显式指定）'
    }
    else {
        Invoke-Check 'guard-unit-test' {
            Test-InstallPathGuardUnitTest -OutputDir (Join-Path $script:RepoRoot 'output') -MakensisPath $mk -WorkDir $WorkDir
        }
    }
}

# ---------------------------------------------------------------------------
# artifact 层：需要 output\Win32 的构建产物，CI 上不存在属设计内
# ---------------------------------------------------------------------------
if ($Mode -eq 'artifact' -or $Mode -eq 'all') {

    . (Join-Path $script:RepoRoot 'test\regression\lib\PeVersion.ps1')

    $present = @()
    $expected = ''
    $vf = Join-Path $script:RepoRoot 'version.txt'
    if (Test-Path -LiteralPath $vf) { $expected = (Get-Content -LiteralPath $vf -Raw -Encoding utf8).Trim() }

    # 构建产物分散在两个目录：output\ 根（WeaselSetup.exe / weasel.dll / weaselx64.dll）
    # 与 output\Win32\（WeaselServer.exe / WeaselDeployer.exe）。
    # 只查单一目录会把「产物在另一个目录」误判成「产物缺失」。
    $searchDirs = @($ArtifactDir, (Split-Path -Parent $ArtifactDir))
    $searchDirs = @($searchDirs | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -Unique)

    $resolved = [ordered]@{}
    foreach ($n in $script:ProductFiles) {
        foreach ($d in $searchDirs) {
            $cand = Join-Path $d $n
            if (Test-Path -LiteralPath $cand) { $resolved[$n] = $cand; break }
        }
    }
    foreach ($n in $resolved.Keys) { $present += $n }

    if ($present.Count -eq 0) {
        Add-SkipRow 'artifact-version-resource' ('没有构建产物（{0}），按设计跳过' -f $ArtifactDir)
    }
    elseif (-not $expected) {
        Add-SkipRow 'artifact-version-resource' '缺 version.txt，无法确定期望版本'
    }
    else {
        $expectFileVersion = "$expected.0"
        Invoke-Check 'artifact-version-resource' {
            $problems = @()
            $checked = 0
            foreach ($n in $present) {
                $p = $resolved[$n]
                $info = Get-PeVersionInfo -Path $p
                if (-not $info.HasVersionResource) { $problems += ('{0}: 没有版本资源' -f $n); continue }
                if ([string]::IsNullOrEmpty($info.StringFileVersion)) {
                    $problems += ('{0}: 字符串表 FileVersion 为空（L175：rc 里要写 STRZ(FILE_VERSION)）' -f $n)
                }
                elseif ($info.StringFileVersion -ne $expectFileVersion) {
                    $problems += ('{0}: FileVersion={1} 期望 {2}' -f $n, $info.StringFileVersion, $expectFileVersion)
                }
                if ($info.FixedVersion -ne $expectFileVersion) {
                    $problems += ('{0}: 固定版本信息={1} 期望 {2}' -f $n, $info.FixedVersion, $expectFileVersion)
                }
                if (-not $info.LayersAgree) { $problems += ('{0}: 固定版本与字符串表不一致' -f $n) }
                $checked++
            }
            $missing = @($script:ProductFiles | Where-Object { $present -notcontains $_ })
            if ($missing.Count -gt 0) {
                $problems += ('产物不齐，缺：{0}' -f ($missing -join ', '))
            }
            if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '；') }
            return New-CheckResult $true ('{0} 个产物 FileVersion 均为 {1}' -f $checked, $expectFileVersion)
        }
    }
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
Write-Host ('L0 RESULT mode={0} checks={1} pass={2} fail={3} skip={4}' -f $Mode, $total, $pass, $fail, $skip)

if ($fail -gt 0) { exit 1 }
if (($total - $skip) -le 0) {
    Write-Host 'L0 没有任何检查真正跑起来 —— 空跑不算通过。' -ForegroundColor Red
    exit 1
}
exit 0
