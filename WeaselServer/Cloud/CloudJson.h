#pragma once
//
// spec 100 阶段 D · 最小 JSON 解析 / 序列化（T301–T305 共用的线格式工具）
//
// 为什么自己写（宪法 P3「不新增第三方依赖」）：
//   系统没有「轻量 JSON」API —— WinRT 的 Windows.Data.Json 需要 C++/WinRT
//   头与 winmd，本工程没有该依赖；引入 nlohmann/json 之类的第三方库又违反 P3。
//   服务端返回体是固定的几种形状（login / refresh / push / pull / devices /
//   错误面），一个严格的小解析器足够，且能把「畸形响应」变成可断言的失败。
//
// 范围（够用即可，不是通用 JSON 库）：
//   · 对象 / 数组 / 字符串 / 数字 / 布尔 / null
//   · 字符串转义：\" \\ \/ \b \f \n \r \t \uXXXX（含 UTF-16 代理对）
//   · 数字按 JSON 文法逐字校验；整数同时保留 int64 视图（server_seq 用）
//   · 解析严格：尾随垃圾、缺引号、未转义控制字符、深度超限 ⇒ 失败并写中文原因
//   · Quote() 与解析器的转义规则对称，用于拼请求体
//
// 节点用「文档 + 节点数组」表示（不递归嵌套结构体）⇒ 头文件里没有
// 「不完整类型自引用」的坑，MSVC 各配置编译一致。
//
// PCH 注意（重要）：本头会被 WeaselServer 的 .cpp 包含，而 WeaselServer 用
//   stdafx.h 做预编译头；stdafx.h 里 **没有** 定义 NOMINMAX ⇒ windows.h 的
//   min/max 宏会污染 <algorithm>。因此凡是包含本头 / FluxingCloudEnvelope.h 的
//   源文件，都必须：
//     ① 自身是 `PrecompiledHeader=NotUsing`（见 WeaselServer.vcxproj）；
//     ② 文件第一行就 #define NOMINMAX。
//   WeaselServer/Cloud/*.cpp 与 test/TestFluxingCloud/TestFluxingCloud.cpp
//   都满足这两条。
//
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace fluxing {
namespace cloud {
namespace json {

// ---------------------------------------------------------------------------
// 数据模型
// ---------------------------------------------------------------------------

enum class JsonType {
  kNull = 0,
  kBool,
  kNumber,
  kString,
  kArray,
  kObject,
};

struct JsonNode {
  JsonType type = JsonType::kNull;
  bool bool_value = false;
  double number_value = 0.0;
  // 数字字面量是整数且落在 int64 内时为 true（server_seq / expires_in 用）。
  bool is_integer = false;
  int64_t int_value = 0;
  std::string string_value;
  // 对象：keys[i] 对应 children[i] 的成员名；数组：children 是元素下标。
  std::vector<std::string> keys;
  std::vector<int> children;
};

struct JsonDocument {
  std::vector<JsonNode> nodes;
  int root = -1;
  bool ok = false;
  std::string error;  // 中文原因（失败时）

  const JsonNode* Node(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= nodes.size())
      return nullptr;
    return &nodes[static_cast<size_t>(index)];
  }
};

// ---------------------------------------------------------------------------
// 取值辅助（找不到 / 类型不符 ⇒ 返回 false 或 nullptr，绝不猜）
// ---------------------------------------------------------------------------

// 取对象的成员节点；对象不存在或没有这个键 ⇒ nullptr。
const JsonNode* Member(const JsonDocument& doc,
                       int object_index,
                       const char* key);

bool MemberString(const JsonDocument& doc,
                  int object_index,
                  const char* key,
                  std::string* out);
bool MemberBool(const JsonDocument& doc,
                int object_index,
                const char* key,
                bool* out);
bool MemberNumber(const JsonDocument& doc,
                  int object_index,
                  const char* key,
                  double* out);
bool MemberInt64(const JsonDocument& doc,
                 int object_index,
                 const char* key,
                 int64_t* out);

// 把原始串转成**带引号**的 JSON 字符串（含转义）。
// 请求体拼装用：拼完直接是合法 JSON 字面量。
std::string Quote(const std::string& raw);

// ---------------------------------------------------------------------------
// 解析
// ---------------------------------------------------------------------------

// 严格解析。成功 ⇒ out->ok = true 且 out->root >= 0；
// 失败 ⇒ out->ok = false 且 out->error 写中文原因。
bool Parse(const std::string& text, JsonDocument* out);

}  // namespace json
}  // namespace cloud
}  // namespace fluxing
