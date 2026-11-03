#Requires -Version 5.1
<#
.SYNOPSIS
装机后校验：不信安装器退出码，只信落盘事实。

.DESCRIPTION
L174 的核心教训是「NSIS 静默模式会吞掉 File 失败」——退出码 0、注册表写到尾，
但目标文件根本没被重写。因此装机验收必须比对五件事实：

  1. 安装目录里的产物 sha256 与构建产物逐一相同
  2. PE 版本资源两层（固定信息 / 字符串表）都非空且一致
  3. 卸载项 DisplayVersion 等于期望版本
  4. 没有残留的自家 UI 进程（它们持独占锁会让下次安装失败）
  5. 目标文件可以被独占打开（有别的句柄在场就失败）

退出码：0 = 通过；1 = 失败；2 = 结论不确定（例如非管理员无法探测独占锁）。

.PARAMETER InstallDir
安装目录。默认 D:\Program Files\fluxing\weasel。

.PARAMETER BuildDir
构建产物目录。默认 <repo>\output\Win32。

.PARAMETER ExpectVersion
期望版本号（如 1.1.9）。默认读 <repo>\version.txt。

.PARAMETER SkipHash
跳过 sha256 比对。

.PARAMETER SkipLockCheck
跳过独占锁探测。

.EXAMPLE
pwsh -NoProfile -File scripts\install\verify-installed.ps1 -ExpectVersion 1.1.9
#>
[CmdletBinding()]
param(
    [string]$InstallDir = '',
    [string]$BuildDir = '',
    [string]$ExpectVersion = '',
    [switch]$SkipHash,
    [switch]$SkipLockCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

$script:RepoRoot = Resolve-RepoRoot
. (Join-Path $script:RepoRoot 'test\regression\lib\PeVersion.ps1')

if (-not $InstallDir) { $InstallDir = 'D:\Program Files\fluxing\weasel' }
if (-not $BuildDir) { $BuildDir = Join-Path $script:RepoRoot 'output' }
if (-not $ExpectVersion) {
    $vt = Join-Path $script:RepoRoot 'version.txt'
    if (Test-Path -LiteralPath $vt) { $ExpectVersion = (Get-Content -LiteralPath $vt -Raw -Encoding utf8).Trim() }
}

$products = @('WeaselServer.exe', 'WeaselDeployer.exe', 'WeaselSetup.exe', 'weasel.dll', 'weaselx64.dll')
$problems = @()
$notes = @()
$inconclusive = $false

# 构建产物分散在 output\ 根与 output\Win32\ 两处，按文件名解析到实际路径。
function Resolve-BuildArtifact {
    param([string]$Name)
    foreach ($d in @($BuildDir, (Join-Path $BuildDir 'Win32'))) {
        $cand = Join-Path $d $Name
        if (Test-Path -LiteralPath $cand) { return $cand }
    }
    return ''
}

function Get-DisplayVersion {
    $keys = @(
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing'
    )
    foreach ($k in $keys) {
        if (-not (Test-Path -LiteralPath $k)) { continue }
        $p = Get-ItemProperty -LiteralPath $k -ErrorAction SilentlyContinue
        if ($null -eq $p) { continue }
        $dv = $p.PSObject.Properties['DisplayVersion']
        if ($null -ne $dv -and $dv.Value) { return [string]$dv.Value }
    }
    # 兜底：扫描全部卸载项，按 DisplayName 找 Fluxing（键名可能与产品名不同）。
    $bases = @(
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
    )
    foreach ($base in $bases) {
        if (-not (Test-Path -LiteralPath $base)) { continue }
        foreach ($sub in @(Get-ChildItem -LiteralPath $base -ErrorAction SilentlyContinue)) {
            $p = Get-ItemProperty -LiteralPath $sub.PSPath -ErrorAction SilentlyContinue
            if ($null -eq $p) { continue }
            $dn = $p.PSObject.Properties['DisplayName']
            if ($null -eq $dn -or -not $dn.Value) { continue }
            if ([string]$dn.Value -notlike '*Fluxing*') { continue }
            $dv = $p.PSObject.Properties['DisplayVersion']
            if ($null -ne $dv -and $dv.Value) { return [string]$dv.Value }
        }
    }
    return ''
}

Write-Host ("VERIFY install_dir={0}" -f $InstallDir)
Write-Host ("VERIFY build_dir={0} expect_version={1}" -f $BuildDir, $ExpectVersion)

if (-not (Test-Path -LiteralPath $InstallDir)) {
    Write-Host ("VERIFY RESULT ok=False inconclusive=False problems=1 detail={0}" -f "安装目录不存在：$InstallDir")
    exit 1
}

# 0) 必备文件存在性 —— 与哈希段解耦：没有构建产物也能判。
#
# 事故（L176）：升级段 Rename 旧 exe 成功 -> File 写新 exe 失败 -> 无条件删掉
# .old.tmp => 该 exe 从安装目录彻底消失，而安装器退出码仍然是 0。旧版本这段只比
# 「存在的文件哈希对不对」，不看「该有的文件在不在」⇒ 事故发生后没有任何门禁变红。
# 所以这一段的判据必须独立于 $BuildDir 是否存在。
$required = @(
    'WeaselServer.exe', 'WeaselDeployer.exe', 'WeaselSetup.exe',
    'uninstall.exe', 'FluxingPhrasesDialog.exe'
)
foreach ($req in $required) {
    $p = Join-Path $InstallDir $req
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) {
        $problems += "安装目录缺必备文件 $req（升级段写失败且未回滚的典型症状）"
        continue
    }
    $fi = Get-Item -LiteralPath $p
    if ($fi.Length -le 0) { $problems += "必备文件 $req 长度为 0" }
}
# .old.tmp 是升级段的退路，不是垃圾：它还在 ⇒ 上次安装的 File 写入失败或回滚未完成。
foreach ($left in @(Get-ChildItem -LiteralPath $InstallDir -File -Filter '*.old.tmp' -ErrorAction SilentlyContinue)) {
    $notes += ("发现升级残留 {0}（{1} B）⇒ 上次安装写新文件失败或回滚未完成" -f $left.Name, $left.Length)
}

# 1) sha256：安装后至少要有一份「活的」文件与构建产物逐字节相同
#
# 与下面版本资源段同口径的「活体判定」：install.nsi 会把 weasel*.dll 改写成版本化
# shim（weaselx64.<ver>.<build>.<seq>.dll）并对那一份 regsvr32；普通名那份可能是
# 删不掉的旧残留。1.1.9 装机现场实证：普通名 weaselx64.dll 是 1.1.7.0 残留
# （哈希 48C02E29EDE0..），而两份 shim 与构建产物同为 C401266DA8C3..。
# ⇒ 判据 = 任一候选哈希 == 构建哈希，而不是只比普通名（否则必然假阳性）。
function Get-InstalledCandidates {
    param(
        [Parameter(Mandatory = $true)][string]$Dir,
        [Parameter(Mandatory = $true)][string]$Name
    )
    $out = @()
    $plain = Join-Path $Dir $Name
    if (Test-Path -LiteralPath $plain) { $out += $plain }
    if ($Name -like '*.dll') {
        $stem = [System.IO.Path]::GetFileNameWithoutExtension($Name)
        $pattern = '^{0}\..+\.dll$' -f [regex]::Escape($stem)
        foreach ($x in @(Get-ChildItem -LiteralPath $Dir -File -ErrorAction SilentlyContinue)) {
            if ($x.Name -match $pattern) { $out += $x.FullName }
        }
    }
    return $out
}

if (-not $SkipHash) {
    if (-not (Test-Path -LiteralPath $BuildDir)) {
        $inconclusive = $true
        $notes += "构建目录不存在，跳过哈希比对：$BuildDir"
    }
    else {
        foreach ($f in $products) {
            $src = Resolve-BuildArtifact -Name $f
            if (-not $src) { $problems += "构建产物缺 $f（无法做哈希比对）"; continue }
            $want = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash
            $cands = @(Get-InstalledCandidates -Dir $InstallDir -Name $f)
            if ($cands.Count -eq 0) { $problems += "安装目录缺 $f"; continue }
            $matched = @()
            $seen = @()
            foreach ($c in $cands) {
                $h = (Get-FileHash -LiteralPath $c -Algorithm SHA256).Hash
                $leaf = Split-Path -Leaf $c
                $seen += ("{0}={1}.." -f $leaf, $h.Substring(0, 12))
                if ($h -eq $want) { $matched += $leaf }
            }
            if ($matched.Count -eq 0) {
                $problems += ("{0} 哈希对不上构建产物（构建侧 {1}..；安装侧 {2}）⇒ 安装没真正重写文件" -f $f, $want.Substring(0, 12), ($seen -join ', '))
            }
            elseif ($matched -notcontains $f) {
                $notes += ("{0} 走的是版本化 shim：{1}（普通名残留不作为判据）" -f $f, ($matched -join ', '))
            }
        }
    }
}

# 2) PE 版本资源：两层都不能空，且必须能对上期望版本。
#    weasel.dll / weaselx64.dll 会被 install.nsi 改写成版本化 shim
#    （weaselx64.<ver>.<build>.<seq>.dll）并 regsvr32 那一份；TSF 锁住原文件时
#    普通名那份可能是删不掉的旧残留 ⇒ 判据必须是「至少有一份活的对得上版本」，
#    而不是只看普通名（否则必然假阳性）。
foreach ($f in $products) {
    $candidates = @()
    $dst = Join-Path $InstallDir $f
    if (Test-Path -LiteralPath $dst) { $candidates += $dst }
    if ($f -like '*.dll') {
        $stem = [System.IO.Path]::GetFileNameWithoutExtension($f)
        $pattern = '^{0}\..+\.dll$' -f [regex]::Escape($stem)
        foreach ($x in @(Get-ChildItem -LiteralPath $InstallDir -File -ErrorAction SilentlyContinue)) {
            if ($x.Name -match $pattern) { $candidates += $x.FullName }
        }
    }
    if ($candidates.Count -eq 0) { continue }

    $good = @()
    $bad = @()
    foreach ($c in $candidates) {
        $leaf = Split-Path -Leaf $c
        $v = Get-PeVersionInfo -Path $c
        if (-not $v.HasVersionResource) { $bad += ("{0}: 无版本资源" -f $leaf); continue }
        if (-not $v.LayersAgree) { $bad += ("{0}: 固定版本({1}) 与字符串表({2}) 不一致" -f $leaf, $v.FixedVersion, $v.StringFileVersion); continue }
        if (-not $v.StringFileVersion) { $bad += ("{0}: StringFileVersion 为空（L175：rc 里 FILE_VERSION 没走 STRZ）" -f $leaf); continue }
        if ($ExpectVersion -and $v.StringFileVersion -ne "$ExpectVersion.0") {
            $bad += ("{0}: FileVersion={1} != {2}.0" -f $leaf, $v.StringFileVersion, $ExpectVersion); continue
        }
        $good += $leaf
    }
    if ($good.Count -eq 0) {
        $problems += ("{0} 没有任何一份版本资源对得上 {1}.0（{2}）" -f $f, $ExpectVersion, ($bad -join "; "))
    }
    elseif ($f -like '*.dll' -and $good -notcontains $f) {
        $notes += ("{0} 走的是版本化 shim：{1}（普通名残留不作为判据）" -f $f, ($good -join ", "))
    }
}

# 3) 卸载项版本
$dv = Get-DisplayVersion
if (-not $dv) {
    $problems += '注册表里找不到 Fluxing 卸载项的 DisplayVersion'
}
elseif ($ExpectVersion -and $dv -ne $ExpectVersion) {
    $problems += ("DisplayVersion={0} != {1}" -f $dv, $ExpectVersion)
}
else {
    $notes += "DisplayVersion=$dv"
}

# 4) 残留自家 UI 进程（孤儿会持独占锁 ⇒ 下次安装失败，L174）
$uiProcs = @(Get-Process -Name 'FluxingPhrasesDialog' -ErrorAction SilentlyContinue)
if ($uiProcs.Count -gt 0) {
    $problems += ("残留 FluxingPhrasesDialog.exe {0} 个（持独占锁会让下次安装失败）" -f $uiProcs.Count)
}

# 5) 独占锁探测：用 Read + None（D:\Program Files 下非管理员无写权限，
#    用 ReadWrite 会抛 UnauthorizedAccessException，误判成「被锁」）
if (-not $SkipLockCheck) {
    foreach ($f in $products) {
        $dst = Join-Path $InstallDir $f
        if (-not (Test-Path -LiteralPath $dst)) { continue }
        $fs = $null
        try {
            $fs = [System.IO.File]::Open($dst, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
        }
        catch [System.IO.IOException] {
            $problems += ("{0} 被其它进程独占（安装无法覆盖，L174）" -f $f)
        }
        catch [System.UnauthorizedAccessException] {
            $inconclusive = $true
            $notes += "无读权限，跳过独占探测：$f"
        }
        finally {
            if ($null -ne $fs) { $fs.Dispose() }
        }
    }
}

# ------------------------------ 汇总 ------------------------------
$ok = ($problems.Count -eq 0)
$detail = '安装状态与构建产物一致'
if (-not $ok) { $detail = ($problems -join '; ') }
if ($notes.Count -gt 0) { $detail = $detail + '；' + ($notes -join '；') }

if (-not $ok) {
    foreach ($p in $problems) { Write-Host ("VERIFY PROBLEM {0}" -f $p) }
}
Write-Host ("VERIFY RESULT ok={0} inconclusive={1} problems={2} detail={3}" -f $ok, $inconclusive, $problems.Count, $detail)

if (-not $ok) { exit 1 }
if ($inconclusive) { exit 2 }
exit 0
