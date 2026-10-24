# Prompt — v0.21.0.1 Shift hotkey IPC 重构 (next session)

> **下一会话的第一句话**。直接复制下面整段贴到新会话即可。

---

承接 Fluxing v0.21.0.1 Shift hotkey IPC 重构任务。完整 handoff 在
`F:/soft/00selfmade/rime_claude/handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md`
(必读 §1-§4 任务背景与根因)。

**第一步(必做)**:
1. Read 整个 handoff.md (10 节,含 8 次失败路径 → 不要重蹈覆辙)
2. Read `F:/soft/00selfmade/rime_claude/Rime-设置说明书-重点快捷键.md` §6 快捷键绑定 + §9 本机现状 + §10 排错
3. Read `F:/soft/00selfmade/rime_claude/.claude/rules/hotkey-binding.md` — **架构规则,不可违反**
4. `git -C "F:/soft/00selfmade/rime_claude" status -sb` 确认源码干净
5. `ls -lt "D:/Program Files/fluxing/weasel/weasel.dll"` 确认装机 DLL md5 = `120d02e2` (git HEAD baseline)
6. `tasklist | grep -iE "weasel|ctfmon"` 确认 TSF 进程状态

**关键认知**:
- 用户诉求:按 Shift 不松不上屏,松 Shift 上屏第 2/3 候选,Shift+字母 出大写字母
- 已经 8 次尝试失败(详见 handoff §2),都集中在 TSF mask 路径
- 唯一可行路径:**IPC hotkey 重构**(WeaselTSF → WeaselServer,服务端调 `rime_api->select_candidate`)
- **不要**在 WeaselTSF 维护新的 hotkey 表/状态(违反 hotkey-binding.md)

**约束**:
- 不改现有 `WeaselIPC::Client::HotkeyBinding` 单 source of truth 规则
- 不改 `default.yaml` 到 `commit_text`(已被否)
- 不在 `KeyEvent.cpp` 改 mask 行为(已验证 8 次失败)
- librime 子模块改动单独 commit
- 不要把过往 send_text binding 改动混到本次
- 改动必须经过 spec.md + plan.md + TDD + sandbox-verify

**关键文件位置**:
- Handoff: `F:/soft/00selfmade/rime_claude/handoff-v0.21.0.1-shift-hotkey-ipc-2026-08-14.md`
- 说明书: `F:/soft/00selfmade/rime_claude/Rime-设置说明书-重点快捷键.md`
- 规则: `F:/soft/00selfmade/rime_claude/.claude/rules/hotkey-binding.md`
- 架构总览: `F:/soft/00selfmade/rime_claude/.research/rime-docs/Rime-customization-digested.md`
- 核心源: `WeaselTSF/KeyEventSink.cpp`, `WeaselServer/WeaselServerApp.cpp`, `RimeWithWeasel/RimeWithWeasel.cpp`, `include/WeaselIPCData.h`
- rime API: `librime/src/rime_api.h:436` (select_candidate)

**rime_api 已有 select_candidate(session_id, index) 0-indexed 调用** — 选第 2 候选 = `select_candidate(session, 1)`。这是 IPC 路径绕开 key_binder 的关键 API。

**必交付**:
1. spec.md + plan.md (spec 077 or N77)
2. 最小化代码修改: WeaselTSF 上报 Shift 状态 + WeaselServer 接收 + rime_select_candidate 调用
3. yaml 改动: `default.yaml` 删除 `Shift+Shift_L/R send 2/3` binding
4. 单元测试覆盖:IPC 路径 + spec 014 测试更新
5. rebuild installer + sandbox-verify
6. commit (Conventional Commits + 模块前缀)

**禁止反模式**:
- ❌ 在 WeaselTSF 加 shiftHeld/shiftHadInterveningKey 状态(违反 hotkey-binding.md)
- ❌ 改 `KeyEvent.cpp` mask 行为(8 次失败路径)
- ❌ `kill ctfmon` 强制 TSF 重载(性能灾难)
- ❌ 直改 `build/*.yaml`(build 重建时丢失)
- ❌ 改 `librime` 的 key_event operator==(改 rime 公共 API = 跨项目影响)
- ❌ 修改 `output/archives/` 历史 installer
- ❌ 把 send_text binding 改动 + IPC 重构 放一个 commit

**优先顺序**:yaml-first → 最小化代码 → IPC 路径 → 完整测试 → sandbox-verify → installer

**建议**: 第一步用 `codebase-memory-mcp` 查 `WeaselIPC::Hotkey` 的现有实现,搞清楚 IPC 扩展点的位置;第二步再决定具体修改方案。不要凭记忆猜。

---

**Prompt 结束。新会话第一句请直接复制以上整段。**
