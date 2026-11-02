#pragma once
// spec 097: 快捷设置栏显隐链路的诊断日志。
//
// 为什么需要它:「切到其他输入法时快捷设置栏不消失」这条链路上横跨三个进程
// (TSF 前端 / 命名管道 / 服务端),任何一环丢消息都不会报错 —— 症状只有
// 「面板还在」。glog 的日志会轮转消失,服务端 DLOG 又被 include/no_logging.h
// 编译成空操作,所以这里另开一个 append-only 的小文件,专门记录
// 「意图 -> 送达 -> 判定 -> 执行」四段。
//
// 约束:
//   - 只在**非 TSF 回调**的线程里调用(前端通知线程 / 服务端 IPC 线程),
//     绝不在 ITfTextInputProcessorEx::* 回调里写盘(宪法 P2)。
//   - 写失败一律静默忽略:诊断通道永远不能成为故障源。
//   - 单次 WriteFile 写整行 => 行级原子,多进程并发写不会互相串行。
//   - 超过大小上限后停止追加(不轮转、不删除 —— 诊断文件要留到人工取证)。
#ifndef FLUXING_DIAG_LOG_H
#define FLUXING_DIAG_LOG_H

#include <windows.h>
#include <string>

namespace fluxing {
namespace diag {

inline std::wstring DiagLogPath() {
  wchar_t buf[MAX_PATH] = {0};
  DWORD n = ::GetTempPathW(MAX_PATH, buf);
  if (n == 0 || n >= MAX_PATH)
    return std::wstring();
  std::wstring path(buf, n);
  path += L"fluxing-profile-notify.log";
  return path;
}

// 上限 2 MB。超过就停止追加。
inline bool DiagLogShouldWrite() {
  const std::wstring path = DiagLogPath();
  if (path.empty())
    return false;
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr))
    return true;  // 文件不存在 => 可以创建
  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  return size.QuadPart < (2ull * 1024 * 1024);
}

// 追加一行。tag 区分来源(tsf / server)。
inline void AppendLine(const char* tag, const std::string& message) {
  try {
    if (!DiagLogShouldWrite())
      return;
    const std::wstring path = DiagLogPath();
    if (path.empty())
      return;
    HANDLE h = ::CreateFileW(path.c_str(), FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
      return;
    std::string line("[");
    line += tag;
    line += "] ";
    line += message;
    line.push_back('\n');
    DWORD written = 0;
    ::WriteFile(h, line.data(), (DWORD)line.size(), &written, NULL);
    ::CloseHandle(h);
  } catch (...) {
    // 诊断通道永远不能成为故障源。
  }
}

}  // namespace diag
}  // namespace fluxing

#endif  // FLUXING_DIAG_LOG_H
