#pragma once
// spec 090 (2026-10-30): 候选词屏蔽名单 —— 读写 + 「可见页内索引 -> 引擎页内
// 索引」映射。
//
// 为什么单独成文件(与 include/FluxingUserPaths.h 同一条理由):屏蔽逻辑必须同时
// 被服务端 RimeWithWeasel 和两个单元测试工程(TestCandidateIgnoreFilter /
// TestCandidateRButtonDown)编译。include/WeaselUtility.h 依赖 stdafx 预置头,还
// 把 COM/wrl 一并拖进来,单元测试工程吃不下;所以这里只依赖 windows.h + STL,
// 全部实现为 inline 纯函数,header-only ⇒ 测试工程零 .vcxproj 改动。
//
// 不变量(FR-005 / FR-006):
//   - 过滤点唯一在服务端 _GetCandidateInfo,UI 侧不得再做第二次过滤;
//   -
//   过滤必须与映射在同一次遍历里产出,否则「可见索引」与「引擎索引」会各说各话。
//
// 名单文件路径沿用既有约定(WeaselPanel::LoadIgnoreList / 托盘「恢复已忽略的候选
// 项」都按它工作):
//   <用户数据目录>\<schema_id>.user_ignore.txt
// 编码兼容 UTF-8 BOM / UTF-16LE BOM / 裸 UTF-8,写入统一 UTF-8 + CRLF。

#include <windows.h>

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

namespace fluxing {

// 名单文件名(不含目录)。schema_id 为空时返回空串,调用方据此短路。
inline std::wstring IgnoreListFileName(const std::wstring& schema_id) {
  if (schema_id.empty())
    return std::wstring();
  return schema_id + L".user_ignore.txt";
}

// 名单文件全路径。任一段为空即返回空串 —— 绝不拼出 "\\" 这类残缺路径,
// 也绝不顺手建目录(P3:用户数据目录只读消费,不由本模块创建)。
inline std::wstring IgnoreListFilePath(const std::wstring& user_data_dir,
                                       const std::wstring& schema_id) {
  const std::wstring name = IgnoreListFileName(schema_id);
  if (name.empty() || user_data_dir.empty())
    return std::wstring();
  return user_data_dir + L"\\" + name;
}

inline std::wstring FluxingUtf8ToWide(const std::string& utf8) {
  if (utf8.empty())
    return std::wstring();
  const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                      (int)utf8.size(), nullptr, 0);
  if (len <= 0)
    return std::wstring();
  std::wstring out((size_t)len, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], len);
  return out;
}

inline std::string FluxingWideToUtf8(const std::wstring& wide) {
  if (wide.empty())
    return std::string();
  const int len = WideCharToMultiByte(
      CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return std::string();
  std::string out((size_t)len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &out[0], len,
                      nullptr, nullptr);
  return out;
}

// 按行切分名单文本:CRLF / LF / CR 都吃,去掉行首尾的空格 / tab / 宽字符 BOM
// (U+FEFF —— UTF-8 BOM 与 UTF-16LE BOM 解码成 wstring 后都表现为它),跳过空行。
// 与 WeaselPanel::LoadIgnoreList 的字节级契约保持一致,保证 UI 侧历史名单文件
// 在服务端读出来是同一份集合。
inline void ParseIgnoreListText(const std::wstring& content,
                                std::unordered_set<std::wstring>* out) {
  if (!out)
    return;
  // 契约:解析前清空出参。重复调用不许叠加旧内容,否则名单里已被用户移除的
  // 词会残留在集合里继续屏蔽(与 LoadIgnoreListFrom / _RefreshIgnoreList 的
  // clear 形成双保险,helper 自身也必须自洽)。
  out->clear();
  size_t pos = 0;
  while (pos < content.size()) {
    size_t eol = content.find_first_of(L"\r\n", pos);
    if (eol == std::wstring::npos)
      eol = content.size();
    std::wstring line = content.substr(pos, eol - pos);
    while (!line.empty() && (line.back() == L' ' || line.back() == L'\t'))
      line.pop_back();
    while (!line.empty() && (line.front() == L' ' || line.front() == L'\t' ||
                             line.front() == L'\uFEFF'))
      line.erase(line.begin());
    if (!line.empty())
      out->insert(line);
    pos = eol;
    if (pos < content.size() && content[pos] == L'\r')
      pos++;
    if (pos < content.size() && content[pos] == L'\n')
      pos++;
  }
}

// 读取名单。只 open 不 create(OPEN_EXISTING):文件不存在 / 为空 / 读失败一律
// 得到空集合,绝不顺手在用户数据目录里落一个空文件。
// 返回 true 表示确实读到了非空名单。
inline bool LoadIgnoreListFrom(const std::wstring& user_data_dir,
                               const std::wstring& schema_id,
                               std::unordered_set<std::wstring>* out_set) {
  if (out_set)
    out_set->clear();
  if (!out_set)
    return false;
  const std::wstring path = IgnoreListFilePath(user_data_dir, schema_id);
  if (path.empty())
    return false;
  // 共享读+写+删除:名单文件可能同时被托盘「恢复已忽略的候选项」(DeleteFileW)
  // 或另一次右键追加(FILE_APPEND_DATA)触碰,只读打开失败时按空名单处理,
  // 不弹框、不阻塞、不重试(spec 090:读盘失败静默降级)。
  HANDLE h =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  const DWORD file_size = GetFileSize(h, nullptr);
  if (file_size == 0 || file_size == INVALID_FILE_SIZE) {
    CloseHandle(h);
    return false;
  }
  std::vector<unsigned char> raw(file_size);
  DWORD bytes_read = 0;
  const BOOL ok = ReadFile(h, raw.data(), file_size, &bytes_read, nullptr);
  CloseHandle(h);
  if (!ok || bytes_read == 0)
    return false;
  raw.resize(bytes_read);

  std::wstring content;
  if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF) {
    content = FluxingUtf8ToWide(
        std::string((const char*)raw.data() + 3, raw.size() - 3));
  } else if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE) {
    content = std::wstring((const wchar_t*)(raw.data() + 2),
                           (raw.size() - 2) / sizeof(wchar_t));
  } else {
    content =
        FluxingUtf8ToWide(std::string((const char*)raw.data(), raw.size()));
  }
  ParseIgnoreListText(content, out_set);
  return !out_set->empty();
}

// 一次遍历同时产出「过滤后的可见候选」与「可见索引 -> 引擎页内索引」映射。
// FR-006:两者必须由同一次遍历产出,否则页内索引会与引擎错位(spec 090 US3)。
// ignore 为空时退化成恒等映射,行为与未引入屏蔽功能时逐字节一致。
inline void FilterCandidatesAndBuildMap(
    const std::vector<std::wstring>& engine_candidates,
    const std::unordered_set<std::wstring>& ignore,
    std::vector<std::wstring>* out_visible,
    std::vector<size_t>* out_visible_to_engine) {
  if (out_visible)
    out_visible->clear();
  if (out_visible_to_engine)
    out_visible_to_engine->clear();
  if (!out_visible || !out_visible_to_engine)
    return;
  out_visible->reserve(engine_candidates.size());
  out_visible_to_engine->reserve(engine_candidates.size());
  for (size_t engine_index = 0; engine_index < engine_candidates.size();
       ++engine_index) {
    if (!ignore.empty() && ignore.count(engine_candidates[engine_index]) > 0)
      continue;
    out_visible->push_back(engine_candidates[engine_index]);
    out_visible_to_engine->push_back(engine_index);
  }
}

// 可见页内索引 -> 引擎页内索引。越界返回 false 且不改写 *out_engine_index。
// 调用方拿到 false 必须「什么都不做」:不选词、不上屏、不关窗(spec 090 US1)。
inline bool MapVisibleIndexToEngine(
    const std::vector<size_t>& visible_to_engine,
    size_t visible_index,
    size_t* out_engine_index) {
  if (!out_engine_index)
    return false;
  if (visible_index >= visible_to_engine.size())
    return false;
  *out_engine_index = visible_to_engine[visible_index];
  return true;
}

// FR-007(2026-10-30 修订,见 ADR 0023 D5):屏蔽候选时**无条件**写名单,同时
// 也调引擎删除。
//
// 修订理由 —— 原方案想「按引擎删除结果分流」(删得掉就不写名单),但取证证明
// 这条路物理上不可行:
//   - librime/src/rime/context.cc:146-154 的 Context::DeleteCandidate fire 完
//     delete_notifier_ 就 `return true`,上游自己的注释写着
//     "CAVEAT: this doesn't mean anything is deleted for sure";
//   - librime/src/rime/rime_api_impl.h:982-1000 的
//     do_with_candidate_on_current_page 只在「无 session / 无 ctx / 无 menu /
//     无 schema / index >= page_size」时返回 false —— 全是调用姿势错误,
//     没有一条代表「这是系统词,删不掉」;
//   - librime 里不存在 is_user_dict,C API 也没在 rime_candidate_t 里暴露它。
// 所以返回值不含任何可用于分流的信息,只能无条件写:
//   - 自造词:引擎真删掉了,名单里多一行同名记录 —— 无害(它已不会再出现);
//   - 系统词:引擎删不掉,名单这一行正是让它下次不再出现的东西。
// 代价:名单会累积一些其实已被引擎删除的词。撤销入口是既有的托盘
// 「恢复已忽略的候选项」(按 *.user_ignore.txt 批量 DeleteFileW),语义不变。
// 追加一条到名单文件:FILE_APPEND_DATA + OPEN_ALWAYS,UTF-8 + CRLF,
// 与 WeaselPanel::_IgnoreCurrentCandidate 的写法逐字节一致,保证托盘
// 「恢复已忽略的候选项」(按 *.user_ignore.txt 批量 DeleteFileW)能一并清掉。
// 不建目录:用户数据目录不存在就失败返回,不顺手 mkdir。
inline bool AppendToIgnoreList(const std::wstring& user_data_dir,
                               const std::wstring& schema_id,
                               const std::wstring& text) {
  if (text.empty())
    return false;
  const std::wstring path = IgnoreListFilePath(user_data_dir, schema_id);
  if (path.empty())
    return false;
  HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE)
    return false;
  const std::string utf8 = FluxingWideToUtf8(text);
  std::vector<char> line(utf8.begin(), utf8.end());
  line.push_back('\r');
  line.push_back('\n');
  DWORD written = 0;
  const BOOL ok =
      WriteFile(h, line.data(), (DWORD)line.size(), &written, nullptr);
  CloseHandle(h);
  return ok && written == (DWORD)line.size();
}

// 名单文件的「指纹」。用于判断是否需要重读:文件被外部改写(托盘「恢复已忽略
// 的候选项」是直接 DeleteFileW)之后,exists / mtime / size 至少有一项变化,
// 于是下一次出候选列表就会重新读取 —— 不需要重启服务端,也不需要切方案(T7)。
struct IgnoreListStamp {
  bool exists = false;
  unsigned long long mtime = 0;
  unsigned long long size = 0;

  bool operator==(const IgnoreListStamp& other) const {
    return exists == other.exists && mtime == other.mtime && size == other.size;
  }
};

// 取指纹。目录 / 文件不存在时返回全零指纹(== 空名单),绝不创建任何东西。
inline IgnoreListStamp StampIgnoreListFile(const std::wstring& user_data_dir,
                                           const std::wstring& schema_id) {
  IgnoreListStamp stamp;
  const std::wstring path = IgnoreListFilePath(user_data_dir, schema_id);
  if (path.empty())
    return stamp;
  WIN32_FILE_ATTRIBUTE_DATA data = {};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
    return stamp;
  stamp.exists = true;
  stamp.size = ((unsigned long long)data.nFileSizeHigh << 32) |
               (unsigned long long)data.nFileSizeLow;
  stamp.mtime =
      ((unsigned long long)data.ftLastWriteTime.dwHighDateTime << 32) |
      (unsigned long long)data.ftLastWriteTime.dwLowDateTime;
  return stamp;
}

}  // namespace fluxing
