#pragma once
// spec 095 (2026-11-01): 「快捷面板常驻」运行期状态 + 状态文件读写 —— 单一事实源
// (header-only)。
//
// 为什么单独成文件(与 include/FluxingUserPaths.h / include/FluxingCandidateIgnore.h
// 同一条理由):常驻开关收进统一设置面板后,「状态」与「落盘」要被两个模块同时
// 用到 ——
//   - QuickPanelDialog: 托盘勾选项、面板的 alpha 系数、三条自动隐藏路径的守卫;
//   - ShortcutSettings: 设置面板「界面 > 快捷面板常驻」勾选框。
// 但 test/TestShortcutSettings 只链接 ShortcutSettings.cpp + ModalChrome.cpp,
// 链不到 QuickPanelDialog.cpp ⇒ 逻辑必须下沉成共享头,生产代码与测试同源
// (constitution R7)。
//
// 不变量:
//   - 运行期状态只有一份(AlwaysShowFlagRef()),QuickPanelDialog::s_mode 已删除,
//     不存在第二个真源。
//   - 运行期初值 = false(= 面板瞬时模式,与 QuickPanelDialog::s_mode 的原初值
//     Mode::kHidden 一致)。
//   - 「状态文件缺失/损坏 ⇒ 默认常驻(true)」是**读盘兜底**规则(spec 089 / ADR
//     0022),与运行期初值是两件事,不可混。
//   - 读盘策略与 ADR 0020 的旧实现**逐条等价**(见 ResolveAlwaysShowFromFiles):
//     primary 读到行 ⇒ 用它(内容不认识 ⇒ false);primary 读不到行 ⇒ 试 legacy;
//     都读不到 ⇒ 默认值。既有 test/TestQuickPanelDialog 的 T13/T16 断言全部
//     继续成立(未改一行断言)。
//   - 写盘后必须回读自证(本仓铁律):0 字节坏文件比没文件更危险。
//   - 状态变化通过 ModeChangedCallback 观察者通知,视觉副作用(alpha 复位 /
//     重绘)由 QuickPanelDialog 注册的回调承担 —— 这样设置面板改状态时
//     面板不会停在淡显状态,又不需要把 QuickPanelDialog.cpp 拖进测试工程。
//
// 依赖面:仅 <filesystem> + <fstream> + <optional> + <string> + FluxingUserPaths.h,
// 不引入 shell32 / COM / D2D(RegOpenKey 走的是 cl.exe 默认链接的 advapi32)。

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include <FluxingUserPaths.h>

namespace fluxing {
namespace always_show {

// 状态文件名(不含目录)。
inline const wchar_t* StateFileName() {
  return L"quickpanel.state";
}

// 运行期状态。用函数内 static 而不是 inline 变量:C++11 起就成立,不必依赖
// /std:c++17,也不会有静态初始化顺序问题(首次使用时才初始化)。
//
// 跨 TU 只有一份:inline 函数的函数内 static 在本 TU 之外也共享同一对象
// (C++17 [basic.def.odr])⇒ QuickPanelDialog.cpp 与 ShortcutSettings.cpp
// 看到的是同一个开关。
inline bool& AlwaysShowFlagRef() {
  static bool flag = false;
  return flag;
}

inline bool AlwaysShowEnabled() {
  return AlwaysShowFlagRef();
}

// 直接写运行期状态(不落盘、不通知观察者)。
// 只有「从磁盘恢复」这一条路径用它 —— Restore 只回填状态,绝不 auto-show
// 面板(spec 055 不变量),也不该触发 alpha 复位 / 重绘副作用。
inline void SetAlwaysShowFlag(bool enabled) {
  AlwaysShowFlagRef() = enabled;
}

// 模式变化观察者。签名刻意保持 POD 函数指针(不是 std::function),以便 header-only
// 且不拖 <functional>。
using ModeChangedCallback = void (*)(bool enabled);

inline ModeChangedCallback& ModeChangedCallbackRef() {
  static ModeChangedCallback cb = nullptr;
  return cb;
}

inline void SetModeChangedCallback(ModeChangedCallback cb) {
  ModeChangedCallbackRef() = cb;
}

inline void NotifyModeChanged(bool enabled) {
  ModeChangedCallback cb = ModeChangedCallbackRef();
  if (cb)
    cb(enabled);
}

// 状态文件内容:单行 ASCII "always_show=0|1\n"(ADR 0019 / ADR 0020)。
inline std::string FormatStateLine(bool enabled) {
  return enabled ? "always_show=1\n" : "always_show=0\n";
}

// 解析一行状态。内容必须精确等于 "always_show=1" / "always_show=0"
// (容忍尾部 '\r' / '\n')。其它内容一律视为「不认识」。
inline std::optional<bool> ParseStateLine(const std::string& raw) {
  std::string line = raw;
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
    line.pop_back();
  if (line == "always_show=1")
    return std::optional<bool>(true);
  if (line == "always_show=0")
    return std::optional<bool>(false);
  return std::nullopt;
}

// 三态读状态文件的第一行:
//   kMissing = 路径为空 / 打不开(文件不存在)
//   kEmpty   = 打开成功但读不出行(0 字节文件)
//   kRead    = 读到一行(内容交给 ParseStateLine 判)
// 「读到但内容不认识」≠ kMissing —— 这是旧实现里 primary 优先、legacy 兜底
// 的判据,不能糊成一个 nullopt。
enum class StateReadStatus { kMissing, kEmpty, kRead };

inline StateReadStatus ReadStateLine(const std::wstring& path,
                                     std::string& out_line) {
  out_line.clear();
  if (path.empty())
    return StateReadStatus::kMissing;
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return StateReadStatus::kMissing;
  if (!std::getline(f, out_line))
    return StateReadStatus::kEmpty;
  if (!out_line.empty() && out_line.back() == '\r')
    out_line.pop_back();  // 容忍手工/CRLF 写盘
  return StateReadStatus::kRead;
}

// 读状态文件的第一行。文件缺失 / 读不出 / 内容不可识别 ⇒ nullopt。
inline std::optional<bool> ReadStateFile(const std::wstring& path) {
  std::string line;
  if (ReadStateLine(path, line) != StateReadStatus::kRead)
    return std::nullopt;
  return ParseStateLine(line);
}

// 读盘解析策略(纯函数,便于判别性测试)。
// 与 ADR 0020 的旧实现(QuickPanelDialog::LoadPersistedAlwaysShowModeFrom)
// **逐条等价**:
//   - primary 读到一行 ⇒ 用它的内容;内容不认识 ⇒ false(不回落 legacy)
//   - primary 读不到行 ⇒ 试 legacy(legacy 为空或等于 primary 则跳过)
//   - 都读不到 ⇒ default_value
inline bool ResolveAlwaysShowFromFiles(const std::wstring& primary,
                                       const std::wstring& legacy,
                                       bool default_value) {
  std::string line;
  if (ReadStateLine(primary, line) == StateReadStatus::kRead) {
    std::optional<bool> v = ParseStateLine(line);
    return v.has_value() ? *v : false;
  }
  if (!legacy.empty() && legacy != primary &&
      ReadStateLine(legacy, line) == StateReadStatus::kRead) {
    std::optional<bool> v = ParseStateLine(line);
    return v.has_value() ? *v : false;
  }
  return default_value;
}

// 写状态文件并回读自证。返回 true = 已写入且回读一致;
// false = 写失败 / 回读不一致(此时把坏文件删掉:0 字节坏文件比没文件更危险)。
inline bool WriteStateFile(const std::wstring& path, bool enabled) {
  if (path.empty())
    return false;
  const std::wstring dir = path.substr(0, path.find_last_of(L"\\/"));
  if (!dir.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
  }
  {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
      return false;
    f << FormatStateLine(enabled);
    f.flush();
    if (!f)
      return false;
  }
  auto back = ReadStateFile(path);
  if (!back.has_value()) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return false;
  }
  return *back == enabled;
}

// 默认落点:<用户数据目录>\quickpanel.state(ADR 0020)。
inline std::wstring DefaultStatePath() {
  std::filesystem::path dir = ResolveFluxingUserDataPath();
  if (dir.empty())
    return L"quickpanel.state";
  return (dir / std::filesystem::path(StateFileName())).wstring();
}

// 测试注入的状态落点。QuickPanelDialog::SetStatePath 与
// test/TestShortcutSettings 共用这一个注入点 ⇒ 两个测试工程不会互相污染
// 真实用户目录里的 quickpanel.state。
inline std::wstring& StatePathOverrideRef() {
  static std::wstring path;
  return path;
}

inline bool HasStatePathOverride() {
  return !StatePathOverrideRef().empty();
}

// 实际落点:注入优先,否则默认落点。
inline std::wstring ResolveStatePath() {
  const std::wstring& ov = StatePathOverrideRef();
  if (!ov.empty())
    return ov;
  return DefaultStatePath();
}

// 置位 + 落盘 + 通知观察者(显式落点)。返回 true = 状态真的变了
// (没变就不写盘、不通知,与 QuickPanelDialog 原语义一致)。
inline bool ApplyAlwaysShowTo(bool enabled, const std::wstring& state_path) {
  if (AlwaysShowFlagRef() == enabled)
    return false;
  AlwaysShowFlagRef() = enabled;
  WriteStateFile(state_path, enabled);
  NotifyModeChanged(enabled);
  return true;
}

// 置位 + 落盘 + 通知观察者(走 ResolveStatePath())。
inline bool ApplyAlwaysShow(bool enabled) {
  return ApplyAlwaysShowTo(enabled, ResolveStatePath());
}

}  // namespace always_show
}  // namespace fluxing
