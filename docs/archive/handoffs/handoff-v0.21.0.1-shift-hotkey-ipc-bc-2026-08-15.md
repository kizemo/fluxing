# Handoff — v0.21.0.1 Shift hotkey IPC 重构 · brainstorming 完成 + spec 已写 (2026-08-15)

> **任务**:实现「按 Shift 不松不上屏,松 Shift 上屏第 2/3 候选,Shift+字母 出大写字母不上屏」,通过 IPC hotkey 路径重构
> **本会话进度**:brainstorming 完成(spec.md 写完 + 自查通过),5 个核心架构决策全部确认
> **状态**:spec.md 未 commit(等用户授权);基线 v0.21.0.0 shipped(commit `3ab771c`),Fluxing HEAD 干净
> **交接原因**:brainstorming → writing-plans → implementation 是大流水线,本会话在干净边界交出

---

## 0. 必读资料(按顺序)

1. **本 handoff**(本文件)
2. **`.specify/specs/077-shift-release-ipc/spec.md`** — 已写完 + 自查通过,300+ 行,5 节(§0 Background / §1 PRD / §2 TDD / §3 Out of scope / §4 Done criteria),是 brainstorming 的完整产物
3. **`handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md`** — 上一个 handoff(继承它的 §2-§4 失败路径 + 根因 + 推测方案)
4. **`.claude/rules/hotkey-binding.md`** — **架构强约束**,不要在 WeaselTSF/Server/Deployer 维护私有 hotkey 表
5. **`.claude/rules/ipc-boundary.md`** — IPC 4 边同步规则 + TSF thread 不能阻塞 I/O
6. **`Rime-设置说明书-重点快捷键.md` §6.8 + §9.3** — Shift release bug 根因 + v0.21.0.1 fix baseline

---

## 1. 已完成的工作(本会话)

### 1.1 Brainstorming — 5 个澄清问题 + 5 节设计

| 问题 | 选项 | 用户选 | 体现位置 |
|---|---|---|---|
| ① Shift 状态机归谁 | A. TSF / **B. Server** / C. 拆分 | **B** | spec §2.1 + §2.4 |
| ② IPC 消息粒度 | **A. 2 条新命令** / B. 复用 + 元数据 / C. 单条状态枚举 | **A** | spec §2.3 + §2.2 |
| ③ TSF 是否吃 Shift | **A. 吃** / B. 双发 | **A** | spec §2.1 + KeyEventSink 改动 |
| ④ 候选下标 | **A. 绝对 + 新 IPC** / B. 页内复用 | **A** | spec §2.3 `WEASEL_IPC_SELECT_CANDIDATE` |
| ⑤ 是否探测菜单 | **A. 不探测** / B. 先 candidate_list_begin / C. 缓存 | **A** | spec §2.4 ShiftUp 不查 menu |

### 1.2 Design 5 节 — 全部已呈现给用户

- §A 架构(模块边界 + 3 边 IPC 表面 + wire encoding)
- §B 组件(TSF / Server / 状态机 / 生命周期)
- §C 数据流(5 个场景 + 跨 session / maintenance 边界,**用户校对了场景 1/2 必须显式覆盖 Shift_R 分支**)
- §D 错误处理与边界(7 个边界:pipe 断 / 重连 / 跨 session / maintenance / session 销毁 / 双按 Shift / yaml 残留)
- §E 测试覆盖(11 unit + 5 integration + 3 现有扩展 + 8 sandbox 场景)

### 1.3 spec.md(已写,未 commit)

- 路径:`.specify/specs/077-shift-release-ipc/spec.md`
- 300+ 行,沿用 spec 014 格式(0 Background / 1 PRD / 2 TDD / 3 Out of scope / 4 Done criteria)
- 自查通过:无 placeholder、内部一致(决策表 ↔ 设计 ↔ 测试 ↔ 验证)、scope 单 ship 可完成、无歧义

---

## 2. 下一步必交付(新会话)

按 brainstorming skill 的**终止状态**——`writing-plans`:

### 2.1 立刻可做(本会话尾巴)

1. **commit spec.md**:`feat(spec): v0.21.0.1 shift-release-ipc brainstorming spec`
   - 仅含 spec.md(plan.md / tasks.md 等 writing-plans skill 输出)
   - 检查 `git status -sb` + `git diff --stat` 确认只 add spec.md

### 2.2 新会话第一段:writing-plans

```bash
# 在新会话开篇直接调:
Skill(skill="writing-plans", args=".specify/specs/077-shift-release-ipc/spec.md")
```

writing-plans 会输出:
- `.specify/specs/077-shift-release-ipc/plan.md`(实现细节 + Constitution Check + Source-of-Truth Citations + Risk Register)
- `.specify/specs/077-shift-release-ipc/tasks.md`(任务清单,每个 task 带 P1/P2 优先级 + US1-A..US1-H 链接)

### 2.3 新会话第二段:实现 + ship

按 tasks.md 顺序,典型路径:

```
T01..T03:写 11 unit test (`test/TestShiftIPCStateMachine/`)
T04..T06:实现 Server 端状态机(`RimeWithWeaselHandler` 3 个 virtual)
T07..T09:实现 TSF 端(`KeyEventSink.cpp` Shift 检测 + 吃事件)
T10..T12:实现 IPC wire(`WeaselIPC.h` + `WeaselClientImpl.cpp` + `ServerImpl` dispatch)
T13:yaml 删除(rime_ice.schema.yaml:240-241,字节级 + BOM/CRLF 验证)
T14..T16:扩展现有 test(2 条 yaml 负断言 + binding 总数 -2)
T17..T18:build + unit/integration test 全跑
T19..T20:installer build + md5 dual-verify
T21..T22:sandbox-verify 8 场景 + commit `feat(WeaselIPC)` + `chore(release)` + ADR 写 + L## 写
```

---

## 3. 关键文件位置(新会话按需 Read,不要重读)

### 核心源(已读,可跳读)

- `WeaselTSF/KeyEventSink.cpp` — `_ProcessKeyEvent` 入口(line 11-63)
- `WeaselTSF/KeyEvent.cpp` — `ConvertKeyEvent` 的 mask 逻辑(line 4-59),RELEASE_MASK 在 line 27-28
- `WeaselTSF/WeaselTSF.h` — 加 `m_shiftDown` + `m_shiftDownIsLeft`
- `WeaselIPC/WeaselClientImpl.cpp` — 现有 `_SendMessage` 模板可抄
- `include/WeaselIPC.h` — 加 enum + Client 方法 + RequestHandler virtual
- `include/WeaselIPCData.h` — **不需要改 wire struct**(packed 风格 wParam/lParam 已够)
- `RimeWithWeasel/RimeWithWeasel.cpp` — `ProcessKeyEvent` line 281-310 是加 `interveningKey = true` 的位置
- `RimeWithWeasel/RimeWithWeasel.h` — 3 个 virtual 声明
- `librime/src/rime_api.h` — `select_candidate` line 436

### 配置(已读,改动点明确)

- `output/data/rime_ice.schema.yaml:240-241` — **删** `Shift+Shift_L send 2` + `Shift+Shift_R send 3`
- `output/data/default.yaml:76-77` — **不动**(`Shift_L: noop` 保留,与本次路径无冲突)

### 测试(已分类)

- 新建:`test/TestShiftIPCStateMachine/`(unit,纯状态机,无 rime_api)
- 新建:`test/TestShiftIPCWire/`(integration,mock pipe)
- 扩展:`test/TestShiftSelectBinding/`(2 条 yaml 负断言)
- 扩展:`test/TestDefaultHotkeys/`(binding 总数 -2)

### 文档(新会话要写)

- `docs/adr/0007-shift-release-ipc.md`(spec §2.8 已给模板)
- `.specify/memory/lessons-learned.md` L##-PhaseM-10(spec 077 落地)
- `CHANGELOG.md` `[0.21.0.1]` 段

### 装机产物

- `output\archives\fluxing-0.21.0.1-installer.exe` — ship artifact
- `release\fluxing-0.21.0.1-installer.exe` — copy to release/

---

## 4. 必交付清单(新会话的 ship-gate)

按 spec §4 Done criteria:

- [ ] spec.md / plan.md / tasks.md 都在 `.specify/specs/077-shift-release-ipc/`,committed
- [ ] `docs/adr/0007-shift-release-ipc.md` committed
- [ ] `.specify/memory/lessons-learned.md` L##-PhaseM-10 段 committed
- [ ] 11 unit + 5 integration tests pass
- [ ] 3 现有测试(TestShiftSelectBinding/TestDefaultHotkeys/TestResponseParser) 仍 pass
- [ ] `output\archives\fluxing-0.21.0.1-installer.exe` 产出 + md5 dual-verify
- [ ] sandbox-verify 8 场景全 pass
- [ ] CHANGELOG.md `[0.21.0.1]` 段 committed
- [ ] commit message 符合 P4(`feat` + `WeaselIPC` scope)
- [ ] 2 commits:`feat(WeaselIPC)` 主变更 + `chore(release)` 版本 bump

---

## 5. 禁止事项(踩过的坑)

- ❌ 改 `WeaselTSF/KeyEvent.cpp` 的 mask 行为(L10 平台限制,治标不治本)
- ❌ 在 `WeaselTSF` 加 `shiftHeld` 全局状态跟踪(违反 hotkey-binding.md)
- ❌ 改 `librime/` 子模块(P3 强约束,需要 ADR + 用户授权;本次不动)
- ❌ 把过往会话的 send_text binding 改动混到本次 commit
- ❌ `git add -A` / 把 `weasel.props` / `env.bat` / `*.obj` 误提交
- ❌ `release/` 旧 installer 不清就直接 ship
- ❌ 不写 `installation.yaml` 警告注释,用户 `*.custom.yaml` 覆盖 binding 后回到"按下就上屏"

---

## 6. 加速模式规则(本次适用)

- **不要 subagent 独立验证**:本任务图谱已熟(spec 077 自己 brainstormed 出来),改用 Read + Grep 直读
- **TSF thread 阻塞 I/O 禁止**:`_ProcessKeyEvent` 内只发 IPC,不当场等 Server 回执(P2 强约束)
- **状态机 idempotent**:Server 端 `ShiftUp` 无条件 reset,reconnect / orphan event 不污染
- **TDD 红绿循环**:TestShiftIPCStateMachine 先写 fail test,再写 state machine 让它 green

---

## 7. 加速 / 工具

- `_build_v02101.ps1` — 沿用 handoff §6 的 build 脚本(已有)
- `_check_dll.ps1` — md5 验证
- `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath <path>` — 真机验证

---

## 8. 状态机不变量(新会话实现时反复对照)

```
OnShiftDown(isLeft):
    s[sid].downRecorded = true
    s[sid].interveningKey = false
    s[sid].lastIsLeft = isLeft

OnShiftUp(isLeft):
    if (s[sid].downRecorded && !s[sid].interveningKey):
        idx = s[sid].lastIsLeft ? 1 : 2
        rime_api->select_candidate(sid, idx)
        _UpdateUI(sid)
    s[sid] = ShiftState{}   // 永远 reset

OnProcessKeyEvent(keyEvent, sid):
    if (!(keyEvent.mask & RELEASE_MASK) &&
        keyEvent.keycode != Shift_L &&
        keyEvent.keycode != Shift_R):
        if (s[sid].downRecorded):
            s[sid].interveningKey = true
```

**任何修改都不能破坏:ShiftUp 末尾无条件 reset。**

---

准备好!新会话开篇:`prompt-v0.21.0.1-shift-hotkey-ipc-write-plan-next.md` 第一句话直接引用本 handoff 路径。