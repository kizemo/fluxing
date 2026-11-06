// SogouImportDialog.h — 搜狗用户词库导入对话框。
//
// 流程：
//   1. 扫描搜狗的两个用户数据目录（普通态 / 账号态），列出可导入的词库来源；
//   2. 用户选择来源与目标用户词典；
//   3. 解析词条 → 用随包 cn_dicts 反查无调全拼 → 生成 TSV；
//   4. 调用 RimeLeversApi::import_user_dict 写入目标词典。
//
// 对话框在 Configurator::SogouImport() 建立的维护区间内弹出，
// 因此 import_user_dict 调用一定落在 StartMaintenance/EndMaintenance
// 之间（librime leveldb 独占锁要求，见 ADR 0032）。

#pragma once

#include "resource.h"

#include <rime_levers_api.h>

#include <string>
#include <vector>

class SogouImportDialog : public CDialogImpl<SogouImportDialog> {
 public:
  enum { IDD = IDD_SOGOU_IMPORT };

  SogouImportDialog();
  ~SogouImportDialog();

 protected:
  BEGIN_MSG_MAP(SogouImportDialog)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDC_SOGOU_IMPORT_BTN, OnImport)
  COMMAND_HANDLER(IDC_SOGOU_SOURCE_LIST, LBN_SELCHANGE, OnSourceSelChange)
  COMMAND_HANDLER(IDC_SOGOU_TARGET_DICT, CBN_SELCHANGE, OnTargetSelChange)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnImport(WORD, WORD, HWND, BOOL&);
  LRESULT OnSourceSelChange(WORD, WORD, HWND, BOOL&);
  LRESULT OnTargetSelChange(WORD, WORD, HWND, BOOL&);

  void PopulateSources();
  void PopulateTargets();
  void UpdateButtons();

  // 一个可导入的搜狗词库来源。
  struct Source {
    std::wstring path;       // .bin 完整路径
    std::wstring label;      // 显示用文本（目录 + 文件名 + 大小）
    size_t entry_count = 0;  // 预估词条数
  };

  std::vector<Source> sources_;
  CListBox source_list_;
  CComboBox target_dict_;
  // 与 target_dict_ 下拉项一一对应的词典名（UTF-8）。
  // 不从组合框读回文本：WTL CComboBox 的取文本接口在版本间命名不一致，
  // 这里直接用填充时已知的原始名字，行为确定且不依赖控件 API。
  std::vector<std::string> target_dict_names_;
  CButton import_button_;
  CStatic status_;
  RimeLeversApi* api_ = nullptr;
};
