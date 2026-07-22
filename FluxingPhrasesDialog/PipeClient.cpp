//
// PipeClient.cpp — Named pipe 客户端实现
//
#include "stdafx.h"
#include "PipeClient.h"
#include "FluxingPipeProtocol.h"

namespace fluxing {

static constexpr int kMaxRetries = 3;
static constexpr DWORD kRetryDelayMs = 1000;
static constexpr DWORD kConnectTimeoutMs = 5000;
static constexpr DWORD kPipeBufSize = 4096;

PipeClient::PipeClient() : m_hPipe(INVALID_HANDLE_VALUE) {}

PipeClient::~PipeClient() {
  Disconnect();
}

bool PipeClient::Connect(const std::wstring& pipeName) {
  m_pipeName = pipeName;

  for (int retry = 0; retry < kMaxRetries; ++retry) {
    // 等待 pipe 可用
    if (!WaitNamedPipeW(pipeName.c_str(), kConnectTimeoutMs)) {
      DWORD err = GetLastError();
      if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PIPE_BUSY) {
        if (retry < kMaxRetries - 1) {
          Sleep(kRetryDelayMs);
          continue;
        }
      }
      OutputDebugStringW((L"[PipeClient] WaitNamedPipe failed, err=" +
                           std::to_wstring(err) + L"\n").c_str());
      return false;
    }

    // 连接
    m_hPipe = CreateFileW(pipeName.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,           // 独占
                          nullptr,     // 默认安全属性
                          OPEN_EXISTING,
                          0,           // 同步 I/O
                          nullptr);
    if (m_hPipe != INVALID_HANDLE_VALUE) {
      // 设为消息模式
      DWORD mode = PIPE_READMODE_MESSAGE;
      if (!SetNamedPipeHandleState(m_hPipe, &mode, nullptr, nullptr)) {
        OutputDebugStringW((L"[PipeClient] SetNamedPipeHandleState failed, err=" +
                             std::to_wstring(GetLastError()) + L"\n").c_str());
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
        return false;
      }
      OutputDebugStringW((L"[PipeClient] Connected to " + pipeName + L"\n").c_str());
      return true;
    }

    DWORD err = GetLastError();
    if (err == ERROR_PIPE_BUSY && retry < kMaxRetries - 1) {
      Sleep(kRetryDelayMs);
      continue;
    }
    OutputDebugStringW((L"[PipeClient] CreateFile failed, err=" +
                         std::to_wstring(err) + L"\n").c_str());
    return false;
  }

  return false;
}

void PipeClient::Disconnect() {
  if (m_hPipe != INVALID_HANDLE_VALUE) {
    CloseHandle(m_hPipe);
    m_hPipe = INVALID_HANDLE_VALUE;
  }
}

bool PipeClient::IsConnected() const {
  return m_hPipe != INVALID_HANDLE_VALUE;
}

std::string PipeClient::ReadMessage() {
  if (!IsConnected()) return "";
  return PipeRecv(m_hPipe, kPipeBufSize);
}

bool PipeClient::SendMessage(const std::string& json) {
  if (!IsConnected()) return false;
  return PipeSend(m_hPipe, json);
}

bool PipeClient::SendCommand(const std::string& json) {
  if (!SendMessage(json)) return false;

  // 等待 ACK/ERR 响应
  std::string resp = PipeRecv(m_hPipe, kPipeBufSize);
  if (resp.empty()) return false;

  PipeMessage msg = ParseMessage(resp);
  return msg.type == PipeMsgType::MT_ACK;
}

}  // namespace fluxing
