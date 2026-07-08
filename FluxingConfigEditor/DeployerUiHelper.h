#pragma once

#include <ShellScalingApi.h>
#include <WeaselUiTheme.h>
#include <dwmapi.h>

#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "Dwmapi.lib")

namespace deployer_ui {

constexpr wchar_t kUiRegistryKey[] = L"Software\\Fluxing\\Fluxing\\UI";
constexpr int kTitleBarHeightDp = weasel_ui::kTitleBarHeightDp;

inline void EnableDarkTitleBar(HWND hwnd) {
  if (!hwnd)
    return;
  BOOL use_dark = TRUE;
  DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &use_dark,
                        sizeof(use_dark));
}



inline UINT GetDpiForWindow(HWND hwnd) {

  UINT dpi = 96;

  HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

  if (mon && SUCCEEDED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dpi, &dpi)))

    return dpi;

  HDC hdc = GetDC(hwnd ? hwnd : NULL);

  if (hdc) {

    dpi = GetDeviceCaps(hdc, LOGPIXELSX);

    ReleaseDC(NULL, hdc);

  }

  return dpi;

}



inline int Scale(int v, UINT dpi) {

  return MulDiv(v, dpi, 96);

}



inline HFONT CreateUiFont(int point_size, UINT dpi, bool bold = false) {

  int height = -MulDiv(point_size, dpi, 72);

  return CreateFontW(height, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE,

                     FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,

                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,

                     DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

}



inline void BringDialogToFront(HWND hwnd) {

  if (!hwnd)

    return;

  HWND fg = GetForegroundWindow();

  DWORD fg_tid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;

  DWORD cur_tid = GetCurrentThreadId();

  if (fg_tid && fg_tid != cur_tid)

    AttachThreadInput(cur_tid, fg_tid, TRUE);

  ShowWindow(hwnd, SW_SHOW);

  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,

               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,

               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  SetForegroundWindow(hwnd);

  BringWindowToTop(hwnd);

  if (fg_tid && fg_tid != cur_tid)

    AttachThreadInput(cur_tid, fg_tid, FALSE);

}



inline void EnableResizableFrame(HWND hwnd) {

  if (!hwnd)

    return;

  LONG style = GetWindowLong(hwnd, GWL_STYLE);

  style |= WS_THICKFRAME | WS_MAXIMIZEBOX;

  style &= ~DS_MODALFRAME;

  SetWindowLong(hwnd, GWL_STYLE, style);

  SetWindowPos(hwnd, NULL, 0, 0, 0, 0,

               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

}



inline bool LoadWindowGeometry(const wchar_t* value_prefix, int& x, int& y,

                               int& w, int& h, int default_w, int default_h) {

  HKEY key = NULL;

  if (RegOpenKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, KEY_READ, &key) !=

      ERROR_SUCCESS)

    return false;

  auto read_dword = [&](const wchar_t* name, int& out, bool allow_zero) -> bool {

    DWORD type = REG_DWORD;

    DWORD size = sizeof(DWORD);

    DWORD value = 0;

    if (RegQueryValueExW(key, name, NULL, &type, (BYTE*)&value, &size) !=

            ERROR_SUCCESS ||

        type != REG_DWORD)

      return false;

    if (!allow_zero && value == 0)

      return false;

    out = static_cast<int>(value);

    return true;

  };

  wchar_t name[64];

  swprintf_s(name, L"%sX", value_prefix);

  if (!read_dword(name, x, true))

    return false;

  swprintf_s(name, L"%sY", value_prefix);

  if (!read_dword(name, y, true))

    return false;

  swprintf_s(name, L"%sW", value_prefix);

  if (!read_dword(name, w, false))

    w = default_w;

  swprintf_s(name, L"%sH", value_prefix);

  if (!read_dword(name, h, false))

    h = default_h;

  RegCloseKey(key);

  return w > 0 && h > 0;
}


// Read a single integer (REG_DWORD) under kUiRegistryKey. Returns true if the
// value existed; out is set to the loaded value (or 0 on failure).
inline bool LoadDwordValue(const wchar_t* name, int& out) {
  out = 0;
  HKEY key = NULL;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, KEY_READ, &key) !=
      ERROR_SUCCESS)
    return false;
  DWORD type = REG_DWORD;
  DWORD size = sizeof(DWORD);
  DWORD value = 0;
  bool ok = (RegQueryValueExW(key, name, NULL, &type, (BYTE*)&value, &size) ==
             ERROR_SUCCESS) &&
            (type == REG_DWORD);
  if (ok) out = static_cast<int>(value);
  RegCloseKey(key);
  return ok;
}

inline void SaveDwordValue(const wchar_t* name, int value) {
  HKEY key = NULL;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, NULL, 0, KEY_WRITE,
                      NULL, &key, NULL) != ERROR_SUCCESS)
    return;
  DWORD v = static_cast<DWORD>(value);
  RegSetValueExW(key, name, 0, REG_DWORD, (BYTE*)&v, sizeof(v));
  RegCloseKey(key);
}

inline void SaveWindowGeometry(const wchar_t* value_prefix, HWND hwnd) {

  if (!hwnd)

    return;

  WINDOWPLACEMENT wp = {sizeof(wp)};

  if (!GetWindowPlacement(hwnd, &wp))

    return;

  RECT rc = wp.rcNormalPosition;

  const int x = rc.left;

  const int y = rc.top;

  const int w = rc.right - rc.left;

  const int h = rc.bottom - rc.top;

  HKEY key = NULL;

  if (RegCreateKeyExW(HKEY_CURRENT_USER, kUiRegistryKey, 0, NULL, 0, KEY_WRITE,

                      NULL, &key, NULL) != ERROR_SUCCESS)

    return;

  auto write_dword = [&](const wchar_t* name, int value) {

    DWORD v = static_cast<DWORD>(value);

    RegSetValueExW(key, name, 0, REG_DWORD, (BYTE*)&v, sizeof(v));

  };

  wchar_t name[64];

  swprintf_s(name, L"%sX", value_prefix);

  write_dword(name, x);

  swprintf_s(name, L"%sY", value_prefix);

  write_dword(name, y);

  swprintf_s(name, L"%sW", value_prefix);

  write_dword(name, w);

  swprintf_s(name, L"%sH", value_prefix);

  write_dword(name, h);

  RegCloseKey(key);

}



inline void ApplySavedOrDefaultGeometry(HWND hwnd, const wchar_t* value_prefix,

                                        int default_w, int default_h,

                                        bool center_if_new = true) {

  UINT dpi = GetDpiForWindow(hwnd);

  int x = 0, y = 0, w = Scale(default_w, dpi), h = Scale(default_h, dpi);

  if (LoadWindowGeometry(value_prefix, x, y, w, h, w, h)) {

    SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);

  } else {

    SetWindowPos(hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);

    if (center_if_new) {

      HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

      MONITORINFO mi = {sizeof(mi)};

      if (GetMonitorInfo(mon, &mi)) {

        RECT wr;

        GetWindowRect(hwnd, &wr);

        const int width = wr.right - wr.left;

        const int height = wr.bottom - wr.top;

        int cx = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - width) / 2;

        int cy = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;

        SetWindowPos(hwnd, NULL, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

      }

    }

  }

}



inline int TitleBarHeight(UINT dpi) {
  return Scale(kTitleBarHeightDp, dpi);
}



inline void PaintDialogBackground(HDC hdc, const RECT& rc,
                                  const wchar_t* title = nullptr,
                                  HFONT font_title = nullptr, UINT dpi = 96) {
  const int title_h = title ? TitleBarHeight(dpi) : 0;
  if (title_h > 0) {
    RECT title_rc = rc;
    title_rc.bottom = title_rc.top + title_h;
    weasel_ui::PaintTitleBar(hdc, title_rc, title, font_title, nullptr,
                             Scale(14, dpi));
  }
  RECT body = rc;
  body.top += title_h;
  HBRUSH brush = CreateSolidBrush(weasel_ui::kSurface);
  FillRect(hdc, &body, brush);
  DeleteObject(brush);
}



}  // namespace deployer_ui

