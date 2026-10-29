// TestFluxingInstallPaths.cpp - spec 088 T3 (2026-10-29)
//
// 行为级测试:程序目录(binary root / WEASEL_ROOT)解析。
//
// 被测对象是生产实现本身 include/FluxingInstallPaths.h(L24 link-probe 模式:
// 直接包含真实头文件,不做镜像复刻)。
//
// 断言分组:
//   T1-T3  安装根 → 程序目录映射(纯字符串:无尾分隔符 / 带尾分隔符 / 空串)
//   T4-T6  候选目录筛选(不像的候选要被跳过;全不像时退回第一个非空候选)
//   T7-T9  启动目标选择(bat 优先 / 退回 exe / 都没有取空)
//   T10    候选来源清单(fork 键优先、旧键兜底 = spec 002 FR-008)
//   T11-T13 端到端解析 + 「旧逻辑解析不到」的负对照
//
// 负对照的用意:复刻上游 WeaselTSF/LanguageBar.cpp 的旧写法(只读 "WeaselRoot"),
// 断言它在装了 fork 的机器上解析不到程序目录 —— 证明这组断言不是恒真。
// 未装 fork 的机器(比如 CI)上 T11-T13 标记为 SKIP,纯逻辑断言仍然全跑。

#include "stdafx.h"

#include <FluxingInstallPaths.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool cond, const char* name) {
  if (cond) {
    ++g_pass;
    std::printf("  PASS: %s\n", name);
  } else {
    ++g_fail;
    std::printf("  FAIL: %s\n", name);
  }
}

std::filesystem::path MakeTempDir(const wchar_t* tag) {
  const std::filesystem::path base =
      std::filesystem::temp_directory_path() / L"fluxing_install_paths_test";
  const std::filesystem::path dir = base / tag;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return dir;
}

void TouchFile(const std::filesystem::path& p) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << "stub";
}

// 旧实现复刻:上游 WeaselTSF/LanguageBar.cpp 的 GetWeaselRegName() +
// "WeaselRoot"。
//   32 位分支读 Software\WOW6432Node\Rime\Weasel,64 位分支读
//   Software\Fluxing\Weasel; 两者都只读 "WeaselRoot",读不到就返回空串。
std::wstring LegacyResolveWeaselRoot() {
  BOOL wow64 = FALSE;
  if (!IsWow64Process(GetCurrentProcess(), &wow64))
    wow64 = FALSE;
  const wchar_t* key = wow64 ? L"Software\\WOW6432Node\\Rime\\Weasel"
                             : L"Software\\Fluxing\\Weasel";
  HKEY hk = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hk) != ERROR_SUCCESS)
    return std::wstring();
  wchar_t buf[MAX_PATH] = {0};
  DWORD cb = sizeof(buf);
  DWORD type = 0;
  const LSTATUS st = RegQueryValueExW(hk, L"WeaselRoot", nullptr, &type,
                                      reinterpret_cast<LPBYTE>(buf), &cb);
  RegCloseKey(hk);
  if (st != ERROR_SUCCESS || type != REG_SZ)
    return std::wstring();
  return std::wstring(buf);
}

}  // namespace

namespace fluxing_test {

int RunInstallPathsTest() {
  std::printf("\n[spec 088] 程序目录解析(binary root / WEASEL_ROOT)\n");

  // ----------------------------------------------------------------
  // T1: 安装根 → 程序目录(无尾分隔符)
  // ----------------------------------------------------------------
  const std::wstring mapped =
      fluxing::BinaryRootFromInstallDir(L"D:\\Program Files\\fluxing");
  Check(mapped == L"D:\\Program Files\\fluxing\\weasel",
        "T1a: 安装根 → <安装根>\\weasel");

  // ----------------------------------------------------------------
  // T2: 带尾分隔符时不产生双分隔符
  // ----------------------------------------------------------------
  const std::wstring mapped_slash =
      fluxing::BinaryRootFromInstallDir(L"D:\\Program Files\\fluxing\\");
  Check(mapped_slash == L"D:\\Program Files\\fluxing\\weasel",
        "T2a: 安装根带尾分隔符 → 同样结果");

  // ----------------------------------------------------------------
  // T3: 空串 → 空串;盘符根 → C:\weasel
  // ----------------------------------------------------------------
  Check(fluxing::BinaryRootFromInstallDir(L"").empty(),
        "T3a: 空安装根 → 空结果");
  Check(fluxing::BinaryRootFromInstallDir(L"C:\\") == L"C:\\weasel",
        "T3b: 盘符根 → C:\\weasel");

  // ----------------------------------------------------------------
  // T4: 候选里第一个不存在 → 被跳过,选中第二个
  // ----------------------------------------------------------------
  const std::filesystem::path good = MakeTempDir(L"good");
  TouchFile(good / L"WeaselServer.exe");
  const std::filesystem::path absent = MakeTempDir(L"absent") / L"not_created";
  {
    const std::vector<std::wstring> candidates = {absent.wstring(),
                                                  good.wstring()};
    Check(fluxing::PickWeaselRoot(candidates) == good.wstring(),
          "T4a: 不存在的候选被跳过,选中像程序目录的那个");
  }

  // ----------------------------------------------------------------
  // T5: 候选存在但里面没有 WeaselServer.exe / start_service.bat → 被跳过
  // ----------------------------------------------------------------
  const std::filesystem::path empty_dir = MakeTempDir(L"empty");
  {
    const std::vector<std::wstring> candidates = {empty_dir.wstring(),
                                                  good.wstring()};
    Check(fluxing::PickWeaselRoot(candidates) == good.wstring(),
          "T5a: 空目录被跳过,选中像程序目录的那个");
  }

  // ----------------------------------------------------------------
  // T6: 全都不像 → 退回第一个非空候选(让调用方能报出路径);空列表 → 空
  // ----------------------------------------------------------------
  {
    const std::vector<std::wstring> candidates = {absent.wstring(),
                                                  empty_dir.wstring()};
    Check(fluxing::PickWeaselRoot(candidates) == absent.wstring(),
          "T6a: 全不像时退回第一个非空候选");
    Check(fluxing::PickWeaselRoot({}).empty(), "T6b: 空候选列表 → 空结果");
  }

  // ----------------------------------------------------------------
  // T7-T9: 启动目标选择
  // ----------------------------------------------------------------
  const std::filesystem::path both = MakeTempDir(L"both");
  TouchFile(both / L"start_service.bat");
  TouchFile(both / L"WeaselServer.exe");
  const std::filesystem::path exe_only = MakeTempDir(L"exe_only");
  TouchFile(exe_only / L"WeaselServer.exe");
  {
    const std::wstring t = fluxing::WeaselServiceLaunchTarget(both.wstring());
    Check(t == (both / L"start_service.bat").wstring(),
          "T7a: 同时有 bat 与 exe 时优先 start_service.bat");
    const std::wstring t2 =
        fluxing::WeaselServiceLaunchTarget(exe_only.wstring());
    Check(t2 == (exe_only / L"WeaselServer.exe").wstring(),
          "T8a: 没有 bat 时退回 WeaselServer.exe");
    Check(fluxing::WeaselServiceLaunchTarget(empty_dir.wstring()).empty(),
          "T9a: 两者都没有 → 空目标");
    Check(fluxing::WeaselServiceLaunchTarget(L"").empty(),
          "T9b: 空目录 → 空目标");
  }

  // ----------------------------------------------------------------
  // T10: 候选来源清单 —— fork 键优先,旧键兜底(spec 002 FR-006/FR-008)
  // ----------------------------------------------------------------
  {
    const std::vector<fluxing::InstallRootSource> sources =
        fluxing::InstallRootSources();
    bool has_fork_root = false;
    bool has_fork_install = false;
    bool has_legacy = false;
    for (const fluxing::InstallRootSource& s : sources) {
      const std::wstring key = s.sub_key;
      if (key == L"Software\\Fluxing\\Weasel" &&
          std::wstring(s.value_name) == L"WeaselRoot")
        has_fork_root = true;
      if (key == L"Software\\Fluxing\\Weasel" &&
          std::wstring(s.value_name) == L"InstallDir" && s.append_binary_subdir)
        has_fork_install = true;
      if (key == L"Software\\Rime\\Weasel")
        has_legacy = true;
    }
    Check(has_fork_root, "T10a: 候选含 fork 键的 WeaselRoot");
    Check(has_fork_install, "T10b: 候选含 fork 键的 InstallDir(拼 \\weasel)");
    Check(has_legacy, "T10c: 候选含旧键 Software\\Rime\\Weasel(就地升级兜底)");
  }

  // ----------------------------------------------------------------
  // T11-T13: 端到端解析(仅在装了 fork 的机器上有效)
  // ----------------------------------------------------------------
  std::wstring install_dir;
  if (fluxing::ReadFluxingInstallDir(install_dir)) {
    const std::wstring expected =
        fluxing::BinaryRootFromInstallDir(install_dir);
    const std::vector<std::wstring> candidates =
        fluxing::WeaselRootDirCandidates();
    bool has_expected = false;
    for (const std::wstring& c : candidates) {
      if (c == expected)
        has_expected = true;
    }
    Check(has_expected, "T11a: 候选列表含 <安装根>\\weasel");

    const std::wstring resolved = fluxing::ResolveFluxingWeaselRoot();
    std::printf("  INFO: 安装根 = %ls,解析到的程序目录 = %ls\n",
                install_dir.c_str(), resolved.c_str());
    Check(!resolved.empty(), "T12a: 端到端解析出非空程序目录");
    Check(fluxing::LooksLikeWeaselRoot(resolved),
          "T12b: 解析结果里确实有 WeaselServer.exe / start_service.bat");

    const std::wstring legacy = LegacyResolveWeaselRoot();
    Check(legacy != resolved,
          "T13a: 旧逻辑(只读 WeaselRoot)解析不到程序目录(负对照)");
  } else {
    std::printf(
        "  SKIP: 本机未安装 fork(注册表无 InstallDir),跳过端到端断言\n");
  }

  std::printf("\nTestFluxingInstallPaths: %d/%d PASS\n", g_pass,
              g_pass + g_fail);
  return g_fail == 0 ? 0 : 1;
}

}  // namespace fluxing_test
