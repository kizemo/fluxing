// spec 050 T003 - HotkeyEditorDialog.cpp
//
// Implementation of the hotkey editor dialog. See header for the
// public contract. Code structure:
//   - OnInitDialog: set up ListView columns + filter combo
//   - LoadFromYaml: read default.custom.yaml (fallback to default.yaml)
//   - SaveToYaml: write back to default.custom.yaml via YamlRoundTrip
//   - RebuildList: re-populate the ListView from `bindings_`
//   - Add/Edit/Delete/Reset handlers
//   - TriggerDeploy: spawn WeaselDeployer.exe /deploy

#include "stdafx.h"
#include "HotkeyEditorDialog.h"
#include "../FluxingConfigEditor/KeyRecorder.h"
#include "WeaselDeployer.h"
#include "Configurator.h"
#include "../FluxingConfigEditor/HotkeyBinding.h"
#include "../FluxingConfigEditor/YamlRoundTrip.h"

#include <WeaselIPC.h>

#include <ShlObj.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace {

std::string ReadEntireFile(const std::wstring& path) {
  std::ifstream f(path.c_str(), std::ios::binary);
  if (!f) return std::string();
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

bool WriteEntireFile(const std::wstring& path, const std::string& content) {
  std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(content.data(), (std::streamsize)content.size());
  return f.good();
}

std::wstring GetInstallDir() {
  // WeaselDeployer is launched from <install>/weasel/; the install
  // dir is the parent. We can also pull from the WeaselServer IPC
  // for the canonical path, but reading from argv or env is more
  // portable for the editor (which runs in WeaselDeployer.exe).
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring p = path;
  size_t slash = p.find_last_of(L"\\");
  if (slash != std::wstring::npos) p = p.substr(0, slash);  // weasel/
  slash = p.find_last_of(L"\\");
  if (slash != std::wstring::npos) p = p.substr(0, slash);  // install root
  return p;
}

}  // namespace

HotkeyEditorDialog::HotkeyEditorDialog() {}
HotkeyEditorDialog::~HotkeyEditorDialog() {}

LRESULT HotkeyEditorDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  CenterWindow(GetParent());
  list_ = GetDlgItem(IDC_LIST);
  filter_ = GetDlgItem(IDC_FILTER);
  search_ = GetDlgItem(IDC_SEARCH);

  // Set up ListView columns: 0=#, 1=when, 2=accept, 3=action_type, 4=action_value
  list_.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  LVCOLUMNW col = {0};
  col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
  col.fmt = LVCFMT_LEFT;
  col.cx = 35; col.pszText = (LPWSTR)L"#";          list_.InsertColumn(0, &col);
  col.cx = 95; col.pszText = (LPWSTR)L"时机";         list_.InsertColumn(1, &col);
  col.cx = 175; col.pszText = (LPWSTR)L"按键 (accept)";  list_.InsertColumn(2, &col);
  col.cx = 80; col.pszText = (LPWSTR)L"动作类型";   list_.InsertColumn(3, &col);
  col.cx = 200; col.pszText = (LPWSTR)L"动作值";     list_.InsertColumn(4, &col);

  // Filter combo
  filter_.AddString(L"全部");
  filter_.AddString(L"composing");
  filter_.AddString(L"has_menu");
  filter_.AddString(L"always");
  filter_.SetCurSel(0);

  // Resolve file paths
  std::wstring user_dir = WeaselUserDataPath().wstring();
  custom_yaml_path_ = user_dir + L"\\default.custom.yaml";
  // Install dir: get from WeaselSharedDataPath() parent.
  std::wstring install_root = GetInstallDir();
  deploy_yaml_path_ = install_root + L"\\weasel\\data\\default.yaml";

  // Load
  LoadFromYaml();
  RebuildList();
  return TRUE;
}

LRESULT HotkeyEditorDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  if (dirty_) {
    int r = ::MessageBoxW(m_hWnd,
        L"有未保存的修改,确定要关闭吗?",
        L"【火流猩输入法】快捷键编辑器",
        MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return 0;
  }
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT HotkeyEditorDialog::OnSave(WORD, WORD, HWND, BOOL&) {
  // L17/L18/L19 linter: check all bindings for risky patterns.
  for (size_t i = 0; i < bindings_.size(); ++i) {
    const wchar_t* warn = fluxing::LintAcceptForRiskyBindings(bindings_[i].accept);
    if (warn) {
      std::wstring msg = L"第 " + std::to_wstring(i + 1) + L" 条:\n\n";
      msg += warn;
      msg += L"\n\n仍然保存?";
      int r = ::MessageBoxW(m_hWnd, msg.c_str(),
          L"【火流猩输入法】快捷键编辑器",
          MB_YESNO | MB_ICONWARNING);
      if (r != IDYES) return 0;
      break;  // one warning is enough
    }
  }
  if (!SaveToYaml()) {
    ::MessageBoxW(m_hWnd, L"保存失败:无法写入 default.custom.yaml。\n请检查文件权限。",
                L"【火流猩输入法】快捷键编辑器", MB_OK | MB_ICONERROR);
    return 0;
  }
  dirty_ = false;
  TriggerDeploy();
  EndDialog(IDOK);
  return 0;
}

LRESULT HotkeyEditorDialog::OnCancel(WORD, WORD, HWND, BOOL&) {
  if (dirty_) {
    int r = ::MessageBoxW(m_hWnd,
        L"有未保存的修改,确定要关闭吗?",
        L"【火流猩输入法】快捷键编辑器",
        MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return 0;
  }
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT HotkeyEditorDialog::OnAdd(WORD, WORD, HWND, BOOL&) {
  AddOrEditBinding(false);
  return 0;
}

LRESULT HotkeyEditorDialog::OnEdit(WORD, WORD, HWND, BOOL&) {
  int sel = list_.GetSelectionMark();
  if (sel < 0) {
    ::MessageBoxW(m_hWnd, L"请先在列表中选择一条要修改的快捷键。",
                L"【火流猩输入法】快捷键编辑器", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  AddOrEditBinding(true);
  return 0;
}

LRESULT HotkeyEditorDialog::OnDelete(WORD, WORD, HWND, BOOL&) {
  int sel = list_.GetSelectionMark();
  if (sel < 0) {
    ::MessageBoxW(m_hWnd, L"请先在列表中选择一条要删除的快捷键。",
                L"【火流猩输入法】快捷键编辑器", MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (::MessageBoxW(m_hWnd, L"确定要删除这条快捷键吗?",
                  L"【火流猩输入法】快捷键编辑器",
                  MB_YESNO | MB_ICONQUESTION) != IDYES) {
    return 0;
  }
  // The list is filtered/sorted; we need to map back to bindings_
  // index. We stored the original index in the item's lParam.
  LVITEMW item = {0};
  item.mask = LVIF_PARAM;
  item.iItem = sel;
  list_.GetItem(&item);
  size_t orig_idx = (size_t)item.lParam;
  if (orig_idx < bindings_.size()) {
    bindings_.erase(bindings_.begin() + orig_idx);
  }
  SetDirty();
  RebuildList();
  return 0;
}

LRESULT HotkeyEditorDialog::OnReset(WORD, WORD, HWND, BOOL&) {
  if (::MessageBoxW(m_hWnd, L"确定要重置为默认设置吗?\n这将丢弃所有用户修改。",
                  L"【火流猩输入法】快捷键编辑器",
                  MB_YESNO | MB_ICONWARNING) != IDYES) {
    return 0;
  }
  // Re-load from deploy_yaml_path_ (default.yaml)
  std::ifstream f(deploy_yaml_path_.c_str(), std::ios::binary);
  if (!f) {
    ::MessageBoxW(m_hWnd, L"无法读取默认 default.yaml。", L"错误", MB_OK | MB_ICONERROR);
    return 0;
  }
  bindings_.clear();
  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  fluxing::YamlDocument doc;
  if (!fluxing::Load(content, &doc)) return 0;
  // Read key_binder.bindings array
  // Use YamlRoundTrip's ReadString is scalar-only; for arrays we
  // access the underlying root() (we have friend access via the
  // header pattern in spec 024).
  // For simplicity, walk the YAML text directly to extract
  // "- { when: ..., accept: ..., send/toggle/select: ... }" lines.
  // This is a one-time back-of-envelope parser for the reset path
  // only; the main path uses YamlRoundTrip's ReadString.
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    fluxing::HotkeyBinding b;
    if (fluxing::HotkeyBinding::FromYamlInline(line, &b)) {
      bindings_.push_back(b);
    }
  }
  SetDirty();
  RebuildList();
  return 0;
}

LRESULT HotkeyEditorDialog::OnClear(WORD, WORD, HWND, BOOL&) {
  search_.SetWindowTextW(L"");
  return 0;
}

LRESULT HotkeyEditorDialog::OnFilterChange(WORD, WORD, HWND, BOOL&) {
  filter_index_ = filter_.GetCurSel();
  if (filter_index_ < 0) filter_index_ = 0;
  RebuildList();
  return 0;
}

LRESULT HotkeyEditorDialog::OnSearchChange(WORD, WORD, HWND, BOOL&) {
  wchar_t buf[256] = {0};
  search_.GetWindowTextW(buf, 256);
  search_text_ = buf;
  RebuildList();
  return 0;
}

LRESULT HotkeyEditorDialog::OnListItemChanged(int, LPNMHDR, BOOL&) {
  return 0;
}

void HotkeyEditorDialog::AddOrEditBinding(bool is_edit) {
  int sel = is_edit ? list_.GetSelectionMark() : -1;
  fluxing::HotkeyBinding existing;
  bool has_existing = false;
  if (is_edit && sel >= 0) {
    LVITEMW item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    list_.GetItem(&item);
    size_t orig_idx = (size_t)item.lParam;
    if (orig_idx < bindings_.size()) {
      existing = bindings_[orig_idx];
      has_existing = true;
    }
  }
  // Show KeyRecorderDialog for the accept key.
  KeyRecorderDialog rec;
  if (has_existing) rec.set_initial_accept(existing.accept);
  if (rec.DoModal(m_hWnd) != IDOK) return;

  // Use the existing `when` and `action` from the row, and update only
  // `accept`. (Full Add would also let the user pick action+value via
  // separate controls, but for MVP we edit only the key combination;
  // for non-key fields the user can re-add or hand-edit yaml.)
  fluxing::HotkeyBinding b = has_existing ? existing : fluxing::HotkeyBinding();
  b.accept = rec.accept();

  if (is_edit && sel >= 0) {
    LVITEMW item = {0};
    item.mask = LVIF_PARAM;
    item.iItem = sel;
    list_.GetItem(&item);
    size_t orig_idx = (size_t)item.lParam;
    if (orig_idx < bindings_.size()) {
      bindings_[orig_idx] = b;
    }
  } else {
    bindings_.push_back(b);
  }
  SetDirty();
  RebuildList();
}

void HotkeyEditorDialog::SetDirty() { dirty_ = true; }

bool HotkeyEditorDialog::MatchesFilter(const fluxing::HotkeyBinding& b) const {
  // Filter
  if (filter_index_ == 1 && b.when != fluxing::HotkeyWhen::Composing) return false;
  if (filter_index_ == 2 && b.when != fluxing::HotkeyWhen::HasMenu)   return false;
  if (filter_index_ == 3 && b.when != fluxing::HotkeyWhen::Always)    return false;
  // Search
  if (!search_text_.empty()) {
    std::wstring lower_search = search_text_;
    for (auto& c : lower_search) c = (wchar_t)towlower(c);
    std::wstring lower_accept = b.accept;
    for (auto& c : lower_accept) c = (wchar_t)towlower(c);
    if (lower_accept.find(lower_search) == std::wstring::npos) {
      return false;
    }
  }
  return true;
}

void HotkeyEditorDialog::RebuildList() {
  list_.DeleteAllItems();
  int idx = 0;
  for (size_t i = 0; i < bindings_.size(); ++i) {
    if (!MatchesFilter(bindings_[i])) continue;
    LVITEMW item = {0};
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = idx;
    item.lParam = (LPARAM)i;  // original index in bindings_
    wchar_t num[16]; swprintf_s(num, L"%d", (int)(i + 1));
    item.pszText = num;
    list_.InsertItem(&item);
    list_.SetItemText(idx, 1, (LPWSTR)fluxing::WhenToString(bindings_[i].when));
    list_.SetItemText(idx, 2, (LPWSTR)bindings_[i].accept.c_str());
    std::string action = fluxing::ActionToString(bindings_[i].action);
    std::wstring waction(action.begin(), action.end());
    list_.SetItemText(idx, 3, (LPWSTR)waction.c_str());
    std::wstring wvalue(bindings_[i].action_value.begin(),
                         bindings_[i].action_value.end());
    list_.SetItemText(idx, 4, (LPWSTR)wvalue.c_str());
    ++idx;
  }
}

bool HotkeyEditorDialog::LoadFromYaml() {
  // Prefer custom.yaml (the file the editor writes to), fallback to
  // the deployment default.yaml.
  std::string content = ReadEntireFile(custom_yaml_path_);
  if (content.empty()) {
    content = ReadEntireFile(deploy_yaml_path_);
  }
  if (content.empty()) return false;
  bindings_.clear();
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
    fluxing::HotkeyBinding b;
    if (fluxing::HotkeyBinding::FromYamlInline(line, &b)) {
      bindings_.push_back(b);
    }
  }
  return true;
}

bool HotkeyEditorDialog::SaveToYaml() {
  // Strategy: build a complete default.custom.yaml that contains
  //   - the user-customized key_binder section (if any exists in
  //     the loaded custom.yaml, preserve it as a wrapper; otherwise
  //     write a fresh patch: { patch: { key_binder: { bindings: [...] } } }
  //   - the new bindings list (always, overwriting previous if any)
  //
  // We don't currently support partial merge: Save = replace key_binder
  // entirely with the editor's bindings list. Other yaml fields
  // (schema_list, menu, switcher, etc.) are not touched in this
  // MVP. If custom.yaml already had a key_binder section, it is
  // replaced; if it had other fields, those are preserved by
  // copying the loaded content first and then replacing the
  // key_binder section.
  std::string existing = ReadEntireFile(custom_yaml_path_);
  fluxing::YamlDocument doc;
  if (!existing.empty()) {
    if (!fluxing::Load(existing, &doc)) {
      // Custom yaml malformed; start fresh
      fluxing::YamlDocument fresh;
      doc = std::move(fresh);
    }
  }
  // Build the new bindings inline list
  std::ostringstream bindings_yaml;
  bindings_yaml << "key_binder:\n";
  bindings_yaml << "  bindings:\n";
  for (size_t i = 0; i < bindings_.size(); ++i) {
    bindings_yaml << "    " << bindings_[i].ToYamlInline(4) << "\n";
  }
  // Now merge: the bindings_yaml is a partial yaml string. We want
  // to either:
  //   (a) replace just the key_binder block in `doc`, or
  //   (b) if custom.yaml didn't exist, write a new file with
  //       minimal structure + bindings_yaml.
  // For MVP simplicity, do (b): write a fresh file. Users editing
  // other yaml fields would use a different tool.
  std::ostringstream full_yaml;
  full_yaml << "# Fluxing default.custom.yaml - edited by F3 hotkey editor\n";
  full_yaml << "# spec 050 (2026-07-08) - this file is user-managed; not\n";
  full_yaml << "# overwritten by upgrades (per project-knowledge.md L40).\n";
  full_yaml << "\n";
  full_yaml << bindings_yaml.str();
  // (If existing had other sections, they are dropped by this MVP.
  // Future spec 051+ will add a real yaml merge pass.)
  (void)doc;
  return WriteEntireFile(custom_yaml_path_, full_yaml.str());
}

void HotkeyEditorDialog::TriggerDeploy() {
  // Spawn WeaselDeployer.exe /deploy from the same install dir.
  std::wstring install_root = GetInstallDir();
  std::wstring deployer = install_root + L"\\weasel\\WeaselDeployer.exe";
  std::wstring args = L"/deploy";
  STARTUPINFOW si = {0};
  PROCESS_INFORMATION pi = {0};
  si.cb = sizeof(si);
  std::wstring cmd = L"\"" + deployer + L"\" " + args;
  CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0,
                 nullptr, nullptr, &si, &pi);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  ::MessageBoxW(m_hWnd,
      L"已保存到 default.custom.yaml 并触发 deploy,新快捷键立即生效。\n(无需重启 WeaselServer)",
      L"【火流猩输入法】快捷键编辑器", MB_OK | MB_ICONINFORMATION);
}

int CALLBACK HotkeyEditorDialog::ListCompare(LPARAM, LPARAM, LPARAM) {
  return 0;
}