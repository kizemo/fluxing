---
paths:
  - "test/**/*.cpp"
  - ".specify/specs/**/*.md"
  - "CHANGELOG.md"
---

# Commit checklist (project-specific)

The general development workflow lives in `AGENTS.md §5` and
`.specify/memory/constitution.md` §Development Workflow. **This file
covers the Fluxing branch–specific commit gate and project-rule
verification.**

## Pre-commit (5 steps, in order)

1. **clang-format** on every file you touched:
   ```bash
   clang-format -i <changed-files>
   ```
   (`.clang-format` is Chromium-based — leave unrelated blocks alone.)

2. **Targeted build**:
   ```bash
   msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32
   ```
   Or `xbuild.bat` for inner-loop projects (typically `xmake.lua` modules).

3. **Run the relevant unit test**:
   ```bash
   test/Test<Concern>/Release/Test<Concern>.exe
   ```
   Constitution §II + project rule R2 mandate test-backed changes.
   Non-trivial changes need a `test/Test<Concern>/` next to or after them.

4. **NSIS smoke test** (only required when `output/install.nsi` was touched):
   see `AGENTS.md §2.5`. Not skippable, otherwise.

5. **CHANGELOG.md** under `### 主要更新` for any user-visible change
   (per P5: default key bindings, scheme behaviour, user-visible strings,
   paths).

## Commit message

```
<type>(<scope>): <subject>

<body>

<footer>
```

- **type**: `feat | fix | refactor | docs | test | chore | perf` (Conventional Commits).
- **scope** (REQUIRED — constitution P4): one of
  `WeaselTSF | WeaselServer | WeaselDeployer | WeaselSetup | WeaselUI |
   WeaselIPC | RimeWithWeasel | installer | docs | fluxing | ci`.
  Pick the *module whose ownership is most affected*, not the directory
  whose files you touched.
- **subject**: imperative, ≤ 72 chars, no trailing period.
- **body**: if non-obvious, the "why"; link spec / issue / ADR.

## Forbidden before commit

- `weasel.props`, `env.bat`, `*.user`, `*.suo`, `*.log`, `Test*.obj`,
  `output/archives/*` (already in `.gitignore`, but verify with `git status`).
- A changeset that mixes a brand-fork rebrand with an unrelated bugfix
  (P8 — rebinds go in lockstep).
- A `librime/` submodule change without an ADR + explicit user approval
  (P3, L10).
- A `RimeLeversApi::{export,import}_user_dict` call that is not wrapped in
  `StartMaintenance` / `EndMaintenance` (leveldb LOCK failures).

## Verification

Run `git status -sb` then `git diff --stat`; both should show only
intentional files (no orphans, no `.obj`, no `output/`).
