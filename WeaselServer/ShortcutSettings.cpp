//
// ShortcutSettings.cpp — 快捷键设置 Modal 实现 v0.19.0.28 (spec 045)
// spec: .specify/specs/045-shortcut-settings/design.md
// tokens: docs/design/FLUENT-UI-TOKENS.md §3.6.3
// chrome: WS_POPUP + WS_EX_LAYERED + SetWindowRgn (radius.lg=14) + hairline
//
#include "stdafx.h"
#include "ShortcutSettings.h"
#include "ModalChrome.h"  // v0.19.0.31: BG+Border paint helper

#include <KeyNameMapper.h>  // ADR 0011 D1: shared key name unit
#include <WeaselUtility.h>  // WeaselUserDataPath: 用户目录单一事实源 (ADR 0020)

#include "ShortcutSettingsT110.h"  // T110: 全局快捷键表基线 (default.yaml)

#include <algorithm>
#include <cassert>
#include <commctrl.h>
#include <fstream>
#include <iostream>
#include <shlobj.h>
#include <shellapi.h>  // ShellExecuteW: SpawnDeploy 拉起 WeaselDeployer

// ===== Static state 定义 =====
HWND ShortcutSettings::s_hwnd = nullptr;
HWND ShortcutSettings::s_hTable = nullptr;
HWND ShortcutSettings::s_hStatus = nullptr;
HWND ShortcutSettings::s_hToolbar = nullptr;
HWND ShortcutSettings::s_hSearch = nullptr;
HWND ShortcutSettings::s_hPopover = nullptr;
HWND ShortcutSettings::s_hBtnAdd = nullptr;
HWND ShortcutSettings::s_hBtnReset = nullptr;
HWND ShortcutSettings::s_hBtnImport = nullptr;
HWND ShortcutSettings::s_hBtnExport = nullptr;
HWND ShortcutSettings::s_hBtnCancel = nullptr;
HWND ShortcutSettings::s_hBtnSave = nullptr;
HWND ShortcutSettings::s_hChkAlwaysShow = nullptr;
std::wstring ShortcutSettings::s_yamlPath;
DWORD ShortcutSettings::s_showTime = 0;
HHOOK ShortcutSettings::s_hKeyboardHook = nullptr;

int ShortcutSettings::s_capturingRow = -1;
std::wstring ShortcutSettings::s_pendingChord;

std::vector<ShortcutSettings::Hotkey> ShortcutSettings::m_hotkeys;
std::vector<ShortcutSettings::Hotkey> ShortcutSettings::m_defaults;
std::vector<ShortcutSettings::Hotkey> ShortcutSettings::m_pristine;

// T103: 搜索过滤词;T102: 状态行是否存在冲突
std::wstring ShortcutSettings::s_filterQuery;
bool ShortcutSettings::s_statusConflict = false;

int ShortcutSettings::kTableH_phys = 0;
int ShortcutSettings::kBtnY_phys = 0;

// ===== 设计常量 (FLUENT-UI-TOKENS.md §3.6 + §3.6.3) =====
namespace {

// 尺寸 (size.modal.shortcut.* + size.shortcut.*)
constexpr int kDialogW = 800;    // size.shortcut.dialog.w
constexpr int kDialogH = 680;    // size.shortcut.dialog.h
constexpr int kTitleH = 56;      // title bar (icon + title + subtitle + X)
constexpr int kToolbarH = 44;    // filter chips row
constexpr int kStatusBarH = 28;  // status row
constexpr int kBtnH = 34;
constexpr int kBtnW = 92;
constexpr int kBtnGap = 8;
constexpr int kGap = 10;
constexpr int kDlgRadius = 14;  // radius.lg

// spec 095: 「界面」分组行(快捷面板常驻勾选框所在行)
constexpr int kPrefsH = 30;
// spec 095: 「界面」分组行 y —— 工具栏行之后、状态行之前。
constexpr int kPrefsY = kTitleH + kToolbarH;

constexpr int kPopoverW = 240;  // size.shortcut.popover.w
constexpr int kPopoverH = 100;  // size.shortcut.popover.h
constexpr int kPopoverTailH = 8;
constexpr int kPopoverGap = 8;  // T111: popover 与目标行的水平间距

// 颜色 (color.modal.* + color.scheme.hotkey.*)
constexpr COLORREF kBorderColor = RGB(217, 217, 217);
constexpr COLORREF kBgTop = RGB(245, 245, 248);
constexpr COLORREF kBgBot = RGB(220, 222, 230);
constexpr COLORREF kTextColor = RGB(30, 30, 40);
constexpr COLORREF kTextSecondary = RGB(120, 120, 130);
constexpr COLORREF kTextTertiary = RGB(160, 160, 170);
constexpr COLORREF kSelBg = RGB(255, 235, 220);  // peach
constexpr COLORREF kHoverBg = RGB(245, 245, 250);
constexpr COLORREF kAccentPrimary = RGB(255, 95, 49);  // 火流猩品牌橙
constexpr COLORREF kMonospacedFg = RGB(60, 60, 80);

constexpr COLORREF kConflictBg =
    RGB(255, 218, 210);  // §3.6.3 hotkey.conflict_bg
constexpr COLORREF kWarningText =
    RGB(196, 110, 28);  // §3.6.3 hotkey.warning_text
constexpr COLORREF kSuccessText =
    RGB(36, 138, 61);  // §3.6.3 hotkey.success_text
constexpr COLORREF kUnchangedText =
    RGB(140, 140, 150);  // §3.6.3 hotkey.unchanged_text
constexpr COLORREF kChangedText =
    RGB(255, 95, 49);  // §3.6.3 hotkey.changed_text
constexpr COLORREF kDestructiveRed = RGB(208, 69, 69);  // FLUENT destructive

constexpr COLORREF kSearchBg = RGB(255, 255, 255);
constexpr COLORREF kSearchBorder = RGB(220, 220, 225);
constexpr COLORREF kRowActiveBg = RGB(255, 240, 225);
constexpr COLORREF kRowActiveStripe = RGB(255, 95, 49);

constexpr COLORREF kPopoverBg = RGB(252, 252, 254);
constexpr COLORREF kPopoverShadow = RGB(0, 0, 0);
constexpr COLORREF kPopoverStripe = RGB(255, 95, 49);

constexpr COLORREF kButtonBg = RGB(245, 245, 248);
constexpr COLORREF kButtonBgPrimary = RGB(255, 95, 49);
constexpr COLORREF kButtonFgPrimary = RGB(255, 255, 255);
constexpr COLORREF kButtonBgDanger = RGB(255, 255, 255);
constexpr COLORREF kButtonBorderDanger = RGB(208, 69, 69);
constexpr COLORREF kButtonFgDanger = RGB(208, 69, 69);

// 字体 (font.ui.*)
constexpr int kUiFontSize = 14;
constexpr int kTitleFontSize = 21;
constexpr int kSubFontSize = 13;
constexpr int kSmallFontSize = 11;
constexpr int kMonoFontSize = 14;
constexpr int kBigKeyFontSize = 16;

// 工具:小写转换(搜索用)
std::wstring ToLower(const std::wstring& s) {
  std::wstring r = s;
  CharLowerBuffW(r.data(), static_cast<DWORD>(r.size()));
  return r;
}

bool ContainsCI(const std::wstring& haystack, const std::wstring& needle) {
  if (needle.empty())
    return true;
  return ToLower(haystack).find(ToLower(needle)) != std::wstring::npos;
}

}  // namespace

// ===== Public API =====

void ShortcutSettings::SetBaselinePath(const std::wstring& path) {
  // T110: 显式注入基线文件 (单测用 fixture)。生产路径不需要调用 ——
  // 基线由 <user_data>/default.yaml 或 <WeaselServer.exe 目录>/data/default.yaml
  // 自动解析。换基线文件 ⇒ 清缓存, 下次 EnsureBaseline 重新加载。
  fluxing::shortcut_baseline::State& s =
      fluxing::shortcut_baseline::BaselineState();
  s.override_path = path;
  fluxing::shortcut_baseline::ResetBaseline();
}

void ShortcutSettings::Show() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    SetForegroundWindow(s_hwnd);
    s_showTime = GetTickCount();
    return;
  }

  // 1. 加载 YAML (default.custom.yaml -> fallback default 内置 30 条)
  // spec 045 P0: 原来这里硬编码 %APPDATA%\Rime\default.custom.yaml,而本 fork
  // 的真实用户目录是 %APPDATA%\fluxing(ADR 0020 单一事实源
  // WeaselUserDataPath())。写到 Rime 目录, librime 那边根本读不到, 保存等于
  // 没保存。改用 WeaselUserDataPath(),与 WeaselDeployer/HotkeyEditorDialog.cpp
  // 同源。
  if (s_yamlPath.empty()) {
    const std::wstring user_dir = WeaselUserDataPath().wstring();
    if (user_dir.empty()) {
      s_yamlPath = L"default.custom.yaml";
    } else {
      s_yamlPath = user_dir + L"\\default.custom.yaml";
    }
  }
  m_hotkeys.clear();
  if (!LoadHotkeys(s_yamlPath, m_hotkeys) || m_hotkeys.empty()) {
    // 解析失败 / 文件里没有 bindings 段 => fallback 到内置默认表
    // (LoadDefaults 是 schema 内的硬编码 ~14 条)。
    // spec 045 P0: 原来只判 LoadHotkeys 的返回值 —— 文件能打开但没有
    // bindings 段时会得到 0 行,表格全空,用户以为界面坏了。
    std::wcerr << L"[ShortcutSettings] WARN: cannot load " << s_yamlPath
               << L", fallback to defaults" << std::endl;
    m_hotkeys.clear();
    LoadDefaults(m_hotkeys);
  }
  // reset 用: T110 起「恢复默认 / 恢复这一行」的基准是基线原样行 (m_pristine),
  // 不是「打开时读到的表」—— 后者带着用户自己的改动。
  m_defaults = m_pristine.empty() ? m_hotkeys : m_pristine;

  // 2. 注册 ListView 控件
  INITCOMMONCONTROLSEX icc = {sizeof(icc),
                              ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
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
        std::wcerr << L"[ShortcutSettings] RegisterClassEx failed, err=" << err
                   << std::endl;
        return;
      }
    }
    s_classRegistered = true;
  }

  s_hwnd =
      CreateWindowExW(exStyle, L"FluxingShortcutSettings",
                      L"设置",  // 设置
                      style, CW_USEDEFAULT, CW_USEDEFAULT, kDialogW, kDialogH,
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
    HRGN rgn = CreateRoundRectRgn(0, 0, kDialogW, kDialogH, kDlgRadius * 2,
                                  kDlgRadius * 2);
    if (rgn)
      SetWindowRgn(s_hwnd, rgn, TRUE);
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
    KillTimer(s_hwnd, IDT_HINT_HIDE);  // T111 fix: 只读提示定时器也要收口
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
  s_hChkAlwaysShow = nullptr;
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
      {L"Control+space", L"always", L"commit_code",
       L"上屏编码"},  // 上屏编码
      {L"Escape", L"always", L"clear",
       L"清空筛选"},  // 清空候选
      {L"Return", L"always", L"commit",
       L"回车上屏"},  // 回车上屏
      {L"Control+Return", L"always", L"commit_raw",
       L"Ctrl+Enter 上屏"},  // 上屏
      {L"BackSpace", L"always", L"backspace",
       L"退格"},                                   // 退格
      {L"Delete", L"always", L"delete", L"删除"},  // 删除
      // composing
      {L"Shift+Left", L"composing", L"cursor_left",
       L"光标左移"},  // 光标左移
      {L"Shift+Right", L"composing", L"cursor_right",
       L"光标右移"},  // 光标右移
      {L"Home", L"composing", L"cursor_home",
       L"光标到首"},  // 光标到首
      {L"End", L"composing", L"cursor_end",
       L"光标到尾"},  // 光标到尾
      // has_menu
      {L"1", L"has_menu", L"select 1",
       L"选第 1 候选"},  // 选第 1 候选
      {L"2", L"has_menu", L"select 2",
       L"选第 2 候选"},
      {L"3", L"has_menu", L"select 3",
       L"选第 3 候选"},
      {L"Tab", L"has_menu", L"select_next",
       L"下个候选"},  // 下个候选
      {L"Shift+Tab", L"has_menu", L"select_prev",
       L"上个候选"},  // 上个候选
      // paging (when=paging in librime)
      {L"comma", L"paging", L"Page_Up",
       L"翻页上"},  // 翻页上
      {L"period", L"paging", L"Page_Down",
       L"翻页下"},  // 翻页下
      // toggle (ascii_mode 当 accept 是 Shift+space 等)
      {L"Shift+space", L"always", L"toggle ascii_mode",
       L"切换中英"},  // 切换中英
      {L"Control+Shift+numbersign", L"always", L"toggle ascii_punct",
       L"中英标点切换"
       L""},  // 中英标点切换
      {L"Control+Shift+dollar", L"always", L"toggle traditionalization",
       L"简繁切换"},  // 简繁切换
  };
  return kBuiltins;
}

bool ShortcutSettings::LoadDefaults(std::vector<Hotkey>& out) {
  // ~14 条 UI 默认显示
  out.clear();
  out.push_back(MakeBuiltin(
      Action_ToggleChinese, L"always", L"Shift+space",
      L"切换中英",  // 切换中英
      L"accept: \"Shift+space\" toggle: ascii_mode", L"toggle: ascii_mode"));
  out.push_back(MakeBuiltin(Action_ToggleFullHalf, L"always",
                            L"Control+Shift+F",
                            L"全/"
                            L"半角切换"
                            L"",  // 全/半角切换
                            L"accept: \"Control+Shift+F\" toggle: full_shape",
                            L"toggle: full_shape"));
  out.push_back(MakeBuiltin(
      Action_ToggleAsciiPunct, L"always", L"Control+Shift+numbersign",
      L"中英标点切换"
      L"",
      L"accept: \"Control+Shift+numbersign\" toggle: ascii_punct",
      L"toggle: ascii_punct"));
  out.push_back(MakeBuiltin(
      Action_ToggleTrad, L"always", L"Control+Shift+dollar",
      L"简繁切换",
      L"accept: \"Control+Shift+dollar\" toggle: traditionalization",
      L"toggle: traditionalization"));
  out.push_back(MakeBuiltin(
      Action_PageUp, L"paging", L"comma",
      L"翻页 - 上页",  // 翻页 - 上页
      L"when: paging accept: comma send: Page_Up", L"send: Page_Up"));
  out.push_back(MakeBuiltin(
      Action_PageDown, L"paging", L"period",
      L"翻页 - 下页",  // 翻页 - 下页
      L"when: paging accept: period send: Page_Down", L"send: Page_Down"));
  out.push_back(MakeBuiltin(
      Action_SelectN, L"has_menu", L"Control+1",
      L"选第 2 候选",  // 选第 2 候选
      L"when: has_menu accept: \"Control+1\" send: 2", L"send: 2"));
  out.push_back(
      MakeBuiltin(Action_SelectN, L"has_menu", L"Control+2",
                  L"选第 3 候选",
                  L"when: has_menu accept: \"Control+2\" send: 3", L"send: 3"));
  out.push_back(MakeBuiltin(
      Action_OpenQuickPanel, L"always", L"Alt+comma",
      L"快捷静态",  // 快捷静态
                                                            // (QuickPanel =
                                                            // Alt+,)
      L"accept: \"Alt+comma\" custom: open_quick_panel",
      L"custom: open_quick_panel"));
  out.push_back(MakeBuiltin(
      Action_OpenPhrases, L"always", L"Alt+period",
      L"常用短语",  // 常用短语
      L"accept: \"Alt+period\" custom: open_phrases", L"custom: open_phrases"));
  // v0.19.0.60 (Phase L 调整 1): Action_OpenUserDict 删除 — UserDictionary
  // 模块下线。
  out.push_back(MakeBuiltin(
      Action_SecondCandidate, L"has_menu", L"grave",
      L"第二候选",  // 第二候选
      L"when: has_menu accept: grave send: 2", L"send: 2"));
  out.push_back(MakeBuiltin(
      Action_TriggerDeploy, L"always", L"Control+F5",
      L"重新部署",  // 重新部署
      L"accept: \"Control+F5\" custom: trigger_deploy",
      L"custom: trigger_deploy"));
  return true;
}

// ===== YAML I/O (default.custom.yaml, spec 045 §5) =====

bool ShortcutSettings::LoadHotkeys(const std::wstring& path,
                                   std::vector<Hotkey>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    // file 不存在 ⇒ 面板显示「基线全局表 + 应用级默认」(T110)。
    // 旧实现 fallback 到 LoadDefaults 的 12 条硬编码 —— 那不是全局表,
    // 落盘时会被当成「完整列表」写进 patch ⇒ 覆盖掉真正的全局表。
    return MergeHotkeys(std::vector<Hotkey>(), out);
  }
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

  std::vector<Hotkey> fileRows;
  // minimal parser: 找 "patch.key_binder/bindings:" 后的 -{ ... } 行
  // 每条 binding 是 inline dict: { when: ..., accept: ..., send|toggle: ... }
  size_t pos = 0;
  bool inBindings = false;
  int bindingsDepth = 0;
  while (pos < wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos)
      eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r')
      line.pop_back();
    pos = eol + 1;

    auto trim = [](std::wstring& s) {
      size_t a = 0, b = s.size();
      while (a < b && (s[a] == L' ' || s[a] == L'\t'))
        ++a;
      while (b > a &&
             (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r'))
        --b;
      s = s.substr(a, b - a);
    };
    std::wstring trimmed = line;
    trim(trimmed);

    if (!inBindings) {
      // 探测 patch.key_binder/bindings: 行 (可能是 "key_binder/bindings:" 前缀
      // path)
      // 注意: "key_binder/bindings:" 是 20 个字符,原来这里取尾 18 个字符
      // 去比 —— 永远不相等 ⇒ 用户 yaml 的 bindings 段从来没被解析到,
      // 界面里的行全是内置默认值(spec 045 P0)。改成按 20 取尾;
      // 带前缀的 path 也覆盖("patch.key_binder/bindings:" 的尾部同样
      // 是这 20 个字符)。
      if (trimmed.size() >= 20 &&
          trimmed.substr(trimmed.size() - 20) == L"key_binder/bindings:") {
        inBindings = true;
      }
      continue;
    }
    // 已 inBindings: 找 "{ when: ..., accept: ..., ... }" 行
    if (trimmed.empty() || trimmed[0] == L'#' || trimmed[0] == L'-' ||
        trimmed[0] == L'[') {
      // '-' 开头可能是 - { ... } 但这里简化: 看 { 紧跟 - 1 步
      if (!trimmed.empty() && trimmed.size() >= 3 && trimmed[0] == L'-' &&
          trimmed[1] == L'{' && trimmed.back() == L'}') {
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
          if (colon == std::wstring::npos)
            break;
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
          } else if (!key.empty() && !val.empty()) {
            // 其余一切 "动作: 参数" (custom / key / paging / ...) 原样收下。
            // 否则本界面自己写出的 custom: 行读回来就丢了 (T106 修)。
            hk.action_yaml = key + L": " + val;
          }
        }
        if (!hk.accept.empty() && !hk.action_yaml.empty()) {
          hk.action_desc =
              L"自定义";  // 自定义 (raw fallback)
          hk.accept_new.clear();
          hk.yaml_raw = trimmed;
          fileRows.push_back(hk);
        }
      } else if (trimmed.empty()) {
        // 空行 - 继续
      }
      continue;
    }
  }
  // T110: 面板行 = 基线全局表 (default.yaml) + 应用级默认 + 用户自定义行,
  // 由 MergeHotkeys 统一合成。旧实现把文件行折进「打开时读到的表」,
  // 落盘时又只写改动行 ⇒ librime 的 patch 对 key_binder/bindings 是整表覆盖
  // ⇒ 一次保存就把全局快捷键表清成剩下的那几行 (T110 的破坏性 patch)。
  return MergeHotkeys(fileRows, out);
}

// ===== T110: 基线 (全局快捷键表) 合成 =====
//
// 面板行 = 基线行 (default.yaml 的全局表, 按原顺序) + 基线没覆盖的应用级默认行
//          + 用户文件里没被认领的自定义行。
// 落盘时写的是「完整合并列表」—— librime 的 patch 对 key_binder/bindings 是
// 整表覆盖, 只写改动行会把全局表清成剩下的那几行 (T110 的破坏性 patch)。

namespace {

// 文件路径 → 父目录 (基线优先从用户目录找)
std::wstring ParentDirOf(const std::wstring& path) {
  const size_t slash = path.find_last_of(L"\\/");
  return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

// 键位名大小写不敏感比较 (同一张表在不同文件里大小写可能不一致)
bool EqualAcceptCI(const std::wstring& a, const std::wstring& b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    wchar_t ca = a[i];
    wchar_t cb = b[i];
    if (ca >= L'A' && ca <= L'Z')
      ca = static_cast<wchar_t>(ca - L'A' + L'a');
    if (cb >= L'A' && cb <= L'Z')
      cb = static_cast<wchar_t>(cb - L'A' + L'a');
    if (ca != cb)
      return false;
  }
  return true;
}

// action_yaml → 界面描述 (基线行没有内建描述, 按动作翻)
std::wstring DescribeActionYaml(const std::wstring& ay) {
  if (ay == L"toggle: ascii_mode") return L"切换中英";
  if (ay == L"toggle: full_shape") return L"全/半角切换";
  if (ay == L"toggle: ascii_punct") return L"中英标点切换";
  if (ay == L"toggle: traditionalization") return L"简繁切换";
  if (ay == L"send: Page_Up") return L"翻页 - 上页";
  if (ay == L"send: Page_Down") return L"翻页 - 下页";
  if (ay == L"custom: open_quick_panel") return L"快捷面板";
  if (ay == L"custom: open_phrases") return L"常用短语";
  if (ay == L"custom: trigger_deploy") return L"重新部署";
  if (ay.rfind(L"send: ", 0) == 0) {
    const std::wstring n = ay.substr(5);
    bool digits = !n.empty();
    for (wchar_t c : n) {
      if (c < L'0' || c > L'9') {
        digits = false;
        break;
      }
    }
    if (digits)
      return L"选第 " + n + L" 候选";
  }
  return L"自定义";
}

// action_yaml → HotkeyAction
ShortcutSettings::HotkeyAction ActionOfActionYaml(const std::wstring& ay) {
  if (ay == L"toggle: ascii_mode")
    return ShortcutSettings::Action_ToggleChinese;
  if (ay == L"toggle: full_shape")
    return ShortcutSettings::Action_ToggleFullHalf;
  if (ay == L"toggle: ascii_punct")
    return ShortcutSettings::Action_ToggleAsciiPunct;
  if (ay == L"toggle: traditionalization")
    return ShortcutSettings::Action_ToggleTrad;
  if (ay == L"send: Page_Up")
    return ShortcutSettings::Action_PageUp;
  if (ay == L"send: Page_Down")
    return ShortcutSettings::Action_PageDown;
  if (ay == L"custom: open_quick_panel")
    return ShortcutSettings::Action_OpenQuickPanel;
  if (ay == L"custom: open_phrases")
    return ShortcutSettings::Action_OpenPhrases;
  if (ay == L"custom: trigger_deploy")
    return ShortcutSettings::Action_TriggerDeploy;
  if (ay.rfind(L"send: ", 0) == 0)
    return ShortcutSettings::Action_SelectN;
  return ShortcutSettings::Action_CustomUser;
}

// 基线原文里可能有块式写法的续行 (内含 \n) ⇒ 统一成 CRLF 再写 (L09)
std::wstring NormalizeEol(const std::wstring& s) {
  std::wstring out;
  out.reserve(s.size() + 8);
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == L'\n' && (i == 0 || s[i - 1] != L'\r'))
      out += L"\r\n";
    else
      out += s[i];
  }
  return out;
}

}  // namespace

bool ShortcutSettings::MergeHotkeys(std::vector<Hotkey> fileRows,
                                    std::vector<Hotkey>& out) {
  using namespace fluxing::shortcut_baseline;
  out.clear();
  m_pristine.clear();

  // 1) 基线 = 全局快捷键表 (default.yaml 的 key_binder/bindings)
  const std::wstring user_dir = ParentDirOf(s_yamlPath);
  const bool have_base = EnsureBaseline(user_dir);
  const std::vector<Binding>& base = BaselineState().bindings;

  // 2) 应用级默认行 (WeaselServer 自己的 RegisterHotKey 热键, 不在 librime 表里)
  std::vector<Hotkey> hardcoded;
  LoadDefaults(hardcoded);

  // 3) 认领: 把文件里的行挂到基线行上。
  //    先按 (accept, when) 精确匹配; 再按「同动作 + 同 when」匹配 (用户在界面
  //    上改键位就是这个形状), 同一动作有多条时用修饰键签名挑最像的一条。
  std::vector<bool> taken(base.size(), false);
  std::vector<int> file_to_base(fileRows.size(), -1);
  for (size_t i = 0; i < fileRows.size(); ++i) {
    const Hotkey& f = fileRows[i];
    const std::wstring fw = WhenOf(f.when);
    int hit = -1;
    for (size_t j = 0; j < base.size() && hit < 0; ++j) {
      if (taken[j] || !base[j].parsed)
        continue;
      if (f.accept == base[j].accept && fw == WhenOf(base[j].when))
        hit = static_cast<int>(j);
    }
    if (hit < 0) {
      int fallback = -1;
      for (size_t j = 0; j < base.size(); ++j) {
        if (taken[j] || !base[j].parsed)
          continue;
        if (f.action_yaml != base[j].action_yaml)
          continue;
        if (fw != WhenOf(base[j].when))
          continue;
        if (fallback < 0)
          fallback = static_cast<int>(j);
        if (ModifierSignature(f.accept) == ModifierSignature(base[j].accept)) {
          hit = static_cast<int>(j);
          break;
        }
      }
      if (hit < 0)
        hit = fallback;
    }
    if (hit >= 0) {
      taken[hit] = true;
      file_to_base[i] = hit;
    }
  }

  // 4) 基线行 (按基线顺序): 面板里没被认领的就是基线原样
  for (size_t j = 0; j < base.size(); ++j) {
    if (!base[j].parsed)
      continue;  // 解析不了的条目不进面板, 但落盘时逐字写回 (不丢行)
    Hotkey row;
    row.action = ActionOfActionYaml(base[j].action_yaml);
    row.action_desc = DescribeActionYaml(base[j].action_yaml);
    row.action_yaml = base[j].action_yaml;
    row.when = WhenOf(base[j].when);
    row.accept = base[j].accept;
    row.accept_new.clear();
    row.yaml_raw = base[j].raw;
    row.is_custom = false;
    row.is_default = true;
    row.baseline_index = static_cast<int>(j);
    out.push_back(row);
    m_pristine.push_back(row);
  }

  // 5) 文件里的行覆盖被认领的基线行 (用户改过的键位)
  for (size_t i = 0; i < fileRows.size(); ++i) {
    const int j = file_to_base[i];
    if (j < 0)
      continue;
    const Hotkey& f = fileRows[i];
    Hotkey& row = out[j];
    row.action = ActionOfActionYaml(f.action_yaml);
    row.action_desc = DescribeActionYaml(f.action_yaml);
    row.action_yaml = f.action_yaml;
    row.when = WhenOf(f.when);
    row.accept = f.accept;
    row.accept_new.clear();
    row.yaml_raw = f.yaml_raw;
    row.is_custom = true;
    row.is_default = false;
  }

  // 6) 应用级默认行: 基线已经有的不重复出现
  for (const auto& h : hardcoded) {
    bool covered = false;
    for (const auto& b : base) {
      if (!b.parsed)
        continue;
      if (b.action_yaml != h.action_yaml)
        continue;
      if (WhenOf(b.when) != WhenOf(h.when))
        continue;
      if (!EqualAcceptCI(b.accept, h.accept))
        continue;
      covered = true;
      break;
    }
    if (covered)
      continue;
    Hotkey row = h;
    row.baseline_index = -1;
    // T111: 应用级行标只读, UI 不许改键/重置。
    row.read_only = ShortcutSettings::IsAppLevelAction(row.action);
    out.push_back(row);
    m_pristine.push_back(row);
  }

  // 7) 文件里没被认领的行 ⇒ 用户自定义行, 追加在最后
  for (size_t i = 0; i < fileRows.size(); ++i) {
    if (file_to_base[i] >= 0)
      continue;
    Hotkey row = fileRows[i];
    row.action = ActionOfActionYaml(row.action_yaml);
    row.action_desc = DescribeActionYaml(row.action_yaml);
    row.when = WhenOf(row.when);
    row.accept_new.clear();
    row.is_custom = true;
    row.is_default = false;
    row.baseline_index = -1;
    // T111: 历史污染行 (曾把应用级热键写进 bindings 的遗留行) 也按应用级
    // 归一为只读 —— 否则它们在 UI 上表现为可编辑的重复行, 且会被继续写回。
    row.read_only = ShortcutSettings::IsAppLevelAction(row.action);
    out.push_back(row);
  }
  (void)have_base;
  return true;
}

// T111 (方案 A): 快捷静态 / 常用短语由 WeaselServer 自身注册, 是程序行为,
// 不是 librime 的 key_binder 键位; Action_TriggerDeploy 故意不并入 (见批次 O
// 交接卡「待核」), sync 假动作同理不在本轮范围。
bool ShortcutSettings::IsAppLevelAction(int action) {
  return action == Action_OpenQuickPanel || action == Action_OpenPhrases;
}

// ===== T106/T107: 托管块落盘 (default.custom.yaml) =====
//
// 旧实现把整张表 (含一行没改过的内置默认) 全量写进 default.custom.yaml, 并且
// trunc 重写整份文件 —— 用户在这个文件里的 schema_list / menu / switcher 等段
// 一次保存就没了; action_yaml 为空的行还会写成
// "- { when: X, accept: Y,  }" 这种无动作 binding, librime 直接忽略。
//
// 现在只写「真正需要 override 的行」, 并且只动文件里的托管块。
//
// T107 (正文级真合并): 落盘按旧文件的形态分三条路 ——
//   A 没有顶层 `patch:`   ⇒ 追加 patch: + 托管子块 (T106 行为);
//   B 有顶层 `patch:` 段  ⇒ 把托管子块插进该段内部, 块外内容逐字节保留;
//   C 段里有手写 key_binder 或 patch 是内联写法 ⇒ 明确拒绝, 文案说清原因。
// 托管子块本身不含 patch:, A/B 共用同一份块体, 因此永远不会写出第二个 patch:。

namespace {

// 每次落盘的账目, 供 SaveAndDeploy 拼状态文案 —— UI 不对自己做的事说谎,
// 静默跳过和静默失败一样不可接受 (spec 045 P0 教训, 见 SaveAndDeploy)。
struct SaveStats {
  int written = 0;            // 写进 bindings 的行数
  int skipped_no_action = 0;  // 改过但没有 YAML 动作 ⇒ 写出来是坏 binding
  bool backed_up = false;     // 首次接管前备份了旧文件
  // T107: 拒绝写入的两条真原因分开记账。用户看到的必须是「为什么没写」,
  // 不是从前那句泛泛的「已有 patch 段」—— 那段现在本来就能写了。
  bool refused = false;                      // 任一拒绝路径
  bool refused_has_manual_bindings = false;  // patch 段里有用户手写的 key_binder
  bool refused_inline_patch = false;         // patch: {…} 内联映射, 文本层插不进
  // T110: 读不到全局快捷键表基线 (default.yaml) ⇒ 不写。宁可不写, 也不能
  // 写出一份「不完整列表」把全局表覆盖掉。
  bool refused_no_baseline = false;
  // T110: 有效列表与基线完全一致 ⇒ 没有可写的 bindings (不碰全局表)。
  bool no_change = false;
};
SaveStats g_saveStats;

// 托管块标记: 纯 ASCII, 便于按字节定位与替换。
const char kManagedBegin[] =
    "# >>> fluxing-shortcuts (spec 045) managed block - do not edit";
const char kManagedEnd[] = "# <<< fluxing-shortcuts (spec 045) end";

// ---- T107: 行级模型 ----
// 保留每行原有行尾: 混行尾的 YAML 合法, 一次保存不该顺手整份改写用户的文件。
struct Line {
  std::string text;  // 不含行尾
  std::string eol;   // "\r\n" / "\n" / "" (末行没有换行)
};

std::vector<Line> SplitLines(const std::string& utf8) {
  std::vector<Line> out;
  size_t pos = 0;
  while (pos < utf8.size()) {
    const size_t eol = utf8.find('\n', pos);
    Line ln;
    if (eol == std::string::npos) {
      ln.text = utf8.substr(pos);
      out.push_back(ln);
      break;
    }
    ln.text = utf8.substr(pos, eol - pos);
    if (!ln.text.empty() && ln.text.back() == '\r') {
      ln.text.pop_back();
      ln.eol = "\r\n";
    } else {
      ln.eol = "\n";
    }
    out.push_back(ln);
    pos = eol + 1;
  }
  return out;
}

std::string JoinLines(const std::vector<Line>& lines) {
  std::string out;
  for (const auto& ln : lines) {
    out += ln.text;
    out += ln.eol.empty() ? "\r\n" : ln.eol;
  }
  return out;
}

std::string TrimAscii(const std::string& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r'))
    ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
    --b;
  return s.substr(a, b - a);
}

// 摘掉历史托管块 —— 包括 T106 那种「追加在文件尾、块内自带 patch:」的形态。
// 标记行按「去掉缩进后文本相等」匹配, 缩进怎么写都能自愈; 块体整段丢弃,
// 所以重写后既不会留孤儿缩进键, 也不会出现第二个顶层 patch:。
std::vector<Line> RemoveManagedBlock(const std::vector<Line>& lines) {
  std::vector<Line> out;
  bool in_block = false;
  for (const auto& ln : lines) {
    const std::string t = TrimAscii(ln.text);
    if (!in_block && t == kManagedBegin) {
      in_block = true;
      continue;
    }
    if (in_block) {
      if (t == kManagedEnd)
        in_block = false;
      continue;
    }
    out.push_back(ln);
  }
  return out;
}

// 顶层 `patch:` 行下标 (行首无缩进、非注释); 没有返回 -1。
// inlineValue = true 表示 `patch:` 后面还跟着内容 (`patch: {…}`): 那是内联映射,
// 文本层没法往里插子键 ⇒ 由调用方拒绝, 不猜。
int FindTopLevelPatch(const std::vector<Line>& lines, bool& inlineValue) {
  inlineValue = false;
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& s = lines[i].text;
    if (s.empty() || s[0] == ' ' || s[0] == '\t' || s[0] == '#')
      continue;
    if (s.compare(0, 6, "patch:") != 0)
      continue;
    const std::string rest = TrimAscii(s.substr(6));
    if (!rest.empty() && rest[0] != '#')
      inlineValue = true;
    return static_cast<int>(i);
  }
  return -1;
}

// patch 段范围: patch 行之后, 到「第一个行首无缩进且非空非注释」的行之前。
// 顶层注释不算段尾 —— 托管块自己的标记行就是列 0 注释。
size_t PatchSectionEnd(const std::vector<Line>& lines, size_t patchIdx) {
  for (size_t i = patchIdx + 1; i < lines.size(); ++i) {
    const std::string& s = lines[i].text;
    if (s.empty() || s[0] == ' ' || s[0] == '\t' || s[0] == '#')
      continue;
    return i;
  }
  return lines.size();
}

// patch 段里已有用户手写的 key_binder 配置 (整段 `key_binder:` 或裸 `bindings:`)。
// 再插一个 key_binder/bindings: 就是同一 mapping 里的重复键 ⇒ librime 读错甚至
// 整份失效。要让「手工项与托管项共存」得先定语义 (handoff §3.2 边界), 只拒绝。
bool HasManualKeyBinder(const std::vector<Line>& lines,
                        size_t patchIdx,
                        size_t end) {
  for (size_t i = patchIdx + 1; i < end; ++i) {
    const std::string t = TrimAscii(lines[i].text);
    if (t.empty() || t[0] == '#')
      continue;
    if (t.compare(0, 10, "key_binder") == 0)
      return true;
    if (t == "bindings:")
      return true;
  }
  return false;
}

// 托管子块: 标记 + 中文注释 + (bindings 子块 或 空说明), 行尾 CRLF (L09)。
// **不含 `patch:`** —— B 形态要把它插进用户已有的 patch: 段内部; A 形态与导出
// 才由调用方补 patch:。返回结构体而不是顺手改 g_saveStats, 是为了让这块保持
// 纯函数、结果可断言 (constitution R7: 生产与单测共用同一实现)。
struct ManagedBlock {
  std::string text;
  int rows = 0;
  int skipped_no_action = 0;
  // T110: 这次落盘到底写不写 key_binder/bindings。false = 有效列表与基线
  // 完全一致 (或没有可写的行) ⇒ 只留注释, 不碰全局快捷键表。
  bool writes_bindings = false;
};

ManagedBlock BuildManagedSubBlockV2(
    const std::vector<ShortcutSettings::Hotkey>& data) {
  using namespace fluxing::shortcut_baseline;
  ManagedBlock block;
  auto appendAscii = [&block](const char* s) {
    block.text += s;
    block.text += "\r\n";
  };

  auto appendLine = [&block](const std::wstring& wline) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                                  static_cast<int>(wline.size()), nullptr, 0,
                                  nullptr, nullptr);
    if (len > 0) {
      std::string buf(len, 0);
      WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                          static_cast<int>(wline.size()), &buf[0], len, nullptr,
                          nullptr);
      block.text += buf;
    }
    block.text += "\r\n";
  };

  appendAscii(kManagedBegin);
  appendLine(L"# 本段由「快捷键设置」托管, 手工修改会被下次保存覆盖。");
  appendLine(L"# 生成者: spec 045 ShortcutSettings (v0.19.0.28)");

  // T110: 写的是「完整合并列表」—— 基线全表 (未改的行逐字保留) + 用户的改动
  // 行 + 用户自定义行。librime 的 patch 对 key_binder/bindings 是整表覆盖,
  // 少写一行就是丢一行 (T110 的破坏性 patch)。旧实现只写改动行 ⇒ 全局表
  // 被覆盖成剩下的那几行。
  const State& bs = BaselineState();
  const bool have_base = bs.ok;
  const std::vector<Binding>& base = bs.bindings;

  // 面板行按 baseline_index 建索引 (基线行 → 面板行)
  std::map<int, const ShortcutSettings::Hotkey*> byIndex;
  for (const auto& h : data) {
    if (h.baseline_index >= 0 && h.baseline_index < (int)base.size())
      byIndex[h.baseline_index] = &h;
  }

  std::vector<std::wstring> rows;
  bool differs = false;

  // 1) 基线行: 面板里没有的照抄原文; 改过的重渲染; 解析不了的也照抄 (不丢行)
  for (size_t j = 0; j < base.size(); ++j) {
    const Binding& b = base[j];
    auto it = byIndex.find(static_cast<int>(j));
    if (it == byIndex.end() || !b.parsed) {
      rows.push_back(L"    " + b.raw);
      continue;
    }
    const ShortcutSettings::Hotkey& h = *it->second;
    const std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    const bool same = (eff == b.accept) && (h.action_yaml == b.action_yaml) &&
                      (WhenOf(h.when) == WhenOf(b.when));
    if (same) {
      rows.push_back(L"    " + b.raw);
      continue;
    }
    const std::wstring r = RenderRow(eff, h.when, h.action_yaml);
    if (r.empty()) {
      // 渲染不出来 (缺键或缺动作) ⇒ 退回基线原文, 绝不丢行
      ++block.skipped_no_action;
      rows.push_back(L"    " + b.raw);
      differs = true;
      continue;
    }
    rows.push_back(r);
    differs = true;
  }

  // 2) 不属于基线的行: 应用级热键没改就不进 librime 的表; 用户自定义行写
  for (const auto& h : data) {
    if (h.baseline_index >= 0 && h.baseline_index < (int)base.size())
      continue;
    // T111 (方案 A): 应用级热键 (快捷静态 / 常用短语) 由程序注册, 永不写进
    // 托管块 —— 含历史污染行与用户改过键的应用级行, 下次保存即自愈清污。
    if (ShortcutSettings::IsAppLevelAction(h.action))
      continue;
    const bool edited = !h.accept_new.empty() && h.accept_new != h.accept;
    if (!h.is_custom && !edited)
      continue;
    const std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    if (eff.empty() || h.action_yaml.empty()) {
      ++block.skipped_no_action;
      continue;
    }
    const std::wstring r = RenderRow(eff, h.when, h.action_yaml);
    if (r.empty()) {
      ++block.skipped_no_action;
      continue;
    }
    rows.push_back(r);
    differs = true;
  }

  // 没有基线时 rows 里只有用户自定义行 ⇒ 写下去等于把全局表清成那几行,
  // 由调用方 (SaveHotkeys) 拒绝。
  block.writes_bindings = have_base ? differs : !rows.empty();
  block.rows = block.writes_bindings ? static_cast<int>(rows.size()) : 0;

  if (!block.writes_bindings) {
    // 不写空的 bindings: —— patch 里的空 bindings 会把用户已配的 key_binder
    // 覆盖成空。这里只留注释, 让旧块被清掉的同时不改变任何语义。
    appendLine(L"  # 当前没有需要落盘的改动 (不写空 bindings, 避免清空 key_binder)。");
  } else {
    appendLine(L"  key_binder/bindings:");
    for (const auto& r : rows)
      appendLine(r);
  }
  block.rows = block.writes_bindings ? static_cast<int>(rows.size()) : 0;
  appendAscii(kManagedEnd);
  return block;
}

// 首次接管前整份备份。备份失败就不写 —— 没有退路不该覆盖用户配置。
bool BackupFile(const std::wstring& path) {
  SYSTEMTIME st;
  GetLocalTime(&st);
  auto two = [](int v) {
    return (v < 10 ? std::wstring(L"0") : std::wstring()) + std::to_wstring(v);
  };
  const std::wstring dst = path + L".bak-" + std::to_wstring(st.wYear) +
                           two(st.wMonth) + two(st.wDay) + L"-" +
                           two(st.wHour) + two(st.wMinute) + two(st.wSecond);
  return CopyFileW(path.c_str(), dst.c_str(), FALSE) != FALSE;
}

}  // namespace

bool ShortcutSettings::SaveHotkeys(const std::wstring& path,
                                   const std::vector<Hotkey>& data,
                                   bool overwrite_file) {
  g_saveStats = SaveStats();

  // T110: 落盘写的是「完整合并列表」⇒ 必须先拿到全局快捷键表基线。
  // 面板路径 (LoadHotkeys → MergeHotkeys) 已经加载过基线; 直接调用 SaveHotkeys
  // 的路径 (单测 / 导出) 不能因为「没走加载」就被误判成「没有基线」。
  fluxing::shortcut_baseline::EnsureBaseline(ParentDirOf(path));

  // ---- 1. 读旧文件 (T106: 只替换托管块, 块外内容原样保留) ----
  std::string existing;
  {
    std::ifstream in(path, std::ios::binary);
    if (in) {
      existing.assign(std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>());
    }
  }
  if (existing.size() >= 3 && (unsigned char)existing[0] == 0xEF &&
      (unsigned char)existing[1] == 0xBB &&
      (unsigned char)existing[2] == 0xBF) {
    existing = existing.substr(3);
  }

  // ---- 2. 组织托管子块 (只含 key_binder/bindings, 不含 patch:) ----
  const ManagedBlock block = BuildManagedSubBlockV2(data);
  g_saveStats.written = block.rows;
  g_saveStats.skipped_no_action = block.skipped_no_action;
  g_saveStats.no_change = !block.writes_bindings;

  // T110 fail-safe: 要写 bindings 却读不到基线 ⇒ 拒绝写。
  // librime 的 patch 对 key_binder/bindings 是整表覆盖, 没有基线就拼不出
  // 完整列表, 写下去等于把全局快捷键表清成剩下那几行。
  if (block.writes_bindings &&
      !fluxing::shortcut_baseline::BaselineState().ok) {
    g_saveStats.refused = true;
    g_saveStats.refused_no_baseline = true;
    return false;
  }

  // ---- 3. 组装 ----
  // 形态判定全部在写盘之前完成: 任一拒绝路径都不碰文件。
  std::string out;
  if (overwrite_file) {
    // 导出: 整份文件就是托管块, 所以要自带顶层 patch:
    out = block.rows > 0 ? ("patch:\r\n" + block.text) : block.text;
  } else {
    // 先摘掉历史托管块 (T106 的形态是「追加在文件尾、块内含 patch:」), 再在
    // 剩下的用户正文上判形态 —— 自愈不会留下第二个顶层 patch: 或孤儿缩进键。
    const bool had_block = existing.find(kManagedBegin) != std::string::npos;
    std::vector<Line> lines = RemoveManagedBlock(SplitLines(existing));

    bool inline_patch = false;
    const int patch_idx = FindTopLevelPatch(lines, inline_patch);

    if (patch_idx >= 0 && inline_patch) {
      // `patch: {…}` 是内联映射: 文本层插不进去, 也不该猜。
      g_saveStats.refused = true;
      g_saveStats.refused_inline_patch = true;
      return false;
    }

    if (patch_idx >= 0) {
      const size_t patch_end =
          PatchSectionEnd(lines, static_cast<size_t>(patch_idx));
      if (HasManualKeyBinder(lines, static_cast<size_t>(patch_idx), patch_end)) {
        // 用户自己在 patch 段里手写了 key_binder: 再插一个同名键就是 YAML
        // 重复键, librime 会读错或整份失效。放开这条属于 §3.2 边界, 要用户拍板。
        g_saveStats.refused = true;
        g_saveStats.refused_has_manual_bindings = true;
        return false;
      }
    }

    if (!had_block && !existing.empty()) {
      if (!BackupFile(path))
        return false;  // 没有退路就不覆盖用户配置
      g_saveStats.backed_up = true;
    }

    if (patch_idx >= 0) {
      // B 形态 (T107): 插到用户 patch: 段的最前面, 即 patch 行的下一行。
      // 块外内容 (schema_list / menu / …) 逐字节原样保留。
      std::vector<Line> sub = SplitLines(block.text);
      lines.insert(lines.begin() + patch_idx + 1, sub.begin(), sub.end());
      out = JoinLines(lines);
    } else {
      // A 形态: 文件里没有顶层 patch: ⇒ 追加一个 (T106 行为)。
      out = JoinLines(lines);
      if (!out.empty() && out.back() != '\n')
        out += "\r\n";
      if (block.rows > 0)
        out += "patch:\r\n";
      out += block.text;
    }
  }

  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
  const char bom[3] = {'\xEF', '\xBB', '\xBF'};
  f.write(bom, 3);
  f.write(out.c_str(), static_cast<std::streamsize>(out.size()));
  return f.good();
}

// ===== Chord capture (shared unit, ADR 0011 D1) =====

namespace {

// The keyboard snapshot the shared unit expects. The low level hook only
// knows which modifiers are held down, but the unit wants the same shape of
// snapshot the front end builds with GetKeyboardState(): the key that was
// pressed plus the modifiers that were down with it.
void BuildHookKeyState(const KBDLLHOOKSTRUCT* ks,
                       bool ctrl,
                       bool shift,
                       bool alt,
                       bool sup,
                       BYTE* state) {
  state[ks->vkCode] = 0x80;
  if (shift) {
    state[VK_SHIFT] = 0x80;
    state[VK_LSHIFT] = 0x80;
    state[VK_RSHIFT] = 0x80;
  }
  if (ctrl) {
    state[VK_CONTROL] = 0x80;
    state[VK_LCONTROL] = 0x80;
    state[VK_RCONTROL] = 0x80;
  }
  if (alt) {
    state[VK_MENU] = 0x80;
    state[VK_LMENU] = 0x80;
    state[VK_RMENU] = 0x80;
  }
  if (sup) {
    state[VK_LWIN] = 0x80;
    state[VK_RWIN] = 0x80;
  }
}

// Resolve the chord the hook saw the way the front end resolves it, so that
// the pattern written into the yaml is one librime can really match
// (ADR 0011 D1/D3). A chord that cannot be written down is refused, never
// rewritten.
bool CaptureHookChord(const KBDLLHOOKSTRUCT* ks,
                      bool ctrl,
                      bool shift,
                      bool alt,
                      bool sup,
                      fluxing::CapturedAccept* out) {
  BYTE state[256] = {};
  BuildHookKeyState(ks, ctrl, shift, alt, sup, state);
  const DWORD extended = (ks->flags & LLKHF_EXTENDED) ? 1u : 0u;
  const LPARAM lparam =
      static_cast<LPARAM>((static_cast<DWORD>(ks->scanCode) << 16) | extended);
  *out = fluxing::CaptureAccept(ks->vkCode, KeyInfo(lparam), state,
                                GetKeyboardLayout(0));
  return out->ok;
}

}  // namespace

// ===== Conflict detection =====

ShortcutSettings::ConflictReport ShortcutSettings::CheckConflict(
    const std::wstring& combo,
    const std::wstring& when,
    int exclude_row) {
  ConflictReport r;
  if (combo.empty())
    return r;

  // 1) L18/L19 防御: Shift_L/R 单键 阻止 (硬错误)
  if (combo == L"Shift_L" || combo == L"Shift_R" || combo == L"Shift+Shift_L" ||
      combo == L"Shift+Shift_R") {
    r.score = 2;
    r.l18_l19_error = true;
    r.warning_action =
        L"此组合不被允"
        L"许 (Shift_L/R 单键)";  // 此组合不被允许
                                                                   // (Shift_L/R
                                                                   // 单键)
    return r;
  }

  // 2) 同表 cross-row duplicate
  for (int i = 0; i < static_cast<int>(m_hotkeys.size()); ++i) {
    if (i == exclude_row)
      continue;
    const auto& h = m_hotkeys[i];
    std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    if (fluxing::AcceptEquals(eff, combo) && h.when == when) {
      r.score = 2;
      r.duplicate_error = true;
      r.warning_action = L"与 \"" + h.action_desc +
                         L"\" 冲突";  // 与「…」冲突
      return r;
    }
  }

  // 3) 内置 binding override 警告 (允许 override,只是警告)
  for (const auto& b : GetBuiltinBindings()) {
    if (fluxing::AcceptEquals(b.combo, combo) &&
        (b.when == when || when == L"always" || b.when == when)) {
      if (r.score < 1) {
        r.score = 1;
        r.warning_action =
            L"覆盖默认行为"
            L" \"" +
            b.desc_zh + L"\"";  // 覆盖默认行为「…」
      }
      break;
    }
  }
  return r;
}

// ===== Table populate / update =====

void ShortcutSettings::PopulateTable() {
  if (!s_hTable || !IsWindow(s_hTable))
    return;
  ListView_DeleteAllItems(s_hTable);

  // T103: 搜索过滤在这里落地 —— 不匹配的行根本不进列表(LVS_REPORT 不支持
  // 隐藏单项;SetItemState(LVIS_CUT) 只是把文字画淡,行还在,等于没过滤)。
  // 同时 lParam 改存"数据下标",显示行号与 m_hotkeys 下标从此解耦 ⇒ 过滤
  // 生效后点第三行不会误改第一条快捷键。
  int row = 0;
  for (int i = 0; i < static_cast<int>(m_hotkeys.size()); ++i) {
    const auto& h = m_hotkeys[i];
    std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
    if (!s_filterQuery.empty() && !ContainsCI(h.action_desc, s_filterQuery) &&
        !ContainsCI(eff, s_filterQuery) && !ContainsCI(h.when, s_filterQuery) &&
        !ContainsCI(h.action_yaml, s_filterQuery)) {
      continue;
    }
    LVITEMW li = {};
    li.mask = LVIF_TEXT | LVIF_PARAM;
    li.iItem = row;
    li.iSubItem = 0;
    li.pszText = const_cast<LPWSTR>(h.action_desc.c_str());
    li.lParam = static_cast<LPARAM>(i);  // 数据下标,不是显示行号
    ListView_InsertItem(s_hTable, &li);

    ListView_SetItemText(s_hTable, row, 1, const_cast<LPWSTR>(eff.c_str()));
    // New 列 (col 2): 用户编辑后显示;否则显示空(绿对勾),或 "未改"
    if (!h.accept_new.empty() && h.accept_new != h.accept) {
      ListView_SetItemText(s_hTable, row, 2,
                           const_cast<LPWSTR>(h.accept_new.c_str()));
    } else {
      ListView_SetItemText(s_hTable, row, 2, const_cast<wchar_t*>(L""));
    }
    ++row;
  }
  // T102: 状态行收口 —— 所有改动 in-memory 状态的路径都经过这里刷新
  RefreshStatus();
}

// T103: 显示行 → m_hotkeys 数据下标。commctrl.h 既无 ListView_GetItemData 宏，
// 也无 LVM_GETITEMDATA 常量（SDK 10.0.26100 核对：整棵 Include 树 0 命中，
// 该消息位被跳过），故走 ListView_GetItem + LVIF_PARAM 读 lParam。
static int TableItemData(HWND hTable, int item) {
  LVITEMW lvi = {};
  lvi.mask = LVIF_PARAM;
  lvi.iItem = item;
  lvi.iSubItem = 0;
  if (!ListView_GetItem(hTable, &lvi))
    return -1;
  return static_cast<int>(lvi.lParam);
}

void ShortcutSettings::UpdateRow(int dataRow) {
  if (!s_hTable || !IsWindow(s_hTable))
    return;
  if (dataRow < 0 || dataRow >= static_cast<int>(m_hotkeys.size()))
    return;
  // T103: 显示行号 != 数据下标,必须按 lParam 反查
  const int count = ListView_GetItemCount(s_hTable);
  int item = -1;
  for (int i = 0; i < count; ++i) {
    if (TableItemData(s_hTable, i) == dataRow) {
      item = i;
      break;
    }
  }
  if (item < 0) {  // 被过滤掉 / 尚未填充
    PopulateTable();
    return;
  }
  const auto& h = m_hotkeys[dataRow];
  std::wstring eff = h.accept_new.empty() ? h.accept : h.accept_new;
  ListView_SetItemText(s_hTable, item, 1, const_cast<LPWSTR>(eff.c_str()));
  if (!h.accept_new.empty() && h.accept_new != h.accept) {
    ListView_SetItemText(s_hTable, item, 2,
                         const_cast<LPWSTR>(h.accept_new.c_str()));
  } else {
    ListView_SetItemText(s_hTable, item, 2, const_cast<wchar_t*>(L""));
  }
  RefreshStatus();
}

// ===== Search filter =====

void ShortcutSettings::ApplySearchFilter(const std::wstring& q) {
  // T103: 原实现把 match 算完就丢,再无条件 PopulateTable ⇒ 输入任何关键词
  // 列表都不变(假过滤)。现在过滤词进 s_filterQuery,由 PopulateTable 真正
  // 按匹配重建列表。
  s_filterQuery = q;
  PopulateTable();
  RepaintLayered(s_hwnd);
}

// ===== Capturing =====

void ShortcutSettings::EnterCapturing(int row) {
  if (row < 0 || row >= static_cast<int>(m_hotkeys.size()))
    return;
  s_capturingRow = row;
  s_pendingChord.clear();
  // T111 fix: 进入捕获态前, 撤掉可能还在跑的只读提示隐藏定时器,
  // 否则 3s 后它会把**捕获 popover** 当成提示窗销毁。
  if (s_hwnd && IsWindow(s_hwnd))
    KillTimer(s_hwnd, IDT_HINT_HIDE);
  // T111 fix: 若残留只读提示 popover, 重建捕获 popover, 避免复用旧文案窗口
  if (s_hPopover && IsWindow(s_hPopover)) {
    DestroyWindow(s_hPopover);
    s_hPopover = nullptr;
  }

  // 启动 WH_KEYBOARD_LL hook (本进程线程范围)
  if (!s_hKeyboardHook) {
    s_hKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, &LowLevelKeyboardProc,
                                        GetModuleHandle(nullptr), 0);
  }
  SetTimer(s_hwnd, IDT_BLINK, kCaptureBlinkMs, nullptr);
  // 显示 popover
  EnsurePopover();
  // T111 fix: 创建完必须定位到"正在编辑的那一行"旁边, 否则提示出现在屏幕外。
  PositionPopover(DisplayItemForRow(row));
  SetTimer(s_hwnd, IDT_POPOVER, kPopoverShowMs, nullptr);
  SetPopoverHint(L"请按快捷键…（Esc 还原 / Backspace 清空）");
  PopulateTable();  // refresh (no special UI marking - lParam remains, captured
                    // row handled in repaint)
  RepaintLayered(s_hwnd);
}

void ShortcutSettings::ExitCapturing(bool save) {
  if (s_capturingRow < 0)
    return;
  if (save) {
    if (s_capturingRow >= 0 &&
        s_capturingRow < static_cast<int>(m_hotkeys.size())) {
      // T101: 硬错误拦截 (design §4.5 / §6.3) —— Shift_L / Shift_R 单键
      // 写进 accept 会破坏 librime 的 Shift 语义, 命中时报错并**留在捕获态**,
      // 让用户重按或 Esc 还原, 绝不写进 accept_new。
      const ConflictReport hard = CheckConflict(
          s_pendingChord, m_hotkeys[s_capturingRow].when, s_capturingRow);
      if (hard.score >= 2 && hard.l18_l19_error) {
        SetPopoverHint(ConflictHintText(hard));
        RefreshStatus();
        RepaintLayered(s_hwnd);
        return;
      }
      // 把 s_pendingChord 写入 m_hotkeys[s_capturingRow].accept_new
      m_hotkeys[s_capturingRow].accept_new = s_pendingChord;
    }
  } else {
    // 还原:accept_new 保持原状(= 原 accept 表示未改)
  }
  s_capturingRow = -1;
  s_pendingChord.clear();

  KillTimer(s_hwnd, IDT_BLINK);
  KillTimer(s_hwnd, IDT_POPOVER);
  KillTimer(s_hwnd, IDT_HINT_HIDE);
  if (s_hKeyboardHook) {
    UnhookWindowsHookEx(s_hKeyboardHook);
    s_hKeyboardHook = nullptr;
  }
  DestroyPopover();
  PopulateTable();
  RepaintLayered(s_hwnd);
}

void ShortcutSettings::ResetCapturingRow() {
  if (s_capturingRow < 0)
    return;
  if (s_capturingRow < static_cast<int>(m_hotkeys.size())) {
    m_hotkeys[s_capturingRow].accept_new.clear();
  }
  s_pendingChord.clear();
  PopulateTable();
  RepaintLayered(s_hwnd);
}

// ===== Low-level keyboard hook =====

// 捕获提示: popover + 状态行 (抓到的组合 / 拒绝原因)
void ShortcutSettings::SetCaptureHint(const wchar_t* text) {
  if (!text)
    return;
  // T101: 只写 popover。状态行现在由 RefreshStatus() 独占持有
  // ("● 未保存 (N 修改)" + 冲突计数),不再被一次性提示覆盖。
  SetPopoverHint(text);
}

// T111 fix: popover 窗口原本只在 EnterCapturing 创建。只读行永远进不了捕获态,
// 点击后的 SetPopoverHint 因窗口不存在而静默 no-op —— 提示文案从未显示。
// 把创建逻辑抽成 EnsurePopover(), 捕获路径与只读提示路径共用。
// T111 fix: 兜底清扫 —— s_hPopover 指针一旦丢失(对话框重建 / Hide 批量置 null)
// 而旧提示窗仍在, 就会累积成不可见也关不掉的残留顶层窗。创建前把**本对话框拥有的、
// 尺寸正好等于 popover** 的 Static 全部销毁(只碰自己造的窗口, 不动别的控件)。
static BOOL CALLBACK SweepStalePopover(HWND h, LPARAM owner) {
  if (GetWindow(h, GW_OWNER) != reinterpret_cast<HWND>(owner))
    return TRUE;
  wchar_t cls[32] = {};
  if (GetClassNameW(h, cls, 32) == 0 || lstrcmpiW(cls, L"Static") != 0)
    return TRUE;
  RECT r;
  GetWindowRect(h, &r);
  if (r.right - r.left == kPopoverW &&
      r.bottom - r.top == kPopoverH + kPopoverTailH)
    DestroyWindow(h);
  return TRUE;
}

ShortcutSettings::PopoverPlace ShortcutSettings::ComputePopoverPlace(
    RECT rowScreen, RECT work, int pw, int ph, int gap) {
  PopoverPlace p;
  const int rowCy = rowScreen.top + (rowScreen.bottom - rowScreen.top) / 2;
  // 默认贴行右侧; 右侧放不下(或根本没给行宽)就翻到行左侧。
  int x = rowScreen.right + gap;
  if (x + pw > work.right)
    x = rowScreen.left - gap - pw;
  // 仍越界(例如整屏宽度都不够)则夹进工作区, 保证窗口至少可见。
  if (x + pw > work.right)
    x = work.right - pw;
  if (x < work.left)
    x = work.left;
  int y = rowCy - ph / 2;
  if (y + ph > work.bottom)
    y = work.bottom - ph;
  if (y < work.top)
    y = work.top;
  p.x = x;
  p.y = y;
  return p;
}

void ShortcutSettings::EnsurePopover() {
  if (s_hPopover && IsWindow(s_hPopover)) {
    SetWindowPos(s_hPopover, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return;
  }
  if (s_hwnd && IsWindow(s_hwnd))
    EnumWindows(&SweepStalePopover, reinterpret_cast<LPARAM>(s_hwnd));
  s_hPopover = nullptr;
  // T111 fix: 先**不带 WS_VISIBLE** 创建 —— 未定位就显示会在错误位置闪一帧。
  // 位置由随后的 PositionPopover() 定, 定完才 ShowWindow。
  s_hPopover =
      CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"STATIC",
                      L"请按快捷键"
                      L"...",  // 请按快捷键...
                      WS_POPUP, 0, 0, kPopoverW, kPopoverH + kPopoverTailH,
                      s_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

// T111 fix: 把 popover 摆到目标行旁边并显示。
void ShortcutSettings::PositionPopover(int displayItem) {
  if (!s_hPopover || !IsWindow(s_hPopover))
    return;
  RECT rr = {};
  BOOL screenCoords = FALSE;
  if (s_hTable && IsWindow(s_hTable)) {
    if (displayItem >= 0 &&
        SendMessageW(s_hTable, LVM_GETITEMRECT, static_cast<WPARAM>(displayItem),
                     reinterpret_cast<LPARAM>(&rr)) == TRUE) {
      ClientToScreen(s_hTable, reinterpret_cast<POINT*>(&rr.left));
      ClientToScreen(s_hTable, reinterpret_cast<POINT*>(&rr.right));
      screenCoords = TRUE;
    } else {
      // 空白处 / 取不到行矩形: 退回整个列表区右侧, 垂直居中。
      GetWindowRect(s_hTable, &rr);
      screenCoords = TRUE;
    }
  } else {
    GetWindowRect(s_hwnd, &rr);
    screenCoords = rr.left != 0 || rr.top != 0 || rr.right != 0 ||
                   rr.bottom != 0;
  }
  if (!screenCoords)
    return;
  const HMONITOR mon = MonitorFromRect(&rr, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  const PopoverPlace p = ComputePopoverPlace(
      rr, mi.rcWork, kPopoverW, kPopoverH + kPopoverTailH, kPopoverGap);
  SetWindowPos(s_hPopover, HWND_TOPMOST, p.x, p.y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void ShortcutSettings::DestroyPopover() {
  if (s_hPopover && IsWindow(s_hPopover))
    DestroyWindow(s_hPopover);
  s_hPopover = nullptr;
}

// T103: 显示行号 → 数据下标已有 TableItemData(); 这里反查(行数 <= 几十, 直接扫)。
int ShortcutSettings::DisplayItemForRow(int row) {
  if (!s_hTable || !IsWindow(s_hTable) || row < 0)
    return -1;
  const int n = static_cast<int>(
      SendMessageW(s_hTable, LVM_GETITEMCOUNT, 0, 0));
  for (int i = 0; i < n; ++i) {
    if (TableItemData(s_hTable, i) == row)
      return i;
  }
  return -1;
}

void ShortcutSettings::SetPopoverHint(const std::wstring& text) {
  if (s_hPopover && IsWindow(s_hPopover))
    SetWindowTextW(s_hPopover, text.c_str());
}

// T101: design §4.5 的四档中文文案
std::wstring ShortcutSettings::ConflictHintText(const ConflictReport& r) {
  if (r.l18_l19_error)
    return L"✗ 此组合不被允许 (Shift_L/R 单键)";
  if (r.duplicate_error)
    return L"⚠ " + r.warning_action + L"，仍可保存";
  if (r.score >= 1)
    return L"⚠ " + r.warning_action;
  return L"✓ 无冲突";
}

// T101: 捕获过程中的实时冲突检测 (hook 路径 —— 只做内存比较, 无 I/O)
void ShortcutSettings::UpdateCaptureConflict() {
  if (s_capturingRow < 0 ||
      s_capturingRow >= static_cast<int>(m_hotkeys.size())) {
    return;
  }
  if (s_pendingChord.empty())
    return;
  const ConflictReport r = CheckConflict(
      s_pendingChord, m_hotkeys[s_capturingRow].when, s_capturingRow);
  SetPopoverHint(L"⌨ " + s_pendingChord + L" · " + ConflictHintText(r));
  RefreshStatus();
  RepaintLayered(s_hwnd);
}

// T102: 状态行统一刷新
void ShortcutSettings::RefreshStatus() {
  if (!s_hStatus || !IsWindow(s_hStatus))
    return;
  int modified = 0;
  int conflicts = 0;
  for (int i = 0; i < static_cast<int>(m_hotkeys.size()); ++i) {
    const auto& h = m_hotkeys[i];
    const bool changed = !h.accept_new.empty() && h.accept_new != h.accept;
    if (changed)
      ++modified;
    const bool capturing = (i == s_capturingRow);
    // 未改动的行不参与冲突统计 —— 否则一开面板就报一片"覆盖默认行为"
    if (!changed && !capturing)
      continue;
    if (capturing && s_pendingChord.empty())
      continue;
    std::wstring eff = changed ? h.accept_new : h.accept;
    if (capturing && !s_pendingChord.empty())
      eff = s_pendingChord;
    if (eff.empty())
      continue;
    if (CheckConflict(eff, h.when, i).score > 0)
      ++conflicts;
  }

  std::wstring text = L"● 未保存 (" + std::to_wstring(modified) + L" 修改)";
  if (conflicts > 0)
    text += L"   ⚠ " + std::to_wstring(conflicts) + L" 个冲突";
  else
    text += L"   ✓ 无冲突";
  SetWindowTextW(s_hStatus, text.c_str());

  // OnCtlColor 据此决定文字颜色 (冲突=警示橙)
  const bool nowConflict = (conflicts > 0);
  if (nowConflict != s_statusConflict) {
    s_statusConflict = nowConflict;
    InvalidateRect(s_hStatus, nullptr, TRUE);
  }
}

LRESULT CALLBACK ShortcutSettings::LowLevelKeyboardProc(int nCode,
                                                        WPARAM wp,
                                                        LPARAM lp) {
  if (nCode != HC_ACTION) {
    return CallNextHookEx(nullptr, nCode, wp, lp);
  }
  // 仅 keydown WM_KEYDOWN (避免 keyup 误触发)
  if (wp != WM_KEYDOWN)
    return CallNextHookEx(nullptr, nCode, wp, lp);

  KBDLLHOOKSTRUCT* ks = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
  DWORD vk = ks->vkCode;
  bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
  bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
  bool sup = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 ||
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
      vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_MENU ||
      vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN;

  if (isModifierOnly) {
    // 仅修饰键: 等用户继续按,啥都不做
    return CallNextHookEx(nullptr, nCode, wp, lp);
  }

  // Real chord: 与前端同一套字符解析, 产出的 accept 串 librime 才真的匹配得到
  // (ADR 0011 D1/D3)
  fluxing::CapturedAccept captured;
  if (!CaptureHookChord(ks, ctrl, shift, alt, sup, &captured)) {
    // 该组合键写不成绑定: 清掉 pending, 并用中文说明原因 (constitution P2)
    ShortcutSettings::s_pendingChord.clear();
    SetCaptureHint(fluxing::AcceptErrorMessage(captured.error));
    return CallNextHookEx(nullptr, nCode, wp, lp);
  }

  ShortcutSettings::s_pendingChord = captured.pattern;
  // T101: 实时冲突检测 —— 纯内存比较, 不 SendInput / 不 IPC (AP-045-B)
  ShortcutSettings::UpdateCaptureConflict();

  // 不吞掉修饰键以外的键 → 让用户按键透传到 WeaselServer,但我们已经记录
  // 这里为了 UX 完整,继续传给下一 hook(避免吞掉其他应用);若 ShortcutSettings
  // 是 focus 窗口,正常不会冲突;spec AP-045-B:不 SendInput / 不触发 IPC
  return CallNextHookEx(nullptr, nCode, wp, lp);
}

// ===== WndProc =====

LRESULT CALLBACK ShortcutSettings::WndProc(HWND hwnd,
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
      // v0.19.0.33 fix: 焦点在对话框自己的子控件之间转移时,父窗口同样会收到
      // WM_KILLFOCUS(wParam = 获得焦点的子窗口)。原来这里无条件 Hide() ⇒
      // 用户打开面板超过 kShowGraceMs 之后点任意一行,面板当场自毁:
      // 真机实测(批次 N)先点行 → 捕获 popover 刚创建就被销毁,后面所有键捕获
      // 步骤全部无从触发。只有焦点真的离开本窗口(落到别的窗口)才该隐藏。
      HWND next = reinterpret_cast<HWND>(wp);
      if (next == hwnd || (next && IsChild(hwnd, next))) {
        return 0;
      }
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
  // v0.19.0.31 fix: 删 SetLayeredWindowAttributes(LWA_ALPHA)。
  // ShortcutSettings 没有 UpdateLayeredWindow 路径 (RedrawWindow),
  // 但保留 LWA_ALPHA 跟后续可能在 WndProc 加的 ULW_ALPHA / memDc 画 chrome
  // 路径互斥。原 v0.19.0.28-30 chrome 只画 title bar 30-56px, body 空。
  // 修法:删 LWA_ALPHA + OnPaint 改画整 client (见 OnPaint fix)。

  // 几何 (runtime)
  // spec 095: 「界面」分组行(kPrefsH)插在工具栏行与状态行之间 ⇒ 表格变矮、
  // 按钮行下移。旧公式不含 kPrefsH,新增这一行必须同步改这两个式子。
  kTableH_phys = kDialogH - kTitleH - kToolbarH - kStatusBarH - kBtnH -
                 6 * kGap - kPrefsH;
  kBtnY_phys = kTitleH + kToolbarH + kStatusBarH + kPrefsH + kTableH_phys +
               2 * kGap;
  if (kTableH_phys < 100)
    kTableH_phys = 100;
  if (kBtnY_phys + kBtnH > kDialogH)
    kBtnY_phys = kDialogH - kBtnH - 2;

  // 主字体
  HFONT hfUi = CreateFontW(kUiFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
  HFONT hfMono =
      CreateFontW(kMonoFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Mono");
  HFONT hfTitle = CreateFontW(kTitleFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
  (void)hfMono;
  (void)hfTitle;  // set on controls below

  // Toolbar row: filter chips (label only,非交互简化)+ search + 提示
  int toolY = kTitleH + kGap;
  s_hSearch = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, kDialogW / 2, toolY,
      kDialogW / 2 - 2 * kGap, 28, hwnd, reinterpret_cast<HMENU>(ID_SEARCH),
      GetModuleHandle(nullptr), nullptr);
  if (s_hSearch && hfUi) {
    SendMessageW(s_hSearch, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // spec 095: 「界面」分组行 —— 快捷面板常驻勾选框。
  // 只改「常驻」状态 + 落盘,不主动 Show/Hide 面板(与托盘菜单
  // 「开⇒显示 / 关⇒收起」的语义不同,差异见 spec 095 决策 D7)。
  // 视觉副作用(alpha 复位 / 重绘)由 QuickPanelDialog 注册的
  // ModeChangedCallback 承担,这里只负责状态。
  s_hChkAlwaysShow = CreateWindowExW(
      0, L"BUTTON", L"快捷面板常驻",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_AUTOCHECKBOX,
      kGap + 56, kPrefsY + 2, 200, kPrefsH - 4, hwnd,
      reinterpret_cast<HMENU>(ID_CHK_ALWAYS_SHOW), GetModuleHandle(nullptr),
      nullptr);
  if (s_hChkAlwaysShow && hfUi) {
    SendMessageW(s_hChkAlwaysShow, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi),
                 TRUE);
  }
  if (s_hChkAlwaysShow) {
    SendMessageW(s_hChkAlwaysShow, BM_SETCHECK,
                 AlwaysShowChecked() ? BST_CHECKED : BST_UNCHECKED, 0);
  }

  // Status row: 副标题 (status)
  int statusY = kTitleH + kToolbarH + kPrefsH + kGap;
  s_hStatus = CreateWindowExW(
      0, L"STATIC",
      L"● 未保存 (0 "
      L"修改)",  // ● 未保存 (0 修改)
      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | WS_CLIPSIBLINGS, 0,
      statusY, kDialogW, kStatusBarH, hwnd, nullptr, GetModuleHandle(nullptr),
      nullptr);
  if (s_hStatus && hfUi) {
    SendMessageW(s_hStatus, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
  }

  // Table (ListView LVS_REPORT, 3 列: Action / Current / New)
  int tableY = statusY + kStatusBarH + kGap;
  s_hTable = CreateWindowExW(
      WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | LVS_REPORT |
          LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
      kGap, tableY, kDialogW - 2 * kGap, kTableH_phys, hwnd,
      reinterpret_cast<HMENU>(ID_TABLE), GetModuleHandle(nullptr), nullptr);
  if (s_hTable && hfMono) {
    SendMessageW(s_hTable, WM_SETFONT, reinterpret_cast<WPARAM>(hfMono), TRUE);
  }
  // 表头 + 列
  {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = const_cast<LPWSTR>(L"动作");  // 动作
    col.cx = 280;
    col.iSubItem = 0;
    ListView_InsertColumn(s_hTable, 0, &col);
    col.pszText = const_cast<LPWSTR>(L"当前");  // 当前
    col.cx = 200;
    col.iSubItem = 1;
    ListView_InsertColumn(s_hTable, 1, &col);
    col.pszText = const_cast<LPWSTR>(L"新值");  // 新值
    col.cx = 200;
    col.iSubItem = 2;
    ListView_InsertColumn(s_hTable, 2, &col);
  }
  // Extended styles: grid + full row select
  ListView_SetExtendedListViewStyle(s_hTable,
                                    LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  // 按钮行: + 自定义 / ⟳ 恢复默认 / 导入 / 导出 / 取消 / 保存
  // v0.19.0.32 fix: 父窗口必须用 OnCreate 的入参 hwnd,不能用
  // ShortcutSettings::s_hwnd —— s_hwnd 是在 CreateWindowExW 返回之后才被赋值,
  // WM_CREATE 期间它仍是 nullptr ⇒ CreateWindowExW(WS_CHILD, 父=NULL) 失败 ⇒
  // 底栏 6 个按钮(自定义/恢复默认/导入/导出/取消/保存)全部从未创建。
  // 真机实测症状:对话框能打开,但子控件只有搜索框/勾选框/状态行/表格,
  // 保存按钮不存在 ⇒ 界面路径无法落盘。
  auto makeBtn = [&hfUi, hwnd](HWND& out, const wchar_t* label, UINT id, int x,
                               int y) {
    out = CreateWindowExW(
        0, L"BUTTON", label,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_PUSHBUTTON, x, y, kBtnW,
        kBtnH, hwnd, reinterpret_cast<HMENU>(id),
        GetModuleHandle(nullptr), nullptr);
    if (out && hfUi) {
      SendMessageW(out, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    }
  };

  int btnY = kBtnY_phys;
  int btnX = kGap;
  makeBtn(s_hBtnAdd, L"+ 自定义",
          ID_BTN_ADD,  // + 自定义
          btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnReset, L"⟳ 恢复默认",
          ID_BTN_RESET,  // ⟳ 恢复默认 (注:这会是 ⟳ 字符 + 恢复)
          btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnImport, L"导入", ID_BTN_IMPORT, btnX, btnY);
  btnX += kBtnW + kBtnGap;
  makeBtn(s_hBtnExport, L"导出", ID_BTN_EXPORT, btnX, btnY);
  // right-aligned: 取消 + 保存
  int rightX = kDialogW - kGap;
  makeBtn(s_hBtnSave, L"保存", ID_BTN_SAVE,  // 保存
          rightX - kBtnW, btnY);
  rightX -= kBtnW + kBtnGap;
  makeBtn(s_hBtnCancel, L"取消", ID_BTN_CANCEL,  // 取消
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
  s_hChkAlwaysShow = nullptr;
  return 0;
}

LRESULT ShortcutSettings::OnPaint(HWND hwnd) {
  PAINTSTRUCT ps;
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT rc;
  GetClientRect(hwnd, &rc);

  // v0.19.0.31: 整 client 渐变 (原 v0.19.0.28-30 只画 [0, kTitleH) 56px)。
  ModalChrome::PaintBackgroundAndBorder(hdc, kDialogW, kDialogH, kBgTop, kBgBot,
                                        kDlgRadius, kBorderColor);

  // 3) Title text ("快捷键设置" 21px bold)
  {
    HFONT hfTitle = CreateFontW(
        kTitleFontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
    HFONT hfOld = static_cast<HFONT>(SelectObject(hdc, hfTitle));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextColor);
    RECT titleRc = {20, 6, kDialogW - 60, 32};
    DrawTextW(
        hdc,
        L"⌨ "
        L"设置",  // 设置
        -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfOld);
    DeleteObject(hfTitle);

    HFONT hfSub = CreateFontW(kSubFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
    HFONT hfOld2 = static_cast<HFONT>(SelectObject(hdc, hfSub));
    SetTextColor(hdc, kTextSecondary);
    RECT subRc = {20, 32, kDialogW - 60, 50};
    std::wstring sub = L"快捷键与界面设置 · " +
                       std::to_wstring(m_hotkeys.size()) + L" 条快捷键";
    DrawTextW(hdc, sub.c_str(), -1, &subRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfOld2);
    DeleteObject(hfSub);
  }

  // spec 095: 「界面」分组标题 + 分隔线(自绘,与标题栏同一套字体)。
  {
    HFONT hfGrp = CreateFontW(
        kSmallFontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
    HFONT hfOldGrp = static_cast<HFONT>(SelectObject(hdc, hfGrp));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kTextTertiary);
    RECT grpRc = {kGap, kPrefsY, kGap + 48, kPrefsY + kPrefsH};
    DrawTextW(hdc, L"界面", -1, &grpRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hfOldGrp);
    DeleteObject(hfGrp);

    HPEN hLine = CreatePen(PS_SOLID, 1, kBorderColor);
    HPEN hOldPen = static_cast<HPEN>(SelectObject(hdc, hLine));
    HBRUSH hOldBrush =
        static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    MoveToEx(hdc, kGap, kPrefsY + kPrefsH, nullptr);
    LineTo(hdc, kDialogW - kGap, kPrefsY + kPrefsH);
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hLine);
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
    DeleteObject(hPen);
    DeleteObject(hBrush);

    HFONT hfX =
        CreateFontW(12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    VARIABLE_PITCH | FF_SWISS, L"Segoe UI Variable");
    HFONT hfXO = static_cast<HFONT>(SelectObject(hdc, hfX));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, kButtonFgPrimary);
    RECT xRc = {cx - 10, cy - 10, cx + 10, cy + 10};
    DrawTextW(hdc, L"×", -1, &xRc,  // ×
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
    // T103: 搜索框过滤后 显示行号 ≠ m_hotkeys 下标 ⇒ 一律按 lParam 取数据下标
    int row = ia->iItem >= 0 ? TableItemData(s_hTable, ia->iItem) : -1;
    if (row >= 0 && row < static_cast<int>(m_hotkeys.size())) {
      // T111: 应用级行只读, 不进键捕获; popover 给出原因。
      if (m_hotkeys[row].read_only) {
        EnsurePopover();
        SetPopoverHint(L"该快捷键由程序注册，当前版本暂不支持自定义");
        // T111 fix: 定位到**被点的那一行**旁边(否则提示落在屏外看不见),
        // 并挂一次性定时器, 到点自动消失, 不再永久残留。
        PositionPopover(ia->iItem);
        SetTimer(s_hwnd, IDT_HINT_HIDE, kHintHideMs, nullptr);
        return 1;
      }
      EnterCapturing(row);
      return 1;
    }
  } else if (nmhdr->code == NM_RCLICK) {
    LPNMITEMACTIVATE ia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
    // T103: 同上 —— 右键命中的行也按 lParam 取数据下标
    int row = ia->iItem >= 0 ? TableItemData(s_hTable, ia->iItem) : -1;
    if (row >= 0 && row < static_cast<int>(m_hotkeys.size())) {
      // T111: 应用级行连「重置此行」也没有 —— 它本来就不受本表管辖。
      if (m_hotkeys[row].read_only)
        return 1;
      // 弹简易菜单:重置此行
      HMENU hMenu = CreatePopupMenu();
      AppendMenuW(
          hMenu, MF_STRING, 1001,
          L"重置此行");  // 重置此行
      POINT pt;
      GetCursorPos(&pt);
      int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y,
                               0, hwnd, nullptr);
      DestroyMenu(hMenu);
      if (cmd == 1001) {
        if (row < static_cast<int>(m_defaults.size())) {
          // T110: 整行还原 (键位 + 动作 + when)。只还原 accept 会留下
          // 「基线键位配了用户的动作」这种错配行。
          m_hotkeys[row] = m_defaults[row];
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
      m_hotkeys.push_back(
          MakeBuiltin(Action_CustomUser, L"always", L"",
                      L"自定义 - "
                      L"确认后记录"
                      L"",  // 自定义 - 确认后记录
                      L"custom: user_added", L"custom: user_added"));
      PopulateTable();
      RepaintLayered(hwnd);
      return 0;
    case ID_BTN_RESET: {
      // ⟳ 恢复默认: 弹确认后全 reset (spec §8 table decision)
      int ok = MessageBoxW(
          hwnd,
          L"将丢弃所有"
          L"自定义快捷"
          L"键，确定"
          L"？",  // 将丢弃所有自定义快捷键，确定？
          L"恢复默认",  // 恢复默认
          MB_YESNO | MB_ICONWARNING);
      if (ok == IDYES) {
        // spec 045 P0: 原来还原的是 m_defaults —— 而 m_defaults 是"打开时
        // 读到的内容",所以对有自定义的用户来说,"恢复默认"等于什么都没做。
        // 这里真的回到 schema 内置默认表。
        // T110: 恢复基线原样 (全局表) 并丢弃用户自定义行。
        if (!m_pristine.empty()) {
          m_hotkeys = m_pristine;
        } else {
          m_hotkeys.clear();
          LoadDefaults(m_hotkeys);
        }
        PopulateTable();
        RepaintLayered(hwnd);
      }
      return 0;
    }
    case ID_BTN_IMPORT: {
      // spec 045 P0: 原来"导入"是重读同一个线上文件(把未保存的编辑丢掉),
      // "导出"是覆写同一个线上文件(既不 deploy 又骗人)。现在导入/导出走
      // 一对独立的往返文件,线上配置只由"保存"按钮改。
      std::vector<Hotkey> imported;
      if (!LoadHotkeys(ExportYamlPath(), imported) || imported.empty()) {
        if (s_hStatus && IsWindow(s_hStatus)) {
          SetWindowTextW(s_hStatus, L"✗ 导入失败：没有可导入的导出文件");
        }
        return 0;
      }
      m_hotkeys = imported;
      PopulateTable();
      RepaintLayered(hwnd);
      if (s_hStatus && IsWindow(s_hStatus)) {
        SetWindowTextW(s_hStatus, L"✓ 已导入导出文件");
      }
      return 0;
    }
    case ID_BTN_EXPORT: {
      const std::wstring path = ExportYamlPath();
      if (!SaveHotkeys(path, m_hotkeys, true)) {
        if (s_hStatus && IsWindow(s_hStatus)) {
          SetWindowTextW(s_hStatus, L"✗ 导出失败：无法写入导出文件");
        }
        return 0;
      }
      if (s_hStatus && IsWindow(s_hStatus)) {
        SetWindowTextW(s_hStatus, (L"✓ 已导出到 " + path).c_str());
      }
      return 0;
    }
    case ID_CHK_ALWAYS_SHOW: {
      // spec 095: 勾选框只改「常驻」状态 + 落盘,不主动 Show/Hide 面板。
      if (HIWORD(wp) == BN_CLICKED) {
        const bool checked =
            (SendMessageW(s_hChkAlwaysShow, BM_GETCHECK, 0, 0) ==
             BST_CHECKED);
        SetAlwaysShowChecked(checked);
      }
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
  if (wp == IDT_HINT_HIDE) {
    // T111 fix: 只读提示是**一次性**提示 —— 到点必须消失并释放窗口。
    // 捕获态(IDT_BLINK 在跑)的 popover 由 ExitCapturing 管, 这里不能碰。
    KillTimer(hwnd, IDT_HINT_HIDE);
    if (s_capturingRow < 0)
      DestroyPopover();
    return 0;
  }
  return 0;
}

LRESULT ShortcutSettings::OnKeyDown(HWND hwnd, WPARAM wp) {
  // Ctrl+F → focus search
  if ((GetKeyState(VK_CONTROL) & 0x8000) && wp == 'F') {
    if (s_hSearch && IsWindow(s_hSearch))
      SetFocus(s_hSearch);
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
    if (!sb)
      sb = CreateSolidBrush(kSearchBg);
    return reinterpret_cast<LRESULT>(sb);
  }
  if (target == s_hStatus) {
    SetBkColor(hdc, kBgTop);
    // T102: 有冲突时警示橙, 否则沿用次级文字色 (不新增硬编码颜色)
    SetTextColor(hdc, s_statusConflict ? kWarningText : kTextSecondary);
    static HBRUSH sb2 = nullptr;
    if (!sb2)
      sb2 = CreateSolidBrush(kBgTop);
    return reinterpret_cast<LRESULT>(sb2);
  }
  return DefWindowProcW(hwnd, WM_CTLCOLORSTATIC, wp, lp);
}

// ===== Save + deploy =====

bool ShortcutSettings::SaveAndDeploy() {
  if (s_yamlPath.empty())
    return false;
  if (!SaveHotkeys(s_yamlPath, m_hotkeys)) {
    if (s_hStatus && IsWindow(s_hStatus)) {
      // 拒绝写入和写失败是两件事, 文案不能混; 两条拒绝原因也不能混。
      // T107 起「已有 patch 段」不再是拒绝理由 (那种文件现在能正文级合并),
      // 会走到这里的只剩「段里有手写 key_binder」与「patch 是内联写法」两种,
      // 各自给一句可执行的下一步。UI 不对自己做的事说谎。
      const wchar_t* reason =
          g_saveStats.refused_no_baseline
              ? L"✗ 未写入：读不到全局快捷键表基线（default.yaml），为避免清空全局快捷键表已放弃保存"
              : (g_saveStats.refused_has_manual_bindings
                     ? L"✗ 未写入：patch 段里已有手写的 key_binder，请先移除或手工合并"
                     : (g_saveStats.refused_inline_patch
                            ? L"✗ 未写入：patch 用了内联写法（patch: {…}），请改成换行块写法"
                            : L"✗ 保存失败：无法写入 default.custom.yaml"));
      SetWindowTextW(s_hStatus, reason);
    }
    return false;
  }
  if (g_saveStats.no_change) {
    // 有效列表与基线完全一致 ⇒ 没有可写的 bindings, 也就没必要触发部署。
    if (s_hStatus && IsWindow(s_hStatus)) {
      SetWindowTextW(s_hStatus, L"✓ 无改动 · 未触发部署");
    }
    return false;
  }
  const bool deployed = SpawnDeploy();
  // spec 045 P0: 原来这里无条件写 "deploy 已触发", 而 SpawnDeploy() 是空函数
  // —— UI 在说谎,新快捷键其实永远不生效。现在按实际返回值给文案。
  if (s_hStatus && IsWindow(s_hStatus)) {
    std::wstring msg =
        deployed ? L"✓ 已保存 · 已触发部署，新快捷键已生效"
                 : L"⚠ 已保存 · 部署未触发（新快捷键尚未生效）";
    // 跳过与备份也要说出来,静默处理等于悄悄丢用户的编辑。
    if (g_saveStats.skipped_no_action > 0) {
      msg += L" · " + std::to_wstring(g_saveStats.skipped_no_action) +
             L" 行缺 YAML 动作已跳过";
    }
    if (g_saveStats.backed_up)
      msg += L" · 原文件已备份";
    SetWindowTextW(s_hStatus, msg.c_str());
  }
  return true;
}

bool ShortcutSettings::SpawnDeploy() {
  // spec §7.1: 触发 WeaselDeployer /deploy。spec AP-045-B: 不在 hook 做 IPC
  // —— 这里跑在按钮点击路径上,不在键盘 hook 里,合规。
  // WeaselDeployer.exe 与 WeaselServer.exe 同目录(output/install.nsi 如此安装)。
  wchar_t exe_path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, exe_path, _countof(exe_path));
  std::wstring dir(exe_path);
  const size_t pos = dir.find_last_of(L"\\/");
  if (pos == std::wstring::npos)
    return false;
  const std::wstring deployer = dir.substr(0, pos + 1) + L"WeaselDeployer.exe";
  if (GetFileAttributesW(deployer.c_str()) == INVALID_FILE_ATTRIBUTES)
    return false;
  const HINSTANCE rc = ShellExecuteW(nullptr, nullptr, deployer.c_str(),
                                      L"/deploy", nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(rc) > 32;
}

std::wstring ShortcutSettings::ExportYamlPath() {
  const std::wstring dir = WeaselUserDataPath().wstring();
  if (dir.empty())
    return L"default.custom.exported.yaml";
  return dir + L"\\default.custom.exported.yaml";
}

// ===== spec 095: 「界面 > 快捷面板常驻」勾选框的生产 API =====
// 设置面板的勾选框与单元测试共用同一实现(constitution R7);不建窗口也能读写,
// 所以 test/TestShortcutSettings 可以直接断言它们。
bool ShortcutSettings::AlwaysShowChecked() {
  return fluxing::always_show::AlwaysShowEnabled();
}

void ShortcutSettings::SetAlwaysShowChecked(bool checked) {
  // 落盘 + 通知观察者(QuickPanelDialog 注册的回调负责 alpha 复位 / 重绘)。
  fluxing::always_show::ApplyAlwaysShow(checked);
  if (s_hChkAlwaysShow && IsWindow(s_hChkAlwaysShow)) {
    SendMessageW(s_hChkAlwaysShow, BM_SETCHECK,
                 checked ? BST_CHECKED : BST_UNCHECKED, 0);
  }
}

// ===== Center / Repaint =====

void ShortcutSettings::CenterOnPrimaryMonitor(HWND hwnd, int w, int h) {
  HMONITOR mon = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
  int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;
  SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void ShortcutSettings::RepaintLayered(HWND hwnd) {
  // L97 fix: WS_EX_LAYERED 窗口必须 explicit RedrawWindow with UPDATENOW
  if (hwnd && IsWindow(hwnd)) {
    RedrawWindow(hwnd, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
  }
}
