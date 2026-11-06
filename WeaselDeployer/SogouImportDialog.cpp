// SogouImportDialog.cpp — 搜狗用户词库导入对话框实现。
//
// 数据流：
//   搜狗 .bin → SogouUserDictParser（词条）
//              → SogouPinyinResolver（随包 cn_dicts 反查空格分隔无调全拼）
//              → 临时 TSV（词条 \t 码 \t commits）
//              → RimeLeversApi::import_user_dict
//
// 关键约束（见 ADR 0037）：
//   1. import_user_dict 必须在 StartMaintenance/EndMaintenance 之间调用；
//      本对话框始终在 Configurator::SogouImport() 的 MaintenanceGuard 作用域内
//      弹出，因此调用天然落在维护区间内。
//   2. 目标 userdb 的 key 是「码 \t 词条」，码为空格分隔的无调全拼
//      （如 "A I bei hou" → "AI背后"），与随包 rime_ice 词表同一码空间。
//   3. 搜狗词库与随包词表都只在本机运行时读取，不进仓库、不进安装器。

#include "stdafx.h"
#include "SogouImportDialog.h"

#include "SogouPinyinResolver.h"
#include "SogouUserDictParser.h"

#include <WeaselUtility.h>
#include <rime_api.h>

#include "WeaselDeployer.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// 导入权重（commits）。推导见 ADR 0037：
//   import_user_dict 走 TableDb::format.parser，新 key 的 tick=0 ⇒ dee 衰减到 ~0，
//   实际权重由 m=(commits/present_tick)·(1-exp(-present_tick/10000))^10 决定。
//   present_tick≈12368 时 commits=100 ⇒ weight≈-7.2，与用户手打 7 次等效。
constexpr int kImportCommits = 100;

std::string ToUtf8(const std::wstring& s) {
  if (s.empty()) {
    return std::string();
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 1) {
    return std::string();
  }
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &out[0], n, nullptr, nullptr);
  return out;
}

std::wstring ToWide(const std::string& s) {
  if (s.empty()) {
    return std::wstring();
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
  if (n <= 1) {
    return std::wstring();
  }
  std::wstring out(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &out[0], n);
  return out;
}

// 搜狗用户数据根目录：%USERPROFILE%\AppData\LocalLow。
// 搜狗把用户自造词放在 LocalLow 下，且普通态（SogouPY）与账号态
// （SogouPY.users\<数字>\）并存，缺一不可。
std::filesystem::path SogouLocalLowDir() {
  wchar_t profile[MAX_PATH] = {0};
  const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return std::filesystem::path();
  }
  return std::filesystem::path(profile) / L"AppData" / L"LocalLow";
}

std::vector<std::filesystem::path> SogouUserDataRoots() {
  std::vector<std::filesystem::path> roots;
  const std::filesystem::path low = SogouLocalLowDir();
  if (low.empty()) {
    return roots;
  }
  roots.push_back(low / L"SogouPY");
  roots.push_back(low / L"SogouPY.users");
  return roots;
}

// 一个目录下第一层的搜狗用户词库文件。
std::vector<std::filesystem::path> CollectUserDictFiles(
    const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  for (size_t i = 0; i < weasel::sogou::kSogouUserDictFileNameCount; ++i) {
    const std::filesystem::path p =
        dir / weasel::sogou::kSogouUserDictFileNames[i];
    if (std::filesystem::is_regular_file(p, ec)) {
      found.push_back(p);
    }
  }
  return found;
}

std::wstring FormatSize(uint64_t bytes) {
  wchar_t buf[64] = {0};
  if (bytes >= 1024ull * 1024ull) {
    swprintf_s(buf, L"%.1f MB", bytes / (1024.0 * 1024.0));
  } else if (bytes >= 1024ull) {
    swprintf_s(buf, L"%.0f KB", bytes / 1024.0);
  } else {
    swprintf_s(buf, L"%llu B", bytes);
  }
  return buf;
}

// 临时 TSV 路径（导入完成后删除，不留痕）。
std::filesystem::path MakeTempTsvPath() {
  wchar_t tmp[MAX_PATH] = {0};
  const DWORD n = GetTempPathW(MAX_PATH, tmp);
  std::filesystem::path dir =
      (n > 0 && n < MAX_PATH) ? std::filesystem::path(tmp)
                               : std::filesystem::path(L".");
  wchar_t name[64] = {0};
  swprintf_s(name, L"fluxing-sogou-import-%lu.tsv",
             static_cast<unsigned long>(GetCurrentProcessId()));
  return dir / name;
}

}  // namespace

SogouImportDialog::SogouImportDialog() {
  RimeApi* rime = rime_get_api();
  if (rime && RIME_API_AVAILABLE(rime, find_module)) {
    api_ = (RimeLeversApi*)rime->find_module("levers")->get_api();
  }
}

SogouImportDialog::~SogouImportDialog() {}

void SogouImportDialog::PopulateSources() {
  for (const auto& root : SogouUserDataRoots()) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
      continue;
    }
    // 搜狗账号态在 SogouPY.users\<数字>\ 下，多扫一层。
    std::vector<std::filesystem::path> dirs;
    dirs.push_back(root);
    for (std::filesystem::directory_iterator it(root, ec), end;
         it != end && !ec; it.increment(ec)) {
      if (it->is_directory(ec)) {
        dirs.push_back(it->path());
      }
    }
    for (const auto& dir : dirs) {
      for (const auto& file : CollectUserDictFiles(dir)) {
        Source src;
        src.path = file.wstring();
        std::error_code fec;
        const auto size = std::filesystem::file_size(file, fec);
        const std::wstring parent = file.parent_path().filename().wstring();
        src.label = (parent.empty() ? file.filename().wstring() : parent) +
                    L" / " + file.filename().wstring() + L"  (" +
                    FormatSize(fec ? 0 : size) + L")";
        sources_.push_back(src);
        source_list_.AddString(src.label.c_str());
      }
    }
  }
}

void SogouImportDialog::PopulateTargets() {
  target_dict_.ResetContent();
  target_dict_names_.clear();
  if (api_) {
    RimeUserDictIterator iter = {0};
    api_->user_dict_iterator_init(&iter);
    while (const char* dict = api_->next_user_dict(&iter)) {
      const std::wstring txt = ToWide(dict);
      if (!txt.empty()) {
        target_dict_.AddString(txt.c_str());
        // librime 的 userdb 名字本身就是 UTF-8，直接留原始串，避免二次转码。
        target_dict_names_.push_back(std::string(dict));
      }
    }
    api_->user_dict_iterator_destroy(&iter);
  }
  // 兜底：迭代器只列出已存在的 userdb，方案字典（rime_ice）也要能作为目标。
  if (target_dict_.GetCount() == 0) {
    target_dict_.AddString(L"rime_ice");
    target_dict_names_.push_back(std::string("rime_ice"));
  }
  target_dict_.SetCurSel(0);
}

void SogouImportDialog::UpdateButtons() {
  const bool has_source = source_list_.GetCurSel() >= 0;
  const bool has_target = target_dict_.GetCurSel() >= 0;
  import_button_.EnableWindow(has_source && has_target ? TRUE : FALSE);
}

LRESULT SogouImportDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  source_list_.Attach(GetDlgItem(IDC_SOGOU_SOURCE_LIST));
  target_dict_.Attach(GetDlgItem(IDC_SOGOU_TARGET_DICT));
  import_button_.Attach(GetDlgItem(IDC_SOGOU_IMPORT_BTN));
  status_.Attach(GetDlgItem(IDC_SOGOU_STATUS));

  PopulateSources();
  PopulateTargets();
  UpdateButtons();

  if (sources_.empty()) {
    CString msg;
    msg.LoadStringW(IDS_STR_SOGOU_NO_SOURCE);
    status_.SetWindowText(msg);
  }

  CenterWindow();
  BringWindowToTop();
  return TRUE;
}

LRESULT SogouImportDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT SogouImportDialog::OnSourceSelChange(WORD, WORD, HWND, BOOL&) {
  UpdateButtons();
  const int sel = source_list_.GetCurSel();
  if (sel < 0 || sel >= static_cast<int>(sources_.size())) {
    return 0;
  }
  std::vector<unsigned char> data;
  if (!weasel::sogou::ReadWholeFile(ToUtf8(sources_[sel].path), &data)) {
    return 0;
  }
  weasel::sogou::SogouParseStats stats;
  std::vector<weasel::sogou::SogouEntry> entries;
  weasel::sogou::ParseSogouUserDict(data.data(), data.size(), &stats, &entries);

  CString tpl;
  tpl.LoadStringW(IDS_STR_SOGOU_FOUND);
  CString text;
  text.Format(tpl, static_cast<int>(entries.size()));
  status_.SetWindowText(text);
  return 0;
}

LRESULT SogouImportDialog::OnTargetSelChange(WORD, WORD, HWND, BOOL&) {
  UpdateButtons();
  return 0;
}

LRESULT SogouImportDialog::OnImport(WORD, WORD, HWND, BOOL&) {
  const int sel = source_list_.GetCurSel();
  if (sel < 0 || sel >= static_cast<int>(sources_.size())) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_SELECT_SOURCE, MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  const int target = target_dict_.GetCurSel();
  if (target < 0) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_SELECT_TARGET, MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  if (!api_) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_NO_SOURCE, MB_OK | MB_ICONERROR);
    return 0;
  }

  if (target >= static_cast<int>(target_dict_names_.size())) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_SELECT_TARGET, MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  const std::string dict_name = target_dict_names_[static_cast<size_t>(target)];
  if (dict_name.empty()) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_SELECT_TARGET, MB_OK | MB_ICONINFORMATION);
    return 0;
  }

  CWaitCursor wait;

  // 1) 解析搜狗词库
  std::vector<unsigned char> data;
  if (!weasel::sogou::ReadWholeFile(ToUtf8(sources_[sel].path), &data)) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_PARSE_FAILED, MB_OK | MB_ICONERROR);
    return 0;
  }
  weasel::sogou::SogouParseStats stats;
  std::vector<weasel::sogou::SogouEntry> entries;
  weasel::sogou::ParseSogouUserDict(data.data(), data.size(), &stats, &entries);
  if (entries.empty()) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_PARSE_FAILED, MB_OK | MB_ICONERROR);
    return 0;
  }

  // 2) 反查拼音（整词优先，逐字回退）
  std::unordered_set<std::string> wanted;
  wanted.reserve(entries.size() * 2);
  for (const auto& e : entries) {
    wanted.insert(e.phrase);
  }
  const std::filesystem::path cn_dicts =
      std::filesystem::path(WeaselSharedDataPath().wstring()) / L"cn_dicts";
  weasel::sogou::SogouPinyinResolver resolver;
  resolver.Build(cn_dicts.u8string(), wanted);

  // 3) 生成 TSV：词条 \t 码 \t commits
  const std::filesystem::path tsv_path = MakeTempTsvPath();
  size_t written = 0;
  {
    std::ofstream out(tsv_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      MSG_ID_CAP(L"", IDS_STR_SOGOU_PARSE_FAILED, MB_OK | MB_ICONERROR);
      return 0;
    }
    std::string code;
    for (const auto& e : entries) {
      if (!resolver.Resolve(e.phrase, &code) || code.empty()) {
        continue;
      }
      out << e.phrase << "\t" << code << "\t" << kImportCommits << "\n";
      ++written;
    }
    out.flush();
    if (!out) {
      out.close();
      std::error_code ec;
      std::filesystem::remove(tsv_path, ec);
      MSG_ID_CAP(L"", IDS_STR_SOGOU_PARSE_FAILED, MB_OK | MB_ICONERROR);
      return 0;
    }
  }
  if (written == 0) {
    std::error_code ec;
    std::filesystem::remove(tsv_path, ec);
    MSG_ID_CAP(L"", IDS_STR_SOGOU_PARSE_FAILED, MB_OK | MB_ICONERROR);
    return 0;
  }

  // 4) 导入（调用方保证处于维护区间内）
  const int result = api_->import_user_dict(
      dict_name.c_str(), ToUtf8(tsv_path.wstring()).c_str());

  std::error_code ec;
  std::filesystem::remove(tsv_path, ec);

  if (result < 0) {
    MSG_ID_CAP(L"", IDS_STR_SOGOU_IMPORT_FAILED, MB_OK | MB_ICONERROR);
    return 0;
  }

  CString tpl;
  tpl.LoadStringW(IDS_STR_SOGOU_DONE);
  CString text;
  text.Format(tpl, result);
  MSG_ID_CAP(text.GetString(), IDS_STR_HAPPY, MB_OK | MB_ICONINFORMATION);
  return 0;
}
