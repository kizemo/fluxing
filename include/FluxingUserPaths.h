#pragma once
// Fluxing 用户数据目录解析 —— 单一事实源(header-only)。
//
// ADR 0020 的根因:1.1.2 之前 quickpanel.state 硬编码在 %APPDATA%\Rime\,与本
// fork 的用户数据目录(%AppData%\fluxing,或安装器写入的 HKCU RimeUserDir)
// 分叉。于是 ADR 0019 D1 的立论前提「与 ShortcutSettings 同目录,备份用户数据
// 目录时状态随目录一起走」事实上不成立 —— ShortcutSettings 走的是
// WeaselUserDataPath(),quickpanel.state 走的是另一条路。
//
// 为什么单独成文件:include/WeaselUtility.h 依赖 stdafx 预置的 <chrono> 等头
// 文件,还把 COM/wrl 一并拖进来;QuickPanelDialog 与单元测试工程
// (test/TestQuickPanelDialog 不链接 RimeWithWeasel)都不适合直接包含它。
// ⇒ 解析逻辑下沉到这里,RimeWithWeasel/WeaselUtility.cpp 与
// WeaselServer/QuickPanelDialog.cpp 共用同一份实现。
//
// 依赖面:仅 windows.h + <filesystem> + <string>,不引入 shell32 / COM。

#include <windows.h>

#include <filesystem>
#include <string>

// spec 002 FR-002: 强制用户数据路径以 `fluxing` 结尾。
// Idempotent. Treats the path case-insensitively for the last segment; appends
// `\fluxing` if and only if the last path component is not already `fluxing`.
// Preserves any trailing backslash the caller originally supplied.
inline std::wstring EnsureFluxingUserDataSuffix(const std::wstring& path) {
  if (path.empty())
    return path;
  std::wstring p = path;
  while (!p.empty() && (p.back() == L'\\' || p.back() == L'/'))
    p.pop_back();
  if (p.empty())
    return path;
  size_t sep = p.find_last_of(L"\\/");
  std::wstring last = (sep == std::wstring::npos) ? p : p.substr(sep + 1);
  auto ieq = [](const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); ++i) {
      wchar_t ca = a[i];
      if (ca >= L'A' && ca <= L'Z')
        ca = (wchar_t)(ca - L'A' + L'a');
      wchar_t cb = b[i];
      if (cb >= L'A' && cb <= L'Z')
        cb = (wchar_t)(cb - L'A' + L'a');
      if (ca != cb)
        return false;
    }
    return true;
  };
  if (ieq(last, L"fluxing"))
    return path;
  std::wstring out = p + L"\\fluxing";
  if (path.back() == L'\\' || path.back() == L'/')
    out += path.back();
  return out;
}

// 解析用户数据目录:
//   1) HKCU\Software\Fluxing\Weasel :: RimeUserDir(安装器写入,可能被指到 D 盘)
//   2) 回退 %AppData%\fluxing
inline std::filesystem::path ResolveFluxingUserDataPath() {
  WCHAR _path[MAX_PATH] = {0};
  const WCHAR KEY[] = L"Software\\Fluxing\\Weasel";
  HKEY hKey;
  LSTATUS ret = RegOpenKey(HKEY_CURRENT_USER, KEY, &hKey);
  if (ret == ERROR_SUCCESS) {
    DWORD len = sizeof(_path);
    DWORD type = 0;
    ret =
        RegQueryValueEx(hKey, L"RimeUserDir", NULL, &type, (LPBYTE)_path, &len);
    RegCloseKey(hKey);
    if (ret == ERROR_SUCCESS && type == REG_SZ && _path[0]) {
      // spec 002 FR-002: enforce fluxing suffix (safety net for tools that
      // may have written a non-fluxing path to the registry)
      std::wstring enforced = EnsureFluxingUserDataSuffix(std::wstring(_path));
      return std::filesystem::path(enforced);
    }
  }
  // default location
  ExpandEnvironmentStringsW(L"%AppData%\\fluxing", _path, _countof(_path));
  return std::filesystem::path(_path);
}
