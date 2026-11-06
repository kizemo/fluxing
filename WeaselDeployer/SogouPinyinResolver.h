// SogouPinyinResolver.h — 搜狗词条 → 无调全拼码 的解析器。
//
// 搜狗用户词库（二进制）只存词条，不直接携带拼音；拼音需要由随包词表反查：
//   cn_dicts/base.dict.yaml —— 整词表，物理列序「词条 \t 码 \t 权重」
//   cn_dicts/8105.dict.yaml —— 《通用规范汉字表》单字表，同列序
// 同词多行时取权重最高的一行（多音字按词频决胜）。
//
// 解析顺序：整词命中 base 优先；否则逐字回退 8105（单字表是权威注音）。
// 逐字回退仍缺字时，该词条被判定为不可导入。
//
// 纯 C++17，不依赖任何 Windows API，便于 test/TestSogouUserDict
// 直接编译同一份源文件（宪法 R7）。

#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace weasel {
namespace sogou {

class SogouPinyinResolver {
 public:
  SogouPinyinResolver();

  // 扫描 cn_dicts_dir 下的词表，为 wanted 中的词条建立码表。
  // cn_dicts_dir 是 UTF-8 路径，形如 <共享数据目录>\cn_dicts。
  // 词表文件缺失时返回 false；已经读到的内容仍然有效。
  bool Build(const std::string& cn_dicts_dir,
             const std::unordered_set<std::string>& wanted);

  // 解析词条为「空格分隔的无调全拼码」。整词优先，逐字回退其次。
  // 失败返回 false，code 保持为空串。
  bool Resolve(const std::string& phrase, std::string* code) const;

  // 已建表的整词条数 / 单字条数（诊断用）。
  size_t phrase_count() const { return phrase_count_; }
  size_t char_count() const { return char_count_; }

 private:
  // 词条 → (码, 权重)。权重用于同词多行时择一。
  using WeightMap = std::unordered_map<std::string, std::pair<std::string, double>>;

  bool ScanBase(const std::string& path,
                const std::unordered_set<std::string>& wanted);
  bool ScanChars(const std::string& path);

  WeightMap phrase_map_;  // 整词 → (码, 权重)
  WeightMap char_map_;    // 单字 → (码, 权重)
  size_t phrase_count_ = 0;
  size_t char_count_ = 0;
};

}  // namespace sogou
}  // namespace weasel
