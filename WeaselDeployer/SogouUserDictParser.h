// SogouUserDictParser.h — 搜狗输入法用户词库（二进制）解析器。
//
// 背景：搜狗把用户自造词存进 `sgim_gd_usr.bin` / `sgim_gd_usr_od.bin`
// 等私有二进制文件里，没有公开的导出格式。本文件只做「读结构」，
// 不复制搜狗的任何代码或数据资产进仓库（ROADMAP §6.2 红线）：
// 解析器只在用户本机运行时读取搜狗的数据文件。
//
// 搜狗有两套用户词库文件，布局互不相同，解析器同时支持两套语法：
//
//   语法 A —— 账号态 sgim_gd_usr_od.bin（约 460 KB）：
//     [u16 A][u16 B][词条: B 字节, UTF-16LE][后缀 10+2K 字节]
//       A == B + 2
//       B == 词条的字节数（必为偶数）
//       后缀 = [u16 flag=1][u16 记录序号][u32 保留=0][u16 K][u16 × K 音节索引]
//       K == 词条字数时后缀可信；否则只取词条、丢弃音节索引。
//
//   语法 B —— 普通态主词库 sgim_gd_usr.bin（约 3.8 MB，文件头 "SGPU"）：
//     [u16 B][B 字节 音节索引][u16 X][u16 B][词条: B 字节, UTF-16LE][u16 B][B 字节 音节索引]
//       B == 词条的字节数（必为偶数，2..64）
//       X == 2B + 4（恰好覆盖 [u16 B][词条][u16 B][音节索引] 这 2B+4 字节）
//       末尾音节索引与开头音节索引逐字节相同 —— 这是该语法最强的自校验
//       判据，随机数据误命中的概率可忽略。
//
// 词条约束：1..32 个汉字（U+3400..U+9FFF 等 CJK 统一表意文字）。
// 搜狗里也存了纯英文的自造词（node / OAuth 之类），但 Rime 侧码空间是
// 「无调全拼 + 空格」，英文词条落不到任何候选上，故一律不导入。
//
// 纯 C++17，不依赖任何 Windows API（除 <filesystem> 读目录外），
// 这样 test/TestSogouUserDict 可以直接编译同一份源文件（宪法 R7）。

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace weasel {
namespace sogou {

// 一条搜狗用户词库记录。
struct SogouEntry {
  std::string phrase;               // UTF-8 词条
  std::vector<uint16_t> syllables;  // 音节索引（原始值，未解码）
};

// 解析统计。
struct SogouParseStats {
  size_t bytes_scanned = 0;   // 扫描字节数
  size_t record_count = 0;    // 命中的记录数（词条去重后）
  size_t skipped = 0;         // 命中签名但后缀不可信而被降级的记录数
  size_t duplicates = 0;      // 词条重复而被合并的记录数
  size_t rejected = 0;        // 命中签名但词条含非 CJK 而被丢弃的记录数
};

// 解析搜狗用户词库二进制。data/size 为整个文件的内容。
// 同时支持语法 A（账号态）与语法 B（普通态 SGPU），命中判据均为
// 「签名扫描」，不依赖任何硬编码偏移：
//   语法 A：u16 A == u16 B + 2，B 为偶数且 2..64，其后 B 字节是全 CJK 的 UTF-16LE。
//   语法 B：u16 B 为偶数且 2..64，两处音节索引逐字节相同且 X == 2B + 4，
//           其间的 B 字节是全 CJK 的 UTF-16LE。
// 该判据在 CJK 密集区不可能误命中：CJK 码位 >= 0x3400，远大于 64。
bool ParseSogouUserDict(const unsigned char* data,
                        size_t size,
                        SogouParseStats* stats,
                        std::vector<SogouEntry>* entries);

// 读整个文件到内存。失败返回 false。
bool ReadWholeFile(const std::string& path, std::vector<unsigned char>* out);

// 搜狗用户词库候选文件名（普通态 / 账号态各一套目录）。
// 返回的是文件名，不含路径。
extern const char* const kSogouUserDictFileNames[];
extern const size_t kSogouUserDictFileNameCount;

}  // namespace sogou
}  // namespace weasel
