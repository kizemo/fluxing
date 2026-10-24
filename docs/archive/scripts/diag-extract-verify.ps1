$ErrorActionPreference = "Stop"
$installer = "F:\soft\00selfmade\rime_claude\output\archives\fluxing-0.19.0.66-installer.exe"
$extractDir = "F:\soft\00selfmade\rime_claude\output\verify-v066"
$sevenZip = "$env:ProgramFiles\7-Zip\7z.exe"
if (-not (Test-Path $sevenZip)) {
  $sevenZip = "C:\Program Files\7-Zip\7z.exe"
}

if (Test-Path $extractDir) {
  Remove-Item -Path $extractDir -Recurse -Force
}
New-Item -Path $extractDir -ItemType Directory | Out-Null

Write-Host "=== installer md5 ==="
Get-FileHash -Algorithm MD5 $installer | Select-Object Hash, Path | Format-Table -AutoSize
Write-Host "=== extracting installer ==="
& $sevenZip e $installer "-o$extractDir" -y | Out-Null
Write-Host "=== extracted files ==="
Get-ChildItem $extractDir | Where-Object { $_.Name -match 'weasel|Fluxing' -or $_.Extension -eq '.exe' } | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
Write-Host ""
Write-Host "=== binary md5 (extracted) ==="
Get-ChildItem $extractDir -Recurse -Include *.exe,*.dll | ForEach-Object {
  $h = Get-FileHash -Algorithm MD5 $_.FullName
  Write-Host ("  md5=" + $h.Hash + "  size=" + $_.Length + "  path=" + $_.Name)
}