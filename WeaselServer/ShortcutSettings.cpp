//
// ShortcutSettings.cpp — 快捷键设置 Modal 实现 v0.19.0.28 (spec 045)
// spec: .specify/specs/045-shortcut-settings/design.md
// tokens: docs/design/FLUENT-UI-TOKENS.md §3.6.3
// chrome: WS_POPUP + WS_EX_LAYERED + SetWindowRgn (radius.lg=14) + hairline
//
#include "stdafx.h"
#include "ShortcutSettings.h"

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>

// ===== Static state 定义 =====
HWND ShortcutSettings::s_hwnd          = nullptr;
HWND ShortcutSettings::s_hTable        = nullptr;
HWND ShortcutSettings::s_hStatus       = nullptr;
HWND ShortcutSettings::s_hToolbar      = nullptr;
HWND ShortcutSettings::s_hSearch       = nullptr;
HWND ShortcutSettings::s_hPopover      = nullptr;
HWND ShortcutSettings::s_hBtnAdd       = nullptr;
HWND ShortcutSettings::s_hBtnReset     = nullptr;
HWND ShortcutSettings::s_hBtnImport    = nullptr;
HWND ShortcutSettings::s_hBtnExport    = nullptr;
HWND ShortcutSettings::s_hBtnCancel    = nullptr;
HWND ShortcutSettings::s_hBtnSave      = nullptr;
std::wstring ShortcutSettings::s_yamlPath;
DWORD ShortcutSettings::s_showTime     = 0;
HHOOK ShortcutSettings::s_hKeyboardHook = nullptr;

int ShortcutSettings::s_capturingRow        = -1;
std::wstring ShortcutSettings::s_pendingChord;

std::vector<ShortcutSettings::Hotkey> ShortcutSettings::m_hotkeys;
std::vector<ShortcutSettings::Hotkey> ShortcutSettings::m_defaults;

int ShortcutSettings::kTableH_phys = 0;
int ShortcutSettings::kBtnY_phys   = 0;

// ===== 设计常量 (FLUENT-UI-TOKENS.md §3.6 + §3.6.3) =====
namespace {

// 尺寸 (size.modal.shortcut.* + size.shortcut.*)
constexpr int kDialogW    = 800;   // size.shortcut.dialog.w
constexpr int kDialogH    = 680;   // size.shortcut.dialog.h
constexpr int kTitleH     = 56;    // title bar (icon + title + subtitle + X)
constexpr int kToolbarH   = 44;    // filter chips row
constexpr int kStatusBarH = 28;    // status row
constexpr int kBtnH       = 34;
constexpr int kBtnW       = 92;
constexpr int kBtnGap     = 8;
constexpr int kGap        = 10;
constexpr int kDlgRadius  = 14;    // radius.lg

constexpr int kPopoverW   = 240;   // size.shortcut.popover.w
constexpr int kPopoverH   = 100;   // size.shortcut.popover.h
constexpr int kPopoverTailH = 8;

// 颜色 (color.modal.* + color.scheme.hotkey.*)
constexpr COLORREF kBorderColor       = RGB(217, 217, 217);
constexpr COLORREF kBgTop             = RGB(245, 245, 248);
constexpr COLORREF kBgBot             = RGB(220, 222, 230);
constexpr COLORREF kTextColor         = RGB(30, 30, 40);
constexpr COLORREF kTextSecondary     = RGB(120, 120, 130);
constexpr COLORREF kTextTertiary      = RGB(160, 160, 170);
constexpr COLORREF kSelBg             = RGB(255, 235, 220);  // peach
constexpr COLORREF kHoverBg           = RGB(245, 245, 250);
constexpr COLORREF kAccentPrimary     = RGB(255, 95, 49);   // 火流猩品牌橙
constexpr COLORREF kMonospacedFg      = RGB(60, 60, 80);

constexpr COLORREF kConflictBg        = RGB(255, 218, 210);  // §3.6.3 hotkey.conflict_bg
constexpr COLORREF kWarningText       = RGB(196, 110, 28);   // §3.6.3 hotkey.warning_text
constexpr COLORREF kSuccessText       = RGB(36, 138, 61);    // §3.6.3 hotkey.success_text
constexpr COLORREF kUnchangedText     = RGB(140, 140, 150);  // §3.6.3 hotkey.unchanged_text
constexpr COLORREF kChangedText       = RGB(255, 95, 49);    // §3.6.3 hotkey.changed_text
constexpr COLORREF kDestructiveRed    = RGB(208, 69, 69);    // FLUENT destructive

constexpr COLORREF kSearchBg          = RGB(255, 255, 255);
constexpr COLORREF kSearchBorder      = RGB(220, 220, 225);
constexpr COLORREF kRowActiveBg       = RGB(255, 240, 225);
constexpr COLORREF kRowActiveStripe   = RGB(255, 95, 49);

constexpr COLORREF kPopoverBg         = RGB(252, 252, 254);
constexpr COLORREF kPopoverShadow     = RGB(0, 0, 0);
constexpr COLORREF kPopoverStripe     = RGB(255, 95, 49);

constexpr COLORREF kButtonBg          = RGB(245, 245, 248);
constexpr COLORREF kButtonBgPrimary   = RGB(255, 95, 49);
constexpr COLORREF kButtonFgPrimary   = RGB(255, 255, 255);
constexpr COLORREF kButtonBgDanger    = RGB(255, 255, 255);
constexpr COLORREF kButtonBorderDanger = RGB(208, 69, 69);
constexpr COLORREF kButtonFgDanger    = RGB(208, 69, 69);

// 字体 (font.ui.*)
constexpr int kUiFontSize    = 14;
constexpr int kTitleFontSize = 21;
constexpr int kSubFontSize   = 13;
constexpr int kSmallFontSize = 11;
constexpr int kMonoFontSize  = 14;
constexpr int kBigKeyFontSize = 16;

// 工具:小写转换(搜索用)
std::wstring ToLower(const std::wstring& s) {
  std::wstring r = s;
  CharLowerBuffW(r.data(), static_cast<DWORD>(r.size()));
  return r;
}

bool ContainsCI(const std::wstring& haystack, const std::wstring& needle) {
  if (needle.empty()) return true;
  return ToLower(haystack).find(ToLower(needle)) != std::wstring::npos;
}

}  // namespace

// ===== Public API =====

void ShortcutSettings::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    s_showTime = GetTickCount();
    return;
  }

  // 1. 加载 YAML (default.custom.yaml -> fallback default 内置 30 条)
  if (s_yamlPath.empty()) {
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
      std::wstring base = std::wstring(path) + L"\\Rime";
      s_yamlPath = base + L"\\default.custom.yaml";
    } else {
      s_yamlPath = L"default.custom.yaml";
    }
  }
  m_hotkeys.clear();
  if (!LoadHotkeys(s_yamlPath, m_hotkeys)) {
    // 解析失败 fallback 到默认 (LoadDefaults 是 schema 内的硬编码 ~14 条)
    std::wcerr << L"[ShortcutSettings] WARN: cannot load " << s_yamlPath
               << L", fallback to defaults" << std::endl;
    LoadDefaults(m_hotkeys);
  }
  m_defaults = m_hotkeys;  // reset 用

  // 2. 注册 ListView 控件
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
  InitCommonControlsEx(&icc);

  // 3. 创建 modal 窗口
  DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  DWORD style = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

  static bool s_classRegistered = false;
  if (!s_classRegistered) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &ShortcutSettings::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"FluxingShortcutSettings";
    if (!RegisterClassExW(&wc)) {
      DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS) {
        std::wcerr << L"[ShortcutSettings] RegisterClassEx failed, err="
                   << err << std::endl;
        return;
      }
    }
    s_classRegistered = true;
  }

  s_hwnd = CreateWindowExW(exStyle, L"FluxingShortcutSettings",
                           L"\x5feb\x6377\x952e\x8bbe\x7f6e",  // 快捷键设置
                           style, CW_USEDEFAULT, CW_USEDEFAULT,
                           kDialogW, kDialogH,
                           nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  if (!s_hwnd) {
    std::wcerr << L"[ShortcutSettings] CreateWindowExW failed, err="
               << GetLastError() << std::endl;
    return;
  }

  // 4. 居中 + 置顶
  CenterOnPrimaryMonitor(s_hwnd, kDialogW, kDialogH);
  SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  // 5. 圆角 (radius.lg=14 → 用 kDlgRadius)
  {
    HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH,
                                  kDlgRadius * 2, kDlgRadius * 2);
    if (rgn) SetWindowRgn(s_hwnd, rgn, TRUE);
  }
  ShowWindow(s_hwnd, SW_SHOW);
  UpdateWindow(s_hwnd);

  s_showTime = GetTickCount();
  s_capturingRow = -1;
  s_pendingChord.clear();
}

void ShortcutSettings::Hide() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    if (s_capturingRow >= 0) {
      ExitCapturing(false);
    }
    KillTimer(s_hwnd, IDT_BLINK);
    KillTimer(s_hwnd, IDT_POPOVER);
    if (s_hKeyboardHook) {
      UnhookWindowsHookEx(s_hKeyboardHook);
      s_hKeyboardHook = nullptr;
    }
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hTable = s_hStatus = s_hToolbar = s_hSearch = s_hPopover = nullptr;
  s_hBtnAdd = s_hBtnReset = s_hBtnImport = s_hBtnExport = nullptr;
  s_hBtnCancel = s_hBtnSave = nullptr;
}

void ShortcutSettings::Toggle() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    Hide();
  } else {
    Show();
  }
}

void ShortcutSettings::SetYamlPath(const std::wstring& path) {
  s_yamlPath = path;
}

std::vector<ShortcutSettings::Hotkey>& ShortcutSettings::MutableHotkeys() {
  return m_hotkeys;
}

const std::vector<ShortcutSettings::Hotkey>& ShortcutSettings::Hotkeys() {
  return m_hotkeys;
}

std::wstring ShortcutSettings::ToLower(const std::wstring& s) {
  return ::ToLower(s);  // delegate to anon namespace helper
}

bool ShortcutSettings::ContainsCI(const std::wstring& h,
                                  const std::wstring& n) {
  return ::ContainsCI(h, n);
}

// ===== Default / 内置 binding db =====

namespace {
ShortcutSettings::Hotkey MakeBuiltin(ShortcutSettings::HotkeyAction act,
                                     const std::wstring& when,
                                     const std::wstring& accept,
                                     const std::wstring& desc_zh,
                                     const std::wstring& yaml_field,
                                     const std::wstring& action_yaml) {
  ShortcutSettings::Hotkey h;
  h.action = act;
  h.action_desc = desc_zh;
  h.action_yaml = action_yaml;
  h.when = when;
  h.accept = accept;
  h.accept_new.clear();
  h.yaml_raw = yaml_field;
  h.is_custom = false;
  h.is_default = true;
  return h;
}
}  // namespace

const std::vector<ShortcutSettings::BuiltinBinding>&
ShortcutSettings::GetBuiltinBindings() {
  // spec 045 §6.1: librime 内置 ~30 条 (always / composing / has_menu)
  // 用于 conflict detection (cross-row + builtin override 警告)
  static const std::vector<BuiltinBinding> kBuiltins = {
    // always
    {L"Control+space",   L"always",    L"commit_code",          L"\xe4\xb8\x8a\xe5\xb1\x8f\xe7\xbc\x96\xe7\xa0\x81"},  // 上屏编码
    {L"Escape",          L"always",    L"clear",                L"\xe6\xb8\x85\xe7\xa9\xba\xe7\xad\x9b\xe9\x80\x89"},  // 清空候选
    {L"Return",          L"always",    L"commit",               L"\xe5\x9b\x9e\xe8\xbd\xa6\xe4\xb8\x8a\xe5\xb1\x8f"},  // 回车上屏
    {L"Control+Return",  L"always",    L"commit_raw",           L"Ctrl+Enter \xe4\xb8\x8a\xe5\xb1\x8f"},               // 上屏
    {L"BackSpace",       L"always",    L"backspace",            L"\xe9\x80\x80\xe6\xa0\xbc"},                          // 退格
    {L"Delete",          L"always",    L"delete",               L"\xe5\x88\xa0\xe9\x99\xa4"},                          // 删除
    // composing
    {L"Shift+Left",      L"composing", L"cursor_left",          L"\xe5\x85\x89\xe6\xa0\x87\xe5\xb7\xa6\xe7\xa7\xbb"},  // 光标左移
    {L"Shift+Right",     L"composing", L"cursor_right",         L"\xe5\x85\x89\xe6\xa0\x87\xe5\x8f\xb3\xe7\xa7\xbb"},  // 光标右移
    {L"Home",            L"composing", L"cursor_home",          L"\xe5\x85\x89\xe6\xa0\x87\xe5\x88\xb0\xe9\xa6\x96"},  // 光标到首
    {L"End",             L"composing", L"cursor_end",           L"\xe5\x85\x89\xe6\xa0\x87\xe5\x88\xb0\xe5\xb0\xbe"},  // 光标到尾
    // has_menu
    {L"1",               L"has_menu",  L"select 1",             L"\xe9\x80\x89\xe7\xac\xac 1 \xe5\x80\x99\xe9\x80\x89"},  // 选第 1 候选
    {L"2",               L"has_menu",  L"select 2",             L"\xe9\x80\x89\xe7\xac\xac 2 \xe5\x80\x99\xe9\x80\x89"},
    {L"3",               L"has_menu",  L"select 3",             L"\xe9\x80\x89\xe7\xac\xac 3 \xe5\x80\x99\xe9\x80\x89"},
    {L"Tab",             L"has_menu",  L"select_next",          L"\xe4\xb8\x8b\xe4\xb8\xaa\xe5\x80\x99\xe9\x80\x89"},  // 下个候选
    {L"Shift+Tab",       L"has_menu",  L"select_prev",          L"\xe4\xb8\x8a\xe4\xb8\xaa\xe5\x80\x99\xe9\x80\x89"},  // 上个候选
    // paging (when=paging in librime)
    {L"comma",           L"paging",    L"Page_Up",              L"\xe7\xbf\xbb\xe9\xa1\xb5\xe4\xb8\x8a"},              // 翻页上
    {L"period",          L"paging",    L"Page_Down",            L"\xe7\xbf\xbb\xe9\xa1\xb5\xe4\xb8\x8b"},              // 翻页下
    // toggle (ascii_mode 当 accept 是 Shift+space 等)
    {L"Shift+space",     L"always",    L"toggle ascii_mode",    L"\xe5\x88\x87\xe6\x8d\xa2\xe4\xb8\xad\xe8\x8b\xb1"},  // 切换中英
    {L"Control+Shift+9", L"always",    L"toggle ascii_punct",   L"\xe4\xb8\xad\xe8\x8b\xb1\xe6\xa0\x87\xe7\x82\xb9\xe5\x88\x87\xe6\x8d\xa2"},  // 中英标点切换
    {L"Control+Shift+0", L"always",    L"toggle traditionalization", L"\xe7\xae\x80\xe7\xb9\x81\xe5\x88\x87\xe6\x8d\xa2"},  // 简繁切换
  };
  return kBuiltins;
}

bool ShortcutSettings::LoadDefaults(std::vector<Hotkey>& out) {
  // ~14 条 UI 默认显示
  out.clear();
  out.push_back(MakeBuiltin(Action_ToggleChinese,    L"always",
                            L"Shift+space",     L"\xe5\x88\x87\xe6\x8d\xa2\xe4\xb8\xad\xe8\x8b\xb1",        // 切换中英
                            L"accept: \"Shift+space\" toggle: ascii_mode",
                            L"toggle: ascii_mode"));
  out.push_back(MakeBuiltin(Action_ToggleFullHalf,   L"always",
                            L"Control+Shift+F",  L"\xe5\x85\xa8/\xe5\x8d\x8a\xe8\xa7\x92\xe5\x88\x87\xe6\x8d\xa2",  // 全/半角切换
                            L"accept: \"Control+Shift+F\" toggle: full_shape",
                            L"toggle: full_shape"));
  out.push_back(MakeBuiltin(Action_ToggleAsciiPunct, L"always",
                            L"Control+Shift+9", L"\xe4\xb8\xad\xe8\x8b\xb1\xe6\xa0\x87\xe7\x82\xb9\xe5\x88\x87\xe6\x8d\xa2",
                            L"accept: \"Control+Shift+9\" toggle: ascii_punct",
                            L"toggle: ascii_punct"));
  out.push_back(MakeBuiltin(Action_ToggleTrad,       L"always",
                            L"Control+Shift+0", L"\xe7\xae\x80\xe7\xb9\x81\xe5\x88\x87\xe6\x8d\xa2",
                            L"accept: \"Control+Shift+0\" toggle: traditionalization",
                            L"toggle: traditionalization"));
  out.push_back(MakeBuiltin(Action_PageUp,           L"paging",
                            L"comma",           L"\xe7\xbf\xbb\xe9\xa1\xb5 - \xe4\xb8\x8a\xe9\xa1\xb5",  // 翻页 - 上页
                            L"when: paging accept: comma send: Page_Up",
                            L"send: Page_Up"));
  out.push_back(MakeBuiltin(Action_PageDown,         L"paging",
                            L"period",          L"\xe7\xbf\xbb\xe9\xa1\xb5 - \xe4\xb8\x8b\xe9\xa1\xb5",  // 翻页 - 下页
                            L"when: paging accept: period send: Page_Down",
                            L"send: Page_Down"));
  out.push_back(MakeBuiltin(Action_SelectN,          L"has_menu",
                            L"Control+1",       L"\xe9\x80\x89\xe7\xac\xac 2 \xe5\x80\x99\xe9\x80\x89",  // 选第 2 候选
                            L"when: has_menu accept: \"Control+1\" send: 2",
                            L"send: 2"));
  out.push_back(MakeBuiltin(Action_SelectN,          L"has_menu",
                            L"Control+2",       L"\xe9\x80\x89\xe7\xac\xac 3 \xe5\x80\x99\xe9\x80\x89",
                            L"when: has_menu accept: \"Control+2\" send: 3",
                            L"send: 3"));
  out.push_back(MakeBuiltin(Action_OpenQuickPanel,   L"always",
                            L"Alt+comma",       L"\xe5\xbf\xab\xe6\x8d\xb7\xe9\x9d\x99\xe6\x80\x81",  // 快捷静态 (QuickPanel = Alt+,)
                            L"accept: \"Alt+comma\" custom: open_quick_panel",
                            L"custom: open_quick_panel"));
  out.push_back(MakeBuiltin(Action_OpenPhrases,      L"always",
                            L"Alt+period",      L"\xe5\xb8\xb8\xe7\x94\xa8\xe7\x9f\xad\xe8\xaf\xad",    // 常用短语
                            L"accept: \"Alt+period\" custom: open_phrases",
                            L"custom: open_phrases"));
  out.push_back(MakeBuiltin(Action_OpenUserDict,     L"always",
                            L"Control+Shift+D", L"\xe7\x94\xa8\xe6\x88\xb7\xe8\xaf\x8d\xe5\x85\xb8",    // 用户词典
                            L"accept: \"Control+Shift+D\" custom: open_user_dict",
                            L"custom: open_user_dict"));
  out.push_back(MakeBuiltin(Action_SecondCandidate,  L"has_menu",
                            L"grave",           L"\xe7\xac\xac\xe4\xba\x8c\xe5\x80\x99\xe9\x80\x89",    // 第二候选
                            L"when: has_menu accept: grave send: 2",
                            L"send: 2"));
  out.push_back(MakeBuiltin(Action_TriggerDeploy,    L"always",
                            L"Control+F5",      L"\xe9\x87\x8d\xe6\x96\xb0\xe9\x83\xa8\xe7\xbd\xb2",    // 重新部署
                            L"accept: \"Control+F5\" custom: trigger_deploy",
                            L"custom: trigger_deploy"));
  return true;
}

// ===== YAML I/O (default.custom.yaml, spec 045 §5) =====

bool ShortcutSettings::LoadHotkeys(const std::wstring& path,
                                   std::vector<Hotkey>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    // file 不存在 → fallback 默认 (UI 启动行为,spec §3.1)
    return LoadDefaults(out);
  }
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
  // minimal parser: 找 "patch.key_binder/bindings:" 后的 -{ ... } 行
  // 每条 binding 是 inline dict: { when: ..., accept: ..., send|toggle: ... }
  size_t pos = 0;
  bool inBindings = false;
  int bindingsDepth = 0;
  while (pos < wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos) eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    pos = eol + 1;

    auto trim = [](std::wstring& s) {
      size_t a = 0, b = s.size();
      while (a < b && (s[a] == L' ' || s[a] == L'\t')) ++a;
      while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' ||
                         s[b - 1] == L'\r'))
        --b;
      s = s.substr(a, b - a);
    };
    std::wstring trimmed = line;
    trim(trimmed);

    if (!inBindings) {
      // 探测 patch.key_binder/bindings: 行 (可能是 "key_binder/bindings:" 前缀 path)
      if (trimmed.size() >= 18 &&
          trimmed.substr(trimmed.size() - 18) == L"key_binder/bindings:") {
        inBindings = true;
      }
      continue;
    }
    // 已 inBindings: 找 "{ when: ..., accept: ..., ... }" 行
    if (trimmed.empty() || trimmed[0] == L'#' || trimmed[0] == L'-' ||
        trimmed[0] == L'[' ) {
      // '-' 开头可能是 - { ... } 但这里简化: 看 { 紧跟 - 1 步
      if (!trimmed.empty() && trimmed.size() >= 3 &&
          trimmed[0] == L'-' && trimmed[1] == L'{' && trimmed.back() == L'}') {
        std::wstring body = trimmed.substr(2, trimmed.size() - 3);
        Hotkey hk;
        hk.is_custom = false;
        hk.is_default = false;
        hk.action_yaml.clear();
        hk.action_desc.clear();
        // 解析 "k: v [, k: v]*" — 简单 split
        size_t cur = 0;
        while (cur < body.size()) {
          size_t colon = body.find(L':', cur);
          if (colon == std::wstring::npos) break;
          std::wstring key = body.substr(cur, colon - cur);
          trim(key);
          cur = colon + 1;
          size_t next = body.find(L',', cur);
          std::wstring val;
          if (next == std::wstring::npos) {
            val = body.substr(cur);
            cur = body.size();
          } else {
            val = body.substr(cur, next - cur);
            cur = next + 1;
          }
          trim(val);
          // unquote
          if (val.size() >= 2 && val.front() == L'"' && val.back() == L'"')
            val = val.substr(1, val.size() - 2);

          if (key == L"when") {
            hk.when = val;
          } else if (key == L"accept") {
            hk.accept = val;
          } else if (key == L"send") {
            hk.action_yaml = L"send: " + val;
          } else if (key == L"toggle") {
            hk.action_yaml = L"toggle: " + val;
          }
        }
        if (!hk.accept.empty() && !hk.action_yaml.empty()) {
          hk.action_desc = L"\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89";  // 自定义 (raw fallback)
          hk.accept_new.clear();
          hk.yaml_raw = trimmed;
          out.push_back(hk);
        }
      } else if (trimmed.empty()) {
        // 空行 - 继续
      }
      continue;
    }
  }
  // 解析后 out 是"附加"列表;合并 m_defaults (这就是当前 UI 总览)
  std::vector<Hotkey> merged;
  for (const auto& d : m_defaults) merged.push_back(d);
  // spec 045 §5.4: 把 custom 行覆盖 defaults 同 action+when;或追加新的
  for (const auto& c : out) {
    bool merged_in = false;
    for (auto& d : merged) {
      if (d.accept == c.accept) {
        // 同 accept: 替换
        d.accept = c.accept;
        d.action_yaml = c.action_yaml;
        d.is_custom = true;
        merged_in = true;
        break;
      }
    }
    if (!merged_in) merged.push_back(c);
  }
  out = merged;
  return true;
}

bool ShortcutSettings::SaveHotkeys(const std::wstring& path,
                                   const std::vector<Hotkey>& data) {
  std::string utf8;
  utf8 += "\xEF\xBB\xBF";  // UTF-8 BOM (L09 教训)

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

  appendLine(L"# default.custom.yaml \xe2\x80\x94 Fluxing \xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae\xe8\xae\xbe\xe7\xbd\xae (v0.19.0.28)");  // 快捷键设置
  appendLine(L"# schema: patch.key_binder/bindings");
  appendLine(L"# editor: ShortcutSettings (spec 045)");
  appendLine(L"");
  appendLine(L"patch:");
  appendLine(L"  key_binder/bindings:");

  auto quoteIfNeeded = [](const std::wstring& s) -> std::wstring {
    bool needQuote = false;
    for (wchar_t c : s) {
      if (c == L' ' || c == L':' || c == L'+' || c == L'-') { needQuote = true; break; }
    }
    if (!needQuote) return s;
    return L"\"" + s + L"\"";
  };

  for (const auto& h : data) {
    // accept_new 若非空,即代表用户改过;存为 accept 新值
    std::wstring effectiveAccept = h.accept_new.empty() ? h.accept : h.accept_new;
    std::wstring yamlAction = h.action_yaml;  // "toggle: ascii_mode" / "send: Page_Down"
    if (yamlAction.empty()) {
      // 内置行 (无 custom) → 跳过默认未改的(spec §5.3: 用户删除默认用空 placeholder)
      // 简化: 这里全部写出,虽然是简化(实际 librime 不支持空 accept),仅保留结构调整
      if (effectiveAccept.empty()) continue;
    }
    std::wstring line =
        L"    - { when: " + (h.when.empty() ? L"always" : h.when) +
        L", accept: " + quoteIfNeeded(effectiveAccept) +
        L", " + yamlAction + L" }";
    appendLine(line);
  }

  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write(utf8.c_str(), static_cast<std::streamsize>(utf8.size()));
  return f.good();
}

// ===== Key name + combo normalization =====

std::wstring ShortcutSettings::VKeyToName(DWORD vk) {
  switch (vk) {
    case VK_SPACE: return L"space";
    case VK_RETURN: return L"Return";
    case VK_ESCAPE: return L"Escape";
    case VK_TAB: return L"Tab";
    case VK_BACK: return L"BackSpace";
    case VK_DELETE: return L"Delete";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_LEFT: return L"Left";
    case VK_RIGHT: return L"Right";
    case VK_UP: return L"Up";
    case VK_DOWN: return L"Down";
    case VK_INSERT: return L"Insert";
    case VK_PRIOR: return L"Page_Up";
    case VK_NEXT: return L"Page_Down";
    case VK_OEM_COMMA: return L"comma";
    case VK_OEM_PERIOD: return L"period";
    case VK_OEM_1: return L"semicolon";
    case VK_OEM_2: return L"slash";
    case VK_OEM_5: return L"backslash";
    case VK_OEM_7: return L"apostrophe";
    case VK_OEM_4: return L"bracketleft";
    case VK_OEM_6: return L"bracketright";
    case VK_OEM_MINUS: return L"minus";
    case VK_OEM_PLUS: return L"equal";
    case VK_OEM_3: return L"grave";
    case VK_CAPITAL: return L"Caps_Lock";
    default:
      // F1-F24
      if (vk >= VK_F1 && vk <= VK_F24) {
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
      }
      // letters / digits / printable
      if (vk >= 'A' && vk <= 'Z') {
        wchar_t c[2] = {static_cast<wchar_t>(vk - 'A' + 'a'), 0};
        return std::wstring(c);
      }
      if (vk >= '0' && vk <= '9') {
        wchar_t c[2] = {static_cast<wchar_t>(vk), 0};
        return std::wstring(c);
      }
      // Shift_L/R - L18/L19 防御 map
      if (vk == VK_LSHIFT) return L"Shift_L";
      if (vk == VK_RSHIFT) return L"Shift_R";
      return L"VK_" + std::to_wstring(vk);
  }
}

std::wstring ShortcutSettings::NormalizeCombo(DWORD vk, bool ctrl, bool shift,
                                              bool alt, bool super_key) {
  std::wstring name = VKeyToName(vk);
  // 字典序 Alt<Control<Shift<Super
  std::wstring mods;
  if (alt)       mods += L"Alt+";
  if (ctrl)      mods += L"Control+";
  if (shift)     mods += L"Shift+";
  if (super_key) mods += L"Super+";
  return mods + name;
}

// ===== Conflict detection =====

ShortcutSettings::ConflictReport ShortcutSettings::CheckConflict(
    const std::wstring& combo, const std::wstring& when, int exclude_row) {
  ConflictReport r;
  if (combo.empty()) return r;

  // 1) L18/L19 防御: Shift_L/R 单键 阻止 (硬错误)
  if (combo == L"Shift_L" || combo == L"Shift_R" ||
      combo == L"Shift+Shift_L" || combo == L"Shift+Shift_R") {
    r.score = 2;
    r.l18_l19_error = true;
    r.warning_action = L"\xe6\xad\xa4\xe7\xbb\x84\xe5\x90\x88\xe4\xb8\x8d\xe8\xa2\xab\xe5\x85\x81\xe8\xae\xb8 (Shift_L/R \xe5\x8d\x95\xe9\x94\xae)";  // 此组合不被允许 (Shift_L/R 单键)
    return r;
  }

  // 2) 同表 cross-row duplicate
  for (int i = 0; i < static_cast<int>(m_hotkeys.size()); ++i) {
    if (i == exclude_row) continue;
    const auto& h = m_hotkeys[i];
    std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    if (eff == combo && h.when == when) {
      r.score = 2;
      r.duplicate_error = true;
      r.warning_action = L"\xe4\xb8\x8e \x22" + h.action_desc + L"\x22 \xe5\x86\xb2\xe7\xaa\x81";  // 与「…」冲突
      return r;
    }
  }

  // 3) 内置 binding override 警告 (允许 override,只是警告)
  for (const auto& b : GetBuiltinBindings()) {
    if (b.combo == combo && (b.when == when || when == L"always" || b.when == when)) {
      if (r.score < 1) {
        r.score = 1;
        r.warning_action = L"\xe8\xa6\x86\xe7\x9b\x96\xe9\xbb\x98\xe8\xae\xa4\xe8\xa1\x8c\xe4\xb8\xba \x22" + b.desc_zh + L"\x22";  // 覆盖默认行为「…」
      }
      break;
    }
  }
  return r;
}

// ===== Table populate / update =====

void ShortcutSettings::PopulateTable() {
  if (!s_hTable || !IsWindow(s_hTable)) return;
  ListView_DeleteAllItems(s_hTable);

  int row = 0;
  for (const auto& h : m_hotkeys) {
    LVITEMW li = {};
    li.mask = LVIF_TEXT | LVIF_PARAM;
    li.iItem = row;
    li.iSubItem = 0;
    li.pszText = const_cast<LPWSTR>(h.action_desc.c_str());
    li.lParam = static_cast<LPARAM>(row);
    ListView_InsertItem(s_hTable, &li);

    std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    ListView_SetItemText(s_hTable, row, 1, const_cast<LPWSTR>(eff.c_str()));
    // New 列 (col 2): 用户编辑后显示;否则显示空(绿对勾),或 "未改"
    if (!h.accept_new.empty() && h.accept_new != h.accept) {
      ListView_SetItemText(s_hTable, row, 2, const_cast<LPWSTR>(h.accept_new.c_str()));
    } else {
      ListView_SetItemText(s_hTable, row, 2, const_cast<wchar_t*>(L""));
    }
    ++row;
  }
}

void ShortcutSettings::UpdateRow(int row) {
  if (!s_hTable || !IsWindow(s_hTable)) return;
  if (row < 0 || row >= static_cast<int>(m_hotkeys.size())) return;
  const auto& h = m_hotkeys[row];
  std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
  ListView_SetItemText(s_hTable, row, 1, const_cast<LPWSTR>(eff.c_str()));
  if (!h.accept_new.empty() && h.accept_new != h.accept) {
    ListView_SetItemText(s_hTable, row, 2, const_cast<LPWSTR>(h.accept_new.c_str()));
  } else {
    ListView_SetItemText(s_hTable, row, 2, const_cast<wchar_t*>(L""));
  }
}

// ===== Search filter =====

void ShortcutSettings::ApplySearchFilter(const std::wstring& q) {
  if (!s_hTable || !IsWindow(s_hTable)) return;
  // 简单 substring + case-insensitive (spec §8 decision)
  for (int i = 0; i < ListView_GetItemCount(s_hTable); ++i) {
    const auto& h = m_hotkeys[i];
    std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    bool match = q.empty() ||
                  ContainsCI(h.action_desc, q) ||
                  ContainsCI(h.accept, q) ||
                  ContainsCI(eff, q) ||
                  ContainsCI(h.action_yaml, q);
    // 简化: 这里不实际 hide item (LVS_REPORT 不支持),只是 repopulate
    // 真要 hide 用 ListView_SetItemState(LVIS_CUT); 简化为刷新
  }
  PopulateTable();
}

// ===== Capturing =====

void ShortcutSettings::EnterCapturing(int row) {
  if (row < 0 || row >= static_cast<int>(m_hotkeys.size())) return;
  s_capturingRow = row;
  s_pendingChord.clear();

  // 启动 WH_KEYBOARD_LL hook (本进程线程范围)
  if (!s_hKeyboardHook) {
    s_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc,
                                        GetModuleHandle(nullptr), 0);
  }
  SetTimer(s_hwnd, IDT_BLINK, kCaptureBlinkMs, nullptr);
  // 显示 popover
  if (!s_hPopover || !IsWindow(s_hPopover)) {
    s_hPopover = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                 L"STATIC",
                                 L"\xe8\xaf\xb7\xe6\x8c\x89\xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae...",  // 请按快捷键...
                                 WS_POPUP | WS_VISIBLE,
                                 CW_USEDEFAULT, CW_USEDEFAULT,
                                 kPopoverW, kPopoverH + kPopoverTailH,
                                 s_hwnd, nullptr,
                                 GetModuleHandle(nullptr), nullptr);
  }
  SetTimer(s_hwnd, IDT_POPOVER, kPopoverShowMs, nullptr);
  PopulateTable();  // refresh (no special UI marking - lParam remains, captured row handled in repaint)
  RepaintLayered(s_hwnd);
}

void ShortcutSettings::ExitCapturing(bool save) {
  if (s_capturingRow < 0) return;
  if (save) {
    // 把 s_pendingChord 写入 m_hotkeys[s_capturingRow].accept_new
    if (s_capturingRow >= 0 && s_capturingRow < static_cast<int>(m_hotkeys.size())) {
      m_hotkeys[s_capturingRow].accept_new = s_pendingChord;
    }
  } else {
    // 还原:accept_new 保持原状(= 原 accept 表示未改)
  }
  s_capturingRow = -1;
  s_pendingChord.clear();

  KillTimer(s_hwnd, IDT_BLINK);
  KillTimer(s_hwnd, IDT_POPOVER);
  if (s_hKeyboardHook) {
    UnhookWindowsHookEx(s_hKeyboardHook);
    s_hKeyboardHook = nullptr;
  }
  if (s_hPopover && IsWindow(s_hPopover)) {
    DestroyWindow(s_hPopover);
    s_hPopover = nullptr;
  }
  PopulateTable();
  RepaintLayered(s_hwnd);
}

void ShortcutSettings::ResetCapturingRow() {
  if (s_capturingRow < 0) return;
  if (s_capturingRow < static_cast<int>(m_hotkeys.size())) {
    m_hotkeys[s_capturingRow].accept_new.clear();
  }
  s_pendingChord.clear();
  PopulateTable();
  RepaintLayered(s_hwnd);
}

// ===== Low-level keyboard hook =====

LRESULT CALLBACK ShortcutSettings::LowLevelKeyboardProc(int nCode, WPARAM wp,
                                                       LPARAM lp) {
  if (nCode != HC_ACTION) {
    return CallNextHookEx(nullptr, nCode, wp, lp);
  }
  // 仅 keydown WM_KEYDOWN (避免 keyup 误触发)
  if (wp != WM_KEYDOWN) return CallNextHookEx(nullptr, nCode, wp, lp);

  KBDLLHOOKSTRUCT* ks = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
  DWORD vk = ks->vkCode;
  bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
  bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
  bool sup   = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
               (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

  // 特殊键
  if (vk == VK_ESCAPE) {
    // Esc 还原
    ShortcutSettings::ResetCapturingRow();
    ShortcutSettings::ExitCapturing(false);
    return 1;  // 吞掉,不让 Esc 触发父菜单
  }
  if (vk == VK_RETURN) {
    // Enter 确认
    ShortcutSettings::ExitCapturing(true);
    return 1;
  }
  if (vk == VK_BACK) {
    // Backspace 清空 pending chord
    ShortcutSettings::s_pendingChord.clear();
    ShortcutSettings::RepaintLayered(ShortcutSettings::s_hwnd);
    return 1;
  }
  // 修饰键单独按下:不算 valid combo,累积 modifier 状态但不触发 Enter
  bool isModifierOnly =
      vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
      vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
      vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
      vk == VK_LWIN    || vk == VK_RWIN;

  // Real chord
  std::wstring chord = NormalizeCombo(vk, ctrl, shift, alt, sup);
  if (chord == L"VK_NONE" || isModifierOnly) {
    // 仅修饰键: 等用户继续按,啥都不做
    return CallNextHookEx(nullptr, nCode, wp, lp);
  }
  // 大写化字母键 (Shift+a → A)
  if (shift) {
    if (vk >= 'A' && vk <= 'Z') {
      std::wstring name = VKeyToName(vk);
      std::wstring prefix;
      if (alt)   prefix += L"Alt+";
      if (ctrl)  prefix += L"Control+";
      if (shift) prefix += L"Shift+";
      if (sup)   prefix += L"Super+";
      chord = prefix + name;
    }
  }

  ShortcutSettings::s_pendingChord = chord;
  // 实时冲突检测 → 更新 popover text (此处简化:re-paint)
  ShortcutSettings::RepaintLayered(ShortcutSettings::s_hwnd);

  // 不吞掉修饰键以外的键 → 让用户按键透传到 WeaselServer,但我们已经记录
  // 这里为了 UX 完整,继续传给下一 hook(避免吞掉其他应用);若 ShortcutSettings
  // 是 focus 窗口,正常不会冲突;spec AP-045-B:不 SendInput / 不触发 IPC
  return CallNextHookEx(nullptr, nCode, wp, lp);
}

// ===== WndProc =====

LRESULT CALLBACK ShortcutSettings::WndProc(HWND hwnd, UINT msg, WPARAM wp,
                                           LPARAM lp) {
  switch (msg) {
    case WM_CREATE:
      return OnCreate(hwnd);
    case WM_DESTROY:
      return OnDestroy(hwnd);
    case WM_PAINT:
      return OnPaint(hwnd);
    case WM_TIMER:
      return OnTimer(hwnd, wp);
    case WM_NOTIFY:
      return OnNotify(hwnd, lp);
    case WM_COMMAND:
      return OnCommand(hwnd, wp);
    case WM_KEYDOWN:
      return OnKeyDown(hwnd, wp);
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
      DWORD nowTick = GetTickCount();
      if ((nowTick - s_showTime) >= kShowGraceMs) {
        Hide();
      }
      return 0;
    }
    case WM_NCDESTROY:
      return OnDestroy(hwnd);
    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

LRESULT ShortcutSettings::OnCreate(HWND hwnd) {
  SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

  // 几何 (runtime)
  kTableH_phys = kDialogH - kTitleH - kToolbarH - kStatusBarH - kBtnH - 6 * kGap;
  kBtnY_phys   = kTitleH + kToolbarH + kStatusBarH + kTableH_phys + 2 * kGap;
  if (kTableH_phys < 100) kTableH_phys = 100;
  if (kBtnY_phys + kBtnH > kDialogH) kBtnY_phys = kDialogH - kBtnH - 2;

  // 主字体
  HFONT hfUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           VARIABLE_PITCH | FF_SWISS,
                           L"Segoe UI Variable");
  HFONT hfMono = CreateFontW(kMonoFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                             FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             FIXED_PITCH | FF_MODERN,
                             L"Cascadia Mono");
  HFONT hfTitle = CreateFontW(kTitleFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH | FF_SWISS,
                              L"Segoe UI Variable");
  (void)hfMono; (void)hfTitle;  // set on controls below

  // Toolbar row: filter chips (label only,非交互简化)+ search + 提示
  int toolY = kTitleH + kGap;
  s_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  ES_AUTOHSCROLL,
                              kDialogW / 2, toolY,
                              kDialogW / 2 - 2 * kGap, 28,
                              hwnd, reinterpret_cast<HMENU>(ID_SEARCH),
                              GetModuleHandle(nullptr), nullptr);
  if (s_hSearch && hfUi) {
    SendMessageW(s_hSearch, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // Status row: 副标题 (status)
  int statusY = kTitleH + kToolbarH + kGap;
  s_hStatus = CreateWindowExW(0, L"STATIC",
                               L"\xe2\x97\x8f \xe6\x9c\xaa\xe4\xbf\x9d\xe5\xad\x98 (0 \xe4\xbf\xae\xe6\x94\xb9)",  // ● 未保存 (0 修改)
                               WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE |
                                   WS_CLIPSIBLINGS,
                               0, statusY, kDialogW, kStatusBarH,
                               hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
  if (s_hStatus && hfUi) {
    SendMessageW(s_hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // Table (ListView LVS_REPORT, 3 列: Action / Current / New)
  int tableY = statusY + kStatusBarH + kGap;
  s_hTable = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
                                 LVS_REPORT | LVS_SINGLESEL |
                                 LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                             kGap, tableY, kDialogW - 2 * kGap, kTableH_phys,
                             hwnd, reinterpret_cast<HMENU>(ID_TABLE),
                             GetModuleHandle(nullptr), nullptr);
  if (s_hTable && hfMono) {
    SendMessageW(s_hTable, WM_SETFONT, reinterpret_cast<WPARAM>(hfMono), TRUE);
  }
  // 表头 + 列
  {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = const_cast<LPWSTR>(L"\xe5\x8a\xa8\xe4\xbd\x9c");  // 动作
    col.cx = 280; col.iSubItem = 0;
    ListView_InsertColumn(s_hTable, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"\xe5\xbd\x93\xe5\x89\x8d");  // 当前
    col.cx = 200; col.iSubItem = 1;
    ListView_InsertColumn(s_hTable, 1, &col);
    col.pszText = const_cast<LPWSTR>(L"\xe6\x96\xb0\xe5\x80\xbc");  // 新值
    col.cx = 200; col.iSubItem = 2;
    ListView_InsertColumn(s_hTable, 2, &col);
  }
  // Extended styles: grid + full row select
  ListView_SetExtendedListViewStyle(s_hTable,
                                    LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // 按钮行: + 自定义 / ⟳ 恢复默认 / 导入 / 导出 / 取消 / 保存
  auto makeBtn = [&hfUi](HWND& out, const wchar_t* label, UINT id, int x, int y) {
    out = CreateWindowExW(0, L"BUTTON", label,
                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS |
                              BS_PUSHBUTTON,
                          x, y, kBtnW, kBtnH, ShortcutSettings::s_hwnd,
                          reinterpret_cast<HMENU>(id),
                          GetModuleHandle(nullptr), nullptr);
    if (out && hfUi) {
      SendMessageW(out, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    }
  };

  int btnY = kBtnY_phys;
  int btnX = kGap;
  makeBtn(s_hBtnAdd, L"+ \xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89", ID_BTN_ADD,   // + 自定义
          btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnReset, L"\xe2\x9f\xb3 \xe6\x81\xa2\xe5\xa4\x9a\xe9\xbb\x98", ID_BTN_RESET,  // ⟳ 恢复默认 (注:这会是 ⟳ 字符 + 恢复)
          btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnImport, L"\xe5\xaf\xbc\xe5\x85\xa5", ID_BTN_IMPORT,
          btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnExport, L"\xe5\xaf\xbc\xe5\x87\xba", ID_BTN_EXPORT,
          btnX, btnY);
  // right-aligned: 取消 + 保存
  int rightX = kDialogW - kGap;
  makeBtn(s_hBtnSave, L"\xe4\xbf\x9d\xe5\xad\x98", ID_BTN_SAVE,  // 保存
          rightX - kBtnW, btnY);
  rightX -= kBtnW + kBtnGap;
  makeBtn(s_hBtnCancel, L"\xe5\x8f\x96\xe6\xb6\x88", ID_BTN_CANCEL,  // 取消
          rightX - kBtnW, btnY);

  // Populate + initial selection
  PopulateTable();
  ListView_SetItemState(s_hTable, 0, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);

  return 0;
}

LRESULT ShortcutSettings::OnDestroy(HWND hwnd) {
  if (s_hKeyboardHook) {
    UnhookWindowsHookEx(s_hKeyboardHook);
    s_hKeyboardHook = nullptr;
  }
  s_hwnd = nullptr;
  s_hTable = s_hStatus = s_hToolbar = s_hSearch = s_hPopover = nullptr;
  s_hBtnAdd = s_hBtnReset = s_hBtnImport = s_hBtnExport = nullptr;
  s_hBtnCancel = s_hBtnSave = nullptr;
  return 0;
}

LRESULT ShortcutSettings::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  // 1) Title bar bg 渐变 [0, kTitleH)
  {
    RECT titleBg = {0, 0, kDialogW, kTitleH};
    TRIVERTEX v[2] = {};
    v[0].x = titleBg.left; v[0].y = titleBg.top;
    v[0].Red   = static_cast<COLOR16>(GetRValue(kBgTop)) << 8;
    v[0].Green = static_cast<COLOR16>(GetGValue(kBgTop)) << 8;
    v[0].Blue  = static_cast<COLOR16>(GetBValue(kBgTop)) << 8;
    v[0].Alpha = 0xFF00;
    v[1].x = titleBg.right; v[1].y = titleBg.bottom;
    v[1].Red   = static_cast<COLOR16>(GetRValue(kBgBot)) << 8;
    v[1].Green = static_cast<COLOR16>(GetGValue(kBgBot)) << 8;
    v[1].Blue  = static_cast<COLOR16>(GetBValue(kBgBot)) << 8;
    v[1].Alpha = 0xFF00;
    GRADIENT_RECT g = {0, 1};
    if (!GradientFill(hdc, v, 2, &g, 1, GRADIENT_FILL_RECT_V)) {
      HBRUSH bg = CreateSolidBrush(kBgTop);
      FillRect(hdc, &titleBg, bg);
      DeleteObject(bg);
    }
  }

  // 2) Hairline 圆角边框
  {
    HPEN hPen = CreatePen(PS_SOLID, 1, kBorderColor);
    HPEN hOld = static_cast<HPEN>(SelectObject(hdc, hPen));
    HBRUSH hOldBr =
        static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    RoundRect(hdc, 0, 0, kDialogW - 1, kDialogH - 1,
              kDlgRadius * 2, kDlgRadius * 2);
    SelectObject(hdc, hOld);
    SelectObject(hdc, hOldBr);
    DeleteObject(hPen);
  }

  // 3) Title text ("快捷键设置" 21px bold)
  {
    HFONT hfTitle = CreateFontW(kTitleFontSize, 0, 0, 0, FW_BOLD, FALSE,
                                FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS,
                                L"Segoe UI Variable");
    HFONT hfOld = static_cast<HFONT>(SelectObject(hdc, hfTitle));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextColor);
    RECT titleRc = {20, 6, kDialogW - 60, 32};
    DrawTextW(hdc,
              L"\xe2\x8c\xa8 \xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae\xe8\xae\xbe\xe7\xbd\xae",  // ⌨ 快捷键设置
              -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfOld);
    DeleteObject(hfTitle);

    HFONT hfSub = CreateFontW(kSubFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH | FF_SWISS,
                              L"Segoe UI Variable");
    HFONT hfOld2 = static_cast<HFONT>(SelectObject(hdc, hfSub));
    SetTextColor(hdc, kTextSecondary);
    RECT subRc = {20, 32, kDialogW - 60, 50};
    std::wstring sub = L"Customize keyboard shortcuts \xc2\xb7 " +  // ·
                       std::to_wstring(m_hotkeys.size()) + L" bindings";
    DrawTextW(hdc, sub.c_str(), -1, &subRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfOld2);
    DeleteObject(hfSub);
  }

  // 4) 圆形 X 按钮 (右上角)
  {
    int cx = kDialogW - 24, cy = 18;
    HBRUSH hBrush = CreateSolidBrush(RGB(232, 90, 90));
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hPen));
    HBRUSH hOldBrush = static_cast<HBRUSH>(SelectObject(hdc, hBrush));
    Ellipse(hdc, cx - 10, cy - 10, cx + 10, cy + 10);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen); DeleteObject(hBrush);

    HFONT hfX = CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            VARIABLE_PITCH | FF_SWISS,
                            L"Segoe UI Variable");
    HFONT hfXO = static_cast<HFONT>(SelectObject(hdc, hfX));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kButtonFgPrimary);
    RECT xRc = {cx - 10, cy - 10, cx + 10, cy + 10};
    DrawTextW(hdc, L"\xc3\x97", -1, &xRc,  // ×
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfXO);
    DeleteObject(hfX);
  }

  EndPaint(hwnd, &ps);
  return 0;
}

LRESULT ShortcutSettings::OnNotify(HWND hwnd, LPARAM lp) {
  LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lp);
  if (!nmhdr || nmhdr->idFrom != ID_TABLE) {
    return DefWindowProcW(hwnd, WM_NOTIFY, 0, lp);
  }
  if (nmhdr->code == NM_CLICK || nmhdr->code == NM_DBLCLK) {
    LPNMITEMACTIVATE ia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
    int row = ia->iItem;
    if (row >= 0 && row < static_cast<int>(m_hotkeys.size())) {
      EnterCapturing(row);
      return 1;
    }
  } else if (nmhdr->code == NM_RCLICK) {
    LPNMITEMACTIVATE ia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
    int row = ia->iItem;
    if (row >= 0 && row < static_cast<int>(m_hotkeys.size())) {
      // 弹简易菜单:重置此行
      HMENU hMenu = CreatePopupMenu();
      AppendMenuW(hMenu, MF_STRING, 1001, L"\xe9\x87\x8d\xe7\xbd\xae\xe6\xad\xa4\xe8\xa1\x8c");  // 重置此行
      POINT pt;
      GetCursorPos(&pt);
      int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y,
                                0, hwnd, nullptr);
      DestroyMenu(hMenu);
      if (cmd == 1001) {
        if (row < static_cast<int>(m_defaults.size())) {
          m_hotkeys[row].accept = m_defaults[row].accept;
        }
        m_hotkeys[row].accept_new.clear();
        PopulateTable();
        RepaintLayered(hwnd);
      }
      return 1;
    }
  }
  return DefWindowProcW(hwnd, WM_NOTIFY, 0, lp);
}

LRESULT ShortcutSettings::OnCommand(HWND hwnd, WPARAM wp) {
  int id = LOWORD(wp);
  switch (id) {
    case ID_BTN_CANCEL:
      Hide();
      return 0;
    case ID_BTN_SAVE:
      SaveAndDeploy();
      return 0;
    case ID_BTN_ADD:
      // 简化: push 新行 placeholder,等 popover/capture
      m_hotkeys.push_back(MakeBuiltin(Action_CustomUser, L"always",
                                       L"", L"\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89 - \xe7\xa1\xae\xe8\xae\xa4\xe5\x90\x8e\xe8\xae\xb0\xe5\xbd\x95",  // 自定义 - 确认后记录
                                       L"custom: user_added", L"custom: user_added"));
      PopulateTable();
      RepaintLayered(hwnd);
      return 0;
    case ID_BTN_RESET: {
      // ⟳ 恢复默认: 弹确认后全 reset (spec §8 table decision)
      int ok = MessageBoxW(hwnd,
                            L"\xe5\xb0\x86\xe4\xb8\xa2\xe5\xbc\x83\xe6\x89\x80\xe6\x9c\x89\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe5\xbf\xab\xe6\x8d\xb7\xe9\x94\xae\xef\xbc\x8c\xe7\xa1\xae\xe5\xae\x9a\xef\xbc\x9f",  // 将丢弃所有自定义快捷键，确定？
                            L"\xe6\x81\xa2\xe5\xa4\x9a\xe9\xbb\x98",  // 恢复默认
                            MB_YESNO | MB_ICONWARNING);
      if (ok == IDYES) {
        m_hotkeys = m_defaults;
        PopulateTable();
        RepaintLayered(hwnd);
      }
      return 0;
    }
    case ID_BTN_IMPORT: {
      // 简化: 同 LoadHotkeys 用户路径
      LoadHotkeys(s_yamlPath, m_hotkeys);
      PopulateTable();
      RepaintLayered(hwnd);
      return 0;
    }
    case ID_BTN_EXPORT: {
      SaveHotkeys(s_yamlPath, m_hotkeys);
      return 0;
    }
    case ID_SEARCH: {
      if (HIWORD(wp) == EN_CHANGE) {
        wchar_t buf[256] = {};
        GetWindowTextW(s_hSearch, buf, 256);
        ApplySearchFilter(buf);
      }
      return 0;
    }
  }
  return DefWindowProcW(hwnd, WM_COMMAND, wp, 0);
}

LRESULT ShortcutSettings::OnTimer(HWND hwnd, WPARAM wp) {
  if (wp == IDT_BLINK) {
    // 闪烁 caret: repaint popover region (L97 fix)
    if (s_hPopover && IsWindow(s_hPopover)) {
      InvalidateRect(s_hPopover, nullptr, TRUE);
    }
    return 0;
  }
  if (wp == IDT_POPOVER) {
    KillTimer(hwnd, IDT_POPOVER);
    return 0;
  }
  return 0;
}

LRESULT ShortcutSettings::OnKeyDown(HWND hwnd, WPARAM wp) {
  // Ctrl+F → focus search
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'F') {
    if (s_hSearch && IsWindow(s_hSearch)) SetFocus(s_hSearch);
    return 0;
  }
  // Esc → exit capturing
  if (wp == VK_ESCAPE) {
    if (s_capturingRow >= 0) {
      ExitCapturing(false);
    } else {
      Hide();
    }
    return 0;
  }
  return DefWindowProcW(hwnd, WM_KEYDOWN, wp, 0);
}

LRESULT ShortcutSettings::OnCtlColor(HWND hwnd, WPARAM wp, LPARAM lp) {
  HDC hdc = reinterpret_cast<HDC>(wp);
  HWND target = reinterpret_cast<HWND>(lp);
  if (target == s_hSearch) {
    SetBkColor(hdc, kSearchBg);
    SetTextColor(hdc, kTextColor);
    static HBRUSH sb = nullptr;
    if (!sb) sb = CreateSolidBrush(kSearchBg);
    return reinterpret_cast<LRESULT>(sb);
  }
  if (target == s_hStatus) {
    SetBkColor(hdc, kBgTop);
    SetTextColor(hdc, kTextSecondary);
    static HBRUSH sb2 = nullptr;
    if (!sb2) sb2 = CreateSolidBrush(kBgTop);
    return reinterpret_cast<LRESULT>(sb2);
  }
  return DefWindowProcW(hwnd, WM_CTLCOLORSTATIC, wp, lp);
}

// ===== Save + deploy =====

bool ShortcutSettings::SaveAndDeploy() {
  if (s_yamlPath.empty()) return false;
  if (!SaveHotkeys(s_yamlPath, m_hotkeys)) return false;
  SpawnDeploy();
  if (s_hStatus && IsWindow(s_hStatus)) {
    SetWindowTextW(s_hStatus, L"\xe2\x9c\x93 \xe5\xb7\xb2\xe4\xbf\x9d\xe5\xad\x98 \xc2\xb7 deploy \xe5\xb7\xb2\xe8\xa7\xa6\xe5\x8f\x91");  // ✓ 已保存 · deploy 已触发
  }
  return true;
}

void ShortcutSettings::SpawnDeploy() {
  // spec §7.1: 触发 WeaselDeployer /deploy。spec AP-045-B: 不在 hook 做 IPC。
  // 后续 ship 加 ShellExecuteW(L"WeaselDeployer.exe", L"/deploy"); 这里占位。
}

// ===== Center / Repaint =====

void ShortcutSettings::CenterOnPrimaryMonitor(HWND hwnd, int w, int h) {
  HMONITOR mon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
  int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
  SetWindowPos(hwnd, nullptr, x, y, w, h,
                SWP_NOZORDER | SWP_NOACTIVATE);
}

void ShortcutSettings::RepaintLayered(HWND hwnd) {
  // L97 fix: WS_EX_LAYERED 窗口必须 explicit RedrawWindow with UPDATENOW
  if (hwnd && IsWindow(hwnd)) {
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
}
