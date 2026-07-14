#pragma once
//
// PhrasesDialog v0.19.0.27 — 常用短语 Modal 对话框 (v2)
// spec: .specify/specs/043-phrases-ui-v2/design.md
//
// 设计 (spec 043):
// - Modal WS_POPUP + WS_EX_LAYERED (per-pixel alpha,跟 QuickPanel 一致风格)
// - 标题栏: 30px 自绘 (L96-AP-G 不再用 WS_CAPTION)
// - 搜索框: 32px EDIT (ES_AUTOHSCROLL) — Ctrl+F 激活,实时 dim 不匹配项
// - 状态栏: 24px STATIC 自绘 — 仅在 YAML 解析失败时显示
// - 树形: SysTreeView32 (TVS_HASBUTTONS | TVS_LINESATROOT) — 沿用 v0.19.0.25
// - 内联编辑 (L96-AP-A): F2 / Ctrl+N 触发,inline edit 控件就地编辑,避免
//   v0.19.0.25 子 dialog + EnableWindow(FALSE) 视觉消失 bug
// - Debounce save (L96-AP-C): 500ms one-shot timer (IDT_SAVE),写盘失败保留 in-memory + toast + 2s 重试 1 次
// - YAML 文件 <user_data>/phrases.yaml,自写 minimal parser,容错 fallback 空 list
// - SendInput + KEYEVENTF_UNICODE 注入,跨 IME 状态工作
// - 键盘: ↑↓ ←→ Enter Esc Tab F2 Delete Ctrl+N/E/F/S (spec §4)
//
// 历史雷区 (不重复犯错,见 lessons-learned L94/L95/L96):
// ❌ 绝对不用 GDI+ (QuickPanel L67-L69 教训)
// ❌ 绝对不在 PhrasesDialog 内写 YAML parser 之外的字符串硬编码颜色 (走 FLUENT-UI-TOKENS §3.6)
// ❌ 绝对不在 WM_KEYDOWN / WndProc 回调中阻塞 I/O (constitution P2) → debounce 解决
// ❌ 绝对不在子 dialog 用 EnableWindow(parent, FALSE) (L96-AP-A)
// ❌ 绝对不在 OnPaint GradientFill 整个 client (L96-AP-B) — 只涂 [0, kTitleH)
// ❌ 绝对不在 Hide() 时忘记 FlushSave() (spec §10 risk)
// ✅ SysTreeView32 是 native Windows UX,chrome 用 Liquid Glass → "modern container,
//   classic content" (spec §12 risk 行)
// ✅ YAML parse 失败 → 空 list + status bar warn,不 crash (spec §4 决策)
// ✅ SendInput 用 KEYEVENTF_UNICODE | KEYEVENTF_KEYUP 对,确保 IME composing 中也能
//   commit (spec §10.1)
// ✅ Hide() 前 KillTimer + FlushSave,保证数据落盘 (spec §10 risk)
//
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <windows.h>
#include <commctrl.h>

// 测试友好的注入函数指针(默认 SendInput,test 可换 mock)
// 在 class 外 using(alias 必须在使用前可见)
using PhrasesInjectFn = void (*)(const std::wstring& text);

class PhrasesDialog {
 public:
  // 兼容别名(老代码引用 PhrasesDialog::InjectFn)
  using InjectFn = PhrasesInjectFn;
  // ===== 单例 API =====
  // Show() 加载 YAML + 创窗口 + PopulateTree + ShowWindow
  // Hide() 先 FlushSave + KillTimer 再 DestroyWindow
  static void Show();
  static void Hide();

  // 设置注入函数(测试用,默认 = &DefaultInject via SendInput)
  static void SetInjectFn(InjectFn fn);

  // 设置 YAML 路径(测试用,默认 = <user_data>/phrases.yaml via RimeGetUserDataDir)
  static void SetYamlPath(const std::wstring& path);

  // ===== 公开访问 (供 Track 3 / 测试) =====
  // 数据读写(Add/Edit/Delete 用,测试可绕开 GUI 直接改)
  struct Phrase {
    std::wstring text;
    std::wstring category;  // "" = uncategorized
  };
  static std::vector<Phrase>& MutablePhrases();
  static const std::vector<Phrase>& Phrases();

  // YAML I/O 公开(测试用)
  static bool LoadPhrases(const std::wstring& path, std::vector<Phrase>& out);
  static bool SavePhrases(const std::wstring& path, const std::vector<Phrase>& data);

  // 默认注入(直接调 SendInput)
  static void DefaultInject(const std::wstring& text);

  // 内部:tree populate(测试可用 mock HDC 验证 item 数量)
  static int PopulateTreeCount(HWND hTree);

  // v0.19.0.27 测试/调试: 应用搜索过滤(对 tree 应用 TVIS_CUT dim 50%)
  // searchQuery 为空时清除 dim。返回受影响 item 数(测试可达)。
  static int ApplySearchFilter(const std::wstring& searchQuery);

  // v0.19.0.27 测试/调试: 调度 debounce 写盘(500ms one-shot)。真实写入由 WM_TIMER
  // IDT_SAVE 触发;test 也可直接调 FlushSave 跳过 debounce。
  static void ScheduleSave();
  static void FlushSave();

  // v0.19.0.27 测试/调试: 显示 toast(右下角,3000ms 自动消失)+ HideToast。
  static void ShowToast(const std::wstring& text);
  static void HideToast();
  static bool IsToastVisible();

  // v0.19.0.27 测试/调试: 触发 inline edit (F2/Ctrl+N/Ctrl+E),进入 Editing 状态。
  // newText/newCategory 空 = 新增模式,非空 = 编辑模式。返回是否成功。
  // (测试可达;真实 WndProc 通过 WM_COMMAND/TVN_BEGINLABELEDIT 进入)
  static bool BeginInlineEdit(int phraseIndex, const std::wstring& newText,
                              const std::wstring& newCategory);

  // State enum (spec §3): Hidden / Loading / Browsing / Search / Editing
  enum State { State_Hidden, State_Loading, State_Browsing, State_Search, State_Editing };
  static State GetState() { return s_state; }

  // WndProc
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND   s_hwnd;
  static HWND   s_hTree;
  static HWND   s_hSearch;       // v0.19.0.27: 搜索框 EDIT
  static HWND   s_hStatus;       // v0.19.0.27: 状态栏 STATIC (warn only)
  static HWND   s_hToast;        // v0.19.0.27: toast STATIC (右下角)
  static HWND   s_hBtnAdd;
  static HWND   s_hBtnEdit;
  static HWND   s_hBtnDel;
  static HWND   s_hBtnCancel;
  static HWND   s_hBtnSave;      // v0.19.0.27: 编辑态动态显示的保存按钮
  static std::wstring s_yamlPath;
  static InjectFn      s_injectFn;

  // 折叠状态:key = category(L"" = 未分类)
  static std::unordered_map<std::wstring, bool> m_expanded;

  // v0.19.0.27: 当前 state (spec §3 state machine)
  static State s_state;

  // v0.19.0.27: 当前 inline edit 索引 (m_phrases 内; -1 = 新增模式)
  static int m_editingIndex;
  // v0.19.0.27: inline edit 控件(EDIT for text, EDIT for category)
  static HWND s_hEditText;
  static HWND s_hEditCat;

  // v0.19.0.27: 当前 search query (lowercased, 测试可达)
  static std::wstring s_searchQuery;
  // v0.19.0.27: dirty flag (m_phrases 修改过但未 flush,test 可查)
  static bool m_dirty;
  // v0.19.0.27: 上次写盘失败时间(GetTickCount);0 = 无失败
  static DWORD s_lastSaveFailTick;

  // v0.19.0.26-cleanup(issue 2-cleanup):grace 计时 — 移植 QuickPanel L94 (kShowGraceMs=2000)。
  // Show() 末尾设 s_showTime,WM_ACTIVATEAPP/WM_KILLFOCUS grace 内不立即 Hide。
  static constexpr DWORD kShowGraceMs = 2000;
  static DWORD s_showTime;       // Show() 时刻(GetTickCount),auto-hide grace

  // v0.19.0.27 (spec §7.5): 时间常量
  static constexpr DWORD kSaveDebounceMs = 500;  // IDT_SAVE debounce
  static constexpr DWORD kToastMs        = 3000; // IDT_TOAST 显示
  static constexpr DWORD kRetryMs        = 2000; // 写盘失败重试

  // v0.19.0.27: Timer IDs
  static constexpr UINT_PTR IDT_SAVE  = 9001;
  static constexpr UINT_PTR IDT_TOAST = 9002;

  // v0.19.0.26-cleanup(issue 1-cleanup):HFONT 静态持有,OnDestroy 释放。
  static HFONT s_hFontUi;

  // v0.19.0.26-fix(issue 2a):Tree 高度自适应,避免 378 > 370 撞 button。
  // v0.19.0.27: kDialogH 460 + 搜索框 + 状态栏纳入公式。
  // 运行时:kTreeH_phys = kDialogH - kTitleH - kSearchH - kStatusBarH - kBtnH - 5*kGap
  static int kTreeH_phys;        // 物理 tree 高度,OnCreate 末尾算
  static int kBtnY_phys;         // 物理 button 起点 y

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
  static LRESULT OnTimer(HWND, WPARAM);
  static LRESULT OnCtlColor(HWND, WPARAM, LPARAM);

  // Tree populate / rebuild
  static void PopulateTree(HWND hTree);
  // 把 m_phrases push 到 TreeView,返回 inserted item 总数(2 cats + 3 + 2 = 7)
  static int  PopulateTreeImpl(HWND hTree);

  // 键盘导航辅助
  // 收集所有"可见"HTREEITEM 的顺序(尊重 m_expanded),返回 vector
  static std::vector<HTREEITEM> CollectVisibleItems(HWND hTree);
  // 移动选中 (dir = +1 / -1);返回新选中
  static HTREEITEM MoveSelection(HWND hTree, int dir);
  // 处理 ←/→ on category / phrase
  static void HandleLeftRight(HWND hTree, bool right);
  // 处理 Enter
  static void HandleEnter(HWND hTree);

  // v0.19.0.27: inline edit helpers
  static void EnterEditingState(int phraseIndex, bool isAdd);
  static void ExitEditingState(bool save);

  // v0.19.0.27: status bar
  static void ShowStatusWarning(const std::wstring& text);

  // v0.19.0.27: button row 切换 (Browsing = 4 buttons, Editing = Save/Cancel)
  static void SetButtonsForBrowsing();
  static void SetButtonsForEditing();

  // 工具
  static void InjectText(const std::wstring& text);
  static void CenterOnPrimaryMonitor(HWND hwnd, int w, int h);

  // 应用数据(in-memory;Show() 时 Load,YAML 路径不变)
  static std::vector<Phrase> m_phrases;
  static int m_selectedIndex;  // 当前选中 phrase 在 m_phrases 里的 index;-1 = 分类
};
