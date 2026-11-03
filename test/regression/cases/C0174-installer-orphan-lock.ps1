# C0174 —— D0174：安装器必须清掉「父进程已死」的自家 UI 孤儿（否则目标文件被独占锁）
# CASE-LEVEL: heavy
# DEFECT: D0174
#
# 守的是什么（L174）：
#   上一代自家 UI 子进程在 server 被强杀后成了**孤儿**，它不在任何活着的进程树里 ⇒
#   `taskkill /T` 打不到它 ⇒ 目标 exe 被独占锁 ⇒ 交互安装弹「无法打开要写入的文件」。
#   修法是 install.nsi 里 4 处**按映像名**杀（不带 /T），映像名与
#   `fluxing::OwnUiClientExeName()`（include/WeaselIPC.h）同源。
#
# 两段式：
#   §A 静态段（任何机器都跑）：install.nsi 的按映像名清理块必须 >= 4 处，且映像名与头文件一致。
#   §B A/B 段（需要显式开关 + 两个安装器）：旧安装器必须**装不上**、新安装器必须**装上**。
#
# §B 会真装、会覆盖本机已安装产品 ⇒ 必须显式指定环境变量才执行，否则 SKIP：
#   $env:FLUXING_ALLOW_INSTALL_AB = '1'
#   $env:FLUXING_AB_NEW_INSTALLER  = 'F:\...\output\archives\fluxing-1.1.9-installer.exe'
#   $env:FLUXING_AB_OLD_INSTALLER  = 'F:\...\output\archives\fluxing-1.1.8-installer.exe'   # 必须能复现缺陷
#   $env:FLUXING_AB_INSTALL_DIR    = 'D:\Program Files\fluxing'    # 可选
#
# 判据不认安装器退出码：NSIS 静默模式会**吞掉 File 失败**并照样返回 0（L174）。

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\lib\CaseHarness.ps1')

Initialize-Case -Id 'C0174' -Title '安装器按映像名清理自家 UI 孤儿（含旧/新安装器 A/B）'

$ownUi = 'FluxingPhrasesDialog.exe'
$ownUiBase = [System.IO.Path]::GetFileNameWithoutExtension($ownUi)

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

# 独占打开成功 ⇒ 没被锁；抛异常 ⇒ 被锁。
function Test-ExclusiveLocked {
    param([Parameter(Mandatory = $true)][string]$Path)
    try {
        $fs = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $fs.Close()
        $fs.Dispose()
        return $false
    }
    catch {
        return $true
    }
}

function Get-OwnUiProcesses {
    return @(Get-Process -Name $ownUiBase -ErrorAction SilentlyContinue)
}

function Stop-OwnUiProcesses {
    foreach ($p in (Get-OwnUiProcesses)) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction Stop } catch { }
    }
}

try {
    $root = Get-RepoRoot
    $nsi = Join-Path $root 'output\install.nsi'
    $ipcHeader = Join-Path $root 'include\WeaselIPC.h'
    $versionFile = Join-Path $root 'version.txt'

    ## ---------- §A 静态段 ----------
    if (-not (Test-Path -LiteralPath $nsi)) {
        Assert-Prerequisite $false "缺 output\install.nsi" | Out-Null
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    $nsiText = Get-Content -LiteralPath $nsi -Raw -Encoding utf8
    # 按映像名杀、不带 /T 的那一条（带 /T 的父进程树清扫是另一回事，不能算）。
    $killPattern = [regex]::Escape("taskkill /F /IM $ownUi'")
    $killCount = ([regex]::Matches($nsiText, $killPattern)).Count
    Assert-Case ($killCount -ge 4) "install.nsi 按映像名清理 '$ownUi' 的块 >= 4 处（实际 $killCount）"

    # 映像名与 include/WeaselIPC.h 的 OwnUiClientExeName() 必须同源（防改名漏改）。
    $headerText = ''
    if (Test-Path -LiteralPath $ipcHeader) { $headerText = Get-Content -LiteralPath $ipcHeader -Raw -Encoding utf8 }
    $m = [regex]::Match($headerText, 'OwnUiClientExeName\(\)\s*\{\s*return\s*"([^"]+)"')
    if (Assert-Prerequisite $m.Success "could not parse OwnUiClientExeName() from include/WeaselIPC.h") {
        Assert-Equal $ownUi $m.Groups[1].Value 'install.nsi 里的映像名与 OwnUiClientExeName() 同源' | Out-Null
    }

    # 「不带 /T」这条也必须真的成立：被守护的清理块不能退化成父进程树清扫。
    $withTree = ([regex]::Matches($nsiText, [regex]::Escape("taskkill /F /IM $ownUi /T'"))).Count
    Assert-Equal $withTree 0 "按映像名清理块不得带 /T（/T 只走活着的进程树，打不到孤儿）" | Out-Null

    ## ---------- §B A/B 段 ----------
    $instDir = 'D:\Program Files\fluxing'
    if ($env:FLUXING_AB_INSTALL_DIR) { $instDir = $env:FLUXING_AB_INSTALL_DIR }
    $target = Join-Path $instDir "weasel\$ownUi"
    $uninstKey = 'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing'

    $expectedVersion = ''
    if (Test-Path -LiteralPath $versionFile) {
        $expectedVersion = (Get-Content -LiteralPath $versionFile -Raw -Encoding utf8).Trim()
    }

    $newInstaller = $env:FLUXING_AB_NEW_INSTALLER
    if (-not $newInstaller -and $expectedVersion -ne '') {
        $newInstaller = Join-Path $root "output\archives\fluxing-$expectedVersion-installer.exe"
    }
    $oldInstaller = $env:FLUXING_AB_OLD_INSTALLER

    $prereqOk = $true
    $prereqOk = (Assert-Prerequisite ([bool]$env:FLUXING_ALLOW_INSTALL_AB -and $env:FLUXING_ALLOW_INSTALL_AB -eq '1') `
            '未设 FLUXING_ALLOW_INSTALL_AB=1：§B 会真装并覆盖本机已安装产品，默认不执行') -and $prereqOk
    $prereqOk = (Assert-Prerequisite ([bool]$oldInstaller -and (Test-Path -LiteralPath $oldInstaller)) `
            "缺旧安装器（FLUXING_AB_OLD_INSTALLER）：A/B 的反向对照要求旧安装器在同一场景下必须失败") -and $prereqOk
    if ($newInstaller) {
        $prereqOk = (Assert-Prerequisite (Test-Path -LiteralPath $newInstaller) "缺新安装器：$newInstaller") -and $prereqOk
    }
    else {
        $prereqOk = (Assert-Prerequisite $false '缺新安装器（FLUXING_AB_NEW_INSTALLER 未设且 output\archives 下找不到匹配 version.txt 的产物）') -and $prereqOk
    }
    $prereqOk = (Assert-Prerequisite (Test-Path -LiteralPath $target) "缺已安装目标文件：$target（先装一次基线版本）") -and $prereqOk

    if (-not $prereqOk) {
        throw [System.OperationCanceledException]::new('prerequisite-missing')
    }

    # 清场 + 兜底清理（A/B 自己拉起的孤儿必须由本用例收掉）
    Stop-OwnUiProcesses
    Register-Cleanup -Name "kill leftover $ownUi" -Action { Stop-OwnUiProcesses }

    # 造靶子：N 个孤儿（父进程就是本脚本，但 --pipe 指向不存在的 server ⇒ 它们只会占着文件镜像）。
    # 注意：这里不依赖真实 server，锁来自「进程镜像被加载」，与缺陷现场同机制。
    $orphanCount = 6
    $probeArgs = '--pipe=\\.\pipe\FluxingPhrasesDialog\999999'
    $spawned = 0
    for ($i = 0; $i -lt $orphanCount; $i++) {
        try {
            Start-Process -FilePath $target -ArgumentList $probeArgs -WindowStyle Hidden -ErrorAction Stop | Out-Null
            $spawned++
        }
        catch {
            # 单个失败不致命，但下面会断言「确实被锁住」，锁不上就没法做 A/B。
        }
    }
    Start-Sleep -Milliseconds 800
    Assert-Case ($spawned -gt 0) "已拉起 $spawned/$orphanCount 个自家 UI 占位进程"
    Assert-Case (Test-ExclusiveLocked $target) '目标 exe 被独占锁（复现缺陷现场）'

    $preMtime = (Get-Item -LiteralPath $target).LastWriteTimeUtc
    $preHash = Get-Sha256 $target
    $newHash = Get-Sha256 $newInstaller

    function Invoke-SilentInstall {
        param([Parameter(Mandatory = $true)][string]$InstallerExe, [Parameter(Mandatory = $true)][string]$Dir, [int]$TimeoutSec = 600)
        $argline = '"{0}" /S /D={1}' -f $InstallerExe, $Dir
        $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @('/c', $argline) -Wait -PassThru -WindowStyle Hidden
        return $p.ExitCode
    }

    # --- A：旧安装器必须「装不上」（目标文件没被重写）---
    $rcOld = Invoke-SilentInstall -InstallerExe $oldInstaller -Dir $instDir
    $aMtime = (Get-Item -LiteralPath $target).LastWriteTimeUtc
    Assert-Equal $aMtime $preMtime 'A/B·A：旧安装器未重写目标（缺陷复现成功，反向对照成立）' | Out-Null
    Assert-Case ($rcOld -eq 0) "A/B·A：旧安装器退出码=$rcOld（说明退出码不可信，判据必须看落盘）"

    # --- B：新安装器必须装上 ---
    $rcNew = Invoke-SilentInstall -InstallerExe $newInstaller -Dir $instDir
    $bMtime = (Get-Item -LiteralPath $target).LastWriteTimeUtc
    $bHash = Get-Sha256 $target
    Assert-Case ($bMtime -ne $aMtime) 'A/B·B：新安装器重写了目标文件（mtime 变化）'
    Assert-Equal $bHash $newHash 'A/B·B：落盘 sha256 == 新安装器构建产物' | Out-Null
    Assert-Case ($rcNew -eq 0) "A/B·B：新安装器退出码=$rcNew"

    # 清理覆盖面：装完之后不该再有自家 UI 残留
    Assert-Equal (Get-OwnUiProcesses).Count 0 'A/B·B：装完后自家 UI 残留进程数' | Out-Null

    # 注册表写到尾（NSIS 静默模式可能在 File 之后仍继续写注册表 ⇒ 必须两维都验）
    if (Test-Path -LiteralPath $uninstKey) {
        $dv = (Get-ItemProperty -LiteralPath $uninstKey -ErrorAction SilentlyContinue).DisplayVersion
        Assert-Equal $dv $expectedVersion 'A/B·B：卸载项 DisplayVersion 已写到尾' | Out-Null
    }
    else {
        Assert-Case $false "找不到卸载注册表项：$uninstKey"
    }

    # 顺带把「装的是不是当前构建」钉住，防止 A/B 拿错产物。
    $buildServer = Join-Path $root 'output\Win32\WeaselServer.exe'
    if (Test-Path -LiteralPath $buildServer) {
        $installed = Join-Path $instDir 'weasel\WeaselServer.exe'
        if (Test-Path -LiteralPath $installed) {
            Assert-Equal (Get-Sha256 $installed) (Get-Sha256 $buildServer) 'A/B·B：已安装 WeaselServer.exe == 当前构建' | Out-Null
        }
    }
}
catch [System.OperationCanceledException] {
}
finally {
    Complete-Case
}
