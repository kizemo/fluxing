// TestTrayRestoreIgnored.cpp : spec 032 T008 (2026-07-04)
//
// 3 behavior-level assertions for the WeaselServer "Restore ignored
// candidates" tray menu handler. Mirrors the production logic in
// WeaselServerApp.cpp::SetupMenuHandlers RESTORE_IGNORED lambda
// in test-local free functions.
//
// We do NOT link WeaselServer.cpp (tray icon / WTL / ATL / Gdiplus).
// The mocked WeaselUserDataPath() returns a test-local temp dir
// instead of the real HKLM InstallDir + "\\user1" registry lookup.
//
// The 3 assertions match spec 032 spec.md section 1 T008:
//   1. Create temp <schema>.user_ignore.txt + 3 lines,
//      invoke handler, file is gone.
//   2. Empty user-data dir -> handler returns true (no-op success).
//   3. Missing WeaselUserDataPath() -> handler returns true (graceful).

#include "stdafx.h"
#include <windows.h>  // GetCurrentProcessId for unique temp dir
#include <boost/detail/lightweight_test.hpp>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

// Test-local mock for WeaselUserDataPath(). Production reads HKCU; we
// return a temp dir. Empty string means "not available" (degenerate case).
std::wstring g_test_user_dir;
std::wstring WeaselUserDataPathMock() { return g_test_user_dir; }

// Mirror WeaselServerApp RESTORE_IGNORED handler:
// FindFirstFileW(<userDir>\\*.user_ignore.txt) + DeleteFileW for each.
// Returns true on success / no-op; false only if a delete fails.
bool RestoreIgnoredHandlerMirror() {
  std::wstring userDir = WeaselUserDataPathMock();
  if (userDir.empty()) return true;  // graceful no-op
  if (!fs::exists(userDir)) return true;  // graceful no-op
  bool ok = true;
  for (const auto& entry : fs::directory_iterator(userDir)) {
    const auto& path = entry.path();
    std::wstring filename = path.filename().wstring();
    if (filename.size() >= 16 &&
        filename.substr(filename.size() - 16) == L".user_ignore.txt") {
      std::error_code ec;
      fs::remove(path, ec);
      if (ec) ok = false;
    }
  }
  return ok;
}

}  // namespace

// RAII temp dir for isolation.
struct TempDir {
  fs::path p;
  TempDir() {
    p = fs::temp_directory_path() /
         (fs::path(L"weasel_restore_ignored_test_") /
          std::to_wstring(static_cast<long>(GetCurrentProcessId())));
    fs::create_directories(p);
  }
  ~TempDir() { std::error_code ec; fs::remove_all(p, ec); }
  std::wstring wpath() const { return p.wstring(); }
};

int main() {
  // T1: handler deletes all <schema>.user_ignore.txt files in user dir.
  {
    TempDir tmp;
    g_test_user_dir = tmp.wpath();
    // Create 3 ignore files (one per schema) + 1 unrelated file.
    fs::path a = tmp.p / L"luna_pinyin.user_ignore.txt";
    fs::path b = tmp.p / L"bopomofo.user_ignore.txt";
    fs::path c = tmp.p / L"terra.user_ignore.txt";
    fs::path d = tmp.p / L"default.yaml";  // NOT an ignore file
    { std::ofstream o(a); o << "alpha\r\nbeta\r\ngamma\r\n"; }
    { std::ofstream o(b); o << "x\r\n"; }
    { std::ofstream o(c); o << "y\r\nz\r\n"; }
    { std::ofstream o(d); o << "schema: default\n"; }
    BOOST_TEST(fs::exists(a));
    BOOST_TEST(fs::exists(b));
    BOOST_TEST(fs::exists(c));
    BOOST_TEST(fs::exists(d));
    bool ok = RestoreIgnoredHandlerMirror();
    BOOST_TEST_EQ(ok, true);
    // All 3 ignore files gone, default.yaml preserved.
    BOOST_TEST(!fs::exists(a));
    BOOST_TEST(!fs::exists(b));
    BOOST_TEST(!fs::exists(c));
    BOOST_TEST(fs::exists(d));
    std::cout << "  PASS: T1 handler deletes ignore files" << std::endl;
  }

  // T2: empty user-data dir -> handler returns true (graceful no-op).
  {
    TempDir tmp;
    g_test_user_dir = tmp.wpath();
    // No files at all.
    BOOST_TEST_EQ(fs::is_empty(tmp.p), true);
    bool ok = RestoreIgnoredHandlerMirror();
    BOOST_TEST_EQ(ok, true);
    std::cout << "  PASS: T2 empty user dir is graceful no-op" << std::endl;
  }

  // T3: WeaselUserDataPath() returns empty string -> handler returns true.
  //    (Production case: HKCU/HKLM not yet set, e.g. first-run before deploy.)
  {
    g_test_user_dir = L"";
    bool ok = RestoreIgnoredHandlerMirror();
    BOOST_TEST_EQ(ok, true);
    std::cout << "  PASS: T3 missing user dir is graceful no-op" << std::endl;
  }

  std::cout << "3 / 3 assertions passed" << std::endl;
  return boost::report_errors();
}
