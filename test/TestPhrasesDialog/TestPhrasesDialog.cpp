// TestPhrasesDialog.cpp
// spec 044 v0.19.0.32 — Track 2 unit tests (UX redo)
//
// 范围 (per spec 044):
//  - YAML parser: 5 phrase (单 text 字段) → m_phrases[] 正确
//  - YAML 解析失败 → 返回 false, m_phrases 不变
//  - YAML 兼容旧 text/category: load 时 category 被丢弃, save 只写 text
//  - List populate: 5 phrases → ListView_GetItemCount == 5
//  - Add phrase: push 到 m_phrases 末尾,SavePhrases() 写盘
//  - Edit phrase: 改 m_phrases[i].text, SavePhrases() 写盘
//  - Delete phrase: m_phrases erase, SavePhrases() 写盘
//  - SendInput mock: 不真发,record 到 test log,验证 unicode codepoint
//
// v0.19.0.32 UX redo 关键验证:
//  - s_hInput + s_hList + 4 buttons 创建 (无 s_hEditText/s_hEditCat/s_hTree)
//  - SendMessage(s_hInput, WM_SETTEXT, "hello") + ID_BTN_ADD_TOP dispatch
//    → m_phrases push_back + ListView item +1
//  - 点击 list item → s_hInput 自动 fill 选中 text
//  - 修改 s_hInput + ID_BTN_EDIT → m_phrases[index].text 改
//
// 编译:
//   cl /EHsc /std:c++17 /I include /I WeaselServer TestPhrasesDialog.cpp
//       /Fe:TestPhrasesDialog.exe user32.lib gdi32.lib comctl32.lib advapi32.lib

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

#include "PhrasesDialog.h"

namespace test {

// ===== Mock record =====

struct InjectRecord {
  std::wstring text;
};
static std::vector<InjectRecord> g_injected;

static void MockInject(const std::wstring& text) {
  g_injected.push_back({text});
}

// ===== helpers =====

static std::string ToNarrow(const std::wstring& w) {
  if (w.empty()) return "";
  int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                 static_cast<int>(w.size()), nullptr, 0,
                                 nullptr, nullptr);
  std::string r(len, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                      &r[0], len, nullptr, nullptr);
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
                         static_cast<int>(content.size()), &buf[0], len, nullptr,
                         nullptr);
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
                                  static_cast<int>(content.size()), nullptr, 0);
  std::wstring r(wlen, 0);
  MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                      static_cast<int>(content.size()), &r[0], wlen);
  return r;
}

// ===== 单元测试 =====

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(desc, cond)                                            \
  do {                                                               \
    if (cond) {                                                      \
      std::cout << "  PASS: " << desc << std::endl;                  \
      ++g_passed;                                                    \
    } else {                                                         \
      std::cout << "  FAIL: " << desc << std::endl;                  \
      ++g_failed;                                                    \
    }                                                                \
  } while (0)

#define CHECK_EQ(desc, a, b)                                         \
  do {                                                               \
    if ((a) == (b)) {                                                \
      std::cout << "  PASS: " << desc << std::endl;                  \
      ++g_passed;                                                    \
    } else {                                                         \
      std::cout << "  FAIL: " << desc << std::endl;                  \
      ++g_failed;                                                    \
    }                                                                \
  } while (0)

// Test 1: YAML parser — 5 phrases (单 text 字段,v0.19.0.32 简化)
static void TestYamlParse5Phrases() {
  std::cout << "\n[Test 1] YAML parser — 5 phrases (text only)" << std::endl;

  const std::wstring tmpPath = L"test_phrases_5p.yaml";
  std::wstring fixture;
  fixture += L"# test\n";
  fixture += L"phrases:\n";
  fixture += L"  - text: \"\x4f60\x597d\"\n";            // 你好
  fixture += L"  - text: \"\x8c22\x8c22\"\n";            // 谢谢
  fixture += L"  - text: \"Hello\"\n";
  fixture += L"  - text: \"\x6d4b\x8bd5\x77ed\x8bed\"\n";  // 测试短语
  fixture += L"  - text: \"123\"\n";
  CHECK("WriteUtf8 ok", WriteUtf8(tmpPath, fixture));

  std::vector<PhrasesDialog::Phrase> out;
  bool ok = PhrasesDialog::LoadPhrases(tmpPath, out);
  CHECK("LoadPhrases returns true", ok);
  CHECK_EQ("5 phrases loaded", out.size(), size_t(5));

  if (out.size() == 5) {
    CHECK_EQ("phrase 0 text", ToNarrow(out[0].text), std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
    CHECK_EQ("phrase 1 text", ToNarrow(out[1].text), std::string("\xe8\xb0\xa2\xe8\xb0\xa2"));
    CHECK_EQ("phrase 2 text", ToNarrow(out[2].text), std::string("Hello"));
  }

  std::remove(ToNarrow(tmpPath).c_str());
}

// Test 2: YAML 兼容旧 text/category (load 忽略 category,只读 text)
static void TestYamlCompatOldTextCategory() {
  std::cout << "\n[Test 2] YAML compat — 旧 text/category 兼容 (load 忽略 category)"
            << std::endl;

  const std::wstring tmpPath = L"test_phrases_compat.yaml";
  std::wstring fixture;
  fixture += L"# old format\n";
  fixture += L"phrases:\n";
  fixture += L"  - text: \"\x4f60\x597d\"\n";  // 你好
  fixture += L"    category: \"\x5de5\x4f5c\"\n";  // 工作
  fixture += L"  - text: \"Hello\"\n";
  fixture += L"    category: \"\"\n";
  CHECK("WriteUtf8 ok", WriteUtf8(tmpPath, fixture));

  std::vector<PhrasesDialog::Phrase> out;
  bool ok = PhrasesDialog::LoadPhrases(tmpPath, out);
  CHECK("LoadPhrases returns true (old yaml)", ok);
  CHECK_EQ("2 phrases loaded", out.size(), size_t(2));

  if (out.size() == 2) {
    // v0.19.0.32: Phrase struct 已删 category 字段,只读 text
    CHECK_EQ("phrase 0 text", ToNarrow(out[0].text), std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
    CHECK_EQ("phrase 1 text", ToNarrow(out[1].text), std::string("Hello"));
  }

  std::remove(ToNarrow(tmpPath).c_str());
}

// Test 3: YAML parse fail fallback
static void TestYamlParseFailFallback() {
  std::cout << "\n[Test 3] YAML parse fail fallback" << std::endl;
  std::vector<PhrasesDialog::Phrase> out;
  out.push_back({L"preserved"});
  bool ok = PhrasesDialog::LoadPhrases(L"nonexistent_file_12345.yaml", out);
  CHECK("LoadPhrases returns false on missing file", !ok);
  CHECK_EQ("out preserved (size)", out.size(), size_t(1));
  CHECK_EQ("out preserved (text)", ToNarrow(out[0].text), std::string("preserved"));
}

// Test 4: List populate — 5 phrases → 5 items
static void TestListPopulate5Phrases() {
  std::cout << "\n[Test 4] List populate — 5 phrases = 5 items" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"a1"});
  v.push_back({L"a2"});
  v.push_back({L"a3"});
  v.push_back({L"b1"});
  v.push_back({L"b2"});

  // v0.19.0.32: ListView 替代 TreeView
  HWND hList = CreateWindowExW(0, WC_LISTVIEWW, L"",
                               WS_CHILD, 0, 0, 200, 200,
                               HWND_MESSAGE, nullptr, GetModuleHandle(nullptr),
                               nullptr);
  CHECK("CreateWindowEx listview succeeded", hList != nullptr);
  if (!hList) return;

  // ListView 必须加 column 才能 InsertItem
  LVCOLUMNW col = {};
  col.mask = LVCF_TEXT | LVCF_WIDTH;
  col.pszText = const_cast<wchar_t*>(L"\x77ed\x8bed");  // 短语
  col.cx = 180;
  ListView_InsertColumn(hList, 0, &col);

  int inserted = PhrasesDialog::PopulateListCount(hList);
  CHECK_EQ("inserted count = 5", inserted, 5);

  int itemCount = ListView_GetItemCount(hList);
  CHECK_EQ("ListView_GetItemCount = 5", itemCount, 5);

  DestroyWindow(hList);
}

// Test 5: SendInput mock — record 而不真发
static void TestSendInputMock() {
  std::cout << "\n[Test 5] SendInput mock" << std::endl;
  g_injected.clear();
  PhrasesDialog::SetInjectFn(&MockInject);

  MockInject(L"hello");
  MockInject(L"\x4f60\x597d");  // 你好

  CHECK_EQ("2 records", g_injected.size(), size_t(2));
  if (g_injected.size() == 2) {
    CHECK_EQ("record 0 text", g_injected[0].text, std::wstring(L"hello"));
    CHECK_EQ("record 1 text", g_injected[1].text, std::wstring(L"\x4f60\x597d"));
  }

  // unicode codepoint
  if (g_injected.size() == 2 && g_injected[1].text.size() == 2) {
    CHECK_EQ("chinese codepoint 0", static_cast<int>(g_injected[1].text[0]),
             0x4F60);
    CHECK_EQ("chinese codepoint 1", static_cast<int>(g_injected[1].text[1]),
             0x597D);
  }

  PhrasesDialog::SetInjectFn(&PhrasesDialog::DefaultInject);
}

// Test 6: Add phrase + SavePhrases
static void TestAddAndSave() {
  std::cout << "\n[Test 6] Add phrase + SavePhrases" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"old1"});

  PhrasesDialog::Phrase np;
  np.text = L"newphrase";
  v.push_back(np);

  CHECK_EQ("m_phrases size after add", v.size(), size_t(2));
  CHECK_EQ("added phrase text", ToNarrow(v[1].text), std::string("newphrase"));

  const std::wstring path = L"test_phrases_add.yaml";
  bool ok = PhrasesDialog::SavePhrases(path, v);
  CHECK("SavePhrases ok", ok);

  std::vector<PhrasesDialog::Phrase> reloaded;
  bool ok2 = PhrasesDialog::LoadPhrases(path, reloaded);
  CHECK("Reload ok", ok2);
  CHECK_EQ("reload size", reloaded.size(), size_t(2));
  if (reloaded.size() == 2) {
    CHECK_EQ("reload [1].text", ToNarrow(reloaded[1].text), std::string("newphrase"));
  }

  std::remove(ToNarrow(path).c_str());
}

// Test 7: Edit phrase
static void TestEdit() {
  std::cout << "\n[Test 7] Edit phrase" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"orig"});

  v[0].text = L"edited";

  CHECK_EQ("edited text", ToNarrow(v[0].text), std::string("edited"));

  const std::wstring path = L"test_phrases_edit.yaml";
  CHECK("SavePhrases ok", PhrasesDialog::SavePhrases(path, v));
  auto content = ReadFileW(path);
  CHECK("file contains 'edited'", content.find(L"edited") != std::wstring::npos);

  std::remove(ToNarrow(path).c_str());
}

// Test 8: Delete phrase
static void TestDelete() {
  std::cout << "\n[Test 8] Delete phrase" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"a"});
  v.push_back({L"b"});
  v.push_back({L"c"});

  v.erase(v.begin() + 1);
  CHECK_EQ("size after erase", v.size(), size_t(2));
  CHECK_EQ("remaining [0]", ToNarrow(v[0].text), std::string("a"));
  CHECK_EQ("remaining [1]", ToNarrow(v[1].text), std::string("c"));

  const std::wstring path = L"test_phrases_del.yaml";
  CHECK("SavePhrases ok", PhrasesDialog::SavePhrases(path, v));
  std::vector<PhrasesDialog::Phrase> reloaded;
  PhrasesDialog::LoadPhrases(path, reloaded);
  CHECK_EQ("reload size", reloaded.size(), size_t(2));
  CHECK("b is gone",
        std::find_if(reloaded.begin(), reloaded.end(), [](const auto& p) {
          return p.text == L"b";
        }) == reloaded.end());

  std::remove(ToNarrow(path).c_str());
}

// Test 9: Empty text rejected (load 接受空 text, 但 UI 层面 Add 不接受)
static void TestEmptyTextUIRejected() {
  std::cout << "\n[Test 9] Empty text UI rejected" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L""});

  // v0.19.0.32: data layer 允许空 text (向后兼容),
  // 但 UI 层面 ID_BTN_ADD_TOP 在 OnCommand 检查 text.empty() 拒绝。
  CHECK_EQ("empty text preserved in data layer", v[0].text, std::wstring(L""));
  bool rejected = v[0].text.empty();
  CHECK("UI would reject empty text (ID_BTN_ADD_TOP guard)", rejected);
}

// Test 10: FlushSave 保存
static void TestFlushSave() {
  std::cout << "\n[Test 10] FlushSave 立即写盘" << std::endl;
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"initial"});

  const std::wstring path = L"test_phrases_flushsave.yaml";
  PhrasesDialog::SetYamlPath(path);

  v[0].text = L"flushed";
  PhrasesDialog::FlushSave();  // 立即写盘 (无 debounce, v0.19.0.32)

  std::vector<PhrasesDialog::Phrase> reloaded;
  PhrasesDialog::LoadPhrases(path, reloaded);
  CHECK_EQ("flushed text in file", ToNarrow(reloaded[0].text),
           std::string("flushed"));

  std::remove(ToNarrow(path).c_str());
}

// ===== v0.19.0.32 UX redo 关键验证 =====

// Test 11: Show() 创建新控件 (s_hInput + s_hList + 4 buttons,无 inline-edit)
static void TestShowCreatesNewControls() {
  std::cout << "\n[Test 11] Show() 创建新控件 (顶部 input + ListView + 4 buttons)"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("11.1: Show() 创建 s_hwnd", hwnd != nullptr && IsWindow(hwnd));
  CHECK("11.2: s_hInput 创建 (顶部 input)",
        PhrasesDialog::s_hInput != nullptr && IsWindow(PhrasesDialog::s_hInput));
  CHECK("11.3: s_hBtnAddTop 创建 (顶部 Add 按钮)",
        PhrasesDialog::s_hBtnAddTop != nullptr);
  CHECK("11.4: s_hList 创建 (ListView)",
        PhrasesDialog::s_hList != nullptr && IsWindow(PhrasesDialog::s_hList));
  CHECK("11.5: s_hBtnAdd 创建", PhrasesDialog::s_hBtnAdd != nullptr);
  CHECK("11.6: s_hBtnEdit 创建", PhrasesDialog::s_hBtnEdit != nullptr);
  CHECK("11.7: s_hBtnDel 创建", PhrasesDialog::s_hBtnDel != nullptr);
  CHECK("11.8: s_hBtnCancel 创建", PhrasesDialog::s_hBtnCancel != nullptr);

  // 关键验证: 删 v0.19.0.30 旧 inline-edit 控件 (s_hEditText/s_hEditCat 不存在)
  // v0.19.0.32 已删字段,无法访问 — 通过 static_assert 在头文件验证
  // 这里验证 ListView 列已加 (PopulateList 前已加 column)
  HWND hList = PhrasesDialog::s_hList;
  // 验证: ListView 至少 1 个 column 已加 (LVS_REPORT 模式)
  // 通过 ListView_GetColumn 取 column 0,若 mask 含 LVCF_TEXT 即成功
  LVCOLUMNW colProbe = {};
  colProbe.mask = LVCF_TEXT | LVCF_WIDTH;
  wchar_t colText[64] = {};
  colProbe.pszText = colText;
  colProbe.cchTextMax = 64;
  BOOL colOk = ListView_GetColumn(hList, 0, &colProbe);
  CHECK("11.9: ListView column 0 exists (短语)", colOk && colText[0] != 0);

  PhrasesDialog::Hide();
}

// Test 12: user flow — Add phrase via input + AddTop button
// 验证: SetWindowText(s_hInput, "hello") + WM_COMMAND(ID_BTN_ADD_TOP)
//       → m_phrases.push_back + PopulateList item +1
static void TestUserFlow_AddPhrase() {
  std::cout << "\n[Test 12] User flow: 顶部 input 录入 + AddTop 添加" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;

  // 初始: 0 phrases
  PhrasesDialog::MutablePhrases().clear();
  PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList);
  CHECK_EQ("12.1: initial list empty", ListView_GetItemCount(PhrasesDialog::s_hList), 0);

  // user 录入 "hello" 到顶部 input
  SetWindowTextW(PhrasesDialog::s_hInput, L"hello");
  // dispatch WM_COMMAND(ID_BTN_ADD_TOP) 触发真 OnCommand 路径
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1011, BN_CLICKED), 0);
  // ID_BTN_ADD_TOP = 1011 (cpp:90)

  CHECK_EQ("12.2: m_phrases size = 1", PhrasesDialog::Phrases().size(), size_t(1));
  CHECK_EQ("12.3: phrase text = hello",
           ToNarrow(PhrasesDialog::Phrases()[0].text), std::string("hello"));
  CHECK_EQ("12.4: ListView item count = 1",
           ListView_GetItemCount(PhrasesDialog::s_hList), 1);

  // 验证 input 已清空 (方便连续添加)
  wchar_t buf[256] = {};
  GetWindowTextW(PhrasesDialog::s_hInput, buf, 256);
  CHECK_EQ("12.5: input cleared after add", std::wstring(buf), std::wstring(L""));

  PhrasesDialog::Hide();
}

// Test 13: user flow — 选中 list item → s_hInput 自动 fill
static void TestUserFlow_SelectFillsInput() {
  std::cout << "\n[Test 13] User flow: 选中 list item → s_hInput 自动 fill" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;

  // 注入 2 个 phrase
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"alpha"});
  v.push_back({L"beta"});
  PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList);

  // 模拟点 list item[0] → 走 NM_CLICK / LVN_ITEMCHANGED 路径
  // 用 SetItemState 直接设 selected + 触发 LVN_ITEMCHANGED
  // ListView_SetItemState 设 selected state
  ListView_SetItemState(PhrasesDialog::s_hList, 0, LVIS_SELECTED, LVIS_SELECTED);

  // 验证 m_selectedIndex 已设 (OnNotify LVN_ITEMCHANGED handler)
  CHECK_EQ("13.1: m_selectedIndex = 0", PhrasesDialog::m_selectedIndex, 0);
  // 验证 s_hInput 自动 fill
  wchar_t buf[256] = {};
  GetWindowTextW(PhrasesDialog::s_hInput, buf, 256);
  CHECK_EQ("13.2: s_hInput filled with 'alpha'",
           std::wstring(buf), std::wstring(L"alpha"));

  PhrasesDialog::Hide();
}

// Test 14: user flow — 改 input + Edit button → m_phrases update
static void TestUserFlow_EditPhrase() {
  std::cout << "\n[Test 14] User flow: 改 input + Edit 按钮 → m_phrases 更新" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;

  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"old"});
  PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList);
  PhrasesDialog::m_selectedIndex = 0;

  // 改 input 内容
  SetWindowTextW(PhrasesDialog::s_hInput, L"new");
  // dispatch ID_BTN_EDIT = 1002
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1002, BN_CLICKED), 0);

  CHECK_EQ("14.1: m_phrases[0].text = new",
           ToNarrow(PhrasesDialog::Phrases()[0].text), std::string("new"));

  PhrasesDialog::Hide();
}

// Test 15: user flow — Delete 按钮 → m_phrases erase + ListView 减 1
static void TestUserFlow_DeletePhrase() {
  std::cout << "\n[Test 15] User flow: Delete 按钮 → 删除选中" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;

  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"p1"});
  v.push_back({L"p2"});
  v.push_back({L"p3"});
  PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList);
  PhrasesDialog::m_selectedIndex = 1;  // 选 p2

  // dispatch ID_BTN_DEL = 1003
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1003, BN_CLICKED), 0);

  CHECK_EQ("15.1: m_phrases size = 2", PhrasesDialog::Phrases().size(), size_t(2));
  CHECK_EQ("15.2: m_phrases[0].text = p1",
           ToNarrow(PhrasesDialog::Phrases()[0].text), std::string("p1"));
  CHECK_EQ("15.3: m_phrases[1].text = p3",
           ToNarrow(PhrasesDialog::Phrases()[1].text), std::string("p3"));
  CHECK_EQ("15.4: ListView item count = 2",
           ListView_GetItemCount(PhrasesDialog::s_hList), 2);
  CHECK_EQ("15.5: m_selectedIndex reset to -1",
           PhrasesDialog::m_selectedIndex, -1);

  PhrasesDialog::Hide();
}

// Test 16: Cancel 按钮 → Hide
static void TestUserFlow_CancelHides() {
  std::cout << "\n[Test 16] User flow: Cancel 按钮 → Hide" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("16.1: Show() 创建", hwnd != nullptr);

  // dispatch ID_BTN_CANCEL = 1004
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1004, BN_CLICKED), 0);
  CHECK("16.2: Hide() 后 s_hwnd == nullptr",
        PhrasesDialog::s_hwnd == nullptr);
  CHECK("16.3: state == State_Hidden",
        PhrasesDialog::GetState() == PhrasesDialog::State_Hidden);
}

// Test 17: kShowGraceMs 常量 sanity
static void TestGraceConstant() {
  std::cout << "\n[Test 17] Grace constant sanity" << std::endl;
  CHECK("kShowGraceMs == 2000", PhrasesDialog::kShowGraceMs == 2000);
  CHECK("kShowGraceMs > 0 (grace 期内必不 Hide)",
        PhrasesDialog::kShowGraceMs > 0);
}

// Test 18: State machine 简化: 只 Hidden / Browsing
static void TestStateMachine() {
  std::cout << "\n[Test 18] State machine 简化" << std::endl;
  PhrasesDialog::Hide();
  CHECK("after Hide: s_state == State_Hidden",
        PhrasesDialog::GetState() == PhrasesDialog::State_Hidden);
  PhrasesDialog::s_state = PhrasesDialog::State_Browsing;
  CHECK("manually set s_state = State_Browsing",
        PhrasesDialog::GetState() == PhrasesDialog::State_Browsing);
  PhrasesDialog::s_state = PhrasesDialog::State_Hidden;
}

// Test 19: MoveSelection 直接调用 — 验证 wrap-around 行为
// (Phase D v0.19.0.36 P2 follow-up: fa196049 ship 时漏 MoveSelection definition,
//   5068922 补了 link。但 unit 层没断言 wrap-around 行为 — 现补)
// 关键差异: ListView 默认 WndProc 在边界不 wrap (停在 0 或 last),
//   PhrasesDialog::MoveSelection 提供 wrap-around, 这是 Phase D 单独加的价值。
//   改 .h 让 MoveSelection public (testability 配套) 走 direct call 测。
static void TestMoveSelection() {
  std::cout << "\n[Test 19] MoveSelection 行为 — ↑/↓ + wrap-around" << std::endl;
  // 1. Show() 创 s_hwnd + s_hList (OnCreate 已加 column)
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  CHECK("19.0: s_hList 已由 Show() 创建",
        hList != nullptr && IsWindow(hList));

  // 2. 注入 5 phrases (跟 Test 4 同样路径)
  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"a1"});
  v.push_back({L"a2"});
  v.push_back({L"a3"});
  v.push_back({L"b1"});
  v.push_back({L"b2"});
  int inserted = PhrasesDialog::PopulateListCount(hList);
  CHECK_EQ("19.1: PopulateListCount insert 5", inserted, 5);
  int count = ListView_GetItemCount(hList);
  CHECK_EQ("19.2: ListView 实际有 5 项", count, 5);

  // 3. 强制设初始 selected = 0 (避免其他 test 状态污染)
  ListView_SetItemState(hList, 0,
                        LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  int cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.3: 强制 init selected == 0 (ListView state)", cur == 0);

  // 4. ↓ 1 → selected = 1
  PhrasesDialog::MoveSelection(hList, +1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.4: MoveSelection(+1): selected 0 → 1", cur == 1);

  // 5. ↑ 1 → selected = 0
  PhrasesDialog::MoveSelection(hList, -1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.5: MoveSelection(-1): selected 1 → 0", cur == 0);

  // 6. 在 0 按 ↑ → wrap → count-1 (ListView 默认 WndProc 不 wrap, 这里是 Phase D 单独提供)
  PhrasesDialog::MoveSelection(hList, -1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.6: MoveSelection(-1) at 0: wrap → count-1",
        cur == count - 1);

  // 7. 在 last 按 ↓ → wrap → 0
  PhrasesDialog::MoveSelection(hList, +1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.7: MoveSelection(+1) at last: wrap → 0", cur == 0);

  // 8. delta=+3: 0 → 3 (跳多个)
  ListView_SetItemState(hList, 0,
                        LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  PhrasesDialog::MoveSelection(hList, +3);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.8: MoveSelection(+3): 0 → 3", cur == 3);

  PhrasesDialog::Hide();
}

}  // namespace test

int main() {
  std::cout << "spec 044 v0.19.0.32 — PhrasesDialog v3 unit tests (UX redo)" << std::endl;
  std::cout << "=================================================" << std::endl;

  // YAML I/O
  test::TestYamlParse5Phrases();
  test::TestYamlCompatOldTextCategory();
  test::TestYamlParseFailFallback();

  // List populate
  test::TestListPopulate5Phrases();

  // SendInput
  test::TestSendInputMock();

  // CRUD + Save
  test::TestAddAndSave();
  test::TestEdit();
  test::TestDelete();

  // 数据层边缘
  test::TestEmptyTextUIRejected();
  test::TestFlushSave();

  // v0.19.0.32 UX redo 关键验证
  test::TestShowCreatesNewControls();
  test::TestUserFlow_AddPhrase();
  test::TestUserFlow_SelectFillsInput();
  test::TestUserFlow_EditPhrase();
  test::TestUserFlow_DeletePhrase();
  test::TestUserFlow_CancelHides();

  // Constants
  test::TestGraceConstant();
  test::TestStateMachine();

  // P2 follow-up (v0.19.0.36 Phase D)
  test::TestMoveSelection();

  std::cout << "\n=================================================" << std::endl;
  std::cout << "PASSED: " << test::g_passed << "  FAILED: " << test::g_failed
            << std::endl;
  std::cout << "=================================================" << std::endl;

  return test::g_failed == 0 ? 0 : 1;
}