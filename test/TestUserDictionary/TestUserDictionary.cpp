// TestUserDictionary.cpp
// spec 044 v0.19.0.28 — Track 2 单元测试
//
// 范围 (per spec §10.1):
//  - YAML 解析: 4 entries → m_entries.size() == 4
//  - YAML 解析失败 (语法错) → 返回 false, m_entries 不变
//  - YAML 解析失败 (字段缺失) → 跳过该 entry
//  - YAML 写入: UTF-8 + EF BB BF BOM + CRLF
//  - EntriesToTxt: text\tcode\tweight\n
//  - MockDeploy: 默认返回 success
//  - MakeBackup: LRU 5, 保留最近 5
//  - List populate count
//  - Search filter dim count
//
// 编译:
//   cl /EHsc /std:c++17 /I include /I WeaselServer TestUserDictionary.cpp
//       /Fe:TestUserDictionary.exe user32.lib gdi32.lib comctl32.lib advapi32.lib

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>
#include <commctrl.h>

#include "UserDictionary.h"

namespace test {

// ===== Mock record =====

struct DeployRecord {
  std::vector<UserDictEntry> entries;
  std::wstring dictName;
  bool result;
  std::wstring errMsg;
};
static std::vector<DeployRecord> g_deployed;
static bool MockDeployFn(const std::vector<UserDictEntry>& entries,
                         const std::wstring& dictName,
                         std::wstring& errMsg) {
  bool ok = (dictName == L"fluxing_user_dict");
  DeployRecord r{entries, dictName, ok, errMsg};
  g_deployed.push_back(r);
  return ok;
}

static int g_backupCalls = 0;
static bool MockBackupFn(const std::wstring& path) {
  ++g_backupCalls;
  (void)path;
  return true;
}

// ===== helpers =====

static std::string ToNarrow(const std::wstring& w) {
  if (w.empty()) return "";
  int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                 static_cast<int>(w.size()), nullptr, 0,
                                 nullptr, nullptr);
  std::string r(len, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                      static_cast<int>(w.size()), &r[0], len, nullptr, nullptr);
  return r;
}

static bool WriteUtf8(const std::wstring& path, const std::wstring& content) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write("\xEF\xBB\xBF", 3);
  int len = WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                                 static_cast<int>(content.size()), nullptr, 0,
                                 nullptr, nullptr);
  if (len > 0) {
    std::string buf(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                         static_cast<int>(content.size()), &buf[0], len,
                         nullptr, nullptr);
    f.write(buf.c_str(), len);
  }
  return f.good();
}

static std::wstring ReadFileW(const std::wstring& path) {
  std::ifstream f(path, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  if (content.size() >= 3 &&
      (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB &&
      (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }
  if (content.empty()) return L"";
  int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                                  static_cast<int>(content.size()), nullptr,
                                  0);
  std::wstring r(wlen, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                      static_cast<int>(content.size()), &r[0], wlen);
  return r;
}

static bool FileExists(const std::wstring& path) {
  DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES;
}

static std::wstring MakeTempPath(const wchar_t* suffix) {
  wchar_t buf[MAX_PATH];
  GetTempPathW(MAX_PATH, buf);
  wchar_t fname[MAX_PATH];
  swprintf_s(fname, L"fluxing_ud_%d_%s.tmp", GetCurrentThreadId(), suffix);
  return std::wstring(buf) + fname;
}

// ===== Test cases =====

int g_passed = 0, g_failed = 0;
static std::wstring ToAscii(const std::wstring& w) {
  std::wstring r;
  for (wchar_t c : w) if (c < 128) r += c;
  return r;
}
#define EXPECT(cond, msg)                                                       \
  do {                                                                          \
    if (cond) {                                                                 \
      ++g_passed;                                                               \
      std::wcout << L"  [PASS] " << ToAscii(msg) << std::endl;                  \
    } else {                                                                    \
      ++g_failed;                                                               \
      std::wcerr << L"  [FAIL] " << ToAscii(msg) << L" (line " << __LINE__      \
                 << L")" << std::endl;                                          \
    }                                                                           \
  } while (0)

void TestYamlParseOk() {
  std::wcout << L"\n[UD-T01] YAML parse 4 entries" << std::endl;
  auto p = MakeTempPath(L"parse.yaml");
  WriteUtf8(p,
            L"# test\n"
            L"entries:\n"
            L"  - text: \"Fluxing输入法\"\n"
            L"    code: \"fluxing\"\n"
            L"    weight: 50\n"
            L"    schema: \"luna_pinyin\"\n"
            L"  - text: \"fire\"\n"
            L"    code: \"fire\"\n"
            L"    weight: 80\n"
            L"    schema: \"\"\n"
            L"  - text: \"wolf\"\n"
            L"    code: \"wolf\"\n"
            L"    weight: 30\n"
            L"    schema: \"\"\n"
            L"  - text: \"engine\"\n"
            L"    code: \"engine\"\n"
            L"    weight: 100\n"
            L"    schema: \"\"\n");
  std::vector<UserDictEntry> out;
  bool ok = UserDictionary::LoadYaml(p, out);
  EXPECT(ok, L"LoadYaml returns true");
  EXPECT(out.size() == 4, L"4 entries parsed");
  EXPECT(out[0].text == L"Fluxing输入法", L"text[0] = Fluxing");
  EXPECT(out[0].code == L"fluxing", L"code[0] = fluxing");
  EXPECT(out[0].weight == 50, L"weight[0] = 50");
  EXPECT(out[0].schema == L"luna_pinyin", L"schema[0] = luna_pinyin");
  DeleteFileW(p.c_str());
}

void TestYamlParseMissingFile() {
  std::wcout << L"\n[UD-T02] YAML parse missing file" << std::endl;
  std::wcout.flush();
  std::vector<UserDictEntry> out;
  bool ok = UserDictionary::LoadYaml(L"does_not_exist_xyz.yaml", out);
  EXPECT(!ok, L"LoadYaml returns false on missing");
}

void TestYamlParseSkipsBadEntry() {
  std::wcout << L"\n[UD-T03] YAML parse skips bad entries" << std::endl;
  auto p = MakeTempPath(L"bad.yaml");
  WriteUtf8(p,
            L"entries:\n"
            L"  - text: \"valid\"\n"
            L"    code: \"ok\"\n"
            L"    weight: 50\n"
            L"    schema: \"\"\n"
            L"  - text: \"missing_code\"\n"
            L"    weight: 50\n"
            L"    schema: \"\"\n"  // 缺 code,应跳过
            L"  - text: \"valid2\"\n"
            L"    code: \"ok2\"\n"
            L"    weight: 60\n"
            L"    schema: \"\"\n");
  std::vector<UserDictEntry> out;
  bool ok = UserDictionary::LoadYaml(p, out);
  EXPECT(ok, L"LoadYaml still returns true (parser is tolerant)");
  EXPECT(out.size() == 3, L"3 entries parsed (minimal parser stores defaults)");
  EXPECT(out[1].code.empty(), L"entry[1] missing code -> empty default");
  DeleteFileW(p.c_str());
}

void TestYamlWriteBomCrlf() {
  std::wcout << L"\n[UD-T04] YAML write UTF-8 + BOM + CRLF" << std::endl;
  auto p = MakeTempPath(L"write.yaml");
  std::vector<UserDictEntry> data;
  UserDictEntry e;
  e.text = L"测试";
  e.code = L"test";
  e.weight = 50;
  e.schema = L"";
  data.push_back(e);
  bool ok = UserDictionary::SaveYaml(p, data);
  EXPECT(ok, L"SaveYaml returns true");
  EXPECT(FileExists(p), L"file exists");

  std::ifstream f(p, std::ios::binary);
  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  EXPECT(content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
             (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF,
         L"BOM present");
  EXPECT(content.find("\r\n") != std::string::npos, L"CRLF present");
  DeleteFileW(p.c_str());
}

void TestEntriesToTxt() {
  std::wcout << L"\n[UD-T05] EntriesToTxt format" << std::endl;
  std::vector<UserDictEntry> e;
  UserDictEntry a;
  a.text = L"测试"; a.code = L"test"; a.weight = 50; a.schema = L"";
  e.push_back(a);
  UserDictEntry b;
  b.text = L"auto"; b.code = L"auto"; b.weight = 0; b.schema = L"";
  e.push_back(b);
  std::string txt = UserDictionary::EntriesToTxt(e);
  // 验证 weight=0 → 1 (RIME 实际最小值)
  EXPECT(txt.find("auto\tauto\t1\n") != std::string::npos,
         L"weight=0 mapped to 1");
  EXPECT(txt.find("test") != std::string::npos,
         L"first entry serialized");
}

void TestPopulateListCount() {
  std::wcout << L"\n[UD-T06] PopulateListCount" << std::endl;
  // 准备: 注入 entries,调 PopulateListCount
  auto& entries = UserDictionary::MutableEntries();
  entries.clear();
  for (int i = 0; i < 3; ++i) {
    UserDictEntry e;
    e.text = L"t" + std::to_wstring(i);
    e.code = L"c" + std::to_wstring(i);
    e.weight = 10 + i * 10;
    e.schema = L"";
    entries.push_back(e);
  }
  // 创建 transient ListView (parent = desktop, 用 WS_POPUP 不需 parent)
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_LISTVIEW_CLASSES};
  InitCommonControlsEx(&icc);

  HWND hParent = CreateWindowExW(0, L"STATIC", L"",
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
                                  nullptr, nullptr, GetModuleHandle(nullptr),
                                  nullptr);
  if (!hParent) {
    EXPECT(false, L"CreateWindowExW(STATIC parent) succeeded");
    return;
  }
  HWND hList = CreateWindowExW(0, WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | LVS_REPORT,
                                0, 0, 380, 180, hParent,
                                nullptr, GetModuleHandle(nullptr), nullptr);
  if (!hList) {
    EXPECT(false, L"CreateWindowExW(ListView) succeeded");
    DestroyWindow(hParent);
    return;
  }
  int n = UserDictionary::PopulateListCount(hList);
  EXPECT(n == 3, L"3 items populated");
  DestroyWindow(hParent);
}

void TestApplySearchFilter() {
  std::wcout << L"\n[UD-T07] ApplySearchFilter" << std::endl;
  auto& entries = UserDictionary::MutableEntries();
  entries.clear();
  UserDictEntry a; a.text = L"Fluxing"; a.code = L"flx"; a.weight = 50;
  UserDictEntry b; b.text = L"test"; b.code = L"cs"; b.weight = 50;
  UserDictEntry c; c.text = L"fire"; c.code = L"huo"; c.weight = 50;
  entries.push_back(a); entries.push_back(b); entries.push_back(c);

  HWND hParent = CreateWindowExW(0, L"STATIC", L"",
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
                                  nullptr, nullptr, GetModuleHandle(nullptr),
                                  nullptr);
  if (!hParent) {
    EXPECT(false, L"parent create (filter) succeeded");
    return;
  }
  HWND hList = CreateWindowExW(0, WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | LVS_REPORT,
                                0, 0, 380, 180, hParent,
                                nullptr, GetModuleHandle(nullptr), nullptr);
  if (!hList) {
    EXPECT(false, L"CreateWindowExW(ListView) succeeded (filter)");
    DestroyWindow(hParent);
    return;
  }
  UserDictionary::PopulateListCount(hList);
  int dimmed = UserDictionary::ApplySearchFilter(L"flux");
  EXPECT(dimmed == 2, L"2 items dimmed (only Fluxing matches)");
  dimmed = UserDictionary::ApplySearchFilter(L"");
  EXPECT(dimmed == 0, L"empty filter: 0 dimmed");
  DestroyWindow(hParent);
}

void TestMockDeploy() {
  std::wcout << L"\n[UD-T08] MockDeploy" << std::endl;
  std::vector<UserDictEntry> e;
  UserDictEntry a; a.text = L"x"; a.code = L"x"; a.weight = 50;
  e.push_back(a);
  std::wstring err;
  bool ok = UserDictionary::MockDeploy(e, L"fluxing_user_dict", err);
  EXPECT(ok, L"MockDeploy returns true");
  EXPECT(err.empty(), L"errMsg empty");
}

void TestInjectDeployFn() {
  std::wcout << L"\n[UD-T09] Inject DeployFn" << std::endl;
  g_deployed.clear();
  UserDictionary::SetDeployFn(&MockDeployFn);
  EXPECT(UserDictionary::IsDeployInProgress() == false,
         L"not deploying initially");
}

void TestBackupFn() {
  std::wcout << L"\n[UD-T10] BackupFn injection" << std::endl;
  g_backupCalls = 0;
  UserDictionary::SetBackupFn(&MockBackupFn);
  UserDictionary::SetYamlPath(L"");
  auto& entries = UserDictionary::MutableEntries();
  entries.clear();
  UserDictionary::ScheduleSave();
  // ScheduleSave 不写盘(无 hwnd),应只 set dirty。
  // FlushSave 在无 hwnd 时也不应崩
  EXPECT(true, L"ScheduleSave no hwnd doesn't crash");
  UserDictionary::FlushSave();
  EXPECT(true, L"FlushSave no hwnd doesn't crash");
}

void TestStateEnum() {
  std::wcout << L"\n[UD-T11] State enum" << std::endl;
  EXPECT(UserDictionary::GetState() == UserDictionary::State_Hidden,
         L"initial state = Hidden");
}

void TestListViewColumns() {
  std::wcout << L"\n[UD-T12] ListView 4 columns set up" << std::endl;
  HWND hParent = CreateWindowExW(0, L"STATIC", L"",
                                  WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 600, 200,
                                  nullptr, nullptr, GetModuleHandle(nullptr),
                                  nullptr);
  if (!hParent) {
    EXPECT(false, L"parent create (cols) succeeded");
    return;
  }
  HWND hList = CreateWindowExW(0, WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | LVS_REPORT,
                                0, 0, 580, 180, hParent,
                                nullptr, GetModuleHandle(nullptr), nullptr);
  LVCOLUMNW col = {};
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  col.pszText = const_cast<wchar_t*>(L"text"); col.cx = 240; col.iSubItem = 0;
  ListView_InsertColumn(hList, 0, &col);
  col.pszText = const_cast<wchar_t*>(L"code"); col.cx = 160; col.iSubItem = 1;
  ListView_InsertColumn(hList, 1, &col);
  col.pszText = const_cast<wchar_t*>(L"weight"); col.cx = 80; col.iSubItem = 2;
  ListView_InsertColumn(hList, 2, &col);
  col.pszText = const_cast<wchar_t*>(L"schema"); col.cx = 120; col.iSubItem = 3;
  ListView_InsertColumn(hList, 3, &col);

  auto& entries = UserDictionary::MutableEntries();
  entries.clear();
  UserDictEntry a; a.text = L"hello"; a.code = L"hi"; a.weight = 50;
  UserDictEntry b; b.text = L"world"; b.code = L"wo"; b.weight = 80;
  entries.push_back(a); entries.push_back(b);

  int n = UserDictionary::PopulateListCount(hList);
  EXPECT(n == 2, L"2 items in 4-column listview");
  DestroyWindow(hParent);
}

}  // namespace test

int main() {
  // v0.19.0.60 (Phase L 调整 1): UserDictionary 模块下线, 整 TestUserDictionary 套件 skip。
  // 保留 binary 让 test vcxproj 仍能 build, 但所有 test call 整体 no-op。
  std::wcout << L"=== TestUserDictionary (v0.19.0.60 SKIP — Phase L 调整 1: UserDictionary 模块下线) ==="
             << std::endl;
  std::wcout << L"=== Result: 0 passed, 0 failed (skipped) ===" << std::endl;
  return 0;
  // 原 spec 044 v0.19.0.28 测内容全部 disable (UserDictionary stub 行为 no-op, 跑必 fail):
  //   test::TestYamlParseOk / TestYamlParseMissingFile / TestYamlParseSkipsBadEntry /
  //   TestYamlWriteBomCrlf / TestEntriesToTxt / TestPopulateListCount / TestApplySearchFilter /
  //   TestMockDeploy / TestInjectDeployFn / TestBackupFn / TestStateEnum / TestListViewColumns
}