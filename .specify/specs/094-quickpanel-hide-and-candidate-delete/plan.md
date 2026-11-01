# plan 094 — 快捷设置栏切走不消失 + 候选词无法从词典删除

> 配套 `spec.md`（缺陷与验收标准）、`tasks.md`（任务分解与执行结果）。

## 0. 一句话

把「切走到别的输入法」的判定抽成**共享纯函数**（宪法 R7 生产/测试同源）并修正
三处时序门禁，让快捷设置栏切走后必然收起；同时修好**从未真正生效过**的右键删词
回调（值成员 → 引用绑定）、新增按物理键位判定的 `Ctrl+Shift+<1..9>` 删词快捷键，
并让删词命令**回响应体**以实现候选窗立即刷新。

---

## 1. Constitution Check

| 条款 | 适用性 | 本次如何满足 |
|---|---|---|
| **P1** Code Style | 适用 | 新增 `include/FluxingCandidateDeleteShortcut.h` 用 `#pragma once` 头卫；函数 `PascalCase`（`IsKeyDownLParam` / `IsKeyStateDown` / `IsCandidateDeleteDigitVk` / `MatchCandidateDeleteShortcut` / `ShouldInterceptCandidateDelete`）、常量 `kCandidateDeleteMaxNumber` 用 `kCamelCase`、局部变量 `camelBack`；缩进沿用 `.clang-format` 实际配置的 **Chromium 风格（2 空格）**（`SortIncludes: false`，故 include 顺序按逻辑分组而非字母序）。**未全仓跑** `clang-format.ps1 -n`（spec 092 已记录该脚本不可全仓跑）。 |
| **P2** Windows-Specific Constraints | 适用 | **无新增用户可见字符串**（本改动不产生任何面向用户的文案）；**无新 IPC 消息类型**（见 P8 三问第 2 条）；`OnActivated` / `Deactivate` 仍然**只投递意图**，管道 I/O 全部留在通知线程上，**不引入新的阻塞类别**；`Ctrl+Shift+数字` 的拦截点 `_ProcessKeyEvent` 内的 `m_client.DeleteCandidateOnCurrentPage()` 是同步管道往返，与该函数**既有**的 `m_client.ProcessKeyEvent(ke)` 同属一类，未新增阻塞形态。 |
| **P3** Build & Dependencies | 适用 | **不引入新第三方依赖**（新共享头只用 `windows.h` + `<cstddef>`）；**零 `.vcxproj` 改动** —— 判定逻辑是 header-only 纯函数，直接挂进**已存在**的 `test/TestCandidateRButtonDown`；`test/TestWeaselIPC` 增加 `/verify-delete-body` 模式，同样无需改工程文件。 |
| **P4** Commit & PR | 适用 | Conventional Commits，按模块分小提交（`fix(WeaselTSF)` / `fix(WeaselUI)` / `fix(WeaselIPC)` / `fix(RimeWithWeasel)` / `test(...)` / `docs(...)`），scope 取自白名单；提交前 `git status -sb` + `git diff --stat` 复核，挡住 `weasel.props` / `env.bat` / `*.obj` / `librime` 误提交。 |
| **P5** UX / 用户体验 | 适用 | 三项用户可感知的行为变化（快捷设置栏切走会消失 / 右键删词恢复 / 新增 `Ctrl+Shift+数字` 删词）⇒ 已在 `CHANGELOG.md` 的 `[Unreleased]` 段登记。**不改动 Rime 默认键位表**：新快捷键只在「候选窗可见且序号落在可见页范围内」时生效，其余一律放行给 Rime 引擎。 |
| **P6** Backwards Compatibility | 适用 | **不触碰**用户数据路径（`%AppData%\Rime` / `%AppData%\Fluxing`），不改 loader，不改注册表 ⇒ 不涉及 P6 waiver 的任何条款（见 P8 三问第 1 条）。IPC **线格式零变化**，新旧前端 / 新旧服务端仍可互通。 |
| **P7** Subprocess Lifecycle | 适用 | **不新增长驻进程**；`WeaselServer` 仍是唯一 supervisor；前端侧的所有新增 IPC 仍走既有的命名管道，删词复用既有的 `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE` 命令。 |
| **P8** Brand Fork: Fluxing | 适用 | 见下方「P8 三问」。 |

### P8 三问（Required plan.md disclosures）

1. **是否触及旧用户数据路径？是否援引 P6 waiver？**
   **否，且不援引任何 P6 waiver 条款。** 本次改动不读写 `%AppData%\Rime` 或
   `%AppData%\Fluxing` 下任何路径，不改注册表、不改互斥体名。
   删词动作写「候选屏蔽名单」的落点**沿用 ADR 0023 / spec 090 已有实现**，
   本次**未改其路径常量、未改其写入逻辑**（本次只在该处理路径上加了响应体回写与
   `interveningKey` 标记）。
   P6 waiver 的三条（`HKCU\Software\Rime\*` 归一、`WeaselUserDataPath()` 默认值与一次性迁移、
   单互斥体改名 + 旧互斥体过渡探测）**均未被触及**。

2. **是否引入新的 IPC 消息类型？**
   **否。** 删词复用**既有的** `WEASEL_IPC_DELETE_CANDIDATE_ON_CURRENT_PAGE`，
   「切走」复用**既有的** `FocusOut` 位标志 `WEASEL_IPC_FOCUS_OUT_FLAG_PROFILE_DEACTIVATED`。
   **命令号 / wParam / lParam 全部零变化** ⇒ 不触发 P2「双端更新」的消息语义要求。

   > **但仍按 CLAUDE.md §2 的 IPC 铁律走完整流程**：`RequestHandler` 的
   > `DeleteCandidateOnCurrentPage` **虚函数签名**变了（多一个 `EatLine eat = 0` 形参），
   > 这是 `include/WeaselIPC.h` 接口面的改动 ⇒ 已一次性提交
   > **server / TSF 两端 + ADR 0025 + `test/TestWeaselIPC/*` 测试**。
   > **deployer 端明确不受影响**：`WeaselDeployer` 没有 `RequestHandler` 实现
   > （`grep "RequestHandler\|DeleteCandidate" WeaselDeployer/` 无匹配），
   > 且线格式零变化 ⇒ 无需同步 deployer 改动。

3. **是否需要 CHANGELOG 条目？**
   **是。** 三项用户可感知的行为变化：
   (a) 切到别的输入法后快捷设置栏**不再残留**；
   (b) **右键删词恢复生效**（此前整条链路从未真正生效）；
   (c) **新增** `Ctrl+Shift+<1..9>` 删词快捷键（候选窗可见时）。
   已在 `CHANGELOG.md` 的 `[Unreleased]` 段登记条目。

### 决策分流（CLAUDE.md §3）

- **性质**：主体是 bugfix ⇒ 归档到 `.specify/memory/lessons-learned.md` 的 `L161` 节。
- **同时产出 ADR 0025**：因为本次**改了 `include/WeaselIPC.h` 的接口面**
  （`RequestHandler` 虚函数签名 + 新增共享判定函数），按 CLAUDE.md §2 的 IPC 铁律，
  必须有 ADR 记录「为什么加 `EatLine`、为什么抽判定表、为什么前端按物理键位拦截」。

---

## 2. 实施步骤

### 2.1 共享判定逻辑（生产 + 测试同源，R7）

| 文件 | 内容 | 为什么是共享头 |
|---|---|---|
| `include/WeaselIPC.h` | `enum class ProfileNotifyDecision` + `inline DecideProfileNotify(bool is_self_clsid, bool is_activated)` | 与既有 `ShouldHideQuickPanelOnFocusOut` 并列；生产（`WeaselTSF::OnActivated`）与测试（`TestWeaselIPC` 的 `/verify-delete-body`）引用**同一份实现** |
| `include/FluxingCandidateDeleteShortcut.h`（新建） | `IsKeyDownLParam` / `IsKeyStateDown` / `IsCandidateDeleteDigitVk` / `MatchCandidateDeleteShortcut` / `ShouldInterceptCandidateDelete` | header-only、只依赖 `windows.h` + `<cstddef>` ⇒ **单元测试工程零 `.vcxproj` 改动**即可编译它 |

### 2.2 缺陷 A 修复（三处叠加，缺一不可）

1. `WeaselTSF::Deactivate()`：`m_client.EndSession()` **之前**
   `_PostProfileNotify(kProfileNotifyDeactivated)` —— 事后补发必然带不上会话号。
2. `WeaselTSF::_ProfileNotifyLoop()`：Deactivated 分支**去掉**
   `if (m_client.HasSession())` 门禁（`ClientImpl::FocusOut` 本身没有 `_Active()` 门禁，
   服务端 `OnFocusOut` 也不校验 session_id）。
3. `WeaselTSF::OnActivated()`：改用 `DecideProfileNotify` 判定表，**删除**旧的
   `_PostProfileNotify(...)` 调用（否则会重复投递）。

### 2.3 缺陷 B 修复

- **右键**：`WeaselPanel::m_deleteCallback` 由**值**改**引用** `std::function<void(size_t)>&`，
  构造时绑 `ui.deleteCandidateCallback()`；新增只读访问器 `deleteCandidateCallback()`
  供测试断言「面板拿到的是 UI 上那一个 `std::function` 对象」。
- **快捷键**：`WeaselTSF/KeyEventSink.cpp` 在 `GetKeyboardState` 后取 `ctrl_down` /
  `shift_down`；命中且「候选窗可见 + 序号在可见页范围内」时调
  `_DeleteCandidateOnCurrentPage(number - 1)` 并吃掉该按键。
- **Shift 抑制**：Ctrl 按住时吞掉 Shift down（记 `_shiftSuppressed`），配对的 up 也吞；
  服务端 DELETE 处置另把该会话 `interveningKey` 置 true 作兜底。
- **立即刷新**：`RequestHandler::DeleteCandidateOnCurrentPage` 增加 `EatLine eat = 0`；
  `WeaselServerImpl` 建 `eat` lambda；`RimeWithWeaselHandler` 成功路径 `_Respond(ipc_id, eat)`；
  客户端 `WeaselTSF::_DeleteCandidateOnCurrentPage` 发完立刻
  `GetResponseData` + `_UpdateUI`。

### 2.4 判别性回归测试

| 工程 | 新增 | 作用 |
|---|---|---|
| `test/TestCandidateRButtonDown` | T7 判定表（VK + Ctrl/Shift + key-down）<br/>T8 拦截边界（可见页范围 / 零候选）<br/>T9 端到端（`Ctrl+Shift+3` → 删**可见**第 3 项 → 引擎索引 3） | 覆盖删词快捷键的**全部判定分支**与映射正确性 |
| `test/TestWeaselIPC` | `/verify-delete-body` 模式：<br/>① `DecideProfileNotify` 四条判定表断言<br/>② mock 的 `DeleteCandidateOnCurrentPage` override 回 body（判别标记 `status.composing=1`）<br/>③ mock 身份探针（`Greeting=`），防止测到真实 `WeaselServer.exe` | 证明 DELETE **真的回 body**，且判定表与生产同源 |
| `scripts/test-infra/run-test-suite.bat` | 接线 `/verify-delete-body`（放在 `/verify-focusout-flags` 之后、`/stop` 之前 —— 必须趁 mock server 还在时跑） | 进入准入门禁 |

---

## 3. Quality Gates（提交前）

1. ✅ `clang-format` 风格对齐 `.clang-format` 实际配置（Chromium 2 空格）；**未全仓**跑。
2. ⏳ `build.bat weasel` 构建成功（产品改动可编译）。
3. ⏳ `scripts/test-infra/run-test-suite.bat` 全量套件退出码 0（含 `TestCandidateRButtonDown`
   新增 T7/T8/T9 与 `TestWeaselIPC` 的 `/verify-delete-body`）。
4. ✅ `CHANGELOG.md` 已更新（`[Unreleased]` 段）。
5. ✅ ADR 0025 + `docs/adr/README.md` 索引 + lessons-learned L161 + spec 094 三件套。
6. ⏳ Conventional Commits + 白名单 scope + 提交前 `git status -sb` / `git diff --stat` 复核。
7. ⏳ **人工验证**（TSF 层真机行为无自动化证据，见 spec 094 §7）：
   切走输入法看快捷设置栏是否消失；候选窗内右键删词；`Ctrl+Shift+1..N` 删词并立即刷新；
   候选窗不可见时 `Ctrl+Shift+3` 仍触发 Rime 的 `ascii_punct`。

## 4. 回滚

本项改动自包含：共享头 1 个（新增）+ 判定函数 1 处（新增）+ 前端 5 处 + 服务端 3 处
+ 测试 2 个工程 + 文档 4 份。**IPC 线格式零变化** ⇒ 回滚不涉及任何协议/数据迁移，
`git revert` 对应提交即可，用户侧无残留状态（屏蔽名单沿用既有实现，本次未改其落点）。
