# spec 099 P0-6 真机取证：起新构建 WeaselServer，触发快捷面板，量物理像素与 DPI。
# 只读测量 + 结束后停进程，不改任何持久状态。
param(
    [string]$ServerExe = 'F:\soft\00selfmade\rime_claude\output\Win32\WeaselServer.exe',
    [string]$OutDir    = 'F:\soft\00selfmade\rime_claude\tmp\spec099-probe'
)
$ErrorActionPreference = 'Continue'

Add-Type -AssemblyName System.Drawing

$sig = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class W {
    public delegate bool EnumProc(IntPtr h, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern int GetWindowLongW(IntPtr h, int i);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    public class Row {
        public IntPtr H; public string Cls; public string Txt; public bool Vis; public int ExStyle;
    }

    public static List<Row> Collect(uint pid, bool childrenToo) {
        var list = new List<Row>();
        EnumProc cb = (h, p) => {
            uint wpid; GetWindowThreadProcessId(h, out wpid);
            if (wpid == pid) {
                var sb = new StringBuilder(512);
                GetClassName(h, sb, 512);
                var tb = new StringBuilder(512);
                GetWindowTextW(h, tb, 512);
                var r = new Row { H = h, Cls = sb.ToString(), Txt = tb.ToString(),
                                  Vis = IsWindowVisible(h), ExStyle = GetWindowLongW(h, -20) };
                list.Add(r);
                if (childrenToo) {
                    EnumProc ccb = (ch, cp) => {
                        var csb = new StringBuilder(512); GetClassName(ch, csb, 512);
                        var ctb = new StringBuilder(512); GetWindowTextW(ch, ctb, 512);
                        list.Add(new Row { H = ch, Cls = csb.ToString(), Txt = ctb.ToString(),
                                           Vis = IsWindowVisible(ch), ExStyle = GetWindowLongW(ch, -20) });
                        return true;
                    };
                    EnumChildWindows(h, ccb, IntPtr.Zero);
                }
            }
            return true;
        };
        EnumWindows(cb, IntPtr.Zero);
        return list;
    }
}
'@
Add-Type -TypeDefinition $sig -Language CSharp

$report = New-Object System.Collections.ArrayList
function Rec($k, $v) { [void]$report.Add("$k = $v"); Write-Host "$k = $v" }

# 0) 先确认没有残留 server
Get-Process WeaselServer -ErrorAction SilentlyContinue | ForEach-Object {
    Rec "PREEXISTING" ("pid={0} path={1}" -f $_.Id, $_.Path)
}

# 1) 起新构建 server
$proc = Start-Process -FilePath $ServerExe -PassThru
Rec "STARTED_PID" $proc.Id
Start-Sleep -Seconds 4
$proc.Refresh()
if ($proc.HasExited) { Rec "FATAL" ("server exited early, code=" + $proc.ExitCode); $report | Set-Content -LiteralPath (Join-Path $OutDir 'measure-panel.txt') -Encoding UTF8; exit 2 }
$pidServer = [uint32]$proc.Id

# 2) 枚举该进程的窗口
$rows = [W]::Collect($pidServer, $true)
Rec "WINDOW_COUNT" $rows.Count
foreach ($r in $rows) {
    Rec "WIN" ("hwnd=0x{0:X} vis={1} ex=0x{2:X} cls='{3}' txt='{4}'" -f $r.H.ToInt64(), $r.Vis, $r.ExStyle, $r.Cls, $r.Txt)
}

# 3) 向疑似 IPC/服务窗口发 show(40020)
$cands = $rows | Where-Object { $_.Cls -match 'Weasel|IPC|Server|ServerImpl' -and $_.Cls -notmatch 'Tray|Notify' }
if (-not $cands) { $cands = $rows }
foreach ($c in $cands) {
    $ok = [W]::PostMessage($c.H, 0x0111, [IntPtr]40020, [IntPtr]::Zero)   # WM_COMMAND / ID_WEASELTRAY_QUICK_PANEL_SHOW
    Rec "POST_SHOW" ("hwnd=0x{0:X} cls='{1}' ok={2}" -f $c.H.ToInt64(), $c.Cls, $ok)
}
Start-Sleep -Seconds 2

# 4) 找面板窗口并量尺寸
$rows2 = [W]::Collect($pidServer, $true)
$panel = $rows2 | Where-Object { $_.Vis -and ($_.Cls -match 'QuickPanel|Fluxing' -or $_.Txt -match '快捷') }
Rec "PANEL_CANDIDATES" $panel.Count
$bmpSaved = $null
foreach ($p in $panel) {
    $rect = New-Object W+RECT
    [void][W]::GetWindowRect($p.H, [ref]$rect)
    $dpi = [W]::GetDpiForWindow($p.H)
    $w = $rect.Right - $rect.Left; $h = $rect.Bottom - $rect.Top
    Rec "PANEL" ("hwnd=0x{0:X} cls='{1}' dpi={2} rect={3},{4},{5},{6} size={7}x{8}" -f $p.H.ToInt64(), $p.Cls, $dpi, $rect.Left, $rect.Top, $rect.Right, $rect.Bottom, $w, $h)
    # 截面板区域图
    try {
        $bw = [Math]::Max($w, 1); $bh = [Math]::Max($h, 1)
        $bmp = New-Object System.Drawing.Bitmap $bw, $bh
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
        $f = Join-Path $OutDir ("panel-{0}.png" -f $p.H.ToInt64())
        $bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose(); $bmp.Dispose()
        $bmpSaved = $f
        Rec "PANEL_SHOT" ("{0} ({1} B)" -f $f, (Get-Item -LiteralPath $f).Length)
    } catch { Rec "PANEL_SHOT_ERR" $_.Exception.Message }
}

# 5) 全屏截图（带上下文）
try {
    Add-Type -AssemblyName System.Windows.Forms
    $vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bmp = New-Object System.Drawing.Bitmap $vs.Width, $vs.Height
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($vs.Left, $vs.Top, 0, 0, $bmp.Size)
    $f = Join-Path $OutDir 'desktop-with-panel.png'
    $bmp.Save($f, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Rec "DESKTOP_SHOT" ("{0} ({1} B)" -f $f, (Get-Item -LiteralPath $f).Length)
} catch { Rec "DESKTOP_SHOT_ERR" $_.Exception.Message }

# 6) 隐藏面板 + 停 server
foreach ($c in $cands) { [void][W]::PostMessage($c.H, 0x0111, [IntPtr]40021, [IntPtr]::Zero) }
Start-Sleep -Milliseconds 500
try { Stop-Process -Id $pidServer -Force -ErrorAction Stop; Rec "STOPPED" $pidServer } catch { Rec "STOP_ERR" $_.Exception.Message }

$report | Set-Content -LiteralPath (Join-Path $OutDir 'measure-panel.txt') -Encoding UTF8
Write-Output 'REPORT_DONE'