---
name: context-engineering
description: Optimize agent context setup. Use at session start, when output quality degrades, when switching tasks, or setting up a project for AI-assisted development.
---

# Context Engineering (Project Onboarding)

> Feed agents the right information at the right time. Context is the single biggest lever for agent output quality — too little and the agent hallucinates, too much and it loses focus.

## When to use

- Starting a new coding session
- Agent output quality is declining (wrong patterns, hallucinated APIs)
- Switching to a different part of the codebase
- Setting up a new project for AI assistance
- Agent is not following project conventions

## The Context Hierarchy

Structure context from most persistent to most transient:

```
┌─────────────────────────────────────┐
│ Project constitution (always load)  │  constitution.md, AGENTS.md
├─────────────────────────────────────┤
│ Spec / plan / tasks (current task)  │  .specify/specs/NNN-*
├─────────────────────────────────────┤
│ Code map (project overview)         │  docs/Fluxing-code-map/*
├─────────────────────────────────────┤
│ Module deep-dive (when touching)    │  docs/Fluxing-code-map/04
├─────────────────────────────────────┤
│ Lessons (relevant L## only)         │  .specify/memory/lessons-learned.md
├─────────────────────────────────────┤
│ Current code (only files in diff)   │  per file, not whole tree
├─────────────────────────────────────┤
│ Transient (test output, git log)    │  just-in-time, not cached
└─────────────────────────────────────┘
```

## Fluxing project context (always load)

When starting a session on this project, the agent should be told:

1. **Constitution** at `.specify/memory/constitution.md`
   - 5 原则 (Intent / Test-Backed / Spec-Artifact / Clarification / Incremental)
   - 9 硬规则 (R1-R9)
   - P1-P8 项目规则 (P8 = brand fork Fluxing)

2. **AGENTS.md** at repo root
   - Build commands (`xbuild.bat weasel installer`, `build.bat all`)
   - Dangerous zones (NSIS, env.bat, librime, thirdparty, secrets, release/)
   - Anti-patterns (A1-A13)

3. **Project knowledge** at `.specify/memory/project-knowledge.md`
   - 12-section速查: directory, deploy layers, config override, user dict vs phrase, hotkeys, yaml edit, QuickPanel, FluxingComponents, build, NSIS traps, commands, roadmap

4. **Code map** at `docs/Fluxing-code-map/`
   - 01-overview (project)
   - 02-architecture (processes, IPC, state)
   - 03-build-pipeline
   - 04-module-deep-dive (per module)
   - 05-data-and-config
   - 06-customization-points (brand fork)

5. **PRD** at `.specify/PRD.md` (v2 scope, F1-F13)

6. **TDD** at `.specify/TDD.md` (test strategy)

7. **Lessons** at `.specify/memory/lessons-learned.md` (L01-L55, lookup by keyword)

## What to include in current context

- ✅ Current spec.md / plan.md / tasks.md
- ✅ Specific files in the diff
- ✅ Relevant L## (use Grep by keyword)
- ✅ Test output for current task
- ❌ Whole source tree (read on demand)
- ❌ Unrelated spec history
- ❌ Test code for unrelated features

## Common failure modes

- **"AI knows RIME"**: RIME 1.13 has very specific quirks (modifier case, action names). Always cite docs / L##.
- **"Skip the constitution"**: R1-R9 are non-negotiable. Read before any non-trivial work.
- **"Read all L##"**: Use Grep by keyword. Don't dump 4000 lines into context.

## Anti-patterns

- Dumping whole repo into context
- Skipping constitution / AGENTS.md
- Reading every lessons-learned entry
- "I'll figure it out" (use codebase-recon-skill instead)
