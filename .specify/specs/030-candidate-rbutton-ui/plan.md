# 030 · Plan · 候选字右键（stage 2: UI 接线）

## 1. 技术上下文

- **C++ 17 / WTL / ATL / Boost**（与 WeaselUI / WeaselTSF / WeaselIPCServer 同栈）。
- **修改文件**（7 个）：
  - `include/WeaselIPC.h` — 加 `RequestHandler::DeleteCandidateOnCurrentPage` 虚函数 + `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE` enum + `Client::DeleteCandidateOnCurrentPage`。
  - `include/RimeWithWeasel.h` — 加 `override` 关键字。
  - `WeaselIPC/WeaselClientImpl.cpp` — 加 `ClientImpl::DeleteCandidateOnCurrentPage` + `Client::DeleteCandidateOnCurrentPage`。
  - `WeaselIPCServer/WeaselServerImpl.h` — 加 `OnDeleteCandidateOnCurrentPage` 声明。
  - `WeaselIPCServer/WeaselServerImpl.cpp` — 加 impl + `HandlePipeMessage` 注册。
  - `WeaselUI/WeaselPanel.h` — 加 `m_deleteCallback` + setter + `OnRButtonDown` 声明。
  - `WeaselUI/WeaselPanel.cpp` — 加 `OnRButtonDown` 实现 + `BEGIN_MSG_MAP` 注册。
  - `WeaselTSF/CandidateList.cpp` — 在 `OnKeyDown` 设 callback 旁多设一个。
  - `WeaselTSF/WeaselTSF.h` — 加 `HandleDeleteCandidate` + `_DeleteCandidateOnCurrentPage` 声明。
  - `WeaselTSF/WeaselTSF.cpp` — 加 impl。
- **新建**（1 个）：
  - `test/TestCandidateRButtonDown/TestCandidateRButtonDown.cpp` — 行为级测试。
  - `test/TestCandidateRButtonDown/TestCandidateRButtonDown.vcxproj` — 8 号 test project。
- **不引入新依赖**；`weasel.sln` 加 `TestCandidateRButtonDown` 单元。
- **不修改** `output/install.nsi`（不做 installer release）。
- **不修改** `env.bat` / `weasel.props`（bookkeeping sub-release，跳过 version bump）。

## 2. Architecture

### 2.1 端到端数据流

```
User right-clicks candidate[index]
       |
       v
Windows OS -> WM_RBUTTONDOWN -> WeaselPanel::OnRButtonDown
       |
       v
WeaselPanel::m_deleteCallback(index)        [新 std::function]
       |
       v
WeaselTSF::HandleDeleteCandidate(index)     [TSF 端 lambda 包装]
       |
       v
WeaselTSF::_DeleteCandidateOnCurrentPage(index)
       |
       v
m_client.DeleteCandidateOnCurrentPage(index) [weasel::Client]
       |
       v
PipeChannel.Transact(WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE)
       |
       v
WeaselServer/WeaselServerImpl::OnDeleteCandidateOnCurrentPage
       |
       v
m_pRequestHandler->DeleteCandidateOnCurrentPage(wParam, lParam)
       |
       v
RimeWithWeaselHandler::DeleteCandidateOnCurrentPage(index, ipc_id)
       |
       v
rime_api->delete_candidate_on_current_page(session, index)
       |
       v
_UpdateUI(ipc_id)                           [候选窗自动刷新]
```

### 2.2 关键决策

- **新独立 callback** `m_deleteCallback`（不动 `_UICallback` 4-arg 签名）。
- **WeaselPanel ctor** 不改签名 — `m_deleteCallback` 默认为空 lambda，setter 调一次。
- **防抖**用 `static` 局部变量（仅单 panel 实例，无冲突）。
- **不**模拟 `VK_SELECT` — 删除后 server 端 `_UpdateUI` 自然刷新。

## 3. Constitution Check

| Rule | Status | Notes |
|---|---|---|
| I. Intent | OK | spec 030 §0 明确 wire-up intent |
| II. Test | OK | TestCandidateRButtonDown 4 真实 assertions |
| III. Spec-Artifact | OK | 3 件套齐全 |
| IV. Clarification | OK | spec 028 漏父类虚函数已识别 |
| V. Incremental | OK | spec 008 拆分 stage 2 独立可 ship |
| R1-R9 | OK | 引用 L22/L23/L25/L31 + spec 028 模板 |
| P1-P8 | OK | P8 brand-fork scope |
| L33/L34 | OK | here-string 走 %TEMP% + Copy-Item；不写 here-string 进项目文件 |

## 4. 风险

- **R1**（来自 spec 030 §4）：`WeaselTSF::HandleUICallback` 4-arg 不动，加新 callback 代价是多一个 setter + 多一次 TSF 内 dispatch。可接受。
- **R2**：`m_hoverIndex` 在快速右键时未更新（用户拖拽）→ 删错。spec 008 已接受这行为。
- **R3**：`FakePanel` 在 test 里复刻防抖逻辑，跟生产代码有 drift 风险。task T007 加 review note。
- **R4**：weasel.sln 加 test project 可能撞 spec 029 的 L31 修复 — task T008 跟 `scripts/test-infra/verify-test-binaries-fresh.bat` 同步验证 5/5 vcxproj 路径。

## 5. 验证步骤

1. `cmd /c xbuild.bat weasel` → 0 errors。
2. `cmd /c scripts\test-infra\run-test-suite.bat` → 8/8 PASS（spec 028 是 7/7）。
3. `cmd /c scripts\test-infra\verify-test-binaries-fresh.bat` → 5/5 binaries fresh in `test\Release\`。
4. AGENTS.md sec 5 五步 pre-commit gate（byte health, unit test, build hygiene, format, scope）。
5. 不做 NSIS smoke test（spec 不改 install.nsi）。

## 6. Release

- **version bump**：无（bookkeeping sub-release，spec 029 已 ship 0.18.18.0；本 spec 仍 0.18.18.0）。
  下次发 installer 时（spec 008 stage 3 完）跳到 v0.19.0.0。
- **tag**：无（无 release）。
- **commit**：`feat(fluxing): spec 030 - candidate right-click UI wiring (stage 2 of 008)`
- **push**：`git push kizemo Fluxing`（不 push `origin`）。

## 7. References

- spec 008 / spec 028（接力） / TDD §3.1
- `include/WeaselIPC.h:48-86`（RequestHandler）
- `include/WeaselIPC.h:121-150`（Client）
- `include/RimeWithWeasel.h:46-60`（RimeWithWeaselHandler）
- `WeaselIPC/WeaselClientImpl.cpp:75-89`（模板）
- `WeaselIPCServer/WeaselServerImpl.cpp:325-360`（模板）
- `WeaselTSF/CandidateList.cpp:295-310`（callback 注册点）
- `WeaselTSF/WeaselTSF.h:140-160`（声明段）
- `WeaselUI/WeaselPanel.h:23-100`（msg map + 成员段）
- `WeaselUI/WeaselPanel.cpp:60-75`（ctor `_UICallback` 初始化）
- `WeaselUI/WeaselPanel.cpp:285-345`（OnMouseWheel / OnLeftClickedUp 模板）
- `test/TestUserDictUpdate/TestUserDictUpdate.vcxproj`（8 号 test project 模板）
- `scripts/test-infra/verify-test-binaries-fresh.bat`（spec 029 L31 fix）