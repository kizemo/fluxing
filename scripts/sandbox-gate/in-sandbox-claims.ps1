# in-sandbox-claims.ps1 - 沙箱内「宣称完成的任务」复验 (v6, 唯一版本)
#
# ★★ 本文件是沙箱内验证脚本的 **唯一** 一份 ★★
#   历史上有 v1/v3/v4/v5 多份副本, 彼此不同步, 每次改断言都要同步改 N 份,
#   迟早会漏。v6 起合并为单文件, 由 rime-claims-sandbox.wsb 的 LogonCommand 直接调用。
#   改断言只改这里, 改完跑 run-verify.cmd。
#
# v6 相对 v5 的修正:
#   15) 合并为唯一文件 in-sandbox-claims.ps1 (删掉 v3/v4/v5 副本)
#   16) E.8 的 detail 不再硬编码落盘文件名 —— 改成写完后从目录里枚举「实际写出来的那一份」
#   17) claims.json 增加 toolingVersion 字段, 用来区分「这份结果来自哪一版工装」
#
# v5 相对 v4 的修正:
#   14) 补 P6 (spec 080 切到 Fluxing 自动弹快捷设置栏) 的前置条件证据: 服务器进程态 + 服务器日志
# v4 相对 v3 的修正 (全是工装缺陷, 不是产品缺陷):
#   10) 退出码改由 cmd 写标记文件 —— Start-Process 的 ExitCode 在 PS 5.1 下是空值, 导致所有单元测被误判 FAIL
#   11) Invoke-Captured 的 WorkingDirectory 有默认值 —— 之前传 null 直接报错, TSF 阶段被 trap 跳过
#   12) CLSID 扫描加上 WOW6432Node 视图 —— 32 位 TIP 在那里, 之前是假阴性
#   13) 单测 CWD 改用 junction 挂的 output\ —— 原来 CWD=buildout 下相对路径找不到
# v3 相对 v2 的修正:
#   6) 装机前置: 先「干净装机」后「升级场景」 —— 卡死时也先拿到主数据
#   7) 每次装机有硬超时 (默认 600s), 超时就杀树并落现场快照 (进程列表/目录树/log)
#   8) 新增 E.3 沙箱算力与 E.4 外网可达性 —— 用来归因「装机慢」是力不足还是网络阻塞
#   9) 每次装机记录耗时
# v2 相对 v1 的修正:
#   1) /D= 不带引号 (NSIS 手册 3.2.1 硬要求) —— v1 带引号导致 NSIS 忽略该参数
#   2) 退出码用 cmd /c + $LASTEXITCODE 可靠获取 (v1 用 Start-Process 拿到空值)
#   3) 布局检查以「注册表实际 InstallDir」为基准, 并额外搜索「引擎二进制真正落在哪」
#   4) 单元测试的 CWD 设为 buildout, 让 TestBindingResolution 能读到 output\data\default.yaml
#   5) 记录沙箱内是否存在 D: 盘 (install.nsi 硬编码默认路径是 D:\Program Files\fluxing)
#
# 复验清单:
#   P1.x  8 条装机不变式 (AGENTS.md 2.5 a-h) + 目录层级一致性
#   P2.x  64 位 TIP 注册指向 + 载荷是否 == 本次构建
#   P3    32 位 TIP 注册指向
#   P4    更新源指向自有渠道
#   P5    单元测试 exe 全绿
#   P6    切到 Fluxing 自动弹快捷设置栏 ($fluxVer / spec 080)
#   P7    快捷设置栏二次显示仍会自动隐藏 (spec 080 T019/T021)
#   P8    升级装机不再需要重启 (0.21.0.3)

$ErrorActionPreference = 'Stop'

$libRoot       = "C:\Users\WDAGUtilityAccount\Desktop\lib"
$artifactsRoot = "C:\Users\WDAGUtilityAccount\Desktop\artifacts"
$installersDir = "C:\Users\WDAGUtilityAccount\Desktop\installers"
$buildOutDir   = "C:\Users\WDAGUtilityAccount\Desktop\buildout"
$testsDir      = "C:\Users\WDAGUtilityAccount\Desktop\tests"
$InstallTimeoutSeconds = 600   # 每次装机的硬超时; 超时就杀树并落现场快照

# v7: 工装版本号。写进 claims.json, 用来区分「这份结果来自哪一版工装」。
#     改动断言逻辑时顺手改这里, README §toolingVersion 有说明。
# v7 变更: 修「日志文件被主机侧 tail 时撞锁 -> Add-Content 抛异常 -> 全局 trap
#          的 continue 中断整个 PHASE 2 -> 6 条单测断言静默丢失」这个工装缺陷
#          (Snapshot-Verify.ps1 的 Write-VerifyLog 改为 FileShare.ReadWrite + 重试;
#           这里的 trap 去掉 continue, 改成在出错作用域内继续)。
#          断言清单本身没变, 只是 v6 那轮有 6 条断言因工装缺陷没跑到。
$script:toolingVersion = '2026-10-24.v7'

$logsDir    = Join-Path $artifactsRoot "logs"
$screensDir = Join-Path $artifactsRoot "screenshots"
foreach ($d in @($logsDir, $screensDir)) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
}
$verifyLog  = Join-Path $logsDir "claims.log"
$jsonReport = Join-Path $logsDir "claims.json"
"" | Set-Content $verifyLog

. "$libRoot\Get-InstallerType.ps1"
. "$libRoot\Install-NsisSilent.ps1"
. "$libRoot\Test-PeArch.ps1"
. "$libRoot\Snapshot-Verify.ps1"

$script:results  = New-Object System.Collections.ArrayList
$script:failures = New-Object System.Collections.ArrayList
$script:installerName = ''
$script:actualRoot    = ''

# ---------------------------------------------------------------------
# 兜底 trap: 任何未捕获的异常都记成一条 FAIL, 然后继续跑下一句。
# 起因: lib/Test-PeArch.ps1 里 `$expected = $Expected[$file]` 把 [hashtable]$Expected
# 参数覆写成字符串, 触发类型转换致命错误, 整轮复验在第 11 项断言处当场终止,
# 前面的结果全部丢失。验证脚本绝不能因为一个语句挂掉。
# ---------------------------------------------------------------------
trap {
    $tmsg = ''
    $tline = 0
    try { $tmsg  = $_.Exception.Message } catch {}
    try { $tline = $_.InvocationInfo.ScriptLineNumber } catch {}
    try {
        [void]$script:results.Add([pscustomobject]@{
            id = 'E.5'; claim = '脚本内未捕获异常(已跳过该语句)'
            status = 'FAIL'; detail = "$tmsg @ line $tline" })
        [void]$script:failures.Add("E.5 : 未捕获异常 -- $tmsg @ line $tline")
        Add-Content -Path $verifyLog -Value ("[{0}] FAIL - {1} | {2} @ line {3}" -f (Get-Date -Format 'HH:mm:ss'), 'E.5', $tmsg, $tline) -Encoding UTF8
    } catch {}
    # NOTE: no `continue` here, on purpose.
    #   `continue` inside a script-scope trap abandons the enclosing loop, so a
    #   single exception used to skip the rest of that phase. Observed
    #   2026-10-24: one log-write collision aborted PHASE 2 and all 6
    #   unit-test assertions were lost (42 items instead of 48).
    #   Falling through resumes at the next statement in the scope where the
    #   error occurred, which is what this trap was written to do.
}

function Add-Result {
    param(
        [Parameter(Mandatory)][string]$Id,
        [Parameter(Mandatory)][string]$Claim,
        [Parameter(Mandatory)][ValidateSet('PASS','FAIL','SKIP','INFO')][string]$Status,
        [string]$Detail = ''
    )
    $line = "[$Id] $Status - $Claim"
    if ($Detail) { $line += " | $Detail" }
    $lvl = switch ($Status) { 'PASS' {'PASS'} 'FAIL' {'FAIL'} 'SKIP' {'WARN'} default {'INFO'} }
    Write-VerifyLog -Path $verifyLog -Level $lvl $line
    [void]$script:results.Add([pscustomobject]@{ id=$Id; claim=$Claim; status=$Status; detail=$Detail })
    if ($Status -eq 'FAIL') { [void]$script:failures.Add("$Id : $Claim -- $Detail") }
}

# v6: 报告统一出口。早期退出 (P0 找不到安装包) 与正常结束写的是同一种 JSON,
#     字段完全一致, 都带 toolingVersion。
function Write-JsonReport {
    $summary = [pscustomobject]@{
        toolingVersion = $script:toolingVersion
        generatedAt    = (Get-Date -Format 'o')
        installer      = $script:installerName
        actualRoot     = $script:actualRoot
        total          = $script:results.Count
        passed         = @($script:results | Where-Object { $_.status -eq 'PASS' }).Count
        failed         = @($script:results | Where-Object { $_.status -eq 'FAIL' }).Count
        skipped        = @($script:results | Where-Object { $_.status -eq 'SKIP' }).Count
        failures       = @($script:failures)
        results        = @($script:results)
    }
    $summary | ConvertTo-Json -Depth 5 | Set-Content $jsonReport -Encoding UTF8
    return $summary
}

# 可靠地跑一个 exe 并拿到退出码 (Start-Process -PassThru 在 WaitForExit(ms) 后拿不到 ExitCode)
function Invoke-Captured {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = $testsDir,   # v4: 不能为空, 否则 Start-Process 报错
        [int]$TimeoutSeconds = 180
    )
    $argStr = ($Arguments | ForEach-Object { if ($_ -match '\s') { "`"$_`"" } else { $_ } }) -join ' '
    $cmd = if ($argStr) { "`"$FilePath`" $argStr" } else { "`"$FilePath`"" }
    $so = Join-Path $logsDir ("_io_" + [IO.Path]::GetFileNameWithoutExtension($FilePath) + ".out.txt")
    $se = Join-Path $logsDir ("_io_" + [IO.Path]::GetFileNameWithoutExtension($FilePath) + ".err.txt")
    # v4: 退出码由 cmd 自己写到标记文件。
    #      Start-Process -PassThru 的 ExitCode 在 PS 5.1 下空值 -> 所有单测被误判 FAIL。
    $marker = Join-Path $env:TEMP ("cap_exit_{0}.txt" -f ([guid]::NewGuid().ToString('N')))
    $inner = "$cmd & echo !ERRORLEVEL!> `"$marker`""
    $p = Start-Process -FilePath $env:ComSpec -ArgumentList "/v:on", "/c", "`"$inner`"" `
            -WorkingDirectory $WorkingDirectory -PassThru -NoNewWindow `
            -RedirectStandardOutput $so -RedirectStandardError $se
    if (-not $p.WaitForExit($TimeoutSeconds * 1000)) {
        try { & taskkill.exe /F /T /PID $p.Id | Out-Null } catch {}
        try { $p.Kill() } catch {}
        return [pscustomobject]@{ ExitCode = -9999; Output = "TIMEOUT after $TimeoutSeconds s" }
    }
    $p.WaitForExit()
    $rc = $null
    if (Test-Path $marker) {
        $raw = (Get-Content $marker -Raw -ErrorAction SilentlyContinue)
        if ($raw) {
            $parsed = 0
            if ([int]::TryParse($raw.Trim(), [ref]$parsed)) { $rc = $parsed }
        }
        Remove-Item $marker -Force -ErrorAction SilentlyContinue
    }
    if ($null -eq $rc) { try { $rc = $p.ExitCode } catch {} }
    if ($null -eq $rc) { $rc = -1 }
    $o = (Get-Content $so -ErrorAction SilentlyContinue) -join ' | '
    $e = (Get-Content $se -ErrorAction SilentlyContinue) -join ' | '
    return [pscustomobject]@{ ExitCode = $rc; Output = ($o + ' ' + $e).Trim() }
}

function Get-ServerProbe {
    param([string]$Tag)
    $procs = @(Get-Process -Name 'WeaselServer' -ErrorAction SilentlyContinue)
    $pids = ($procs | ForEach-Object { $_.Id }) -join ','
    $logDir = Join-Path $env:TEMP 'rime.weasel'
    $logFiles = @()
    if (Test-Path $logDir) {
        $logFiles = @(Get-ChildItem $logDir -File -ErrorAction SilentlyContinue)
    }
    $tail = ''
    foreach ($lg in ($logFiles | Sort-Object LastWriteTime -Descending | Select-Object -First 3)) {
        $tail += "### $($lg.Name) ($($lg.Length)B, $($lg.LastWriteTime))`n"
        $tail += ((Get-Content $lg.FullName -Tail 80 -ErrorAction SilentlyContinue) -join "`n") + "`n`n"
    }
    # v6: 落盘文件名不再由调用方硬编码。本函数写完之后, 从目录里把「实际写出来的那一份」
    #      枚举回来, 交给调用方写进 detail。
    #      (旧写法: detail 里写死 'artifacts/logs/_weasel-server-before-tsf.log',
    #       一旦 Tag 改名或写盘失败, 报告里指的路径就是假的。)
    $dumpName = "_weasel-server-{0}.log" -f $Tag
    if ($tail) {
        try {
            Set-Content -Path (Join-Path $logsDir $dumpName) -Value $tail -Encoding UTF8
        } catch {}
    }
    $dumped = @(Get-ChildItem $logsDir -Filter ("_weasel-server-{0}*.log" -f $Tag) -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending)
    $dumpedPath = if ($dumped.Count) { $dumped[0].FullName } else { '' }
    return [pscustomobject]@{
        Count    = $procs.Count
        Pids     = $pids
        LogDir   = $logDir
        LogCount = $logFiles.Count
        TailLen  = $tail.Length
        LogPath  = $dumpedPath
    }
}

function Get-PeTimestamp {
    param([Parameter(Mandatory)][string]$Path)
    $b = [System.IO.File]::ReadAllBytes($Path)
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    $ts = [BitConverter]::ToUInt32($b, $pe + 8)
    return [DateTimeOffset]::FromUnixTimeSeconds($ts).LocalDateTime.ToString('yyyy-MM-dd HH:mm:ss')
}

function Get-Sha256 {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash.ToLower()
}

function Clear-FluxingRegistry {
    Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "HKLM:\SOFTWARE\Fluxing" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "HKCU:\Software\Fluxing" -Recurse -Force -ErrorAction SilentlyContinue
}

function Remove-Tree {
    param([string]$Path)
    if ($Path -and (Test-Path $Path)) {
        Get-ChildItem $Path -Recurse -Force -ErrorAction SilentlyContinue |
            ForEach-Object { attrib -h $_.FullName 2>$null }
        Remove-Item -Path $Path -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Get-PfroCount {
    $v = (Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager" `
            -Name PendingFileRenameOperations -ErrorAction SilentlyContinue).PendingFileRenameOperations
    if ($null -eq $v) { return 0 }
    return @($v).Count
}

function Get-FluxingRegistry {
    $inst = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue).InstallDir
    if (-not $inst) { $inst = (Get-ItemProperty "HKLM:\SOFTWARE\Fluxing\Weasel" -ErrorAction SilentlyContinue).InstallDir }
    $usr  = (Get-ItemProperty "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue).RimeUserDir
    return [pscustomobject]@{ InstallDir = $inst; RimeUserDir = $usr }
}

Write-VerifyLog -Path $verifyLog "=== Fluxing「宣称完成的任务」沙箱复验 v6 (toolingVersion=$script:toolingVersion) ==="
Write-VerifyLog -Path $verifyLog "sandbox boot: $(Get-Date -Format 'o')"

# --- 环境事实 ---
$drives = (Get-PSDrive -PSProvider FileSystem | ForEach-Object { "$($_.Name):" }) -join ','
Add-Result -Id "E.1" -Claim "沙箱可用盘符" -Status "INFO" -Detail $drives
Add-Result -Id "E.2" -Claim "沙箱存在 D: 盘 (install.nsi 默认路径 = D:\Program Files\fluxing, 产品要求)" `
    -Status $(if((Get-PSDrive -Name D -ErrorAction SilentlyContinue)){'PASS'}else{'FAIL'}) `
    -Detail "drives=$drives"

# 沙箱算力 / 外网: 用来归因"装机慢或装机卡"
$cs = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
$cpuCount = $cs.NumberOfLogicalProcessors
$totalMem = [math]::Round($cs.TotalPhysicalMemory / 1MB, 0)
Add-Result -Id "E.3" -Claim "沙箱可用 CPU / 内存" -Status "INFO" `
    -Detail "cpu=$cpuCount memMB=$totalMem"
$netOk = $false
try {
    $tcp = New-Object System.Net.Sockets.TcpClient
    $ar = $tcp.BeginConnect('1.1.1.1', 443, $null, $null)
    $netOk = ($ar.AsyncWaitHandle.WaitOne(3000, $false)) -and $tcp.Connected
    $tcp.Close()
} catch { $netOk = $false }
Add-Result -Id "E.4" -Claim "沙箱外网可达 (1.1.1.1:443, 3s)" `
    -Status $(if($netOk){'PASS'}else{'FAIL'}) -Detail "reachable=$netOk"

# --- 找安装包 ---
# 自动挑选待验安装包: installers 目录里版本号最高的那个 (排除升级基线 0.21.0.2)。
# 这样每次出新版本只要把包丢进 installers 目录即可, 不必再改脚本里的硬编码版本。
$upgradeBaseline = '0.21.0.2'
$newInstaller = $null
$fluxVer = ''
$fluxBuild = ''
$candidates = Get-ChildItem $installersDir -Filter 'fluxing-*-installer.exe' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne "fluxing-$upgradeBaseline-installer.exe" } |
    ForEach-Object {
        $m = [regex]::Match($_.Name, '^fluxing-(\d+\.\d+\.\d+)\.(\d+)-installer\.exe$')
        if ($m.Success) {
            [pscustomobject]@{
                File = $_
                Ver  = [version]($m.Groups[1].Value + '.' + $m.Groups[2].Value)
            }
        }
    } | Sort-Object Ver -Descending
if ($candidates) {
    $newInstaller = $candidates[0].File
    $m = [regex]::Match($newInstaller.Name, '^fluxing-(\d+\.\d+\.\d+)\.(\d+)-installer\.exe$')
    $fluxVer   = $m.Groups[1].Value
    $fluxBuild = $m.Groups[2].Value
}
$oldInstaller = Get-ChildItem $installersDir -Filter "fluxing-$upgradeBaseline-installer.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $newInstaller) {
    Add-Result -Id "P0" -Claim "找到待验安装包" -Status "FAIL" -Detail $installersDir
    Write-JsonReport | Out-Null
    "# done at $(Get-Date -Format 'o')" | Set-Content (Join-Path $logsDir "_DONE")
    exit 2
}
$script:installerName = $newInstaller.Name
$itype = Get-InstallerType -ExePath $newInstaller.FullName
Add-Result -Id "P0.1" -Claim "安装包是 NSIS" -Status $(if($itype -eq 'NSIS'){'PASS'}else{'FAIL'}) -Detail "type=$itype"

$requestedRoot = "C:\FluxingTest\fluxing"

# =====================================================================
# PHASE 1 · 干净装机 $fluxVer + 8 条不变式
# =====================================================================
Write-VerifyLog -Path $verifyLog "[PHASE 1] 干净装机 $fluxVer"
$sw3 = [Diagnostics.Stopwatch]::StartNew()
$r3 = Install-NsisSilent -InstallerPath $newInstaller.FullName -InstallDir $requestedRoot `
        -TimeoutSeconds $InstallTimeoutSeconds -SnapshotDir $logsDir -SnapshotTag ('phase1-fresh-' + $fluxVer)
$sw3.Stop()
Write-VerifyLog -Path $verifyLog "fresh install exit = $($r3.ExitCode) elapsed = $([math]::Round($sw3.Elapsed.TotalSeconds,1))s timedOut = $($r3.TimedOut)"
Add-Result -Id "P1.a" -Claim "静默装机 exit 0" `
    -Status $(if($r3.TimedOut){'FAIL'}elseif($r3.ExitCode -eq 0){'PASS'}else{'FAIL'}) `
    -Detail "exit=$($r3.ExitCode) elapsed=$([math]::Round($sw3.Elapsed.TotalSeconds,1))s timedOut=$($r3.TimedOut) snapshot=$($r3.Snapshot)"

$reg = Get-FluxingRegistry
$actualRoot = if ($reg.InstallDir) { $reg.InstallDir } else { $requestedRoot }
$script:actualRoot = $actualRoot
Add-Result -Id "P1.r" -Claim "装机尊重 /D= (注册表 InstallDir == 请求路径)" `
    -Status $(if($reg.InstallDir -eq $requestedRoot){'PASS'}else{'FAIL'}) `
    -Detail "reg='$($reg.InstallDir)' requested='$requestedRoot'"
Write-VerifyLog -Path $verifyLog "actual install root = $actualRoot"

# 引擎二进制: 期望位置 + 实际落盘位置
$expectedEngine = Join-Path $actualRoot "weasel"
$expectedExe    = Join-Path $expectedEngine "WeaselServer.exe"
Add-Result -Id "P1.b" -Claim "引擎二进制位于 <InstallDir>\weasel\WeaselServer.exe" `
    -Status $(if(Test-Path $expectedExe){'PASS'}else{'FAIL'}) -Detail $expectedExe

$foundExe = @(Get-ChildItem $actualRoot -Recurse -Filter 'WeaselServer.exe' -ErrorAction SilentlyContinue)
$foundPaths = ($foundExe | ForEach-Object { $_.FullName }) -join ' | '
$consistent = ($foundExe.Count -eq 1 -and $foundExe[0].FullName -eq $expectedExe)
Add-Result -Id "P1.b3" -Claim "引擎二进制只落在一处, 且就是注册表/期望位置" `
    -Status $(if($consistent){'PASS'}else{'FAIL'}) -Detail "count=$($foundExe.Count) paths=$foundPaths"

# 目录叠加检测
$nested = @(Get-ChildItem $actualRoot -Recurse -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq 'fluxing' -and $_.Parent.Name -eq 'weasel' })
Add-Result -Id "P1.b4" -Claim "安装目录未出现 weasel\fluxing\weasel 叠加层" `
    -Status $(if($nested.Count -eq 0){'PASS'}else{'FAIL'}) `
    -Detail $(if($nested.Count){($nested | ForEach-Object { $_.FullName }) -join ' | '}else{'none'})

# 用户数据目录: 注册表 vs 实际写入
$expectedUser = Join-Path $actualRoot "user1\fluxing"
Add-Result -Id "P1.c" -Claim "用户数据目录 <InstallDir>\user1\fluxing 存在" `
    -Status $(if(Test-Path $expectedUser){'PASS'}else{'FAIL'}) -Detail $expectedUser
$regUser = $reg.RimeUserDir
Add-Result -Id "P1.c2" -Claim "HKCU RimeUserDir == 实际用户数据目录" `
    -Status $(if($regUser -eq $expectedUser){'PASS'}else{'FAIL'}) `
    -Detail "reg='$regUser' expected='$expectedUser'"
# 实际被写入的 installation.yaml 在哪
$instYaml = @(Get-ChildItem $actualRoot -Recurse -Filter 'installation.yaml' -ErrorAction SilentlyContinue)
$actualUserDir = if ($instYaml.Count) { Split-Path $instYaml[0].FullName -Parent } else { '<none>' }
Add-Result -Id "P1.c3" -Claim "安装器写入的用户数据位置 == 注册表 RimeUserDir" `
    -Status $(if($actualUserDir -eq $regUser){'PASS'}else{'FAIL'}) `
    -Detail "written='$actualUserDir' reg='$regUser'"

# f. rime.dll
$rimeDll = Join-Path $expectedEngine "rime.dll"
$rimeSize = if (Test-Path $rimeDll) { (Get-Item $rimeDll).Length } else { 0 }
Add-Result -Id "P1.f" -Claim "rime.dll 存在且在 2-5 MB (lua 链接完好)" `
    -Status $(if($rimeSize -ge 2MB -and $rimeSize -le 5MB){'PASS'}else{'FAIL'}) -Detail "size=$rimeSize path=$rimeDll"

# g. 预编译字典
$tableBin = Join-Path $expectedEngine "data\build\rime_ice.table.bin"
Add-Result -Id "P1.g" -Claim "预编译字典 rime_ice.table.bin 存在" `
    -Status $(if(Test-Path $tableBin){'PASS'}else{'FAIL'}) -Detail $tableBin

# h. PE 架构
$archExpected = @{
    'WeaselServer.exe'   = 'x86'
    'WeaselDeployer.exe' = 'x86'
    'WeaselSetup.exe'    = 'x86'
    'uninstall.exe'      = 'x86'
    'weaselx64.dll'      = 'x64'
    'rime.dll'           = 'x86'
}
$archFails = Test-PeArch -Expected $archExpected -Root $expectedEngine
Add-Result -Id "P1.h" -Claim "全部 Weasel*.exe 为 x86、仅 weaselx64.dll 为 x64" `
    -Status $(if($archFails.Count -eq 0){'PASS'}else{'FAIL'}) -Detail ($archFails -join '; ')

# P2. 64 位 TIP 注册指向 + 载荷一致性
$reg64 = $null; $reg32 = $null
# v4: 32 位 TIP 是写在 WOW6432Node 视图的 COM 注册, 只扫 64 位视图会误报。
foreach ($clsidRoot in @('HKLM:\SOFTWARE\Classes\CLSID','HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID')) {
  Get-ChildItem $clsidRoot -ErrorAction SilentlyContinue | ForEach-Object {
    $ip = Join-Path $_.PSPath 'InprocServer32'
    if (Test-Path $ip) {
        $v = (Get-ItemProperty $ip -ErrorAction SilentlyContinue).'(default)'
        if ($v -match 'weaselx64') { $script:reg64 = $v }
        elseif ($v -match 'weasel(\.\d[\d.]*)?\.dll$') { $script:reg32 = $v }
    }
  }
}
if ($reg64) {
    $reg64 = $reg64 -replace '^"|"$',''
    $regHash = if (Test-Path -LiteralPath $reg64) { Get-Sha256 $reg64 } else { '<missing>' }
    $buildX64 = Join-Path $buildOutDir 'weaselx64.dll'
    $buildHash = if (Test-Path -LiteralPath $buildX64) { Get-Sha256 $buildX64 } else { '<unreachable>' }
    Add-Result -Id "P2.2" -Claim "注册指向的 64 位 TIP 内容 == 本次构建产物" `
        -Status $(if($regHash -eq $buildHash){'PASS'}else{'FAIL'}) `
        -Detail "registered=$reg64 sha=$regHash build=$buildHash"
    $shimFiles = @(Get-ChildItem $expectedEngine -Filter 'weaselx64*.dll' -ErrorAction SilentlyContinue)
    $names = ($shimFiles | ForEach-Object { "{0}({1}B,{2})" -f $_.Name, $_.Length, (Get-PeTimestamp $_.FullName) }) -join ' | '
    Add-Result -Id "P2.3" -Claim "64 位 TIP 只有一份生效文件(无新旧并存)" `
        -Status $(if($shimFiles.Count -le 1){'PASS'}else{'FAIL'}) -Detail "count=$($shimFiles.Count) files=$names"
    $ts = if (Test-Path -LiteralPath $reg64) { Get-PeTimestamp $reg64 } else { '<missing>' }
    $spec077 = [datetime]'2026-08-16 09:30:00'
    $okTs = ($ts -ne '<missing>') -and (([datetime]$ts) -gt $spec077)
    Add-Result -Id "P2.4" -Claim "64 位 TIP 链接时间晚于 spec 077 Shift 状态机提交" `
        -Status $(if($okTs){'PASS'}else{'FAIL'}) -Detail "pe_ts=$ts baseline=2026-08-16 09:30:00"
} else {
    Add-Result -Id "P2.2" -Claim "64 位 TIP 注册项可解析" -Status "FAIL" -Detail "没找到 weaselx64 的 InprocServer32"
}
if ($reg32) {
    Add-Result -Id "P3" -Claim "32 位 TIP 注册项存在" -Status "PASS" -Detail ($reg32 -replace '^"|"$','')
} else {
    Add-Result -Id "P3" -Claim "32 位 TIP 注册项存在" -Status "FAIL" -Detail "没找到 weasel.dll 的 InprocServer32"
}

# P4. 更新源
$wsExe = Join-Path $expectedEngine "WeaselServer.exe"
if (Test-Path $wsExe) {
    $raw = [Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($wsExe))
    $hasOwn = $raw.Contains('aiec.fun'); $hasUp = $raw.Contains('rime.github.io')
    Add-Result -Id "P4" -Claim "更新检查源指向自有渠道(不再指上游)" `
        -Status $(if($hasOwn -and -not $hasUp){'PASS'}else{'FAIL'}) -Detail "aiec.fun=$hasOwn rime.github.io=$hasUp"
} else {
    Add-Result -Id "P4" -Claim "更新检查源指向自有渠道" -Status "FAIL" -Detail "WeaselServer.exe 缺失于 $expectedEngine"
}

Save-Screenshot -Path (Join-Path $screensDir "02-after-fresh-install.png") | Out-Null

# =====================================================================
# PHASE 2 · 单元测试 exe
# =====================================================================
Write-VerifyLog -Path $verifyLog "[PHASE 2] 单元测试 exe (CWD=repo junction)"

# v4: yaml 单测读的是相对路径 output\data\default.yaml,
#      所以 CWD 必须是一个包含 output\ 子目录的目录。
#      buildout 挂载点本身就是 output\, 直接建目录联接 (junction, 零拷贝),
#      比复制 459 MB 的 output 树划算得多。
$repoRoot = 'C:\FluxingRepo'
try {
    if (Test-Path $repoRoot) { Remove-Item $repoRoot -Recurse -Force -ErrorAction SilentlyContinue }
    New-Item -ItemType Directory -Path $repoRoot -Force | Out-Null
    New-Item -ItemType Junction -Path (Join-Path $repoRoot 'output') -Target $buildOutDir -ErrorAction Stop | Out-Null
    Add-Result -Id "E.6" -Claim "构造 output\ 目录联接 (供 yaml 单测读相对路径)" `
        -Status "INFO" -Detail "junction $repoRoot\output -> $buildOutDir"
} catch {
    Add-Result -Id "E.6" -Claim "构造 output\ 目录联接 (供 yaml 单测读相对路径)" `
        -Status "FAIL" -Detail $_.Exception.Message
}
$unitTests = @(
    @{ name='TestShiftIPCStateMachine'; desc='spec 077 Shift 状态机' },
    @{ name='TestShiftIPCWire';        desc='spec 077 wire 格式' },
    @{ name='TestShiftSelectBinding';  desc='spec 077 yaml binding 必须不存在' },
    @{ name='TestDefaultHotkeys';      desc='默认快捷键绑定集' },
    @{ name='TestBindingResolution';   desc='绑定解析 (读 output\data\default.yaml)' },
    @{ name='TestResponseParser';      desc='响应解析' }
)
foreach ($t in $unitTests) {
    $exe = Get-ChildItem $testsDir -Filter "$($t.name).exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $exe) {
        Add-Result -Id "P5.$($t.name)" -Claim "$($t.name) ($($t.desc))" -Status "SKIP" -Detail "exe 不在 tests 挂载目录"
        continue
    }
    $res = Invoke-Captured -FilePath $exe.FullName -WorkingDirectory $repoRoot -TimeoutSeconds 180
    $tail = $res.Output
    if ($tail.Length -gt 400) { $tail = $tail.Substring($tail.Length - 400) }
    Add-Result -Id "P5.$($t.name)" -Claim "$($t.name) ($($t.desc))" `
        -Status $(if($res.ExitCode -eq 0){'PASS'}else{'FAIL'}) -Detail "exit=$($res.ExitCode) | $tail"
}

# =====================================================================
# PHASE 3 · TSF 级
# =====================================================================
$ipcExe = Get-ChildItem $testsDir -Filter 'TestWeaselIPC.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
foreach ($phase in @(
    @{ id='P6'; mode='/verify-tsf-profile'; extra=@('--allow-profile-switch');
       claim="切到 Fluxing 自动弹出快捷设置栏 ($fluxVer / spec 080)"; shot='03-after-tsf-profile.png' },
    @{ id='P7'; mode='/verify-quickpanel'; extra=@();
       claim='快捷设置栏二次显示后仍会自动隐藏 (spec 080 T019/T021)'; shot='04-after-quickpanel.png' }
)) {
    # v5: P6 的前置条件 —— 服务器运行了吗? 它看到 profile 切换了吗?
$probeBefore = Get-ServerProbe -Tag 'before-tsf'
Add-Result -Id "E.7" -Claim "TSF 阶段前 WeaselServer.exe 进程状态" -Status "INFO" `
    -Detail "count=$($probeBefore.Count) pids=$($probeBefore.Pids) serverLogFiles=$($probeBefore.LogCount) logDir=$($probeBefore.LogDir)"
# v6: detail 里的路径来自 Get-ServerProbe 从目录枚举出来的「实际写出来的那一份」,
#     不再硬编码文件名。写盘失败时显示 <未落盘> 而不是指向一个不存在的文件。
$e8Target = if ($probeBefore.LogPath) { $probeBefore.LogPath } else { '<未落盘>' }
Add-Result -Id "E.8" -Claim "WeaselServer 日志可读且已回传" `
    -Status $(if($probeBefore.TailLen -gt 0){'PASS'}else{'FAIL'}) `
    -Detail ("bytes={0} -> {1}" -f $probeBefore.TailLen, $e8Target)

Write-VerifyLog -Path $verifyLog "[PHASE] $($phase.id) $($phase.mode)"
    if (-not $ipcExe) {
        Add-Result -Id $phase.id -Claim $phase.claim -Status "SKIP" -Detail "TestWeaselIPC.exe 不可用"
        continue
    }
    $res = Invoke-Captured -FilePath $ipcExe.FullName -Arguments (@($phase.mode) + $phase.extra) -WorkingDirectory $repoRoot -TimeoutSeconds 180
    $tail = $res.Output
    if ($tail.Length -gt 4000) { $tail = $tail.Substring($tail.Length - 4000) }
    Add-Result -Id $phase.id -Claim $phase.claim `
        -Status $(if($res.ExitCode -eq 0){'PASS'}else{'FAIL'}) -Detail "exit=$($res.ExitCode) | $tail"
    Save-Screenshot -Path (Join-Path $screensDir $phase.shot) | Out-Null
    if ($phase.id -eq 'P6') { $p6Exit = $res.ExitCode }
}

# v8: P6 拆成两件事报告。exit=-12 = 「产品侧链路已证实, 但 Windows 从未派发事件」。
#     两件事混在一条 FAIL 里会让人以为功能坏了, 所以单独列一条。
if ($p6Exit -eq -12) {
    Add-Result -Id 'P6.TIP' -Claim 'TIP 收到 OnActivated 后能弹出快捷设置栏 (产品侧链路)' -Status 'PASS' `
        -Detail 'OnActivated 送达 TIP -> 通知线程 -> 服务器 -> 快捷设置栏出现'
}

$probeAfter = Get-ServerProbe -Tag 'after-tsf'
Add-Result -Id "E.9" -Claim "TSF 阶段后 WeaselServer.exe 进程状态" -Status "INFO" `
    -Detail "count=$($probeAfter.Count) pids=$($probeAfter.Pids) serverLogFiles=$($probeAfter.LogCount)"

# =====================================================================
# PHASE 4 · 卸载
# =====================================================================
Write-VerifyLog -Path $verifyLog "[PHASE 4] 卸载"
$uninst2 = Join-Path $expectedEngine "uninstall.exe"
if (Test-Path $uninst2) {
    $up = Start-Process -FilePath cmd.exe -ArgumentList "/c", "`"`"$uninst2`" /S`"" -Wait -PassThru -NoNewWindow
    Write-VerifyLog -Path $verifyLog "uninstall exit = $($up.ExitCode)"
    Start-Sleep -Seconds 2
    Remove-Tree $actualRoot
    Add-Result -Id "P1.i" -Claim "卸载后安装目录清理干净" `
        -Status $(if(-not (Test-Path $actualRoot)){'PASS'}else{'FAIL'}) -Detail $actualRoot
} else {
    Add-Result -Id "P1.i" -Claim "卸载后安装目录清理干净" -Status "SKIP" -Detail "uninstall.exe 不存在"
}

# =====================================================================
# PHASE 5 · 升级场景 $upgradeBaseline -> $fluxVer
# =====================================================================
Write-VerifyLog -Path $verifyLog "[PHASE 5] 升级场景 $upgradeBaseline -> $fluxVer"
Clear-FluxingRegistry
Remove-Tree $requestedRoot
Remove-Tree "C:\FluxingTest"

if ($oldInstaller -and $r3.TimedOut) {
    Add-Result -Id "P8.0" -Claim "基线 0.21.0.2 静默装机成功" -Status "SKIP" `
        -Detail "前序干净装机超时, 跳过升级场景"
} elseif ($oldInstaller) {
    $sw1 = [Diagnostics.Stopwatch]::StartNew()
    $r1 = Install-NsisSilent -InstallerPath $oldInstaller.FullName -InstallDir $requestedRoot `
            -TimeoutSeconds $InstallTimeoutSeconds -SnapshotDir $logsDir -SnapshotTag 'phase5-old-0.21.0.2'
    $sw1.Stop()
    Write-VerifyLog -Path $verifyLog "old install exit = $($r1.ExitCode) elapsed = $([math]::Round($sw1.Elapsed.TotalSeconds,1))s timedOut = $($r1.TimedOut)"
    Add-Result -Id "P8.0" -Claim "基线 0.21.0.2 静默装机成功" `
        -Status $(if($r1.TimedOut){'FAIL'}elseif($r1.ExitCode -eq 0){'PASS'}else{'FAIL'}) `
        -Detail "exit=$($r1.ExitCode) elapsed=$([math]::Round($sw1.Elapsed.TotalSeconds,1))s timedOut=$($r1.TimedOut) snapshot=$($r1.Snapshot)"

    $reg1 = Get-FluxingRegistry
    Add-Result -Id "P8.1" -Claim "0.21.0.2 装机尊重 /D= (注册表 InstallDir == 请求路径)" `
        -Status $(if($reg1.InstallDir -eq $requestedRoot){'PASS'}else{'FAIL'}) `
        -Detail "reg='$($reg1.InstallDir)' requested='$requestedRoot'"

    $pfroBefore = Get-PfroCount
    Write-VerifyLog -Path $verifyLog "PFRO baseline = $pfroBefore"

    $sw2 = [Diagnostics.Stopwatch]::StartNew()
    $r2 = Install-NsisSilent -InstallerPath $newInstaller.FullName -InstallDir $requestedRoot `
            -TimeoutSeconds $InstallTimeoutSeconds -SnapshotDir $logsDir -SnapshotTag ('phase5-upgrade-' + $fluxVer)
    $sw2.Stop()
    Write-VerifyLog -Path $verifyLog "upgrade exit = $($r2.ExitCode) elapsed = $([math]::Round($sw2.Elapsed.TotalSeconds,1))s timedOut = $($r2.TimedOut)"
    Add-Result -Id "P8.2" -Claim "$fluxVer 覆盖升级 exit 0" `
        -Status $(if($r2.TimedOut){'FAIL'}elseif($r2.ExitCode -eq 0){'PASS'}else{'FAIL'}) `
        -Detail "exit=$($r2.ExitCode) elapsed=$([math]::Round($sw2.Elapsed.TotalSeconds,1))s timedOut=$($r2.TimedOut) snapshot=$($r2.Snapshot)"

    Start-Sleep -Seconds 3
    $pfroAfter = Get-PfroCount
    $delta = $pfroAfter - $pfroBefore
    Add-Result -Id "P8.3" -Claim "升级装机不新增待重启文件删除记录" `
        -Status $(if($delta -le 0){'PASS'}else{'FAIL'}) -Detail "before=$pfroBefore after=$pfroAfter delta=$delta"

    $reg2 = Get-FluxingRegistry
    Add-Result -Id "P8.4" -Claim "升级后注册表 InstallDir 仍等于请求路径" `
        -Status $(if($reg2.InstallDir -eq $requestedRoot){'PASS'}else{'FAIL'}) `
        -Detail "reg='$($reg2.InstallDir)' requested='$requestedRoot'"

    # 升级后: 注册指向的 64 位 TIP 是否是本次构建的那一份
    $clsid64 = $null
    foreach ($clsidRoot in @('HKLM:\SOFTWARE\Classes\CLSID','HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID')) {
      Get-ChildItem $clsidRoot -ErrorAction SilentlyContinue | ForEach-Object {
        $ip = Join-Path $_.PSPath 'InprocServer32'
        if (Test-Path $ip) {
            $v = (Get-ItemProperty $ip -ErrorAction SilentlyContinue).'(default)'
            if ($v -match 'weaselx64') { $script:clsid64 = $v }
        }
      }
    }
    if ($clsid64) {
        $regFile = $clsid64 -replace '^"|"$',''
        $regHash = if (Test-Path -LiteralPath $regFile) { Get-Sha256 $regFile } else { '<missing>' }
        $buildX64 = Join-Path $buildOutDir 'weaselx64.dll'
        $buildHash = if (Test-Path -LiteralPath $buildX64) { Get-Sha256 $buildX64 } else { '<unreachable>' }
        Add-Result -Id "P2.1" -Claim "升级后注册指向的 64 位 TIP == 本次构建产物" `
            -Status $(if($regHash -eq $buildHash){'PASS'}else{'FAIL'}) `
            -Detail "registered=$regFile sha=$regHash build=$buildHash"
    } else {
        Add-Result -Id "P2.1" -Claim "升级后 64 位 TIP 注册项可解析" -Status "FAIL" -Detail "没找到指向 weaselx64 的 InprocServer32"
    }
} else {
    Add-Result -Id "P8.0" -Claim "找到 0.21.0.2 基线安装包" -Status "SKIP" -Detail "installers 里没有 0.21.0.2"
}

# =====================================================================
# PHASE 6 · 64 位 TIP 被占用时的版本化 shim 路径 (L120)
# =====================================================================
# 场景: 有 64 位应用已映射 weaselx64.dll => 文件被锁 => 覆盖写失败。
#       install.nsi 此时应改用版本化 shim (weaselx64.<ver>.<build>.dll)
#       并把 TSF 注册指向它, 而不是退化到「等重启再换」(stage2 marker)。
#
# 锁的实现用独占方式打开文件 (FileShare.None), 不用 LoadLibrary:
#   LoadLibrary 会执行 TIP 的 DllMain, 在沙箱里注册 COM 对象, 副作用太大。
#   独占句柄同样能让 NSIS 的 Rename / File 覆盖写失败 —— 这正是要复现的条件。
$tipX64     = Join-Path $requestedRoot 'weasel\weaselx64.dll'
$verShim    = Join-Path $requestedRoot ("weasel\weaselx64.{0}.{1}.dll" -f $fluxVer, $fluxBuild)
$stage2Mark = Join-Path $requestedRoot 'weasel\weaselx64.dll.stage2'

if (-not (Test-Path -LiteralPath $tipX64)) {
    Add-Result -Id "P9.0" -Claim "64 位 TIP 已就位 (可施加锁)" -Status "SKIP" -Detail "missing=$tipX64"
} else {
    $lockStream = $null
    $r6 = $null
    try {
        $lockStream = [System.IO.File]::Open($tipX64, 'Open', 'ReadWrite', 'None')
        Write-VerifyLog -Path $verifyLog "[PHASE 6] 已独占锁定 $tipX64"
        $sw6 = [Diagnostics.Stopwatch]::StartNew()
        $r6 = Install-NsisSilent -InstallerPath $newInstaller.FullName -InstallDir $requestedRoot `
                -TimeoutSeconds $InstallTimeoutSeconds -SnapshotDir $logsDir -SnapshotTag 'phase6-tiplock'
        $sw6.Stop()
        Write-VerifyLog -Path $verifyLog "phase6 reinstall-under-lock exit = $($r6.ExitCode) elapsed = $([math]::Round($sw6.Elapsed.TotalSeconds,1))s"
    } catch {
        Write-VerifyLog -Path $verifyLog "phase6 lock failed: $($_.Exception.Message)"
        Add-Result -Id "P9.0" -Claim "64 位 TIP 可被独占锁定" -Status "FAIL" -Detail $_.Exception.Message
    } finally {
        if ($lockStream) { try { $lockStream.Close() } catch { } }
    }

    if ($r6) {
        Add-Result -Id "P9.1" -Claim "锁住 64 位 TIP 时覆盖安装仍 exit 0" `
            -Status $(if($r6.TimedOut){'FAIL'}elseif($r6.ExitCode -eq 0){'PASS'}else{'FAIL'}) `
            -Detail "exit=$($r6.ExitCode) timedOut=$($r6.TimedOut)"

        $shimExists = Test-Path -LiteralPath $verShim
        Add-Result -Id "P9.2" -Claim "改用版本化 shim (weaselx64.$fluxVer.$fluxBuild.dll)" `
            -Status $(if($shimExists){'PASS'}else{'FAIL'}) -Detail "path=$verShim exists=$shimExists"

        $stage2Exists = Test-Path -LiteralPath $stage2Mark
        Add-Result -Id "P9.3" -Claim "未退化为「等重启再换」(无 stage2 marker)" `
            -Status $(if($stage2Exists){'FAIL'}else{'PASS'}) -Detail "marker=$stage2Mark exists=$stage2Exists"

        # 64 位视图的 COM 注册必须指向版本化 shim
        $clsid64v = $null
        foreach ($clsidRoot in @('HKLM:\SOFTWARE\Classes\CLSID','HKLM:\SOFTWARE\WOW6432Node\Classes\CLSID')) {
            Get-ChildItem $clsidRoot -ErrorAction SilentlyContinue | ForEach-Object {
                $ip = Join-Path $_.PSPath 'InprocServer32'
                if (Test-Path $ip) {
                    $v = (Get-ItemProperty -Path $ip -ErrorAction SilentlyContinue).'(default)'
                    if ($v -match 'weaselx64') { $script:clsid64v = $v }
                }
            }
        }
        $inproc = if ($clsid64v) { $clsid64v -replace '^"|"$','' } else { '<no-key>' }
        $pointsToShim = ($inproc -like "*weaselx64.$fluxVer.$fluxBuild.dll*")
        Add-Result -Id "P9.4" -Claim "64 位 TIP 注册指向版本化 shim" `
            -Status $(if($pointsToShim){'PASS'}else{'FAIL'}) `
            -Detail "InprocServer32=$inproc"
    }
}

# 卸载 + 清干净
$uninst0 = Join-Path $requestedRoot "weasel\uninstall.exe"
if (Test-Path $uninst0) {
    $up = Start-Process -FilePath cmd.exe -ArgumentList "/c", "`"`"$uninst0`" /S`"" -Wait -PassThru -NoNewWindow
    Write-VerifyLog -Path $verifyLog "phase1 uninstall exit = $($up.ExitCode)"
}
Start-Sleep -Seconds 2
Clear-FluxingRegistry
Remove-Tree $requestedRoot
Remove-Tree "C:\FluxingTest"

# =====================================================================
# 报告
# =====================================================================
$summary = Write-JsonReport

Write-VerifyLog -Path $verifyLog "(end of log)"
Write-VerifyLog -Path $verifyLog "=== 汇总: PASS=$($summary.passed) FAIL=$($summary.failed) SKIP=$($summary.skipped) / 共 $($summary.total) ==="
if ($summary.failed -eq 0) {
    Write-VerifyLog -Path $verifyLog -Level PASS "全部复验项通过"
} else {
    Write-VerifyLog -Path $verifyLog -Level FAIL "有 $($summary.failed) 项未达标:"
    foreach ($f in $script:failures) { Write-VerifyLog -Path $verifyLog -Level FAIL "  - $f" }
}
Save-Screenshot -Path (Join-Path $screensDir "05-after-uninstall.png") | Out-Null
"# done at $(Get-Date -Format 'o')" | Set-Content (Join-Path $logsDir "_DONE")
exit $(if($summary.failed -eq 0){0}else{1})
