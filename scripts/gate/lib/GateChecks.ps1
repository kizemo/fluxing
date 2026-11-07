# GateChecks.ps1 —— L0/L3 共用的纯函数检查集合
#
# 设计约束（ADR 0033）：
#   - 每个检查都接受**显式路径**，不自己去仓库里找东西 ⇒ L3 变异门禁可以把它指向临时副本，
#     从而证明「同一个检查实现」真的会翻红（有齿），而不是另写一套判据。
#   - 每个检查返回对象 {Ok = $true/$false; Detail = '...'}，不抛异常、不 exit。
#   - 不做写操作（除 Test-NsiCompiles / Test-InstallPathGuardUnitTest 需要在 $WorkDir 下产出）。

Set-StrictMode -Version Latest

function New-CheckResult {
    param([bool]$Ok, [string]$Detail, $Data = $null)
    return [pscustomobject]@{ Ok = $Ok; Detail = $Detail; Data = $Data }
}

# ---------------------------------------------------------------------------
# 文本文件形状：BOM / 行尾
# ---------------------------------------------------------------------------
function Get-TextFileShape {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $hasBom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    # Latin-1 让字节 1:1 映射成字符，便于用正则数行尾。
    $text = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    $crlf = ([regex]::Matches($text, "`r`n")).Count
    $bareLf = ([regex]::Matches($text, "(?<!`r)`n")).Count
    return [pscustomobject]@{
        Path   = $Path
        Size   = $bytes.Length
        HasBom = $hasBom
        Crlf   = $crlf
        BareLf = $bareLf
    }
}

# 断言一个文本文件的形状。ExpectBom=false 时要求「无 BOM」。
function Test-TextFileShape {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$ExpectBom,
        [switch]$AllowNoContent
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        return New-CheckResult $false "缺文件：$Path"
    }
    $s = Get-TextFileShape -Path $Path
    $problems = @()
    if ($s.HasBom -ne $ExpectBom) { $problems += ("BOM={0}（期望 {1}）" -f $s.HasBom, $ExpectBom) }
    if ($s.BareLf -ne 0) { $problems += ("裸 LF={0} 处（必须 100% CRLF）" -f $s.BareLf) }
    if (-not $AllowNoContent -and $s.Size -le 0) { $problems += '文件为空' }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') $s }
    return New-CheckResult $true ("size={0} BOM={1} crlf={2} bareLF=0" -f $s.Size, $s.HasBom, $s.Crlf) $s
}

# ---------------------------------------------------------------------------
# UTF-16LE（rc 文件）形状：BOM + 000D000A 行尾，且不得出现裸 000A
# ---------------------------------------------------------------------------
function Test-Utf16LeFileShape {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) { return New-CheckResult $false "缺文件：$Path" }
    $b = [System.IO.File]::ReadAllBytes($Path)
    if ($b.Length -lt 2) { return New-CheckResult $false '文件为空' }
    $hasBom = ($b[0] -eq 0xFF -and $b[1] -eq 0xFE)
    $units = [int](($b.Length - 2) / 2)
    $crlf = 0
    $bareLf = 0
    for ($i = 0; $i -lt $units; $i++) {
        # 必须显式 [int]：PowerShell 的 -shl 会把结果截断回左操作数的类型，
        # 对 [byte] 高字节移位恒得 0，导致 0x6B0A 这类码元被误判成 0x000A。
        $u = [int]$b[2 + 2 * $i] -bor ([int]$b[3 + 2 * $i] -shl 8)
        if ($u -eq 0x000A) {
            $prev = 0
            if ($i -gt 0) { $prev = [int]$b[2 + 2 * ($i - 1)] -bor ([int]$b[3 + 2 * ($i - 1)] -shl 8) }
            if ($prev -eq 0x000D) { $crlf++ } else { $bareLf++ }
        }
    }
    $problems = @()
    if (-not $hasBom) { $problems += 'BOM 不是 FF FE（UTF-16LE）' }
    if ($bareLf -ne 0) { $problems += ("裸 LF={0} 处" -f $bareLf) }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') }
    return New-CheckResult $true ("utf-16le bom=ok crlf={0} bareLF=0" -f $crlf) $null
}

# ---------------------------------------------------------------------------
# 磁盘布局：lint×W 磁盘数量上界
# ---------------------------------------------------------------------------
function Get-DiskLayoutShape {
    param([Parameter(Mandatory = $true)][string]$Path)
    $s = Get-TextFileShape -Path $Path
    return New-CheckResult $true ("size={0}" -f $s.Size) $s
}

# ---------------------------------------------------------------------------
# 自家 UI 映像名的单一真相源（include/WeaselIPC.h）
# ---------------------------------------------------------------------------
function Get-OwnUiExeNameFromHeader {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $text = Get-Content -LiteralPath $Path -Raw -Encoding utf8
    $m = [regex]::Match($text, 'OwnUiClientExeName\(\)\s*\{\s*return\s*"([^"]+)"')
    if ($m.Success) { return $m.Groups[1].Value }
    return $null
}

# ---------------------------------------------------------------------------
# install.nsi：按映像名清理自家 UI 进程（L174 的齿）
# ---------------------------------------------------------------------------
function Test-InstallerOwnUiKillBlocks {
    param(
        [Parameter(Mandatory = $true)][string]$NsiPath,
        [Parameter(Mandatory = $true)][string]$ExpectedExeName,
        [int]$MinBlocks = 4
    )
    if (-not (Test-Path -LiteralPath $NsiPath)) { return New-CheckResult $false "缺文件：$NsiPath" }
    $text = Get-Content -LiteralPath $NsiPath -Raw -Encoding utf8

    $bare = ([regex]::Matches($text, [regex]::Escape("taskkill /F /IM $ExpectedExeName'"))).Count
    $withTree = ([regex]::Matches($text, [regex]::Escape("taskkill /F /IM $ExpectedExeName /T'"))).Count
    $problems = @()
    if ($bare -lt $MinBlocks) { $problems += ("按映像名清理块 {0} 处 < 要求 {1} 处" -f $bare, $MinBlocks) }
    if ($withTree -ne 0) { $problems += ("有 {0} 处清理块带了 /T（/T 只走活着的进程树，打不到孤儿）" -f $withTree) }
    # 注释里的「同源」提示行也要在：它是给下一个人看的防改名提示。
    $syncHints = ([regex]::Matches($text, 'Keep this name in sync with fluxing::OwnUiClientExeName')).Count
    if ($syncHints -lt $MinBlocks) { $problems += ("同源提示注释 {0} 处 < {1} 处" -f $syncHints, $MinBlocks) }

    $data = [pscustomobject]@{ BareBlocks = $bare; WithTree = $withTree; SyncHints = $syncHints }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') $data }
    return New-CheckResult $true ("按映像名清理块={0}（无 /T）同源注释={1}" -f $bare, $syncHints) $data
}

# ---------------------------------------------------------------------------
# install.nsi：升级段「File 失败必须回滚」（L176 的齿）
#
# 事故：Rename 旧 exe -> .old.tmp 成功后，File 写新 exe 失败（文件仍被占用/被
# 杀软拦截）时，旧代码裸跑 File 后无条件 Delete .old.tmp => 旧二进制和新二进制
# 双双消失，安装目录里该 exe 彻底不见，而安装器退出码仍是 0。
# 判据：每个 exe 必须存在 三件套 = IfErrors 分支 + Rename 回滚 + _rename_ok 标签，
# 且收尾 sweep 里的 Delete .old.tmp 必须被 FileExists 守卫。
# ---------------------------------------------------------------------------
function Test-InstallerUpgradeRollback {
    param(
        [Parameter(Mandatory = $true)][string]$NsiPath,
        [string[]]$ExeNames = @('WeaselServer', 'WeaselDeployer', 'WeaselSetup')
    )
    if (-not (Test-Path -LiteralPath $NsiPath)) { return New-CheckResult $false "缺文件：$NsiPath" }
    $lines = @(Get-Content -LiteralPath $NsiPath -Encoding utf8)
    $problems = @()
    $rows = @()

    foreach ($x in $ExeNames) {
        $okLabel = $x + '_rename_ok:'
        $doneLabel = $x + '_done:'
        $guard = 'IfErrors 0 ' + $x + '_rename_ok'
        $rollback = 'Rename "$INSTDIR\' + $x + '.exe.old.tmp" "$INSTDIR\' + $x + '.exe"'
        $fileLine = 'File "Win32\' + $x + '.exe"'
        $fileLineAlt = 'File "' + $x + '.exe"'
        $tailDel = 'Delete "$INSTDIR\' + $x + '.exe.old.tmp"'
        $existsGuard = '${If} ${FileExists} "$INSTDIR\' + $x + '.exe"'

        $idxOk = -1; $idxDone = -1; $okCount = 0; $idxGuard = -1; $idxRollback = -1
        $idxFile = -1; $idxTail = -1
        for ($i = 0; $i -lt $lines.Count; $i++) {
            $t = $lines[$i].Trim()
            if ($t -eq $okLabel) { $okCount++; if ($idxOk -lt 0) { $idxOk = $i } }
            if ($t -eq $doneLabel) { $idxDone = $i }
            if ($t -eq $guard -and $idxGuard -lt 0) { $idxGuard = $i }
            if ($t -eq $rollback -and $idxGuard -ge 0) { $idxRollback = $i }
            if ($t -eq $fileLine -and $idxGuard -lt 0) { $idxFile = $i }
            elseif ($t -eq $fileLineAlt -and $idxGuard -lt 0) { $idxFile = $i }
            if ($t -eq $tailDel) { $idxTail = $i }
        }

        if ($okCount -ne 1) { $problems += ("{0}: {1} 出现 {2} 次（应恰好 1 次）" -f $x, $okLabel, $okCount) }
        if ($idxDone -lt 0) { $problems += ("{0}: 缺 {1}" -f $x, $doneLabel) }
        if ($idxGuard -lt 0) { $problems += ("{0}: 缺 '{1}' —— 升级段 File 失败无分支" -f $x, $guard) }
        if ($idxRollback -lt 0) { $problems += ("{0}: 缺 '{1}' —— File 失败不回滚，exe 会消失" -f $x, $rollback) }
        if ($idxGuard -ge 0 -and $idxFile -lt 0) { $problems += ("{0}: IfErrors 分支之前找不到 File 行（升级段结构变了）" -f $x) }

        # 收尾 sweep：最后一次 Delete .old.tmp 必须被 FileExists 守卫包住
        $tailGuarded = $false
        if ($idxTail -ge 0) {
            $lo = [Math]::Max(0, $idxTail - 2)
            $hi = [Math]::Min($lines.Count - 1, $idxTail + 2)
            for ($j = $lo; $j -lt $idxTail; $j++) {
                if ($lines[$j].Trim() -eq $existsGuard) { $tailGuarded = $true }
            }
        }
        if ($idxTail -lt 0) { $problems += ("{0}: 收尾 sweep 缺 Delete .old.tmp（结构变了，请人工复核）" -f $x) }
        elseif (-not $tailGuarded) { $problems += ("{0}: 收尾 Delete .old.tmp 未被 FileExists 守卫 —— 回滚失败时删掉唯一退路" -f $x) }

        $rows += [pscustomobject]@{
            Exe        = $x
            OkLabel    = $idxOk
            Guard      = $idxGuard
            Rollback   = $idxRollback
            TailGuard  = $tailGuarded
        }
    }

    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') $rows }
    return New-CheckResult $true ("三段升级回滚完整（{0} 个 exe，均有 IfErrors/Rename/_rename_ok + 收尾 FileExists 守卫）" -f $ExeNames.Count) $rows
}

# ---------------------------------------------------------------------------
# NSIS 编译（G-a）：用 /DFLUXING_OUTFILE= 把产物导到别处，绝不覆盖 output\archives
# ---------------------------------------------------------------------------
function Test-NsiCompiles {
    param(
        [Parameter(Mandatory = $true)][string]$NsiPath,
        [Parameter(Mandatory = $true)][string]$MakensisPath,
        [Parameter(Mandatory = $true)][string]$OutFile
    )
    if (-not (Test-Path -LiteralPath $MakensisPath)) { return New-CheckResult $false "缺 makensis：$MakensisPath" }
    if (-not (Test-Path -LiteralPath $NsiPath)) { return New-CheckResult $false "缺 nsi：$NsiPath" }
    if (Test-Path -LiteralPath $OutFile) { Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue }

    $nsiDir = Split-Path -Parent $NsiPath
    $nsiLeaf = Split-Path -Leaf $NsiPath
    Push-Location -LiteralPath $nsiDir
    try {
        $out = & $MakensisPath "/DFLUXING_OUTFILE=$OutFile" $nsiLeaf 2>&1
        $rc = $LASTEXITCODE
    }
    finally { Pop-Location }

    $tail = @($out | Select-Object -Last 6) -join ' / '
    if ($rc -ne 0) { return New-CheckResult $false ("makensis 退出码={0}: {1}" -f $rc, $tail) }
    if (-not (Test-Path -LiteralPath $OutFile)) { return New-CheckResult $false ("makensis 成功但没有产物：{0}" -f $OutFile) }
    return New-CheckResult $true ("makensis ok -> {0} ({1} B)" -f (Split-Path -Leaf $OutFile), (Get-Item -LiteralPath $OutFile).Length)
}

# ---------------------------------------------------------------------------
# 安装路径守卫单测（G-b，L169）：编译 + 运行，要求 TOTAL=16 MISMATCH=0
# ---------------------------------------------------------------------------
function Test-InstallPathGuardUnitTest {
    param(
        [Parameter(Mandatory = $true)][string]$OutputDir,
        [Parameter(Mandatory = $true)][string]$MakensisPath,
        [Parameter(Mandatory = $true)][string]$WorkDir
    )
    $nsi = Join-Path $OutputDir 'guard_install_path_test.nsi'
    if (-not (Test-Path -LiteralPath $nsi)) { return New-CheckResult $false "缺守卫测例：$nsi" }
    if (-not (Test-Path -LiteralPath $MakensisPath)) { return New-CheckResult $false "缺 makensis：$MakensisPath" }
    if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null }
    $exe = Join-Path $WorkDir 'guard_install_path_test.exe'
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force -ErrorAction SilentlyContinue }

    # 测例的结果通道是文件而不是 stdout（nsi:10-14 的契约）：
    #   ${GUARD_TEST_OUT}\guard_install_path_result.txt，末行 TOTAL=<n> MISMATCH=<m>
    # GUARD_TEST_OUT 默认 $EXEDIR ⇒ 与 $exe 同目录。必须先删上一轮残留，否则 exe
    # 没跑起来时会读到旧结果，把「没运行」误判成「跑通了」。
    $resultFile = Join-Path $WorkDir 'guard_install_path_result.txt'
    if (Test-Path -LiteralPath $resultFile) { Remove-Item -LiteralPath $resultFile -Force -ErrorAction SilentlyContinue }

    # L0 自检缺陷（已实测）：makensis 的 /O<file> 不是本测例支持的覆盖开关。
    # output\guard_install_path_test.nsi:29-31 声明的契约是 !ifdef GUARD_TEST_OUTFILE
    # ⇒ 必须传 /DGUARD_TEST_OUTFILE=<绝对路径>（/D 的值不能带引号，也不能含空格）。
    # 用 /O 时真正的 exe 落在脚本同目录，/O 指向的位置被写成 882 B 编译文本
    # （首 4 字节为 ASCII "Proc"），运行段才报
    # "The specified executable is not a valid application for this OS platform."
    if ($exe -match '\s') {
        return New-CheckResult $false ("-WorkDir 路径含空格，NSIS 的 /D 传参无法表达：$exe")
    }
    Push-Location -LiteralPath $OutputDir
    try {
        $cout = & $MakensisPath "/DGUARD_TEST_OUTFILE=$exe" 'guard_install_path_test.nsi' 2>&1
        $crc = $LASTEXITCODE
    }
    finally { Pop-Location }
    if ($crc -ne 0 -or -not (Test-Path -LiteralPath $exe)) {
        return New-CheckResult $false ("守卫测例编译失败 rc={0}: {1}" -f $crc, (@($cout | Select-Object -Last 5) -join ' / '))
    }

    # 反向自检：产物必须是 PE（MZ 头）。若将来有人把输出路径覆盖改错（例如回到
    # /O 或换个不存在的开关），makensis 的编译文本会静默落在这里，而运行段只会
    # 给出误导性的「不是有效应用程序」——那会烧掉一整轮排查。
    $magic = New-Object byte[] 2
    $probeStream = [System.IO.File]::OpenRead($exe)
    try { [void]$probeStream.Read($magic, 0, 2) } finally { $probeStream.Dispose() }
    if ($magic[0] -ne 0x4D -or $magic[1] -ne 0x5A) {
        return New-CheckResult $false ("守卫测例产物不是 PE（前 2 字节={0}，{1} B）⇒ makensis 输出路径覆盖没生效" -f ([BitConverter]::ToString($magic)), (Get-Item -LiteralPath $exe).Length)
    }

    # L0 自检缺陷：& $exe 2>&1 在宿主 stdout 未做进程级重定向时会抛
    # "StandardOutputEncoding is only supported when standard output is redirected"，
    # 因为 PowerShell 原生命令机制要接管输出编码。这里用 ProcessStartInfo 显式重定向。
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = (Split-Path -Parent $exe)
    $proc = [System.Diagnostics.Process]::Start($psi)
    $routText = $proc.StandardOutput.ReadToEnd()
    $rerrText = $proc.StandardError.ReadToEnd()
    $proc.WaitForExit()
    $rrc = $proc.ExitCode
    $proc.Dispose()
    # nsi 用 Unicode true ⇒ NSIS 的 FileWrite 写 UTF-16LE + BOM。
    # ReadAllText 会自动按 BOM 选编码；Get-Content -Encoding utf8 会读成乱码。
    $rout = @($routText -split "`r?`n")
    if ($rerrText) { $rout += @($rerrText -split "`r?`n") }
    $diag = @($rout | Where-Object { $_.Trim() }) | Select-Object -First 3
    if (-not (Test-Path -LiteralPath $resultFile)) {
        return New-CheckResult $false ("守卫测例没产出结果文件（rc={0}；{1}；stdout/stderr: {2}）" -f $rrc, $resultFile, ($diag -join ' / '))
    }
    $resLines = @(([System.IO.File]::ReadAllText($resultFile)) -split "`r?`n")
    $line = @($resLines | Where-Object { $_ -match '^TOTAL=' }) | Select-Object -Last 1
    if (-not $line) { return New-CheckResult $false ("结果文件里没有 TOTAL 行（rc={0}）：{1}" -f $rrc, $resultFile) }
    $m = [regex]::Match($line, 'TOTAL=(\d+)\s+MISMATCH=(\d+)')
    if (-not $m.Success) { return New-CheckResult $false ("TOTAL 行格式异常：{0}" -f $line) }
    $total = [int]$m.Groups[1].Value
    $mismatch = [int]$m.Groups[2].Value
    if ($total -le 0) { return New-CheckResult $false '守卫测例用例数为 0（空测例不算通过）' }
    if ($mismatch -ne 0 -or $rrc -ne 0) {
        $fails = @($resLines | Where-Object { $_ -match '^MISMATCH' }) | Select-Object -First 4
        return New-CheckResult $false ("守卫单测不符：{0} rc={1}；{2}" -f $line, $rrc, ($fails -join ' / '))
    }
    return New-CheckResult $true ("{0} rc=0（{1}）" -f $line, (Split-Path -Leaf $resultFile))
}

# ---------------------------------------------------------------------------
# 版本单一真相源：version.txt ↔ weasel.props ↔ env.bat ↔ CHANGELOG.md
# ---------------------------------------------------------------------------
function Get-VersionFacts {
    param([Parameter(Mandatory = $true)][string]$Root)

    $facts = [ordered]@{}
    $vFile = Join-Path $Root 'version.txt'
    $facts.VersionTxt = if (Test-Path -LiteralPath $vFile) { (Get-Content -LiteralPath $vFile -Raw -Encoding utf8).Trim() } else { '' }

    $props = Join-Path $Root 'weasel.props'
    $propsText = ''
    if (Test-Path -LiteralPath $props) { $propsText = Get-Content -LiteralPath $props -Raw -Encoding utf8 }
    $pick = {
        param($text, $tag)
        $m = [regex]::Match($text, "<$tag>\s*([^<]+?)\s*</$tag>")
        if ($m.Success) { return $m.Groups[1].Value } else { return '' }
    }
    $facts.PropsMajor = & $pick $propsText 'VERSION_MAJOR'
    $facts.PropsMinor = & $pick $propsText 'VERSION_MINOR'
    $facts.PropsPatch = & $pick $propsText 'VERSION_PATCH'
    $facts.PropsProduct = & $pick $propsText 'PRODUCT_VERSION'
    $facts.PropsFile = & $pick $propsText 'FILE_VERSION'

    $envBat = Join-Path $Root 'env.bat'
    $envText = ''
    if (Test-Path -LiteralPath $envBat) { $envText = Get-Content -LiteralPath $envBat -Raw -Encoding utf8 }
    $pickEnv = {
        param($text, $name)
        $m = [regex]::Match($text, ('set\s+"?' + $name + '=([0-9.]+)"?'))
        if ($m.Success) { return $m.Groups[1].Value } else { return '' }
    }
    $facts.EnvFluxing = & $pickEnv $envText 'FLUXING_VERSION'
    $facts.EnvProduct = & $pickEnv $envText 'PRODUCT_VERSION'
    $facts.EnvFile = & $pickEnv $envText 'FILE_VERSION'
    $facts.EnvPatch = & $pickEnv $envText 'VERSION_PATCH'

    $changelog = Join-Path $Root 'CHANGELOG.md'
    $logText = ''
    if (Test-Path -LiteralPath $changelog) { $logText = Get-Content -LiteralPath $changelog -Raw -Encoding utf8 }
    $facts.ChangelogTop = ''
    $m = [regex]::Match($logText, '(?m)^##\s*\[(\d+\.\d+\.\d+)\]')
    if ($m.Success) { $facts.ChangelogTop = $m.Groups[1].Value }

    return [pscustomobject]$facts
}

function Test-VersionConsistency {
    param([Parameter(Mandatory = $true)][string]$Root)

    $f = Get-VersionFacts -Root $Root
    $v = $f.VersionTxt
    $problems = @()
    $skipped = @()
    if ($v -notmatch '^\d+\.\d+\.\d+$') { return New-CheckResult $false "version.txt 内容非法：'$v'" $f }
    $parts = $v.Split('.')
    $expectProduct = "$v.0"

    # weasel.props / env.bat 是机器本地文件（git-ignored，weasel.props 含机器特有 BOOST_ROOT）。
    # 缺失时跳过该组比较并在 detail 里显式说明 —— 否则 CI（干净 clone）第一天就恒红，
    # 逼人直接把门禁关掉，比没有门禁更差。
    $hasProps = Test-Path -LiteralPath (Join-Path $Root 'weasel.props')
    $hasEnv = Test-Path -LiteralPath (Join-Path $Root 'env.bat')
    if (-not $hasProps) { $skipped += 'weasel.props 缺失（git-ignored）已跳过该组' }
    if (-not $hasEnv) { $skipped += 'env.bat 缺失（git-ignored）已跳过该组' }

    if ($hasProps) {
        if ($f.PropsMajor -ne $parts[0]) { $problems += "weasel.props VERSION_MAJOR=$($f.PropsMajor) != $($parts[0])" }
        if ($f.PropsMinor -ne $parts[1]) { $problems += "weasel.props VERSION_MINOR=$($f.PropsMinor) != $($parts[1])" }
        if ($f.PropsPatch -ne $parts[2]) { $problems += "weasel.props VERSION_PATCH=$($f.PropsPatch) != $($parts[2])" }
        if ($f.PropsProduct -ne $expectProduct) { $problems += "weasel.props PRODUCT_VERSION=$($f.PropsProduct) != $expectProduct" }
        if ($f.PropsFile -ne $expectProduct) { $problems += "weasel.props FILE_VERSION=$($f.PropsFile) != $expectProduct" }
    }
    if ($hasEnv) {
        if ($f.EnvFluxing -ne $v) { $problems += "env.bat FLUXING_VERSION=$($f.EnvFluxing) != $v" }
        if ($f.EnvProduct -ne $expectProduct) { $problems += "env.bat PRODUCT_VERSION=$($f.EnvProduct) != $expectProduct" }
        if ($f.EnvFile -ne $expectProduct) { $problems += "env.bat FILE_VERSION=$($f.EnvFile) != $expectProduct" }
        if ($f.EnvPatch -ne $parts[2]) { $problems += "env.bat VERSION_PATCH=$($f.EnvPatch) != $($parts[2])" }
    }
    if ($f.ChangelogTop -ne $v) { $problems += "CHANGELOG.md 顶部版本=$($f.ChangelogTop) != $v" }

    $note = if ($skipped.Count -gt 0) { '；' + ($skipped -join '；') } else { '' }
    if ($problems.Count -gt 0) { return New-CheckResult $false (($problems -join '; ') + $note) $f }
    return New-CheckResult $true ("version=$v 一致" + $note) $f
}

# ---------------------------------------------------------------------------
# 脚本可解析性 + .cmd 纯 ASCII
# ---------------------------------------------------------------------------
function Test-ScriptSyntax {
    param([Parameter(Mandatory = $true)][string]$Root)

    $ps1 = @()
    foreach ($dir in @((Join-Path $Root 'scripts'), (Join-Path $Root 'test'))) {
        if (Test-Path -LiteralPath $dir) {
            $ps1 += Get-ChildItem -LiteralPath $dir -Recurse -File -Filter '*.ps1' -ErrorAction SilentlyContinue
        }
    }
    $problems = @()
    foreach ($f in $ps1) {
        $tokens = $null
        $errors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($f.FullName, [ref]$tokens, [ref]$errors) | Out-Null
        if ($errors -and $errors.Count -gt 0) {
            $first = $errors[0]
            $problems += ("{0}:{1} {2}" -f $f.Name, $first.Extent.StartLineNumber, $first.Message)
        }
    }

    $cmds = @()
    foreach ($dir in @((Join-Path $Root 'scripts'), (Join-Path $Root 'test'))) {
        if (Test-Path -LiteralPath $dir) {
            $cmds += Get-ChildItem -LiteralPath $dir -Recurse -File -Filter '*.cmd' -ErrorAction SilentlyContinue
        }
    }
    foreach ($f in $cmds) {
        $b = [System.IO.File]::ReadAllBytes($f.FullName)
        for ($i = 0; $i -lt $b.Length; $i++) {
            if ($b[$i] -ge 0x80) {
                $problems += ("{0}: 第 {1} 字节 >=0x80（.cmd 必须纯 ASCII）" -f $f.Name, $i)
                break
            }
        }
    }

    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') }
    return New-CheckResult $true ("ps1={0} 个可解析, cmd={1} 个纯 ASCII" -f $ps1.Count, $cmds.Count)
}

# ---------------------------------------------------------------------------
# 缺陷登记表 ⇄ 用例双向一致 + 用例内禁 tmp/（RC1 的齿）
# ---------------------------------------------------------------------------
function Test-DefectsRegistry {
    param([Parameter(Mandatory = $true)][string]$Root)

    $regPath = Join-Path $Root 'test\regression\DEFECTS.yaml'
    $casesDir = Join-Path $Root 'test\regression\cases'
    if (-not (Test-Path -LiteralPath $regPath)) { return New-CheckResult $false "缺 DEFECTS.yaml：$regPath" }
    if (-not (Test-Path -LiteralPath $casesDir)) { return New-CheckResult $false "缺 cases 目录：$casesDir" }

    $lines = Get-Content -LiteralPath $regPath -Encoding utf8
    # 注意：这里必须先声明 $problems。曾经在函数末尾再 `$problems = @()`
    # 会把前面收集到的问题全部清空 ⇒ 检查永远绿（L177 的同类假牙）。
    $problems = @()
    $entries = @()
    $cur = $null
    foreach ($line in $lines) {
        if ($line -match '^\s*-\s+id:\s*(D\d{4})\s*$') {
            if ($null -ne $cur) { $entries += $cur }
            $cur = [pscustomobject]@{ Id = $Matches[1]; Status = ''; Case = ''; Level = '' }
            continue
        }
        if ($null -eq $cur) { continue }
        if ($line -match '^\s+status:\s*(\S+)\s*$') { $cur.Status = $Matches[1] }
        elseif ($line -match '^\s+case:\s*(C\d{4})\s*$') { $cur.Case = $Matches[1] }
        elseif ($line -match '^\s+level:\s*(\S+)\s*$') { $cur.Level = $Matches[1] }
    }
    if ($null -ne $cur) { $entries += $cur }

    $caseFiles = @(Get-ChildItem -LiteralPath $casesDir -File -Filter 'C*.ps1' -ErrorAction SilentlyContinue)
    $caseIds = @()
    foreach ($f in $caseFiles) {
        if ($f.Name -match '^(C\d{4})-') {
            $cid = $Matches[1]
            $caseIds += $cid
            # 用例头必须声明级别，且与 DEFECTS.yaml 一致（防「登记为 fast、实际要真机」）。
            $head = Get-Content -LiteralPath $f.FullName -TotalCount 5 -Encoding utf8
            $declared = ''
            foreach ($h in $head) { if ($h -match '^#\s*CASE-LEVEL:\s*(\S+)') { $declared = $Matches[1] } }
            $reg = $entries | Where-Object { $_.Case -eq $cid } | Select-Object -First 1
            if (-not $declared) { $problems += ("{0}: 缺 '# CASE-LEVEL:' 头" -f $f.Name) }
            elseif ($reg -and $reg.Level -ne $declared) { $problems += ("{0}: CASE-LEVEL={1} 与 DEFECTS.yaml level={2} 不一致" -f $f.Name, $declared, $reg.Level) }
        }
        else {
            $problems += ("{0}: 文件名必须是 C####-<slug>.ps1" -f $f.Name)
        }
        # 用例里出现 tmp/ 或 tmp\ ⇒ 该资产在下一台机器上不存在（RC1）。
        $text = Get-Content -LiteralPath $f.FullName -Raw -Encoding utf8
        $tmpHit = [regex]::Matches($text, 'tmp[/\\]')
        if ($tmpHit.Count -gt 0) { $problems += ("{0}: 引用了 tmp/ 路径（资产会随 git-ignore 丢失）" -f $f.Name) }
    }

    # 注意：这里**不能**再出现 `$problems = @()` —— 那会把上面扫用例收集到的问题全部丢掉，
    # 让本检查变成恒绿假牙（本轮实测踩过）。
    foreach ($e in $entries) {
        if ($e.Status -eq 'guarded') {
            if (-not $e.Case) { $problems += ("{0}: status=guarded 但没有 guard.case" -f $e.Id) }
            elseif ($caseIds -notcontains $e.Case) { $problems += ("{0}: guard.case={1} 在 cases/ 下不存在" -f $e.Id, $e.Case) }
        }
    }
    foreach ($cid in $caseIds) {
        if (-not ($entries | Where-Object { $_.Case -eq $cid })) { $problems += ("{0}: 用例没有被 DEFECTS.yaml 任何条目引用（孤儿用例）" -f $cid) }
    }
    if ($caseIds.Count -eq 0) { $problems += 'cases/ 下没有任何用例' }
    if ($entries.Count -eq 0) { $problems += 'DEFECTS.yaml 里没有任何条目' }

    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') }
    return New-CheckResult $true ("缺陷={0} 用例={1} 双向一致，无 tmp/ 引用" -f $entries.Count, $caseIds.Count)
}

# ---------------------------------------------------------------------------
# rc 对话框模板解析：把每个 IDD_* DIALOGEX 块切成 { Id, Lines }
#
# rc 是 UTF-16LE + BOM：必须按字节判 BOM 再用 Unicode 解码，
# 用默认编码读会得到乱码（而且「读出来像乱码」很容易被误当成「模板里就是乱码」）。
# ---------------------------------------------------------------------------
function Get-RcDialogBlocks {
    param([Parameter(Mandatory = $true)][string]$RcPath)

    if (-not (Test-Path -LiteralPath $RcPath)) {
        return [pscustomobject]@{ Ok = $false; Detail = "缺 rc 文件：$RcPath"; Blocks = @() }
    }
    $bytes = [System.IO.File]::ReadAllBytes($RcPath)
    if ($bytes.Length -lt 2 -or $bytes[0] -ne 0xFF -or $bytes[1] -ne 0xFE) {
        return [pscustomobject]@{ Ok = $false; Detail = "$RcPath 不是 UTF-16LE + BOM"; Blocks = @() }
    }
    $rcText = [System.Text.Encoding]::Unicode.GetString($bytes, 2, $bytes.Length - 2)

    $blocks = @()
    $curId = ''
    $cur = @()
    foreach ($ln in ($rcText -split "`r`n")) {
        if ($ln -match '^\s*(IDD_\w+)\s+DIALOGEX') {
            if ($curId -ne '') { $blocks += [pscustomobject]@{ Id = $curId; Lines = $cur } }
            $curId = $Matches[1]
            $cur = @()
            continue
        }
        if ($curId -eq '') { continue }
        if ($ln -match '^\s*END\s*$') {
            $blocks += [pscustomobject]@{ Id = $curId; Lines = $cur }
            $curId = ''
            $cur = @()
            continue
        }
        $cur += $ln
    }
    if ($curId -ne '') { $blocks += [pscustomobject]@{ Id = $curId; Lines = $cur } }
    return [pscustomobject]@{ Ok = $true; Detail = ''; Blocks = $blocks }
}

# ---------------------------------------------------------------------------
# Bug #3 的防回归齿：模板有「取消」按钮 ⇄ 消息映射必须处理 IDCANCEL
#
# WTL 的 CDialogImpl 不会自动处理 IDCANCEL：模板里的「取消」按钮与 Esc 键
# 都发 WM_COMMAND/IDCANCEL，消息映射里没有对应 handler 就没人 EndDialog，
# 窗口关不掉（Bug #3 的根因）。它依赖消息循环，单测测不出来，只能静态扫。
#
# 判据失效也要判红：rc 里一个取消按钮都找不到、或者取消按钮对不上任何
# 对话框类，都说明这条检查已经没在管事 —— 恒绿比漏检更危险。
# ---------------------------------------------------------------------------
function Test-DialogCancelHandlerCoverage {
    param(
        [Parameter(Mandatory = $true)][string]$DeployerDir,
        [Parameter(Mandatory = $true)][string]$RcPath
    )

    if (-not (Test-Path -LiteralPath $DeployerDir)) { return New-CheckResult $false "缺目录：$DeployerDir" }

    $parsed = Get-RcDialogBlocks -RcPath $RcPath
    if (-not $parsed.Ok) { return New-CheckResult $false $parsed.Detail }

    # 1) 哪些模板带了 IDCANCEL 按钮
    $cancelIds = @()
    foreach ($b in $parsed.Blocks) {
        foreach ($ln in $b.Lines) {
            if ($ln -match '^\s*(?:PUSHBUTTON|DEFPUSHBUTTON)\b[^,]*,\s*IDCANCEL\s*,') {
                $cancelIds += $b.Id
                break
            }
        }
    }
    $cancelIds = @($cancelIds | Select-Object -Unique)
    if ($cancelIds.Count -eq 0) { return New-CheckResult $false 'rc 里找不到任何带 IDCANCEL 按钮的对话框 ⇒ 判据失效' }

    # 2) 头文件里「资源 id → 类 → 消息映射」
    $classes = @{}
    foreach ($h in @(Get-ChildItem -LiteralPath $DeployerDir -Recurse -File -Filter '*.h' -ErrorAction SilentlyContinue)) {
        $text = [System.IO.File]::ReadAllText($h.FullName)
        foreach ($m in [regex]::Matches($text, 'class\s+(\w+)\s*:\s*public\s+CDialogImpl<\s*\1\s*>')) {
            $cls = $m.Groups[1].Value
            $tail = $text.Substring($m.Index)
            $idm = [regex]::Match($tail, 'enum\s*\{\s*IDD\s*=\s*(IDD_\w+)\s*\}')
            if (-not $idm.Success) { continue }
            $mapm = [regex]::Match($tail, 'BEGIN_MSG_MAP[\s\S]*?END_MSG_MAP')
            $map = ''
            if ($mapm.Success) { $map = $mapm.Value }
            $classes[$idm.Groups[1].Value] = [pscustomobject]@{ Class = $cls; File = $h.Name; Map = $map }
        }
    }

    $problems = @()
    $checked = 0
    foreach ($rid in $cancelIds) {
        if (-not $classes.ContainsKey($rid)) { continue }
        $checked++
        $c = $classes[$rid]
        if ($c.Map -notmatch 'IDCANCEL') {
            $problems += ("{0}：模板有 IDCANCEL 按钮，但 {1} 的消息映射没有 IDCANCEL handler ⇒ 点「取消」/按 Esc 关不掉窗口" -f $rid, $c.Class)
        }
    }
    if ($checked -eq 0) { return New-CheckResult $false 'rc 里的取消按钮没有对上任何对话框类 ⇒ 判据失效' }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') }
    return New-CheckResult $true ("带取消按钮的对话框 {0} 个，消息映射均已处理 IDCANCEL" -f $checked)
}

# ---------------------------------------------------------------------------
# P2 的防回归齿：fork 自有对话框模板的用户可见文案必须是简体中文
#
# constitution P2：「用户可见错误消息保持中文（简体）」。
# 只对 fork 自有对话框生效（上游 weasel 的 en-US 语言块是上游本地化资源，
# 不在 P2 的整改范围内），避免误伤。
# ---------------------------------------------------------------------------
function Test-DialogTemplateChinese {
    param(
        [Parameter(Mandatory = $true)][string]$RcPath,
        [Parameter(Mandatory = $true)][string]$DialogId
    )

    $parsed = Get-RcDialogBlocks -RcPath $RcPath
    if (-not $parsed.Ok) { return New-CheckResult $false $parsed.Detail }

    $targets = @($parsed.Blocks | Where-Object { $_.Id -eq $DialogId })
    if ($targets.Count -eq 0) { return New-CheckResult $false "rc 里找不到 $DialogId 模板 ⇒ 判据失效" }

    $problems = @()
    $literals = 0
    foreach ($b in $targets) {
        foreach ($ln in $b.Lines) {
            if ($ln -notmatch '^\s*(LTEXT|RTEXT|CTEXT|PUSHBUTTON|DEFPUSHBUTTON|GROUPBOX|CHECKBOX|RADIOBUTTON|CAPTION)\b') { continue }
            $m = [regex]::Match($ln, '"([^"]*)"')
            if (-not $m.Success) { continue }
            $lit = $m.Groups[1].Value
            if ($lit -eq '') { continue }
            $literals++
            # 去掉快捷键助记符（&I 之类）再判拉丁字母，否则 "开始导入(&I)" 会被误判成英文。
            $probe = $lit -replace '&[A-Za-z0-9]', ''
            if ($probe -match '[A-Za-z]') {
                $problems += ("{0}：用户可见文案含英文 —— '{1}'" -f $DialogId, $lit)
            }
        }
    }
    if ($literals -eq 0) { return New-CheckResult $false "$DialogId 模板里没解析出任何用户可见文案 ⇒ 判据失效" }
    if ($problems.Count -gt 0) { return New-CheckResult $false ($problems -join '; ') }
    return New-CheckResult $true ("{0} 的 {1} 条用户可见文案均为简体中文" -f $DialogId, $literals)
}
