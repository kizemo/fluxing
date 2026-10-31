// spec 093 T002 - TestServerCmdLine.cpp
//
// 行为级测试：WeaselServer.exe 的命令行选项解析（L160 缺陷回归）。
//
// 缺陷背景
// --------
//   _tWinMain 收到的 lpstrCmdLine 是 WinMain 的**原始** lpCmdLine。MSDN
//   明确说明该字符串在第一个参数之前带一个空格：
//     WeaselServer.exe /quit   ->   lpCmdLine == " /quit"
//
//   旧代码用整串比较判定选项：
//     if (!wcscmp(L"/quit", lpstrCmdLine)) { ... }
//   对 " /quit" 恒为假，于是 /q /quit /ascii /nascii /userdir /weaseldir
//   /update 这 7 个选项**全部是死选项**。其中 /quit 实际落入"已在运行则
//   重启服务"分支，语义与用户预期完全相反（用户要停服，程序在重启服务）。
//
// 测试策略
// --------
//   本测试直接调用生产代码所用的**同一份共享头**（宪法 R7：生产与测试同源，
//   共用共享头纯函数），不在测试里重写一份解析逻辑，避免两份实现各自漂移。
//
//   用例：
//     T1 WinMain 原始命令行带前导空格时，全部 7 个选项仍可达（缺陷回归）
//     T2 /q 与 /quit 等价；大小写语义保持旧的精确匹配
//     T3 空 / 空白 / 未知选项 -> None
//     T4 记号化：尾随空白、多余参数、Tab -> 仍取首个记号
//     T5 缺陷见证：旧的整串比较对原始命令行恒为假（锁死根因）
//     T6 调用点守卫：WeaselServer.cpp 必须走共享解析，不得回退到整串 wcscmp
//
//   T6 存在的原因：单元测试覆盖的是共享头，覆盖不到 WeaselServer.cpp 的
//   调用点；如果有人把调用点改回 `wcscmp(L"/quit", lpstrCmdLine)`，T1–T5
//   依然全绿。T6 把调用点钉死，是本测试的判别性所在。

#include "stdafx.h"

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <windows.h>

// 生产与测试同源：与 WeaselServer.cpp 引用的是同一个头文件。
#include "../../WeaselServer/ServerCmdLine.h"

namespace {

int g_failures = 0;

#define EXPECT(cond)                                                       \
  do {                                                                      \
    if (!(cond)) {                                                          \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                         \
    }                                                                       \
  } while (0)

using fluxing::servercmd::Option;

// --- T1 缺陷回归：WinMain 原始命令行带前导空格，选项仍须可达 ---
void T1_LeadingSpaceIsTolerated() {
  std::printf("[T1] WinMain raw lpCmdLine carries a leading space...\n");
  // 真实形态：WeaselServer.exe /quit -> lpCmdLine == " /quit"
  EXPECT(fluxing::servercmd::Parse(L" /quit") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /q") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /ascii") == Option::Ascii);
  EXPECT(fluxing::servercmd::Parse(L" /nascii") == Option::Nascii);
  EXPECT(fluxing::servercmd::Parse(L" /userdir") == Option::UserDir);
  EXPECT(fluxing::servercmd::Parse(L" /weaseldir") == Option::WeaselDir);
  EXPECT(fluxing::servercmd::Parse(L" /update") == Option::Update);
  // 无前导空格的写法同样必须工作。
  EXPECT(fluxing::servercmd::Parse(L"/quit") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L"/ascii") == Option::Ascii);
}

// --- T2 /q 与 /quit 等价；保持旧的精确匹配（区分大小写）语义 ---
void T2_QuitAliases() {
  std::printf("[T2] /q is an alias of /quit; case stays exact...\n");
  EXPECT(fluxing::servercmd::Parse(L" /q") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /q") == fluxing::servercmd::Parse(L" /quit"));
  // 旧实现用 wcscmp 精确匹配，修复不扩大为不区分大小写。
  EXPECT(fluxing::servercmd::Parse(L" /QUIT") == Option::None);
  EXPECT(fluxing::servercmd::Parse(L" /Quit") == Option::None);
}

// --- T3 空 / 空白 / 未知选项 -> None ---
void T3_NoneCases() {
  std::printf("[T3] empty / blank / unknown -> None...\n");
  EXPECT(fluxing::servercmd::Parse(L"") == Option::None);
  EXPECT(fluxing::servercmd::Parse(L"   ") == Option::None);
  EXPECT(fluxing::servercmd::Parse(L"\t") == Option::None);
  EXPECT(fluxing::servercmd::Parse(static_cast<const wchar_t*>(NULL)) == Option::None);
  EXPECT(fluxing::servercmd::Parse(L" /bogus") == Option::None);
  EXPECT(fluxing::servercmd::Parse(L" -quit") == Option::None);
  EXPECT(fluxing::servercmd::Parse(L" quit") == Option::None);
}

// --- T4 记号化：取首个记号，忽略其后的空白与多余参数 ---
void T4_Tokenization() {
  std::printf("[T4] Tokenization: first token wins...\n");
  EXPECT(fluxing::servercmd::Parse(L"  /quit  ") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /quit extra") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L"\t/quit\r\n") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /ascii /quit") == Option::Ascii);
}

// --- T5 缺陷见证：旧的整串比较对 WinMain 原始命令行恒为假 ---
void T5_LegacyWholeStringCompareWasAlwaysFalse() {
  std::printf("[T5] Legacy whole-string compare was always false...\n");
  // 旧实现：!wcscmp(L"/quit", lpstrCmdLine)。对原始 lpCmdLine 恒为假，
  // 这就是 7 个选项全部失效的根因。这里把根因钉死，防止有人只修一半。
  EXPECT(std::wcscmp(L"/quit", L" /quit") != 0);
  EXPECT(std::wcscmp(L"/q", L" /q") != 0);
  EXPECT(std::wcscmp(L"/ascii", L" /ascii") != 0);
  EXPECT(std::wcscmp(L"/nascii", L" /nascii") != 0);
  EXPECT(std::wcscmp(L"/userdir", L" /userdir") != 0);
  EXPECT(std::wcscmp(L"/weaseldir", L" /weaseldir") != 0);
  EXPECT(std::wcscmp(L"/update", L" /update") != 0);
  // 同一批命令行，新的共享解析能识别。
  EXPECT(fluxing::servercmd::Parse(L" /quit") == Option::Quit);
  EXPECT(fluxing::servercmd::Parse(L" /update") == Option::Update);
}

bool FileExists(const std::wstring& path) {
  DWORD attr = ::GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

// 定位仓库根：优先用 exe 所在目录（<repo>\Release\xxx.exe）的上一级，
// 退化到当前工作目录。
std::wstring RepoRoot() {
  const wchar_t* kServerSrc = L"WeaselServer\\WeaselServer.cpp";
  wchar_t exe[MAX_PATH] = {0};
  DWORD n = ::GetModuleFileNameW(NULL, exe, MAX_PATH);
  if (n > 0 && n < MAX_PATH) {
    std::wstring p(exe, n);
    size_t slash = p.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
      std::wstring dir = p.substr(0, slash);
      size_t up = dir.find_last_of(L"\\/");
      if (up != std::wstring::npos) {
        std::wstring root = dir.substr(0, up);
        if (FileExists(root + L"\\" + kServerSrc)) return root;
      }
      if (FileExists(dir + L"\\" + kServerSrc)) return dir;
    }
  }
  wchar_t cwd[MAX_PATH] = {0};
  if (::GetCurrentDirectoryW(MAX_PATH, cwd) != 0) {
    std::wstring c(cwd);
    if (FileExists(c + L"\\" + kServerSrc)) return c;
  }
  return std::wstring();
}

bool ReadFileBytes(const std::wstring& path, std::string& out) {
  HANDLE h = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size;
  if (!::GetFileSizeEx(h, &size) || size.QuadPart <= 0) {
    ::CloseHandle(h);
    return false;
  }
  out.resize(static_cast<size_t>(size.QuadPart));
  DWORD got = 0;
  BOOL ok = ::ReadFile(h, &out[0], static_cast<DWORD>(out.size()), &got, NULL);
  ::CloseHandle(h);
  if (!ok) return false;
  out.resize(got);
  return true;
}

// --- T6 调用点守卫：生产代码必须走共享解析 ---
void T6_CallSiteUsesSharedParser() {
  std::printf("[T6] Call-site guard: WeaselServer.cpp must use the shared parser...\n");
  std::wstring root = RepoRoot();
  EXPECT(!root.empty());
  if (root.empty()) {
    std::fprintf(stderr, "     cannot locate repo root (WeaselServer\\WeaselServer.cpp)\n");
    return;
  }
  std::string src;
  EXPECT(ReadFileBytes(root + L"\\WeaselServer\\WeaselServer.cpp", src));
  if (src.empty()) {
    std::fprintf(stderr, "     cannot read WeaselServer\\WeaselServer.cpp\n");
    return;
  }
  // 正向：调用点必须调用共享解析。
  EXPECT(src.find("servercmd::Parse") != std::string::npos);
  // 反向：不得残留任何整串选项比较（旧实现的 wcscmp(L"/xxx", lpstrCmdLine)）。
  EXPECT(src.find("wcscmp(L\"/") == std::string::npos);
}

}  // namespace

int main() {
  std::printf("=== TestServerCmdLine (spec 093, L160) ===\n");
  T1_LeadingSpaceIsTolerated();
  T2_QuitAliases();
  T3_NoneCases();
  T4_Tokenization();
  T5_LegacyWholeStringCompareWasAlwaysFalse();
  T6_CallSiteUsesSharedParser();

  if (g_failures == 0) {
    std::printf("=== ALL TESTS PASSED ===\n");
    std::fflush(stdout);
    ExitProcess(0);
    return 0;
  }
  std::fprintf(stderr, "=== %d FAILURE(S) ===\n", g_failures);
  std::fflush(stderr);
  ExitProcess(1);
  return 1;
}
