param([int]$WaitSec = 10)
# ASCII-only on purpose (lessons L181)
$ErrorActionPreference = 'Continue'
$OutDir = "F:\soft\00selfmade\rime_claude\tmp\spec099-probe"

Add-Type @"
using System;using System.Text;using System.Runtime.InteropServices;using System.Collections.Generic;
public class W7{
 public delegate bool EP(IntPtr h,IntPtr l);
 public static List<IntPtr> tops = new List<IntPtr>();
 public static List<IntPtr> kids = new List<IntPtr>();
 [DllImport("user32.dll",EntryPoint="GetWindowTextW",CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll",EntryPoint="GetClassNameW",CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EP cb,IntPtr p);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p,EP cb,IntPtr l);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte scan,uint flags,IntPtr extra);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f,int dx,int dy,uint d,IntPtr e);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int cmd);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int cx,int cy,uint flags);
 [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr hdc,uint flags);
 [DllImport("user32.dll",EntryPoint="SendMessageTimeoutW",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageTimeoutW(IntPtr h,uint m,IntPtr w,StringBuilder l,uint f,uint t,out IntPtr r);
 [DllImport("user32.dll")] public static extern IntPtr GetKeyboardLayout(uint tid);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int Left,Top,Right,Bottom;}
}
"@
try { [void][W7]::SetProcessDpiAwarenessContext([IntPtr](-4)) } catch { }

$log = New-Object System.Collections.ArrayList
function L([string]$s) { [void]$log.Add($s); Write-Host $s }

function Info([IntPtr]$h) {
  $c = New-Object System.Text.StringBuilder 256; [void][W7]::GetClassNameW($h,$c,256)
  $t = New-Object System.Text.StringBuilder 512; [void][W7]::GetWindowTextW($h,$t,512)
  [uint32]$p = 0; [void][W7]::GetWindowThreadProcessId($h,[ref]$p)
  $r = New-Object W7+RECT; [void][W7]::GetWindowRect($h,[ref]$r)
  [pscustomobject]@{ cls=$c.ToString(); title=$t.ToString(); pid=$p
    vis=[W7]::IsWindowVisible($h); rect="$($r.Left),$($r.Top),$($r.Right),$($r.Bottom)"
    w=($r.Right-$r.Left); ht=($r.Bottom-$r.Top); L2=$r.Left; T2=$r.Top }
}
function Tops() {
  [W7]::tops.Clear()
  $cb = [W7+EP]{ param($h,$l) [void][W7]::tops.Add($h); return $true }
  [void][W7]::EnumWindows($cb,[IntPtr]::Zero)
  return @([W7]::tops)
}
function Kids([IntPtr]$parent) {
  [W7]::kids.Clear()
  $cb = [W7+EP]{ param($h,$l) [void][W7]::kids.Add($h); return $true }
  [void][W7]::EnumChildWindows($parent,$cb,[IntPtr]::Zero)
  return @([W7]::kids)
}
Add-Type -AssemblyName System.Drawing
function ShotCrop([string]$name, [int]$x, [int]$y, [int]$w, [int]$h) {
  if ($w -le 0 -or $h -le 0) { return "<skip>" }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($x, $y, 0, 0, (New-Object System.Drawing.Size $w, $h)); $g.Dispose()
  $p = Join-Path $OutDir $name
  $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  return $p
}
function CapPrint([IntPtr]$h, [string]$name) {
  try {
    $r = New-Object W7+RECT; [void][W7]::GetWindowRect($h,[ref]$r)
    $w = $r.Right-$r.Left; $ht = $r.Bottom-$r.Top
    if ($w -le 0 -or $ht -le 0) { return "<skip>" }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc(); $ok = [W7]::PrintWindow($h,$hdc,2)
    $g.ReleaseHdc($hdc); $g.Dispose()
    $p = Join-Path $OutDir $name
    $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    return "$p ($ok)"
  } catch { return "ERR $($_.Exception.Message)" }
}
function VisSnap {
  $h = @{}
  foreach ($w in (Tops)) { $i = Info $w; if ($i.vis) { $h[$w.ToInt64()] = $i } }
  return $h
}
function ClickAt([int]$x, [int]$y) {
  [void][W7]::SetCursorPos($x,$y); Start-Sleep -Milliseconds 250
  [W7]::mouse_event(0x0002,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 90
  [W7]::mouse_event(0x0004,0,0,0,[IntPtr]::Zero)
}
function TypeNihao {
  foreach ($vk in @(0x4E,0x49,0x48,0x41,0x4F)) {
    [W7]::keybd_event([byte]$vk,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 110
    [W7]::keybd_event([byte]$vk,0,2,[IntPtr]::Zero); Start-Sleep -Milliseconds 110
  }
}

# ---- host + edit ----
$hostH = [IntPtr]::Zero; $editH = [IntPtr]::Zero
foreach ($w in (Tops)) { $i = Info $w; if ($i.title -eq "FluxingIMEProbeHost") { $hostH = $w } }
if ($hostH -eq [IntPtr]::Zero) { L "NO_HOST"; exit 2 }
foreach ($k in (Kids $hostH)) { $i = Info $k; if ($i.cls -imatch "EDIT") { $editH = $k } }
if ($editH -eq [IntPtr]::Zero) { L "NO_EDIT"; exit 3 }
$er = New-Object W7+RECT; [void][W7]::GetWindowRect($editH,[ref]$er)
$ecx = [int](($er.Left+$er.Right)/2); $ecy = [int](($er.Top+$er.Bottom)/2)

function Compose() {
  [void][W7]::ShowWindow($hostH, 9)
  [void][W7]::SetWindowPos($hostH, [IntPtr](-1), 600, 300, 1200, 600, 0x0040)
  Start-Sleep -Milliseconds 600
  [void][W7]::SetForegroundWindow($hostH)
  Start-Sleep -Milliseconds 400
  ClickAt $ecx $ecy
  Start-Sleep -Milliseconds 600
  [W7]::keybd_event(0x11,0,0,[IntPtr]::Zero); [W7]::keybd_event(0x41,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80; [W7]::keybd_event(0x41,0,2,[IntPtr]::Zero); [W7]::keybd_event(0x11,0,2,[IntPtr]::Zero); Start-Sleep -Milliseconds 200; [W7]::keybd_event(0x2E,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80; [W7]::keybd_event(0x2E,0,2,[IntPtr]::Zero); Start-Sleep -Milliseconds 400; TypeNihao
  Start-Sleep -Milliseconds 900
}
function GetComp {
  foreach ($w in (Tops)) { $i = Info $w; if ($i.cls -eq "SoPY_Comp" -and $i.vis) { return $w } }
  return [IntPtr]::Zero
}

L "=== PROBE7 start ==="
Compose
$compH = GetComp
if ($compH -eq [IntPtr]::Zero) { L "NO_COMP pass1 diagnostics:"; $fgw=[W7]::GetForegroundWindow(); [uint32]$fpid=0; $ftid=[W7]::GetWindowThreadProcessId($fgw,[ref]$fpid); L ("  fg=0x" + $fgw.ToInt64().ToString("X") + " pid=" + $fpid + " HKL=0x" + ([W7]::GetKeyboardLayout($ftid)).ToInt64().ToString("X")); foreach ($w in (Tops)) { $i = Info $w; if ($i.cls -match "SoPY|Sogou|WindowsForms") { L ("  win cls=" + $i.cls + " vis=" + $i.vis + " rect=[" + $i.rect + "] pid=" + $i.pid) } }; $sb2 = New-Object System.Text.StringBuilder 2048; $rr=[IntPtr]::Zero; [void][W7]::SendMessageTimeoutW($editH,0x000D,[IntPtr]2048,$sb2,2,1500,[ref]$rr); L ("  EDIT text=[" + $sb2.ToString() + "]"); Compose; $compH = GetComp; if ($compH -eq [IntPtr]::Zero) { L "NO_COMP pass2 abort"; exit 4 } }
$ci = Info $compH
L ("COMP rect=[{0}] {1}x{2}" -f $ci.rect,$ci.w,$ci.ht)
$cL=$ci.L2; $cT=$ci.T2; $cR=[int]$ci.rect.Split(',')[2]; $cB=[int]$ci.rect.Split(',')[3]
L ("COMP geometry L={0} T={1} R={2} B={3}" -f $cL,$cT,$cR,$cB)

$targets = @(
  @{n="chev0"; dx=-133; dy=71},
  @{n="hamb"; dx=-50; dy=71},
  @{n="hamb2"; dx=-44; dy=71},
  @{n="hamb3"; dx=-56; dy=71},
  @{n="chev"; dx=-133; dy=71},
  @{n="hamb4"; dx=-50; dy=63},
  @{n="hamb5"; dx=-50; dy=79},
  @{n="h6"; dx=-60; dy=71},
  @{n="h7"; dx=-40; dy=71},
  @{n="h8"; dx=-30; dy=71},
  @{n="h9"; dx=-20; dy=71},
  @{n="h10"; dx=-10; dy=71},
  @{n="h11"; dx=-90; dy=71},
  @{n="h12"; dx=-120; dy=71},
  @{n="h13"; dx=-160; dy=71},
  @{n="h14"; dx=-200; dy=71}
)

foreach ($tg in $targets) {
  $ch = GetComp
  if ($ch -eq [IntPtr]::Zero) {
    L ("### {0}: comp gone, recompose" -f $tg.n)
    Compose
    $ch = GetComp
    if ($ch -eq [IntPtr]::Zero) { L ("### {0}: still no comp" -f $tg.n); continue }
  }
  $ci = Info $ch
  $cL=$ci.L2; $cT=$ci.T2; $cR=[int]$ci.rect.Split(',')[2]; $cB=[int]$ci.rect.Split(',')[3]
  $px = $cR + [int]$tg.dx
  $py = $cT + [int]$tg.dy
  L ("### target {0} click=({1},{2})  [comp {3},{4},{5},{6}]" -f $tg.n,$px,$py,$cL,$cT,$cR,$cB)
  L ("pre = " + (ShotCrop ("p7-" + $tg.n + "-pre.png") ([Math]::Max(0,$cR-280)) ([Math]::Max(0,$cT-60)) 340 150))
  $before = VisSnap
  ClickAt $px $py
  Start-Sleep -Milliseconds 1400
  $after = VisSnap
  $ciA = Info $ch; L ("  comp_after rect=[" + $ciA.rect + "] " + $ciA.w + "x" + $ciA.ht)
  $newc = 0
  foreach ($k in $after.Keys) {
    if (-not $before.ContainsKey($k)) {
      $n = $after[$k]; $newc++
      L ("  NEW cls='{0}' title='{1}' pid={2} rect=[{3}] {4}x{5}" -f $n.cls,$n.title,$n.pid,$n.rect,$n.w,$n.ht)
      L ("   shot = " + (ShotCrop ("p7-" + $tg.n + "-new-" + $n.cls + ".png") ([Math]::Max(0,$n.L2-50)) ([Math]::Max(0,$n.T2-50)) ($n.w+100) ($n.ht+100)))
      L ("   print = " + (CapPrint ([IntPtr]$k) (Join-Path $OutDir ("p7-" + $tg.n + "-new-" + $n.cls + "-print.png"))))
    }
  }
  L ("  new_count=" + $newc)
  L ("  region = " + (ShotCrop ("p7-" + $tg.n + "-region.png") ([Math]::Max(0,$cR-280)) ([Math]::Max(0,$cT-60)) 340 150))
  $kids = @(Kids $ch)
  L ("  comp_children=" + $kids.Count)
  foreach ($k in $kids) { $i = Info $k; L ("   kid cls='{0}' vis={1} rect=[{2}]" -f $i.cls,$i.vis,$i.rect) }
  if ($newc -gt 0) { L "  HIT - stopping sweep"; break }
}

L ("desktop = " + (ShotCrop "p7-desktop.png" 0 0 3840 2160))
L "=== PROBE7 end ==="
$log | Set-Content -Path (Join-Path $OutDir "sogou-probe7.txt") -Encoding UTF8
