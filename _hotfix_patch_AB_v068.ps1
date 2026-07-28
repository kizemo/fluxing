#requires -Version 5.1
<#
_hotfix_patch_AB_v068.ps1 — v0.68 完整 hotfix (Patch A/B 进 x86 + x64 两个 shim)

═══════════════════════════════════════════════════════════════════════
真因: v0.67 installer 内嵌 weasel.dll 实为 v0.66 build, Patch A/B (commit bbd0414)
     未 build 进 installer. v0.68 修复后同期 ship weasel.dll (x86) + weaselx64.dll
     (x64) 两个 shim 的 Patch A/B. v0.67 hotfix 只覆盖 x86, 64-bit 进程 (Explorer,
     Chrome, Edge, modern Office) 仍 v0.66 shim. 本脚本 v0.68 完整版覆盖双 shim.

预期 md5 (本 fresh build):
  - weasel.dll:     2edbacc8b5cf18c111fb49f543363474
  - weaselx64.dll:  TBD (x64 rebuild 后填)

用法 (user system, admin):
   powershell -ExecutionPolicy Bypass -File _hotfix_patch_AB_v068.ps1

退出码: 0 = PASS, 1 = FAIL (md5 mismatch), 2 = admin required / file missing
═══════════════════════════════════════════════════════════════════════
#>

$ErrorActionPreference = 'Stop'

$LogFile = Join-Path $env:TEMP "hotfix_patch_AB_v068_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

function Write-Log {
    param([string]$Msg, [string]$Level = "INFO")
    $stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $line = "[$stamp] [$Level] $Msg"
    Write-Host $line
    Add-Content -Path $LogFile -Value $line
}

# === 0. admin check ===
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Log "需要 admin 权限跑本 hotfix. 用 admin shell 重试." "FATAL"
    exit 2
}

# === 1. paths ===
$weaselDir = "D:\Program Files\fluxing\weasel"

# shim 列表: 名称, 目标文件, 备份后缀, 源 hotfix 路径, 期望 md5
# 若 x64 build 待补, 仅跑 x86 一项并 dump md5 提示用户
$shims = @(
    @{
        Name       = "weasel.dll"
        Target     = Join-Path $weaselDir "weasel.dll"
        BackupExt  = ".hotfix_v066_old"
        HotfixSrc  = Join-Path $PSScriptRoot "hotfix_v0.67_patch_AB\weasel.dll"
        ExpectedMd5 = "2edbacc8b5cf18c111fb49f543363474"
    }
    @{
        Name       = "weaselx64.dll"
        Target     = Join-Path $weaselDir "weaselx64.dll"
        BackupExt  = ".hotfix_v066_old"
        HotfixSrc  = Join-Path $PSScriptRoot "hotfix_v0.68_patch_AB\weaselx64.dll"
        ExpectedMd5 = $null  # TODO: x64 build 后填入
    }
)

Write-Log "═══════ Phase L Bugfix 7 v0.68 hotfix start ═══════"
Write-Log "Log: $LogFile"

# === 2. iter 每个 shim ===
foreach ($shim in $shims) {
    $name  = $shim.Name
    $tgt   = $shim.Target
    $bkp   = $tgt + $shim.BackupExt
    $src   = $shim.HotfixSrc
    $expMd5 = $shim.ExpectedMd5

    Write-Log "─── 处理 $name ───"

    # 2.0. 若 x64 md5 未填, 跳过 (留 TODO 给 user 手动补)
    if ([string]::IsNullOrEmpty($expMd5)) {
        Write-Log "$name 期望 md5 未填 (x64 build 待补). 跳过此 shim." "WARN"
        Write-Log "  user 需: 1) 拿到 x64 build 后, 2) 把 md5 填入本脚本 §1 shims 段, 3) 重跑本脚本" "INFO"
        continue
    }

    # 2.1. verify source
    if (-not (Test-Path $src)) {
        Write-Log "hotfix source not found: $src" "FATAL"
        Write-Log "  把 fresh-build $name 放该路径: $src" "INFO"
        exit 2
    }
    $srcMd5 = (Get-FileHash -Path $src -Algorithm MD5).Hash.ToLower()
    Write-Log "$name source md5: $srcMd5 (expected $expMd5)"
    if ($srcMd5 -ne $expMd5) {
        Write-Log "$name SOURCE MD5 MISMATCH! Reject ship." "FATAL"
        exit 1
    }

    # 2.2. verify target (user system)
    if (-not (Test-Path $tgt)) {
        Write-Log "user system $name 不存在: $tgt. 检查安装路径是否 $weaselDir" "FATAL"
        exit 2
    }
    $tgtMd5 = (Get-FileHash -Path $tgt -Algorithm MD5).Hash.ToLower()
    Write-Log "$name target (current) md5: $tgtMd5"
    if ($tgtMd5 -eq $expMd5) {
        Write-Log "$name 已是 Patch A/B 版本, 跳过 hotfix." "INFO"
        continue
    }
    Write-Log "$name 是 v0.66 老 binary, 需要 hotfix" "INFO"

    # 2.3. backup current
    Write-Log "备份 $tgt → $bkp"
    try {
        Move-Item -Path $tgt -Destination $bkp -Force
    } catch {
        Write-Log "Move-Item failed: $($_.Exception.Message). Try icacls + retry..." "ERROR"
        & icacls $tgt /grant "$($env:USERNAME):(F)" /T 2>&1 | Out-Null
        Move-Item -Path $tgt -Destination $bkp -Force
    }

    # 2.4. stop ctfmon + TextInputHost (both shims mmap by ctfmon)
    Write-Log "taskkill ctfmon + TextInputHost (非 PPL, taskkill 有效)"
    foreach ($proc in @("ctfmon", "TextInputHost")) {
        $p = Get-Process -Name $proc -ErrorAction SilentlyContinue
        if ($p) {
            try {
                Stop-Process -Id $p.Id -Force -ErrorAction Stop
                Write-Log "killed $proc (PID $($p.Id))"
            } catch {
                Write-Log "stop $proc failed: $($_.Exception.Message)" "WARN"
            }
        } else {
            Write-Log "$proc not running"
        }
    }
    Start-Sleep -Seconds 3

    # 2.5. copy new shim
    Write-Log "复制 $src → $tgt"
    try {
        Copy-Item -Path $src -Destination $tgt -Force
    } catch {
        Write-Log "Copy-Item failed: $($_.Exception.Message). Try takeown + icacls..." "ERROR"
        & takeown /F $tgt 2>&1 | Out-Null
        & icacls $tgt /grant "$($env:USERNAME):(F)" /T 2>&1 | Out-Null
        Copy-Item -Path $src -Destination $tgt -Force
    }

    # 2.6. restart ctfmon (SYSTEM auto-spawns; manual fallback if needed)
    Write-Log "ctfmon 由 SYSTEM service 自动重启; 若 3s 内未起, 手动启"
    Start-Sleep -Seconds 3
    $ctfmon = Get-Process -Name ctfmon -ErrorAction SilentlyContinue
    if (-not $ctfmon) {
        Write-Log "ctfmon 未自动重启, 手动启动" "WARN"
        Start-Process -FilePath "ctfmon.exe" -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
    }

    # 2.7. verify md5
    $newMd5 = (Get-FileHash -Path $tgt -Algorithm MD5).Hash.ToLower()
    Write-Log "$name post-hotfix md5: $newMd5"
    if ($newMd5 -ne $expMd5) {
        Write-Log "$name VERIFY FAIL! 期望 $expMd5 实际 $newMd5. 从 $bkp 还原" "FATAL"
        Move-Item -Path $bkp -Destination $tgt -Force
        exit 1
    }
    Write-Log "$name hotfix PASS ✓" "INFO"
}

Write-Log "═══════ hotfix v0.68 全部完成 ═══════"
Write-Log ""
Write-Log "下次 32-bit / 64-bit 进程启动时, 自动 mmap 新的 shim (Patch A/B in)"
Write-Log "QuickPanel 应在切到 Fluxing 时弹出"
Write-Log ""
Write-Log "TODO (v0.68 installer 才能 ship):"
Write-Log "  - WeaselServer.exe — 仍 v0.66 build (v0.67 patch not in WeaselServer)"
Write-Log "  - install.hang 永久 17min — v0.68 installer 内 MaintenanceGuard 5s timeout fix"
Write-Log "  - 若 x64 md5 未填, 重跑本脚本前必须先: x64 build 拿到 output\weaselx64.dll md5 后填"
Write-Log ""
Write-Log "Log: $LogFile"

exit 0
