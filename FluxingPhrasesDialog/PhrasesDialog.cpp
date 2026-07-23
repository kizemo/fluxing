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
#include "PipeClient.h"
#include "FluxingPipeProtocol.h"

#include <algorithm>
#include <cassert>
#include <commctrl.h>
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
std::wstring PhrasesDialog::s_pipeName;
fluxing::PipeClient* PhrasesDialog::s_pipeClient = nullptr;
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
// v0.19.0.44 (Feature 2: 长按 reorder ListView entry): state 初始化
bool PhrasesDialog::s_reorderDragActive = false;
bool PhrasesDialog::s_isReorderDragging = false;
int PhrasesDialog::s_dragSourceIdx = -1;  // -1 = 无 drag
int PhrasesDialog::s_dropTargetIdx = -1;

// ===== 设计常量 (spec 044 §3) =====
namespace {
// 尺寸 (size.modal.*)
constexpr int kDialogW = 360;
constexpr int kDialogH = 460;
// v0.19.0.49 (Phase J Bug A/D): kTitleH 30→48 让 title bar click 命中区扩大。
//   装机 v0.19.0.48 user 反馈 "顶部小蓝条高度过窄, 无法点中" + Bug 3 title bar
//   立即 drag 无法触发。物理 30 像素在 100% DPI 仅 24-28 视觉像素, click 困难;
//   改 48 后 ~38-44 视觉像素, 标准 Windows dialog title bar 大小。
constexpr int kTitleH = 48;
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

  // Phase K2 (v0.19.0.51): pipe IPC 接收短语数据
  // 有 pipe → 连接 WeaselServer 收 PHRASES；无 pipe → 硬编码测试数据
  m_phrases.clear();
  if (!s_pipeName.empty()) {
    if (!s_pipeClient) {
      s_pipeClient = new fluxing::PipeClient();
    }
    if (s_pipeClient->Connect(s_pipeName)) {
      std::string json = s_pipeClient->ReadMessage();
      if (!json.empty()) {
        fluxing::PipeMessage pm = fluxing::ParseMessage(json);
        if (pm.type == fluxing::PipeMsgType::MT_PHRASES) {
          for (const auto& pp : pm.phrases) {
            m_phrases.push_back({pp.text, pp.category});
          }
        }
      }
      if (m_phrases.empty()) {
        // pipe 连通但无数据 → 放一条占位
        m_phrases.push_back({L"\x7b49\x5f85\x6570\x636e...", L""});  // 等待数据...
      }
    } else {
      // pipe 连接失败 → 降级到硬编码
      OutputDebugStringW(L"[PhrasesDialog] Pipe connect failed, using fallback data\n");
      m_phrases.push_back({L"\x4f60\x597d", L""});       // 你好
      m_phrases.push_back({L"\x4e16\x754c", L""});       // 世界
      m_phrases.push_back({L"\x6d4b\x8bd5", L""});       // 测试
    }
  } else {
    // 无 pipe: 硬编码测试数据 (独立测试用)
    m_phrases.push_back({L"\x4f60\x597d", L""});       // 你好
    m_phrases.push_back({L"\x4e16\x754c", L""});       // 世界
    m_phrases.push_back({L"\x6d4b\x8bd5", L""});       // 测试
    m_phrases.push_back({L"\x5e38\x7528", L""});       // 常用
    m_phrases.push_back({L"\x77ed\x8bed", L""});       // 短语
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
    FlushSave();  // 最后写一次 (pipe 模式: 已通过 pipe 实时同步)
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

  // Phase K2: pipe 断开 (但不发 SHUTDOWN — Hide 后可能还要 Show)
  if (s_pipeClient && s_pipeClient->IsConnected()) {
    s_pipeClient->Disconnect();
  }
}

// v0.19.0.55 (Phase K3 T019 Bug 3 修): NM_DBLCLK/NM_RETURN/VK_RETURN
//   路径用 — 先调本函数 Hide (但保留 pipe), Hide 后 dialog DestroyWindow
//   同步归还 foreground 给 user app, 然后再用残留 pipe 发 INJECT + ACK,
//   最后再调普通 Hide 收尾 (会 Disconnect pipe)。
//   跟 Hide() 唯一区别: 跳过 s_pipeClient->Disconnect() (保留 pipe 让
//   后续 INJECT 能 send + read ACK)。
void PhrasesDialog::HideWithoutDisconnect() {
  if (s_hwnd && IsWindow(s_hwnd)) {
    FlushSave();  // 最后写一次 (pipe 模式: 已通过 pipe 实时同步)
    DestroyWindow(s_hwnd);
  }
  s_hwnd = nullptr;
  s_hInput = nullptr;
  s_hBtnAddTop = nullptr;
  s_hList = nullptr;
  s_hBtnEdit = s_hBtnDel = s_hBtnCancel = nullptr;
  s_state = State_Hidden;
  m_selectedIndex = -1;
  s_isDragging = false;
  s_longPressActive = false;
  // 关键: 不 Disconnect pipe — 让 NM_DBLCLK 等后续 INJECT 能 send + read ACK
}

void PhrasesDialog::SetInjectFn(InjectFn fn) {
  s_injectFn = fn;
}

void PhrasesDialog::SetYamlPath(const std::wstring& path) {
  s_yamlPath = path;
}

void PhrasesDialog::SetPipeName(const std::wstring& pipeName) {
  s_pipeName = pipeName;
}

const std::wstring& PhrasesDialog::GetPipeName() {
  return s_pipeName;
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
  // Phase K1 (v0.19.0.50): yaml 读写已迁移到 WeaselServer 端（pipe IPC）
  // 本 exe 通过 pipe 接收短语数据，此函数保留接口兼容，内部为 no-op
  (void)path;
  (void)out;
  return false;
}

bool PhrasesDialog::SavePhrases(const std::wstring& path,
                                const std::vector<Phrase>& data) {
  // Phase K1 (v0.19.0.50): yaml 持久化迁移到 WeaselServer 端
  // 本 exe 通过 pipe 发送 Add/Edit/Delete 指令，Server 端负责落盘
  (void)path;
  (void)data;
  return false;
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
    // v0.19.0.44 (Feature 1: resize layout): WM_SIZE 触发 LayoutDialog
    // 重新定位所有 child widgets (input + AddTop fixed, list 伸缩, 3 个按钮
    // anchored bottom-right 固定边距)。
    case WM_SIZE: {
      UINT newW = LOWORD(lp);
      UINT newH = HIWORD(lp);
      // SIZE_MINIMIZED (0,0) 跳过 — minimize 时 client area 无效
      if (newW > 0 && newH > 0) {
        LayoutDialog((int)newW, (int)newH);
      }
      return 0;
    }
    case WM_ACTIVATEAPP: {
      if (wp == FALSE) {
        DWORD nowTick = GetTickCount();
        if ((nowTick - s_showTime) >= kShowGraceMs) {
          Hide();
        }
      }
      return 0;
    }
    // v0.19.0.48 (Phase J Bug 2 真修): PhrasesDialog open 时
    //   Ctrl+Shift / Win+Space 切 IME 不响应。原因: dialog SetForegroundWindow
    //   抢 foreground lock + WS_EX_TOPMOST, OS IME 切换路由被劫持。
    //   修法: WM_INPUTLANGCHANGEREQUEST 转发到 OS (ActivateKeyboardLayout)
    //   + WM_INPUTLANGCHANGE 记录当前 IME (后续可调 ImmSetConversionStatus)。
    case WM_INPUTLANGCHANGEREQUEST: {
      // wParam: input language identifier (HKL), lParam: flags
      HKL hkl = (HKL)wp;
      WORD flags = LOWORD(lp);
      if (hkl) {
        ActivateKeyboardLayout(hkl, flags);
      }
      return DefWindowProcW(hwnd, WM_INPUTLANGCHANGEREQUEST, wp, lp);
    }
    case WM_INPUTLANGCHANGE: {
      WORD charset = HIWORD(wp);
      HKL hkl = (HKL)lp;
      OutputDebugStringW(L"[PhrasesDialog v0.19.0.48] WM_INPUTLANGCHANGE "
                          L"charset=0x");
      // 简化日志: 不打印完整 HKL, 装机端可用 ImmGetIMEFileName 查
      return DefWindowProcW(hwnd, WM_INPUTLANGCHANGE, wp, lp);
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
      // v0.19.0.44 (Feature 2): reorder drag 中也不 Hide (跟 dialog drag 同理)
      if (s_isReorderDragging) {
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
  // v0.19.0.45 (Phase H Bug 1 真修): 提前设 s_hwnd = hwnd。CreateWindowExW
  //   同步 dispatch WM_CREATE → OnCreate 在 CreateWindowExW 返回前就跑了,
  //   此时 Show() 的 `s_hwnd = CreateWindowExW(...)` 还没执行, s_hwnd = null。
  //   LayoutDialog 末尾会调一次 (line 757), 但它的 `if (!s_hwnd) return;`
  //   early-return, 结果 OnCreate 阶段 column width / child 位置都没设 —
  //   user 装机反馈 "初始 UI 列表框右边距过大" 真因之一。
  //   修法: OnCreate 入口 set s_hwnd = hwnd, 让 LayoutDialog 能跑。
  s_hwnd = hwnd;

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
  // v0.19.0.45 (Phase H Bug 2): input 左 margin 从 kBtnMarginX (= 12)
  //   改成 gap (= 8)。跟 LayoutDialog 保持一致 (LayoutDialog 末尾 line 744
  //   调一次覆盖此初始位置, 视觉一致)。
  int inputY = titleH + gap;
  int inputX = gap;
  s_hInput = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", L"",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
      inputX, inputY, inputW, inputH, hwnd, reinterpret_cast<HMENU>(ID_INPUT),
      GetModuleHandle(nullptr), nullptr);
  if (s_hInput && hfUi) {
    SendMessageW(s_hInput, WM_SETFONT, reinterpret_cast<WPARAM>(hfUi), TRUE);
    // v0.19.0.46 (Phase I Bug 4 真修): **删** ImmCreateContext +
    //   ImmAssociateContext 调用。历史 (v0.19.0.35/36/43/45) 4 个 ship 版本
    //   装机端都报"输入框不能输中文", 见 lessons-learned.md L##-PhaseH-IME。
    //   根因: WeaselServer.exe 是 TSF shim 进程 (weaselx64.dll 主导),
    //   TSF 要求 hwnd 用 system default IME context; 我们自己创建 isolated
    //   HIMC 给 s_hInput, 跟 TSF IME 互斥 → 中文候选词不出。修法: 让 TSF
    //   shim 自动给 hwnd 配 system default IME context (跟主编辑框同路径,
    //   v0.19.0.32 之前裸 CreateWindowExW 路径)。
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
  // v0.19.0.45 (Phase H Bug 1 真修): 删 cx 设置, 只 mask = LVCF_TEXT +
  //   LVCF_SUBITEM。装机 v0.19.0.44 user 反馈 "初始 UI 列表框右侧边距过大,
  //   resize 后正常" — 原版 col.cx = dW - 2*gap - 4 (raw clientW - slack),
  //   没考虑 vertical scrollbar。PopulateList 后 scrollbar 显示, header
  //   client = ListView client - scrollbar, 实际 visible column 比 cx 小 →
  //   视觉"边距过大" (column 内容 overflow 17px, 视觉上像 column 右边
  //   空了一大块)。resize 后 LayoutDialog 重算 cx = header client - 4
  //   (scrollbar-aware), 跟 ListView visible 匹配 → 视觉"正常"。
  // 修法: OnCreate 只 create column (text + subitem), 不设宽度。让
  //   LayoutDialog 末尾 (line ~744 调一次) 统一算 column width, 用
  //   header client rect - 4, 自动减去 scrollbar。
  {
    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_SUBITEM;
    col.pszText = const_cast<wchar_t*>(
        L"");  // 空 — 整个 ListView 内容都是短语, header "短语" 冗余
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

  // v0.19.0.44 (Feature 1: resize layout): OnCreate 末尾统一调 LayoutDialog
  // 定位所有 child (跟 WM_SIZE 用同一函数, 避免初始位置跟 resize 后位置
  // 漂移)。Input AddTop 初始位置已经在创建时算过, LayoutDialog 重新 layout
  // 也无害(同位置)。
  RECT rcClient;
  GetClientRect(hwnd, &rcClient);
  LayoutDialog(rcClient.right - rcClient.left, rcClient.bottom - rcClient.top);

  // v0.19.0.35 (Phase C P0-3): 默认选第一条 (用户报"未选中第一条")
  // 同时把焦点放 ListView (后续键盘 handler 立刻可用)。
  if (s_hList && ListView_GetItemCount(s_hList) > 0) {
    ListView_SetItemState(s_hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    m_selectedIndex = 0;
  }
  // v0.19.0.47 (Phase I Bug 4 续修): 装机 v0.19.0.46 后 user flow 第 3 步
  //   "打拼音出候选词" 仍 fail (真因未完全定位, 候选: TSF shim 懒 attach
  //   只在 SetFocus 该 hwnd 时触发, s_hInput 从未 SetFocus, TSF 从未 attach
  //   default IME context 给 s_hInput, user click 时 TSF attach 失败)。
  //   修法: OnCreate 末 SetFocus(s_hList) 之前先 SetFocus(s_hInput) 强制
  //   触发一次 TSF attach, 然后 SetFocus(s_hList) 让 ListView 拿焦点。
  //   双重 SetFocus 确保 s_hInput 至少触发 1 次 TSF attach, 后续 user click
  //   input 时 IME 应 work。
  if (s_hInput) {
    SetFocus(s_hInput);
  }
  if (s_hList) {
    SetFocus(s_hList);
  }

  // Phase K1 (v0.19.0.50): IMM32 fallback 代码移除 — 独立进程不运行在
  // TSF shim 环境中，系统自动提供 IMM32 IME context，无需手动干预。
  // 原 v0.19.0.49 的 ImmGetContext/ImmAssociateContext/ImmSetOpenStatus
  // 调用是针对 in-process TSF 环境的 workaround，在独立进程中反而有害。

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

  // v0.19.0.57 (Phase K4 Bug 1 修): 顶部蓝条去除 — 整 client 用单色填充
  //   (kBgBot, kBgBot) 替代原来的 gradient (kBgTop → kBgBot)。原 gradient
  //   在顶部 kTitleH 区形成一条浅色带,user 反馈"蓝条多余想删"。单色填充
  //   让 dialog body 一致, 视觉上不再有 title bar 独立色块。
  //   保留 PaintBorder 维持圆角 + hairline 边框。
  ModalChrome::PaintBackground(hdc, paintW, paintH, kBgBot, kBgBot);
  ModalChrome::PaintBorder(hdc, paintW, paintH, paintRadius, kBorderColor);

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
      // 焦点在 input → 智能 add/edit (v0.19.0.57 Phase K4 Bug 3 修)
      //   - list 有选中 (m_selectedIndex >= 0) → Edit (user 在编辑选中的
      //     phrase), 走 ID_BTN_EDIT
      //   - 否则 → Add (新增模式), 走 ID_BTN_ADD_TOP
      //   旧版永远发 ID_BTN_ADD_TOP, 编辑模式下按 Enter 会改错位置
      //   (新增到末尾), 不符合 user 直觉。
      if (s_hInput && GetFocus() == s_hInput) {
        WORD cmdId = (m_selectedIndex >= 0 &&
                      m_selectedIndex < static_cast<int>(m_phrases.size()))
                         ? ID_BTN_EDIT
                         : ID_BTN_ADD_TOP;
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(cmdId, BN_CLICKED), 0);
        return 0;
      }
      // 焦点在 list → inject 选中 phrase text
      if (s_hList && GetFocus() == s_hList && m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        // v0.19.0.55 (Phase K3 T019 Bug 3 修): 改顺序 — Hide 先于 INJECT。
        //   本地捕获 idx + text (Hide 会重置 m_selectedIndex = -1, 后续
        //   不能依赖它)。HideWithoutDisconnect 不 Disconnect pipe, 仍能
        //   发 INJECT + 读 ACK, 然后 Hide 收尾 (Disconnect pipe)。
        int idx = m_selectedIndex;
        std::wstring text = m_phrases[idx].text;
        if (s_pipeClient && s_pipeClient->IsConnected()) {
          HideWithoutDisconnect();
          if (s_pipeClient && s_pipeClient->IsConnected()) {
            s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
            s_pipeClient->ReadMessage();  // 读 ACK
          }
          Hide();
        } else {
          Hide();
          InjectText(text);
        }
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
      // v0.19.0.44 (Feature 2: reorder ListView entry): ListView 内置 drag-drop
      // LVN_BEGINDRAG 用户开始拖, LVN_ENDDRAG 用户释放。 我们手工 reorder
      // m_phrases (不靠 ListView 默认 reorder) — 给 ListView 我们提供
      // 视觉反馈 (insert mark)。
      // 注: LVN_ENDDRAG 在 MS docs 是 LVN_FIRST - 10, 但 SDK header 没定义
      // 这里用 literal -110 (跟 MS docs 一致)。
      case LVN_BEGINDRAG: {
        LPNMLISTVIEW pn = reinterpret_cast<LPNMLISTVIEW>(lp);
        int idx = pn->iItem;
        if (idx >= 0 && idx < (int)m_phrases.size()) {
          s_dragSourceIdx = idx;
          s_dropTargetIdx = idx;
          s_isReorderDragging = true;
          SetCapture(hwnd);  // 确保 mouse move 离开 list 也收到
          ListView_SetInsertMark(s_hList, idx, TRUE);
        }
        return 0;  // 让 ListView 也处理 (不返回 TRUE 取消)
      }
      case -110: {  // LVN_ENDDRAG = LVN_FIRST - 10
        if (s_isReorderDragging) {
          LPNMLISTVIEW pn = reinterpret_cast<LPNMLISTVIEW>(lp);
          int target = pn->iItem;
          if (target < 0) {
            // 拖到 list 下方空区 — 插到末尾
            target = ListView_GetItemCount(s_hList);
          }
          s_dropTargetIdx = target;
          CommitReorder(s_hList);
          EndReorderDrag(hwnd);
        }
        return 0;
      }
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
        // v0.19.0.57 (Phase K4 Bug 3 polish): 选中丢失 → reset state +
        //   清 input, 避免按 Enter 时误触发 Edit。
        //   LVN_ITEMCHANGED 在 selected→not-selected 转换时, uChanged 含
        //   LVIF_STATE + uNewState 不含 LVIS_SELECTED。需检查 iItem ==
        //   当前 m_selectedIndex 才 reset (避免误清其他 item 的状态)。
        else if ((pn->uChanged & LVIF_STATE) &&
                 !(pn->uNewState & LVIS_SELECTED) &&
                 m_selectedIndex == pn->iItem) {
          m_selectedIndex = -1;
          if (s_hInput) {
            SetWindowTextW(s_hInput, L"");
          }
        }
        return 0;
      }
      case NM_DBLCLK: {
        // 双击 → inject
        // v0.19.0.55 (Phase K3 T019 Bug 3 修): 改顺序 — Hide 先于 INJECT。
        //   本地降级模式: 已有 Hide→InjectText (Hide 时 foreground 归位
        //   user app, 然后 InjectText SendInput 命中 user app)。
        //   pipe 模式: HideWithoutDisconnect (保留 pipe) → 发 INJECT →
        //   读 ACK → Hide 收尾 (Disconnect pipe)。
        LPNMITEMACTIVATE pia = reinterpret_cast<LPNMITEMACTIVATE>(lp);
        if (pia && pia->iItem >= 0 &&
            pia->iItem < static_cast<int>(m_phrases.size())) {
          int idx = pia->iItem;
          std::wstring text = m_phrases[idx].text;
          m_selectedIndex = idx;
          if (s_pipeClient && s_pipeClient->IsConnected()) {
            HideWithoutDisconnect();
            if (s_pipeClient && s_pipeClient->IsConnected()) {
              s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
              s_pipeClient->ReadMessage();  // 读 ACK
            }
            Hide();
          } else {
            Hide();
            InjectText(text);
          }
        }
        return 0;
      }
      case NM_RETURN: {
        if (m_selectedIndex >= 0 &&
            m_selectedIndex < static_cast<int>(m_phrases.size())) {
          int idx = m_selectedIndex;
          std::wstring text = m_phrases[idx].text;
          if (s_pipeClient && s_pipeClient->IsConnected()) {
            HideWithoutDisconnect();
            if (s_pipeClient && s_pipeClient->IsConnected()) {
              s_pipeClient->SendMessage(fluxing::BuildINJECT(idx));
              s_pipeClient->ReadMessage();  // 读 ACK
            }
            Hide();
          } else {
            Hide();
            InjectText(text);
          }
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
      if (s_hInput && IsWindow(s_hInput)) {
        wchar_t buf[1024] = {};
        GetWindowTextW(s_hInput, buf, 1024);
        std::wstring text = buf;
        if (!text.empty()) {
          // Phase K2: pipe 模式 → 发送 ADD 命令到 server
          if (s_pipeClient && s_pipeClient->IsConnected()) {
            s_pipeClient->SendMessage(fluxing::BuildADD(text));
            std::string resp = s_pipeClient->ReadMessage();
            if (!resp.empty()) {
              fluxing::PipeMessage pm = fluxing::ParseMessage(resp);
              if (pm.type == fluxing::PipeMsgType::MT_PHRASES) {
                m_phrases.clear();
                for (const auto& pp : pm.phrases) {
                  m_phrases.push_back({pp.text, pp.category});
                }
                PopulateList(s_hList);
              }
            }
          } else {
            // 本地模式: 直接操作 m_phrases
            Phrase np;
            np.text = text;
            m_phrases.push_back(np);
            PopulateList(s_hList);
            FlushSave();
          }
          SetWindowTextW(s_hInput, L"");
          SetFocus(s_hInput);
        }
      }
      return 0;
    }
    // v0.19.0.41 删 ID_BTN_ADD case — 底部 Add 按钮已删, 跟 s_hBtnAddTop 重复
    case ID_BTN_EDIT: {
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size()) && s_hInput &&
          IsWindow(s_hInput)) {
        wchar_t buf[1024] = {};
        GetWindowTextW(s_hInput, buf, 1024);
        std::wstring text = buf;
        if (!text.empty()) {
          if (s_pipeClient && s_pipeClient->IsConnected()) {
            s_pipeClient->SendMessage(fluxing::BuildEDIT(m_selectedIndex, text));
            std::string resp = s_pipeClient->ReadMessage();
            if (!resp.empty()) {
              fluxing::PipeMessage pm = fluxing::ParseMessage(resp);
              if (pm.type == fluxing::PipeMsgType::MT_PHRASES) {
                m_phrases.clear();
                for (const auto& pp : pm.phrases) {
                  m_phrases.push_back({pp.text, pp.category});
                }
                PopulateList(s_hList);
              }
            }
          } else {
            m_phrases[m_selectedIndex].text = text;
            PopulateList(s_hList);
            FlushSave();
          }
          // v0.19.0.57 (Phase K4 Bug 3 polish): Edit 完成 → 清 input + reset
          //   m_selectedIndex + focus 回 input (跟 ID_BTN_ADD_TOP 收尾一致)。
          //   旧版 Edit 后状态残留: m_selectedIndex 仍 >= 0 + input 仍显示
          //   旧 phrase text → user 第二次按 Enter 误触发 Edit 同一 idx,
          //   或 input 显示旧 text 干扰下一次 Add。
          SetWindowTextW(s_hInput, L"");
          SetFocus(s_hInput);
          m_selectedIndex = -1;
        }
      }
      return 0;
    }
    case ID_BTN_DEL: {
      if (m_selectedIndex >= 0 &&
          m_selectedIndex < static_cast<int>(m_phrases.size())) {
        if (s_pipeClient && s_pipeClient->IsConnected()) {
          s_pipeClient->SendMessage(fluxing::BuildDELETE(m_selectedIndex));
          std::string resp = s_pipeClient->ReadMessage();
          if (!resp.empty()) {
            fluxing::PipeMessage pm = fluxing::ParseMessage(resp);
            if (pm.type == fluxing::PipeMsgType::MT_PHRASES) {
              m_phrases.clear();
              for (const auto& pp : pm.phrases) {
                m_phrases.push_back({pp.text, pp.category});
              }
              PopulateList(s_hList);
            }
          }
        } else {
          m_phrases.erase(m_phrases.begin() + m_selectedIndex);
          PopulateList(s_hList);
          FlushSave();
        }
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

  // v0.19.0.48 (Phase J Bug 3 真修): 顶部小蓝条 painted title bar (ModalChrome
  //   画的) 区点击立即 drag, 不像 v0.19.0.41 长按 500ms 才 drag (太不直观)。
  //   检测: y 在 title 区域 (y < titleH_phys = kTitleH * dpiScale) → 立即
  //   BeginDrag + SetCapture + return 0 (不让 Windows 默认处理)。
  const int titleH_phys = (int)(kTitleH * s_dpiScale);
  if (y < titleH_phys) {
    // 顶部 title bar 区 — 立即 drag (跟 Windows 标准 dialog 一致)
    BeginDrag(hwnd);
    return 0;
  }

  // 2. 在其他 chrome 区域 (input 上下空隙 / list 周围) — 启动 long press
  StartLongPressTimer(hwnd, x, y);
  return 0;
}

LRESULT PhrasesDialog::OnLButtonUp(HWND hwnd, WPARAM wp, LPARAM lp) {
  // v0.19.0.44 (Feature 2): reorder 优先于 cancel — 如果在 drag mode 要
  // 先 commit reorder 再 reset state。
  if (s_isReorderDragging) {
    CommitReorder(s_hList);
    EndReorderDrag(hwnd);
    return 0;
  }
  // 释放 dialog drag + 取消 long press (二者互斥)
  EndDrag(hwnd);
  CancelLongPress(hwnd);
  return 0;
}

LRESULT PhrasesDialog::OnMouseMove(HWND hwnd, WPARAM wp, LPARAM lp) {
  if (s_isReorderDragging) {
    // v0.19.0.44 (Feature 2: reorder drag): 在 reorder drag mode, 计算
    // insertion position (基于 mouse client y), 更新 ListView insert mark。
    int y = HIWORD(lp);
    UpdateReorderDropTarget(s_hList, y);
    return 0;
  }
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
    // 500ms 到 — 进入 dialog drag mode (long-press 已经 fire)
    KillTimer(hwnd, kLongPressTimerId);
    s_longPressActive = false;
    BeginDrag(hwnd);
    return 0;
  }
  if (wp == kReorderTimerId) {
    // v0.19.0.44 (Feature 2): reorder 500ms — 进入 reorder drag mode
    BeginReorderDrag(hwnd);
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
  CaptureDragOrigin(hwnd);
  // 第二个参数: 500ms 长按阈值 (kLongPressMs)
  SetTimer(hwnd, kLongPressTimerId, kLongPressMs, nullptr);
}

// v0.19.0.57 (Phase K4 Bug 2 修): DRY helper — 抽取 drag origin 捕获到
//   独立函数。BeginDrag (顶部 title bar 即时 drag 路径) + StartLongPressTimer
//   (chrome 区 500ms 长按 drag 路径) 都调, 确保两条路径都初始化 s_dragOrigin
//   + s_dragWndOrigin。
//   Bug 2 真因: BeginDrag 旧版不调 GetCursorPos + GetWindowRect, 直接 SetCapture
//   → OnMouseMove 第一次触发时 s_dragOrigin/s_dragWndOrigin 仍是静态 {0,0},
//   newX = 0 + (pt.x - 0) = pt.x → dialog 跳到 cursor 屏幕坐标, cursor
//   落在新 dialog 左上角 = snap-to-topleft 视觉。
void PhrasesDialog::CaptureDragOrigin(HWND hwnd) {
  POINT pt;
  GetCursorPos(&pt);
  s_dragOrigin = pt;
  RECT rc;
  GetWindowRect(hwnd, &rc);
  s_dragWndOrigin = {rc.left, rc.top};
}

void PhrasesDialog::CancelLongPress(HWND hwnd) {
  if (s_longPressActive) {
    KillTimer(hwnd, kLongPressTimerId);
    s_longPressActive = false;
  }
}

void PhrasesDialog::BeginDrag(HWND hwnd) {
  // v0.19.0.57 (Phase K4 Bug 2 修): 顶部 title bar 即时 drag 路径必须先
  //   捕获 origin (cursor screen + window rect), 否则 OnMouseMove 第一次
  //   触发时用 stale {0,0} 算 delta → dialog 跳到 cursor 屏幕绝对坐标
  //   → cursor 视觉上落在新 dialog 左上角 (snap-to-topleft)。
  //   跟 StartLongPressTimer 走同 CaptureDragOrigin helper (DRY)。
  CaptureDragOrigin(hwnd);
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

// ===== v0.19.0.44 (Feature 1: resize layout) =====

// LayoutDialog 在 OnCreate 初始化后 + WM_SIZE 触发时调用。
// 输入 client area 物理尺寸 (DPI-scaled), 重新定位所有 child widget:
// - input row (input + AddTop): top-left, 固定位置 + fixed 尺寸
// - list: middle, 伸缩高度 (新 height = clientH - 顶部 input 区 - 底部 button
// 区)
// - 3 个底部按钮 (Edit/Del/Cancel): anchored 到 bottom-right, 固定
//   右边距 (`marginX` = kBtnMarginX) + 固定底边距 (`gap` = kGap),btn 之间
//   间距 (btnGap = kBtnGap) 不变。整体效果: dialog resize 时, 中间
//   输入框/列表 stretch, 按钮边距不变 — 跟 user 装机后的诉求一致。
void PhrasesDialog::LayoutDialog(int clientW, int clientH) {
  if (!s_hwnd)
    return;

  // DPI-scaled dimensions (跟 OnCreate 计算一致)
  const int titleH = (int)(kTitleH * s_dpiScale);
  const int inputH = (int)(kInputH * s_dpiScale);
  const int btnAddTopW = (int)(kBtnAddTopW * s_dpiScale);
  const int btnH = (int)(kBtnH * s_dpiScale);
  const int btnW = (int)(kBtnW * s_dpiScale);
  const int btnGap = (int)(kBtnGap * s_dpiScale);
  // v0.19.0.45 (Phase H Bug 2 真修): UI 通用 margin 从 kBtnMarginX (= 12)
  //   改成 kGap (= 8)。装机 v0.19.0.44 user 反馈 "添加按钮和UI的边距应该
  //   缩小, 使编辑框尽量宽" — 原版 12px 单边过宽, input + AddTop 合计 margin
  //   24px (= 24/360 ≈ 6.7%) 浪费, 改 8px 节省 8px 单边 (16px 总) 让 input
  //   宽 8px @ DPI=1。
  const int gap = (int)(kGap * s_dpiScale);

  // 1. input row — v0.19.0.45 (Bug 2): inputW stretch 跟随 clientW,
  //    左 margin = gap (was kBtnMarginX = 12)。inputX 固定 = gap, inputW
  //    = clientW - 2*gap - btnAddTopW - gap, AddTop anchored 到右边。
  //    整体: gap + input + gap + btnAddTop + gap (对称边距)。
  int inputX = gap;
  int inputY = titleH + gap;
  int inputW = clientW - 2 * gap - btnAddTopW - gap;  // stretch
  if (inputW < 80)
    inputW = 80;  // floor — 不能太小
  if (s_hInput) {
    SetWindowPos(s_hInput, nullptr, inputX, inputY, inputW, inputH,
                 SWP_NOZORDER);
  }

  // 2. AddTop 按钮 (anchored right, 右边距 = gap)
  if (s_hBtnAddTop) {
    int btnTopX = clientW - gap - btnAddTopW;
    SetWindowPos(s_hBtnAddTop, nullptr, btnTopX, inputY, btnAddTopW, inputH,
                 SWP_NOZORDER);
  }

  // 3. list (middle, stretches vertically + horizontally)
  if (s_hList) {
    int listY = inputY + inputH + gap;       // y 起点 = input 底部 + gap
    int btnAreaH = btnH + 2 * gap;           // 底部 button area 占用高度
    int listH = clientH - listY - btnAreaH;  // 剩余给 list
    if (listH < 80)
      listH = 80;  // floor — list 不能太小
    int listX = gap;
    int listW = clientW - 2 * gap;  // 左右各 gap margin
    SetWindowPos(s_hList, nullptr, listX, listY, listW, listH, SWP_NOZORDER);
    // ListView 列宽跟随新 dialog 宽度 (避免 resize 后列宽不变留白)
    HWND hdr = ListView_GetHeader(s_hList);
    if (hdr) {
      RECT rcHdr;
      GetClientRect(hdr, &rcHdr);
      int colW = rcHdr.right - rcHdr.left - 4;  // 4px scrollbar slack
      if (colW < 40)
        colW = 40;
      LVCOLUMNW col = {};
      col.mask = LVCF_WIDTH;
      col.cx = colW;
      ListView_SetColumn(s_hList, 0, &col);
    }
  }

  // 4. 3 个底部按钮 — anchored bottom-right, 固定 btnGap 间距, 右边距 = gap
  //    (v0.19.0.45 Phase H: 改用 gap = 8 统一 UI margin)
  if (s_hBtnEdit && s_hBtnDel && s_hBtnCancel) {
    int btnY = clientH - btnH - gap;  // 底边距 = gap (跟 OnPaint chrome 一致)
    int totalBtnW = 3 * btnW + 2 * btnGap;
    int btnX = clientW - gap - totalBtnW;  // 右边距 = gap
    SetWindowPos(s_hBtnEdit, nullptr, btnX, btnY, btnW, btnH, SWP_NOZORDER);
    btnX += btnW + btnGap;
    SetWindowPos(s_hBtnDel, nullptr, btnX, btnY, btnW, btnH, SWP_NOZORDER);
    btnX += btnW + btnGap;
    SetWindowPos(s_hBtnCancel, nullptr, btnX, btnY, btnW, btnH, SWP_NOZORDER);
  }

  // Legacy/static 字段更新 (test 兼容, Test 28.3 检查 kListH_phys ≥ 80)
  kListH_phys = clientH - titleH - inputH - btnH - 3 * gap;
  if (kListH_phys < 80)
    kListH_phys = 80;
  kBtnY_phys = clientH - btnH - gap;
}

// ===== v0.19.0.44 (Feature 2: reorder ListView entry via drag) =====

// 设计说明: 没用 custom timer 实现"长按 + drag" (需要 subclass ListView,
// 复杂度高), 改用 ListView 内置 drag-drop (LVN_BEGINDRAG/ENDDRAG)。
// 标准 desktop UX: 用户按下 + 拖动 + 释放 → reordering. 不需长按 500ms
// (ListView DragDetect 阈值 ~4px). 用户装机反馈 "长按拖动" 实现按 desktop
// 标准 click+drag 实现, 长按阈值 (500ms) 后续 PR 可加 subclass 后支持。
//
// 实现路径:
// - LVN_BEGINDRAG (从 OnNotify dispatch): save source index,
//   s_isReorderDragging=true, SetCapture(dialog), ListView_SetInsertMark
// - WM_MOUSEMOVE (reorder drag mode): hit-test → 更新 insert mark
// - LVN_ENDDRAG (从 OnNotify dispatch): commit reorder via CommitReorder
//   helper (reorder m_phrases + FlushSave + PopulateList) + EndReorderDrag
//   reset state

void PhrasesDialog::StartReorderTimer(HWND hList, int itemIdx) {
  // 当前实现未使用 (LVN_BEGINDRAG 路径直接 StartDrag), 保留以备未来
  // long-press subclass 实现
  (void)hList;
  (void)itemIdx;
}

void PhrasesDialog::CancelReorder(HWND hwnd) {
  // LVN_ENDDRAG 路径在 commit 后调用, 清掉 insert mark + reset state
  if (s_hList) {
    ListView_SetInsertMark(s_hList, -1, FALSE);
  }
  if (s_isReorderDragging) {
    ReleaseCapture();
  }
  s_isReorderDragging = false;
  s_reorderDragActive = false;
  s_dragSourceIdx = -1;
  s_dropTargetIdx = -1;
  (void)hwnd;  // unused
}

void PhrasesDialog::BeginReorderDrag(HWND hwnd) {
  // 当前实现未使用 (由 LVN_BEGINDRAG 直接调用), 保留以备 long-press 实现
  (void)hwnd;
}

void PhrasesDialog::EndReorderDrag(HWND hwnd) {
  // LVN_ENDDRAG 路径: release capture + reset state
  // 注: v0.19.0.44 ListView_SetInsertMark(-1, FALSE) 在某些 sandbox/SDK
  // 组合下 hang (CallWindowProc 进 ListView WndProc + GC 死循环 bug?),
  // 改为不在 EndReorderDrag 里清 — CommitReorder 已经 PopulateList 刷新
  // ListView, 插入线自然消失 (PopulateList 重新 ListView_DeleteAllItems +
  // 重新 InsertItem, 旧的 insert mark 被 reset)。
  if (s_isReorderDragging) {
    ReleaseCapture();
  }
  s_isReorderDragging = false;
  s_reorderDragActive = false;
  s_dragSourceIdx = -1;
  s_dropTargetIdx = -1;
  (void)hwnd;  // unused
}

void PhrasesDialog::UpdateReorderDropTarget(HWND hList, int clientY) {
  // dialog client y → ListView client coords → hit-test
  if (!s_isReorderDragging)
    return;
  if (!s_hwnd)
    return;
  POINT pt = {0, clientY};
  MapWindowPoints(s_hwnd, hList, &pt, 1);
  LVHITTESTINFO hit = {};
  hit.pt = pt;
  int hitIdx = ListView_HitTest(hList, &hit);
  int n = ListView_GetItemCount(hList);
  int target;
  if (hitIdx < 0 || n == 0) {
    target = 0;
  } else if (hit.flags & LVHT_ABOVE) {
    target = hitIdx;
  } else if (hit.flags & LVHT_BELOW) {
    target = hitIdx + 1;
  } else {
    target = hitIdx;  // center hit, default above
  }
  if (target < 0)
    target = 0;
  if (target > n)
    target = n;
  if (target != s_dropTargetIdx) {
    s_dropTargetIdx = target;
    ListView_SetInsertMark(hList, target, TRUE);
  }
}

void PhrasesDialog::CommitReorder(HWND hList) {
  if (!s_isReorderDragging)
    return;
  if (s_dragSourceIdx < 0 || s_dragSourceIdx >= (int)m_phrases.size())
    return;
  int src = s_dragSourceIdx;
  int dst = s_dropTargetIdx;
  if (dst == src || dst == src + 1) {
    return;  // no change
  }
  Phrase p = m_phrases[src];
  m_phrases.erase(m_phrases.begin() + src);
  int insertPos = dst;
  if (insertPos > src)
    insertPos--;
  if (insertPos < 0)
    insertPos = 0;
  if (insertPos > (int)m_phrases.size())
    insertPos = (int)m_phrases.size();
  m_phrases.insert(m_phrases.begin() + insertPos, p);
  FlushSave();
  PopulateList(hList);
  // Restore selection to new index
  if (insertPos >= 0 && insertPos < (int)m_phrases.size()) {
    ListView_SetItemState(hList, insertPos, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    m_selectedIndex = insertPos;
  }
}

// 注: v0.19.0.33 (Phase B Bug 2b) commit 8fe29885 删除 RepaintLayered 函数。
