#pragma once
//
// PhrasesDialogIPC.h — Named pipe IPC 服务端 (WeaselServer.exe 端)
//
// Phase K2 (v0.19.0.51): 创建 pipe server + 管理 FluxingPhrasesDialog.exe 子进程。
// 独立 worker thread 处理 pipe 通信，不阻塞 TSF 回调。
//
#include <string>
#include <vector>
#include <windows.h>

namespace fluxing {

struct PipePhrase;  // fwd (from FluxingPipeProtocol.h)

class PhrasesDialogIPC {
 public:
  // 启动: 创建 pipe、启动子进程、等客户端连接、发送初始短语数据
  static void Show();

  // 关闭: 发 SHUTDOWN、终止子进程、清理 pipe
  static void Hide();

  // 是否正在显示
  static bool IsVisible();

  // 设置短语数据 (外部注入，替代旧的 m_phrases)
  static void SetPhrases(const std::vector<PipePhrase>& phrases);
  static const std::vector<PipePhrase>& GetPhrases();

  // YAML 持久化路径
  static void SetYamlPath(const std::wstring& path);
  static const std::wstring& GetYamlPath();

  // YAML 读写 (从原 PhrasesDialog 搬过来)
  static bool LoadPhrasesFromYaml(const std::wstring& path,
                                  std::vector<PipePhrase>& out);
  static bool SavePhrasesToYaml(const std::wstring& path,
                                const std::vector<PipePhrase>& data);

  // 文本注入 (SendInput, 从原 PhrasesDialog 搬过来)
  static void InjectText(const std::wstring& text);

 private:
  // Pipe worker thread (静态函数，不阻塞 TSF)
  static DWORD WINAPI PipeThreadProc(LPVOID param);

  // 发送全量短语给客户端
  static bool SendPhrasesToClient(HANDLE hPipe);

  // 处理一条客户端命令，返回 ACK 或 ERR
  static void ProcessCommand(HANDLE hPipe, const std::string& utf8Json);

  // 子进程管理
  static bool LaunchPhrasesDialog(const std::wstring& pipeName);
  static void TerminatePhrasesDialog();

  // ===== 状态 =====
  static HANDLE s_hPipe;
  static HANDLE s_hThread;
  static HANDLE s_hProcess;       // FluxingPhrasesDialog.exe 进程句柄
  static DWORD s_pid;             // 本进程 PID (pipe name 后缀)
  static volatile bool s_running; // worker thread 运行标志
  static std::vector<PipePhrase> s_phrases;
  static std::wstring s_yamlPath;
};

}  // namespace fluxing
