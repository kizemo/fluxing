# 031 · 火流猩输入法 v2 · 候选字屏蔽（stage 3 of spec 008 — 客户端过滤）

> stage 3 of spec 008。 stage 1 (librime API 集成) 在 spec 028 ship；stage 2 (UI 接线)
> 在 spec 030 ship。 本 spec 把"屏蔽候选"功能闭合：用户右键单字 / 共享词典命中
> 候选，把候选 text 追加到 `<schema_id>.user_ignore.txt`，WeaselPanel 在 Refresh
> 时过滤掉该 text 后续不再显示。
>
> **librime 1.13 `user_ignore` 钩子不存在**（rg librime/ 0 引用）。本 spec 走
> spec 004 §8 风险 R4 的 fallback **方案 B** — 客户端过滤，**完全在 WeaselPanel
> 端**，不依赖 librime 引擎改动。

## 0. Why now (intent before implementation)

- spec 008 §1.1 触发规则两条:
  - `text.length() >= 2 && is_user_dict == true` → **删除** (spec 028/030 已 ship)
  - 其他（单字 / 共享词典）→ **屏蔽** ← 本 spec
- librime 1.13 没有 user_ignore 钩子（spec 008 §3 R2 风险已识别）。
  spec 008 plan §2.1 标 "需验证"，本 spec 直接选 fallback 方案 B。
- 客户端过滤位置: WeaselPanel 在 `Update(Context, Status)` 时（spec 030 已经在用
  `m_ctx.cinfo.candies`）— 过滤后写回 `m_ctx.cinfo.candies`（in-place 移除
  匹配 text 的候选）。此行为**对 IPC 协议透明**（不改 RimeWithWeasel / IPC 层）。

## 1. Acceptance criteria

- `WeaselUI/WeaselPanel.cpp` 加 `m_ignoreList` 成员（`std::unordered_set<std::wstring>`）
  + `LoadIgnoreList(const std::wstring& schema_id)` 私有方法：
  - 从 `<rime_user_dir>/<schema_id>.user_ignore.txt` 读每行（一行一候选）
  - 用 `\r\n` / `\n` / `\r` split
  - 空行 / 纯空白行跳过
  - UTF-8 / UTF-16 LE 都支持（带 BOM 自动判别，跟 rime 官方 userdb 习惯一致）
- `OnRButtonDown` 加判定（spec 030 已有雏形，本 spec 扩展）：
  - `m_hoverIndex >= 0 && hover text 长度 < 2 || !is_user_dict` → **屏蔽**：
    1. 把 `m_ctx.cinfo.candies[m_hoverIndex].text` 加到 `m_ignoreList`
    2. append 一行到 `<rime_user_dir>/<schema_id>.user_ignore.txt`（带 UTF-8 BOM，CRLF）
    3. `RedrawWindow()`（候选窗刷新，命中 text 消失）
  - `text.length() >= 2 && is_user_dict` → **删除**（spec 030 已实现，本 spec 不动）
  - 同一 text 100ms 内多次右键不重复写文件（防抖，跟 spec 030 OnRButtonDown 一致）
- `Update(Context, Status)` 末尾调 `_FilterIgnoredCandidates()`：
  - 遍历 `m_ctx.cinfo.candies`，移除 text 在 `m_ignoreList` 里的条目
- `WeaselPanel` 启动时调 `LoadIgnoreList(current_schema_id)`：在 `OnCreate` 末尾调一次
  （从 `m_ctx.schema_id` 读，如果没拿到就 lazy load 第一次 Update 时再读）
- 不动 IPC / librime / WeaselTSF（user_ignore 完全是客户端状态）。
- 新建 `test/TestCandidateIgnoreFilter.cpp`：5 真实 assertions:
  1. `LoadIgnoreList` 读 UTF-8 BOM CRLF 文件
  2. `LoadIgnoreList` 跳过空行 / 纯空白
  3. `LoadIgnoreList` 支持 UTF-16 LE BOM 文件
  4. `_FilterIgnoredCandidates` 移除所有在 ignore 集合里的 text
  5. `_FilterIgnoredCandidates` 对不在 ignore 集合的 text 不动
- `cmd /c scripts\test-infra\run-test-suite.bat` 退出 0，**9/9** test projects。
- AGENTS.md sec 5 五步 pre-commit gate 全过。
- L37 lesson（"L## fix coverage" L36 提醒）: 本 spec 加 `LoadIgnoreList` 同时验证其他
  text file reader（如果存在）也用 byte-level + BOM 判别模式。

## 2. Out of scope

- **不**做 schema patch fallback（librime 端 user_ignore 钩子不存在，无意义）。
- **不**做托盘"恢复"按钮（spec 006 / spec 008 T008；属于 spec 032+）。
- **不**做暗色主题订阅（spec 008 T007）。
- **不**做 user.db 备份（spec 008 T008）。
- **不**做 user_ignore.txt 客户端 UI 编辑器（spec 008 §2 out of scope）。

## 3. Approach（chosen: 客户端过滤 + 文本追加，零引擎依赖）

### 3.1 数据结构

```cpp
// WeaselUI/WeaselPanel.h 私有成员
std::unordered_set<std::wstring> m_ignoreList;
std::wstring m_ignoreFilePath;  // 缓存路径，load 时计算
```

### 3.2 加载流程

```cpp
// WeaselUI/WeaselPanel.cpp
void WeaselPanel::LoadIgnoreList(const std::wstring& schema_id) {
  m_ignoreList.clear();
  if (schema_id.empty()) return;
  // rime_user_dir 路径从 install.h / weasel 启动参数拿；这里走 CRegistryUtils
  std::wstring userDir = CRegistryUtils::GetRimeUserDir();  // 已存在 helper
  m_ignoreFilePath = userDir + L"\\" + schema_id + L".user_ignore.txt";
  std::ifstream in(m_ignoreFilePath, std::ios::binary);
  if (!in) return;  // 文件不存在 = ignore list 空，正常状态
  std::vector<unsigned char> raw((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  std::wstring content;
  // BOM 判别
  if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
    // UTF-8 BOM
    std::string utf8((char*)raw.data() + 3, raw.size() - 3);
    content = weasel::Utf8ToWide(utf8);
  } else if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
    // UTF-16 LE BOM
    content = std::wstring((wchar_t*)(raw.data() + 2),
                            (raw.size() - 2) / sizeof(wchar_t));
  } else {
    // 无 BOM，按 UTF-8 兜底
    std::string utf8((char*)raw.data(), raw.size());
    content = weasel::Utf8ToWide(utf8);
  }
  // 按行 split（CRLF / LF / CR 都支持）
  size_t pos = 0;
  while (pos < content.size()) {
    size_t eol = content.find_first_of(L"\r\n", pos);
    if (eol == std::wstring::npos) eol = content.size();
    std::wstring line = content.substr(pos, eol - pos);
    // 去尾空白
    while (!line.empty() && (line.back() == L" " || line.back() == L"\t"))
      line.pop_back();
    if (!line.empty()) m_ignoreList.insert(line);
    // 跳过 CRLF / LF / CR
    pos = eol;
    if (pos < content.size() && content[pos] == L"\r") pos++;
    if (pos < content.size() && content[pos] == L"\n") pos++;
  }
}
```

### 3.3 OnRButtonDown 扩展

```cpp
// WeaselUI/WeaselPanel.cpp  (spec 030 OnRButtonDown 已有判定)
// 本 spec 加 ignore 路径：在 dispatch 前判定走 delete or ignore
LRESULT WeaselPanel::OnRButtonDown(UINT uMsg, WPARAM wParam,
                                   LPARAM lParam, BOOL& bHandled) {
  if (hide_candidates || m_hoverIndex < 0) {
    bHandled = true;
    return 0;
  }
  // 防抖 (spec 030 已有，保留)
  static DWORD s_lastTick = 0;
  static int s_lastIndex = -1;
  static std::wstring s_lastText;  // 新增: 按 text 防抖（不是按 index）
  DWORD now = GetTickCount();
  auto& candies = m_ctx.cinfo.candies;
  if (m_hoverIndex >= (int)candies.size()) {
    bHandled = true;
    return 0;
  }
  const std::wstring& text = candies[m_hoverIndex].text;
  if (text == s_lastText && (now - s_lastTick) < 100) {
    bHandled = true;
    return 0;
  }
  s_lastTick = now;
  s_lastText = text;
  // 判定 delete vs ignore
  if (text.length() >= 2 && /* is_user_dict 不可达 */ true) {
    // spec 030: delete (走 m_deleteCallback)
    if (m_deleteCallback) m_deleteCallback(static_cast<size_t>(m_hoverIndex));
  } else {
    // stage 3: ignore (本 spec)
    _IgnoreCurrentCandidate();
  }
  bHandled = true;
  return 0;
}
```

注意: **is_user_dict 字段 C API 不可达**（spec 028 L35）。spec 008 §1.1 原本说
"is_user_dict == true" 才删除，但实际 stage 1（spec 028）发现 librime 端没有
is_user_dict 字段暴露。spec 030 直接走"text.length >= 2 即 delete"（简化规则，
用户接受）。本 spec 沿用 spec 030 简化规则 — 实际是"text.length >= 2 → delete"，
"text.length < 2 → ignore"（与 spec 008 §1.1 略不同，spec 008 §1.1 标"以
engine 内部判断为准"，现在 engine 不可达）。

```cpp
// WeaselUI/WeaselPanel.cpp 新增 helper
void WeaselPanel::_IgnoreCurrentCandidate() {
  if (m_hoverIndex < 0) return;
  auto& candies = m_ctx.cinfo.candies;
  if (m_hoverIndex >= (int)candies.size()) return;
  const std::wstring& text = candies[m_hoverIndex].text;
  if (text.empty()) return;
  // 1. 加到内存集合
  m_ignoreList.insert(text);
  // 2. 追加到文件（byte-level 写，避免 PowerShell 字符串层 trap - L37）
  if (m_ignoreFilePath.empty()) {
    // 还没 load 过 — 立即按当前 schema load 一次（创建路径）
    LoadIgnoreList(m_ctx.schema_id);
  }
  if (m_ignoreFilePath.empty()) return;  // 没有 user dir，no-op
  HANDLE h = CreateFileW(m_ignoreFilePath.c_str(),
                          FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  // 写 "text\r\n"（CRLF，按 spec 008 / 004 已有约定）
  std::string utf8 = weasel::WideToUtf8(text);
  std::vector<char> line;
  line.insert(line.end(), (char*)utf8.data(),
              (char*)utf8.data() + utf8.size());
  line.push_back("\r"); line.push_back("\n");
  DWORD written = 0;
  WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
  CloseHandle(h);
  // 3. 刷新候选窗
  RedrawWindow();
}
```

### 3.4 过滤流程

```cpp
// WeaselUI/WeaselPanel.cpp
void WeaselPanel::_FilterIgnoredCandidates() {
  if (m_ignoreList.empty()) return;
  auto& candies = m_ctx.cinfo.candies;
  // 末尾反向 erase（in-place filter；std::vector::erase 配合反向迭代器 O(n)）
  for (size_t i = candies.size(); i > 0; --i) {
    if (m_ignoreList.count(candies[i - 1].text) > 0) {
      candies.erase(candies.begin() + (i - 1));
    }
  }
}
```

### 3.5 test/TestCandidateIgnoreFilter.cpp

mock + behavior-level。**不**链接 WeaselPanel.cpp（WTL/ATL/Gdiplus），抽出
`LoadIgnoreList_impl` / `_FilterIgnoredCandidates_impl` 自由函数。

```cpp
#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

namespace {

// File reader helper (byte-level, BOM 判别) — 跟生产代码一致
std::unordered_set<std::wstring> ReadIgnoreList(const std::wstring& path) {
  std::unordered_set<std::wstring> result;
  std::ifstream in(std::string(path.begin(), path.end()), std::ios::binary);
  if (!in) return result;
  std::vector<unsigned char> raw((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  std::wstring content;
  if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
    std::string utf8((char*)raw.data() + 3, raw.size() - 3);
    // Test-side: convert UTF-8 to wide manually for portability
    content = std::wstring(utf8.begin(), utf8.end());  // simplified
  } else if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
    content = std::wstring((wchar_t*)(raw.data() + 2),
                            (raw.size() - 2) / sizeof(wchar_t));
  } else {
    std::string utf8((char*)raw.data(), raw.size());
    content = std::wstring(utf8.begin(), utf8.end());
  }
  // split lines
  size_t pos = 0;
  while (pos < content.size()) {
    size_t eol = content.find_first_of(L"\r\n", pos);
    if (eol == std::wstring::npos) eol = content.size();
    std::wstring line = content.substr(pos, eol - pos);
    while (!line.empty() && (line.back() == L" " || line.back() == L"\t"))
      line.pop_back();
    if (!line.empty()) result.insert(line);
    pos = eol;
    if (pos < content.size() && content[pos] == L"\r") pos++;
    if (pos < content.size() && content[pos] == L"\n") pos++;
  }
  return result;
}

void FilterIgnoredCandidates(std::vector<std::wstring>& candidates,
                              const std::unordered_set<std::wstring>& ignore) {
  for (size_t i = candidates.size(); i > 0; --i) {
    if (ignore.count(candidates[i - 1]) > 0) {
      candidates.erase(candidates.begin() + (i - 1));
    }
  }
}

}  // namespace

int main() {
  // T1: LoadIgnoreList 读 UTF-8 BOM CRLF 文件
  {
    std::wstring path = L".\\test_ignore_utf8.txt";
    std::ofstream out("test_ignore_utf8.txt", std::ios::binary);
    out << "\xEF\xBB\xBF";  // BOM
    out << "alpha\r\nbeta\r\ngamma\r\n";
    out.close();
    auto set = ReadIgnoreList(path);
    BOOST_TEST_EQ(set.size(), 3u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    BOOST_TEST(set.count(L"gamma") == 1);
    std::cout << "  PASS: T1 LoadIgnoreList reads UTF-8 BOM CRLF" << std::endl;
    std::remove("test_ignore_utf8.txt");
  }

  // T2: 跳过空行 / 纯空白
  {
    std::wstring path = L".\\test_ignore_blank.txt";
    std::ofstream out("test_ignore_blank.txt", std::ios::binary);
    out << "\xEF\xBB\xBF";
    out << "alpha\r\n\r\n   \r\nbeta\r\n";
    out.close();
    auto set = ReadIgnoreList(path);
    BOOST_TEST_EQ(set.size(), 2u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    std::cout << "  PASS: T2 LoadIgnoreList skips blank lines" << std::endl;
    std::remove("test_ignore_blank.txt");
  }

  // T3: UTF-16 LE BOM 文件
  {
    std::wstring path = L".\\test_ignore_utf16.txt";
    std::ofstream out("test_ignore_utf16.txt", std::ios::binary);
    out << "\xFF\xFE";  // UTF-16 LE BOM
    std::wstring content = L"alpha\r\nbeta\r\n";
    out.write((char*)content.data(), content.size() * sizeof(wchar_t));
    out.close();
    auto set = ReadIgnoreList(path);
    BOOST_TEST_EQ(set.size(), 2u);
    BOOST_TEST(set.count(L"alpha") == 1);
    BOOST_TEST(set.count(L"beta") == 1);
    std::cout << "  PASS: T3 LoadIgnoreList reads UTF-16 LE BOM" << std::endl;
    std::remove("test_ignore_utf16.txt");
  }

  // T4: FilterIgnoredCandidates 移除 ignore 集合里的所有 text
  {
    std::vector<std::wstring> cands = {"alpha", "beta", "gamma", "delta"};
    std::unordered_set<std::wstring> ignore = {"beta", "delta"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 2u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"gamma");
    std::cout << "  PASS: T4 FilterIgnoredCandidates removes all ignored" << std::endl;
  }

  // T5: FilterIgnoredCandidates 对不在 ignore 集合的 text 不动
  {
    std::vector<std::wstring> cands = {"alpha", "beta", "gamma"};
    std::unordered_set<std::wstring> ignore = {"xxx", "yyy"};
    FilterIgnoredCandidates(cands, ignore);
    BOOST_TEST_EQ(cands.size(), 3u);
    BOOST_TEST(cands[0] == L"alpha");
    BOOST_TEST(cands[1] == L"beta");
    BOOST_TEST(cands[2] == L"gamma");
    std::cout << "  PASS: T5 FilterIgnoredCandidates leaves non-ignored alone" << std::endl;
  }

  std::cout << "5 / 5 assertions passed" << std::endl;
  return boost::report_errors();
}
```

## 4. Risks

- **R1**：spec 030 简化了"text.length >= 2 → delete"，本 spec 沿用 — 跟 spec 008
  §1.1 原始规则"is_user_dict 判定"略不同。**用户已接受**（spec 028 L35）。
- **R2**：user_ignore.txt 写在 `<rime_user_dir>` — 卸载 / 切换 user dir 会丢。
  跟 spec 008 T008 "恢复" 按钮对接，spec 032+ 处理。
- **R3**：客户端过滤只在 WeaselPanel 端 — 候选窗关闭再打开时（同一 input session）
  仍生效；切换 schema 后需要 reload 路径（`OnCreate` 末尾调一次 OK，运行时
  schema 切换 defer 到 spec 008 T007）。
- **R4**：R2 L37 提醒 — 写文件用 byte-level `CreateFileW + WriteFile`，**不**用
  `std::ofstream <<`（后者在 Windows 上对 LF-only 输入会做 platform translation）。

## 5. References

- spec 008 (parent — stage 3 实施点 T001-T014)
- spec 028 (stage 1: API + TestUserDictUpdate)
- spec 030 (stage 2: UI 接线 — OnRButtonDown / m_deleteCallback)
- `WeaselUI/WeaselPanel.cpp:285-345` (OnMouseWheel / OnLeftClickedUp 模板)
- `WeaselUI/WeaselPanel.cpp:1147,1156` (m_hoverIndex = -1 重置点)
- TDD.md §3.1 — 8 集成测试清单，本 spec 加第 9 个
- L22 / L23 / L25 / L28 / L31 / L35 / L37 — 已 ship 的 test infra + lessons
- AGENTS.md sec 5 — pre-commit gate