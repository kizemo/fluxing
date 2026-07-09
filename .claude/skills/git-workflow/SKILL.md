---
name: git-workflow
description: Git workflow and conventional commits for Fluxing. Use for any commit, branch, push, or conflict. P4 + AGENTS.md §3 enforcement.
---

# Git Workflow and Versioning

> Git is your safety net. Treat commits as save points, branches as sandboxes, history as documentation.

## Core principles

### Trunk-based with branded fork

- `master` tracks upstream `rime/weasel` — DO NOT push directly
- `Fluxing` is the brand-fork branch (P8 scope) — push to `kizemo/Fluxing`
- Pre-state snapshots = branches (e.g. `Fluxing-snapshot-2026-06-30`)
- Releases = tags (e.g. `v0.18.33.0`)

## Conventional Commits (P4)

```
type(scope): subject

[body]

[footer with L## / spec / issue refs]
```

### Allowed types

| Type | Use |
|---|---|
| feat | New feature |
| fix | Bug fix |
| docs | Documentation only |
| style | Format / whitespace |
| refactor | Code change, no behavior change |
| test | Add/fix tests |
| chore | Tooling / build / config |
| perf | Performance |
| release | Version bump / ship commit |

### Allowed scopes

`WeaselTSF / WeaselServer / WeaselDeployer / WeaselSetup / WeaselUI / WeaselIPC / RimeWithWeasel / librime / installer / docs / ci / fluxing`

(`fluxing` is brand-fork only, per P8.)

### Examples

- `fix(WeaselTSF): handle empty RimeUserDir`
- `fix(installer): spec 053 L58 iron rule - default install to D:\Program Files\fluxing`
- `docs(memory): lessons-learned L10 - librime-lua integration`
- `chore(ci): spec 053 cleanup - add /TestOrphanRecovery.obj to .gitignore`

## Per-day workflow

```bash
# 1. Pull latest
cd F:/soft/00selfmade/rime_claude
git fetch kizemo
git rebase kizemo/Fluxing    # or merge if prefer

# 2. Work on a small slice
# ... edit, test, verify ...

# 3. Stage SPECIFIC files (never `git add .`)
git add WeaselServer/Foo.cpp WeaselServer/Foo.h

# 4. Commit with conventional message
git commit -m "fix(WeaselServer): ..."

# 5. Push
git push kizemo Fluxing

# 6. Verify
git status -sb
# Should show: ## Fluxing...kizemo/Fluxing
```

## Pre-commit checklist (AGENTS.md §5)

1. **Byte health** (L01/L02/L45): no 0xC0/0xC1, BOM only on .nsi/.rc
2. **Tests**: `cmd /c "scripts\test-infra\run-test-suite.bat"` → all PASS
3. **Build hygiene**: no new *.log, no new github_token.txt, env.bat/weasel.props untracked
4. **Format**: `clang-format.ps1 -i` (Windows) or `./clang-format.sh -i` (bash)
5. **Scope check**: commit message uses ONE scope from the allowed set

## Branches vs Tags (AGENTS.md §3.5)

| Use case | Type | Example |
|---|---|---|
| Long-lived line of work | Branch | `Fluxing`, `master` |
| Pre-state snapshot (rollback) | Branch + annotated tag | `Fluxing-snapshot-2026-06-30` |
| Version release | Tag (lightweight) | `v0.18.33.0` |
| WIP snapshot | Branch (not tag) | `Fluxing-pre-foo` |

**Never** push to `origin` (upstream `rime/weasel`).

## Emergency rollback

```bash
git reflog  # find the safe commit hash
git reset --hard <hash>
# or
git checkout -b Fluxing-rollback <safe-commit>
```

## Anti-patterns (A10-A13)

- **A10**: `git add .` from repo root (sweeps up env.bat, weasel.props, *.log)
- **A11**: branch for every release (pollutes `git branch -a`)
- **A12**: tag for WIP snapshot (tags should be immutable)
- **A13**: delete snapshot ref "to clean up" (snapshot refs are cheap)
