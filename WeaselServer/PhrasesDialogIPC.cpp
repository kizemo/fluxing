//
// PhrasesDialogIPC.cpp — Named pipe IPC 服务端实现
//
// Phase K2 (v0.19.0.51): 替代 in-process PhrasesDialog 的 IPC 后端。
// 独立 worker thread 处理 pipe I/O，CreateProcess 管理子进程生命周期。
//
#include "stdafx.h"
#include "PhrasesDialogIPC.h"
#include "FluxingPipeProtocol.h"
#include "ForegroundCapture.h"  // v0.19.0.52 (Phase K3 T011 Option B): InjectText 抢 foreground
#include <WeaselUtility.h>      // WeaselUserDataPath (懒初始化 yaml path 用)
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fluxing {

// ===== 静态状态 =====
HANDLE PhrasesDialogIPC::s_hPipe = INVALID_HANDLE_VALUE;
HANDLE PhrasesDialogIPC::s_hThread = nullptr;
HANDLE PhrasesDialogIPC::s_hProcess = nullptr;
DWORD PhrasesDialogIPC::s_pid = 0;
volatile bool PhrasesDialogIPC::s_running = false;
std::vector<PipePhrase> PhrasesDialogIPC::s_phrases;
std::wstring PhrasesDialogIPC::s_yamlPath;

// ===== 公开 API =====

void PhrasesDialogIPC::Show() {
  if (s_running) return;  // 已经显示

  // v0.19.0.52 (Phase K3 T011 Option B): 惰性初始化 yaml path,兜底消除
  //   "hotkey fires before SetYamlPath" race。WeaselServerApp 在 Run() 入口
  //   显式 SetYamlPath 是 override 的语义,here 是 fallback (无 Set 时用 default)。
  //   同时也意味着 TestPhrasesDialog 之类不依赖外部 init 也能跑。
  if (s_yamlPath.empty()) {
    std::filesystem::path yamlDefault =
        WeaselUserDataPath().wstring() + L"\\phrases.yaml";
    s_yamlPath = yamlDefault.wstring();
  }

  // 1. 从 YAML 加载短语数据
  if (!s_yamlPath.empty()) {
    std::vector<PipePhrase> loaded;
    if (LoadPhrasesFromYaml(s_yamlPath, loaded)) {
      s_phrases = std::move(loaded);
    }
  }

  // 如果没有加载到数据，放一条默认的
  if (s_phrases.empty()) {
    s_phrases.push_back({L"\x4f60\x597d", L""});  // 你好
  }

  s_pid = GetCurrentProcessId();
  std::wstring pipeName = MakePipeName(s_pid);

  // 2. 创建 named pipe
  s_hPipe = CreateNamedPipeW(
      pipeName.c_str(),
      PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      1,                          // 最多 1 个实例
      4096,                       // 输出缓冲
      4096,                       // 输入缓冲
      0,                          // 默认超时
      nullptr);                   // 默认安全属性

  if (s_hPipe == INVALID_HANDLE_VALUE) {
    OutputDebugStringW((L"[PhrasesDialogIPC] CreateNamedPipe failed, err=" +
                         std::to_wstring(GetLastError()) + L"\n").c_str());
    return;
  }

  OutputDebugStringW((L"[PhrasesDialogIPC] Pipe created: " + pipeName + L"\n").c_str());

  // 3. 启动子进程
  if (!LaunchPhrasesDialog(pipeName)) {
    OutputDebugStringW(L"[PhrasesDialogIPC] LaunchPhrasesDialog failed\n");
    CloseHandle(s_hPipe);
    s_hPipe = INVALID_HANDLE_VALUE;
    return;
  }

  // 4. 启动 worker thread
  s_running = true;
  s_hThread = CreateThread(nullptr, 0, PipeThreadProc, nullptr, 0, nullptr);
  if (!s_hThread) {
    OutputDebugStringW(L"[PhrasesDialogIPC] CreateThread failed\n");
    s_running = false;
    TerminatePhrasesDialog();
    CloseHandle(s_hPipe);
    s_hPipe = INVALID_HANDLE_VALUE;
    return;
  }
}

void PhrasesDialogIPC::Hide() {
  if (!s_running) return;

  // 发 SHUTDOWN (如果 pipe 已连接)
  if (s_hPipe != INVALID_HANDLE_VALUE) {
    PipeSend(s_hPipe, BuildSHUTDOWN());
  }

  // 停止 worker thread
  s_running = false;

  // 终止子进程
  TerminatePhrasesDialog();

  // 清理 thread
  if (s_hThread) {
    WaitForSingleObject(s_hThread, 3000);
    CloseHandle(s_hThread);
    s_hThread = nullptr;
  }

  // 清理 pipe
  if (s_hPipe != INVALID_HANDLE_VALUE) {
    DisconnectNamedPipe(s_hPipe);
    CloseHandle(s_hPipe);
    s_hPipe = INVALID_HANDLE_VALUE;
  }
}

bool PhrasesDialogIPC::IsVisible() {
  return s_running;
}

void PhrasesDialogIPC::SetPhrases(const std::vector<PipePhrase>& phrases) {
  s_phrases = phrases;
}

const std::vector<PipePhrase>& PhrasesDialogIPC::GetPhrases() {
  return s_phrases;
}

void PhrasesDialogIPC::SetYamlPath(const std::wstring& path) {
  s_yamlPath = path;
}

const std::wstring& PhrasesDialogIPC::GetYamlPath() {
  return s_yamlPath;
}

// ===== Worker Thread =====

DWORD WINAPI PhrasesDialogIPC::PipeThreadProc(LPVOID /*param*/) {
  OutputDebugStringW(L"[PhrasesDialogIPC] Worker thread started\n");

  // 等待客户端连接 (阻塞)
  BOOL connected = ConnectNamedPipe(s_hPipe, nullptr);
  if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
    OutputDebugStringW((L"[PhrasesDialogIPC] ConnectNamedPipe failed, err=" +
                         std::to_wstring(GetLastError()) + L"\n").c_str());
    s_running = false;
    return 1;
  }

  OutputDebugStringW(L"[PhrasesDialogIPC] Client connected\n");

  // 发送初始短语数据
  if (!SendPhrasesToClient(s_hPipe)) {
    OutputDebugStringW(L"[PhrasesDialogIPC] SendPhrasesToClient failed\n");
    s_running = false;
    return 2;
  }

  // 命令处理循环
  while (s_running) {
    std::string json = PipeRecv(s_hPipe, 4096);
    if (json.empty()) {
      // 客户端断开
      OutputDebugStringW(L"[PhrasesDialogIPC] Client disconnected\n");
      break;
    }
    ProcessCommand(s_hPipe, json);
  }

  // 清理
  // v0.19.0.55 (Phase K3 T019 Bug 2 真修): worker exit path 必须
  //   DisconnectNamedPipe + CloseHandle + reset s_hPipe = INVALID。
  //   原版本只 DisconnectNamedPipe 但不 CloseHandle — Windows 命名管道
  //   句柄存在期间 pipe NAME 被占用, 下次 PhrasesDialogIPC::Show() 调
  //   CreateNamedPipeW 同名 → ERROR_ACCESS_DENIED (or ERROR_PIPE_BUSY)
  //   → Show() 静默 early return → user 反馈 "按 Alt+. 无反应"。
  DisconnectNamedPipe(s_hPipe);
  CloseHandle(s_hPipe);
  s_hPipe = INVALID_HANDLE_VALUE;
  s_running = false;
  OutputDebugStringW(L"[PhrasesDialogIPC] Worker thread exiting\n");
  return 0;
}

bool PhrasesDialogIPC::SendPhrasesToClient(HANDLE hPipe) {
  std::string json = BuildPHRASES(s_phrases);
  return PipeSend(hPipe, json);
}

void PhrasesDialogIPC::ProcessCommand(HANDLE hPipe,
                                       const std::string& utf8Json) {
  PipeMessage msg = ParseMessage(utf8Json);

  switch (msg.type) {
    case PipeMsgType::MT_ADD: {
      if (!msg.text.empty()) {
        PipePhrase p;
        p.text = msg.text;
        s_phrases.push_back(p);
        if (!s_yamlPath.empty()) {
          SavePhrasesToYaml(s_yamlPath, s_phrases);
        }
        // ACK = 全量 PHRASES 推送
        SendPhrasesToClient(hPipe);
      } else {
        PipeSend(hPipe, BuildERR(L"Empty text"));
      }
      break;
    }
    case PipeMsgType::MT_EDIT: {
      if (msg.id >= 0 && msg.id < (int)s_phrases.size() && !msg.text.empty()) {
        s_phrases[msg.id].text = msg.text;
        if (!s_yamlPath.empty()) {
          SavePhrasesToYaml(s_yamlPath, s_phrases);
        }
        SendPhrasesToClient(hPipe);  // ACK = 全量 PHRASES
      } else {
        PipeSend(hPipe, BuildERR(L"Invalid id or empty text"));
      }
      break;
    }
    case PipeMsgType::MT_DELETE: {
      if (msg.id >= 0 && msg.id < (int)s_phrases.size()) {
        s_phrases.erase(s_phrases.begin() + msg.id);
        if (!s_yamlPath.empty()) {
          SavePhrasesToYaml(s_yamlPath, s_phrases);
        }
        SendPhrasesToClient(hPipe);  // ACK = 全量 PHRASES
      } else {
        PipeSend(hPipe, BuildERR(L"Invalid id"));
      }
      break;
    }
    case PipeMsgType::MT_INJECT: {
      if (msg.id >= 0 && msg.id < (int)s_phrases.size()) {
        const std::wstring& text = s_phrases[msg.id].text;
        // v0.19.0.55 (Phase K3 T019 Bug 1 简化): 删 AttachThreadInput
        //   强抢 foreground。原 v0.19.0.52 T011-B Option B 假设
        //   SetForegroundWindow + AttachThreadInput 能从 cluster 抢到
        //   user app foreground — Win11 22H2+ foreground 限制极严,
        //   实测仍可能拒, 结果 SendInput 命中 dialog s_hInput 而非
        //   user app。
        // 修法: client NM_DBLCLK / NM_RETURN / VK_RETURN 全部 "Hide 先
        //   于 INJECT" (Bug 3 修) — dialog DestroyWindow 后 foreground
        //   自动归位 user app, server 端直接 InjectText 即可。
        //   Option B 仅作 best-effort fallback: 如果 cached target
        //   HWND 仍 IsWindow, 试图 SetForegroundWindow 抢回 (某些机器
        //   上 work), 否则直接 InjectText 走当前 foreground (Hide 后
        //   = user app)。
        HWND target = fluxing::foreground_restore::GetHwnd();
        if (target && IsWindow(target)) {
          // best-effort: 不 attach thread input (Win11 限制), 只
          // 尝试 OS-level foreground restore; 失败也不 abort (Hide
          // 已让 foreground = user app, 直接 SendInput 即可)。
          SetForegroundWindow(target);
        }
        InjectText(text);
      }
      PipeSend(hPipe, BuildACK());
      break;
    }
    case PipeMsgType::MT_SHUTDOWN: {
      PipeSend(hPipe, BuildACK());
      s_running = false;
      break;
    }
    default: {
      PipeSend(hPipe, BuildERR(L"Unknown message type"));
      break;
    }
  }
}

// ===== 子进程管理 =====

bool PhrasesDialogIPC::LaunchPhrasesDialog(const std::wstring& pipeName) {
  // 获取 WeaselServer.exe 所在目录 (FluxingPhrasesDialog.exe 在同一目录)
  wchar_t exePath[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exePath, MAX_PATH);
  std::wstring dir = exePath;
  size_t lastSlash = dir.find_last_of(L"\\/");
  if (lastSlash != std::wstring::npos) {
    dir = dir.substr(0, lastSlash);
  }
  std::wstring phrasesExe = dir + L"\\FluxingPhrasesDialog.exe";

  // 检查 exe 是否存在
  if (!std::filesystem::exists(phrasesExe)) {
    OutputDebugStringW((L"[PhrasesDialogIPC] Exe not found: " + phrasesExe + L"\n").c_str());
    return false;
  }

  // 命令行: FluxingPhrasesDialog.exe --pipe=\\.\pipe\FluxingPhrasesDialog\{pid}
  std::wstring cmdLine = L"\"" + phrasesExe + L"\" --pipe=" + pipeName;

  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;  // FluxingPhrasesDialog 自己会 Show window

  PROCESS_INFORMATION pi = {};
  BOOL ok = CreateProcessW(phrasesExe.c_str(), cmdLine.data(),
                           nullptr, nullptr, FALSE,
                           0, nullptr, nullptr, &si, &pi);
  if (!ok) {
    OutputDebugStringW((L"[PhrasesDialogIPC] CreateProcess failed, err=" +
                         std::to_wstring(GetLastError()) + L"\n").c_str());
    return false;
  }

  s_hProcess = pi.hProcess;
  CloseHandle(pi.hThread);

  OutputDebugStringW((L"[PhrasesDialogIPC] Launched FluxingPhrasesDialog.exe PID=" +
                       std::to_wstring(pi.dwProcessId) + L"\n").c_str());
  return true;
}

void PhrasesDialogIPC::TerminatePhrasesDialog() {
  if (s_hProcess) {
    // 优雅退出: 等 2 秒
    DWORD waitResult = WaitForSingleObject(s_hProcess, 2000);
    if (waitResult == WAIT_TIMEOUT) {
      TerminateProcess(s_hProcess, 0);
    }
    CloseHandle(s_hProcess);
    s_hProcess = nullptr;
  }
}

// ===== 文本注入 =====

void PhrasesDialogIPC::InjectText(const std::wstring& text) {
  if (text.empty()) return;
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

// ===== YAML I/O (从 WeaselServer/PhrasesDialog.cpp 搬过来) =====

namespace {

std::wstring YamlTrim(const std::wstring& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == L' ' || s[a] == L'\t' || s[a] == L'\r'))
    ++a;
  while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t' || s[b - 1] == L'\r'))
    --b;
  return s.substr(a, b - a);
}

std::wstring YamlUnquote(const std::wstring& s) {
  std::wstring t = YamlTrim(s);
  if (t.size() >= 2 && t.front() == L'"' && t.back() == L'"') {
    return t.substr(1, t.size() - 2);
  }
  if (t.size() >= 2 && t.front() == L'\'' && t.back() == L'\'') {
    return t.substr(1, t.size() - 2);
  }
  return t;
}

}  // namespace

bool PhrasesDialogIPC::LoadPhrasesFromYaml(const std::wstring& path,
                                            std::vector<PipePhrase>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;

  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
      (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
    content = content.substr(3);
  }

  std::wstring wtext;
  if (!content.empty()) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                                    (int)content.size(), nullptr, 0);
    if (wlen > 0) {
      wtext.resize(wlen);
      MultiByteToWideChar(CP_UTF8, 0, content.c_str(),
                          (int)content.size(), &wtext[0], wlen);
    }
  }

  out.clear();
  size_t pos = 0;
  PipePhrase cur;
  bool inPhrase = false;
  while (pos <= wtext.size()) {
    size_t eol = wtext.find(L'\n', pos);
    if (eol == std::wstring::npos) eol = wtext.size();
    std::wstring line = wtext.substr(pos, eol - pos);
    if (!line.empty() && line.back() == L'\r') line.pop_back();
    pos = eol + 1;

    auto trimmed = YamlTrim(line);
    if (trimmed.empty() || trimmed[0] == L'#') continue;

    if (trimmed.size() >= 2 && trimmed[0] == L'-' &&
        (trimmed[1] == L' ' || trimmed[1] == L'\t')) {
      if (inPhrase) out.push_back(cur);
      cur = PipePhrase();
      inPhrase = true;

      auto rest = trimmed.substr(2);
      auto trimmedRest = YamlTrim(rest);
      if (trimmedRest.size() >= 9 &&
          trimmedRest.substr(0, 9) == L"category:") {
        cur.category = YamlUnquote(trimmedRest.substr(9));
      } else if (trimmedRest.size() >= 5 &&
                 trimmedRest.substr(0, 5) == L"text:") {
        cur.text = YamlUnquote(trimmedRest.substr(5));
      }
    } else if (inPhrase) {
      auto trimmedFull = YamlTrim(line);
      if (trimmedFull.size() >= 9 &&
          trimmedFull.substr(0, 9) == L"category:") {
        cur.category = YamlUnquote(trimmedFull.substr(9));
      } else if (trimmedFull.size() >= 5 &&
                 trimmedFull.substr(0, 5) == L"text:") {
        cur.text = YamlUnquote(trimmedFull.substr(5));
      } else if (!trimmedFull.empty() && trimmedFull[0] != L'-' &&
                 trimmedFull[0] != L'#') {
        cur.text = trimmedFull;
      }
    }
    if (pos > wtext.size()) break;
  }
  if (inPhrase) out.push_back(cur);
  return true;
}

bool PhrasesDialogIPC::SavePhrasesToYaml(const std::wstring& path,
                                          const std::vector<PipePhrase>& data) {
  std::string utf8;
  utf8 += "\xEF\xBB\xBF";  // UTF-8 BOM

  auto appendLine = [&utf8](const std::wstring& wline) {
    int len = WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                                  (int)wline.size(), nullptr, 0,
                                  nullptr, nullptr);
    if (len > 0) {
      std::string buf(len, 0);
      WideCharToMultiByte(CP_UTF8, 0, wline.c_str(),
                          (int)wline.size(), &buf[0], len, nullptr, nullptr);
      utf8 += buf;
    }
    utf8 += "\r\n";
  };

  for (const auto& p : data) {
    if (!p.category.empty()) {
      appendLine(L"- category: \"" + p.category + L"\"");
      appendLine(L"  text: \"" + p.text + L"\"");
    } else if (!p.text.empty()) {
      appendLine(L"- text: \"" + p.text + L"\"");
    }
  }

  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(utf8.c_str(), utf8.size());
  f.close();
  return f.good();
}

}  // namespace fluxing
