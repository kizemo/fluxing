# Fluxing BH-01: account dialog UI evidence probe (PURE ASCII source).
#   1) kIdSaveAccount (1019) visible and CHECKED by default in the logged-out layout.
#   2) clicking the email EDIT (1001) pops up the inline suggest LISTBOX (1099) that belongs to the
#      email box itself; picking a row refills the email box; a second click pops it again; clicking
#      another child hides it. Ids 1018 (kIdHistoryCombo) / 1034 (kIdHistoryLabel) must NOT be visible.
#   3) label/edit pairs share the same y/h and their ink centre lines coincide; no clipped row.
# Modes: SelfTest (load types + guards + dialog discovery, NO state change) | UiCheck (full).
# Exit codes: 0 ok | 2 guard / precondition refused | 3 verdict FAIL
# Ids verified against WeaselServer\Cloud\CloudAccountUi.h (kIdEmail=1001 ... kIdSaveAccount=1019).

param(
    [Parameter(Mandatory = $true)][ValidateSet('SelfTest', 'UiCheck')][string]$Mode,
    [string]$OutDir = 'F:\soft\00selfmade\rime_claude\reports\fluxing-bh-ui',
    [string]$Email = '25940969@qq.com',
    [int]$LogoutWaitSec = 120,
    [int]$ListWaitMs = 2500
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# ---------------- ids / constants ----------------
$K_EMAIL = 1001; $K_PASSWORD = 1002; $K_LOGIN = 1003; $K_LOGOUT = 1004; $K_STATUS = 1006
$K_CLOSE = 1010; $K_REGISTER = 1013; $K_HISTCOMBO = 1018; $K_SAVE = 1019
$K_EMAIL_LABEL = 1025; $K_PW_LABEL = 1026; $K_HISTLABEL = 1034; $K_KEYNOTE = 1035; $K_SUGGEST = 1099
$DIALOG_CLASS = 'FluxingCloudAccountDialog_v1'
$TRAY_COMMAND = 40024
$WM_COMMAND = 0x0111; $WM_CLOSE = 0x0010; $BM_CLICK = 0x00F5; $BM_GETCHECK = 0x00F0
$LB_GETCOUNT = 0x018B; $LB_GETTEXT = 0x0189; $LB_GETITEMHEIGHT = 0x01A1; $BST_CHECKED = 1

# ---------------- Win32 (block 1: verbatim reuse of the verified BF probe) ----------------
Add-Type -Name 'U' -Namespace 'W' -MemberDefinition @'
[DllImport("user32.dll")] public static extern IntPtr GetShellWindow();
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr l);
public delegate bool EnumWindowsProc(IntPtr h, IntPtr l);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int m);
[DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref P p);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, string l);
[DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, System.Text.StringBuilder s, int m);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
[DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
[DllImport("user32.dll", SetLastError=true)] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
[DllImport("user32.dll", SetLastError=true)] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
[DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
[DllImport("user32.dll")] public static extern bool AreDpiAwarenessContextsEqual(IntPtr a, IntPtr b);
[DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
[DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
[DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, int dc);
[DllImport("user32.dll", SetLastError=true)] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
[DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr dc, int idx);
[DllImport("gdi32.dll", CharSet=CharSet.Unicode)] public static extern bool GetTextFaceW(IntPtr dc, int c, System.Text.StringBuilder f);
[StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Rt, B; }
[StructLayout(LayoutKind.Sequential)] public struct P { public int X, Y; }

public static System.Collections.Generic.List<IntPtr> TopLevelOf(uint pid) {
    var list = new System.Collections.Generic.List<IntPtr>();
    EnumWindowsProc cb = (h, l) => {
        uint p2 = 0; GetWindowThreadProcessId(h, out p2);
        if (p2 == pid && h != GetShellWindow()) list.Add(h);
        return true;
    };
    EnumWindows(cb, IntPtr.Zero);
    GC.KeepAlive(cb);
    return list;
}
public static System.Collections.Generic.List<IntPtr> Children(IntPtr parent) {
    var list = new System.Collections.Generic.List<IntPtr>();
    IntPtr c = IntPtr.Zero;
    while (true) { c = FindWindowExW(parent, c, null, null); if (c == IntPtr.Zero) break; list.Add(c); }
    return list;
}
[DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindowExW(IntPtr p, IntPtr c, string cls, string win);
public static string Cls(IntPtr h) {
    var sb = new System.Text.StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString();
}
public static string Title(IntPtr h) {
    var sb = new System.Text.StringBuilder(512); GetWindowTextW(h, sb, 512); return sb.ToString();
}
'@

# ---------------- Win32 (block 2: geometry / desktop / capture) ----------------
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace W {
public static class G {
[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
[DllImport("user32.dll", SetLastError = true)] public static extern IntPtr GetThreadDesktop(uint t);
[DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
[DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool GetUserObjectInformationW(IntPtr h, int i, IntPtr buf, int n, out int need);
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
[StructLayout(LayoutKind.Sequential)] public struct RECT { public int left; public int top; public int right; public int bottom; }
[StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
public static string DesktopName() {
    uint tid = GetCurrentThreadId();
    IntPtr h = GetThreadDesktop(tid);
    if (h == IntPtr.Zero) { throw new Exception("GetThreadDesktop(" + tid + ") returned NULL, gle=" + Marshal.GetLastWin32Error()); }
    IntPtr buf = Marshal.AllocHGlobal(1024);
    try {
        int need = 0;
        if (!GetUserObjectInformationW(h, 2, buf, 1024, out need)) { throw new Exception("GetUserObjectInformationW failed, gle=" + Marshal.GetLastWin32Error()); }
        return Marshal.PtrToStringUni(buf);
    } finally { Marshal.FreeHGlobal(buf); }
}
static RECT WinRc(IntPtr h) {
    RECT r; if (!GetWindowRect(h, out r)) { throw new Exception("GetWindowRect failed, hwnd=" + h); }
    return r;
}
public static int[] WinSize(IntPtr h) { RECT r = WinRc(h); return new int[] { r.right - r.left, r.bottom - r.top }; }
public static int[] CltSize(IntPtr h) {
    RECT r; if (!GetClientRect(h, out r)) { throw new Exception("GetClientRect failed, hwnd=" + h); }
    return new int[] { r.right - r.left, r.bottom - r.top };
}
public static int[] ClientOffset(IntPtr h) {
    RECT wr = WinRc(h); POINT p = new POINT(); p.X = 0; p.Y = 0;
    if (!ClientToScreen(h, ref p)) { throw new Exception("ClientToScreen failed, hwnd=" + h); }
    return new int[] { p.X - wr.left, p.Y - wr.top };
}
public static int[] CtlRect(IntPtr hwnd, IntPtr c) {
    RECT wr = WinRc(hwnd); RECT r = WinRc(c); POINT p = new POINT(); p.X = 0; p.Y = 0;
    if (!ClientToScreen(hwnd, ref p)) { throw new Exception("ClientToScreen failed, hwnd=" + hwnd); }
    return new int[] { r.left - p.X, r.top - p.Y, r.right - r.left, r.bottom - r.top };
}
public static int[] ScreenPt(IntPtr hwnd, int cx, int cy) {
    POINT p = new POINT(); p.X = cx; p.Y = cy;
    if (!ClientToScreen(hwnd, ref p)) { throw new Exception("ClientToScreen failed, hwnd=" + hwnd); }
    return new int[] { p.X, p.Y };
}
public static bool Grab(IntPtr h, IntPtr hdc) { return PrintWindow(h, hdc, 2u); }
}
}
'@

# ---------------- Win32 (block 3: input + listbox + window state) ----------------
Add-Type -Name 'X' -Namespace 'W' -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
[DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
[DllImport("user32.dll")] public static extern bool GetCursorPos(out Q p);
[DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, IntPtr extra);
[DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
[StructLayout(LayoutKind.Sequential)] public struct Q { public int X, Y; }
'@

# ---------------- guards ----------------
$desktop = [W.G]::DesktopName()
if ($desktop -ne 'Default') { Write-Host "ABORT_DESKTOP=$desktop"; exit 2 }
[void][W.U]::SetProcessDpiAwarenessContext([IntPtr](-4))
$procs = @(Get-Process WeaselServer -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
if ($procs.Count -ne 1) { Write-Host "ABORT_SERVER_COUNT=$($procs.Count)"; exit 2 }
$srv = [uint32]$procs[0]
$ver = (Get-Process -Id $srv).MainModule.FileVersionInfo.FileVersion
if ($ver -ne '1.3.8.0') { Write-Host "ABORT_VERSION=$ver"; exit 2 }

# ---------------- helpers ----------------
$lines = [System.Collections.Generic.List[string]]::new()
function L($s) { $script:lines.Add([string]$s); Write-Host $s }
function Digest($text) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $b = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($text)); return (([System.BitConverter]::ToString($b)) -replace '-', '').Substring(0, 16) }
    finally { $sha.Dispose() }
}
function Find-Dialog {
    foreach ($h in [W.U]::TopLevelOf($script:srv)) { if ([W.U]::Cls($h) -eq $DIALOG_CLASS) { return $h } }
    return [IntPtr]::Zero
}
function Open-Dialog {
    $d = Find-Dialog
    if ($d -ne [IntPtr]::Zero) { return @{ hwnd = $d; openedByUs = $false } }
    foreach ($h in [W.U]::TopLevelOf($script:srv)) { [void][W.U]::PostMessageW($h, $WM_COMMAND, [IntPtr]$TRAY_COMMAND, [IntPtr]::Zero) }
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        $d = Find-Dialog
        if ($d -ne [IntPtr]::Zero) { Start-Sleep -Milliseconds 900; return @{ hwnd = $d; openedByUs = $true } }
    }
    return @{ hwnd = [IntPtr]::Zero; openedByUs = $false }
}
function Ctl-Map($hwnd) {
    $map = @{}
    foreach ($c in [W.U]::Children($hwnd)) { $id = [W.U]::GetDlgCtrlID($c); if ($id -ne 0) { $map[$id] = $c } }
    return $map
}
function Phys-Rect($hwnd, $c) {
    $cr = [W.G]::CtlRect($hwnd, $c)
    return @{ x = $cr[0]; y = $cr[1]; w = $cr[2]; h = $cr[3] }
}
function MulDiv96($v, $dpi) { return [int][Math]::Floor(($v * $dpi + 48) / 96) }
function Vis($map, $id) { return ($map.ContainsKey($id) -and [W.U]::IsWindowVisible($map[$id])) }
function Captured($hwnd, $path) {
    $ws = [W.G]::WinSize($hwnd)
    $cs = [W.G]::CltSize($hwnd)
    $co = [W.G]::ClientOffset($hwnd)
    if ($ws[0] -lt 50 -or $ws[1] -lt 50) { throw ('capture refused: window size ' + ($ws -join 'x')) }
    if ($cs[0] -lt 40 -or $cs[1] -lt 40) { throw ('capture refused: client size ' + ($cs -join 'x')) }
    $full = New-Object System.Drawing.Bitmap($ws[0], $ws[1], [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($full)
    $hdc = $g.GetHdc()
    $ok = [W.G]::Grab($hwnd, $hdc)
    $g.ReleaseHdc($hdc); $g.Dispose()
    if (-not $ok) { $full.Dispose(); throw 'PrintWindow failed' }
    $bmp = New-Object System.Drawing.Bitmap($cs[0], $cs[1], [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g2 = [System.Drawing.Graphics]::FromImage($bmp)
    $src = New-Object System.Drawing.Rectangle($co[0], $co[1], $cs[0], $cs[1])
    $dst = New-Object System.Drawing.Rectangle(0, 0, $cs[0], $cs[1])
    $g2.DrawImage($full, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
    $g2.Dispose(); $full.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    return $bmp
}
function Save-Crop($bmp, $x, $y, $w, $h, $path) {
    if ($x -lt 0) { $x = 0 }; if ($y -lt 0) { $y = 0 }
    if ($x + $w -gt $bmp.Width) { $w = $bmp.Width - $x }
    if ($y + $h -gt $bmp.Height) { $h = $bmp.Height - $y }
    if ($w -lt 1 -or $h -lt 1) { return $false }
    $crop = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($crop)
    $src = New-Object System.Drawing.Rectangle($x, $y, $w, $h)
    $dst = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
    $g.DrawImage($bmp, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose(); $crop.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $crop.Dispose()
    return $true
}
# Ink profile of ONE control: local modal background (a single global bg would mark the white EDIT
# client area as ink) + a "not a solid full-width row" cap that filters 3D borders / bevel lines.
function Crop-Ink($bmp, $r) {
    $x = $r.x; $y = $r.y; $w = $r.w; $h = $r.h
    if ($x -lt 0) { $x = 0 }; if ($y -lt 0) { $y = 0 }
    if ($x + $w -gt $bmp.Width) { $w = $bmp.Width - $x }
    if ($y + $h -gt $bmp.Height) { $h = $bmp.Height - $y }
    if ($w -lt 10 -or $h -lt 10) { return $null }
    $tally = @{}
    for ($yy = 0; $yy -lt $h; $yy += 2) {
        for ($xx = 2; $xx -lt ($w - 2); $xx += 2) {
            $p = $bmp.GetPixel($x + $xx, $y + $yy)
            $key = ((([int]$p.R) -shl 16) -bor (([int]$p.G) -shl 8) -bor ([int]$p.B))
            if ($tally.ContainsKey($key)) { $tally[$key] = $tally[$key] + 1 } else { $tally[$key] = 1 }
        }
    }
    $bgKey = 0; $bgN = -1
    foreach ($k in $tally.Keys) { if ($tally[$k] -gt $bgN) { $bgN = $tally[$k]; $bgKey = $k } }
    $bg = @((($bgKey -shr 16) -band 255), (($bgKey -shr 8) -band 255), ($bgKey -band 255))
    $counts = New-Object 'int[]' $h
    $samples = 0
    for ($yy = 0; $yy -lt $h; $yy++) {
        $ink = 0; $n = 0
        for ($xx = 3; $xx -lt ($w - 3); $xx += 2) {
            $p = $bmp.GetPixel($x + $xx, $y + $yy)
            $d = [Math]::Abs([int]$p.R - $bg[0]) + [Math]::Abs([int]$p.G - $bg[1]) + [Math]::Abs([int]$p.B - $bg[2])
            if ($d -gt 90) { $ink++ }
            $n++
        }
        if ($n -gt $samples) { $samples = $n }
        $counts[$yy] = $ink
    }
    $maxSolid = [int][Math]::Floor($samples * 0.8)
    $first = -1; $last = -1; $rows = 0
    for ($yy = 0; $yy -lt $h; $yy++) {
        if ($counts[$yy] -gt 0 -and $counts[$yy] -le $maxSolid) {
            if ($first -lt 0) { $first = $yy }
            $last = $yy; $rows++
        }
    }
    $center = -1
    if ($first -ge 0) { $center = [Math]::Round((($first + $last) / 2.0), 1) }
    return @{ bg = ($bg -join ','); first = $first; last = $last; rows = $rows; center = $center; h = $h; w = $w; samples = $samples; maxSolid = $maxSolid }
}
function Click-Screen($x, $y) {
    [void][W.X]::SetCursorPos($x, $y)
    Start-Sleep -Milliseconds 150
    [W.X]::mouse_event(0x0002, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 70
    [W.X]::mouse_event(0x0004, 0, 0, 0, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 300
}
function Click-Ctl($dlg, $r, $fx, $fy) {
    $cx = [int]($r.x + [Math]::Round($r.w * $fx))
    $cy = [int]($r.y + [Math]::Round($r.h * $fy))
    $sp = [W.G]::ScreenPt($dlg, $cx, $cy)
    Click-Screen $sp[0] $sp[1]
    return @{ x = $sp[0]; y = $sp[1] }
}
function Wait-Suggest($map, $want, $ms) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalMilliseconds -lt $ms) {
        $now = [W.X]::IsWindow($map[$K_SUGGEST]) -and [W.U]::IsWindowVisible($map[$K_SUGGEST])
        if ($now -eq $want) { return $true }
        Start-Sleep -Milliseconds 100
    }
    return $false
}

# ---------------- header facts ----------------
L "TS=$([DateTime]::Now.ToString('o'))"
L "MODE=$Mode"
L "DESKTOP=$desktop"
L "SRV_PID=$srv"
L "VER=$ver"
$PM_V1 = [IntPtr](-1)
L "SELF_CTX_IS_PIV2=$([W.U]::AreDpiAwarenessContextsEqual([W.U]::GetWindowDpiAwarenessContext([W.U]::GetShellWindow()), $PM_V1))"
$dc = [W.U]::GetDC([IntPtr]::Zero)
$sysDpi = [W.U]::GetDeviceCaps($dc, 88)
$face = New-Object System.Text.StringBuilder(64); [void][W.U]::GetTextFaceW($dc, 64, $face)
[void][W.U]::ReleaseDC([IntPtr]::Zero, $dc)
L "SYS_DPI=$sysDpi SCALE_PCT=$([int](100 * $sysDpi / 96)) STOCK_GUI_FACE=$($face.ToString())"

$fail = 0
$restoreCursor = $true
$cp = New-Object 'W.X+Q'
if ([W.X]::GetCursorPos([ref]$cp)) { $script:Cursor0 = @($cp.X, $cp.Y) } else { $script:Cursor0 = $null }

switch ($Mode) {

    'SelfTest' {
        $o = Open-Dialog
        if ($o.hwnd -eq [IntPtr]::Zero) { L 'ABORT=could_not_open_dialog'; $fail = 2; break }
        L "DLG=$($o.hwnd) OPENED_BY_US=$($o.openedByUs)"
        $dpiW = [W.U]::GetDpiForWindow($o.hwnd)
        $csz = [W.G]::CltSize($o.hwnd)
        L "WIN_DPI=$dpiW CLIENT_SIZE_PHYS=$($csz[0])x$($csz[1]) BASE=480x290 EXPECTED_CLIENT=$(MulDiv96 480 $dpiW)x$(MulDiv96 290 $dpiW)"
        $map = Ctl-Map $o.hwnd
        $all = @($map.Keys | Sort-Object)
        L "ALL_IDS=$($all -join ',')"
        L "SUGGEST_EXISTS=$($map.ContainsKey($K_SUGGEST)) SUGGEST_VISIBLE=$(Vis $map $K_SUGGEST)"
        L "SELFTEST=OK"
        if ($o.openedByUs) { [void][W.U]::PostMessageW($o.hwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero); L 'DIALOG_CLOSED=1' }
    }

    'UiCheck' {
        $o = Open-Dialog
        if ($o.hwnd -eq [IntPtr]::Zero) { L 'ABORT=could_not_open_dialog'; $fail = 2; break }
        $dlg = $o.hwnd
        L "DLG=$dlg OPENED_BY_US=$($o.openedByUs) TITLE_LEN=$([W.U]::Title($dlg).Length)"

        # ---- foreground precondition (a click that lands on nothing produces fake FAILs) ----
        [void][W.X]::SetWindowPos($dlg, [IntPtr](-1), 0, 0, 0, 0, 0x0013)
        [void][W.X]::SetForegroundWindow($dlg)
        Start-Sleep -Milliseconds 350
        [void][W.X]::SetWindowPos($dlg, [IntPtr](-2), 0, 0, 0, 0, 0x0013)
        Start-Sleep -Milliseconds 250
        $fg = [W.X]::GetForegroundWindow()
        L "FOREGROUND_OK=$($fg -eq $dlg) FG=$fg DLG=$dlg"
        if ($fg -ne $dlg) { L 'PRECOND=FOREGROUND_FAIL'; $fail = 2; break }

        $dpiW = [W.U]::GetDpiForWindow($dlg)
        $csz = [W.G]::CltSize($dlg)
        L "WIN_DPI=$dpiW CLIENT_SIZE_PHYS=$($csz[0])x$($csz[1]) BASE=480x290 EXPECTED_CLIENT=$(MulDiv96 480 $dpiW)x$(MulDiv96 290 $dpiW)"

        # ---- logout if the restored credential store put us in the logged-in layout ----
        $map = Ctl-Map $dlg
        $loggedIn = Vis $map $K_KEYNOTE
        L "LOGGED_IN_AT_START=$loggedIn"
        if ($loggedIn) {
            if (-not $map.ContainsKey($K_LOGOUT)) { L 'ABORT=no_logout_control'; $fail = 2; break }
            $t0 = Get-Date
            [void][W.U]::SendMessageW($map[$K_LOGOUT], $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
            L 'SENT=BM_CLICK_1004(kIdLogout)'
            $okLogout = $false
            while (((Get-Date) - $t0).TotalSeconds -lt $LogoutWaitSec) {
                Start-Sleep -Milliseconds 500
                $d2 = Find-Dialog
                if ($d2 -eq [IntPtr]::Zero) { continue }
                $m2 = Ctl-Map $d2
                if ((Vis $m2 $K_EMAIL) -and (-not (Vis $m2 $K_KEYNOTE))) {
                    $dlg = $d2; $map = $m2; $okLogout = $true
                    L "LOGOUT_OK_AFTER_SEC=$([Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)) DLG=$dlg DIALOG_REBUILT=$($dlg -ne $o.hwnd)"
                    break
                }
            }
            if (-not $okLogout) { L 'ABORT=logout_did_not_switch_layout'; $fail = 2; break }
        }

        $all = @($map.Keys | Sort-Object); $vis = @()
        foreach ($k in $all) { if ([W.U]::IsWindowVisible($map[$k])) { $vis += $k } }
        L "ALL_IDS=$($all -join ',')"
        L "VISIBLE_IDS=$($vis -join ',')"
        L "NO_LEGACY_HISTORY_WIDGET=$((-not ($vis -contains $K_HISTCOMBO)) -and (-not ($vis -contains $K_HISTLABEL)))"

        $bmp = Captured $dlg (Join-Path $OutDir 'evidence-ui-loggedout.png')
        L "PNG=evidence-ui-loggedout.png SIZE=$($bmp.Width)x$($bmp.Height)"

        # ================= C1: 1019 visible + checked by default =================
        $c1a = Vis $map $K_SAVE
        $c1b = $false; $c1text = ''
        if ($map.ContainsKey($K_SAVE)) {
            $st = [W.U]::SendMessageW($map[$K_SAVE], $BM_GETCHECK, [IntPtr]::Zero, [IntPtr]::Zero)
            $c1b = ([int64]$st -eq $BST_CHECKED)
            $c1text = [W.U]::Title($map[$K_SAVE])
            L "C1_SAVE_ACCOUNT check_state=$([int64]$st) expect=$BST_CHECKED"
            L "C1_SAVE_ACCOUNT text_len=$($c1text.Length) sha16=$(Digest $c1text)"
            $r = Phys-Rect $dlg $map[$K_SAVE]
            L "C1_SAVE_ACCOUNT rect x=$($r.x) y=$($r.y) w=$($r.w) h=$($r.h)"
            [void](Save-Crop $bmp $r.x $r.y $r.w $r.h (Join-Path $OutDir 'evidence-c1-save-account.png'))
            L 'PNG=evidence-c1-save-account.png'
        }
        L "C1_VERDICT=$(if ($c1a -and $c1b) { 'PASS' } else { 'FAIL' })"
        if (-not ($c1a -and $c1b)) { $fail = 3 }

        # ============ C2: click the account field to open the saved-account dropdown ==========
        # BH v4 rewrite in the HOST vocabulary (the v3 block was lost by a bad splice).
        # Two hard rules, both learned from the c2c_reopen=False investigation:
        #   (a) every synthetic click re-asserts the dialog foreground and prints FG_OK, so an
        #       undelivered click can never be read as "the product did not show the list";
        #   (b) c2d (hide-on-other) is judged only from the SHOWING state, so it is no longer a
        #       vacuous negative assertion satisfied by any hidden baseline.
        function C2-Text($h) {
            $sb = New-Object System.Text.StringBuilder(256)
            [void][W.X]::SendMessageW($h, 0x000D, [IntPtr]255, $sb)          # WM_GETTEXT
            return $sb.ToString()
        }
        function C2-ListItem($hl, $i) {
            $sb = New-Object System.Text.StringBuilder(256)
            [void][W.X]::SendMessageW($hl, $LB_GETTEXT, [IntPtr]$i, $sb)
            return $sb.ToString()
        }
        function C2-Count($hl) { return [int]([int64][W.U]::SendMessageW($hl, $LB_GETCOUNT, [IntPtr]::Zero, [IntPtr]::Zero) -band 0xFFFF) }
        function C2-ItemHeight($hl) { return [int]([int64][W.U]::SendMessageW($hl, $LB_GETITEMHEIGHT, [IntPtr]::Zero, [IntPtr]::Zero) -band 0xFFFF) }
        function C2-ClickCtl($tag, $r) {
            [void][W.X]::SetWindowPos($dlg, [IntPtr](-1), 0, 0, 0, 0, 0x0013)
            [void][W.X]::SetForegroundWindow($dlg)
            Start-Sleep -Milliseconds 250
            [void][W.X]::SetWindowPos($dlg, [IntPtr](-2), 0, 0, 0, 0, 0x0013)
            Start-Sleep -Milliseconds 150
            $fgok = ([W.X]::GetForegroundWindow() -eq $dlg)
            $pt = Click-Ctl $dlg $r 0.5 0.5
            L "C2_CLICK_$tag at=$($pt.x),$($pt.y) FG_OK=$fgok"
            if (-not $fgok) { $script:c2delivered = $false }
            return $pt
        }
        $script:c2delivered = $true
        $c2a = $false; $c2b = $false; $c2c = $false; $c2d = $false
        $c2d_judged = $false; $c2item0 = ''
        $he = $map[$K_EMAIL]; $hp = $map[$K_PASSWORD]
        $re0 = Phys-Rect $dlg $he
        L "C2_EMAIL_RECT x=$($re0.x) y=$($re0.y) w=$($re0.w) h=$($re0.h)"
        # The saved-account history file lives under the Rime user data dir; the v4 block
        # referenced $HistFile without ever defining it, which aborted the whole C2 segment.
        $HistFile = $null
        try {
            $ud = (Get-ItemProperty -Path 'HKCU:\Software\Fluxing\Weasel' -Name 'RimeUserDir' -ErrorAction Stop).RimeUserDir
        } catch { $ud = $null }
        if ($ud) { $HistFile = Join-Path ([string]$ud) 'fluxing-cloud-accounts.history' }
        if (-not $HistFile) { $HistFile = 'D:\Program Files\fluxing\user1\fluxing\fluxing-cloud-accounts.history' }
        L "C2_HISTORY_PATH=$HistFile"
        $c2hist = $false
        if (Test-Path $HistFile) {
            $hb = [System.IO.File]::ReadAllBytes($HistFile)
            $linesH = @([System.Text.Encoding]::UTF8.GetString($hb) -split "`r?`n" | Where-Object { $_.Length -gt 0 })
            if ($linesH.Count -ge 1) { $c2hist = $true }
            L "C2_HISTORY_FILE bytes=$($hb.Length) lines=$($linesH.Count) first_sha16=$(Digest $linesH[0])"
        } else { L 'C2_HISTORY_FILE=MISSING' }
        $lstExists = $map.ContainsKey($K_SUGGEST)
        $lst0 = $false
        if ($lstExists) { $lst0 = [W.U]::IsWindowVisible($map[$K_SUGGEST]) }
        L "C2_SUGGEST_BEFORE exists=$lstExists visible=$lst0"
        if (-not $c2hist) {
            L 'C2_VERDICT=NOT_JUDGED (no saved account in the history file: an empty dropdown is correct behaviour)'
            $fail = 4
        } else {
            if ($lst0) {
                [void](C2-ClickCtl 'reset_1006' (Phys-Rect $dlg $map[$K_STATUS]))
                L "C2_SUGGEST_AFTER_RESET visible=$([W.U]::IsWindowVisible($map[$K_SUGGEST]))"
            }
            $before = C2-Text $he
            L "C2_EMAIL_BEFORE len=$($before.Length)"
            [void](C2-ClickCtl 'email1' $re0)
            $c2a = Wait-Suggest $map $true $ListWaitMs
            $listShown = $c2a
            if ($c2a) {
                $hl = $map[$K_SUGGEST]
                $lr = Phys-Rect $dlg $hl
                $cnt = C2-Count $hl
                $ih = C2-ItemHeight $hl
                if ($cnt -ge 1) { $c2item0 = C2-ListItem $hl 0 }
                L "C2_SUGGEST_OPEN rect x=$($lr.x) y=$($lr.y) w=$($lr.w) h=$($lr.h)"
                L "C2_LIST_COUNT=$cnt ITEM0_LEN=$($c2item0.Length) ITEM0_SHA16=$(Digest $c2item0) ITEM_HEIGHT=$ih"
                $wantY = $re0.y + $re0.h + (MulDiv96 2 $dpiW)
                L "C2_GEOM want_y=$wantY got_y=$($lr.y) dy=$($lr.y - $wantY) want_x=$($re0.x) got_x=$($lr.x) dx=$($lr.x - $re0.x) want_w=$($re0.w) got_w=$($lr.w) dw=$($lr.w - $re0.w)"
                $c2geom_ok = (([Math]::Abs($lr.y - $wantY) -le 3) -and ([Math]::Abs($lr.x - $re0.x) -le 3) -and ([Math]::Abs($lr.w - $re0.w) -le 3))
                $ip = New-Object 'W.X+Q'; [void][W.X]::GetCursorPos([ref]$ip)
                $lp = (([int]($ip.Y - $lr.y)) -shl 16) -bor ([int]($ip.X - $lr.x))
                $rawHit = [int64][W.U]::SendMessageW($hl, 0x01A9, [IntPtr]::Zero, [IntPtr]$lp)
                $hitRow = [int]($rawHit -band 0xFFFF)
                $c2pos_ok = (($cnt -ge 1) -and ($hitRow -ge 0) -and ($hitRow -lt $cnt))
                L "C2_POS_IN_LIST=$c2pos_ok C2_GEOM_OK=$c2geom_ok hit_row=$hitRow of $cnt"
                $rowPt = [W.G]::ScreenPt($dlg, ($lr.x + [int]($lr.w / 2)), ($lr.y + $hitRow * $ih + [int]($ih / 2)))
                L "C2_ROW click=$($rowPt[0]),$($rowPt[1])"
                Click-Screen $rowPt[0] $rowPt[1]
                $c2b = Wait-Suggest $map $false $ListWaitMs
                $after = C2-Text $he
                L "C2_AFTER_PICK list_hidden=$c2b email_len=$($after.Length) email_sha16=$(Digest $after) filled_from_item=$($after -eq $c2item0)"
                $re1 = Phys-Rect $dlg $he
                [void](C2-ClickCtl 'email2_after_hide' $re1)
                $c2c = Wait-Suggest $map $true $ListWaitMs
                if ($c2c) {
                    $lr2 = Phys-Rect $dlg $map[$K_SUGGEST]
                    L "C2_SUGGEST_REOPEN rect x=$($lr2.x) y=$($lr2.y) w=$($lr2.w) h=$($lr2.h)"
                    $listShown = $true
                } else {
                    $ed2 = C2-Text $he
                    L "C2_REOPEN_MISS count=$(C2-Count $map[$K_SUGGEST]) email_len=$($ed2.Length) email_sha16=$(Digest $ed2) email_equals_item=$($ed2 -eq $c2item0) fg=$([W.X]::GetForegroundWindow())"
                }
            }
            if ($listShown) {
                $c2d_judged = $true
                [void](C2-ClickCtl 'status_1006' (Phys-Rect $dlg $map[$K_STATUS]))
                $c2d = Wait-Suggest $map $false $ListWaitMs
                L "C2_HIDE_ON_OTHER_CLICK judged=True hidden=$c2d"
            } else {
                L 'C2_HIDE_ON_OTHER_CLICK judged=False (the list was not showing at that point: not judged)'
            }
            $c2v = if ($c2a -and $c2b -and $c2c -and $c2d) { 'PASS' } else { 'FAIL' }
            L "C2_VERDICT=$c2v c2a_open=$c2a c2b_pick=$c2b c2c_reopen=$c2c c2d_hide_on_other=$c2d c2d_judged=$c2d_judged all_clicks_delivered=$script:c2delivered"
            if ($c2v -ne 'PASS') { $fail = 3 }
        }

        # ---- C3 front-matter: put TEXT in both EDITs, hide the dropdown, THEN capture ----
        # run-uicheck5.log root cause #2: this block cleared the email EDIT to '' BEFORE the
        # capture, so both EDIT crops had no ink (first=-1 rows=0) and the ink metric had
        # nothing to measure. Root cause #1: $c3Setup was never assigned at all in that
        # version (the C2/C3 block was spliced in from a standalone fragment), which forced
        # C3_VERDICT=NOT_JUDGED and exit 4 no matter what the crops said.
        $dlg2 = $dlg; $map2 = $map
        $he2 = $map2[$K_EMAIL]
        $hp2 = $map2[$K_PASSWORD]
        $seedTxt = 'probe@x'
        if ($c2item0.Length -ge 7) { $seedTxt = $c2item0.Substring(0, 7) }
        $seedPw = 'ProbePw7'
        [void][W.U]::SendMessageW($he2, 0x000C, [IntPtr]::Zero, $seedTxt)          # WM_SETTEXT
        [void][W.U]::SendMessageW($hp2, 0x000C, [IntPtr]::Zero, $seedPw)
        Start-Sleep -Milliseconds 400
        $seedTx = C2-Text $he2
        $seedPx = C2-Text $hp2
        # WM_SETTEXT fires EN_CHANGE, which by design re-opens the dropdown => hide it the same
        # way C2-d proved works: a click on the STATIC status label.
        [void](C2-ClickCtl 'c3_hide_list' (Phys-Rect $dlg2 $map2[$K_STATUS]))
        $c3hidden = Wait-Suggest $map2 $false $ListWaitMs
        $seedTx2 = C2-Text $he2
        $seedPx2 = C2-Text $hp2
        $seedList = $false
        if ($map2.ContainsKey($K_SUGGEST)) { $seedList = [W.U]::IsWindowVisible($map2[$K_SUGGEST]) }
        $c3d = ($seedTx.Length -eq $seedTxt.Length)
        L "C3_SEED want_len=$($seedTxt.Length) email_len=$($seedTx.Length) pwd_len=$($seedPx.Length) list_hidden_after_click=$c3hidden c3d_seed_roundtrip=$c3d"
        $re = Phys-Rect $dlg2 $map2[$K_EMAIL]
        $rp = Phys-Rect $dlg2 $map2[$K_PASSWORD]
        $c3Png = Join-Path $OutDir 'evidence-c3-seeded.png'
        $bmp3 = Captured $dlg2 $c3Png
        # run-uicheck8: c3_setup was still False although c2d now passes. Root cause is the probe,
        # not the product: an ES_PASSWORD EDIT refuses cross-process WM_GETTEXT and returns an empty
        # string, so the password half of this precondition ($seedPx2.Length -eq $seedPw.Length) can
        # never be satisfied by any build. The very same frame proves the characters did land
        # (C3_pwd_edit rows=16 of ink). So judge the password seeding by two channels that do work:
        # EM_GETTEXTLEN (control side) and the ink profile of the frame under measurement (pixel
        # side). Neither one reads the text back, so a WM_SETTEXT that genuinely failed would leave
        # both at zero => the guard keeps its discriminating power.
        $c3pwLenCtl = [int](([int64][W.U]::SendMessageW($hp2, 0x00C1, [IntPtr]::Zero, [IntPtr]::Zero)) -band 0xFFFF)
        $c3pwInk = $null
        if ($bmp3 -ne $null) { $c3pwInk = Crop-Ink $bmp3 (Phys-Rect $dlg2 $hp2) }
        $c3pwInkRows = if ($c3pwInk -ne $null) { [int]$c3pwInk.rows } else { -1 }
        $c3pwSeeded = (($seedPx2.Length -eq $seedPw.Length) -or ($c3pwLenCtl -eq $seedPw.Length) -or ($c3pwInkRows -ge 8))
        $c3Setup = $true
        if (-not ($c3hidden -and (-not $seedList))) { $c3Setup = $false }
        if (-not ($seedTx2.Length -eq $seedTxt.Length -and $c3pwSeeded)) { $c3Setup = $false }
        if ($bmp3 -eq $null -or -not (Test-Path $c3Png)) { $c3Setup = $false }
        L "C3_SEED_CAPTURE size=$($bmp3.Width)x$($bmp3.Height) PNG=evidence-c3-seeded.png email_after_hide_len=$($seedTx2.Length) pwd_after_hide_len=$($seedPx2.Length) pwd_EM_GETTEXTLEN=$c3pwLenCtl pwd_ink_rows=$c3pwInkRows pwd_seeded=$c3pwSeeded list_visible_at_capture=$seedList c3_setup=$c3Setup"
        # ================= C3: label/edit vertical centring + no clipped row =================
        $pairs = @(
            @{ name = 'email'; label = $K_EMAIL_LABEL; edit = $K_EMAIL },
            @{ name = 'pwd'; label = $K_PW_LABEL; edit = $K_PASSWORD }
        )
        $c3a = $true; $c3b = $true; $c3c = $true
        foreach ($pr2 in $pairs) {
            foreach ($side in @('label', 'edit')) {
                $id = $pr2[$side]
                if (-not $map.ContainsKey($id)) { L "C3_$($pr2.name)_$side MISSING_ID=$id"; $c3a = $false; continue }
                $r = Phys-Rect $dlg $map[$id]
                $ink = Crop-Ink $bmp3 $r
                if ($ink -eq $null) { L "C3_$($pr2.name)_$side CROP_TOO_SMALL"; $c3a = $false; continue }
                L "C3_$($pr2.name)_$side rect x=$($r.x) y=$($r.y) w=$($r.w) h=$($r.h) bg=$($ink.bg) first=$($ink.first) last=$($ink.last) center=$($ink.center) rows=$($ink.rows) samples=$($ink.samples) maxSolid=$($ink.maxSolid)"
                if ($ink.rows -lt 2) { $c3a = $false }
                if (-not (($ink.first -gt 1) -and ($ink.last -lt ($ink.h - 2)))) { $c3c = $false }
                $pr2[$side + '_c'] = $ink.center
                $pr2[$side + '_y'] = $r.y
                [void](Save-Crop $bmp3 $r.x $r.y $r.w $r.h (Join-Path $OutDir ("evidence-c3-" + $pr2.name + "-" + $side + ".png")))
            }
            if ($pr2.ContainsKey('label_c') -and $pr2.ContainsKey('edit_c')) {
                # Compare ink centres in CLIENT-AREA coordinates: label and edit no longer share
                # the same y (the EDIT is shrunk to the font line height), so the delta of the
                # in-rect centres alone would be meaningless.
                $ly = $pr2['label_y'] + $pr2['label_c']
                $ey = $pr2['edit_y'] + $pr2['edit_c']
                $d = [Math]::Abs($ly - $ey)
                L "C3_$($pr2.name)_CENTER_DELTA=$d label_c=$($pr2['label_c']) edit_c=$($pr2['edit_c']) label_y=$($pr2['label_y']) edit_y=$($pr2['edit_y']) label_abs=$ly edit_abs=$ey"
                if ($d -gt 4) { $c3b = $false }
            }
        }
        # positive control: 1003 button text is vertically centred => the metric must see that
        if ($map.ContainsKey($K_LOGIN)) {
            $r = Phys-Rect $dlg $map[$K_LOGIN]
            $ink = Crop-Ink $bmp3 $r
            if ($ink -ne $null) {
                L "C3_POSCTL_1003 center=$($ink.center) h=$($ink.h) first=$($ink.first) last=$($ink.last) rows=$($ink.rows)"
                if ($ink.center -gt 0 -and [Math]::Abs($ink.center - ($ink.h / 2)) -le 4) { L 'C3_POSCTL_OK=True' } else { L 'C3_POSCTL_OK=False' }
            }
        }
        # negative / descriptive: 1006 is a plain STATIC (no SS_CENTERIMAGE) => top aligned expected
        if ($map.ContainsKey($K_STATUS)) {
            $r = Phys-Rect $dlg $map[$K_STATUS]
            $ink = Crop-Ink $bmp3 $r
            if ($ink -ne $null) { L "C3_NEGCTL_1006 center=$($ink.center) h=$($ink.h) first=$($ink.first) last=$($ink.last) rows=$($ink.rows) (descriptive only)" }
        }
        L "C3_VERDICT=$(if (-not $c3Setup) { 'NOT_JUDGED' } elseif ($c3a -and $c3b -and $c3c) { 'PASS' } else { 'FAIL' }) c3a_has_ink=$c3a c3b_centered=$c3b c3c_not_clipped=$c3c c3_setup=$c3Setup"
        if ($c3Setup -and -not ($c3a -and $c3b -and $c3c)) { $fail = 3 }
        if (-not $c3Setup) { $fail = 4 }

        # restore the empty EDITs and release the seeded frame
        if ($c3Setup) {
            [void][W.U]::SendMessageW($map[$K_EMAIL], 0x000C, [IntPtr]::Zero, '')
            [void][W.U]::SendMessageW($map[$K_PASSWORD], 0x000C, [IntPtr]::Zero, '')
            L 'C3_SEED_CLEARED=1'
        }
        if (-not [object]::ReferenceEquals($bmp3, $bmp)) { $bmp3.Dispose() }

        $bmp.Dispose()
        L "BH_VERDICT=$(if ($fail -eq 0) { 'PASS' } else { 'FAIL' })"
        # deliberately LEAVE the dialog open and logged out: the relogin safety net in the same call
        # finds it and restores the logged-in layout.
        L "DIALOG_LEFT_OPEN=1 LOGGED_OUT_LEFT=1"
    }
}

if ($script:Cursor0 -ne $null) { [void][W.X]::SetCursorPos($script:Cursor0[0], $script:Cursor0[1]) }
L "EXIT=$fail"
$logPath = Join-Path $OutDir ('ui-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + $Mode + '.txt')
[System.IO.File]::WriteAllLines($logPath, $lines, [System.Text.UTF8Encoding]::new($false))
Write-Host "LOG=$logPath"
exit $fail
