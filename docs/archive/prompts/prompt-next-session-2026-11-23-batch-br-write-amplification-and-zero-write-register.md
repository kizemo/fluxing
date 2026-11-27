# 接力 prompt — BR-①g 接力链 · 阶段 2 已收口（判据④ FAIL ⇒ 停链）· 阶段 3 待人工裁定

> 本文件只给指针，不复述 handoff 内容。背景 / 目标 / 进度 / 计划 / 裁决全文见下列文件；**人工裁决全文 = `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` §5**。
>
> **⚠ 本链当前 `status=blocked`（2026-11-27 阶段 2 收口）。** 停链原因 = 四道判据中**判据④ = FAIL**（6 条 `bucket_older` 全部 `RED_bucket_hlc_exceeds_cutoff`），且该判据在上一环（`mvs_3ff9eb40…`，13:19）已 FAIL 一次 ⇒ 命中 `stop_conditions`「same acceptance failed twice -> status=blocked」。**未取得人工裁定前不得起新环、不得自行改判据④ 为 PASS、不得触碰桶阶段 3（切读）/ 4（退役）。**
>
> **⚠ 阶段序列于 2026-11-27 15:47 变更（用户裁定「摘掉阶段 3，先修 P0 假空再切源」）**：原阶段 3（D1 写预算阈值 20,000）**整条作废**，阶段 3 槽位改载 **P0 假空修复**并升为切源硬前置。`docs\relay.json` 的 `stage_titles[2]` 仍是旧文案且闸门不提供改写动作 ⇒ **阶段语义一律以 `docs\plan-relay.md` §2 为准**，不要按台账标题执行。

## 当前状态（以盘上回读为准，勿凭转述）

- 台账 `F:\soft\00selfmade\rime_claude\docs\relay.json` = `status=blocked` / `current_stage=2` / `max_stages=6`，**只经 `F:\soft\00selfmade\rime_claude\docs\relay\relay-ledger.ps1` 闸门改写，禁手改**；`blocked_on[]` 已登记判据④ 判红与待裁定项。
- 上一环 `mvs_f16dfeac…`（`relay-02of06`）= 阶段 2 **修复 + 重判轮**：①-1 `bucket pull` 有限重试已落码 + 门禁四行全绿 + 升版 **1.3.14** 出包装机 + 真机等到第 2 轮 `kBoth` 完成桶推（901 桶全 accepted / 零 skipped）⇒ 判据①②③ PASS、**判据④ FAIL**。
- 再上一环 `mvs_3ff9eb40…`（`relay-03of06`）与 `mvs_55b84714…`（`relay-02of06` 阶段 1 修复轮）产物同样在盘上，阶段 1–2 的 §1–§13 节不得删改。
- 本链写集（若人工放行阶段 3）：`F:\soft\00selfmade\rime_claude\WeaselServer\Cloud\**` + `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\**`（用户已授权）+ `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\**`（证据，不入库）+ `F:\soft\00selfmade\rime_claude\docs\relay\stage-3-*.md` + handoff 新顶节 + 本 prompt（就地改写）。

## 必读（全路径，按序）

1. `F:\soft\00selfmade\rime_claude\docs\plan-relay.md` —— §5 人工裁决全文；**§2 阶段 3 现行定义 = P0 假空修复（四件按序：机制定性只读 → 失败复现用例须先能红 → 修复 → 门禁+出包装机+真机）**；§3 每阶段通用收尾动作（六步）；§4 已知工具坑。
2. `F:\soft\00selfmade\rime_claude\docs\archive\handoffs\handoff-2026-11-23-batch-br-write-amplification-and-zero-write-register.md` —— **顶节 `## AN` = 阶段 2 收口 + 停链裁决**（其前 `## AM` = 阶段 1 确认环；`## P` / `## P-P` = 桶化客户端实现与升版出包装机配方）。
3. `F:\soft\00selfmade\rime_claude\docs\relay\stage-2-inventory.md` —— **§14 = 本环（`relay-02of06`）阶段 2 收口全文**：修复轮三文件表、两轮真机日志、四判据裁定表、判据④ 归因轮数字与 6 条红明细、鉴别力三样本、本环工具坑、证据清单。§1–§13 属上一链口径，冲突以 §14 为准但**旧节不得删改**。
4. `F:\soft\00selfmade\rime_claude\docs\relay\stage-2-rulings-20261127.md` —— 四道判据的 **v2 改写口径**（① `4xx=0` 硬红 / ② 完整轮 / ③ 桶推执行证明 / ④ `unmapped=0` 且 `bucket_older=0` 经四条准入）与 `AC-213`/`AC-214` 退役决定；**判据④ 现行结论 = FAIL（`bucket_older=6` 全红）**。
5. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\stage2-judge4-verdict.md` + `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\judge4-v2-older-attrib.tsv` —— 判据④ 判红的裁定与逐条归因原始证据（**不入库**）。
6. `F:\soft\00selfmade\rime_claude\docs\relay\stage-1-confirm-20261127.md` + `stage-1-inventory.md` + `stage-1-legacy-gap-classification.md` —— 阶段 1 三个产物（确认环 / 修复轮实现对照 / 926 缺口分类）。
7. `F:\soft\00selfmade\rime_claude\docs\adr\0081-cloud-sync-criterion-four-substitute-and-bucket-rule-one-v2-draft.md` —— 判据④ 替代口径 + 规则 1 v2（**Status=Draft ⇒ 人工未批，不得当已生效**）。
8. `F:\soft\00selfmade\rime_claude\docs\adr\0077-bucket-level-sync-storage-architecture.md` —— 四道判据 / 回退条件 / 阶段 3 硬前置。
9. `F:\soft\00selfmade\rime_claude\docs\relay.json` —— 台账（`blocked_on[]` / `stages[]` / `notes[]` 含全部已登记事实）。
10. `F:\soft\00selfmade\rime_claude\docs\AGENT-RELAY.md` + `F:\soft\00selfmade\rime_claude\docs\relay\RELAY-SOP.md` —— 接力协议与 SOP。
11. `F:\soft\00selfmade\rime_claude\docs\RELAY-ADAPTATION.md` —— §4 门禁命令形状；§6 本项目覆盖（**git commit / push 在本仓不是人工检查点**：门禁绿后可自行 commit + push）。
12. `F:\soft\00selfmade\rime_claude\.specify\memory\constitution.md` —— 法则层（R1–R9 / P1–P8）。
13. `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\` —— 盘上原始证据（**不入库**）。

## 本环硬约束（红线，逐字执行）

- **停链状态**：未取得人工裁定前**不得**起新环；`status=blocked` 只能由人工改回 `running`。若被人工放行，从 `docs\plan-relay.md` §2 的**阶段 3（P0 假空修复）**起手，**不得**回头重做阶段 1/2 的修复与重判。
- 阶段不得自动递增；桶阶段 `FLUXING_CLOUD_SYNC_BUCKET_PHASE` 的 **3（切读）/ 4（退役）永不启用**；规则 1 盲区未闭合 + 判据④ 未重判通过前**不得切读**。
- **判据④ 仍记 FAIL，不得改记**；`AC-09` 仍 FAIL **不得改记**（只有日志出现 `AC09_VERDICT=PASS` 才可改）；`AC-212` / `AC-215②` 未判定；`AC-213` / `AC-214` 永久退役。
- 生产 D1 / R2 / Cloudflare **只读**；**不得**执行服务端迁移或写生产 D1（须人工单独授权）；不动用户词典数据（D10：不清洗 / 不重建用户词典）。
- **禁止**从脚本调用服务端 `/v1/auth/refresh`（成功即轮换并撤销旧 refresh_token，会打断运行中客户端）⇒ 401 一律改为「重读本机凭据库」重试。
- 真机取证必须等**第 2 轮 `kBoth`**：进程启动后第 1 轮恒 `kPullOnly`、轮间隔实测 ≈62 分钟 ⇒ 不要拿启动轮判红（`CloudSyncEngine.cpp:205-213`）。
- **绝不杀 / 重启 `WeaselServer`**（会重置 `startup_pending_`，把 `kBoth` 轮再推后 ≈60 min）；不得并发跑会杀 `WeaselServer` 的 runner；切阶段必须让**新进程**拿到新环境块（`std::getenv` 读进程环境块快照，改 User 级注册表变量对运行中进程无效）。
- 取证 runner 必须**只读或自带落盘判据 + 心跳**（detached runner 会被环境回收）；中间产物一律写文件，对话只留指针 + 结论。
- 门禁：单独 msbuild `F:\soft\00selfmade\rime_claude\test\TestFluxingCloud\TestFluxingCloud.vcxproj`（**显式 `/p:SolutionDir=F:\soft\00selfmade\rime_claude\`**，`weasel.sln` 不含该工程 ⇒ 跑旧 exe 会假绿）+ `F:\soft\00selfmade\rime_claude\reports\fluxing-br-1g\gate-p0-run.ps1`；跑前设 `$env:FLUXING_CLOUD_SYNC_BUCKET_PHASE='0'`；判绿 = `PASS=<n>` 且 `FAIL=0` + `UFFFD_COUNT=0` + `CLANGFORMAT_HUNK_DRIFT=0`（**不认 `$LASTEXITCODE`**）；**当前基线 `PASS=1047`**（1.3.14 轮实测）。
- 出包 / 装机 / 运行时自证配方见 handoff `## P` / `## P-P` / `## AN`（版本号四处同步、sha256 三处全等、静默装机 `INSTALL_EXIT=0`、`/D=` 必须最后且不加引号）。**当前运行时 = 1.3.14，`WeaselServer.exe` sha256 `2E38EBB30207BE816BD50FCB2F95A5C189467DBE577D86BD99C24A42F6E432DA`。**
- push 走**裸** `git push kizemo feat/100-cloud-sync` 与 `git ls-remote kizemo refs/heads/feat/100-cloud-sync`（仓库级 SOCKS5 `socks5h://127.0.0.1:7897` 已设好，**不要**加 `-c http.proxy=`）；硬判据 = `ls-remote` 与 `rev-parse HEAD` **逐字相等**。**该硬判据在本链阶段 2 已成立过一次并被人工接受的写法是：`git ls-remote` 失败即停链记 `blocked`，不得用 force-push 绕。**
- commit：Conventional Commits + 模块前缀（`WeaselServer | WeaselTSF | WeaselDeployer | cloud | docs | fluxing | installer | ci`）；**按路径显式 `git add`，禁 `git add .`**；`librime/`、`release/`、`reports/**`、`tmp/**`、`env.bat`、`msbuild*.log` **不入库**；提交前必跑 `git status -sb` + `git diff --stat`。
- 台账数字一律**盘上回读**；收口条目自带完整背景 / 目标 / 进度 / 计划 / 裁决；接力 prompt 写在回答正文里，不另建文件。

## 一句话

先读 1–5 号文件确认停链原因（判据④ FAIL 的 6 条红与两轮 FAIL 记录）、四判据 v2 口径与阶段 3 现行定义；**未取得人工裁定不得起新环**，放行后按 `docs\plan-relay.md` §2 阶段 3 定义执行 P0 假空修复（第一步必须重读现码、不许照抄 handoff 的机制描述）；证据落文件、对话只留指针与结论，**不伪造验收证据**。