// WeaselServer/ServerCmdLine.h
//
// spec 093 T001 - WeaselServer 命令行选项解析（共享纯函数，宪法 R7）。
//
// 缺陷背景（L160）
// ----------------
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
// 为什么抽成共享头
// ----------------
//   单元测试覆盖不到 WeaselServer.cpp 的调用点；如果只在测试里重写一份解析
//   逻辑，生产与测试会各自漂移。宪法 R7 要求生产代码与测试**同源**，因此把
//   解析收敛成本头文件里的纯函数：
//     生产侧 WeaselServer.cpp 调用 fluxing::servercmd::Parse()
//     测试侧 test/TestServerCmdLine 调用同一个 Parse()
//   解析规则只存在一份，改一处即改两处。
//
// 纯函数：无副作用、不依赖 ATL / Windows API，可在任意测试工程中编译。

#ifndef FLUXING_SERVER_CMDLINE_H_
#define FLUXING_SERVER_CMDLINE_H_

#include <cwchar>

namespace fluxing {
namespace servercmd {

// WeaselServer.exe 支持的命令行选项。
enum class Option {
  None,      // 无参数 / 未知选项
  UserDir,   // /userdir    打开用户数据目录
  WeaselDir, // /weaseldir  打开安装目录
  Ascii,     // /ascii      开启 ASCII 模式
  Nascii,    // /nascii     关闭 ASCII 模式
  Quit,      // /q 或 /quit 停止正在运行的服务
  Update,    // /update     检查更新
};

namespace detail {

inline bool IsSpace(wchar_t c) {
  return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\v' ||
         c == L'\f';
}

// 归一化一段文本：去首尾空白、剥掉成对的双引号。
// 返回写入 out 的字符数（不含结尾 '\0'）；outLen 为 0 时返回 0。
inline unsigned Normalize(const wchar_t* in, wchar_t* out, unsigned outLen) {
  if (out == 0 || outLen == 0) {
    return 0;
  }
  out[0] = L'\0';
  if (in == 0) {
    return 0;
  }
  const wchar_t* b = in;
  while (*b != 0 && IsSpace(*b)) {
    ++b;
  }
  const wchar_t* e = b;
  while (*e != 0) {
    ++e;
  }
  while (e > b && IsSpace(e[-1])) {
    --e;
  }
  unsigned n = static_cast<unsigned>(e - b);
  // 剥掉成对的双引号（命令行里 "/quit" 这种写法）。
  if (n >= 2 && b[0] == L'"' && b[n - 1] == L'"') {
    ++b;
    n -= 2;
    while (n > 0 && IsSpace(*b)) {
      ++b;
      --n;
    }
    while (n > 0 && IsSpace(b[n - 1])) {
      --n;
    }
  }
  if (n > outLen - 1) {
    n = outLen - 1;
  }
  for (unsigned i = 0; i < n; ++i) {
    out[i] = b[i];
  }
  out[n] = L'\0';
  return n;
}

}  // namespace detail

// 取首个记号（以空白分隔）。没有记号时 out[0] = L'\0' 并返回 false。
inline bool FirstToken(const wchar_t* in, wchar_t* out, unsigned outLen) {
  if (out == 0 || outLen == 0) {
    return false;
  }
  out[0] = L'\0';
  wchar_t buf[256];
  if (detail::Normalize(in, buf, 256) == 0) {
    return false;
  }
  unsigned n = 0;
  while (buf[n] != 0 && !detail::IsSpace(buf[n])) {
    ++n;
  }
  if (n > outLen - 1) {
    n = outLen - 1;
  }
  for (unsigned i = 0; i < n; ++i) {
    out[i] = buf[i];
  }
  out[n] = L'\0';
  return n > 0;
}

// 解析 WinMain 传入的原始 lpCmdLine，返回识别出的选项。
// 未识别的输入一律返回 Option::None（调用方按"无参数"处理）。
inline Option Parse(const wchar_t* rawCmdLine) {
  wchar_t token[64];
  if (!FirstToken(rawCmdLine, token, 64)) {
    return Option::None;
  }
  if (std::wcscmp(token, L"/userdir") == 0) {
    return Option::UserDir;
  }
  if (std::wcscmp(token, L"/weaseldir") == 0) {
    return Option::WeaselDir;
  }
  if (std::wcscmp(token, L"/ascii") == 0) {
    return Option::Ascii;
  }
  if (std::wcscmp(token, L"/nascii") == 0) {
    return Option::Nascii;
  }
  if (std::wcscmp(token, L"/q") == 0 || std::wcscmp(token, L"/quit") == 0) {
    return Option::Quit;
  }
  if (std::wcscmp(token, L"/update") == 0) {
    return Option::Update;
  }
  return Option::None;
}

}  // namespace servercmd
}  // namespace fluxing

#endif  // FLUXING_SERVER_CMDLINE_H_
