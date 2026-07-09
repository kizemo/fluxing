# CLAUDE.md — 进站指引 (Claude 专用)

> **一句话定位**:WeaselServer / 小狼毫 = RIME 输入法引擎的 Windows 原生前端;本仓库是
> **Fluxing / 火流猩输入法** 品牌 fork,品牌、安装路径、用户数据路径、CLSID 皆已替换,
> 与上游 `rime/weasel` 互不冲突。
>
> **文档分层** (硬规则以 constitution 为准, 操作手册见 AGENTS.md, 词汇表见 CONTEXT.md);
> 本文件**只做导航**, 不重复任何更权威文档的内容。

---

## 1. 进站阅读顺序 (5 步, 5 分钟)

依序读以下 5 份,**合计约 1,800 行 / ~70 KB**。前 30 % 涵盖核心信息;但
**§4 不变量** 与 **§Hard Rules** 必须完整读完。

1. [`CONTEXT.md`](CONTEXT.md) §1 身份 / §2 模块地图 / §3 词汇表 / §4 不变量
2. [`AGENTS.md`](AGENTS.md) §1 项目地图 + §4 危险区 + §5 提交前 checklist + §7 anti-patterns
3. [`.specify/memory/constitution.md`](.specify/memory/constitution.md) §Core Principles + §Hard Rules (R1–R9) + P2 / P8
4. [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) §A 与 §11 — 部署路径与 NSIS silent-install 配方
5. [`docs/agents/domain.md`](docs/agents/domain.md) — 工程技能的消费者契约

按需再读:`docs/agents/issue-tracker.md` (新建 issue) / `docs/adr/` (架构决策) /
`.specify/specs/NNN-*/spec.md` (实现特定功能)。

---

## 2. 硬规则 (verifiable)

### 2.1 宪法硬规则 (R1–R9, 来自 constitution.md)

| # | 规则 | 验证 |
|---|---|---|
| R1 | 改代码前先在聊天里表明意图 + 验收标准 | 首条消息含 "What / Why / How verified" |
| R2 | `spec.md` 不出现技术栈词汇 | `grep -E "WTL\|Boost\|C++\|MFC" spec.md` 应返回 0 行 |
| R3 | 每个 task 单一优先级 P1–P3 + user story 标签 | 在 `tasks.md` 中可见 |
| R4 | `plan.md` 填 "Constitution Check" 五原则 | `grep "## Constitution Check" plan.md` 应命中 |
| R5 | 单一 task ≤ 4 小时 / 1–3 文件 / 输入输出明确 | 拒绝模糊 task |
| R6 | "完成" 必须粘贴测试输出或文档化手动验证 | 由 `verification-before-completion` 守门 |
| R7 | 一概念一真相;spec / plan / tasks 冲突 → 停 + 跑 `spec-check` | 跑 `spec-check` |
| R8 | spec 进 git 版本化, 不在聊天里 | `git log .specify/specs/` 有 dated entries |
| R9 | 不熟模块先读 `include/*.h` + librime/README, 不要凭记忆推 | `source-driven-development` 触发 |

### 2.2 Fluxing 分支业务红线 (不与 R1–R9 重号, 强制)

| # | 业务红线 | 验证 |
|---|---|---|
| BR1 | 所有面向用户字符串保持简体中文 | 新增字符串 grep 无英文错误提示 (`git diff` 检查) |
| BR2 | 非 trivial 改动必须配对 `test/Test<Concern>/` 测试 | PR diff 中 `test/` 必须有对应新增 / 修改 |
| BR3 | 大改动必须有 `.specify/specs/NNN-*/` 三件套 | 合入前 `ls .specify/specs/NNN-*/{spec,plan,tasks}.md` 都存在 |
| BR4 | Conventional Commits + **模块前缀必填** (`WeaselTSF / WeaselServer / ... / fluxing`) | `git log --format=%s` 应符合 |
| BR5 | 仅 `Fluxing` 分支可改产品名 / CLSID / 安装路径 / 注册表 / 二进制名 (constitution P8) | 改动只在 `Fluxing` 分支可见, master 不允许 |

详细条文与例外,见 [`constitution.md` §Hard Rules](.specify/memory/constitution.md#hard-rules-9--verifiable-must)
与 §P1–P8。

---

## 3. 工作纪律

### 3.1 探索代码前先查 codebase-memory 图谱

项目索引 ID: **`F-soft-00selfmade-rime_claude`** (12,679 节点 / 38,137 边)。

**强制顺序**: `search_graph` → `trace_path` → `get_code_snippet` → `query_graph` → `get_architecture`。

**降级到 Grep / Read 的条件** (任一满足):

1. `search_graph` 返回 0 命中且符号理应在 scope 内
2. 需要 `git blame` / 提交历史 (图谱无时间轴)
3. 需要二进制或非代码文件 (图、NSIS、exe)
4. MCP 不可用 / 超时 / 工具返回 `expected_nodes=0`
5. 验证会话中途的临时改动 (此时先 `index_repository` 再继续)

详见 [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md)。

### 3.2 决策 → ADR; 事故 → L##

- **架构决策**: 在 [`docs/adr/`](docs/adr/) 起 `NNNN-<kebab>.md` (MADR: Status / Context / Decision / Consequences),遵循 [`docs/adr/README.md`](docs/adr/README.md) 规约
- **事故复盘**: 在 [`.specify/memory/lessons-learned.md`](.specify/memory/lessons-learned.md) 续写 `L##`,在 `CONTEXT.md §5` 引用

### 3.3 跨进程边界不允许创新

IPC 协议 (`key=value` + `.\n`) 改动意味着**发送端 + 接收端 + 消息目录 + ADR + 测试** 5 件同步;
详见 [`.claude/rules/ipc-boundary.md`](.claude/rules/ipc-boundary.md)。

TSF 线程**绝对不阻塞 I/O** (P2);这条已经被 L10 / L17 / L18 反复打破过。

---

## 4. 高频踩坑 (不要做的事)

| 编号 | 描述 | 后果 |
|---|---|---|
| **L09** | 不带 BOM 处理 `output/install.nsi` (必须 UTF-8 + EF BB BF + CRLF) | 安装器中文乱码 |
| **L13 / L17** | silent install 时不带 `/D=<path>` | 未知参数污染 `$INSTDIR` 与注册表 |
| **L10 §4** | 改 `librime/plugins/lua/` 而不是 `thirdparty/librime-lua/` | 下次 `build.bat rime` 被覆盖 |
| **L10 §6** | 用 `git describe --tags` 取 release 版本号 | release 标签不可靠, 必须设 `RELEASE_BUILD=1` |
| **A11 / L14** | 安装 x64 Weasel*.exe 到 x64 Windows (librime Win32-only) | 0xC000007B 启动崩溃 |
| **A4 / L10 §2** | 信 `cmake install(TARGETS rime)` 自动复制 .lib | 必须 manual copy in `build.bat` |
| **userdb LOCK** | `export_user_dict` / `import_user_dict` 不裹 `StartMaintenance` / `EndMaintenance` | leveldb LOCK 失败 |

完整列表见 [`AGENTS.md` §7 anti-patterns](AGENTS.md#7-anti-patterns-seen-in-this-project-do-not-repeat)
与 `.specify/memory/lessons-learned.md`。

---

## 附录 A — 专项规则 (按需加载)

| 文件 | 何时读 |
|---|---|
| [`.claude/rules/codebase-graph.md`](.claude/rules/codebase-graph.md) | 任何代码探索场景 (路径已 frontmatter 触发) |
| [`.claude/rules/ipc-boundary.md`](.claude/rules/ipc-boundary.md) | 改 `WeaselIPC*` / `WeaselIPCData.h` 时 |
| [`.claude/rules/build-toolchain.md`](.claude/rules/build-toolchain.md) | 改 `*.bat` / `*.sln` / `xmake.lua` 时 |
| [`.claude/rules/commit-checklist.md`](.claude/rules/commit-checklist.md) | 准备 commit / 改 spec / CHANGELOG 时 |
| [`.claude/rules/hotkey-binding.md`](.claude/rules/hotkey-binding.md) | 改 Hotkey / Shift / Key binding / 相关 test 时 |

## 附录 B — 索引与项目元数据

- **持久化图谱**: `.codebase-memory/graph.db.zst` (10.3 MB) 已 commit;
  重组跑用 `index_repository(repo_path=…, mode="moderate", persistence=true)`
- **持久化 ADR** (MCP): 13-section 在 `F-soft-00selfmade-rime_claude`
  项目的 codebase-memory-mcp 内 (`manage_adr` 读写);人类读版本见
  [`docs/adr/`](docs/adr/) (目前空)
- **职责金字塔**:
  1. 法则层: `.specify/memory/constitution.md`
  2. 词典层: `CONTEXT.md`
  3. 手册层: `AGENTS.md`
  4. 事故层: `.specify/memory/lessons-learned.md`
  5. 活知识层: `.specify/memory/project-knowledge.md`
  6. 决策层: `docs/adr/`
  7. 上游参考层: `docs/Fluxing-code-map/`
  8. 工程技能消费契约: `docs/agents/`

## 附录 C — 一句话自检

> CLAUDE.md **不重复** AGENTS.md / CONTEXT.md / constitution.md 写过的东西;
> 任何"被新文件覆盖的章节"直接删除,不要为本文件演化成拷贝。
