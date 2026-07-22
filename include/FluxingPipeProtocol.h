#pragma once
//
// FluxingPipeProtocol.h — Named pipe IPC 协议定义
//
// Phase K2 (v0.19.0.51): WeaselServer ↔ FluxingPhrasesDialog 双向通信
// 设计: 单 JSON 文本流 over named pipe (message mode), payload < 10KB
// 编码: UTF-8 (pipe 传输) ↔ UTF-16 (Windows UI)
//
#include <string>
#include <vector>
#include <windows.h>

namespace fluxing {

// ===== Pipe 名称 =====
// 格式: \\.\pipe\FluxingPhrasesDialog\{pid}
// PID 后缀区分多实例 (同一用户可开多个 WeaselServer)
inline std::wstring MakePipeName(DWORD pid) {
  return L"\\\\.\\pipe\\FluxingPhrasesDialog\\" + std::to_wstring(pid);
}

// ===== 消息类型 =====
enum class PipeMsgType {
  MT_PHRASES,   // Server → Client: 全量短语列表
  MT_ADD,       // Client → Server: 添加短语
  MT_EDIT,      // Client → Server: 编辑短语
  MT_DELETE,    // Client → Server: 删除短语
  MT_INJECT,    // Client → Server: 注入文本到当前应用
  MT_ACK,       // Server → Client: 操作确认
  MT_ERR,       // Server → Client: 错误信息
  MT_SHUTDOWN   // Bidirectional: 关闭连接
};

inline const wchar_t* PipeMsgTypeToString(PipeMsgType t) {
  switch (t) {
    case PipeMsgType::MT_PHRASES:  return L"PHRASES";
    case PipeMsgType::MT_ADD:      return L"ADD";
    case PipeMsgType::MT_EDIT:     return L"EDIT";
    case PipeMsgType::MT_DELETE:   return L"DELETE";
    case PipeMsgType::MT_INJECT:   return L"INJECT";
    case PipeMsgType::MT_ACK:      return L"ACK";
    case PipeMsgType::MT_ERR:      return L"ERR";
    case PipeMsgType::MT_SHUTDOWN: return L"SHUTDOWN";
  }
  return L"UNKNOWN";
}

inline PipeMsgType ParsePipeMsgType(const std::wstring& s) {
  if (s == L"PHRASES")  return PipeMsgType::MT_PHRASES;
  if (s == L"ADD")      return PipeMsgType::MT_ADD;
  if (s == L"EDIT")     return PipeMsgType::MT_EDIT;
  if (s == L"DELETE")   return PipeMsgType::MT_DELETE;
  if (s == L"INJECT")   return PipeMsgType::MT_INJECT;
  if (s == L"ACK")      return PipeMsgType::MT_ACK;
  if (s == L"ERR")      return PipeMsgType::MT_ERR;
  if (s == L"SHUTDOWN") return PipeMsgType::MT_SHUTDOWN;
  return PipeMsgType::MT_ERR;  // fallback
}

// ===== 短语数据结构 =====
struct PipePhrase {
  std::wstring text;
  std::wstring category;  // 空 = 无分类
};

// ===== 消息体 =====
struct PipeMessage {
  PipeMsgType type = PipeMsgType::MT_ACK;
  std::vector<PipePhrase> phrases;  // PHRASES 消息用
  std::wstring text;                // ADD / EDIT 用
  int id = -1;                      // EDIT / DELETE / INJECT 用 (-1 = 无)
  std::wstring errMsg;              // ERR 用
};

// ===== JSON 序列化 (手动, 不引入 nlohmann/json) =====

// 工具: JSON 字符串转义 (只处理 \ " 和换行)
inline std::wstring JsonEscape(const std::wstring& s) {
  std::wstring out;
  out.reserve(s.size() + 4);
  for (wchar_t c : s) {
    switch (c) {
      case L'"':  out += L"\\\""; break;
      case L'\\': out += L"\\\\"; break;
      case L'\n': out += L"\\n";  break;
      case L'\r': out += L"\\r";  break;
      case L'\t': out += L"\\t";  break;
      default:    out += c;       break;
    }
  }
  return out;
}

// 工具: 简单 JSON 字符串值提取 (读取 "..." 内的内容, 不处理转义)
inline std::wstring JsonExtractString(const std::wstring& json,
                                       const std::wstring& key) {
  std::wstring search = L"\"" + key + L"\":\"";
  size_t pos = json.find(search);
  if (pos == std::wstring::npos) return L"";
  pos += search.size();
  size_t end = json.find(L'"', pos);
  if (end == std::wstring::npos) return L"";
  return json.substr(pos, end - pos);
}

// 工具: 简单 JSON 整数值提取
inline int JsonExtractInt(const std::wstring& json, const std::wstring& key) {
  std::wstring search = L"\"" + key + L"\":";
  size_t pos = json.find(search);
  if (pos == std::wstring::npos) return -1;
  pos += search.size();
  // 跳过非数字字符
  while (pos < json.size() && !iswdigit(json[pos]) && json[pos] != L'-')
    ++pos;
  if (pos >= json.size()) return -1;
  return std::stoi(json.substr(pos));
}

// ===== 消息构建 (UTF-16 → UTF-8 for wire) =====

inline std::string BuildPHRASES(const std::vector<PipePhrase>& phrases) {
  std::wstring json = L"{\"type\":\"PHRASES\",\"phrases\":[";
  for (size_t i = 0; i < phrases.size(); ++i) {
    if (i > 0) json += L",";
    json += L"{\"text\":\"" + JsonEscape(phrases[i].text) + L"\"";
    if (!phrases[i].category.empty()) {
      json += L",\"category\":\"" + JsonEscape(phrases[i].category) + L"\"";
    }
    json += L"}";
  }
  json += L"]}\n";

  // UTF-16 → UTF-8
  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildADD(const std::wstring& text,
                             const std::wstring& category = L"") {
  std::wstring json = L"{\"type\":\"ADD\",\"text\":\"" + JsonEscape(text) + L"\"";
  if (!category.empty())
    json += L",\"category\":\"" + JsonEscape(category) + L"\"";
  json += L"}\n";

  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildEDIT(int id, const std::wstring& text) {
  std::wstring json = L"{\"type\":\"EDIT\",\"id\":" + std::to_wstring(id) +
                      L",\"text\":\"" + JsonEscape(text) + L"\"}\n";

  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildDELETE(int id) {
  std::wstring json = L"{\"type\":\"DELETE\",\"id\":" + std::to_wstring(id) +
                      L"}\n";

  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildINJECT(int id) {
  std::wstring json = L"{\"type\":\"INJECT\",\"id\":" + std::to_wstring(id) +
                      L"}\n";

  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildACK() {
  return std::string("{\"type\":\"ACK\"}\n");
}

inline std::string BuildERR(const std::wstring& msg) {
  std::wstring json = L"{\"type\":\"ERR\",\"msg\":\"" + JsonEscape(msg) +
                      L"\"}\n";

  int len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string out(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(),
                      &out[0], len, nullptr, nullptr);
  return out;
}

inline std::string BuildSHUTDOWN() {
  return std::string("{\"type\":\"SHUTDOWN\"}\n");
}

// ===== 消息解析 (UTF-8 wire → PipeMessage) =====

inline std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return L"";
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
                                 nullptr, 0);
  std::wstring out(len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
                      &out[0], len);
  return out;
}

inline PipeMessage ParseMessage(const std::string& utf8Json) {
  PipeMessage msg;
  std::wstring json = Utf8ToWide(utf8Json);

  // 提取 type 字段
  std::wstring typeStr = JsonExtractString(json, L"type");
  msg.type = ParsePipeMsgType(typeStr);

  switch (msg.type) {
    case PipeMsgType::MT_PHRASES: {
      // 解析 phrases 数组: {"text":"...","category":"..."},...
      size_t arrStart = json.find(L"\"phrases\":[");
      if (arrStart == std::wstring::npos) break;
      size_t pos = json.find(L'{', arrStart);
      while (pos != std::wstring::npos && pos < json.size()) {
        size_t objEnd = json.find(L'}', pos);
        if (objEnd == std::wstring::npos) break;
        std::wstring obj = json.substr(pos, objEnd - pos + 1);
        PipePhrase p;
        p.text = JsonExtractString(obj, L"text");
        p.category = JsonExtractString(obj, L"category");
        msg.phrases.push_back(p);
        pos = json.find(L'{', objEnd + 1);
      }
      break;
    }
    case PipeMsgType::MT_ADD:
      msg.text = JsonExtractString(json, L"text");
      break;
    case PipeMsgType::MT_EDIT:
      msg.id = JsonExtractInt(json, L"id");
      msg.text = JsonExtractString(json, L"text");
      break;
    case PipeMsgType::MT_DELETE:
    case PipeMsgType::MT_INJECT:
      msg.id = JsonExtractInt(json, L"id");
      break;
    case PipeMsgType::MT_ERR:
      msg.errMsg = JsonExtractString(json, L"msg");
      break;
    default:
      break;
  }
  return msg;
}

// ===== Pipe I/O 工具 =====

// 发送消息到 pipe (阻塞写入)
inline bool PipeSend(HANDLE hPipe, const std::string& data) {
  DWORD written = 0;
  BOOL ok = WriteFile(hPipe, data.c_str(), (DWORD)data.size(), &written, nullptr);
  if (!ok || written != data.size()) {
    OutputDebugStringW(L"[FluxingPipe] PipeSend WriteFile failed\n");
    return false;
  }
  FlushFileBuffers(hPipe);
  return true;
}

// 从 pipe 读取一条消息 (阻塞读取, 消息模式, 最多 bufsz 字节)
inline std::string PipeRecv(HANDLE hPipe, DWORD bufSize = 4096) {
  std::vector<char> buf(bufSize);
  DWORD bytesRead = 0;
  BOOL ok = ReadFile(hPipe, buf.data(), bufSize, &bytesRead, nullptr);
  if (!ok || bytesRead == 0) {
    if (GetLastError() == ERROR_BROKEN_PIPE) {
      OutputDebugStringW(L"[FluxingPipe] PipeRecv: broken pipe\n");
    } else {
      OutputDebugStringW(L"[FluxingPipe] PipeRecv ReadFile failed\n");
    }
    return "";
  }
  return std::string(buf.data(), bytesRead);
}

}  // namespace fluxing
