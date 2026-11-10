// TestFluxingCloud.cpp — 火流猩 · 用户资料云同步 测试工程骨架（T104）
//
// 本文件当前只承担两件事：
//   1. 证明 test/TestFluxingCloud 工程能编译、能运行、用例能通过（冒烟）；
//   2. 证明 winhttp.lib / bcrypt.lib / advapi32.lib
//   三个系统库**真的接进了构建链**。
//
// 后续批次（T201 信封编解码、T202 HLC、T503
// 合并）的纯逻辑单测追加到本文件下方， 断言一律"生产与单测同源"（宪法
// R7）：被测函数必须在 include/ 下，不在测试里复制实现。

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <winreg.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool ok, const char* name) {
  if (ok) {
    ++g_pass;
    std::printf("  [PASS] %s\n", name);
  } else {
    ++g_fail;
    std::printf("  [FAIL] %s\n", name);
  }
}

// ---- 冒烟 1：工程本身可运行 ----------------------------------------------
void TestHarnessAlive() {
  std::printf("[1] 冒烟：测试工程可运行\n");
  Check(1 + 1 == 2, "算术基线");
  // 证明源文件按 /utf-8 编译：中文窄字符串字面量应是 3 字节 UTF-8。
  Check(std::strlen("火") == 3, "UTF-8 字面量为 3 字节（/utf-8 生效）");
}

// ---- 冒烟 2：winhttp.lib 已链接 -------------------------------------------
void TestWinHttpLinked() {
  std::printf("[2] 链接：winhttp.lib\n");
  HINTERNET session = ::WinHttpOpen(
      L"Fluxing-TestFluxingCloud/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  Check(session != nullptr, "WinHttpOpen 返回有效会话句柄");
  if (session) {
    ::WinHttpCloseHandle(session);
  }
}

// ---- 冒烟 3：bcrypt.lib 已链接（CNG，后续用于 DPAPI/HMAC 基线）------------
void TestBCryptLinked() {
  std::printf("[3] 链接：bcrypt.lib\n");
  BCRYPT_ALG_HANDLE alg = nullptr;
  NTSTATUS st =
      ::BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  Check(NT_SUCCESS(st) && alg != nullptr,
        "BCryptOpenAlgorithmProvider(SHA256) 成功");
  if (NT_SUCCESS(st) && alg) {
    DWORD obj_len = 0;
    DWORD cb = 0;
    NTSTATUS st2 = ::BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                       reinterpret_cast<PUCHAR>(&obj_len),
                                       sizeof(obj_len), &cb, 0);
    Check(NT_SUCCESS(st2) && obj_len > 0,
          "BCryptGetProperty(OBJECT_LENGTH) 返回正数");
    ::BCryptCloseAlgorithmProvider(alg, 0);
  }
}

// ---- 冒烟 4：advapi32.lib 已链接（注册表 / 凭据管理器基线）-----------------
void TestAdvApiLinked() {
  std::printf("[4] 链接：advapi32.lib\n");
  HKEY key = nullptr;
  LONG rc = ::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &key);
  Check(rc == ERROR_SUCCESS && key != nullptr,
        "RegOpenKeyExW(HKCU\\Software) 成功");
  if (rc == ERROR_SUCCESS && key) {
    ::RegCloseKey(key);
  }
}

}  // namespace

int main() {
  ::SetConsoleOutputCP(CP_UTF8);  // 中文输出不乱码
  std::printf("==== TestFluxingCloud · 用户资料云同步 · 骨架冒烟 ====\n");

  TestHarnessAlive();
  TestWinHttpLinked();
  TestBCryptLinked();
  TestAdvApiLinked();

  std::printf("\nRESULT: PASS=%d FAIL=%d\n", g_pass, g_fail);
  std::printf(g_fail == 0 ? "VERDICT: ALL PASS\n" : "VERDICT: FAILED\n");
  return g_fail == 0 ? 0 : 1;
}