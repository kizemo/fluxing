# RELAY-ADAPTATION.md —— 本项目接力裁决

> 通用协议见 `docs/AGENT-RELAY.md`（自包含、全部仓库相对路径）。
> 本文件是**本项目专属裁决**：优先级高于通用协议，低于 `AGENTS.md` 其余家规。
> 由 relay-port 安装器生成：§3/§4/§5 的取值由安装器**首次运行即自动探测**，或在托管 agent 提问后按
> `-AnswersFile` 直接落盘，本文件**不保留待填占位**。来源标成 `defaults` 时，起链前必须与用户确认。

## 1. 项目身份

- 项目名：rime_claude
- 运行时：mcode
- 仓库根（绝对路径，反斜杠，用于 `session_create.workspace_dir`）：F:\soft\00selfmade\rime_claude
- 计划文档（仓库相对）：docs\superpowers\plans\2026-07-15-ruflo-mcp-deployment.md
- 本轮目标：BR-①g 桶化双写后续批次（2026-11-27 起）
- 计划阶段数：6
- 安装日期：2026-11-27

## 2. 阶段拆分

1. 阶段 1 双写常驻取证收口（kBoth 轮四道判据 + 侧车 + 生产桶表差分 + handoff 回写）
2. 规则 1 盲区修复（inventory_complete 死分支）+ ADR + 门禁用例
3. 写预算阈值口径裁定落盘（盘上 80000 对齐拍板值）
4. R2 对象体核对与搬运（切读硬前置一）
5. PG 快照重导重灌与一致性核对（切读硬前置二）
6. 切源 W7 四段落地（双写-校验-切读-退役）与收口交接

> 每阶段的 acceptance 判据写进计划文档（`docs\superpowers\plans\2026-07-15-ruflo-mcp-deployment.md`）；接力链每一环都把计划文档当真源读。

## 3. 写集与并发检测（source: answers）

- 允许被 agent 修改的目录 / 文件（逐条列全，写死）：
  - `".github\`
  - `".research\`
  - `"Fluxing-\`
  - `"Rime-\`
  - `"docs\`
  - `"rime-skill\`
  - `.claude\`
  - `.codebase-memory\`
  - `.learnings\`
  - `.loopx\`
  - `.research\`
  - `.specify\`
  - `FluxingConfigEditor\`
  - `FluxingPhrasesDialog\`
  - `RimeWithWeasel\`
  - `WeaselDeployer\`
  - `WeaselIPC\`
  - `WeaselIPCServer\`
  - `WeaselServer\`
  - `WeaselSetup\`
  - `WeaselTSF\`
  - `WeaselUI\`
  - `arm64x_wrapper\`
  - `cloud\`
  - `docs\`
  - `include\`
  - `lib\`
  - `lib64\`
  - `maintenance\`
  - `memory\`
  - `output\`
  - `reports\`
  - `resource\`
  - `rime-skill\`
  - `scripts\`
  - `test\`
  - `thirdparty\`
  - `tmp-race-diag\`
  - `tools\`
  - `update\`
- 禁止触碰（发布脚本、CI、签名、密钥、用户数据、门禁阈值）：
  - `.github\workflows\`
  - `**\.env*`
  - `**\*.key`
  - `**\*.pem`
  - `**\*.pfx`
  - `scripts\publish-*`
  - `scripts\release-*`
  - `scripts\deploy-*`
  - `scripts\sign-*`
  - `**\gate*.ps1`
  - `docs\relay\*.ps1`
  - `env.bat`
  - `librime\`
  - `release\`
- 并发检测口径（先跑什么命令，看到什么就停）：
  - `pwsh -NoProfile -ExecutionPolicy Bypass -File docs\relay\relay-lock.ps1 -Action status`
  - 退出码 4 或输出含 `LOCK_HELD` ⇒ 立刻停链回写 `blocked`，**不要抢锁**。

> `source=defaults` 表示本节三段是安装器的**探测值**，不是用户确认值：起链前必须按
> `docs/relay/adaptation-questions.md` 向用户逐项发问，拿到答案后用 `-AnswersFile` 重跑安装器。

## 4. 验收门禁（source: answers）

- 门禁命令：`& 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe' 'test\TestFluxingCloud\TestFluxingCloud.vcxproj' /p:Configuration=Release /p:Platform=x64 '/p:SolutionDir=F:\soft\00selfmade\rime_claude\' /m /v:m /nologo
pwsh -NoProfile -ExecutionPolicy Bypass -File 'reports\fluxing-br-1g\gate-p0-run.ps1'`
- 判绿口径：**只认产物文件里的结论字段，不认管道末端的 `$LASTEXITCODE`**。当前口径：`PASS=<n> 且 FAIL=0；UFFFD_COUNT=0；CLANGFORMAT_HUNK_DRIFT=0（只看 gate-p0-run.ps1 落盘输出，不认 $LASTEXITCODE）`
- 核数口径：`git status --porcelain` 一律带 `--untracked-files=all`。
- 门禁脚本的阈值与白名单属**人工验收点**，agent 不得自行修改。

## 5. 模型与额度策略（source: answers）

- 首选：`minimax/MiniMax-M3`
- 日间回退：`custom_provider:provider-aa5659/auto`（08:00-19:00 Asia/Shanghai）
- 夜间回退：`custom_provider:provider-aa5659/deepseek-v4.1-flash-1017`（19:01-08:00 Asia/Shanghai）
- 上面两条 `custom_provider:provider-*` 是**本机 provider 注册 id**，属 device-specific, refill 项：换机器或换
  profile 必须重填，否则回退那一环会指向不存在的 provider，白烧一轮才暴露。`tool\scripts\install-relay.ps1`
  不会替你校验它（静态审计见 `tool\references\portability-audit-2026-11-25.md`）。
- `session_create` **没有** `effort` 字段，任何情况下都不传；同样禁传 `defer_first_turn` / `parent_session_id`。
- 建环即开跑（`turn.mode=activated`）：建环后立刻 `session get` 回读 `workspace_dir` / `effective_model` / `status` 三要素再登记。
- 判活看 `session messages` 末条 timestamp，**不看** `session get.updatedAt`。

## 6. 人工验收点（命中即停链写 blocked）

- git commit / push / tag / stash / checkout
- 发布 / 部署 / 分发产物
- 删除或覆盖用户数据
- 付费、配额、账号、权限
- 改用户级 hook / provider 配置
- 改门禁脚本阈值与白名单
- 与其它会话的写集冲突

## 7. 停链条件

- 全部验收判据成立且门禁绿 → `status=done`
- 命中人工验收点 → `status=blocked`（写明缺什么）
- `current_stage >= max_stages` → `status=blocked`
- 同一验收连续两次失败 → `status=blocked`
- 连续两阶段超出工具调用预算（单环 >40 次）→ `status=blocked`

## 8. 起链与交接

- 起链只能由**用户在本项目会话内**明确指令触发；启动器：`docs\relay\start-relay.ps1`。
- 激活包：`docs\relay\ACTIVATION-PROMPT.txt`（用户一句话即可起链）。
- 每环落盘：`docs\relay\stage-<N>-<短名>.md`，编号逐字续编，不新建跨项目指针。
- 台账：`docs\relay.json`，回写只许追加，回写前必须重读盘。

## 9. 运行时与起环方式（安装器自动填，装完核对）

- 运行时：`mcode`。**换运行时必须重装**（`-Force`）或手工改本节，否则家规块与本文件会各说各话。
- 起环方式：Start the next link with the built-in `session_create` tool: pass only `agent_name` / `title` / `workspace_dir` (backslash absolute path) / `model` / `content`; never pass `effort` / `defer_first_turn` / `parent_session_id`.
- 家规落点：`AGENTS.md`
  - `AGENTS.md`：Codex CLI / OpenClaw / MCode 原生读取。
  - `CLAUDE.md`：Claude Code 只在没有 `CLAUDE.md` 时才回退读 `AGENTS.md`；装了 `CLAUDE.md` 就会遮蔽 `AGENTS.md`，所以两种运行时都要写。
- 运行时约束（人工核对）：
  - `mcode`：起环用内置 `session_create` 工具；`workspace_dir` 必须反斜杠绝对路径。
  - `claude-code`：下一环是无头 `claude -p` 进程，子进程跑完即结束，靠 `relay-spawn.ps1` 再起下一环。
  - `codex`：下一环是无头 `codex exec` 进程，退出码即成败。
  - `openclaw`：下一环用 `sessions_spawn` 工具；**OpenClaw 只注入它 workspace 根目录的 `AGENTS.md`**，仓库不是 workspace 时必须先把家规块复制过去。

---

## 6. 本项目对通用接力协议的覆盖（PROJECT OVERRIDE · 2026-11-27 用户确认）

**通用协议中「git 写操作（commit / push / tag / stash / checkout）是人类检查点」一条在本仓不适用。**

- 依据：本仓宪法 P4（Conventional Commits + 模块前缀）与本批规约 —— **每阶段收尾必须 commit + push**；
  链内环在**门禁绿**（`PASS=<n>` 且 `FAIL=0`、`UFFFD_COUNT=0`、`CLANGFORMAT_HUNK_DRIFT=0`）之后可自行 commit + push 到 `kizemo/feat/100-cloud-sync`。
- 硬判据：`git ls-remote kizemo refs/heads/feat/100-cloud-sync` 与 `git rev-parse HEAD` 逐字相等；远端直连（`-c http.proxy= -c https.proxy=`）。
- 仍属人类检查点：推送失败或硬判据不成立；force-push / tag / 删分支 / 改远端默认分支；生产 D1 / R2 / Cloudflare 数据写入；阶段 6 切读与放行范围；越出写集或红线的改动。
- 完整条款见 `docs/plan-relay.md` 的「★ 本项目覆盖」小节；**本节与 §3/§4/§5 同等权威**。