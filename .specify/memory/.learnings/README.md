<!--
SYNC IMPACT REPORT
==================
Version: 1.0.0 (initial)
Context: Loop Engineering L3/L4 补缺口层 — 单次经验入站口
Relationship to existing files:
  - 现有 `.specify/memory/lessons-learned.md` (L## 永久归档) 不动
  - 现有 `.specify/memory/project-knowledge.md` (项目知识) 不动
  - 现有 `.specify/memory/constitution.md` (R/P 宪法) 不动
  - 本目录是 lessons-learned 的**前置缓冲** — 单次经验先入这里
-->

# `.learnings/` — 单次经验临时入站口

> Loop Engineering 视角下的「沉淀层 Loop」(L3) 中间层。
> 详见 `CLAUDE.md` §6.3 与 §6.4。

## 用途

补 constitution / lessons-learned 的两个缺口：

1. **错误经验**：所有单次错误必须先入 `ERRORS.md`，`Recurrence-Count ≥ 3` 自动晋升到 `lessons-learned.md` L##
2. **正向经验**：用户纠正 / 最佳实践入 `LEARNINGS.md`，按 category 分类

## 目录结构

```
.learnings/
├── README.md         ← 本文件
├── ERRORS.md         ← 单次错误条目（带 Recurrence-Count）
├── LEARNINGS.md      ← 用户纠正 (correction) + 最佳实践 (best_practice)
└── SHARED/           ← 跨 Agent / 跨项目通用经验
```

## 晋升规则（系统级触发，不依赖 Agent 判断）

```
单次错误 → ERRORS.md (Recurrence-Count=1)
    ↓ 同 Pattern-Key 再次出现
Recurrence-Count=2 → 仍留 ERRORS.md
    ↓ 第三次
Recurrence-Count=3 → 自动晋升到 lessons-learned.md L## (标记 Pattern-Key)
    ↓ 跨项目通用
走 constitution Amendment Process 晋升 R/P
```

## 与现有体系的关系

| 文件 | 内容 | 生命周期 | 写入人 |
|---|---|---|---|
| `memory/YYYY-MM-DD.md` | 当天日志 | 1 天 | Agent 自动 |
| `.learnings/ERRORS.md` | 单次错误 | Recurrence-Count < 3 | Agent 自动 |
| `.learnings/LEARNINGS.md` | 单次正向 | 跨任务适用前 | Agent 自动 |
| `.learnings/SHARED/` | 跨 Agent 共享 | 永久 | Agent 自动 |
| `lessons-learned.md` L## | 跨任务适用 | 永久 | 系统晋升（≥ 3 次） |
| `constitution.md` R/P | 团队级 | 永久 | 走 Amendment |

## 条目模板

### ERRORS.md 条目

```markdown
- [YYYY-MM-DD] Pattern-Key: `<短关键词>`
    现象: <一句话错误现象>
    原因: <为什么会犯>
    修复: <怎么修的>
    防重犯: <怎么验证不会重犯>
    Recurrence-Count: 1
    任务引用: <task.md Phase 或 commit hash>
```

### LEARNINGS.md 条目

```markdown
- [YYYY-MM-DD] category: <correction | best_practice>
    用户纠正: <user 原话或意图>（仅 correction）
    场景: <何时这么做>
    做法: <应该怎么做>
    效果: <对比上期差异 %>
    任务引用: <task.md Phase 或 commit hash>
```