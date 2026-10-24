- [2026-07-18] Pattern-Key: `loop-engineering-five-layer`
    现象: 多数 AI 工作流仍停留在「单次 prompt - 回答 - 结束」 思维，user 实际充当「人肉胶水」 拼接各 Agent。
    正确做法: 目标导向改造为五层闭环 — **L5 编排层 (TaskCreate + task.md) → L1 执行层 (Read→Edit→verify) → L2 审核层 (verification-before-completion + AskUserQuestion 红线) → L3 沉淀层 (memory/YYYY-MM-DD.md + .learnings/) → L4 进化层 (Skill 「已知陷阱」 章节 + 重复模式晋升)**
    防重犯: Harness ≥ 80% (本项目已达成) 才能走 LE 路径;否则 LE 是「失控的发动机」。Generator/Evaluator 必须分离 (写的人和查的人分开) — 检查者要独立 session / 独立 skill, 不能同上下文复用。
    External State 是地基 — 记忆必须在磁盘 (`memory/YYYY-MM-DD.md` + `.learnings/` + `MEMORY.md`), 不能塞上下文窗口。
    Recurrence-Count: 1 (LE 元任务首条)
    任务引用: task.md Phase LE-3.4 + LE 元任务整体
    跨 Agent: 适用于任何 Claude Code 项目 (不只是 Fluxing)。复制本条目到用户其他项目的 `.learnings/SHARED/` 即可。
