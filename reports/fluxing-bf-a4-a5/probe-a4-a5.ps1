# Fluxing BF-01: A-4 / A-5 account-dialog logged-out evidence probe (PURE ASCII source).
#   A-4 = control id 1035 (kIdKeyRotationNote) must be PRESENT BUT NOT VISIBLE when logged out.
#         ApplyLayout only ShowWindow(SW_HIDE) (CloudAccountDialog.cpp:1221-1269) => it never destroys controls,
#         so "not enumerated" would be the WRONG discriminator; visibility is the right one.
#   A-5 = bottom whitespace of the logged-out layout, quantified by control geometry + per-row ink profile.
# Modes: Baseline (logged-in positive control) | Logout | Relogin
# Ids verified against WeaselServer\Cloud\CloudAccountUi.h:36-74 (kIdLogout=1004, kIdLogin=1003, kIdResend=1015).
# Exit codes: 0 ok | 2 guard refused | 3 verdict FAIL

param(
    [Parameter(Mandatory = $true)][ValidateSet('Baseline', 'Logout', 'Relogin')][string]$Mode,
    [string]$OutDir = 'F:\soft\00selfmade\rime_claude\reports\fluxing-bf-a4-a5',
    [string]$PwFile = 'F:\soft\00selfmade\rime_claude\tmp\AN-register-qq.txt',
    [string]$Email = '25940969@qq.com',
    [int]$LogoutWaitSec = 90,
    [int]$ReloginWaitSec = 180
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

# ---------------- expected layout tables (96 dpi base) ----------------
$LOGIN_TABLE = [ordered]@{
    1027 = @(12, 14, 456, 20); 1020 = @(12, 38, 456, 22); 1004 = @(12, 70, 140, 28); 1016 = @(160, 70, 140, 28)
    1017 = @(12, 108, 456, 20); 1022 = @(12, 132, 456, 18); 1035 = @(12, 156, 456, 36); 1006 = @(12, 198, 456, 40)
    1010 = @(368, 250, 100, 28)
}
$LOGOUT_TABLE = [ordered]@{
    1025 = @(12, 14, 70, 20); 1001 = @(88, 10, 186, 26); 1034 = @(278, 14, 48, 20); 1018 = @(330, 10, 138, 26)
    1026 = @(12, 48, 70, 20); 1002 = @(88, 44, 244, 26); 1019 = @(88, 78, 244, 20); 1003 = @(12, 108, 140, 28)
    1013 = @(160, 108, 140, 28); 1021 = @(308, 108, 160, 28); 1006 = @(12, 148, 456, 40); 1010 = @(368, 250, 100, 28)
}
$K_NOTE = 1035; $K_STATUS = 1006; $K_CLOSE = 1010; $K_LOGOUT = 1004; $K_LOGIN = 1003; $K_EMAIL = 1001; $K_PASSWORD = 1002
$DIALOG_CLASS = 'FluxingCloudAccountDialog_v1'
$TRAY_COMMAND = 40024          # resource.h:71 ID_TRAY_CLOUD_ACCOUNT
$WM_COMMAND = 0x0111; $WM_CLOSE = 0x0010; $WM_SETTEXT = 0x000C; $BM_CLICK = 0x00F5

# ---------------- Win32 (single compilation unit) ----------------
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
[DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
[DllImport("user32.dll", SetLastError=true)] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
[DllImport("user32.dll", SetLastError=true)] public static extern IntPtr GetThreadDesktop(uint tid);
[DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Unicode)] public static extern bool GetUserObjectInformationW(IntPtr h, int idx, IntPtr pv, int n, out int needed);
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
public static string DesktopName() {
    IntPtr h = GetThreadDesktop(unchecked((uint)System.Threading.Thread.CurrentThread.ManagedThreadId));
    if (h == IntPtr.Zero) { throw new Exception("GetThreadDesktop returned NULL, gle=" + System.Runtime.InteropServices.Marshal.GetLastWin32Error()); }
    IntPtr buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(1024);
    try {
        int need = 0;
        if (!GetUserObjectInformationW(h, 2, buf, 1024, out need)) { throw new Exception("GetUserObjectInformationW failed, gle=" + System.Runtime.InteropServices.Marshal.GetLastWin32Error()); }
        return System.Runtime.InteropServices.Marshal.PtrToStringUni(buf);
    } finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }
}
'@

# ---------------- geometry / desktop helper (W.G) ----------------
# 2026-11-21 修正：W.U 里的 DesktopName() 把 Thread.ManagedThreadId 当 Win32 线程 id ⇒ GetThreadDesktop 必返 NULL（gle=0），
# 桌面守卫假红。这里改用 GetCurrentThreadId()；同时把 RECT/POINT 的 P/Invoke 封在 C# 内部、对外只返回 int[]，
# 避免 PowerShell 侧用 New-Object 'W.U+R' 实例化嵌套结构体（不可靠）。
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
public static int[] ClientOffset(IntPtr h) {   // 客户区原点在「窗口位图」里的物理像素偏移
    RECT wr = WinRc(h); POINT p = new POINT(); p.X = 0; p.Y = 0;
    if (!ClientToScreen(h, ref p)) { throw new Exception("ClientToScreen failed, hwnd=" + h); }
    return new int[] { p.X - wr.left, p.Y - wr.top };
}
public static int[] CtlRect(IntPtr hwnd, IntPtr c) {   // {clientX, clientY, w, h} 物理像素
    RECT wr = WinRc(hwnd); RECT r = WinRc(c); POINT p = new POINT(); p.X = 0; p.Y = 0;
    if (!ClientToScreen(hwnd, ref p)) { throw new Exception("ClientToScreen failed, hwnd=" + hwnd); }
    return new int[] { r.left - p.X, r.top - p.Y, r.right - r.left, r.bottom - r.top };
}
public static bool Grab(IntPtr h, IntPtr hdc) { return PrintWindow(h, hdc, 2u); }   // PW_RENDERFULLCONTENT
}
}
'@

# ---------------- guards ----------------
$desktop = [W.G]::DesktopName()
if ($desktop -ne 'Default') { Write-Host "ABORT_DESKTOP=$desktop"; exit 2 }
[void][W.U]::SetProcessDpiAwarenessContext([IntPtr](-4))   # PERF_SETTING_HOST / PerMonitorV2

$procs = @(Get-Process WeaselServer -ErrorAction SilentlyContinue | ForEach-Object { $_.Id })
if ($procs.Count -ne 1) { Write-Host "ABORT_SERVER_COUNT=$($procs.Count)"; exit 2 }
$srv = [uint32]$procs[0]
$ver = (Get-Process -Id $srv).MainModule.FileVersionInfo.FileVersion
if ($ver -ne '1.3.8.0') { Write-Host "ABORT_VERSION=$ver"; exit 2 }

# ---------------- helpers ----------------
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
    # [W.G]::CtlRect 返回的已是「客户区相对」物理像素 {x, y, w, h}
    $cr = [W.G]::CtlRect($hwnd, $c)
    return @{ x = $cr[0]; y = $cr[1]; w = $cr[2]; h = $cr[3] }
}
function MulDiv96($v, $dpi) { return [int][Math]::Floor(($v * $dpi + 48) / 96) }   # matches C++ AccountScale
function Captured($hwnd, $path) {
    # 抓整窗 -> 裁出「客户区」再落盘，保证读图坐标与 Phys-Rect(客户区相对) 同一原点
    $ws = [W.G]::WinSize($hwnd)
    $cs = [W.G]::CltSize($hwnd)
    $co = [W.G]::ClientOffset($hwnd)
    $script:LastCapture = @{ win_w = $ws[0]; win_h = $ws[1]; off_x = $co[0]; off_y = $co[1] }
    if ($ws[0] -lt 50 -or $ws[1] -lt 50) { throw ('capture refused: window size ' + ($ws -join 'x')) }
    if ($cs[0] -lt 40 -or $cs[1] -lt 40) { throw ('capture refused: client size ' + ($cs -join 'x')) }
    $full = New-Object System.Drawing.Bitmap($ws[0], $ws[1], [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($full)
    $hdc = $g.GetHdc()
    $ok = [W.G]::Grab($hwnd, $hdc)     # PW_RENDERFULLCONTENT
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
function Row-Ink($bmp, $bg) {
    $w = $bmp.Width; $h = $bmp.Height
    $counts = New-Object 'int[]' $h
    for ($y = 0; $y -lt $h; $y++) {
        $ink = 0
        for ($x = 6; $x -lt ($w - 3); $x += 3) {
            $p = $bmp.GetPixel($x, $y)
            if (([Math]::Abs([int]$p.R - [int]$bg[0]) + [Math]::Abs([int]$p.G - [int]$bg[1]) + [Math]::Abs([int]$p.B - [int]$bg[2])) -gt 90) { $ink++ }
        }
        $counts[$y] = $ink
    }
    return ,$counts
}

$lines = [System.Collections.Generic.List[string]]::new()
function L($s) { $script:lines.Add([string]$s); Write-Host $s }
function Digest($text) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { $b = $sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($text)); return (([System.BitConverter]::ToString($b)) -replace '-', '').Substring(0, 16) }
    finally { $sha.Dispose() }
}

# ---------------- header facts ----------------
L "TS=$([DateTime]::Now.ToString('o'))"
L "MODE=$Mode"
L "DESKTOP=$desktop"
L "SRV_PID=$srv"
L "VER=$ver"
$PM_PI = [IntPtr](-4); $PM_V1 = [IntPtr](-1)
L "SELF_CTX_IS_PIV2=$([W.U]::AreDpiAwarenessContextsEqual([W.U]::GetWindowDpiAwarenessContext([W.U]::GetShellWindow()), $PM_V1))"
$dc = [W.U]::GetDC([IntPtr]::Zero)
$sysDpi = [W.U]::GetDeviceCaps($dc, 88)     # LOGPIXELSX
$face = New-Object System.Text.StringBuilder(64); [void][W.U]::GetTextFaceW($dc, 64, $face)
[void][W.U]::ReleaseDC([IntPtr]::Zero, $dc)
L "SYS_DPI=$sysDpi SCALE_PCT=$([int](100 * $sysDpi / 96)) STOCK_GUI_FACE=$($face.ToString())"

$fail = 0

switch ($Mode) {

    'Baseline' {
        $o = Open-Dialog
        if ($o.hwnd -eq [IntPtr]::Zero) { L 'ABORT=could_not_open_dialog'; $fail = 3; break }
        $dlg = $o.hwnd
        L "DLG=$dlg OPENED_BY_US=$($o.openedByUs) TITLE_LEN=$([W.U]::Title($dlg).Length)"
        $dpiW = [W.U]::GetDpiForWindow($dlg)
        L "WIN_DPI=$dpiW WIN_CTX_IS_PIV2=$([W.U]::AreDpiAwarenessContextsEqual([W.U]::GetWindowDpiAwarenessContext($dlg), $PM_PI))"
        $wsz = [W.G]::WinSize($dlg); $csz = [W.G]::CltSize($dlg)
        $wantW = MulDiv96 480 $dpiW; $wantH = MulDiv96 290 $dpiW
        L "WIN_SIZE_PHYS=$($wsz[0])x$($wsz[1]) CLIENT_SIZE_PHYS=$($csz[0])x$($csz[1]) BASE=480x290 DPIW=$dpiW EXPECTED_CLIENT=${wantW}x${wantH}"
        L ("CLIENT_SIZE_RULE=" + $(if ($csz[0] -eq $wantW -and $csz[1] -eq $wantH) { 'MATCH_BASE_TABLE_X_DPI_div_96' } else { 'DIFFER' }))
        $map = Ctl-Map $dlg
        L "CHILD_WITH_ID_COUNT=$($map.Count)"
        $all = @(); $vis = @()
        foreach ($k in ($map.Keys | Sort-Object)) { $all += $k; if ([W.U]::IsWindowVisible($map[$k])) { $vis += $k } }
        L "ALL_IDS=$($all -join ',')"
        L "VISIBLE_IDS=$($vis -join ',')"
        # positive control for A-4: the harness MUST be able to see 1035 while logged in
        $noteOk = ($all -contains $K_NOTE) -and ($vis -contains $K_NOTE)
        L "A4_POSITIVE_CONTROL_1035_SEEN=$noteOk"
        if (-not $noteOk) { L 'VERDICT_BASELINE=FAIL'; $fail = 3 } else { L 'VERDICT_BASELINE=PASS' }
        # logged-in visible set must equal the logged-in layout table
        $expect = @($LOGIN_TABLE.Keys | Sort-Object)
        $missing = @($expect | Where-Object { $vis -notcontains $_ }); $extra = @($vis | Where-Object { $expect -notcontains $_ })
        L "EXPECT_SET=$($expect -join ',') MISSING=$($missing -join ',') EXTRA=$($extra -join ',') SET_MATCH=$(($missing.Count -eq 0) -and ($extra.Count -eq 0))"
        $bmp = Captured $dlg (Join-Path $OutDir 'evidence-baseline.png')
        L "PNG=evidence-baseline.png SIZE=$($bmp.Width)x$($bmp.Height)"
        $p = $bmp.GetPixel(6, 6); $bg = @([int]$p.R, [int]$p.G, [int]$p.B)
        L "BG_RGB=$($bg -join ',')"
        if ($map.ContainsKey($K_NOTE)) {
            $nr = Phys-Rect $dlg $map[$K_NOTE]
            L "NOTE_PHYS x=$($nr.x) y=$($nr.y) w=$($nr.w) h=$($nr.h)"
            [void](Save-Crop $bmp $nr.x $nr.y $nr.w $nr.h (Join-Path $OutDir 'evidence-baseline-note.png'))
            L 'PNG=evidence-baseline-note.png'
        }
        $ink = Row-Ink $bmp $bg
        $rows = @(); for ($y = 0; $y -lt $ink.Count; $y++) { if ($ink[$y] -gt 3) { $rows += $y } }
        L "ROWS_WITH_INK=$($rows.Count) FIRST=$($rows[0]) LAST=$($rows[-1])"
        $bmp.Dispose()
        if ($o.openedByUs) { [void][W.U]::PostMessageW($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero); L 'DIALOG_CLOSED=1' }
    }

    { $_ -in @('Logout', 'Relogin') } {
        $o = Open-Dialog
        if ($o.hwnd -eq [IntPtr]::Zero) { L 'ABORT=could_not_open_dialog'; $fail = 3; break }
        $dlg = $o.hwnd; $opened = $o.openedByUs
        L "DLG=$dlg OPENED_BY_US=$opened"
        $map = Ctl-Map $dlg

        if ($Mode -eq 'Logout') {
            # guard: only log out when we can positively see the logged-in note (else no discriminator)
            if (-not $map.ContainsKey($K_NOTE)) { L 'GUARD=ABORT_1035_NOT_ENUMERATED'; $fail = 2; break }
            if (-not [W.U]::IsWindowVisible($map[$K_NOTE])) { L 'GUARD=ABORT_ALREADY_LOGGED_OUT'; $fail = 2; break }
            L 'GUARD_1035_VISIBLE_BEFORE_LOGOUT=True'
            [void][W.U]::SendMessageW($map[$K_LOGOUT], $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
            L 'SENT=BM_CLICK_1004(kIdLogout)'
            $waited = 0
            while ($waited -lt $LogoutWaitSec) {
                Start-Sleep -Seconds 1; $waited++
                $map = Ctl-Map $dlg
                if (-not [W.U]::IsWindowVisible($map[$K_NOTE])) { L "NOTE_HIDDEN_AFTER_SEC=$waited"; break }
            }
            if ([W.U]::IsWindowVisible($map[$K_NOTE])) { L 'LOGOUT_DID_NOT_SWITCH_LAYOUT=1'; $fail = 3; break }
        }
        else {
            if ($map.ContainsKey($K_NOTE) -and [W.U]::IsWindowVisible($map[$K_NOTE])) { L 'GUARD=ALREADY_LOGGED_IN_skip' ; $fail = 0 }
            else {
                # password: read from file, two independent statements of it must agree; never printed
                $raw = Get-Content -Raw -Encoding UTF8 $PwFile
                $m1 = [regex]::Match($raw, "FLUXING_PROBE_PASSWORD\s*=\s*'([^']+)'")
                if (-not $m1.Success) { $m1 = [regex]::Match($raw, 'FLUXING_PROBE_PASSWORD\s*=\s*"([^"]+)"') }
                if (-not $m1.Success) { $m1 = [regex]::Match($raw, 'FLUXING_PROBE_PASSWORD\s*=\s*([^\r\n"''\s]+)') }
                if (-not $m1.Success) { L 'ABORT=password_marker_not_found'; $fail = 2; break }
                $pw = $m1.Groups[1].Value.Trim()
                $alt = ''
                foreach ($ln in ($raw -split "`r?`n")) { $mm = [regex]::Match($ln, '([!-~]{8,})\s*$'); if ($mm.Success -and $mm.Groups[1].Value -clike 'Fluxing-*') { $alt = $mm.Groups[1].Value; break } }
                L "PW_SOURCE_A_len=$($pw.Length) sha16=$(Digest $pw)"
                L "PW_SOURCE_B_len=$($alt.Length) sha16=$(Digest $alt)"
                if ($alt -and ($alt -cne $pw)) { L 'ABORT=password_sources_disagree'; $fail = 2; break }
                L "PW_CROSSCHECK=$([bool]($alt -ne '' -and $alt -eq $pw))"
                [void][W.U]::SendMessageW($map[$K_EMAIL], $WM_SETTEXT, [IntPtr]::Zero, $Email)
                [void][W.U]::SendMessageW($map[$K_PASSWORD], $WM_SETTEXT, [IntPtr]::Zero, $pw)
                L "FILLED=email_len=$($Email.Length)+password ctl_email=$($map.ContainsKey($K_EMAIL)) ctl_pw=$($map.ContainsKey($K_PASSWORD))"
                [void][W.U]::SendMessageW($map[$K_LOGIN], $BM_CLICK, [IntPtr]::Zero, [IntPtr]::Zero)
                L 'SENT=BM_CLICK_1003(kIdLogin)'
                $waited = 0; $reopened = $false
                $restored = $false
                while ($waited -lt $ReloginWaitSec) {
                    Start-Sleep -Seconds 1; $waited++
                    $d2 = Find-Dialog
                    if ($d2 -eq [IntPtr]::Zero) {
                        $oo = Open-Dialog
                        if ($oo.hwnd -eq [IntPtr]::Zero) { continue }
                        $d2 = $oo.hwnd; $reopened = $true
                    }
                    $m2 = Ctl-Map $d2
                    if ($m2.ContainsKey($K_NOTE) -and [W.U]::IsWindowVisible($m2[$K_NOTE])) { L "RELOGIN_OK_AFTER_SEC=$waited dialogReopenedDuringWait=$reopened"; $dlg = $d2; $map = $m2; $opened = $oo.openedByUs -or $opened; $restored = $true; break }
                }
                if (-not $restored) { L 'ABORT=login_did_not_restore_logged_in_layout'; $fail = 3; break }
            }
        }

        Start-Sleep -Milliseconds 1500
        $dpiW = [W.U]::GetDpiForWindow($dlg)
        L "WIN_DPI=$dpiW SCALE_PCT=$([int](100 * $dpiW / 96))"
        $all = @(); $vis = @(); $hid = @()
        foreach ($k in ($map.Keys | Sort-Object)) { $all += $k; if ([W.U]::IsWindowVisible($map[$k])) { $vis += $k } else { $hid += $k } }
        L "ALL_IDS=$($all -join ',')"
        L "VISIBLE_IDS=$($vis -join ',')"
        L "HIDDEN_IDS=$($hid -join ',')"

        $table = $LOGOUT_TABLE
        if ($Mode -eq 'Relogin') { $table = $LOGIN_TABLE }
        $expect = @($table.Keys | Sort-Object)
        $missing = @($expect | Where-Object { $vis -notcontains $_ }); $extra = @($vis | Where-Object { $expect -notcontains $_ })
        L "EXPECT_SET=$($expect -join ',')"
        L "MISSING=$($missing -join ',') EXTRA=$($extra -join ',')"
        L "SET_MATCH=$(($missing.Count -eq 0) -and ($extra.Count -eq 0))"

        if ($Mode -eq 'Logout') {
            $present = $all -contains $K_NOTE
            $hidden = $hid -contains $K_NOTE
            $absentFromVis = -not ($vis -contains $K_NOTE)
            L "A4_1035_PRESENT=$present A4_1035_HIDDEN=$hidden A4_1035_NOT_IN_VISIBLE_SET=$absentFromVis"
            $v = ($present -and $hidden -and $absentFromVis -and ($missing.Count -eq 0) -and ($extra.Count -eq 0))
            L "A4_VERDICT=$(if ($v) { 'PASS' } else { 'FAIL' })"
            if (-not $v) { $fail = 3 }
        }
        else {
            $v = ($vis -contains $K_NOTE) -and ($missing.Count -eq 0) -and ($extra.Count -eq 0)
            L "A4_RESTORE_VERDICT=$(if ($v) { 'PASS' } else { 'FAIL' })"
            if (-not $v) { $fail = 3 }
        }

        # geometry (ADR 0068 whole-table scaling): strict MulDiv(v,dpi,96)
        $geoFail = 0
        foreach ($k in $expect) {
            if (-not $map.ContainsKey($k)) { L "G_$k=ABSENT"; $geoFail++; continue }
            $got = Phys-Rect $dlg $map[$k]; $b = $table[$k]
            $ex = @(MulDiv96 $b[0] $dpiW, MulDiv96 $b[1] $dpiW, MulDiv96 $b[2] $dpiW, MulDiv96 $b[3] $dpiW)
            $okg = ($got.x -eq $ex[0] -and $got.y -eq $ex[1] -and $got.w -eq $ex[2] -and $got.h -eq $ex[3])
            if (-not $okg) { $geoFail++ }
            L "G_$k exp=$($ex -join ',') got=$($got.x),$($got.y),$($got.w),$($got.h) ok=$okg"
        }
        L "GEOMETRY_MISMATCH_COUNT=$geoFail"

        # A-5 whitespace, quantified
        $contentBottomBase = if ($Mode -eq 'Logout') { 188 } else { 238 }
        $contentBottom = MulDiv96 $contentBottomBase $dpiW
        $closeTop = MulDiv96 $table[$K_CLOSE][1] $dpiW
        $closeBottom = MulDiv96 ($table[$K_CLOSE][1] + $table[$K_CLOSE][3]) $dpiW
        $bmp = Captured $dlg (Join-Path $OutDir "evidence-$($Mode.ToLower()).png")
        $custBottom = $bmp.Height
        L "PNG=evidence-$($Mode.ToLower()).png SIZE=$($bmp.Width)x$($bmp.Height)"
        $p = $bmp.GetPixel(6, 6); $bg = @([int]$p.R, [int]$p.G, [int]$p.B)
        L "BG_RGB=$($bg -join ',')"
        $ink = Row-Ink $bmp $bg
        $rows = @(); for ($y = 0; $y -lt $ink.Count; $y++) { if ($ink[$y] -gt 3) { $rows += $y } }
        L "ROWS_TOTAL=$($ink.Count) ROWS_WITH_INK=$($rows.Count) FIRST_INK=$($rows[0]) LAST_INK=$($rows[-1])"
        L "A5_contentBottom=$contentBottom closeTop=$closeTop closeBottom=$closeBottom clientBottom=$custBottom"
        $g1 = @($rows | Where-Object { $_ -ge $contentBottom -and $_ -lt $closeTop })
        $g2 = @($rows | Where-Object { $_ -ge $closeBottom -and $_ -lt $custBottom })
        L "A5_gap1=$(( $closeTop - $contentBottom ))px inkRowsInGap1=$($g1.Count)"
        L "A5_gap2=$(( $custBottom - $closeBottom ))px inkRowsInGap2=$($g2.Count)"
        L "A5_trailing_blank_below_last_ink=$($custBottom - 1 - $rows[-1])"
        [void](Save-Crop $bmp 0 $contentBottom $bmp.Width ($custBottom - $contentBottom) (Join-Path $OutDir "evidence-$($Mode.ToLower())-bottom.png"))
        L "PNG=evidence-$($Mode.ToLower())-bottom.png"
        $bmp.Dispose()
        if ($opened) { [void][W.U]::PostMessageW($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero); L 'DIALOG_CLOSED=1' }
        L "VERDICT_$($Mode.ToUpper())=$(if ($fail -eq 0) { 'PASS' } else { 'FAIL' })"
    }
}

$stamp = (Get-Date).ToString('yyyyMMdd-HHmmss')
$dst = Join-Path $OutDir "$($Mode.ToLower())-$stamp.txt"
Set-Content -Path $dst -Value ($lines -join "`r`n") -Encoding UTF8
L "WROTE=$dst"
L "EXIT=$fail"
exit $fail
