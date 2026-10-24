$files = @{
  'weasel.dll (Win32 TSF)' = 'F:\soft\00selfmade\rime_claude\output\weasel.dll'
  'weaselx64.dll (x64 TSF)' = 'F:\soft\00selfmade\rime_claude\output\weaselx64.dll'
  'WeaselServer.exe' = 'F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe'
  'WeaselDeployer.exe' = 'F:\soft\00selfmade\rime_claude\output\Win32\WeaselDeployer.exe'
}
foreach ($k in $files.Keys) {
  $p = $files[$k]
  if (Test-Path $p) {
    $h = (Get-FileHash $p -Algorithm MD5).Hash.ToLower()
    Write-Host "  $k = $h"
  } else {
    Write-Host "  $k = MISSING ($p)"
  }
}
