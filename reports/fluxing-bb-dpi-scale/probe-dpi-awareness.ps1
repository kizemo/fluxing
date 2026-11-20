# probe-dpi-awareness.ps1 -- pure ASCII. Batch BB follow-up: can THIS host read real DPI / real window rects?
#
# WHY
#   reports\fluxing-bb-dpi-scale\dpi-tier-probe-status.md section 8 found that the Agent shell reads
#   GetDpiForSystem / GetDeviceCaps(LOGPIXELSX) as 96 no matter what the real scale is, because
#   SetProcessDpiAwarenessContext() returns False there (awareness already fixed at process start).
#   Consequence NOT yet registered: an unaware process also gets *DPI virtualization* on
#   GetWindowRect / GetClientRect, i.e. every measurement is divided by (real_dpi / 96). Then
#   150% / 175% / 200% produce the SAME virtualized numbers and the pixel probe has zero
#   discriminating power. So before any tier evidence we must find a host that is really aware.
#
# WHAT IT PRINTS (all ASCII, one KEY=VALUE per line)
#   HOST / SET_PMV2 / THREAD_AWARENESS / GETDPIFORSYSTEM / LOGPIXELSX
#   TRAY_*        : taskbar rect + its window dpi (known system-DPI-aware window)
#   SERVER_*      : running WeaselServer pid / version
#   TOP_*         : its top level windows (class, rect, dpi)
#   ACCT_*        : the account window found by child id 1035 (rect, client rect)
#   CTRL_*        : child controls of interest (1022 known single line, 1035 note, 1006 status, 1010 close)
#
# EXIT: 0 always (this is a measurement, not a gate).

param(
    [string]$OutFile = '',
    [switch]$OpenAccount,
    [int]$WaitMs = 5000
)

$ErrorActionPreference = 'Stop'
$lines = New-Object System.Collections.Generic.List[string]
function L([string]$s) { $lines.Add($s) | Out-Null; Write-Host $s }

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public struct Aw7Rect { public int Left, Top, Right, Bottom; }

public static class Aw7Native {
    public delegate bool EnumProc(IntPtr h, IntPtr p);

    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
    [DllImport("user32.dll")] public static extern IntPtr GetThreadDpiAwarenessContext();
    [DllImport("user32.dll")] public static extern int GetAwarenessFromDpiAwarenessContext(IntPtr c);
    [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hdc, int index);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string name);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Aw7Rect r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out Aw7Rect r);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder sb, int n);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder sb, int n);
    [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);

    public static IntPtr[] TopWindows(uint pid) {
        List<IntPtr> list = new List<IntPtr>();
        EnumWindows(delegate(IntPtr h, IntPtr p) {
            uint w; GetWindowThreadProcessId(h, out w);
            if (w == pid) { list.Add(h); }
            return true;
        }, IntPtr.Zero);
        return list.ToArray();
    }

    public static IntPtr[] Children(IntPtr parent) {
        List<IntPtr> list = new List<IntPtr>();
        EnumChildWindows(parent, delegate(IntPtr h, IntPtr p) { list.Add(h); return true; }, IntPtr.Zero);
        return list.ToArray();
    }

    public static int CtrlId(IntPtr h) { return GetDlgCtrlID(h); }

    public static string ClassOf(IntPtr h) {
        StringBuilder sb = new StringBuilder(256);
        GetClassNameW(h, sb, 256);
        return sb.ToString();
    }

    public static string TextOf(IntPtr h) {
        StringBuilder sb = new StringBuilder(1024);
        GetWindowTextW(h, sb, 1024);
        return sb.ToString();
    }

    public static int[] RectOf(IntPtr h) {
        Aw7Rect r;
        if (!GetWindowRect(h, out r)) { return new int[] { 0, 0, 0, 0 }; }
        return new int[] { r.Left, r.Top, r.Right, r.Bottom };
    }

    public static int[] ClientOf(IntPtr h) {
        Aw7Rect r;
        if (!GetClientRect(h, out r)) { return new int[] { 0, 0, 0, 0 }; }
        return new int[] { r.Left, r.Top, r.Right, r.Bottom };
    }

    public static bool Post(IntPtr h, uint msg, IntPtr w, IntPtr l) { return PostMessageW(h, msg, w, l); }
    public static bool Alive(IntPtr h) { return IsWindow(h); }
}
'@

function RectStr([int[]]$r) {
    return ("{0},{1},{2},{3} size={4}x{5}" -f $r[0], $r[1], $r[2], $r[3], ($r[2] - $r[0]), ($r[3] - $r[1]))
}

# ---------- 1. can this host be aware at all? ----------
$proc = [System.Diagnostics.Process]::GetCurrentProcess()
L ("HOST=" + $proc.ProcessName + " pid=" + $PID)
L ("HOST_EXE=" + $proc.MainModule.FileName)
$setOk = [Aw7Native]::SetProcessDpiAwarenessContext([IntPtr](-4))
L ("SET_PMV2=" + $setOk)
$aw = [Aw7Native]::GetAwarenessFromDpiAwarenessContext([Aw7Native]::GetThreadDpiAwarenessContext())
L ("THREAD_AWARENESS=" + $aw + " (0=unaware 1=system 2=permonitor)")
L ("GETDPIFORSYSTEM=" + [Aw7Native]::GetDpiForSystem())
$dc = [Aw7Native]::GetDC([IntPtr]::Zero)
L ("LOGPIXELSX=" + [Aw7Native]::GetDeviceCaps($dc, 88))

# ---------- 2. known system-dpi-aware window (taskbar) ----------
$tray = [Aw7Native]::FindWindowW('Shell_TrayWnd', $null)
L ("TRAY_HWND=" + $tray)
if ($tray -ne [IntPtr]::Zero) {
    L ("TRAY_RECT=" + (RectStr ([Aw7Native]::RectOf($tray))))
    L ("TRAY_DPI=" + [Aw7Native]::GetDpiForWindow($tray))
}

# ---------- 3. the running service ----------
$srv = Get-Process -Name WeaselServer -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $srv) {
    L 'SERVER=NONE'
} else {
    $srvVer = '<n/a>'
    try { $srvVer = $srv.MainModule.FileVersionInfo.FileVersion } catch { $srvVer = '<denied>' }
    L ("SERVER_PID=" + $srv.Id + " VER=" + $srvVer)
    $tops = [Aw7Native]::TopWindows([uint32]$srv.Id)
    L ("SERVER_TOPCOUNT=" + $tops.Length)
    foreach ($h in $tops) {
        L ("TOP hwnd={0} class={1} rect={2} dpi={3} text={4}" -f $h, [Aw7Native]::ClassOf($h), (RectStr ([Aw7Native]::RectOf($h))), [Aw7Native]::GetDpiForWindow($h), ([Aw7Native]::TextOf($h) -replace '\s+', ' ').Substring(0, [math]::Min(24, ([Aw7Native]::TextOf($h) -replace '\s+', ' ').Length)))
    }

    # ---------- 4. optionally open the account window (WM_COMMAND 40024, batch AX recipe) ----------
    if ($OpenAccount) {
        foreach ($h in $tops) { [void][Aw7Native]::Post($h, 0x0111, [IntPtr]40024, [IntPtr]::Zero) }
        $acct = [IntPtr]::Zero
        $deadline = (Get-Date).AddMilliseconds($WaitMs)
        while ((Get-Date) -lt $deadline) {
            foreach ($h in [Aw7Native]::TopWindows([uint32]$srv.Id)) {
                foreach ($c in [Aw7Native]::Children($h)) {
                    if ([Aw7Native]::CtrlId($c) -eq 1035) { $acct = $h; break }
                }
                if ($acct -ne [IntPtr]::Zero) { break }
            }
            if ($acct -ne [IntPtr]::Zero) { break }
            Start-Sleep -Milliseconds 250
        }
        if ($acct -eq [IntPtr]::Zero) {
            L 'ACCT=NONE (no top level window of WeaselServer owns child id 1035)'
        } else {
            L ("ACCT_HWND=" + $acct + " class=" + [Aw7Native]::ClassOf($acct))
            L ("ACCT_RECT=" + (RectStr ([Aw7Native]::RectOf($acct))))
            $cl = [Aw7Native]::ClientOf($acct)
            L ("ACCT_CLIENT=" + $cl[2] + "x" + $cl[3] + " (virtualized? compare against expected 720x435 at 144dpi / 480x290 at 96dpi)")
            L ("ACCT_DPI=" + [Aw7Native]::GetDpiForWindow($acct))
            foreach ($id in 1022, 1035, 1006, 1010) {
                foreach ($c in [Aw7Native]::Children($acct)) {
                    if ([Aw7Native]::CtrlId($c) -eq $id) {
                        L ("CTRL id={0} rect={1} dpi={2}" -f $id, (RectStr ([Aw7Native]::RectOf($c))), [Aw7Native]::GetDpiForWindow($c))
                    }
                }
            }
            [void][Aw7Native]::Post($acct, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
            Start-Sleep -Milliseconds 800
            L ("ACCT_CLOSED=" + (-not [Aw7Native]::Alive($acct)))
        }
    }
}

if ($OutFile -ne '') {
    $dir = Split-Path -Parent $OutFile
    if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $lines | Set-Content -Path $OutFile -Encoding UTF8
    Write-Host ("OUT=" + $OutFile)
}
exit 0