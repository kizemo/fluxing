# CLAUDE.md — Fluxing 进站指引

> WeaselServer / 小狼毫 的 RIME 前端 fork;**Fluxing / 火流猩输入法** 品牌分支,
> 安装路径 / 用户数据 / CLSID 均替换, 与上游 `rime/weasel` 互不冲突。
>
> 本文件**只承载**"删除后 Claude 会做错" 的导航与规则。其余一律改为链接, 不复制。

---

## 1. 进站阅读 (5 份, in order)

1. [`CONTEXT.md`](CONTEXT.md) — 身份 / 模块 / 词汇 / 不变量
2. [`AGENTS.md`](AGENTS.md) — 操作手册, 首屏 (§1) + 危险区 (§4) + checklist (§5)
3. [`.specify/memory/constitution.md`](.specify/memory/constitution.md) — 法则层, 含 R1–R9 / P1–P8
4. [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) §A 与 §11 — 部署 + NSIS 配方
5. [`docs/agents/domain.md`](docs/agents/domain.md) — 工程技能消费契约

按需再读: `docs/agents/issue-tracker.md` · `docs/adr/` · `.specify/specs/NNN-*/spec.md`。

---

## 2. 必须强制 (删除就出错)

- **必查图谱**: 探索任何源码前, 用 `codebase-memory-mcp` (`F-soft-00selfmade-rime_claude`,
  12,679 节点 / 38,137 边);顺序 `search_graph → trace_path → get_code_snippet → query_graph → get_architecture`。
  降级到 Grep / Read 的条件在 [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md)。
- **必须中文**: 用户可见字符串保持简体中文 (constitution P2/P5)。
- **必须 IPC 四边同步**: `include/WeaselIPCData.h` 与协议的任何变更 → server / TSF / deployer 三端 + 一个 ADR + 一个 `test/TestWeaselIPC/*` 测试, 一次性提交。
- **必须不开 TSF 阻塞 I/O** (P2): `ITfTextInputProcessor::*` 回调内禁止 `CreateFile` / 同步注册表 / `BeginWaitForSingleObject`。
- **必须 `RELEASE_BUILD=1` in `env.bat`** for release (L10 §6);禁用 `git describe --tags` 取版本号。
- **必须 `StartMaintenance` / `EndMaintenance` 包裹** `RimeLeversApi::{export,import}_user_dict`,否则 leveldb LOCK 失败。
- **必须 UTF-8 + `EF BB BF` + CRLF** 写 `output/install.nsi` (L09)。
- **必须 `/D=<path>` 显式** silent-install (L13 / L17), 且先清 `HKLM\Software\Fluxing\Weasel` 与 `HKCU\Software\Fluxing` (L54)。
- **必须 Conventional Commits + 模块前缀** (`WeaselTSF | WeaselServer | WeaselDeployer | WeaselSetup | WeaselUI | WeaselIPC | RimeWithWeasel | installer | docs | fluxing | ci`), 否则 commit 不合 P4。
- **必须 `git status -sb` + `git diff --stat`** 在 commit 前 — 阻止 `weasel.props` / `env.bat` / `*.obj` 误提交。

完整的 "为什么" 与事故溯源在 `lessons-learned.md`;Const. 链接 + Verification 见各专项 `.claude/rules/`。

---

## 3. 必须分流

| 决策类型 | 写到 |
|---|---|
| 架构决策 (改 PPL? / 改 IPC? / 换构建?) | `docs/adr/NNNN-<kebab>.md` (MADR 格式, `docs/adr/README.md`) |
| 事故 / bugfix (L09 / L13 / L17 / `WeaselServer.exe` 不退出) | `.specify/memory/lessons-learned.md` L##, 引用 `CONTEXT.md §5` |

不要把决策埋在 commit message / PR body — 会被 commit history 覆盖。

---

## 4. 专项规则 (前 7 项按 frontmatter `paths:` 自动加载)

| 文件 | 触发 glob |
|---|---|
| [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md) | `Weasel*` / `RimeWithWeasel` / `include` / `test` |
| [`.claude/rules/ipc-boundary.md`](.claude/rules/ipc-boundary.md) | `WeaselIPC*` / `WeaselIPCData.h` / `PipeChannel*` |
| [`.claude/rules/build-toolchain.md`](.claude/rules/build-toolchain.md) | `*.bat` / `*.sln` / `*.xmake.lua` / `*.props` / `env.bat` |
| [`.claude/rules/commit-checklist.md`](.claude/rules/commit-checklist.md) | `test/` / `.specify/specs/` / `CHANGELOG.md` |
| [`.claude/rules/hotkey-binding.md`](.claude/rules/hotkey-binding.md) | `Hotkey*` / `Shift*` / `Test*Hotkey*` / `Test*Binding*` |

---

## 5. 自我约束

> CLAUDE.md 不重复 AGENTS.md / CONTEXT.md / constitution.md;被新文件覆盖过的章节直接删, 不要演化成拷贝。
