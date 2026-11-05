#pragma once
#include <WeaselIPCData.h>
#include <unordered_set>
#include <string>
// 1.1.5 (spec 090): 候选屏蔽名单的加载 / 过滤 / 可见索引→引擎索引映射。
// header-only，只依赖 windows.h + STL。
#include <FluxingCandidateIgnore.h>
#include <WeaselUI.h>
#include "StandardLayout.h"
#include "Layout.h"
#include "GdiplusBlur.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using namespace weasel;

typedef CWinTraits<WS_POPUP | WS_CLIPSIBLINGS | WS_DISABLED,
                   WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                       WS_EX_LAYERED>
    CWeaselPanelTraits;

enum class BackType {
  TEXT = 0,
  CAND = 1,
  BACKGROUND = 2  // background
};

class WeaselPanel
    : public CWindowImpl<WeaselPanel, CWindow, CWeaselPanelTraits>,
      CDoubleBufferImpl<WeaselPanel> {
 public:
  BEGIN_MSG_MAP(WeaselPanel)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_MOUSEACTIVATE, OnMouseActivate)
  MESSAGE_HANDLER(WM_LBUTTONUP, OnLeftClickedUp)
  MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLeftClickedDown)
  MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
  MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
  MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
  MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  CHAIN_MSG_MAP(CDoubleBufferImpl<WeaselPanel>)
  END_MSG_MAP()

  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDpiChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnMouseActivate(UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL& bHandled);
  LRESULT OnLeftClickedUp(UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          BOOL& bHandled);
  LRESULT OnLeftClickedDown(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled);
  LRESULT OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnMouseLeave(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnSettingChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

  WeaselPanel(weasel::UI& ui);
  ~WeaselPanel();

  void MoveTo(RECT const& rc);
  void SetDeleteCandidateCallback(std::function<void(size_t)> const& func) {
    m_deleteCallback = func;
  }
  // spec 094: 暴露右键删词回调的只读视图,供 TestWeaselIPC 断言
  // 「面板拿到的是 UI 上那一个 std::function 对象」(引用绑定,不是值拷贝)。
  // 判别性:若有人把 m_deleteCallback 改回值成员,这里的地址就不再等于
  // UI 上的那个地址,断言立刻失败。
  const std::function<void(size_t)>& deleteCandidateCallback() const {
    return m_deleteCallback;
  }
  void LoadIgnoreList(const std::wstring& schema_id);
  void Refresh();
  void DoPaint(CDCHandle dc);
  bool GetIsReposition() { return m_istorepos; }
  void RedrawWindow();

  static VOID CALLBACK OnTimer(_In_ HWND hwnd,
                               _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent,
                               _In_ DWORD dwTime);
  static const int AUTOREV_TIMER = 20240315;
  static UINT_PTR ptimer;

 private:
  template <typename T>
  int DPI_SCALE(T t) {
    return (int)(t * dpiScaleLayout);
  }
  void _InitFontRes(bool forced = false);
  void _CaptureRect(CRect& rect);
  void _IgnoreCurrentCandidate();
  // spec 100 / L186: 按点击坐标命中候选索引, 口径与 OnMouseMove 一致。
  // 必须存在: m_hoverIndex 只在 hover_type != NONE 时被赋值, 出厂默认
  // hover_type: none 下它恒为 -1, 右键删词不能依赖它。
  int _HitTestCandidateIndex(CPoint point);
  void _FilterIgnoredCandidates();
  void _RefreshStylePalette();
  std::unordered_set<std::wstring> m_ignoreList;
  std::wstring m_ignoreFilePath;
  bool m_mouse_entry = false;
  CPoint m_lastMousePos = {-1, -1};
  void _CreateLayout();
  void _ResizeWindow();
  void _RepositionWindow(const bool& adj = false);
  bool _DrawPreedit(const Text& text, CDCHandle dc, const CRect& rc);
  bool _DrawPreeditBack(const Text& text, CDCHandle dc, const CRect& rc);
  bool _DrawCandidates(CDCHandle& dc, bool back = false);
  void _HighlightText(CDCHandle& dc,
                      const CRect& rc,
                      const COLORREF& color,
                      const COLORREF& shadowColor,
                      const int& radius,
                      const BackType& type,
                      const IsToRoundStruct& rd,
                      const COLORREF& bordercolor);
  void _TextOut(const CRect& rc,
                const std::wstring& psz,
                const size_t& cch,
                const int& inColor,
                IDWriteTextFormat1* const pTextFormat = NULL);

  void _LayerUpdate(const CRect& rc, CDCHandle dc);

  weasel::Layout* m_layout;
  weasel::Context& m_ctx;
  weasel::Context& m_octx;
  weasel::Status& m_status;
  weasel::UIStyle& m_style;
  weasel::UIStyle& m_ostyle;
  const bool& m_in_server;

  CRect m_inputPos;
  int m_offsetys[MAX_CANDIDATES_COUNT];  // offset y for candidates when
                                         // vertical layout over bottom
  int m_offsety_preedit;
  int m_offsety_aux;
  bool m_istorepos;

  CIcon m_iconDisabled;
  CIcon m_iconEnabled;
  CIcon m_iconAlpha;
  CIcon m_iconFull;
  CIcon m_iconHalf;
  std::wstring m_current_zhung_icon;
  std::wstring m_current_ascii_icon;
  std::wstring m_current_half_icon;
  std::wstring m_current_full_icon;
  // for gdiplus drawings
  Gdiplus::GdiplusStartupInput _m_gdiplusStartupInput;
  ULONG_PTR _m_gdiplusToken;

  UINT dpi;

  CRect rcw;
  BYTE m_candidateCount;
  BYTE m_lastCandidateCount;

  bool hide_candidates;
  bool m_sticky;
  // for multi font_face & font_point
  PDWR pDWR;
  std::function<void(size_t* const, size_t* const, bool* const, bool* const)>&
      _UICallback;
  // spec 094: 引用绑定,不是值拷贝。
  //
  // 过去这里是 `std::function<void(size_t)> m_deleteCallback;`(值),而
  // WeaselPanel 的构造初始化列表只写了 _UICallback(ui.uiCallback()),
  // 没有给 m_deleteCallback 赋初值 => 值成员默认构造为空 =>
  // OnRButtonDown 里的 `if (m_deleteCallback)` 恒假 => 右键删词整条链路
  // 是死的(spec 094 bug 2 的根因)。
  //
  // 改成引用后,面板拿到的就是 UI 上那一个 std::function 对象,与同文件里
  // 已经生效的 _UICallback 引用模式完全同构:无论
  // UI::SetDeleteCandidateCallback 在面板创建之前还是之后调用,面板都能
  // 看到最新值。
  std::function<void(size_t)>& m_deleteCallback;
  float bar_scale_ = 1.0;
  float dpiScaleLayout = 1.0f;
  int m_hoverIndex = -1;
  HMONITOR m_hMonitor = NULL;
  bool m_redraw_by_monitor_change = false;
};
