$ErrorActionPreference = "Stop"
$dllPath = "D:\Program Files\fluxing\weasel\weasel.dll"
$newDll = "F:\soft\00selfmade\rime_claude\output\weasel.dll"

# Kill all weasel processes
Stop-Process -Name "WeaselServer" -Force -ErrorAction SilentlyContinue

# Find processes holding our DLL
$holders = Get-Process | Where-Object { $_.Modules | Where-Object { $_.FileName -eq $dllPath } }
if ($holders) {
    foreach ($p in $holders) {
        Write-Output "Killing $($p.ProcessName) PID=$($p.Id)"
        Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    }
}

Start-Sleep 3

# Try to replace
try {
    Copy-Item $newDll $dllPath -Force -ErrorAction Stop
    Write-Output "DLL replaced successfully"
} catch {
    Write-Output "Copy failed, trying rename approach..."
    Remove-Item "$dllPath.bak" -Force -ErrorAction SilentlyContinue
    Rename-Item $dllPath "$dllPath.bak" -Force -ErrorAction SilentlyContinue
    Copy-Item $newDll $dllPath -Force
    Write-Output "DLL replaced via rename"
}

# Verify
$newMd5 = (Get-FileHash $newDll -Algorithm MD5).Hash
$depMd5 = (Get-FileHash $dllPath -Algorithm MD5).Hash
Write-Output "Source MD5: $newMd5"
Write-Output "Deploy MD5: $depMd5"

if ($newMd5 -eq $depMd5) {
    Write-Output "VERIFIED: DLL deployed correctly"
} else {
    Write-Output "WARNING: MD5 mismatch!"
}

Write-Output "Starting WeaselServer..."
Start-Process "D:\Program Files\fluxing\weasel\WeaselServer.exe" -WindowStyle Hidden
Start-Sleep 4
if (Get-Process -Name "WeaselServer" -ErrorAction SilentlyContinue) {
    Write-Output "WeaselServer running"
} else {
    Write-Output "WeaselServer failed to start"
}
