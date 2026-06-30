# Fluxing cleanup script for users with stale 0.18.2.0 / 0.18.3.0 installs.
# Run as Administrator in PowerShell.

# 1. Kill any running Fluxing processes
Write-Host "Step 1: Killing running Fluxing processes..."
Get-Process -Name "WeaselServer", "WeaselDeployer", "WeaselSetup" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep 1

# 2. Run the existing uninstaller if it exists (from the most recent install)
$uninstallPaths = @(
    "C:\Program Files\fluxing\weasel\uninstall.exe",
    "C:\Program Files (x86)\fluxing\weasel\uninstall.exe",
    "D:\Program Files\fluxing\weasel\uninstall.exe",
    "D:\Program Files\weasel\uninstall.exe"
)
$uninstallRan = $false
foreach ($p in $uninstallPaths) {
    if (Test-Path $p) {
        Write-Host "Step 2: Running $p /S"
        & $p /S
        Start-Sleep 3
        $uninstallRan = $true
    }
}
if (-not $uninstallRan) {
    Write-Host "Step 2: No uninstaller found; will clean up manually"
}

# 3. Remove stale install directories
Write-Host "Step 3: Removing stale install directories..."
$dirs = @(
    "C:\Program Files\fluxing",
    "C:\Program Files (x86)\fluxing",
    "C:\Program Files\weasel",
    "D:\Program Files\fluxing",
    "D:\Program Files\weasel",
    "D:\Program Files\user1"
)
foreach ($d in $dirs) {
    if (Test-Path $d) {
        Write-Host "  Removing $d"
        Remove-Item -Recurse -Force $d -ErrorAction SilentlyContinue
    }
}

# 4. Clean up registry
Write-Host "Step 4: Cleaning up registry..."
Remove-Item "HKLM:\SOFTWARE\Fluxing\Weasel" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue
Remove-Item "HKCU:\Software\Fluxing\Weasel" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Fluxing" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Weasel" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\Rime" -ErrorAction SilentlyContinue
Remove-Item "HKLM:\SOFTWARE\WOW6432Node\Rime" -ErrorAction SilentlyContinue
Remove-ItemProperty "HKLM:\Software\Microsoft\Windows\CurrentVersion\Run" "WeaselServer" -ErrorAction SilentlyContinue

# 5. Verify cleanup
Write-Host ""
Write-Host "Step 5: Verifying cleanup..."
$stillThere = $false
$checkPaths = @(
    "C:\Program Files\fluxing",
    "C:\Program Files (x86)\fluxing",
    "D:\Program Files\fluxing",
    "D:\Program Files\weasel"
)
foreach ($p in $checkPaths) {
    if (Test-Path $p) {
        Write-Host "  WARNING: $p still exists" -ForegroundColor Yellow
        $stillThere = $true
    }
}
$reg = (Get-ItemProperty "HKLM:\SOFTWARE\WOW6432Node\Fluxing\Weasel" -ErrorAction SilentlyContinue).InstallDir
if ($reg) {
    Write-Host "  WARNING: HKLM registry still has InstallDir = $reg" -ForegroundColor Yellow
    $stillThere = $true
}

if ($stillThere) {
    Write-Host ""
    Write-Host "Some stale state remains. Please run this script as Administrator, or check Windows Event Log for the cause." -ForegroundColor Yellow
} else {
    Write-Host "Cleanup complete. You can now run fluxing-0.18.4.0-installer.exe." -ForegroundColor Green
}