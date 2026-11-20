# diag-enum-01  2026-11-20 23:21:05

## 0. desktop = Default

## 1. run-01 leftovers
  report.json  381 B  mtime=23:15:28
      | ﻿{
      |   "started_local": "2026-11-20 23:15:18",
      |   "verdict": "ABORT_HOST_WINDOW_MISSING",
      |   "notes": [
      |     "server pid=77332 ver=1.3.7.0",
      |     "host1 pid=420460",
      |     "ABORT host1 window not enumerable"
      |   ],
      |   "legs": {},
      |   "desktop": "Default",
      |   "server_pid": 77332,
      |   "server_ver": "1.3.7.0",
      |   "log_bytes_start": 295893,
      |   "finished_local": "2026-11-20 23:15:28"
      | }
  notes.txt  84 B  mtime=23:15:28
      | ﻿server pid=77332 ver=1.3.7.0
      | host1 pid=420460
      | ABORT host1 window not enumerable
  host1.marker  MISSING
  host2.marker  MISSING

## 2. old host pid 420460 (run-01 child)
  gone

## 3. fresh host subprocess
  Start-Process .Id = 128480  start=23:21:05
  marker raw:
      | status=ready
      | shown_epoch_ms=1795188067397
      | host_pid=128480
  marker host_pid = 128480   (Start-Process Id = 128480)  same=True

  HIT on attempt 1

### 3a. windows owned by the host pid
  128480|vis=0|sz=502x276|WindowsForms10.Window.8.app.0.3219388_r3_ad1|FluxingOrphanProbeHost
  128480|vis=0|sz=1x1|GDI+ Hook Window Class|GDI+ Window (pwsh.exe)
  128480|vis=0|sz=1488x776|ConsoleWindowClass|管理员:C:\Program Files\WindowsApps\Microsoft.PowerShell_7.7.0.0_x64__8wekyb3d8bbwe\pwsh.exe
  128480|vis=1|sz=204x59|SoPY_Status|
  128480|vis=0|sz=0x0|MSCTFIME UI|MSCTFIME UI
  128480|vis=0|sz=0x0|SoPY_Hint|HintWnd
  128480|vis=0|sz=0x0|SoPY_UI|
  128480|vis=0|sz=0x0|Sogou_TSF_UI|Sogou_TSF_UI
  128480|vis=0|sz=0x0|IME|Default IME
  128480|vis=0|sz=0x0|IME|Default IME

### 3b. ALL pwsh-owned windows (cross-check pid attribution)
  count = 11
  128480|vis=0|sz=502x276|WindowsForms10.Window.8.app.0.3219388_r3_ad1|FluxingOrphanProbeHost
  128480|vis=0|sz=1x1|GDI+ Hook Window Class|GDI+ Window (pwsh.exe)
  128480|vis=0|sz=1488x776|ConsoleWindowClass|管理员:C:\Program Files\WindowsApps\Microsoft.PowerShell_7.7.0.0_x64__8wekyb3d8bbwe\pwsh.exe
  328124|vis=0|sz=1488x776|ConsoleWindowClass|管理员:C:\Program Files\WindowsApps\Microsoft.PowerShell_7.7.0.0_x64__8wekyb3d8bbwe\pwsh.exe
  128480|vis=1|sz=204x59|SoPY_Status|
  128480|vis=0|sz=0x0|MSCTFIME UI|MSCTFIME UI
  128480|vis=0|sz=0x0|SoPY_Hint|HintWnd
  128480|vis=0|sz=0x0|SoPY_UI|
  128480|vis=0|sz=0x0|Sogou_TSF_UI|Sogou_TSF_UI
  128480|vis=0|sz=0x0|IME|Default IME
  128480|vis=0|sz=0x0|IME|Default IME

### 3c. every window whose class mentions WindowsForms (any pid, whole desktop)
  count = 1
  128480|vis=0|sz=502x276|WindowsForms10.Window.8.app.0.3219388_r3_ad1|FluxingOrphanProbeHost

### 3d. largest visible windows on desktop (top 12 by area, sanity)
  186060|vis=1|sz=3840x2160|Windows.UI.Core.CoreWindow|Windows 输入体验
  196740|vis=1|sz=1920x1080|Progman|Program Manager
  469348|vis=1|sz=1942x1030|Chrome_WidgetWin_1|MiniMax Code
  93916|vis=1|sz=1942x1030|Chrome_WidgetWin_1|ecclaw
  20200|vis=1|sz=1942x1030|Chrome_WidgetWin_1|MiniMax API 开放平台 和另外 13 个页面 - 个人 2 - Microsoft​ Edge
  192220|vis=1|sz=1942x1030|Chrome_WidgetWin_1|Claude Code Haha
  53384|vis=1|sz=1920x1008|Windows.UI.Core.CoreWindow|设置
  180284|vis=1|sz=900x1007|CDesktopCalendarWnd|优效日历-桌面日历
  196740|vis=1|sz=1920x72|Shell_TrayWnd|
  128480|vis=1|sz=204x59|SoPY_Status|
  330624|vis=1|sz=237x39|ApplicationFrameWindow|设置
  196740|vis=1|sz=1897x4|EdgeUiInputTopWndClass|

### 3e. child exit state
  child ALIVE  MainWindowHandle=3277442

## 4. child stdout/stderr
  --- diag-host.out.txt (6 B)
      | True
      | 
  --- diag-host.err.txt (52 B)
      | libpng warning: iCCP: known incorrect sRGB profile
      | 
