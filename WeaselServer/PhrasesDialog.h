#pragma once
//
// PhrasesDialog v0.19.0.25 — 常用短语 Modal 对话框
//
// 设计: .specify/specs/042-phrases-ui/spec.md
// - Modal WS_POPUP + WS_EX_LAYERED (per-pixel alpha,跟 QuickPanel 一致风格)
// - 内嵌 Windows 原生 SysTreeView32 (TVS_HASBUTTONS | TVS_LINESATROOT),
//   树形分类 UX,节省 ~400 行自绘代码(spec §4 "原生优先")
// - YAML 文件 <user_data>/phrases.yaml,自写 minimal parser,容错 fallback 空 list
// - SendInput + KEYEVENTF_UNICODE 注入,跨 IME 状态工作
// - 键盘: ↑↓ ←→ Enter Esc Tab 全实现 (spec §8)
// - Add/Edit 弹子 dialog (新 CreateWindow);Delete 直接删
//
// 历史雷区 (不重复犯错):
// ❌ 绝对不用 GDI+ (QuickPanel L67-L69 教训)
// ❌ 绝对不在 PhrasesDialog 内写 YAML parser 之外的字符串硬编码颜色 (走 FLUENT-UI-TOKENS,
//   spec 040 锁定)
// ❌ 绝对不在 WM_KEYDOWN 中阻塞 I/O (constitution P2)
// ✅ SysTreeView32 是 native Windows UX,chrome 用 Liquid Glass → "modern container,
//   classic content" (spec §12 risk 行)
// ✅ YAML parse 失败 → 空 list + log,不 crash (spec §4 决策)
// ✅ SendInput 用 KEYEVENTF_UNICODE | KEYEVENTF_KEYUP 对,确保 IME composing 中也能
//   commit (spec §10.1)
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
  // Hide() 销毁窗口
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

  // WndProc
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND   s_hwnd;
  static HWND   s_hTree;
  static HWND   s_hBtnAdd;
  static HWND   s_hBtnEdit;
  static HWND   s_hBtnDel;
  static HWND   s_hBtnCancel;
  static std::wstring s_yamlPath;
  static InjectFn      s_injectFn;

  // 折叠状态:key = category(L"" = 未分类)
  static std::unordered_map<std::wstring, bool> m_expanded;

  // v0.19.0.26-cleanup(issue 2-cleanup):grace 计时 — 移植 QuickPanel L94 (kShowGraceMs=2000)。
  // Show() 末尾设 s_showTime,WM_ACTIVATEAPP/WM_KILLFOCUS grace 内不立即 Hide。
  // 移除原 no-op polling timer (cleanup item 2):modal dialog 设计就 Esc/X/Cancel/Enter 关,
  // 不用 polling,见 lessons-learned "polling 是 no-op"。
  // kShowGraceMs 走 design-md "show 出来能看一会儿" 期望。
  static constexpr DWORD kShowGraceMs = 2000;
  static DWORD s_showTime;       // Show() 时刻(GetTickCount),auto-hide grace

  // v0.19.0.26-cleanup(issue 1-cleanup):HFONT 静态持有,OnDestroy 释放(原 OnCreate 局部变量
  // 漏 DeleteObject 泄漏 GDI handle)。Segoe UI Variable 失败时 Win32 font mapper
  // 自动 substitute,见 OnCreate 注释。
  static HFONT s_hFontUi;

  // v0.19.0.26-fix(issue 2a):Tree 高度自适应,避免 378 > 370 撞 button。
  // kTreeH 改成 = kDialogH - kTitleH - kBtnH - 3*gap,在 OnCreate 之前算一次。
  // kTreeH 设计常量保留供 test / 其它 reference 使用,运行时用 kTreeH_phys。
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

  // 子 dialog(Add / Edit)
  static void ShowEditDialog(HWND parent, int editIndex /* -1 = add */);
  static LRESULT CALLBACK EditDlgProc(HWND, UINT, WPARAM, LPARAM);

  // 工具
  static void InjectText(const std::wstring& text);
  static void CenterOnPrimaryMonitor(HWND hwnd, int w, int h);

  // 应用数据(in-memory;Show() 时 Load,YAML 路径不变)
  static std::vector<Phrase> m_phrases;
  static int m_selectedIndex;  // 当前选中 phrase 在 m_phrases 里的 index;-1 = 分类
};