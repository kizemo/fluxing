# Prompt — v0.21.0.1 Shift hotkey IPC 重构 · writing-plans + implementation

承接 `handoff-v0.21.0.1-shift-hotkey-ipc-bc-2026-08-15.md`(必读)。

## 当前状态

- 基线:`Fluxing` 分支 HEAD = `3ab771c`(v0.21.0.0 shipped)
- spec 已写完 + 自查通过: `.specify/specs/077-shift-release-ipc/spec.md`(300+ 行,**未 commit**)
- 5 个架构决策已确认: **B / A / A / A / A**(Server 拥有 / 2 条新 IPC / TSF 吃 / 绝对下标 / 不探测菜单)

## 必交付(顺序)

1. **commit spec.md**:`feat(spec): v0.21.0.1 shift-release-ipc brainstorming spec`(只 add spec.md)
2. **调 `writing-plans` skill** → 输出 `plan.md` + `tasks.md` 到同目录(> spec 已 ready,直接调)
3. **按 tasks.md 顺序实现**:
   - 先写 11 unit test 让它 fail(TDD 红)
   - 再写 Server 状态机让它 green
   - TSF 端吃事件 + IPC wire
   - yaml 删除 2 条 binding(字节级 + BOM/CRLF)
   - build + 现有测试仍 pass
   - installer build + md5 dual-verify
   - sandbox-verify 8 场景
   - 写 ADR `docs/adr/0007-shift-release-ipc.md`
   - 写 `.specify/memory/lessons-learned.md` L##-PhaseM-10
   - CHANGELOG.md `[0.21.0.1]` 段
   - 2 commits:`feat(WeaselIPC)` + `chore(release)`

## 禁止事项

- ❌ 改 `WeaselTSF/KeyEvent.cpp` 的 mask(L10 治标不治本)
- ❌ 在 WeaselTSF 加 `shiftHeld` 全局状态(违反 hotkey-binding.md)
- ❌ 改 `librime/` 子模块(P3 强约束)
- ❌ `git add -A` / 把 `weasel.props` / `env.bat` / `*.obj` 误提交
- ❌ 把过往 send_text binding 改动混到本次 commit
- ❌ TSF thread 阻塞 I/O(P2 强约束)
- ❌ state machine 漏 reset(永远在 ShiftUp 末尾无条件 `s = ShiftState{}`)

## 加速模式

- 不要 subagent 独立验证,改用 Read + Grep 直读
- TDD 红绿循环优先(`TestShiftIPCStateMachine` 先 fail 再 green)
- 字节级 yaml 删除后立即验证 BOM absence + CR/LF parity(L11)

## 装机工具

- `_build_v02101.ps1` build · `_check_dll.ps1` md5
- `F:\soft\00selfmade\sandbox-verify\rime-claude\rime-verify.ps1 -InstallerPath <path>` 真机验证
- TSF DLL 强制重载:用户必须 Win+L 锁屏 + 解锁

## 不变量(实现时反复对照)

```
OnShiftUp: 无条件 reset  // handoff §8
OnProcessKeyEvent: 仅 down 事件 + 非 Shift keycode 算 intervening
idx = s.lastIsLeft ? 1 : 2  // 0-based absolute
```