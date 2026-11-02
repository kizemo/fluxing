//
// FluxingTextInjector.h — 常用短语上屏: 剪贴板 + 粘贴快捷键 (spec 097 诉求 2)
//
// Why:
//   逐字 SendInput 的每个字符都是一个 VK_PACKET 键事件, 逐个穿过前台应用的
//   IME/TSF 栈 (Fluxing 作为 IME 参与 ⇒ 每字一次 WeaselIPC 往返 + Rime 处理)。
//   毫秒探针实测: 103 字 ⇒ 206 个输入事件 ⇒ 服务端 SendInput 内部耗时
//   625 ms (双击路径) / 703 ms (回车路径), ≈3.1 ms/事件; 而客户端隐藏链
//   16-32 ms、管道往返 0 ms、ACK 往返 0 ms 全部可忽略 ⇒ 延时 100% 在逐字注入。
//
// How:
//   一次性把短语放进剪贴板, 只发 4 个按键事件 (Ctrl+V), 由宿主程序自己粘贴
//   ⇒ 只剩 1 个按键穿过 IME 栈, 延时降到 1 个按键的量级。
//
//   Ctrl+V 不与 Rime 冲突:
//   - output/data/default.yaml 里 Control+v 只出现在注释, 没有实际绑定;
//   - Rime 对未绑定按键返回 handled=false ⇒ WeaselTSF 不 eat 该键
//     (WeaselTSF/KeyEventSink.cpp:101 `*pfEaten = (BOOL)m_client.ProcessKeyEvent(ke)`
//      + RimeWithWeasel/RimeWithWeasel.cpp:329 `handled = rime_api->process_key(...)`)
//     ⇒ 按键照常抵达宿主程序, 由宿主程序读剪贴板完成粘贴。
//
// 降级:
//   剪贴板被别的进程独占 (OpenClipboard 失败) ⇒ 退回旧的逐字 SendInput
//   (慢, 但一定能上屏)。
//
// 剪贴板恢复:
//   粘贴后延迟 kClipboardRestoreDelayMs; 若剪贴板内容仍是我们放进去的短语
//   ⇒ 恢复原文本 (只在原剪贴板本来就有文本时恢复)。原剪贴板没有文本内容时
//   不清空 (宁可留着短语, 也不给用户一个空剪贴板); 非文本格式无法恢复
//   (已知限制, CHANGELOG 已声明)。
//
// 用户可见行为变化 (CHANGELOG):
//   短语上屏由「逐字敲键盘」改为「粘贴」, 换行符现在能正确进多行编辑框
//   (逐字 KEYEVENTF_UNICODE 的 \n 会被编辑框忽略)。
//
#pragma once

#ifndef _WINDOWS_
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace fluxing {

// ===== 参数 =====
constexpr int kClipboardOpenRetry = 5;          // OpenClipboard 重试次数
constexpr int kClipboardOpenRetryDelayMs = 40;   // 每次重试间隔
constexpr DWORD kClipboardRestoreDelayMs = 2000; // 剪贴板恢复延迟

// 打开剪贴板 (带重试)。成功返回 true, 调用方负责 CloseClipboard。
inline bool OpenClipboardWithRetry() {
  for (int i = 0; i < kClipboardOpenRetry; ++i) {
    if (OpenClipboard(nullptr))
      return true;
    Sleep(kClipboardOpenRetryDelayMs);
  }
  return false;
}

// 读剪贴板里的 CF_UNICODETEXT (调用前必须已 OpenClipboard)。
// had 输出该格式是否存在 (区分「没有文本」与「文本为空」)。
inline std::wstring ReadClipboardText(bool* had) {
  *had = false;
  HANDLE h = GetClipboardData(CF_UNICODETEXT);
  if (!h)
    return std::wstring();
  const wchar_t* p = static_cast<const wchar_t*>(GlobalLock(h));
  if (!p)
    return std::wstring();
  std::wstring s(p);
  GlobalUnlock(h);
  *had = true;
  return s;
}

// 写文本到剪贴板 (调用前必须已 OpenClipboard 且已 EmptyClipboard)。
inline bool WriteClipboardText(const std::wstring& text) {
  const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (!h)
    return false;
  void* p = GlobalLock(h);
  if (!p) {
    GlobalFree(h);
    return false;
  }
  memcpy(p, text.c_str(), bytes);
  GlobalUnlock(h);
  if (!SetClipboardData(CF_UNICODETEXT, h)) {
    GlobalFree(h);
    return false;
  }
  return true;
}

// 发 Ctrl+V (4 个按键事件: Ctrl down / V down / V up / Ctrl up)。
inline bool SendPasteShortcut() {
  INPUT inputs[4] = {};
  inputs[0].type = INPUT_KEYBOARD;
  inputs[0].ki.wVk = VK_CONTROL;
  inputs[1].type = INPUT_KEYBOARD;
  inputs[1].ki.wVk = L'V';
  inputs[2] = inputs[1];
  inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
  inputs[3] = inputs[0];
  inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
  return SendInput(4, inputs, sizeof(INPUT)) == 4;
}

// 旧的逐字注入 (降级路径): 每字 2 个 KEYEVENTF_UNICODE 事件。
inline void SendTextByUnicode(const std::wstring& text) {
  if (text.empty())
    return;
  std::vector<INPUT> inputs;
  inputs.reserve(text.size() * 2);
  for (wchar_t c : text) {
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wScan = c;
    down.ki.dwFlags = KEYEVENTF_UNICODE;
    inputs.push_back(down);

    INPUT up = down;
    up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    inputs.push_back(up);
  }
  SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

// 剪贴板恢复任务 (延迟线程参数)。
struct ClipboardRestoreTask {
  std::wstring injected;    // 我们放进去的短语
  std::wstring original;    // 原剪贴板文本 (可能为空)
  bool hadOriginal = false; // 原剪贴板本来就有文本吗
};

// 若剪贴板内容仍是我们放进去的短语 ⇒ 恢复原文本。
inline void RestoreClipboardIfUnchanged(const ClipboardRestoreTask& task) {
  if (!OpenClipboardWithRetry())
    return;
  bool nowHad = false;
  std::wstring now = ReadClipboardText(&nowHad);
  if (now == task.injected && task.hadOriginal) {
    EmptyClipboard();
    WriteClipboardText(task.original);
  }
  CloseClipboard();
}

inline DWORD WINAPI ClipboardRestoreThread(LPVOID param) {
  std::unique_ptr<ClipboardRestoreTask> task(
      static_cast<ClipboardRestoreTask*>(param));
  Sleep(kClipboardRestoreDelayMs);
  if (task)
    RestoreClipboardIfUnchanged(*task);
  return 0;
}

// 主路径: 剪贴板 + 粘贴快捷键。成功返回 true。
inline bool InjectTextViaClipboard(const std::wstring& text) {
  if (text.empty())
    return true;

  ClipboardRestoreTask task;
  task.injected = text;
  if (!OpenClipboardWithRetry())
    return false;  // 剪贴板被别人独占 ⇒ 降级
  task.original = ReadClipboardText(&task.hadOriginal);
  if (!EmptyClipboard()) {
    CloseClipboard();
    return false;
  }
  if (!WriteClipboardText(text)) {
    CloseClipboard();
    return false;
  }
  CloseClipboard();

  if (!SendPasteShortcut()) {
    // 粘贴键没发出去 ⇒ 剪贴板已被我们改写, 立刻恢复, 再让调用方降级
    RestoreClipboardIfUnchanged(task);
    return false;
  }

  // 延迟恢复剪贴板 (线程由注入方进程承载; 进程先退 ⇒ 剪贴板留着短语)
  ClipboardRestoreTask* heap = new ClipboardRestoreTask(task);
  HANDLE h = CreateThread(nullptr, 0, &ClipboardRestoreThread, heap, 0, nullptr);
  if (h)
    CloseHandle(h);
  else
    delete heap;
  return true;
}

// 统一入口: 优先剪贴板粘贴, 剪贴板不可用时降级逐字注入。
// 返回 true = 走了剪贴板路径; false = 降级逐字注入。
inline bool InjectTextWithFallback(const std::wstring& text) {
  if (text.empty())
    return true;
  if (InjectTextViaClipboard(text))
    return true;
  SendTextByUnicode(text);
  return false;
}

}  // namespace fluxing
