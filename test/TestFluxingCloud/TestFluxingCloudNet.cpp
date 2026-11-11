// TestFluxingCloudNet.cpp — 真实 WinHTTP 传输层的**回环**回归测试（批次 AK）。
//
// 为什么单独一个文件：winsock2.h 必须排在 windows.h 之前，否则会与
// windows.h 拉进来的 winsock.h 冲突。本文件用不到 windows.h 的任何东西
// （只做 socket + 线程），因此只包含 winsock2.h / ws2tcpip.h，
// 完全绕开包含顺序问题。
//
// 判据（对应生产缺陷 L213）：
//   1. HTTP 200 ⇒ transport_ok == true、status == 200、body 逐字相符；
//   2. HTTP 404 ⇒ transport_ok **仍为 true**（HTTP 层错误不是传输层失败）
//      —— 上层「网络请求失败」与「服务器返回错误」的分流就靠这条契约；
//   3. 连不上 ⇒ transport_ok == false 且 error 非空（中文原因）。
// 修复前（WinHttpTransport 从不把 transport_ok 置 true）1 / 2 必然失败
// ⇒ 本组用例有牙，不是摆设。
//
// 纯本地回环：不连外网、不依赖线上服务，任何机器都能跑。

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include "../../WeaselServer/Cloud/CloudTransport.h"

namespace fc = fluxing::cloud;

namespace {

int g_net_pass = 0;
int g_net_fail = 0;

void NetCheck(bool ok, const char* name) {
  if (ok) {
    ++g_net_pass;
    std::printf("  [PASS] %s\n", name);
  } else {
    ++g_net_fail;
    std::printf("  [FAIL] %s\n", name);
  }
}

// ---------------------------------------------------------------------------
// 极简回环 HTTP/1.1 服务端：按请求路径回 200 或 404。
// ---------------------------------------------------------------------------
class LoopbackHttpServer {
 public:
  LoopbackHttpServer() {
    WSADATA wsa;
    wsa = WSADATA();
    ::WSAStartup(MAKEWORD(2, 2), &wsa);

    listen_socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // 由系统分配端口
    ::bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::listen(listen_socket_, 4);
    int addr_len = sizeof(addr);
    ::getsockname(listen_socket_, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    port_ = ::ntohs(addr.sin_port);

    running_ = true;
    thread_ = std::thread([this]() { Serve(); });
  }

  ~LoopbackHttpServer() {
    running_ = false;
    // 唤醒 accept：自己连一次，线程循环看到 running_=false 即退出。
    if (listen_socket_ != INVALID_SOCKET && port_ != 0) {
      SOCKET wake = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      sockaddr_in addr = {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
      addr.sin_port = ::htons(port_);
      ::connect(wake, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
      ::closesocket(wake);
    }
    if (thread_.joinable())
      thread_.join();
    if (listen_socket_ != INVALID_SOCKET)
      ::closesocket(listen_socket_);
    ::WSACleanup();
  }

  LoopbackHttpServer(const LoopbackHttpServer&) = delete;
  LoopbackHttpServer& operator=(const LoopbackHttpServer&) = delete;

  unsigned short port() const { return port_; }

 private:
  void Serve() {
    while (running_.load()) {
      sockaddr_in peer = {};
      int peer_len = sizeof(peer);
      SOCKET conn = ::accept(listen_socket_, reinterpret_cast<sockaddr*>(&peer),
                             &peer_len);
      if (conn == INVALID_SOCKET)
        break;
      DWORD timeout_ms = 2000;
      ::setsockopt(conn, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms),
                   sizeof(timeout_ms));
      char buf[4096] = {0};
      const int n = ::recv(conn, buf, static_cast<int>(sizeof(buf)), 0);
      const std::string request(buf, n > 0 ? static_cast<size_t>(n) : 0);
      const bool missing = request.find("/missing") != std::string::npos;
      const std::string body =
          missing ? "{\"error\":{\"code\":\"not_found\"}}" : "{\"ok\":true}";
      std::string response =
          missing ? "HTTP/1.1 404 Not Found\r\n" : "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: application/json\r\n";
      response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
      response += "Connection: close\r\n\r\n";
      response += body;
      ::send(conn, response.data(), static_cast<int>(response.size()), 0);
      ::shutdown(conn, SD_BOTH);
      ::closesocket(conn);
    }
  }

  SOCKET listen_socket_ = INVALID_SOCKET;
  unsigned short port_ = 0;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

// 预留一个「绑定后立刻关闭」的端口 ⇒ 连接必然被拒（ECONNREFUSED）。
unsigned short ReserveClosedPort() {
  SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  ::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  int len = sizeof(addr);
  ::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &len);
  const unsigned short port = ::ntohs(addr.sin_port);
  ::closesocket(s);
  return port;
}

}  // namespace

int RunFluxingCloudNetTests(int* out_pass) {
  g_net_pass = 0;
  g_net_fail = 0;
  std::printf("---- 批次 AK：WinHTTP 传输层回环回归（不连外网）----\n");

  LoopbackHttpServer server;
  NetCheck(server.port() != 0, "回环服务端已监听随机端口");

  fc::CloudTransportOptions options;
  options.max_retries = 0;  // 失败不重试 ⇒ 用例快
  options.timeout_ms = 5000;
  fc::WinHttpTransport transport(
      "http://127.0.0.1:" + std::to_string(static_cast<int>(server.port())),
      options);

  // 1) HTTP 200 —— 修复前 transport_ok 恒为假，这一组必然失败。
  {
    fc::CloudHttpRequest req;
    req.method = "GET";
    req.path = "/probe";
    const fc::CloudHttpResponse resp = transport.Send(req);
    NetCheck(resp.transport_ok, "HTTP 200 ⇒ transport_ok 为真");
    NetCheck(resp.status == 200, "HTTP 200 ⇒ status 为 200");
    NetCheck(resp.body == "{\"ok\":true}", "HTTP 200 ⇒ 响应体逐字相符");
    NetCheck(resp.error.empty(), "HTTP 200 ⇒ error 为空");
    NetCheck(resp.attempts == 1, "max_retries=0 ⇒ 只发一次");
  }

  // 2) HTTP 404 —— HTTP 层错误**不是**传输层失败（上层分流契约）。
  {
    fc::CloudHttpRequest req;
    req.method = "GET";
    req.path = "/missing";
    const fc::CloudHttpResponse resp = transport.Send(req);
    NetCheck(resp.transport_ok, "HTTP 404 ⇒ transport_ok 仍为真");
    NetCheck(resp.status == 404, "HTTP 404 ⇒ status 为 404");
    NetCheck(resp.body.find("not_found") != std::string::npos,
             "HTTP 404 ⇒ 响应体含错误码");
  }

  // 3) 连不上 ⇒ 传输层失败，且给出中文原因（不是空串兜底）。
  {
    const unsigned short closed = ReserveClosedPort();
    fc::CloudTransportOptions fail_options;
    fail_options.max_retries = 0;
    fail_options.timeout_ms = 3000;
    fc::WinHttpTransport dead(
        "http://127.0.0.1:" + std::to_string(static_cast<int>(closed)),
        fail_options);
    fc::CloudHttpRequest req;
    req.method = "GET";
    req.path = "/probe";
    const fc::CloudHttpResponse resp = dead.Send(req);
    NetCheck(!resp.transport_ok, "连不上 ⇒ transport_ok 为假");
    NetCheck(!resp.error.empty(), "连不上 ⇒ error 非空（中文原因）");
  }

  if (out_pass != nullptr)
    *out_pass = g_net_pass;
  std::printf("  回环回归：PASS=%d FAIL=%d\n", g_net_pass, g_net_fail);
  return g_net_fail;
}
