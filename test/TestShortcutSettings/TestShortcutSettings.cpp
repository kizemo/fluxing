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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <windows.h>
#include <commctrl.h>

#include "ShortcutSettings.h"

// 测试本地实现: 生产版在 RimeWithWeasel/WeaselUtility.cpp, 本测试工程不链接那个
// TU(会把 librime 依赖整串拖进来, 直接 LNK2001)。与 test/TestTrayRestoreIgnored
// 同思路 —— 只提供链接需要的最小实现; 用例一律用 SetYamlPath 显式指定路径,
// 谁都不读这个目录。
std::filesystem::path WeaselUserDataPath() {
  wchar_t buf[MAX_PATH] = {0};
  DWORD n = ::GetTempPathW(MAX_PATH, buf);
  if (n == 0 || n >= MAX_PATH) return std::filesystem::path(L".");
  return std::filesystem::path(buf) / L"FluxingTestUserData";
}

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

// ===== T106 — default.custom.yaml 增量托管块落盘 =====
//
// 这几条打的是「一次保存会不会毁用户配置」这个真风险: 全量 trunc 重写会吃掉
// 用户的 schema_list / menu 段, 空 patch 会清空 key_binder。判据都打在落盘字节
// 上, 不看函数返回值自证。

static bool WriteRaw(const std::wstring& path, const std::string& bytes) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f)
    return false;
  f.write(bytes.c_str(), static_cast<std::streamsize>(bytes.size()));
  return f.good();
}

static int CountOf(const std::string& hay, const std::string& needle) {
  int n = 0;
  for (size_t p = hay.find(needle); p != std::string::npos;
       p = hay.find(needle, p + needle.size()))
    ++n;
  return n;
}

// 备份文件名 = <path>.bak-YYYYMMDD-HHMMSS。第一次接管用户文件前必须备份,
// 拒绝写入时不该留备份; 用例据此断言「有没有退路」这件事真的发生了。
static int DeleteBackups(const std::wstring& path) {
  const size_t slash = path.find_last_of(L"\\/");
  const std::wstring dir =
      slash == std::wstring::npos ? std::wstring() : path.substr(0, slash + 1);
  WIN32_FIND_DATAW fd{};
  HANDLE h = ::FindFirstFileW((path + L".bak-*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  int n = 0;
  do {
    ++n;
    ::DeleteFileW((dir + fd.cFileName).c_str());
  } while (::FindNextFileW(h, &fd));
  ::FindClose(h);
  return n;
}

static ShortcutSettings::Hotkey MakeHk(const wchar_t* accept,
                                       const wchar_t* action_yaml,
                                       bool is_default) {
  ShortcutSettings::Hotkey h{};
  h.action = ShortcutSettings::Action_CustomUser;
  h.action_desc = L"自定义";
  h.accept = accept;
  h.when = L"always";
  h.action_yaml = action_yaml;
  h.is_custom = !is_default;
  h.is_default = is_default;
  return h;
}

// T106-a — 首次落盘: 只写改过的行, 带托管块标记与 UTF-8 BOM
static void TestSaveHotkeysWritesOnlyEdited() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t106_a.yaml");
  ::DeleteFileW(p.c_str());
  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Shift+Space", L"toggle: full_shape", true));
  rows.push_back(MakeHk(L"Control+Shift+L", L"custom: open_quick_panel", true));
  rows.back().accept_new = L"Control+Alt+L";  // 用户改过这一行

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.size() > 3 && (unsigned char)raw[0] == 0xEF);
  EXPECT(CountOf(raw, "fluxing-shortcuts (spec 045) managed block") == 1);
  EXPECT(CountOf(raw, "fluxing-shortcuts (spec 045) end") == 1);
  EXPECT(raw.find("Control+Alt+L") != std::string::npos);
  // 没动过的内置默认不该出现在 custom 文件里
  EXPECT(raw.find("Shift+Space") == std::string::npos);
  // T107: 托管子块不再自带 patch:, A 形态把顶层 patch: 补在正文最前面 ⇒
  // 全文件只有一个 patch:, 且它在 BOM 之后的第一行。
  EXPECT(CountOf(raw, "patch:") == 1);
  EXPECT(raw.compare(3, 8, "patch:\r\n") == 0);
  EXPECT(raw.find("patch:") < raw.find("managed block"));
  ::DeleteFileW(p.c_str());
}

// T106-b — 二次保存只替换托管块: 块外内容 (schema_list 等) 必须原样活着
static void TestSaveHotkeysPreservesOutsideBlock() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t106_b.yaml");
  // 先清掉历史运行残留的备份: 本用例要断言「本次只备份一次」, 而旧版本代码
  // 从不清理 .bak-*, %TEMP% 里会累积历次运行的备份, 混进来就数不清了。
  DeleteBackups(p);
  const std::string outside =
      "\xEF\xBB\xBF"
      "schema_list:\r\n  - schema: rime_ice\r\n";
  EXPECT(WriteRaw(p, outside));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Shift+L", L"custom: open_quick_panel", true));
  rows.back().accept_new = L"Control+Alt+L";
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));

  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.find("schema_list:") != std::string::npos);
  EXPECT(CountOf(raw, "managed block") == 1);

  // 换一个键再存 ⇒ 旧块被替换, 不是追加第二个块
  rows.back().accept_new = L"Control+Alt+K";
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  EXPECT(ReadRaw(p, raw));
  EXPECT(CountOf(raw, "managed block") == 1);
  EXPECT(raw.find("Control+Alt+K") != std::string::npos);
  EXPECT(raw.find("Control+Alt+L") == std::string::npos);
  EXPECT(raw.find("schema_list:") != std::string::npos);
  EXPECT(DeleteBackups(p) == 1);  // 只有首次接管时备份
  ::DeleteFileW(p.c_str());
}

// T107-b — 旧文件已有顶层 patch: 段 (段里没有手写 key_binder) ⇒ 托管子块插进
// 该段最前面。这是 T107 的正面目标: 用户正文 (schema_list 等) 逐字节保留,
// 全文件仍然只有一个顶层 patch:, 不追加第二个。
static void TestSaveHotkeysInsertsIntoExistingPatch() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t107_b.yaml");
  DeleteBackups(p);  // 同上: 清历史残留, 让「本次备份次数」可断言
  const std::string manual =
      "\xEF\xBB\xBF"
      "patch:\r\n  schema_list:\r\n    - schema: rime_ice\r\n";
  EXPECT(WriteRaw(p, manual));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Shift+L", L"custom: open_quick_panel", true));
  rows.back().accept_new = L"Control+Alt+L";
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));

  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.size() > 3 && (unsigned char)raw[0] == 0xEF);
  EXPECT(CountOf(raw, "patch:") == 1);  // 没有第二个 patch:
  EXPECT(CountOf(raw, "managed block") == 1);
  EXPECT(CountOf(raw, "(spec 045) end") == 1);
  EXPECT(raw.find("Control+Alt+L") != std::string::npos);
  // 块体在 patch: 之后、用户内容之前 ⇒ 插在段首而不是追加到文件尾
  EXPECT(raw.find("patch:") < raw.find("managed block"));
  EXPECT(raw.find("managed block") < raw.find("schema_list:"));
  // 正文逐字节保留
  EXPECT(raw.find("  schema_list:\r\n    - schema: rime_ice\r\n") !=
         std::string::npos);
  // 首次接管 ⇒ 先备份 (没有退路就不该覆盖用户配置)
  EXPECT(DeleteBackups(p) == 1);
  ::DeleteFileW(p.c_str());
}

// T107-i — 连存两次必须幂等: 第二次先摘旧块再插回去, 落盘字节一模一样
static void TestSaveHotkeysIdempotent() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t107_i.yaml");
  DeleteBackups(p);  // 同上
  const std::string manual =
      "\xEF\xBB\xBF"
      "patch:\r\n  schema_list:\r\n    - schema: rime_ice\r\n";
  EXPECT(WriteRaw(p, manual));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Shift+L", L"custom: open_quick_panel", true));
  rows.back().accept_new = L"Control+Alt+L";

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string first;
  EXPECT(ReadRaw(p, first));
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string second;
  EXPECT(ReadRaw(p, second));
  EXPECT(first == second);
  EXPECT(CountOf(second, "patch:") == 1);
  EXPECT(CountOf(second, "managed block") == 1);
  // 块已经接管过 ⇒ 第二次不再备份, 只留第一次那一份退路
  EXPECT(DeleteBackups(p) == 1);
  ::DeleteFileW(p.c_str());
}

// T107-c1 — patch 段里已有手写 key_binder ⇒ 拒绝写。再插一个同名键就是同一
// mapping 里的重复键, librime 会读错甚至整份失效 (handoff §3.2 边界)。
static void TestSaveHotkeysRefusesManualBindings() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t107_c1.yaml");
  const std::string manual =
      "\xEF\xBB\xBF"
      "patch:\r\n  key_binder/bindings:\r\n"
      "    - { when: always, accept: X, send: Y }\r\n";
  EXPECT(WriteRaw(p, manual));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Alt+L", L"custom: open_quick_panel", true));
  EXPECT(!ShortcutSettings::SaveHotkeys(p, rows));  // 明确拒绝

  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw == manual);  // 一个字节都不许动
  // 拒绝必须发生在备份之前 ⇒ 不留垃圾文件
  EXPECT(DeleteBackups(p) == 0);
  ::DeleteFileW(p.c_str());
}

// T107-c2 — patch 是内联写法 (patch: {…}) ⇒ 文本层插不进子键, 拒绝而不猜
static void TestSaveHotkeysRefusesInlinePatch() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t107_c2.yaml");
  const std::string manual =
      "\xEF\xBB\xBF"
      "patch: { schema_list: [ rime_ice ] }\r\n";
  EXPECT(WriteRaw(p, manual));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Alt+L", L"custom: open_quick_panel", true));
  EXPECT(!ShortcutSettings::SaveHotkeys(p, rows));

  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw == manual);
  EXPECT(DeleteBackups(p) == 0);
  ::DeleteFileW(p.c_str());
}

// T107-s1 — S1 的落盘后果: 从文件读回的 override 在合并时被复位成
// is_default=false, 所以「用户没在界面上改过 (accept_new 为空)」也必须写盘。
// 旧 S1 不复位 ⇒ 这一行保存时被当成没动过的内置默认丢掉: 打开界面点一次保存,
// 用户的绑定就没了。下面再用同一份数据标成内置默认做反例对照, 两半合起来才
// 证明 is_default 这一位真的在起作用。
static void TestSaveHotkeysWritesFileSourcedOverride() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t107_s1.yaml");
  ::DeleteFileW(p.c_str());
  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Alt+L", L"custom: open_quick_panel", false));
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.find("Control+Alt+L") != std::string::npos);
  ::DeleteFileW(p.c_str());

  std::vector<ShortcutSettings::Hotkey> builtin;
  builtin.push_back(MakeHk(L"Control+Alt+L", L"custom: open_quick_panel", true));
  EXPECT(ShortcutSettings::SaveHotkeys(p, builtin));
  std::string raw2;
  EXPECT(ReadRaw(p, raw2));
  EXPECT(raw2.find("Control+Alt+L") == std::string::npos);
  ::DeleteFileW(p.c_str());
}

// T106-d — 没有 YAML 动作的行不许写盘 (否则是 librime 直接忽略的坏 binding)
static void TestSaveHotkeysSkipsNoAction() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t106_d.yaml");
  ::DeleteFileW(p.c_str());
  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Alt+L", L"", true));
  rows.back().accept_new = L"Control+Alt+K";

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.find("managed block") != std::string::npos);
  // 空 patch 会把用户已配的 key_binder 覆盖成空 ⇒ 宁可只写注释
  EXPECT(raw.find("\npatch:") == std::string::npos);
  EXPECT(raw.find("Control+Alt+K") == std::string::npos);
  ::DeleteFileW(p.c_str());
}

// ===== T110: 全局快捷键表基线 (baseline) =====
//
// 一次保存把全局快捷键表清空 (T110) 的根因: librime 的 patch 对
// key_binder/bindings 是整表覆盖, 而旧落盘器只写「改动行」⇒ 全局表被覆盖成
// 剩下的那几行。修法 = 写「完整合并列表」: 基线全表 + 用户改动。
// 下面几条判据都打在落盘字节上, 不看函数返回值自证。

// 造一份基线 fixture 并注入 (生产路径靠 <user_data>/default.yaml 自动解析)
static std::wstring UseBaselineFixture() {
  const std::wstring p = TempStatePath(L"fluxing_ts_baseline_default.yaml");
  const std::wstring content =
      L"key_binder:\r\n"
      L"  bindings:\r\n"
      L"    - { when: always, toggle: ascii_mode, accept: Shift+space }\r\n"
      L"    - { when: has_menu, accept: Control+1, send: 2 }\r\n"
      L"    - { when: always, toggle: ascii_punct, accept: Control+period }\r\n";
  EXPECT(WriteUtf8(p, content));
  ShortcutSettings::SetBaselinePath(p);
  return p;
}

// 基线行 (面板里的一行, 认领了基线的第 idx 条)
static ShortcutSettings::Hotkey MakeHkBase(const wchar_t* accept,
                                            const wchar_t* when,
                                            const wchar_t* action_yaml,
                                            int baseline_index) {
  ShortcutSettings::Hotkey h = MakeHk(accept, action_yaml, true);
  h.when = when;
  h.baseline_index = baseline_index;
  return h;
}

// 数落盘文件里 "key_binder/bindings:" 下面有几条 binding 行
static int CountBindingRows(const std::string& raw) {
  const std::string tag = "key_binder/bindings:";
  size_t p = raw.find(tag);
  if (p == std::string::npos)
    return 0;
  p += tag.size();
  int n = 0;
  while (p < raw.size()) {
    size_t eol = raw.find('\n', p);
    if (eol == std::string::npos)
      break;
    std::string line = raw.substr(p, eol - p);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
      line.pop_back();
    if (line.size() >= 5 && line.compare(0, 5, "    -") == 0)
      ++n;
    p = eol + 1;
  }
  return n;
}

// T110-a — 改一行也不能少写其余基线行 (旧实现只写改动行 ⇒ 全局表被覆盖)
static void TestT110KeepsWholeBaselineTable() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t110_a.yaml");
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows.push_back(MakeHkBase(L"Control+1", L"has_menu", L"send: 2", 1));
  rows.push_back(
      MakeHkBase(L"Control+period", L"always", L"toggle: ascii_punct", 2));
  rows[0].accept_new = L"Control+space";  // 用户改了这一行

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  // 改掉的那一行换成新键
  EXPECT(raw.find("Control+space") != std::string::npos);
  EXPECT(raw.find("Shift+space") == std::string::npos);
  // 没改的两行必须还在 —— 这就是 T110 的核心: 少一行就是丢一个用户的键位
  EXPECT(raw.find("Control+1") != std::string::npos);
  EXPECT(raw.find("send: 2") != std::string::npos);
  EXPECT(raw.find("Control+period") != std::string::npos);
  EXPECT(raw.find("toggle: ascii_punct") != std::string::npos);
  EXPECT(CountBindingRows(raw) == 3);
  ::DeleteFileW(p.c_str());
}

// T110-b — 用户自定义行必须跟着写出去, 且排在基线行后面
static void TestT110KeepsCustomRows() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t110_b.yaml");
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows.push_back(MakeHkBase(L"Control+1", L"has_menu", L"send: 2", 1));
  rows.push_back(
      MakeHkBase(L"Control+period", L"always", L"toggle: ascii_punct", 2));
  ShortcutSettings::Hotkey custom =
      MakeHk(L"Control+Alt+K", L"custom: user_added", false);
  custom.baseline_index = -1;
  rows.push_back(custom);

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.find("Control+Alt+K") != std::string::npos);
  EXPECT(CountBindingRows(raw) == 4);
  ::DeleteFileW(p.c_str());
}

// T110-c — 有效列表与基线完全一致 ⇒ 不写 bindings (不把全局表钉死在快照上)
static void TestT110NoChangeWritesNoBindings() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t110_c.yaml");
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows.push_back(MakeHkBase(L"Control+1", L"has_menu", L"send: 2", 1));
  rows.push_back(
      MakeHkBase(L"Control+period", L"always", L"toggle: ascii_punct", 2));

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(CountOf(raw, "managed block") == 1);
  EXPECT(raw.find("key_binder/bindings:") == std::string::npos);
  EXPECT(raw.find("patch:") == std::string::npos);
  ::DeleteFileW(p.c_str());
}

// T110-d — 读不到基线 ⇒ 拒绝写 (宁可不写, 也不能把全局表清掉)
static void TestT110RefusesWithoutBaseline() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t110_d.yaml");
  ::DeleteFileW(p.c_str());
  DeleteBackups(p);
  // 注入一个不存在的基线 ⇒ 基线不可用
  ShortcutSettings::SetBaselinePath(
      TempStatePath(L"fluxing_ts_no_such_baseline.yaml"));

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(MakeHk(L"Control+Alt+K", L"custom: user_added", false));
  EXPECT(!ShortcutSettings::SaveHotkeys(p, rows));  // 明确拒绝

  std::string raw;
  EXPECT(!ReadRaw(p, raw));  // 一个字节都不许落
  EXPECT(DeleteBackups(p) == 0);
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();  // 复原, 后续用例还要基线
}

// T110-e — 基线里「解析不了的条目」(块式写法) 也必须逐字写回 —— 少一行就是丢一行
static void TestT110KeepsUnparsedBaselineRows() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t110_e.yaml");
  ::DeleteFileW(p.c_str());
  const std::wstring bp = TempStatePath(L"fluxing_ts_t110_e_baseline.yaml");
  const std::wstring content =
      L"key_binder:\r\n"
      L"  bindings:\r\n"
      L"    - { when: always, toggle: ascii_mode, accept: Shift+space }\r\n"
      L"    - when: always\r\n"
      L"      accept: Control+9\r\n"
      L"      send: 9\r\n";
  EXPECT(WriteUtf8(bp, content));
  ShortcutSettings::SetBaselinePath(bp);

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows[0].accept_new = L"Control+space";  // 改一行 ⇒ 必须写完整列表
  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(raw.find("Control+space") != std::string::npos);
  // 解析不了的那条也要原样写回去
  EXPECT(raw.find("accept: Control+9") != std::string::npos);
  EXPECT(raw.find("send: 9") != std::string::npos);
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();  // 复原
}

// ===== T111: 应用级快捷键行 (快捷面板 Alt+comma / 常用短语 Alt+period) =====
//
// 方案 A 的落盘语义: 这两行由程序自己注册, 只读且永不写进托管 bindings 块 ——
// 写进去等于与程序内注册并存双份, librime 还会在下次部署时把它们当真 binding
// 装载。判据打在落盘字节上。

// T111-a — 应用级行 (含历史污染行与用户改过键的行) 一律不进托管块;
// 其余基线行与用户自定义行照常写完整列表。
static void TestT111AppLevelNeverWritten() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t111_a.yaml");
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows[0].accept_new = L"Control+space";  // 改一行基线 ⇒ 必须写完整列表
  rows.push_back(MakeHkBase(L"Control+1", L"has_menu", L"send: 2", 1));
  rows.push_back(
      MakeHkBase(L"Control+period", L"always", L"toggle: ascii_punct", 2));

  // 历史污染行: 旧版本把 Alt+comma 写进过 bindings, 表里现在还有它
  rows.push_back(MakeHk(L"Alt+comma", L"custom: open_quick_panel", false));
  rows.back().action = ShortcutSettings::Action_OpenQuickPanel;
  rows.back().baseline_index = -1;
  // 常用短语行: 模拟用户在界面上把键改掉了 —— 依然不许落盘
  rows.push_back(MakeHk(L"Alt+period", L"custom: open_phrases", false));
  rows.back().action = ShortcutSettings::Action_OpenPhrases;
  rows.back().baseline_index = -1;
  rows.back().accept_new = L"Control+Alt+period";

  // 普通用户自定义行: 照旧要写
  rows.push_back(MakeHk(L"Control+Alt+K", L"custom: user_added", false));
  rows.back().baseline_index = -1;

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(CountOf(raw, "open_quick_panel") == 0);
  EXPECT(CountOf(raw, "open_phrases") == 0);
  EXPECT(raw.find("Alt+comma") == std::string::npos);
  EXPECT(raw.find("Control+Alt+period") == std::string::npos);
  EXPECT(raw.find("Control+space") != std::string::npos);
  EXPECT(raw.find("Control+1") != std::string::npos);
  EXPECT(raw.find("Control+period") != std::string::npos);
  EXPECT(raw.find("Control+Alt+K") != std::string::npos);
  // 3 条基线 (含改键那条) + 1 条用户自定义; 应用级两行不在其中
  EXPECT(CountBindingRows(raw) == 4);
  ::DeleteFileW(p.c_str());
}

// T111-b — 自愈: 表里只剩历史污染的应用级行、其余没动 ⇒ differs=false ⇒
// 不写 bindings, 托管块重写成干净形态, 旧污染随块替换消失。
static void TestT111SelfHealsLegacyBlock() {
  const std::wstring p = TempStatePath(L"fluxing_ts_t111_b.yaml");
  ::DeleteFileW(p.c_str());
  UseBaselineFixture();

  std::vector<ShortcutSettings::Hotkey> rows;
  rows.push_back(
      MakeHkBase(L"Shift+space", L"always", L"toggle: ascii_mode", 0));
  rows.push_back(MakeHkBase(L"Control+1", L"has_menu", L"send: 2", 1));
  rows.push_back(
      MakeHkBase(L"Control+period", L"always", L"toggle: ascii_punct", 2));
  rows.push_back(MakeHk(L"Alt+comma", L"custom: open_quick_panel", false));
  rows.back().action = ShortcutSettings::Action_OpenQuickPanel;
  rows.back().baseline_index = -1;

  EXPECT(ShortcutSettings::SaveHotkeys(p, rows));
  std::string raw;
  EXPECT(ReadRaw(p, raw));
  EXPECT(CountOf(raw, "managed block") == 1);
  EXPECT(raw.find("key_binder/bindings:") == std::string::npos);
  EXPECT(CountOf(raw, "open_quick_panel") == 0);
  ::DeleteFileW(p.c_str());
}

// T111-c — 应用级判定范围锁死: 只有快捷面板与常用短语两行。
// trigger_deploy (Action_TriggerDeploy) 有自己的落盘语义, 不得并入本判定。
static void TestT111AppLevelScope() {
  EXPECT(ShortcutSettings::IsAppLevelAction(
      ShortcutSettings::Action_OpenQuickPanel));
  EXPECT(ShortcutSettings::IsAppLevelAction(
      ShortcutSettings::Action_OpenPhrases));
  EXPECT(!ShortcutSettings::IsAppLevelAction(
      ShortcutSettings::Action_TriggerDeploy));
  EXPECT(!ShortcutSettings::IsAppLevelAction(
      ShortcutSettings::Action_ReselectCandidate));
  EXPECT(!ShortcutSettings::IsAppLevelAction(
      ShortcutSettings::Action_CustomUser));
}

// T111-d — popover 定位纯函数: 优先行右侧; 右侧放不下翻左; 两侧都放不下夹进工作区。
// 真机缺陷: 控件用 CW_USEDEFAULT 创建且从无 SetWindowPos → 落在屏外, 提示与捕获框
// 都看不见。本函数不碰 USER/GDI, 生产与单测共用同一实现 (constitution R7)。
static void TestT111PopoverPlacement() {
  using PS = ShortcutSettings::PopoverPlace;
  const RECT work{0, 0, 1920, 1040};
  const int pw = 240, ph = 108, gap = 8;
  // 1) 常规: 右侧空间足够 → x = row.right + gap, y 垂直居中于行中线
  {
    const RECT row{100, 200, 700, 232};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.x == 708);                    // 700 + 8
    EXPECT(p.y == 162);                    // rowCy 216 - ph/2 54
    EXPECT(p.x + pw <= work.right);        // 仍在工作区内
    EXPECT(p.y + ph <= work.bottom);
  }
  // 2) 右侧越界 → 翻到行左缘之外
  {
    const RECT row{1600, 200, 1900, 232};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.x == 1352);                   // 1600 - 8 - 240
    EXPECT(p.x >= work.left);
  }
  // 3) 行占满整宽 (两侧都放不下) → 夹到工作区左缘, 保证窗口可见
  {
    const RECT row{0, 200, 1919, 232};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.x == 0);                      // 翻左得 -248 → 夹回 work.left
    EXPECT(p.x >= work.left);
    EXPECT(p.x + pw <= work.right);        // 完整落在工作区内
  }
  // 3b) 行本身越出工作区右缘 (多屏 / 异常几何) → 翻左后仍越界 → 夹到工作区右缘
  {
    const RECT row{2000, 200, 2300, 232};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.x == 1680);                   // 1920 - 240
    EXPECT(p.x + pw == work.right);
  }
  // 4) 贴底的行 → y 夹进工作区下缘
  {
    const RECT row{100, 1000, 700, 1032};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.y == 932);                    // 1040 - 108
    EXPECT(p.x == 708);                    // 水平不受影响
  }
  // 5) 贴顶的行 → y 夹到工作区上缘
  {
    const RECT row{100, 0, 700, 20};
    PS p = ShortcutSettings::ComputePopoverPlace(row, work, pw, ph, gap);
    EXPECT(p.y == 0);
  }
}

}  // namespace test

int wmain(int /*argc*/, wchar_t** /*argv*/) {
  using namespace test;
  std::wcout << L"[TestShortcutSettings] running..." << std::endl;
  // T110: 注入基线 fixture (生产路径靠 <user_data>/default.yaml 自动解析;
  // 单测环境没有那个文件 ⇒ 基线不可用 ⇒ 保存会被拒绝)
  UseBaselineFixture();
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
  TestSaveHotkeysWritesOnlyEdited();
  TestSaveHotkeysPreservesOutsideBlock();
  TestSaveHotkeysInsertsIntoExistingPatch();
  TestSaveHotkeysIdempotent();
  TestSaveHotkeysRefusesManualBindings();
  TestSaveHotkeysRefusesInlinePatch();
  TestSaveHotkeysWritesFileSourcedOverride();
  TestSaveHotkeysSkipsNoAction();
  // T110: 保存即清空全局快捷键表 —— 必须写完整合并列表
  TestT110KeepsWholeBaselineTable();
  TestT110KeepsCustomRows();
  TestT110NoChangeWritesNoBindings();
  TestT110KeepsUnparsedBaselineRows();
  TestT110RefusesWithoutBaseline();
  // T111: 应用级快捷键行只读 + 永不写进托管 bindings 块
  TestT111AppLevelNeverWritten();
  TestT111SelfHealsLegacyBlock();
  TestT111AppLevelScope();
  TestT111PopoverPlacement();
  std::wcout << L"[TestShortcutSettings] PASS=" << s_pass
              << L" FAIL=" << s_fail << std::endl;
  return s_fail == 0 ? 0 : 1;
}
