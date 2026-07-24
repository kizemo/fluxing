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
# v0.19.0.58 (Phase K5 Bug 3+4 真修 hotfix): source 上 v0.19.0.57 phase K4
#   装的 Enter 智能 add/edit 在装机端实际仍 fail (Esc 也无法退出)。真因(Phase
#   K5 root cause): WS_POPUP + main.cpp 简单 message loop (无 IsDialogMessage)
#   → 子控件 focus 时 WM_KEYDOWN VK_RETURN/VK_ESCAPE 不 bubble 到 PhrasesDialog
#   WndProc → OnKeyDown handler 失效。Test 40/41/42 在 sandbox 模拟 SetFocus +
#   SendMessage(hwnd, WM_KEYDOWN, ...) 直接派发到 dialog WndProc,绕过 input 子控件
#   focus 路由, 所以 PASS 但装机 user 不显式 set focus → 失败。
#   修法: subclass s_hInput (Edit control) WndProc hook WM_KEYDOWN VK_RETURN /
#   VK_ESCAPE → SendMessage(parent, WM_KEYDOWN, ...) → OnKeyDown 触发。
#   Test 43/44 (新增) 模拟 input focus path,验证子类 bubble 路径。
#   WeaselServer.exe md5 跟 v0.19.0.57 一样(无 source 改动)。FluxingPhrasesDialog.exe
#   md5 不同 (Phase K5 source 改动 → 真 binary 重建)。installer md5 不同 (含新 binary)。
# v0.19.0.59 (A5 ship, sftp_sync 通用化 + 56 tests in git; source unchanged):
#   WeaselServer.exe + FluxingPhrasesDialog.exe 都是 MSBuild Win32 Release + x64
#   rebuild from v0.19.0.58 base, 装机器上 0 code change。installer md5 反映新 binary。
$expectedMd5 = '02876AA73BE5FCF33527F8E13904C178'  # v0.19.0.62 WeaselServer.exe (MSBuild Win32 Release, build 2026-07-24 14:10, Phase L 调整 1.2: QuickPanel 5→4 buttons + kPanelW 289→245)
$expectedFluxingMd5 = 'C969AEB31E24D9EE1468F8869150970D'  # v0.19.0.62 FluxingPhrasesDialog.exe (x64, source unchanged from v0.19.0.59, shipped binary unchanged)
$expectedInstallerMd5 = '64F9A3C8A9D47A03A81236EDD1761EB4'  # v0.19.0.62 NSIS installer (build 2026-07-24 14:10, MSBuild rebuild weasel.sln)
$expectedBuildTime = '2026-07-24 14:10:00'  # v0.19.0.62 NSIS timestamp

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
        $match = if ($h -eq $expectedMd5) { '<== MATCHES expected (v0.19.0.55 T019 hotfix)' } else { '<== MISMATCH (old binary or wrong build!)' }
        Write-Host "  $p"
        Write-Host "    md5=$h  size=$size  mtime=$mtime  $match"
    }
}
if (-not $foundAny) {
    Write-Host "  No WeaselServer.exe found in any of the standard install paths!"
}
Write-Host "  Expected: md5=$expectedMd5 (v0.19.0.54 T018 ship, out-of-process PhrasesDialog + Option B foreground fix)"
Write-Host ""

# ============================================================
# 2b. Installed FluxingPhrasesDialog.exe (out-of-process, NEW in v0.19.0.54)
# ============================================================
Write-Host "=== [2b] Installed FluxingPhrasesDialog.exe on disk (v0.19.0.54 NEW) ==="
$fluxingPaths = @(
    'D:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe',
    'C:\Program Files\fluxing\weasel\FluxingPhrasesDialog.exe',
    'C:\Program Files\Fluxing\weasel\FluxingPhrasesDialog.exe'
)
$foundFluxing = $false
foreach ($p in $fluxingPaths) {
    if (Test-Path $p) {
        $foundFluxing = $true
        $h = (Get-FileHash -Path $p -Algorithm MD5).Hash
        $size = (Get-Item $p).Length
        $mtime = (Get-Item $p).LastWriteTime
        $match = if ($h -eq $expectedFluxingMd5) { '<== MATCHES expected (v0.19.0.54)' } else { '<== MISMATCH!' }
        Write-Host "  $p"
        Write-Host "    md5=$h  size=$size  mtime=$mtime  $match"
    }
}
if (-not $foundFluxing) {
    Write-Host "  No FluxingPhrasesDialog.exe found! install.nsi v0.19.0.52+ miss the File directive."
}
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
Write-Host "  Installer v0.19.0.54 md5 expected: $expectedInstallerMd5"
Write-Host "  WeaselServer.exe md5 expected:     $expectedMd5 (build $expectedBuildTime)"
Write-Host "  FluxingPhrasesDialog.exe md5:      $expectedFluxingMd5 (T009 ship, unchanged in v0.19.0.54)"
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
Write-Host "    [2b] 如果 Install 缺 FluxingPhrasesDialog.exe → install.nsi v0.19.0.52+ miss (装 v0.19.0.49 老 installer)"
Write-Host "    [3] 如果 HKLM InstallDir != [2] path → registry 指向错位置"
Write-Host "    [4] 如果有 AppHangTransient weasel → 启动挂死"
Write-Host "    [5] newest file 是 7/22 20:35 → v0.19.0.54 装过; 是更早 → 没覆盖"
Write-Host ""
Write-Host "  Post-mortem copy: 把以上所有输出贴回 Fluxing Claude session"