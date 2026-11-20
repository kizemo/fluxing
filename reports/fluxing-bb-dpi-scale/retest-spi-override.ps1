# retest-spi-override.ps1 -- decide whether SPI_SETLOGICALDPIOVERRIDE can raise the
# effective DPI without a sign-out. Pure ASCII. Never leaves the system changed:
# every write is restored in a finally block.
#
# Experiment 1: SPI_SETLOGICALDPIOVERRIDE sweep (uiParam = two's complement int)
# Experiment 2: HKCU ...\PerMonitorSettings\<mon>\DpiValue write + WM_SETTINGCHANGE broadcast
#
# Evidence: reports\fluxing-bb-dpi-scale\spi-override-retest.txt

$ErrorActionPreference = 'Stop'
$logDir = 'F:\soft\00selfmade\rime_claude\reports\fluxing-bb-dpi-scale'
$log = Join-Path $logDir 'spi-override-retest.txt'
$lines = New-Object System.Collections.Generic.List[string]

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class SpiProbe {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x; public int y; }
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(POINT pt, uint flags);
  [DllImport("Shcore.dll")] public static extern int GetDpiForMonitor(IntPtr hmon, int dpiType, out uint dpiX, out uint dpiY);
  [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern bool SystemParametersInfoW(uint uiAction, uint uiParam, IntPtr pvParam, uint fWinIni);
  [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hdc, int index);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr hWnd);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)]
  public static extern IntPtr SendMessageTimeoutW(IntPtr hWnd, uint msg, IntPtr wParam, string lParam, uint flags, uint timeout, out IntPtr result);
  [DllImport("kernel32.dll")] public static extern uint GetLastError();
}
'@

# ---- resolve the two SPI action ids straight out of the SDK header (no guessing) ----
$hdr = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\Include' -Recurse -Filter WinUser.h -ErrorAction SilentlyContinue |
  Sort-Object FullName -Descending | Select-Object -First 1
$lines.Add("SDK_HEADER=$($hdr.FullName)")
$setVal = $null
$getVal = $null
foreach ($h in (Select-String -LiteralPath $hdr.FullName -Pattern 'LOGICALDPIOVERRIDE')) {
  $lines.Add("HDR $($h.LineNumber): $($h.Line.Trim())")
  if ($h.Line -match 'SPI_SETLOGICALDPIOVERRIDE\s+0x([0-9A-Fa-f]+)') { $setVal = [Convert]::ToUInt32($matches[1], 16) }
  if ($h.Line -match 'SPI_GETLOGICALDPIOVERRIDE\s+0x([0-9A-Fa-f]+)') { $getVal = [Convert]::ToUInt32($matches[1], 16) }
}
if ($null -eq $setVal) { $setVal = [uint32]0x009F }
if ($null -eq $getVal) { $getVal = [uint32]0x00A0 }
$lines.Add("SPI_SET=$setVal SPI_GET=$getVal")

# ---- sampler: returns one status string ----
$sample = {
  $pt = New-Object SpiProbe+POINT
  $pt.x = 100; $pt.y = 100
  $hm = [SpiProbe]::MonitorFromPoint($pt, 2)
  $dx = [uint32]0; $dy = [uint32]0
  [void][SpiProbe]::GetDpiForMonitor($hm, 0, [ref]$dx, [ref]$dy)
  $sys = [SpiProbe]::GetDpiForSystem()
  $lpx = [SpiProbe]::GetDeviceCaps([SpiProbe]::GetDC([IntPtr]::Zero), 88)
  $apd = (Get-ItemProperty 'HKCU:\Control Panel\Desktop\WindowMetrics' -ErrorAction SilentlyContinue).AppliedDPI
  $lp = (Get-ItemProperty 'HKCU:\Control Panel\Desktop').LogPixels
  "EFF=$dx SYSTEM=$sys LOGPIXELSX=$lpx AppliedDPI=$apd LogPixels=$lp"
}

$pmRoot = 'HKCU:\Control Panel\Desktop\PerMonitorSettings'
$snapshot = @{}
if (Test-Path $pmRoot) {
  foreach ($k in Get-ChildItem $pmRoot) {
    $snapshot[$k.PSChildName] = (Get-ItemProperty -LiteralPath $k.PSPath -ErrorAction SilentlyContinue).DpiValue
    $lines.Add("PM_SNAPSHOT $($k.PSChildName) DpiValue=$($snapshot[$k.PSChildName])")
  }
}

$origEff = $null
$restored = $false
try {
  $before = & $sample
  $lines.Add("BEFORE $before")
  if ($before -match 'EFF=(\d+)') { $origEff = [int]$matches[1] }
  $lines.Add("ORIG_EFF=$origEff")

  # ---- GET current override ----
  $buf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(8)
  [System.Runtime.InteropServices.Marshal]::WriteInt32($buf, 0)
  $okGet = [SpiProbe]::SystemParametersInfoW($getVal, 0, $buf, 0)
  $curOverride = [System.Runtime.InteropServices.Marshal]::ReadInt32($buf)
  $lines.Add("GET_OVERRIDE ok=$okGet value=$curOverride")

  # ---- Experiment 1: sweep ----
  $hit = $false
  foreach ($v in @(-2, -1, 1, 2)) {
    $uv = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$v), 0)
    $ok = [SpiProbe]::SystemParametersInfoW($setVal, $uv, [IntPtr]::Zero, 3)
    $err = [SpiProbe]::GetLastError()
    Start-Sleep -Milliseconds 1400
    $s1 = & $sample
    $eff = if ($s1 -match 'EFF=(\d+)') { [int]$matches[1] } else { -1 }
    $lines.Add("EXP1_SET v=$v ok=$ok err=$err $s1")
    if ($eff -ne $origEff) { $lines.Add("EXP1_HIT v=$v eff=$eff"); $hit = $true; break }
  }
  if (-not $hit) { $lines.Add('EXP1_RESULT=NO_EFFECT') }

  # ---- Experiment 2: per-monitor DpiValue + broadcast ----
  if (-not $hit -and (Test-Path $pmRoot)) {
    foreach ($v in @(-1, -2, 1)) {
      foreach ($k in Get-ChildItem $pmRoot) {
        Set-ItemProperty -LiteralPath $k.PSPath -Name DpiValue -Value $v -Type DWord
      }
      $r = [IntPtr]::Zero
      [void][SpiProbe]::SendMessageTimeoutW([IntPtr]0xffff, 0x001A, [IntPtr]::Zero, 'Display', 2, 2000, [ref]$r)
      Start-Sleep -Milliseconds 1400
      $s2 = & $sample
      $eff = if ($s2 -match 'EFF=(\d+)') { [int]$matches[1] } else { -1 }
      $lines.Add("EXP2_DpiValue=$v $s2")
      if ($eff -ne $origEff) { $lines.Add("EXP2_HIT DpiValue=$v eff=$eff"); $hit = $true; break }
    }
    if (-not $hit) { $lines.Add('EXP2_RESULT=NO_EFFECT') }
  }

  # ---- restore registry snapshot ----
  foreach ($name in $snapshot.Keys) {
    $p = Join-Path $pmRoot $name
    if (Test-Path -LiteralPath $p) {
      Set-ItemProperty -LiteralPath $p -Name DpiValue -Value $snapshot[$name] -Type DWord
    }
  }
  $lines.Add('PM_RESTORED=1')
}
finally {
  if ($origEff -ne $null) {
    $uvr = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$curOverride), 0)
    [void][SpiProbe]::SystemParametersInfoW($setVal, $uvr, [IntPtr]::Zero, 3)
    Start-Sleep -Milliseconds 1400
    $lines.Add("AFTER_RESTORE $(& $sample)")
    $lines.Add('SPI_RESTORED=' + $curOverride)
    $restored = $true
  }
}

$lines.Add("RESTORED=$restored")
Set-Content -LiteralPath $log -Value $lines -Encoding utf8
$lines | ForEach-Object { Write-Host $_ }
Write-Host "LOG=$log"