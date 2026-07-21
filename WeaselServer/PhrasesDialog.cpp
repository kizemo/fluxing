//
// PhrasesDialog.cpp — 常用短语 Modal 实现 v3 (spec 044 UX redo)
//
// v0.19.0.32 简化:
// - 删 inline-edit overlay (s_hEditText / s_hEditCat / m_editingIndex /
//   BeginInlineEdit / EnterEditingState / ExitEditingState /
//   SetButtonsForBrowsing / SetButtonsForEditing) — 移除 2 个 EDIT 堆叠
//   遮挡 ListView 的 bug
// - 删 TreeView (s_hTree) → 改 ListView (s_hList),单列 "phrase text"
// - 删 s_hSearch (搜索框) → 删 ApplySearchFilter / Search state machine
// - 删 s_hStatus (状态栏) → 删 YAML fail warn UI
// - 删 s_hToast (toast) → 删 IDT_TOAST timer
// - 删 s_hBtnSave → 删 Save 按钮 (Save 合并到 Edit 流程)
// - Phrase struct 简化: 只保留 text 字段,删 category
// - 状态机简化: 只 Hidden / Browsing 两态
//
#include "stdafx.h"
#include "PhrasesDialog.h"
#include "ModalChrome.h"

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>

// ===== Static state 定义 =====
HWND PhrasesDialog::s_hwnd = nullptr;
HWND PhrasesDialog::s_hInput = nullptr;
HWND PhrasesDialog::s_hBtnAddTop = nullptr;
HWND PhrasesDialog::s_hList = nullptr;
// v0.19.0.41 删 s_hBtnAdd — 跟 s_hBtnAddTop 功能重复, 留一个就够
HWND PhrasesDialog::s_hBtnEdit = nullptr;
HWND PhrasesDialog::s_hBtnDel = nullptr;
HWND PhrasesDialog::s_hBtnCancel = nullptr;
std::wstring PhrasesDialog::s_yamlPath;
PhrasesDialog::InjectFn PhrasesDialog::s_injectFn =
    &PhrasesDialog::DefaultInject;
// v0.19.0.39 (Phase F fix): 默认指 OS API, test 可注入 mock (Test 23).
PhrasesDialog::SetForegroundFn PhrasesDialog::s_setForegroundFn =
    &::SetForegroundWindow;
PhrasesDialog::AllowSetForegroundFn PhrasesDialog::s_allowSetForegroundFn =
    &::AllowSetForegroundWindow;
std::vector<PhrasesDialog::Phrase> PhrasesDialog::m_phrases;
int PhrasesDialog::m_selectedIndex = -1;
DWORD PhrasesDialog::s_showTime = 0;
HFONT PhrasesDialog::s_hFontUi = nullptr;
int PhrasesDialog::kListH_phys = 0;
int PhrasesDialog::kBtnY_phys = 0;
PhrasesDialog::State PhrasesDialog::s_state = PhrasesDialog::State_Hidden;
// v0.19.0.41 (Bug 2: DPI 缩放): default 1.0 = 96 DPI。OnCreate 通过
// GetDpiForWindow 算实际值。TestPhrasesDialog sandbox 走 96 DPI = 1.0。
double PhrasesDialog::s_dpiScale = 1.0;
// v0.19.0.41 (Feature: 长按 drag): state 初始化
bool PhrasesDialog::s_longPressActive = false;
bool PhrasesDialog::s_isDragging = false;
POINT PhrasesDialog::s_dragOrigin = {0, 0};
POINT PhrasesDialog::s_dragWndOrigin = {0, 0};

// ===== 设计常量 (spec 044 §3) =====
namespace {
// 尺寸 (size.modal.*)
constexpr int kDialogW = 360;
constexpr int kDialogH = 460;
constexpr int kTitleH = 30;
constexpr int kInputH = 32;   // v0.19.0.32: 顶部 input 行高
constexpr int kInputW = 240;  // v0.19.0.32: input 宽 (右侧 Add 按钮让位)
constexpr int kBtnAddTopW = 76;  // v0.19.0.32: 顶部 Add 按钮宽
constexpr int kBtnH = 32;
constexpr int kBtnW = 76;
constexpr int kBtnGap = 8;
constexpr int kBtnMarginX = 12;
constexpr int kGap = 8;
constexpr int kDlgRadius = 14;

// 颜色 (color.modal.*,跟 v0.19.0.27 一致)
constexpr COLORREF kBorderColor = RGB(217, 217, 217);
constexpr COLORREF kBgTop = RGB(245, 245, 248);
constexpr COLORREF kBgBot = RGB(220, 222, 230);
constexpr COLORREF kTextColor = RGB(30, 30, 40);
constexpr COLORREF kSelBg = RGB(255, 235, 220);
constexpr COLORREF kInputBg = RGB(255, 255, 255);
constexpr COLORREF kInputBorder = RGB(220, 220, 225);
constexpr COLORREF kButtonBg = RGB(245, 245, 248);

// 字体
constexpr int kUiFontSize = 14;

// 子控件 ID (v0.19.0.32 重新编号, v0.19.0.41 删 ID_BTN_ADD 重复)
constexpr UINT ID_INPUT = 1010;        // 顶部 input
constexpr UINT ID_BTN_ADD_TOP = 1011;  // 顶部 Add 按钮 (v0.19.0.41 唯一 Add)
constexpr UINT ID_LIST = 1100;         // ListView
// v0.19.0.41 删 ID_BTN_ADD (1001) — 底部重复 Add 已删, 只留顶部 ID_BTN_ADD_TOP
constexpr UINT ID_BTN_EDIT = 1002;
constexpr UINT ID_BTN_DEL = 1003;
constexpr UINT ID_BTN_CANCEL = 1004;
}  // namespace

// ===== Public API =====

void PhrasesDialog::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    if (s_hInput) {
      SetFocus(s_hInput);
    }
    s_showTime = GetTickCount();
    return;
  }

  // 1. 加载 YAML
  if (s_yamlPath.empty()) {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
      std::wstring base = std::wstring(path) + L"\\Rime";
      s_yamlPath = base + L"\\phrases.yaml";
    } else {
      s_yamlPath = L"phrases.yaml";
    }
  }
  m_phrases.clear();
  if (!LoadPhrases(s_yamlPath, m_phrases)) {
    std::wcerr << L"[PhrasesDialog] WARN: cannot load " << s_yamlPath
               << L", start with empty list" << std::endl;
    m_phrases.clear();
  }

  // 2. 注册 ListView 控件类
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  // 3. 创建 modal 窗口
  // v0.19.0.33 (Phase B Bug 2b 真修): 删 WS_EX_LAYERED。WS_EX_LAYERED
  // 窗口不通过 WM_PAINT / BeginPaint 路径画 body — DWM 把它当 layered surface,
  // OnPaint 的 FillRect/DrawText/DrawEdge 全部丢失, body
  // 透明成用户看到的"无内容"。 走 ShortcutSettings 同样路径 (WS_EX_LAYERED
  // 不设), BeginPaint/EndPaint 正常 paint body。
  DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  // v0.19.0.41 (Feature): 加 WS_THICKFRAME — 让用户能用鼠标拖边界 resize
  // (Win32 默认: WS_THICKFRAME = 可调整边框 + 可最大化/最小化按钮, 我们
  // 不要 min/max 所以不加 WS_MAXIMIZEBOX / WS_MINIMIZEBOX)。
  DWORD style =
      WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_THICKFRAME;

  // 注册 window class(幂等)
  static bool s_classRegistered = false;
  if (!s_classRegistered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &PhrasesDialog::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"FluxingPhrasesDialogV3";
    if (!RegisterClassExW(&wc)) {
      DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS) {
        std::wcerr << L"[PhrasesDialog] RegisterClassEx failed, err=" << err
                   << std::endl;
        return;
      }
    }
    s_classRegistered = true;
  }

  // v0.19.0.43 (Bug 2.3 layout clipping 真修): outer 尺寸必须按 DPI 缩放 + 加
  // WS_THICKFRAME border (否则 client area 实际 < kDialogW × kDialogH, 4 个
  // 出框 child: 顶部 Add 按钮 (右), 列表 (底), 3 个底部按钮 (底))。
  //
  // 原 v0.19.0.41/0.19.0.42 bug:
  //   CreateWindowEx 用 raw kDialogW × kDialogH 作 outer 尺寸
  //   → client 实际 ~344 × 436 (8px border each side)
  //   → OnCreate 用 dW × dH (DPI 缩放后) 算 child 位置 (e.g. 200% DPI: 720×920)
  //   → child 位置基于 client 错误基准 → 全部出框
  //
  // 修法: Show() 先用 AdjustWindowRectEx 算 outer 尺寸 (client + border),
  //       DPI 缩放同步 (跟 OnCreate 一致 → 用 GetDpiForSystem 跟
  //       GetDpiForWindow 同源 primary monitor)。
  int sysDpi = GetDpiForSystem();
  double sysDpiScale = (sysDpi > 0) ? (double)sysDpi / 96.0 : 1.0;
  if (sysDpiScale < 0.5)
    sysDpiScale = 0.5;
  if (sysDpiScale > 4.0)
    sysDpiScale = 4.0;
  int clientW = (int)(kDialogW * sysDpiScale);
  int clientH = (int)(kDialogH * sysDpiScale);
  RECT rcOuter = {0, 0, clientW, clientH};
  // AdjustWindowRectEx: 给定 client 尺寸 + style + exStyle, 返回 outer 尺寸
  // (包含 border + caption)。menu=FALSE (无 menu)。
  AdjustWindowRectEx(&rcOuter, style, FALSE, exStyle);
  int outerW = rcOuter.right - rcOuter.left;
  int outerH = rcOuter.bottom - rcOuter.top;

  s_hwnd = CreateWindowExW(exStyle, L"FluxingPhrasesDialogV3",
                           L"\x5e38\x7528\x77ed\x8bed",  // 常用短语
                           style, CW_USEDEFAULT, CW_USEDEFAULT, outerW, outerH,
                           nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  if (!s_hwnd) {
    std::wcerr << L"[PhrasesDialog] CreateWindowExW failed, err="
               << GetLastError() << std::endl;
    return;
  }

  // 4. 居中 + 强制置顶 (用 outer 尺寸, 不是 client — 居中按 outer 算)
  CenterOnPrimaryMonitor(s_hwnd, outerW, outerH);
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  // v0.19.0.35 (Phase C P0-2): 删 SetWindowRgn 圆角 region。
  // 原因: 圆角 region 切 IME composition/candidate window, 输入框 (s_hInput)
  // 中文 IME 显示不全 (用户报"输入框无法输入中文,只能输入英文")。
  // 妥协: 不画圆角 chrome, 改用 default region (IME 完整可见)。
  // 视觉: chrome 边界是 default square, 损失 macOS Liquid Glass 圆角效果,
  // 但 IME 中文输入工作 优先。
  // 5. 圆角 - 暂时禁用 (Phase C 后续 polish)
  // {
  //   HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
  //                                 kDlgRadius * 2, kDlgRadius * 2);
  //   if (rgn) {
  //     SetWindowRgn(s_hwnd, rgn, TRUE);
  //   }
  // }
  ShowWindow(s_hwnd, SW_SHOW);
  UpdateWindow(s_hwnd);

  // v0.19.0.39 (Phase F fix Bug 1 + Bug 3): 强制 foreground 让 keyboard events
  // 路由到 dialog 真 root cause (装机反馈): QuickPanel button 启动
  // PhrasesDialog 时, QuickPanelDialog 仍是
  //   foreground, 键盘事件 (↑↓/Enter/Esc) 发到 QuickPanel, 不传 PhrasesDialog
  //   (双击能 work 是因为 WM_LBUTTONDBLCLK 是 mouse event, mouse 直接命中
  //   ListView 触发). Alt+. 路径已 work (hotkey 触发自动让 WeaselServer 进
  //   foreground).
  // 修法 (Option C 双保险):
  //   1. AllowSetForegroundWindow(ASFW_ANY) 拿抢 foreground 锁 (Vista+
  //   foreground lock)
  //   2. SetForegroundWindow(s_hwnd) 强制 foreground 让 dialog 收 keyboard
  //   events (按钮路径配套: WeaselServerApp.cpp onPhrases lambda 先
  //   QuickPanelDialog::Hide() 释放
  //    foreground, 配合 Show() 的 SetForegroundWindow 完成 foreground
  //    transition)
  s_allowSetForegroundFn(ASFW_ANY);
  s_setForegroundFn(s_hwnd);

  s_state = State_Browsing;
  s_showTime = GetTickCount();
}

void PhrasesDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    FlushSave();  // 最后写一次
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hInput = nullptr;
  s_hBtnAddTop = nullptr;
  s_hList = nullptr;
  // v0.19.0.41 删 s_hBtnAdd
  s_hBtnEdit = s_hBtnDel = s_hBtnCancel = nullptr;
  s_state = State_Hidden;
  m_selectedIndex = -1;
  // 复位 drag state (避免下次 Show 还在 drag)
  s_isDragging = false;
  s_longPressActive = false;
}

void PhrasesDialog::SetInjectFn(InjectFn fn) {
  s_injectFn = fn;
}

void PhrasesDialog::SetYamlPath(const std::wstring& path) {
  s_yamlPath = path;
}

void PhrasesDialog::SetSetForegroundFn(SetForegroundFn fn) {
  s_setForegroundFn = fn;
}

void PhrasesDialog::SetAllowSetForegroundFn(AllowSetForegroundFn fn) {
  s_allowSetForegroundFn = fn;
}

std::vector<PhrasesDialog::Phrase>& PhrasesDialog::MutablePhrases() {
  return m_phrases;
}

const std::vector<PhrasesDialog::Phrase>& PhrasesDialog::Phrases() {
  return m_phrases;
}

// ===== YAML I/O =====

std::wstring PhrasesDialog::Trim(const std::wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r'))
    ++a;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r'))
    --b;
  return s.substr(a, b - a);
}

std::wstring PhrasesDialog::Unquote(const std::wstring& s) {
  std::wstring t = Trim(s);
  if (t.size() >= 2 && t.front() == L'"' && t.back() == L'"') {
    return t.substr(1, t.size() - 2);
  }
  if (t.size() >= 2 && t.front() == L'\'' && t.back() == L'\'') {
    return t.substr(1, t.size() - 2);
  }
  return t;
}

bool PhrasesDialog::LoadPhrases(const std::wstring& path,
                                std::vector<Phrase>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;

  std::string content((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

  if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }

  std::wstring wtext;
  if (!content.empty()) {
    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                            static_cast<int>(content.size()), nullptr, 0);
    if (wlen > 0) {
      wtext.resize(wlen);
      MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                          static_cast<int>(content.size()), &wtext[0], wlen);
    }
  }

  out.clear();
  size_t pos = 0;
  Phrase cur;
  bool inPhrase = false;
  while (pos <= wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos)
      eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r')
      line.pop_back();
    pos = eol + 1;

    auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == L'#')
      continue;

    if (trimmed.size() >= 2 && trimmed[0] == L'-' &&
        (trimmed[1] == L' ' || trimmed[1] == L'\t')) {
      if (inPhrase)
        out.push_back(cur);
      cur = Phrase();
      inPhrase = true;

      auto rest = trimmed.substr(2);
      auto trimmedRest = Trim(rest);
      // v0.19.0.35 (Phase C P0-4): 恢复 category 字段解析 (v0.19.0.25 引入)
      // YAML 格式: phrases: [ { category: 常用, text: 你好 } ]
      if (trimmedRest.size() >= 9 && trimmedRest.substr(0, 9) == L"category:") {
        cur.category = Unquote(trimmedRest.substr(9));
      } else if (trimmedRest.size() >= 5 &&
                 trimmedRest.substr(0, 5) == L"text:") {
        // 兼容老 YAML 格式 (text: 行)
        cur.text = Unquote(trimmedRest.substr(5));
      }
    } else if (inPhrase) {
      auto trimmedFull = Trim(line);
      // v0.19.0.35: 解析 category: / text: 行 (一行一个字段)
      if (trimmedFull.size() >= 9 && trimmedFull.substr(0, 9) == L"category:") {
        cur.category = Unquote(trimmedFull.substr(9));
      } else if (trimmedFull.size() >= 5 &&
                 trimmedFull.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmedFull.substr(5));
      }
      // 兼容老格式: 单行 phrase text (无 category, 无 text: 前缀)
      else if (!trimmedFull.empty() && trimmedFull[0] != L'-' &&
               trimmedFull[0] != L'#') {
        cur.text = trimmedFull;
      }
    }
    if (pos > wtext.size())
      break;
  }
  if (inPhrase)
    out.push_back(cur);
  return true;
}

bool PhrasesDialog::SavePhrases(const std::wstring& path,
                                const std::vector<Phrase>& data) {
  std::string utf8;
  utf8 += "\xEF\xBB\xBF";  // UTF-8 BOM

  auto appendLine = [&utf8](const std::wstring& wline) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                                  static_cast<int>(wline.size()), nullptr, 0,
                                  nullptr, nullptr);
    if (len > 0) {
      std::string buf(len, 0);
      WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                          static_cast<int>(wline.size()), &buf[0], len, nullptr,
                          nullptr);
      utf8 += buf;
    }
    utf8 += "\r\n";
  };

  appendLine(
      L"# phrases.yaml \u2014 Fluxing \u5e38\u7528\u77ed\u8bed (v0.19.0.35)");
  appendLine(
      L"# schema: phrases: [ { category: \u5206\u7c7b, text: \u77ed\u8bed } ]");
  appendLine(L"");
  appendLine(L"phrases:");
  for (const auto& p : data) {
    // v0.19.0.36 (Phase D fix): escape lambda 移出 if 避免 nested lambda 缩进错
    auto escape = [](const std::wstring& s) {
      std::wstring r;
      for (wchar_t c : s) {
        if (c == L'\\')
          r += L"\\\\";
        else if (c == L'"')
          r += L"\\\"";
        else
          r += c;
      }
      return r;
    };
    // v0.19.0.35 (Phase C P0-4): 写 category + text
    // 格式: 缩进 2 空格, 分类优先 (老格式: 无 category)
    if (!p.category.empty()) {
      appendLine(L"  - category: \"" + escape(p.category) + L"\"");
      appendLine(L"    text: \"" + escape(p.text) + L"\"");
    } else {
      // 兼容老格式 (无 category)
      appendLine(L"  - text: \"" + escape(p.text) + L"\"");
    }
  }

  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
  return f.good();
}

// ===== SendInput =====

void PhrasesDialog::DefaultInject(const std::wstring& text) {
  if (text.empty())
    return;
  std::vector<INPUT> inputs;
  inputs.reserve(text.size() * 2);
  for (wchar_t c : text) {
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wScan = c;
    down.ki.dwFlags = KEYEVENTF_UNICODE;
    inputs.push_back(down);

    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    inputs.push_back(up);
  }
  SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void PhrasesDialog::InjectText(const std::wstring& text) {
  if (s_injectFn)
    s_injectFn(text);
}

// ===== v0.19.0.32: 立即写盘 (无 debounce,task spec 简化) =====

void PhrasesDialog::FlushSave() {
  if (!s_yamlPath.empty() && !m_phrases.empty()) {
    if (!SavePhrases(s_yamlPath, m_phrases)) {
      std::wcerr << L"[PhrasesDialog] WARN: SavePhrases failed, "
                 << L"preserved in-memory" << std::endl;
    }
  }
}

// ===== WndProc =====

LRESULT CALLBACK PhrasesDialog::WndProc(HWND hwnd,
                                        UINT msg,
                                        WPARAM wp,
                                        LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      return OnCreate(hwnd);
    case WM_DESTROY:
      return OnDestroy(hwnd);
    case WM_PAINT:
      return OnPaint(hwnd);
    case WM_KEYDOWN:
      return OnKeyDown(hwnd, wp);
    case WM_NOTIFY:
      return OnNotify(hwnd, lp);
    case WM_COMMAND:
      return OnCommand(hwnd, wp);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
      return OnCtlColor(hwnd, wp, lp);
    // v0.19.0.41 (Feature: resize + long-press drag)
    case WM_NCHITTEST:
      return OnNcHitTest(hwnd, lp);
    case WM_LBUTTONDOWN:
      return OnLButtonDown(hwnd, wp, lp);
    case WM_LBUTTONUP:
      return OnLButtonUp(hwnd, wp, lp);
    case WM_MOUSEMOVE:
      return OnMouseMove(hwnd, wp, lp);
    case WM_TIMER:
      return OnTimer(hwnd, wp);
    case WM_GETMINMAXINFO:
      return OnGetMinMaxInfo(hwnd, lp);
    case WM_ACTIVATEAPP: {
      if (wp == FALSE) {
        DWORD nowTick = GetTickCount();
        if ((nowTick - s_showTime) >= kShowGraceMs) {
          Hide();
        }
      }
      return 0;
    }
    case WM_KILLFOCUS: {
      // v0.19.0.43 (Bug 2.2 drag 修复): drag 中不 Hide
      // 装机 v0.19.0.42 user 反馈 "拖动时 UI 可能消失" — 拖动期间 SetCapture
      // 偶尔会触发 WM_KILLFOCUS (e.g. dialog 失去 active 状态), grace 时间已
      // 过 (kShowGraceMs=2000ms), 老逻辑就 Hide()。drag 中应该跳过 Hide,
      // 保持 dialog 可见直到 mouse up。
      if (s_isDragging) {
        return 0;
      }
      // v0.19.0.32: 检查新焦点是否为本 dialog 内的子控件
      HWND newFocus = (HWND)wp;
      auto isChild = [newFocus](HWND h) { return h && newFocus == h; };
      // v0.19.0.41: 删 s_hBtnAdd (重复 Add 按钮), 现在 4 按钮 (1 top + 3 bot)
      if (isChild(s_hInput) || isChild(s_hBtnAddTop) || isChild(s_hList) ||
          isChild(s_hBtnEdit) || isChild(s_hBtnDel) || isChild(s_hBtnCancel)) {
        return 0;
      }
      DWORD nowTick = GetTickCount();
      if ((nowTick - s_showTime) >= kShowGraceMs) {
        Hide();
      }
      return 0;
    }
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

LRESULT PhrasesDialog::OnCreate(HWND hwnd) {
  // ===== v0.19.0.41 (Bug 2: UI 过小): DPI 缩放 =====
  // GetDpiForWindow 返回 dialog 所在 monitor 的 effective DPI (PerMonitorV2
  // 启用时 = 当前 monitor DPI, 否则 = system DPI)。96 是 base (100% 缩放)。
  // 4K 屏 200% 缩放 → 192 → s_dpiScale = 2.0 → 所有物理像素 ×2。
  int dpi = GetDpiForWindow(hwnd);
  double dpiScale = (dpi > 0) ? (double)dpi / 96.0 : 1.0;
  // Clamp 防止异常 DPI (e.g. 50% 或 500% 缩放) 让 dialog 失真
  if (dpiScale < 0.5)
    dpiScale = 0.5;
  if (dpiScale > 4.0)
    dpiScale = 4.0;
  s_dpiScale = dpiScale;

  // 缩放所有尺寸 — 用 const int 避免后续类型混乱
  const int dW = (int)(kDialogW * dpiScale);
  const int dH = (int)(kDialogH * dpiScale);
  const int titleH = (int)(kTitleH * dpiScale);
  const int inputH = (int)(kInputH * dpiScale);
  const int inputW = (int)(kInputW * dpiScale);
  const int btnAddTopW = (int)(kBtnAddTopW * dpiScale);
  const int btnH = (int)(kBtnH * dpiScale);
  const int btnW = (int)(kBtnW * dpiScale);
  const int btnGap = (int)(kBtnGap * dpiScale);
  const int btnMarginX = (int)(kBtnMarginX * dpiScale);
  const int gap = (int)(kGap * dpiScale);
  const int fontSize = (int)(kUiFontSize * dpiScale);

  // v0.19.0.32 UX redo: layout = title + (input + AddTop) row + list + 3
  // bottom buttons (v0.19.0.41 删 4 → 3, 移除重复 Add)
  kListH_phys = dH - titleH - inputH - btnH - 3 * gap;
  kBtnY_phys = titleH + inputH + kListH_phys + 2 * gap;
  // v0.19.0.42 fix (Stop hook SUGGESTION 3): floor 也 × s_dpiScale 保持一致
  // (80 物理 = 40 逻辑 on 200% DPI, 跟 DPI 缩放意图矛盾)
  const int kMinListHPhys = (int)(80 * dpiScale);
  if (kListH_phys < kMinListHPhys)
    kListH_phys = kMinListHPhys;
  if (kBtnY_phys + btnH > dH)
    kBtnY_phys = dH - btnH - 2;

  s_hFontUi = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
  HFONT hfUi = s_hFontUi;

  // v0.19.0.32: 顶部 input + Add 按钮 (同 row, 左右分布)
  int inputY = titleH + gap;
  int inputX = btnMarginX;
  s_hInput = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
      inputX, inputY, inputW, inputH, hwnd, reinterpret_cast<HMENU>(ID_INPUT),
      GetModuleHandle(nullptr), nullptr);
  if (s_hInput && hfUi) {
    SendMessageW(s_hInput, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    // v0.19.0.35 (Phase C P0-2): 输入框支持中文 IME
    // 原因: 默认创建 Edit control 没设输入法关联, GUI app 加载时无 IME 焦点。
    // 显式 ImmAssociateContext 启用中文 IME (用户报"无法输入中文,只能英文")。
    // imm32.lib 已在 WeaselServer.vcxproj 隐式 link (comdlg32.h 间接引用)。
    //
    // v0.19.0.36 (P2 hotfix): 5068922 commit 把 ImmReleaseContext(himc) 改
    //   ImmDestroyContext(himc) — 错的! ImmDestroyContext 销毁 himc = 关联的
    //   IME context 销毁 = s_hInput IME 死。revert 回 2 参 ImmReleaseContext。
    //
    // v0.19.0.43 (Bug 1.2 真修): ImmReleaseContext **仍然错** — 它把刚关联的
    //   context refcount 从 1 减到 0 → 销毁 context → s_hInput 重新无 IME。
    //   装机 v0.19.0.42 user 报告"输入框仍无法输入中文", 装机端 5 项 verify
    //   也确认 IME fail。
    //   **正确 Win32 模式**: ImmCreateContext 创建 (refcount=1) +
    //   ImmAssociateContext 转移 ownership 给 hwnd (refcount 内部 +1 = 2) →
    //   **不调** ImmReleaseContext (那是给 ImmGetContext 配对的)。Context 跟
    //   hwnd 一起销毁 (DestroyWindow 或 ImmAssociateContext(NULL))。 Per MSDN:
    //   "The ImmAssociateContext function associates the input context with the
    //   specified window. The application should not call ImmReleaseContext for
    //   a handle returned by ImmAssociateContext."
    HIMC himc = ImmCreateContext();
    if (himc) {
      ImmAssociateContext(s_hInput, himc);
      // 不调 ImmReleaseContext — context 归 s_hInput 所有, 跟 hwnd 一起销毁
    }
  }

  // v0.19.0.32: 顶部 Add 按钮 (input 右侧) — 唯一 Add (v0.19.0.41)
  int btnTopX = inputX + inputW + gap;
  s_hBtnAddTop = CreateWindowExW(
      0, L"BUTTON", L"+ \x6dfb\x52a0",  // + 添加
      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON, btnTopX, inputY,
      btnAddTopW, inputH, hwnd, reinterpret_cast<HMENU>(ID_BTN_ADD_TOP),
      GetModuleHandle(nullptr), nullptr);
  if (s_hBtnAddTop && hfUi) {
    SendMessageW(s_hBtnAddTop, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi),
                 TRUE);
  }

  // v0.19.0.32: ListView (单列 "phrase text")
  int listY = inputY + inputH + gap;
  DWORD listEx = WS_EX_CLIENTEDGE;
  DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                    LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
  s_hList = CreateWindowExW(listEx, WC_LISTVIEWW, L"", listStyle, gap, listY,
                            dW - 2 * gap, kListH_phys, hwnd,
                            reinterpret_cast<HMENU>(ID_LIST),
                            GetModuleHandle(nullptr), nullptr);
  if (s_hList && hfUi) {
    SendMessageW(s_hList, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }
  // v0.19.0.32: ListView 加单列 (P2 polish: header text 空, 避免跟下面 row
  // "短语" 视觉混淆)
  {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = const_cast<wchar_t*>(
        L"");  // 空 — 整个 ListView 内容都是短语, header "短语" 冗余
    col.cx = dW - 2 * gap - 4;
    col.iSubItem = 0;
    ListView_InsertColumn(s_hList, 0, &col);
  }
  // 全行选择
  ListView_SetExtendedListViewStyle(s_hList, LVS_EX_FULLROWSELECT);

  // v0.19.0.41: 底部 3 按钮 (Edit / Delete / Cancel) — 删重复的 Add
  // v0.19.0.32 原 4 按钮 (Add / Edit / Delete / Cancel), 但顶部已有
  // s_hBtnAddTop, 底部 Add 跟它功能一样, 算"重复" (用户报 Bug 4)。删底部 Add,
  // 留 3 按钮。
  int btnY = kBtnY_phys;
  int totalW = btnW * 3 + btnGap * 2;
  int btnX = (dW - totalW) / 2;
  const wchar_t* kBtnLabels[3] = {
      L"\u270e \x7f16\x8f91",  // ✎ 编辑
      L"- \x5220\x9664",       // - 删除
      L"\x53d6\x6d88",         // 取消
  };
  UINT kBtnIds[3] = {ID_BTN_EDIT, ID_BTN_DEL, ID_BTN_CANCEL};
  HWND* kBtnHwnds[3] = {&s_hBtnEdit, &s_hBtnDel, &s_hBtnCancel};
  for (int i = 0; i < 3; ++i) {
    *kBtnHwnds[i] = CreateWindowExW(
        0, L"BUTTON", kBtnLabels[i],
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON, btnX, btnY,
        btnW, btnH, hwnd, reinterpret_cast<HMENU>(kBtnIds[i]),
        GetModuleHandle(nullptr), nullptr);
    btnX += btnW + btnGap;
  }

  // 字体
  if (hfUi) {
    for (int i = 0; i < 3; ++i) {
      SendMessageW(*kBtnHwnds[i], WM_SETFONT, reinterpret_cast<WPARAM>(hfUi),
                   TRUE);
    }
  }

  PopulateList(s_hList);

  // v0.19.0.35 (Phase C P0-3): 默认选第一条 (用户报"未选中第一条")
  // 同时把焦点放 ListView (后续键盘 handler 立刻可用)。
  if (s_hList && ListView_GetItemCount(s_hList) > 0) {
    ListView_SetItemState(s_hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    m_selectedIndex = 0;
  }
  if (s_hList) {
    SetFocus(s_hList);
  }

  return 0;
}

LRESULT PhrasesDialog::OnDestroy(HWND hwnd) {
  if (s_hFontUi) {
    DeleteObject(s_hFontUi);
    s_hFontUi = nullptr;
  }
  s_hwnd = nullptr;
  s_hInput = nullptr;
  s_hBtnAddTop = nullptr;
  s_hList = nullptr;
  // v0.19.0.41 删 s_hBtnAdd
  s_hBtnEdit = s_hBtnDel = s_hBtnCancel = nullptr;
  return 0;
}

LRESULT PhrasesDialog::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  // v0.19.0.41 (Bug 2 + Feature): 用实际 client 尺寸 (DPI-scaled + resize 后)
  // 而非 hardcode kDialogW/kDialogH — 否则 resize 后 chrome 错位, DPI 2x 屏
  // chrome 仍画 360×460 (只占左上角 1/4)。
  int paintW = rc.right - rc.left;
  int paintH = rc.bottom - rc.top;
  int paintRadius = (int)(kDlgRadius * s_dpiScale);

  ModalChrome::PaintBackgroundAndBorder(hdc, paintW, paintH, kBgTop, kBgBot,
                                        paintRadius, kBorderColor);

  // 标题文字
  HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HFONT hfOld = static_cast<HFONT>(SelectObject(hdc, hf));
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, kTextColor);
  RECT titleRc = {0, 0, paintW, (int)(kTitleH * s_dpiScale)};
  DrawTextW(hdc, L"\x5e38\x7528\x77ed\x8bed", -1, &titleRc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  SelectObject(hdc, hfOld);

  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT PhrasesDialog::OnCtlColor(HWND hwnd, WPARAM wp, LPARAM lp) {
  HDC hdc = reinterpret_cast<HDC>(wp);
  HWND target = reinterpret_cast<HWND>(lp);
  if (target == s_hInput) {
    SetBkColor(hdc, kInputBg);
    SetTextColor(hdc, kTextColor);
    static HBRUSH s_hBrushInput = nullptr;
    if (!s_hBrushInput)
      s_hBrushInput = CreateSolidBrush(kInputBg);
    return reinterpret_cast<LRESULT>(s_hBrushInput);
  }
  return DefWindowProcW(hwnd, WM_CTLCOLOREDIT, wp, lp);
}

LRESULT PhrasesDialog::OnKeyDown(HWND hwnd, WPARAM wp) {
  // Enter on input → 触发 Add 按钮
  // Esc → Hide
  // Delete → 删除选中 item
  // v0.19.0.35 (Phase C P0-4): 完整键盘导航
  // - ↑/↓ 切换 ListView 选中 (MoveSelection helper, 已有可复用)
  // - ← 折叠 TreeView 分类 (TreeView 焦点时)
  // - → 展开 TreeView 分类 + 选第一条子节点
  // - Enter 焦点 ListView → inject 选中 phrase (原逻辑)
  switch (wp) {
    case VK_UP: {
      // 焦点在 list → MoveSelection(-1)
      if (s_hList && GetFocus() == s_hList) {
        MoveSelection(s_hList, -1);
        return 0;
      }
      break;
    }
    case VK_DOWN: {
      // 焦点在 list → MoveSelection(+1)
      if (s_hList && GetFocus() == s_hList) {
        MoveSelection(s_hList, +1);
        return 0;
      }
      break;
    }
    case VK_LEFT: {
      // 折叠 TreeView 当前分类 (TreeView 焦点时)
      // v0.19.0.35: TreeView 控件 (v0.19.0.32 删了, 重新加, 未来 Phase C 接入)
      // 暂时: 折叠未实现 (TreeView 控件待加), break 让 Windows 默认处理
      break;
    }
    case VK_RIGHT: {
      // 展开 TreeView 当前分类 + 选第一条子节点
      // 暂时 break
      break;
    }
    case VK_RETURN: {
      // 焦点在 input → Add (调 ID_BTN_ADD_TOP)
      if (s_hInput && GetFocus() == s_hInput) {
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_BTN_ADD_TOP, BN_CLICKED),
                     0);
        return 0;
      }
      // 焦点在 list → inject 选中 phrase text
      if (s_hList && GetFocus() == s_hList && m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        InjectText(m_phrases[m_selectedIndex].text);
        Hide();
      }
      return 0;
    }
    case VK_ESCAPE:
      Hide();
      return 0;
    case VK_DELETE: {
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        m_phrases.erase(m_phrases.begin() + m_selectedIndex);
        PopulateList(s_hList);
        FlushSave();
        m_selectedIndex = -1;
      }
      return 0;
    }
    case VK_TAB: {
      // v0.19.0.41 (Bug 4): 6 元素 — s_hBtnAdd 删, 加 s_hBtnAddTop + s_hBtnDel
      //   (旧 array 漏了 s_hBtnAddTop + s_hBtnDel, 顺手修)
      HWND order[6] = {s_hInput,   s_hBtnAddTop, s_hList,
                       s_hBtnEdit, s_hBtnDel,    s_hBtnCancel};
      HWND cur = GetFocus();
      int start = 0;
      for (int i = 0; i < 6; ++i) {
        if (order[i] == cur) {
          start = (i + 1) % 6;
          break;
        }
      }
      SetFocus(order[start]);
      return 0;
    }
  }
  return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
}

// v0.19.0.36 (Phase D fix): ListView selected index move helper.
// fa196049 (v0.19.0.35) OnKeyDown VK_UP/VK_DOWN 调用本函数但 ship 时漏定义
// → unresolved external / link error。补 definition 让 keyboard nav 路径可链接
// + 可测试。 delta = -1 (上移) / +1 (下移);循环 wrap-around;空 list / 无选中
// fallback。
void PhrasesDialog::MoveSelection(HWND hList, int delta) {
  if (!hList)
    return;
  int count = ListView_GetItemCount(hList);
  if (count <= 0)
    return;
  int cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  if (cur < 0)
    cur = 0;
  int next = (cur + delta + count) % count;
  ListView_SetItemState(hList, next, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  ListView_EnsureVisible(hList, next, FALSE);
  // 同步 m_selectedIndex (避免 VK_RETURN 路径读到 stale 值;
  // LVN_ITEMCHANGED 异步触发可能晚于同帧的 Enter 处理)
  m_selectedIndex = next;
}

LRESULT PhrasesDialog::OnNotify(HWND hwnd, LPARAM lp) {
  LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lp);
  if (!pnm)
    return 0;
  if (pnm->idFrom == ID_LIST) {
    switch (pnm->code) {
      case LVN_ITEMCHANGED: {
        LPNMLISTVIEW pn = reinterpret_cast<LPNMLISTVIEW>(lp);
        if ((pn->uChanged & LVIF_STATE) && (pn->uNewState & LVIS_SELECTED)) {
          m_selectedIndex = pn->iItem;
          // 选中 item → 自动 fill 顶部 input
          if (s_hInput && m_selectedIndex >= 0 &&
              m_selectedIndex < static_cast<int>(m_phrases.size())) {
            SetWindowTextW(s_hInput, m_phrases[m_selectedIndex].text.c_str());
          }
        }
        return 0;
      }
      case NM_DBLCLK: {
        // 双击 → inject
        // v0.19.0.36 (P2 polish, 用户装机反馈 "双击不上屏"):
        //   真 root cause 是 Hide() 重置 m_selectedIndex = -1 (Hide line 196),
        //   原 handler 顺序 InjectText → Hide 改成 Hide → InjectText 后,
        //   InjectText 读 m_phrases[m_selectedIndex] 越界 (-1) → SendInput
        //   空字符串。
        //   **正确修法**: 捕获 idx + text 本地变量 (Hide 前), InjectText 用本地
        //   text — 不依赖 m_selectedIndex (Hide 后会被清)。
        //   Hide 先 InjectText 后 的顺序是因为: 销毁 modal dialog 后 foreground
        //   自动 还给原 app, SendInput 才到原 app (否则发到 dialog 自身)。
        LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
        if (pia && pia->iItem >= 0 &&
            pia->iItem < static_cast<int>(m_phrases.size())) {
          int idx = pia->iItem;
          std::wstring text = m_phrases[idx].text;
          m_selectedIndex = idx;  // 同步, 后续 NM_RETURN 也能用
          Hide();
          InjectText(text);
        }
        return 0;
      }
      case NM_RETURN: {
        // v0.19.0.36 (P2 polish, 用户装机反馈 "回车不上屏"):
        //   Enter 焦点 ListView + selected item → ListView 默认发 NM_RETURN 给
        //   parent。 原 OnNotify 没收, return 0 → 不 inject。修法: 跟 NM_DBLCLK
        //   同路径, 同样捕获本地 idx + text (Hide 重置 m_selectedIndex)。
        if (m_selectedIndex >= 0 &&
            m_selectedIndex < static_cast<int>(m_phrases.size())) {
          int idx = m_selectedIndex;
          std::wstring text = m_phrases[idx].text;
          Hide();
          InjectText(text);
        }
        return 0;
      }
      case NM_CLICK: {
        LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
        if (pia && pia->iItem >= 0 &&
            pia->iItem < static_cast<int>(m_phrases.size())) {
          m_selectedIndex = pia->iItem;
          if (s_hInput) {
            SetWindowTextW(s_hInput, m_phrases[m_selectedIndex].text.c_str());
          }
        }
        return 0;
      }
      case LVN_KEYDOWN: {
        // v0.19.0.40 (Phase F Bug 3 续修): ListView 焦点时按 Esc, ListView
        // 自身不处理,
        //   转 LVN_KEYDOWN 给 parent. 原 OnNotify 缺此 case → return 0 → Esc
        //   无响应. Test 24 (sandbox) false-positive 因为直接 dispatch
        //   WM_KEYDOWN 到 dialog WndProc, 不模拟 ListView 焦点 + WM_NOTIFY
        //   路径. 修法: OnNotify 加 case LVN_KEYDOWN 处理 VK_ESCAPE → Hide. (跟
        //   NM_DBLCLK / NM_RETURN 模式一致, 不 subclass ListView WndProc.)
        LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
        if (pnkd && pnkd->wVKey == VK_ESCAPE) {
          Hide();
        }
        return 0;
      }
    }
  }
  return 0;
}

LRESULT PhrasesDialog::OnCommand(HWND hwnd, WPARAM wp) {
  switch (LOWORD(wp)) {
    case ID_BTN_ADD_TOP: {
      // 顶部 Add 按钮: 读 input → push_back → PopulateList → FlushSave → clear
      if (s_hInput && IsWindow(s_hInput)) {
        wchar_t buf[1024] = {};
        GetWindowTextW(s_hInput, buf, 1024);
        std::wstring text = buf;
        if (!text.empty()) {
          Phrase np;
          np.text = text;
          m_phrases.push_back(np);
          PopulateList(s_hList);
          FlushSave();
          // 清空 input + 保持焦点在 input (方便连续添加)
          SetWindowTextW(s_hInput, L"");
          SetFocus(s_hInput);
        }
      }
      return 0;
    }
    // v0.19.0.41 删 ID_BTN_ADD case — 底部 Add 按钮已删, 跟 s_hBtnAddTop 重复
    case ID_BTN_EDIT: {
      // 编辑: 读 input → 更新 m_phrases[m_selectedIndex] → FlushSave
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size()) && s_hInput &&
          IsWindow(s_hInput)) {
        wchar_t buf[1024] = {};
        GetWindowTextW(s_hInput, buf, 1024);
        std::wstring text = buf;
        if (!text.empty()) {
          m_phrases[m_selectedIndex].text = text;
          PopulateList(s_hList);
          FlushSave();
        }
      }
      return 0;
    }
    case ID_BTN_DEL: {
      // 删除: erase → PopulateList → FlushSave
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        m_phrases.erase(m_phrases.begin() + m_selectedIndex);
        PopulateList(s_hList);
        FlushSave();
        m_selectedIndex = -1;
        if (s_hInput) {
          SetWindowTextW(s_hInput, L"");
        }
      }
      return 0;
    }
    case ID_BTN_CANCEL:
      Hide();
      return 0;
  }
  return DefWindowProcW(hwnd, WM_COMMAND, wp, 0);
}

// ===== List populate =====

void PhrasesDialog::PopulateList(HWND hList) {
  PopulateListImpl(hList);
}

int PhrasesDialog::PopulateListCount(HWND hList) {
  return PopulateListImpl(hList);
}

int PhrasesDialog::PopulateListImpl(HWND hList) {
  if (!hList)
    return 0;
  s_hList = hList;
  ListView_DeleteAllItems(hList);

  int inserted = 0;
  for (size_t i = 0; i < m_phrases.size(); ++i) {
    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = static_cast<int>(i);
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(m_phrases[i].text.c_str());
    item.lParam = static_cast<LPARAM>(i);
    ListView_InsertItem(hList, &item);
    ++inserted;
  }
  return inserted;
}

// ===== 工具 =====

void PhrasesDialog::CenterOnPrimaryMonitor(HWND hwnd, int w, int h) {
  HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  GetMonitorInfoW(hm, &mi);
  int cx = (mi.rcWork.left + mi.rcWork.right - w) / 2;
  int cy = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
  SetWindowPos(hwnd, nullptr, cx, cy, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

// ===== v0.19.0.41 (Feature: resize + long-press drag) =====

LRESULT PhrasesDialog::OnNcHitTest(HWND hwnd, LPARAM lp) {
  // v0.19.0.42 (Stop hook SUGGESTION 4): 改用 DefWindowProc 默认行为。
  // DefWindowProc 给 WS_THICKFRAME 边框返回 HTLEFT/HTRIGHT/HTTOP/HTBOTTOM
  // (让 Windows 处理 resize), 内部区域返回 HTCLIENT (让我们的 OnLButtonDown
  // 长按 500ms 后触发 drag; 不要在这里返回 HTCAPTION — 那会让 Windows 默认
  // click+drag 跳过 long press 逻辑)。之前注释误导地说"Chrome 区域返回
  // HTCLIENT",其实 DefWindowProc 内部已经处理 client + border 区分。
  return DefWindowProcW(hwnd, WM_NCHITTEST, 0, lp);
}

LRESULT PhrasesDialog::OnLButtonDown(HWND hwnd, WPARAM wp, LPARAM lp) {
  int x = LOWORD(lp);
  int y = HIWORD(lp);
  POINT ptClient = {x, y};
  // 1. 检查鼠标是否在子控件上 (input / list / 按钮) — 如果是, 不启动 long
  //    press, 让 Windows 默认处理 (input 获焦点, button click, list select 等)
  HWND child = ChildWindowFromPoint(hwnd, ptClient);
  if (child && child != hwnd) {
    return DefWindowProcW(hwnd, WM_LBUTTONDOWN, wp, lp);
  }

  // 2. 在 chrome 区域 (标题栏 / input 上下空隙 / list 周围) — 启动 long press
  StartLongPressTimer(hwnd, x, y);
  return 0;
}

LRESULT PhrasesDialog::OnLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
  // 释放 drag + 取消 long press (二者互斥, 都在的话 cancel 全部)
  EndDrag(hwnd);
  CancelLongPress(hwnd);
  return 0;
}

LRESULT PhrasesDialog::OnMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
  if (s_isDragging) {
    // v0.19.0.43 (Bug 2.2 drag 修复): 用 GetCursorPos (screen coords) 而非
    // LOWORD(lp)/HIWORD(lp) (client coords)。
    // 装机 v0.19.0.42 user 反馈 "拖动 UI 剧烈晃动" — 原版用 client coords 计算
    // delta: dialog 一移动 (SetWindowPos), 鼠标在 client area 的相对位置跳变
    // → 下一次 WM_MOUSEMOVE 的 client coord 跟 origin 比对 → 数学错位
    // → SetWindowPos 跳到错位置 → 视觉上"剧烈晃动"。
    // 修法: 全程用 screen coords (GetCursorPos), dialog 移动不影响 mouse screen
    // 位置, delta 算式保持稳定。
    POINT pt;
    GetCursorPos(&pt);
    int newX = s_dragWndOrigin.x + (pt.x - s_dragOrigin.x);
    int newY = s_dragWndOrigin.y + (pt.y - s_dragOrigin.y);
    SetWindowPos(hwnd, nullptr, newX, newY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  return DefWindowProcW(hwnd, WM_MOUSEMOVE, wp, lp);
}

LRESULT PhrasesDialog::OnTimer(HWND hwnd, WPARAM wp) {
  if (wp == kLongPressTimerId) {
    // 500ms 到 — 进入 drag mode
    KillTimer(hwnd, kLongPressTimerId);
    s_longPressActive = false;
    BeginDrag(hwnd);
    return 0;
  }
  return DefWindowProcW(hwnd, WM_TIMER, wp, 0);
}

LRESULT PhrasesDialog::OnGetMinMaxInfo(HWND hwnd, LPARAM lp) {
  MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lp);
  if (mmi) {
    // v0.19.0.42 fix (Stop hook BLOCKER): kMinW/kMaxW 是 raw 96-DPI 像素,
    // 必须 × s_dpiScale。否则高 DPI 屏 (e.g. 200%) 初始尺寸 720×920 (raw
    // max 1200×900) — height 920 > raw max 900 → WM_GETMINMAXINFO 强制 cap
    // 到 1200×900 比初始尺寸还小, 跟 Bug 2 DPI 缩放逻辑矛盾。
    mmi->ptMinTrackSize.x = (LONG)(kMinW * s_dpiScale);
    mmi->ptMinTrackSize.y = (LONG)(kMinH * s_dpiScale);
    mmi->ptMaxTrackSize.x = (LONG)(kMaxW * s_dpiScale);
    mmi->ptMaxTrackSize.y = (LONG)(kMaxH * s_dpiScale);
  }
  return 0;
}

void PhrasesDialog::StartLongPressTimer(HWND hwnd, int x, int y) {
  s_longPressActive = true;
  // v0.19.0.43 (Bug 2.2): 用 GetCursorPos 拿 screen coords (跟 OnMouseMove
  // 一致) x/y 参数是 client coords (从 WM_LBUTTONDOWN lp 来), 不用, 用
  // GetCursorPos
  POINT pt;
  GetCursorPos(&pt);
  s_dragOrigin = pt;
  RECT rc;
  GetWindowRect(hwnd, &rc);
  s_dragWndOrigin = {rc.left, rc.top};
  // 第二个参数: 500ms 长按阈值 (kLongPressMs)
  SetTimer(hwnd, kLongPressTimerId, kLongPressMs, nullptr);
}

void PhrasesDialog::CancelLongPress(HWND hwnd) {
  if (s_longPressActive) {
    KillTimer(hwnd, kLongPressTimerId);
    s_longPressActive = false;
  }
}

void PhrasesDialog::BeginDrag(HWND hwnd) {
  s_isDragging = true;
  // SetCapture 让 mouse move 即使鼠标移出 dialog client 区也能收到
  SetCapture(hwnd);
}

void PhrasesDialog::EndDrag(HWND hwnd) {
  if (s_isDragging) {
    s_isDragging = false;
    ReleaseCapture();
  }
}

// 注: v0.19.0.33 (Phase B Bug 2b) commit 8fe29885 删除 RepaintLayered 函数。
//   理由: grep 0 caller (无 caller) — 改走 BeginPaint/EndPaint 路径后, OnPaint
//   在 InvalidateRect 后自动触发, 不再需要显式 RedrawWindow helper。
//   同模块的 ShortcutSettings / UserDictionary 仍保留各自 RepaintLayered
//   (因为它们继续用 WS_EX_LAYERED + UpdateLayeredWindow 模式)。
