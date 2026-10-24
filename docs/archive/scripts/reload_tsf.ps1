$ErrorActionPreference = "Continue"
$dll = "D:\Program Files\fluxing\weasel\weasel.dll"

Stop-Process -Name WeaselServer -Force -ErrorAction SilentlyContinue
Stop-Process -Name ctfmon -Force -ErrorAction SilentlyContinue
Start-Sleep 3

# Re-register
regsvr32 /s /u $dll
Start-Sleep 2
regsvr32 /s $dll
Write-Output "Re-registered DLL"

# Verify MD5 of current DLL
$md5 = (Get-FileHash $dll -Algorithm MD5).Hash
Write-Output "Current DLL MD5: $md5"
if ($md5 -eq "CAAE7E5361AD1B5AF2A986E36AE5B3E1") {
    Write-Output "CORRECT: v0.21.0.1 patched DLL"
} else {
    Write-Output "WARNING: unexpected MD5"
}

# Restart WeaselServer
Start-Process "D:\Program Files\fluxing\weasel\WeaselServer.exe" -WindowStyle Hidden
Start-Sleep 5
if (Get-Process -Name WeaselServer -ErrorAction SilentlyContinue) {
    Write-Output "WeaselServer started OK"
    Write-Output ""
    Write-Output "=== NEXT: Please press Windows+L lock screen, then unlock ==="
    Write-Output "=== This forces TSF to reload the text service DLL ==="
}
