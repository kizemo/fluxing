// TestServerSideIgnoreMapping.cpp : spec 090 (2026-10-30)
//
// 服务端候选屏蔽名单的纯函数契约测试。覆盖 include/FluxingCandidateIgnore.h
// 与 include/ShiftStateMachine.h 里三个被服务端复用的纯函数:
//   - FilterCandidatesAndBuildMap / MapVisibleIndexToEngine(可见索引 ->
//   引擎索引)
//   - IsPageLocalIndexValid(Shift 上屏的页内校验)
//   - IgnoreListFilePath / ParseIgnoreListText(名单路径与名单文本解析)
//
// 两个头文件都是 header-only、只依赖 windows.h + STL,所以本工程不需要链接
// 任何 .cpp(与 TestCandidateIgnoreFilter.cpp 同一条纪律:不链接
// WeaselPanel.cpp / RimeWithWeasel.cpp,避免拖进 WTL/ATL/Gdiplus 与 librime)。
// 断言模式沿用 boost::detail::lightweight_test。
//
// 最后一组是「坏数据负对照」:如果名单被误当成空名单使用,映射就不会变短、
// 映射后取到的原文就会等于原索引取到的原文 —— 那两条断言会立刻失败。

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <FluxingCandidateIgnore.h>
#include <ShiftStateMachine.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::vector<std::wstring> EnginePage() {
  // 引擎一页 5 个候选(0-based 引擎页内索引 0..4)。
  return {L"甲", L"乙", L"丙", L"丁", L"戊"};
}

}  // namespace

// spec 090: 与 TestCandidateIgnoreFilter.cpp 共用一个 main() 入口,
// 否则同工程两个 _main 触发 LNK2005 / LNK1169。
int RunServerSideIgnoreMappingTests() {
  // T1: 空名单退化成恒等映射 —— 可见候选与引擎候选逐项相同,映射为 0..N-1。
  //     这是「未启用屏蔽功能时行为逐字节不变」的保证(FR-005)。
  {
    std::unordered_set<std::wstring> ignore;  // 空
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST_EQ(visible.size(), 5u);
    BOOST_TEST_EQ(map.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
      BOOST_TEST_EQ(map[i], i);
      BOOST_TEST(visible[i] == EnginePage()[i]);
    }
    // 恒等映射下,可见索引与引擎索引一致。
    size_t engine = 999;
    BOOST_TEST(fluxing::MapVisibleIndexToEngine(map, 3, &engine));
    BOOST_TEST_EQ(engine, 3u);
    std::cout << "  PASS: T1 空名单 -> 恒等映射" << std::endl;
  }

  // T2: 屏蔽首项 —— 可见列表少一项,映射整体后移。
  {
    std::unordered_set<std::wstring> ignore{L"甲"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST_EQ(visible.size(), 4u);
    BOOST_TEST_EQ(map.size(), 4u);
    BOOST_TEST(visible[0] == L"乙");
    BOOST_TEST_EQ(map[0], 1u);  // 可见 0 -> 引擎 1
    BOOST_TEST_EQ(map[3], 4u);  // 可见 3 -> 引擎 4
    size_t engine = 999;
    BOOST_TEST(fluxing::MapVisibleIndexToEngine(map, 0, &engine));
    BOOST_TEST_EQ(engine, 1u);
    // 可见索引 4 已越界(只剩 4 项),必须返回 false 且不改写出参。
    engine = 999;
    BOOST_TEST(!fluxing::MapVisibleIndexToEngine(map, 4, &engine));
    BOOST_TEST_EQ(engine, 999u);
    std::cout << "  PASS: T2 屏蔽首项 -> 映射后移一位" << std::endl;
  }

  // T3: 屏蔽中间项 —— 映射在屏蔽点处出现空洞。
  {
    std::unordered_set<std::wstring> ignore{L"丙"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST_EQ(visible.size(), 4u);
    BOOST_TEST_EQ(map[0], 0u);
    BOOST_TEST_EQ(map[1], 1u);
    BOOST_TEST_EQ(map[2], 3u);  // 跳过引擎索引 2
    BOOST_TEST_EQ(map[3], 4u);
    BOOST_TEST(visible[2] == L"丁");
    std::cout << "  PASS: T3 屏蔽中间项 -> 映射出现空洞" << std::endl;
  }

  // T4: 屏蔽末项。
  {
    std::unordered_set<std::wstring> ignore{L"戊"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST_EQ(visible.size(), 4u);
    BOOST_TEST_EQ(map[3], 3u);
    BOOST_TEST(visible[3] == L"丁");
    std::cout << "  PASS: T4 屏蔽末项" << std::endl;
  }

  // T5: 全部屏蔽 —— 可见列表与映射都空;任何可见索引都越界返回 false。
  //     调用方拿到 false 必须「什么都不做」:不选词、不上屏、不关窗。
  {
    std::unordered_set<std::wstring> ignore{L"甲", L"乙", L"丙", L"丁", L"戊"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST(visible.empty());
    BOOST_TEST(map.empty());
    size_t engine = 999;
    BOOST_TEST(!fluxing::MapVisibleIndexToEngine(map, 0, &engine));
    BOOST_TEST_EQ(engine, 999u);
    // 空出参指针也必须安全返回 false,不许崩。
    BOOST_TEST(!fluxing::MapVisibleIndexToEngine(map, 0, nullptr));
    std::cout << "  PASS: T5 全部屏蔽 -> 空映射 + 越界 false" << std::endl;
  }

  // T6: 名单里有当前页不存在的词 —— 不影响本页映射(名单是全局的,按词匹配)。
  {
    std::unordered_set<std::wstring> ignore{L"不在本页的词", L"乙"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST_EQ(visible.size(), 4u);
    BOOST_TEST_EQ(map[0], 0u);
    BOOST_TEST_EQ(map[1], 2u);
    std::cout << "  PASS: T6 名单含不存在的词 -> 只按命中项过滤" << std::endl;
  }

  // T7: IsPageLocalIndexValid —— 页内校验(count = 5)。
  //     缺陷 A 的根因就是翻页后拿第一页的候选数当页内上界。
  {
    BOOST_TEST(fluxing::IsPageLocalIndexValid(0, 5));
    BOOST_TEST(fluxing::IsPageLocalIndexValid(1, 5));
    BOOST_TEST(fluxing::IsPageLocalIndexValid(4, 5));
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(5, 5));  // 上界闭不了
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(9, 5));
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(-1, 5));  // 负数
    std::cout << "  PASS: T7 IsPageLocalIndexValid count=5 边界" << std::endl;
  }

  // T8: IsPageLocalIndexValid —— count = 0(当前页没有任何候选)一律 false。
  {
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(0, 0));
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(1, 0));
    BOOST_TEST(!fluxing::IsPageLocalIndexValid(-1, 0));
    std::cout << "  PASS: T8 IsPageLocalIndexValid count=0 全 false"
              << std::endl;
  }

  // T9: IgnoreListFilePath 正常拼接。
  {
    std::wstring path = fluxing::IgnoreListFilePath(
        L"C:\\Users\\me\\AppData\\Roaming\\Fluxing", L"luna_pinyin");
    BOOST_TEST(path ==
               L"C:\\Users\\me\\AppData\\Roaming\\Fluxing\\luna_pinyin"
               L".user_ignore.txt");
    // 与 IgnoreListFileName 自洽。
    BOOST_TEST(fluxing::IgnoreListFileName(L"luna_pinyin") ==
               L"luna_pinyin.user_ignore.txt");
    std::cout << "  PASS: T9 IgnoreListFilePath 正常拼接" << std::endl;
  }

  // T10: IgnoreListFilePath 任一入参为空 -> 返回空串(绝不拼出残缺路径)。
  //      调用方据此短路,不会去用户数据目录里乱写文件。
  {
    BOOST_TEST(fluxing::IgnoreListFilePath(L"", L"luna_pinyin").empty());
    BOOST_TEST(fluxing::IgnoreListFilePath(L"C:\\dir", L"").empty());
    BOOST_TEST(fluxing::IgnoreListFilePath(L"", L"").empty());
    BOOST_TEST(fluxing::IgnoreListFileName(L"").empty());
    std::cout << "  PASS: T10 IgnoreListFilePath 空入参 -> 空串" << std::endl;
  }

  // T11: ParseIgnoreListText —— CRLF / LF / CR 混排 + 空行 + 首尾空白 +
  // U+FEFF。
  {
    std::wstring text =
        L"\uFEFF"      // UTF-8 BOM / UTF-16LE BOM 解码后都表现为它
        L"甲\r\n"      // CRLF
        L"\r\n"        // 空行
        L"  乙 \r\n"   // 首尾空格
        L"丙\n"        // LF
        L"\t丁\t\n"    // 首尾 tab
        L"\uFEFF戊\r"  // 行内再出现一次 BOM + CR 结尾
        L"   \n"       // 只有空白的行
        L"己";         // 末尾无换行
    std::unordered_set<std::wstring> out;
    fluxing::ParseIgnoreListText(text, &out);
    BOOST_TEST_EQ(out.size(), 6u);
    BOOST_TEST(out.count(L"甲") == 1);
    BOOST_TEST(out.count(L"乙") == 1);
    BOOST_TEST(out.count(L"丙") == 1);
    BOOST_TEST(out.count(L"丁") == 1);
    BOOST_TEST(out.count(L"戊") == 1);
    BOOST_TEST(out.count(L"己") == 1);
    // 带空白的原样串不应进集合(否则永远匹配不上引擎候选)。
    BOOST_TEST(out.count(L"  乙 ") == 0);
    std::cout << "  PASS: T11 ParseIgnoreListText 混排换行/空白/BOM"
              << std::endl;
  }

  // T12: ParseIgnoreListText 空串 / 只有换行 -> 空集合。
  {
    std::unordered_set<std::wstring> out;
    fluxing::ParseIgnoreListText(L"", &out);
    BOOST_TEST(out.empty());
    out.insert(L"残留");
    fluxing::ParseIgnoreListText(L"\r\n\n  \r", &out);
    BOOST_TEST(out.empty());  // 解析前必须清空出参,不许叠加旧内容
    fluxing::ParseIgnoreListText(L"甲", nullptr);  // 空出参不许崩
    std::cout << "  PASS: T12 ParseIgnoreListText 空输入 -> 空集合"
              << std::endl;
  }

  // T13: 坏数据负对照(1) —— 名单非空时映射必须变短。
  //      若名单被误当空名单(例如读取静默失败没被发现),这条会失败。
  {
    std::unordered_set<std::wstring> ignore{L"乙", L"丁"};
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(EnginePage(), ignore, &visible, &map);
    BOOST_TEST(map.size() < EnginePage().size());
    BOOST_TEST_EQ(map.size(), 3u);
    BOOST_TEST_EQ(visible.size(), map.size());
    std::cout << "  PASS: T13 负对照: 名单非空 -> 映射变短" << std::endl;
  }

  // T14: 坏数据负对照(2) —— 映射后索引指向的原文必须 != 原索引指向的原文。
  //      这一条正是「直接用可见索引打引擎」会犯的错(spec 090 缺陷 A/B 同源)。
  {
    std::unordered_set<std::wstring> ignore{L"甲"};
    auto page = EnginePage();
    std::vector<std::wstring> visible;
    std::vector<size_t> map;
    fluxing::FilterCandidatesAndBuildMap(page, ignore, &visible, &map);
    size_t engine = 999;
    BOOST_TEST(fluxing::MapVisibleIndexToEngine(map, 0, &engine));
    // 用户看到的第 1 项是「乙」;引擎索引 0 是「甲」。两者必须不同,
    // 否则映射就没起作用(等价于把命令发给了被屏蔽的那个候选)。
    BOOST_TEST(page[engine] != page[0]);
    BOOST_TEST(page[engine] == L"乙");
    BOOST_TEST(visible[0] == page[engine]);
    std::cout << "  PASS: T14 负对照: 映射后原文 != 原索引原文" << std::endl;
  }

  std::cout << "14 / 14 assertions passed" << std::endl;
  return 0;  // 错误计数由唯一 main() 的 boost::report_errors() 统一汇总
}
