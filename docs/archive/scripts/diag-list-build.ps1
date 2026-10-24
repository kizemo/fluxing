$procs = Get-Process | Where-Object {
  $_.ProcessName -in 'cmd','powershell','makensis','msbuild'
} | Select-Object Id, ProcessName, SessionId, StartTime
$procs | Format-Table -AutoSize
Write-Host ""
Write-Host "=== killing cmd 2492 + powershell 25316 ==="
Get-Process -Id 2492 -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process -Id 25316 -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
Write-Host ""
Write-Host "=== after kill ==="
$procs = Get-Process | Where-Object {
  $_.ProcessName -in 'cmd','powershell','makensis','msbuild'
} | Select-Object Id, ProcessName, SessionId
$procs | Format-Table -AutoSize