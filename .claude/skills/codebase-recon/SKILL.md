---
name: codebase-recon
description: Use when entering an unfamiliar codebase, onboarding to a new project, or assessing codebase health before reading code. Analyzes git history to reveal hotspots, risk areas, team structure, development momentum.
---

# Codebase Recon

> Analyze git history to understand a codebase before reading code. Reveals project health, risk areas, team structure.

## Phase 1: Probe

Before running analysis, determine repo scale.

```bash
# Repo vitals
cd F:/soft/00selfmade/rime_claude
echo "=== HEAD ==="
git log --oneline -1
echo "=== Branch ==="
git branch --show-current
echo "=== Total commits ==="
git rev-list --count HEAD
echo "=== First commit ==="
git log --reverse --oneline -1
echo "=== Last 30 days ==="
git log --since="30 days ago" --oneline | wc -l
echo "=== File count ==="
git ls-files | wc -l
echo "=== Top contributors (last 90d) ==="
git shortlog -sn --since="90 days ago" | head -10
```

## Phase 2: Hotspot Analysis

Files with most churn = highest risk:

```bash
# Top 20 most-modified files in last 90 days
git log --since="90 days ago" --name-only --pretty=format: | \
  grep -v "^$" | sort | uniq -c | sort -rn | head -20

# Files with most distinct authors (knowledge risk)
git log --format= --name-only | sort -u | \
  xargs -I {} sh -c 'echo "$(git log --format=%an -- {} | sort -u | wc -l) {}"' | \
  sort -rn | head -10
```

## Phase 3: Risk Areas

```bash
# Hotfix frequency (indicates unstable areas)
git log --grep="hotfix" --grep="L" --oneline | head -20

# Files with recent force-pushes (rewriting history = risk)
git reflog --date=iso | head -20

# Branches not merged in 30 days (abandoned work)
git branch --no-merged | head -20
```

## Phase 4: Team Structure

```bash
# Who's working on what
git log --since="30 days ago" --format="%an" | sort | uniq -c | sort -rn

# Per-module commit frequency
git log --since="90 days ago" --name-only --pretty=format: | \
  grep -E "^(WeaselTSF|WeaselServer|WeaselDeployer|WeaselUI|RimeWithWeasel)/" | \
  awk -F/ '{print $1}' | sort | uniq -c | sort -rn
```

## Fluxing-specific recon

```bash
# Recent L## in lessons-learned (recurring themes)
grep -E "^## L[0-9]+" .specify/memory/lessons-learned.md | tail -20

# Recent specs (active work)
ls .specify/specs/ | grep -E "^[0-9]+" | sort -V | tail -10

# Test coverage delta (new tests added)
git log --since="30 days ago" --diff-filter=A --name-only --pretty=format: | \
  grep "test/" | wc -l

# Installer size trend
ls -la release/fluxing-*.exe | tail -10
```

## Output Format

```
## Codebase Recon: F:\soft\00selfmade\rime_claude

### Vitals
- HEAD: 9856915 docs(spec): spec 054 cleanup-baseline
- Branch: Fluxing
- Commits: 12 ahead of kizemo/Fluxing
- Last 30 days: 12 commits (1 spec shipped per 2.5 days)
- Files tracked: ~2000

### Hotspots (last 90d)
- WeaselServer/QuickPanelDialog.cpp: 8 commits
- WeaselUI/FluxingComponents/: 6 commits
- WeaselServer/WeaselServerApp.cpp: 5 commits
...

### Risk Areas
- 3 hotfixes in last 7 days (L49, L50, L51)
- 1 submodule drift (librime)

### Team
- 1 active contributor (duanyi)
- 0 PRs pending
- 0 stale branches
```

## Anti-patterns

- Skip probe (assume size)
- Dump raw `git log` (no insight)
- "I'll just read the code" (miss the history)
