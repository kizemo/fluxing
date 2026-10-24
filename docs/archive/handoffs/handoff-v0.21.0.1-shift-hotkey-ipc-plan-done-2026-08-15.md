# Handoff — v0.21.0.1 Shift hotkey IPC · planning 完成 (2026-08-15)

> **任务**: spec 077 实施规划(spec.md / plan.md / tasks.md 全部落盘)→ 实现接力
> **本会话进度**: 写作 planning 完成; **实现未启**,交棒下一会话从 T01 开始按 TDD 红/绿循环
> **状态**: spec 已 commit (1506abc) + plan+tasks 已 commit (ff03ad8)
> **基线**: Fluxing HEAD = ff03ad8 (ahead 2 commits on top of base 3ab771c)
> **交接原因**: brainstorming (上会话) → writing-plans (本会话) → implementation (下会话),三段式大流水线,本会话在干净边界交出

---

## 0. 必读资料(按顺序)

1. **本 handoff** (本文件)
2. **`handoff-v0.21.0.1-shift-hotkey-ipc-bc-2026-08-15.md`** — brainstorming 上下文(5 决策点)
3. **`.specify/specs/077-shift-release-ipc/spec.md`** (371 行)
4. **`.specify/specs/077-shift-release-ipc/plan.md`** (337 行)— 全局约束 + 文件地图 + Interface Contracts + Risk Register + Source-of-Truth Citations + Done criteria
5. **`.specify/specs/077-shift-release-ipc/tasks.md`** (1655 行)— T01..T22 TDD 步骤

---

## 1. 已完成的工作(本会话)

### 1.1 Commit `1506abc` — spec.md 单独提交

仅添加 `.specify/specs/077-shift-release-ipc/spec.md` (372 行),commit message:
```
feat(spec): v0.21.0.1 shift-release-ipc brainstorming spec
```

### 1.2 Commit `ff03ad8` — plan.md + tasks.md 联合提交

按 writing-plans skill 输出 + 用户偏好位置:
- `plan.md` (337 行) — meta 文档(Goal / Architecture / Global Constraints C1-C6 / File Map / Interface Contracts / Risk Register R1-R13 / Verification Procedure / Done criteria)
- `tasks.md` (1655 行) — T01..T22 TDD 实施步骤

commit message:
```
docs(spec): v0.21.0.1 shift-release-ipc plan + tasks
```

### 1.3 实测校对纠正(spec/handoff 文件名行号错位)

新会话实现时注意,**spec/handoff 中以下项需以实测为准**:

| 项 | spec/handoff 写 | 实测 |
|---|---|---|
| binding 文件 | `rime_ice.schema.yaml:240-241` ❌ | **`output/data/default.yaml:240-241`** ✓ |
| Server dispatch | `WeaselIPCServer/ServerImpl.cpp` ❌ | **`WeaselIPCServer/WeaselServerImpl.cpp`** ✓ |
| Handler header | `RimeWithWeasel/RimeWithWeasel.h` ❌ | **`include/RimeWithWeasel.h`** ✓ |

Tasks.md 的 T15/T08/T09 第一步均已用 Grep 或 Read 实测定位。

### 1.4 Risk Register 新增(plan.md §R10-R13,实测驱动)

- **R10** `Shift_L/R` + letter 复合事件时 `_lpbKeyState` 仍可能注入 `SHIFT_MASK` → `key_binder` 层 shift 复合仍触发。Mitigation 在 T12 已显式 early-return(eat 整事件,letter 路径不进入)。
- **R11** TSF `m_shiftDown` 是 per-instance (单 instance,即 TSF 单例);跨 session 切 alt-tab 不会 race 因为状态在 Server side (per `WeaselSessionId`)。
- **R12** spec.md / handoff 的 `rime_ice.schema.yaml` 实际上是 `default.yaml`——T15 第一步 grep 验证。
- **R13** T06 (enum + 接口) → T07 (Client impls) → T08 (Server dispatch) 必须按顺序,否则编译失败。Tasks.md T06/T07/T08 严格按序。

### 1.5 spec §4 Done criteria 验收表(下会话最终核对)

- [x] spec.md / plan.md / tasks.md 都在 `.specify/specs/077-shift-release-ipc/`,committed (spec=1506abc, plan+tasks=ff03ad8)
- [ ] docs/adr/0007-shift-release-ipc.md committed(T21)
- [ ] `.specify/memory/lessons-learned.md` L##-PhaseM-10 committed(T21)
- [ ] 11 unit + 5 integration tests pass(T01-T05, T13-T14)
- [ ] 3 现有测试(扩展后)pass: TestShiftSelectBinding/TestDefaultHotkeys/TestResponseParser (T16-T17)
- [ ] installer ship artifact + md5 dual-verify (T19)
- [ ] sandbox-verify 8 scenarios pass (T20)
- [ ] CHANGELOG.md [0.21.0.1] committed (T21)
- [ ] commit message P4 (`feat(WeaselIPC)` + `chore(release)` 2 commits) (T21 + T22)

---

## 2. 下一步必交付(新会话)

按 `writing-plans` skill 输出的 tasks.md 顺序执行,**严格 TDD**:

```
T01..T05: 11 unit test (ShiftStateMachine)
T06..T08: 3 IPC commands wire (WeaselIPC.h + Client + Server dispatch)
T09..T10: Server state machine impl (RimeWithWeasel.h + .cpp 3 virtual)
T11..T12: TSF Shift detection + eating (WeaselTSF.h + KeyEventSink.cpp)
T13..T14: 5 wire integration tests
T15    : yaml 字节级删除 (default.yaml:240-241)
T16..T17: 扩展现有 TestShiftSelectBinding (2 neg) + TestDefaultHotkeys (count -2)
T18    : Rebuild + run all unit/integration tests
T19    : installer build + md5 dual-verify
T20    : sandbox-verify 8 scenarios
T21    : feat(WeaselIPC) commit + ADR + L## + CHANGELOG
T22    : chore(release) version bump
```

每个 task 终止条件 + commit message 见 tasks.md。

执行到 T20 完成 → sandbox log pass 后做 T21 commit;T22 是最后一步。

---

## 3. 环境/分支/基线状态

- **分支**: `Fluxing`
- **HEAD**: `ff03ad8` (docs(spec): v0.21.0.1 plan + tasks)
- **改动文件状态**:
  - 已 commit: `.specify/specs/077-shift-release-ipc/{spec.md, plan.md, tasks.md}` (= 1506abc + ff03ad8)
  - **预存未提交 diff**(`M CLAUDE.md` + `M librime`):**不属于本任务**,**严禁**误纳入主 commit
  - 工作树 `--ahead 2`,`--behind 0` 相对远程 `kizemo/Fluxing`
- **必备工具/路径**:
  - `_build_v02101.ps1` (沿用 handoff §6)
  - `_check_dll.ps1` (md5 dual-verify,L100)
  - `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath <path>` (L54)
  - **不用 `git describe --tags`** 取版本号 (AGENTS.md §6)

---

## 4. 避坑提示(继承 handoff §5 + 本会话新增)

| # | 坑 | 踩雷后果 | 避雷 |
|---|---|---|---|
| 1 | 改 `WeaselTSF/KeyEvent.cpp` mask | L10 平台限制,治标不治本 | T12 显式禁用,R10 mitigation |
| 2 | TSF 加 `shiftHeld` 全局 | 违反 hotkey-binding.md | 不加,TSF 仅 `m_shiftDown` 本地;状态在 Server |
| 3 | 改 `librime/` 子模块 | P3 强约束 | **本期不碰**;无 librime 改动 |
| 4 | 字节级 yaml delete 引入 BOM 或 LF-only | L07/L09/L11 | T15 PowerShell 字节验证 + BOM absence + CRLF parity |
| 5 | 状态机漏 reset → 后续 Shift 误触发 | R1 | `ShiftUp` 末尾无条件 `s = ShiftState{}`(C6, T10) |
| 6 | `git add -A` / 把 `weasel.props` / `env.bat` / `*.obj` 误提交 | C4 / A10 | T21 + T22 用 explicit paths;`git status -sb` + `git diff --cached --stat` 必检 |
| 7 | `rime_ice.schema.yaml` 误当成 binding 位置 | R12 / T15 第一动作 grep | 已纠正:binding 在 `default.yaml` |
| 8 | `RequestHandler::ShiftDown` build 失败(假设) | 误判 link fail | **不** link fail,因为 virtual 默认 `{}`,T08 commit 是 valid 的 |
| 9 | `output/archives/*` 误纳入 | 已被 .gitignore | 用 `_build_v02101.ps1` 后 md5 + log,**不 commit 安装包** |
| 10 | spec.md / handoff 文件名错位 | R12-R13 | spec.md 后续变成权威源;plan/tasks.md 已实测代码定位 |

---

## 5. 必读顺序(下会话新 Claude / 工程师)

1. 先读本 handoff(本文件)
2. 然后读 `spec.md` 理解 PRD + TDD + Out-of-scope + Done criteria
3. 然后读 `plan.md` 理解架构 + 6 个 Global Constraints + 13 条 Risk
4. 然后读 `tasks.md` 顶部「Commit strategy」+ 底部「Cross-Reference Index」
5. 开始 **T01**(ShiftStateMachine class skeleton + 11 unit tests red)
6. 严格按 TDD 红→绿循环推进至 T22

---

## 6. 子会话检查点(checkpoint)

| 节点 | 检查 | 通过条件 |
|---|---|---|
| T05 完成 | 11 unit tests 全绿,no regression | `TestShiftIPCStateMachine.exe` 报 11 PASS / 0 FAIL |
| T12 完成 | TSF eats Shift,IPC 发送,R10 风险已锁定 | sandbox 手测 US1-A 候选不变 + shift 写日志 `ShiftDown ipc_id=...` |
| T18 完成 | 11 unit + 5 wire + 3 现有扩展测试全 PASS | aggregate tally = 0 fail |
| T20 完成 | sandbox 8 场景全 pass | `rime-verify.ps1` 报 8/8 + log evidence |
| T22 完成 | 2 commits `feat(WeaselIPC)` + `chore(release)` | `git log --oneline -5` 看到两条 |

任一 checkpoint 失败 → STOP + 写 `incident-<topic>-<date>.md` + handoff。

---

新会话第一句话直接引用: `prompt-v0.21.0.1-impl-next.md`(≤30 行,同目录)。新会话从 T01 开始。
