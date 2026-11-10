// CloudJson.cpp — 最小 JSON 解析器实现。
//
// 解析器是**递归下降 + 显式节点数组**：
//   · 递归只在「值 → 值」这一层发生，深度上限 64（防栈溢出）；
//   · 每个值解析成一个 JsonNode 放进 document.nodes，父子关系用下标表达
//     ⇒ JsonNode 里没有自引用的不完整类型，各编译配置行为一致。
//
#include "CloudJson.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>

namespace fluxing {
namespace cloud {
namespace json {
namespace {

constexpr int kMaxDepth = 64;

bool IsWs(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void AppendUtf8(uint32_t cp, std::string* out) {
  if (cp <= 0x7F) {
    out->push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0x0F | (cp >> 18)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

class Parser {
 public:
  Parser(const std::string& text, JsonDocument* doc) : s_(text), doc_(doc) {}

  bool Run() {
    SkipWs();
    int root = -1;
    if (!ParseValue(&root))
      return false;
    SkipWs();
    if (pos_ != s_.size())
      return Fail("JSON 结尾有多余内容");
    doc_->root = root;
    doc_->ok = true;
    return true;
  }

 private:
  bool Fail(const char* message) {
    if (doc_->error.empty())
      doc_->error = message;
    return false;
  }

  void SkipWs() {
    while (pos_ < s_.size() && IsWs(s_[pos_]))
      ++pos_;
  }

  int AddNode(JsonNode node) {
    doc_->nodes.push_back(std::move(node));
    return static_cast<int>(doc_->nodes.size()) - 1;
  }

  bool ParseHex4(uint32_t* out) {
    if (pos_ + 4 > s_.size())
      return Fail("JSON 的 \\u 转义不完整");
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = s_[pos_ + static_cast<size_t>(i)];
      v <<= 4;
      if (c >= '0' && c <= '9')
        v |= static_cast<uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f')
        v |= static_cast<uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        v |= static_cast<uint32_t>(c - 'A' + 10);
      else
        return Fail("JSON 的 \\u 转义含非十六进制字符");
    }
    pos_ += 4;
    *out = v;
    return true;
  }

  bool ParseString(std::string* out) {
    out->clear();
    if (pos_ >= s_.size() || s_[pos_] != '"')
      return Fail("JSON 字符串缺少左引号");
    ++pos_;
    for (;;) {
      if (pos_ >= s_.size())
        return Fail("JSON 字符串没有结束引号");
      const unsigned char c = static_cast<unsigned char>(s_[pos_]);
      if (c == '"') {
        ++pos_;
        return true;
      }
      if (c == '\\') {
        ++pos_;
        if (pos_ >= s_.size())
          return Fail("JSON 转义序列不完整");
        const char e = s_[pos_++];
        switch (e) {
          case '"':
            out->push_back('"');
            break;
          case '\\':
            out->push_back('\\');
            break;
          case '/':
            out->push_back('/');
            break;
          case 'b':
            out->push_back('\b');
            break;
          case 'f':
            out->push_back('\f');
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'u': {
            uint32_t cp = 0;
            if (!ParseHex4(&cp))
              return false;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
              if (pos_ + 1 >= s_.size() || s_[pos_] != '\\' ||
                  s_[pos_ + 1] != 'u')
                return Fail("JSON 的高代理缺少低代理");
              pos_ += 2;
              uint32_t low = 0;
              if (!ParseHex4(&low))
                return false;
              if (low < 0xDC00 || low > 0xDFFF)
                return Fail("JSON 的代理对不合法");
              cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
              return Fail("JSON 出现孤立的低代理");
            }
            AppendUtf8(cp, out);
            break;
          }
          default:
            return Fail("JSON 出现未知转义字符");
        }
        continue;
      }
      if (c < 0x20)
        return Fail("JSON 字符串含未转义的控制字符");
      out->push_back(static_cast<char>(c));
      ++pos_;
    }
  }

  bool ParseNumber(JsonNode* node) {
    const size_t start = pos_;
    if (pos_ < s_.size() && s_[pos_] == '-')
      ++pos_;
    if (pos_ >= s_.size())
      return Fail("JSON 数字不完整");
    if (s_[pos_] == '0') {
      ++pos_;
    } else if (s_[pos_] >= '1' && s_[pos_] <= '9') {
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9')
        ++pos_;
    } else {
      return Fail("JSON 数字格式不正确");
    }
    bool is_integer = true;
    if (pos_ < s_.size() && s_[pos_] == '.') {
      is_integer = false;
      ++pos_;
      if (pos_ >= s_.size() || s_[pos_] < '0' || s_[pos_] > '9')
        return Fail("JSON 小数部分格式不正确");
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9')
        ++pos_;
    }
    if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      is_integer = false;
      ++pos_;
      if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-'))
        ++pos_;
      if (pos_ >= s_.size() || s_[pos_] < '0' || s_[pos_] > '9')
        return Fail("JSON 指数部分格式不正确");
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9')
        ++pos_;
    }

    const std::string text = s_.substr(start, pos_ - start);
    node->type = JsonType::kNumber;
    errno = 0;
    char* end = nullptr;
    const double d = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0')
      return Fail("JSON 数字无法解析");
    node->number_value = d;
    if (is_integer) {
      errno = 0;
      char* iend = nullptr;
      const long long v = std::strtoll(text.c_str(), &iend, 10);
      if (errno == 0 && iend != text.c_str() && *iend == '\0') {
        node->is_integer = true;
        node->int_value = static_cast<int64_t>(v);
      }
    }
    return true;
  }

  bool ParseLiteral(const char* literal,
                    JsonType type,
                    bool bool_value,
                    int* out_index) {
    const size_t len = std::strlen(literal);
    if (s_.compare(pos_, len, literal) != 0)
      return Fail("JSON 出现无法识别的字面量");
    pos_ += len;
    JsonNode node;
    node.type = type;
    node.bool_value = bool_value;
    *out_index = AddNode(std::move(node));
    return true;
  }

  bool ParseValue(int* out_index) {
    if (depth_ >= kMaxDepth)
      return Fail("JSON 嵌套层数过深");
    SkipWs();
    if (pos_ >= s_.size())
      return Fail("JSON 内容意外结束");
    const char c = s_[pos_];

    if (c == '{') {
      ++pos_;
      ++depth_;
      JsonNode node;
      node.type = JsonType::kObject;
      const int index = AddNode(std::move(node));
      SkipWs();
      if (pos_ < s_.size() && s_[pos_] == '}') {
        ++pos_;
        --depth_;
        *out_index = index;
        return true;
      }
      for (;;) {
        SkipWs();
        std::string key;
        if (!ParseString(&key))
          return false;
        SkipWs();
        if (pos_ >= s_.size() || s_[pos_] != ':')
          return Fail("JSON 对象缺少冒号");
        ++pos_;
        int child = -1;
        if (!ParseValue(&child))
          return false;
        doc_->nodes[static_cast<size_t>(index)].keys.push_back(key);
        doc_->nodes[static_cast<size_t>(index)].children.push_back(child);
        SkipWs();
        if (pos_ < s_.size() && s_[pos_] == ',') {
          ++pos_;
          continue;
        }
        if (pos_ < s_.size() && s_[pos_] == '}') {
          ++pos_;
          break;
        }
        return Fail("JSON 对象格式不正确");
      }
      --depth_;
      *out_index = index;
      return true;
    }

    if (c == '[') {
      ++pos_;
      ++depth_;
      JsonNode node;
      node.type = JsonType::kArray;
      const int index = AddNode(std::move(node));
      SkipWs();
      if (pos_ < s_.size() && s_[pos_] == ']') {
        ++pos_;
        --depth_;
        *out_index = index;
        return true;
      }
      for (;;) {
        int child = -1;
        if (!ParseValue(&child))
          return false;
        doc_->nodes[static_cast<size_t>(index)].children.push_back(child);
        SkipWs();
        if (pos_ < s_.size() && s_[pos_] == ',') {
          ++pos_;
          continue;
        }
        if (pos_ < s_.size() && s_[pos_] == ']') {
          ++pos_;
          break;
        }
        return Fail("JSON 数组格式不正确");
      }
      --depth_;
      *out_index = index;
      return true;
    }

    if (c == '"') {
      std::string str;
      if (!ParseString(&str))
        return false;
      JsonNode node;
      node.type = JsonType::kString;
      node.string_value = std::move(str);
      *out_index = AddNode(std::move(node));
      return true;
    }

    if (c == 't')
      return ParseLiteral("true", JsonType::kBool, true, out_index);
    if (c == 'f')
      return ParseLiteral("false", JsonType::kBool, false, out_index);
    if (c == 'n')
      return ParseLiteral("null", JsonType::kNull, false, out_index);

    if (c == '-' || (c >= '0' && c <= '9')) {
      JsonNode node;
      if (!ParseNumber(&node))
        return false;
      *out_index = AddNode(std::move(node));
      return true;
    }

    return Fail("JSON 出现无法识别的字符");
  }

  const std::string& s_;
  JsonDocument* doc_;
  size_t pos_ = 0;
  int depth_ = 0;
};

}  // namespace

const JsonNode* Member(const JsonDocument& doc,
                       int object_index,
                       const char* key) {
  if (key == nullptr)
    return nullptr;
  const JsonNode* obj = doc.Node(object_index);
  if (obj == nullptr || obj->type != JsonType::kObject)
    return nullptr;
  const size_t count = obj->keys.size() < obj->children.size()
                           ? obj->keys.size()
                           : obj->children.size();
  for (size_t i = 0; i < count; ++i) {
    if (obj->keys[i] == key)
      return doc.Node(obj->children[i]);
  }
  return nullptr;
}

bool MemberString(const JsonDocument& doc,
                  int object_index,
                  const char* key,
                  std::string* out) {
  if (out == nullptr)
    return false;
  const JsonNode* node = Member(doc, object_index, key);
  if (node == nullptr || node->type != JsonType::kString)
    return false;
  *out = node->string_value;
  return true;
}

bool MemberBool(const JsonDocument& doc,
                int object_index,
                const char* key,
                bool* out) {
  if (out == nullptr)
    return false;
  const JsonNode* node = Member(doc, object_index, key);
  if (node == nullptr || node->type != JsonType::kBool)
    return false;
  *out = node->bool_value;
  return true;
}

bool MemberNumber(const JsonDocument& doc,
                  int object_index,
                  const char* key,
                  double* out) {
  if (out == nullptr)
    return false;
  const JsonNode* node = Member(doc, object_index, key);
  if (node == nullptr || node->type != JsonType::kNumber)
    return false;
  *out = node->number_value;
  return true;
}

bool MemberInt64(const JsonDocument& doc,
                 int object_index,
                 const char* key,
                 int64_t* out) {
  if (out == nullptr)
    return false;
  const JsonNode* node = Member(doc, object_index, key);
  if (node == nullptr || node->type != JsonType::kNumber)
    return false;
  if (!node->is_integer)
    return false;
  *out = node->int_value;
  return true;
}

std::string Quote(const std::string& raw) {
  std::string out;
  out.reserve(raw.size() + 2);
  out.push_back('"');
  for (size_t i = 0; i < raw.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(raw[i]);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          static const char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(kHex[(c >> 4) & 0x0F]);
          out.push_back(kHex[c & 0x0F]);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

bool Parse(const std::string& text, JsonDocument* out) {
  if (out == nullptr)
    return false;
  out->nodes.clear();
  out->root = -1;
  out->ok = false;
  out->error.clear();
  if (text.empty()) {
    out->error = "JSON 内容为空";
    return false;
  }
  Parser parser(text, out);
  if (!parser.Run()) {
    out->ok = false;
    if (out->error.empty())
      out->error = "JSON 解析失败";
    return false;
  }
  return true;
}

}  // namespace json
}  // namespace cloud
}  // namespace fluxing
