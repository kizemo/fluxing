#pragma once
//
// spec 100 阶段 D · T302 传输抽象（ICloudTransport + WinHTTP 实现）
//
// 目标（tasks.md T302）：
//   · 网络层可 mock（单测用假实现，不需要联网）；
//   · 超时、重试退避、代理自动探测；
//   · **证书校验不得被关闭**（本实现从不设置 WINHTTP_OPTION_SECURITY_FLAGS
//     的忽略标志，也不设置 WINHTTP_OPTION_SECURITY_FLAG_IGNORE_*）;
//   · **不得在 UI 线程调用**（由 CloudAccountService / CloudSyncService
//     在自己的后台线程里调用）。
//
// 依赖纪律（宪法 P3）：只用系统自带 WinHTTP（winhttp.lib），**不引 libcurl**。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {

// ---------------------------------------------------------------------------
// 线格式
// ---------------------------------------------------------------------------

struct CloudHttpHeader {
  std::string name;
  std::string value;
};

struct CloudHttpRequest {
  std::string method = "GET";
  std::string path;  // 必须以 '/' 开头（如 /v1/auth/login）
  std::vector<CloudHttpHeader> headers;
  std::string body;
};

struct CloudHttpResponse {
  // false = 网络层失败（DNS / 连接 / TLS / 超时）；此时 status 无意义。
  bool transport_ok = false;
  int status = 0;  // HTTP 状态码（transport_ok 为 true 时有效）
  std::string body;
  std::string error;  // 中文原因（网络失败时写这里）
  int attempts = 0;   // 实际发出的请求次数（含首次）
};

// ---------------------------------------------------------------------------
// 抽象接口（单测注入假实现）
// ---------------------------------------------------------------------------

class ICloudTransport {
 public:
  virtual ~ICloudTransport() = default;

  // 阻塞调用。实现必须保证：不在调用方是 UI 线程时被使用。
  virtual CloudHttpResponse Send(const CloudHttpRequest& request) = 0;
};

// ---------------------------------------------------------------------------
// WinHTTP 实现
// ---------------------------------------------------------------------------

struct CloudTransportOptions {
  // ADR 0074 决策：服务端单批 64 条并发化后为秒级，30 s 留余量。
  int timeout_ms = 30000;
  // 额外重试次数（总尝试次数 = 1 + max_retries）。
  int max_retries = 2;
  int retry_backoff_base_ms = 500;  // 指数退避基数
  int max_response_bytes = 8 * 1024 * 1024;
  std::string user_agent = "Fluxing/1.0";
};

class WinHttpTransport : public ICloudTransport {
 public:
  // base_url 形如 https://api.example.com（可带端口与路径前缀）。
  explicit WinHttpTransport(
      std::string base_url,
      CloudTransportOptions options = CloudTransportOptions());
  ~WinHttpTransport() override;

  WinHttpTransport(const WinHttpTransport&) = delete;
  WinHttpTransport& operator=(const WinHttpTransport&) = delete;

  CloudHttpResponse Send(const CloudHttpRequest& request) override;

  const std::string& base_url() const { return base_url_; }
  const CloudTransportOptions& options() const { return options_; }

 private:
  // 单次尝试（不含重试逻辑）。
  CloudHttpResponse SendOnce(const CloudHttpRequest& request);

  std::string base_url_;
  CloudTransportOptions options_;
  std::mutex mutex_;
};

// ---------------------------------------------------------------------------
// 纯函数（不联网，可单测）
// ---------------------------------------------------------------------------

// 把 base_url 与 path 拼成绝对 URL。
//   · base_url 必须以 http:// 或 https:// 开头；
//   · path 必须以 '/' 开头；
//   · base_url 的尾部 '/' 与 path 的头部 '/' 归一化成一个。
bool BuildUrl(const std::string& base_url,
              const std::string& path,
              std::string* out,
              std::string* err);

// 是否值得再试一次：网络层失败、429、5xx。
bool ShouldRetry(bool transport_ok, int status, int attempt, int max_retries);

// 退避时长（毫秒）：base * 2^(attempt-1)，封顶 8000ms。
int RetryBackoffMs(const CloudTransportOptions& options, int attempt);

// 路径段百分号编码（设备标识等放进 URL 路径时用）。
std::string UrlEncodePathSegment(const std::string& raw);

}  // namespace cloud
}  // namespace fluxing
