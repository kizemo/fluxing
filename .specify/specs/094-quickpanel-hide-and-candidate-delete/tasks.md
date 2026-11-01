# 094 · Tasks · 快捷设置栏切走不消失 + 候选词无法从词典删除

> 状态标记：`[x]` 已完成并有证据 · `[~]` 已完成待验证 · `[ ]` 未开始

## Phase 1 · 根因调查（已完成）

- [x] T001 [P1] 排除「装机载荷陈旧」假设：比对 `output/weasel.dll` 与
  `D:\Program Files\fluxing\weasel\weasel.dll` 的 SHA256 ⇒ 完全一致。
- [x] T002 [P1] 读 ADR 0024 / 0023 / 0008，梳理 QuickPanel 显隐链路。
- [x] T003 [P1] 坐实缺陷 A 三处根因（A1 `OnActivated` 只认自身 clsid、
  A2 `Deactivate` 先 `EndSession`、A3 `HasSession` 门禁）。
- [x] T004 [P1] 坐实缺陷 B 三处根因（B1 `m_deleteCallback` 值成员、
  B2 无快捷键拦截、B3 DELETE 不回 body）。
- [x] T005 [P1] 确认服务端 `FocusOut` 链路可承载修复（无 `_Active()` 门禁、
  不校验 session_id），无需改服务端协议。

## Phase 2 · 共享判定逻辑（宪法 R7）

- [x] T006 [P1] 新建 `include/FluxingCandidateDeleteShortcut.h`：header-only 纯函数
  （`IsKeyDownLParam` / `IsKeyStateDown` / `IsCandidateDeleteDigitVk` /
  `MatchCandidateDeleteShortcut` / `ShouldInterceptCandidateDelete`）。
- [x] T007 [P1] `include/WeaselIPC.h` 增加 `ProfileNotifyDecision` +
  `DecideProfileNotify`，与 `ShouldHideQuickPanelOnFocusOut` 并列。

## Phase 3 · 缺陷 A 修复

- [x] T008 [P1] `WeaselTSF.cpp::Deactivate`：`EndSession()` **之前**
  `_PostProfileNotify(kProfileNotifyDeactivated)`。
- [x] T009 [P1] `WeaselTSF.cpp::_ProfileNotifyLoop`：Deactivated 分支去掉
  `if (m_client.HasSession())` 门禁。
- [x] T010 [P1] `WeaselTSF.cpp::OnActivated`：改用 `DecideProfileNotify` 判定表，
  删除旧的 `_PostProfileNotify(...)` 调用（避免重复投递）。

## Phase 4 · 缺陷 B 修复

- [x] T011 [P1] `WeaselUI/WeaselPanel.h`：`m_deleteCallback` 改**引用**绑定，
  新增只读访问器 `deleteCandidateCallback()`。
- [x] T012 [P1] `WeaselUI/WeaselPanel.cpp`：构造初始化列表绑定
  `m_deleteCallback(ui.deleteCandidateCallback())`。
- [x] T013 [P1] `include/WeaselIPC.h` + `include/RimeWithWeasel.h`：
  `DeleteCandidateOnCurrentPage` 增加 `EatLine eat` 形参（IPC 铁律三端同步）。
- [x] T014 [P1] `WeaselIPCServer/WeaselServerImpl.cpp`：DELETE 建 `eat` lambda
  并传给 handler。
- [x] T015 [P1] `RimeWithWeasel/RimeWithWeasel.cpp`：DELETE 路径 `_Respond(ipc_id, eat)`，
  并把该会话 `interveningKey` 置 true（兜底 Shift 状态机）。
- [x] T016 [P1] `WeaselTSF/KeyEventSink.cpp`：`GetKeyboardState` 后算
  `ctrl_down` / `shift_down`；Ctrl 按住时吞掉 Shift down/up（`_shiftSuppressed`）；
  候选窗可见且序号在范围内时拦截数字键并调 `_DeleteCandidateOnCurrentPage`。
- [x] T017 [P1] `WeaselTSF/CandidateList.h`：新增 `GetVisibleCandidateCount()`。
- [x] T018 [P1] `WeaselTSF/CandidateList.cpp::_DeleteCandidateOnCurrentPage`：
  发命令后 `GetResponseData` + `_UpdateUI` 立即刷新；`StartUI` 缩进整理。

## Phase 5 · 判别性回归测试（宪法 II / R7）

- [x] T019 [P1] `test/TestCandidateRButtonDown`：T7 判定表 / T8 拦截边界 /
  T9 端到端「Ctrl+Shift+3 → 删可见第 3 项」。
- [x] T020 [P1] `test/TestWeaselIPC`：`/verify-delete-body` 模式，含
  `DecideProfileNotify` 四条判定表断言 + mock 的 DELETE override 回 body。
- [x] T021 [P1] `scripts/test-infra/run-test-suite.bat`：接线 `/verify-delete-body`。

## Phase 6 · 文档与交付

- [x] T022 [P1] `.specify/specs/094-*/{spec,plan,tasks}.md`。
- [x] T023 [P1] `docs/adr/0025-*.md`（MADR 四段式，显式声明 deployer 不受影响）。
- [x] T024 [P1] `docs/adr/README.md` 索引补 0025。
- [x] T025 [P1] `.specify/memory/lessons-learned.md` L161。
- [~] T026 [P1] 构建 `build.bat weasel` + 跑 `scripts/test-infra/run-test-suite.bat`。
- [~] T027 [P1] 按模块分小提交（Conventional Commits + 模块 scope）。
- [ ] T028 [P1] 人工验证：切走输入法看快捷设置栏是否消失；候选窗内右键删词；
  `Ctrl+Shift+1..N` 删词并立即刷新；候选窗不可见时 `Ctrl+Shift+3` 不被抢。
