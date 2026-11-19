#pragma once
//
// 批次 AT-1 · 账号页「历史邮箱」存储
//
// 为什么必须与凭据存储解耦：`Logout` 走 `CloudCredentialStore::Delete`
// 会把整条凭据记录（含 email）删掉 ⇒ 历史邮箱**不能**存在凭据存储里，
// 否则「退出登录」就把用户的历史一起清了。
//
// 落点：用户数据目录（真相源 = ResolveFluxingUserDataPath）下的一个
// **纯文本**文件，每行一个邮箱，UTF-8 无 BOM，最多 10 条，最近在前。
//
// PCH 注意：包含本头的 .cpp 必须是 `PrecompiledHeader=NotUsing` 且自带
//   `#define NOMINMAX`（原因见 CloudJson.h 顶部注释）。
//

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>

namespace fluxing {
namespace cloud {
namespace account_history {

// 历史列表的**上限**（条）。
constexpr size_t kMaxHistoryEntries = 10;

// ---------------------------------------------------------------------------
// 纯逻辑（不碰磁盘，可单测）
// ---------------------------------------------------------------------------

// 并入一个新邮箱：去重（大小写不敏感）/ 上限 10 条 / 最近在前。
// 空邮箱或只有空白 ⇒ 原样返回（不改变列表）。
std::vector<std::string> MergeHistory(const std::vector<std::string>& history,
                                      const std::string& email);

// 文本编解码：每行一个邮箱，行尾 '\n'，UTF-8 无 BOM。
std::string SerializeHistory(const std::vector<std::string>& history);
std::vector<std::string> ParseHistory(const std::string& text);

// ---------------------------------------------------------------------------
// 磁盘读写
// ---------------------------------------------------------------------------

// 写入历史（自动去重 / 截断 / 建目录）。失败时写中文原因。
bool SaveHistoryFile(const std::wstring& path,
                     const std::vector<std::string>& history,
                     std::string* err);

// 读取历史。文件不存在 / 读不出来 ⇒ 返回**空列表**（不抛异常、不崩）。
std::vector<std::string> LoadHistoryFile(const std::wstring& path);

// 默认落盘路径：<用户数据目录>\fluxing-cloud-accounts.history
std::wstring DefaultHistoryPath();

}  // namespace account_history
}  // namespace cloud
}  // namespace fluxing
