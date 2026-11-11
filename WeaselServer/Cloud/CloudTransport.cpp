// CloudTransport.cpp — WinHTTP 传输实现 + 可单测的纯函数。
//
// 关键约束：
//   · 证书校验**保持开启**：本文件从不写 WINHTTP_OPTION_SECURITY_FLAGS，
//     也不设置任何 WINHTTP_OPTION_SECURITY_FLAG_IGNORE_*。
//   · 代理自动探测：WinHttpOpen 用 WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY。
//   · 不在 UI 线程调用（由上层的后台线程保证）。
//
#include "CloudTransport.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winhttp.h>

#include <cstdio>

namespace fluxing {
namespace cloud {
namespace {

std::wstring Utf8ToWide(const std::string& text) {
  if (text.empty())
    return std::wstring();
  const int count = ::MultiByteToWideChar(
      CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
  if (count <= 0)
    return std::wstring();
  std::wstring wide(static_cast<size_t>(count), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        &wide[0], count);
  return wide;
}

std::string WideToUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0')
    return std::string();
  const int count =
      ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (count <= 1)
    return std::string();
  std::string out(static_cast<size_t>(count - 1), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, text, -1, &out[0], count, nullptr, nullptr);
  return out;
}

std::string LastErrorText(DWORD code) {
  LPWSTR buffer = nullptr;
  const DWORD len = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  std::string message;
  if (len > 0 && buffer != nullptr) {
    message = WideToUtf8(buffer);
    while (!message.empty() &&
           (message.back() == '\n' || message.back() == '\r'))
      message.pop_back();
  }
  if (buffer != nullptr)
    ::LocalFree(buffer);
  if (message.empty()) {
    char fallback[64] = {0};
    std::snprintf(fallback, sizeof(fallback), "错误码 %lu",
                  static_cast<unsigned long>(code));
    message = fallback;
  }
  return message;
}

}  // namespace

// ---------------------------------------------------------------------------
// 纯函数
// ---------------------------------------------------------------------------

bool BuildUrl(const std::string& base_url,
              const std::string& path,
              std::string* out,
              std::string* err) {
  if (out == nullptr)
    return false;
  out->clear();
  const bool https = base_url.rfind("https://", 0) == 0;
  const bool http = base_url.rfind("http://", 0) == 0;
  if (!https && !http) {
    if (err)
      *err = "服务地址必须以 http:// 或 https:// 开头";
    return false;
  }
  if (path.empty() || path[0] != '/') {
    if (err)
      *err = "请求路径必须以 / 开头";
    return false;
  }
  std::string base = base_url;
  while (!base.empty() && base.back() == '/')
    base.pop_back();
  *out = base + path;
  return true;
}

bool ShouldRetry(bool transport_ok, int status, int attempt, int max_retries) {
  if (attempt > max_retries)
    return false;
  if (!transport_ok)
    return true;
  if (status == 429)
    return true;
  return status >= 500 && status <= 599;
}

int RetryBackoffMs(const CloudTransportOptions& options, int attempt) {
  int base = options.retry_backoff_base_ms;
  if (base <= 0)
    base = 500;
  int delay = base;
  for (int i = 1; i < attempt && delay < 8000; ++i)
    delay *= 2;
  if (delay > 8000)
    delay = 8000;
  return delay;
}

std::string UrlEncodePathSegment(const std::string& raw) {
  static const char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(raw.size() + 8);
  for (size_t i = 0; i < raw.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(raw[i]);
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      out.push_back(static_cast<char>(c));
      continue;
    }
    out.push_back('%');
    out.push_back(kHex[(c >> 4) & 0x0F]);
    out.push_back(kHex[c & 0x0F]);
  }
  return out;
}

// ---------------------------------------------------------------------------
// WinHttpTransport
// ---------------------------------------------------------------------------

WinHttpTransport::WinHttpTransport(std::string base_url,
                                   CloudTransportOptions options)
    : base_url_(std::move(base_url)), options_(options) {
  if (options_.timeout_ms <= 0)
    options_.timeout_ms = 15000;
  if (options_.max_retries < 0)
    options_.max_retries = 0;
  if (options_.retry_backoff_base_ms <= 0)
    options_.retry_backoff_base_ms = 500;
}

WinHttpTransport::~WinHttpTransport() = default;

CloudHttpResponse WinHttpTransport::Send(const CloudHttpRequest& request) {
  CloudHttpResponse response;
  std::lock_guard<std::mutex> guard(mutex_);

  const int max_attempts = 1 + options_.max_retries;
  for (int attempt = 1; attempt <= max_attempts; ++attempt) {
    response = SendOnce(request);
    response.attempts = attempt;
    if (!ShouldRetry(response.transport_ok, response.status, attempt,
                     options_.max_retries))
      break;
    if (attempt < max_attempts) {
      const int delay = RetryBackoffMs(options_, attempt);
      ::Sleep(static_cast<DWORD>(delay));
    }
  }
  return response;
}

CloudHttpResponse WinHttpTransport::SendOnce(const CloudHttpRequest& request) {
  CloudHttpResponse response;

  std::string url;
  std::string url_error;
  if (!BuildUrl(base_url_, request.path, &url, &url_error)) {
    response.transport_ok = false;
    response.error = url_error.empty() ? "请求地址无效" : url_error;
    return response;
  }

  const std::wstring url_w = Utf8ToWide(url);
  const std::wstring method_w =
      Utf8ToWide(request.method.empty() ? std::string("GET") : request.method);
  const std::wstring agent_w = Utf8ToWide(options_.user_agent);

  HINTERNET session =
      ::WinHttpOpen(agent_w.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (session == nullptr) {
    response.transport_ok = false;
    response.error = "无法建立网络会话：" + LastErrorText(::GetLastError());
    return response;
  }

  const int timeout = options_.timeout_ms;
  ::WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);

  // 关闭自动认证：避免 401 挑战时把本机 Windows 凭据（NTLM / Negotiate）
  // 自动交给服务端。本项目只用 Bearer 令牌，不需要 Windows 集成认证。
  // 注意：本 SDK 里没有 WINHTTP_AUTOLOGON_POLICY_IGNORE 这个常量
  // （只有 WINHTTP_AUTOLOGON_SECURITY_LEVEL_{MEDIUM,LOW}）；真正能做到
  // 「完全不做自动认证」的是 WINHTTP_OPTION_DISABLE_FEATURE +
  // WINHTTP_DISABLE_AUTHENTICATION。
  DWORD disable_features = WINHTTP_DISABLE_AUTHENTICATION;
  ::WinHttpSetOption(session, WINHTTP_OPTION_DISABLE_FEATURE, &disable_features,
                     sizeof(disable_features));

  HINTERNET connect = nullptr;
  HINTERNET request_handle = nullptr;
  bool ok = false;
  std::string error;

  wchar_t host[256] = {0};
  wchar_t url_path[2048] = {0};
  wchar_t extra[2048] = {0};
  URL_COMPONENTS components;
  ZeroMemory(&components, sizeof(components));
  components.dwStructSize = sizeof(components);
  components.lpszHostName = host;
  components.dwHostNameLength = ARRAYSIZE(host);
  components.lpszUrlPath = url_path;
  components.dwUrlPathLength = ARRAYSIZE(url_path);
  components.lpszExtraInfo = extra;
  components.dwExtraInfoLength = ARRAYSIZE(extra);

  if (!::WinHttpCrackUrl(url_w.c_str(), 0, 0, &components)) {
    error = "请求地址无法解析：" + LastErrorText(::GetLastError());
  } else {
    const std::wstring path_text(url_path);
    std::wstring object = path_text.empty() ? std::wstring(L"/") : path_text;
    const std::wstring extra_text(extra);
    if (!extra_text.empty())
      object += L"?" + extra_text;

    connect = ::WinHttpConnect(session, host, components.nPort, 0);
    if (connect == nullptr) {
      error = "无法连接到服务器：" + LastErrorText(::GetLastError());
    } else {
      // WinHttpOpenRequest 共 7 个参数：hConnect / pwszVerb / pwszObjectName /
      //   pwszVersion / pwszReferrer / ppwszAcceptTypes / dwFlags。
      // 少传一个会把 dwFlags 错位到 ppwszAcceptTypes 上 ⇒ C2660。
      request_handle = ::WinHttpOpenRequest(
          connect, method_w.c_str(), object.c_str(), L"HTTP/1.1",
          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
          (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE
                                                        : 0);
      if (request_handle == nullptr) {
        error = "无法创建网络请求：" + LastErrorText(::GetLastError());
      } else {
        ok = true;
      }
    }
  }

  if (!ok) {
    if (request_handle != nullptr)
      ::WinHttpCloseHandle(request_handle);
    if (connect != nullptr)
      ::WinHttpCloseHandle(connect);
    ::WinHttpCloseHandle(session);
    response.transport_ok = false;
    response.error = error.empty() ? "网络请求失败" : error;
    return response;
  }

  // 组装请求头（WinHttpSendRequest 追加的默认头与自定义头合并）。
  std::wstring headers;
  bool has_content_type = false;
  for (size_t i = 0; i < request.headers.size(); ++i) {
    const CloudHttpHeader& header = request.headers[i];
    if (header.name.empty())
      continue;
    std::string lower;
    lower.reserve(header.name.size());
    for (size_t k = 0; k < header.name.size(); ++k) {
      const char c = header.name[k];
      lower.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a')
                                             : c);
    }
    if (lower == "content-type")
      has_content_type = true;
    headers += Utf8ToWide(header.name);
    headers += L": ";
    headers += Utf8ToWide(header.value);
    headers += L"\r\n";
  }
  if (!request.body.empty() && !has_content_type) {
    headers += L"Content-Type: application/json; charset=utf-8\r\n";
  }

  const char* body_ptr = request.body.empty() ? nullptr : request.body.data();
  const DWORD body_len = static_cast<DWORD>(request.body.size());

  if (!::WinHttpSendRequest(
          request_handle, headers.empty() ? nullptr : headers.c_str(),
          headers.empty() ? 0 : static_cast<DWORD>(-1),
          const_cast<char*>(body_ptr), body_len, body_len, 0)) {
    response.transport_ok = false;
    response.error = "发送请求失败：" + LastErrorText(::GetLastError());
    ::WinHttpCloseHandle(request_handle);
    ::WinHttpCloseHandle(connect);
    ::WinHttpCloseHandle(session);
    return response;
  }

  if (!::WinHttpReceiveResponse(request_handle, nullptr)) {
    response.transport_ok = false;
    response.error = "接收响应失败：" + LastErrorText(::GetLastError());
    ::WinHttpCloseHandle(request_handle);
    ::WinHttpCloseHandle(connect);
    ::WinHttpCloseHandle(session);
    return response;
  }

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  if (::WinHttpQueryHeaders(
          request_handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
          WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size,
          WINHTTP_NO_HEADER_INDEX)) {
    response.status = static_cast<int>(status_code);
  } else {
    response.status = 0;
  }

  std::string body;
  for (;;) {
    DWORD available = 0;
    if (!::WinHttpQueryDataAvailable(request_handle, &available)) {
      response.transport_ok = false;
      response.error = "读取响应失败：" + LastErrorText(::GetLastError());
      break;
    }
    if (available == 0)
      break;
    if (static_cast<int>(body.size() + available) >
        options_.max_response_bytes) {
      response.transport_ok = false;
      response.error = "响应内容超过上限";
      break;
    }
    const size_t offset = body.size();
    body.resize(offset + available);
    DWORD read = 0;
    if (!::WinHttpReadData(request_handle, &body[offset], available, &read)) {
      response.transport_ok = false;
      response.error = "读取响应失败：" + LastErrorText(::GetLastError());
      body.resize(offset);
      break;
    }
    body.resize(offset + read);
  }

  ::WinHttpCloseHandle(request_handle);
  ::WinHttpCloseHandle(connect);
  ::WinHttpCloseHandle(session);

  // ★ 传输成功判定：整个响应读取过程中没有产生任何错误 ⇒ transport_ok=true。
  //   修复前这里**从未**把 transport_ok 置 true（结构体默认值是 false），
  //   于是**任何**正常响应都被上层（CloudAccountService / CloudSyncService）
  //   判成「网络层失败」，原因被兜底成「网络请求失败」，
  //   账号页最终显示「注册失败：网络请求失败：网络请求失败」。
  //   见 .specify/memory/lessons-learned.md L213。
  response.transport_ok = response.error.empty();
  response.body = std::move(body);
  if (response.transport_ok) {
    response.error.clear();
  } else if (response.error.empty()) {
    response.error = "网络请求失败";
  }
  return response;
}

}  // namespace cloud
}  // namespace fluxing
