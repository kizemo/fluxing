# 077 - Shift release-only candidate select via IPC state machine (v0.21.0.1)

> Scope: 实现「按 Shift 不松不上屏,松 Shift 上屏第 2/3 候选,Shift+字母 出大写字母不上屏」,通过新增 3 条 IPC 命令 + Server-side 状态机实现 release-only 触发,绕开 librime `key_event.h:43` `operator==` 把 release 事件算成 modifier mismatch 的根本限制。
>
> 这是 spec 014 / 012 / L18 / L19 / L21 反复 8 次失败的根治路径——把 Shift 状态机从 librime yaml 层搬到 WeaselServer C++ 层,通过 IPC 上报。

## 0. Background

- spec 005 v1.1 US1-B 承诺:Shift_L 选第 2 候选,Shift_R 选第 3 候选(菜单可见时)。
- spec 012 / 014 多次尝试 yaml-only 修复,失败模式:
  - `key_binder/bindings` `accept: Shift+Shift_L send 2` 匹配 **Shift_L down**(modifier=Shift),按下就触发 → 违反场景 ①。
  - librime `key_event.h:43` `operator==` 严格比较 keycode+modifier,**包含** `RELEASE_MASK`。
  - WeaselTSF `KeyEvent.cpp:27-28` 在 key-up 强制加 `RELEASE_MASK`(bit14),Shift_L up = `{Shift_L, RELEASE_MASK|Shift}`。
  - 因此 `accept: Shift+Shift_L` (modifier=Shift) **永不匹配** release 事件(RELEASE_MASK 占 bit14,与 bit0 Shift 不冲突但 modifier 字段值不同)——yaml 层无法表达「release-only」。
  - `ascii_composer/switch_key: Shift_L: commit_text` 走另一条路径(不参与 release 比较),单独 Shift 可 toggle ascii_mode——但这违反场景 ①(有候选时不该 commit)。
- L21 lessons learned: 「YAML 层 binding 改动与 C++ hotkey 代码不同 commit,不同 reviewer」。本次彻底放弃 yaml 层修复,改走 IPC 重构。
- handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md §2 详列 8 次失败路径。
- handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md §3.1 详列 TSF 平台限制。

## 1. Product Angle (PRD section)

### 1.1 Goal

实现 release-only Shift hotkey 候选选择:有候选时,按 Shift 不松不上屏;松 Shift(中间无其他键)上屏第 2(左)/ 第 3(右)候选。Shift+字母照常,Shift+空格/标点 等既有行为不变。

### 1.2 User Stories

- **US1-A** [P1]: 有候选(≥3),按 Shift_L 不松。**期望**:什么都不发生(候选菜单不变,不 commit)。
- **US1-B** [P1]: US1-A 状态松 Shift_L。**期望**:上屏第 2 候选(0-based idx=1),菜单关闭。
- **US1-C** [P1]: 有候选(≥3),按 Shift_R 松开(无中间键)。**期望**:上屏第 3 候选(0-based idx=2)。
- **US1-D** [P1]: 有候选,按 Shift 不松 + 按字母 'a' + 松 Shift。**期望**:
  - 'A' 显示在 preedit(或直接 commit 到 app,沿用 rime Chinese-mode 处理)
  - 松 Shift 时**不**触发 select_candidate(因有 intervening key)
- **US1-E** [P1]: 无候选,Shift+空格。**期望**:ascii_mode toggle(走 `Shift+space toggle ascii_mode` 既有 key_binder binding),不触发 select_candidate。
- **US1-F** [P1]: Shift+, 出《。**期望**:《 上屏,松 Shift 不触发 select_candidate(有 intervening comma)。
- **US1-G** [P2]: 跨 session 边界——Shift 在 app A 按下,在 app B 松开。**期望**:不触发 select_candidate(防止 stale 状态污染)。
- **US1-H** [P2]: 用户在 IME maintenance 期间(deployer 跑中)按 Shift。**期望**:Shift 事件被吞,状态机不被污染,deployer 结束后恢复正常。

### 1.3 Acceptance (GWT)

- Given 候选菜单存在,有 ≥ 3 个候选
- When 用户按 Shift_L 不松
- Then rime.weasel.*.INFO log 无 `select_candidate` 行;UI 候选不变

- Same context, 用户松 Shift_L
- Then 日志有 `select_candidate(idx=1)` 行;UI 第 2 候选被选并 commit

- Same context, 用户按 Shift_R 后松
- Then 日志有 `select_candidate(idx=2)` 行;UI 第 3 候选被选并 commit

- Same context, 用户 Shift+a(有候选)
- Then preedit(或 app)收到 'A';松 Shift 时日志**无** `select_candidate` 行(intervening)

- 无候选,Shift+空格
- Then ascii_mode toggle;日志**无** `select_candidate` 行

- 输出目录 `output/data/rime_ice.schema.yaml` 的 `key_binder/bindings` 中
- 不存在 `{ when: has_menu, accept: Shift+Shift_L, send: 2 }`
- 不存在 `{ when: has_menu, accept: Shift+Shift_R, send: 3 }`

- 服务端 state machine 的 unit test(`test/TestShiftIPCStateMachine/`)11 个用例全 pass

## 2. Technical Angle (TDD section)

### 2.1 Architecture overview

```
┌──────────┐  ShiftDown/ShiftUp IPC   ┌─────────────────────────┐
│ WeaselTSF│ ────────────────────────→│ WeaselServer            │
│ (TSF)    │                          │ RimeWithWeaselHandler   │
│          │ ←─────────────────────── │  m_shiftState[session]: │
│  TSF sees│   ProcessKeyEvent IPC     │    downRecorded         │
│  all keys│   (other keys normal)    │    interveningKey       │
│          │                          │    lastIsLeft           │
└──────────┘                          └─────────────────────────┘
                                            │
                                            ▼ ShiftUp && !interveningKey
                                       rime_api->select_candidate(
                                          session, lastIsLeft?1:2)
```

**关键决定**(从 brainstorming 5 个澄清问题确认):

1. **状态机归属 = Server**(`hotkey-binding.md`「单一 source of truth 在 Server」原则)
2. **IPC 粒度 = 2 条新命令**(`WEASEL_IPC_SHIFT_DOWN` + `WEASEL_IPC_SHIFT_UP`,与现有 granular 风格一致)
3. **TSF 行为 = 吃掉 Shift down/up**(不转发给 rime 的 `ProcessKeyEvent`,letter/space/标点等键照常转发,SHIFT_MASK 来自 Windows `keyState` 仍在)
4. **候选下标 = 绝对下标**(新增 `WEASEL_IPC_SELECT_CANDIDATE` + `Client::SelectCandidate`,不复用 `SelectCandidateOnCurrentPage`,不受 `default.yaml:page_size` 影响)
5. **菜单探测 = 不探测**(直接调 `select_candidate`,rime 无菜单时 no-op 返回 false,Server 不做 guard)

### 2.2 Files changed

| 文件 | 改动 |
|---|---|
| `include/WeaselIPC.h` | +3 enum `WEASEL_IPC_SHIFT_DOWN`/`SHIFT_UP`/`SELECT_CANDIDATE`;+3 Client 方法 `ShiftDown/ShiftUp/SelectCandidate`;+3 RequestHandler virtual `ShiftDown/ShiftUp/SelectCandidate` |
| `WeaselIPC/WeaselClientImpl.cpp` | +3 方法实现(`_SendMessage` 复用现有 packed `wParam/lParam` 风格) |
| `WeaselTSF/KeyEventSink.cpp` | `_ProcessKeyEvent` 内增加 Shift_L/R 分支:发 `ShiftDown/Up` IPC + 吃事件(`*pfEaten = TRUE` + early return) |
| `WeaselTSF/WeaselTSF.h` | +2 成员:`m_shiftDown`(bool),`m_shiftDownIsLeft`(bool),per-session reset |
| `WeaselIPCServer/ServerImpl.cpp` | +3 case 分支 dispatch 到 `m_handler->ShiftDown/ShiftUp/SelectCandidate` |
| `RimeWithWeasel/RimeWithWeasel.h` | +3 virtual override 声明 |
| `RimeWithWeasel/RimeWithWeasel.cpp` | +`m_shiftState`(`std::unordered_map<WeaselSessionId, ShiftState>`);+3 virtual impl;`AddSession`/`RemoveSession` 维护 map;`ProcessKeyEvent` 现有 handler 加 `interveningKey = true` 标记逻辑(当 keycode ∉ Shift_L/R && !RELEASE) |
| `output/data/rime_ice.schema.yaml` | **删** `key_binder/bindings:240-241` 的 2 条 `Shift+Shift_L/R send 2/3`(因为 IPC 实现接管了) |
| `test/TestShiftIPCStateMachine/`(新) | unit test for `ShiftStateMachine`(抽离的纯状态机,无 rime_api 依赖),11 用例 |
| `test/TestShiftIPCWire/`(新) | integration test for IPC 编码 roundtrip(对照 `TestPipeChannelRace` 模式) |
| `test/TestShiftSelectBinding/`(扩展) | 加 2 条 yaml 负断言:`Shift+Shift_L send 2` 不存在;`Shift+Shift_R send 3` 不存在 |
| `test/TestDefaultHotkeys/`(扩展) | binding 总数 -2 断言 |
| `docs/adr/0007-shift-release-ipc.md` | 新 MADR ADR,记录 8 次失败 → IPC 重构路径 |
| `.specify/memory/lessons-learned.md` | L##-PhaseM-10(spec 077 落地): yaml-only 修复在 release-only 场景下根本不可行;根治必须 IPC 状态机 |
| `env.bat` + `weasel.props` | bump `FLUXING_VERSION 0.21.0.0 → 0.21.0.1`,`WEASEL_BUILD=0` 保持 |
| `CHANGELOG.md` | `[0.21.0.1]` 段:`feat(WeaselIPC): Shift release-only candidate select via IPC state machine` |
| `release/fluxing-0.21.0.1-installer.exe` | installer ship artifact |

### 2.3 Wire encoding(沿用 packed `wParam/lParam`)

| 新命令 | `wParam` | `lParam` |
|---|---|---|
| `WEASEL_IPC_SHIFT_DOWN` | bit0: 0=左, 1=右 | `WeaselSessionId`(DWORD) |
| `WEASEL_IPC_SHIFT_UP` | bit0: 0=左, 1=右 | `WeaselSessionId` |
| `WEASEL_IPC_SELECT_CANDIDATE` | `size_t index`(0-based) | `WeaselSessionId` |

`include/WeaselIPCData.h` **不需要改 wire struct**(新命令用 packed 风格,沿用 `KeyEvent` 不引入新类型,符合 L20 minimal surface 原则)。

### 2.4 Server 端状态机(`RimeWithWeaselHandler` 内)

```cpp
struct ShiftState {
  bool downRecorded = false;
  bool interveningKey = false;
  bool lastIsLeft = false;
};
std::unordered_map<WeaselSessionId, ShiftState> m_shiftState;
```

**ShiftDown 流程**:
```cpp
void RimeWithWeaselHandler::ShiftDown(WeaselSessionId sid, bool isLeft) {
  if (m_disabled) return;  // maintenance guard,与 ProcessKeyEvent 一致
  auto& s = m_shiftState[sid];
  s.downRecorded = true;
  s.interveningKey = false;
  s.lastIsLeft = isLeft;
}
```

**ShiftUp 流程**(核心决策点):
```cpp
void RimeWithWeaselHandler::ShiftUp(WeaselSessionId sid, bool isLeft) {
  if (m_disabled) return;
  auto it = m_shiftState.find(sid);
  if (it == m_shiftState.end()) return;        // 无 Down → 忽略
  auto& s = it->second;
  if (s.downRecorded && !s.interveningKey) {
    size_t idx = s.lastIsLeft ? 1 : 2;         // 2nd / 3rd 候选
    // menu 检查 = 不检查(让 rime no-op)
    rime_api->select_candidate(to_session_id(sid), idx);
    _UpdateUI(sid);
  }
  s = ShiftState{};                            // reset
}
```

**interveningKey 触发点**(`ProcessKeyEvent` handler 末尾追加):
```cpp
BOOL RimeWithWeaselHandler::ProcessKeyEvent(KeyEvent keyEvent,
                                            WeaselSessionId sid, EatLine eat) {
  // ... 现有 rime_api->process_key 调用 + TryLazyRecovery + _Respond/_UpdateUI 不变 ...
  // 新增:仅非 Shift 键 + 非 release 算 intermediate
  if (!(keyEvent.mask & ibus::Modifier::RELEASE_MASK) &&
      keyEvent.keycode != ibus::Shift_L &&
      keyEvent.keycode != ibus::Shift_R) {
    auto it = m_shiftState.find(sid);
    if (it != m_shiftState.end() && it->second.downRecorded) {
      it->second.interveningKey = true;
    }
  }
  // ... 现有返回逻辑不变 ...
}
```

**生命周期**:
- `AddSession`:`m_shiftState[sid]` 默认构造(空)
- `RemoveSession`:`m_shiftState.erase(sid)`(防止 stale state)
- `FocusOut`:不清(等 session 销毁或下一次 ShiftDown 覆盖)

### 2.5 yaml 改动(`output/data/rime_ice.schema.yaml`)

**删** line 240-241:
```yaml
- { when: has_menu, accept: Shift+Shift_L, send: 2 }   # ← DELETE
- { when: has_menu, accept: Shift+Shift_R, send: 3 }   # ← DELETE
```

**保留** 其他 binding(line 212-263 的 Shift+comma/period/slash/semicolon/apostrophe/backslash/bracketleft/bracketright 8 条中文标点,以及 line 262 `Shift+space toggle ascii_mode`)。这些 binding 匹配 **down** 事件,与本次 release-only 路径无冲突。

**字节级约束**(per L07/L09/L11):
- UTF-8 (no BOM) + CRLF
- Byte-level delete via `[IO.File]::ReadAllBytes` + concat + `WriteAllBytes`
- 删后验证 BOM absence + CR/LF parity

### 2.6 4 边同步规则遵守(per `.claude/rules/ipc-boundary.md`)

虽然 `WeaselIPCData.h` 没改,但 `WeaselIPC.h` 的 enum 改动也是 wire protocol 变化,触发 4 边同步:

| 端 | 文件 | 改动 |
|---|---|---|
| Wire enum | `include/WeaselIPC.h` | ✅ +3 enum + 3 virtual + 3 Client 方法 |
| Server (writer) | `WeaselServer/` + `WeaselIPCServer/` | ✅ `ServerImpl` dispatch + `RimeWithWeaselHandler` 3 virtual impl |
| TSF (reader) | `WeaselTSF/` + `WeaselIPC/WeaselClientImpl` | ✅ `KeyEventSink` 检测 + `Client::ShiftDown/Up/SelectCandidate` |
| Deployer (reader) | `WeaselDeployer/` | ⚠️ 无改动需要——Deployer 是 client of pipe,但不发 Shift 事件(`WeaselDeployer.cpp` 走 `RegisterHotKey` 系统级热键,不走 `ProcessKeyEvent` 路径);`Client::ShiftDown/Up/SelectCandidate` 是新增方法,Deployer 不调用即可 |
| ADR | `docs/adr/0007-shift-release-ipc.md` | ✅ 见 §2.8 |
| Test | `test/TestShiftIPCWire/` + `test/TestShiftIPCStateMachine/` | ✅ 见 §2.7 |

### 2.7 Tests

#### 2.7.1 Unit: `test/TestShiftIPCStateMachine/`(新)

**被测对象**:抽离的纯状态机 `ShiftStateMachine`,无 rime_api 依赖。

11 用例:

| # | 场景 | 期望 |
|---|---|---|
| 1 | Down→Up(L),无中间键 | outIdx=1, fire=true |
| 2 | Down→Up(R),无中间键 | outIdx=2, fire=true |
| 3 | Down→InterveningKey→Up | fire=false(intervening) |
| 4 | Up(无 Down) | fire=false(ignored) |
| 5 | Down→Down(连续 2 次,左后右) | 第 2 次覆盖 lastIsLeft=false |
| 6 | Down→InterveningKey×2→Up | fire=false |
| 7 | Down→Up(L)→立即 Down(R)→Up(R) | 第 2 次 fire=true, outIdx=2 |
| 8 | Reset 后 Up | fire=false |
| 9 | OnInterveningKey 在 Down 前调用 | 不污染(等下次 Down 重置) |
| 10 | 快速交替 fuzz(连续 100 次 Down/Up) | 无 crash / 越界 |
| 11 | 并发 2 session(sid=A, sid=B) | 状态独立,A 触发不影响 B |

#### 2.7.2 Integration: `test/TestShiftIPCWire/`(新)

**被测对象**:`Client::ShiftDown` → pipe → `ServerImpl` → `RimeWithWeaselHandler::ShiftDown`。

5 用例(沿用 `TestPipeChannelRace` 的 mock pipe 模式):

| # | 测试 |
|---|---|
| 1 | `ShiftDown`: wParam=0, lParam=0x42 → Server 收到 isLeft=true, sid=0x42 |
| 2 | `ShiftDown`: wParam=1 → isLeft=false |
| 3 | `ShiftUp`: 同 1+2 |
| 4 | `SelectCandidate`: wParam=3, lParam=0x100 → idx=3, sid=0x100 |
| 5 | pipe 写端关闭后读端收 EOF,不解引用空指针 |

#### 2.7.3 扩展现有

| 测试 | 改动 |
|---|---|
| `test/TestShiftSelectBinding/`(spec 014) | +2 yaml 负断言:`Shift+Shift_L send 2` 不存在;`Shift+Shift_R send 3` 不存在 |
| `test/TestDefaultHotkeys/` | binding 总数 -2 断言(line 240-241 删除) |
| `test/TestResponseParser/` | 无需改(不影响响应解析) |
| `test/TestBindingResolution/` | 无需改(不动 binding 解析逻辑) |

#### 2.7.4 端到端 sandbox-verify

按 handoff §1 的 5 个场景 + 2 个边界,在 `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1` 跑:

| # | 操作 | 期望日志 / 行为 |
|---|---|---|
| ① | 输入"ni"→ 候选 → Shift_L 不松 | 无 `select_candidate`;UI 不变 |
| ② | 同上,松 Shift_L | `select_candidate(idx=1)`;选第 2 |
| ②' | 同上,Shift_R | `select_candidate(idx=2)`;选第 3 |
| ③ | 候选 → Shift+a → 松 Shift | preedit 有 'A';无 `select_candidate` |
| ④ | 候选空 → Shift+空格 | ascii_mode toggle;无 `select_candidate` |
| ⑤ | Shift+, | 《 上屏;无 `select_candidate` |
| US1-G | Shift down 后 alt-tab 松开 | 不触发(跨 session) |
| US1-H | deployer 跑中按 Shift | 吞事件,deployer 结束恢复 |

### 2.8 ADR: `docs/adr/0007-shift-release-ipc.md`

MADR 格式,记录:

- **Context**: spec 014/012/L18/L19/L21 8 次 yaml-only 失败;TSF `KeyEvent.cpp:27-28` 强制 RELEASE_MASK;librime `key_event.h:43` operator== 包含 RELEASE_MASK,导致 release-only binding 不可表达
- **Decision**: 放弃 yaml 层修复,改走 IPC 状态机——TSF 上报 Shift down/up,Server 维护 per-session 状态,ShiftUp alone 时调 `rime_api->select_candidate`
- **Consequences**:
  - **Pro**: 根治 release-only 触发,与 hotkey-binding.md「Server 单一 source of truth」一致
  - **Con**: 新增 3 条 IPC 命令 + 状态机 + 4 边同步(虽然 Deployer 不动);server-side 状态生命周期需小心
  - **Mitigation**: 单元 + 集成 + sandbox 端到端测试覆盖 11 + 5 + 8 路径
- **Alternatives rejected**:
  - yaml 层改 `key_binder/bindings`(已尝试 8 次失败,L18/L19 反复 ship 又撤回)
  - 改 `WeaselTSF/KeyEvent.cpp` 的 mask 行为(L10 平台限制,导致字母 a down 时 shiftHadInterveningKey 错乱)
  - `ascii_composer/switch_key: Shift_L: commit_text`(违反场景 ①,有候选时不该 commit)
  - `kill ctfmon`(性能灾难,TSF 服务降级)

### 2.9 Risks

- **R1**: Server 状态机忘记 reset → 后续 Shift 误触发。Mitigation: `ShiftUp` 末尾无条件 `s = ShiftState{}`;Test #8 覆盖 reset。
- **R2**: 跨 session stale state(用户 alt-tab 后 Shift release 在新 session)误触发。Mitigation: state per-`WeaselSessionId`;US1-G + Test #11 覆盖。
- **R3**: maintenance 期间 Shift 事件污染状态机。Mitigation: 3 个 virtual impl 顶部 `if (m_disabled) return`;US1-H + Test #8(Reset path)覆盖。
- **R4**: `ProcessKeyEvent` handler 加 `interveningKey = true` 可能误判(letter 'a' 上屏前 release 算不算中间键)。Mitigation: 仅 **down** 事件算中间键(`mask & RELEASE_MASK == 0`),letter release 不触发;Test #3/#6 覆盖。
- **R5**: TSF pipe 断开期间 Shift down 状态丢失。Mitigation: `m_shiftDown` 在 TSF 本地保留;pipe 重连后 Server 收到 orphan ShiftUp → state 无 downRecorded → 忽略;Test #5(覆盖)验证。
- **R6**: yaml `bindings` 删除后用户 `user-custom/*.custom.yaml` 重新添加两条 binding 回到"按下就上屏"行为。Mitigation: 本期不修(留 spec 075+ 跟进),在 `installation.yaml` 写警告注释。
- **R7**: `librime/` 子模块改动?Mitigation: 无——本次纯 Fluxing 端改动,不改 librime。
- **R8**: `weasel.props` / `env.bat` 误提交?Mitigation: commit checklist §1 `git status -sb` + `git diff --stat` 强约束。
- **R9**: 删 yaml binding 时字节错位引入 BOM 或 LF-only。Mitigation: 字节级 delete + 验证 BOM absence + CR/LF parity(L11 强约束)。

### 2.10 Verification procedure

1. **Build**:
   ```
   msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
   ```
   Expected:0 错误。

2. **Unit + integration tests**:
   ```
   test\TestShiftIPCStateMachine\Release\TestShiftIPCStateMachine.exe
   test\TestShiftIPCWire\Release\TestShiftIPCWire.exe
   test\TestShiftSelectBinding\Release\TestShiftSelectBinding.exe
   test\TestDefaultHotkeys\Release\TestDefaultHotkeys.exe
   test\TestResponseParser\Release\TestResponseParser.exe
   ```
   Expected:11 + 5 + (existing) + (existing) + (existing) 全部 pass。

3. **Installer build**:
   ```
   _build_v02101.ps1   # 沿用 handoff §6 脚本
   ```
   Expected:`output\archives\fluxing-0.21.0.1-installer.exe` + md5 与 `_check_dll.ps1` 对比。

4. **sandbox-verify**:
   ```
   F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 `
       -InstallerPath release\fluxing-0.21.0.1-installer.exe `
       -Scenarios "1,2,2',3,4,5,G,H"
   ```
   Expected:8 场景全 pass。

5. **TSF DLL 强制重载**:用户必须 Win+L 锁屏 + 解锁(DLL 进程缓存)。

6. **日志检查**:
   - `%TEMP%\rime.weasel.*.INFO.*.log` 无 `unrecognized modifier 'shift'`
   - 无 `invalid key binding` 警告
   - 含新 `select_candidate(idx=1/2)` 日志

7. **commit message**:
   ```
   feat(WeaselIPC): v0.21.0.1 shift release-only candidate select
   
   - Add 3 IPC commands (ShiftDown/ShiftUp/SelectCandidate)
   - Server-side per-session state machine (ShiftState)
   - TSF eats Shift events to prevent double-fire
   - yaml: remove Shift+Shift_L/R send 2/3 from rime_ice.schema.yaml
   - Tests: 11 unit + 5 integration + sandbox 8 scenarios
   ```

## 3. Out of scope

- **Shift+l / Shift+r 复合键**(L16 已确认不需要)
- **Shift+Shift_L/R 触发 ascii_mode toggle**(由 `Shift+space` 既有 binding 接管,ascii_composer.switch_key.Shift_L/R: noop 保留)
- **librime 子模块改动**(本次纯 Fluxing 端)
- **user-custom/*.custom.yaml 用户覆盖检测**(留 spec 075+ 跟进,本期仅在 `installation.yaml` 写警告)
- **installer NSIS 改动**(本次 IPC 重构不涉及装机脚本)
- **weasel.props / env.bat 跟踪改动**(已在 .gitignore;commit 时 verify)
- **upstream PR**(P8 waiver,brand-fork only)
- **新 CI workflow 改动**(现有 `test:` job 自动 pick up 新 vcxproj)

## 4. Done criteria

- spec.md / plan.md / tasks.md 都在 `.specify/specs/077-shift-release-ipc/`,committed
- docs/adr/0007-shift-release-ipc.md committed
- .specify/memory/lessons-learned.md 新增 L##-PhaseM-10 段 committed
- 11 unit + 5 integration tests pass
- 3 现有测试(TestShiftSelectBinding/TestDefaultHotkeys/TestResponseParser) 仍 pass
- `output\archives\fluxing-0.21.0.1-installer.exe` ship artifact 产出 + md5 dual-verify
- sandbox-verify 8 场景全 pass(US1-A..US1-H)
- CHANGELOG.md `[0.21.0.1]` 段 committed
- commit message 符合 P4(feat + WeaselIPC scope)
- 2 commits 拆分:`feat(WeaselIPC)` 主变更 + `chore(release)` 版本 bump + installer