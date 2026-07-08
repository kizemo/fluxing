// spec 050 T003 - HotkeyEditorDialog.h
//
// Visual hotkey binding editor for F3 MVP. Sits in WeaselDeployer.exe
// (launched via menu item "快捷键编辑器") so it can read the live
// deployment default.yaml + write to user_data_dir/default.custom.yaml
// + invoke /deploy all in-process.
//
// MVP scope: only the key_binder/bindings list. No StylePage /
// SchemaPage / UserDictPage (those are spec 051/052/053).

#pragma once

#include "resource.h"
#include <atlbase.h>
#include <atlwin.h>         // MSVC ATL (from VC\\atlmfc\\include)
#include <wtl/atlapp.h>     // WTL extensions (from include\\wtl\\)
#include <wtl/atlcrack.h>
#include <wtl/atlctrls.h>
#include <wtl/atldlgs.h>
#include <string>
#include <vector>
#include "../FluxingConfigEditor/HotkeyBinding.h"

class HotkeyEditorDialog : public ATL::CDialogImpl<HotkeyEditorDialog> {
 public:
  enum { IDD = IDD_HOTKEY_EDITOR };

  BEGIN_MSG_MAP(HotkeyEditorDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    COMMAND_ID_HANDLER(IDOK, OnSave)
    COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    COMMAND_ID_HANDLER(IDC_ADD, OnAdd)
    COMMAND_ID_HANDLER(IDC_EDIT, OnEdit)
    COMMAND_ID_HANDLER(IDC_DELETE, OnDelete)
    COMMAND_ID_HANDLER(IDC_RESET, OnReset)
    COMMAND_ID_HANDLER(IDC_CLEAR, OnClear)
    COMMAND_HANDLER(IDC_FILTER, CBN_SELCHANGE, OnFilterChange)
    NOTIFY_HANDLER(IDC_LIST, LVN_ITEMCHANGED, OnListItemChanged)
    COMMAND_HANDLER(IDC_SEARCH, EN_CHANGE, OnSearchChange)
  END_MSG_MAP()

  HotkeyEditorDialog();
  ~HotkeyEditorDialog();

 private:
  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnSave(WORD, WORD, HWND, BOOL&);
  LRESULT OnCancel(WORD, WORD, HWND, BOOL&);
  LRESULT OnAdd(WORD, WORD, HWND, BOOL&);
  LRESULT OnEdit(WORD, WORD, HWND, BOOL&);
  LRESULT OnDelete(WORD, WORD, HWND, BOOL&);
  LRESULT OnReset(WORD, WORD, HWND, BOOL&);
  LRESULT OnClear(WORD, WORD, HWND, BOOL&);
  LRESULT OnFilterChange(WORD, WORD, HWND, BOOL&);
  LRESULT OnListItemChanged(int, LPNMHDR, BOOL&);
  LRESULT OnSearchChange(WORD, WORD, HWND, BOOL&);

  // File paths
  std::wstring custom_yaml_path_;  // <user_data_dir>/default.custom.yaml
  std::wstring deploy_yaml_path_;  // <install_dir>/data/default.yaml (fallback)

  // State
  std::vector<fluxing::HotkeyBinding> bindings_;
  int filter_index_ = 0;  // 0=all, 1=composing, 2=has_menu, 3=always
  std::wstring search_text_;
  bool dirty_ = false;

  // Controls
  CListViewCtrl list_;
  CComboBox filter_;
  CEdit search_;

  // Helpers
  bool LoadFromYaml();
  bool SaveToYaml();
  void RebuildList();
  void AddOrEditBinding(bool is_edit);  // shared between Add and Edit
  void TriggerDeploy();
  void SetDirty();
  static int CALLBACK ListCompare(LPARAM, LPARAM, LPARAM);
  bool MatchesFilter(const fluxing::HotkeyBinding& b) const;
};