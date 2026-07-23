#pragma once
//
// PhrasesDialog v0.19.0.32 — 常用短语 Modal 对话框 (v3, UX redo)
//
// 设计 (User report-driven, spec 044 follow-up):
// - 顶部 input + 右侧 Add 按钮 (同 row) — 录入后按 Add 进 list
// - 中间 ListView (单列 "phrase text") — 替代 TreeView + inline-edit
// - 底部 4 按钮: Add / Edit / Delete / Cancel (Save 合并到 Edit 流程)
// - 选 list item → 自动 fill 顶部 input → 直接改 → 点 Edit
// - 删 v0.19.0.30 inline-edit overlay (BeginInlineEdit / s_hEditText /
//   s_hEditCat / m_editingIndex / EnterEditingState / ExitEditingState /
//   SetButtonsForBrowsing/Editing)— 移除 2 个 EDIT 堆叠遮挡 listview bug
//
// 历史雷区 (不重复犯错):
// ❌ 绝对不在 PhrasesDialog 内写 YAML parser 之外的字符串硬编码颜色
// ❌ 绝对不在 WM_KEYDOWN / WndProc 回调中阻塞 I/O (P2)
// ❌ 绝对不在 Hide() 时忘记 FlushSave (spec §10 risk)
// ✅ SendInput 用 KEYEVENTF_UNICODE | KEYEVENTF_KEYUP 对,IME composing 中
//   也能 commit
// ✅ Hide() 前 FlushSave,保证数据落盘
// ✅ User flow 简化: Phrase struct 只保留 text 字段 (单字段简化)
//   YAML 兼容旧 text/category 字段 (load 时读 + 丢弃 category, save 只写 text)
//
#include <string>
#include <vector>
#include <windows.h>
#include <commctrl.h>

// 前向声明 (避免 PipeClient 头文件污染)
namespace fluxing {
class PipeClient;
}

// 测试友好的注入函数指针(默认 SendInput,test 可换 mock)
using PhrasesInjectFn = void (*)(const std::wstring& text);

class PhrasesDialog {
 public:
  // 兼容别名
  using InjectFn = PhrasesInjectFn;

  // ===== 单例 API =====
  static void Show();
  static void Hide();

  // v0.19.0.55 (Phase K3 T019 Bug 3 修): "Hide 但不 Disconnect pipe" —
  //   NM_DBLCLK / NM_RETURN / VK_RETURN 路径用: 先 Hide (DestroyWindow 同步
  //   归还 foreground 给 user app), 再通过残留 pipe 连接发 INJECT + 读 ACK。
  //   然后再调普通 Hide 收尾 (会 Disconnect pipe)。
  //   跟 Hide() 区别: 跳过 s_pipeClient->Disconnect() (保留 pipe 让后续
  //   INJECT 能 send)。
  static void HideWithoutDisconnect();

  // 设置注入函数(测试用)
  static void SetInjectFn(InjectFn fn);

  // 设置 YAML 路径(测试用)
  static void SetYamlPath(const std::wstring& path);

  // Phase K2: 设置 pipe 名称 (从命令行接收)
  static void SetPipeName(const std::wstring& pipeName);
  static const std::wstring& GetPipeName();

  // ===== 公开访问 (供 Track 3 / 测试) =====
  // v0.19.0.35 (Phase C 恢复 v0.19.0.25 3f2c1694 + v0.19.0.28 c6fec000):
  // Phrase struct 加回 category 字段 (v0.19.0.32 cd6f61a9 UX redo 误删)。
  // TreeView 显示分类 (左侧), ListView 显示该分类下的 phrase (右侧)。
  struct Phrase {
    std::wstring text;
    std::wstring category;  // 空字符串 = 无分类
  };
  static std::vector<Phrase>& MutablePhrases();
  static const std::vector<Phrase>& Phrases();

  // YAML I/O 公开(测试用)
  static bool LoadPhrases(const std::wstring& path, std::vector<Phrase>& out);
  static bool SavePhrases(const std::wstring& path,
                          const std::vector<Phrase>& data);

  // 默认注入
  static void DefaultInject(const std::wstring& text);

  // v0.19.0.32 测试/调试: 调度立即写盘(测试可达,无 debounce)。
  static void FlushSave();

  // v0.19.0.32 测试/调试: list populate item 数(测试可达)。
  static int PopulateListCount(HWND hList);

  // v0.19.0.39 (Phase F fix): foreground API mock 支持 (Test 23 RED 测试)
  // 真 root cause (装机反馈 Bug 1 + Bug 3): QuickPanel button 启动
  // PhrasesDialog 时,
  //   QuickPanelDialog 仍是 foreground, 键盘事件不传 PhrasesDialog. 修法:
  //   Show() 创建路径 调 AllowSetForegroundWindow + SetForegroundWindow 强制抢
  //   foreground. test 可注入 mock 函数指针, 验证调用次数 + 参数 (不真调 OS
  //   API).
  using SetForegroundFn = BOOL(WINAPI*)(HWND);
  using AllowSetForegroundFn = BOOL(WINAPI*)(DWORD);
  static void SetSetForegroundFn(SetForegroundFn fn);
  static void SetAllowSetForegroundFn(AllowSetForegroundFn fn);
  static SetForegroundFn s_setForegroundFn;
  static AllowSetForegroundFn s_allowSetForegroundFn;

  // State enum (v0.19.0.32 简化): Hidden / Browsing
  enum State { State_Hidden, State_Browsing };
  static State GetState() { return s_state; }

  // WndProc
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND s_hwnd;
  // v0.19.0.32 UX redo → v0.19.0.41 修正:
  // - s_hInput: 顶部单 input (替代 inline-edit + 旧 search box)
  // - s_hBtnAddTop: 顶部 Add 按钮 (跟 s_hInput 同行右侧) — 唯一 Add
  // - s_hList: ListView (替代 TreeView)
  // - s_hBtnEdit / s_hBtnDel / s_hBtnCancel: 底部 3 按钮
  //   (v0.19.0.41 删 s_hBtnAdd 重复 — 跟 s_hBtnAddTop 功能一样)
  // 删:s_hSearch / s_hTree / s_hStatus / s_hToast / s_hBtnSave /
  //    s_hEditText / s_hEditCat / s_hBtnAdd (v0.19.0.41)
  static HWND s_hInput;
  static HWND s_hBtnAddTop;
  static HWND s_hList;
  static HWND s_hBtnEdit;
  static HWND s_hBtnDel;
  static HWND s_hBtnCancel;

  static std::wstring s_yamlPath;
  static InjectFn s_injectFn;

  // 当前选中 list item 的 m_phrases index;-1 = 无选中
  static int m_selectedIndex;

  // v0.19.0.32: 简化 state (删 Loading / Search / Editing)
  static State s_state;

  // v0.19.0.32 UX redo: 删 m_editingIndex / s_searchQuery / m_dirty /
  //   s_lastSaveFailTick / s_hFontUi / m_expanded (cat 字段一并删除)

  // v0.19.0.32: grace 计时,沿用 QuickPanel L94 (kShowGraceMs=2000)。
  static constexpr DWORD kShowGraceMs = 2000;
  static DWORD s_showTime;

  // v0.19.0.32: HFONT 静态持有,OnDestroy 释放
  static HFONT s_hFontUi;

  // 物理几何常量 (跟 v0.19.0.27 一致,但 tree 高度改为 list 高度)
  // v0.19.0.41: 这些 constexpr 不再被直接使用 (DPI 缩放后), OnCreate 计算
  // 实际尺寸时用 s_dpiScale 乘。保留作为 base / debug 锚点。
  static int kListH_phys;  // 物理 list 高度 (DPI-scaled 之后)
  static int kBtnY_phys;   // 物理 button 起点 y (DPI-scaled 之后)

  // v0.19.0.41 (Bug 2: UI 过小): DPI 缩放因子
  // GetDpiForWindow(hwnd) 返回当前 dialog 所在 monitor 的 DPI; 96 是 base。
  // 4K 屏 200% 缩放 → s_dpiScale = 2.0 → 所有物理像素 ×2 (kDialogW 360 → 720)
  // Default = 1.0 (96 DPI), test 走 default。
  static double s_dpiScale;

  // v0.19.0.41 (Feature: 长按拖动位置): drag state
  // s_longPressActive = timer 启动了还没 fire
  // s_isDragging = timer fired + 鼠标移动 → 进入 drag mode
  // s_dragOrigin = LBUTTONDOWN 时的鼠标位置 (屏幕坐标)
  // s_dragWndOrigin = LBUTTONDOWN 时的 window 位置 (屏幕坐标)
  static bool s_longPressActive;
  static bool s_isDragging;
  static POINT s_dragOrigin;
  static POINT s_dragWndOrigin;
  static constexpr UINT_PTR kLongPressTimerId = 9001;  // WndProc WM_TIMER
  static constexpr DWORD kLongPressMs = 500;           // 长按阈值

  // v0.19.0.44 (Feature: 长按拖动 reorder ListView entry):
  // s_reorderDragActive = timer 启动了还没 fire
  // s_isReorderDragging = timer fired + 鼠标移动 → 进入 reorder drag mode
  // s_dragSourceIdx = LBUTTONDOWN 时的 ListView item index (-1 = no drag)
  // s_dropTargetIdx = 当前 insertion position (LVM_SETINSERTMARK 用)
  static bool s_reorderDragActive;
  static bool s_isReorderDragging;
  static int s_dragSourceIdx;
  static int s_dropTargetIdx;
  static constexpr UINT_PTR kReorderTimerId = 9002;  // 同长按 500ms

  // v0.19.0.41 (Feature: 鼠标拖动边界 resize): min/max 物理尺寸 (DPI-scaled)
  // v0.19.0.44: 调小 min 让 user 缩小范围更大 (240×360 能装 input + 1 个 list
  // row + 3 buttons)
  static constexpr int kMinW = 240;
  static constexpr int kMinH = 360;
  static constexpr int kMaxW = 1200;
  static constexpr int kMaxH = 900;

  // v0.19.0.36 (Phase D): ListView selected index move helper (delta=-1/+1)
  // v0.19.0.36 (P2 follow-up): 移到 public — unit test (Test 19) 需直接断言
  //   wrap-around 行为 (ListView 默认 WndProc 不 wrap, 这是 MoveSelection
  //   单独提供)
  static void MoveSelection(HWND hList, int delta);

  // v0.19.0.41 (Feature: resize + long-press drag) + v0.19.0.42 (Stop hook fix
  // 暴露给 test):
  // - OnNcHitTest: 让 chrome 区域 (非子控件) 报 HTCAPTION 给 Windows
  //   (用于 cursor 反馈, 实际 drag 由 long press 触发)
  // - OnLButtonDown/Up/MouseMove: 长按 drag state machine
  // - OnTimer: 长按 500ms 后 fire → 进入 drag mode
  // - OnGetMinMaxInfo: 限制 resize 范围
  // (public 让 test/TestPhrasesDialog e2e 调 — sandbox 不能等 500ms timer)
  static LRESULT OnNcHitTest(HWND, LPARAM);
  static LRESULT OnLButtonDown(HWND, WPARAM, LPARAM);
  static LRESULT OnLButtonUp(HWND, WPARAM, LPARAM);
  static LRESULT OnMouseMove(HWND, WPARAM, LPARAM);
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnGetMinMaxInfo(HWND, LPARAM);

  // v0.19.0.41: 长按 drag 内部 helper (public 给 test)
  static void StartLongPressTimer(HWND hwnd, int x, int y);
  static void CancelLongPress(HWND hwnd);
  static void BeginDrag(HWND hwnd);
  static void EndDrag(HWND hwnd);
  // v0.19.0.57 (Phase K4 Bug 2 修): DRY helper — capture drag origin
  //   (cursor screen + window rect), BeginDrag + StartLongPressTimer 共用。
  static void CaptureDragOrigin(HWND hwnd);

  // v0.19.0.58 (Phase K5 Bug 3+4 真修): Subclass s_hInput (Edit control)
  //   WndProc 转发 WM_KEYDOWN VK_RETURN / VK_ESCAPE 到 PhrasesDialog parent.
  //   真因(Phase K5 root cause): WS_POPUP + main.cpp 简单 message loop (无
  //   IsDialogMessage) → 子控件 focus 时 WM_KEYDOWN 不 bubble 到 PhrasesDialog
  //   WndProc → OnKeyDown handler 失效 → Edit focus + Enter/Esc 无任何效果。
  //   Test 40/41 模拟 `SetFocus(s_hInput)` 显式 set 时 PASS(走 SendMessageW
  //   WM_KEYDOWN 直接派发到 dialog WndProc,绕过 input 子控件 focus 路由),
  //   但装机端 user 不显式 set focus → click input 打字 → Edit focus → Enter
  //   走 Edit WndProc default no-op → OnKeyDown 不到 → 修法失效。
  //   修法: SetWindowLongPtr(s_hInput, GWL_WNDPROC, InputSubclassProc) hook
  //   WM_KEYDOWN VK_RETURN / VK_ESCAPE → SendMessage(parent, WM_KEYDOWN, ...)
  //   → OnKeyDown 触发。Install / Remove helpers 给 test 验证(Test 43/44)。
  //   注: WndProc 是 free function (namespace-scope), 不能是 class member。
  static void InstallInputSubclass(HWND hInput);
  static void RemoveInputSubclass(HWND hInput);

  // v0.19.0.44 (Feature 1: resize layout): 重新定位所有 child
  // 入参 client area 物理尺寸 (DPI-scaled)。OnCreate 初始化 + WM_SIZE 触发
  // + 测试可直接调验证。
  static void LayoutDialog(int clientW, int clientH);

  // v0.19.0.44 (Feature 2: 长按 reorder ListView entry): 内部 helper
  // StartReorderTimer: LVN_BEGINDRAG 时设 timer, 500ms 后 fire → drag mode
  // UpdateReorderDropTarget: WM_MOUSEMOVE 计算当前 hit-test y 位置
  // CommitReorder: WM_LBUTTONUP reorder m_phrases + FlushSave + PopulateList
  static void StartReorderTimer(HWND hList, int itemIdx);
  static void CancelReorder(HWND hwnd);
  static void BeginReorderDrag(HWND hwnd);
  static void EndReorderDrag(HWND hwnd);
  static void UpdateReorderDropTarget(HWND hList, int clientY);
  static void CommitReorder(HWND hList);

 private:
  // 内部 helpers(测试可达)
  static std::wstring Trim(const std::wstring& s);
  static std::wstring Unquote(const std::wstring& s);

  // WndProc 分发
  static LRESULT OnCreate(HWND);
  static LRESULT OnDestroy(HWND);
  static LRESULT OnPaint(HWND);
  static LRESULT OnKeyDown(HWND, WPARAM);
  static LRESULT OnNotify(HWND, LPARAM);
  static LRESULT OnCommand(HWND, WPARAM);
  static LRESULT OnCtlColor(HWND, WPARAM, LPARAM);

  // v0.19.0.41 (Feature: resize + long-press drag) — handler 声明移到上面
  // public 段 (Test 30 e2e 调用), 这里是空的占位注释

  // List populate
  static void PopulateList(HWND hList);
  static int PopulateListImpl(HWND hList);

  // 工具
  static void InjectText(const std::wstring& text);
  static void CenterOnPrimaryMonitor(HWND hwnd, int w, int h);

  // Phase K2 (v0.19.0.51): pipe IPC
  static std::wstring s_pipeName;
  static fluxing::PipeClient* s_pipeClient;

  // 应用数据(in-memory)
  static std::vector<Phrase> m_phrases;
};
