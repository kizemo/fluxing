//
// PhrasesDialog.cpp — 常用短语 Modal 实现 v2 (spec 043)
// spec: .specify/specs/043-phrases-ui-v2/design.md
//
#include "stdafx.h"  // v0.19.0.30 fix: 同 UserDictionary.cpp 漏 include。
                     // 之前 PhrasesDialog.cpp 不在 WeaselServer.vcxproj,所以此
                     // 问题被掩盖;v0.19.0.30 加进 vcxproj 后 PCH 扫描失败。
#include "PhrasesDialog.h"
#include "ModalChrome.h"  // v0.19.0.31: 抽出的 BG+Border paint helper

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>

// ===== Static state 定义 =====
HWND PhrasesDialog::s_hwnd = nullptr;
HWND PhrasesDialog::s_hTree = nullptr;
HWND PhrasesDialog::s_hSearch = nullptr;
HWND PhrasesDialog::s_hStatus = nullptr;
HWND PhrasesDialog::s_hToast = nullptr;
HWND PhrasesDialog::s_hBtnAdd = nullptr;
HWND PhrasesDialog::s_hBtnEdit = nullptr;
HWND PhrasesDialog::s_hBtnDel = nullptr;
HWND PhrasesDialog::s_hBtnCancel = nullptr;
HWND PhrasesDialog::s_hBtnSave = nullptr;
HWND PhrasesDialog::s_hEditText = nullptr;
HWND PhrasesDialog::s_hEditCat = nullptr;
std::wstring PhrasesDialog::s_yamlPath;
PhrasesDialog::InjectFn PhrasesDialog::s_injectFn = &PhrasesDialog::DefaultInject;
std::unordered_map<std::wstring, bool> PhrasesDialog::m_expanded;
std::vector<PhrasesDialog::Phrase> PhrasesDialog::m_phrases;
int PhrasesDialog::m_selectedIndex = -1;
DWORD PhrasesDialog::s_showTime = 0;
HFONT PhrasesDialog::s_hFontUi = nullptr;
int   PhrasesDialog::kTreeH_phys = 0;
int   PhrasesDialog::kBtnY_phys = 0;

// v0.19.0.27 新增
PhrasesDialog::State PhrasesDialog::s_state = PhrasesDialog::State_Hidden;
int PhrasesDialog::m_editingIndex = -1;
std::wstring PhrasesDialog::s_searchQuery;
bool PhrasesDialog::m_dirty = false;
DWORD PhrasesDialog::s_lastSaveFailTick = 0;

// ===== 设计常量 (spec 043 §7; FLUENT-UI-TOKENS.md §3.6) =====
namespace {
// 尺寸 (size.modal.*)
constexpr int kDialogW      = 360;
constexpr int kDialogH      = 460;  // v0.19.0.27: 420→460 (+40 给 search)
constexpr int kTitleH       = 30;
constexpr int kSearchH      = 32;   // v0.19.0.27 新增
constexpr int kStatusBarH   = 24;   // v0.19.0.27 新增 (warn only)
constexpr int kBtnH         = 32;
constexpr int kBtnW         = 76;
constexpr int kBtnRadius    = 6;
constexpr int kBtnGap       = 8;
constexpr int kBtnMarginX   = 12;
constexpr int kSearchMarginX = 12;  // v0.19.0.27 新增
constexpr int kGap          = 8;
constexpr int kDlgRadius    = 14;

// 颜色 (color.modal.*)
constexpr COLORREF kBorderColor     = RGB(217, 217, 217);  // 8% 黑 hairline
constexpr COLORREF kBgTop           = RGB(245, 245, 248);
constexpr COLORREF kBgBot           = RGB(220, 222, 230);
constexpr COLORREF kTextColor       = RGB(30, 30, 40);
constexpr COLORREF kSelBg           = RGB(255, 235, 220);
constexpr COLORREF kWarnBg          = RGB(255, 244, 220);  // v0.19.0.27 新增
constexpr COLORREF kWarnText        = RGB(120, 80, 30);   // v0.19.0.27 新增
constexpr COLORREF kSearchBg        = RGB(255, 255, 255);  // v0.19.0.27 新增
constexpr COLORREF kSearchBorder    = RGB(220, 220, 225);  // v0.19.0.27 新增
constexpr COLORREF kEditBg          = RGB(255, 252, 240);  // v0.19.0.27 新增
constexpr COLORREF kButtonBg        = RGB(245, 245, 248);  // v0.19.0.27 新增
constexpr COLORREF kButtonBgHover   = RGB(230, 232, 240);  // v0.19.0.27 新增
constexpr COLORREF kButtonBgPressed = RGB(255, 235, 220);  // v0.19.0.27 新增

// 字体 (font.ui.*)
constexpr int kUiFontSize = 14;

// 子控件 ID (v0.19.0.27 重新编号)
constexpr UINT ID_SEARCH    = 1010;
constexpr UINT ID_TREE      = 1100;
constexpr UINT ID_EDIT_TEXT = 1201;  // v0.19.0.27: inline edit text
constexpr UINT ID_EDIT_CAT  = 1202;  // v0.19.0.27: inline edit category
constexpr UINT ID_BTN_ADD    = 1001;
constexpr UINT ID_BTN_EDIT   = 1002;
constexpr UINT ID_BTN_DEL    = 1003;
constexpr UINT ID_BTN_CANCEL = 1004;
constexpr UINT ID_BTN_SAVE   = 1005;  // v0.19.0.27 新增 (Editing 态)

// item data payload (沿用 v0.19.0.25)
constexpr LPARAM ITEM_DATA_PHRASE(int idx) {
  return static_cast<LPARAM>(idx) & 0x7FFFFFFF;
}
constexpr LPARAM ITEM_DATA_CATEGORY = static_cast<LPARAM>(0xFFFFFFFF);
constexpr bool IS_CATEGORY(LPARAM data) { return data == ITEM_DATA_CATEGORY; }

// 工具:小写转换(搜索用)
std::wstring ToLower(const std::wstring& s) {
  std::wstring r = s;
  CharLowerBuffW(r.data(), static_cast<DWORD>(r.size()));
  return r;
}

// 工具:子串匹配 (大小写不敏感)
bool ContainsCI(const std::wstring& haystack, const std::wstring& needle) {
  if (needle.empty()) return true;
  return ToLower(haystack).find(ToLower(needle)) != std::wstring::npos;
}
}  // namespace

// ===== Public API =====

void PhrasesDialog::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hTree);
    s_showTime = GetTickCount();
    return;
  }

  s_state = State_Loading;

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
    // 失败 fallback 空 list + 状态栏 warn(spec §4)
    std::wcerr << L"[PhrasesDialog] WARN: cannot load " << s_yamlPath
               << L", start with empty list" << std::endl;
    m_phrases.clear();
  }

  // 2. 注册 TreeView 控件类
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TREEVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  // 3. 创建 modal 窗口
  DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  DWORD style = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

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
    wc.lpszClassName = L"FluxingPhrasesDialog";
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

  s_hwnd = CreateWindowExW(exStyle, L"FluxingPhrasesDialog",
                           L"\x5e38\x7528\x77ed\x8bed",  // 常用短语
                           style, CW_USEDEFAULT, CW_USEDEFAULT, kDialogW, kDialogH,
                           nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  if (!s_hwnd) {
    std::wcerr << L"[PhrasesDialog] CreateWindowExW failed, err=" << GetLastError()
               << std::endl;
    return;
  }

  // 4. 居中 + 强制置顶
  CenterOnPrimaryMonitor(s_hwnd, kDialogW, kDialogH);
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  // 5. 圆角
  {
    HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
                                  kDlgRadius * 2, kDlgRadius * 2);
    if (rgn) {
      SetWindowRgn(s_hwnd, rgn, TRUE);
    }
  }
  ShowWindow(s_hwnd, SW_SHOW);
  UpdateWindow(s_hwnd);

  s_state = State_Browsing;
  s_showTime = GetTickCount();
  s_searchQuery.clear();
  m_dirty = false;
  s_lastSaveFailTick = 0;
}

void PhrasesDialog::Hide() {
  // spec §10 risk: Hide() 前必须 KillTimer + FlushSave
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_SAVE);
    KillTimer(s_hwnd, IDT_TOAST);
    FlushSave();  // 最后写一次
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hTree = nullptr;
  s_hSearch = nullptr;
  s_hStatus = nullptr;
  s_hToast = nullptr;
  s_hBtnAdd = s_hBtnEdit = s_hBtnDel = s_hBtnCancel = s_hBtnSave = nullptr;
  s_hEditText = s_hEditCat = nullptr;
  s_state = State_Hidden;
  m_editingIndex = -1;
}

void PhrasesDialog::SetInjectFn(InjectFn fn) { s_injectFn = fn; }

void PhrasesDialog::SetYamlPath(const std::wstring& path) { s_yamlPath = path; }

std::vector<PhrasesDialog::Phrase>& PhrasesDialog::MutablePhrases() {
  return m_phrases;
}

const std::vector<PhrasesDialog::Phrase>& PhrasesDialog::Phrases() {
  return m_phrases;
}

// ===== YAML I/O =====

std::wstring PhrasesDialog::Trim(const std::wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r')) ++a;
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
  if (!f) return false;

  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());

  if (content.size() >= 3 &&
      (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB &&
      (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }

  std::wstring wtext;
  if (!content.empty()) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
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
    if (eol == std::wstring::npos) eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    pos = eol + 1;

    auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == L'#') continue;

    if (trimmed.size() >= 2 && trimmed[0] == L'-' &&
        (trimmed[1] == L' ' || trimmed[1] == L'\t')) {
      if (inPhrase) out.push_back(cur);
      cur = Phrase();
      inPhrase = true;

      auto rest = trimmed.substr(2);
      auto trimmedRest = Trim(rest);
      if (trimmedRest.size() >= 5 && trimmedRest.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmedRest.substr(5));
      }
    } else if (inPhrase) {
      auto trimmedFull = Trim(line);
      if (trimmedFull.size() >= 9 &&
          trimmedFull.substr(0, 9) == L"category:") {
        cur.category = Unquote(trimmedFull.substr(9));
      } else if (trimmedFull.size() >= 5 &&
                 trimmedFull.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmedFull.substr(5));
      }
    }
    if (pos > wtext.size()) break;
  }
  if (inPhrase) out.push_back(cur);
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

  appendLine(L"# phrases.yaml \u2014 Fluxing \u5e38\u7528\u77ed\u8bed (v0.19.0.27)");
  appendLine(L"# schema: phrases: [ { text, category } ]");
  appendLine(L"# category: \"\" represents uncategorized");
  appendLine(L"");
  appendLine(L"phrases:");
  for (const auto& p : data) {
    std::wstring text = p.text;
    std::wstring cat = p.category;
    auto escape = [](const std::wstring& s) {
      std::wstring r;
      for (wchar_t c : s) {
        if (c == L'\\') r += L"\\\\";
        else if (c == L'"') r += L"\\\"";
        else r += c;
      }
      return r;
    };
    appendLine(L"  - text: \"" + escape(text) + L"\"");
    appendLine(L"    category: \"" + escape(cat) + L"\"");
  }

  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
  return f.good();
}

// ===== SendInput =====

void PhrasesDialog::DefaultInject(const std::wstring& text) {
  if (text.empty()) return;
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
  if (s_injectFn) s_injectFn(text);
}

// ===== v0.19.0.27: Debounce save / Toast =====

void PhrasesDialog::ScheduleSave() {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  KillTimer(s_hwnd, IDT_SAVE);
  SetTimer(s_hwnd, IDT_SAVE, kSaveDebounceMs, nullptr);
  m_dirty = true;
}

void PhrasesDialog::FlushSave() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_SAVE);
  }
  if (!m_dirty) return;
  if (s_yamlPath.empty()) return;
  if (SavePhrases(s_yamlPath, m_phrases)) {
    m_dirty = false;
    s_lastSaveFailTick = 0;
  } else {
    // 失败保留 in-memory,记时间,toast 提示
    s_lastSaveFailTick = GetTickCount();
    ShowToast(L"\u4fdd\u5b58\u5931\u8d25\uff0c\u5df2\u4fdd\u7559\u5185\u5b58\uff0c\u5c06\u91cd\u8bd5");  // 保存失败,已保留内存,将重试
  }
}

void PhrasesDialog::ShowToast(const std::wstring& text) {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  if (s_hToast && IsWindow(s_hToast)) {
    DestroyWindow(s_hToast);
    s_hToast = nullptr;
  }
  // toast 在右下角 (宽 200, 高 32)
  const int tw = 240;
  const int th = 32;
  RECT rc;
  GetClientRect(s_hwnd, &rc);
  int tx = rc.right - tw - kGap;
  int ty = rc.bottom - th - kBtnH - 2 * kGap;  // 按钮行上方
  s_hToast = CreateWindowExW(WS_EX_TOPMOST, L"STATIC", text.c_str(),
                             WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE |
                                 WS_CLIPSIBLINGS,
                             tx, ty, tw, th, s_hwnd, nullptr,
                             GetModuleHandle(nullptr), nullptr);
  if (s_hToast && s_hFontUi) {
    SendMessageW(s_hToast, WM_SETFONT, reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
  }
  // 3s 自动隐藏
  SetTimer(s_hwnd, IDT_TOAST, kToastMs, nullptr);
}

void PhrasesDialog::HideToast() {
  if (s_hToast && IsWindow(s_hToast)) {
    DestroyWindow(s_hToast);
  }
  s_hToast = nullptr;
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_TOAST);
  }
}

bool PhrasesDialog::IsToastVisible() {
  return s_hToast && IsWindow(s_hToast);
}

// ===== v0.19.0.27: Search filter =====

int PhrasesDialog::ApplySearchFilter(const std::wstring& searchQuery) {
  s_searchQuery = searchQuery;
  if (!s_hTree || !IsWindow(s_hTree)) return 0;
  int dimmed = 0;

  // helper: extract text-only label (strip ▼/▶ prefix and phrase indent)
  auto ExtractLabel = [](const std::wstring& raw, bool isCat) -> std::wstring {
    std::wstring label = raw;
    if (label.size() >= 2 && label[0] == 0x25BC && label[1] == L' ')
      label = label.substr(2);
    else if (label.size() >= 2 && label[0] == 0x25B6 && label[1] == L' ')
      label = label.substr(2);
    if (!isCat && label.size() >= 3 && label.substr(0, 3) == L"   ")
      label = label.substr(3);
    return label;
  };

  HTREEITEM h = TreeView_GetRoot(s_hTree);
  while (h) {
    wchar_t buf[256] = {};
    TVITEMW ti = {};
    ti.mask = TVIF_TEXT | TVIF_PARAM;
    ti.hItem = h;
    ti.pszText = buf;
    ti.cchTextMax = 256;
    TreeView_GetItem(s_hTree, &ti);

    bool isCat = IS_CATEGORY(ti.lParam);
    std::wstring label = ExtractLabel(buf, isCat);

    bool match = searchQuery.empty() || ContainsCI(label, searchQuery);
    if (isCat && !searchQuery.empty()) {
      HTREEITEM c = TreeView_GetChild(s_hTree, h);
      bool anyChild = false;
      while (c) {
        wchar_t cb[256] = {};
        TVITEMW ci = {};
        ci.mask = TVIF_TEXT | TVIF_PARAM;
        ci.hItem = c;
        ci.pszText = cb;
        ci.cchTextMax = 256;
        TreeView_GetItem(s_hTree, &ci);
        std::wstring cl = ExtractLabel(cb, IS_CATEGORY(ci.lParam));
        bool childMatch = ContainsCI(cl, searchQuery);
        // apply TVIS_CUT to child
        TVITEMW cupd = {};
        cupd.mask = TVIF_STATE;
        cupd.hItem = c;
        cupd.stateMask = TVIS_CUT;
        cupd.state = childMatch ? 0 : TVIS_CUT;
        BOOL okSet = TreeView_SetItem(s_hTree, &cupd);
        if (childMatch) anyChild = true;
        c = TreeView_GetNextSibling(s_hTree, c);
      }
      match = ContainsCI(label, searchQuery) || anyChild;
    }
    // apply TVIS_CUT to this item
    TVITEMW upd = {};
    upd.mask = TVIF_STATE;
    upd.hItem = h;
    upd.stateMask = TVIS_CUT;
    upd.state = match ? 0 : TVIS_CUT;
    TreeView_SetItem(s_hTree, &upd);
    if (!match) ++dimmed;
    h = TreeView_GetNextSibling(s_hTree, h);
  }
  return dimmed;
}

// ===== v0.19.0.27: Inline edit helpers =====

bool PhrasesDialog::BeginInlineEdit(int phraseIndex, const std::wstring& newText,
                                    const std::wstring& newCategory) {
  if (!s_hwnd || !IsWindow(s_hwnd)) return false;
  if (s_state == State_Editing) return false;
  m_editingIndex = phraseIndex;  // -1 = add

  // 在 tree 顶部插入一个 EDIT 用于 text
  // 简化: 用 CreateWindow 直接叠在 tree 上,不实际嵌入 tree 行(spec YAGNI)
  RECT treeRc;
  GetWindowRect(s_hTree, &treeRc);
  POINT pt = {treeRc.left, treeRc.top};
  ScreenToClient(s_hwnd, &pt);
  int editY = pt.y + 8;
  int editW = treeRc.right - treeRc.left - 16;

  s_hEditText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                newText.empty() ? L"" : newText.c_str(),
                                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                pt.x + 8, editY, editW - 100, 22, s_hwnd,
                                reinterpret_cast<HMENU>(ID_EDIT_TEXT),
                                GetModuleHandle(nullptr), nullptr);
  s_hEditCat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                               newCategory.empty() ? L"" : newCategory.c_str(),
                               WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                               pt.x + 8, editY + 26, editW - 100, 22, s_hwnd,
                               reinterpret_cast<HMENU>(ID_EDIT_CAT),
                               GetModuleHandle(nullptr), nullptr);
  if (s_hFontUi) {
    SendMessageW(s_hEditText, WM_SETFONT, reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
    SendMessageW(s_hEditCat, WM_SETFONT, reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
  }
  // v0.19.0.30-fix(issue 3a): SetFocus 之前先 set s_state = State_Editing。
  // 之前 SetFocus 触发 WM_KILLFOCUS(parent 失去焦点),但此时 s_state 还是
  // State_Browsing,WM_KILLFOCUS handler 走 grace check → grace 满后 Hide()。
  // 修法:SetFocus 之前先切到 Editing 态,WM_KILLFOCUS handler 看到 State_Editing
  // 直接 return 0 (cpp:732) 不走 grace Hide 路径。
  s_state = State_Editing;
  if (s_hEditText) {
    SetFocus(s_hEditText);
    if (!newText.empty()) {
      // 全选
      SendMessageW(s_hEditText, EM_SETSEL, 0, -1);
    } else {
      // 占位符提示 - 空 edit 用 cue banner
      // 简化: 不加 cue banner,占位用 "(新短语)" 写 text 字段
      // (实际 UX 改进留给后续 spec)
    }
  }
  SetButtonsForEditing();
  return true;
}

void PhrasesDialog::EnterEditingState(int phraseIndex, bool isAdd) {
  std::wstring initText, initCat;
  if (!isAdd && phraseIndex >= 0 &&
      phraseIndex < static_cast<int>(m_phrases.size())) {
    initText = m_phrases[phraseIndex].text;
    initCat = m_phrases[phraseIndex].category;
  } else {
    initText = L"";
    // 取当前选中节点的 category(若有)
    HTREEITEM cur = TreeView_GetSelection(s_hTree);
    if (cur) {
      TVITEMW ti = {};
      ti.mask = TVIF_PARAM;
      ti.hItem = cur;
      TreeView_GetItem(s_hTree, &ti);
      if (IS_CATEGORY(ti.lParam)) {
        wchar_t buf[256] = {};
        TVITEMW tx = {};
        tx.mask = TVIF_TEXT;
        tx.hItem = cur;
        tx.pszText = buf;
        tx.cchTextMax = 256;
        TreeView_GetItem(s_hTree, &tx);
        std::wstring lbl = buf;
        if (lbl.size() >= 2 && lbl[0] == 0x25BC && lbl[1] == L' ')
          lbl = lbl.substr(2);
        else if (lbl.size() >= 2 && lbl[0] == 0x25B6 && lbl[1] == L' ')
          lbl = lbl.substr(2);
        if (lbl != std::wstring(L"\x672a\x5206\x7c7b"))  // (未分类)
          initCat = lbl;
      }
    }
  }
  BeginInlineEdit(phraseIndex, initText, initCat);
}

void PhrasesDialog::ExitEditingState(bool save) {
  if (s_state != State_Editing) return;
  std::wstring text, cat;
  if (save && s_hEditText && IsWindow(s_hEditText)) {
    wchar_t buf[1024] = {};
    GetWindowTextW(s_hEditText, buf, 1024);
    text = buf;
    if (s_hEditCat && IsWindow(s_hEditCat)) {
      wchar_t cb[512] = {};
      GetWindowTextW(s_hEditCat, cb, 512);
      cat = cb;
    }
    if (text.empty()) {
      // 拒绝空 text - 弹出提示并保持 editing
      ShowToast(L"\u6587\u672c\u4e0d\u80fd\u4e3a\u7a7a");  // 文本不能为空
      if (s_hEditText) SetFocus(s_hEditText);
      return;
    }
    // 应用修改
    if (m_editingIndex < 0) {
      // 新增
      Phrase np;
      np.text = text;
      np.category = cat;
      m_phrases.push_back(np);
    } else if (m_editingIndex < static_cast<int>(m_phrases.size())) {
      m_phrases[m_editingIndex].text = text;
      m_phrases[m_editingIndex].category = cat;
    }
    PopulateTree(s_hTree);
    ScheduleSave();
  }
  if (s_hEditText && IsWindow(s_hEditText)) DestroyWindow(s_hEditText);
  if (s_hEditCat && IsWindow(s_hEditCat)) DestroyWindow(s_hEditCat);
  s_hEditText = s_hEditCat = nullptr;
  m_editingIndex = -1;
  s_state = State_Browsing;
  SetButtonsForBrowsing();
  if (s_hTree) SetFocus(s_hTree);
}

// ===== v0.19.0.27: Status bar =====

void PhrasesDialog::ShowStatusWarning(const std::wstring& text) {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  if (!s_hStatus || !IsWindow(s_hStatus)) {
    // 计算 status bar Y (搜索框下面)
    int y = kTitleH + kSearchH + kGap;
    s_hStatus = CreateWindowExW(0, L"STATIC", text.c_str(),
                                WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE |
                                    WS_CLIPSIBLINGS,
                                0, y, kDialogW, kStatusBarH, s_hwnd, nullptr,
                                GetModuleHandle(nullptr), nullptr);
    if (s_hStatus && s_hFontUi) {
      SendMessageW(s_hStatus, WM_SETFONT,
                   reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
    }
  } else {
    SetWindowTextW(s_hStatus, text.c_str());
    ShowWindow(s_hStatus, SW_SHOW);
  }
}

// ===== v0.19.0.27: Buttons swap (Browsing / Editing) =====

void PhrasesDialog::SetButtonsForBrowsing() {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  auto showBtn = [](HWND h, bool show) {
    if (h && IsWindow(h)) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
  };
  showBtn(s_hBtnAdd, true);
  showBtn(s_hBtnEdit, true);
  showBtn(s_hBtnDel, true);
  showBtn(s_hBtnCancel, true);
  showBtn(s_hBtnSave, false);
}

void PhrasesDialog::SetButtonsForEditing() {
  auto showBtn = [](HWND h, bool show) {
    if (h && IsWindow(h)) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
  };
  showBtn(s_hBtnAdd, false);
  showBtn(s_hBtnEdit, false);
  showBtn(s_hBtnDel, false);
  showBtn(s_hBtnCancel, true);
  showBtn(s_hBtnSave, true);
}

// ===== WndProc =====

LRESULT CALLBACK PhrasesDialog::WndProc(HWND hwnd, UINT msg, WPARAM wp,
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
    case WM_TIMER:
      return OnTimer(hwnd, wp);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
      return OnCtlColor(hwnd, wp, lp);
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
      // v0.19.0.27: editing 态时焦点进 tree 也保留 edit
      if (s_state == State_Editing) return 0;
      // v0.19.0.30-fix(issue 3b): 子控件夺焦点例外。
      // 触发链:Browsing 态 user 点 tree / search box / button → WM_KILLFOCUS(parent)
      // → grace 满后 Hide(),但 user 还在 dialog 内操作,不应关。
      // 修法:检查新焦点是否为本 dialog 内的子控件,是则 return 0 不 Hide。
      // 旧判定只走 State_Editing,browsing 态点 tree/search 也会误关。
      HWND newFocus = (HWND)wp;
      auto isChild = [newFocus](HWND h) {
        return h && newFocus == h;
      };
      if (isChild(s_hTree) || isChild(s_hSearch) || isChild(s_hStatus) ||
          isChild(s_hToast) || isChild(s_hBtnAdd) || isChild(s_hBtnEdit) ||
          isChild(s_hBtnDel) || isChild(s_hBtnCancel) ||
          isChild(s_hBtnSave) || isChild(s_hEditText) || isChild(s_hEditCat)) {
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
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // v0.19.0.27: layout formula
  // tree 在 search 下方,status bar 在 tree 上方(kStatusBarH 高,但不渲染除非 warn)
  // 实际公式:kTreeH_phys = kDialogH - kTitleH - kSearchH - kBtnH - 4*kGap
  kTreeH_phys = kDialogH - kTitleH - kSearchH - kBtnH - 4 * kGap;
  kBtnY_phys  = kTitleH + kSearchH + kTreeH_phys + 2 * kGap;
  if (kTreeH_phys < 80) kTreeH_phys = 80;
  if (kBtnY_phys + kBtnH > kDialogH) kBtnY_phys = kDialogH - kBtnH - 2;

  s_hFontUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS,
                          L"Segoe UI Variable");
  HFONT hfUi = s_hFontUi;

  // v0.19.0.27: 搜索框 (kTitleH + kGap, kSearchH 高)
  int searchY = kTitleH + kGap;
  int searchX = kSearchMarginX;
  int searchW = kDialogW - 2 * kSearchMarginX;
  s_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                                  ES_AUTOHSCROLL,
                              searchX, searchY, searchW, kSearchH, hwnd,
                              reinterpret_cast<HMENU>(ID_SEARCH),
                              GetModuleHandle(nullptr), nullptr);
  if (s_hSearch && hfUi) {
    SendMessageW(s_hSearch, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }
  // placeholder via cue banner (emulated)
  // Win32 EDIT 没 cue banner,用 static label 替代 (YAGNI,空实现)

  // Tree (SysTreeView32)
  int treeY = searchY + kSearchH + kGap;
  DWORD treeEx = WS_EX_CLIENTEDGE;
  DWORD treeStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                    TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
                    TVS_FULLROWSELECT;
  s_hTree = CreateWindowExW(treeEx, WC_TREEVIEWW, L"",
                            treeStyle, kGap, treeY,
                            kDialogW - 2 * kGap, kTreeH_phys,
                            hwnd, reinterpret_cast<HMENU>(ID_TREE),
                            GetModuleHandle(nullptr), nullptr);
  if (s_hTree && hfUi) {
    SendMessageW(s_hTree, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // 按钮行 (Browsing 态)
  int btnY = kBtnY_phys;
  int totalW = kBtnW * 4 + kBtnGap * 3;
  int btnX = (kDialogW - totalW) / 2;
  // Editing 态有 2 个按钮 (Save + Cancel),中心对齐
  const wchar_t* kBtnLabels[4] = {
      L"+ \x6dfb\x52a0",   // + 添加
      L"\u270e \x7f16\x8f91",  // ✎ 编辑
      L"- \x5220\x9664",   // - 删除
      L"\x53d6\x6d88",     // 取消
  };
  UINT kBtnIds[4] = {ID_BTN_ADD, ID_BTN_EDIT, ID_BTN_DEL, ID_BTN_CANCEL};
  HWND* kBtnHwnds[4] = {&s_hBtnAdd, &s_hBtnEdit, &s_hBtnDel, &s_hBtnCancel};
  for (int i = 0; i < 4; ++i) {
    *kBtnHwnds[i] = CreateWindowExW(
        0, L"BUTTON", kBtnLabels[i],
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
        btnX, btnY, kBtnW, kBtnH, hwnd,
        reinterpret_cast<HMENU>(kBtnIds[i]), GetModuleHandle(nullptr), nullptr);
    btnX += kBtnW + kBtnGap;
  }

  // Save 按钮 (Editing 态),2 按钮居中
  int saveTotalW = kBtnW * 2 + kBtnGap;
  int saveX = (kDialogW - saveTotalW) / 2;
  s_hBtnSave = CreateWindowExW(0, L"BUTTON", L"\x4fdd\x5b58",  // 保存
                               WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
                               saveX, btnY, kBtnW, kBtnH, hwnd,
                               reinterpret_cast<HMENU>(ID_BTN_SAVE),
                               GetModuleHandle(nullptr), nullptr);

  // button 字体
  if (hfUi) {
    SendMessageW(s_hBtnAdd,    WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnEdit,   WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnDel,    WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnSave,   WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hSearch,    WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // 初始: 隐藏 Save
  ShowWindow(s_hBtnSave, SW_HIDE);

  PopulateTree(s_hTree);

  // 默认全部展开
  HTREEITEM hRoot = TreeView_GetRoot(s_hTree);
  while (hRoot) {
    TreeView_Expand(s_hTree, hRoot, TVE_EXPAND);
    hRoot = TreeView_GetNextSibling(s_hTree, hRoot);
  }

  // 选中第一项
  HTREEITEM hFirst = TreeView_GetRoot(s_hTree);
  if (hFirst) {
    TreeView_SelectItem(s_hTree, hFirst);
    SetFocus(s_hTree);
  }

  return 0;
}

LRESULT PhrasesDialog::OnDestroy(HWND hwnd) {
  if (s_hFontUi) {
    DeleteObject(s_hFontUi);
    s_hFontUi = nullptr;
  }
  s_hwnd = nullptr;
  s_hTree = nullptr;
  s_hSearch = nullptr;
  s_hStatus = nullptr;
  s_hToast = nullptr;
  s_hBtnAdd = s_hBtnEdit = s_hBtnDel = s_hBtnCancel = s_hBtnSave = nullptr;
  s_hEditText = s_hEditCat = nullptr;
  return 0;
}

LRESULT PhrasesDialog::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  // v0.19.0.31: WS_POPUP+WS_EX_LAYERED 模式不自动填背景 → paint 全 client
  // 渐变 (原 v0.19.0.27-30 bug 只画 [0,kTitleH)=30px, body 透明 → 空 body)。
  ModalChrome::PaintBackgroundAndBorder(hdc, kDialogW, kDialogH,
                                         kBgTop, kBgBot,
                                         kDlgRadius, kBorderColor);

  // 标题文字 — 覆盖在 [0, kTitleH) 渐变之上
  HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HFONT hfOld = static_cast<HFONT>(SelectObject(hdc, hf));
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, kTextColor);
  RECT titleRc = {0, 0, kDialogW, kTitleH};
  DrawTextW(hdc, L"\x5e38\x7528\x77ed\x8bed", -1, &titleRc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  SelectObject(hdc, hfOld);

  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT PhrasesDialog::OnCtlColor(HWND hwnd, WPARAM wp, LPARAM lp) {
  HDC hdc = reinterpret_cast<HDC>(wp);
  HWND target = reinterpret_cast<HWND>(lp);
  if (target == s_hSearch) {
    SetBkColor(hdc, kSearchBg);
    SetTextColor(hdc, kTextColor);
    static HBRUSH s_hBrushSearch = nullptr;
    if (!s_hBrushSearch) s_hBrushSearch = CreateSolidBrush(kSearchBg);
    return reinterpret_cast<LRESULT>(s_hBrushSearch);
  }
  if (target == s_hStatus) {
    SetBkColor(hdc, kWarnBg);
    SetTextColor(hdc, kWarnText);
    static HBRUSH s_hBrushWarn = nullptr;
    if (!s_hBrushWarn) s_hBrushWarn = CreateSolidBrush(kWarnBg);
    return reinterpret_cast<LRESULT>(s_hBrushWarn);
  }
  if (target == s_hEditText || target == s_hEditCat) {
    SetBkColor(hdc, kEditBg);
    SetTextColor(hdc, kTextColor);
    static HBRUSH s_hBrushEdit = nullptr;
    if (!s_hBrushEdit) s_hBrushEdit = CreateSolidBrush(kEditBg);
    return reinterpret_cast<LRESULT>(s_hBrushEdit);
  }
  return DefWindowProcW(hwnd, WM_CTLCOLOREDIT, wp, lp);
}

LRESULT PhrasesDialog::OnKeyDown(HWND hwnd, WPARAM wp) {
  // v0.19.0.27: editing 态时,所有键透传给 edit 控件(WndProc 焦点不在 edit 上时)
  if (s_state == State_Editing) {
    if (wp == VK_ESCAPE) {
      ExitEditingState(false);
      return 0;
    }
    if (wp == VK_RETURN) {
      ExitEditingState(true);
      return 0;
    }
    return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
  }
  // Ctrl+F 进 search 态
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'F') {
    if (s_hSearch && IsWindow(s_hSearch)) {
      SetFocus(s_hSearch);
      s_state = State_Search;
    }
    return 0;
  }
  // Ctrl+N 新建
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'N') {
    EnterEditingState(-1, true);
    return 0;
  }
  // Ctrl+E 编辑
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'E') {
    if (m_selectedIndex >= 0 &&
        m_selectedIndex < static_cast<int>(m_phrases.size())) {
      EnterEditingState(m_selectedIndex, false);
    }
    return 0;
  }
  // Ctrl+S 立即保存
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'S') {
    FlushSave();
    return 0;
  }

  switch (wp) {
    case VK_UP:
      MoveSelection(s_hTree, -1);
      return 0;
    case VK_DOWN:
      MoveSelection(s_hTree, +1);
      return 0;
    case VK_LEFT:
      HandleLeftRight(s_hTree, false);
      return 0;
    case VK_RIGHT:
      HandleLeftRight(s_hTree, true);
      return 0;
    case VK_RETURN:
      HandleEnter(s_hTree);
      return 0;
    case VK_ESCAPE:
      if (s_state == State_Search) {
        SetWindowTextW(s_hSearch, L"");
        ApplySearchFilter(L"");
        s_state = State_Browsing;
        if (s_hTree) SetFocus(s_hTree);
      } else {
        Hide();
      }
      return 0;
    case VK_F2:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        EnterEditingState(m_selectedIndex, false);
      } else {
        // 分类行 F2 = no-op (rename category 不在 v1 范围)
        HTREEITEM cur = TreeView_GetSelection(s_hTree);
        if (cur) {
          TVITEMW ti = {};
          ti.mask = TVIF_PARAM;
          ti.hItem = cur;
          TreeView_GetItem(s_hTree, &ti);
          if (IS_CATEGORY(ti.lParam)) {
            ShowToast(L"\u5206\u7c7b\u91cd\u547d\u540d\u8bf7\u7528\u53f3\u952e\u83dc\u5355");  // 分类重命名请用右键菜单
          }
        }
      }
      return 0;
    case VK_DELETE: {
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        m_phrases.erase(m_phrases.begin() + m_selectedIndex);
        PopulateTree(s_hTree);
        ScheduleSave();
        m_selectedIndex = -1;
      }
      return 0;
    }
    case VK_TAB: {
      HWND order[4] = {s_hBtnAdd, s_hBtnEdit, s_hBtnDel, s_hBtnCancel};
      HWND cur = GetFocus();
      int start = 0;
      for (int i = 0; i < 4; ++i) {
        if (order[i] == cur) {
          start = (i + 1) % 4;
          break;
        }
      }
      SetFocus(order[start]);
      return 0;
    }
  }
  return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
}

LRESULT PhrasesDialog::OnNotify(HWND hwnd, LPARAM lp) {
  LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lp);
  if (!pnm) return 0;
  if (pnm->idFrom == ID_SEARCH) {
    // 搜索框 EN_CHANGE 触发过滤
    if (pnm->code == EN_CHANGE) {
      wchar_t buf[256] = {};
      GetWindowTextW(s_hSearch, buf, 256);
      ApplySearchFilter(buf);
    }
    return 0;
  }
  if (pnm->idFrom != ID_TREE) return 0;

  switch (pnm->code) {
    case NM_DBLCLK: {
      // 双击触发 inline edit (phrase 行)
      HTREEITEM cur = TreeView_GetSelection(s_hTree);
      if (cur) {
        TVITEMW ti = {};
        ti.mask = TVIF_PARAM;
        ti.hItem = cur;
        TreeView_GetItem(s_hTree, &ti);
        if (!IS_CATEGORY(ti.lParam) && ti.lParam >= 0) {
          EnterEditingState(static_cast<int>(ti.lParam), false);
        }
      }
      return 0;
    }
    case TVN_KEYDOWN: {
      LPNMTVKEYDOWN pnkd = reinterpret_cast<LPNMTVKEYDOWN>(lp);
      if (pnkd->wVKey == VK_RETURN) HandleEnter(s_hTree);
      return 0;
    }
    case TVN_SELCHANGED: {
      LPNMTREEVIEW pn = reinterpret_cast<LPNMTREEVIEW>(lp);
      m_selectedIndex = -1;
      if (pn->itemNew.lParam != ITEM_DATA_CATEGORY && pn->itemNew.lParam >= 0) {
        m_selectedIndex = static_cast<int>(pn->itemNew.lParam);
      }
      return 0;
    }
  }
  return 0;
}

LRESULT PhrasesDialog::OnCommand(HWND hwnd, WPARAM wp) {
  switch (LOWORD(wp)) {
    case ID_BTN_ADD:
      EnterEditingState(-1, true);
      return 0;
    case ID_BTN_EDIT:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        EnterEditingState(m_selectedIndex, false);
      }
      return 0;
    case ID_BTN_DEL:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        m_phrases.erase(m_phrases.begin() + m_selectedIndex);
        PopulateTree(s_hTree);
        ScheduleSave();
        m_selectedIndex = -1;
      }
      return 0;
    case ID_BTN_CANCEL:
      if (s_state == State_Editing) {
        ExitEditingState(false);
      } else {
        Hide();
      }
      return 0;
    case ID_BTN_SAVE:
      if (s_state == State_Editing) {
        ExitEditingState(true);
      }
      return 0;
  }
  return DefWindowProcW(hwnd, WM_COMMAND, wp, 0);
}

LRESULT PhrasesDialog::OnTimer(HWND hwnd, WPARAM wp) {
  switch (wp) {
    case IDT_SAVE:
      KillTimer(hwnd, IDT_SAVE);
      FlushSave();
      return 0;
    case IDT_TOAST:
      KillTimer(hwnd, IDT_TOAST);
      HideToast();
      return 0;
  }
  return DefWindowProcW(hwnd, WM_TIMER, wp, 0);
}

// ===== Tree populate =====

void PhrasesDialog::PopulateTree(HWND hTree) { PopulateTreeImpl(hTree); }

int PhrasesDialog::PopulateTreeCount(HWND hTree) {
  return PopulateTreeImpl(hTree);
}

int PhrasesDialog::PopulateTreeImpl(HWND hTree) {
  if (!hTree) return 0;
  // v0.19.0.27: 同步更新 s_hTree 让 ApplySearchFilter 能用
  s_hTree = hTree;
  TreeView_DeleteAllItems(hTree);

  std::set<std::wstring> cats;
  for (auto& p : m_phrases) cats.insert(p.category);
  bool hasUncat = cats.count(L"") > 0;
  cats.erase(L"");

  int inserted = 0;

  for (auto& cat : cats) {
    std::wstring label = L"\u25bc " + cat;
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = const_cast<wchar_t*>(label.c_str());
    tvis.item.lParam = ITEM_DATA_CATEGORY;
    HTREEITEM hCat = TreeView_InsertItem(hTree, &tvis);
    ++inserted;

    bool expanded = true;
    auto it = m_expanded.find(cat);
    if (it != m_expanded.end()) expanded = it->second;
    if (expanded) TreeView_Expand(hTree, hCat, TVE_EXPAND);

    for (size_t i = 0; i < m_phrases.size(); ++i) {
      if (m_phrases[i].category == cat) {
        TVINSERTSTRUCTW pi = {};
        pi.hParent = hCat;
        pi.hInsertAfter = TVI_LAST;
        pi.item.mask = TVIF_TEXT | TVIF_PARAM;
        std::wstring plabel = L"   " + m_phrases[i].text;
        pi.item.pszText = const_cast<wchar_t*>(plabel.c_str());
        pi.item.lParam = ITEM_DATA_PHRASE(static_cast<int>(i));
        TreeView_InsertItem(hTree, &pi);
        ++inserted;
      }
    }
  }

  if (hasUncat) {
    std::wstring label = L"\u25bc (\x672a\x5206\x7c7b)";
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = TVI_ROOT;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = const_cast<wchar_t*>(label.c_str());
    tvis.item.lParam = ITEM_DATA_CATEGORY;
    HTREEITEM hUncat = TreeView_InsertItem(hTree, &tvis);
    ++inserted;

    bool expanded = true;
    auto it = m_expanded.find(L"");
    if (it != m_expanded.end()) expanded = it->second;
    if (expanded) TreeView_Expand(hTree, hUncat, TVE_EXPAND);

    for (size_t i = 0; i < m_phrases.size(); ++i) {
      if (m_phrases[i].category.empty()) {
        TVINSERTSTRUCTW pi = {};
        pi.hParent = hUncat;
        pi.hInsertAfter = TVI_LAST;
        pi.item.mask = TVIF_TEXT | TVIF_PARAM;
        std::wstring plabel = L"   " + m_phrases[i].text;
        pi.item.pszText = const_cast<wchar_t*>(plabel.c_str());
        pi.item.lParam = ITEM_DATA_PHRASE(static_cast<int>(i));
        TreeView_InsertItem(hTree, &pi);
        ++inserted;
      }
    }
  }

  // v0.19.0.27: 重建后应用现有 search 过滤
  if (!s_searchQuery.empty()) {
    ApplySearchFilter(s_searchQuery);
  }

  return inserted;
}

// ===== 键盘导航 =====

std::vector<HTREEITEM> PhrasesDialog::CollectVisibleItems(HWND hTree) {
  std::vector<HTREEITEM> result;
  HTREEITEM h = TreeView_GetRoot(hTree);
  while (h) {
    result.push_back(h);
    if (TreeView_GetItemState(hTree, h, TVIS_EXPANDED) & TVIS_EXPANDED) {
      HTREEITEM c = TreeView_GetChild(hTree, h);
      while (c) {
        result.push_back(c);
        c = TreeView_GetNextSibling(hTree, c);
      }
    }
    h = TreeView_GetNextSibling(hTree, h);
  }
  return result;
}

HTREEITEM PhrasesDialog::MoveSelection(HWND hTree, int dir) {
  auto items = CollectVisibleItems(hTree);
  if (items.empty()) return nullptr;
  HTREEITEM cur = TreeView_GetSelection(hTree);
  int idx = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i] == cur) {
      idx = static_cast<int>(i);
      break;
    }
  }
  int newIdx = idx + dir;
  if (newIdx < 0) newIdx = 0;
  if (newIdx >= static_cast<int>(items.size()))
    newIdx = static_cast<int>(items.size()) - 1;
  TreeView_SelectItem(hTree, items[newIdx]);
  return items[newIdx];
}

void PhrasesDialog::HandleLeftRight(HWND hTree, bool right) {
  HTREEITEM cur = TreeView_GetSelection(hTree);
  if (!cur) return;
  TVITEMW ti = {};
  ti.mask = TVIF_PARAM | TVIF_STATE;
  ti.hItem = cur;
  TreeView_GetItem(hTree, &ti);

  if (IS_CATEGORY(ti.lParam)) {
    bool expanded = (ti.state & TVIS_EXPANDED) != 0;
    if (right && !expanded) {
      TreeView_Expand(hTree, cur, TVE_EXPAND);
      wchar_t buf[256] = {};
      TVITEMW tx = {};
      tx.mask = TVIF_TEXT;
      tx.hItem = cur;
      tx.pszText = buf;
      tx.cchTextMax = 256;
      TreeView_GetItem(hTree, &tx);
      std::wstring label = buf;
      if (label.size() >= 2 && label[0] == 0x25BC && label[1] == L' ')
        label = label.substr(2);
      m_expanded[label] = true;
    } else if (!right && expanded) {
      TreeView_Expand(hTree, cur, TVE_COLLAPSE);
      wchar_t buf[256] = {};
      TVITEMW tx = {};
      tx.mask = TVIF_TEXT;
      tx.hItem = cur;
      tx.pszText = buf;
      tx.cchTextMax = 256;
      TreeView_GetItem(hTree, &tx);
      std::wstring label = buf;
      if (label.size() >= 2 && label[0] == 0x25BC && label[1] == L' ')
        label = label.substr(2);
      m_expanded[label] = false;
    } else if (!right && !expanded) {
    } else if (right && expanded) {
      HTREEITEM c = TreeView_GetChild(hTree, cur);
      if (c) TreeView_SelectItem(hTree, c);
    }
  } else {
    if (!right) {
      HTREEITEM parent = TreeView_GetParent(hTree, cur);
      if (parent) TreeView_SelectItem(hTree, parent);
    }
  }
}

void PhrasesDialog::HandleEnter(HWND hTree) {
  HTREEITEM cur = TreeView_GetSelection(hTree);
  if (!cur) return;
  TVITEMW ti = {};
  ti.mask = TVIF_PARAM;
  ti.hItem = cur;
  TreeView_GetItem(hTree, &ti);

  if (IS_CATEGORY(ti.lParam)) {
    bool expanded = (TreeView_GetItemState(hTree, cur, TVIS_EXPANDED) &
                     TVIS_EXPANDED) != 0;
    TreeView_Expand(hTree, cur, expanded ? TVE_COLLAPSE : TVE_EXPAND);
    return;
  }

  int idx = static_cast<int>(ti.lParam);
  if (idx < 0 || idx >= static_cast<int>(m_phrases.size())) return;
  InjectText(m_phrases[idx].text);
  Hide();
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
