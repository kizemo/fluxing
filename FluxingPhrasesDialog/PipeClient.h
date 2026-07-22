#pragma once
//
// PipeClient.h — Named pipe 客户端 (FluxingPhrasesDialog.exe 端)
//
// Phase K2 (v0.19.0.51): 连接 WeaselServer 的 pipe server，
// 接收短语数据 + 发送操作指令。
//
#include <string>
#include <vector>
#include <windows.h>

namespace fluxing {

class PipeClient {
 public:
  PipeClient();
  ~PipeClient();

  // 连接到 pipe server (带重试)
  // pipeName: 完整 pipe 路径, 如 \\.\pipe\FluxingPhrasesDialog\12345
  bool Connect(const std::wstring& pipeName);

  // 断开连接
  void Disconnect();

  // 是否已连接
  bool IsConnected() const;

  // 阻塞读取一条消息 (返回 UTF-8 JSON)
  std::string ReadMessage();

  // 发送一条消息 (UTF-8 JSON)
  bool SendMessage(const std::string& json);

  // 发送命令并等 ACK (超时 5s)
  bool SendCommand(const std::string& json);

 private:
  HANDLE m_hPipe;
  std::wstring m_pipeName;
};

}  // namespace fluxing
