// TestSogouUserDict.cpp — 「导入搜狗输入法用户词库」单元测试。
//
// 被测生产代码（直接编译同一份源文件，宪法 R7；不链接 WeaselDeployer 工程）：
//   WeaselDeployer/SogouUserDictParser.cpp —— 搜狗用户词库二进制解析
//   WeaselDeployer/SogouPinyinResolver.cpp —— 词条 → 无调全拼码解析
//
// 红线（ROADMAP §6.2）：本文件不依赖本机搜狗真实文件，也不包含任何搜狗
// 二进制/词库资产；所有 fixture 均在测试内合成。合成词表用 UTF-8 无 BOM
// 写出，行尾分别覆盖 LF 与 CRLF 两种真实形态。
//
// 构建（产物落在 <repo>\release\TestSogouUserDict.exe）：
//   msbuild test\TestSogouUserDict\TestSogouUserDict.vcxproj /t:Build
//            /p:Configuration=Release /p:Platform=Win32
//            /p:SolutionDir=F:\soft\00selfmade\rime_claude

#include "SogouPinyinResolver.h"
#include "SogouUserDictParser.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_pass = 0;
int g_fail = 0;

// 把音节索引数组格式化成可打印的十六进制列表，便于失败时定位。
std::string HexList(const std::vector<uint16_t>& v) {
  std::string out = "[";
  char buf[16];
  for (size_t i = 0; i < v.size(); ++i) {
    if (i != 0) {
      out += ", ";
    }
    std::snprintf(buf, sizeof(buf), "0x%04X", v[i]);
    out += buf;
  }
  out += "]";
  return out;
}

void Check(const std::string& desc, bool ok) {
  if (ok) {
    ++g_pass;
    std::cout << "  PASS: " << desc << std::endl;
  } else {
    ++g_fail;
    std::cout << "  FAIL: " << desc << std::endl;
  }
}

void CheckEqStr(const std::string& desc,
                const std::string& expected,
                const std::string& actual) {
  if (expected == actual) {
    ++g_pass;
    std::cout << "  PASS: " << desc << std::endl;
  } else {
    ++g_fail;
    std::cout << "  FAIL: " << desc << " (期望: [" << expected << "] 实际: ["
              << actual << "])" << std::endl;
  }
}

void CheckEqNum(const std::string& desc,
                unsigned long long expected,
                unsigned long long actual) {
  if (expected == actual) {
    ++g_pass;
    std::cout << "  PASS: " << desc << std::endl;
  } else {
    ++g_fail;
    std::cout << "  FAIL: " << desc << " (期望: " << expected
              << " 实际: " << actual << ")" << std::endl;
  }
}

void CheckEqSyl(const std::string& desc,
                const std::vector<uint16_t>& expected,
                const std::vector<uint16_t>& actual) {
  if (expected == actual) {
    ++g_pass;
    std::cout << "  PASS: " << desc << std::endl;
  } else {
    ++g_fail;
    std::cout << "  FAIL: " << desc << " (期望: " << HexList(expected)
              << " 实际: " << HexList(actual) << ")" << std::endl;
  }
}

// ---------------------------------------------------------------------------
// 合成搜狗用户词库二进制
// ---------------------------------------------------------------------------

void PushU16(std::vector<unsigned char>& buf, uint16_t v) {
  buf.push_back(static_cast<unsigned char>(v & 0xFF));
  buf.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
}

void PushU32(std::vector<unsigned char>& buf, uint32_t v) {
  PushU16(buf, static_cast<uint16_t>(v & 0xFFFF));
  PushU16(buf, static_cast<uint16_t>((v >> 16) & 0xFFFF));
}

// 合成一条搜狗用户词库记录并追加到 buf。
//   [u16 A][u16 B][词条: B 字节 UTF-16LE][后缀]
//   后缀 = [u16 flag=1][u16 seq][u32 保留][u16 K][u16 × K 音节索引]
// a_override >= 0：用它替代 A（负例：A != B+2）
// b_override >= 0：用它替代 B（负例：B 为奇数 / B 超过 64）
// k_override >= 0：用它替代后缀里的 K（负例：K != 词条字数）
// non_cjk       ：词条首字符换成 ASCII 'A'（负例：词条含非 CJK）
void AppendRecord(std::vector<unsigned char>& buf,
                  const std::vector<uint16_t>& phrase,
                  const std::vector<uint16_t>& syllables,
                  int a_override = -1,
                  int b_override = -1,
                  int k_override = -1,
                  bool non_cjk = false) {
  std::vector<uint16_t> cp = phrase;
  if (non_cjk && !cp.empty()) {
    cp[0] = 0x0041;  // ASCII 'A'，不属于 CJK
  }
  const uint16_t b =
      (b_override >= 0) ? static_cast<uint16_t>(b_override)
                        : static_cast<uint16_t>(cp.size() * 2);
  const uint16_t a =
      (a_override >= 0) ? static_cast<uint16_t>(a_override)
                        : static_cast<uint16_t>(b + 2);
  PushU16(buf, a);
  PushU16(buf, b);
  for (uint16_t c : cp) {
    PushU16(buf, c);
  }
  PushU16(buf, 1);      // flag
  PushU16(buf, 0x0001);  // 记录序号
  PushU32(buf, 0);      // 保留字段
  const uint16_t k =
      (k_override >= 0) ? static_cast<uint16_t>(k_override)
                        : static_cast<uint16_t>(syllables.size());
  PushU16(buf, k);
  for (uint16_t s : syllables) {
    PushU16(buf, s);
  }
}

// 常用汉字码位（仅用于合成 fixture，不含任何搜狗资产）。
const uint16_t kNi = 0x4F60;   // 你
const uint16_t kHao = 0x597D;  // 好
const uint16_t kShu = 0x8F93;  // 输
const uint16_t kRu = 0x5165;   // 入
const uint16_t kFa = 0x6CD5;   // 法
const uint16_t kHuo = 0x706B;  // 火
const uint16_t kLiu = 0x6D41;  // 流
const uint16_t kXing = 0x7329; // 猩

// ---------------------------------------------------------------------------
// A. 搜狗二进制解析器
// ---------------------------------------------------------------------------

void TestParserValidRecords() {
  std::cout << "[A] 合成搜狗二进制 → 合法记录" << std::endl;

  std::vector<unsigned char> buf;
  AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002});
  AppendRecord(buf, {kShu, kRu, kFa}, {0x0003, 0x0004, 0x0005});
  AppendRecord(buf, {kHuo, kLiu, kXing}, {0x0006, 0x0007, 0x0008});

  weasel::sogou::SogouParseStats stats;
  std::vector<weasel::sogou::SogouEntry> entries;
  const bool ok = weasel::sogou::ParseSogouUserDict(
      buf.data(), buf.size(), &stats, &entries);

  Check("A1 解析返回成功", ok);
  CheckEqNum("A2 命中记录数 == 3", 3, stats.record_count);
  CheckEqNum("A3 取回词条数 == 3", 3, entries.size());
  CheckEqStr("A4 首条词条 == 你好", "你好",
             entries.size() > 0 ? entries[0].phrase : std::string());
  CheckEqStr("A5 次条词条 == 输入法", "输入法",
             entries.size() > 1 ? entries[1].phrase : std::string());
  CheckEqStr("A6 末条词条 == 火流猩", "火流猩",
             entries.size() > 2 ? entries[2].phrase : std::string());
  CheckEqSyl("A7 首条音节索引被采信", {0x0001, 0x0002},
             entries.size() > 0 ? entries[0].syllables
                                : std::vector<uint16_t>());
  CheckEqNum("A8 三字词的音节数 == 3", 3,
             entries.size() > 1 ? entries[1].syllables.size() : 0u);
  CheckEqNum("A9 未发生降级 (skipped == 0)", 0, stats.skipped);
  CheckEqNum("A10 扫描字节数 == 缓冲区长度", buf.size(), stats.bytes_scanned);
}

void TestParserNegativeRecords() {
  std::cout << "[B] 负例：签名不成立的记录必须被拒" << std::endl;

  // B 组 1：A == B + 1（签名 A != B+2）
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002},
                 /*a_override=*/0x0005, /*b_override=*/0x0004);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    const bool ok = weasel::sogou::ParseSogouUserDict(
        buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B1 A == B+1 的记录不被采信 (record_count == 0)", 0,
               stats.record_count);
    Check("B2 A == B+1 的记录解析返回失败", !ok);
  }

  // B 组 2：A == B + 3（另一个方向的不匹配）
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002},
                 /*a_override=*/0x0007, /*b_override=*/0x0004);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B3 A == B+3 的记录不被采信 (record_count == 0)", 0,
               stats.record_count);
  }

  // B 组 3：B 为奇数
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002},
                 /*a_override=*/-1, /*b_override=*/0x0003);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B4 B 为奇数的记录不被采信 (record_count == 0)", 0,
               stats.record_count);
  }

  // B 组 4：B 超过 64
  {
    std::vector<uint16_t> long_phrase;
    for (int i = 0; i < 33; ++i) {
      long_phrase.push_back(static_cast<uint16_t>(0x4E00 + i));
    }
    std::vector<unsigned char> buf;
    AppendRecord(buf, long_phrase, {0x0001},
                 /*a_override=*/-1, /*b_override=*/0x0042);  // 66 > 64
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B5 B 超过 64 的记录不被采信 (record_count == 0)", 0,
               stats.record_count);
  }

  // B 组 5：词条含非 CJK 字符
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002}, /*a_override=*/-1,
                 /*b_override=*/-1, /*k_override=*/-1, /*non_cjk=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B6 词条含非 CJK 的记录不被采信 (record_count == 0)", 0,
               stats.record_count);
  }

  // B 组 6：混合缓冲区 —— 合法 + 非法 + 合法
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001, 0x0002});
    AppendRecord(buf, {kShu, kRu, kFa}, {0x0003},
                 /*a_override=*/0x0007, /*b_override=*/0x0006);  // A != B+2
    AppendRecord(buf, {kHuo, kLiu, kXing}, {0x0006, 0x0007, 0x0008});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("B7 混合缓冲区只命中 2 条合法记录", 2, stats.record_count);
    CheckEqStr("B8 混合缓冲区取回的第一条 == 你好",
               "你好", entries.size() > 0 ? entries[0].phrase : std::string());
    CheckEqStr("B9 混合缓冲区取回的第二条 == 火流猩", "火流猩",
               entries.size() > 1 ? entries[1].phrase : std::string());
  }
}

void TestParserSuffixTrust() {
  std::cout << "[C] 后缀可信度：K != 字数降级 / K == 字数采信" << std::endl;

  // C 组 1：K(1) != 字数(2) → 词条保留，音节索引丢弃，skipped 计数 +1
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kNi, kHao}, {0x0001},
                 /*a_override=*/-1, /*b_override=*/-1, /*k_override=*/0x0001);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("C1 K != 字数时词条仍被保留 (record_count == 1)", 1,
               stats.record_count);
    CheckEqStr("C2 K != 字数时词条内容仍正确", "你好",
               entries.size() > 0 ? entries[0].phrase : std::string());
    CheckEqNum("C3 K != 字数时音节索引为空", 0,
               entries.size() > 0 ? entries[0].syllables.size() : 1u);
    CheckEqNum("C4 K != 字数时 skipped == 1", 1, stats.skipped);
  }

  // C 组 2：K(3) == 字数(3) → 音节索引被采信
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kShu, kRu, kFa}, {0x0003, 0x0004, 0x0005});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("C5 K == 字数时音节数 == 字数", 3,
               entries.size() > 0 ? entries[0].syllables.size() : 0u);
    CheckEqSyl("C6 K == 字数时音节索引值逐个相等", {0x0003, 0x0004, 0x0005},
               entries.size() > 0 ? entries[0].syllables
                                  : std::vector<uint16_t>());
    CheckEqNum("C7 K == 字数时不计为 skipped", 0, stats.skipped);
  }
}

void TestParserIoAndNames() {
  std::cout << "[D] 空输入 / 读文件 / 候选文件名" << std::endl;

  {
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    const bool ok = weasel::sogou::ParseSogouUserDict(
        nullptr, 0, &stats, &entries);
    Check("D1 空指针输入返回失败", !ok);
    CheckEqNum("D2 空指针输入不产生记录", 0, stats.record_count);
  }
  {
    const unsigned char dummy[4] = {0, 0, 0, 0};
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    const bool ok = weasel::sogou::ParseSogouUserDict(
        dummy, 0, &stats, &entries);
    Check("D3 长度为零的输入返回失败", !ok);
  }

  // ReadWholeFile：写临时文件 → 读回 → 比对
  {
    const fs::path dir = fs::temp_directory_path() / "TestSogouUserDict_io";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const fs::path file = dir / "sample.bin";
    const std::vector<unsigned char> content = {0x00, 0x7F, 0x80, 0xFF, 0x01};
    {
      std::ofstream out(file, std::ios::binary | std::ios::trunc);
      out.write(reinterpret_cast<const char*>(content.data()),
                static_cast<std::streamsize>(content.size()));
    }
    std::vector<unsigned char> read_back;
    const bool ok = weasel::sogou::ReadWholeFile(file.string(), &read_back);
    Check("D4 ReadWholeFile 读取存在的文件成功", ok);
    CheckEqNum("D5 ReadWholeFile 读回长度一致", content.size(),
               read_back.size());
    bool same = read_back.size() == content.size();
    for (size_t i = 0; same && i < content.size(); ++i) {
      same = (read_back[i] == content[i]);
    }
    Check("D6 ReadWholeFile 读回内容逐字节一致", same);

    std::vector<unsigned char> missing;
    Check("D7 ReadWholeFile 读不存在的文件失败",
          !weasel::sogou::ReadWholeFile(
              (dir / "nope.bin").string(), &missing));

    const fs::path empty_file = dir / "empty.bin";
    { std::ofstream out(empty_file, std::ios::binary | std::ios::trunc); }
    std::vector<unsigned char> empty_out;
    Check("D8 ReadWholeFile 读空文件失败",
          !weasel::sogou::ReadWholeFile(empty_file.string(), &empty_out));
    fs::remove_all(dir, ec);
  }

  CheckEqNum("D9 搜狗用户词库候选文件名数量 == 2",
             weasel::sogou::kSogouUserDictFileNameCount, 2);
  bool has_usr = false;
  for (size_t i = 0; i < weasel::sogou::kSogouUserDictFileNameCount; ++i) {
    const std::string name = weasel::sogou::kSogouUserDictFileNames[i];
    if (name == "sgim_gd_usr.bin") {
      has_usr = true;
    }
  }
  Check("D10 候选文件名包含 sgim_gd_usr.bin", has_usr);
}

// ---------------------------------------------------------------------------
// E. 拼音解析器
// ---------------------------------------------------------------------------

// 以 UTF-8 无 BOM 写出合成词表。
void WriteUtf8NoBom(const fs::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// 合成词表内容（物理列序：词条 \t 码 [\t 权重]）。
// base 用 CRLF 行尾（覆盖「只剥行尾 \r」），8105 用 LF 行尾。
std::string BaseDictContent() {
  std::string s;
  s += "# 合成测试词表（单元测试自造，不含任何搜狗真实数据）\r\n";
  s += "# 物理列序：词条\t码[\t权重]\r\n";
  s += "啊啊\ta a\t27365\r\n";          // 三列行：码内部含空格
  s += "爸爸\tba ba\r\n";               // 两列行：码内部含空格 + 行尾 \r
  s += "你好\tni hao\t900\r\n";
  s += "行\txing\t100\r\n";             // 同词多行，权重低
  s += "行\thang\t900\r\n";             // 同词多行，权重高
  s += "与查询无关\twu guan xi\t5\r\n";  // 不在 wanted 里
  return s;
}

std::string CharDictContent() {
  std::string s;
  s += "# 合成单字表（单元测试自造，不含任何搜狗真实数据）\n";
  s += "你\tni\t17648808\n";
  s += "好\thao\t5000\n";
  s += "啊\ta\t3000\n";
  s += "啊\ta a\t10\n";  // 同词多行，权重低
  s += "爸\tba\t4000\n";
  s += "行\txing\t100\n";  // 单字表里的行，base 命中时应让位给 base
  s += "你你\tni ni\t999\n";  // 两字行：单字表只收单字
  return s;
}

fs::path MakeTempDir() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path dir =
      fs::temp_directory_path() / ("TestSogouUserDict_" + std::to_string(stamp));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  return dir;
}

void TestResolver() {
  std::cout << "[E] 拼音解析器：整词命中 / 权重 / 逐字回退" << std::endl;

  const fs::path dir = MakeTempDir();
  const fs::path cn_dicts = dir / "cn_dicts";
  std::error_code ec;
  fs::create_directories(cn_dicts, ec);
  WriteUtf8NoBom(cn_dicts / "base.dict.yaml", BaseDictContent());
  WriteUtf8NoBom(cn_dicts / "8105.dict.yaml", CharDictContent());

  std::unordered_set<std::string> wanted = {"啊啊", "爸爸", "你好", "行", "好啊"};
  weasel::sogou::SogouPinyinResolver resolver;
  const bool built = resolver.Build(cn_dicts.u8string(), wanted);

  Check("E1 Build 读取两份词表成功", built);
  CheckEqNum("E2 整词表只收 wanted 里的词（与查询无关被过滤）", 4,
             resolver.phrase_count());
  CheckEqNum("E3 单字表只收单字（两字行被排除）", 5, resolver.char_count());

  std::string code;
  Check("E4 整词命中：啊啊",
        resolver.Resolve("啊啊", &code) && code == "a a");
  CheckEqStr("E5 整词命中：码内部空格原样保留", "a a", code);

  Check("E6 两列行：爸爸",
        resolver.Resolve("爸爸", &code) && code == "ba ba");
  CheckEqStr("E7 两列行：行尾 \\r 已剥除且码内空格保留", "ba ba", code);

  Check("E8 整词命中：你好",
        resolver.Resolve("你好", &code) && code == "ni hao");

  Check("E9 同词多行取权重最高：行 → hang",
        resolver.Resolve("行", &code) && code == "hang");
  Check("E10 整词表优先于单字表：行 ≠ xing", code != "xing");

  Check("E11 整词未命中时逐字回退：好啊 → hao a",
        resolver.Resolve("好啊", &code) && code == "hao a");
  CheckEqStr("E12 逐字回退：单字表同词多行取权重最高（啊 取权重更高的 a）",
             "hao a", code);

  code = "SENTINEL";
  Check("E13 缺字时解析失败：好龘", !resolver.Resolve("好龘", &code));
  CheckEqStr("E14 缺字时输出码为空串", "", code);

  Check("E15 空词条解析失败", !resolver.Resolve("", &code));
  Check("E16 空码指针解析失败", !resolver.Resolve("你好", nullptr));

  Check("E17 不在 wanted 的词条无法解析",
        !resolver.Resolve("与查询无关", &code));

  // 词表缺失时的契约：Build 返回 false，但已读到的内容仍然有效。
  {
    const fs::path partial = dir / "cn_dicts_partial";
    fs::create_directories(partial, ec);
    WriteUtf8NoBom(partial / "base.dict.yaml", BaseDictContent());
    weasel::sogou::SogouPinyinResolver r2;
    std::unordered_set<std::string> w2 = {"你好"};
    const bool ok2 = r2.Build(partial.u8string(), w2);
    Check("E18 缺 8105 词表时 Build 返回失败", !ok2);
    std::string c2;
    Check("E19 缺 8105 词表时已读到的整词仍可用",
          r2.Resolve("你好", &c2) && c2 == "ni hao");
  }
  {
    weasel::sogou::SogouPinyinResolver r3;
    std::unordered_set<std::string> w3 = {"你好"};
    Check("E20 词表目录不存在时 Build 返回失败",
          !r3.Build((dir / "no_such_dir").u8string(), w3));
  }

  fs::remove_all(dir, ec);
}

// ---------------------------------------------------------------------------
// F. 普通态 SGPU 主词库（语法 B）
// ---------------------------------------------------------------------------

// 合成一条 SGPU（语法 B）记录并追加到 buf：
//   [u16 B][B 字节 音节索引][u16 X][u16 B][词条][u16 B][B 字节 音节索引]
//   X == 2B + 4；两处音节索引逐字节相同。
// 负例开关：break_x / break_b2 / break_b3 / break_syll / non_cjk。
void AppendRecordB(std::vector<unsigned char>& buf,
                   const std::vector<uint16_t>& phrase,
                   const std::vector<uint16_t>& syllables,
                   bool break_x = false,
                   bool break_b2 = false,
                   bool break_b3 = false,
                   bool break_syll = false,
                   bool non_cjk = false) {
  std::vector<uint16_t> cp = phrase;
  if (non_cjk && !cp.empty()) {
    cp[0] = 0x0041;  // ASCII 'A'，不属于 CJK
  }
  const uint16_t b = static_cast<uint16_t>(cp.size() * 2);
  PushU16(buf, b);
  for (uint16_t s : syllables) {
    PushU16(buf, s);
  }
  PushU16(buf, break_x ? static_cast<uint16_t>(2 * b + 6)
                       : static_cast<uint16_t>(2 * b + 4));
  PushU16(buf, break_b2 ? static_cast<uint16_t>(b + 2) : b);
  for (uint16_t c : cp) {
    PushU16(buf, c);
  }
  PushU16(buf, break_b3 ? static_cast<uint16_t>(b + 2) : b);
  std::vector<uint16_t> tail = syllables;
  if (break_syll && !tail.empty()) {
    tail[0] = static_cast<uint16_t>(tail[0] ^ 0x0001);
  }
  for (uint16_t s : tail) {
    PushU16(buf, s);
  }
}

void TestParserGrammarB() {
  std::cout << "[F] 普通态 SGPU 主词库（语法 B）" << std::endl;

  // F 组 1：三条合法记录
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002});
    AppendRecordB(buf, {kShu, kRu, kFa}, {0x0003, 0x0004, 0x0005});
    AppendRecordB(buf, {kHuo, kLiu, kXing}, {0x0006, 0x0007, 0x0008});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    const bool ok = weasel::sogou::ParseSogouUserDict(
        buf.data(), buf.size(), &stats, &entries);
    Check("F1 语法 B 解析返回成功", ok);
    CheckEqNum("F2 语法 B 命中记录数 == 3", 3, stats.record_count);
    CheckEqStr("F3 语法 B 首条词条 == 你好", "你好",
               entries.size() > 0 ? entries[0].phrase : std::string());
    CheckEqStr("F4 语法 B 末条词条 == 火流猩", "火流猩",
               entries.size() > 2 ? entries[2].phrase : std::string());
    CheckEqSyl("F5 语法 B 音节索引被采信", {0x0003, 0x0004, 0x0005},
               entries.size() > 1 ? entries[1].syllables
                                  : std::vector<uint16_t>());
    CheckEqNum("F6 语法 B 不计为 skipped", 0, stats.skipped);
    CheckEqNum("F7 语法 B 不产生被拒记录", 0, stats.rejected);
  }

  // F 组 2：X != 2B + 4
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002}, /*break_x=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F8 X != 2B+4 的记录不被采信", 0, stats.record_count);
  }

  // F 组 3：第二个 B 与 B 不一致
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002},
                  /*break_x=*/false, /*break_b2=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F9 第二个 B 不一致的记录不被采信", 0, stats.record_count);
  }

  // F 组 4：第三个 B 与 B 不一致
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002},
                  /*break_x=*/false, /*break_b2=*/false, /*break_b3=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F10 第三个 B 不一致的记录不被采信", 0, stats.record_count);
  }

  // F 组 5：两处音节索引不一致
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002},
                  /*break_x=*/false, /*break_b2=*/false, /*break_b3=*/false,
                  /*break_syll=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F11 两处音节索引不一致的记录不被采信", 0,
               stats.record_count);
  }

  // F 组 6：词条含非 CJK → 被拒（不进词条表，但计入 rejected）
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002},
                  /*break_x=*/false, /*break_b2=*/false, /*break_b3=*/false,
                  /*break_syll=*/false, /*non_cjk=*/true);
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F12 词条含非 CJK 的记录不进词条表", 0, stats.record_count);
    CheckEqNum("F13 词条含非 CJK 的记录计入 rejected", 1, stats.rejected);
  }

  // F 组 7：重复词条被合并
  {
    std::vector<unsigned char> buf;
    AppendRecordB(buf, {kNi, kHao}, {0x0001, 0x0002});
    AppendRecordB(buf, {kNi, kHao}, {0x0003, 0x0004});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F14 重复词条只保留一条", 1, stats.record_count);
    CheckEqNum("F15 重复词条计入 duplicates", 1, stats.duplicates);
  }

  // F 组 8：语法 A 与语法 B 混在同一缓冲区，两者都要能解析
  {
    std::vector<unsigned char> buf;
    AppendRecord(buf, {kShu, kRu, kFa}, {0x0003, 0x0004, 0x0005});
    AppendRecordB(buf, {kHuo, kLiu, kXing}, {0x0006, 0x0007, 0x0008});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F16 两种语法混排时都命中", 2, stats.record_count);
    CheckEqStr("F17 混排时语法 A 词条在前", "输入法",
               entries.size() > 0 ? entries[0].phrase : std::string());
    CheckEqStr("F18 混排时语法 B 词条在后", "火流猩",
               entries.size() > 1 ? entries[1].phrase : std::string());
  }

  // F 组 9：16 字词条（B = 32）必须被接受
  {
    std::vector<uint16_t> long_phrase;
    for (int i = 0; i < 16; ++i) {
      long_phrase.push_back(static_cast<uint16_t>(0x4E00 + i));
    }
    std::vector<unsigned char> buf;
    AppendRecordB(buf, long_phrase, {0x0001, 0x0002, 0x0003, 0x0004,
                                      0x0005, 0x0006, 0x0007, 0x0008,
                                      0x0009, 0x000A, 0x000B, 0x000C,
                                      0x000D, 0x000E, 0x000F, 0x0010});
    weasel::sogou::SogouParseStats stats;
    std::vector<weasel::sogou::SogouEntry> entries;
    weasel::sogou::ParseSogouUserDict(buf.data(), buf.size(), &stats, &entries);
    CheckEqNum("F19 16 字词条被接受", 1, stats.record_count);
    CheckEqNum("F20 16 字词条的音节数 == 16", 16,
               entries.size() > 0 ? entries[0].syllables.size() : 0u);
  }
}

}  // namespace

int main() {
  std::cout << "TestSogouUserDict: 搜狗用户词库导入单元测试" << std::endl;

  TestParserValidRecords();
  TestParserNegativeRecords();
  TestParserSuffixTrust();
  TestParserGrammarB();
  TestParserIoAndNames();
  TestResolver();

  std::cout << std::endl;
  std::cout << "断言总数: " << (g_pass + g_fail) << "（通过 " << g_pass
            << " / 失败 " << g_fail << "）" << std::endl;
  if (g_fail == 0) {
    std::cout << "TESTS PASSED" << std::endl;
    std::cout << "FAILURES=0" << std::endl;
    return 0;
  }
  std::cout << "TESTS FAILED" << std::endl;
  std::cout << "FAILURES=" << g_fail << std::endl;
  return 1;
}
