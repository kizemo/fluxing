# Skills Index (Fluxing 项目)

> **28 个项目所需 skills**,从 codex 翻译而来。**所有 skills 都是项目级**(`F:\soft\00selfmade\rime_claude\.claude\skills\`),跟随 git。
>
> 用法:`/skill-name` 调用,或在 chat 里说"使用 brainstorming skill"。

## 完整 skill 列表(28 个)

| # | Skill | 用途 | 对应 codex skill |
|---|---|---|---|
| 01 | spec-init | 三件套(spec/plan/tasks) | spec-init |
| 02 | spec-check | 校验一致性 | spec-check |
| 03 | spec-driven-development | 4 阶段 spec 门控 | spec-driven-development |
| 04 | brainstorming | 设计前 1-question 对话 | brainstorming |
| 05 | interview-me | 提取用户意图 | interview-me |
| 06 | idea-refine | 想法精炼 | idea-refine |
| 07 | systematic-debugging | 5 阶段调试流程 | systematic-debugging |
| 08 | debugging-and-error-recovery | fix + 验证 + 文档化 | debugging-and-error-recovery |
| 09 | verification-before-completion | 证据-断言铁律 | verification-before-completion |
| 10 | tdd | 红绿重构循环 | test-driven-development |
| 11 | test-guard | 测试代码审查 | test-guard |
| 12 | clean-code-guard | 产线代码审查 | clean-code-guard |
| 13 | code-review | 多轴 code review | code-review-and-quality |
| 14 | code-simplification | 简化复杂度 | code-simplification |
| 15 | planning-and-task-breakdown | 任务分解 | planning-and-task-breakdown |
| 16 | writing-plans | 完整 plan 文档 | writing-plans |
| 17 | incremental-implementation | 增量实现 | incremental-implementation |
| 18 | doubt-driven-development | 反方审查 | doubt-driven-development |
| 19 | git-workflow | git 操作规范 | git-workflow-and-versioning |
| 20 | context-engineering | 上下文管理 | context-engineering |
| 21 | codebase-recon | git-history 健康探针 | codebase-recon-skill |
| 22 | api-and-interface-design | 接口设计 | api-and-interface-design |
| 23 | documentation-and-adrs | 文档决策记录 | documentation-and-adrs |
| 24 | frontend-ui-engineering | UI 工程质量 | frontend-ui-engineering |
| 25 | performance-optimization | 性能优化 | performance-optimization |
| 26 | observability-and-instrumentation | 可观测性 | observability-and-instrumentation |
| 27 | security-and-hardening | 安全加固 | security-and-hardening |
| 28 | shipping-and-launch | 预发布 checklist | shipping-and-launch |

## 不安装的 codex skills

- **browser-testing-with-devtools** — Fluxing 是 native Win32,无 browser
- **clean-code-guard / test-guard / docs-guard** — 项目同名,只保留与项目相关的
- **code-simplification** — 已翻译为 `code-simplification`
- **ci-cd-and-automation** — 项目 CI 简单,无独立 skill 价值
- **deprecation-and-migration** — P8 waiver 已通过 spec 002 + constitution 实施,不需要专门 skill
- **dispatching-parallel-agents** — 用 claude-code 的 Agent / TaskCreate 替代
- **parallel-agent-worktree-skill** — 用 git worktree 原生命令
- **performance-optimization** — 已翻译
- **guizang-ppt-skill / ppt-master / word-format** — 文档生成,Fluxing 不需要
- **long-doc-processor** — 通用工具,Fluxing 不常使用
- **ponytail / ponytail-audit / ponytail-debt / ponytail-gain / ponytail-help / ponytail-review** — codex 内部命名空间
- **repo-atlas** — 用 `codebase-recon` 替代
- **requesting-code-review** — 用 `code-review` 替代
- **skill-creator** — 一次性工具,需要时手动创建
- **using-agent-skills** — meta-skill,claude-code 不适用

## 调用方式

1. **直接 invoke**: `/skill-name` 或 `/skill-name <args>`
2. **chat 里说**: "使用 brainstorming skill" 或 "按 systematic-debugging 流程"
3. **自动触发**: 描述任务时如果匹配 skill description,claude-code 会自动 load

## 关键 cross-ref

每个 SKILL.md 头部都引用项目关键 L##:
- L09/L13/L17: NSIS install.nsi
- L10/L14: x64 架构一致性
- L19/L21: Shift 键位
- L42/L48/L49/L52: byte-verify / message map / atexit
- L55: MaintenanceGuard RAII
- constitution.md 5 原则 + 9 硬规则 + P1-P8
- AGENTS.md §3.5 (分支/tag) + §5 (pre-commit checklist)
