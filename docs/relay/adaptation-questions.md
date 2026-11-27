# relay-port 首次安装：三项裁决待确认

安装器已经替你探测过一遍，下面每项都带一个 `default` 值。

**`default` 不是用户确认值**：它是安装器按仓库现状猜出来的。把这三项（写集 / 验收门禁 / 模型策略）当成默认值直接起链，是 relay-port 最常见的翻车方式——链会改到你不想让它改的目录，或者把「命令退出码 0」当成门禁绿。

确认流程：

1. 逐项向用户提问（`ask_user`），一次 2-3 项，不要一次抛 8 项。
2. 把确认结果写成一个 JSON 文件，键名照抄下面的 `answer_key`。
3. 用 `-AnswersFile` 重跑安装器（幂等，只定点替换 §3/§4/§5，不动其它章节）。

```
pwsh -NoProfile -ExecutionPolicy Bypass -File <repo>\docs\relay\install-relay.ps1 -RepoRoot <repo> -AnswersFile <repo>\docs\relay\adaptation-answers.json
```

用户明确说「就按探测值」时，才算 `source=defaults` 可接受，并把这句确认写进台账。

## 1. 写集与并发检测（§3）

> 先问写集。允许集要具体到目录，禁止集按项目补充（发布/CI/密钥/门禁阈值是硬禁止）。

### 允许被 agent 修改的目录 / 文件

- 答案键: `write_set_allow`
- 类型: `list`
- 探测默认值（是安装器的猜测，不是用户确认值）:
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
- 要问用户: 哪些目录或文件允许被接力链里的 agent 修改？请逐条列全，不要写「看情况」。
- 判定依据: 探测值来自 `git ls-files` 的顶层目录（已剔除 node_modules / dist / build / .venv 等依赖与产物目录）。探测为空时手填，例如 `src\`、`tests\`、`docs\`。

### 禁止触碰的路径

- 答案键: `write_set_deny`
- 类型: `list`
- 探测默认值（是安装器的猜测，不是用户确认值）:
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
- 要问用户: 哪些路径绝对禁止触碰？（发布/部署/签名脚本、CI、密钥、用户数据、门禁阈值）
- 判定依据: 默认清单已含 `.github/workflows/`、`.env*`、`*.key/*.pem/*.pfx`、`scripts/publish-*|release-*|deploy-*|sign-*`、`**/gate*.ps1`、`docs/relay/*.ps1`。按项目补充，别删默认项。

### 并发检测命令

- 答案键: `concurrency_check`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `pwsh -NoProfile -ExecutionPolicy Bypass -File docs\relay\relay-lock.ps1 -Action status`
- 要问用户: 起环前先跑哪条命令来判断有没有别的会话正在跑？看到什么就停？
- 判定依据: 默认是 relay 自己的锁状态查询：`pwsh -NoProfile -ExecutionPolicy Bypass -File docs\relay\relay-lock.ps1 -Action status`。退出 4 或输出 `LOCK_HELD` 即停链回写 blocked，不要抢锁。

## 2. 验收门禁（§4）

> 问清「跑哪条命令」和「看哪个字段判绿」。判绿口径必须落在产物文件里，不能是 $LASTEXITCODE。

### 门禁命令

- 答案键: `gate_command`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `reports\fluxing-br-1g\gate-p0-run.ps1`
- 要问用户: 验收门禁跑哪条命令？请给可直接复制执行的完整命令。
- 判定依据: 探测口径：仓库内最浅的 `*gate*.ps1`，没有就退回 `scripts\selftest.ps1`，两者都没有则填 `none` 并手填真命令。

### 判绿口径（产物字段）

- 答案键: `gate_verdict_field`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `none`
- 要问用户: 判绿看产物文件里的哪个结论字段？例如 `GATE_VERDICT`、`FAIL=0`、`SUMMARY 里的 verdict`。
- 判定依据: **只认产物字段，不认管道末端的 `$LASTEXITCODE`**。默认值写的是这条口径本身；你的项目有具体字段名时请替换成真字段名 + 期望值。

## 3. 模型与额度策略（§5）

> 首选模型必问。回退模型含本机 provider 注册 id（`custom_provider:provider-*`），换机器必须重填。

### 首选模型

- 答案键: `model_primary`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `minimax/MiniMax-M3`
- 要问用户: 接力链首选哪个模型？（给 `<provider>/<model>` 全名）
- 判定依据: 默认 `minimax/MiniMax-M3`。`session_create` 没有 `effort` 字段，任何情况下都不传。

### 日间回退模型（08:00-19:00 Asia/Shanghai）

- 答案键: `model_fallback_day`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `custom_provider:provider-aa5659/auto`
- 要问用户: 首选模型额度用尽时，日间回退到哪个模型？
- 判定依据: 默认 `custom_provider:provider-aa5659/auto`。`custom_provider:provider-*` 是本机 provider 注册 id，属 device-specific, refill 项：换机器或换 profile 必须重填，否则回退那一环会指向不存在的 provider，白烧一轮才暴露。

### 夜间回退模型（19:01-08:00 Asia/Shanghai）

- 答案键: `model_fallback_night`
- 类型: `text`
- 探测默认值（是安装器的猜测，不是用户确认值）:
  - `custom_provider:provider-aa5659/deepseek-v4.1-flash-1017`
- 要问用户: 首选模型额度用尽时，夜间回退到哪个模型？
- 判定依据: 默认 `custom_provider:provider-aa5659/deepseek-v4.1-flash-1017`。同样属 device-specific, refill 项。

---

填好答案后重跑安装器：

```
pwsh -NoProfile -ExecutionPolicy Bypass -File <repo>\docs\relay\install-relay.ps1 -RepoRoot <repo> -AnswersFile <repo>\docs\relay\adaptation-answers.json
```

重跑是幂等的：只定点替换 `docs\RELAY-ADAPTATION.md` 的 §3/§4/§5，其它章节与用户手改内容保持原样。
