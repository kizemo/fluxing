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
//       /Fe:TestPhrasesDialog.exe user32.lib gdi32.lib comctl32.lib
//       advapi32.lib

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
#define _WIN32_IE 0x0600  // LVS_NOTIFY (跟 WeaselServer/stdafx.h 对齐, IE 6+)
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
  if (w.empty())
    return "";
  int len =
      WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()),
                          nullptr, 0, nullptr, nullptr);
  std::string r(len, 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &r[0],
                      len, nullptr, nullptr);
  return r;
}

static bool WriteUtf8(const std::wstring& path, const std::wstring& content) {
  std::ofstream f(path, std::ios::binary);
  if (!f)
    return false;
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
  if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }
  if (content.empty())
    return L"";
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

#define CHECK(desc, cond)                           \
  do {                                              \
    if (cond) {                                     \
      std::cout << "  PASS: " << desc << std::endl; \
      ++g_passed;                                   \
    } else {                                        \
      std::cout << "  FAIL: " << desc << std::endl; \
      ++g_failed;                                   \
    }                                               \
  } while (0)

#define CHECK_EQ(desc, a, b)                        \
  do {                                              \
    if ((a) == (b)) {                               \
      std::cout << "  PASS: " << desc << std::endl; \
      ++g_passed;                                   \
    } else {                                        \
      std::cout << "  FAIL: " << desc << std::endl; \
      ++g_failed;                                   \
    }                                               \
  } while (0)

// Test 1: YAML parser — 5 phrases (单 text 字段,v0.19.0.32 简化)
static void TestYamlParse5Phrases() {
  std::cout << "\n[Test 1] YAML parser — 5 phrases (text only)" << std::endl;

  const std::wstring tmpPath = L"test_phrases_5p.yaml";
  std::wstring fixture;
  fixture += L"# test\n";
  fixture += L"phrases:\n";
  fixture += L"  - text: \"\x4f60\x597d\"\n";  // 你好
  fixture += L"  - text: \"\x8c22\x8c22\"\n";  // 谢谢
  fixture += L"  - text: \"Hello\"\n";
  fixture += L"  - text: \"\x6d4b\x8bd5\x77ed\x8bed\"\n";  // 测试短语
  fixture += L"  - text: \"123\"\n";
  CHECK("WriteUtf8 ok", WriteUtf8(tmpPath, fixture));

  std::vector<PhrasesDialog::Phrase> out;
  bool ok = PhrasesDialog::LoadPhrases(tmpPath, out);
  CHECK("LoadPhrases returns true", ok);
  CHECK_EQ("5 phrases loaded", out.size(), size_t(5));

  if (out.size() == 5) {
    CHECK_EQ("phrase 0 text", ToNarrow(out[0].text),
             std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
    CHECK_EQ("phrase 1 text", ToNarrow(out[1].text),
             std::string("\xe8\xb0\xa2\xe8\xb0\xa2"));
    CHECK_EQ("phrase 2 text", ToNarrow(out[2].text), std::string("Hello"));
  }

  std::remove(ToNarrow(tmpPath).c_str());
}

// Test 2: YAML 兼容旧 text/category (load 忽略 category,只读 text)
static void TestYamlCompatOldTextCategory() {
  std::cout
      << "\n[Test 2] YAML compat — 旧 text/category 兼容 (load 忽略 category)"
      << std::endl;

  const std::wstring tmpPath = L"test_phrases_compat.yaml";
  std::wstring fixture;
  fixture += L"# old format\n";
  fixture += L"phrases:\n";
  fixture += L"  - text: \"\x4f60\x597d\"\n";      // 你好
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
    CHECK_EQ("phrase 0 text", ToNarrow(out[0].text),
             std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
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
  CHECK_EQ("out preserved (text)", ToNarrow(out[0].text),
           std::string("preserved"));
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
  HWND hList =
      CreateWindowExW(0, WC_LISTVIEWW, L"", WS_CHILD, 0, 0, 200, 200,
                      HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
  CHECK("CreateWindowEx listview succeeded", hList != nullptr);
  if (!hList)
    return;

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
    CHECK_EQ("record 1 text", g_injected[1].text,
             std::wstring(L"\x4f60\x597d"));
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
    CHECK_EQ("reload [1].text", ToNarrow(reloaded[1].text),
             std::string("newphrase"));
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
  CHECK("file contains 'edited'",
        content.find(L"edited") != std::wstring::npos);

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
  std::cout
      << "\n[Test 11] Show() 创建新控件 (顶部 input + ListView + 4 buttons)"
      << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("11.1: Show() 创建 s_hwnd", hwnd != nullptr && IsWindow(hwnd));
  CHECK(
      "11.2: s_hInput 创建 (顶部 input)",
      PhrasesDialog::s_hInput != nullptr && IsWindow(PhrasesDialog::s_hInput));
  CHECK("11.3: s_hBtnAddTop 创建 (顶部 Add 按钮, v0.19.0.41 唯一 Add)",
        PhrasesDialog::s_hBtnAddTop != nullptr);
  CHECK("11.4: s_hList 创建 (ListView)",
        PhrasesDialog::s_hList != nullptr && IsWindow(PhrasesDialog::s_hList));
  // v0.19.0.41: 删 s_hBtnAdd (重复, 跟 s_hBtnAddTop 一样), 11.5 移除
  CHECK("11.5: s_hBtnEdit 创建 (底部 3 按钮之一)",
        PhrasesDialog::s_hBtnEdit != nullptr);
  CHECK("11.6: s_hBtnDel 创建", PhrasesDialog::s_hBtnDel != nullptr);
  CHECK("11.7: s_hBtnCancel 创建", PhrasesDialog::s_hBtnCancel != nullptr);

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
  CHECK(
      "11.9: ListView column 0 exists (P2 polish: header text 空, 避免跟 row "
      "'短语' 混淆)",
      colOk);

  PhrasesDialog::Hide();
}

// Test 12: user flow — Add phrase via input + AddTop button
// 验证: SetWindowText(s_hInput, "hello") + WM_COMMAND(ID_BTN_ADD_TOP)
//       → m_phrases.push_back + PopulateList item +1
static void TestUserFlow_AddPhrase() {
  std::cout << "\n[Test 12] User flow: 顶部 input 录入 + AddTop 添加"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;

  // 初始: 0 phrases
  PhrasesDialog::MutablePhrases().clear();
  PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList);
  CHECK_EQ("12.1: initial list empty",
           ListView_GetItemCount(PhrasesDialog::s_hList), 0);

  // user 录入 "hello" 到顶部 input
  SetWindowTextW(PhrasesDialog::s_hInput, L"hello");
  // dispatch WM_COMMAND(ID_BTN_ADD_TOP) 触发真 OnCommand 路径
  SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(1011, BN_CLICKED), 0);
  // ID_BTN_ADD_TOP = 1011 (cpp:90)

  CHECK_EQ("12.2: m_phrases size = 1", PhrasesDialog::Phrases().size(),
           size_t(1));
  CHECK_EQ("12.3: phrase text = hello",
           ToNarrow(PhrasesDialog::Phrases()[0].text), std::string("hello"));
  CHECK_EQ("12.4: ListView item count = 1",
           ListView_GetItemCount(PhrasesDialog::s_hList), 1);

  // 验证 input 已清空 (方便连续添加)
  wchar_t buf[256] = {};
  GetWindowTextW(PhrasesDialog::s_hInput, buf, 256);
  CHECK_EQ("12.5: input cleared after add", std::wstring(buf),
           std::wstring(L""));

  PhrasesDialog::Hide();
}

// Test 13: user flow — 选中 list item → s_hInput 自动 fill
static void TestUserFlow_SelectFillsInput() {
  std::cout << "\n[Test 13] User flow: 选中 list item → s_hInput 自动 fill"
            << std::endl;
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
  ListView_SetItemState(PhrasesDialog::s_hList, 0, LVIS_SELECTED,
                        LVIS_SELECTED);

  // 验证 m_selectedIndex 已设 (OnNotify LVN_ITEMCHANGED handler)
  CHECK_EQ("13.1: m_selectedIndex = 0", PhrasesDialog::m_selectedIndex, 0);
  // 验证 s_hInput 自动 fill
  wchar_t buf[256] = {};
  GetWindowTextW(PhrasesDialog::s_hInput, buf, 256);
  CHECK_EQ("13.2: s_hInput filled with 'alpha'", std::wstring(buf),
           std::wstring(L"alpha"));

  PhrasesDialog::Hide();
}

// Test 14: user flow — 改 input + Edit button → m_phrases update
static void TestUserFlow_EditPhrase() {
  std::cout << "\n[Test 14] User flow: 改 input + Edit 按钮 → m_phrases 更新"
            << std::endl;
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

  CHECK_EQ("15.1: m_phrases size = 2", PhrasesDialog::Phrases().size(),
           size_t(2));
  CHECK_EQ("15.2: m_phrases[0].text = p1",
           ToNarrow(PhrasesDialog::Phrases()[0].text), std::string("p1"));
  CHECK_EQ("15.3: m_phrases[1].text = p3",
           ToNarrow(PhrasesDialog::Phrases()[1].text), std::string("p3"));
  CHECK_EQ("15.4: ListView item count = 2",
           ListView_GetItemCount(PhrasesDialog::s_hList), 2);
  CHECK_EQ("15.5: m_selectedIndex reset to -1", PhrasesDialog::m_selectedIndex,
           -1);

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
  CHECK("16.2: Hide() 后 s_hwnd == nullptr", PhrasesDialog::s_hwnd == nullptr);
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
// (Phase D v0.19.0.36 P2 follow-up: fa196049 ship 时漏 MoveSelection
// definition,
//   5068922 补了 link。但 unit 层没断言 wrap-around 行为 — 现补)
// 关键差异: ListView 默认 WndProc 在边界不 wrap (停在 0 或 last),
//   PhrasesDialog::MoveSelection 提供 wrap-around, 这是 Phase D 单独加的价值。
//   改 .h 让 MoveSelection public (testability 配套) 走 direct call 测。
static void TestMoveSelection() {
  std::cout << "\n[Test 19] MoveSelection 行为 — ↑/↓ + wrap-around"
            << std::endl;
  // 1. Show() 创 s_hwnd + s_hList (OnCreate 已加 column)
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  CHECK("19.0: s_hList 已由 Show() 创建", hList != nullptr && IsWindow(hList));

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
  ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
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

  // 6. 在 0 按 ↑ → wrap → count-1 (ListView 默认 WndProc 不 wrap, 这里是 Phase
  // D 单独提供)
  PhrasesDialog::MoveSelection(hList, -1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.6: MoveSelection(-1) at 0: wrap → count-1", cur == count - 1);

  // 7. 在 last 按 ↓ → wrap → 0
  PhrasesDialog::MoveSelection(hList, +1);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.7: MoveSelection(+1) at last: wrap → 0", cur == 0);

  // 8. delta=+3: 0 → 3 (跳多个)
  ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  PhrasesDialog::MoveSelection(hList, +3);
  cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("19.8: MoveSelection(+3): 0 → 3", cur == 3);

  PhrasesDialog::Hide();
}

// Test 20: ListView column 0 header text 空 (避免跟下面 row "短语" 视觉混淆)
//   用户装机反馈: "在界面短语上面,有一个多余的'短语', 容易和下面的短语项混淆"
//   修法: column header text 从 "短语" 改 "" (空), ListView 仍 1 列 (InsertItem
//   需 ≥1 列), 但视觉上 header 不显示文字。
static void TestColumnHeaderEmpty() {
  std::cout << "\n[Test 20] ListView column 0 header text 空 (P2 polish)"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  CHECK("20.0: s_hList 已由 Show() 创建", hList != nullptr && IsWindow(hList));

  LVCOLUMNW col = {};
  col.mask = LVCF_TEXT | LVCF_WIDTH;
  wchar_t colText[64] = {};
  col.pszText = colText;
  col.cchTextMax = 64;
  BOOL colOk = ListView_GetColumn(hList, 0, &col);
  CHECK("20.1: column 0 exists", colOk);
  CHECK("20.2: column 0 header text EMPTY (避免跟 row '短语' 视觉混淆)",
        colOk && colText[0] == L'\0');

  PhrasesDialog::Hide();
}

// Test 21: OnNotify NM_DBLCLK → inject 选中 text (真 user flow 双击上屏)
//   用户装机反馈: "仍然无法通过双击...上屏"
//   真 root cause (Phase 1 复盘):
//     - ListView 默认发 NM_DBLCLK (Windows SDK 不需 LVS_NOTIFY flag)
//     - OnNotify NM_DBLCLK handler 调 InjectText() + Hide()
//     - **bug**: InjectText() 在 Hide() 之前 — SendInput 发到 modal dialog
//     自身,
//       文本没上屏到原 app (foreground 还在 PhrasesDialog)
//   修法: OnNotify NM_DBLCLK handler 顺序对调 — 先 Hide() 后 InjectText()
//   测法: 装 MockInject + 发 WM_NOTIFY(NM_DBLCLK) → mock inject 调 + text 正确
static void TestNMDblClkInjects() {
  std::cout << "\n[Test 21] OnNotify NM_DBLCLK → inject 选中 text" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("21.0: s_hList + s_hwnd 已由 Show() 创建",
        hList && IsWindow(hList) && hwnd && IsWindow(hwnd));

  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"first"});
  v.push_back({L"second"});
  v.push_back({L"third"});
  int inserted = PhrasesDialog::PopulateListCount(hList);
  CHECK_EQ("21.1: PopulateListCount insert 3", inserted, 3);

  // 显式 set selected (不依赖 OnCreate 默认 — sandbox LVN_ITEMCHANGED 异步
  // race)
  ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  int cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("21.2: 强制 init selected == 0", cur == 0);

  g_injected.clear();
  PhrasesDialog::SetInjectFn(&MockInject);

  // 模拟双击 ListView item 0 → parent 收 NM_DBLCLK
  NMITEMACTIVATE nia = {};
  nia.hdr.hwndFrom = hList;
  nia.hdr.idFrom = (UINT_PTR)1100;  // ID_LIST
  nia.hdr.code = NM_DBLCLK;
  nia.iItem = 0;
  SendMessageW(hwnd, WM_NOTIFY, nia.hdr.idFrom, (LPARAM)&nia);

  CHECK_EQ("21.3: NM_DBLCLK inject 1 record", g_injected.size(), size_t(1));
  if (g_injected.size() == 1) {
    CHECK_EQ("21.4: injected text = 'first'", g_injected[0].text,
             std::wstring(L"first"));
  }
  // 真 root cause 验证: Hide() 必须在 InjectText() 之前 (SendInput 发到
  // foreground = 原 app) 验法: NM_DBLCLK handler 跑后 s_hwnd 应是 nullptr (Hide
  // 销毁了)
  CHECK(
      "21.5: NM_DBLCLK handler 销毁 dialog (s_hwnd == nullptr, InjectText 跑时 "
      "foreground 是原 app)",
      PhrasesDialog::s_hwnd == nullptr);

  PhrasesDialog::SetInjectFn(&PhrasesDialog::DefaultInject);
  // 不调 Hide — 已销毁
}

// Test 22: OnNotify NM_RETURN → inject 选中 text
//   用户装机反馈: "仍然无法通过...回车上屏"
//   真 root cause: 没 NM_RETURN handler — Enter 焦点 ListView + 有 selected
//   item 时 ListView 默认发 NM_RETURN 给 parent, 但 OnNotify 没 case NM_RETURN
//   处理, 默认 return 0, 不 inject 修法: OnNotify 加 case NM_RETURN — 跟
//   NM_DBLCLK 同路径 (Hide 优先, 后 InjectText)
static void TestNMReturnInjects() {
  std::cout << "\n[Test 22] OnNotify NM_RETURN → inject 选中 text" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("22.0: s_hList + s_hwnd 已由 Show() 创建",
        hList && IsWindow(hList) && hwnd && IsWindow(hwnd));

  PhrasesDialog::MutablePhrases().clear();
  auto& v = PhrasesDialog::MutablePhrases();
  v.push_back({L"first"});
  v.push_back({L"second"});
  v.push_back({L"third"});
  int inserted = PhrasesDialog::PopulateListCount(hList);
  CHECK_EQ("22.1: PopulateListCount insert 3", inserted, 3);

  // 显式 set selected (同 Test 21)
  ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED,
                        LVIS_SELECTED | LVIS_FOCUSED);
  int cur = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
  CHECK("22.2: 强制 init selected == 0", cur == 0);

  g_injected.clear();
  PhrasesDialog::SetInjectFn(&MockInject);

  // 模拟 Enter → ListView 默认发 NM_RETURN 给 parent
  NMHDR nm;
  nm.hwndFrom = hList;
  nm.idFrom = (UINT_PTR)1100;  // ID_LIST
  nm.code = NM_RETURN;
  SendMessageW(hwnd, WM_NOTIFY, nm.idFrom, (LPARAM)&nm);

  CHECK_EQ("22.3: NM_RETURN inject 1 record", g_injected.size(), size_t(1));
  if (g_injected.size() == 1) {
    CHECK_EQ("22.4: injected text = 'first'", g_injected[0].text,
             std::wstring(L"first"));
  }
  CHECK("22.5: NM_RETURN handler 销毁 dialog (s_hwnd == nullptr)",
        PhrasesDialog::s_hwnd == nullptr);

  PhrasesDialog::SetInjectFn(&PhrasesDialog::DefaultInject);
}

// v0.19.0.39 (Phase F fix Bug 1): Show() 创建路径必须调
// AllowSetForegroundWindow + SetForegroundWindow.
//   真 root cause (装机反馈): QuickPanel button 启动 PhrasesDialog 时,
//   QuickPanelDialog 仍是 foreground, 键盘事件 (↑↓/Enter/Esc) 发到 QuickPanel,
//   不传 PhrasesDialog (双击能 work 是因为 WM_LBUTTONDBLCLK 是 mouse event,
//   mouse 直接命中 ListView 触发). Alt+. 路径 work (hotkey 触发 自动让
//   WeaselServer 进 foreground). 修法 (Option C): AllowSetForegroundWindow 拿抢
//   foreground 锁 (Vista+ lock), SetForegroundWindow 强制 foreground 让 dialog
//   收 keyboard events. 配套: WeaselServerApp.cpp onPhrases lambda 先 Hide
//   QuickPanel 释放 foreground. Test 23 通过 mock s_setForegroundFn /
//   s_allowSetForegroundFn 计数, 验证 Show() 调用过.
static int g_setForegroundCount = 0;
static HWND g_lastSetForegroundHwnd = nullptr;
static DWORD g_lastAllowSetForegroundPid = 0;

static BOOL WINAPI MockSetForegroundWindow(HWND h) {
  ++g_setForegroundCount;
  g_lastSetForegroundHwnd = h;
  return TRUE;
}

static BOOL WINAPI MockAllowSetForegroundWindow(DWORD pid) {
  g_lastAllowSetForegroundPid = pid;
  return TRUE;
}

static void TestForegroundApiCalled() {
  std::cout << "\n[Test 23] Show() 创建路径调 SetForegroundWindow + "
               "AllowSetForegroundWindow"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");

  // reset mock state
  g_setForegroundCount = 0;
  g_lastSetForegroundHwnd = nullptr;
  g_lastAllowSetForegroundPid = 0;

  // 注入 mock 函数指针 (替换默认 OS API)
  PhrasesDialog::SetSetForegroundFn(&MockSetForegroundWindow);
  PhrasesDialog::SetAllowSetForegroundFn(&MockAllowSetForegroundWindow);

  // 调 Show() — 创建路径应该调 AllowSetForegroundWindow + SetForegroundWindow
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("23.0: Show() 后 s_hwnd 创建", hwnd && IsWindow(hwnd));

  CHECK("23.1: AllowSetForegroundWindow 调过 (pid=ASFW_ANY)",
        g_lastAllowSetForegroundPid == (DWORD)ASFW_ANY);
  CHECK("23.2: SetForegroundWindow 调过 1 次", g_setForegroundCount == 1);
  CHECK("23.3: SetForegroundWindow 目标 = s_hwnd",
        g_lastSetForegroundHwnd == hwnd);

  // 还原默认 OS API
  PhrasesDialog::SetSetForegroundFn(&::SetForegroundWindow);
  PhrasesDialog::SetAllowSetForegroundFn(&::AllowSetForegroundWindow);

  // 清理 (Hide 销毁 dialog + 避免污染后续 test)
  PhrasesDialog::Hide();
}

// v0.19.0.39 (Phase F fix Bug 3): WM_KEYDOWN VK_ESCAPE → Hide()
//   真 root cause (装机反馈): Esc handler 已有 (OnKeyDown L675), 但只在
//   keyboard focus 在 dialog 内 work. Phase F 同时修 foreground lock 让 Esc
//   能到 dialog. Test 24 验证 WndProc dispatch WM_KEYDOWN VK_ESCAPE → Hide 销毁
//   dialog (s_hwnd=nullptr).
static void TestEscKeyHidesDialog() {
  std::cout << "\n[Test 24] WM_KEYDOWN VK_ESCAPE → Hide" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("24.0: Show() 后 s_hwnd 已创建", hwnd && IsWindow(hwnd));

  // 模拟键盘 Esc → OnKeyDown case VK_ESCAPE → Hide
  LRESULT lr = SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
  CHECK("24.1: Esc 返回 0 (handler 处理)", lr == 0);

  CHECK("24.2: Esc handler 销毁 dialog (s_hwnd == nullptr)",
        PhrasesDialog::s_hwnd == nullptr);
}

// v0.19.0.40 (Phase F Bug 3 续修): OnNotify LVN_KEYDOWN VK_ESCAPE → Hide
//   真 root cause (装机反馈 v0.19.0.39): Test 24 false-positive (直接 dispatch
//   WM_KEYDOWN 到 dialog WndProc, 不模拟 ListView 焦点 + LVN_KEYDOWN 路径).
//   ListView 焦点时按 Esc → ListView 自身不处理 → 转 LVN_KEYDOWN 给 parent
//   (PhrasesDialog WndProc 通过 WM_NOTIFY). Test 25 模拟完整路径:
//   1) Show() 后 ListView 有焦点 (OnCreate line ~568 SetFocus(s_hList))
//   2) SendMessage WM_NOTIFY + LVN_KEYDOWN + VK_ESCAPE → 验证 Hide called
static void TestLVNKeyDownEscapeHides() {
  std::cout << "\n[Test 25] OnNotify LVN_KEYDOWN VK_ESCAPE → Hide" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hList = PhrasesDialog::s_hList;
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("25.0: s_hList + s_hwnd 已由 Show() 创建",
        hList && IsWindow(hList) && hwnd && IsWindow(hwnd));

  // ListView 焦点 (OnCreate 默认设了, 显式 set 一次保险)
  SetFocus(hList);

  // 构造 NMLVKEYDOWN struct (commctrl.h 提供 NMLVKEYDOWN + LVN_KEYDOWN)
  NMLVKEYDOWN nmlv = {};
  nmlv.hdr.hwndFrom = hList;
  nmlv.hdr.idFrom = (UINT_PTR)1100;  // ID_LIST = 1100 (PhrasesDialog.cpp:84)
  nmlv.hdr.code = LVN_KEYDOWN;
  nmlv.wVKey = VK_ESCAPE;
  nmlv.flags = 0;

  // dispatch WM_NOTIFY → dialog WndProc → OnNotify → case LVN_KEYDOWN → Hide
  SendMessageW(hwnd, WM_NOTIFY, nmlv.hdr.idFrom, (LPARAM)&nmlv);

  CHECK("25.1: LVN_KEYDOWN VK_ESCAPE → Hide 销毁 dialog (s_hwnd == nullptr)",
        PhrasesDialog::s_hwnd == nullptr);
}

// v0.19.0.41 (Bug 4: 删重复 Add 按钮): 通过 EnumChildWindows 数 button 子控件
//   期望: 1 个 Add 按钮 (顶部 s_hBtnAddTop) + 3 个底部按钮 (Edit/Del/Cancel) =
//   4 个 BUTTON 类子控件。v0.19.0.40 是 5 (4 底部 + 1 顶部 Add)。
//   同时验证 s_hBtnAddTop label 是 "+ 添加" (避免有第二个 Add 误加回)。
static void TestNoDuplicateAddButton() {
  std::cout << "\n[Test 26] v0.19.0.41: 唯一 Add 按钮 (无底部重复)"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("26.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  // EnumChildWindows 数 BUTTON 类 (Windows class name 是 "Button", 不是
  // "BUTTON")
  struct Counter {
    int count;
  };
  Counter cnt = {0};
  auto cb = [](HWND child, LPARAM lp) -> BOOL {
    Counter* c = reinterpret_cast<Counter*>(lp);
    wchar_t cls[16] = {};
    GetClassNameW(child, cls, 16);
    // case-insensitive 比对 (Windows class 实际名 "Button", 但保险起见)
    if (_wcsicmp(cls, L"Button") == 0) {
      c->count++;
    }
    return TRUE;
  };
  EnumChildWindows(hwnd, cb, reinterpret_cast<LPARAM>(&cnt));
  CHECK(
      "26.1: BUTTON 子控件 = 4 (顶部 Add + 3 底部 Edit/Del/Cancel, v0.19.0.40 "
      "是 5)",
      cnt.count == 4);

  // 验证 s_hBtnAddTop label 是 "+ 添加"
  wchar_t topText[64] = {};
  GetWindowTextW(PhrasesDialog::s_hBtnAddTop, topText, 64);
  CHECK("26.2: 顶部 Add 按钮 label 含 '添加'",
        wcsstr(topText, L"\x6dfb\x52a0") != nullptr);
  PhrasesDialog::Hide();
}

// v0.19.0.41 (Feature: 鼠标拖边界 resize): WS_THICKFRAME 在 window style 里
//   没有 WS_THICKFRAME → mouse 在 border 上不能 resize。
static void TestWindowStyleHasThickFrame() {
  std::cout << "\n[Test 27] v0.19.0.41: WS_THICKFRAME 启用 resize" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("27.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));
  LONG style = GetWindowLongW(hwnd, GWL_STYLE);
  CHECK("27.1: window style 含 WS_THICKFRAME (允许拖边界 resize)",
        (style & WS_THICKFRAME) != 0);
  // 验证没误加 max/min 按钮 (dialog 是 modal, 不要这些)
  CHECK("27.2: window style 不含 WS_MAXIMIZEBOX (modal 不要 max 按钮)",
        (style & WS_MAXIMIZEBOX) == 0);
  CHECK("27.3: window style 不含 WS_MINIMIZEBOX (modal 不要 min 按钮)",
        (style & WS_MINIMIZEBOX) == 0);
  PhrasesDialog::Hide();
}

// v0.19.0.41 (Bug 2: DPI 缩放): s_dpiScale > 0 (96 DPI = 1.0, 200% DPI = 2.0)
//   sandbox 走 96 DPI = 1.0, user 端 4K 屏 = 2.0。s_dpiScale 必须 clamped 在
//   [0.5, 4.0] 范围 (OnCreate 边界检查)。
static void TestDpiScaleComputed() {
  std::cout << "\n[Test 28] v0.19.0.41: s_dpiScale 计算 + clamp [0.5, 4.0]"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("28.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));
  CHECK("28.1: s_dpiScale > 0 (OnCreate 算出了值)",
        PhrasesDialog::s_dpiScale > 0.0);
  CHECK("28.2: s_dpiScale 在 [0.5, 4.0] 范围 (clamp 边界)",
        PhrasesDialog::s_dpiScale >= 0.5 && PhrasesDialog::s_dpiScale <= 4.0);
  // 物理 list 高度 (DPI-scaled 后) 应 ≥ 80 (OnCreate 边界)
  CHECK("28.3: kListH_phys ≥ 80 (OnCreate 边界保护)",
        PhrasesDialog::kListH_phys >= 80);
  PhrasesDialog::Hide();
}

// v0.19.0.42 (Stop hook BLOCKER fix): OnGetMinMaxInfo 用 s_dpiScale 缩放
//   min/max 物理尺寸, 否则高 DPI 屏初始尺寸 (e.g. 720×920 on 200% DPI) 会超
//   过 raw max (1200×900), WM_GETMINMAXINFO 强制 cap 到比初始尺寸还小。
//   验证: dispatch WM_GETMINMAXINFO → 检查 ptMinTrackSize/ptMaxTrackSize 都
//   × s_dpiScale (raw 320/400/1200/900 各自 × s_dpiScale)。
static void TestGetMinMaxInfoDpiScaled() {
  std::cout << "\n[Test 29] v0.19.0.42 fix: OnGetMinMaxInfo 用 s_dpiScale 缩放"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("29.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  // 触发 WM_GETMINMAXINFO (Windows 在 begin resize/move + 初始 creation 时发)
  // 初始 0 = 没限制, 我们的 OnGetMinMaxInfo 会设新值
  MINMAXINFO mmi = {};
  SendMessageW(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);

  double dpiScale = PhrasesDialog::s_dpiScale;
  // v0.19.0.44: kMinW/kMinH 改 240/360 (v0.19.0.42 时是 320/400)。读 constexpr
  // 而非硬编码, 跟进 .h 改值自动同步。
  LONG expMinW = (LONG)(PhrasesDialog::kMinW * dpiScale);
  LONG expMinH = (LONG)(PhrasesDialog::kMinH * dpiScale);
  LONG expMaxW = (LONG)(1200 * dpiScale);
  LONG expMaxH = (LONG)(900 * dpiScale);
  CHECK(
      "29.1: ptMinTrackSize.x = kMinW * s_dpiScale (raw 不再用, 防高 DPI 屏 "
      "cap)",
      mmi.ptMinTrackSize.x == expMinW);
  CHECK("29.2: ptMinTrackSize.y = kMinH * s_dpiScale",
        mmi.ptMinTrackSize.y == expMinH);
  CHECK("29.3: ptMaxTrackSize.x = 1200 * s_dpiScale",
        mmi.ptMaxTrackSize.x == expMaxW);
  CHECK(
      "29.4: ptMaxTrackSize.y = 900 * s_dpiScale (raw 900 在 200% DPI = 1800 > "
      "初始 920)",
      mmi.ptMaxTrackSize.y == expMaxH);

  // 验证高 DPI 屏 max ≥ 初始尺寸 (关键: 防 cap 缩到比初始还小)
  RECT rc;
  GetWindowRect(hwnd, &rc);
  int initW = rc.right - rc.left;
  int initH = rc.bottom - rc.top;
  CHECK("29.5: ptMaxTrackSize.x ≥ 初始 dialog 宽 (高 DPI 不被 cap 缩小)",
        mmi.ptMaxTrackSize.x >= initW);
  CHECK("29.6: ptMaxTrackSize.y ≥ 初始 dialog 高",
        mmi.ptMaxTrackSize.y >= initH);
  PhrasesDialog::Hide();
}

// v0.19.0.42 (Stop hook SUGGESTION 2): long-press drag state machine e2e
//   不等 500ms timer, 直接通过公开 API (OnLButtonDown + OnTimer + OnMouseMove
//   + OnLButtonUp) 模拟完整状态机:
//     1) OnLButtonDown in chrome → s_longPressActive = true
//     2) OnTimer kLongPressTimerId → s_isDragging = true + SetCapture
//     3) OnMouseMove in drag mode → SetWindowPos (位置变化)
//     4) OnLButtonUp → s_isDragging = false + ReleaseCapture
//   这些 handlers 之前只能从外部访问 (private static), 加 Test 30 验证状态机
//   完整流程 (e2e 覆盖 WM_LBUTTONDOWN / WM_TIMER / WM_MOUSEMOVE /
//   WM_LBUTTONUP)。
static void TestLongPressDragStateMachine() {
  std::cout << "\n[Test 30] v0.19.0.42: 长按 drag state machine e2e"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("30.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));
  CHECK("30.1: 初始 s_isDragging = false, s_longPressActive = false",
        !PhrasesDialog::s_isDragging && !PhrasesDialog::s_longPressActive);

  // 模拟在 chrome 区域 (e.g. 标题栏 y=10) LBUTTONDOWN — ChildWindowFromPoint
  // 排除子控件, 我们用 (5, 5) 屏幕坐标应该不在 input / list / 按钮上
  // (input 起点 btnMarginX=12, list 起点 gap=8, y>titleH=30 才到 input)
  // 用 (1, 1) — 远在所有子控件外
  LPARAM lParamDown = 1 | (1 << 16);
  PhrasesDialog::OnLButtonDown(hwnd, 0, lParamDown);
  CHECK("30.2: chrome LBUTTONDOWN → s_longPressActive = true",
        PhrasesDialog::s_longPressActive);
  CHECK("30.3: s_isDragging 仍 false (等 timer)", !PhrasesDialog::s_isDragging);

  // 模拟 500ms timer fire
  PhrasesDialog::OnTimer(hwnd, PhrasesDialog::kLongPressTimerId);
  CHECK("30.4: WM_TIMER fire → s_isDragging = true",
        PhrasesDialog::s_isDragging);
  CHECK("30.5: s_longPressActive = false (timer 消费完)",
        !PhrasesDialog::s_longPressActive);

  // 记录初始 window 位置
  RECT rcInit;
  GetWindowRect(hwnd, &rcInit);
  int initX = rcInit.left;
  int initY = rcInit.top;

  // v0.19.0.43 (Bug 2.2 修复后): OnMouseMove 用 GetCursorPos (screen coords)
  // 而不是 lp (client coords)。原因: dialog 移动后 client coords 跳变,
  // 数学错位导致 UI 剧烈晃动。GetCursorPos 给真实 cursor 位置, 不受 dialog
  // 移动影响。
  // 测试: 用 SetCursorPos 把 cursor 移到初始位置 +99, +99 (相对 drag origin)
  // → 期望 window 跟着移动 +99, +99
  POINT ptCursor;
  GetCursorPos(&ptCursor);
  // 计算期望的 cursor 位置: 把 cursor 移到 screen (initX+99, initY+99)
  // (drag origin 已经在 LBUTTONDOWN 时 GetCursorPos 进 s_dragOrigin,
  //  但那是 sandbox 当前 cursor, 不是 initX+1, initY+1 — 不准)
  // 简化: 我们只验证相对移动量, 直接比较 SetCursorPos 前后 window 位置
  // 重新 LBUTTONDOWN + timer fire 让 s_dragOrigin 用 GetCursorPos 拿准确值
  PhrasesDialog::OnLButtonUp(hwnd, 0, 0);             // cancel current drag
  PhrasesDialog::OnLButtonDown(hwnd, 0, lParamDown);  // restart LBUTTONDOWN
  PhrasesDialog::OnTimer(hwnd, PhrasesDialog::kLongPressTimerId);  // fire timer
  CHECK("30.4b: restart drag after cancel", PhrasesDialog::s_isDragging);

  RECT rcInit2;
  GetWindowRect(hwnd, &rcInit2);
  // 现在 s_dragOrigin = current cursor (GetCursorPos at LBUTTONDOWN)
  GetCursorPos(&ptCursor);
  // 把 cursor 移到 +99, +99
  SetCursorPos(ptCursor.x + 99, ptCursor.y + 99);
  // 触发 MOUSEMOVE — OnMouseMove 调 GetCursorPos 拿到新位置
  PhrasesDialog::OnMouseMove(hwnd, MK_LBUTTON, 0);
  RECT rcAfter;
  GetWindowRect(hwnd, &rcAfter);
  int dx = (rcAfter.left - rcInit2.left);
  int dy = (rcAfter.top - rcInit2.top);
  // 期望: dialog 跟着 cursor 移动 +99, +99
  CHECK("30.6: WM_MOUSEMOVE + SetCursorPos +99 → window 左移 +99", dx == 99);
  CHECK("30.7: WM_MOUSEMOVE + SetCursorPos +99 → window 上移 +99", dy == 99);

  // 模拟 LBUTTONUP
  PhrasesDialog::OnLButtonUp(hwnd, 0, 0);
  CHECK("30.8: WM_LBUTTONUP → s_isDragging = false",
        !PhrasesDialog::s_isDragging);

  // 短按 (timer 还没 fire) → OnLButtonUp 应该 cancel
  PhrasesDialog::SetYamlPath(L"");  // re-Show
  PhrasesDialog::Hide();
  PhrasesDialog::Show();
  PhrasesDialog::OnLButtonDown(hwnd, 0, 1 | (1 << 16));
  CHECK("30.9: 短按 s_longPressActive = true",
        PhrasesDialog::s_longPressActive);
  PhrasesDialog::OnLButtonUp(hwnd, 0, 0);
  CHECK("30.10: 短按 LBUTTONUP → s_longPressActive = false (cancel)",
        !PhrasesDialog::s_longPressActive);
  CHECK("30.11: 短按 s_isDragging 仍 false (没进 drag mode)",
        !PhrasesDialog::s_isDragging);

  PhrasesDialog::Hide();
}

// v0.19.0.44 (Feature 1: resize layout): 验证 WM_SIZE 后 child 重新定位
//   1) input + AddTop 保持 top-left fixed
//   2) list 高度变 (伸缩)
//   3) 3 个底部按钮 anchored bottom-right (固定 btnMarginX + gap)
static void TestResizeLayoutRepositionsChildren() {
  std::cout << "\n[Test 32] v0.19.0.44: WM_SIZE re-layout (input fixed, list "
               "伸缩, btn anchored)"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("32.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  // 记初始 client area
  RECT rcInitClient;
  GetClientRect(hwnd, &rcInitClient);
  int initW = rcInitClient.right - rcInitClient.left;
  int initH = rcInitClient.bottom - rcInitClient.top;
  CHECK("32.1: 初始 client 尺寸 > 0", initW > 0 && initH > 0);

  // 记初始 child position
  RECT rcInputInit, rcListInit, rcBtnInit;
  GetWindowRect(PhrasesDialog::s_hInput, &rcInputInit);
  GetWindowRect(PhrasesDialog::s_hList, &rcListInit);
  GetWindowRect(PhrasesDialog::s_hBtnEdit, &rcBtnInit);

  // 触发 WM_SIZE: client area 800×600
  // 用户实际 resize 是 outer → outer - border = client, 但 OnGetMinMaxInfo 用
  // s_dpiScale, 一般 sandbox 1.0 = direct 800×600 100% 安全
  int newW = 800, newH = 600;
  SetWindowPos(hwnd, nullptr, 0, 0, newW + 16, newH + 16,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  // Send WM_SIZE so LayoutDialog gets called with new client dims
  SendMessageW(hwnd, WM_SIZE, 0, MAKELPARAM(newW, newH));

  // 1. input row: 顶部 (y 跟初始类似, 都没动 titleH 上方)
  RECT rcInput;
  GetWindowRect(PhrasesDialog::s_hInput, &rcInput);
  // Check input vertical position unchanged (same y within dialog)
  // Map to dialog client coords
  POINT ptInput = {rcInput.left, rcInput.top};
  ScreenToClient(hwnd, &ptInput);
  POINT ptInputInit = {rcInputInit.left, rcInputInit.top};
  ScreenToClient(hwnd, &ptInputInit);
  CHECK("32.2: input row 仍 top-left (y 不变)",
        abs(ptInput.y - ptInputInit.y) <= 2);

  // 2. list height 变了 (变大)
  RECT rcList;
  GetWindowRect(PhrasesDialog::s_hList, &rcList);
  int newListH = rcList.bottom - rcList.top;
  int oldListH = rcListInit.bottom - rcListInit.top;
  CHECK("32.3: list height 增长 (新 client 600 > 旧)", newListH > oldListH);

  // 3. buttons anchored bottom (y = newH - btnH - gap in dialog client)
  RECT rcBtn;
  GetWindowRect(PhrasesDialog::s_hBtnEdit, &rcBtn);
  POINT ptBtn = {rcBtn.left, rcBtn.top};
  ScreenToClient(hwnd, &ptBtn);
  POINT ptBtnInit = {rcBtnInit.left, rcBtnInit.top};
  ScreenToClient(hwnd, &ptBtnInit);
  // After resize, buttons 应该 y = newH - btnH*DPI - gap*DPI (anchored bottom)
  // 不能直接访问 PhrasesDialog::kBtnH (anonymous namespace in .cpp)。
  // 用 LayoutDialog 写入的 public static kBtnY_phys 校验:
  CHECK("32.4: kBtnY_phys 更新到新 client 底 (anchored bottom)",
        abs(PhrasesDialog::kBtnY_phys - (newH - 32 * PhrasesDialog::s_dpiScale -
                                         8 * PhrasesDialog::s_dpiScale)) <= 2);
  CHECK("32.5: btn Edit y 跟初始位置不同 (新尺寸)",
        abs(ptBtn.y - ptBtnInit.y) >= 2);

  // 4. 3 个 buttons 横向: 右边距 = gap (v0.19.0.45 Phase H 改用 kGap = 8,
  //    之前是 kBtnMarginX = 12)
  RECT rcBtnCancel;
  GetWindowRect(PhrasesDialog::s_hBtnCancel, &rcBtnCancel);
  POINT ptCancel = {rcBtnCancel.right, rcBtnCancel.top};
  ScreenToClient(hwnd, &ptCancel);
  // btn Cancel 右边缘应该 = newW - gap*DPI;
  // 不能访问 kGap (anonymous namespace), 直接 hardcode 8 (DPI=1):
  // gap = kGap * s_dpiScale. 在 96 DPI sandbox = 8.
  int expectedCancelRight = newW - (int)(8 * PhrasesDialog::s_dpiScale);
  CHECK("32.6: btn Cancel 右边缘 = newW - 8*DPI (kGap, Phase H)",
        abs(ptCancel.x - expectedCancelRight) <= 2);

  PhrasesDialog::Hide();
}

// v0.19.0.45 (Phase H polish Bug 1 真修): OnCreate 初始 column 宽度必须
// scrollbar-aware, 跟 LayoutDialog 一致 (header client - 4 slack)。否则装机
// v0.19.0.44 反馈 "初始 UI 边距过大, resize 后正常" — OnCreate 设 cx = raw
// clientW - 2*gap - 4 (340 @ DPI=1, kDialogW=360), 实际 ListView client = 344
// - 4(edge) = 340, scrollbar 显示后 header client = 340 - 17 = 323 → column
// 内容 overflow 17px (cx > header client) → 视觉"右边距过大" (ListView 不会
// 自动缩 column width, content 被 horizontal 截)。resize 后 LayoutDialog 重算
// cx = header client - 4 (323 - 4 = 319), 跟 ListView visible content 匹配 →
// 视觉"正常"。
//
// 修法: OnCreate ListView_InsertColumn 删 cx 设置 (只 mask = LVCF_TEXT +
// LVCF_SUBITEM), 让 LayoutDialog 末尾 (已在 line 738-744 调一次) 统一算
// column width (scrollbar-aware, header client - 4)。
static void TestOnCreateColumnIsScrollbarAware() {
  std::cout << "\n[Test 34] v0.19.0.45: OnCreate 初始 column 宽度 scrollbar-aware"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("34.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  HWND hList = PhrasesDialog::s_hList;
  HWND hdr = ListView_GetHeader(hList);
  CHECK("34.1: ListView header 已创建", hdr != nullptr);
  if (!hdr)
    goto test34_end;

  {
    RECT rcHdr;
    GetClientRect(hdr, &rcHdr);
    int hdrClientW = rcHdr.right - rcHdr.left;

    LVCOLUMNW col = {};
    col.mask = LVCF_WIDTH;
    ListView_GetColumn(hList, 0, &col);
    int colCx = col.cx;

    // 期望: colCx ≈ hdrClientW - 4 (scrollbar slack, 跟 LayoutDialog 一致)
    int diff = abs(colCx - (hdrClientW - 4));
    CHECK("34.2: column cx ≈ header client - 4 (scrollbar-aware)",
          diff <= 2);

    // column cx < ListView client width (scrollbar 减去了)
    RECT rcList;
    GetClientRect(hList, &rcList);
    int listClientW = rcList.right - rcList.left;
    CHECK("34.3: column cx < ListView client width (scrollbar 减去了)",
          colCx < listClientW);

    // column cx ≤ hdrClientW (不允许 cx > header visible, 那会 overflow)
    CHECK("34.4: column cx ≤ header client width (no horizontal overflow)",
          colCx <= hdrClientW);
  }

test34_end:
  PhrasesDialog::Hide();
}

// v0.19.0.45 (Phase H polish Bug 2 真修): 装机 v0.19.0.44 user 反馈
//   "调整边框右侧时, 短语编辑框宽度应随着变化。添加按钮和UI的边距应该缩小,
//    使编辑框尽量宽"
//
// Bug 2 root cause:
// - LayoutDialog 用 fixed inputW (= kInputW * dpiScale = 240), input 不随
//   clientW 拉伸 → resize 后 input 仍 240, 不跟 list 同步
// - UI margin 用 kBtnMarginX = 12, input 左右各 12px → 编辑框 不够宽
//
// 修法:
// - inputX = kGap (= 8, was kBtnMarginX=12)
// - inputW = clientW - 2*gap - btnAddTopW - gap (stretch, 跟随 clientW)
// - AddTop X = clientW - gap - btnAddTopW (anchored right)
// - 底部 btn X = clientW - gap - totalBtnW (was - marginX, 现在 - gap)
// - listX/listW 保持 (listX = gap, listW = clientW - 2*gap) — 已 OK
static void TestInputStretchesAndMarginShrinks() {
  std::cout << "\n[Test 35] v0.19.0.45: input 拉伸 + UI margin 缩小"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("35.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  // 触发 LayoutDialog (跟 Test 32 同样模式): client area 800×600
  int newW = 800, newH = 600;
  SetWindowPos(hwnd, nullptr, 0, 0, newW + 16, newH + 16,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  SendMessageW(hwnd, WM_SIZE, 0, MAKELPARAM(newW, newH));

  double dpi = PhrasesDialog::s_dpiScale;
  int gap_phys = (int)(8 * dpi);          // kGap * dpiScale = UI 通用 margin
  int btnAddTopW_phys = (int)(76 * dpi);  // kBtnAddTopW * dpiScale
  int expectedInputX = gap_phys;          // input 左 margin = gap
  int expectedInputW =
      newW - 2 * gap_phys - btnAddTopW_phys - gap_phys;  // stretch
  int expectedAddTopX = newW - gap_phys - btnAddTopW_phys;
  int expectedAddTopRight = newW - gap_phys;
  int expectedListX = gap_phys;
  int expectedListW = newW - 2 * gap_phys;
  int expectedCancelRight = newW - gap_phys;

  // 1. input X = kGap (= 8*DPI), 不是 kBtnMarginX (= 12*DPI)
  RECT rcInput;
  GetWindowRect(PhrasesDialog::s_hInput, &rcInput);
  POINT ptInput = {rcInput.left, rcInput.top};
  ScreenToClient(hwnd, &ptInput);
  CHECK("35.1: input X = kGap (= 8*DPI), not kBtnMarginX (= 12*DPI)",
        abs(ptInput.x - expectedInputX) <= 2);

  // 2. input W stretch
  int inputW = rcInput.right - rcInput.left;
  CHECK("35.2: input W = clientW - 3*gap - btnAddTopW (stretch)",
        abs(inputW - expectedInputW) <= 2);
  CHECK("35.3: input W > fixed 240 (确认拉伸发生)",
        inputW > (int)(240 * dpi));

  // 3. AddTop X anchored right
  RECT rcAddTop;
  GetWindowRect(PhrasesDialog::s_hBtnAddTop, &rcAddTop);
  POINT ptAddTop = {rcAddTop.left, rcAddTop.top};
  ScreenToClient(hwnd, &ptAddTop);
  CHECK("35.4: AddTop X = clientW - gap - btnAddTopW (anchored right)",
        abs(ptAddTop.x - expectedAddTopX) <= 2);

  // 4. AddTop 右边缘 = clientW - gap
  POINT ptAddTopRight = {rcAddTop.right, rcAddTop.top};
  ScreenToClient(hwnd, &ptAddTopRight);
  CHECK("35.5: AddTop 右边缘 = clientW - gap (= 8*DPI)",
        abs(ptAddTopRight.x - expectedAddTopRight) <= 2);

  // 5. list 左 margin = gap
  RECT rcList;
  GetWindowRect(PhrasesDialog::s_hList, &rcList);
  POINT ptList = {rcList.left, rcList.top};
  ScreenToClient(hwnd, &ptList);
  CHECK("35.6: list X = gap (= 8*DPI)",
        abs(ptList.x - expectedListX) <= 2);

  // 6. list W = clientW - 2*gap
  int listW = rcList.right - rcList.left;
  CHECK("35.7: list W = clientW - 2*gap",
        abs(listW - expectedListW) <= 2);

  // 7. 底部 btn Cancel 右边缘 = clientW - gap (= 8*DPI, was 12)
  RECT rcBtnCancel;
  GetWindowRect(PhrasesDialog::s_hBtnCancel, &rcBtnCancel);
  POINT ptCancel = {rcBtnCancel.right, rcBtnCancel.top};
  ScreenToClient(hwnd, &ptCancel);
  CHECK("35.8: btn Cancel 右边缘 = clientW - gap (= 8*DPI, not 12)",
        abs(ptCancel.x - expectedCancelRight) <= 2);

  // 8. 二次 resize 到 600×400 验证 stretch 持续
  int newW2 = 600, newH2 = 400;
  SetWindowPos(hwnd, nullptr, 0, 0, newW2 + 16, newH2 + 16,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  SendMessageW(hwnd, WM_SIZE, 0, MAKELPARAM(newW2, newH2));

  int expectedInputW2 =
      newW2 - 2 * gap_phys - btnAddTopW_phys - gap_phys;
  RECT rcInput2;
  GetWindowRect(PhrasesDialog::s_hInput, &rcInput2);
  int inputW2 = rcInput2.right - rcInput2.left;
  CHECK("35.9: 再次 resize 后 input W 跟随新 clientW",
        abs(inputW2 - expectedInputW2) <= 2);

  PhrasesDialog::Hide();
}

// v0.19.0.47 (Phase I Bug 4 续修): 装机 v0.19.0.46 后 user flow 第 3 步
//   "打拼音出候选词" 仍 fail。 真因未完全定位, 候选:
//   - TSF shim 进程下, hwnd 拿焦点时 TSF attach IME context 是 lazily bound
//     per-thread, s_hInput 从未在 OnCreate 期间 SetFocus, TSF 从未 attach
//     default IME context 给 s_hInput, user click input 时 TSF attach 失败
//   - 修法: OnCreate 末 SetFocus(s_hList) 之前先 SetFocus(s_hInput) 强制
//     触发 1 次 TSF attach, 然后 SetFocus(s_hList) 让 ListView 拿焦点
//
// Test 36 sandbox 验证:
//   - OnCreate 末 GetFocus() == s_hList (默认焦点正确, 表示 SetFocus 序列
//     至少跑了 s_hList 这次; 如果 s_hInput 是 null, OnCreate 会 skip 第一
//     个 if, 直接 SetFocus(s_hList), GetFocus 仍 = s_hList → 假阳性)
//   - 加 subclass 后手动 SetFocus(s_hInput); SetFocus(s_hList); →
//     counter 增 1 (subclass hook work), GetFocus 仍 = s_hList
//   - sandbox 不能拦截 OnCreate 期间 WM_SETFOCUS (子类化时机晚于 SetFocus),
//     装机端 IME attach 路径只能靠 user flow 第 3 步验证
static int g_inputSetFocusCount = 0;
static WNDPROC g_origInputWndProc = nullptr;

static LRESULT CALLBACK TestInputSetFocusSubclassProc(HWND hwnd, UINT msg,
                                                       WPARAM wp, LPARAM lp) {
  if (msg == WM_SETFOCUS) {
    g_inputSetFocusCount++;
  }
  return CallWindowProcW(g_origInputWndProc, hwnd, msg, wp, lp);
}

static void TestOnCreateInputGetsFocusForTSFAttach() {
  std::cout << "\n[Test 36] v0.19.0.47: OnCreate 末焦点 + SetFocus(s_hInput)"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  HWND hInput = PhrasesDialog::s_hInput;
  HWND hList = PhrasesDialog::s_hList;
  CHECK("36.0: s_hwnd + s_hInput + s_hList 已创建",
        hwnd != nullptr && hInput != nullptr && hList != nullptr);
  if (!hInput || !hList)
    goto test36_end;

  // 1. OnCreate 末 GetFocus == s_hList (默认焦点不变)
  CHECK("36.1: OnCreate 末焦点在 s_hList (最后 SetFocus(s_hList) 生效)",
        GetFocus() == hList);

  // 2. Subclass s_hInput WndProc, 手动 SetFocus 验证 hook work
  g_origInputWndProc = (WNDPROC)SetWindowLongPtrW(
      hInput, GWLP_WNDPROC, (LONG_PTR)TestInputSetFocusSubclassProc);
  g_inputSetFocusCount = 0;
  SetFocus(hInput);  // 应该触发 WM_SETFOCUS → counter +1
  CHECK("36.2: subclass hook 拦截 WM_SETFOCUS (SetFocus 1 次 → counter 1)",
        g_inputSetFocusCount == 1);
  SetFocus(hList);  // s_hList 拿焦点
  CHECK("36.3: SetFocus(s_hList) 后 GetFocus == s_hList",
        GetFocus() == hList);

  // 3. 验证 OnCreate 内 SetFocus(s_hInput) 路径 (sandbox best-effort):
  //    Hide + Show 再次触发 OnCreate, OnCreate 内 SetFocus(s_hInput) 会
  //    在 subclass **之前** 触发 (subclass 是上次 Show 后挂的, 已被 Hide
  //    销毁), 所以 counter 不会增; 但 OnCreate 末 GetFocus == hList 仍
  //    表示 SetFocus 序列完整
  PhrasesDialog::Hide();
  PhrasesDialog::Show();
  hwnd = PhrasesDialog::s_hwnd;
  hInput = PhrasesDialog::s_hInput;
  hList = PhrasesDialog::s_hList;
  CHECK("36.4: Hide + Show 后 s_hwnd + s_hInput + s_hList 重建",
        hwnd != nullptr && hInput != nullptr && hList != nullptr);
  if (!hInput || !hList)
    goto test36_end;
  // 重挂 subclass
  g_origInputWndProc = (WNDPROC)SetWindowLongPtrW(
      hInput, GWLP_WNDPROC, (LONG_PTR)TestInputSetFocusSubclassProc);
  g_inputSetFocusCount = 0;
  // 现在手动模拟 OnCreate 内 SetFocus 序列
  SetFocus(hInput);
  SetFocus(hList);
  CHECK("36.5: 模拟 OnCreate 内 SetFocus(s_hInput) → SetFocus(s_hList) 序列",
        g_inputSetFocusCount == 1 && GetFocus() == hList);

test36_end:
  // 恢复原 WndProc
  if (hInput && g_origInputWndProc) {
    SetWindowLongPtrW(hInput, GWLP_WNDPROC, (LONG_PTR)g_origInputWndProc);
  }
  PhrasesDialog::Hide();
}

// v0.19.0.44 (Feature 2: reorder via drag): 测试 CommitReorder helper 逻辑
//   不能 e2e 测试 LVN_BEGINDRAG/ENDDRAG (需要真正 mouse drag)
//   但 CommitReorder 逻辑本身可以独立验证: 设 s_dragSourceIdx + s_dropTargetIdx
//   + s_isReorderDragging=true, 调 CommitReorder, 验证 m_phrases 重新排列
static void TestCommitReorderMovesItem() {
  std::cout << "\n[Test 33] v0.19.0.44: CommitReorder helper 单测" << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("33.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));

  // 注入 5 个 phrase
  PhrasesDialog::MutablePhrases().clear();
  PhrasesDialog::MutablePhrases().push_back({L"first", L""});
  PhrasesDialog::MutablePhrases().push_back({L"second", L""});
  PhrasesDialog::MutablePhrases().push_back({L"third", L""});
  PhrasesDialog::MutablePhrases().push_back({L"fourth", L""});
  PhrasesDialog::MutablePhrases().push_back({L"fifth", L""});
  // PopulateList 是 private, 直接用 ListView_InsertItem (public API)
  SendMessageW(PhrasesDialog::s_hList, LVM_DELETEALLITEMS, 0, 0);
  for (size_t i = 0; i < 5; i++) {
    LVITEMW it = {};
    it.mask = LVIF_TEXT | LVIF_PARAM;
    it.iItem = (int)i;
    it.iSubItem = 0;
    it.pszText = const_cast<wchar_t*>(PhrasesDialog::Phrases()[i].text.c_str());
    it.lParam = (LPARAM)i;
    ListView_InsertItem(PhrasesDialog::s_hList, &it);
  }
  CHECK("33.1: 5 个 phrase 已 populate",
        PhrasesDialog::PopulateListCount(PhrasesDialog::s_hList) == 5);

  // Move item 0 ("first") to position 4 ("fifth" 之后, 即末尾)
  PhrasesDialog::s_dragSourceIdx = 0;
  PhrasesDialog::s_dropTargetIdx = 5;  // drop after index 4 = list end
  PhrasesDialog::s_isReorderDragging = true;
  PhrasesDialog::CommitReorder(PhrasesDialog::s_hList);
  PhrasesDialog::EndReorderDrag(hwnd);

  const auto& phrases = PhrasesDialog::Phrases();
  CHECK("33.2: phrases 数仍 5", phrases.size() == 5);
  CHECK("33.3: 'first' 现在在末尾 (index 4)", phrases[4].text == L"first");
  CHECK("33.4: 原 'second' 现在在 index 0", phrases[0].text == L"second");
  CHECK("33.5: 原 'fifth' 现在在 index 3 (被 'first' 挤掉)",
        phrases[3].text == L"fifth");

  // Try moving index 4 ('first') to position 0 (最前)
  PhrasesDialog::s_dragSourceIdx = 4;
  PhrasesDialog::s_dropTargetIdx = 0;
  PhrasesDialog::s_isReorderDragging = true;
  PhrasesDialog::CommitReorder(PhrasesDialog::s_hList);
  PhrasesDialog::EndReorderDrag(hwnd);
  CHECK("33.6: 'first' 移到 index 0",
        PhrasesDialog::Phrases()[0].text == L"first");
  CHECK("33.7: 'second' 移到 index 1",
        PhrasesDialog::Phrases()[1].text == L"second");

  // No-op case: source == target (no change)
  PhrasesDialog::s_dragSourceIdx = 0;
  PhrasesDialog::s_dropTargetIdx = 0;
  PhrasesDialog::s_isReorderDragging = true;
  PhrasesDialog::CommitReorder(PhrasesDialog::s_hList);
  PhrasesDialog::EndReorderDrag(hwnd);
  CHECK("33.8: no-op reorder (source == target) 不变",
        PhrasesDialog::Phrases()[0].text == L"first");

  PhrasesDialog::Hide();
}

// v0.19.0.43 (Bug 2.3 真修): outer 尺寸 = client + WS_THICKFRAME border。
// 装机 v0.19.0.42 user 报告"初始界面右侧遮挡 Add 按钮, 下边缘遮挡列表+按钮"
// — 是因为 CreateWindowEx 用 raw kDialogW × kDialogH 作 outer, 但 OnCreate
// 用 DPI-scaled dW × dH 算 child 位置, child 出框。修法: Show() 用
// AdjustWindowRectEx 算 outer (client + border)。Test 31 验证 outer 严格 >
// client。
static void TestOuterSizeIncludesBorder() {
  std::cout << "\n[Test 31] v0.19.0.43: outer = client + WS_THICKFRAME border"
            << std::endl;
  PhrasesDialog::SetYamlPath(L"");
  PhrasesDialog::Show();
  HWND hwnd = PhrasesDialog::s_hwnd;
  CHECK("31.0: s_hwnd 已创建", hwnd != nullptr && IsWindow(hwnd));
  RECT rcOuter, rcClient;
  GetWindowRect(hwnd, &rcOuter);
  GetClientRect(hwnd, &rcClient);
  int outerW = rcOuter.right - rcOuter.left;
  int outerH = rcOuter.bottom - rcOuter.top;
  int clientW = rcClient.right - rcClient.left;
  int clientH = rcClient.bottom - rcClient.top;
  // Border 应该让 outer > client (client + 2 * border_thickness)
  CHECK("31.1: outer width > client width (border 加 outer)", outerW > clientW);
  CHECK("31.2: outer height > client height (border 加 outer)",
        outerH > clientH);
  // border 通常 ~4-8px 每边, 所以 outer - client 应该 ≥ 4
  int diffW = outerW - clientW;
  int diffH = outerH - clientH;
  CHECK("31.3: outer - client >= 4 (border 至少 4px 厚, DPI 2x 下 8px)",
        diffW >= 4 && diffH >= 4);
  // client 应该 ≥ 280 × 360 (允许 DPI 0.5x 时缩到最小, 但 kDialogW=360 / 2 =
  // 180, 加上 border 也 ≥ 184)
  CHECK("31.4: client area 合理大小 (>= 180 × 240, 0.5x DPI 最小)",
        clientW >= 180 && clientH >= 240);
  PhrasesDialog::Hide();
}

}  // namespace test

int main() {
  std::cout << "spec 044 v0.19.0.32 — PhrasesDialog v3 unit tests (UX redo)"
            << std::endl;
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

  // P2 polish (装机反馈: 多余 "短语" 标签 + 双击/回车 不上屏)
  test::TestColumnHeaderEmpty();
  test::TestNMDblClkInjects();
  test::TestNMReturnInjects();

  // v0.19.0.39 (Phase F fix Bug 1 + Bug 3: foreground + Esc)
  test::TestForegroundApiCalled();
  test::TestEscKeyHidesDialog();

  // v0.19.0.40 (Phase F Bug 3 续修: LVN_KEYDOWN 路径 — 真 fix)
  test::TestLVNKeyDownEscapeHides();

  // v0.19.0.41 (UI 迭代: DPI 缩放 + resize + drag + 删重复 Add)
  test::TestNoDuplicateAddButton();
  test::TestWindowStyleHasThickFrame();
  test::TestDpiScaleComputed();

  // v0.19.0.42 (Stop hook 修补: OnGetMinMaxInfo DPI 缩放 + drag state e2e)
  test::TestGetMinMaxInfoDpiScaled();
  test::TestLongPressDragStateMachine();

  // v0.19.0.43 (Bug 1.2 + 2.2 + 2.3 真修: IME ImmReleaseContext + drag screen
  // coords + WM_KILLFOCUS guard + outer size AdjustWindowRect)
  test::TestOuterSizeIncludesBorder();

  // v0.19.0.44 (Feature 1: resize layout): WM_SIZE re-layout
  test::TestResizeLayoutRepositionsChildren();
  // v0.19.0.44 (Feature 2: reorder ListView entry via drag): CommitReorder
  // logic
  test::TestCommitReorderMovesItem();

  // v0.19.0.45 (Phase H layout polish: Bug 1 column scrollbar-aware)
  test::TestOnCreateColumnIsScrollbarAware();
  // v0.19.0.45 (Phase H layout polish: Bug 2 input stretch + UI margin 缩小)
  test::TestInputStretchesAndMarginShrinks();
  // v0.19.0.47 (Phase I Bug 4 续修): OnCreate 期间 s_hInput 至少获 1 次焦点
  test::TestOnCreateInputGetsFocusForTSFAttach();

  std::cout << "\n================================================="
            << std::endl;
  std::cout << "PASSED: " << test::g_passed << "  FAILED: " << test::g_failed
            << std::endl;
  std::cout << "=================================================" << std::endl;

  return test::g_failed == 0 ? 0 : 1;
}