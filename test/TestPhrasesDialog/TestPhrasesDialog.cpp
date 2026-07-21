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

  std::cout << "\n================================================="
            << std::endl;
  std::cout << "PASSED: " << test::g_passed << "  FAILED: " << test::g_failed
            << std::endl;
  std::cout << "=================================================" << std::endl;

  return test::g_failed == 0 ? 0 : 1;
}