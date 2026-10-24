$expected = @{
  'weasel.dll' = '7f7361608af6ec321d399076b44ddacf'
  'weaselx64.dll' = 'ea4dece5b4c76a01704204472c2aa5f4'
  'WeaselServer.exe' = '6cfa366b681a082f87fd2b6aeef8c76a'
  'WeaselDeployer.exe' = 'f81a5c4b7547266552898ec240357948'
}
$extractDir = 'C:\Users\Duanyi\AppData\Local\Temp\v071_rebuild'
$allMatch = $true
foreach ($name in $expected.Keys) {
  $path = Join-Path $extractDir $name
  if (Test-Path $path) {
    $h = (Get-FileHash $path -Algorithm MD5).Hash.ToLower()
    $exp = $expected[$name].ToLower()
    if ($h -eq $exp) {
      Write-Host "  MATCH: $name  $h"
    } else {
      Write-Host "  STALE: $name  actual=$h  expected=$exp"
      $allMatch = $false
    }
  } else {
    Write-Host "  MISSING: $name"
    $allMatch = $false
  }
}
if ($allMatch) {
  Write-Host "`nAll 4 binaries cross-source verify MATCH." -ForegroundColor Green
  exit 0
} else {
  Write-Host "`nSome binaries STALE — DO NOT SHIP." -ForegroundColor Red
  exit 1
}
