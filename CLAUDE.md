# CLAUDE.md — 进站指引(Claude 专用)

> 给在本仓库工作的 Claude 代理读的第一份文件。它**只做导航**,不重复任何已被更权威文档覆盖的内容。
>
> 整个项目存在明确的"职责金字塔",任意文件都只能被其中一层覆盖,内容不可重复:
>
> | 层 | 文件 | 角色 | 谁负责维护 |
> |---|---|---|---|
> | 法则层 | [`.specify/memory/constitution.md`](.specify/memory/constitution.md) | 5 原则 + 9 硬规则 + 8 项目规则 — **binding** | 治理层 |
> | 词典层 | [`CONTEXT.md`](CONTEXT.md) | 词汇表 / 不变量 / 阅读顺序 | 工程技能 |
> | 手册层 | [`AGENTS.md`](AGENTS.md) | 构建 / 测试 / 提交流程 / anti-pattern | 工程技能 |
> | 事故层 | [`.specify/memory/lessons-learned.md`](.specify/memory/lessons-learned.md) | L01–L## 真实事故 post-mortem | 工程技能 |
> | 活知识层 | [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) | 部署路径 / 注册表 / NSIS 陷阱(随时间变化) | 工程技能 |
> | 决策层 | [`docs/adr/`](docs/adr/) | 后置式架构决策记录 | 决策者 |
> | 上游参考层 | [`docs/Fluxing-code-map/`](docs/Fluxing-code-map/00-index.md) | 只读的上游 weasel 代码图谱(快照) | 不维护 |
>
> CLAUDE.md 写在本文件,你应该在每一次进入项目后**先读 §1 的 5 步阅读顺序**,再开始任何工作。

---

## 1. 进站阅读顺序(必读,5 步)

按顺序依次读下面 5 份,**5 分钟内完成**,然后才开始工作:

1. [`CONTEXT.md`](CONTEXT.md) — 项目身份、模块地图、词汇表、不变量(invariants 在 §4)。
2. [`AGENTS.md`](AGENTS.md) — 操作手册的第一屏(§1 项目地图)+ §4 危险区 + §7 anti-patterns + §5 提交前 checklist。
3. [`.specify/memory/constitution.md`](.specify/memory/constitution.md) — 完整法则。重点: §II 测试守则、§IV 澄清规则、§P2 TSF 线程、§P8 Fluxing 品牌 fork 范围。
4. [`.specify/memory/project-knowledge.md`](.specify/memory/project-knowledge.md) §A 与 §11 — 部署路径、注册表项、NSIS silent-install 配方(可执行命令)。
5. [`docs/agents/domain.md`](docs/agents/domain.md) — 工程技能的"消费者契约",包括哪些文件必须在每次运行前读。

> 接下来再按需读:`docs/agents/issue-tracker.md`(如果你要新建 issue)、`docs/adr/`(架构相关决策)、`.specify/specs/NNN-*/spec.md`(实现特定功能时)。

---

## 2. 5 条硬规则(摘录,完整在 constitution)

这些是**违反了就必须停下**的硬约束。原条文与上下文见
[constitution.md §Hard Rules](.specify/memory/constitution.md#hard-rules-9--verifiable-must)。

- **R1 中文 UX**: 所有面向用户字符串保持简体中文,错误信息也走中文。引用 P2 / P5。
- **R2 测试守则**: 任何非 trivial 改动都必须配对一个 `test/Test<Concern>/` 测试。这是 §II 硬守则,non-negotiable。
- **R3 Spec-工件纪律**: 大改动必须有 `.specify/specs/NNN-*/` 三件套(spec / plan / tasks),否则只是探索性代码而不应合入主线。
- **R4 提交规范**: Conventional Commits,**模块前缀必填** — 用 `WeaselTSF / WeaselServer / WeaselDeployer / WeaselSetup / WeaselUI / WeaselIPC / RimeWithWeasel / installer / docs / fluxing` 之一。
- **R5 品牌分支边界(P8)**: 仅 `Fluxing` 分支可改产品名 / CLSID / 安装路径 / 注册表 / 二进制名;**禁止**改 `librime/`、`Boost`、`OpenCC`、`.clang-format`,以及 IPC 协议语义。

详细版本与例外条款走原文件,不要让本节"擅自覆盖"。

---

## 3. 进站后**立即**遵守的几条指令(CLAUDE.md 原创)

这些是**CLAUDE.md 才写、其他文档不写**的"Claude 工作纪律"。

### 3.1 探索代码前先查 codebase-memory 图谱

本项目已用 `codebase-memory-mcp` 建立知识图谱 (`F-soft-00selfmade-rime_claude`,12,679 节点 / 38,137 边)。

**强制使用顺序**(不要先 Grep/Read 文件):

1. `search_graph(name_pattern=...)` 或 `search_graph(query=...)` 找符号
2. `trace_path(function_name=..., mode=calls|data_flow)` 看调用链
3. `get_code_snippet(qualified_name=...)` 看精确源码
4. `query_graph(...)` 做复杂 Cypher 模式
5. `get_architecture(aspects=[...])` 看高层概览

只有"图谱里没有"或"需要看 git blame/二进制"`才`用 Grep / Read 兜底。索引状态可在
`Bash` 中通过 graph 工具直接确认。

### 3.2 决策记录写到 ADR,事故写到 lessons

- **架构决策**(`WeaselServer 改 PPL?`、`IPC 加新消息类型?`、`换构建系统?`)→ 在
  [`docs/adr/`](docs/adr/) 起 NNNN-<kebab>.md,遵循 [`docs/adr/README.md`](docs/adr/README.md) 规约。
- **事故复盘**(`get-rime.ps1 跑挂`、`silent install /D= 被吞`、`WeaselServer.exe 旧实例不退出`)→
  在 [`.specify/memory/lessons-learned.md`](.specify/memory/lessons-learned.md) 续写 L## 条目,并在
  `CONTEXT.md` §5 引用之。

不要把决策散在 commit message 或 PR body 里;它们会被 commit history 覆盖。

### 3.3 跨进程边界不允许创新

IPC 文本协议(`key=value` + `.\n` 终止,见 `CONTEXT.md` §3.1)的语义改动意味着**发送端 + 接收端 + 消息目录**必须同步更新,否则会出现"部署失败但无错误日志"。在 [`include/WeaselIPCData.h`](include/WeaselIPCData.h) 加新消息类型前,**必须先开 ADR**,再同步 `WeaselIPC/`、`WeaselTSF/`、`WeaselServer/`、`WeaselDeployer/` 四处。

TSF 线程绝对不阻塞 I/O(P2)。这条已经被 L10 / L17 / L18 反复打破过。

### 3.4 工具与构建身份

- 包管理器 / 构建系统:**xmake**(通过 `xbuild.bat`) + **MSBuild**(`build.bat all`) 双栈。
- 释放构建必须 `RELEASE_BUILD=1` in `env.bat`(L10 §6)。
- 不要把 `weasel.props` / `env.bat` / `msbuild*.log` 提交(`.gitignore` 已加)。
- PowerShell 处理中文文本时必须 byte-level(`[IO.File]::ReadAllBytes`)或
  `[Console]::OutputEncoding = UTF8` + `Out-File -Encoding utf8`,详见 L01 / L02 / A1。
- `gh issue create` 中文 body 会乱码(L08):用 `--body-file` 或
  `gh api --method POST repos/kizemo/fluxing/issues -f body=@file`。

### 3.5 提交前必跑清单(5 步,in order)

完整版见 [`AGENTS.md` §5](AGENTS.md)。最低限度:

1. `clang-format -i` 你改动的文件(L10 `git clang-format` 等价物)。
2. 跑相关单元测试(`msbuild weasel.sln /t:Build /p:Configuration=Release /p:Platform=Win32`,
   然后 `test/Test<YourConcern>/Release/Test<YourConcern>.exe`)。
3. 任何 `output/install.nsi` 改动**必须**跑
   `end-to-end silent-install smoke test`(`AGENTS.md` §2.5),不能跳过。
4. 更新 [`CHANGELOG.md`](CHANGELOG.md) 对应 release 区段(用户可见改动)。
5. Conventional Commits + 模块前缀(P4)。

---

## 4. 一句话定位

> **WeaselServer / 小狼毫** = RIME 输入法引擎的 Windows 原生前端;本仓库是
> **Fluxing / 火流猩输入法** 品牌 fork,品牌、安装路径、用户数据路径、CLSID 皆已
> 替换为 `Fluxing` / `火流猩输入法`,与上游 rime/weasel **互不冲突**。

补充信息看 [`README.md`](README.md)(用户向快速上手) 与 [`INSTALL.md`](INSTALL.md)(构建工具链)。

---

## 5. 索引与项目元数据

- 知识图谱:`F-soft-00selfmade-rime_claude`(`codebase-memory-mcp`,12,679 节点 /
  38,137 边,持久化到 `.codebase-memory/graph.db.zst`)。
- 持久化 ADR(`codebase-memory-mcp manage_adr`):12 个 section 描述"Weasel/Fluxing
  架构决策",由本项目 ADR 索引 — 跨进程边界、layering、Macro 噪音处理都已落盘。
- AGENTS.md / CONTEXT.md / constitution.md 是**人类可读**的;持久化 ADR 是
  **机器可读**的;两者并存不冗余。

---

## 6. 不要做的事(摘自 constitution / AGENTS.md / lessons)

> 完整版见 [`AGENTS.md` §7 anti-patterns](AGENTS.md#7-anti-patterns-seen-in-this-project-do-not-repeat)
> 与 [`.specify/memory/lessons-learned.md`](.specify/memory/lessons-learned.md)。
> 这里只列 CLAUDE.md 应当直接提示的几条:

- **不要**用普通 string API 改 `output/install.nsi` — 它必须是 UTF-8 + BOM + CRLF(L09)。
- **不要**在 silent install 时不带 `/D=<path>` — 未知 CLI 参数会被拼进 `$INSTDIR` 污染注册表(L13 / L17)。
- **不要**改 `librime/` 子模块内的任何源文件(P3 / L10)— Lua 插件改
  `thirdparty/librime-lua/`,会在 `build.bat rime` 时被自动覆盖。
- **不要**对 `RimeLeversApi::export_user_dict / import_user_dict` 调用省掉
  `StartMaintenance` / `EndMaintenance` — `leveldb` LOCK 会失败。
- **不要**用 `git describe --tags` 取 release 版本号 — 在 release 中改设 `RELEASE_BUILD=1`(L10 §6)。

---

## 7. CLAUDE.md 与持久化 ADR 的关系

`codebase-memory-mcp manage_adr` 写入的 12-section 持久化 ADR 是**机器读**版本,
[`docs/adr/`](docs/adr/) 是**人类读**版本。两者不重复 — 持久化 ADR 用
classes/leiden clusters/edge-types 描述架构,人类版用 prose。两边都更新时,以
人类版为准(它可以被 PR review)。

---

*这份文件本身**不重复** AGENTS.md / CONTEXT.md / constitution.md 写过的东西。
如发现重复,合并到对应原始文档并把本文件改成纯链接。任何"被新文件覆盖的章节"
请直接删除,不要为本文件演化成一份拷贝。*
