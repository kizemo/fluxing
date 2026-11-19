#pragma once
//
// 批次 AT-1 · 账号页系列窗口的共用 Win32 小工具
//
// 账号页在本批次拆成 5 个窗口（主窗口 + 验证码 / 注册 / 找回口令 /
// 设备与云数据），它们共用同一套「建控件 / 读控件 / 清零口令 / 保证窗口
// 可见」的 helper —— 抽到这里，避免每个 .cpp 各写一份（DRY）。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <string>

namespace fluxing {
namespace cloud {
namespace widgets {

// ---------------------------------------------------------------------------
// 文本转换
// ---------------------------------------------------------------------------

// UTF-8 → UTF-16。
inline std::wstring ToWide(const std::string& s) {
  if (s.empty())
    return std::wstring();
  const int need = ::MultiByteToWideChar(
      CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
  if (need <= 0)
    return std::wstring();
  std::wstring w(static_cast<size_t>(need), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                        &w[0], need);
  return w;
}

// UTF-16 → UTF-8。
inline std::string ToUtf8(const std::wstring& w) {
  if (w.empty())
    return std::string();
  const int need =
      ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                            nullptr, 0, nullptr, nullptr);
  if (need <= 0)
    return std::string();
  std::string s(static_cast<size_t>(need), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                        &s[0], need, nullptr, nullptr);
  return s;
}

// ---------------------------------------------------------------------------
// 控件
// ---------------------------------------------------------------------------

// 默认 UI 字体（进程内单例；只在 UI 线程用）。
inline HFONT DefaultUiFont() {
  static HFONT font =
      reinterpret_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
  return font;
}

inline void ApplyFont(HWND hwnd) {
  if (hwnd != nullptr && DefaultUiFont() != nullptr)
    ::SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(DefaultUiFont()),
                   TRUE);
}

// 建一个子控件（默认 WS_CHILD | WS_VISIBLE）。可见性随后由
// AccountVisibleControlIds 统一裁决（ShowWindow 切换，不销毁重建）。
inline HWND AddControl(HWND parent,
                       const wchar_t* cls,
                       const wchar_t* text,
                       DWORD style,
                       int x,
                       int y,
                       int w,
                       int h,
                       int id) {
  HWND hwnd = ::CreateWindowExW(
      0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      ::GetModuleHandleW(nullptr), nullptr);
  ApplyFont(hwnd);
  return hwnd;
}

// 读取控件文本（宽字符）。缓冲区用后立刻抹掉 —— 口令与验证码都走这条路。
inline std::wstring ReadControlText(HWND parent, int id) {
  HWND ctrl = ::GetDlgItem(parent, id);
  if (ctrl == nullptr)
    return std::wstring();
  wchar_t buf[256] = {0};
  ::GetWindowTextW(ctrl, buf, 256);
  std::wstring value(buf);
  ::SecureZeroMemory(buf, sizeof(buf));
  return value;
}

// 主动清零宽字符串本体：std::wstring 析构不清堆内存，口令会留在页里。
inline void SecureZeroWide(std::wstring* s) {
  if (s == nullptr || s->empty())
    return;
  ::SecureZeroMemory(&(*s)[0], s->size() * sizeof(wchar_t));
  s->clear();
}

// 把窗口摆在 owner 窗口正中；owner 为空 ⇒ 摆在屏幕工作区中心。
// （子窗口都从主窗口弹出来，叠在一起会让人找不到。）
inline void CenterOnOwner(HWND hwnd, HWND owner) {
  if (hwnd == nullptr)
    return;
  RECT wrc;
  if (!::GetWindowRect(hwnd, &wrc))
    return;
  const int width = wrc.right - wrc.left;
  const int height = wrc.bottom - wrc.top;

  RECT anchor;
  bool has_anchor = false;
  if (owner != nullptr && ::IsWindow(owner))
    has_anchor = ::GetWindowRect(owner, &anchor) != FALSE;
  if (!has_anchor) {
    has_anchor = ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0) != FALSE;
  }
  if (!has_anchor)
    return;
  const int x = anchor.left + ((anchor.right - anchor.left) - width) / 2;
  const int y = anchor.top + ((anchor.bottom - anchor.top) - height) / 2;
  ::SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// 显示
// ---------------------------------------------------------------------------

// 保证窗口真的可见。**不用 ShowWindow 来显示**:
//   进程内**首次** ShowWindow 的 nCmdShow 会被启动信息
//   (STARTUPINFO + STARTF_USESHOWWINDOW)覆盖 —— WeaselServer 以隐藏/最小化
//   方式拉起时, 首次 ShowWindow(SW_SHOW) 被改写成 SW_HIDE, 窗口从此停在
//   隐藏态, 而缓存的句柄还在 ⇒ 之后每次点击都只是"对一个隐藏窗
//   SetForegroundWindow", 用户看到的就是"点菜单毫无反应"(2026-11-11 真机
//   缺陷, 探针实证: 窗口存在但 WS_VISIBLE=0)。
//   SetWindowPos(SWP_SHOWWINDOW) 直接置 WS_VISIBLE 位, 不走 ShowWindow 的
//   nCmdShow 语义, 因此不受该覆盖影响。
inline void EnsureWindowShown(HWND hwnd) {
  if (hwnd == nullptr || !::IsWindow(hwnd))
    return;
  if (::IsIconic(hwnd))
    ::ShowWindow(hwnd, SW_RESTORE);
  if (!::IsWindowVisible(hwnd))
    ::SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  ::UpdateWindow(hwnd);
}

}  // namespace widgets
}  // namespace cloud
}  // namespace fluxing
