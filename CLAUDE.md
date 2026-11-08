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

按需再读:
- `docs/agents/issue-tracker.md` · `docs/adr/` · `.specify/specs/NNN-*/spec.md`
- [`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md) — 改 `output/data/**/*.yaml` (schema/custom/key_binder/editor/ascii_composer/switcher/punctuator/recognizer/translator/app_options) 时必读;重点章节 §5–§7、§9(本机现状)、§10(常见坑)

---

## 2. 必须强制 (删除就出错)

- **必查图谱**: 探索任何源码前, 用 `codebase-memory-mcp` (`F-soft-00selfmade-rime-claude`,
  12,715 节点 / 38,206 边);顺序 `search_graph → trace_path → get_code_snippet → query_graph → get_architecture`。
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
- **必须先读 [`Rime-设置说明书-重点快捷键.md`](Rime-设置说明书-重点快捷键.md)** 当修改 `output/data/**/*.yaml` / `output/data/**/*.custom.yaml` 的 `key_binder/bindings` / `editor/bindings` / `ascii_composer/switch_key` / `switches` / `recognizer/patterns` / `punctuator/*` / `translator/*` / `app_options` / `switcher/hotkeys`;遵循 §3 补靪语法、§6/§7 action 完整列表、§9.3 WeaselTSF `RELEASE_MASK` 坑。

- **必须阶段收尾双产物**: 完成 `.specify/specs/NNN-*/tasks.md` 任一阶段任务时, 必须同步更新 `docs/archive/handoffs/handoff-YYYY-MM-DD-<slug>.md` 并就地改写 `docs/archive/prompts/prompt-next-session-YYYY-MM-DD-<slug>.md` (接力 prompt 只给全路径指针, 不复述 handoff 内容); 规则全文见 [`AGENTS.md` §9](AGENTS.md#9-handoff--relay-prompt-mandatory)。

- **本批交接指针**: [批次 N · spec 045 T107 正文级真合并（勘察定稿 + 实施设计）](docs/archive/handoffs/handoff-2026-11-08-batch-n-t107-merge-design.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **上一批交接指针**: [批次 M · spec 045 快捷键设置 P1 收口（T101/T102/T103/T106）](docs/archive/handoffs/handoff-2026-11-08-batch-m-shortcut-settings-p1.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **更早批次交接指针（L）**: [批次 L · 快捷设置右键菜单「快捷键设置」+ 托盘「中英文」摘除上机 + 快捷键设置界面 P0 修复](docs/archive/handoffs/handoff-2026-11-08-batch-l-shortcut-settings-menu.md) · 接力 prompt: 见该 handoff 文档；按用户 2026-11-08 指示，接力 prompt 直接给在当轮回答正文、不另落文件
- **更早批次交接指针（K）**: [批次 K · 快捷设置栏图标 v13 英文逗号左缘裁切](docs/archive/handoffs/handoff-2026-11-08-batch-k-v13-punct-comma-cut.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-k-v13-punct-comma-cut.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-k-v13-punct-comma-cut.md)
- **更早批次交接指针（J）**: [批次 J · 快捷设置栏图标 v12 英文逗号 + 语言栏退出常驻托盘](docs/archive/handoffs/handoff-2026-11-08-batch-j-v12-punct-comma-tray-icon.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-j-v12-punct-comma-tray-icon.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-j-v12-punct-comma-tray-icon.md)
- **更早批次交接指针（I）**: [批次 I · 图标 v11 + 标点跟随状态 + 会话门禁 + 提交边界标点](docs/archive/handoffs/handoff-2026-11-08-batch-i-v11-punct-session-gate.md) · 接力 prompt: [prompt-next-session-2026-11-08-batch-i-v11-punct-session-gate.md](docs/archive/prompts/prompt-next-session-2026-11-08-batch-i-v11-punct-session-gate.md)
- **更早批次交接指针（H2）**: [批次 H2 · 快捷面板图标 v10 字符逗号 + 全角=中文标点 + 点击不失焦](docs/archive/handoffs/handoff-2026-11-08-quickpanel-v10-punct-semantics.md) · 接力 prompt: [prompt-next-session-2026-11-08-quickpanel-v10-punct-semantics.md](docs/archive/prompts/prompt-next-session-2026-11-08-quickpanel-v10-punct-semantics.md)
- **更早批次交接指针（H）**: [批次 H · 快捷面板全半角图标 v9 + 半角=英文标点 + 中英联动标点](docs/archive/handoffs/handoff-2026-11-07-quickpanel-v9-fwicon-batch-h.md) · 接力 prompt: [prompt-next-session-2026-11-07-quickpanel-v9-fwicon-batch-h.md](docs/archive/prompts/prompt-next-session-2026-11-07-quickpanel-v9-fwicon-batch-h.md)
- **并行线交接指针（spec 100 · 云端账号与多端同步）**: [云端账号与多端同步 · 规划落地（spec 100）](docs/archive/handoffs/handoff-2026-11-07-fluxing-cloud-sync-plan.md) · 接力 prompt: [prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md](docs/archive/prompts/prompt-next-session-2026-11-07-fluxing-cloud-sync-plan.md)
- **更早批次交接指针（G）**: [批次 G · 全半角按钮图标 v7 对角矢量布局](docs/archive/handoffs/handoff-2026-11-07-quickpanel-v7-fwicon-diagonal.md) · 接力 prompt: [prompt-next-session-2026-11-07-quickpanel-v7-fwicon-diagonal.md](docs/archive/prompts/prompt-next-session-2026-11-07-quickpanel-v7-fwicon-diagonal.md)
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
| [`.claude/rules/rime-settings.md`](.claude/rules/rime-settings.md) | `output/data/**/*.yaml` / `output/data/**/*.custom.yaml` / `weasel.yaml` |

---

## 5. 自我约束

> CLAUDE.md 不重复 AGENTS.md / CONTEXT.md / constitution.md;被新文件覆盖过的章节直接删, 不要演化成拷贝。
