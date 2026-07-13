//
// PhrasesDialog.cpp — 常用短语 Modal 实现
// spec: .specify/specs/042-phrases-ui/spec.md
//
#include "PhrasesDialog.h"

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>

// ===== Static state 定义 =====
HWND PhrasesDialog::s_hwnd = nullptr;
HWND PhrasesDialog::s_hTree = nullptr;
HWND PhrasesDialog::s_hBtnAdd = nullptr;
HWND PhrasesDialog::s_hBtnEdit = nullptr;
HWND PhrasesDialog::s_hBtnDel = nullptr;
HWND PhrasesDialog::s_hBtnCancel = nullptr;
std::wstring PhrasesDialog::s_yamlPath;
PhrasesDialog::InjectFn PhrasesDialog::s_injectFn = &PhrasesDialog::DefaultInject;
std::unordered_map<std::wstring, bool> PhrasesDialog::m_expanded;
std::vector<PhrasesDialog::Phrase> PhrasesDialog::m_phrases;
int PhrasesDialog::m_selectedIndex = -1;
// v0.19.0.26-cleanup(issue 1+2-cleanup):grace 计时字段(移除 s_outsideMs / IDT_PHRASE_POLL)
DWORD PhrasesDialog::s_showTime = 0;
// v0.19.0.26-cleanup(issue 1-cleanup):HFONT 静态持有,OnDestroy 释放
HFONT PhrasesDialog::s_hFontUi = nullptr;
// v0.19.0.26-fix(issue 2a):Tree 高度 / 按钮 Y 自适应
int   PhrasesDialog::kTreeH_phys = 340;
int   PhrasesDialog::kBtnY_phys = 378;

// ===== 设计常量 =====
namespace {
constexpr int kDialogW = 360;
constexpr int kDialogH = 420;
// v0.19.0.26-fix(issue 2a):Tree 高度 / Button 起点改成自适应,常量保留作为 reference。
// 运行时 kTreeH_phys = kDialogH - kTitleH - kBtnH - 3*kGap,见 OnCreate。
constexpr int kTreeH = 340;        // reference(测试可达)
constexpr int kBtnH = 32;
constexpr int kBtnW = 76;
constexpr int kBtnGap = 8;
constexpr int kBtnMarginX = 12;
constexpr int kGap = 8;            // title→tree / tree→btn / btn→bottom 通用间距
constexpr int kTitleH = 30;

// v0.19.0.26-fix(issue 2c):圆角 radius(对应 FLUENT-UI-TOKENS.md radius.lg=14, 2x 用于
// GDI 椭圆 = 28)。同时画 hairline 边框(8% 黑 ≈ RGB(217,217,217))。
constexpr int kDlgRadius = 14;
constexpr COLORREF kBorderColor = RGB(217, 217, 217);

constexpr COLORREF kBgTop = RGB(245, 245, 248);
constexpr COLORREF kBgBot = RGB(220, 222, 230);
constexpr COLORREF kTextColor = RGB(30, 30, 40);
constexpr COLORREF kSelBg = RGB(255, 235, 220);  // 选中行浅橙底

// v0.19.0.26-fix(issue 2c):Segoe UI Variable 是 Windows 11 新 UI 字(mac 风格首选),
// 缺时 fallback 到系统默认 GUI font。size 14px ≈ font.ui.size.body (FLUENT-UI-TOKENS)。
// Test/mock 路径可能无 system font,设 fallback 链。
constexpr int kUiFontSize = 14;

constexpr UINT ID_BTN_ADD = 1001;
constexpr UINT ID_BTN_EDIT = 1002;
constexpr UINT ID_BTN_DEL = 1003;
constexpr UINT ID_BTN_CANCEL = 1004;
constexpr UINT ID_TREE = 1100;
constexpr UINT ID_EDIT_TEXT = 1201;
constexpr UINT ID_EDIT_CAT = 1202;
constexpr UINT ID_EDIT_OK = 1210;
constexpr UINT ID_EDIT_CANCEL = 1211;

// item data payload: 高 16 位是 category 标识(0xFFFF = 分类,其他 = index)
// 用低 31 位存 m_phrases index(分类行 item data = 0xFFFFFFFF)
constexpr LPARAM ITEM_DATA_PHRASE(int idx) {
  return static_cast<LPARAM>(idx) & 0x7FFFFFFF;
}
constexpr LPARAM ITEM_DATA_CATEGORY = static_cast<LPARAM>(0xFFFFFFFF);
constexpr bool IS_CATEGORY(LPARAM data) { return data == ITEM_DATA_CATEGORY; }
}  // namespace

// ===== Public API =====

void PhrasesDialog::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hTree);
    // v0.19.0.26-cleanup(issue 2-cleanup):reuse 路径重置 grace 计时。
    // 移植自 QuickPanelDialog::Show 1109-1114 段(同 L94 pattern)。
    s_showTime = GetTickCount();
    return;
  }

  // 1. 加载 YAML
  if (s_yamlPath.empty()) {
    // 默认 = RimeGetUserDataDir() + "phrases.yaml"
    wchar_t path[MAX_PATH] = {};
    // 用 SHGetFolderPathW 拿 APPDATA 即可;RIME 的 user_data 通常 = APPDATA\Rime
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
      std::wstring base = std::wstring(path) + L"\\Rime";
      s_yamlPath = base + L"\\phrases.yaml";
    } else {
      s_yamlPath = L"phrases.yaml";
    }
  }
  m_phrases.clear();
  if (!LoadPhrases(s_yamlPath, m_phrases)) {
    // 失败 fallback 空 list + log(防御性,spec §4)
    std::wcerr << L"[PhrasesDialog] WARN: cannot load " << s_yamlPath
               << L", start with empty list" << std::endl;
    m_phrases.clear();
  }

  // 2. 注册 TreeView 控件类(commctrl)
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TREEVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  // 3. 创建 modal 窗口
  // v0.19.0.26-fix(issue 2c):WS_CAPTION | WS_SYSMENU 移除 → 自绘 title bar(mac 风格)。
  // WS_CLIPCHILDREN:OnPaint 涂 client 时不覆盖子控件(防 button 被涂没)。
  // WS_CLIPSIBLINGS:子控件之间互不覆盖。
  // WS_EX_LAYERED:per-pixel alpha 圆角路径(panel 外 alpha=0,见 QuickPanel pattern)。
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
  // v0.19.0.26-fix(issue 2c):SetWindowRgn 圆角(mac 风格;同 QuickPanel 路径)。
  // radius.lg = 14 (FLUENT-UI-TOKENS.md §3.6 line 152)。
  // CreateRoundRectRgn 第 5/6 参数是椭圆宽高,2*r=28 让 GDI 画圆角。
  {
    HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
                                  kDlgRadius * 2, kDlgRadius * 2);
    if (rgn) {
      SetWindowRgn(s_hwnd, rgn, TRUE);
      // SetWindowRgn 接管 rgn 生命周期,不要 DeleteObject。
    }
  }
  ShowWindow(s_hwnd, SW_SHOW);
  UpdateWindow(s_hwnd);

  // v0.19.0.26-cleanup(issue 2-cleanup):grace 计时 — modal dialog 设计就
  // Esc/X/Cancel/Enter 关,不需要 polling timer。s_showTime 用于 WM_ACTIVATEAPP /
  // WM_KILLFOCUS grace guard(见 WndProc case WM_ACTIVATEAPP)。
  s_showTime = GetTickCount();
}

void PhrasesDialog::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hTree = nullptr;
  s_hBtnAdd = s_hBtnEdit = s_hBtnDel = s_hBtnCancel = nullptr;
  // s_showTime 在下次 Show 重新设(复用路径已设),无需在此重置。
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
  // 用 binary 模式读,自己处理 UTF-8 → wstring(spec §10.2 用 wifstream 但 Windows
  // 默认 ANSI locale 跟 wifstream 不兼容,简单方案:binary 读后 utf8→wchar)
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;

  // 读整个文件
  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());

  // 跳过 UTF-8 BOM(若有)
  if (content.size() >= 3 &&
      (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB &&
      (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }

  // UTF-8 → wstring(MultiByteToWideChar)
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
  // 按行处理
  size_t pos = 0;
  Phrase cur;
  bool inPhrase = false;
  while (pos <= wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos) eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    // 去掉行尾 \r
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    pos = eol + 1;

    auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == L'#') continue;

    if (trimmed.size() >= 2 && trimmed[0] == L'-' &&
        (trimmed[1] == L' ' || trimmed[1] == L'\t')) {
      // 新 phrase 起始
      if (inPhrase) out.push_back(cur);
      cur = Phrase();
      inPhrase = true;

      // 解析 `- text: <value>` (inline form)
      auto rest = trimmed.substr(2);
      auto trimmedRest = Trim(rest);
      if (trimmedRest.size() >= 5 && trimmedRest.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmedRest.substr(5));
      }
    } else if (inPhrase) {
      // 解析 `category: <value>` 或 `text:`(跨行)
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
  // 用 binary 模式写 UTF-8 + BOM,避免 wofstream 的 ANSI locale 问题
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

  appendLine(L"# phrases.yaml \u2014 Fluxing \u5e38\u7528\u77ed\u8bed (v0.19.0.25)");
  appendLine(L"# schema: phrases: [ { text, category } ]");
  appendLine(L"# category: \"\" represents uncategorized");
  appendLine(L"");
  appendLine(L"phrases:");
  for (const auto& p : data) {
    // 转义内部 " 为 ""
    std::wstring text = p.text;
    std::wstring cat = p.category;
    // 简单 YAML 字符串:双引号包裹,内部 " → ""
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

// ===== SendInput (spec §10.1) =====

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
    case WM_ACTIVATEAPP: {
      // v0.19.0.26-fix(issue 1):失焦关闭走 grace guard。
      // 移植自 QuickPanel L94 pattern (kShowGraceMs=2000, s_showTime 在 Show 末尾设)。
      // 修法:在 grace 期间 (now - s_showTime) < kShowGraceMs 不 Hide。
      // 根因(原 PhrasesDialog.cpp:352-358):无条件 Hide() → Show 启动瞬间
      // 切到 en-US / focus 切走时被系统 WM_ACTIVATEAPP 立即关掉,用户看到"短暂消失"。
      if (wp == FALSE) {
        DWORD nowTick = GetTickCount();
        if ((nowTick - s_showTime) >= kShowGraceMs) {
          Hide();
        }
      }
      return 0;
    }
    case WM_KILLFOCUS: {
      // v0.19.0.26-fix(issue 1):同 grace guard。原 cpp:356-358 完全 no-op,
      // 但有的版本 WM_ACTIVATEAPP 不到;补 KILLFOCUS 关闭路径走 grace。
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
  // Layered window: 设为 uniform alpha 255 (GDI 画, per-pixel 由 region 控制)
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // v0.19.0.26-fix(issue 2a):Tree 高度 / 按钮 Y 自适应,避免 btnY=378 撞 tree 底 370。
  // 公式:kTreeH_phys = kDialogH - kTitleH - kBtnH - 3*kGap,留 8px 底 padding。
  // 同理 kBtnY_phys = kTitleH + kTreeH_phys + kGap(tree 顶 = titleH, tree 底 = titleH+treeH)。
  kTreeH_phys = kDialogH - kTitleH - kBtnH - 3 * kGap;
  kBtnY_phys  = kTitleH + kTreeH_phys + kGap;
  // 防御:负值保护(测试或 resize 异常时)
  if (kTreeH_phys < 80) kTreeH_phys = 80;
  if (kBtnY_phys + kBtnH > kDialogH) kBtnY_phys = kDialogH - kBtnH - 2;

  // v0.19.0.26-cleanup(issue 3-cleanup):Segoe UI Variable 字体(mac 风格; FLUENT-UI-TOKENS.md
  // §3.4 font.ui.size.body = 14px)。Win32 font mapper 找不到时会自动 substitute 退到
  // Segoe UI / 系统默认,所以单次 CreateFontW 足够,不再写假象 fallback 链。HFONT 持有到
  // 静态成员 s_hFontUi,OnDestroy 释放(原 OnCreate 局部变量漏 DeleteObject 泄漏 GDI handle)。
  s_hFontUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS,
                          L"Segoe UI Variable");
  HFONT hfUi = s_hFontUi;

  // Tree (SysTreeView32)
  // v0.19.0.26-fix(issue 2b):子控件加 WS_CLIPSIBLINGS 避免 sibling 互相覆盖。
  // WS_EX_CLIENTEDGE 给 tree 1px 内边,跟 native 视觉一致(老版已经这样)。
  DWORD treeEx = WS_EX_CLIENTEDGE;
  DWORD treeStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                    TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
                    TVS_FULLROWSELECT;
  s_hTree = CreateWindowExW(treeEx, WC_TREEVIEWW, L"",
                            treeStyle, kGap, kTitleH + kGap,
                            kDialogW - 2 * kGap, kTreeH_phys,
                            hwnd, reinterpret_cast<HMENU>(ID_TREE),
                            GetModuleHandle(nullptr), nullptr);
  if (s_hTree && hfUi) {
    SendMessageW(s_hTree, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // 按钮行
  int btnY = kBtnY_phys;
  int totalW = kBtnW * 4 + kBtnGap * 3;
  int btnX = (kDialogW - totalW) / 2;

  // v0.19.0.26-fix(issue 2b):button 加 WS_CLIPSIBLINGS 避免 gradient 涂没。
  s_hBtnAdd = CreateWindowExW(0, L"BUTTON", L"+ \x6dfb\x52a0",  // + 添加
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                  BS_PUSHBUTTON,
                              btnX, btnY, kBtnW, kBtnH, hwnd,
                              reinterpret_cast<HMENU>(ID_BTN_ADD),
                              GetModuleHandle(nullptr), nullptr);
  btnX += kBtnW + kBtnGap;
  s_hBtnEdit = CreateWindowExW(0, L"BUTTON", L"\u270e \x7f16\x8f91",  // ✎ 编辑
                               WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                   BS_PUSHBUTTON,
                               btnX, btnY, kBtnW, kBtnH, hwnd,
                               reinterpret_cast<HMENU>(ID_BTN_EDIT),
                               GetModuleHandle(nullptr), nullptr);
  btnX += kBtnW + kBtnGap;
  s_hBtnDel = CreateWindowExW(0, L"BUTTON", L"- \x5220\x9664",  // - 删除
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                  BS_PUSHBUTTON,
                              btnX, btnY, kBtnW, kBtnH, hwnd,
                              reinterpret_cast<HMENU>(ID_BTN_DEL),
                              GetModuleHandle(nullptr), nullptr);
  btnX += kBtnW + kBtnGap;
  s_hBtnCancel = CreateWindowExW(0, L"BUTTON", L"\x53d6\x6d88",  // 取消
                                 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                                     BS_PUSHBUTTON,
                                 btnX, btnY, kBtnW, kBtnH, hwnd,
                                 reinterpret_cast<HMENU>(ID_BTN_CANCEL),
                                 GetModuleHandle(nullptr), nullptr);

  // button 也用新字体
  if (hfUi) {
    SendMessageW(s_hBtnAdd,    WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnEdit,   WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnDel,    WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    SendMessageW(s_hBtnCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  PopulateTree(s_hTree);

  // 默认全部展开(用户首次用,UX 友好)
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
  // v0.19.0.26-cleanup(issue 1-cleanup):释放 HFONT,否则每次 Show 泄漏一个 GDI handle。
  if (s_hFontUi) {
    DeleteObject(s_hFontUi);
    s_hFontUi = nullptr;
  }
  s_hwnd = nullptr;
  s_hTree = nullptr;
  s_hBtnAdd = s_hBtnEdit = s_hBtnDel = s_hBtnCancel = nullptr;
  return 0;
}

LRESULT PhrasesDialog::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  // v0.19.0.26-fix(issue 2b):OnPaint 不再 GradientFill 整个 client(会把 button 涂没)。
  // 改为:1) 画 title bar bg [0, kTitleH) 渐变(自绘 title 用),2) 画 hairline 边框
  // 沿 rgn 圆角矩形,3) 画 title text。
  // Tree [kTitleH, btnY) 和 button [btnY, kDialogH) 区域**不画** — 子控件自己画,
  // 配合 WS_CLIPCHILDREN Windows 自动 clip,button 跟 tree 永远可见。
  //
  // 1) Title bar bg(渐变 kBgTop → 中色,只在 [0, kTitleH) 范围)
  {
    RECT titleBg = {0, 0, kDialogW, kTitleH};
    TRIVERTEX v[2] = {};
    v[0].x = titleBg.left;
    v[0].y = titleBg.top;
    v[0].Red = static_cast<COLOR16>(GetRValue(kBgTop)) << 8;
    v[0].Green = static_cast<COLOR16>(GetGValue(kBgTop)) << 8;
    v[0].Blue = static_cast<COLOR16>(GetBValue(kBgTop)) << 8;
    v[0].Alpha = 0xFF00;
    v[1].x = titleBg.right;
    v[1].y = titleBg.bottom;
    v[1].Red = static_cast<COLOR16>(GetRValue(kBgBot)) << 8;
    v[1].Green = static_cast<COLOR16>(GetGValue(kBgBot)) << 8;
    v[1].Blue = static_cast<COLOR16>(GetBValue(kBgBot)) << 8;
    v[1].Alpha = 0xFF00;
    GRADIENT_RECT g = {0, 1};
    if (!GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V)) {
      HBRUSH bg = CreateSolidBrush(kBgTop);
      FillRect(hdc, &titleBg, bg);
      DeleteObject(bg);
    }
  }

  // 2) Hairline 边框(mac 风格; FLUENT-UI-TOKENS.md §3.6 line 158 8% 黑 = RGB(217,217,217))。
  // 注意:画在 client 坐标,inset 1px 让边框不被 SetWindowRgn clip 切到。
  {
    HPEN hPen = CreatePen(PS_SOLID, 1, kBorderColor);
    HPEN hOld = static_cast<HPEN>(SelectObject(hdc, hPen));
    HBRUSH hOldBr = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    RoundRect(hdc, 0, 0, kDialogW - 1, kDialogH - 1,
              kDlgRadius * 2, kDlgRadius * 2);
    SelectObject(hdc, hOld);
    SelectObject(hdc, hOldBr);
    DeleteObject(hPen);
  }

  // 3) 标题文字(自绘 title bar,跟 mac 风格一致)
  HFONT hf = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HFONT hfOld = static_cast<HFONT>(SelectObject(hdc, hf));
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, kTextColor);
  RECT titleRc = {0, 0, kDialogW, kTitleH};
  DrawTextW(hdc, L"\x5e38\x7528\x77ed\x8bed", -1, &titleRc,  // 常用短语
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  SelectObject(hdc, hfOld);

  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT PhrasesDialog::OnKeyDown(HWND hwnd, WPARAM wp) {
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
      Hide();
      return 0;
    case VK_TAB: {
      // 在按钮间循环
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
  if (pnm->idFrom != ID_TREE) return 0;

  switch (pnm->code) {
    case NM_DBLCLK:
    case TVN_KEYDOWN: {
      // 双击 phrase 注入;Enter 走 OnKeyDown
      if (pnm->code == TVN_KEYDOWN) {
        LPNMTVKEYDOWN pnkd = reinterpret_cast<LPNMTVKEYDOWN>(lp);
        if (pnkd->wVKey == VK_RETURN) HandleEnter(s_hTree);
      } else {
        HandleEnter(s_hTree);
      }
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
      ShowEditDialog(hwnd, -1);
      return 0;
    case ID_BTN_EDIT:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        ShowEditDialog(hwnd, m_selectedIndex);
      }
      return 0;
    case ID_BTN_DEL:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        m_phrases.erase(m_phrases.begin() + m_selectedIndex);
        if (!s_yamlPath.empty()) SavePhrases(s_yamlPath, m_phrases);
        PopulateTree(s_hTree);
      }
      return 0;
    case ID_BTN_CANCEL:
      Hide();
      return 0;
  }
  return DefWindowProcW(hwnd, WM_COMMAND, wp, 0);
}

// ===== Tree populate (spec §10.3) =====

void PhrasesDialog::PopulateTree(HWND hTree) {
  PopulateTreeImpl(hTree);
}

int PhrasesDialog::PopulateTreeCount(HWND hTree) {
  return PopulateTreeImpl(hTree);
}

int PhrasesDialog::PopulateTreeImpl(HWND hTree) {
  if (!hTree) return 0;
  TreeView_DeleteAllItems(hTree);

  // 收集所有 category (sorted)
  std::set<std::wstring> cats;
  for (auto& p : m_phrases) cats.insert(p.category);
  bool hasUncat = cats.count(L"") > 0;
  cats.erase(L"");

  int inserted = 0;

  // 渲染具名分类(按字母序)
  for (auto& cat : cats) {
    std::wstring label = L"\u25bc " + cat;  // ▼ 三角下箭头
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

  // 未分类(放最后)
  if (hasUncat) {
    std::wstring label = L"\u25bc (\x672a\x5206\x7c7b)";  // ▼ (未分类)
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

  return inserted;
}

// ===== 键盘导航 (spec §8) =====

std::vector<HTREEITEM> PhrasesDialog::CollectVisibleItems(HWND hTree) {
  std::vector<HTREEITEM> result;
  HTREEITEM h = TreeView_GetRoot(hTree);
  while (h) {
    result.push_back(h);
    // 如果展开了,把 child 也加入
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
    // 分类行
    bool expanded = (ti.state & TVIS_EXPANDED) != 0;
    if (right && !expanded) {
      TreeView_Expand(hTree, cur, TVE_EXPAND);
      // 把当前分类记录到 m_expanded
      // 从 parent 获取 category 字符串 - 简化:用 item text
      wchar_t buf[256] = {};
      TVITEMW tx = {};
      tx.mask = TVIF_TEXT;
      tx.hItem = cur;
      tx.pszText = buf;
      tx.cchTextMax = 256;
      TreeView_GetItem(hTree, &tx);
      std::wstring label = buf;
      // 去掉 "▼ " 前缀
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
      // 折叠的分类 ← no-op(spec §8)
    } else if (right && expanded) {
      // 已展开 → 跳到第一个 child
      HTREEITEM c = TreeView_GetChild(hTree, cur);
      if (c) TreeView_SelectItem(hTree, c);
    }
  } else {
    // phrase 行:← 跳到 parent(分类行)
    if (!right) {
      HTREEITEM parent = TreeView_GetParent(hTree, cur);
      if (parent) TreeView_SelectItem(hTree, parent);
    }
    // → on phrase = no-op
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
    // 分类行 Enter = toggle 折叠
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

// ===== 子 dialog (Add / Edit) =====

void PhrasesDialog::ShowEditDialog(HWND parent, int editIndex) {
  // 简化为 CreateWindow 模态小 dialog(避免引入 DialogBox + .rc)
  static bool s_editClassReg = false;
  if (!s_editClassReg) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &PhrasesDialog::EditDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"FluxingPhrasesEditDlg";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      return;
    }
    s_editClassReg = true;
  }

  HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FluxingPhrasesEditDlg",
                              editIndex < 0 ? L"\x6dfb\x52a0\x77ed\x8bed"
                                            : L"\x7f16\x8f91\x77ed\x8bed",
                              WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 320, 140,
                              parent, nullptr, GetModuleHandle(nullptr),
                              reinterpret_cast<LPVOID>(static_cast<intptr_t>(editIndex)));
  if (!hDlg) return;

  EnableWindow(parent, FALSE);

  // 模态消息循环
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (msg.message == WM_QUIT) {
      PostQuitMessage(static_cast<int>(msg.wParam));
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
    if (!IsWindow(hDlg)) break;
  }

  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);
}

LRESULT CALLBACK PhrasesDialog::EditDlgProc(HWND hwnd, UINT msg, WPARAM wp,
                                            LPARAM lp) {
  static int s_editIndex = -1;
  static HWND s_hText = nullptr;
  static HWND s_hCat = nullptr;

  switch (msg) {
    case WM_CREATE: {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
      s_editIndex = static_cast<int>(reinterpret_cast<intptr_t>(cs->lpCreateParams));
      s_hText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                12, 12, 280, 22, hwnd,
                                reinterpret_cast<HMENU>(ID_EDIT_TEXT),
                                GetModuleHandle(nullptr), nullptr);
      s_hCat = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                               12, 40, 280, 22, hwnd,
                               reinterpret_cast<HMENU>(ID_EDIT_CAT),
                               GetModuleHandle(nullptr), nullptr);
      CreateWindowExW(0, L"STATIC", L"text:", WS_CHILD | WS_VISIBLE,
                     0, 0, 0, 0, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
      CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                     130, 76, 70, 24, hwnd,
                     reinterpret_cast<HMENU>(ID_EDIT_OK),
                     GetModuleHandle(nullptr), nullptr);
      CreateWindowExW(0, L"BUTTON", L"Cancel",
                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                     210, 76, 70, 24, hwnd,
                     reinterpret_cast<HMENU>(ID_EDIT_CANCEL),
                     GetModuleHandle(nullptr), nullptr);

      if (s_editIndex >= 0 &&
          s_editIndex < static_cast<int>(m_phrases.size())) {
        SetWindowTextW(s_hText, m_phrases[s_editIndex].text.c_str());
        SetWindowTextW(s_hCat, m_phrases[s_editIndex].category.c_str());
      }
      SetFocus(s_hText);
      return 0;
    }
    case WM_COMMAND: {
      switch (LOWORD(wp)) {
        case ID_EDIT_OK: {
          wchar_t buf[512] = {};
          GetWindowTextW(s_hText, buf, 512);
          std::wstring text = buf;
          GetWindowTextW(s_hCat, buf, 512);
          std::wstring cat = buf;
          if (text.empty()) {
            // 禁止空 text
            MessageBoxW(hwnd, L"text \x4e0d\x80fd\x4e3a\x7a7a", L"Fluxing",
                        MB_OK | MB_ICONWARNING);
            return 0;
          }
          if (s_editIndex < 0) {
            Phrase np;
            np.text = text;
            np.category = cat;
            m_phrases.push_back(np);
          } else if (s_editIndex < static_cast<int>(m_phrases.size())) {
            m_phrases[s_editIndex].text = text;
            m_phrases[s_editIndex].category = cat;
          }
          if (!s_yamlPath.empty()) SavePhrases(s_yamlPath, m_phrases);
          PopulateTree(s_hTree);
          DestroyWindow(hwnd);
          return 0;
        }
        case ID_EDIT_CANCEL:
          DestroyWindow(hwnd);
          return 0;
      }
      return 0;
    }
    case WM_KEYDOWN:
      if (wp == VK_ESCAPE) DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
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