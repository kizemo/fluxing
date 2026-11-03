//
// AboutDialog.cpp — 「关于 / 账号说明」页(spec 099 P0-3)
//
// 落点:快捷面板第 4 槽(Account)。用户点击该槽 ⇒ 打开本页(槽位保留)。
//
// 关键约束(见 AboutDialog.h):
//  - P2:显示期与点击期零 I/O —— 版本/产品名/数据目录只在启动期读一次。
//  - P5:用户可见文案全部简体中文。
//  - 版本/产品名单一真相源 = PE 版本资源(字符串表 → 固定版本信息回退链)。
//  - 不用 MessageBox(会阻塞服务端 UI 线程)。
//  - 「打开数据目录」= 本地 ShellExecute(explore),不联网、不等待。
//
#include "stdafx.h"
#include "AboutDialog.h"
#include "ModalChrome.h"

#include <FluxingUserPaths.h>
#include <shellapi.h>

#include <algorithm>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "version.lib")

namespace {

const wchar_t kWindowClassName[] = L"FluxingAboutDialog_v1";

// 版式常量(与 ShortcutSettings 同一套视觉语言)
constexpr int kClientW = 420;
constexpr int kClientH = 300;
constexpr int kPadX = 28;
constexpr int kTitleY = 22;
constexpr int kVersionY = 58;
constexpr int kPrivacyY = 92;
constexpr int kDataDirY = 162;
constexpr int kButtonY = 228;
constexpr int kButtonW = 124;
constexpr int kButtonH = 32;
constexpr int kGap = 14;

constexpr COLORREF kBgTop = RGB(245, 245, 248);
constexpr COLORREF kBgBot = RGB(226, 228, 236);
constexpr COLORREF kBorderColor = RGB(214, 216, 224);
constexpr COLORREF kTextColor = RGB(32, 34, 40);
constexpr COLORREF kTextSecondary = RGB(104, 108, 120);

constexpr int IDC_TITLE = 1001;
constexpr int IDC_VERSION = 1002;
constexpr int IDC_PRIVACY = 1003;
constexpr int IDC_DATADIR = 1004;
constexpr int IDC_OPEN_DIR = 1005;
constexpr int IDC_CLOSE = 1006;

HWND g_hwnd = nullptr;
std::wstring g_product;
std::wstring g_version;
std::wstring g_data_dir;
HFONT g_font = nullptr;
HFONT g_font_bold = nullptr;

// 产品名唯一来源 = PE 版本资源的 ProductName;读不到时才有兜底。
std::wstring ProductOrFallback() {
  return g_product.empty() ? std::wstring(L"Fluxing") : g_product;
}

std::wstring VersionOrFallback() {
  return g_version.empty() ? std::wstring(L"未知") : g_version;
}

HFONT MakeFont(int px, bool bold) {
  return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE,
                     FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                     DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

void CenterOnPrimary(HWND hwnd, int w, int h) {
  RECT work = {0, 0, 0, 0};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0))
    work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  const int x = work.left + ((work.right - work.left) - w) / 2;
  const int y = work.top + ((work.bottom - work.top) - h) / 2;
  SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

HWND AddChild(HWND parent, const wchar_t* cls, int id,
               const std::wstring& text, int x, int y, int w, int h,
               HFONT font) {
  // CreateWindowExW 有 **12** 个形参(exStyle, class, name, style, x, y, w, h,
  // parent, hMenu, hInstance, lpParam)——hMenu 位在 parent 之后,控件 id 走
  // hMenu 传。少传一个直接编译不过(C2660)。
  // id → HMENU:先 static_cast<int→INT_PTR>(Win32 下 INT_PTR 就是 int),
  // 再 reinterpret_cast(int→指针)。**不能**写成 reinterpret_cast<INT_PTR>(id):
  // int→int 属于标准转换,reinterpret_cast 不允许做标准转换 ⇒ MSVC C2440。
  HWND child = CreateWindowExW(
      0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, y, w, h,
      parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  if (child != nullptr && font != nullptr)
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  return child;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l);

}  // namespace

namespace AboutDialog {

// 读 PE 版本资源。
//
// 为什么不只读一个字段:rc 的字符串表里 FileVersion 曾因缺 STRZ() 被写成
// 空串(lessons-learned L175)⇒ 单字段读取会静默拿到空值。回退链:
//   ProductVersion → FileVersion → VS_FIXEDFILEINFO(固定版本信息)。
//
// 语言块:rc 同时有 zh-CN / zh-TW / neutral 三块,必须显式按语言优先级挑,
// 不能只取第一块(否则在非中文系统上会显示错语种的产品名)。
bool ReadVersionInfo(const std::wstring& file_path,
                     std::wstring* product_name,
                     std::wstring* product_version) {
  if (product_name)
    product_name->clear();
  if (product_version)
    product_version->clear();
  if (file_path.empty())
    return false;

  DWORD handle = 0;
  const DWORD size = GetFileVersionInfoSizeW(file_path.c_str(), &handle);
  if (size == 0)
    return false;
  std::vector<BYTE> buf(size);
  if (!GetFileVersionInfoW(file_path.c_str(), handle, size, buf.data()))
    return false;

  // 固定版本信息(总是存在)——「数字版本」的兜底来源。
  std::wstring fixed_version;
  VS_FIXEDFILEINFO* ffi = nullptr;
  UINT ffi_len = 0;
  if (VerQueryValueW(buf.data(), L"\\", reinterpret_cast<LPVOID*>(&ffi),
                     &ffi_len) &&
      ffi != nullptr && ffi_len >= sizeof(VS_FIXEDFILEINFO)) {
    wchar_t num[64] = {0};
    swprintf_s(num, L"%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS),
               LOWORD(ffi->dwFileVersionMS), HIWORD(ffi->dwFileVersionLS),
               LOWORD(ffi->dwFileVersionLS));
    fixed_version = num;
  }

  // 字符串表:枚举全部 Translation,按语言优先级排序后逐个取。
  std::vector<std::pair<WORD, WORD>> translations;
  LPVOID trans = nullptr;
  UINT trans_len = 0;
  if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", &trans,
                     &trans_len) &&
      trans != nullptr && trans_len >= 4) {
    const WORD* raw = reinterpret_cast<const WORD*>(trans);
    const size_t count = trans_len / 4;
    for (size_t i = 0; i < count; ++i)
      translations.emplace_back(raw[i * 2], raw[i * 2 + 1]);
  }
  auto lang_rank = [](WORD lang) -> int {
    switch (lang) {
      case 0x0804:
        return 0;  // zh-CN
      case 0x0404:
        return 1;  // zh-TW
      case 0x0409:
        return 2;  // en-US
      case 0x0809:
        return 2;  // en-GB
      default:
        return 9;
    }
  };
  std::stable_sort(translations.begin(), translations.end(),
                   [&](const std::pair<WORD, WORD>& a,
                       const std::pair<WORD, WORD>& b) {
                     return lang_rank(a.first) < lang_rank(b.first);
                   });

  std::wstring product, product_ver, file_ver;
  for (const auto& t : translations) {
    wchar_t block[32] = {0};
    swprintf_s(block, L"\\StringFileInfo\\%04x%04x", t.first, t.second);
    auto query = [&](const wchar_t* key, std::wstring* out) {
      if (!out->empty())
        return;
      const std::wstring path = std::wstring(block) + L"\\" + key;
      LPVOID val = nullptr;
      UINT val_len = 0;
      if (VerQueryValueW(buf.data(), path.c_str(), &val, &val_len) &&
          val != nullptr && val_len > 0) {
        // 字符串值以 '\0' 结尾(VerQueryValue 契约),长度用 wcslen 取,
        // 不用 val_len(该值对字符串是「字符数」还是「字节数」各家文档不一)。
        const wchar_t* s = reinterpret_cast<const wchar_t*>(val);
        const size_t n = wcslen(s);
        if (n > 0)
          out->assign(s, n);
      }
    };
    query(L"ProductName", &product);
    query(L"ProductVersion", &product_ver);
    query(L"FileVersion", &file_ver);
    if (!product.empty() && !product_ver.empty() && !file_ver.empty())
      break;
  }

  std::wstring version;
  if (!product_ver.empty())
    version = product_ver;
  else if (!file_ver.empty())
    version = file_ver;
  else
    version = fixed_version;

  if (product_name)
    *product_name = product;
  if (product_version)
    *product_version = version;
  return !version.empty() || !product.empty();
}

void InitializeFromPath(const std::wstring& exe_path) {
  std::wstring product, version;
  if (ReadVersionInfo(exe_path, &product, &version)) {
    g_product = product;
    g_version = version;
  }
  // 数据目录同样在启动期解析好(注册表读一次),显示期零 I/O。
  g_data_dir = ResolveFluxingUserDataPath().wstring();
}

void Initialize() {
  wchar_t path[MAX_PATH] = {0};
  const DWORD n = GetModuleFileNameW(nullptr, path, _countof(path));
  if (n > 0 && n < _countof(path))
    InitializeFromPath(path);
  else
    g_data_dir = ResolveFluxingUserDataPath().wstring();
}

std::wstring ProductName() {
  return g_product;
}

std::wstring VersionText() {
  return g_version;
}

std::wstring DataDirText() {
  if (g_data_dir.empty())
    g_data_dir = ResolveFluxingUserDataPath().wstring();
  return g_data_dir;
}

HWND ActiveHwnd() {
  return g_hwnd;
}

HWND Show() {
  if (g_hwnd != nullptr && IsWindow(g_hwnd)) {
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    return g_hwnd;
  }

  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;  // 整个 client 由 WM_PAINT 画(见 ModalChrome)
  wc.lpszClassName = kWindowClassName;
  RegisterClassExW(&wc);  // 重复注册返回 0(ERROR_CLASS_ALREADY_EXISTS),无害

  const std::wstring caption = L"关于 " + ProductOrFallback();
  RECT rc = {0, 0, kClientW, kClientH};
  AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                     WS_EX_TOOLWINDOW);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;

  g_hwnd = CreateWindowExW(
      WS_EX_TOOLWINDOW, kWindowClassName, caption.c_str(),
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT, w,
      h, nullptr, nullptr, wc.hInstance, nullptr);
  if (g_hwnd == nullptr)
    return nullptr;
  CenterOnPrimary(g_hwnd, w, h);
  ShowWindow(g_hwnd, SW_SHOW);
  UpdateWindow(g_hwnd);
  return g_hwnd;
}

}  // namespace AboutDialog

namespace {

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_CREATE: {
      g_font = MakeFont(13, false);
      g_font_bold = MakeFont(19, true);
      const int w = kClientW - kPadX * 2;
      // 产品名来自 PE 版本资源(唯一真相源),不硬编码第二份字面量。
      AddChild(hwnd, L"STATIC", IDC_TITLE, ProductOrFallback(), kPadX, kTitleY,
               w, 28, g_font_bold);
      AddChild(hwnd, L"STATIC", IDC_VERSION, L"版本 " + VersionOrFallback(),
               kPadX, kVersionY, w, 20, g_font);
      // 隐私说明:本 fork 无云端账号,数据全在本机。
      AddChild(hwnd, L"STATIC", IDC_PRIVACY,
               L"本输入法不含云端账号。词库、配置与用户数据全部保存在本机，不上传、不同步。",
               kPadX, kPrivacyY, w, 44, g_font);
      AddChild(hwnd, L"STATIC", IDC_DATADIR,
               L"数据目录：" + AboutDialog::DataDirText(), kPadX, kDataDirY, w,
               20, g_font);
      AddChild(hwnd, L"BUTTON", IDC_OPEN_DIR, L"打开数据目录", kPadX, kButtonY,
               kButtonW, kButtonH, g_font);
      AddChild(hwnd, L"BUTTON", IDC_CLOSE, L"关闭", kPadX + kButtonW + kGap,
               kButtonY, 96, kButtonH, g_font);
      return 0;
    }
    case WM_PAINT: {
      PAINTSTRUCT ps = {0};
      HDC dc = BeginPaint(hwnd, &ps);
      RECT rc = {0, 0, 0, 0};
      GetClientRect(hwnd, &rc);
      ModalChrome::PaintBackgroundAndBorder(dc, rc.right, rc.bottom, kBgTop,
                                            kBgBot, 12, kBorderColor);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(w);
      const int id = GetDlgCtrlID(reinterpret_cast<HWND>(l));
      // 透明底 ⇒ 子控件文字直接叠在父窗口画好的渐变上,无接缝。
      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, (id == IDC_VERSION || id == IDC_DATADIR)
                            ? kTextSecondary
                            : kTextColor);
      // TRANSPARENT 背景下必须回 NULL_BRUSH:回 COLOR_WINDOW 会用白刷子把
      // 子控件矩形刷成白色,在渐变底上出现一块块白斑。
      return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }
    case WM_COMMAND: {
      const int id = LOWORD(w);
      if (id == IDC_OPEN_DIR) {
        const std::wstring dir = AboutDialog::DataDirText();
        if (!dir.empty())
          ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr,
                        SW_SHOWNORMAL);
        return 0;
      }
      if (id == IDC_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    }
    case WM_KEYDOWN: {
      if (w == VK_ESCAPE) {
        DestroyWindow(hwnd);
        return 0;
      }
      break;
    }
    case WM_DESTROY: {
      g_hwnd = nullptr;
      return 0;
    }
    case WM_NCDESTROY: {
      if (g_font != nullptr) {
        DeleteObject(g_font);
        g_font = nullptr;
      }
      if (g_font_bold != nullptr) {
        DeleteObject(g_font_bold);
        g_font_bold = nullptr;
      }
      break;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, w, l);
}

}  // namespace
