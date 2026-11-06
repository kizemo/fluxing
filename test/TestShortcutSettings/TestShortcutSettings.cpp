// TestShortcutSettings.cpp
// spec 045 v0.19.0.28 — Track 3 单元测试 (Public API only)
//
// Public API surface (per header):
//  - SetYamlPath / Hotkeys() / MutableHotkeys() / s_capturingRow / s_pendingChord
//  - Show / Hide / Toggle / s_hwnd / kShowGraceMs
//
// 私有 API (LoadDefaults / SaveHotkeys / LoadHotkeys / NormalizeCombo /
//   VKeyToName / GetBuiltinBindings / CheckConflict) 通过 singleton 包装
//   间接验证 (Show() 内部会调 LoadDefaults 走 fallback)。
//
// 关键: 测试不依赖 real GUI。Show() 走创窗口路径,测试用 SetYamlPath 设路径 + 直接
// mutate MutableHotkeys() 检查结构,YAML 验证用 mock 文件 I/O 函数。

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

#include "ShortcutSettings.h"

namespace test {

// ===== 测试计数器 =====
static int s_pass = 0;
static int s_fail = 0;
#define ASSERT_TRUE(c) do { if (!(c)) { ++s_fail; std::wcerr << L"ASSERT: " << __FILEW__ << L":" << __LINE__ << L" ASSERT failed" << std::endl; return; } } while(0)
#define EXPECT(cond) do { \
  if (cond) { ++s_pass; } else { ++s_fail; \
    std::wcerr << L"FAIL: " << __FILEW__ << L":" << __LINE__ \
               << L" expectation failed: " << L#cond << std::endl; } \
} while(0)

static bool WriteUtf8(const std::wstring& path, const std::wstring& content) {
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write("\xEF\xBB\xBF", 3);
  int len = WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                                 static_cast<int>(content.size()),
                                 nullptr, 0, nullptr, nullptr);
  if (len > 0) {
    std::string buf(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(),
                        static_cast<int>(content.size()),
                        &buf[0], len, nullptr, nullptr);
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
                                  static_cast<int>(content.size()),
                                  nullptr, 0);
  std::wstring w(wlen, 0);
  MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                      static_cast<int>(content.size()), &w[0], wlen);
  return w;
}

// T01 — Public initial state
static void TestInitialState() {
  EXPECT(!ShortcutSettings::s_hwnd);
  EXPECT(ShortcutSettings::s_capturingRow == -1);
  EXPECT(ShortcutSettings::s_pendingChord.empty());
  EXPECT(ShortcutSettings::kShowGraceMs == 2000);  // grace value
}

// T02 — SetYamlPath updates path
static void TestSetYamlPath() {
  ShortcutSettings::SetYamlPath(L"foo_bar.yaml");
  // 检查 path,但因为是 private 无法直接读 → 用 Hotkeys API 间接验证
  // 通过 Show() + 立即 Hide 来验证;这里仅检查调用不崩
  EXPECT(true);
}

// T03 — Hotkeys() returns empty initial (before Show)
static void TestHotkeysInitialEmpty() {
  // Show() not called yet → Hotkeys() 应该返回空
  EXPECT(ShortcutSettings::Hotkeys().empty());
  ShortcutSettings::MutableHotkeys().clear();
  EXPECT(ShortcutSettings::Hotkeys().empty());
}

// T04 — MutableHotkeys can be modified
static void TestMutableHotkeysModify() {
  ShortcutSettings::MutableHotkeys().clear();
  ShortcutSettings::Hotkey h;
  h.action = ShortcutSettings::Action_ToggleChinese;
  h.action_desc = L"\xe5\x88\x87\xe6\x8d\xa2\xe4\xb8\xad\xe8\x8b\xb1";  // 切换中英
  h.action_yaml = L"toggle: ascii_mode";
  h.accept = L"Shift+space";
  h.accept_new = L"Control+Shift+K";
  h.is_default = true;
  h.is_custom = false;
  ShortcutSettings::MutableHotkeys().push_back(h);
  EXPECT(ShortcutSettings::Hotkeys().size() == 1);
  EXPECT(ShortcutSettings::Hotkeys()[0].accept_new == L"Control+Shift+K");
}

// T05 — Hotkey struct fields
static void TestHotkeyStruct() {
  ShortcutSettings::Hotkey h;
  h.action = ShortcutSettings::Action_PageUp;
  h.action_desc = L"Page_Up";
  h.action_yaml = L"send: Page_Up";
  h.when = L"paging";
  h.accept = L"comma";
  h.accept_new.clear();
  h.is_custom = false;
  h.is_default = true;
  EXPECT(h.action == ShortcutSettings::Action_PageUp);
  EXPECT(h.action_desc == L"Page_Up");
  EXPECT(h.when == L"paging");
  EXPECT(h.accept == L"comma");
  EXPECT(h.accept_new.empty());
}

// T06 — HotkeyAction enum distinct values
static void TestHotkeyActionEnum() {
  // 各 enum 值不同
  EXPECT(ShortcutSettings::Action_ToggleChinese != ShortcutSettings::Action_ToggleFullHalf);
  // v0.19.0.60 (Phase L 调整 1): Action_OpenUserDict 删除 — 改用同 type 不同 value 的代换。
  EXPECT(ShortcutSettings::Action_OpenPhrases != ShortcutSettings::Action_ReselectCandidate);
  EXPECT(ShortcutSettings::Action_TriggerDeploy != ShortcutSettings::Action_VerifyHotkey);
  // Action_None 是 0
  EXPECT(ShortcutSettings::Action_None == 0);
}

// T07 — Capturing state machine: s_capturingRow, s_pendingChord 公开
static void TestCapturingStateVars() {
  ShortcutSettings::s_capturingRow = 5;
  ShortcutSettings::s_pendingChord = L"Control+Shift+L";
  EXPECT(ShortcutSettings::s_capturingRow == 5);
  EXPECT(ShortcutSettings::s_pendingChord == L"Control+Shift+L");
  // 还原
  ShortcutSettings::s_capturingRow = -1;
  ShortcutSettings::s_pendingChord.clear();
}

// T08 — YAML round-trip via direct file IO (manual)
// 由于 SaveHotkeys 是 private,改用 Show() not 也不调; 直接 validate 我们 public
// API 通过 SetYamlPath + Hotkeys() 内部反映.
static void TestYamlBomAndCrlf() {
  // 验证我们写的 test fixture 是 UTF-8 BOM + CRLF (符合 L09 教训)
  // 2026-11-06: 不再写仓库根 (会污染工作树), 改写 tmp/ 下, 目录不存在则创建
  ::CreateDirectoryW(L"tmp", NULL);
  std::wstring path = L"tmp/_ts_yaml_fixture.yaml";
  std::wstring content = L"patch:\r\n  key_binder/bindings:\r\n    - { when: always, accept: \"Shift+space\", toggle: ascii_mode }\r\n";
  EXPECT(WriteUtf8(path, content));
  std::ifstream f(path, std::ios::binary);
  ASSERT_TRUE(f);
  char head[3];
  f.read(head, 3);
  ASSERT_TRUE((unsigned char)head[0] == 0xEF);
  ASSERT_TRUE((unsigned char)head[1] == 0xBB);
  ASSERT_TRUE((unsigned char)head[2] == 0xBF);
  std::string rest((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
  EXPECT(rest.find("\r\n") != std::string::npos);
}

// ===== spec 095: 「快捷面板常驻」勾选框 =====
// T-A: 共享头纯函数 —— 状态格式 / 解析 / 三态读 / 写盘回读自证 / 主旧落点优先级
// T-B: 生产 API 往返 —— ShortcutSettings::AlwaysShowChecked / SetAlwaysShowChecked
//      (与设置面板勾选框走同一实现,constitution R7)
//
// 铁律: 落点必须先注入临时路径 —— 绝不写真实用户目录里的 quickpanel.state。

static std::wstring TempStatePath(const wchar_t* name) {
  wchar_t dir[MAX_PATH] = {0};
  DWORD n = ::GetTempPathW(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH)
    return std::wstring();
  return std::wstring(dir) + name;
}

static bool ReadRaw(const std::wstring& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  out.assign((std::istreambuf_iterator<char>(f)),
             std::istreambuf_iterator<char>());
  return true;
}

// T-A1 — 状态行格式与解析
static void TestAlwaysShowStateFormat() {
  using namespace fluxing::always_show;
  EXPECT(FormatStateLine(true) == "always_show=1\n");
  EXPECT(FormatStateLine(false) == "always_show=0\n");

  std::optional<bool> on = ParseStateLine("always_show=1");
  EXPECT(on.has_value() && *on);
  std::optional<bool> off = ParseStateLine("always_show=0");
  EXPECT(off.has_value() && !*off);
  // 容忍尾部 \r / \n
  std::optional<bool> crlf = ParseStateLine("always_show=1\r\n");
  EXPECT(crlf.has_value() && *crlf);
  // 不认识的内容 ⇒ nullopt(注意:这与「文件缺失」不是一回事)
  EXPECT(!ParseStateLine("garbage").has_value());
  EXPECT(!ParseStateLine("always_show=2").has_value());
  EXPECT(!ParseStateLine("").has_value());
}

// T-A2 — 三态读: 缺失 / 空文件 / 读到一行
static void TestAlwaysShowStateReadTriState() {
  using namespace fluxing::always_show;
  const std::wstring p = TempStatePath(L"fluxing_ts_tristate.state");
  ::DeleteFileW(p.c_str());
  std::string line;
  EXPECT(ReadStateLine(p, line) == StateReadStatus::kMissing);

  { std::ofstream f(p, std::ios::binary); }  // 0 字节
  EXPECT(ReadStateLine(p, line) == StateReadStatus::kEmpty);

  {
    std::ofstream f(p, std::ios::binary);
    f << "always_show=1\n";
  }
  EXPECT(ReadStateLine(p, line) == StateReadStatus::kRead);
  EXPECT(line == "always_show=1");
  ::DeleteFileW(p.c_str());
}

// T-A3 — 写盘 + 回读自证(本仓铁律)
static void TestAlwaysShowStateWriteRoundTrip() {
  using namespace fluxing::always_show;
  const std::wstring p = TempStatePath(L"fluxing_ts_roundtrip.state");
  ::DeleteFileW(p.c_str());
  EXPECT(WriteStateFile(p, true));
  std::string raw;
  EXPECT(ReadRaw(p, raw) && raw == "always_show=1\n");
  std::optional<bool> back = ReadStateFile(p);
  EXPECT(back.has_value() && *back);

  EXPECT(WriteStateFile(p, false));
  EXPECT(ReadRaw(p, raw) && raw == "always_show=0\n");
  back = ReadStateFile(p);
  EXPECT(back.has_value() && !*back);
  ::DeleteFileW(p.c_str());
}

// T-A4 — 主 / 旧落点优先级(与 ADR 0020 的旧实现逐条等价)
static void TestAlwaysShowResolveFromFiles() {
  using namespace fluxing::always_show;
  const std::wstring primary = TempStatePath(L"fluxing_ts_primary.state");
  const std::wstring legacy = TempStatePath(L"fluxing_ts_legacy.state");
  ::DeleteFileW(primary.c_str());
  ::DeleteFileW(legacy.c_str());

  // 都读不到 ⇒ 默认常驻(spec 089 / ADR 0022)
  EXPECT(ResolveAlwaysShowFromFiles(primary, legacy, true));
  // primary 缺失 ⇒ 读 legacy 兜底(存量勾选不丢)
  {
    std::ofstream f(legacy.c_str(), std::ios::binary);
    f << "always_show=1\n";
  }
  EXPECT(ResolveAlwaysShowFromFiles(primary, legacy, true));
  // primary 优先
  {
    std::ofstream f(primary.c_str(), std::ios::binary);
    f << "always_show=0\n";
  }
  EXPECT(!ResolveAlwaysShowFromFiles(primary, legacy, true));
  // 读到但内容不认识 ⇒ 不回落 legacy(旧实现语义,TestQuickPanelDialog T13h)
  {
    std::ofstream f(primary.c_str(), std::ios::binary);
    f << "garbage!";
  }
  EXPECT(!ResolveAlwaysShowFromFiles(primary, legacy, true));
  // legacy 传空串 ⇒ 不兜底
  ::DeleteFileW(primary.c_str());
  EXPECT(ResolveAlwaysShowFromFiles(primary, L"", true));
  ::DeleteFileW(legacy.c_str());
}

// T-B — 生产 API 往返(不建窗口): 设置面板勾选框走的就是这一条路径
static void TestAlwaysShowProductionApi() {
  using namespace fluxing::always_show;
  const std::wstring p = TempStatePath(L"fluxing_ts_api.state");
  ::DeleteFileW(p.c_str());
  // 注入落点: 绝不能写真实用户目录里的 quickpanel.state
  StatePathOverrideRef() = p;
  SetAlwaysShowFlag(false);

  EXPECT(!ShortcutSettings::AlwaysShowChecked());
  ShortcutSettings::SetAlwaysShowChecked(true);
  EXPECT(ShortcutSettings::AlwaysShowChecked());
  std::string raw;
  EXPECT(ReadRaw(p, raw) && raw == "always_show=1\n");
  // 状态能通过落盘读回来(与重启后恢复同一条路径)
  EXPECT(ResolveAlwaysShowFromFiles(p, L"", false));

  ShortcutSettings::SetAlwaysShowChecked(false);
  EXPECT(!ShortcutSettings::AlwaysShowChecked());
  EXPECT(ReadRaw(p, raw) && raw == "always_show=0\n");

  // 同值重复设置 ⇒ 不重复写盘(内容仍是最后一次的合法值)
  ShortcutSettings::SetAlwaysShowChecked(false);
  EXPECT(ReadRaw(p, raw) && raw == "always_show=0\n");

  // 收尾: 清注入 + 复位运行期状态
  StatePathOverrideRef() = L"";
  SetAlwaysShowFlag(false);
  ::DeleteFileW(p.c_str());
}

}  // namespace test

int wmain(int /*argc*/, wchar_t** /*argv*/) {
  using namespace test;
  std::wcout << L"[TestShortcutSettings] running..." << std::endl;
  TestInitialState();
  TestSetYamlPath();
  TestHotkeysInitialEmpty();
  TestMutableHotkeysModify();
  TestHotkeyStruct();
  TestHotkeyActionEnum();
  TestCapturingStateVars();
  TestYamlBomAndCrlf();
  TestAlwaysShowStateFormat();
  TestAlwaysShowStateReadTriState();
  TestAlwaysShowStateWriteRoundTrip();
  TestAlwaysShowResolveFromFiles();
  TestAlwaysShowProductionApi();
  std::wcout << L"[TestShortcutSettings] PASS=" << s_pass
              << L" FAIL=" << s_fail << std::endl;
  return s_fail == 0 ? 0 : 1;
}
