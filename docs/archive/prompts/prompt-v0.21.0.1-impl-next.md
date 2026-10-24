# Prompt — v0.21.0.1 shift-hotkey-ipc implementation next

承接 `handoff-v0.21.0.1-shift-hotkey-ipc-plan-done-2026-08-15.md`。

**基线**: Fluxing HEAD = `ff03ad8` (`docs(spec): v0.21.0.1 plan + tasks` 已 commit)。`ahead 2` on top of base `3ab771c`。

**预存未提交 diff**(严禁纳入本次实现 commit):
- `M CLAUDE.md` + `M librime` (既有,不是本任务)

**必交付(T01..T22 严格按 tasks.md 顺序)**:
- T01–T05: 11 unit test red→green (ShiftStateMachine)
- T06–T12: 3 IPC commands + Client + Server dispatch + TSF Shift detection
- T13–T14: 5 wire integration tests
- T15: yaml 字节级删除 (`default.yaml:240-241`,NOT `rime_ice.schema.yaml`)
- T16–T17: 扩展 TestShiftSelectBinding + TestDefaultHotkeys
- T18: Rebuild + 全 tests
- T19–T20: installer + md5 + sandbox-verify 8 场景
- T21: `feat(WeaselIPC)` ship commit + ADR 0007 + L##-PhaseM-10 + CHANGELOG [0.21.0.1]
- T22: `chore(release)` version bump

**禁止事项**:
- ❌ `git add -A` / 误提交 `weasel.props` / `env.bat` / `*.obj`
- ❌ 改 `WeaselTSF/KeyEvent.cpp` mask / TSF `shiftHeld` 全局 / librime/ 子模块
- ❌ 状态机漏 reset (T10 末尾无条件 `s = ShiftState{}`)
- ❌ TSF thread 阻塞 I/O (P2 强约束;ShiftDown/Up 只发不收)

**加速模式**:
- 不开 subagent 独立验证(图谱已熟)
- TDD 红→绿循环优先于直接写实现
- 每 task 终止条件 + commit 见 tasks.md

**完成后**: 写 `handoff-v0.21.0.1-shift-hotkey-ipc-shipped-<date>.md` + 本会话收尾。

**关键已校对**: binding 在 `default.yaml` 非 `rime_ice.schema.yaml`;Server dispatch 在 `WeaselServerImpl.cpp` 非 `ServerImpl.cpp`;handler header 在 `include/RimeWithWeasel.h` 非子目录。
