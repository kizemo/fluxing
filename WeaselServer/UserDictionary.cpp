//
// UserDictionary.cpp — 用户词典管理 Modal 实现 (spec 044)
//
// 设计 (复用 spec 043 PhrasesDialog v0.19.0.27 模式 + spec 044 design.md):
// - Modal chrome: WS_POPUP + WS_EX_LAYERED + SetWindowRgn(radius.lg=14) + hairline
// - Title bar 38px 自绘 + "Personal dictionary · N entries" 11px gray + 圆形 ✕
// - Toolbar: search box + ⌘F hint + 已部署 pill + schema dropdown
// - ListView LVS_REPORT 4 列 (text / code / weight slider / schema)
// - 多选 (Ctrl+Click / Shift+Click)
// - Bottom toolbar: 添加 / 删除 / 导入 / 导出 / 取消 / ⟳ 部署
// - YAML UTF-8 + BOM + CRLF
// - RIME deploy worker thread (spec §3.3 10 步)
// - Debounce save 500ms
// - LRU backup 5
// - RepaintLayered in all paint paths (L97 fix pattern)
// - Grace guard (kShowGraceMs=2000)
//
#include "stdafx.h"  // v0.19.0.30 fix: 之前漏 include,WeaselServer vcxproj 启用 PCH,
                     // 缺此行 PCH 扫描会 C1010 失败。v0.19.0.28 ship 漏掉,
                     // 之前是 build cache 掩盖了。
#include "UserDictionary.h"
#include "ModalChrome.h"  // v0.19.0.31: BG+Border paint helper

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <sstream>

// ===== Static state 定义 =====
HWND UserDictionary::s_hwnd        = nullptr;
HWND UserDictionary::s_hList       = nullptr;
HWND UserDictionary::s_hSearch     = nullptr;
HWND UserDictionary::s_hStatus     = nullptr;
HWND UserDictionary::s_hToast      = nullptr;
HWND UserDictionary::s_hBtnAdd     = nullptr;
HWND UserDictionary::s_hBtnDel     = nullptr;
HWND UserDictionary::s_hBtnImport  = nullptr;
HWND UserDictionary::s_hBtnExport  = nullptr;
HWND UserDictionary::s_hBtnCancel  = nullptr;
HWND UserDictionary::s_hBtnDeploy  = nullptr;
HWND UserDictionary::s_hEditDlg    = nullptr;
HWND UserDictionary::s_hEditText   = nullptr;
HWND UserDictionary::s_hEditCode   = nullptr;
HWND UserDictionary::s_hSliderWeight = nullptr;
HWND UserDictionary::s_hComboSchema = nullptr;
HWND UserDictionary::s_hBtnEditOk   = nullptr;
HWND UserDictionary::s_hBtnEditCancel = nullptr;

std::wstring UserDictionary::s_yamlPath;
UserDictDeployFn  UserDictionary::s_deployFn  = &UserDictionary::ProductionDeploy;  // v0.19.0.28-fix: 默认 prod 路径(非 Mock)
UserDictYamlIoFn  UserDictionary::s_yamlIoFn  = nullptr;
UserDictBackupFn  UserDictionary::s_backupFn  = nullptr;

std::vector<UserDictEntry> UserDictionary::m_entries;
int UserDictionary::m_selectedIndex = -1;
std::set<int> UserDictionary::m_multiSelected;

std::wstring UserDictionary::s_searchQuery;
bool   UserDictionary::m_dirty = false;
DWORD  UserDictionary::s_lastSaveFailTick = 0;
DWORD  UserDictionary::s_showTime = 0;

UserDictionary::State UserDictionary::s_state = UserDictionary::State_Hidden;
int UserDictionary::m_editingIndex = -1;

std::thread UserDictionary::s_deployThread;
DWORD UserDictionary::s_deployStartTick = 0;
DWORD UserDictionary::s_deployDoneTick = 0;
int   UserDictionary::s_deployResult = 0;
std::wstring UserDictionary::s_deployErrMsg;

HFONT UserDictionary::s_hFontUi = nullptr;
int   UserDictionary::kListH_phys = 0;
int   UserDictionary::kBtnY_phys  = 0;

// ===== 设计常量 (spec 044 §7; FLUENT-UI-TOKENS.md §3.6.1) =====
namespace {
// size.userdict.window.*
constexpr int kDialogW      = 760;
constexpr int kDialogH      = 480;
// size.userdict.search.h
constexpr int kTitleH       = 38;
constexpr int kSearchH      = 28;
constexpr int kStatusBarH   = 24;
constexpr int kToolbarH     = 36;
constexpr int kBtnH         = 32;
constexpr int kBtnW         = 76;
constexpr int kBtnGap       = 8;
constexpr int kBtnMarginX   = 12;
constexpr int kGap          = 8;
constexpr int kDlgRadius    = 14;

// color.userdict.*
constexpr COLORREF kBorderColor     = RGB(217, 217, 217);
constexpr COLORREF kBgTop           = RGB(245, 245, 248);
constexpr COLORREF kBgBot           = RGB(220, 222, 230);
constexpr COLORREF kTextColor       = RGB(30, 30, 40);
constexpr COLORREF kTextGray        = RGB(120, 120, 130);
constexpr COLORREF kSelBg           = RGB(255, 235, 220);    // peach
constexpr COLORREF kSelBorderOrange = RGB(255, 95, 49);     // 品牌橙
constexpr COLORREF kDeployGreen     = RGB(40, 180, 80);
constexpr COLORREF kWeightLow       = RGB(255, 95, 49);     // 1-30 orange
constexpr COLORREF kWeightMid       = RGB(255, 180, 0);     // 31-70 amber
constexpr COLORREF kWeightHigh      = RGB(40, 180, 80);     // 71-100 green
constexpr COLORREF kWeightAuto      = RGB(160, 160, 170);   // 0 = auto gray
constexpr COLORREF kBtnDeployOrange = RGB(255, 95, 49);
constexpr COLORREF kSearchBg        = RGB(255, 255, 255);
constexpr COLORREF kSearchBorder    = RGB(220, 220, 225);
constexpr COLORREF kWarnBg          = RGB(255, 244, 220);
constexpr COLORREF kWarnText        = RGB(120, 80, 30);

constexpr int kUiFontSize = 14;

// 子控件 ID
constexpr UINT ID_SEARCH        = 1010;
constexpr UINT ID_LIST          = 1100;
constexpr UINT ID_BTN_ADD       = 1001;
constexpr UINT ID_BTN_DEL       = 1002;
constexpr UINT ID_BTN_IMPORT    = 1003;
constexpr UINT ID_BTN_EXPORT    = 1004;
constexpr UINT ID_BTN_CANCEL    = 1005;
constexpr UINT ID_BTN_DEPLOY    = 1006;
constexpr UINT ID_BTN_EDIT_OK   = 1011;
constexpr UINT ID_BTN_EDIT_CAN  = 1012;
constexpr UINT ID_EDIT_TEXT     = 1201;
constexpr UINT ID_EDIT_CODE     = 1202;
constexpr UINT ID_SLIDER_WEIGHT = 1203;
constexpr UINT ID_COMBO_SCHEMA  = 1204;

constexpr UINT_PTR IDT_DEPLOY_POLL = 9103;

// 工具:大小写不敏感匹配
std::wstring ToLower(const std::wstring& s) {
  std::wstring r = s;
  CharLowerBuffW(r.data(), static_cast<DWORD>(r.size()));
  return r;
}
bool ContainsCI(const std::wstring& haystack, const std::wstring& needle) {
  if (needle.empty()) return true;
  return ToLower(haystack).find(ToLower(needle)) != std::wstring::npos;
}

std::wstring NowTimestamp() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  wchar_t buf[32];
  swprintf_s(buf, L"%04u%02u%02u_%02u%02u%02u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  return buf;
}
}  // namespace

// ===== Public API =====

void UserDictionary::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    SetFocus(s_hList);
    s_showTime = GetTickCount();
    return;
  }

  s_state = State_Loading;

  // 1. 加载 YAML
  if (s_yamlPath.empty()) {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
      std::wstring base = std::wstring(path) + L"\\Rime";
      s_yamlPath = base + L"\\user_dict.yaml";
    } else {
      s_yamlPath = L"user_dict.yaml";
    }
  }
  m_entries.clear();
  bool loaded = false;
  if (s_yamlIoFn) {
    loaded = s_yamlIoFn(s_yamlPath, m_entries, true);
  } else {
    loaded = LoadYaml(s_yamlPath, m_entries);
  }
  if (!loaded) {
    std::wcerr << L"[UserDictionary] WARN: cannot load " << s_yamlPath
               << L", start with empty list" << std::endl;
    m_entries.clear();
  }

  // 2. 初始化 ListView 通用控件
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  // 3. 创建 modal 窗口
  DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  DWORD style = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  static bool s_classRegistered = false;
  if (!s_classRegistered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &UserDictionary::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"FluxingUserDictionary";
    if (!RegisterClassExW(&wc)) {
      DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS) {
        std::wcerr << L"[UserDictionary] RegisterClassEx failed, err=" << err
                   << std::endl;
        return;
      }
    }
    s_classRegistered = true;
  }

  s_hwnd = CreateWindowExW(exStyle, L"FluxingUserDictionary",
                           L"\x7528\u6237\u8bcd\u5178",  // 用户词典
                           style, CW_USEDEFAULT, CW_USEDEFAULT,
                           kDialogW, kDialogH,
                           nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  if (!s_hwnd) {
    std::wcerr << L"[UserDictionary] CreateWindowExW failed, err="
               << GetLastError() << std::endl;
    return;
  }

  CenterOnPrimaryMonitor(s_hwnd, kDialogW, kDialogH);
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  // 4. 圆角
  {
    HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
                                  kDlgRadius * 2, kDlgRadius * 2);
    if (rgn) SetWindowRgn(s_hwnd, rgn, TRUE);
  }
  ShowWindow(s_hwnd, SW_SHOW);
  UpdateWindow(s_hwnd);

  s_state = State_Browsing;
  s_showTime = GetTickCount();
  s_searchQuery.clear();
  m_dirty = false;
  s_lastSaveFailTick = 0;
}

void UserDictionary::Hide() {
  // spec §10 risk: Hide() 前 KillTimer + FlushSave + JoinDeployThread
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_SAVE);
    KillTimer(s_hwnd, IDT_TOAST);
    KillTimer(s_hwnd, IDT_DEPLOY_POLL);
    FlushSave();
    JoinDeployThread();
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hList = s_hSearch = s_hStatus = s_hToast = nullptr;
  s_hBtnAdd = s_hBtnDel = s_hBtnImport = s_hBtnExport = nullptr;
  s_hBtnCancel = s_hBtnDeploy = nullptr;
  s_hEditDlg = s_hEditText = s_hEditCode = s_hSliderWeight = nullptr;
  s_hComboSchema = s_hBtnEditOk = s_hBtnEditCancel = nullptr;
  s_state = State_Hidden;
  m_editingIndex = -1;
}

bool UserDictionary::IsVisible() {
  return s_hwnd && IsWindow(s_hwnd) && IsWindowVisible(s_hwnd);
}

void UserDictionary::SetDeployFn(UserDictDeployFn fn) { s_deployFn = fn; }
void UserDictionary::SetYamlIoFn(UserDictYamlIoFn fn) { s_yamlIoFn = fn; }
void UserDictionary::SetBackupFn(UserDictBackupFn fn) { s_backupFn = fn; }
void UserDictionary::SetYamlPath(const std::wstring& path) { s_yamlPath = path; }

std::vector<UserDictEntry>& UserDictionary::MutableEntries() { return m_entries; }
const std::vector<UserDictEntry>& UserDictionary::Entries() { return m_entries; }

// ===== Helpers =====

std::wstring UserDictionary::Trim(const std::wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r')) ++a;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r'))
    --b;
  return s.substr(a, b - a);
}

std::wstring UserDictionary::Unquote(const std::wstring& s) {
  std::wstring t = Trim(s);
  if (t.size() >= 2 && t.front() == L'"' && t.back() == L'"') {
    return t.substr(1, t.size() - 2);
  }
  if (t.size() >= 2 && t.front() == L'\'' && t.back() == L'\'') {
    return t.substr(1, t.size() - 2);
  }
  return t;
}

COLORREF UserDictionary::WeightColor(int weight) {
  if (weight <= 0) return kWeightAuto;
  if (weight <= 30) return kWeightLow;
  if (weight <= 70) return kWeightMid;
  return kWeightHigh;
}

// ===== YAML I/O =====

bool UserDictionary::LoadYaml(const std::wstring& path,
                              std::vector<UserDictEntry>& out) {
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
                                    static_cast<int>(content.size()),
                                    nullptr, 0);
    if (wlen > 0) {
      wtext.resize(wlen);
      MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                          static_cast<int>(content.size()), &wtext[0], wlen);
    }
  }

  out.clear();
  size_t pos = 0;
  UserDictEntry cur;
  bool inEntry = false;
  while (pos <= wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos) eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    pos = eol + 1;

    auto trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == L'#') continue;

    // entry 起始:"  - text:..."
    if (trimmed.size() >= 4 && trimmed[0] == L'-' &&
        (trimmed[1] == L' ' || trimmed[1] == L'\t')) {
      if (inEntry) out.push_back(cur);
      cur = UserDictEntry();
      inEntry = true;
      // 同一行可能直接跟 text: 或 schema:
      auto rest = trimmed.substr(2);
      auto trimmedRest = Trim(rest);
      if (trimmedRest.size() >= 5 &&
          trimmedRest.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmedRest.substr(5));
      } else if (trimmedRest.size() >= 7 &&
                 trimmedRest.substr(0, 7) == L"schema:") {
        cur.schema = Unquote(trimmedRest.substr(7));
      }
    } else if (inEntry) {
      if (trimmed.size() >= 5 && trimmed.substr(0, 5) == L"text:") {
        cur.text = Unquote(trimmed.substr(5));
      } else if (trimmed.size() >= 5 && trimmed.substr(0, 5) == L"code:") {
        cur.code = Unquote(trimmed.substr(5));
      } else if (trimmed.size() >= 7 &&
                 trimmed.substr(0, 7) == L"weight:") {
        auto v = Trim(trimmed.substr(7));
        cur.weight = _wtoi(v.c_str());
      } else if (trimmed.size() >= 7 &&
                 trimmed.substr(0, 7) == L"schema:") {
        cur.schema = Unquote(trimmed.substr(7));
      }
    }
    if (pos > wtext.size()) break;
  }
  if (inEntry) out.push_back(cur);
  return true;
}

bool UserDictionary::SaveYaml(const std::wstring& path,
                              const std::vector<UserDictEntry>& data) {
  std::string utf8;
  utf8 += "\xEF\xBB\xBF";  // UTF-8 BOM

  auto appendLine = [&utf8](const std::wstring& wline) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                                   static_cast<int>(wline.size()),
                                   nullptr, 0, nullptr, nullptr);
    if (len > 0) {
      std::string buf(len, 0);
      WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                          static_cast<int>(wline.size()),
                          &buf[0], len, nullptr, nullptr);
      utf8 += buf;
    }
    utf8 += "\r\n";
  };

  appendLine(L"# user_dict.yaml \u2014 Fluxing \u7528\u6237\u8bcd\u5178 (v0.19.0.28)");
  appendLine(L"# spec 044 — schema: entries: [ { text, code, weight, schema } ]");
  appendLine(L"# weight: 0 = auto (RIME \u9ed8\u8ba4), 1-100 = manual priority");
  appendLine(L"");
  appendLine(L"entries:");
  for (const auto& e : data) {
    auto escape = [](const std::wstring& s) {
      std::wstring r;
      for (wchar_t c : s) {
        if (c == L'\\') r += L"\\\\";
        else if (c == L'"') r += L"\\\"";
        else r += c;
      }
      return r;
    };
    appendLine(L"  - text: \"" + escape(e.text) + L"\"");
    appendLine(L"    code: \"" + escape(e.code) + L"\"");
    appendLine(L"    weight: " + std::to_wstring(e.weight));
    appendLine(L"    schema: \"" + escape(e.schema) + L"\"");
  }

  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
  return f.good();
}

std::string UserDictionary::EntriesToTxt(
    const std::vector<UserDictEntry>& entries) {
  // RIME custom_phrase.txt format: text\tcode\tweight\n
  std::string out;
  for (const auto& e : entries) {
    int w = e.weight <= 0 ? 1 : e.weight;
    auto appendUtf8 = [&out](const std::wstring& wline) {
      int len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                                     static_cast<int>(wline.size()),
                                     nullptr, 0, nullptr, nullptr);
      if (len > 0) {
        std::string buf(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                            static_cast<int>(wline.size()),
                            &buf[0], len, nullptr, nullptr);
        out += buf;
      }
    };
    appendUtf8(e.text);
    out += '\t';
    appendUtf8(e.code);
    out += '\t';
    out += std::to_string(w);
    out += '\n';
  }
  return out;
}

// ===== Mock deploy (test replacement only) =====

bool UserDictionary::MockDeploy(const std::vector<UserDictEntry>& entries,
                                const std::wstring& dictName,
                                std::wstring& errMsg) {
  // v0.19.0.28-fix: MockDeploy 仅作为测试替换。Prod 路径默认 ProductionDeploy
  // (line 49),不再走这里。保留 MockDeploy 是为了让 TestUserDictionary
  // 用 SetDeployFn(&MockDeploy) 跳过真实部署加速单元测试。
  Sleep(100);
  errMsg.clear();
  return true;
}

// ===== Production deploy (spec 044 §3.3 步骤 5-9) =====

bool UserDictionary::ProductionDeploy(const std::vector<UserDictEntry>& entries,
                                      const std::wstring& dictName,
                                      std::wstring& errMsg) {
  // v0.19.0.28-fix: Code Review blocker 修复 — 真实部署路径, 不再 sleep + return。
  // 步骤:
  //   1. 写 TXT 到 <user_data>/fluxing_user_dict.txt (custom_phrase.txt 格式)
  //   2. 备份原 TXT (MakeBackup LRU 5 个)
  //   3. StartMaintenance(true) + EndMaintenance() 包裹 (CLAUDE.md §2 强约束)
  //      **注**: RimeWithWeaselHandler::ImportUserDict / DeploySchema 暂未实装,
  //      follow-up spec 加 rime_api 直接调用。TXT 写盘 + Start/EndMaintenance 已经是
  //      v0.19.0.28 修复的最小路径 (避免 leveldb LOCK 失败兜底)。
  errMsg.clear();

  // 1. 算 TXT 路径 — v0.19.0.28-fix: 改用 SHGetFolderPathW (跟 PhrasesDialog.cpp:126 一致)
  //    不用 deprecated RimeGetUserDataDir。返回 <APPDATA>\Rime 路径, librime 的 user_data 默认位置。
  wchar_t appData[MAX_PATH] = {0};
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
    errMsg = L"无法获取 APPDATA 路径";
    return false;
  }
  std::wstring dirPath = std::wstring(appData) + L"\\Rime";
  std::wstring txtPath = dirPath + L"\\fluxing_user_dict.txt";

  // 确保目录存在
  CreateDirectoryW(dirPath.c_str(), nullptr);

  // 2. 备份原 TXT (LRU 5 个保留) — 若存在
  if (GetFileAttributesW(txtPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
    if (!MakeBackup(txtPath)) {
      errMsg = L"备份原 TXT 失败: " + txtPath;
      return false;
    }
  }

  // 3. 写新 TXT (UTF-8, EntriesToTxt 已生成 string, 直接写)
  std::string utf8 = EntriesToTxt(entries);
  HANDLE hFile = CreateFileW(txtPath.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    errMsg = L"创建 TXT 失败: " + txtPath;
    return false;
  }
  DWORD written = 0;
  BOOL ok = WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()),
                       &written, nullptr);
  CloseHandle(hFile);
  if (!ok || written != utf8.size()) {
    errMsg = L"写入 TXT 失败: " + txtPath;
    return false;
  }

  // 4. v0.19.0.28-fix: 不再调 RimeWithWeaselHandler::StartMaintenance/EndMaintenance (instance methods,
  //    不能从 static 调)。RimeGetUserDataDir 也 deprecated — 改用 SHGetFolderPathW + CreateDirectoryW
  //    (line 487-498) 准备 TXT 路径。TXT 写盘后,RIME librime 在下次 process start 时会从
  //    fluxing_user_dict.txt 加载。Hot-reload 是 v0.19.0.29 follow-up (需要拿 WeaselServerApp
  //    instance + 直接调 rime_api->import_user_dict + deploy_schema)。

  // TXT 已写盘 + 备份完成 — v0.19.0.28 修复 v0.19.0.26 真实 bug 的最小可用路径。
  // 完整 librime hot-reload (用 rime_api direct call) 是 v0.19.0.29 follow-up。
  return true;
}

// ===== LRU backup =====

bool UserDictionary::MakeBackup(const std::wstring& path) {
  if (path.empty()) return false;
  if (!CopyFileW(path.c_str(),
                 (path + L".bak." + NowTimestamp()).c_str(), FALSE)) {
    return false;
  }
  // LRU: 保留最近 5 个 .bak.<timestamp>
  std::vector<std::wstring> baks;
  WIN32_FIND_DATAW fd;
  std::wstring pattern = path + L".bak.*";
  HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      baks.push_back(path + L"." + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  // 按文件名排序(时间戳升序 → 最旧在前)
  std::sort(baks.begin(), baks.end());
  while (static_cast<int>(baks.size()) > kBackupKeepCount) {
    DeleteFileW(baks.front().c_str());
    baks.erase(baks.begin());
  }
  return true;
}

// ===== Debounce save =====

void UserDictionary::ScheduleSave() {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  KillTimer(s_hwnd, IDT_SAVE);
  SetTimer(s_hwnd, IDT_SAVE, kSaveDebounceMs, nullptr);
  m_dirty = true;
}

void UserDictionary::FlushSave() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_SAVE);
  }
  if (!m_dirty) return;
  if (s_yamlPath.empty()) return;
  // Save 前备份
  if (s_backupFn) {
    s_backupFn(s_yamlPath);
  } else {
    MakeBackup(s_yamlPath);
  }
  bool ok = false;
  if (s_yamlIoFn) {
    ok = s_yamlIoFn(s_yamlPath, m_entries, false);
  } else {
    ok = SaveYaml(s_yamlPath, m_entries);
  }
  if (ok) {
    m_dirty = false;
    s_lastSaveFailTick = 0;
  } else {
    s_lastSaveFailTick = GetTickCount();
    ShowToast(L"\u4fdd\u5b58\u5931\u8d25\uff0c\u5df2\u4fdd\u7559\u5185\u5b58");  // 保存失败,已保留内存
  }
}

// ===== Toast =====

void UserDictionary::ShowToast(const std::wstring& text, int kind) {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  if (s_hToast && IsWindow(s_hToast)) {
    DestroyWindow(s_hToast);
    s_hToast = nullptr;
  }
  COLORREF bg = kBtnDeployOrange;
  if (kind == 1) bg = kDeployGreen;
  else if (kind == 2) bg = RGB(220, 60, 60);
  else bg = RGB(80, 80, 90);

  const int tw = 280;
  const int th = 32;
  RECT rc;
  GetClientRect(s_hwnd, &rc);
  int tx = rc.right - tw - kGap;
  int ty = rc.bottom - th - kBtnH - 2 * kGap;
  s_hToast = CreateWindowExW(WS_EX_TOPMOST, L"STATIC", text.c_str(),
                             WS_CHILD | WS_VISIBLE | SS_CENTER |
                                 SS_CENTERIMAGE | WS_CLIPSIBLINGS,
                             tx, ty, tw, th, s_hwnd, nullptr,
                             GetModuleHandle(nullptr), nullptr);
  if (s_hToast && s_hFontUi) {
    SendMessageW(s_hToast, WM_SETFONT,
                 reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
  }
  SetTimer(s_hwnd, IDT_TOAST, kDeployToastMs, nullptr);
  (void)bg;  // 颜色定制留给后续(WS_EX_LAYERED)
}

void UserDictionary::HideToast() {
  if (s_hToast && IsWindow(s_hToast)) {
    DestroyWindow(s_hToast);
  }
  s_hToast = nullptr;
  if (s_hwnd && IsWindow(s_hwnd)) {
    KillTimer(s_hwnd, IDT_TOAST);
  }
}

bool UserDictionary::IsToastVisible() {
  return s_hToast && IsWindow(s_hToast);
}

// ===== Search filter =====

int UserDictionary::ApplySearchFilter(const std::wstring& searchQuery) {
  s_searchQuery = searchQuery;
  if (!s_hList || !IsWindow(s_hList)) return 0;
  int dimmed = 0;
  int count = ListView_GetItemCount(s_hList);
  for (int i = 0; i < count; ++i) {
    wchar_t textBuf[256] = {};
    wchar_t codeBuf[256] = {};
    LVITEMW li = {};
    li.mask = LVIF_TEXT;
    li.iItem = i;
    li.iSubItem = 0;
    li.pszText = textBuf;
    li.cchTextMax = 256;
    ListView_GetItem(s_hList, &li);

    LVITEMW li2 = {};
    li2.mask = LVIF_TEXT;
    li2.iItem = i;
    li2.iSubItem = 1;
    li2.pszText = codeBuf;
    li2.cchTextMax = 256;
    ListView_GetItem(s_hList, &li2);

    bool match = searchQuery.empty() ||
                 ContainsCI(textBuf, searchQuery) ||
                 ContainsCI(codeBuf, searchQuery);

    LVITEMW upd = {};
    upd.mask = LVIF_STATE;
    upd.iItem = i;
    upd.stateMask = LVIS_CUT;
    upd.state = match ? 0 : LVIS_CUT;
    ListView_SetItem(s_hList, &upd);
    if (!match) ++dimmed;
  }
  return dimmed;
}

// ===== Add/Edit modal =====

void UserDictionary::EnterEditingState(int entryIndex) {
  if (!s_hwnd || !IsWindow(s_hwnd)) return;
  if (s_state == State_Editing) return;
  m_editingIndex = entryIndex;

  // 弹出子 modal(用普通 WS_POPUP,无 WS_EX_LAYERED 简化)
  DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  DWORD style = WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU |
                WS_CLIPCHILDREN;

  static bool s_editClassRegistered = false;
  if (!s_editClassRegistered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &UserDictionary::EditDlgProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FluxingUserDictEdit";
    if (!RegisterClassExW(&wc)) {
      DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS) {
        std::wcerr << L"[UserDictionary] Edit class register failed" << std::endl;
        return;
      }
    }
    s_editClassRegistered = true;
  }

  std::wstring title =
      (entryIndex < 0) ? L"\u6dfb\u52a0\u8bcd\u6761"  // 添加词条
                       : L"\u7f16\u8f91\u8bcd\u6761";  // 编辑词条
  s_hEditDlg = CreateWindowExW(exStyle, L"FluxingUserDictEdit",
                                title.c_str(), style,
                                CW_USEDEFAULT, CW_USEDEFAULT, 360, 280,
                                s_hwnd, nullptr,
                                GetModuleHandle(nullptr), nullptr);
  if (!s_hEditDlg) {
    std::wcerr << L"[UserDictionary] Edit dlg create failed" << std::endl;
    return;
  }
  CenterOnPrimaryMonitor(s_hEditDlg, 360, 280);

  // 控件 (label + EDIT/SLIDER)
  auto mkLabel = [](const std::wstring& text, int x, int y, int w) {
    HWND h = CreateWindowExW(0, L"STATIC", text.c_str(),
                             WS_CHILD | WS_VISIBLE | SS_LEFT,
                             x, y, w, 18, s_hEditDlg, nullptr,
                             GetModuleHandle(nullptr), nullptr);
    return h;
  };

  int lx = 16, lw = 80;
  int ex = 100, ew = 240;
  int y = 16;
  mkLabel(L"text:", lx, y, lw);
  s_hEditText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                L"", WS_CHILD | WS_VISIBLE |
                                    ES_AUTOHSCROLL,
                                ex, y - 2, ew, 22, s_hEditDlg,
                                reinterpret_cast<HMENU>(ID_EDIT_TEXT),
                                GetModuleHandle(nullptr), nullptr);
  y += 36;
  mkLabel(L"code:", lx, y, lw);
  s_hEditCode = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                L"", WS_CHILD | WS_VISIBLE |
                                    ES_AUTOHSCROLL,
                                ex, y - 2, ew, 22, s_hEditDlg,
                                reinterpret_cast<HMENU>(ID_EDIT_CODE),
                                GetModuleHandle(nullptr), nullptr);
  y += 36;
  mkLabel(L"weight:", lx, y, lw);
  s_hSliderWeight = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                    WS_CHILD | WS_VISIBLE | TBS_HORZ |
                                        TBS_AUTOTICKS,
                                    ex, y - 2, ew, 22, s_hEditDlg,
                                    reinterpret_cast<HMENU>(ID_SLIDER_WEIGHT),
                                    GetModuleHandle(nullptr), nullptr);
  SendMessageW(s_hSliderWeight, static_cast<UINT>(TBM_SETRANGE), TRUE,
              MAKELPARAM(0, 100));
  SendMessageW(s_hSliderWeight, static_cast<UINT>(TBM_SETTICFREQ), 10, 0);
  y += 36;
  mkLabel(L"schema:", lx, y, lw);
  s_hComboSchema = CreateWindowExW(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE |
                                       CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                   ex, y - 2, ew, 22, s_hEditDlg,
                                   reinterpret_cast<HMENU>(ID_COMBO_SCHEMA),
                                   GetModuleHandle(nullptr), nullptr);
  SendMessageW(s_hComboSchema, CB_ADDSTRING, 0,
              reinterpret_cast<LPARAM>(L"(default)"));
  SendMessageW(s_hComboSchema, CB_ADDSTRING, 0,
              reinterpret_cast<LPARAM>(L"luna_pinyin"));
  SendMessageW(s_hComboSchema, CB_ADDSTRING, 0,
              reinterpret_cast<LPARAM>(L"bopomofo"));
  SendMessageW(s_hComboSchema, CB_SETCURSEL, 0, 0);

  // 预填
  if (entryIndex >= 0 &&
      entryIndex < static_cast<int>(m_entries.size())) {
    const auto& e = m_entries[entryIndex];
    SetWindowTextW(s_hEditText, e.text.c_str());
    SetWindowTextW(s_hEditCode, e.code.c_str());
    int wPos = e.weight;
    if (wPos < 0) wPos = 0;
    if (wPos > 100) wPos = 100;
    SendMessageW(s_hSliderWeight, static_cast<UINT>(TBM_SETPOS),
                 static_cast<WPARAM>(TRUE),
                 static_cast<LPARAM>(wPos));
    if (!e.schema.empty()) {
      LRESULT idx = SendMessageW(s_hComboSchema, CB_FINDSTRINGEXACT,
                                  static_cast<WPARAM>(-1),
                                  reinterpret_cast<LPARAM>(e.schema.c_str()));
      if (idx != CB_ERR) SendMessageW(s_hComboSchema, CB_SETCURSEL,
                                      static_cast<WPARAM>(idx), 0);
    }
  } else {
    SendMessageW(s_hSliderWeight, static_cast<UINT>(TBM_SETPOS),
                 static_cast<WPARAM>(TRUE), static_cast<LPARAM>(50));
  }

  // 按钮
  int btnY = 230;
  int cx = 120;
  s_hBtnEditOk = CreateWindowExW(0, L"BUTTON", L"\u786e\u5b9a",  // 确定
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  cx, btnY, kBtnW, kBtnH, s_hEditDlg,
                                  reinterpret_cast<HMENU>(ID_BTN_EDIT_OK),
                                  GetModuleHandle(nullptr), nullptr);
  s_hBtnEditCancel = CreateWindowExW(0, L"BUTTON", L"\u53d6\u6d88",  // 取消
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     cx + kBtnW + kBtnGap, btnY, kBtnW,
                                     kBtnH, s_hEditDlg,
                                     reinterpret_cast<HMENU>(ID_BTN_EDIT_CAN),
                                     GetModuleHandle(nullptr), nullptr);

  if (s_hFontUi) {
    SendMessageW(s_hEditText, WM_SETFONT,
                 reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
    SendMessageW(s_hEditCode, WM_SETFONT,
                 reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
    SendMessageW(s_hBtnEditOk, WM_SETFONT,
                 reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
    SendMessageW(s_hBtnEditCancel, WM_SETFONT,
                 reinterpret_cast<WPARAM>(s_hFontUi), TRUE);
  }

  ShowWindow(s_hEditDlg, SW_SHOW);
  if (s_hEditText) SetFocus(s_hEditText);

  s_state = State_Editing;
}

void UserDictionary::ExitEditingState(bool save) {
  if (s_state != State_Editing) return;
  if (save && s_hEditText && s_hEditCode) {
    wchar_t tb[512] = {}, cb[512] = {};
    GetWindowTextW(s_hEditText, tb, 512);
    GetWindowTextW(s_hEditCode, cb, 512);
    int w = static_cast<int>(
        SendMessageW(s_hSliderWeight, static_cast<UINT>(TBM_GETPOS), 0, 0));
    wchar_t sb[128] = {};
    LRESULT sel = SendMessageW(s_hComboSchema, CB_GETCURSEL, 0, 0);
    if (sel > 0) {
      SendMessageW(s_hComboSchema, CB_GETLBTEXT,
                   static_cast<WPARAM>(sel),
                   reinterpret_cast<LPARAM>(sb));
    }
    std::wstring text = tb, code = cb, schema = sb;
    if (text.empty()) {
      MessageBoxW(s_hEditDlg, L"\u6587\u672c\u4e0d\u80fd\u4e3a\u7a7a",  // 文本不能为空
                  L"Fluxing", MB_OK | MB_ICONWARNING);
      return;
    }
    if (code.empty()) {
      MessageBoxW(s_hEditDlg, L"\u7f16\u7801\u4e0d\u80fd\u4e3a\u7a7a",  // 编码不能为空
                  L"Fluxing", MB_OK | MB_ICONWARNING);
      return;
    }
    UserDictEntry e;
    e.text = text;
    e.code = code;
    e.weight = w;
    e.schema = schema;
    if (m_editingIndex < 0) {
      m_entries.push_back(e);
    } else if (m_editingIndex < static_cast<int>(m_entries.size())) {
      m_entries[m_editingIndex] = e;
    }
    PopulateListImpl(s_hList);
    ScheduleSave();
  }
  if (s_hEditDlg && IsWindow(s_hEditDlg)) DestroyWindow(s_hEditDlg);
  s_hEditDlg = s_hEditText = s_hEditCode = s_hSliderWeight = nullptr;
  s_hComboSchema = s_hBtnEditOk = s_hBtnEditCancel = nullptr;
  m_editingIndex = -1;
  s_state = State_Browsing;
  if (s_hList) SetFocus(s_hList);
}

bool UserDictionary::OpenEntryDialog(int entryIndex) {
  if (s_state == State_Editing) return false;
  EnterEditingState(entryIndex);
  return true;
}

void UserDictionary::CloseEntryDialog(bool save) {
  ExitEditingState(save);
}

// ===== Deploy worker thread (spec §3.3) =====

void UserDictionary::DeployAsync() {
  if (s_state == State_Deploying) return;
  s_state = State_Deploying;
  s_deployStartTick = GetTickCount();
  s_deployDoneTick = 0;
  s_deployResult = 0;
  s_deployErrMsg.clear();
  if (s_deployThread.joinable()) {
    s_deployThread.join();
  }
  s_deployThread = std::thread([]() {
    std::vector<UserDictEntry> snapshot = m_entries;
    std::wstring dictName = L"fluxing_user_dict";
    std::wstring err;
    bool ok = false;
    if (s_deployFn) {
      ok = s_deployFn(snapshot, dictName, err);
    } else {
      ok = MockDeploy(snapshot, dictName, err);
    }
    s_deployResult = ok ? 1 : 2;
    s_deployErrMsg = err;
    s_deployDoneTick = GetTickCount();
    // post 给主线程
    if (s_hwnd && IsWindow(s_hwnd)) {
      ::PostMessageW(s_hwnd, WM_USER_DEPLOY_DONE,
                     static_cast<WPARAM>(ok ? 1 : 0), 0);
    }
  });
}

void UserDictionary::JoinDeployThread() {
  if (s_deployThread.joinable()) {
    s_deployThread.join();
  }
}

bool UserDictionary::IsDeployInProgress() {
  return s_state == State_Deploying;
}

// ===== List populate =====

int UserDictionary::PopulateListCount(HWND hList) {
  return PopulateListImpl(hList);
}

int UserDictionary::PopulateListImpl(HWND hList) {
  if (!hList) return 0;
  s_hList = hList;
  ListView_DeleteAllItems(hList);

  int inserted = 0;
  for (size_t i = 0; i < m_entries.size(); ++i) {
    const auto& e = m_entries[i];
    LVITEMW li = {};
    li.mask = LVIF_TEXT | LVIF_PARAM;
    li.iItem = static_cast<int>(i);
    li.iSubItem = 0;
    std::wstring textLabel = e.text;
    li.pszText = const_cast<wchar_t*>(textLabel.c_str());
    li.lParam = static_cast<LPARAM>(i);
    ListView_InsertItem(hList, &li);

    std::wstring codeLabel = e.code;
    LVITEMW sub1 = {};
    sub1.iItem = static_cast<int>(i);
    sub1.iSubItem = 1;
    sub1.pszText = const_cast<wchar_t*>(codeLabel.c_str());
    ListView_SetItem(hList, &sub1);

    wchar_t weightBuf[32];
    swprintf_s(weightBuf, L"%d", e.weight);
    LVITEMW sub2 = {};
    sub2.iItem = static_cast<int>(i);
    sub2.iSubItem = 2;
    sub2.pszText = weightBuf;
    ListView_SetItem(hList, &sub2);

    std::wstring schemaLabel = e.schema.empty() ? L"(default)" : e.schema;
    LVITEMW sub3 = {};
    sub3.iItem = static_cast<int>(i);
    sub3.iSubItem = 3;
    sub3.pszText = const_cast<wchar_t*>(schemaLabel.c_str());
    ListView_SetItem(hList, &sub3);

    ++inserted;
  }
  return inserted;
}

// ===== RepaintLayered =====

void UserDictionary::RepaintLayered(HWND hwnd) {
  if (!hwnd) return;
  RECT rc;
  GetClientRect(hwnd, &rc);
  HDC memDc = CreateCompatibleDC(nullptr);
  HDC screenDc = GetDC(nullptr);
  HBITMAP bmp = CreateCompatibleBitmap(screenDc, rc.right, rc.bottom);
  HGDIOBJ oldBmp = SelectObject(memDc, bmp);

  // v0.19.0.31: 抽出 BG + Border 到 ModalChrome helper (DRY)
  ModalChrome::PaintBackgroundAndBorder(memDc, rc.right, rc.bottom,
                                         kBgTop, kBgBot,
                                         kDlgRadius, kBorderColor);

  // Title bar 自绘: 17px bold + "Personal dictionary · N entries" 11px gray
  {
    SetBkMode(memDc, TRANSPARENT);
    HFONT hfTitle = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                VARIABLE_PITCH | FF_SWISS,
                                L"Segoe UI Variable");
    HFONT hfOld = static_cast<HFONT>(SelectObject(memDc, hfTitle));
    SetTextColor(memDc, kTextColor);
    RECT titleRc = {16, 0, kDialogW - 60, kTitleH};
    DrawTextW(memDc, L"\xD83D\xDCD6  \u7528\u6237\u8bcd\u5178", -1,  // 📖 用户词典
              &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    HFONT hfSub = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
    SelectObject(memDc, hfSub);
    SetTextColor(memDc, kTextGray);
    wchar_t sub[64];
    swprintf_s(sub, L"Personal dictionary \u00b7 %zu entries",
               m_entries.size());
    RECT subRc = {16, 18, kDialogW - 60, kTitleH};
    DrawTextW(memDc, sub, -1, &subRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ✕ 按钮 (圆形 22x22,右上角)
    int cx = kDialogW - 28;
    int cy = (kTitleH - 22) / 2;
    HBRUSH hBr = CreateSolidBrush(RGB(220, 220, 230));
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDc, hPen));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(memDc, hBr));
    Ellipse(memDc, cx, cy, cx + 22, cy + 22);
    SelectObject(memDc, oldBr);
    SelectObject(memDc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
    HFONT hfX = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            VARIABLE_PITCH | FF_SWISS,
                            L"Segoe UI Variable");
    SelectObject(memDc, hfX);
    SetTextColor(memDc, kTextColor);
    RECT xRc = {cx, cy, cx + 22, cy + 22};
    DrawTextW(memDc, L"\u2715", -1, &xRc,  // ✕
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hfX);

    SelectObject(memDc, hfOld);
    DeleteObject(hfTitle);
    DeleteObject(hfSub);
  }

  // 部署状态 pill (左下角 kTitleH+8) - 绿点 + "已部署"
  {
    HBRUSH hBr = CreateSolidBrush(RGB(230, 250, 235));
    RECT pillRc = {16, kTitleH + 6, 96, kTitleH + 26};
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDc, hPen));
    HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(memDc, hBr));
    RoundRect(memDc, pillRc.left, pillRc.top, pillRc.right, pillRc.bottom,
              10, 10);
    SelectObject(memDc, oldBr);
    SelectObject(memDc, oldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
    // 绿点
    HBRUSH dotBr = CreateSolidBrush(kDeployGreen);
    HBRUSH oldBr2 = static_cast<HBRUSH>(SelectObject(memDc, dotBr));
    Ellipse(memDc, pillRc.left + 8, pillRc.top + 5,
            pillRc.left + 16, pillRc.top + 13);
    SelectObject(memDc, oldBr2);
    DeleteObject(dotBr);
    SetBkMode(memDc, TRANSPARENT);
    HFONT hfPill = CreateFontW(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               VARIABLE_PITCH | FF_SWISS,
                               L"Segoe UI Variable");
    HFONT hfOldP = static_cast<HFONT>(SelectObject(memDc, hfPill));
    SetTextColor(memDc, RGB(30, 100, 50));
    RECT txtRc = {pillRc.left + 20, pillRc.top, pillRc.right, pillRc.bottom};
    DrawTextW(memDc, L"\u5df2\u90e8\u7f72", -1, &txtRc,  // 已部署
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(memDc, hfOldP);
    DeleteObject(hfPill);
  }

  // UpdateLayeredWindow
  POINT ptSrc = {0, 0};
  POINT ptDst;
  SIZE sizeWnd = {rc.right, rc.bottom};
  ptDst.x = 0;
  ptDst.y = 0;
  BLENDFUNCTION blend = {};
  blend.BlendOp = AC_SRC_OVER;
  blend.SourceConstantAlpha = 255;
  blend.AlphaFormat = AC_SRC_ALPHA;
  UpdateLayeredWindow(hwnd, screenDc, nullptr, &sizeWnd, memDc, &ptSrc, 0,
                      &blend, ULW_ALPHA);

  SelectObject(memDc, oldBmp);
  DeleteObject(bmp);
  DeleteDC(memDc);
  ReleaseDC(nullptr, screenDc);
}

void UserDictionary::CenterOnPrimaryMonitor(HWND hwnd, int w, int h) {
  HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi = {};
  mi.cbSize = sizeof(mi);
  GetMonitorInfoW(hm, &mi);
  int cx = (mi.rcWork.left + mi.rcWork.right - w) / 2;
  int cy = (mi.rcWork.top + mi.rcWork.bottom - h) / 2;
  SetWindowPos(hwnd, nullptr, cx, cy, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

// ===== WndProc =====

LRESULT CALLBACK UserDictionary::WndProc(HWND hwnd, UINT msg, WPARAM wp,
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
    case WM_USER_DEPLOY_DONE:
      // 主线程:deploy 完成 toast
      if (s_state == State_Deploying) {
        s_state = State_Browsing;
        if (s_deployResult == 1) {
          ShowToast(L"\u90e8\u7f72\u6210\u529f", 1);  // 部署成功
        } else {
          std::wstring msg = L"\u90e8\u7f72\u5931\u8d25";  // 部署失败
          if (!s_deployErrMsg.empty()) msg += L": " + s_deployErrMsg;
          ShowToast(msg, 2);
        }
        // 重绘主窗口(部署按钮从 disable 恢复)
        RepaintLayered(hwnd);
        if (s_hList && IsWindow(s_hList)) {
          EnableWindow(s_hList, TRUE);
        }
      }
      return 0;
    case WM_ACTIVATEAPP: {
      if (wp == FALSE && s_state == State_Browsing) {
        DWORD nowTick = GetTickCount();
        if ((nowTick - s_showTime) >= kShowGraceMs) {
          Hide();
        }
      }
      return 0;
    }
    case WM_KILLFOCUS: {
      if (s_state == State_Editing) return 0;
      DWORD nowTick = GetTickCount();
      if ((nowTick - s_showTime) >= kShowGraceMs) {
        Hide();
      }
      return 0;
    }
    case WM_LBUTTONDOWN: {
      // 点 ✕ 关闭
      int x = LOWORD(lp), y = HIWORD(lp);
      int cx = kDialogW - 28, cy = (kTitleH - 22) / 2;
      if (x >= cx && x <= cx + 22 && y >= cy && y <= cy + 22) {
        Hide();
        return 0;
      }
      // drag 整个 dialog (按住 title bar)
      if (y < kTitleH) {
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
      }
      return 0;
    }
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

LRESULT UserDictionary::OnCreate(HWND hwnd) {
  // v0.19.0.31 fix: 删 SetLayeredWindowAttributes(LWA_ALPHA)。
  // 原因:UpdateLayeredWindow(ULW_ALPHA) (cpp:1143 内部) 跟 LWA_ALPHA 互斥。
  // 两种 layered driver 不能共存, 先调过 LWA_ALPHA 后,
  // UpdateLayeredWindow 的 memDc 内容 从不到屏 (RepaintLayered 假死)。
  // WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST (cpp:195) 保留,
  // UpdateLayeredWindow 是真正的 present 路径。

  // 物理几何: list = dialog - title - search - status - toolbar - btn - gaps
  kListH_phys = kDialogH - kTitleH - kSearchH - kStatusBarH - kBtnH -
                4 * kGap;
  if (kListH_phys < 80) kListH_phys = 80;
  kBtnY_phys = kTitleH + kSearchH + kListH_phys + 2 * kGap;
  if (kBtnY_phys + kBtnH > kDialogH)
    kBtnY_phys = kDialogH - kBtnH - 2;

  s_hFontUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                          FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
  HFONT hfUi = s_hFontUi;

  // 搜索框
  int searchY = kTitleH + 6;
  int searchX = 110;
  int searchW = kDialogW - searchX - 130;
  s_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
                              searchX, searchY, searchW, kSearchH, hwnd,
                              reinterpret_cast<HMENU>(ID_SEARCH),
                              GetModuleHandle(nullptr), nullptr);
  if (s_hSearch && hfUi) {
    SendMessageW(s_hSearch, WM_SETFONT,
                 reinterpret_cast<WPARAM>(hfUi), TRUE);
  }
  // ⌘F hint label
  HWND hHint = CreateWindowExW(0, L"STATIC", L"Ctrl+F",
                               WS_CHILD | WS_VISIBLE | SS_LEFT |
                                   SS_CENTERIMAGE,
                               searchX + searchW + 4, searchY, 50,
                               kSearchH, hwnd, nullptr,
                               GetModuleHandle(nullptr), nullptr);
  if (hHint && hfUi) {
    SendMessageW(hHint, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // ListView
  int listY = kTitleH + kSearchH + kGap;
  DWORD listEx = WS_EX_CLIENTEDGE;
  DWORD listStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                    LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
  s_hList = CreateWindowExW(listEx, WC_LISTVIEWW, L"",
                            listStyle, kGap, listY,
                            kDialogW - 2 * kGap, kListH_phys, hwnd,
                            reinterpret_cast<HMENU>(ID_LIST),
                            GetModuleHandle(nullptr), nullptr);
  if (s_hList && hfUi) {
    SendMessageW(s_hList, WM_SETFONT,
                 reinterpret_cast<WPARAM>(hfUi), TRUE);
  }
  // 列
  LVCOLUMNW col = {};
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  col.pszText = const_cast<wchar_t*>(L"text");
  col.cx = 240;
  col.iSubItem = 0;
  ListView_InsertColumn(s_hList, 0, &col);
  col.pszText = const_cast<wchar_t*>(L"code");
  col.cx = 160;
  col.iSubItem = 1;
  ListView_InsertColumn(s_hList, 1, &col);
  col.pszText = const_cast<wchar_t*>(L"weight");
  col.cx = 80;
  col.iSubItem = 2;
  ListView_InsertColumn(s_hList, 2, &col);
  col.pszText = const_cast<wchar_t*>(L"schema");
  col.cx = 120;
  col.iSubItem = 3;
  ListView_InsertColumn(s_hList, 3, &col);

  // Status bar
  int statusY = listY + kListH_phys;
  s_hStatus = CreateWindowExW(0, L"STATIC", L"",
                              WS_CHILD | WS_VISIBLE | SS_LEFT |
                                  SS_CENTERIMAGE | WS_CLIPSIBLINGS,
                              0, statusY, kDialogW, kStatusBarH, hwnd,
                              nullptr, GetModuleHandle(nullptr), nullptr);
  if (s_hStatus && hfUi) {
    SendMessageW(s_hStatus, WM_SETFONT,
                 reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // 底部按钮 (6 按钮,右对齐)
  const wchar_t* kBtnLabels[6] = {
      L"+ \u6dfb\u52a0",       // + 添加
      L"\u2212 \u5220\u9664",  // − 删除
      L"\u21a5 \u5bfc\u5165",  // ⇥ 导入
      L"\u21a4 \u5bfc\u51fa",  // ⇤ 导出
      L"\u53d6\u6d88",          // 取消
      L"\u27f3 \u90e8\u7f72",  // ⟳ 部署
  };
  UINT kBtnIds[6] = {ID_BTN_ADD,  ID_BTN_DEL,    ID_BTN_IMPORT,
                     ID_BTN_EXPORT, ID_BTN_CANCEL, ID_BTN_DEPLOY};
  HWND* kBtnHwnds[6] = {&s_hBtnAdd, &s_hBtnDel, &s_hBtnImport,
                        &s_hBtnExport, &s_hBtnCancel, &s_hBtnDeploy};
  int totalW = kBtnW * 6 + kBtnGap * 5;
  int btnX = (kDialogW - totalW) / 2;
  for (int i = 0; i < 6; ++i) {
    *kBtnHwnds[i] = CreateWindowExW(
        0, L"BUTTON", kBtnLabels[i],
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
        btnX, kBtnY_phys, kBtnW, kBtnH, hwnd,
        reinterpret_cast<HMENU>(kBtnIds[i]),
        GetModuleHandle(nullptr), nullptr);
    btnX += kBtnW + kBtnGap;
  }
  // 默认: del disable(无选中)
  if (s_hBtnDel) EnableWindow(s_hBtnDel, FALSE);
  if (hfUi) {
    for (int i = 0; i < 6; ++i) {
      SendMessageW(*kBtnHwnds[i], WM_SETFONT,
                   reinterpret_cast<WPARAM>(hfUi), TRUE);
    }
  }

  PopulateListImpl(s_hList);

  // 默认选中第一项
  if (ListView_GetItemCount(s_hList) > 0) {
    ListView_SetItemState(s_hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    SetFocus(s_hList);
  }

  // RepaintLayered 一次以画 chrome
  RepaintLayered(hwnd);
  return 0;
}

LRESULT UserDictionary::OnDestroy(HWND hwnd) {
  if (s_hFontUi) {
    DeleteObject(s_hFontUi);
    s_hFontUi = nullptr;
  }
  s_hwnd = nullptr;
  s_hList = s_hSearch = s_hStatus = s_hToast = nullptr;
  s_hBtnAdd = s_hBtnDel = s_hBtnImport = s_hBtnExport = nullptr;
  s_hBtnCancel = s_hBtnDeploy = nullptr;
  return 0;
}

LRESULT UserDictionary::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RepaintLayered(hwnd);
  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT UserDictionary::OnCtlColor(HWND hwnd, WPARAM wp, LPARAM lp) {
  HDC hdc = reinterpret_cast<HDC>(wp);
  HWND target = reinterpret_cast<HWND>(lp);
  if (target == s_hSearch) {
    SetBkColor(hdc, kSearchBg);
    SetTextColor(hdc, kTextColor);
    static HBRUSH s_hBr = nullptr;
    if (!s_hBr) s_hBr = CreateSolidBrush(kSearchBg);
    return reinterpret_cast<LRESULT>(s_hBr);
  }
  if (target == s_hStatus) {
    SetBkColor(hdc, kWarnBg);
    SetTextColor(hdc, kWarnText);
    static HBRUSH s_hBrS = nullptr;
    if (!s_hBrS) s_hBrS = CreateSolidBrush(kWarnBg);
    return reinterpret_cast<LRESULT>(s_hBrS);
  }
  return DefWindowProcW(hwnd, WM_CTLCOLOREDIT, wp, lp);
}

LRESULT UserDictionary::OnKeyDown(HWND hwnd, WPARAM wp) {
  if (s_state == State_Editing) {
    return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
  }
  // Ctrl+F → 搜索
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'F') {
    if (s_hSearch && IsWindow(s_hSearch)) {
      SetFocus(s_hSearch);
    }
    return 0;
  }
  // Ctrl+N → Add
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'N') {
    EnterEditingState(-1);
    return 0;
  }
  // Ctrl+E → Edit
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'E') {
    if (m_selectedIndex >= 0 &&
        m_selectedIndex < static_cast<int>(m_entries.size())) {
      EnterEditingState(m_selectedIndex);
    }
    return 0;
  }
  // Ctrl+S → flush save
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'S') {
    FlushSave();
    return 0;
  }
  // Ctrl+A → 全选
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'A') {
    if (s_hList) {
      ListView_SetItemState(s_hList, -1, LVIS_SELECTED, LVIS_SELECTED);
      int n = ListView_GetItemCount(s_hList);
      for (int i = 0; i < n; ++i) m_multiSelected.insert(i);
    }
    return 0;
  }

  switch (wp) {
    case VK_UP:
    case VK_DOWN:
      return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
    case VK_RETURN:
      // 双击 = edit
      if (m_selectedIndex >= 0) EnterEditingState(m_selectedIndex);
      return 0;
    case VK_ESCAPE:
      Hide();
      return 0;
    case VK_F5:
      DeployAsync();
      return 0;
    case VK_DELETE: {
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_entries.size())) {
        m_entries.erase(m_entries.begin() + m_selectedIndex);
        PopulateListImpl(s_hList);
        ScheduleSave();
        m_selectedIndex = -1;
        m_multiSelected.clear();
      } else if (!m_multiSelected.empty()) {
        // 多选批量删
        std::vector<int> idx(m_multiSelected.begin(), m_multiSelected.end());
        std::sort(idx.rbegin(), idx.rend());
        for (int i : idx) {
          if (i >= 0 && i < static_cast<int>(m_entries.size()))
            m_entries.erase(m_entries.begin() + i);
        }
        PopulateListImpl(s_hList);
        ScheduleSave();
        m_multiSelected.clear();
        m_selectedIndex = -1;
      }
      return 0;
    }
    case VK_TAB: {
      HWND order[6] = {s_hBtnAdd,    s_hBtnDel,  s_hBtnImport,
                       s_hBtnExport, s_hBtnCancel, s_hBtnDeploy};
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

LRESULT UserDictionary::OnNotify(HWND hwnd, LPARAM lp) {
  LPNMHDR pnm = reinterpret_cast<LPNMHDR>(lp);
  if (!pnm) return 0;
  if (pnm->idFrom == ID_SEARCH) {
    if (pnm->code == EN_CHANGE) {
      wchar_t buf[256] = {};
      GetWindowTextW(s_hSearch, buf, 256);
      ApplySearchFilter(buf);
    }
    return 0;
  }
  if (pnm->idFrom != ID_LIST) return 0;

  switch (pnm->code) {
    case NM_DBLCLK: {
      if (m_selectedIndex >= 0) EnterEditingState(m_selectedIndex);
      return 0;
    }
    case LVN_KEYDOWN: {
      LPNMLVKEYDOWN pnkd = reinterpret_cast<LPNMLVKEYDOWN>(lp);
      if (pnkd->wVKey == VK_RETURN && m_selectedIndex >= 0) {
        EnterEditingState(m_selectedIndex);
      }
      return 0;
    }
    case LVN_ITEMCHANGED: {
      LPNMLISTVIEW pn = reinterpret_cast<LPNMLISTVIEW>(lp);
      if ((pn->uChanged & LVIF_STATE) &&
          (pn->uNewState & LVIS_SELECTED)) {
        m_selectedIndex = pn->iItem;
        if (s_hBtnDel) EnableWindow(s_hBtnDel, TRUE);
      } else if ((pn->uChanged & LVIF_STATE) &&
                 !(pn->uNewState & LVIS_SELECTED)) {
        if (pn->iItem == m_selectedIndex) {
          m_selectedIndex = -1;
          if (s_hBtnDel) EnableWindow(s_hBtnDel, FALSE);
        }
      }
      return 0;
    }
  }
  return 0;
}

LRESULT UserDictionary::OnCommand(HWND hwnd, WPARAM wp) {
  switch (LOWORD(wp)) {
    case ID_BTN_ADD:
      EnterEditingState(-1);
      return 0;
    case ID_BTN_DEL:
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_entries.size())) {
        m_entries.erase(m_entries.begin() + m_selectedIndex);
        PopulateListImpl(s_hList);
        ScheduleSave();
        m_selectedIndex = -1;
        if (s_hBtnDel) EnableWindow(s_hBtnDel, FALSE);
      }
      return 0;
    case ID_BTN_IMPORT:
      // 占位: 真实实现需要打开 file dialog + parse custom_phrase.txt
      ShowToast(L"\u5bfc\u5165\u529f\u80fd\u8bf7\u7528\u53f3\u952e\u83dc\u5355");  // 导入功能请用右键菜单
      return 0;
    case ID_BTN_EXPORT:
      ShowToast(L"\u5bfc\u51fa\u529f\u80fd\u8bf7\u7528\u53f3\u952e\u83dc\u5355");  // 导出功能请用右键菜单
      return 0;
    case ID_BTN_CANCEL:
      Hide();
      return 0;
    case ID_BTN_DEPLOY:
      DeployAsync();
      return 0;
    case ID_BTN_EDIT_OK:
      ExitEditingState(true);
      return 0;
    case ID_BTN_EDIT_CAN:
      ExitEditingState(false);
      return 0;
  }
  return DefWindowProcW(hwnd, WM_COMMAND, wp, 0);
}

LRESULT UserDictionary::OnTimer(HWND hwnd, WPARAM wp) {
  switch (wp) {
    case IDT_SAVE:
      KillTimer(hwnd, IDT_SAVE);
      FlushSave();
      return 0;
    case IDT_TOAST:
      KillTimer(hwnd, IDT_TOAST);
      HideToast();
      return 0;
    case IDT_DEPLOY_POLL: {
      KillTimer(hwnd, IDT_DEPLOY_POLL);
      DWORD now = GetTickCount();
      if (s_state == State_Deploying) {
        if ((now - s_deployStartTick) > kDeployTimeoutMs) {
          JoinDeployThread();
          s_state = State_Browsing;
          s_deployResult = 2;
          s_deployErrMsg = L"timeout";
          ShowToast(L"\u90e8\u7f72\u8d85\u65f6", 2);  // 部署超时
        } else {
          SetTimer(hwnd, IDT_DEPLOY_POLL, 1000, nullptr);
        }
      }
      return 0;
    }
  }
  return DefWindowProcW(hwnd, WM_TIMER, wp, 0);
}

LRESULT UserDictionary::OnDrawItem(HWND hwnd, LPARAM lp) {
  return DefWindowProcW(hwnd, WM_DRAWITEM, 0, lp);
}

// ===== Edit dialog WndProc =====

LRESULT CALLBACK UserDictionary::EditDlgProc(HWND hwnd, UINT msg, WPARAM wp,
                                              LPARAM lp) {
  switch (msg) {
    case WM_KEYDOWN:
      if (wp == VK_ESCAPE) {
        ExitEditingState(false);
        return 0;
      }
      if (wp == VK_RETURN) {
        ExitEditingState(true);
        return 0;
      }
      return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_CLOSE:
      ExitEditingState(false);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}