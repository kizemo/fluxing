# Prompt — Rime 自定义机制研究 + 修复会话 (next session)

> 下一会话的第一句话。直接复制下面整段贴到新会话即可。

---

承接 Fluxing Rime 自定义机制研究 + 标点修复任务。完整 handoff 在
`F:\soft\00selfmade\rime_claude\handoff-rime-customization-2026-08-01.md`。

**第一步(必做)**:
1. Read 整个 handoff.md (10 节,含路径索引、文件位置、待验证清单)
2. Read `.research/rime-docs/Rime-customization-digested.md` (我的消化理解, 724 行)
3. git status -sb verify 当前状态 (期望: 跟 handoff §0 一致, working tree 不动)
4. ls `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` verify 修复文件在位
5. ls `D:\Program Files\fluxing\user1\fluxing\build\rime_ice.schema.yaml` 检查 `__build_info` 里 `rime_ice.custom` 是否已变成非 0 (说明 Rime 自动 redeploy 已发生)
6. 简短报告: 当前状态 + 上次完成情况 + 待继续的方向

**关键文件位置**:
- Handoff: `F:\soft\00selfmade\rime_claude\handoff-rime-customization-2026-08-01.md`
- 消化理解: `F:\soft\00selfmade\rime_claude\.research\rime-docs\Rime-customization-digested.md`
- Active Rime user dir: `D:\Program Files\fluxing\user1\fluxing\`
- 修复的 custom: `D:\Program Files\fluxing\user1\fluxing\rime_ice.custom.yaml` (8 标点 + Shift)
- 源 doc fetched: `F:\soft\00selfmade\rime_claude\.research\rime-docs\` (7 个文件)

**约束**:
- 不动 Fluxing 项目 source code (v0.74.3 WIP 在 stash@{0} 保留)
- 不重 build v0.62
- 不写新 spec 除非 user 明确要求
- 不动 $APPDATA/Rime 旧残留 (有 user 私货,删前要 confirm)
- 任何改动先 Read handoff.md
- 自我设 2 小时活跃时间预算

**用户原话上下文**:
> "1. 请根据文档，修改自定义设置而非代码，实现在中文输入法状态，输入 /，输出中文 、符号。
> 2. 撰写Handoff.md，并撰写 prompt，另开新会话继续执行任务，确保新会话理解任务背景/进度/新抓取的官方文档。"

**Part 1 状态**: ✅ 已完成 (migrate rime_ice.custom.yaml 到 active dir, Rime 自动 redeploy 待触发)

**Part 2 状态**: ✅ 已完成 (本 handoff + 本 prompt)

**可继续的方向**(handoff §6 详):
- A: 验证 + 完善本次修复 (回归测试, 同步到其他方案)
- B: 扩展 Rime 自定义 (custom_phrase / lua_translator / OpenCC / 配色)
- C: 写 spec 固化 (`.specify/specs/NNN-rime-customization/`)
- D: 跨项目借鉴 (风险高, 需明确授权)

**优先级建议**: A > B > C > D

---

**Prompt 结束。新会话第一句请直接复制以上整段。**
