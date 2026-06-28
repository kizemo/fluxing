<!--
SYNC IMPACT REPORT
==================
Version: 1.1.0 (amendment 1)
Context: Single-developer mid-term maintenance of rime/weasel
         (Windows Rime IME front-end, C++/WTL/Boost, Visual Studio 2017+).
Source of truth: distills existing project conventions from
                 .clang-format, INSTALL.md, CHANGELOG.md, weasel.sln, .github/workflows/ci.yml.

Amendment 1 (1.0.0 → 1.1.0, MINOR):
  - Adds P8 "Brand Fork: Fluxing / 火流猩输入法", granting an explicit,
    scoped waiver of P6 (user-data path) for the brand-fork worktree.
  - Adds "Skills Mapping" appendix, clarifying that all skills referenced
    in this document (spec-init, spec-check, brainstorming, interview-me,
    codebase-recon, verification-before-completion, source-driven-development)
    are installed at the USER level (`~/.codex/skills/`), are not project-
    level artifacts, and are invoked by the main agent declaring the skill
    and following the SKILL.md procedure (no MCP tool handle).
  - Adds `installer` and `fluxing` to the Conventional Commits scope set (P4).
  - Ratified/Amended date: 2026-06-28.

No principle (I-V) was removed or redefined; no hard rule (R1-R9) was removed.
Backwards-compatible with v1.0.0 plans that did not foresee a brand fork.

Amendments: see Governance §Amendment Process.
-->

# Rime / Weasel Project Constitution

This constitution governs all AI-assisted changes to the **rime/weasel** codebase
(Windows input method front-end for the Rime Input Method Engine).

It is a **project-level** document and overrides `~/.codex/AGENTS.md` where they
conflict. In case of conflict, this document wins.

---

## Project Context (for AI agents)

- **Language**: C++ (C++14/17), WTL (Windows Template Library), MFC, ATL
- **Platform**: Windows 8.1 ~ Windows 11
- **Build**: Visual Studio 2017+ (`weasel.sln`), `xmake.lua`, or `xbuild.bat`
- **Dependencies**: Boost >= 1.60, librime (submodule, `librime/`), OpenCC, NSIS
- **License**: GPLv3 — every change MUST remain GPL-compatible
- **Code style**: `.clang-format` based on Chromium (uses tabs, see `.clang-format`)
- **Commits**: Conventional Commits, modules named `WeaselTSF`, `WeaselServer`, etc.
- **Audience**: end users are Chinese typists; all UX strings and error messages stay in Chinese

---

## Core Principles (binding)

### I. Intent Before Implementation
Every change MUST start with a clear intent:
- **What** capability or fix is being added
- **Why** it matters to the user (the typing experience)
- **Acceptance criteria** that can be observed (typed behavior, no crash, etc.)

If the user request is vague, ask. Do not assume. Examples of vague requests
that MUST be clarified:
- "Make it faster" (which operation? which metric?)
- "Fix the input lag" (which input method? which app context?)
- "Improve the UI" (which screen? which user pain?)

### II. Test-Backed Change (NON-NEGOTIABLE)
- Bug fixes MUST include a regression test when feasible (e.g. `test/TestResponseParser`).
- New code paths SHOULD have unit tests under `test/`.
- "It compiles and runs" is NOT verification. Run the test, paste the output.
- For pure UI / TSF behavior where unit tests are hard, the agent MUST
  document manual verification steps and reference the affected build.

### III. Spec-Artifact Discipline
For any change touching **≥ 3 files** or **spanning multiple sessions**:
- Create `.specify/specs/NNN-<short-name>/` with `spec.md` + `plan.md` + `tasks.md`
- Spec stays tech-agnostic at the user-experience level
- Plan may reference `WeaselTSF`, `WeaselServer`, etc. by name
- Tasks include exact file paths from this project

Trivial changes (typo, one-liner, single-file obvious bug) do NOT need spec/plan/tasks,
but MUST show intent + acceptance in the chat response.

### IV. Structured Clarification
- Unknowns marked `[NEEDS CLARIFICATION: <specific question>]` in spec/plan.
- Use `interview-me` skill when user prompt is underspecified.
- Use `brainstorming` skill before any user-facing UX change.

### V. Incremental Delivery
- Slice work so each piece is independently testable (compile + manual test).
- Each user story in `spec.md` MUST be shippable on its own.
- Default to "ship the smallest fix that solves the user's problem first."

---

## Hard Rules (9 — verifiable, MUST)

| # | Rule | Verification |
|---|---|---|
| **R1** | Before any code change, articulate intent + acceptance criteria in the chat response | First message of a change shows "What / Why / How verified" |
| **R2** | No tech-stack words in `spec.md` (WHAT layer). Tech names only in `plan.md` and `tasks.md` | `grep -E "WTL\|Boost\|C++\|MFC" spec.md` returns nothing |
| **R3** | Every task has a single priority P1/P2/P3 and a user story tag `[US1]` etc. | Visible in `tasks.md` |
| **R4** | Constitution Check table is filled in `plan.md` before implementation | `plan.md` contains "## Constitution Check" section with all 5 principles |
| **R5** | Task granularity: single task ≤ 4 hours, 1-3 files, has clear input/output | Reject vague tasks; rewrite or split |
| **R6** | "Done" requires pasted test output OR documented manual verification steps | From `verification-before-completion` skill |
| **R7** | One source of truth per concept. Conflict between spec/plan/tasks = stop and run `spec-check` | Run `spec-check` skill before resolving |
| **R8** | Specs are versioned in git, not chat | `git log .specify/specs/` shows dated entries |
| **R9** | Lookup beats memory. For unfamiliar modules, read `include/*.h` and `librime/README` before assuming | From `source-driven-development` skill |

---

## Project-Specific Rules (binding for rime/weasel)

### P1. Code Style
- Follow `.clang-format` (Chromium-based, tabs, no sort-includes).
- Run `clang-format.ps1` or `clang-format.sh` before commit.
- Header guards: `#pragma once` (preferred) or `#ifndef WEASEL_<NAME>_H_`.
- Names: classes `PascalCase`, functions `PascalCase`, members `camelBack_`,
  constants `kCamelCase`, macros `UPPER_SNAKE`.

### P2. Windows-Specific Constraints
- **NO** use of `std::wcout` / `printf` for user-facing strings. Use ATL/MFC
  conversion helpers (`CW2A`, `CA2W`, `CString`).
- User-visible error messages stay in **Chinese (Simplified)**, matching existing
  strings in `WeaselDeployer`, `WeaselTSF`, `WeaselSetup`.
- TSF module: respect `ITfContext`, `ITfComposition`, and the input thread
  model. Never block the TSF thread for I/O.
- Subprocess IPC (`WeaselServer` ↔ front-ends): use the existing `WeaselIPC`
  protocol; don't invent new message types without updating
  `WeaselIPCData.h` AND the receiving side.

### P3. Build & Dependencies
- Do NOT add new third-party dependencies without a justification in `plan.md`
  (existing deps: Boost, librime submodule, OpenCC, WinSparkle).
- Submodule changes (`librime/`) require explicit user approval and a separate
  PR/commit referencing upstream librime commit.
- VS solution is the source of truth for project membership; `xmake.lua` is a
  mirror — keep them in sync.

### P4. Commit & PR
- Conventional Commits: `type(scope): subject` (e.g. `fix(WeaselTSF): handle empty RimeUserDir`).
- Scopes: `WeaselTSF`, `WeaselServer`, `WeaselDeployer`, `WeaselSetup`, `WeaselUI`, `WeaselIPC`, `RimeWithWeasel`, `librime`, `installer`, `docs`, `ci`, `fluxing` (brand-fork work only, see P8).
  `WeaselIPC`, `RimeWithWeasel`, `librime`, `installer`, `docs`, `ci`.
- Update `CHANGELOG.md` under "### 主要更新" for user-visible changes.
- Each PR/commit MUST be self-contained and pass `ci.yml`.

### P5. UX / 用户体验
- Never silently change a default key binding, scheme behavior, or user-visible
  string without a CHANGELOG entry.
- When adding a feature, respect the existing "no global hotkey conflicts"
  convention (Ctrl+`, F4 are reserved; check `WeaselConstants.h`).
- Color / style changes go in the appropriate YAML scheme, not hardcoded.

### P6. Backwards Compatibility
- User data lives at `%AppData%\Rime`. NEVER change this path; it's referenced
  in registry, uninstaller, and docs.
- Existing user customizations (`*.yaml`, user dictionaries) MUST continue to
  work after any change to the loader.

### P7. Subprocess Lifecycle
- `WeaselServer` is the single supervisor. Front-ends communicate via the
  named pipe protocol in `WeaselIPC.h`.
- Do NOT spawn new long-lived processes from front-ends.

---
### P8. Brand Fork: Fluxing / 火流猩输入法  (AMENDMENT 1, v1.1.0)
This project MAY be forked into a separate brand **"Fluxing" (中文: 火流猩输入法)**
on a dedicated long-lived branch (e.g. `Fluxing`). Such a fork is permitted to
diverge from the upstream Rime/Weasel identity in a **scoped, auditable** way.

**Scope of allowed changes (only while the brand-fork branch is active):**
- Product name strings: "Weasel"/"小狼毫" → "Fluxing"/"火流猩输入法" in
  `*.rc` string tables, NSIS `LangString`s,
  `WeaselUtility.h::get_weasel_ime_name()`, tray menu, README/INSTALL/CHANGELOG,
  `update/appcast.xml` title.
- TSF GUIDs (`c_clsidTextService`, `c_guidProfile` in `WeaselSetup/imesetup.cpp`)
  and the `PSZTITLE_HANS / PSZTITLE_HANT` strings that embed them — **must be
  freshly generated**, never reused.
- Binary file names: `weasel*.dll / WeaselServer.exe / WeaselDeployer.exe /
  WeaselSetup.exe` → `fluxing*.dll / FluxingServer.exe / FluxingDeployer.exe /
  FluxingSetup.exe`. The `arm64x_wrapper` DLL name follows the same rule.
- Windows services, registry paths, WinSparkle registry path,
  `WtsApi*`-style mutex names (`WeaselDeployerExclusiveMutex`, the
  `WeaselServer.exe` literal in `_EnsureServerConnected`, the
  `WeaselIME` service name) — rebrand in lockstep.
- `RimeTraits::app_name` / `distribution_name` / `distribution_code_name`
  in `RimeWithWeaselHandler::_Setup`.
- `.ico` assets under `resource/`, `WeaselSetup/WeaselSetup.ico`.
- `.github/workflows/ci.yml` artifact names and `appcast` URL.

**P6 waiver (scoped):**
P6 is waived **only for the brand-fork branch**, **only with respect to**:
  - `HKCU\Software\Rime\Weasel` and `HKCU\Software\Rime\weasel` (case
    mismatch is an upstream bug; the fork MUST consolidate to a single
    path under `HKCU\Software\Fluxing\`).
  - `WeaselUserDataPath()` default `%AppData%\Rime`. The fork MAY change
    this default to `%AppData%\Fluxing`, **and** MUST ship a one-shot
    migration that, on first run of Fluxing, copies (not moves) the
    existing `%AppData%\Rime` contents into `%AppData%\Fluxing` and
    records the original path in `HKCU\Software\Fluxing\ImportedFrom`
    for rollback.
  - The single-mutex `WeaselDeployerExclusiveMutex` MUST be renamed, but
    `WeaselServer` MUST additionally probe the legacy mutex for one
    release cycle and forward a graceful stop to the legacy server before
    starting.

**Out of scope (fork MUST NOT change):**
- RIME engine behavior (librime API, schemas, key tables, OpenCC tables).
- The IPC wire protocol in `WeaselIPC.h` / `WeaselIPCData.h` message
  *semantics*. Cosmetic rebranding of `distribution_code_name` is allowed;
  inventing new message types is still governed by P2.
- `Boost`, `OpenCC`, `librime` submodule contents.
- `.clang-format` style (governed by P1).

**Required plan.md disclosures (Constitution Check, per change):**
- Whether each change touches the legacy user-data path and which P6-waiver
  clause it invokes.
- Whether the change introduces a new IPC message type (must update both
  sides per P2).
- CHANGELOG.md entry under `### 主要更新` for every user-visible string,
  default, or path change (per P5).

**Reverting:** the `Fluxing` branch MUST keep a documented rollback path
(e.g. tag `pre-fluxing-fork`) so that the brand fork can be abandoned
without leaving user data in an unrecoverable state.


## Quality Gates (per change)

Before any commit:
1. ✅ `clang-format` applied to changed files
2. ✅ `weasel.sln` builds (or relevant project, if isolated change)
3. ✅ Tests pass if `test/` was touched (e.g. `TestResponseParser`, `TestWeaselIPC`)
4. ✅ CHANGELOG.md updated for user-visible changes
5. ✅ Commit message follows Conventional Commits + uses a recognized scope

Before any PR (when working with upstream):
1. ✅ All five above
2. ✅ Manual test on Windows 10/11
3. ✅ No new compiler warnings at default warning level

---

## When to Use Spec-Driven Development

| Change Scope | Required Artifacts |
|---|---|
| Single-line typo, comment fix, one-liner obvious bug | None (intent + acceptance in chat) |
| Small bug fix in one module (e.g. `WeaselTSF` color parsing) | Mini-plan in chat (3-5 lines) |
| New feature touching 2-3 files | `spec.md` + `tasks.md` (use `spec-init`) |
| New feature touching ≥ 4 files OR spans multiple sessions | Full `spec.md` + `plan.md` + `tasks.md` (use `spec-init` + `spec-check`) |
| Refactor across modules, dependency upgrade, IPC change | Full SDD + PR to upstream + maintainer discussion |

---

## Development Workflow

1. **Read first**: explore relevant code in `include/`, `librime/`, or `weasel.sln`
   before writing. Use `codebase-recon` skill on unfamiliar code.
2. **Plan**: for non-trivial work, run `spec-init` skill to produce artifacts.
3. **Constitution Check**: run `spec-check` skill after spec/plan/tasks.
4. **Implement**: follow `tasks.md` in order; mark `[P]` tasks as parallel.
5. **Verify**: run `verification-before-completion` before claiming done.
6. **Format**: run `clang-format` on changed files.
7. **Commit**: Conventional Commits with module scope.
8. **Changelog**: update if user-visible.
9. **PR** (if working with upstream): ensure all 9 hard rules pass.

---

## Skills Mapping (AMENDMENT 1, v1.1.0)

All skills referenced in this constitution are **user-level** Codex skills,
installed under `~/.codex/skills/<skill-name>/SKILL.md` (Windows:
`C:\Users\<user>\.codex\skills\<skill-name>\SKILL.md`). They are **not**
committed to the repository and are **not** invoked via a tool handle. The
main agent invokes a skill by *declaring* it in the chat response and then
following the `SKILL.md` procedure step by step. Sub-agents may execute the
task work described in a skill, but **reading and interpreting a `SKILL.md`
is the main agent's responsibility** (per the `using-agent-skills` meta-skill).

| Skill | Purpose | Where it applies in this project |
| --- | --- | --- |
| `using-agent-skills` | Meta: discover and apply the right skill | Always; pick a skill before starting a non-trivial task. |
| `spec-driven-development` | When & how to write spec.md / plan.md / tasks.md | Triggered by Principle III when a change touches >= 3 files or spans sessions. |
| `spec-init` | Bootstrap a SDD feature spec from a natural-language request | Run before writing any spec/plan/tasks. Has a 4-question pre-flight. |
| `spec-check` | Cross-validate spec/plan/tasks against this constitution | Run after spec/plan/tasks, before implementation. Produces the Constitution Check table. |
| `brainstorming` | One-question-at-a-time design dialogue; HARD-GATE no code before design approval | Run when the request is creative or behaviour-changing. |
| `interview-me` | Surface what the user actually wants (one question + best guess each turn) | Run when the ask lacks "for whom / why now / success criterion / binding constraint". |
| `codebase-recon` | Git-history-driven health probe before reading code | Run before Principle R9 lookup on unfamiliar modules. |
| `source-driven-development` | Verify implementation against authoritative documentation | Used when touching a public API (RimeTraits, IPC message semantics per P2). |
| `verification-before-completion` | Evidence-before-assertion gate for "done" claims | Enforces R6: paste test output or document manual steps before claiming completion. |
| `code-review-and-quality` | Multi-axis code review | Before merging or before claiming a slice complete. |
| `git-workflow-and-versioning` | Trunk-based discipline, atomic commits, scopes, commit messages | P4 enforcement; consult before each commit. |
| `ci-cd-and-automation` | Configure CI quality gates | Touching .github/workflows/ci.yml (e.g. P8 brand-fork CI). |
| `deprecation-and-migration` | Manage old-system retirement and user-data migration | Required for the P8 one-shot %AppData%\Rime -> %AppData%\Fluxing migration. |
| `documentation-and-adrs` | Capture ADRs and project docs | Used for docs/Fluxing-code-map/* and any future ADRs. |
| `idea-refine` | Refine raw ideas into sharp concepts | Optional precursor to spec-init when the request is fuzzy. |
| `writing-plans` / `planning-and-task-breakdown` | Break work into implementable tasks | Used inside spec-init to produce tasks.md. |
| `incremental-implementation` | Deliver changes in vertical slices | Default style for executing tasks.md. |
| `doubt-driven-development` | Adversarial review of non-trivial decisions in-flight | Use during tasks.md execution when a decision feels off. |
| `test-driven-development` | Failing test first, then make it pass | Principle II / R6 enforcement where unit tests are feasible. |
| `systematic-debugging` | Root-cause debugging | Use before any fix attempt. |
| `debugging-and-error-recovery` | Step-by-step debug workflow | Companion to systematic-debugging. |
| `frontend-ui-engineering` | Production-quality UI work | Touching WeaselUI/, WeaselPanel, layout or render code. |
| `security-and-hardening` | OWASP-aware input validation, ACL, DACL | Touching WeaselIPCServer/SecurityAttribute.cpp or any new IPC channel. |
| `observability-and-instrumentation` | Logs / metrics / traces | Adding or changing log lines / counters (LOG, DLOG, DebugStream). |
| `shipping-and-launch` | Pre-launch checklist, rollback | Tagging a Fluxing release; preparing the one-shot migration. |

**If a referenced skill is missing on the host** (e.g. an old image, a CI
runner), the agent MUST stop and surface a blocker rather than guess. Do
not silently substitute an unrelated skill.

---

## Governance

This constitution supersedes ad-hoc convention. Conflicts are resolved by
amending the spec/plan/tasks, not by diluting the principle.

### Authority
- Principles I-V are binding. The Constitution Check in `plan.md` MUST
  be evaluated against them before implementation.
- Project-specific rules P1-P7 are binding within the rime/weasel codebase.

### Amendment Process
1. Open a PR with rationale and a version bump.
2. Update SYNC IMPACT REPORT at top of this file.
3. Adjust dependent skills (`spec-init`, `spec-check`) in the same PR if needed.
4. Maintainer approval required for MAJOR bumps.

### Versioning (SemVer for governance)
- **MAJOR**: principle removal/redefinition, or hard rule removal
- **MINOR**: new principle, new hard rule, or material expansion
- **PATCH**: clarifications, examples, typo fixes

### Compliance Review
- Every PR review MUST verify compliance with Principles I-V and relevant P-rules.
- Justified deviations MUST be recorded in `plan.md` Complexity Tracking.
- Unjustified deviations block merge.

---

**Version**: 1.1.0 | **Ratified**: 2026-06-28 | **Last Amended**: 2026-06-28 (amendment 1: P8 + Skills Mapping)

