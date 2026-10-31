#pragma once
// Fluxing 程序目录(binary root / WEASEL_ROOT)解析 —— 单一事实源(header-only)。
//
// 背景(fork 级缺陷,spec 088 修复):
//   上游 WeaselTSF/LanguageBar.cpp 用 GetWeaselRegName() 取注册表键,再读值
//   "WeaselRoot" 当作程序目录。本 fork 的安装器从不写 WeaselRoot ——
//   output/install.nsi 只写 HKLM\Software\Fluxing\Weasel :: InstallDir
//   (用户可见安装根),程序在 <InstallDir>\weasel(install.nsi:25 的
//   WEASEL_ROOT = $INSTDIR\weasel)。
//   ⇒「重启算法服务」「程序文件夹」以及 WeaselTSF.cpp 的「服务未运行时自动
//     拉起」全部拿到空串,静默失败:点了没反应,也不报错。
//
// 注册表视图坑(本机实测,见 spec 088 plan.md 取证 E5):
//   fork 的 NSIS 安装器把 InstallDir 写进 **32 位视图**
//   (HKLM\SOFTWARE\WOW6432Node\Fluxing\Weasel);64 位视图下该键根本不存在。
//   ⇒ 64 位输入法前端用默认视图读 HKLM\Software\Fluxing\Weasel 必然落空;
//     32 位前端用默认视图读反而命中(WOW64 重定向)。
//   ⇒ 不能用「拼 WOW6432Node 路径」或「拼视图前缀」这种固定写法,必须显式
//     遍历 KEY_WOW64_64KEY / KEY_WOW64_32KEY 两个视图。
//
// 契约来源:spec 002 FR-006(fork 的注册表写入集中在 Software\Fluxing\Weasel)
//           + spec 002 FR-008(旧的 Software\Rime\Weasel 读法必须继续可用)。
//
// 为什么单独成文件:include/WeaselUtility.h 依赖 stdafx 预置的 <chrono> 等头
// 文件,还把 COM/wrl 一并拖进来;WeaselTSF 与单元测试工程都不适合直接包含它。
// ⇒ 解析逻辑下沉到这里,WeaselTSF/LanguageBar.cpp 与
//   test/TestFluxingInstallPaths 共用同一份实现。
//
// 依赖面:仅 windows.h + <filesystem> + <string> + <vector>,不引入 shell32 /
// COM。

#include <windows.h>

#include <cwchar>
#include <filesystem>
#include <string>
#include <vector>

namespace fluxing {

// 程序子目录名 —— 与 output/install.nsi 的 WEASEL_ROOT = $INSTDIR\weasel 一致。
inline const wchar_t* BinaryRootSubdir() {
  return L"weasel";
}

// 判断一个目录"像不像"程序目录:里面有 WeaselServer.exe 或 start_service.bat。
inline bool LooksLikeWeaselRoot(const std::wstring& dir) {
  if (dir.empty())
    return false;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec))
    return false;
  const std::filesystem::path base(dir);
  return std::filesystem::exists(base / L"WeaselServer.exe", ec) ||
         std::filesystem::exists(base / L"start_service.bat", ec);
}

// 纯映射:用户可见安装根 → 程序目录。
//   "D:\Program Files\fluxing"   → "D:\Program Files\fluxing\weasel"
//   "D:\Program Files\fluxing\"  → 同上(不产生双分隔符)
//   ""                           → ""
inline std::wstring BinaryRootFromInstallDir(const std::wstring& install_dir) {
  if (install_dir.empty())
    return std::wstring();
  std::wstring p = install_dir;
  while (p.size() > 1 && (p.back() == L'\\' || p.back() == L'/'))
    p.pop_back();
  return p + L"\\" + BinaryRootSubdir();
}

// 候选来源描述。顺序即优先级:fork 键优先(spec 002 FR-006),旧键兜底
// (spec 002 FR-008,保证就地升级与上游共存机器都能用)。
struct InstallRootSource {
  const wchar_t* sub_key;     // 注册表子键
  const wchar_t* value_name;  // 值名
  bool append_binary_subdir;  // true ⇒ 取值后拼 <值>\weasel
};

inline std::vector<InstallRootSource> InstallRootSources() {
  return {
      {L"Software\\Fluxing\\Weasel", L"WeaselRoot", false},
      {L"Software\\Fluxing\\Weasel", L"InstallDir", true},
      {L"Software\\Rime\\Weasel", L"WeaselRoot", false},
      {L"Software\\Rime\\Weasel", L"InstallDir", true},
  };
}

namespace detail {

// 读一个注册表字符串值;view_flags 用来显式指定 32/64 位视图。
inline bool ReadRegStringWithView(HKEY root,
                                  const wchar_t* sub_key,
                                  const wchar_t* value_name,
                                  REGSAM view_flags,
                                  std::wstring& out) {
  HKEY hkey = nullptr;
  if (RegOpenKeyExW(root, sub_key, 0, KEY_READ | view_flags, &hkey) !=
      ERROR_SUCCESS)
    return false;
  DWORD type = 0;
  DWORD cb = 0;
  bool ok = false;
  if (RegQueryValueExW(hkey, value_name, nullptr, &type, nullptr, &cb) ==
          ERROR_SUCCESS &&
      cb > 0) {
    std::wstring buf(cb / sizeof(wchar_t) + 1, L'\0');
    cb = static_cast<DWORD>(buf.size() * sizeof(wchar_t));
    if (RegQueryValueExW(hkey, value_name, nullptr, &type,
                         reinterpret_cast<LPBYTE>(buf.data()),
                         &cb) == ERROR_SUCCESS) {
      if (type == REG_SZ || type == REG_EXPAND_SZ) {
        if (type == REG_EXPAND_SZ) {
          WCHAR expanded[MAX_PATH * 2] = {0};
          if (ExpandEnvironmentStringsW(buf.c_str(), expanded,
                                        _countof(expanded)) > 0)
            buf = expanded;
        }
        buf.resize(wcslen(buf.c_str()));
        if (!buf.empty()) {
          out = buf;
          ok = true;
        }
      }
    }
  }
  RegCloseKey(hkey);
  return ok;
}

// 两个视图都试:64 位视图优先,再 32 位视图。
// 32 位系统上 KEY_WOW64_64KEY 会失败,自然跳过。
inline bool ReadRegStringAnyView(HKEY root,
                                 const wchar_t* sub_key,
                                 const wchar_t* value_name,
                                 std::wstring& out) {
  static const REGSAM kViews[] = {KEY_WOW64_64KEY, KEY_WOW64_32KEY};
  for (const REGSAM view : kViews) {
    if (ReadRegStringWithView(root, sub_key, value_name, view, out))
      return true;
  }
  return false;
}

}  // namespace detail

// 展开候选:注册表取值 → 具体目录列表(顺序 = 优先级)。
//   WeaselRoot → [值本身]
//   InstallDir → [<值>\weasel, <值>本身]
inline std::vector<std::wstring> ExpandRootDirs(const std::wstring& value,
                                                bool append_binary_subdir) {
  std::vector<std::wstring> dirs;
  if (value.empty())
    return dirs;
  if (append_binary_subdir) {
    const std::wstring sub = BinaryRootFromInstallDir(value);
    if (!sub.empty())
      dirs.push_back(sub);
    dirs.push_back(value);
  } else {
    dirs.push_back(value);
  }
  return dirs;
}

// 从候选目录里挑第一个"像"程序目录的。
// 一个都不像时返回第一个非空候选 —— 让调用方能把路径报给用户,而不是空手而归。
inline std::wstring PickWeaselRoot(
    const std::vector<std::wstring>& candidates) {
  for (const std::wstring& c : candidates) {
    if (LooksLikeWeaselRoot(c))
      return c;
  }
  for (const std::wstring& c : candidates) {
    if (!c.empty())
      return c;
  }
  return std::wstring();
}

// 启动算法服务该执行什么。
//   优先
//   start_service.bat(与安装器随附的脚本一致);脚本缺失时退回服务可执行文件。
inline std::wstring WeaselServiceLaunchTarget(const std::wstring& root) {
  if (root.empty())
    return std::wstring();
  std::error_code ec;
  const std::filesystem::path base(root);
  if (std::filesystem::exists(base / L"start_service.bat", ec))
    return (base / L"start_service.bat").wstring();
  if (std::filesystem::exists(base / L"WeaselServer.exe", ec))
    return (base / L"WeaselServer.exe").wstring();
  return std::wstring();
}

// 注册表 → 候选目录列表(未筛选)。
inline std::vector<std::wstring> WeaselRootDirCandidates() {
  std::vector<std::wstring> dirs;
  for (const InstallRootSource& src : InstallRootSources()) {
    std::wstring value;
    if (!detail::ReadRegStringAnyView(HKEY_LOCAL_MACHINE, src.sub_key,
                                      src.value_name, value))
      continue;
    for (const std::wstring& d :
         ExpandRootDirs(value, src.append_binary_subdir))
      dirs.push_back(d);
  }
  return dirs;
}

// 端到端:解析程序目录。
inline std::wstring ResolveFluxingWeaselRoot() {
  return PickWeaselRoot(WeaselRootDirCandidates());
}

// 读 fork 的安装根(用户可见目录)—— 供诊断与单元测试使用。
inline bool ReadFluxingInstallDir(std::wstring& out) {
  return detail::ReadRegStringAnyView(
      HKEY_LOCAL_MACHINE, L"Software\\Fluxing\\Weasel", L"InstallDir", out);
}

}  // namespace fluxing
