---
name: clean-code-guard
description: Review generated/changed PRODUCTION code before ship, using Clean Code, SOLID, DRY, KISS, YAGNI, and LLM-specific failure-mode checks. Use after a coding agent writes code, before commit/merge.
---

# Clean Code Guard

> Reactive pass after the first implementation. Reject code bloat. Apply the rules below as a guard pass, not as a from-scratch reviewer.

## When to use

- "Review this PR" / "Is this safe to merge?" / "Make this cleaner" / "Audit this code"
- After a coding agent produced implementation code
- Before committing non-trivial changes

## When NOT to use

- Factual/conceptual questions
- CI/tooling config
- Git workflow
- Running/debugging tests
- Test code review (use test-guard instead)

## Universal rules (apply in priority order)

### BLOCKER (must fix before ship)

- **B1**: Function >100 lines (split)
- **B2**: Class >500 lines or >10 responsibilities (SRP violation)
- **B3**: Magic number without named constant (e.g., `if (size > 4096)` → `kMaxBufferSize`)
- **B4**: Resource leak (raw new without smart pointer / delete, FILE* without fclose)
- **B5**: Memory-unsafe cast (`reinterpret_cast` without `static_cast` first, or no bounds check)
- **B6**: Public API change without updating header / .rc / .nsi / docs (L02 byte-level rule applies to .h changes too)
- **B7**: Hard-coded path (`C:\Program Files\Fluxing` — should be `HKLM\...InstallDir` registry lookup)

### WARNING

- **W1**: Duplicated logic (DRY) — extract to helper
- **W2**: Premature optimization (YAGNI) — remove unless profiler says so
- **W3**: Dead code (commented-out blocks, unreachable branches, unused functions) — delete
- **W4**: Inconsistent naming (P1: PascalCase classes, camelBack_ members, kCamelCase constants, UPPER_SNAKE macros)
- **W5**: Header without `#pragma once` (project preference per P1)
- **W6**: Stringly-typed API (use enum or strongly-typed struct)
- **W7**: Catch-all exception (`catch (...)`) without rethrow or specific log
- **W8**: Mutable global state (project uses `m_*` members, not globals)
- **W9**: `std::cout` / `printf` for user-visible strings (P2 requires ATL `CW2A` / `CString`)
- **W10**: Missing `override` on virtual method override
- **W11**: Missing `const` on getter or `const&` parameter
- **W12**: Inconsistent `nullptr` vs `NULL` (project: `nullptr` for C++, `NULL` for Win32 interop)

### INFO (style)

- **I1**: Inconsistent brace style (project uses Chromium .clang-format: attached braces, tabs)
- **I2**: Commented-out code (delete, not comment)
- **I3**: Magic string (`"Fluxing"`) without named constant

## Rime/Weasel-specific gotchas

- **L09 / L13 / L17**: NSIS changes require AGENTS.md §2.5 silent-install smoke test
- **L10**: librime is Win32-only; no x64 build, no `${If} ${RunningX64}` for Weasel binaries
- **L14**: Weasel*.exe must be x86; only `weaselx64.dll` is x64
- **L40 / L44 / L45**: byte-level file writes (don't use PowerShell string APIs)
- **L47**: BOM only on .nsi; .h/.cpp must be LF only
- **L49**: ATL message map is runtime — function declared ≠ function reachable

## Output format

```
Review of WeaselServer/Foo.cpp (lines X-Y):

🔴 BLOCKER B1: HandleKeyEvent is 250 lines
   Fix: split into HandleKeyEvent_Down + HandleKeyEvent_Up + Dispatch

🟡 WARNING W3: Commented-out old implementation in lines 50-80
   Fix: delete, git history preserves it

🟢 INFO I1: Mixed brace styles (some attached, some on new line)
   Fix: run clang-format.ps1 -i

Lines reviewed: 250
Blockers: 1, Warnings: 1, Info: 1
```
