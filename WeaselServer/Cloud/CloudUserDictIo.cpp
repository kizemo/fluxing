// spec 100 阶段 D · T305 用户词典快照的解析 / 生成
//
// 通道语义见 CloudUserDictIo.h 顶部注释（T102 取证结论）：
// 行格式是**快照行**，不是 export_user_dict 的 TSV。
//
// PCH 纪律：本文件不使用预编译头（WeaselServer.vcxproj 逐文件设
// PrecompiledHeader=NotUsing），并在第一行 #define NOMINMAX ——
// 否则 stdafx.h 先于 <algorithm> 注入 windows.h 的 min/max 宏。

#define NOMINMAX

#include "CloudUserDictIo.h"

#include <algorithm>
#include <functional>
#include <map>
#include <vector>

namespace fluxing {
namespace cloud {

namespace {

// 按制表符切分；不丢空字段（尾部空字段也要保留）。
std::vector<std::string> SplitTab(const std::string& line) {
  std::vector<std::string> cols;
  size_t pos = 0;
  while (true) {
    size_t tab = line.find('\t', pos);
    if (tab == std::string::npos) {
      cols.push_back(line.substr(pos));
      break;
    }
    cols.push_back(line.substr(pos, tab - pos));
    pos = tab + 1;
  }
  return cols;
}

CloudMaintenanceFn g_begin_maintenance = nullptr;
CloudMaintenanceFn g_end_maintenance = nullptr;

}  // namespace

bool ParseUserDictSnapshots(const std::string& text,
                            std::vector<UserDictSnapshot>* out,
                            std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：词典快照输出为空。";
    return false;
  }
  out->clear();

  std::map<std::string, size_t> dict_index;  // 词典名 → out 下标
  std::string current_dict;
  size_t current_index = 0;
  bool has_current = false;

  size_t pos = 0;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    const bool last = (nl == std::string::npos);
    std::string line = last ? text.substr(pos) : text.substr(pos, nl - pos);
    pos = last ? text.size() + 1 : nl + 1;

    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.empty())
      continue;

    if (line[0] == '#') {
      // 元信息行："#@<key>\t<value>"；描述行 "# ..." 直接跳过。
      if (line.size() > 2 && line[1] == '@') {
        const std::vector<std::string> cols = SplitTab(line.substr(2));
        if (cols.size() >= 2) {
          if (cols[0] == "/db_name") {
            current_dict = cols[1];
            if (dict_index.find(current_dict) == dict_index.end()) {
              UserDictSnapshot snap;
              snap.dict = current_dict;
              dict_index[current_dict] = out->size();
              out->push_back(snap);
              current_index = out->size() - 1;
            } else {
              current_index = dict_index[current_dict];
            }
            has_current = true;
          } else if (cols[0] == "/tick" && has_current) {
            (*out)[current_index].tick = strtoull(cols[1].c_str(), nullptr, 10);
          }
        }
      }
      continue;
    }

    // 数据行：<code><空格><TAB><phrase><TAB><value>
    const std::vector<std::string> cols = SplitTab(line);
    if (cols.size() < 2 || cols[0].empty() || cols[1].empty())
      continue;
    if (!has_current)
      continue;  // 没有 #@/db_name 的行无法定位词典，跳过

    CloudDictEntry entry;
    entry.text = MakeDictItemKey(current_dict, cols[1]);
    entry.payload = cols[0];
    if (cols.size() >= 3)
      entry.payload += "\t" + cols[2];
    (*out)[current_index].rows.push_back(entry);
  }

  if (out->empty()) {
    if (err)
      *err = "用户词典快照内容为空。";
    return false;
  }
  return true;
}

bool BuildUserDictSnapshots(const std::vector<CloudDictEntry>& rows,
                            std::string* out,
                            std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：词典快照输出为空。";
    return false;
  }
  out->clear();

  // 按词典分组（保持首次出现的顺序）。
  std::vector<std::string> order;
  std::map<std::string, std::vector<const CloudDictEntry*> > grouped;
  for (size_t i = 0; i < rows.size(); ++i) {
    std::string dict;
    std::string phrase;
    if (!SplitDictItemKey(rows[i].text, &dict, &phrase)) {
      if (err)
        *err = "词典条目键格式不正确。";
      return false;
    }
    std::string code;
    std::string value;
    if (!SplitDictPayload(rows[i].payload, &code, &value)) {
      if (err)
        *err = "词典条目载荷格式不正确。";
      return false;
    }
    if (grouped.find(dict) == grouped.end())
      order.push_back(dict);
    grouped[dict].push_back(&rows[i]);
  }

  for (size_t d = 0; d < order.size(); ++d) {
    const std::string& dict = order[d];
    const std::vector<const CloudDictEntry*>& items = grouped[dict];

    uint64_t max_tick = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      std::string code;
      std::string value;
      if (!SplitDictPayload(items[i]->payload, &code, &value))
        continue;
      // value 形如 "c=<n> d=<x> t=<tick>"
      const size_t tp = value.rfind("t=");
      if (tp == std::string::npos)
        continue;
      const uint64_t tick = strtoull(value.c_str() + tp + 2, nullptr, 10);
      if (tick > max_tick)
        max_tick = tick;
    }

    out->append("# Rime user dictionary\n");
    out->append("#@/db_name\t");
    out->append(dict);
    out->append("\n");
    out->append("#@/db_type\tuserdb\n");
    out->append("#@/tick\t");
    out->append(std::to_string(max_tick));
    out->append("\n");
    for (size_t i = 0; i < items.size(); ++i) {
      out->append(JoinDictRow(*items[i]));
      out->append("\n");
    }
  }

  if (out->empty()) {
    if (err)
      *err = "没有可写回的词典条目。";
    return false;
  }
  return true;
}

bool SplitUserDictSnapshotSections(const std::string& text,
                                   std::vector<std::string>* sections,
                                   std::string* err) {
  if (sections == nullptr) {
    if (err)
      *err = "内部错误：快照分段输出为空。";
    return false;
  }
  sections->clear();

  const std::string marker = "#@/db_name\t";
  size_t pos = 0;
  size_t section_start = std::string::npos;
  while (pos <= text.size()) {
    size_t nl = text.find('\n', pos);
    const bool last = (nl == std::string::npos);
    std::string line = last ? text.substr(pos) : text.substr(pos, nl - pos);
    const size_t next = last ? text.size() + 1 : nl + 1;
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.compare(0, marker.size(), marker) == 0) {
      if (section_start != std::string::npos) {
        sections->push_back(text.substr(section_start, pos - section_start));
      }
      section_start = pos;
    }
    pos = next;
  }
  if (section_start != std::string::npos) {
    sections->push_back(text.substr(section_start));
  }

  if (sections->empty()) {
    if (err)
      *err = "快照文本里没有找到词典分段。";
    return false;
  }
  return true;
}

std::string JoinDictRow(const CloudDictEntry& entry) {
  std::string dict;
  std::string phrase;
  if (!SplitDictItemKey(entry.text, &dict, &phrase))
    return std::string();
  std::string code;
  std::string value;
  if (!SplitDictPayload(entry.payload, &code, &value))
    return std::string();
  std::string row = code;
  row += "\t";
  row += phrase;
  row += "\t";
  row += value;
  return row;
}

bool SplitDictRow(const std::string& row,
                  const std::string& dict,
                  CloudDictEntry* out,
                  std::string* err) {
  if (out == nullptr) {
    if (err)
      *err = "内部错误：词典行输出为空。";
    return false;
  }
  const std::vector<std::string> cols = SplitTab(row);
  if (cols.size() < 2 || cols[0].empty() || cols[1].empty()) {
    if (err)
      *err = "词典行缺少码或词条。";
    return false;
  }
  out->text = MakeDictItemKey(dict, cols[1]);
  out->payload = cols[0];
  if (cols.size() >= 3)
    out->payload += "\t" + cols[2];
  return true;
}

std::string MakeDictItemKey(const std::string& dict,
                            const std::string& phrase) {
  std::string key = dict;
  key += "\t";
  key += phrase;
  return key;
}

bool SplitDictItemKey(const std::string& item_key,
                      std::string* dict,
                      std::string* phrase) {
  if (dict == nullptr || phrase == nullptr)
    return false;
  const size_t tab = item_key.find('\t');
  if (tab == std::string::npos || tab == 0 || tab + 1 >= item_key.size())
    return false;
  *dict = item_key.substr(0, tab);
  *phrase = item_key.substr(tab + 1);
  return true;
}

bool SplitDictPayload(const std::string& payload,
                      std::string* code,
                      std::string* value) {
  if (code == nullptr || value == nullptr)
    return false;
  const size_t tab = payload.find('\t');
  if (tab == std::string::npos)
    return false;
  *code = payload.substr(0, tab);
  *value = payload.substr(tab + 1);
  return true;
}

// ---------------------------------------------------------------------------
// 维护态钩子
// ---------------------------------------------------------------------------

void SetCloudMaintenanceHooks(CloudMaintenanceFn begin,
                              CloudMaintenanceFn end) {
  g_begin_maintenance = begin;
  g_end_maintenance = end;
}

bool HasCloudMaintenanceHooks() {
  return g_begin_maintenance != nullptr && g_end_maintenance != nullptr;
}

bool CallCloudBeginMaintenance(std::string* err) {
  if (g_begin_maintenance == nullptr) {
    if (err)
      *err = "尚未接入用户词典维护态，无法安全地读写用户词典。";
    return false;
  }
  return g_begin_maintenance(err);
}

bool CallCloudEndMaintenance(std::string* err) {
  if (g_end_maintenance == nullptr) {
    if (err)
      *err = "尚未接入用户词典维护态，无法安全地读写用户词典。";
    return false;
  }
  return g_end_maintenance(err);
}

}  // namespace cloud
}  // namespace fluxing
