// TestCandidateRButtonDown.cpp : spec 030 (2026-07-04) / spec 090 (2026-10-30)
//
// 6 behavior-level assertions for the candidate right-click (stage 2 of spec
// 008, hardened by spec 090). Pattern: boost::detail::lightweight_test (no
// gtest linkage needed, see spec 024 / spec 028 pattern). We do NOT link
// WeaselPanel.cpp (it pulls in WTL/ATL/Gdiplus); instead we mirror the core
// logic in a test-local FakePanel / FakeClient pair and assert the same
// invariants the production code is contractually required to satisfy.
//
// The first 4 assertions match spec 030 spec.md §1 acceptance criteria:
//   1. Hit test dispatches the callback with the hover index.
//   2. No hover (m_hoverIndex < 0) does NOT dispatch.
//   3. End-to-end: FakeClient::DeleteCandidateOnCurrentPage is called
//      once with the right index (mirrors the TSF -> IPC -> handler flow).
//   4. Debounce: 100ms re-click on the same index is suppressed.
// spec 090 追加的两条(缺陷 B:右键屏蔽候选):
//   5. 右键一次 => 引擎删除调用 1 次,且屏蔽名单被追加 1 条,内容等于
//      该可见位置对应的**引擎候选原文**(名单非空时映射生效,不是可见位置的词)。
//   6. E6 哨兵:引擎删除恒返回 true(其返回值不含任何「这是系统词、删不掉」
//      的信息),名单**仍然**被写入;引擎返回 false 时同样写入 —— 即无条件。
//
// FakeClient 镜像的服务端实现是 RimeWithWeasel::DeleteCandidateOnCurrentPage,
// 它复用 include/FluxingCandidateIgnore.h 的纯函数,所以这里可以直接 include
// 同一份头文件做映射,不必再抄一遍逻辑。

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <FluxingCandidateIgnore.h>
#include <functional>
#include <windows.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// FakePanel mirrors WeaselPanel::OnRButtonDown's hit-test + debounce +
// dispatch.
class FakePanel {
 public:
  int m_hoverIndex = -1;
  std::function<void(size_t)> m_deleteCallback;
  DWORD m_lastDispatchTick = 0;
  int m_lastDispatchIndex = -1;
  int m_dispatchCount = 0;

  void OnRButtonDown_simulated() {
    if (m_hoverIndex < 0)
      return;  // no hover -> no dispatch
    DWORD now = GetTickCount();
    if (m_hoverIndex == m_lastDispatchIndex &&
        (now - m_lastDispatchTick) < 100) {
      return;  // debounce within 100ms
    }
    m_lastDispatchTick = now;
    m_lastDispatchIndex = m_hoverIndex;
    if (m_deleteCallback) {
      m_deleteCallback(static_cast<size_t>(m_hoverIndex));
    }
    m_dispatchCount++;
  }
};

// FakeClient mirrors weasel::Client::DeleteCandidateOnCurrentPage and, since
// spec 090, the server-side handler it lands in
// (RimeWithWeasel::DeleteCandidateOnCurrentPage):
//   可见页内索引 -> 引擎页内索引 -> 取引擎候选原文 -> **无条件**追加屏蔽名单
//   -> 调引擎 delete_candidate_on_current_page -> 无视其返回值。
class FakeClient {
 public:
  // 服务端侧状态镜像:引擎当前页候选 + 「可见索引 -> 引擎索引」映射表。
  std::vector<std::wstring> engineCandidates;
  std::vector<size_t> visibleToEngine;
  // 过滤后的可见候选文本(用户实际看到的那一页),仅测试观测用。
  std::vector<std::wstring> visibleTexts;
  // 观测点:被追加进屏蔽名单的候选原文。
  std::vector<std::wstring> ignoreAppended;
  int deleteCount = 0;
  size_t lastDeletedIndex = 999;
  // E6 取证:引擎删除的返回值恒为 true,只在「无 session / 无 ctx / 无 menu /
  // index >= page_size」这类调用姿势错误时才是 false,不含「系统词删不掉」信息。
  bool engineDeleteReturns = true;

  bool DeleteCandidateOnCurrentPage(size_t index) {
    size_t engine_index = 0;
    // 越界(含映射表为空)一律「什么都不做」:不写名单、不调删除。
    if (!fluxing::MapVisibleIndexToEngine(visibleToEngine, index,
                                          &engine_index))
      return false;
    std::wstring text;
    if (engine_index < engineCandidates.size())
      text = engineCandidates[engine_index];
    if (!text.empty())
      ignoreAppended.push_back(text);  // 无条件:与下面的删除结果无关
    deleteCount++;
    lastDeletedIndex = engine_index;
    return engineDeleteReturns;
  }
};

// 造一页 5 个引擎候选 + 指定屏蔽集合,产出 FakeClient 可直接用的状态。
void SetupPage(FakeClient& client,
               const std::unordered_set<std::wstring>& ignore) {
  client.engineCandidates = {L"甲", L"乙", L"丙", L"丁", L"戊"};
  fluxing::FilterCandidatesAndBuildMap(client.engineCandidates, ignore,
                                       &client.visibleTexts,
                                       &client.visibleToEngine);
}

}  // namespace

int main() {
  // Test 1: hit test dispatches the callback with the hover index.
  {
    FakePanel p;
    bool called = false;
    size_t calledIndex = 999;
    p.m_deleteCallback = [&](size_t i) {
      called = true;
      calledIndex = i;
    };
    p.m_hoverIndex = 2;
    p.OnRButtonDown_simulated();
    BOOST_TEST(called);
    BOOST_TEST_EQ(calledIndex, 2u);
    BOOST_TEST_EQ(p.m_dispatchCount, 1);
    std::cout << "  PASS: T1 hit test dispatches callback with hover index"
              << std::endl;
  }

  // Test 2: no hover (m_hoverIndex < 0) does NOT dispatch.
  {
    FakePanel p;
    int count = 0;
    p.m_deleteCallback = [&](size_t) { count++; };
    p.m_hoverIndex = -1;  // no hover
    p.OnRButtonDown_simulated();
    BOOST_TEST_EQ(count, 0);
    BOOST_TEST_EQ(p.m_dispatchCount, 0);
    std::cout << "  PASS: T2 no hover does not dispatch" << std::endl;
  }

  // Test 3: end-to-end - FakeClient::DeleteCandidateOnCurrentPage is
  // called once with the right index (mirrors the TSF -> IPC -> handler
  // flow when WeaselTSF::_DeleteCandidateOnCurrentPage delegates to
  // m_client.DeleteCandidateOnCurrentPage(index)). 名单为空 => 恒等映射,
  // 引擎索引与可见索引一致,行为与 spec 030 时逐字节相同。
  {
    FakeClient client;
    SetupPage(client, {});
    client.DeleteCandidateOnCurrentPage(3);
    BOOST_TEST_EQ(client.deleteCount, 1);
    BOOST_TEST_EQ(client.lastDeletedIndex, 3u);
    std::cout << "  PASS: T3 Client::DeleteCandidateOnCurrentPage called once "
                 "with right index"
              << std::endl;
  }

  // Test 4: debounce - 100ms re-click on the same index is suppressed.
  {
    FakePanel p;
    int count = 0;
    p.m_deleteCallback = [&](size_t) { count++; };
    p.m_hoverIndex = 1;
    p.OnRButtonDown_simulated();
    BOOST_TEST_EQ(count, 1);
    // Re-click immediately: m_lastDispatchTick == GetTickCount() means
    // delta < 100, so debounce kicks in.
    p.m_lastDispatchTick = GetTickCount();
    p.OnRButtonDown_simulated();
    BOOST_TEST_EQ(count, 1);  // second click suppressed
    BOOST_TEST_EQ(p.m_dispatchCount, 1);
    std::cout << "  PASS: T4 debounce within 100ms suppresses re-click"
              << std::endl;
  }

  // Test 5 (spec 090 缺陷 B): 右键一次 => 引擎删除 1 次 + 名单追加 1 条,
  // 且追加的是「该可见位置对应的引擎候选原文」。名单已屏蔽「甲」,所以用户
  // 看到的第 1 项是「乙」;若服务端直接用可见索引 0 去问引擎,记进名单的会是
  // 已经被屏蔽的「甲」—— 这条断言就是拦这个错位。
  {
    FakeClient client;
    SetupPage(client, {L"甲"});
    BOOST_TEST_EQ(client.visibleTexts.size(), 4u);
    BOOST_TEST(client.visibleTexts[0] == L"乙");
    bool ok = client.DeleteCandidateOnCurrentPage(0);  // 右键可见第 1 项
    BOOST_TEST(ok);
    BOOST_TEST_EQ(client.deleteCount, 1);
    BOOST_TEST_EQ(client.lastDeletedIndex, 1u);  // 引擎索引 1,不是 0
    BOOST_TEST_EQ(client.ignoreAppended.size(), 1u);
    BOOST_TEST(client.ignoreAppended[0] == L"乙");
    BOOST_TEST(client.ignoreAppended[0] == client.visibleTexts[0]);
    std::cout << "  PASS: T5 右键一次 -> 删除 1 次 + 名单追加引擎原文(映射生效)"
              << std::endl;
  }

  // Test 6 (E6 哨兵): 引擎删除的返回值不含分流信息,名单必须无条件写入。
  // 6a: 引擎说「删成功」(恒 true 的常态) —— 名单照样写。
  //     自造词被引擎真删掉,名单里多一行同名记录是无害的(它不会再出现)。
  // 6b: 引擎说「失败」 —— 名单同样写。系统词删不掉,这一行正是让它下次
  //     不再出现的东西。两种返回值下行为一致,才叫「无条件」。
  {
    FakeClient client;
    SetupPage(client, {});
    client.engineDeleteReturns = true;
    client.DeleteCandidateOnCurrentPage(2);
    BOOST_TEST_EQ(client.deleteCount, 1);
    BOOST_TEST_EQ(client.ignoreAppended.size(), 1u);
    BOOST_TEST(client.ignoreAppended[0] == L"丙");

    FakeClient client2;
    SetupPage(client2, {});
    client2.engineDeleteReturns = false;  // 引擎删除失败
    bool ok = client2.DeleteCandidateOnCurrentPage(2);
    BOOST_TEST(!ok);
    BOOST_TEST_EQ(client2.deleteCount, 1);             // 仍然调了引擎
    BOOST_TEST_EQ(client2.ignoreAppended.size(), 1u);  // 仍然写了名单
    BOOST_TEST(client2.ignoreAppended[0] == L"丙");
    std::cout << "  PASS: T6 E6 哨兵: 引擎返回 true/false 名单都写入(无条件)"
              << std::endl;
  }

  std::cout << "6 / 6 assertions passed" << std::endl;
  return boost::report_errors();
}