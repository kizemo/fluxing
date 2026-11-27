# 接力 prompt — BR-①g 接力链 · 阶段 2（修复 + 重判轮）

> 本文件只给指针，不复述 handoff 内容。背景 / 目标 / 进度 / 计划 / 裁决全文见下列文件；**人工裁决全文 = `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` §5**。
>
> **⚠ 阶段序列于 2026-11-27 15:47 变更（用户裁定「摘掉阶段 3，先修 P0 假空再切源」）**：原阶段 3（D1 写预算阈值 20,000）**整条作废**，阶段 3 槽位改载 **P0 假空修复**并升为切源硬前置。`docs\relay.json` 的 `stage_titles[2]` 仍是旧文案且闸门不提供改写动作 ⇒ **阶段语义一律以 `docs\plan-relay.md` §2 为准**，不要按台账标题执行。

## 当前状态（以盘上回读为准，勿凭转述）

- 台账 `F:\soft\00selfmade\rime_claude\docs\relay.json` = 本链（`status=running` / `current_stage=2` / `max_stages=6`），**只经 `F:\soft\00selfmade\rime_claude\docs\relay\relay-ledger.ps1` 闸门改写，禁手改**。
- 上一环 = 阶段 1 **确认环**（对 `1d86d8c` 的零改动复核，3/3 PASS，产物 `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-confirm-20261127.md`）。
- 本环 = 阶段 2 **修复 + 重判轮**：①-1 `bucket pull` 有限重试 → 门禁 + 出包装机 → 真机 kBoth 轮取证 → 四判据 v2 重判。
- 本环写集：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\**` + `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\**`（产品代码 / 门禁用例，**用户已授权**）+ `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\**`（证据，不入库）+ `F:\soft\00selfmade\rime_claude\docs\relay\stage-2-*.md`（新建）+ `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md`（新顶节）+ 本 prompt（就地改写）。

## 必读（全路径，按序）

1. `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` —— §5 人工裁决全文；§2 阶段 2 定义；§3 每阶段通用收尾动作（六步）；§4 已知工具坑。
2. `F:\soft\00selfmade\rime_claude\docs\relay\stage-2-rulings-20261127.md` —— 四道判据的 **v2 改写口径**（判据① `4xx=0` / 判据② 完整轮 / 判据③ 桶推执行证明 / 判据④ 桶侧不得旧于本地）与 `AC-213`/`AC-214` 退役决定。
3. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-confirm-20261127.md` —— 上一环收口（三判据实测、防假绿三交叉、未被验证项）。
4. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` —— 顶节 `## AM` = 阶段 1 确认环收口（其前 `## AL` / `## AK` = 阶段 1 修复轮与更早状态）。
5. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-inventory.md` —— 阶段 1 收口（§9 = 修复轮：实现对照与门禁数字）。
6. `F:\soft\00selfmade\rime_claude\docs\adr\0081-cloud-sync-criterion-four-substitute-and-bucket-rule-one-v2-draft.md` —— 判据④ 替代口径 + 规则 1 v2（**Status=Draft ⇒ 人工未批，不得当已生效**）。
7. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-legacy-gap-classification.md` —— 926 → 1 分类表与判别口径。
8. `F:\soft\00selfmade\rime_claude\docs\adr\0077-bucket-level-sync-storage-architecture.md` —— 四道判据 / 回退条件 / 阶段 3 硬前置。
9. `F:\soft\00selfmade\rime_claude\docs\relay.json` —— 台账（`stages[]` 含本链已登记阶段与其 `session_id`）。
10. `F:\soft\00selfmade\rime_claude\docs\AGENT-RELAY.md` + `F:\soft\00selfmade\rime_claude\docs\relay\RELAY-SOP.md` —— 接力协议与 SOP。
11. `F:\soft\00selfmade\rime_claude\docs\RELAY-ADAPTATION.md` —— §4 门禁命令形状；§6 本项目覆盖（**git commit / push 在本仓不是人工检查点**：门禁绿后可自行 commit + push）。
12. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` —— 法则层（R1–R9 / P1–P8）。
13. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\` —— 盘上原始证据（**不入库**）。

## 本环硬约束（红线，逐字执行）

- 阶段不得自动递增；桶阶段 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` 的 **3（切读）/ 4（退役）永不启用**；规则 1 盲区未闭合 + 判据④ 未重判前**不得切读**。
- 生产 D1 / R2 / Cloudflare **只读**；**不得**执行服务端迁移或写生产 D1（须人工单独授权）；不动用户词典数据；`AC-09` 仍 FAIL **不得改记**；`AC-212` / `AC-215②` 未判定；凭据值绝不进日志 / 输出 / 提交物。
- **禁止**从脚本调用服务端 `/v1/auth/refresh`（成功即轮换并撤销旧 refresh_token，会打断运行中客户端）⇒ 401 一律改为「重读本机凭据库」重试。
- 真机取证必须等**一轮 `kBoth`**：进程启动后第 1 轮恒 `kPullOnly`、第 2 轮才 `kBoth`，轮间隔实测 ≈62 分钟 ⇒ 不要拿启动轮判红。
- 取证 runner 必须**只读或自带落盘判据 + 心跳**（detached runner 会被环境回收）；**不得并发**跑会杀 `WeaselServer` 的 runner（互重置 `startup_pending_`）；切阶段 / 重启必须让新进程拿到新环境块（User 级变量只写注册表）。
- 判据④ 仍按**原文**口径裁定并如实记红 / 绿；ADR 0081 **草案**口径只作**参考测量**并标注「草案 · 未批准」，**不得替换原文裁定**。
- 门禁：单独 msbuild `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.vcxproj`（**显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，`weasel.sln` 不含该工程 ⇒ 跑旧 exe 会假绿）+ `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\gate-p0-run.ps1`；跑前设 `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'`；判绿 = `PASS=<n>` 且 `FAIL=0` + `UFFFD_COUNT=0` + `CLANGFORMAT_HUNK_DRIFT=0`（**不认 `$LASTEXITCODE`**）；本轮起点基线 `PASS=1014`。
- 出包 / 装机 / 运行时自证配方见 handoff 顶节 §P / §P-P（版本号、sha256 三处全等、静默装机 `INSTALL_EXIT=0`）。
- push 走**裸** `git push kizemo feat/100-cloud-sync` 与 `git ls-remote kizemo refs/heads/feat/100-cloud-sync`（仓库级 SOCKS5 `socks5h://127.0.0.1:7897` 已设好，**不要**加 `-c http.proxy=`）；硬判据 = `ls-remote` 与 `rev-parse HEAD` **逐字相等**。
- commit：Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**；`librime/`、`release/`、`reports/**`、`tmp/**`、`env.bat`、`msbuild*.log` **不入库**。
- 台账数字一律**盘上回读**；收口条目自带完整背景 / 目标 / 进度 / 计划 / 裁决。

## 一句话

先读 1–4 号文件掌握背景、进度与四判据 v2 口径，再按 `plan-relay.md` §2 的阶段 2 定义执行（先修 ①-1 有限重试并过门禁出包，再等 `kBoth` 轮取证重判）；证据落文件、对话只留指针与结论，**不伪造验收证据**。