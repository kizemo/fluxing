---
paths:
  - "test/**/*.cpp"
  - ".specify/specs/**/*.md"
  - "CHANGELOG.md"
---

# Commit checklist (Fluxing branch)

Workflow 在 `AGENTS.md §5` 与 `.specify/memory/constitution.md §Development Workflow`。
本文件只覆盖**本分支强约束**。

## Pre-commit (5 步, in order)

1. `clang-format -i <changed-files>` (`.clang-format` Chromium-based, 只动你改的)。
2. `msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32`,
   或模块级 `xbuild.bat`。
3. `test/Test<Concern>/Release/Test<Concern>.exe` (R2 / constitution §II)。
4. `output/install.nsi` 改动 → `AGENTS.md §2.5` smoke test。
5. `CHANGELOG.md` `### 主要更新` for user-visible 改动 (P5)。

## Message

```
<type>(<scope>): <subject>
```

- **type** = `feat | fix | refactor | docs | test | chore | perf`
- **scope** (REQUIRED, P4) = `WeaselTSF | WeaselServer | WeaselDeployer |
  WeaselSetup | WeaselUI | WeaselIPC | RimeWithWeasel | installer | docs |
  fluxing | ci`
- **subject** = 祈使句, ≤72 字符, 不加句号。

## Forbidden (clone 或 commit 前必检)

- `weasel.props` / `env.bat` / `*.user` / `*.suo` / `*.log` / `Test*.obj` /
  `output/archives/*` (已在 `.gitignore`, 但 `git status -sb` 验证)。
- 品牌 rebrand 与无关 bugfix 混在一个 changeset (P8 锁定绑)。
- `librime/` 子模块改动无 ADR + 用户授权 (P3 / L10)。
- `RimeLeversApi::{export,import}_user_dict` 不裹 `StartMaintenance` /
  `EndMaintenance` (leveldb LOCK 失败)。
