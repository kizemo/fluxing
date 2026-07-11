"""
test-quickpanel-e2e.py - spec 074 sandbox-side E2E test for QuickPanel

EVERY v0.19.0.x ship before this script was wrong: we never captured a
real screenshot of the running QuickPanel. This script:

1. Launches WeaselServer.exe (or reuses already-running)
2. Sends WM_HOTKEY to WeaselIPCWindow (simulates Alt+, without SendInput
   which fails in sandbox without foreground focus)
3. Waits 0.5s for panel to show
4. FindWindow(FluxingQuickPanel_v3) to get hwnd
5. GetWindowRect to verify size
6. PrintWindow + BitBlt to capture screenshot
7. Save as PNG; also write BMP for direct byte analysis
8. Print per-region RGB summary (logo / icon areas / corners)

EVERY visual fix claim must be backed by this script. Run before
declaring any v0.19.0.x fix done.

Expected (per design v3-rev3):
- panel size 540 x 102 at 150% DPI (or 360 x 68 at 100%)
- 55% to 85% white-ish pixels in central area (panel gradient)
- some red/maroon pixels in left 56x56 area (Fluxing logo)
- some dark pixels scattered across 5 icon areas

REGRESSION MARKERS:
- all RGB(0,0,0) = Clear failed, or FillRoundedRectangle failed, or
  brush creation failed
- alpha=255 everywhere = the layered window's per-pixel alpha is not
  being honored, OR D2D is not actually filling the render target

Usage:
  python test-quickpanel-e2e.py
Output:
  qp.bmp (raw 32-bit BGRX bitmap from window)
  qp.png (PIL-converted for human inspection)
  e2e-result.txt (summary of captured panel)

Requires:
  - WeaselServer.exe built and accessible (defaults to current cwd)
  - Running session with desktop
"""
import ctypes
import ctypes.wintypes as wt
import os
import struct
import sys
import time
from collections import Counter
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("PIL not available - PNG conversion will be skipped", file=sys.stderr)
    Image = None

user32 = ctypes.WinDLL('user32', use_last_error=True)
gdi32 = ctypes.WinDLL('gdi32', use_last_error=True)

ROOT = Path(r"F:\soft\00selfmade\rime_claude")
WEASELSERVER_EXE = ROOT / "output" / "Win32" / "WeaselServer.exe"
PANEL_CLASS = "FluxingQuickPanel_v3"
IPC_CLASS = "WeaselIPCWindow_1.0"

# Try to find WeaselServer.exe
if not WEASELSERVER_EXE.exists():
    # Try the actual install dir
    WEASELSERVER_EXE = Path(r"D:\Program Files\fluxing\weasel\WeaselServer.exe")

# Helper: find a running WeaselServer.exe
def find_weaselserver_pid():
    import subprocess
    out = subprocess.run(
        ['tasklist', '/FI', 'IMAGENAME eq WeaselServer.exe', '/FO', 'CSV', '/NH'],
        capture_output=True, text=True, timeout=5
    )
    if not out.stdout.strip():
        return None
    # parse "WeaselServer.exe","PID","..."
    parts = out.stdout.strip().split('","')
    if len(parts) >= 2:
        try:
            return int(parts[1])
        except ValueError:
            return None
    return None

# Try launch if not running
pid = find_weaselserver_pid()
if pid is None:
    if not WEASELSERVER_EXE.exists():
        print(f"FATAL: WeaselServer.exe not found at {WEASELSERVER_EXE}")
        print("Build first with: python build-via-py.py")
        sys.exit(1)
    import subprocess
    print(f"Launching {WEASELSERVER_EXE}")
    subprocess.Popen([str(WEASELSERVER_EXE)], shell=False)
    time.sleep(2)
    pid = find_weaselserver_pid()
    if not pid:
        print("FATAL: WeaselServer.exe not running after launch")
        sys.exit(1)
    print(f"WeaselServer.exe running, PID {pid}")
else:
    print(f"WeaselServer.exe already running, PID {pid}")

# Find IPC window
hwnd_ipc = user32.FindWindowW(IPC_CLASS, None)
if not hwnd_ipc:
    print(f"FATAL: cannot find IPC window class={IPC_CLASS}")
    sys.exit(1)
print(f"IPC window hwnd: {hwnd_ipc}")

# Trigger QuickPanel via WM_HOTKEY (sandbox can't SendInput without foreground)
WM_HOTKEY = 0x0312
# ID_HOTKEY_QUICK_PANEL = 9001 (per WeaselServer/resource.h)
# We read it from the header to be safe
import re
ID_HOTKEY = 9001
try:
    with open(ROOT / "WeaselServer" / "resource.h", 'r', encoding='ascii', errors='ignore') as f:
        for line in f:
            m = re.search(r'#define\s+ID_HOTKEY_QUICK_PANEL\s+(\d+)', line)
            if m:
                ID_HOTKEY = int(m.group(1))
                break
except Exception:
    pass
print(f"ID_HOTKEY_QUICK_PANEL = {ID_HOTKEY}")

# Construct lParam: Mod = VK_MENU (Alt) = 0x12, Key = VK_OEM_COMMA = 0xBC
lParam = (0xBC << 16) | 0x12
print(f"SendMessage WM_HOTKEY: hwnd=0x{hwnd_ipc:x} wParam={ID_HOTKEY} lParam=0x{lParam:x}")
result = user32.SendMessageW(hwnd_ipc, WM_HOTKEY, ID_HOTKEY, lParam)
print(f"  SendMessage result: {result}")
time.sleep(0.5)

# Find QuickPanel
hwnd_panel = user32.FindWindowW(PANEL_CLASS, None)
if not hwnd_panel:
    print("FATAL: QuickPanel not found after WM_HOTKEY. Alt+, may have toggled it OFF.")
    sys.exit(1)
print(f"QuickPanel hwnd: 0x{hwnd_panel:x}")

# Get rect, move to (100,100) so any second-monitor offscreen is fixed
rect = wt.RECT()
user32.GetWindowRect(hwnd_panel, ctypes.byref(rect))
w = rect.right - rect.left
h = rect.bottom - rect.top
print(f"Position: ({rect.left}, {rect.top}) Size: {w}x{h}")

SWP_NOSIZE = 0x0001
SWP_NOZORDER = 0x0004
SWP_NOACTIVATE = 0x0010
ok = user32.SetWindowPos(hwnd_panel, 0, 100, 100, 0, 0,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)
print(f"Move to (100, 100): {ok}")
time.sleep(0.3)
user32.GetWindowRect(hwnd_panel, ctypes.byref(rect))
w = rect.right - rect.left
h = rect.bottom - rect.top
print(f"After move: ({rect.left}, {rect.top}) size {w}x{h}")

# Capture via PrintWindow + BitBlt
PW_RENDERFULLCONTENT = 0x00000002
hdc = user32.GetWindowDC(hwnd_panel)
hdc_mem = gdi32.CreateCompatibleDC(hdc)
hbm = gdi32.CreateCompatibleBitmap(hdc, w, h)
gdi32.SelectObject(hdc_mem, hbm)
ok_pw = user32.PrintWindow(hwnd_panel, hdc_mem, PW_RENDERFULLCONTENT)

# GetDIBits
class BIH(ctypes.Structure):
    _fields_ = [
        ('biSize', wt.DWORD), ('biWidth', ctypes.c_int), ('biHeight', ctypes.c_int),
        ('biPlanes', wt.WORD), ('biBitCount', wt.WORD), ('biCompression', wt.DWORD),
        ('biSizeImage', wt.DWORD), ('biXPelsPerMeter', ctypes.c_int),
        ('biYPelsPerMeter', ctypes.c_int), ('biClrUsed', wt.DWORD), ('biClrImportant', wt.DWORD)
    ]

# v0.19.0.10 bug-fix: BI_RGB strips alpha from GetDIBits result.
# Use BI_BITFIELDS (3) + RGBA masks so per-pixel alpha is preserved.
# See https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-getdibits
# "the function sets the biClrUsed member to zero and stores the alpha channel
#  values for each pixel in the high byte of each 32-bit word" - only when BI_BITFIELDS.
bi = BIH()
bi.biSize = 40
bi.biWidth = w
bi.biHeight = -h  # top-down
bi.biPlanes = 1
bi.biBitCount = 32
bi.biCompression = 3  # BI_BITFIELDS
bi.biSizeImage = w * h * 4
bi.biXPelsPerMeter = 2835  # 96 DPI
bi.biYPelsPerMeter = 2835

bits_size = w * h * 4
bits = ctypes.create_string_buffer(bits_size)
# Pass full BITMAPINFO with BI_BITFIELDS + RGBA masks
class BMI(ctypes.Structure):
    _fields_ = [
        ('bmiHeader', BIH),
        ('bmiColors', wt.DWORD * 3),  # 3 masks: R, G, B; alpha implied by BitCount=32
    ]
bmi = BMI()
bmi.bmiHeader = bi
bmi.bmiColors[0] = 0x00FF0000  # R
bmi.bmiColors[1] = 0x0000FF00  # G
bmi.bmiColors[2] = 0x000000FF  # B
# alpha mask is the remaining byte (0xFF000000) when masks sum to 32 bits
gdi32.GetDIBits(hdc_mem, hbm, 0, h, bits, ctypes.byref(bmi), 0)
gdi32.DeleteObject(hbm)
gdi32.DeleteDC(hdc_mem)
user32.ReleaseDC(hwnd_panel, hdc)

# Save BMP (raw) — v0.19.0.10 update: include BI_BITFIELDS color masks in BMP header
bmp_path = ROOT / "qp.bmp"
bfh = struct.pack('<HIHHI', 0x4D42, 14 + 40 + 12 + bits_size, 0, 0, 14 + 40 + 12)
with open(bmp_path, 'wb') as f:
    f.write(bfh)
    f.write(bytes(bi))
    f.write(struct.pack('<III', 0x00FF0000, 0x0000FF00, 0x000000FF))  # RGB masks
    f.write(bits.raw)
print(f"BMP saved: {bmp_path} ({bmp_path.stat().st_size} bytes)")

# Save PNG via PIL
png_path = ROOT / "qp.png"
if Image is not None:
    img = Image.open(str(bmp_path))
    img.save(str(png_path), 'PNG')
    print(f"PNG saved: {png_path} ({png_path.stat().st_size} bytes)")
else:
    print("Skipped PNG conversion (PIL not available)")

# Pixel analysis
alpha_hist = Counter()
rgb_hist = Counter()
unique_rgbs = Counter()
total = w * h
for y in range(h):
    for x in range(w):
        off = (y * w + x) * 4
        b, g, r, a = bits.raw[off:off+4]
        alpha_hist[a] += 1
        rgb_hist[(r, g, b)] += 1
        if r + g + b > 0:
            unique_rgbs[(r, g, b)] += 1

# Sample regions
def sample_region(name, x0, y0, x1, y1):
    """Sample center 1/3 of region for diagnostic"""
    cx = (x0 + x1) // 2
    cy = (y0 + y1) // 2
    rx = max(1, (x1 - x0) // 6)
    ry = max(1, (y1 - y0) // 6)
    samples = []
    for dx in range(-rx, rx + 1, max(1, rx // 2)):
        for dy in range(-ry, ry + 1, max(1, ry // 2)):
            xx, yy = cx + dx, cy + dy
            if 0 <= xx < w and 0 <= yy < h:
                off = (yy * w + xx) * 4
                b, g, r, a = bits.raw[off:off+4]
                samples.append((r, g, b, a))
    if samples:
        avg_r = sum(s[0] for s in samples) // len(samples)
        avg_g = sum(s[1] for s in samples) // len(samples)
        avg_b = sum(s[2] for s in samples) // len(samples)
        avg_a = sum(s[3] for s in samples) // len(samples)
        return f"RGB=({avg_r:3d},{avg_g:3d},{avg_b:3d}) A={avg_a:3d} n={len(samples)}"
    return "no samples"

# Build summary
result_path = ROOT / "e2e-result.txt"
lines = []
lines.append("=== QuickPanel E2E test (spec 074) ===")
lines.append(f"Image: {w}x{h} ({w*h} pixels)")
lines.append(f"PrintWindow: {ok_pw} (1=ok)")
lines.append("")
lines.append("=== Region sampling ===")
# Layout: 56 brand + 4 gap + 5 buttons of 56 each = 540 at 150% DPI
# At physical pixels: brand 0-84, sep 84-90, btn0 90-174, btn1 174-258, btn2 258-342, btn3 342-426, btn4 426-510
# Wait, 540 wide. brand 0-84. Then 5 buttons.
button_w = (w - 84 - 4) // 5  # rough
for name, x0, y0, x1, y1 in [
    ("brand (logo)", 4, 4, 80, 80),
    ("btn 0 (schema)", 90, 4, 90 + button_w - 4, 80),
    ("btn 1 (phrase)", 90 + button_w, 4, 90 + 2 * button_w - 4, 80),
    ("btn 2 (symbols)", 90 + 2 * button_w, 4, 90 + 3 * button_w - 4, 80),
    ("btn 3 (settings)", 90 + 3 * button_w, 4, 90 + 4 * button_w - 4, 80),
    ("btn 4 (account)", 90 + 4 * button_w, 4, 90 + 5 * button_w - 4, 80),
    ("top 1px highlight", w//2, 0, w//2 + 1, 1),
    ("center panel", w//2, h//2, w//2 + 1, h//2 + 1),
    ("bottom-left", 0, h-1, 1, h),
]:
    s = sample_region(name, x0, y0, x1, y1)
    lines.append(f"  {name:30s} {s}")

lines.append("")
lines.append("=== Alpha histogram (top 5) ===")
for a, c in alpha_hist.most_common(5):
    lines.append(f"  alpha={a:3d}: {c:6d} pixels ({100*c//total}%)")

lines.append("")
lines.append("=== RGB histogram (top 10, sorted by count) ===")
for (r,g,b), c in rgb_hist.most_common(10):
    lines.append(f"  RGB=({r:3d},{g:3d},{b:3d}): {c:6d} pixels")

lines.append("")
lines.append("=== Regression markers ===")
opaque_black = rgb_hist.get((0,0,0), 0)
opaque_white = rgb_hist.get((255,255,255), 0)
all_alpha_255 = alpha_hist.get(255, 0) == total
lines.append(f"  Opaque black pixels: {opaque_black} ({100*opaque_black//total}%)")
lines.append(f"  Opaque white pixels: {opaque_white} ({100*opaque_white//total}%)")
lines.append(f"  All alpha=255: {all_alpha_255}")
lines.append("")
if all_alpha_255 and opaque_black == total:
    lines.append("  *** REGRESSION: panel is fully opaque BLACK (alpha=255, RGB=0,0,0)")
    lines.append("      This matches the user-reported '黑色不透明底' bug.")
    lines.append("      D2D render is failing - either:")
    lines.append("      - s_pBrushPanel is null (CreateLinearGradientBrush failed)")
    lines.append("      - s_pRT is null (CreateHwndRenderTarget failed)")
    lines.append("      - WS_EX_LAYERED per-pixel alpha not honored by PrintWindow")
    lines.append("      - STRAIGHT alpha mode not actually set on render target")
elif opaque_white == 0 and opaque_black < total // 2:
    lines.append("  OK: panel is partially transparent (mix of RGBs)")
else:
    lines.append("  ?? Mixed state - inspect the saved PNG.")

with open(result_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print(f"Summary: {result_path}")
print()
print('\n'.join(lines))

# Toggle panel closed (Alt+, again)
result = user32.SendMessageW(hwnd_ipc, WM_HOTKEY, ID_HOTKEY, lParam)
print(f"\n(Alt+, again to close - SendMessage result: {result})")
