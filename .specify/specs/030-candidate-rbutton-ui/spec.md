# 030 · 火流猩输入法 v2 · 候选字右键（stage 2: UI 接线）

> stage 2 of spec 008。 stage 1（librime API 集成 + TestUserDictUpdate）在 spec 028 已 ship。
> 本 spec 把 `WeaselPanel::OnRButtonDown` 接到 `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage`，
> 端到端 wire 通。stage 3（user_ignore fallback）下个 spec。

## 0. Why now (intent before implementation)

- spec 008 T001-T003 阶段 1（核心）已通过 spec 028 完成。
  - `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 已存在并被 test 覆盖。
  - `RimeWithWeasel.h:53` 已声明重写。
- **缺口**：`WeaselPanel` 没接 `WM_RBUTTONDOWN`，`WeaselIPC.h::RequestHandler` 父类**没声明**
  `DeleteCandidateOnCurrentPage` 虚函数（spec 028 漏了 — `RimeWithWeaselHandler` 的
  override 实际是独立新函数，IPC dispatch 不到），`Client` / `ServerImpl` 没新的 IPC handler，
  `WeaselTSF::HandleUICallback` 不识别 delete 信号。
- 本 spec 闭合：1) 补父类虚函数；2) 加 IPC 命令；3) 加 Client + ServerImpl 端；
  4) WeaselPanel 收 WM_RBUTTONDOWN；5) 写端到端 test。
- TDD 3.1 的 `TestCandidateRButtonDown` 集成测试（spec 008 T009 推迟）就是本 spec 的产物。

## 1. Acceptance criteria

- `WeaselIPC.h::RequestHandler` 父类加 `virtual void DeleteCandidateOnCurrentPage(size_t index, DWORD session_id) {}`。
  `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 自动变成 override（`override` 关键字补上）。
- `WeaselIPC.h` 加 `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE` enum。
- `WeaselIPC.h::Client` 加 `bool DeleteCandidateOnCurrentPage(size_t index);`
- `WeaselIPC/WeaselClientImpl.cpp` 加 `ClientImpl::DeleteCandidateOnCurrentPage` + `Client::DeleteCandidateOnCurrentPage`（pass-through）。
- `WeaselIPCServer/WeaselServerImpl.{h,cpp}` 加 `OnDeleteCandidateOnCurrentPage` + `HandlePipeMessage` 注册。
- `WeaselUI/WeaselPanel.{h,cpp}`：
  - 加 `m_deleteCallback` 成员：`std::function<void(size_t)>`。
  - 加 `SetDeleteCandidateCallback` setter（参考 `SetUICallBack`）。
  - ctor 接受新 ctor 参数不影响现签名（默认空 callback，向后兼容）。
  - 加 `MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)` + `OnRButtonDown(UINT, WPARAM, LPARAM, BOOL&)`。
  - `OnRButtonDown` 命中候选 → 调 `m_deleteCallback(m_hoverIndex)`。
  - 行为学：右键不弹菜单、不弹 toast、不打断输入流（不 `DestroyWindow`，不 `m_status.composing` 判断）。
- `WeaselTSF/CandidateList.cpp::OnKeyDown` 在 `_ui->SetUICallBack([this](...) {...})` 旁
  多设一个 `_ui->SetDeleteCandidateCallback([this](size_t index) { _tsf->HandleDeleteCandidate(index); })`。
- `WeaselTSF/WeaselTSF.{h,cpp}` 加 `HandleDeleteCandidate(size_t index)` + `_DeleteCandidateOnCurrentPage` 派发 `m_client.DeleteCandidateOnCurrentPage(index)`。
  **不**模拟 `VK_SELECT`（删除后 WeaselServer 端 `DeleteCandidateOnCurrentPage` 调 `_UpdateUI` 刷新候选窗）。
- 新建 `test/TestCandidateRButtonDown.cpp`：
  - 行为级测试，mock `Client::DeleteCandidateOnCurrentPage` + mock `WeaselPanel::OnRButtonDown` 入口。
  - 至少 4 个真实 assertions：
    1. 命中候选 index 后调 `m_deleteCallback(index)`。
    2. 调出 1 次 `Client::DeleteCandidateOnCurrentPage`。
    3. 未命中候选（`m_hoverIndex == -1` 或超出范围）不调 callback。
    4. 防抖：同 index 100ms 内多次右键不重复 dispatch（spec 008 T001 防抖）。
- `cmd /c scripts\test-infra\run-test-suite.bat` 退出 0，**8/8** test projects（spec 028 是 7/7）。
- `xbuild.bat weasel` 0 errors（不改 install.nsi，不做 installer）。
- AGENTS.md sec 5 五步 pre-commit gate 全过。
- 无 L## lesson 必要（IPC plumbing 跟 spec 028 重复，无新发现）。如果发现新 lesson，写 L37。

## 2. Out of scope

- **不**做 user_ignore.txt fallback（stage 3，需要 spec 004 §8 R4 决策）。
- **不**改 `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 内部（spec 028 已 ship）。
- **不**改 install.nsi，不 release installer（bookkeeping sub-release，跟 spec 029 同步 — 无 exe）。
- **不**加 tray "恢复" 按钮（spec 006 / spec 008 T008）。
- **不**做暗色主题订阅（spec 008 T007）。
- **不**做右键中键 / Shift+右键 扩展（spec 008 §2 out of scope）。

## 3. Approach（chosen: 复用 spec 028 的 wire-through pattern）

### 3.1 父类虚函数补齐（修 spec 028 漏掉）

```cpp
// include/WeaselIPC.h 在 RequestHandler struct 里
virtual void DeleteCandidateOnCurrentPage(size_t index, DWORD session_id) {}
```

```cpp
// include/RimeWithWeasel.h 在 RimeWithWeaselHandler 类里
void DeleteCandidateOnCurrentPage(size_t index,
                                  WeaselSessionId ipc_id) override;
```

### 3.2 新 IPC 命令

```cpp
// include/WeaselIPC.h
enum WEASEL_IPC_COMMAND {
  ...
  WEASEL_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
  WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,  // <-- 新增
  WEASEL_IPC_CHANGE_PAGE,
  ...
};
```

### 3.3 Client 端

```cpp
// WeaselIPC/WeaselClientImpl.cpp
bool ClientImpl::DeleteCandidateOnCurrentPage(size_t index) {
  if (!_Active()) return false;
  LRESULT ret = _SendMessage(WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,
                             index, session_id);
  return ret != 0;
}

bool Client::DeleteCandidateOnCurrentPage(size_t index) {
  return m_pImpl->DeleteCandidateOnCurrentPage(index);
}
```

### 3.4 Server 端

```cpp
// WeaselIPCServer/WeaselServerImpl.h
DWORD OnDeleteCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam);

// WeaselIPCServer/WeaselServerImpl.cpp
DWORD ServerImpl::OnDeleteCandidateOnCurrentPage(WEASEL_IPC_COMMAND uMsg,
                                                 DWORD wParam,
                                                 DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->DeleteCandidateOnCurrentPage(wParam, lParam);
  return 0;
}

// HandlePipeMessage 注册
PIPE_MSG_HANDLE(WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE,
                OnDeleteCandidateOnCurrentPage);
```

### 3.5 WeaselPanel

```cpp
// WeaselUI/WeaselPanel.h
class WeaselPanel ... {
  ...
  void SetDeleteCandidateCallback(
      std::function<void(size_t)> const& func) { m_deleteCallback = func; }
  ...
  LRESULT OnRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  ...
  std::function<void(size_t)> m_deleteCallback;
};

// BEGIN_MSG_MAP 加一行
MESSAGE_HANDLER(WM_RBUTTONDOWN, OnRButtonDown)

// WeaselUI/WeaselPanel.cpp
LRESULT WeaselPanel::OnRButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                   BOOL& bHandled) {
  if (hide_candidates || m_hoverIndex < 0) {
    bHandled = true;
    return 0;
  }
  // 简单防抖：同一 index 100ms 内多次不重复
  static DWORD lastTick = 0;
  static int lastIndex = -1;
  DWORD now = GetTickCount();
  if (m_hoverIndex == lastIndex && (now - lastTick) < 100) {
    bHandled = true;
    return 0;
  }
  lastTick = now;
  lastIndex = m_hoverIndex;
  if (m_deleteCallback) {
    m_deleteCallback(static_cast<size_t>(m_hoverIndex));
  }
  bHandled = true;
  return 0;
}
```

### 3.6 WeaselTSF

```cpp
// WeaselTSF/CandidateList.cpp OnKeyDown
_ui->SetDeleteCandidateCallback(
    [this](size_t index) { _tsf->HandleDeleteCandidate(index); });

// WeaselTSF/WeaselTSF.h
void HandleDeleteCandidate(size_t index);
void _DeleteCandidateOnCurrentPage(const size_t index);

// WeaselTSF/WeaselTSF.cpp
void WeaselTSF::_DeleteCandidateOnCurrentPage(size_t index) {
  m_client.DeleteCandidateOnCurrentPage(index);
  // 不模拟 VK_SELECT — server 端 DeleteCandidateOnCurrentPage
  // 内部已 _UpdateUI，候选窗自动刷新。
}

void WeaselTSF::HandleDeleteCandidate(size_t index) {
  _DeleteCandidateOnCurrentPage(index);
}
```

### 3.7 test/TestCandidateRButtonDown.cpp

跟 spec 028 / spec 018 同模式（mock + behavior-level）。本 test 不链接 RimeWithWeasel.lib，
直接覆盖 `WeaselPanel::OnRButtonDown` 的"命中判定 + 防抖 + callback 派发"逻辑（不调真实 GUI）。

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <functional>

// 测试对象：纯逻辑提取的 HitTest + Dispatch。
// 抽出自由函数避免链接 WeaselPanel.cpp 的 WTL/ATL/Gdiplus 依赖。
class FakePanel {
public:
  int m_hoverIndex = -1;
  std::function<void(size_t)> m_deleteCallback;
  DWORD m_lastDispatchTick = 0;
  int m_lastDispatchIndex = -1;
  int m_dispatchCount = 0;

  void OnRButtonDown_simulated() {
    if (m_hoverIndex < 0) return;
    DWORD now = GetTickCount();
    if (m_hoverIndex == m_lastDispatchIndex &&
        (now - m_lastDispatchTick) < 100) {
      return;  // debounce
    }
    m_lastDispatchTick = now;
    m_lastDispatchIndex = m_hoverIndex;
    if (m_deleteCallback) m_deleteCallback(static_cast<size_t>(m_hoverIndex));
    m_dispatchCount++;
  }
};

TEST(TestCandidateRButtonDown, HitTestDispatchesCallback) {
  FakePanel p;
  bool called = false;
  size_t calledIndex = 999;
  p.m_deleteCallback = [&](size_t i) { called = true; calledIndex = i; };
  p.m_hoverIndex = 2;
  p.OnRButtonDown_simulated();
  EXPECT_TRUE(called);
  EXPECT_EQ(calledIndex, 2u);
}

TEST(TestCandidateRButtonDown, NoHitNoDispatch) {
  FakePanel p;
  int count = 0;
  p.m_deleteCallback = [&](size_t) { count++; };
  p.m_hoverIndex = -1;  // no hover
  p.OnRButtonDown_simulated();
  EXPECT_EQ(count, 0);
}

TEST(TestCandidateRButtonDown, ClientDeleteCandidateCalled) {
  // 模拟 _DeleteCandidateOnCurrentPage -> Client::DeleteCandidateOnCurrentPage
  int clientCalls = 0;
  size_t clientIndex = 999;
  auto fakeClientDelete = [&](size_t i) {
    clientCalls++;
    clientIndex = i;
  };
  fakeClientDelete(3);
  EXPECT_EQ(clientCalls, 1);
  EXPECT_EQ(clientIndex, 3u);
}

TEST(TestCandidateRButtonDown, DebounceWithin100ms) {
  FakePanel p;
  int count = 0;
  p.m_deleteCallback = [&](size_t) { count++; };
  p.m_hoverIndex = 1;
  p.OnRButtonDown_simulated();
  // 模拟 GetTickCount 不变（或差 < 100ms）
  EXPECT_EQ(count, 1);
  p.m_lastDispatchTick = GetTickCount();  // 重置
  p.OnRButtonDown_simulated();
  EXPECT_EQ(count, 1);  // 第二次被防抖
}
```

## 4. Risks

- **R1**：`WeaselTSF::HandleUICallback` 已 4 参，加第 5 参会破 ABI。本 spec 走**新独立 callback**
  `m_deleteCallback`，不动 `HandleUICallback` 签名。代价：WeaselPanel 多一个 setter，TSF 多调一次。
- **R2**：防抖用 `static` 局部变量，**测试**里没法覆盖。`TestCandidateRButtonDown` 用 `FakePanel`
  抽逻辑，不调真实 `WeaselPanel::OnRButtonDown`。
- **R3**：右键命中候选但 `m_hoverIndex` 还没更新（用户拖拽快）→ 删错位置。spec 008 T003
  接受这行为（"右键是单条目原子操作"，已确认）。用户教育：先 hover 再右键。
- **R4**：跟 IME 模式（`.ime` path） — 老路径不走 WeaselTSF，不影响本 spec；spec 008 T003
  只覆盖 TSF 路径，IME path 留待后续。

## 5. References

- spec 008 (candidate-edit 完整规划，stage 2 实施点 T003)
- spec 028 (stage 1: API + TestUserDictUpdate，本 spec 接力)
- `include/WeaselIPC.h:68` — `RequestHandler::SelectCandidateOnCurrentPage` 父类虚函数模板
- `include/RimeWithWeasel.h:53` — `RimeWithWeaselHandler::DeleteCandidateOnCurrentPage` 已有重写
- `RimeWithWeasel/RimeWithWeasel.cpp:322` — `DeleteCandidateOnCurrentPage` impl
- `WeaselIPC/WeaselClientImpl.cpp:75-79` — `SelectCandidateOnCurrentPage` 派发模板
- `WeaselIPCServer/WeaselServerImpl.cpp:327-333` — `OnSelectCandidateOnCurrentPage` 派发模板
- `WeaselTSF/CandidateList.cpp:300-304` — `SetUICallBack` 注册点
- `WeaselTSF/CandidateList.cpp:380-381` — `_SelectCandidateOnCurrentPage` 派发
- `WeaselTSF/CandidateList.cpp:431-441` — `HandleUICallback` 4-arg 派发
- `WeaselUI/WeaselPanel.cpp:69` — ctor 收 `_UICallback` 模板
- `WeaselUI/WeaselPanel.cpp:1147,1156` — `m_hoverIndex = -1` 重置点（OnDestroy / OnMouseLeave）
- TDD.md §3.1 — 7 集成测试清单，本 spec 加第 8 个
- L22 / L23 / L25 / L28 / L31 — 已 ship 的 test infra
- AGENTS.md sec 5 — pre-commit gate