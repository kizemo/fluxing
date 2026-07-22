# _check_install_v2.ps1
# Phase A.10: 多角度 root cause evidence collection (增强版)
#
# User 跑这个 → 输出 → 贴回来 → 我能直接定位 root cause
#
# 6 大证据模块:
#   1. Running WeaselServer process: path/md5/mtime + 装载的 fluxing/weasel/rime DLL
#   2. Installed WeaselServer.exe on disk (3 个 candidate 路径)
#   3. Registry InstallDir / RimeUserDir / Uninstall key
#   4. Windows event log: Application Error / AppHang / .NET Runtime 最近 24h
#   5. 装机路径下所有文件 mtime 排序 (看 newest = installer 时间, 或 oldest = 老 binary)
#   6. QuickPanel button 截图 + SendMessage WM_HOTKEY 自测 (需要 interactive console)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # 加速 Get-ChildItem

Write-Host "=========================================="
Write-Host " Fluxing install state evidence dump v2"
Write-Host " $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "=========================================="
Write-Host ""

# ============================================================
# 1. Running WeaselServer process + loaded modules
# ============================================================
Write-Host "=== [1] Running WeaselServer process ==="
$proc = Get-Process WeaselServer -ErrorAction SilentlyContinue
if (-not $proc) {
  Write-Host "  WeaselServer: NOT RUNNING"
} else {
  foreach ($p in $proc) {
    Write-Host "  PID=$($p.Id) StartTime=$($p.StartTime) MainWindowHandle=$($p.MainWindowHandle)"
    Write-Host "  Path: $($p.MainModule.FileName)"
    $h = (Get-FileHash -Path $p.MainModule.FileName -Algorithm MD5).Hash
    $size = (Get-Item $p.MainModule.FileName).Length
    $mtime = (Get-Item $p.MainModule.FileName).LastWriteTime
    Write-Host "  md5=$h  size=$size  mtime=$mtime"
    Write-Host "  Loaded modules (filter: weasel|fluxing|phrases|userd|shortcut|rime|tsf):"
    $p.Modules | Where-Object { $_.ModuleName -match 'weasel|fluxing|phrases|userd|shortcut|rime|tsf' } | ForEach-Object {
      $modHash = ''
      if (Test-Path $_.FileName) {
        try { $modHash = (Get-FileHash -Path $_.FileName -Algorithm MD5 -ErrorAction SilentlyContinue).Hash } catch {}
      }
      Write-Host "    $($_.ModuleName) - $($_.FileName)"
      Write-Host "      md5=$modHash  size=$($_.FileName.Length)"
    }
  }
}
Write-Host ""

# ============================================================
# 2. Installed WeaselServer.exe on disk (3 candidate paths)
# ============================================================
Write-Host "=== [2] Installed WeaselServer.exe on disk ==="
$expectedMd5 = '3049f3bbb091961901b457105371aa1b'  # v0.19.0.48 binary (Phase J: Bug 1 IMM32 fallback + Bug 2 IME 切换 + Bug 3 title bar 立即 drag + install.nsi Stage 2 PPL fallback)
$expectedInstallerMd5 = '0889e77358e7795da47414ba5bf2019f'  # v0.19.0.48 installer
$expectedBuildTime = '2026-07-22 09:08:00'  # v0.19.0.48 build (NSIS timestamp)

$paths = @(
    'D:\Program Files\fluxing\weasel\WeaselServer.exe',
    'D:\Program Files\fluxing\weasel\weasel\WeaselServer.exe',  # if double-subdir happened
    'C:\Program Files\fluxing\weasel\WeaselServer.exe',
    'C:\Program Files\Fluxing\weasel\WeaselServer.exe',
    'C:\Program Files (x86)\fluxing\weasel\WeaselServer.exe'
)
$foundAny = $false
foreach ($p in $paths) {
    if (Test-Path $p) {
        $foundAny = $true
        $h = (Get-FileHash -Path $p -Algorithm MD5).Hash
        $size = (Get-Item $p).Length
        $mtime = (Get-Item $p).LastWriteTime
        $match = if ($h -eq $expectedMd5) { '<== MATCHES expected (v0.19.0.48 Phase J 3 bug + Stage 2)' } else { '<== MISMATCH (old binary or wrong build!)' }
        Write-Host "  $p"
        Write-Host "    md5=$h  size=$size  mtime=$mtime  $match"
    }
}
if (-not $foundAny) {
    Write-Host "  No WeaselServer.exe found in any of the standard install paths!"
}
Write-Host "  Expected: md5=$expectedMd5 (v0.19.0.48 Phase J 3 bug + Stage 2, build 2026-07-22)"
Write-Host ""

# 额外扫整个 D:/C: 找所有 WeaselServer.exe (探测多安装)
Write-Host "  --- Wildcard search for ALL WeaselServer.exe on C: and D: ---"
foreach ($drive in @('C:', 'D:')) {
    if (Test-Path $drive) {
        Get-ChildItem -Path $drive -Recurse -Filter 'WeaselServer.exe' -ErrorAction SilentlyContinue -Force | ForEach-Object {
            $h = (Get-FileHash -Path $_.FullName -Algorithm MD5 -ErrorAction SilentlyContinue).Hash
            $match = if ($h -eq $expectedMd5) { 'MATCH' } else { 'MISMATCH' }
            Write-Host "    [$drive] $($_.FullName) md5=$h mtime=$($_.LastWriteTime) $match"
        }
    }
}
Write-Host ""

# ============================================================
# 3. Registry: InstallDir / RimeUserDir / Uninstall
# ============================================================
Write-Host "=== [3] Registry state ==="
$regKeys = @(
    @{ Path = 'HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel'; Name = 'InstallDir' },
    @{ Path = 'HKLM:\SOFTWARE\Fluxing\Weasel'; Name = 'InstallDir' },
    @{ Path = 'HKCU:\Software\Fluxing\Weasel'; Name = 'RimeUserDir' },
    @{ Path = 'HKCU:\Software\Rime\Weasel'; Name = 'RimeUserDir' },
    @{ Path = 'HKLM:\SOFTWARE\Fluxing\Weasel'; Name = 'WeaselRoot' },
    @{ Path = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing'; Name = 'InstallLocation' },
    @{ Path = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing'; Name = 'DisplayVersion' },
    @{ Path = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing'; Name = 'UninstallString' }
)
foreach ($k in $regKeys) {
    if (Test-Path $k.Path) {
        $val = (Get-ItemProperty -Path $k.Path -Name $k.Name -ErrorAction SilentlyContinue).$($k.Name)
        Write-Host "  $($k.Path)\$($k.Name) = $val"
    } else {
        Write-Host "  $($k.Path) -- NOT FOUND"
    }
}
Write-Host ""

# ============================================================
# 4. Windows Event Log - Application Error / AppHang
# ============================================================
Write-Host "=== [4] Windows Event Log (last 24h, fluxing/weasel errors) ==="
$since = (Get-Date).AddHours(-24)
try {
    Write-Host "  --- Application Error (Event ID 1000) ---"
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Id = 1000; StartTime = $since } -ErrorAction SilentlyContinue |
      Where-Object { $_.Message -match 'weasel|fluxing' } |
      Select-Object -First 5 |
      ForEach-Object {
        $msg = $_.Message -replace "`r`n", ' '
        if ($msg.Length -gt 200) { $msg = $msg.Substring(0, 200) + '...' }
        Write-Host "    [$($_.TimeCreated)] $msg"
      }
} catch { Write-Host "    (no Application Error events)" }

try {
    Write-Host "  --- AppHangTransient (Event ID 1002) ---"
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Id = 1002; StartTime = $since } -ErrorAction SilentlyContinue |
      Where-Object { $_.Message -match 'weasel|fluxing' } |
      Select-Object -First 5 |
      ForEach-Object {
        $msg = $_.Message -replace "`r`n", ' '
        if ($msg.Length -gt 200) { $msg = $msg.Substring(0, 200) + '...' }
        Write-Host "    [$($_.TimeCreated)] $msg"
      }
} catch { Write-Host "    (no AppHang events)" }

try {
    Write-Host "  --- Application popup 26 (installer / DLL popup) ---"
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; Id = 26; StartTime = $since } -ErrorAction SilentlyContinue |
      Select-Object -First 5 |
      ForEach-Object {
        $msg = $_.Message -replace "`r`n", ' '
        if ($msg.Length -gt 200) { $msg = $msg.Substring(0, 200) + '...' }
        Write-Host "    [$($_.TimeCreated)] $msg"
      }
} catch { Write-Host "    (no popup 26 events)" }
Write-Host ""

# ============================================================
# 5. Install path full file listing (sort by mtime desc)
# ============================================================
Write-Host "=== [5] Install path full file listing (newest 15) ==="
$installDirs = @('D:\Program Files\fluxing\weasel', 'D:\Program Files\fluxing', 'C:\Program Files\fluxing\weasel', 'C:\Program Files\fluxing')
foreach ($d in $installDirs) {
    if (Test-Path $d) {
        Write-Host "  $d"
        Get-ChildItem $d -File -Recurse -ErrorAction SilentlyContinue -Force |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 15 |
            ForEach-Object {
                $h = ''
                if ($_.Length -lt 10MB) {
                    try { $h = (Get-FileHash -Path $_.FullName -Algorithm MD5 -ErrorAction SilentlyContinue).Hash } catch {}
                }
                $rel = $_.FullName.Substring($d.Length)
                Write-Host "    [$($_.LastWriteTime.ToString('MM-dd HH:mm'))] $($_.Length.ToString().PadLeft(10))  $h  $rel"
            }
    }
}
Write-Host ""

# ============================================================
# 6. Expected vs actual summary
# ============================================================
Write-Host "=== [6] Summary ==="
Write-Host "  Installer v0.19.0.48 md5 expected: $expectedInstallerMd5"
Write-Host "  WeaselServer.exe md5 expected:     $expectedMd5 (build $expectedBuildTime)"
Write-Host "  Module 3 L66 expected keys (admin install OK):"
Write-Host "    HKLM\SOFTWARE\Microsoft\CTF\KnownClasses = '{A3F4CDED-...}' = 'Fluxing Text Service'"
Write-Host "    HKCU\Software\Microsoft\CTF\Assemblies\0x00000804\{3D02CAB6-...}\Default = CLSID"
Write-Host "    HKCU\...\0x00000804\{3D02CAB6-...}\Profile = CLSID"
Write-Host "    HKCU\...\0x00000804\{3D02CAB6-...}\KeyboardLayout = 0x08040804"
Write-Host ""
Write-Host "  How to interpret:"
Write-Host "    [1] 如果 Running WeaselServer path != [2] Install path → 多装/老 binary"
Write-Host "    [1] 如果 Running md5 != expected → 跑的仍是老 binary"
Write-Host "    [2] 如果 Install md5 != expected → 装机没覆盖 (File 失败/lock)"
Write-Host "    [3] 如果 HKLM InstallDir != [2] path → registry 指向错位置"
Write-Host "    [4] 如果有 AppHangTransient weasel → 启动挂死"
Write-Host "    [5] newest file 是 7/21 14:22 → installer 装过; 是更早 → 没覆盖"
Write-Host "    [3] Module 3 L66 keys 缺 → installer L66-fix 失效 (C41FEED9 ship 应已无条件写)"
Write-Host ""
Write-Host "  Post-mortem copy: 把以上所有输出贴回 Fluxing Claude session"