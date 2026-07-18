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

// 测试友好的注入函数指针(默认 SendInput,test 可换 mock)
using PhrasesInjectFn = void (*)(const std::wstring& text);

class PhrasesDialog {
 public:
  // 兼容别名
  using InjectFn = PhrasesInjectFn;

  // ===== 单例 API =====
  static void Show();
  static void Hide();

  // 设置注入函数(测试用)
  static void SetInjectFn(InjectFn fn);

  // 设置 YAML 路径(测试用)
  static void SetYamlPath(const std::wstring& path);

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
  static bool SavePhrases(const std::wstring& path, const std::vector<Phrase>& data);

  // 默认注入
  static void DefaultInject(const std::wstring& text);

  // v0.19.0.32 测试/调试: 调度立即写盘(测试可达,无 debounce)。
  static void FlushSave();

  // v0.19.0.32 测试/调试: list populate item 数(测试可达)。
  static int PopulateListCount(HWND hList);

  // State enum (v0.19.0.32 简化): Hidden / Browsing
  enum State { State_Hidden, State_Browsing };
  static State GetState() { return s_state; }

  // WndProc
  static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

  // ===== Static state =====
  static HWND   s_hwnd;
  // v0.19.0.32 UX redo:
  // - s_hInput: 顶部单 input (替代 inline-edit + 旧 search box)
  // - s_hBtnAddTop: 顶部 Add 按钮 (跟 s_hInput 同行右侧)
  // - s_hList: ListView (替代 TreeView)
  // - s_hBtnAdd / s_hBtnEdit / s_hBtnDel / s_hBtnCancel: 底部 4 按钮
  // 删:s_hSearch / s_hTree / s_hStatus / s_hToast / s_hBtnSave /
  //    s_hEditText / s_hEditCat
  static HWND   s_hInput;
  static HWND   s_hBtnAddTop;
  static HWND   s_hList;
  static HWND   s_hBtnAdd;
  static HWND   s_hBtnEdit;
  static HWND   s_hBtnDel;
  static HWND   s_hBtnCancel;

  static std::wstring s_yamlPath;
  static InjectFn      s_injectFn;

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
  static int kListH_phys;        // 物理 list 高度
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
  static LRESULT OnCtlColor(HWND, WPARAM, LPARAM);

  // List populate
  static void PopulateList(HWND hList);
  static int  PopulateListImpl(HWND hList);

  // 工具
  static void InjectText(const std::wstring& text);
  static void CenterOnPrimaryMonitor(HWND hwnd, int w, int h);

  // 应用数据(in-memory)
  static std::vector<Phrase> m_phrases;
};
