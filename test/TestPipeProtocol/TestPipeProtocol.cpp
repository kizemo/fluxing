//
// TestPipeProtocol.cpp — Sandbox 测试 named pipe IPC 双向通信
//
// Phase K2 (v0.19.0.51) T009: 验证 PHRASES / ADD / EDIT / DELETE / INJECT 协议
// 编译: 手动 MSBuild 或用 cl.exe 直接编译
//
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "FluxingPipeProtocol.h"

static int s_passed = 0;
static int s_failed = 0;

#define ASSERT(cond, msg) do { \
  if (cond) { s_passed++; printf("  PASS: %s\n", msg); } \
  else { s_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

// ===== 测试用 pipe server (模拟 WeaselServer 端) =====

static DWORD WINAPI TestServerThread(LPVOID param) {
  const wchar_t* pipeName = (const wchar_t*)param;
  std::vector<fluxing::PipePhrase> phrases = {
    {L"\x4f60\x597d", L""},     // 你好
    {L"\x4e16\x754c", L""},     // 世界
    {L"\x6d4b\x8bd5", L""},     // 测试
    {L"\x5e38\x7528", L""},     // 常用
    {L"\x77ed\x8bed", L""},     // 短语
  };

  HANDLE hPipe = CreateNamedPipeW(
      pipeName, PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      1, 4096, 4096, 0, nullptr);

  if (hPipe == INVALID_HANDLE_VALUE) {
    printf("  FAIL: CreateNamedPipe failed, err=%lu\n", GetLastError());
    return 1;
  }

  printf("  [Server] Pipe created, waiting for client...\n");

  BOOL connected = ConnectNamedPipe(hPipe, nullptr);
  if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
    printf("  FAIL: ConnectNamedPipe failed, err=%lu\n", GetLastError());
    CloseHandle(hPipe);
    return 2;
  }

  printf("  [Server] Client connected\n");

  // 1. 发送 PHRASES
  std::string json = fluxing::BuildPHRASES(phrases);
  if (!fluxing::PipeSend(hPipe, json)) {
    printf("  FAIL: Send PHRASES failed\n");
    return 3;
  }
  printf("  [Server] Sent PHRASES (%zu bytes)\n", json.size());

  // 2. 等待客户端命令循环
  for (int i = 0; i < 6; i++) {
    std::string cmdJson = fluxing::PipeRecv(hPipe, 4096);
    if (cmdJson.empty()) {
      printf("  [Server] No more commands, exiting loop\n");
      break;
    }
    printf("  [Server] Received: %s\n", cmdJson.c_str());

    fluxing::PipeMessage msg = fluxing::ParseMessage(cmdJson);

    switch (msg.type) {
      case fluxing::PipeMsgType::MT_ADD: {
        if (!msg.text.empty()) {
          phrases.push_back({msg.text, L""});
        }
        fluxing::PipeSend(hPipe, fluxing::BuildPHRASES(phrases));
        break;
      }
      case fluxing::PipeMsgType::MT_EDIT: {
        if (msg.id >= 0 && msg.id < (int)phrases.size()) {
          phrases[msg.id].text = msg.text;
        }
        fluxing::PipeSend(hPipe, fluxing::BuildPHRASES(phrases));
        break;
      }
      case fluxing::PipeMsgType::MT_DELETE: {
        if (msg.id >= 0 && msg.id < (int)phrases.size()) {
          phrases.erase(phrases.begin() + msg.id);
        }
        fluxing::PipeSend(hPipe, fluxing::BuildPHRASES(phrases));
        break;
      }
      case fluxing::PipeMsgType::MT_INJECT: {
        fluxing::PipeSend(hPipe, fluxing::BuildACK());
        break;
      }
      case fluxing::PipeMsgType::MT_SHUTDOWN: {
        fluxing::PipeSend(hPipe, fluxing::BuildACK());
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        return 0;
      }
      default:
        fluxing::PipeSend(hPipe, fluxing::BuildERR(L"Unknown"));
        break;
    }
  }

  DisconnectNamedPipe(hPipe);
  CloseHandle(hPipe);
  return 0;
}

// ===== 测试用例 =====

void TestJsonEscape() {
  printf("\n--- Test: JsonEscape ---\n");
  ASSERT(fluxing::JsonEscape(L"hello") == L"hello", "plain text unchanged");
  ASSERT(fluxing::JsonEscape(L"a\"b") == L"a\\\"b", "quote escaped");
  ASSERT(fluxing::JsonEscape(L"a\\b") == L"a\\\\b", "backslash escaped");
  ASSERT(fluxing::JsonEscape(L"a\nb") == L"a\\nb", "newline escaped");
}

void TestBuildMessages() {
  printf("\n--- Test: Build messages ---\n");
  std::vector<fluxing::PipePhrase> phrases = {
    {L"hello", L""},
    {L"world", L"test"},
  };
  std::string json = fluxing::BuildPHRASES(phrases);
  ASSERT(!json.empty(), "BuildPHRASES non-empty");
  ASSERT(json.find("\"type\":\"PHRASES\"") != std::string::npos, "PHRASES has type");
  ASSERT(json.find("\"text\":\"hello\"") != std::string::npos, "PHRASES has text");
  ASSERT(json.find("\"category\":\"test\"") != std::string::npos, "PHRASES has category");

  std::string addJson = fluxing::BuildADD(L"test123");
  ASSERT(addJson.find("\"type\":\"ADD\"") != std::string::npos, "BuildADD has type");
  ASSERT(addJson.find("\"text\":\"test123\"") != std::string::npos, "BuildADD has text");

  std::string editJson = fluxing::BuildEDIT(3, L"modified");
  ASSERT(editJson.find("\"type\":\"EDIT\"") != std::string::npos, "BuildEDIT has type");
  ASSERT(editJson.find("\"id\":3") != std::string::npos, "BuildEDIT has id");

  std::string delJson = fluxing::BuildDELETE(5);
  ASSERT(delJson.find("\"type\":\"DELETE\"") != std::string::npos, "BuildDELETE has type");
  ASSERT(delJson.find("\"id\":5") != std::string::npos, "BuildDELETE has id");

  std::string ackJson = fluxing::BuildACK();
  ASSERT(ackJson.find("\"type\":\"ACK\"") != std::string::npos, "BuildACK has type");

  std::string errJson = fluxing::BuildERR(L"something wrong");
  ASSERT(errJson.find("\"type\":\"ERR\"") != std::string::npos, "BuildERR has type");

  std::string shutdownJson = fluxing::BuildSHUTDOWN();
  ASSERT(shutdownJson.find("\"type\":\"SHUTDOWN\"") != std::string::npos, "BuildSHUTDOWN has type");
}

void TestParseMessages() {
  printf("\n--- Test: Parse messages ---\n");

  // Parse PHRASES
  std::vector<fluxing::PipePhrase> phrases = {{L"abc", L""}, {L"def", L"cat"}};
  std::string json = fluxing::BuildPHRASES(phrases);
  fluxing::PipeMessage msg = fluxing::ParseMessage(json);
  ASSERT(msg.type == fluxing::PipeMsgType::MT_PHRASES, "Parse PHRASES type");
  ASSERT(msg.phrases.size() == 2, "Parse PHRASES count=2");
  ASSERT(msg.phrases[0].text == L"abc", "Parse PHRASES[0].text");
  ASSERT(msg.phrases[1].category == L"cat", "Parse PHRASES[1].category");

  // Parse ACK
  fluxing::PipeMessage ack = fluxing::ParseMessage(fluxing::BuildACK());
  ASSERT(ack.type == fluxing::PipeMsgType::MT_ACK, "Parse ACK type");

  // Parse ADD
  fluxing::PipeMessage add = fluxing::ParseMessage(fluxing::BuildADD(L"new phrase"));
  ASSERT(add.type == fluxing::PipeMsgType::MT_ADD, "Parse ADD type");
  ASSERT(add.text == L"new phrase", "Parse ADD text");

  // Parse ERR
  fluxing::PipeMessage err = fluxing::ParseMessage(fluxing::BuildERR(L"fail"));
  ASSERT(err.type == fluxing::PipeMsgType::MT_ERR, "Parse ERR type");
}

void TestPipeCommunication() {
  printf("\n--- Test: Pipe communication ---\n");

  DWORD pid = GetCurrentProcessId();
  std::wstring pipeName = fluxing::MakePipeName(pid);
  printf("  Pipe name: %ls\n", pipeName.c_str());

  // 启动 server 线程
  HANDLE hServerThread = CreateThread(nullptr, 0, TestServerThread,
                                       (LPVOID)pipeName.c_str(), 0, nullptr);
  ASSERT(hServerThread != nullptr, "Server thread created");

  // 给 server 一点时间创建 pipe
  Sleep(200);

  // 客户端连接
  HANDLE hClient = CreateFileW(pipeName.c_str(),
                                GENERIC_READ | GENERIC_WRITE,
                                0, nullptr, OPEN_EXISTING, 0, nullptr);
  ASSERT(hClient != INVALID_HANDLE_VALUE, "Client connected to pipe");
  if (hClient == INVALID_HANDLE_VALUE) {
    printf("  CreateFile err=%lu\n", GetLastError());
    WaitForSingleObject(hServerThread, 5000);
    return;
  }

  // 设消息模式
  DWORD mode = PIPE_READMODE_MESSAGE;
  SetNamedPipeHandleState(hClient, &mode, nullptr, nullptr);

  // 1. 接收 PHRASES
  std::string resp = fluxing::PipeRecv(hClient, 4096);
  ASSERT(!resp.empty(), "Client received PHRASES");
  fluxing::PipeMessage pm = fluxing::ParseMessage(resp);
  ASSERT(pm.type == fluxing::PipeMsgType::MT_PHRASES, "PHRASES type correct");
  ASSERT(pm.phrases.size() == 5, "PHRASES has 5 phrases");
  ASSERT(pm.phrases[0].text == L"\x4f60\x597d", "PHRASES[0] = 你好");

  // 2. 发送 ADD
  bool sent = fluxing::PipeSend(hClient, fluxing::BuildADD(L"\x65b0\x77ed\x8bed"));  // 新短语
  ASSERT(sent, "Client sent ADD");
  std::string addResp = fluxing::PipeRecv(hClient, 4096);
  ASSERT(!addResp.empty(), "Client received ADD response");
  fluxing::PipeMessage addMsg = fluxing::ParseMessage(addResp);
  ASSERT(addMsg.type == fluxing::PipeMsgType::MT_PHRASES, "ADD response is PHRASES");
  ASSERT(addMsg.phrases.size() == 6, "After ADD: 6 phrases");

  // 3. 发送 EDIT
  sent = fluxing::PipeSend(hClient, fluxing::BuildEDIT(0, L"\x4f60\x597d!"));  // 你好!
  ASSERT(sent, "Client sent EDIT");
  std::string editResp = fluxing::PipeRecv(hClient, 4096);
  ASSERT(!editResp.empty(), "Client received EDIT response");
  fluxing::PipeMessage editMsg = fluxing::ParseMessage(editResp);
  ASSERT(editMsg.type == fluxing::PipeMsgType::MT_PHRASES, "EDIT response is PHRASES");
  ASSERT(editMsg.phrases[0].text == L"\x4f60\x597d!", "EDIT changed text");

  // 4. 发送 DELETE
  sent = fluxing::PipeSend(hClient, fluxing::BuildDELETE(0));
  ASSERT(sent, "Client sent DELETE");
  std::string delResp = fluxing::PipeRecv(hClient, 4096);
  ASSERT(!delResp.empty(), "Client received DELETE response");
  fluxing::PipeMessage delMsg = fluxing::ParseMessage(delResp);
  ASSERT(delMsg.type == fluxing::PipeMsgType::MT_PHRASES, "DELETE response is PHRASES");
  ASSERT(delMsg.phrases.size() == 5, "After DELETE: 5 phrases");

  // 5. 发送 SHUTDOWN
  sent = fluxing::PipeSend(hClient, fluxing::BuildSHUTDOWN());
  ASSERT(sent, "Client sent SHUTDOWN");
  std::string shutdownResp = fluxing::PipeRecv(hClient, 4096);
  ASSERT(!shutdownResp.empty(), "Client received SHUTDOWN ACK");

  // 清理
  CloseHandle(hClient);
  WaitForSingleObject(hServerThread, 5000);
  CloseHandle(hServerThread);

  printf("  Pipe communication test complete\n");
}

int main() {
  printf("=== TestPipeProtocol — Named Pipe IPC Sandbox Test ===\n");
  printf("Phase K2 (v0.19.0.51) T009\n\n");

  TestJsonEscape();
  TestBuildMessages();
  TestParseMessages();
  TestPipeCommunication();

  printf("\n=== Results: %d PASS, %d FAIL ===\n", s_passed, s_failed);
  return s_failed > 0 ? 1 : 0;
}
