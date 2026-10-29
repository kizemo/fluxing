#include "stdafx.h"
#include <filesystem>
#include <string>
#include <WeaselUtility.h>

// ADR 0020: 实现下沉到 include/FluxingUserPaths.h(用户数据目录解析的单一事实
// 源),与 QuickPanelDialog 的勾选态落点共用同一份解析。行为不变。
fs::path WeaselUserDataPath() {
  return ResolveFluxingUserDataPath();
}

fs::path WeaselSharedDataPath() {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append("data");
}

std::string GetCustomResource(const char* name, const char* type) {
  const HINSTANCE module = 0;  // main executable
  HRSRC hRes = FindResourceA(module, name, type);
  if (hRes) {
    HGLOBAL hData = LoadResource(module, hRes);
    if (hData) {
      const char* data = (const char*)::LockResource(hData);
      size_t size = ::SizeofResource(module, hRes);

      if (data && size) {
        if (data[size - 1] == '\0')  // null-terminated string
          size--;
        return std::string(data, size);
      }
    }
  }

  return std::string();
}
