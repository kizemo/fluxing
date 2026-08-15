# 077 - Shift release-only candidate select via IPC state machine · Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过新增 3 条 IPC 命令 + Server-side 状态机实现 release-only Shift 候选选择,根治 librime `key_event.h:64` `operator==` 把 release 事件算成 modifier mismatch 的根本限制。

**Architecture:**

```
WeaselTSF (ShiftDown/Up detect + eat)        WeaselServer
   │ Shift_L/R raw Win32 key event            RimeWithWeaselHandler
   │                                          m_shiftState[sid]: ShiftState
   │ m_shiftDown (local)                      { downRecorded
   │       │                                  { interveningKey
   ▼ m_client.ShiftDown/ShiftUp IPC           { lastIsLeft }
   │                                          }
weasel::Client::ShiftDown/ShiftUp / ──────►  RimeWithWeaselHandler::ShiftDown/ShiftUp
   SelectCandidate via pipe                       │ ShiftUp && !interveningKey
                                                  ▼
                                                  rime_api->select_candidate(idx)
                                                  + _UpdateUI + reset state
```

状态机归属 = **Server**。TSF 端吃 Shift 事件,Server 端 per-session 状态。Wire 沿用 packed `wParam/lParam`,**不动** `WeaselIPCData.h`。

**Tech Stack:**

- C++17 · Win32 TSF (no PPL on caller side) · Boost.Asio `PipeChannel<...>` (template-instantiated for DWORD/PipeMessage) · librime 1.13 `select_candidate`
- gtest (existing test infrastructure; `test/TestPipeChannelRace` and `test/TestPipeProtocol` patterns)
- MSBuild `weasel.sln`, vcxproj submodule per test module
- NSIS 3.x via `_build_v02101.ps1` (handoff §6 脚本)

---

## Global Constraints

### C1 — IPC 4 边同步 (per `.claude/rules/ipc-boundary.md`)

`include/WeaselIPC.h` 的 enum + RequestHandler virtual + Client class 改动属 wire protocol 变化,触发 4 边同步。**虽然 `include/WeaselIPCData.h` 的 POD 不动**(packed `wParam/lParam` 复用,符合 L20 minimal surface),但**新增** enum 也归 wire 变化。

| 端 | 文件 | 改动 |
|---|---|---|
| Wire enum | `include/WeaselIPC.h` | ✅ +3 enum + 3 virtual + 3 Client 方法 |
| Server (writer) | `WeaselIPCServer/WeaselServerImpl.cpp` (dispatch + OnShiftDown/Up/SelectCandidate) + `RimeWithWeaselHandler` (virtual impl) | ✅ |
| TSF (reader) | `WeaselTSF/KeyEventSink.cpp` (吃 Shift) + `WeaselClientImpl.cpp` (Client methods) | ✅ |
| Deployer (reader) | `WeaselDeployer/` | ⚠️ **审计结论**:`WeaselDeployer.cpp` 走 `RegisterHotKey` (系统级 Alt+, 全局热键,见 `WeaselServerImpl.cpp:76`),**不**走 `ProcessKeyEvent` 路径,所以也不调 `Client::ShiftDown/ShiftUp/SelectCandidate`。Deployer 端**无**代码改动,但 deployer 仍是 Client 库使用者,需保证 `Client` 类 API 兼容性 (Client 添加新方法不影响 deployer)。 |
| ADR | `docs/adr/0007-shift-release-ipc.md` | ✅ Task T21 |
| Test | `test/TestShiftIPCWire/` (新) + `test/TestShiftIPCStateMachine/` (新) | ✅ Task T11 + T16 |

### C2 — TSF thread 不能阻塞 I/O (P2)

`WeaselTSF::_ProcessKeyEvent` 内调用 `m_client.ShiftDown/ShiftUp` 是**只发不收**:`_SendMessage` (sync) 写 pipe 但**不**等 Server 响应回执。这与现有 `m_client.ProcessKeyEvent(ke)` 同步 round-trip 不同——我们要**开新方法** `Client::ShiftDown/ShiftUp` 而不是改造 `ProcessKeyEvent`。

新增 `Client::ShiftDown/ShiftUp` 内部实现:`_SendMessage` 写完 pipe 即返回,**不调** `channel.Transact` 等待回执,确保 TSF thread 不阻塞在 pipe 读。

⚠️ `Client::SelectCandidate` **可以**走 `_SendMessage` 现有路径(同步)——它从 ShiftUp 直接位于 Server 状态机内,Server 同步调 `rime_api->select_candidate` 后返回;TSF thread 不直接调 SelectCandidate。

### C3 — Convectional Commits + module prefix (P4)

主实现 commit 必须用模块前缀。本期主 feature commit message 必须按 2 split:

1. `feat(WeaselIPC): v0.21.0.1 shift release-only candidate select via IPC state machine`
   - 内容:include/WeaselIPC.h · WeaselClientImpl.cpp · WeaselServerImpl.cpp · include/RimeWithWeasel.h · RimeWithWeasel/RimeWithWeasel.cpp · WeaselTSF/KeyEventSink.cpp · WeaselTSF/WeaselTSF.h · tests 完整
2. `chore(release): v0.21.0.1 ship — version bump env.bat/weasel.props + installer`
   - 内容:env.bat (FLUXING_VERSION 0.21.0.0 → 0.21.0.1) + weasel.props (FLUXING_VERSION 同上 + WEASEL_BUILD=0) + installer artifact

### C4 — 严禁误提交

`git status -sb` + `git diff --stat` 在每个 commit 前必做(commit-checklist.md §1)。**严禁**:
- `git add -A` / `git add .`(A10 — 会扫进 env.bat/weasel.props/*.log/*.obj)
- `weasel.props` / `env.bat` / `*.user` / `*.suo` / `*.log` / `Test*.obj` / `output/archives/*`
- 品牌 rebrand 与无关 bugfix 混 changeset(P8 锁定)
- `librime/` 子模块改动(无 ADR + 用户授权,P3/L10)
- `RimeLeversApi::{export,import}_user_dict` 不裹 `StartMaintenance`/`EndMaintenance`(本期不动 dict,但 maintenance guard 模板照搬)

### C5 — yaml 字节级约束 (L07/L09/L11)

`output/data/default.yaml:240-241` 删除:
- UTF-8 (no BOM) + CRLF
- Byte-level via `[IO.File]::ReadAllBytes` + concat + `WriteAllBytes`
- 删后验证 BOM absence + CR/LF parity
- 同行注释 (line 236-239 L21 注释块) **删 binding 行即可,注释行保留**(注释描述 spec 014 / L21 历史,留作审计 trail;但 write 时说明注释提到「Shift_L/R 选候选 2/3」已不再准确,需要 update 一句话避免误导)

### C6 — Forbidden runtime behaviors

- ❌ 改 `WeaselTSF/KeyEvent.cpp` 的 mask(L10 平台限制)
- ❌ 在 `WeaselTSF` 加 `shiftHeld` 全局状态(违反 hotkey-binding.md)
- ❌ 改 `librime/` 子模块(本期纯 Fluxing 端)
- ❌ 把过往 send_text binding 改动混到本次 commit
- ❌ TSF thread 阻塞 I/O(P2)
- ❌ 状态机漏 reset(永远在 ShiftUp 末尾无条件 `m_shiftState[sid] = ShiftState{}`)

---

## File Map (locked decomposition)

每行:Create / Modify,职责,被谁依赖。

### Create — new files

| 路径 | 职责 | 依赖 |
|---|---|---|
| `include/ShiftStateMachine.h` | 抽离的纯 ShiftStateMachine 类(无 rime_api / 无 GUI / 无 IPC);11 unit test 直接 include | 独立,仅用 `<unordered_map>` + `<cstddef>` |
| `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.cpp` | 11 unit test (per spec §2.7.1) | include/ShiftStateMachine.h |
| `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.vcxproj` | MSBuild 项目 | 同上 |
| `test/TestShiftIPCStateMachine/TestShiftIPCStateMachine.vcxproj.filters` | filter | 同上 |
| `test/TestShiftIPCWire/TestShiftIPCWire.cpp` | 5 integration test (per spec §2.7.2) | include/WeaselIPC.h + WeaselIPC/PipeChannel.h |
| `test/TestShiftIPCWire/TestShiftIPCWire.vcxproj` | MSBuild 项目 | 同上 |
| `test/TestShiftIPCWire/TestShiftIPCWire.vcxproj.filters` | filter | 同上 |
| `docs/adr/0007-shift-release-ipc.md` | MADR ADR(spec §2.8) | 独立 |
| `.specify/memory/.learnings/lessons-learned.md` (新增 L##-PhaseM-10 段) | 8 次失败 → IPC 重构的反思 | 独立 |

### Modify — existing files

| 路径 | 改动 | 责任范围 |
|---|---|---|
| `include/WeaselIPC.h` | +3 enum (`WEASEL_IPC_SHIFT_DOWN`/`SHIFT_UP`/`SELECT_CANDIDATE`,**插在 LAST_COMMAND 前**);+3 RequestHandler virtual;+3 Client method 声明 | Wire enum + 接口 |
| `include/RimeWithWeasel.h` | +`ShiftState` struct + `m_shiftState` member;+3 virtual override (ShiftDown/ShiftUp/SelectCandidate) | Server state machine |
| `RimeWithWeasel/RimeWithWeasel.cpp` | +3 virtual impl + AddSession/RemoveSession 维护 map + ProcessKeyEvent 加 `interveningKey = true` 标记 | Server state machine |
| `WeaselIPC/WeaselClientImpl.cpp` | +3 method (`ShiftDown/ShiftUp/SelectCandidate`);ShiftDown/Up 实现只发不收避免阻塞 | Client wire |
| `WeaselIPCServer/WeaselServerImpl.cpp` | +3 OnXxx 函数 + HandlePipeMessage switch +3 case 分支 | Server dispatch |
| `WeaselTSF/WeaselTSF.h` | +2 private 成员 (`m_shiftDown` + `m_shiftDownIsLeft`) | TSF local state |
| `WeaselTSF/KeyEventSink.cpp` | `_ProcessKeyEvent` 加 Shift_L/R 检测分支 | TSF Shift detection |
| `output/data/default.yaml` | 删 line 240-241 两行 + 更新 line 236-239 注释指向 IPC path | yaml cleanup |
| `test/TestShiftSelectBinding/TestShiftSelectBinding.cpp` | +2 yaml 负断言(`Shift+Shift_L send 2` 不存在;`Shift+Shift_R send 3` 不存在) | spec 014 regression |
| `test/TestDefaultHotkeys/TestDefaultHotkeys.cpp` | binding 总数 -2 断言(line 240-241 删除后) | spec 014 regression |
| `env.bat` | `FLUXING_VERSION 0.21.0.0 → 0.21.0.1` | version bump (chore commit) |
| `weasel.props` | `FLUXING_VERSION` 同上 + `WEASEL_BUILD=0` | version bump |
| `CHANGELOG.md` | `[0.21.0.1]` 段 | user-visible |
| `output/install.nsi` | **不修改**(本期 IPC 不动装机) | — |
| `librime/` | **不修改**(本期纯 Fluxing) | — |

### Out-of-scope (per spec §3,严格不碰)

- Shift+l / Shift+r 复合键
- Shift+Shift_L/R 触发 ascii_mode toggle(走既有 Shift+space binding)
- 用户 `*.custom.yaml` 覆盖检测(spec 075+ 跟进)
- installer NSIS 改动
- weasel.props / env.bat 跟踪(`weasel.props` 在 .gitignore 但版本必须 bump → commit 时 verify `.gitignore` 已屏蔽 `env.bat`)
- upstream PR(P8 waiver)
- 新 CI workflow

---

## Interface Contracts (locked signatures)

确保 T06(WeaselIPC.h)→T07(WeaselClientImpl)→T08(Server dispatch)→T09(T10 TSF) 之间的类型一致。

### `include/WeaselIPC.h` (T06)

```cpp
// Append BEFORE WEASEL_IPC_LAST_COMMAND (current line 36):
WEASEL_IPC_SHIFT_DOWN,
WEASEL_IPC_SHIFT_UP,
WEASEL_IPC_SELECT_CANDIDATE,

// Append to RequestHandler (after SelectCandidateOnCurrentPage virtual at line 69):
virtual void ShiftDown(bool is_left, DWORD session_id) {}
virtual void ShiftUp(bool is_left, DWORD session_id) {}
virtual void SelectCandidate(size_t index, DWORD session_id) {}

// Append to Client class (after SelectCandidateOnCurrentPage at line 132):
bool ShiftDown(bool is_left);
bool ShiftUp(bool is_left);
bool SelectCandidate(size_t index);
```

### `include/RimeWithWeasel.h` (T06, parallel)

```cpp
// New struct (in header, public or private):
struct ShiftState {
  bool downRecorded = false;
  bool interveningKey = false;
  bool lastIsLeft = false;
};

// Add to RimeWithWeaselHandler private members:
std::unordered_map<WeaselSessionId, ShiftState> m_shiftState;

// Public virtual overrides (matching WeaselIPC.h):
virtual void ShiftDown(bool is_left, WeaselSessionId ipc_id);
virtual void ShiftUp(bool is_left, WeaselSessionId ipc_id);
virtual void SelectCandidate(size_t index, WeaselSessionId ipc_id);
```

### `include/ShiftStateMachine.h` (T01, pure)

```cpp
#pragma once
#include <cstddef>
#include <unordered_map>
#include <cstdint>

namespace fluxing {

using SessionId = std::uint32_t;

class ShiftStateMachine {
 public:
  struct EventResult {
    bool fire_select;        // true => caller must call rime_api->select_candidate
    std::size_t index;       // valid only if fire_select == true (0-based)
  };

  void Reset(SessionId sid);
  void OnShiftDown(SessionId sid, bool is_left);
  EventResult OnShiftUp(SessionId sid, bool is_left);
  void OnInterveningKey(SessionId sid);   // called by ProcessKeyEvent handler
  void OnSessionDestroyed(SessionId sid); // called by RemoveSession

  std::size_t DebugSessionCount() const;

 private:
  struct State {
    bool downRecorded = false;
    bool interveningKey = false;
    bool lastIsLeft = false;
  };
  std::unordered_map<SessionId, State> states_;
};

}  // namespace fluxing
```

### Wire (C2 + spec §2.3)

| 命令 | `wParam` | `lParam` | 来源 |
|---|---|---|---|
| `WEASEL_IPC_SHIFT_DOWN` | bit0: 0=左 1=右 | `WeaselSessionId` | TSF `KeyEventSink.cpp` |
| `WEASEL_IPC_SHIFT_UP`   | bit0: 0=左 1=右 | `WeaselSessionId` | TSF `KeyEventSink.cpp` |
| `WEASEL_IPC_SELECT_CANDIDATE` | `size_t index` (0-based) | `WeaselSessionId` | (本期 Server 自己调,不从 TSF 来) |

注:`SelectCandidate` 命令存在是为了将来**其它 session** (e.g. 第三方工具 / scripted test)能 bypass 状态机直接选候选。**当前 spec** ShiftUp 是 Server 内同步调,不经 pipe。要保留命令以维持 surface 完整性(避免未来需添加)。

---

## Risk Register (per spec §2.9 + 实测新增)

| # | 风险 | Mitigation | 覆盖测试 |
|---|---|---|---|
| R1 | Server 状态机忘记 reset → 后续 Shift 误触发 | `ShiftUp` 末尾无条件 `m_shiftState[sid] = ShiftState{}`;OnInterveningKey 路径安全 | T04 (state machine test #8 Reset) |
| R2 | 跨 session stale state(alt-tab 后 Shift release 在新 session 误触发) | state per-`WeaselSessionId`;`OnSessionDestroyed` 即 `RemoveSession` | T11 Test #11 (multi-session) + US1-G |
| R3 | maintenance 期间 Shift 事件污染状态机 | 3 个 virtual impl 顶部 `if (m_disabled) return` | US1-H(本期 manual sandbox,无 unit) |
| R4 | `ProcessKeyEvent` handler 加 `interveningKey = true` 误判(letter 'a' 上屏前 release 算不算中间键) | 仅 **down** 事件算中间键(`mask & RELEASE_MASK == 0`),letter release 不触发 | Test #3 / #6 |
| R5 | TSF pipe 断开期间 Shift down 状态丢失 | TSF 端 `m_shiftDown` 本地保留;pipe 重连后 Server 收到 orphan ShiftUp → state 无 downRecorded → 忽略 | Test #5(模拟断 pipe) |
| R6 | yaml `bindings` 删除后用户 `user-custom/*.custom.yaml` 重新添加两条 binding 回到"按下就上屏"行为 | 本期不修(留 spec 075+ 跟进),在 `installation.yaml` 写警告注释说明 IPC path 已接管 | manual + 知识转移 |
| R7 | `librime/` 子模块改动? | **无**——本期纯 Fluxing 端改动,不改 librime | commit log verify |
| R8 | `weasel.props` / `env.bat` 误提交? | commit checklist §1 `git status -sb` + `git diff --stat` 强约束;`env.bat` 已在 .gitignore 但 .bat 工具需要在 chore commit 单独 bump | Task T22 |
| R9 | 删 yaml binding 时字节错位引入 BOM 或 LF-only | 字节级 delete + 验证 BOM absence + CR/LF parity | Task T18 |
| **R10 (NEW)** | `KeyEventSink::_ProcessKeyEvent` 中识别 Shift_L/R 时,若 letter 携带 Shift modifier `VK_SHIFT` flag 通过 `GetKeyboardState` (line 25) 注入 `ke.mask |= ibus::SHIFT_MASK` (existing behavior),我们吃了 Shift 后 letter 还会带 `SHIFT_MASK` → rime 会把它当 Shift+letter 大写处理 → 触发 `Shift_L: commit_text` 路径仍可能上屏 | T10 需要:**先**吃 Shift_L/R (return early),**不**走 `ConvertKeyEvent` 后的 letter 路径;或确保 Shift modifier 也被剥离。建议在 ConvertKeyEvent 前提前拦截 key,而不是依赖 `ke.mask`。**Review 时重点验证** US1-D (Shift+a -> 'A' 仍上屏但 select 不触发) | T11 Test #5(R5)+ US1-D |
| **R11 (NEW)** | `WeaselTSF::m_shiftDown` 是 TSF 单例全局状态,多 TSF instance 间共享 → 跨 session 切换 alt-tab 会导致 state race | T09 改用 per-TSF-instance local,或改为 per-session(sid 绑 ShiftState);**spec §2.1 已经把 state 放在 Server**,但 TSF 临时 hold 也需 per-instance | T11 Test #5 (mock pipe disconnect) |
| **R12 (NEW)** | `rime_ice.schema.yaml` vs `default.yaml` 文件名混淆 — spec.md / handoff 都写成前者,实测 binding 在后者 | plan.md (本文) §R12 + T18 第一行 grep 验证目标文件 | T18 grep step |
| **R13 (NEW)** | `WeaselServerImpl.cpp` 添加 `OnXxx` 函数需**同时** 增 `WEASEL_IPC_LAST_COMMAND` 之前的 enum(缺一即编译失败) | T06 → T08 严格按顺序,T06 完成后跑 `msbuild weasel.sln /t:Build` 验证 enum + Client 接口对齐 | T15 build verify |

---

## Source-of-Truth Citations

| 引用 | 来源 |
|---|---|
| librime `select_candidate` API | `librime/include/rime_api.h:436` (spec §2.7.2 验证) |
| librime `key_event.h:64` `operator==` 限制 | spec §0 Background 详述;l21 8 次失败路径总结 |
| WeaselTSF `KeyEvent.cpp:27-28` RELEASE_MASK 加 mask 行为 | spec §0;实测 `_lpbKeyState` 在 `KeyEventSink.cpp:25` 注入 |
| `RimeWithWeaselHandler::ProcessKeyEvent` 实现在 line 281-310 | 实测 `RimeWithWeasel/RimeWithWeasel.cpp:281-310` |
| Server dispatch `HandlePipeMessage` switch 表 | 实测 `WeaselIPCServer/WeaselServerImpl.cpp:411-439` (MAP_PIPE_MSG_HANDLE macro + PIPE_MSG_HANDLE cases) |
| yaml 实际位置 `default.yaml:240-241` | 实测 grep: **default.yaml NOT rime_ice.schema.yaml** (spec/handoff 行号错位) |
| 现有 test 模式参考 `test/TestPipeChannelRace/TestPipeChannelRace.cpp` | 实测存在文件 |
| `RequestHandler::EatLine` 模式 (`std::function<bool(std::wstring&)>`) | 实测 `include/WeaselIPC.h:54` |

---

## Verification Procedure

按 `commit-checklist.md` §1 + spec §2.10 + 实测新增:

1. **Pre-commit (每个 commit)**:
   ```
   # 1a. Stage explicitly (NO -A):
   git add <specific-files>
   # 1b. Verify:
   git status -sb
   git diff --cached --stat
   # 1c. Files MUST NOT include: env.bat, weasel.props, *.log, Test*.obj, output/archives/*
   ```

2. **Post-T15 (Build)**:
   ```
   msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
   ```
   Expected: 0 error;verify `output/Win32/WeaselServer.exe` + `WeaselTSF.dll` updated.

3. **Post-T17-19 (Tests)**:
   ```
   test\TestShiftIPCStateMachine\Release\TestShiftIPCStateMachine.exe
   test\TestShiftIPCWire\Release\TestShiftIPCWire.exe
   test\TestShiftSelectBinding\Release\TestShiftSelectBinding.exe
   test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
   ```
   Expected: 11 + 5 + (existing pass + 2 new neg assertions) + (existing pass + count -2) 全部 0 fail。

4. **Post-T20 (Build + installer)**:
   - `_build_v02101.ps1`(沿用 handoff §6)
   - Expected:`output\archives\fluxing-0.21.0.1-installer.exe`
   - md5 dual-verify:`_check_dll.ps1` 对比

5. **Post-T21 (Sandbox-verify)**:
   ```
   F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
       -InstallerPath release\fluxing-0.21.0.1-installer.exe `
       -Scenarios "1,2,2-prime,3,4,5,G,H"
   ```
   Expected: 8 场景全 pass。

6. **TSF DLL force-reload**: 用户必须 Win+L 锁屏 + 解锁(DLL 进程缓存)。

7. **Log evidence**:
   - `%TEMP%\rime.weasel.*.INFO.*.log` 无 `unrecognized modifier 'shift'`
   - 无 `invalid key binding` 警告
   - 含新 `select_candidate(idx=1/2)` 日志

---

## Done Criteria (ship-gate)

[from spec §4]:

- [ ] `spec.md` / `plan.md` / `tasks.md` 都在 `.specify/specs/077-shift-release-ipc/`,committed (spec.md ✅ commit `1506abc`)
- [ ] `docs/adr/0007-shift-release-ipc.md` committed
- [ ] `.specify/memory/lessons-learned.md` 新增 L##-PhaseM-10 段 committed
- [ ] 11 unit + 5 integration tests pass
- [ ] 3 现有测试(扩展后)仍 pass:`TestShiftSelectBinding`(加 2 负断言)/`TestDefaultHotkeys`(binding -2)/`TestResponseParser`(不变)
- [ ] `output\archives\fluxing-0.21.0.1-installer.exe` ship artifact 产出 + md5 dual-verify
- [ ] sandbox-verify 8 场景全 pass(US1-A..US1-H)
- [ ] `CHANGELOG.md` `[0.21.0.1]` 段 committed
- [ ] commit message 符合 P4(`feat(WeaselIPC)` + `chore(release)` 2 commits)
- [ ] 2 commits: `feat(WeaselIPC)` 主变更 + `chore(release)` 版本 bump + installer
