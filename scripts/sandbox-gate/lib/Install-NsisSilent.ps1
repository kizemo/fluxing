# Install-NsisSilent.ps1
# NSIS 安装器静默安装 + 路径显式指定 (/S /D=)
#
# L15/A12: 用 cmd /c 包裹,避免 PS 5.1 在 = 边界把参数合并。
#
# ★★ 2026-10-24 修正: /D= 的值 **绝对不能带引号** ★★
#   NSIS 官方手册 3.2.1 原文:
#     "/D sets the default installation directory ($INSTDIR), overriding
#      InstallDir and InstallDirRegKey. It must be the last parameter used in
#      the command line and must not contain any quotes, even if the path
#      contains spaces. Only absolute paths are supported."
#   旧版本这里写的是 /D="$InstallDir" (带引号) ⇒ NSIS 直接忽略该参数 ⇒
#   $INSTDIR 落到 install.nsi 里硬编码的默认路径 "D:\Program Files\fluxing"。
#   后果: 静默装机验收(AGENTS.md 2.5)从来没在指定路径上真的跑过,
#         所有 "InstallDir == 期望路径" 的断言都是假阴性。
#   证据: 2026-10-24 沙箱复验, 传入 /D=C:\FluxingTest\fluxing (带引号),
#         注册表里写的是 D:\Program Files\fluxing, 且该路径下无任何文件。
#
#   路径含空格是允许的 —— 不要加引号, 直接把 /D= 放在命令行最后。
#
# ★★ 2026-10-24 v3 修正: 退出码 + 超时 ★★
#   退出码改由 cmd 自己在执行后写入标记文件 (cmd /v:on + !ERRORLEVEL!),
#   不再依赖 Start-Process 的 ExitCode (WaitForExit(ms) 后经常是空值)。
#   新增 -TimeoutSeconds: 超时即杀掉整棵进程树并返回 ExitCode = -9999 +
#   TimedOut = $true, 让"装机卡死"变成一条可断言的度量, 而不是整个复验挂死。

function Install-NsisSilent {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$InstallerPath,
        [Parameter(Mandatory)][string]$InstallDir,
        [string]$LogPath,
        [string]$ExtraArgs,
        [int]$TimeoutSeconds = 0,          # 0 = 不限等待 (保持旧行为)
        [string]$SnapshotDir,              # 超时时把现场快照写到这里
        [string]$SnapshotTag = 'install'
    )

    if (-not (Test-Path $InstallerPath)) {
        throw "Installer missing: $InstallerPath"
    }
    if (-not [System.IO.Path]::IsPathRooted($InstallDir)) {
        throw "InstallDir must be an absolute path (NSIS /D= requirement): $InstallDir"
    }

    # /D 必须是最后一个参数, 且值不带引号。
    $argLine = "/S"
    if ($ExtraArgs) { $argLine += " $ExtraArgs" }
    $argLine += " /D=$InstallDir"

    $cmd = "`"$InstallerPath`" $argLine"

    # 让 cmd 自己把退出码写到标记文件 —— 不依赖 Start-Process 的 ExitCode。
    $marker = Join-Path $env:TEMP ("nsis_exit_{0}.txt" -f ([guid]::NewGuid().ToString('N')))
    $inner  = "$cmd & echo !ERRORLEVEL!> `"$marker`""

    Write-Host ">>> $cmd"
    if ($TimeoutSeconds -gt 0) { Write-Host "    (timeout ${TimeoutSeconds}s)" }

    $proc = Start-Process -FilePath $env:ComSpec `
        -ArgumentList "/v:on", "/c", "`"$inner`"" `
        -PassThru -NoNewWindow

    $timedOut = $false
    if ($TimeoutSeconds -gt 0) {
        if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
            $timedOut = $true
            Write-Host "    TIMEOUT after $TimeoutSeconds s - killing process tree"
            try { & taskkill.exe /F /T /PID $proc.Id | Out-Null } catch {}
            try { $proc.Kill() } catch {}
            try { $proc.WaitForExit(15000) | Out-Null } catch {}
        } else {
            $proc.WaitForExit()
        }
    } else {
        $proc.WaitForExit()
    }

    # 读回退出码
    $exitCode = $null
    if (Test-Path $marker) {
        $raw = (Get-Content $marker -Raw -ErrorAction SilentlyContinue)
        if ($raw) {
            $parsed = 0
            if ([int]::TryParse($raw.Trim(), [ref]$parsed)) { $exitCode = $parsed }
        }
        Remove-Item $marker -Force -ErrorAction SilentlyContinue
    }
    if ($null -eq $exitCode) {
        try { $exitCode = $proc.ExitCode } catch {}
    }
    if ($null -eq $exitCode) { $exitCode = -1 }

    $snapshotPath = ''
    if ($timedOut -and $SnapshotDir) {
        $snapshotPath = Join-Path $SnapshotDir ("_snapshot_{0}.txt" -f $SnapshotTag)
        New-Item -ItemType Directory -Path $SnapshotDir -Force | Out-Null
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.AppendLine("=== install snapshot @ $(Get-Date -Format 'o') ===")
        [void]$sb.AppendLine("command: $cmd")
        [void]$sb.AppendLine("timeout: $TimeoutSeconds s  -> KILLED")
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("--- processes (pid | name | command line) ---")
        try {
            Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
                Sort-Object Name |
                ForEach-Object { [void]$sb.AppendLine(("{0} | {1} | {2}" -f $_.ProcessId, $_.Name, $_.CommandLine)) }
        } catch { [void]$sb.AppendLine("process enumeration failed: $($_.Exception.Message)") }
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("--- files under install dir (depth 3) ---")
        try {
            $root = $InstallDir
            if (Test-Path $root) {
                Get-ChildItem $root -Recurse -Depth 3 -Force -ErrorAction SilentlyContinue |
                    ForEach-Object {
                        $rel = $_.FullName.Substring($root.Length)
                        [void]$sb.AppendLine(("{0} {1} {2}" -f $(if($_.PSIsContainer){'<DIR>'}else{$_.Length}), $rel, $_.LastWriteTime))
                    }
            } else {
                [void]$sb.AppendLine("install dir does not exist: $root")
            }
        } catch { [void]$sb.AppendLine("dir walk failed: $($_.Exception.Message)") }
        [void]$sb.AppendLine("")
        [void]$sb.AppendLine("--- weasel/rime logs found ---")
        try {
            $cands = @()
            foreach ($base in @($env:TEMP, $env:LOCALAPPDATA, $InstallDir, (Split-Path $InstallDir -Parent))) {
                if ($base -and (Test-Path $base)) {
                    $cands += Get-ChildItem $base -Recurse -Depth 2 -Include '*.log','*.yaml' -File -ErrorAction SilentlyContinue |
                                Where-Object { $_.Name -match 'rime|weasel|install' }
                }
            }
            foreach ($c in ($cands | Sort-Object FullName -Unique | Select-Object -First 20)) {
                [void]$sb.AppendLine("### $($c.FullName)")
                try { Get-Content $c.FullName -Tail 40 -ErrorAction SilentlyContinue | ForEach-Object { [void]$sb.AppendLine("  $_") } } catch {}
            }
        } catch { [void]$sb.AppendLine("log scan failed: $($_.Exception.Message)") }
        Set-Content -Path $snapshotPath -Value $sb.ToString() -Encoding UTF8
        Write-Host "    snapshot -> $snapshotPath"
    }

    return [pscustomobject]@{
        ExitCode   = $exitCode
        TimedOut   = $timedOut
        Command    = $cmd
        LogPath    = $LogPath
        Snapshot   = $snapshotPath
    }
}
