# 接力 prompt — BR-①g 接力链 · 阶段 2（复核轮）

> 本文件只给指针，不复述 handoff 内容。背景 / 目标 / 进度 / 计划 / 裁决全文见下列文件；**人工裁决全文 = `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` §5**。

## 当前状态（以盘上回读为准，勿凭转述）

- 台账 `F:\soft\00selfmade\rime_claude\docs\relay.json` = 本链（`status=running` / `current_stage=2` / `max_stages=6`），**只经 `F:\soft\00selfmade\rime_claude\docs\relay\relay-ledger.ps1` 闸门改写，禁手改**。
- 上一环 = 阶段 1 **修复轮**（B 定性 + A-0 事实核对 + F3 诊断行 + 规则 1 v2 + 判据④ 口径 ADR 草案）；本环 = 阶段 2 **复核轮**（真机 kBoth 重判四道判据 + 侧车与生产桶表差分）。
- 本环写集：`F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\**`（证据，不入库）+ `F:\soft\00selfmade\rime_claude\docs\relay\stage-2-inventory.md`（新建）+ `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（新顶节）+ `F:\soft\00selfmade\rime_claude\docs\archive\prompts\prompt-next-session-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（就地改写）。

## 必读（全路径，按序）

1. `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` —— §5 人工裁决全文；§2 阶段 2 定义与判据；§3 每阶段通用收尾动作；§4 已知工具坑。
2. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-inventory.md` —— 阶段 1 收口（§9 = 修复轮：实现对照与门禁数字）。
3. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` —— 顶节 `## AK` = 阶段 1 收口（上一节 = `## AJ`）。
4. `F:\soft\00selfmade\rime_claude\docs\adr\0081-cloud-sync-criterion-four-substitute-and-bucket-rule-one-v2-draft.md` —— 判据④ 替代口径 + 规则 1 v2（**Status=Draft ⇒ 人工未批，不得当已生效**）。
5. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-legacy-gap-classification.md` —— 926 → 1 分类表与判别口径。
6. `F:\soft\00selfmade\rime_claude\docs\relay.json` —— 台账。
7. `F:\soft\00selfmade\rime_claude\docs\AGENT-RELAY.md` + `F:\soft\00selfmade\rime_claude\docs\relay\RELAY-SOP.md` —— 接力协议与 SOP。
8. `F:\soft\00selfmade\rime_claude\docs\RELAY-ADAPTATION.md` —— §6 本项目覆盖（**git commit / push 在本仓不是人工检查点**：门禁绿后可自行 commit + push）。
9. `F:\soft\00selfmade\rime_claude\docs\adr\0077-bucket-level-sync-storage-architecture.md` —— 四道判据 / 回退条件 / 阶段 3 硬前置。
10. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` —— 法则层（R1–R9 / P1–P8）。
11. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\` —— 盘上原始证据（**不入库**）。

## 本环硬约束（红线，逐字执行）

- 阶段不得自动递增；阶段 3 / 4 **永不启用**；规则 1 盲区未闭合 + 判据④ 未重判前**不得切读**。
- 生产 D1 / R2 / Cloudflare **只读**；**不得**执行服务端迁移或写生产 D1（须人工单独授权）；不动用户词典数据；`AC-09` 仍 FAIL **不得改记**；`AC-212` / `AC-215②` 未判定；凭据值绝不进日志 / 输出 / 提交物。
- **禁止**从脚本调用服务端 `/v1/auth/refresh`（成功即轮换并撤销旧 refresh_token，会打断运行中客户端）⇒ 401 一律改为「重读本机凭据库」重试。
- 取证 runner 必须**只读**、自带落盘判据 + 心跳（detached runner 会被环境回收）；**不得并发**跑会杀 `WeaselServer` 的 runner（互重置 `startup_pending_`）；切阶段 / 重启必须让新进程拿到新环境块（User 级变量只写注册表）。
- 判据④ 仍按**原文**口径裁定并如实记红 / 绿；ADR 0081 **草案**口径只作**参考测量**并标注「草案 · 未批准」，**不得替换原文裁定**。
- push 走**裸** `git push kizemo feat/100-cloud-sync` 与 `git ls-remote kizemo refs/heads/feat/100-cloud-sync`（仓库级 SOCKS5 `socks5h://127.0.0.1:7897` 已设好，**不要**加 `-c http.proxy=`）；硬判据 = `ls-remote` 与 `rev-parse HEAD` **逐字相等**。
- 门禁：单独 msbuild `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.vcxproj`（**显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，`weasel.sln` 不含该工程 ⇒ 跑旧 exe 会假绿）+ `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\gate-p0-run.ps1`；判绿 = `PASS=<n>` 且 `FAIL=0` + `UFFFD_COUNT=0` + `CLANGFORMAT_HUNK_DRIFT=0`（**不认 `$LASTEXITCODE`**）。
- commit：Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**；`librime/`、`release/`、`reports/**`、`tmp/**`、`env.bat`、`msbuild*.log` **不入库**。
- 台账数字一律**盘上回读**；收口条目自带完整背景 / 目标 / 进度 / 计划 / 裁决。

## 一句话

先读 1–3 号文件掌握背景、进度与人工裁决全文，再按 `plan-relay.md` §2 的阶段 2 定义执行；证据落文件、对话只留指针与结论，**不伪造验收证据**。