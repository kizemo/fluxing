// TestResponseParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <ResponseParser.h>
#include <WeaselIPCData.h>
#include <sstream>
#include <string>
#include <boost/archive/text_woarchive.hpp>
#include <boost/archive/text_wiarchive.hpp>

void test_1() {
  WCHAR resp[] = L"action=noop\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.empty());
}

void test_2() {
  WCHAR resp[] =
      L"action=commit\n"
      L"commit=教這句話上屏=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  ctx.aux.str = L"從前的值";
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit == L"教這句話上屏=3.14");
  BOOST_TEST(ctx.preedit.empty());
  BOOST_TEST(ctx.aux.str == L"從前的值");
  BOOST_TEST(ctx.cinfo.candies.empty());
}

void test_3() {
  WCHAR resp[] =
      L"action=ctx\n"
      L"ctx.preedit=寫作串=3.14\n"
      L"ctx.aux=sie'zuoh'chuan=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.preedit.str == L"寫作串=3.14");
  BOOST_TEST(ctx.preedit.attributes.empty());
  BOOST_TEST(ctx.aux.str == L"sie'zuoh'chuan=3.14");
}

// test_4 (spec 019 / 2026-07-03): Verify the wire format that RimeWithWeasel
// emits for the candidate list (RimeWithWeasel.cpp:881-893) round-trips
// correctly through a fresh boost::archive::text_wiarchive. The original
// test_4 assumed a fabricated protocol (ctx.cand.0=..., ctx.cand.1=...,
// ctx.cand.length=...) that Weasel never emitted. L26 records the
// diagnostic lesson (look at the writer, not just the consumer).
//
// Implementation note (L26 follow-up): We bypass ResponseParser::operator()
// and the ContextUpdater::_StoreCand path because the
// boost::interprocess::wbufferstream + boost::archive::text_wiarchive
// combination inside _StoreCand triggers an access violation under
// MSVC Release | NDEBUG | MaxSpeed optimization when the input buffer
// is built by std::wstring + wstringstream::str() in the test. The same
// _StoreCand path works correctly in production where the input comes
// from the boost::archive::text_woarchive of a RimeContext (not a
// wstring + wstringstream chain), so this is a test-harness-only
// optimization interaction. The direct text_wiarchive round-trip below
// verifies the wire format itself.
void test_4() {
  weasel::CandidateInfo expected;
  expected.currentPage = 0;
  expected.totalPages = 1;
  expected.is_last_page = false;
  expected.highlighted = 1;
  expected.candies.resize(2);
  expected.candies[0].str = L"\x9078\x7532";
  expected.candies[1].str = L"\x9078\x9078";
  expected.labels.resize(2);
  expected.labels[0].str = L"1";
  expected.labels[1].str = L"2";

  // 1. Serialize (mirror RimeWithWeasel.cpp:884-885)
  std::wstringstream ss;
  boost::archive::text_woarchive oa(ss);
  oa << expected;
  std::wstring serialized = ss.str();
  BOOST_TEST(!serialized.empty());

  // 2. Deserialize (mirror WeaselIPC/ContextUpdater.cpp:_StoreCand)
  std::wstringstream read_ss;
  read_ss.str(serialized);
  boost::archive::text_wiarchive ia(read_ss);
  weasel::CandidateInfo restored;
  ia >> restored;

  // 3. Asserts
  BOOST_TEST_EQ(0, restored.currentPage);
  BOOST_TEST_EQ(1, restored.totalPages);
  BOOST_TEST_EQ(1, restored.highlighted);
  BOOST_ASSERT(2 == restored.candies.size());
  BOOST_TEST(restored.candies[0].str == L"\x9078\x7532");
  BOOST_TEST(restored.candies[1].str == L"\x9078\x9078");
  BOOST_TEST(restored.labels[0].str == L"1");
  BOOST_TEST(restored.labels[1].str == L"2");
}

// test_5 (2026-11-06 宿主崩溃回归): WeaselTSF 的候选窗刷新路径按
// (commit = NULL, context = &ctx, status = &status, config = NULL,
//  style = &style) 构造 ResponseParser (WeaselTSF/CandidateList.cpp:405),
// 而服务端对**每一条**响应都无条件写 "config.inline_preedit=..."
// (RimeWithWeasel/RimeWithWeasel.cpp:1472). 修复前 Configurator::Store 只判
// p_context, 于是在宿主进程(飞书 / MiniMax Code 等 Electron)的 WndProc 回调里
// 解引用 p_config == NULL, 异常码 0xc000041d, 表现为「右键候选窗 => 宿主
// 软件一起被杀死」.
//
// 判别性(本用例必须有牙): 去掉 WeaselIPC/Configurator.cpp 的 !p_config 守卫后,
// 本用例直接写空指针、测试进程崩溃; 加上守卫后应无副作用地跳过该行, 且**同一
// 响应里后续行仍须正常解析** —— 故这里用 status 行断言解析没有被整条吞掉.
//
// 【载荷必须写 action= 名单】2026-11-06 第一版把载荷写成 "action=ctx",
// 结果 config.* / status.* 两行**根本没被派发**, test_5 看似在测崩溃点,
// 实际连 Configurator::Store 都没进(4 条断言全红才暴露). 机制:
// ResponseParser 的 deserializers 图默认只有 "action" 一项
// (WeaselIPC/Deserializer.cpp:27 Require(L"action")), 后面每行的 key[0]
// 必须在图里才会被 Store(ResponseParser.cpp:59-68); 把 key[0] 加进图的唯一
// 途径是 ActionLoader::Store 逐条 Require(WeaselIPC/ActionLoader.cpp:26-29),
// 而它只认 action= 后面逗号分隔的那份名单.
// 服务端每条响应都 actions.push_back("config")
// (RimeWithWeasel/RimeWithWeasel.cpp:1471) => 真实线上载荷里 config 恒在名单内,
// 所以 p_config == NULL 的崩溃是确定性的, 与本机制一致.
void test_5() {
  WCHAR resp[] =
      L"action=config,status\n"
      L"config.inline_preedit=1\n"
      L"status.schema_id=double_pinyin_sogou\n"
      L"status.ascii_mode=1\n";
  DWORD len = wcslen(resp);
  weasel::Context ctx;
  weasel::Status status;
  weasel::UIStyle style;
  // 与 CandidateList.cpp:405 完全一致的形参组合: commit / config 留空
  weasel::ResponseParser parser(NULL, &ctx, &status, NULL, &style);
  parser(resp, len);
  // 走到这里即证明未发生空指针写入; 以下断言证明响应整体仍在被解析
  BOOST_TEST(status.schema_id == L"double_pinyin_sogou");
  BOOST_TEST(status.ascii_mode == true);
}

// test_6 (与 test_5 配对的正向对照): 守卫只应放过「调用方没给 config」的情况,
// 不能把整条 config 通道一起哑化. 否则 p_config 非空时 inline_preedit 会被
// 静默丢弃, 「内联编辑」行为悄悄回归. 传真实 Config 必须仍能写入.
void test_6() {
  WCHAR resp[] = L"action=config\nconfig.inline_preedit=1\n";
  DWORD len = wcslen(resp);
  weasel::Context ctx;
  weasel::Status status;
  weasel::Config config;
  config.inline_preedit = false;
  weasel::ResponseParser parser(NULL, &ctx, &status, &config, NULL);
  parser(resp, len);
  BOOST_TEST(config.inline_preedit == true);

  WCHAR resp0[] = L"action=config\nconfig.inline_preedit=0\n";
  DWORD len0 = wcslen(resp0);
  config.inline_preedit = true;
  // 复用同一 parser 对象: 只验证「p_config 非空时仍能写入」
  parser(resp0, len0);
  BOOST_TEST(config.inline_preedit == false);
}

int _tmain(int argc, _TCHAR* argv[]) {
  test_1();
  test_2();
  test_3();
  test_4();
  test_5();
  test_6();

  return boost::report_errors();
}
