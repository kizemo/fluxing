#Requires -Version 5.1
<#
.SYNOPSIS
  Batch AZ / release 1.3.6 - checklist item A-1..A-3 numeric judge for the
  key-rotation note (kIdKeyRotationNote = 1035) in the LOGGED-IN account page.

.DESCRIPTION
  Question: is the 37-fullwidth-char note text clipped by its 456 x 36 box?
  Method (no hardcoded non-ASCII literals - PS 5.1 reads BOM-less files as ANSI):
    1. open the account window of the ALREADY INSTALLED WeaselServer by posting
       WM_COMMAND(40024) to the server's own top-level windows (proven path, batch AX)
    2. locate child control id 1035, read its real window text from the binary
    3. measure that text with the control's OWN font on the control's OWN DC:
         DT_CALCRECT | DT_SINGLELINE  -> single-line width + line height
         DT_CALCRECT | DT_WORDBREAK   -> wrapped width + wrapped height
         DT_CALCRECT | DT_EDITCONTROL | DT_WORDBREAK -> second estimate
       compare wrapped height against the control's real device height
    4. positive control: the sibling note id 1032 (23 chars, h=18) is documented
       as verified single-line; if the same method says 1 line for it, the
       method has teeth on this machine/DPI/font.
    5. pixel evidence: PrintWindow of the note control and of the whole dialog,
       plus a per-row ink profile to see how many text bands actually painted
       and whether ink touches the bottom edge.
  Writes report.json + note-text.txt + PNGs to -OutDir and prints a short summary.
#>
[CmdletBinding()]
param(
    [string]$OutDir = '',
    [int]$NoteId = 1035,
    [int]$ControlIdOfKnownSingleLine = 1032,
    [switch]$KeepOpen
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrEmpty($OutDir)) {
    $OutDir = Join-Path $env:TEMP ('fluxing-a1-note-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

if (-not ('FluxingNote' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class FluxingNote {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    public delegate bool EnumProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h, EnumProc cb, IntPtr p);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern IntPtr SendMessageW(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
    [DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr h, IntPtr dc);
    [DllImport("gdi32.dll")] public static extern IntPtr SelectObject(IntPtr dc, IntPtr obj);
    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)] public static extern int GetTextFaceW(IntPtr dc, int n, StringBuilder buf);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int DrawTextW(IntPtr dc, string text, int count, ref RECT r, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr h);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDC(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetThreadDesktop(int dwThreadId);
    [DllImport("kernel32.dll")] public static extern int GetCurrentThreadId();
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern bool GetUserObjectInformationW(IntPtr hObj, int nIndex, IntPtr pvInfo, int nLength, out int lpnLengthNeeded);

    const uint DT_TOP = 0x00000000;
    const uint DT_LEFT = 0x00000000;
    const uint DT_CALCRECT = 0x00000400;
    const uint DT_WORDBREAK = 0x00000010;
    const uint DT_EDITCONTROL = 0x00002000;
    const uint DT_SINGLELINE = 0x00000020;
    const uint DT_NOPREFIX = 0x00000800;

    public static int[] Rect(IntPtr h) {
        RECT r; if (!GetWindowRect(h, out r)) return null;
        return new int[] { r.Left, r.Top, r.Right, r.Bottom };
    }
    public static int[] Client(IntPtr h) {
        RECT r; if (!GetClientRect(h, out r)) return null;
        return new int[] { r.Left, r.Top, r.Right, r.Bottom };
    }
    public static string Class(IntPtr h) {
        var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString();
    }
    public static string Text(IntPtr h) {
        int n = GetWindowTextLength(h);
        var sb = new StringBuilder(n + 2);
        GetWindowTextW(h, sb, sb.Capacity);
        return sb.ToString();
    }
    public static bool Visible(IntPtr h) { return IsWindowVisible(h); }
    public static int CtrlId(IntPtr h) { return GetDlgCtrlID(h); }
    public static uint Dpi(IntPtr h) { return GetDpiForWindow(h); }
    public static IntPtr Font(IntPtr h) { return SendMessageW(h, 0x0031 /*WM_GETFONT*/, IntPtr.Zero, IntPtr.Zero); }

    /// <summary>
    /// Returns { singleW, singleH, wrapW, wrapH, editWrapW, editWrapH, faceIdLen, fontIsStock }
    /// measured with the control's own font on the control's own DC.
    /// </summary>
    public static int[] Measure(IntPtr hwnd, string text) {
        int singleW = -1, singleH = -1, wrapW = -1, wrapH = -1, ewW = -1, ewH = -1, faceLen = -1;
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return new int[] { singleW, singleH, wrapW, wrapH, ewW, ewH, faceLen, 0 };
        IntPtr font = Font(hwnd);
        IntPtr old = SelectObject(dc, font);
        try {
            var fbuf = new StringBuilder(64);
            faceLen = GetTextFaceW(dc, fbuf.Capacity, fbuf);
            RECT a = new RECT(); a.Left = 0; a.Top = 0; a.Right = 0; a.Bottom = 0;
            DrawTextW(dc, text, -1, ref a, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT | DT_TOP);
            singleW = a.Right - a.Left; singleH = a.Bottom - a.Top;
            RECT b = new RECT(); b.Left = 0; b.Top = 0; b.Right = 0; b.Bottom = 0;
            DrawTextW(dc, text, -1, ref b, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_LEFT | DT_TOP);
            wrapW = b.Right - b.Left; wrapH = b.Bottom - b.Top;
            RECT c = new RECT(); c.Left = 0; c.Top = 0; c.Right = 0; c.Bottom = 0;
            DrawTextW(dc, text, -1, ref c, DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX | DT_LEFT | DT_TOP);
            ewW = c.Right - c.Left; ewH = c.Bottom - c.Top;
        } finally {
            SelectObject(dc, old);
            ReleaseDC(hwnd, dc);
        }
        return new int[] { singleW, singleH, wrapW, wrapH, ewW, ewH, faceLen, 0 };
    }

    /// <summary>Width of the text wrapped into a fixed box width (what the static really gets).</summary>
    public static int[] MeasureInBox(IntPtr hwnd, string text, int boxW) {
        int[] res = new int[] { -1, -1 };
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return res;
        IntPtr font = Font(hwnd);
        IntPtr old = SelectObject(dc, font);
        try {
            RECT r = new RECT(); r.Left = 0; r.Top = 0; r.Right = boxW; r.Bottom = 0;
            DrawTextW(dc, text, -1, ref r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_LEFT | DT_TOP);
            res[0] = r.Right - r.Left; res[1] = r.Bottom - r.Top;
        } finally {
            SelectObject(dc, old);
            ReleaseDC(hwnd, dc);
        }
        return res;
    }

    public static string FaceName(IntPtr hwnd) {
        IntPtr dc = GetDC(hwnd);
        if (dc == IntPtr.Zero) return "";
        IntPtr font = Font(hwnd);
        IntPtr old = SelectObject(dc, font);
        var sb = new StringBuilder(64);
        GetTextFaceW(dc, sb.Capacity, sb);
        SelectObject(dc, old);
        ReleaseDC(hwnd, dc);
        return sb.ToString();
    }

    public static IntPtr[] ChildrenOf(IntPtr parent) {
        var list = new System.Collections.Generic.List<IntPtr>();
        EnumChildWindows(parent, (h, p) => { list.Add(h); return true; }, IntPtr.Zero);
        return list.ToArray();
    }
    public static IntPtr[] TopOf(uint pid) {
        var list = new System.Collections.Generic.List<IntPtr>();
        EnumWindows((h, p) => {
            uint q; GetWindowThreadProcessId(h, out q);
            if (q == pid) list.Add(h);
            return true;
        }, IntPtr.Zero);
        return list.ToArray();
    }
}
'@
}

Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue

$WM_COMMAND = 0x0111
$WM_CLOSE = 0x0010
$ID_MENU_CLOUD_ACCOUNT = 40024
$CLS_ACCOUNT = 'FluxingCloudAccountDialog_v1'

$rep = [ordered]@{
    started_at = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
    outdir     = $OutDir
    steps      = @()
    verdict    = 'UNDECIDED'
}
function Step($name, $detail, $state = 'INFO') {
    $rep.steps += [ordered]@{ step = $name; state = $state; detail = "$detail" }
    Write-Host ("[{0}] {1} :: {2}" -f $state, $name, $detail)
}

# ---- desktop guard (never trust a YingDao virtual desktop capture) --------
# Real check: this thread's desktop object name (UOI_NAME = index 2).
# PrintWindow / GetPixel evidence taken on any other desktop is not the screen.
$hDesk = [FluxingNote]::GetThreadDesktop([FluxingNote]::GetCurrentThreadId())
$deskBuf = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(512)
$deskNeeded = 0
$deskOk = [FluxingNote]::GetUserObjectInformationW($hDesk, 2, $deskBuf, 512, [ref]$deskNeeded)
$deskName = if ($deskOk) { [System.Runtime.InteropServices.Marshal]::PtrToStringUni($deskBuf) } else { '<query-failed>' }
[System.Runtime.InteropServices.Marshal]::FreeHGlobal($deskBuf)
if ($deskName -ne 'Default') {
    Step 'desktop-guard' "thread desktop = '$deskName', need 'Default' (virtual-desktop frames are invalid)" 'FAIL'
    $rep.verdict = 'FAIL_DESKTOP'
    $rep | ConvertTo-Json -Depth 7 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
    exit 3
}
Step 'desktop-guard' 'thread desktop = Default' 'OK'

# ---- precondition: installed server ---------------------------------------
$proc = Get-Process WeaselServer -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $proc) { Step 'precondition' 'WeaselServer.exe not running' 'FAIL'; $rep.verdict = 'FAIL'; $rep | ConvertTo-Json -Depth 7 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8; exit 2 }
Step 'precondition' ("pid=" + $proc.Id + " started=" + $proc.StartTime.ToString('yyyy-MM-dd HH:mm:ss') + " ver=" + (Get-Item $proc.Path).VersionInfo.FileVersion) 'OK'
$procId = [uint32]$proc.Id

# ---- open the account window ----------------------------------------------
$existing = [IntPtr]::Zero
foreach ($h in [FluxingNote]::TopOf($procId)) {
    if ([FluxingNote]::Class($h) -eq $CLS_ACCOUNT -and [FluxingNote]::Visible($h)) { $existing = $h }
}
if ($existing -ne [IntPtr]::Zero) {
    Step 'pre-existing' "closing hwnd=$existing first" 'INFO'
    [void][FluxingNote]::PostMessage($existing, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 800
}

$targets = @()
foreach ($h in [FluxingNote]::TopOf($procId)) {
    $cls = [FluxingNote]::Class($h)
    if ($cls -ne 'TrayIconClass') { $targets += $h }
    Step 'server-window' ("hwnd=" + $h + " class='" + $cls + "'") 'INFO'
}
foreach ($t in $targets) { [void][FluxingNote]::PostMessage($t, $WM_COMMAND, [IntPtr]$ID_MENU_CLOUD_ACCOUNT, [IntPtr]::Zero) }

$dlg = [IntPtr]::Zero
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 12000 -and $dlg -eq [IntPtr]::Zero) {
    Start-Sleep -Milliseconds 200
    foreach ($h in [FluxingNote]::TopOf($procId)) {
        if ([FluxingNote]::Class($h) -eq $CLS_ACCOUNT -and [FluxingNote]::Visible($h)) { $dlg = $h }
    }
}
if ($dlg -eq [IntPtr]::Zero) {
    Step 'open' 'account window did NOT appear (server may be busy/quit)' 'FAIL'
    $rep.verdict = 'FAIL'; $rep | ConvertTo-Json -Depth 7 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8; exit 3
}
$dr = [FluxingNote]::Rect($dlg)
$dc2 = [FluxingNote]::Client($dlg)
Step 'open' ("hwnd=" + $dlg + " window=" + ($dr -join ',') + " client=" + ($dc2 -join ',') + " clientW=" + ($dc2[2] - $dc2[0]) + " clientH=" + ($dc2[3] - $dc2[1]) + " dpi=" + [FluxingNote]::Dpi($dlg)) 'OK'

# ---- enumerate children ----------------------------------------------------
$rows = @()
foreach ($h in [FluxingNote]::ChildrenOf($dlg)) {
    $r = [FluxingNote]::Rect($h)
    $rows += [pscustomobject]@{
        hwnd    = $h
        id      = [FluxingNote]::CtrlId($h)
        cls     = [FluxingNote]::Class($h)
        visible = [FluxingNote]::Visible($h)
        x       = $r[0]; y = $r[1]; w = ($r[2] - $r[0]); hh = ($r[3] - $r[1])
        text    = [FluxingNote]::Text($h)
    }
}
$rows | Sort-Object id | ForEach-Object {
    Step 'child' ("id=" + $_.id + " cls=" + $_.cls + " vis=" + $_.visible + " rect=" + $_.x + ',' + $_.y + ',' + $_.w + 'x' + $_.hh + " len=" + $_.text.Length) 'INFO'
}

function Capture($hwnd, $path) {
    $r = [FluxingNote]::Rect($hwnd)
    if ($null -eq $r) { return $null }
    $w = $r[2] - $r[0]; $ht = $r[3] - $r[1]
    if ($w -le 0 -or $ht -le 0) { return $null }
    $bmp = New-Object System.Drawing.Bitmap($w, $ht)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [void][FluxingNote]::PrintWindow($hwnd, $hdc, 2)
    $g.ReleaseHdc($hdc); $g.Dispose()
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $out = $bmp.Clone()
    $bmp.Dispose()
    return $out
}

function InkProfile($bmp) {
    # background = the most common pixel color in the bitmap (corner sampled dominant)
    $counts = @{}
    $W = $bmp.Width; $H = $bmp.Height
    $bgR = 240; $bgG = 240; $bgB = 240
    for ($y = 0; $y -lt $H; $y += 2) {
        for ($x = 0; $x -lt $W; $x += 5) {
            $p = $bmp.GetPixel($x, $y)
            $k = ('{0},{1},{2}' -f $p.R, $p.G, $p.B)
            if ($counts.ContainsKey($k)) { $counts[$k] += 1 } else { $counts[$k] = 1 }
        }
    }
    $top = $counts.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1
    $parts = $top.Key.Split(',')
    $bgR = [int]$parts[0]; $bgG = [int]$parts[1]; $bgB = [int]$parts[2]
    $rows = @()
    for ($y = 0; $y -lt $H; $y++) {
        $ink = 0
        for ($x = 0; $x -lt $W; $x++) {
            $p = $bmp.GetPixel($x, $y)
            $d = [Math]::Abs($p.R - $bgR) + [Math]::Abs($p.G - $bgG) + [Math]::Abs($p.B - $bgB)
            if ($d -gt 90) { $ink++ }
        }
        $rows += $ink
    }
    $bands = @()
    $start = -1
    for ($y = 0; $y -lt $rows.Count; $y++) {
        if ($rows[$y] -gt 2) { if ($start -lt 0) { $start = $y } }
        else { if ($start -ge 0) { $bands += , @($start, ($y - 1)); $start = -1 } }
    }
    if ($start -ge 0) { $bands += , @($start, ($rows.Count - 1)) }
    return [ordered]@{
        bg           = "$bgR,$bgG,$bgB"
        height       = $H
        bands        = ($bands | ForEach-Object { "$($_[0])-$($_[1])" })
        bandCount    = $bands.Count
        lastBandEndsAtBottom = ($bands.Count -gt 0 -and $bands[-1][1] -ge ($H - 1))
        rowInk       = $rows
    }
}

# ---- target control --------------------------------------------------------
$note = $rows | Where-Object { $_.id -eq $NoteId }
if ($null -eq $note) {
    Step 'note' ("control id $NoteId NOT present -> account page is in the NOT-LOGGED-IN layout; A-1..A-3 cannot be judged, A-4/A-5 can") 'WARN'
    $shot = Capture $dlg (Join-Path $OutDir 'account-dialog.png')
    if ($shot) { $shot.Dispose() }
    $rep['logged_in'] = $false
    $rep['visible_ids'] = ($rows | Where-Object visible | ForEach-Object id)
    $rep.verdict = 'NOT_LOGGED_IN'
    $rep | ConvertTo-Json -Depth 7 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
    if (-not $KeepOpen) { [void][FluxingNote]::PostMessage($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) }
    Write-Host "REPORT: $(Join-Path $OutDir 'report.json')"
    exit 4
}
$rep['logged_in'] = $true
Step 'note' ("found id=$($note.id) cls=$($note.cls) vis=$($note.visible) rect=$($note.x),$($note.y),$($note.w)x$($note.hh) chars=$($note.text.Length)") 'OK'
Set-Content -Path (Join-Path $OutDir 'note-text.txt') -Value $note.text -Encoding UTF8 -NoNewline

$m = [FluxingNote]::Measure($note.hwnd, $note.text)
$face = [FluxingNote]::FaceName($note.hwnd)
$inBox = [FluxingNote]::MeasureInBox($note.hwnd, $note.text, $note.w)
$lineH = $m[1]
if ($lineH -le 0) { $lineH = 1 }
$linesWrap = [Math]::Ceiling($m[3] / $lineH)
$linesEdit = [Math]::Ceiling($m[5] / $lineH)
$linesInBox = [Math]::Ceiling($inBox[1] / $lineH)
Step 'measure' ("face='" + $face + "' dpi=" + [FluxingNote]::Dpi($dlg) + " font=" + [FluxingNote]::Font($note.hwnd)) 'INFO'
Step 'measure' ("single-line w=" + $m[0] + " h=" + $m[1] + " | wordbreak w=" + $m[2] + " h=" + $m[3] + " | editcontrol+wordbreak w=" + $m[4] + " h=" + $m[5] + " | wrapped-into-real-box(" + $note.w + ") w=" + $inBox[0] + " h=" + $inBox[1]) 'OK'
Step 'measure' ("lineHeight=" + $lineH + " -> lines(wordbreak)=" + $linesWrap + " lines(edit)=" + $linesEdit + " lines(inBox)=" + $linesInBox + " | controlH=" + $note.hh) 'OK'
$need = $inBox[1]
$deficit = $need - $note.hh
if ($deficit -gt 0) {
    Step 'judge-A1' ("TEXT NEEDS " + $need + "px, BOX GIVES " + $note.hh + "px -> DEFICIT " + $deficit + "px -> CLIPPED") 'FAIL'
} else {
    Step 'judge-A1' ("TEXT NEEDS " + $need + "px, BOX GIVES " + $note.hh + "px -> HEADROOM " + (-$deficit) + "px -> NOT CLIPPED") 'PASS'
}

# ---- positive control: the sibling 23-char note ----------------------------
$ctl = $rows | Where-Object { $_.id -eq $ControlIdOfKnownSingleLine }
if ($null -ne $ctl) {
    $mc = [FluxingNote]::Measure($ctl.hwnd, $ctl.text)
    $inBoxC = [FluxingNote]::MeasureInBox($ctl.hwnd, $ctl.text, $ctl.w)
    $linesC = [Math]::Ceiling($inBoxC[1] / [Math]::Max(1, $mc[1]))
    Step 'control-method' ("id=" + $ctl.id + " chars=" + $ctl.text.Length + " box=" + $ctl.w + 'x' + $ctl.hh + " singleW=" + $mc[0] + " singleH=" + $mc[1] + " inBoxH=" + $inBoxC[1] + " lines=" + $linesC + " -> " + $(if ($linesC -eq 1 -and $inBoxC[1] -le $ctl.hh) { 'matches the documented single-line fact (method has teeth)' } else { 'DISAGREES with the documented single-line fact -> treat numbers with care' })) 'INFO'
}

# ---- pixel evidence --------------------------------------------------------
$shotNote = Capture $note.hwnd (Join-Path $OutDir 'note-1035.png')
if ($null -ne $shotNote) {
    $prof = InkProfile $shotNote
    Step 'pixels-note' ("height=" + $prof.height + " bg=" + $prof.bg + " bands=" + ($prof.bands -join ' ') + " bandCount=" + $prof.bandCount + " lastBandTouchesBottom=" + $prof.lastBandEndsAtBottom) 'OK'
    $rep['note_pixels'] = [ordered]@{ height = $prof.height; bg = $prof.bg; bands = $prof.bands; bandCount = $prof.bandCount; lastBandTouchesBottom = $prof.lastBandTouchesBottom }
    $shotNote.Dispose()
}
$shotDlg = Capture $dlg (Join-Path $OutDir 'account-dialog.png')
if ($shotDlg) { $shotDlg.Dispose() }

# ---- structured result -----------------------------------------------------
$rep['dpi'] = [int][FluxingNote]::Dpi($dlg)
$rep['dialog_client'] = ($dc2 -join ',')
$rep['note'] = [ordered]@{
    id = $note.id; chars = $note.text.Length
    box_w = $note.w; box_h = $note.hh
    single_w = $m[0]; line_h = $m[1]
    wrap_h_wordbreak = $m[3]; wrap_h_edit = $m[5]
    in_box_h = $inBox[1]; lines = $linesInBox
    deficit_px = $deficit
    clipped = ($deficit -gt 0)
}
$rep['controls'] = ($rows | Where-Object visible | ForEach-Object { [ordered]@{ id = $_.id; cls = $_.cls; w = $_.w; h = $_.hh; chars = $_.text.Length } })
$rep.verdict = $(if ($deficit -gt 0) { 'A1_CLIPPED' } else { 'A1_NOT_CLIPPED' })

if (-not $KeepOpen) {
    [void][FluxingNote]::PostMessage($dlg, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 500
    Step 'cleanup' ("window closed, IsWindow=" + [FluxingNote]::IsWindow($dlg)) 'INFO'
} else {
    Step 'cleanup' "kept open for human inspection hwnd=$dlg" 'INFO'
}
$rep | ConvertTo-Json -Depth 7 | Set-Content -Path (Join-Path $OutDir 'report.json') -Encoding UTF8
Write-Host "REPORT: $(Join-Path $OutDir 'report.json')"
Write-Host ("VERDICT=" + $rep.verdict)
